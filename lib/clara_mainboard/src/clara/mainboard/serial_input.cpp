#include "clara/mainboard/serial_input.h"

namespace clara {
namespace mainboard {

SerialInputSwitch::SerialInputSwitch()
    : remoteEnabled_(false),
      usbWindowMs_(0),
      usbIdleMs_(0),
      mode_(kUsb),
      usbUsed_(false),
      bootMs_(0),
      lastInputMs_(0) {}

void SerialInputSwitch::configure(bool remoteEnabled, uint32_t usbWindowMs, uint32_t usbIdleMs) {
  remoteEnabled_ = remoteEnabled;
  usbWindowMs_ = usbWindowMs;
  usbIdleMs_ = usbIdleMs;
  if (!remoteEnabled_) mode_ = kUsb;
}

void SerialInputSwitch::begin(uint32_t nowMs) {
  mode_ = kUsb;
  usbUsed_ = false;
  bootMs_ = nowMs;
  lastInputMs_ = nowMs;
}

void SerialInputSwitch::onInput(uint32_t nowMs) {
  // In REMOTE mode the input comes from RS485 and says nothing about USB.
  if (mode_ != kUsb) return;
  usbUsed_ = true;
  lastInputMs_ = nowMs;
}

bool SerialInputSwitch::selectRemote() {
  if (!remoteEnabled_) return false;
  mode_ = kRemote;
  return true;
}

uint32_t SerialInputSwitch::msUntilRemote(uint32_t nowMs) const {
  if (mode_ != kUsb || !remoteEnabled_) return 0;
  const uint32_t start = usbUsed_ ? lastInputMs_ : bootMs_;
  const uint32_t length = usbUsed_ ? usbIdleMs_ : usbWindowMs_;
  const uint32_t elapsed = nowMs - start;
  return elapsed >= length ? 0 : length - elapsed;
}

void SerialInputSwitch::update(uint32_t nowMs) {
  if (mode_ == kUsb && remoteEnabled_ && msUntilRemote(nowMs) == 0) mode_ = kRemote;
}

}  // namespace mainboard
}  // namespace clara
