#ifndef _TTGOBUTTON_H
#define _TTGOBUTTON_H
#include "Arduino.h"
#include "TtgoCallback.h"

#define BTN0 0
#define BTN1 35

#define RESULT_CLICK 0
#define RESULT_LONG_CLICK 1
#define RESULT_DOUBLE_CLICK 2
#define RESULT_NONE 0xff

#define SKIP_GLITTER 10
#define LONG_REPEAT_MS 100
#define CLICK_MS 100
#define CLICK_LONG_MS 500
#define DOUBLE_CLICK_MS 50

class TtgoButton {
public:
  TtgoButton(int _pin = BTN0);
  TtgoButton(int _pin, TtgoCallback* _callback);
  void listen();
  void registerCallback(TtgoCallback* _callback);

private:
  TtgoCallback* callback;
  bool callbackDone;
  int pin;
  long lastLowMillis;
  int result;
  int lastHighLow;
  long lastHighLowChange;
  bool preventSingleClick;
  bool preventDoubleClick;
  bool waitForSingleClick;
  long nextRepeatLongMillis;
  long resultNotBeforeMillis;
  long lastDoubleLowMillis;
  int doubleClickCount;
};

#endif