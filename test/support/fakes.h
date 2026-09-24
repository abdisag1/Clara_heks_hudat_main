/**
 * @file fakes.h
 * @brief Test doubles for every hardware interface.
 *
 * They let the complete applications run on a PC with simulated time: the
 * integration tests advance a FakeClock millisecond by millisecond while the
 * fake flowmeter produces pulses and the fake pump executes steps at the rate
 * the application commanded, exactly as the Timer1 interrupt would.
 */
#ifndef CLARA_TEST_FAKES_H
#define CLARA_TEST_FAKES_H

#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

#include "clara/clock.h"
#include "clara/digital_io.h"
#include "clara/dosing/dosing_hal.h"
#include "clara/eeprom_store.h"
#include "clara/mainboard/display_pages.h"
#include "clara/mainboard/mainboard_hal.h"
#include "clara/text_output.h"

namespace fakes {

/** Simulated time. micros() is derived from a 64-bit microsecond counter, so
 *  both millis() and micros() wrap exactly like the Arduino functions. */
class FakeClock : public clara::Clock {
 public:
  explicit FakeClock(uint64_t startUs = 0) : nowUs_(startUs) {}
  uint32_t millis() const override { return static_cast<uint32_t>(nowUs_ / 1000u); }
  uint32_t micros() const override { return static_cast<uint32_t>(nowUs_); }
  void advanceMs(uint32_t ms) { nowUs_ += static_cast<uint64_t>(ms) * 1000u; }
  void advanceUs(uint64_t us) { nowUs_ += us; }
  uint64_t nowUs() const { return nowUs_; }

 private:
  uint64_t nowUs_;
};

/** EEPROM in RAM, initialised to 0xFF like an erased chip. Counts physical writes. */
class FakeEeprom : public clara::EepromDevice {
 public:
  explicit FakeEeprom(uint16_t size = 4096) : bytes_(size, 0xFF), writes_(size, 0) {}
  uint8_t read(uint16_t address) const override { return address < bytes_.size() ? bytes_[address] : 0xFF; }
  void update(uint16_t address, uint8_t value) override {
    if (address >= bytes_.size() || bytes_[address] == value) return;
    bytes_[address] = value;
    ++writes_[address];
  }
  uint16_t size() const override { return static_cast<uint16_t>(bytes_.size()); }

  void poke(uint16_t address, uint8_t value) { bytes_[address] = value; }
  void pokeFloat(uint16_t address, float value) {
    uint8_t raw[4];
    memcpy(raw, &value, 4);  // host is little endian like the AVR
    for (int i = 0; i < 4; ++i) bytes_[address + i] = raw[i];
  }
  uint32_t writesAt(uint16_t address) const { return writes_[address]; }
  uint32_t maxWrites() const {
    uint32_t m = 0;
    for (size_t i = 0; i < writes_.size(); ++i) m = writes_[i] > m ? writes_[i] : m;
    return m;
  }

 private:
  std::vector<uint8_t> bytes_;
  std::vector<uint32_t> writes_;
};

/** Captures console / report output. */
class StringOutput : public clara::TextOutput {
 public:
  void write(const char* text) override { text_ += text; }
  void writeFlash(const char* text) override { text_ += text; }
  const std::string& text() const { return text_; }
  bool contains(const std::string& needle) const { return text_.find(needle) != std::string::npos; }
  void clear() { text_.clear(); }

 private:
  std::string text_;
};

class FakeInput : public clara::DigitalInput {
 public:
  explicit FakeInput(bool state = false) : state_(state) {}
  bool read() const override { return state_; }
  void set(bool state) { state_ = state; }

 private:
  bool state_;
};

/** Flowmeter producing pulses at a programmable frequency. */
class FakeFlowSensor : public clara::dosing::FlowSensor {
 public:
  explicit FakeFlowSensor(const FakeClock& clock) : clock_(clock), frequencyHz_(0.0), phase_(0.0), lastUs_(clock.nowUs()) {
    snapshot_.count = 0;
    snapshot_.lastPulseUs = 0;
  }

  void setFrequency(double hz) { frequencyHz_ = hz; }

  /** Generates the pulses due up to the clock's current time. */
  void advance() {
    const uint64_t now = clock_.nowUs();
    const double elapsedS = static_cast<double>(now - lastUs_) / 1e6;
    lastUs_ = now;
    phase_ += frequencyHz_ * elapsedS;
    while (phase_ >= 1.0) {
      phase_ -= 1.0;
      ++snapshot_.count;
      // Time of this pulse: 'phase_' periods before now.
      snapshot_.lastPulseUs = static_cast<uint32_t>(now - static_cast<uint64_t>(phase_ / frequencyHz_ * 1e6));
    }
  }

  clara::dosing::PulseSnapshot snapshot() const override { return snapshot_; }

 private:
  const FakeClock& clock_;
  double frequencyHz_;
  double phase_;
  uint64_t lastUs_;
  clara::dosing::PulseSnapshot snapshot_;
};

/** Pump that executes queued steps at the commanded rate as simulated time passes. */
class FakePump : public clara::dosing::PumpDriver {
 public:
  explicit FakePump(const FakeClock& clock)
      : clock_(clock), pending_(0), executed_(0), rateHz_(0), phase_(0.0), lastUs_(clock.nowUs()), maxRateSeen_(0) {}

  void addSteps(uint32_t steps) override { pending_ += steps; }
  void setStepRate(uint32_t hz) override {
    rateHz_ = hz;
    if (hz > maxRateSeen_) maxRateSeen_ = hz;
  }
  void abort() override {
    pending_ = 0;
    phase_ = 0.0;
    ++aborts;
  }
  uint32_t pendingSteps() const override { return pending_; }
  uint32_t executedSteps() const override { return executed_; }

  void advance() {
    const uint64_t now = clock_.nowUs();
    const double elapsedS = static_cast<double>(now - lastUs_) / 1e6;
    lastUs_ = now;
    if (pending_ == 0 || rateHz_ == 0) {
      phase_ = 0.0;
      return;
    }
    phase_ += rateHz_ * elapsedS;
    const uint32_t due = static_cast<uint32_t>(phase_);
    const uint32_t run = due < pending_ ? due : pending_;
    phase_ -= due;
    pending_ -= run;
    executed_ += run;
  }

  uint32_t rate() const { return rateHz_; }
  uint32_t maxRateSeen() const { return maxRateSeen_; }
  int aborts = 0;

 private:
  const FakeClock& clock_;
  uint32_t pending_;
  uint32_t executed_;
  uint32_t rateHz_;
  double phase_;
  uint64_t lastUs_;
  uint32_t maxRateSeen_;
};

/** Stores the latest published telemetry frame (what the I2C slave would serve). */
class FakeTelemetrySink : public clara::dosing::TelemetrySink {
 public:
  void publish(const uint8_t* frame, uint8_t length) override {
    frame_.assign(frame, frame + length);
    ++publishCount;
  }
  const std::vector<uint8_t>& frame() const { return frame_; }
  int publishCount = 0;

 private:
  std::vector<uint8_t> frame_;
};

/** I2C bus stand-in: the main board reads what the dosing board published. */
class LoopbackLink : public clara::mainboard::DosingLinkPort {
 public:
  explicit LoopbackLink(const FakeTelemetrySink& sink) : connected(true), corrupt(false), sink_(sink) {}
  uint8_t requestFrame(uint8_t* buffer, uint8_t capacity) override {
    if (!connected || sink_.frame().empty()) return 0;
    const uint8_t n = static_cast<uint8_t>(sink_.frame().size() < capacity ? sink_.frame().size() : capacity);
    memcpy(buffer, sink_.frame().data(), n);
    if (corrupt) buffer[5] ^= 0x40;
    return n;
  }
  bool connected;
  bool corrupt;

 private:
  const FakeTelemetrySink& sink_;
};

class FakeMainboardIo : public clara::mainboard::MainboardIo {
 public:
  FakeMainboardIo() : adc(0), chemicalSignal(false), outputs(), outputWrites(0) {
    for (int i = 0; i < 3; ++i) levels[i] = false;
  }
  bool readLevelSensor(uint8_t index) override { return index < 3 && levels[index]; }
  uint16_t readVoltageAdc() override { return adc; }
  void applyOutputs(const clara::mainboard::CycleOutputs& o) override {
    outputs = o;
    ++outputWrites;
  }
  void setChemicalAvailableSignal(bool available) override { chemicalSignal = available; }

  bool levels[3];
  uint16_t adc;
  bool chemicalSignal;
  clara::mainboard::CycleOutputs outputs;
  int outputWrites;
};

class FakeDisplay : public clara::mainboard::CharacterDisplay {
 public:
  FakeDisplay() : lines(clara::mainboard::kLcdRows) {}
  void writeLine(uint8_t row, const char* text) override { lines[row] = text; }
  std::string screen() const {
    std::string all;
    for (size_t i = 0; i < lines.size(); ++i) all += lines[i] + "|";
    return all;
  }
  bool shows(const std::string& needle) const { return screen().find(needle) != std::string::npos; }
  std::vector<std::string> lines;
};

}  // namespace fakes

#endif  // CLARA_TEST_FAKES_H
