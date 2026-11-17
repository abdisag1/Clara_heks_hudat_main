/**
 * @file serial_input.h
 * @brief Decides who may send commands on the shared serial port: the USB
 *        console or the remote-monitoring unit on RS485.
 *
 * The Mega's USB chip and the RS485 receiver output (RO) both drive RX0
 * (pin 0). While the RS485 receiver is on, it overpowers the USB chip, so the
 * two inputs cannot be used at the same time. The receiver is switched with
 * RE (pin 5, LOW = on); DE (pin 2) enables the transmitter.
 *
 *   power-up / reset      USB mode: receiver off for usb_window_min minutes.
 *                         (Opening the Arduino Serial Monitor resets the
 *                         Mega, so a technician always starts here.)
 *   USB input in window   stays in USB mode until usb_idle_min minutes pass
 *                         without USB input, or "remote" is typed.
 *   otherwise             REMOTE mode: receiver on; commands come from RS485.
 *   remote_cal = 0        receiver never switched on (behaviour of v3.0).
 */
#ifndef CLARA_SERIAL_INPUT_H
#define CLARA_SERIAL_INPUT_H

#include <stdint.h>

namespace clara {
namespace mainboard {

class SerialInputSwitch {
 public:
  enum Mode {
    kUsb = 0,   ///< RS485 receiver off: the USB console has RX0.
    kRemote,    ///< RS485 receiver on: commands come from the remote unit.
  };

  SerialInputSwitch();

  /** @param usbWindowMs USB-only time after a reset, @param usbIdleMs USB timeout after the last input. */
  void configure(bool remoteEnabled, uint32_t usbWindowMs, uint32_t usbIdleMs);
  /** Starts in USB mode (call at power-up). */
  void begin(uint32_t nowMs);
  /** A character arrived on the serial port. */
  void onInput(uint32_t nowMs);
  /** Switches to REMOTE mode now ("remote" command). @return false if remote_cal = 0. */
  bool selectRemote();
  void update(uint32_t nowMs);

  Mode mode() const { return mode_; }
  bool remoteEnabled() const { return remoteEnabled_; }
  /** Milliseconds until REMOTE mode (0 if already remote or disabled). */
  uint32_t msUntilRemote(uint32_t nowMs) const;

 private:
  bool remoteEnabled_;
  uint32_t usbWindowMs_;
  uint32_t usbIdleMs_;
  Mode mode_;
  bool usbUsed_;
  uint32_t bootMs_;
  uint32_t lastInputMs_;
};

}  // namespace mainboard
}  // namespace clara

#endif  // CLARA_SERIAL_INPUT_H
