#include <Arduino.h>
#include <BufferedOutput.h>
#include <WiFi.h>
#include <AudioFileSource.h>
#include <AudioFileSourceBuffer.h>
#include <AudioFileSourceICYStream.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorAAC.h>
#include <AudioOutputI2S.h>
#include <AudioOutputI2SNoDAC.h>
#include <TFT_eSPI.h>
#include "TtgoButton.h"
#include "bg_img.h"

#define USE_SERIAL_OUT
#define BUF_SIZE 0x0c00  // size of streaming buffer (0x400 -> more decoding errors, 0x1000 -> default)
#define AMP_ANI_Y 170    // position of fake animation
#define AMP_COLORFUL     // use colorful amp ani
#define BRIGHTNESS 220   // brightness during display = on (max 255)
#define TITLE_DELTA_INITIAL 24
#define Y_SCROLL 224
#define TITLE_SCROLL_SPEED 2

// TODO: enter your WiFi credentials
const char *SSID = "VincentVega01";
const char *PASSWORD = "winter01";
//==================================

TaskHandle_t streamTask;
TaskHandle_t uiTask;
TaskHandle_t debugTask;
TaskHandle_t animationTask;
TaskHandle_t buttonTask;
TaskHandle_t workerTask;

AudioGenerator *decoder = NULL;
AudioFileSourceICYStream *stream;
AudioFileSourceBuffer *buf;
AudioOutputI2S *out;
const uint8_t fontwidth = 16;  // scroll text width
const int preallocateBufferSize = 0x1000;
const int preallocateCodecSize = 85332;  // AAC+SBR codec max mem needed
void *preallocateBuffer = NULL;
void *preallocateCodec = NULL;

uint32_t lastms = 0;
uint32_t streamingForMs = 0;
uint8_t bgImage = 1;

const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite titleSprite = TFT_eSprite(&tft);
TFT_eSprite timeSprite = TFT_eSprite(&tft);
TFT_eSprite ampSprite = TFT_eSprite(&tft);
TFT_eSprite stationSprite = TFT_eSprite(&tft);

String title;
String lastTitle;
String btnStr;

int titleScroll = 135;
uint8_t titleLength;
uint16_t scroll_pos = 0;
int tcount;
uint8_t titleDeltaY = TITLE_DELTA_INITIAL;
uint8_t scrollInternalCnt;
bool flagCreateTitleSprite = false;
bool flagTitleSpriteAvail = false;

struct rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t dr;
  uint8_t dg;
  uint8_t db;
};

void resultToString(int result) {
  switch (result) {
    case RESULT_CLICK: btnStr = "click"; break;
    case RESULT_DOUBLE_CLICK: btnStr = "double click"; break;
    case RESULT_LONG_CLICK: btnStr = "long click"; break;
    default: btnStr = "<undef>";
  }
}

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
    else if (x > (tft.width() - 48)) ddr = 6;
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
  if (flagTitleSpriteAvail) {
    titleSprite.pushSprite(tcount + tft.width(), Y_SCROLL-2);
    tcount -= TITLE_SCROLL_SPEED;
    int min = (fontwidth * (title.length() + 4)); // + (tft.width() >> 2);
    if(tcount < (-min)) {
      tcount = 0;
    }
  }
  ampSprite.pushSprite(0, AMP_ANI_Y);
}

void initWifi() {
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
      Serial.println("Unable to connect; w/o WiFi this won't work, sorry.");
      i = 0;
      // SSID / PW correct?
#endif
    }
  }
#ifdef USE_SERIAL_OUT
  Serial.println("OK");
#endif
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  tft.init();

  ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);
  ledcAttachPin(TFT_BL, pwmLedChannelTFT);
  ledcWrite(pwmLedChannelTFT, BRIGHTNESS);
  tft.setRotation(0);
  tft.initDMA();
  tft.setSwapBytes(true);
  tft.pushImage(0, 0, tft.width(), tft.height(), &bg_img[bgImage][0]);

  initWifi();

  za = random(256);
  zb = random(256);
  zc = random(256);
  zx = random(256);
  rgb_color = { (uint8_t)(120 - (rnd() >> 2)), (uint8_t)(130 - (rnd() >> 2)), (uint8_t)(100 - (rnd() >> 2)), (uint8_t)(rnd() >> 5), (uint8_t)(rnd() >> 6), (uint8_t)(rnd() >> 4) };
  for (int n = 0; n < sizeof(amp_colors) / sizeof(rgb); n++) {
    amp_colors[n] = rgb_color;
  }
  SafeString::setOutput(Serial);

  xTaskCreatePinnedToCore(
    streamHandler,
    "Stream",
    2048,                             // Stack size (bytes)
    NULL,                             // Parameter to pass
    (configMAX_PRIORITIES - 1) >> 1,  // Task priority
    &streamTask,
    1);

/* not in use currently - early state
  xTaskCreatePinnedToCore(
    uiHandler,
    "UI",
    2048,
    NULL,
    2,
    &uiTask,
    1);
  */

  xTaskCreatePinnedToCore(
    debugHandler,
    "Debug",
    3072,
    NULL,
    2,
    &debugTask,
    1);

  xTaskCreatePinnedToCore(
    animationHandler,
    "Animation",
    3072,
    NULL,
    1,
    &animationTask,
    1);

  xTaskCreatePinnedToCore(
    buttonHandler,
    "TTGOButton",
    3072,
    NULL,
    3,
    &buttonTask,
    1);

  xTaskCreatePinnedToCore(
    workerHandler,
    "Worker",
    2048,
    NULL,
    3,
    &workerTask,
    1);
}

void streamHandler(void *pvParameters) {
  (void)pvParameters;
  decoder = (AudioGenerator *)new AudioGeneratorMP3(preallocateCodec, preallocateCodecSize);
  stream = new AudioFileSourceICYStream("http://icecast.ndr.de/ndr/njoy/live/mp3/128/stream.mp3");
  stream->RegisterMetadataCB(MDCallback, (void *)"ICY");
  buf = new AudioFileSourceBuffer(stream, BUF_SIZE);
  buf->RegisterStatusCB(StatusCallback, (void *)"buffer");
  out = new AudioOutputI2S(0, 1);
  decoder->RegisterStatusCB(StatusCallback, (void *)"mp3");
  decoder->begin(buf, out);
  out->SetGain(0.1);
  while (1) {
    if (decoder->isRunning()) {
    }
    if (!decoder->loop()) {
      decoder->stop();
    }
    vTaskDelay(1);
  }
}

void animationHandler(void *pvParameters) {
  (void)pvParameters;
  while (1) {
    if (streamTask) {
      showAmpAni();
    } else {
      Serial.println("skip ani");
    }
    vTaskDelay(30 / portTICK_PERIOD_MS);
  }
}

void debugHandler(void *pvParameters) {
  (void)pvParameters;
  Serial.begin(115200);
  while (1) {
    Serial.println("======== Tasks status ========");

    Serial.print("Tick count: ");
    Serial.print(xTaskGetTickCount());
    Serial.print(", Task count: ");
    Serial.print(uxTaskGetNumberOfTasks());
    Serial.print(", portTICK_PERIOD_MS: ");
    Serial.print(portTICK_PERIOD_MS);
    Serial.printf(", max thread priority: %d, free mem: %d\n", configMAX_PRIORITIES - 1, esp_get_free_heap_size());

    // Serial task status
    Serial.print("- TASK " + String(pcTaskGetName(NULL)));
    Serial.printf(", max: %d\n", uxTaskGetStackHighWaterMark(NULL));
    if (streamTask) {
      Serial.print("- TASK " + String(pcTaskGetName(streamTask)));
      Serial.printf(", max: %d\n", uxTaskGetStackHighWaterMark(streamTask));
    }
    if (animationTask) {
      Serial.print("- TASK " + String(pcTaskGetName(animationTask)));
      Serial.printf(", max: %d\n", uxTaskGetStackHighWaterMark(animationTask));
    }
    if (uiTask) {
      Serial.print("- TASK " + String(pcTaskGetName(uiTask)));
      Serial.printf(", max: %d\n", uxTaskGetStackHighWaterMark(uiTask));
    }
    if (buttonTask) {
      Serial.print("- TASK " + String(pcTaskGetName(buttonTask)));
      Serial.printf(", max: %d\n", uxTaskGetStackHighWaterMark(buttonTask));
    }
    if (workerTask) {
      Serial.print("- TASK " + String(pcTaskGetName(workerTask)));
      Serial.printf(", max: %d\n", uxTaskGetStackHighWaterMark(workerTask));
    }

    vTaskDelay(20000 / portTICK_PERIOD_MS);
  }
}

/*
void uiHandler(void *pvParameters) {
  (void)pvParameters;
  vTaskDelay(2000);
  while (1) {
    if (buf) {
      Serial.printf("title: '%s', buf pos: %d\n", title.c_str(), buf->getPos());
    }
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}
*/

void workerHandler(void *pvParameters) {
  (void)pvParameters;
  while (1) {
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    if (flagCreateTitleSprite && title.length() > 0) {
      titleSprite.createSprite((title.length() + 1) * fontwidth, 28);
      if (title.length() > 0) {
        titleSprite.fillRect(0, 0, titleSprite.width(), 28, TFT_BLACK);
        titleSprite.setTextSize(2);
        titleSprite.drawString(title, 0, 2, 1);
      }
      flagCreateTitleSprite = false;
      flagTitleSpriteAvail = true;
    }
  }
}

void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void)isUnicode;
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2) - 1] = 0;
  title = s2 + String(" ");
  titleLength = title.length();
  if (titleLength == 0) title = "Wait for title info";
  flagCreateTitleSprite = true;
  lastTitle = title.c_str();
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

void stopPlaying() {
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
#ifdef USE_SERIAL_OUT
  Serial.printf("STATUS(Stopped)\n");
  Serial.flush();
#endif
}

class ButtonCallback : public TtgoButton::ButtonCallback {
public:
  void onButtonPressed(const int &btnPin, const int &result) override {
#ifdef USE_SERIAL_OUT
    resultToString(result);
    Serial.printf("btn %d: '%s'\n", btnPin, btnStr.c_str());
#endif
  }
};

ButtonCallback btnCallback;
TtgoButton btn0(BTN0, &btnCallback);
TtgoButton btn1(BTN1, &btnCallback);

void buttonHandler(void *pvParameters) {
  (void)pvParameters;
  vTaskDelay(200);
  while (1) {
    btn0.Listen();
    btn1.Listen();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void loop() {
  delay(1 << 31);
}
