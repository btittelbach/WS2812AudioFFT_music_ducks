#include <WS2812Serial.h>
#define USE_WS2812SERIAL
#include <FastLED.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <vector>

///// Requirements:
/// * newest FastLED from GitHub (master branch > 3.1.6)
/// * PJRC Audio (included in Teensyduino)
/// * Teensy 3.2
/// * https://github.com/PaulStoffregen/WS2812Serial
///

// Data pin that led data will be written out over
#define WS2812_PIN 5

#define BUTTON_PIN 12

#define LED_PIN 13

#define MICROPHONE_AIN A2

#define PHOTORESISTOR_AIN A3
#define PHOTORESISTOR_PIN 17
//#define PHOTORESISTOR_USE_ADC

#define NUM_LEDS 150
#define BUTTON_DEBOUNCE  500
#define LIGHT_THRESHOLD (500*3300/4096)  //500mV
#define LIGHT_DEBOUNCE 50000

#define EEPROM_CURRENT_VERSION 0
#define EEPROM_ADDR_VERS 0
#define EEPROM_ADDR_CURANIM 1


// GUItool: begin automatically generated code
#ifdef PHOTORESISTOR_USE_ADC
AudioInputAnalogStereo   adc_stereo(MICROPHONE_AIN, PHOTORESISTOR_AIN);          //xy=70.33332824707031,228.33334350585938
#else
AudioInputAnalog   		 adc_stereo(MICROPHONE_AIN);          //xy=70.33332824707031,228.33334350585938
#endif
AudioAnalyzeRMS          audioRMS;           //xy=297.33331298828125,201.33334350585938
AudioAnalyzeFFT256       audioFFT;       //xy=305.33331298828125,240.33334350585938
AudioAnalyzePeak         audioPeak;
#ifdef PHOTORESISTOR_USE_ADC
AudioAnalyzePeak         photoPeak;          //xy=310.33331298828125,287.33331298828125
#endif
//AudioOutputAnalog        dac1;           //xy=329,47 --> DAC Pin
//AudioOutputUSB           usb1;           //xy=220.3333282470703,342.3333282470703
AudioConnection          patchCord1(adc_stereo, 0, audioRMS, 0);
AudioConnection          patchCord2(adc_stereo, 0, audioFFT, 0);
AudioConnection          patchCord3(adc_stereo, 0, audioPeak, 0);
#ifdef PHOTORESISTOR_USE_ADC
AudioConnection          patchCord4(adc_stereo, 1, photoPeak, 0);
#endif
//AudioConnection          patchCord5(adc_stereo, 0, usb1, 0);
//AudioConnection          patchCord6(adc_stereo, 1, usb1, 1);
//AudioConnection          patchCord7(adc_stereo, 0, dac1, 0);
// GUItool: end automatically generated code

#define FFT_SIZE 256

CRGB leds_[NUM_LEDS];
bool is_dark_=true;
float last_max_peak_=0.01;
int32_t dark_count_=0;
uint16_t light_level=0;

#include "animations.h"

AnimationBlack anim_fade_to_black;
AnimationPlasma anim_plasma;
RunOnlyInDarkness anim_plasma_when_dark(anim_plasma, anim_fade_to_black);
AnimationRMSHue anim_rms_hue;
RunOnlyInDarkness anim_rms_hue_when_dark(anim_rms_hue, anim_fade_to_black);
AnimationFFTOctaves anim_fft_octaves;
RunOnlyInDarkness anim_fft_octaves_when_dark(anim_fft_octaves, anim_fade_to_black);
AnimationGravityDots anim_gravity_dots;
RunOnlyInDarkness anim_gravity_dots_when_dark(anim_gravity_dots, anim_fade_to_black);
AnimationFireworks anim_fireworks;
RunOnlyInDarkness anim_fireworks_when_dark(anim_fireworks, anim_fade_to_black);
AnimationFire2012 anim_fire2012;
RunOnlyInDarkness anim_fire2012_when_dark(anim_fire2012, anim_fade_to_black);
AnimationConfetti anim_confetti;
RunOnlyInDarkness anim_confetti_when_dark(anim_confetti, anim_fade_to_black);
AnimationRMSConfetti anim_rms_confetti;
RunOnlyInDarkness anim_rms_confetti_when_dark(anim_rms_confetti, anim_fade_to_black);
AnimationRainbowGlitter anim_rainbow(false);
RunOnlyInDarkness anim_rainbow_when_dark(anim_rainbow, anim_fade_to_black);
AnimationRainbowGlitter anim_rainbow_w_glitter(true);
RunOnlyInDarkness anim_rainbow_w_glitter_when_dark(anim_rainbow_w_glitter, anim_fade_to_black);


AnimationFullFFT anim_fft_full_and_boring;
AnimationPhotosensorDebugging anim_photoresistor_debugging;
AnimationStripTest anim_strip_debugging;

std::&vector<BaseAnimation*> animations_list_=
	{&anim_fft_octaves
	,&anim_fft_octaves_when_dark
	,&anim_rms_hue
	,&anim_rms_hue_when_dark
	,&anim_rms_confetti
	,&anim_rms_confetti_when_dark
	,&anim_plasma
	,&anim_plasma_when_dark
	,&anim_gravity_dots
	,&anim_gravity_dots_when_dark
	,&anim_fireworks
	,&anim_fireworks_when_dark
	,&anim_fire2012
	,&anim_fire2012_when_dark
	,&anim_confetti
	,&anim_confetti_when_dark
	,&anim_rainbow
	,&anim_rainbow_when_dark
	,&anim_rainbow_w_glitter
	,&anim_rainbow_w_glitter_when_dark
	,&anim_fft_full_and_boring
	,&anim_photoresistor_debugging
	,&anim_strip_debugging
	};

uint8_t animation_current_= 1;
#define NUM_ANIM animations_list_.size()


// This function sets up the ledsand tells the controller about them
void setup() {
	AudioMemory(12);
	delay(2000);

	FastLED.addLeds<WS2812SERIAL,WS2812_PIN,GRB>(leds_,NUM_LEDS);
	pinMode(LED_PIN,OUTPUT);
	digitalWrite(LED_PIN, LOW);
	pinMode(BUTTON_PIN, INPUT_PULLUP);
	pinMode(PHOTORESISTOR_AIN, INPUT);
	pinMode(PHOTORESISTOR_PIN, INPUT);
	pinMode(MICROPHONE_AIN, INPUT);
	//init animation
	animations_list_[animation_current_]->init();
	load_from_EEPROM();
}

void save_to_EEPROM()
{
  if (eeprom_read_byte((const uint8_t*)EEPROM_ADDR_VERS) != EEPROM_CURRENT_VERSION)
    eeprom_write_byte((uint8_t *) EEPROM_ADDR_VERS, EEPROM_CURRENT_VERSION);
  if (animation_current_ != eeprom_read_byte((const uint8_t*)EEPROM_ADDR_CURANIM))
    eeprom_write_byte((uint8_t *) EEPROM_ADDR_CURANIM, animation_current_);
}

void load_from_EEPROM()
{
  if (eeprom_read_byte(EEPROM_ADDR_VERS) != EEPROM_CURRENT_VERSION)
    return;

  animation_current_ = eeprom_read_byte((const uint8_t*)EEPROM_ADDR_CURANIM) % NUM_ANIM;
}


void task_check_lightlevel()
{
#ifdef PHOTORESISTOR_USE_ADC
	if (!photoPeak.available())
		return;

	// uint16_t light_level = analogReadADC1(PHOTORESISTOR_AIN);
	light_level = photoPeak.read()*4095;
	if (light_level > LIGHT_THRESHOLD)
#else
	if (digitalRead(PHOTORESISTOR_PIN) == LOW)
#endif
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

void task_animate_leds()
{
	static millis_t next_run=0;

	//ignore overflow for now, as the only thing that happens that we continue immediately once.
	if (millis() < next_run)
		return;

	//run current animation
	millis_t delay_ms = animations_list_[animation_current_]->run();

	// Show the leds (only one of which is set to white, from above)
	FastLED.show();
	next_run=millis()+delay_ms;
}

void animation_switch_next()
{
	animation_current_++;
	animation_current_%=NUM_ANIM;
	save_to_EEPROM();
	animations_list_[animation_current_]->init();
}

void task_check_button()
{
	static uint16_t btn_count_=0;
	if (digitalRead(BUTTON_PIN) == LOW)
	{
		if (btn_count_ < BUTTON_DEBOUNCE+1)
		{
			btn_count_++;
			digitalWrite(LED_PIN,HIGH);
		} else {
			digitalWrite(LED_PIN,LOW);
		}
	} else {
		btn_count_=0;
	}

	if (btn_count_ == BUTTON_DEBOUNCE)
	{
		animation_switch_next();
	}
}

void task_heartbeat()
{
	static uint8_t hbled=0;
	digitalWrite(LED_PIN,(hbled % 16 == 0)?HIGH:LOW);
	hbled++;
}

void loop() {
   task_heartbeat();
   task_check_button();
   task_check_lightlevel();
   task_sample_mic();
   task_animate_leds();
}
