#ifndef MY_PUMP_H
#define MY_PUMP_H

#include "Clara.h"


using namespace std;
#define step_pin 10


class Pump {

private:
byte pin_pwm;
 


public:
  Pump(byte pin_pwm);
  void init();
  void run(Clara &clara);  // Has to be passed as a reference! See p. 255 in "Grundkurs C++"
};
#endif
