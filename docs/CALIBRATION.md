# Calibration guide

Every tunable value is a **parameter**: it has a name, a unit, an allowed range and a
factory default, and it is stored in EEPROM with a checksum. You change parameters from
a serial terminal (Arduino Serial Monitor, `pio device monitor`, PuTTY, ...) at
**9600 baud, line ending "Newline"**.

| Command | Effect |
|---|---|
| `help` (or `cal`) | list the commands |
| `get` | list all parameters: number, name, value, unit, range, description |
| `get <name>` | show one parameter |
| `set <name> <value>` | change a parameter; saved at once, effective at once |
| `set <number> <value>` | same, using the number shown by `get` |
| `defaults` | restore the factory values |
| `status` | live values |

Out-of-range values are rejected with the allowed range, so a typo cannot break the
device.

---

## Dosing board (Arduino Uno, USB port)

### How the dose is calculated

```
flow            Q     = f / flow_k                          (f = flowmeter frequency in Hz)
                Q     = flow_corr_gain * Q + flow_corr_offset   if Q >= flow_corr_min
water per interval  V = integral of Q over dose_interval (20 s)
dose ratio          r = target_frc / naclo_strength          mL NaClO per L water
coefficient         c = coef_low  if Q <= coef_band, else coef_high
NaClO               N = V * r * c                              mL
pump steps          S = N * steps_per_rev / ml_per_rev
```

Example with the defaults at 100 L/min for 20 s: V = 33.3 L, r = 1.5 / 4.5 = 0.333 mL/L,
c = 1.35, N = 15 mL, S = 80 000 steps, spread over the next ~19 s.

### Parameters

| # | Name | Default | Unit | Meaning |
|---|---|---|---|---|
| 0 | `target_frc` | 1.5 | mg/L | chlorine to add to the water |
| 1 | `naclo_strength` | 4.5 | g/L | active chlorine in the produced NaClO |
| 2 | `dose_interval` | 20 | s | dosing control period |
| 3 | `steps_per_rev` | 6400 | steps | driver microsteps per pump revolution |
| 4 | `ml_per_rev` | 1.2 | mL | pump output per revolution (set by `pumpcal`) |
| 5 | `flow_k` | 0.5 | Hz/(L/min) | flowmeter constant (set by `flowcal`) |
| 6 | `flow_corr_min` | 5 | L/min | apply the correction above this flow |
| 7 | `flow_corr_gain` | 1.02 | - | Q = gain·Q + offset |
| 8 | `flow_corr_offset` | 8.61 | L/min | Q = gain·Q + offset |
| 9 | `coef_low` | 1.45 | - | dose multiplier at or below `coef_band` |
| 10 | `coef_high` | 1.35 | - | dose multiplier above `coef_band` |
| 11 | `coef_band` | 60 | L/min | flow separating the multipliers |
| 12 | `max_step_rate` | 10000 | steps/s | pump speed limit |
| 13 | `test_step_rate` | 3200 | steps/s | pump speed for `dose` and `pumpcal` |

### 1. Pump calibration (do this first, and after changing tubing)

You need a measuring cylinder (at least 25 mL, better 50 mL).

1. Prime the pump so the tube is full of liquid (`dose 5`, repeat until liquid comes out).
2. Put the outlet in the empty measuring cylinder.
3. Type `pumpcal 20`. The pump turns exactly 20 revolutions (~40 s at the test speed).
   Automatic dosing pauses meanwhile.
4. When the console prints `pump run finished`, read the volume, e.g. 24.6 mL.
5. Type `pumpcal done 24.6`. The firmware computes `ml_per_rev = 24.6 / 20 = 1.23` and saves it.
6. Check: `dose 20` must now deliver 20 mL (±2 %).

The pumped volume depends a little on speed and back pressure. For the best accuracy,
calibrate against the real injection point, and set `test_step_rate` close to the
typical operating speed (`status` shows the current step rate).

### 2. Flowmeter calibration ("bucket test", optional)

You need a container of known volume (at least 50 L, the more the better).

1. Let water flow into the container at a normal operating flow rate, and type
   `flowcal start` at the moment you start filling.
2. Type `flowcal done <litres>` when you stop, e.g. `flowcal done 200`.
3. The firmware computes `flow_k = pulses / litres / 60`, saves it and resets the linear
   correction to identity (`flow_corr_gain` 1, `flow_corr_offset` 0), because a
   one-point calibration replaces the old curve.

Use `flowcal cancel` to abandon a test. Repeat if fewer than 100 pulses were counted.

If you prefer to keep a two-point curve (like the factory 1.02·Q + 8.61), determine it
against a reference meter at two flows and set `flow_k`, `flow_corr_gain` and
`flow_corr_offset` by hand.

### 3. Target chlorine

* `set naclo_strength <g/L>`: measure the active chlorine of a fresh batch with a
  test kit and enter it. (It depends on salt amount, production time and electrode
  condition.)
* `set target_frc <mg/L>`: the chlorine to add per litre of water.
* Fine-tune with `coef_low` / `coef_high` until the free residual chlorine measured
  at the end of the network matches your target. These multipliers compensate the
  chlorine demand of the water.

### 4. Verification

* `status`: flow, totals, pump queue and speed. `SATURATED` means the pump cannot keep
  up (raise `max_step_rate` if the pump allows it, or use a bigger pump head).
* `log on`: prints every 20 s decision, e.g.
  `33.35 L @ 100.0 L/min x1.35 -> 15.008 mL = 80044 steps`.
* `flowsim 100`: simulates 100 L/min without water, to check the whole chain on the
  bench (`flowsim off` to end; the main board shows "NOTE flow is simulated").

---

## Main board (Arduino Mega, USB port)

The main board's serial port is shared with the RS485 line to the Ecophi unit. Console
output also goes out on RS485; the Ecophi parser ignores it (it never starts with `;`).

| # | Name | Default | Unit | Meaning |
|---|---|---|---|---|
| 0 | `production_min` | 180 | min | electrolysis time per batch |
| 1 | `settling_min` | 5 | min | settling time after electrolysis |
| 2 | `transfer_min` | 10 | min | valve open time to transfer a batch |
| 3 | `polarity_cycles` | 1 | - | reverse electrode polarity every N batches |
| 4 | `report_interval` | 60 | s | Ecophi report period |
| 5 | `level_debounce` | 10 | s | a level sensor must be stable this long |
| 6 | `voltage_ref` | 4.85 | V | ADC reference: measure the Mega's 5 V pin |
| 7 | `voltage_divider` | 22.2 | - | voltage sensor divider ratio |

**Voltage calibration:** measure the battery with a multimeter, compare with `status`, and
scale `voltage_divider` by (multimeter / displayed).

**Bench tests:** `force production`, `force settling`, `force transfer` and
`force standby` jump directly to a state so you can check each relay. `report` sends an
Ecophi frame immediately.
