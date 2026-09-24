#include "clara/mainboard/production_cycle.h"

#include "clara/byte_codec.h"
#include "clara/progmem.h"

namespace clara {
namespace mainboard {

void encodeProgress(const CycleProgress& progress, uint8_t* bytes) {
  bytes[0] = progress.state;
  bytes[1] = progress.polarityReversed;
  putU16(bytes + 2, progress.elapsedMinutes);
  putU32(bytes + 4, progress.completedCycles);
}

CycleProgress decodeProgress(const uint8_t* bytes) {
  CycleProgress progress;
  progress.state = bytes[0];
  progress.polarityReversed = bytes[1];
  progress.elapsedMinutes = getU16(bytes + 2);
  progress.completedCycles = getU32(bytes + 4);
  return progress;
}

ProductionCycle::ProductionCycle()
    : times_(),
      state_(kStateStandby),
      stateStartMs_(0),
      restoredMs_(0),
      polarityReversed_(false),
      completedCycles_(0),
      persistPending_(false),
      lastPersistMs_(0) {
  times_.polarityCycles = 1;
}

void ProductionCycle::begin(uint32_t nowMs) {
  enter(kStateStandby, nowMs);
  persistPending_ = false;  // nothing worth saving yet
}

void ProductionCycle::restore(const CycleProgress& progress, uint32_t nowMs) {
  polarityReversed_ = progress.polarityReversed != 0;
  completedCycles_ = progress.completedCycles;

  const ProcessState saved = static_cast<ProcessState>(progress.state);
  switch (saved) {
    case kStateProduction:
    case kStateSettling:
    case kStateTransferring: {
      uint32_t restored = static_cast<uint32_t>(progress.elapsedMinutes) * 60000ul;
      const uint32_t duration = durationOf(saved);
      if (restored > duration) restored = duration;  // durations may have been shortened since
      enter(saved, nowMs, restored);
      break;
    }
    case kStateWaitingForSpace:
      enter(kStateWaitingForSpace, nowMs);
      break;
    default:
      enter(kStateStandby, nowMs);
      break;
  }
  persistPending_ = false;
}

uint32_t ProductionCycle::durationOf(ProcessState state) const {
  switch (state) {
    case kStateProduction:
      return times_.productionMs;
    case kStateSettling:
      return times_.settlingMs;
    case kStateTransferring:
      return times_.transferMs;
    default:
      return 0;
  }
}

void ProductionCycle::enter(ProcessState state, uint32_t startMs, uint32_t restoredMs) {
  state_ = state;
  stateStartMs_ = startMs;
  restoredMs_ = restoredMs;
  persistPending_ = true;
}

void ProductionCycle::update(uint32_t nowMs, const CycleInputs& inputs) {
  const bool timeUp = durationOf(state_) > 0 ? remainingMs(nowMs) == 0 : true;

  switch (state_) {
    case kStateStandby:
      if (inputs.productionBottleFull) enter(kStateProduction, nowMs);
      break;

    case kStateProduction:
      if (timeUp) {
        ++completedCycles_;
        const uint8_t every = times_.polarityCycles == 0 ? 1 : times_.polarityCycles;
        // Reverse while the electrodes are unpowered (outputs() switches
        // electrolysis off in SETTLING) to avoid wearing the relay contacts.
        if (completedCycles_ % every == 0) polarityReversed_ = !polarityReversed_;
        enter(kStateSettling, deadlineMs());
      }
      break;

    case kStateSettling:
      if (timeUp) {
        const uint32_t deadline = deadlineMs();
        if (inputs.storageTankFull) {
          enter(kStateWaitingForSpace, deadline);
        } else {
          enter(kStateTransferring, deadline);
        }
      }
      break;

    case kStateWaitingForSpace:
      if (!inputs.storageTankFull) enter(kStateTransferring, nowMs);
      break;

    case kStateTransferring:
      if (timeUp) enter(kStateStandby, deadlineMs());
      break;
  }
}

void ProductionCycle::forceState(ProcessState state, uint32_t nowMs) { enter(state, nowMs); }

CycleOutputs ProductionCycle::outputs() const {
  CycleOutputs outputs;
  outputs.electrolysis = state_ == kStateProduction;
  outputs.fan = state_ == kStateProduction;
  outputs.polarityReversed = polarityReversed_;
  outputs.transferValve = state_ == kStateTransferring;
  return outputs;
}

uint32_t ProductionCycle::remainingMs(uint32_t nowMs) const {
  const uint32_t duration = durationOf(state_);
  const uint32_t elapsed = elapsedMs(nowMs);
  return elapsed >= duration ? 0 : duration - elapsed;
}

uint16_t ProductionCycle::remainingMinutes(uint32_t nowMs) const {
  return static_cast<uint16_t>((remainingMs(nowMs) + 59999ul) / 60000ul);
}

CycleProgress ProductionCycle::progress(uint32_t nowMs) const {
  CycleProgress progress;
  progress.state = static_cast<uint8_t>(state_);
  progress.polarityReversed = polarityReversed_ ? 1 : 0;
  const uint32_t minutes = elapsedMs(nowMs) / 60000ul;
  progress.elapsedMinutes = static_cast<uint16_t>(minutes > 0xFFFFu ? 0xFFFFu : minutes);
  progress.completedCycles = completedCycles_;
  return progress;
}

bool ProductionCycle::takePersistRequest(uint32_t nowMs) {
  const bool periodicDue = durationOf(state_) > 0 && nowMs - lastPersistMs_ >= kProgressSavePeriodMs;
  if (!persistPending_ && !periodicDue) return false;
  persistPending_ = false;
  lastPersistMs_ = nowMs;
  return true;
}

const char* stateName(ProcessState state) {
  switch (state) {
    case kStateStandby:
      return CLARA_F("Standby");
    case kStateProduction:
      return CLARA_F("Producing");
    case kStateSettling:
      return CLARA_F("Settling");
    case kStateTransferring:
      return CLARA_F("Transferring");
    case kStateWaitingForSpace:
      return CLARA_F("Tank full");
  }
  return CLARA_F("?");
}

}  // namespace mainboard
}  // namespace clara
