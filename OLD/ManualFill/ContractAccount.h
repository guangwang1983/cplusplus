#ifndef ContractAccount_H
#define ContractAccount_H

#include <string>
#include <deque>
#include "KOEpochTime.h"
#include "TradeEngineBase.h"
#include "Trade.h"
#include "OrderParentInterface.h"
#include "KOOrder.h"
#include "QuoteData.h"

using std::vector;
using std::deque;

namespace KO
{

class SchedulerBase;

class ContractAccount : public OrderParentInterface
{

friend class SchedulerBase;

public:
    enum AccountState
    {
        Trading,
        Halt
    };

    ContractAccount();
    ContractAccount(SchedulerBase* pScheduler,
                    TradeEngineBase* pParent, 
                    QuoteData* pQuoteData,
                    const string& sEngineSlotName, 
                    const string& sProduct, 
                    const string& sExchange, 
                    double dTickSize, 
                    long iMaxOrderSize,
                    long iLimit,
                    double iPnlLimit,
                    const string& sTradingAccount,
                    bool bIsLiveTrading);
    ~ContractAccount();

    const string& sgetAccountProductName();
    const string& sgetAccountExchangeName();
    const string& sgetRootSymbol();
    double dgetTickSize();
    const string& sgetAccountName();
    int igetCID();

    KOOrderPtr psubmitOrder(long iQty, double dPrice, bool bIsIOCOrder = false);
    KOOrderPtr psubmitOrderInTicks(long iQty, long iPriceInTicks, bool bIsIOCOrder = false);
    bool bchangeOrderPrice(KOOrderPtr pOrder, double dNewPrice);
    bool bchangeOrderPriceInTicks(KOOrderPtr pOrder, long iNewPriceInTicks);
    bool bchangeOrder(KOOrderPtr pOrder, double dNewPrice, long iNewOrderSize);
    bool bchangeOrderInTicks(KOOrderPtr pOrder, long iNewPriceInTicks, long iNewOrderSize);
    bool bdeleteOrder(KOOrderPtr pOrder);

    void orderConfirmHandler(int iOrderID);
    void orderFillHandler(int iOrderID, long iFilledQty, double dPrice);
    void manualFillHandler(int iFilledQty, double dPrice);
    void orderRejectHandler(int iOrderID);
    void orderDeleteHandler(int iOrderID);
    void orderDeleteRejectHandler(int iOrderID);
    void orderAmendRejectHandler(int iOrderID);
    void orderCriticalErrorHandler(int iOrderID);
    void orderUnexpectedConfirmHandler(int iOrderID);
    void orderUnexpectedDeleteHandler(int iOrderID);
    void haltTrading();
    void resumeTrading();
    void pullAllActiveOrders();

    long igetCurrentPosition();
    bool bgetPositionInitialised();
    void setInitialPosition(long iInitialPosition);

    void retrieveDailyTrades(vector< boost::shared_ptr<Trade> >& vTargetVector);
    long igetDailyOrderActionCount();    

    void checkOrderStatus();

    double dGetLastPnL();
    double dGetCurrentPnL(double dMTMRatio);
    long iGetCurrentVolume();

    vector<KOOrderPtr>* pgetAllLiveOrders();
private:
    bool bcheckRisk(long iQty);

    bool bcheckMessageFrequency();
    bool bcheckSubmitFrequency();

    void checkCriticalErrors();

    TradeEngineBase* _pParent;
    QuoteData* _pQuoteData;

    string _sEngineSlotName;
    string _sProduct;
    string _sExchange;
    double _dTickSize;
    long   _iMaxOrderSize;
    long   _iLimit;
    double _dPnlLimit;
    string _sTradingAccount;

    bool   _bPositionInitialised;
    long   _iPosition;
    long   _iNumOrderActions;
    double _dRealisedPnL;
    double _dLastMtMPnL;
    long   _iTradedVolume;

    long   _iNumOfCriticalErrors;

    vector<KOOrderPtr> _vLiveOrders;
    vector<KOOrderPtr> _vDeletedOrders;

    vector< boost::shared_ptr<Trade> > _vTrades;

    deque<KOEpochTime> _qOrderSubmitMinuteHistorty;
    deque<KOEpochTime> _qOrderMessageHistorty;

    unsigned int   _iNumSubmitPerMinutePerModel;
    unsigned int   _iNumMessagePerSecondPerModel;

    AccountState _eAccountState;
    bool _bIsLiveTrading;

    bool _bExcessiveOrderHaltTriggered;

    SchedulerBase* _pScheduler;
};

}

#endif /* ContractAccount_H */
