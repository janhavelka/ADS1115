# ADS1115 protocol, commands, and transactions

ADS1115 uses an I2C target address byte, an 8-bit write-only Address Pointer register, and 16-bit register data. It supports standard mode up to 100 kHz, fast mode up to 400 kHz, and high-speed mode up to 3.4 MHz. Register data is transferred MSB first.

Source: ADS111x datasheet Rev. E, pp. 20-22, 24.

## Address byte

The 7-bit address is selected by `ADDR`: `0x48` for GND, `0x49` for VDD, `0x4A` for SDA, and `0x4B` for SCL. The datasheet says `ADDR` is sampled continuously. If SDA is used as the address strap, hold SDA low for at least 100 ns after SCL goes low so the device decodes the address correctly.

Source: ADS111x datasheet Rev. E, p. 20.

## Address pointer

| Pointer bits | Register |
| --- | --- |
| `00b` | Conversion |
| `01b` | Config |
| `10b` | Lo_thresh |
| `11b` | Hi_thresh |

Bits 7:2 of the pointer byte are reserved; the datasheet field description is "Always write 000000b".

## Register access model

| Operation | Bus sequence |
| --- | --- |
| Set pointer | START, address+W, pointer, STOP or repeated START |
| Write register | START, address+W, pointer, data MSB, data LSB, STOP |
| Read current pointer | START, address+R, data MSB, data LSB, STOP |
| Read specific register | START, address+W, pointer, repeated START, address+R, data MSB, data LSB, STOP |

## General-call reset

The device responds to I2C general call address `0000000b` with command byte `0x06`. This performs an internal reset and returns the device to power-down state with default register settings.

Source: ADS111x datasheet Rev. E, pp. 18, 20.

## High-speed I2C

High-speed mode entry is part of the ADS1115 bus protocol:

- The controller sends START plus high-speed controller code `00001XXXb`.
- ADS1115 does not acknowledge that controller-code byte; the I2C specification prohibits acknowledging it.
- After receiving the controller code, ADS1115 switches on high-speed filters and communicates at up to 3.4 MHz.
- ADS1115 leaves high-speed mode at the next STOP condition.

Source: ADS111x datasheet Rev. E, p. 20.

## SMBus alert response

ADS1114 and ADS1115 can respond to the SMBus alert response when `ALERT/RDY`
is asserted by the comparator. In latching comparator mode, the lowest-address
asserting device wins arbitration and a successful alert response clears the
latch. Do not treat SMBus alert response as the normal conversion-ready clear
path.

Source: ADS111x datasheet Rev. E, pp. 16-17.

## Register transaction facts

- Register data is always 16 bits.
- The pointer is sticky, so repeated reads can omit rewriting the pointer if the desired register has not changed.
- Every register write operation includes the pointer byte and two data bytes.
