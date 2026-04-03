# ADS1115 v0.3.0 Release Notes
Date: 2026-04-03

## Highlights
- Added granular transport status codes so injected I2C adapters can distinguish address NACKs, data NACKs, timeouts, and bus faults.
- Tightened raw transport validation inside the driver to reject invalid buffers before calling application-provided callbacks.
- Standardized the example `Wire` adapter around the shared `i2cUser` / `TwoWire*` pattern used across the I2C library workspace.
- Refreshed README and example-helper documentation to match the real transport contract and shipped docs.

## Compatibility
- Existing public APIs remain intact.
- The `Err` enum is append-only in this release; existing values keep their meaning.

## Tag
- `v0.3.0`

## Suggested GitHub Release Title
- `ADS1115 v0.3.0`
