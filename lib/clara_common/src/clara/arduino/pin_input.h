/**
 * @file pin_input.h
 * @brief DigitalInput backed by an Arduino pin.
 */
#ifndef CLARA_ARDUINO_PIN_INPUT_H
#define CLARA_ARDUINO_PIN_INPUT_H

#if defined(ARDUINO)
#include <Arduino.h>

#include "clara/digital_io.h"

namespace clara {

class PinInput : public DigitalInput {
 public:
  /** @param activeHigh true if HIGH means "active". */
  PinInput(uint8_t pin, bool activeHigh) : pin_(pin), activeHigh_(activeHigh) {}

  /** Configures the pin; call from setup(), never from a global constructor. */
  void begin(uint8_t mode = INPUT) { pinMode(pin_, mode); }

  bool read() const override { return (digitalRead(pin_) == HIGH) == activeHigh_; }

 private:
  uint8_t pin_;
  bool activeHigh_;
};

}  // namespace clara

#endif  // ARDUINO
#endif  // CLARA_ARDUINO_PIN_INPUT_H
