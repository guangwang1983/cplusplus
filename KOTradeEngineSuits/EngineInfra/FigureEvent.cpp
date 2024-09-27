#include "FigureEvent.h"
#include "SystemClock.h"

namespace KO
{

FigureEvent::FigureEvent(TradeEngineBase* pTargetEngine, FigureCallPtr pFigureCall)
:TimeEvent(pFigureCall->_cCallTime),
 _pTargetEngine(pTargetEngine),
 _pFigureCall(pFigureCall)
{
	_eTimeEventType = TimeEvent::FigureEvent;
}

FigureEvent::~FigureEvent()
{

}

void FigureEvent::makeCall()
{
	//only calls the engine if it is during trading hours
	if(_pTargetEngine->bisTrading())
	{
        if(_pFigureCall->_cCallTime + KOEpochTime(600, 0) > SystemClock::GetInstance()->cgetCurrentKOEpochTime())
        {
            _pTargetEngine->figureBaseCall(_pFigureCall->_cCallTime, _pFigureCall->_cFigureTime, _pFigureCall->_sFigureName, _pFigureCall->_eFigureAction);
        }
	}
}

}
