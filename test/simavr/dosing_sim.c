/*
 * Firmware-in-the-loop test: runs the real, compiled dosing-board firmware
 * (the .elf flashed to the Uno) in the simavr ATmega328P simulator.
 *
 *  - drives flowmeter pulses into pin 3 (PD3 / INT1) at a fixed frequency,
 *  - holds the NaClO level input A3 (PC3) high,
 *  - counts rising edges on the pump STEP pin 10 (PB2),
 *  - compares the steps with the dose the calibration predicts.
 *
 * This checks what the host tests cannot: interrupt handlers, Timer1 register
 * set-up, micros(), atomic access and the real main loop timing.
 *
 * Build and run:  test/simavr/run_dosing_sim.sh [firmware.elf]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <simavr/avr_ioport.h>
#include <simavr/sim_avr.h>
#include <simavr/sim_elf.h>

static unsigned long long g_steps = 0;

static void on_step_pin(struct avr_irq_t *irq, uint32_t value, void *param) {
  (void)irq;
  (void)param;
  if (value) ++g_steps;
}

int main(int argc, char **argv) {
  const char *elf = argc > 1 ? argv[1] : ".pio/build/dosing/firmware.elf";
  const double flow_hz = argc > 2 ? atof(argv[2]) : 50.0;
  const double seconds = argc > 3 ? atof(argv[3]) : 59.5;
  const double f_cpu = 16000000.0;

  elf_firmware_t firmware;
  memset(&firmware, 0, sizeof(firmware));
  if (elf_read_firmware(elf, &firmware) != 0) {
    fprintf(stderr, "cannot read %s\n", elf);
    return 2;
  }
  strcpy(firmware.mmcu, "atmega328p");
  firmware.frequency = (uint32_t)f_cpu;

  avr_t *avr = avr_make_mcu_by_name(firmware.mmcu);
  avr_init(avr);
  avr_load_firmware(avr, &firmware);
  avr->log = LOG_ERROR;

  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 2), on_step_pin, NULL);
  avr_irq_t *flow = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 3);
  avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('C'), 3), 1);  // NaClO available

  const avr_cycle_count_t half_period = (avr_cycle_count_t)(f_cpu / (2.0 * flow_hz));
  const avr_cycle_count_t end = (avr_cycle_count_t)(seconds * f_cpu);
  avr_cycle_count_t next_edge = half_period;
  int level = 0;
  while (avr->cycle < end) {
    const int state = avr_run(avr);
    if (state == cpu_Done || state == cpu_Crashed) {
      fprintf(stderr, "firmware stopped (state %d) at %.3f s\n", state, avr->cycle / f_cpu);
      return 2;
    }
    if (avr->cycle >= next_edge) {
      level = !level;
      avr_raise_irq(flow, level);
      next_edge += half_period;
    }
  }

  /* Expected with the default calibration: the decisions at 20 s and 40 s have
   * been pumped by t = 59.5 s. Water of the first second is not measured (the
   * frequency meter needs a reference pulse), so 39 s of flow were dosed. */
  const double raw = flow_hz / 0.5;
  const double lpm = raw >= 5.0 ? 1.02 * raw + 8.61 : raw;
  const double coefficient = lpm <= 60.0 ? 1.45 : 1.35;
  const double dosed_seconds = 39.0;
  const double expected = lpm * dosed_seconds / 60.0 * (1.5 / 4.5) * coefficient * 6400.0 / 1.2;
  const double error = (g_steps - expected) / expected * 100.0;
  printf("flow %.1f Hz (%.2f L/min), %.1f s simulated: %llu steps, expected %.0f (%+.2f %%)\n", flow_hz, lpm,
         seconds, g_steps, expected, error);
  return (error > -1.0 && error < 1.0) ? 0 : 1;
}
