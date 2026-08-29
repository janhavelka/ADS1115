# ADS1115 Code Audit

Audit of `include/ADS1115/`, `src/ADS1115.cpp`, `test/`, `tools/`, `examples/`
and the documentation set against the TI ADS111x Rev. E datasheet in
[`reference/`](reference/).

This is a working document. Delete an entry when it is closed; delete the file
when it is empty. Durable behavior changes belong in `CHANGELOG.md`.

Section 1 is already applied. Section 2 is open, each entry with a concrete
proposal. Section 3 records what was checked and found correct, so that ground
is not re-covered. Findings marked **reproduced** were demonstrated against the
real driver with a fake transport; the rest are read from the code.

Every candidate was put through an adversarial refutation pass. That pass covered
57 of about 93 candidates before its budget ran out, so a Section 2 entry that
carries neither a **reproduced** marker nor a refutation note has been read but
not independently attacked.

---

## 1. Applied

`pio test -e native` stays at 178/178 and `check_core_timing_guard.py`,
`check_cli_contract.py`, `check_idf_example_contract.py` and
`generate_version.py check` all pass.

### 1.1 A failed readiness poll wedged the driver in `BUSY` forever — critical, reproduced

`SINGLE_SHOT_POLL_READY` terminated the operation on a transport error or a
CONFIG readback mismatch while leaving `_conversionStarted == true`. Nothing
clears that flag except a *successful* readiness poll, so
`_singleShotMayBeActive()` stayed true permanently and **every** entry point
refused to run:

```text
startRead / startRecover / startInitialize / startApplyProfile / startShutdown
recover() / setMux()     / setGain()       / setDataRate()     / writeConfig()
        -> Err::BUSY "Conversion may still be active"
```

Only `unbind()` / `end()` escaped, and that drops the binding and every
diagnostic with it. One transient NACK on a shared bus bricked the driver
instance for the lifetime of the binding. The same latch was reachable from the
compatibility path through `_readConversionReadyAt()`.

**Fix.** Both failure branches now enter `JobState::WAIT_IDLE_AFTER_ABANDON`,
the bus-silent reconciliation the cancel and deadline paths already used. The
quiet interval clears the conversion latch and the original transport error is
still published as the terminal result:

```text
poll -> RECONCILING (bus-silent) -> FAILED, status = original I2C error
     -> _conversionStarted == false -> startRecover() accepted -> VERIFIED
```

The five hand-rolled copies of that abandon sequence (deadline, ambiguous start
write, cancellation, stalled clock, and the two new readiness branches) are
folded into one `_abandonConversion()` helper, a net removal of duplicated state
mutation rather than an addition.

`test_poll_single_shot_ready_transport_failure_propagates` was updated: it
previously asserted `done == true` on the failing poll, which is the bug.

### 1.2 The abandon terminal state was inferred from the error code — medium

`WAIT_IDLE_AFTER_ABANDON` mapped `Err::TIMEOUT` to
`OperationState::TIMED_OUT`. A *transport-level* timeout inside one callback is
not whole-operation deadline expiry, which is what `TIMED_OUT` is documented to
mean. `_abandonConversion()` now carries the intended terminal state explicitly,
so the reason and the lifecycle state cannot disagree.

### 1.3 An abandoned conversion stayed publishable — high

`README.md` states "The abandoned conversion is never published or reused."
`_conversionReady` was set in `SINGLE_SHOT_POLL_READY` and cleared only on the
success path, so a cancelled, timed-out or read-failed operation left it set and
a later compatibility `readRaw()` published that abandoned sample. It is now
cleared on every non-success exit from `SINGLE_SHOT_READ_CONVERSION` and on
`CANCELLED_AFTER_EFFECT`.

### 1.4 Enabling ALERT/RDY in continuous mode disabled readiness entirely — high, reproduced

`_readConversionReadyAt()` returned early whenever `useAlertRdyPin()` was true,
making the GPIO the *only* readiness source. Datasheet Rev. E §7.3.8 gives
roughly an **8 us** conversion-ready pulse in continuous mode, which polling
cannot catch:

| Configuration | `readConversionReady()` |
| --- | --- |
| continuous + conversion-ready thresholds + `gpioRead` | `ready == false` forever |
| the same profile with no `gpioRead` | `ready == true` correctly |

Binding the pin made the driver strictly worse.

**Fix.** The pin is now an early-accept accelerator, never the sole authority:
an asserted pin short-circuits to ready and everything else falls through to the
existing mode-appropriate timing/OS-bit path. This is also less code than the
branch it replaced.

### 1.5 `writeConfig()` PGA aliases produced a self-inflicted readback mismatch — high, reproduced

PGA codes `110b` and `111b` are datasheet aliases of `101b` (+/-0.256 V).
`writeConfig()` forwarded the alias bits to hardware but cached the canonical
`Gain::FSR_0_256V`, so the next masked CONFIG comparison disagreed with the
device:

```text
before: writeConfig(PGA=111b) -> OK, hw=0xCF83, cache=FSR_0_256V
        readConversionReady() -> Err::READBACK_MISMATCH "Config changed during
                                 conversion", hardwareConfigDirty() == true
after:  writeConfig(PGA=111b) -> OK, hw=0xCB83, cache=FSR_0_256V
        readConversionReady() -> OK, ready, not dirty
```

Combined with 1.1 this also latched the driver into permanent `BUSY`.

**Fix.** `writeConfig()` normalizes the PGA field to the canonical encoding
before the write. The three encodings are electrically identical, so nothing
changes on the wire except that cache and hardware now agree bit for bit.
`test_write_config_normalizes_datasheet_pga_aliases_for_0_256v` asserts it.

### 1.6 Direct setters diverged from the profile that `recover()` replays — high, reproduced

`setMux()`, `setGain()`, `setDataRate()`, `setMode()`, `setComparator*()` and
`setThresholds()` updated `_config` but not `_desiredProfile`. `recover()` and
`startRecover()` replay `_desiredProfile`, so the documented route back to
`VERIFIED` silently discarded the operator's change and reported success:

```text
before: setGain(0.512V) -> hw 0x4983, state UNKNOWN
        recover()       -> OK, hw back to 0x4583, gain 2.048V, state VERIFIED
after:  recover()       -> OK, hw 0x4983, gain 0.512V, state VERIFIED
```

**Fix.** `_writeConfigOnly()` and `setThresholds()` commit the mutation as the
new desired profile. The two representations can no longer diverge, which also
removes `startSingleShot()` sizing its deadline from `_config.dataRate` while
the job it starts uses `_desiredProfile.dataRate`.

### 1.7 Continuous-mode settle window was skipped in three ways — medium, reproduced

Datasheet Rev. E §7.4.2.2: "When writing new configuration settings, the
currently ongoing conversion completes with the previous configuration
settings." The driver models this with `_continuousSettlePeriods = 2`, but:

* `INITIALIZE` was exempted, so the first continuous sample after `begin()`
  could carry the previous device configuration. The device state at
  initialization is not knowable, so the exemption was unsound. Removed.
* `readLatestRaw()` reset `_continuousSettlePeriods` to 1, dropping the guard a
  preceding `setGain()`/`setDataRate()` had just armed. Removed.
* `APPLY_VERIFY_CONFIG` stamped `_conversionStartMs = nowMs` unconditionally.
  Compatibility facades drive `poll()` with `nowMs == 0` when no monotonic hook
  is configured, so `tick(50000)` measured the settle window from zero and
  reported ready immediately. It now arms the `UINT32_MAX` sentinel in that
  case, which `service(nowMs)` already knows how to resolve.

### 1.8 Readiness polling promoted configuration trust it had not verified — high, reproduced

`_readConversionReadyAt()` promoted `ConfigurationState` from `UNKNOWN` to
`VERIFIED`, rewrote `_appliedProfile` and incremented `configGeneration` after
comparing the CONFIG register alone. `VERIFIED` is documented as "Full threshold
and masked CONFIG readback succeeded", and the threshold registers were never
read. Reproduced: after `writeConfig()` moved the state to `UNKNOWN` and the
device's `Hi_thresh` was changed behind the driver's back, one readiness poll
returned the state to `VERIFIED` and re-opened `readVoltage()`.

**Fix.** A readiness poll is not a profile commit. It still detects CONFIG drift
and still returns `READBACK_MISMATCH`, but it no longer promotes trust or
invents a configuration generation. `startApplyConfigJob()`, `recover()` and
`startApplyProfile()` remain the only ways to reach `VERIFIED`.

### 1.9 Dead code, duplication, and naming

* `writeRegister16()` guarded its dirty marking with `st.code != Err::OFFLINE`.
  `Err::OFFLINE` is produced nowhere in `include/` or `src/` — the health state
  is `DriverState::OFFLINE`, a different type — so the guard could never be
  false. Removed.
* `_applyConfig()` and `_verifyConfigReadback()` were a second, independent
  implementation of "write three registers, read three registers back" serving a
  single caller, `enableConversionReadyPin()`. That caller now goes through the
  same bounded facade shape as `recover()` and `shutdown()`, and both helpers are
  deleted.
* `_buildConfigRegister()` duplicated `_buildConfigRegisterFor()` field for
  field; it is now a one-line delegation.
* `MAX_JOB_INSTRUCTIONS` renamed to `kMaxJobInstructions` to match every other
  constant in the class.
* `_jobMux` and `_jobGain` were a second authoritative copy of the MUX and gain
  already held in `_channelRequest`, kept in lockstep by hand across seven use
  sites. The read job now reads `_channelRequest` directly.
* Removed the unused `<climits>` include.
* `examples/esp_idf/basic/main/CMakeLists.txt` added the repository root to
  `INCLUDE_DIRS`. That entry was introduced to serve
  `#include "examples/01_basic_bringup_cli/main.cpp"` when the ESP-IDF example
  shared the Arduino CLI; both includes were deleted when the example was made
  native and the include directory was left behind. Nothing resolves through it:
  there is no repository-root `ADS1115/` directory, so the component's own
  `INCLUDE_DIRS "include"` is what serves `ADS1115/ADS1115.h`. The
  `EXTRA_COMPONENT_DIRS` entry in the parent `CMakeLists.txt` looks like the same
  stale path but is what makes the component discoverable; it must stay.
* `Doxyfile` `EXCLUDE_PATTERNS` listed five paths the explicit `INPUT` list can
  never reach. Three of them (`*/.pio/*`, `*/.git/*`, `*/test/*`) were also a
  latent hazard: doxygen matches these against the *absolute* path, so a checkout
  under any path containing a `test` segment would have excluded every input and
  turned the docs build into a silent no-op. The two directory guards worth
  keeping are retained with a comment saying what they protect against.

Net effect on the core: `src/ADS1115.cpp` 2495 -> 2418 lines, nine defects fixed.

### 1.10 Doxygen contracts that did not match the implementation

Corrected: `readRaw()` and `readBlocking()` transaction counts, the readiness
time-source requirement, `pollApplyConfig()`'s `nowMs` (documented as "reserved
for symmetry" while it drives the deadline, the timeout partition and health
timestamps), `setThresholds()` ordering versus the conversion-ready pattern, the
dirty-state effect of `cancelActiveOperation()`, `disableComparator()`'s
threshold behavior, `getConfig()`'s exposure of the live transport callbacks,
`JobState::SINGLE_SHOT_WAIT_CONVERSION`'s ALERT/RDY claim, the
`validateComparatorProfile()` contract, and the `Mux::AIN0_AIN1` "(default)"
marker on a value nothing in the library defaults to.

### 1.11 Example CLIs printed `UNKNOWN` for the states that matter

`jobStateToStr()` in the ESP-IDF CLI was missing `PROBE_CONFIG`,
`WAIT_IDLE_AFTER_ABANDON`, `SHUTDOWN_WRITE_CONFIG`, `CANCELLED` and `TIMED_OUT`;
the Arduino CLI was missing the first and third. Cancellation and reconciliation
are exactly what an operator watches during a HIL run. Both now name every
`JobState` value.

### 1.12 Documentation cleanup

* `AGENTS.md`: removed the "Hardening Subagent Roles" section, the "Chunked
  Hardening Workflow" prompt framing, the "You are a professional embedded
  software engineer" role prompt, the "each hardening prompt must end with a
  commit" rule, and git-sync narration filed under "## PlatformIO". Corrected
  the release steps to use `scripts/generate_version.py`, which owns the version
  in four files — the previous steps would fail CI. Completed the repository
  tree, which omitted `test/`, `tools/`, `scripts/`, `docs/` and CI. Corrected
  the constants naming rule to match the code.
* `README.md`: removed the embedded HIL run report (cycle counts, command
  counts, worst-case latency, a runner-defect narrative) and the "Do not report
  an unrun build as passed" contributor instruction. Replaced the release-note
  opening paragraph and its hand-maintained test count. Collapsed three
  overlapping documentation indexes into one. Corrected the single-shot read
  callback bound and added the general-call-reset limitation.
* `docs/ADS1115_HARDWARE_VALIDATION_PLAN.md`, `docs/OPEN_ITEMS.md`: removed the
  duplicated `ads1115_559933a_20260804` evidence narrative. That record now
  appears once, in the `CHANGELOG.md` entry for the release that produced it.
* `docs/README.md`, `docs/OPEN_ITEMS.md`, `CONTRIBUTING.md`: the
  evidence-retention rule was stated four times; the validation plan now owns it
  and the others point at it.
* `docs/IDF_PORT.md`, `docs/reference/README.md`: removed a stale version stamp
  and a defensive "this is not a release-readiness statement" disclaimer.
* `CHANGELOG.md` 2.0.0: removed a `TunnelMonitor` cross-project note and a
  pointer to captures that no longer exist.
* Stripped UTF-8 BOMs from `AGENTS.md`, `CHANGELOG.md`,
  `.github/workflows/ci.yml` and `scripts/generate_version.py`, which
  `.editorconfig` (`charset = utf-8`) forbids. The one in the Python script sat
  before the shebang, so `./scripts/generate_version.py` could not execute
  directly on a POSIX host.
* Verified every relative Markdown link in the repository resolves.

---

## 2. Open findings

### 2.1 `getThresholds()` is an observer that reconfigures the driver — high, reproduced

`getThresholds()` copies the values it read into `_config.compThresholdLow/High`.
Two consequences:

* **It can poison the apply path.** With the comparator enabled and the device
  holding the conversion-ready pattern (`Lo = 0x0000`, `Hi = 0x8000`),
  `getThresholds()` adopts `high <= low`, and the next `startApplyConfigJob()`
  fails with `Err::INVALID_CONFIG` "Invalid comparator thresholds". A pure read
  broke a write path, and the repair is a `setThresholds()` call the caller has
  no reason to know it needs.
* **It can switch the readiness source.** If the adopted pair matches the
  conversion-ready pattern and `compQueue == ASSERT_1`,
  `usesAlertRdyPinForConversionReady()` flips and readiness starts depending on
  a GPIO the application may not have wired.

**Proposal.** Make `getThresholds()` a pure observer: report what hardware
holds, keep the existing `VERIFIED -> UNKNOWN` invalidation on mismatch, and
stop writing `_config`. Update the header line "Read signed comparator
thresholds and sync the legacy cache" accordingly.

Not applied because it changes documented behavior; two tests
(`test_thresholds_accept_and_reconstruct_int16_boundaries` at
`test/test_basic.cpp:2851` and
`test_threshold_diagnostic_read_invalidates_full_profile_trust` at
`test/test_basic.cpp:3022`) assert the sync and need the same edit.

### 2.2 A single-shot read is not bounded to three callbacks — high, reproduced

`SINGLE_SHOT_POLL_READY` re-arms at `nowMs + 1` and re-reads CONFIG every
millisecond until the operation deadline. Measured with a device holding
`OS = 0`: **11 callbacks** in eleven polls, and on a 500 ms deadline up to
roughly 500 CONFIG reads on a shared bus.

The retry is correct — it is the right response to an oscillator slower than the
datasheet's -10% bound — but the cadence is wrong for a shared bus. The
documentation has been corrected (1.10); the cadence has not.

**Proposal.** Re-arm the readiness poll at a data-rate-derived interval instead
of 1 ms, for example `worstCaseConversionTimeUs() / 8` clamped to at least 1 ms.
At 8 SPS that is one retry per ~17 ms instead of 140 retries per conversion.

### 2.3 Health diagnostics are silent for the failure that matters most — high, reproduced

`_updateHealth()` returns early while `!_initialized`, and `_initialized` becomes
true only after a *successful* profile verification. A device that never answers
therefore reports:

```text
state() == UNINIT   isOnline() == false
totalFailures() == 0   consecutiveFailures() == 0   lastErrorMs() == 0
```

The health API cannot distinguish "never tried" from "tried and the device is
dead", which is the case an integrator wants it for. The seven successful
transfers of a good initialization are not counted either, so `totalSuccess()`
reads 0 immediately after a successful `startInitialize()`.

**Proposal.** Split the two responsibilities the guard conflates. Always update
counters, timestamps and `_lastError` from the tracked wrappers; keep the
`_initialized` guard only on the `DriverState` transition to `DEGRADED`/
`OFFLINE`, which is what "no health state before initialization" was meant to
protect. `AGENTS.md` "Health Tracking Rules" needs the same split.

### 2.4 `INITIALIZE` and `RECOVER` report the same fault differently — high, reproduced

`INITIALIZE` probes with `_probeRaw()` (untracked, maps `I2C_NACK_ADDR` to
`DEVICE_NOT_FOUND`); `RECOVER` probes with `_readRegister16Tracked()` (tracked,
returns the transport code unchanged). Against one dead device:

| Operation | Status | `state()` | `isOnline()` | `totalFailures()` |
| --- | --- | --- | --- | ---: |
| re-`startInitialize()` | `DEVICE_NOT_FOUND` | `READY` | true | 0 |
| `startRecover()` | `I2C_NACK_ADDR` | `DEGRADED` | true | 1 |

Re-initializing a dead device leaves the driver claiming `READY`.

**Proposal.** Give both operations the same probe step: a tracked CONFIG read
with the `I2C_NACK_ADDR -> DEVICE_NOT_FOUND` mapping applied in both cases. Keep
`probe()` itself on the raw path, which is its documented diagnostic contract.
With 2.3 applied this also makes a failed initialization visible in the counters.

### 2.5 `readBlocking()` can return with the operation still active — high, reproduced

The `Err::CLOCK_STALLED` path enters reconciliation and returns immediately.
Measured with a frozen clock: `readBlocking()` returns `CLOCK_STALLED` while
`jobActive() == true`, `jobState() == WAIT_IDLE_AFTER_ABANDON` and `recover()`
answers `BUSY`. The token was a local inside `readBlocking()`, so the only way
out is `activeOperationToken()` + `poll()` + `takeResult()`, which the header
never mentions. A stalled clock is a fatal system condition, but the driver
should not leave an operation the caller cannot obviously finish.

**Proposal.** Return the token to the caller. Either add a `readBlocking()`
overload with an `OperationToken&` out-parameter, or document
`activeOperationToken()` + `poll()` + `takeResult()` as the required cleanup
path in the header (the second is a documentation-only change and matches how
both example CLIs already recover a token).

### 2.6 `_markHardwareConfigDirty()` contradicts the documented contract — medium

`README.md`: "The driver preserves the original transport/readback error through
`hardwareConfigDirtyError()`." `_markHardwareConfigDirtyIfClean()` does that;
`_markHardwareConfigDirty()` overwrites unconditionally, and most call sites use
the second. A later failure therefore erases the first error, which is the one
that explains the partial write.

**Proposal.** Make preserve-first the default: rename the preserving helper to
`_markHardwareConfigDirty()` and give the overwriting one an explicit name such
as `_replaceHardwareConfigDirty()`, used only where the driver deliberately
records a new cause (`writeRegister16()` after a successful raw write). Audit
the call sites once against that rule.

### 2.7 `kMaxSameTickPolls` is calibrated against an unstated assumption — medium

`readBlocking()` spins on `poll()` and declares `Err::CLOCK_STALLED` after 1024
iterations inside one observed millisecond. On a 240 MHz ESP32 with no
`cooperativeYield` hook an iteration is well under a microsecond, so a healthy
1 ms-resolution timebase can plausibly reach 1024 same-tick polls before the tick
advances — a spurious hard failure, most likely at the slowest data rates where
the wait is longest.

**Proposal.** The guard only has to be finite, not tight. Widen
`kMaxSameTickPolls` to a value no real millisecond can reach (`100000U`, type
widened to `uint32_t`), and state the assumption in the header: the guard detects
a clock that has stopped, not a clock that is coarse. `test/test_basic.cpp:3712`
asserts `st.detail == kMaxSameTickPolls` and needs no change.

### 2.8 Conversion-ready validation is narrower than the datasheet — medium

Datasheet Rev. E §7.3.8 requires only `Hi_thresh` MSB = 1, `Lo_thresh` MSB = 0
and `COMP_QUE != 11b`, and states that `COMP_MODE` and `COMP_LAT` "no longer
control any function". `validateComparatorProfile()` additionally demands
`mode == TRADITIONAL`, `latch == NON_LATCHING`, `queue == ASSERT_1`,
`lowThreshold == 0` and `highThreshold == 0x8000`, and
`isAlertRdyModeConfigured()` recognizes only that exact pair. Legal
configurations are rejected with `INVALID_CONFIG`, and a device another master
put into a legal ready configuration is not recognized as being in one.

**Proposal.** Validate what the datasheet requires — `highThreshold < 0`,
`lowThreshold >= 0`, `queue != DISABLE` — and recognize the same condition in
`isAlertRdyModeConfigured()`. Keep the current exact pattern as what the driver
*writes*; only widen what it *accepts*.

### 2.9 `enableConversionReadyPin()` / `disableComparator()` are asymmetric — medium, reproduced

`disableComparator()` only sets `COMP_QUE = 11b`, leaving the ready-pattern
thresholds cached and on the device. A later `setComparatorQueue()` then enables
a comparator whose thresholds are inverted, and `startApplyConfigJob()` rejects
the resulting cached profile with `INVALID_CONFIG` — a state reached entirely
through documented setter calls. `enableConversionReadyPin()` also does not
require `alertRdyPin`/`gpioRead` to be configured.

**Proposal.** Have `disableComparator()` restore the reset thresholds
(`Lo = 0x8000`, `Hi = 0x7FFF`) alongside the queue write, through the same
apply-and-verify path `enableConversionReadyPin()` now uses. Documented in the
header as an interim measure (1.10).

### 2.10 `startShutdown()` leaves `_desiredProfile` desynchronized — medium, reproduced

The shutdown branch of `APPLY_VERIFY_CONFIG` sets `_config.mode` and
`_appliedProfile.mode` to `SINGLE_SHOT` but not `_desiredProfile.mode`. Measured
after shutting down a `CONTINUOUS` profile:

```text
desired.mode = CONTINUOUS   applied.mode = SINGLE_SHOT   config.mode = SINGLE_SHOT
configurationState = VERIFIED
startRead() -> Err::UNSUPPORTED_OPERATION "Owner-safe reads require single-shot mode"
```

The hardware is in single-shot mode and the driver refuses the read anyway,
because `startRead()` tests `_desiredProfile.mode`. `startShutdown()` also never
sets `ConfigurationState::APPLYING`, unlike every other profile-changing
operation.

**Proposal.** Treat shutdown as what it is — an apply of the current profile with
`mode = SINGLE_SHOT` — and set `_desiredProfile.mode` alongside the other two,
with `APPLYING` for the duration.

### 2.11 `configGeneration` changes on every sample — medium

`SINGLE_SHOT_POLL_READY` increments `_configGeneration` after each read-specific
MUX/PGA verification. `SampleResult::configGeneration` is therefore different for
every sample, which defeats its only obvious use: grouping samples taken under
one configuration. `README.md` documents the behavior, so this is a design
question rather than a defect.

**Proposal.** Increment only on a profile *commit* (initialize, apply, recover)
and let a read refresh `_appliedProfile.defaultMux/defaultGain` without a new
generation. If the current meaning is intentional, rename the field to something
that does not read as "profile version".

Do **not** also remove the `_configurationState = VERIFIED` assignment in the
same block while doing this. `startRead()` sets `APPLYING` on entry and
`_finishOperation()` never restores configuration state, so that line is the only
exit from `APPLYING`; removing it wedges every subsequent `startRead()` at
`CONFIG_UNKNOWN`. It restores the trust the operation inherited -- `startRead()`
requires `VERIFIED` to be accepted at all -- rather than promoting from a weaker
state, which is what 1.8 fixed.

### 2.12 The owner-safe API is less configurable than the compatibility one — medium

`DriverConfig` carries only the two callbacks, a context and a timeout. An
application using `bind()` and never `begin()` cannot set `offlineThreshold`, a
monotonic clock, or the ALERT/RDY GPIO, so `_config.offlineThreshold` stays 5,
`lastOkMs()`/`lastErrorMs()` always report 0, and the GPIO readiness path is
unreachable. That is defensible for the production path — the owner supplies
`nowMs` to `poll()` and owns its own health policy — but it is not stated.

**Proposal.** Document it in `DriverConfig`: the owner-safe path deliberately
takes its time domain from `poll(nowMs, ...)`, does not sample GPIO, and treats
health as passive with a fixed threshold. If a threshold is genuinely wanted,
add it to `DriverConfig` rather than requiring `begin()`.

### 2.13 Packaging constraints do not match the code — medium

* `CMakeLists.txt` sets `target_compile_features(${COMPONENT_LIB} PUBLIC
  cxx_std_17)`, which forces C++17 on every consumer of the component. The core
  compiles clean at `-std=c++11 -Wall -Wextra -Wpedantic`, so `PUBLIC cxx_std_11`
  (or `PRIVATE`) is enough and stops the driver from dictating the application's
  standard.
* `idf_component.yml` has no `files:` filter, so the ESP-IDF package ships
  `docs/` (2.8 MB including the datasheet PDF), `test/` and `tools/`.
  `library.json` already has an `export.include` allow-list; the IDF manifest
  needs the equivalent.
* `idf_component.yml` `targets:` lists only `esp32s2`/`esp32s3`, and
  `library.json` `platforms:` only `espressif32`, for a driver with no platform
  dependency at all. That is a validation claim, not a technical one, and it
  blocks reuse in another firmware — which is an explicit goal for this library.

**Proposal.** Relax `cxx_std_17` to `cxx_std_11`, add a `files:` allow-list
mirroring `library.json`, and either widen the target list or state in
`README.md` that the restriction records where the driver has been validated,
not where it can run.

### 2.14 `setThresholds()` cannot express the conversion-ready pattern — low

`setThresholds()` rejects `high <= low`, correct for a comparator and wrong for
the ready pattern (`low = 0`, `high = 0x8000`). `enableConversionReadyPin()` is
the only route. Documented in the header (1.10); no code change proposed.

### 2.15 `startSingleShot()` and `startApplyConfigJob()` discard their token — low

Both call a `start*()` overload that returns an `OperationToken` and throw it
away, so a caller cannot reach `takeResult(token, ...)` without knowing to ask
`activeOperationToken()` first. Both example CLIs do exactly that, which is
evidence the API is awkward rather than that it is fine. Their headers also
document only `IN_PROGRESS` while both carry `NOT_INITIALIZED`, `INVALID_CONFIG`,
`BUSY` and `CONFIG_UNKNOWN` preconditions.

**Proposal.** Add `OperationToken&` out-parameter overloads next to the existing
signatures, document `activeOperationToken()` as the recovery route for the old
ones, and list the real precondition statuses.

### 2.16 `pollSingleShot()`/`pollApplyConfig()` succeed when nothing is running — low, reproduced

With no operation active the guard `_operationKind != X && (_jobActive ||
_terminalResultAvailable)` is false, so both fall through to `poll()` and return
`status == OK`, `done == true`, `state == IDLE`. A caller that treats `done` as
"my job finished" sees success for a job it never started.

**Proposal.** Return `Err::RESULT_NOT_AVAILABLE` when `_operationKind` is `NONE`,
matching `takeResult()`.

### 2.17 Preconditions are inconsistent across `start*()` — low, reproduced

`startApplyProfile()`, `startRead()` and `startShutdown()` require
`_initialized`; `startInitialize()` and `startRecover()` require only `_bound`.
The headers state neither. `getAppliedProfile()` writes its output parameter
before checking whether the driver is bound, so a caller that ignores the
`NOT_INITIALIZED` return silently reads a default-constructed profile.

**Proposal.** State the precondition on each `start*()` and check `_bound`
before writing in `getAppliedProfile()`.

### 2.18 Compatibility surface that no longer carries weight — low

* `Config::strictInitVerify` is inert. `bind()` takes a `DriverConfig`, which has
  no such field, and `begin()` never copies the caller's value at all: it copies a
  fixed list of hook fields and then forces `true`. Both assignments are
  redundant, because `bind()` calls `unbind()`, which resets `_config = Config{}`,
  and `Config`'s own default is already `true`. The field is documented as inert
  in `Config.h`, in `SettingsSnapshot`, and in the README migration table, so this
  is a naming wart rather than a defect.
* `SettingsSnapshot::hardwareConfigUncertain` aliases `hardwareConfigDirty`,
  `lastConfigApplyError` aliases `hardwareConfigDirtyError`, `timebaseAvailable`
  aliases `hasNowMsHook`, and `operationToken` is read nowhere.
* `driverState()`, `isHardwareConfigUncertain()`, `isHardwareConfigDirty()`,
  `lastConfigApplyError()`, `readRegister()` and `writeRegister()` exist only to
  satisfy an "aliases still compile" test.
* `Err::MEASUREMENT_NOT_READY` is an enumerator alias of
  `CONVERSION_NOT_READY`, so a `switch` cannot handle both names.

**Proposal.** These are 1.x compatibility surface, so removal is a major version
change. Mark them `[[deprecated]]` now and schedule removal for 3.0 in
`CHANGELOG.md`. Do not renumber `Err` values; their numeric stability is a
documented contract.

### 2.19 `_conversionStartMs` sentinel collides with a legal timestamp — low, reproduced

`UINT32_MAX` doubles as "no timebase yet" and as a legal millisecond value, so
`getSettings().conversionStartMs` reports `0` for one millisecond every 49.7
days, and `service()` re-arms the sentinel if an external tick reports exactly
`0xFFFFFFFF`.

**Proposal.** Replace the in-band sentinel with an explicit
`bool _conversionStartMsValid`.

### 2.20 `Version.h` embeds `__DATE__`/`__TIME__` — low

`ADS1115_BUILD_TIMESTAMP` expands `__DATE__ " " __TIME__` in a header included by
every translation unit, which makes builds non-reproducible and defeats
ccache-style reuse for anything that includes it.

**Proposal.** Move the timestamp macros into `src/ADS1115.cpp` behind a function
that returns the string, or have `scripts/generate_version.py` bake a fixed
timestamp at generation time.

### 2.21 `CommandTable.h` field-value constants are unreferenced — low

`MUX_*`, `PGA_*`, `MODE_*`, `DR_*` and `COMP_*_*` (36 constants) plus
`MASK_CONVERSION`, `MASK_LO_THRESH`, `MASK_HI_THRESH` and `OS_BUSY` are used
nowhere. The driver builds the register by shifting enum values, so nothing
cross-checks the two encodings against each other.

**Proposal.** Keep them — they are the vocabulary a `writeConfig()` /
`writeRegister16()` caller needs — but earn them with a `static_assert` block
tying each constant to the shifted enum it duplicates, so editing either side
fails to compile. That turns dead data into a compile-time datasheet check, and
makes `test_basic.cpp:2688`'s currently tautological encoding test meaningful.

### 2.22 The test fake does not behave like an ADS1115 — medium

`FakeBus` is a four-word register file. Compared with the device it models:

* the I2C **address argument is ignored** (`fakeWrite(uint8_t, ...)`), so no test
  in the 178-test suite can detect an addressing regression;
* CONFIG echoes the written `OS` bit, so the device always reads "conversion
  complete" instantly and every readiness test has to inject
  `configReadXorMask` by hand;
* writes to register `0x00` are accepted, though the conversion register is
  read-only;
* a failed write never reaches the register file, so the partial-write "dirty"
  tests assert the fake's model rather than the driver's behavior;
* the clock advances only on writes, so a slow *read* callback overrunning the
  poll deadline is untestable;
* `resetIoCounters()` clears `failWriteOnCall`/`failReadOnCall` but not the
  sticky `writeStatus`/`readStatus`, so fault injection leaks between phases.

Defects 1.1, 1.4 and 1.7 all survived because of these gaps. The GPIO readiness
arm itself is not untested -- `test_alert_ready_pin_path_does_not_poll_config_register`
(`test/test_basic.cpp:1807`) and
`test_no_clock_alert_ready_pin_uses_external_service_timebase` (`:1590`) both
cover it -- but both do so in single-shot mode, which is why the continuous-mode
dead end in 1.4 went unnoticed.

**Proposal.** Give the fake a small device model: honor the address, drive `OS`
from a virtual conversion deadline, reject writes to `0x00`, and apply a failed
write to the register file when the test asks for an *ambiguous* failure.
`OperationState::INDETERMINATE`, `JobState::PROBE_CONFIG`,
`SHUTDOWN_WRITE_CONFIG` and `TIMED_OUT` have no coverage today and become
reachable once the fake can model them.

### 2.23 Tooling gaps — low

* `tools/run_i2c_hil.py:120` — `CommandSpec.failure_tokens` defaults to `()` and
  no spec sets it, so the failure scan at line 281 never executes.
* `tools/run_i2c_hil.py:48` — `RESULT_UNKNOWN` is assigned only inside the
  parser self-test, so three aggregation branches and the `UNKNOWN` counter are
  unreachable in a real run.
* `tools/run_i2c_hil.py` — the full suite never restores gain, mux or rate before
  its final `recover`, so a saturating configuration can be cemented as the
  verified profile; and it writes `0x8583` to CONFIG through `wreg`, which starts
  a conversion the driver does not know about.
* `tools/check_cli_contract.py:60` — the mandatory-command check is satisfied by
  the help text alone, so a command whose dispatch was deleted still passes.
* `.clang-format` is enforced by nothing in CI, so the style it declares is
  advisory only.

**Proposal.** Delete `failure_tokens` and the unreachable `RESULT_UNKNOWN`
branches, make the CLI contract check look for the dispatch and not just the help
line, and add a `clang-format --dry-run --Werror` CI step.

Two things here look like gaps and are not; recorded so they are not
re-reported. `tools/check_idf_example_contract.py`'s `ArduinoCompat` /
`IdfArduinoCompat` entries are a *forbidden*-token list, where matching nothing is
the passing condition. And `.clang-format`'s
`ConstructorInitializerAllOnOneLineOrOnePerLine`,
`AllowAllConstructorInitializersOnNextLine` and `AlignOperands: true` keys are
deprecated but not removed: clang-format 22 parses them silently and resolves
them to exactly the modern spellings, and a genuinely unknown key makes it error
out loudly rather than fall back to another style.

### 2.24 The owner-safe API has no hardware exercise path — medium

Neither bring-up CLI exposes `bind()`, `startInitialize()`, `startRecover()`,
`startShutdown()`, `poll()`, `cancelActiveOperation()` or `unbind()`; both drive
the compatibility facades. `examples/02_owner_safe_poll` uses the owner API but
has no CLI, dead-ends in `AppState::FAILED` on the first transient error, and
never demonstrates cancellation or reconciliation. So the production path is the
one with no HIL coverage, and 1.1 — a wedge in exactly that path — was
undetectable on hardware. `examples/common/BoardConfig.h:44` also hard-disables
ALERT/RDY, so neither example ever installs a GPIO readiness path, which left 1.4
with host coverage in single-shot mode only and none on hardware.

**Proposal.** Add owner-safe commands to the Arduino CLI (`own bind`,
`own init`, `own read <ch>`, `own cancel`, `own poll <n>`, `own recover`,
`own shutdown`, `own unbind`) so `tools/run_i2c_hil.py` can drive the production
lifecycle, and make `examples/02_owner_safe_poll` recover through
`startRecover()` instead of latching `FAILED`.

---

## 3. Verified correct

* `CommandTable.h` masks, bit positions, field values and reset defaults match
  datasheet Rev. E pp. 24-27, including the `0x8583` CONFIG reset value and the
  three `+/-0.256 V` PGA encodings.
* `worstCaseConversionTimeUs()` = `ceil(1e7 / (9 * sps)) + 1000 us` correctly
  implements the datasheet's -10% data-rate tolerance (p. 5, "Data rate
  variation, all data rates, -10% / 10%"). Checked numerically for all eight
  rates: 8 SPS -> 139889 us, 128 SPS -> 9681 us, 860 SPS -> 2292 us, against the
  datasheet's own "approximately 1.2 ms at 860 SPS" (p. 18).
* `operationDeadlineMs()` rounding, `INT32_MAX` saturation, and its zero return
  for an invalid channel count or data rate.
* `rawToMicrovolts()` rounds half away from zero and is exact at `INT16_MIN`,
  `INT16_MAX`, `0` and `+/-1` for every gain; the `int64` intermediate cannot
  overflow `int32`. `getLsbVoltage()` matches the datasheet LSB table.
* Wrap safety across `UINT32_MAX`: `_deadlineReached()`, `elapsedAtLeast()`,
  `_jobNextReadyPollMs` and the `(int32_t)(a - b)` idioms. An initialize and a
  read that straddle the wrap both complete correctly.
* `poll()`'s per-callback timeout partition. `remaining / budget` cannot reach
  zero while `budget > 0`, because `budget` is first clamped to `remaining`.
* The classic ADS1115 `OS`-bit race is avoided. Datasheet p. 18: after `OS = 1`
  the device "powers up in approximately 25 us, resets the OS bit to 0b, and
  starts a single conversion." The driver waits a full worst-case conversion
  interval before its first `OS` read, so it can never mistake the pre-start idle
  level for completion; the 1 ms guard covers the 25 us power-up.
* Writing `OS = 1` during an active conversion has no effect (p. 18), and
  `_singleShotMayBeActive()` prevents it independently.
* `include/` and `src/` are free of Arduino, Wire, ESP-IDF, FreeRTOS, logging,
  heap and global-bus dependencies (`tools/check_core_timing_guard.py`), and
  compile clean at `-std=c++11 -Wall -Wextra -Wpedantic`.
* The ESP-IDF adapter uses `i2c_master_transmit_receive()`, which issues the
  repeated start the pointer-register protocol requires.
* No general-call reset (`0x06` to address `0x00`). Correct for a library that
  does not own the bus — a general call resets every device on it — and now
  stated as a deliberate limitation in `README.md`.
* The `[[deprecated]]` marker on `bool conversionReady()` does produce a
  compiler warning at the call site; the two overloads are not silently
  interchangeable.

---

## 4. Reproducing

```bash
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -fsyntax-only src/ADS1115.cpp
python -m platformio test -e native
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
```
