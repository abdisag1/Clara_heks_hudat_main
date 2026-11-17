#!/bin/sh
# Firmware-in-the-loop test of the main board's RS485 DE/RE control in simavr.
# Requires simavr + libsimavr-dev (Debian/Ubuntu: apt install simavr libsimavr-dev libelf-dev).
#
#   pio run -e mainboard && test/simavr/run_mainboard_sim.sh
#   test/simavr/run_mainboard_sim.sh path/to/firmware.elf
set -e
HERE=$(dirname "$0")
ELF=${1:-.pio/build/mainboard/firmware.elf}
OUT=${TMPDIR:-/tmp}/clara_mainboard_sim
cc -O2 -o "$OUT" "$HERE/mainboard_rs485_sim.c" -lsimavr -lelf
"$OUT" "$ELF"
