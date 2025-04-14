#ifndef SLSL_H
#define SLSL_H

#include "../EngineInfra/TradeEngineBase.h"
#include "../base/Instrument.h"
#include "../EngineInfra/SimpleLogger.h"
#include "../EngineInfra/KOEpochTime.h"
#include "../SLBase/SLBase.h"

namespace KO
{

class SLSL : public SLBase
{

struct DailyStat
{
	double dQuoteInstrumentEXMA;
    long   iQuoteInstrumentEXMANumDataPoints;
	double dQuoteInstrumentWeightedStdevEXMA;
	double dQuoteInstrumentWeightedStdevSqrdEXMA;
    long   iQuoteInstrumentWeightedStdevNumDataPoints;
	double dQuoteInstrumentWeightedStdevAdjustment;
	double dSignalInstrumentEXMA;
    long   iSignalInstrumentEXMANumDataPoints;
	double dSignalInstrumentWeightedStdevEXMA;
	double dSignalInstrumentWeightedStdevSqrdEXMA;
    long   iSignalInstrumentWeightedStdevNumDataPoints;
	double dSignalInstrumentWeightedStdevAdjustment;
	double dProductWeightedStdevEXMA;
	double dProductWeightedStdevSqrdEXMA;
    long   iProductWeightedStdevNumDataPoints;
	double dProductWeightedStdevAdjustment;
    double dLastQuoteMid;
    double dLastSignalMid;
};

public:
    SLSL(const string& sEngineRunTimePath,
         const string& sEngineSlotName,
         KOEpochTime cTradingStartTime,
         KOEpochTime cTradingEndTime,
         SchedulerBase* pScheduler,
         string sTodayDate,
         const string& sSimType,
         KOEpochTime cSlotFirstWakeupCallTime);

	virtual ~SLSL();
	
    void dayInit();
    void dayTrade();
    void dayRun();
    void dayStop();
	
	void readFromStream(istream& is);
	void receive(int iCID);
	void wakeup(KOEpochTime cCallTime);

private:
    void updateStatistics(KOEpochTime cCallTime);

    bool bcheckAllProductsReady();

    void loadRollDelta();
	virtual void saveOvernightStats(bool bRemoveToday) override;
	void loadOvernightStats();

    void writeSpreadLog();

	Instrument*           _pSignalInstrument;

    bool _bSignalInstrumentStaled;

	map< boost::gregorian::date, boost::shared_ptr<DailyStat> > _mDailyStats;

	long _iTotalSignalInstruUpdate;

    double _dLeaderWeight;
    
    bool _bUpdateStats;

    double _dLastSignalMid;

    KOEpochTime _cLastSignalUpdateTime;
};

}

#endif /* SLSL_H */
