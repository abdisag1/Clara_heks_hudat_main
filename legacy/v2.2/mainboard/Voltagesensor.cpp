#include "Voltagesensor.h"
// assign pins globaly

Voltagesensor::Voltagesensor(byte pin) {
  this->pin;

  init();
}
void Voltagesensor::init() {
  pinMode(pin, INPUT);
  // Serial.begin(9600);
}

void Voltagesensor::run(Clara &clara) {
  unsigned long currentMillis = millis();  // Get the current time

  if (currentMillis - previousMillisDisplay >= intervalVoltage_read) {

    previousMillisDisplay = currentMillis;
    value = analogRead(voltageSensor);
    //Serial.println(value);
    vOUT = (value * 4.85) / 1024.0;
    voltage = vOUT*22.2 ;
    clara.set_voltage(voltage);
    // Serial.print("voltage: ");
     //Serial.println(clara.get_voltage());

    
  }
}
