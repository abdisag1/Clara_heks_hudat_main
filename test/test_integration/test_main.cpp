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

void test_mainboard_resumes_batch_after_power_cut() {
  fakes::FakeClock clock;
  fakes::FakeEeprom eeprom;
  DeadLink link;
  {
    MainBench bench(clock, link, eeprom);
    bench.app.begin();
    bench.io.levels[0] = true;
    for (uint32_t t = 0; t < 95 * kMinute; t += 100) {
      clock.advanceMs(100);
      bench.app.update();
    }
    TEST_ASSERT_EQUAL(mainboard::kStateProduction, bench.app.cycle().state());
  }
  // Power cut; the board restarts 2 s later with a fresh millis().
  fakes::FakeClock rebootClock;
  MainBench rebooted(rebootClock, link, eeprom);
  rebooted.app.begin();
  TEST_ASSERT_TRUE(rebooted.app.status().progressRestored);
  TEST_ASSERT_EQUAL(mainboard::kStateProduction, rebooted.app.cycle().state());
  TEST_ASSERT_TRUE(rebooted.io.outputs.electrolysis);
  // Saved every 10 min: at most 10 min are repeated, never skipped.
  const uint32_t remaining = rebooted.app.cycle().remainingMs(rebootClock.millis());
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(180 * kMinute - 95 * kMinute, remaining);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(180 * kMinute - 85 * kMinute, remaining);
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
    }
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
  RUN_TEST(test_manual_dose_is_exact_and_not_counted_as_dosing);
  RUN_TEST(test_stop_command_aborts_pump);
  RUN_TEST(test_flow_simulation_drives_dosing_without_water);
  RUN_TEST(test_changed_interval_takes_effect);
  RUN_TEST(test_status_and_help_commands);
  RUN_TEST(test_mainboard_full_production_cycle);
  RUN_TEST(test_mainboard_resumes_batch_after_power_cut);
  RUN_TEST(test_mainboard_imports_v22_settings);
  RUN_TEST(test_mainboard_console_changes_production_time);
  RUN_TEST(test_mainboard_shows_link_loss);
  RUN_TEST(test_system_telemetry_reaches_display_and_ecophi);
  RUN_TEST(test_system_level3_low_stops_dosing_and_shows_alarm);
  RUN_TEST(test_system_rejects_corrupted_i2c_frames);
  return UNITY_END();
}
