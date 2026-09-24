#include "Dosage.h"
#include <TimerOne.h>
int mytimer = 0;
int mytime_now = 0;
int mytimer_s = 0;


// constructor of the dosage class

Dosage::Dosage(byte pin_pwm, byte pin_direction, byte pin_enable, byte pinA3) {
  this->pin_pwm = pin_pwm;
  this->pin_direction = pin_direction;
  this->pin_enable = pin_enable;
  this->pinA3 = pinA3;
  init();
}

// initialize the dosage class
void Dosage::init() {
  // Sets the two pins as Outputs
  pinMode(step_pin, OUTPUT);
  pinMode(dir_pin, OUTPUT);
  pinMode(enable, OUTPUT);
  digitalWrite(dir_pin, LOW);
  digitalWrite(enable, HIGH);
  TCCR1A = 0;  // set entire TCCR1A register to 0
  TCCR1B = 0;  // same for TCCR1B
  TCNT1 = 0;   //initialize counter value to 0
  // set compare match register for 1hz increments
  OCR1A = 15624;  // = (16*10^6) / (1*1024) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  Serial.begin(9600);
}
ISR(TIMER1_COMPA_vect) {  //This is the interrupt request
  mytimer++;
}
//takes flow rate as an argument and returns the volume of water that passed right before the dosage
float Dosage::volume_of_water_to_be_disinfected(float flow_rate) {
  q = dosage_cofficent*(flow_rate * interval_of_dosage_ms) / (60000);
  return q;
}

// takes volume of water to be disinfected and r_dose
float Dosage::dosage(float q, float r_dose) {
  dosage_rate = q * r_dose;
  // Serial.println("************************");
  // Serial.print("q: ");
  // Serial.println(q);
  // Serial.print("r_dose: ");
  // Serial.println(r_dose);
  // Serial.print("dosage_rate");
  // Serial.println(dosage_rate);

  return dosage_rate;  // dosage_rate [ml]
}

// takes step_per_rev and dosage as a parameter and returns steps
unsigned long Dosage::get_steps(int steps_per_rev, float dosage_rate) {
  steps = (steps_per_rev * dosage_rate) / (volume_per_rev);
  // Serial.println("*********************");
  // Serial.print("steps inside get step function");
  // Serial.print("steps per rev: ");
  // Serial.println(steps_per_rev);
  // Serial.print("dosage rate: ");
  // Serial.println(dosage_rate);
  // Serial.print("Volume per rev: ");
  // Serial.println(volume_per_rev);
  // Serial.print("steps: ");
  // Serial.println(steps);
  // Serial.println("*********************");



  
  // Serial.println(steps);
  return steps;
}

// functions that are used to generate freqency that is used to control the speed of the rotation
int Dosage::freq_gen(float dosage_rate) {
  frequency = (0.4608 * dosage_rate - 0.197) * 1000;
  return frequency;
}
// function that runs forever.
void Dosage::run(Clara &clara) {  // Has to be passed as a reference! See p. 255 in "Grundkurs C++"

  // digitalWrite(dir_pin,HIGH); // Enables the motor to move in a particular direction
  // digitalWrite(enable,LOW);
  r_dose = clara.target_frc/clara.target_naclo_con;
  dosage_cofficent = clara.dosage_cofficent;

  if (mytimer >= 1000) {
    mytimer_s++;
    mytimer = 0;
    // reads the value of level sensor3 to activate or not to activate the pump
    clara.set_liquidlevel_3(digitalRead(pinA3));
    Serial.print("level 3:");
    Serial.println(clara.get_liquidlevel_3());

    commulative_flow_l += clara.get_flow();
    flow_counter++;
    // Serial.print("commulative_flow_l: ");
    // Serial.println(commulative_flow_l);


    // unsigned long targetTime = micros() + period * current_steps * 2;
    // while (micros() < targetTime) {
    //   digitalWrite(step_pin, HIGH);
    //   unsigned long startTime = micros();
    //   while (micros() - startTime < period) {}
    //   digitalWrite(step_pin, LOW);
    //   startTime = micros();
    //   while (micros() - startTime < period) {}
    // }
  }
  if (mytimer_s >= (interval_of_dosage_ms / 1000)) {
    mytimer_s = 0;
    mytime_now++;
    digitalWrite(dir_pin, LOW);  // Enables the motor to move in a particular direction

    average_flow_rate = (commulative_flow_l) / flow_counter;
    // Serial.print("Average Flow rate:");
    // Serial.println(average_flow_rate);
    // increase dosage at lower flow rate
   
    if (average_flow_rate>= 0 && average_flow_rate<=60 ){
      dosage_cofficent =1.45;
      // Serial.println("lower range dosage rate");
    }
    else if (average_flow_rate>=60 && average_flow_rate<160){
      dosage_cofficent = 1.35;
      // Serial.println("mid range dosage rate");  
    }
    else if(average_flow_rate>160){
      dosage_cofficent =1.35;
      // Serial.println("high range dosage rate");

    }
    else {
      dosage_cofficent =1;
    }
    if (average_flow_rate > 0) {
      digitalWrite(enable, LOW);
      
    } else {
      digitalWrite(enable, HIGH);
      clara.period =10000;
    }

    float current_q = volume_of_water_to_be_disinfected(average_flow_rate);
    // Serial.print("volume of water to be disinfected");
    // Serial.println(current_q);
    // Check if level sensor3 is on
    if (clara.get_liquidlevel_3() == 1) {
      current_dosage = dosage(current_q, r_dose);  //current_q [L] r_dose[ml/L]

    } else {
      current_dosage = 0;
    }

    // Serial.print("dosage in time interval");
    // Serial.println(current_dosage);
    clara.set_setpoint(current_dosage);

    current_steps = get_steps(steps_per_rev, current_dosage);
    Serial.print("no of steps");
    Serial.println(current_steps);
    Serial.print("interval of dosage");
    Serial.println(interval_of_dosage_ms);
    long interval_of_dosage_micro = interval_of_dosage_ms;
    // float p = interval_of_dosage_micro/(2*(current_steps/1000));
    // Serial.print("p");
    // Serial.println(p);
    // if (current_steps=0){
    //   periodlocal =10000; // a very large number to handle the exception
    // }
    //else {
    // if (current_steps == 0){
    //   current_steps =1;
    //   periodlocal=10000;
    //   Serial.print("current steps: ");
    //   Serial.println(current_steps);
    // }
    // Serial.print("interval_of_dosage_micro: ");
    // Serial.println(interval_of_dosage_micro);
    if (current_steps > 0) {
      // Serial.print("current_step");
      // Serial.println(current_steps);
      float denominator = (2*current_steps);
      // Serial.print("denominator: ");
      // Serial.println(denominator);
      periodlocal = interval_of_dosage_micro*1000/denominator;
      // Serial.print("local period = ");
      // Serial.println(periodlocal);
      digitalWrite(enable, LOW);
    }
    else {
      
      digitalWrite(enable, HIGH);
    }
    //}
    clara.period = periodlocal;
    // Serial.print("period ");
    // Serial.println(clara.period);

    // unsigned long startTime;
    // for (unsigned long x = 0; x < current_steps; x++) {
    //   digitalWrite(step_pin, HIGH);
    //   startTime = micros();  // by changing this time delay between the steps we can change the rotation speed
    //   while (micros() - startTime < period)
    //     ;
    //   digitalWrite(step_pin, LOW);
    //   startTime = micros();
    //   while (micros() - startTime < period)
    //     ;
    // }
    // unsigned long targetTime = micros() + period * current_steps * 2;
    // while (micros() < targetTime) {
    //   digitalWrite(step_pin, HIGH);
    //   delayMicroseconds(period);
    //   digitalWrite(step_pin, LOW);
    //   delayMicroseconds(period);
    // }

    // for (unsigned long x = 0; x <current_steps; x++) {
    //   digitalWrite(step_pin, HIGH);
    //   delayMicroseconds(period);  // by changing this time delay between the steps we can change the rotation speed
    //   digitalWrite(step_pin, LOW);
    //   delayMicroseconds(period);
    // }

    unsigned long current_freq = freq_gen(current_dosage);
    //Serial.println(current_freq);

    //period = 1000000/current_freq;
    // period = 100;
    // Serial.println(period);
    commulative_flow_l = 0;
    average_flow_rate = 0;
    flow_counter = 0;
  }

  // Sub function to add commulative flow every second
  unsigned long current_flow_ms = millis();
  if (current_flow_ms - previous_millis_flow_ms >= interval_of_flow_reading_ms) {
    previous_millis_flow_ms = current_flow_ms;
    // commulative_flow_l += clara.get_flow();
    // flow_counter++;
    // Serial.print("commulative_flow_l: ");
    // Serial.println(commulative_flow_l);
  }

  unsigned long current_millis_dosage_ms = millis();

  // if (current_millis_dosage_ms - previous_millis_dosage_ms >= interval_of_dosage_ms) {
  //   previous_millis_dosage_ms = current_millis_dosage_ms;
  //   digitalWrite(dir_pin, HIGH);  // Enables the motor to move in a particular direction
  //   digitalWrite(enable, LOW);
  //   average_flow_rate = (commulative_flow_l) / flow_counter;
  //   Serial.print("Average Flow rate:");
  //   Serial.println(average_flow_rate);

  //   float current_q = volume_of_water_to_be_disinfected(average_flow_rate);
  //   Serial.print("volume of water to be disinfected");
  //   Serial.println(current_q);
  //   // Check if level sensor3 is on
  //   if (clara.get_liquidlevel_3() == 1) {
  //     current_dosage = dosage(current_q, r_dose); //current_q [L] r_dose[ml/L]
  //   } else {
  //     current_dosage = 0;
  //   }

  //   Serial.print("dosage in time interval");
  //   Serial.println(current_dosage);
  //   clara.set_setpoint(current_dosage);

  //   unsigned long current_steps = get_steps(steps_per_rev, current_dosage);
  //   Serial.print("no of steps");
  //   Serial.println(current_steps);
  //   Serial.print("interval of dosage");
  //   Serial.println(interval_of_dosage_ms);
  //   long interval_of_dosage_micro = interval_of_dosage_ms;
  //   // float p = interval_of_dosage_micro/(2*(current_steps/1000));
  //   // Serial.print("p");
  //   // Serial.println(p);
  //   period = interval_of_dosage_micro/(2*(current_steps/1000));
  //   Serial.print("period ");
  //   Serial.println(period);

  //   unsigned long startTime;
  //   for (unsigned long x = 0; x < current_steps; x++) {
  //     digitalWrite(step_pin, HIGH);
  //     startTime = micros();  // by changing this time delay between the steps we can change the rotation speed
  //     while (micros() - startTime < period)
  //       ;
  //     digitalWrite(step_pin, LOW);
  //     startTime = micros();
  //     while (micros() - startTime < period)
  //       ;
  //   }

  //   // for (unsigned long x = 0; x <current_steps; x++) {
  //   //   digitalWrite(step_pin, HIGH);
  //   //   delayMicroseconds(period);  // by changing this time delay between the steps we can change the rotation speed
  //   //   digitalWrite(step_pin, LOW);
  //   //   delayMicroseconds(period);
  //   // }

  //   unsigned long current_freq = freq_gen(current_dosage);
  //   //Serial.println(current_freq);

  //   //period = 1000000/current_freq;
  //   // period = 100;
  //   // Serial.println(period);
  //   commulative_flow_l = 0;
  //   average_flow_rate = 0;
  //   flow_counter = 0;
  // }
}
