# ADS1115 register map

Source: ADS111x datasheet Rev. E, pp. 24-27.

| Pointer | Register | Access | Reset | Driver notes |
| --- | --- | --- | --- | --- |
| `00b` | Conversion | R | `0x0000` | Last conversion result, signed two's-complement; remains reset value until the first conversion completes. |
| `01b` | Config | R/W | `0x8583` | OS/status, MUX, PGA, mode, data rate, comparator config. |
| `10b` | Lo_thresh | R/W | `0x8000` | Comparator low threshold; ADS1114/ADS1115 only. |
| `11b` | Hi_thresh | R/W | `0x7FFF` | Comparator high threshold; ADS1114/ADS1115 only. |

## ADS1115 Config register fields

| Bits | Field | Reset | Meaning |
| --- | --- | --- | --- |
| 15 | `OS` | 1 | Write 1 to start a single conversion when powered down; read 0 while converting and 1 when idle/ready. |
| 14:12 | `MUX` | 000b | ADS1115 input mux selection. |
| 11:9 | `PGA` | 010b | Full-scale range; default +/-2.048 V. |
| 8 | `MODE` | 1 | 0 continuous, 1 single-shot/power-down. |
| 7:5 | `DR` | 100b | Data rate; default 128 SPS. |
| 4 | `COMP_MODE` | 0 | Traditional or window comparator. |
| 3 | `COMP_POL` | 0 | ALERT/RDY polarity; default active low. |
| 2 | `COMP_LAT` | 0 | Nonlatching or latching comparator. |
| 1:0 | `COMP_QUE` | 11b | Comparator queue; `11b` disables comparator and makes ALERT/RDY high impedance. |

## ADS1115 MUX settings

Source: ADS111x datasheet Rev. E, p. 25.

| `MUX[2:0]` | Input selection |
| --- | --- |
| `000b` | AIN0 - AIN1 |
| `001b` | AIN0 - AIN3 |
| `010b` | AIN1 - AIN3 |
| `011b` | AIN2 - AIN3 |
| `100b` | AIN0 - GND |
| `101b` | AIN1 - GND |
| `110b` | AIN2 - GND |
| `111b` | AIN3 - GND |

## Threshold registers

The comparator thresholds are signed two's-complement values in the same code
format as the conversion register. When PGA settings change, threshold codes
must be recalculated. Conversion-ready mode is enabled by setting the
Hi_thresh MSB to 1 and Lo_thresh MSB to 0, with `COMP_QUE` set to an enabled
value instead of `11b`. `COMP_POL` still controls ready-pulse polarity;
`COMP_MODE` and `COMP_LAT` do not control the ready-pulse behavior.

Source: ADS111x datasheet Rev. E, p. 27.

## Documented reserved-bit behavior

| Device/register area | Reserved bits | Datasheet behavior |
| --- | --- | --- |
| Address Pointer register | Bits 7:2 | Always write `000000b`; pointer value is bits 1:0. |
| ADS1113 Config | Bits 14:9 and 4:0 | Reserved fields shown with reset values; ADS1113 has fixed input/PGA and no comparator. |
| ADS1114 Config | Bits 14:12 | Reserved; ADS1114 has no MUX and always uses AIN0/AIN1. |
| ADS1113 threshold registers | `Lo_thresh`, `Hi_thresh` | Threshold registers are not available in ADS1113. |

Source: ADS111x datasheet Rev. E, pp. 24-27.
