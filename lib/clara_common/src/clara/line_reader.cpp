#include "clara/line_reader.h"

#include <stdlib.h>

namespace clara {

bool LineReader::push(char c) {
  if (ready_) {  // the previous line has been consumed; start a new one
    length_ = 0;
    ready_ = false;
  }
  if (c == '\r' || c == '\n') {
    if (overflowed_) {  // end of a discarded line: resynchronise
      length_ = 0;
      return false;
    }
    if (length_ == 0) return false;  // empty line or second half of CR LF
    buffer_[length_] = '\0';
    ready_ = true;
    return true;
  }
  if (length_ == 0) overflowed_ = false;  // a new line begins
  if (length_ >= kCapacity) {
    overflowed_ = true;
    return false;
  }
  buffer_[length_++] = c;
  return false;
}

uint8_t splitTokens(char* line, char** tokens, uint8_t maxTokens) {
  uint8_t count = 0;
  char* cursor = line;
  while (*cursor != '\0' && count < maxTokens) {
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    if (*cursor == '\0') break;
    tokens[count++] = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') ++cursor;
    if (*cursor != '\0') *cursor++ = '\0';
  }
  return count;
}

bool parseFloat(const char* text, float& value) {
  if (text == 0 || *text == '\0') return false;
  char* end = 0;
  const double parsed = strtod(text, &end);
  if (end == text || *end != '\0') return false;
  value = static_cast<float>(parsed);
  return true;
}

bool parseUInt(const char* text, uint32_t& value) {
  if (text == 0 || *text == '\0') return false;
  uint32_t result = 0;
  for (const char* p = text; *p != '\0'; ++p) {
    if (*p < '0' || *p > '9') return false;
    const uint32_t digit = static_cast<uint32_t>(*p - '0');
    if (result > (0xFFFFFFFFul - digit) / 10u) return false;  // overflow
    result = result * 10u + digit;
  }
  value = result;
  return true;
}

}  // namespace clara
