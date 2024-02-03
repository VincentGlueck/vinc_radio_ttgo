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
#include "stations.h"
#include "frame.h"
#include "Orbitron_Medium_20.h"

#define USE_SERIAL_OUT
// this will flood the console and slow down things dramatically
// consider going to 115.200baud rate if you need debug infos

#define TFT_GREY 0x5AEB
#define BRIGHTNESS 220        // brightness during display = on
#define TFT_OFF_TIMEOUT 120   // increase/decrease to tweak power consume (diplay off timer) // defaults to 20
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


const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;

const int BTN0 = 0;  // standard TTGO built-in buttons
const int BTN1 = 35;

bool TestMode = false;
uint32_t LastTime = 0;
bool playflag = false;
int station = 0;
float fgain = fgain = stations[station].gain;
String title, lastTitle = "?";
int titleScroll = 134;
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
  tft.fillScreen(TFT_BLACK);

  tft.setCursor(0, 20, 2);
  tft.println("WIFI");
  tft.drawString(SSID, 1, 44, 2);

  init_wifi();
#ifdef CONNECT_DISPLAY
  delay(DELAY_START_UP);
#endif

  tft.fillScreen(TFT_BLACK);
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

  initial_setup();
}

void show_credits() {
  tft.setFreeFont(NULL);
  tft.setCursor(0, 20, 2);
  tft.setFreeFont(&Orbitron_Medium_20);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 60);

  tft.println("Volos\nprojects");
  tft.setFreeFont(NULL);
  tft.drawString("youtube.com/", 2, 180, 2);
  tft.drawString("@VolosProjects", 24, 200, 2);
  tft.drawString("inspired by", 8, 24, 2);
  delay(DELAY_START_UP >> 2);
}

void draw_box(String str, int y, int bgcolor, int color) {
  tft.setFreeFont(NULL);
  tft.fillRoundRect(2, y-1, 64, 16, 3, bgcolor);
  tft.setTextColor(color, bgcolor);
  tft.setTextSize(1);
  tft.setCursor(6, y-1, 2);
  tft.print(str);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

void initial_setup() {
  tft.setTextSize(1);
  tft.fillScreen(TFT_BLACK);
  tft.setFreeFont(&Orbitron_Medium_20);
  tft.setCursor(2, 20);
  tft.println("vincRadio");
  draw_status("Ready", Y_STATUS);
  draw_status(String(fgain), 66);
  tft.drawString(stations[station].name, 12, 110, 2);
  draw_box("Status", Y_STATUS, TFT_BLUE, TFT_WHITE);
  draw_box("Volume", Y_VOLUME, TFT_DARKGREY, TFT_WHITE);
  draw_box("Time", Y_TIME, TFT_BROWN, TFT_BLACK);
  tft.setFreeFont(NULL);
  tft.setTextSize(2);
}

void draw_status(String status, int pos) {
  tft.fillRect(78, pos, 135 - 78, 16, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.drawString(status, 78, pos, 2);
}

void show_play_time() {
  if (streamingForMs == 0) {
    tft.fillRect(78, 91, 135 - 78, 14, TFT_BLACK);
    tft.setTextSize(1);
    tft.fillRect(78, 91, 135 - 78, 16, TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("--:--", 78, 90, 2);
    return;
  }
  uint32_t t = streamingForMs >> 10;
  uint8_t hour = t / 3600;
  String str = "";
  if (hour > 0) {
    str += (String(hour)) + ":";
  }
  str += String(t / 60) + ":";
  uint8_t sec = t % 60;
  str += (sec < 10 ? "0" : "") + String(sec);
  if (str != lastPlayTime) {
    tft.setTextSize(1);
    tft.fillRect(78, 91, 135 - 78, 16, TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(str, 78, 90, 2);
    lastPlayTime = str;
  }
}

void loop() {
  if ((globalCnt & 0x3f) == 0x3f) {
    do_display_brightness();
  }
  if (tftOff && ((globalCnt & 0x1f) != 0x1f)) {
    if ((digitalRead(BTN0) == LOW) || (digitalRead(BTN1) == LOW)) {
      while (((digitalRead(BTN0) == LOW) || (digitalRead(BTN1) == LOW))) delay(1);
      tftOffTimer = 0;
      display(true);
    }
  }

  if (!tftOff && ((globalCnt & 0x3f) != 0x3f)) {
    if (playflag) {
      tft.pushImage(50, 126, animation_width, animation_height, frame[frameCnt++]);
      if (frameCnt == frames) frameCnt = 0;
    } else {
      tft.pushImage(50, 126, animation_width, animation_height, frame[frames - 1]);
    }
  }
  globalCnt++;
  if (!tftOff) {
    if ((globalCnt & 0x1ff) == 0x1ff) {
      tftOffTimer++;
      if (tftOffTimer > TFT_OFF_TIMEOUT) {
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
      tft.setTextSize(1);
      tft.setFreeFont(NULL);
      tft.fillRect(0, 110, 135, 16, TFT_BLACK);
      tft.drawString(stations[station].name, 2, 110, 2);
    }
  }

  if (playflag) {
    if ((globalCnt & 0x3f) == 0x3f) {
      show_play_time();
    }
    if (mp3->isRunning()) {
      if(playflag) streamingForMs += millis() - lastms;
      lastms = millis();
    }
    if (!mp3->loop()) {
      mp3->stop();
    }
  }

  if (playflag && !tftOff) {
    if (lastTitle != title) {
      titleScroll = 12;
      lastTitle = title;
    }
    if ((globalCnt & 0x1f) == 0x1f) {
      titleScroll -= 8;
      if (titleScroll < -500) {
        titleScroll = 134;
      }
      tft.setFreeFont(NULL);
      tft.fillRect(0, 209, 135, 240 - 209, TFT_BLACK);
      tft.drawString(title, titleScroll, 209, 4);
    }

  } else {
#ifdef USE_SERIAL_OUT
    // Serial.printf("MP3 done\n");
#endif
    streamingForMs = 0;
  }
  if (!tftOff && digitalRead(BTN0) == LOW) {
    streamingForMs = 0;
    show_play_time();
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
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
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
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
#ifdef USE_SERIAL_OUT
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
#endif
}