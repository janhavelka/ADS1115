# ADS111x Ultra-Small, Low-Power, I2 C-Compatible, 860SPS, 16-Bit ADCs with Internal Reference, Oscillator, and Programmable Comparator datasheet (Rev. E)

- Source PDF: `ADS111x_datasheet_revE.pdf`
- Extraction tool: pdfplumber
- Page count: 56
- SHA256: `f305c7e7d003f476d1503e1542370486b50c7a7ef2924c589e6852e4b8a2e9b0`

## Page 1

ADS111x Ultra-Small, Low-Power, I2C-Compatible, 860SPS, 16-Bit ADCs
with Internal Reference, Oscillator, and Programmable Comparator
1 Features 3 Description
• Ultra-small packages: The ADS1113, ADS1114, and ADS1115 (ADS111x)
– X2QFN: 2mm × 1.5mm × 0.4mm are precision, low-power, 16-bit, I2C-compatible,
– SOT: 2.9mm × 2.8mm × 0.6mm analog-to-digital converters (ADCs) offered in a
• Wide supply range: 2.0V to 5.5V leadless X2QFN-10, a SOT-10 package, and
• Low current consumption: 150μA a VSSOP-10 package. The ADS111x devices
(continuous-conversion mode) incorporate a low-drift voltage reference and
• Programmable data rate: an oscillator. The ADS1114 and ADS1115 also
8SPS to 860SPS incorporate a programmable gain amplifier (PGA)
• Single-cycle settling and a digital comparator. These features, along
• Internal low-drift voltage reference with a wide operating supply range, are useful for
• Internal oscillator power- and space-constrained, sensor measurement
• I2C interface: four pin-selectable addresses applications.
• Operating temperature range:
The ADS111x devices perform conversions at data
–40°C to +125°C
rates of up to 860 samples per second (SPS).
• Family of devices:
The PGA offers input ranges from ±256mV to
– ADS1113: one single-ended (SE) or differential ±6.144V, allowing precise large- and small-signal
(DE) input measurements. The ADS1115 features an input
– ADS1114: one single-ended or differential input multiplexer (MUX) that allows two differential or
with comparator and PGA four single-ended input measurements. Use the
– ADS1115: four single-ended or two differential digital comparator in the ADS1114 and ADS1115 for
inputs with comparator and PGA undervoltage and overvoltage detection.
2 Applications Package Information
PART NUMBER PACKAGE(1) PACKAGE SIZE(2)
• Portable instrumentation
• Battery voltage and current monitoring RUG (X2QFN, 10) 1.50mm × 2.00mm
• Temperature measurement systems ADS111x DYN (SOT, 10) 2.90mm × 2.80mm
• Consumer electronics DGS (VSSOP, 10) 3.00mm × 4.90mm
• Factory automation and process control
(1) For more information, see Section 14.
(2) The package size (length × width) is a nominal value and
includes pins, where applicable.
Device Information
PART NUMBER INPUT CHANNELS FEATURES(1)
ADS1113 1 DE (1 SE) —
ADS1114 1 DE (1 SE) PGA, comparator
ADS1115 2 DE (4 SE) PGA, comparator
(1) See the Device Comparison Table for details.
VDD
Voltage
Reference
ADDR
AIN0 16-Bit I2C Interface SCL AIN1
SDA
Oscillator
ADS1113
GND
(cid:1) (cid:0)
Comparator
ALERT/
RDY
ADDR AIN0
AIN0 16-Bit ADC AIN1 PGA Inte I2 r C face SCL A A I I N N 1 2 MUX
SDA
AIN3
Oscillator
ADS1114 ADS1115
(cid:2) (cid:3)
Comparator
Voltage ALERT/
Reference RDY
ADDR
16-Bit ADC PGA Inte I2 r C face SCL
SDA
Oscillator
(cid:4) (cid:5)
ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024
VDD VDD
Voltage
Reference
ADC
GND GND
Simplified Block Diagrams
An IMPORTANT NOTICE at the end of this data sheet addresses availability, warranty, changes, use in safety-critical applications,
intellectual property matters and other important disclaimers. PRODUCTION DATA.

## Page 2

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
Table of Contents
1 Features............................................................................1 8.1 Register Map.............................................................24
2 Applications..................................................................... 1 9 Application and Implementation.................................. 28
3 Description.......................................................................1 9.1 Application Information............................................. 28
4 Pin Configuration and Functions...................................3 9.2 Typical Application.................................................... 33
5 Specifications.................................................................. 4 10 Power Supply Recommendations..............................37
5.1 Absolute Maximum Ratings........................................ 4 10.1 Power-Supply Sequencing......................................37
5.2 ESD Ratings............................................................... 4 10.2 Power-Supply Decoupling.......................................37
5.3 Recommended Operating Conditions.........................4 11 Layout........................................................................... 38
5.4 Thermal Information....................................................4 11.1 Layout Guidelines................................................... 38
5.5 Electrical Characteristics.............................................5 11.2 Layout Example...................................................... 39
5.6 Timing Requirements: I2C...........................................6 12 Device and Documentation Support..........................40
5.7 Typical Characteristics................................................ 7 12.1 Documentation Support.......................................... 40
6 Parameter Measurement Information.......................... 11 12.2 Receiving Notification of Documentation Updates..40
6.1 Noise Performance................................................... 11 12.3 Support Resources................................................. 40
7 Detailed Description......................................................12 12.4 Trademarks.............................................................40
7.1 Overview................................................................... 12 12.5 Electrostatic Discharge Caution..............................40
7.2 Functional Block Diagrams....................................... 12 12.6 Glossary..................................................................40
7.3 Feature Description...................................................13 13 Revision History.......................................................... 41
7.4 Device Functional Modes..........................................18 14 Mechanical, Packaging, and Orderable
7.5 Programming............................................................ 19 Information.................................................................... 41
8 Registers........................................................................ 24
2 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 3

Device Comparison Table
MAXIMUM SAMPLE INPUT CHANNELS
RESOLUTION SPECIAL
DEVICE RATE Differential PGA INTERFACE
(Bits) FEATURES
(SPS) (Single-Ended)
ADS1115 16 860 2 (4) Yes I2C Comparator
ADS1114 16 860 1 (1) Yes I2C Comparator
ADS1113 16 860 1(1) No I2C None
ADS1015 12 3300 2 (4) Yes I2C Comparator
ADS1014 12 3300 1 (1) Yes I2C Comparator
ADS1013 12 3300 1 (1) No I2C None
ADS1118 16 860 2 (4) Yes SPI Temperature sensor
ADS1018 12 3300 2 (4) Yes SPI Temperature sensor
4 Pin Configuration and Functions
ADDR 1
ALERT/RDY 2
GND 3
AIN0 4
5
1NIA
9 SDA
8 VDD
7 AIN3
6 AIN2
LCS
01
ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
ADDR 1 10 SCL
ALERT/RDY 2 9 SDA
GND 3 8 VDD
AIN0 4 7 AIN3
AIN1 5 6 AIN2
Not to scale Not to scale
Figure 4-1. RUG Package, Figure 4-2. DYN and DGS Packages,
10-Pin (Top View) 10-Pin (Top View)
Table 4-1. Pin Functions: RUG, DYN, and DGS Packages
PIN
NAME ADS1113 ADS1114 ADS1115 TYPE DESCRIPTION(1)
ADDR 1 1 1 Digital input I2C target address select
AIN0 4 4 4 Analog input Analog input 0
AIN1 5 5 5 Analog input Analog input 1
AIN2 — — 6 Analog input Analog input 2 (ADS1115 only)
AIN3 — — 7 Analog input Analog input 3 (ADS1115 only)
Comparator output or conversion ready (ADS1114 and ADS1115 only).
ALERT/RDY — 2 2 Digital output
Open-drain output. Connect to VDD using a pullup resistor.
GND 3 3 3 Analog Ground
NC 2, 6, 7 6, 7 — — No connect. Leave the pin floating or connect to GND.
SCL 10 10 10 Digital input Serial clock input. Connect to VDD using a pullup resistor.
SDA 9 9 9 Digital I/O Serial data input and output. Connect to VDD using a pullup resistor.
VDD 8 8 8 Analog Power supply. Connect a 0.1μF, power-supply decoupling capacitor to GND.
(1) See the Unused Inputs and Outputs section for unused pin connections.
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 3
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 4

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
5 Specifications
5.1 Absolute Maximum Ratings
over operating ambient temperature range (unless otherwise noted)(1)
MIN MAX UNIT
Power-supply voltage VDD to GND –0.3 7 V
Analog input voltage AIN0, AIN1, AIN2, AIN3 GND – 0.3 VDD + 0.3 V
Digital input voltage SDA, SCL, ADDR, ALERT/RDY GND – 0.3 5.5 V
Input current, continuous Any pin except power supply pins –10 10 mA
Operating ambient, TA –40 125
Temperature Junction, TJ –40 150 °C
Storage, Tstg –60 150
(1) Stresses beyond those listed under Absolute Maximum Ratings may cause permanent damage to the device. These are stress
ratings only, which do not imply functional operation of the device at these or any other conditions beyond those indicated under
Recommended Operating Conditions. Exposure to absolute-maximum-rated conditions for extended periods may affect device
reliability.
5.2 ESD Ratings
VALUE UNIT
Electrostatic Human-body model (HBM), per ANSI/ESDA/JEDEC JS-001(1) ±2000
V V
(ESD) discharge Charged-device model (CDM), per JEDEC specification JESD22-C101(2) ±500
(1) JEDEC document JEP155 states that 500V HBM allows safe manufacturing with a standard ESD control process.
(2) JEDEC document JEP157 states that 250V CDM allows safe manufacturing with a standard ESD control process.
5.3 Recommended Operating Conditions
MIN NOM MAX UNIT
POWER SUPPLY
Power supply (VDD to GND) 2 5.5 V
ANALOG INPUTS(1)
FSR Full-scale input voltage range(2) (VIN = V(AINP) – V(AINN)) ±0.256 ±6.144 V
V(AINx) Absolute input voltage GND VDD V
DIGITAL INPUTS
VDIG Digital input voltage GND 5.5 V
TEMPERATURE
TA Operating ambient temperature –40 125 °C
(1) AINP and AINN denote the selected positive and negative inputs. AINx denotes one of the four available analog inputs.
(2) This parameter expresses the full-scale range of the ADC scaling. No more than VDD + 0.3V must be applied to the analog inputs of
the device. See Table 7-1 for more information.
5.4 Thermal Information
RUG (X2QFN) DYN (SOT) DGS (VSSOP)
THERMAL METRIC(1) UNIT
10 PINS 10 PINS 10 PINS
R Junction-to-ambient thermal resistance 245.2 147.1 182.7 °C/W
θJA
R Junction-to-case (top) thermal resistance 69.3 59.3 67.2 °C/W
θJC(top)
R Junction-to-board thermal resistance 172.0 71.3 103.8 °C/W
θJB
ψ Junction-to-top characterization parameter 8.2 2.8 10.2 °C/W
JT
ψ Junction-to-board characterization parameter 170.8 70.4 102.1 °C/W
JB
R Junction-to-case (bottom) thermal resistance N/A N/A N/A °C/W
θJC(bot)
(1) For more information about traditional and new thermal metrics, see the Semiconductor and IC Package Thermal Metrics application
note.
4 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 5

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
5.5 Electrical Characteristics
at VDD = 3.3V, data rate = 8SPS, and full-scale input voltage range (FSR) = ±2.048V (unless otherwise noted); maximum
and minimum specifications apply from T = –40°C to +125°C; typical specifications are at T = 25°C
A A
PARAMETER TEST CONDITIONS MIN TYP MAX UNIT
ANALOG INPUT
FSR = ±6.144V(1) 10
Common-mode input FSR = ±4.096V(1), FSR = ±2.048V 6
MΩ
impedance FSR = ±1.024V 3
FSR = ±0.512V, FSR = ±0.256V 100
FSR = ±6.144V(1) 22
FSR = ±4.096V(1) 15
MΩ
Differential input impedance FSR = ±2.048V 4.9
FSR = ±1.024V 2.4
FSR = ±0.512V, ±0.256V 710 kΩ
SYSTEM PERFORMANCE
Resolution (no missing codes) 16 Bits
DR Data rate 8, 16, 32, 64, 128, 250, 475, 860 SPS
Data rate variation All data rates –10% 10%
Output noise See Noise Performance section
INL Integral nonlinearity DR = 8SPS, FSR = ±2.048V(2) 1 LSB
FSR = ±2.048V, differential inputs –3 ±1 3
Offset error LSB
FSR = ±2.048V, single-ended inputs ±3
Offset drift over temperature FSR = ±2.048V 0.005 LSB/°C
FSR = ±2.048V, T = 125°C,
Long-term Offset drift A ±1 LSB
1000 hours
Offset power-supply rejection FSR = ±2.048V, DC supply variation 1 LSB/V
Offset channel match Match between any two inputs 3 LSB
Gain error(3) FSR = ±2.048V, T = 25°C 0.01% 0.15%
A
FSR = ±0.256V 7
Gain drift over temperature(3) FSR = ±2.048V 5 40 ppm/°C
FSR = ±6.144V(1) 5
FSR = ±2.048V, T = 125°C,
Long-term gain drift(3) A ±0.05%
1000 hours
Gain power-supply rejection 80 ppm/V
Gain match(3) Match between any two gains 0.02% 0.1%
Gain channel match Match between any two inputs 0.05% 0.1%
At DC, FSR = ±0.256V 105
At DC, FSR = ±2.048V 100
CMRR Common-mode rejection ratio At DC, FSR = ±6.144V(1) 90 dB
f = 60Hz, DR = 8SPS 105
CM
f = 50Hz, DR = 8SPS 105
CM
DIGITAL INPUT/OUTPUT
V High-level input voltage 0.7 VDD 5.5 V
IH
V Low-level input voltage GND 0.3 VDD V
IL
V Low-level output voltage I = 3mA GND 0.15 0.4 V
OL OL
Input leakage current GND < V < VDD –10 10 μA
DIG
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 5
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 6

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
5.5 Electrical Characteristics (continued)
at VDD = 3.3V, data rate = 8SPS, and full-scale input voltage range (FSR) = ±2.048V (unless otherwise noted); maximum
and minimum specifications apply from T = –40°C to +125°C; typical specifications are at T = 25°C
A A
PARAMETER TEST CONDITIONS MIN TYP MAX UNIT
POWER-SUPPLY
T = 25°C 0.5 2
A
Power-down
5
I Supply current μA
VDD
T = 25°C 150 200
A
Operating
300
VDD = 5.0V 0.9
P Power dissipation VDD = 3.3 V 0.5 mW
D
VDD = 2.0V 0.3
(1) This parameter expresses the full-scale range of the ADC scaling. No more than VDD + 0.3V must be applied to the analog inputs of
the device. See Table 7-1 for more information.
(2) Best-fit INL; covers 99% of full-scale
(3) Includes all errors from onboard PGA and voltage reference
5.6 Timing Requirements: I2C
over operating ambient temperature range and VDD = 2.0V to 5.5V (unless otherwise noted)
FAST MODE HIGH-SPEED MODE
MIN MAX MIN MAX UNIT
f SCL clock frequency 0.01 0.4 0.01 3.4 MHz
SCL
t Bus free time between START and STOP condition 600 160 ns
BUF
Hold time after repeated START condition.
t 600 160 ns
HDSTA After this period, the first clock is generated.
t Setup time for a repeated START condition 600 160 ns
SUSTA
t Setup time for STOP condition 600 160 ns
SUSTO
t Data hold time 0 0 ns
HDDAT
t Data setup time 100 10 ns
SUDAT
t Low period of the SCL clock pin 1300 160 ns
LOW
t High period for the SCL clock pin 600 60 ns
HIGH
t Fall time for both SDA and SCL signals(1) 300 160 ns
F
t Rise time for both SDA and SCL signals(1) 300 160 ns
R
(1) For high-speed mode maximum values, the capacitive load on the bus line must not exceed 400pF.
t
LOW
t t t
R F HDSTA
SCL
t HDSTA t HIGH t SUSTA t SUSTO
t t
HDDAT SUDAT
SDA
t
BUF
P S S P
Figure 5-1. I2C Interface Timing
6 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 7

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
5.7 Typical Characteristics
at T = 25°C, VDD = 3.3V, FSR = ±2.048V, DR = 8SPS (unless otherwise noted)
A
300 5.0
4.5
250
A) VDD = 5 V μ
A) 4.0
g
C
urr
e nt
(
μ
2
1
0
5
0
0
VDD = 3.3 V w
n C
urr e
nt
( 3
3
2
.
.
.
5
0
5
er ati n 100 VDD = 2 V er- d o 2 1 . . 0 5 VDD = 5 V
O p o w VDD = 3.3 V
P 1.0
50
0.5
VDD = 2 V
0 0
-40 -20 0 20 40 60 80 100 120 140 -40 -20 0 20 40 60 80 100 120 140
Temperature (°C) Temperature (°C)
Figure 5-2. Operating Current vs Temperature Figure 5-3. Power-Down Current vs Temperature
150 60
FSR = ±4.096 V FSR = ±1.024 V
100 50
FSR = ±2.048 V FSR = ±0.512 V
VDD = 5 V
50
40
V) 0
VDD = 2 V
μ
V)
Err or
(
μ
-50 olt a g
e
( 3
2
0
0 VDD = 4 V
Off
s
et -
-
1
1
0
5
0
0 Off s
et
V
10
VDD = 3 V
0
-200 VDD = 2 V
VDD = 5 V
-250 -10
-300 -20
-40 -20 0 20 40 60 80 100 120 140 -40 -20 0 20 40 60 80 100 120 140
Temperature (°C) Temperature (°C)
Figure 5-4. Single-Ended Offset Error vs Temperature Figure 5-5. Differential Offset Error vs Temperature
0.05 0.15
FSR = ±0.256 V
0.04
0.10
0.03
FSR = ±0.512 V
%) 0.02 %) 0.05
FSR = ±256 mV
or
(
0.01 or
(
Err FSR = ±1.024 V, ±2.048 V, Err 0
ai n 0 ±4.096 V, and ±6.144 V ai n FSR = ±2.048 V
G -0.01 G -0.05
-0.02
-0.10
-0.03
-0.04 -0.15
-40 -20 0 20 40 60 80 100 120 140 2.0 2.5 3.0 3.5 4.0 4.5 5.0 5.5
Temperature (°C) Supply Voltage (V)
Figure 5-6. Gain Error vs Temperature Figure 5-7. Gain Error vs Supply Voltage
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 7
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 8

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
5.7 Typical Characteristics (continued)
at T = 25°C, VDD = 3.3V, FSR = ±2.048V, DR = 8SPS (unless otherwise noted)
A
60 60
50 40
V) V)
μ μ +125°C
y
(
40 y
(
20
e
arit
FSR = ±6.144 V e
arit -40°C
n n
nli 30 nli 0
N o FSR = ±0.512 V, ±0.256 V N o
al 20 FSR = ±2.048 V al -20
gr gr
e e +25°C
nt nt
I 10 I -40
0 -60
2.0 2.5 3.0 3.5 4.0 4.5 5.0 5.5 -2.0 -1.5 -1.0 -0.5 0 0.5 1.0 1.5 2.0
Supply Voltage (V) Input Signal (V)
VDD = 3.3V, FSR = ±2.048V, DR = 8SPS, best fit
Figure 5-8. INL vs Supply Voltage Figure 5-9. INL vs Input Signal
60 60
40 40
V)
+125°C
V)
μ μ
y
(
20 y
(
20
arit -40°C arit T A = -40°C
e e
n n
nli 0 nli 0
o o
al N -20 +25°C al N -20 T A = +125°C T A = +25°C
gr gr
e e
nt nt
I -40 I -40
-60 -60
-0.5 -0.375 -0.250 -0.125 0 0.125 0.250 0.375 0.5 -2.0 -1.5 -1.0 -0.5 0 0.5 1.0 1.5 2.0
Input Signal (V) Input Voltage (V)
VDD = 3.3V, FSR = ±0.512V, DR = 8SPS, best fit VDD = 5V, FSR = ±2.048V, DR = 8SPS, best fit
Figure 5-10. INL vs Input Signal Figure 5-11. INL vs Input Signal
60 140
40 120
V) V)
e
arit y
( μ
20 T
A
= -40°C
T A = +25°C μ
e
arit y
( 10
8
0
0
n n
nli 0 nli VDD = 2 V
gr al
N o
-20
T
A
= +125°C
gr al
N o 6
4
0
0
VDD = 5 V
e e
nt nt
I -40 I
20
VDD = 3.3 V
-60 0
-0.5 -0.4 -0.3 -0.2 -0.1 0 0.1 0.2 0.3 0.4 0.5 -60 -40 -20 0 20 40 60 80 100 120 140
Input Voltage (V) Temperature (°C)
VDD = 5V, FSR = ±0.512V, DR = 8SPS, best fit
Figure 5-12. INL vs Input Signal Figure 5-13. INL vs Temperature
8 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 9

5.7 Typical Characteristics (continued)
at T = 25°C, VDD = 3.3V, FSR = ±2.048V, DR = 8SPS (unless otherwise noted)
A
12 35
FSR = ±2.048 V
30
10
860 SPS
25
V) 8 V)
μ DR = 860 SPS μ
e ( e ( 20
oi s 6 oi s
S
N DR = 128 SPS
S
N 15
R M 4 R M 128 SPS
DR = 8 SPS 10
2 5
8 SPS
0 0
-0.5 -0.4 -0.3 -0.2 -0.1 0 0.1 0.2 0.3 0.4 0.5 2.0 2.5 3.0 3.5 4.0 4.5 5.0 5.5
Input Voltage (V) Supply Voltage (V)
FSR = ±0.512V FSR = ±2.048V
Figure 5-14. Noise vs Input Signal Figure 5-15. Noise vs Supply Voltage
10 30
9
25
8
s
e
V) 7 e n c 20
e
(
μ
6
c
urr
s c
oi 5 O 15
M S
N
4 er
of
R 3 m b 10
u
N
2
5
1
0 0
-40 -20 0 20 40 60 80 100 120 140 0 5 0 5 0 5 0 5 0 5 0 5 0 5 0 5 0 5 0 5 0
1 0 0 1 1 2 2 3 3 4 4 5 5 6 6 7 7 8 8 9
Temperature (°C) 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0 0. 0
- -
FSR = ±2.048V, DR = 8SPS Gain Error (%)
FSR = ±2.048V, 185 units
Figure 5-16. Noise vs Temperature Figure 5-17. Gain Error Histogram
160
140
e s 120 c
n
e urr 100
c
c O 80
of
er 60
b
m
u 40 N
20
0
-3 -2 -1 0 1 2 3
Offset (LSBs)
FSR = ±2.048V, 185 units
Figure 5-18. Offset Histogram
)Vm(
rorrE
latoT
ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
4
3
2
1
0
-1
-2
-3
-4
-2.048 -1.024 0 1.024 2.048
Input Signal (V)
Differential inputs; includes noise, offset and gain error
Figure 5-19. Total Error vs Input Signal
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 9
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 10

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
5.7 Typical Characteristics (continued)
at T = 25°C, VDD = 3.3V, FSR = ±2.048V, DR = 8SPS (unless otherwise noted)
A
4 0
3 -10
VDD = 5 V
2 -20
%)
Err
or
(
1
VDD = 3.3 V d
B)
-30
e 0 n ( -40
at ai
R G
a
-1 -50
at
D
-2 -60
VDD = 2 V
-3 -70
-4 -80
-40 -20 0 20 40 60 80 100 120 140 1 10 100 1k 10k
Temperature (°C) Input Frequency (Hz)
DR = 8SPS
Figure 5-20. Data Rate vs Temperature Figure 5-21. Digital Filter Frequency Response
10 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 11

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
6 Parameter Measurement Information
6.1 Noise Performance
Delta-sigma (ΔΣ) analog-to-digital converters (ADCs) are based on the principle of oversampling. The input
signal of a ΔΣ ADC is sampled at a high frequency (modulator frequency) and subsequently filtered and
decimated in the digital domain to yield a conversion result at the respective output data rate. The ratio between
modulator frequency and output data rate is called oversampling ratio (OSR). By increasing the OSR, and
thus reducing the output data rate, the noise performance of the ADC can be optimized. In other words, the
input-referred noise drops when reducing the output data rate because more samples of the internal modulator
are averaged to yield one conversion result. Increasing the gain also reduces the input-referred noise, which is
particularly useful when measuring low-level signals.
Table 6-1 and Table 6-2 summarize the ADS111x noise performance. Data are representative of typical noise
performance at T = 25°C with the inputs shorted together externally. Table 6-1 shows the input-referred noise
A
in units of μV for the conditions shown. The μV values are shown in parentheses. Table 6-2 shows the
RMS PP
effective resolution calculated from μV values using Equation 1. The noise-free resolution calculated from
RMS
peak-to-peak noise values using Equation 2 are shown in parentheses.
Effective Resolution = ln (FSR / V ) / ln(2) (1)
RMS-Noise
Noise-Free Resolution = ln (FSR / V ) / ln(2) (2)
PP-Noise
Table 6-1. Noise in μV (μV ) at VDD = 3.3V
RMS PP
FSR (Full-Scale Range)
DATA RATE
(SPS) ±6.144V ±4.096V ±2.048V ±1.024V ±0.512 V ±0.256 V
8 187.5 (187.5) 125 (125) 62.5 (62.5) 31.25 (31.25) 15.62 (15.62) 7.81 (7.81)
16 187.5 (187.5) 125 (125) 62.5 (62.5) 31.25 (31.25) 15.62 (15.62) 7.81 (7.81)
32 187.5 (187.5) 125 (125) 62.5 (62.5) 31.25 (31.25) 15.62 (15.62) 7.81 (7.81)
64 187.5 (187.5) 125 (125) 62.5 (62.5) 31.25 (31.25) 15.62 (15.62) 7.81 (7.81)
128 187.5 (187.5) 125 (125) 62.5 (62.5) 31.25 (31.25) 15.62 (15.62) 7.81 (12.35)
250 187.5 (252.09) 125 (148.28) 62.5 (84.03) 31.25 (39.54) 15.62 (16.06) 7.81 (18.53)
475 187.5 (266.92) 125 (227.38) 62.5 (79.08) 31.25 (56.84) 15.62 (32.13) 7.81 (25.95)
860 187.5 (430.06) 125 (266.93) 62.5 (118.63) 31.25 (64.26) 15.62 (40.78) 7.81 (35.83)
Table 6-2. Effective Resolution from RMS Noise (Noise-Free Resolution from Peak-to-Peak Noise) at
VDD = 3.3V
FSR (Full-Scale Range)
DATA RATE
(SPS) ±6.144V ±4.096V ±2.048V ±1.024V ±0.512 V ±0.256 V
8 16 (16) 16 (16) 16 (16) 16 (16) 16 (16) 16 (16)
16 16 (16) 16 (16) 16 (16) 16 (16) 16 (16) 16 (16)
32 16 (16) 16 (16) 16 (16) 16 (16) 16 (16) 16 (16)
64 16 (16) 16 (16) 16 (16) 16 (16) 16 (16) 16 (16)
128 16 (16) 16 (16) 16 (16) 16 (16) 16 (16) 16 (15.33)
250 16 (15.57) 16 (15.75) 16 (15.57) 16 (15.66) 16 (15.96) 16 (14.75)
475 16 (15.49) 16 (15.13) 16 (15.66) 16 (15.13) 16 (14.95) 16 (14.26)
860 16 (14.8) 16 (14.9) 16 (15.07) 16 (14.95) 16 (14.61) 16 (13.8)
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 11
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 12

7 Detailed Description
7.1 Overview
The ADS111x devices are very small, low-power, 16-bit, delta-sigma (ΔΣ) analog-to-digital converters (ADCs).
The ADS111x consist of a ΔΣ ADC core with an internal voltage reference, a clock oscillator, and an I2C
interface. The ADS1114 and ADS1115 also integrate a programmable gain amplifier (PGA) and a programmable
digital comparator. Figure 7-1, Figure 7-2, and Figure 7-3 show the functional block diagrams of the ADS1115,
ADS1114, and ADS1113, respectively.
The ADS111x ADC core measures a differential signal, V , that is the difference of V and V . The
IN (AINP) (AINN)
converter core consists of a differential, switched-capacitor ΔΣ modulator followed by a digital filter. This
architecture results in a very strong attenuation of any common-mode signals. Input signals are compared to
the internal voltage reference. The digital filter receives a high-speed bitstream from the modulator and outputs a
code proportional to the input voltage.
The ADS111x have two available conversion modes: single-shot and continuous-conversion. In single-shot
mode, the ADC performs one conversion of the input signal upon request, stores the conversion value to an
internal conversion register, and then enters a power-down state. This mode is intended to provide significant
power savings in systems that only require periodic conversions or when there are long idle periods between
conversions. In continuous-conversion mode, the ADC automatically begins a conversion of the input signal as
soon as the previous conversion is completed. The rate of continuous conversion is equal to the programmed
data rate. Data can be read at any time and always reflect the most recent completed conversion.
7.2 Functional Block Diagrams
ADS1115 Comparator
Voltage ALERT/RDY
Reference
ADDR
16-Bit I2C Interface SCL
SDA
(cid:1) (cid:0)
VDD
MUX
AIN0
AIN1 PGA ADC
AIN2
Oscillator
AIN3
GND
Figure 7-1. ADS1115 Block Diagram
VDD
ADS1114 Comparator
Voltage ALERT/RDY
Reference
ADDR
AIN0
PGA 16-Bit Inte I2 r C face SCL
AIN1
SDA
GND
(cid:1) (cid:0)
VDD
ADS1113
Voltage
Reference
ADDR
AIN0
16-Bit I2C ADC Interface SCL
AIN1
SDA
Oscillator
GND
Figure 7-2. ADS1114 Block Diagram
(cid:1) (cid:0)
ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
ADC
Oscillator
Figure 7-3. ADS1113 Block Diagram
12 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 13

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
7.3 Feature Description
7.3.1 Multiplexer
The ADS1115 contains an input multiplexer (MUX), as shown in Figure 7-4. Either four single-ended or two
differential signals can be measured. Additionally, AIN0 and AIN1 can be measured differentially to AIN3. The
multiplexer is configured by bits MUX[2:0] in the Config register. When single-ended signals are measured, the
negative input of the ADC is internally connected to GND by a switch within the multiplexer.
VDD ADS1115
AIN0
VDD
GND
AINP
AIN1
AINN
VDD
GND
AIN2
VDD
GND
AIN3
GND
GND
Figure 7-4. Input Multiplexer
The ADS1113 and ADS1114 do not have an input multiplexer and can measure either one differential signal
or one single-ended signal. For single-ended measurements, connect the AIN1 pin to GND externally. In
subsequent sections of this data sheet, AIN refers to AIN0 and AIN refers to AIN1 for the ADS1113 and
P N
ADS1114.
Electrostatic discharge (ESD) diodes connected to VDD and GND protect the ADS111x analog inputs. Keep the
absolute voltage of any input within the range shown in Equation 3 to prevent the ESD diodes from turning on.
GND – 0.3V < V < VDD + 0.3V (3)
(AINX)
If the voltages on the input pins can potentially violate these conditions, use external Schottky diodes and series
resistors to limit the input current to safe values (see the Absolute Maximum Ratings table). Overdriving an input
on the ADS1115 can affect conversions taking place on other inputs. If overdriving an input is possible, clamp the
signal with external Schottky diodes.
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 13
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 14

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
7.3.2 Analog Inputs
The ADS111x use a switched-capacitor input stage where capacitors are continuously charged and then
discharged to measure the voltage between AIN and AIN . The frequency at which the input signal is sampled
P N
is called the sampling frequency or the modulator frequency (f ). The ADS111x has a 1MHz internal oscillator
MOD
that is further divided by a factor of 4 to generate f at 250kHz. The capacitors used in this input stage
MOD
are small, and to external circuitry, the average loading appears resistive. Figure 7-5 shows this structure. The
capacitor values set the resistance and switching rate. Figure 7-6 shows the timing for the switches in Figure
7-5. During the sampling phase, switches S are closed. This event charges C to V , C to V , and
1 A1 (AINP) A2 (AINN)
C to (V – V ). During the discharge phase, S is first opened and then S is closed. Both C and
B (AINP) (AINN) 1 2 A1
C then discharge to approximately 0.7V and C discharges to 0V. This charging draws a very small transient
A2 B
current from the source driving the ADS111x analog inputs. The average value of this current can be used to
calculate the effective impedance (Z ), where Z = V / I .
eff eff IN AVERAGE
0.7 V
CA1 ZCM
AINP 0.7 V Equivalent
S1
CB
S2 Circuit AINP
ZDIFF
S1 S2
AINN 0.7 V AINN
fMOD = 250 kHz ZCM
CA2
0.7 V
Figure 7-5. Simplified Analog Input Circuit
t
SAMPLE
ON
S
1
OFF
ON
S
2
OFF
Figure 7-6. S and S Switch Timing
1 2
The common-mode input impedance is measured by applying a common-mode signal to the shorted AIN and
P
AIN inputs and measuring the average current consumed by each pin. The common-mode input impedance
N
changes depending on the full-scale range, but is approximately 6MΩ for the default full-scale range. In Figure
7-5, the common-mode input impedance is Z .
CM
The differential input impedance is measured by applying a differential signal to AIN and AIN inputs where
P N
one input is held at 0.7V. The current that flows through the pin connected to 0.7 V is the differential current and
scales with the full-scale range. In Figure 7-5, the differential input impedance is Z .
DIFF
Make sure to consider the typical value of the input impedance. Unless the input source has a low impedance,
the ADS111x input impedance can affect the measurement accuracy. For sources with high-output impedance,
buffering can be necessary. Active buffers introduce noise, and also introduce offset and gain errors. Consider all
of these factors in high-accuracy applications.
The clock oscillator frequency drifts slightly with temperature; therefore, the input impedances also drift. For most
applications, this input impedance drift is negligible, and can be ignored.
14 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 15

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
7.3.3 Full-Scale Range (FSR) and LSB Size
A programmable gain amplifier (PGA) is implemented before the ΔΣ ADC of the ADS1114 and ADS1115. The
full-scale range is configured by bits PGA[2:0] in the Config register and can be set to ±6.144V, ±4.096V,
±2.048V, ±1.024V, ±0.512V, and ±0.256V. Table 7-1 shows the FSR together with the corresponding LSB size.
Equation 4 shows how to calculate the LSB size from the selected full-scale range.
LSB = FSR / 216 (4)
Table 7-1. Full-Scale Range and Corresponding LSB
Size
FSR LSB SIZE
±6.144V(1) 187.5μV
±4.096V(1) 125μV
±2.048V 62.5μV
±1.024V 31.25μV
±0.512V 15.625μV
±0.256V 7.8125μV
(1) This parameter expresses the full-scale range of the ADC
scaling. Do not apply more than VDD + 0.3V to the analog
inputs of the device.
The FSR of the ADS1113 is fixed at ±2.048V.
Analog input voltages must never exceed the analog input voltage limits given in the Absolute Maximum Ratings.
If a VDD supply voltage greater than 4V is used, the ±6.144V full-scale range allows input voltages to extend
up to the supply. Although in this case (or whenever the supply voltage is less than the full-scale range; for
example, VDD = 3.3V and full-scale range = ±4.096V), a full-scale ADC output code cannot be obtained. For
example, with VDD = 3.3V and FSR = ±4.096V, only differential signals up to V = ±3.3V can be measured. The
IN
code range that represents voltages |V | > 3.3V is not used in this case.
IN
7.3.4 Voltage Reference
The ADS111x have an integrated voltage reference. An external reference cannot be used with these devices.
The ADS111x does not use a traditional band-gap reference to generate the internal voltage reference. For that
reason, the reference does not have an actual specified voltage value. Instead of using the reference voltage
value and the gain setting to derive the full-scale range of the ADC, use the FSR values provided in Table 7-1
directly.
Errors associated with the initial voltage reference accuracy and the reference drift with temperature are included
in the gain error and gain drift specifications in the Electrical Characteristics table.
7.3.5 Oscillator
The ADS111x have an integrated oscillator running at 1MHz. No external clock can be applied to operate these
devices. The internal oscillator drifts over temperature and time. The output data rate scales proportionally with
the oscillator frequency.
7.3.6 Output Data Rate and Conversion Time
The ADS111x offer programmable output data rates. Use the DR[2:0] bits in the Config register to select output
data rates of 8SPS, 16SPS, 32SPS, 64SPS, 128SPS, 250SPS, 475SPS, or 860SPS.
Conversions in the ADS111x settle within a single cycle; thus, the conversion time is equal to 1 / DR.
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 15
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 16

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
7.3.7 Digital Comparator (ADS1114 and ADS1115 Only)
The ADS1115 and ADS1114 feature a programmable digital comparator that can issue an alert on the
ALERT/RDY pin. The COMP_MODE bit in the Config register configures the comparator as either a traditional
comparator or a window comparator. In traditional comparator mode, the ALERT/RDY pin asserts (active low by
default) when conversion data exceeds the limit set in the high-threshold register (Hi_thresh). The comparator
then deasserts only when the conversion data falls below the limit set in the low-threshold register (Lo_thresh).
In window comparator mode, the ALERT/RDY pin asserts when the conversion data exceeds the Hi_thresh
register or falls below the Lo_thresh register value.
In either window or traditional comparator mode, the comparator can be configured to latch after being asserted
by the COMP_LAT bit in the Config register. This setting causes the assertion to remain even if the input signal
is not beyond the bounds of the threshold registers. This latched assertion can only be cleared by issuing an
SMBus alert response or by reading the Conversion register. The ALERT/RDY pin can be configured as active
high or active low by the COMP_POL bit in the Config register. Operational diagrams for both the comparator
modes are shown in ALERT Pin Timing Diagram.
The comparator can also be configured to activate the ALERT/RDY pin only after a set number of
successive readings exceed the threshold values set in the threshold registers (Hi_thresh and Lo_thresh). The
COMP_QUE[1:0] bits in the Config register configure the comparator to wait for one, two, or four readings
beyond the threshold before activating the ALERT/RDY pin. The COMP_QUE[1:0] bits can also disable the
comparator function and put the ALERT/RDY pin into a high state.
TH_H TH_H
Input Signal Input Signal
TH_L TH_L
Time Time
Latching
Latching Successful Comparator Successful Successful
Comparator SMBus Alert Output SMBus Alert SMBus Alert
Output Response Response Response
Time Time
Non-Latching
Non-Latching Comparator
Comparator Output
Output
Time Time
TRADITIONAL COMPARATOR MODE WINDOW COMPARATOR MODE
Figure 7-7. ALERT Pin Timing Diagram
16 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 17

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
7.3.8 Conversion Ready Pin (ADS1114 and ADS1115 Only)
The ALERT/RDY pin can also be configured as a conversion-ready pin. Set the most-significant bit of the
Hi_thresh register to 1b and the most-significant bit of Lo_thresh register to 0b to enable the pin as a
conversion-ready pin. The COMP_POL bit continues to function as expected. Set the COMP_QUE[1:0] bits
to any 2-bit value other than 11b to keep the ALERT/RDY pin enabled, and allow the conversion-ready signal to
appear at the ALERT/RDY pin output. The COMP_MODE and COMP_LAT bits no longer control any function.
When configured as a conversion-ready pin, ALERT/RDY continues to require a pullup resistor. The ADS111x
provide an approximately 8μs conversion-ready pulse on the ALERT/RDY pin at the end of each conversion in
continuous-conversion mode, as shown in Figure 7-8. In single-shot mode, the ALERT/RDY pin asserts low at
the end of a conversion if the COMP_POL bit is set to 0b.
ADS1114/5
Converting Converting Converting Converting
Status
Conversion Ready Conversion Ready Conversion Ready
8 μs
ALERT/RDY
(active high)
Figure 7-8. Conversion Ready Pulse in Continuous-Conversion Mode
7.3.9 SMbus Alert Response
In latching comparator mode (COMP_LAT = 1b), the ALERT/RDY pin asserts when the comparator detects a
conversion that exceeds the upper or lower threshold value. This assertion is latched and can be cleared only by
reading conversion data, or by issuing a successful SMBus alert response and reading the asserting device I2C
address. If conversion data exceed the upper or lower threshold values after being cleared, the pin reasserts.
This assertion does not affect conversions that are already in progress. The ALERT/RDY pin is an open-drain
output. This architecture allows several devices to share the same interface bus. When disabled, the pin holds a
high state so that the pin does not interfere with other devices on the same bus line.
When the controller senses that the ALERT/RDY pin has latched, the controller issues an SMBus alert command
(00011001b) to the I2C bus. Any ADS1114 and ADS1115 data converters on the I2C bus with the ALERT/RDY
pins asserted respond to the command with the target address. If more than one ADS111x on the I2C bus assert
the latched ALERT/RDY pin, arbitration during the address response portion of the SMBus alert determines
which device clears assertion. The device with the lowest I2C address always wins arbitration. If a device loses
arbitration, the device does not clear the comparator output pin assertion. The controller then repeats the SMBus
alert response until all devices have the respective assertions cleared. In window comparator mode, the SMBus
alert status bit indicates a 1b if signals exceed the high threshold, and a 0b if signals exceed the low threshold.
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 17
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 18

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
7.4 Device Functional Modes
7.4.1 Reset and Power-Up
The ADS111x reset on power-up and set all the bits in the Config register to the respective default settings.
The ADS111x enter a power-down state after completion of the reset process. The device interface and digital
blocks are active, but no data conversions are performed. The initial power-down state of the ADS111x relieves
systems with tight power-supply requirements from encountering a surge during power-up.
The ADS111x respond to the I2C general-call reset commands. When the ADS111x receive a general call reset
command (06h), an internal reset is performed as if the device is powered up.
7.4.2 Operating Modes
The ADS111x operate in one of two modes: continuous-conversion or single-shot. The MODE bit in the Config
register selects the respective operating mode.
7.4.2.1 Single-Shot Mode
When the MODE bit in the Config register is set to 1b, the ADS111x enter a power-down state, and operate
in single-shot mode. This power-down state is the default state for the ADS111x when power is first applied.
Although powered down, the devices still respond to commands. The ADS111x remain in this power-down state
until a 1b is written to the operational status (OS) bit in the Config register. When the OS bit is asserted,
the device powers up in approximately 25 μs, resets the OS bit to 0b, and starts a single conversion. When
conversion data are ready for retrieval, the device powers down again. Writing a 1b to the OS bit while a
conversion is ongoing has no effect. To switch to continuous-conversion mode, write a 0b to the MODE bit in the
Config register.
7.4.2.2 Continuous-Conversion Mode
In continuous-conversion mode (MODE bit set to 0b), the ADS111x perform conversions continuously. When
a conversion is complete, the ADS111x place the result in the Conversion register and immediately begin
another conversion. When writing new configuration settings, the currently ongoing conversion completes with
the previous configuration settings. Thereafter, continuous conversions with the new configuration settings start.
To switch to single-shot conversion mode, write a 1b to the MODE bit in the configuration register or reset the
device.
7.4.3 Duty Cycling For Low Power
The noise performance of a ΔΣ ADC generally improves when lowering the output data rate because more
samples of the internal modulator are averaged to yield one conversion result. In applications where power
consumption is critical, improved noise performance at low data rates is not always required. For these
applications, the ADS111x support duty cycling that yields significant power savings by periodically requesting
high data rate readings at an effectively lower data rate. For example, an ADS111x in the power-down state with
a data rate set to 860SPS can be operated by a microcontroller that instructs a single-shot conversion every
125ms (8SPS). A conversion at 860SPS only requires approximately 1.2ms, so the ADS111x enter power-down
state for the remaining 123.8ms. In this configuration, the ADS111x consume approximately 1/100th the power
that is otherwise consumed in continuous-conversion mode. The duty cycling rate is completely arbitrary and is
defined by the controller. The ADS111x offer lower data rates that do not implement duty cycling and also offer
improved noise performance if required.
18 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 19

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
7.5 Programming
7.5.1 I2C Interface
The ADS111x communicate through an I2C interface. I2C is a two-wire open-drain interface that supports
multiple devices and controllers on a single bus. Devices on the I2C bus only drive the bus lines low by
connecting them to ground; the devices never drive the bus lines high. Instead, the bus wires are pulled high
by pullup resistors, so the bus wires are always high when no device is driving them low. As a result of
this configuration, two devices cannot conflict. If two devices drive the bus simultaneously, there is no driver
contention.
Communication on the I2C bus always takes place between two devices, one acting as the controller and the
other as the target. Both the controller and target can read and write, but the target can only do so under the
direction of the controller. Some I2C devices can act as a controller or target, but the ADS111x can only act as a
target device.
An I2C bus consists of two lines: SDA and SCL. SDA carries data; SCL provides the clock. All data are
transmitted across the I2C bus in groups of eight bits. To send a bit on the I2C bus, drive the SDA line to the
appropriate level while SCL is low (a low on SDA indicates the bit is zero; a high indicates the bit is one). After
the SDA line settles, the SCL line is brought high, then low. This pulse on SCL clocks the SDA bit into the
receiver shift register. If the I2C bus is held idle for more than 25 ms, the bus times out.
The I2C bus is bidirectional; that is, the SDA line is used for both transmitting and receiving data. When the
controller reads from a target, the target drives the data line; when the controller writes to a target, the controller
drives the data line. The controller always drives the clock line. The ADS111x cannot act as a controller, and
therefore can never drive SCL.
Most of the time the bus is idle; no communication occurs, and both lines are high. When communication takes
place, the bus is active. Only a controller device can start a communication and initiate a START condition
on the bus. Normally, the data line is only allowed to change state when the clock line is low. If the data line
changes state when the clock line is high, this change is either a START condition or a STOP condition. A
START condition occurs when the clock line is high, and the data line goes from high to low. A STOP condition
occurs when the clock line is high, and the data line goes from low to high.
After the controller issues a START condition, the controller sends a byte that indicates which target device to
communicate with. This byte is called the address byte. Each device on an I2C bus has a unique 7-bit address
that the device responds to. The controller sends an address in the address byte, together with a bit that
indicates whether the controller wishes to read from or write to the target device.
Every byte (address and data) transmitted on the I2C bus is acknowledged with an acknowledge bit. When the
controller finishes sending a byte (eight data bits) to a target, the controller stops driving SDA and waits for the
target to acknowledge the byte. The target acknowledges the byte by pulling SDA low. The controller then sends
a clock pulse to clock the acknowledge bit. Similarly, when the controller completes reading a byte, the controller
pulls SDA low to acknowledge this completion to the target. The controller then sends a clock pulse to clock the
bit. The controller always drives the clock line.
If a device is not present on the bus, and the controller attempts to address the device, the controller receives
a not-acknowledge because no device is present at that address to pull the line low. A not-acknowledge is
performed by simply leaving SDA high during an acknowledge cycle.
When the controller has finished communicating with a target, the controller can issue a STOP condition. When
a STOP condition is issued, the bus becomes idle again. The controller can also issue another START condition.
When a START condition is issued while the bus is active, this condition is called a repeated start condition.
The Timing Requirements section shows a timing diagram for the ADS111x I2C communication.
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 19
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 20

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
7.5.1.1 I2C Address Selection
The ADS111x have one address pin, ADDR, that configures the I2C address of the device. This pin can be
connected to GND, VDD, SDA, or SCL, allowing for four different addresses to be selected with one pin, as
shown in Table 7-2. The state of address pin ADDR is sampled continuously. Use the GND, VDD and SCL
addresses first. If SDA is used as the device address, hold the SDA line low for at least 100 ns after the SCL line
goes low to make sure the device decodes the address correctly during I2C communication.
Table 7-2. ADDR Pin Connection and Corresponding Target Address
ADDR PIN CONNECTION TARGET ADDRESS
GND 1001000b
VDD 1001001b
SDA 1001010b
SCL 1001011b
7.5.1.2 I2C General Call
The ADS111x respond to the I2C general call address (0000000b) if the eighth bit is 0b. The devices
acknowledge the general call address and respond to commands in the second byte. If the second byte is
00000110b (06h), the ADS111x reset the internal registers and enter a power-down state.
7.5.1.3 I2C Speed Modes
The I2C bus operates at one of three speeds. Standard mode allows a clock frequency of up to 100kHz; fast
mode permits a clock frequency of up to 400kHz; and high-speed mode (also called Hs mode) allows a clock
frequency of up to 3.4MHz. The ADS111x are fully compatible with all three modes.
No special action is required to use the ADS111x in standard or fast mode, but high-speed mode must be
activated. To activate high-speed mode, send a special address byte of 00001xxxb following the START
condition, where xxx are bits unique to the Hs-capable controller. This byte is called the Hs controller code,
and is different from normal address bytes; the eighth bit does not indicate read/write status. The ADS111x
do not acknowledge this byte; the I2C specification prohibits acknowledgment of the Hs controller code. Upon
receiving a controller code, the ADS111x switch on Hs mode filters, and communicate at up to 3.4MHz. The
ADS111x switch out of Hs mode with the next STOP condition.
For more information on high-speed mode, consult the I2C specification.
7.5.2 Target Mode Operations
The ADS111x act as target receivers or target transmitters. The ADS111x cannot drive the SCL line as target
devices.
7.5.2.1 Receive Mode
In target receive mode, the first byte transmitted from the controller to the target consists of the 7-bit device
address followed by a low R/W bit. The next byte transmitted by the controller is the Address Pointer register.
The ADS111x then acknowledge receipt of the Address Pointer register byte. The next two bytes are written
to the address given by the register address pointer bits, P[1:0]. The ADS111x acknowledge each byte sent.
Register bytes are sent with the most significant byte first, followed by the least significant byte.
7.5.2.2 Transmit Mode
In target transmit mode, the first byte transmitted by the controller is the 7-bit target address followed by the
high R/W bit. This byte places the target into transmit mode and indicates that the ADS111x are being read
from. The next byte transmitted by the target is the most significant byte of the register that is indicated by
the register address pointer bits, P[1:0]. This byte is followed by an acknowledgment from the controller. The
remaining least significant byte is then sent by the target and is followed by an acknowledgment from the
controller. The controller can terminate transmission after any byte by not acknowledging or issuing a START or
STOP condition.
20 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 21

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
7.5.3 Writing To and Reading From the Registers
To access a specific register from the ADS111x, the controller must first write an appropriate value to register
address pointer bits P[1:0] in the Address Pointer register. The Address Pointer register is written to directly after
the target address byte, low R/W bit, and a successful target acknowledgment. After the Address Pointer register
is written, the target acknowledges, and the controller issues a STOP or a repeated START condition.
When reading from the ADS111x, the previous value written to bits P[1:0] determines the register that is read.
To change which register is read, a new value must be written to P[1:0]. To write a new value to P[1:0], the
controller issues a target address byte with the R/W bit low, followed by the Address Pointer register byte. No
additional data has to be transmitted, and a STOP condition can be issued by the controller. The controller can
now issue a START condition and send the target address byte with the R/W bit high to begin the read. Figure
7-9 details this sequence. If repeated reads from the same register are desired, there is no need to continually
send the Address Pointer register, because the ADS111x store the value of P[1:0] until modified by a write
operation. However, for every write operation, the Address Pointer register must be written with the appropriate
values.
1 9 1 9
1⁄4
SCL
SDA 1 0 0 1 0 A1 (1) A0 (1) R/W 0 0 0 0 0 0 P1 P0
Start By ACK By ACK By Stop By
Controller ADS1113/4/5 ADS1113/4/5 Controller
Frame 1: Target Address Byte Frame 2: Address Pointer Register
1 9 1 9
SCL 1⁄4
(Continued)
SDA 1 0 0 1 0 A1 (1) A0 (1) R/W D15 D14 D13 D12 D11 D10 D9 D8 1⁄4
(Continued)
Start By ACK By From ACK By
Controller ADS1113/4/5 ADS1113/4/5 Controller (2)
Frame 3: Target Address Byte Frame 4: Data Byte 1 Read Register
1 9
SCL
(Continued)
SDA
D7 D6 D5 D4 D3 D2 D1 D0
(Continued)
From ACK By Stop By
ADS1113/4/5 Controller (3) Controler
Frame 5: Data Byte 2 Read Register
A. The values of A0 and A1 are determined by the ADDR pin.
B. The controller can leave SDA high to terminate a single-byte read operation.
C. The controller can leave SDA high to terminate a two-byte read operation.
Figure 7-9. Timing Diagram for Reading From the ADS111x
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 21
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 22

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
1 9 1 9
1⁄4
SCL
1⁄4
SDA 1 0 0 1 0 A1(1) A0(1) R/W 0 0 0 0 0 0 P1 P0
Start By ACK By ACK By
Controller ADS1113/4/5 ADS1113/4/5
Frame 1: Target Address Byte Frame 2: Address Pointer Register
1 9 1 9
SCL
(Continued)
SDA
D15 D14 D13 D12 D11 D10 D9 D8 D7 D6 D5 D4 D3 D2 D1 D0
(Continued)
ACK By ACK By Stop By
ADS1113/4/5 ADS1113/4/5 Controller
Frame 3: Data Byte 1 Frame 4: Data Byte 2
A. The values of A0 and A1 are determined by the ADDR pin.
Figure 7-10. Timing Diagram for Writing to the ADS111x
ALERT
1 9 1 9
SCL
SDA 0 0 0 1 1 0 0 R/W 1 0 0 1 A1 A0 Status
Start By ACK By From NACK By Stop By
Controller ADS1113/4/5 ADS1113/4/5 Controller Controller
Frame 1: SMBus ALERT Response Address Byte Frame 2: Target Address
A. The values of A0 and A1 are determined by the ADDR pin.
Figure 7-11. Timing Diagram for SMBus Alert Response
22 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 23

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
7.5.4 Data Format
The ADS111x provide 16 bits of data in binary 2's-complement format. A positive full-scale (+FS) input produces
an output code of 7FFFh and a negative full-scale (–FS) input produces an output code of 8000h. The output
clips at these codes for signals that exceed full-scale. Table 7-3 summarizes the ideal output codes for different
input signals. Figure 7-12 shows code transitions versus input voltage.
Table 7-3. Input Signal Versus Ideal Output Code
INPUT SIGNAL
V = (V – V ) IDEAL OUTPUT CODE(1) (1)
IN AINP AINN
≥ +FS (215 – 1)/215 7FFFh
+FS/215 0001h
0 0000h
–FS/215 FFFFh
≤ –FS 8000h
(1) Excludes the effects of noise, INL, offset, and gain errors.
7FFFh
7FFEh
.
.
.
0001h
e
d
o 0000h
C
ut FFFFh
p
O
ut
.
. .
8001h
8000h
-FS . . . 0 . . . +FS
Input Voltage V
IN
215 - 1 215 - 1
-FS +FS
215 215
Figure 7-12. Code Transition Diagram
Note
Single-ended signal measurements, where V = 0V and V = 0V to +FS, only use the positive
AINN AINP
code range from 0000h to 7FFFh. However, because of device offset, the ADS111x can still output
negative codes in case V is close to 0V.
AINP
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 23
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 24

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
8 Registers
8.1 Register Map
The ADS111x have four registers that are accessible through the I2C interface using the Address Pointer
register. The Conversion register contains the result of the last conversion. The Config register is used to change
the ADS111x operating modes and query the status of the device. The other two registers, Lo_thresh and
Hi_thresh, set the threshold values used for the comparator function, and are not available in the ADS1113.
8.1.1 Address Pointer Register (address = N/A) [reset = N/A]
All four registers are accessed by writing to the Address Pointer register; see Figure 7-9.
Figure 8-1. Address Pointer Register
7 6 5 4 3 2 1 0
RESERVED P[1:0]
W-000000b W-00b
LEGEND: R/W = Read/Write; R = Read only; W = Write only; -n = value after reset
Table 8-1. Address Pointer Register Field Descriptions
Bit Field Type Reset Description
7:2 RESERVED W 000000b Always write 000000b
1:0 P[1:0] W 00b Register address pointer
00b : Conversion register
01b : Config register
10b : Lo_thresh register
11b : Hi_thresh register
8.1.2 Conversion Register (P[1:0] = 00b) [reset = 0000h]
The 16-bit Conversion register contains the result of the last conversion in binary two's-complement format.
Following power-up, the Conversion register is cleared to 0000h, and remains 0000h until the first conversion
completes.
Figure 8-2. Conversion Register
15 14 13 12 11 10 9 8
D[15:8]
R-00h
7 6 5 4 3 2 1 0
D[7:0]
R-00h
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 8-2. Conversion Register Field Descriptions
Bit Field Type Reset Description
15:0 D[15:0] R 0000h 16-bit conversion result
24 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 25

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
8.1.3 Config Register (P[1:0] = 01b) [reset = 8583h]
The 16-bit Config register controls the operating mode, input selection, data rate, full-scale range, and
comparator modes.
Figure 8-3. Config Register — ADS1113
15 14 13 12 11 10 9 8
OS RESERVED MODE
R/W-1b R/W-000010b R/W-1b
7 6 5 4 3 2 1 0
DR[2:0] RESERVED
R/W-100b R/W-00011b
Figure 8-4. Config Register — ADS1114
15 14 13 12 11 10 9 8
OS RESERVED PGA[2:0] MODE
R/W-1b R/W-000b R/W-010b R/W-1b
7 6 5 4 3 2 1 0
DR[2:0] COMP_MODE COMP_POL COMP_LAT COMP_QUE[1:0]
R/W-100b R/W-0b R/W-0b R/W-0b R/W-11b
Figure 8-5. Config Register — ADS1115
15 14 13 12 11 10 9 8
OS MUX[2:0] PGA[2:0] MODE
R/W-1b R/W-000b R/W-010b R/W-1b
7 6 5 4 3 2 1 0
DR[2:0] COMP_MODE COMP_POL COMP_LAT COMP_QUE[1:0]
R/W-100b R/W-0b R/W-0b R/W-0b R/W-11b
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 8-3. Config Register Field Descriptions
Bit Field Type Reset Description
Operational status or single-shot conversion start
This bit determines the operational status of the device. OS can only be written
when in power-down state and has no effect when a conversion is ongoing.
When writing:
15 OS R/W 1b 0b : No effect
1b : Start a single conversion (when in power-down state)
When reading:
0b : Device is currently performing a conversion.
1b : Device is not currently performing a conversion.
Input multiplexer configuration (ADS1115 only)
These bits configure the input multiplexer.
These bits serve no function on the ADS1113 and ADS1114. ADS1113 and
ADS1114 always use inputs AIN = AIN0 and AIN = AIN1.
P N
000b : AIN = AIN0 and AIN = AIN1 (default)
P N
001b : AIN = AIN0 and AIN = AIN3
14:12 MUX[2:0] R/W 000b P N
010b : AIN = AIN1 and AIN = AIN3
P N
011b : AIN = AIN2 and AIN = AIN3
P N
100b : AIN = AIN0 and AIN = GND
P N
101b : AIN = AIN1 and AIN = GND
P N
110b : AIN = AIN2 and AIN = GND
P N
111b : AIN = AIN3 and AIN = GND
P N
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 25
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 26

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
Table 8-3. Config Register Field Descriptions (continued)
Bit Field Type Reset Description
Programmable gain amplifier configuration
These bits set the FSR of the programmable gain amplifier.
These bits serve no function on the ADS1113. ADS1113 always uses FSR =
±2.048V.
000b : FSR = ±6.144V(1)
001b : FSR = ±4.096V(1)
11:9 PGA[2:0] R/W 010b
010b : FSR = ±2.048V (default)
011b : FSR = ±1.024V
100b : FSR = ±0.512V
101b : FSR = ±0.256V
110b : FSR = ±0.256V
111b : FSR = ±0.256V
Device operating mode
This bit controls the operating mode.
8 MODE R/W 1b
0b : Continuous-conversion mode
1b : Single-shot mode or power-down state (default)
Data rate
These bits control the data rate setting.
000b : 8SPS
001b : 16SPS
010b : 32SPS
7:5 DR[2:0] R/W 100b
011b : 64SPS
100b : 128SPS (default)
101b : 250SPS
110b : 475SPS
111b : 860SPS
Comparator mode (ADS1114 and ADS1115 only)
This bit configures the comparator operating mode.
4 COMP_MODE R/W 0b This bit serves no function on the ADS1113.
0b : Traditional comparator (default)
1b : Window comparator
Comparator polarity (ADS1114 and ADS1115 only)
This bit controls the polarity of the ALERT/RDY pin.
3 COMP_POL R/W 0b This bit serves no function on the ADS1113.
0b : Active low (default)
1b : Active high
Latching comparator (ADS1114 and ADS1115 only)
This bit controls whether the ALERT/RDY pin latches after being asserted or
clears after conversions are within the margin of the upper and lower threshold
values.
This bit serves no function on the ADS1113.
2 COMP_LAT R/W 0b 0b : Nonlatching comparator. The ALERT/RDY pin does not latch when asserted
(default).
1b : Latching comparator. The asserted ALERT/RDY pin remains latched until
conversion data are read by the controller or an appropriate SMBus alert
response is sent by the controller. The device responds with an address, and
is the lowest address currently asserting the ALERT/RDY bus line.
Comparator queue and disable (ADS1114 and ADS1115 only)
These bits perform two functions. When set to 11, the comparator is disabled
and the ALERT/RDY pin is set to a high-impedance state. When set to any other
value, the ALERT/RDY pin and the comparator function are enabled, and the set
value determines the number of successive conversions exceeding the upper or
1:0 COMP_QUE[1:0] R/W 11b lower threshold required before asserting the ALERT/RDY pin.
These bits serve no function on the ADS1113.
00b : Assert after one conversion
01b : Assert after two conversions
10b : Assert after four conversions
11b : Disable comparator and set ALERT/RDY pin to high-impedance (default)
(1) This parameter expresses the full-scale range of the ADC scaling. Do not apply more than VDD + 0.3V to the analog inputs of the
device.
26 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 27

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
8.1.4 Lo_thresh (P[1:0] = 10b) [reset = 8000h] and Hi_thresh (P[1:0] = 11b) [reset = 7FFFh] Registers
These two registers are applicable to the ADS1115 and ADS1114. These registers serve no purpose in the
ADS1113. The upper and lower threshold values used by the comparator are stored in two 16-bit registers in
2's complement format. The comparator is implemented as a digital comparator; therefore, the values in these
registers must be updated whenever the PGA settings are changed.
The conversion-ready function of the ALERT/RDY pin is enabled by setting the Hi_thresh register MSB to 1b and
the Lo_thresh register MSB to 0b. To use the comparator function of the ALERT/RDY pin, the Hi_thresh register
value must always be greater than the Lo_thresh register value. The threshold register formats are shown in
Figure 8-6. When set to RDY mode, the ALERT/RDY pin outputs the OS bit when in single-shot mode, and
provides a continuous-conversion ready pulse when in continuous-conversion mode.
Figure 8-6. Lo_thresh Register
15 14 13 12 11 10 9 8
Lo_thresh[15:8]
R/W-80h
7 6 5 4 3 2 1 0
Lo_thresh[7:0]
R/W-00h
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 8-4. Hi_thresh Register
15 14 13 12 11 10 9 8
Hi_thresh[15:8]
R/W-7Fh
7 6 5 4 3 2 1 0
Hi_thresh[7:0]
R/W-FFh
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 8-5. Lo_thresh and Hi_thresh Register Field Descriptions
Bit Field Type Reset Description
15:0 Lo_thresh[15:0] R/W 8000h Low threshold value
15:0 Hi_thresh[15:0] R/W 7FFFh High threshold value
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 27
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 28

9 Application and Implementation
Note
Information in the following applications sections is not part of the TI component specification,
and TI does not warrant its accuracy or completeness. TI’s customers are responsible for
determining suitability of components for their purposes, as well as validating and testing their design
implementation to confirm system functionality.
9.1 Application Information
The following sections give example circuits and suggestions for using the ADS111x in various situations.
9.1.1 Basic Connections
The principle I2C connections for the ADS1115 are shown in Figure 9-1.
VDD
VDD 1-k
Microcontroller or
Microprocessor
with I2C Port
SCL
SDA
GPIO
Inputs Selected
from Configuration
Register
(cid:1)
to 10-k
(cid:0)
ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
10
ADS1115
SCL
(typ) 1 ADDR SDA 9
Pullup Resistors
2 ALERT/RDY VDD 8
3 GND AIN3 7 0.1 μF (typ)
4 AIN0 AIN2 6
AIN1
5
Figure 9-1. Typical Connections of the ADS1115
The fully-differential voltage input of the ADS111x is ideal for connection to differential sources with moderately
low source impedance, such as thermocouples and thermistors. Although the ADS111x can read bipolar
differential signals, these devices cannot accept negative voltages on either input.
The ADS111x draw transient currents during conversion. A 0.1μF power-supply bypass capacitor supplies the
momentary bursts of extra current required from the supply.
The ADS111x interface directly to standard mode, fast mode, and high-speed mode I2C controllers. Any
microcontroller I2C peripheral, including controller-only and single-controller I2C peripherals, operates with the
ADS111x. The ADS111x does not perform clock-stretching (that is, the device never pulls the clock line low), so
this function does not need to be provided for unless other clock-stretching devices are on the same I2C bus.
Pullup resistors are required on both the SDA and SCL lines because I2C bus drivers are open drain. The size
of these resistors depends on the bus operating speed and capacitance of the bus lines. Higher-value resistors
consume less power, but increase the transition times on the bus, thus limiting the bus speed. Lower-value
resistors allow higher speed, but at the expense of higher power consumption. Long bus lines have higher
capacitance and require smaller pullup resistors to compensate. Do not use resistors that are too small to avoid
bus drivers being unable to pull the bus lines low.
28 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 29

9.1.2 Single-Ended Inputs
The ADS1113 and ADS1114 can measure one, and the ADS1115 up to four, single-ended signals. The
ADS1113 and ADS1114 can measure single-ended signals by connecting AIN1 to GND externally. The ADS1115
measures single-ended signals by appropriate configuration of the MUX[2:0] bits in the Config register. Figure
9-2 shows a single-ended connection scheme for ADS1115. The single-ended signal ranges from 0 V up to
positive supply or +FS, whichever is lower. Negative voltages cannot be applied to these devices because the
ADS111x can only accept positive voltages with respect to ground. The ADS111x do not lose linearity within the
input range.
The ADS111x offer a differential input voltage range of ±FSR. Single-ended configurations use only one-half
of the full-scale input voltage range. Differential configurations maximize the dynamic range of the ADC and
provide better common-mode noise rejection than single-ended configurations.
0.1
(cid:1)
ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
VDD
10 Output Codes
ADS1115 SCL 0-32767
1 ADDR SDA 9
2 ALERT/RDY VDD 8
3 GND AIN3 7 F (typ)
4 AIN0 AIN2 6
AIN1
5
Inputs Selected
from Configuration
Register
NOTE: Digital pin connections are omitted for clarity.
Figure 9-2. Measuring Single-Ended Inputs
The ADS1115 also allows AIN3 to serve as a common point for measurements by the appropriate setting of
the MUX[2:0] bits. AIN0, AIN1, and AIN2 can all be measured with respect to AIN3. In this configuration, the
ADS1115 operates with inputs, where AIN3 serves as the common point. This ability improves the usable range
over the single-ended configuration because negative differential voltages are allowed when
GND < V < VDD; however, common-mode noise attenuation is not offered.
(AIN3)
9.1.3 Input Protection
The ADS111x are fabricated in a small-geometry, low-voltage process. The analog inputs feature protection
diodes to the supply rails. However, the current-handling ability of these diodes is limited, and the ADS111x
can be permanently damaged by analog input voltages that exceed approximately 300mV beyond the rails for
extended periods. One way to protect against overvoltage is to place current-limiting resistors on the input lines.
The ADS111x analog inputs can withstand continuous currents as large as 10mA.
9.1.4 Unused Inputs and Outputs
Follow the guidelines below for the connection of unused device pins:
• Either float unused analog inputs, or tie unused analog inputs to GND.
• Either float NC (not connected) pins, or tie the NC pins to GND.
• If the ALERT/RDY output pin is not used, leave the pin unconnected or tie the pin to VDD using a weak
pullup resistor.
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 29
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 30

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
9.1.5 Analog Input Filtering
Analog input filtering serves two purposes:
1. Limits the effect of aliasing during the sampling process
2. Reduces external noise from being a part of the measurement
Aliasing occurs when frequency components are present in the input signal that are higher than half the
sampling frequency of the ADC (also known as the Nyquist frequency). These frequency components fold back
and show up in the actual frequency band of interest below half the sampling frequency. The filter response of
the digital filter repeats at multiples of the sampling frequency, also known as the modulator frequency (f ),
MOD
as shown in Figure 9-3. Signals or noise up to a frequency where the filter response repeats are attenuated to
a certain amount by the digital filter depending on the filter architecture. Any frequency components present in
the input signal around the modulator frequency, or multiples thereof, are not attenuated and alias back into the
band of interest, unless attenuated by an external analog filter.
Magnitude
Sensor Unwanted
Signal Unwanted Signals
Signals
Output fMOD / 2 fMOD Frequency
Data Rate
Magnitude
Digital Filter
Aliasing of
Unwanted Signals
Output fMOD / 2 fMOD Frequency
Data Rate
Magnitude
External
Antialiasing Filter
Roll-Off
Output fMOD / 2 fMOD Frequency
Data Rate
Figure 9-3. Effect of Aliasing
Many sensor signals are inherently band-limited; for example, the output of a thermocouple has a limited rate of
change. In this case, the sensor signal does not alias back into the pass-band when using a ΔΣ ADC. However,
any noise pick-up along the sensor wiring or the application circuitry can potentially alias into the pass-band.
Power line-cycle frequency and harmonics are one common noise source. External noise can also be generated
from electromagnetic interference (EMI) or radio frequency interference (RFI) sources, such as nearby motors
and cellular phones. Another noise source typically exists on the printed-circuit-board (PCB) in the form of clocks
and other digital signals. Analog input filtering helps remove unwanted signals from affecting the measurement
result.
A first-order resistor-capacitor (RC) filter is (in most cases) sufficient to either totally eliminate aliasing, or to
reduce the effect of aliasing to a level within the noise floor of the sensor. Ideally, any signal beyond f / 2 is
MOD
attenuated to a level below the noise floor of the ADC. The digital filter of the ADS111x attenuate signals to a
certain degree, as shown in Figure 5-21. In addition, noise components are usually smaller in magnitude than
the actual sensor signal. Therefore, use a first-order RC filter with a cutoff frequency set at the output data rate
or 10x higher as a generally good starting point for a system design.
30 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 31

9.1.6 Connecting Multiple Devices
Up to four ADS111x devices can be connected to a single I2C bus using different address pin configurations for
each device. Use the address pin to set the ADS111x to one of four different I2C addresses. Use the GND, VDD,
and SCL addresses first. If SDA is used as the device address, hold the SDA line low for at least 100 ns after
the SCL line goes low to make sure the device decodes the address correctly during I2C communication. An
example showing four ADS111x devices on the same I2C bus is shown in Figure 9-4. One set of pullup resistors
is required per bus. If needed, lower the pullup resistor values to compensate for the additional bus capacitance
presented by multiple devices and increased line length.
1-k VDD
Microcontroller or
Microprocessor
With I2C Port
SCL
SDA
(cid:1) to 10-k (cid:0)
ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
VDD
GND
10
ADS1115
SCL
1 ADDR SDA 9
I2C Pullup Res i ( s t t y o p r ) s 2 ALERT/RDY VDD 8
3 GND AIN3 7
4 AIN0 AIN2 6
AIN1
5
10
ADS1115
SCL
1 ADDR SDA 9
2 ALERT/RDY VDD 8
3 GND AIN3 7
4 AIN0 AIN2 6
AIN1
5
10
ADS1115
SCL
1 ADDR SDA 9
2 ALERT/RDY VDD 8
3 GND AIN3 7
4 AIN0 AIN2 6
AIN1
5
10
ADS1115
SCL
1 ADDR SDA 9
2 ALERT/RDY VDD 8
3 GND AIN3 7
4 AIN0 AIN2 6
AIN1
5
NOTE: The ADS111x power and input connections are omitted for clarity. The ADDR pin selects the I2C address.
Figure 9-4. Connecting Multiple ADS111x Devices
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 31
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 32

9.1.7 Quick-Start Guide
This section provides a brief example of ADS111x communications. Hardware for this design includes: one
ADS111x configured with an I2C address of 1001000b; a microcontroller with an I2C interface; discrete
components such as resistors, capacitors, and serial connectors; and a 2-V to 5V power supply. Figure 9-5
shows the basic hardware configuration.
The ADS111x communicate with the controller (microcontroller) through an I2C interface. The controller provides
a clock signal on the SCL pin and data are transferred using the SDA pin. The ADS111x never drive the SCL pin.
For information on programming and debugging the microcontroller being used, see the device-specific product
data sheet.
The first byte sent by the controller is the ADS111x address, followed by the R/W bit that instructs the ADS111x
to listen for a subsequent byte. The second byte is the Address Pointer register byte. The third and fourth bytes
sent from the controller are written to the register indicated in register address pointer bits P[1:0]. See Figure 7-9
and Figure 7-10 for read and write operation timing diagrams, respectively. All read and write transactions with
the ADS111x must be preceded by a START condition, and followed by a STOP condition.
For example, to write to the configuration register to set the ADS111x to continuous-conversion mode and then
read the conversion result, send the following bytes in this order:
1. Write to Config register:
• First byte: 10010000b (first 7-bit I2C address followed by a low R/W bit)
• Second byte: 00000001b (points to Config register)
• Third byte: 10000100b (MSB of the Config register to be written)
• Fourth byte: 10000011b (LSB of the Config register to be written)
2. Write to Address Pointer register:
• First byte: 10010000b (first 7-bit I2C address followed by a low R/W bit)
• Second byte: 00000000b (points to Conversion register)
3. Read Conversion register:
• First byte: 10010001b (first 7-bit I2C address followed by a high R/W bit)
• Second byte: the ADS111x responds with the MSB of the Conversion register.
• Third byte: the ADS111x responds with the LSB of the Conversion register.
3.3 V
ADS111x
VDD
GND 3.3 V I2C-Capable Controller
MSP430F2002)
AIN0
AIN1
ADDR 10 k
AIN2 (ADS1115 Only)
SCL SCL (P1.6) VDD
AIN3 (ADS1115 Only)
SDA SDA (P1.7) GND
ALERT
(ADS1114/5 Only)
JTAG Serial/UART
(cid:1)
0.1 μF
10 k
(cid:0)
ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
3.3 V
0.1 μF
Figure 9-5. Basic Hardware Configuration
32 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 33

9.2 Typical Application
Shunt-based, current-measurement solutions are widely used to monitor load currents. Low-side, current-shunt
measurements are independent of the bus voltage because the shunt common-mode voltage is near ground.
Figure 9-6 shows an example circuit for a bidirectional, low-side, current-shunt measurement system. The load
current is determined by measuring the voltage across the shunt resistor that is amplified and level-shifted by a
low-drift operational amplifier, OPA333 . The OPA333 output voltage is digitized with ADS1115 and sent to the
microcontroller using the I2C interface. This circuit is capable of measuring bidirectional currents flowing through
the shunt resistor with great accuracy and precision.
High-Voltage Bus
LOAD
VSHUNT
R
TNUHS
VCM VDD VDD
R6
AINN
ILOAD R3 VINX +
±
OP
V
A
O
3
UT
33 R5
CC
C
M
D
1
IFF AIN A P DS1115 I2C
R1 R2
nivleK
eriW-4
noitcennoC
ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
CCM2
R4
Figure 9-6. Low-Side Current Shunt Monitoring
9.2.1 Design Requirements
Table 9-1 shows the design parameters for this application.
Table 9-1. Design Parameters
DESIGN PARAMETER VALUE
Supply voltage (VDD) 5V
Voltage across shunt resistor (VSHUNT) ±50mV
Output data rate (DR) ≥200 readings per second
Typical measurement accuracy at TA = 25°C(1) ±0.2%
(1) Does not account for inaccuracy of shunt resistor and the precision resistors used in the
application.
9.2.2 Detailed Design Procedure
The first stage of the application circuit consists of an OPA333 in a noninverting summing amplifier configuration
and serves two purposes:
1. To level-shift the ground-referenced signal to allow bidirectional current measurements while running off a
unipolar supply. The voltage across the shunt resistor, V , is level-shifted by a common-mode voltage,
SHUNT
V , as shown in Figure 9-6. The level-shifted voltage, V , at the noninverting input, is given by Equation
CM INX
5.
V = (V · R + V · R ) / (R + R ) (5)
INX CM 3 SHUNT 4 3 4
2. To amplify the level-shifted voltage (V ). The OPA333 is configured in a noninverting gain configuration
INX
with the output voltage, V , given by Equation 6.
OUT
V = V · (1 + R / R ) (6)
OUT INX 2 1
Using Equation 5 and Equation 6, V is given as a function of V and V by Equation 7.
OUT SHUNT CM
V = (V · R + V · R ) / (R + R ) · (1 + R / R ) (7)
OUT CM 3 SHUNT 4 3 4 2 1
Using Equation 7 the ADC differential input voltage, before the first-order RC filter, is given by Equation 8.
V – V = V · (1 + R / R ) / (1 + R / R ) + V · (R / R – R / R ) / (1 + R / R ) (8)
OUT CM SHUNT 2 1 4 3 CM 2 1 3 4 3 4
If R = R and R = R , Equation 8 is simplified to Equation 9.
1 4 2 3
V – V = V · (1 + R / R ) / (1 + R / R ) (9)
OUT CM SHUNT 2 1 4 3
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 33
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 34

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
9.2.2.1 Shunt Resistor Considerations
A shunt resistor (R ) is an accurate resistance inserted in series with the load as shown in Figure 9-6. If the
SHUNT
absolute voltage drop across the shunt, |V |, is a larger percentage of the bus voltage, the voltage drop can
SHUNT
reduce the overall efficiency and system performance. If |V | is too low, measuring the small voltage drop
SHUNT
requires careful design attention and proper selection of the ADC, operation amplifier, and precision resistors.
Make sure that the absolute voltage at the shunt terminals does not result in violation of the input common-mode
voltage range requirements of the operational amplifier. The power dissipation on the shunt resistor increases
the temperature because of the current flowing through the resistor. To minimize the measurement errors
resulting from variation in temperature, select a low-drift shunt resistor. To minimize the measurement gain error,
select a shunt resistor with low tolerance value. To remove the errors caused by stray ground resistance, use a
four-wire Kelvin-connected shunt resistor, as shown in Figure 9-6.
9.2.2.2 Operational Amplifier Considerations
The operational amplifier used for this design example requires the following features:
• Unipolar supply operation (5V)
• Low input offset voltage (< 10μV) and input offset voltage drift (< 0.5μV/°C)
• Rail-to-rail input and output capability
• Low thermal and flicker noise
• High common-mode rejection (> 100dB)
The OPA333 offers all these benefits and is selected for this application.
9.2.2.3 ADC Input Common-Mode Considerations
V sets the V common-mode voltage by appropriate selection of precision resistors R , R , R , and R .
CM OUT 1 2 3 4
If R = R , R = R , and V = 0V, V is given by Equation 10.
1 3 2 4 SHUNT OUT
V = V (10)
OUT CM
If V is connected to the ADC positive input (AINP) and V is connected to the ADC negative input
OUT CM
(AINN), V appears as a common-mode voltage to the ADC. This configuration allows pseudo-differential
CM
measurements and uses the maximum dynamic range of the ADC if V is set at mid-supply (VDD / 2). A
CM
resistor divider from VDD to GND followed by a buffer amplifier can be used to generate V .
CM
9.2.2.4 Resistor (R , R , R , R ) Considerations
1 2 3 4
Proper selection of resistors R , R , R , and R is critical for meeting the overall accuracy requirements.
1 2 3 4
Using Equation 8, the offset term, V , and the gain term, A , of the differential ADC input are
OUT-OS OUT
represented by Equation 11 and Equation 12, respectively. The error contributions from the first-order RC filters
are ignored.
V = V · (R / R – R / R ) / (1 + R / R ) (11)
OUT-OS CM 2 1 3 4 3 4
A = (1 + R / R ) / (1 + R / R ) (12)
OUT 2 1 4 3
The tolerance, drift, and linearity performance of these resistors is critical to meeting the overall accuracy
requirements. In Equation 11, if R = R and R = R , V = 0V and therefore, the common-mode voltage,
1 3 2 4 OUT-OS
V , only contributes to level-shift V and does not introduce any error at the differential ADC inputs.
CM SHUNT
High-precision resistors provide better common-mode rejection from V .
CM
9.2.2.5 Noise and Input Impedance Considerations
If v represents the input-referred rms noise from all the resistors, v represents the input-referred rms
n_res n_op
noise of OPA333, and v represents the input-referred rms noise of ADS1115, the total input-referred noise
n_ADC
of the entire system, v , can be approximated by Equation 13.
N
v 2 = v 2 + v 2 + v / (1 + R / R )2 (13)
N n_res n_op n_ADC 2 1
34 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 35

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
The ADC noise contribution, v , is attenuated by the noninverting gain stage.
n_ADC
If the gain of the noninverting gain stage is high (≥ 5), a good approximation for v 2 is given by Equation 14.
n_res
The noise contribution from resistors R , R , R , and R when referred to the input is smaller in comparison to R
2 4 5 6 1
and R and can be neglected for approximation purposes.
3
v 2 = 4 · k · T · (R + R ) · Δf (14)
n_res 1 3
where:
• where k = Boltzmann constant
• T = temperature (in kelvins)
• Δf = noise bandwidth
An approximation for the input impedance, R , of the application circuit is given by Equation 15. R can be
IN IN
modeled as a resistor in parallel with the shunt resistor, and can contribute to additional gain error.
R = R + R (15)
IN 3 4
From Equation 14 and Equation 15, a trade-off exists between v and R . If R increases, v increases, and
N IN 3 n_res
therefore, the total input-referred rms system noise, v , increases. If R decreases, the input impedance, R ,
N 3 IN
drops, and causes additional gain error.
9.2.2.6 First-Order RC Filter Considerations
Although the device's digital filter attenuates high-frequency noise, use a first-order, low-pass RC filter at the
ADC inputs to further reject out-of-bandwidth noise and avoid aliasing. A differential low-pass RC filter formed by
R5, R6, and the differential capacitor C sets the –3dB cutoff frequency, f , given by Equation 16. These filter
DIFF C
resistors produce a voltage drop because of the input currents flowing into and out of the ADC. This voltage drop
can contribute to an additional gain error. Limit the filter resistor values to below 1kΩ.
f = 1 / [2π · (R + R ) · C ] (16)
C 5 6 DIFF
Two common-mode filter capacitors (C and C ) are also added to offer attenuation of high-frequency,
CM1 CM2
common-mode noise components. Select a differential capacitor, C , that is at least an order of magnitude
DIFF
(10x) larger than these common-mode capacitors because mismatches in these common-mode capacitors can
convert common-mode noise into differential noise.
9.2.2.7 Circuit Implementation
Table 9-2 shows the chosen values for this design.
Table 9-2. Parameters
PARAMETER VALUE
VCM 2.5V
FSR of ADC ±0.256V
Output data rate 250SPS
R1, R3 1kΩ(1)
R2, R4 5kΩ(1)
R5, R6 100Ω(1)
CDIFF 0.22μF
CCM1, CCM2 0.022μF
(1) 1% precision resistors used.
Using Equation 7, if V ranges from –50mV to +50mV, the application circuit produces a differential voltage
SHUNT
ranging from –0.250V to +0.250V across the ADC inputs. The ADC is therefore configured at an FSR of ±0.256V
to maximize the dynamic range of the ADC.
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 35
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 36

The –3dB cutoff frequencies of the differential low-pass filter and the common-mode low-pass filters are set at
3.6kHz and 0.36kHz, respectively.
R typically ranges from 0.01mΩ to 100mΩ. Therefore, if R = R = 1kΩ, a good trade-off exists between
SHUNT 1 3
the circuit input impedance and input referred resistor noise as explained in the Noise and Input Impedance
Considerations section.
A simple resistor divider followed by a buffer amplifier is used to generate V of 2.5V from a 5V supply.
CM
9.2.2.8 Results Summary
A precision voltage source is used to sweep V from –50mV to +50mV. The application circuit produces
SHUNT
a differential voltage of –250mV to +250mV across the ADC inputs. Figure 9-7 and Figure 9-8 show the
measurement results. The measurements are taken at T = 25°C. Although 1% tolerance resistors are used,
A
the exact value of these resistors are measured with a Fluke 4.5 digit multimeter to exclude the errors resulting
from inaccuracy of these resistors. In Figure 9-7, the x-axis represents V and the black line represents the
SHUNT
measured digital output voltage in mV. In Figure 9-8, the x-axis represents V , the black line represents
SHUNT
the total measurement error in %, the blue line represents the total measurement error in % after excluding the
errors from precision resistors, and the green line represents the total measurement error in % after excluding
the errors from precision resistors and performing a system offset calibration with V = 0V. Table 9-3 shows
SHUNT
a results summary.
Table 9-3. Results Summary
PARAMETER(1) VALUE
Total error, including errors from 1% precision resistors 1.89%
Total error, excluding errors from 1% precision resistors 0.17%
Total error, after offset calibration, excluding errors from 1% precision resistors 0.11%
(1) T = 25°C, not accounting for inaccuracy of shunt resistor.
A
9.2.3 Application Curves
Shunt Voltage (mV)
)Vm(
tuptuO
derusaeM
250
200
150
100
50
0
-50
-100
-150
-200
-250
-60 -50 -40 -30 -20 -10 0 10 20 30 40 50 60
D004 Shunt Voltage (mV)
Figure 9-7. Measured Output vs Shunt Voltage
(V ) SHUNT
(cid:8) )
(
rorrE
tnemerusaeM
ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
2
1.75
1.5
1.25
1
0.75
0.5
0.25
0
-0.25
-0.5
-0.75
-1
-1.25 Including all errors
-1.5 Excluding resistor errors
-1.75 Excluding resistor errors, after offset calibration
-2
-50 -40 -30 -20 -10 0 10 20 30 40 50
D005
Figure 9-8. Measurement Error vs Shunt Voltage
(V ) SHUNT
36 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 37

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
10 Power Supply Recommendations
The device requires a single unipolar supply, VDD, to power both the analog and digital circuitry of the device.
10.1 Power-Supply Sequencing
Wait approximately 50μs after VDD is stabilized before communicating with the device to allow the power-up
reset process to complete.
10.2 Power-Supply Decoupling
Good power-supply decoupling is important to achieve optimum performance. VDD must be decoupled with at
least a 0.1μF capacitor, as shown in Figure 10-1. The 0.1μF bypass capacitor supplies the momentary bursts
of extra current required from the supply when the device is converting. Place the bypass capacitor as close to
the power-supply pin of the device as possible using low-impedance connections. Use multilayer ceramic chip
capacitors (MLCCs) that offer low equivalent series resistance (ESR) and inductance (ESL) characteristics for
power-supply decoupling purposes. For very sensitive systems, or for systems in harsh noise environments,
avoid using vias for connecting the capacitors to the device pins for better noise immunity. Using multiple vias in
parallel lowers the overall inductance, and is beneficial for connections to ground planes.
VDD
10
TI Device
SCL
1 ADDR SDA 9
2 ALERT/RDY VDD 8
3 GND AIN3 7 0.1 μF
4 AIN0 AIN2 6
AIN1
5
Figure 10-1. ADS1115 Power-Supply Decoupling
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 37
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 38

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
11 Layout
11.1 Layout Guidelines
Employ best design practices when laying out a printed-circuit board (PCB) for both analog and digital
components. For optimal performance, separate the analog components [such as ADCs, amplifiers, references,
digital-to-analog converters (DACs), and analog MUXs] from digital components [such as microcontrollers,
complex programmable logic devices (CPLDs), field-programmable gate arrays (FPGAs), radio frequency (RF)
transceivers, universal serial bus (USB) transceivers, and switching regulators]. An example of good component
placement is shown in Figure 11-1. Although Figure 11-1 provides a good example of component placement, the
best placement for each application is unique to the geometries, components, and PCB fabrication capabilities
employed. That is, there is no single layout that is perfect for every design and careful consideration must always
be used when designing with any analog component.
G
G
r
r
o
o
u
u
n
n
d
d
P
F
l
i
a
ll
n
o
e
r
S
plit
C ut G
G
r
r
o
o
u
u
n
n
d
d
P
F
l
i
a
ll
n
o
e
r
Signal pti o n al: Gr o u n d Ge S n u e p r p a l t y ion
O
Conditioning
Interface
(RC Filters Device Microcontroller
Transceiver
and
Amplifiers)
S
plit
C
ut Connector
G G r r o o u u n n d d P F l i a ll n o e r O pti o n
al:
Gr o u n
d
G G r r o o u u n n d d P F l i a ll n o e r
or Antenna
Figure 11-1. System Component Placement
The following outlines some basic recommendations for the layout of the ADS111x to get the best possible
performance of the ADC. A good design can be ruined with a bad circuit layout.
• Separate analog and digital signals. To start, partition the board into analog and digital sections where
the layout permits. Route digital lines away from analog lines. This placement prevents digital noise from
coupling back into analog signals.
• Fill void areas on signal layers with ground fill.
• Provide good ground return paths. Signal return currents flow on the path of least impedance. If the ground
plane is cut or has other traces that block the current from flowing right next to the signal trace, the current
must find another path to return to the source and complete the circuit. A longer return current path increases
the chance that the signal radiates. Sensitive signals are more susceptible to EMI interference.
• Use bypass capacitors on supplies to reduce high-frequency noise. Do not place vias between bypass
capacitors and the active device. Placing the bypass capacitors on the same layer as close to the active
device yields the best results.
• Consider the resistance and inductance of the routing. Often, traces for the inputs have resistances that react
with the input bias current and cause an added error voltage. Reduce the loop area enclosed by the source
signal and the return current in order to reduce the inductance in the path. Reduce the inductance to reduce
the EMI pickup, and reduce the high frequency impedance observed by the device.
• Differential inputs must be matched for both the inputs going to the measurement source.
• Analog inputs with differential connections must have a capacitor placed differentially across the inputs. Best
input combinations for differential measurements use adjacent analog input lines such as AIN0, AIN1 and
AIN2, AIN3. The differential capacitors must be of high quality. The best ceramic chip capacitors are C0G
(NPO), which have stable properties and low-noise characteristics.
38 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 39

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
11.2 Layout Example
Figure 11-2. ADS1115 X2QFN Package
Figure 11-3. ADS1115 VSSOP Package
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 39
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 40

ADS1113, ADS1114, ADS1115
SBAS444E – MAY 2009 – REVISED DECEMBER 2024 www.ti.com
12 Device and Documentation Support
12.1 Documentation Support
12.1.1 Related Documentation
For related documentation see the following:
• OPAx333 1.8-V, microPower, CMOS Operational Amplifiers, Zero-Drift Series (SBOS351)
• MSP430F20x3, MSP430F20x2, MSP430F20x1 Mixed-Signal Microcontrollers (SLAS491)
• TIDA-00824 Human Skin Temperature Sensing for Wearable Applications Reference Design (TIDUAY7)
12.2 Receiving Notification of Documentation Updates
To receive notification of documentation updates, navigate to the device product folder on ti.com. Click on
Notifications to register and receive a weekly digest of any product information that has changed. For change
details, review the revision history included in any revised document.
12.3 Support Resources
TI E2ETM support forums are an engineer's go-to source for fast, verified answers and design help — straight
from the experts. Search existing answers or ask your own question to get the quick design help you need.
Linked content is provided "AS IS" by the respective contributors. They do not constitute TI specifications and do
not necessarily reflect TI's views; see TI's Terms of Use.
12.4 Trademarks
TI E2ETM is a trademark of Texas Instruments.
All trademarks are the property of their respective owners.
12.5 Electrostatic Discharge Caution
This integrated circuit can be damaged by ESD. Texas Instruments recommends that all integrated circuits be handled
with appropriate precautions. Failure to observe proper handling and installation procedures can cause damage.
ESD damage can range from subtle performance degradation to complete device failure. Precision integrated circuits may
be more susceptible to damage because very small parametric changes could cause the device not to meet its published
specifications.
12.6 Glossary
TI Glossary This glossary lists and explains terms, acronyms, and definitions.
40 Submit Document Feedback Copyright © 2024 Texas Instruments Incorporated
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 41

ADS1113, ADS1114, ADS1115
www.ti.com SBAS444E – MAY 2009 – REVISED DECEMBER 2024
13 Revision History
NOTE: Page numbers for previous revisions may differ from page numbers in the current version.
Changes from January 1, 2018 to December 5, 2024 (from Revision D (January 2018) to
Revision E (December 2024)) Page
• Changed all instances of legacy terminology to controller and target where I2C is mentioned..........................1
• Updated the numbering format for tables, figures, and cross-references throughout the document................. 1
• Added DYN package and device family information to Features section...........................................................1
• Added Device Information table, added DYN package to Package Information table and deleted last
paragraph from Description section....................................................................................................................1
• Added DYN package to Pin Configuration and Functions section and changed Pin Functions table................ 3
• Added DYN package to Thermal Information table............................................................................................ 4
• Changed Y-axis unit of Total Error vs Input Signal figure from μV to mV in Typical Characteristics section...... 7
• Added additional information to last paragraph in Multiplexer section............................................................. 13
• Added additional information to the Voltage Reference section....................................................................... 15
• Moved the ALERT Pin Timing Diagram from the Conversion Ready Pin section to the Digital Comparator
section.............................................................................................................................................................. 16
• Corrected cross reference to Timing Diagram for Reading From the ADS111x figure in Writing to and Reading
From the Registers section...............................................................................................................................21
• Changed bit setting notation from hexadecimal to binary where beneficial for clarity throughout Register Map
section.............................................................................................................................................................. 24
• Added dedicated Config Register tables for ADS1113, ADS1114, and ADS1115 and changed bit descriptions
in Config Register Field Descriptions table in Config Register section.............................................................25
• Changed first paragraph in Lo_threh and Hi_thresh Registers section............................................................27
• Changed Unused Inputs and Outputs section..................................................................................................29
• Changed ADS1115 Power-Supply Decoupling figure.......................................................................................37
Changes from Revision C (May 2009) to Revision D (January 2018) Page
• Changed Digital input voltage max value from VDD + 0.3 V to 5.5 V in Absolute Maximum Ratings table....... 4
• Added "over temperature" to Offset drift parameter for clarity............................................................................5
• Added Long-term Offset drift parameter in Electrical Characteristics table........................................................5
• Added "over temperature" to Gain drift parameter for clarity..............................................................................5
• Added Long-term gain drift parameter in Electrical Characteristics table...........................................................5
• Changed V parameter max value from VDD to 5.5V in Electrical Characteristics table..................................5
IH
• Added Output Data Rate and Conversion Time section for clarity................................................................... 15
• Changed Figure 28, ALERT Pin Timing Diagram for clarity............................................................................. 17
• Changed Figure 39, Typical Connections of the ADS1115, for clarity.............................................................. 28
• Changed the resistor values in Figure 43, Basic Hardware Configuration, from 10Ω to 10kΩ.........................32
14 Mechanical, Packaging, and Orderable Information
The following pages include mechanical, packaging, and orderable information. This information is the most
current data available for the designated devices. This data is subject to change without notice and revision of
this document. For browser-based versions of this data sheet, refer to the left-hand navigation.
Copyright © 2024 Texas Instruments Incorporated Submit Document Feedback 41
Product Folder Links: ADS1113 ADS1114 ADS1115

## Page 42

PACKAGE OPTION ADDENDUM
www.ti.com 14-Oct-2025
PACKAGING INFORMATION
Orderable part number Status Material type Package | Pins Package qty | Carrier RoHS Lead finish/ MSL rating/ Op temp (°C) Part marking
(1) (2) (3) Ball material Peak reflow (6)
(4) (5)
ADS1113IDGSR Active Production VSSOP (DGS) | 10 2500 | LARGE T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BROI
ADS1113IDGSR.A Active Production VSSOP (DGS) | 10 2500 | LARGE T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BROI
ADS1113IDGSR.B Active Production VSSOP (DGS) | 10 2500 | LARGE T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BROI
ADS1113IDGST Active Production VSSOP (DGS) | 10 250 | SMALL T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BROI
ADS1113IDGST.A Active Production VSSOP (DGS) | 10 250 | SMALL T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BROI
ADS1113IDGST.B Active Production VSSOP (DGS) | 10 250 | SMALL T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BROI
ADS1113IRUGR Active Production X2QFN (RUG) | 10 3000 | LARGE T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N6J
ADS1113IRUGR.A Active Production X2QFN (RUG) | 10 3000 | LARGE T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N6J
ADS1113IRUGR.B Active Production X2QFN (RUG) | 10 3000 | LARGE T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N6J
ADS1113IRUGT Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N6J
ADS1113IRUGT.A Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N6J
ADS1113IRUGT.B Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N6J
ADS1114IDGSR Active Production VSSOP (DGS) | 10 2500 | LARGE T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BRNI
ADS1114IDGSR.A Active Production VSSOP (DGS) | 10 2500 | LARGE T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BRNI
ADS1114IDGSR.B Active Production VSSOP (DGS) | 10 2500 | LARGE T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BRNI
ADS1114IDGST Active Production VSSOP (DGS) | 10 250 | SMALL T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BRNI
ADS1114IDGST.A Active Production VSSOP (DGS) | 10 250 | SMALL T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BRNI
ADS1114IDGST.B Active Production VSSOP (DGS) | 10 250 | SMALL T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BRNI
ADS1114IRUGR Active Production X2QFN (RUG) | 10 3000 | LARGE T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N5J
ADS1114IRUGR.A Active Production X2QFN (RUG) | 10 3000 | LARGE T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N5J
ADS1114IRUGR.B Active Production X2QFN (RUG) | 10 3000 | LARGE T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N5J
ADS1114IRUGT Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N5J
ADS1114IRUGT.A Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N5J
ADS1114IRUGT.B Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N5J
ADS1114IRUGTG4 Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N5J
ADS1114IRUGTG4.A Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N5J
ADS1114IRUGTG4.B Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N5J
ADS1115IDGSR Active Production VSSOP (DGS) | 10 2500 | LARGE T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BOGI
ADS1115IDGSR.A Active Production VSSOP (DGS) | 10 2500 | LARGE T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BOGI
Addendum-Page 1

## Page 43

PACKAGE OPTION ADDENDUM
www.ti.com 14-Oct-2025
Orderable part number Status Material type Package | Pins Package qty | Carrier RoHS Lead finish/ MSL rating/ Op temp (°C) Part marking
(1) (2) (3) Ball material Peak reflow (6)
(4) (5)
ADS1115IDGSR.B Active Production VSSOP (DGS) | 10 2500 | LARGE T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BOGI
ADS1115IDGST Active Production VSSOP (DGS) | 10 250 | SMALL T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BOGI
ADS1115IDGST.A Active Production VSSOP (DGS) | 10 250 | SMALL T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BOGI
ADS1115IDGST.B Active Production VSSOP (DGS) | 10 250 | SMALL T&R Yes NIPDAU Level-2-260C-1 YEAR -40 to 125 BOGI
ADS1115IDYNR Active Production SOT-5X3 (DYN) | 10 3000 | LARGE T&R Yes SN Level-1-260C-UNLIM -40 to 125 P1115
ADS1115IRUGR Active Production X2QFN (RUG) | 10 3000 | LARGE T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N4J
ADS1115IRUGR.A Active Production X2QFN (RUG) | 10 3000 | LARGE T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N4J
ADS1115IRUGR.B Active Production X2QFN (RUG) | 10 3000 | LARGE T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N4J
ADS1115IRUGT Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N4J
ADS1115IRUGT.A Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N4J
ADS1115IRUGT.B Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N4J
ADS1115IRUGTG4 Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N4J
ADS1115IRUGTG4.A Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N4J
ADS1115IRUGTG4.B Active Production X2QFN (RUG) | 10 250 | SMALL T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 N4J
(1) Status: For more details on status, see our product life cycle.
(2) Material type: When designated, preproduction parts are prototypes/experimental devices, and are not yet approved or released for full production. Testing and final process, including without limitation quality assurance,
reliability performance testing, and/or process qualification, may not yet be complete, and this item is subject to further changes or possible discontinuation. If available for ordering, purchases will be subject to an additional
waiver at checkout, and are intended for early internal evaluation purposes only. These items are sold without warranties of any kind.
(3) RoHS values: Yes, No, RoHS Exempt. See the TI RoHS Statement for additional information and value definition.
(4) Lead finish/Ball material: Parts may have multiple material finish options. Finish options are separated by a vertical ruled line. Lead finish/Ball material values may wrap to two lines if the finish value exceeds the maximum
column width.
(5) MSL rating/Peak reflow: The moisture sensitivity level ratings and peak solder (reflow) temperatures. In the event that a part has multiple moisture sensitivity ratings, only the lowest level per JEDEC standards is shown.
Refer to the shipping label for the actual reflow temperature that will be used to mount the part to the printed circuit board.
(6) Part marking: There may be an additional marking, which relates to the logo, the lot trace code information, or the environmental category of the part.
Multiple part markings will be inside parentheses. Only one part marking contained in parentheses and separated by a "~" will appear on a part. If a line is indented then it is a continuation of the previous line and the two
combined represent the entire part marking for that device.
Addendum-Page 2

## Page 44

PACKAGE OPTION ADDENDUM
www.ti.com 14-Oct-2025
Important Information and Disclaimer:The information provided on this page represents TI's knowledge and belief as of the date that it is provided. TI bases its knowledge and belief on information provided by third parties, and
makes no representation or warranty as to the accuracy of such information. Efforts are underway to better integrate information from third parties. TI has taken and continues to take reasonable steps to provide representative
and accurate information but may not have conducted destructive testing or chemical analysis on incoming materials and chemicals. TI and TI suppliers consider certain information to be proprietary, and thus CAS numbers
and other limited information may not be available for release.
In no event shall TI's liability arising out of such information exceed the total purchase price of the TI part(s) at issue in this document sold by TI to Customer on an annual basis.
OTHER QUALIFIED VERSIONS OF ADS1113, ADS1114, ADS1115 :
• Automotive : ADS1113-Q1, ADS1114-Q1, ADS1115-Q1
NOTE: Qualified Version Definitions:
• Automotive - Q100 devices qualified for high-reliability automotive applications targeting zero defects
Addendum-Page 3

## Page 45

PACKAGE MATERIALS INFORMATION
www.ti.com 10-Oct-2025
TAPE AND REEL INFORMATION
REEL DIMENSIONS TAPE DIMENSIONS
K0 P1
W
B0
Reel
Diameter
Cavity A0
A0 Dimension designed to accommodate the component width
B0 Dimension designed to accommodate the component length
K0 Dimension designed to accommodate the component thickness
W Overall width of the carrier tape
P1 Pitch between successive cavity centers
Reel Width (W1)
QUADRANT ASSIGNMENTS FOR PIN 1 ORIENTATION IN TAPE
Sprocket Holes
Q1 Q2 Q1 Q2
Q3 Q4 Q3 Q4 User Direction of Feed
Pocket Quadrants
*All dimensions are nominal
Device Package Package Pins SPQ Reel Reel A0 B0 K0 P1 W Pin1
Type Drawing Diameter Width (mm) (mm) (mm) (mm) (mm) Quadrant
(mm) W1 (mm)
ADS1113IDGSR VSSOP DGS 10 2500 330.0 12.4 5.3 3.3 1.3 8.0 12.0 Q1
ADS1113IDGST VSSOP DGS 10 250 180.0 12.4 5.3 3.3 1.3 8.0 12.0 Q1
ADS1113IRUGR X2QFN RUG 10 3000 180.0 8.4 1.75 2.25 0.55 4.0 8.0 Q1
ADS1113IRUGT X2QFN RUG 10 250 180.0 8.4 1.75 2.25 0.55 4.0 8.0 Q1
ADS1114IDGSR VSSOP DGS 10 2500 330.0 12.4 5.3 3.3 1.3 8.0 12.0 Q1
ADS1114IDGST VSSOP DGS 10 250 180.0 12.4 5.3 3.3 1.3 8.0 12.0 Q1
ADS1114IRUGR X2QFN RUG 10 3000 180.0 8.4 1.75 2.25 0.55 4.0 8.0 Q1
ADS1114IRUGT X2QFN RUG 10 250 180.0 8.4 1.75 2.25 0.55 4.0 8.0 Q1
ADS1114IRUGTG4 X2QFN RUG 10 250 180.0 8.4 1.75 2.25 0.55 4.0 8.0 Q1
ADS1115IDGSR VSSOP DGS 10 2500 330.0 12.4 5.3 3.3 1.3 8.0 12.0 Q1
ADS1115IDGST VSSOP DGS 10 250 180.0 12.4 5.3 3.3 1.3 8.0 12.0 Q1
ADS1115IDYNR SOT-5X3 DYN 10 3000 180.0 8.4 3.1 3.2 1.1 4.0 8.0 Q3
ADS1115IRUGR X2QFN RUG 10 3000 180.0 8.4 1.75 2.25 0.55 4.0 8.0 Q1
ADS1115IRUGT X2QFN RUG 10 250 180.0 8.4 1.75 2.25 0.55 4.0 8.0 Q1
ADS1115IRUGTG4 X2QFN RUG 10 250 180.0 8.4 1.75 2.25 0.55 4.0 8.0 Q1
Pack Materials-Page 1

## Page 46

PACKAGE MATERIALS INFORMATION
www.ti.com 10-Oct-2025
TAPE AND REEL BOX DIMENSIONS
Width (mm) H
W L
*All dimensions are nominal
Device Package Type Package Drawing Pins SPQ Length (mm) Width (mm) Height (mm)
ADS1113IDGSR VSSOP DGS 10 2500 367.0 367.0 38.0
ADS1113IDGST VSSOP DGS 10 250 213.0 191.0 35.0
ADS1113IRUGR X2QFN RUG 10 3000 210.0 185.0 35.0
ADS1113IRUGT X2QFN RUG 10 250 210.0 185.0 35.0
ADS1114IDGSR VSSOP DGS 10 2500 367.0 367.0 38.0
ADS1114IDGST VSSOP DGS 10 250 213.0 191.0 35.0
ADS1114IRUGR X2QFN RUG 10 3000 210.0 185.0 35.0
ADS1114IRUGT X2QFN RUG 10 250 210.0 185.0 35.0
ADS1114IRUGTG4 X2QFN RUG 10 250 210.0 185.0 35.0
ADS1115IDGSR VSSOP DGS 10 2500 367.0 367.0 38.0
ADS1115IDGST VSSOP DGS 10 250 213.0 191.0 35.0
ADS1115IDYNR SOT-5X3 DYN 10 3000 210.0 185.0 35.0
ADS1115IRUGR X2QFN RUG 10 3000 210.0 185.0 35.0
ADS1115IRUGT X2QFN RUG 10 250 210.0 185.0 35.0
ADS1115IRUGTG4 X2QFN RUG 10 250 210.0 185.0 35.0
Pack Materials-Page 2

## Page 47

PACKAGE OUTLINE
DYN0010A SOT - 0.8 mm max height
SCALE 6.000
PLASTIC SMALL OUTLINE
2.9
2.7
PIN 1 A
ID AREA
1
10
8X 0.5
3.0
2X 2
2.8
NOTE 3
6
5
2.2 0.45
B 10X
2.0 0.25
0.05
TYP
2X 4 -15 0.00
0.8 MAX C
SEATING PLANE
0.18 SYMM
10X 0.08 0.05 C
SYMM (0.185) TYP
0.30
10X
0.18
0.1 C A B
0.05 C
0.55
10X
0.35
4229803/F 10/2025
NOTES:
1. All linear dimensions are in millimeters. Any dimensions in parenthesis are for reference only. Dimensioning and tolerancing
per ASME Y14.5M.
2. This drawing is subject to change without notice.
3. This dimension does not include mold flash, protrusions, or gate burrs. Mold flash, protrusions, or gate burrs shall not
exceed 0.15 mm per side.
4. Reference JEDEC MO-368, variation AA.
www.ti.com

## Page 48

EXAMPLE BOARD LAYOUT
DYN0010A SOT - 0.8 mm max height
PLASTIC SMALL OUTLINE
10X
(0.82)
SYMM
1
10X (0.3) 10
8X (0.5)
SYMM
5 6
(R0.05) TYP
(2.53)
LAND PATTERN EXAMPLE
EXPOSED METAL SHOWN
SCALE:25X
0.05 MAX 0.05 MIN
AROUND AROUND
EXPOSED EXPOSED
METAL METAL
SOLDER MASK METAL METAL UNDER SOLDER MASK
OPENING SOLDER MASK OPENING
NON SOLDER MASK
SOLDER MASK
DEFINED
DEFINED
(PREFERRED)
SOLDERMASK DETAILS
4229803/F 10/2025
NOTES: (continued)
5. Publication IPC-7351 may have alternate designs.
6. Solder mask tolerances between and around signal pads can vary based on board fabrication site.
7. Land pattern design aligns to IPC-610, Bottom Termination Component (BTC) solder joint inspection criteria.
www.ti.com

## Page 49

EXAMPLE STENCIL DESIGN
DYN0010A SOT - 0.8 mm max height
PLASTIC SMALL OUTLINE
10X
(0.82) SYMM
1
10X (0.3) 10
8X (0.5)
SYMM
5 6
(R0.05) TYP
(2.53)
SOLDER PASTE EXAMPLE
BASED ON 0.1 mm THICK STENCIL
SCALE:25X
4229803/F 10/2025
NOTES: (continued)
8. Laser cutting apertures with trapezoidal walls and rounded corners may offer better paste release. IPC-7525 may have alternate
design recommendations.
9. Board assembly site may have different recommendations for stencil design.
www.ti.com

## Page 50

PACKAGE OUTLINE
DGS0010A VSSOP - 1.1 mm max height
SCALE 3.200
SMALL OUTLINE PACKAGE
C
5.05
4.75 TYP SEATING PLANE
A PIN 1 ID 0.1 C
AREA
8X 0.5
10
1
3.1
2X
2.9
NOTE 3 2
5
6
0.27
10X
0.17
B 3.1 0.1 C A B 1.1 MAX
2.9
NOTE 4
0.23
TYP
SEE DETAIL A 0.13
0.25
GAGE PLANE
0.15
0.7
0 - 8 0.05
0.4
DETAIL A
TYPICAL
4221984/A 05/2015
NOTES:
1. All linear dimensions are in millimeters. Any dimensions in parenthesis are for reference only. Dimensioning and tolerancing
per ASME Y14.5M.
2. This drawing is subject to change without notice.
3. This dimension does not include mold flash, protrusions, or gate burrs. Mold flash, protrusions, or gate burrs shall not
exceed 0.15 mm per side.
4. This dimension does not include interlead flash. Interlead flash shall not exceed 0.25 mm per side.
5. Reference JEDEC registration MO-187, variation BA.
www.ti.com

## Page 51

EXAMPLE BOARD LAYOUT
DGS0010A VSSOP - 1.1 mm max height
SMALL OUTLINE PACKAGE
10X (1.45)
10X (0.3) SYMM (R0.05)
TYP
1
10
SYMM
8X (0.5) 5 6
(4.4)
LAND PATTERN EXAMPLE
SCALE:10X
S O O P L E D N E IN R G MASK METAL M SO E L T D A E L R U N M D A E S R K S O O P L E D N E IN R G MASK
0.05 MAX 0.05 MIN
ALL AROUND ALL AROUND
NON SOLDER MASK SOLDER MASK
DEFINED DEFINED
SOLDER MASK DETAILS
NOT TO SCALE
4221984/A 05/2015
NOTES: (continued)
6. Publication IPC-7351 may have alternate designs.
7. Solder mask tolerances between and around signal pads can vary based on board fabrication site.
www.ti.com

## Page 52

EXAMPLE STENCIL DESIGN
DGS0010A VSSOP - 1.1 mm max height
SMALL OUTLINE PACKAGE
10X (1.45)
SYMM (R0.05) TYP
10X (0.3)
1
10
SYMM
8X (0.5)
5 6
(4.4)
SOLDER PASTE EXAMPLE
BASED ON 0.125 mm THICK STENCIL
SCALE:10X
4221984/A 05/2015
NOTES: (continued)
8. Laser cutting apertures with trapezoidal walls and rounded corners may offer better paste release. IPC-7525 may have alternate
design recommendations.
9. Board assembly site may have different recommendations for stencil design.
www.ti.com

## Page 53

GENERIC PACKAGE VIEW
RUG 10 X2QFN - 0.4 mm max height
1.5 x 2, 0.5 mm pitch PLASTIC QUAD FLATPACK - NO LEAD
This image is a representation of the package family, actual package may vary.
Refer to the product data sheet for package details.
4231768/A
www.ti.com

## Page 54

[No extractable text on this page.]

## Page 55

[No extractable text on this page.]

## Page 56

IMPORTANT NOTICE AND DISCLAIMER
TI PROVIDES TECHNICAL AND RELIABILITY DATA (INCLUDING DATASHEETS), DESIGN RESOURCES (INCLUDING REFERENCE
DESIGNS), APPLICATION OR OTHER DESIGN ADVICE, WEB TOOLS, SAFETY INFORMATION, AND OTHER RESOURCES “AS IS”
AND WITH ALL FAULTS, AND DISCLAIMS ALL WARRANTIES, EXPRESS AND IMPLIED, INCLUDING WITHOUT LIMITATION ANY
IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT OF THIRD
PARTY INTELLECTUAL PROPERTY RIGHTS.
These resources are intended for skilled developers designing with TI products. You are solely responsible for (1) selecting the appropriate
TI products for your application, (2) designing, validating and testing your application, and (3) ensuring your application meets applicable
standards, and any other safety, security, regulatory or other requirements.
These resources are subject to change without notice. TI grants you permission to use these resources only for development of an
application that uses the TI products described in the resource. Other reproduction and display of these resources is prohibited. No license
is granted to any other TI intellectual property right or to any third party intellectual property right. TI disclaims responsibility for, and you fully
indemnify TI and its representatives against any claims, damages, costs, losses, and liabilities arising out of your use of these resources.
TI’s products are provided subject to TI’s Terms of Sale, TI’s General Quality Guidelines, or other applicable terms available either on
ti.com or provided in conjunction with such TI products. TI’s provision of these resources does not expand or otherwise alter TI’s applicable
warranties or warranty disclaimers for TI products. Unless TI explicitly designates a product as custom or customer-specified, TI products
are standard, catalog, general purpose devices.
TI objects to and rejects any additional or different terms you may propose.
IMPORTANT NOTICE
Copyright © 2025, Texas Instruments Incorporated
Last updated 10/2025
