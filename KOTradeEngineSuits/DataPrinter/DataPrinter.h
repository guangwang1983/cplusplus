#ifndef DataPrinter_H
#define DataPrinter_H

#include "../EngineInfra/TradeEngineBase.h"
#include "../EngineInfra/KOEpochTime.h"

namespace KO
{

class DataPrinter : public TradeEngineBase
{
public:
    DataPrinter(const string& sEngineRunTimePath,
                      const string& sEngineSlotName,
                      KOEpochTime cTradingStartTime,
                      KOEpochTime cTradingEndTime,
                      SchedulerBase* pScheduler,
                      string sTodayDate,
                      const string& sSimType);

    ~DataPrinter();

    virtual void dayInit();
    virtual void dayRun();
    virtual void dayStop();

    void readFromStream(istream& is);
    void receive(int iCID);
    void wakeup(KOEpochTime cCallTime);

private:
};

}

#endif
