#ifndef MY_DOSAGE_H
#define MY_DOSAGE_H

#include "Clara.h"

// defines pins
#define step_pin 10
#define dir_pin 8
#define enable 7

using namespace std;

class Dosage {

private:
 

  byte pin_pwm;
  byte pin_enable;
  byte pin_direction;
  byte pinA3;
  // variables to create a desired dosage interval using millis() function
  int interval_of_dosage_ms = 20000;
  unsigned long previous_millis_dosage_ms = 0;

  unsigned long interval_of_flow_reading_ms = 1000;
  unsigned long previous_millis_flow_ms = 0;


  // variables used to create a desired frequency using millis() function
  double periodlocal = 50;
  unsigned long lastStateChange = 0;


 

  float q = 0;
  float r_dose =0.3;  // it 0.3ml of Naclo is needed to disinfect 1 litter of water. 0.3ml/l
  float dosage_cofficent =1;
  int steps_per_rev = 6400;
  float volume_per_rev = 1.2; // 1.2 for the smaller kamore khm pump
  
  unsigned long steps;
  int frequency = 10000;
  float dosage_rate;
  double commulative_flow_l =0;
  float average_flow_rate = 0;
  int flow_counter =0;
  float current_dosage=0;
  unsigned long current_steps;



public:
  Dosage(byte pin_pwm, byte pin_direction, byte pin_enable, byte pinA3);
  float volume_of_water_to_be_disinfected(float flow_rate);
  float dosage(float q, float r_dose);
  unsigned long get_steps(int steps_per_rev, float dosage_rate);
  int freq_gen(float dosage_rate);
  void proportional_controller(int pulse_count, float set_point);
  void init();
  void run(Clara &clara);  // Has to be passed as a reference! See p. 255 in "Grundkurs C++"
};
#endif
