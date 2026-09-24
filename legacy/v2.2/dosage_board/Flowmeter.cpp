#include "Flowmeter.h"


using namespace std;

// defination of static member variables inside isr2() function
unsigned int Flowmeter::pulse_count_flow =0 ;
unsigned long Flowmeter::pulse_flow =0;
volatile long Flowmeter:: commulative_pulse_flow =0;
unsigned long Flowmeter:: currTime_flow = 0;
unsigned long Flowmeter:: prevTime_flow = 0;
long randNumber;

Flowmeter::Flowmeter(byte pin_flow) {
  this->pin_flow = pin_flow;
  init();
}

// Initialaization
void Flowmeter::init() {
  pinMode(pin_flow, INPUT);
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(pin_flow), isr2, RISING);
  sei();
}

// Interrupt function that will be called inside when triggered.
void Flowmeter::isr2() {
  // Get the current time
  currTime_flow = micros();
  // Calculate the pulse duration
  pulse_flow = currTime_flow - prevTime_flow;
  prevTime_flow = currTime_flow;
  pulse_count_flow++;
  // whenever there is an interupt the pulse will be commulated.
  commulative_pulse_flow += pulse_flow;
  TCNT1 = 0;  // Reset the timer1 counter
}

// function that reads the flowrate returns the flow rate
float Flowmeter::read_flow() {
  
  average_pulse = commulative_pulse_flow / pulse_count_flow;
  
  flow_freq = 1000000 / average_pulse;
  flowrate = flow_freq / callibration;
  if (flowrate<0) {
    flowrate =0;
  }

  if (flowrate>= 5){
  Q_corrected =1.02*flowrate +8.61;
  flowrate = Q_corrected;
  }
  
 
  Q = flowrate;
 

  // Serial.print("Flow Rate: ");
  // Serial.print(Q);
  // Serial.println(" L/min");
  pulse_count_flow = 0;
  commulative_pulse_flow = 0;
 return Q;
  
}

// main function that runs forever
void Flowmeter::run(Clara &clara) {
  unsigned long currentMillis = millis();  // Get the current time

  // Check if it's time to call function1 (every second)
  if (currentMillis - previousMillis1 >= interval1) {
  
    previousMillis1 = currentMillis;

    clara.set_flow(read_flow());
    Serial.print("flow rate: ");
    Serial.println(clara.get_flow());
    // gets the adds up the flowrate each second to get commulative flowrate;
    clara.c_flowrate += clara.get_flow()/60;
    // resets teh commulative flow rate to 0
    if (clara.c_flowrate >=10000) {
      clara.c_flowrate = 0;
    }
    if (clara.c_flowrate<=0){
      clara.c_flowrate =0;
    }
    
  }
}