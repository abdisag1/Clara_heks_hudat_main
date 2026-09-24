/**
 * @file dosing_console.h
 * @brief USB serial console of the dosing board (calibration and bench tests).
 */
#ifndef CLARA_DOSING_CONSOLE_H
#define CLARA_DOSING_CONSOLE_H

#include <stdint.h>

#include "clara/dosing/dosing_app.h"
#include "clara/line_reader.h"
#include "clara/param_console.h"
#include "clara/text_output.h"

namespace clara {
namespace dosing {

/**
 * Commands (type `help` in the serial monitor, 9600 baud, newline):
 *
 *   status                     live values
 *   get / set / defaults       calibration parameters (see ParamConsole)
 *   dose <mL>                  pump a fixed volume at test speed
 *   pumpcal <revs>             run the pump N revolutions ...
 *   pumpcal done <mL>          ... then enter the measured volume
 *   flowcal start              start counting flowmeter pulses ...
 *   flowcal done <litres>      ... then enter the collected volume
 *   flowcal cancel
 *   flowsim <L/min> | off      simulate a flow (bench test without water)
 *   stop                       stop the pump
 *   log on | off               print every dosing decision
 */
class DosingConsole {
 public:
  DosingConsole(DosingApp& app, TextOutput& out);

  /** Feed every received character. */
  void onChar(char c);

  /** Call from loop(): prints asynchronous events (decisions, run completion). */
  void update();

  void printBanner();

 private:
  void execute(char* line);
  void printHelp();
  void printStatus();
  void printDecision(const DoseDecision& decision);
  void handlePumpCal(char** tokens, uint8_t count);
  void handleFlowCal(char** tokens, uint8_t count);
  void handleFlowSim(char** tokens, uint8_t count);

  DosingApp& app_;
  TextOutput& out_;
  LineReader reader_;
  ParamConsole paramConsole_;
  bool logDecisions_;
  uint32_t lastDecisionPrinted_;
  Mode lastMode_;
};

}  // namespace dosing
}  // namespace clara

#endif  // CLARA_DOSING_CONSOLE_H
