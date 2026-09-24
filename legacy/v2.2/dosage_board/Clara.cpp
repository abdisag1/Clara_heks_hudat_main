/*
list of global functions the main code
*/
#include "Clara.h"
using namespace std;

//initialization of methods
Clara::Clara(){
  init();
}
void Clara:: init() {

}
float Clara:: get_flow() {
  // Serial.print("get global flowrate =");
  // Serial.println(flowrate);
  return flowrate;

}
void Clara:: set_flow(float flowrate) {
  this->flowrate=flowrate;
  // Serial.print("flow rate set to =");
  // Serial.println(flowrate);
}
// method to get the set point
float Clara:: get_setpoint() {
  return set_point_global;
  
}

// function to set the setpoint
void Clara:: set_setpoint(float setpoint) {
  
  this->set_point_global=setpoint;
  
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
// function: return bool value of liquidlevel_3
int Clara::get_liquidlevel_3() {
  return liquidlevel_3;
}
// function: set the value of liquidlevel_3
void Clara::set_liquidlevel_3(int liquidlevel_3) {
  this->liquidlevel_3 = liquidlevel_3;
}


