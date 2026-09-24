/**
 * @file pins.h
 * @brief Wiring of the main board (Arduino Mega 2560). Unchanged from v2.2.
 */
#ifndef CLARA_MAINBOARD_PINS_H
#define CLARA_MAINBOARD_PINS_H

#include <Arduino.h>

namespace pins {

const uint8_t kLevelSensors[3] = {A0, A1, A2};  ///< L1 production bottle, L2 storage tank, L3 NaClO tank.
const uint8_t kVoltageSensor = A3;              ///< Supply voltage divider.
const uint8_t kElectrolysis = 10;               ///< Electrode power relay.
const uint8_t kPolarity = 4;                    ///< Electrode polarity reversal relay.
const uint8_t kFan = 7;                         ///< Fan relay.
const uint8_t kTransferValve = 11;              ///< Production bottle -> storage tank valve.
const uint8_t kChemicalSignal = 9;              ///< L3 forwarded to dosing board A3.
const uint8_t kRs485Direction = 2;              ///< RS485 transceiver DE/RE (HIGH = transmit).

/** Level sensors read HIGH when liquid is present (v2.2 behaviour). */
const bool kLevelActiveHigh = true;

const uint8_t kLcdAddress = 0x27;

}  // namespace pins

#endif  // CLARA_MAINBOARD_PINS_H
