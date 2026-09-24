#ifndef MY_VOLTAGESENSOR_H
#define MY_VOLTAGESENSOR_H

#include "Clara.h"
using namespace std;

class Voltagesensor {

private:
  int pin;
  const int voltageSensor = A3;

  float vOUT = 0.0;
  float voltage = 0.0;
  float R1 = 10;
  float R2 = 10;
  int value = 0;
  float high = 13.9;
  float low = 13.7;
  float batterypercentage;
  unsigned long previousMillisDisplay = 0;
  const long intervalVoltage_read = 1000;


public:
  Voltagesensor(byte pin);
  void init();
  void run(Clara &clara);  // Has to be passed as a reference! See p. 255 in "Grundkurs C++"
};
#endif
