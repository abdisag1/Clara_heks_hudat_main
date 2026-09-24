#include "board_io.h"

#include <Wire.h>
#include <string.h>

#include "clara/dosing_link.h"
#include "pins.h"

// --- BoardIo -------------------------------------------------------------------

void BoardIo::begin() {
  for (uint8_t i = 0; i < 3; ++i) pinMode(pins::kLevelSensors[i], INPUT);
  pinMode(pins::kVoltageSensor, INPUT);

  const uint8_t outputs[] = {pins::kElectrolysis, pins::kPolarity, pins::kFan, pins::kTransferValve,
                             pins::kChemicalSignal};
  for (uint8_t i = 0; i < sizeof(outputs); ++i) {
    digitalWrite(outputs[i], LOW);  // set the level before enabling the driver: no glitch
    pinMode(outputs[i], OUTPUT);
  }
}

bool BoardIo::readLevelSensor(uint8_t index) {
  if (index >= 3) return false;
  return (digitalRead(pins::kLevelSensors[index]) == HIGH) == pins::kLevelActiveHigh;
}

uint16_t BoardIo::readVoltageAdc() { return static_cast<uint16_t>(analogRead(pins::kVoltageSensor)); }

void BoardIo::applyOutputs(const clara::mainboard::CycleOutputs& outputs) {
  // Order matters: electrodes off before the polarity relay moves, polarity
  // settled before the electrodes are powered.
  if (!outputs.electrolysis) digitalWrite(pins::kElectrolysis, LOW);
  digitalWrite(pins::kPolarity, outputs.polarityReversed ? HIGH : LOW);
  if (outputs.electrolysis) digitalWrite(pins::kElectrolysis, HIGH);
  digitalWrite(pins::kFan, outputs.fan ? HIGH : LOW);
  digitalWrite(pins::kTransferValve, outputs.transferValve ? HIGH : LOW);
}

void BoardIo::setChemicalAvailableSignal(bool available) {
  digitalWrite(pins::kChemicalSignal, available ? HIGH : LOW);
}

// --- I2cDosingLink -------------------------------------------------------------

uint8_t I2cDosingLink::requestFrame(uint8_t* buffer, uint8_t capacity) {
  const uint8_t received =
      Wire.requestFrom(clara::link::kDosingBoardAddress, clara::link::kTelemetryFrameSize);
  uint8_t count = 0;
  while (Wire.available() > 0) {
    const int value = Wire.read();
    if (count < capacity) buffer[count++] = static_cast<uint8_t>(value);
  }
  return received == 0 ? 0 : count;
}

// --- LcdDisplay ----------------------------------------------------------------

void LcdDisplay::begin() {
  lcd_.init();
  lcd_.backlight();
  lcd_.clear();
  invalidate();
}

void LcdDisplay::invalidate() {
  for (uint8_t row = 0; row < clara::mainboard::kLcdRows; ++row) shown_[row][0] = '\0';
}

void LcdDisplay::writeLine(uint8_t row, const char* text) {
  if (row >= clara::mainboard::kLcdRows || strcmp(shown_[row], text) == 0) return;
  lcd_.setCursor(0, row);
  lcd_.print(text);  // fixed width: overwrites the previous text completely
  strncpy(shown_[row], text, clara::mainboard::kLcdColumns);
  shown_[row][clara::mainboard::kLcdColumns] = '\0';
}
