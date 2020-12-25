# Ergonomic timer with "machine learning"

Project contains a firmware of periodic timer.
Firmware is written for microcontroller Attiny85.
Schematics is dead simple couple buttons, leds, relays and npn 3904 transistors, and
it is not present.

## User guide

Periodic timer has period 24 hours. There is no binding to localtime.
It is designed to be simple in programming for old people.

1. Plug-in to the power socket in the moment when you want to turn on something.
2. Press Stop button to turn off the usefull load and mark turn off time for consequent days.
That is all with the main usecase.

The Pause button is used to turn off the load temporarily (auto unpause in 50 min), 
without changing the regular schedule.
Let's say you want to water plats or to run a treadmeal and it is inside a winter garden 
with supportive lighting controll by this device.

Time spents on pause is compensated through postponing turn off time in that day.


