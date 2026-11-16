/**
 * @file telemetry_slave.h
 * @brief I2C slave: serves telemetry to the main board and receives its
 *        calibration commands (protocol in clara/dosing_link.h).
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
  uint32_t requestCount() const override;
  uint8_t takeCommand(uint8_t* frame, uint8_t capacity) override;
  void publishReply(const uint8_t* frame, uint8_t length) override;
};

#endif  // CLARA_TELEMETRY_SLAVE_H
