#include "esp32-hal.h"
#include "esp32-hal-gpio.h"
#include "TtgoButton.h"
#include "Arduino.h"  // only for debugging (Serial.print)


TtgoButton::TtgoButton(int _pin) {
  pin = _pin;
  pinMode(pin, INPUT);
  lastLowMillis = -1;
  nextRepeatLongMillis = -1;
  lastDoubleLowMillis = -1;
  doubleClickCount = 0;
  preventSingleClick = false;
  waitForSingleClick = false;
  callbackDone = true;
  result = RESULT_NONE;
}

TtgoButton::TtgoButton(int _pin, TtgoCallback* _callback)
  : TtgoButton(_pin) {
  callback = _callback;
}

void TtgoButton::registerCallback(TtgoCallback* _callback) {
  callback = _callback;
}

long diffToNow(long timeMs) {
  return millis() - timeMs;
}

void TtgoButton::listen() {
  if (result != RESULT_LONG_CLICK) result = RESULT_NONE;
  int pinState = digitalRead(pin);
  if (pinState != lastHighLow && diffToNow(lastHighLowChange) < SKIP_GLITTER) {
    return;
  } else {
    lastHighLow = pinState;
    lastHighLowChange = millis();
  }
  if (pinState == LOW) {
    if (lastLowMillis == -1) {
      lastLowMillis = millis();
      return;
    }
    long time = diffToNow(lastLowMillis);
    if ((time > CLICK_LONG_MS) && ((millis() > nextRepeatLongMillis) || nextRepeatLongMillis == -1)) {
      nextRepeatLongMillis = millis() + LONG_REPEAT_MS;
      doubleClickCount = 0;
      result = RESULT_LONG_CLICK;
      preventSingleClick = true;
      callbackDone = false;
    } else if ((time > CLICK_MS) && !waitForSingleClick && !preventSingleClick) {
      waitForSingleClick = true;
      resultNotBeforeMillis = millis() + (CLICK_MS >> 1);
      callbackDone = false;
    } else if ((time > DOUBLE_CLICK_MS) && !waitForSingleClick) {
      if ((lastDoubleLowMillis != -1) && (millis() - lastDoubleLowMillis) > DOUBLE_CLICK_MS) {
        doubleClickCount++;
      }
      lastDoubleLowMillis = -1;
    }
  } else {
    lastDoubleLowMillis = millis();
    lastLowMillis = -1;
    nextRepeatLongMillis = -1;
    waitForSingleClick = false;
    preventSingleClick = false;
    callbackDone = true;
    result = RESULT_NONE;
  }

  if (!callbackDone && (doubleClickCount > 1)) {
    doubleClickCount = 0;
    callback->onButtonPressed(RESULT_DOUBLE_CLICK);
  }
  if ((result == RESULT_LONG_CLICK) && !preventDoubleClick && !callbackDone) {
    preventSingleClick = true;
    callbackDone = true;
    nextRepeatLongMillis = millis() + LONG_REPEAT_MS;
    callback->onButtonPressed(RESULT_LONG_CLICK);
  }
  if ((millis() > resultNotBeforeMillis) && !callbackDone) {
    doubleClickCount = 0;
    callbackDone = true;
    callback->onButtonPressed(RESULT_CLICK);
  }
}
