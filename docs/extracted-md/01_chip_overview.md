# ADS1115 chip overview

The ADS1115 is part of the ADS111x family of 16-bit, low-power, I2C-compatible delta-sigma ADCs with internal reference and oscillator. The ADS1115 adds a four-input multiplexer, programmable gain amplifier, and programmable comparator/ready output.

Source: ADS111x datasheet Rev. E, pp. 1, 12.

## Driver-facing capabilities

| Capability | ADS1115 facts to model | Source |
| --- | --- | --- |
| 16-bit conversion register | Read signed two's-complement ADC codes from pointer `00b`. | Datasheet, p. 24 |
| ADS1115 input MUX | Support four single-ended inputs and four differential selections through `MUX[2:0]`. | Datasheet, pp. 12, 25 |
| Programmable FSR | `PGA[2:0]` selects +/-6.144 V through +/-0.256 V scaling, subject to input absolute limits. | Datasheet, pp. 15, 25-26 |
| Data rates | 8, 16, 32, 64, 128, 250, 475, or 860 SPS. | Datasheet, pp. 5, 15, 26 |
| Single-shot and continuous modes | `MODE` controls power-down/single-shot versus continuous conversion. | Datasheet, pp. 18, 25-26 |
| Comparator / ready output | ADS1114/ADS1115 `ALERT/RDY` can act as comparator or conversion-ready output. | Datasheet, pp. 16-17, 26-27 |
| Four I2C addresses | `ADDR` selects `0x48` to `0x4B`. | Datasheet, p. 20 |

## Implementation facts tied to the datasheet

- All accessible ADS1115 registers are 16 bits and are transferred MSB first.
- The Address Pointer register is write-only; bits 7:2 are reserved and must be written `000000b`.
- ADS1115 `MUX[2:0]`, `PGA[2:0]`, comparator fields, and threshold registers are documented; ADS1113 and ADS1114 reserve or omit some of those fields.
