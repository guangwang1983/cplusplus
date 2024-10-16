#ifndef QuickFixScheduler_H
#define QuickFixHCScheduler_H

#include "quickfix/Application.h"
#include "quickfix/MessageCracker.h"
#include "quickfix/fix44/MarketDataRequest.h"
#include "quickfix/fix44/ExecutionReport.h"
#include "quickfix/fix44/BusinessMessageReject.h"
#include "quickfix/fix44/Reject.h"
#include "quickfix/fix44/MarketDataSnapshotFullRefresh.h"
#include "quickfix/fix44/MarketDataRequestReject.h"
#include "quickfix/fix44/OrderCancelReject.h"
#include "quickfix/fix44/Logout.h"
#include "SchedulerBase.h"
#include "KOOrder.h"

using namespace KO;
using namespace FIX;
using namespace std;

namespace KO
{

class QuickFixScheduler : public FIX::Application, 
                          public FIX::MessageCracker,
                          public SchedulerBase
{

public:
    QuickFixScheduler(SchedulerConfig &cfg, bool bIsLiveTrading);
    ~QuickFixScheduler();

    void init();

    virtual KOEpochTime cgetCurrentTime();
    virtual bool bisLiveTrading();

    virtual bool sendToExecutor(const string& sProduct, long iDesiredPos);
    virtual bool sendToLiquidationExecutor(const string& sProduct, long iDesiredPos);
    virtual void assignPositionToLiquidator(const string& sProduct, long iPosToLiquidate);
    virtual void exitScheduler();

    bool bschedulerFinished();

    void onTimer();

    virtual void onCreate(const SessionID& cSessionID);
    virtual void onLogon(const SessionID& cSessionID);
    virtual void onLogout(const SessionID&);
    virtual void toAdmin(Message& cMessage, const SessionID& cSessionID);
    virtual void toApp(Message& cMessage, const SessionID& cSessionID) throw(DoNotSend);
    virtual void fromAdmin(const Message& cMessage, const SessionID& cSessionID) throw(FieldNotFound, IncorrectDataFormat, IncorrectTagValue, RejectLogon);
    virtual void fromApp(const Message& cMessage, const SessionID& cSessionID) throw(FieldNotFound, IncorrectDataFormat, IncorrectTagValue, UnsupportedMessageType);

    virtual void onMessage(const FIX44::MarketDataSnapshotFullRefresh& cMarketDataSnapshotFullRefresh, const FIX::SessionID& cSessionID);
    virtual void onMessage(const FIX44::ExecutionReport& cExecutionReport, const FIX::SessionID& cSessionID);
    virtual void onMessage(const FIX44::BusinessMessageReject& cBusinessMessageReject, const FIX::SessionID& cSessionID);
    virtual void onMessage(const FIX44::OrderCancelReject& cOrderCancelReject, const FIX::SessionID& cSessionID);
    virtual void onMessage(const FIX44::Logout& cLogout, const FIX::SessionID& cSessionID);
    virtual void onMessage(const FIX44::MarketDataRequestReject& cReject, const FIX::SessionID& cSessionID);
    
    virtual void onMessage(FIX44::BusinessMessageReject& cBusinessMessageReject, const FIX::SessionID& cSessionID);
    virtual void onMessage(FIX44::Reject& cReject, const FIX::SessionID& cSessionID);

    bool bgetMarketDataSessionLoggedOn();
    bool bgetMarketDataSubscribed();
    bool bgetOrderSessionLoggedOn();
    bool bgetOrderSubmitted();

private:
    void updateOrderPrice(unsigned int iProductIdx);
    void updateLiquidationOrderPrice(unsigned int iProductIdx);    

    void submitOrderBestPrice(unsigned int iProductIdx, long iQty, bool bIsLiquidation);
    void deleteOrder(KOOrderPtr pOrder);
    void amendOrder(KOOrderPtr pOrder, long iQty, long iPriceInTicks);
    void amendOrderPriceInTicks(KOOrderPtr pOrder, long iPriceInTicks);
    long icalcualteOrderPrice(unsigned int iProductIdx, long iOrderPrice, long iQty, bool bOrderInMarket, bool bIsLiquidation);

    bool bcheckRisk(unsigned int iProductIdx, long iNewQty);
    void checkOrderState(KOEpochTime cCurrentTime);    
    void resetOrderState();
    virtual void updateAllPnL();
    bool bcheckOrderMsgHistory(KOOrderPtr pOrder);

    bool _bIsLiveTrading;
    bool _bScheduleFinished;

    const FIX::SessionID* _pMarketDataSessionID;
    const FIX::SessionID* _pOrderSessionID;
    string _sOrderSenderCompID;
    string _sMDSenderCompID;

    vector<vector<KOOrderPtr>> _vProductOrderList;
    vector<int> _vProductDesiredPos;

    vector<vector<KOOrderPtr>> _vProductLiquidationOrderList;
    vector<int> _vProductLiquidationDesiredPos;

    vector<string> _vLastOrderError;

    vector<KOEpochTime> _vFirstOrderTime;

    long _iTotalNumMsg;


    bool _bMarketDataSessionLoggedOn;
    bool _bOrderSessionLoggedOn;
    bool _bMarketDataSubscribed;
    bool _bOrderSubmitted;

    string _sOrderID;
    int _iTimeIndex;
};

}

#endif
