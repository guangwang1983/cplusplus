#ifndef FigureEvent_H
#define FigureEvent_H

#include "TimeEvent.h"
#include "TradeEngineBase.h"
#include "Figures.h"

namespace KO
{

class FigureEvent : public TimeEvent
{
public:
	FigureEvent(TradeEngineBase* pTargetEngine, FigureCallPtr pFigureCall);
	~FigureEvent();

	void makeCall();
private:
	TradeEngineBase*    _pTargetEngine;
    FigureCallPtr       _pFigureCall;
};

}

#endif /* FigureEvent_H */
