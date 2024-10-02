#include <iostream>
#include <boost/math/special_functions/round.hpp>

#include "KOOrder.h"
#include "ErrorHandler.h"
#include "SystemClock.h"

namespace KO
{
KOOrder::KOOrder(const string& sPendingOrderID, int iCID, double dTickSize, bool bIsIOC, const string& sProduct, const string& sAccount, const string& sExchange, InstrumentType eInstrumentType, OrderParentInterface* pParent)
:_eInstrumentType(eInstrumentType),
 _eOrderState(INACTIVE),
 _sPendingOrderID(sPendingOrderID),
 _sConfirmedOrderID("NONE"),
 _sTBOrderID("NONE"),
 _iCID(iCID),
 _bIsIOC(bIsIOC),
 _sProduct(sProduct),
 _sExchange(sExchange),
 _sAccount(sAccount),
 _dTickSize(dTickSize),
 _pParent(pParent),
 _iOrderRemainQty(0),
 _dOrderPrice(-100000),
 _iOrderPriceInTicks(-100000),
 _bOrderNoReplyTriggered(false),
 _bOrderNoFillTriggered(false)
{
}

InstrumentType KOOrder::egetInstrumentType()
{
    return _eInstrumentType;
}

const string& KOOrder::sgetPendingOrderID()
{
    return _sPendingOrderID;
}

const string& KOOrder::sgetConfirmedOrderID()
{
    return _sConfirmedOrderID;
}

const string& KOOrder::sgetTBOrderID()
{
    return _sTBOrderID;
}

int KOOrder::igetProductCID()
{
    return _iCID;
}

bool KOOrder::bgetIsIOC()
{
    return _bIsIOC;
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

KOOrder::OrderState KOOrder::egetOrderstate()
{
    return _eOrderState;
}

long KOOrder::igetOrderOrgQty()
{
    return _iOrderOrgQty;
}

long KOOrder::igetOrderRemainQty()
{
    return _iOrderRemainQty;
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

}
