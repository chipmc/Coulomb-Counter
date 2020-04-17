# Coulomb-Counter

This sketch is based on a sketch from Sparkfun which I modified in two main ways:
1) I changed it to take advantage of the Particle Platform
2) I added a "Sleep Test" mode to facilitiate testing Particle device
current consumption when in different sleep modes

From the original Sparkfun code: This is  how to use the LTC4150 Coulomb Counter breakout
board along with interrupts to implement a battery "gas gauge."

Product page: https://www.sparkfun.com/products/12052
Software repository: https://github.com/sparkfun/LTC4150_Coulomb_Counter_BOB

Chip's Particle Modified Version - https://github.com/chipmc/Coulomb-Counter

## HOW IT WORKS:

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

## HARDWARE MODIFICATIONS ON THE SENSOR:

For this sketch, leave SJ1 soldered (closed).
This connects INT and CLR to clear interrupts automatically.

Since the Particle Argon is 3.3V, we need to close (solder) both SJ2 and SJ3.

## RUNNING THE SKETCH:

### BATTERY TEST MODE (default):
This sketch monitors current moving into and out of a battery.
Whenever it detects a low INT signal from the LTC4150, it will
update the battery state-of-charge (how full the battery is),
current draw, etc.

### SLEEP TEST MODE:
In this mode you use the D7 pin on the device you are testing to be "On"
when the device is awake and "off" when it is asleep.  Calculations on
instant and average current will start once the device sleeps and 
stop and reset once it is awake.

The sketch uses Particle functions to set the capacity nd charge level
of the battery.  It also has a function to reset the sketch to initial
values.  All output is in the Particle console.


## LICENSE - FROM ORIGINAL SPARKFUN SKETCH:

Our example code uses the "beerware" license. You can do anything
you like with this code. No really, anything. If you find it useful
and you meet one of us in person someday, consider buying us a beer.

Have fun! -Your friends at SparkFun.
*/

// Chip's Version tracking
// v1.0 - First pass at adding a second mode - Sleep testing
// v1.1 - Improved sampling and simplified program flow
