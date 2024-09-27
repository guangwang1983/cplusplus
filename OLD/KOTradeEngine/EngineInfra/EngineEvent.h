#ifndef EngineEvent_H
#define EngineEvent_H

#include "TimeEvent.h"
#include "KOEpochTime.h"

namespace KO
{

class TradeEngineBase;

class EngineEvent : public TimeEvent
{
public:
	enum EngineEventType
	{
		RUN,
		STOP,
		LIM_LIQ,
		FAST_LIQ
	};

	EngineEvent(TradeEngineBase* _pTargetEngine, EngineEventType eEventType, KOEpochTime cCallTime);
	~EngineEvent();

	void makeCall();
private:
	TradeEngineBase*    _pTargetEngine;
	EngineEventType     _eEventType;
};

}

#endif /* EngineEvent_H */
