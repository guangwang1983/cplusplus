#ifndef SLRD_H
#define SLRD_H

#include "../EngineInfra/TradeEngineBase.h"
#include "../base/Instrument.h"
#include "../base/MultiLegProduct.h"
#include "../EngineInfra/SimpleLogger.h"
#include "../SDBase/SDBase.h"
#include <vector>
#include <deque>
#include <map>

using namespace std;

namespace KO
{

class SLRD : public SDBase
{

struct DailyLegStat
{
	double dLegEXMA;
    long   iLegEXMANumDataPoints;
	double dLegStd;
};

public:
    SLRD(const string& sEngineRunTimePath,
         const string& sEngineSlotName,
         KOEpochTime cTradingStartTime,
         KOEpochTime cTradingEndTime,
         SchedulerBase* pScheduler,
         string sTodayDate,
         const string& sSimType);

	virtual ~SLRD();

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
	void saveOvernightStats();

    void writeSpreadLog();

    void loadDirectionalSettings();

    void loadPrevRollDelta();

    void updateSignal();

    vector<double> vmultiplyPCAMatrix(vector<double>& vInputVector);

    vector<Instrument*> _vInstruments;
    vector<bool> _vInstrumentsStaled;
    vector<double> _vLastInstrumentsMid;
    vector<long> _vTotalInstrumentsUpdates;

    vector<double> _vInputMeans;
    vector<double> _vInputStdevs;

    vector<deque<double>> _vIndictorsDiff;
    long _iDiffLimit;

    vector<double> _vRollAdjustment;
    vector<double> _vRollDelta;

    // string key is the instrument index
    vector<pair<string, vector<int>>> _vIndictorDiffSettings;

    double         _dRegressionBias;
    vector<double> _vPCAOutputWeights;
    vector<vector<double>> _vPCAMatrix;

    map< boost::gregorian::date, vector<DailyLegStat> > _mDailyLegStats;
    map< boost::gregorian::date, vector<double> > _mDailyLegMid;

    map< int, double > _mTriggerDict;

    double _dDirectionSignal;

    bool _bUpdateStats;

    double _dSignalStdev;
};

}

#endif /* SLRD_H */
