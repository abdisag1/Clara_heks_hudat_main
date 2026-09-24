#include "clara/dosing/dosing_params.h"

#include "clara/progmem.h"

namespace clara {
namespace dosing {

namespace {

// Names, units and help texts live in flash to spare the Uno's 2 KiB of RAM.
const char kNameTargetFrc[] CLARA_PROGMEM = "target_frc";
const char kNameNaclO[] CLARA_PROGMEM = "naclo_strength";
const char kNameInterval[] CLARA_PROGMEM = "dose_interval";
const char kNameStepsRev[] CLARA_PROGMEM = "steps_per_rev";
const char kNameMlRev[] CLARA_PROGMEM = "ml_per_rev";
const char kNameKFactor[] CLARA_PROGMEM = "flow_k";
const char kNameCorrThreshold[] CLARA_PROGMEM = "flow_corr_min";
const char kNameCorrGain[] CLARA_PROGMEM = "flow_corr_gain";
const char kNameCorrOffset[] CLARA_PROGMEM = "flow_corr_offset";
const char kNameCoefLow[] CLARA_PROGMEM = "coef_low";
const char kNameCoefHigh[] CLARA_PROGMEM = "coef_high";
const char kNameCoefBand[] CLARA_PROGMEM = "coef_band";
const char kNameMaxRate[] CLARA_PROGMEM = "max_step_rate";
const char kNameTestRate[] CLARA_PROGMEM = "test_step_rate";

const char kUnitMgL[] CLARA_PROGMEM = "mg/L";
const char kUnitGL[] CLARA_PROGMEM = "g/L";
const char kUnitS[] CLARA_PROGMEM = "s";
const char kUnitSteps[] CLARA_PROGMEM = "steps";
const char kUnitMl[] CLARA_PROGMEM = "mL";
const char kUnitHzPerLpm[] CLARA_PROGMEM = "Hz/(L/min)";
const char kUnitLpm[] CLARA_PROGMEM = "L/min";
const char kUnitNone[] CLARA_PROGMEM = "";
const char kUnitStepRate[] CLARA_PROGMEM = "steps/s";

const char kHelpTargetFrc[] CLARA_PROGMEM = "chlorine to add to the water";
const char kHelpNaclO[] CLARA_PROGMEM = "strength of produced NaClO";
const char kHelpInterval[] CLARA_PROGMEM = "dosing control period";
const char kHelpStepsRev[] CLARA_PROGMEM = "driver microsteps per revolution";
const char kHelpMlRev[] CLARA_PROGMEM = "pump output per rev (use pumpcal)";
const char kHelpKFactor[] CLARA_PROGMEM = "flowmeter Hz per L/min (use flowcal)";
const char kHelpCorrThreshold[] CLARA_PROGMEM = "apply correction above this flow";
const char kHelpCorrGain[] CLARA_PROGMEM = "Q = gain * Qraw + offset";
const char kHelpCorrOffset[] CLARA_PROGMEM = "Q = gain * Qraw + offset";
const char kHelpCoefLow[] CLARA_PROGMEM = "dose multiplier, flow <= coef_band";
const char kHelpCoefHigh[] CLARA_PROGMEM = "dose multiplier, flow > coef_band";
const char kHelpCoefBand[] CLARA_PROGMEM = "flow separating the multipliers";
const char kHelpMaxRate[] CLARA_PROGMEM = "pump speed limit";
const char kHelpTestRate[] CLARA_PROGMEM = "pump speed for dose/pumpcal";

// Defaults reproduce the behaviour of the v2.2 field firmware.
const ParamInfo kTable[kParamCount] CLARA_PROGMEM = {
    // name               unit           help                min     max       default  dec
    {kNameTargetFrc,      kUnitMgL,      kHelpTargetFrc,     0.1f,   10.0f,    1.5f,    2},
    {kNameNaclO,          kUnitGL,       kHelpNaclO,         0.5f,   60.0f,    4.5f,    2},
    {kNameInterval,       kUnitS,        kHelpInterval,      5.0f,   300.0f,   20.0f,   0},
    {kNameStepsRev,       kUnitSteps,    kHelpStepsRev,      200.0f, 51200.0f, 6400.0f, 0},
    {kNameMlRev,          kUnitMl,       kHelpMlRev,         0.01f,  50.0f,    1.2f,    4},
    {kNameKFactor,        kUnitHzPerLpm, kHelpKFactor,       0.01f,  100.0f,   0.5f,    4},
    {kNameCorrThreshold,  kUnitLpm,      kHelpCorrThreshold, 0.0f,   2000.0f,  5.0f,    2},
    {kNameCorrGain,       kUnitNone,     kHelpCorrGain,      0.1f,   10.0f,    1.02f,   4},
    {kNameCorrOffset,     kUnitLpm,      kHelpCorrOffset,    -100.0f, 100.0f,  8.61f,   2},
    {kNameCoefLow,        kUnitNone,     kHelpCoefLow,       0.0f,   5.0f,     1.45f,   3},
    {kNameCoefHigh,       kUnitNone,     kHelpCoefHigh,      0.0f,   5.0f,     1.35f,   3},
    {kNameCoefBand,       kUnitLpm,      kHelpCoefBand,      0.0f,   5000.0f,  60.0f,   1},
    {kNameMaxRate,        kUnitStepRate, kHelpMaxRate,       100.0f, 15000.0f, 10000.0f, 0},
    {kNameTestRate,       kUnitStepRate, kHelpTestRate,      100.0f, 15000.0f, 3200.0f, 0},
};

}  // namespace

const ParamInfo* parameterTable() { return kTable; }

}  // namespace dosing
}  // namespace clara
