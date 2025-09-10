#ifndef WakeupUSDEvent_H
#define WakeupUSDEvent_H

#include "TimeEvent.h"
#include "KOEpochTime.h"
#include "TimerCallbackInterface.h"

namespace KO
{

class WakeupUSDEvent : public TimeEvent
{
public:
	WakeupUSDEvent(TimerCallbackInterface* pTarget, KOEpochTime cCallTime);
	~WakeupUSDEvent();

	void makeCall();
private:
	TimerCallbackInterface*                    _pTarget;
};

}

#endif /* WakeupUSDEvent_H */
