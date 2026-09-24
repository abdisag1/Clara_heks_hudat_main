/*
list of global functions the main code
*/
#include "Clara.h"
using namespace std;



//initialization of methods
Clara::Clara() {
  init();
}

void Clara::init() {
}
// method returns flowrate
float Clara::get_flow() {
  return flowrate;
}
// method sets flow rate
void Clara::set_flow(float flowrate) {
  this->flowrate = flowrate;
}
// method to get the set point
float Clara::get_setpoint() {
  return set_point_global;
}

// function to set the setpoint
void Clara::set_setpoint(float setpoint) {

  this->set_point_global = setpoint;
}
// Function to read parameters value from EEPROM
float Clara::readParameterValueFromEEPROM(int state) {
  // Retrieve 4 bytes from EEPROM and convert back to float
  int address = state * sizeof(float);
  byte floatBytes[4];
  for (int i = 0; i < 4; i++) {
    floatBytes[i] = EEPROM.read(address + i);
  }
  union {
    float f;
    byte b[4];
  } convert;
  for (int i = 0; i < 4; i++) {
    convert.b[i] = floatBytes[i];
  }
  float retrievedFloat = convert.f;

  // Serial.println("Retrieved float value:");
  // Serial.println(retrievedFloat);
  return retrievedFloat;
}

// Function to write a parameter value to EEPROM
void Clara::writeParameterToEEPROM(int state, float parametervalue) {

  int address = state * sizeof(float);
  // Convert float to 4 bytes and store in EEPROM
  byte floatBytes[4];
  union {
    float f;
    byte b[4];
  } convert;
  convert.f = parametervalue;
  for (int i = 0; i < 4; i++) {
    EEPROM.write(address + i, convert.b[i]);
  }
  // Serial.println("Stored float value:");
  // Serial.println(parametervalue);
}

// function to write the state value to EEPROM
void Clara::write_state_to_EEPROM(int state_value) {
  // I chose address 64 to save the state value at.
  int address = 64;
  EEPROM.write(address, state_value);
  // Serial.println("The state value is saved to EEPROM ");
}

// Function to read the state value from address 64
int Clara::read_state_from_EEPROM() {
  int address = 64;
  return EEPROM.read(address);
}
// function to write production time at address 68
void Clara::write_pr_min_to_EEPROM(int minutes) {
  int address = 68;
  return EEPROM.write(address, minutes);
}
// method read production time form eeprom
int Clara::read_pr_min_from_EEPROM() {
  int address = 68;
  return EEPROM.read(address);
}

// function: to write settling time for address 72
void Clara::write_settling_min_to_EEPROM(int minutes) {
  int address = 72;
  return EEPROM.write(address, minutes);
}
// function: to read setling time from EEPROM addres 72
int Clara::read_settling_min_from_EEPROM() {
  int address = 72;
  return EEPROM.read(address);
}
// function:s to store the value of the previous calibration variable for the production
void Clara::write_prev_cal_pr_min_to_EEPROM(int minutes) {
  int address = 76;
  return EEPROM.write(address, minutes);
}
// function: to read values of the previous calibration variabe for production
int Clara::read_prev_cal_pr_min_from_EEPROM() {
  int address = 76;
  return EEPROM.read(address);
}
// function: write the default production minutes to EEPROM addres 80
void Clara:: write_default_pr_min_to_EEPROM(int minutes){
  int address = 80;
  Serial.print("default production time is set to:");
  Serial.println(minutes);
  return EEPROM.write(address,minutes);
}
// function: to read the default production minutes from EEPROM address 80
int Clara:: read_default_pr_min_from_EEPROM(){
  int address = 80;
  Serial.print("default prodution time is set to default");
  return EEPROM.read(address);
}

// function: return bool value liquidlevel_1
int Clara::get_liquidlevel_1() {
  return liquidlevel_1;
}
// fucntion set the value of liquidlevel_1 
void Clara::set_liquidlevel_1(int liquidlevel_1) {
  this->liquidlevel_1 = liquidlevel_1;
}

// function: return bool value liquidlevel_2
int Clara::get_liquidlevel_2() {
  return liquidlevel_2;
}
// function: set the value of liquidlevel_2
void Clara::set_liquidlevel_2(int liquidlevel_2) {
  this->liquidlevel_2 = liquidlevel_2;
}
// function: return bool value of liquidlevel_3
int Clara::get_liquidlevel_3() {
  return liquidlevel_3;
}
// function: set the value of liquidlevel_3
void Clara::set_liquidlevel_3(int liquidlevel_3) {
  this->liquidlevel_3 = liquidlevel_3;
}

int Clara::get_state() {
  return state;
}
// function:: set value of state
void Clara::set_state(int state) {
  this->state = state;
}
// function:s return float value voltage
float Clara::get_voltage() {
  return voltage;
}
// function:: set the value of voltage
void Clara::set_voltage(float voltage) {
  this->voltage = voltage;
  //Serial.println(voltage);
}

// function:s return float value active chlorine value
float Clara::get_active_chlorine() {
  return active_chlorine;
}
// function:: set the value of active chorine value
void Clara::set_active_chlorine(float active_chlorine) {
  this->active_chlorine = active_chlorine;
}

// function:s return float ph value
float Clara::get_ph_value() {
  return active_chlorine;
}
// function:: set the value of active chorine value
void Clara::set_ph_value(float ph_value) {
  this->ph_value = ph_value;
}
