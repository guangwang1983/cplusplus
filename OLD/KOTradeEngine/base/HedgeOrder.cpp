#include "HedgeOrder.h"
#include <boost/math/special_functions/round.hpp>
#include "../EngineInfra/SystemClock.h"

namespace KO
{

HedgeOrder::HedgeOrder(boost::shared_ptr<ContractAccount> pContractAccount, Instrument* pInstrument, SimpleLogger* pLogger)
:_pContractAccount(pContractAccount),
 _pInstrument(pInstrument),
 _pLogger(pLogger)
{

}

bool HedgeOrder::bsubmitOrder(long iQty, long iEntryOffSet, long iStopDistance, long iMaxStopDistance)
{
	bool bResult = false;

	if(iStopDistance <= iMaxStopDistance)
	{
        _iOrderEntryQty = iQty; 

        if(iQty > 0)
        {
            _iOrderEntryPrice = _pInstrument->igetBestBid() + iEntryOffSet;
            _iStopPrice = _iOrderEntryPrice + iStopDistance;
            _iMaxStopPrice = _iOrderEntryPrice + iMaxStopDistance;
        }
        else
        {
            _iOrderEntryPrice = _pInstrument->igetBestAsk() - iEntryOffSet;
            _iStopPrice = _iOrderEntryPrice - iStopDistance;
            _iMaxStopPrice = _iOrderEntryPrice - iMaxStopDistance;
        }

        long iMarketStopPrice = iCalculateMarketStop(true);

        _pBasicOrder = _pContractAccount->psubmitOrderInTicks(iQty, iMarketStopPrice);

        if(_pBasicOrder.get())
        {
            bResult = true;
            (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Hedge Order submitted with market stop price " << iMarketStopPrice << "\n";
        }
	}

	return bResult;
}

bool HedgeOrder::bchangeHedgeOrderPrice(long iEntryOffSet, long iStopDistance, long iMaxStopDistance)
{
    bool bResult = false;

    if(iStopDistance <= iMaxStopDistance)
    {
        if(igetOrderRemainQty() > 0)
        {
            _iOrderEntryPrice = _pInstrument->igetBestBid() + iEntryOffSet;
            _iStopPrice = _iOrderEntryPrice + iStopDistance;
            _iMaxStopPrice = _iOrderEntryPrice + iMaxStopDistance;
        }
        else
        {
            _iOrderEntryPrice = _pInstrument->igetBestAsk() - iEntryOffSet;
            _iStopPrice = _iOrderEntryPrice - iStopDistance;
            _iMaxStopPrice = _iOrderEntryPrice - iMaxStopDistance;
        }

        long iMarketStopPrice = iCalculateMarketStop(true);

        if(iMarketStopPrice != igetOrderPrice())
        {
            if(bchangeOrderPrice(iMarketStopPrice))
            {
                (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Hedge Order Amended with market stop price " << iMarketStopPrice << "\n";
                bResult = true;
            }
        }
    }

    return bResult;
}

bool HedgeOrder::bchangeHedgeOrder(long iEntryOffSet, long iStopDistance, long iMaxStopDistance, long iNewSize)
{
    bool bResult = false;

    if(iStopDistance <= iMaxStopDistance)
    {
        if(iNewSize > 0)
        {
            _iOrderEntryPrice = _pInstrument->igetBestBid() + iEntryOffSet;
            _iStopPrice = _iOrderEntryPrice + iStopDistance;
            _iMaxStopPrice = _iOrderEntryPrice + iMaxStopDistance;
        }
        else
        {
            _iOrderEntryPrice = _pInstrument->igetBestAsk() - iEntryOffSet;
            _iStopPrice = _iOrderEntryPrice - iStopDistance;
            _iMaxStopPrice = _iOrderEntryPrice - iMaxStopDistance;
        }

        long iMarketStopPrice = iCalculateMarketStop(true);
        if(iMarketStopPrice != igetOrderPrice() || iNewSize != igetOrderRemainQty())
        {
            if(bchangeOrder(iMarketStopPrice, iNewSize))
            {
                (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Hedge Order Amended with market stop price " << iMarketStopPrice << "\n";
                bResult = true;
            }
        }
    }

    return bResult;
}

bool HedgeOrder::bchangeOrderPrice(long iNewPrice)
{
    bool bResult = false;

    if(borderCanBeChanged())
    {
        bResult = _pContractAccount->bchangeOrderPriceInTicks(_pBasicOrder, iNewPrice);
    }

    return bResult;
}

bool HedgeOrder::bchangeOrder(long iNewPrice, long iNewSize)
{
    bool bResult = false;

    if(borderCanBeChanged())
    {
        bResult = _pContractAccount->bchangeOrderInTicks(_pBasicOrder, iNewPrice, iNewSize);
    }

    return bResult;
}

bool HedgeOrder::bchangeHedgeOrderSize(long iNewSize)
{
    bool bResult = false;

    if(borderCanBeChanged())
    {
        if(iNewSize != igetOrderRemainQty())
        {
            long iOrderPrice = igetOrderPrice();
            bResult = _pContractAccount->bchangeOrderInTicks(_pBasicOrder, iOrderPrice, iNewSize);
        }
    }

    return bResult;
}

bool HedgeOrder::bdeleteOrder()
{
    bool bResult = false;

    if(borderCanBeChanged())
    {
        bResult = _pContractAccount->bdeleteOrder(_pBasicOrder);
    }

    return bResult;
}

long HedgeOrder::igetOrderID()
{
	long iResult = -1;

	if(_pBasicOrder.get())
	{
	    iResult = _pBasicOrder->igetOrderID();
	}

	return iResult;
}

long HedgeOrder::igetOrderPrice()
{
	long iResult = 0;

	if(_pBasicOrder.get())
	{
        iResult = _pBasicOrder->igetOrderPriceInTicks();
	}

    return iResult;
}

long HedgeOrder::igetOrderStopPrice()
{
    return _iStopPrice;
}

long HedgeOrder::igetOrderMaxStopPrice()
{
    return _iMaxStopPrice;
}

long HedgeOrder::igetOrderRemainQty()
{
	long iResult = 0;

	if(_pBasicOrder.get())
	{
		iResult = _pBasicOrder->igetOrderRemainQty();
	}

	return iResult;
}

long HedgeOrder::iCalculateMarketStop(bool bResetEntryPrice)
{
    long iNewOrderPrice = 0;

    if(!_pBasicOrder.get() || _pBasicOrder->igetOrderRemainQty() == 0 || bResetEntryPrice == true)
    {
        iNewOrderPrice = _iOrderEntryPrice;
    }
    else
    {
		iNewOrderPrice = igetOrderPrice();
    }

    if(_iOrderEntryQty > 0)
    {
        if(_pInstrument->bgetAskTradingOut())
        {
            if(_iStopPrice <= _pInstrument->igetBestAsk() && _pInstrument->igetBestAsk() <= _iMaxStopPrice)
            {
                if(_pInstrument->igetBestAsk() > iNewOrderPrice)
                {
                    iNewOrderPrice = _pInstrument->igetBestAsk();
                }
            }
        }
        else if(_iStopPrice <= _pInstrument->igetBestBid() && _pInstrument->igetBestBid() <= _iMaxStopPrice)
        {
            if(_pInstrument->igetBestBid() > iNewOrderPrice)
            {
                iNewOrderPrice = _pInstrument->igetBestBid();
            }
        }
    }
    else if(_iOrderEntryQty < 0)	
    {
        if(_pInstrument->bgetBidTradingOut())
        {
            if(_iStopPrice >= _pInstrument->igetBestBid() && _pInstrument->igetBestBid() >= _iMaxStopPrice)
            {
                if(_pInstrument->igetBestBid() < iNewOrderPrice)
                {
                    iNewOrderPrice = _pInstrument->igetBestBid();
                }
            }
        }
        else if(_iStopPrice >= _pInstrument->igetBestAsk() && _pInstrument->igetBestAsk() >= _iMaxStopPrice)
        {
            if(_pInstrument->igetBestAsk() < iNewOrderPrice)
            {
                iNewOrderPrice = _pInstrument->igetBestAsk();
            }
        }
	}

    return iNewOrderPrice;
}

void HedgeOrder::checkOrderStop()
{
    if(_pBasicOrder.get() && _pBasicOrder->igetOrderRemainQty() != 0)
    {
        long iMarketStopPrice = iCalculateMarketStop(false);

        if(iMarketStopPrice != igetOrderPrice())
        { 
            if(bchangeOrderPrice(iMarketStopPrice))
            {
                (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Hedge Order Amended with market stop price " << iMarketStopPrice << "\n";
            }
        }
    }
}

bool HedgeOrder::borderCanBeChanged()
{
    return _pBasicOrder->borderCanBeChanged();
}

}

