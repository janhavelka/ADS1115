# ADS1115 I2C Uniformization Prompt

Repository: `ADS1115`

Absolute path: `C:\Users\Honza\Documents\Projects\ADS1115`

## Execution Rules

You are working inside this single repository. Implement this prompt directly;
do not repeat the cross-repository audit.

You may spawn subagents for read-only inspection of APIs, tests, I2C
transactions, docs, and diagnostics. Keep final judgment, edits, and
verification in the main agent.

Prefer simple, robust, readable code. Before adding code, inspect whether
existing code can be simplified, reused, tightened, or deleted.

Preserve dirty user changes. Do not commit unless explicitly asked.

## Common Uniformization Target

Apply this shared I2C library contract: injected non-owning transport, `Status` returns, cache-only `getSettings(SettingsSnapshot&) const`, active `probe()`/diagnostics named explicitly, `DriverState` with `state()` and `driverState()`, `isOnline()`, `lastOkMs()`, `lastErrorMs()`, `lastError()`, `consecutiveFailures()`, `totalFailures()`, and `totalSuccess()`.

Keep the common `Err` vocabulary append-only where missing: `OK`, `NOT_INITIALIZED`, `INVALID_CONFIG`, `INVALID_PARAM`, `I2C_ERROR`, `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, `I2C_BUS`, `DEVICE_NOT_FOUND`, `TIMEOUT`, `BUSY`, and `IN_PROGRESS`. Preserve ADS1115-specific conversion/readback/dirty-state codes.

Uniformization is not a new base class or framework. Make only local, source-compatible additions and tests.

## Current State

- Public lifecycle and health live in `include\ADS1115\ADS1115.h`: `DriverState` at line 16, `Status begin(const Config&)` at line 116, `probe()` at line 155, `recover()` at line 160, `getSettings(SettingsSnapshot&)` at line 165, `state()` at line 169, `lastOkMs()` through `totalSuccess()` at lines 178-198.
- `SettingsSnapshot` includes `hardwareConfigDirty` and `hardwareConfigDirtyError` at `include\ADS1115\ADS1115.h:49-61`.
- Raw and tracked register helpers are exposed as `readRegister16()`, `writeRegister16()`, and compatibility aliases at `include\ADS1115\ADS1115.h:376-405`.
- Implementation separates raw and tracked I2C in `src\ADS1115.cpp:1349-1468`; `_updateHealth()` is in `src\ADS1115.cpp:1480`.
- ADS1115 has no chip-ID register. `probe()` can only prove plausible I2C/register access, not true identity.
- HIL support is a capture script, `tools\hil_ads1115_capture.py`, not a standard `tools\run_i2c_hil.py` runner.

## Best Sources To Adapt

- Use BME280 dirty register-write documentation and behavior as the closest match: `C:\Users\Honza\Documents\Projects\BME280\include\BME280\BME280.h:549-585` and `src\BME280.cpp:1595-1605`.
- Use SHT3x health naming for the missing cross-driver alias: `C:\Users\Honza\Documents\Projects\SHT3x-main\include\SHT3x\SHT3x.h:227-230`.
- Use BME280 or SHT3x HIL runner style only if ADS1115 already has a maintained serial CLI command surface: `BME280\tools\run_i2c_hil.py` and `SHT3x-main\tools\run_i2c_hil.py`.

## Implementation Tasks

1. Add a non-breaking `DriverState driverState() const { return state(); }` alias next to `state()` in `include\ADS1115\ADS1115.h`.
   Preserve existing compatibility aliases; do not remove or rename public APIs to achieve uniform naming.
2. Keep `probe()` diagnostic-only and raw/no-health. Do not make it look like device identity proof; document that it is config-register plausibility for ADS1115.
3. Keep `readRegister16()`/`writeRegister16()` as diagnostic helpers. Verify failed writes that may have reached hardware still set `hardwareConfigDirty()` with the original transport status.
4. Audit every wait/poll path, especially `readBlocking()`, for finite timeout bounds and visible `TIMEOUT`/transport status returns.
5. If a maintained ADS1115 CLI already exists, add `tools\run_i2c_hil.py` by adapting the BME280/SHT3x runner shape. Commands should cover the common minimum contract: `version`, `scan`, `probe`, `settings`, `health`, failure-token classification, dry-run/parser test support, and one bounded conversion. If no CLI exists, do not create a fake runner; update README to state that only `tools\hil_ads1115_capture.py` exists.
6. Add or update tests for the `driverState()` alias and for `probe()` not changing `totalSuccess()`, `totalFailures()`, `consecutiveFailures()`, or `lastError()`.

## API Changes Required

- Add only the `driverState()` alias unless the HIL runner requires no library API changes. Do not rename existing APIs.

## Simplifications Before Adding Code

- Check whether any old health alias or duplicate dirty-state helper can be removed from examples only. Do not remove public compatibility aliases.

## Tests To Add Or Update

- Native test: `driverState()` returns the same value as `state()`.
- Native test: `getSettings(SettingsSnapshot&) const` is bus-silent.
- Native fault-injection test: `probe()` transport failure does not update health counters.
- HIL parser tests only if a real runner is added.

## Commands To Run

- `pio test -e native`
- `pio run -e esp32s3dev`
- If HIL runner is added: `python tools\run_i2c_hil.py --dry-run` and a parser-only host test.
- Do not claim live HIL unless a serial target is connected and the full command runs.

## Constraints And Non-Goals

- Do not add a global I2C bus owner, Wire dependency, bus reset policy, or hidden retry loop.
- Do not add hidden retries inside normal operations; recovery must remain explicit and application-scheduled.
- Preserve distinct timeout, address NACK/device-not-found, data NACK, bus, readback, and dirty-state statuses. Do not collapse them into generic `I2C_ERROR` or use `DEVICE_NOT_FOUND` for timeout/data/bus failures.
- Do not claim ADS1115 identity from ACK alone.
- Do not add placeholder APIs for future schedulers or managers.

## Risks And Open Questions

- Open: whether ADS1115 should have the same full HIL runner contract as BME280/SHT3x, given lack of chip ID and analog-input fixture requirements.
