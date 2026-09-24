#include "clara/mainboard/main_params.h"

#include "clara/progmem.h"

namespace clara {
namespace mainboard {

namespace {

const char kNameProduction[] CLARA_PROGMEM = "production_min";
const char kNameSettling[] CLARA_PROGMEM = "settling_min";
const char kNameTransfer[] CLARA_PROGMEM = "transfer_min";
const char kNamePolarity[] CLARA_PROGMEM = "polarity_cycles";
const char kNameReport[] CLARA_PROGMEM = "report_interval";
const char kNameDebounce[] CLARA_PROGMEM = "level_debounce";
const char kNameVref[] CLARA_PROGMEM = "voltage_ref";
const char kNameDivider[] CLARA_PROGMEM = "voltage_divider";
const char kNameResume[] CLARA_PROGMEM = "resume_batch";

const char kUnitMin[] CLARA_PROGMEM = "min";
const char kUnitS[] CLARA_PROGMEM = "s";
const char kUnitV[] CLARA_PROGMEM = "V";
const char kUnitNone[] CLARA_PROGMEM = "";

const char kHelpProduction[] CLARA_PROGMEM = "electrolysis time per batch";
const char kHelpSettling[] CLARA_PROGMEM = "settling time after electrolysis";
const char kHelpTransfer[] CLARA_PROGMEM = "valve open time to transfer a batch";
const char kHelpPolarity[] CLARA_PROGMEM = "reverse electrodes every N batches";
const char kHelpReport[] CLARA_PROGMEM = "Ecophi report period";
const char kHelpDebounce[] CLARA_PROGMEM = "level sensor filter time";
const char kHelpVref[] CLARA_PROGMEM = "ADC reference (measure 5V pin)";
const char kHelpDivider[] CLARA_PROGMEM = "voltage sensor divider ratio";
const char kHelpResume[] CLARA_PROGMEM = "1 = continue batch after power cut";

// Defaults reproduce the behaviour of the v2.2 field firmware.
const ParamInfo kTable[kParamCount] CLARA_PROGMEM = {
    // name            unit       help             min    max      default  dec
    {kNameProduction,  kUnitMin,  kHelpProduction, 1.0f,  1440.0f, 180.0f,  0},
    {kNameSettling,    kUnitMin,  kHelpSettling,   0.0f,  240.0f,  5.0f,    0},
    {kNameTransfer,    kUnitMin,  kHelpTransfer,   1.0f,  240.0f,  10.0f,   0},
    {kNamePolarity,    kUnitNone, kHelpPolarity,   1.0f,  100.0f,  1.0f,    0},
    {kNameReport,      kUnitS,    kHelpReport,     5.0f,  3600.0f, 60.0f,   0},
    {kNameDebounce,    kUnitS,    kHelpDebounce,   1.0f,  120.0f,  10.0f,   0},
    {kNameVref,        kUnitV,    kHelpVref,       1.0f,  5.5f,    4.85f,   3},
    {kNameDivider,     kUnitNone, kHelpDivider,    1.0f,  100.0f,  22.2f,   3},
    {kNameResume,      kUnitNone, kHelpResume,     0.0f,  1.0f,    1.0f,    0},
};

}  // namespace

const ParamInfo* parameterTable() { return kTable; }

}  // namespace mainboard
}  // namespace clara
