/**
 * @file periodic_timer.h
 * @brief Drift-free, roll-over-safe software timer driven by millis().
 */
#ifndef CLARA_PERIODIC_TIMER_H
#define CLARA_PERIODIC_TIMER_H

#include <stdint.h>

namespace clara {

/**
 * Fires once per period, without accumulating drift.
 *
 * The common Arduino idiom
 * @code
 *   if (now - last >= period) { last = now; ... }
 * @endcode
 * re-arms from the moment the check *happened*. Every tick therefore inherits
 * the latency of the main loop and the error accumulates (a 1 s tick serviced
 * 3 ms late on average loses ~4 minutes per day). This class re-arms from the
 * moment the tick was *due* (previous deadline + period), so loop latency never
 * accumulates and the long-term rate equals the crystal accuracy.
 *
 * All arithmetic is unsigned 32-bit, which is correct across the millis()
 * roll-over (every ~49.7 days).
 *
 * The clock is passed in by the caller, so unit tests can drive the timer with
 * simulated time.
 */
class PeriodicTimer {
 public:
  explicit PeriodicTimer(uint32_t periodMs = 1000)
      : periodMs_(periodMs == 0 ? 1 : periodMs), deadlineBaseMs_(0), running_(false) {}

  /** Starts (or restarts) the timer; the first tick is due one period later. */
  void start(uint32_t nowMs) {
    deadlineBaseMs_ = nowMs;
    running_ = true;
  }

  void stop() { running_ = false; }
  bool running() const { return running_; }

  /** Changes the period. Takes effect relative to the last deadline. */
  void setPeriod(uint32_t periodMs) { periodMs_ = periodMs == 0 ? 1 : periodMs; }
  uint32_t period() const { return periodMs_; }

  /**
   * @return true once for every period that has elapsed.
   *
   * If the caller fell behind by two or more periods (e.g. a long blocking
   * call), the missed ticks are dropped and the timer re-aligns to @p nowMs so
   * callers never receive a burst of catch-up ticks. Code that needs the true
   * elapsed time should measure it (see elapsedSince()) rather than count ticks.
   */
  bool poll(uint32_t nowMs) {
    if (!running_) return false;
    const uint32_t elapsed = nowMs - deadlineBaseMs_;
    if (elapsed < periodMs_) return false;
    if (elapsed >= 2u * periodMs_) {
      deadlineBaseMs_ = nowMs;  // overrun: re-align instead of bursting
    } else {
      deadlineBaseMs_ += periodMs_;  // normal case: keep the exact cadence
    }
    return true;
  }

 private:
  uint32_t periodMs_;
  uint32_t deadlineBaseMs_;
  bool running_;
};

/** Roll-over-safe elapsed time between two millis()/micros() readings. */
inline uint32_t elapsedSince(uint32_t startTime, uint32_t now) { return now - startTime; }

}  // namespace clara

#endif  // CLARA_PERIODIC_TIMER_H
