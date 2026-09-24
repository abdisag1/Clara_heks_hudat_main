#include "clara/mainboard/ecophi_report.h"

#include "clara/fixed_format.h"

namespace clara {
namespace mainboard {

uint8_t formatEcophiFrame(const EcophiFrame& frame, char* buffer, uint8_t size) {
  if (size == 0) return 0;
  buffer[0] = '\0';
  appendText(buffer, size, ";");
  appendFixed(buffer, size, frame.flowLpm, 2);
  appendText(buffer, size, ",");
  appendFixed(buffer, size, frame.voltage, 2);
  appendText(buffer, size, frame.level1 ? ",1" : ",0");
  appendText(buffer, size, frame.level2 ? ",1" : ",0");
  appendText(buffer, size, frame.level3 ? ",1," : ",0,");
  appendFixed(buffer, size, frame.naclOMlPerMin, 2);
  appendText(buffer, size, ",");
  appendFixed(buffer, size, frame.targetFrcMgL, 2);
  appendText(buffer, size, ",");
  appendFixed(buffer, size, frame.activeChlorine, 2);
  appendText(buffer, size, ",");
  appendFixed(buffer, size, frame.ph, 2);
  return appendText(buffer, size, ":");
}

void EcophiAverager::addSample(float flowLpm, float activeChlorine, float ph) {
  flowSum_ += flowLpm;
  activeChlorineSum_ += activeChlorine;
  phSum_ += ph;
  if (count_ < 0xFFFF) ++count_;
}

void EcophiAverager::reset() {
  flowSum_ = 0.0f;
  activeChlorineSum_ = 0.0f;
  phSum_ = 0.0f;
  count_ = 0;
}

float DosedRateMeter::takeRate(uint32_t totalMicroL, bool totalValid, uint32_t nowMs) {
  const uint32_t elapsedMs = nowMs - lastMs_;
  lastMs_ = nowMs;
  if (!totalValid) return 0.0f;

  uint32_t deltaMicroL;
  if (!haveTotal_) {
    deltaMicroL = 0;  // first report after start-up: no reference yet
  } else if (totalMicroL >= lastTotalMicroL_) {
    deltaMicroL = totalMicroL - lastTotalMicroL_;
  } else {
    deltaMicroL = totalMicroL;  // dosing board restarted
  }
  lastTotalMicroL_ = totalMicroL;
  haveTotal_ = true;

  if (elapsedMs == 0) return 0.0f;
  return static_cast<float>(deltaMicroL) / 1000.0f * 60000.0f / static_cast<float>(elapsedMs);
}

}  // namespace mainboard
}  // namespace clara
