#include "Wata.h"
// assign pins globaly
using namespace std;
int wa_timer = 0;
int wa_time_min = 0;
int wa_timer_s = 0;
int wa_timer_hr = 0;
int sedmentation_dely = 0;
int sed_timer_s = 0;
int sed_time_min = 0;
int settled = 0;
int val_timer = 0;
int val_time_min = 0;
int val_timer_s = 0;
int val_timer_hr = 0;
// initialize a variable that stores the wata count
unsigned int Wata_count = 0;
// Assign number of cycles when to reverse the polarity
unsigned int polarity_reversal_count = 1;
// initializes the variable that stores the state of the pins
bool reverse = true;
int pin4 = 4;
bool excuted_production_state_save = false;
bool excuted_settling_state_save = false;
bool excuted_transfering_fluid_state_save = false;
bool excuted_standby_state_save = false;


boolean flag;
Wata::Wata(byte pin10, byte pin4, byte pin11, byte pin7) {
  this->pin10 = pin10;
  this->pin4 = pin4;
  this->pin11 = pin11;
  this->pin7 = pin7;


  init();
}
void Wata::init() {
  TCCR0A = (1 << WGM01);  //Set the CTC mode
  OCR0A = 0xF9;           //Value for ORC0A for 1ms

  TIMSK0 |= (1 << OCIE0A);  //Set the interrupt request
                            // sei(); //Enable interrupt

  TCCR0B |= (1 << CS01);  //Set the prescale 1/64 clock
  TCCR0B |= (1 << CS00);
  pinMode(10,OUTPUT);
  pinMode(11,OUTPUT);
  pinMode(4,OUTPUT);
  pinMode(7,OUTPUT);

  pinMode(this->pin10, OUTPUT);
  pinMode(this->pin4, OUTPUT);
  pinMode(this->pin11, OUTPUT);
  pinMode(this->pin7, OUTPUT);
  Serial.begin(9600);
}
ISR(TIMER0_COMPA_vect) {  //This is the interrupt request
  wa_timer++;
}
void reverse_polarity() {


  if (Wata_count % polarity_reversal_count == 0) {
    reverse = !reverse;
    // Serial.print("polarity reversed ");
    // Serial.println(reverse);
  }
  digitalWrite(pin4, reverse);
}



void Wata::run(Clara &clara) {

  
    // creates one second delay to control the wata
  if (wa_timer >= 1000) {
    if (wa_state == HIGH) {

      wa_timer_s++;
      wa_timer = 0;
      //Serial.println(wa_timer_s);
      
    }
// creates one second delay to control the valve
    if (v_state == HIGH) {

      val_timer_s++;
      wa_timer = 0;
      //Serial.println("Transfer seconds");
    }
    // creates one second delay to control the sedmentation delay
    if (sedmentation_dely == HIGH) {
      sed_timer_s++;
      wa_timer = 0;
       //Serial.println("SED seconds");
    }
  }
  /**
     here we create one minute delay from the seconds that we created above
     for controlling the wata production,valve and a delay to settle the chlorine
  */

  if (wa_timer_s >= 59) {
    wa_time_min++;
    wa_timer_s = 0;

    // Serial.println("one minute delay for wata");
    clara.pr_time_min--;
 //   Serial.println(clara.pr_time_min);
    // every 30 min save the production time so that it can continue from where it stopped.
    if (wa_time_min % save_data_rep == 0) {
      clara.writeParameterToEEPROM(0, clara.pr_time_min);
      //clara.write_pr_min_to_EEPROM(clara.pr_time_min);
     // Serial.print("production min saved");
      clara.write_state_to_EEPROM(3);
    }
    // condition to restore the default value on the EEPROM so that when the production is done to start from the preset production minute.
    if (clara.pr_time_min ==0){
      clara.pr_time_min = clara.default_pr_time_min;
      clara.writeParameterToEEPROM(0, clara.read_default_pr_min_from_EEPROM());
      // Serial.println("");
    }
  }
  if (val_timer_s >= 60) {

    val_time_min++;
    val_timer_s = 0;
    clara.liquid_tranfer_min--;
  }
  if (sed_timer_s >= 60) {
    sed_time_min++;
    sed_timer_s = 0;
    clara.settling_min--;
    //Serial.println("one min");
  }


  if ((wa_time_min >= clara.default_pr_time_min) && (wa_state == HIGH)) {  //creates a delay of three hours
    wa_time_min = 0;
    
    digitalWrite(this->pin10, LOW);
    digitalWrite(this->pin7,LOW);
    // checks the number of production cycles to reverse the polarity
    reverse_polarity();
    Wata_count++;

    wa_state = LOW;
    wata_ready = 1;
    ready_to_settle = 1;
    count_down_min = clara.pr_time_min;
    //    Serial.print("plolarity: ");
    //    Serial.println(reverse);
    //    Serial.print("Wata count: ");
    //    Serial.println(Wata_count);
     //Serial.println("wata production is complete");
    clara.set_state(2);
    clara.write_pr_min_to_EEPROM(clara.default_pr_time_min);  // resets the production time to the default after the production is completed.
    clara.pr_time_min = clara.default_pr_time_min;
  }

  /**
     checks the if liquidlevel ==1 and wata_ready to start wata production
     @wata_ready -> prevents the system from producing a wata twice i.e after the valve is opened for 10 minutes wata_ready =1
     @clara.liquidlevel -> checks if there is liquid on the upper tank
  */
  if ((clara.liquidlevel_1 == 1) && (wata_ready == 0)) {
    digitalWrite(this->pin10, HIGH);  //start the wata production
    digitalWrite(this->pin7,HIGH);


    wa_state = HIGH;
    // Serial.print("Production time: ");
   // Serial.println(clara.pr_time_min);

     //Serial.println("wata is in production");
     clara.set_state(3);
    
    while (excuted_production_state_save == false) {
      clara.write_state_to_EEPROM(3);
      //Serial.println("excuted production state on eeprom once");
      clara.set_state(3);
      clara.write_state_to_EEPROM(clara.get_state());
      excuted_production_state_save = true;
    }



    //display on the lcd the process is about to start
  }

  /*
     this if conditions creates a delay of 5 min  when ready_to_settle == 1 and sed_time_min >5
  */

  if ((sedmentation_dely == HIGH) && (sed_time_min >= clara.default_settling_min)) {

    sedmentation_dely = LOW;
    ready_to_settle = 0;
    settled = 1;


    sed_time_min = 0;
   // Serial.println("Settling Completed");
    clara.settling_min = clara.default_settling_min;
  }
  if (ready_to_settle == 1) {


    sedmentation_dely = HIGH;
    //clara.set_state(5);
   // Serial.println("wata is settling");

    clara.set_state(4);
    while (excuted_settling_state_save == false) {
      clara.write_state_to_EEPROM(4);
     // Serial.println("excuted saving settling state on eeprom once");
      clara.write_state_to_EEPROM(clara.get_state());
      excuted_settling_state_save = true;
    }
  }
  /**
     the if conditions below checks if clara.wata_ready ==1 (which will be on after 10 minutes of dely after checking if it settled)
      @v_state -> will be HIGH when clara.wata_ready ==1

  */


  if ((val_time_min >= clara.default_liquid_transfer_min) && (v_state == HIGH)) { 
    val_time_min = 0;

    digitalWrite(this->pin11, LOW);
    //clara.write_state_to_EEPROM(2);
    clara.set_state(2);
    // Serial.println("Valve is closed");
    clara.liquid_tranfer_min =clara.default_liquid_transfer_min;
    //clara.set_state(3);
    v_state = LOW;
    wata_ready = 0;
    settle_state = 0;
    settled = 0;
  }


  if (settled == 1 && (clara.liquidlevel_2 == 0)) {
    digitalWrite(this->pin11, HIGH);
    //clara.write_state_to_EEPROM(5);
    clara.set_state(5);
    //Serial.println("valve is opened for 10 min");
    v_state = HIGH;
    valvecome = millis();

    while (excuted_transfering_fluid_state_save == false) {
      clara.write_state_to_EEPROM(5);
     // Serial.println("excuted saving transfering fluid once");
      clara.write_state_to_EEPROM(clara.get_state());
      excuted_transfering_fluid_state_save = true;
    }
    //clara.set_state(6);
  }
}
