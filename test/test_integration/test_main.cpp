/**
 * Integration tests: the complete dosing and main-board applications run on a
 * simulated clock with fake hardware, individually and connected to each other
 * (I2C telemetry loopback, L3 signal wire). These tests exercise the same code
 * that runs on the boards; only the lowest driver layer is replaced.
 */
#include <math.h>
#include <string.h>
#include <unity.h>

#include "../support/fakes.h"
#include "clara/dosing/dosing_app.h"
#include "clara/dosing/dosing_console.h"
#include "clara/mainboard/mainboard_app.h"
#include "clara/mainboard/mainboard_console.h"

using namespace clara;

void setUp() {}
void tearDown() {}

namespace {

const uint32_t kSecond = 1000ul;
const uint32_t kMinute = 60000ul;

/** Expected corrected flow for a flowmeter frequency (default calibration). */
double expectedFlowLpm(double hz) {
  const double raw = hz / 0.5;
  return raw >= 5.0 ? 1.02 * raw + 8.61 : raw;
}

/** Expected NaClO for a given flow over a duration (default calibration). */
double expectedNaclOMl(double flowLpm, double minutes) {
  const double coefficient = flowLpm <= 60.0 ? 1.45 : 1.35;
  return flowLpm * minutes * (1.5 / 4.5) * coefficient;
}

/** Dosing board with fake hardware. */
struct DosingBench {
  fakes::FakeClock& clock;
  fakes::FakeFlowSensor flow;
  fakes::FakePump pump;
  fakes::FakeInput chemical;
  fakes::FakeEeprom& eeprom;
  fakes::FakeTelemetrySink sink;
  fakes::StringOutput console;
  dosing::DosingApp app;
  dosing::DosingConsole shell;

  DosingBench(fakes::FakeClock& c, fakes::FakeEeprom& e)
      : clock(c),
        flow(c),
        pump(c),
        chemical(true),
        eeprom(e),
        app(c, flow, pump, chemical, e, sink),
        shell(app, console) {}

  /** One loop() pass after the hardware caught up with simulated time. */
  void step() {
    flow.advance();
    pump.advance();
    app.update();
    shell.update();
  }

  void run(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i) {
      clock.advanceMs(1);
      step();
    }
  }

  void type(const char* line) {
    for (const char* p = line; *p; ++p) shell.onChar(*p);
    shell.onChar('\n');
  }

  double pumpedMl() const { return pump.executedSteps() * 1.2 / 6400.0; }
};

/** Main board with fake hardware. */
struct MainBench {
  fakes::FakeClock& clock;
  fakes::FakeMainboardIo io;
  fakes::FakeDisplay display;
  fakes::StringOutput serial;  // RS485 + console
  fakes::FakeEeprom& eeprom;
  mainboard::MainboardApp app;
  mainboard::MainboardConsole shell;

  MainBench(fakes::FakeClock& c, mainboard::DosingLinkPort& link, fakes::FakeEeprom& e)
      : clock(c), eeprom(e), app(c, io, link, display, serial, e), shell(app, serial) {}

  void type(const char* line) {
    for (const char* p = line; *p; ++p) shell.onChar(*p);
    shell.onChar('\n');
  }
};

/** A link port that never answers (dosing board absent). */
class DeadLink : public mainboard::DosingLinkPort {
 public:
  uint8_t requestFrame(uint8_t*, uint8_t) override { return 0; }
};

}  // namespace

// =============================================================================
// Dosing board
// =============================================================================

void test_dosing_constant_flow_for_one_hour_is_accurate() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();

  bench.flow.setFrequency(50.0);  // 100 L/min raw -> 110.61 L/min corrected
  bench.run(60 * kMinute);
  bench.flow.setFrequency(0.0);
  bench.run(45 * kSecond);  // let the last interval's steps drain

  const double flow = expectedFlowLpm(50.0);
  TEST_ASSERT_FLOAT_WITHIN(flow * 60.0 * 0.002, flow * 60.0, bench.app.status().totalWaterLiters);

  const double expected = expectedNaclOMl(flow, 60.0);
  // Delivered NaClO within 0.5 % of the theoretical dose.
  TEST_ASSERT_FLOAT_WITHIN(expected * 0.005, expected, bench.pumpedMl());
  TEST_ASSERT_FLOAT_WITHIN(expected * 0.005, expected, bench.app.status().totalNaclOMl);
  TEST_ASSERT_EQUAL_UINT32(0, bench.pump.pendingSteps());
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(10000, bench.pump.maxRateSeen());
  TEST_ASSERT_FALSE(bench.app.status().saturated);
}

void test_dosing_follows_changing_flow() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();

  bench.flow.setFrequency(20.0);  // low band (49.4 L/min, coefficient 1.45)
  bench.run(30 * kMinute);
  bench.flow.setFrequency(60.0);  // high band (131 L/min, coefficient 1.35)
  bench.run(30 * kMinute);
  bench.flow.setFrequency(0.0);
  bench.run(45 * kSecond);

  const double expected = expectedNaclOMl(expectedFlowLpm(20.0), 30.0) + expectedNaclOMl(expectedFlowLpm(60.0), 30.0);
  // One 20 s interval straddles the change: allow 1 %.
  TEST_ASSERT_FLOAT_WITHIN(expected * 0.01, expected, bench.pumpedMl());
}

void test_dosing_is_independent_of_loop_speed() {
  // Same hour, but loop() only runs every 25 ms (as if the console were busy).
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();
  bench.flow.setFrequency(50.0);
  for (uint32_t t = 0; t < 60 * kMinute; t += 25) {
    clock.advanceMs(25);
    bench.step();
  }
  bench.flow.setFrequency(0.0);
  bench.run(45 * kSecond);
  const double expected = expectedNaclOMl(expectedFlowLpm(50.0), 60.0);
  TEST_ASSERT_FLOAT_WITHIN(expected * 0.005, expected, bench.pumpedMl());
}

void test_dosing_stops_when_naclo_tank_is_empty() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();
  bench.flow.setFrequency(50.0);
  bench.run(2 * kMinute);
  TEST_ASSERT_GREATER_THAN_UINT32(0, bench.pump.pendingSteps());

  bench.chemical.set(false);
  bench.run(1100);  // within one flow sample the pump must stop
  TEST_ASSERT_EQUAL_UINT32(0, bench.pump.pendingSteps());
  const uint32_t stepsWhenEmpty = bench.pump.executedSteps();
  bench.run(5 * kMinute);
  TEST_ASSERT_EQUAL_UINT32(stepsWhenEmpty, bench.pump.executedSteps());

  link::DosingTelemetry t;
  TEST_ASSERT_EQUAL(link::kDecodeOk, link::decodeTelemetry(bench.sink.frame().data(), 28, t));
  TEST_ASSERT_EQUAL(0, t.flags & link::kFlagChemicalAvailable);

  bench.chemical.set(true);  // refilled: dosing resumes
  bench.run(1 * kMinute);
  TEST_ASSERT_GREATER_THAN_UINT32(stepsWhenEmpty, bench.pump.executedSteps());
}

void test_dosing_reports_saturation() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();
  bench.type("set max_step_rate 1000");
  bench.flow.setFrequency(50.0);
  bench.run(1 * kMinute);
  TEST_ASSERT_TRUE(bench.app.status().saturated);
  link::DosingTelemetry t;
  link::decodeTelemetry(bench.sink.frame().data(), 28, t);
  TEST_ASSERT_NOT_EQUAL(0, t.flags & link::kFlagPumpSaturated);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(1000, bench.pump.rate());
}

void test_pump_calibration_procedure() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  {
    DosingBench bench(clock, eeprom);
    bench.app.begin();
    bench.type("pumpcal 10");
    TEST_ASSERT_EQUAL(dosing::kModePumpCalibration, bench.app.status().mode);
    bench.run(30 * kSecond);  // 64 000 steps at 3200 steps/s = 20 s
    TEST_ASSERT_EQUAL_UINT32(64000, bench.pump.executedSteps());
    TEST_ASSERT_EQUAL(dosing::kModeAutomatic, bench.app.status().mode);
    TEST_ASSERT_TRUE(bench.console.contains("pumpcal done"));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, bench.app.status().totalNaclOMl);  // went into the cylinder

    bench.type("pumpcal done 13");  // operator measured 13 mL
    TEST_ASSERT_TRUE(bench.console.contains("ml_per_rev = 1.3000"));
  }
  // After a reset the calibration is loaded from EEPROM.
  DosingBench rebooted(clock, eeprom);
  rebooted.app.begin();
  TEST_ASSERT_EQUAL(ParamPersistence::kLoadOk, rebooted.app.status().calibrationLoad);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.3f, rebooted.app.params().get(dosing::kPumpMlPerRev));
}

void test_pumpcal_done_without_run_is_rejected() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();
  bench.type("pumpcal done 13");
  TEST_ASSERT_TRUE(bench.console.contains("error"));
  TEST_ASSERT_EQUAL_FLOAT(1.2f, bench.app.params().get(dosing::kPumpMlPerRev));
}

void test_flow_calibration_procedure() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();
  bench.flow.setFrequency(40.0);  // meter truly gives 40 Hz at 60 L/min -> 40 pulses per L
  bench.type("flowcal start");
  bench.run(150 * kSecond);       // 150 L collected in the "bucket" (6000 pulses)
  bench.type("flowcal done 150");
  TEST_ASSERT_TRUE(bench.console.contains("6000 pulses"));
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 40.0f / 60.0f, bench.app.params().get(dosing::kFlowKFactor));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, bench.app.params().get(dosing::kFlowCorrGain));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, bench.app.params().get(dosing::kFlowCorrOffset));
  bench.run(5 * kSecond);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 60.0f, bench.app.status().flowLpm);
}

void test_flow_meter_selection() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  {
    DosingBench bench(clock, eeprom);
    bench.app.begin();

    // 2 inch YF-DN50: f = 0.2 Q -> 20 Hz is 100 L/min (no correction curve).
    bench.type("meter 2");
    TEST_ASSERT_TRUE(bench.console.contains("2 inch YF-DN50"));
    TEST_ASSERT_TRUE(bench.console.contains("12.0 pulses/L"));
    bench.flow.setFrequency(20.0);
    bench.run(5 * kSecond);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 100.0f, bench.app.status().flowLpm);

    // 3 inch YF-DN80: f = 0.5 Q -> 50 Hz is 100 L/min.
    bench.type("set flow_meter 3");  // same as "meter 3"
    bench.flow.setFrequency(50.0);
    bench.run(5 * kSecond);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 100.0f, bench.app.status().flowLpm);

    // A 3 inch meter labelled f = 0.05 Q: 5 Hz is 100 L/min.
    bench.type("meter 3 0.05");
    TEST_ASSERT_TRUE(bench.console.contains("3.0 pulses/L"));
    bench.flow.setFrequency(5.0);
    bench.run(5 * kSecond);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 100.0f, bench.app.status().flowLpm);

    bench.console.clear();
    bench.type("meter 7");
    TEST_ASSERT_TRUE(bench.console.contains("usage"));
  }
  // The selection survives a reboot.
  DosingBench rebooted(clock, eeprom);
  rebooted.app.begin();
  TEST_ASSERT_EQUAL_FLOAT(3.0f, rebooted.app.params().get(dosing::kFlowMeterSize));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.05f, rebooted.app.params().get(dosing::kFlowKFactor));
  rebooted.type("status");
  TEST_ASSERT_TRUE(rebooted.console.contains("flow meter: 3 inch YF-DN80"));
}

void test_flow_k_can_be_adjusted_from_the_console() {
  // flow_k is the datasheet coefficient: f [Hz] = flow_k x Q [L/min].
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  {
    DosingBench bench(clock, eeprom);
    bench.app.begin();
    bench.type("meter 2");           // f = 0.2 Q, correction off
    bench.type("set flow_k 0.21");   // fine-tuned after a bucket test
    TEST_ASSERT_TRUE(bench.console.contains("flow_k = 0.2100 Hz/(L/min)"));
    bench.flow.setFrequency(21.0);   // 21 Hz / 0.21 = 100 L/min
    bench.run(5 * kSecond);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 100.0f, bench.app.status().flowLpm);

    bench.type("set flow_k 0.005");  // below the allowed 0.01: rejected
    TEST_ASSERT_TRUE(bench.console.contains("out of range"));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.21f, bench.app.params().get(dosing::kFlowKFactor));
  }
  DosingBench rebooted(clock, eeprom);  // the adjusted value is saved
  rebooted.app.begin();
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.21f, rebooted.app.params().get(dosing::kFlowKFactor));
  TEST_ASSERT_EQUAL_FLOAT(2.0f, rebooted.app.params().get(dosing::kFlowMeterSize));
}

void test_dosing_keeps_calibration_saved_by_previous_firmware() {
  // EEPROM written by the previous release (14 parameters, no flow_meter).
  fakes::FakeEeprom eeprom;
  {
    float values[14];
    ParamStore previous(dosing::parameterTable(), 14, values);
    previous.set(dosing::kPumpMlPerRev, 1.3f);
    ParamPersistence(eeprom, dosing::kEepromParamsAddress).save(previous);
  }
  fakes::FakeClock clock;
  DosingBench bench(clock, eeprom);
  bench.app.begin();
  TEST_ASSERT_EQUAL(ParamPersistence::kLoadOk, bench.app.status().calibrationLoad);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.3f, bench.app.params().get(dosing::kPumpMlPerRev));
  TEST_ASSERT_EQUAL_FLOAT(3.0f, bench.app.params().get(dosing::kFlowMeterSize));
}

void test_manual_dose_is_exact_and_not_counted_as_dosing() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();
  bench.type("dose 10");
  bench.run(30 * kSecond);
  TEST_ASSERT_EQUAL_UINT32(53333, bench.pump.executedSteps());  // 10 mL x 6400 / 1.2
  TEST_ASSERT_EQUAL_FLOAT(0.0f, bench.app.status().totalNaclOMl);
  TEST_ASSERT_TRUE(bench.console.contains("automatic dosing resumed"));
}

void test_stop_command_aborts_pump() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();
  bench.type("dose 100");
  bench.run(1 * kSecond);
  bench.type("stop");
  const uint32_t executed = bench.pump.executedSteps();
  bench.run(10 * kSecond);
  TEST_ASSERT_EQUAL_UINT32(executed, bench.pump.executedSteps());
  TEST_ASSERT_EQUAL(dosing::kModeAutomatic, bench.app.status().mode);
}

void test_flow_simulation_drives_dosing_without_water() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();
  bench.type("flowsim 100");
  bench.run(10 * kMinute);
  bench.type("flowsim off");
  bench.run(45 * kSecond);
  const double expected = expectedNaclOMl(100.0, 10.0);
  TEST_ASSERT_FLOAT_WITHIN(expected * 0.01, expected, bench.pumpedMl());
}

void test_changed_interval_takes_effect() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();
  bench.type("set dose_interval 10");
  bench.run(20 * kSecond + 5);
  // First decision still at the old 20 s cadence, the next ones every 10 s.
  const uint32_t decisions = bench.app.status().decisionCount;
  bench.run(30 * kSecond);
  TEST_ASSERT_EQUAL_UINT32(decisions + 3, bench.app.status().decisionCount);
}

void test_status_and_help_commands() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();
  bench.shell.printBanner();
  TEST_ASSERT_TRUE(bench.console.contains("factory defaults in use"));
  bench.type("help");
  TEST_ASSERT_TRUE(bench.console.contains("pumpcal <revs>"));
  bench.type("status");
  TEST_ASSERT_TRUE(bench.console.contains("mode: automatic"));
  bench.type("nonsense");
  TEST_ASSERT_TRUE(bench.console.contains("unknown command"));
}

// =============================================================================
// Main board
// =============================================================================

void test_mainboard_full_production_cycle() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DeadLink link;
  MainBench bench(clock, link, eeprom);
  bench.app.begin();

  bench.io.levels[0] = true;  // brine in the production bottle
  uint32_t electrolysisOnMs = 0, electrolysisOffMs = 0, valveOnMs = 0, valveOffMs = 0;
  bool wasOn = false, valveWasOn = false;
  for (uint32_t t = 0; t < 4 * 60 * kMinute; t += 10) {
    clock.advanceMs(10);
    bench.app.update();
    if (bench.io.outputs.electrolysis && !wasOn) electrolysisOnMs = clock.millis();
    if (!bench.io.outputs.electrolysis && wasOn) {
      electrolysisOffMs = clock.millis();
      bench.io.levels[0] = false;  // the batch will be transferred
    }
    if (bench.io.outputs.transferValve) bench.io.levels[2] = true;  // liquid reaches L3
    if (bench.io.outputs.transferValve && !valveWasOn) valveOnMs = clock.millis();
    if (!bench.io.outputs.transferValve && valveWasOn) valveOffMs = clock.millis();
    wasOn = bench.io.outputs.electrolysis;
    valveWasOn = bench.io.outputs.transferValve;
  }
  // Level debounce: production starts 10 s after brine is detected.
  TEST_ASSERT_UINT32_WITHIN(1000, 10 * kSecond, electrolysisOnMs);
  // Electrolysis lasts exactly 180 min (to the 10 ms simulation step).
  TEST_ASSERT_UINT32_WITHIN(10, 180 * kMinute, electrolysisOffMs - electrolysisOnMs);
  TEST_ASSERT_UINT32_WITHIN(10, 5 * kMinute, valveOnMs - electrolysisOffMs);
  TEST_ASSERT_UINT32_WITHIN(10, 10 * kMinute, valveOffMs - valveOnMs);
  TEST_ASSERT_EQUAL(mainboard::kStateStandby, bench.app.cycle().state());
  TEST_ASSERT_TRUE(bench.io.outputs.polarityReversed);  // reversed after the first batch
}

void test_mainboard_transfer_pump_error() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DeadLink link;
  MainBench bench(clock, link, eeprom);
  bench.app.begin();
  bench.type("set production_min 1");
  bench.type("set settling_min 1");
  bench.type("set transfer_min 1");
  bench.io.levels[0] = true;  // brine; L3 stays off: the transfer pump "fails"
  for (uint32_t t = 0; t < 5 * kMinute; t += 10) {
    clock.advanceMs(10);
    bench.app.update();
  }
  TEST_ASSERT_EQUAL(mainboard::kStateTransferFault, bench.app.cycle().state());
  TEST_ASSERT_EQUAL_UINT32(1, bench.app.cycle().completedCycles());
  TEST_ASSERT_FALSE(bench.io.outputs.electrolysis);  // L1 = 1, but no new batch
  TEST_ASSERT_TRUE(bench.display.shows("Transfer pump error"));

  bench.type("status");
  TEST_ASSERT_TRUE(bench.serial.contains("TRANSFER PUMP ERROR"));
  bench.type("clear");
  TEST_ASSERT_TRUE(bench.serial.contains("transfer pump error cleared"));
  for (uint32_t t = 0; t < 2 * kSecond; t += 10) {
    clock.advanceMs(10);
    bench.app.update();
  }
  TEST_ASSERT_EQUAL(mainboard::kStateProduction, bench.app.cycle().state());  // L1 still full
}

/** Total physical writes to the production-progress ring in @p eeprom. */
uint32_t ringWrites(const fakes::FakeEeprom& eeprom) {
  uint32_t writes = 0;
  const uint16_t end = mainboard::kEepromProgressAddress +
                       RingRecordStore::footprint(mainboard::kProgressSlots, mainboard::kCycleProgressSize);
  for (uint16_t a = mainboard::kEepromProgressAddress; a < end; ++a) writes += eeprom.writesAt(a);
  return writes;
}

/** Main board produces for @p minutes, power is cut, @return production time left after reboot. */
uint32_t remainingAfterRealPowerCut(uint32_t minutes, bool resume = true, uint32_t* ringWritesDuringProduction = 0) {
  fakes::FakeEeprom eeprom;
  DeadLink link;
  uint32_t productionStartMs = 0;
  {
    fakes::FakeClock clock;
    MainBench bench(clock, link, eeprom);
    bench.app.begin();
    if (!resume) bench.type("set resume_batch 0");
    bench.io.levels[0] = true;
    uint32_t writesAtStart = 0;
    while (true) {  // run until "minutes" after electrolysis switched on
      clock.advanceMs(100);
      bench.app.update();
      if (productionStartMs == 0 && bench.io.outputs.electrolysis) {
        productionStartMs = clock.millis();
        writesAtStart = ringWrites(eeprom);  // includes the save at production start
      }
      if (productionStartMs != 0 && clock.millis() - productionStartMs >= minutes * kMinute) break;
    }
    TEST_ASSERT_EQUAL(mainboard::kStateProduction, bench.app.cycle().state());
    if (ringWritesDuringProduction != 0) *ringWritesDuringProduction = ringWrites(eeprom) - writesAtStart;
  }
  fakes::FakeClock rebootClock;  // power cut: the board restarts with a fresh millis()
  MainBench rebooted(rebootClock, link, eeprom);
  rebooted.app.begin();
  if (!resume) {
    // Feature off: nothing is resumed, the board starts in standby with the outputs off.
    TEST_ASSERT_FALSE(rebooted.app.status().progressRestored);
    TEST_ASSERT_TRUE(rebooted.app.status().progressDiscarded);
    TEST_ASSERT_EQUAL(mainboard::kStateStandby, rebooted.app.cycle().state());
    TEST_ASSERT_FALSE(rebooted.io.outputs.electrolysis);
    return 0;
  }
  TEST_ASSERT_TRUE(rebooted.app.status().progressRestored);
  TEST_ASSERT_EQUAL(mainboard::kStateProduction, rebooted.app.cycle().state());
  TEST_ASSERT_TRUE(rebooted.io.outputs.electrolysis);
  return rebooted.app.cycle().remainingMs(rebootClock.millis());
}

void test_mainboard_resumes_batch_after_power_cut() {
  TEST_ASSERT_EQUAL_UINT32(150 * kMinute, remainingAfterRealPowerCut(45));
  TEST_ASSERT_EQUAL_UINT32(120 * kMinute, remainingAfterRealPowerCut(65));
  TEST_ASSERT_EQUAL_UINT32(90 * kMinute, remainingAfterRealPowerCut(95));
}

void test_mainboard_no_checkpoints_when_resume_disabled() {
  uint32_t writes = 0;
  // resume_batch = 1: checkpoints at 30 and 60 min are written during 65 min of production.
  TEST_ASSERT_EQUAL_UINT32(120 * kMinute, remainingAfterRealPowerCut(65, true, &writes));
  TEST_ASSERT_GREATER_THAN_UINT32(0, writes);
  // resume_batch = 0: no checkpoint is written and nothing is resumed.
  remainingAfterRealPowerCut(45, false, &writes);
  TEST_ASSERT_EQUAL_UINT32(0, writes);
  remainingAfterRealPowerCut(65, false, &writes);
  TEST_ASSERT_EQUAL_UINT32(0, writes);
}

void test_mainboard_eeprom_wear_per_batch() {
  // Ten complete batches: count the physical writes to the busiest EEPROM byte.
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DeadLink link;
  MainBench bench(clock, link, eeprom);
  bench.app.begin();
  for (int batch = 0; batch < 10; ++batch) {
    bench.io.levels[0] = true;
    bench.io.levels[2] = true;
    for (uint32_t t = 0; t < 200 * kMinute; t += 1000) {
      clock.advanceMs(1000);
      bench.app.update();
      if (bench.io.outputs.transferValve) bench.io.levels[0] = false;
    }
    TEST_ASSERT_EQUAL(mainboard::kStateStandby, bench.app.cycle().state());
  }
  TEST_ASSERT_EQUAL_UINT32(10, bench.app.cycle().completedCycles());
  // 9 saves per batch spread over 8 slots: about 12 writes per byte for 10
  // batches. 100 000 cycles therefore last > 80 000 batches (~30 years at 7/day).
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(13, eeprom.maxWrites());
}

/** Runs the main board until it is @p minutes into production, then "cuts the power". */
void produceFor(fakes::FakeEeprom& eeprom, uint32_t minutes, const char* command = 0) {
  fakes::FakeClock clock;
  DeadLink link;
  MainBench bench(clock, link, eeprom);
  bench.app.begin();
  bench.io.levels[0] = true;
  for (uint32_t t = 0; t < minutes * kMinute; t += 100) {
    clock.advanceMs(100);
    bench.app.update();
  }
  TEST_ASSERT_EQUAL(mainboard::kStateProduction, bench.app.cycle().state());
  if (command != 0) bench.type(command);
}

void test_mainboard_cancel_command_prevents_resume() {
  fakes::FakeEeprom eeprom;
  produceFor(eeprom, 30, "cancel");
  fakes::FakeClock clock;
  DeadLink link;
  MainBench rebooted(clock, link, eeprom);
  rebooted.app.begin();
  TEST_ASSERT_FALSE(rebooted.app.status().progressRestored);
  TEST_ASSERT_EQUAL(mainboard::kStateStandby, rebooted.app.cycle().state());
  TEST_ASSERT_FALSE(rebooted.io.outputs.electrolysis);
}

void test_mainboard_resume_can_be_disabled() {
  fakes::FakeEeprom eeprom;
  produceFor(eeprom, 30, "set resume_batch 0");
  fakes::FakeClock clock;
  DeadLink link;
  MainBench rebooted(clock, link, eeprom);
  rebooted.app.begin();
  rebooted.shell.printBanner();
  TEST_ASSERT_TRUE(rebooted.app.status().progressDiscarded);
  TEST_ASSERT_EQUAL(mainboard::kStateStandby, rebooted.app.cycle().state());
  TEST_ASSERT_FALSE(rebooted.io.outputs.electrolysis);
  TEST_ASSERT_TRUE(rebooted.serial.contains("not resumed"));
}

void test_mainboard_keeps_settings_saved_by_previous_firmware() {
  // EEPROM written by the first v3 release, which had 8 parameters (no resume_batch).
  fakes::FakeEeprom eeprom;
  {
    float values[8];
    ParamStore previous(mainboard::parameterTable(), 8, values);
    previous.set(mainboard::kProductionMin, 150.0f);
    ParamPersistence(eeprom, mainboard::kEepromParamsAddress).save(previous);
  }
  fakes::FakeClock clock;
  DeadLink link;
  MainBench bench(clock, link, eeprom);
  bench.app.begin();
  TEST_ASSERT_EQUAL(ParamPersistence::kLoadOk, bench.app.status().calibrationLoad);
  TEST_ASSERT_EQUAL_FLOAT(150.0f, bench.app.params().get(mainboard::kProductionMin));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, bench.app.params().get(mainboard::kResumeBatch));
  TEST_ASSERT_EQUAL_UINT32(115200, bench.app.serialBaudRate());
}

void test_mainboard_report_baud_rate() {
  fakes::FakeEeprom eeprom;
  {
    fakes::FakeClock clock;
    DeadLink link;
    MainBench bench(clock, link, eeprom);
    bench.app.begin();
    TEST_ASSERT_EQUAL_UINT32(115200, bench.app.serialBaudRate());  // default
    bench.shell.printBanner();
    TEST_ASSERT_TRUE(bench.serial.contains("115200 baud"));

    bench.type("set report_baud 12345");  // not a standard rate
    TEST_ASSERT_TRUE(bench.serial.contains("error: use 9600, 19200, 38400, 57600 or 115200"));
    TEST_ASSERT_EQUAL_UINT32(115200, bench.app.serialBaudRate());

    bench.type("set report_baud 9600");
    TEST_ASSERT_TRUE(bench.serial.contains("switching to 9600 baud"));
    TEST_ASSERT_EQUAL_UINT32(9600, bench.app.serialBaudRate());
  }
  fakes::FakeClock clock;  // saved across a reboot
  DeadLink link;
  MainBench rebooted(clock, link, eeprom);
  rebooted.app.begin();
  TEST_ASSERT_EQUAL_UINT32(9600, rebooted.app.serialBaudRate());
  rebooted.type("defaults");
  TEST_ASSERT_EQUAL_UINT32(115200, rebooted.app.serialBaudRate());
}

void runMain(MainBench& bench, fakes::FakeClock& clock, uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 100) {
    clock.advanceMs(100);
    bench.app.update();
  }
}

void test_mainboard_rs485_receiver_after_usb_window() {
  fakes::FakeEeprom eeprom;
  fakes::FakeClock clock;
  DeadLink link;
  MainBench bench(clock, link, eeprom);
  bench.app.begin();
  bench.shell.printBanner();
  TEST_ASSERT_TRUE(bench.serial.contains("RS485 remote commands start in 2 min unless you type here"));
  TEST_ASSERT_FALSE(bench.io.remoteListening);  // USB console first
  runMain(bench, clock, 2 * kMinute);
  TEST_ASSERT_TRUE(bench.io.remoteListening);   // nobody typed: listen on RS485

  // Remote commands work; unknown lines and noise get no answer.
  bench.serial.clear();
  bench.type("get water_ph");
  TEST_ASSERT_TRUE(bench.serial.contains("water_ph = 7.10"));
  bench.serial.clear();
  bench.type("hello");
  bench.type("\x01\xff garbage");
  TEST_ASSERT_EQUAL_STRING("", bench.serial.text().c_str());
  bench.type("status");
  TEST_ASSERT_TRUE(bench.serial.contains("command input RS485 remote"));
}

void test_mainboard_usb_console_keeps_rs485_off_while_used() {
  fakes::FakeEeprom eeprom;
  fakes::FakeClock clock;
  DeadLink link;
  MainBench bench(clock, link, eeprom);
  bench.app.begin();
  runMain(bench, clock, 1 * kMinute);
  bench.type("status");  // technician on USB
  TEST_ASSERT_TRUE(bench.serial.contains("command input USB, RS485 in 600 s"));
  runMain(bench, clock, 9 * kMinute);
  TEST_ASSERT_FALSE(bench.io.remoteListening);
  bench.type("get usb_idle_min");
  runMain(bench, clock, 9 * kMinute);
  TEST_ASSERT_FALSE(bench.io.remoteListening);
  runMain(bench, clock, 1 * kMinute + kSecond);  // 10 min without USB input
  TEST_ASSERT_TRUE(bench.io.remoteListening);
}

void test_mainboard_remote_command_and_remote_cal_off() {
  fakes::FakeEeprom eeprom;
  {
    fakes::FakeClock clock;
    DeadLink link;
    MainBench bench(clock, link, eeprom);
    bench.app.begin();
    bench.type("remote");
    TEST_ASSERT_TRUE(bench.serial.contains("ok - commands now come from RS485"));
    TEST_ASSERT_TRUE(bench.io.remoteListening);
    bench.type("set remote_cal 0");  // e.g. sent remotely: receiver off at once
    TEST_ASSERT_FALSE(bench.io.remoteListening);
  }
  fakes::FakeClock clock;
  DeadLink link;
  MainBench bench(clock, link, eeprom);
  bench.app.begin();
  runMain(bench, clock, 60 * kMinute);
  TEST_ASSERT_FALSE(bench.io.remoteListening);  // never listens, as v3.0
  bench.type("remote");
  TEST_ASSERT_TRUE(bench.serial.contains("error - remote_cal is 0"));
  bench.type("hello");
  TEST_ASSERT_TRUE(bench.serial.contains("unknown command"));  // USB still answers
}

/** Production started, then "restart" typed after @p minutes; returns the EEPROM. */
void restartDuringProduction(fakes::FakeEeprom& eeprom, uint32_t minutes, bool resume, bool remote) {
  fakes::FakeClock clock;
  DeadLink link;
  MainBench bench(clock, link, eeprom);
  bench.app.begin();
  if (!resume) bench.type("set resume_batch 0");
  if (remote) {
    bench.type("remote");
  } else {
    bench.type("set usb_idle_min 240");  // technician stays on USB for the whole test
  }
  bench.io.levels[0] = true;
  runMain(bench, clock, 11 * kSecond);  // level debounce: production starts
  TEST_ASSERT_EQUAL(mainboard::kStateProduction, bench.app.cycle().state());
  runMain(bench, clock, minutes * kMinute);
  bench.serial.clear();
  bench.type("restart");
  TEST_ASSERT_TRUE(bench.serial.contains("ok - restarting the main board"));
  TEST_ASSERT_TRUE(bench.app.restartRequested());
}

void test_mainboard_restart_command_keeps_batch_position() {
  fakes::FakeEeprom eeprom;
  restartDuringProduction(eeprom, 47, true, false);
  fakes::FakeClock clock;
  DeadLink link;
  MainBench bench(clock, link, eeprom);
  bench.app.begin();
  bench.shell.printBanner();
  TEST_ASSERT_TRUE(bench.serial.contains("restarted by the restart command"));
  TEST_ASSERT_EQUAL(mainboard::kStateProduction, bench.app.cycle().state());
  // Exactly where it was (47 min done), not the 30-min checkpoint.
  TEST_ASSERT_EQUAL_UINT16(180 - 47, bench.app.cycle().remainingMinutes(clock.millis()));
  TEST_ASSERT_FALSE(bench.app.restartRequested());
  TEST_ASSERT_FALSE(bench.io.remoteListening);  // restarted from USB: USB console first

  // The flag is used once: a later power cut is a normal power cut again.
  fakes::FakeClock clock2;
  MainBench again(clock2, link, eeprom);
  again.app.begin();
  TEST_ASSERT_FALSE(again.app.status().restartedByCommand);
}

void test_mainboard_remote_restart_listens_at_once_and_resumes_even_without_resume_batch() {
  fakes::FakeEeprom eeprom;
  restartDuringProduction(eeprom, 20, false, true);
  fakes::FakeClock clock;
  DeadLink link;
  MainBench bench(clock, link, eeprom);
  bench.app.begin();
  TEST_ASSERT_TRUE(bench.io.remoteListening);  // no 2-min USB window after a remote restart
  TEST_ASSERT_EQUAL(mainboard::kStateProduction, bench.app.cycle().state());  // resume_batch 0, still resumed
  TEST_ASSERT_EQUAL_UINT16(180 - 20, bench.app.cycle().remainingMinutes(clock.millis()));
}

void test_mainboard_imports_v22_settings() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  eeprom.pokeFloat(0, 42.0f);  // v2.2 "remaining production time": must be ignored
  eeprom.pokeFloat(4, 7.0f);   // v2.2 settling time
  eeprom.pokeFloat(8, 12.0f);  // v2.2 transfer time
  DeadLink link;
  MainBench bench(clock, link, eeprom);
  bench.app.begin();
  TEST_ASSERT_TRUE(bench.app.status().legacyImported);
  TEST_ASSERT_EQUAL_FLOAT(180.0f, bench.app.params().get(mainboard::kProductionMin));
  TEST_ASSERT_EQUAL_FLOAT(7.0f, bench.app.params().get(mainboard::kSettlingMin));
  TEST_ASSERT_EQUAL_FLOAT(12.0f, bench.app.params().get(mainboard::kTransferMin));
}

void test_mainboard_console_changes_production_time() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DeadLink link;
  MainBench bench(clock, link, eeprom);
  bench.app.begin();
  bench.type("set production_min 2");
  TEST_ASSERT_TRUE(bench.serial.contains("ok: "));
  bench.type("force production");
  TEST_ASSERT_TRUE(bench.io.outputs.electrolysis);
  for (uint32_t t = 0; t < 2 * kMinute; t += 100) {
    clock.advanceMs(100);
    bench.app.update();
  }
  TEST_ASSERT_EQUAL(mainboard::kStateSettling, bench.app.cycle().state());
  bench.type("status");
  TEST_ASSERT_TRUE(bench.serial.contains("state: Settling"));
  bench.type("set production_min 5000");
  TEST_ASSERT_TRUE(bench.serial.contains("out of range"));
}

void test_mainboard_shows_link_loss() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DeadLink link;
  MainBench bench(clock, link, eeprom);
  bench.app.begin();
  for (uint32_t t = 0; t < 10 * kSecond; t += 10) {
    clock.advanceMs(10);
    bench.app.update();
  }
  TEST_ASSERT_FALSE(bench.app.status().linkOk);
  TEST_ASSERT_GREATER_THAN_UINT32(5, bench.app.status().linkErrors);
  // Find the dosing page among the alternating pages.
  bool shown = false;
  for (uint32_t t = 0; t < 5 * kSecond && !shown; t += 10) {
    clock.advanceMs(10);
    bench.app.update();
    shown = bench.display.shows("Dosing board: NO");
  }
  TEST_ASSERT_TRUE(shown);
}

// =============================================================================
// Complete system: both boards connected
// =============================================================================

struct SystemBench {
  fakes::FakeClock clock;
  fakes::FakeEeprom dosingEeprom;
  fakes::FakeEeprom mainEeprom;
  DosingBench dosing;
  fakes::LoopbackLink link;
  MainBench main;

  SystemBench() : dosing(clock, dosingEeprom), link(dosing.sink), main(clock, link, mainEeprom) {}

  void begin() {
    dosing.app.begin();
    main.app.begin();
  }

  void run(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i) {
      clock.advanceMs(1);
      dosing.chemical.set(main.io.chemicalSignal);  // main pin 9 -> dosing A3
      dosing.step();
      main.app.update();
      main.shell.update();
    }
  }

  /** Types a main-board command and runs until every dosing command was answered. */
  void command(const char* line) {
    main.serial.clear();
    main.type(line);
    for (int i = 0; i < 5000 && main.shell.dosingBusy(); ++i) run(1);
    TEST_ASSERT_FALSE(main.shell.dosingBusy());
  }
};

void test_system_telemetry_reaches_display_and_ecophi() {
  SystemBench system;
  system.main.io.levels[2] = true;  // NaClO tank has liquid
  system.main.io.adc = 44;          // ~9.2 V
  system.begin();
  system.dosing.flow.setFrequency(50.0);
  system.run(3 * kMinute);

  TEST_ASSERT_TRUE(system.main.app.status().linkOk);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, expectedFlowLpm(50.0), system.main.app.status().dosing.flowLpm);

  bool flowShown = false;
  for (int i = 0; i < 4000 && !flowShown; ++i) {
    system.run(1);
    flowShown = system.main.display.shows("Flow: 110.61 L/min");
  }
  TEST_ASSERT_TRUE(flowShown);

  // Ecophi frames every 60 s; the third one covers a full minute of dosing.
  const std::string& out = system.main.serial.text();
  size_t start = 0;
  for (int frame = 0; frame < 3; ++frame) start = out.find(';', start) + 1;
  const std::string third = out.substr(start - 1, out.find(':', start) - start + 2);
  float flow, voltage, naclo, frc, activeCl, ph;
  int l1, l2, l3;
  TEST_ASSERT_EQUAL_INT(9, sscanf(third.c_str(), ";%f,%f,%d,%d,%d,%f,%f,%f,%f:", &flow, &voltage, &l1, &l2, &l3,
                                  &naclo, &frc, &activeCl, &ph));
  TEST_ASSERT_FLOAT_WITHIN(0.1f, expectedFlowLpm(50.0), flow);
  TEST_ASSERT_EQUAL_INT(1, l3);
  TEST_ASSERT_EQUAL_FLOAT(1.5f, frc);
  const float expectedRate = static_cast<float>(expectedNaclOMl(expectedFlowLpm(50.0), 1.0));
  TEST_ASSERT_FLOAT_WITHIN(expectedRate * 0.05f, expectedRate, naclo);  // pumped mL/min
  TEST_ASSERT_EQUAL_FLOAT(0.5f, activeCl);  // defaults until a site visit
  TEST_ASSERT_EQUAL_FLOAT(7.1f, ph);
}

void test_mainboard_reports_site_visit_ph_and_chlorine() {
  fakes::FakeEeprom eeprom;
  {
    fakes::FakeClock clock;
    DeadLink link;
    MainBench bench(clock, link, eeprom);
    bench.app.begin();
    bench.type("report");
    TEST_ASSERT_TRUE(bench.serial.contains(",0.50,7.10:"));
    bench.type("set water_ph 6.85");
    bench.type("set active_cl 0.42");
    bench.type("set water_ph 15");  // outside 0..14: rejected
    TEST_ASSERT_EQUAL_FLOAT(6.85f, bench.app.params().get(mainboard::kWaterPh));
  }
  fakes::FakeClock clock;  // saved across a reboot
  DeadLink link;
  MainBench rebooted(clock, link, eeprom);
  rebooted.app.begin();
  rebooted.type("report");
  TEST_ASSERT_TRUE(rebooted.serial.contains(",0.42,6.85:"));
}

void test_system_level3_low_stops_dosing_and_shows_alarm() {
  SystemBench system;
  system.main.io.levels[2] = true;
  system.begin();
  system.dosing.flow.setFrequency(50.0);
  system.run(1 * kMinute);
  TEST_ASSERT_GREATER_THAN_UINT32(0, system.dosing.pump.executedSteps());

  system.main.io.levels[2] = false;  // NaClO tank empty
  system.run(15 * kSecond);          // 10 s debounce on the main board + 1 s on the dosing board
  const uint32_t steps = system.dosing.pump.executedSteps();
  system.run(1 * kMinute);
  TEST_ASSERT_EQUAL_UINT32(steps, system.dosing.pump.executedSteps());

  bool alarm = false;
  for (int i = 0; i < 4000 && !alarm; ++i) {
    system.run(1);
    alarm = system.main.display.shows("NaClO tank EMPTY");
  }
  TEST_ASSERT_TRUE(alarm);
}

void test_system_rejects_corrupted_i2c_frames() {
  SystemBench system;
  system.begin();
  system.run(3 * kSecond);
  TEST_ASSERT_TRUE(system.main.app.status().linkOk);
  system.link.corrupt = true;
  system.run(6 * kSecond);
  TEST_ASSERT_FALSE(system.main.app.status().linkOk);
  system.link.corrupt = false;
  system.run(2 * kSecond);
  TEST_ASSERT_TRUE(system.main.app.status().linkOk);
}

void test_system_link_diagnostics() {
  SystemBench system;
  system.begin();
  system.run(3 * kSecond);
  system.main.type("i2c");
  TEST_ASSERT_TRUE(system.main.serial.contains("0x21  dosing board"));
  TEST_ASSERT_TRUE(system.main.serial.contains("0x27  LCD backpack"));

  // Dosing processor not answering (held in reset, jumpers open ...).
  system.link.connected = false;
  system.run(6 * kSecond);
  system.main.serial.clear();
  system.main.type("status");
  TEST_ASSERT_TRUE(system.main.serial.contains("NO ANSWER from address 0x21"));
  system.main.type("i2c");
  TEST_ASSERT_TRUE(system.main.serial.contains("dosing board (0x21) NOT found"));

  // Dosing board answering with foreign data (e.g. old firmware).
  system.link.connected = true;
  system.link.corrupt = true;
  system.run(6 * kSecond);
  system.main.serial.clear();
  system.main.type("status");
  TEST_ASSERT_TRUE(system.main.serial.contains("28 bytes, checksum wrong"));
  TEST_ASSERT_TRUE(system.main.serial.contains("upload the v3"));

  // Dosing console reports that nobody has polled it (fake sink counts nothing).
  system.dosing.type("status");
  TEST_ASSERT_TRUE(system.dosing.console.contains("I2C requests from main board: 0"));
}

// --- Calibrating the dosing board from the main board (I2C commands) ---------

void test_dosing_executes_link_commands() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();
  link::CommandFrame c;
  c.sequence = 1;
  c.command = link::kCmdSetParam;
  c.index = dosing::kTargetFrc;
  c.value = 1.8f;
  link::CommandReply r = bench.app.execute(c);
  TEST_ASSERT_EQUAL_UINT8(link::kResultOk, r.result);
  TEST_ASSERT_EQUAL_FLOAT(1.8f, bench.app.params().get(dosing::kTargetFrc));

  c.value = 50.0f;  // out of range: unchanged
  r = bench.app.execute(c);
  TEST_ASSERT_EQUAL_UINT8(link::kResultOutOfRange, r.result);
  TEST_ASSERT_EQUAL_FLOAT(1.8f, r.value);

  c.index = 200;
  TEST_ASSERT_EQUAL_UINT8(link::kResultBadIndex, bench.app.execute(c).result);

  c.command = link::kCmdGetParam;
  c.index = dosing::kCoefLowFlow;
  r = bench.app.execute(c);
  TEST_ASSERT_EQUAL_FLOAT(1.45f, r.value);
  TEST_ASSERT_EQUAL_UINT32(dosing::kParamCount, r.extra);

  // "set flow_meter 2" loads the 2-inch datasheet calibration, like "meter 2".
  c.command = link::kCmdSetParam;
  c.index = dosing::kFlowMeterSize;
  c.value = 2.0f;
  TEST_ASSERT_EQUAL_UINT8(link::kResultOk, bench.app.execute(c).result);
  TEST_ASSERT_EQUAL_FLOAT(0.2f, bench.app.params().get(dosing::kFlowKFactor));

  c.command = link::kCmdPumpCalDone;  // no pump run yet
  TEST_ASSERT_EQUAL_UINT8(link::kResultNotReady, bench.app.execute(c).result);
  c.command = 99;
  TEST_ASSERT_EQUAL_UINT8(link::kResultUnknownCommand, bench.app.execute(c).result);

  // Saved: a reboot keeps the values.
  fakes::FakeClock clock2;
  DosingBench rebooted(clock2, eeprom);
  rebooted.app.begin();
  TEST_ASSERT_EQUAL_FLOAT(1.8f, rebooted.app.params().get(dosing::kTargetFrc));
}

void test_system_set_dosing_parameter_from_main_board() {
  SystemBench system;
  system.begin();
  system.run(3 * kSecond);

  system.command("dosing set target_frc 1.8");
  TEST_ASSERT_TRUE(system.main.serial.contains("ok - dosing board saved:  0  target_frc = 1.80 mg/L"));
  TEST_ASSERT_EQUAL_FLOAT(1.8f, system.dosing.app.params().get(dosing::kTargetFrc));
  system.run(2 * kSecond);
  TEST_ASSERT_EQUAL_FLOAT(1.8f, system.main.app.status().dosing.targetFrcMgL);  // telemetry follows

  system.command("dosing set coef_low 9");
  TEST_ASSERT_TRUE(system.main.serial.contains("error - out of range, allowed 0.000 .. 5.000"));
  TEST_ASSERT_EQUAL_FLOAT(1.45f, system.dosing.app.params().get(dosing::kCoefLowFlow));

  system.command("dosing set nonsense 1");
  TEST_ASSERT_TRUE(system.main.serial.contains("unknown dosing parameter"));

  system.command("dosing get");
  TEST_ASSERT_TRUE(system.main.serial.contains(" 0  target_frc = 1.80 mg/L"));
  TEST_ASSERT_TRUE(system.main.serial.contains("14  flow_meter = 3 inch"));
  TEST_ASSERT_FALSE(system.main.serial.contains(";"));  // nothing the Ecophi receiver could misread

  system.command("dosing meter 2");
  TEST_ASSERT_TRUE(system.main.serial.contains("flow_meter = 2 inch"));
  TEST_ASSERT_TRUE(system.main.serial.contains("flow_k = 0.2000"));

  // Saved in the dosing board's EEPROM.
  fakes::FakeClock clock;
  DosingBench rebooted(clock, system.dosingEeprom);
  rebooted.app.begin();
  TEST_ASSERT_EQUAL_FLOAT(1.8f, rebooted.app.params().get(dosing::kTargetFrc));
  TEST_ASSERT_EQUAL_FLOAT(2.0f, rebooted.app.params().get(dosing::kFlowMeterSize));
}

void test_system_pump_calibration_from_main_board() {
  SystemBench system;
  system.begin();
  system.run(2 * kSecond);
  system.command("dosing pumpcal done 10");  // no run yet
  TEST_ASSERT_TRUE(system.main.serial.contains("error - run dosing pumpcal <revs> first"));

  const uint32_t steps = system.dosing.pump.executedSteps();  // no flow: the pump is idle
  system.command("dosing pumpcal 2");
  TEST_ASSERT_TRUE(system.main.serial.contains("dosing pump running"));
  system.run(10 * kSecond);  // 2 revs x 6400 steps at 3200 steps/s = 4 s
  TEST_ASSERT_EQUAL_UINT32(2 * 6400, system.dosing.pump.executedSteps() - steps);
  TEST_ASSERT_TRUE(system.main.serial.contains("dosing pump run finished - measure the volume"));

  system.command("dosing pumpcal done 7.3");
  TEST_ASSERT_TRUE(system.main.serial.contains("ok - ml_per_rev = 3.6500 (saved on the dosing board)"));
  TEST_ASSERT_EQUAL_FLOAT(3.65f, system.dosing.app.params().get(dosing::kPumpMlPerRev));
}

void test_system_flowmeter_bucket_test_from_main_board() {
  SystemBench system;
  system.begin();
  system.run(2 * kSecond);
  system.command("dosing flowcal start");
  TEST_ASSERT_TRUE(system.main.serial.contains("counting pulses"));
  system.dosing.flow.setFrequency(50.0);  // 100 L/min with k = 0.5
  system.run(2 * kMinute);                // 200 L, 6000 pulses
  system.dosing.flow.setFrequency(0.0);
  system.run(2 * kSecond);
  system.command("dosing flowcal done 200");
  TEST_ASSERT_TRUE(system.main.serial.contains("ok - 6000 pulses, flow_k = 0.5000"));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, system.dosing.app.params().get(dosing::kFlowCorrGain));
}

void test_system_lost_reply_is_recovered_without_executing_twice() {
  SystemBench system;
  system.begin();
  system.run(2 * kSecond);
  system.link.dropReplies = 1;
  system.command("dosing dose 5");
  TEST_ASSERT_TRUE(system.main.serial.contains("dosing... (dosing stop aborts)"));
  TEST_ASSERT_EQUAL_UINT32(1, system.dosing.app.commandCount());  // re-sent, executed once
  TEST_ASSERT_GREATER_THAN_INT(1, system.link.commandsSent);
  system.run(10 * kSecond);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 5.0, system.dosing.pumpedMl());
  TEST_ASSERT_TRUE(system.main.serial.contains("dosing pump run finished, automatic dosing resumed"));
}

void test_dosing_runs_same_command_again_after_main_board_reboot() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();
  link::CommandFrame c;
  c.sequence = 1;  // a rebooted main board starts again at 1
  c.command = link::kCmdDose;
  c.index = 0;
  c.value = 1.0f;
  uint8_t frame[link::kCommandFrameSize];
  link::encodeCommand(c, frame);
  bench.sink.receive(frame, sizeof(frame));
  bench.run(100);
  bench.sink.receive(frame, sizeof(frame));  // re-send: not executed again
  bench.run(100);
  TEST_ASSERT_EQUAL_UINT32(1, bench.app.commandCount());
  bench.run(5 * kSecond);
  bench.sink.receive(frame, sizeof(frame));  // same bytes, but later: a new command
  bench.run(100);
  TEST_ASSERT_EQUAL_UINT32(2, bench.app.commandCount());
}

void test_system_restart_dosing_board_from_main_board() {
  SystemBench system;
  system.begin();
  system.run(2 * kSecond);
  system.command("dosing restart");
  TEST_ASSERT_TRUE(system.main.serial.contains("ok - dosing board restarting"));
  TEST_ASSERT_TRUE(system.dosing.app.restartRequested());

  // Dosing console too.
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DosingBench bench(clock, eeprom);
  bench.app.begin();
  bench.type("restart");
  TEST_ASSERT_TRUE(bench.app.restartRequested());
}

void test_system_dosing_command_without_dosing_board() {
  SystemBench system;
  system.begin();
  system.link.connected = false;
  system.command("dosing set target_frc 1.2");
  TEST_ASSERT_TRUE(system.main.serial.contains("error - the dosing board did not answer"));
  // The main board kept running and the next command works once the link is back.
  system.link.connected = true;
  system.run(2 * kSecond);
  system.command("dosing get target_frc");
  TEST_ASSERT_TRUE(system.main.serial.contains("target_frc = 1.50 mg/L"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_dosing_constant_flow_for_one_hour_is_accurate);
  RUN_TEST(test_dosing_follows_changing_flow);
  RUN_TEST(test_dosing_is_independent_of_loop_speed);
  RUN_TEST(test_dosing_stops_when_naclo_tank_is_empty);
  RUN_TEST(test_dosing_reports_saturation);
  RUN_TEST(test_pump_calibration_procedure);
  RUN_TEST(test_pumpcal_done_without_run_is_rejected);
  RUN_TEST(test_flow_calibration_procedure);
  RUN_TEST(test_flow_meter_selection);
  RUN_TEST(test_flow_k_can_be_adjusted_from_the_console);
  RUN_TEST(test_dosing_keeps_calibration_saved_by_previous_firmware);
  RUN_TEST(test_manual_dose_is_exact_and_not_counted_as_dosing);
  RUN_TEST(test_stop_command_aborts_pump);
  RUN_TEST(test_flow_simulation_drives_dosing_without_water);
  RUN_TEST(test_changed_interval_takes_effect);
  RUN_TEST(test_status_and_help_commands);
  RUN_TEST(test_mainboard_full_production_cycle);
  RUN_TEST(test_mainboard_transfer_pump_error);
  RUN_TEST(test_mainboard_resumes_batch_after_power_cut);
  RUN_TEST(test_mainboard_no_checkpoints_when_resume_disabled);
  RUN_TEST(test_mainboard_eeprom_wear_per_batch);
  RUN_TEST(test_mainboard_cancel_command_prevents_resume);
  RUN_TEST(test_mainboard_resume_can_be_disabled);
  RUN_TEST(test_mainboard_keeps_settings_saved_by_previous_firmware);
  RUN_TEST(test_mainboard_report_baud_rate);
  RUN_TEST(test_mainboard_reports_site_visit_ph_and_chlorine);
  RUN_TEST(test_mainboard_rs485_receiver_after_usb_window);
  RUN_TEST(test_mainboard_usb_console_keeps_rs485_off_while_used);
  RUN_TEST(test_mainboard_remote_command_and_remote_cal_off);
  RUN_TEST(test_mainboard_restart_command_keeps_batch_position);
  RUN_TEST(test_mainboard_remote_restart_listens_at_once_and_resumes_even_without_resume_batch);
  RUN_TEST(test_system_restart_dosing_board_from_main_board);
  RUN_TEST(test_mainboard_imports_v22_settings);
  RUN_TEST(test_mainboard_console_changes_production_time);
  RUN_TEST(test_mainboard_shows_link_loss);
  RUN_TEST(test_system_telemetry_reaches_display_and_ecophi);
  RUN_TEST(test_dosing_executes_link_commands);
  RUN_TEST(test_system_set_dosing_parameter_from_main_board);
  RUN_TEST(test_system_pump_calibration_from_main_board);
  RUN_TEST(test_system_flowmeter_bucket_test_from_main_board);
  RUN_TEST(test_system_lost_reply_is_recovered_without_executing_twice);
  RUN_TEST(test_dosing_runs_same_command_again_after_main_board_reboot);
  RUN_TEST(test_system_dosing_command_without_dosing_board);
  RUN_TEST(test_system_level3_low_stops_dosing_and_shows_alarm);
  RUN_TEST(test_system_rejects_corrupted_i2c_frames);
  RUN_TEST(test_system_link_diagnostics);
  return UNITY_END();
}
