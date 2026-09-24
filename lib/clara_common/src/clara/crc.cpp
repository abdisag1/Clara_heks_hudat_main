#include "clara/crc.h"

namespace clara {

uint8_t crc8(const uint8_t* data, size_t length, uint8_t crc) {
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80u) ? static_cast<uint8_t>((crc << 1) ^ 0x07u) : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

uint16_t crc16(const uint8_t* data, size_t length, uint16_t crc) {
  for (size_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1) ^ 0x1021u) : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

}  // namespace clara
