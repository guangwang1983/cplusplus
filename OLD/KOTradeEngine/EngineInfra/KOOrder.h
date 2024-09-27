#ifndef KOOrder_H
#define KOOrder_H

#include <string>

#include "KOEpochTime.h"
#include "OrderParentInterface.h"
#include <boost/shared_ptr.hpp>
#include <framework/Order.h>
#include "QuoteData.h"

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
    friend class KOScheduler;
    friend class SchedulerBase;
    friend class SimulationExchange;

    KOOrder(unsigned long iOrderID, const string& sSlot, int iCID, short iGatewayID, int64_t iSecurityID, double dTickSize, bool bIsIOC, const string& sProduct, const string& sAccount, const string& sExchange, InstrumentType eInstrumentType, OrderParentInterface* pParent);

    InstrumentType egetInstrumentType();
    int igetOrderID();
    int igetProductCID();
    long igetOrderRemainQty();
    double dgetOrderPrice();
    long igetOrderPriceInTicks();
    bool bgetIsIOC();
    const string& sgetOrderSlotName();
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

    InstrumentType _eInstrumentType;

    short _iGatewayID;
    int64_t _iSecurityID;

    OrderState _eOrderState;

    int _iOrderID;
    string _sSlot;
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
