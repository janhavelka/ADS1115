# ADS1115 I2C Industry Readiness Engineering Report

Date: 2026-05-29

## Executive Summary

The ADS1115 workspace is in a strong pre-production state for an embedded I2C
ADC library. The core driver has the right architectural direction: it is
transport-injected, framework-neutral, bounded at the I2C transaction level,
status-returning, health-tracked, and mostly complete against the ADS1115 feature
set.

It is not yet fully "industry-grade" as a shipped field library. The remaining
gaps are not basic coding quality issues. They are production-hardening issues:
timebase contracts, partial multi-register write behavior, worst-case latency
documentation, shared-bus integration, ESP-IDF build validation, hardware fault
validation, and explicit concurrency/ISR contracts.

Overall readiness classification:

**Solid engineering-grade / pre-production. Not yet industry-grade for unattended
field deployment without additional hardening and validation.**

## Scope

Reviewed areas:

- Core public API: `include/ADS1115/*.h`
- Core implementation: `src/ADS1115.cpp`
- Arduino example transport and scanner: `examples/common/*`
- ESP-IDF example transport and CLI: `examples/esp_idf/basic/*`
- Build/package metadata: `platformio.ini`, `library.json`, `CMakeLists.txt`,
  `idf_component.yml`
- Tests and guard scripts: `test/test_basic.cpp`, `tools/*.py`
- Documentation: `README.md`, `docs/IDF_PORT_IMPLEMENTATION.md`

Review focus:

- I2C ownership and transport abstraction
- Determinism and timeout behavior
- Error/status mapping
- Health tracking and recovery semantics
- Framework independence
- ESP32-S2 / ESP32-S3 Arduino and ESP-IDF readiness
- Example adapter quality
- Test, CI, packaging, and release readiness

## Readiness Scorecard

| Area | Rating | Notes |
| --- | --- | --- |
| Core I2C ownership model | Strong | Driver never owns I2C and uses injected callbacks. |
| Core framework independence | Strong | No Arduino/ESP-IDF headers in `include/` or `src/`. |
| Status/error model | Good | Status codes are consistent, but `probe()` loses diagnostic precision. |
| Health tracking | Good | Centralized tracked wrappers and offline latch are implemented. |
| Determinism | Medium | I2C operations are bounded by callback timeout, but blocking conversion timeout depends on optional `nowMs`. |
| API latency contract | Medium | Calls are bounded but worst-case durations are not documented per API. |
| Multi-register atomicity | Medium/Weak | Cache rollback exists, but hardware can remain partially changed after failed multi-write operations. |
| ADS1115 feature coverage | Good | Main mux/gain/rate/mode/comparator/readiness features exist. |
| Arduino adapter/examples | Medium | Good bring-up examples, not production shared-bus templates. |
| ESP-IDF adapter/examples | Medium | Native IDF code exists and passes contract guard, but CLI loop and bus ownership are demo-grade. |
| Tests | Good | Native tests cover many contracts; hardware/fault-injection coverage is still missing. |
| CI/build matrix | Medium | Arduino S2/S3 and native tests are covered; ESP-IDF build is not proven in PlatformIO CI. |
| Documentation | Good | README and IDF port notes are useful; some production contracts remain implicit. |

## What Is Strong

### Transport Ownership Is Correctly Inverted

The core library does not touch `Wire`, IDF bus handles, pins, or bus
configuration. I2C is supplied through callbacks:

- `I2cWriteFn` in [`include/ADS1115/Config.h`](../include/ADS1115/Config.h#L18)
- `I2cWriteReadFn` in [`include/ADS1115/Config.h`](../include/ADS1115/Config.h#L30)
- Callback fields in [`Config`](../include/ADS1115/Config.h#L118)

This is the right model for production systems where the I2C bus is shared by
multiple devices and owned by a platform bus manager.

### Core Is Framework-Neutral

The core headers and implementation include only C/C++ standard headers and
project headers. They do not include `Arduino.h`, `Wire.h`, ESP-IDF headers, or
compatibility facades.

The timing guard confirms that framework timing calls are absent from core:

```text
python tools/check_core_timing_guard.py
Core timing guard PASSED
```

### Health Tracking Architecture Is Mostly Correct

The intended layered model is present:

```text
Public API
  -> register helpers
  -> tracked I2C wrappers
  -> raw I2C wrappers
  -> transport callbacks
```

Key locations:

- Tracked read wrapper:
  [`src/ADS1115.cpp`](../src/ADS1115.cpp#L921)
- Tracked write wrapper:
  [`src/ADS1115.cpp`](../src/ADS1115.cpp#L934)
- Health update:
  [`src/ADS1115.cpp`](../src/ADS1115.cpp#L1018)
- Raw diagnostic probe:
  [`src/ADS1115.cpp`](../src/ADS1115.cpp#L241)
- Manual recovery:
  [`src/ADS1115.cpp`](../src/ADS1115.cpp#L253)

The driver implements the expected four-state health model:

- `UNINIT`
- `READY`
- `DEGRADED`
- `OFFLINE`

The offline latch is also meaningful: normal tracked operations stop touching
the bus when the driver is offline and instruct the application to call
`recover()`.

### Status Model Is Suitable For Embedded Use

All fallible APIs return `Status` and avoid exceptions. `Status` is small,
static-message based, and carries an implementation detail field:

- [`include/ADS1115/Status.h`](../include/ADS1115/Status.h#L29)

The error set includes both generic and transport-specific I2C failures:

- `I2C_ERROR`
- `I2C_NACK_ADDR`
- `I2C_NACK_DATA`
- `I2C_TIMEOUT`
- `I2C_BUS`

### No Steady-State Heap Pattern In Core

The core driver stores fixed fields, uses stack buffers for register
transactions, and does not use `String`, `std::vector`, `new`, dynamic
allocation, or logging in the library implementation.

### ADS1115 Feature Coverage Is Broad

The core supports:

- 0x48 through 0x4B I2C addresses
- Four single-ended channels
- Four differential mux enum values present in code, including AIN0-AIN1,
  AIN0-AIN3, AIN1-AIN3, and AIN2-AIN3
- All ADS1115 PGA settings
- All ADS1115 data rates
- Single-shot mode
- Continuous mode
- Comparator mode, polarity, latch, and queue fields
- ALERT/RDY conversion-ready mode
- OS-bit conversion-ready polling
- Signed two's-complement conversion values
- Raw-to-voltage scaling

## Verification Performed

Commands run locally:

```text
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
```

Results:

| Command | Result |
| --- | --- |
| `check_core_timing_guard.py` | Passed |
| `check_cli_contract.py` | Passed |
| `check_idf_example_contract.py` | Passed |
| `generate_version.py check` | `Version.h` up to date |
| `python -m platformio test -e native` | Passed, 36/36 tests |
| `python -m platformio run -e esp32s3dev` | Success |
| `python -m platformio run -e esp32s2dev` | Success |

Arduino firmware build size results:

| Environment | RAM | Flash | Result |
| --- | --- | --- | --- |
| `esp32s3dev` | 22288 bytes, 6.8% | 394514 bytes, 30.1% | Success |
| `esp32s2dev` | 36736 bytes, 11.2% | 386349 bytes, 29.5% | Success |

Not verified in this pass:

- `idf.py -C examples/esp_idf/basic set-target esp32s3 build`
- `idf.py -C examples/esp_idf/basic set-target esp32s2 build`
- Hardware behavior on real ADS1115 devices
- Fault injection on ESP-IDF I2C NACK/timeout/bus-error paths

## High-Severity Findings

### 1. Blocking Timeout Behavior Depends On Optional `nowMs`

Severity: High

`Config::nowMs` is optional:

- [`include/ADS1115/Config.h`](../include/ADS1115/Config.h#L123)

But `readBlocking()` uses `_nowMs()` to compute deadlines:

- [`src/ADS1115.cpp`](../src/ADS1115.cpp#L492)
- [`src/ADS1115.cpp`](../src/ADS1115.cpp#L505)
- [`src/ADS1115.cpp`](../src/ADS1115.cpp#L522)

If no hook is supplied, `_nowMs()` returns `0`:

- [`src/ADS1115.cpp`](../src/ADS1115.cpp#L1106)

The implementation includes a same-tick spin guard, so it is not an unbounded
loop. However, the API parameter `timeoutMs` is not a real millisecond timeout
unless the application supplies `nowMs`.

Impact:

- Blocking conversion APIs can consume CPU in a tight polling path.
- Timeout behavior is deterministic but not time-correct without a clock hook.
- The API looks safer than it is for consumers who omit `nowMs`.

Recommended fix:

- Make `nowMs` required in `begin()` when users intend to call blocking
  conversion APIs, or
- Make `readBlocking()` and `readBlockingVoltage()` return `INVALID_CONFIG`
  when `_config.nowMs == nullptr`, or
- Split APIs into clearly named clock-dependent and tick-driven variants.

Recommended contract:

```text
Blocking conversion APIs require Config::nowMs. Without it they return
Err::INVALID_CONFIG and do not start a conversion.
```

### 2. Multi-Register Writes Can Leave Hardware And Cache Diverged

Severity: High

Several operations write multiple registers but only update the cache after all
writes succeed.

Examples:

- `setThresholds()` writes low threshold, then high threshold:
  [`src/ADS1115.cpp`](../src/ADS1115.cpp#L688)
- `enableConversionReadyPin()` changes threshold/comparator fields and calls
  `_applyConfig()`:
  [`src/ADS1115.cpp`](../src/ADS1115.cpp#L794)
- `_applyConfig()` writes low threshold, high threshold, then config:
  [`src/ADS1115.cpp`](../src/ADS1115.cpp#L1066)

If a later I2C write fails, earlier hardware writes may have already taken
effect, while the software cache rolls back or remains unchanged.

Impact:

- Hardware and cached configuration can diverge.
- A later `getSettings()` snapshot can report old cached state while the chip is
  partly changed.
- Comparator/ALERT behavior may be surprising after partial failure.

Current mitigation:

- The docs note that `recover()` reapplies cached configuration after partial
  writes.
- Some native tests verify cache rollback behavior.

Why this is still not industry-grade:

- The driver does not mark the hardware state as dirty.
- The failing operation does not clearly tell the caller whether hardware may
  have changed.
- Recovery is manual, so the caller must infer when it is necessary.

Recommended fix options:

1. Add a `_hardwareConfigDirty` flag set when a multi-register operation may
   have partially completed.
2. Expose this in `SettingsSnapshot`.
3. Return a specific status detail or new error code for partial apply failure.
4. Add `syncConfig()` / `recover()` documentation that explicitly clears dirty
   state after successfully reapplying cache.
5. Add tests for partial hardware divergence, not only cache rollback.

### 3. Worst-Case Public API Latency Is Not Documented

Severity: High

The driver is bounded, but several public APIs can perform multiple I2C
transactions. The default I2C timeout is 50 ms:

- [`include/ADS1115/Config.h`](../include/ADS1115/Config.h#L129)

`_applyConfig()` performs three writes:

- Low threshold
- High threshold
- Config register

Reference:

- [`src/ADS1115.cpp`](../src/ADS1115.cpp#L1066)

Therefore setters such as `setGain()`, `setMux()`, `setDataRate()`, `setMode()`,
and comparator setters can block up to roughly:

```text
3 * Config::i2cTimeoutMs
```

`begin()` also performs a probe plus `_applyConfig()`, so its worst case can be
roughly:

```text
1 read timeout + 3 write timeouts
```

Impact:

- The driver is not nonblocking in the strict sense.
- In a real control loop, a 150 ms setter path may be unacceptable.
- Users need explicit latency budgets to integrate safely.

Recommended fix:

- Document worst-case I2C transaction count per public API.
- Add a "latency model" table to README and Doxygen.
- Consider optimized single-register config updates for setters that only need
  to write CONFIG, while preserving threshold writes for comparator-specific
  operations.

Suggested API documentation table:

| API | I2C transactions | Worst-case latency |
| --- | --- | --- |
| `readRegister16()` | 1 write-read | `1 * i2cTimeoutMs` |
| `writeRegister16()` | 1 write | `1 * i2cTimeoutMs` |
| `setGain()` | Currently 3 writes | `3 * i2cTimeoutMs` |
| `setMux()` | Currently 3 writes | `3 * i2cTimeoutMs` |
| `begin()` | 1 read + 3 writes | `4 * i2cTimeoutMs` |
| `recover()` | 1 read + 3 writes | `4 * i2cTimeoutMs` |

## Medium-Severity Findings

### 4. `probe()` Collapses Transport Errors Into `DEVICE_NOT_FOUND`

Severity: Medium

`probe()` reads the CONFIG register using the raw path. If the read fails with a
transport error, it returns `DEVICE_NOT_FOUND`:

- [`src/ADS1115.cpp`](../src/ADS1115.cpp#L241)

Impact:

- Address NACK, bus timeout, arbitration/bus error, and physical absence are
  different operational conditions.
- Collapsing them weakens bring-up and field diagnostics.
- The original detail code remains, but the high-level `Err` code is less
  precise.

Recommended fix:

- Preserve the original I2C error code where practical.
- If `DEVICE_NOT_FOUND` is retained, use it specifically for address NACK or
  transport adapters that can clearly identify absence.
- Add a `probeDetailed()` or document exact mapping.

### 5. Device Presence Check Is Minimal

Severity: Medium

`begin()` accepts a device if CONFIG register read succeeds:

- [`src/ADS1115.cpp`](../src/ADS1115.cpp#L187)
- [`src/ADS1115.cpp`](../src/ADS1115.cpp#L241)

ADS1115 has no ID register, so this limitation is partly inherent. However, the
driver does not read back after `_applyConfig()`.

Impact:

- Any I2C device that ACKs and returns two bytes at pointer `0x01` can pass the
  probe.
- Configuration write success depends entirely on transport status, not a
  read-back check.

Recommended fix:

- Add optional strict initialization:
  - Read CONFIG.
  - Write requested CONFIG.
  - Read CONFIG back.
  - Compare writable fields.
- Keep this optional because read-back costs another transaction and some
  systems may prefer faster startup.

### 6. Continuous-Mode Freshness Contract Is Ambiguous

Severity: Medium

`readConversionReady()` models fresh continuous-mode samples by elapsed
data-rate interval:

- [`src/ADS1115.cpp`](../src/ADS1115.cpp#L393)

But `readRaw()` in continuous mode reads the conversion register immediately,
and `readBlocking()` in continuous mode simply calls `readRaw()`:

- [`src/ADS1115.cpp`](../src/ADS1115.cpp#L444)
- [`src/ADS1115.cpp`](../src/ADS1115.cpp#L497)

Impact:

- It is not obvious whether continuous-mode reads mean "latest register value"
  or "fresh sample since last read".
- Users may assume `readBlocking()` waits for a fresh sample in continuous mode,
  but it does not.

Recommended fix:

- Document continuous-mode semantics explicitly.
- Optionally add separate APIs:
  - `readLatestRaw()`
  - `readFreshRaw()`
  - `waitFreshRaw(timeoutMs)`

### 7. `end()` Silently Ignores Shutdown Write Failure

Severity: Medium

`end()` attempts to write single-shot mode using raw I2C and discards the result:

- [`include/ADS1115/ADS1115.h`](../include/ADS1115/ADS1115.h#L65)
- [`src/ADS1115.cpp`](../src/ADS1115.cpp#L215)

The lifecycle contract requires `void end()`, so this may be intentional.
However, in continuous mode the user has no way to know whether the chip was
actually powered down.

Recommended fix:

- Keep `void end()` for compatibility.
- Add `Status shutdown()` or `Status endWithStatus()` for applications that need
  a verified stop.
- Document that `end()` is best-effort and does not update health.

### 8. Example I2C Adapters Are Demos, Not Production Bus Managers

Severity: Medium

The Arduino adapter ignores `timeoutMs` at the individual callback boundary and
relies on prior global `Wire.setTimeOut()`:

- [`examples/common/I2cTransport.h`](../examples/common/I2cTransport.h#L49)
- [`examples/common/I2cTransport.h`](../examples/common/I2cTransport.h#L108)
- [`examples/common/I2cTransport.h`](../examples/common/I2cTransport.h#L201)

The scanner and recovery utilities mutate global bus state:

- `Wire.end()` / `Wire.begin()` in
  [`examples/common/I2cScanner.h`](../examples/common/I2cScanner.h#L22)
- Scanner changes timeout in
  [`examples/common/I2cScanner.h`](../examples/common/I2cScanner.h#L54)

The ESP-IDF adapter owns a single global transport, creates its own bus/device,
and hardcodes `I2C_NUM_0`:

- [`examples/esp_idf/basic/main/Ads1115IdfI2cTransport.cpp`](../examples/esp_idf/basic/main/Ads1115IdfI2cTransport.cpp#L13)
- [`examples/esp_idf/basic/main/Ads1115IdfI2cTransport.cpp`](../examples/esp_idf/basic/main/Ads1115IdfI2cTransport.cpp#L57)
- [`examples/esp_idf/basic/main/Ads1115IdfI2cTransport.cpp`](../examples/esp_idf/basic/main/Ads1115IdfI2cTransport.cpp#L63)

Impact:

- Good for examples and bring-up.
- Not a production shared-bus integration template.
- Does not demonstrate bus locking or integration with an application-owned I2C
  manager.

Recommended fix:

- Add a `production_bus_manager` example or doc.
- Show explicit external bus ownership.
- Add optional locking hooks or explain that locking belongs in the transport.
- Demonstrate preserving and restoring bus timeout policy around diagnostics.

### 9. ESP-IDF CLI Loop Can Block `tick()`

Severity: Medium

The ESP-IDF example uses `std::fgets()` inside an infinite loop:

- [`examples/esp_idf/basic/main/main.cpp`](../examples/esp_idf/basic/main/main.cpp#L1172)

`device.tick(nowMs())` is called before `fgets()`, but if `fgets()` blocks,
`tick()` does not continue running.

Impact:

- The example is a bring-up CLI, not a production event-loop template.
- Tick-driven conversion waiting is not reliable while the console is idle.

Recommended fix:

- Move CLI input to a separate task.
- Use nonblocking console/VFS polling where available.
- Keep `device.tick()` in a periodic task.
- Document the current example as a diagnostic CLI, not a production scheduler
  model.

### 10. ESP-IDF Build Readiness Is Not Fully Proven In PlatformIO

Severity: Medium

`library.json` advertises both Arduino and ESP-IDF:

- [`library.json`](../library.json#L31)

But `platformio.ini` defines Arduino S2/S3 environments and native tests only:

- [`platformio.ini`](../platformio.ini#L7)
- [`platformio.ini`](../platformio.ini#L32)
- [`platformio.ini`](../platformio.ini#L47)
- [`platformio.ini`](../platformio.ini#L62)

There is a root ESP-IDF component:

- [`CMakeLists.txt`](../CMakeLists.txt#L22)
- [`idf_component.yml`](../idf_component.yml)

And an ESP-IDF example, but the local verification in this review did not run
`idf.py`.

Recommended fix:

- Add CI jobs for:
  - `idf.py -C examples/esp_idf/basic set-target esp32s3 build`
  - `idf.py -C examples/esp_idf/basic set-target esp32s2 build`
- Or add explicit PlatformIO ESP-IDF environments.
- Make README verification commands match CI coverage.

## Low-Severity Findings

### 11. Thread-Safety And ISR Contract Is Implicit

Severity: Low

The driver stores mutable state and has no internal lock. This is normal for
embedded drivers, but the contract should be explicit.

Impact:

- Concurrent calls from multiple tasks can race health state, conversion state,
  and cached configuration.
- ISR use is not safe because public calls can perform blocking I2C.

Recommended documentation:

```text
ADS1115 instances are not thread-safe. Use one task or provide external
serialization around all public methods. Public APIs are not ISR-safe.
Transport callbacks must not call back into the same ADS1115 instance.
```

### 12. Copy/Move Should Be Disabled

Severity: Low

The `ADS1115` class owns runtime state and stores callback/user pointers, but
does not explicitly delete copy/move operations:

- [`include/ADS1115/ADS1115.h`](../include/ADS1115/ADS1115.h#L54)

Impact:

- Accidental copies can duplicate driver state while sharing the same transport
  context.
- Health counters and conversion state can become misleading.

Recommended fix:

```cpp
ADS1115(const ADS1115&) = delete;
ADS1115& operator=(const ADS1115&) = delete;
ADS1115(ADS1115&&) = delete;
ADS1115& operator=(ADS1115&&) = delete;
```

### 13. IDF Example Include Boundary Is Loose

Severity: Low

The IDF example main component exposes the repo root:

- [`examples/esp_idf/basic/main/CMakeLists.txt`](../examples/esp_idf/basic/main/CMakeLists.txt#L1)

Current guard checks prevent Arduino/facade usage in the IDF example, and the
source currently obeys the native-only rule. Still, broad include paths make
accidental cross-example inclusion easier.

Recommended fix:

- Narrow include directories to the library include path and local example main
  directory.
- Keep the guard script as a backstop.

## Tests And Coverage Assessment

The native test suite is meaningful and covers many important contracts:

- Status behavior
- Config defaults
- `begin()` validation
- Failed begin reset behavior
- Probe no-health side effects
- Recover health effects
- Offline latch behavior
- Partial recover failure preservation
- Conversion timing wraparound
- I2C failure propagation
- Signed conversion register reconstruction
- Continuous readiness timing
- ALERT/RDY path avoiding CONFIG polling
- Config bit writes
- PGA alias handling
- Cache rollback on write failures
- Invalid register rejection
- End behavior while offline

Representative test file:

- [`test/test_basic.cpp`](../test/test_basic.cpp)

Coverage still missing for industry-grade confidence:

- Real ADS1115 hardware at addresses 0x48, 0x49, 0x4A, 0x4B
- ALERT/RDY active-low and active-high electrical validation
- Comparator latch behavior on hardware
- NACK and bus timeout injection on ESP-IDF
- Bus stuck recovery behavior with another device holding SDA low
- Shared-bus contention and locking behavior
- Brownout/unplug/replug recovery
- Long-duration continuous sampling soak test
- I2C clock rates beyond the default example rate
- Cross-build verification with pure ESP-IDF CI
- Static analysis and sanitizer-like host checks where applicable

## Documentation Assessment

The README is useful and accurately describes major architecture points:

- Injected I2C transport
- No core Arduino dependency
- Basic Arduino snippet
- API table
- Health behavior
- Example structure
- Verification commands

Important documentation gaps:

- Blocking APIs require a real monotonic `nowMs` hook for time-correct behavior.
- Public API worst-case I2C transaction counts are not listed.
- Multi-register partial write behavior needs a direct warning and recovery
  recipe.
- `end()` is best-effort.
- Thread-safety and ISR-safety are implicit.
- Continuous mode "latest" versus "fresh" sample semantics are ambiguous.
- Example adapters should be labeled clearly as bring-up/demo adapters, not
  production bus-manager templates.

## Packaging And Release Assessment

Good:

- `library.json` is the version source of truth.
- `Version.h` is generated and synchronized.
- `CMakeLists.txt` generates `Version.h` for ESP-IDF builds.
- `library.json` packages only `include` and `src`, keeping examples out of the
  library artifact.
- CI runs PlatformIO Arduino builds, native tests, and guard scripts.

Weak:

- No PlatformIO ESP-IDF environment despite `library.json` advertising
  `espidf`.
- CI does not currently prove `idf.py` example builds.
- Hardware validation blockers are documented but still open:
  [`docs/IDF_PORT_IMPLEMENTATION.md`](IDF_PORT_IMPLEMENTATION.md#L37)

## Industry-Grade Exit Criteria

The library should be considered industry-grade when all of the following are
true:

1. Blocking API timing contract is explicit and enforced.
2. Worst-case latency per public API is documented.
3. Partial multi-register write behavior is tracked, reported, and tested.
4. `probe()` preserves useful transport failure information or documents
   precise mapping.
5. Continuous-mode freshness semantics are documented or split into explicit
   APIs.
6. `end()` best-effort behavior is documented, or a status-returning shutdown
   API exists.
7. Thread-safety and ISR-safety contracts are documented.
8. Copy/move operations are explicitly disabled.
9. ESP-IDF builds run in CI for ESP32-S2 and ESP32-S3.
10. A production-style shared-bus manager example exists.
11. Hardware validation has been completed for all supported ADS1115 addresses.
12. ALERT/RDY active-low and active-high modes are hardware-validated.
13. NACK, timeout, bus error, unplug/replug, and offline recovery paths are
    validated on hardware or an IDF-level I2C test double.
14. Long-duration soak testing has been run in continuous mode and repeated
    single-shot mode.

## Recommended Remediation Plan

### Phase 1: Tighten Core Contracts

Priority: Highest

- Enforce or document `nowMs` requirement for blocking APIs.
- Add explicit latency documentation.
- Delete copy/move constructors and assignment operators.
- Add thread-safety and ISR-safety notes to Doxygen and README.
- Clarify `end()` as best-effort or add a status-returning shutdown API.

### Phase 2: Handle Partial Hardware State

Priority: High

- Add a dirty hardware state flag for failed multi-register apply paths.
- Expose dirty state in `SettingsSnapshot`.
- Clear dirty state only after successful `recover()` or explicit resync.
- Add tests that prove partial hardware writes are visible and reported.

### Phase 3: Improve Diagnostics

Priority: Medium

- Preserve `I2C_TIMEOUT`, `I2C_NACK_ADDR`, and `I2C_BUS` where possible in
  `probe()`.
- Add optional strict read-back verification after begin/recover.
- Add a diagnostic field or method for "last raw transport error".

### Phase 4: Production Integration Examples

Priority: Medium

- Add an ESP-IDF shared-bus manager example.
- Add external locking around I2C callbacks in the example.
- Move IDF CLI input away from the driver tick loop.
- Label current Arduino and IDF examples as bring-up/diagnostic examples.

### Phase 5: Build And Hardware Validation

Priority: Medium

- Add ESP-IDF build jobs to CI.
- Add hardware validation matrix documentation.
- Run address matrix: 0x48, 0x49, 0x4A, 0x4B.
- Run fault matrix: NACK, timeout, bus stuck, unplug/replug, offline recovery.
- Run ALERT/RDY polarity and comparator validation.
- Run soak tests.

## Final Assessment

This codebase has the correct foundation for a production ADS1115 library. The
core I2C abstraction and health model are notably stronger than typical
embedded sensor libraries. The driver is not blocked by a bad architecture.

The work remaining is production discipline:

- enforce timing assumptions,
- make partial failure states explicit,
- document worst-case behavior,
- validate ESP-IDF and hardware fault paths,
- and provide a real shared-bus integration example.

After those are addressed, the library would be credible as an industry-grade
ADS1115 I2C driver for ESP32-S2/S3 Arduino and ESP-IDF consumers.
