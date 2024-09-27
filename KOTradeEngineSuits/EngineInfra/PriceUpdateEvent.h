#ifndef PriceUpdateEvent_H
#define PriceUpdateEvent_H

#include "TimeEvent.h"
#include "KOScheduler.h"
#include "KOEpochTime.h"

namespace KO
{

class PriceUpdateEvent : public TimeEvent
{
public:
    PriceUpdateEvent(KOScheduler* pScheduler, KOEpochTime cPriceUpdateTime);
    ~PriceUpdateEvent();

    void makeCall();
private:
    KOScheduler*        _pScheduler;
};

}

#endif /* PriceUpdateEvent_H */
