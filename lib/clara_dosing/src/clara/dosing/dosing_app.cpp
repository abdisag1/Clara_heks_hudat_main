#include "clara/dosing/dosing_app.h"

namespace clara {
namespace dosing {

DosingApp::DosingApp(const Clock& clock, FlowSensor& flowSensor, PumpDriver& pump, DigitalInput& chemicalLevel,
                     EepromDevice& eeprom, TelemetrySink& telemetry)
    : clock_(clock),
      flowSensor_(flowSensor),
      pump_(pump),
      chemicalLevel_(chemicalLevel),
      telemetry_(telemetry),
      params_(parameterTable(), kParamCount, paramValues_),
      persistence_(eeprom, kEepromParamsAddress),
      flowCalibration_(),
      frequencyMeter_(),
      flowTimer_(kFlowSamplePeriodMs),
      lastFlowSampleMs_(0),
      executedStepsSeen_(0),
      flowCalibrationStartCount_(0),
      telemetrySequence_(0),
      lastCommandFrame_(),
      lastCommandMs_(0),
      lastReplyFrame_(),
      commandCount_(0),
      restartRequested_(false),
      status_() {}

void DosingApp::begin() {
  status_.calibrationLoad = persistence_.load(params_);
  if (status_.calibrationLoad == ParamPersistence::kLoadExtended) {
    persistence_.save(params_);  // record from an older firmware: add the new parameters
    status_.calibrationLoad = ParamPersistence::kLoadOk;
  }
  applyParams();

  const uint32_t nowMs = clock_.millis();
  frequencyMeter_.reset(flowSensor_.snapshot());
  executedStepsSeen_ = pump_.executedSteps();
  lastFlowSampleMs_ = nowMs;
  flowTimer_.start(nowMs);
  engine_.start(nowMs);
  status_.mode = kModeAutomatic;
  status_.chemicalAvailable = chemicalLevel_.read();
  publishTelemetry(nowMs);
}

void DosingApp::applyParams() {
  flowCalibration_.kFactorHzPerLpm = params_.get(kFlowKFactor);
  flowCalibration_.correctionThresholdLpm = params_.get(kFlowCorrThreshold);
  flowCalibration_.correctionGain = params_.get(kFlowCorrGain);
  flowCalibration_.correctionOffsetLpm = params_.get(kFlowCorrOffset);
  engine_.configure(doseSettingsFromParams(params_), static_cast<uint32_t>(params_.get(kDoseIntervalS) * 1000.0f),
                    static_cast<uint32_t>(params_.get(kPumpMaxStepRate)));
}

void DosingApp::saveAndApplyParams() {
  persistence_.save(params_);
  status_.calibrationLoad = ParamPersistence::kLoadOk;
  applyParams();
}

SetResult DosingApp::setParameter(uint8_t index, float value) {
  if (index >= params_.count()) return kSetUnknownParameter;
  if (index == kFlowMeterSize && (value == 2.0f || value == 3.0f)) {
    return selectFlowMeter(static_cast<uint8_t>(value)) ? kSetOk : kSetOutOfRange;
  }
  const SetResult result = params_.set(index, value);
  if (result == kSetOk) saveAndApplyParams();
  return result;
}

void DosingApp::restoreDefaults() {
  params_.resetToDefaults();
  saveAndApplyParams();
}

void DosingApp::update() {
  const uint32_t nowMs = clock_.millis();

  serviceCommands();

  if (flowTimer_.poll(nowMs)) {
    sampleFlow(nowMs);
    // NaClO tank ran empty: stop at once instead of at the end of the interval.
    if (!status_.chemicalAvailable && status_.mode == kModeAutomatic && pump_.pendingSteps() > 0) pump_.abort();
    accountPumpedVolume();
    publishTelemetry(nowMs);
  }

  // A manual run is finished when its steps are done.
  if (status_.mode != kModeAutomatic && pump_.pendingSteps() == 0) {
    accountPumpedVolume();
    status_.mode = kModeAutomatic;
  }

  DoseDecision decision;
  if (engine_.update(nowMs, status_.chemicalAvailable, pump_.pendingSteps(), decision)) {
    if (status_.mode != kModeAutomatic) {
      // Operator test in progress: the water of this interval is not dosed.
      decision.doseMl = 0.0f;
      decision.stepsToAdd = 0;
      decision.stepRateHz = 0;
      decision.saturated = false;
    } else if (!decision.chemicalAvailable) {
      pump_.abort();  // nothing to pump: do not run the pump dry
    } else {
      pump_.setStepRate(decision.stepRateHz);
      pump_.addSteps(decision.stepsToAdd);
    }
    status_.lastDecision = decision;
    status_.saturated = decision.saturated;
    ++status_.decisionCount;
  }
}

void DosingApp::sampleFlow(uint32_t nowMs) {
  const PulseSnapshot snapshot = flowSensor_.snapshot();
  status_.frequencyHz = frequencyMeter_.update(snapshot, clock_.micros());
  status_.flowLpm = status_.flowSimulated ? status_.simulatedFlowLpm
                                          : frequencyToFlowLpm(flowCalibration_, status_.frequencyHz);
  if (status_.flowCalibrationActive) status_.flowCalibrationPulses = snapshot.count - flowCalibrationStartCount_;

  // Integrate over the measured time since the previous sample, so a late
  // sample does not lose water.
  const uint32_t elapsedMs = nowMs - lastFlowSampleMs_;
  lastFlowSampleMs_ = nowMs;
  const float liters = status_.flowLpm * static_cast<float>(elapsedMs) / 60000.0f;
  totalWater_.add(liters);
  engine_.addWater(liters);
  status_.totalWaterLiters = totalWater_.liters();

  status_.chemicalAvailable = chemicalLevel_.read();
}

void DosingApp::accountPumpedVolume() {
  const uint32_t executed = pump_.executedSteps();
  const uint32_t newSteps = executed - executedStepsSeen_;
  executedStepsSeen_ = executed;
  // Steps of manual runs go into a measuring cylinder, not into the water.
  if (status_.mode != kModeAutomatic || newSteps == 0) return;
  const float stepsPerMlValue = engine_.settings().stepsPerMl;
  if (stepsPerMlValue > 0.0f) {
    totalNaclOMicroLiters_.add(static_cast<float>(newSteps) * 1000.0f / stepsPerMlValue);
    status_.totalNaclOMl = totalNaclOMicroLiters_.liters() / 1000.0f;
  }
}

void DosingApp::publishTelemetry(uint32_t nowMs) {
  link::DosingTelemetry telemetry;
  telemetry.sequence = ++telemetrySequence_;
  telemetry.flags = 0;
  if (status_.chemicalAvailable) telemetry.flags |= link::kFlagChemicalAvailable;
  if (pump_.pendingSteps() > 0) telemetry.flags |= link::kFlagPumpRunning;
  if (status_.saturated) telemetry.flags |= link::kFlagPumpSaturated;
  if (status_.calibrationLoad != ParamPersistence::kLoadOk) telemetry.flags |= link::kFlagConfigDefaults;
  if (status_.flowSimulated) telemetry.flags |= link::kFlagFlowSimulated;
  if (status_.mode != kModeAutomatic) telemetry.flags |= link::kFlagManualMode;
  if (status_.flowCalibrationActive) telemetry.flags |= link::kFlagFlowCalibration;
  telemetry.flags |= link::kFlagCommands;
  telemetry.flowLpm = status_.flowLpm;
  telemetry.lastDoseMl = status_.lastDecision.doseMl;
  telemetry.totalWaterL = totalWater_.wholeLiters();
  telemetry.totalNaclOMicroL = totalNaclOMicroLiters_.wholeLiters();  // units: uL
  telemetry.targetFrcMgL = params_.get(kTargetFrc);
  telemetry.uptimeS = nowMs / 1000u;

  uint8_t frame[link::kTelemetryFrameSize];
  link::encodeTelemetry(telemetry, frame);
  telemetry_.publish(frame, sizeof(frame));
}

bool DosingApp::startManualRun(Mode mode, uint32_t steps) {
  if (steps == 0) return false;
  pump_.abort();
  accountPumpedVolume();  // attribute any automatic steps before switching mode
  status_.mode = mode;
  pump_.setStepRate(static_cast<uint32_t>(params_.get(kPumpTestStepRate)));
  pump_.addSteps(steps);
  return true;
}

bool DosingApp::dispense(float ml) {
  if (!(ml > 0.0f) || ml > 1000.0f) return false;
  StepQuantizer quantizer;
  return startManualRun(kModeManualDose, quantizer.toSteps(ml, engine_.settings().stepsPerMl));
}

bool DosingApp::startPumpCalibration(float revolutions) {
  if (!(revolutions > 0.0f) || revolutions > 1000.0f) return false;
  const uint32_t steps = static_cast<uint32_t>(revolutions * params_.get(kPumpStepsPerRev) + 0.5f);
  if (!startManualRun(kModePumpCalibration, steps)) return false;
  status_.pumpCalibrationRevs = revolutions;
  return true;
}

bool DosingApp::finishPumpCalibration(float measuredMl, float& mlPerRev) {
  if (status_.pumpCalibrationRevs <= 0.0f || status_.mode != kModeAutomatic) return false;
  mlPerRev = mlPerRevFromMeasurement(status_.pumpCalibrationRevs, measuredMl);
  if (params_.set(kPumpMlPerRev, mlPerRev) != kSetOk) return false;
  status_.pumpCalibrationRevs = 0.0f;
  saveAndApplyParams();
  return true;
}

void DosingApp::stopPump() {
  pump_.abort();
  accountPumpedVolume();
  status_.mode = kModeAutomatic;
}

void DosingApp::setFlowSimulation(bool enabled, float flowLpm) {
  status_.flowSimulated = enabled;
  status_.simulatedFlowLpm = enabled && flowLpm > 0.0f ? flowLpm : 0.0f;
}

bool DosingApp::selectFlowMeter(uint8_t sizeInch, float kFactorOverride) {
  const FlowMeterPreset* preset = findFlowMeterPreset(sizeInch);
  if (preset == 0) return false;
  const float kFactor = kFactorOverride > 0.0f ? kFactorOverride : preset->kFactorHzPerLpm;
  if (!params_.isValid(kFlowKFactor, kFactor)) return false;
  params_.set(kFlowMeterSize, sizeInch);
  params_.set(kFlowKFactor, kFactor);
  // The datasheet relation is linear: no correction curve.
  params_.set(kFlowCorrGain, 1.0f);
  params_.set(kFlowCorrOffset, 0.0f);
  saveAndApplyParams();
  frequencyMeter_.reset(flowSensor_.snapshot());
  return true;
}

void DosingApp::startFlowCalibration() {
  flowCalibrationStartCount_ = flowSensor_.snapshot().count;
  status_.flowCalibrationPulses = 0;
  status_.flowCalibrationActive = true;
}

bool DosingApp::finishFlowCalibration(float liters, float& kFactor, uint32_t& pulses) {
  if (!status_.flowCalibrationActive) return false;
  pulses = flowSensor_.snapshot().count - flowCalibrationStartCount_;
  kFactor = kFactorFromBucketTest(pulses, liters);
  if (params_.set(kFlowKFactor, kFactor) != kSetOk) return false;
  params_.set(kFlowCorrGain, 1.0f);
  params_.set(kFlowCorrOffset, 0.0f);
  status_.flowCalibrationActive = false;
  saveAndApplyParams();
  return true;
}

void DosingApp::serviceCommands() {
  uint8_t frame[link::kCommandFrameSize + 1];
  const uint8_t length = telemetry_.takeCommand(frame, sizeof(frame));
  link::CommandFrame command;
  if (length == 0 || !link::decodeCommand(frame, length, command)) return;  // nothing, or noise: no reply
  // The same frame again, shortly after: the master missed the reply. Serve
  // the same reply again without executing twice (e.g. a second pump run).
  const uint32_t nowMs = clock_.millis();
  bool repeat = nowMs - lastCommandMs_ < kCommandRepeatWindowMs;
  for (uint8_t i = 0; i < link::kCommandFrameSize && repeat; ++i) repeat = frame[i] == lastCommandFrame_[i];
  if (!repeat) {
    const link::CommandReply reply = execute(command);
    link::encodeReply(reply, lastReplyFrame_);
    for (uint8_t i = 0; i < link::kCommandFrameSize; ++i) lastCommandFrame_[i] = frame[i];
    ++commandCount_;
  }
  lastCommandMs_ = nowMs;
  telemetry_.publishReply(lastReplyFrame_, sizeof(lastReplyFrame_));
}

link::CommandReply DosingApp::execute(const link::CommandFrame& command) {
  link::CommandReply reply;
  reply.sequence = command.sequence;
  reply.command = command.command;
  reply.result = link::kResultOk;
  reply.index = command.index;
  reply.value = 0.0f;
  reply.extra = 0;

  switch (command.command) {
    case link::kCmdGetParam:
      if (command.index >= params_.count()) {
        reply.result = link::kResultBadIndex;
      } else {
        reply.value = params_.get(command.index);
      }
      reply.extra = params_.count();
      break;
    case link::kCmdSetParam: {
      const SetResult result = setParameter(command.index, command.value);
      reply.result = result == kSetOk ? link::kResultOk
                                      : (result == kSetUnknownParameter ? link::kResultBadIndex : link::kResultOutOfRange);
      reply.value = params_.get(command.index);
      break;
    }
    case link::kCmdDefaults:
      restoreDefaults();
      break;
    case link::kCmdDose:
      if (!dispense(command.value)) reply.result = link::kResultOutOfRange;
      break;
    case link::kCmdStop:
      stopPump();
      break;
    case link::kCmdPumpCalStart:
      if (!startPumpCalibration(command.value)) reply.result = link::kResultOutOfRange;
      break;
    case link::kCmdPumpCalDone: {
      float mlPerRev = 0.0f;
      if (finishPumpCalibration(command.value, mlPerRev)) {
        reply.value = mlPerRev;
      } else {
        reply.result = link::kResultNotReady;
      }
      break;
    }
    case link::kCmdFlowCalStart:
      startFlowCalibration();
      break;
    case link::kCmdFlowCalDone: {
      float kFactor = 0.0f;
      uint32_t pulses = 0;
      if (finishFlowCalibration(command.value, kFactor, pulses)) {
        reply.value = kFactor;
        reply.extra = pulses;
      } else {
        reply.result = link::kResultNotReady;
      }
      break;
    }
    case link::kCmdFlowCalCancel:
      cancelFlowCalibration();
      break;
    case link::kCmdSelectMeter:
      if (command.index == 0) {
        params_.set(kFlowMeterSize, 0.0f);  // label only: keep the current calibration
        saveAndApplyParams();
      } else if (findFlowMeterPreset(command.index) == 0) {
        reply.result = link::kResultBadIndex;
      } else if (!selectFlowMeter(command.index, command.value)) {
        reply.result = link::kResultOutOfRange;
      }
      reply.value = params_.get(kFlowKFactor);
      break;
    case link::kCmdFlowSim:
      if (command.index != 0 && !(command.value >= 0.0f)) {
        reply.result = link::kResultOutOfRange;
      } else {
        setFlowSimulation(command.index != 0, command.value);
      }
      break;
    case link::kCmdRestart:
      requestRestart();
      break;
    default:
      reply.result = link::kResultUnknownCommand;
      break;
  }
  return reply;
}

}  // namespace dosing
}  // namespace clara
