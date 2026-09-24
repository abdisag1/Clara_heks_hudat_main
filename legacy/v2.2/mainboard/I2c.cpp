#include "I2c.h"

using namespace std;
I2c::I2c(byte pin_i2c) {
  this->pin_i2;
  init();
}


void I2c::init() {
 Wire.begin();
  Serial.begin(9600);
}


void I2c::run(Clara &clara) {

  unsigned long currentMillis = millis();  // Get the current time
  if (currentMillis - previousMillisDisplay >= interval_communication) {
     previousMillisDisplay = currentMillis;

  Wire.requestFrom(0x21, sizeof(float) * 8);  // Request data from slave

  float receivedData[4];
  Wire.readBytes((uint8_t*)receivedData, sizeof(receivedData));

  // Process the received data (e.g., print it)
  for (int i = 0; i < 4; i++) {
    // Serial.print("Received data ");
    // Serial.print(i);
    // Serial.print(": ");
    // Serial.println(receivedData[i], 2);  // Print with 2 decimal places
    
    //ensure the values of the flow rate is unsigned number incase of error.
    if (receivedData[1]<0 ){
      receivedData[1] = 0;
    }
      //ensure the values of the Setpoint is unsigned number incase of error.
    if (receivedData[0]<0 ){
      receivedData[0] = 0;
    }
    clara.target_frc = receivedData[3];
    // Serial.print("Target FRC");
    clara.c_flowrate = receivedData[2];
    // Serial.print("C Flowrate: ");
    // Serial.println(clara.c_flowrate);
    clara.set_flow(receivedData[1]);
    // Serial.print("flow rate data received: ");
    // Serial.println(receivedData[1]);
    clara.set_setpoint(receivedData[0]);

  }
  // part of code that sends data to the slave
  // Create an array of 8 float variables
  float dataToSend[7] = { 1, 1.5, 4.5, 1, 1.5, 3, 1 };
  dataToSend[0]= clara.readParameterValueFromEEPROM(3);// flow_ratio
  dataToSend[1]= clara.readParameterValueFromEEPROM(4); // target frc
  dataToSend[2] = clara.readParameterValueFromEEPROM(5);// target NaClO concenteration
  dataToSend[3] = clara.readParameterValueFromEEPROM(6);  // Kp controller
  dataToSend[4] = clara.readParameterValueFromEEPROM(7);  // KI controller
  dataToSend[5] = clara.readParameterValueFromEEPROM(8);  // feed back interval
  dataToSend[6] = clara.readParameterValueFromEEPROM(9);  // correction factor
  

  // // Send the data to the slave
  Wire.beginTransmission(0X21); // Slave address
  Wire.write((uint8_t*)dataToSend, sizeof(dataToSend));
  Wire.endTransmission();
  }
  // send of the transmission to the slave


}
  