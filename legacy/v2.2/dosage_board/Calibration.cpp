#include "Calibration.h"
// assign pins globaly

Calibration::Calibration(byte pin) {
  this->pin;

  init();
}
void Calibration::init() {

  Serial.begin(9600);
}

void Calibration::run(Clara& clara) {

  // Change the previously callibrated values to the eeprom
  if (Serial.available()) {
    // Read the user input until the newline character
    Serial.readBytesUntil('\n', buffer, 32);

    // Parse the user input using the space as the delimiter
    char* command = strtok(buffer, " ");
    char* parameter = strtok(NULL, " ");
    char* parameterValue = strtok(NULL, " ");
    // Check if cal is entered to print the menu
    if (strcmp(command, "cal") == 0) {
      Serial.println("Instructions to set and get parameters for the Dosage board:");
      Serial.println("1: Type 'get' to get the values of the previously set pwm values ");
      Serial.println("\t  ");
      Serial.println("2: Type 'set [parameter] [value]' to calibarate the values");
      Serial.println("\t * To set the value of production time:");
      Serial.println("\t \t  Type 'set 0 #min'");

      Serial.println("\t * To set the value of settling time:");
      Serial.println("\t \t  Type 'set 1 #min'");

      Serial.println("\t * To set the value of liquid transfer time:");
      Serial.println("\t \t  Type 'set 2 #min'");

      Serial.println("\t * To set the value of flow ratio:");
      Serial.println("\t \t  Type 'set 3 #value'");

      Serial.println("\t * To set the value of Target FRC concentration:");
      Serial.println("\t \t  Type 'set 4 #value'");

      Serial.println("\t * To set the value of target NaClO concentration in g/l:");
      Serial.println("\t \t  Type 'set 5 #value'");

      Serial.println("\t * To set the value of kp:");
      Serial.println("\t \t  Type 'set 6 #value'");

      Serial.println("\t * To set the value of ki:");
      Serial.println("\t \t  Type 'set 7 #value'");

      Serial.println("\t * To set the value of feedback interval:");
      Serial.println("\t \t Type 'set 8 #s'");

      Serial.println("\t * To set the value of correction factor:");
      Serial.println("\t \t Type 'set 9 #value'");
      Serial.println("**************************************************************************");
    }


    // Check if the command is GET
    if (strcmp(command, "get") == 0) {

      for (int state = 0; state <= 8; state++) {
        paramvalue = clara.readParameterValueFromEEPROM(state);
        
        if (state == 0) {
          Serial.print("Flow ratio value = ");
          Serial.println(paramvalue);
        }
        if (state == 1) {
          Serial.print("Target FRC concentration = ");
          Serial.println(paramvalue);
        }
        if (state == 2) {
          Serial.print("Target NaClO concentration = ");
          Serial.println(paramvalue);
        }
        if (state == 3) {
          Serial.print("kp = ");
          Serial.println(paramvalue);
        }
        if (state == 4){
          Serial.print("ki =");
          Serial.println(paramvalue);
        }
        if (state == 5) {
          Serial.print("Feed back interval = ");
          Serial.println(paramvalue);
        }
        if (state == 6) {
          Serial.print("Correction factor = ");
          Serial.println(paramvalue);
        }
        if (state == 7) {
          Serial.print("Set to default value");
        }
      }
    }

    // Check if the command is SET

    if (strcmp(command, "set") == 0) {
      // Convert the state and value to integers
      int parameterNum = atoi(parameter);
      float valueInput = atof(parameterValue);

      // Check if the state is valid
      if (parameterNum >= 0 && parameterNum < 7) {
        // Write the ParameterValue to the EEPROM
        clara.writeParameterToEEPROM(parameterNum, valueInput);

        // Print the confirmation message to the serial monitor
        Serial.print("value for parameter");
        Serial.print(parameterNum);
        Serial.print("=");
        Serial.println(clara.readParameterValueFromEEPROM(parameterNum));
      }
      else if(parameterNum==7){
        clara.writeParameterToEEPROM(0, 1);
        clara.writeParameterToEEPROM(1, 1.5);
        clara.writeParameterToEEPROM(2,4.5 );
        clara.writeParameterToEEPROM(3,1.5 );
        clara.writeParameterToEEPROM(4, 1.5);
        clara.writeParameterToEEPROM(5, 3);
        clara.writeParameterToEEPROM(6, 1.5);

      }
       else {
        // Print the error message to the serial monitor
        Serial.println("Invalid state number. Please enter a value between 0 and 7.");
      }
    } else {
      // Print the error message to the serial monitor
      Serial.println("Invalid command. Please use 'SET [state number] [valueInput]' to set parameter's values.");
    }
  }
}
