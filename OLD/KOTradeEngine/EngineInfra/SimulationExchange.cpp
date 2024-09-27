#include "SimulationExchange.h"
#include "ContractAccount.h"
#include "SystemClock.h"
#include <boost/math/special_functions/round.hpp>
#include "KOScheduler.h"
#include <chrono>

using std::cerr;
using namespace std::chrono;

namespace KO
{

SimulationExchange::SimulationExchange(KOScheduler* pScheduler)
:_pKOScheduler(pScheduler)
{

}

SimulationExchange::~SimulationExchange()
{
	_vQuoteDatas.clear();

	for(vector< vector <KOOrderPtr> >::iterator itr = _cWorkingOrderMap.begin(); itr != _cWorkingOrderMap.end(); itr++)
	{
		(*itr).clear();
	}
}

void SimulationExchange::addProductForSimulation(QuoteData* pQuoteData, long iOrderLatency)
{
    _vQuoteDatas.push_back(pQuoteData);
    _vOrderLatencies.push_back(KOEpochTime(0, iOrderLatency));
    _cWorkingOrderMap.push_back(vector <KOOrderPtr>());
}

bool SimulationExchange::bsubmitOrder(KOOrderPtr pOrder, double dPrice, long iQty)
{
    KOEpochTime cLatency = _vOrderLatencies[pOrder->_iCID];

    pOrder->_iOrderPendingQty = iQty;
    pOrder->_dOrderPendingPrice = dPrice;
    pOrder->_iOrderPendingPriceInTicks = boost::math::iround(dPrice / _vQuoteDatas[pOrder->igetProductCID()]->dTickSize);

    _cWorkingOrderMap[pOrder->_iCID].push_back(pOrder);

    _pKOScheduler->addNewOrderConfirmCall(this, pOrder, SystemClock::GetInstance()->cgetCurrentKOEpochTime() + cLatency);

//    cerr << "Order " << pOrder->_iOrderID << " submiting to Simulator. Price " << dPrice << " Qty " << iQty << " submitted to the simulator\n";

	return true;
}

bool SimulationExchange::bchangeOrderPrice(KOOrderPtr pOrder, double dNewPrice)
{
    KOEpochTime cLatency = _vOrderLatencies[pOrder->_iCID];

    pOrder->_dOrderPendingPrice = dNewPrice;
    pOrder->_iOrderPendingPriceInTicks = boost::math::iround(dNewPrice / _vQuoteDatas[pOrder->igetProductCID()]->dTickSize);

    _pKOScheduler->addNewOrderConfirmCall(this, pOrder, SystemClock::GetInstance()->cgetCurrentKOEpochTime() + cLatency);

//    cerr << "Order " << pOrder->_iOrderID << " changing price to " << dNewPrice << " submitted to the simulator\n";

    return true;
}

bool SimulationExchange::bchangeOrder(KOOrderPtr pOrder, double dNewPrice, long iNewQty)
{
    KOEpochTime cLatency = _vOrderLatencies[pOrder->_iCID];

    pOrder->_iOrderPendingQty = iNewQty;
    pOrder->_dOrderPendingPrice = dNewPrice;
    pOrder->_iOrderPendingPriceInTicks = boost::math::iround(dNewPrice / _vQuoteDatas[pOrder->igetProductCID()]->dTickSize);

    _pKOScheduler->addNewOrderConfirmCall(this, pOrder, SystemClock::GetInstance()->cgetCurrentKOEpochTime() + cLatency);

//    cerr << "Order " << pOrder->_iOrderID << " changing price to " << dNewPrice << " submitted to the simulator \n";

    return true;
}

bool SimulationExchange::bdeleteOrder(KOOrderPtr pOrder)
{
    KOEpochTime cLatency = _vOrderLatencies[pOrder->_iCID];

    _pKOScheduler->addNewOrderConfirmCall(this, pOrder, SystemClock::GetInstance()->cgetCurrentKOEpochTime() + cLatency);

//    cerr << "Order " << pOrder->_iOrderID << " delete submitted to the simulator\n";

    return true;
}

void SimulationExchange::orderConfirmCallBack(KOOrderPtr pOrder)
{
//microseconds in = duration_cast< microseconds >(system_clock::now().time_since_epoch());
    if(pOrder->_eOrderState == KOOrder::PENDINGCREATION)
    {
        pOrder->_iOrderConfirmedQty = pOrder->_iOrderPendingQty;
        pOrder->_dOrderConfirmedPrice = pOrder->_dOrderPendingPrice;
        pOrder->_iOrderConfirmedPriceInTicks = pOrder->_iOrderPendingPriceInTicks;

//        cerr << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Order " << pOrder->_iOrderID << " entry confirmed in simulator new price " << pOrder->dgetOrderPrice() << " new qty " << pOrder->igetOrderRemainQty() << "\n";

        if(bapplyOrderToMarketPrice(pOrder))
        {
            for(vector<KOOrderPtr>::iterator itr = _cWorkingOrderMap[pOrder->igetProductCID()].begin(); itr != _cWorkingOrderMap[pOrder->igetProductCID()].end();)
            {
                if((*itr)->igetOrderID() == pOrder->igetOrderID())
                {
                    _cWorkingOrderMap[pOrder->igetProductCID()].erase(itr);
                }
                else
                {
                    itr++;
                }
            }
        }
        else
        {
            _pKOScheduler->onConfirm(pOrder);
            resetQueue(pOrder);
        }
    }
    else if(pOrder->_eOrderState == KOOrder::PENDINGCHANGE)
    {
        pOrder->_iOrderConfirmedQty = pOrder->_iOrderPendingQty;
        pOrder->_dOrderConfirmedPrice = pOrder->_dOrderPendingPrice;
        pOrder->_iOrderConfirmedPriceInTicks = pOrder->_iOrderPendingPriceInTicks;

//        cerr << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Order " << pOrder->_iOrderID << " amend confirmed in simulator new price " << pOrder->dgetOrderPrice() << " new qty " << pOrder->igetOrderRemainQty() << "\n";

        if(bapplyOrderToMarketPrice(pOrder))
        {
            for(vector<KOOrderPtr>::iterator itr = _cWorkingOrderMap[pOrder->igetProductCID()].begin(); itr != _cWorkingOrderMap[pOrder->igetProductCID()].end();)
            {
                if((*itr)->igetOrderID() == pOrder->igetOrderID())
                {
                    _cWorkingOrderMap[pOrder->igetProductCID()].erase(itr);
                }
                else
                {
                    itr++;
                }
            }
        }
        else
        {
            _pKOScheduler->onConfirm(pOrder);
            resetQueue(pOrder);
        }
    }
    else if(pOrder->_eOrderState == KOOrder::PENDINGDELETE)
    {
        _pKOScheduler->onDelete(pOrder);
//        cerr << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Order " << pOrder->_iOrderID << " delete confirmed in simulator " << std::endl;

        for(vector<KOOrderPtr>::iterator itr = _cWorkingOrderMap[pOrder->igetProductCID()].begin(); itr != _cWorkingOrderMap[pOrder->igetProductCID()].end();)
        {
            if((*itr)->igetOrderID() == pOrder->igetOrderID())
            {
                _cWorkingOrderMap[pOrder->igetProductCID()].erase(itr);
            }
            else
            {
                itr++;
            }
        }

    }

//microseconds out = duration_cast< microseconds >(system_clock::now().time_since_epoch());

//cerr << "orderConfirmCallBack " << (out - in).count() << "\n";
}

void SimulationExchange::newPriceUpdate(long iProductIndex)
{
//microseconds in = duration_cast< microseconds >(system_clock::now().time_since_epoch());

    for(vector<KOOrderPtr>::iterator itr = _cWorkingOrderMap[iProductIndex].begin(); itr != _cWorkingOrderMap[iProductIndex].end();)
    {
        if(bapplyPriceUpdateToOrder(*itr))
        {
            _cWorkingOrderMap[iProductIndex].erase(itr);			
        }
        else
        {
            itr++;
        }
    }
//microseconds out = duration_cast< microseconds >(system_clock::now().time_since_epoch());

//cerr << "newPriceUpdate " << (out - in).count() << "\n";
}

bool SimulationExchange::bapplyOrderToMarketPrice(KOOrderPtr pOrder)
{
    bool bResult = false;

    long iProductIndex = pOrder->igetProductCID();

    // Check price validity
    if(_vQuoteDatas[iProductIndex]->iBestBidInTicks < _vQuoteDatas[iProductIndex]->iBestAskInTicks && _vQuoteDatas[iProductIndex]->iBidSize != 0 && _vQuoteDatas[iProductIndex]->iAskSize != 0)
    {
        if(pOrder->_eOrderState != KOOrder::INACTIVE)
        {
/*
            cerr << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << _vQuoteDatas[iProductIndex]->cControlUpdateTime.igetPrintable() << "|applyOrderToMarketPrice|";
            cerr << _vQuoteDatas[iProductIndex]->iBidSize << "|" << _vQuoteDatas[iProductIndex]->iBestBidInTicks << "|" << _vQuoteDatas[iProductIndex]->iBestAskInTicks << "|" << _vQuoteDatas[iProductIndex]->iAskSize << "|"
                 << _vQuoteDatas[iProductIndex]->iLastTradeInTicks << "|" << _vQuoteDatas[iProductIndex]->iTradeSize << "|" << _vQuoteDatas[iProductIndex]->iAccumuTradeSize << "|" << _vQuoteDatas[iProductIndex]->iLastTradeInTicks << "\n";
*/
            if(pOrder->igetOrderRemainQty() > 0)
            {
                if(pOrder->_iOrderConfirmedPriceInTicks >= _vQuoteDatas[iProductIndex]->iBestAskInTicks)
                {
                    bResult = bfillOrder(pOrder, 0, true, true);
                }
            }
            else if(pOrder->igetOrderRemainQty() < 0)
            {
                if(pOrder->_iOrderConfirmedPriceInTicks <= _vQuoteDatas[iProductIndex]->iBestBidInTicks)
                {
                    bResult = bfillOrder(pOrder, 0, true, true);
                }
            }
        }
    }

    return bResult;
}

bool SimulationExchange::bapplyPriceUpdateToOrder(KOOrderPtr pOrder)
{
    bool bOrderDeleted = false;

    long iProductIndex = pOrder->igetProductCID();

    // Check price validity
//    if(_vQuoteDatas[iProductIndex]->iBestBidInTicks < _vQuoteDatas[iProductIndex]->iBestAskInTicks && _vQuoteDatas[iProductIndex]->iBidSize != 0 && _vQuoteDatas[iProductIndex]->iAskSize != 0)
//    {
//        if(pOrder->_eOrderState == KOOrder::ACTIVE || pOrder->_eOrderState == KOOrder::PENDINGCHANGE)
//        {
//			cerr << to_simple_string(_vQuoteDatas[iProductIndex]->cControlUpdateTime) << "|applyPriceUpdateToOrder|";
//			cerr << _vQuoteDatas[iProductIndex]->iBidSize << "|" << _vQuoteDatas[iProductIndex]->iBestBidInTicks << "|" << _vQuoteDatas[iProductIndex]->iBestAskInTicks << "|" << _vQuoteDatas[iProductIndex]->iAskSize << "|"
//				 << _vQuoteDatas[iProductIndex]->iLastTradeInTicks << "|" << _vQuoteDatas[iProductIndex]->iTradeSize << "|" << _vQuoteDatas[iProductIndex]->iAccuVolume << "|" << _vQuoteDatas[iProductIndex]->iLastTradeInTicks << "\n";

//            long iPriceTraded = _vQuoteDatas[iProductIndex]->iLastTradeInTicks;				
//            long iSizeTraded = _vQuoteDatas[iProductIndex]->iTradeSize;

            if(pOrder->igetOrderRemainQty() > 0)
            {
                // check bid order

                // check price move between grid points
                if(_vQuoteDatas[iProductIndex]->iLowAskInTicks <= pOrder->_iOrderConfirmedPriceInTicks)
                {
                    bOrderDeleted = bfillOrder(pOrder, 0, true, false);
                }

/*
                if(iSizeTraded != 0)
                {
                    if(pOrder->_iOrderConfirmedPriceInTicks > iPriceTraded)
                    {
                        bOrderDeleted = bfillOrder(pOrder, iSizeTraded, true, false);
                    }
                    else if(pOrder->_iOrderConfirmedPriceInTicks == iPriceTraded)
                    {
                        if(pOrder->_iOrderConfirmedPriceInTicks > _vQuoteDatas[iProductIndex]->iBestBidInTicks)
                        {
                            bOrderDeleted = bfillOrder(pOrder, iSizeTraded, true, false);
                        }
                        else
                        {
                            bOrderDeleted = bfillOrder(pOrder, iSizeTraded, false, false);
                        }
                    }
                }
                else
                {
                    if(pOrder->_iOrderConfirmedPriceInTicks >= _vQuoteDatas[iProductIndex]->iBestAskInTicks)
                    {
                        bOrderDeleted = bfillOrder(pOrder, iSizeTraded, true, false);
                    }
                }
*/
            }
            else if(pOrder->igetOrderRemainQty() < 0)
            {
                // check offer order

                // check price move between grid points
                if(_vQuoteDatas[iProductIndex]->iHighBidInTicks >= pOrder->_iOrderConfirmedPriceInTicks)
                {
                    bOrderDeleted = bfillOrder(pOrder, 0, true, false);
                }
/*
                if(iSizeTraded != 0)
                {
                    if(pOrder->_iOrderConfirmedPriceInTicks < iPriceTraded)
                    {
                        bOrderDeleted = bfillOrder(pOrder, iSizeTraded, true, false);
                    }
                    else if(pOrder->_iOrderConfirmedPriceInTicks == iPriceTraded)
                    {
                        if(pOrder->_iOrderConfirmedPriceInTicks < _vQuoteDatas[iProductIndex]->iBestAskInTicks)
                        {
                            bOrderDeleted = bfillOrder(pOrder, iSizeTraded, true, false);
                        }
                        else
                        {
                            bOrderDeleted = bfillOrder(pOrder, iSizeTraded, false, false);
                        }
                    }
                }
                else
                {
                    if(pOrder->_iOrderConfirmedPriceInTicks <= _vQuoteDatas[iProductIndex]->iBestBidInTicks)
                    {
                        bOrderDeleted = bfillOrder(pOrder, iSizeTraded, true, false);
                    }
                }
*/
//            }

//            if(!bOrderDeleted)
//            {
//                updateQueue(pOrder);
//            }
        }
//    }

    return bOrderDeleted;
}

bool SimulationExchange::bfillOrder(KOOrderPtr pWorkingOrder, long iSizeTraded, bool bOrderCrossed, bool bAggressOrderAction)
{
    long iProductIndex = pWorkingOrder->igetProductCID();

	// Filled order when it is either crossed with market or front of the queue
	// Returns true if order is fully filled
	bool bFullyFilled = false;
	
	double dFillPrice = 0; 
   	long iFillPriceInTicks = 0;   
	long iFilledQty = 0; 

//cerr << "iSizeTraded " << iSizeTraded << " bOrderCrossed " << bOrderCrossed << "\n"; 

	if(bOrderCrossed)
	{
		if(pWorkingOrder->_iOrderConfirmedQty > 0)
		{
			if(bAggressOrderAction)
			{
//cerr << "Aggressive Order crossed Assign best ask to fill price " << _vQuoteDatas[iProductIndex]->dBestAsk << "\n";
				dFillPrice = _vQuoteDatas[iProductIndex]->dBestAsk;
				iFillPriceInTicks = _vQuoteDatas[iProductIndex]->iBestAskInTicks;
			}
			else
			{
//cerr << "Passive Order crossed Assign _dOrderPrice to fill price " << pWorkingOrder->_dOrderPrice << "\n";
				dFillPrice = pWorkingOrder->_dOrderConfirmedPrice;
				iFillPriceInTicks = pWorkingOrder->_iOrderConfirmedPriceInTicks;
			}
		}
		else if(pWorkingOrder->_iOrderConfirmedQty < 0)
		{
			if(bAggressOrderAction)
			{
				dFillPrice = _vQuoteDatas[iProductIndex]->dBestBid;
				iFillPriceInTicks = _vQuoteDatas[iProductIndex]->iBestBidInTicks;
			}
			else
			{
				dFillPrice = pWorkingOrder->_dOrderConfirmedPrice;
				iFillPriceInTicks = pWorkingOrder->_iOrderConfirmedPriceInTicks;
			}
		}

		iFilledQty = pWorkingOrder->_iOrderConfirmedQty;
        pWorkingOrder->_iOrderPendingQty = 0;
        pWorkingOrder->_iOrderConfirmedQty = 0;
        pWorkingOrder->_iOrderRemainingQty = 0;
		bFullyFilled = true;
		//cerr << "Order crossed. Fully filled \n";
	}
	else
	{
		long iSizeAllocatedToOrder = 0; 

//cerr << "Order not crossed Assign last trade to fill price " << pWorkingOrder->_dOrderPrice << "\n";
		dFillPrice = pWorkingOrder->_dOrderConfirmedPrice;
		iFillPriceInTicks = pWorkingOrder->_iOrderConfirmedPriceInTicks;

//cerr << "pWorkingOrder->_iQueuePosition is " << pWorkingOrder->_iQueuePosition << "\n";
        if(iSizeTraded >= pWorkingOrder->_iQueuePosition)
        {
//cerr << "pWorkingOrder->_iRemaining " << pWorkingOrder->_iRemaining << "\n";
            if(iSizeTraded - pWorkingOrder->_iQueuePosition > abs(pWorkingOrder->_iOrderConfirmedQty))
            {
                iSizeAllocatedToOrder = abs(pWorkingOrder->_iOrderConfirmedQty);
            }
            else
            {
                iSizeAllocatedToOrder = iSizeTraded - pWorkingOrder->_iQueuePosition;
            }
        }

        if(iSizeAllocatedToOrder != 0)
        {
//		    cerr << "FIFO allocates " << iSizeAllocatedToOrder << " lot \n";
        }

		if(pWorkingOrder->_iOrderConfirmedQty > 0)
		{
			iFilledQty = iSizeAllocatedToOrder;
		}
		else if(pWorkingOrder->_iOrderConfirmedQty < 0)
		{
			iFilledQty = -1 * iSizeAllocatedToOrder;
		}	

		pWorkingOrder->_iOrderConfirmedQty = pWorkingOrder->_iOrderConfirmedQty - iFilledQty;

		if(pWorkingOrder->_iOrderConfirmedQty == 0)
		{
			bFullyFilled = true;
		}
	}

	if(iFilledQty != 0)
	{
//		cerr << "Order " << pWorkingOrder->_iOrderID << " filled. Qty " << iFilledQty << " Remaining " << pWorkingOrder->_iOrderConfirmedQty << " dFillPrice " << dFillPrice << " iFillPriceInTicks " << iFillPriceInTicks << "\n";
		//cerr << "Order " << pWorkingOrder->_iOrderID << " filled. Qty " << iFilledQty << " dFillPrice " << dFillPrice << " iFillPriceInTicks " << iFillPriceInTicks << "\n";
		_pKOScheduler->onFill(pWorkingOrder, dFillPrice, iFilledQty);
	}

	if(bFullyFilled)
	{
//		cerr << "Order " << pWorkingOrder->_iOrderID << " deleted from Simulator \n";
	}

	return bFullyFilled;
}

void SimulationExchange::resetQueue(KOOrderPtr pWorkingOrder)
{
/*
    long iProductIndex = pWorkingOrder->igetProductCID();

    // check price validity
    if(_vQuoteDatas[iProductIndex]->iBestBidInTicks < _vQuoteDatas[iProductIndex]->iBestAskInTicks && _vQuoteDatas[iProductIndex]->iBidSize != 0 && _vQuoteDatas[iProductIndex]->iAskSize != 0)
    {
        if(pWorkingOrder->_iOrderConfirmedQty > 0)
        {
            if(pWorkingOrder->_iOrderConfirmedPriceInTicks > _vQuoteDatas[iProductIndex]->iBestBidInTicks)
            {
                pWorkingOrder->_iQueuePosition = 0;
            }
            else if(pWorkingOrder->_iOrderConfirmedPriceInTicks == _vQuoteDatas[iProductIndex]->iBestBidInTicks)
            {
                pWorkingOrder->_iQueuePosition = _vQuoteDatas[iProductIndex]->iBidSize;
            }
            else if(pWorkingOrder->_iOrderConfirmedPriceInTicks < _vQuoteDatas[iProductIndex]->iBestBidInTicks) 
            {
                // Set the queue position to something very big, becaseu we dont use book in the simulator
                pWorkingOrder->_iQueuePosition = 500000;
            }
        }
        else if(pWorkingOrder->_iOrderConfirmedQty < 0)
        {
            if(pWorkingOrder->_iOrderConfirmedPriceInTicks < _vQuoteDatas[iProductIndex]->iBestAskInTicks)
            {
                pWorkingOrder->_iQueuePosition = 0;
            }
            else if(pWorkingOrder->_iOrderConfirmedPriceInTicks == _vQuoteDatas[iProductIndex]->iBestAskInTicks)
            {
                pWorkingOrder->_iQueuePosition = _vQuoteDatas[iProductIndex]->iAskSize;
            }
            else if(pWorkingOrder->_iOrderConfirmedPriceInTicks > _vQuoteDatas[iProductIndex]->iBestAskInTicks) 
            {
                // Set the queue position to something very big, becaseu we dont use book in the simulator
                pWorkingOrder->_iQueuePosition = 500000;
            }
        }
    }
    else
    {
        pWorkingOrder->_iQueuePosition = 500000;
    }

//cerr << to_simple_string(SystemTimeMonitor::GetInstance()->cgetCurrentPTime()) << " Order " << pWorkingOrder->_iOrderID << "|" << pWorkingOrder->_iQueuePosition << "|" << pWorkingOrder->igetOrderPriceInTicks() << "|" << pWorkingOrder->_iRemaining << "\n";
*/
}

void SimulationExchange::updateQueue(KOOrderPtr pWorkingOrder)
{
/*
    long iProductIndex = pWorkingOrder->igetProductCID();

	if(pWorkingOrder->_iOrderConfirmedQty > 0)
	{
		if(pWorkingOrder->_iOrderConfirmedPriceInTicks > _vQuoteDatas[iProductIndex]->iBestBidInTicks)
		{
			pWorkingOrder->_iQueuePosition = 0;
		}
		else if(pWorkingOrder->_iOrderConfirmedPriceInTicks == _vQuoteDatas[iProductIndex]->iBestBidInTicks)
		{
			if(_vQuoteDatas[iProductIndex]->iBestBidInTicks == _vQuoteDatas[iProductIndex]->iLastTradeInTicks && _vQuoteDatas[iProductIndex]->iTradeSize != 0)
			{
				pWorkingOrder->_iQueuePosition = pWorkingOrder->_iQueuePosition - _vQuoteDatas[iProductIndex]->iTradeSize;
			}
			else
			{
				if(_vQuoteDatas[iProductIndex]->iBidSize < pWorkingOrder->_iQueuePosition)
				{
					pWorkingOrder->_iQueuePosition = _vQuoteDatas[iProductIndex]->iBidSize;
				}
			}
		}
	}
	else if(pWorkingOrder->_iOrderConfirmedQty < 0)
	{
		if(pWorkingOrder->_iOrderConfirmedPriceInTicks < _vQuoteDatas[iProductIndex]->iBestAskInTicks)
		{
			pWorkingOrder->_iQueuePosition = 0;
		}
		else if(pWorkingOrder->_iOrderConfirmedPriceInTicks == _vQuoteDatas[iProductIndex]->iBestAskInTicks)
		{
			if(_vQuoteDatas[iProductIndex]->iBestAskInTicks == _vQuoteDatas[iProductIndex]->iLastTradeInTicks && _vQuoteDatas[iProductIndex]->iTradeSize != 0)
			{
				pWorkingOrder->_iQueuePosition = pWorkingOrder->_iQueuePosition - _vQuoteDatas[iProductIndex]->iTradeSize;
			}
			else
			{
				if(_vQuoteDatas[iProductIndex]->iAskSize < pWorkingOrder->_iQueuePosition)
				{
					pWorkingOrder->_iQueuePosition = _vQuoteDatas[iProductIndex]->iAskSize;
				}
			}
		}
	}

	if(pWorkingOrder->_iQueuePosition < 0)
	{
		pWorkingOrder->_iQueuePosition = 0;
	}
	
	//cerr << "Order " << pWorkingOrder->_iOrderID << "|" << pWorkingOrder->_iQueuePosition << "|" << pWorkingOrder->igetOrderPriceInTicks() << "|" << pWorkingOrder->_iRemaining << "\n";
*/
}

}
