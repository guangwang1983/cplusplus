#include "WakeupUSDEvent.h"

namespace KO
{

WakeupUSDEvent::WakeupUSDEvent(TimerCallbackInterface* pTarget, KOEpochTime cCallTime)
:TimeEvent(cCallTime),
 _pTarget(pTarget)
{
	_eTimeEventType = TimeEvent::WakeupUSDEvent;
}

WakeupUSDEvent::~WakeupUSDEvent()
{

}

void WakeupUSDEvent::makeCall()
{
	// only calls the engine if it is during trading hours
    _pTarget->wakeup(_cCallTime);
}

}
