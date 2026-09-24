/**
 * @file ecophi_report.h
 * @brief Report frames for the Ecophi remote-monitoring unit (RS485, 9600 baud).
 *
 * The frame format is unchanged from v2.2 so the existing Ecophi device and
 * server keep working:
 *
 *   ;<flow>,<voltage>,<L1>,<L2>,<L3>,<naclo>,<frc>,<active_cl>,<ph>:
 *
 * | field     | meaning                                         | format     |
 * |-----------|-------------------------------------------------|------------|
 * | flow      | average water flow over the report period, L/min | 2 decimals |
 * | voltage   | supply voltage, V                               | 2 decimals |
 * | L1..L3    | level sensors (production, storage, NaClO tank) | 0 / 1      |
 * | naclo     | NaClO dosed, mL/min averaged over the period    | 2 decimals |
 * | frc       | target free residual chlorine, mg/L             | 2 decimals |
 * | active_cl | active chlorine sensor (not fitted: 0.00)       | 2 decimals |
 * | ph        | pH sensor (not fitted: 0.00)                    | 2 decimals |
 *
 * Change: v2.2 estimated "naclo" from flow x target ratio; v3 reports the
 * volume the pump actually delivered (it reads 0 when the NaClO tank is empty).
 */
#ifndef CLARA_ECOPHI_REPORT_H
#define CLARA_ECOPHI_REPORT_H

#include <stdint.h>

namespace clara {
namespace mainboard {

struct EcophiFrame {
  float flowLpm;
  float voltage;
  bool level1;
  bool level2;
  bool level3;
  float naclOMlPerMin;
  float targetFrcMgL;
  float activeChlorine;
  float ph;
};

/** Largest frame formatEcophiFrame() can produce, plus the terminator. */
const uint8_t kEcophiFrameCapacity = 112;

/** Formats @p frame; @return the length written. */
uint8_t formatEcophiFrame(const EcophiFrame& frame, char* buffer, uint8_t size);

/** Running average of the 1 Hz samples between two reports. */
class EcophiAverager {
 public:
  EcophiAverager() { reset(); }

  void addSample(float flowLpm, float activeChlorine, float ph);
  void reset();

  float averageFlowLpm() const { return average(flowSum_); }
  float averageActiveChlorine() const { return average(activeChlorineSum_); }
  float averagePh() const { return average(phSum_); }
  uint16_t sampleCount() const { return count_; }

 private:
  float average(float sum) const { return count_ > 0 ? sum / count_ : 0.0f; }

  float flowSum_;
  float activeChlorineSum_;
  float phSum_;
  uint16_t count_;
};

/**
 * NaClO dosing rate from the dosing board's cumulative pumped volume. A
 * decreasing total means the dosing board restarted; the new total then counts
 * as the volume of the period.
 */
class DosedRateMeter {
 public:
  DosedRateMeter() : lastTotalMicroL_(0), lastMs_(0), haveTotal_(false) {}

  /** Starts a period at @p nowMs. */
  void start(uint32_t nowMs) { lastMs_ = nowMs; }
  /** @return mL/min since the previous call and starts a new period. */
  float takeRate(uint32_t totalMicroL, bool totalValid, uint32_t nowMs);

 private:
  uint32_t lastTotalMicroL_;
  uint32_t lastMs_;
  bool haveTotal_;
};

}  // namespace mainboard
}  // namespace clara

#endif  // CLARA_ECOPHI_REPORT_H
