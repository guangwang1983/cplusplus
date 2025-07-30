#ifndef QuickFixSchedulerFXMultiBook_H
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

class QuickFixSchedulerFXMultiBook : public FIX::Application, 
                          public FIX::MessageCracker,
                          public SchedulerBase
{

public:
    QuickFixSchedulerFXMultiBook(SchedulerConfig &cfg, bool bIsLiveTrading);
    ~QuickFixSchedulerFXMultiBook();

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
    virtual void onLogout(const SessionID& cSessionID);
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

private:
   
    struct SessionDetails
    {
        const FIX::SessionID* pFixSessionID;
        string sSenderCompID;
        bool bIsLoggedOn;
    };

    void checkProductsForPriceSubscription();

    void updateOrderPrice(unsigned int iProductIdx);
    void updateLiquidationOrderPrice(unsigned int iProductIdx);    

    void submitOrderBestPriceMultiBook(unsigned int iProductIdx, long iQty, bool bIsLiquidation);

    long icalcualteOrderPrice(unsigned int iProductIdx, long iOrderPrice, long iQty, bool bOrderInMarket, bool bIsLiquidation, bool bIsIOC);

    bool bcheckRisk(unsigned int iProductIdx, long iNewQty);
    void checkOrderState(KOEpochTime cCurrentTime);    
    void resetOrderState();
    virtual void updateAllPnL();
    bool bcheckOrderMsgHistory(KOOrderPtr pOrder);

    void updateQuoteDataSubscribed();
    void calculateTriangPrices();

    bool _bIsLiveTrading;
    bool _bScheduleFinished;

    vector<SessionDetails> _vMDSessions;
    const FIX::SessionID* _pOrderSessionID;
    string _sOrderSenderCompID;
    bool _bIsOrderSessionLoggedOn;

    vector<vector<KOOrderPtr>> _vProductOrderList;
    vector<int> _vProductDesiredPos;

    vector<vector<KOOrderPtr>> _vProductLiquidationOrderList;
    vector<int> _vProductLiquidationDesiredPos;

    vector<string> _vLastOrderError;

    vector<KOEpochTime> _vFirstOrderTime;

    map<unsigned int, vector<QuoteData>> _vProductMultiBooks;

    long _iTotalNumMsg;
    long _iNumTimerCallsReceived;

    SimpleLogger _cSubBookMarketDataLogger;
};

}

#endif
