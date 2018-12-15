#ifndef ANIMATIONS_INCLUDE__H
#define ANIMATIONS_INCLUDE__H

//(c) Bernhard Tittelbach, xro@realraum.at, 2018
//MIT license, except where code from other projects was borrowed and other licenses might apply


#include <vector>

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

typedef uint32_t ledctr_t;
typedef unsigned long millis_t;

bool areAllPixelsBlack(void)
{
	uint32_t pxsum = 0;
	for (ledctr_t l=0; l< NUM_LEDS; l++)
	{
		pxsum += leds_[l].r;
		pxsum += leds_[l].g;
		pxsum += leds_[l].b;
	}
	return (pxsum == 0);
}

class BaseAnimation
{
public:
	virtual void init()
	{
		fill_solid(leds_, NUM_LEDS, CRGB::Black);
		FastLED.setBrightness(80);
	}

	virtual millis_t run()
	{
		return 100;
	}
};

class AnimationBlack : public BaseAnimation {
public:
	virtual millis_t run()
	{
		CPixelView<CRGB>(leds_,0,NUM_LEDS).fadeToBlackBy(20);
		return 200;
	}
};

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
class AnimationBlackSleepESP8266 : public BaseAnimation {
private:
	uint32_t sleep_duration_s_;

public:
	AnimationBlackSleepESP8266(uint32_t sleep_duration_s=10) : sleep_duration_s_(sleep_duration_s) {}

	virtual millis_t run()
	{
		CPixelView<CRGB>(leds_,0,NUM_LEDS).fadeToBlackBy(20);
		if (areAllPixelsBlack()) //fadeout finished
		{
			#ifdef LED_PIN
			digitalWrite(LED_PIN,LOW);
			#endif
			ESP.deepSleep(sleep_duration_s_ * 1000000);
		}
		return 100;
	}
};
#endif

#if defined(TEENSYDUINO) && defined(Snooze_h) && !defined(__AVR_ATmega32U4__)
///
/// // example use:
/// #include <Snooze.h>
/// #include <animations.h> //animations after Snooze
/// SnoozeTimer timer;
/// SnoozeDigital digital;
/// timer.setTimer(5000);// milliseconds
/// digital.pinMode(21, INPUT_PULLUP, RISING);//pin, mode, type
/// digital.pinMode(22, INPUT_PULLUP, RISING);//pin, mode, type
/// SnoozeBlock sleep_config(sleep_timer_,sleep_digital_)
/// AnimationBlackSleepTeensy anim_hibernate(sleep_config)
///
class AnimationBlackSleepTeensy : public BaseAnimation {
private:
	SnoozeBlock sleep_config_;

public:
	AnimationBlackSleepTeensy(SnoozeBlock &sleep_config) : sleep_config_(sleep_config) {}

	virtual millis_t run()
	{
		CPixelView<CRGB>(leds_,0,NUM_LEDS).fadeToBlackBy(20);
		if (areAllPixelsBlack()) //fadeout finished
		{
			#ifdef LED_PIN
			digitalWrite(LED_PIN,LOW);
			#endif
			Snooze.hibernate(sleep_config_);
		}
		return 100;
	}
};
#endif

// onlyInDarkness Decorator
class RunOnlyInDarkness : public BaseAnimation {
private:
	BaseAnimation *dark_animation;
	BaseAnimation *light_animation;
	bool  last_dark=false;
public:
	RunOnlyInDarkness(BaseAnimation &in_darkness, BaseAnimation &in_daylight) :dark_animation(&in_darkness), light_animation(&in_daylight) {}

	virtual void init()
	{
		last_dark = is_dark_;
		if (is_dark_)
			return dark_animation->init();
		else
			return light_animation->init();
	}

	virtual millis_t run()
	{
		if (last_dark != is_dark_)
		{
			this->init();
		}

		if (is_dark_)
			return dark_animation->run();
		else
			return light_animation->run();
	}
};


// AutoSwitch Collection Decorator
class AutoSwitchAnimationCollection : public BaseAnimation {
private:
	std::vector <BaseAnimation*> &autoswitch_list_;
	std::vector <BaseAnimation*>::iterator curanim_;
	millis_t switch_after_ms_=0;
	millis_t next_switch_=0;

public:
	AutoSwitchAnimationCollection(millis_t switch_after_ms, std::vector<BaseAnimation*> &anim_list) : autoswitch_list_(anim_list), curanim_(autoswitch_list_.begin()), switch_after_ms_(switch_after_ms) {}

	virtual void init()
	{
		(*curanim_)->init();
	}

	virtual millis_t run()
	{
		millis_t time=millis();
		if (time > next_switch_)
		{
			curanim_++;
			if (autoswitch_list_.end() == curanim_)
			{
				curanim_ = autoswitch_list_.begin();
			}
			(*curanim_)->init();
			next_switch_ = time+switch_after_ms_;
		}
		return (*curanim_)->run();
	}
};

class AnimationCampingLight : public BaseAnimation {
private:
	static const uint8_t brightness_lower = 150;
	static const uint8_t brightness_upper = 210;
	uint8_t brightness_drift_=0;
	ledctr_t black_pos_=0;
	ledctr_t black_size_=2;
	ledctr_t blend_size_=64; //>= 1
public:
	virtual void init()
	{
		BaseAnimation::init();
		FastLED.setBrightness(blend8(brightness_lower,brightness_upper,quadwave8(brightness_drift_)));
	}

	virtual millis_t run()
	{
		if (0 == black_pos_ % blend_size_*3)
		{
			brightness_drift_++;
			FastLED.setBrightness(blend8(brightness_lower,brightness_upper,quadwave8(brightness_drift_)));
		}

		//all white
		fill_solid(leds_,NUM_LEDS,CRGB::White);

		if (black_size_ > 0)
		{
			//black start
			uint8_t sin_brightness = 0x80/(blend_size_-1) * (black_pos_ % blend_size_);
			uint8_t brightness = quadwave8(sin_brightness);
			leds_[black_pos_ / blend_size_] = CRGB(brightness,brightness,brightness);

			//black inbetween
			for (ledctr_t bl=(black_pos_/blend_size_)+1; bl<min((black_pos_/blend_size_)+black_size_,NUM_LEDS); ++bl)
			{
 				leds_[bl]=CRGB::Black;
			}

			//black end
			if (black_pos_ / blend_size_+black_size_ < NUM_LEDS)
			{
				brightness = quadwave8(sin_brightness+0x7f);
				leds_[black_pos_ / blend_size_+black_size_] = CRGB(brightness,brightness,brightness);
			}
		}

		black_pos_++;
		black_pos_%= NUM_LEDS*blend_size_;

		//start a new random black size
		if (0 == black_pos_)
		{
			black_size_=random8(0,11);
		}

		return 1000/30;
	}
};

class AnimationBreatheLight : public BaseAnimation {
private:
	static const uint8_t brightness_lower = 0;
	static const uint8_t brightness_upper = 210;
	uint16_t brightness_drift_=0;
public:
	virtual void init()
	{
		BaseAnimation::init();
		FastLED.setBrightness(blend8(brightness_lower,brightness_upper,quadwave8(brightness_drift_)));
	}

	virtual millis_t run()
	{
		if (brightness_drift_ < 0x100)
		{
			FastLED.setBrightness(blend8(brightness_lower,brightness_upper,quadwave8(static_cast<uint8_t>(brightness_drift_))));
			brightness_drift_++;
		} else if (brightness_drift_ > 0x300)
		{
			brightness_drift_=0;
		} else
		{
			brightness_drift_++;
		}

		//all white
		fill_solid(leds_,NUM_LEDS,CRGB::White);

		return 1000/80;
	}
};

class AnimationJustMaximumLight : public BaseAnimation {
private:
	uint8_t brightness_drift_=0;
	ledctr_t black_pos_=0;
	ledctr_t black_size_=1;
	ledctr_t blend_size_=4;
public:
	virtual void init()
	{
		BaseAnimation::init();
		FastLED.setBrightness(255);
		fill_solid(leds_,NUM_LEDS,CRGB::White);
	}

	virtual millis_t run()
	{
		return 1000;
	}
};

class AnimationPlasma : public BaseAnimation {
private:
	uint8_t steps_=0;

public:
	virtual void init()
	{
		BaseAnimation::init();
		FastLED.setBrightness(64);
	}

	virtual millis_t run()
	{

		for (uint8_t i=0;i<NUM_LEDS;i++)
		{
			uint16_t spani = (0x7f*(uint16_t)i)/(NUM_LEDS-1);
			uint8_t s = sin8(add8(steps_,mul8(spani,8)));
			uint8_t u = sin8(-(steps_*2)-(spani*3)+sin8(i*2));
			uint8_t v = max(0, 255-s-u); //sin((t+(t/2))+(i*5));
			leds_[i].r=s;
			leds_[i].g=u;
			leds_[i].b=v;
		}
		steps_++;
		return 1000/60;
	}
};

class AnimationPhotosensorDebugging : public BaseAnimation {
public:
	virtual millis_t run()
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
};

class AnimationStripTest : public BaseAnimation {
private:
	ledctr_t whiteLed=0;

public:
	virtual millis_t run()
	{
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
};

#ifdef USE_PJRC_AUDIO
class AnimationRMSHue : public BaseAnimation {
private:
	uint8_t hue=0;

public:
	virtual millis_t run()
	{
		if (!audioRMS.available() || !audioPeak.available())
			return 2;
		float rms = audioRMS.read(); //0.0 ... 1.0
		uint8_t audiopower = static_cast<uint8_t>(rms*0xff);
		//move pattern forwards
		for (ledctr_t l=NUM_LEDS-1; l>0; l--)
		{
			leds_[l]=leds_[l-1];
		}
		hsv2rgb_rainbow(CHSV(hue,128,audiopower),leds_[0]);
		hue++;
		return 10;
	}
};
#endif


#define NUM_OCTAVES  8 //log2(256)

#ifdef USE_PJRC_AUDIO
void fft_calc_octaves255(uint8_t led_octaves_magnitude[NUM_OCTAVES])
{
	const float gain = 1.8;
    led_octaves_magnitude[0] = min(1.0,gain*audioFFT.read(1)) * 0xff;
    led_octaves_magnitude[1] = min(1.0,gain*audioFFT.read(2)) * 0xff;
    led_octaves_magnitude[2] = min(1.0,gain*audioFFT.read(3,  4)) * 0xff;
    led_octaves_magnitude[3] = min(1.0,gain*audioFFT.read(5,  8)) * 0xff;
    led_octaves_magnitude[4] = min(1.0,gain*audioFFT.read(9,  17)) * 0xff;
    led_octaves_magnitude[5] = min(1.0,gain*audioFFT.read(18, 35)) * 0xff;
    led_octaves_magnitude[6] = min(1.0,gain*audioFFT.read(36, 64)) * 0xff;
    led_octaves_magnitude[7] = min(1.0,gain*audioFFT.read(65, 127)) * 0xff;
}
#else
void fft_calc_octaves255(uint8_t led_octaves_magnitude[NUM_OCTAVES])
{
	//TODO
}
#endif

uint8_t get_fft_octaves_beat(uint8_t led_octaves_magnitude[NUM_OCTAVES])
{
	const uint8_t beat_threshold=180;
	uint8_t beat=0;
	for (uint8_t o=1; o<NUM_OCTAVES; o++)
	{
		beat += ((led_octaves_magnitude[o] > beat_threshold)? 1:0);
	}
	//again for last one, adding +2 in sum if beat.
	beat += ((led_octaves_magnitude[NUM_OCTAVES-1] > beat_threshold)? 1:0);
	return beat;
}

// heavily inspired and some calculations and estimations borrowed from buzzandy
// (https://www.hackster.io/buzzandy/music-reactive-led-strip-5645ed)
class AnimationFFTOctaves : public BaseAnimation {
private:
	uint8_t last_beat=0;
	ledctr_t led_shift=0;

public:
	virtual void init()
	{
		BaseAnimation::init();
		FastLED.setBrightness(255);
	}

	virtual millis_t run()
	{
		const millis_t default_delay=10;
#ifdef USE_PJRC_AUDIO
		if (!audioFFT.available() || !audioPeak.available())
			return 2;
#endif
		uint8_t led_octaves_magnitude[NUM_OCTAVES];
		//calc octaves magnitude
		fft_calc_octaves255(led_octaves_magnitude);
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

		const ledctr_t start_octave = 0; //0 is first octave. would be mainly DC offset if not for PJRC correction. set to 1 to ignore first octave, etc..
		const ledctr_t spectr_width = (NUM_OCTAVES-start_octave)*2-1; //e.g. 8 + 7 in between = 15
		const ledctr_t spectr_repetitions = NUM_LEDS / spectr_width;

		//repeat same pattern over whole strip
		for (ledctr_t repetition=0; repetition<spectr_repetitions; repetition++)
		{
			ledctr_t start_pos = spectr_width*repetition;
			//paint octaves to every second LED
			//leaving one LED blank in between.
			for (ledctr_t o=start_octave; o < NUM_OCTAVES; o++)
			{
				hsv2rgb_rainbow(
					CHSV(led_octaves_magnitude[o] + repetition*30,		//H
						blend8(150,0xff,led_octaves_magnitude[o]),	//S
						led_octaves_magnitude[o])						//V
					,leds_[addmod8(start_pos+o*2,led_shift,NUM_LEDS)]
				);
			}

			//interpolate color of LEDs in between (the one's we left free before)
			for (ledctr_t o=start_octave; o < NUM_OCTAVES-1; o++)
			{
				ledctr_t pos_before = addmod8(start_pos+o*2,led_shift,NUM_LEDS);
				ledctr_t pos_between = addmod8(pos_before,1,NUM_LEDS);
				ledctr_t pos_after   = addmod8(pos_before,2,NUM_LEDS);
				leds_[pos_between].r = (static_cast<uint16_t>(leds_[pos_before].r)+static_cast<uint16_t>(leds_[pos_after].r)) / 2;
				leds_[pos_between].g = (static_cast<uint16_t>(leds_[pos_before].g)+static_cast<uint16_t>(leds_[pos_after].g)) / 2;
				leds_[pos_between].b = (static_cast<uint16_t>(leds_[pos_before].b)+static_cast<uint16_t>(leds_[pos_after].b)) / 2;
			}
		}

		if (beat > 0)
		{
			led_shift+=((beat+4)/2-2);
			led_shift%=NUM_LEDS;
		}

		if (beat>3 && beat<7)
	        return default_delay*2;
	    else
			return default_delay;
	}
};

#ifdef USE_PJRC_AUDIO
class AnimationFullFFT : public BaseAnimation {
public:
	virtual void init()
	{
		BaseAnimation::init();
		FastLED.setBrightness(255);
	}

	virtual millis_t run()
	{
		if (!audioFFT.available())
			return 2;
		for (ledctr_t l=0; l<min(FFT_SIZE,NUM_LEDS);l++)
		{
			uint8_t v = static_cast<uint8_t>(audioFFT.read(l)*255.0);
			CHSV x(l*4,255,v);
			hsv2rgb_rainbow(x,leds_[l]);
		}
		return 10; //1ms max delay
	}
};
#endif

class AnimationGravityDots : public BaseAnimation {
private:
	uint8_t static const num_dots=6;
	CRGB dot_color[num_dots];
	int8_t dot_pos[num_dots];
	int8_t dot_speed[num_dots];

	int8_t static const dot_gravity_limit = 3;
	int8_t static const dot_max_speed = 2;
	uint16_t zero_move_ticks_=0;

public:
	virtual void init()
	{
		BaseAnimation::init();
		FastLED.setBrightness(164);
		CHSV dothsv;
		dothsv.v=128;
		dothsv.s=0xFF;
		dothsv.h=random8();
		for (ledctr_t d=0; d<num_dots;d++)
		{
			hsv2rgb_rainbow(dothsv,dot_color[d]);
			dothsv.h+=0xFF/num_dots;
			dot_speed[d]=dot_max_speed-(int8_t)random8(0,dot_max_speed*2);
			dot_pos[d]=random8(0,NUM_LEDS-1);
		}
		zero_move_ticks_=0;
	}

	virtual millis_t run()
	{
		//calc
		for (ledctr_t d1=0; d1<num_dots;d1++)
		{
			for (ledctr_t d2=d1+1; d2<num_dots;d2++)
			{
				int8_t dot_distance = dot_pos[d1] - dot_pos[d2];
				//accel/decel
				if (abs8(dot_distance) < dot_gravity_limit)
				{
					int8_t gravity = dot_gravity_limit-(dot_distance*dot_distance/dot_gravity_limit) + (dot_distance==0)?+1-random8(0,2):0;
					if (dot_distance < 0)
					{
						dot_speed[d1] += gravity;
						dot_speed[d2] -= gravity;
					} else {
						dot_speed[d1] -= gravity;
						dot_speed[d2] += gravity;
					}
				}
			}
		}


		for(int i = 0; i < NUM_LEDS; i++)
		{
			leds_[i].r = 0; leds_[i].g = 0; leds_[i].b = 0;
		}

		ledctr_t zerospeed=0;

		for (ledctr_t d=0; d<num_dots; d++)
		{
			dot_speed[d]=max(min(dot_speed[d],dot_max_speed),0-dot_max_speed);
			dot_pos[d]+=dot_speed[d];
			if (dot_pos[d] < 0)
				dot_pos[d] += NUM_LEDS;
			dot_pos[d]%=NUM_LEDS;

			leds_[dot_pos[d]].r+=dot_color[d].r;
			leds_[dot_pos[d]].g+=dot_color[d].g;
			leds_[dot_pos[d]].b+=dot_color[d].b;

			zerospeed+=(dot_speed[d]==0)?1:0;
		}


		if (zerospeed>0)
		{
			zero_move_ticks_+=zerospeed;
		} else {
			zero_move_ticks_=0;
		}
		if (zero_move_ticks_ > 1024)
		{
			this->init();
		}
		return 1000/12;
	}
};

uint32_t ledGetColorCode(CRGB &l)
{
	return (
		static_cast<uint32_t>(l.raw[0])
		| static_cast<uint32_t>(l.raw[1]) << 8
		| static_cast<uint32_t>(l.raw[2]) << 16
			);
}

class AnimationFireworks : public BaseAnimation {
public:
	virtual millis_t run()
	{
		uint8_t color = random8();
		uint32_t prevLed, thisLed, nextLed;
		bool triggered = random(30) == 3;

		fadeToBlackBy(leds_,NUM_LEDS,127); // reduce each LEDs brightness by half

		// set brightness(i) = ((brightness(i-1)/4 + brightness(i+1)) / 4) + brightness(i)
		for (ledctr_t i=0 + 1; i <NUM_LEDS; i++)
		{
			prevLed = (ledGetColorCode(leds_[i-1]) >> 2) & 0x3F3F3F3F;
			thisLed = ledGetColorCode(leds_[i]);
			nextLed = (ledGetColorCode(leds_[i+1]) >> 2) & 0x3F3F3F3F;
			leds_[i] = CRGB(prevLed + thisLed + nextLed);
		}

		if(!triggered)
		{
			for(ledctr_t i=0; i<max(1, NUM_LEDS/20); i++)
			{
				if(random(10) == 0)
				{
					hsv2rgb_rainbow(CHSV(color,0xff,0xff),leds_[0 + random(NUM_LEDS)]);
				}
			}
		} else
		{
			for(ledctr_t i=0; i<max(1, NUM_LEDS/10); i++) {
				hsv2rgb_rainbow(CHSV(color,200,0xff),leds_[0 + random(NUM_LEDS)]);
			}
		}
		return 1000/20;
	}
};

//(c) FastLED
class AnimationFire2012 : public BaseAnimation {
public:
// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
////
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation,
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking.
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100
#define COOLING  60

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 50


	virtual void init()
	{
		BaseAnimation::init();
		FastLED.setBrightness(32);
	}

	virtual millis_t run()
	{
	// Array of temperature readings at each simulation cell
	  static byte heat[NUM_LEDS];

	  // Step 1.  Cool down every cell a little
	    for( int i = 0; i < NUM_LEDS; i++) {
	      heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
	    }

	    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
	    for( int k= NUM_LEDS - 1; k >= 2; k--) {
	      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
	    }

	    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
	    if( random8() < SPARKING ) {
	    	int y = random8(7);
	    	heat[y] = qadd8( heat[y], random8(160,255) );
	    }

	    // Step 4.  Map from heat cells to LED colors
	    for( int j = 0; j < NUM_LEDS; j++) {
	    	CRGB color = HeatColor( heat[j]);
	    	int pixelnumber;
	    	pixelnumber = j;
	    	leds_[pixelnumber] = color;
	    }
	    return 1000/10;
	}
};

//(c) FastLED
class AnimationConfetti : public BaseAnimation {
private:
	uint8_t cur_hue_ = 0;
	uint8_t ctr_ = 0;

public:

	virtual millis_t run()
	{
		fadeToBlackBy( leds_, NUM_LEDS, 10);
		int pos = random16(NUM_LEDS);
		leds_[pos] += CHSV( cur_hue_ + random8(64), 200, 255);
		if (ctr_++ % 8 == 0)
			cur_hue_++;
		return 1000/60;
	}
};

#ifdef USE_PJRC_AUDIO
//(c) FastLED
class AnimationRMSConfetti : public BaseAnimation {
private:
	uint8_t cur_hue_ = 0;
	uint8_t ctr_ = 0;
	float threshold_;

public:

	AnimationRMSConfetti(float threshold=0.05) : threshold_(threshold) {}

	virtual millis_t run()
	{
		fadeToBlackBy( leds_, NUM_LEDS, 10);
		if (audioPeak.available())
		{
			float peak = audioPeak.read();
			if (peak > threshold_)
			{
				//one for sure
				int pos = random16(NUM_LEDS);
				leds_[pos] += CHSV( cur_hue_ + static_cast<uint8_t>(peak*128), 200, 255);
				//maybe more
				for (uint8_t p=0; p<static_cast<uint8_t>(peak*4); p++)
				{
					pos = random16(NUM_LEDS);
					leds_[pos] += CHSV( cur_hue_ + static_cast<uint8_t>(peak*128), 200, 255);
				}
			}
		}
		if (ctr_++ % 4 == 0)
			cur_hue_++;
		return 1000/60;
	}
};
#endif

class AnimationRainbowGlitter : public BaseAnimation {
private:
	bool with_glitter_ = false;
	uint8_t cur_hue_ = 0;

	void rainbow()
	{
		// FastLED's built-in rainbow generator
		fill_rainbow( leds_, NUM_LEDS, cur_hue_, 7);
	}

	void addGlitter( uint8_t chanceOfGlitter)
	{
		if (random8() < chanceOfGlitter)
		{
			leds_[ random16(NUM_LEDS) ] = CRGB::White;
		}
	}

public:
	AnimationRainbowGlitter(bool with_glitter=true) : with_glitter_(with_glitter), cur_hue_(0) {}

	virtual void init()
	{
		BaseAnimation::init();
		if (with_glitter_)
			FastLED.setBrightness(32);
		else
			FastLED.setBrightness(64);
	}

	virtual millis_t run()
	{
		rainbow();
		cur_hue_++;
		if (with_glitter_)
		{
			addGlitter(80);
		}
		return 1000/50;
	}
};


class AnimationTOCFairyDustFire : public BaseAnimation {
private:
	uint8_t led_ring_rings = 6;
	const ledctr_t led_ring_sizes[9] = {1,8,12,16,24,32,48,60};
	ledctr_t last_led = 241;
	uint8_t fairydust_sparking = 60;
	uint8_t centric_heatwave_phase = 0;
  uint32_t mainheatplume_step = 0;

public:
  AnimationTOCFairyDustFire(uint8_t num_rings=6):led_ring_rings(num_rings)
  {
    last_led=0;
    for (uint8_t c=0; c<num_rings; c++)
    {
      last_led+=led_ring_sizes[c];
    }
    last_led-=1;
  }
  
	virtual void init()
	{
		BaseAnimation::init();
		FastLED.setBrightness(64);
		//assert(NUM_LEDS == last_led);
	}

	virtual millis_t run()
	{
		//decrease heat with each ring (low freq sin)
		//have heatwave propage from outside to insdie (high freq sin)
		//vary heat inside each ring with sinus
		//have inside-ring-sinus drift with phase
		const float tau=2*3.1416;

		// float mainheatplume_amplitude = 8;  // /¯\________/¯\________
		// float mainheatplume_width = 0;      //  __/¯\________/¯\______
		const float mainheatplume_amplitude_min = 8;
		const float mainheatplume_width_min = 0.2;
		const float mainheatplume_amplitude_max = 32;
		const float mainheatplume_width_max = 2.0;
		const float mainheatplume_amplitude_phase = 0.0;
		const float mainheatplume_width_phase = tau/2;
		const uint32_t mainheatplume_substeps=100;
		const float mainheatplume_step_size = tau/static_cast<float>(mainheatplume_substeps);
		const uint32_t mainheatplume_step_max = 5*mainheatplume_substeps;
		const float mainheatplume_amplitude_t_max = tau/2+mainheatplume_amplitude_phase;
		const float mainheatplume_width_t_max = tau/2+mainheatplume_width_phase;

		float t = mainheatplume_step_size * mainheatplume_step;
		mainheatplume_step++;
		mainheatplume_step%=mainheatplume_step_max;

		float mainheatplume_amplitude = mainheatplume_amplitude_min + (mainheatplume_amplitude_max-mainheatplume_amplitude_min)*max(0.0,sin(min(mainheatplume_amplitude_t_max, t) + mainheatplume_amplitude_phase));
		float mainheatplume_width = mainheatplume_width_min + (mainheatplume_width_max-mainheatplume_width_min)*max(0.0,sin(min(mainheatplume_width_t_max, t)+ mainheatplume_width_phase));

		const uint16_t overlay_freq_multiplier = 5;
		const uint16_t overlay_amplitude_divider = 8;

		centric_heatwave_phase++;

		ledctr_t first_led_in_rings=0;
		uint8_t spiral_heat_phase = 0;
		for (uint8_t c=0;c<led_ring_rings; c++)
		{
			float ring_base_heat_float = mainheatplume_amplitude * sin(tau/4 + ((tau/4/mainheatplume_width)/led_ring_rings)*c);
			uint8_t ring_base_heat = (uint8_t)(64.0 + ring_base_heat_float);
			//uint8_t ring_base_heat = (uint16_t)quadwave8(64 + (64-mainheatplume_width)/led_ring_rings*c)*mainheatplume_amplitude/16;
      //uint8_t ring_base_heat = qadd8(qsub8((uint16_t)quadwave8(64 + (64-mainheatplume_width)/led_ring_rings*c)*mainheatplume_amplitude/16,256/overlay_amplitude_divider/2),quadwave8(64 + 64/led_ring_rings*c*overlay_freq_multiplier + centric_heatwave_phase)/overlay_amplitude_divider);

			for (ledctr_t l=0; l < led_ring_sizes[c]; l++)
			{
				uint8_t heat = ring_base_heat;
			//	uint8_t heat = qsub8(ring_base_heat,256/overlay_amplitude_divider/2) + quadwave8(64 + 256/led_ring_sizes[c]*l + spiral_heat_phase) / overlay_amplitude_divider;

			//    if( random8() < fairydust_sparking )
			//    {
			//		  heat = qadd8( heat, random8(160,255) );
			//    }

				leds_[last_led - (first_led_in_rings + l)] = HeatColor( heat );
		  }
			//leds_[ last_led - first_led_in_rings ] = HeatColor( ring_base_heat   );
			first_led_in_rings += led_ring_sizes[c];
			spiral_heat_phase++;
		}
		return 1000/20;
	}
};


#endif //ANIMATIONS_INCLUDE__H

