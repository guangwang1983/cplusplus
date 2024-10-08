#include <iostream>
#include <boost/math/special_functions/round.hpp>

#include "KOOrder.h"
#include "ErrorHandler.h"
#include "SystemClock.h"

namespace KO
{
KOOrder::KOOrder(unsigned long iOrderID, const string& sSlot, int iCID, short iGatewayID, int64_t iSecurityID, double dTickSize, bool bIsIOC, const string& sProduct, const string& sAccount, const string& sExchange, InstrumentType eInstrumentType, OrderParentInterface* pParent)
:_pHCOrder(NULL),
 _eInstrumentType(eInstrumentType),
 _iGatewayID(iGatewayID),
 _iSecurityID(iSecurityID),
 _eOrderState(INACTIVE),
 _iOrderID(iOrderID),
 _sSlot(sSlot),
 _iCID(iCID),
 _bIsIOC(bIsIOC),
 _sProduct(sProduct),
 _sExchange(sExchange),
 _sAccount(sAccount),
 _dTickSize(dTickSize),
 _pParent(pParent),
 _iOrderRemainingQty(0),
 _dOrderPrice(-100000),
 _iOrderPriceInTicks(-100000),
 _iOrderPendingQty(0),
 _dOrderPendingPrice(-100000),
 _iOrderPendingPriceInTicks(-100000),
 _iOrderConfirmedQty(0),
 _dOrderConfirmedPrice(-100000),
 _iOrderConfirmedPriceInTicks(-100000),
 _bOrderNoReplyTriggered(false),
 _bOrderNoFillTriggered(false),
 _iQueuePosition(500000),
 _bIsKOOrder(false)
{

}

InstrumentType KOOrder::egetInstrumentType()
{
    return _eInstrumentType;
}

void KOOrder::setIsKOOrder(bool bIsKOOrder)
{
    _bIsKOOrder = bIsKOOrder;
}

int KOOrder::igetOrderID()
{
    return _iOrderID;
}

int KOOrder::igetProductCID()
{
    return _iCID;
}

bool KOOrder::bgetIsIOC()
{
    return _bIsIOC;
}

const string& KOOrder::sgetOrderSlotName()
{
    return _sSlot;
}

const string& KOOrder::sgetOrderProductName()
{
    return _sProduct;
}

const string& KOOrder::sgetOrderExchange()
{
    return _sExchange;
}

const string& KOOrder::sgetOrderAccount()
{
    return _sAccount;
}

void KOOrder::changeOrderstat(OrderState eNewOrderState)
{
    _eOrderState = eNewOrderState;

    if(eNewOrderState == PENDINGCREATION || 
       eNewOrderState == PENDINGDELETE || 
       eNewOrderState == PENDINGCHANGE)
    {
        _cPendingRequestTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
    }

    updateOrderRemainingQty();
    updateOrderPrice();
}

long KOOrder::igetOrderRemainQty()
{
    return _iOrderRemainingQty;
}

double KOOrder::dgetOrderPrice()
{
    return _dOrderPrice;    
}

long KOOrder::igetOrderPriceInTicks()
{
    return _iOrderPriceInTicks;
}

bool KOOrder::borderCanBeChanged()
{
    return _eOrderState == ACTIVE; 
}

void KOOrder::updateOrderRemainingQty()
{
    if(_bIsKOOrder == false)
    {
        if(_eOrderState == ACTIVE || 
           _eOrderState == PENDINGCREATION || 
           _eOrderState == PENDINGDELETE ||
           _eOrderState == PENDINGCHANGE)
        {
            _iOrderRemainingQty = _pHCOrder->getAmountLeft();

            if(_pHCOrder->getAction() == ACTION::SELL)
            {
                _iOrderRemainingQty = _iOrderRemainingQty * -1;
            }
        }
        else if(_eOrderState == INACTIVE)
        {
            _iOrderRemainingQty = 0;
        }
    }
    else
    {
        if(_eOrderState == INACTIVE)
        {
            _iOrderRemainingQty = 0;
        }
        else if(_eOrderState == PENDINGCREATION)
        {
            _iOrderRemainingQty = _iOrderPendingQty;
        }
        else
        {
            _iOrderRemainingQty = _iOrderConfirmedQty;
        }
    }
}

void KOOrder::updateOrderPrice()
{
    if(_bIsKOOrder == false)
    {
        if(_eOrderState == ACTIVE || 
           _eOrderState == PENDINGCREATION || 
           _eOrderState == PENDINGDELETE ||
           _eOrderState == PENDINGCHANGE)
        {
            _dOrderPrice = _pHCOrder->getOrderRate();
        }
        else if(_eOrderState == INACTIVE)
        {
            _dOrderPrice = 0;
        }
    }
    else
    {
        if(_eOrderState == INACTIVE)
        {
            _dOrderPrice = 0;
        }
        else if(_eOrderState == PENDINGCREATION)
        {
            _dOrderPrice = _dOrderPendingPrice;
        }
        else
        {
            _dOrderPrice = _dOrderConfirmedPrice;
        }
    }

    _iOrderPriceInTicks = boost::math::iround(_dOrderPrice / _dTickSize);
}

}
