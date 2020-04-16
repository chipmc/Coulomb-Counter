/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "Particle.h"
#line 1 "/Users/chipmc/Documents/Maker/Particle/Projects/Coulomb-Counter/src/Coulomb-Counter.ino"
/*
 * Project Coulomb-Counter for Sleep Current Measurements
 * Description: Count the coulombs used in the Particle device so I can size batteries and determine current requriements
 * Author: Chip McClelland adapted Mike's code (see attribution below)
 * Date: 9-22-19
 */

/* LTC4150 Coulomb Counter interrupt example code
Mike Grusin, SparkFun Electronics

This sketch shows how to use the LTC4150 Coulomb Counter breakout
board along with interrupts to implement a battery "gas gauge."

Product page: https://www.sparkfun.com/products/12052
Software repository: https://github.com/sparkfun/LTC4150_Coulomb_Counter_BOB

Chip's Particle Modified Version - https://github.com/chipmc/Coulomb-Counter

HOW IT WORKS:

Battery capacity is measured in amp-hours (Ah). For example, a one
amp-hour battery can provide 1 amp of current for one hour, or 2 amps
for half an hour, or half an amp for two hours, etc.

The LTC4150 monitors current passing into or out of a battery.
It has an output called INT (interrupt) that will pulse low every
time 0.0001707 amp-hours passes through the part. Or to put it
another way, the INT signal will pulse 5859 times for one amp-hour.

If you hook up a full 1Ah (1000mAh) battery to the LTC4150, you
can expect to get 5859 pulses before it's depleted. If you keep track
of these pulses, you can accurately determine the remaining battery
capacity.

There is also a POL (polarity) signal coming out of the LTC4150.
When you detect a pulse, you can check the POL signal to see whether
current is moving into or out of the battery. If POL is low, current is
coming out of the battery (discharging). If POL is high, current is
going into the battery (charging).

(Note that because of chemical inefficiencies, it takes a bit more current
to charge a battery than you will eventually get out of it. This sketch
does not take this into account. For better accuracy you might provide
a method to "zero" a full battery, either automatically or manually.)

Although it isn't the primary function of the part, you can also
measure the time between pulses to calculate current draw. At 1A
(the maximum allowed), INT will pulse every 0.6144 seconds, or
1.6275 Hz. Note that for low currents, pulses will be many seconds
apart, so don't expect frequent updates.

HARDWARE MODIFICATIONS ON THE SENSOR:

For this sketch, leave SJ1 soldered (closed).
This connects INT and CLR to clear interrupts automatically.

Since the Particle Argon is 3.3V, we need to close (solder) both SJ2 and SJ3.


RUNNING THE SKETCH:

BATTERY TEST MODE (default):
This sketch monitors current moving into and out of a battery.
Whenever it detects a low INT signal from the LTC4150, it will
update the battery state-of-charge (how full the battery is),
current draw, etc.

SLEEP TEST MODE:
In this mode you use the D7 pin on the device you are testing to be "On"
when the device is awake and "off" when it is asleep.  Calculations on
instant and average current will start once the device sleeps and 
stop and reset once it is awake.

The sketch uses Particle functions to set the capacity nd charge level
of the battery.  It also has a function to reset the sketch to initial
values.  All output is in the Particle console.


LICENSE - FROM ORIGINAL SPARKFUN SKETCH:

Our example code uses the "beerware" license. You can do anything
you like with this code. No really, anything. If you find it useful
and you meet one of us in person someday, consider buying us a beer.

Have fun! -Your friends at SparkFun.
*/


// I edited the pin assignments to match the Particle Photon / Argon

void setup();
void loop();
void publishResult();
void coulombISR();
void sleepWakeISR();
void makeCalculations(bool resetValues);
int setCapacity(String command);
int setCharge(String command);
int resetTest(String command);
int setMode(String command);
bool meterParticlePublish(void);
#line 91 "/Users/chipmc/Documents/Maker/Particle/Projects/Coulomb-Counter/src/Coulomb-Counter.ino"
const int intPin = D2;                            // For the Particle Photon
const int polPin = D3;                            // Polarity signal
const int blueLED = D7;                           // Standard Arduino LED
const int sleepIndicator = D4;                    // Connects device we are testing indicates sleep / wake

// Change the following two lines to match your battery
// and its initial state-of-charge:

char capacityStr[16] = "NA";                      // String expressions to add units for Console viewing
char chargeStr[16] = "NA";                         
char currentStr[16] = "NA";      
char averageCurrentStr[16] = "NA";                

// Global variables ("volatile" means the interrupt can
// change them behind the scenes):
volatile boolean coulombFlag = false;             // Interrupt flag each time a coulumb is counted
volatile boolean sleepFlag = false;               // Interrupt indicating a change in the sleep / wake cycle
const float ah_quanta = 0.17067759;               // mAh for each INT
float percent_quanta;                             // % battery for each INT
bool batteryTestMode = true;                      // default to Battery Test Mode
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
}

void loop()
{
  if (coulombFlag) {
    coulombFlag = false;                                                    // Reset the flag to false so we only do this once per INT
    makeCalculations(false);                                                // Update the calculations
    digitalWrite(blueLED,HIGH);                                             // Blink the LED
    delay(100);
    digitalWrite(blueLED,LOW);
    publishResult();                                                        // Print out current status (variables set by myISR())
  }
  if (sleepFlag) {
    sleepFlag = false;
    if (digitalRead(sleepIndicator)) {
      sleepState = false;
      waitUntil(meterParticlePublish);
      Particle.publish("State","Device Awake - Resetting",PRIVATE);
      makeCalculations(true);                                           // This resets all the values and gets us ready for the next sleep cycle
      setMode("Sleep");
    }
    else {
      sleepState = true;                                                 // In the next sleep cycle - start tracking
      waitUntil(meterParticlePublish);
      Particle.publish("Status","Device Sleeping - starting test",PRIVATE);
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
  else snprintf(data,sizeof(data),"Waiting for device to sleep");

  Particle.publish("Status",data,PRIVATE);
}

void coulombISR() {                                                       // Run automatically for falling edge on D2
  coulombFlag = true;                                                     // Set isrflag so main loop knows an interrupt occurred
}

void sleepWakeISR() {                                                     // Runs when the device under test changes sleep / wake state
  sleepFlag = true;
}

void makeCalculations(bool resetValues) {
  static unsigned long runTime = 0; 
  static unsigned long lasttime = 0;                                      // These are based on micros()
  const int numReadings = 10;               // How Many numbers to average for calibration
  static float currentBuffer[numReadings];       // the readings from the pressure sensor for calibration
  static int index = 0;
  float runningTotal = 0;
  static bool bufferFull = false;
  int sampleOver = 0;

  if (resetValues) {
    currentState.startTime = Time.now();                                  // When did we start the test
    currentState.currentCapacity_mAh = currentState.startingCapacity_mAh;
    currentState.currentCharge_percent = currentState.startingCharge_percent;
    snprintf(capacityStr,sizeof(capacityStr),"%4.1f mAh",currentState.currentCapacity_mAh);
    snprintf(chargeStr,sizeof(chargeStr),"%3.1f %%",currentState.currentCharge_percent);
    strcpy(currentStr,"NA");
    strcpy(averageCurrentStr,"NA");
    lasttime = 0;
    index = 0;
    currentStateWriteNeeded = true;
    return; 
  }

  if (lasttime == 0) {                                                    // First time through we are getting a bad reading - bail on this one.
    lasttime = runTime;
    runTime = micros();
    return;
  }

  lasttime = runTime;                                                     // Note that first interrupt will be incorrect (no previous time!)
  runTime = micros();

  boolean polarity = digitalRead(polPin);                                         // Get polarity value 
  if (polarity) {                                                         // high = charging
    currentState.currentCapacity_mAh += ah_quanta;
    currentState.currentCharge_percent += percent_quanta;
  }
  else {                                                                  // low = discharging
    currentState.currentCapacity_mAh -= ah_quanta;
    currentState.currentCharge_percent -= percent_quanta;
  }

  currentState.currentCurrent = 614.4/((runTime-lasttime)/1000000);       // Calculate mA from time delay (optional)
  if (polarity) currentState.currentCurrent = -1.0 * currentState.currentCurrent;// If charging, we'll set mA negative (optional)

  if (batteryTestMode == false && sleepState) {
    if (index == numReadings || bufferFull) {
      bufferFull = true;
      sampleOver = numReadings;
    }
    else {
      sampleOver = index +1;
    }
    currentBuffer[index] = currentState.currentCurrent;
    index = (index + 1) % numReadings;
    for (int i=0; i < sampleOver; i++) runningTotal += currentBuffer[i];
    currentState.averageCurrent = runningTotal/sampleOver;               // average of readings
  }

  snprintf(capacityStr,sizeof(capacityStr),"%4.1f mAh",currentState.currentCapacity_mAh);
  snprintf(chargeStr,sizeof(chargeStr),"%3.1f %%",currentState.currentCharge_percent);
  snprintf(currentStr, sizeof(currentStr),"%4.2f mA",currentState.currentCurrent);
  snprintf(averageCurrentStr, sizeof(averageCurrentStr), "%4.2f mA", currentState.averageCurrent);
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