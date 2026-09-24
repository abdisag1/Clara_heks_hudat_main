#ifndef MY_CLARA_H
#define MY_CLARA_H
#include <EEPROM.h>  // Include EEPROM library

#include "Arduino.h"


using namespace std;

class Clara {

private:

  float set_point_global = 0;
  float flowrate = 0;

public:
  Clara();
  void init();
  float get_flow();
  void set_flow(float flowrate);
  float get_setpoint();
  void set_setpoint(float setpoint);
  // functions to read and write from the eeprom
  float readParameterValueFromEEPROM(int state);
  void writeParameterToEEPROM(int state, float parametervalue);
  int get_liquidlevel_3();
  void set_liquidlevel_3(int liquidlevel_3);
  

  float c_flowrate = 0;
  // variables to communicate between the two boards

  // change this variables to adjust you dosage at the site.
  float target_frc = 1.5;
  float target_naclo_con =4.5;
  float dosage_cofficent=1;



  float flow_ratio;
  float ki;
  float kp;
  float feed_back_interval;
  float correction_factor;
  bool liquidlevel_3 = 0;
  float pwm_value;
  double period = 1000;

};
#endif