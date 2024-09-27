#ifndef FigureExecutionEvent_H
#define FigureExecutionEvent_H

#include "TimeEvent.h"
#include "TradeEngineBase.h"
#include "Figures.h"

namespace KO
{

class FigureExecutionEvent : public TimeEvent
{
public:
	FigureExecutionEvent(SchedulerBase* pSchedulerBase, int iProductIdx, int iDepthLevel, KOEpochTime cCallTime);
	~FigureExecutionEvent();

	void makeCall();
private:
	SchedulerBase*    _pSchedulerBase;
    KOEpochTime       _cCallTime;
    int               _iProductIdx;
    int               _iDepthLevel;
};

}

#endif /* FigureEvent_H */
