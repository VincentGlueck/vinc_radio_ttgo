// https://github.com/VincentGlueck/ttgo_radio.git

/*************************************************************
* REMEMBER
*
* in libraries/TFT_eSPI activate TTGO in User_Setup_Select.h
* (line 58 approx.) and comment out //#include <User_Setup.h>
*
**************************************************************/
/*
Default PIN layout
==================

#define TFT_MOSI            19
#define TFT_SCLK            18
#define TFT_CS              5
#define TFT_DC              16
#define TFT_RST             23
#define TFT_BL              4 

*** TOUCH_CS *** is undefined as TTGO does not support touch, ignore warnings
*/

#ifndef ESP32
#error "Requires ESP32, TTGO TFT display (135x240 pixels)"
#endif

#define TFT_W 135
#define TFT_H 240

#include <WiFi.h>
#include <AudioFileSource.h>
#include <AudioFileSourceBuffer.h>
#include <AudioFileSourceICYStream.h>
#include <AudioGeneratorTalkie.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <AudioOutputI2SNoDAC.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <spiram-fast.h>
#include "Orbitron_Medium_20.h"
#include "stations.h"
#include "beetle.h"
#include "colors.h"

#define USE_SERIAL_OUT
// this will flood the console and slow down things dramatically
// consider going to 115.200baud rate if you need debug infos
#define DEEP_SERIAL_OUT
// this even more

#define BRIGHTNESS 220        // brightness during display = on
#define TFT_OFF_TIMEOUT 1200  // increase/decrease to tweak power consume (diplay off timer) // defaults to 20
#define CONNECT_DISPLAY       // uncomment to speed things up at start
#define CREDITS_DISPLAY       // uncomment to be unkind ;-)
#define DELAY_START_UP 300

/**********************
* OUTPUT L/R:  PIN26  *
* MONO !!! ONLY !!!   *
* connect GND!!!      *
 *********************/

const char *SSID = "VincentVega01";
const char *PASSWORD = "winter01";

#define Y_STATUS 44
#define Y_VOLUME 66
#define Y_TIME 88
#define Y_SCROLL 224
#define X_FRAME 38
#define Y_FRAME 134

const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;

const int BTN0 = 0;  // standard TTGO built-in buttons
const int BTN1 = 35;

uint8_t color = 0;
uint16_t foreGroundColor = foregroundColors[color];
uint16_t backGroundColor = TFT_BLACK;
bool TestMode = false;
uint32_t LastTime = 0;
bool playflag = false;
int station = 0;
float fgain = fgain = stations[station].gain;
String title, lastTitle = "?";
int titleScroll = 135;
int globalCnt, frameCnt;
uint16_t tftOffTimer = 0;
bool tftOff = false;
uint8_t tftBrightness = BRIGHTNESS;
short deltaBrightness = 0;
uint8_t titleLength;
uint32_t lastms = 0;
uint32_t streamingForMs = 0;
String lastPlayTime = "x";

AudioGeneratorTalkie *talkie;
AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buf;
AudioOutputI2S *out;
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite stext = TFT_eSprite(&tft);
TFT_eSprite ttext = TFT_eSprite(&tft);

// scroll
uint16_t array_pos = 0;
unsigned long nowMillis;
unsigned long startMillis;
uint8_t scrolldelay = 12;
int tcount;
String blank;
const uint8_t fontwidth = (16);

void setup() {
#ifdef USE_SERIAL_OUT
  Serial.begin(115200);
  while (!Serial) delay(1);
#endif
  pinMode(BTN0, INPUT);
  pinMode(BTN1, INPUT);

  tft.init();
  tft.setRotation(0);
  tft.setSwapBytes(true);
  display(true);

  tft.setFreeFont(&Orbitron_Medium_20);
  tft.fillScreen(backGroundColor);

  tft.setCursor(0, 20, 2);
  tft.println("WIFI");
  tft.drawString(SSID, 1, 44, 2);

  init_wifi();
#ifdef CONNECT_DISPLAY
  delay(DELAY_START_UP);
#endif

  tft.fillScreen(backGroundColor);
  tft.setCursor(0, 60);
  tft.println("Done");
  tft.setFreeFont(NULL);
  tft.setTextSize(1);
  tft.print(WiFi.localIP());

#ifdef CONNECT_DISPLAY
  delay(DELAY_START_UP);
#endif

#ifdef CREDITS_DISPLAY
  show_credits();
#endif

  ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);
  ledcAttachPin(TFT_BL, pwmLedChannelTFT);

  out = new AudioOutputI2S(0, 1);
  out->SetOutputModeMono(true);
  out->SetGain(fgain * 0.05);

  initialSetup();
  startMillis = millis();
  stext.setTextWrap(false);  // Don't wrap text to next line
  stext.setTextSize(2);      // larger letters
  ttext.setTextWrap(false);
  ttext.setTextFont(2);
}

void show_credits() {
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
  delay(DELAY_START_UP >> 2);
}

void draw_box(String str, int y, int bgcolor) {
  tft.setFreeFont(NULL);
  tft.fillRoundRect(2, y - 1, 64, 16, 3, foreGroundColor);
  tft.setTextColor(backGroundColor);
  tft.setTextSize(1);
  tft.setCursor(6, y - 1, 2);
  tft.print(str);
  tft.setTextColor(foreGroundColor);
}

void initialSetup() {
  tft.setTextSize(1);
  tft.pushImage(0, 0, 135, 240, bg_img);
  tft.setFreeFont(&Orbitron_Medium_20);
  tft.setCursor(2, 20);
  tft.println("vincRadio");
  drawLabels();
  showPlayTime();
}

void drawLabels() {
  tft.setTextColor(foreGroundColor);
  draw_status("Ready", Y_STATUS);
  draw_status(String(fgain), 66);
  showStation();
  draw_box("Status", Y_STATUS, backGroundColor);
  draw_box("Volume", Y_VOLUME, backGroundColor);
  draw_box("Time", Y_TIME, backGroundColor);
  tft.setFreeFont(NULL);
  tft.setTextSize(2);
}

void draw_status(String status, int pos) {
  tft.fillRoundRect(72, pos - 1, 135 - 76, 18, 3, backGroundColor);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(foreGroundColor, backGroundColor);
  tft.drawString(status, 78, pos, 2);
}

void showPlayTime() {
  String str = "";
  ttext.createSprite(tft.width() - 74 - 6, 16);
  ttext.fillRoundRect(0, 0, tft.width() - 74 - 4, 14, 3, backGroundColor);
  if (streamingForMs == 0) {
    str = "--:--";
  } else {
    uint32_t t = streamingForMs >> 10;
    uint8_t hour = t / 3600;
    if (hour > 0) {
      str += (String(hour)) + ":";
    }
    str += String(t / 60) + ":";
    uint8_t sec = t % 60;
    str += (sec < 10 ? "0" : "") + String(sec);
    if (str != lastPlayTime) {
      lastPlayTime = str;
    }
  }
  ttext.setTextColor(foreGroundColor);
  ttext.drawString(str, 4, 0, 2);
  ttext.pushSprite(74, 90);
}

void scrollText() {
  uint16_t arraysize = title.length() + 1;
  char string_array[arraysize];
  title.toCharArray(string_array, arraysize);

  if (array_pos >= arraysize) {
    array_pos = 0;
  }
  stext.createSprite(TFT_W + fontwidth, 32);
  stext.setTextFont(foreGroundColor);
  if (nowMillis - startMillis >= scrolldelay) {
    stext.pushSprite(0, Y_SCROLL);
    stext.scroll(-1);
    tcount--;
    if (tcount <= 0) {
      char x = string_array[array_pos];
      tcount = fontwidth;
      stext.drawString(String(x), TFT_W, 0, 1);
      array_pos++;
    }
    startMillis = nowMillis;
  }
}

void restoreBg(int y, int height) {
  tft.pushImage(0, y, tft.width(), height, &bg_img[tft.width()*y]);
}

void showStation() {
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  restoreBg(108, 20);
  tft.drawRoundRect(0, 109, 135, 19, 3, TFT_RED);
  tft.drawString(stations[station].name, 4, 110, 2);
  tft.setTextColor(TFT_WHITE);
}


void loop() {
  nowMillis = millis();
  if ((globalCnt & 0x1f) == 0x1f) {
    do_display_brightness();
  }
  if (tftOff && ((globalCnt & 0x1f) != 0x1f)) {
    if ((digitalRead(BTN0) == LOW) || (digitalRead(BTN1) == LOW)) {
      while (((digitalRead(BTN0) == LOW) || (digitalRead(BTN1) == LOW))) delay(1);
      tftOffTimer = 0;
      display(true);
    }
  }

  globalCnt++;
  if (!tftOff) {
    if ((globalCnt & 0x1ff) == 0x1ff) {
      tftOffTimer++;
      if (tftOffTimer > (TFT_OFF_TIMEOUT << 4)) {
        display(false);
      }
    }
  }

  if (!playflag && !tftOff) {
    if (digitalRead(BTN0) == LOW) {
      tftOffTimer = 0;
      while (digitalRead(BTN0) == LOW) delay(1);
      draw_status("Playing", Y_STATUS);
      streamingForMs = 0;
      StartPlaying();
      playflag = true;
    }

    if (digitalRead(BTN1) == LOW) {
      tftOffTimer = 0;
      while (digitalRead(BTN1) == LOW) delay(1);
      station++;
      if (station >= (sizeof(stations) / sizeof(Station) - 1)) station = 0;
      fgain = stations[station].gain;
      streamingForMs = 0;
      showStation();
    }
  }

  if (playflag) {
    if ((globalCnt & 0x3f) == 0x3f) {
      showPlayTime();
    }
    if (mp3->isRunning()) {
      if (playflag) streamingForMs += millis() - lastms;
      lastms = millis();
    }
    if (!mp3->loop()) {
      mp3->stop();
    }
    scrollText();
  }

  if (playflag && !tftOff) {
    if (lastTitle != title) {
      lastTitle = title;
    }
  } else {
#ifdef USE_SERIAL_OUT
    // Serial.printf("MP3 done\n");
#endif
    streamingForMs = 0;
  }
  if (!tftOff && digitalRead(BTN0) == LOW) {
    streamingForMs = 0;
    showPlayTime();
    tftOffTimer = 0;
    while (digitalRead(BTN0) == LOW) delay(1);
    StopPlaying();
    playflag = false;
  }
  if (!tftOff && digitalRead(BTN1) == LOW) {
    tftOffTimer = 0;
    while (digitalRead(BTN1) == LOW) delay(1);
    fgain = fgain + 1.0;
    if (fgain > 10.0) {
      fgain = 1.0;
    }
    out->SetGain(fgain * 0.05);
    draw_status(String(fgain), 66);
#ifdef USE_SERIAL_OUT
    Serial.printf("STATUS(Gain) %f \n", fgain * 0.05);
#endif
  }
}

void display(bool on) {
  tftOff = !on;
  deltaBrightness = on ? 1 : -1;
}

void do_display_brightness() {
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

void init_wifi() {
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
#ifdef USE_SERIAL_OUT
    Serial.print("STATUS(Connecting to WiFi) ");
#endif
    delay(200);
    i = i + 1;
    if (i > 100) {
#ifdef USE_SERIAL_OUT
      Serial.println("Unable to connect");
      // SSID / PW correct?
#endif
    }
  }
#ifdef USE_SERIAL_OUT
  Serial.println("OK");
#endif
}

void StartPlaying() {
  file = new AudioFileSourceICYStream(stations[station].url);
  file->RegisterMetadataCB(MDCallback, (void *)"ICY");
  buf = new AudioFileSourceBuffer(file, 0x800);  // increase if needed
  buf->RegisterStatusCB(StatusCallback, (void *)"buffer");
  out = new AudioOutputI2S(0, 1);  // Output to builtInDAC
  out->SetOutputModeMono(true);
  out->SetGain(fgain * 0.05);
  mp3 = new AudioGeneratorMP3();
  mp3->RegisterStatusCB(StatusCallback, (void *)"mp3");
  mp3->begin(buf, out);
#ifdef USE_SERIAL_OUT
  Serial.printf("STATUS(URL) %s \n", stations[station].url);
  Serial.flush();
#endif
}

void StopPlaying() {
  if (mp3) {
    mp3->stop();
    delete mp3;
    mp3 = NULL;
  }
  if (buf) {
    buf->close();
    delete buf;
    buf = NULL;
  }
  if (file) {
    file->close();
    delete file;
    file = NULL;
  }
  draw_status("Stopped", Y_STATUS);
#ifdef USE_SERIAL_OUT
  Serial.printf("STATUS(Stopped)\n");
  Serial.flush();
#endif
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
  titleLength = title.length();
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
#ifdef DEEP_SERIAL_OUT
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
#endif
}