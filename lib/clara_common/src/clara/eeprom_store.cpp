#include "clara/eeprom_store.h"

#include "clara/byte_codec.h"
#include "clara/crc.h"

namespace clara {

namespace {

const uint16_t kParamMagic = 0x434C;  // "CL"
const uint8_t kMaxRingPayload = 16;   // keeps the scratch buffers on the stack small

uint16_t crc16Eeprom(const EepromDevice& eeprom, uint16_t address, uint16_t length) {
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < length; ++i) {
    const uint8_t byte = eeprom.read(static_cast<uint16_t>(address + i));
    crc = crc16(&byte, 1, crc);
  }
  return crc;
}

uint16_t readU16(const EepromDevice& eeprom, uint16_t address) {
  const uint8_t bytes[2] = {eeprom.read(address), eeprom.read(static_cast<uint16_t>(address + 1))};
  return getU16(bytes);
}

void writeBytes(EepromDevice& eeprom, uint16_t address, const uint8_t* bytes, uint8_t length) {
  for (uint8_t i = 0; i < length; ++i) eeprom.update(static_cast<uint16_t>(address + i), bytes[i]);
}

}  // namespace

// ---------------------------------------------------------------------------
// ParamPersistence
// ---------------------------------------------------------------------------

ParamPersistence::LoadResult ParamPersistence::load(ParamStore& params) const {
  params.resetToDefaults();

  if (readU16(eeprom_, base_) != kParamMagic) return kLoadEmpty;
  if (readU16(eeprom_, static_cast<uint16_t>(base_ + 2)) != params.schemaId() ||
      eeprom_.read(static_cast<uint16_t>(base_ + 4)) != params.count()) {
    return kLoadSchemaChanged;
  }

  const uint16_t payloadLength = static_cast<uint16_t>(5u + 4u * params.count());
  const uint16_t storedCrc = readU16(eeprom_, static_cast<uint16_t>(base_ + payloadLength));
  if (crc16Eeprom(eeprom_, base_, payloadLength) != storedCrc) return kLoadCorrupt;

  bool repaired = false;
  for (uint8_t i = 0; i < params.count(); ++i) {
    uint8_t bytes[4];
    for (uint8_t b = 0; b < 4; ++b) bytes[b] = eeprom_.read(static_cast<uint16_t>(base_ + 5u + 4u * i + b));
    if (params.set(i, getF32(bytes)) != kSetOk) repaired = true;  // keeps the default
  }
  return repaired ? kLoadRepaired : kLoadOk;
}

void ParamPersistence::save(const ParamStore& params) {
  uint8_t header[5];
  putU16(header, kParamMagic);
  putU16(header + 2, params.schemaId());
  header[4] = params.count();
  writeBytes(eeprom_, base_, header, sizeof(header));

  for (uint8_t i = 0; i < params.count(); ++i) {
    uint8_t bytes[4];
    putF32(bytes, params.get(i));
    writeBytes(eeprom_, static_cast<uint16_t>(base_ + 5u + 4u * i), bytes, sizeof(bytes));
  }

  const uint16_t payloadLength = static_cast<uint16_t>(5u + 4u * params.count());
  uint8_t crcBytes[2];
  putU16(crcBytes, crc16Eeprom(eeprom_, base_, payloadLength));
  writeBytes(eeprom_, static_cast<uint16_t>(base_ + payloadLength), crcBytes, sizeof(crcBytes));
}

// ---------------------------------------------------------------------------
// RingRecordStore
// ---------------------------------------------------------------------------

RingRecordStore::RingRecordStore(EepromDevice& eeprom, uint16_t baseAddress, uint8_t slotCount,
                                 uint8_t payloadSize)
    : eeprom_(eeprom),
      base_(baseAddress),
      slotCount_(slotCount == 0 ? 1 : slotCount),
      payloadSize_(payloadSize > kMaxRingPayload ? kMaxRingPayload : payloadSize),
      nextSlot_(0),
      nextSequence_(0),
      positionKnown_(false) {}

uint16_t RingRecordStore::slotAddress(uint8_t slot) const {
  return static_cast<uint16_t>(base_ + slot * (payloadSize_ + 3u));
}

bool RingRecordStore::readSlot(uint8_t slot, uint16_t& sequence, uint8_t* payload) const {
  uint8_t raw[kMaxRingPayload + 3];
  const uint8_t length = static_cast<uint8_t>(payloadSize_ + 3u);
  const uint16_t address = slotAddress(slot);
  for (uint8_t i = 0; i < length; ++i) raw[i] = eeprom_.read(static_cast<uint16_t>(address + i));

  // An erased slot (all 0xFF) must never be accepted, even if its CRC happened to match.
  bool erased = true;
  for (uint8_t i = 0; i < length; ++i) erased = erased && raw[i] == 0xFF;
  if (erased || crc8(raw, length - 1u) != raw[length - 1u]) return false;

  sequence = getU16(raw);
  if (payload != 0) {
    for (uint8_t i = 0; i < payloadSize_; ++i) static_cast<uint8_t*>(payload)[i] = raw[2 + i];
  }
  return true;
}

bool RingRecordStore::findNewest(uint8_t& newestSlot, uint16_t& newestSequence) const {
  bool found = false;
  for (uint8_t slot = 0; slot < slotCount_; ++slot) {
    uint16_t sequence;
    if (!readSlot(slot, sequence, 0)) continue;
    // Serial-number arithmetic: correct across the 16-bit sequence wrap as long
    // as the valid records are fewer than 32768 saves apart (always true here).
    if (!found || static_cast<int16_t>(sequence - newestSequence) > 0) {
      newestSlot = slot;
      newestSequence = sequence;
      found = true;
    }
  }
  return found;
}

bool RingRecordStore::load(void* payload) {
  uint8_t slot = 0;
  uint16_t sequence = 0;
  const bool found = findNewest(slot, sequence);
  if (found) {
    readSlot(slot, sequence, static_cast<uint8_t*>(payload));
    nextSlot_ = static_cast<uint8_t>((slot + 1u) % slotCount_);
    nextSequence_ = static_cast<uint16_t>(sequence + 1u);
  } else {
    nextSlot_ = 0;
    nextSequence_ = 0;
  }
  positionKnown_ = true;
  return found;
}

void RingRecordStore::save(const void* payload) {
  if (!positionKnown_) {
    uint8_t scratch[kMaxRingPayload];
    load(scratch);  // establishes nextSlot_ / nextSequence_
  }
  uint8_t raw[kMaxRingPayload + 3];
  putU16(raw, nextSequence_);
  for (uint8_t i = 0; i < payloadSize_; ++i) raw[2 + i] = static_cast<const uint8_t*>(payload)[i];
  const uint8_t length = static_cast<uint8_t>(payloadSize_ + 3u);
  raw[length - 1u] = crc8(raw, length - 1u);
  writeBytes(eeprom_, slotAddress(nextSlot_), raw, length);

  nextSlot_ = static_cast<uint8_t>((nextSlot_ + 1u) % slotCount_);
  nextSequence_ = static_cast<uint16_t>(nextSequence_ + 1u);
}

void RingRecordStore::clear() {
  const uint16_t length = footprint(slotCount_, payloadSize_);
  for (uint16_t i = 0; i < length; ++i) eeprom_.update(static_cast<uint16_t>(base_ + i), 0xFF);
  nextSlot_ = 0;
  nextSequence_ = 0;
  positionKnown_ = true;
}

}  // namespace clara
