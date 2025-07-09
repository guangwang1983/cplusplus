#ifndef SL3L_FI_Filter_H
#define SL3L_FI_Filter_H

#include "../EngineInfra/TradeEngineBase.h"
#include "../base/Instrument.h"
#include "../base/SyntheticSpread.h"
#include "../EngineInfra/SimpleLogger.h"
#include "../SLBase/SLBase.h"

namespace KO
{

class SL3L_FI_Filter : public SLBase
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

    double dSpreadInstrumentEXMA;
    long   iSpreadInstrumentEXMANumDataPoints;
	double dSpreadInstrumentWeightedStdevEXMA;
	double dSpreadInstrumentWeightedStdevSqrdEXMA;
    long   iSpreadInstrumentWeightedStdevNumDataPoints;
	double dSpreadInstrumentWeightedStdevAdjustment;

	double dProductWeightedStdevEXMA;
	double dProductWeightedStdevSqrdEXMA;
    long   iProductWeightedStdevNumDataPoints;
	double dProductWeightedStdevAdjustment;

    double dLastQuoteMid;
    double dLastSpreadFrontMid;
    double dLastSpreadBackMid;

    double dFilterFrontEXMA;
    long   iFilterFrontEXMANumDataPoints;

    double dFilterBackEXMA;
    long   iFilterBackEXMANumDataPoints;
};

public:
    SL3L_FI_Filter(const string& sEngineRunTimePath,
         const string& sEngineSlotName,
         KOEpochTime cTradingStartTime,
         KOEpochTime cTradingEndTime,
         SchedulerBase* pScheduler,
         string sTodayDate,
         const string& sSimType,
         KOEpochTime cSlotFirstWakeupCallTime);

	virtual ~SL3L_FI_Filter();

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
	void loadOvernightStats();
	virtual void saveOvernightStats(bool bRemoveToday) override;

    void writeSpreadLog();

	Instrument* 			_pSpreadFrontInstrument;
	Instrument* 			_pSpreadBackInstrument;
	Instrument* 			_pFilterFrontInstrument;
	Instrument* 			_pFilterBackInstrument;
	SyntheticSpread* 		_pSpreadInstrument;

    bool _bUseFrontFilter;
    bool _bUseBackFilter;

    bool _bSpreadFrontInstrumentStaled;
    bool _bSpreadBackInstrumentStaled;

	map< boost::gregorian::date, boost::shared_ptr<DailyStat> > _mDailyStats;

	bool _bSpreadPositiveCorrelation;

    long _iTotalSpreadFrontInstruUpdate;
    long _iTotalSpreadBackInstruUpdate;

    double _dLeaderWeight;

    bool _bUpdateStats;

    double _dLastSpreadFrontMid;
    double _dLastSpreadBackMid;

    KOEpochTime _cLastSpreadFrontUpdateTime;
    KOEpochTime _cLastSpreadBackUpdateTime;
};

}

#endif /* SL3L_FI_Filter_H */
