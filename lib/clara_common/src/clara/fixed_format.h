/**
 * @file fixed_format.h
 * @brief Allocation-free number formatting.
 *
 * avr-libc's printf family has no %f support and Arduino's String class
 * fragments the tiny heap, so all numbers shown on the LCD, the console or the
 * Ecophi link are formatted with these helpers.
 */
#ifndef CLARA_FIXED_FORMAT_H
#define CLARA_FIXED_FORMAT_H

#include <stdint.h>

namespace clara {

/**
 * Formats @p value with exactly @p decimals digits after the decimal point,
 * rounding half away from zero (the same result Arduino's Serial.print(float)
 * gives). The output is always NUL-terminated and truncated to fit @p size.
 *
 * Special cases: NaN renders as "nan", values too large to format as "ovf".
 *
 * @param decimals clamped to 0..6.
 * @return number of characters written (excluding the terminator).
 */
uint8_t formatFixed(char* buffer, uint8_t size, float value, uint8_t decimals);

/** Formats an unsigned integer in decimal. Same conventions as formatFixed(). */
uint8_t formatUInt(char* buffer, uint8_t size, uint32_t value);

/**
 * Appends @p text to @p buffer (NUL-terminated, capacity @p size) and returns
 * the new length. Used to compose LCD lines without snprintf.
 */
uint8_t appendText(char* buffer, uint8_t size, const char* text);

/** Same as appendText() but reads @p flashText from flash. */
uint8_t appendFlash(char* buffer, uint8_t size, const char* flashText);

/** Appends a fixed-point number, see formatFixed(). */
uint8_t appendFixed(char* buffer, uint8_t size, float value, uint8_t decimals);

/** Appends an unsigned integer, see formatUInt(). */
uint8_t appendUInt(char* buffer, uint8_t size, uint32_t value);

}  // namespace clara

#endif  // CLARA_FIXED_FORMAT_H
