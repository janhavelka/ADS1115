# ADS1115 library suitability for TunnelMonitor-node

**Audit date:** 2026-07-18

**Decision:** not suitable unchanged; good base for a focused breaking refactor

**Recommended first scope:** fixed-profile, poll-driven single-shot acquisition with comparator and ALERT/RDY disabled

**Library candidate audited:** `v1.2.0`, commit `9c9ade807f18eabd534423c64b2dfe02efe43de4`

**Remote revalidation:** on 2026-07-18, `origin` was fetched with tags and pruning. `origin/HEAD` points to `origin/main`; local `main`, `origin/main`, and release tag `v1.2.0` resolve to the audited commit above. No fetched remote branch has a newer commit. The final source was re-read and every hard finding, source reference, severity, and recommended action was checked again. There is no code delta from the audited revision, and all findings remain current.

## Executive summary

The ADS1115 library has a strong chip-level foundation. It uses an injected I2C transport, has fixed memory, typed MUX/PGA/data-rate settings, signed 16-bit decoding, explicit dirty-state diagnostics, bounded poll instruction budgets, deleted copy/move operations, native tests, Arduino and ESP-IDF examples, and retained hardware evidence. These parts should be kept.

The library is not safe to integrate unchanged into TunnelMonitor's `I2cTask`. The most serious defect is conversion cancellation and state recovery:

1. A single-shot conversion is started in hardware.
2. The job is cancelled or a blocking call times out.
3. The library clears its software conversion flag even though the ADC may still be converting.
4. A new conversion is requested for another MUX.
5. ADS1115 ignores `OS=1` while a conversion is already active.
6. The earlier conversion can be read and reported as the newer channel.

That is a wrong-channel data defect, not only a diagnostic issue.

Other required fixes are:

- `begin()`, `recover()`, and some configuration paths perform multiple synchronous I2C transfers.
- Poll-driven single-shot jobs have no whole-operation deadline.
- Direct configuration setters are allowed while a direct single-shot conversion is active.
- Dirty or unknown MUX/PGA configuration does not block labelled and scaled samples.
- Staged results are stored only in a shared `lastRawValue` without MUX, gain, time, sequence, validity, or configuration generation.
- Continuous-mode freshness is incorrect after MUX/PGA/rate changes and can be early at 8 SPS and 16 SPS.
- Driver-owned OFFLINE admission conflicts with TunnelMonitor's I2C owner health and recovery policy.
- Default initialization readback is optional even though ADS1115 has no identity register.
- ALERT/RDY GPIO ownership and comparator behavior are not suitable for the first platform profile.

TunnelMonitor also has no current ADS1115 product or board contract. The project must first decide whether ADS1115 replaces INA228 as the power-monitor implementation or measures a new set of analog signals. Replacement can reuse existing power contracts only if the analog front end produces the same bus-voltage/current meanings and quality behavior. A separate ADC source needs explicit new contracts and selected-profile capacity changes.

The preferred solution is a focused library refactor, followed by a fixed compile-time product profile. Do not add a second firmware state machine that tries to compensate for wrong-channel, dirty-state, or blocking-lifecycle behavior.

## Suitability by use case

| Use case | Current fit | Fit after focused refactor | Comment |
|---|---:|---:|---|
| Service probe and raw diagnostics | Partial | Good | ADS1115 has no chip ID; probe is only register plausibility. |
| Low-rate one-channel single-shot sample | No | Good | Recommended first library and TunnelMonitor scope. |
| Fixed sequence of up to four channels | No | Good | Requires atomic per-channel results, a derived deadline, and skew policy. |
| Replacement for INA228 power monitor | Undefined | Conditional | Only valid with a proven analog front end and identical project-level meanings. |
| Separate analog measurement source | Undefined | Conditional | Requires new device, operation, result, health, status, and sample contracts. |
| Continuous fresh-sample stream | No | Possible later | Current freshness and reconfiguration semantics are unsafe. |
| Comparator alarm or conversion-ready interrupt | No | Conditional | Requires board pin/electrical design and typed event ownership. |

## Audit basis

### Exact source revision

The audited checkout was clean and current at:

- tag: `v1.2.0`
- commit: `9c9ade807f18eabd534423c64b2dfe02efe43de4`
- branch/tracking state: `main`, `origin/main`, and `origin/HEAD`
- commit date: 2026-06-25

All ADS1115 source line references in this report refer to that exact revision. TunnelMonitor references refer to its working tree on 2026-07-18.

### Evidence reviewed

- Public headers in `include/ADS1115/`
- Core implementation `src/ADS1115.cpp`
- Native tests in `test/test_basic.cpp`
- Examples, package metadata, CI, and repository guard scripts
- `README.md` and current validation documents
- Archived reports only as historical evidence
- Bundled TI ADS1113/ADS1114/ADS1115 datasheet, SBAS444E, revised December 2024
- TunnelMonitor architecture authority under `docs/guidelines/`
- TunnelMonitor board, I2C, measurement, status, capacity, storage, and settings contracts

The bundled TI PDF was visually checked on the pages covering electrical limits, data-rate tolerance, analog input impedance, PGA/LSB values, conversion modes, comparator and conversion-ready behavior, register fields, reset, power sequencing, and application wiring. Extracted text was used for search and source location, but findings were checked against rendered pages.

This report is engineering evidence. It does not override TunnelMonitor's `docs/guidelines/` architecture authority.

## Product decision required before integration

ADS1115 is not named in the current TunnelMonitor hardware or product contract. There is no address, ALERT/RDY pin, channel list, analog front end, calibration, engineering unit, or sample field for it.

Evidence:

- `include/TunnelMonitor/BoardPins.h:26-30`
- `include/TunnelMonitor/BoardPins.h:80-87`
- `docs/guidelines/target_architecture.md:118-139`
- current PlatformIO dependency list

Two reasonable product shapes exist.

### Option A - ADS1115 replaces INA228

Use this only if a fixed analog circuit produces the same project meanings currently represented by `PowerReadResult`, `PowerStatus`, and the durable power sample fields.

Reasonable shape:

- keep `DeviceId::PowerMonitor`
- keep `I2cOperation::ReadPower`
- keep one power-monitor health row
- keep one measurement source slot
- select INA228 or ADS1115 through a compile-time hardware profile
- keep the chip adapter private to `I2cTask`

Do not create runtime polymorphism or a generic sensor registry. A small compile-time selection is enough.

Replacement is not valid merely because ADS1115 can measure a voltage. The final circuit must prove bus voltage, shunt/current scaling, polarity, expected range, fault flags, calibration, and required-field behavior equivalent to the existing power contract.

### Option B - ADS1115 is a distinct analog source

Use this if the channels measure other signals. Add a real ADC contract rather than disguising the values as ENV or power data.

This path needs:

- `DeviceId::Adc`
- `I2cOperation::ReadAdc`
- a fixed-profile `AdcReadCommand`
- an `AdcReadResult`
- an ADC health row and optional public status
- measurement-source and sample-schema changes
- storage, replay, cloud, and interface tests

The product decision must state what every populated channel means. `AIN0`, `AIN1`, and raw code are not durable product meanings.

## TunnelMonitor constraints

### One I2C owner

`I2cTask` is the only bus owner. It owns scheduling, deadlines, transfer timeout, bus recovery, retry policy, results, device health, and diagnostics.

Relevant authority:

- `docs/guidelines/ownership.md:42-48`
- `docs/guidelines/i2c_peripherals.md:100-118`
- `docs/guidelines/i2c_peripherals.md:303-327`

Normal owner work is one backend transfer or library callback per poll. The ADS library must not block across a complete initialization, recovery, or multi-channel acquisition.

Current project facts:

- I2C bus: 400 kHz
- command/result queue depth: 8/8
- normal owner poll budget: 5 ms
- normal optional-device transfer cap: 20 ms
- owner-facing uptime: wrap-safe 64-bit

The library default transfer timeout is 50 ms. TunnelMonitor must supply its approved 20 ms per-transfer cap through the injected transport.

### Address compatibility

ADS1115 supports `0x48` through `0x4B`. These do not collide with the current board addresses.

For a new board, `0x48` with ADDR tied to GND is the simplest choice. ADDR tied to SDA or SCL provides no current benefit and creates additional electrical dependence. Freeze one address in the board profile. Do not probe all four as runtime product selection.

ADS1115 has no device-ID register. A readable CONFIG register and successful readback improve confidence but cannot prove identity. Address inventory and final-board HIL are therefore important.

### No ALERT/RDY pin

The current `BoardPins` has no ADS1115 ALERT/RDY GPIO. The first integration should use single-shot OS-bit polling and keep comparator output disabled.

If a later product requires alarms or conversion-ready edges, add a reviewed board-profile pin, pull-up, polarity, edge/latch behavior, and HIL evidence. The ISR may record a bounded edge/timestamp only. I2C remains in `I2cTask`.

### Capacity facts

Current fixed capacities are already tight:

- known I2C device table: exactly five rows
- public production health inventory: exactly 16 of 16 rows
- active measurement sources: exactly four of four
- sample fields: 37 of 48
- settings rows: 49 of 51
- I2C result payload: 128 bytes

Evidence:

- `include/TunnelMonitor/i2c/I2cConfig.h:81-82`
- `src/i2c/I2cDiagnostics.cpp:45-54`
- `include/TunnelMonitor/contracts/Capacities.h:81-100`
- `src/system/SystemHealthProjection.h:73-91`
- `include/TunnelMonitor/measurement/MeasurementScheduler.h:12-16`
- `src/measurement/MeasurementRuntime.cpp:34-73`
- `include/TunnelMonitor/contracts/Sample.h:41-81`
- `docs/guidelines/settings.md:330-341`

An INA228 replacement can keep the existing counts. A coexisting ADC needs at least one additional device-health row and one additional measurement-source slot. The selected product profile must calculate these counts and fail at compile time if they do not fit.

Four new scalar product fields fit the current sample value capacity, leaving seven free slots. Capacity alone does not authorize the schema change. The sample profile ID, descriptors, CSV, replay, cloud payload, masks, and compatibility behavior must change together.

## Strengths to retain

The library should be refactored rather than replaced without cause. Keep these parts:

- Framework-neutral core and headers.
- Injected, non-owning I2C transport with explicit per-transfer timeout.
- No `Wire`, Arduino, ESP-IDF, task, mutex, logging, pin setup, or bus-recovery ownership in the core.
- No heap allocation or dynamic steady-state containers.
- Typed support for all four legal addresses, eight MUX values, six PGA ranges, and eight data rates.
- Correct signed two's-complement conversion-register decoding.
- Correct PGA register encoding, including normalization of the two alias encodings for the smallest range.
- Structured `Status` results for most fallible operations.
- Explicit dirty-state diagnostics after raw and ambiguous writes.
- Preservation of the original transport error for partial multi-register writes.
- Rejection of writes to the read-only conversion register.
- Deleted copy and move operations for an instance that contains callbacks and live job state.
- Staged single-shot and configuration jobs.
- Fixed and clamped poll instruction budgets.
- Active staged jobs block normal I2C methods.
- Wrap-safe elapsed-time arithmetic in nonblocking paths.
- No general-call reset API. This is good on a shared bus because general call would reset every responding ADS111x.
- Broad native test coverage and honest validation documentation.

The single-shot job is close to the right owner model. It should be repaired and made the main production path rather than surrounded by another adapter state machine.

## Hard findings and required refactors

### ADS-TM-01 - The product role and analog meanings are undefined

**Severity:** integration blocker
**Applies to:** all TunnelMonitor uses

The current firmware does not say whether ADS1115 replaces INA228 or measures new analog signals. It also does not define channel count, MUX, supply, PGA, rate, front-end transfer function, units, valid range, required/optional role, or calibration.

**Required before implementation**

Freeze:

1. Replacement or coexistence.
2. Exact board revision and product profile.
3. Required or optional device role.
4. Address strap.
5. Populated input channels and single-ended/differential MUX for each.
6. ADS supply voltage and analog protection.
7. Divider, shunt, amplifier, offset, filtering, and source impedance.
8. PGA and data rate per channel.
9. Product engineering unit and valid range per channel.
10. Calibration ownership, version, and persistence.
11. Maximum channel-to-channel acquisition skew.
12. Saturation, floating/open input, and disconnected-sensor behavior.

The simplest safe first product profile uses fixed settings compiled with the board profile. Do not expose arbitrary runtime MUX, gain, or calibration rows to operators.

### ADS-TM-02 - Cancel or timeout can cause a sample to be assigned to the wrong MUX

**Severity:** critical, data correctness
**Applies to:** single-shot acquisition

`cancelJob()` clears `_conversionStarted` even when the CONFIG/start write has already reached the device:

- `src/ADS1115.cpp:1032-1044`

`readBlocking()` does the same on timeout:

- `src/ADS1115.cpp:733-739`

A new single-shot is then accepted because software says no conversion is active:

- `src/ADS1115.cpp:756-784`

The TI datasheet states that writing `OS=1` during an ongoing single-shot conversion has no effect. Therefore the second start can be ignored. The first conversion can finish and be read under the second requested MUX.

Current cancellation tests cancel before the first job poll, before hardware is touched:

- `test/test_basic.cpp:2054-2081`

**Required refactor**

- Track whether the start write was attempted and whether it succeeded or failed ambiguously.
- A clean pre-I2C cancel may return directly to idle.
- A post-start cancel or timeout must enter `WaitIdleAfterAbandon` or equivalent.
- Reject new conversions while hardware may still be active.
- Poll OS or wait the proven worst-case conversion interval before returning the device to reusable state.
- Preserve a `Cancelled` or `TimedOut` terminal result for the abandoned request.
- Never reuse the abandoned conversion as a sample for another request.
- Add tests that cancel after every job stage and immediately request a different MUX.

The cancel call itself can remain bus-silent. Cleanup is a later owner-scheduled state-machine step.

### ADS-TM-03 - Configuration mutations are allowed during a direct conversion

**Severity:** high, data correctness
**Applies to:** direct single-shot API and runtime setters

Direct setters check `_jobActive` but do not reject `_conversionStarted`. `setMux()`, `setGain()`, `setDataRate()`, `setMode()`, `writeConfig()`, comparator setters, shutdown, and raw configuration writes can therefore run while a direct single-shot is physically active.

Examples:

- direct start: `src/ADS1115.cpp:430-495`
- MUX/gain/rate/mode setters: `src/ADS1115.cpp:1050-1129`
- raw CONFIG write: `src/ADS1115.cpp:1142-1177`
- `_writeConfigOnly()` resets software conversion state in single-shot mode: `src/ADS1115.cpp:1721-1736`

This produces the same hardware/software divergence as cancellation. A later start may be ignored and an old-MUX or old-gain result can be labelled with new settings.

**Required refactor**

- Enforce one active device operation across staged and direct APIs.
- Reject every measurement-affecting mutation while a conversion may be active.
- Do not clear conversion state merely because a configuration write was attempted.
- Prefer one staged `startApplyProfile()` over live piecemeal setters.
- Keep direct setters as explicit diagnostics only, if retained.

### ADS-TM-04 - Initialization, recovery, and shutdown do not fit `I2cTask`

**Severity:** high, owner liveness
**Applies to:** lifecycle and recovery

`begin()` performs one CONFIG-register probe and three writes. Strict readback adds three reads:

- `src/ADS1115.cpp:149-237`
- `src/ADS1115.cpp:1680-1771`
- documented transaction counts: `include/ADS1115/ADS1115.h:105-117`

`recover()` performs a CONFIG read and the same apply:

- `src/ADS1115.cpp:343-375`

At the library default 50 ms callback timeout, strict initialization can expose seven callback timeouts, or 350 ms, in one public call.

`begin()` also clears the existing working binding before validating the candidate:

- reset before validation: `src/ADS1115.cpp:149-175`
- validation: `src/ADS1115.cpp:177-199`

`end()` performs a hidden best-effort shutdown write and discards its status:

- `src/ADS1115.cpp:266-286`

**Required refactor**

- Validate and bind transport/profile with zero I2C.
- Add staged initialize and recover operations.
- Apply at most the caller-approved transaction budget per poll.
- Make unbind/end bus-silent.
- Keep shutdown as an explicit staged, status-returning operation.
- Preserve a working binding until a replacement profile is valid.
- Publish READY only after required readback completes.

The existing `startApplyConfigJob()` is useful but cannot initialize an uninitialized object and can only reapply the current cache. Extend this path rather than adding a parallel wrapper lifecycle.

### ADS-TM-05 - The staged single-shot job has no terminal deadline

**Severity:** high, unattended liveness
**Applies to:** poll-driven single-shot

`pollSingleShot()` can wait for ALERT/RDY or poll OS forever:

- `src/ADS1115.cpp:788-886`

There is no stored deadline, maximum ready-poll count, or terminal timeout. Caller cancellation exists but currently has the unsafe post-start behavior described above.

**Required refactor**

- Give every read an explicit wrap-safe whole-operation deadline.
- Derive it from selected data rate, the TI -10% rate tolerance, channel count, owner scheduling margin, and I2C transfer budgets.
- Throttle OS polls after the earliest possible completion instead of reading on every fast owner poll.
- Finish the request exactly once with success, timeout, cancel, or transport error.
- If timeout occurs after start, enter the safe abandon/wait-idle state before accepting a new request.

Do not copy the existing 1000 ms ENV/power deadline without calculation. Four channels at 8 SPS take about 500 ms nominal and about 556 ms at the slow tolerance before owner and I2C overhead.

### ADS-TM-06 - Dirty configuration does not block labelled or scaled samples

**Severity:** high, data correctness
**Applies to:** raw-to-labelled and voltage paths

The library correctly marks `_hardwareConfigDirty` after ambiguous or raw writes:

- job failure handling: `src/ADS1115.cpp:1452-1468`
- raw write handling: `src/ADS1115.cpp:1561-1584`
- dirty helpers: `src/ADS1115.cpp:1794-1805`

However, `readRaw()`, `readVoltage()`, and `rawToVoltage()` do not check the dirty state:

- `src/ADS1115.cpp:573-632`
- `src/ADS1115.cpp:1362-1377`

If hardware accepted a MUX/PGA write but the transport returned an ambiguous error, the cache can describe different settings from the ADC. The API still labels and scales the sample with the cache.

**Required refactor**

- Replace the Boolean-only policy with explicit configuration state: `Unconfigured`, `Applying`, `Verified`, or `Unknown`.
- Reject typed/scaled acquisition while measurement configuration is unknown.
- Permit an unlabelled conversion-register read only through an explicit diagnostic API and mark its quality `ConfigUnknown`.
- Clear unknown state only after complete desired-profile replay and readback.
- Increment configuration generation only after verification.

### ADS-TM-07 - The sample result lacks provenance and makes stale publication easy

**Severity:** high, data correctness
**Applies to:** staged reads and delayed conversion

A successful staged job writes only `_lastRawValue`:

- `src/ADS1115.cpp:866-879`

`PollResult` contains no sample value, MUX, gain, rate, timestamp, sequence, validity, or configuration generation:

- `include/ADS1115/ADS1115.h:40-46`

`lastRawValue()` returns a value before any successful read and after failure/cancellation. It is initialized to zero:

- `include/ADS1115/ADS1115.h:341-342`
- `src/ADS1115.cpp:159`

`rawToVoltage(raw)` uses the current cached gain, not the gain that produced the raw value:

- `src/ADS1115.cpp:1362-1377`

An old sample converted after a gain change is scaled incorrectly.

**Required refactor**

- Publish the sample and terminal operation status atomically.
- Include exact MUX, gain, data rate, configuration generation, sequence, and quality.
- Make a result available only after successful terminal completion.
- Provide `takeSample(expectedToken, out)` or include it in the terminal poll result.
- Invalidate or supersede the previous result when a new request starts.
- Use a pure conversion helper with explicit `raw` and `Gain` arguments.
- Let TunnelMonitor add its 64-bit completion timestamp when accepting the result.

### ADS-TM-08 - Continuous-mode fresh-sample claims are unsafe

**Severity:** high if continuous mode is used; excluded from first profile
**Applies to:** continuous conversion

Continuous readiness is inferred from a software interval:

- `src/ADS1115.cpp:519-555`

The TI datasheet specifies +/-10% data-rate variation. Current helpers return:

- 130 ms at 8 SPS, while the slow limit is about 138.9 ms
- 68 ms at 16 SPS, while the slow limit is about 69.4 ms

Evidence:

- conversion helper: `src/ADS1115.cpp:1396-1412`
- TI electrical-characteristics data-rate variation

The timer can therefore report a fresh sample before a new conversion exists.

There is a second problem after reconfiguration. TI states that a conversion already in progress finishes with the previous settings. New settings apply to later conversions. The library starts its freshness timer at the configuration write:

- `_writeConfigOnly()`: `src/ADS1115.cpp:1721-1736`

After one interval, the conversion register may still contain the final old-profile sample while the cache and voltage conversion use the new MUX/gain.

**Required refactor if continuous mode is retained**

- Use a true worst-case conversion period.
- Track configuration generation and conversion phase.
- After MUX/PGA/rate change, discard or explicitly qualify the old-profile completion.
- Require a confirmed new-profile conversion before publishing.
- Use captured ALERT/RDY edges if exact per-conversion freshness is required.

The simpler first TunnelMonitor solution is single-shot mode.

### ADS-TM-09 - Driver-owned OFFLINE policy conflicts with the bus owner

**Severity:** high for TunnelMonitor architecture
**Applies to:** health, backoff, and recovery

Every tracked physical transfer updates driver health:

- `src/ADS1115.cpp:1519-1542`
- `src/ADS1115.cpp:1632-1666`

After the internal threshold is reached, normal access is rejected without touching the bus:

- `src/ADS1115.cpp:1482-1488`
- `src/ADS1115.cpp:1521-1534`

`recover()` temporarily bypasses and may re-latch the driver state:

- `src/ADS1115.cpp:343-374`

TunnelMonitor already owns device health, retry, backoff, bus recovery, queue expiry, and recovery authorization. Two independent policies can disagree and prevent owner-authorized recovery.

**Required refactor**

- Make library counters passive diagnostics only.
- Remove internal OFFLINE admission from the production core, or provide an owner-controlled mode that never blocks transport from internal counts.
- Return precise operation errors and configuration state.
- Let `I2cTask` decide when to retry, recover, or mark the device offline.

Do not work around this by setting an artificially high offline threshold.

### ADS-TM-10 - Initialization verification is optional and identity cannot be proven

**Severity:** medium-high for unattended startup
**Applies to:** probe, initialization, and recovery

ADS1115 has no device identity register. `_probeRaw()` only reads a CONFIG-like register:

- `src/ADS1115.cpp:327-340`

The library correctly documents this as plausibility rather than identity. However, `strictInitVerify` defaults to false:

- `include/ADS1115/Config.h:137-140`

Default begin therefore proves only a readable register and acknowledged writes. It does not verify the applied threshold/config profile.

**Required platform policy/refactor**

- Make verified apply the production initialization/recovery path.
- Read back low threshold, high threshold, and masked writable CONFIG fields.
- Keep identity wording honest: `RegisterProfileVerified`, not `IdentityVerified`.
- Freeze the address and expected device inventory in the board profile.
- Run absent/wrong-address/final-board HIL.

Do not add I2C general-call reset as an identity or recovery shortcut. It is a broadcast operation on the shared bus.

### ADS-TM-11 - ALERT/RDY and comparator ownership are not platform-safe

**Severity:** conditional; do not include in first profile
**Applies to:** comparator events and conversion-ready GPIO

`Config` stores a platform GPIO number and a bool-only read callback:

- `include/ADS1115/Config.h:34-47`
- `include/ADS1115/Config.h:155-158`

The callback cannot report pin-read failure. Every Boolean is treated as a real electrical level:

- `src/ADS1115.cpp:68-85`

The staged single-shot ALERT path checks the pin immediately after the start step without first enforcing minimum conversion time:

- `src/ADS1115.cpp:831-839`

A stale or stuck asserted level can cause the previous conversion register to be read as the new result.

In continuous mode, TI documents an ALERT/RDY ready pulse of about 8 microseconds. Ordinary task-level GPIO polling can miss it.

Comparator configuration is also piecemeal:

- thresholds use two synchronous writes: `src/ADS1115.cpp:1184-1205`
- gain is changed independently: `src/ADS1115.cpp:1069-1086`
- conversion-ready setup performs a blocking full apply: `src/ADS1115.cpp:1310-1338`
- disabling the comparator changes only the queue and retains old thresholds: `src/ADS1115.cpp:1341-1355`

Thresholds are raw conversion codes. Changing gain changes their physical voltage meaning. A later queue enable can reactivate retained conversion-ready thresholds.

**Required only if a product needs ALERT/comparator**

- Board/application owns the GPIO and pull-up.
- ISR records a bounded edge and timestamp only.
- `I2cTask` performs all I2C.
- Enforce minimum conversion time before accepting a ready level.
- Use one typed `ComparatorProfile` coupled to gain.
- Separate normal threshold mode from conversion-ready mode.
- Validate `high > low` for normal comparator use; allow only the explicit TI ready pattern as the special reversed case.
- Define conversion-register read and latched-alert clear behavior.
- Apply and verify the whole comparator profile as one staged job.

For the first platform profile, disable comparator output and poll OS.

### ADS-TM-12 - Analog front-end and calibration requirements are outside the current contract

**Severity:** integration blocker for meaningful engineering values
**Applies to:** all analog measurements

Correct I2C and register code do not prove a correct analog measurement.

TI requirements relevant to the product include:

- recommended analog input range is GND to VDD
- absolute input limit is GND - 0.3 V to VDD + 0.3 V
- PGA full-scale setting is ADC scaling, not permission to exceed input limits
- a `+/-6.144 V` or `+/-4.096 V` code range cannot make a 3.3 V-powered input accept more than its supply limit
- the input is a switched-capacitor load; source impedance can affect accuracy
- differential readings may be negative, but neither physical input may go below ground
- data rate varies by +/-10%
- offset, gain error, channel match, supply rejection, and temperature drift are finite
- about 50 microseconds is required after VDD stabilizes before communication

The library cannot infer divider ratio, shunt value, amplifier gain, source impedance, excitation, protection, or physical sensor transfer function.

**Required project work**

- Put the fixed analog topology in the hardware profile.
- Define expected voltage and engineering range per channel.
- Use checked `int64_t` rational scaling.
- Define unit calibration separately from factory/library nominal conversion.
- Store calibration with version, CRC, channel identity, units, bounds, and provenance.
- Define floating/open input behavior in hardware; an unconnected ADC input is not a reliable disconnect detector.
- Define saturation based on supply and expected channel range, not only raw `INT16_MIN/MAX`.
- Verify representative points using calibrated equipment on the final board.

### ADS-TM-13 - The configuration API is too broad for production ownership

**Severity:** medium
**Applies to:** public surface and adapter design

MUX, gain, rate, mode, threshold, comparator, conversion-ready, and raw register methods are all public. Most direct setters perform immediate synchronous I2C. `startApplyConfigJob()` can only reapply the current cache, not stage a new candidate profile.

Evidence:

- direct configuration methods: `src/ADS1115.cpp:1050-1355`
- staged apply start: `src/ADS1115.cpp:888-912`
- raw access: `src/ADS1115.cpp:1548-1584`

There are also compatibility methods that hide errors:

- `tick()` discards `service()` status: `src/ADS1115.cpp:240-263`
- bool-only `conversionReady()` discards read status: `src/ADS1115.cpp:497-505`

`readLatestRaw()` intentionally reads the current register even if no fresh sample exists. This is useful diagnostics but unsafe as routine measurement.

**Required refactor/integration rule**

- Production API: zero-I2C bind, staged apply/recover/read, poll, safe cancel, atomic take-result, cache-only status.
- Pure API: explicit conversion and timing helpers.
- Diagnostic API: latest/raw register access, clearly separate and blocked during active operations.
- Deprecate error-hiding methods from production use.
- Do not expose raw writes, per-read gain/rate, or comparator setters through TunnelMonitor Web, cloud, or ordinary CLI.
- Keep product settings in one validated compile-time `DeviceProfile` and fixed `ChannelProfile[]`.

### ADS-TM-14 - TunnelMonitor contracts and capacities need selected-profile changes

**Severity:** integration blocker after library refactor
**Applies to:** distinct ADC source; smaller impact for INA228 replacement

The current I2C command boundary has no ADC operation:

- `I2cOperation` ends at `Scan = 9`: `include/TunnelMonitor/contracts/FieldBus.h:13-25`
- I2C command validation has no ADC case: `src/i2c/I2cTask.cpp:14-24`, `103-203`
- optional active jobs cover ENV, INA228, and display only: `include/TunnelMonitor/i2c/I2cTask.h:82-149`

Adding a coexisting ADC exceeds the current production health and measurement-source counts. Public status has no ADC section. The settings registry has only two row slots free, which is not enough for runtime per-channel configuration.

**Required for a distinct source**

- Append, do not reorder, `DeviceId::Adc` and `I2cOperation::ReadAdc`.
- Add fixed `AdcReadCommand`, `AdcReadResult`, and owner-private active-job state.
- Add the known-device and health row only to selected profiles that contain ADS1115.
- Raise or derive selected-profile health and source capacities with compile-time checks.
- Add explicit ADC result storage and abandonment handling in measurement runtime.
- Add a compact `AdcStatus` only if operators need it.
- Recheck public JSON 3072-byte and compact JSON 1024-byte capacities.
- Update sample profile ID, field descriptors, CSV, replay, cloud, masks, and compatibility tests together.
- Keep fixed channel configuration out of the nearly full general settings table.

**Required for INA228 replacement**

- Keep one power-monitor identity and health row.
- Prove that both implementations produce identical project units, fields, flags, validity, and required/optional semantics.
- Do not surface chip-specific raw ADC settings in the generic power contract.

### ADS-TM-15 - Current validation is a strong baseline but not release evidence for TunnelMonitor

**Severity:** medium release gate
**Applies to:** release and platform acceptance

The exact `v1.2.0` native suite and builds pass. Existing hardware evidence does not cover the final product.

The retained hardware summary says:

- COM19 limited run used clean older v1.0.0 firmware.
- COM8 targeted, 8-hour, and about 20-hour evidence used dirty v1.1-era firmware.
- Long raw COM8 transcripts were removed; only summary metrics and hashes remain.
- No clean v1.2 hardware run exists.
- Analog accuracy, comparator electrical behavior, physical fault recovery, and pure ESP-IDF HIL remain incomplete.

The library CI/build inputs are also broad:

- PlatformIO uses unpinned `platform = espressif32`.
- CI installs unpinned PlatformIO.
- component metadata accepts IDF `>=5.0`.

**Required release work**

- Exact-pin the accepted library version/commit in TunnelMonitor.
- Pin the tested PlatformIO platform/toolchain for reproducible release CI.
- Run clean v1.2 library HIL if the library makes a production claim.
- Run final TunnelMonitor board, analog, recovery, and workload HIL before product acceptance.
- Retain condensed raw evidence that allows failures to be reviewed.

## Recommended library refactor

The required design is small and ADS1115-specific. It does not need a generic ADC framework.

### Lifecycle and state machine

Recommended states:

```text
Unbound -> Bound -> Applying -> Verifying -> Ready
                         |           |
                         +-------> ConfigUnknown

Ready -> StartWrite -> Converting -> ReadyCheck -> ReadResult -> Ready
              |
              +-- cancel/timeout --> WaitIdleAfterAbandon -> Ready
```

Recommended production operations:

```cpp
Status bind(const DriverConfig& transport,
            const DeviceProfile& profile);       // zero I2C
Status startInitialize();
Status startApplyProfile(const DeviceProfile& profile);
Status startRecover();
Status startRead(const ChannelRequest& request,
                 uint32_t deadlineMs);
PollResult poll(uint32_t nowMs, uint8_t maxTransactions = 1);
CancelDisposition cancelActiveOperation();       // zero I2C
Status takeSample(OperationToken token, SampleResult& out);
Status startShutdown();
void unbind();                                    // zero I2C
```

Rules:

- One active hardware operation.
- No more than the caller-approved transport callback budget per poll.
- No blocking loops or sleeps.
- One owner time domain.
- Whole-operation deadline for every conversion and multi-register apply.
- No new start while a prior conversion may still be active.
- Configuration becomes verified only after complete readback.
- Terminal result is published once and tied to an operation token.
- Internal health never blocks owner-authorized I2C.

### Device and channel profiles

```cpp
enum class Address : uint8_t {
  AddrToGnd = 0x48,
  AddrToVdd = 0x49,
  AddrToSda = 0x4A,
  AddrToScl = 0x4B,
};

struct DeviceProfile {
  Address address{Address::AddrToGnd};
  DataRate dataRate{DataRate::SPS_128};
  Mode mode{Mode::SINGLE_SHOT};
  bool comparatorDisabled{true};
  bool verifyReadback{true};
};

struct ChannelRequest {
  uint16_t channelId{0};
  Mux mux{Mux::AIN0_GND};
  Gain gain{Gain::FSR_2_048V};
};
```

For the first TunnelMonitor profile:

- mode is always single-shot
- comparator is always disabled
- address, rate, channel MUX, and gain are compile-time facts
- runtime commands select only a fixed profile/read operation, not arbitrary registers

### Atomic sample type

```cpp
enum class SampleFlag : uint16_t {
  None = 0,
  ConfigVerified = 1U << 0,
  AtPositiveCodeLimit = 1U << 1,
  AtNegativeCodeLimit = 1U << 2,
  DirectUnverified = 1U << 3,
};

struct SampleResult {
  int16_t rawCode{0};
  int32_t microvolts{0};
  uint16_t channelId{0};
  Mux mux{Mux::AIN0_GND};
  Gain gain{Gain::FSR_2_048V};
  DataRate dataRate{DataRate::SPS_128};
  uint16_t flags{0};
  uint32_t configGeneration{0};
  uint32_t sequence{0};
};
```

TunnelMonitor applies 64-bit start/completion timestamps when it accepts the terminal result. Board divider/shunt/amplifier and physical engineering conversion remain in the product channel profile, not the chip driver.

## Recommended TunnelMonitor result contract

For a distinct fixed four-channel ADC source, a result can fit inside the existing 128-byte I2C payload when layout is controlled and statically checked:

```cpp
enum class AdcInputSelection : uint8_t {
  Ain0Ain1,
  Ain0Ain3,
  Ain1Ain3,
  Ain2Ain3,
  Ain0Ground,
  Ain1Ground,
  Ain2Ground,
  Ain3Ground,
};

enum class AdcFullScale : uint8_t {
  MilliVolts6144,
  MilliVolts4096,
  MilliVolts2048,
  MilliVolts1024,
  MilliVolts512,
  MilliVolts256,
};

enum class AdcSampleFlag : uint16_t {
  Valid = 0x0001,
  ConfigVerified = 0x0002,
  AtPositiveCodeLimit = 0x0004,
  AtNegativeCodeLimit = 0x0008,
  OutsideExpectedRange = 0x0010,
};

struct AdcChannelSample {
  AdcInputSelection input{};
  AdcFullScale fullScale{};
  uint16_t flags{0};
  int16_t rawCode{0};
  int32_t inputMicrovolts{0};
  uint64_t completedUptimeMs{0};
};

struct AdcReadResult {
  uint8_t address{0};
  uint8_t sampleCount{0};
  uint16_t profileId{0};
  uint64_t firstConversionUptimeMs{0};
  uint64_t completedUptimeMs{0};
  AdcChannelSample samples[4]{};
};
```

Required static checks:

- result is trivially copyable
- exact result size is at most 128 bytes
- maximum channel count matches the selected profile
- reserved bytes are zero and rejected on input

Durable sample fields should contain the product engineering value. Raw ADC code and microvolts belong in diagnostics unless the product explicitly requires them in storage.

## Required helpers, enums, and types

### Required library helpers

- `dataRateSps(DataRate)`
- `worstCaseConversionTimeUs(DataRate)` using the TI -10% rate tolerance and a documented guard
- `gainFullScaleMicrovolts(Gain)`
- `rawToMicrovolts(int16_t raw, Gain gain, int32_t& out)`
- `isSingleEnded(Mux)`
- `positiveInput(Mux)`
- `negativeInput(Mux)`
- `validateDeviceProfile()`
- `validateChannelRequest()`
- `validateComparatorProfile()` if comparator support is retained
- `operationDeadlineMs(channelCount, rate, schedulingMargin)`

`rawToMicrovolts()` should use checked `int64_t` rational arithmetic and explicit rounding. The 0.256 V range has a 7.8125 microvolt LSB, so a simple integer microvolts-per-LSB constant is not exact.

### Required state and result types

- `OperationToken`
- `OperationKind`
- `OperationState`
- `ConfigurationState`
- `CancelDisposition`
- `DeviceProfile`
- `ChannelRequest`
- `SampleResult`
- `SampleFlag`
- `AppliedProfileSnapshot`

### Conditional comparator types

Add only if a selected product needs them:

- `ComparatorUse { Disabled, Threshold, ConversionReady }`
- `ComparatorMode`
- `ComparatorPolarity`
- `ComparatorLatch`
- `ComparatorQueueDepth`
- `ComparatorProfile`
- `AlertEvent`
- `AlertAcknowledgeResult`

### Nice-to-have helpers

These are useful after the hard requirements and only with a real caller:

- fixed enum-to-name helpers for diagnostics
- `inputPairName(Mux)`
- profile-difference report showing the first mismatched register
- bounded driver transition trace supplied by the owner
- checked engineering-unit rational scaling helper in TunnelMonitor
- compact four-channel acquisition-duration estimate
- diagnostic formatter showing raw code, microvolts, MUX, PGA, rate, and profile generation

Do not add generic ADC plugins, runtime channel registries, dynamic scan lists, sensor fusion, or arbitrary formula interpreters.

## API cleanup

Separate the public surface into:

1. **Production owner-safe API** - bind, staged apply/recover/read/shutdown, poll, safe cancel, take result, cache-only status.
2. **Pure codec API** - validation, timing, raw-to-microvolt conversion.
3. **Diagnostic API** - latest register and raw register access, explicitly not ordinary measurement.

Recommended cleanup:

- Keep copy/move deleted.
- Make `end()` bus-silent.
- Deprecate void `tick()` and bool-only `conversionReady()` from production use.
- Do not use `readBlocking*()` in `I2cTask`.
- Remove implicit dependence of conversion helpers on mutable cached gain.
- Add a candidate-profile staged apply rather than immediate setter groups.
- Preserve `readLatestRaw()` only as a clearly stale-capable diagnostic.
- Keep raw writes out of normal TunnelMonitor CLI, Web, display, and cloud paths.
- Use stable typed error codes rather than parsing static messages.

## What not to do

Reject these band-aids:

- Clearing the software conversion flag on cancel without reconciling hardware.
- Retrying an ambiguous start write immediately.
- Sleeping for a nominal conversion delay inside `I2cTask`.
- Calling current blocking `begin()` or `recover()` from one owner callback.
- Reading `lastRawValue()` after a failed/cancelled job and assuming it is new.
- Scaling an old raw code with the driver's current gain.
- Continuing labelled samples while `hardwareConfigDirty()` is true.
- Using continuous mode to avoid fixing single-shot state handling.
- Polling an 8 microsecond ALERT/RDY pulse from a normal task.
- Giving the library and `I2cTask` separate offline/recovery authority.
- Exposing raw MUX/PGA/register settings as normal remote configuration.
- Logging both raw and converted fields for every channel without a product need.
- Treating a CONFIG read as proof of ADS1115 identity.
- Using general-call reset on the shared bus as routine recovery.
- Creating a generic ADC service framework for one fixed device profile.

## Validation performed during this audit

Checks were run against exact `v1.2.0`:

| Check | Result |
|---|---|
| Version metadata check | PASS |
| Core timing guard | PASS |
| CLI contract check | PASS |
| ESP-IDF example contract check | PASS |
| HIL parser and dry-run checks | PASS |
| Native Unity suite | PASS, 151/151 |
| Arduino ESP32-S3 build | PASS, flash 405,370 B, RAM 22,352 B |
| Arduino ESP32-S2 build | PASS, flash 397,593 B, RAM 36,800 B |
| PlatformIO package creation | PASS; temporary archive removed |
| Source diff/whitespace check | PASS |
| Local ESP-IDF compile | NOT RUN; `idf.py` was unavailable |
| CI ESP-IDF coverage | Configured for IDF 5.3 on ESP32-S2 and ESP32-S3 |
| New physical HIL | NOT RUN |
| TunnelMonitor integration build | NOT RUN; integration does not exist |

The Arduino builds compile the ADS1115 bring-up CLI, not TunnelMonitor's board profile or shared-bus owner.

## Native tests to add

Keep the 151-case baseline and add:

- cancel before start write
- cancel after successful start write
- cancel after ambiguous start-write failure
- blocking timeout while hardware remains active
- immediate different-MUX request after cancel/timeout
- OS start ignored while a prior conversion is active
- each direct setter while a single-shot conversion is active
- single-shot OS stuck busy until deadline
- OS poll throttling and zero-I2C wait polls
- 8 SPS and 16 SPS at TI -10% rate tolerance
- continuous reconfiguration where one old-profile conversion completes
- dirty MUX/PGA blocking labelled/scaled sample
- stale previous result after failure and cancellation
- operation token and sequence matching
- configuration generation changes only after verified apply
- ALERT/RDY stale asserted immediately after start
- fallible external ready-event handling if later supported
- normal comparator `high > low` validation
- explicit conversion-ready threshold exception
- staged initialization/recovery with one-transfer budgets
- owner-controlled health with no internal OFFLINE gate
- wrap-boundary deadlines
- every gain endpoint, zero, rounding direction, and overflow-safe raw-to-microvolt conversion

## Existing HIL evidence and limits

The repository contains useful but limited hardware evidence.

### COM19 limited run

- clean older firmware: v1.0.0, commit `9551bee`
- ESP32-S2 class board
- ADS-range devices at `0x48` and `0x49`
- address selection, absent-address negative cases, restore behavior, self-test, and short stress passed
- tracked raw transcript exists for this limited run

Missing fixture facts include exact module/revision, VDD, I2C rate, pull-ups, instruments, and wiring photos.

### COM8 longer runs

- targeted, broad, 8-hour, and about 20-hour runs reported zero classified command failures in the retained summaries
- firmware was dirty v1.1-era code, not clean v1.2.0
- the about 20-hour run processed roughly 1.79 million commands
- long raw transcripts were removed after hashes and summaries were recorded
- evidence rows remained marked `EVIDENCE_REQUIRED`

This is strong evidence for digital API/CLI stability on those fixtures. It does not prove final analog correctness or product integration.

### HIL gaps

- no clean v1.2.0 target run
- no calibrated analog sweep for every selected channel/range
- no gain/offset/temperature acceptance against product limits
- no ALERT/RDY scope capture or comparator electrical matrix
- no final-board address, supply, source impedance, protection, and filtering proof
- no physical unplug/replug, held SDA/SCL, brownout, or reset matrix
- no pure ESP-IDF hardware run
- no exact TunnelMonitor hardware 2.0.0 or approved new revision integration
- no all-device shared-bus fairness/deadline test
- no storage/cloud/schema workload test

## Release gates for the recommended first scope

### Product and board gates

- [ ] Decide INA228 replacement or distinct ADC source.
- [ ] Freeze product profile and board revision.
- [ ] Freeze required/optional role.
- [ ] Freeze address and ADDR strap.
- [ ] Freeze populated channel list and MUX.
- [ ] Freeze supply, input limits, protection, source impedance, and filtering.
- [ ] Freeze PGA and rate per channel.
- [ ] Freeze engineering units, valid ranges, and skew limit.
- [ ] Freeze calibration and persistence contract.
- [ ] Keep ALERT/RDY unused for the first profile.

### Library gates

- [ ] Safe post-start cancellation and timeout drain state.
- [ ] No configuration mutation while conversion may be active.
- [ ] Zero-I2C bind and unbind.
- [ ] Staged initialize, apply, recover, read, and shutdown.
- [ ] One transaction per normal owner poll.
- [ ] Whole-operation deadline and poll throttling.
- [ ] Atomic tokened sample result with MUX/PGA/rate provenance.
- [ ] Verified/unknown configuration state and generation.
- [ ] Dirty state blocks labelled/scaled samples.
- [ ] Explicit raw-plus-gain fixed-unit conversion helper.
- [ ] Passive health with no internal OFFLINE admission.
- [ ] Verified profile apply as the production default.
- [ ] Continuous mode excluded or fully repaired.
- [ ] Comparator/ALERT excluded or fully typed and tested.

### TunnelMonitor native gates

- [ ] Append-only enum changes where applicable.
- [ ] Exact device/operation/command identity validation.
- [ ] Result is trivially copyable and at most 128 bytes.
- [ ] Reserved bytes validated.
- [ ] One library transport callback per normal poll.
- [ ] Owner deadline, cancellation, queue expiry, and exactly one terminal result.
- [ ] No blind retry after ambiguous start write.
- [ ] Per-channel validity and partial-acquisition behavior.
- [ ] Raw sign, MUX, range, microvolt conversion, and product rational scaling.
- [ ] Required/optional quality and health behavior.
- [ ] Selected-profile device/source/health/sample/settings capacities.
- [ ] Public/compact JSON capacity if ADC status is added.
- [ ] CSV, replay, cloud, schema/profile, and maximum-size tests.
- [ ] No steady-state allocation and bounded stack/result storage.
- [ ] INA228 replacement equivalence tests if replacement is selected.

### Final-board HIL gates

- [ ] ADS address and all other I2C devices visible together at 400 kHz.
- [ ] Known reference near zero and representative points across every selected signal range.
- [ ] Negative differential input if any differential MUX is selected.
- [ ] Near-range-limit behavior and supply-limited clipping.
- [ ] Analog-front-end scaling and calibration tolerance.
- [ ] Channel order and measured first-to-last skew.
- [ ] Open/disconnected input behavior defined by the final circuit.
- [ ] Optional absence or required-source failure behavior.
- [ ] Unplug/replug and owner recovery.
- [ ] Device reset/brownout followed by verified profile replay.
- [ ] Held-bus recovery with the full I2C population.
- [ ] Measurement, storage, display, web, and cloud workload without starvation.
- [ ] Several-hour or overnight soak at final cadence with reviewable evidence.

## Suggested implementation order

1. Freeze replacement versus distinct-source product scope and analog hardware facts.
2. Fix single-shot cancellation/timeout and hardware-idle reconciliation.
3. Enforce one active operation across conversion and configuration APIs.
4. Add zero-I2C bind, staged lifecycle, deadline, and atomic result.
5. Gate typed/scaled samples on verified configuration.
6. Add explicit fixed-unit conversion and timing helpers.
7. Remove driver-owned offline admission from the production path.
8. Add native fault/timing tests and rerun all library builds.
9. Add the smallest selected-profile TunnelMonitor adapter and contracts.
10. Run integration native tests and final-board analog/shared-bus HIL.
11. Add continuous mode or comparator support only if a product requirement needs them.

## Final recommendation

Use ADS1115 `v1.2.0` as the protocol, test, and diagnostic baseline, not as the production integration version.

The library is closer to TunnelMonitor's owner model than a typical blocking ADC wrapper because it already has fixed memory, staged single-shot/config jobs, poll budgets, dirty diagnostics, and transport injection. A focused refactor can make it platform-ready without rewriting the register protocol.

The first integration should use fixed compile-time channels, single-shot mode, OS-bit polling, strict profile readback, one transaction per owner poll, and comparator/ALERT disabled. Results must contain the raw code plus exact MUX/PGA/rate/configuration generation and fixed microvolts, then TunnelMonitor converts to the product engineering unit with checked calibration.

Do not integrate until the wrong-MUX cancellation path is fixed and the product decides whether ADS1115 replaces INA228 or represents a new analog measurement source.
