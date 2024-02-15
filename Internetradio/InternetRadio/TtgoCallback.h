#ifndef _TTGOCALLBACK_H
#define _TTGOCALLBACK_H

#include "Arduino.h"

class TtgoCallback {
  public:
    virtual void onButtonPressed(const int& result);
};


#endif