#ifndef WakeupEvent_H
#define WakeupEvent_H

#include "TimeEvent.h"
#include "KOEpochTime.h"
#include "TimerCallbackInterface.h"

namespace KO
{

class WakeupEvent : public TimeEvent
{
public:
	WakeupEvent(TimerCallbackInterface* pTarget, KOEpochTime cCallTime);
	~WakeupEvent();

	void makeCall();
private:
	TimerCallbackInterface*                    _pTarget;
};

}

#endif /* WakeupEvent_H */
