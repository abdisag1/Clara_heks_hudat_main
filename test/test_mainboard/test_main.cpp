/**
 * Unit tests for lib/clara_mainboard: production state machine, Ecophi report,
 * LCD pages and sensor conversions.
 */
#include <string.h>
#include <unity.h>

#include <string>

#include "clara/mainboard/display_pages.h"
#include "clara/mainboard/ecophi_report.h"
#include "clara/mainboard/main_params.h"
#include "clara/mainboard/production_cycle.h"
#include "clara/mainboard/voltage.h"

using namespace clara;
using namespace clara::mainboard;

void setUp() {}
void tearDown() {}

namespace {

const uint32_t kMinute = 60000ul;

CycleTimes defaultTimes() {
  CycleTimes t;
  t.productionMs = 180 * kMinute;
  t.settlingMs = 5 * kMinute;
  t.transferMs = 10 * kMinute;
  t.polarityCycles = 1;
  return t;
}

CycleInputs inputs(bool bottleFull, bool tankFull) {
  CycleInputs i;
  i.productionBottleFull = bottleFull;
  i.storageTankFull = tankFull;
  return i;
}

}  // namespace

// --- Parameters -----------------------------------------------------------------

void test_main_defaults_are_valid_and_match_v22() {
  float values[kParamCount];
  ParamStore params(parameterTable(), kParamCount, values);
  for (uint8_t i = 0; i < kParamCount; ++i) TEST_ASSERT_TRUE(params.isValid(i, params.get(i)));
  TEST_ASSERT_EQUAL_FLOAT(180.0f, params.get(kProductionMin));
  TEST_ASSERT_EQUAL_FLOAT(5.0f, params.get(kSettlingMin));
  TEST_ASSERT_EQUAL_FLOAT(10.0f, params.get(kTransferMin));
}

// --- ProductionCycle --------------------------------------------------------------

void test_standby_until_production_bottle_is_full() {
  ProductionCycle cycle;
  cycle.configure(defaultTimes());
  cycle.begin(0);
  cycle.update(1000, inputs(false, false));
  TEST_ASSERT_EQUAL(kStateStandby, cycle.state());
  TEST_ASSERT_FALSE(cycle.outputs().electrolysis);
  cycle.update(2000, inputs(true, false));
  TEST_ASSERT_EQUAL(kStateProduction, cycle.state());
  TEST_ASSERT_TRUE(cycle.outputs().electrolysis);
  TEST_ASSERT_TRUE(cycle.outputs().fan);
}

void test_full_cycle_timing_is_exact() {
  ProductionCycle cycle;
  cycle.configure(defaultTimes());
  cycle.begin(0);
  cycle.update(0, inputs(true, false));

  cycle.update(180 * kMinute - 1, inputs(true, false));
  TEST_ASSERT_EQUAL(kStateProduction, cycle.state());
  TEST_ASSERT_EQUAL_UINT16(1, cycle.remainingMinutes(180 * kMinute - 1));
  cycle.update(180 * kMinute, inputs(true, false));
  TEST_ASSERT_EQUAL(kStateSettling, cycle.state());
  TEST_ASSERT_FALSE(cycle.outputs().electrolysis);

  cycle.update(185 * kMinute, inputs(false, false));
  TEST_ASSERT_EQUAL(kStateTransferring, cycle.state());
  TEST_ASSERT_TRUE(cycle.outputs().transferValve);

  cycle.update(195 * kMinute - 1, inputs(false, false));
  TEST_ASSERT_EQUAL(kStateTransferring, cycle.state());
  cycle.update(195 * kMinute, inputs(false, false));
  TEST_ASSERT_EQUAL(kStateStandby, cycle.state());
  TEST_ASSERT_FALSE(cycle.outputs().transferValve);
  TEST_ASSERT_EQUAL_UINT32(1, cycle.completedCycles());
}

void test_late_loop_does_not_stretch_the_cycle() {
  // The production end is detected 7 s late; settling still ends 185 min after start.
  ProductionCycle cycle;
  cycle.configure(defaultTimes());
  cycle.begin(0);
  cycle.update(0, inputs(true, false));
  cycle.update(180 * kMinute + 7000, inputs(true, false));
  TEST_ASSERT_EQUAL(kStateSettling, cycle.state());
  TEST_ASSERT_EQUAL_UINT32(5 * kMinute - 7000, cycle.remainingMs(180 * kMinute + 7000));
}

void test_waits_for_space_in_storage_tank() {
  ProductionCycle cycle;
  cycle.configure(defaultTimes());
  cycle.begin(0);
  cycle.forceState(kStateSettling, 0);
  cycle.update(5 * kMinute, inputs(false, true));
  TEST_ASSERT_EQUAL(kStateWaitingForSpace, cycle.state());
  TEST_ASSERT_FALSE(cycle.outputs().transferValve);
  cycle.update(90 * kMinute, inputs(false, true));
  TEST_ASSERT_EQUAL(kStateWaitingForSpace, cycle.state());
  cycle.update(91 * kMinute, inputs(false, false));
  TEST_ASSERT_EQUAL(kStateTransferring, cycle.state());
  TEST_ASSERT_EQUAL_UINT32(10 * kMinute, cycle.remainingMs(91 * kMinute));
}

void runOneBatch(ProductionCycle& cycle, uint32_t& now) {
  cycle.update(now, inputs(true, false));
  now += 180 * kMinute;
  cycle.update(now, inputs(true, false));
  now += 5 * kMinute;
  cycle.update(now, inputs(true, false));
  now += 10 * kMinute;
  cycle.update(now, inputs(false, false));
  TEST_ASSERT_EQUAL(kStateStandby, cycle.state());
}

void test_polarity_reverses_every_n_batches() {
  CycleTimes times = defaultTimes();
  times.polarityCycles = 2;
  ProductionCycle cycle;
  cycle.configure(times);
  cycle.begin(0);
  uint32_t now = 0;
  runOneBatch(cycle, now);
  TEST_ASSERT_FALSE(cycle.outputs().polarityReversed);
  runOneBatch(cycle, now);
  TEST_ASSERT_TRUE(cycle.outputs().polarityReversed);
  runOneBatch(cycle, now);
  TEST_ASSERT_TRUE(cycle.outputs().polarityReversed);
  runOneBatch(cycle, now);
  TEST_ASSERT_FALSE(cycle.outputs().polarityReversed);
}

void test_polarity_never_changes_while_electrolysis_is_on() {
  ProductionCycle cycle;
  cycle.configure(defaultTimes());
  cycle.begin(0);
  cycle.update(0, inputs(true, false));
  const bool before = cycle.outputs().polarityReversed;
  for (uint32_t t = 0; t < 180 * kMinute; t += kMinute) {
    cycle.update(t, inputs(true, false));
    TEST_ASSERT_EQUAL(before, cycle.outputs().polarityReversed);
  }
  cycle.update(180 * kMinute, inputs(true, false));
  TEST_ASSERT_FALSE(cycle.outputs().electrolysis);
  TEST_ASSERT_NOT_EQUAL(before, cycle.outputs().polarityReversed);
}

void test_progress_restore_resumes_the_batch() {
  ProductionCycle before;
  before.configure(defaultTimes());
  before.begin(0);
  before.update(0, inputs(true, false));
  const CycleProgress saved = before.progress(100 * kMinute + 30000);  // 100.5 min in
  TEST_ASSERT_EQUAL_UINT16(100, saved.elapsedMinutes);

  uint8_t bytes[kCycleProgressSize];
  encodeProgress(saved, bytes);

  ProductionCycle after;  // power cut, reboot at t = 5 s
  after.configure(defaultTimes());
  after.restore(decodeProgress(bytes), 5000);
  TEST_ASSERT_EQUAL(kStateProduction, after.state());
  TEST_ASSERT_TRUE(after.outputs().electrolysis);
  TEST_ASSERT_EQUAL_UINT32(80 * kMinute, after.remainingMs(5000));
}

void test_restore_clamps_elapsed_to_shortened_duration() {
  CycleProgress saved;
  saved.state = kStateProduction;
  saved.polarityReversed = 1;
  saved.elapsedMinutes = 170;
  saved.completedCycles = 4;
  CycleTimes times = defaultTimes();
  times.productionMs = 120 * kMinute;  // operator shortened the production time
  ProductionCycle cycle;
  cycle.configure(times);
  cycle.restore(saved, 0);
  TEST_ASSERT_EQUAL_UINT32(0, cycle.remainingMs(0));
  TEST_ASSERT_TRUE(cycle.outputs().polarityReversed);
  cycle.update(1, inputs(true, false));
  TEST_ASSERT_EQUAL(kStateSettling, cycle.state());
  TEST_ASSERT_EQUAL_UINT32(5, cycle.completedCycles());
}

void test_restore_of_garbage_state_goes_to_standby() {
  CycleProgress saved = {99, 0, 0, 0};
  ProductionCycle cycle;
  cycle.configure(defaultTimes());
  cycle.restore(saved, 0);
  TEST_ASSERT_EQUAL(kStateStandby, cycle.state());
}

void test_persist_requested_on_state_change_and_periodically() {
  ProductionCycle cycle;
  cycle.configure(defaultTimes());
  cycle.begin(0);
  TEST_ASSERT_FALSE(cycle.takePersistRequest(0));
  cycle.update(1000, inputs(true, false));
  TEST_ASSERT_TRUE(cycle.takePersistRequest(1000));
  TEST_ASSERT_FALSE(cycle.takePersistRequest(2000));
  TEST_ASSERT_FALSE(cycle.takePersistRequest(1000 + kProgressSavePeriodMs - 1));
  TEST_ASSERT_TRUE(cycle.takePersistRequest(1000 + kProgressSavePeriodMs));
}

// --- Ecophi -----------------------------------------------------------------------

void test_ecophi_frame_format_is_v22_compatible() {
  EcophiFrame f;
  f.flowLpm = 123.456f;
  f.voltage = 13.2f;
  f.level1 = true;
  f.level2 = false;
  f.level3 = true;
  f.naclOMlPerMin = 55.5f;
  f.targetFrcMgL = 1.5f;
  f.activeChlorine = 0.0f;
  f.ph = 7.25f;
  char text[kEcophiFrameCapacity];
  formatEcophiFrame(f, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING(";123.46,13.20,1,0,1,55.50,1.50,0.00,7.25:", text);
}

void test_ecophi_worst_case_frame_fits() {
  EcophiFrame f;
  // Largest magnitude that still formats as digits (larger values print "ovf").
  const float widest = -42949000.0f;
  f.flowLpm = f.voltage = widest;
  f.level1 = f.level2 = f.level3 = true;
  f.naclOMlPerMin = f.targetFrcMgL = f.activeChlorine = f.ph = widest;
  char text[kEcophiFrameCapacity];
  const uint8_t length = formatEcophiFrame(f, text, sizeof(text));
  TEST_ASSERT_EQUAL_CHAR(':', text[length - 1]);
  TEST_ASSERT_NULL(strstr(text, "ovf"));
}

void test_ecophi_averager() {
  EcophiAverager averager;
  TEST_ASSERT_EQUAL_FLOAT(0.0f, averager.averageFlowLpm());
  averager.addSample(10.0f, 0.2f, 7.0f);
  averager.addSample(20.0f, 0.4f, 8.0f);
  TEST_ASSERT_EQUAL_FLOAT(15.0f, averager.averageFlowLpm());
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.3f, averager.averageActiveChlorine());
  TEST_ASSERT_EQUAL_FLOAT(7.5f, averager.averagePh());
  averager.reset();
  TEST_ASSERT_EQUAL_UINT16(0, averager.sampleCount());
}

void test_dosed_rate_meter() {
  DosedRateMeter meter;
  meter.start(0);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, meter.takeRate(1000000, true, 60000));  // first total: reference only
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 30.0f, meter.takeRate(1030000, true, 120000));  // 30 mL in 1 min
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 5.0f, meter.takeRate(5000, true, 180000));     // dosing board restarted
  TEST_ASSERT_EQUAL_FLOAT(0.0f, meter.takeRate(9000, false, 240000));            // link down
}

// --- Display ----------------------------------------------------------------------

DisplayModel sampleModel() {
  DisplayModel m;
  memset(&m, 0, sizeof(m));
  m.state = kStateProduction;
  m.remainingMin = 123;
  m.linkOk = true;
  m.chemicalAvailable = true;
  m.flowLpm = 12.5f;
  m.lastDoseMl = 3.25f;
  m.totalWaterM3 = 1.234f;
  m.targetFrcMgL = 1.5f;
  m.voltage = 13.21f;
  m.level1 = true;
  m.level3 = true;
  return m;
}

void assertPage(const LcdLine lines[kLcdRows], const char* l0, const char* l1, const char* l2, const char* l3) {
  const char* expected[kLcdRows] = {l0, l1, l2, l3};
  for (uint8_t row = 0; row < kLcdRows; ++row) {
    std::string padded(expected[row]);
    padded.resize(kLcdColumns, ' ');
    TEST_ASSERT_EQUAL_STRING(padded.c_str(), lines[row]);
  }
}

void test_display_dosing_page() {
  LcdLine lines[kLcdRows];
  renderPage(sampleModel(), 0, lines);
  assertPage(lines, "Flow: 12.50 L/min", "NaClO.Inj: 3.25 mL", "Comm.Flow: 1.23 m3", "Target FRC: 1.50mg/L");
}

void test_display_process_page() {
  LcdLine lines[kLcdRows];
  renderPage(sampleModel(), 1, lines);
  assertPage(lines, "State: Producing", "Time Left: 123 min", "Voltage: 13.21 V", "L1: 1 L2: 0 L3: 1");
}

void test_display_alarms_and_splash() {
  DisplayModel m = sampleModel();
  LcdLine lines[kLcdRows];
  m.chemicalAvailable = false;
  renderPage(m, 0, lines);
  TEST_ASSERT_EQUAL_STRING("NaClO tank EMPTY    ", lines[1]);

  m.linkOk = false;
  renderPage(m, 0, lines);
  TEST_ASSERT_EQUAL_STRING("Dosing board: NO    ", lines[0]);

  m.splash = true;
  renderPage(m, 1, lines);
  assertPage(lines, "Clara Begins", "Add 210g of salt", "& Add 7L of Water", "Into The Prod Bottle");
}

void test_display_lines_never_exceed_20_columns() {
  DisplayModel m = sampleModel();
  m.flowLpm = 123456.0f;
  m.lastDoseMl = 99999.0f;
  m.totalWaterM3 = 4294967.0f;
  m.voltage = -1000.0f;
  m.remainingMin = 65535;
  LcdLine lines[kLcdRows];
  const ProcessState states[] = {kStateStandby, kStateProduction, kStateSettling, kStateTransferring,
                                 kStateWaitingForSpace};
  for (uint8_t s = 0; s < 5; ++s) {
    m.state = states[s];
    for (uint8_t page = 0; page < kPageCount; ++page) {
      renderPage(m, page, lines);
      for (uint8_t row = 0; row < kLcdRows; ++row) TEST_ASSERT_EQUAL_UINT(kLcdColumns, strlen(lines[row]));
    }
  }
}

// --- Voltage ----------------------------------------------------------------------

void test_voltage_conversion_matches_v22() {
  // v2.2: value * 4.85 / 1024 * 22.2
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 128.0f * 4.85f / 1024.0f * 22.2f, adcToVolts(128.0f, 4.85f, 22.2f));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_main_defaults_are_valid_and_match_v22);
  RUN_TEST(test_standby_until_production_bottle_is_full);
  RUN_TEST(test_full_cycle_timing_is_exact);
  RUN_TEST(test_late_loop_does_not_stretch_the_cycle);
  RUN_TEST(test_waits_for_space_in_storage_tank);
  RUN_TEST(test_polarity_reverses_every_n_batches);
  RUN_TEST(test_polarity_never_changes_while_electrolysis_is_on);
  RUN_TEST(test_progress_restore_resumes_the_batch);
  RUN_TEST(test_restore_clamps_elapsed_to_shortened_duration);
  RUN_TEST(test_restore_of_garbage_state_goes_to_standby);
  RUN_TEST(test_persist_requested_on_state_change_and_periodically);
  RUN_TEST(test_ecophi_frame_format_is_v22_compatible);
  RUN_TEST(test_ecophi_worst_case_frame_fits);
  RUN_TEST(test_ecophi_averager);
  RUN_TEST(test_dosed_rate_meter);
  RUN_TEST(test_display_dosing_page);
  RUN_TEST(test_display_process_page);
  RUN_TEST(test_display_alarms_and_splash);
  RUN_TEST(test_display_lines_never_exceed_20_columns);
  RUN_TEST(test_voltage_conversion_matches_v22);
  return UNITY_END();
}
