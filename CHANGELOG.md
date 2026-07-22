# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- Kept explicitly staged diagnostic jobs under `job poll` ownership in both
  Arduino and ESP-IDF loops so zero and bounded callback budgets remain
  observable instead of being consumed by background service.
- Hardened the HIL gate so exhaustive runs include full and targeted boundary
  coverage, all current error statuses are rejected, self-test skips fail the
  contract, any soak command failure fails the soak, and a failed prerequisite
  plan cannot silently continue into a requested soak.
- Added owner-safe native regressions for profile commit, tokened shutdown,
  recovery after failed initialization, and signed saturation flags; the suite
  now contains 178 tests.

### Changed

- Consolidated unfinished release and integration work in `docs/OPEN_ITEMS.md`
  and moved completed release/audit material out of the active documentation.
- Replaced long HIL transcripts and prompt-era reports with concise historical
  summaries that preserve identities, commands, outcomes, limitations, and
  artifact hashes.
- Removed the redundant generated datasheet text extraction and aligned
  contributor, security, Doxygen, and engineering-contract documentation with
  the 2.0 owner-safe lifecycle.

### Removed

- Removed the legacy unclassified HIL capture helper; the classified runner is
  now the single supported serial validation path.

## [2.0.0] - 2026-07-19

### Added

- Fixed-memory owner API: `bind()`, tokened `startInitialize()`,
  `startApplyProfile()`, `startRecover()`, `startRead()`, `startShutdown()`,
  transaction-budgeted `poll()`, bus-silent `cancelActiveOperation()`,
  exactly-once `takeResult()`, and bus-silent `unbind()`.
- `DriverConfig`, complete `DeviceProfile` / `ComparatorProfile`, and typed
  `ChannelRequest` contracts with validation before I2C.
- Explicit `ConfigurationState`, configuration generation, `OperationKind`,
  `OperationState`, `OperationToken`, cancellation disposition, and terminal
  hardware-uncertainty reporting.
- Atomic `SampleResult` provenance: raw code, rounded ADC-input microvolts,
  application channel ID, MUX, PGA, data rate, flags, verified configuration
  generation, and successful-sample sequence.
- Pure bounded helpers for profile/request validation, SPS lookup, worst-case
  conversion time, gain full scale, integer microvolt conversion, MUX input
  mapping, and multi-channel deadline derivation.
- Native fault injection for applied-then-error writes, token/result lifetime,
  per-poll budgets, deadline reconciliation, cancellation after each effect
  stage, unknown configuration, passive health, verified shutdown, continuous
  settling, active-conversion rebind rejection, owner-time health timestamps,
  post-callback abandoned-conversion timing, multi-callback deadline
  partitioning, staged terminal acknowledgement, threshold-profile trust, and
  bus-silent teardown. The native suite now contains 174 tests.
- A compiled owner-safe Arduino example with a static shared-bus mutex,
  deadline-aware callback timeout policy, and one callback per owner-loop pass.

### Changed

- Owner initialization/recovery always writes and reads back both thresholds
  plus masked CONFIG fields. ADS1115 reachability/readback remains plausibility,
  not chip identity.
- Owner single-shot reads verify CONFIG before reading conversion data and
  publish samples only from `VERIFIED`, clean configuration state.
- Whole-operation deadlines partition the remaining time across each poll's
  callback budget, so the sum of callback timeout caps cannot exceed the
  remaining boundary. Conversion waits consume no transport budget.
- Cancel/timeout after a confirmed or ambiguous start now enters bus-silent
  wait-idle reconciliation timed from the first post-callback owner poll;
  abandoned conversions cannot be reused under a new MUX or gain.
- Health `OFFLINE` is now a passive diagnostic threshold. It no longer denies
  owner-authorized transport callbacks or owns recovery admission.
- `end()` is bus-silent. Hardware idle is requested explicitly through
  `startShutdown()` or the synchronous `shutdown()` compatibility facade.
- Compatibility `begin()` now performs mandatory readback regardless of the
  supplied `strictInitVerify` value. The field remains for source migration.
- Direct typed/raw mutation APIs are classified as advanced diagnostics and
  move configuration trust to `UNKNOWN`/dirty until verified replay.
- Diagnostic CLIs now complete typed mutations with a bounded full
  apply/readback, acknowledge staged terminal tokens, and test continuous mode
  through latest-raw success plus explicit scaled-read rejection.
- Production acquisition is fixed-profile single-shot OS polling. Continuous
  latest-register and ALERT/RDY GPIO paths remain diagnostic; continuous timing
  now accounts for -10% rate tolerance and two settling periods after change.
- PlatformIO Core is pinned to `6.1.19`, PlatformIO Native to `1.2.1`,
  pioarduino espressif32 to the exact `54.03.20` archive, and ESP-IDF CI to the
  `v5.3.5` image digest. Host runner/compiler variability remains explicit.

### Compatibility

- Existing error enum numeric values remain unchanged; `CANCELLED`,
  `CONFIG_UNKNOWN`, `RESULT_NOT_AVAILABLE`, `TOKEN_MISMATCH`, and
  `INDETERMINATE` are appended.
- The 1.x `Config`, blocking, direct setter/register, and staged-job APIs remain
  available for diagnostic/source migration, but lifecycle, verification,
  health admission, continuous reads, and shutdown behavior changed. Review
  the 2.0 migration section in `README.md` before updating production callers.

### Validation limits

- Existing COM19/COM8 captures predate 2.0. Clean current hardware evidence is
  still required for final boards, analog accuracy, shared-bus contention,
  cancellation/timeout, electrical faults, and production workload.
- TunnelMonitor-node was re-audited but not changed: product role, board/profile,
  channel meanings, analog front end, units, calibration, capacity, and product
  acceptance remain external integration gates.

## [1.2.0] - 2026-06-25

### Added
- Appended `Err::CLOCK_STALLED` for blocking waits whose injected monotonic
  clock does not advance.
- `SettingsSnapshot::hardwareConfigDirtyAddress` for preserving the address
  associated with dirty hardware/cache diagnostics.
- HIL runner `--fail-on-unknown`, contract/evidence verdict reporting, and
  summary-anchored `stress_mix` failure parsing.

### Changed
- `readBlocking()` now waits for a fresh continuous-mode sample instead of
  returning the latest continuous conversion register immediately.
- `startApplyConfigJob()` now supports normal continuous-mode conversion state
  and only rejects active single-shot conversions.
- The ESP-IDF diagnostic example now supports `addr <0x48..0x4B>`, reports
  dirty/timebase settings fields, and uses `service(nowMs)` for observable
  periodic driver work.
- `bool conversionReady()` is now explicitly marked compatibility-only;
  examples and normal tests use `readConversionReady(bool&)`.
- HIL runner analog/electrical evidence gaps are reported as
  `EVIDENCE_REQUIRED`; `UNKNOWN` is reserved for genuinely ambiguous runner
  outcomes.
- Release-facing docs now collapse prompt-era COM8 HIL reports and tracked local
  HIL summaries into one compact validation summary.

### Fixed
- First poll-single-shot CONFIG write failures no longer mark hardware/cache
  dirty for definite address absence.
- `stress_mix` HIL validation no longer accepts an earlier progress `fail=0`
  when the final summary reports failures.

## [1.1.0] - 2026-06-02

### Added
- ESP-IDF component metadata, generated-version CMake support, and a native
  ESP-IDF `i2c_master` example with full bring-up CLI command parity.
- `tools/check_idf_example_contract.py` to guard ESP-IDF example structure,
  native-driver dependencies, and CLI parity.
- `SettingsSnapshot` struct for reading cached configuration and runtime state without I2C.
- `getSettings(SettingsSnapshot&)` method to populate a settings snapshot.
- `Status::is(Err)` method for type-safe error code comparison.
- `Status::operator bool()` explicit conversion for concise success checks.
- `readRegister()` and `writeRegister()` compatibility aliases for `readRegister16()` / `writeRegister16()`.
- `readConversionReady(bool&)` for conversion readiness checks with explicit transport error reporting.
- `conversionReady(bool&)` status-returning alias while keeping the existing bool-only convenience overload.
- `service(uint32_t)` status-returning periodic service while keeping `tick(uint32_t)` for compatibility.
- `shutdown()` public API for explicit best-effort single-shot idle handling before `end()`.
- `readLatestRaw(int16_t&)` for continuous-mode latest-register reads without promising a fresh sample.
- `Config::strictInitVerify` for optional writable-register read-back plausibility checks.
- `hardwareConfigDirty()` and `hardwareConfigDirtyError()` diagnostics for partial apply and raw diagnostic writes.
- Appended `Err::OFFLINE`, `Err::UNSUPPORTED_OPERATION`, `Err::READBACK_MISMATCH`, and `Err::HARDWARE_CONFIG_DIRTY` without reordering existing status values.
- Datasheet PGA alias handling for raw CONFIG writes: encodings `110` and `111` are accepted as `+/-0.256 V`.
- Native coverage for register-modeled conversion reads, readiness failures, ALERT/RDY readiness, setter rollback, register validation, and stalled-clock blocking timeouts.
- Native coverage proving latched `OFFLINE` blocks normal I2C operations without touching the bus while `recover()` remains the explicit recovery path.
- Native coverage for invalid config boundaries, tracked I2C status taxonomy, strict read-back recover branches, signed threshold/scaling boundaries, setter rollback variants, and dirty-state preservation.
- Version metadata checks now verify `library.json`, `idf_component.yml`, `Doxyfile`, and generated `Version.h` agree.
- Limited COM19 HIL evidence for address handling, restore sequencing,
  initialized-address selftests, and short stress runs. The raw transcript is
  tracked under `docs/evidence/hil/2026-06-02_COM19/`.

### Changed
- Driver core timing/yield ownership moved fully behind application callbacks;
  Arduino examples now provide explicit timing hooks instead of relying on core fallbacks.
- Doxyfile project metadata now matches `library.json`, and archived prompt
  metadata no longer contains placeholder ownership values.
- Core guard script now rejects framework leakage and dynamic allocation patterns
  in `include/` and `src/`, including Arduino/Wire symbols, ESP-IDF/FreeRTOS
  symbols, logging calls, `std::string`, `std::vector`, and heap allocation.
- Reference documentation now uses a human-readable vendor PDF name and separates compact chip notes from full PDF extraction under `docs/reference/extracted-md/` and `docs/reference/pdf-extracted-md/`.
- Explicit recovery bypass internals now use the shared `ScopedOfflineI2cAllowance` / `_reassertOfflineLatch()` procedure so failed recovery attempts that begin from `OFFLINE` keep the latch asserted.
- Continuous-mode readiness now tracks the configured data-rate interval instead of reporting ready immediately.
- CLI `poll`, `selftest`, and mixed stress paths now handle `Err::IN_PROGRESS` correctly and preserve readiness I2C errors.
- README now documents conversion, configuration, comparator, ALERT/RDY, and configuration constraint APIs.
- `begin()` failure now clears stale cached configuration/runtime state, and successful startup no longer seeds runtime health counters.
- Health behavior is now standardized on latched `OFFLINE`: normal public I2C operations return `Err::OFFLINE` with `Driver is offline; call recover()` and do not touch I2C until `recover()` succeeds.
- Hardware validation wording now uses pending evidence rows instead of implying
  fresh HIL coverage.
- The ESP-IDF example now exposes the same user-visible commands, help,
  diagnostics, status output, register access, comparator controls, stress
  paths, and self-test flow as the Arduino CLI without including Arduino CLI
  sources or compatibility facades.
- `examples/common/` is now Arduino example glue only; the IDF example owns its
  native stdio CLI, GPIO, timing, scan, and transport code.
- Release-facing documentation now has an explicit `docs/README.md` index.
  Historical audit and hardening reports were moved under `docs/archive/`.
- README validation wording now distinguishes limited COM19 HIL evidence from
  hardware validation that remains pending.

### Fixed
- Typed config and comparator setters no longer commit cached configuration changes when their I2C writes fail.
- Raw register helpers now reject pointers outside the ADS1115 `0x00..0x03` map, and reject conversion-register writes to read-only `0x00`, before touching the bus.
- Example diagnostic error strings now include granular `I2C_*` status codes.
- `readBlocking()` now has a finite escape path if an injected clock hook stops advancing.
- Arduino CLI address selection now preserves the previously initialized driver
  and transport callbacks when probing an absent requested address.
- HIL capture waits for the CLI prompt before sending the next command, avoiding
  overlapped long-running stress commands.

## [1.0.0] - 2026-04-05

### Changed
- Promoted to v1.0.0 as a feature-complete, production-oriented API-stable candidate pending dated hardware validation evidence.
- Hardware/build validation status must be tracked through explicit run logs,
  not inferred from this changelog entry.

## [0.4.0] - 2026-04-05

### Added
- Public lifecycle introspection helpers: `isInitialized()` and `getConfig()`.
- Public tracked raw-register helpers: `readRegister16()` and `writeRegister16()`.
- `Err::MEASUREMENT_NOT_READY` alias for cross-library uniformity.

### Changed
- `end()` now best-effort returns the ADC to single-shot idle and clears cached conversion state.
- `recover()` now clears conversion state and re-applies cached configuration after the tracked probe succeeds.
- Bringup CLI now exposes `reg` / `wreg` diagnostics for raw register inspection and service writes.

## [0.3.0] - 2026-04-03

### Added
- Granular I2C transport status codes: `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, and `I2C_BUS`.

### Changed
- Documented the newer transport contract in the README, including granular `I2C_*` status mapping, `i2cUser` usage, and manager-owned timeout guidance.
- `examples/common/I2cTransport.h` now uses `TwoWire*` via `i2cUser`, validates buffer parameters, and treats per-call `timeoutMs` as advisory.
- Added native coverage for raw transport parameter validation.

### Fixed
- `_i2cWriteRaw()` and `_i2cWriteReadRaw()` now reject null and zero-length transactions before calling the injected transport.

## [0.2.1] - 2026-04-03

### Added
- `CommandHandler.h` example helper for serial command parsing (`cmd::readLine`, `cmd::match`, `cmd::parseInt`).
- `HealthDiag.h` example helper with verbose health diagnostics, color-coded output, snapshots, diffs, and `HealthMonitor` class for continuous monitoring.

### Changed
- `I2cScanner.h` upgraded: advanced table-format scan, bus recovery via `recoverBus()`, timeout support, yield() calls, common address hints, `LOG_SERIAL` macro usage.
- `I2cTransport.h` upgraded: `TwoWire*` via user pointer (no global Wire), null pointer checks, 128-byte buffer validation, detailed per-error-code mapping, `ARDUINO_ARCH_ESP32` guards.
- `BusDiag.h` updated to use `i2c_scanner::scan(Wire)` and include `<Wire.h>`.
- `Log.h`: added `LOGV` runtime-verbose macro, ESP32-S3 USB CDC delay in `log_begin()`.
- `BuildConfig.h`: added Doxygen header comment block.
- `main.cpp` example updated to use `bus_diag::scan()`, set `cfg.i2cUser = &Wire`, corrected includes.

## [0.2.0] - 2026-03-01

### Changed
- Unified `01_basic_bringup_cli` command behavior and operator-facing output clarity.
- Updated `docs/IDF_PORT.md` to reflect current timing-hook architecture and no direct core timing calls in driver core.

### Fixed
- CLI reporting consistency for probe/stress/health paths in the bringup example.

## [0.1.2] - 2026-02-28

### Added
- Unified example framework helpers under `examples/common/*` (`BuildConfig`, `BusDiag`, `CliShell`, `HealthView`, `TransportAdapter`).
- `docs/IDF_PORT.md` for standardized ESP-IDF portability guidance.
- CLI/timing contract checks via `tools/check_cli_contract.py` and `tools/check_core_timing_guard.py`.

### Changed
- Consolidated example workflow to canonical `examples/01_basic_bringup_cli`.
- Aligned CI/test layout to the shared unification profile (`test/test_basic.cpp`, platform and workflow updates).

## [0.1.1] - 2026-02-22

### Fixed
- `readBlocking()` tight CPU spin loop without `yield()` — could trigger ESP32 watchdog timeout at low data rates (up to 130 ms at 8 SPS)
- `readBlocking()` stale deadline calculation when joining an already-running conversion (BUSY path)
- `readBlocking()` not clearing `_conversionStarted` on timeout — caused all subsequent `startConversion()` calls to permanently return BUSY
- `int16_t compThresholdLow` default `0x8000` narrowing conversion — changed to portable `-32768`
- `enableConversionReadyPin()` same narrowing fix for high threshold assignment
- Example CLI unbounded serial input buffer — capped at 128 characters to prevent heap exhaustion
- Example CLI unbounded `read N` / `stress N` iteration count — capped at 10 000 / 100 000
- Test `RUN_TEST` macro missing `try` block — was a syntax error
- Test assertions used `assert()` which calls `abort()` — replaced with throwing macros that report file and line
- Test Arduino.h stub missing `#include <string>` and `yield()` stub
- Test Wire.h stub `write(uint8_t)` buffer overflow — added bounds check
- Test Wire.h stub `write(buf, len)` returned `len` instead of actual bytes written
- Test Wire.h stub `available()` unsigned underflow when `_rxIdx > _rxLen`

## [0.1.0] - 2026-01-19

### Added
- ADS1115 driver with injected I2C transport and health tracking
- Single-shot and continuous conversion APIs with voltage helpers
- Comparator configuration and ALERT/RDY support
- Bringup CLI example for ESP32-S2 / ESP32-S3

[Unreleased]: https://github.com/janhavelka/ADS1115/compare/v2.0.0...HEAD
[2.0.0]: https://github.com/janhavelka/ADS1115/compare/v1.2.0...v2.0.0
[1.2.0]: https://github.com/janhavelka/ADS1115/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/janhavelka/ADS1115/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/janhavelka/ADS1115/compare/v0.4.0...v1.0.0
[0.4.0]: https://github.com/janhavelka/ADS1115/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/janhavelka/ADS1115/compare/v0.2.1...v0.3.0
[0.2.1]: https://github.com/janhavelka/ADS1115/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/janhavelka/ADS1115/compare/v0.1.2...v0.2.0
[0.1.2]: https://github.com/janhavelka/ADS1115/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/janhavelka/ADS1115/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/janhavelka/ADS1115/releases/tag/v0.1.0
