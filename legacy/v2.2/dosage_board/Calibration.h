#ifndef MY_CALIBRATION_H
#define MY_CALIBRATION_H

#include "Clara.h"
using namespace std;

class Calibration {

private:
  int pin;
  char buffer[32];
  float paramvalue;
  

public:
  Calibration(byte pin);
  void init();
  void run(Clara &clara);  // Has to be passed as a reference! See p. 255 in "Grundkurs C++"
};
#endif
