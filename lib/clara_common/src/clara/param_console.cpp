#include "clara/param_console.h"

#include "clara/line_reader.h"
#include "clara/progmem.h"

namespace clara {

namespace {

bool equalsFlash(const char* token, const char* flashText) {
  return clara_strcasecmp_P(token, flashText) == 0;
}

}  // namespace

void ParamConsole::printHelp() {
  out_.printLineFlash(CLARA_F("  get                   list all parameters"));
  out_.printLineFlash(CLARA_F("  get <name|#>          show one parameter"));
  out_.printLineFlash(CLARA_F("  set <name|#> <value>  change and save a parameter"));
  out_.printLineFlash(CLARA_F("  defaults              restore factory calibration"));
}

void ParamConsole::printParam(uint8_t index) {
  const ParamInfo description = params_.info(index);
  if (index < 10) out_.write(" ");
  out_.printUInt(index);
  out_.write("  ");
  out_.printFlash(description.name);
  out_.write(" = ");
  out_.printFloat(params_.get(index), description.decimals);
  out_.write(" ");
  out_.printFlash(description.unit);
  out_.write("  [");
  out_.printFloat(description.minValue, description.decimals);
  out_.write(" .. ");
  out_.printFloat(description.maxValue, description.decimals);
  out_.write("]  ");
  out_.printFlash(description.help);
  out_.newline();
}

void ParamConsole::printAll() {
  for (uint8_t i = 0; i < params_.count(); ++i) printParam(i);
}

bool ParamConsole::handle(char** tokens, uint8_t count, bool& changed) {
  changed = false;
  if (count == 0) return false;

  if (equalsFlash(tokens[0], CLARA_F("get"))) {
    if (count == 1) {
      printAll();
      return true;
    }
    const int16_t index = params_.find(tokens[1]);
    if (index < 0) {
      out_.printLineFlash(CLARA_F("error: unknown parameter (type 'get' for the list)"));
    } else {
      printParam(static_cast<uint8_t>(index));
    }
    return true;
  }

  if (equalsFlash(tokens[0], CLARA_F("set"))) {
    if (count != 3) {
      out_.printLineFlash(CLARA_F("usage: set <name|#> <value>"));
      return true;
    }
    const int16_t index = params_.find(tokens[1]);
    float value;
    if (index < 0) {
      out_.printLineFlash(CLARA_F("error: unknown parameter (type 'get' for the list)"));
    } else if (!parseFloat(tokens[2], value)) {
      out_.printLineFlash(CLARA_F("error: value is not a number"));
    } else if (params_.set(static_cast<uint8_t>(index), value) != kSetOk) {
      out_.printFlash(CLARA_F("error: out of range, allowed "));
      const ParamInfo description = params_.info(static_cast<uint8_t>(index));
      out_.printFloat(description.minValue, description.decimals);
      out_.write(" .. ");
      out_.printFloat(description.maxValue, description.decimals);
      out_.newline();
    } else {
      changed = true;
      out_.printFlash(CLARA_F("ok: "));
      printParam(static_cast<uint8_t>(index));
    }
    return true;
  }

  if (equalsFlash(tokens[0], CLARA_F("defaults"))) {
    params_.resetToDefaults();
    changed = true;
    out_.printLineFlash(CLARA_F("ok: factory defaults restored"));
    return true;
  }

  return false;
}

}  // namespace clara
