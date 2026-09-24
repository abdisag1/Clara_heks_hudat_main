#ifndef MY_Wata_H
#define MY_Wata_H

#include "Clara.h"
using namespace std;

class Wata {

  private:
   int pin10;
   int pin4;
   int pin11; 
   int pin7;
   int v_state;
   int settle_state;
   int valvecome; 
   int wa_state;
   int watacome;
   int count_down_min = 0;
   bool wata_ready;
   bool ready_to_settle;
   int save_data_rep = 30;


  public:
    Wata(byte pin10, byte pin4, byte pin11, byte pin7);
    void init();
    void run(Clara &clara); // Has to be passed as a reference! See p. 255 in "Grundkurs C++"
    

};
#endif
