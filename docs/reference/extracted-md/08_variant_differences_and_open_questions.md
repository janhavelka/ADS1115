# ADS1115 variants and source caveats

## Family differences

Source: ADS111x datasheet Rev. E, pp. 1, 3, 12, 25-27.

| Device | Inputs | PGA | Comparator / ALERT/RDY | Notes |
| --- | --- | --- | --- | --- |
| ADS1113 | One differential or one single-ended input | No; fixed +/-2.048 V FSR | No | Config register reserves MUX/PGA/comparator fields. |
| ADS1114 | One differential or one single-ended input | Yes | Yes | No MUX; AIN0/AIN1 only. |
| ADS1115 | Four single-ended selections and four differential selections | Yes | Yes | Full MUX support. |

## Datasheet facts used by this repo

| Topic | Datasheet fact |
| --- | --- |
| Primary target | ADS1115. |
| Register width | All accessible registers are 16 bits. |
| Byte order | Big-endian, MSB first. |
| Address range | `0x48` to `0x4B`. |
| Default Config | `0x8583`. |

## Not documented in PDFs

| Missing or ambiguous fact | Source status |
| --- | --- |
| A dedicated ADS1115 device-ID register | The ADS111x register map has no ID register. Presence checks must use I2C acknowledgement or sane register read/write behavior. |
| Clock stretching behavior | The datasheet states ADS111x cannot drive SCL as targets; it does not document clock-stretch timing. |
| Read data from unlisted pointer values | Pointer bits are only `00b` through `11b`; bits 7:2 are write-only reserved and must be zero. |
