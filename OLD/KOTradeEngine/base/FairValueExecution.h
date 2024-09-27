#ifndef FairValueExecution_H
#define FairValueExecution_H

#include "Instrument.h"
#include "QuoteOrder.h"
#include "IOCOrder.h"
#include "../EngineInfra/SimpleLogger.h"

namespace KO
{

class FairValueExecution : public OrderParentInterface
{

public:
    enum ExecutionState
    {
        Quoting,
        PatientQuoting,
        Hitting,
        PatientHitting,
        IOC,
        PatientIOC,
        Off
    };

    enum HittingSignalStat
    {
        BUY_ENTRY,
        SELL_ENTRY,
        FLAT
    };

    enum OrderType
    {
        OPEN_BUY,
        OPEN_SELL,
        CLOSE_BUY,
        CLOSE_SELL
    };

    typedef boost::shared_ptr<QuoteOrder> QuoteOrderPtr;
    typedef boost::shared_ptr<IOCOrder> IOCOrderPtr;

    FairValueExecution(boost::shared_ptr<ContractAccount> pTargetAccount, string& sEngineSlotName, Instrument* pInstrument, long iQuoteQty, long lInitPosition, TradeEngineBase* pParent, SimpleLogger* pLogger);
    ~FairValueExecution();

    void setPosition(long iNewPosition);

    void wakeup();
    void orderConfirmHandler(int iOrderID);
    void orderFillHandler(int iOrderID, long iFilledQty, double dPrice);
    void orderRejectHandler(int iOrderID);
    void orderDeleteHandler(int iOrderID);
    void orderDeleteRejectHandler(int iOrderID);
    void orderAmendRejectHandler(int iOrderID);
    void orderCriticalErrorHandler(int iOrderID);
    void orderUnexpectedConfirmHandler(int iOrderID);
    void orderUnexpectedDeleteHandler(int iOrderID);

    void setExecutionState(ExecutionState eExecutionState);
    ExecutionState egetExecutionState();

    void setTheoPosition(long iNewTheoPosition);
    void newTheoQuotePrices(double dNewOpenBuyPrice, double dNewOpenSellPrice, double dNewCloseBuyPrice, double dNewCloseSellPrice, bool bApplyNewPrices);
    void updateOrders();

    long igetTotalOpenOrderQty();

    void setEstablishSegEntryPos(bool bEstablishSegEntryPos);
private:
    QuoteOrderPtr lookUpOrder(string& sOutputOrderType, int iOrderID);

    void pullOpenBuyQuote();
    void pullOpenSellQuote();
    void pullExitBuyQuote();
    void pullExitSellQuote();

    bool bsubmitIOC(long iNetPosChange, long iOrderPrice, OrderType eOrderType);

    bool bsubmitQuote(long iNetPosChange, long iOrderPrice, OrderType eOrderType);
    bool bamendQuote(long iNetPosChange, long iOrderPrice, OrderType eOrderType);
    void pullAllQuotes();
    void pullAllOpenQuotes();

    void adjustTheoQuotePrices();

    void updateBuyQuotePrice();
    void updateSellQuotePrice();
    void updateQuotePrice();

    long igetOpenBuyQuoteQty();
    long igetOpenSellQuoteQty();
    long igetCloseBuyQuoteQty();
    long igetCloseSellQuoteQty();

    long igetOpenBuyIOCQty();
    long igetOpenSellIOCQty();
    long igetCloseBuyIOCQty();
    long igetCloseSellIOCQty();

    long _iTheoBid;
    long _iTheoOffer;
    long _iTheoExitBid;
    long _iTheoExitOffer;

    long _iPrevBidPrice;
    long _iPrevOfferPrice;
    long _iPrevBidExitPrice;
    long _iPrevOfferExitPrice;

    long _iCurrentBidPrice;
    long _iCurrentOfferPrice;
    long _iCurrentBidExitPrice;
    long _iCurrentOfferExitPrice;

    bool _bEstablishSegEntryPos;
    long _iQuoteQty;
    long _iTheoPosition;
    long _iPosition;

    string _sEngineSlotName;

    boost::shared_ptr<ContractAccount>  _pTargetAccount;
    Instrument* _pInstrument;
    
    QuoteOrderPtr _pOpenBuyQuote;
    QuoteOrderPtr _pOpenSellQuote;
    QuoteOrderPtr _pCloseBuyQuote;
    QuoteOrderPtr _pCloseSellQuote;

    IOCOrderPtr _pIOCOpenBuyOrder;
    IOCOrderPtr _pIOCOpenSellOrder;
    IOCOrderPtr _pIOCCloseBuyOrder;
    IOCOrderPtr _pIOCCloseSellOrder;
   
    ExecutionState _eExecutionState;  
    HittingSignalStat _eHittingSignalState;
 
    SimpleLogger*       _pLogger;
    TradeEngineBase*    _pParent;
};

}

#endif /* FairValueExecution_H */
