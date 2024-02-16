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
#include "Orbitron_Medium_20.h"
#include "stations.h"
//#include "bg_cat.h"
#include "bg_img.h"
#include "TtgoButton.h"
#include "colors.h"

#define USE_SERIAL_OUT
// this will flood the console and slow down things
// consider going to 115.200baud rate if you need debug infos
#define BUF_SIZE 0xc00         // size of streaming buffer (0x400 -> more decoding errors, 0x1000 -> default)
#define BRIGHTNESS 220         // brightness during display = on (max 255)
#define TFT_OFF_TIMEOUT 30000  // display will go off after xyz milliseconds
#define TFT_ALWAYS_ON          // comment out to activate display off
#define CREDITS_display        // uncomment to be unkind ;-)
#define DELAY_START_UP 1500    // starup credits/slow down
#define MIN_BG_SWITCH_MS 5000  // background switch on title change not before ms
#define MAX_BG_SAME_MS 25000   // background will force switch after ms
//#define USE_STATION_GAIN     // comment in to change volume to default station's volume on station switch

/**********************
* OUTPUT L/R:  PIN26  *
* MONO !!! ONLY !!!   *
* connect GND!!!      *
 *********************/

// TODO: enter your WiFi credentials, real one is not too secret, but sometimes I change it to *** shit //
const char *SSID = "VincentVega01";
const char *PASSWORD = "winter01";

#define Y_STATUS 44
#define Y_VOLUME 66
#define Y_TIME 88
#define Y_SCROLL 224
#define X_FRAME 38
#define Y_FRAME 134
#define TITLE_DELTA_INITIAL 24

#define MAX_GAIN 20.0f

const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;

uint8_t color = 0;
uint16_t foreGroundColor = foregroundColors[color];
uint16_t backGroundColor = TFT_BLACK;
bool playflag = false;
int station = 0;
float fgain = fgain = stations[station].gain;
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

AudioGeneratorTalkie *talkie;
AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buf;
AudioOutputI2S *out;
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite titleSprite = TFT_eSprite(&tft);
TFT_eSprite timeSprite = TFT_eSprite(&tft);
TFT_eSprite stationSprite = TFT_eSprite(&tft);

// scroll
uint16_t array_pos = 0;
unsigned long nowMillis;
unsigned long startMillis;
uint8_t scrolldelay = 12;
int tcount;
String blank;
const uint8_t fontwidth = 16;
uint8_t titleDeltaY = TITLE_DELTA_INITIAL;
uint8_t bgImage = 1;
long lowestBgChangeTimeMs;
long highstBgUnChangeTimeMs;
int tftBgRow = 0;

// forwards
void display(bool);
void restoreBg(int y, int height);
void ShowPlayTime();
void drawStatus(String status, int pos);
void stopPlaying();
void startPlaying();
void HandleVolumeChange();
void switchStation(int direction);
void setVolume(int direction);

String resultToString(int result) {
  switch (result) {
    case RESULT_CLICK: return "click";
    case RESULT_DOUBLE_CLICK: return "double click";
    case RESULT_LONG_CLICK: return "long click";
    default: return "<undef>";
  }
}

void initTitle() {
  drawStatus("Playing", Y_STATUS);
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
            if (!playflag) {
              initTitle();
              startPlaying();
              playflag = true;
            } else {
              streamingForMs = 0;
              ShowPlayTime();
              restoreBg(tft.height() - 24, 24);
              stopPlaying();
              playflag = false;
            }
          };
          break;
        case RESULT_LONG_CLICK:
          {
            setVolume(-1);
          }
          break;
        case RESULT_DOUBLE_CLICK: break;
      }
    } else {
      switch (result) {
        case RESULT_CLICK:
          {
            if (playflag) {
              setVolume(1);
            } else {
              switchStation(1);
            }
          }
          break;
        case RESULT_LONG_CLICK:
          {
            setVolume(1);
          }
          break;
        case RESULT_DOUBLE_CLICK:
          {
            if (playflag) {
              setVolume(-1);
            } else {
              switchStation(-1);
            }
          }
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
      tftBrightness += 2;
      ledcWrite(pwmLedChannelTFT, tftBrightness);
      delay(8);
    }
  } else {
    while (tftBrightness >= 2) {
      tftBrightness -= 2;
      ledcWrite(pwmLedChannelTFT, tftBrightness);
      delay(5);
    }
  }
}

void showConnectStatus(String main, String sub) {
  tft.fillScreen(backGroundColor);
  tft.drawCentreString(main, (tft.width() >> 1), (tft.height() >> 1) - 36, 4);
  tft.drawCentreString(sub, (tft.width() >> 1), (tft.height() >> 1), 2);
  fade(true);
  fade(false);
}

void setup() {
#ifdef USE_SERIAL_OUT
  Serial.begin(115200);
  while (!Serial) delay(1);
#endif

  tft.init();
  ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);
  ledcAttachPin(TFT_BL, pwmLedChannelTFT);
  ledcWrite(pwmLedChannelTFT, 0);
  tft.setRotation(0);
  tft.setSwapBytes(true);
  display(true);
  showConnectStatus("WIFI", SSID);
  initWiFi();
  showConnectStatus("Done", WiFi.localIP().toString());
  delay(500); // sorry, hard coded, IP might be of interest

#ifdef CREDITS_DISPLAY
  showCredits();
#endif

  out = new AudioOutputI2S(0, 1);
  out->SetOutputModeMono(true);
  out->SetGain(fgain * 0.05);
  drawBasicLabels();

  startMillis = millis();
  titleSprite.setTextWrap(false);  // Don't wrap text to next line
  titleSprite.setTextSize(2);      // larger letters
  timeSprite.setTextWrap(false);
  timeSprite.setTextFont(2);
  tftOffTimer = millis() + TFT_OFF_TIMEOUT;
  lowestBgChangeTimeMs = millis() + MIN_BG_SWITCH_MS;
  highstBgUnChangeTimeMs = millis() + MAX_BG_SAME_MS;
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
  tft.setTextSize(1);
  tft.setFreeFont(&Orbitron_Medium_20);
  tft.setCursor(2, 20);
  tft.println("vincRadio");
  drawLabels();
  ShowPlayTime();
}

void drawLabels() {
  tft.setTextColor(foreGroundColor);
  drawStatus(playflag ? "Playing" : "Waiting", Y_STATUS);
  drawVolumeBar(String(fgain), 66);
  showStation();
  drawBox("Status", Y_STATUS, backGroundColor);
  drawBox("Volume", Y_VOLUME, backGroundColor);
  drawBox("Time", Y_TIME, backGroundColor);
  tft.setFreeFont(NULL);
  tft.setTextSize(2);
}

void drawStatus(String status, int pos) {
  tft.fillRoundRect(72, pos - 1, 135 - 74, 18, 3, backGroundColor);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(foreGroundColor, backGroundColor);
  tft.drawString(status, 78, pos, 2);
}

void drawVolumeBar(String status, int pos) {
  float length = 54 * (fgain / MAX_GAIN);
  tft.fillRoundRect(72, pos - 1, 135 - 74, 18, 3, backGroundColor);
  tft.fillRoundRect(76, pos + 3, length, 10, 2, foreGroundColor);
}


void ShowPlayTime() {
  String str = "";
  timeSprite.createSprite(tft.width() - 74, 18);
  timeSprite.fillRoundRect(0, 0, tft.width() - 74 - 6, 18, 3, backGroundColor);
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
  timeSprite.setTextColor(foreGroundColor);
  timeSprite.drawString(str, 4, 0, 2);
  timeSprite.pushSprite(74, Y_TIME);
}

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

void restoreBg(int y, int height) {
  tft.pushImage(0, y, tft.width(), height, &bg_img[bgImage][tft.width() * y]);
}

void showStation() {
  restoreBg(108, 20);
  tft.setTextColor(foreGroundColor);
  tft.setTextSize(2);
  tft.drawString(stations[station].name, 4, 110, 1);
}

void setVolume(int direction) {
  fgain = fgain + (float)direction;
  if (fgain > MAX_GAIN) {
    fgain = 1.0;
  }
  out->SetGain(fgain * 0.05);
  drawVolumeBar(String(fgain), Y_VOLUME);
#ifdef USE_SERIAL_OUT
  Serial.printf("STATUS(Gain) %f \n", fgain * 0.05);
#endif
}

void switchStation(int direction) {
  station = station + direction;
  if (station < 0) station = (sizeof(stations) / sizeof(Station)) - 1;
  if (station > (sizeof(stations) / sizeof(Station)) - 1) station = 0;
#ifdef USE_STATION_GAIN
  fgain = stations[station].gain;
  out->SetGain(fgain * 0.05);
  drawVolumeBar(String(fgain), Y_VOLUME);
#endif
  streamingForMs = 0;
  showStation();
}

void handlePlay() {
  if ((globalCnt & 0x3f) == 0x3f) {
    ShowPlayTime();
  }
  if (mp3->isRunning()) {
    if (playflag) streamingForMs += millis() - lastms;
    lastms = millis();
  }
  if (!mp3->loop()) {
    mp3->stop();
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

void startPlaying() {
  lowestBgChangeTimeMs = millis() + MIN_BG_SWITCH_MS;
  highstBgUnChangeTimeMs = millis() + MAX_BG_SAME_MS;
  file = new AudioFileSourceICYStream(stations[station].url);
  file->RegisterMetadataCB(MDCallback, (void *)"ICY");
  buf = new AudioFileSourceBuffer(file, BUF_SIZE);
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

void stopPlaying() {
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
  drawStatus("Stopped", Y_STATUS);
#ifdef USE_SERIAL_OUT
  Serial.printf("STATUS(Stopped)\n");
  Serial.flush();
#endif
}

void nextBackground() {
  bgImage++;
  if(bgImage >= (sizeof(bg_img) / sizeof(bg_img[0]))) bgImage = 0;
  tftBgRow = 0;
  drawBasicLabels();
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

  bool changeBg = (title != lastTitle) && !tftOff && (millis() > lowestBgChangeTimeMs);
   if(changeBg) {  
    lowestBgChangeTimeMs = millis() + MIN_BG_SWITCH_MS;
    highstBgUnChangeTimeMs = millis() + MAX_BG_SAME_MS;
    nextBackground();
  }
  lastTitle = title;
  titleLength = title.length();
  if (titleLength == 0) title = "Wait for title info";
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

void bgRepaint() { // hack to avoid tickering
  if(tftBgRow < tft.height()) {
    restoreBg(tftBgRow, 0x20);
    tftBgRow += 0x20;
    if(tftBgRow >= tft.height()) {
      drawBasicLabels();
    }
  }
}

void loop() {
  nowMillis = millis();
  bgRepaint();
  if (millis() > highstBgUnChangeTimeMs) {
  highstBgUnChangeTimeMs = millis() + MAX_BG_SAME_MS;
  nextBackground();
}
#ifndef TFT_ALWAYS_ON  
  if (!tftOff && (millis() > tftOffTimer)) display(false);
#endif  
  displayBrightness();
  if (playflag) handlePlay();
  btn0.Listen();
  btn1.Listen();
  globalCnt++;
}