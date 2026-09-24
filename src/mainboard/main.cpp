/**
 * @file main.cpp
 * @brief Clara v3 main board (Arduino Mega 2560).
 *
 * Produces NaClO by electrolysis in batches, reads the level and voltage
 * sensors, shows the status on a 20x4 LCD, polls the dosing board over I2C and
 * reports to the Ecophi remote-monitoring unit over RS485 (Serial, 9600 baud).
 * The same serial port offers a calibration console (type "help").
 *
 * All logic lives in lib/clara_mainboard (unit tested on the PC); this file
 * only creates the hardware drivers and wires them together.
 */
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#if defined(CLARA_ENABLE_WATCHDOG)
#include <avr/wdt.h>
#endif

#include "board_io.h"
#include "clara/arduino/arduino_clock.h"
#include "clara/arduino/eeprom_device.h"
#include "clara/arduino/print_output.h"
#include "clara/mainboard/mainboard_app.h"
#include "clara/mainboard/mainboard_console.h"
#include "pins.h"

namespace {

// Global objects only store configuration in their constructors; hardware is
// initialised in setup(), after the Arduino core's init().
LiquidCrystal_I2C gLcd(pins::kLcdAddress, clara::mainboard::kLcdColumns, clara::mainboard::kLcdRows);
clara::ArduinoClock gClock;
BoardIo gIo;
I2cDosingLink gDosingLink;
LcdDisplay gDisplay(gLcd);
clara::ArduinoEeprom gEeprom;
clara::PrintOutput gSerialOutput(Serial);  // RS485 to Ecophi + USB console

clara::mainboard::MainboardApp gApp(gClock, gIo, gDosingLink, gDisplay, gSerialOutput, gEeprom);
clara::mainboard::MainboardConsole gConsole(gApp, gSerialOutput);

}  // namespace

void setup() {
#if defined(CLARA_ENABLE_WATCHDOG)
  wdt_disable();
#endif
  gIo.begin();  // outputs off first

  Serial.begin(9600);
  pinMode(pins::kRs485Direction, OUTPUT);
  digitalWrite(pins::kRs485Direction, HIGH);  // transmit only, as in v2.2

  Wire.begin();
#if defined(WIRE_HAS_TIMEOUT)
  // Never hang on a disturbed I2C bus (long cables, relay noise).
  Wire.setWireTimeout(25000, true);
#endif
  gDisplay.begin();

  gApp.begin();
  gConsole.printBanner();

#if defined(CLARA_ENABLE_WATCHDOG)
  // Only enable with a bootloader that handles watchdog resets (the stock
  // Mega 2560 bootloader of many clones loops forever after one).
  wdt_enable(WDTO_4S);
#endif
}

void loop() {
#if defined(CLARA_ENABLE_WATCHDOG)
  wdt_reset();
#endif
  while (Serial.available() > 0) gConsole.onChar(static_cast<char>(Serial.read()));
  gApp.update();
}
