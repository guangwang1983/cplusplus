#ifndef TimeEvent_H
#define TimeEvent_H

#include "KOEpochTime.h"

namespace KO
{

class TimeEvent
{
public:
	enum TimeEventType
	{
        OrderConfirmEvent,
        PriceUpdateEvent,
		FigureEvent,
		EngineEvent,
		WakeupEvent,
        WakeupUSDEvent,
		DefaultEvent
	};

	TimeEvent(KOEpochTime cCallTime):_cCallTime(cCallTime), _eTimeEventType(DefaultEvent){};
	virtual ~TimeEvent(){};
	
	inline KOEpochTime cgetCallTime() {return _cCallTime;};
	inline TimeEventType egetTimeEventType(){return _eTimeEventType;};

	bool checkTime(KOEpochTime cCurrentTime) 
    {
        return _cCallTime <= cCurrentTime;
    };
	virtual void makeCall() = 0;
protected:
	KOEpochTime                         _cCallTime;
	TimeEventType						_eTimeEventType;
};

}

#endif /* TimeEvent_H */
