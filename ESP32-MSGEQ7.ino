/*\
 *

  ESP32-MSGEQ7 is placed under the MIT license

  Copyleft (c+) 2020 tobozo

  Permission is hereby granted, free of charge, to any person obtaining a copy
  ociated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

  This M5Stack/MSGEQ7 Audio spectrum Visualizer is an adaptation of jollyFactory's
  example code provided with the MSGEQ7 Audio spectrum Visualizer Kit:

    - https://www.tindie.com/products/nick64/jf-audio-spectrum-visualizer-board-basic-kit/

  It has been adapted to work with ESP32 instead of Arduino Nano.

  Project resources:
    - http://www.instructables.com/id/Arduino-Based-Mini-Audio-Spectrum-Visualizer/
    - https://github.com/debsahu/ESP32_FFT_Audio_LEDs
    - https://tronixstuff.files.wordpress.com/2013/01/msgeq7-data-sheet.pdf

 *
\*/

#include <ESP32-Chimera-Core.h> // https://github.com/tobozo/ESP32-Chimera-Core
#include <M5StackUpdater.h> // https://github.com/tobozo/M5Stack-SD-Updater
#define tft M5.Lcd

/*\
 * MSGEQ7 Pinout
\*/
#if defined( ARDUINO_M5Stack_Core_ESP32 ) || defined( ARDUINO_M5STACK_FIRE )
  // Pinout for M5Stack reuses Speaker pins for Strobe.
  // The speaker will buzz unlesse the cables are cut.
  // Pick another output pin if you can't afford that.
  int STROBE_PIN = 25; // strobe pin : digital OUTPUT
  int RES_PIN    = 26; // reset pin  : digital OUTPUT
  int LBAND_PIN  = 35; // left band pin  : analog INPUT
  int RBAND_PIN  = 36; // right band pin : analog INPUT
#else
  // pinout for ESP32-WROVER-KIT reuses camera pins
  int STROBE_PIN = 26; // strobe pin : digital OUTPUT
  int RES_PIN    = 27; // reset pin  : digital OUTPUT
  int LBAND_PIN  = 34; // left band pin  : analog INPUT
  int RBAND_PIN  = 35; // right band pin : analog INPUT
#endif

TFT_eSprite sprite = TFT_eSprite( &tft );

RGBColor RGBStart = { 255,0,0 };
RGBColor RGBEnd   = { 0,255,0 };
RGBColor tc;

enum EqEffects {
  EFFECT_HISTOGRAM_CUSTOM_RAIN,
  EFFECT_HISTOGRAM,
  EFFECT_MIRROR
};

enum Channel {
  CHANNEL_LEFT,
  CHANNEL_RIGHT,
  FALLING_LEFT,
  FALLING_RIGHT,
  VOLUME_LEFT,
  VOLUME_RIGHT
};

int EQVal;     // store translated eq band value
int EQValFall; // store peak values

// fps counter
unsigned long framesCount = 0;
uint32_t fstart = millis();
int fpsInterval = 100;
float fpsscale = 1000.0/fpsInterval; // fpi to fps
int fps = 0;   // frames per second
float fpi = 0; // frames per interval
bool showFPS = true;

// adjust this according to bit depth of analogRead
int min_sample = 0;
int max_sample = 4096;

// bigger dimensions = slower rendering
int spriteWidth = 200;
int spriteHeight = 80;
// calculated during setup
int spritePosX = 0;
int spritePosY = 0;

// pre calculate some values for positioning
int halfWidth   = spriteWidth/2;
int bandSpace  = halfWidth / 7;
int band_margin = 4; // multiple of 2
int halfBandMargin = band_margin / 2;
int bandWidth  = bandSpace - band_margin;
int paddingTop = 8;
int volumePosY =  spriteHeight-paddingTop;
int posX = 0; // temp placeholder

// for Rain Effect
int falling_left[7];
int falling_right[7];
int fallingBandHeight = spriteHeight/32;
int falling_speed = 16;

// Channels and Bands
int left[7];    // store left audio band values in these arrays
int right[7];   // store right audio band values in these arrays
int avgleft, avgright;
int band;
int noiseLevel = 127;  // change this value to suppress display due to noise pickup

// used for later v/h map()ping
int bandMinX = 0;
int bandMaxX = spriteHeight - paddingTop;

// histogram effects browser
int numEffects = sizeof(enum EqEffects);
int currentEffect = EFFECT_HISTOGRAM_CUSTOM_RAIN;


/*\
 * gfx helpers
\*/

void fillGradientVRect( uint16_t x, uint16_t y, uint16_t width, uint16_t height, RGBColor RGBStart, RGBColor RGBEnd ) {
  for( uint16_t w = 0; w < width; w++ ) {
    sprite.drawGradientVLine( x+w, y, height, RGBStart, RGBEnd );
  }
}

void fillGradientHRect( uint16_t x, uint16_t y, uint16_t width, uint16_t height, RGBColor RGBStart, RGBColor RGBEnd ) {
  for( uint16_t h = 0; h < height; h++ ) {
    sprite.drawGradientHLine( x, y+h, width, RGBStart, RGBEnd );
  }
}

uint16_t color565( RGBColor color ) {
  return tft.color565( color.r, color.g, color.b );
}


RGBColor colorAt( int32_t start, int32_t end, int32_t pos, RGBColor RGBStart, RGBColor RGBEnd ) {
  if( pos == end ) return RGBEnd;
  if( pos == start ) return RGBStart;
  uint8_t r,g,b;
  r = map( pos, start, end, RGBStart.r, RGBEnd.r );
  g = map( pos, start, end, RGBStart.g, RGBEnd.g );
  b = map( pos, start, end, RGBStart.b, RGBEnd.b );
  return {r,g,b};
}


/*\
 * MSGEQ7 driver
\*/

void initMSGEQ7() {
  pinMode( RES_PIN, OUTPUT );      // reset
  pinMode( STROBE_PIN, OUTPUT );   // STROBE_PIN

  pinMode( LBAND_PIN, INPUT_PULLUP );
  pinMode( RBAND_PIN, INPUT_PULLUP );

  digitalWrite( RES_PIN, LOW );
  digitalWrite( STROBE_PIN, HIGH );
}


void readMSGEQ7() {
  // get the MSGEQ7's attention
  digitalWrite( RES_PIN, HIGH );
  digitalWrite( RES_PIN, LOW );
  // reset average values
  avgleft  = 0;
  avgright = 0;
  // for each available EQ band
  for( band = 0; band < 7; band++ ) {
    digitalWrite( STROBE_PIN, LOW ); // STROBE_PIN pin - kicks the IC up to the next band
    delayMicroseconds(30); // should be 36 but works with 30
    left[band]  = analogRead( LBAND_PIN ); // store left band reading
    right[band] = analogRead( RBAND_PIN ); // store right band reading
    avgleft  += left[band];  // add for later averaging
    avgright += right[band]; // add for later averaging
    digitalWrite( STROBE_PIN, HIGH );
  }
  avgleft/=7;  // average volume for left channel
  avgright/=7; // average volume for right channel
}


/*\
 * pre-processors
\*/

void levelNoise( Channel channel, int band ) {
  switch( channel ) {
    case CHANNEL_LEFT:
      if ( left[band] < noiseLevel ) {
        left[band] = 0;
      }
    break;
    case CHANNEL_RIGHT:
      if ( right[band] < noiseLevel ) {
        right[band] = 0;
      }
    break;
    case FALLING_LEFT:
      if ( falling_left[band] > left[band] ) {
        falling_left[band] -= falling_speed;
      } else {
        falling_left[band] = left[band]+100;
      }
      if ( falling_left[band] < noiseLevel ) {
        falling_left[band] = 0;
      }
    break;
    case FALLING_RIGHT:
      if ( falling_right[band] > right[band] ) {
        falling_right[band] -= falling_speed;
      } else {
        falling_right[band] = right[band]+100;
      }
      if ( falling_right[band] < noiseLevel ) {
        falling_right[band] = 0;
      }
    break;
  }
}


int mapEQ( Channel channel, int band ) {
  switch( channel ) {
    case CHANNEL_LEFT:  return map( left[band], min_sample, max_sample, bandMinX, bandMaxX );
    case CHANNEL_RIGHT: return map( right[band], min_sample, max_sample, bandMinX, bandMaxX );
    case FALLING_LEFT:  return map( falling_left[band], min_sample, max_sample, bandMinX, bandMaxX );
    case FALLING_RIGHT: return map( falling_right[band], min_sample, max_sample, bandMinX, bandMaxX );
    case VOLUME_LEFT:   return map( avgleft, min_sample, max_sample, 0, halfWidth );
    case VOLUME_RIGHT:  return map( avgright, min_sample, max_sample, 0, halfWidth );
    default: return 0;
  }
}


/*\
 * rendering
\*/

void drawHistogram( int band ) {
  EQVal = mapEQ( CHANNEL_LEFT, band );
  tc = colorAt( bandMinX, bandMaxX, EQVal, RGBEnd, RGBStart );
  fillGradientVRect( halfBandMargin+band*bandSpace, volumePosY-EQVal, bandWidth, EQVal, tc, RGBEnd );

  EQVal = mapEQ( CHANNEL_RIGHT, band );
  tc = colorAt( bandMinX, bandMaxX, EQVal, RGBEnd, RGBStart );
  fillGradientVRect( halfBandMargin+halfWidth+(6-band)*bandSpace, volumePosY-EQVal, bandWidth, EQVal, tc, RGBEnd );
}


void drawHistogramCustom( Channel channel, int band ) {
  switch( channel ) {
    case CHANNEL_LEFT:
      posX      = halfBandMargin+band*bandSpace;
      EQVal     = mapEQ( CHANNEL_LEFT, band );
      EQValFall = mapEQ( FALLING_LEFT, band );
    break;
    case CHANNEL_RIGHT:
      posX      = halfBandMargin+halfWidth+(6-band)*bandSpace;
      EQVal     = mapEQ( CHANNEL_RIGHT, band );
      EQValFall = mapEQ( FALLING_RIGHT, band );
    break;
  }

  tc = colorAt( bandMinX, bandMaxX, EQVal, RGBEnd, RGBStart );
  fillGradientVRect( posX, volumePosY-EQVal, bandWidth, EQVal, tc, RGBEnd );

  tc = colorAt( bandMinX, bandMaxX, EQValFall, RGBEnd, RGBStart );
  sprite.fillRect( posX, volumePosY-EQValFall, bandWidth, fallingBandHeight, color565( tc ) );  // Display falling
}


void drawHistogramCustom( int band ) {
  drawHistogramCustom( CHANNEL_LEFT, band );
  drawHistogramCustom( CHANNEL_RIGHT, band );
}


void drawMirror( int band ) {
  EQVal = mapEQ( CHANNEL_LEFT, band );
  tc = colorAt( bandMinX, bandMaxX, EQVal, RGBEnd, RGBStart );
  sprite.fillRect( halfBandMargin+band*bandSpace, (spriteHeight-EQVal)/2, bandWidth, EQVal, color565( tc ) );

  EQVal = mapEQ( CHANNEL_RIGHT, band );
  tc = colorAt( bandMinX, bandMaxX, EQVal, RGBEnd, RGBStart );
  sprite.fillRect( halfBandMargin+halfWidth+(6-band)*bandSpace, (spriteHeight-EQVal)/2, bandWidth, EQVal, color565( tc ) );
}


void drawVolume() {
  EQVal = mapEQ( VOLUME_LEFT, band );
  tc = colorAt( 0, halfWidth, EQVal, RGBEnd, RGBStart );
  fillGradientHRect( halfWidth-EQVal-halfBandMargin, volumePosY+halfBandMargin, EQVal, paddingTop-halfBandMargin, tc, RGBEnd );

  EQVal = mapEQ( VOLUME_RIGHT, band );
  tc = colorAt( 0, halfWidth, EQVal, RGBEnd, RGBStart );
  fillGradientHRect( halfWidth-halfBandMargin, volumePosY+halfBandMargin, EQVal, paddingTop-halfBandMargin, RGBEnd, tc );
}


void checkButton() {
  #if defined( ARDUINO_M5Stack_Core_ESP32 ) || defined( ARDUINO_M5STACK_FIRE )
    M5.update();
    if( M5.BtnC.wasPressed() ) {
      currentEffect++;
      currentEffect=currentEffect%numEffects;
      delay( 100 );
    }
    if( M5.BtnA.wasPressed() ) {
      // toggle fps counter
      showFPS = !  showFPS;
    }
  #endif
}


void renderFPS() {
  unsigned long nowMillis = millis();
  if(nowMillis - fstart >= fpsInterval) {
    fpi = float(framesCount * fpsInterval) / float(nowMillis - fstart);
    fps = int(fpi*fpsscale);
    fstart = nowMillis;
    framesCount = 0;
  } else {
    framesCount++;
  }
  sprite.setCursor( 0, 0 );
  sprite.printf( "fps: %3d", fps );
}


void setup() {
  M5.begin();
  #if defined( ARDUINO_M5Stack_Core_ESP32 ) || defined( ARDUINO_M5STACK_FIRE )
    if( BUTTON_A_PIN > 0 && digitalRead(BUTTON_A_PIN) == 0) {
      Serial.println("Will Load menu binary");
      updateFromFS( SD );
      ESP.restart();
    }
  #endif
  initMSGEQ7();
  tft.setBrightness(20);
  if( psramInit() && spriteWidth*spriteHeight < 238*238 ) {
    sprite.setPsram( false ); // disable psram if possible
  }
  sprite.createSprite( spriteWidth, spriteHeight );

  spritePosX = tft.width()/2  - spriteWidth/2;
  spritePosY = tft.height()/2 - spriteHeight/2;
}


void loop() {
  checkButton();
  readMSGEQ7();
  sprite.fillRect(0, 0, spriteWidth, spriteHeight, TFT_BLACK);

  for( band = 0; band < 7; band++ ) {
    levelNoise( CHANNEL_LEFT, band );
    levelNoise( CHANNEL_RIGHT, band );

    switch( currentEffect ) {
      case EFFECT_HISTOGRAM:
        drawHistogram( band );
        drawVolume();
      break;
      case EFFECT_MIRROR:
        drawMirror( band );
      break;
      default:
      case EFFECT_HISTOGRAM_CUSTOM_RAIN:
        levelNoise( FALLING_LEFT, band );
        levelNoise( FALLING_RIGHT, band );
        drawHistogramCustom( band );
        drawVolume();
      break;
    }
  }

  if( showFPS ) renderFPS();
  sprite.pushSprite( spritePosX, spritePosY );
}
