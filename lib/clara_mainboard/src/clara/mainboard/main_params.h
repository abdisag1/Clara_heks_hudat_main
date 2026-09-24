/**
 * @file main_params.h
 * @brief Calibration parameters of the main board.
 */
#ifndef CLARA_MAIN_PARAMS_H
#define CLARA_MAIN_PARAMS_H

#include "clara/param_store.h"

namespace clara {
namespace mainboard {

/** Parameter indices (console numbering). Append new entries at the end. */
enum Param {
  kProductionMin = 0,  ///< Electrolysis duration per batch, min.
  kSettlingMin,        ///< Settling time after electrolysis, min.
  kTransferMin,        ///< Valve open time to transfer the batch, min.
  kPolarityCycles,     ///< Reverse electrode polarity every N batches.
  kReportIntervalS,    ///< Ecophi report period, s.
  kLevelDebounceS,     ///< A level sensor must be stable this long to change state, s.
  kVoltageRef,         ///< ADC reference voltage as measured on the board, V.
  kVoltageDivider,     ///< Voltage sensor divider ratio (battery V per ADC V).
  kParamCount
};

const ParamInfo* parameterTable();

}  // namespace mainboard
}  // namespace clara

#endif  // CLARA_MAIN_PARAMS_H
