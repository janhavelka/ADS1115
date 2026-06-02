# ADS1115 electrical and timing notes

## Operating limits relevant to software

| Parameter | Value | Source |
| --- | --- | --- |
| Recommended VDD | 2.0 V to 5.5 V | Datasheet, p. 5 |
| Absolute VDD | -0.3 V to 7 V | Datasheet, p. 4 |
| Analog input absolute range | GND - 0.3 V to VDD + 0.3 V | Datasheet, p. 4 |
| Continuous input current, non-supply pins | -10 mA to +10 mA | Datasheet, p. 4 |
| Data rates | 8 to 860 SPS | Datasheet, p. 5 |
| I2C standard mode | Up to 100 kHz | Datasheet, p. 20 |
| I2C fast mode | Up to 400 kHz | Datasheet, p. 20 |
| I2C high-speed mode | Up to 3.4 MHz per I2C high-speed protocol | Datasheet, pp. 6, 20 |

## Full-scale range and LSB

Source: ADS111x datasheet Rev. E, p. 15.

| `PGA[2:0]` | FSR | LSB |
| --- | --- | --- |
| `000b` | +/-6.144 V | 187.5 uV |
| `001b` | +/-4.096 V | 125 uV |
| `010b` | +/-2.048 V | 62.5 uV |
| `011b` | +/-1.024 V | 31.25 uV |
| `100b` | +/-0.512 V | 15.625 uV |
| `101b`, `110b`, `111b` | +/-0.256 V | 7.8125 uV |

The ADS1113 FSR is fixed at +/-2.048 V. ADS1114 and ADS1115 use the PGA field.

## Data-rate settings

Source: ADS111x datasheet Rev. E, p. 26.

| `DR[2:0]` | Rate |
| --- | --- |
| `000b` | 8 SPS |
| `001b` | 16 SPS |
| `010b` | 32 SPS |
| `011b` | 64 SPS |
| `100b` | 128 SPS |
| `101b` | 250 SPS |
| `110b` | 475 SPS |
| `111b` | 860 SPS |

## Timing implications

- In single-shot mode, write `OS=1` to start a conversion, then poll `OS` or wait based on data rate.
- At 860 SPS, a conversion takes about 1.2 ms; lower rates take longer.
- High-speed I2C requires the controller-code entry sequence `00001XXXb` before transfers at up to 3.4 MHz.
