#include "clara/mainboard/display_pages.h"

#include "clara/fixed_format.h"
#include "clara/progmem.h"

namespace clara {
namespace mainboard {

namespace {

void clearLine(LcdLine line) { line[0] = '\0'; }

/** Pads with spaces to exactly kLcdColumns characters. */
void padLine(LcdLine line) {
  uint8_t length = 0;
  while (length < kLcdColumns && line[length] != '\0') ++length;
  while (length < kLcdColumns) line[length++] = ' ';
  line[kLcdColumns] = '\0';
}

void renderSplash(LcdLine lines[kLcdRows]) {
  appendFlash(lines[0], sizeof(LcdLine), CLARA_F("Clara Begins"));
  appendFlash(lines[1], sizeof(LcdLine), CLARA_F("Add 210g of salt"));
  appendFlash(lines[2], sizeof(LcdLine), CLARA_F("& Add 7L of Water"));
  appendFlash(lines[3], sizeof(LcdLine), CLARA_F("Into The Prod Bottle"));
}

void renderDosingPage(const DisplayModel& m, LcdLine lines[kLcdRows]) {
  if (!m.linkOk) {
    appendFlash(lines[0], sizeof(LcdLine), CLARA_F("Dosing board: NO"));
    appendFlash(lines[1], sizeof(LcdLine), CLARA_F("  COMMUNICATION"));
    appendFlash(lines[2], sizeof(LcdLine), CLARA_F("Check I2C cable"));
    return;
  }
  appendFlash(lines[0], sizeof(LcdLine), CLARA_F("Flow: "));
  appendFixed(lines[0], sizeof(LcdLine), m.flowLpm, 2);
  appendFlash(lines[0], sizeof(LcdLine), CLARA_F(" L/min"));

  if (!m.chemicalAvailable) {
    appendFlash(lines[1], sizeof(LcdLine), CLARA_F("NaClO tank EMPTY"));
  } else {
    appendFlash(lines[1], sizeof(LcdLine), CLARA_F("NaClO.Inj: "));
    appendFixed(lines[1], sizeof(LcdLine), m.lastDoseMl, 2);
    appendFlash(lines[1], sizeof(LcdLine), m.pumpSaturated ? CLARA_F(" mL MAX") : CLARA_F(" mL"));
  }

  appendFlash(lines[2], sizeof(LcdLine), CLARA_F("Comm.Flow: "));
  // Fewer decimals as the total grows so the line stays within 20 columns.
  const uint8_t decimals = m.totalWaterM3 < 100.0f ? 2 : (m.totalWaterM3 < 10000.0f ? 1 : 0);
  appendFixed(lines[2], sizeof(LcdLine), m.totalWaterM3, decimals);
  appendFlash(lines[2], sizeof(LcdLine), CLARA_F(" m3"));

  appendFlash(lines[3], sizeof(LcdLine), CLARA_F("Target FRC: "));
  appendFixed(lines[3], sizeof(LcdLine), m.targetFrcMgL, 2);
  appendFlash(lines[3], sizeof(LcdLine), CLARA_F("mg/L"));
}

void renderProcessPage(const DisplayModel& m, LcdLine lines[kLcdRows]) {
  appendFlash(lines[0], sizeof(LcdLine), CLARA_F("State: "));
  appendFlash(lines[0], sizeof(LcdLine), stateName(m.state));

  switch (m.state) {
    case kStateProduction:
    case kStateSettling:
    case kStateTransferring:
      appendFlash(lines[1], sizeof(LcdLine), CLARA_F("Time Left: "));
      appendUInt(lines[1], sizeof(LcdLine), m.remainingMin);
      appendFlash(lines[1], sizeof(LcdLine), CLARA_F(" min"));
      break;
    case kStateWaitingForSpace:
      appendFlash(lines[1], sizeof(LcdLine), CLARA_F("Waiting for space"));
      break;
    case kStateStandby:
      appendFlash(lines[1], sizeof(LcdLine), CLARA_F("Waiting for brine"));
      break;
  }

  appendFlash(lines[2], sizeof(LcdLine), CLARA_F("Voltage: "));
  appendFixed(lines[2], sizeof(LcdLine), m.voltage, 2);
  appendFlash(lines[2], sizeof(LcdLine), CLARA_F(" V"));

  appendFlash(lines[3], sizeof(LcdLine), CLARA_F("L1: "));
  appendFlash(lines[3], sizeof(LcdLine), m.level1 ? CLARA_F("1") : CLARA_F("0"));
  appendFlash(lines[3], sizeof(LcdLine), CLARA_F(" L2: "));
  appendFlash(lines[3], sizeof(LcdLine), m.level2 ? CLARA_F("1") : CLARA_F("0"));
  appendFlash(lines[3], sizeof(LcdLine), CLARA_F(" L3: "));
  appendFlash(lines[3], sizeof(LcdLine), m.level3 ? CLARA_F("1") : CLARA_F("0"));
}

}  // namespace

void renderPage(const DisplayModel& model, uint8_t page, LcdLine lines[kLcdRows]) {
  for (uint8_t row = 0; row < kLcdRows; ++row) clearLine(lines[row]);

  if (model.splash) {
    renderSplash(lines);
  } else if (page % kPageCount == 0) {
    renderDosingPage(model, lines);
  } else {
    renderProcessPage(model, lines);
  }

  for (uint8_t row = 0; row < kLcdRows; ++row) padLine(lines[row]);
}

}  // namespace mainboard
}  // namespace clara
