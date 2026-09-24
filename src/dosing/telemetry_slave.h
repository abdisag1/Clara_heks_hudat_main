/**
 * @file telemetry_slave.h
 * @brief I2C slave that serves the latest telemetry frame to the main board.
 */
#ifndef CLARA_TELEMETRY_SLAVE_H
#define CLARA_TELEMETRY_SLAVE_H

#include <Arduino.h>

#include "clara/dosing/dosing_hal.h"

class I2cTelemetrySlave : public clara::dosing::TelemetrySink {
 public:
  /** Joins the I2C bus as slave kDosingBoardAddress; call from setup(). */
  void begin();

  /** Stores @p frame as the answer to the next request (thread-safe). */
  void publish(const uint8_t* frame, uint8_t length) override;
};

#endif  // CLARA_TELEMETRY_SLAVE_H
