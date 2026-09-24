/**/
#include "Clara.h"
#include "Flowmeter.h"
#include "Dosage.h"
//#include "Eeprom.h"
#include "I2c.h"
//#include "Calibration.h"
#include "Arduino.h"
#include "TimerOne.h"
//#include <Wire.h>
#include "Display.h"
#include "pump.h"
//#include <LiquidCrystal_I2C.h>       // Display driver
//LiquidCrystal_I2C lcd(0x27, 20, 4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

using namespace std;

//declaration of pin connections
static byte pin_pwm = 10;
static byte pin_flow = 3;
static byte pin_rpm = 2;
static byte pin_enable =7;
static byte pin_eeprom = 13;
static byte pin_i2c = 14;
static byte pin_direction =8;
static byte pin_display = 16;
static byte pin_pinch_valve_activation = 11;
static byte pinA3 = A3;
static byte pin6 =6; // pin to control pump relay
// create clara objects

Clara clara;
Flowmeter flowmeter_obj(pin_flow);
//Dosage dosage_obj(pin_pwm,pin_rpm);
Dosage dosage_obj(pin_pwm, pin_direction,pin_enable,pinA3);
//Eeprom eeprom_obj(pin_eeprom);
I2c i2c_obj(pin_i2c);
Pump pump_obj(pin_pwm);
//Calibration calibration_obj(pin_eeprom);

//Display display_obj(pin_display);

void setup() {
  // put your setup code here, to run once:
  // lcd.init();
  // lcd.begin(20, 4, LCD_5x8DOTS);
  // lcd.backlight();
  
  // clara.flow_ratio = clara.readParameterValueFromEEPROM(0);
  // clara.target_frc=clara.readParameterValueFromEEPROM(1);
  // clara.target_naclo_con = clara.readParameterValueFromEEPROM(2);
  // clara.kp= clara.readParameterValueFromEEPROM(3);
  // clara.ki= clara.readParameterValueFromEEPROM(4);
  // clara.feed_back_interval =clara.readParameterValueFromEEPROM(5);
  // clara.correction_factor = clara.readParameterValueFromEEPROM(6);
  

   //pinMode(pin_rpm, INPUT);
  pinMode(pin_pwm,OUTPUT);
  pinMode(pin_direction,OUTPUT);
  //pinMode(pin_pinch_valve_activation,OUTPUT);
  pinMode(pinA3,INPUT);
  pinMode(pin6,OUTPUT);
  digitalWrite(pin6,HIGH);
  
  digitalWrite(pin_direction,LOW);
  //Timer1.initialize(100);  // Set the timer period in microseconds for desired frequency
  Serial.begin(9600);
  //Timer1.pwm(pin_pwm, 128); 
  //Serial.print("dosage init");
  //delay(1000);
  
}

void loop() {
  // put your main code here, to run repeatedly:
  flowmeter_obj.run(clara);
  dosage_obj.run(clara);
  pump_obj.run(clara);
  //eeprom_obj.run(clara);
  i2c_obj.run(clara);
  //calibration_obj.run(clara);

 // display_obj.run(clara, lcd);
}
