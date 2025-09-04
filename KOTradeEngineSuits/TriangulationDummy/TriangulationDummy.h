#ifndef TriangulationDummy_H
#define TriangulationDummy_H

#include "../EngineInfra/TradeEngineBase.h"
#include "../base/Instrument.h"
#include "../base/SyntheticSpread.h"
#include "../EngineInfra/Figures.h"
#include "../EngineInfra/KOEpochTime.h"

namespace KO
{

class TriangulationDummy : public TradeEngineBase
{

public:
    TriangulationDummy(const string& sEngineRunTimePath,
                       const string& sEngineSlotName,
                       KOEpochTime cTradingStartTime,
                       KOEpochTime cTradingEndTime,
                       SchedulerBase* pScheduler,
                       const string& sTodayDate,
                       const string& sSimType,
                       KOEpochTime cSlotFirstWakeupCallTime);

	virtual ~TriangulationDummy();
	
    virtual void dayInit();
    virtual void dayTrade();
    virtual void dayRun();
    virtual void dayStop();
	
	virtual void readFromStream(istream& is);
	virtual void receive(int iCID);
	virtual void wakeup(KOEpochTime cCallTime);
	virtual void figureCall(KOEpochTime cCallTime, KOEpochTime cEventTime, const string& sFigureName, FigureAction::options eFigureAction, double dForecast, double dActual, bool bReleased);

    virtual void writeSpreadLog();

protected:
    KOEpochTime _cSlotFirstWakeupCallTime;
};

}

#endif /* TriangulationDummy_H */
