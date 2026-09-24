/**
 * @file dosing_hal.h
 * @brief Hardware interfaces required by the dosing application.
 *
 * The AVR implementations are in src/dosing; unit and integration tests use the
 * fakes in test/support. Keeping hardware behind these interfaces is what makes
 * the complete dosing chain testable on a PC.
 */
#ifndef CLARA_DOSING_HAL_H
#define CLARA_DOSING_HAL_H

#include <stdint.h>

#include "clara/dosing/flow_measurement.h"

namespace clara {
namespace dosing {

/** Flowmeter input: a pulse counter maintained by an interrupt. */
class FlowSensor {
 public:
  /** Returns count and last-pulse time read atomically. */
  virtual PulseSnapshot snapshot() const = 0;

 protected:
  ~FlowSensor() {}
};

/**
 * Stepper-driven peristaltic pump with a step queue. Steps are generated in the
 * background (hardware timer) at the configured rate until the queue is empty.
 */
class PumpDriver {
 public:
  /** Appends @p steps to the queue and starts the pump if needed. */
  virtual void addSteps(uint32_t steps) = 0;
  /** Sets the speed used for the queued steps; 0 pauses the pump. */
  virtual void setStepRate(uint32_t stepsPerSecond) = 0;
  /** Stops immediately and drops the queue. */
  virtual void abort() = 0;
  /** Steps still queued. */
  virtual uint32_t pendingSteps() const = 0;
  /** Steps executed since start-up (monotonic, wraps at 2^32). */
  virtual uint32_t executedSteps() const = 0;

 protected:
  ~PumpDriver() {}
};

/** Receives each encoded telemetry frame (the I2C slave serves the latest one). */
class TelemetrySink {
 public:
  virtual void publish(const uint8_t* frame, uint8_t length) = 0;

 protected:
  ~TelemetrySink() {}
};

}  // namespace dosing
}  // namespace clara

#endif  // CLARA_DOSING_HAL_H
