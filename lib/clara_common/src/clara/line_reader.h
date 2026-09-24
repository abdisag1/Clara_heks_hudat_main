/**
 * @file line_reader.h
 * @brief Non-blocking assembly of text commands from a serial port.
 */
#ifndef CLARA_LINE_READER_H
#define CLARA_LINE_READER_H

#include <stdint.h>

namespace clara {

/**
 * Collects characters until CR or LF. Unlike Serial.readBytesUntil() (used in
 * v2.2) it never blocks, so a half-typed command cannot stall dosing or
 * production timing for the 1 s Stream timeout.
 */
class LineReader {
 public:
  static const uint8_t kCapacity = 48;  ///< Longest accepted line, excluding terminator.

  LineReader() : length_(0), ready_(false), overflowed_(false) { buffer_[0] = '\0'; }

  /**
   * Feeds one received character.
   * @return true when a complete, non-empty line is available via line().
   *         Over-long lines are discarded and reported through overflowed().
   */
  bool push(char c);

  /** The last completed line (valid until the next push()). Mutable so it can be tokenised in place. */
  char* line() { return buffer_; }

  /** True if the last line was too long and has been discarded. */
  bool overflowed() const { return overflowed_; }

 private:
  char buffer_[kCapacity + 1];
  uint8_t length_;
  bool ready_;
  bool overflowed_;
};

/**
 * Splits @p line in place at spaces/tabs.
 * @return number of tokens stored in @p tokens (at most @p maxTokens).
 */
uint8_t splitTokens(char* line, char** tokens, uint8_t maxTokens);

/** Strict float parser: the whole token must be a number. */
bool parseFloat(const char* text, float& value);

/** Strict unsigned integer parser (decimal). */
bool parseUInt(const char* text, uint32_t& value);

}  // namespace clara

#endif  // CLARA_LINE_READER_H
