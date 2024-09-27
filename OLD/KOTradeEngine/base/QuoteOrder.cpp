#include <iostream>
#include <boost/math/special_functions/round.hpp>

#include "QuoteOrder.h"

namespace KO
{

QuoteOrder::QuoteOrder(boost::shared_ptr<ContractAccount> pContractAccount)
:_pContractAccount(pContractAccount)
{

}

bool QuoteOrder::bsubmitQuote(long iQty, long iPrice)
{
    bool bResult = false;

    _pBasicOrder = _pContractAccount->psubmitOrderInTicks(iQty, iPrice);

    if(_pBasicOrder.get())
    {
        bResult = true;
    }

    return bResult;
}

bool QuoteOrder::bchangeQuotePrice(long iNewPrice)
{
    bool bResult = false;

    if(iNewPrice != igetOrderPrice())
    {
        bResult = _pContractAccount->bchangeOrderPriceInTicks(_pBasicOrder, iNewPrice);
    }

    return bResult;
}

bool QuoteOrder::bchangeQuote(long iNewPrice, long iNewSize)
{
    bool bResult = false;

    if(iNewSize != igetOrderRemainQty() || iNewPrice != igetOrderPrice())
    {
        bResult = _pContractAccount->bchangeOrderInTicks(_pBasicOrder, iNewPrice, iNewSize);
    }
    else
    {
        // TODO order cannot be changed now. save cached order action, set a timer and change it again
    }

    return bResult;
}

bool QuoteOrder::bdeleteQuote()
{
    bool bResult = false;

    bResult = _pContractAccount->bdeleteOrder(_pBasicOrder);

    return bResult;
}

bool QuoteOrder::bquoteCanBeChanged()
{
    return _pBasicOrder->borderCanBeChanged();
}

long QuoteOrder::igetOrderID()
{
	long iResult = -1;

	if(_pBasicOrder.get())
	{
    	iResult = _pBasicOrder->igetOrderID();
	}

	return iResult;
}

long QuoteOrder::igetOrderPrice()
{
    long iResult = 0;

    if(_pBasicOrder.get())
    {
        iResult = _pBasicOrder->igetOrderPriceInTicks();
    }

    return iResult;
}

long QuoteOrder::igetOrderRemainQty()
{
	long iResult = 0;

	if(_pBasicOrder.get())
	{
		iResult = _pBasicOrder->igetOrderRemainQty();
	}

	return iResult;
}

}
