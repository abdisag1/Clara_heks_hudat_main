#!/bin/sh
# Firmware-in-the-loop test of the dosing board in the simavr simulator.
# Requires simavr + libsimavr-dev (Debian/Ubuntu: apt install simavr libsimavr-dev libelf-dev).
#
#   pio run -e dosing && test/simavr/run_dosing_sim.sh
#   test/simavr/run_dosing_sim.sh path/to/firmware.elf
set -e
HERE=$(dirname "$0")
ELF=${1:-.pio/build/dosing/firmware.elf}
OUT=${TMPDIR:-/tmp}/clara_dosing_sim
cc -O2 -o "$OUT" "$HERE/dosing_sim.c" -lsimavr -lelf
for HZ in 10 50 90; do
  "$OUT" "$ELF" "$HZ" 59.5
done
