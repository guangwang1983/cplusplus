#include "OrderRiskChecker.h"
#include "SchedulerBase.h"
#include "ErrorHandler.h"
#include <boost/math/special_functions/round.hpp>
#include "SystemClock.h"

using std::stringstream;

namespace KO
{

boost::shared_ptr<OrderRiskChecker> OrderRiskChecker::_pInstance = boost::shared_ptr<OrderRiskChecker>();

OrderRiskChecker::OrderRiskChecker()
{

}

boost::shared_ptr<OrderRiskChecker> OrderRiskChecker::GetInstance()
{
    if(!_pInstance.get())
    {
        _pInstance.reset(new OrderRiskChecker);
    }

    return _pInstance;
}

void OrderRiskChecker::assignScheduler(SchedulerBase* pScheduler)
{
    _pScheduler = pScheduler;
}

bool OrderRiskChecker::bsubmitOrder(KOOrderPtr pOrder, double dPrice, long iQty)
{
    // only check global risk when it is live trading
    if(bcheckNewOrderRisk(pOrder, iQty))
    {
        // only check price validity when it is live trading
        if(bcheckPrice(pOrder, dPrice, iQty))
        {
            return _pScheduler->bsubmitOrder(pOrder, dPrice, iQty);
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool OrderRiskChecker::bchangeOrderPrice(KOOrderPtr pOrder, double dNewPrice)
{
    bool bResult = false;

    bResult = _pScheduler->bchangeOrderPrice(pOrder, dNewPrice);

    return bResult;
}

bool OrderRiskChecker::bchangeOrder(KOOrderPtr pOrder, double dNewPrice, long iNewQty)
{  
    bool bResult = false;
 
    // only check global risk when it is live trading
    if(bcheckAmendOrderRisk(pOrder, iNewQty))
    {
        // only check price validity when it is live trading
        if(bcheckPrice(pOrder, dNewPrice, iNewQty))
        {
            bResult =  _pScheduler->bchangeOrder(pOrder, dNewPrice, iNewQty);
        }
    }

    return bResult;
}

bool OrderRiskChecker::bdeleteOrder(KOOrderPtr pOrder)
{
    return _pScheduler->bdeleteOrder(pOrder);
}

void OrderRiskChecker::checkOrderStatus(KOOrderPtr pOrder)
{
//    _pScheduler->checkOrderStatus(pOrder);
}

bool OrderRiskChecker::bcheckPrice(KOOrderPtr pOrder, double dPrice, long iQty)
{
    bool bResult = false;

    bResult = true;

/*
    int iProductIndex = -1;

    for(unsigned int i = 0; i < _pScheduler->_vContractQuoteDatas.size(); i++)
    {
        if(_pScheduler->_vContractQuoteDatas[i]->iCID == pOrder->igetProductCID())
        {
            iProductIndex = i;
            break;
        }
    }

    double dProductTickSize = _pScheduler->_vContractQuoteDatas[iProductIndex]->dTickSize;
    double dMidPrice = (_pScheduler->_vContractQuoteDatas[iProductIndex]->dBestBid + _pScheduler->_vContractQuoteDatas[iProductIndex]->dBestAsk) / 2 ;

    long iMidPriceInTicks = boost::math::iround(dMidPrice / dProductTickSize);
    long iOrderPriceInTicks =  boost::math::iround(dPrice / dProductTickSize);
    long iOrderPriceDeviationInTicks = 10;

    if(iQty > 0)
    {
        if(iOrderPriceInTicks > iMidPriceInTicks + iOrderPriceDeviationInTicks) 
        {
            bResult = false;
        
            stringstream cStringStream;
            cStringStream << "Buy Order " << pOrder->igetOrderID() << " failed to pass order price deviation check! Order Price: " << dPrice << " Mid Price: " << dMidPrice << ".";
            ErrorHandler::GetInstance()->newWarningMsg("4.2", pOrder->sgetOrderAccount(), pOrder->sgetOrderProductName(), cStringStream.str());
        }
        else
        {
            bResult = true;
        }
    }
    else if(iQty < 0)
    {
        if(iOrderPriceInTicks < iMidPriceInTicks - iOrderPriceDeviationInTicks)
        {
            bResult = false;

            stringstream cStringStream;
            cStringStream << "Sell Order " << pOrder->igetOrderID() << " failed to pass order price deviation check! Order Price: " << dPrice << " Mid Price: " << dMidPrice << ".";
            ErrorHandler::GetInstance()->newWarningMsg("4.2", pOrder->sgetOrderAccount(), pOrder->sgetOrderProductName(), cStringStream.str());
        }
        else
        {
            bResult = true;
        }
    }
*/
    return bResult;
}

bool OrderRiskChecker::bcheckNewOrderRisk(KOOrderPtr pOrder, long iQty)
{
    bool bResult = false;

    // comment out the global portoflio risk check,  as that is no longer a requirment

/*
    vector<SchedulerBase::ProductRiskPtr>::iterator itr = _pScheduler->_vGlobalPositions.begin();
    for(;itr != _pScheduler->_vGlobalPositions.end();itr++)
    {
        if((*itr)->iProductCID == pOrder->igetProductCID())
        {
            break;
        }
    }

    if(itr != _pScheduler->_vGlobalPositions.end())
    {
        if(abs(iQty) > (*itr)->iMaxOrderSize)
        {
            bResult = false;

            stringstream cStringStream;
            cStringStream << "Order " << pOrder->igetOrderID() << " failed to pass max order check! Order Qty: " << iQty << " max order size: " << (*itr)->iMaxOrderSize << ".";
            ErrorHandler::GetInstance()->newWarningMsg("4.1", pOrder->sgetOrderAccount(), pOrder->sgetOrderProductName(), cStringStream.str());
        }
        else
        {
            if(iQty > 0)
            {
                if(iQty + _pScheduler->igetTotalOpenBuy(pOrder->igetProductCID()) + (*itr)->iPos > (*itr)->iGlobalLimit)
                {
                    bResult = false;
                
                    stringstream cStringStream;
                    cStringStream << "Buy Order " << pOrder->igetOrderID() << " failed to pass global risk check! Order Qty: " << iQty << " Global Position: " << (*itr)->iPos << " Global Open Buy Qty: " << _pScheduler->igetTotalOpenBuy(pOrder->igetProductCID()) << " Global Limit: " << (*itr)->iGlobalLimit << ".";
                    ErrorHandler::GetInstance()->newWarningMsg("5.1", pOrder->sgetOrderAccount(), pOrder->sgetOrderProductName(), cStringStream.str());
                }
                else
                {
                    bResult = true;
                }
            }
            else if(iQty < 0)
            {
                if(iQty + _pScheduler->igetTotalOpenSell(pOrder->igetProductCID()) * -1 + (*itr)->iPos < (*itr)->iGlobalLimit * -1)
                {
                    bResult = false;

                    stringstream cStringStream;
                    cStringStream << "Sell Order " << pOrder->igetOrderID() << " failed to pass global risk check! Order Qty: " << iQty << " Global Position: " << (*itr)->iPos << " Global Open Sell Qty: " << _pScheduler->igetTotalOpenSell(pOrder->igetProductCID()) << " Global Limit: " << (*itr)->iGlobalLimit << ".";
                    ErrorHandler::GetInstance()->newWarningMsg("5.1", pOrder->sgetOrderAccount(), pOrder->sgetOrderProductName(), cStringStream.str());
                }
                else
                {
                    bResult = true;
                }
            }
        }
    }
    else
    {
        if(!_pScheduler->bisLiveTrading())
        {
            bResult = true;
        }
    }
*/

    bResult = true;

    return bResult;
}

bool OrderRiskChecker::bcheckAmendOrderRisk(KOOrderPtr pOrder, long iQty)
{
    bool bResult = false;

    // comment out the global portoflio risk check,  as that is no longer a requirment
    //
/*
    vector<SchedulerBase::ProductRiskPtr>::iterator itr = _pScheduler->_vGlobalPositions.begin();
    for(;itr != _pScheduler->_vGlobalPositions.end();itr++)
    {
        if((*itr)->iProductCID == pOrder->igetProductCID())
        {
            break;
        }
    }

    if(itr != _pScheduler->_vGlobalPositions.end())
    {
        if(abs(iQty) > (*itr)->iMaxOrderSize)
        {
            bResult = false;

            stringstream cStringStream;
            cStringStream << "Order " << pOrder->igetOrderID() << " failed to pass max order check! Order Qty: " << iQty << " max order size: " << (*itr)->iMaxOrderSize << ".";
            ErrorHandler::GetInstance()->newWarningMsg("4.1", pOrder->sgetOrderAccount(), pOrder->sgetOrderProductName(), cStringStream.str());
        }
        else
        {
            if(iQty > 0)
            {
                long iQtyDelta = iQty - pOrder->igetOrderRemainQty();

                if(iQtyDelta > 0)
                {
                    // only do global risk check if there is a increase in position    
                    if(iQtyDelta + _pScheduler->igetTotalOpenBuy(pOrder->igetProductCID()) + (*itr)->iPos > (*itr)->iGlobalLimit)
                    {
                        bResult = false;
                    
                        stringstream cStringStream;
                        cStringStream << "Buy order amend " << pOrder->igetOrderID() << " failed to pass global risk check! Order Qty Delta: " << iQtyDelta << " Global Position: " << (*itr)->iPos << " Global Open Buy Qty: " << _pScheduler->igetTotalOpenBuy(pOrder->igetProductCID()) << " Global Limit: " << (*itr)->iGlobalLimit << ".";
                        ErrorHandler::GetInstance()->newWarningMsg("5.1", pOrder->sgetOrderAccount(), pOrder->sgetOrderProductName(), cStringStream.str());
                    }
                    else
                    {
                        bResult = true;
                    }
                }
                else if(iQtyDelta == 0)
                {
                    bResult = true;
                }
            }
            else if(iQty < 0)
            {
                long iQtyDelta = iQty - pOrder->igetOrderRemainQty();

                if(iQtyDelta < 0)
                {
                    if(iQtyDelta + _pScheduler->igetTotalOpenSell(pOrder->igetProductCID()) * -1 + (*itr)->iPos < (*itr)->iGlobalLimit * -1)
                    {
                        bResult = false;

                        stringstream cStringStream;
                        cStringStream << "Sell order amend " << pOrder->igetOrderID() << " failed to pass global risk check! Order Qty Delta: " << iQtyDelta << " Global Position: " << (*itr)->iPos << " Global Open Sell Qty: " << _pScheduler->igetTotalOpenSell(pOrder->igetProductCID()) << " Global Limit: " << (*itr)->iGlobalLimit << ".";
                        ErrorHandler::GetInstance()->newWarningMsg("5.1", pOrder->sgetOrderAccount(), pOrder->sgetOrderProductName(), cStringStream.str());
                    }
                    else
                    {
                        bResult = true;
                    }
                }
                else if(iQtyDelta == 0)
                {
                    bResult = true;
                }
            }
        }
    }
    else
    {
        if(!_pScheduler->bisLiveTrading())
        {
            bResult = true;
        }
    }
*/

    bResult = true;

    return bResult;
}

bool OrderRiskChecker::bcheckActionFrequency(int iTargetCID)
{
    bool bResult = false;

/*
    vector<pair<int, boost::shared_ptr<deque<KOEpochTime> > > >::iterator itr = _vOrderSubmitHistorty.begin();
    for(;itr != _vOrderSubmitHistorty.end();itr++)
    {
        if(itr->first == iTargetCID)
        {
            break;
        }
    }

    if(itr != _vOrderSubmitHistorty.end())
    {
        boost::shared_ptr<deque<KOEpochTime> > _qOrderSubmitHistorty = itr->second;

        // remove anything that is older than a second and push the new order to the back of the queue
        while(_qOrderSubmitHistorty->size() != 0 && _qOrderSubmitHistorty->front() < SystemClock::GetInstance()->cgetCurrentKOEpochTime() - KOEpochTime(1,0))
        {
            _qOrderSubmitHistorty->pop_front();
        }
   
        if(_qOrderSubmitHistorty->size() >= _pScheduler->_vGlobalPositions[iTargetCID]->iNumOrderPerSecondPerModel)
        {
            bResult = false;
        }
        else
        {
            bResult = true;
            _qOrderSubmitHistorty->push_back(SystemClock::GetInstance()->cgetCurrentKOEpochTime());
        }
    }
    else
    {
        if(!_pScheduler->bisLiveTrading())
        {
            bResult = true;
        }
    }
*/  
    bResult = true;  

    return bResult;
}

}
