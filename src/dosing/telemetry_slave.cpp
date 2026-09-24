#include "telemetry_slave.h"

#include <Wire.h>
#include <string.h>
#include <util/atomic.h>

#include "clara/dosing_link.h"

namespace {

// The frame is written by loop() and read by the TWI interrupt; publish()
// copies it with interrupts disabled so a request never sees half a frame.
uint8_t gFrame[clara::link::kTelemetryFrameSize];

void onRequest() { Wire.write(gFrame, sizeof(gFrame)); }

void onReceive(int count) {
  // The master sends nothing in protocol v3; drain anything (e.g. from an old
  // v2.2 main board) so the bus is never left waiting.
  while (count-- > 0 && Wire.available() > 0) Wire.read();
}

}  // namespace

void I2cTelemetrySlave::begin() {
  Wire.begin(clara::link::kDosingBoardAddress);
  Wire.onRequest(onRequest);
  Wire.onReceive(onReceive);
}

void I2cTelemetrySlave::publish(const uint8_t* frame, uint8_t length) {
  if (length != sizeof(gFrame)) return;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { memcpy(gFrame, frame, sizeof(gFrame)); }
}
