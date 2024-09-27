#ifndef Liquidator_H
#define Liquidator_H

#include "../EngineInfra/SimpleLogger.h"
#include "../EngineInfra/TradeEngineBase.h"
#include "../EngineInfra/OrderParentInterface.h"
#include "../EngineInfra/KOOrder.h"
#include "../EngineInfra/KOEpochTime.h"
#include <map>

namespace KO
{

class Liquidator : public TradeEngineBase
{
public:
    Liquidator(const string& sEngineRunTimePath,
               const string& sEngineSlotName,
               KOEpochTime cTradingStartTime,
               KOEpochTime cTradingEndTime,
               SchedulerBase* pScheduler,
               string sTodayDate,
               PositionServerConnection* pPositionServerConnection);

    ~Liquidator();    

    virtual void dayInit();
    virtual void dayRun();
    virtual void dayStop();

    void readFromStream(istream& is);
    void receive(int iCID);
    void wakeup(KOEpochTime cCallTime);

    void onLimitLiqAllSignal();
    void onFastLiqAllSignal();
    void onLimitLiqSlotsSignal();
    void onFastLiqSlotsSignal();
    void onOffSignal();

    void manageLiquidationOrders(long iProductIndex);

    void orderConfirmHandler(int iOrderID);
    void orderFillHandler(int iOrderID, long iFilledQty, double dPrice);
    void orderRejectHandler(int iOrderID);
    void orderDeleteHandler(int iOrderID);
    void orderDeleteRejectHandler(int iOrderID);
    void orderAmendRejectHandler(int iOrderID);
    void orderUnexpectedConfirmHandler(int iOrderID);
    void orderUnexpectedDeleteHandler(int iOrderID);

    void positionRequestCallback(string sProduct, string sAccount, long iPosition, bool bTimeout);

    void liquidateSlot(string sSlot);

private:
    enum LiquidationState {OFF, LIMIT_ALL, FAST_ALL, LIMIT_SLOTS, FAST_SLOTS};

    void requestPosition();
    double dgetLiquidationPrice(long iProductIndex);
    long igetTotalLiquidationQty(long iProductIndex);

    SimpleLogger _cLogger;

    vector< vector<KOOrderPtr> > _vLiquidationOrders;

    vector<long> _vPositionToLiquidate;

    LiquidationState _eLiquidationState;

    vector<string> _vLiquidatingSlot;

    map<string, map<string, long> > _mProductAccountPositions;
};

}

#endif
