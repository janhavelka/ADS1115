# ADS1115 pinout and signals

Source: ADS111x datasheet Rev. E, p. 3.

## 10-pin packages

| Pin | ADS1115 name | Type | Driver relevance |
| --- | --- | --- | --- |
| 1 | ADDR | Digital input | I2C target-address select. |
| 2 | ALERT/RDY | Digital output | Open-drain comparator output or conversion-ready signal. |
| 3 | GND | Ground | Device ground. |
| 4 | AIN0 | Analog input | ADC input 0. |
| 5 | AIN1 | Analog input | ADC input 1. |
| 6 | AIN2 | Analog input | ADC input 2. |
| 7 | AIN3 | Analog input | ADC input 3. |
| 8 | VDD | Power | 2.0 V to 5.5 V operating supply; decouple to GND. |
| 9 | SDA | Digital I/O | Open-drain I2C data; pull up to VDD. |
| 10 | SCL | Digital input | I2C clock; pull up to VDD. |

## Address selection

Source: ADS111x datasheet Rev. E, p. 20.

| ADDR connection | 7-bit address |
| --- | --- |
| GND | `0x48` |
| VDD | `0x49` |
| SDA | `0x4A` |
| SCL | `0x4B` |

The datasheet says `ADDR` is sampled continuously. If `ADDR` is tied to SDA, hold SDA low for at least 100 ns after SCL goes low so the address decodes correctly.

## Input signal notes

- ADS1115 supports four single-ended inputs and four differential MUX selections:
  AIN0-AIN1, AIN0-AIN3, AIN1-AIN3, and AIN2-AIN3. The latter three share AIN3
  as the negative input.
- Analog input voltage must remain within the absolute input limits; PGA full-scale range does not override `GND - 0.3 V` to `VDD + 0.3 V` absolute limits.
- If `ALERT/RDY` is unused, leave it unconnected or tie it to VDD with a weak pullup as appropriate for the board.
