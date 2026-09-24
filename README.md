# Clara v3 firmware

Firmware for the **Clara automated water disinfection device**. Clara produces sodium
hypochlorite (NaClO) on site by electrolysing brine, and injects it into the water
in proportion to the measured flow.

| Board | MCU | Responsibilities |
|---|---|---|
| **Main board** | Arduino Mega 2560 | NaClO production cycle (electrolysis, settling, transfer), level and voltage sensors, 20x4 LCD, polling the dosing board over I2C, reports to the Ecophi remote-monitoring unit over RS485 |
| **Dosing board** | Arduino Uno | Flow measurement, flow-proportional NaClO dosing with a stepper-driven peristaltic pump, dosing calibration and bench-test console, telemetry to the main board |

Version 3 is a rewrite of the v2.2 field firmware (kept for reference in
[`legacy/v2.2`](legacy/v2.2)). The wiring, the Ecophi frame format and the default
calibration values are unchanged, so the boards can be swapped without hardware work.
[`docs/V2.2_REVIEW.md`](docs/V2.2_REVIEW.md) lists the v2.2 defects this rewrite fixes.

## Highlights

* **Accurate timing.** The v2.2 code reprogrammed the Arduino hardware timers, and the
  Arduino core then silently changed them back, so none of the "seconds" and "minutes"
  had the intended length. v3 never touches Timer0. All timing uses `millis()` through a
  drift-free timer (`PeriodicTimer`), and durations are measured as elapsed time. A
  180-minute batch lasts 180 minutes to the accuracy of the crystal, and phases are
  chained on their exact deadlines.
* **Exact dosing.** v2.2 made the pump steps in a busy loop inside `loop()`, so every
  serial print or I2C transfer slowed the pump down. v3 generates the steps from the
  Timer1 interrupt. The number of steps (the NaClO volume) is computed exactly, including
  rounding carry-over. In simulation the delivered dose is within 0.03 % of theory from
  4 to 190 L/min, whether `loop()` runs every 1 ms or every 50 ms.
* **Easy calibration.** Every tunable value is a named, range-checked parameter stored
  in EEPROM with a checksum. Guided procedures cover the pump (`pumpcal`) and the
  flowmeter (`flowcal`). See [`docs/CALIBRATION.md`](docs/CALIBRATION.md).
* **Testable.** All logic is hardware-independent C++ in `lib/`, and runs on a PC:
  87 unit and integration tests, including both boards running together on a
  simulated clock. See [`docs/TESTING.md`](docs/TESTING.md).
* **Robust.** A batch resumes after a power cut (progress is saved in a wear-levelled
  EEPROM ring). I2C frames are checksummed and link loss is shown on the display. The
  pump stops within 1 s when the NaClO tank runs empty. The dosing board has a watchdog.

## Repository layout

```
platformio.ini            build configuration (mainboard, dosing, native tests)
Makefile                  host test runner for machines without PlatformIO
lib/
  clara_common/           shared: timers, EEPROM records, parameter tables,
                          console, number formatting, I2C protocol
  clara_dosing/           dosing board logic (no Arduino dependencies)
  clara_mainboard/        main board logic (no Arduino dependencies)
src/
  dosing/                 Uno drivers: Timer1 step generator, flowmeter interrupt,
                          I2C slave, main.cpp
  mainboard/              Mega drivers: relays, sensors, LCD, I2C master, main.cpp
test/
  support/fakes.h         fake hardware (clock, EEPROM, pump, flowmeter, ...)
  test_common/            unit tests
  test_dosing/            unit tests
  test_mainboard/         unit tests
  test_integration/       both applications running on simulated time
arduino/                  generated, ready-to-upload Arduino IDE sketches
tools/export_arduino.py   regenerates arduino/ from lib/ and src/
docs/                     calibration, testing, protocol notes, v2.2 review
legacy/v2.2/              the previous firmware, unchanged
```

### Architecture

```
            +------------------ lib/ (pure C++, unit tested) -------------------+
            |                                                                   |
 loop() --> |  DosingApp / MainboardApp   <-- console commands (ParamConsole)   |
            |     |  uses: DosingEngine, ProductionCycle, PeriodicTimer,        |
            |     |        PulseFrequencyMeter, ParamStore, EcophiReporter ...  |
            |     v                                                             |
            |  hardware interfaces: Clock, FlowSensor, PumpDriver, MainboardIo, |
            |  DosingLinkPort, CharacterDisplay, EepromDevice, TextOutput       |
            +-----------------------------|-------------------------------------+
                                          |
              src/ (AVR drivers)          |          test/support (fakes)
          StepperPump (Timer1 ISR)  <-----+----->    FakePump, FakeFlowSensor,
          InterruptFlowSensor (INT1)                 FakeClock, FakeEeprom ...
          BoardIo, LcdDisplay, I2C
```

Each application only talks to hardware through small interfaces. On the board these
are implemented in `src/`; in the tests they are replaced by fakes that run on a
simulated clock. Global objects never touch hardware in their constructors: everything
is initialised in `setup()`, after the Arduino core.

## Building and flashing

### PlatformIO (recommended)

Install [PlatformIO](https://platformio.org/install) (for example the VS Code extension), then:

```sh
pio run -e dosing -t upload      # Arduino Uno   (dosing board)
pio run -e mainboard -t upload   # Arduino Mega  (main board)
pio device monitor               # console, 9600 baud
```

### Arduino IDE

Ready-to-upload sketches are in [`arduino/`](arduino). No Clara libraries have to be installed:

1. Install **LiquidCrystal I2C** (Frank de Brabander) from the Library Manager (main board only).
2. Open `arduino/ClaraDosing/ClaraDosing.ino`, select **Arduino Uno**, and click Upload.
3. Open `arduino/ClaraMainboard/ClaraMainboard.ino`, select **Arduino Mega or Mega 2560**,
   and click Upload.

The code is in the `*_main.cpp` tab and the `src/` folder of each sketch; the `.ino`
file is intentionally almost empty. These files are **generated** from `lib/` and `src/`:
after changing the sources, run `python3 tools/export_arduino.py` (CI fails if
`arduino/` is out of date).

## Running the tests

```sh
make test               # g++ with AddressSanitizer/UBSan; fetches Unity on first run
# or
pio test -e native
```

## First start after upgrading from v2.2

1. Flash both boards.
2. The main board imports the v2.2 settling and transfer times from EEPROM. The
   production time goes back to 180 min (v2.2 always used 180, whatever was set).
3. The dosing board starts with the v2.2 defaults (target FRC 1.5 mg/L, NaClO 4.5 g/L,
   1.2 mL/rev, flowmeter k 0.5 with the 1.02·Q + 8.61 correction), and prints
   `warning: no calibration in EEPROM`. Run the pump calibration (and ideally the
   flowmeter calibration); see [`docs/CALIBRATION.md`](docs/CALIBRATION.md).
4. **Measure the free chlorine in the treated water after the upgrade.** v2.2
   delivered less NaClO than it calculated (see the review), especially at high flow.
   v3 delivers what it calculates, so the residual chlorine may be higher than before.
   Adjust `coef_low` / `coef_high` or `target_frc` if needed.

## Behaviour changes compared with v2.2

| Area | v2.2 | v3 |
|---|---|---|
| Timing | re-configured hardware timers, 59-second minutes | crystal-accurate `millis()` timing |
| Pump steps | busy loop in `loop()`, slowed by other work | Timer1 interrupt; exact step count |
| Dosing calibration | sent from main board over I2C, but the frame was always rejected, so hard-coded values were used | stored in the dosing board's EEPROM, set from its USB console |
| Production time | always 180 min (EEPROM value never loaded) | `production_min` parameter |
| Power cut during production | batch restarted | batch resumes (progress saved every 10 min) |
| Storage tank full after settling | shown as "Settling" indefinitely | explicit "Tank full" state, transfer starts when there is space |
| Level sensors | array comparisons that read past the buffers | 10 s consecutive-sample filter (`level_debounce`) |
| NaClO tank empty | pump stopped at the next 20 s decision | pump stops within 1 s; LCD shows "NaClO tank EMPTY" |
| Ecophi `naclo` field | estimate: flow × target ratio | volume actually pumped (mL/min) |
| Ecophi period | ~8 s (timer side effects) | `report_interval`, default 60 s |
| LCD | `clear()` every 2 s (flicker), FRC shown as "g/L" | in-place updates, FRC in mg/L, link-loss and alarm messages |
| Flow coefficient at exactly 160 L/min | 1.0 (gap in if/else chain) | 1.35 |

## Protocols

* **Main ↔ dosing board (I2C, address 0x21):** the main board requests a 28-byte
  telemetry frame every second (versioned, CRC-8). See
  [`lib/clara_common/src/clara/dosing_link.h`](lib/clara_common/src/clara/dosing_link.h).
* **Main board → Ecophi (RS485, 9600 baud):**
  `;flow,voltage,L1,L2,L3,naclo,frc,active_cl,ph:`, unchanged from v2.2. See
  [`lib/clara_mainboard/src/clara/mainboard/ecophi_report.h`](lib/clara_mainboard/src/clara/mainboard/ecophi_report.h).
* **Level sensor 3 (NaClO tank):** main board pin 9 → dosing board A3, HIGH = NaClO available.

## Pin assignments

Unchanged from v2.2; see [`src/mainboard/pins.h`](src/mainboard/pins.h) and
[`src/dosing/pins.h`](src/dosing/pins.h).
