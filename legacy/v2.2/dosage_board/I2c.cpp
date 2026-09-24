#include "I2c.h"
// defination of static member variables inside isr2() function
float I2c::dataToSend[4] = { 1.23, 2.34, 1.0,1.5 };
float I2c::receivedData[8];
int i2c_timer_s = 0;
int i2c_timer = 0;

using namespace std;
I2c::I2c(byte pin_i2c) {
  this->pin_i2;
  init();
}


void I2c::init() {
  TCCR2A = 0;  // set entire TCCR2A register to 0
  TCCR2B = 0;  // same for TCCR2B
  TCNT2 = 0;   //initialize counter value to 0
  // set compare match register for 8khz increments
  OCR2A = 249;  // = (16*10^6) / (8000*8) - 1 (must be <256)
  // turn on CTC mode`
  TCCR2A |= (1 << WGM21);
  // Set CS21 bit for 8 prescaler
  TCCR2B |= (1 << CS21);
  // enable timer compare interrupt
  TIMSK2 |= (1 << OCIE2A);
  Wire.begin(0X21);  // Slave address
  Wire.onReceive(receiveEvent);
  Wire.onRequest(sendData);
  Serial.begin(9600);
}
ISR(TIMER2_COMPA_vect) {  //This is the interrupt request
  i2c_timer++;
}
void I2c::receiveEvent(int howMany) {
  if (howMany == sizeof(float) * 8) {
    Wire.readBytes((uint8_t *)receivedData, sizeof(receivedData));

    // Print received data
    for (int i = 0; i < 8; i++) {
      // Serial.print("Received data ");
      // Serial.print(i);
      // Serial.print(": ");
      // Serial.println(receivedData[i], 2);  // Print with 2 decimal places
      // delay(1000);
    }
  }
}

void I2c::sendData() {
  Wire.write((uint8_t *)dataToSend, sizeof(dataToSend));
}

void I2c::run(Clara &clara) {
  if (i2c_timer >= 250) {
    i2c_timer_s++;
    i2c_timer = 0;
    // float receivedData[8];
    // Wire.readBytes((uint8_t *)receivedData, sizeof(receivedData));
    // //Serial.println("one second");

    // // Process the received data (e.g., print it)
    // for (int i = 0; i < 8; i++) {
    //   // Serial.print("Received data from master ");
    //   // Serial.print(i);
    //   // Serial.print(": ");
    //   // Serial.println(receivedData[i], 2);  // Print with 2 decimal places

    //   // clara.flow_ratio = clara.readParameterValueFromEEPROM(0);
    //   // clara.target_frc = clara.readParameterValueFromEEPROM(1);
    //   // clara.target_naclo_con = clara.readParameterValueFromEEPROM(2);
    //   // clara.kp = clara.readParameterValueFromEEPROM(3);
    //   // clara.ki = clara.readParameterValueFromEEPROM(4);
    //   // clara.feed_back_interval = clara.readParameterValueFromEEPROM(5);
    //   // clara.correction_factor = clara.readParameterValueFromEEPROM(6);

    //   // if (receivedData[i] != clara.readParameterValueFromEEPROM(i)) {
    //   //   clara.writeParameterToEEPROM(i, receivedData[i]);
    //   //   Serial.println("parameter changed sucessfully");
    //   // }
    // }
    // part of code that sends data to the slave
    // Create an array of 8 float variables

    // put a delay that is needed here.
    float NaClO_pumped = clara.get_setpoint();
    float flow_rate = clara.get_flow();
    float c_flow_rate = clara.c_flowrate;
    float target_frc = clara.target_frc;

    //Serial.print("NaClopumped: ");
    dataToSend[0] = NaClO_pumped;
    // Serial.print("NaClO pumped");
    // Serial.println(dataToSend[0]);
    dataToSend[1] = flow_rate;
    // Serial.print("Flowrate to send: ");
    // Serial.println(clara.get_flow());
    // Serial.print("Commulative Flowrate to send: ");
    // Serial.println(clara.c_flowrate);
    //Serial.println(dataToSend[1]);
    dataToSend[2] = c_flow_rate;
    // Serial.print("Commulative Flowrate: ");
    // Serial.println(dataToSend[2]);
    dataToSend[3] = target_frc;

    // Send the data to the Master

    // send of the transmission to the slave
  }


  // Send and recive data from master every 1 second
  // unsigned long currentMillis = millis();  // Get the current time
  // if (currentMillis - previousMillisDisplay >= interval_communication) {
  //   previousMillisDisplay = currentMillis;

  //   float receivedData[8];
  //   Wire.readBytes((uint8_t *)receivedData, sizeof(receivedData));

  //   // Process the received data (e.g., print it)
  //   for (int i = 0; i < 8; i++) {
  //     // Serial.print("Received data from master ");
  //     // Serial.print(i);
  //     // Serial.print(": ");
  //     // Serial.println(receivedData[i], 2);  // Print with 2 decimal places

  //     // clara.flow_ratio = clara.readParameterValueFromEEPROM(0);
  //     // clara.target_frc = clara.readParameterValueFromEEPROM(1);
  //     // clara.target_naclo_con = clara.readParameterValueFromEEPROM(2);
  //     // clara.kp = clara.readParameterValueFromEEPROM(3);
  //     // clara.ki = clara.readParameterValueFromEEPROM(4);
  //     // clara.feed_back_interval = clara.readParameterValueFromEEPROM(5);
  //     // clara.correction_factor = clara.readParameterValueFromEEPROM(6);

  //     // if (receivedData[i] != clara.readParameterValueFromEEPROM(i)) {
  //     //   clara.writeParameterToEEPROM(i, receivedData[i]);
  //     //   Serial.println("parameter changed sucessfully");
  //     // }
  //   }
  //   // part of code that sends data to the slave
  //   // Create an array of 8 float variables

  //   // put a delay that is needed here.
  //   float NaClO_pumped = clara.get_setpoint();
  //   float flow_rate = clara.get_flow();
  //   float c_flow_rate = clara.c_flowrate;

  //   //Serial.print("NaClopumped: ");
  //   dataToSend[0] = NaClO_pumped;
  //   // Serial.print("NaClO pumped");
  //   // Serial.println(dataToSend[0]);
  //   dataToSend[1] = flow_rate;
  //   // Serial.print("Flowrate to send: ");
  //   // Serial.println(clara.get_flow());
  //   // Serial.print("Commulative Flowrate to send: ");
  //   // Serial.println(clara.c_flowrate);
  //   //Serial.println(dataToSend[1]);
  //   dataToSend[2] = c_flow_rate;
  //   // Serial.print("Commulative Flowrate: ");
  //   // Serial.println(dataToSend[2]);

  //   // Send the data to the Master

  //   // send of the transmission to the slave
  // }

  //delay(1000); // Delay for demonstration purposes
}