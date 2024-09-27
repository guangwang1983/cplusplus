#include "ContractAccount.h"
#include <iostream>
#include <boost/math/special_functions/round.hpp>
#include "SystemClock.h"
#include "ErrorHandler.h"
#include "SimPositionServer.h"
#include "SchedulerBase.h"

using std::stringstream;

namespace KO
{

ContractAccount::ContractAccount()
{

}

ContractAccount::ContractAccount(SchedulerBase* pScheduler,
                                 TradeEngineBase* pParent, 
                                 QuoteData* pQuoteData,
                                 const string& sEngineSlotName, 
                                 const string& sProduct,
                                 const string& sExchange, 
                                 double dTickSize, 
                                 long iMaxOrderSize,
                                 long iLimit,
                                 double dPnlLimit,
                                 const string& sTradingAccount,
                                 bool bIsLiveTrading)
:_pParent(pParent),
 _pQuoteData(pQuoteData),
 _sEngineSlotName(sEngineSlotName),
 _sProduct(sProduct),
 _sExchange(sExchange),
 _dTickSize(dTickSize),
 _iMaxOrderSize(iMaxOrderSize),
 _iLimit(iLimit),
 _dPnlLimit(dPnlLimit),
 _sTradingAccount(sTradingAccount),
 _bPositionInitialised(false),
 _iPosition(0),
 _iNumOrderActions(0),
 _dRealisedPnL(0.0),
 _dLastMtMPnL(0.0),
 _iTradedVolume(0),
 _iNumOfCriticalErrors(0),
 _eAccountState(Trading),
 _bIsLiveTrading(bIsLiveTrading),
 _pScheduler(pScheduler)
{
    _iNumSubmitPerMinutePerModel = 75;
    _iNumMessagePerSecondPerModel = 5;

    _bExcessiveOrderHaltTriggered = false;
}
   
ContractAccount::~ContractAccount()
{

}

const string& ContractAccount::sgetAccountProductName()
{
    return _sProduct;
}

const string& ContractAccount::sgetAccountExchangeName()
{
    return _sExchange;
}

const string& ContractAccount::sgetRootSymbol()
{
    return _pQuoteData->sRoot;
}

double ContractAccount::dgetTickSize()
{
    return _dTickSize;
}

const string& ContractAccount::sgetAccountName()
{
    return _sTradingAccount;
}

int ContractAccount::igetCID()
{
    return _pQuoteData->iCID;
}

KOOrderPtr ContractAccount::psubmitOrder(long iQty, double dPrice, bool bIsIOCOrder)
{
    KOOrderPtr pNewOrder;

    if(_eAccountState == Trading)
    {
        if(bcheckRisk(iQty))
        {
            if(bcheckMessageFrequency())
            {
                if(bcheckSubmitFrequency())
                {
                    pNewOrder.reset(new KOOrder(igetNextOrderID(), _sEngineSlotName, _pQuoteData->iCID, _pQuoteData->iGatewayID, _pQuoteData->iSecurityID, _pQuoteData->dTickSize, bIsIOCOrder, _sProduct, _sTradingAccount, _sExchange, _pQuoteData->eInstrumentType, this));

                    if(_pScheduler->bsubmitOrder(pNewOrder, dPrice, iQty))
                    {
                        _vLiveOrders.push_back(pNewOrder);
                        _iNumOrderActions = _iNumOrderActions + 1;
                    }
                    else
                    {
                        pNewOrder.reset();
                        _iNumOfCriticalErrors = _iNumOfCriticalErrors + 1;
                        stringstream cStringStream;
                        cStringStream << "Cannot submit new order. Number of internal order critical error is " << _iNumOfCriticalErrors << "\n";
                        ErrorHandler::GetInstance()->newWarningMsg("0", _sEngineSlotName, _sProduct, cStringStream.str());
                        checkCriticalErrors();
                    }
                }
            }
        }
    }

    return pNewOrder;
}

KOOrderPtr ContractAccount::psubmitOrderInTicks(long iQty, long iPriceInTicks, bool bIsIOCOrder)
{
    double dPrice = (double)iPriceInTicks * _dTickSize;
    return psubmitOrder(iQty, dPrice, bIsIOCOrder);
}

bool ContractAccount::bchangeOrderPrice(KOOrderPtr pOrder, double dNewPrice)
{
    bool bResult = false;

    if(_eAccountState == Trading)
    {
        if(pOrder.get())
        {
            if(pOrder->borderCanBeChanged())
            {
                if(bcheckMessageFrequency())
                {
                    if(_pScheduler->bchangeOrderPrice(pOrder, dNewPrice) == true)
                    {
                        _iNumOrderActions = _iNumOrderActions + 1;
                        bResult = true;
                    }
                    else
                    {
                        _iNumOfCriticalErrors = _iNumOfCriticalErrors + 1;
                        stringstream cStringStream;
                        cStringStream << "Cannot change order price. Number of internal order critical errors is " << _iNumOfCriticalErrors << "\n";
                        ErrorHandler::GetInstance()->newWarningMsg("0", _sEngineSlotName, _sProduct, cStringStream.str());
                        checkCriticalErrors();
                    }
                }
            }
        }
    }

    return bResult;
}

bool ContractAccount::bchangeOrderPriceInTicks(KOOrderPtr pOrder, long iNewPriceInTicks)
{
    double dPrice = (double)iNewPriceInTicks * _dTickSize;
    return bchangeOrderPrice(pOrder, dPrice);
}

bool ContractAccount::bchangeOrder(KOOrderPtr pOrder, double dNewPrice, long iNewOrderSize)
{
    bool bResult = false;

    if(_eAccountState == Trading)
    {
        if(pOrder.get())
        {
            if(bcheckMessageFrequency())
            {
                if(pOrder->borderCanBeChanged())
                {
                    if(_pScheduler->bchangeOrder(pOrder, dNewPrice, iNewOrderSize) == true)
                    {
                        _iNumOrderActions = _iNumOrderActions + 1;
                        bResult = true;
                    }
                    else
                    {
                        _iNumOfCriticalErrors = _iNumOfCriticalErrors + 1;
                        stringstream cStringStream;
                        cStringStream << "Cannot change order price and size. Number of internal order critical error is " << _iNumOfCriticalErrors << "\n";
                        ErrorHandler::GetInstance()->newWarningMsg("0", _sEngineSlotName, _sProduct, cStringStream.str());
                        checkCriticalErrors();
                    }
                }
            }
        }
    }

    return bResult;
}

bool ContractAccount::bchangeOrderInTicks(KOOrderPtr pOrder, long iNewPriceInTicks, long iNewOrderSize)
{
    double dPrice = (double)iNewPriceInTicks * _dTickSize;
    return bchangeOrder(pOrder, dPrice, iNewOrderSize);
}

bool ContractAccount::bcheckMessageFrequency()
{
    bool bResult = false;

    // remove anything that is older than a second and push the new order to the back of the queue
    while(_qOrderMessageHistorty.size() != 0 && _qOrderMessageHistorty.front() <= SystemClock::GetInstance()->cgetCurrentKOEpochTime() - KOEpochTime(1,0))
    {
        _qOrderMessageHistorty.pop_front();
    }

    if(_qOrderMessageHistorty.size() >= _iNumMessagePerSecondPerModel)
    {
        bResult = false;
    }
    else
    {
        bResult = true;
        _qOrderMessageHistorty.push_back(SystemClock::GetInstance()->cgetCurrentKOEpochTime());
    }

    return bResult;
}

bool ContractAccount::bcheckSubmitFrequency()
{
    bool bResult = true;

    // remove anything that is older than a second and push the new order to the back of the queue
    while(_qOrderSubmitMinuteHistorty.size() != 0 && _qOrderSubmitMinuteHistorty.front() <= SystemClock::GetInstance()->cgetCurrentKOEpochTime() - KOEpochTime(60,0))
    {
        _qOrderSubmitMinuteHistorty.pop_front();
    }

    if(_qOrderSubmitMinuteHistorty.size() >= _iNumSubmitPerMinutePerModel)
    {
        bResult = bResult && false;

        if(_bExcessiveOrderHaltTriggered == false)
        {
            _bExcessiveOrderHaltTriggered = true;

            _eAccountState = Halt;
            _pParent->manualHaltTrading();

            stringstream cStringStream;
            cStringStream << "Config tried to submit more than " << _iNumSubmitPerMinutePerModel << ". " << _sEngineSlotName << " in halt mode.";
            ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, _sProduct, cStringStream.str());
        }
    }
    else
    {
        bResult = bResult && true;
        _qOrderSubmitMinuteHistorty.push_back(SystemClock::GetInstance()->cgetCurrentKOEpochTime());
    }

    return bResult;
}

bool ContractAccount::bdeleteOrder(KOOrderPtr pOrder)
{
    bool bResult = false;

    if(pOrder.get())
    {
        if(bcheckMessageFrequency())
        {
            if(pOrder->borderCanBeChanged())
            {
                bResult = _pScheduler->bdeleteOrder(pOrder);
            }
        }
    }

    return bResult;
}

bool ContractAccount::bcheckRisk(long iNewOrderQty)
{
    bool bResult = false;

    long iTotalBuyOrderQty = 0;
    long iTotalSellOrderQty = 0;

    for(vector<KOOrderPtr>::iterator itr = _vLiveOrders.begin(); itr != _vLiveOrders.end(); ++itr)
    {
        if((*itr)->igetOrderRemainQty() > 0)
        {
            iTotalBuyOrderQty = iTotalBuyOrderQty + (*itr)->igetOrderRemainQty();
        }
        else if((*itr)->igetOrderRemainQty() < 0)
        {
            iTotalSellOrderQty = iTotalSellOrderQty + (*itr)->igetOrderRemainQty();
        }
    }

    for(vector<KOOrderPtr>::iterator itr = _vDeletedOrders.begin(); itr != _vDeletedOrders.end(); ++itr)
    {
        if((*itr)->igetOrderRemainQty() > 0)
        {
            iTotalBuyOrderQty = iTotalBuyOrderQty + (*itr)->igetOrderRemainQty();
        }
        else if((*itr)->igetOrderRemainQty() < 0)
        {
            iTotalSellOrderQty = iTotalSellOrderQty + (*itr)->igetOrderRemainQty();
        }
    }

    if(iNewOrderQty > 0)
    {
        if(iNewOrderQty + iTotalBuyOrderQty + _iPosition > _iLimit)
        {
            bResult = false;
            _iNumOfCriticalErrors = _iNumOfCriticalErrors + 1;
            stringstream cStringStream;
            cStringStream << "New buy order failed to pass risk limit check! Contract: " << _sProduct
                          << " iNewOrderQty: " << iNewOrderQty << " iTotalBuyOrderQty: "
                          << iTotalBuyOrderQty << " _iPosition: " << _iPosition << " _iLimit " << _iLimit << ".";
            ErrorHandler::GetInstance()->newErrorMsg("4.3", _sTradingAccount, _sProduct, cStringStream.str());
            checkCriticalErrors();
        }
        else
        {
            bResult = true;
        }
    }
    else if(iNewOrderQty < 0)
    {
        if(iNewOrderQty + iTotalSellOrderQty + _iPosition < _iLimit * -1)
        {
            bResult = false;
            _iNumOfCriticalErrors = _iNumOfCriticalErrors + 1;
            stringstream cStringStream;
            cStringStream << "New sell order failed to pass risk limit check! Contract:" << _sProduct
                          << " iNewOrderQty: " << iNewOrderQty << " iTotalSellOrderQty: "
                          << iTotalSellOrderQty << " _iPosition: " << _iPosition << " _iLimit " << _iLimit << ".";
            ErrorHandler::GetInstance()->newErrorMsg("4.3", _sTradingAccount, _sProduct, cStringStream.str());
            checkCriticalErrors();
        }
        else
        {
            bResult = true;
        }
    }

    return bResult;
}

void ContractAccount::checkOrderStatus()
{
    for(vector<KOOrderPtr>::iterator itr = _vLiveOrders.begin(); itr != _vLiveOrders.end(); itr++)
    {
        _pScheduler->checkOrderStatus(*itr);
    }
}

void ContractAccount::orderConfirmHandler(int iOrderID)
{
    _pParent->orderConfirmHandler(iOrderID);
}

void ContractAccount::orderUnexpectedConfirmHandler(int iOrderID)
{
    _pParent->orderUnexpectedConfirmHandler(iOrderID);
}

void ContractAccount::orderFillHandler(int iOrderID, long iFilledQty, double dPrice)
{
    _iPosition = _iPosition + iFilledQty;

    boost::shared_ptr<Trade> pNewTrade (new Trade);
    pNewTrade->cTradeTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
    pNewTrade->sProduct = _sProduct;
    pNewTrade->iQty = iFilledQty;
    pNewTrade->dPrice = dPrice;

    _dRealisedPnL = _dRealisedPnL + iFilledQty * dPrice * -1;
    _iTradedVolume = _iTradedVolume + abs(iFilledQty);

    _vTrades.push_back(pNewTrade);

    _pParent->orderFillHandler(iOrderID, iFilledQty, dPrice);
}

void ContractAccount::orderRejectHandler(int iOrderID)
{
    for(vector<KOOrderPtr>::iterator itr = _vLiveOrders.begin(); itr != _vLiveOrders.end(); )
    {
        if((*itr)->igetOrderID() == iOrderID)
        {
            _vDeletedOrders.push_back((*itr));
            _vLiveOrders.erase(itr);
        }
        else
        {
            ++itr;
        }
    }

    _pParent->orderRejectHandler(iOrderID);
}

void ContractAccount::orderDeleteHandler(int iOrderID)
{
    for(vector<KOOrderPtr>::iterator itr = _vLiveOrders.begin(); itr != _vLiveOrders.end(); )
    {
        if((*itr)->igetOrderID() == iOrderID)
        {
            _vDeletedOrders.push_back((*itr));
            _vLiveOrders.erase(itr);
        }
        else
        {
            ++itr;
        }
    }

    _pParent->orderDeleteHandler(iOrderID);
}

void ContractAccount::orderUnexpectedDeleteHandler(int iOrderID)
{
    for(vector<KOOrderPtr>::iterator itr = _vLiveOrders.begin(); itr != _vLiveOrders.end(); )
    {
        if((*itr)->igetOrderID() == iOrderID)
        {
            _vDeletedOrders.push_back((*itr));
            _vLiveOrders.erase(itr);
        }
        else
        {
            ++itr;
        }
    }

    _pParent->orderUnexpectedDeleteHandler(iOrderID);
}

void ContractAccount::orderDeleteRejectHandler(int iOrderID)
{
    _pParent->orderDeleteRejectHandler(iOrderID);
}

void ContractAccount::orderAmendRejectHandler(int iOrderID)
{
    _pParent->orderAmendRejectHandler(iOrderID);
}

void ContractAccount::orderCriticalErrorHandler(int iOrderID)
{
    _pParent->orderCriticalErrorHandler(iOrderID);
}

void ContractAccount::pullAllActiveOrders()
{
    for(vector<KOOrderPtr>::iterator itr = _vLiveOrders.begin(); itr != _vLiveOrders.end(); ++itr)
    {
        bdeleteOrder(*itr);
    }
}

void ContractAccount::retrieveDailyTrades(vector< boost::shared_ptr<Trade> >& vTargetVector)
{
    vTargetVector.insert(vTargetVector.end(), _vTrades.begin(), _vTrades.end());
}

void ContractAccount::resumeTrading()
{
    _eAccountState = Trading;
    _bExcessiveOrderHaltTriggered = false;
}

void ContractAccount::haltTrading()
{
    _eAccountState = Halt;
    pullAllActiveOrders();
}

long ContractAccount::igetDailyOrderActionCount()
{
    return _iNumOrderActions;
}

long ContractAccount::igetCurrentPosition()
{
    return _iPosition;
}

bool ContractAccount::bgetPositionInitialised()
{
    return _bPositionInitialised;
}

void ContractAccount::setInitialPosition(long iInitialPosition)
{
    _bPositionInitialised = true;
    _iPosition = iInitialPosition;
}

double ContractAccount::dGetCurrentPnL(double dMTMRatio)
{   
    // only update MtM if exhcange price is valid
    if(_pQuoteData->iBidSize != 0 && _pQuoteData->iAskSize != 0)
    {
        double dPnLInPrice = _dRealisedPnL;

        double dProductMidPrice;

        if(_pQuoteData->dBestAsk - _pQuoteData->dBestBid > 1.5 * _pQuoteData->dTickSize)
        {
            dProductMidPrice = (_pQuoteData->dBestAsk + _pQuoteData->dBestBid) / 2;
        }
        else
        {
            dProductMidPrice = _pQuoteData->dTickSize * ((double)_pQuoteData->iBidSize / ((double)_pQuoteData->iBidSize + (double)_pQuoteData->iAskSize)) + _pQuoteData->dBestBid;
        }

        if(_iPosition == 0)
        {
            dPnLInPrice = _dRealisedPnL;
        }
        else if(_iPosition > 0)
        {
            double dMarkingPrice;

            if(dMTMRatio > 1)
            {
                dMarkingPrice = dProductMidPrice;
            }
            else if(dMTMRatio < 0)
            {
                dMarkingPrice = _pQuoteData->dBestBid;
            }
            else
            {
                dMarkingPrice = dMTMRatio * dProductMidPrice + (1 - dMTMRatio) * _pQuoteData->dBestBid;
            }

            dPnLInPrice = _dRealisedPnL + _iPosition * dMarkingPrice;       
        }
        else 
        {
            double dMarkingPrice;

            if(dMTMRatio > 1)
            {
                dMarkingPrice = dProductMidPrice;
            }
            else if(dMTMRatio < 0)
            {
                dMarkingPrice = _pQuoteData->dBestAsk;
            }
            else
            {
                dMarkingPrice = dMTMRatio * dProductMidPrice + (1 - dMTMRatio) * _pQuoteData->dBestAsk;
            }

            dPnLInPrice = _dRealisedPnL + _iPosition * dMarkingPrice; 
        }

        double dPnLInDollar = dPnLInPrice * _pQuoteData->dContractSize * _pQuoteData->dRateToDollar;

        double dFee;
        dFee = 1000;

        if(_pQuoteData->eInstrumentType == KO_FUTURE)
        {
            dFee = _iTradedVolume * _pQuoteData->dTradingFee;
        }
        else if(_pQuoteData->eInstrumentType == KO_FX)
        {
            dFee = _iTradedVolume * _pQuoteData->dTradingFee;
        }

        _dLastMtMPnL = dPnLInDollar - dFee;
    }

    return _dLastMtMPnL;
}

double ContractAccount::dGetLastPnL()
{
    return _dLastMtMPnL;
}

long ContractAccount::iGetCurrentVolume()
{
    return _iTradedVolume;
}

void ContractAccount::checkCriticalErrors()
{
    if(_iNumOfCriticalErrors > 5)
    {
        _eAccountState = Halt;
        _pParent->manualHaltTrading();
        stringstream cStringStream;
        cStringStream << "More than 5 internal order critical errors received. " << _sEngineSlotName << " in halt mode.";
        ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, _sProduct, cStringStream.str());
    }
}

vector<KOOrderPtr>* ContractAccount::pgetAllLiveOrders()
{
    return &_vLiveOrders;
}

}
