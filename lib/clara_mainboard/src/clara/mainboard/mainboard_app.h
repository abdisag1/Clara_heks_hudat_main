/**
 * @file mainboard_app.h
 * @brief The main board application: production cycle, sensors, dosing-board
 *        link, LCD and Ecophi reporting.
 */
#ifndef CLARA_MAINBOARD_APP_H
#define CLARA_MAINBOARD_APP_H

#include <stdint.h>

#include "clara/clock.h"
#include "clara/debouncer.h"
#include "clara/dosing_link.h"
#include "clara/eeprom_store.h"
#include "clara/mainboard/display_pages.h"
#include "clara/mainboard/ecophi_report.h"
#include "clara/mainboard/main_params.h"
#include "clara/mainboard/mainboard_hal.h"
#include "clara/mainboard/production_cycle.h"
#include "clara/param_store.h"
#include "clara/periodic_timer.h"
#include "clara/text_output.h"

namespace clara {
namespace mainboard {

// --- EEPROM map ----------------------------------------------------------------
// 0 .. 83    v2.2 data (read once to import settling/transfer times)
// 128 ..     calibration parameters (ParamPersistence)
// 256 ..     production progress ring buffer (8 slots x 11 bytes)
const uint16_t kEepromParamsAddress = 128;
const uint16_t kEepromProgressAddress = 256;
const uint8_t kProgressSlots = 8;

// --- Timing --------------------------------------------------------------------
const uint32_t kSensorPeriodMs = 1000;        ///< Level sensors and averages.
const uint32_t kVoltageSamplePeriodMs = 100;  ///< 10 ADC samples per second are averaged.
const uint32_t kLinkPollPeriodMs = 1000;      ///< Dosing board telemetry request.
const uint32_t kLinkTimeoutMs = 5000;         ///< Link declared lost after this silence.
const uint32_t kDisplayRefreshMs = 500;
const uint32_t kDisplayPageMs = 2000;         ///< Alternates the two LCD pages.
const uint32_t kSplashMs = 5000;              ///< Start-up instructions screen.

/** Everything the console and the tests may want to inspect. */
struct MainboardStatus {
  bool levels[kLevelSensorCount];  ///< Debounced level sensors.
  float voltage;
  bool linkOk;
  uint32_t linkErrors;             ///< Failed or corrupt telemetry frames since start-up.
  link::DosingTelemetry dosing;    ///< Last valid telemetry.
  ParamPersistence::LoadResult calibrationLoad;
  bool progressRestored;           ///< A batch was resumed after a power cut.
  bool progressDiscarded;          ///< A batch was interrupted, but resume_batch = 0.
  bool legacyImported;             ///< v2.2 settling/transfer times imported.
};

class MainboardApp {
 public:
  MainboardApp(const Clock& clock, MainboardIo& io, DosingLinkPort& link, CharacterDisplay& display,
               TextOutput& ecophi, EepromDevice& eeprom);

  /** Loads calibration and saved progress, sets the outputs to a safe state. */
  void begin();
  /** One pass of the main loop. */
  void update();

  ParamStore& params() { return params_; }
  void saveAndApplyParams();

  /** Bench test: jump to a process state. */
  void forceState(ProcessState state);
  /**
   * Abandons the running batch: outputs off, STANDBY, and the saved progress
   * is replaced so the batch is not resumed at the next power-up. (If the
   * production bottle still reads full, a new batch starts after the level
   * debounce time.)
   */
  void cancelBatch() { forceState(kStateStandby); }
  /** Sends an Ecophi report immediately (and restarts the report period). */
  void sendReportNow();

  const MainboardStatus& status() const { return status_; }
  const ProductionCycle& cycle() const { return cycle_; }
  uint32_t now() const { return clock_.millis(); }

 private:
  void applyParams();
  void loadCalibration();
  void importLegacySettings();
  void sampleSensors(uint32_t nowMs);
  void pollDosingBoard(uint32_t nowMs);
  void refreshDisplay(uint32_t nowMs);
  void sendReport(uint32_t nowMs);
  void persistProgress(uint32_t nowMs, bool force = false);

  const Clock& clock_;
  MainboardIo& io_;
  DosingLinkPort& link_;
  CharacterDisplay& display_;
  TextOutput& ecophi_;
  EepromDevice& eeprom_;

  float paramValues_[kParamCount];
  ParamStore params_;
  ParamPersistence persistence_;
  RingRecordStore progressStore_;

  ProductionCycle cycle_;
  Debouncer levelFilters_[kLevelSensorCount];

  PeriodicTimer sensorTimer_;
  PeriodicTimer voltageTimer_;
  PeriodicTimer linkTimer_;
  PeriodicTimer displayTimer_;
  PeriodicTimer reportTimer_;

  uint32_t voltageAdcSum_;
  uint8_t voltageSamples_;
  uint32_t lastLinkOkMs_;
  bool everLinked_;
  uint32_t bootMs_;

  EcophiAverager averager_;
  DosedRateMeter dosedRate_;

  MainboardStatus status_;
};

}  // namespace mainboard
}  // namespace clara

#endif  // CLARA_MAINBOARD_APP_H
