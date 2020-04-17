/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "Particle.h"
#line 1 "/Users/chipmc/Documents/Maker/Particle/Projects/Coulomb-Counter/src/Coulomb-Counter.ino"
/*
 * Project Coulomb-Counter for Battery Capacity and Sleep Current Measurements
 * Description: Count the coulombs used in the Particle device so I can size batteries and determine current requriements
 * Author: Chip McClelland adapted Mike's code (see attribution below and in the Readme)
 * Date: 9-22-19 updated on 4-16-20
 */
/* 

Original code Attribition: LTC4150 Coulomb Counter interrupt example code
Mike Grusin, SparkFun Electronics

Product page: https://www.sparkfun.com/products/12052
Software repository: https://github.com/sparkfun/LTC4150_Coulomb_Counter_BOB

Chip's Particle Modified Version - https://github.com/chipmc/Coulomb-Counter

*/


// Chip's Version tracking
// v1.0 - First pass at adding a second mode - Sleep testing


// I edited the pin assignments to match the Particle Photon / Argon
void setup();
void loop();
void publishResult();
void coulombISR();
void sleepWakeISR();
bool makeCalculations(bool resetValues);
int setCapacity(String command);
int setCharge(String command);
int resetTest(String command);
int setMode(String command);
bool meterParticlePublish(void);
#line 25 "/Users/chipmc/Documents/Maker/Particle/Projects/Coulomb-Counter/src/Coulomb-Counter.ino"
const int intPin = D2;                            // For the Particle Photon / Argon
const int polPin = D3;                            // Polarity signal
const int blueLED = D7;                           // Standard Arduino LED
const int sleepIndicator = D4;                    // Connects device we are testing indicates sleep / wake

// Change the following two lines to match your battery
// and its initial state-of-charge:

char capacityStr[16] = "NA";                      // String expressions to add units for Console viewing
char chargeStr[16] = "NA";                         
char currentStr[16] = "NA";      
char averageCurrentStr[16] = "NA";        
char versionStr[8] = "1.00";

// Global variables ("volatile" means the interrupt can
// change them behind the scenes):
volatile boolean coulombFlag = false;             // Interrupt flag each time a coulumb is counted
volatile boolean sleepFlag = false;               // Interrupt indicating a change in the sleep / wake cycle
const float ah_quanta = 0.17067759;               // mAh for each INT
float percent_quanta;                             // % battery for each INT
bool batteryTestMode = false;                     // Battery Test Mode
bool sleepState = false;                          // Is the device awake or not?
bool currentStateWriteNeeded = false;             // Did we make a change to a value in the current state structure?

struct currentState_Structure {                   // This structure will be saved at each coulomb
  unsigned long startTime;
  float startingCapacity_mAh;
  float currentCapacity_mAh;
  float startingCharge_percent;
  float currentCharge_percent;
  float currentCurrent;
  float averageCurrent;
};

currentState_Structure currentState;

void setup()
{
  // Set up I/O pins:
  pinMode(intPin,INPUT);                            // Interrupt input pin (must be D2 or D3)
  pinMode(polPin,INPUT);                            // Polarity input pin
  pinMode(sleepIndicator,INPUT);                    // Important - must be an input
  pinMode(blueLED,OUTPUT);                          // Standard Particle status LED
  digitalWrite(blueLED,LOW);  

  Particle.function("Set-Capacity", setCapacity);   // Set the capacity
  Particle.function("Set-Charge", setCharge);       // Set the charge level
  Particle.function("Reset-Test",resetTest);        // Set all the values back to start
  Particle.function("Battery-or-Sleep-Mode",setMode);

  Particle.variable("Capacity", capacityStr);
  Particle.variable("Charge", chargeStr);           // charge value
  Particle.variable("Release",versionStr);

  attachInterrupt(intPin,coulombISR,FALLING);
  attachInterrupt(sleepIndicator, sleepWakeISR,CHANGE);

  waitUntil(Particle.connected);                  // Get connected first - helps to ensure we have the right time

  EEPROM.get(0,currentState);

  if (Time.now() - currentState.startTime > 300) {    // Too much time went by, must be a new test
    currentState.startTime = Time.now();            // When did we start the test
    currentState.currentCapacity_mAh = currentState.startingCapacity_mAh;
    currentState.currentCharge_percent = currentState.startingCharge_percent;
    currentStateWriteNeeded = true;
  }

  percent_quanta = 1.0/(currentState.startingCapacity_mAh/1000.0*5859.0/100.0);   // % battery for each INT
  
  snprintf(capacityStr,sizeof(capacityStr),"%4.1f mAh",currentState.currentCapacity_mAh);
  snprintf(chargeStr,sizeof(chargeStr),"%3.1f %%",currentState.currentCharge_percent);

  Particle.publish("Startup","LTC4150 Coulomb Counter",PRIVATE);
  waitUntil(meterParticlePublish);
  Particle.publish("Status","Select either 'Battery' or 'Sleep' modes to start",PRIVATE);
}

void loop()
{
  if (coulombFlag) {
    coulombFlag = false;                                                    // Reset the flag to false so we only do this once per INT
    digitalWrite(blueLED,HIGH);                                             // Blink the LED
    if (makeCalculations(false) && (batteryTestMode || sleepState)) publishResult();  // Update the calculations and publish if needed
    digitalWrite(blueLED,LOW);
  }
  if (sleepFlag) {
    sleepFlag = false;
    if (digitalRead(sleepIndicator)) {
      sleepState = false;
      waitUntil(meterParticlePublish);
      Particle.publish("State","Device Awake - Reset and wait for device to sleep again",PRIVATE);
      makeCalculations(true);                                           // This resets all the values and gets us ready for the next sleep cycle
      setMode("Sleep");
    }
    else {
      sleepState = true;                                                 // In the next sleep cycle - start tracking
      waitUntil(meterParticlePublish);
      Particle.publish("Status","Device Sleeping - waiting for first coulomb",PRIVATE);
    }
  }
  if (currentStateWriteNeeded) {
    currentStateWriteNeeded = false;
    EEPROM.put(0,currentState);   
  }
}

void publishResult() {
  char data[96];
  if (batteryTestMode) {
    int elapsedSec = Time.now() - currentState.startTime;
    snprintf(data, sizeof(data), "Status: %4.0f mAh, %3.1f%% charge, %4.3f mA elapsed time:%i:%i:%i:%i", currentState.currentCapacity_mAh, currentState.currentCharge_percent, currentState.currentCurrent, Time.day(elapsedSec)-1, Time.hour(elapsedSec), Time.minute(elapsedSec), Time.second(elapsedSec));
  }
  else if(sleepState) {
    snprintf(data, sizeof(data), "Sleeping: current: %4.3f mA, average: %4.3f mA", currentState.currentCurrent, currentState.averageCurrent);
  }
  Particle.publish("Status",data,PRIVATE);
}

void coulombISR() {                                                                 // Run automatically for falling edge on D2
  coulombFlag = true;                                                               // Set isrflag so main loop knows an interrupt occurred
}

void sleepWakeISR() {                                                               // Runs when the device under test changes sleep / wake state
  sleepFlag = true;
}

bool makeCalculations(bool resetValues) {
  static unsigned long runTime = 0; 
  static unsigned long lasttime = 0;                                                // These are based on micros()
  static float currentBuffer[10];                                          // the readings from the pressure sensor for calibration
  static int index = 0;
  float runningTotal = 0;
  static int numberOfSamples = 0;
  static bool firstSampleDiscard = true;

  // This is where we reset all the values needed to restart a test
  if (resetValues) {
    currentState.startTime = Time.now();                                            // When did we start the test
    currentState.currentCapacity_mAh = currentState.startingCapacity_mAh;
    currentState.currentCharge_percent = currentState.startingCharge_percent;
    snprintf(capacityStr,sizeof(capacityStr),"%4.1f mAh",currentState.currentCapacity_mAh);
    snprintf(chargeStr,sizeof(chargeStr),"%3.1f %%",currentState.currentCharge_percent);
    strcpy(currentStr,"NA");
    strcpy(averageCurrentStr,"NA");
    lasttime =  index = runningTotal = numberOfSamples = 0;
    currentStateWriteNeeded = firstSampleDiscard = true;
    return false; 
  }

  // In this section we will calculate values common to both Battery and System modes
  if (lasttime == 0) {                                                              // First time through we are getting a bad reading - bail on this one.
    lasttime = runTime = micros();
    return false;
  }

  lasttime = runTime;                                                               // update times from last event
  runTime = micros();

  boolean polarity = digitalRead(polPin);                                           // Get polarity value 
  currentState.currentCurrent = 614.4/((runTime-lasttime)/1000000);                 // Calculate mA from time delay
  if (polarity) currentState.currentCurrent = -1.0 * currentState.currentCurrent;   // If charging, we'll set mA negative

  // Here we will perform calculations needed for the Battery Test mode
  if (batteryTestMode) {
    if (polarity) {                                                                 // high = charging
      currentState.currentCapacity_mAh += ah_quanta;
      currentState.currentCharge_percent += percent_quanta;
    }
    else {                                                                          // low = discharging
      currentState.currentCapacity_mAh -= ah_quanta;
      currentState.currentCharge_percent -= percent_quanta;
    }
    snprintf(capacityStr,sizeof(capacityStr),"%4.1f mAh",currentState.currentCapacity_mAh);
    snprintf(chargeStr,sizeof(chargeStr),"%3.1f %%",currentState.currentCharge_percent);
  }
  // Here we perform calculations needed for the Sleep Test mode
  else if (batteryTestMode == false && sleepState) {
    if (firstSampleDiscard) {
      firstSampleDiscard = false;
      return false;
    }
    numberOfSamples++;
    if (numberOfSamples > 10) numberOfSamples = 10; 
    index = (numberOfSamples-1) % 10; 
    currentBuffer[index] = currentState.currentCurrent;                           // In this section, we will calculate the average
    
    for (int i=0; i < numberOfSamples; i++) runningTotal += currentBuffer[i];
    currentState.averageCurrent = runningTotal/numberOfSamples;                        // average of readings

    snprintf(currentStr, sizeof(currentStr),"%4.2f mA",currentState.currentCurrent);
    snprintf(averageCurrentStr, sizeof(averageCurrentStr), "%4.2f mA", currentState.averageCurrent);
  }
  return true;
}

int setCapacity(String command)
{
  char * pEND;
  float inputValue = strtof(command,&pEND);                              // Looks for the first float and interprets it
  if ((inputValue < 0.0) || (inputValue > 6000.0)) return 0;              // Make sure it falls in a valid range or send a "fail" result
  currentState.startingCapacity_mAh = inputValue;                                              // Assign the input to the battery capacity variable
  snprintf(capacityStr,sizeof(capacityStr),"%4.1f mAh",currentState.startingCapacity_mAh);
  if (Particle.connected()) {                                            // Publish result if feeling verbose
    waitUntil(meterParticlePublish);
    Particle.publish("Capacity",capacityStr, PRIVATE);
  }
  return 1;
}

int setCharge(String command)
{
  char * pEND;
  float inputValue = strtof(command,&pEND);                              // Looks for the first float and interprets it
  if ((inputValue < 0.0) || (inputValue > 100.0)) return 0;              // Make sure it falls in a valid range or send a "fail" result
  currentState.startingCharge_percent = inputValue;                                              // Assign the input to the battery capacity variable
  snprintf(chargeStr,sizeof(chargeStr),"%3.1f %%",currentState.startingCharge_percent);
  if (Particle.connected()) {                                            // Publish result if feeling verbose
    waitUntil(meterParticlePublish);
    Particle.publish("Charge",chargeStr, PRIVATE);
  }
  return 1;
}

int resetTest(String command)                                           // Resets the current hourly and daily counts
{
  if (command == "1")
  {
    makeCalculations(true);                                             // This resets all the values
    return 1;
  }
  else return 0;
}

int setMode(String command)                                       // Resets the current hourly and daily counts
{
  if (command == "Battery") {
    batteryTestMode = true;
    makeCalculations(true);                                       // Resets all the values to start new mode
    waitUntil(meterParticlePublish);
    Particle.publish("Mode","Battery Capacity Test Mode",PRIVATE);
    return 1;
  }
  else if (command == "Sleep") {
    batteryTestMode = false;
    if (digitalRead(sleepIndicator)) sleepState = false;
    else sleepState = true;
    makeCalculations(true);                                       // Resets all the values to start new mode
    waitUntil(meterParticlePublish);
    Particle.publish("Mode","Sleep Test Mode",PRIVATE);
    return 1;
  }
  else return 0;
}

bool meterParticlePublish(void)
{
  static unsigned long lastPublish=0;                                   // Initialize and store value here
  if(millis() - lastPublish >= 1000) {                                  // Particle rate limits at 1 publish per second
    lastPublish = millis();
    return 1;
  }
  else return 0;
}