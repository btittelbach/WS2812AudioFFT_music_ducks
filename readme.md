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

Pretty weak signal currently. Needs fixing.
