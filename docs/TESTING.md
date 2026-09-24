# Testing

The firmware is tested at four levels. The first two run on any PC in a few seconds
and should pass before every commit; CI runs them on every push.

| Level | What runs | Where | Command |
|---|---|---|---|
| Unit tests | single classes: timers, dose maths, state machine, EEPROM records, protocol, LCD pages ... | PC | `make test` or `pio test -e native` |
| Integration tests | complete `DosingApp` and `MainboardApp`, alone and connected to each other, on simulated time with fake hardware | PC | same |
| Firmware-in-the-loop | the compiled dosing `.elf` in the simavr ATmega328P simulator, with flowmeter pulses injected | PC (Linux) | `test/simavr/run_dosing_sim.sh` |
| Bench test | real boards on the bench, driven from the serial consoles | hardware | checklist below |

## 1. Unit and integration tests

```sh
make test              # g++ -std=gnu++11 -pedantic with AddressSanitizer + UBSan
make test_dosing       # a single suite
pio test -e native     # the same tests through PlatformIO
```

Suites:

* `test/test_common`: drift-free timer (a simulated day with random loop latency),
  `millis()` roll-over, formatting, CRCs, EEPROM records (corruption, schema change,
  torn writes, wear levelling), console, I2C frame (every single-bit error detected).
* `test/test_dosing`: flow calibration, reciprocal frequency measurement (including
  `micros()` roll-over), dose maths against worked examples, fractional-step carry,
  engine intervals, saturation, empty tank, Timer1 settings accurate to 0.1 % from
  1 to 15 000 steps/s.
* `test/test_mainboard`: production cycle timing to the millisecond, deadline
  chaining, polarity reversal, resume after power cut, Ecophi frame byte-for-byte
  compatible with v2.2, LCD pages (never wider than 20 columns).
* `test/test_integration`:
  * one hour of dosing at constant and changing flow, within 0.5 % of theory;
  * the same with a slow main loop (proves dosing does not depend on loop timing);
  * NaClO tank empty: pump stops within 1 s, resumes after refill;
  * `pumpcal`, `flowcal`, `dose`, `stop`, `flowsim` procedures through the console,
    including calibration surviving a reboot;
  * main board: a full 180 + 5 + 10 min cycle with 10 ms resolution, power cut in the
    middle of production, v2.2 EEPROM import, console changes;
  * both boards connected: telemetry reaches the LCD and the Ecophi frame, level 3
    low stops the pump and raises the LCD alarm, corrupted I2C frames are rejected.

### Writing a new test

All hardware is behind interfaces (`FlowSensor`, `PumpDriver`, `MainboardIo`, ...).
`test/support/fakes.h` has a fake for each; the fakes follow a `FakeClock`, so hours
of operation run in milliseconds:

```cpp
fakes::FakeClock clock;
fakes::FakeEeprom eeprom;
DosingBench bench(clock, eeprom);   // see test_integration/test_main.cpp
bench.app.begin();
bench.flow.setFrequency(50.0);      // flowmeter pulses at 50 Hz
bench.run(10 * 60000);              // ten simulated minutes
TEST_ASSERT_FLOAT_WITHIN(...);
```

## 2. Firmware-in-the-loop (simavr)

```sh
sudo apt install simavr libsimavr-dev libelf-dev
pio run -e dosing
test/simavr/run_dosing_sim.sh            # or: run_dosing_sim.sh path/to/firmware.elf
```

The script runs the real dosing firmware for 59.5 simulated seconds at 10, 50 and
90 Hz of flowmeter signal and counts the STEP pulses on pin 10. Expected output:

```
flow 10.0 Hz (29.01 L/min), 59.5 s simulated: 48607 steps, expected 48608 (-0.00 %)
flow 50.0 Hz (110.61 L/min), 59.5 s simulated: 172551 steps, expected 172552 (-0.00 %)
flow 90.0 Hz (192.21 L/min), 59.5 s simulated: 299850 steps, expected 299848 (+0.00 %)
```

## 3. Bench test checklist (real hardware)

Connect both boards as in the device (I2C, pin 9 → A3, common ground). Open two serial
monitors at 9600 baud: dosing board USB, main board USB.

### Dosing board

| # | Action | Expected |
|---|---|---|
| D1 | power up | banner; `warning: no calibration` on a fresh board |
| D2 | `status` | mode automatic, flow 0.00 |
| D3 | `dose 10` into a measuring cylinder | 10 mL ±2 % after `pumpcal` (below) |
| D4 | `pumpcal 20`, measure, `pumpcal done <mL>` | `ml_per_rev` saved; D3 now accurate |
| D5 | `flowsim 100`, `log on`, wait 60 s | every 20 s: `33.3 L @ 100.0 L/min x1.35 -> 15.0 mL = 80000 steps` (with 1.2 mL/rev), pump runs continuously |
| D6 | pull A3 low (main board L3 empty) | pump stops within 1 s; `status` shows NaClO available: NO |
| D7 | `flowsim off`, run real water | `status` flow matches the reference meter |
| D8 | power cycle | calibration kept (no warning) |

### Main board

| # | Action | Expected |
|---|---|---|
| M1 | power up | splash screen for 5 s, then alternating pages every 2 s |
| M2 | `status` | dosing link ok; flow as on the dosing board |
| M3 | unplug I2C | after 5 s the LCD shows "Dosing board: NO COMMUNICATION" |
| M4 | `set production_min 2`, then `force production` | electrolysis and fan relays on; LCD "Time Left: 2 min" |
| M5 | wait 2 min | relays off, polarity relay toggles, "Settling" 5 min, then valve opens 10 min |
| M6 | L2 (storage tank) high during settling | "Tank full" state, valve stays closed until L2 goes low |
| M7 | power off during production, power on | production continues (banner: "resumed the batch") |
| M8 | watch the RS485 line | a `;...:` frame every `report_interval` seconds |
| M9 | `set production_min 180` | restore the production time |
