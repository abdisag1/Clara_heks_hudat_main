/**
 * @file param_console.h
 * @brief Serial-console commands shared by both boards: get / set / defaults.
 */
#ifndef CLARA_PARAM_CONSOLE_H
#define CLARA_PARAM_CONSOLE_H

#include <stdint.h>

#include "clara/param_store.h"
#include "clara/text_output.h"

namespace clara {

/**
 * Handles the generic parameter commands:
 *
 *   get                    list every parameter with value, unit and range
 *   get <name|#>           show one parameter
 *   set <name|#> <value>   change a parameter (range-checked)
 *   defaults               restore factory defaults
 *
 * The board console calls handle() first; if it returns false the command is
 * board specific. When handle() reports @p changed the caller persists the
 * parameters and applies them to the running application.
 */
class ParamConsole {
 public:
  ParamConsole(ParamStore& params, TextOutput& out) : params_(params), out_(out) {}

  /** @return true if the command was recognised (successfully or not). */
  bool handle(char** tokens, uint8_t count, bool& changed);

  /** Prints the generic command help lines. */
  void printHelp();

  /** Prints the whole parameter table. */
  void printAll();

  /** Prints one line: "#  name = value unit  [min .. max]  help". */
  void printParam(uint8_t index);

 private:
  ParamStore& params_;
  TextOutput& out_;
};

}  // namespace clara

#endif  // CLARA_PARAM_CONSOLE_H
