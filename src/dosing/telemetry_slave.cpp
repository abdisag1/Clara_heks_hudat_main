#include "telemetry_slave.h"

#include <Wire.h>
#include <string.h>
#include <util/atomic.h>

#include "clara/dosing_link.h"

namespace {

// Frames are written by loop() and read by the TWI interrupt (and the other
// way round for commands); every copy is made with interrupts disabled, so
// neither side ever sees half a frame.
uint8_t gFrame[clara::link::kTelemetryFrameSize];
uint8_t gReply[clara::link::kReplyFrameSize];
volatile bool gReplyPending = false;
uint8_t gCommand[clara::link::kCommandFrameSize];
volatile uint8_t gCommandLength = 0;
volatile uint32_t gRequests = 0;

void onRequest() {
  // A command reply is served once, then the telemetry again.
  if (gReplyPending) {
    Wire.write(gReply, sizeof(gReply));
    gReplyPending = false;
  } else {
    Wire.write(gFrame, sizeof(gFrame));
  }
  ++gRequests;
}

void onReceive(int count) {
  // Keep a command frame for loop(); drain anything else (e.g. the frames an
  // old v2.2 main board sends) so the bus is never left waiting.
  uint8_t length = 0;
  uint8_t buffer[clara::link::kCommandFrameSize];
  while (count-- > 0 && Wire.available() > 0) {
    const uint8_t value = static_cast<uint8_t>(Wire.read());
    if (length < sizeof(buffer)) buffer[length] = value;
    ++length;
  }
  if (length == clara::link::kCommandFrameSize && buffer[0] == clara::link::kCommandMarker) {
    memcpy(gCommand, buffer, sizeof(gCommand));
    gCommandLength = length;
  }
}

}  // namespace

void I2cTelemetrySlave::begin() {
  Wire.begin(clara::link::kDosingBoardAddress);
  Wire.onRequest(onRequest);
  Wire.onReceive(onReceive);
}

uint32_t I2cTelemetrySlave::requestCount() const {
  uint32_t count;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { count = gRequests; }
  return count;
}

void I2cTelemetrySlave::publish(const uint8_t* frame, uint8_t length) {
  if (length != sizeof(gFrame)) return;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { memcpy(gFrame, frame, sizeof(gFrame)); }
}

uint8_t I2cTelemetrySlave::takeCommand(uint8_t* frame, uint8_t capacity) {
  uint8_t length = 0;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    if (gCommandLength > 0 && gCommandLength <= capacity) {
      memcpy(frame, gCommand, gCommandLength);
      length = gCommandLength;
    }
    gCommandLength = 0;
  }
  return length;
}

void I2cTelemetrySlave::publishReply(const uint8_t* frame, uint8_t length) {
  if (length != sizeof(gReply)) return;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    memcpy(gReply, frame, sizeof(gReply));
    gReplyPending = true;
  }
}
