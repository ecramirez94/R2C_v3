#ifndef TIMERS_H
#define TIMERS_H

#include <Arduino.h>
#include "pins.h"

class Timers
{
  public:
    /* 8-bit Timer/Counter0. "Hall Pulse Counter" */
    void initHallPulseCounter(void);
    void enableHallPulseCounter(void);
    void disableHallPulseCounter(void);
    void clearHallPulseCounter(void);

    /* 16-bit Timer/Counter1. "Pulse Collection Timer" */
    void initPulseCollectionTimer(void);
    void startPulseCollectionTimer(void);
    void stopPulseCollectionTimer(void);

    /* 8-bit Timer/Counter2. "Display Update Timer" */
    void startDisplayUpdateTimer(void);
    void stopDisplayUpdateTimer(void);

  protected:
  private:
};

#endif
