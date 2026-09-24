Clara V2 software documentation
1. Introduction
Overview of the Clara V2 device and its functionality
Clara-V2 system is capable producing disinfectant up to  of disinfecting water up m3  The system continuously doses the right amount of NaClO into the water by reading the flow-rate as an input and  by applying a PI controller.  The system also sends real-time data regarding water flow-rate, availability of NaClO and  NaClO production status.
Purpose of the firmware
This document serves as a guide for understanding the code used for the Clara V2 system  , the development of the code was focused on the usage of widely used syntax, a simple extensible, conciseness, and simplicity in general.
Concerning this guide, the focus does not lay on a thorough description of the code to entirely replace the lecture of the code while working on it. The hope however is that it can reduce the study of the relevant parts of the code to a minimum to facilitate especially the extension of the system.
Target hardware platform and constraints
This firmware have been tested with Atmega328 , Atmega2560 and have been partially tested with Arduino boards such as Arduino pro mini, Arduino-nano.  The firmware can be used with esp micro-controller with some modifications.
Document conventions and terminology
PWM  …………. pulse width modulation`
I2C ………………Inter inter communication
EEPROM …….. Electronically Erasable re-programmable read only memory
PI controller ….. 
2. System Architecture
High-level overview of the firmware architecture
Major components and their interactions
Diagrams illustrating system structure (e.g., block diagrams, UML diagrams)
3. Software Design
Detailed description of the software design
Object-oriented design principles and patterns used
Class diagrams and sequence diagrams to illustrate interactions
Data flow diagrams to visualize data processing

4. Module Descriptions
In-depth explanation of each software module
The CLARA-system makes two demands which are usually hard to meet with classical procedural programming. The first of these demands is the demand for integrating many different parts that integrate with each other, for example level-sensors, pumps, the flow-meter, or the Echophi monitoring. The second is the demand for multitasking, so that for example the Echophi device can still send a message while the pump is pumping. Both these demands can be met in an easier way with object-oriented programming (OOP).
The software have two categories that will be installed in two processors. Namely main-processor and dosage processor. Main-processor will handle the production of NaClO, sending data to remote monitoring device, reading values from the level-sensors, reading values from voltage sensor, storing values to EEPROM, retrieving values to the EEPROM and receiving values from the dosage micro-controller. The dosage-processor will read the flow-rate, receives calibration values using I2c buss from the main board and handles the dosage process using a proportional controller. 
Clarav2.2_mainboard: will be installed on the main-processor. which will handle the production of NaClO, sending data to remote monitoring device, reading values from the level-sensors, reading values from voltage sensor, storing values to EEPROM, retrieving values to the EEPROM and receiving values from the dosage micro-controller.
Clarav2.2_dosage_board: will be installed on the dosage processor, which will read the flow-rate, receives calibration values using I2c buss from the main board and handles the dosage process using a proportional controller
Architecture
To account for the multitasking aspect it is important to understand that this implies that the delay()- function cannot be used when implementing multitasking. The reason for this is simply that it blocks all other code from being executed during the time of its activity.
As an alternative, for example for the wata production process, the time when the electrode start the electrolysis process is saved as a variable. Then in 
This object has a method named run(). in the loop()- function of the main program this run()-method is then called to check whether or not start the electrolysis process. In this case it is necessary to know how the value from level sensor 2. As this information is an attribute of the level-sensor_object, it is necessary to have an object which provides methods to get attributes from all the other objects to interlink the object which provides methods to get attributes from all the other objects to interlink the objects. This object is the clara-object which is always passed as a reference in the run-method of all the other objects.
Finally, this leads to the following code interlinking the objects for the level-sensor and the wata, as well as the clara-object. The constructor of the level-senosr and the wata further take the pins for these electronical components as arguments. Note that this is only an excerpt of the main code and that additional objects are used.
Clara clara;
Levelsensor level_sensors_obj(pinA0,pinA1,pinA2);
Wata wata_obj(pin10,pin4,pin11);
void loop() {
level_sensors_obj.run(clara);
wata_obj.run(clara);
}
Module responsibilities and interfaces for Clarav2.2_mainboard 
Clarav2.2_mainboard.ino
Clara.cpp and Clara.h
Display.cpp and Display.h
Echophi.h and Echophi.cpp
Flowmeter.cpp and Flowmeter.h
Levelsensor.h and Levelsensor.cpp
Pump.cpp and Pump.h
Voltagesensor.cpp and Voltagesensor.h
wata.cpp and wata.h
Detailed descriptions of classes, functions, and data structures

Code examples and pseudo-code where necessary
UML diagrams to visualize module interactions
5. Algorithms and Data Structures
Description of key algorithms used in the firmware
Data structures used for storing and processing information
Pseudocode or code snippets to illustrate algorithms
6. Hardware Interfaces
Description of how the firmware interacts with hardware components
Communication protocols and interfaces used
Driver and peripheral management
Interrupt handling and timing
7. User Interface (if applicable)
Description of the user interface (if any)
User interaction with the device
Menu structure and navigation
Display and input handling
8. Testing and Verification
Test plan and test cases
Verification and validation procedures
Test results and reports
9. Troubleshooting
Common issues and solutions
Error codes and their meanings
Diagnostic procedures
10. Appendices
Glossary of terms
Acronyms and abbreviations
Detailed specifications (e.g., hardware specifications, communication protocols)
Source code listings (optional)
