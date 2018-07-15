#include "FastLED.h"
//#include <Audio.h>
//#include <SD.h>
//#include <Wire.h>
#define LOG_OUT 1 // use the log output function
#define FHT_LOG 8
#define FHT_N (1<<FHT_LOG) // set to 256 point fht
#define OCTAVE 1
#include <FHT.h>

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

#define BUTTON_DEBOUNCE  20
#define LIGHT_THRESHOLD (300*3300/4096)  //300mV
#define LIGHT_DEBOUNCE 30000

// GUItool: begin automatically generated code
/*
//// NOT ON TEENSY-LC
AudioInputAnalog         adc1(MICROPHONE_AIN);           //xy=240.3333282470703,59.33332824707031
AudioAnalyzeFFT256       audioFFT;       //xy=315.33331298828125,157.33334350585938
AudioAnalyzeRMS          audioRMS;           //xy=414.33331298828125,74.33333587646484
AudioConnection          patchCord1(adc1, audioFFT);
AudioConnection          patchCord2(adc1, audioRMS);
*/
// GUItool: end automatically generated code

#define AUTOSCALE 1


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

	uint16_t level = analogRead(PHOTODIODE_AIN);
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

void task_sample_mic()
{
	int32_t avg = 0;
	for(int i=0; i<FHT_N; i++)
	{
		int16_t val = analogRead(MICROPHONE_AIN);
		fht_input[i] = val;
	}

	fht_window(); // window the data for better frequency response
	fht_reorder(); // reorder the data before doing the fht
	fht_run(); // process the data in the fht
	fht_mag_octave(); // take the output of the fht

	// Serial.write(255); // send a start byte
	// Serial.write(fht_log_out, FHT_N/2); // send out the data

// 	//remove DC offset and gain up to 16 bits
// 	avg = avg/FHT_N;
// 	for (int i=0; i<FHT_N; i++)
// 		fft_data_[i] = (fft_data_[i] - avg) * 64;

// 	//run the FFT
// 	ZeroFFT(fft_data_, FHT_N);

// #if AUTOSCALE
//   //get the maximum value
//   float maxVal = 0;
//   //fft_data_ is only meaningful up to sample rate/2, discard the other half
//   for(int i=0; i<FHT_N/2; i++)
//   	if(fft_data_[i] > maxVal)
//   		maxVal = fft_data_[i];

//   //normalize to the maximum returned value
//   for(int i=0; i<FHT_N/2; i++)
//     fft_data_[i] =(float)fft_data_[i] / maxVal * 256;
// #endif

}

uint16_t animation_black()
{
	for (ledctr_t led=0; led<NUM_LEDS; led++)
		leds_[led].fadeToBlackBy(80);
	return 1000;
}

uint16_t animation_fft_hue()
{
	for (ledctr_t l=0; l<min(FHT_N/2,NUM_LEDS);l++)
	{
		CHSV x(fht_log_out[l],128,fht_log_out[l]);
		hsv2rgb_rainbow(x,leds_[l]);
	}
	return 1; //1ms max delay
}

uint16_t animation_fft_octaves()
{
	const uint8_t num_octaves = FHT_LOG;
	const ledctr_t ledwith_octave = NUM_LEDS / num_octaves;
	for (ledctr_t oct=0; oct < num_octaves; oct++)
	{
		ledctr_t middle = oct*ledwith_octave + (ledwith_octave/2);
		ledctr_t width = fht_oct_out[oct] * ledwith_octave / (1<<(sizeof(int16_t)-1));
		uint8_t value = fht_oct_out[oct] * ledwith_octave / (1<<(sizeof(int16_t)-1));
	}	
}

uint16_t animation_whitestrip()
{
	static ledctr_t whiteLed=0;
	// Turn our current led on to white, then show the leds
	leds_[whiteLed] = CRGB::Black;
	leds_[addmod8(whiteLed,1,NUM_LEDS)] = CRGB::White;
	leds_[addmod8(whiteLed,2,NUM_LEDS)] = CRGB::White;
	leds_[addmod8(whiteLed,3,NUM_LEDS)] = CRGB::White;
	whiteLed++;
	whiteLed%=NUM_LEDS;
	return 100;
}

/*
uint16_t animation_audio_rms()
{
	static uint32_t audiopower=0;
	if (!audioRMS.available())
		return 10;
	float rms = audioRMS.read(); //0.0 ... 1.0
	audiopower+=static_cast<uint32_t>(rms*20);
	for (ledctr_t l=NUM_LEDS-1; l>0; l++)
	{
		leds[l]=leds[l-1];
	}
	if (audiopower > 0)
	{
		leds[0]=CRGB::Orange;
		audiopower--;
	} else {
		leds[0]=CRGB::Black;
	}
	return 10;
}
*/

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
				delay_ms=animation_fft_hue(); //FIXME
			else
				delay_ms=animation_black();
			break;
		case ANIM_FFT:
			delay_ms=animation_whitestrip(); //FIXME
			break;
		case ANIM_SPARKLES:
			delay_ms=animation_whitestrip(); //FIXME
			break;
		case ANIM_WHITESTRIP_TEST:
			delay_ms=animation_whitestrip(); //FIXME
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
		btn_count_++;
	} else {
		btn_count_=0;
	}

	if (btn_count_ == BUTTON_DEBOUNCE)
	{
		animation_current_++;
		animation_current_%=NUM_ANIM;
		save_to_EEPROM();
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
