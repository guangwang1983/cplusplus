#ifndef GridDataGenerator_H
#define GridDataGenerator_H

#include "../EngineInfra/TradeEngineBase.h"
#include "../EngineInfra/KOEpochTime.h"

namespace KO
{

class GridDataGenerator : public TradeEngineBase
{
public:
    GridDataGenerator(const string& sEngineRunTimePath,
                      const string& sEngineSlotName,
                      KOEpochTime cTradingStartTime,
                      KOEpochTime cTradingEndTime,
                      SchedulerBase* pScheduler,
                      string sTodayDate,
                      PositionServerConnection* pPositionServerConnection);

    ~GridDataGenerator();

    virtual void dayInit();
    virtual void dayRun();
    virtual void dayStop();

    void readFromStream(istream& is);
    void receive(int iCID);
    void wakeup(KOEpochTime cCallTime);

private:
    long iprevBidSize;
    long iprevBidInTicks;
    long iprevAskInTicks;
    long iprevAskSize;
    long iprevLastInTicks;
    long iprevVolume;

    int _iStartTimeInSecond;
    int _iGridFreq;
    long _iNumberOfUpdateReceived;
    string _sGridFileContent;
    string _sGridFileOutputPath;

    vector<GridData> _vGridData;

    double _dLastLowBid;
    long _iLastLowBidInTicks;
    long _iLastLowBidSize;

    double _dLastHighBid;
    long _iLastHighBidInTicks;
    long _iLastHighBidSize;

    double _dLastHighAsk;
    long _iLastHighAskInTicks;
    long _iLastHighAskSize;

    double _dLastLowAsk;
    long _iLastLowAskInTicks;
    long _iLastLowAskSize;

    double _dVWAPConsideration;
    long _iVWAPVolume;

    std::stringstream _cOutputStringStream;

    double _dSumAllSpread;
    long _iNumberSpreadSample;

    double _dHigh;
    double _dLow;
    long _iNumberInvalidUpdates;
};

}

#endif
