#include "clara/dosing/dosing_console.h"

#include "clara/progmem.h"

namespace clara {
namespace dosing {

namespace {

const uint8_t kMaxTokens = 4;

bool is(const char* token, const char* flashText) { return clara_strcasecmp_P(token, flashText) == 0; }

}  // namespace

DosingConsole::DosingConsole(DosingApp& app, TextOutput& out)
    : app_(app),
      out_(out),
      reader_(),
      paramConsole_(app.params(), out),
      logDecisions_(false),
      lastDecisionPrinted_(0),
      lastMode_(kModeAutomatic) {}

void DosingConsole::printBanner() {
  out_.printLineFlash(CLARA_F("Clara dosing board v3.0 - type 'help'"));
  const ParamPersistence::LoadResult load = app_.status().calibrationLoad;
  if (load == ParamPersistence::kLoadRepaired) {
    out_.printLineFlash(CLARA_F("warning: some calibration values were invalid and reset to defaults"));
  } else if (load != ParamPersistence::kLoadOk) {
    out_.printLineFlash(CLARA_F("warning: no calibration in EEPROM, factory defaults in use"));
  }
}

void DosingConsole::onChar(char c) {
  if (reader_.push(c)) execute(reader_.line());
}

void DosingConsole::update() {
  const DosingStatus& status = app_.status();
  if (logDecisions_ && status.decisionCount != lastDecisionPrinted_) printDecision(status.lastDecision);
  lastDecisionPrinted_ = status.decisionCount;

  if (lastMode_ != kModeAutomatic && status.mode == kModeAutomatic) {
    if (lastMode_ == kModePumpCalibration && status.pumpCalibrationRevs > 0.0f) {
      out_.printLineFlash(CLARA_F("pump run finished: measure the volume, then 'pumpcal done <mL>'"));
    } else {
      out_.printLineFlash(CLARA_F("pump run finished, automatic dosing resumed"));
    }
  }
  lastMode_ = status.mode;
}

void DosingConsole::execute(char* line) {
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
  } else if (is(tokens[0], CLARA_F("dose"))) {
    float ml;
    if (count == 2 && parseFloat(tokens[1], ml) && app_.dispense(ml)) {
      out_.printLineFlash(CLARA_F("dosing... ('stop' aborts)"));
    } else {
      out_.printLineFlash(CLARA_F("usage: dose <mL>   (0 < mL <= 1000)"));
    }
  } else if (is(tokens[0], CLARA_F("pumpcal"))) {
    handlePumpCal(tokens, count);
  } else if (is(tokens[0], CLARA_F("flowcal"))) {
    handleFlowCal(tokens, count);
  } else if (is(tokens[0], CLARA_F("flowsim"))) {
    handleFlowSim(tokens, count);
  } else if (is(tokens[0], CLARA_F("stop"))) {
    app_.stopPump();
    out_.printLineFlash(CLARA_F("pump stopped"));
  } else if (is(tokens[0], CLARA_F("log"))) {
    logDecisions_ = count == 2 && is(tokens[1], CLARA_F("on"));
    out_.printLineFlash(logDecisions_ ? CLARA_F("decision log on") : CLARA_F("decision log off"));
  } else {
    out_.printLineFlash(CLARA_F("unknown command, type 'help'"));
  }
}

void DosingConsole::printHelp() {
  out_.printLineFlash(CLARA_F("Clara dosing board commands:"));
  out_.printLineFlash(CLARA_F("  status                live values"));
  paramConsole_.printHelp();
  out_.printLineFlash(CLARA_F("  dose <mL>             pump a fixed volume (test)"));
  out_.printLineFlash(CLARA_F("  pumpcal <revs>        run pump N revolutions, then"));
  out_.printLineFlash(CLARA_F("  pumpcal done <mL>     enter measured volume -> ml_per_rev"));
  out_.printLineFlash(CLARA_F("  flowcal start         count flowmeter pulses, then"));
  out_.printLineFlash(CLARA_F("  flowcal done <L>      enter collected volume -> flow_k"));
  out_.printLineFlash(CLARA_F("  flowcal cancel"));
  out_.printLineFlash(CLARA_F("  flowsim <L/min>|off   simulate flow (bench test)"));
  out_.printLineFlash(CLARA_F("  stop                  stop the pump"));
  out_.printLineFlash(CLARA_F("  log on|off            print each dosing decision"));
}

void DosingConsole::printStatus() {
  const DosingStatus& s = app_.status();
  out_.printFlash(CLARA_F("mode: "));
  out_.printLineFlash(s.mode == kModeAutomatic ? CLARA_F("automatic")
                                               : (s.mode == kModeManualDose ? CLARA_F("manual dose")
                                                                            : CLARA_F("pump calibration")));
  out_.printFlash(CLARA_F("flow: "));
  out_.printFloat(s.flowLpm, 2);
  out_.printFlash(CLARA_F(" L/min ("));
  out_.printFloat(s.frequencyHz, 3);
  out_.printFlash(s.flowSimulated ? CLARA_F(" Hz, SIMULATED)") : CLARA_F(" Hz)"));
  out_.newline();
  out_.printFlash(CLARA_F("water total: "));
  out_.printFloat(s.totalWaterLiters, 1);
  out_.printFlash(CLARA_F(" L   NaClO total: "));
  out_.printFloat(s.totalNaclOMl, 1);
  out_.printLineFlash(CLARA_F(" mL"));
  out_.printFlash(CLARA_F("NaClO available: "));
  out_.printLineFlash(s.chemicalAvailable ? CLARA_F("yes") : CLARA_F("NO - dosing stopped"));
  out_.printFlash(CLARA_F("pump queue: "));
  out_.printUInt(app_.pendingSteps());
  out_.printFlash(CLARA_F(" steps at "));
  out_.printUInt(s.lastDecision.stepRateHz);
  out_.printFlash(CLARA_F(" steps/s"));
  out_.printLineFlash(s.saturated ? CLARA_F("  SATURATED (under-dosing!)") : CLARA_F(""));
  out_.printFlash(CLARA_F("last interval: "));
  printDecision(s.lastDecision);
  if (s.flowCalibrationActive) {
    out_.printFlash(CLARA_F("flowcal pulses: "));
    out_.printUInt(s.flowCalibrationPulses);
    out_.newline();
  }
}

void DosingConsole::printDecision(const DoseDecision& d) {
  out_.printFloat(d.waterLiters, 2);
  out_.printFlash(CLARA_F(" L @ "));
  out_.printFloat(d.averageFlowLpm, 1);
  out_.printFlash(CLARA_F(" L/min x"));
  out_.printFloat(d.coefficient, 2);
  out_.printFlash(CLARA_F(" -> "));
  out_.printFloat(d.doseMl, 3);
  out_.printFlash(CLARA_F(" mL = "));
  out_.printUInt(d.stepsToAdd);
  out_.printFlash(CLARA_F(" steps"));
  if (!d.chemicalAvailable) out_.printFlash(CLARA_F(" (no NaClO)"));
  out_.newline();
}

void DosingConsole::handlePumpCal(char** tokens, uint8_t count) {
  float value;
  if (count == 2 && parseFloat(tokens[1], value)) {
    if (app_.startPumpCalibration(value)) {
      out_.printLineFlash(CLARA_F("pump running - collect the output in a measuring cylinder"));
    } else {
      out_.printLineFlash(CLARA_F("error: revolutions must be 0 < revs <= 1000"));
    }
    return;
  }
  if (count == 3 && is(tokens[1], CLARA_F("done")) && parseFloat(tokens[2], value)) {
    float mlPerRev;
    if (app_.finishPumpCalibration(value, mlPerRev)) {
      out_.printFlash(CLARA_F("ok: ml_per_rev = "));
      out_.printFloat(mlPerRev, 4);
      out_.printLineFlash(CLARA_F(" (saved)"));
    } else {
      out_.printLineFlash(CLARA_F("error: run 'pumpcal <revs>' first and wait until it finishes"));
    }
    return;
  }
  out_.printLineFlash(CLARA_F("usage: pumpcal <revs> | pumpcal done <mL>"));
}

void DosingConsole::handleFlowCal(char** tokens, uint8_t count) {
  if (count == 2 && is(tokens[1], CLARA_F("start"))) {
    app_.startFlowCalibration();
    out_.printLineFlash(CLARA_F("counting pulses - collect at least 50 L, then 'flowcal done <L>'"));
    return;
  }
  if (count == 2 && is(tokens[1], CLARA_F("cancel"))) {
    app_.cancelFlowCalibration();
    out_.printLineFlash(CLARA_F("flow calibration cancelled"));
    return;
  }
  float liters;
  if (count == 3 && is(tokens[1], CLARA_F("done")) && parseFloat(tokens[2], liters)) {
    float kFactor = 0.0f;
    uint32_t pulses = 0;
    if (app_.finishFlowCalibration(liters, kFactor, pulses)) {
      out_.printFlash(CLARA_F("ok: "));
      out_.printUInt(pulses);
      out_.printFlash(CLARA_F(" pulses, flow_k = "));
      out_.printFloat(kFactor, 4);
      out_.printLineFlash(CLARA_F(" Hz/(L/min), correction reset to gain 1 offset 0 (saved)"));
      if (pulses < 100) out_.printLineFlash(CLARA_F("warning: fewer than 100 pulses, repeat with more water"));
    } else {
      out_.printLineFlash(CLARA_F("error: run 'flowcal start' first; result must be in range"));
    }
    return;
  }
  out_.printLineFlash(CLARA_F("usage: flowcal start | flowcal done <L> | flowcal cancel"));
}

void DosingConsole::handleFlowSim(char** tokens, uint8_t count) {
  float lpm;
  if (count == 2 && is(tokens[1], CLARA_F("off"))) {
    app_.setFlowSimulation(false, 0.0f);
    out_.printLineFlash(CLARA_F("flow simulation off"));
  } else if (count == 2 && parseFloat(tokens[1], lpm) && lpm >= 0.0f) {
    app_.setFlowSimulation(true, lpm);
    out_.printLineFlash(CLARA_F("flow simulation on (not saved; 'flowsim off' to end)"));
  } else {
    out_.printLineFlash(CLARA_F("usage: flowsim <L/min> | flowsim off"));
  }
}

}  // namespace dosing
}  // namespace clara
