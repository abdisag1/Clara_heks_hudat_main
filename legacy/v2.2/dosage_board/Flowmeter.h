#ifndef MY_FLOWMETER_H
#define MY_FLOWMETER_H
#include "Clara.h"
using namespace std;

class Flowmeter {
private:
  
  byte pin_flow;

  // variables inside readflow() functionz
  float Q = 0;  // variable to stores the multiplied flowrate with the flow ratio
  float Q_corrected =0;
  float callibration = 0.5; // 0.2 for 2 inch flow meter, 0.5 for 3 inch flowmeter
  float flow_freq = 0;
  float flowrate = 0;
  float average_pulse = 0;

  // variables inside the isr2() static function 
  static unsigned long currTime_flow;
  static volatile long commulative_pulse_flow;
  static unsigned long pulse_flow;
  static unsigned long prevTime_flow;
  static unsigned int pulse_count_flow;

  // delay function
  const long interval1 = 1000;
  unsigned long previousMillis1 = 0;  // Variable to store the last time function1 was called
  static void isr2();
public:
  Flowmeter(byte pin);
  float read_flow();
  void init();
  void run(Clara &clara);
};
#endif
