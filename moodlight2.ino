/*

    M O O D L I G H T 2

    Steuerung einer 11x11 Pixel großen Matrix von WS2812 RGB-LEDS
    mit diversen Animationen auf einem ESP8266 (NodeMCU)

    11x11 pixel led 2812 matrix with animations
    powered by ESP8266

    CC-BY SA 2020 Kai Laborenz


*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>     // ota updates
#include <strings_en.h>     // wifi manager
#include <WiFiManager.h>    // wifi manager
#include <FastLED.h>        // FastLED Animation library
#include <Encoder.h>        // Rotary encoder
// to enable Encoder library to work with wemos d1 I had to replace Encoder.h with
// https://raw.githubusercontent.com/RLars/Encoder/master/Encoder.h according to
// https://github.com/PaulStoffregen/Encoder/issues/40
#include <Bounce2.h>         // bebounce routine for switches

Encoder myEnc(D1, D2);       // initialize Encoder
long oldEncoderPos  = 0;
#define SWITCH_PIN D7        // rotary encoder switch pin

Bounce modeSwitch = Bounce();  // Instantiate Bounce object
int mode = 4;               // animation modes
const int maxModes = 6;     // number of modes (cycling, starting from 1 so 4 modes)

// FastLED definitions

#define FASTLED_ALLOW_INTERRUPTS 0 // needed to work 

#define LED_PIN D3          // controlpin for ws2812
// strangely has to be placed on pin D0 !?

#define NUM_LEDS 64        // number of leds in matrix
#define NUM_ROWS 8         // number of rows in matrix
#define NUM_COLS 8         // number of cols in matrix

#define COLOR_ORDER GRB
#define CHIPSET     WS2812
#define FRAMES_PER_SECOND 60

#define MASTER_BRIGHTNESS   96
#define STARTING_BRIGHTNESS 64
#define FADE_IN_SPEED       32
#define FADE_OUT_SPEED      20
#define DENSITY            255

int brightness = STARTING_BRIGHTNESS;

CRGB leds[NUM_LEDS];

int animDelay = 0;               // delay of animation loop in ms (controlled by encoder)
const int animDelaySplash = 50;
const int animDelayParty = 250;  // delay start value for animation "Party Time"
const int animDelaytwinkle = 20; // delay start value for animation "Twinkle"
const int animDelayRainbow = 50; // delay start value for animation "Rainbow"

const int minanimDelay = 1;      // minimum delay
const int maxanimDelay = 500;    // maximum delay

uint8_t fHue = 0;               // start color for flat color mode
uint8_t gHue = 0;               // rotating "base color" for fire animation an

bool gReverseDirection = true;  // for twinkling animation

byte led = 13;

#define DEBUG                   // Debugging on

// web server configuration

ESP8266WebServer server(80);

void handleRoot() {
  digitalWrite(led, 1);
  server.send(200, "text/plain", "hello from esp8266!");
  digitalWrite(led, 0);
}


void handleSet() {

  // main function to handle control commands from web to device

  String message = "";
  if (server.arg("mode") == "") {   // Animation mode
    message = "no-mode";
  } else {    //Parameter found
    message = "Mode = ";
    message += server.arg("mode");

    mode = server.arg("mode").toInt();
  }

  if (server.arg("bright") == "") {   // LED brightness
    message += " no-brightness";
  } else {    //Parameter found
    message += " Brightness = ";
    message += server.arg("bright");

    brightness = server.arg("bright").toInt();
    FastLED.setBrightness(brightness);
  }

  if (server.arg("hue") == "") {   // Color hue for flat color display
    message += " no-hue";
  } else {    //Parameter found
    message += " Hue = ";
    message += server.arg("hue");

    fHue = server.arg("hue").toInt();
  }

  server.send(200, "text/plain", message);          //Returns the HTTP response
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}


void setup() {

  Serial.begin(115200);

  // switch for mode change with rotary encoder push button
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  attachInterrupt(SWITCH_PIN, changeMode, RISING);
  modeSwitch.attach(SWITCH_PIN);
  modeSwitch.interval(5); // interval in

  // start LED library and set default brightness
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(MASTER_BRIGHTNESS);

  Serial.println("Booting Moodlight2 ...");

  // Starting wifi manager

    WiFiManager wifiManager;
    wifiManager.autoConnect("MOODLIGHT2");

  //
  // starting ota update capability
  //

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
  
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
  
    Serial.println("Moodlight2 ready!");

  // starting server

    if (MDNS.begin("esp8266")) {
      Serial.println("MDNS responder started");
    }


  // server commands

  server.on("/", handleRoot); // default web page
  server.on("/set", handleSet); // set mode
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
  delay(1000);

}

void loop() {

  switch (mode) {

    ///////    ANIMATION 1    ///////

    case 1:

      Serial.println("Flat Color");

      while (mode == 1) {

        uint16_t i;

        myEnc.write(fHue);

        for (i = 0; i < NUM_LEDS; i++) {
          //leds[i] = CHSV(hue, 255, 255); // set random color
          fill_solid(leds, NUM_LEDS, CHSV(fHue, 255, 255));
        }

        // read hue from encoder
        long newEncoderPos = constrain((myEnc.read()), 0, 255);
        if (newEncoderPos != oldEncoderPos) {
          oldEncoderPos = newEncoderPos;
        }
        fHue = newEncoderPos;
        #ifdef DEBUG
          Serial.print("Mode: ");
          Serial.print(mode);
          Serial.print(" | ");
          Serial.print("Hue: ");
          Serial.println(fHue);
        #endif

        FastLED.show();
        ArduinoOTA.handle();

        server.handleClient();
        MDNS.update();

        FastLED.delay(1000 / FRAMES_PER_SECOND);

      }
      break;


    ///////    ANIMATION 2    ///////

    case 2:

      Serial.println("Splash worms!");

      animDelay = animDelaySplash;
      myEnc.write(animDelay);

      while (mode == 2) {

        // eight colored dots, weaving in and out of sync with each other
        fadeToBlackBy( leds, NUM_LEDS, 20);

        byte dothue = 0;
        for ( int i = 0; i < 8; i++) {
          leds[beatsin16(i + 7, 0, NUM_LEDS)] |= CHSV(dothue, 200, 255);
          dothue += 32;

          FastLED.show();
          yield(); // needed to prevent soft reset by esp watchdog timer

          long newEncoderPos = constrain((myEnc.read()), minanimDelay, maxanimDelay);

          if (newEncoderPos != oldEncoderPos) {
            oldEncoderPos = newEncoderPos;
          }
          animDelay = newEncoderPos;

        #ifdef DEBUG
          Serial.print("Mode: ");
          Serial.print(mode);
          Serial.print(" | ");
          Serial.print("Delay: ");
          Serial.println(animDelay);
        #endif
        }
        server.handleClient();
        MDNS.update();
        FastLED.delay(animDelay);
      }
      break;


    ///////    ANIMATION 3    ///////

    case 3:

      Serial.println("Party Time!");

      uint16_t i;
      animDelay = animDelayParty;
      myEnc.write(animDelay);
      Serial.println(animDelay);

      while (mode == 3) {
        for (i = 0; i < NUM_LEDS; i++) {
          leds[i] = CHSV(random8(), 255, 255); // set random color
        }

        FastLED.show();

        // read animation speed delay from encoder
        long newEncoderPos = constrain((2 * myEnc.read()), minanimDelay, maxanimDelay);

        if (newEncoderPos != oldEncoderPos) {
          oldEncoderPos = newEncoderPos;
        }
        animDelay = newEncoderPos;

        #ifdef DEBUG
          Serial.print("Mode: ");
          Serial.print(mode);
          Serial.print(" | ");
          Serial.print("Delay: ");
          Serial.println(animDelay);
        #endif

        FastLED.delay(animDelay);

        server.handleClient();
        MDNS.update();

      } // end while
      FastLED.clear();
      break;


    ///////    ANIMATION 4    ///////

    //  Twinkling 'holiday' lights that fade up and down in brightness.
    //  Colors are chosen from a palette; a few palettes are provided.
    //
    //  The basic operation is that all pixels stay black until they
    //  are 'seeded' with a relatively dim color.  The dim colors
    //  are repeatedly brightened until they reach full brightness, then
    //  are darkened repeatedly until they are fully black again.
    //
    //  A set of 'directionFlags' is used to track whether a given
    //  pixel is presently brightening up or darkening down.
    //
    //  Darkening colors accurately is relatively easy: scale down the
    //  existing color channel values.  Brightening colors is a bit more
    //  error prone, as there's some loss of precision.  If your colors
    //  aren't coming our 'right' at full brightness, try increasing the
    //  STARTING_BRIGHTNESS value.
    //
    //  -Mark Kriegsman, December 2014

    case 4:

      Serial.println("Twinkling Lights!");

      animDelay = animDelaytwinkle;
      myEnc.write(animDelay);

      while (mode == 4) {

        chooseColorPalette();
        colortwinkles();
        FastLED.show();
        FastLED.delay(animDelay);

        long newEncoderPos = constrain((myEnc.read() / 3), minanimDelay, maxanimDelay);
        if (newEncoderPos != oldEncoderPos) {
          oldEncoderPos = newEncoderPos;
        }
        animDelay = newEncoderPos;

        #ifdef DEBUG
          Serial.print("Mode: ");
          Serial.print(mode);
          Serial.print(" | ");
          Serial.print("Delay: ");
          Serial.println(animDelay);
        #endif

        server.handleClient();
        MDNS.update();
        FastLED.delay(animDelay);
      }
      break;

    ///////    ANIMATION 4    ///////

    case 5:

      Serial.println("Rainbow!");

      animDelay = animDelayRainbow;
      myEnc.write(animDelay);
      Serial.println(animDelay);

      while (mode == 5) {
        yield();
        uint32_t ms = millis();
        int32_t yHueDelta32 = ((int32_t)cos16( ms * 27 ) * (350 / NUM_COLS));
        int32_t xHueDelta32 = ((int32_t)cos16( ms * 39 ) * (310 / NUM_ROWS));
        DrawOneFrame( ms / 65536, yHueDelta32 / 32768, xHueDelta32 / 32768);
        FastLED.show();

        long newEncoderPos = constrain((myEnc.read() / 3), minanimDelay, maxanimDelay);

        if (newEncoderPos != oldEncoderPos) {
          oldEncoderPos = newEncoderPos;
        }
        animDelay = newEncoderPos;
        FastLED.delay(animDelay);
        
        #ifdef DEBUG
          Serial.print("Mode: ");
          Serial.print(mode);
          Serial.print(" | ");
          Serial.print("Delay: ");
          Serial.println(animDelay);
        #endif

        server.handleClient();
        MDNS.update();
      }
      break;

    case 6:
      Serial.println("Fire!");

      while (mode == 6) {
        Fire2012();

        server.handleClient();
        MDNS.update();
      }
      break;


  }   // end switch
}     // end main loop

void DrawOneFrame( byte startHue8, int8_t yHueDelta8, int8_t xHueDelta8)
{
  byte lineStartHue = startHue8;
  for ( byte y = 0; y < NUM_ROWS; y++) {
    lineStartHue += yHueDelta8;
    byte pixelHue = lineStartHue;
    for ( byte x = 0; x < NUM_COLS; x++) {
      pixelHue += xHueDelta8;
      leds[ XY(x, y)]  = CHSV( pixelHue, 255, 255);
    }
  }
}
// Helper function that translates from x, y into an index into the LED array
// Handles both 'row order' and 'serpentine' pixel layouts.
uint16_t XY( uint8_t x, uint8_t y)
{
  uint16_t i;

  if ( gReverseDirection == false) {
    i = (y * NUM_COLS) + x;
  } else {
    if ( y & 0x01) {
      // Odd rows run backwards
      uint8_t reverseX = (NUM_COLS - 1) - x;
      i = (y * NUM_COLS) + reverseX;
    } else {
      // Even rows run forwards
      i = (y * NUM_COLS) + x;
    }
  }

  return i;
}

/////  ANIMATION MODES  /////

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
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100
#define COOLING  55

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 120

void Fire2012()
{
  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
  for ( int i = 0; i < NUM_LEDS; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( int k = NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if ( random8() < SPARKING ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160, 255) );
  }

  // Step 4.  Map from heat cells to LED colors
  for ( int j = 0; j < NUM_LEDS; j++) {
    CRGB color = HeatColor( heat[j]);
    int pixelnumber;
    if ( gReverseDirection ) {
      pixelnumber = (NUM_LEDS - 1) - j;
    } else {
      pixelnumber = j;
    }
    leds[pixelnumber] = color;
  }
  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}


void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 5);
  FastLED.show(); // display this frame
}


/// Routines for animation tinkling lights ///

CRGBPalette16 gPalette;

void chooseColorPalette()
{
  uint8_t numberOfPalettes = 5;
  uint8_t secondsPerPalette = 10;
  uint8_t whichPalette = (millis() / (1000 * secondsPerPalette)) % numberOfPalettes;

  CRGB r(CRGB::Red), b(CRGB::Blue), w(85, 85, 85), g(CRGB::Green), W(CRGB::White), l(0xE1A024);

  switch ( whichPalette) {
    case 0: // Red, Green, and White
      gPalette = CRGBPalette16( r, r, r, r, r, r, r, r, g, g, g, g, w, w, w, w );
      break;
    case 1: // Blue and White
      //gPalette = CRGBPalette16( b,b,b,b, b,b,b,b, w,w,w,w, w,w,w,w );
      gPalette = CloudColors_p; // Blues and whites!
      break;
    case 2: // Rainbow of colors
      gPalette = RainbowColors_p;
      break;
    case 3: // Incandescent "fairy lights"
      gPalette = CRGBPalette16( l, l, l, l, l, l, l, l, l, l, l, l, l, l, l, l );
      break;
    case 4: // Snow
      gPalette = CRGBPalette16( W, W, W, W, w, w, w, w, w, w, w, w, w, w, w, w );
      break;
  }
}

enum { GETTING_DARKER = 0, GETTING_BRIGHTER = 1 };

void colortwinkles()
{
  // Make each pixel brighter or darker, depending on
  // its 'direction' flag.
  brightenOrDarkenEachPixel( FADE_IN_SPEED, FADE_OUT_SPEED);

  // Now consider adding a new random twinkle
  if ( random8() < DENSITY ) {
    int pos = random16(NUM_LEDS);
    if ( !leds[pos]) {
      leds[pos] = ColorFromPalette( gPalette, random8(), STARTING_BRIGHTNESS, NOBLEND);
      setPixelDirection(pos, GETTING_BRIGHTER);
    }
  }
}

void brightenOrDarkenEachPixel( fract8 fadeUpAmount, fract8 fadeDownAmount)
{
  for ( uint16_t i = 0; i < NUM_LEDS; i++) {
    if ( getPixelDirection(i) == GETTING_DARKER) {
      // This pixel is getting darker
      leds[i] = makeDarker( leds[i], fadeDownAmount);
    } else {
      // This pixel is getting brighter
      leds[i] = makeBrighter( leds[i], fadeUpAmount);
      // now check to see if we've maxxed out the brightness
      if ( leds[i].r == 255 || leds[i].g == 255 || leds[i].b == 255) {
        // if so, turn around and start getting darker
        setPixelDirection(i, GETTING_DARKER);
      }
    }
  }
}

CRGB makeBrighter( const CRGB& color, fract8 howMuchBrighter)
{
  CRGB incrementalColor = color;
  incrementalColor.nscale8( howMuchBrighter);
  return color + incrementalColor;
}

CRGB makeDarker( const CRGB& color, fract8 howMuchDarker)
{
  CRGB newcolor = color;
  newcolor.nscale8( 255 - howMuchDarker);
  return newcolor;
}

uint8_t  directionFlags[ (NUM_LEDS + 7) / 8];

bool getPixelDirection( uint16_t i) {
  uint16_t index = i / 8;
  uint8_t  bitNum = i & 0x07;
  return bitRead( directionFlags[index], bitNum);
}
void setPixelDirection( uint16_t i, bool dir) {
  uint16_t index = i / 8;
  uint8_t  bitNum = i & 0x07;
  bitWrite( directionFlags[index], bitNum, dir);
}



/////  I2C + SWITCH SUBROUTINES  /////

ICACHE_RAM_ATTR void changeMode() {
  // changes mode after encoder button is pressed

  mode = mode + 1;
  if (mode > maxModes) mode = 1;
  if (mode < 1) mode = maxModes;

  //delay(1000);
  //Serial.print("Switched to Modus: ");
  //Serial.println(mode);
}
