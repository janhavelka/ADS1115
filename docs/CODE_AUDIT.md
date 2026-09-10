# Audit Findings Backlog

Open, actionable defects and cleanups found by the 2026-09-09 code audit.
Each entry states the problem, a concrete failure scenario, and a proposed fix.

This is a working backlog, not a session log. **Delete an entry when it is
resolved** and record the durable behavior change in `CHANGELOG.md`. Do not
accumulate completed work here.

Baseline: `b57c682`, driver version 2.0.1, native suite 203/203.

Verified against the checked-in TI ADS111x Rev. E datasheet. The register map,
bit encodings, MUX/PGA/DR tables, OS polarity, byte order, address range and the
`worstCaseConversionTimeUs()` formula were all re-derived and are **correct**;
no defect below is a datasheet-conformance defect.

A follow-up review pass independently reproduced **C1, C2, C4 and C10** against
the source, withdrew **C3** as a false positive, and corrected the proposals in
**C6** and **C7**. Those three entries record what was wrong with the original
reasoning so it is not repeated.

---

## 1. Correctness

### C1 - `isUncertainWriteFailure()` is a fail-open allowlist

`src/ADS1115.cpp:150-156`. Five error codes mark a failed write as "may have
reached hardware"; **everything else is treated as proof of no effect**.

```cpp
bool isUncertainWriteFailure(Err err) {
  return err == Err::I2C_ERROR || err == Err::TIMEOUT ||
         err == Err::I2C_NACK_DATA || err == Err::I2C_TIMEOUT ||
         err == Err::I2C_BUS;
}
```

`Config.h` tells transport authors to "preserve transport-native detail" and not
to claim a NACK phase they cannot prove. It does **not** restrict them to those
five codes, and `Err` has 25 values.

**Failure scenario.** A transport maps a mid-transfer bus fault to
`Err::DEVICE_NOT_FOUND` - a mapping the driver itself produces elsewhere. During
`startRead()`'s CONFIG start-write the address phase ACKs, the first data byte
goes out, then the bus faults. `isUncertainWriteFailure(DEVICE_NOT_FOUND)` is
false, so the driver restores `VERIFIED` trust, leaves `_conversionStarted`
false and does not mark dirty - while the device may be mid-conversion. The next
`startRead()` is accepted and writes `OS_START` into a converting device.

**Proposal.** Invert to fail-closed so an unmapped or newly added code defaults
to "hardware may have changed":

```cpp
// A write is provably ineffective only when the address phase was rejected, or
// when the driver never handed the buffer to the transport at all.
bool isDefiniteNoEffectWriteFailure(Err err) {
  return err == Err::I2C_NACK_ADDR ||
         err == Err::INVALID_CONFIG ||
         err == Err::INVALID_PARAM;
}
bool isUncertainWriteFailure(Err err) {
  return !isDefiniteNoEffectWriteFailure(err);
}
```

Note the opposite direction also exists today and is merely wasteful: the
owner-safe example returns `Err::I2C_TIMEOUT` when *lock acquisition* fails,
before any bus activity, which currently dirties clean state. Tightening the
`Config.h` callback contract to name the definite-no-effect codes fixes both
directions.

### C2 - The abandon quiet interval trusts a data rate the driver just disproved

`src/ADS1115.cpp:2338-2345`.

```cpp
DataRate ADS1115::_operationGuardDataRate() const {
  if (_operationKind == OperationKind::SHUTDOWN &&
      (_configurationStateBeforeOperation != ConfigurationState::VERIFIED ||
       _hardwareConfigDirty)) {
    return DataRate::SPS_8;
  }
  return _desiredProfile.dataRate;
}
```

The conservative fallback is gated on `SHUTDOWN`. But the one case where the
driver holds positive evidence that its cached rate is wrong is a
`READ_SINGLE_SHOT` abandon caused by a **readback mismatch** - and that path
uses the cached rate.

**Failure scenario.** `_desiredProfile.dataRate = SPS_860`. The device browns
out back to POR defaults (`0x8583`, DR = 128 SPS). The readiness poll sees a
CONFIG mismatch and abandons. The quiet interval is computed from 860 SPS
(3 ms) when the device actually needs 10 ms - and against an 8 SPS device it is
3 ms versus 140 ms, a 47x shortfall. `startRecover()` is then accepted and
writes into a still-converting device, defeating the entire reconciliation
guarantee.

**Proposal.** Make the guard rate a function of trust, not of operation kind.
`_abandonConversion()` already sets `_configurationState = UNKNOWN` before the
wait is ever evaluated, so this covers every abandon path:

```cpp
DataRate ADS1115::_operationGuardDataRate() const {
  // The cached rate is only meaningful while the driver still trusts its record
  // of hardware; anything else must assume the slowest conversion the part can
  // perform.
  const bool trusted =
      !_hardwareConfigDirty &&
      _configurationState != ConfigurationState::UNKNOWN &&
      (_operationKind != OperationKind::SHUTDOWN ||
       _configurationStateBeforeOperation == ConfigurationState::VERIFIED);
  return trusted ? _desiredProfile.dataRate : DataRate::SPS_8;
}
```

The `SHUTDOWN` clause must be kept: `startShutdown()` sets
`_configurationState = APPLYING`, so a predicate of the form
`_configurationState != VERIFIED` alone would force every shutdown onto the
8 SPS interval and regress shutdown latency.

Cost: one extra 140 ms bus-silent wait on an already-failing operation.

### C3 - WITHDRAWN (false positive): failed readiness read clears the conversion flag

Originally filed as a safety hole: `_readConversionReadyAt()` clears
`_conversionStarted` when the readiness CONFIG read fails, which appeared to
unlock writers while the device might still be converting.

**This is wrong and the current code is correct.** The CONFIG read is
reachable only past the timing gate:

```cpp
if ((nowMs - _conversionStartMs) < getConversionTimeMs() * requiredPeriods) {
  return Status::Ok();   // not ready, no I2C
}
```

`getConversionTimeMs()` is the worst-case interval, already including the
-10% data-rate tolerance and a 1 ms guard. Every other path out of
`_readConversionReadyAt()` (cached-ready, not-started, unarmed timestamp,
ALERT asserted, continuous) returns before the read. So by the time the read
is attempted the conversion is provably complete, and clearing
`_conversionStarted` is truthful regardless of whether the read succeeded.
Nothing is marked dirty because no write occurred.

The one way the gate can be too short is a cached data rate that no longer
matches hardware - which is C2, tracked separately.

Kept as a withdrawn entry so it is not re-filed.
### C4 - Ambiguous CONFIG writes that carry `OS_START` are not recorded

`writeConfig()` (`src/ADS1115.cpp:1968-1972`) marks dirty on an ambiguous
failure but does not set `_conversionStarted`, even though the written value may
have started a conversion. `writeRegister16()` (`src/ADS1115.cpp:2484-2509`)
has the same gap on the **success** path: a raw write of
`CONFIG | OS_START` starts a conversion the driver never tracks.

`startConversion()` gets this right (`src/ADS1115.cpp:1419-1425`).

**Proposal.** One private helper, used by `startConversion()` (both overloads),
`writeConfig()`, `writeRegister16()` and `_writeConfigOnly()`, so the five
copies cannot drift:

```cpp
// Record that a CONFIG value carrying OS_START may have started a conversion.
void ADS1115::_noteConversionMayHaveStarted(uint16_t configValue);
```

### C5 - Per-request gain changes silently rescale comparator thresholds

Datasheet, section 8.1.4: *"The comparator is implemented as a digital
comparator; therefore, the values in these registers must be updated whenever
the PGA settings are changed."*

`startRead()` accepts a `ChannelRequest::gain` that may differ from
`DeviceProfile::defaultGain`, and writes/verifies it without rewriting the
thresholds. `setGain()` does the same. With a `ComparatorUse::THRESHOLD`
profile, a threshold code of 16000 means 1.0 V at +/-2.048 V and 0.5 V at
+/-1.024 V - the trip point silently moves.

Harmless for `ComparatorUse::OFF` and for `CONVERSION_READY` (whose thresholds
are pure MSB flags).

**Proposal.** Reject the ambiguous combination rather than documenting around
it. In `startRead()`, after `validateChannelRequest()`:

```cpp
if (_desiredProfile.comparator.use == ComparatorUse::THRESHOLD &&
    request.gain != _desiredProfile.defaultGain) {
  return Status::Error(Err::INVALID_PARAM,
                       "Per-request gain change requires threshold recalculation");
}
```

The doc half of this is already applied (`setGain()` and `startRead()` now carry
the caveat); only the enforcement is outstanding.

### C6 - `tick()`/`service()` do not state which timebase they require

`src/ADS1115.cpp:1166-1170` forwards to `poll(nowMs, 1)` whenever a job is
active. `_operationDeadlineMs`, `_jobNextReadyPollMs` and `_abandonWaitUntilMs`
were all established from the owner's `nowMs`, so a caller that passes a
*different* monotonic source to `service()` corrupts those guards: a
reconciliation wait armed at 5 000 000 + 140 and then polled with
`tick(millis())` = 12 000 evaluates `int32_t(12000 - 5000140)`, so the wait
never expires. Mixed domains also produce spurious `TIMEOUT`.

**Severity: documentation, not behavior.** The forwarding itself is the
documented contract (`ADS1115.h`: "Advances an active owner operation by at
most one transaction, or services legacy conversion polling when no owner
operation is active"), and the driver is designed around a single monotonic
domain (`Config.h`: "Owner time comes only from the nowMs argument supplied
to poll()"). Feeding it two clocks is an application error. An earlier draft
of this entry proposed making `service()` return `BUSY` while a job is active;
that would break the documented forwarding and is **not** the right fix.

**Proposal.** State the requirement where callers will read it - the `tick()`
and `service()` doc comments must say that `nowMs` has to come from the same
monotonic source used for `start*()` and `poll()`. Optionally harden by
recording the domain at `_beginOperation()` and forwarding only operations
`service()` itself started; that is defence in depth, not a contract change.
### C7 - `WAIT_IDLE_AFTER_ABANDON` has no bound and no owner-authorized escape

`poll()` exempts this state from the operation deadline
(`src/ADS1115.cpp:552`), `poll()` has no stalled-clock guard, and
`cancelActiveOperation()` returns `RECONCILIATION_REQUIRED` without changing
state. If the supplied clock stops, the binding is stuck: `_jobActive` stays
true, so every `start*()` and every compatibility I2C method returns `BUSY`,
and the only escape is `unbind()`, which drops the binding entirely.

**Proposal.** An earlier draft proposed an absolute `_abandonDeadlineMs`. That
does not work: a frozen clock never reaches an absolute deadline either. The
bound has to be independent of time.

1. Add a same-tick poll guard to `poll()`, mirroring the existing
   `kMaxSameTickPolls` heuristic already used by `shutdown()` and
   `readBlocking()`. On trip, do **not** declare the device idle: finish with
   `OperationState::INDETERMINATE`, `hardwareStateUncertain = true`, and leave
   `_conversionStarted` set so the next caller still knows hardware may be busy.
2. That alone is not an escape, because consuming the terminal result leaves
   `_conversionStarted` true and `_singleShotMayBeActive()` then blocks
   `startRecover()` forever. So also let `startRecover()` proceed when no
   operation is active and trust is already `UNKNOWN`. Recovery is explicitly
   owner-authorized and re-establishes truth by probe + apply + verify; a CONFIG
   write during an in-flight conversion is legal (the datasheet only says the OS
   bit has no effect while converting), and the abandoned sample is discarded
   anyway.

Both parts are needed; either alone leaves the binding unrecoverable.
### C8 - `pollSingleShot()`/`pollApplyConfig()` return `done=false` with no active job

`src/ADS1115.cpp:1783-1787` and its twin. When a *different* operation holds an
unconsumed terminal result, these facades return `BUSY, done=false` forever -
no polling can make them done, only `takeResult()` from elsewhere. A caller
running the documented `while (!r.done)` loop spins at CPU speed with zero bus
traffic.

**Proposal.** Make `done` a derived property with a single source of truth,
which also removes the parameter from ~20 call sites:

```cpp
PollResult ADS1115::_pollResult(Status status, uint8_t used) const {
  PollResult r;
  r.status = status;
  r.instructionsUsed = used;
  r.done = !_jobActive;          // single source of truth
  ...
}
```

`_finishOperation()` already clears `_jobActive` before returning, so every
existing call site keeps its current value and the facades become
self-consistent by construction.

### C9 - `ConfigurationState::APPLYING` can leak out as a terminal state

Every `start*()` sets `APPLYING`, but three terminal paths never resolve it: the
stalled-clock branches in `shutdown()` and `readBlocking()`, and the
`default:` arm of `poll()`'s switch. Because
`_terminalResult.hardwareStateUncertain` is derived from `UNKNOWN`-ness,
those results report `hardwareStateUncertain == false` after a CONFIG write that
was never verified, and `configurationState()` reports `APPLYING` with nothing
active.

All three are hard to reach, so this is a latent invariant violation rather than
a live bug.

**Proposal.** Make `_finishOperation()` the single owner of trust resolution by
giving it an explicit disposition, which also deletes ~12 duplicated
`_configurationState = ...` assignments at its call sites:

```cpp
enum class TrustOutcome : uint8_t { Restore, Verified, Unknown };
PollResult _finishOperation(const Status& status, OperationState state,
                            uint8_t used, TrustOutcome trust,
                            bool sampleValid = false);
```

`APPLYING` then becomes structurally impossible to leak.

### C10 - `writeConfig()` and `_writeConfigOnly()` disagree about the desired profile

`_writeConfigOnly()` deliberately promotes an operator mutation to
`_desiredProfile` so a later `recover()` replays what was set. `setThresholds()`
does the same. `writeConfig()` updates only `_config.*`, so `recover()` silently
reverts an operator's `writeConfig()` while reporting success.

**Proposal.** Have `writeConfig()` perform the same
`_desiredProfile = _profileFromConfig()` promotion.

---

## 2. Readability and maintainability

The compatibility surface is roughly five times the size of the production
surface (117 public methods, 57 private state members) and drives most of the
complexity below. None of these are defects; they are what makes the defects
above easy to introduce.

### M1 - `poll()` is a 452-line function

The largest function in the codebase, one switch over 13 `JobState` values.
`APPLY_WRITE_{LOW,HIGH,CONFIG}` and `APPLY_VERIFY_{LOW,HIGH,CONFIG}` are five
near-identical ~18-line blocks differing only in register, value, next state and
the first-write dirty policy. The `used >= budget` guard appears nine times.
`APPLY_VERIFY_CONFIG` alone mixes three concerns: the shutdown idle gate,
verification, and commit logic for four operation kinds.

**Proposal (behavior-preserving).** A static
`{reg, value, nextState}` table for the apply sequence plus
`_stepWriteRegister()`, `_stepVerifyRegister()`, `_commitProfile()` and
`_commitShutdown()` helpers; hoist the budget check into the dispatch loop.
Target ~120 lines of dispatch.

### M2 - Precondition boilerplate is repeated ~77 times

24 `_jobBusyStatus()` guards, 21 `_singleShotMayBeActive()` guards and 32
`NOT_INITIALIZED` checks across ~85 method bodies, hand-written each time. Only
5 of 24 methods check `_terminalResultAvailable`, and `startConversion()` uses a
raw `_conversionStarted` read instead of `_singleShotMayBeActive()`, so it is
reachable with an unconsumed owner terminal result pending.

**Proposal.** One private helper with flags, so an omission is visible:

```cpp
Status _checkPreconditions(uint8_t requirements) const;  // kInitialized |
                                                         // kIdleJob |
                                                         // kHardwareIdle |
                                                         // kNoPendingResult
```

### M3 - 47 helper functions are duplicated between the two CLI examples

`examples/01_basic_bringup_cli/main.cpp` (2141 lines) and
`examples/esp_idf/basic/main/main.cpp` (1547 lines) carry 47 identically-named
helpers - every enum-to-string converter, both parsers, and the whole diagnostic
command set. Parity is enforced only by token matching in
`tools/check_idf_example_contract.py`, which is precisely why the `readVoltage`
selftest defect (now fixed) went undetected.

**Proposal.** Extract the framework-neutral half into
`examples/common/CliCore.h`, parameterized on a small output sink and a
`nowMs` function. Leave only the I/O adapter and `setup`/`app_main` per
platform. Removes ~900 duplicated lines and makes most of the contract script
unnecessary.

### M4 - CLI dispatch is a 53-branch `else if` chain

`processCommand()` is 415 lines of `String` comparisons, plus two
sub-dispatchers. A command table would also let `printHelp()` be generated from
the same source, eliminating the help/dispatch drift that
`check_idf_example_contract.py` currently guards by regex.

**Note.** `tools/check_cli_contract.py` matches literal `cmd == "probe"`
patterns, so it must be updated in the same commit.

### M5 - `_applyCachedConfigSynchronously()` has exactly zero poll slack

`src/ADS1115.cpp` drives `for (step < 2)` polls x 3 transactions = exactly the
6 an apply needs. Any future added step silently turns
`enableConversionReadyPin()` into `INDETERMINATE "Cached config apply did not
terminate"`. Compare `begin()`/`recover()`: 12 available for 7 needed.

**Proposal.** Drive the loop by `_jobActive` with a bound derived from the step
count, or simply raise it to 3 polls.

### M6 - `Err::MEASUREMENT_NOT_READY` is a duplicate enumerator value

`Status.h:19` aliases it to `CONVERSION_NOT_READY`. Any `switch (err)` listing
both labels fails to compile. Harmless today; worth a comment so nobody tries.

### M7 - `Config::strictInitVerify` is write-only

Assigned `true` unconditionally in two places and only ever read back into
`SettingsSnapshot`. Already documented as a compatibility field. Removal is a
breaking change; schedule it for the next major version.

---

## 3. Tests

The suite is high quality - it asserts exact I/O counts, bus silence, timeout
partitioning, and error `detail` propagation. Three gaps matter.

### T1 - The fake models the device address and the OS bit only on request

`FakeBus::expectedAddress` defaults to 0, which short-circuits both address
guards; **one** of 203 tests enables it. Mutation proof: replacing
`_config.i2cAddress` with a literal `0x48` in both transport wrappers fails
**1/203**. Likewise `modelConversionState` defaults false for 195 tests, so a
CONFIG readback always reports idle; neutering the OS-idle guard in
`_readConversionReadyAt` fails **1/203**.

For comparison, swapping the two data bytes in `_writeRegister16Tracked` fails
**180/203** - byte order is genuinely well covered.

**Proposal.** Two one-line default changes: `expectedAddress = 0x48` with
`makeConfig()`/`makeDriverConfig()` setting it from the config, and
`modelConversionState = true` with the few always-idle tests opting out.

### T2 - The enum-stability guard pins 5 of 25 values

`test_status_taxonomy_additions_are_append_only` pins `OFFLINE=14` through
`CLOCK_STALLED=18`. Inserting one enumerator after `CLOCK_STALLED` - shifting
`CANCELLED`, `CONFIG_UNKNOWN`, `RESULT_NOT_AVAILABLE`, `TOKEN_MISMATCH` and
`INDETERMINATE` - fails **0/203**. These numbers are a live serial contract; the
CLI prints `code=%u`.

**Proposal.** Pin all 25 with a `static_assert` block in `Status.h` itself, so
the contract lives next to the enum.

### T3 - Four untested public methods

`terminalResultAvailable()`, the `readRegister()` alias,
`isAlertRdyPinConfigured()` and `isConversionReadyModeEnabled()`. The first is
on the critical path of both shipped example CLIs.

### T4 - ~1200-1500 lines of the 6327 are mechanical

965 `static_cast<uint8_t>(` occurrences; 236 two-line status-code assertions;
213 `FakeBus bus;` / 216 `ADS1115::ADS1115 dev;` preambles. Six clusters are
directly table-drivable (invalid-enum rejection appears twice, once through
`begin()` and once through the setters).

**Proposal.** A `TEST_ASSERT_ERR(expected, st)` macro, a small `Fixture` struct,
and six `for (const auto& c : kCases)` conversions. ~6327 -> ~4900 lines with
identical coverage.

C1, C2, C4, C8 and C10 are deterministically reproducible with the existing
fake and a scripted `nowMs`; each fix should land with its regression test.

---

## 4. Documentation and tooling

### D1 - Typed setters do not document their effect on configuration trust

The most consequential doc gap on the compatibility surface: a successful
`setMux()`/`setGain()`/`setDataRate()`/`setMode()`/`setComparator*()` rewrites
`_desiredProfile` **and drops `configurationState()` to `UNKNOWN`**, which blocks
`startRead()` with `CONFIG_UNKNOWN` until a full apply or recover. None of the
setter doc comments say so.

### D2 - Undocumented return codes

`readBlocking()` documents 3 of 9 possible codes (missing `INVALID_PARAM`,
two `BUSY` forms, `UNSUPPORTED_OPERATION`, `RESULT_NOT_AVAILABLE`,
`INDETERMINATE`). `recover()` documents only `Ok()`. `startConversion(Mux)`
omits `INVALID_PARAM` while its `startSingleShot(Mux)` sibling lists it.
`begin()` can return `BUSY` because of an unconsumed owner result and never says
so. `service()` returns `NOT_INITIALIZED` with a message that says "not
initialized" while testing `!_bound`.

### D3 - `cancelActiveOperation()` doc overstates the dirty rule

The header states that cancelling after any possible hardware effect sets
`hardwareConfigDirty()` and moves trust to `UNKNOWN`. Cancelling in
`SINGLE_SHOT_READ_CONVERSION` correctly retains `VERIFIED` and clean state - the
readback already proved the config and OS already proved idle, so only the
sample is discarded. **The implementation is right; the header should carve out
this case.**

### D4 - A cancel arriving during reconciliation is silently discarded

`cancelActiveOperation()` returns `RECONCILIATION_REQUIRED`, which reads as
"queued", but `_abandonTerminalState` is never updated, so the operation
terminates as `FAILED`/`TIMED_OUT`, never `CANCELLED`. Either record the intent
(the wait duration is unchanged, only the label) or document the no-op.

### D5 - `SampleResult::configGeneration` cannot answer "same configuration?"

The generation is bumped on **every** successful typed read, because a read
commits its mux/gain into the applied profile. Two consecutive reads of the same
channel with the same profile therefore produce generations N and N+1, so
`a.configGeneration == b.configGeneration` never holds. Either suppress the bump
when the committed record is unchanged, or document that generation identifies a
*commit*, not a *configuration*.

### D6 - `SampleFlag::CONFIG_VERIFIED` is a hard-coded literal

Always set unconditionally. True today because trust was set `VERIFIED` two
statements earlier, but it carries no information and would silently lie if the
surrounding logic changed. Derive it from
`_configurationState`/`_hardwareConfigDirty`.

### D7 - HIL toolchain pins are never validated by CI

`tools/run_i2c_hil.py` hard-codes `EXPECTED_ARDUINO_ESP32_VERSION = "3.3.11"`
and `EXPECTED_ESP_IDF_VERSION = "v5.5.5"` and hard-fails a real run on
mismatch, but CI only exercises `--parser-test` and `--dry-run`. Bumping the
pioarduino release in `platformio.ini` silently invalidates them.

There are three independently pinned toolchains that must move together
(`ci.yml` IDF container, `platformio.ini` pioarduino release, the HIL pins) with
no single source of truth.

**Proposal.** Derive the expected versions from `platformio.ini`, or downgrade a
mismatch from `FAIL` to `EVIDENCE_REQUIRED`.

### D8 - `generate_version.py` writes a tracked file when imported

Module scope ends with `result = main([])`, so when SCons imports it as a
pre-script it runs `_sync_outputs(check_only=False)` and can rewrite
`include/ADS1115/Version.h`. A plain `pio run` can silently modify a tracked
source file. Idempotent today, but surprising.

**Proposal.** Make the pre-script path `check_only=True` and fail loudly when
stale.

### D9 - The native test environment drops the warning flags

`[env:native] build_flags` replaces `[env].build_flags` wholesale, so the test
build loses `-Wall -Wextra -Werror=return-type`. The test file is already clean
under those flags, so enabling them is free. `extra_scripts` on the same
environment is also redundant with `[env]`.

### D10 - `idf_component.yml` uses a denylist that omits build output

`files.exclude` does not list `.pio/**/*` or `hil_logs/**/*`, and the ESP-IDF
component manager does not honour `.gitignore`. A `compote component upload`
from a working copy would try to pack the PlatformIO cache. Latent - CI does not
invoke it. (`library.json` uses an allowlist and is safe.)

### D11 - The ESP-IDF example never exercises the owner-safe API

Its only owner-path call is `startRead()` inside the compatibility job facade;
there is no `own` command group and `check_idf_example_contract.py` does not
require one. The library's production model therefore has exactly one example,
and it is Arduino-only.

---

## 5. Release

### REL1 - README pins a tag that predates the documented behavior

`README.md` recommends `#v2.0.1` for both PlatformIO and ESP-IDF, but `main` is
several commits past that tag and the README documents behavior the tag does not
contain - notably the one-eighth readiness retry ("about 18 ms at 8 SPS"), which
does not exist in `v2.0.1`, and `kMaxSameTickPolls` (1024 there, 100000 here).
A consumer following the README gets the 1 ms poll flood the README says was
fixed.

**Proposal.** Cut 2.0.2: `python scripts/generate_version.py bump patch`, move
`CHANGELOG.md`'s `[Unreleased]` section to `[2.0.2]`, update both README pins,
and tag. Do this after the C-series fixes land so the release is worth pinning.
