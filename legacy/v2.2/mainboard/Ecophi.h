#ifndef MY_ECOPHI_H
#define MY_ECOPHI_H

#include "Clara.h"

using namespace std;

class Ecophi {

  private:

       byte pin0;
       byte pin1;
       byte pin2;
       float minute_flow;
       float minute_flow_commulative;
       float total_flow_commulative;
       float average_flow;
       float minute_naclo_pumped;
       float minute_naclo_commulative;
       float total_naclo_commulative;
       float average_naclo_pumped;
       float minute_active_chlorine;
       float minute_active_chlorine_commulative;
       float total_active_chlorine_commulative;
       float average_active_chlorine;
       float minute_ph_value;
       float minute_ph_value_commulative;
       float total_ph_value_commulative;
       float average_ph_value;


  public:
    Ecophi(byte pin0,byte pin1,byte pin2);
    void init();
    void run(Clara &clara); // Has to be passed as a reference! See p. 255 in "Grundkurs C++"
  
};
#endif
