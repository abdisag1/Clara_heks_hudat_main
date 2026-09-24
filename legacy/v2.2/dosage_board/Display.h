#ifndef MY_DISPLAY_H
#define MY_DISPLAY_H
#include <Arduino.h>
#include "Clara.h"
#include <LiquidCrystal_I2C.h> // Display driver





using namespace std;

class Display {

  private:
    //LiquidCrystal_I2C lcd;
    byte pin;
    
    String text;
    bool render_necessary(Clara &clara);
    void LCDWrite(String text1, String text2, String text3, String text4, LiquidCrystal_I2C &lcd);
    const long intervalDisplay = 500;
    unsigned long previousMillisDisplay = 0; 
    
  public:
    Display(byte pin);
    void init();
    void run(Clara &clara,LiquidCrystal_I2C &lcd); // Has to be passed as a reference! See p. 255 in "Grundkurs C++"

};
#endif
