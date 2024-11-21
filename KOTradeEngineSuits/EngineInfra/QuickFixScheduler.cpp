#include "QuickFixScheduler.h"
#include "ErrorHandler.h"

#include "quickfix/fix44/MarketDataRequest.h"
#include "quickfix/fix44/ExecutionReport.h"
#include "quickfix/fix44/BusinessMessageReject.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/math/special_functions/round.hpp>

#include <iostream>
#include <string>
#include <stdlib.h>
#include <chrono>

using namespace std;
using namespace boost::posix_time;

namespace KO
{

static string sgetNextFixOrderID()
{
    static long iNextFixOrderID = 0;

    stringstream cStringStream;
    cStringStream << "KO_" << to_iso_string(second_clock::local_time()) << "_" << iNextFixOrderID;
    iNextFixOrderID++;
    return cStringStream.str();
};

QuickFixScheduler::QuickFixScheduler(SchedulerConfig &cfg, bool bIsLiveTrading)
:SchedulerBase("Config", cfg)
{
    _bIsLiveTrading =  bIsLiveTrading;
    _bScheduleFinished = false;

    _iNumTimerCallsReceived = 0;
    _iTotalNumMsg = 0;

    _sOrderSenderCompID = "";
    _bIsOrderSessionLoggedOn= false;

    preCommonInit();
}

QuickFixScheduler::~QuickFixScheduler()
{

}

void QuickFixScheduler::init()
{
    stringstream cStringStream;
    cStringStream << "Initialising quick fix scheduler";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    for(unsigned int i = 0; i < _cSchedulerCfg.vProducts.size(); ++i)
    {
        cStringStream.str("");
        cStringStream.clear();
        cStringStream << "Creating product " << _cSchedulerCfg.vProducts[i];
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

        QuoteData* pNewQuoteDataPtr;

        if(_cSchedulerCfg.vProducts[i].find("CFX") == 0)
        {
            pNewQuoteDataPtr = pregisterProduct(_cSchedulerCfg.vProducts[i], KO_FX);
        }
        else
        {
            pNewQuoteDataPtr = pregisterProduct(_cSchedulerCfg.vProducts[i], KO_FUTURE);
        }

        pNewQuoteDataPtr->iCID = i;
        pNewQuoteDataPtr->iProductExpoLimit = _cSchedulerCfg.vProductExpoLimit[i];
        pNewQuoteDataPtr->sTBProduct = _cSchedulerCfg.vTBProducts[i];
        pNewQuoteDataPtr->bIsLocalProduct = _cSchedulerCfg.vIsLocalProducts[i];

        pNewQuoteDataPtr->iProductMaxRisk = _cSchedulerCfg.vProductMaxRisk[i];
        pNewQuoteDataPtr->iProductExpoLimit = _cSchedulerCfg.vProductExpoLimit[i];

        pNewQuoteDataPtr->bDataSubscribed = false;

        _vProductOrderList.push_back(vector<KOOrderPtr>());
        _vProductDesiredPos.push_back(0);
        _vProductPos.push_back(0);

        _vLastOrderError.push_back("");

        _vProductLiquidationOrderList.push_back(vector<KOOrderPtr>());
        _vProductLiquidationDesiredPos.push_back(0);
        _vProductLiquidationPos.push_back(0);

        _vProductConsideration.push_back(0.0);
        _vProductVolume.push_back(0);

        _vProductStopLoss.push_back(_cSchedulerCfg.vProductStopLoss[i]);
        _vProductAboveSLInSec.push_back(0);
        _vProductLiquidating.push_back(false);

        _vFirstOrderTime.push_back(KOEpochTime());        

    }

    postCommonInit();

    sortTimeEvent();
}

void QuickFixScheduler::checkProductsForPriceSubscription()
{
    for(unsigned int i = 0; i < _cSchedulerCfg.vProducts.size(); ++i)
    {
        if(_vContractQuoteDatas[i]->bDataSubscribed == false)
        {
            string sTBExchange = "NONE";
            if(_vContractQuoteDatas[i]->sExchange == "XTMX" || _vContractQuoteDatas[i]->sExchange == "XASX")
            {
                sTBExchange = "ACTIV";
            }       
            else if(_vContractQuoteDatas[i]->sExchange == "XCME")
            {
                sTBExchange = "CME";
            } 
            else if(_vContractQuoteDatas[i]->sExchange == "XEUR")
            {
                sTBExchange = "EUREX";
            } 
            else if(_vContractQuoteDatas[i]->sExchange == "XLIF")
            {
                sTBExchange = "ICE";
            } 

            int iMDSessionsIdx = 0;
            const FIX::SessionID* pMarketDataSessionID = NULL;
            string sSenderCompID = "";
            bool bIsLoggedOn = false;
            for(;iMDSessionsIdx < _vMDSessions.size();iMDSessionsIdx++)
            {
                if(_vMDSessions[iMDSessionsIdx].sSenderCompID.find(sTBExchange) != std::string::npos)
                {
                    pMarketDataSessionID = _vMDSessions[iMDSessionsIdx].pFixSessionID;
                    sSenderCompID = _vMDSessions[iMDSessionsIdx].sSenderCompID;
                    bIsLoggedOn = _vMDSessions[iMDSessionsIdx].bIsLoggedOn;
                    break;
                }
            }

            if(pMarketDataSessionID == NULL)
            {
                stringstream cStringStream;
                cStringStream << "Cannot find market data fix session for " << _vContractQuoteDatas[i]->sExchange;
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
            }
            else
            {
                if(bIsLoggedOn == true)
                {
                    FIX::Message message;
                    message.getHeader().setField(8, "FIX.4.4");
                    message.getHeader().setField(49, _vMDSessions[iMDSessionsIdx].sSenderCompID);
                    message.getHeader().setField(56, "TBRICKS");
                    message.getHeader().setField(35, "V");

                    message.setField(22, "9");
                    message.setField(48, _cSchedulerCfg.vTBProducts[i]);
                    message.setField(15, _vContractQuoteDatas[i]->sCurrency);

                    stringstream cStringStream;
                    cStringStream << _vContractQuoteDatas[i]->iCID;
                    message.setField(262, cStringStream.str());
                    message.setField(55, _cSchedulerCfg.vProducts[i]);
                    
                    message.setField(263, "1"); // subcription type snapshot + updates
                    message.setField(264, "1"); // tob only
                    message.setField(265, "0"); // full refresh

                    // number requested data field - bid size|bid|offer|offer size
                    FIX44::MarketDataRequest::NoMDEntryTypes cMDEntryGroup;
                    cMDEntryGroup.set(FIX::MDEntryType('0'));
                    message.addGroup(cMDEntryGroup);
                    cMDEntryGroup.set(FIX::MDEntryType('1'));
                    message.addGroup(cMDEntryGroup);
                    cMDEntryGroup.set(FIX::MDEntryType('2'));
                    message.addGroup(cMDEntryGroup);

                    // number of requested instrument
                    message.setField(146, "1");
                    FIX::Session::sendToTarget(message, *pMarketDataSessionID);

                    stringstream cLogStringStream;
                    cLogStringStream << "Sending market data subscription request for " << _cSchedulerCfg.vProducts[i] << " TB Code " << _cSchedulerCfg.vTBProducts[i] << ".";
                    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[i]->sProduct, cLogStringStream.str());

                    _vContractQuoteDatas[i]->bDataSubscribed = true;
                }
            }
        }
    }
}

KOEpochTime QuickFixScheduler::cgetCurrentTime()
{
    uint64_t iMicrosecondsSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return KOEpochTime((long)iMicrosecondsSinceEpoch / 1000000, iMicrosecondsSinceEpoch % 1000000);
}

bool QuickFixScheduler::bisLiveTrading()
{
    return _bIsLiveTrading;
}

bool QuickFixScheduler::bschedulerFinished()
{
    return _bScheduleFinished;
}

bool QuickFixScheduler::sendToExecutor(const string& sProduct, long iDesiredPos)
{
    if(sProduct.substr(0, 1) == "L")
    {
        iDesiredPos = iDesiredPos / 2;
    }

    int iProductIdx = -1;

    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
    {
        if(_vContractQuoteDatas[i]->sProduct == sProduct)
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

        if((cgetCurrentTime() - _vFirstOrderTime[iProductIdx]).igetPrintable() < 300000000 || _vProductLiquidating[iProductIdx] == true)
        {
            iProductExpoLimit = iProductExpoLimit / 5;
        }

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
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream.str());

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
                        long iOrderPriceInTicks = icalcualteOrderPrice(iProductIdx, (*pOrderList)[i]->igetOrderPriceInTicks(), (*pOrderList)[i]->igetOrderRemainQty(), true, false);
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
        cStringStream << "Unrecognised product " << sProduct << " received in executor.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    return true;
}

bool QuickFixScheduler::sendToLiquidationExecutor(const string& sProduct, long iDesiredPos)
{
    if(sProduct.substr(0, 1) == "L")
    {
        iDesiredPos = iDesiredPos / 2;
    }

    int iProductIdx = -1;

    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
    {
        if(_vContractQuoteDatas[i]->sProduct == sProduct)
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
                        long iOrderPriceInTicks = icalcualteOrderPrice(iProductIdx, (*pOrderList)[i]->igetOrderPriceInTicks(), (*pOrderList)[i]->igetOrderRemainQty(), true, true);
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
        cStringStream << "Unrecognised product " << sProduct << " received in liquidation executor.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    return true;
}

void QuickFixScheduler::assignPositionToLiquidator(const string& sProduct, long iPosToLiquidate)
{
    if(sProduct.substr(0, 1) == "L")
    {
        iPosToLiquidate = iPosToLiquidate / 2;
    }

    unsigned int iProductIdx = -1;

    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
    {
        if(_vContractQuoteDatas[i]->sProduct == sProduct)
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
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream.str());
        }
    }
}

void QuickFixScheduler::exitScheduler()
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

void QuickFixScheduler::submitOrderBestPrice(unsigned int iProductIdx, long iQty, bool bIsLiquidation)
{
    if(bcheckRisk(iProductIdx, iQty) == false)
    {
        return;
    }

    KOOrderPtr pOrder;
    string sPendingOrderID = sgetNextFixOrderID();
    pOrder.reset(new KOOrder(sPendingOrderID, iProductIdx, _vContractQuoteDatas[iProductIdx]->dTickSize, false, _vContractQuoteDatas[iProductIdx]->sProduct, _vContractQuoteDatas[iProductIdx]->sProduct, _vContractQuoteDatas[iProductIdx]->sExchange, _vContractQuoteDatas[iProductIdx]->eInstrumentType, NULL));

    FIX::Message message;
    message.getHeader().setField(8, "FIX.4.4");
    message.getHeader().setField(49, _sOrderSenderCompID);
    message.getHeader().setField(56, "TBRICKS");
    message.getHeader().setField(35, "D"); // message type for new order
    message.getHeader().setField(11, sPendingOrderID); // client order id
    message.getHeader().setField(1, "TestAccount"); // account
    message.getHeader().setField(21, "1"); // exeuction type, always 1

    // set instrument
    message.setField(55, _vContractQuoteDatas[iProductIdx]->sProduct);
    message.setField(48, _vContractQuoteDatas[iProductIdx]->sTBProduct);
    message.setField(22, "9");

    if(_vContractQuoteDatas[iProductIdx]->sExchange == "XEUR")
    {
        message.setField(207, "XEUR");
    }
    else if(_vContractQuoteDatas[iProductIdx]->sExchange == "XLIF")
    {
        message.setField(207, "IFLL");
    }
    else if(_vContractQuoteDatas[iProductIdx]->sExchange == "XCME")
    {
        message.setField(207, "XCME");
    }

    if(iQty > 0)
    {
        message.setField(54, "1");
    }
    else if(iQty < 0)
    {
        message.setField(54, "2");
    }

    message.setField(9000, "N");
    message.setField(20014, "3");
    message.setField(20015, "3");
    message.setField(20011, "3000058");
    message.setField(20012, "3000058");
    message.setField(2362, "Y");
    message.setField(9001, "Trondheim");

    stringstream cQtyStream;
    cQtyStream << abs(iQty);
    message.setField(38, cQtyStream.str());

    double dOrderPrice = icalcualteOrderPrice(iProductIdx, 0, iQty, false, bIsLiquidation) * _vContractQuoteDatas[iProductIdx]->dTickSize;
    stringstream cPriceStream;
    cPriceStream.precision(10);
    cPriceStream << dOrderPrice;
    message.setField(44, cPriceStream.str()); // TODO: TEST all products with decimal prices ZT, ZF, ZN, I, L, FX

    message.setField(40, "2"); // limit order

    if(pOrder->_bIsIOC)
    {
        message.setField(59, "4"); 
    }
    else
    {
        message.setField(59, "0"); 
    }

    stringstream cStringStream;
    cStringStream.precision(10);
    if(bIsLiquidation == false)
    {
        cStringStream << "Submitting new order " << pOrder->sgetPendingOrderID() << ". qty " << iQty << " price " << dOrderPrice << ".";
    }
    else
    {
        cStringStream << "Submitting new liquidation order " << pOrder->sgetPendingOrderID() << ". qty " << iQty << " price " << dOrderPrice << ".";
    }
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());
    cStringStream.str("");
    cStringStream.clear();

    if(_bIsOrderSessionLoggedOn == true)
    {
        if(FIX::Session::sendToTarget(message, *_pOrderSessionID))
        {
            cStringStream << "Order submitted";
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());

            if(bIsLiquidation == false)
            {
                _vProductOrderList[iProductIdx].push_back(pOrder);
            }
            else
            {
                _vProductLiquidationOrderList[iProductIdx].push_back(pOrder);

            }

            pOrder->_eOrderState =  KOOrder::PENDINGCREATION;
            pOrder->_cPendingRequestTime = cgetCurrentTime();
            pOrder->_qOrderMessageHistory.push_back(cgetCurrentTime());

            _iTotalNumMsg = _iTotalNumMsg + 1;
        }
        else
        {
            cStringStream << "Failed to submit order";
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());

            if(_vLastOrderError[iProductIdx] != cStringStream.str())
            {
                _vLastOrderError[iProductIdx] = cStringStream.str();
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());
            }
        }
    }
}

void QuickFixScheduler::deleteOrder(KOOrderPtr pOrder)
{
    if(bcheckOrderMsgHistory(pOrder) == true)
    {
        FIX::Message message;
        message.getHeader().setField(8, "FIX.4.4");
        message.getHeader().setField(49, _sOrderSenderCompID);
        message.getHeader().setField(56, "TBRICKS");
        message.getHeader().setField(35, "F"); // message type for order cancel

        string sNewOrderID = sgetNextFixOrderID();
        message.setField(11, sNewOrderID);
        message.setField(41, pOrder->_sConfirmedOrderID);

        stringstream cStringStream;
        cStringStream << "Deleting order confirmed ID" << pOrder->_sConfirmedOrderID << " pending ID " << sNewOrderID << " TB order id " <<  pOrder->_sTBOrderID;
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", pOrder->sgetOrderProductName(), cStringStream.str());
        cStringStream.str("");
        cStringStream.clear();

        if(_bIsOrderSessionLoggedOn == true)
        {
            if(FIX::Session::sendToTarget(message, *_pOrderSessionID))
            {
                pOrder->_eOrderState = KOOrder::PENDINGDELETE;
                pOrder->_cPendingRequestTime = cgetCurrentTime();
                pOrder->_sPendingOrderID = sNewOrderID;
                cStringStream << "Order delete submitted. \n";
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
                cStringStream.str("");
                cStringStream.clear();

                _iTotalNumMsg = _iTotalNumMsg + 1;
            }
            else
            {
                cStringStream << "Failed to submit order delete for Order TB ID " << pOrder->_sTBOrderID;
                if(_vLastOrderError[pOrder->_iCID] != cStringStream.str())
                {
                    _vLastOrderError[pOrder->_iCID] = cStringStream.str();
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                cStringStream.str("");
                cStringStream.clear();
            }
        }
    }
}

void QuickFixScheduler::amendOrder(KOOrderPtr pOrder, long iQty, long iPriceInTicks)
{
    long iqtyDelta = iQty - pOrder->igetOrderRemainQty();

    if(bcheckRisk(pOrder->_iCID, iqtyDelta) == true && bcheckOrderMsgHistory(pOrder) == true)
    {
        long iNewOrgQty = pOrder->igetOrderOrgQty() + iqtyDelta;

        long iNewQty;
        if(iNewOrgQty > 0)
        {
            iNewQty = iNewOrgQty;
        }
        else
        {
            iNewQty = iNewOrgQty * -1;
        }

        double dNewPrice = iPriceInTicks * pOrder->_dTickSize;

        FIX::Message message;
        message.getHeader().setField(8, "FIX.4.4");
        message.getHeader().setField(49, _sOrderSenderCompID);
        message.getHeader().setField(56, "TBRICKS");
        message.getHeader().setField(35, "G"); // message type for order amend

        string sNewOrderID = sgetNextFixOrderID();
        message.setField(11, sNewOrderID);
        message.setField(41, pOrder->_sConfirmedOrderID);
    
        if(iNewOrgQty > 0)
        {
            message.setField(54, "1");
        }
        else if(iNewOrgQty < 0)
        {
            message.setField(54, "2");
        }
     
        stringstream cQtyStream;
        cQtyStream << abs(iNewQty);
        message.setField(38, cQtyStream.str());

        stringstream cPriceStream;
        cPriceStream.precision(10);
        cPriceStream << dNewPrice;
        message.setField(44, cPriceStream.str()); // TODO: TEST all products with decimal prices ZT, ZF, ZN, I, L, FX

        stringstream cStringStream;
        cStringStream.precision(10);
        cStringStream << "Amending order confirmed ID " << pOrder->_sConfirmedOrderID << " pending ID " << sNewOrderID << " TB order id " <<  pOrder->_sTBOrderID << ". new qty " << iQty << " new price " << dNewPrice << ".";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", pOrder->sgetOrderProductName(), cStringStream.str());
        cStringStream.str("");
        cStringStream.clear(); 

        if(_bIsOrderSessionLoggedOn == true)
        {
            if(FIX::Session::sendToTarget(message, *_pOrderSessionID))
            {
                pOrder->_sPendingOrderID = sNewOrderID;
                pOrder->_eOrderState = KOOrder::PENDINGCHANGE;
                pOrder->_cPendingRequestTime = cgetCurrentTime();
                pOrder->_qOrderMessageHistory.push_back(cgetCurrentTime());

                cStringStream << "Order amendment submitted. \n";
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
                cStringStream.str("");
                cStringStream.clear();

                _iTotalNumMsg = _iTotalNumMsg + 1;
            }
            else
            {
                cStringStream << "Failed to submit amendment for Order TB ID " << pOrder->_sTBOrderID;

                if(_vLastOrderError[pOrder->_iCID] != cStringStream.str())
                {
                    _vLastOrderError[pOrder->_iCID] = cStringStream.str();
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                cStringStream.str("");
                cStringStream.clear();
            }
        }
    }
}

long QuickFixScheduler::icalcualteOrderPrice(unsigned int iProductIdx, long iOrderPrice, long iQty, bool bOrderInMarket, bool bIsLiquidation)
{
    long iNewOrderPrice;

    if(bIsLiquidation == true)
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

void QuickFixScheduler::resetOrderState()
{
    for(unsigned int iProductIdx = 0; iProductIdx < _vProductOrderList.size(); iProductIdx++)
    {
        vector<KOOrderPtr>* pOrderList = &_vProductOrderList[iProductIdx];
        for(vector<KOOrderPtr>::iterator itr = pOrderList->begin(); itr != pOrderList->end();)
        {
            if((*itr)->_bOrderNoReplyTriggered == true)
            {
                stringstream cStringStream;
                cStringStream << "Removing stuck order " << (*itr)->_sPendingOrderID << " TB Order ID " << (*itr)->_sTBOrderID  << ".";
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", (*itr)->_sProduct, cStringStream.str());

                (*itr)->_eOrderState = KOOrder::INACTIVE;
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
                cStringStream << "Removing stuck order " << (*itr)->_sPendingOrderID << " TB Order ID " << (*itr)->_sTBOrderID  << ".";
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", (*itr)->_sProduct, cStringStream.str());

                (*itr)->_eOrderState = KOOrder::INACTIVE;
                itr = pOrderList->erase(itr);
            }
            else
            {
                itr++;
            }
        }
    }
}

void QuickFixScheduler::updateAllPnL()
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

void QuickFixScheduler::onTimer()
{
    KOEpochTime cNewUpdateTime = cgetCurrentTime();
    checkOrderState(cNewUpdateTime);
    SchedulerBase::wakeup(cNewUpdateTime);
    processTimeEvents(cNewUpdateTime);

    _iNumTimerCallsReceived = _iNumTimerCallsReceived + 1; 

    if(_iNumTimerCallsReceived == 2)
    {
        checkProductsForPriceSubscription();
    }
    else if((int)(cNewUpdateTime.igetPrintable() / 1000000) % 10 == 0)
    {
        checkProductsForPriceSubscription();
    }

    if(_iNumTimerCallsReceived == 10)
    {
        if(_sOrderSenderCompID != "")
        {
            if(_bIsOrderSessionLoggedOn == false)
            {
                stringstream cStringStream;
                cStringStream << "Order session " << _sOrderSenderCompID << " not logged on 10 seconds after engine start";
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
            }
        }

        for(int i = 0; i < _vMDSessions.size(); i++)
        {
            if(_vMDSessions[i].bIsLoggedOn == false)
            {
                stringstream cStringStream;
                cStringStream << "Market data session " << _vMDSessions[i].sSenderCompID << " not logged on 10 seconds after engine start";
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
            }
        } 
    }
}

void QuickFixScheduler::onCreate(const SessionID& cSessionID)
{
    string sSenderCompID = cSessionID.getSenderCompID();

    if(sSenderCompID.find("MD") != std::string::npos)
    {
        _vMDSessions.push_back(SessionDetails());
        _vMDSessions.back().pFixSessionID = &cSessionID;
        _vMDSessions.back().sSenderCompID = sSenderCompID;
        _vMDSessions.back().bIsLoggedOn = false;
    }
    else if(sSenderCompID.find("TR") != std::string::npos)
    {
        _pOrderSessionID = &cSessionID;
        _sOrderSenderCompID = sSenderCompID;
        _bIsOrderSessionLoggedOn = false;
    }

    stringstream cStringStream;
    cStringStream << "Creating fix session " << sSenderCompID;
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
}

void QuickFixScheduler::onLogon(const SessionID& cSessionID)
{
    string sSenderCompID = cSessionID.getSenderCompID();

    if(sSenderCompID.find("MD") != std::string::npos)
    {
        for(int i = 0; i < _vMDSessions.size(); i++)
        {
            if(_vMDSessions[i].sSenderCompID == sSenderCompID)
            {
                _vMDSessions[i].bIsLoggedOn = true;
                stringstream cStringStream;
                cStringStream << "Logged on to market data fix session " << sSenderCompID;
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                break;
            }
        }
    }
    else if(sSenderCompID.find("TR") != std::string::npos)
    {
        _bIsOrderSessionLoggedOn = true;
        stringstream cStringStream;
        cStringStream << "Logged on to order fix session " << _sOrderSenderCompID;
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void QuickFixScheduler::onLogout(const SessionID&)
{
}

void QuickFixScheduler::toAdmin(Message& cMessage, const SessionID& cSessionID)
{
    crack(cMessage, cSessionID);
}

void QuickFixScheduler::toApp(Message& cMessage, const SessionID& cSessionID) throw(DoNotSend)
{
    crack(cMessage, cSessionID);
}

void QuickFixScheduler::fromAdmin(const Message& cMessage, const SessionID& cSessionID) throw(FieldNotFound, IncorrectDataFormat, IncorrectTagValue, RejectLogon)
{
    crack(cMessage, cSessionID);
}

void QuickFixScheduler::fromApp(const Message& cMessage, const SessionID& cSessionID) throw(FieldNotFound, IncorrectDataFormat, IncorrectTagValue, UnsupportedMessageType)
{
    crack(cMessage, cSessionID);
}

void QuickFixScheduler::onMessage(const FIX44::Logout& cLogout, const FIX::SessionID& cSessionID)
{
    FIX::Text cText;
    cLogout.get(cText);

    string sSenderCompID = cSessionID.getSenderCompID();

    if(sSenderCompID.find("MD") != std::string::npos)
    {
        for(int i = 0; i < _vMDSessions.size(); i++)
        {
            if(_vMDSessions[i].sSenderCompID == sSenderCompID)
            {
                _vMDSessions[i].bIsLoggedOn = false;
                stringstream cStringStream;
                cStringStream << "Logged out from market data fix session " << sSenderCompID << ". Reason: " << cText;
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                break;
            }
        }
    }
    else if(sSenderCompID.find("TR") != std::string::npos)
    {
        _bIsOrderSessionLoggedOn = false;
        stringstream cStringStream;
        cStringStream << "Logged out from order fix session " << _sOrderSenderCompID << ". Reason: " << cText;
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void QuickFixScheduler::onMessage(FIX44::Reject& cReject, const FIX::SessionID& cSessionID)
{
    FIX::Text cText;
    cReject.get(cText);

    string sSenderCompID = cSessionID.getSenderCompID();    
    if(sSenderCompID.find("MD") != std::string::npos)
    {
        stringstream cStringStream;
        cStringStream << "Cannot translate server market data fix message. Reason: " << cText;
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
    else if(sSenderCompID.find("TR") != std::string::npos)
    {
        stringstream cStringStream;
        cStringStream << "Cannot translate server order fix message. Reason: " << cText;
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void QuickFixScheduler::onMessage(const FIX44::MarketDataRequestReject& cReject, const FIX::SessionID& cSessionID)
{
    FIX::Text cText;
    cReject.get(cText);

    int iCID;
    iCID = atoi(cReject.getField(262).c_str());

    stringstream cStringStream;
    cStringStream << "Market Data request " << _vContractQuoteDatas[iCID]->sProduct << " rejected. Reason: " << cText;
    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
}

void QuickFixScheduler::onMessage(const FIX44::ExecutionReport& cExecutionReport, const FIX::SessionID& cSessionID)
{
    FIX::OrderID cTBOrderID;
    cExecutionReport.get(cTBOrderID);
    string sTBOrderID = cTBOrderID;

    FIX::ClOrdID cClientOrderID;
    cExecutionReport.get(cClientOrderID);
    string sClientOrderID = cClientOrderID;
    
    FIX::OrdStatus cOrderStatus;
    cExecutionReport.get(cOrderStatus);
    char charOrderStatus = cOrderStatus;

    FIX::ExecType cExecType;
    cExecutionReport.get(cExecType);
    char charExecType = cExecType;

    bool bIsLiquidationOrder = false;
    bool bOrderFound = false;
    unsigned int iProductIdx = 0;
    unsigned int iOrderIdx = 0;
    vector<KOOrderPtr>* pOrderList;

    for(; iProductIdx < _vProductOrderList.size(); iProductIdx++)
    {
        pOrderList = &_vProductOrderList[iProductIdx];
        iOrderIdx = 0;
        for(; iOrderIdx < pOrderList->size(); iOrderIdx++)
        {
            if((*pOrderList)[iOrderIdx]->_eOrderState == KOOrder::PENDINGCREATION)
            {
                if(sClientOrderID == (*pOrderList)[iOrderIdx]->sgetPendingOrderID())
                {
                    bOrderFound = true;
                    break;
                } 
            }
            else
            {
                if(sTBOrderID == (*pOrderList)[iOrderIdx]->sgetTBOrderID())
                {
                    bOrderFound = true;
                    break;
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
                if((*pOrderList)[iOrderIdx]->_eOrderState == KOOrder::PENDINGCREATION)
                {
                    if(sClientOrderID == (*pOrderList)[iOrderIdx]->sgetPendingOrderID())
                    {
                        bIsLiquidationOrder = true;
                        bOrderFound = true;
                        break;
                    } 
                }
                else
                {
                    if(sTBOrderID == (*pOrderList)[iOrderIdx]->sgetTBOrderID())
                    {
                        bIsLiquidationOrder = true;
                        bOrderFound = true;
                        break;
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

        if(charExecType == 'A')
        {
            pOrderToBeUpdated->_sTBOrderID = sTBOrderID;
        }

        if(charExecType == 'F') // order filled
        {
            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCREATION)
            {
                pOrderToBeUpdated->_eOrderState = KOOrder::ACTIVE;
                pOrderToBeUpdated->_sTBOrderID = sTBOrderID;
            }

            long iRemainQty = atoi(cExecutionReport.getField(151).c_str());
            long iSide = atoi(cExecutionReport.getField(54).c_str());
            if(iSide == 2)
            {
                iRemainQty = iRemainQty * -1;
            }

            pOrderToBeUpdated->_iOrderRemainQty = iRemainQty;

            long iFillQty = atoi(cExecutionReport.getField(32).c_str());
            if(iSide == 2)
            {
                iFillQty = iFillQty * -1;
            }
            double dFillPrice = atof(cExecutionReport.getField(31).c_str());

            stringstream cStringStream;
            if(bIsLiquidationOrder == true)
            {
                cStringStream << "Liquidation order Filled - Order confirmed ID: " << pOrderToBeUpdated->_sConfirmedOrderID << " pending ID: " << pOrderToBeUpdated->_sPendingOrderID << " TB ID: " << pOrderToBeUpdated->_sTBOrderID << " qty: " << iFillQty << " price: " << dFillPrice << ".";
            }
            else
            {
                cStringStream << "Order Filled - Order confirmed ID: " << pOrderToBeUpdated->_sConfirmedOrderID << " pending ID: " << pOrderToBeUpdated->_sPendingOrderID << " TB ID: " << pOrderToBeUpdated->_sTBOrderID << " qty: " << iFillQty << " price: " << dFillPrice << ".";
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

            _vProductConsideration[iProductIdx] = _vProductConsideration[iProductIdx] - (double)iFillQty * dFillPrice;
            _vProductVolume[iProductIdx] = _vProductVolume[iProductIdx] + abs(iFillQty);

            long iAdjustedFillQty = iFillQty;
            if(_vContractQuoteDatas[iProductIdx]->sProduct.substr(0, 1) == "L")
            {
                iAdjustedFillQty = iAdjustedFillQty * 2;
            }

            _pTradeSignalMerger->onFill(_vContractQuoteDatas[iProductIdx]->sProduct, iAdjustedFillQty, dFillPrice, bIsLiquidationOrder);
            if(iRemainQty == 0)
            {
                cStringStream << "Order Fully Filled - Order confirmed ID: " << pOrderToBeUpdated->_sConfirmedOrderID << " pending ID: " << pOrderToBeUpdated->_sPendingOrderID << " TB ID: " << pOrderToBeUpdated->_sTBOrderID << ".";
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());

                _vLastOrderError[iProductIdx] = "";

                pOrderToBeUpdated->_sTBOrderID = "";
                pOrderToBeUpdated->_eOrderState = KOOrder::INACTIVE;
                pOrderList->erase(pOrderList->begin() + iOrderIdx);
            }
        }
        else if(charExecType == '0' || charExecType == '5') // order accepted
        {
            long iOrgQty = atoi(cExecutionReport.getField(38).c_str());
            long iRemainQty = atoi(cExecutionReport.getField(151).c_str());
            long iSide = atoi(cExecutionReport.getField(54).c_str());
            if(iSide == 2)
            {
                iOrgQty = iOrgQty * -1;
                iRemainQty = iRemainQty * -1;
            }
            double dOrderPrice = atof(cExecutionReport.getField(44).c_str());

            pOrderToBeUpdated->_iOrderOrgQty = iOrgQty;
            pOrderToBeUpdated->_iOrderRemainQty = iRemainQty;
            pOrderToBeUpdated->_dOrderPrice = dOrderPrice;
            pOrderToBeUpdated->_iOrderPriceInTicks = boost::math::iround(dOrderPrice / pOrderToBeUpdated->_dTickSize);

            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCREATION || pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCHANGE)
            {
                if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCREATION)
                {
                    pOrderToBeUpdated->_sTBOrderID = sTBOrderID;
                }

                stringstream cStringStream;
                cStringStream.precision(10);

                _vLastOrderError[iProductIdx] = "";

                cStringStream << "Order confirmed - Order confirmed ID: " << pOrderToBeUpdated->_sConfirmedOrderID << " pending ID: " << pOrderToBeUpdated->_sPendingOrderID << " TB ID: " << pOrderToBeUpdated->_sTBOrderID << " price: " << dOrderPrice << " qty " << iRemainQty << ".";
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());
            }
            else
            {
                stringstream cStringStream;
                cStringStream.precision(10);
                cStringStream << "Unexpected ACCEPTED received! Order Price " << dOrderPrice  << " Order Qty " << iRemainQty << " TB ID " << pOrderToBeUpdated->_sTBOrderID << " order state is " << pOrderToBeUpdated->_eOrderState << ".";

                if(_vLastOrderError[iProductIdx] != cStringStream.str())
                {
                    _vLastOrderError[iProductIdx] = cStringStream.str();
                    ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
            }
               
            pOrderToBeUpdated->_sConfirmedOrderID = pOrderToBeUpdated->_sPendingOrderID; 
            pOrderToBeUpdated->_eOrderState = KOOrder::ACTIVE;
        }
        else if(charExecType == '3' || charExecType == '4' || charExecType == 'C') // order cancelled
        {
            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGDELETE || pOrderToBeUpdated->bgetIsIOC())
            {
                stringstream cStringStream;
                cStringStream.precision(10);
                cStringStream << "Order cancel acked - Order confirmed ID: " << pOrderToBeUpdated->_sConfirmedOrderID << " pending ID: " << pOrderToBeUpdated->_sPendingOrderID << " TB ID: " << pOrderToBeUpdated->_sTBOrderID << ".";
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[iProductIdx]->sProduct, cStringStream.str());

                _vLastOrderError[iProductIdx] = "";

                pOrderToBeUpdated->_sTBOrderID = "";
                pOrderToBeUpdated->_eOrderState = KOOrder::INACTIVE;
                pOrderList->erase(pOrderList->begin() + iOrderIdx);
            }
            else
            {
                stringstream cStringStream;
                cStringStream << "Unexpected CANCELLED received! TB ID: " << pOrderToBeUpdated->_sTBOrderID << " order state is " << pOrderToBeUpdated->_eOrderState << ".";

                if(_vLastOrderError[iProductIdx] != cStringStream.str())
                {
                    _vLastOrderError[iProductIdx] = cStringStream.str();
                    ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());

                pOrderToBeUpdated->_sTBOrderID = "";
                pOrderToBeUpdated->_eOrderState = KOOrder::INACTIVE;
                pOrderList->erase(pOrderList->begin() + iOrderIdx);
            }
        }
        else if(charExecType == '8') // order rejected
        {
            FIX::Text cText;
            cExecutionReport.get(cText);
            string sRejectReason = cText;

            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCREATION)
            {
                stringstream cStringStream;
                cStringStream << "Submit rejected. Pending ID: " << pOrderToBeUpdated->_sPendingOrderID << " TB ID " << sTBOrderID << ". Reason: " << sRejectReason << ".";

                if(_vLastOrderError[iProductIdx] != cStringStream.str())
                {
                    if(_vLastOrderError[iProductIdx].find(sRejectReason) == std::string::npos)
                    {
                        _vLastOrderError[iProductIdx] = cStringStream.str();
                        ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                    }
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());

                pOrderToBeUpdated->_sTBOrderID = "";
                pOrderToBeUpdated->_eOrderState = KOOrder::INACTIVE;
                pOrderList->erase(pOrderList->begin() + iOrderIdx);
            }
            else
            {
                stringstream cStringStream;
                cStringStream << "Unexpected REJECTED received! TB ID " << sTBOrderID << " order state is " << pOrderToBeUpdated->_eOrderState << ". Reason: " << sRejectReason << ".";

                if(_vLastOrderError[iProductIdx] != cStringStream.str())
                {
                    _vLastOrderError[iProductIdx] = cStringStream.str();
                    ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
            }
        }
        else
        {
            if(charExecType != 'A' && charExecType != '6' && charExecType != 'E')
            {
                stringstream cStringStream;
                cStringStream << "Unknown execution type " << charExecType << " for order TB ID " << sTBOrderID;

                if(_vLastOrderError[iProductIdx] != cStringStream.str())
                {
                    _vLastOrderError[iProductIdx] = cStringStream.str();
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
            }
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "orderUpdate callback received with unknown order. TB ID " << sTBOrderID << " KO ID " << sClientOrderID << ".";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void QuickFixScheduler::onMessage(const FIX44::MarketDataSnapshotFullRefresh& cMarketDataSnapshotFullRefresh, const FIX::SessionID& cSessionID)
{
    int iCID;
    iCID = atoi(cMarketDataSnapshotFullRefresh.getField(262).c_str());

    int iNumEntries;
    iNumEntries = atoi(cMarketDataSnapshotFullRefresh.getField(268).c_str());

    FIX44::MarketDataSnapshotFullRefresh::NoMDEntries group;
    FIX::MDEntryType MDEntryType;
    FIX::MDEntryPx MDEntryPx;
    FIX::MDEntrySize MDEntrySize;

    long iBidSize;
    double dBestBid;
    double dBestAsk;
    long iAskSize;
    double dLastTrade;
    long iLastTradeSize;

    _vContractQuoteDatas[iCID]->iTradeSize = 0;
    _vContractQuoteDatas[iCID]->iPrevBidInTicks = _vContractQuoteDatas[iCID]->iBestBidInTicks;
    _vContractQuoteDatas[iCID]->iPrevAskInTicks = _vContractQuoteDatas[iCID]->iBestAskInTicks;
    _vContractQuoteDatas[iCID]->iPrevBidSize = _vContractQuoteDatas[iCID]->iBidSize;
    _vContractQuoteDatas[iCID]->iPrevAskSize = _vContractQuoteDatas[iCID]->iAskSize;

    for(int i = 0; i < iNumEntries; i++)
    {
        cMarketDataSnapshotFullRefresh.getGroup(i+1, group);
        group.get(MDEntryType);
        group.get(MDEntryPx);
        group.get(MDEntrySize);

        if(MDEntryType == '0')
        {
            iBidSize = MDEntrySize;
            dBestBid = MDEntryPx;

            _vContractQuoteDatas[iCID]->dBestBid = dBestBid;
            _vContractQuoteDatas[iCID]->iBestBidInTicks = boost::math::iround(_vContractQuoteDatas[iCID]->dBestBid / _vContractQuoteDatas[iCID]->dTickSize);
            _vContractQuoteDatas[iCID]->iBidSize = iBidSize;
        }
        else if(MDEntryType == '1')
        {
            iAskSize = MDEntrySize;
            dBestAsk = MDEntryPx;

            _vContractQuoteDatas[iCID]->dBestAsk = dBestAsk;
            _vContractQuoteDatas[iCID]->iBestAskInTicks = boost::math::iround(_vContractQuoteDatas[iCID]->dBestAsk / _vContractQuoteDatas[iCID]->dTickSize);
            _vContractQuoteDatas[iCID]->iAskSize = iAskSize;
        }
        else if(MDEntryType == '2')
        {
            iLastTradeSize = MDEntrySize;
            dLastTrade = MDEntryPx;

            _vContractQuoteDatas[iCID]->dLastTradePrice = dLastTrade;
            _vContractQuoteDatas[iCID]->iLastTradeInTicks = boost::math::iround(_vContractQuoteDatas[iCID]->dLastTradePrice / _vContractQuoteDatas[iCID]->dTickSize);
            _vContractQuoteDatas[iCID]->iTradeSize = iLastTradeSize;
            _vContractQuoteDatas[iCID]->iAccumuTradeSize = _vContractQuoteDatas[iCID]->iAccumuTradeSize + iLastTradeSize;
        }
    }

    if(_vContractQuoteDatas[iCID]->iBidSize != 0 && _vContractQuoteDatas[iCID]->iAskSize != 0)
    {
        if(_vContractQuoteDatas[iCID]->iPrevBidInTicks != _vContractQuoteDatas[iCID]->iBestBidInTicks || _vContractQuoteDatas[iCID]->iPrevAskInTicks != _vContractQuoteDatas[iCID]->iBestAskInTicks || _vContractQuoteDatas[iCID]->iPrevBidSize != _vContractQuoteDatas[iCID]->iBidSize || _vContractQuoteDatas[iCID]->iPrevAskSize != _vContractQuoteDatas[iCID]->iAskSize || iLastTradeSize != 0)
        {
            double dWeightedMidInTicks;
            if(_vContractQuoteDatas[iCID]->iBestAskInTicks - _vContractQuoteDatas[iCID]->iBestBidInTicks != 1 || (_vContractQuoteDatas[iCID]->iBidSize + _vContractQuoteDatas[iCID]->iAskSize == 0))
            {
                dWeightedMidInTicks = (double)(_vContractQuoteDatas[iCID]->iBestAskInTicks + _vContractQuoteDatas[iCID]->iBestBidInTicks) / 2;
            }
            else
            {
                dWeightedMidInTicks = (double)_vContractQuoteDatas[iCID]->iBestBidInTicks + (double)_vContractQuoteDatas[iCID]->iBidSize / (double)(_vContractQuoteDatas[iCID]->iBidSize + _vContractQuoteDatas[iCID]->iAskSize);
            }

            _vContractQuoteDatas[iCID]->dWeightedMidInTicks = dWeightedMidInTicks;
            _vContractQuoteDatas[iCID]->dWeightedMid = dWeightedMidInTicks * _vContractQuoteDatas[iCID]->dTickSize;
            newPriceUpdate(iCID);

            if(_vContractQuoteDatas[iCID]->iBestAskInTicks - _vContractQuoteDatas[iCID]->iBestBidInTicks > 0)
            {
                updateOrderPrice(iCID);
                updateLiquidationOrderPrice(iCID);
            }
        }
    }
}

void QuickFixScheduler::onMessage(FIX44::BusinessMessageReject& cBusinessMessageReject, const FIX::SessionID& cSessionID)
{
    FIX::Text cText;
    cBusinessMessageReject.get(cText);

    string sSenderCompID = cSessionID.getSenderCompID();
    if(sSenderCompID.find("MD") != std::string::npos)
    {
        stringstream cStringStream;
        cStringStream << "Unable to handle market data server message. Reason: " << cText;
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
    else if(sSenderCompID.find("TR") != std::string::npos)
    {
        stringstream cStringStream;
        cStringStream << "Unable to handle order server message. Reason: " << cText;
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void QuickFixScheduler::onMessage(const FIX44::BusinessMessageReject& cBusinessMessageReject, const FIX::SessionID& cSessionID)
{
    FIX::Text cText;
    cBusinessMessageReject.get(cText);
    string sText = cText;

    FIX::RefMsgType cRefMsgType;
    cBusinessMessageReject.get(cRefMsgType);

    if(cRefMsgType ==  "V")
    {
        stringstream cStringStream;
        cStringStream << "Unable to register market data for session " << cSessionID.getSenderCompID() << ". Reason: " << cText;
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
    else if(cRefMsgType == "D")
    {
        FIX::BusinessRejectRefID cBusinessRejectRefID;
        cBusinessMessageReject.get(cBusinessRejectRefID);
        string sClientOrderID = cBusinessRejectRefID;

        bool bIsLiquidationOrder = false;
        bool bOrderFound = false;
        unsigned int iProductIdx = 0;
        unsigned int iOrderIdx = 0;
        vector<KOOrderPtr>* pOrderList;
        for(; iProductIdx < _vProductOrderList.size(); iProductIdx++)
        {
            pOrderList = &_vProductOrderList[iProductIdx];
            iOrderIdx = 0;
            for(; iOrderIdx < pOrderList->size(); iOrderIdx++)
            {
                if((*pOrderList)[iOrderIdx]->_eOrderState == KOOrder::PENDINGCREATION)
                {
                    if(sClientOrderID == (*pOrderList)[iOrderIdx]->sgetPendingOrderID())
                    {
                        bOrderFound = true;
                        break;
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
                    if((*pOrderList)[iOrderIdx]->_eOrderState == KOOrder::PENDINGCREATION)
                    {
                        if(sClientOrderID == (*pOrderList)[iOrderIdx]->sgetPendingOrderID())
                        {
                            bIsLiquidationOrder = true;
                            bOrderFound = true;
                            break;
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

            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGCREATION)
            {
                stringstream cStringStream;
                cStringStream << "Submit rejected. Pending ID: " << pOrderToBeUpdated->_sPendingOrderID << ". Reason: " << sText << ".";

                if(_vLastOrderError[iProductIdx] != cStringStream.str())
                {
                    if(_vLastOrderError[iProductIdx].find(sText) == std::string::npos)
                    {
                        _vLastOrderError[iProductIdx] = cStringStream.str();
                        ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                    }
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());

                pOrderToBeUpdated->_sTBOrderID = "";
                pOrderToBeUpdated->_eOrderState = KOOrder::INACTIVE;
                pOrderList->erase(pOrderList->begin() + iOrderIdx);
            }
            else
            {
                stringstream cStringStream;
                cStringStream << "Unexpected REJECTED received! Confirmed ID: " << pOrderToBeUpdated->_sConfirmedOrderID << " order state is " << pOrderToBeUpdated->_eOrderState << ". Reason: " << sText << ".";

                if(_vLastOrderError[iProductIdx] != cStringStream.str())
                {
                    _vLastOrderError[iProductIdx] = cStringStream.str();
                    ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
            }
        }
        else
        {
            stringstream cStringStream;
            cStringStream << "order business rejected received with unknown pending ID " << sClientOrderID << ".";

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
        cStringStream << "Unhandled business rejected received. Message type: " << cRefMsgType << " Reason " << cText;
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void QuickFixScheduler::onMessage(const FIX44::OrderCancelReject& cOrderCancelReject, const FIX::SessionID& cSessionID)
{
    // order cancel or amend rejected
    FIX::OrdStatus cOrderStatus;
    cOrderCancelReject.get(cOrderStatus);

    if(cOrderStatus != '2')
    {
        FIX::OrderID cOrderID;
        cOrderCancelReject.get(cOrderID);
        string sTBOrderID = cOrderID;

        FIX::ClOrdID cClientOrderID;
        cOrderCancelReject.get(cClientOrderID);
        string sClientOrderID = cClientOrderID;

        FIX::OrigClOrdID cOrgClientOrderID;
        cOrderCancelReject.get(cOrgClientOrderID);
        string sOrgClientOrderID = cOrgClientOrderID;

        FIX::Text cText;
        cOrderCancelReject.get(cText);
        string sText = cText;

        bool bIsLiquidationOrder = false;
        bool bOrderFound = false;
        unsigned int iProductIdx = 0;
        unsigned int iOrderIdx = 0;
        vector<KOOrderPtr>* pOrderList;
        for(; iProductIdx < _vProductOrderList.size(); iProductIdx++)
        {
            pOrderList = &_vProductOrderList[iProductIdx];
            iOrderIdx = 0;
            for(; iOrderIdx < pOrderList->size(); iOrderIdx++)
            {
                if((*pOrderList)[iOrderIdx]->_eOrderState == KOOrder::PENDINGCREATION)
                {
                    if(sClientOrderID == (*pOrderList)[iOrderIdx]->sgetPendingOrderID())
                    {
                        bOrderFound = true;
                        break;
                    }
                }
                else
                {
                    if(sTBOrderID == (*pOrderList)[iOrderIdx]->sgetTBOrderID())
                    {
                        bOrderFound = true;
                        break;
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
                    if((*pOrderList)[iOrderIdx]->_eOrderState == KOOrder::PENDINGCREATION)
                    {
                        if(sClientOrderID == (*pOrderList)[iOrderIdx]->sgetPendingOrderID())
                        {
                            bIsLiquidationOrder = true;
                            bOrderFound = true;
                            break;
                        }
                    }
                    else
                    {
                        if(sTBOrderID == (*pOrderList)[iOrderIdx]->sgetTBOrderID())
                        {
                            bIsLiquidationOrder = true;
                            bOrderFound = true;
                            break;
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

            if(pOrderToBeUpdated->_eOrderState == KOOrder::PENDINGDELETE)
            {
                stringstream cStringStream;

                cStringStream << "Cancel or amend rejected confirmed ID: " << pOrderToBeUpdated->_sConfirmedOrderID << " pending ID: " << pOrderToBeUpdated->_sPendingOrderID << " TB ID: " << pOrderToBeUpdated->_sTBOrderID << " Reason: " << sText;

                if(_vLastOrderError[iProductIdx] != cStringStream.str())
                {
                    _vLastOrderError[iProductIdx] = cStringStream.str();
                    ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());

                pOrderToBeUpdated->_eOrderState = KOOrder::ACTIVE;
            }
            else
            {
                stringstream cStringStream;
                cStringStream << "Unexpected CANCEL_REJECTED received! TB ID " << pOrderToBeUpdated->_sTBOrderID << " order state is " << pOrderToBeUpdated->_eOrderState << ". Reason: " << sText;

                if(_vLastOrderError[iProductIdx] != cStringStream.str())
                {
                    _vLastOrderError[iProductIdx] = cStringStream.str();
                    ErrorHandler::GetInstance()->newErrorMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrderToBeUpdated->_sAccount, pOrderToBeUpdated->sgetOrderProductName(), cStringStream.str());

                pOrderToBeUpdated->_eOrderState = KOOrder::ACTIVE;
            }
        }
        else
        {
            stringstream cStringStream;
            cStringStream << "Order cancel reject received with unknown TB ID " << sTBOrderID << ".";
            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
        }
    }
}

void QuickFixScheduler::updateOrderPrice(unsigned int iProductIdx)
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
            long iOrderPriceInTicks = icalcualteOrderPrice(iProductIdx, (*pOrderList)[0]->igetOrderPriceInTicks(), iOpenOrderQty, true, false);
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

void QuickFixScheduler::updateLiquidationOrderPrice(unsigned int iProductIdx)
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
        long iOrderPriceInTicks = icalcualteOrderPrice(iProductIdx, (*pOrderList)[0]->igetOrderPriceInTicks(), iOpenOrderQty, true, true);
        if(iOrderPriceInTicks != (*pOrderList)[0]->igetOrderPriceInTicks())
        {
            for(unsigned int i = 0; i < pOrderList->size(); i++)
            {
                amendOrderPriceInTicks((*pOrderList)[i], iOrderPriceInTicks);
            }
        }
    }
}

void QuickFixScheduler::checkOrderState(KOEpochTime cCurrentTime)
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
                        cStringStream << "Order " << (*pOrderList)[iOrderIdx]->_sPendingOrderID << " TB Order ID " << (*pOrderList)[iOrderIdx]->_sTBOrderID  << " held by server for more thatn 10 seconds. Contact trade support. ";
                        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", (*pOrderList)[iOrderIdx]->_sProduct, cStringStream.str());
                        (*pOrderList)[iOrderIdx]->_bOrderNoReplyTriggered = true;
                    }
                }
            }

            if((*pOrderList)[iOrderIdx]->borderCanBeChanged())
            {
                if((*pOrderList)[iOrderIdx]->igetOrderRemainQty() > 0)
                {
                    if((*pOrderList)[iOrderIdx]->_iOrderPriceInTicks > _vContractQuoteDatas[(*pOrderList)[iOrderIdx]->_iCID]->iBestAskInTicks)
                    {
                        if((*pOrderList)[iOrderIdx]->_bOrderNoFill == false)
                        {
                            (*pOrderList)[iOrderIdx]->_bOrderNoFill = true;
                            (*pOrderList)[iOrderIdx]->_cOrderNoFillTime = cCurrentTime;
                        }
                        else
                        {
                            if((cCurrentTime - (*pOrderList)[iOrderIdx]->_cOrderNoFillTime).sec() > 2)
                            {
                                if((*pOrderList)[iOrderIdx]->_bOrderNoFillTriggered == false)
                                {
                                    (*pOrderList)[iOrderIdx]->_bOrderNoFillTriggered = true;

                                    stringstream cStringStream;
                                    cStringStream << "Order " << (*pOrderList)[iOrderIdx]->_sConfirmedOrderID << "TB Order ID " << (*pOrderList)[iOrderIdx]->_sTBOrderID << " market ask price " << _vContractQuoteDatas[(*pOrderList)[iOrderIdx]->_iCID]->dBestAsk << " smaller than buy order price " << (*pOrderList)[iOrderIdx]->dgetOrderPrice() << " for more than 2 seconds.";
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
                            (*pOrderList)[iOrderIdx]->_cOrderNoFillTime = cCurrentTime;
                        }
                        else
                        {
                            if((cCurrentTime - (*pOrderList)[iOrderIdx]->_cOrderNoFillTime).sec() > 2)
                            {
                                if((*pOrderList)[iOrderIdx]->_bOrderNoFillTriggered == false)
                                {
                                    (*pOrderList)[iOrderIdx]->_bOrderNoFillTriggered = true;

                                    stringstream cStringStream;
                                    cStringStream << "Order " << (*pOrderList)[iOrderIdx]->_sConfirmedOrderID << "TB Order ID " << (*pOrderList)[iOrderIdx]->_sTBOrderID << " market bid price " << _vContractQuoteDatas[(*pOrderList)[iOrderIdx]->_iCID]->dBestBid << " bigger than sell order price " << (*pOrderList)[iOrderIdx]->dgetOrderPrice() << " for more than 2 seconds.";
                                    ErrorHandler::GetInstance()->newErrorMsg("0", (*pOrderList)[iOrderIdx]->_sProduct, (*pOrderList)[iOrderIdx]->_sProduct, cStringStream.str());
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
                        cStringStream << "Order " << (*pOrderList)[iOrderIdx]->_sPendingOrderID << " TB Order ID " << (*pOrderList)[iOrderIdx]->_sTBOrderID  << " held by server for more thatn 10 seconds. Contact trade support. ";
                        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", (*pOrderList)[iOrderIdx]->_sProduct, cStringStream.str());
                        (*pOrderList)[iOrderIdx]->_bOrderNoReplyTriggered = true;
                    }
                }
            }
        }
    }
}

void QuickFixScheduler::amendOrderPriceInTicks(KOOrderPtr pOrder, long iPriceInTicks)
{
    if(bcheckOrderMsgHistory(pOrder) == true)
    {
        double dNewPrice = iPriceInTicks * pOrder->_dTickSize;
        long iNewQty = pOrder->igetOrderOrgQty();

        FIX::Message message;
        message.getHeader().setField(8, "FIX.4.4");
        message.getHeader().setField(49, _sOrderSenderCompID);
        message.getHeader().setField(56, "TBRICKS");
        message.getHeader().setField(35, "G"); // message type for order amend

        string sNewOrderID = sgetNextFixOrderID();
        message.setField(11, sNewOrderID);
        message.setField(41, pOrder->_sConfirmedOrderID);

        if(iNewQty > 0)
        {
            message.setField(54, "1");
        }
        else if(iNewQty < 0)
        {
            message.setField(54, "2");
        }

        stringstream cQtyStream;
        cQtyStream << abs(iNewQty);
        message.setField(38, cQtyStream.str());

        stringstream cPriceStream;
        cPriceStream.precision(10);
        cPriceStream << dNewPrice;
        message.setField(44, cPriceStream.str()); // TODO: TEST all products with decimal prices ZT, ZF, ZN, I, L, FX

        stringstream cStringStream;
        cStringStream.precision(10);
        cStringStream << "Amending order confirmed ID" << pOrder->_sConfirmedOrderID << " pending ID " << sNewOrderID << " TB order id " <<  pOrder->_sTBOrderID << ". new price " << dNewPrice << ".";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", pOrder->sgetOrderProductName(), cStringStream.str());
        cStringStream.str("");
        cStringStream.clear();

        if(_bIsOrderSessionLoggedOn == true)
        {
            if(FIX::Session::sendToTarget(message, *_pOrderSessionID))
            {
                pOrder->_sPendingOrderID = sNewOrderID;
                pOrder->_eOrderState = KOOrder::PENDINGCHANGE;
                pOrder->_cPendingRequestTime = cgetCurrentTime();
                pOrder->_qOrderMessageHistory.push_back(cgetCurrentTime());

                cStringStream << "Order amendment submitted. \n";
                ErrorHandler::GetInstance()->newInfoMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
                cStringStream.str("");
                cStringStream.clear();

                _iTotalNumMsg = _iTotalNumMsg + 1;
            }
            else
            {
                cStringStream << "Failed to submit amendment for Order TB ID " << pOrder->_sTBOrderID;

                if(_vLastOrderError[pOrder->_iCID] != cStringStream.str())
                {
                    _vLastOrderError[pOrder->_iCID] = cStringStream.str();
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                }
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                cStringStream.str("");
                cStringStream.clear();
            }
        }
    }
}

bool QuickFixScheduler::bcheckOrderMsgHistory(KOOrderPtr pOrder)
{
    bool bResult = false;
    while(pOrder->_qOrderMessageHistory.size() != 0 && pOrder->_qOrderMessageHistory.front() <= cgetCurrentTime() - KOEpochTime(1,0))
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
        pOrder->_qOrderMessageHistory.push_back(cgetCurrentTime());
    }

    return bResult;
}

bool QuickFixScheduler::bcheckRisk(unsigned int iProductIdx, long iNewQty)
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

}
