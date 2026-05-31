# ADS1115 Industry-Standard Exploration

Date: 2026-05-31
Repository: `C:/Users/HonzovoSpectre/Documents/Projects/ADS1115`
Branch: `exploration/ads1115-industry-standard`
Audit mode: deep exploration / no implementation

## Executive Summary

The ADS1115 library is much stronger than an early prototype. The core is framework-neutral, transport-injected, broadly feature-complete for ADS1115, has a coherent health/offline model, and has useful native fake-transport coverage. Arduino and ESP-IDF integration paths exist, and the documentation is unusually explicit about bus ownership, timing, thread/ISR safety, and ADS1115 strict-init limitations.

It is not defensible to call the library industry-grade or production-ready yet. The main blockers are diagnostic correctness around partial hardware mutation during `begin()`, ambiguous status codes, incomplete HIL/fault evidence, and a few public API/status-contract wrinkles. The current best classification is:

## Current Readiness Classification

Near industry-grade pending P0 fixes and validation

This means the architecture is close enough to harden, not redesign. It still needs P0 fixes and evidence before any field-grade claim:

- Preserve visibility of partial hardware mutation when `begin()` fails after one or more writes.
- Tighten status taxonomy for offline, unsupported operation, and strict read-back mismatch.
- Resolve bool-only or void public paths that can hide transport failure (`conversionReady()`, `tick()` diagnostics).
- Add missing native fault cases for begin-path partial writes, strict read-back branches, raw scaling, and invalid config.
- Produce dated HIL logs/captures for addresses, mux/gain/rate, comparator, ALERT/RDY, fault/recover, S2/S3 Arduino, and S2/S3 ESP-IDF.

## Scope Reviewed

Inspected:

- Repository metadata: `library.json`, `platformio.ini`, `CMakeLists.txt`, `idf_component.yml`, `Doxyfile`, `.github/workflows/ci.yml`.
- Public headers: `include/ADS1115/ADS1115.h`, `Config.h`, `Status.h`, `CommandTable.h`, `Version.h`.
- Core source: `src/ADS1115.cpp`.
- Tests: `test/test_basic.cpp`, `test/stubs/Arduino.h`, `test/stubs/Wire.h`.
- Examples: `examples/01_basic_bringup_cli/main.cpp`, `examples/common/*`, `examples/esp_idf/basic/*`.
- Docs: `README.md`, `CHANGELOG.md`, `docs/IDF_PORT.md`, `docs/HARDENING_FINAL_REPORT.md`, `docs/ADS1115_SELFTEST_POLISH_REPORT.md`, extracted datasheet notes, TI reference snapshot.
- Tools: `tools/check_core_timing_guard.py`, `tools/check_cli_contract.py`, `tools/check_idf_example_contract.py`, `scripts/generate_version.py`.

Subagents used:

- Repository discovery
- ADS1115 datasheet/register contract
- Core architecture
- Timing/determinism
- Status/error/health
- Partial-state/cache consistency
- Device-feature/API completeness
- Tests/fault injection
- ESP-IDF/Arduino integration
- Documentation/release
- Hardware validation
- Final integration review

Repository orientation commands:

- `pwd` - `C:\Users\HonzovoSpectre\Documents\Projects\ADS1115`
- `git rev-parse --show-toplevel` - `C:/Users/HonzovoSpectre/Documents/Projects/ADS1115`
- `git branch --show-current` - started on `hardening/ads1115-industry-readiness`
- `git status --short` - clean before edits
- `git remote -v` - `origin https://github.com/janhavelka/ADS1115.git`
- `bash -lc "find . -maxdepth 3 -type f | sort | head -300"` - failed because WSL has no installed distributions
- PowerShell equivalent file inventory was run successfully with `Get-ChildItem -File -Recurse -Depth 2`
- `git checkout -b exploration/ads1115-industry-standard` - created and switched to the exploration branch

## Datasheet Sources

- `docs/ADS111x_datasheet_revE.pdf`
- `docs/pdf-extracted-md/ADS111x_datasheet_revE.md`
- `docs/extracted-md/00_document_inventory.md`
- `docs/extracted-md/01_chip_overview.md`
- `docs/extracted-md/02_pinout_and_signals.md`
- `docs/extracted-md/03_electrical_and_timing.md`
- `docs/extracted-md/04_protocol_commands_and_transactions.md`
- `docs/extracted-md/05_register_map.md`
- `docs/extracted-md/06_modes_interrupts_status_and_faults.md`
- `docs/extracted-md/07_initialization_reset_and_operational_notes.md`
- `docs/extracted-md/08_variant_differences_and_open_questions.md`

Key source anchors from the extracted PDF:

- ADS1113/ADS1114/ADS1115 family, 16-bit, 860 SPS, PGA/comparator differences: `docs/pdf-extracted-md/ADS111x_datasheet_revE.md:47`, `:123`.
- ADS1115 MUX and analog input limits: `:864`, `:889`.
- PGA ranges and alias codes: `:973`, `:1505`.
- Data rates and single-cycle settling: `:1012`.
- Single-shot/continuous mode and OS bit: `:1117`, `:1126`, `:1462`.
- I2C addresses and ADDR behavior: `:1203`.
- Pointer/register map: `:1392`.
- Conversion register two's-complement format: `:1413`.
- Comparator and conversion-ready ALERT/RDY: `:1023`, `:1065`, `:1576`.
- ALERT/RDY open-drain/pull-up: `:168`, `:1071`, `:1088`.
- I2C target-only/no clock stretching and pull-up caveats: `:1160`, `:1648`, `:1652`.
- General-call reset: `:1215`.

## ADS1115 Datasheet Contract Checklist

| Behavior | Status | Evidence |
| --- | --- | --- |
| 16-bit conversion output, up to 860 SPS | PASS | Datasheet extract `:123`; `Config::DataRate` in `include/ADS1115/Config.h:76`. |
| ADS1115 four single-ended inputs | PASS | `Config.h:55`; `CommandTable.h:74`; datasheet extract `:864`. |
| ADS1115 differential selections | PASS | API supports all four MUX codes in `Config.h:51`; datasheet register map shows four selections. Note: some docs say "two differential measurements", which is less precise than the full MUX code list. |
| PGA ranges | PASS | `Config.h:66`; `CommandTable.h:80`; datasheet extract `:973`. |
| PGA FSR does not relax input absolute limits | PASS | README warns at `README.md:289`; datasheet extract `:188`, `:889`. |
| Data rates 8..860 SPS and single-cycle settling | PASS | `Config.h:76`; `ADS1115.cpp:917`; datasheet extract `:1012`. |
| Single-shot mode | PASS | `Mode::SINGLE_SHOT` in `Config.h:90`; start path in `ADS1115.cpp:361`; OS polling in `ADS1115.cpp:460`. |
| Continuous mode latest conversion behavior | PASS | `ADS1115.cpp:433`, `:510`; README latest-vs-fresh note at `README.md:171`. |
| I2C addresses 0x48..0x4B | PASS | Runtime validation at `ADS1115.cpp:12`, `:164`; docs at `README.md:238`. |
| Pointer register and four 16-bit registers | PASS | `CommandTable.h:14`; raw access validation at `ADS1115.cpp:993`; datasheet extract `:1392`. |
| No device-ID/chip-ID register | PASS | README strict-init note at `README.md:207`; datasheet register map has only four registers. |
| OS bit is dynamic and must be masked in read-back | PASS | `kConfigReadbackMask` excludes OS at `ADS1115.cpp:14`; strict verify in `ADS1115.cpp:1165`. |
| Comparator traditional/window, polarity, latch, queue | PASS | Enums in `Config.h:93`; setters in `ADS1115.cpp:773`; datasheet extract `:1023`. |
| Threshold registers are signed raw codes | PASS | README `README.md:225`; `setThresholds()` at `ADS1115.cpp:730`; datasheet extract `:1571`. |
| ALERT/RDY open-drain pull-up and ready mode | PASS | README `README.md:228`; `enableConversionReadyPin()` at `ADS1115.cpp:837`; datasheet extract `:1065`. |
| General-call reset is bus policy, not core behavior | PASS | No core general-call use found; datasheet extract `:1215`. |
| I2C electrical timing and pull-up caveats | PARTIAL | Datasheet notes exist in extracted docs; README does not summarize ADDR strap timing or SDA/SCL pull-up sizing beyond ALERT/RDY. |

## Architecture Scorecard

| Area | Rating | Evidence |
| --- | --- | --- |
| Framework-neutral core | Strong | Core includes only project headers and `<climits>` at `src/ADS1115.cpp:4`; guard passed. |
| Injected I2C transport | Strong | Callback types and opaque contexts in `Config.h:18`, `:30`, `:122`; callbacks used at `ADS1115.cpp:948`, `:960`. |
| Public API clarity | Good | Feature coverage is broad; `conversionReady()` bool-only and overloaded `BUSY` weaken contracts. |
| Error/status precision | Medium | Distinct I2C status codes exist in `Status.h:10`, but no `OFFLINE`, `UNSUPPORTED_OPERATION`, `READBACK_MISMATCH`, or explicit partial-state status. |
| Timing/determinism | Good | No core framework delay calls; bounded transport calls; `readBlocking()` loop has deadline but can multiply I2C polls after ready time. |
| Partial-state handling | Medium | Dirty tracking works for recover/apply paths, but failed `begin()` and raw writes can hide/desync hardware state. |
| Health/recovery | Good | Tracked wrappers at `ADS1115.cpp:964`; update health at `:1061`; offline latch and manual recover are coherent. |
| Tests/fault injection | Good | 57 native tests pass; fake bus has per-call failure injection; missing edge coverage remains. |
| ESP-IDF readiness | Good | Component metadata and native IDF example exist; local `idf.py` unavailable; example error mapping is not fully precise. |
| Arduino readiness | Good | S2/S3 PlatformIO builds pass; example is honestly diagnostic. |
| Documentation honesty | Medium | README is mostly honest; `CHANGELOG.md:41` overclaims "production-ready" without HIL logs. |
| Hardware validation | Weak | No checked-in HIL logs/captures found; required matrix exists but not results. |

## What Is Already Strong

- Core has no Arduino, Wire, ESP-IDF, FreeRTOS, logging, pin setup, bus setup, task creation, or framework delay dependency in `include/` or `src/`.
- I2C is non-owning and injected through callbacks with opaque user context and per-transaction timeout.
- Public feature coverage is broad: all ADS1115 addresses, MUX codes, gains, data rates, modes, comparator fields, ALERT/RDY ready mode, raw register access, strict read-back, and shutdown are represented.
- Copy and move operations are explicitly deleted in `include/ADS1115/ADS1115.h:69`.
- Health tracking has clear READY/DEGRADED/OFFLINE behavior; normal public I2C short-circuits while OFFLINE until manual `recover()`.
- `probe()` uses raw I2C and avoids health side effects; address NACK maps to `DEVICE_NOT_FOUND`.
- Multi-register dirty tracking exists for many partial-write cases and preserves the transport error for the current dirtying operation.
- Strict init correctly avoids claiming identity verification; ADS1115 has no ID register.
- README documents thread/ISR unsafety, external serialization, bus ownership, ALERT/RDY pull-up/pulse caveats, PGA absolute-limit caveat, raw threshold semantics, and continuous latest-vs-fresh behavior.
- Native tests passed locally: 57/57.
- Arduino ESP32-S3 and ESP32-S2 PlatformIO builds passed locally.

## High-Severity Findings

### H1. Failed `begin()` can hide partial hardware mutation

Severity: High

Evidence:

- `begin()` installs requested config, probes, then calls `_applyConfig()` at `src/ADS1115.cpp:199` and `:204`.
- `_applyConfig()` writes `LO_THRESH`, `HI_THRESH`, then `CONFIG`, and marks dirty after later failures at `src/ADS1115.cpp:1109`.
- `failBeginAfterConfig` clears `_hardwareConfigDirty`, `_hardwareConfigDirtyError`, and `_config` before returning at `src/ADS1115.cpp:187`.
- Existing test `test_begin_strict_readback_mismatch_fails_without_initializing` expects dirty false after a failed strict begin path at `test/test_basic.cpp:345`.

Impact:

- If the first threshold write reaches the chip and a later write or read-back fails, hardware can be left with partial config while the uninitialized driver exposes no dirty diagnostic. Field code can retry or switch workflows without knowing the chip may not match defaults or the requested config.

Recommended remediation:

- Preserve a begin-failure diagnostic across uninitialized state. Options: keep a sticky `lastBeginHardwareConfigDirty`/`lastBeginError`, leave initialized false but retain diagnostic fields, or return a distinct partial-state error while exposing the original transport error.
- Add failure-position tests for `begin()` writes 1, 2, 3 and strict read-back failures.

Suggested tests:

- Begin failure after second `_applyConfig()` write marks begin dirty and preserves original `Status`.
- Begin strict low/high/config read-back mismatch exposes read-back mismatch detail.
- A successful later `begin()` or `recover`/full apply clears the sticky begin dirty state only after confirmed full resync.

### H2. Hardware validation evidence is absent while release notes overclaim readiness

Severity: High

Evidence:

- No actual HIL log/capture artifacts were found in common formats.
- `CHANGELOG.md:41` says v1.0.0 is "production-ready".
- `README.md:310` lists a hardware validation matrix, but not results.
- `docs/HARDENING_FINAL_REPORT.md:178` says hardware validation is still required.
- `docs/ADS1115_SELFTEST_POLISH_REPORT.md:129` says hardware commands were not run and `:137` lists pending fresh hardware work.

Impact:

- A downstream user can interpret the package as field-proven when addresses, ALERT/RDY timing, comparator behavior, physical faults, and ESP-IDF boards have not been evidenced in this repo.

Recommended remediation:

- Reword release documentation to "feature-complete/API-stable pending hardware validation" unless logs are added.
- Create a validation-results matrix with date, board, commit, command transcript, instrument/capture artifact, and status.

Suggested tests:

- HIL address sweep 0x48..0x4B with CLI and logic analyzer.
- ALERT/RDY pulse capture at representative rates, especially continuous 860 SPS.
- Unplug/replug, brownout, stuck bus, wrong address, and manual recover logs.

### H3. Status taxonomy is not precise enough for production diagnostics

Severity: High

Evidence:

- `Err::BUSY` covers continuous mode active at `src/ADS1115.cpp:366`, conversion already in progress at `:369`, and offline latch at `:967`.
- No `OFFLINE`, `UNSUPPORTED_OPERATION`, `READBACK_MISMATCH`, or explicit partial-state code exists in `include/ADS1115/Status.h:10`.
- Strict read-back mismatches return `Err::I2C_ERROR` at `src/ADS1115.cpp:1172`, `:1182`, and `:1194`.
- `readBlocking()` accepts `Err::BUSY` from `startConversion()` as an existing-conversion path at `src/ADS1115.cpp:542`, making the source of BUSY important.

Impact:

- Applications must inspect strings or driver state to distinguish offline from unsupported mode from conversion busy. Field logs can misclassify register verification failure as a bus fault.

Recommended remediation:

- Append new error codes instead of reordering: `OFFLINE`, `UNSUPPORTED_OPERATION`, `READBACK_MISMATCH`, optionally `HARDWARE_CONFIG_DIRTY`.
- Make `readBlocking()` distinguish conversion-busy from offline/unsupported.
- Preserve current aliases where needed for source compatibility, but update docs and tests.

Suggested tests:

- OFFLINE public I2C returns `Err::OFFLINE` without bus access.
- `startConversion()` in continuous mode returns `Err::UNSUPPORTED_OPERATION`.
- Strict mismatch returns `Err::READBACK_MISMATCH` with observed register detail.

### H4. Raw public register writes can silently desync cache and hardware

Severity: High

Evidence:

- `writeRegister16()` writes any writable register through tracked I2C at `src/ADS1115.cpp:1003`.
- It does not update `_config` or mark dirty for raw writes to CONFIG, LO_THRESH, or HI_THRESH.
- README warns raw writes bypass typed helpers at `README.md:254`, but the API remains public.

Impact:

- Diagnostic or service code can write hardware state that diverges from cache. Later typed reads, voltage scaling, comparator state, or recover behavior can operate from stale cached assumptions without a dirty warning.

Recommended remediation:

- Either make raw writes explicitly diagnostic with a dirty/stale-cache flag, or update cache for supported raw CONFIG/threshold writes with validation.
- Expose `cacheDirtyFromRawWrite` or mark `hardwareConfigDirty()` after successful raw writes that change cached domains.

Suggested tests:

- Raw CONFIG write changes hardware and sets cache-stale/dirty diagnostic or updates cache.
- Raw threshold write followed by `getSettings()` does not falsely present old thresholds as synchronized.

## Medium-Severity Findings

### M1. `conversionReady()` hides fallible status

Severity: Medium

Evidence:

- `conversionReady()` discards `readConversionReady()` status and returns false at `src/ADS1115.cpp:414`.
- Header documents false on transport failure at `include/ADS1115/ADS1115.h:162`, but mandatory rules prefer meaningful `Status` for fallible APIs.

Impact:

- Callers using the convenience method cannot distinguish "not ready" from "I2C failure" except via health diagnostics.

Recommended remediation:

- Deprecate bool-only `conversionReady()` or make docs strongly steer production users to `readConversionReady(bool&)`.
- Consider adding `Status conversionReady(bool&)` as the primary API name in a future major/minor-compatible path.

Suggested tests:

- Bool convenience false-on-error remains documented.
- Explicit readiness API preserves transport error.

### M2. `tick()` can perform I2C and drops immediate status

Severity: Medium

Evidence:

- `tick()` calls `_readConversionReadyAt()` and ignores status at `src/ADS1115.cpp:222`.
- Header requires `void tick(uint32_t nowMs)` at `include/ADS1115/ADS1115.h:87`.

Impact:

- Health is updated, but the scheduler cannot directly log the failure from the tick call.

Recommended remediation:

- Keep `void tick()` for compatibility, but document that failures are visible through `lastError()`, `state()`, and counters.
- Add `Status service(uint32_t nowMs)` or similar in a future API if status-returning service is desired.

Suggested tests:

- Tick-induced I2C failure updates health and last error.

### M3. Optional `nowMs` affects more than blocking reads

Severity: Medium

Evidence:

- `Config::nowMs` is documented as required by blocking APIs in `include/ADS1115/Config.h:127`.
- `_nowMs()` returns 0 without a hook at `src/ADS1115.cpp:1223`.
- Nonblocking conversion start/readiness and health timestamps also use `_nowMs()` at `src/ADS1115.cpp:380`, `:421`, `:515`, `:1066`.

Impact:

- Without a hook, health timestamps stay 0 and direct nonblocking readiness can remain ambiguous unless the caller uses `tick(nowMs)` consistently.

Recommended remediation:

- Require `nowMs` in `begin()` for all managed conversion/health features, or document a degraded no-clock contract explicitly.
- Add a diagnostic flag in `SettingsSnapshot` that timestamps are unavailable.

Suggested tests:

- Direct `readConversionReady()` without `nowMs` after `startConversion()` behavior is pinned.
- Health diagnostics render "time unavailable" rather than "never".

### M4. `readBlocking()` loop can multiply I2C polls after ready time

Severity: Medium

Evidence:

- Deadline loop starts at `src/ADS1115.cpp:564`.
- Same-tick guard permits up to 65,536 same-tick iterations at `src/ADS1115.cpp:560`.
- After `readyAtMs`, repeated `readRaw()` calls can each include readiness I2C and conversion reads until timeout.

Impact:

- Bound is finite, but practical latency can be `timeoutMs` plus multiple transport timeouts depending on clock and transport behavior.

Recommended remediation:

- Use a deadline-aware single poll cadence or compute remaining budget and pass smaller timeout to transport if adapters support it.
- Clarify latency docs: blocking conversion bound depends on both API timeout and per-transaction timeout.

Suggested tests:

- Fake clock that advances slowly verifies maximum poll count and total transaction count.

### M5. ESP-IDF example error mapping is functional but not precise

Severity: Medium

Evidence:

- `examples/esp_idf/basic/main/main.cpp:32` maps `ESP_ERR_TIMEOUT` to `I2C_TIMEOUT`, invalid state/arg to `I2C_BUS`, and everything else to `I2C_ERROR`.

Impact:

- Address NACK/data NACK may not be distinguishable in ESP-IDF diagnostics, weakening field logs versus Arduino Wire mapping.

Recommended remediation:

- Document that the ESP-IDF example is minimal and not a production diagnostic mapper.
- Add an adapter pattern or platform note for mapping ESP-IDF transaction outcomes as precisely as possible.

Suggested tests:

- ESP-IDF hardware/fault-injection logs for missing device, SDA stuck, SCL stuck, and timeout.

### M6. Native test gaps remain despite strong baseline

Severity: Medium

Evidence:

- 57 native tests are present and pass.
- Gaps: invalid address/enum/ALERT config, tracked-path transport taxonomy, begin partial writes, raw-to-voltage boundaries, low/high strict read-back mismatch, `getThresholds()` sign reconstruction, more setter rollback variants.

Impact:

- The highest-risk public contracts have good coverage, but several edge contracts are still inferred rather than pinned.

Recommended remediation:

- Add a focused P1 test batch before broad implementation.

Suggested tests:

- See P1 plan below.

## Low-Severity Findings

### L1. Core has a FreeRTOS-specific comment

Severity: Low

Evidence:

- `src/ADS1115.cpp:581` says the cooperative yield feeds watchdog and lets other FreeRTOS tasks run.

Impact:

- No dependency leak exists, but wording weakens framework neutrality.

Recommended remediation:

- Change comment to framework-neutral wording such as "Give the application scheduler a chance to run."

### L2. Documentation has minor differential wording inconsistency

Severity: Low

Evidence:

- API supports four ADS1115 differential MUX codes.
- `docs/extracted-md/02_pinout_and_signals.md:35` says "two differential measurements", which matches high-level datasheet wording but not the full register selection list.

Impact:

- Could confuse users about available differential selections.

Recommended remediation:

- Clarify "two simultaneous differential input pairs / four differential MUX selections" in public docs.

### L3. Core guard script is useful but narrow

Severity: Low

Evidence:

- `tools/check_core_timing_guard.py` forbids `millis`, `micros`, `delayMicroseconds`, `yield`, and Arduino include.
- It does not scan for `delay(`, `Wire.h`, ESP-IDF/FreeRTOS includes, `String`, `Serial`, `TwoWire`, heap use, or logging APIs.

Impact:

- Framework leakage can still slip through unless code review catches it.

Recommended remediation:

- Expand forbidden token set and include heap/logging checks.

## API Latency / Blocking Table

Let `T = Config::i2cTimeoutMs`. Bounds assume the injected transport honors `T`.

| API | I2C transactions | Conversion wait | Other wait | Worst-case bound | Notes |
| --- | ---: | --- | --- | --- | --- |
| Constructor/destructor, deleted copy/move | 0 | None | None | Immediate | Copy/move deleted. |
| `begin()` | 1 CONFIG read + 3 writes; strict adds 3 reads | None | None | `4T`, strict `7T` | Failed begin can hide partial writes. |
| `tick(nowMs)` | 0 or 1 CONFIG read | No active wait | None | `T` | Drops immediate status; health captures tracked failures. |
| `end()` | 0 or 1 CONFIG write | None | None | `T` | Best-effort shutdown skipped while OFFLINE. |
| `shutdown()` | 1 CONFIG write | None | None | `T` | Returns status. |
| State/config/health getters, `getSettings()` | 0 | None | None | Immediate | Cache only. |
| `probe()` | 1 raw CONFIG read | None | None | `T` | No health tracking. |
| `recover()` | 1 CONFIG read + 3 writes; strict adds 3 reads | None | None | `4T`, strict `7T` | Allows I2C while OFFLINE only inside recover. |
| `startConversion()` | 1 CONFIG write | None | None | `T` | Single-shot only; returns `IN_PROGRESS`. |
| `startConversion(mux)` | 1 CONFIG write | None | None | `T` | Rolls cached MUX back on write failure. |
| `conversionReady()` | 0 or 1 CONFIG read | No active wait | None | `T` | Bool-only; false can mean error. |
| `readConversionReady(bool&)` | 0 or 1 CONFIG read | No active wait | None | `T` | Uses ALERT/GPIO, elapsed time, or OS bit depending config. |
| `readRaw()` | Continuous: 1 read; single: 0, 1, or 2 reads | No active wait | None | up to `2T` normally | Single-shot checks readiness first when needed. |
| `readLatestRaw()` | 1 conversion read | None | None | `T` | Bypasses readiness. |
| `readVoltage()` | Same as `readRaw()` | Same | None | Same as `readRaw()` | Adds local float scaling. |
| `readBlocking()` | Continuous: 1 read; single normal: start write + readiness + conversion read | Yes, deadline loop | Optional cooperative yield | API timeout plus repeated transaction cost | Requires `nowMs`; same-tick guard is finite. |
| `readBlockingVoltage()` | Same as `readBlocking()` | Same | Same | Same | Adds local float scaling. |
| `setMux()` / `setGain()` / `setDataRate()` / `setMode()` | 1 CONFIG write | None | None | `T` | Cache rollback on write failure. |
| `readConfig()` / `readRegister16()` / `readRegister()` | 1 read | None | None | `T` | OFFLINE short-circuits before I2C. |
| `writeConfig()` | 1 write | None | None | `T` | Syncs typed cache after success. |
| `writeRegister16()` / `writeRegister()` | 1 write | None | None | `T` | Raw writes can desync cache. |
| `setThresholds()` | 2 writes | None | None | `2T` | Dirty if second write fails after first reached hardware. |
| `getThresholds()` | 2 reads | None | None | `2T` | Syncs threshold cache after both reads. |
| Comparator setters, `disableComparator()` | 1 CONFIG write | None | None | `T` | Cache rollback on failure. |
| `enableConversionReadyPin()` | 3 writes; strict adds 3 reads | None | None | `3T`, strict `6T` | Partial failure sets dirty except begin path issue. |
| `rawToVoltage()`, `getLsbVoltage()`, `getConversionTimeMs()` | 0 | None | None | Immediate | Pure calculations. |

Conversion time table in code:

| Data rate | Driver conservative delay |
| --- | ---: |
| 8 SPS | 130 ms |
| 16 SPS | 68 ms |
| 32 SPS | 37 ms |
| 64 SPS | 21 ms |
| 128 SPS | 10 ms |
| 250 SPS | 6 ms |
| 475 SPS | 4 ms |
| 860 SPS | 3 ms |

## Partial Hardware State / Cache Consistency

Strong points:

- `_applyConfig()` writes low threshold, high threshold, and CONFIG; failures after the first write mark dirty.
- `setThresholds()` commits cache only after both threshold writes succeed and marks dirty if the second write fails.
- `enableConversionReadyPin()` rolls cache back on `_applyConfig()` failure and exposes dirty for partial apply failures after initialization.
- Successful full `_applyConfig()` clears dirty only after all writes and optional strict verification pass.
- Config-only typed setters roll cache back on write failure and do not clear existing dirty state.

Gaps:

- `begin()` clears dirty diagnostics after failed `_applyConfig()`, even though hardware may have been partially changed.
- Raw public register writes can intentionally or accidentally desync cache without dirty/stale-cache visibility.
- `_markHardwareConfigDirty()` overwrites the prior dirty error; this preserves the latest dirtying failure, not necessarily the first unresolved dirty event.
- Tests do not directly pin begin-path partial write failures or all `enableConversionReadyPin()` failure positions.

## Continuous Mode Semantics

The code and docs correctly treat continuous-mode reads as latest-register reads, not guaranteed fresh samples. `readRaw()` and `readLatestRaw()` return the conversion register immediately in continuous mode. `readConversionReady()` uses elapsed data-rate timing as a fresh-sample indication and does not prove the hardware register changed. Datasheet wording supports this: continuous mode writes completed conversions to the conversion register and data can be read any time as the most recent completed conversion.

Risk:

- Any claim of "fresh since caller's last read" requires explicit tracking beyond the current latest-register read.

## ALERT/RDY and Comparator Assessment

PASS:

- Traditional/window comparator fields, polarity, latch, and queue are represented.
- Threshold registers are treated as signed raw codes.
- Conversion-ready mode programs `Lo_thresh = 0`, `Hi_thresh = 0x8000`, queue assert-1, traditional, non-latching.
- GPIO read is injected; core does not own pin setup.
- Docs warn that ALERT/RDY is open-drain, requires pull-up, and ready pulses can be short.

Remaining validation:

- Capture ALERT/RDY pulse width in continuous mode, especially at 860 SPS.
- Validate active-high/active-low behavior and latch clearing on actual hardware.
- Validate comparator queue depths and raw threshold recalculation across PGA changes.

## Strict Init / Read-Back Assessment

ADS1115 has no device-ID register. The library correctly does not claim identity verification. `begin()` checks presence by reading CONFIG through `probe()`. `strictInitVerify` checks low threshold, high threshold, and masked CONFIG writable fields after a full apply; dynamic OS/status is masked out.

Gaps:

- Strict mismatch returns generic `I2C_ERROR`.
- Failed strict `begin()` can hide dirty state from partial earlier writes.
- Tests cover CONFIG mismatch and OS masking but not low/high threshold mismatch and strict read transport failure branches.

## Thread and ISR Safety Assessment

The public contract is explicit: the driver is not internally thread-safe and is not ISR-safe. Applications must externally serialize all calls into a driver instance. This is documented in `include/ADS1115/ADS1115.h:56` and `README.md:282`.

This is acceptable for a small embedded driver as long as examples and adapters keep showing external locking where relevant.

## Tests and CI Coverage

Local command results:

- `python --version` - Python 3.13.12
- `python -m platformio --version` - PlatformIO Core 6.1.19
- `python tools/check_core_timing_guard.py` - PASSED
- `python tools/check_cli_contract.py` - PASSED
- `python tools/check_idf_example_contract.py` - PASSED
- `python scripts/generate_version.py check` - up to date
- `python -m platformio test -e native` - PASSED, 57 test cases succeeded
- `python -m platformio run -e esp32s3dev` - SUCCESS
- `python -m platformio run -e esp32s2dev` - SUCCESS
- `python -m platformio pkg pack` - SUCCESS; generated tarball removed after validation
- `idf.py --version` - FAILED, command not found
- `idf.py -C examples/esp_idf/basic set-target esp32s3 build` - not run because `idf.py` is unavailable
- `idf.py -C examples/esp_idf/basic set-target esp32s2 build` - not run because `idf.py` is unavailable

CI coverage:

- PlatformIO Arduino builds for `esp32s3dev` and `esp32s2dev`.
- Native tests.
- Core timing guard.
- ESP-IDF container builds for `esp32s3` and `esp32s2`.
- CLI and IDF example contract guards.
- Package validation.

Native test strengths:

- Fake register-backed transport.
- Per-call read/write failure injection.
- Copy/move prevention static asserts.
- Probe side-effect behavior.
- Offline latch and manual recovery.
- Strict read-back OS masking.
- Continuous latest/fresh distinction.
- ALERT/RDY GPIO path avoids CONFIG polling.
- Partial multi-register dirty paths through recover/apply.

Native test gaps:

- Direct `probe()` success no-health test.
- Invalid I2C address, invalid enum values, invalid ALERT/RDY pin config.
- Tracked normal read/write taxonomy across all transport errors.
- Begin-path partial writes.
- Low/high strict read-back mismatch and strict read transport failures.
- More setter rollback variants.
- `getThresholds()` sign reconstruction.
- `INT16_MIN`/`INT16_MAX` threshold boundaries.
- `rawToVoltage()` and `getLsbVoltage()` boundary scaling across every gain.
- Broader framework-leakage guard coverage.

## ESP-IDF and Arduino Integration Assessment

Arduino:

- PlatformIO has `esp32s3dev` and `esp32s2dev`; both build locally.
- Arduino bring-up CLI is clearly labeled diagnostic, not a production shared-bus manager.
- Wire adapter maps common Wire return codes to distinct library statuses and warns production adapters need locking and timeout policy.
- CLI calls `device.tick(millis())` in `loop()`, but blocking commands can monopolize the diagnostic loop. This is acceptable for a diagnostic example, not a production console model.

ESP-IDF:

- Root `CMakeLists.txt` and `idf_component.yml` exist.
- `examples/esp_idf/basic` uses `app_main`, `driver/i2c_master.h`, `esp_timer`, FreeRTOS mutex/task delay, native buffers, and no Arduino facades.
- ESP-IDF example demonstrates external bus context and mutex locking.
- Local `idf.py` is unavailable, so pure IDF builds were not run locally. CI is configured to run them in an Espressif IDF container.
- ESP-IDF example error mapping is coarse and should not be represented as production-precise diagnostics.

## Hardware Validation Matrix

No hardware was run in this audit. Before field or production claims, capture dated logs/captures for:

| Area | Required cases | Evidence expected |
| --- | --- | --- |
| Test identity | Commit SHA, version, ADS1115 board, MCU board, VDD, pull-ups, bus speed, wiring, ambient, instruments | Serial header, schematic/photo, instrument setup |
| Addresses | 0x48, 0x49, 0x4A, 0x4B | `addr`, `probe`, `cfg`, `selftest`, stress logs and I2C capture |
| Wrong/missing address | Unpopulated address and changed ADDR strap | Expected NACK/device-not-found status and health behavior |
| Mux raw/voltage | All 8 MUX selections | Precision source/DMM comparison, raw sign and voltage tolerance |
| Gains | All 6 PGA settings | Safe input-level scaling; no rail-limit violation |
| Data rates | 8, 16, 32, 64, 128, 250, 475, 860 SPS | Measured cadence/tolerance |
| Modes | Single-shot, continuous, mode switching | Correct power-down/latest behavior and post-change settling discipline |
| Blocking/tick | `startConversion`, `readConversionReady`, `readRaw`, `readBlocking`, `tick()` | Bounded waits and timeout behavior |
| Comparator traditional | Threshold crossings, polarity, latch, queues | ALERT/RDY assertion and clear behavior |
| Comparator window | Below/inside/above window, latch, polarity, queues | ALERT/RDY behavior and threshold logs |
| Conversion-ready ALERT/RDY | Single-shot and continuous, 8/128/860 SPS, pull-up values | Logic analyzer pulse width and GPIO/interrupt capture rate |
| Address NACK | Device absent/wrong address | Status mapping and offline/recover behavior |
| Data NACK | Fault injector/emulator data-phase NACK | `I2C_NACK_DATA` and detail preservation |
| Stuck bus | SDA low, SCL low, arbitration/bus error if possible | Adapter maps timeout/bus error; app-level recovery logs |
| Unplug/replug | Remove during reads, reconnect, recover | DEGRADED/OFFLINE, no bus hammering, manual recover returns READY |
| Brownout/reset | Drop ADS1115 VDD and restore | No silent success; resync after recover/begin |
| Partial write | Fault injection after first/second writes | Dirty diagnostic and full resync clear |
| ESP-IDF S2/S3 | Native example on both boards | Build, serial output, I2C captures, error mapping |
| Arduino S2/S3 | CLI on both boards | Functional matrix and command transcript |
| Soak/stress | 24 h nominal, 2 h 860 SPS, fault/recover cycles | No unexpected statuses; bounded counters; analog sanity |

## Recommended Future Implementation Plan

### P0 - Must fix before production claim

- Preserve partial hardware mutation diagnostics when `begin()` fails after writes or strict read-back.
- Reword `CHANGELOG.md` and any release docs that call v1.0.0 production-ready unless HIL logs are added.
- Add status codes or otherwise make OFFLINE, unsupported operation, strict read-back mismatch, and partial hardware state distinguishable without string parsing.
- Decide and document raw register write cache/dirty semantics.
- Add begin-path partial-write tests before changing implementation.

### P1 - Should fix before release/merge

- Deprecate or sharply document bool-only `conversionReady()` for production use.
- Document `tick()` failure visibility through health diagnostics or add a status-returning service API.
- Clarify `nowMs` requirements beyond blocking reads, especially for health timestamps and direct nonblocking readiness.
- Tighten `readBlocking()` polling cadence and latency documentation.
- Expand native tests for invalid config, transport taxonomy on tracked paths, strict read-back branches, `getThresholds()`, raw voltage scaling, and more setter rollback paths.
- Improve ESP-IDF adapter error mapping notes and test evidence.
- Expand core guard script to catch broader framework and heap/log leakage.

### P2 - Useful hardening

- Replace the FreeRTOS-specific core comment with framework-neutral wording.
- Add README address strap mapping and ADDR/SDA caveat from datasheet.
- Clarify differential selection wording in docs.
- Add validation-results matrix with status/date/board/log columns.
- Automate or check version sync across `library.json`, `idf_component.yml`, `Doxyfile`, and generated `Version.h`.

## Proposed Implementation Branch

```text
hardening/ads1115-industry-readiness
```

## Final Verdict

Do not implement broad refactors from this branch. The correct next step is a focused hardening branch that first fixes begin-path partial-state diagnostics and status taxonomy, then expands targeted native fault tests. Hardware validation should be gathered before any renewed production or industry-grade claim. Documentation wording can be corrected immediately because it currently overstates readiness relative to the checked-in evidence.
