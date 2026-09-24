
#include "Clara.h"

#include "Ecophi.h"
#define EcoRX 0         //Serial Receive pin
#define EcoTX 1         //Serial Transmit pin
#define EcoTxControl 2  //RS485 Direction control
#define RS485Transmit HIGH
#define RS485Receive LOW
using namespace std;
int i = 0;
int eco_timer = 0;
int eco_time_now = 0;
int eco_timer_s = 0;
int eco_timer4 = 0;
float flowcommulative = 0;
float flowaverage = 0;
int flow_count = 0;

// set the ecophi send time here the default should be 15 min
int eco_time_send_min = 1;

Ecophi::Ecophi(byte pin0, byte pin1, byte pin2) {
  this->pin0;
  this->pin1;
  this->pin2;

  init();
}


void Ecophi::init() {
  TCCR2A = 0;  // set entire TCCR2A register to 0
  TCCR2B = 0;  // same for TCCR2B
  TCNT2 = 0;   //initialize counter value to 0
  // set compare match register for 8khz increments
  OCR2A = 249;  // = (16*10^6) / (8000*8) - 1 (must be <256)
  // turn on CTC mode
  TCCR2A |= (1 << WGM21);
  // Set CS21 bit for 8 prescaler
  TCCR2B |= (1 << CS21);
  // enable timer compare interrupt
  TIMSK2 |= (1 << OCIE2A);
  Serial.begin(9600);
  pinMode(EcoTxControl, OUTPUT);
  digitalWrite(EcoTxControl, RS485Transmit);  // Init Transceiver
}
ISR(TIMER2_COMPA_vect) {  //This is the interrupt request
  eco_timer++;
}


void Ecophi::run(Clara &clara) {
  // Has to be passed as a reference! See p. 255 in "Grundkurs C++"

  if (eco_timer >= 250) {
    eco_timer_s++;
    eco_timer = 0;
    float flownow = clara.get_flow();  //get the flow each second from clara.get_flow()
    minute_flow_commulative += flownow;
    // Serial.print("minute flow commulative:");
    // Serial.println(minute_flow_commulative);
    flow_count++;


    //Serial.println(eco_timer_s);
    // check if the default variabes have changed
    // Serial.print("production time: ");
    // Serial.println(clara.pr_time_min);
    // Serial.print("Default production time: ");
    // Serial.println(clara.default_pr_time_min);
  }

  if (eco_timer_s >= 30) {
    eco_time_now++;
    eco_timer_s = 0;
    minute_flow = minute_flow_commulative / flow_count;  //get the average of the last minute
    total_flow_commulative += minute_flow;

    minute_active_chlorine = clara.active_chlorine;
    total_active_chlorine_commulative += minute_active_chlorine;


    minute_ph_value = clara.ph_value;
    total_ph_value_commulative += minute_ph_value;

    // Serial.print("flow in the last one min:");
    // Serial.println(minute_flow);



    // reset the values in a minute
    minute_flow_commulative = 0;
    flowaverage = 0;
    flow_count = 0;
    //Serial.println("this is 1 min");
  }


  if (eco_time_now >= eco_time_send_min) {
    eco_time_now = 0;
    // calculate average of the flow rate
    
    average_flow = total_flow_commulative / eco_time_send_min;

    // calculate average of Naclo pumped
    average_naclo_pumped = average_flow*(clara.target_frc/clara.target_naclo_con);

    // calculate average of active chlorine
    
    average_active_chlorine = total_active_chlorine_commulative/ eco_time_send_min;

    // calculate average of PH value
   
    average_ph_value = total_ph_value_commulative/ eco_time_send_min;

    // Serial.print("Average flow in the last 2 minutes:");
    // Serial.println(average_flow);
    // Serial.print("Naclo pumped:");
    // Serial.println(average_naclo_pumped);


    float e_flow = average_flow;
    float e_votage = clara.get_voltage();
    int e_level_1 = clara.get_liquidlevel_1();
    int e_level_2 = clara.get_liquidlevel_2();
    int e_level_3 = clara.get_liquidlevel_3();
    float e_naclo_consumed = average_naclo_pumped;
    float e_target_frc = clara.target_frc;
    float e_active_chlorine = average_active_chlorine;
    float e_ph_value = average_ph_value;
    //float cl_pumped = clara.pump_count * clara.volume_per_pump;
    //Serial.println("this is 4 min");
    //Serial.println(flow);
    // if (Serial.available() > 0)
    // {
    Serial.print(";");
    Serial.print(e_flow);
    Serial.print(",");
    Serial.print(e_votage);
    Serial.print(",");
    Serial.print(e_level_1);
    Serial.print(",");
    Serial.print(e_level_2);
    Serial.print(",");
    Serial.print(e_level_3);
    Serial.print(",");
    Serial.print(e_naclo_consumed);
    Serial.print(",");
    Serial.print(e_target_frc);
    Serial.print(",");
    Serial.print(e_active_chlorine);
    Serial.print(",");
    Serial.print(e_ph_value);
    Serial.print(":");
    // Serial.println("");
    // }

    // reset the values of flow rate after sending
    total_flow_commulative = 0;
    average_flow = 0;
    minute_flow=0;
    // reset the values of naclopumped after sending
    average_naclo_pumped =0;
    // reset the value so of active chlorine after sending
    total_active_chlorine_commulative =0;
    minute_active_chlorine =0;
    average_active_chlorine =0;

    // reset the value of active chlorine after sending
    total_ph_value_commulative =0;
    minute_ph_value =0;
    average_ph_value =0;

  }
}
