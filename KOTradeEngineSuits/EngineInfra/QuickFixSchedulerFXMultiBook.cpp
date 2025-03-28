#include "QuickFixSchedulerFXMultiBook.h"
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

struct LP
{
    int iLPIdx;
    int iPriceInTicks;
    long iSize;
    string sLPProductCode;
    string sExchange;
};

bool compareLPsAsk(LP LP1, LP LP2)
{
    return (LP1.iPriceInTicks < LP2.iPriceInTicks);
}

bool compareLPsBid(LP LP1, LP LP2)
{
    return (LP1.iPriceInTicks > LP2.iPriceInTicks);
}

QuickFixSchedulerFXMultiBook::QuickFixSchedulerFXMultiBook(SchedulerConfig &cfg, bool bIsLiveTrading)
:SchedulerBase("Config", cfg)
{
    _bIsLiveTrading =  bIsLiveTrading;
    _bScheduleFinished = false;

    _iNumTimerCallsReceived = 0;
    _iTotalNumMsg = 0;

    _bIsOrderSessionLoggedOn= false;

    preCommonInit();
}

QuickFixSchedulerFXMultiBook::~QuickFixSchedulerFXMultiBook()
{

}

void QuickFixSchedulerFXMultiBook::init()
{
    stringstream cStringStream;
    cStringStream << "Initialising quick fix scheduler";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    for(unsigned int i = 0; i < _cSchedulerCfg.vProducts.size(); ++i)
    {
        QuoteData* pNewQuoteDataPtr;

        if(_cSchedulerCfg.vProducts[i].find("CFX") == 0)
        {
            cStringStream.str("");
            cStringStream.clear();
            cStringStream << "Creating multi book product " << _cSchedulerCfg.vProducts[i];
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

            pNewQuoteDataPtr = pregisterProduct(_cSchedulerCfg.vProducts[i], KO_FX);
            _vProductMultiBooks.insert(std::pair<unsigned int, vector<QuoteData>>(i, vector<QuoteData>()));
        }
        else
        {
            cStringStream.str("");
            cStringStream.clear();
            cStringStream << "Creating product " << _cSchedulerCfg.vProducts[i];
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
            
            pNewQuoteDataPtr = pregisterProduct(_cSchedulerCfg.vProducts[i], KO_FUTURE);
        }

        pNewQuoteDataPtr->iCID = i;
        pNewQuoteDataPtr->iProductExpoLimit = _cSchedulerCfg.vProductExpoLimit[i];
        pNewQuoteDataPtr->sTBProduct = _cSchedulerCfg.vTBProducts[i];
        pNewQuoteDataPtr->bIsLocalProduct = _cSchedulerCfg.vIsLocalProducts[i];

        pNewQuoteDataPtr->iProductMaxRisk = _cSchedulerCfg.vProductMaxRisk[i];
        pNewQuoteDataPtr->iProductExpoLimit = _cSchedulerCfg.vProductExpoLimit[i];

        pNewQuoteDataPtr->bDataSubscribed = false;
        pNewQuoteDataPtr->bDataSubscriptionPending = false;
        pNewQuoteDataPtr->sSubscriptionRejectReason = "";

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

    for(unsigned int i = 0; i < _cSchedulerCfg.vFXSubProducts.size(); ++i)
    {
        cStringStream.str("");
        cStringStream.clear();
        cStringStream << "Creating sub product " << _cSchedulerCfg.vFXSubProducts[i];
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

        string root = _cSchedulerCfg.vFXSubProducts[i].substr(0, _cSchedulerCfg.vFXSubProducts[i].rfind("."));
        string sExchange = _cSchedulerCfg.vFXSubProducts[i].substr(_cSchedulerCfg.vFXSubProducts[i].rfind(".")+1);
        int iSubCID = -1;

        for(unsigned int iCID = 0; iCID < _vContractQuoteDatas.size(); iCID++)
        {
            if(_vContractQuoteDatas[iCID]->sTBProduct.find(root) != string::npos)
            {
                for(map<unsigned int, vector<QuoteData>>::iterator itr = _vProductMultiBooks.begin(); itr != _vProductMultiBooks.end(); itr++)
                {
                    if(itr->first == iCID)
                    {
                        // de-reference quote data object to create a copy
                        itr->second.push_back((*_vContractQuoteDatas[iCID]));
                        itr->second.back().sProduct = _cSchedulerCfg.vFXSubProducts[i];
                        itr->second.back().sTBProduct = _cSchedulerCfg.vFXSubProducts[i];
                        itr->second.back().sExchange = sExchange;
                        itr->second.back().bDataSubscribed = false;
                        itr->second.back().bDataSubscriptionPending = false;
                        itr->second.back().sSubscriptionRejectReason = "";
                        itr->second.back().bStalenessErrorTriggered = false;
                        itr->second.back().cLastUpdateTime = KOEpochTime();
                        iSubCID = itr->second.size() - 1;
                        itr->second.back().iCID = iSubCID;
                        break;
                    }
                }
                break;
            }
        }

        if(iSubCID == -1)
        {
            stringstream cLogStringStream;
            cLogStringStream << "Unable to match sub FX product" << _cSchedulerCfg.vFXSubProducts[i] << " to parent product";
            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cLogStringStream.str());
        }
    }

    postCommonInit();

    if(_cSchedulerCfg.bLogMarketData)
    {
        _cSubBookMarketDataLogger.openFile(_cSchedulerCfg.sLogPath + "/SubBookMarketDataLog.out", true, true);
    }
    else
    {
        _cSubBookMarketDataLogger.openFile(_cSchedulerCfg.sLogPath + "/SubBookMarketDataLog.out", false, true);
    }

    sortTimeEvent();
}

void QuickFixSchedulerFXMultiBook::checkProductsForPriceSubscription()
{
    for(unsigned int i = 0; i < _cSchedulerCfg.vProducts.size(); ++i)
    {
        if(_cSchedulerCfg.vProducts[i].find("CFX") == std::string::npos)
        {
            if(_vContractQuoteDatas[i]->bDataSubscribed == false && _vContractQuoteDatas[i]->bDataSubscriptionPending == false)
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

                        _vContractQuoteDatas[i]->bDataSubscriptionPending = true;
                    }
                }
            }
        }
    }

    int iParentCID;
    for(map<unsigned int, vector<QuoteData>>::iterator itr = _vProductMultiBooks.begin(); itr != _vProductMultiBooks.end(); itr++)
    {
        iParentCID = itr->first;
        int iSubCID = 0;
        for(vector<QuoteData>::iterator pSubProduct = itr->second.begin(); pSubProduct != itr->second.end(); pSubProduct++)
        {
            if(pSubProduct->bDataSubscribed == false && pSubProduct->bDataSubscriptionPending == false)
            {
                string sTBExchange = "";
                sTBExchange = pSubProduct->sExchange;
        
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
                    cStringStream << "Cannot find market data fix session for " << pSubProduct->sExchange;
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                }
                else
                {
                    if(bIsLoggedOn == true)
                    {
                        FIX::Message message;
                        message.getHeader().setField(8, "FIX.4.4");
                        message.getHeader().setField(49, sSenderCompID);
                        message.getHeader().setField(56, "TBRICKS");
                        message.getHeader().setField(35, "V");

                        message.setField(22, "9");

                        message.setField(48, pSubProduct->sTBProduct);
                        message.setField(15, _vContractQuoteDatas[iParentCID]->sCurrency);

                        stringstream cStringStream;
                        cStringStream << "_" << iParentCID << "_" << iSubCID;
                        message.setField(262, cStringStream.str());
                        message.setField(55, _cSchedulerCfg.vProducts[iParentCID]);
                    
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
                        cLogStringStream << "Sending market data subscription request for sub FX product " << pSubProduct->sTBProduct;
                        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _cSchedulerCfg.vProducts[iParentCID], cLogStringStream.str());

                        pSubProduct->bDataSubscriptionPending = true;
                    }
                }
            }
            iSubCID = iSubCID + 1;
        }
    }
}

KOEpochTime QuickFixSchedulerFXMultiBook::cgetCurrentTime()
{
    uint64_t iMicrosecondsSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return KOEpochTime((long)iMicrosecondsSinceEpoch / 1000000, iMicrosecondsSinceEpoch % 1000000);
}

bool QuickFixSchedulerFXMultiBook::bisLiveTrading()
{
    return _bIsLiveTrading;
}

bool QuickFixSchedulerFXMultiBook::bschedulerFinished()
{
    return _bScheduleFinished;
}

bool QuickFixSchedulerFXMultiBook::sendToExecutor(const string& sProduct, long iDesiredPos)
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
                iOpenOrderQty = iOpenOrderQty + (*pOrderList)[i]->igetOrderRemainQty();
            }

            if(iPosDelta - iOpenOrderQty != 0)
            {
                submitOrderBestPriceMultiBook(iProductIdx, iPosDelta - iOpenOrderQty, false);
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

bool QuickFixSchedulerFXMultiBook::sendToLiquidationExecutor(const string& sProduct, long iDesiredPos)
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
                iOpenOrderQty = iOpenOrderQty + (*pOrderList)[i]->igetOrderRemainQty();
            }

            if(iPosDelta - iOpenOrderQty != 0)
            {
                submitOrderBestPriceMultiBook(iProductIdx, iPosDelta - iOpenOrderQty, true);
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

void QuickFixSchedulerFXMultiBook::assignPositionToLiquidator(const string& sProduct, long iPosToLiquidate)
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

void QuickFixSchedulerFXMultiBook::exitScheduler()
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

void QuickFixSchedulerFXMultiBook::submitOrderBestPriceMultiBook(unsigned int iProductIdx, long iQty, bool bIsLiquidation)
{
    bool bIsIOC = true;

    if(bcheckRisk(iProductIdx, iQty) == false)
    {
        return;
    }

    if(bIsIOC == true)
    {
        if(_vContractQuoteDatas[iProductIdx]->iBestAskInTicks - _vContractQuoteDatas[iProductIdx]->iBestBidInTicks > 25)
        {
            stringstream cStringStream;
            cStringStream.precision(10);
            cStringStream << "Ignore new Order submit. Spread width greater than 25.";
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

    long iOrderPriceInTicks = icalcualteOrderPrice(iProductIdx, 0, iQty, false, bIsLiquidation, bIsIOC);

    for(map<unsigned int, vector<QuoteData>>::iterator itr = _vProductMultiBooks.begin(); itr != _vProductMultiBooks.end(); itr++)
    {
        long iQtyToBeAllocated = iQty;
        if(itr->first == iProductIdx)
        {
            if(iQtyToBeAllocated > 0)
            {
                int index = 0;
                vector<LP> vLPs;
                for(vector<QuoteData>::iterator subQuoteItr = itr->second.begin(); subQuoteItr != itr->second.end(); subQuoteItr++)
                {
                    string sECN = subQuoteItr->sExchange;

                    bool bIgnoreLP = false;
                    for(vector<string>::iterator itrLP = _vForbiddenFXLPs.begin(); itrLP != _vForbiddenFXLPs.end(); itrLP++)
                    {
                        if(sECN == (*itrLP))
                        {
                            bIgnoreLP = true;
                            break;
                        }
                    }

                    if(bIgnoreLP == false && subQuoteItr->bStalenessErrorTriggered == false)
                    {
                        stringstream cStringStreamPrice;
                        cStringStreamPrice.precision(10);
                        cStringStreamPrice << sECN << " Snapshot:" << (*subQuoteItr).iBidSize << "|" << (*subQuoteItr).iBestBidInTicks << "|" << (*subQuoteItr).iBestAskInTicks << "|" << (*subQuoteItr).iAskSize;
                        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", (*subQuoteItr).sProduct, cStringStreamPrice.str());

                        vLPs.push_back(LP());
                        vLPs.back().iLPIdx = index;
                        vLPs.back().iPriceInTicks = (*subQuoteItr).iBestAskInTicks;
                        vLPs.back().iSize = (*subQuoteItr).iAskSize;
                        vLPs.back().sLPProductCode = (*subQuoteItr).sTBProduct;
                        vLPs.back().sExchange = sECN;
                    }

                    index = index + 1;
                }
                sort(vLPs.begin(), vLPs.end(), compareLPsAsk);

                for(vector<LP>::iterator LPItr = vLPs.begin(); LPItr != vLPs.end(); LPItr++)
                {
                    long iOrderSize;
                    int iActualOrderPrice;
                    string sLPProductCode = LPItr->sLPProductCode;
                    string sExchange = LPItr->sExchange;

                    if(iOrderPriceInTicks + 1 >= LPItr->iPriceInTicks)
                    {
                        iActualOrderPrice = LPItr->iPriceInTicks;

                        if(iQtyToBeAllocated > LPItr->iSize)
                        {
                            iOrderSize = LPItr->iSize;
                            iQtyToBeAllocated = iQtyToBeAllocated - iOrderSize;
                        }
                        else
                        {
                            iOrderSize = iQtyToBeAllocated;
                            iQtyToBeAllocated = 0;
                        }

                        if(iOrderSize != 0)
                        {
                            KOOrderPtr pOrder;
                            string sPendingOrderID = sgetNextFixOrderID();
                            pOrder.reset(new KOOrder(sPendingOrderID, iProductIdx, _vContractQuoteDatas[iProductIdx]->dTickSize, bIsIOC, _vContractQuoteDatas[iProductIdx]->sProduct, _vContractQuoteDatas[iProductIdx]->sProduct, _vContractQuoteDatas[iProductIdx]->sExchange, _vContractQuoteDatas[iProductIdx]->eInstrumentType, NULL));

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
                            message.setField(48, sLPProductCode);
                            message.setField(22, "9");

                            if(iOrderSize > 0)
                            {
                                message.setField(54, "1");
                            }
                            else if(iOrderSize < 0)
                            {
                                message.setField(54, "2");
                            }

                            message.setField(207, "XXXX");

                            stringstream cQtyStream;
                            cQtyStream << abs(iOrderSize);
                            message.setField(38, cQtyStream.str());

                            double dOrderPrice = iActualOrderPrice * _vContractQuoteDatas[iProductIdx]->dTickSize;
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
                                cStringStream << "Submitting new order " << pOrder->sgetPendingOrderID() << ". qty " << iQty << " price " << dOrderPrice << " to " << sExchange << ".";
                            }
                            else
                            {
                                cStringStream << "Submitting new liquidation order " << pOrder->sgetPendingOrderID() << ". qty " << iQty << " price " << dOrderPrice << " to " << sExchange << ".";
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

                        if(iQtyToBeAllocated == 0)
                        {
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else if(iQtyToBeAllocated < 0)
            {
                int index = 0;
                vector<LP> vLPs;
                for(vector<QuoteData>::iterator subQuoteItr = itr->second.begin(); subQuoteItr != itr->second.end(); subQuoteItr++)
                {
                    string sECN = subQuoteItr->sExchange;

                    bool bIgnoreLP = false;
                    for(vector<string>::iterator itrLP = _vForbiddenFXLPs.begin(); itrLP != _vForbiddenFXLPs.end(); itrLP++)
                    {
                        if(sECN == (*itrLP))
                        {
                            bIgnoreLP = true;
                            break;
                        }
                    }

                    if(bIgnoreLP == false && subQuoteItr->bStalenessErrorTriggered == false)
                    {
                        stringstream cStringStreamPrice;
                        cStringStreamPrice.precision(10);
                        cStringStreamPrice << sECN << " Snapshot:" << subQuoteItr->iBidSize << "|" << subQuoteItr->iBestBidInTicks << "|" << subQuoteItr->iBestAskInTicks << "|" << subQuoteItr->iAskSize;
                        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", (*subQuoteItr).sProduct, cStringStreamPrice.str());

                        vLPs.push_back(LP());
                        vLPs.back().iLPIdx = index;
                        vLPs.back().iPriceInTicks = subQuoteItr->iBestBidInTicks;
                        vLPs.back().iSize = subQuoteItr->iBidSize;
                        vLPs.back().sLPProductCode = (*subQuoteItr).sTBProduct;
                        vLPs.back().sExchange = sECN;
                    }

                    index = index + 1;
                }
                sort(vLPs.begin(), vLPs.end(), compareLPsBid);

                for(vector<LP>::iterator LPItr = vLPs.begin(); LPItr != vLPs.end(); LPItr++)
                {
                    long iOrderSize;
                    int iActualOrderPrice;
                    string sLPProductCode = LPItr->sLPProductCode;
                    string sExchange = LPItr->sExchange;

                    if(iOrderPriceInTicks - 1 <= LPItr->iPriceInTicks)
                    {
                        iActualOrderPrice = LPItr->iPriceInTicks;

                        if(iQtyToBeAllocated * -1 > LPItr->iSize)
                        {
                            iOrderSize = LPItr->iSize * -1;
                            iQtyToBeAllocated = iQtyToBeAllocated - iOrderSize;
                        }
                        else
                        {
                            iOrderSize = iQtyToBeAllocated;
                            iQtyToBeAllocated = 0;
                        }

                        if(iOrderSize != 0)
                        {
                            KOOrderPtr pOrder;
                            string sPendingOrderID = sgetNextFixOrderID();
                            pOrder.reset(new KOOrder(sPendingOrderID, iProductIdx, _vContractQuoteDatas[iProductIdx]->dTickSize, bIsIOC, _vContractQuoteDatas[iProductIdx]->sProduct, _vContractQuoteDatas[iProductIdx]->sProduct, _vContractQuoteDatas[iProductIdx]->sExchange, _vContractQuoteDatas[iProductIdx]->eInstrumentType, NULL));

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
                            message.setField(48, sLPProductCode);
                            message.setField(22, "9");

                            if(iOrderSize > 0)
                            {
                                message.setField(54, "1");
                            }
                            else if(iOrderSize < 0)
                            {
                                message.setField(54, "2");
                            }

                            message.setField(207, "XXXX");

                            stringstream cQtyStream;
                            cQtyStream << abs(iOrderSize);
                            message.setField(38, cQtyStream.str());

                            double dOrderPrice = iActualOrderPrice * _vContractQuoteDatas[iProductIdx]->dTickSize;
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
                                message.setField(59, "0"); // TODO: need to test if orders get cancelled when the engine dies
                            }

                            stringstream cStringStream;
                            cStringStream.precision(10);
                            if(bIsLiquidation == false)
                            {
                                cStringStream << "Submitting new order " << pOrder->sgetPendingOrderID() << ". qty " << iQty << " price " << dOrderPrice << " to " << sExchange << ".";
                            }
                            else
                            {
                                cStringStream << "Submitting new liquidation order " << pOrder->sgetPendingOrderID() << ". qty " << iQty << " price " << dOrderPrice << " to " << sExchange << ".";
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

                        if(iQtyToBeAllocated == 0)
                        {
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
    }
}

long QuickFixSchedulerFXMultiBook::icalcualteOrderPrice(unsigned int iProductIdx, long iOrderPrice, long iQty, bool bOrderInMarket, bool bIsLiquidation, bool bIsIOC)
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

void QuickFixSchedulerFXMultiBook::resetOrderState()
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

void QuickFixSchedulerFXMultiBook::updateAllPnL()
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

void QuickFixSchedulerFXMultiBook::onTimer()
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

    for(map<unsigned int, vector<QuoteData>>::iterator itr = _vProductMultiBooks.begin(); itr != _vProductMultiBooks.end(); itr++)
    {
        for(vector<QuoteData>::iterator pSubProduct = itr->second.begin(); pSubProduct != itr->second.end(); pSubProduct++)
        {
            if(pSubProduct->cLastUpdateTime.sec() != 0)
            {
                if((cNewUpdateTime - pSubProduct->cLastUpdateTime).sec() > 300)
                {
                    if(pSubProduct->bStalenessErrorTriggered == false)
                    {
                        pSubProduct->bStalenessErrorTriggered = true;
                        stringstream cStringStream;
                        cStringStream << "FX pricing on " << pSubProduct->sProduct << " stopped";
                        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                    }
                }
            }
        }
    }

    if(cNewUpdateTime.sec() % 2 == 0)
    {
        _cSubBookMarketDataLogger.flush();
    }
}

void QuickFixSchedulerFXMultiBook::onCreate(const SessionID& cSessionID)
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

void QuickFixSchedulerFXMultiBook::onLogon(const SessionID& cSessionID)
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

void QuickFixSchedulerFXMultiBook::onLogout(const SessionID& cSessionID)
{
    string sSenderCompID = cSessionID.getSenderCompID();

    if(sSenderCompID.find("MD") != std::string::npos)
    {
        for(int i = 0; i < _vMDSessions.size(); i++)
        {
            if(_vMDSessions[i].sSenderCompID == sSenderCompID)
            {
                if(_vMDSessions[i].bIsLoggedOn == true)
                {
                    _vMDSessions[i].bIsLoggedOn = false;
                    updateQuoteDataSubscribed();

                    stringstream cStringStream;
                    cStringStream << "Disconnected from market data fix session " << sSenderCompID;
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                }

                stringstream cStringStream;
                cStringStream << "Disconnected from market data fix session " << sSenderCompID;
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                break;
            }
        }
    }
    else if(sSenderCompID.find("TR") != std::string::npos)
    {
        if(_bIsOrderSessionLoggedOn == true)
        {
            stringstream cStringStream;
            cStringStream << "Disconnected from order fix session " << _sOrderSenderCompID;
            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
        }

        _bIsOrderSessionLoggedOn = false;
        stringstream cStringStream;
        cStringStream << "Disconnected from order fix session " << _sOrderSenderCompID;
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void QuickFixSchedulerFXMultiBook::updateQuoteDataSubscribed()
{
    for(unsigned int i = 0; i < _cSchedulerCfg.vProducts.size(); ++i)
    {
        if(_cSchedulerCfg.vProducts[i].find("CFX") == std::string::npos)
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

            if(pMarketDataSessionID != NULL)
            {
                if(bIsLoggedOn == false)
                {
                    _vContractQuoteDatas[i]->bDataSubscribed = false;
                }
            }
        }
    }

    int iParentCID;
    for(map<unsigned int, vector<QuoteData>>::iterator itr = _vProductMultiBooks.begin(); itr != _vProductMultiBooks.end(); itr++)
    {
        for(vector<QuoteData>::iterator pSubProduct = itr->second.begin(); pSubProduct != itr->second.end(); pSubProduct++)
        {
            string sTBExchange = "";
            sTBExchange = pSubProduct->sExchange;

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

            if(pMarketDataSessionID != NULL)
            {
                if(bIsLoggedOn == false)
                {
                    pSubProduct->bDataSubscribed = false;
                }
            }
        }
    }
}

void QuickFixSchedulerFXMultiBook::toAdmin(Message& cMessage, const SessionID& cSessionID)
{
    crack(cMessage, cSessionID);
}

void QuickFixSchedulerFXMultiBook::toApp(Message& cMessage, const SessionID& cSessionID) throw(DoNotSend)
{
    crack(cMessage, cSessionID);
}

void QuickFixSchedulerFXMultiBook::fromAdmin(const Message& cMessage, const SessionID& cSessionID) throw(FieldNotFound, IncorrectDataFormat, IncorrectTagValue, RejectLogon)
{
    crack(cMessage, cSessionID);
}

void QuickFixSchedulerFXMultiBook::fromApp(const Message& cMessage, const SessionID& cSessionID) throw(FieldNotFound, IncorrectDataFormat, IncorrectTagValue, UnsupportedMessageType)
{
    crack(cMessage, cSessionID);
}

void QuickFixSchedulerFXMultiBook::onMessage(const FIX44::Logout& cLogout, const FIX::SessionID& cSessionID)
{
    string sSenderCompID = cSessionID.getSenderCompID();

    if(sSenderCompID.find("MD") != std::string::npos)
    {
        for(int i = 0; i < _vMDSessions.size(); i++)
        {
            if(_vMDSessions[i].sSenderCompID == sSenderCompID)
            {
                if(_vMDSessions[i].bIsLoggedOn == true)
                {
                    _vMDSessions[i].bIsLoggedOn = false;
                    updateQuoteDataSubscribed();

                    stringstream cStringStream;
                    cStringStream << "Disconnected from market data fix session " << sSenderCompID;
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                }

                stringstream cStringStream;
                cStringStream << "Disconnected from market data fix session " << sSenderCompID;
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                break;
            }
        }
    }
    else if(sSenderCompID.find("TR") != std::string::npos)
    {
        if(_bIsOrderSessionLoggedOn == true)
        {
            stringstream cStringStream;
            cStringStream << "Disconnected from order fix session " << _sOrderSenderCompID;
            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
        }

        _bIsOrderSessionLoggedOn = false;
        stringstream cStringStream;
        cStringStream << "Disconnected from order fix session " << _sOrderSenderCompID;
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void QuickFixSchedulerFXMultiBook::onMessage(FIX44::Reject& cReject, const FIX::SessionID& cSessionID)
{
    FIX::Text cText;
    cReject.get(cText);

    string sSenderCompID = cSessionID.getSenderCompID();
    if(sSenderCompID.find("MD") != std::string::npos)
    {
        stringstream cStringStream;
        cStringStream << "Invalid market data fix message. Reason: " << cText;
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
    else if(sSenderCompID.find("TR") != std::string::npos)
    {
        stringstream cStringStream;
        cStringStream << "Invalid order fix message. Reason: " << cText;
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void QuickFixSchedulerFXMultiBook::onMessage(const FIX44::MarketDataRequestReject& cReject, const FIX::SessionID& cSessionID)
{
    FIX::Text cText;
    cReject.get(cText);

    string sClientRef = cReject.getField(262).c_str();

    if(sClientRef.c_str()[0] == '_')
    {
        std::istringstream cIDStream (sClientRef);

        string sParentCID;
        std::getline(cIDStream, sParentCID, '_');
        std::getline(cIDStream, sParentCID, '_');
        int iCID = atoi(sParentCID.c_str());

        string sSubCID;
        std::getline(cIDStream, sSubCID, '_');
        int iSubCID = atoi(sSubCID.c_str());

        for(map<unsigned int, vector<QuoteData>>::iterator itr = _vProductMultiBooks.begin(); itr != _vProductMultiBooks.end(); itr++)
        {
            if(itr->first == iCID)
            {
                for(vector<QuoteData>::iterator subQuoteItr = itr->second.begin(); subQuoteItr != itr->second.end(); subQuoteItr++)
                {
                    if(iSubCID == subQuoteItr->iCID)
                    {
                        subQuoteItr->bDataSubscriptionPending = false;
                        if(subQuoteItr->sSubscriptionRejectReason != cText)
                        {
                            subQuoteItr->sSubscriptionRejectReason = cText;
                            stringstream cStringStream;
                            cStringStream << "Market Data request " << subQuoteItr->sProduct << " rejected. Reason: " << cText;
                            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                        }
                        stringstream cStringStream;
                        cStringStream << "Market Data request " << subQuoteItr->sProduct << " rejected. Reason: " << cText;
                        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                    }
                }
            }
        }
    }
    else
    {
        int iCID = atoi(sClientRef.c_str());

        _vContractQuoteDatas[iCID]->bDataSubscriptionPending = false;
        if(_vContractQuoteDatas[iCID]->sSubscriptionRejectReason != cText)
        {
            _vContractQuoteDatas[iCID]->sSubscriptionRejectReason = cText;
            stringstream cStringStream;
            cStringStream << "Market Data request " << _vContractQuoteDatas[iCID]->sProduct << " rejected. Reason: " << cText;
            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
        }
        stringstream cStringStream;
        cStringStream << "Market Data request " << _vContractQuoteDatas[iCID]->sProduct << " rejected. Reason: " << cText;
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void QuickFixSchedulerFXMultiBook::onMessage(const FIX44::ExecutionReport& cExecutionReport, const FIX::SessionID& cSessionID)
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

            string sTBProduct = cExecutionReport.getField(48);
            string sExchange = sTBProduct.substr(sTBProduct.rfind(".")+1);

            stringstream cStringStream;
            if(bIsLiquidationOrder == true)
            {
                cStringStream << "Liquidation order Filled - Order confirmed ID: " << pOrderToBeUpdated->_sConfirmedOrderID << " pending ID: " << pOrderToBeUpdated->_sPendingOrderID << " TB ID: " << pOrderToBeUpdated->_sTBOrderID << " qty: " << iFillQty << " price: " << dFillPrice << " from" << sExchange << ".";
            }
            else
            {
                cStringStream << "Order Filled - Order confirmed ID: " << pOrderToBeUpdated->_sConfirmedOrderID << " pending ID: " << pOrderToBeUpdated->_sPendingOrderID << " TB ID: " << pOrderToBeUpdated->_sTBOrderID << " qty: " << iFillQty << " price: " << dFillPrice << " from " << sExchange << ".";
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
                string sTBProduct = cExecutionReport.getField(48);
                string sExchange = sTBProduct.substr(sTBProduct.rfind(".")+1);

                stringstream cStringStream;
                cStringStream.precision(10);
                cStringStream << "Order cancel acked - Order confirmed ID: " << pOrderToBeUpdated->_sConfirmedOrderID << " pending ID: " << pOrderToBeUpdated->_sPendingOrderID << " TB ID: " << pOrderToBeUpdated->_sTBOrderID << " from " << sExchange << ".";
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
                cStringStream << "Unkown last order state " << charOrderStatus << " for order TB ID " << sTBOrderID;

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

        if(_vLastOrderError[iProductIdx] != cStringStream.str())
        {
            _vLastOrderError[iProductIdx] = cStringStream.str();
            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
        }
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
    }
    
}

void QuickFixSchedulerFXMultiBook::onMessage(const FIX44::MarketDataSnapshotFullRefresh& cMarketDataSnapshotFullRefresh, const FIX::SessionID& cSessionID)
{
    string sClientRef = cMarketDataSnapshotFullRefresh.getField(262);
        
    int iNumEntries;
    iNumEntries = atoi(cMarketDataSnapshotFullRefresh.getField(268).c_str());

    FIX44::MarketDataSnapshotFullRefresh::NoMDEntries group;
    FIX::MDEntryType MDEntryType;
    FIX::MDEntryPx MDEntryPx;
    FIX::MDEntrySize MDEntrySize;
    FIX::Text MDText;

    long iProdBidSize = -1;
    double dProdBestBid = -1;
    double dProdBestAsk = -1;
    long iProdAskSize = -1;

    double dCombBestBid = 0;
    double dCombBestAsk = 99999999.0;
    int iCombBestBidInTicks = 0;
    int iCombBestAskInTicks = 999999999;
    int iCombBidSize = 0;
    int iCombAskSize = 0;

    double dLastTrade;
    long iLastTradeSize;

    bool bTradeUpdate = false;
    bool bSubscriptionReset = false;   
 
    for(int i = 0; i < iNumEntries; i++)
    {
        cMarketDataSnapshotFullRefresh.getGroup(i+1, group);

        if(group.isSetField(58) == true)
        {
            group.get(MDText);
            if(MDText == "Source unavailable")
            {
                bSubscriptionReset = true;
                break;
            }
        }

        group.get(MDEntryType);
        group.get(MDEntryPx);
        group.get(MDEntrySize);

        if(MDEntryType == '0')
        {
            iProdBidSize = MDEntrySize;
            dProdBestBid = MDEntryPx;
        }
        else if(MDEntryType == '1')
        {
            iProdAskSize = MDEntrySize;
            dProdBestAsk = MDEntryPx;
        }
        else if(MDEntryType == '2')
        {
            bTradeUpdate = true;
            iLastTradeSize = MDEntrySize;
            dLastTrade = MDEntryPx;
        }
    }


    int iCID;
    if(sClientRef.c_str()[0] == '_')
    {
        std::istringstream cIDStream (sClientRef);

        string sParentCID;
        std::getline(cIDStream, sParentCID, '_');
        std::getline(cIDStream, sParentCID, '_');
        iCID = atoi(sParentCID.c_str());

        string sSubCID;
        std::getline(cIDStream, sSubCID, '_');
        int iSubCID = atoi(sSubCID.c_str());

        bool bIgnoreLP = false;
        for(map<unsigned int, vector<QuoteData>>::iterator itr = _vProductMultiBooks.begin(); itr != _vProductMultiBooks.end(); itr++)
        {
            if(itr->first == iCID)
            {
                for(vector<QuoteData>::iterator subQuoteItr = itr->second.begin(); subQuoteItr != itr->second.end(); subQuoteItr++)
                {
                    if(iSubCID == subQuoteItr->iCID)
                    {
                        if(bSubscriptionReset == true)
                        {
                            subQuoteItr->bDataSubscribed = false;
                            subQuoteItr->bDataSubscriptionPending = false;
                            subQuoteItr->sSubscriptionRejectReason = "";

                            stringstream cStringStream;
                            cStringStream << "Tbrick data pricing on " << subQuoteItr->sProduct << " stopped!";
                            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());

                            subQuoteItr->dBestBid = 0;
                            subQuoteItr->iBestBidInTicks = 0;
                            subQuoteItr->iBidSize = 0;

                            subQuoteItr->dBestAsk = 999999.0;
                            subQuoteItr->iBestAskInTicks = 999999999;
                            subQuoteItr->iAskSize = 0;
                        }
                        else
                        {
                            subQuoteItr->bDataSubscribed = true;
                            subQuoteItr->bDataSubscriptionPending = false;
                            subQuoteItr->sSubscriptionRejectReason = "";

                            string sECN = subQuoteItr->sExchange;
                            for(vector<string>::iterator itrLP = _vForbiddenFXLPs.begin(); itrLP != _vForbiddenFXLPs.end(); itrLP++)
                            {
                                if(sECN == (*itrLP))
                                {
                                    bIgnoreLP = true;
                                    break;
                                }
                            }

                            if(bIgnoreLP == false)
                            {
                                if(iProdBidSize > 0)
                                {
                                    subQuoteItr->dBestBid = dProdBestBid;
                                    subQuoteItr->iBestBidInTicks = boost::math::iround(subQuoteItr->dBestBid / subQuoteItr->dTickSize);
                                    subQuoteItr->iBidSize = iProdBidSize;
                                }

                                if(iProdAskSize > 0)
                                {
                                    subQuoteItr->dBestAsk = dProdBestAsk;
                                    subQuoteItr->iBestAskInTicks = boost::math::iround(subQuoteItr->dBestAsk / subQuoteItr->dTickSize);
                                    subQuoteItr->iAskSize = iProdAskSize;
                                }

                                subQuoteItr->cLastUpdateTime = cgetCurrentTime();

                                if(subQuoteItr->bStalenessErrorTriggered == true)
                                {
                                    stringstream cStringStream;
                                    cStringStream << "FX pricing on " << subQuoteItr->sProduct << " resumed";
                                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());

                                    subQuoteItr->bStalenessErrorTriggered = false;
                                }

                                _cSubBookMarketDataLogger << cgetCurrentTime().igetPrintable()
                                                          << "|" << (*subQuoteItr).sProduct
                                                          << "|" << (*subQuoteItr).iBidSize
                                                          << "|" << (*subQuoteItr).iBestBidInTicks
                                                          << "|" << (*subQuoteItr).iBestAskInTicks
                                                          << "|" << (*subQuoteItr).iAskSize << "\n";
                            }
                            else
                            {
                                subQuoteItr->dBestBid = 0;
                                subQuoteItr->iBestBidInTicks = 0;
                                subQuoteItr->iBidSize = 0;

                                subQuoteItr->dBestAsk = 999999.0;
                                subQuoteItr->iBestAskInTicks = 999999999;
                                subQuoteItr->iAskSize = 0;

                                _cSubBookMarketDataLogger << cgetCurrentTime().igetPrintable()
                                                          << "|Ignoring price update from forbidden LP " << sECN << "\n";
                            }
                        }
                    }

                    if(subQuoteItr->iBidSize > 0 && subQuoteItr->iAskSize > 0 && subQuoteItr->bStalenessErrorTriggered == false)
                    {
                        if(subQuoteItr->iBestBidInTicks > iCombBestBidInTicks)
                        {
                            iCombBestBidInTicks = subQuoteItr->iBestBidInTicks;
                            dCombBestBid = subQuoteItr->dBestBid;
                            iCombBidSize = subQuoteItr->iBidSize;
                        }
                        else if(subQuoteItr->iBestBidInTicks == iCombBestBidInTicks)
                        {
                            iCombBidSize = iCombBidSize + subQuoteItr->iBidSize;
                        }

                        if(subQuoteItr->iBestAskInTicks < iCombBestAskInTicks)
                        {
                            iCombBestAskInTicks = subQuoteItr->iBestAskInTicks;
                            dCombBestAsk = subQuoteItr->dBestAsk;
                            iCombAskSize = subQuoteItr->iAskSize;
                        }
                        else if(subQuoteItr->iBestAskInTicks == iCombBestAskInTicks)
                        {
                            iCombAskSize = iCombAskSize + subQuoteItr->iAskSize;
                        }
                    }
                }

                break;
            }
        }
    }
    else
    {
        iCID = atoi(cMarketDataSnapshotFullRefresh.getField(262).c_str());

        if(bSubscriptionReset == true)
        {
            _vContractQuoteDatas[iCID]->bDataSubscribed = false;
            _vContractQuoteDatas[iCID]->bDataSubscriptionPending = false;
            _vContractQuoteDatas[iCID]->sSubscriptionRejectReason = "";
        }
        else
        {
            _vContractQuoteDatas[iCID]->bDataSubscribed = true;
            _vContractQuoteDatas[iCID]->bDataSubscriptionPending = false;
            _vContractQuoteDatas[iCID]->sSubscriptionRejectReason = "";

            dCombBestBid = dProdBestBid;
            dCombBestAsk = dProdBestAsk;
            iCombBidSize = iProdBidSize;
            iCombAskSize = iProdAskSize;

            iCombBestBidInTicks = boost::math::iround(dCombBestBid / _vContractQuoteDatas[iCID]->dTickSize);
            iCombBestAskInTicks = boost::math::iround(dCombBestAsk / _vContractQuoteDatas[iCID]->dTickSize);
        }
    }


    if(bTradeUpdate == false)
    {
        _vContractQuoteDatas[iCID]->iTradeSize = 0;

        _vContractQuoteDatas[iCID]->iPrevBidInTicks = _vContractQuoteDatas[iCID]->iBestBidInTicks;
        _vContractQuoteDatas[iCID]->iPrevAskInTicks = _vContractQuoteDatas[iCID]->iBestAskInTicks;
        _vContractQuoteDatas[iCID]->iPrevBidSize = _vContractQuoteDatas[iCID]->iBidSize;
        _vContractQuoteDatas[iCID]->iPrevAskSize = _vContractQuoteDatas[iCID]->iAskSize;

        _vContractQuoteDatas[iCID]->dBestBid = dCombBestBid;
        _vContractQuoteDatas[iCID]->iBestBidInTicks = iCombBestBidInTicks;
        _vContractQuoteDatas[iCID]->iBidSize = iCombBidSize;

        _vContractQuoteDatas[iCID]->dBestAsk = dCombBestAsk;
        _vContractQuoteDatas[iCID]->iBestAskInTicks = iCombBestAskInTicks;
        _vContractQuoteDatas[iCID]->iAskSize = iCombAskSize;
    }
    else
    {
        _vContractQuoteDatas[iCID]->dLastTradePrice = dLastTrade;
        _vContractQuoteDatas[iCID]->iLastTradeInTicks = boost::math::iround(_vContractQuoteDatas[iCID]->dLastTradePrice / _vContractQuoteDatas[iCID]->dTickSize);
        _vContractQuoteDatas[iCID]->iTradeSize = iLastTradeSize;
        _vContractQuoteDatas[iCID]->iAccumuTradeSize = _vContractQuoteDatas[iCID]->iAccumuTradeSize + iLastTradeSize;
    }

    if(_vContractQuoteDatas[iCID]->iBidSize != 0 && _vContractQuoteDatas[iCID]->iAskSize != 0)
    {
        if(_vContractQuoteDatas[iCID]->iPrevBidInTicks != _vContractQuoteDatas[iCID]->iBestBidInTicks || _vContractQuoteDatas[iCID]->iPrevAskInTicks != _vContractQuoteDatas[iCID]->iBestAskInTicks || _vContractQuoteDatas[iCID]->iPrevBidSize != _vContractQuoteDatas[iCID]->iBidSize || _vContractQuoteDatas[iCID]->iPrevAskSize != _vContractQuoteDatas[iCID]->iAskSize || iLastTradeSize != 0)
        {
            double dWeightedMidInTicks;
            if(_vContractQuoteDatas[iCID]->eInstrumentType == KO_FX)
            {
                dWeightedMidInTicks = (double)(_vContractQuoteDatas[iCID]->iBestAskInTicks + _vContractQuoteDatas[iCID]->iBestBidInTicks) / 2;
            }
            else
            {
                if(_vContractQuoteDatas[iCID]->iBestAskInTicks - _vContractQuoteDatas[iCID]->iBestBidInTicks != 1 || (_vContractQuoteDatas[iCID]->iBidSize + _vContractQuoteDatas[iCID]->iAskSize == 0))
                {
                    dWeightedMidInTicks = (double)(_vContractQuoteDatas[iCID]->iBestAskInTicks + _vContractQuoteDatas[iCID]->iBestBidInTicks) / 2;
                }
                else
                {
                    dWeightedMidInTicks = (double)_vContractQuoteDatas[iCID]->iBestBidInTicks + (double)_vContractQuoteDatas[iCID]->iBidSize / (double)(_vContractQuoteDatas[iCID]->iBidSize + _vContractQuoteDatas[iCID]->iAskSize);
                }
            }

            _vContractQuoteDatas[iCID]->dWeightedMidInTicks = dWeightedMidInTicks;
            _vContractQuoteDatas[iCID]->dWeightedMid = dWeightedMidInTicks * _vContractQuoteDatas[iCID]->dTickSize;
            newPriceUpdate(iCID);
        }
    }
}

void QuickFixSchedulerFXMultiBook::onMessage(FIX44::BusinessMessageReject& cBusinessMessageReject, const FIX::SessionID& cSessionID)
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

void QuickFixSchedulerFXMultiBook::onMessage(const FIX44::BusinessMessageReject& cBusinessMessageReject, const FIX::SessionID& cSessionID)
{
    FIX::Text cText;
    cBusinessMessageReject.get(cText);
    string sText = cText;

    string sSenderCompID = cSessionID.getSenderCompID();

    FIX::RefMsgType cRefMsgType;
    cBusinessMessageReject.get(cRefMsgType);

    if(cRefMsgType ==  "V")
    {
        stringstream cStringStream;
        cStringStream << "Unable to register market data for session " << cSessionID.getSenderCompID() << ". Reason: " << cText;
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());

        for(unsigned int i = 0; i < _cSchedulerCfg.vProducts.size(); ++i)
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

            if(sSenderCompID.find(sTBExchange) != std::string::npos)
            {
                _vContractQuoteDatas[i]->bDataSubscriptionPending = false;
            }
        }

        for(map<unsigned int, vector<QuoteData>>::iterator itr = _vProductMultiBooks.begin(); itr != _vProductMultiBooks.end(); itr++)
        {
            for(vector<QuoteData>::iterator subQuoteItr = itr->second.begin(); subQuoteItr != itr->second.end(); subQuoteItr++)
            {
                if(sSenderCompID.find(subQuoteItr->sExchange) != std::string::npos)
                {
                    subQuoteItr->bDataSubscriptionPending = false;
                }
            }
        }
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

void QuickFixSchedulerFXMultiBook::onMessage(const FIX44::OrderCancelReject& cOrderCancelReject, const FIX::SessionID& cSessionID)
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
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
        }
    }
}

void QuickFixSchedulerFXMultiBook::checkOrderState(KOEpochTime cCurrentTime)
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

bool QuickFixSchedulerFXMultiBook::bcheckOrderMsgHistory(KOOrderPtr pOrder)
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

bool QuickFixSchedulerFXMultiBook::bcheckRisk(unsigned int iProductIdx, long iNewQty)
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
