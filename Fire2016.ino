// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY

#include <FastLED.h>
#include <TimerOne.h>
#include "ClickEncoder.h"

#define LED_PIN     11
#define COLOR_ORDER GRB
#define CHIPSET     WS2812B
#define NUM_LEDS    60

#define BRIGHTNESS  16
#define FRAMES_PER_SECOND 60
#define MEASURE_FRAMES true

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

// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 120

// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100
#define COOLING  55

// knob modes
#define MODE_STOPPED 0
#define MODE_BRIGHTNESS 1
#define MODE_SPARKING 2
#define MODE_COOLING 3

// menu timeout
#define HIGHLIGHT_DURATION 700

bool gReverseDirection = false;

CRGB leds[NUM_LEDS];

ClickEncoder *encoder;

byte mode = MODE_BRIGHTNESS;

int brightness = BRIGHTNESS;
int sparking = SPARKING;
int cooling = COOLING;

long highlight = 0;
long msec = 0;
int cnt = 0;

void timerIsr() {
  encoder->service();
}

void setup() {
  Serial.begin(115000);
  encoder = new ClickEncoder(2, 3, 4, 4); // ,4 for single steps

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);

  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
}

void loop()
{
  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy(random());

  // menu handler
  if (highlight > 0) {
    if (millis() - highlight > HIGHLIGHT_DURATION) {
      highlight = 0;
    }
  }
  else {
    if (mode != MODE_STOPPED) {
      Fire2012(); // run simulation frame
    }
  }

  Knob(); // change settings

  FastLED.show(); // display this frame

#ifdef MEASURE_FRAMES
  if (msec != 0 && cnt++ % 500 == 0) {
    Serial.print("fps: ");
    Serial.println(1000 / (millis() - msec));
  }
  msec = millis();
#endif

  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void setLeds(CRGB c, int use = NUM_LEDS, int spare = 0)
{
  // set use# leds
  for (int j = spare; j < use; j++) {
    leds[j] = c;
  }
  // blank remaining leds
  for (int j = use; j < NUM_LEDS; j++) {
    leds[j] = CRGB::Black;
  }
}

int inputHandler(int delta, int current, int minVal, int maxVal, CRGB color) 
{
  int step = (maxVal - minVal) * delta / NUM_LEDS;
  current += step;
/*
  if (delta > 0)
    current += step;
  else
    current -= step;
*/
  current = constrain(current, minVal, maxVal);

  setLeds(color, map(current, minVal, maxVal, 0, NUM_LEDS));
  //FastLED.show(); // display this frame
  
  return current;
}

void Knob()
{
  // knob turning
  int value = encoder->getValue();
  if (value != 0) {
    Serial.print("Encoder Value: ");
    Serial.println(value);
    // indicate settings changed
    highlight = millis();

    switch (mode) {
      case MODE_BRIGHTNESS:
        //brightness = constrain(brightness + value, 0, 255);
        brightness = inputHandler(value, brightness, 0, 255, CRGB::Yellow);
        FastLED.setBrightness(brightness);
        Serial.print("brightness: ");
        Serial.println(brightness);
        break;
      case MODE_SPARKING:
        //sparking = constrain(sparking + value, 50, 200);
        sparking = inputHandler(value, sparking, 50, 200, CRGB::Red);
        Serial.print("sparking: ");
        Serial.println(sparking);
        break;
      case MODE_COOLING:
        //cooling = constrain(cooling + value, 20, 100);
        cooling = inputHandler(value, cooling, 20, 100, CRGB::Blue);
        Serial.print("cooling: ");
        Serial.println(cooling);
        break;
    }
  }

  // knob pressing
  ClickEncoder::Button b = encoder->getButton();
  if (b != ClickEncoder::Open) {
    // ignore button if switched off
    if (mode == MODE_STOPPED && b != ClickEncoder::Held)  {
      return;
    }
      
    // indicate settings changed
    highlight = millis();

    switch (b) {
      case ClickEncoder::Held:
        Serial.println("ClickEncoder::Held");
        if (mode == MODE_STOPPED) {
          // set default mode after wakeup
          mode = MODE_BRIGHTNESS;
          setLeds(CRGB::White);
        }
        else {
          // turn display off
          mode = MODE_STOPPED;
          setLeds(CRGB::Black);
        }
        
        // wait for button release to ensure single events
        do {
          FastLED.delay(10);
          setLeds(CRGB::Black);
        } while (encoder->getButton() != ClickEncoder::Released);
        Serial.println("ClickEncoder::Released");
        
        Serial.print("mode: ");
        Serial.println(mode);
        break;
        
      case ClickEncoder::Clicked: {
        Serial.println("ClickEncoder::Clicked");
        mode = mode % 3 + 1;
        // indicate mode
        CRGB color = CRGB::Yellow;
        if (mode == MODE_SPARKING) {
          color = CRGB::Red;
        } else if (mode == MODE_COOLING) {
          color = CRGB::Blue;
        }
        setLeds(color);
        Serial.print("mode: ");
        Serial.println(mode);
        break;
      }
      
      case ClickEncoder::DoubleClicked:
        Serial.println("ClickEncoder::DoubleClicked");
        // default values
        mode = MODE_BRIGHTNESS;

        brightness = BRIGHTNESS;
        FastLED.setBrightness(brightness);

        sparking = SPARKING;
        cooling = COOLING;
        setLeds(CRGB::Black);
        break;
    }
  }
}

void Fire2012()
{
  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
  for (int i = 0; i < NUM_LEDS; i++) {
    heat[i] = qsub8(heat[i],  random8(0, ((cooling * 10) / NUM_LEDS) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for (int k= NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if (random8() < sparking) {
    int y = random8(7);
    heat[y] = qadd8(heat[y], random8(160, 255));
  }

  // Step 4.  Map from heat cells to LED colors
  for (int j = 0; j < NUM_LEDS; j++) {
    CRGB color = HeatColor(heat[j]);
    int pixelnumber;
    if (gReverseDirection) {
      pixelnumber = (NUM_LEDS-1) - j;
    } else {
      pixelnumber = j;
    }
    leds[pixelnumber] = color;
  }
}

