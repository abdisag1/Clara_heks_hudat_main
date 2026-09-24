/**
 * @file dose_calculator.h
 * @brief Pure functions that turn a volume of water into pump steps.
 *
 * The chain, with the default calibration:
 *
 *   dose ratio   = target FRC / NaClO strength         1.5 mg/L / 4.5 g/L = 0.333 mL/L
 *   NaClO volume = water x dose ratio x coefficient    100 L x 0.333 x 1.35 = 45 mL
 *   pump steps   = NaClO volume x steps/rev / mL/rev   45 x 6400 / 1.2 = 240 000 steps
 */
#ifndef CLARA_DOSE_CALCULATOR_H
#define CLARA_DOSE_CALCULATOR_H

#include <stdint.h>

#include "clara/param_store.h"

namespace clara {
namespace dosing {

/** The subset of the calibration needed to compute a dose. */
struct DoseSettings {
  float targetFrcMgL;     ///< Target free residual chlorine, mg/L.
  float naclOStrengthGL;  ///< Active chlorine in the NaClO solution, g/L.
  float coefLowFlow;      ///< Dose multiplier for flow <= coefBandLpm.
  float coefHighFlow;     ///< Dose multiplier for flow >  coefBandLpm.
  float coefBandLpm;      ///< Flow separating the two multipliers, L/min.
  float stepsPerMl;       ///< Pump steps per mL of NaClO (steps/rev / mL/rev).
};

/**
 * mL of NaClO solution per litre of water to raise the chlorine by
 * @p targetFrcMgL: (mg/L) / (g/L) = 1e-3 L/L = mL/L.
 */
float doseRatioMlPerL(float targetFrcMgL, float naclOStrengthGL);

/**
 * Empirical dose multiplier (compensates chlorine demand of the water, which
 * matters more at low flow). v2.2 used 1.45 up to 60 L/min and 1.35 above.
 */
float flowCoefficient(const DoseSettings& settings, float averageFlowLpm);

/** NaClO volume (mL) for @p waterLiters flowing at @p averageFlowLpm. */
float naclOVolumeMl(const DoseSettings& settings, float waterLiters, float averageFlowLpm);

/** Pump steps per mL from the pump calibration. Returns 0 for invalid input. */
float stepsPerMl(float stepsPerRevolution, float mlPerRevolution);

/**
 * Pump calibration: after running @p revolutions, the operator measured
 * @p measuredMl in a graduated cylinder. @return mL per revolution, or 0 if
 * the inputs are unusable.
 */
float mlPerRevFromMeasurement(float revolutions, float measuredMl);

/**
 * Converts volumes to whole pump steps and carries the fractional remainder to
 * the next conversion. Truncating every interval (as v2.2 did) systematically
 * under-doses; with the carry the long-term total is exact.
 */
class StepQuantizer {
 public:
  StepQuantizer() : remainder_(0.0f) {}
  uint32_t toSteps(float volumeMl, float stepsPerMlValue);
  void reset() { remainder_ = 0.0f; }
  float remainder() const { return remainder_; }

 private:
  float remainder_;
};

/** Builds DoseSettings from the dosing parameter store. */
DoseSettings doseSettingsFromParams(const ParamStore& params);

}  // namespace dosing
}  // namespace clara

#endif  // CLARA_DOSE_CALCULATOR_H
