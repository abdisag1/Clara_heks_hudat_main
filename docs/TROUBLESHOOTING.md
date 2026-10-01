# Troubleshooting (Clara V2 Rev 4 PCB)

References: `ClaraV2_Schematics.pdf` (sheet 1 main board, sheet 2 dosing processor) and
the Rev 4 assembly drawing. Both processors are on the same PCB: the main processor is
the Arduino Mega plugged into U4; the dosing processor is the ATmega328 (U8) in a DIP
socket with crystal Y1 and programming header P14.

## No data from the dosing board ("Dosing board: NO COMMUNICATION")

### 1. Ask the firmware what is wrong

On the **main board** Serial Monitor (115200 baud or your `report_baud`, Newline):

```
status
i2c
```

| Result | Meaning | Go to |
|---|---|---|
| `i2c` lists 0x27 but **not 0x21**; status says `NO ANSWER from address 0x21` | the dosing processor is not on the bus | step 2 |
| status says `28 bytes, checksum wrong` / `frame length wrong` / `protocol version wrong` | the dosing processor answers but runs different firmware | step 3 |
| `i2c` finds nothing at all | SDA/SCL bus problem (then the LCD would fail too) | check P4/U4 wiring |

On the **dosing processor** console (P14 or the Uno's USB, 9600 baud): `status` shows
`I2C requests from main board: N`. N must increase by 1 every second. If the
dosing console does not answer at all, the processor is not running (step 2).

### 2. The dosing processor does not answer

Check on the PCB (sheet 2):

| Jumper | Must be | Why |
|---|---|---|
| **J13** | **OPEN** | J13 connects RESET (pin 1) to GND. Bridged, it holds the ATmega328 in reset permanently. |
| **J14** | closed | SCL: PC5 (pin 28) to the main-board SCL |
| **J15** | closed | SDA: PC4 (pin 27) to the main-board SDA |

Then measure with a multimeter, power on, dosing chip in its socket:

* U8 pin 7 and pin 20 to GND: 5 V.
* U8 pin 1 (RESET) to GND: 5 V. If it is 0 V, J13 is bridged or C6 is shorted.
* Continuity U8 pin 27 ↔ Mega SDA (pin 20), and U8 pin 28 ↔ Mega SCL (pin 21).
  Wrong or open means J14/J15 are not closed or the traces are broken.
* The chip must be seated correctly: pin 1 notch matching the socket print.

The chip itself:

* It must be an **ATmega328P** with a bootloader for 16 MHz, and Y1 must be a
  **16 MHz** crystal. The firmware assumes 16 MHz.
* It must contain the **v3 dosing firmware** (`arduino/ClaraDosing`). A quick
  check: the dosing console prints `Clara dosing board v3.0` at power-up.

### 3. The dosing processor answers with the wrong data

Both boards must run v3 firmware. The v3 frame is 28 bytes with a checksum; v2.2 sent
16 unprotected bytes. Upload `arduino/ClaraDosing` to the dosing processor **and**
`arduino/ClaraMainboard` to the Mega.

## Other jumpers on sheet 2 (stepper-driven peristaltic pump)

| Jumper | Setting | Function |
|---|---|---|
| J16 | closed | STEP from pin 10 (PB2), used by the firmware |
| J24 | open | STEP from pin 9 (alternative) |
| J25 | closed | DIR from pin 8 (PB0) |
| J22 | closed | ENABLE from pin 2 (PD2) via U10B to P19 "Enable" |
| J23 | open | pin 2 as brushless-motor feedback instead |
| J17 | closed | pump power relay K5 from pin 6 |
| J18 | open | pump relay from main-board D12 instead |
| J19 | closed | NaClO level: dosing pin A3 ← main-board D9 |
| J20 | closed | flowmeter to dosing pin 3 (INT1) |

On sheet 1 (flow meter): the drawing connects the meter to the dosing processor
through **J12** and to the main board D3 through **J11** (the note on the sheet
states it the other way round). Close J12 for dosing; J11 may stay open.

Note: the pump signals pass through LM358/TL972 buffers powered from 5 V. An LM358
output only reaches about 3.5 V and is slow. If the stepper driver misses steps at
high speed, fit the TL972 (rail-to-rail) or lower `max_step_rate`.
