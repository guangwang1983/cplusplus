#include "WakeupEvent.h"

namespace KO
{

WakeupEvent::WakeupEvent(TimerCallbackInterface* pTarget, KOEpochTime cCallTime)
:TimeEvent(cCallTime),
 _pTarget(pTarget)
{
	_eTimeEventType = TimeEvent::WakeupEvent;
}

WakeupEvent::~WakeupEvent()
{

}

void WakeupEvent::makeCall()
{
	// only calls the engine if it is during trading hours
    _pTarget->wakeup(_cCallTime);
}

}
