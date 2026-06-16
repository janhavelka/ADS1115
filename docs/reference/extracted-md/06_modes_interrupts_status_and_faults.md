# ADS1115 modes, interrupts, status, and faults

## Conversion modes

Source: ADS111x datasheet Rev. E, p. 18.

| Mode | Config | Behavior |
| --- | --- | --- |
| Single-shot / power-down | `MODE=1` | Default. Device powers down after a conversion. Write `OS=1` to start one conversion. |
| Continuous | `MODE=0` | Device continuously converts at the selected data rate. Writing new config starts conversions with the new settings after current conversion behavior completes. |

## Status polling

`OS` in the Config register is both a start bit and a status bit:

- Write `OS=1` to start a single conversion when the device is powered down.
- Read `OS=0` while a conversion is active.
- Read `OS=1` when the device is not converting.

Source: ADS111x datasheet Rev. E, p. 25.

## Comparator and ALERT/RDY

ADS1114 and ADS1115 include a programmable digital comparator. ADS1113 does not.

| Field | Function |
| --- | --- |
| `COMP_MODE` | Traditional comparator or window comparator. |
| `COMP_POL` | Active-low or active-high output. |
| `COMP_LAT` | Nonlatching or latching behavior. |
| `COMP_QUE` | Number of successive threshold events before assertion, or disabled. |
| `Lo_thresh`, `Hi_thresh` | Signed threshold values in conversion-code format. |

The ALERT/RDY pin can also be configured for conversion-ready signaling by
setting Hi_thresh MSB to 1 and Lo_thresh MSB to 0. In continuous conversion the
ready pulse can be approximately 8 us, so polling tasks can miss it unless the
hardware path uses an interrupt-capable input, latching, or OS-bit polling.

Source: ADS111x datasheet Rev. E, pp. 16-17, 26-27.

## Clear behavior

- In latching comparator mode, the asserted ALERT/RDY pin remains latched until conversion data are read or a successful SMBus alert response clears the asserting device.
- In nonlatching mode, the output clears when conversions are back within the selected threshold behavior.
- Default `COMP_QUE=11b` disables the comparator and leaves ALERT/RDY high impedance.
