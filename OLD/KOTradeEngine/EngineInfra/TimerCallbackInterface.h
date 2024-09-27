#ifndef TimerCallbackInterface_H
#define TimerCallbackInterface_H

#include "KOEpochTime.h"

namespace KO
{

class TimerCallbackInterface
{
public:
    TimerCallbackInterface(){};

    virtual void wakeup(KOEpochTime cCallTime) = 0;
};

}

#endif
