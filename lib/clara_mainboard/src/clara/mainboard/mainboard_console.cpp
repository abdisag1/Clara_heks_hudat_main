#include "clara/mainboard/mainboard_console.h"

#include "clara/progmem.h"

namespace clara {
namespace mainboard {

namespace {

const uint8_t kMaxTokens = 4;

bool is(const char* token, const char* flashText) { return clara_strcasecmp_P(token, flashText) == 0; }

}  // namespace

MainboardConsole::MainboardConsole(MainboardApp& app, TextOutput& out)
    : app_(app), out_(out), reader_(), paramConsole_(app.params(), out) {}

void MainboardConsole::printBanner() {
  // No ';' in console output: the Ecophi receiver on the shared line ignores it.
  out_.printLineFlash(CLARA_F("Clara main board v3.0 - type help"));
  const MainboardStatus& status = app_.status();
  if (status.legacyImported) out_.printLineFlash(CLARA_F("imported settling and transfer time from v2.2"));
  if (status.calibrationLoad != ParamPersistence::kLoadOk) {
    out_.printLineFlash(CLARA_F("warning - no valid calibration in EEPROM, defaults in use"));
  }
  if (status.progressRestored) out_.printLineFlash(CLARA_F("resumed the batch interrupted by a power cut"));
}

void MainboardConsole::onChar(char c) {
  if (reader_.push(c)) execute(reader_.line());
}

void MainboardConsole::execute(char* line) {
  char* tokens[kMaxTokens];
  const uint8_t count = splitTokens(line, tokens, kMaxTokens);
  if (count == 0) return;

  bool changed = false;
  if (paramConsole_.handle(tokens, count, changed)) {
    if (changed) app_.saveAndApplyParams();
    return;
  }

  if (is(tokens[0], CLARA_F("help")) || is(tokens[0], CLARA_F("cal"))) {
    printHelp();
  } else if (is(tokens[0], CLARA_F("status"))) {
    printStatus();
  } else if (is(tokens[0], CLARA_F("force"))) {
    handleForce(tokens, count);
  } else if (is(tokens[0], CLARA_F("report"))) {
    app_.sendReportNow();
    out_.newline();
  } else {
    out_.printLineFlash(CLARA_F("unknown command, type help"));
  }
}

void MainboardConsole::printHelp() {
  out_.printLineFlash(CLARA_F("Clara main board commands"));
  out_.printLineFlash(CLARA_F("  status                live values"));
  paramConsole_.printHelp();
  out_.printLineFlash(CLARA_F("  force <standby|production|settling|transfer>"));
  out_.printLineFlash(CLARA_F("                        jump to a state (bench test)"));
  out_.printLineFlash(CLARA_F("  report                send an Ecophi frame now"));
  out_.printLineFlash(CLARA_F("Dosing calibration is done on the dosing board USB port."));
}

void MainboardConsole::printStatus() {
  const MainboardStatus& s = app_.status();
  const ProductionCycle& cycle = app_.cycle();
  const uint32_t nowMs = app_.now();

  out_.printFlash(CLARA_F("state: "));
  out_.printFlash(stateName(cycle.state()));
  out_.printFlash(CLARA_F("  remaining "));
  out_.printUInt(cycle.remainingMs(nowMs) / 1000u);
  out_.printFlash(CLARA_F(" s  batches "));
  out_.printUInt(cycle.completedCycles());
  out_.printFlash(cycle.outputs().polarityReversed ? CLARA_F("  polarity reversed") : CLARA_F("  polarity normal"));
  out_.newline();

  out_.printFlash(CLARA_F("levels L1 "));
  out_.printUInt(s.levels[0]);
  out_.printFlash(CLARA_F(" L2 "));
  out_.printUInt(s.levels[1]);
  out_.printFlash(CLARA_F(" L3 "));
  out_.printUInt(s.levels[2]);
  out_.printFlash(CLARA_F("   voltage "));
  out_.printFloat(s.voltage, 2);
  out_.printLineFlash(CLARA_F(" V"));

  out_.printFlash(CLARA_F("dosing link "));
  out_.printFlash(s.linkOk ? CLARA_F("ok") : CLARA_F("LOST"));
  out_.printFlash(CLARA_F(" (errors "));
  out_.printUInt(s.linkErrors);
  out_.printLineFlash(CLARA_F(")"));
  if (s.linkOk) {
    out_.printFlash(CLARA_F("  flow "));
    out_.printFloat(s.dosing.flowLpm, 2);
    out_.printFlash(CLARA_F(" L/min  last dose "));
    out_.printFloat(s.dosing.lastDoseMl, 2);
    out_.printFlash(CLARA_F(" mL  water "));
    out_.printUInt(s.dosing.totalWaterL);
    out_.printFlash(CLARA_F(" L  NaClO "));
    out_.printFloat(static_cast<float>(s.dosing.totalNaclOMicroL) / 1000.0f, 1);
    out_.printLineFlash(CLARA_F(" mL"));
    if (s.dosing.flags & link::kFlagPumpSaturated) out_.printLineFlash(CLARA_F("  WARNING pump saturated"));
    if (s.dosing.flags & link::kFlagConfigDefaults) {
      out_.printLineFlash(CLARA_F("  WARNING dosing board uses default calibration"));
    }
    if (s.dosing.flags & link::kFlagFlowSimulated) out_.printLineFlash(CLARA_F("  NOTE flow is simulated"));
  }
}

void MainboardConsole::handleForce(char** tokens, uint8_t count) {
  if (count == 2) {
    if (is(tokens[1], CLARA_F("standby"))) {
      app_.forceState(kStateStandby);
    } else if (is(tokens[1], CLARA_F("production"))) {
      app_.forceState(kStateProduction);
    } else if (is(tokens[1], CLARA_F("settling"))) {
      app_.forceState(kStateSettling);
    } else if (is(tokens[1], CLARA_F("transfer"))) {
      app_.forceState(kStateTransferring);
    } else {
      count = 0;  // fall through to usage
    }
    if (count == 2) {
      out_.printFlash(CLARA_F("ok - state "));
      out_.printLineFlash(stateName(app_.cycle().state()));
      return;
    }
  }
  out_.printLineFlash(CLARA_F("usage - force <standby|production|settling|transfer>"));
}

}  // namespace mainboard
}  // namespace clara
