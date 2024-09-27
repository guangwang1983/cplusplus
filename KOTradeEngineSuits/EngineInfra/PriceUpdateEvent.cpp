#include "PriceUpdateEvent.h"

namespace KO
{

PriceUpdateEvent::PriceUpdateEvent(KOScheduler* pScheduler, KOEpochTime cPriceUpdateTime)
:TimeEvent(cPriceUpdateTime),
 _pScheduler(pScheduler)
{
    _eTimeEventType = TimeEvent::PriceUpdateEvent;
}

PriceUpdateEvent::~PriceUpdateEvent()
{

}

void PriceUpdateEvent::makeCall()
{
    _pScheduler->applyPriceUpdateOnTime(_cCallTime);
}
    
}
