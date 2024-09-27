#include <stdlib.h>
#include "Liquidator.h"
#include "../EngineInfra/SchedulerBase.h"
#include "../EngineInfra/OrderRiskChecker.h"
#include "../EngineInfra/ContractAccount.h"
#include "../EngineInfra/SystemClock.h"
#include <boost/math/special_functions/round.hpp>

namespace KO
{

using namespace std;

Liquidator::Liquidator(const string& sEngineRunTimePath,
                       const string& sEngineSlotName,
                       KOEpochTime cTradingStartTime,
                       KOEpochTime cTradingEndTime,
                       SchedulerBase* pScheduler,
                       string sTodayDate,
                       PositionServerConnection* pPositionServerConnection)
:TradeEngineBase(sEngineRunTimePath, "Liquidator", sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, pPositionServerConnection)
{
}

Liquidator::~Liquidator()
{

}

double Liquidator::dgetLiquidationPrice(long iProductIndex)
{
    double dLiquidationPrice;

    long iBidAskSpread = boost::math::iround((vContractQuoteDatas[iProductIndex]->dBestAsk - vContractQuoteDatas[iProductIndex]->dBestBid) / vContractQuoteDatas[iProductIndex]->dTickSize);
    double dBidAskSizeRatio = (double)vContractQuoteDatas[iProductIndex]->iBidSize / (double)vContractQuoteDatas[iProductIndex]->iAskSize;

    if(_vPositionToLiquidate[iProductIndex] > 0)
    {
        if(_eLiquidationState == LIMIT_ALL || _eLiquidationState == LIMIT_SLOTS)
        {
            if(iBidAskSpread >= 2)
            {
                dLiquidationPrice = vContractQuoteDatas[iProductIndex]->dBestAsk - vContractQuoteDatas[iProductIndex]->dTickSize;
            }
            else if(iBidAskSpread == 1)
            {
                if(dBidAskSizeRatio < 0.25)
                {
                    dLiquidationPrice = vContractQuoteDatas[iProductIndex]->dBestBid;
                }
                else
                {
                    dLiquidationPrice = vContractQuoteDatas[iProductIndex]->dBestAsk;
                }
            }
        }
        else if(_eLiquidationState == FAST_ALL || _eLiquidationState == FAST_SLOTS)
        {
            dLiquidationPrice = vContractQuoteDatas[iProductIndex]->dBestAsk - 6 * vContractQuoteDatas[iProductIndex]->dTickSize;
        }
    }
    else if(_vPositionToLiquidate[iProductIndex] < 0)
    {
        if(_eLiquidationState == LIMIT_ALL || _eLiquidationState == LIMIT_SLOTS)
        {
            if(iBidAskSpread >= 2)
            {
                dLiquidationPrice = vContractQuoteDatas[iProductIndex]->dBestBid + vContractQuoteDatas[iProductIndex]->dTickSize;
            }
            else if(iBidAskSpread == 1)
            {
                if(dBidAskSizeRatio > 0.25)
                {
                    dLiquidationPrice = vContractQuoteDatas[iProductIndex]->dBestAsk;
                }
                else
                {
                    dLiquidationPrice = vContractQuoteDatas[iProductIndex]->dBestBid;
                }
            }
        }
        else if(_eLiquidationState == FAST_ALL || _eLiquidationState == FAST_SLOTS)
        {
            dLiquidationPrice = vContractQuoteDatas[iProductIndex]->dBestBid + 6 * vContractQuoteDatas[iProductIndex]->dTickSize;
        }
    }

    return dLiquidationPrice;
}

void Liquidator::dayInit()
{
    TradeEngineBase::dayInit();

    string sLogFileName = _sEngineRunTimePath + "Liquidator-" + _sTodayDate;

    if(_pScheduler->bisLiveTrading())
    {
        _cLogger.openFile(sLogFileName, true, true);
    }
    else
    {
        _cLogger.openFile(sLogFileName, true, false);
    }

    _cLogger << "Enter day init" << "\n";

    for(unsigned int i = 0;i < vContractQuoteDatas.size(); i++)
    {
        _vPositionToLiquidate.push_back(0);
        _vLiquidationOrders.push_back(vector<KOOrderPtr>());
        _cLogger << vContractQuoteDatas[i]->sProduct << "\n";
    }

    KOEpochTime cEngineLiveDuration = _cTradingEndTime - _cTradingStartTime;

    for(int i = 0; i < cEngineLiveDuration.sec();i++)
    {
        _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(i,0), this);
    }

    _cLogger << "Exit day init" << "\n";
}

void Liquidator::dayRun()
{
    _cLogger << "Enter day run" << "\n";

    TradeEngineBase::dayRun();

    _cLogger << "Exit day run" << "\n";
}

void Liquidator::dayStop()
{
    _cLogger << "Enter day stop" << "\n";

    TradeEngineBase::dayStop();

    _cLogger << "Exit day stop" << "\n";

    _cLogger.closeFile();
}

void Liquidator::readFromStream(istream& is)
{

}

void Liquidator::receive(int iCID)
{
    int iUpdateIndex = -1;

    for(unsigned int i = 0;i < vContractQuoteDatas.size(); i++)
    {
        if(vContractQuoteDatas[i]->iCID == iCID)
        {
            iUpdateIndex = i;
            break;
        }
    }

    if(_eLiquidationState != OFF)
    {
        if(_vLiquidationOrders[iUpdateIndex].size() != 0)
        {
            double dLiquidationPrice = dgetLiquidationPrice(iUpdateIndex);

            for(vector<KOOrderPtr>::iterator itr = _vLiquidationOrders[iUpdateIndex].begin();
                itr != _vLiquidationOrders[iUpdateIndex].end();
                itr++)
            {
                if((*itr)->igetOrderRemainQty() != 0)
                {
                    if((*itr)->borderCanBeChanged())
                    {
                        if((*itr)->igetOrderPriceInTicks() != boost::math::iround(dLiquidationPrice / vContractQuoteDatas[iUpdateIndex]->dTickSize))
                        {
                            if(vContractAccount[iUpdateIndex]->bchangeOrderPrice((*itr), dLiquidationPrice))
                            {
                                _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Submitted amend for " << vContractQuoteDatas[iUpdateIndex]->sProduct << " liquidation order. Order ID " << (*itr)->igetOrderID() << " New Price " << dLiquidationPrice << "\n";
                            }
                            else
                            {
                                _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Failed to amend price for " << vContractQuoteDatas[iUpdateIndex]->sProduct << " liquidation order. Order ID " << (*itr)->igetOrderID() << "\n";
                            }
                        }
                    }
                } 
            }
        }
    }
}

long Liquidator::igetTotalLiquidationQty(long iProductIndex)
{
    long iTotalLiquidationQty = 0;

    for(vector<KOOrderPtr>::iterator itr = _vLiquidationOrders[iProductIndex].begin();
        itr != _vLiquidationOrders[iProductIndex].end();
        itr++)
    {
        iTotalLiquidationQty = iTotalLiquidationQty + (*itr)->igetOrderRemainQty();
    }    

    return iTotalLiquidationQty;
}

void Liquidator::wakeup(KOEpochTime cCallTime)
{
    if(_eLiquidationState == OFF)
    {
        for(unsigned int i = 0; i < vContractQuoteDatas.size(); i++)
        {
            _vPositionToLiquidate[i] = 0;

            if(_vLiquidationOrders[i].size() != 0)
            {
                for(vector<KOOrderPtr>::iterator itr = _vLiquidationOrders[i].begin();
                    itr != _vLiquidationOrders[i].end();)
                {
                    if((*itr)->igetOrderRemainQty() != 0)
                    {
                        if(vContractAccount[i]->bdeleteOrder((*itr)))
                        {
                            _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Submitted delete for " << vContractQuoteDatas[i]->sProduct << " liquidation order. Order ID " << (*itr)->igetOrderID() << "\n";
                        }
                        else
                        {
                            _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Failed to delete " << vContractQuoteDatas[i]->sProduct << " liquidation order. Order ID " << (*itr)->igetOrderID() << "\n";
                        }
                        itr++;
                    }
                    else
                    {
                        itr = _vLiquidationOrders[i].erase(itr);
                    }
                }
            }
        }
    }
    else
    {
        for(unsigned int i = 0; i < vContractQuoteDatas.size(); i++)
        {
            // request position
            if(_eLiquidationState == LIMIT_SLOTS || _eLiquidationState == FAST_SLOTS)
            {
                for(vector<string>::iterator itr = _vLiquidatingSlot.begin();
                    itr != _vLiquidatingSlot.end();
                    itr++)
                {
                    _pPositionServerConnection->requestPosition(vContractAccount[i]->sgetAccountProductName(), *itr, this);
                }

                _vPositionToLiquidate[i] = 0;   

                map<string, map<string, long> >::iterator productAccoutnItr = _mProductAccountPositions.find(vContractQuoteDatas[i]->sProduct);

                if(productAccoutnItr != _mProductAccountPositions.end())
                {
                    for(map<string, long>::iterator itr = productAccoutnItr->second.begin();
                        itr != productAccoutnItr->second.end();
                        itr++)
                    {
                        _vPositionToLiquidate[i] = _vPositionToLiquidate[i] + itr->second;
                    }
                }
                _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Total slot position to liquidate for product " << vContractQuoteDatas[i]->sProduct << " is " << _vPositionToLiquidate[i] << "\n";
                _vPositionToLiquidate[i] = _vPositionToLiquidate[i] + vContractAccount[i]->igetCurrentPosition();
            
                _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Remaining Position to liquidat for product " << vContractQuoteDatas[i]->sProduct << " is " << _vPositionToLiquidate[i] << "\n";

                manageLiquidationOrders(i);
            }
            else if(_eLiquidationState == LIMIT_ALL || _eLiquidationState == FAST_ALL)
            {
_cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " request position for all \n";
                _pPositionServerConnection->requestPosition(vContractAccount[i]->sgetAccountProductName(), "ALL", this);
            }
        }
    }

    _cLogger.flush();
}

void Liquidator::manageLiquidationOrders(long iProductIndex)
{
    if(_vPositionToLiquidate[iProductIndex] != 0)
    {
        double dLiquidationPrice = dgetLiquidationPrice(iProductIndex);

        if(igetTotalLiquidationQty(iProductIndex) == 0)
        {
            long iQtyNeededToBeSubmitted = _vPositionToLiquidate[iProductIndex] * -1;
            long iLiquidationOrderClipQty = 20;

            if(iQtyNeededToBeSubmitted < 0)
            {
                iLiquidationOrderClipQty = iLiquidationOrderClipQty * -1;
            }
            
            while(iQtyNeededToBeSubmitted != 0)
            {
                long iNewOrderQty;
                if(abs(iQtyNeededToBeSubmitted) >= abs(iLiquidationOrderClipQty))
                {
                    iNewOrderQty = iLiquidationOrderClipQty;
                    iQtyNeededToBeSubmitted = iQtyNeededToBeSubmitted - iNewOrderQty;
                }
                else
                {
                    iNewOrderQty = iQtyNeededToBeSubmitted;
                    iQtyNeededToBeSubmitted = 0;
                }

                KOOrderPtr pNewLiquidationOrder = vContractAccount[iProductIndex]->psubmitOrder(iNewOrderQty, dLiquidationPrice);
                if(pNewLiquidationOrder.get())
                {
                    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Submitted new liquidation order for " << vContractQuoteDatas[iProductIndex]->sProduct << " price " << dLiquidationPrice << " qty " << iNewOrderQty << "\n";
                    _vLiquidationOrders[iProductIndex].push_back(pNewLiquidationOrder);
                }
                else
                {
                    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Failed to submit new liquidation order for " << vContractQuoteDatas[iProductIndex]->sProduct << "\n";
                }
            }
        }
        else
        {
            if(igetTotalLiquidationQty(iProductIndex) == _vPositionToLiquidate[iProductIndex] * -1)
            {
                for(vector<KOOrderPtr>::iterator itr = _vLiquidationOrders[iProductIndex].begin();
                    itr != _vLiquidationOrders[iProductIndex].end();
                    itr++)
                {
                    if((*itr)->igetOrderPriceInTicks() != boost::math::iround(dLiquidationPrice / vContractQuoteDatas[iProductIndex]->dTickSize))
                    {
                        if(vContractAccount[iProductIndex]->bchangeOrderPrice((*itr), dLiquidationPrice))
                        {
                            _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Submitted amend for " << vContractQuoteDatas[iProductIndex]->sProduct << " liquidation order. Order ID " << (*itr)->igetOrderID() << " New Price " << dLiquidationPrice << "\n";
                        }
                        else
                        {
                            _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Failed to amend price for " << vContractQuoteDatas[iProductIndex]->sProduct << " liquidation order. Order ID " << (*itr)->igetOrderID() << "\n";
                        }
                    }
                }
            }
            else
            {
                for(vector<KOOrderPtr>::iterator itr = _vLiquidationOrders[iProductIndex].begin();
                    itr != _vLiquidationOrders[iProductIndex].end();
                    itr++)
                {
                    vContractAccount[iProductIndex]->bdeleteOrder((*itr));
                }
            }
        }
    }
}

void Liquidator::onLimitLiqAllSignal()
{
    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|received limit liq all liquidation signal" << "\n";
    _eLiquidationState = LIMIT_ALL;
}

void Liquidator::onFastLiqAllSignal()
{ 
    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|received fast liquidation all signal" << "\n";
    _eLiquidationState = FAST_ALL;
}

void Liquidator::onLimitLiqSlotsSignal()
{
    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|received limit liquidation slots signal" << "\n";
    _eLiquidationState = LIMIT_SLOTS;
}

void Liquidator::onFastLiqSlotsSignal()
{ 
    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|received fast liquidation slots signal" << "\n";
    _eLiquidationState = FAST_SLOTS;
}
void Liquidator::onOffSignal()
{
    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|received off liquidation signal" << "\n";
    _eLiquidationState = OFF;
}

void Liquidator::orderConfirmHandler(int iOrderID)
{
    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Order " << iOrderID << " confirmed \n";
}

void Liquidator::orderFillHandler(int iOrderID, long iFilledQty, double dPrice)
{
    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Order " << iOrderID << " filled. qty " << iFilledQty << " price " << dPrice << "\n";
}

void Liquidator::orderRejectHandler(int iOrderID)
{
    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Order " << iOrderID << " rejected \n";
}

void Liquidator::orderDeleteHandler(int iOrderID)
{
    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Order " << iOrderID << " deleted \n";
}

void Liquidator::orderDeleteRejectHandler(int iOrderID)
{
    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Order " << iOrderID << " delete request rejected \n"; 
}

void Liquidator::orderAmendRejectHandler(int iOrderID)
{
    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Order " << iOrderID << " amend request rejected \n"; 
}

void Liquidator::liquidateSlot(string sSlot)
{
    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Adding slot " << sSlot << " for liquidation \n"; 
    _vLiquidatingSlot.push_back(sSlot);
}

void Liquidator::orderUnexpectedConfirmHandler(int iOrderID)
{
    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Order " << iOrderID << " amended and confirmed from outside the engine \n";
}

void Liquidator::orderUnexpectedDeleteHandler(int iOrderID)
{
    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Order " << iOrderID << " deleted from outside the engine.\n";
}

void Liquidator::positionRequestCallback(string sProduct, string sAccount, long iPosition, bool bTimeout)
{
    if(bTimeout == false)
    {
        for(unsigned int i = 0; i < vContractQuoteDatas.size(); i++)
        {
            if(vContractQuoteDatas[i]->sProduct.compare(sProduct) == 0)
            {
                if(sAccount.compare("ALL") == 0)
                {
                    _vPositionToLiquidate[i] = iPosition;
                    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable()
                             << "|Received global position for product "
                             << vContractQuoteDatas[i]->sProduct
                             << ": "
                             << _vPositionToLiquidate[i]
                             << "\n";

                    manageLiquidationOrders(i);
                }
                else
                {
                    _mProductAccountPositions[vContractQuoteDatas[i]->sProduct][sAccount] = iPosition;
                    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|Received new position for product " << vContractQuoteDatas[i]->sProduct << " account " << sAccount << ": " << iPosition << "\n";
                }
                
                break;
            }
        }
    }
}

}
