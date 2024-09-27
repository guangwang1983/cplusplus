#ifndef SingleMarketOrder_H
#define SingleMarketOrder_H

#include "../base/Instrument.h"
#include "../base/HedgeOrder.h"
#include "../EngineInfra/TradeEngineBase.h"

namespace KO
{

class OrderTest : public TradeEngineBase
{
public:
    typedef boost::shared_ptr<Instrument> InstrumentPtr;
    typedef boost::shared_ptr<HedgeOrder> HedgeOrderPtr;

    OrderTest(const string& sEngineRunTimePath,
                      const string& sEngineSlotName,
                      KOEpochTime cTradingStartTime,
                      KOEpochTime cTradingEndTime,
                      SchedulerBase* pScheduler,
                      string sTodayDate,
                      PositionServerConnection* pPositionServerConnection);

    ~OrderTest();

    void dayInit();
    void dayRun();
    void dayStop();

    void readFromStream(istream& is);
    void receive(int iCID);
    void wakeup(KOEpochTime cCallTime);

    void orderConfirmHandler(int iOrderID);
    void orderFillHandler(int iOrderID, long iFilledQty, double dPrice);
    void orderRejectHandler(int iOrderID);
    void orderDeleteHandler(int iOrderID);
    void orderDeleteRejectHandler(int iOrderID);
    void orderAmendRejectHandler(int iOrderID);

private:
    KOOrderPtr pOrder;
};

}

#endif

