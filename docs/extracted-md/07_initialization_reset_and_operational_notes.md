# ADS1115 initialization, reset, and operational notes

## Reset and startup

On power-up, the ADS111x reset the Config register to defaults and enter power-down state. The device also responds to I2C general-call reset command `0x06`, which performs an internal reset.

Source: ADS111x datasheet Rev. E, pp. 18, 20.

## Practical single-shot read sequence

1. Select the 7-bit address from `ADDR`.
2. Build the Config word with desired `MUX`, `PGA`, `DR`, `MODE=1`, comparator settings, and `OS=1`.
3. Write pointer `01b` and the Config word.
4. Wait for conversion time or poll Config until `OS=1`.
5. Write pointer `00b`.
6. Read the 16-bit Conversion register.
7. Convert signed code to volts with the selected LSB.

## Practical continuous-read sequence

1. Write Config with desired `MUX`, `PGA`, `DR`, and `MODE=0`.
2. Set pointer to Conversion register.
3. Read conversion samples at or below the selected data rate.
4. If changing MUX or PGA, write a new Config value and allow a new conversion period before trusting the result.

## Operational cautions

- Full-scale range is ADC scaling, not permission to exceed analog input absolute limits.
- Single-ended readings use only the positive half of the signed output code range.
- ADS1115 input overdrive can affect conversions on other inputs; protect or clamp inputs if overdrive is possible.
- Wait about 50 us after VDD is stable before communicating with the device.

Source: ADS111x datasheet Rev. E, pp. 15, 28, 37.
