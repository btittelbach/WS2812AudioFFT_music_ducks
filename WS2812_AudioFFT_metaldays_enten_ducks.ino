#include <WS2812Serial.h>
#define USE_WS2812SERIAL
#include <FastLED.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

///// Requirements:
/// * newest FastLED from GitHub (master branch > 3.1.6)
/// * PJRC Audio (included in Teensyduino)
/// * Teensy 3.2
/// * https://github.com/PaulStoffregen/WS2812Serial
///

// How many leds are in the strip?
#define NUM_LEDS 150

typedef uint32_t ledctr_t;
typedef unsigned long millis_t;

// Data pin that led data will be written out over
#define WS2812_PIN 5

#define BUTTON_PIN 12

#define LED_PIN 13

#define MICROPHONE_AIN A2

#define PHOTORESISTOR_AIN A3
#define PHOTORESISTOR_PIN 17
//#define PHOTORESISTOR_USE_ADC

#define BUTTON_DEBOUNCE  400
#define LIGHT_THRESHOLD (500*3300/4096)  //500mV
#define LIGHT_DEBOUNCE 30000


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

typedef uint8_t animation_t;
const animation_t ANIM_FFT_SPARKLES_WHEN_DARK=0;
const animation_t ANIM_FFT=1;
const animation_t ANIM_SPARKLES=2;
const animation_t ANIM_STRIP_TEST=3;
const animation_t ANIM_PHOTOSENSOR_TEST=4;
const animation_t ANIM_RMS=5;
const animation_t NUM_ANIM=6;

animation_t animation_current_=ANIM_FFT_SPARKLES_WHEN_DARK;
bool is_dark_=true;
float last_max_peak_=0.01;

// This is an array of leds.  One item for each led in your strip.
CRGB leds_[NUM_LEDS];

// This function sets up the ledsand tells the controller about them
void setup() {
	AudioMemory(12);
	delay(2000);

	FastLED.addLeds<WS2812SERIAL,WS2812_PIN,GRB>(leds_,NUM_LEDS);
	FastLED.setBrightness(127);
	pinMode(LED_PIN,OUTPUT);
	digitalWrite(LED_PIN, LOW);
	pinMode(BUTTON_PIN, INPUT_PULLUP);
	pinMode(PHOTORESISTOR_AIN, INPUT);
	pinMode(PHOTORESISTOR_PIN, INPUT);
	pinMode(MICROPHONE_AIN, INPUT);
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

//TODO FIXME
/*
analogRead() must not be used, because AudioInputAnalog is regularly accessing the ADC hardware. If both access the hardware at the same moment, analogRead() can end up waiting forever, which effectively crashes your program.
*/
int32_t dark_count_=0;
uint16_t light_level=0;
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

millis_t animation_black()
{
	for (ledctr_t led=0; led<NUM_LEDS; led++)
		leds_[led].fadeToBlackBy(80);
	return 1000;
}

millis_t animation_fft_hue()
{
	if (!audioFFT.available())
		return 2;
	for (ledctr_t l=0; l<min(FFT_SIZE,NUM_LEDS);l++)
	{
		uint8_t v = static_cast<uint8_t>(audioFFT.read(l)*255.0);
		CHSV x(v,128,60);
		hsv2rgb_rainbow(x,leds_[l]);
	}
	return 10; //1ms max delay
}

#define NUM_OCTAVES  8 //log2(256)

void fft_calc_octaves255(uint8_t led_octaves_magnitude[NUM_OCTAVES])
{
	led_octaves_magnitude[0] =  audioFFT.read(0) /last_max_peak_ * 0xff;
    led_octaves_magnitude[1] =  audioFFT.read(1) /last_max_peak_ * 0xff;
    led_octaves_magnitude[2] =  audioFFT.read(2,  3) /last_max_peak_ * 0xff;
    led_octaves_magnitude[3] =  audioFFT.read(4,  7) /last_max_peak_ * 0xff;
    led_octaves_magnitude[4] =  audioFFT.read(8,  16) /last_max_peak_ * 0xff;
    led_octaves_magnitude[5] = audioFFT.read(17,  32) /last_max_peak_ * 0xff;
    led_octaves_magnitude[6] = audioFFT.read(33, 64) /last_max_peak_ * 0xff;
    led_octaves_magnitude[7] = audioFFT.read(65, 127) /last_max_peak_ * 0xff;
}

uint8_t get_fft_octaves_beat(uint8_t led_octaves_magnitude[NUM_OCTAVES])
{
	uint8_t beat=0;
	for (uint8_t o=1; o<NUM_OCTAVES; o++)
	{
		beat += ((led_octaves_magnitude[0] > 180)? 1:0);
	}
	//again for last one, adding +2 in sum if beat.
	beat += ((led_octaves_magnitude[NUM_OCTAVES-1] > 180)? 1:0);
	return beat;
}

millis_t animation_fft_octaves()
{
	static uint8_t last_beat=0;
	static ledctr_t led_shift=0;

	const millis_t default_delay=10;

	if (!audioFFT.available() || !audioPeak.available())
		return 2;
	uint8_t led_octaves_magnitude[NUM_OCTAVES];
	//calc octaves magnitude
	fft_calc_octaves255(led_octaves_magnitude);
	last_max_peak_ = max(last_max_peak_,audioPeak.read());
	uint8_t beat = get_fft_octaves_beat(led_octaves_magnitude);

	//set brightness depending on beat
    if (beat >= 7)
    {
		fill_solid(leds_, NUM_LEDS, CRGB::Gray);
        FastLED.setBrightness(120);
        return default_delay;
    } else if (last_beat != beat)
    {
        FastLED.setBrightness( 40+beat*beat*5 );
        last_beat = beat;
	}

	const ledctr_t start_octave = 0;
	const ledctr_t spectr_width = (NUM_OCTAVES-start_octave)*2-1; //8 + 7 in between = 15
	const ledctr_t spectr_repetitions = NUM_LEDS / spectr_width;

	for (ledctr_t repetition=0; repetition<spectr_repetitions; repetition++)
	{
		ledctr_t start_pos = spectr_width*repetition;
		//paint octaves to every second pixel
		for (ledctr_t o=start_octave; o < NUM_OCTAVES; o++)
		{
			hsv2rgb_rainbow(
				CHSV(led_octaves_magnitude[o] + repetition*30,
					constrain(led_octaves_magnitude[o]+30, 0,0xff),
					led_octaves_magnitude[o])
				,leds_[addmod8(start_pos+o*2,led_shift,NUM_LEDS)]
			);
		}

		//interpolate pixels in between
		for (ledctr_t o=start_octave; o < NUM_OCTAVES-1; o++)
		{
			ledctr_t pos_before = addmod8(start_pos+o*2,led_shift,NUM_LEDS);
			ledctr_t pos_between = addmod8(pos_before+1,led_shift,NUM_LEDS);
			ledctr_t pos_after   = addmod8(pos_before+2,led_shift,NUM_LEDS);
			leds_[pos_between].r = (leds_[pos_before].r+leds_[pos_after].r) / 2;
			leds_[pos_between].g = (leds_[pos_before].g+leds_[pos_after].g) / 2;
			leds_[pos_between].b = (leds_[pos_before].b+leds_[pos_after].b) / 2;
		}
	}

	if (beat > 0)
	{
		led_shift+=((beat+4)/2-2);
	}

	if (beat>3 && beat<7)
        return default_delay*2;
    else
		return default_delay;
}

		// ledctr_t middle = oct*ledwith_octave + (ledwith_octave/2);
		// ledctr_t width = fht_oct_out[oct] * ledwith_octave / (1<<(sizeof(int16_t)-1));
		// uint8_t value = fht_oct_out[oct] * ledwith_octave / (1<<(sizeof(int16_t)-1));

millis_t animation_striptest()
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

millis_t animation_photosensor_level()
{
	for (ledctr_t l=0; l<NUM_LEDS; l++)
	{
		leds_[l]=CRGB::Black;
	}
	ledctr_t light_level_mapped_onto_leds = (light_level*NUM_LEDS/4095);
	for (ledctr_t l=0; l<light_level_mapped_onto_leds; l++)
	{
		leds_[l].b=30;
	}
	ledctr_t dark_count_mapped_onto_leds = min(NUM_LEDS, ((dark_count_+LIGHT_DEBOUNCE)*NUM_LEDS/(2*LIGHT_DEBOUNCE)));
	for (ledctr_t l=0; l<dark_count_mapped_onto_leds; l++)
	{
		leds_[l].r=60;
	}
	if (is_dark_)
	{
		leds_[0].g=100;
	}
	return 100;
}


millis_t animation_audio_rms_hue()
{
	static uint8_t hue=0;
	if (!audioRMS.available() || !audioPeak.available())
		return 2;
	float rms = audioRMS.read(); //0.0 ... 1.0
	uint8_t audiopower = static_cast<uint8_t>(rms/last_max_peak_*0xff);
	last_max_peak_ = max(last_max_peak_,audioPeak.read());
	//move pattern forwards
	for (ledctr_t l=NUM_LEDS-1; l>0; l--)
	{
		leds_[l]=leds_[l-1];
	}
	hsv2rgb_rainbow(CHSV(hue,128,audiopower),leds_[0]);
	hue++;
	return 4;
}

millis_t animation_plasma()
{
	static ledctr_t steps=0;
	for (ledctr_t i=0;i<NUM_LEDS;i++)
	{
		uint16_t spani = (0x7f*(uint16_t)i)/(NUM_LEDS-1);
		uint8_t s = quadwave8(add8(steps,mul8(spani,8)));
		uint8_t u = quadwave8((-(steps<<1)-mul8(spani,3)+(quadwave8(spani<<1))));
		leds_[i].r=s;
		leds_[i].g=u;
		leds_[i].b=0;
	}
	steps++;
}

void task_animate_leds()
{
	static millis_t next_run=0;

	//ignore overflow for now, as the only thing that happens that we continue immediately once.
	if (millis() < next_run)
		return;

	millis_t delay_ms=0;
	switch (animation_current_)
	{
		default:
		case ANIM_FFT_SPARKLES_WHEN_DARK:
			if (is_dark_)
				delay_ms=animation_fft_octaves(); //FIXME
			else
				delay_ms=animation_black();
			break;
		case ANIM_RMS:
			delay_ms=animation_audio_rms_hue();
			break;
		case ANIM_FFT:
			delay_ms=animation_fft_hue(); //FIXME
			break;
		case ANIM_SPARKLES:
			delay_ms=animation_fft_octaves(); //FIXME
			break;
		case ANIM_STRIP_TEST:
			delay_ms=animation_striptest(); //FIXME
			break;
		case ANIM_PHOTOSENSOR_TEST:
			delay_ms=animation_photosensor_level(); //FIXME
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
		fill_solid(leds_, NUM_LEDS, CRGB::Black);
		last_max_peak_=0.01;
		FastLED.setBrightness(120);
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
