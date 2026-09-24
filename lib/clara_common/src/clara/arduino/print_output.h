/**
 * @file print_output.h
 * @brief Adapts an Arduino Print (Serial, SoftwareSerial, ...) to TextOutput.
 *
 * Header-only and guarded by ARDUINO so host builds never see it.
 */
#ifndef CLARA_ARDUINO_PRINT_OUTPUT_H
#define CLARA_ARDUINO_PRINT_OUTPUT_H

#if defined(ARDUINO)
#include <Arduino.h>

#include "clara/text_output.h"

namespace clara {

class PrintOutput : public TextOutput {
 public:
  explicit PrintOutput(Print& sink) : sink_(sink) {}
  void write(const char* text) override { sink_.print(text); }
  void writeFlash(const char* flashText) override {
    sink_.print(reinterpret_cast<const __FlashStringHelper*>(flashText));
  }

 private:
  Print& sink_;
};

}  // namespace clara

#endif  // ARDUINO
#endif  // CLARA_ARDUINO_PRINT_OUTPUT_H
