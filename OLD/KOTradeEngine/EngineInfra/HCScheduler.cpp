#include <fstream>
#include <signal.h>

#include "HCScheduler.h"
#include "TradeEngineBase.h"
#include "ErrorHandler.h"
#include "SimPositionServer.h"
#include "SystemClock.h"

#include <framework/Book.h>
#include <msg/message_enum.h>

#include <boost/math/special_functions/round.hpp>

#include <core/IVelioSession.h>

using std::stringstream;
using std::cerr;

namespace KO
{

HCScheduler::HCScheduler(SchedulerConfig &cfg, VelioSessionManager* pVelioSessionManager, bool bIsLiveTrading)
:SchedulerBase(cfg),
 _pVelioSessionManager(pVelioSessionManager)
{
    _bSchedulerInitialised = false;
    _bHCTimerInitialised = false;
    _bIsLiveTrading = bIsLiveTrading;

    preCommonInit();
}

HCScheduler::~HCScheduler()
{

}

void HCScheduler::KOInit()
{
    postCommonInit();
}

void HCScheduler::addInstrument(string sKOProductName, string sHCProductName, InstrumentType eInstrumentType)
{
    QuoteData* pNewQuoteDataPtr = pregisterProduct(sKOProductName, eInstrumentType);
    pNewQuoteDataPtr->sHCProduct = sHCProductName;
    pNewQuoteDataPtr->sHCExchange = _pStaticDataHandler->sGetHCExchange(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange);
}

void HCScheduler::initialize ()
{
    if(!_bSchedulerInitialised)
    {
        _bSchedulerInitialised = true;

        _sModelName = m_ctx->getModelConfig(m_ctx->getModelKey())->name;

        ErrorHandler::GetInstance()->newInfoMsg("0", "", "", "Model Name is " + _sModelName);

        for(int i = 0;i < _vContractQuoteDatas.size();i++)
        {
            string sMarket = _vContractQuoteDatas[i]->sHCExchange;
            string sHCProduct = _vContractQuoteDatas[i]->sHCProduct;
            InstrumentType eInstrumentType = _vContractQuoteDatas[i]->eInstrumentType;

            // TODO error checking for return value -1 here

            short iGatewayID = m_ctx->getSourcePriceKey(sMarket.c_str());
            _vContractQuoteDatas[i]->iGatewayID = iGatewayID;

            if(eInstrumentType == KO_FUTURE)
            {
                int16_t m_ecn = m_ctx->getEcnKey(sMarket.c_str());
                int64_t iProductKey = m_ctx->getFuturesKey(m_ecn, sHCProduct.c_str());
                int64_t iSecurityID = m_ctx->getFuturesSecurityID(iProductKey);
 
                _vContractQuoteDatas[i]->iCID = iProductKey;
                _vContractQuoteDatas[i]->iSecurityID = iSecurityID;
            }
            else if(eInstrumentType == KO_FX)
            {

                int64_t iProductKey = m_ctx->getCcyKey(sHCProduct.c_str());
                _vContractQuoteDatas[i]->iCID = iProductKey;
            }
        }
            
        m_ctx->addTimerEvent(&updateTimer, this, 1000, true);
    }
}

void HCScheduler::bookUpdate(Book * pBook)
{
	for(unsigned int i = 0; i < _vContractQuoteDatas.size(); ++i)
	{
		if(_vContractQuoteDatas[i]->iCID == pBook->getInstrument())
		{
            _vContractQuoteDatas[i]->iTradeSize = 0;

			PRICE_ENTRY cBidEntry;
			PRICE_ENTRY cAskEntry;

			pBook->getBestBid(cBidEntry);
			pBook->getBestAsk(cAskEntry);

			_vContractQuoteDatas[i]->dBestBid = cBidEntry.price;
			_vContractQuoteDatas[i]->dBestAsk = cAskEntry.price;

            _vContractQuoteDatas[i]->iBestBidInTicks = boost::math::iround(_vContractQuoteDatas[i]->dBestBid / _vContractQuoteDatas[i]->dTickSize);
            _vContractQuoteDatas[i]->iBestAskInTicks = boost::math::iround(_vContractQuoteDatas[i]->dBestAsk / _vContractQuoteDatas[i]->dTickSize);

            const std::multiset<PRICE_ENTRY*, bool(*)(PRICE_ENTRY*, PRICE_ENTRY*)>* bookOrders;
            std::multiset<PRICE_ENTRY*, bool(*)(PRICE_ENTRY*, PRICE_ENTRY*)>::iterator itr;

            long iBidContracts = 0;
            bookOrders = pBook->getSortedBids();
            for(itr = bookOrders->begin(); itr != bookOrders->end(); itr++)
            {
                PRICE_ENTRY* entry = *itr ;
                if(entry)
                {
                    if((_vContractQuoteDatas[i]->dBestBid - entry->price) > 0.000001)
                    {
                        break ;
                    }
                    else
                    {
                        iBidContracts += entry->maxAmount;
                    }
                }
            }

            long iAskContracts = 0;
            bookOrders = pBook->getSortedAsks();
            for (itr = bookOrders->begin(); itr != bookOrders->end( ); itr++)
            {
                PRICE_ENTRY* entry = *itr;
                if(entry)
                {
                    if((entry->price - _vContractQuoteDatas[i]->dBestAsk) > 0.000001)
                    {
                        break ;
                    }
                    else
                    {
                        iAskContracts += entry->maxAmount;
                    }
                }
            }

			_vContractQuoteDatas[i]->iBidSize = iBidContracts;
 			_vContractQuoteDatas[i]->iAskSize = iAskContracts;
            newPriceUpdate(i);
		}
	}
}

void HCScheduler::futureTickerUpdate(FUTURES_TICKER_ENTRY* ticker)
{
	for(unsigned int i = 0; i < _vContractQuoteDatas.size(); ++i)
	{
		if(_vContractQuoteDatas[i]->iCID == ticker->instrumentKey)
		{
			_vContractQuoteDatas[i]->iTradeSize = ticker->nContracts;
			_vContractQuoteDatas[i]->dLastTradePrice = ticker->price;
            _vContractQuoteDatas[i]->iLastTradeInTicks = boost::math::iround(_vContractQuoteDatas[i]->dLastTradePrice / _vContractQuoteDatas[i]->dTickSize);
            _vContractQuoteDatas[i]->iAccumuTradeSize = _vContractQuoteDatas[i]->iAccumuTradeSize + _vContractQuoteDatas[i]->iTradeSize;
            newPriceUpdate(i);
		}
	}
}

bool HCScheduler::bsubmitOrder(KOOrderPtr pOrder, double dPrice, long iQty)
{
    if(m_orderManager->isInstrumentEnabled(pOrder->_iCID))
    {
        if(pOrder->egetInstrumentType() == KO_FUTURE)
        {
            pOrder->_pHCOrder = m_orderManager->createOrder(FUTURES, pOrder->_iGatewayID, pOrder->_iCID, pOrder->_iSecurityID); 
        }
        else
        {
            pOrder->_pHCOrder = m_orderManager->createOrder(FX, pOrder->_iGatewayID, pOrder->_iCID, pOrder->_iSecurityID); 
        }

        pOrder->_pHCOrder->setIFM(true);
        pOrder->_pHCOrder->setOrderRate(dPrice);

        if(iQty > 0)
        {
            pOrder->_pHCOrder->setAction(BUY);
        }
        else
        {
            pOrder->_pHCOrder->setAction(SELL);
        }

        pOrder->_pHCOrder->setOrderType(FIX_ORDER_TYPE::LIMIT);
        
        if(pOrder->_bIsIOC)
        {
            pOrder->_pHCOrder->setTimeInForce(FIX_TIME_IN_FORCE::IOC);
        }
        else
        {
            pOrder->_pHCOrder->setTimeInForce(FIX_TIME_IN_FORCE::DAY);
        }

        pOrder->_pHCOrder->setAmount(abs(iQty));

        if(pOrder->sgetOrderExchange() == "XLIF")
        {
            if(_sModelName == "levelup_aur_01")
            {
                pOrder->_pHCOrder->setAlgoFlag(92);
            }
            else if(_sModelName == "levelup_aur_02")
            {
                pOrder->_pHCOrder->setAlgoFlag(93);
            }
            else if(_sModelName == "levelup_aur_03")
            {
                pOrder->_pHCOrder->setAlgoFlag(94);
            }
            else if(_sModelName == "levelup_aur_04")
            {
                pOrder->_pHCOrder->setAlgoFlag(95);
            }
            else if(_sModelName == "levelup_aur_06")
            {
                pOrder->_pHCOrder->setAlgoFlag(84);
            }
            else if(_sModelName == "levelup_fra_01")
            {
                pOrder->_pHCOrder->setAlgoFlag(87);
            }
            else if(_sModelName == "levelup_fra_02")
            {
                pOrder->_pHCOrder->setAlgoFlag(88);
            }
            else if(_sModelName == "levelup_fra_03")
            {
                pOrder->_pHCOrder->setAlgoFlag(89);
            }
            else if(_sModelName == "levelup_fra_04")
            {
                pOrder->_pHCOrder->setAlgoFlag(90);
            }
            else if(_sModelName == "levelup_fra_05")
            {
                pOrder->_pHCOrder->setAlgoFlag(85);
            }
        }
        else if(pOrder->sgetOrderExchange() == "XEUR")
        {
            pOrder->_pHCOrder->setAlgoFlag(331);
        }


        ORDER_SEND_ERROR cSendOrderResult = m_orderManager->sendOrder(pOrder->_pHCOrder, this);

        if(cSendOrderResult == SUCCESS)
        {
            pOrder->changeOrderstat(KOOrder::PENDINGCREATION);
            _mInternalOrdersMap[pOrder->_pHCOrder->getOrderKey()] = pOrder;

            if(!_bIsLiveTrading)
            {
                _cOrderActionLogger << m_ctx->getTime(CURRENT_TIME) << "|REQUEST|SUBMIT|KOID:" << pOrder->igetOrderID() << "|Product:" << pOrder->sgetOrderProductName() << "|Price:" << dPrice << "|Qty:" << iQty << "\n";
            }

            return true;
        }
        else
        {
            pOrder->_pHCOrder = 0;
            stringstream cStringStream;
            cStringStream << "Failed to submit new order for KOOrder ID " << pOrder->igetOrderID() << "." << " Reason:";

            switch(cSendOrderResult)
            {
                case GLOBAL_TRADING_DISABLED:
                {
                    cStringStream << " GLOBAL_TRADING_DISABLED.";
                    break;
                }
                case NODE_DISABLED:
                {
                    cStringStream << " NODE_DISABLED.";
                    break;
                }
                case INSTRUMENT_DISABLED:
                {
                    cStringStream << " INSTRUMENT_DISABLED.";
                    break;
                }
                case INSTRUMENT_FOR_MODEL_DISABLED:
                {
                    cStringStream << " INSTRUMENT_FOR_MODEL_DISABLED.";
                    break;
                }
                case ECN_DISABLED:
                {
                    cStringStream << " ECN_DISABLED.";
                    break;
                }
                case SELL_SIDE_DISABLED:
                {
                    cStringStream << " SELL_SIDE_DISABLED.";
                    break;
                }
                case BUY_SIDE_DISABLED:
                {
                    cStringStream << " BUY_SIDE_DISABLED.";
                    break;
                }
                case MODEL_KEY_DISABLED:
                {
                    cStringStream << " MODEL_KEY_DISABLED.";
                    break;
                }
                case MODEL_NOT_REGISTERED_FOR_ITEM:
                {
                    cStringStream << " MODEL_NOT_REGISTERED_FOR_ITEM.";
                    break;
                }

                case ORDER_SIZE_TOO_LARGE:
                {
                    cStringStream << " ORDER_SIZE_TOO_LARGE.";
                    break;
                }
                case OVER_ORDER_LIMIT:
                {
                    cStringStream << " OVER_ORDER_LIMIT.";
                    break;
                }
                case OVER_MAX_ORDER_LIMIT:
                {
                    cStringStream << " OVER_MAX_ORDER_LIMIT.";
                    break;
                }
                case SELL_MAX_POSITION_DISABLED:
                {
                    cStringStream << " SELL_MAX_POSITION_DISABLED.";
                    break;
                }
                case BUY_MAX_POSITION_DISABLED:
                {
                    cStringStream << " BUY_MAX_POSITION_DISABLED.";
                    break;
                }
                case OTHER:
                {
                    cStringStream << " OTHER.";
                    break;
                }
                case UNKNOWN_ERROR:
                {
                    cStringStream << " UNKNOWN_ERROR.";
                    break;
                }         
            }

            if(!_bIsLiveTrading)
            {
                _cOrderActionLogger << m_ctx->getTime(CURRENT_TIME) << "|REPLY|SUBMIT_REJECT|KOID:" << pOrder->igetOrderID() << "|Reason:" << cStringStream.str();
            }

            ErrorHandler::GetInstance()->newWarningMsg("3.4", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());

            return false;
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Failed to submit order! Product " << pOrder->_sProduct << " disabled for trading.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());

        return false;
    }
}

bool HCScheduler::bchangeOrderPrice(KOOrderPtr pOrder, double dNewPrice)
{
    pOrder->_pHCOrder->setReplaceAmount(pOrder->_pHCOrder->getAmount());
    pOrder->_pHCOrder->setReplaceRate(dNewPrice);

    ORDER_SEND_ERROR cSendOrderResult = m_orderManager->sendOrderReplace(pOrder->_pHCOrder);  

    if(cSendOrderResult == SUCCESS)
    {
        pOrder->changeOrderstat(KOOrder::PENDINGCHANGE); 

        if(!_bIsLiveTrading)
        {
            _cOrderActionLogger << m_ctx->getTime(CURRENT_TIME) << "|REQUEST|CHANGE_PRICE|KOID:" << pOrder->igetOrderID() << "|HCID:" << pOrder->_pHCOrder->getOrderKey() << "|NewPrice:" << dNewPrice << "\n";
       }

        return true;
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Failed to submit order amendment for KOOrder ID " << pOrder->igetOrderID() << " HCID " << pOrder->_pHCOrder->getOrderKey() << "." << " Reason:";

        switch(cSendOrderResult)
        {
            case GLOBAL_TRADING_DISABLED:
            {
                cStringStream << " GLOBAL_TRADING_DISABLED.";
                break;
            }
            case NODE_DISABLED:
            {
                cStringStream << " NODE_DISABLED.";
                break;
            }
            case INSTRUMENT_DISABLED:
            {
                cStringStream << " INSTRUMENT_DISABLED.";
                break;
            }
            case INSTRUMENT_FOR_MODEL_DISABLED:
            {
                cStringStream << " INSTRUMENT_FOR_MODEL_DISABLED.";
                break;
            }
            case ECN_DISABLED:
            {
                cStringStream << " ECN_DISABLED.";
                break;
            }
            case SELL_SIDE_DISABLED:
            {
                cStringStream << " SELL_SIDE_DISABLED.";
                break;
            }
            case BUY_SIDE_DISABLED:
            {
                cStringStream << " BUY_SIDE_DISABLED.";
                break;
            }
            case MODEL_KEY_DISABLED:
            {
                cStringStream << " MODEL_KEY_DISABLED.";
                break;
            }
            case MODEL_NOT_REGISTERED_FOR_ITEM:
            {
                cStringStream << " MODEL_NOT_REGISTERED_FOR_ITEM.";
                break;
            }

            case ORDER_SIZE_TOO_LARGE:
            {
                cStringStream << " ORDER_SIZE_TOO_LARGE.";
                break;
            }
            case OVER_ORDER_LIMIT:
            {
                cStringStream << " OVER_ORDER_LIMIT.";
                break;
            }
            case OVER_MAX_ORDER_LIMIT:
            {
                cStringStream << " OVER_MAX_ORDER_LIMIT.";
                break;
            }
            case SELL_MAX_POSITION_DISABLED:
            {
                cStringStream << " SELL_MAX_POSITION_DISABLED.";
                break;
            }
            case BUY_MAX_POSITION_DISABLED:
            {
                cStringStream << " BUY_MAX_POSITION_DISABLED.";
                break;
            }
            case OTHER:
            {
                cStringStream << " OTHER.";
                break;
            }
            case UNKNOWN_ERROR:
            {
                cStringStream << " UNKNOWN_ERROR.";
                break;
            }         

        }
            
        ErrorHandler::GetInstance()->newWarningMsg("3.4", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());

        return false;
    }
}

bool HCScheduler::bchangeOrder(KOOrderPtr pOrder, double dNewPrice, long iNewQty)
{
    long iqtyDelta = abs(iNewQty) - pOrder->_pHCOrder->getAmountLeft();

    pOrder->_pHCOrder->setReplaceRate(dNewPrice);
    pOrder->_pHCOrder->setReplaceAmount(pOrder->_pHCOrder->getAmount() + iqtyDelta);

    ORDER_SEND_ERROR cSendOrderResult = m_orderManager->sendOrderReplace(pOrder->_pHCOrder);  

    if(cSendOrderResult == SUCCESS)
    { 
        pOrder->changeOrderstat(KOOrder::PENDINGCHANGE); 

        if(!_bIsLiveTrading)
        {
            _cOrderActionLogger << m_ctx->getTime(CURRENT_TIME) << "|REQUEST|CHANGE|KOID:" << pOrder->igetOrderID() << "|HCID:" << pOrder->_pHCOrder->getOrderKey() << "|NewPrice:" << dNewPrice << "|NewQty:" << iNewQty << "\n";
        }

        return true;
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Failed to submit order amendment for KOOrder ID " << pOrder->igetOrderID() << " HCID " << pOrder->_pHCOrder->getOrderKey() << "." << " Reason:";

        switch(cSendOrderResult)
        {
            case GLOBAL_TRADING_DISABLED:
            {
                cStringStream << " GLOBAL_TRADING_DISABLED.";
                break;
            }
            case NODE_DISABLED:
            {
                cStringStream << " NODE_DISABLED.";
                break;
            }
            case INSTRUMENT_DISABLED:
            {
                cStringStream << " INSTRUMENT_DISABLED.";
                break;
            }
            case INSTRUMENT_FOR_MODEL_DISABLED:
            {
                cStringStream << " INSTRUMENT_FOR_MODEL_DISABLED.";
                break;
            }
            case ECN_DISABLED:
            {
                cStringStream << " ECN_DISABLED.";
                break;
            }
            case SELL_SIDE_DISABLED:
            {
                cStringStream << " SELL_SIDE_DISABLED.";
                break;
            }
            case BUY_SIDE_DISABLED:
            {
                cStringStream << " BUY_SIDE_DISABLED.";
                break;
            }
            case MODEL_KEY_DISABLED:
            {
                cStringStream << " MODEL_KEY_DISABLED.";
                break;
            }
            case MODEL_NOT_REGISTERED_FOR_ITEM:
            {
                cStringStream << " MODEL_NOT_REGISTERED_FOR_ITEM.";
                break;
            }

            case ORDER_SIZE_TOO_LARGE:
            {
                cStringStream << " ORDER_SIZE_TOO_LARGE.";
                break;
            }
            case OVER_ORDER_LIMIT:
            {
                cStringStream << " OVER_ORDER_LIMIT.";
                break;
            }
            case OVER_MAX_ORDER_LIMIT:
            {
                cStringStream << " OVER_MAX_ORDER_LIMIT.";
                break;
            }
            case SELL_MAX_POSITION_DISABLED:
            {
                cStringStream << " SELL_MAX_POSITION_DISABLED.";
                break;
            }
            case BUY_MAX_POSITION_DISABLED:
            {
                cStringStream << " BUY_MAX_POSITION_DISABLED.";
                break;
            }
            case OTHER:
            {
                cStringStream << " OTHER.";
                break;
            }
            case UNKNOWN_ERROR:
            {
                cStringStream << " UNKNOWN_ERROR.";
                break;
            }         

        }
            
        ErrorHandler::GetInstance()->newWarningMsg("3.4", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());

        return false;
    }

}

bool HCScheduler::bdeleteOrder(KOOrderPtr pOrder)
{
    ORDER_SEND_ERROR cSendOrderResult = m_orderManager->sendOrderCancel(pOrder->_pHCOrder);

    if(cSendOrderResult == SUCCESS)
    {
        pOrder->changeOrderstat(KOOrder::PENDINGDELETE);
   
        if(!_bIsLiveTrading)
        { 
            _cOrderActionLogger << m_ctx->getTime(CURRENT_TIME) << "|REQUEST|DELETE|KOID:" << pOrder->igetOrderID() << "|HCID:" << pOrder->_pHCOrder->getOrderKey() << "\n";
        }

        return true;
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Failed to submit order delete for KOOrder ID " << pOrder->igetOrderID() << " HCID " << pOrder->_pHCOrder->getOrderKey() << "." << " Reason:";

        switch(cSendOrderResult)
        {
            case GLOBAL_TRADING_DISABLED:
            {
                cStringStream << " GLOBAL_TRADING_DISABLED.";
                break;
            }
            case NODE_DISABLED:
            {
                cStringStream << " NODE_DISABLED.";
                break;
            }
            case INSTRUMENT_DISABLED:
            {
                cStringStream << " INSTRUMENT_DISABLED.";
                break;
            }
            case INSTRUMENT_FOR_MODEL_DISABLED:
            {
                cStringStream << " INSTRUMENT_FOR_MODEL_DISABLED.";
                break;
            }
            case ECN_DISABLED:
            {
                cStringStream << " ECN_DISABLED.";
                break;
            }
            case SELL_SIDE_DISABLED:
            {
                cStringStream << " SELL_SIDE_DISABLED.";
                break;
            }
            case BUY_SIDE_DISABLED:
            {
                cStringStream << " BUY_SIDE_DISABLED.";
                break;
            }
            case MODEL_KEY_DISABLED:
            {
                cStringStream << " MODEL_KEY_DISABLED.";
                break;
            }
            case MODEL_NOT_REGISTERED_FOR_ITEM:
            {
                cStringStream << " MODEL_NOT_REGISTERED_FOR_ITEM.";
                break;
            }

            case ORDER_SIZE_TOO_LARGE:
            {
                cStringStream << " ORDER_SIZE_TOO_LARGE.";
                break;
            }
            case OVER_ORDER_LIMIT:
            {
                cStringStream << " OVER_ORDER_LIMIT.";
                break;
            }
            case OVER_MAX_ORDER_LIMIT:
            {
                cStringStream << " OVER_MAX_ORDER_LIMIT.";
                break;
            }
            case SELL_MAX_POSITION_DISABLED:
            {
                cStringStream << " SELL_MAX_POSITION_DISABLED.";
                break;
            }
            case BUY_MAX_POSITION_DISABLED:
            {
                cStringStream << " BUY_MAX_POSITION_DISABLED.";
                break;
            }
            case ALREADY_PENDING_CANCEL:
            {
                cStringStream << " ALREADY_PENDING_CANCEL.";
                break;
            }
            case OTHER:
            {
                cStringStream << " OTHER.";
                break;
            }
            case UNKNOWN_ERROR:
            {
                cStringStream << " UNKNOWN_ERROR.";
                break;
            }

        }

        ErrorHandler::GetInstance()->newWarningMsg("3.4", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());

        return false;
    }

}

void HCScheduler::orderUpdate(Order* order)
{
    map<int, KOOrderPtr>::iterator orderMapItr = _mInternalOrdersMap.find(order->getOrderKey());
    if(orderMapItr != _mInternalOrdersMap.end())
    {
        KOOrderPtr pOrderToBeUpdated = orderMapItr->second;
        pOrderToBeUpdated->updateOrderRemainingQty();

        ORDER_STATUS eLastOrderState = order->getLastOrderState();

        if(eLastOrderState == PARTIALLY_FILLED)
        {
            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCREATION)
            {
                orderConfirmed(pOrderToBeUpdated);
            }

            long iFillQty = order->getLastFillAmount();

            if(order->getAction() == ACTION::SELL)
            {
                iFillQty = iFillQty * -1;
            }    

            if(!_bIsLiveTrading)
            {
                _cOrderActionLogger << m_ctx->getTime(CURRENT_TIME) << "|REPLY|FILL|KOID:" << pOrderToBeUpdated->igetOrderID() << "|HCID:" << pOrderToBeUpdated->_pHCOrder->getOrderKey() << "|Price:" << order->getLastFillRate() << "|Qty:" << iFillQty << "\n";
            }

            stringstream cStringStream;
            cStringStream << m_ctx->getTime(CURRENT_TIME) << "|REPLY|FILL|KOID:" << pOrderToBeUpdated->igetOrderID() << "|HCID:" << pOrderToBeUpdated->_pHCOrder->getOrderKey() << "|Price:" << order->getLastFillRate() << "|Qty:" << iFillQty << "|replyOrderId:" << order->getOrderKey();
            ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());

            orderFilled(pOrderToBeUpdated, iFillQty, order->getLastFillRate());

            if(order->isFinal())
            {
                if(!_bIsLiveTrading)
                {
                    _cOrderActionLogger << m_ctx->getTime(CURRENT_TIME) << "|REPLY|DELETE|KOID:" << pOrderToBeUpdated->igetOrderID() << "|HCID:" << pOrderToBeUpdated->_pHCOrder->getOrderKey() << "\n";
                }

                pOrderToBeUpdated->_pHCOrder = NULL;
                orderDeleted(pOrderToBeUpdated);
            }   
        }
        else if(eLastOrderState == ACCEPTED)
        {
            if(!_bIsLiveTrading)
            {
                _cOrderActionLogger << m_ctx->getTime(CURRENT_TIME) << "|REPLY|CONFIRM|KOID:" << pOrderToBeUpdated->igetOrderID() << "|HCID:" << pOrderToBeUpdated->_pHCOrder->getOrderKey() << "\n";
            }

            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCREATION || pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCHANGE)
            {
                orderConfirmed(pOrderToBeUpdated);
            }
            else
            {
                stringstream cStringStream;
                cStringStream.precision(10);
                cStringStream << "Unexpected ACCEPTED received! Order Price " << order->getOrderRate()  << " Order Qty " << order->getAmountLeft() << " KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order id " << order->getOrderKey() << " order state is " << pOrderToBeUpdated->_eOrderState << ".";
                ErrorHandler::GetInstance()->newWarningMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());

                orderConfirmedUnexpectedly(pOrderToBeUpdated);
            }
        }
        else if(eLastOrderState == REPLACE_REJECTED)
        {
            if(!_bIsLiveTrading)
            {
                _cOrderActionLogger << m_ctx->getTime(CURRENT_TIME) << "|REPLY|REPLACE_REJECT|KOID:" << pOrderToBeUpdated->igetOrderID() << "|HCID:" << pOrderToBeUpdated->_pHCOrder->getOrderKey() << "\n";
            }

            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCHANGE)
            {
                orderAmendRejected(pOrderToBeUpdated);

                pOrderToBeUpdated->_pParent->orderCriticalErrorHandler(pOrderToBeUpdated->igetOrderID());
                stringstream cStringStream;
                cStringStream << "Amend rejected for KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order id " << order->getOrderKey() << ".";
                ErrorHandler::GetInstance()->newWarningMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
            }
            else
            {
                stringstream cStringStream;
                cStringStream << "Unexpected REPLACE_REJECTED received! KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order id " << order->getOrderKey() << " order state is " << pOrderToBeUpdated->_eOrderState << ".";
                ErrorHandler::GetInstance()->newWarningMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
            }
        }
        else if(eLastOrderState == CANCEL_REJECTED)
        {
            if(!_bIsLiveTrading)
            {
                _cOrderActionLogger << m_ctx->getTime(CURRENT_TIME) << "|REPLY|DELET_REJECT|KOID:" << pOrderToBeUpdated->igetOrderID() << "|HCID:" << pOrderToBeUpdated->_pHCOrder->getOrderKey() << "\n";
            }

            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGDELETE)
            {
                orderDeleteRejected(pOrderToBeUpdated);

                stringstream cStringStream;
                cStringStream << "Cancel rejected for KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order sequence number " << order->getOrderKey() << ".";
                ErrorHandler::GetInstance()->newWarningMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
            }
            else
            {
                stringstream cStringStream;
                cStringStream << "Unexpected CANCEL_REJECTED received! KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order id " << order->getOrderKey() << " order state is " << pOrderToBeUpdated->_eOrderState << ".";
                ErrorHandler::GetInstance()->newWarningMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
            }
        }
        else if(eLastOrderState == REJECTED)
        {
            if(!_bIsLiveTrading)
            {
                _cOrderActionLogger << m_ctx->getTime(CURRENT_TIME) << "|REPLY|REJECT|KOID:" << pOrderToBeUpdated->igetOrderID() << "|HCID:" << pOrderToBeUpdated->_pHCOrder->getOrderKey() << "\n";
            }

            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCREATION)
            {
                orderRejected(pOrderToBeUpdated);
                pOrderToBeUpdated->_pParent->orderCriticalErrorHandler(pOrderToBeUpdated->igetOrderID());

                stringstream cStringStream;
                cStringStream << "Submit rejected for KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order id " << order->getOrderKey() << ".";
                ErrorHandler::GetInstance()->newWarningMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());

                pOrderToBeUpdated->_pHCOrder = NULL;
            }
            else
            {
                stringstream cStringStream;
                cStringStream << "Unexpected REJECTED received! KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order id " << order->getOrderKey() << " order state is " << pOrderToBeUpdated->_eOrderState << ".";
                ErrorHandler::GetInstance()->newWarningMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
            }
        }
        else if(eLastOrderState == CANCELLED || eLastOrderState == EXPIRED)
        {
            if(!_bIsLiveTrading)
            {
                _cOrderActionLogger << m_ctx->getTime(CURRENT_TIME) << "|REPLY|DELETE|KOID:" << pOrderToBeUpdated->igetOrderID() << "|HCID:" << pOrderToBeUpdated->_pHCOrder->getOrderKey() << "\n";
            }

            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGDELETE || pOrderToBeUpdated->bgetIsIOC())
            {
                pOrderToBeUpdated->_pHCOrder = NULL;
                orderDeleted(pOrderToBeUpdated);
            }
            else
            {
                stringstream cStringStream;
                cStringStream << "Unexpected CANCELLED received! KOOrder ID " << pOrderToBeUpdated->igetOrderID() << " HC Order id " << order->getOrderKey() << " order state is " << pOrderToBeUpdated->_eOrderState << ".";
                ErrorHandler::GetInstance()->newWarningMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());

                pOrderToBeUpdated->_pHCOrder = NULL;
                orderDeletedUnexpectedly(pOrderToBeUpdated);
            }
        }
        else
        {
            stringstream cStringStream;
            cStringStream << "Unkown last order state " << eLastOrderState;
            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "orderUpdate callback received with unknown HC order id " << order->getOrderKey() << ".";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

bool HCScheduler::bisLiveTrading()
{
    return _bIsLiveTrading;
}

long HCScheduler::igetTotalOpenBuy(int iProductCID)
{
    return 0;
}

long HCScheduler::igetTotalOpenSell(int iProductCID)
{
    return 0;
}

void HCScheduler::globalPositionUpdate(POSITION_DATA_STRUCT* pPosition)
{

}

int HCScheduler::updateTimer(lbm_context_t *ctx, const void * data)
{
    HCScheduler* pScheduler = const_cast<HCScheduler*>(static_cast<const HCScheduler*>(data));
    pScheduler->onTimer();
    return 0;
}

void HCScheduler::onTimer()
{
    KOEpochTime cNewUpdateTime;

    cNewUpdateTime = KOEpochTime(0, m_ctx->getTime(CURRENT_TIME) / 1000);

    SchedulerBase::wakeup(cNewUpdateTime);
    processTimeEvents(cNewUpdateTime);
}

KOEpochTime HCScheduler::cgetCurrentTime()
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

void HCScheduler::registerPositionServerSocket(int socketID)
{
    _pVelioSessionManager->registerFD(socketID, &socketCallback, this, LBM_FD_EVENT_READ | LBM_FD_EVENT_EXCEPT | LBM_FD_EVENT_CLOSE);
}

void HCScheduler::unregisterPositionServerSocket(int socketID)
{
    _pVelioSessionManager->unregisterFD(socketID, LBM_FD_EVENT_READ | LBM_FD_EVENT_EXCEPT | LBM_FD_EVENT_CLOSE);
}

int HCScheduler::socketCallback(lbm_context_t *ctx, lbm_handle_t handle, lbm_ulong_t ev, void *clientd)
{
    HCScheduler* pScheduler = const_cast<HCScheduler*>(static_cast<const HCScheduler*>(clientd));

    if(ev == LBM_FD_EVENT_READ)
    {
        pScheduler->_cPositionServerConnection.socketRead();
    }
    else if(ev == LBM_FD_EVENT_EXCEPT)
    {
        pScheduler->_cPositionServerConnection.socketError();
    }
    else if(ev == LBM_FD_EVENT_CLOSE)
    {
        pScheduler->_cPositionServerConnection.socketClose();
    }
    
    return 1;
}

}
