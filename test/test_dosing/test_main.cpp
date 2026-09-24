/**
 * Unit tests for lib/clara_dosing: flow measurement, dose maths, the dosing
 * engine, pump timer maths and calibration helpers.
 */
#include <math.h>
#include <unity.h>

#include "../support/fakes.h"
#include "clara/dosing/dose_calculator.h"
#include "clara/dosing/dosing_engine.h"
#include "clara/dosing/dosing_params.h"
#include "clara/dosing/flow_measurement.h"
#include "clara/dosing/step_timer.h"

using namespace clara;
using namespace clara::dosing;

void setUp() {}
void tearDown() {}

namespace {

FlowCalibration defaultFlowCalibration() {
  FlowCalibration c;
  c.kFactorHzPerLpm = 0.5f;
  c.correctionThresholdLpm = 5.0f;
  c.correctionGain = 1.02f;
  c.correctionOffsetLpm = 8.61f;
  return c;
}

DoseSettings defaultDoseSettings() {
  float values[kParamCount];
  ParamStore params(parameterTable(), kParamCount, values);
  return doseSettingsFromParams(params);
}

PulseSnapshot pulses(uint32_t count, uint32_t lastUs) {
  PulseSnapshot s;
  s.count = count;
  s.lastPulseUs = lastUs;
  return s;
}

}  // namespace

// --- Parameter table -------------------------------------------------------------

void test_every_default_is_inside_its_range() {
  float values[kParamCount];
  ParamStore params(parameterTable(), kParamCount, values);
  for (uint8_t i = 0; i < kParamCount; ++i) TEST_ASSERT_TRUE(params.isValid(i, params.get(i)));
}

void test_defaults_match_v22_field_values() {
  const DoseSettings s = defaultDoseSettings();
  TEST_ASSERT_EQUAL_FLOAT(1.5f, s.targetFrcMgL);
  TEST_ASSERT_EQUAL_FLOAT(4.5f, s.naclOStrengthGL);
  TEST_ASSERT_EQUAL_FLOAT(6400.0f / 1.2f, s.stepsPerMl);
}

// --- Flow calibration ----------------------------------------------------------

void test_flow_below_threshold_is_uncorrected() {
  TEST_ASSERT_EQUAL_FLOAT(4.0f, frequencyToFlowLpm(defaultFlowCalibration(), 2.0f));  // 2 Hz / 0.5
}

void test_flow_above_threshold_uses_linear_correction() {
  // 50 Hz / 0.5 = 100 L/min raw -> 1.02 * 100 + 8.61
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 110.61f, frequencyToFlowLpm(defaultFlowCalibration(), 50.0f));
}

void test_flow_is_never_negative() {
  FlowCalibration c = defaultFlowCalibration();
  c.correctionOffsetLpm = -50.0f;
  TEST_ASSERT_EQUAL_FLOAT(0.0f, frequencyToFlowLpm(c, 5.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, frequencyToFlowLpm(c, 0.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, frequencyToFlowLpm(c, -1.0f));
}

void test_k_factor_from_bucket_test() {
  // 3000 pulses for 100 L = 30 pulses/L = 0.5 Hz per L/min
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, kFactorFromBucketTest(3000, 100.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, kFactorFromBucketTest(0, 100.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, kFactorFromBucketTest(10, 0.0f));
}

// --- PulseFrequencyMeter -------------------------------------------------------

void test_frequency_meter_measures_between_pulse_timestamps() {
  PulseFrequencyMeter meter;
  meter.reset(pulses(0, 0));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, meter.update(pulses(1, 400000), 1000000));  // first pulse: reference only
  // 5 more pulses, last one 2.0 s after the reference -> 2.5 Hz exactly,
  // although a 1 s gate would have counted 2 or 3.
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.5f, meter.update(pulses(6, 2400000), 2500000));
}

void test_frequency_meter_decays_and_times_out_when_flow_stops() {
  PulseFrequencyMeter meter(3000000);
  meter.reset(pulses(0, 0));
  meter.update(pulses(1, 0), 10000);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 10.0f, meter.update(pulses(11, 1000000), 1000000));
  // No pulse for 2 s: the frequency cannot be above 0.5 Hz.
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.5f, meter.update(pulses(11, 1000000), 3000000));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, meter.update(pulses(11, 1000000), 4000001));
}

void test_frequency_meter_handles_micros_wrap() {
  PulseFrequencyMeter meter;
  meter.reset(pulses(100, 0xFFF00000ul));
  meter.update(pulses(101, 0xFFFF0000ul), 0xFFFF0000ul);
  // 20 pulses over 1 s, spanning the 32-bit micros() wrap.
  const uint32_t last = static_cast<uint32_t>(0xFFFF0000ul + 1000000ull);  // wraps past zero
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 20.0f, meter.update(pulses(121, last), last + 1000));
}

// --- VolumeTotalizer ------------------------------------------------------------

void test_totalizer_keeps_precision_for_large_totals() {
  VolumeTotalizer total;
  for (int i = 0; i < 100000; ++i) total.add(1.0f);    // 100 m3
  for (int i = 0; i < 1000; ++i) total.add(0.001f);    // + 1 L in small increments
  // A plain float total would be stuck: 100000 + 0.001 == 100000 in float.
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, total.liters() - 100000.0f);
  total.add(-5.0f);  // ignored
  total.add(NAN);    // ignored
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, total.liters() - 100000.0f);
}

// --- Dose maths ------------------------------------------------------------------

void test_dose_ratio() {
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.5f / 4.5f, doseRatioMlPerL(1.5f, 4.5f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, doseRatioMlPerL(1.5f, 0.0f));
}

void test_flow_coefficient_bands_match_v22() {
  const DoseSettings s = defaultDoseSettings();
  TEST_ASSERT_EQUAL_FLOAT(1.45f, flowCoefficient(s, 0.0f));
  TEST_ASSERT_EQUAL_FLOAT(1.45f, flowCoefficient(s, 60.0f));
  TEST_ASSERT_EQUAL_FLOAT(1.35f, flowCoefficient(s, 60.1f));
  // v2.2 fell through to 1.0 at exactly 160 L/min (a gap in its if/else chain).
  TEST_ASSERT_EQUAL_FLOAT(1.35f, flowCoefficient(s, 160.0f));
  TEST_ASSERT_EQUAL_FLOAT(1.35f, flowCoefficient(s, 500.0f));
}

void test_naclo_volume_worked_example() {
  // 100 L at 100 L/min: 100 x (1.5 / 4.5) x 1.35 = 45 mL
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 45.0f, naclOVolumeMl(defaultDoseSettings(), 100.0f, 100.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, naclOVolumeMl(defaultDoseSettings(), 0.0f, 0.0f));
}

void test_pump_calibration_maths() {
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.25f, mlPerRevFromMeasurement(20.0f, 25.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, mlPerRevFromMeasurement(0.0f, 25.0f));
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 5333.333f, stepsPerMl(6400.0f, 1.2f));
}

void test_step_quantizer_carries_fractions() {
  StepQuantizer quantizer;
  uint32_t total = 0;
  // 1000 intervals of 0.3 steps each: truncation would give 0, carry gives 300.
  for (int i = 0; i < 1000; ++i) total += quantizer.toSteps(0.3f, 1.0f);
  TEST_ASSERT_UINT32_WITHIN(1, 300u, total);
}

// --- DosingEngine ----------------------------------------------------------------

void test_engine_decides_once_per_interval() {
  DosingEngine engine;
  engine.configure(defaultDoseSettings(), 20000, 10000);
  engine.start(0);
  DoseDecision d;
  engine.addWater(10.0f);
  TEST_ASSERT_FALSE(engine.update(19999, true, 0, d));
  TEST_ASSERT_TRUE(engine.update(20000, true, 0, d));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 30.0f, d.averageFlowLpm);  // 10 L in 20 s
  TEST_ASSERT_EQUAL_FLOAT(1.45f, d.coefficient);
  const float expectedMl = 10.0f * (1.5f / 4.5f) * 1.45f;
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, expectedMl, d.doseMl);
  TEST_ASSERT_UINT32_WITHIN(1, static_cast<uint32_t>(expectedMl * 6400.0f / 1.2f), d.stepsToAdd);
  // Steps are spread over 95 % of the next interval.
  TEST_ASSERT_UINT32_WITHIN(2, static_cast<uint32_t>(ceil(d.stepsToAdd / 19.0)), d.stepRateHz);
  TEST_ASSERT_FALSE(d.saturated);
}

void test_engine_includes_pending_steps_in_rate() {
  DosingEngine engine;
  engine.configure(defaultDoseSettings(), 20000, 10000);
  engine.start(0);
  DoseDecision d;
  TEST_ASSERT_TRUE(engine.update(20000, true, 19000, d));  // no water, but a backlog
  TEST_ASSERT_EQUAL_UINT32(0, d.stepsToAdd);
  TEST_ASSERT_EQUAL_UINT32(1000, d.stepRateHz);  // 19000 steps in 19 s
}

void test_engine_reports_saturation() {
  DosingEngine engine;
  engine.configure(defaultDoseSettings(), 20000, 1000);  // slow pump
  engine.start(0);
  engine.addWater(100.0f);  // needs ~240 000 steps, pump can do 20 000 per interval
  DoseDecision d;
  TEST_ASSERT_TRUE(engine.update(20000, true, 0, d));
  TEST_ASSERT_TRUE(d.saturated);
  TEST_ASSERT_EQUAL_UINT32(1000, d.stepRateHz);
}

void test_engine_doses_nothing_without_chemical() {
  DosingEngine engine;
  engine.configure(defaultDoseSettings(), 20000, 10000);
  engine.start(0);
  engine.addWater(50.0f);
  DoseDecision d;
  TEST_ASSERT_TRUE(engine.update(20000, false, 5000, d));
  TEST_ASSERT_FALSE(d.chemicalAvailable);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, d.doseMl);
  TEST_ASSERT_EQUAL_UINT32(0, d.stepsToAdd);
  // The water of that interval is not carried into the next one.
  TEST_ASSERT_TRUE(engine.update(40000, true, 0, d));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, d.waterLiters);
}

void test_step_rate_for_window() {
  TEST_ASSERT_EQUAL_UINT32(0, stepRateForWindow(0, 1000, 100));
  TEST_ASSERT_EQUAL_UINT32(10, stepRateForWindow(10, 1000, 100));
  TEST_ASSERT_EQUAL_UINT32(11, stepRateForWindow(101, 10000, 100));  // rounds up
  TEST_ASSERT_EQUAL_UINT32(100, stepRateForWindow(4000000000ul, 1000, 100));
}

// --- Timer1 maths ---------------------------------------------------------------

void test_timer1_settings_are_accurate_over_the_whole_range() {
  const uint32_t cpu = 16000000ul;
  for (uint32_t rate = 1; rate <= 15000; rate = rate < 100 ? rate + 1 : rate + 37) {
    const Timer1Setting s = timer1ForStepRate(cpu, rate);
    TEST_ASSERT_NOT_EQUAL(0, s.prescaler);
    const float actual = actualStepRate(cpu, s);
    // Worst case error is half a timer tick: < 0.1 % over the useful range.
    TEST_ASSERT_FLOAT_WITHIN(rate * 0.001f, static_cast<float>(rate), actual);
  }
}

void test_timer1_setting_for_zero_rate_is_disabled() {
  TEST_ASSERT_EQUAL(0, timer1ForStepRate(16000000ul, 0).prescaler);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_default_is_inside_its_range);
  RUN_TEST(test_defaults_match_v22_field_values);
  RUN_TEST(test_flow_below_threshold_is_uncorrected);
  RUN_TEST(test_flow_above_threshold_uses_linear_correction);
  RUN_TEST(test_flow_is_never_negative);
  RUN_TEST(test_k_factor_from_bucket_test);
  RUN_TEST(test_frequency_meter_measures_between_pulse_timestamps);
  RUN_TEST(test_frequency_meter_decays_and_times_out_when_flow_stops);
  RUN_TEST(test_frequency_meter_handles_micros_wrap);
  RUN_TEST(test_totalizer_keeps_precision_for_large_totals);
  RUN_TEST(test_dose_ratio);
  RUN_TEST(test_flow_coefficient_bands_match_v22);
  RUN_TEST(test_naclo_volume_worked_example);
  RUN_TEST(test_pump_calibration_maths);
  RUN_TEST(test_step_quantizer_carries_fractions);
  RUN_TEST(test_engine_decides_once_per_interval);
  RUN_TEST(test_engine_includes_pending_steps_in_rate);
  RUN_TEST(test_engine_reports_saturation);
  RUN_TEST(test_engine_doses_nothing_without_chemical);
  RUN_TEST(test_step_rate_for_window);
  RUN_TEST(test_timer1_settings_are_accurate_over_the_whole_range);
  RUN_TEST(test_timer1_setting_for_zero_rate_is_disabled);
  return UNITY_END();
}
