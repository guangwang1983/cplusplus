#include <iostream>
#include <boost/math/special_functions/round.hpp>

#include "IOCOrder.h"

namespace KO
{

IOCOrder::IOCOrder(boost::shared_ptr<ContractAccount> pContractAccount)
:_pContractAccount(pContractAccount)
{

}

bool IOCOrder::bsubmitOrder(long iQty, long iPrice)
{
    bool bResult = false;

    _pBasicOrder = _pContractAccount->psubmitOrderInTicks(iQty, iPrice, true);

    if(_pBasicOrder.get())
    {
        bResult = true;
    }

    return bResult;
}

long IOCOrder::igetOrderID()
{
	long iResult = -1;

	if(_pBasicOrder.get())
	{
    	iResult = _pBasicOrder->igetOrderID();
	}

	return iResult;
}

long IOCOrder::igetOrderPrice()
{
    long iResult = 0;

    if(_pBasicOrder.get())
    {
        iResult = _pBasicOrder->igetOrderPriceInTicks();
    }

    return iResult;
}

long IOCOrder::igetOrderRemainQty()
{
	long iResult = 0;

	if(_pBasicOrder.get())
	{
		iResult = _pBasicOrder->igetOrderRemainQty();
	}

	return iResult;
}

}
