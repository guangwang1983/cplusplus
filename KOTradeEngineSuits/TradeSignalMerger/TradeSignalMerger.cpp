#include "TradeSignalMerger.h"
#include <boost/math/special_functions/round.hpp>
#include "../EngineInfra/SystemClock.h"
#include "../EngineInfra/SchedulerBase.h"
#include "../EngineInfra/ErrorHandler.h"

using namespace std;

namespace KO
{

TradeSignalMerger::TradeSignalMerger(SchedulerBase* pScheduler)
{
    _pScheduler = pScheduler;
    _iNextSlotID = 0;
}

TradeSignalMerger::~TradeSignalMerger()
{

}

void TradeSignalMerger::printConfigPos()
{
    for(map<string, bool>::iterator itr = _mProductPrintPos.begin();
        itr != _mProductPrintPos.end();
        itr++)
    {
        itr->second = true;
    }
}

void TradeSignalMerger::takePosSnapShot()
{
    _cInstradayPosSnapshot << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();

    for(map<string, vector<SlotSignal>>::iterator itr = _mSlotSignals.begin();
        itr != _mSlotSignals.end();
        itr++)
    {
        string sProduct = itr->first;
        long iTotalPos = 0;

        for(vector<SlotSignal>::iterator slotItr = itr->second.begin();
            slotItr != itr->second.end();
            slotItr++)
        {
            iTotalPos = iTotalPos + (*slotItr).iPos;
        }   

        _cInstradayPosSnapshot << ";" << sProduct << ":" << iTotalPos; 
    }    

    _cInstradayPosSnapshot << "\n";
}

void TradeSignalMerger::writeEoDResult(const string& sResultPath, const string& sTodayDate)
{
    (void) sResultPath;

    double dEodPnl = 0;
    double dEodVolume = 0;
    double dEodNotionalTurnOver = 0;

    map<string, double> mProductsPnL;
    map<string, double> mProductsVolume;

    fstream fsTransactionsFile;
    fsTransactionsFile.open(sResultPath + "/Transactions.out", fstream::out);

    if(fsTransactionsFile.is_open())
    {
        for(vector<Trade>::iterator itr = _vTotalTrades.begin(); itr != _vTotalTrades.end(); itr++)
        {
            fsTransactionsFile << itr->cTradeTime.igetPrintable() << ";" << itr->sProduct << ";" << itr->iQty << ";" << itr->dPrice << ";";

            if(itr->eTradeType == Trade::KO_INTERNAL)
            {
                fsTransactionsFile << "INTERNAL";
            }
            else if(itr->eTradeType == Trade::KO_PAPER)
            {
                fsTransactionsFile << "PAPER";
            }
            else if(itr->eTradeType == Trade::KO_TRI)
            {
                fsTransactionsFile << "TRI";
            }
            else if(itr->eTradeType == Trade::KO_MATCHED)
            {
                fsTransactionsFile << "MATCHED";
            }


            fsTransactionsFile << "\n";

            double dContractSize = _mProductToContractSize[itr->sProduct];
            double dDollarRate = _mProductToDollarRate[itr->sProduct];
            double dTradingFee = _mProductToTradingFee[itr->sProduct];
            double FXDominatorUSDRate = _mProductFXDominatorUSDRate[itr->sProduct];

            double dNewFee;
            if(itr->eInstrumentType == KO_FX)
            {
                if(itr->eTradeType == Trade::KO_INTERNAL || itr->eTradeType == Trade::KO_MATCHED)
                {
                    dNewFee = 0;
                }
                else
                {
                    dNewFee =  (double)abs(itr->iQty) * FXDominatorUSDRate * dTradingFee;
                }
            }
            else
            {
                dNewFee = (double)abs(itr->iQty) * dTradingFee * dDollarRate;
            }

            double dNewConsideration = (-1.0 * (double)itr->iQty * itr->dPrice * dContractSize * dDollarRate) - dNewFee;

            long dTradePaperQty = 0;
            if(itr->eTradeType != Trade::KO_INTERNAL)
            {
                dTradePaperQty = abs(itr->iQty);
            }

            if(mProductsPnL.find(itr->sProduct) == mProductsPnL.end())
            {
                mProductsPnL[itr->sProduct] = dNewConsideration;
                mProductsVolume[itr->sProduct] = dTradePaperQty;
            }
            else
            {
                mProductsPnL[itr->sProduct] = dNewConsideration + mProductsPnL[itr->sProduct];
                mProductsVolume[itr->sProduct] = dTradePaperQty + mProductsVolume[itr->sProduct];
            }

            dEodNotionalTurnOver = dEodNotionalTurnOver + abs(dNewConsideration);
            dEodPnl = dEodPnl + dNewConsideration;
            dEodVolume = dEodVolume + abs(itr->iQty);
        }

        fsTransactionsFile.close();
    }

    fstream fsDailyResultFile;
    fsDailyResultFile.open(sResultPath + "/DailyResult.out", fstream::out);

    if(fsDailyResultFile.is_open())
    {
        fsDailyResultFile << sTodayDate << ";" << dEodPnl << ";" << dEodVolume << ";" << dEodNotionalTurnOver << "\n";
        fsDailyResultFile.close();
    }

    for(map<string, double>::iterator itr = mProductsPnL.begin(); itr!= mProductsPnL.end(); ++itr)
    {
        fstream fsProductResultFile;
        fsProductResultFile.open(sResultPath + "/" + itr->first + "_ProductResult.out", fstream::out);
        if(fsProductResultFile.is_open())
        {
            fsProductResultFile << sTodayDate << ";" << itr->second << ";" << mProductsVolume[itr->first] << "\n";
            fsProductResultFile.close();
        }

        fstream fsProductFillRatioFile;
        fsProductFillRatioFile.open(sResultPath + "/" + itr->first + "_FillRatio.out", fstream::out);
        if(fsProductFillRatioFile.is_open())
        {
            fsProductFillRatioFile << sTodayDate << ";" << _pScheduler->dgetFillRatio(itr->first) << "\n";
            fsProductFillRatioFile.close();
        }
    }

    fstream fsPosSnapshotFile;
    fsPosSnapshotFile.open(sResultPath + "/PositionSnapshot.out", fstream::out);
    if(fsPosSnapshotFile.is_open())
    {
        fsPosSnapshotFile << _cInstradayPosSnapshot.str();
        fsPosSnapshotFile.close();    
    }
}

int TradeSignalMerger::registerTradingSlot(const string& sProduct, const string& sSlotName, double dContractSize, double dDollarRate, double dTradingFee, double dDominatorUSDRate)
{
    if(_mSlotSignals.find(sProduct) == _mSlotSignals.end())
    {
        _mSlotSignals[sProduct] = vector<SlotSignal>();
        _mProductToContractSize[sProduct] = dContractSize;
        _mProductToDollarRate[sProduct] = dDollarRate;
        _mProductFXDominatorUSDRate[sProduct] = dDominatorUSDRate;
        _mProductToTradingFee[sProduct] = dTradingFee;
        _mProductPrintPos[sProduct] = false;
        _mProductPendingTriFillQty[sProduct] = 0;
        _mProductTheoVolume[sProduct] = 0;
        _mProductPaperVolume[sProduct] = 0;
    }

    _mSlotSignals[sProduct].push_back(SlotSignal());
    (_mSlotSignals[sProduct].end()-1)->sSlotName = sSlotName;
    (_mSlotSignals[sProduct].end()-1)->iDesiredPos = 0;
    (_mSlotSignals[sProduct].end()-1)->iPrevDesiredPos = 0;
    (_mSlotSignals[sProduct].end()-1)->iPendingDesiredPos = 0;
    (_mSlotSignals[sProduct].end()-1)->iSignalState = 2;
    (_mSlotSignals[sProduct].end()-1)->bMarketOrder = false;
    (_mSlotSignals[sProduct].end()-1)->bReady = false;
    (_mSlotSignals[sProduct].end()-1)->bActivate = false;

    return _mSlotSignals[sProduct].size()-1;
}

void TradeSignalMerger::activateSlot(const string& sProduct, long iSlotID)
{
    _mSlotSignals[sProduct][iSlotID].bActivate = true;
}

void TradeSignalMerger::deactivateSlot(const string& sProduct, long iSlotID)
{
    _mSlotSignals[sProduct][iSlotID].bActivate = false;
}

void TradeSignalMerger::setSlotReady(const string& sProduct, long iSlotID)
{
    _mSlotSignals[sProduct][iSlotID].bReady = true;
    bool allSlotsReady = true;

    for(vector<SlotSignal>::iterator itr = _mSlotSignals[sProduct].begin();
        itr != _mSlotSignals[sProduct].end();
        itr++)
    {
        if(itr->bActivate == true)
        {
            allSlotsReady = allSlotsReady && itr->bReady;
        }
    }

    if(allSlotsReady == true)
    {
        aggregateAndSend(sProduct);

        for(vector<SlotSignal>::iterator itr = _mSlotSignals[sProduct].begin();
            itr != _mSlotSignals[sProduct].end();
            itr++)
        {
            itr->bReady = false;
        }
    }
}

void TradeSignalMerger::updateSlotSignal(const string& sProduct, long iSlotID, long iDesiredPos, int iSignalState, bool bMarketOrder)
{
    if(iSignalState == 2) // STOP
    {
        _mSlotSignals[sProduct][iSlotID].iPendingDesiredPos = _mSlotSignals[sProduct][iSlotID].iPos;
    }
    else if(iSignalState == 0 or iSignalState == 1) // BUY or SELL
    {
        _mSlotSignals[sProduct][iSlotID].iPendingDesiredPos = iDesiredPos;
    }
    else if(iSignalState == 3) // FLAT
    {
        _mSlotSignals[sProduct][iSlotID].iPendingDesiredPos = 0;
    }
    else if(iSignalState == 4) // FLAT_ALL_LONG
    {
        if(_mSlotSignals[sProduct][iSlotID].iPos > 0)
        {
            _mSlotSignals[sProduct][iSlotID].iPendingDesiredPos = 0;
        }
        else
        {
            _mSlotSignals[sProduct][iSlotID].iPendingDesiredPos = _mSlotSignals[sProduct][iSlotID].iPos;
        }
    }
    else if(iSignalState == 5) // FLAT_ALL_SHORT
    {
        if(_mSlotSignals[sProduct][iSlotID].iPos < 0)
        {
            _mSlotSignals[sProduct][iSlotID].iPendingDesiredPos = 0;
        }
        else
        {
            _mSlotSignals[sProduct][iSlotID].iPendingDesiredPos = _mSlotSignals[sProduct][iSlotID].iPos;
        }
    }

//std::cerr << "Adjusted desired position is " << _mSlotSignals[sProduct][iSlotID].iPendingDesiredPos << "\n";

    _mSlotSignals[sProduct][iSlotID].iSignalState = iSignalState;

    _mSlotSignals[sProduct][iSlotID].bMarketOrder = bMarketOrder;
}

void TradeSignalMerger::fullyFillAllOrders(vector<vector<SlotSignal>::iterator>& vOrderItrs)
{
//std::cerr << "in fullyFillAllOrders \n";
    for(unsigned int idx = 0; idx < vOrderItrs.size(); idx++)
    {
        long iFilledQty = vOrderItrs[idx]->getWorkingQty();
        if(iFilledQty != 0)
        {
            vOrderItrs[idx]->iPos = vOrderItrs[idx]->iDesiredPos;
//std::cerr << "slot " << vOrderItrs[idx]->sSlotName << " filled. qty: " << iFilledQty << "\n";
        }
    }
//std::cerr << "exit fullyFillAllOrders \n";
}

void TradeSignalMerger::prorataFillAllOrders(vector<vector<SlotSignal>::iterator>& vOrderItrs, long iTotalFillQty)
{
//std::cerr << "in prorataFillAllOrders iTotalFillQty " << iTotalFillQty << "\n";
    long iAllocatedQty = 0;
    long iTotalWorkingQty = 0;

    for(unsigned int idx = 0; idx < vOrderItrs.size(); idx++)
    {
//std::cerr << "vOrderItrs[idx]->getWorkingQty() " << vOrderItrs[idx]->getWorkingQty() << "\n";
        if((vOrderItrs[idx]->getWorkingQty() < 0) == (iTotalFillQty < 0))
        {
            iTotalWorkingQty = iTotalWorkingQty + vOrderItrs[idx]->getWorkingQty();
        }
    }
//std::cerr << "iTotalWorkingQty is " << iTotalWorkingQty << "\n";
    for(unsigned int idx = 0; idx < vOrderItrs.size(); idx++)
    {
        if(vOrderItrs[idx]->getWorkingQty() != 0 && (vOrderItrs[idx]->getWorkingQty() < 0) == (iTotalFillQty < 0))
        {
            int iFilledQtyForSlot;
            double dTheoFilledQtyForSlot = ((double)vOrderItrs[idx]->getWorkingQty() / (double)iTotalWorkingQty) * (double)iTotalFillQty;

            if(dTheoFilledQtyForSlot > 0)
            {
                iFilledQtyForSlot = floor(((double)vOrderItrs[idx]->getWorkingQty() / (double)iTotalWorkingQty) * (double)iTotalFillQty);
            }
            else
            {
                iFilledQtyForSlot = ceil(((double)vOrderItrs[idx]->getWorkingQty() / (double)iTotalWorkingQty) * (double)iTotalFillQty);
            }
           
            if(iFilledQtyForSlot != 0)
            { 
                vOrderItrs[idx]->iPos = vOrderItrs[idx]->iPos + iFilledQtyForSlot;
//std::cerr << "iAllocatedQty before " << iAllocatedQty << "\n";
                iAllocatedQty = iAllocatedQty + iFilledQtyForSlot;
//std::cerr << "iAllocatedQty after " << iAllocatedQty << "\n";
            }
        }
    }

    unsigned int idx = 0;

//std::cerr << "iAllocatedQty is " << iAllocatedQty << "\n";
//std::cerr << "iTotalFillQty is " << iTotalFillQty << "\n";

    while(abs(iAllocatedQty) < abs(iTotalFillQty))
    {
//std::cerr << "vOrderItrs size " << vOrderItrs.size() << "\n";
        if((vOrderItrs[idx]->getWorkingQty() < 0) == (iTotalFillQty < 0))
        {
            if(vOrderItrs[idx]->getWorkingQty() != 0)
            {
                if(iTotalFillQty > 0)
                {
                    vOrderItrs[idx]->iPos = vOrderItrs[idx]->iPos + 1;
                    iAllocatedQty = iAllocatedQty + 1;    
                }
                else if(iTotalFillQty < 0)
                {
                    vOrderItrs[idx]->iPos = vOrderItrs[idx]->iPos - 1;
                    iAllocatedQty = iAllocatedQty - 1;
                }
            }
        }

        idx++;
        if(idx == vOrderItrs.size())
        {
            idx = 0;
            if(iAllocatedQty != iTotalFillQty)
            {
                stringstream cStringStream;
                cStringStream << "Error: remainder allocation didnt finish after a full iteration \n";
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
            }
            break;
        }
    }

    if(iAllocatedQty != iTotalFillQty)
    {
        stringstream cStringStream;
        cStringStream << "Error: Unallocated fill qty detected! \n";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
//std::cerr << "exit prorataFillAllOrders \n";
}

void TradeSignalMerger::aggregateAndSend(const string& sProduct)
{
    long iTotalLongWorkingQty = 0;
    long iTotalShortWorkingQty = 0;

    long iTotalLongLiquidationQty = 0;
    long iTotalShortLiquidationQty = 0;

    long iTotalExternalDesiredQty = 0;
    long iTotalPosToLiquidate = 0;

    stringstream cPosContentSStream;

    vector<vector<SlotSignal>::iterator> vBuyingSlotItrs;
    vector<vector<SlotSignal>::iterator> vSellingSlotItrs;

    vector<vector<SlotSignal>::iterator> vBuyingLiquidationSlotItrs;
    vector<vector<SlotSignal>::iterator> vSellingLiquidationSlotItrs;

    for(vector<SlotSignal>::iterator itr = _mSlotSignals[sProduct].begin();
        itr != _mSlotSignals[sProduct].end();
        itr++)
    {
        itr->iPrevDesiredPos = itr->iDesiredPos;
        itr->iDesiredPos = itr->iPendingDesiredPos;

//        if(sProduct == "EURUSD")
//        {
//            stringstream cStringStream;
//            cStringStream << itr->sSlotName << " " << itr->iDesiredPos << " " << sProduct;
//            ErrorHandler::GetInstance()->newWarningMsg("0", "ALL", sProduct, cStringStream.str());
//        }

        if(itr->iSignalState != 2)
        {
            if(itr->getWorkingQty() != 0)
            {
                if(itr->getWorkingQty() > 0)
                {
                    if(itr->bMarketOrder == false)
                    {
                        vBuyingSlotItrs.push_back(itr);
                        iTotalLongWorkingQty = iTotalLongWorkingQty + itr->getWorkingQty();
                    }
                    else
                    {
                        vBuyingLiquidationSlotItrs.push_back(itr);
                        iTotalLongLiquidationQty = iTotalLongLiquidationQty + itr->getWorkingQty();
                    }
                }
                else if(itr->getWorkingQty() < 0)
                {
                    if(itr->bMarketOrder == false)
                    {
                        vSellingSlotItrs.push_back(itr);
                        iTotalShortWorkingQty = iTotalShortWorkingQty + itr->getWorkingQty();
                    }
                    else
                    {
                        vSellingLiquidationSlotItrs.push_back(itr);
                        iTotalShortLiquidationQty = iTotalShortLiquidationQty + itr->getWorkingQty();
                    }
                }
            }
        }

        if(itr->bMarketOrder == false)
        {
            iTotalExternalDesiredQty = iTotalExternalDesiredQty + itr->iDesiredPos;
        }
        else
        {
            iTotalPosToLiquidate = iTotalPosToLiquidate + itr->iPos;
        }
    }

    if(iTotalShortLiquidationQty != 0 && iTotalLongLiquidationQty != 0)
    {
        long iInternalMatchedQty;

        if(abs(iTotalLongLiquidationQty) <= abs(iTotalShortLiquidationQty))
        {
            iInternalMatchedQty = iTotalLongLiquidationQty;
        }
        else
        {
            iInternalMatchedQty = iTotalShortLiquidationQty;
        }

        stringstream cStringStream;
        cStringStream << "Matching liquidation order internally " << iInternalMatchedQty << " lots.";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream.str());
    }
 
    if(abs(iTotalShortLiquidationQty) == abs(iTotalLongLiquidationQty) && iTotalShortLiquidationQty != 0)
    {
        fullyFillAllOrders(vBuyingLiquidationSlotItrs);
        fullyFillAllOrders(vSellingLiquidationSlotItrs);        
    }
    else if(abs(iTotalShortLiquidationQty) > abs(iTotalLongLiquidationQty) && iTotalLongLiquidationQty != 0)
    {
        fullyFillAllOrders(vBuyingLiquidationSlotItrs);
        prorataFillAllOrders(vSellingLiquidationSlotItrs, -1 * iTotalLongLiquidationQty);
    }
    else if(abs(iTotalShortLiquidationQty) < abs(iTotalLongLiquidationQty) && iTotalShortLiquidationQty != 0)
    {
        fullyFillAllOrders(vSellingLiquidationSlotItrs);
        prorataFillAllOrders(vBuyingLiquidationSlotItrs, -1 * iTotalShortLiquidationQty);
    }
 
    if(iTotalShortWorkingQty != 0 && iTotalLongWorkingQty != 0)
    {
        long iInternalMatchedQty;

        if(abs(iTotalLongWorkingQty) <= abs(iTotalShortWorkingQty))
        {
            iInternalMatchedQty = iTotalLongWorkingQty;
        }
        else
        {
            iInternalMatchedQty = iTotalShortWorkingQty;
        }

        stringstream cStringStream;
        cStringStream << "Matching order internally " << iInternalMatchedQty << " lots.";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream.str());
    }

    if(abs(iTotalShortWorkingQty) == abs(iTotalLongWorkingQty) && iTotalShortWorkingQty != 0)
    {
        fullyFillAllOrders(vBuyingSlotItrs);
        fullyFillAllOrders(vSellingSlotItrs);        
    }
    else if(abs(iTotalShortWorkingQty) > abs(iTotalLongWorkingQty) && iTotalLongWorkingQty != 0)
    {
        fullyFillAllOrders(vBuyingSlotItrs);
        prorataFillAllOrders(vSellingSlotItrs, -1 * iTotalLongWorkingQty);
    }
    else if(abs(iTotalShortWorkingQty) < abs(iTotalLongWorkingQty) && iTotalShortWorkingQty != 0)
    {
        fullyFillAllOrders(vSellingSlotItrs);
        prorataFillAllOrders(vBuyingSlotItrs, -1 * iTotalShortWorkingQty);
    }

    long iTotalWorkingQty = iTotalShortWorkingQty + iTotalLongWorkingQty;
    long iPendingTriQty = _mProductPendingTriFillQty[sProduct] * -1;

    if(iTotalWorkingQty * iPendingTriQty > 0)
    {
        long iMatchedQty = 0;

        if(abs(iPendingTriQty) > abs(iTotalWorkingQty))
        {
            iMatchedQty = iTotalWorkingQty;

            iPendingTriQty = iPendingTriQty - iTotalWorkingQty;

            if(iTotalWorkingQty > 0)
            {
                stringstream cStringStream1;
                cStringStream1 << "Fully fill all buy orders";
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream1.str());

                fullyFillAllOrders(vBuyingSlotItrs);
            }
            else
            {
                stringstream cStringStream1;
                cStringStream1 << "Fully fill all sell orders";
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream1.str());

                fullyFillAllOrders(vSellingSlotItrs);
            }
        }
        else if(abs(iPendingTriQty) < abs(iTotalWorkingQty))
        {
            iMatchedQty = iPendingTriQty;

            iPendingTriQty = 0;
            if(iTotalWorkingQty > 0)
            {
                stringstream cStringStream1;
                cStringStream1 << "Partial fill all buy orders " << iMatchedQty << " lots";
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream1.str());
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", sProduct, cStringStream1.str());

                prorataFillAllOrders(vBuyingSlotItrs, iMatchedQty); 
            }
            else
            {
                stringstream cStringStream1;
                cStringStream1 << "Partial fill all sell orders " << iMatchedQty << " lots";
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream1.str());
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", sProduct, cStringStream1.str());

                prorataFillAllOrders(vSellingSlotItrs, iMatchedQty); 
            }
        }
        else if(abs(iPendingTriQty) == abs(iTotalWorkingQty))
        {
            iMatchedQty = iPendingTriQty;

            iPendingTriQty = 0;
            if(iTotalWorkingQty > 0)
            {
                stringstream cStringStream1;
                cStringStream1 << "Fully fill all buy orders";
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream1.str());

                fullyFillAllOrders(vBuyingSlotItrs); 
            }
            else
            {
                stringstream cStringStream1;
                cStringStream1 << "Fully fill all sell orders";
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream1.str());

                fullyFillAllOrders(vSellingSlotItrs); 
            }
        }

        _mProductPendingTriFillQty[sProduct] = iPendingTriQty * -1;

        if(iMatchedQty != 0)
        {    
            stringstream cStringStream;
            cStringStream << "Matching against triangultion fill " << iMatchedQty << " lots.";
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream.str());

            stringstream cStringStream1;
            cStringStream1 << "New Pending Tri Fill qty is " << _mProductPendingTriFillQty[sProduct];
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream1.str());

            double dMatchedPrice = 0;
            for(vector<Trade>::reverse_iterator itr = _vTotalTrades.rbegin();itr != _vTotalTrades.rend();itr++)
            {
                if(itr->sProduct == sProduct)
                {
                    dMatchedPrice = itr->dPrice;
                    break;
                }
            }

            _vTotalTrades.push_back(Trade());
            _vTotalTrades.back().cTradeTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
            _vTotalTrades.back().sProduct = sProduct;
            _vTotalTrades.back().iQty = iMatchedQty * -1;
            _vTotalTrades.back().dPrice = dMatchedPrice;
            _vTotalTrades.back().eInstrumentType = KO_FX;
            _vTotalTrades.back().eTradeType = Trade::KO_INTERNAL;

            _vTotalTrades.push_back(Trade());
            _vTotalTrades.back().cTradeTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
            _vTotalTrades.back().sProduct = sProduct;
            _vTotalTrades.back().iQty = iMatchedQty;
            _vTotalTrades.back().dPrice = dMatchedPrice;
            _vTotalTrades.back().eInstrumentType = KO_FX;
            _vTotalTrades.back().eTradeType = Trade::KO_MATCHED;
        }
    }

    // send order to executor 
//std::cerr << "Net external desired pos for " << sProduct << " is " << iTotalExternalDesiredQty << "\n";
    _pScheduler->assignPositionToLiquidator(sProduct, iTotalPosToLiquidate);
    _pScheduler->sendToExecutor(sProduct, iTotalExternalDesiredQty);
    _pScheduler->sendToLiquidationExecutor(sProduct, 0);
}

long TradeSignalMerger::igetPaperVolume(string sProduct)
{
    return _mProductPaperVolume[sProduct];
}

long TradeSignalMerger::igetTheoVolume(string sProduct)
{
    return _mProductTheoVolume[sProduct];
}

void TradeSignalMerger::addPendingTriFillQty(const string& sProduct, int iQty, double dPrice)
{
    _vTotalTrades.push_back(Trade());
    _vTotalTrades.back().cTradeTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
    _vTotalTrades.back().sProduct = sProduct;
    _vTotalTrades.back().iQty = iQty * -1; // we move this amount of position away from the major to from a cross, therefore minus
    _vTotalTrades.back().dPrice = dPrice;
    _vTotalTrades.back().eInstrumentType = KO_FX;
    _vTotalTrades.back().eTradeType = Trade::KO_INTERNAL;

    stringstream cStringStream1;
    cStringStream1 << "Adding " << iQty << " to pending tri fill qty";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream1.str());

    _mProductPendingTriFillQty[sProduct] = _mProductPendingTriFillQty[sProduct] + iQty;
}

void TradeSignalMerger::onFill(const string& sProduct, int iQty, double dPrice, bool bIsLiquidationFill, InstrumentType eInstrumentType, bool bIsInteranlFill)
{
//std::cerr << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "TradeSignalMerger received external fill " << sProduct << " Qty " << iQty << " Price " << dPrice << "\n";
    if(_mProductPendingTriFillQty[sProduct] * iQty > 0)
    {
        long iAllocatedQty;

        if(abs(_mProductPendingTriFillQty[sProduct]) > abs(iQty))
        {
            iAllocatedQty = iQty;

            _mProductPendingTriFillQty[sProduct] = _mProductPendingTriFillQty[sProduct] - iAllocatedQty;
            iQty = 0;

            stringstream cStringStream;
            cStringStream << "Allocating " << iAllocatedQty << " to pending trigulation trades. Left over pending is " << _mProductPendingTriFillQty[sProduct];
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream.str());
        }
        else
        {
            iAllocatedQty = _mProductPendingTriFillQty[sProduct];

            iQty = iQty - iAllocatedQty;
            _mProductPendingTriFillQty[sProduct] = 0;

            stringstream cStringStream;
            cStringStream << "Allocating " << iAllocatedQty << " to pending trigulation trades. left over pending is " << _mProductPendingTriFillQty[sProduct];
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream.str());
        }

        _mProductPaperVolume[sProduct] = _mProductPaperVolume[sProduct] + abs(iAllocatedQty);

        _vTotalTrades.push_back(Trade());
        _vTotalTrades.back().cTradeTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
        _vTotalTrades.back().sProduct = sProduct;
        _vTotalTrades.back().iQty = iAllocatedQty;
        _vTotalTrades.back().dPrice = dPrice;
        _vTotalTrades.back().eInstrumentType = eInstrumentType;
        _vTotalTrades.back().eTradeType = Trade::KO_TRI;
    }

    stringstream cStringStream1;
    cStringStream1 << "Fill qty to be allocated is " << iQty;
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sProduct, cStringStream1.str());

    if(iQty != 0)
    {
        _vTotalTrades.push_back(Trade());
        _vTotalTrades.back().cTradeTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
        _vTotalTrades.back().sProduct = sProduct;
        _vTotalTrades.back().iQty = iQty;
        _vTotalTrades.back().dPrice = dPrice;
        _vTotalTrades.back().eInstrumentType = eInstrumentType;

        if(bIsInteranlFill == true)
        {
            _vTotalTrades.back().eTradeType = Trade::KO_INTERNAL;
            _mProductTheoVolume[sProduct] = _mProductTheoVolume[sProduct] + abs(iQty);
        }
        else
        {
            _vTotalTrades.back().eTradeType = Trade::KO_PAPER;
            _mProductTheoVolume[sProduct] = _mProductTheoVolume[sProduct] + abs(iQty);
            _mProductPaperVolume[sProduct] = _mProductPaperVolume[sProduct] + abs(iQty);
        }

        vector<vector<SlotSignal>::iterator> vFilledOrderList;
        vector<vector<SlotSignal>::iterator> vFullOrderList;
        long iTotalWorkingQty = 0;

        for(vector<SlotSignal>::iterator itr = _mSlotSignals[sProduct].begin();
            itr != _mSlotSignals[sProduct].end();
            itr++)
        {
            if(itr->iSignalState != 2)
            {
                if((itr->getWorkingQty() < 0) == (iQty < 0))
                {
                    if(bIsLiquidationFill == false || (bIsLiquidationFill == true && itr->bMarketOrder))
                    {
                        vFilledOrderList.push_back(itr);
                        iTotalWorkingQty = iTotalWorkingQty + itr->getWorkingQty();
                    }
                }
            }

            if(itr->bActivate == true)
            {
                vFullOrderList.push_back(itr);
            }
        }

        if(abs(iTotalWorkingQty) >= abs(iQty))
        {
    //std::cerr << "alloccatng working fill " << iQty << " lots \n";
            prorataFillAllOrders(vFilledOrderList, iQty);
        }
        else
        {
            long iLeftOverQty = iQty - iTotalWorkingQty;

            if(iTotalWorkingQty != 0)
            {
    //std::cerr << "alloccated working fill " << iTotalWorkingQty << " lots \n";
                prorataFillAllOrders(vFilledOrderList, iTotalWorkingQty);
            }

    //std::cerr << "alloccated unwanted fill " << iLeftOverQty << " lots \n";

            stringstream cStringStream;
            cStringStream << " Trying to allocate unwanted fill quantity " << iLeftOverQty << ".";
            ErrorHandler::GetInstance()->newWarningMsg("0", "ALL", sProduct, cStringStream.str());

            unwantedFillAllOrders(vFullOrderList, iLeftOverQty);
        }
    }
}

void TradeSignalMerger::unwantedFillAllOrders(vector<vector<SlotSignal>::iterator>& vOrderItrs, long iTotalFillQty)
{
    long iAllocatedQty = 0;

    for(unsigned int i = 0; i < vOrderItrs.size(); i++)
    {
        if(iAllocatedQty == iTotalFillQty)
        {
            break;
        }

        long iUnallocatedQty = iTotalFillQty - iAllocatedQty;

        if(vOrderItrs[i]->getPrevWorkingQty() != 0)
        {
            if((vOrderItrs[i]->getPrevWorkingQty() < 0) == (iTotalFillQty < 0))
            {
                long iFilledQty = 0;
                if(abs(vOrderItrs[i]->getPrevWorkingQty()) > abs(iUnallocatedQty))
                {
                    iFilledQty = iUnallocatedQty;
                    vOrderItrs[i]->iPos = vOrderItrs[i]->iPos + iUnallocatedQty;
                }
                else
                {
                    iFilledQty = vOrderItrs[i]->getPrevWorkingQty();
                    vOrderItrs[i]->iPos = vOrderItrs[i]->iPos + vOrderItrs[i]->getPrevWorkingQty();
                }
            
                iAllocatedQty = iAllocatedQty + iFilledQty;
            }
        }
    }

    while(true)
    {
        if(iAllocatedQty == iTotalFillQty)
        {
            break;
        }

        long iQtyAllocatedThisAround = 0;
        for(unsigned int i = 0; i < vOrderItrs.size(); i++)
        {
            if(iTotalFillQty > 0)
            {
                if(vOrderItrs[i]->iSignalState == 0 || // BUY
                   vOrderItrs[i]->iSignalState == 5) // FLAT_ALL_SHORT
                {
                    vOrderItrs[i]->iPos = vOrderItrs[i]->iPos + 1;
                    iAllocatedQty = iAllocatedQty + 1;
                    iQtyAllocatedThisAround = iQtyAllocatedThisAround + 1;
                }
            }
            else
            {
                if(vOrderItrs[i]->iSignalState == 1 || // SELL 
                   vOrderItrs[i]->iSignalState == 4) // FLAT_ALL_LONG
                {
                    vOrderItrs[i]->iPos = vOrderItrs[i]->iPos - 1;
                    iAllocatedQty = iAllocatedQty - 1;
                    iQtyAllocatedThisAround = iQtyAllocatedThisAround - 1;
                }
            }

            if(iAllocatedQty == iTotalFillQty)
            {
                break;
            }
        }

        if(iQtyAllocatedThisAround == 0)
        {
            break;
        }
    }

    unsigned int idx = 0;
    while(true)
    {
        if(iAllocatedQty == iTotalFillQty)
        {
            break;
        }

        if(iTotalFillQty > 0)
        {
            vOrderItrs[idx]->iPos = vOrderItrs[idx]->iPos + 1;
            iAllocatedQty = iAllocatedQty + 1;
        }
        else
        {
            vOrderItrs[idx]->iPos = vOrderItrs[idx]->iPos - 1;
            iAllocatedQty = iAllocatedQty - 1;
        }

        idx++;
        if(idx == vOrderItrs.size())
        {
            idx = 0;
        }
    }
}

}
