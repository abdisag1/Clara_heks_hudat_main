/**
 * @file dosing_engine.h
 * @brief Flow-proportional dosing control loop (hardware independent).
 */
#ifndef CLARA_DOSING_ENGINE_H
#define CLARA_DOSING_ENGINE_H

#include <stdint.h>

#include "clara/dosing/dose_calculator.h"
#include "clara/periodic_timer.h"

namespace clara {
namespace dosing {

/** Everything decided at the end of one dosing interval (also used for logging). */
struct DoseDecision {
  float waterLiters;       ///< Water measured during the interval.
  float averageFlowLpm;    ///< waterLiters / interval duration.
  float coefficient;       ///< Dose multiplier that was applied.
  float doseMl;            ///< NaClO commanded for this interval.
  uint32_t stepsToAdd;     ///< Steps to append to the pump queue.
  uint32_t stepRateHz;     ///< Pump speed for the coming interval.
  bool chemicalAvailable;  ///< False: NaClO tank empty, nothing dosed, queue must be flushed.
  bool saturated;          ///< The pump cannot deliver the demand even at max speed.
};

/**
 * Flow-paced dosing with a step budget.
 *
 * The flow task reports every litre of water through addWater(). At the end
 * of each interval the engine converts the water of that interval into pump
 * steps and chooses a speed that spreads the queued steps over ~95 % of the
 * next interval. The number of steps (hence the volume of NaClO) is exact and
 * independent of main-loop timing; only the speed is time-based. v2.2 generated
 * the steps in a busy-wait inside loop(), so every Serial.print slowed the pump
 * and the delivered dose fell below the computed one.
 */
class DosingEngine {
 public:
  DosingEngine();

  /** @param intervalMs control period. @param maxStepRateHz pump speed limit. */
  void configure(const DoseSettings& settings, uint32_t intervalMs, uint32_t maxStepRateHz);

  /** Starts the interval timer. Water added before start() is discarded. */
  void start(uint32_t nowMs);

  /** Adds water measured by the flow task. */
  void addWater(float liters);

  /**
   * Call on every main-loop pass.
   * @param pendingSteps steps still queued in the pump from earlier intervals.
   * @return true when an interval ended and @p decision was filled in.
   */
  bool update(uint32_t nowMs, bool chemicalAvailable, uint32_t pendingSteps, DoseDecision& decision);

  const DoseSettings& settings() const { return settings_; }
  uint32_t intervalMs() const { return timer_.period(); }

 private:
  DoseSettings settings_;
  PeriodicTimer timer_;
  StepQuantizer quantizer_;
  uint32_t maxStepRateHz_;
  uint32_t intervalStartMs_;
  float waterLiters_;
};

/**
 * Step rate that runs @p steps within @p windowMs (rounded up), clamped to
 * @p maxRateHz. Returns 0 when there is nothing to do.
 */
uint32_t stepRateForWindow(uint32_t steps, uint32_t windowMs, uint32_t maxRateHz);

}  // namespace dosing
}  // namespace clara

#endif  // CLARA_DOSING_ENGINE_H
