# AGENTS.md - ADS1115 Production Embedded Guidelines

## PlatformIO

Before editing, fetch remotes and fast-forward the newest intended working
branch to its upstream. Stop and report dirty, divergent, or conflicted state;
never overwrite work to force a sync.

On Windows, use `.\scripts\pio.cmd <arguments>`; it selects the current user's
VS Code-managed installation. Never install another PlatformIO Core; if the
wrapper cannot find it, stop and report the missing installation.

## Role and Target
You are a professional embedded software engineer building a production-grade ADS1115 16-bit ADC library.

- Target: ESP32-S2 / ESP32-S3, Arduino and ESP-IDF consumers, PlatformIO/ESP-IDF.
- Goals: deterministic behavior, long-term stability, clean API contracts, portability, no surprises in the field.
- These rules are binding.

## Chunked Hardening Workflow

- Work chunk-by-chunk; do not perform broad refactors during hardening prompts.
- Keep implementation changes scoped to the current prompt and the existing
  architecture unless the prompt explicitly authorizes a wider change.
- Prefer simplicity, clarity, correctness, robustness, safety, and readability
  over clever abstractions or speculative flexibility.
- Before coding, inspect whether existing code can be simplified, reused, or
  deleted.
- Prefer deleting unnecessary code over adding new code.
- Prefer extending existing owners, modules, APIs, and contracts over creating
  parallel abstractions.
- Before adding a new service, class, file, interface, or abstraction, check
  whether an existing owner or module is the correct home.
- Add abstractions only for a concrete current need with a clear caller or test.
- Do not add placeholder classes, future stubs, empty managers, broad
  frameworks, plugin systems, registries, generic layers, or speculative
  extension points unless the current task explicitly requires them.
- Preserve dirty user changes; never revert unrelated work.
- Preserve the framework-neutral core in `include/` and `src/`.
- No Arduino, Wire, ESP-IDF, FreeRTOS, logging framework, global bus, pin
  ownership, task, or framework delay dependencies may be introduced into the
  core library.
- Core I2C stays injected and non-owning. Bus handles, pins, clock rate,
  timeout policy, locking, and recovery policy stay in application or adapter
  code.
- Public fallible APIs must return meaningful `Status`; when changing or adding
  production APIs, do not hide transport errors behind bool-only or void APIs.
- Do not reorder existing status enum values unless the compatibility impact is
  explicitly documented. Prefer appending new status codes.
- ADS1115 has no chip-ID register. Strict init and read-back are only
  plausibility/read-back verification, not identity verification.
- Multi-register writes can partially reach hardware. Dirty or partial
  hardware-state diagnostics must be explicit.
- Raw diagnostic register writes must either update the cache safely or mark
  cache/hardware state dirty.
- Public APIs are not ISR-safe, and driver instances are not internally
  thread-safe unless explicitly proven, documented, and tested.
- Hardware validation claims require dated logs or captures.
- CI/build claims require actual command output or CI configuration evidence.
- Each hardening prompt must end with a commit and push/sync.

---

## Repository Model (Single Library)

```
include/ADS1115/         - Public API headers only (Doxygen)
  CommandTable.h         - Register addresses and bit masks
  Status.h
  Config.h
  ADS1115.h
  Version.h              - Auto-generated (do not edit)
src/                     - Implementation (.cpp)
examples/
  01_basic_bringup_cli/  - Arduino diagnostic CLI
  02_owner_safe_poll/    - Production owner-loop pattern
  common/                - Live Arduino example helpers only
  esp_idf/basic/         - Native ESP-IDF diagnostic example
platformio.ini
library.json
README.md
CHANGELOG.md
AGENTS.md
```

Rules:
- `examples/common/` is NOT part of the library. It simulates project glue and keeps examples self-contained.
- No board-specific pins/bus in library code; only in `Config`.
- Public headers only in `include/ADS1115/`.
- Examples demonstrate usage and may use `examples/common/BoardConfig.h`.
- Arduino examples may use Arduino APIs and `examples/common/` helpers.
- ESP-IDF examples must be native IDF code. They must use `app_main`,
  `driver/i2c_master.h`, `esp_timer`, FreeRTOS delays, IDF GPIO, and fixed C
  buffers or native console APIs. They must not include Arduino example sources
  or use `Arduino.h`, `Wire.h`, `String`, `Serial`, `TwoWire`,
  `ArduinoCompat`, or `IdfArduinoCompat` facades.
- Keep the layout boring and predictable.

---

## Core Engineering Rules (Mandatory)

- Deterministic: no unbounded loops/waits; all timeouts via deadlines, never `delay()` in library code.
- No unbounded waits, retries, loops, allocations, queues, or buffers in steady
  paths.
- Production lifecycle: bus-silent `bind()` / `start*()`, owner-driven
  `poll(uint32_t nowMs, uint8_t maxTransactions)`, exactly-once
  `takeResult()`, and bus-silent `unbind()`.
- Split multi-transfer work into bounded state-machine steps driven by the
  serialized owner's `poll()` calls. Compatibility facades may be synchronous
  only when their documented transfer count and timeout remain bounded.
- No heap allocation in steady state (no `String`, `std::vector`, `new` in normal ops).
- Avoid dynamic allocation in steady embedded paths unless it is already an
  accepted local pattern and the bound is clear.
- No logging in library code; examples may log.
- No macros for constants; use `static constexpr`. Macros only for conditional compile or logging helpers.
- Every hardware operation that can block must have a timeout and an observable
  failure path.
- Recovery logic must be bounded, deterministic, and testable.
- Prefer explicit state, explicit ownership, and small local helpers over hidden
  global state.
- Do not hide hardware failures behind silent retries or fake success.
- Public/core library headers and `src/` must not require Arduino or ESP-IDF
  framework headers unless a platform-specific adapter is explicitly documented.
- The core library is not thread-safe and is not ISR-safe. Applications must
  externally serialize all calls into a driver instance and must not call public
  APIs from interrupt context.
- Public fallible APIs must return `Status`; silent failure is unacceptable.
- Multi-register hardware updates must be explicit about partial hardware state.
  If a later transaction fails after an earlier register write may have reached
  the chip, the driver must preserve the original transport error, expose a
  hardware-config-dirty diagnostic, and clear it only after a successful full
  resync/recover path.

---

## I2C Manager + Transport (Required)

- The I2C bus must have one clear owner.
- The library MUST NOT own I2C. It never touches `Wire` directly.
- `Config` MUST accept a transport adapter (function pointers or abstract interface).
- I2C transactions must be timeout-bounded and report errors clearly.
- Transport errors MUST map to `Status` (no leaking `Wire`, `esp_err_t`, etc.).
- The library MUST NOT configure bus timeouts or pins.
- The library MUST remain transport-injected and non-owning of the I2C bus.
  Bus handles, pins, clock rate, timeout policy, bus recovery, and locking
  belong to the application or example adapter, not the core library.
- Device drivers must not directly own or reconfigure a shared bus unless this
  repository's architecture explicitly says so.
- Keep chip-level protocol code inside the driver or wrapper. Keep application
  policy outside the chip driver.
- Do not implement chip protocols manually if an existing hardened project
  library already provides the needed timeout, recovery, and testability
  behavior.
- Do not add fake devices, simulated buses, or test doubles to production paths.
- Example adapters that are diagnostic-only must say so. Production examples
  must demonstrate shared-bus ownership, external serialization/locking,
  timeout policy, and bounded owner `poll()` scheduling.
- ESP-IDF adapters/examples own IDF bus handles and map `esp_err_t` to
  `Status`; the core must never expose or store IDF handles.

---

## Status / Error Handling (Mandatory)

All fallible APIs return `Status`:

```cpp
struct Status {
  Err code;
  int32_t detail;
  const char* msg;  // static string only
};
```

- Silent failure is unacceptable.
- No exceptions.

---

## ADS1115 Driver Requirements

- I2C address configurable: 0x48 (ADDR->GND), 0x49 (ADDR->VDD), 0x4A (ADDR->SDA), 0x4B (ADDR->SCL).
- Initialization must check reachability by reading the CONFIG register and
  must verify the complete writable profile. ADS1115 has no identity register.
- Support input multiplexer configurations:
  - 4 single-ended input selections (AIN0-AIN3 vs GND)
  - 4 differential MUX selections (AIN0-AIN1, AIN0-AIN3, AIN1-AIN3, AIN2-AIN3)
- Configurable PGA (gain): +/-6.144V, +/-4.096V, +/-2.048V, +/-1.024V, +/-0.512V, +/-0.256V
- Configurable data rate: 8, 16, 32, 64, 128, 250, 475, 860 SPS
- Support operating modes:
  - **Single-shot mode**: One conversion on demand, then power down
  - **Continuous mode**: Continuous conversions at configured data rate
- Comparator support (optional): window/traditional mode, latching, active high/low, queue depth
- Conversion ready detection via:
  - Polling OS bit in config register
  - ALERT/RDY pin (comparator mode with thresholds set appropriately)
- Proper 16-bit signed result handling (two's complement)
- Voltage calculation from raw ADC value and gain setting

---

## Driver Architecture: Owner-Safe Managed Driver

The production path follows a fixed-memory, owner-polled model with health
tracking. The compatibility path retains bounded synchronous facades:

- `start*()`, cancellation, result consumption, `bind()`, and `unbind()` are
  bus-silent. Only owner-authorized `poll()` performs I2C for production
  operations.
- Individual transport callbacks may block only within their supplied timeout;
  an owner operation is split across bounded `poll()` steps.
- `tick()` and synchronous calls remain compatibility/diagnostic surfaces, not
  the production shared-bus lifecycle.
- Health is tracked via **tracked transport wrappers** -- public API never calls `_updateHealth()` directly.
- Recovery is application-controlled through `startRecover()` / `poll()`;
  `recover()` is the bounded synchronous compatibility facade.

### DriverState (4 states only)

```cpp
enum class DriverState : uint8_t {
  UNINIT,    // No successful initialization; also after end()/unbind()
  READY,     // Operational, consecutiveFailures == 0
  DEGRADED,  // 1 <= consecutiveFailures < offlineThreshold
  OFFLINE    // consecutiveFailures >= offlineThreshold
};
```

State transitions:
- `bind()` -> UNINIT with configuration state `UNCONFIGURED` and no I2C
- Successful owner initialization or compatibility `begin()` -> READY
- Any tracked I2C failure after initialization in READY -> DEGRADED
- Successful tracked I2C in DEGRADED/OFFLINE -> READY
- Failures reach `offlineThreshold` -> OFFLINE
- `unbind()` / `end()` -> UNINIT

### Transport Wrapper Architecture

All I2C goes through layered wrappers:

```
Owner `poll()` steps and compatibility/diagnostic I2C APIs
    ↓
Register helpers (_readRegister16Tracked, _writeRegister16Tracked)
    ↓
TRACKED wrappers (_i2cWriteReadTracked, _i2cWriteTracked)
    ↓  <- _updateHealth() called here ONLY
RAW wrappers (_i2cWriteReadRaw, _i2cWriteRaw)
    ↓
Injected transport callbacks (DriverConfig/Config)
```

**Rules:**
- Public API methods NEVER call `_updateHealth()` directly
- Tracked register helpers use TRACKED wrappers -> health updated automatically
- `probe()` uses RAW wrappers -> no health tracking (diagnostic only)
- Owner and compatibility recovery use a tracked CONFIG read for their probe
  step, so failures count after initialization

### Health Tracking Rules

- `_updateHealth()` called ONLY inside tracked transport wrappers.
- State transitions are guarded by `_initialized` (no DEGRADED/OFFLINE before
  successful owner initialization or compatibility `begin()`).
- NOT called for config/param validation errors (INVALID_CONFIG, INVALID_PARAM).
- NOT called for precondition errors (NOT_INITIALIZED).
- `probe()` uses raw I2C and does NOT update health (diagnostic only).

### Health Tracking Fields

- `_lastOkMs` - timestamp of last successful I2C operation
- `_lastErrorMs` - timestamp of last failed I2C operation
- `_lastError` - most recent error Status
- `_consecutiveFailures` - failures since last success (resets on success)
- `_totalFailures` / `_totalSuccess` - lifetime counters (wrap at max)

---

## Hardening Subagent Roles

When using subagents for hardening work, split responsibilities as follows:

- **Core Contracts Agent**: public API compatibility, framework neutrality,
  transport injection, timeout semantics, strict init/read-back behavior,
  dirty-state tracking, shutdown semantics, and copy/move prevention.
- **Tests/Fault Injection Agent**: native fake-transport tests for missing
  clocks, partial write failures, dirty-state clearing, probe error mapping,
  strict read-back masking, continuous-mode semantics, shutdown behavior, and
  compile-time copy/move prevention.
- **Docs/Examples/CI Agent**: README/Doxygen/API latency documentation,
  example honesty, ALERT/RDY/PGA/comparator warnings, hardware validation
  matrix, PlatformIO/ESP-IDF build coverage, and CI command accuracy.
- **Final Integration Review Agent**: verify all changes together, check that
  guard scripts and builds match claimed results, identify remaining hardware
  validation gaps, and ensure no unrelated churn was introduced.

---

## Versioning and Releases

Single source of truth: `library.json`. `Version.h` is auto-generated and must never be edited.

SemVer:
- MAJOR: breaking API/Config/enum changes.
- MINOR: new backward-compatible features or error codes (append only).
- PATCH: bug fixes, refactors, docs.

Release steps:
1. Update `library.json`.
2. Update `CHANGELOG.md` (Added/Changed/Fixed/Removed).
3. Update `README.md` if API or examples changed.
4. Commit and tag: `Release vX.Y.Z`.

---

## Naming Conventions

- Member variables: `_camelCase`
- Methods/Functions: `camelCase`
- Constants: `CAPS_CASE`
- Enum values: `CAPS_CASE` or short forms (e.g., `AIN0_GND`)
- Locals/params: `camelCase`
- Config fields: `camelCase`
