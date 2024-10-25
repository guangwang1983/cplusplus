#ifndef DataRecorder_H
#define DataRecorder_H

#include "../EngineInfra/TradeEngineBase.h"
#include "../EngineInfra/KOEpochTime.h"
#include "../EngineInfra/SimpleLogger.h"

namespace KO
{

class DataRecorder : public TradeEngineBase
{
public:
    DataRecorder(const string& sEngineRunTimePath,
                      const string& sEngineSlotName,
                      KOEpochTime cTradingStartTime,
                      KOEpochTime cTradingEndTime,
                      SchedulerBase* pScheduler,
                      string sTodayDate,
                      const string& sSimType);

    ~DataRecorder();

    virtual void dayInit();
    virtual void dayRun();
    virtual void dayStop();

    void readFromStream(istream& is);
    void receive(int iCID);
    void wakeup(KOEpochTime cCallTime);

private:
    long _iPrevAccumTradeVolume;
    string _sTodayDate;
    SimpleLogger _cMarketDataLogger;
    
    bool _bCheckDataStaled;
    bool _bDataStaledErrorTriggered;
    KOEpochTime _cLastDataPointTime;

    int _iFlushPosition;
    int _iFlushBufferSec;

    long _iNumTimerCallsTriggered;
};

}

#endif
