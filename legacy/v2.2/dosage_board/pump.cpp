#include "pump.h"
unsigned long startTime;

using namespace std;
Pump::Pump(byte pin_pwm) {
  this->pin_pwm;
  init();
}


void Pump::init() {
  pinMode(10, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);

  Serial.begin(9600);
}


void Pump::run(Clara &clara) {
  digitalWrite(8, HIGH);  // Enables the motor to move in a particular direction
  //digitalWrite(7, HIGH);
  // Makes 6400 pulses for making one full cycle rotation
   for (unsigned long x = 0; x < 1; x++) {
    digitalWrite(10, HIGH);
    startTime = micros();  // by changing this time delay between the steps we can change the rotation speed
    while (micros() - startTime < clara.period);
    digitalWrite(10, LOW);
    startTime = micros();
    while (micros() - startTime < clara.period);
   }



  // digitalWrite(10, HIGH);
  // delayMicroseconds(clara.period);
  // // unsigned long startTime = micros();
  // // while (micros() - startTime < 50) {}
  // digitalWrite(10, LOW);
  // delayMicroseconds(clara.period);
  // // startTime = micros();
  // // while (micros() - startTime < 50) {}
}