#ifndef SL3LEqualWeight_H
#define SL3LEqualWeight_H

#include "../EngineInfra/TradeEngineBase.h"
#include "../base/Instrument.h"
#include "../base/SyntheticSpread.h"
#include "../EngineInfra/SimpleLogger.h"
#include "../base/FairValueExecution.h"
#include "../SLBase/SLBase.h"

namespace KO
{

class SL3LEqualWeight : public SLBase
{

struct DailyStat
{
	double dQuoteInstrumentEXMA;
    long   iQuoteInstrumentEXMANumDataPoints;
	double dQuoteInstrumentWeightedStdevEXMA;
	double dQuoteInstrumentWeightedStdevSqrdEXMA;
    long   iQuoteInstrumentWeightedStdevNumDataPoints;
	double dQuoteInstrumentWeightedStdevAdjustment;

	double dSpreadFrontInstrumentEXMA;
    long   iSpreadFrontInstrumentEXMANumDataPoints;
	double dSpreadFrontInstrumentWeightedStdevEXMA;
	double dSpreadFrontInstrumentWeightedStdevSqrdEXMA;
    long   iSpreadFrontInstrumentWeightedStdevNumDataPoints;
	double dSpreadFrontInstrumentWeightedStdevAdjustment;

	double dSpreadBackInstrumentEXMA;
    long   iSpreadBackInstrumentEXMANumDataPoints;
	double dSpreadBackInstrumentWeightedStdevEXMA;
	double dSpreadBackInstrumentWeightedStdevSqrdEXMA;
    long   iSpreadBackInstrumentWeightedStdevNumDataPoints;
	double dSpreadBackInstrumentWeightedStdevAdjustment;

	double dSpreadInstrumentWeightedStdevEXMA;
	double dSpreadInstrumentWeightedStdevSqrdEXMA;
    long   iSpreadInstrumentWeightedStdevNumDataPoints;
	double dSpreadInstrumentWeightedStdevAdjustment;

	double dProductWeightedStdevEXMA;
	double dProductWeightedStdevSqrdEXMA;
    long   iProductWeightedStdevNumDataPoints;
	double dProductWeightedStdevAdjustment;
};

public:
    SL3LEqualWeight(const string& sEngineRunTimePath,
         const string& sEngineSlotName,
         KOEpochTime cTradingStartTime,
         KOEpochTime cTradingEndTime,
         SchedulerBase* pScheduler,
         string sTodayDate,
         PositionServerConnection* pPositionServerConnection);

	virtual ~SL3LEqualWeight();

    void dayInit();
    void dayRun();
    void dayStop();    
		
	void readFromStream(istream& is);
	void receive(int iCID);
	void wakeup(KOEpochTime cCallTime);

private:
    void updateStatistics(KOEpochTime cCallTime);

    bool bcheckAllProductsReady();

    void loadRollDelta();
	void saveOvernightStats();
	void loadOvernightStats();

    void writeSpreadLog();

	Instrument* 			_pSpreadFrontInstrument;
	Instrument* 			_pSpreadBackInstrument;
	SyntheticSpread* 		_pSpreadInstrument;

    bool _bSpreadFrontInstrumentStaled;
    bool _bSpreadBackInstrumentStaled;

	map< boost::gregorian::date, boost::shared_ptr<DailyStat> > _mDailyStats;

	bool _bSpreadPositiveCorrelation;
	bool _bProductPostiveCorrelation;

    long _iTotalSpreadFrontInstruUpdate;
    long _iTotalSpreadBackInstruUpdate;

    double _dLeaderWeight;

    bool _bUpdateStats;
};

}

#endif /* SL3LEqualWeight_H */
