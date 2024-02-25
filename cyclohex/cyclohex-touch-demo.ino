/*
   CycloHex Touch Demo: https://github.com/jasoncoon/cyclohex-touch-demo
   Copyright (C) 2022 Jason Coon

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <FastLED.h>  // https://github.com/FastLED/FastLED
#include <Button.h>   // https://github.com/madleech/Button
#include "Adafruit_FreeTouch.h" //https://github.com/adafruit/Adafruit_FreeTouch
#include "GradientPalettes.h"

FASTLED_USING_NAMESPACE

#define DATA_PIN      A10
#define LED_TYPE      WS2812B
#define COLOR_ORDER   GRB
#define NUM_LEDS      228

#include "Map.h"

#define MILLI_AMPS         1400 // IMPORTANT: set the max milli-Amps of your power supply (4A = 4000mA)
#define FRAMES_PER_SECOND  240

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

CRGB leds[NUM_LEDS];

uint8_t brightnesses[] = { 128, 96, 64, 32, 16 };
uint8_t currentBrightnessIndex = 1;

Adafruit_FreeTouch touch0 = Adafruit_FreeTouch(A0, OVERSAMPLE_4, RESISTOR_0, FREQ_MODE_NONE);
Adafruit_FreeTouch touch1 = Adafruit_FreeTouch(A1, OVERSAMPLE_4, RESISTOR_0, FREQ_MODE_NONE);
Adafruit_FreeTouch touch2 = Adafruit_FreeTouch(A2, OVERSAMPLE_4, RESISTOR_0, FREQ_MODE_NONE);
Adafruit_FreeTouch touch3 = Adafruit_FreeTouch(A3, OVERSAMPLE_4, RESISTOR_0, FREQ_MODE_NONE);
Adafruit_FreeTouch touch4 = Adafruit_FreeTouch(A6, OVERSAMPLE_4, RESISTOR_0, FREQ_MODE_NONE);
Adafruit_FreeTouch touch5 = Adafruit_FreeTouch(A7, OVERSAMPLE_4, RESISTOR_0, FREQ_MODE_NONE);

Button button0(A8);
Button button1(A9);

#define touchPointCount 6

// These values were discovered using the commented-out Serial.print statements in handleTouch below

// minimum values for each touch pad, used to filter out noise
uint16_t touchMin[touchPointCount] = { 834, 575, 725, 650, 601, 527 };

// maximum values for each touch pad, used to determine when a pad is touched
uint16_t touchMax[touchPointCount] = { 1016, 1016, 1016, 1016, 1016, 1016 };

// raw capacitive touch sensor readings
uint16_t touchRaw[touchPointCount] = { 0, 0, 0, 0, 0, 0 };

// capacitive touch sensor readings, mapped/scaled one one byte each (0-255)
uint8_t touch[touchPointCount] = { 0, 0, 0, 0, 0, 0 };

// coordinates of the touch points
uint8_t touchPointX[touchPointCount] = { 181, 74, 21, 74, 181, 234 };
uint8_t touchPointY[touchPointCount] = { 24, 24, 128, 231, 231, 128 };

boolean activeWaves = false;

// Forward declarations of an array of cpt-city gradient palettes, and
// a count of how many there are.
extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];

uint8_t gCurrentPaletteNumber = 0;

CRGBPalette16 gCurrentPalette(gGradientPalettes[0]);
CRGBPalette16 gTargetPalette( gGradientPalettes[0] );

// ten seconds per color palette makes a good demo
// 20-120 is better for deployment
uint8_t secondsPerPalette = 10;

uint8_t mode = 0;
const uint8_t modeCount = 2;

void setup() {
  Serial.begin(115200);
  //  delay(3000);

  button0.begin();
  button1.begin();

  if (!touch0.begin())
    Serial.println("Failed to begin qt on pin A0");
  if (!touch1.begin())
    Serial.println("Failed to begin qt on pin A1");
  if (!touch2.begin())
    Serial.println("Failed to begin qt on pin A2");
  if (!touch3.begin())
    Serial.println("Failed to begin qt on pin A3");
  if (!touch4.begin())
    Serial.println("Failed to begin qt on pin A6");
  if (!touch5.begin())
    Serial.println("Failed to begin qt on pin A7");

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setDither(true);
  FastLED.setCorrection(TypicalSMD5050);
  FastLED.setBrightness(brightnesses[currentBrightnessIndex]);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MILLI_AMPS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  FastLED.setBrightness(brightnesses[currentBrightnessIndex]);
}

void loop() {
  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy(random());

  handleTouch();

  if (button0.pressed()) {
    Serial.println("button 0 pressed");
    if (mode < modeCount - 1) mode++;
    else mode = 0;
    Serial.print("mode: ");
    Serial.println(mode);
  }

  if (button1.pressed()) {
    Serial.println("button 1 pressed");
    currentBrightnessIndex = (currentBrightnessIndex + 1) % ARRAY_SIZE(brightnesses);
    FastLED.setBrightness(brightnesses[currentBrightnessIndex]);
    Serial.print("brightness: ");
    Serial.println(brightnesses[currentBrightnessIndex]);
  }

  // change to a new cpt-city gradient palette
  EVERY_N_SECONDS( secondsPerPalette ) {
    gCurrentPaletteNumber = addmod8( gCurrentPaletteNumber, 1, gGradientPaletteCount);
    gTargetPalette = gGradientPalettes[ gCurrentPaletteNumber ];
  }

  EVERY_N_MILLISECONDS(40) {
    // slowly blend the current palette to the next
    nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 8);
  }

  if (mode == 0) {
    if (!activeWaves)
      colorWaves();
    touchWaves();
  } else if (mode == 1) {
    touchSnakes();
    drawSnakes();
  }

  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

bool touchChanged = true;

void handleTouch() {
  for (uint8_t i = 0; i < touchPointCount; i++) {
    if (i == 0) touchRaw[i] = touch0.measure();
    else if (i == 1) touchRaw[i] = touch1.measure();
    else if (i == 2) touchRaw[i] = touch2.measure();
    else if (i == 3) touchRaw[i] = touch3.measure();
    else if (i == 4) touchRaw[i] = touch4.measure();
    else if (i == 5) touchRaw[i] = touch5.measure();

    // // uncomment to display raw touch values in the serial monitor/plotter
    // Serial.print(touchRaw[i]);
    // Serial.print(" ");

    if (touchRaw[i] < touchMin[i]) {
      touchMin[i] = touchRaw[i];
      touchChanged = true;
    }

    if (touchRaw[i] > touchMax[i]) {
      touchMax[i] = touchRaw[i];
      touchChanged = true;
    }

    touch[i] = map(touchRaw[i], touchMin[i], touchMax[i], 0, 255);

    // // uncomment to display mapped/scaled touch values in the serial monitor/plotter
    // Serial.print(touch[i]);
    // Serial.print(" ");
  }

  // // uncomment to display raw and/or mapped/scaled touch values in the serial monitor/plotter
  // Serial.println();

  // uncomment to display raw, scaled, min, max touch values in the serial monitor/plotter
  if (touchChanged) {
    for (uint8_t i = 0; i < touchPointCount; i++) {
    //  Serial.print(touchRaw[i]);
    //  Serial.print(" ");
    //  Serial.print(touch[i]);
    //  Serial.print(" ");
      Serial.print(touchMin[i]);
      Serial.print(" ");
    // Serial.print(touchMax[i]);
    // Serial.print(" ");
    }

    Serial.println();

    touchChanged = false;
  }
}

// adds a color to a pixel given it's XY coordinates and a "thickness" of the logical pixel
// since we're using a sparse logical grid for mapping, there isn't an LED at every XY coordinate
// thickness adds a little "fuzziness"
void addColorXY(int x, int y, CRGB color, uint8_t thickness = 0)
{
  // ignore coordinates outside of our one byte map range
  if (x < 0 || x > 255 || y < 0 || y > 255) return;

  // loop through all of the LEDs
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    // get the XY coordinates of the current LED
    uint8_t ix = coordsX[i];
    uint8_t iy = coordsY[i];

    // are the current LED's coordinates within the square centered
    // at X,Y, with width and height of thickness?
    if (ix >= x - thickness && ix <= x + thickness &&
        iy >= y - thickness && iy <= y + thickness) {

      // add to the color instead of just setting it
      // so that colors blend
      // FastLED automatically prevents overflowing over 255
      leds[i] += color;
    }
  }
}

// algorithm from http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
void drawCircle(int x0, int y0, int radius, const CRGB color, uint8_t thickness = 0)
{
  int a = radius, b = 0;
  int radiusError = 1 - a;

  if (radius == 0) {
    addColorXY(x0, y0, color, thickness);
    return;
  }

  while (a >= b)
  {
    addColorXY(a + x0, b + y0, color, thickness);
    addColorXY(b + x0, a + y0, color, thickness);
    addColorXY(-a + x0, b + y0, color, thickness);
    addColorXY(-b + x0, a + y0, color, thickness);
    addColorXY(-a + x0, -b + y0, color, thickness);
    addColorXY(-b + x0, -a + y0, color, thickness);
    addColorXY(a + x0, -b + y0, color, thickness);
    addColorXY(b + x0, -a + y0, color, thickness);

    b++;
    if (radiusError < 0)
      radiusError += 2 * b + 1;
    else
    {
      a--;
      radiusError += 2 * (b - a + 1);
    }
  }
}

const uint8_t waveCount = 8;

// track the XY coordinates and radius of each wave
uint16_t radii[waveCount];
uint8_t waveX[waveCount];
uint8_t waveY[waveCount];
CRGB waveColor[waveCount];

const uint16_t maxRadius = 512;

void touchWaves() {
  // fade all of the LEDs a small amount each frame
  // increasing this number makes the waves fade faster
  fadeToBlackBy(leds, NUM_LEDS, 30);

  for (uint8_t i = 0; i < touchPointCount; i++) {
    // start new waves when there's a new touch
    if (touch[i] > 127 && radii[i] == 0) {
      radii[i] = 32;
      waveX[i] = touchPointX[i];
      waveY[i] = touchPointY[i];
      waveColor[i] = CHSV(random8(), 255, 255);
    }
  }

  activeWaves = false;

  for (uint8_t i = 0; i < waveCount; i++)
  {
    // increment radii if it's already been set in motion
    if (radii[i] > 0 && radii[i] < maxRadius) radii[i] = radii[i] + 8;

    // reset waves already at max
    if (radii[i] >= maxRadius) {
      activeWaves = true;
      radii[i] = 0;
    }

    if (radii[i] == 0)
      continue;

    activeWaves = true;

    CRGB color = waveColor[i];

    uint8_t x = waveX[i];
    uint8_t y = waveY[i];

    // draw waves starting from the corner closest to each touch sensor
    drawCircle(x, y, radii[i], color, 4);
  }
}

// ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void fillWithColorWaves(CRGB* ledarray, uint16_t numleds, CRGBPalette16& palette) {
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  // uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 300, 1500);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < numleds; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if ( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16 += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    uint8_t index = hue8;
    //index = triwave8( index);
    index = scale8( index, 240);

    CRGB newcolor = ColorFromPalette( palette, index, bri8);

    uint16_t pixelnumber = i;

    pixelnumber = (numleds - 1) - pixelnumber;

    nblend(ledarray[pixelnumber], newcolor, 128);
  }
}

void colorWaves() {
  fillWithColorWaves(leds, NUM_LEDS, gCurrentPalette);
}

const uint8_t snakeCount = 6;
bool snakeVisible[snakeCount] = { false, true, false, false, false, true };
uint8_t snakeDebounce[snakeCount] = { 0, 0, 0, 0, 0, 0 };
uint8_t currentRings[snakeCount] = { 17, 15, 13, 11, 9, 7 };
const uint8_t defaultRings[snakeCount] = { 17, 15, 13, 11, 9, 7 };
uint8_t snakeHues[snakeCount] = { 192, 160, 128, 96, 64, 0 };

void touchSnakes() {
  for (uint8_t i = 0; i < touchPointCount; i++) {
    if (snakeDebounce[i] > 0) snakeDebounce[i] = snakeDebounce[i] - 1;

    if (touch[i] > 127 && snakeDebounce[i] == 0) {
      snakeVisible[i] = !snakeVisible[i];
      currentRings[i] = defaultRings[i];
      snakeDebounce[i] = 64;
    }
  }
}

void drawSnakes()
{
  static int8_t directions[snakeCount] = { -1, 1, -1, 1, -1, 1 };
  static int8_t currentRingPixels[snakeCount] = { 0, 0, 0, 0, 0, 0 };
  static uint32_t lastJumpMilliss[snakeCount] = { 0, 0, 0, 0, 0, 0 };

  fadeToBlackBy(leds, NUM_LEDS, 4);

  bool move = false;

  EVERY_N_MILLIS(30) { move = true; }

  for (uint8_t snakeIndex = 0; snakeIndex < snakeCount; snakeIndex++)
  {
    if (!snakeVisible[snakeIndex]) continue;

    uint8_t direction = directions[snakeIndex];
    uint8_t currentRing = currentRings[snakeIndex];
    int8_t currentRingPixel = currentRingPixels[snakeIndex];
    uint32_t lastJumpMillis = lastJumpMilliss[snakeIndex];

    uint8_t currentIndex = currentRing * 12 + currentRingPixel;

    if (direction == 0) direction = 1;

    if (move) {
      bool jumped = false;

      if (random8() > 128 && millis() - lastJumpMillis > 250)
      { // jump occasionally, but not too often
        // find jump point
        uint8_t newIndex = NUM_LEDS;

        for (uint8_t i = 0; i < 42; i++)
        {
          if (connections[i][0] == currentIndex)
          {
            newIndex = connections[i][1];
            jumped = true;
            break;
          }
          else if (connections[i][1] == currentIndex)
          {
            newIndex = connections[i][0];
            jumped = true;
            break;
          }
        }

        if (jumped)
        {
          currentRing = newIndex / 12;
          currentRingPixel = newIndex % 12;
          lastJumpMillis = millis();
          direction *= -1; // flip direction every jump
        }
      }

      if (!jumped)
      {
        currentRingPixel += direction;
        if (currentRingPixel > 11)
          currentRingPixel = 0;
        if (currentRingPixel < 0)
          currentRingPixel = 11;
      }
      
      currentIndex = currentRing * 12 + currentRingPixel;
    }

    // leds[currentIndex] += ColorFromPalette(gCurrentPalette, beat8(speed));
    leds[currentIndex] += CHSV(snakeHues[snakeIndex], 255, 255);

    directions[snakeIndex] = direction;
    currentRings[snakeIndex] = currentRing;
    currentRingPixels[snakeIndex] = currentRingPixel;
    lastJumpMilliss[snakeIndex] = lastJumpMillis;
  }
}
