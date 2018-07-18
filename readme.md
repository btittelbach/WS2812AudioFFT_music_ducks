WS2812 FFT Visualisation for realraum Ducks and music festivals
===============================================================


Usage
-----

Connect to WS2812B 150 LED Strip and Battery.

Press Button to switch to next Effect or EffectCollection.

Depending on current setting, the currently used effect depends on weather daylight is detected or not.


Daylight Sensor
---------------

Is connected to two trim-potis. Teensy 3.2 pins schmidth-trigger for LOW below 0.35V and for HIGH above 0.7V.
Adjust the two trim-potis to trigger at the light-levels of your choice.

Alternatively the Daylight Sensor pin can be sampled with the ADC via the AudioLibrary as second channel of a stereo input.
That might enable more involved light detection.


Audio Sensor
------------

The sensor is an ... and needs between 1.5V and 3.3V Vdd and outputs audio signals in the range 0V..Vdd.
The PJRC-Audio Library sets the ADC voltage refernce to INTERNAL (1.2V) in order to reduce noise. (The voltage reference is Teensy Vdd, which can be noisy coming from USB)

In order to simplify the schematic, I choose drive the microphone directly with 3.3V Vdd.

This is fine since:
1. The LED strip is supposed to blink. So some noise and random blinking would be perfectly fine.
2. Since we drive directly  Teensy from LiIon Battery, there is so power supply noise.

It _does_ mean that you have to patch the PJRC-Audio lib however when compiling this.

    --- hardware/teensy/avr/libraries/Audio/input_adc.cpp   2018-07-18 22:52:21.620422432 +0200
    +++ /home/bernhard/Dropbox/r3/input_adc.cpp     2018-07-18 22:24:04.528331672 +0200
    @@ -48,7 +48,7 @@
    				// conversion.  This completes the self calibration stuff and
    				// leaves the ADC in a state that's mostly ready to use
    				analogReadRes(16);
    -       analogReference(INTERNAL); // range 0 to 1.2 volts
    +//     analogReference(INTERNAL); // range 0 to 1.2 volts
     #if F_BUS == 96000000 || F_BUS == 48000000 || F_BUS == 24000000
    				analogReadAveraging(8);
     #else


