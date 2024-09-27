#include "FigureExecutionEvent.h"
#include "SystemClock.h"

namespace KO
{

FigureExecutionEvent::FigureExecutionEvent(SchedulerBase* pSchedulerBase, int iProductIdx, int iDepthLevel, KOEpochTime cCallTime)
:TimeEvent(cCallTime)
{
    _pSchedulerBase = pSchedulerBase;
    _cCallTime = cCallTime;
    _iProductIdx = iProductIdx;
    _iDepthLevel = iDepthLevel;
	_eTimeEventType = TimeEvent::FigureEvent;
}

FigureExecutionEvent::~FigureExecutionEvent()
{

}

void FigureExecutionEvent::makeCall()
{
    //_pSchedulerBase->figureExecutionCall(_cCallTime, _iProductIdx, _iDepthLevel);
}

}
