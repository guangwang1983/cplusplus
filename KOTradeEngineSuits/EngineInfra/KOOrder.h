#ifndef KOOrder_H
#define KOOrder_H

#include <string>

#include "KOEpochTime.h"
#include "OrderParentInterface.h"
#include <boost/shared_ptr.hpp>
#include <framework/Order.h>
#include "QuoteData.h"
#include <deque>

using std::string;

namespace KO
{

class KOOrder
{
public:
    enum OrderState
    {
        INACTIVE,
        ACTIVE,
        PENDINGCREATION,
        PENDINGCHANGE,
        PENDINGDELETE
    };

    friend class HCScheduler;
    friend class HCFXScheduler;
    friend class HCFXSchedulerMultiBook;
    friend class KOScheduler;
    friend class SchedulerBase;
    friend class SimulationExchange;

    KOOrder(unsigned long iOrderID, int iCID, HC::source_t iGatewayID, HC::instrumentkey_t iInstrumentKey, double dTickSize, bool bIsIOC, const string& sProduct, const string& sAccount, const string& sExchange, InstrumentType eInstrumentType, OrderParentInterface* pParent);

    InstrumentType egetInstrumentType();
    int igetOrderID();
    int igetProductCID();
    long igetOrderRemainQty();
    double dgetOrderPrice();
    long igetOrderPriceInTicks();
    bool bgetIsIOC();
    const string& sgetOrderAccount();
    const string& sgetOrderProductName();
    const string& sgetOrderExchange();
    void changeOrderstat(OrderState eNewOrderState);
   
    void setIsKOOrder(bool bIsKOOrder);

    virtual void updateOrderRemainingQty();
    virtual void updateOrderPrice();

    bool borderCanBeChanged();
    
protected:
    Order* _pHCOrder;
    OrderRouter* _pHCOrderRouter;

    InstrumentType _eInstrumentType;

    HC::source_t _iGatewayID;
    HC::instrumentkey_t _iInstrumentKey;

    OrderState _eOrderState;

    unsigned long _iOrderID;
    int _iCID;
    bool _bIsIOC;
    string _sProduct;
    string _sExchange;
    string _sAccount;
    double _dTickSize;
    OrderParentInterface* _pParent;

    KOEpochTime _cPendingRequestTime;

    long _iOrderRemainingQty;
    double _dOrderPrice;
    long _iOrderPriceInTicks;

    bool _bOrderNoReplyTriggered;

    bool _bOrderNoFill;
    KOEpochTime _cOrderNoFillTime;    
    bool _bOrderNoFillTriggered;

    std::deque<KOEpochTime> _qOrderMessageHistory;

/******************* used in KO SIM ******************/
    long _iOrderPendingQty;
    double _dOrderPendingPrice;
    long _iOrderPendingPriceInTicks;

    long _iOrderConfirmedQty;
    double _dOrderConfirmedPrice;
    long _iOrderConfirmedPriceInTicks;

    long _iQueuePosition;

    bool _bIsKOOrder;
/*****************************************************/
};

typedef boost::shared_ptr<KOOrder> KOOrderPtr;

}

#endif
