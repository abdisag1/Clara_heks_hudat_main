#ifndef MY_CLARA_H
#define MY_CLARA_H
#include <EEPROM.h>  // Include EEPROM library

#include "Arduino.h"

using namespace std;

class Clara {


private:
  float flowrate = 0;
  float set_point_global = 0;
  byte state = 0;


public:
  Clara();
  void init();
  int state_standby = 2;
  int state_production = 3;
  int state_settling = 4;
  int state_transfering_fluid = 5;


  bool liquidlevel_1 = 0;
  bool liquidlevel_2 = 0;
  bool liquidlevel_3 = 0;
  float voltage;
  float get_flow();
  void set_flow(float flowrate);
  float get_setpoint();
  void set_setpoint(float setpoint);
  int get_state();
  void set_state(int state);
  // functions to read and write from the eeprom
  float readParameterValueFromEEPROM(int state);
  void writeParameterToEEPROM(int state, float parametervalue);

  void write_state_to_EEPROM(int state_value);
  int read_state_from_EEPROM();


  // functions used to contiously save the value of production time throught the production process
  void write_pr_min_to_EEPROM(int minutes);
  int read_pr_min_from_EEPROM();
  // functions used to contiously save the value of settling time throught the settling process
  void write_settling_min_to_EEPROM(int minutes);
  int read_settling_min_from_EEPROM();

  // functions to store the value of the previous calibration variable for the production
  void write_prev_cal_pr_min_to_EEPROM(int minutes);
  int read_prev_cal_pr_min_from_EEPROM();


  void write_valve_open_min_to_EEPROM(int minutes);
  int read_valve_open_min_from_EEPROM();

  void write_default_pr_min_to_EEPROM(int minutes);
  int read_default_pr_min_from_EEPROM();

  int get_liquidlevel_1();
  void set_liquidlevel_1(int liquidlevel_1);
  int get_liquidlevel_2();
  void set_liquidlevel_2(int liquidlevel_2);
  int get_liquidlevel_3();
  void set_liquidlevel_3(int liquidlevel_3);
  float get_voltage();
  void set_voltage(float voltage);
  float get_active_chlorine();
  void set_active_chlorine(float active_chlorine);
  float get_ph_value();
  void set_ph_value(float ph_value);

  float c_flowrate = 0;
  // variables to communicate between the two boards
  float flow_ratio = 1;
  // change the target_frc here to the value needed.
  float target_frc = 1.5;
  float target_naclo_con = 4.5;
  float ki = 2.5;
  float kp = 2;
  float feed_back_interval = 3000;
  float correction_factor = 1;
  float active_chlorine;
  float ph_value;

  //* 
 // Change both the default_pr_time_min and pr_time_min to change the production time
  
  // saves the defalut production min to be set.
   unsigned int default_pr_time_min = 180;
  // saves the production min that changes throughout the course of the production.
  unsigned int pr_time_min = 180;

  // saves the defalut settling min to be set.
  unsigned int default_settling_min = 5;
  // saves the settling min that changes through out the course of the settling.
  unsigned int settling_min = 5;

  // saves the defalut liquid transfer min to be set.
  unsigned int default_liquid_transfer_min = 10;
  // saves the liquid_transfer min that changes through out the course of liquid transfer.
  unsigned int liquid_tranfer_min = 10;

  // store the values of previous production states
  unsigned int prev_pr_time_min;
  unsigned int prev_settling_min;
  unsigned int prev_liquid_transfer_min;



};
#endif