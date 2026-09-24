#ifndef MYI2C_H
#define MYI2C_H
#include "Clara.h"
#include "Eeprom.h"
#include <Wire.h>


using namespace std;

class I2c {
private:
  //Eeprom eeprom(13);
  byte pin_i2;
  static float dataToSend[4];
  static float receivedData[8];  // Array to store received data from the master
  static void sendData();
  static void receiveEvent(int howMany);
  float temp_cal_flow_ratio;
  float temp_cal_frc;
  float temp_cal_na_clo_concentration;
  float temp_cal_kp;
  float temp_cal_ki;
  float temp_cal_feed_back_interval;
  float temp_cal_correction_factor;
  unsigned long previousMillisDisplay = 0;
  const long interval_communication = 1000;


public:
  I2c(byte pin);

  void init();
  void run(Clara &clara);
};

#endif