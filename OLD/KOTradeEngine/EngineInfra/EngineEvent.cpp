#include "EngineEvent.h"
#include "TradeEngineBase.h"

namespace KO
{

EngineEvent::EngineEvent(TradeEngineBase* pTargetEngine, EngineEventType eEventType, KOEpochTime cCallTime)
:TimeEvent(cCallTime),
 _pTargetEngine(pTargetEngine),
 _eEventType(eEventType)
{
	_eTimeEventType = TimeEvent::EngineEvent;
}

EngineEvent::~EngineEvent()
{

}

void EngineEvent::makeCall()
{
	switch (_eEventType)
	{
		case RUN:
		{
			_pTargetEngine->dayRun();
			break;
		}
		case STOP:
		{
			_pTargetEngine->dayStop();
			break;
		}
		case LIM_LIQ:
		{
			_pTargetEngine->manualLimitLiquidation();
			break;
		}
		case FAST_LIQ:
		{
			_pTargetEngine->manualFastLiquidation();
			break;
		}
	}
}

}
