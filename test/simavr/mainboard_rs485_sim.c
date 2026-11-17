/*
 * Firmware-in-the-loop test of the main board's RS485 direction control: runs
 * the real, compiled main-board firmware (.elf for the Mega 2560) in simavr.
 *
 *  - records every byte the firmware sends on UART0 together with the levels
 *    of DE (pin 2 = PE4) and RE (pin 5 = PE3) at that moment,
 *  - checks that DE is released only after the last byte has been shifted out,
 *  - types commands into UART0 as the USB console or the RS485 unit would.
 *
 * Checks: every byte leaves with DE HIGH and RE HIGH (transmitter on, receiver
 * off, so no echo); after power-up the receiver is off (USB console); after
 * "remote" the transceiver idles with DE LOW and RE LOW (listening); commands
 * received in that state are answered, and the transceiver listens again.
 *
 * Build and run:  test/simavr/run_mainboard_sim.sh [firmware.elf]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <simavr/avr_ioport.h>
#include <simavr/avr_uart.h>
#include <simavr/sim_avr.h>
#include <simavr/sim_elf.h>

static const double kFcpu = 16000000.0;
static int g_de = -1, g_re = -1;
static char g_out[8192];
static size_t g_len = 0;
static int g_bad_bytes = 0;

static avr_t *g_avr;
static avr_cycle_count_t g_last_byte_cycle = 0;
static avr_cycle_count_t g_min_release_gap = (avr_cycle_count_t)-1;  /* last byte written -> DE LOW */

static void on_de(struct avr_irq_t *irq, uint32_t value, void *param) {
  (void)irq;
  (void)param;
  if (g_de == 1 && value == 0 && g_last_byte_cycle != 0) {
    const avr_cycle_count_t gap = g_avr->cycle - g_last_byte_cycle;
    if (gap < g_min_release_gap) g_min_release_gap = gap;
  }
  g_de = (int)value;
}
static void on_re(struct avr_irq_t *irq, uint32_t value, void *param) { (void)irq; (void)param; g_re = (int)value; }

static void on_uart_out(struct avr_irq_t *irq, uint32_t value, void *param) {
  (void)irq;
  (void)param;
  if (g_de != 1 || g_re != 1) ++g_bad_bytes;  // sent while not transmitting, or with the receiver on
  g_last_byte_cycle = g_avr->cycle;
  if (g_len < sizeof(g_out) - 1) g_out[g_len++] = (char)value;
  g_out[g_len] = '\0';
}

static avr_irq_t *g_uart_in;

static int run_until(double seconds) {
  const avr_cycle_count_t end = (avr_cycle_count_t)(seconds * kFcpu);
  while (g_avr->cycle < end) {
    const int state = avr_run(g_avr);
    if (state == cpu_Done || state == cpu_Crashed) {
      fprintf(stderr, "firmware stopped (state %d) at %.3f s\n", state, g_avr->cycle / kFcpu);
      return 0;
    }
  }
  return 1;
}

static void type_line(const char *line) {
  for (const char *p = line; *p; ++p) avr_raise_irq(g_uart_in, (uint8_t)*p);
  avr_raise_irq(g_uart_in, '\n');
}

static int check(int ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  return ok ? 0 : 1;
}

int main(int argc, char **argv) {
  const char *elf = argc > 1 ? argv[1] : ".pio/build/mainboard/firmware.elf";
  elf_firmware_t firmware;
  memset(&firmware, 0, sizeof(firmware));
  if (elf_read_firmware(elf, &firmware) != 0) {
    fprintf(stderr, "cannot read %s\n", elf);
    return 2;
  }
  strcpy(firmware.mmcu, "atmega2560");
  firmware.frequency = (uint32_t)kFcpu;
  g_avr = avr_make_mcu_by_name(firmware.mmcu);
  avr_init(g_avr);
  avr_load_firmware(g_avr, &firmware);
  g_avr->log = LOG_ERROR;

  avr_irq_register_notify(avr_io_getirq(g_avr, AVR_IOCTL_IOPORT_GETIRQ('E'), 4), on_de, NULL);
  avr_irq_register_notify(avr_io_getirq(g_avr, AVR_IOCTL_IOPORT_GETIRQ('E'), 3), on_re, NULL);
  avr_irq_register_notify(avr_io_getirq(g_avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT), on_uart_out, NULL);
  g_uart_in = avr_io_getirq(g_avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_INPUT);
  /* Real UARTs never deliver bytes faster than the line rate; simavr's FIFO does. */
  uint32_t flags = 0;
  avr_ioctl(g_avr, AVR_IOCTL_UART_GET_FLAGS('0'), &flags);
  flags &= ~AVR_UART_FLAG_STDIO;
  avr_ioctl(g_avr, AVR_IOCTL_UART_SET_FLAGS('0'), &flags);

  int failures = 0;
  if (!run_until(1.0)) return 2;
  failures += check(strstr(g_out, "Clara main board v3.1") != NULL, "banner sent");
  failures += check(g_de == 1 && g_re == 1, "after power-up: DE HIGH, RE HIGH (receiver off, USB console)");

  g_len = 0;
  type_line("remote");
  if (!run_until(1.5)) return 2;
  failures += check(strstr(g_out, "ok - commands now come from RS485") != NULL, "remote command answered");
  failures += check(g_de == 0 && g_re == 0, "then idle listening: DE LOW, RE LOW");

  g_len = 0;
  type_line("get water_ph");
  if (!run_until(2.0)) return 2;
  failures += check(strstr(g_out, "water_ph = 7.10") != NULL, "command received over RS485 answered");
  failures += check(g_de == 0 && g_re == 0, "listening again after the answer");

  g_len = 0;
  type_line("report");
  if (!run_until(2.5)) return 2;
  failures += check(g_out[0] == ';' && strchr(g_out, ':') != NULL, "report frame sent");
  failures += check(g_de == 0 && g_re == 0, "listening again after the report");

  failures += check(g_bad_bytes == 0, "every byte sent with DE HIGH and RE HIGH");
  /* One character at 115200 baud (10 bits) lasts 1389 CPU cycles: DE must stay
   * HIGH at least that long after the last byte was handed to the UART. */
  printf("      shortest time from last byte to DE LOW: %.1f us\n", g_min_release_gap / kFcpu * 1e6);
  failures += check(g_min_release_gap != (avr_cycle_count_t)-1 && g_min_release_gap >= 1389,
                    "DE released only after the last byte has left the UART");
  return failures == 0 ? 0 : 1;
}
