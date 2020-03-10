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


  Adaptations:
    - ESP32 support instead (was Arduino Nano)
    - Band levelling
    - Settings Persistence
    - Web Server and UI


  Project resources:
    - http://www.instructables.com/id/Arduino-Based-Mini-Audio-Spectrum-Visualizer/
    - https://github.com/debsahu/ESP32_FFT_Audio_LEDs
    - https://tronixstuff.files.wordpress.com/2013/01/msgeq7-data-sheet.pdf

 *
\*/

#include <ESP32-Chimera-Core.h> // https://github.com/tobozo/ESP32-Chimera-Core
#include <M5StackUpdater.h> // https://github.com/tobozo/M5Stack-SD-Updater
#include "web.h"
#include <Preferences.h>

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
TFT_eSprite uiSprite = TFT_eSprite( & tft );
Preferences preferences;

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

const char *bandNames[] = {
  "63",
  "160",
  "400",
  "1 K",
  "2.5 K",
  "6.25 K",
  "16 K"
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
bool showFPS = false;

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

static byte spectrumFactors[7] = {9, 11, 13, 13, 12, 12, 13};

// Channels and Bands
int left[7];    // store left audio band values in these arrays
int right[7];   // store right audio band values in these arrays
int avgleft, avgright;
//float avgGain;
int band;
int noiseLevel = 127;  // change this value to suppress display due to noise pickup
int noiseLevels[7] = { noiseLevel, noiseLevel, noiseLevel, noiseLevel, noiseLevel, noiseLevel, noiseLevel };

// used for later v/h map()ping
int bandMinX = 0;
int bandMaxX = spriteHeight - paddingTop;

// histogram effects browser
int numEffects = sizeof(enum EqEffects);
int currentEffect = EFFECT_HISTOGRAM_CUSTOM_RAIN;



/*\
 * EQ Gain controls
\*/

// gain per EQ Band (setter)
void setSpectrumFactor( int band, int value ) {
  spectrumFactors[band] = value;
}

// gain per EQ Band (getter)
int getSpectrumFactor( int band ) {
  return spectrumFactors[band];
}



/*\
 * Prefs persistence
\*/

int getPref( int band, const char* name ) {
  preferences.begin("MSGEQ7Prefs", true);
  char prefname[32];
  const char* preftpl = "%s-%d";
  snprintf( prefname, 32, preftpl, name, band );
  int value = preferences.getInt( prefname, getSpectrumFactor( band ) );
  Serial.printf("Loading pref #%d '%s:%d'\n", band, prefname, value);
  preferences.end();
  return value;
}


void setPref( int band, int value, const char* name  ) {
  preferences.begin("MSGEQ7Prefs", false);
  char prefname[32];
  const char* preftpl = "%s-%d";
  snprintf( prefname, 32, preftpl, name, band );
  preferences.putInt( prefname, value );
  Serial.printf("Saving pref #%d '%s:%d'\n", band, prefname, value);
  setSpectrumFactor( band, value );
  preferences.end();
  drawRangeSliders();
}



/*\
 * Because ESP32 WiFi can't reconnect by itself (bug)
\*/

void stubbornConnect() {
  uint8_t wifi_retry_count = 0;
  uint8_t max_retries = 3;
  unsigned long stubbornness_factor = 3000; // ms to wait between attempts

  #ifdef ESP32
    while (WiFi.status() != WL_CONNECTED && wifi_retry_count < 3)
  #else
    while (WiFi.waitForConnectResult() != WL_CONNECTED && wifi_retry_count < max_retries)
  #endif
  {
    WiFi.begin(/* "ssid", "password" */); // put your ssid / pass if required, only needed once
    Serial.printf("WiFi connect - Attempt No. %d\n", wifi_retry_count+1);
    delay( stubbornness_factor );
    wifi_retry_count++;
  }

  if(wifi_retry_count >= 3) {
    Serial.println("no connection, forcing restart");
    ESP.restart();
  }

  if (WiFi.waitForConnectResult() == WL_CONNECTED){
    Serial.println("Connected as");
    Serial.println(WiFi.localIP());
  }
}



/*\
 * gfx helpers
\*/

void fillGradientVRect( TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t width, uint16_t height, RGBColor RGBStart, RGBColor RGBEnd ) {
  for( uint16_t w = 0; w < width; w++ ) {
    sprite.drawGradientVLine( x+w, y, height, RGBStart, RGBEnd );
  }
}

void fillGradientHRect( TFT_eSprite &sprite,  uint16_t x, uint16_t y, uint16_t width, uint16_t height, RGBColor RGBStart, RGBColor RGBEnd ) {
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

  for( band = 0; band < 7; band++ ) {
    setSpectrumFactor( band, getPref( band ) );
  }

  drawRangeSliders();

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

    levelNoise( CHANNEL_LEFT, band );
    levelNoise( CHANNEL_RIGHT, band );
    levelNoise( FALLING_LEFT, band );
    levelNoise( FALLING_RIGHT, band );

    avgleft  += left[band];  // add for later averaging
    avgright += right[band]; // add for later averaging
    digitalWrite( STROBE_PIN, HIGH );

  }
  avgleft  /=7;  // average volume for left channel
  avgright /=7; // average volume for right channel
}


/*

// readMSGEQ7() rewrite in progress


// Smooth/average settings
#define SPECTRUMSMOOTH 0.08
#define PEAKDECAY 0.01
#define NOISEFLOOR 64

// AGC settings
#define AGCSMOOTH 0.004
#define GAINUPPERLIMIT 31.0
#define GAINLOWERLIMIT 0.1

// Global variables
uint8_t spectrumByteLeft[7];        // holds 8-bit adjusted adc values
uint8_t spectrumAvgLeft;
unsigned int spectrumValueLeft[7];  // holds raw adc values
float spectrumDecayLeft[7] = {0};   // holds time-averaged values
float spectrumPeaksLeft[7] = {0};   // holds peak values
float audioAvgLeft = 270.0;
float gainAGCLeft = 0.0;

uint8_t spectrumByteRight[7];        // holds 8-bit adjusted adc values
uint8_t spectrumAvgRight;
unsigned int spectrumValueRight[7];  // holds raw adc values
float spectrumDecayRight[7] = {0};   // holds time-averaged values
float spectrumPeaksRight[7] = {0};   // holds peak values
float audioAvgRight = 270.0;
float gainAGCRight = 0.0;

static byte noiseFloor[7] = { 32, 64, 64, 64, 92, 92, 92 };

void readAudio() {

  // reset MSGEQ7 to first frequency bin
  digitalWrite(RES_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(RES_PIN, LOW);

  // store sum of values for AGC
  int analogsumLeft  = 0;
  int analogsumRight = 0;

  // cycle through each MSGEQ7 bin and read the analog values
  for (int i = 0; i < 7; i++) {

    // set up the MSGEQ7
    digitalWrite(STROBE_PIN, LOW);
    delayMicroseconds(50); // to allow the output to settle

    // read the analog value
    spectrumValueLeft[i]  = analogRead(LBAND_PIN);
    spectrumValueRight[i] = analogRead(RBAND_PIN);
    // map to byte range
    spectrumValueLeft[i]  = map( spectrumValueLeft[i], min_sample, max_sample, 0, 1024 );
    spectrumValueRight[i] = map( spectrumValueRight[i], min_sample, max_sample, 0, 1024 );

    digitalWrite(STROBE_PIN, HIGH);

    // noise floor filter
    if (spectrumValueLeft[i] < noiseFloor[i]) {
      spectrumValueLeft[i] = 0;
    } else {
      spectrumValueLeft[i] -= noiseFloor[i];
    }

    // noise floor filter
    if (spectrumValueRight[i] < noiseFloor[i]) {
      spectrumValueRight[i] = 0;
    } else {
      spectrumValueRight[i] -= noiseFloor[i];
    }

    // apply correction factor per frequency bin
    spectrumValueLeft[i]  = (spectrumValueLeft[i]  * spectrumFactors[i]) / 10;
    spectrumValueRight[i] = (spectrumValueRight[i] * spectrumFactors[i]) / 10;

    // prepare average for AGC
    analogsumLeft  += spectrumValueLeft[i];
    analogsumRight += spectrumValueRight[i];

    // apply current gain value
    spectrumValueLeft[i]  *= gainAGCLeft;
    spectrumValueRight[i] *= gainAGCRight;

    // process time-averaged values
    spectrumDecayLeft[i]  = (1.0 - SPECTRUMSMOOTH) * spectrumDecayLeft[i]  + SPECTRUMSMOOTH * spectrumValueLeft[i];
    spectrumDecayRight[i] = (1.0 - SPECTRUMSMOOTH) * spectrumDecayRight[i] + SPECTRUMSMOOTH * spectrumValueRight[i];

    // process peak values
    if (spectrumPeaksLeft[i]  < spectrumDecayLeft[i])  spectrumPeaksLeft[i]  = spectrumDecayLeft[i];
    if (spectrumPeaksRight[i] < spectrumDecayRight[i]) spectrumPeaksRight[i] = spectrumDecayLeft[i];
    spectrumPeaksLeft[i]  = spectrumPeaksLeft[i]  * (1.0 - PEAKDECAY);
    spectrumPeaksRight[i] = spectrumPeaksRight[i] * (1.0 - PEAKDECAY);

    spectrumByteLeft[i]  = spectrumValueLeft[i] / 4;
    spectrumByteRight[i] = spectrumValueRight[i] / 4;
  }

  // Calculate audio levels for automatic gain
  audioAvgLeft  = (1.0 - AGCSMOOTH) * audioAvgLeft +  AGCSMOOTH * (analogsumLeft / 7.0);
  audioAvgRight = (1.0 - AGCSMOOTH) * audioAvgRight + AGCSMOOTH * (analogsumRight / 7.0);

  spectrumAvgLeft  = (analogsumLeft / 7.0) / 4;
  spectrumAvgRight = (analogsumRight / 7.0) / 4;

  // Calculate gain adjustment factor
  gainAGCLeft = 270.0 / audioAvgLeft;
  gainAGCRight = 270.0 / audioAvgLeft;
  if (gainAGCLeft  > GAINUPPERLIMIT) gainAGCLeft  = GAINUPPERLIMIT;
  if (gainAGCLeft  < GAINLOWERLIMIT) gainAGCLeft  = GAINLOWERLIMIT;
  if (gainAGCRight > GAINUPPERLIMIT) gainAGCRight = GAINUPPERLIMIT;
  if (gainAGCRight < GAINLOWERLIMIT) gainAGCRight = GAINLOWERLIMIT;

  for (int band = 0; band < 7; band++) {
    Serial.print(spectrumByteLeft[band]);
    Serial.print("\t");
  }
  Serial.println();

}

*/



/*\
 * pre-processors
\*/

void levelNoise( Channel channel, int band ) {
  switch( channel ) {
    case CHANNEL_LEFT:
      if ( left[band] < noiseLevels[band] ) {
        left[band] = 0;
      }
      left[band]  = (left[band]  * spectrumFactors[band]) / 10;
    break;
    case CHANNEL_RIGHT:
      if ( right[band] < noiseLevels[band] ) {
        right[band] = 0;
      }
      right[band]  = (right[band]  * spectrumFactors[band]) / 10;
    break;
    case FALLING_LEFT:
      if ( falling_left[band] > left[band] ) {
        falling_left[band] -= falling_speed;
      } else {
        falling_left[band] = left[band]+100;
      }
      if ( falling_left[band] < noiseLevels[band] ) {
        falling_left[band] = 0;
      }
    break;
    case FALLING_RIGHT:
      if ( falling_right[band] > right[band] ) {
        falling_right[band] -= falling_speed;
      } else {
        falling_right[band] = right[band]+100;
      }
      if ( falling_right[band] < noiseLevels[band] ) {
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


void drawRangeSlider( int posX, int posY, int width, int height, int margin, int value, int minRange=0, int maxRange=99 ) {
  int sliderWidth  = width * .2;
  int sliderHeight = height * .25;
  int graduationWidth = width * .65;

  uiSprite.createSprite( width, height );

  fillGradientVRect( uiSprite, 0, 0, width, height, {0x44,0xff,0x44}, {0x88, 0x22, 0xaa} );

  uiSprite.drawFastVLine( 0+width/2, margin/2, height-margin, TFT_BLACK );
  uiSprite.drawFastVLine( 2+width/2, margin/2, height-margin, tft.color565( 0x77, 0x77, 0x77 ) );

  float istep = float(height/8);
  bool isodd = true;
  for( float i=margin/2; i<height-margin/2; i+=istep/2 ) {
    int relw = isodd ? graduationWidth/2 : graduationWidth;
    uiSprite.drawFastHLine( 1+width/2 - relw/2, i, relw, TFT_BLACK );
    isodd = ! isodd;
  }

  // logarithmic slider
  float yslider = log( value + 10 ) / log(10); // dafuq arduino maths notation log() vs logn() ?
  int sliderPos = map( int(yslider*1000.0), 1000/*log(10)*1000*/, 2037/*log(99+10)*1000*/, height, 0 );

  fillGradientVRect( uiSprite, 1+ width/2 - sliderWidth, sliderPos - sliderHeight, sliderWidth*2, sliderHeight, {0x11,0x11,0x11},   {0x77, 0x77, 0x77} );
  fillGradientVRect( uiSprite, 1+ width/2 - sliderWidth, sliderPos,                sliderWidth*2, sliderHeight, {0x66, 0x66, 0x66}, {0x44, 0x44, 0x44} );

  uiSprite.drawFastHLine( 1+ width/2 - sliderWidth, sliderPos-sliderHeight+1, sliderWidth*2, TFT_WHITE );
  uiSprite.drawFastHLine( 1+ width/2 - sliderWidth, sliderPos+sliderHeight-1, sliderWidth*2, tft.color565( 0x11,0x11,0x11 ) );

  uiSprite.drawFastHLine( 1+ width/2 - sliderWidth, sliderPos-1,              sliderWidth*2, TFT_BLACK );
  uiSprite.drawFastHLine( 1+ width/2 - sliderWidth, sliderPos,                sliderWidth*2, tft.color565( 0xaa, 0xaa, 0xaa ) );
  uiSprite.drawFastHLine( 1+ width/2 - sliderWidth, sliderPos+1,              sliderWidth*2, TFT_WHITE );

  uiSprite.drawRect(      1+ width/2 - sliderWidth, sliderPos-sliderHeight,   sliderWidth*2, 1+sliderHeight*2, TFT_BLACK );

  uiSprite.pushSprite( posX, posY );
  uiSprite.deleteSprite();
}


void drawRangeSliders() {
  float margin = (spriteWidth*3)/70;
  int x = 0;//spritePosX;
  int posX = 0;
  int posY = spritePosY + spriteHeight + margin;
  int height = 50;
  int width  = (tft.width() - margin*7 ) / 7 ;

  for( band = 0; band < 7; band++ ) {
    posX = x + (band* float(width+margin));
    drawRangeSlider( posX + margin/2, posY, width, height, margin, spectrumFactors[band] );

    int w = tft.textWidth( bandNames[band] );
    int gap = (width+margin)/2 - w/2;

    tft.setCursor( posX+gap, posY + height + margin );
    tft.print( bandNames[band] );
  }
}


void drawHistogram( int band ) {
  EQVal = mapEQ( CHANNEL_LEFT, band );
  tc = colorAt( bandMinX, bandMaxX, EQVal, RGBEnd, RGBStart );
  fillGradientVRect( sprite, halfBandMargin+band*bandSpace, volumePosY-EQVal, bandWidth, EQVal, tc, RGBEnd );

  EQVal = mapEQ( CHANNEL_RIGHT, band );
  tc = colorAt( bandMinX, bandMaxX, EQVal, RGBEnd, RGBStart );
  fillGradientVRect( sprite, halfBandMargin+halfWidth+(6-band)*bandSpace, volumePosY-EQVal, bandWidth, EQVal, tc, RGBEnd );
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
  fillGradientVRect( sprite, posX, volumePosY-EQVal, bandWidth, EQVal, tc, RGBEnd );

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
  fillGradientHRect( sprite, halfWidth-EQVal-halfBandMargin, volumePosY+halfBandMargin, EQVal, paddingTop-halfBandMargin, tc, RGBEnd );

  EQVal = mapEQ( VOLUME_RIGHT, band );
  tc = colorAt( 0, halfWidth, EQVal, RGBEnd, RGBStart );
  fillGradientHRect( sprite, halfWidth-halfBandMargin, volumePosY+halfBandMargin, EQVal, paddingTop-halfBandMargin, RGBEnd, tc );
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
  stubbornConnect();

  if (MDNS.begin("esp32-msgeq7")) {
    Serial.println("MDNS responder started: esp32-msgeq7.local");
    tft.setCursor(0, 0);
    tft.print( WiFi.localIP() );
    tft.setCursor(0, 16);
    tft.print("http://esp32-msgeq7.local");
  }

  server.on("/", sendForm);
  server.on(UriRegex("^\\/level\\/([0-6])\\/([0-9]+)$"), handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
  webServerRunning = true; // TODO: add means to start/stop WiFi+WebServer

  tft.setBrightness(20);
  if( psramInit() && spriteWidth*spriteHeight < 238*238 ) {
    sprite.setPsram( false ); // disable psram if possible
  }
  sprite.createSprite( spriteWidth, spriteHeight );

  spritePosX = tft.width()/2  - spriteWidth/2;
  spritePosY = tft.height()/2 - spriteHeight/2;

  initMSGEQ7();
}


void loop() {
  checkButton();

  if( webServerRunning ) {
    server.handleClient();
  }

  readMSGEQ7();

  sprite.fillRect(0, 0, spriteWidth, spriteHeight, TFT_BLACK);

  for( band = 0; band < 7; band++ ) {
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
        drawHistogramCustom( band );
        drawVolume();
      break;
    }
  }

  if( showFPS ) renderFPS();
  sprite.pushSprite( spritePosX, spritePosY );
}
