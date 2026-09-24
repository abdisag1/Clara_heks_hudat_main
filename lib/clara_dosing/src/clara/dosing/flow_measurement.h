/**
 * @file flow_measurement.h
 * @brief Converts flowmeter pulses into flow rate and volume.
 */
#ifndef CLARA_FLOW_MEASUREMENT_H
#define CLARA_FLOW_MEASUREMENT_H

#include <stdint.h>

namespace clara {
namespace dosing {

/**
 * Flowmeter calibration.
 *
 *   Qraw = f / kFactor                       (f = pulse frequency, Hz)
 *   Q    = gain * Qraw + offset   if Qraw >= threshold
 *   Q    = Qraw                   otherwise
 *
 * The defaults (k = 0.5 for the 3" meter, Q = 1.02 Qraw + 8.61 above 5 L/min)
 * are the v2.2 field values.
 */
struct FlowCalibration {
  float kFactorHzPerLpm;
  float correctionThresholdLpm;
  float correctionGain;
  float correctionOffsetLpm;
};

/** Applies @p calibration to a pulse frequency. Never returns a negative flow. */
float frequencyToFlowLpm(const FlowCalibration& calibration, float frequencyHz);

/** Atomic copy of the pulse counter maintained by the flowmeter interrupt. */
struct PulseSnapshot {
  uint32_t count;        ///< Total pulses since start-up (wraps).
  uint32_t lastPulseUs;  ///< micros() timestamp of the most recent pulse.
};

/**
 * Pulse frequency by the reciprocal (period) method.
 *
 * Counting pulses in a fixed 1 s gate is useless at low flow: at 5 L/min the
 * meter gives 2.5 Hz, so a gate sees 2 or 3 pulses (+-20 %). Instead this meter
 * divides the number of new pulses by the exact time between the first and last
 * of them (both timestamped in the interrupt), which is accurate to a few
 * microseconds at any frequency. When no pulse arrives the estimate decays
 * (the frequency cannot be higher than 1 / time-since-last-pulse) and drops to
 * zero after @c timeoutUs.
 */
class PulseFrequencyMeter {
 public:
  explicit PulseFrequencyMeter(uint32_t timeoutUs = 3000000ul);

  /** Forgets history (e.g. after enabling the interrupt). */
  void reset(const PulseSnapshot& snapshot);

  /** Call periodically (typically 1 Hz). @return frequency in Hz. */
  float update(const PulseSnapshot& snapshot, uint32_t nowUs);

  float frequencyHz() const { return frequencyHz_; }

 private:
  uint32_t timeoutUs_;
  uint32_t lastCount_;
  uint32_t lastPulseUs_;
  bool haveReference_;  ///< lastPulseUs_ is a valid pulse timestamp to measure from
  float frequencyHz_;
};

/**
 * Accumulates volume without losing precision: a float alone cannot add 0.02 L
 * to a total of 100 000 L. Whole litres are kept in an integer and only the
 * fractional part in a float.
 */
class VolumeTotalizer {
 public:
  VolumeTotalizer() : wholeLiters_(0), fraction_(0.0f) {}

  void add(float liters);
  void reset() {
    wholeLiters_ = 0;
    fraction_ = 0.0f;
  }
  uint32_t wholeLiters() const { return wholeLiters_; }
  /** Total as a float (for display; loses precision above ~16 000 m3). */
  float liters() const { return static_cast<float>(wholeLiters_) + fraction_; }

 private:
  uint32_t wholeLiters_;
  float fraction_;
};

/**
 * Flowmeter calibration by collecting a known volume ("bucket test"):
 * pulses per litre = pulses / litres, and since f = Q * k with Q in L/min,
 * k = pulses per litre / 60.
 * @return the k-factor, or 0 if the inputs are unusable.
 */
float kFactorFromBucketTest(uint32_t pulses, float liters);

}  // namespace dosing
}  // namespace clara

#endif  // CLARA_FLOW_MEASUREMENT_H
