/**
 * @file production_cycle.h
 * @brief State machine of the on-site NaClO (sodium hypochlorite) production.
 *
 * @code
 *            production bottle full (L1)
 *   STANDBY ---------------------------> PRODUCTION  (electrolysis + fan on)
 *      ^                                     | production_min elapsed
 *      |                                     v       (polarity reversed every N batches)
 *      |                                  SETTLING
 *      |                                     | settling_min elapsed
 *      |            storage tank full (L2)   v
 *      |          +-------------------- [tank full?] ---------+
 *      |          v                                           | no
 *      |   WAITING_FOR_SPACE --- storage tank not full -----> |
 *      |                                                      v
 *      +----------- transfer_min elapsed ------------- TRANSFERRING (valve open)
 * @endcode
 *
 * Timing is based on elapsed milliseconds, not on counting ticks, so it has the
 * accuracy of the crystal (v2.2 counted 59-second "minutes" of a re-configured
 * hardware timer). Consecutive phases are chained on their exact deadlines, so
 * main-loop latency does not add up over a cycle.
 */
#ifndef CLARA_PRODUCTION_CYCLE_H
#define CLARA_PRODUCTION_CYCLE_H

#include <stdint.h>

namespace clara {
namespace mainboard {

/** Process states. Numeric values 2..5 are the v2.2 state numbers. */
enum ProcessState {
  kStateStandby = 2,
  kStateProduction = 3,
  kStateSettling = 4,
  kStateTransferring = 5,
  kStateWaitingForSpace = 6,
};

struct CycleTimes {
  uint32_t productionMs;
  uint32_t settlingMs;
  uint32_t transferMs;
  uint8_t polarityCycles;  ///< Reverse polarity after every N completed batches.
};

struct CycleInputs {
  bool productionBottleFull;  ///< Level sensor 1: brine present in the production bottle.
  bool storageTankFull;       ///< Level sensor 2: NaClO storage tank full.
};

struct CycleOutputs {
  bool electrolysis;      ///< Electrode power relay.
  bool fan;               ///< Cooling / hydrogen venting fan.
  bool polarityReversed;  ///< Electrode polarity relay (only switched while electrolysis is off).
  bool transferValve;     ///< Valve from production bottle to storage tank.
};

/** Progress persisted in EEPROM so a power cut does not restart a batch. */
struct CycleProgress {
  uint8_t state;
  uint8_t polarityReversed;
  uint16_t elapsedMinutes;
  uint32_t completedCycles;
};

const uint8_t kCycleProgressSize = 8;
void encodeProgress(const CycleProgress& progress, uint8_t* bytes);
CycleProgress decodeProgress(const uint8_t* bytes);

/** How often the progress is saved while a timed phase runs. With 8 EEPROM
 *  slots this is ~6 500 writes per slot and year (EEPROM is rated 100 000). */
const uint32_t kProgressSavePeriodMs = 10ul * 60ul * 1000ul;

class ProductionCycle {
 public:
  ProductionCycle();

  void configure(const CycleTimes& times) { times_ = times; }

  /** Starts in STANDBY. */
  void begin(uint32_t nowMs);

  /**
   * Resumes a saved batch. The elapsed time is restored with minute resolution
   * (rounded down), so a phase is extended rather than shortened.
   */
  void restore(const CycleProgress& progress, uint32_t nowMs);

  /** Evaluates transitions; call on every main-loop pass. */
  void update(uint32_t nowMs, const CycleInputs& inputs);

  /** Jumps to a state (bench testing from the console). */
  void forceState(ProcessState state, uint32_t nowMs);

  ProcessState state() const { return state_; }
  CycleOutputs outputs() const;

  /** Time spent in the current state (including restored time). */
  uint32_t elapsedMs(uint32_t nowMs) const { return nowMs - stateStartMs_ + restoredMs_; }
  /** Time left in a timed state, 0 otherwise. */
  uint32_t remainingMs(uint32_t nowMs) const;
  /** remainingMs() rounded up to whole minutes (what the LCD shows). */
  uint16_t remainingMinutes(uint32_t nowMs) const;

  uint32_t completedCycles() const { return completedCycles_; }

  CycleProgress progress(uint32_t nowMs) const;

  /** @return true (once) when the progress should be written to EEPROM. */
  bool takePersistRequest(uint32_t nowMs);

 private:
  void enter(ProcessState state, uint32_t startMs, uint32_t restoredMs = 0);
  /** Duration of a timed state; 0 for untimed states. */
  uint32_t durationOf(ProcessState state) const;
  /** Exact end time of the current timed state. */
  uint32_t deadlineMs() const { return stateStartMs_ + (durationOf(state_) - restoredMs_); }

  CycleTimes times_;
  ProcessState state_;
  uint32_t stateStartMs_;
  uint32_t restoredMs_;
  bool polarityReversed_;
  uint32_t completedCycles_;
  bool persistPending_;
  uint32_t lastPersistMs_;
};

/** Human readable state name (flash string). */
const char* stateName(ProcessState state);

}  // namespace mainboard
}  // namespace clara

#endif  // CLARA_PRODUCTION_CYCLE_H
