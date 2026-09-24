#include "Clara.h"
#include "Levelsensors.h"
#include "Wata.h"
#include "Display.h"
#include "I2c.h"
#include "Voltagesensor.h"
#include "Calibration.h"
#include "Wata.h"
#include "Ecophi.h"

#include <LiquidCrystal_I2C.h>       // Display driver
LiquidCrystal_I2C lcd(0x27, 20, 4);  // set the LCD address to 0x27 for a 16 chars and 2 line display


//declaration of pins
using namespace std;
static byte pinA0 = A0;  // assigned for level sensor 1
static byte pinA1 = A1;  // assigned for level sensor 2
static byte pinA2 = A2;  // assigned for level sensor 3
static byte pinA3 = A3;  // assigned for voltage sensor

static byte pin0 = 0;
static byte pin1 =1;
static byte pin2 =2;   
static byte pin3 = 3;    // assigned for flowmeter
static byte pin10 = 10;  // assigned for wata activation
static byte pin4 = 4;    // assigned for wata polarity reversal
static byte pin7 = 7;    // assigned for fan
static byte pin9 =9;   // assigned for reading value of level3
static byte pin11 = 11;  // assigned for valve activation
static byte pin13 = 13;  // assigned for display obj
static byte pin14 = 14;
static byte pin12 = 12; // controls the peristaltic pump's activation


Clara clara;
Levelsensors level_sensors_obj(pinA0, pinA1, pinA2,pin9);
Wata wata_obj(pin10, pin4, pin11,pin7);
Display display_obj(pin13);
I2c i2c_obj(pin14);
Voltagesensor voltage_sensor_obj(pinA3);
Calibration calibration_obj(pin13);
Ecophi ecophi_obj(pin0,pin1,pin2);



void setup() {
  //clara.write_pr_min_to_EEPROM(clara.default_pr_time_min);
  Serial.begin(9600);
  pinMode(A0,INPUT);
  pinMode(A1,INPUT);
  pinMode(A2,INPUT);


  pinMode(pin9,OUTPUT);
  pinMode(pin3,INPUT);
  digitalWrite(pin12,LOW);
  // clara.writeParameterToEEPROM(0, 1);
  // clara.writeParameterToEEPROM(1, 1);
  // clara.writeParameterToEEPROM(2, 20);

  // retreive parameters from EEPROM
  // clara.pr_time_min = clara.readParameterValueFromEEPROM(0);
  // clara.default_pr_time_min = clara.readParameterValueFromEEPROM(0);
  clara.settling_min = clara.readParameterValueFromEEPROM(1);
  clara.default_settling_min = clara.readParameterValueFromEEPROM(1);
  clara.liquid_tranfer_min = clara.readParameterValueFromEEPROM(2);
  clara.default_liquid_transfer_min = clara.readParameterValueFromEEPROM(2);
  // get variables before calibrartion happened.
  clara.prev_pr_time_min = clara.read_prev_cal_pr_min_from_EEPROM();
 


  // if (clara.read_state_from_EEPROM() == 3) {
  //   if (clara.pr_time_min == clara.prev_pr_time_min){
  //     clara.pr_time_min = clara.read_prev_cal_pr_min_from_EEPROM();
  //   }
  //   else{
  //     clara.pr_time_min = clara.readParameterValueFromEEPROM(0);
  //   }
  //   clara.write_pr_min_to_EEPROM(clara.readParameterValueFromEEPROM(0));
  //   //Serial.print("Saved production min on EEPROM: ");
  //   //Serial.println(clara.pr_time_min);
  // }
  // else if(clara.read_state_from_EEPROM() ==2){
  //   clara.pr_time_min = clara.readParameterValueFromEEPROM(0);
  // }
  // else if(clara.read_state_from_EEPROM() ==4){
  //   clara.pr_time_min = clara.readParameterValueFromEEPROM(0);
  // }
  // else if(clara.read_state_from_EEPROM() ==5){
  //   clara.pr_time_min = clara.readParameterValueFromEEPROM(0);
  // }
  lcd.init();
  lcd.begin(20, 4, LCD_5x8DOTS);
  lcd.backlight();
  Serial.begin(9600);
  //clara.pr_time_min = clara.read_min_from_EEPROM();


  // put your setup code here, to run once:
}


void loop() {

  level_sensors_obj.run(clara);
  wata_obj.run(clara);
  display_obj.run(clara, lcd);
  i2c_obj.run(clara);
  voltage_sensor_obj.run(clara);
 calibration_obj.run(clara);
  ecophi_obj.run(clara);

  // put your main code here, to run repeatedly:
}
