// https://github.com/VincentGlueck/ttgo_radio.git

/*************************************************************
* REMEMBER
*
* in libraries/TFT_eSPI activate TTGO in User_Setup_Select.h
* (line 58 approx.) and comment out //#include <User_Setup.h>
*
**************************************************************/

#ifndef ESP32
#error "Requires ESP32, TTGO TFT display (135x240 pixels)"
#endif

#include <Preferences.h>
#include <WiFi.h>
#include <AudioFileSource.h>
#include <AudioFileSourceBuffer.h>
#include <AudioFileSourceICYStream.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorAAC.h>
#include <AudioOutputI2S.h>
#include <AudioOutputI2SNoDAC.h>
#include <TFT_eSPI.h>
#include <spiram-fast.h>
#include "SafeString.h"
#include "Orbitron_Medium_20.h"
#include "stations.h"
#include "bg_img.h"
#include "TtgoButton.h"
#include "colors.h"
#include "time.h"

#define USE_SERIAL_OUT          // this will output some status messages to the console and slow down things (use 115.200 or above)
#define BUF_SIZE 0x0c00         // size of streaming buffer (0x400 -> more decoding errors, 0x1000 -> default)
#define USE_STEREO true         // stereo (Pin25,26) or mono (Pin26 only) on most TTGOs
#define BRIGHTNESS 210          // brightness during display = on (max 255)
#define TFT_OFF_TIMEOUT 300000  // display will go off after xyz milliseconds
//#define TFT_ALWAYS_ON           // activate display always on, otherwise TFT_OFF_TIMEOUT millis will smoothly turn of display
#define CREDITS_DISPLAY         // uncomment to be unkind ;-)
#define DELAY_START_UP 600      // startup credits/slow down
#define MIN_BG_SWITCH_MS 5000   // background switch on title change not before ms
#define MAX_BG_SAME_MS 30000    // background will force switch after ms
//#define USE_STATION_GAIN        // comment in to change volume to default station's volume on station switch
#define AMP_ANI_Y 160             // position of fake animation
#define AMP_COLORFUL   // use colorful amp ani
//#define RESTORE_BG_RAND_LINES  // change background with animation, comment out to use top-down mode
#define SWITCH_BG_WHEN_PAUSED   // comment out to ensure same background when not playing
//#define SHOW_BRAND              // include title/'brand' - comment out to keep things simple and clear
#define TIME_FONT 7             // valid values (without further changes) would be 6 or 7 (48 pixel fonts)
#define USE_BG_SCROLLER         // title will be displayed incl. background image (slower), uncomment for speed optimized version

/************************
* OUTPUT PIN26, PIN 25  *
* (stereo, 'mono': 26)  *
* MONO !!! ONLY !!!     *
* connect GND!!!        *
 ***********************/

// TODO: enter your WiFi credentials
const char *SSID = "*****ssid*****";
const char *PASSWORD = "****pw****";
//==================================

const char *PREF_NAMESPACE = "vtgor";

//#define DO_GREETINGS          // sensless, or priceless?
const char *GREET_0 = "Dedicated";
const char *GREET_1 = "to";
const char *GREET_2 = "whomever";

#define Y_STATUS 44
#define X_FRAME 38
#define Y_FRAME 134
#define Y_VOLUME 114
#define Y_TIME 66
#define Y_SCROLL 224
#define Y_STATION 29
#define TITLE_DELTA_INITIAL 24

#define MAX_GAIN 20.0f

const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;

uint8_t color = 0;
uint16_t foreGroundColor = foregroundColors[color];
uint16_t backGroundColor = TFT_BLACK;
bool playFlag = false;
bool isStopped = false;
bool stopRequested = false;
int station = 0;
float targetGain = stations[station].gain;
float lastGain = targetGain;
float deltaGain = 0.1f;
float fgain = 0.0f;
String title = "?";
String lastTitle = "";
int titleScroll = 135;
int globalCnt, frameCnt;
long tftOffTimer;
bool tftOff = false;
uint8_t tftBrightness = BRIGHTNESS;
short deltaBrightness = 0;
uint8_t titleLength;
uint32_t lastms = 0;
uint32_t streamingForMs = 0;
String lastPlayTime = "x";

AudioGenerator *decoder = NULL;
AudioFileSourceICYStream *stream;
AudioFileSourceBuffer *buf;
AudioOutputI2S *out;
const int preallocateBufferSize = 0x1000;
const int preallocateCodecSize = 85332;  // AAC+SBR codec max mem needed
void *preallocateBuffer = NULL;
void *preallocateCodec = NULL;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite titleSprite = TFT_eSprite(&tft);
TFT_eSprite timeSprite = TFT_eSprite(&tft);
TFT_eSprite ampSprite = TFT_eSprite(&tft);
TFT_eSprite stationSprite = TFT_eSprite(&tft);
TFT_eSprite brandSprite = TFT_eSprite(&tft);
TFT_eSprite volumeSprite = TFT_eSprite(&tft);

TFT_eSprite cursorSprite = TFT_eSprite(&tft);
bool cursor = false;

// scroll
uint16_t array_pos = 0;
unsigned long nowMillis;
unsigned long startMillis;
uint8_t scrolldelay = 12;
int tcount;
int scrollPos;
String blank;
const uint8_t fontwidth = 16;
uint8_t titleDeltaY = TITLE_DELTA_INITIAL;
uint8_t bgImage = 1;
long lowestBgChangeTimeMs;
long highstBgUnChangeTimeMs;
int tftBgRow = 0;
#ifdef RESTORE_BG_RAND_LINES
uint8_t bgRowOrder[60];
#else
uint8_t bgRowOrder[10];
#endif
int rowFactor;
uint8_t showPos = 0;
const uint8_t typeSpeed = 0x7f;

// forwards
void display(bool);
void restoreBg(int y, int height);
void showPlayTime();
void stopPlaying();
void startPlaying();
void switchStation(int direction);
void setVolume(int direction);

struct rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t dr;
  uint8_t dg;
  uint8_t db;
};

uint8_t za, zb, zc, zx, counter;
rgb amp_colors[((135 - 32) >> 2) + 1];
rgb rgb_color;

uint8_t rnd() {
  zx++;
  za = (za ^ zc ^ zx);
  zb = (zb + za);
  zc = ((zc + (zb >> 1)) ^ za);
  return zc;
}

uint32_t rgb24to565(struct rgb *c) {
  return ((c->r & 0xf8) << 8) | ((c->g & 0xfc) << 3) | (c->b >> 3);
}

String resultToString(int result) {
  switch (result) {
    case RESULT_CLICK: return "click";
    case RESULT_DOUBLE_CLICK: return "double click";
    case RESULT_LONG_CLICK: return "long click";
    default: return "<undef>";
  }
}

void showCursor(int x, int y) {
  if ((globalCnt & 0x3f) == 0x3f) {
    cursor = !cursor;
    if (cursor) {
      cursorSprite.createSprite(4, 12);
      cursorSprite.fillRect(0, 0, 4, 12, TFT_WHITE);
    } else {
      cursorSprite.createSprite(4, 12);
      cursorSprite.fillRect(0, 0, 4, 12, TFT_BLACK);
    }
    cursorSprite.pushSprite(x, y);
  }
}

void typeWrite(String str, uint8_t y, uint16_t x) {
  ledcWrite(pwmLedChannelTFT, BRIGHTNESS);
  while (showPos < str.length()) {
    delay(2);
    showCursor((showPos * 12) + x + 2, y);
    if ((globalCnt & typeSpeed) == typeSpeed) {
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(2);
      tft.drawChar(str.charAt(showPos), (showPos * 12) + x - 1 + (rnd() >> 7), y + (rnd() >> 5) - 4, 1);
      showPos++;
    }
    globalCnt++;
  }
  showPos = 0;
}

void initTitle() {
  streamingForMs = 0;
  lastms = millis();
  titleDeltaY = TITLE_DELTA_INITIAL;
  title = "";
}

class ButtonCallback : public TtgoButton::ButtonCallback {
public:
  void onButtonPressed(const int &btnPin, const int &result) override {
#ifdef USE_SERIAL_OUT
    Serial.println("btn " + String(btnPin) + ": " + resultToString(result));
#endif
    if (tftOff) {
      display(true);
      return;
    }
    tftOffTimer = millis() + TFT_OFF_TIMEOUT;
    if (btnPin == BTN0) {
      switch (result) {
        case RESULT_CLICK:
          {
            if (!playFlag) {
              initTitle();
              targetGain = lastGain;
              if (targetGain < 1.0f) targetGain = 1.0f;
              deltaGain = 0.1f;
              startPlaying();
            } else {
              targetGain = 0.0f;
              deltaGain = -0.35f;
              stopRequested = true;
            }
          };
          break;
        case RESULT_LONG_CLICK: setVolume(-1); break;
        case RESULT_DOUBLE_CLICK: break;
      }
    } else {
      switch (result) {
        case RESULT_CLICK:
          if (playFlag) setVolume(1.0f);
          else switchStation(1);
          break;
        case RESULT_LONG_CLICK: setVolume(1); break;
        case RESULT_DOUBLE_CLICK:
          if (playFlag) setVolume(-1);
          else switchStation(-1);
          break;
      }
    }
  }
};

ButtonCallback btnCallback;
TtgoButton btn0(BTN0, &btnCallback);
TtgoButton btn1(BTN1, &btnCallback);

void fade(bool in) {
  if (in) {
    tftBrightness = 0;
    while (tftBrightness < BRIGHTNESS) {
      tftBrightness += 3;
      ledcWrite(pwmLedChannelTFT, tftBrightness);
      delay(5);
    }
  } else {
    while (tftBrightness >= 3) {
      tftBrightness -= 3;
      ledcWrite(pwmLedChannelTFT, tftBrightness);
      delay(3);
    }
  }
}

void setup() {
#ifdef USE_SERIAL_OUT
  Serial.begin(115200);
  while (!Serial) delay(1);
#endif
  za = random(256);
  zb = random(256);
  zc = random(256);
  zx = random(256);
  preallocateBuffer = malloc(preallocateBufferSize);
  preallocateCodec = malloc(preallocateCodecSize);
  if (!preallocateBuffer || !preallocateCodec) {
    Serial.printf_P(PSTR("FATAL ERROR:  Unable to preallocate %d bytes\n"), preallocateBufferSize + preallocateCodecSize);
    while (1) delay(1000);  // Infinite halt
  }
  tft.init();
  tft.fillRect(0, 0, tft.width(), tft.height(), TFT_BLACK);
  tft.setSwapBytes(true);
  ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);
  ledcAttachPin(TFT_BL, pwmLedChannelTFT);
#ifdef DO_GREETINGS
  typeWrite(GREET_0, (tft.height() >> 1) - 28, 2);
  typeWrite(GREET_1, (tft.height() >> 1), 60);
  typeWrite(GREET_2, (tft.height() >> 1) + 36, 12);
  delay(1000);
#endif
  tft.setTextSize(1);
  ledcWrite(pwmLedChannelTFT, 0);
  tft.setRotation(0);
  display(true);
  initWiFi();
  btn0.SetLongPressRepeatMillis(180);
  btn1.SetLongPressRepeatMillis(180);
  rowFactor = tft.height() / sizeof(bgRowOrder);
#ifdef CREDITS_DISPLAY
  showCredits();
#endif
  readPreferences();
  switchStation(0);
  if (targetGain == 0.0f) {
    targetGain = 10.0f;
  }
  drawBasicLabels();
  startMillis = millis();
  titleSprite.setTextWrap(false);
  titleSprite.setTextSize(2);
  timeSprite.setTextWrap(false);
  timeSprite.setTextFont(2);
  tftOffTimer = millis() + TFT_OFF_TIMEOUT;
  lowestBgChangeTimeMs = millis() + MIN_BG_SWITCH_MS;
  highstBgUnChangeTimeMs = millis() + MAX_BG_SAME_MS;
  rgb_color = { (uint8_t)(120 - (rnd() >> 2)), (uint8_t)(130 - (rnd() >> 2)), (uint8_t)(100 - (rnd() >> 2)), (uint8_t)(rnd() >> 5), (uint8_t)(rnd() >> 6), (uint8_t)(rnd() >> 4) };
  for (int n = 0; n < sizeof(amp_colors) / sizeof(rgb); n++) {
    amp_colors[n] = rgb_color;
  }
}

void writePreferences() {
  Preferences preferences;
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putInt("station", station);
  preferences.putInt("gain", (int)targetGain);
  preferences.end();
}

void readPreferences() {
  Preferences preferences;
  preferences.begin(PREF_NAMESPACE, true);
  station = preferences.getInt("station");
  targetGain = (float)preferences.getInt("gain");
  preferences.end();
  if (targetGain == 0) {
    targetGain = 10;
  }
  lastGain = targetGain;
  preferences.end();
}

void showCredits() {
  tft.setFreeFont(NULL);
  tft.setCursor(0, 20, 2);
  tft.setFreeFont(&Orbitron_Medium_20);
  tft.fillScreen(backGroundColor);
  tft.setCursor(0, 60);

  tft.println("Volos\nprojects");
  tft.setFreeFont(NULL);
  tft.drawString("youtube.com/", 2, 180, 2);
  tft.drawString("@VolosProjects", 24, 200, 2);
  tft.drawString("inspired by", 8, 24, 2);
  fade(true);
  delay(DELAY_START_UP);
  fade(false);
}

void drawBox(String str, int y, int bgcolor) {
  tft.setFreeFont(NULL);
  tft.fillRoundRect(2, y - 2, 64, 19, 3, backGroundColor);
  tft.setTextColor(foreGroundColor);
  tft.setTextSize(1);
  tft.setCursor(6, y - 1, 2);
  tft.print(str);
  tft.setTextColor(foreGroundColor);
}

void drawBasicLabels() {
#ifdef SHOW_BRAND
  brandSprite.createSprite(tft.width(), 24);
  brandSprite.setSwapBytes(true);
  brandSprite.pushImage(0, 0, tft.width(), 24, &bg_img[bgImage][0]);
  brandSprite.setTextSize(1);
  brandSprite.setFreeFont(&Orbitron_Medium_20);
  brandSprite.setCursor(7, 20);
  brandSprite.print("vincRadio");
  brandSprite.pushSprite(0, 0);
#endif
  showPlayTime();
  showStationAndVolume();
  showPlayTime();
}

void showPlayTime() {
  String str = "";
  if (streamingForMs > 0) {
    uint32_t t = streamingForMs / 1000;
    uint8_t sec = t % 60;
    uint8_t min = (t / 60) % 99;
    str += (min < 10 ? "0" : "") + String(min);
    str += (sec < 10 ? ":0" : ":") + String(sec);
    if (str != lastPlayTime) {
      lastPlayTime = str;
      timeSprite.createSprite(tft.width(), 48);
      timeSprite.setSwapBytes(true);
      timeSprite.pushImage(0, 0, tft.width(), 48, &bg_img[bgImage][tft.width() * Y_TIME]);
      timeSprite.setTextColor(TFT_WHITE);
      timeSprite.drawCentreString(str, tft.width() >> 1, 0, TIME_FONT);
      timeSprite.pushSprite(0, Y_TIME);
    }
  }
}


#ifdef USE_BG_SCROLLER
void showTitle() {
  if (nowMillis - startMillis >= scrolldelay) {
    int height = 26;
    int y = tft.height() - height;
    titleSprite.createSprite(tft.width(), height);
    titleSprite.setSwapBytes(true);
    rgb c;
    int sRow = 0;
    int idx = tft.width() * 214;
    for (int row = 0; row < titleSprite.height(); row++) {
      for (int col = 0; col < tft.width(); col++) {
        unsigned short val = bg_img[bgImage][idx];
        c.r = ((((val & 0xf800) >> 11) << 3) >> 2) + ((((val & 0xf800) >> 11) << 3) >> 1);
        c.g = ((((val & 0x07e0) >> 5) << 2) >> 2) + ((((val & 0x07e0) >> 5) << 2) >> 1);
        c.b = ((((val & 0x1f)) << 3) >> 2) + ((((val & 0x1f)) << 3) >> 1);
        titleSprite.drawPixel(col, sRow, rgb24to565(&c));
        idx++;
      }
      sRow++;
    }
    titleSprite.setTextFont(2);
    titleSprite.setTextSize(2);
    titleSprite.setCursor(scrollPos, -2);
    titleSprite.setTextColor(TFT_WHITE);
    titleSprite.print(title);
    scrollPos--;
    if (scrollPos < -(titleLength * fontwidth)) {
      scrollPos = tft.width();
    }
    titleSprite.pushSprite(0, Y_SCROLL - 9 + titleDeltaY);
    if (titleDeltaY > 0 && ((globalCnt & 0x3) == 0x3)) {
      titleDeltaY--;
    }
    startMillis = nowMillis;
  }
}

#else

void showTitle() {
  uint16_t arraysize = title.length() + 1;
  char string_array[arraysize];
  title.toCharArray(string_array, arraysize);
  if (array_pos >= arraysize) {
    array_pos = 0;
  }
  titleSprite.createSprite(tft.width() + fontwidth, 26);
  if (nowMillis - startMillis >= scrolldelay) {
    titleSprite.pushSprite(0, Y_SCROLL - 2 + titleDeltaY);
    if (titleDeltaY > 0 && ((globalCnt & 0x3) == 0x3)) {
      titleDeltaY--;
    }
    titleSprite.scroll(-1);
    tcount--;
    if (tcount <= 0) {
      char x = string_array[array_pos];
      tcount = fontwidth;
      titleSprite.setTextSize(2);
      titleSprite.setTextColor(foreGroundColor);
      titleSprite.drawString(String(x), tft.width(), 2, 1);
      array_pos++;
    }
    startMillis = nowMillis;
  }
}
#endif

void restoreBg(int y, int height) {
  tft.pushImage(0, y, tft.width(), height, &bg_img[bgImage][tft.width() * y]);
}

void showStationAndVolume() {
  stationSprite.createSprite(tft.width(), 28);
  rgb c;
  int sRow = 0;
  int idx = tft.width() * 24;
  for (int row = 0; row < stationSprite.height(); row++) {
    for (int col = 0; col < tft.width(); col++) {
      unsigned short val = bg_img[bgImage][idx + col];
      c.r = (((val & 0xf800) >> 11) << 3) >> 1;
      c.g = (((val & 0x07e0) >> 5) << 2) >> 1;
      c.b = (((val & 0x1f)) << 3) >> 1;
      idx++;
      stationSprite.drawPixel(col, sRow, rgb24to565(&c));
    }
    sRow++;
  }
  float length = (tft.width() - 32) * (targetGain / MAX_GAIN);
  stationSprite.fillRect(16, 22, length, 3, TFT_WHITE);
  stationSprite.setTextColor(foreGroundColor);
  stationSprite.setTextSize(2);
  stationSprite.drawCentreString(stations[station].name, tft.width() >> 1, 3, 1);
#ifdef SHOW_BRAND
  stationSprite.pushSprite(0, Y_STATION);
#else
  stationSprite.pushSprite(0, Y_STATION - 12);
#endif
}

void alterColors(uint8_t *col, uint8_t *delta) {
  (*col) += (*delta);
  if ((*col) < 128) {
    (*col) = 128;
    (*delta) = (rnd() >> 5) + 1;
  } else if ((*col) > 224) {
    (*col) = 224;
    (*delta) = -((rnd() >> 5) + 1);
  }
}

void showAmpAni() {
  int height = 44;
  ampSprite.createSprite(tft.width(), height);
  ampSprite.setSwapBytes(true);
  ampSprite.pushImage(0, 0, tft.width(), height, &bg_img[bgImage][tft.width() * AMP_ANI_Y]);
  int x = 0;
  int r = rnd() >> 5;
  int ddr = 3;
  int dr;
#ifdef AMP_COLORFUL
  for (int n = sizeof(amp_colors) / sizeof(rgb) - 2; n >= 0; n--) {
    amp_colors[n] = amp_colors[n + 1];
  }
  rgb_color = { (uint8_t)(140 - (rnd() >> 2)), (uint8_t)(160 - (rnd() >> 2)), (uint8_t)(170 - (rnd() >> 2)), (uint8_t)(rnd() >> 5), (uint8_t)(rnd() >> 6), (uint8_t)(rnd() >> 4) };
  amp_colors[sizeof(amp_colors) / sizeof(rgb) - 1] = rgb_color;
#endif
  int col = 0;
  while (x < tft.width() - 32 && (col < (sizeof(amp_colors) / sizeof(rgb)))) {
    dr = (rnd() & 0x7) - ddr;
    r += dr;
    if (r > (height >> 1)) {
      r = height >> 1;
    } else if (r < 4) {
      r = 4;
    }
    if (x > (tft.width() >> 1) - 16) ddr = 4;
    else if (x > (tft.width() - 48)) ddr = 5;
#ifdef AMP_COLORFUL
    uint32_t col32 = rgb24to565(&amp_colors[col]);
    ampSprite.drawFastVLine(x + 16, (height >> 1) - r, r << 1, col32);
    ampSprite.drawFastVLine(x + 18, (height >> 1) - r, r << 1, col32);
#else
    ampSprite.drawFastVLine(x + 16, (height >> 1) - r, r << 1, TFT_LIGHTGREY);
    ampSprite.drawFastVLine(x + 18, (height >> 1) - r, r << 1, TFT_LIGHTGREY);
#endif
    x += 4;
  }
  ampSprite.pushSprite(0, AMP_ANI_Y);
}

void setVolume(int direction) {
  targetGain = targetGain + (float)direction;
  if (targetGain > MAX_GAIN) {
    targetGain = MAX_GAIN;
  } else if (targetGain < 0.0f) {
    targetGain = 0.0f;
  }
  fgain = targetGain;
  lastGain = targetGain;
  if (out) out->SetGain(fgain * 0.05);
  writePreferences();
  showStationAndVolume();
#ifdef USE_SERIAL_OUT
  Serial.printf("STATUS(Gain) %f \n", targetGain * 0.05);
#endif
}

void switchStation(int direction) {
  station = station + direction;
  if (station < 0) station = (sizeof(stations) / sizeof(Station)) - 1;
  if (station > (sizeof(stations) / sizeof(Station)) - 1) station = 0;
  writePreferences();
#ifdef USE_STATION_GAIN
  targetGain = stations[station].gain;
  showStation();
#endif
  streamingForMs = 0;
  showStationAndVolume();
}

void handlePlay() {
  if ((globalCnt & 0x1ff) == 0x1ff) {
    showPlayTime();
#ifdef USE_BG_SCROLLER    
    if ((globalCnt & 0x3f) == 0x3f) showAmpAni();
#else
    if ((globalCnt & 0x1ff) == 0x1ff) showAmpAni();
#endif    
    if (fgain != targetGain) {
      fgain = fgain + deltaGain;
      if (((fgain > targetGain) && (deltaGain > 0.05f)) || ((fgain < targetGain) && (deltaGain < 0.05f))) {
        fgain = targetGain;
      }
      out->SetGain(fgain * 0.05);
    }
  }
  if (decoder->isRunning()) {
    if (playFlag) streamingForMs += (millis() - lastms);
    lastms = millis();
  }
  if (!decoder->loop()) {
    decoder->stop();
  }
  showTitle();
}

void display(bool on) {
  tftOff = !on;
  deltaBrightness = on ? 2 : -1;
  if (on) tftOffTimer = millis() + TFT_OFF_TIMEOUT;
  else {
#ifdef USE_SERIAL_OUT
    Serial.println("TFT off");
#endif
  }
}

void displayBrightness() {
  if ((globalCnt & 0xff) == 0xff) {
    tftBrightness += deltaBrightness;
    if (tftBrightness > BRIGHTNESS) {
      deltaBrightness = 0;
      tftBrightness = BRIGHTNESS;
    } else if (tftBrightness <= 0) {
      deltaBrightness = 0;
      tftBrightness = 0;
    }
    ledcWrite(pwmLedChannelTFT, tftBrightness);
  }
}

void initWiFi() {
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
#ifdef USE_SERIAL_OUT
  Serial.printf("\nSTATUS(Connecting to WiFi) %s", SSID);
#endif
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
#ifdef USE_SERIAL_OUT
    Serial.print(".");
#endif
    i = i + 1;
    if (i > 50) {
#ifdef USE_SERIAL_OUT
      Serial.println("\nUnable to connect\nRetrying ");
      i = 0;
      // SSID / PW correct?
#endif
    }
  }
#ifdef USE_SERIAL_OUT
  Serial.println("\nOK");
#endif
}

void startPlaying() {
  lowestBgChangeTimeMs = millis() + MIN_BG_SWITCH_MS;
  highstBgUnChangeTimeMs = millis() + MAX_BG_SAME_MS;
  decoder = stations[station].isAAC ? (AudioGenerator *)new AudioGeneratorAAC(preallocateCodec, preallocateCodecSize)
                                    : (AudioGenerator *)new AudioGeneratorMP3(preallocateCodec, preallocateCodecSize);
  stream = new AudioFileSourceICYStream(stations[station].url);
  stream->RegisterMetadataCB(MDCallback, (void *)"ICY");
  buf = new AudioFileSourceBuffer(stream, BUF_SIZE);
  buf->RegisterStatusCB(StatusCallback, (void *)"buffer");
  out = new AudioOutputI2S(0, 1);
  out->SetOutputModeMono(!USE_STEREO);  // set to true if you don't need stereo
  fgain = 1.0;
  targetGain = lastGain;
  deltaGain = 0.1f;
  decoder->RegisterStatusCB(StatusCallback, (void *)"mp3");
  decoder->begin(buf, out);
  isStopped = false;
  playFlag = true;
  showStationAndVolume();
#ifdef USE_SERIAL_OUT
  Serial.printf("STATUS(URL) %s \n", stations[station].url);
  Serial.flush();
#endif
}

void stopSmooth() {
  if (!isStopped) {
    handlePlay();
    if (fgain <= 0.2f) {
      stopPlaying();
      targetGain = lastGain;
      showStationAndVolume();
      isStopped = true;
    }
  }
}

void stopPlaying() {
  restoreBg(AMP_ANI_Y - 1, 64);
  streamingForMs = 0;
  showPlayTime();
  restoreBg(tft.height() - 24, 24);
  if (decoder) {
    decoder->stop();
    delete decoder;
    decoder = NULL;
  }
  if (buf) {
    buf->close();
    delete buf;
    buf = NULL;
  }
  if (stream) {
    stream->close();
    delete stream;
    stream = NULL;
  }
  playFlag = false;
#ifdef USE_SERIAL_OUT
  Serial.printf("STATUS(Stopped)\n");
  Serial.flush();
#endif
}

void bgRepaint() {  // hack to avoid sound ticks, will use random line order if RESTORE_BG_RAND_LINES is defined
#ifdef RESTORE_BG_RAND_LINES
  if ((globalCnt & 0x7f) != 0x7f) return;
#else
  if ((globalCnt & 0x3f) != 0x3f) return;
#endif
  if (tftBgRow > sizeof(bgRowOrder) || tftBgRow < 0) return;
  if (tftBgRow == 0) createBgRestoreOrder();
  int row = bgRowOrder[tftBgRow];
  restoreBg(row * rowFactor, rowFactor);
  tftBgRow++;
  if (tftBgRow >= sizeof(bgRowOrder)) drawBasicLabels();
}

void createBgRestoreOrder() {
  for (int n = 0; n < sizeof(bgRowOrder); n++) bgRowOrder[n] = n;
#ifdef RESTORE_BG_RAND_LINES
  for (int n = 0; n < (sizeof(bgRowOrder) >> 1); n++) {
    uint8_t first = rnd() >> 2;
    if (first >= sizeof(bgRowOrder)) first -= sizeof(bgRowOrder);
    uint8_t second = rnd() >> 2;
    if (second >= sizeof(bgRowOrder)) second -= sizeof(bgRowOrder);
    uint8_t swap = bgRowOrder[first];
    bgRowOrder[first] = bgRowOrder[second];
    bgRowOrder[second] = swap;
  }
#endif
}

void nextBackground() {
  bgImage++;
  if (bgImage >= (sizeof(bg_img) / sizeof(bg_img[0]))) bgImage = 0;
  tftBgRow = 0;
}

void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void)isUnicode;
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2) - 1] = 0;
  title = String(s2);
  streamingForMs = 0;
  bool changeBg = (title != lastTitle) && !tftOff && (millis() > lowestBgChangeTimeMs);
  if (changeBg) {
    lowestBgChangeTimeMs = millis() + MIN_BG_SWITCH_MS;
    highstBgUnChangeTimeMs = millis() + MAX_BG_SAME_MS;
#ifndef SWITCH_BG_WHEN_PAUSED
    if (playFlag)
#endif
      nextBackground();
  }
  lastTitle = title;
  titleLength = title.length();
  if (titleLength == 0) {
    title = "Wait for title info";
    titleLength = title.length();
  }
  scrollPos = tft.width();
#ifdef USE_SERIAL_OUT
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  Serial.flush();
#endif
}

void StatusCallback(void *cbData, int code, const char *string) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
#ifdef USE_SERIAL_OUT
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
#endif
}

void loop() {
  nowMillis = millis();
  bgRepaint();
  if (millis() > highstBgUnChangeTimeMs) {
    highstBgUnChangeTimeMs = millis() + MAX_BG_SAME_MS;
#ifndef SWITCH_BG_WHEN_PAUSED
    if (playFlag)
#endif
      nextBackground();
  }
#ifndef TFT_ALWAYS_ON
  if (!tftOff && (millis() > tftOffTimer)) display(false);
#endif
  displayBrightness();
  if (stopRequested) stopSmooth();
  else if (playFlag) handlePlay();
  btn0.Listen();
  btn1.Listen();
  globalCnt++;
}