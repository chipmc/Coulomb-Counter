/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "application.h"
#line 1 "/Users/chipmc/Documents/Maker/Particle/Projects/Coulomb-Counter/src/Coulomb-Counter.ino"
/*
 * Project Coulomb-Counter
 * Description: Count the coulombs used in the Particle device so I can size batteries
 * Author: Chip McClelland adapted Mike's code (see attribution below)
 * Date: 9-22-19
 */

/* LTC4150 Coulomb Counter interrupt example code
Mike Grusin, SparkFun Electronics

This sketch shows how to use the LTC4150 Coulomb Counter breakout
board along with interrupts to implement a battery "gas gauge."

Product page: https://www.sparkfun.com/products/12052
Software repository: https://github.com/sparkfun/LTC4150_Coulomb_Counter_BOB

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

HARDWARE CONNECTIONS:

Before connecting this board to your Arduino, double check that
all three solder jumpers are set appropriately:

For this sketch, leave SJ1 soldered (closed).
This connects INT and CLR to clear interrupts automatically.

If you're using a 5V Arduino, leave both SJ2 and SJ3 open (unsoldered).

If you're using a 3.3V Arduino, close (solder) both SJ2 and SJ3.

Connect the following pins to your Arduino:

VIO to VCC
GND to GND
INT to D3
POL to D4

Note that if you solder headers to the bottom of the breakout board,
you can plug it directly into Arduino header pins D2 (VIO) through
D7 (SHDN).

RUNNING THE SKETCH:

This sketch monitors current moving into and out of a battery.
Whenever it detects a low INT signal from the LTC4150, it will
update the battery state-of-charge (how full the battery is),
current draw, etc.

The sketch is hardcoded for a 2000mAh battery that is 100% full
when the sketch starts. You can easily change this by editing
line 120 and 121:

  volatile double battery_mAh = 2000.0; // milliamp-hours (mAh)
  volatile double battery_percent = 100.0;  // state-of-charge (percent)

After uploading the sketch, open the Serial Monitor and set the
baud rate to 9600. Whenever the sketch detects an INT pulse, it
will update its calculations and print them out.

LICENSE:

Our example code uses the "beerware" license. You can do anything
you like with this code. No really, anything. If you find it useful
and you meet one of us in person someday, consider buying us a beer.

Have fun! -Your friends at SparkFun.
*/


// (If you are not plugging the board directly into the
// header, you can remove all references to VIO, GND,
// CLR and SHDN.)

// I edited the pin assignments to match the Particle Photon

void setup();
void loop();
void publishResult();
void myISR();
int setCapacity(String command);
int setCharge(String command);
int resetTest(String command);
bool meterParticlePublish(void);
#line 107 "/Users/chipmc/Documents/Maker/Particle/Projects/Coulomb-Counter/src/Coulomb-Counter.ino"
const int intPin = D2;                            // For the Particle Photon
const int polPin = D3;                            // Polarity signal
const int blueLED = D7;                           // Standard Arduino LED

// Change the following two lines to match your battery
// and its initial state-of-charge:

char capacityStr[16] = "NA";                       // String to make debounce more readable on the mobile app
char chargeStr[16] = "NA";                         // String to make debounce more readable on the mobile app

// Global variables ("volatile" means the interrupt can
// change them behind the scenes):
volatile boolean isrflag = false;                 // Interrupt flag
volatile unsigned long runTime, lasttime;         // These are based on micros() and cannot be saved
volatile float mA;
float ah_quanta = 0.17067759;                     // mAh for each INT
float percent_quanta;                             // % battery for each INT

// Keypad struct for mapping buttons, notes, note values, LED array index, and default color
struct battery_test_struct {                      // This structure will be saved at each coulomb
  unsigned long startTime;
  float startingCapacity_mAh;
  volatile float currentCapacity_mAh;
  float startingCharge_percent;
  volatile float currentCharge_percent;
};

battery_test_struct battery_data;

void setup()
{
  // Set up I/O pins:
  pinMode(intPin,INPUT);                          // Interrupt input pin (must be D2 or D3)
  pinMode(polPin,INPUT);                          // Polarity input pin
  pinMode(blueLED,OUTPUT);                        // Standard Particle status LED
  digitalWrite(blueLED,LOW);  

  Particle.function("Set-Capacity", setCapacity);  // Set the capacity
  Particle.function("Set-Charge", setCharge);     // Set the charge level
  Particle.function("Reset-Test",resetTest);      // Set all the values back to start

  Particle.variable("Capacity", capacityStr);
  Particle.variable("Charge", chargeStr);                   // charge value

  attachInterrupt(intPin,myISR,FALLING);

  waitUntil(Particle.connected);                  // Get connected first - helps to ensure we have the right time

  EEPROM.get(0,battery_data);

  if (Time.now() - battery_data.startTime > 300) {    // Too much time went by, must be a new test
    battery_data.startTime = Time.now();            // When did we start the test
    battery_data.currentCapacity_mAh = battery_data.startingCapacity_mAh;
    battery_data.currentCharge_percent = battery_data.startingCharge_percent;
    EEPROM.put(0,battery_data);
  }

  percent_quanta = 1.0/(battery_data.startingCapacity_mAh/1000.0*5859.0/100.0);   // % battery for each INT
  
  snprintf(capacityStr,sizeof(capacityStr),"%4.1f mAh",battery_data.currentCapacity_mAh);
  snprintf(chargeStr,sizeof(chargeStr),"%3.1f %%",battery_data.currentCharge_percent);

  Particle.publish("Startup","LTC4150 Coulomb Counter",PRIVATE);
}

void loop()
{
  if (isrflag) {
    isrflag = false;                              // Reset the flag to false so we only do this once per INT
    // Blink the LED
    digitalWrite(blueLED,HIGH);
    delay(100);
    digitalWrite(blueLED,LOW);
    publishResult();                              // Print out current status (variables set by myISR())
    EEPROM.put(0,battery_data);                   // Write the value to EEPROM
    snprintf(capacityStr,sizeof(capacityStr),"%4.1f mAh",battery_data.currentCapacity_mAh);
    snprintf(chargeStr,sizeof(chargeStr),"%3.1f %%",battery_data.currentCharge_percent);
  }
}

void publishResult() {
  char data[96];
  int elapsedSec = Time.now() - battery_data.startTime;
  snprintf(data, sizeof(data), "Status: %4.0f mAh, %3.1f%% charge, %4.3f mA at time %i:%i:%i:%i seconds", battery_data.currentCapacity_mAh, battery_data.currentCharge_percent, mA, Time.day(elapsedSec)-1, Time.hour(elapsedSec), Time.minute(elapsedSec), Time.second(elapsedSec));
  Particle.publish("Status",data,PRIVATE);
}

void myISR() // Run automatically for falling edge on D3 (INT1)
{
  static boolean polarity;
  // Determine delay since last interrupt (for mA calculation)
  // Note that first interrupt will be incorrect (no previous time!)
  lasttime = runTime;
  runTime = micros();

  // Get polarity value 
  polarity = digitalRead(polPin);
  if (polarity) // high = charging
  {
    battery_data.currentCapacity_mAh += ah_quanta;
    battery_data.currentCharge_percent += percent_quanta;
  }
  else // low = discharging
  {
    battery_data.currentCapacity_mAh -= ah_quanta;
    battery_data.currentCharge_percent -= percent_quanta;
  }

  // Calculate mA from time delay (optional)
  mA = 614.4/((runTime-lasttime)/1000000);

  // If charging, we'll set mA negative (optional)
  if (polarity) mA = mA * -1.0;
  
  // Set isrflag so main loop knows an interrupt occurred
  isrflag = true;
}

int setCapacity(String command)
{
  char * pEND;
  float inputValue = strtof(command,&pEND);                              // Looks for the first float and interprets it
  if ((inputValue < 0.0) || (inputValue > 6000.0)) return 0;              // Make sure it falls in a valid range or send a "fail" result
  battery_data.startingCapacity_mAh = inputValue;                                              // Assign the input to the battery capacity variable
  snprintf(capacityStr,sizeof(capacityStr),"%4.1f mAh",battery_data.startingCapacity_mAh);
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
  battery_data.startingCharge_percent = inputValue;                                              // Assign the input to the battery capacity variable
  snprintf(chargeStr,sizeof(chargeStr),"%3.1f %%",battery_data.startingCharge_percent);
  if (Particle.connected()) {                                            // Publish result if feeling verbose
    waitUntil(meterParticlePublish);
    Particle.publish("Charge",chargeStr, PRIVATE);
  }
  return 1;
}

int resetTest(String command)                                       // Resets the current hourly and daily counts
{
  if (command == "1")
  {
    battery_data.startTime = Time.now();            // When did we start the test
    battery_data.currentCapacity_mAh = battery_data.startingCapacity_mAh;
    battery_data.currentCharge_percent = battery_data.startingCharge_percent;
    EEPROM.put(0,battery_data);    
    snprintf(capacityStr,sizeof(capacityStr),"%4.1f mAh",battery_data.currentCapacity_mAh);
    snprintf(chargeStr,sizeof(chargeStr),"%3.1f %%",battery_data.currentCharge_percent);
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