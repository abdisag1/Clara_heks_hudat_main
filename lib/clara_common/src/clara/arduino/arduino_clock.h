/**
 * @file arduino_clock.h
 * @brief Clock implementation using the Arduino core's millis()/micros().
 *
 * Timer0 is owned by the Arduino core for these functions; the firmware never
 * reconfigures it (v2.2 did, which changed the time base of everything).
 */
#ifndef CLARA_ARDUINO_CLOCK_H
#define CLARA_ARDUINO_CLOCK_H

#if defined(ARDUINO)
#include <Arduino.h>

#include "clara/clock.h"

namespace clara {

class ArduinoClock : public Clock {
 public:
  uint32_t millis() const override { return ::millis(); }
  uint32_t micros() const override { return ::micros(); }
};

}  // namespace clara

#endif  // ARDUINO
#endif  // CLARA_ARDUINO_CLOCK_H
