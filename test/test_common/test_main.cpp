/**
 * Unit tests for lib/clara_common: timing, formatting, checksums, persistence,
 * the parameter table, the console and the inter-board protocol.
 */
#include <math.h>
#include <string.h>
#include <unity.h>

#include "../support/fakes.h"
#include "clara/byte_codec.h"
#include "clara/crc.h"
#include "clara/debouncer.h"
#include "clara/dosing_link.h"
#include "clara/eeprom_store.h"
#include "clara/fixed_format.h"
#include "clara/line_reader.h"
#include "clara/param_console.h"
#include "clara/param_store.h"
#include "clara/periodic_timer.h"

using namespace clara;

void setUp() {}
void tearDown() {}

// --- PeriodicTimer ---------------------------------------------------------------

void test_timer_fires_once_per_period() {
  PeriodicTimer timer(1000);
  timer.start(0);
  TEST_ASSERT_FALSE(timer.poll(999));
  TEST_ASSERT_TRUE(timer.poll(1000));
  TEST_ASSERT_FALSE(timer.poll(1500));
  TEST_ASSERT_TRUE(timer.poll(2000));
}

void test_timer_does_not_drift_with_loop_latency() {
  // Poll with an irregular 0..6 ms loop latency for one simulated day.
  PeriodicTimer timer(1000);
  timer.start(0);
  uint32_t ticks = 0;
  uint32_t now = 0;
  uint32_t jitter = 1;
  while (now < 86400000ul) {
    jitter = (jitter * 1103515245u + 12345u) & 0x7fffffffu;
    now += 1 + jitter % 6;
    if (timer.poll(now)) ++ticks;
  }
  // The naive "last = now" pattern would lose ~3 ticks per 1000; this must be exact.
  TEST_ASSERT_UINT32_WITHIN(1, 86400, ticks);
}

void test_timer_survives_millis_rollover() {
  PeriodicTimer timer(1000);
  timer.start(0xFFFFFE00ul);  // 512 ms before the 32-bit wrap
  TEST_ASSERT_FALSE(timer.poll(0xFFFFFFFFul));
  TEST_ASSERT_FALSE(timer.poll(0x000001E7ul));  // 999 ms after start, after the wrap
  TEST_ASSERT_TRUE(timer.poll(0x000001E8ul));   // exactly 1000 ms after start
}

void test_timer_realigns_after_long_stall_instead_of_bursting() {
  PeriodicTimer timer(1000);
  timer.start(0);
  TEST_ASSERT_TRUE(timer.poll(10500));  // stalled for 10 periods
  TEST_ASSERT_FALSE(timer.poll(10600));  // no burst of catch-up ticks
  TEST_ASSERT_TRUE(timer.poll(11500));
}

void test_stopped_timer_never_fires() {
  PeriodicTimer timer(10);
  TEST_ASSERT_FALSE(timer.poll(100));
  timer.start(0);
  timer.stop();
  TEST_ASSERT_FALSE(timer.poll(100));
}

// --- Debouncer ---------------------------------------------------------------------

void test_debouncer_requires_consecutive_samples() {
  Debouncer filter(3, false);
  TEST_ASSERT_FALSE(filter.update(true));
  TEST_ASSERT_FALSE(filter.update(true));
  TEST_ASSERT_FALSE(filter.update(false));  // glitch resets the count
  TEST_ASSERT_FALSE(filter.update(true));
  TEST_ASSERT_FALSE(filter.update(true));
  TEST_ASSERT_TRUE(filter.update(true));
  TEST_ASSERT_TRUE(filter.update(false));  // a single low sample is ignored
  TEST_ASSERT_TRUE(filter.update(true));
}

// --- Formatting --------------------------------------------------------------------

void test_format_fixed() {
  char buffer[24];
  formatFixed(buffer, sizeof(buffer), 12.345f, 2);
  TEST_ASSERT_EQUAL_STRING("12.35", buffer);
  formatFixed(buffer, sizeof(buffer), -0.5f, 1);
  TEST_ASSERT_EQUAL_STRING("-0.5", buffer);
  formatFixed(buffer, sizeof(buffer), -0.001f, 2);
  TEST_ASSERT_EQUAL_STRING("0.00", buffer);  // no "-0.00"
  formatFixed(buffer, sizeof(buffer), 3.0f, 0);
  TEST_ASSERT_EQUAL_STRING("3", buffer);
  formatFixed(buffer, sizeof(buffer), 0.05f, 3);
  TEST_ASSERT_EQUAL_STRING("0.050", buffer);
  formatFixed(buffer, sizeof(buffer), NAN, 2);
  TEST_ASSERT_EQUAL_STRING("nan", buffer);
  formatFixed(buffer, sizeof(buffer), 1e12f, 2);
  TEST_ASSERT_EQUAL_STRING("ovf", buffer);
}

void test_format_truncates_to_buffer() {
  char buffer[5];
  formatFixed(buffer, sizeof(buffer), 123456.0f, 1);
  TEST_ASSERT_EQUAL_STRING("1234", buffer);
  formatUInt(buffer, sizeof(buffer), 4294967295ul);
  TEST_ASSERT_EQUAL_STRING("4294", buffer);
}

void test_append_helpers() {
  char buffer[21] = "";
  appendText(buffer, sizeof(buffer), "Flow: ");
  appendFixed(buffer, sizeof(buffer), 1.5f, 2);
  appendFlash(buffer, sizeof(buffer), " L/min");
  TEST_ASSERT_EQUAL_STRING("Flow: 1.50 L/min", buffer);
  appendText(buffer, sizeof(buffer), " overflowing text");
  TEST_ASSERT_EQUAL_UINT(20, strlen(buffer));
}

// --- CRC / codec -------------------------------------------------------------------

void test_crc_check_values() {
  const uint8_t text[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  TEST_ASSERT_EQUAL_HEX8(0xF4, crc8(text, sizeof(text)));
  TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16(text, sizeof(text)));
}

void test_byte_codec_roundtrip() {
  uint8_t bytes[4];
  putU32(bytes, 0x12345678ul);
  TEST_ASSERT_EQUAL_HEX8(0x78, bytes[0]);  // little endian
  TEST_ASSERT_EQUAL_HEX32(0x12345678ul, getU32(bytes));
  putF32(bytes, -3.25f);
  TEST_ASSERT_EQUAL_FLOAT(-3.25f, getF32(bytes));
}

// --- LineReader --------------------------------------------------------------------

bool feed(LineReader& reader, const char* text) {
  bool complete = false;
  for (const char* p = text; *p; ++p) complete = reader.push(*p);
  return complete;
}

void test_line_reader_handles_crlf_and_empty_lines() {
  LineReader reader;
  TEST_ASSERT_TRUE(feed(reader, "set 1 2\r"));
  TEST_ASSERT_EQUAL_STRING("set 1 2", reader.line());
  TEST_ASSERT_FALSE(feed(reader, "\n"));  // LF of CR LF is not a second line
  TEST_ASSERT_TRUE(feed(reader, "get\n"));
  TEST_ASSERT_EQUAL_STRING("get", reader.line());
}

void test_line_reader_discards_overlong_lines() {
  LineReader reader;
  std::string longLine(LineReader::kCapacity + 10, 'x');
  longLine += "\n";
  TEST_ASSERT_FALSE(feed(reader, longLine.c_str()));
  TEST_ASSERT_TRUE(reader.overflowed());
  TEST_ASSERT_TRUE(feed(reader, "status\n"));
  TEST_ASSERT_EQUAL_STRING("status", reader.line());
}

void test_tokenizer_and_parsers() {
  char line[] = "  set   target_frc\t1.75 ";
  char* tokens[4];
  TEST_ASSERT_EQUAL_UINT8(3, splitTokens(line, tokens, 4));
  TEST_ASSERT_EQUAL_STRING("set", tokens[0]);
  TEST_ASSERT_EQUAL_STRING("target_frc", tokens[1]);
  float value = 0;
  TEST_ASSERT_TRUE(parseFloat(tokens[2], value));
  TEST_ASSERT_EQUAL_FLOAT(1.75f, value);
  TEST_ASSERT_FALSE(parseFloat("1.5x", value));
  TEST_ASSERT_FALSE(parseFloat("", value));
  uint32_t number = 0;
  TEST_ASSERT_TRUE(parseUInt("4294967295", number));
  TEST_ASSERT_FALSE(parseUInt("4294967296", number));
  TEST_ASSERT_FALSE(parseUInt("-1", number));
}

// --- ParamStore / persistence ---------------------------------------------------

const ParamInfo kTestTable[] = {
    {"alpha", "s", "first", 0.0f, 10.0f, 5.0f, 1},
    {"beta", "mL", "second", -1.0f, 1.0f, 0.5f, 2},
};
const ParamInfo kOtherTable[] = {
    {"alpha", "s", "first", 0.0f, 10.0f, 5.0f, 1},
    {"gamma", "mL", "renamed", -1.0f, 1.0f, 0.5f, 2},
};

void test_param_store_defaults_find_and_range() {
  float values[2];
  ParamStore store(kTestTable, 2, values);
  TEST_ASSERT_EQUAL_FLOAT(5.0f, store.get(0));
  TEST_ASSERT_EQUAL_INT16(1, store.find("BETA"));
  TEST_ASSERT_EQUAL_INT16(1, store.find("1"));
  TEST_ASSERT_EQUAL_INT16(-1, store.find("2"));
  TEST_ASSERT_EQUAL_INT16(-1, store.find("delta"));
  TEST_ASSERT_EQUAL(kSetOk, store.set(0, 10.0f));
  TEST_ASSERT_EQUAL(kSetOutOfRange, store.set(0, 10.01f));
  TEST_ASSERT_EQUAL(kSetOutOfRange, store.set(1, NAN));
  TEST_ASSERT_EQUAL(kSetUnknownParameter, store.set(7, 1.0f));
  TEST_ASSERT_EQUAL_FLOAT(10.0f, store.get(0));
}

void test_param_persistence_roundtrip() {
  fakes::FakeEeprom eeprom;
  float values[2];
  ParamStore store(kTestTable, 2, values);
  ParamPersistence persistence(eeprom, 100);
  TEST_ASSERT_EQUAL(ParamPersistence::kLoadEmpty, persistence.load(store));

  store.set(0, 7.5f);
  store.set(1, -0.25f);
  persistence.save(store);

  float loadedValues[2];
  ParamStore loaded(kTestTable, 2, loadedValues);
  TEST_ASSERT_EQUAL(ParamPersistence::kLoadOk, persistence.load(loaded));
  TEST_ASSERT_EQUAL_FLOAT(7.5f, loaded.get(0));
  TEST_ASSERT_EQUAL_FLOAT(-0.25f, loaded.get(1));
}

void test_param_persistence_rejects_corruption_and_schema_change() {
  fakes::FakeEeprom eeprom;
  float values[2];
  ParamStore store(kTestTable, 2, values);
  ParamPersistence persistence(eeprom, 0);
  store.set(0, 1.0f);
  persistence.save(store);

  float otherValues[2];
  ParamStore other(kOtherTable, 2, otherValues);
  TEST_ASSERT_EQUAL(ParamPersistence::kLoadSchemaChanged, persistence.load(other));

  eeprom.poke(6, eeprom.read(6) ^ 0x01);  // flip one bit of a value
  TEST_ASSERT_EQUAL(ParamPersistence::kLoadCorrupt, persistence.load(store));
  TEST_ASSERT_EQUAL_FLOAT(5.0f, store.get(0));  // defaults restored
}

void test_param_persistence_accepts_record_of_shorter_table() {
  // Firmware update that appended "beta": the saved "alpha" must survive.
  fakes::FakeEeprom eeprom;
  ParamPersistence persistence(eeprom, 0);
  float oldValues[1];
  ParamStore oldStore(kTestTable, 1, oldValues);
  oldStore.set(0, 9.0f);
  persistence.save(oldStore);

  float values[2];
  ParamStore store(kTestTable, 2, values);
  TEST_ASSERT_EQUAL(ParamPersistence::kLoadExtended, persistence.load(store));
  TEST_ASSERT_EQUAL_FLOAT(9.0f, store.get(0));
  TEST_ASSERT_EQUAL_FLOAT(0.5f, store.get(1));  // new parameter: default
  persistence.save(store);
  TEST_ASSERT_EQUAL(ParamPersistence::kLoadOk, persistence.load(store));

  // The other direction (downgrade) is rejected.
  TEST_ASSERT_EQUAL(ParamPersistence::kLoadSchemaChanged, persistence.load(oldStore));
}

void test_param_persistence_only_writes_changed_bytes() {
  fakes::FakeEeprom eeprom;
  float values[2];
  ParamStore store(kTestTable, 2, values);
  ParamPersistence persistence(eeprom, 0);
  persistence.save(store);
  persistence.save(store);
  persistence.save(store);
  TEST_ASSERT_EQUAL_UINT32(1, eeprom.maxWrites());
}

// --- RingRecordStore -----------------------------------------------------------

void test_ring_store_returns_newest_record_across_wrap() {
  fakes::FakeEeprom eeprom;
  RingRecordStore ring(eeprom, 10, 4, 2);
  uint8_t payload[2];
  TEST_ASSERT_FALSE(ring.load(payload));
  for (uint16_t i = 0; i < 1000; ++i) {
    payload[0] = static_cast<uint8_t>(i);
    payload[1] = static_cast<uint8_t>(i >> 8);
    ring.save(payload);
  }
  RingRecordStore reopened(eeprom, 10, 4, 2);  // e.g. after a reset
  TEST_ASSERT_TRUE(reopened.load(payload));
  TEST_ASSERT_EQUAL_UINT16(999, payload[0] | (payload[1] << 8));
  // Wear levelling: 1000 saves spread over 4 slots.
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(250, eeprom.maxWrites());
}

void test_ring_store_falls_back_when_newest_slot_is_torn() {
  fakes::FakeEeprom eeprom;
  RingRecordStore ring(eeprom, 0, 4, 1);
  uint8_t payload = 1;
  ring.save(&payload);  // slot 0
  payload = 2;
  ring.save(&payload);  // slot 1
  eeprom.poke(1 * 4 + 2, 0x00);  // power cut while writing slot 1: damage its payload
  RingRecordStore reopened(eeprom, 0, 4, 1);
  TEST_ASSERT_TRUE(reopened.load(&payload));
  TEST_ASSERT_EQUAL_UINT8(1, payload);
}

void test_ring_store_sequence_wraps() {
  fakes::FakeEeprom eeprom;
  RingRecordStore ring(eeprom, 0, 3, 1);
  uint8_t payload = 0;
  for (uint32_t i = 0; i < 70000ul; ++i) {  // more than 2^16 saves
    payload = static_cast<uint8_t>(i % 251);
    ring.save(&payload);
  }
  RingRecordStore reopened(eeprom, 0, 3, 1);
  uint8_t loaded = 0;
  TEST_ASSERT_TRUE(reopened.load(&loaded));
  TEST_ASSERT_EQUAL_UINT8(payload, loaded);
}

// --- ParamConsole --------------------------------------------------------------

void run(ParamConsole& console, const char* command, bool& changed) {
  char line[64];
  strncpy(line, command, sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  char* tokens[4];
  const uint8_t count = splitTokens(line, tokens, 4);
  TEST_ASSERT_TRUE(console.handle(tokens, count, changed));
}

void test_param_console_set_get_defaults() {
  float values[2];
  ParamStore store(kTestTable, 2, values);
  fakes::StringOutput out;
  ParamConsole console(store, out);
  bool changed = false;

  run(console, "set alpha 2.5", changed);
  TEST_ASSERT_TRUE(changed);
  TEST_ASSERT_EQUAL_FLOAT(2.5f, store.get(0));

  run(console, "set 1 3", changed);
  TEST_ASSERT_FALSE(changed);
  TEST_ASSERT_TRUE(out.contains("out of range"));

  out.clear();
  run(console, "get", changed);
  TEST_ASSERT_TRUE(out.contains("alpha = 2.5 s"));
  TEST_ASSERT_TRUE(out.contains("beta = 0.50 mL"));

  run(console, "defaults", changed);
  TEST_ASSERT_TRUE(changed);
  TEST_ASSERT_EQUAL_FLOAT(5.0f, store.get(0));

  char line[] = "status";
  char* tokens[1] = {line};
  TEST_ASSERT_FALSE(console.handle(tokens, 1, changed));  // not a parameter command
}

// --- Dosing link protocol ------------------------------------------------------

link::DosingTelemetry sampleTelemetry() {
  link::DosingTelemetry t;
  t.sequence = 42;
  t.flags = link::kFlagChemicalAvailable | link::kFlagPumpRunning;
  t.flowLpm = 123.25f;
  t.lastDoseMl = 13.5f;
  t.totalWaterL = 987654u;
  t.totalNaclOMicroL = 123456789u;
  t.targetFrcMgL = 1.5f;
  t.uptimeS = 3600u;
  return t;
}

void test_telemetry_roundtrip() {
  uint8_t frame[link::kTelemetryFrameSize];
  link::encodeTelemetry(sampleTelemetry(), frame);
  link::DosingTelemetry decoded;
  TEST_ASSERT_EQUAL(link::kDecodeOk, link::decodeTelemetry(frame, sizeof(frame), decoded));
  TEST_ASSERT_EQUAL_UINT8(42, decoded.sequence);
  TEST_ASSERT_EQUAL_FLOAT(123.25f, decoded.flowLpm);
  TEST_ASSERT_EQUAL_UINT32(987654u, decoded.totalWaterL);
  TEST_ASSERT_EQUAL_UINT32(123456789u, decoded.totalNaclOMicroL);
  TEST_ASSERT_EQUAL_UINT32(3600u, decoded.uptimeS);
  TEST_ASSERT_LESS_OR_EQUAL(32, link::kTelemetryFrameSize);  // AVR Wire buffer
}

void test_telemetry_rejects_bad_frames() {
  uint8_t frame[link::kTelemetryFrameSize];
  link::encodeTelemetry(sampleTelemetry(), frame);
  link::DosingTelemetry decoded;
  TEST_ASSERT_EQUAL(link::kDecodeBadLength, link::decodeTelemetry(frame, 16, decoded));

  uint8_t corrupted[link::kTelemetryFrameSize];
  for (uint8_t bit = 0; bit < 8 * (link::kTelemetryFrameSize - 1); ++bit) {
    memcpy(corrupted, frame, sizeof(frame));
    corrupted[bit / 8] ^= static_cast<uint8_t>(1u << (bit % 8));
    TEST_ASSERT_NOT_EQUAL(link::kDecodeOk, link::decodeTelemetry(corrupted, sizeof(corrupted), decoded));
  }

  uint8_t zeros[link::kTelemetryFrameSize] = {0};  // what a slave returns before its first publish
  TEST_ASSERT_NOT_EQUAL(link::kDecodeOk, link::decodeTelemetry(zeros, sizeof(zeros), decoded));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_timer_fires_once_per_period);
  RUN_TEST(test_timer_does_not_drift_with_loop_latency);
  RUN_TEST(test_timer_survives_millis_rollover);
  RUN_TEST(test_timer_realigns_after_long_stall_instead_of_bursting);
  RUN_TEST(test_stopped_timer_never_fires);
  RUN_TEST(test_debouncer_requires_consecutive_samples);
  RUN_TEST(test_format_fixed);
  RUN_TEST(test_format_truncates_to_buffer);
  RUN_TEST(test_append_helpers);
  RUN_TEST(test_crc_check_values);
  RUN_TEST(test_byte_codec_roundtrip);
  RUN_TEST(test_line_reader_handles_crlf_and_empty_lines);
  RUN_TEST(test_line_reader_discards_overlong_lines);
  RUN_TEST(test_tokenizer_and_parsers);
  RUN_TEST(test_param_store_defaults_find_and_range);
  RUN_TEST(test_param_persistence_roundtrip);
  RUN_TEST(test_param_persistence_rejects_corruption_and_schema_change);
  RUN_TEST(test_param_persistence_accepts_record_of_shorter_table);
  RUN_TEST(test_param_persistence_only_writes_changed_bytes);
  RUN_TEST(test_ring_store_returns_newest_record_across_wrap);
  RUN_TEST(test_ring_store_falls_back_when_newest_slot_is_torn);
  RUN_TEST(test_ring_store_sequence_wraps);
  RUN_TEST(test_param_console_set_get_defaults);
  RUN_TEST(test_telemetry_roundtrip);
  RUN_TEST(test_telemetry_rejects_bad_frames);
  return UNITY_END();
}
