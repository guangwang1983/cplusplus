#include "HCFXScheduler.h"

#include "SystemClock.h"
#include "ErrorHandler.h"
#include <framework/IModelContext.h>
#include <msg/message_enum.h>
#include <boost/math/special_functions/round.hpp>

namespace KO
{

static unsigned long igetNextOrderID()
{
    static long iNextOrderID = 0;
    return iNextOrderID++;
};

HCFXScheduler::HCFXScheduler(SchedulerConfig &cfg, bool bIsLiveTrading)
:SchedulerBase("Config", cfg)
{
    _iNextCIDKey = 0;
    _bSchedulerInitialised = false;
    _bIsLiveTrading =  bIsLiveTrading;
    _iTotalNumMsg = 0;
    
    preCommonInit();
}

HCFXScheduler::~HCFXScheduler()
{
}

void HCFXScheduler::initialize()
{
    if(!_bSchedulerInitialised)
    {
        _bSchedulerInitialised = true;
        postCommonInit();

        _sModelName = m_ctx->getModelConfig(m_ctx->getModelKey())->name;
        for(unsigned int i = 0;i < _vContractQuoteDatas.size();i++)
        {
            string sMarket = _vContractQuoteDatas[i]->sHCExchange;
            short iGatewayID = m_ctx->getSourcePriceKey(sMarket.c_str());
            _vContractQuoteDatas[i]->iGatewayID = iGatewayID;
        }

        ErrorHandler::GetInstance()->newInfoMsg("0", "", "", "Model Name is " + _sModelName);

        time_t rawtime;
        struct tm* aTm;

        time(&rawtime);
        aTm = localtime(&rawtime);

        HC::Ev_Abs_Time abs;
        abs.year = aTm->tm_year + 1900;
        abs.month = aTm->tm_mon + 1;
        abs.day = aTm->tm_mday;
        abs.hour = aTm->tm_hour;
        abs.minute = aTm->tm_min;
        abs.second = aTm->tm_sec + 20;       
        abs.micro_second = 1200; 

        m_ctx->addEvTimer(&updateTimer, this, abs, 1000000);

//      old timer code for simulation
//        m_ctx->addEvTimer(&updateTimer, this, 1000000, true);

        sortTimeEvent();
    }
}

void HCFXScheduler::updateTimer(int iID, void * data)
{
    (void) iID;
    HCFXScheduler* pScheduler = const_cast<HCFXScheduler*>(static_cast<const HCFXScheduler*>(data));
    pScheduler->onTimer();
}

void HCFXScheduler::onTimer()
{
    KOEpochTime cNewUpdateTime;

    cNewUpdateTime = KOEpochTime(0, m_ctx->getTime(CURRENT_TIME) / 1000);

    checkOrderState(cNewUpdateTime);

    SchedulerBase::wakeup(cNewUpdateTime);
    processTimeEvents(cNewUpdateTime);
}

void HCFXScheduler::addInstrument(unsigned int iProdConfigIdx, HC::instrumentkey_t iInstrumentKey)
{
    QuoteData* pNewQuoteDataPtr;

    if(_cSchedulerCfg.vProducts[iProdConfigIdx].find("CFX") == 0)
    {
        pNewQuoteDataPtr = pregisterProduct(_cSchedulerCfg.vProducts[iProdConfigIdx], KO_FX);
    }
    else
    {
        pNewQuoteDataPtr = pregisterProduct(_cSchedulerCfg.vProducts[iProdConfigIdx], KO_FUTURE);
    }

    pNewQuoteDataPtr->sHCProduct = _cSchedulerCfg.vHCProducts[iProdConfigIdx];
    pNewQuoteDataPtr->bIsLocalProduct = _cSchedulerCfg.vIsLocalProducts[iProdConfigIdx];
    pNewQuoteDataPtr->iHCInstrumentKey = iInstrumentKey;
    pNewQuoteDataPtr->iCID = _iNextCIDKey;
    pNewQuoteDataPtr->bInBundle = false;
    pNewQuoteDataPtr->iProductMaxRisk = _cSchedulerCfg.vProductMaxRisk[iProdConfigIdx];
    pNewQuoteDataPtr->iProductExpoLimit = _cSchedulerCfg.vProductExpoLimit[iProdConfigIdx];
    _vProductOrderList.push_back(vector<KOOrderPtr>());
    _vProductDesiredPos.push_back(0);
    _vProductPos.push_back(0);

    _vLastOrderError.push_back("");

    _vProductLiquidationOrderList.push_back(vector<KOOrderPtr>());
    _vProductLiquidationDesiredPos.push_back(0);
    _vProductLiquidationPos.push_back(0);

    _vProductConsideration.push_back(0.0);
    _vProductVolume.push_back(0);

    _vProductStopLoss.push_back(_cSchedulerCfg.vProductStopLoss[iProdConfigIdx]);
    _vProductAboveSLInSec.push_back(0);
    _vProductLiquidating.push_back(false);

    _vFirstOrderTime.push_back(KOEpochTime());

    _iNextCIDKey = _iNextCIDKey + 1;
}

void HCFXScheduler::futuresMsgEvent(short gateway, HC_GEN::instrumentkey_t instrumentKey, FUTURES_EVENT_TYPE eventType, FUTURES_MSG_INFO_STRUCT * msgInfo)
{
    (void) msgInfo;

    if(eventType == EVENT_END_BUNDLE)
    {
        for(unsigned int i = 0; i < _vContractQuoteDatas.size(); ++i)
        {
            if(_vContractQuoteDatas[i]->iHCInstrumentKey == instrumentKey)
            {
                if(_vContractQuoteDatas[i]->bIsLocalProduct == true)
                {
                    IBook* pBook = this->m_ctx->getIBook(gateway, instrumentKey);
                    newHCPriceUpdate(i, pBook);
                    break;
                }
            }
        }
    }
}

void HCFXScheduler::onIBook(IBook *pBook)
{
    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); ++i)
    {
        if(_vContractQuoteDatas[i]->iHCInstrumentKey == pBook->getInstrumentKey())
        {
            if(_vContractQuoteDatas[i]->bIsLocalProduct == false || _sModelName == "levelup_01_aur" || _sModelName == "levelup_02_aur")
            {
                newHCPriceUpdate(i, pBook);
            }
            break;
        }
    }

    Model::onIBook(pBook);
}

void HCFXScheduler::onConflationDone() 
{
    if(_cSchedulerCfg.bUseOnConflationDone == true)
    {
        const IBook * pBook = getNextUpdateIBook();

        while (pBook != nullptr)
        {
            for(unsigned int i = 0; i < _vContractQuoteDatas.size(); ++i)
            {
                if(_vContractQuoteDatas[i]->iHCInstrumentKey == pBook->getInstrumentKey())
                {
                    if(_vContractQuoteDatas[i]->bIsLocalProduct == true)
                    {
                        newHCPriceUpdate(i, pBook);
                    }
                    break;
                }
            }

            pBook = getNextUpdateIBook();
        }

        clearUpdatedIBooks();
    }
}

void HCFXScheduler::newHCPriceUpdate(int i, const IBook *pBook)
{
    _vContractQuoteDatas[i]->iPrevBidInTicks = _vContractQuoteDatas[i]->iBestBidInTicks;
    _vContractQuoteDatas[i]->iPrevAskInTicks = _vContractQuoteDatas[i]->iBestAskInTicks;
    _vContractQuoteDatas[i]->iPrevBidSize = _vContractQuoteDatas[i]->iBidSize;
    _vContractQuoteDatas[i]->iPrevAskSize = _vContractQuoteDatas[i]->iAskSize;

    IPriceLevel * pBidLvl = pBook->getPriceLevel(HC_GEN::SIDE::BID, 0, OUTRIGHT_AND_IMPLIED_LEVEL);
    if(pBidLvl != NULL)
    {
        _vContractQuoteDatas[i]->dBestBid = pBidLvl->getPrice();
        _vContractQuoteDatas[i]->iBestBidInTicks = boost::math::iround(_vContractQuoteDatas[i]->dBestBid / _vContractQuoteDatas[i]->dTickSize);
        _vContractQuoteDatas[i]->iBidSize = pBidLvl->getQuantity(OUTRIGHT_AND_IMPLIED_LEVEL);
    }

    IPriceLevel* pOfferLvl = pBook->getPriceLevel(HC_GEN::SIDE::OFFER, 0, OUTRIGHT_AND_IMPLIED_LEVEL) ;
    if(pOfferLvl != NULL)
    {
        _vContractQuoteDatas[i]->dBestAsk = pOfferLvl->getPrice();
        _vContractQuoteDatas[i]->iBestAskInTicks = boost::math::iround(_vContractQuoteDatas[i]->dBestAsk / _vContractQuoteDatas[i]->dTickSize);
        _vContractQuoteDatas[i]->iAskSize = pOfferLvl->getQuantity(OUTRIGHT_AND_IMPLIED_LEVEL);
    }

    // we dont use last trade info in live trading. set the accum trade size to dummy value to pass the price validity check
    _vContractQuoteDatas[i]->iAccumuTradeSize = -1;

    if(pOfferLvl != NULL && pBidLvl != NULL)
    {
        if(_vContractQuoteDatas[i]->iPrevBidInTicks != _vContractQuoteDatas[i]->iBestBidInTicks || _vContractQuoteDatas[i]->iPrevAskInTicks != _vContractQuoteDatas[i]->iBestAskInTicks || _vContractQuoteDatas[i]->iPrevBidSize != _vContractQuoteDatas[i]->iBidSize || _vContractQuoteDatas[i]->iPrevAskSize != _vContractQuoteDatas[i]->iAskSize)
        {
            double dWeightedMidInTicks;

            if(_vContractQuoteDatas[i]->iBestAskInTicks - _vContractQuoteDatas[i]->iBestBidInTicks <= 0)
            {
                dWeightedMidInTicks = (double)(_vContractQuoteDatas[i]->iBestAskInTicks + _vContractQuoteDatas[i]->iBestBidInTicks) / 2;
            }
            else if(_vContractQuoteDatas[i]->iBestAskInTicks - _vContractQuoteDatas[i]->iBestBidInTicks > 1 && _vContractQuoteDatas[i]->iBidSize > 0 && _vContractQuoteDatas[i]->iAskSize > 0)
            {
                dWeightedMidInTicks = (double)(_vContractQuoteDatas[i]->iBestAskInTicks + _vContractQuoteDatas[i]->iBestBidInTicks) / 2;
            }
            else
            {
                dWeightedMidInTicks = (double)_vContractQuoteDatas[i]->iBestBidInTicks + (double)_vContractQuoteDatas[i]->iBidSize / (double)(_vContractQuoteDatas[i]->iBidSize + _vContractQuoteDatas[i]->iAskSize);
            }

            _vContractQuoteDatas[i]->dWeightedMidInTicks = dWeightedMidInTicks;
            _vContractQuoteDatas[i]->dWeightedMid = dWeightedMidInTicks * _vContractQuoteDatas[i]->dTickSize;
            newPriceUpdate(i);

            if(_vContractQuoteDatas[i]->iBestAskInTicks - _vContractQuoteDatas[i]->iBestBidInTicks > 0)
            {
//                no order amend for fx
//                updateOrderPrice(i);
//                updateLiquidationOrderPrice(i);
            }
        }
    }
}

void HCFXScheduler::updateLiquidationOrderPrice(unsigned int iProductIdx)
{
    vector<KOOrderPtr>* pOrderList = &_vProductLiquidationOrderList[iProductIdx];
    bool bAllOrderConfirmed = true;
    long iOpenOrderQty = 0;
    for(unsigned int i = 0; i < pOrderList->size(); i++)
    {
        bAllOrderConfirmed = bAllOrderConfirmed && (*pOrderList)[i]->borderCanBeChanged();
        iOpenOrderQty = iOpenOrderQty + (*pOrderList)[i]->igetOrderRemainQty();
    }

    if(bAllOrderConfirmed && iOpenOrderQty != 0)
    {
        long iOrderPriceInTicks = icalcualteOrderPrice(iProductIdx, (*pOrderList)[0]->igetOrderPriceInTicks(), iOpenOrderQty, true, true, false);
        if(iOrderPriceInTicks != (*pOrderList)[0]->igetOrderPriceInTicks())
        {
            for(unsigned int i = 0; i < pOrderList->size(); i++)
            {
                amendOrderPriceInTicks((*pOrderList)[i], iOrderPriceInTicks);
            }
        }
    }
}

void HCFXScheduler::updateOrderPrice(unsigned int iProductIdx)
{
    vector<KOOrderPtr>* pOrderList = &_vProductOrderList[iProductIdx];
    bool bAllOrderConfirmed = true;
    long iOpenOrderQty = 0;
    for(unsigned int i = 0; i < pOrderList->size(); i++)
    {
        bAllOrderConfirmed = bAllOrderConfirmed && (*pOrderList)[i]->borderCanBeChanged();
        iOpenOrderQty = iOpenOrderQty + (*pOrderList)[i]->igetOrderRemainQty();
    }

    if(bAllOrderConfirmed && iOpenOrderQty != 0)
    {
        if(pOrderList->size() > 0)
        {
            long iOrderPriceInTicks = icalcualteOrderPrice(iProductIdx, (*pOrderList)[0]->igetOrderPriceInTicks(), iOpenOrderQty, true, false, false);
            if(iOrderPriceInTicks != (*pOrderList)[0]->igetOrderPriceInTicks())
            {
                for(unsigned int i = 0; i < pOrderList->size(); i++)
                {
                    amendOrderPriceInTicks((*pOrderList)[i], iOrderPriceInTicks);
                }
            }
        }
    }
}

KOEpochTime HCFXScheduler::cgetCurrentTime()
{
    if(_bSchedulerInitialised)
    {
        return KOEpochTime(0, m_ctx->getTime(CURRENT_TIME) / 1000);
    }
    else
    {
        return KOEpochTime(0,0);
    }
}

bool HCFXScheduler::bisLiveTrading()
{
    return _bIsLiveTrading;
}

bool HCFXScheduler::sendToExecutor(const string& sProduct, long iDesiredPos)
{
    // for sterling sonia swtich
    if(sProduct.substr(0, 1) == "L")
    {
        iDesiredPos = iDesiredPos / 2;
    }

    int iProductIdx = -1;

    // for fx trading
    string sAdjustedProduct = sProduct;
    if(sAdjustedProduct.find("6B") != std::string::npos)
    {
        sAdjustedProduct = "GBPUSD";
    }
    else if(sAdjustedProduct.find("6E") != std::string::npos)
    {
        sAdjustedProduct = "EURUSD";
    }        

    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
    {
        if(_vContractQuoteDatas[i]->sProduct == sAdjustedProduct)
        {
            iProductIdx = i;
            break;
        }
    }

    if(iProductIdx != -1)
    {
        long iProductExpoLimit = _vContractQuoteDatas[iProductIdx]->iProductExpoLimit;

        if(_vFirstOrderTime[iProductIdx].igetPrintable() == 0)
        {
            _vFirstOrderTime[iProductIdx] = cgetCurrentTime();
        }

        //if((cgetCurrentTime() - _vFirstOrderTime[iProductIdx]).igetPrintable() < 300000000 || _vProductLiquidating[iProductIdx] == true)
        //{
        //    iProductExpoLimit = iProductExpoLimit / 5;
        //}

        long iAdjustedDesiredPos = iDesiredPos;

        long iPosDelta = iDesiredPos - _vProductPos[iProductIdx];
        if(iPosDelta > iProductExpoLimit)
        {
            iAdjustedDesiredPos = iProductExpoLimit + _vProductPos[iProductIdx];
            iPosDelta = iProductExpoLimit;
        }
        else if(iPosDelta < -1 * iProductExpoLimit)
        {
            iAdjustedDesiredPos = -1 * iProductExpoLimit + _vProductPos[iProductIdx];
            iPosDelta = -1 * iProductExpoLimit;
        }

        stringstream cStringStream;
        cStringStream << "Target Update Qty: " << iDesiredPos << " Adjusted Qty " << iAdjustedDesiredPos << ".";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sAdjustedProduct, cStringStream.str());

        _vProductDesiredPos[iProductIdx] = iAdjustedDesiredPos;

        vector<KOOrderPtr>* pOrderList = &_vProductOrderList[iProductIdx];
        bool bAllOrderConfirmed = true;
        for(unsigned int i = 0; i < pOrderList->size(); i++)
        {
            bAllOrderConfirmed = bAllOrderConfirmed && (*pOrderList)[i]->borderCanBeChanged();
        }

        if(bAllOrderConfirmed)
        {
            int iOpenOrderQty = 0;

            for(unsigned int i = 0; i < pOrderList->size(); i++)
            {
                if(iPosDelta * (*pOrderList)[i]->igetOrderRemainQty() > 0)
                {
                    int iUnallocatedQty = iPosDelta - iOpenOrderQty;
                    if(iUnallocatedQty == 0)
                    {
                        deleteOrder((*pOrderList)[i]);
                    }
                    else if(abs(iUnallocatedQty) < abs((*pOrderList)[i]->igetOrderRemainQty()))
                    {
                        long iOrderPriceInTicks = icalcualteOrderPrice(iProductIdx, (*pOrderList)[i]->igetOrderPriceInTicks(), (*pOrderList)[i]->igetOrderRemainQty(), true, false, false);
                        amendOrder((*pOrderList)[i], iUnallocatedQty, iOrderPriceInTicks);
                        iOpenOrderQty = iOpenOrderQty + iUnallocatedQty;
                    }
                    else
                    {
                        iOpenOrderQty = iOpenOrderQty + (*pOrderList)[i]->igetOrderRemainQty();
                    }
                }
                else
                {
                    deleteOrder((*pOrderList)[i]);
                }
            }

            if(iPosDelta - iOpenOrderQty != 0)
            {
                submitOrderBestPrice(iProductIdx, iPosDelta - iOpenOrderQty, false);
            }
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Unrecognised product " << sAdjustedProduct << " received in executor.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    return true;
}

long HCFXScheduler::icalcualteOrderPrice(unsigned int iProductIdx, long iOrderPrice, long iQty, bool bOrderInMarket, bool bIsLiquidation, bool bIsIOC)
{
    long iNewOrderPrice;

    if(bIsLiquidation == true || bIsIOC == true)
    {
        if(iQty > 0)
        {
            iNewOrderPrice = _vContractQuoteDatas[iProductIdx]->iBestAskInTicks;
        }
        else
        {
            iNewOrderPrice = _vContractQuoteDatas[iProductIdx]->iBestBidInTicks;
        }
    }
    else
    {
        long iMarketQty;
        if(iQty > 0)
        {
            iNewOrderPrice = _vContractQuoteDatas[iProductIdx]->iBestBidInTicks;
            iMarketQty = _vContractQuoteDatas[iProductIdx]->iBidSize;
        }
        else
        {
            iNewOrderPrice = _vContractQuoteDatas[iProductIdx]->iBestAskInTicks;
            iMarketQty = _vContractQuoteDatas[iProductIdx]->iAskSize;
        }

        if(_vContractQuoteDatas[iProductIdx]->iBestAskInTicks - _vContractQuoteDatas[iProductIdx]->iBestBidInTicks > 2)
        {
            if(iMarketQty < 10)
            {
                if(bOrderInMarket)
                {
                    if(iNewOrderPrice != iOrderPrice)
                    {
                        if(iQty > 0)
                        {
                            iNewOrderPrice = iNewOrderPrice - 1;
                        }
                        else
                        {
                            iNewOrderPrice = iNewOrderPrice + 1;
                        }
                    }
                    else
                    {
                        if(abs(iQty) == iMarketQty)
                        {
                            if(iQty > 0)
                            {
                                iNewOrderPrice = iNewOrderPrice - 1;
                            }
                            else
                            {
                                iNewOrderPrice = iNewOrderPrice + 1;
                            }
                        }
                    }
                }
                else
                {
                    if(iQty > 0)
                    {
                        iNewOrderPrice = iNewOrderPrice - 1;
                    }
                    else
                    {
                        iNewOrderPrice = iNewOrderPrice + 1;
                    }
                }
            }
            else
            {
                stringstream cStringStream;
                cStringStream << "join best price wide market " << iNewOrderPrice;
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
            }
        }
        else
        {
            if(bOrderInMarket)
            {
                if(abs(iQty) == iMarketQty)
                {
                    if(iQty > 0)
                    {
                        iNewOrderPrice = iNewOrderPrice - 1;
                    }
                    else
                    {
                        iNewOrderPrice = iNewOrderPrice + 1;
                    }
                }
            }
        }
    }

    return iNewOrderPrice;
}

void HCFXScheduler::submitOrderBestPrice(unsigned int iProductIdx, long iQty, bool bIsLiquidation)
{
    bool bIsIOC = false;
    if(_vContractQuoteDatas[iProductIdx]->eInstrumentType == KO_FUTURE)
    {
        bIsIOC = false;
    }
    else
    {
        bIsIOC = true;
    }

    if(bcheckRisk(iProductIdx, iQty) == false)
    {
        return;
    }

    if(bIsIOC == true)
    {
        if(_vContractQuoteDatas[iProductIdx]->iBestAskInTicks - _vContractQuoteDatas[iProductIdx]->iBestBidInTicks > 10)
        {
            stringstream cStringStream;
            cStringStream.precision(10);
            cStringStream << "Ignore new Order submit. Spread width greater than 10.";
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());
            return;
        }
        else
        {
            stringstream cStringStream;
            cStringStream.precision(10);
            cStringStream << "IOC Market Snapshot:" << _vContractQuoteDatas[iProductIdx]->iBidSize << "|" << _vContractQuoteDatas[iProductIdx]->iBestBidInTicks << "|" << _vContractQuoteDatas[iProductIdx]->iBestAskInTicks << "|" << _vContractQuoteDatas[iProductIdx]->iAskSize;
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());
        }
    }

    if(m_orderManager->isInstrumentEnabled(_vContractQuoteDatas[iProductIdx]->iHCInstrumentKey))
    {
        KOOrderPtr pOrder;
        
        pOrder.reset(new KOOrder(igetNextOrderID(), iProductIdx, _vContractQuoteDatas[iProductIdx]->iGatewayID, _vContractQuoteDatas[iProductIdx]->iHCInstrumentKey, _vContractQuoteDatas[iProductIdx]->dTickSize, bIsIOC, _vContractQuoteDatas[iProductIdx]->sProduct, _vContractQuoteDatas[iProductIdx]->sProduct, _vContractQuoteDatas[iProductIdx]->sExchange, _vContractQuoteDatas[iProductIdx]->eInstrumentType, NULL));

         double dOrderPrice = icalcualteOrderPrice(iProductIdx, 0, iQty, false, bIsLiquidation, bIsIOC) * _vContractQuoteDatas[iProductIdx]->dTickSize;

        ORDER_SEND_ERROR cSendOrderResult;
        if(pOrder->egetInstrumentType() == KO_FUTURE)
        {
            pOrder->_pHCOrder = m_orderManager->createOrder(HC_GEN::FUTURES, pOrder->_iGatewayID, pOrder->_iInstrumentKey);

            if(iQty > 0)
            {
                pOrder->_pHCOrder->setAction(HC_GEN::ACTION::BUY);
            }
            else
            {
                pOrder->_pHCOrder->setAction(HC_GEN::ACTION::SELL);
            }

            pOrder->_pHCOrder->setIFM(true);

            pOrder->_pHCOrder->setOrderRate(dOrderPrice);

            pOrder->_pHCOrder->setOrderType(HC_GEN::LIMIT);

            if(pOrder->_bIsIOC)
            {
                pOrder->_pHCOrder->setTimeInForce(HC_GEN::IOC);
            }
            else
            {
                pOrder->_pHCOrder->setTimeInForce(HC_GEN::DAY);
            }

            pOrder->_pHCOrder->setAmount(abs(iQty));

            if(pOrder->sgetOrderExchange() == "XEUR")
            {
                pOrder->_pHCOrder->setAlgoFlag(418);
            }

            stringstream cStringStream;
            cStringStream.precision(10);
            if(bIsLiquidation == false)
            {
                if(bIsIOC == true)
                {
                    cStringStream << "Submitting new IOC order " << pOrder->igetOrderID() << ". qty " << iQty << " price " << dOrderPrice << ".";
                }
                else
                {
                    cStringStream << "Submitting new order " << pOrder->igetOrderID() << ". qty " << iQty << " price " << dOrderPrice << ".";
                }
            }
            else
            {
                cStringStream << "Submitting new liquidation order " << pOrder->igetOrderID() << ". qty " << iQty << " price " << dOrderPrice << ".";
            }
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());

            cSendOrderResult = m_orderManager->sendOrder(pOrder->_pHCOrder, this);
        }
        else
        {
            if(iQty > 0)
            {
                pOrder->_pHCOrderRouter = m_orderManager->createOrderRouter(pOrder->_iInstrumentKey, HC_GEN::ACTION::BUY, abs(iQty), dOrderPrice);
            }
            else
            {
                pOrder->_pHCOrderRouter = m_orderManager->createOrderRouter(pOrder->_iInstrumentKey, HC_GEN::ACTION::SELL, abs(iQty), dOrderPrice);
            }

            stringstream cStringStream;
            cStringStream.precision(10);
            if(bIsLiquidation == false)
            {
                cStringStream << "Submitting new order router " << pOrder->igetOrderID() << ". qty " << iQty << " price " << dOrderPrice << ".";
            }
            else
            {
                cStringStream << "Submitting new liquidation order router " << pOrder->igetOrderID() << ". qty " << iQty << " price " << dOrderPrice << ".";
            }
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());
           
            cSendOrderResult = m_orderManager->sendOrderRouter(pOrder->_pHCOrderRouter, this);
        }

        if(cSendOrderResult == SUCCESS)
        {
            pOrder->changeOrderstat(KOOrder::PENDINGCREATION);
            if(bIsLiquidation == false)
            {
                _vProductOrderList[iProductIdx].push_back(pOrder);
            }
            else
            {
                _vProductLiquidationOrderList[iProductIdx].push_back(pOrder);

            }

            pOrder->_cPendingRequestTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();

            pOrder->_qOrderMessageHistory.push_back(SystemClock::GetInstance()->cgetCurrentKOEpochTime());

            stringstream cStringStream;
            cStringStream.precision(10);
            if(pOrder->egetInstrumentType() == KO_FUTURE)
            {
                cStringStream << "Order submitted. ID is " << pOrder->_pHCOrder->getOrderKey() << ".";
            }
            else
            {
                cStringStream << "Order router submitted. ID is " << pOrder->_pHCOrderRouter->getOrderID() << ".";
            }
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());
            _iTotalNumMsg = _iTotalNumMsg + 1;
        }
        else
        {
            pOrder->_pHCOrder = NULL;
            pOrder->_pHCOrderRouter = NULL;

            stringstream cStringStream;
            cStringStream.precision(10);
            cStringStream << "Failed to submit new order. Reason: ";
            cStringStream << stranslateHCErrorCode(cSendOrderResult) << ".";

            if(_vLastOrderError[iProductIdx] != cStringStream.str())
            {
                _vLastOrderError[iProductIdx] = cStringStream.str();
                ErrorHandler::GetInstance()->newErrorMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
            }
            ErrorHandler::GetInstance()->newInfoMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
        }
    }
    else
    {
            stringstream cStringStream;
            cStringStream << "Failed to submit order! Product " << _vContractQuoteDatas[iProductIdx]->sProduct << " disabled for trading.";
            
            if(_vLastOrderError[iProductIdx] != cStringStream.str())
            {
                _vLastOrderError[iProductIdx] = cStringStream.str();
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
            }
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void HCFXScheduler::amendOrder(KOOrderPtr pOrder, long iQty, long iPriceInTicks)
{
    if(pOrder->_pHCOrder != NULL)
    {
        long iqtyDelta = abs(iQty) - pOrder->_pHCOrder->getAmountLeft();
        long iqtyRealDelta = iqtyDelta;
        if(iQty < 0)
        {
            iqtyRealDelta = iqtyRealDelta * -1;
        }

        if(bcheckRisk(pOrder->_iCID, iqtyRealDelta) == true || bcheckOrderMsgHistory(pOrder) == true)
        {
            double dNewPrice = iPriceInTicks * pOrder->_dTickSize;
            long iNewQty = pOrder->_pHCOrder->getAmount() + iqtyDelta;

            pOrder->_pHCOrder->setReplaceRate(dNewPrice);
            pOrder->_pHCOrder->setReplaceAmount(iNewQty);

            stringstream cStringStream;
            cStringStream.precision(10);
            cStringStream << "Amending order " << pOrder->igetOrderID() << " HC order id " <<  pOrder->_pHCOrder->getOrderKey() << ". new qty " << iQty << " new price " << dNewPrice << ".";
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", pOrder->sgetOrderProductName(), cStringStream.str());
            cStringStream.str("");
            cStringStream.clear();

            ORDER_SEND_ERROR cSendOrderResult = m_orderManager->sendOrderReplace(pOrder->_pHCOrder);

            if(cSendOrderResult == SUCCESS)
            {
                pOrder->changeOrderstat(KOOrder::PENDINGCHANGE);
                pOrder->_cPendingRequestTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
                pOrder->_qOrderMessageHistory.push_back(SystemClock::GetInstance()->cgetCurrentKOEpochTime());
                cStringStream << "Order amendment submitted. \n";
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
                cStringStream.str("");
                cStringStream.clear();

                _iTotalNumMsg = _iTotalNumMsg + 1;
            }
            else
            {
                cStringStream << "Failed to submit order amendment for KOOrder ID " << pOrder->igetOrderID() << " HCID " << pOrder->_pHCOrder->getOrderKey() << "." << " Reason: ";
                cStringStream << stranslateHCErrorCode(cSendOrderResult) << ".";

                if(_vLastOrderError[pOrder->_iCID] != stranslateHCErrorCode(cSendOrderResult))
                {
                    _vLastOrderError[pOrder->_iCID] = stranslateHCErrorCode(cSendOrderResult);
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                cStringStream.str("");
                cStringStream.clear();
            }
        }
    }
}

void HCFXScheduler::amendOrderPriceInTicks(KOOrderPtr pOrder, long iPriceInTicks)
{
    if(pOrder->_pHCOrder != NULL)
    {
        if(bcheckOrderMsgHistory(pOrder) == true)
        {
            double dNewPrice = iPriceInTicks * pOrder->_dTickSize;
            pOrder->_pHCOrder->setReplaceAmount(pOrder->_pHCOrder->getAmount());
            pOrder->_pHCOrder->setReplaceRate(dNewPrice);


            stringstream cStringStream;
            cStringStream.precision(10);
            cStringStream << "Amending price for order " << pOrder->igetOrderID() << " HC order id " <<  pOrder->_pHCOrder->getOrderKey() << ". new price " << dNewPrice << ".";
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", pOrder->sgetOrderProductName(), cStringStream.str());
            cStringStream.str("");
            cStringStream.clear();

            ORDER_SEND_ERROR cSendOrderResult = m_orderManager->sendOrderReplace(pOrder->_pHCOrder);

            if(cSendOrderResult == SUCCESS)
            {
                pOrder->changeOrderstat(KOOrder::PENDINGCHANGE);
                pOrder->_cPendingRequestTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
                cStringStream << "Order amendment submitted. \n";
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
                cStringStream.str("");
                cStringStream.clear();

                _iTotalNumMsg = _iTotalNumMsg + 1;
            }
            else
            {
                cStringStream << "Failed to submit order amendment for KOOrder ID " << pOrder->igetOrderID() << " HCID " << pOrder->_pHCOrder->getOrderKey() << "." << " Reason: ";
                cStringStream << stranslateHCErrorCode(cSendOrderResult) << ".";

                if(_vLastOrderError[pOrder->_iCID] != stranslateHCErrorCode(cSendOrderResult))
                {
                    _vLastOrderError[pOrder->_iCID] = stranslateHCErrorCode(cSendOrderResult);
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                cStringStream.str("");
                cStringStream.clear();
            }
        }
    }
}

void HCFXScheduler::deleteOrder(KOOrderPtr pOrder)
{
    if(pOrder->_pHCOrder != NULL)
    {
        if(bcheckOrderMsgHistory(pOrder) == true)
        {
            ORDER_SEND_ERROR cSendOrderResult = m_orderManager->sendOrderCancel(pOrder->_pHCOrder);

            stringstream cStringStream;
            cStringStream << "Deleting order " << pOrder->igetOrderID() << " HC order id " <<  pOrder->_pHCOrder->getOrderKey() << ".";
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", pOrder->sgetOrderProductName(), cStringStream.str());
            cStringStream.str("");
            cStringStream.clear();

            if(cSendOrderResult == SUCCESS)
            {
                pOrder->changeOrderstat(KOOrder::PENDINGDELETE);
                cStringStream << "Order delete submitted. \n";
                pOrder->_cPendingRequestTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime(); 
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
                cStringStream.str("");
                cStringStream.clear();

                _iTotalNumMsg = _iTotalNumMsg + 1;
            }
            else
            {
                cStringStream << "Failed to submit order delete for KOOrder ID " << pOrder->igetOrderID() << " HCID " << pOrder->_pHCOrder->getOrderKey() << "." << " Reason: ";
                cStringStream << stranslateHCErrorCode(cSendOrderResult) << ".";

                if(_vLastOrderError[pOrder->_iCID] != stranslateHCErrorCode(cSendOrderResult))
                {
                    _vLastOrderError[pOrder->_iCID] = stranslateHCErrorCode(cSendOrderResult);
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                cStringStream.str("");
                cStringStream.clear();
            }
        }
    }
}

string HCFXScheduler::stranslateHCRejectReason(FuturesOrder::OrderRejectReason::OrderRejectReason cRejectReason)
{
    switch(cRejectReason)
    {
        case FuturesOrder::OrderRejectReason::GTC_NOT_ENABLED_FOR_MODEL:
        {
            return "GTC_NOT_ENABLED_FOR_MODEL:";
        }
        case FuturesOrder::OrderRejectReason::UNKNOWN_ALGO_ID:
        {
            return "UNKNOWN_ALGO_ID";
        }
        case FuturesOrder::OrderRejectReason::INVALID_ORDER_PER_EXCHANGE:
        {
            return "INVALID_ORDER_PER_EXCHANGE";
        }
        case FuturesOrder::OrderRejectReason::UNKNOWN_CONTRACT:
        {
            return "UNKNOWN_CONTRACT";
        }
        case FuturesOrder::OrderRejectReason::CONNECTION_DROPPED:
        {
            return "CONNECTION_DROPPED";
        }
        case FuturesOrder::OrderRejectReason::UNSPECIFIED:
        {
            return "UNSPECIFIED";
        }
        case FuturesOrder::OrderRejectReason::SUCCESS:
        {
            return "SUCCESS";
        }
        case FuturesOrder::OrderRejectReason::ORDER_EXCEEDS_LIMIT:
        {
            return "ORDER_EXCEEDS_LIMIT";
        }
        case FuturesOrder::OrderRejectReason::MARKET_CLOSED_OR_PAUSED:
        {
            return "MARKET_CLOSED_OR_PAUSED";
        }
        case FuturesOrder::OrderRejectReason::INVALID_FIELD:
        {
            return "INVALID_FIELD";
        }
        case FuturesOrder::OrderRejectReason::PRICE_MUST_BE_GREATER_0:
        {
            return "PRICE_MUST_BE_GREATER_0";
        }
        case FuturesOrder::OrderRejectReason::MODEL_NOT_PROVISIONED:
        {
            return "MODEL_NOT_PROVISIONED";
        }
        case FuturesOrder::OrderRejectReason::UNSUPPORTED_ORDER_CHARACTERISTIC:
        {
            return "UNSUPPORTED_ORDER_CHARACTERISTIC";
        }
        case FuturesOrder::OrderRejectReason::MARKET_PRICE_ORDERS_NOT_SUPPORTED_BY_OPPOSITE_LIMIT:
        {
            return "MARKET_PRICE_ORDERS_NOT_SUPPORTED_BY_OPPOSITE_LIMIT";
        }
        case FuturesOrder::OrderRejectReason::OTHERS:
        {
            return "OTHERS";
        }
        case FuturesOrder::OrderRejectReason::GTD_EXPIRE_DATE_EARILER_THAN_CURRENT_EOD:
        {
            return "GTD_EXPIRE_DATE_EARILER_THAN_CURRENT_EOD";
        }
        case FuturesOrder::OrderRejectReason::REJECTED:
        {
            return "REJECTED";
        }
        case FuturesOrder::OrderRejectReason::ORDER_NOT_IN_BOOK:
        {
            return "ORDER_NOT_IN_BOOK";
        }
        case FuturesOrder::OrderRejectReason::DISCLOSED_QUANTITY_GREATER_THAN_TOTAL_OR_REMAINING_QTY:
        {
            return "DISCLOSED_QUANTITY_GREATER_THAN_TOTAL_OR_REMAINING_QTY";
        }
        case FuturesOrder::OrderRejectReason::STOP_PRICE_SMALLER_THAN_TRIGGER_PRICE:
        {
            return "STOP_PRICE_SMALLER_THAN_TRIGGER_PRICE";
        }
        case FuturesOrder::OrderRejectReason::STOP_PRICE_GREATER_THAN_TRIGGER_PRICE:
        {
            return "STOP_PRICE_GREATER_THAN_TRIGGER_PRICE";
        }
        case FuturesOrder::OrderRejectReason::STOP_PRICE_MUST_BELOW_LAST_TRADE_PRICE:
        {
            return "STOP_PRICE_MUST_BELOW_LAST_TRADE_PRICE";
        }
        case FuturesOrder::OrderRejectReason::STOP_PRICE_MUST_ABOVE_LAST_TRADE_PRICE:
        {
            return "STOP_PRICE_MUST_ABOVE_LAST_TRADE_PRICE";
        }
        case FuturesOrder::OrderRejectReason::ORDER_QTY_OUTSIDE_RANGE:
        {
            return "ORDER_QTY_OUTSIDE_RANGE";
        }
        case FuturesOrder::OrderRejectReason::ORDER_TYPE_NOT_PERMITTED_WHILE_MTK_IS_PRE_OPEN:
        {
            return "ORDER_TYPE_NOT_PERMITTED_WHILE_MTK_IS_PRE_OPEN";
        }
        case FuturesOrder::OrderRejectReason::ORDER_PRICE_OUTSIDE_LIMITS:
        {
            return "ORDER_PRICE_OUTSIDE_LIMITS";
        }
        case FuturesOrder::OrderRejectReason::ORDER_PRICE_OUTSIDE_BANDS:
        {
            return "ORDER_PRICE_OUTSIDE_BANDS";
        }
        case FuturesOrder::OrderRejectReason::ORDER_QTY_TOO_LOW:
        {
            return "ORDER_QTY_TOO_LOW";
        }
        case FuturesOrder::OrderRejectReason::ORDER_NOT_ALLOWED_WHILE_MTK_IS_RESERVED:
        {
            return "ORDER_NOT_ALLOWED_WHILE_MTK_IS_RESERVED";
        }
        case FuturesOrder::OrderRejectReason::TRADE_DISABLED:
        {
            return "TRADE_DISABLED";
        }
    }

    return "UNKNOWN REASON";
}

string HCFXScheduler::stranslateHCErrorCode(ORDER_SEND_ERROR cSendOrderResult)
{
    switch(cSendOrderResult)
    {
        case LIQUIDATE_ONLY:
        {
            return "LIQUIDATE_ONLY";
        }
        case GLOBAL_TRADING_DISABLED:
        {
            return "GLOBAL_TRADING_DISABLED";
        }
        case NODE_DISABLED:
        {
            return "NODE_DISABLED";
        }
        case INSTRUMENT_DISABLED:
        {
            return "INSTRUMENT_DISABLED";
        }
        case INSTRUMENT_FOR_MODEL_DISABLED:
        {
            return "INSTRUMENT_FOR_MODEL_DISABLED";
        }
        case ECN_DISABLED:
        {
            return "ECN_DISABLED";
        }
        case SELL_SIDE_DISABLED:
        {
            return "SELL_SIDE_DISABLED";
        }
        case BUY_SIDE_DISABLED:
        {
            return "BUY_SIDE_DISABLED";
        }
        case MODEL_KEY_DISABLED:
        {
            return "MODEL_KEY_DISABLED";
        }
        case MODEL_NOT_REGISTERED_FOR_ITEM:
        {
            return "MODEL_NOT_REGISTERED_FOR_ITEM";
        }
        case ORDER_SIZE_TOO_LARGE:
        {
            return "ORDER_SIZE_TOO_LARGE";
        }
        case OVER_ORDER_LIMIT:
        {
            return "OVER_ORDER_LIMIT";
        }
        case OVER_MAX_ORDER_LIMIT:
        {
            return "OVER_MAX_ORDER_LIMIT";
        }
        case SELL_MAX_POSITION_DISABLED:
        {
            return "SELL_MAX_POSITION_DISABLED";
        }
        case BUY_MAX_POSITION_DISABLED:
        {
            return "BUY_MAX_POSITION_DISABLED";
        }
        case ALREADY_PENDING_CANCEL:
        {
            return "ALREADY_PENDING_CANCEL";
        }
        case OTHER:
        {
            return "OTHER";
        }
        case UNKNOWN_ERROR:
        {
            return "UNKNOWN_ERROR";
        }
        case NO_AVAILABLE_ECN:
        {
            return "NO_AVAILABLE_ECN";
        }
        case ORDER_DOES_NOT_EXIST:
        {
            return "ORDER_DOES_NOT_EXIST";
        }
        case NOT_INITIALIZED:
        {
            return "NOT_INITIALIZED";
        }
        case NO_FREE_TRIGGERS:
        {
            return "NO_FREE_TRIGGERS";
        }
        case NOT_VALID_STATE:
        {
            return "NOT_VALID_STATE";
        }
        case ORDER_CAN_NOT_BE_MODIFIED:
        {
            return "ORDER_CAN_NOT_BE_MODIFIED";
        }
        case ORDER_SIZE_LESS_THAN_ZERO:
        {
            return "ORDER_SIZE_LESS_THAN_ZERO";
        }
        case SUCCESS:
        {
            return "SUCCESS";
        }
    }

    return "NO KNOWN ERROR";
}

void HCFXScheduler::orderUpdate(Order* order)
{
    bool bIsOrderRouterOrder = false;
    bool bIsLiquidationOrder = false;
    bool bOrderFound = false;
    unsigned int iProductIdx = 0;
    unsigned int iOrderIdx = 0;

    if(order->getOrderRouter() != nullptr)
    {
        bIsOrderRouterOrder = true;
    }

    vector<KOOrderPtr>* pOrderList;
    for(; iProductIdx < _vProductOrderList.size(); iProductIdx++)
    {
        pOrderList = &_vProductOrderList[iProductIdx];
        iOrderIdx = 0;
        for(; iOrderIdx < pOrderList->size(); iOrderIdx++)
        {
            if(bIsOrderRouterOrder == false)
            {
                if((*pOrderList)[iOrderIdx]->_pHCOrder != NULL)
                {
                    if(order->getOrderKey() == (*pOrderList)[iOrderIdx]->_pHCOrder->getOrderKey())
                    {
                        bOrderFound = true;
                        break;
                    }
                }
            }
            else
            {
                if((*pOrderList)[iOrderIdx]->_pHCOrderRouter != NULL)
                {
                    if(order->getOrderRouter()->getOrderID() == (*pOrderList)[iOrderIdx]->_pHCOrderRouter->getOrderID())
                    {
                        bOrderFound = true;
                        break;
                    }
                }
            }
        }
        if(bOrderFound == true)
        {
            break;
        }
    }

    if(bOrderFound == false)
    {
        iProductIdx = 0;
        for(; iProductIdx < _vProductLiquidationOrderList.size(); iProductIdx++)
        {
            pOrderList = &_vProductLiquidationOrderList[iProductIdx];
            iOrderIdx = 0;
            for(; iOrderIdx < pOrderList->size(); iOrderIdx++)
            {
                if(bIsOrderRouterOrder == false)
                {
                    if((*pOrderList)[iOrderIdx]->_pHCOrder != NULL)
                    {
                        if(order->getOrderKey() == (*pOrderList)[iOrderIdx]->_pHCOrder->getOrderKey())
                        {
                            bIsLiquidationOrder = true;
                            bOrderFound = true;
                            break;
                        }
                    }
                }
                else
                {
                    if((*pOrderList)[iOrderIdx]->_pHCOrderRouter != NULL)
                    {
                        if(order->getOrderRouter()->getOrderID() == (*pOrderList)[iOrderIdx]->_pHCOrderRouter->getOrderID())
                        {
                            bIsLiquidationOrder = true;
                            bOrderFound = true;
                            break;
                        }
                    }
                }
            }
            if(bOrderFound == true)
            {
                break;
            }
        }
    }

    if(bOrderFound == true)
    {
        KOOrderPtr pOrderToBeUpdated = (*pOrderList)[iOrderIdx];
        pOrderToBeUpdated->updateOrderRemainingQty();

        ORDER_STATUS eLastOrderState = order->getLastOrderState();

        if(eLastOrderState == PARTIALLY_FILLED)
        {
            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCREATION)
            {
                pOrderToBeUpdated->changeOrderstat(KOOrder::ACTIVE);
            }

            long iFillQty = order->getLastFillAmount();

            if(order->getAction() == HC_GEN::ACTION::SELL)
            {
                iFillQty = iFillQty * -1;
            }

            string sFilledECN = "Unknown";
            const char* csECN = m_ctx->getNameForECN(order->getECN());
            if(csECN != NULL)
            {
                sFilledECN = csECN;
            }    

            stringstream cStringStream;
            if(bIsLiquidationOrder == true)
            {
                cStringStream << "Liquidation order Filled - Order ID: " << pOrderToBeUpdated->igetOrderID() << " HC Order ID: " << order->getOrderKey() << " qty: " << iFillQty << " price: " << order->getLastFillRate() << " ecn: " << sFilledECN << ".";
            }
            else
            {
                cStringStream << "Order Filled - Order ID: " << pOrderToBeUpdated->igetOrderID() << " HC Order ID: " << order->getOrderKey() << " qty: " << iFillQty << " price: " << order->getLastFillRate() << " ecn: " << sFilledECN << ".";
            }
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());
            cStringStream.str("");
            cStringStream.clear();

            if(bIsLiquidationOrder == false)
            {
                _vProductPos[iProductIdx] = _vProductPos[iProductIdx] + iFillQty;
            }
            else
            {
                _vProductLiquidationPos[iProductIdx] = _vProductLiquidationPos[iProductIdx] + iFillQty;
            }

            _vProductConsideration[iProductIdx] = _vProductConsideration[iProductIdx] - (double)iFillQty * order->getLastFillRate();
            _vProductVolume[iProductIdx] = _vProductVolume[iProductIdx] + abs(iFillQty);

            // for sterling to sonia switch
            long iAdjustedFillQty = iFillQty;
            if(_vContractQuoteDatas[iProductIdx]->sProduct.substr(0, 1) == "L")
            {
                iAdjustedFillQty = iAdjustedFillQty * 2;
            }

            // for fx trading           
            string sAdjustedFillProduct = _vContractQuoteDatas[iProductIdx]->sProduct;
            if(sAdjustedFillProduct == "GBPUSD")
            {
                for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
                {
                    if(_vContractQuoteDatas[i]->sProduct.find("6B") != std::string::npos)
                    {
                        sAdjustedFillProduct = _vContractQuoteDatas[i]->sProduct;
                        break;
                    }
                }
            }
            else if(sAdjustedFillProduct == "EURUSD")
            {
                for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
                {
                    if(_vContractQuoteDatas[i]->sProduct.find("6E") != std::string::npos)
                    {
                        sAdjustedFillProduct = _vContractQuoteDatas[i]->sProduct;
                        break;
                    }
                }
            }

            _pTradeSignalMerger->onFill(sAdjustedFillProduct, iAdjustedFillQty, order->getLastFillRate(), bIsLiquidationOrder);

            if(bIsOrderRouterOrder == false)
            {
                if(order->isFinal())
                {
                    cStringStream << "Order Fully Filled - Order ID: " << pOrderToBeUpdated->igetOrderID() << ".";
                    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());

                    _vLastOrderError[iProductIdx] = "";

                    pOrderToBeUpdated->_pHCOrder = NULL;
                    pOrderToBeUpdated->changeOrderstat(KOOrder::INACTIVE);
                    pOrderList->erase(pOrderList->begin() + iOrderIdx);
                }
            }
            else
            {
                if(order->getOrderRouter()->getIsFinal())
                {
                    cStringStream << "Order Router Fully Filled - Order ID: " << pOrderToBeUpdated->igetOrderID() << ".";
                    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());

                    _vLastOrderError[iProductIdx] = "";

                    pOrderToBeUpdated->_pHCOrderRouter = NULL;
                    pOrderToBeUpdated->changeOrderstat(KOOrder::INACTIVE);
                    pOrderList->erase(pOrderList->begin() + iOrderIdx);
                }
            }
        }
        else if(eLastOrderState == ACCEPTED)
        {
           if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCREATION || pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCHANGE)
            {
                pOrderToBeUpdated->changeOrderstat(KOOrder::ACTIVE);
                stringstream cStringStream;
                cStringStream.precision(10);

                _vLastOrderError[iProductIdx] = "";

                cStringStream << "Order confirmed - Order ID: " << pOrderToBeUpdated->igetOrderID() << " HC Order ID: " << order->getOrderKey() << " price: " << order->getOrderRate() << " qty " << order->getAmountLeft() << ".";
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());
            }
            else
            {
                if(bIsOrderRouterOrder == false)
                {
                    stringstream cStringStream;
                    cStringStream.precision(10);
                    cStringStream << "Unexpected ACCEPTED received! Order Price " << order->getOrderRate()  << " Order Qty " << order->getAmountLeft() << " KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order id " << order->getOrderKey() << " order state is " << pOrderToBeUpdated->_eOrderState << ".";

                    if(_vLastOrderError[iProductIdx] != cStringStream.str())
                    {
                        _vLastOrderError[iProductIdx] = cStringStream.str();
                        ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                    }
                    ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());

                    pOrderToBeUpdated->changeOrderstat(KOOrder::ACTIVE);
                }
            }
        }
        else if(eLastOrderState == REPLACE_REJECTED)
        {
            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCHANGE)
            {
                pOrderToBeUpdated->changeOrderstat(KOOrder::ACTIVE);
                stringstream cStringStream;
                cStringStream << "Amend rejected for KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order id " << order->getOrderKey() << ". Reason: ";
                cStringStream << stranslateHCRejectReason(pOrderToBeUpdated->_pHCOrder->getFuturesRejectReason());

                if(_vLastOrderError[iProductIdx] != cStringStream.str())
                {
                    _vLastOrderError[iProductIdx] = cStringStream.str();
                    ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
            }
            else
            {
                if(bIsOrderRouterOrder == false)
                {
                    stringstream cStringStream;
                    cStringStream << "Unexpected REPLACE_REJECTED received! KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order id " << order->getOrderKey() << " order state is " << pOrderToBeUpdated->_eOrderState << ".";

                    if(_vLastOrderError[iProductIdx] != cStringStream.str())
                    {
                        _vLastOrderError[iProductIdx] = cStringStream.str();
                        ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                    }
                    ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                }
            }
        }
        else if(eLastOrderState == CANCEL_REJECTED)
        {
            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGDELETE)
            {
                pOrderToBeUpdated->changeOrderstat(KOOrder::ACTIVE);
                stringstream cStringStream;
                cStringStream << "Cancel rejected for KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order sequence number " << order->getOrderKey() << ". Reason: ";
                cStringStream << stranslateHCRejectReason(pOrderToBeUpdated->_pHCOrder->getFuturesRejectReason());

                if(_vLastOrderError[iProductIdx] != cStringStream.str())
                {
                    _vLastOrderError[iProductIdx] = cStringStream.str();
                    ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
            }
            else
            {
                if(bIsOrderRouterOrder == false)
                {
                    stringstream cStringStream;
                    cStringStream << "Unexpected CANCEL_REJECTED received! KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order id " << order->getOrderKey() << " order state is " << pOrderToBeUpdated->_eOrderState << ". Reason: ";
                    cStringStream << stranslateHCRejectReason(pOrderToBeUpdated->_pHCOrder->getFuturesRejectReason());

                    if(_vLastOrderError[iProductIdx] != cStringStream.str())
                    {
                        _vLastOrderError[iProductIdx] = cStringStream.str();
                        ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                    }
                    ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                }
            }
        }
        else if(eLastOrderState == REJECTED)
        {
            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCREATION)
            {
                stringstream cStringStream;
                cStringStream << "Submit rejected for KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order id " << order->getOrderKey() << ". Reason: ";
                cStringStream << stranslateHCRejectReason(pOrderToBeUpdated->_pHCOrder->getFuturesRejectReason());

                if(_vLastOrderError[iProductIdx] != cStringStream.str())
                {
                    if(_vLastOrderError[iProductIdx].find(stranslateHCRejectReason(pOrderToBeUpdated->_pHCOrder->getFuturesRejectReason())) == std::string::npos)
                    {
                        _vLastOrderError[iProductIdx] = cStringStream.str();
                        ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                    }
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());

                if(bIsOrderRouterOrder == false)
                {
                    pOrderToBeUpdated->_pHCOrder = NULL;
                    pOrderToBeUpdated->changeOrderstat(KOOrder::INACTIVE);
                    pOrderList->erase(pOrderList->begin() + iOrderIdx);
                }
                else
                {
                    if(order->getOrderRouter()->getIsFinal())
                    {
                        pOrderToBeUpdated->_pHCOrderRouter = NULL;
                        pOrderToBeUpdated->changeOrderstat(KOOrder::INACTIVE);
                        pOrderList->erase(pOrderList->begin() + iOrderIdx);

                        stringstream cDeleteStringStream;
                        cDeleteStringStream.precision(10);
                        cDeleteStringStream << "Order route - Order ID: " << pOrderToBeUpdated->igetOrderID() << " finalised.";
                        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cDeleteStringStream.str());
                    }
                }
            }
            else
            {
                if(bIsOrderRouterOrder == false)
                {
                    stringstream cStringStream;
                    cStringStream << "Unexpected REJECTED received! KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order id " << order->getOrderKey() << " order state is " << pOrderToBeUpdated->_eOrderState << ". Reason: ";
                    cStringStream << stranslateHCRejectReason(pOrderToBeUpdated->_pHCOrder->getFuturesRejectReason());

                    if(_vLastOrderError[iProductIdx] != cStringStream.str())
                    {
                        _vLastOrderError[iProductIdx] = cStringStream.str();
                        ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                    }
                    ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                }
                else
                {
                    if(order->getOrderRouter()->getIsFinal())
                    {
                        pOrderToBeUpdated->_pHCOrderRouter = NULL;
                        pOrderToBeUpdated->changeOrderstat(KOOrder::INACTIVE);
                        pOrderList->erase(pOrderList->begin() + iOrderIdx);

                        stringstream cDeleteStringStream;
                        cDeleteStringStream.precision(10);
                        cDeleteStringStream << "Order route - Order ID: " << pOrderToBeUpdated->igetOrderID() << " finalised.";
                        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cDeleteStringStream.str());
                    }

                }
            }
        }
        else if(eLastOrderState == CANCELLED || eLastOrderState == EXPIRED)
        {
            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGDELETE || pOrderToBeUpdated->bgetIsIOC())
            {
                stringstream cStringStream;
                cStringStream.precision(10);
                cStringStream << "Order cancel acked - Order ID: " << pOrderToBeUpdated->igetOrderID() << " HC Order ID: " << order->getOrderKey() << ".";
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());

                _vLastOrderError[iProductIdx] = "";

                if(bIsOrderRouterOrder == false)
                {
                    pOrderToBeUpdated->_pHCOrder = NULL;
                    pOrderToBeUpdated->changeOrderstat(KOOrder::INACTIVE);
                    pOrderList->erase(pOrderList->begin() + iOrderIdx);
                }
                else
                {
                    if(order->getOrderRouter()->getIsFinal())
                    {
                        pOrderToBeUpdated->_pHCOrderRouter = NULL;
                        pOrderToBeUpdated->changeOrderstat(KOOrder::INACTIVE);
                        pOrderList->erase(pOrderList->begin() + iOrderIdx);

                        stringstream cDeleteStringStream;
                        cDeleteStringStream.precision(10);
                        cDeleteStringStream << "Order route - Order ID: " << pOrderToBeUpdated->igetOrderID() << " finalised.";
                        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cDeleteStringStream.str());
                    }
                }
            }
            else
            {
                if(bIsOrderRouterOrder == false)
                {
                    stringstream cStringStream;
                    cStringStream << "Unexpected CANCELLED received! KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order id " << order->getOrderKey() << " order state is " << pOrderToBeUpdated->_eOrderState << ".";

                    if(_vLastOrderError[iProductIdx] != cStringStream.str())
                    {
                        _vLastOrderError[iProductIdx] = cStringStream.str();
                        ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                    }
                    ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());

                    pOrderToBeUpdated->_pHCOrder = NULL;
                    pOrderToBeUpdated->changeOrderstat(KOOrder::INACTIVE);
                    pOrderList->erase(pOrderList->begin() + iOrderIdx);
                }
                else
                {
                    if(order->getOrderRouter()->getIsFinal())
                    {
                        pOrderToBeUpdated->_pHCOrderRouter = NULL;
                        pOrderToBeUpdated->changeOrderstat(KOOrder::INACTIVE);
                        pOrderList->erase(pOrderList->begin() + iOrderIdx);

                        stringstream cStringStream;
                        cStringStream.precision(10);
                        cStringStream << "Order route - Order ID: " << pOrderToBeUpdated->igetOrderID() << " finalised.";
                        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());
                    }
                }
            }
        }
        else
        {
            stringstream cStringStream;
            cStringStream << "Unkown last order state " << eLastOrderState;

            if(_vLastOrderError[iProductIdx] != cStringStream.str())
            {
                _vLastOrderError[iProductIdx] = cStringStream.str();
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
            }
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "orderUpdate callback received with unknown HC order id " << order->getOrderKey() << ".";

        if(_vLastOrderError[iProductIdx] != cStringStream.str())
        {
            _vLastOrderError[iProductIdx] = cStringStream.str();
            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
        }
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

bool HCFXScheduler::sendToLiquidationExecutor(const string& sProduct, long iDesiredPos)
{
    if(sProduct.substr(0, 1) == "L")
    {
        iDesiredPos = iDesiredPos / 2;
    }

    // fx trading
    string sAdjustedProduct = sProduct;
    if(sAdjustedProduct.find("6B") != std::string::npos)
    {
        sAdjustedProduct = "GBPUSD";
    }
    else if(sAdjustedProduct.find("6E") != std::string::npos)
    {
        sAdjustedProduct = "EURUSD";
    }

    int iProductIdx = -1;

    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
    {
        if(_vContractQuoteDatas[i]->sProduct == sAdjustedProduct)
        {
            iProductIdx = i;
            break;
        }
    }

    if(iProductIdx != -1)
    {
        long iAdjustedDesiredPos = iDesiredPos;

        int iPosDelta = iDesiredPos - _vProductLiquidationPos[iProductIdx];
        if(iPosDelta > _vContractQuoteDatas[iProductIdx]->iProductExpoLimit)
        {
            iAdjustedDesiredPos = _vContractQuoteDatas[iProductIdx]->iProductExpoLimit + _vProductLiquidationPos[iProductIdx];
            iPosDelta = _vContractQuoteDatas[iProductIdx]->iProductExpoLimit;
        }
        else if(iPosDelta < -1 * _vContractQuoteDatas[iProductIdx]->iProductExpoLimit)
        {
            iAdjustedDesiredPos = -1 * _vContractQuoteDatas[iProductIdx]->iProductExpoLimit + _vProductLiquidationPos[iProductIdx];
            iPosDelta = -1 * _vContractQuoteDatas[iProductIdx]->iProductExpoLimit;
        }

        _vProductLiquidationDesiredPos[iProductIdx] = iAdjustedDesiredPos;

        vector<KOOrderPtr>* pOrderList = &_vProductLiquidationOrderList[iProductIdx];
        bool bAllOrderConfirmed = true;
        for(unsigned int i = 0; i < pOrderList->size(); i++)
        {
            bAllOrderConfirmed = bAllOrderConfirmed && (*pOrderList)[i]->borderCanBeChanged();
        }

        if(bAllOrderConfirmed)
        {
            int iOpenOrderQty = 0;

            for(unsigned int i = 0; i < pOrderList->size(); i++)
            {
                if(iPosDelta * (*pOrderList)[i]->igetOrderRemainQty() > 0)
                {
                    int iUnallocatedQty = iPosDelta - iOpenOrderQty;
                    if(iUnallocatedQty == 0)
                    {
                        deleteOrder((*pOrderList)[i]);
                    }
                    else if(abs(iUnallocatedQty) < abs((*pOrderList)[i]->igetOrderRemainQty()))
                    {
                        long iOrderPriceInTicks = icalcualteOrderPrice(iProductIdx, (*pOrderList)[i]->igetOrderPriceInTicks(), (*pOrderList)[i]->igetOrderRemainQty(), true, true, false);
                        amendOrder((*pOrderList)[i], iUnallocatedQty, iOrderPriceInTicks);
                        iOpenOrderQty = iOpenOrderQty + iUnallocatedQty;
                    }
                    else
                    {
                        iOpenOrderQty = iOpenOrderQty + (*pOrderList)[i]->igetOrderRemainQty();
                    }
                }
                else
                {
                    deleteOrder((*pOrderList)[i]);
                }
            }

            if(iPosDelta - iOpenOrderQty != 0)
            {
                submitOrderBestPrice(iProductIdx, iPosDelta - iOpenOrderQty, true);
            }
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Unrecognised product " << sAdjustedProduct << " received in liquidation executor.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    return true;
}

void HCFXScheduler::assignPositionToLiquidator(const string& sProduct, long iPosToLiquidate)
{
    if(sProduct.substr(0, 1) == "L")
    {
        iPosToLiquidate = iPosToLiquidate / 2;
    }

    // fx trading
    string sAdjustedProduct = sProduct;
    if(sAdjustedProduct.find("6B") != std::string::npos)
    {
        sAdjustedProduct = "GBPUSD";
    }
    else if(sAdjustedProduct.find("6E") != std::string::npos)
    {
        sAdjustedProduct = "EURUSD";
    }

    unsigned int iProductIdx = -1;

    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
    {
        if(_vContractQuoteDatas[i]->sProduct == sAdjustedProduct)
        {
            iProductIdx = i;
            break;
        }
    }

    long iLiquidatorPos = _vProductLiquidationPos[iProductIdx];
    if(iLiquidatorPos != iPosToLiquidate)
    {
        long iPosDelta = iPosToLiquidate - iLiquidatorPos;
        if(iPosDelta != 0)
        {
            _vProductLiquidationPos[iProductIdx] = _vProductLiquidationPos[iProductIdx] + iPosDelta;
            _vProductPos[iProductIdx] = _vProductPos[iProductIdx] - iPosDelta;

            stringstream cStringStream;
            cStringStream << "Transfered " << iPosDelta << " lots to fast liquidator";
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sAdjustedProduct, cStringStream.str());
        }
    }
}

void HCFXScheduler::checkOrderState(KOEpochTime cCurrentTime)
{
    vector<KOOrderPtr>* pOrderList;
    for(unsigned int iProductIdx = 0; iProductIdx < _vProductOrderList.size(); iProductIdx++)
    {
        pOrderList = &_vProductOrderList[iProductIdx];
        for(unsigned int iOrderIdx = 0; iOrderIdx < pOrderList->size(); iOrderIdx++)
        {
            if((*pOrderList)[iOrderIdx]->borderCanBeChanged() == false)
            {
                if((cCurrentTime - (*pOrderList)[iOrderIdx]->_cPendingRequestTime).sec() > 10)
                {
                    if((*pOrderList)[iOrderIdx]->_bOrderNoReplyTriggered == false)
                    {
                        stringstream cStringStream;
                        if((*pOrderList)[iOrderIdx]->_pHCOrderRouter != NULL)
                        {
                            cStringStream << "Order " << (*pOrderList)[iOrderIdx]->_iOrderID << " HC Order ID " << (*pOrderList)[iOrderIdx]->_pHCOrderRouter->getOrderID()  << " held by server for more thatn 10 seconds. Contact trade support. ";
                        }
                        else
                        {
                            cStringStream << "Order " << (*pOrderList)[iOrderIdx]->_iOrderID << " HC Order ID " << (*pOrderList)[iOrderIdx]->_pHCOrder->getOrderKey()  << " held by server for more thatn 10 seconds. Contact trade support. ";
                        }
                        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", (*pOrderList)[iOrderIdx]->_sProduct, cStringStream.str());
                        (*pOrderList)[iOrderIdx]->_bOrderNoReplyTriggered = true;
                    }
                }
            }

            if((*pOrderList)[iOrderIdx]->_pHCOrderRouter == NULL)
            {
                if((*pOrderList)[iOrderIdx]->borderCanBeChanged())
                {
                    if((*pOrderList)[iOrderIdx]->igetOrderRemainQty() > 0)
                    {
                        if((*pOrderList)[iOrderIdx]->_iOrderPriceInTicks > _vContractQuoteDatas[(*pOrderList)[iOrderIdx]->_iCID]->iBestAskInTicks)
                        {
                            if((*pOrderList)[iOrderIdx]->_bOrderNoFill == false)
                            {
                                (*pOrderList)[iOrderIdx]->_bOrderNoFill = true;
                                (*pOrderList)[iOrderIdx]->_cOrderNoFillTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
                            }
                            else
                            {
                                if((SystemClock::GetInstance()->cgetCurrentKOEpochTime() - (*pOrderList)[iOrderIdx]->_cOrderNoFillTime).sec() > 2)
                                {
                                    if((*pOrderList)[iOrderIdx]->_bOrderNoFillTriggered == false)
                                    {
                                        (*pOrderList)[iOrderIdx]->_bOrderNoFillTriggered = true;

                                        stringstream cStringStream;
                                        cStringStream << "Order " << (*pOrderList)[iOrderIdx]->_iOrderID << "HC Order ID " << (*pOrderList)[iOrderIdx]->_pHCOrder->getOrderKey() << " market ask price " << _vContractQuoteDatas[(*pOrderList)[iOrderIdx]->_iCID]->dBestAsk << " smaller than buy order price " << (*pOrderList)[iOrderIdx]->dgetOrderPrice() << " for more than 2 seconds.";
                                        ErrorHandler::GetInstance()->newErrorMsg("0", (*pOrderList)[iOrderIdx]->_sProduct, (*pOrderList)[iOrderIdx]->_sProduct, cStringStream.str());
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        if((*pOrderList)[iOrderIdx]->_iOrderPriceInTicks < _vContractQuoteDatas[(*pOrderList)[iOrderIdx]->_iCID]->iBestBidInTicks)
                        {
                            if((*pOrderList)[iOrderIdx]->_bOrderNoFill == false)
                            {
                                (*pOrderList)[iOrderIdx]->_bOrderNoFill = true;
                                (*pOrderList)[iOrderIdx]->_cOrderNoFillTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
                            }
                            else
                            {
                                if((SystemClock::GetInstance()->cgetCurrentKOEpochTime() - (*pOrderList)[iOrderIdx]->_cOrderNoFillTime).sec() > 2)
                                {
                                    if((*pOrderList)[iOrderIdx]->_bOrderNoFillTriggered == false)
                                    {
                                        (*pOrderList)[iOrderIdx]->_bOrderNoFillTriggered = true;

                                        stringstream cStringStream;
                                        cStringStream << "Order " << (*pOrderList)[iOrderIdx]->_iOrderID << "HC Order ID " << (*pOrderList)[iOrderIdx]->_pHCOrder->getOrderKey() << " market bid price " << _vContractQuoteDatas[(*pOrderList)[iOrderIdx]->_iCID]->dBestBid << " bigger than sell order price " << (*pOrderList)[iOrderIdx]->dgetOrderPrice() << " for more than 2 seconds.";
                                        ErrorHandler::GetInstance()->newErrorMsg("0", (*pOrderList)[iOrderIdx]->_sProduct, (*pOrderList)[iOrderIdx]->_sProduct, cStringStream.str());
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    for(unsigned int iProductIdx = 0; iProductIdx < _vProductLiquidationOrderList.size(); iProductIdx++)
    {
        pOrderList = &_vProductLiquidationOrderList[iProductIdx];
        for(unsigned int iOrderIdx = 0; iOrderIdx < pOrderList->size(); iOrderIdx++)
        {
            if((*pOrderList)[iOrderIdx]->borderCanBeChanged() == false)
            {
                if(((*pOrderList)[iOrderIdx]->_cPendingRequestTime - cCurrentTime).sec() > 10)
                {
                    if((*pOrderList)[iOrderIdx]->_bOrderNoReplyTriggered == false)
                    {
                        stringstream cStringStream;
                        if((*pOrderList)[iOrderIdx]->_pHCOrderRouter != NULL)
                        {
                            cStringStream << "Order " << (*pOrderList)[iOrderIdx]->_iOrderID << " HC Order ID " << (*pOrderList)[iOrderIdx]->_pHCOrderRouter->getOrderID()  << " held by server for more thatn 10 seconds. Contact trade support. ";
                        }
                        else
                        {
                            cStringStream << "Order " << (*pOrderList)[iOrderIdx]->_iOrderID << " HC Order ID " << (*pOrderList)[iOrderIdx]->_pHCOrder->getOrderKey()  << " held by server for more thatn 10 seconds. Contact trade support. ";
                        }
                        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", (*pOrderList)[iOrderIdx]->_sProduct, cStringStream.str());
                        (*pOrderList)[iOrderIdx]->_bOrderNoReplyTriggered = true;
                    }
                }
            }
        }
    }
}

void HCFXScheduler::resetOrderState()
{
    for(unsigned int iProductIdx = 0; iProductIdx < _vProductOrderList.size(); iProductIdx++)
    {
        vector<KOOrderPtr>* pOrderList = &_vProductOrderList[iProductIdx];
        for(vector<KOOrderPtr>::iterator itr = pOrderList->begin(); itr != pOrderList->end();)
        {
            if((*itr)->_bOrderNoReplyTriggered == true)
            {
                stringstream cStringStream;
                cStringStream << "Removing stuck order " << (*itr)->_iOrderID << " HC Order ID " << (*itr)->_pHCOrder->getOrderKey()  << ".";
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", (*itr)->_sProduct, cStringStream.str());

                (*itr)->_pHCOrder = NULL;
                (*itr)->changeOrderstat(KOOrder::INACTIVE);
                itr = pOrderList->erase(itr);
            }
            else
            {
                itr++;
            }
        }
    }

    for(unsigned int iProductIdx = 0; iProductIdx < _vProductLiquidationOrderList.size(); iProductIdx++)
    {
        vector<KOOrderPtr>* pOrderList = &_vProductLiquidationOrderList[iProductIdx];
        for(vector<KOOrderPtr>::iterator itr = pOrderList->begin(); itr != pOrderList->end();)
        {
            if((*itr)->_bOrderNoReplyTriggered == true)
            {
                stringstream cStringStream;
                cStringStream << "Removing stuck order " << (*itr)->_iOrderID << " HC Order ID " << (*itr)->_pHCOrder->getOrderKey()  << ".";
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", (*itr)->_sProduct, cStringStream.str());

                (*itr)->_pHCOrder = NULL;
                (*itr)->changeOrderstat(KOOrder::INACTIVE);
                itr = pOrderList->erase(itr);
            }
            else
            {
                itr++;
            }
        }
    }
}

void HCFXScheduler::updateAllPnL()
{
    for(unsigned int i = 0;i < _vContractQuoteDatas.size();i++)
    {
        bool bPrintProductPNL = false;
        if(_bIsLiveTrading == true)
        {
            if(_vContractQuoteDatas[i]->bIsLocalProduct == true)
            {
                bPrintProductPNL = true;
            }
        }
        else
        {
            bPrintProductPNL = true;
        }

        if(bPrintProductPNL == true)
        {
            updateProductPnL(i);
        }
    }
}

bool HCFXScheduler::bcheckRisk(unsigned int iProductIdx, long iNewQty)
{
    int iTotalProductPos = _vProductPos[iProductIdx] + _vProductLiquidationPos[iProductIdx];
    int iTotalOpenOrderQty = 0;

    vector<KOOrderPtr>* pOrderList = &_vProductOrderList[iProductIdx];
    for(unsigned int iOrderIdx = 0; iOrderIdx < pOrderList->size(); iOrderIdx++)
    {
        iTotalOpenOrderQty = iTotalOpenOrderQty + (*pOrderList)[iOrderIdx]->igetOrderRemainQty();
    }
   
    pOrderList = &_vProductLiquidationOrderList[iProductIdx];
    for(unsigned int iOrderIdx = 0; iOrderIdx < pOrderList->size(); iOrderIdx++)
    {
        iTotalOpenOrderQty = iTotalOpenOrderQty + (*pOrderList)[iOrderIdx]->igetOrderRemainQty();
    }
 
    if(abs(iTotalProductPos + iTotalOpenOrderQty + iNewQty) > _vContractQuoteDatas[iProductIdx]->iProductMaxRisk)
    {
        stringstream cStringStream;
        cStringStream << "Failed to pass risk check " << _vContractQuoteDatas[iProductIdx]->sProduct << " position: " << iTotalProductPos << " open order qty: " << iTotalOpenOrderQty << " new order size: " << iNewQty << " limit: " << _vContractQuoteDatas[iProductIdx]->iProductMaxRisk;
         
        if(_vLastOrderError[iProductIdx] != cStringStream.str())
        {
            _vLastOrderError[iProductIdx] = cStringStream.str();
            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());
        }
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());

        return false;
    }
    else
    {
        return true;
    }
}

bool HCFXScheduler::bcheckOrderMsgHistory(KOOrderPtr pOrder)
{
    bool bResult = false;
    while(pOrder->_qOrderMessageHistory.size() != 0 && pOrder->_qOrderMessageHistory.front() <= SystemClock::GetInstance()->cgetCurrentKOEpochTime() - KOEpochTime(1,0))
    {
        pOrder->_qOrderMessageHistory.pop_front();
    }

    unsigned long iMsgLimit = 20;    
    if(pOrder->_qOrderMessageHistory.size() >= iMsgLimit)
    {
        bResult = false;
        stringstream cStringStream;
        cStringStream << "Order message limit of " << iMsgLimit << " reached";
        ErrorHandler::GetInstance()->newWarningMsg("0", "ALL", pOrder->sgetOrderProductName(), cStringStream.str());
    }
    else
    {
        bResult = true;
        pOrder->_qOrderMessageHistory.push_back(SystemClock::GetInstance()->cgetCurrentKOEpochTime());
    }

    return bResult;
}

void HCFXScheduler::exitScheduler()
{
    _pTradeSignalMerger->writeEoDResult(_cSchedulerCfg.sLogPath, _cSchedulerCfg.sDate);

    fstream fsOrderMsgFile;
    fsOrderMsgFile.open(_cSchedulerCfg.sLogPath + "/OrderMsg.out", fstream::out);
    if(fsOrderMsgFile.is_open())
    {
        fsOrderMsgFile << "Total order messages: " << _iTotalNumMsg;
        fsOrderMsgFile.close();
    }

    exit(0);
}

}

