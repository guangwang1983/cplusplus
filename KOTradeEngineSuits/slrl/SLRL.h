#ifndef SLRL_H
#define SLRL_H

#include "../EngineInfra/TradeEngineBase.h"
#include "../base/Instrument.h"
#include "../base/MultiLegProduct.h"
#include "../EngineInfra/SimpleLogger.h"
#include "../SLBase/SLBase.h"
#include <vector>
#include <map>

using namespace std;

namespace KO
{

class SLRL : public SLBase
{

struct DailyLegStat
{
	double dLegEXMA;
    long   iLegEXMANumDataPoints;
    double dLegStd;
};

public:
    SLRL(const string& sEngineRunTimePath,
         const string& sEngineSlotName,
         KOEpochTime cTradingStartTime,
         KOEpochTime cTradingEndTime,
         SchedulerBase* pScheduler,
         string sTodayDate,
         const string& sSimType);

	virtual ~SLRL();

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

    void loadIndicatorSettings();

    void loadPrevRollDelta();

    MultiLegProduct* _pMultiLegProduct;

    vector<Instrument*> _vInstruments;
    vector<bool> _vInstrumentsStaled;
    vector<double> _vLastInstrumentsMid;
    vector<long> _vTotalInstrumentsUpdates;

    vector<Instrument*> _vIndicators;
    vector<double> _vIndicatorWeights;

    vector<double> _vRollAdjustment;
    vector<double> _vRollDelta;

    map< boost::gregorian::date, double > _mDailySpreadStats;
    map< boost::gregorian::date, DailyLegStat > _mDailyTargetStats;
    map< boost::gregorian::date, vector<DailyLegStat> > _mDailyLegStats;

    map< boost::gregorian::date, double> _mDailyTargetMid;
    map< boost::gregorian::date, vector<double> > _mDailyLegMid;

    bool _bUpdateStats;

    string _sIndicatorStr;

    vector<KOEpochTime> _vLastIndicatorUpdateTime;
};

}

#endif /* SLRL_H */
