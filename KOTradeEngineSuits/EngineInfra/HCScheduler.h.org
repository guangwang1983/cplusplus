#ifndef HCScheduler_H
#define HCScheduler_H

#include <stdio.h>
#include <framework/Model.h>
#include "SchedulerBase.h"
#include "KOOrder.h"

namespace KO
{

class HCScheduler : public SchedulerBase,
                    public Model
{
public:
    HCScheduler(SchedulerConfig &cfg, bool bIsLiveTrading);
    ~HCScheduler();

    KOEpochTime cgetCurrentTime();
    bool bisLiveTrading();
    bool sendToExecutor(const string& sProduct, long iDesiredPos);
    bool sendToLiquidationExecutor(const string& sProduct, long iDesiredPos);
    void assignPositionToLiquidator(const string& sProduct, long iPosToLiquidate);
    void exitScheduler();

    virtual __attribute__((cold)) void initialize( ) override ;
    void addInstrument(unsigned int iProdConfigIdx, HC::instrumentkey_t instrumentKey);
    virtual void futuresMsgEvent(short gateway, HC_GEN::instrumentkey_t instrumentKey, FUTURES_EVENT_TYPE eventType, FUTURES_MSG_INFO_STRUCT* msgInfo) override;
    void onIBook(IBook *pBook) override;
    void handleSHSide(HC_GEN::SUPER_HEDGER_SIDE_STRUCT* s) { (void)s; };
    void onConflationDone() override ;

    void orderUpdate(Order* order);
    static void updateTimer(int iID, void * data);
    void onTimer();

    void resetOrderState();

    virtual void updateAllPnL();
private:
    void updateOrderPrice(unsigned int iProductIdx);
    void updateLiquidationOrderPrice(unsigned int iProductIdx);

    void submitOrderBestPrice(unsigned int iProductIdx, long iQty, bool bIsLiquidation);
    void amendOrder(KOOrderPtr pOrder, long iQty, long iPriceInTicks);
    void amendOrderPriceInTicks(KOOrderPtr pOrder, long iPriceInTicks);
    void deleteOrder(KOOrderPtr pOrder);

    bool bsubmitHCOrder(KOOrderPtr pOrder, double dPrice, long iQty);

    long icalcualteOrderPrice(unsigned int iProductIdx, long iOrderPrice, long iQty, bool bOrderInMarket, bool bIsLiquidation);

    string stranslateHCRejectReason(FuturesOrder::OrderRejectReason::OrderRejectReason cRejectReason);
    string stranslateHCErrorCode(ORDER_SEND_ERROR cHCErrorCode);
   
    void checkOrderState(KOEpochTime cCurrentTime);

    void newHCPriceUpdate(int i, const IBook *pBook);

    bool bcheckRisk(unsigned int iProductIdx, long iNewQty);
    bool bcheckOrderMsgHistory(KOOrderPtr pOrder);
 
    int _iNextCIDKey;
    bool _bSchedulerInitialised;

    vector<vector<KOOrderPtr>> _vProductOrderList;
    vector<int> _vProductDesiredPos;

    vector<vector<KOOrderPtr>> _vProductLiquidationOrderList;
    vector<int> _vProductLiquidationDesiredPos;

    vector<string> _vLastOrderError;

    vector<KOEpochTime> _vFirstOrderTime;

    string _sModelName;

    bool _bIsLiveTrading;

    long _iTotalNumMsg;
};

}

#endif
