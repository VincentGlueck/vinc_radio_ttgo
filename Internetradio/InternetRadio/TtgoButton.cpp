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
  allowSingleClickMillis = 0;
  longPressRepeatMillis = LONG_REPEAT_MS;
  result = RESULT_NONE;
}

TtgoButton::TtgoButton(int _pin, ButtonCallback* _callback) : TtgoButton(_pin) {
  callback = _callback;
}

void TtgoButton::RegisterCallback(ButtonCallback* _callback) {
  callback = _callback;
}

void TtgoButton::SetPressedOnHigh(bool high) {
  pinLevelPressed = high ? HIGH : LOW;
}

void TtgoButton::SetLongPressRepeatMillis(long millis) {
  longPressRepeatMillis = millis;
}

long diffToNow(long timeMs) {
  return millis() - timeMs;
}

void TtgoButton::Listen() {
  if (result != RESULT_LONG_CLICK) {
     result = RESULT_NONE;
  }
  int pinState = digitalRead(pin);
  if (pinState != lastHighLow && diffToNow(lastHighLowChange) < DEBOUNCE) {
    return;
  }
  lastHighLow = pinState;
  lastHighLowChange = millis();
  if (pinState == pinLevelPressed) {
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
    } else if ((time > CLICK_MS) && !waitForSingleClick && !preventSingleClick && (millis() > allowSingleClickMillis)) {
      waitForSingleClick = true;
      resultNotBeforeMillis = millis() + (CLICK_MS >> 1);
      callbackDone = false;
    } else if ((time > DOUBLE_CLICK_MS) && !waitForSingleClick) {
      if ((lastDoubleLowMillis != -1) && (millis() - lastDoubleLowMillis) > DOUBLE_CLICK_MS) {
        allowSingleClickMillis = millis() + (DOUBLE_CLICK_MS << 1);
        doubleClickCount++;
      }
      lastDoubleLowMillis = -1;
    }
  } else {
    if ((millis() > resultNotBeforeMillis) && !callbackDone) {
      doubleClickCount = 0;
      callbackDone = true;
      callback->onButtonPressed(pin, RESULT_CLICK);
    }
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
    preventSingleClick = false;
    allowSingleClickMillis = 0;
    callback->onButtonPressed(pin, RESULT_DOUBLE_CLICK);
  }
  if ((result == RESULT_LONG_CLICK) && !preventDoubleClick && !callbackDone) {
    preventSingleClick = true;
    callbackDone = true;
    nextRepeatLongMillis = millis() + longPressRepeatMillis;
    callback->onButtonPressed(pin, RESULT_LONG_CLICK);
  }
}