#include "Display.h"
#include <LiquidCrystal_I2C.h>

using namespace std;




Display::Display(byte pin) {
  this->pin = pin;
  this->text = "";

  init();
}



void Display::init() {  // put your setup code here, to run once:
  // put your setup code here, to run once:
  


  Serial.begin(9600);
}

void Display::run(Clara &clara, LiquidCrystal_I2C &lcd) {  // Has to be passed as a reference! See p. 255 in "Grundkurs C++"

  unsigned long currentMillis = millis();  // Get the current time

  // Check if it's time to call function1 (every second)
   if (currentMillis - previousMillisDisplay >= intervalDisplay) {

    previousMillisDisplay = currentMillis;
    String flow_string = String(clara.get_flow()) + " l/min";  //
    String NaClO_pumped = String(clara.get_setpoint());        //
    String C_flow = String(clara.c_flowrate);
    


    LCDWrite("Flow: " + flow_string, "NaClO.Pumped: " + NaClO_pumped + " L", "C.Flow:" +C_flow +" L", "State: ", lcd);
   }
}

void Display::LCDWrite(String text1, String text2, String text3, String text4, LiquidCrystal_I2C &lcd) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(text1);
  lcd.setCursor(0, 1);
  lcd.print(text2);
  lcd.setCursor(0, 2);
  lcd.print(text3);
  lcd.setCursor(0, 3);
  lcd.print(text4);
}
