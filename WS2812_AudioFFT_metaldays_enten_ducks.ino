#include "FastLED.h"
#include <Audio.h>
#include <SD.h>
#include <Wire.h>

///////////////////////////////////////////////////////////////////////////////////////////
//
// Move a white dot along the strip of leds.  This program simply shows how to configure the leds,
// and then how to turn a single pixel white and then off, moving down the line of pixels.
// 

// How many leds are in the strip?
#define NUM_LEDS 150

typedef uint8_t ledctr_t;

// Data pin that led data will be written out over
#define WS2812_PIN 4

#define BUTTON_PIN 11

#define LED_PIN 13

#define MICROPHONE_AIN A0  //14

#define PHOTODIODE_AIN A9  //23

#define BUTTON_DEBOUNCE  6
#define LIGHT_THRESHOLD (300*3300/4096)  //300mV
#define LIGHT_DEBOUNCE 30000

// GUItool: begin automatically generated code
AudioInputAnalogStereo   adc_stereo(MICROPHONE_AIN, PHOTODIODE_AIN);          //xy=70.33332824707031,228.33334350585938
AudioAnalyzeRMS          audioRMS;           //xy=297.33331298828125,201.33334350585938
AudioAnalyzeFFT256       audioFFT;       //xy=305.33331298828125,240.33334350585938
AudioAnalyzePeak         photoPeak;          //xy=310.33331298828125,287.33331298828125
AudioConnection          patchCord1(adc_stereo, 0, audioRMS, 0);
AudioConnection          patchCord2(adc_stereo, 0, audioFFT, 0);
AudioConnection          patchCord3(adc_stereo, 1, photoPeak, 0);
// GUItool: end automatically generated code

#define FFT_SIZE 256

typedef uint8_t animation_t;
const animation_t ANIM_FFT_SPARKLES_WHEN_DARK=0;
const animation_t ANIM_FFT=1;
const animation_t ANIM_SPARKLES=2;
const animation_t ANIM_WHITESTRIP_TEST=3;
const animation_t NUM_ANIM=4;

animation_t animation_current_=ANIM_FFT_SPARKLES_WHEN_DARK;
bool is_dark_=true;

// This is an array of leds.  One item for each led in your strip.
CRGB leds_[NUM_LEDS];

// This function sets up the ledsand tells the controller about them
void setup() {
	// sanity check delay - allows reprogramming if accidently blowing power w/leds	
	delay(2000);

	FastLED.addLeds<WS2812B, WS2812_PIN,  RGB>(leds_, NUM_LEDS);
	pinMode(LED_PIN,OUTPUT);
	digitalWrite(LED_PIN, LOW);
	pinMode(BUTTON_PIN, INPUT_PULLUP);
	pinMode(PHOTODIODE_AIN, INPUT);
	pinMode(MICROPHONE_AIN, INPUT);
	load_from_EEPROM();
}

void save_to_EEPROM()
{
	//TODO
}

void load_from_EEPROM()
{
	//TODO
}

//TODO FIXME
/*
analogRead() must not be used, because AudioInputAnalog is regularly accessing the ADC hardware. If both access the hardware at the same moment, analogRead() can end up waiting forever, which effectively crashes your program. 
*/
void task_check_lightlevel()
{
	static int16_t dark_count_=0;

	if (!photoPeak.available())
		return;

	// uint16_t level = analogReadADC1(PHOTODIODE_AIN);
	uint16_t level = photoPeak.read()*4096;
	if (level > LIGHT_THRESHOLD)
	{
		//assume daylight
		if (dark_count_ < LIGHT_DEBOUNCE) {
			dark_count_++;
		}
	} else {
		//assume darkness
		if (dark_count_ > -LIGHT_DEBOUNCE) {
			dark_count_--;
		}
	}
	if (dark_count_ <= -LIGHT_DEBOUNCE)
	{
		is_dark_ = true;
	} else if (dark_count_ >= LIGHT_DEBOUNCE)
	{
		is_dark_ = false;
	}
}

inline void task_sample_mic()
{
	//done by PJRC Audio

}

uint16_t animation_black()
{
	for (ledctr_t led=0; led<NUM_LEDS; led++)
		leds_[led].fadeToBlackBy(80);
	return 1000;
}

uint16_t animation_fft_hue()
{
	if (!audioFFT.available())
		return 2;
	for (ledctr_t l=0; l<min(FFT_SIZE,NUM_LEDS);l++)
	{
		uint8_t v = audioFFT.read(l)*0xff;
		CHSV x(v,128,60);
		hsv2rgb_rainbow(x,leds_[l]);
	}
	return 10; //1ms max delay
}

#define NUM_OCTAVES  8 //log2(256)

void fft_calc_octaves255(uint8_t led_octaves_magnitude[NUM_OCTAVES])
{
	led_octaves_magnitude[0] =  audioFFT.read(0) * 0xff;
    led_octaves_magnitude[1] =  audioFFT.read(1) * 0xff;
    led_octaves_magnitude[2] =  audioFFT.read(2,  3) * 0xff;
    led_octaves_magnitude[3] =  audioFFT.read(4,  7) * 0xff;
    led_octaves_magnitude[4] =  audioFFT.read(8,  16) * 0xff;
    led_octaves_magnitude[5] = audioFFT.read(17,  32) * 0xff;
    led_octaves_magnitude[6] = audioFFT.read(33, 64) * 0xff;
    led_octaves_magnitude[7] = audioFFT.read(65, 127) * 0xff;
}

uint16_t animation_fft_octaves()
{
	if (!audioFFT.available())
		return 2;
	uint8_t led_octaves_magnitude[NUM_OCTAVES];
	//calc octaves magnitude
	fft_calc_octaves255(led_octaves_magnitude);

	//paint octaves to every second pixel
	for (ledctr_t o=0; o < NUM_OCTAVES; o++)
	{
		hsv2rgb_rainbow(CHSV(led_octaves_magnitude[o],128,led_octaves_magnitude[o]),leds_[o*2]);
	}

	//interpolate pixels in between
	for (ledctr_t o=0; o < NUM_OCTAVES-1; o++)
	{
		leds_[o*2+1].r = (leds_[o*2].r+leds_[(o+1)*2].r) / 2;
		leds_[o*2+1].g = (leds_[o*2].g+leds_[(o+1)*2].g) / 2;
		leds_[o*2+1].b = (leds_[o*2].b+leds_[(o+1)*2].b) / 2;
	}

	// /// repeat Pattern until strip ends
	// for (ledctr_t l=NUM_OCTAVES*2; l < NUM_LEDS; l+=(NUM_OCTAVES*2+3))
	// {
	// 	leds_[l] = CRGB::Black;
	// 	leds_[l+1] = CRGB::Black;
	// 	leds_[l+2] = CRGB::Black;
	// 	for (ledctr_t o=0; o < NUM_OCTAVES*2; o++)
	// 	{
	// 		leds_[l+3+o]=leds_[o];
	// 	}
	// }

	return 10;
}

		// ledctr_t middle = oct*ledwith_octave + (ledwith_octave/2);
		// ledctr_t width = fht_oct_out[oct] * ledwith_octave / (1<<(sizeof(int16_t)-1));
		// uint8_t value = fht_oct_out[oct] * ledwith_octave / (1<<(sizeof(int16_t)-1));

uint16_t animation_striptest()
{
	static ledctr_t whiteLed=0;
	// Turn our current led on to white, then show the leds
	leds_[whiteLed+0] = CRGB::Black;
	leds_[(whiteLed+1)%NUM_LEDS] = CRGB::Blue;
	leds_[(whiteLed+2)%NUM_LEDS] = CRGB::Green;
	leds_[(whiteLed+3)%NUM_LEDS] = CRGB::Red;
	leds_[(whiteLed+4)%NUM_LEDS] = CRGB::White;
	whiteLed++;
	whiteLed %= NUM_LEDS;
	return 100;
}


uint16_t animation_audio_rms()
{
	static uint32_t audiopower=0;
	if (!audioRMS.available())
		return 2;
	float rms = audioRMS.read(); //0.0 ... 1.0
	audiopower+=static_cast<uint32_t>(rms*20);
	for (ledctr_t l=NUM_LEDS-1; l>0; l++)
	{
		leds_[l]=leds_[l-1];
	}
	if (audiopower > 0)
	{
		leds_[0]=CRGB::Orange;
		audiopower--;
	} else {
		leds_[0]=CRGB::Black;
	}
	return 100;
}


void task_animate_leds()
{
	static uint32_t next_run=0;

	if (millis() < next_run)
		return;

	uint32_t delay_ms=0;
	switch (animation_current_)
	{
		default:
		case ANIM_FFT_SPARKLES_WHEN_DARK:
			if (is_dark_)
				delay_ms=animation_audio_rms(); //FIXME
			else
				delay_ms=animation_black();
			break;
		case ANIM_FFT:
			delay_ms=animation_fft_hue(); //FIXME
			break;
		case ANIM_SPARKLES:
			delay_ms=animation_fft_octaves(); //FIXME
			break;
		case ANIM_WHITESTRIP_TEST:
			delay_ms=animation_striptest(); //FIXME
			break;
	}
	// Show the leds (only one of which is set to white, from above)
	FastLED.show();
	next_run=millis()+delay_ms;
}

void task_check_button()
{
	static uint16_t btn_count_=0;
	if (digitalRead(BUTTON_PIN) == LOW)
	{
		if (btn_count_ < BUTTON_DEBOUNCE+1)
			btn_count_++;
	} else {
		btn_count_=0;
	}

	if (btn_count_ == BUTTON_DEBOUNCE)
	{
		animation_current_++;
		animation_current_%=NUM_ANIM;
		save_to_EEPROM();
		for (ledctr_t led=0; led<NUM_LEDS; led++)
			leds_[led] = CRGB::Black;
	}
}

void task_heartbeat()
{
	static bool hbled=false;
	digitalWrite(LED_PIN,(hbled)?HIGH:LOW);
	hbled = !hbled;	
}

void loop() {
   // Move a single white led_ 
   task_heartbeat();
   task_check_button();
   task_check_lightlevel();
   task_sample_mic();
   task_animate_leds();
}
