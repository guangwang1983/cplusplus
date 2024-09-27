#include <iostream>
#include "ExecutorSim.h"
#include "../EngineInfra/ErrorHandler.h"
#include <math.h>
#include <boost/math/special_functions/round.hpp>
#include "../EngineInfra/ErrorHandler.h"
#include <unistd.h>

using namespace H5;
using namespace std;

namespace KO
{

ExecutorSim::ExecutorSim(const string& sProduct, long iSubmitLatency, long iAmendLatency, double dTickSize, const string& sDataFile, long iExpoLimit, const string& sDate, const string& sLogPath, bool bWriteLog, bool bIsLiquidator, bool bLogMarketData, bool bIOC, int iIOCSpreadWidthLimit)
:_sProduct(sProduct),
 _pLoadedData(NULL),
 _iNumLoadedDataPoints(0),
 _iCurrentDataIdx(-1),
 _iNextDataSetIdx(-1),
 _iSubmitLatency(iSubmitLatency),
 _iAmendLatency(iAmendLatency),
 _iTotalOrdersInMarket(0),
 _dTickSize(dTickSize),
 _iExpoLimit(iExpoLimit),
 _bIsLiquidator(bIsLiquidator),
 _absTotalNewQty(0),
 _absTotalFillQty(0),
 _bLogMarketData(bLogMarketData)
{
    _sDataFileName = sDataFile;
    _bIOC = bIOC;
    _iIOCSpreadWidthLimit = iIOCSpreadWidthLimit;

    _cPrevBatchLastPrint.iEpochTimeStamp = 0;

    bool bFileOpened = false;
    int iNumTries = 0;
    while(bFileOpened == false)
    {
        try
        {
            _pH5File = new H5File(sDataFile, H5F_ACC_RDONLY);
            bFileOpened = true;
        }
        catch(...)
        {
            iNumTries = iNumTries + 1;
            if(iNumTries > 100)
            {
                cerr << "Error: Failed to open data file " << _sDataFileName << "\n";
                exit (EXIT_FAILURE);
            }
            usleep(100000);
        }
    }

    _pGridDataType = new CompType(sizeof(GridData));

    _pGridDataType->insertMember("EpochTimeStamp", HOFFSET(GridData, iEpochTimeStamp), PredType::NATIVE_LONG);

    _pGridDataType->insertMember("Bid", HOFFSET(GridData, dBid), PredType::NATIVE_DOUBLE);
    _pGridDataType->insertMember("BidInTicks", HOFFSET(GridData, iBidInTicks), PredType::NATIVE_LONG);
    _pGridDataType->insertMember("BidSize", HOFFSET(GridData, iBidSize), PredType::NATIVE_LONG);

    _pGridDataType->insertMember("Ask", HOFFSET(GridData, dAsk), PredType::NATIVE_DOUBLE);
    _pGridDataType->insertMember("AskInTicks", HOFFSET(GridData, iAskInTicks), PredType::NATIVE_LONG);
    _pGridDataType->insertMember("AskSize", HOFFSET(GridData, iAskSize), PredType::NATIVE_LONG);

    _pGridDataType->insertMember("Last", HOFFSET(GridData, dLast), PredType::NATIVE_DOUBLE);
    _pGridDataType->insertMember("LastInTicks", HOFFSET(GridData, iLastInTicks), PredType::NATIVE_LONG);
    _pGridDataType->insertMember("TradeSize", HOFFSET(GridData, iTradeSize), PredType::NATIVE_LONG);
    _pGridDataType->insertMember("AccumumTradeSize", HOFFSET(GridData, iAccumuTradeSize), PredType::NATIVE_LONG);
    _pGridDataType->insertMember("WeightedMid", HOFFSET(GridData, dWeightedMid), PredType::NATIVE_DOUBLE);
    _pGridDataType->insertMember("WeightedMidInTicks", HOFFSET(GridData, dWeightedMidInTicks), PredType::NATIVE_DOUBLE);

    string sLogFileName = sLogPath;

    if(sLogPath != "")
    {
        sLogFileName = sLogFileName + "/";        
    }

    if(_bIsLiquidator == false)
    {
        sLogFileName = sLogFileName + "Execution-" + sProduct + "-" + sDate;
    }
    else
    {
        sLogFileName = sLogFileName + "FastLiquidation-" + sProduct + "-" + sDate;
    }

    if(_sProduct.substr(0,1) == "I" ||
       _sProduct.substr(0,1) == "L" ||
       _sProduct.substr(0,2) == "GE" ||
       _sProduct.substr(0,3) == "BAX")
    {
        _bJoinMidPrice = true;
    }
    else
    {
        _bJoinMidPrice = false;
    }

    _cLogger.openFile(sLogFileName, bWriteLog, false);
    _cLogger << "_iExpoLimit is " << _iExpoLimit << "\n";

    _bIsTheo = false;

    _iSecondMsgLimit = 10;
}

ExecutorSim::~ExecutorSim()
{
    delete _pGridDataType;
    delete _pH5File;
}

void ExecutorSim::setTheoreticalSim(bool bIsTheo)
{
    _bIsTheo = bIsTheo;
}

const string& ExecutorSim::sgetProduct()
{
    return _sProduct;
}

bool ExecutorSim::bloadHDF5(long iDataSetIdx)
{
    bool bResult = false;

    if(_pLoadedData != NULL)
    {
        delete _pLoadedData;
        _pLoadedData = NULL;
    }

    _iNumLoadedDataPoints = 0;
    _iCurrentDataIdx = 0;

    bool bDataSetExists = false;
    DataSet cDataSet;

    try
    {
        cDataSet = _pH5File->openDataSet(to_string(iDataSetIdx));
        bDataSetExists = true;
    }
    catch(FileIException& not_found_error)
    {
        //cerr << "DataSet " << iDataSetIdx << " not found \n";
    }

    if(bDataSetExists == true)
    {
        //cerr << "DataSet " << iDataSetIdx << " loaded \n";
        hsize_t cDim[1];
        DataSpace cDataSpace = cDataSet.getSpace();
        cDataSpace.getSimpleExtentDims(cDim);
        _iNumLoadedDataPoints = cDim[0];

        if(_iNumLoadedDataPoints > 0)
        {
            _pLoadedData = new GridData [cDim[0]];
            bool bDataLoaded = false;
            int iNumTries = 0;
            while(bDataLoaded == false)
            {
                try
                {
                    cDataSet.read(_pLoadedData, *_pGridDataType);
                    bDataLoaded = true;
                }
                catch(...)
                {
                    iNumTries = iNumTries + 1;
                    if(iNumTries > 100)
                    {
                        cerr << "Error: Failed to read data set " << iDataSetIdx << " from " << _sDataFileName << "\n";
                        exit (EXIT_FAILURE);
                    }
                    usleep(100000);
                }
            }
        }

        bResult = true;
    }

    return bResult;
}

void ExecutorSim::findLastPrint(long iDataIdx)
{
//cerr << "trying to find last print \n";
    bool bLastPrintFound = false;
    int iSearchIdx = iDataIdx;

    while(bLastPrintFound != true)
    {
        iSearchIdx = iSearchIdx - 1;

        bool bDataSetExists = false;
        DataSet cDataSet;

        try
        {
            cDataSet = _pH5File->openDataSet(to_string(iSearchIdx));
            bDataSetExists = true;
        }
        catch(FileIException& not_found_error)
        {
            //cerr << "DataSet " << iSearchIdx << " not found \n";
        }

        if(bDataSetExists == true)
        {
            hsize_t cDim[1];
            DataSpace cDataSpace = cDataSet.getSpace();
            cDataSpace.getSimpleExtentDims(cDim);
            if(cDim[0] > 0)
            {
                bLastPrintFound = true;

                GridData* pData = new GridData [cDim[0]];
                bool bDataLoaded = false;
                int iNumTries = 0;
                while(bDataLoaded == false)
                {
                    try
                    {
                        cDataSet.read(pData, *_pGridDataType);
                        bDataLoaded = true;
                    }
                    catch(...)
                    {
                        iNumTries = iNumTries + 1;
                        if(iNumTries > 100)
                        {
                            cerr << "Error: Failed to read data set " << iSearchIdx << " from " << _sDataFileName << "\n";
                            exit (EXIT_FAILURE);
                        }
                        usleep(100000);
                    }
                }

                _cPrevBatchLastPrint = pData[cDim[0] - 1];
//cerr << "last print found in data set " << iSearchIdx << " timestamp " << _cPrevBatchLastPrint.iEpochTimeStamp << "\n";

                delete pData;
                break;
            }
        }
        else
        {
            // reached beginning of the data
//cerr << "Beginning of data reached. Cannot find last print \n";
            break;
        }
    }
}

void ExecutorSim::transferPosition(long iPortfolioID, long iAdditionalPosition)
{
    _vSimPortfolios[iPortfolioID].iPortfolioPosition = _vSimPortfolios[iPortfolioID].iPortfolioPosition + iAdditionalPosition;
}

int ExecutorSim::igetPortfolioPosition(long iPortfolioID)
{
    return _vSimPortfolios[iPortfolioID].iPortfolioPosition;
}

bool ExecutorSim::bloadDataForSignal(long iSignalTimeStamp)
{
    bool bResult = true;
    if((long)(iSignalTimeStamp / 60000000) >= _iNextDataSetIdx)
    {
        long dataIdx = (long)(iSignalTimeStamp / 60000000);

        if(dataIdx == _iNextDataSetIdx)
        {
            if(_iNumLoadedDataPoints != 0)
            {
                _cPrevBatchLastPrint = _pLoadedData[_iNumLoadedDataPoints-1];
            }
        }
        else
        {
            findLastPrint(dataIdx);
        }

//cerr << "last print is " << _cPrevBatchLastPrint.iEpochTimeStamp << "\n";
        bResult = bloadHDF5(dataIdx);
        _iNextDataSetIdx = dataIdx + 1;
    }
    return bResult;
}

bool ExecutorSim::bloadMoreData()
{
    if(_iNumLoadedDataPoints != 0)
    {
        _cPrevBatchLastPrint = _pLoadedData[_iNumLoadedDataPoints-1];
    }

    bool bResult = bloadHDF5(_iNextDataSetIdx);
//cerr << "last print is " << _cPrevBatchLastPrint.iEpochTimeStamp << "\n";
    _iNextDataSetIdx = _iNextDataSetIdx + 1;

    return bResult;
}

/*
void ExecutorSim::updateOrderPrice(ExecutorSimOrder& cExecutorSimOrder, const GridData& cNewDataPoint, long iSubmitTime)
{
//cerr << "in updateOrderPrice \n";
//cerr << "order qty " << cExecutorSimOrder.iRemainQty << " price " << cExecutorSimOrder.iPriceInTicks << " state is " << cExecutorSimOrder.eState << "\n";
    if(cExecutorSimOrder.eState == ExecutorSimOrder::ACTIVE || cExecutorSimOrder.eState == ExecutorSimOrder::PENDING_SUBMIT_CHANGE || cExecutorSimOrder.eState == ExecutorSimOrder::PENDING_SUBMIT_DELETE)
    {
        long iNewOrderBestPrice = getOrderBestPrice(cExecutorSimOrder, cNewDataPoint);
//cerr << "Order new best price is " << iNewOrderBestPrice << "\n";
        if(cExecutorSimOrder.iRemainQty > 0)
        {
            if(iNewOrderBestPrice > cExecutorSimOrder.iPriceInTicks)
            {
                changeOrder(cExecutorSimOrder, cExecutorSimOrder.iRemainQty, iNewOrderBestPrice, iSubmitTime);
            }
        }
        else
        {
            if(iNewOrderBestPrice < cExecutorSimOrder.iPriceInTicks)
            {
                changeOrder(cExecutorSimOrder, cExecutorSimOrder.iRemainQty, iNewOrderBestPrice, iSubmitTime);
            }
        }
    }
}
*/

int ExecutorSim::iaddPortfolio(H5File* pH5File, CompType* pTransactionDataType, SchedulerBase* pSchedulerBase)
{
    ExecutionPortfolio cNewPortfolio;
    cNewPortfolio.pH5File = pH5File;
    cNewPortfolio.pTransactionDataType = pTransactionDataType;
    cNewPortfolio.cCurrentSignal.iEpochTimeStamp = 0;
    cNewPortfolio.cCurrentSignal.iDesiredPos = 0;
    cNewPortfolio.cCurrentSignal.iSignalState = STOP;
    cNewPortfolio.cCurrentSignal.bMarketOrder = false;
    cNewPortfolio.pSchedulerBase = pSchedulerBase;
    cNewPortfolio.iPortfolioPosition = 0;

    int iPortfolioID = _vSimPortfolios.size();
    _vSimPortfolios.push_back(cNewPortfolio);
    _vStratgyOrders.push_back(vector<ExecutorSimOrder>());
    _vPortfolioSimTransactions.push_back(vector<SimTransaction>());   
    return iPortfolioID;
}

void ExecutorSim::adjustOrderForSignal(TradeSignal& cTradeSignal)
{
    if(cTradeSignal.iSignalState == STOP)
    {
        // delete all working orders for the strategy
        deleteAllPortfolioOrders(cTradeSignal.iPortfolioID, cTradeSignal.iEpochTimeStamp); 
    }
    else
    {
        if(cTradeSignal.iSignalState == FLAT_ALL_LONG)
        {
            if(_vSimPortfolios[cTradeSignal.iPortfolioID].iPortfolioPosition <= 0)
            {
                cTradeSignal.iDesiredPos = _vSimPortfolios[cTradeSignal.iPortfolioID].iPortfolioPosition;
            }
            else
            {
                cTradeSignal.iDesiredPos = 0;
            }
        }
        else if(cTradeSignal.iSignalState == FLAT_ALL_SHORT)
        {
            if(_vSimPortfolios[cTradeSignal.iPortfolioID].iPortfolioPosition >= 0)
            {
                cTradeSignal.iDesiredPos = _vSimPortfolios[cTradeSignal.iPortfolioID].iPortfolioPosition;
            }
            else
            {
                cTradeSignal.iDesiredPos = 0;
            }
        }

        int iNewQty = cTradeSignal.iDesiredPos - _vSimPortfolios[cTradeSignal.iPortfolioID].iPortfolioPosition;
    
        if(iNewQty > _iExpoLimit)
        {
            iNewQty = _iExpoLimit;
        }
        else if(iNewQty < -1 * _iExpoLimit)
        {
            iNewQty = -1 * _iExpoLimit;
        }

        if(_vStratgyOrders[cTradeSignal.iPortfolioID].size() == 0)
        {
            if(iNewQty != 0)
            {
                submitNewOrder(cTradeSignal.iPortfolioID, iNewQty, cTradeSignal.iEpochTimeStamp, cTradeSignal.bMarketOrder);
            }
        }
        else
        {
            if(iNewQty == 0)
            {
                deleteAllPortfolioOrders(cTradeSignal.iPortfolioID, cTradeSignal.iEpochTimeStamp);
            }
            else
            {
                for(vector<ExecutorSimOrder>::iterator itr = _vStratgyOrders[cTradeSignal.iPortfolioID].begin();
                    itr != _vStratgyOrders[cTradeSignal.iPortfolioID].end();
                    itr++)
                {
                    itr->bIsMarketOrder = cTradeSignal.bMarketOrder;
                }


                if((iNewQty > 0) == (igetOrderQty((*_vStratgyOrders[cTradeSignal.iPortfolioID].begin())) > 0))
                {
                    // new signal and active order has the same direction. adjust order size
                    long iQtyToBeAllocated = iNewQty;
                    for(vector<ExecutorSimOrder>::iterator itr = _vStratgyOrders[cTradeSignal.iPortfolioID].begin();
                    itr != _vStratgyOrders[cTradeSignal.iPortfolioID].end();
                    itr++)
                    {
                        long iCurrentOrderQty = igetOrderQty(*itr);
                        if(iQtyToBeAllocated == 0)
                        {
                             deleteOrder(*itr, cTradeSignal.iEpochTimeStamp);
                        }
                        else
                        {
                            if(abs(iCurrentOrderQty) <= abs(iQtyToBeAllocated))
                            {
                                // match current order qty and move to the next
                                iQtyToBeAllocated = iQtyToBeAllocated - iCurrentOrderQty;
                            }
                            else
                            {
                                // reduce current order qty, all qty located
                                itr->eState = ExecutorSimOrder::PENDING_SUBMIT_CHANGE;
                                itr->iPendingRemainQty = iQtyToBeAllocated;
                                if(itr->iPendingPriceInTicks == 0)
                                {
                                    itr->iPendingPriceInTicks = itr->iPriceInTicks;
                                }
                                itr->iConfirmTime = cTradeSignal.iEpochTimeStamp;

                                iQtyToBeAllocated = 0; 
                            }
                        }
                    }

                    if(iQtyToBeAllocated != 0)
                    {
                        submitNewOrder(cTradeSignal.iPortfolioID, iQtyToBeAllocated, cTradeSignal.iEpochTimeStamp, cTradeSignal.bMarketOrder);
                    }
                }
                else
                {
                    // new signal and active order has different directions. remove all active orders and submit new signal order
                    deleteAllPortfolioOrders(cTradeSignal.iPortfolioID, cTradeSignal.iEpochTimeStamp);
                    submitNewOrder(cTradeSignal.iPortfolioID, iNewQty, cTradeSignal.iEpochTimeStamp, cTradeSignal.bMarketOrder);
                }
            } 
        }
    }
}

void ExecutorSim::deleteAllPortfolioOrders(int iPortfolioID, long iDeleteTime)
{
    for(vector<ExecutorSimOrder>::iterator itr = _vStratgyOrders[iPortfolioID].begin();
    itr != _vStratgyOrders[iPortfolioID].end();
    itr++)
    {
///////////////////////////////////////////////////////////
// Needs to deal with deleting an order in pending state //
///////////////////////////////////////////////////////////

        deleteOrder(*itr, iDeleteTime);
    }
}

long ExecutorSim::igetOrderQty(const ExecutorSimOrder& cOrder)
{
    if(cOrder.eState == ExecutorSimOrder::ACTIVE || cOrder.eState == ExecutorSimOrder::INACTIVE)
    {
        return cOrder.iRemainQty;
    }
    else
    {
        return cOrder.iPendingRemainQty;
    }
}

GridData* ExecutorSim::pgetLastDataPoint()
{
    if(_iCurrentDataIdx == 0)
    {
//cerr << "using last data point " << _cPrevBatchLastPrint.iEpochTimeStamp << "|" << _cPrevBatchLastPrint.iBidSize << "|" << _cPrevBatchLastPrint.iBidInTicks << "|" << _cPrevBatchLastPrint.iAskInTicks << "|" << _cPrevBatchLastPrint.iAskSize << "|" << _cPrevBatchLastPrint.iLastInTicks << "|" << _cPrevBatchLastPrint.iTradeSize << "|" << _cPrevBatchLastPrint.iAccumuTradeSize << "\n";
        return &_cPrevBatchLastPrint;
    }
    else
    {
//cerr << "using last data point " << _pLoadedData[_iCurrentDataIdx-1].iEpochTimeStamp << "|" << _pLoadedData[_iCurrentDataIdx-1].iBidSize << "|" << _pLoadedData[_iCurrentDataIdx-1].iBidInTicks << "|" << _pLoadedData[_iCurrentDataIdx-1].iAskInTicks << "|" << _pLoadedData[_iCurrentDataIdx-1].iAskSize << "|" << _pLoadedData[_iCurrentDataIdx-1].iLastInTicks << "|" << _pLoadedData[_iCurrentDataIdx-1].iTradeSize << "|" << _pLoadedData[_iCurrentDataIdx-1].iAccumuTradeSize << "\n";
        return &_pLoadedData[_iCurrentDataIdx - 1];
    }
}

void ExecutorSim::changeOrder(ExecutorSimOrder& cExectorOrder, long iNewQty, long iNewPriceInTicks, long iChangeTime)
{
    bool bMsgCheckPassed = true;

    if((int)cExectorOrder.qOrderMsgHistorySecond.size() > _iSecondMsgLimit)
    {
        bMsgCheckPassed = false;
        _cLogger << "Strategy " << cExectorOrder.iPortfolioID << " 1 Second msg " << cExectorOrder.qOrderMsgHistorySecond.size() << " greater than limit " << _iSecondMsgLimit << ". Ignore order change request \n";
    }

    if(bMsgCheckPassed)
    {
        cExectorOrder.eState = ExecutorSimOrder::PENDING_CHANGE;
        cExectorOrder.iPendingRemainQty = iNewQty;
        cExectorOrder.iPendingPriceInTicks = iNewPriceInTicks;
        cExectorOrder.iConfirmTime = iChangeTime + _iAmendLatency;

        _cLogger << "prepare new order change for strategy " << cExectorOrder.iPortfolioID << " new qty " << cExectorOrder.iPendingRemainQty << " new price " << cExectorOrder.iPendingPriceInTicks << " confirm time " << cExectorOrder.iConfirmTime << "\n";
    }
}

void ExecutorSim::changeOrderPrice(ExecutorSimOrder& cExectorOrder, long iNewPriceInTicks, long iChangeTime)
{
    bool bMsgCheckPassed = true;

    if((int)cExectorOrder.qOrderMsgHistorySecond.size() > _iSecondMsgLimit)
    {
        bMsgCheckPassed = false;
        _cLogger << "Strategy " << cExectorOrder.iPortfolioID << " 1 Second msg " << cExectorOrder.qOrderMsgHistorySecond.size() << " greater than limit " << _iSecondMsgLimit << ". Ignore order change request \n";
    }

    if(bMsgCheckPassed)
    {
        cExectorOrder.eState = ExecutorSimOrder::PENDING_PRICE_CHANGE;
        cExectorOrder.iPendingPriceInTicks = iNewPriceInTicks;
        cExectorOrder.iConfirmTime = iChangeTime + _iAmendLatency;

        _cLogger << "prepare new order price change for strategy " << cExectorOrder.iPortfolioID << " new qty " << cExectorOrder.iRemainQty << " new price " << cExectorOrder.iPendingPriceInTicks << " confirm time " << cExectorOrder.iConfirmTime << "\n";

        stringstream cStringStream;
        cStringStream << "Replacing order qty " << cExectorOrder.iRemainQty << " price " <<  cExectorOrder.iPendingPriceInTicks  << ".";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _sProduct, cStringStream.str());
    }
}

void ExecutorSim::deleteOrder(ExecutorSimOrder& cExectorOrder, long iDeleteTime)
{
    cExectorOrder.eState = ExecutorSimOrder::PENDING_SUBMIT_DELETE;
//cerr << "order confirmed qty " << cExectorOrder.iRemainQty << " before delete \n";
//cerr << "order pending qty " << cExectorOrder.iPendingRemainQty << " before delete \n";
    cExectorOrder.iPendingRemainQty = 0;
    cExectorOrder.iConfirmTime = iDeleteTime;
    _cLogger << "prepare new order delete for strategy " << cExectorOrder.iPortfolioID << " confirm time " << cExectorOrder.iConfirmTime << "\n";
}

void ExecutorSim::submitNewOrder(int iPortfolioID, long iNewQty, long iSubmitTime, bool bIsMarketOrder)
{
    bool bMsgCheckPassed = true;

    if(bMsgCheckPassed)
    {
        ExecutorSimOrder cNewOrder;
        cNewOrder.eState = ExecutorSimOrder::PENDING_SUBMIT_CREATION;
        cNewOrder.iRemainQty = 0;
        cNewOrder.iPriceInTicks = 0;
        cNewOrder.iPendingRemainQty = iNewQty;
        cNewOrder.iPortfolioID = iPortfolioID;
        cNewOrder.iConfirmTime = iSubmitTime;
        cNewOrder.bOrderCrossedMarket = false;
        cNewOrder.bIsMarketOrder = bIsMarketOrder;

        _vStratgyOrders[iPortfolioID].push_back(cNewOrder);

        _iTotalOrdersInMarket = _iTotalOrdersInMarket + 1;
        _cLogger << "prepare new order for strategy " << iPortfolioID << " qty " << iNewQty << " confirm time " << cNewOrder.iConfirmTime << "\n";

        _absTotalNewQty = _absTotalNewQty + abs(iNewQty);
    }
}

long ExecutorSim::calcuatedAdjustBidQty(long iMktPrice, long iMktQty)
{
_cLogger << "in calcuatedAdjustBidQty " << iMktPrice << " " << iMktQty << "\n";
    for(deque<RemovedLiquidity>::iterator itr = _qRemovedBidLiquidity.begin();
        itr != _qRemovedBidLiquidity.end();
        itr++)
    {
_cLogger << (*itr).iPrice << " " << (*itr).iSize << "\n";
        if((*itr).iPrice == iMktPrice)
        {
            iMktQty = iMktQty - (*itr).iSize;
            if(iMktQty < 0)
            {
                iMktQty = 0;
            }
            break;
        }  
    } 
_cLogger << "adjust bid qty is " << iMktQty << "\n";
    return iMktQty;
}

long ExecutorSim::calcuatedAdjustAskQty(long iMktPrice, long iMktQty)
{
_cLogger << "in calcuatedAdjustAskQty " << iMktPrice << " " << iMktQty << "\n";
    for(deque<RemovedLiquidity>::iterator itr = _qRemovedAskLiquidity.begin();
        itr != _qRemovedAskLiquidity.end();
        itr++)
    {
_cLogger << (*itr).iPrice << " " << (*itr).iSize << "\n";
        if((*itr).iPrice == iMktPrice)
        {
            iMktQty = iMktQty - (*itr).iSize;
            if(iMktQty < 0)
            {
                iMktQty = 0;
            }
            break;
        }  
    } 
_cLogger << "adjust ask qty is " << iMktQty << "\n";
    return iMktQty;
}

void ExecutorSim::updateRemovedLiqFromTrade(long iFillPrice, long iFilledQty)
{
    if(iFilledQty < 0)
    {
_cLogger << "adding removed bid liquidity price " << iFillPrice << " qty " << iFilledQty << "\n";
        iFilledQty = iFilledQty * -1;
        if(_qRemovedBidLiquidity.size() > 0)
        {
            if(iFillPrice > _qRemovedBidLiquidity.front().iPrice)
            {
                RemovedLiquidity cRemovedLiquidity;
                cRemovedLiquidity.iPrice = iFillPrice;
                cRemovedLiquidity.iSize = iFilledQty;
                _qRemovedBidLiquidity.push_front(cRemovedLiquidity);
            }
            else if(iFillPrice < _qRemovedBidLiquidity.back().iPrice)
            {
                RemovedLiquidity cRemovedLiquidity;
                cRemovedLiquidity.iPrice = iFillPrice;
                cRemovedLiquidity.iSize = iFilledQty;
                _qRemovedBidLiquidity.push_back(cRemovedLiquidity);
            }
            else
            {
                for(deque<RemovedLiquidity>::iterator itr = _qRemovedBidLiquidity.begin();
                    itr != _qRemovedBidLiquidity.end();
                    itr++)
                {
                    if((*itr).iPrice == iFillPrice)
                    {
                        (*itr).iSize = (*itr).iSize + iFilledQty;
                        break;
                    }
                }
            }
        }
        else
        {
            RemovedLiquidity cRemovedLiquidity;
            cRemovedLiquidity.iPrice = iFillPrice;
            cRemovedLiquidity.iSize = iFilledQty;
            _qRemovedBidLiquidity.push_back(cRemovedLiquidity);
        }
    }
    else
    {
_cLogger << "adding removed ask liquidity price " << iFillPrice << " qty " << iFilledQty << "\n";

        if(_qRemovedAskLiquidity.size() > 0)
        {
            if(iFillPrice < _qRemovedAskLiquidity.front().iPrice)
            {
                RemovedLiquidity cRemovedLiquidity;
                cRemovedLiquidity.iPrice = iFillPrice;
                cRemovedLiquidity.iSize = iFilledQty;
                _qRemovedAskLiquidity.push_front(cRemovedLiquidity);
            }
            else if(iFillPrice > _qRemovedAskLiquidity.back().iPrice)
            {
                RemovedLiquidity cRemovedLiquidity;
                cRemovedLiquidity.iPrice = iFillPrice;
                cRemovedLiquidity.iSize = iFilledQty;
                _qRemovedAskLiquidity.push_back(cRemovedLiquidity);
            }
            else
            {
                for(deque<RemovedLiquidity>::iterator itr = _qRemovedAskLiquidity.begin();
                    itr != _qRemovedAskLiquidity.end();
                    itr++)
                {
                    if((*itr).iPrice == iFillPrice)
                    {
                        (*itr).iSize = (*itr).iSize + iFilledQty;
                        break;
                    }
                }
            }
        }
        else
        {
            RemovedLiquidity cRemovedLiquidity;
            cRemovedLiquidity.iPrice = iFillPrice;
            cRemovedLiquidity.iSize = iFilledQty;
            _qRemovedAskLiquidity.push_back(cRemovedLiquidity);
        }
    }
}

void ExecutorSim::updateRemovedLiqFromMarket(const GridData& cDataPoint)
{
    while(_qRemovedBidLiquidity.size() != 0)
    {
        if(cDataPoint.iBidInTicks < _qRemovedBidLiquidity.front().iPrice)
        {
            _qRemovedBidLiquidity.pop_front();
        }
        else
        {
            break;
        }
    }

    while(_qRemovedAskLiquidity.size() != 0)
    {
        if(cDataPoint.iAskInTicks > _qRemovedAskLiquidity.front().iPrice)
        {
            _qRemovedAskLiquidity.pop_front();
        }
        else
        {
            break;
        }
    }
}

void ExecutorSim::checkPendingPriceOrderConfirm(ExecutorSimOrder& cOrder, long iCurrentEpochTime)
{
    if(cOrder.iConfirmTime <= iCurrentEpochTime)
    {
        cOrder.qOrderMsgHistorySecond.push_back(iCurrentEpochTime);

        cOrder.eState = ExecutorSimOrder::ACTIVE;

        bool bOrderQueuePositionRest = false;

        if(cOrder.iPendingPriceInTicks != cOrder.iPriceInTicks) 
        {
            bOrderQueuePositionRest = true;
            cOrder.iQueuePosition = 9999999;
            cOrder.iQueueAdjustment = 0;
        }

        cOrder.iPriceInTicks = cOrder.iPendingPriceInTicks;
        cOrder.iPendingPriceInTicks = 0;

        _cLogger << "order price change confirmed for strategy " << cOrder.iPortfolioID << " qty " << cOrder.iRemainQty << " price " << cOrder.iPriceInTicks << " state " << cOrder.eState << "\n";

        checkOrderCross(cOrder, *pgetLastDataPoint(), cOrder.iConfirmTime, true);

        if(cOrder.eState == ExecutorSimOrder::ACTIVE)
        {
            if(bOrderQueuePositionRest == true)
            {
                setOrderQueuePosition(cOrder);
            }

//            long iNextPriceUpdateTime = ceil((double)cOrder.iConfirmTime / (double)1000000) * 1000000;

//            while(iNextPriceUpdateTime < iCurrentEpochTime)
//            {
//                iNextPriceUpdateTime = iNextPriceUpdateTime + 1000000;
//            }

//            cOrder.eState = ExecutorSimOrder::PENDING_UPDATE;
//            cOrder.iConfirmTime = iNextPriceUpdateTime;
//            _cLogger << "Next order update time is " << iNextPriceUpdateTime << "\n";
        }
    }
}

void ExecutorSim::checkPendingOrderConfirm(ExecutorSimOrder& cOrder, long iCurrentEpochTime)
{
    if(cOrder.iConfirmTime <= iCurrentEpochTime)
    {
        cOrder.qOrderMsgHistorySecond.push_back(iCurrentEpochTime);

        cOrder.eState = ExecutorSimOrder::ACTIVE;

        bool bOrderQueuePositionRest = false;

        if(cOrder.iPendingPriceInTicks != cOrder.iPriceInTicks ||
           abs(cOrder.iPendingRemainQty) > abs(cOrder.iRemainQty))
        {
            bOrderQueuePositionRest = true;
            cOrder.iQueuePosition = 9999999;
            cOrder.iQueueAdjustment = 0;
        }

        cOrder.iPriceInTicks = cOrder.iPendingPriceInTicks;
        cOrder.iPendingPriceInTicks = 0;

        cOrder.iRemainQty = cOrder.iPendingRemainQty;

        _cLogger << "order confirmed for strategy " << cOrder.iPortfolioID << " qty " << cOrder.iRemainQty << " price " << cOrder.iPriceInTicks << " state " << cOrder.eState << "\n";

        stringstream cStringStream;
        cStringStream << "Order entry acked qty " << cOrder.iRemainQty << " price " <<  cOrder.iPriceInTicks  << ".";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _sProduct, cStringStream.str());

        checkOrderCross(cOrder, *pgetLastDataPoint(), cOrder.iConfirmTime, true);

        if(cOrder.eState == ExecutorSimOrder::ACTIVE)
        {
            if(bOrderQueuePositionRest == true)
            {
                setOrderQueuePosition(cOrder);
            }

// To be commented out for changing order on tick update
/*
            long iNextPriceUpdateTime = ceil((double)cOrder.iConfirmTime / (double)1000000) * 1000000;

            while(iNextPriceUpdateTime < iCurrentEpochTime)
            {
                iNextPriceUpdateTime = iNextPriceUpdateTime + 1000000;
            }

            cOrder.eState = ExecutorSimOrder::PENDING_UPDATE;
            cOrder.iConfirmTime = iNextPriceUpdateTime;
            _cLogger << "Next order update time is " << iNextPriceUpdateTime << "\n";
*/
// To be commented out for changing order on tick update
        }
    }
}

void ExecutorSim::checkPendingOrderDelete(ExecutorSimOrder& cOrder, long iCurrentEpochTime)
{
    if(cOrder.iConfirmTime <= iCurrentEpochTime)
    {
        cOrder.qOrderMsgHistorySecond.push_back(iCurrentEpochTime);

        _cLogger << "confirm order delete for strategy " << cOrder.iPortfolioID << "\n";

        stringstream cStringStream;
        cStringStream << "Order cancel acked.";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _sProduct, cStringStream.str());

        cOrder.eState = ExecutorSimOrder::INACTIVE;
        cOrder.iRemainQty = 0;
        _iTotalOrdersInMarket = _iTotalOrdersInMarket - 1;
    }
}

void ExecutorSim::checkOrderCross(ExecutorSimOrder& cOrder, const GridData& cDataPoint, long iEventEpochTime, bool bIsAggresorOrder)
{
    if(cOrder.eState != ExecutorSimOrder::INACTIVE && cOrder.eState != ExecutorSimOrder::PENDING_CREATION && cOrder.eState != ExecutorSimOrder::PENDING_SUBMIT_CREATION)
    {
        long iFilledQty = 0;
        long iFilledPrice = 0;

        if(cOrder.iRemainQty > 0)
        {   
            if(cOrder.iPriceInTicks >= cDataPoint.iAskInTicks)
            {
                long iAdjustMktQty = calcuatedAdjustAskQty(cDataPoint.iAskInTicks, cDataPoint.iAskSize);

                if(cOrder.iRemainQty <= iAdjustMktQty)
                {
                    iFilledQty = cOrder.iRemainQty;
                    if(bIsAggresorOrder)
                    {
                        iFilledPrice = cDataPoint.iAskInTicks;
                    }
                    else
                    {
                        iFilledPrice = cOrder.iPriceInTicks;
                    }
                    cOrder.iRemainQty = 0;
                    cOrder.eState = ExecutorSimOrder::INACTIVE;            
                    _iTotalOrdersInMarket = _iTotalOrdersInMarket - 1; 
                }
                else
                {
                    // order took all best ask, and bid over. front of the queue now
                    cOrder.iQueuePosition = 0;
                    iFilledQty = iAdjustMktQty;
                    if(bIsAggresorOrder)
                    {
                        iFilledPrice = cDataPoint.iAskInTicks;
                    }
                    else
                    {
                        iFilledPrice = cOrder.iPriceInTicks;
                    }
                    cOrder.iRemainQty = cOrder.iRemainQty - iFilledQty;
                    cOrder.bOrderCrossedMarket = true;
                }            
            }
            else
            {
                cOrder.bOrderCrossedMarket = false;
            }
        }
        else
        {
            if(cOrder.iPriceInTicks <= cDataPoint.iBidInTicks)
            {
                long iAdjustMktQty = calcuatedAdjustBidQty(cDataPoint.iBidInTicks, cDataPoint.iBidSize);

                if(abs(cOrder.iRemainQty) <= iAdjustMktQty)
                {
                    iFilledQty = cOrder.iRemainQty;
                    if(bIsAggresorOrder)
                    {
                        iFilledPrice = cDataPoint.iBidInTicks;
                    }
                    else
                    {
                        iFilledPrice = cOrder.iPriceInTicks;
                    }
                    cOrder.iRemainQty = 0;
                    cOrder.eState = ExecutorSimOrder::INACTIVE;             
                    _iTotalOrdersInMarket = _iTotalOrdersInMarket - 1;
                }
                else
                {
                    // order took all best bid, and offered over. front of the queue now
                    cOrder.iQueuePosition = 0;
                    iFilledQty = iAdjustMktQty * -1;
                    if(bIsAggresorOrder)
                    {
                        iFilledPrice = cDataPoint.iBidInTicks;
                    }
                    else
                    {
                        iFilledPrice = cOrder.iPriceInTicks;
                    }
                    cOrder.iRemainQty = cOrder.iRemainQty - iFilledQty;
                    cOrder.bOrderCrossedMarket = true;
                }            
            }
            else
            {
                cOrder.bOrderCrossedMarket = false;
            }
        }

        if(iFilledQty != 0)
        {
            if(_bIsTheo == false)
            {
                updateRemovedLiqFromTrade(iFilledPrice, iFilledQty);
            }

            _vSimPortfolios[cOrder.iPortfolioID].iPortfolioPosition = _vSimPortfolios[cOrder.iPortfolioID].iPortfolioPosition + iFilledQty;
            addNewFill(iEventEpochTime, cOrder.iPortfolioID, iFilledQty, iFilledPrice);

            _cLogger << "Order filled for strategy " << cOrder.iPortfolioID << " qty " << iFilledQty << " price " << iFilledPrice << " Portfolio Position is " << _vSimPortfolios[cOrder.iPortfolioID].iPortfolioPosition << " remaining qty " << cOrder.iRemainQty << "\n";

            stringstream cStringStream;
            if(_bIsLiquidator == false)
            {
                cStringStream << "Order Filled qty " << iFilledQty << " price " << iFilledPrice << " product pos " <<  _vSimPortfolios[cOrder.iPortfolioID].iPortfolioPosition;
            }
            else
            {
                cStringStream << "Fast Liquidation Order Filled qty " << iFilledQty << " price " << iFilledPrice << " product pos " <<  _vSimPortfolios[cOrder.iPortfolioID].iPortfolioPosition;
            }
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _sProduct, cStringStream.str());
       }

        if(_bIOC == true)
        {
            _cLogger << cDataPoint.iEpochTimeStamp << "|" <<  cDataPoint.iBidSize << "|" << cDataPoint.iBidInTicks << "|" << cDataPoint.iAskInTicks << "|" << cDataPoint.iAskSize << "|" << cDataPoint.iLastInTicks << "|" << cDataPoint.iTradeSize << "|" << cDataPoint.iAccumuTradeSize << "\n";


            if(abs(cOrder.iRemainQty) != 0)
            {
                cOrder.iRemainQty = 0;
                cOrder.eState = ExecutorSimOrder::INACTIVE;            
                _iTotalOrdersInMarket = _iTotalOrdersInMarket - 1; 

                _cLogger << "confirm IOC order delete for strategy " << cOrder.iPortfolioID << "\n";

                stringstream cStringStream;
                cStringStream << "IOC Order cancel acked.";
                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _sProduct, cStringStream.str());
            }
        }
    }
} 

void ExecutorSim::addNewFill(long iFillTimeStamp, int iPortfolioID, long iFilledQty, long iFillPriceInTicks)
{
    _vPortfolioSimTransactions[iPortfolioID].push_back(SimTransaction());
    (_vPortfolioSimTransactions[iPortfolioID].end()-1)->iEpochTimeStamp = iFillTimeStamp * 1000;
    strcpy((_vPortfolioSimTransactions[iPortfolioID].end()-1)->sProduct, _sProduct.c_str());
    (_vPortfolioSimTransactions[iPortfolioID].end()-1)->iQty = iFilledQty;
    (_vPortfolioSimTransactions[iPortfolioID].end()-1)->dPrice = iFillPriceInTicks * _dTickSize;

    if(_vSimPortfolios[iPortfolioID].pSchedulerBase != NULL)
    {
        _vSimPortfolios[iPortfolioID].pSchedulerBase->onFill(_sProduct, iFilledQty, iFillPriceInTicks * _dTickSize, _bIsLiquidator);
    }
    _absTotalFillQty = _absTotalFillQty + abs(iFilledQty);
}

void ExecutorSim::setOrderQueuePosition(ExecutorSimOrder& cOrder)
{
    cOrder.iTradingSizeSeen = 0;

    if(cOrder.iRemainQty > 0)
    {
        if(cOrder.iPriceInTicks == pgetLastDataPoint()->iBidInTicks)
        {
            cOrder.iQueuePosition = pgetLastDataPoint()->iBidSize;
        }
        else if(cOrder.iPriceInTicks > pgetLastDataPoint()->iBidInTicks)
        {
            cOrder.iQueuePosition = 0;
        }
    }
    else
    {
        if(cOrder.iPriceInTicks == pgetLastDataPoint()->iAskInTicks)
        {
            cOrder.iQueuePosition = pgetLastDataPoint()->iAskSize;
        }
        else if(cOrder.iPriceInTicks < pgetLastDataPoint()->iAskInTicks)
        {
            cOrder.iQueuePosition = 0;
        }
    }

    cOrder.iMinMarketSizeSeen = cOrder.iQueuePosition;
    _cLogger << "new order queue pos is " << cOrder.iQueuePosition << "\n";
}


void ExecutorSim::updatePassiveOrder(ExecutorSimOrder& cOrder, GridData& cNewDataPoint)
{
    if(cOrder.bOrderCrossedMarket == false && cOrder.eState != ExecutorSimOrder::INACTIVE && cOrder.eState != ExecutorSimOrder::PENDING_CREATION && cOrder.eState != ExecutorSimOrder::PENDING_SUBMIT_CREATION)
    {
        bool bOrderCrossed = false;
        if(cOrder.iRemainQty > 0)
        {
            if(cNewDataPoint.iLastInTicks <= cOrder.iPriceInTicks)
            {
                bOrderCrossed = true;
            }
        }
        else
        {
            if(cNewDataPoint.iLastInTicks >= cOrder.iPriceInTicks)
            {
                bOrderCrossed = true;
            }
        }

        if(cNewDataPoint.iTradeSize != 0 && bOrderCrossed)
        {
            if(cNewDataPoint.iTradeSize >= cOrder.iQueuePosition)
            {
                long iPotentialMatchSize = 0;

/*
                if(cOrder.iPriceInTicks == cNewDataPoint.iAskInTicks &&
                   cNewDataPoint.iAskSize == cNewDataPoint.iTradeSize)
                {
                    iPotentialMatchSize = cNewDataPoint.iTradeSize;
                }
                else if(cOrder.iPriceInTicks == cNewDataPoint.iBidInTicks &&
                        cNewDataPoint.iBidSize == cNewDataPoint.iTradeSize)
                {
                    iPotentialMatchSize = cNewDataPoint.iTradeSize;
                }
                else
                {
*/
                    iPotentialMatchSize = cNewDataPoint.iTradeSize - cOrder.iQueuePosition;
//                }

                long iFilledQty = 0;

                if(iPotentialMatchSize >= abs(cOrder.iRemainQty))
                {
                    iFilledQty = cOrder.iRemainQty;
                    cOrder.eState = ExecutorSimOrder::INACTIVE;
                    cOrder.iRemainQty = 0;
                    _iTotalOrdersInMarket = _iTotalOrdersInMarket - 1;
                }
                else
                {
                    iFilledQty = iPotentialMatchSize;
                    if(cOrder.iRemainQty < 0)
                    {
                        iFilledQty = iFilledQty * -1;
                    }
                    cOrder.iRemainQty = cOrder.iRemainQty - iFilledQty;
                    cOrder.iQueuePosition = 0;
                }

                if(iFilledQty != 0)
                {
                    _vSimPortfolios[cOrder.iPortfolioID].iPortfolioPosition = _vSimPortfolios[cOrder.iPortfolioID].iPortfolioPosition + iFilledQty;
                    addNewFill(cNewDataPoint.iEpochTimeStamp, cOrder.iPortfolioID, iFilledQty, cOrder.iPriceInTicks);

                    _cLogger << "Order filled for strategy " << cOrder.iPortfolioID << " qty " << iFilledQty << " price " << cOrder.iPriceInTicks << " Portfolio Position is " << _vSimPortfolios[cOrder.iPortfolioID].iPortfolioPosition << " remaining qty " << cOrder.iRemainQty << "\n";

                    stringstream cStringStream;
                    if(_bIsLiquidator == false)
                    {
                        cStringStream << "Order Filled qty " << iFilledQty << " price " << cOrder.iPriceInTicks << " product pos " <<  _vSimPortfolios[cOrder.iPortfolioID].iPortfolioPosition;
                    }
                    else
                    {
                        cStringStream << "Fast Liquidation Order Filled qty " << iFilledQty << " price " << cOrder.iPriceInTicks << " product pos " <<  _vSimPortfolios[cOrder.iPortfolioID].iPortfolioPosition;
                    }
                    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _sProduct, cStringStream.str());

                    if(_bIsTheo == false)
                    {
                        // remove matched size from trade
                        cNewDataPoint.iTradeSize = cNewDataPoint.iTradeSize - abs(iFilledQty);
                    }
                }
            }
        }

        if(_pLoadedData[_iCurrentDataIdx].iTradeSize != 0)
        {
            updateOrderQueueFromTrade(cOrder);
        }
        else
        {
            updateOrderQueueFromSizeChange(cOrder);
        }

        _cLogger << "order updated queue pos is " << cOrder.iQueuePosition << "\n";
    }
}

void ExecutorSim::updateOrderQueueFromTrade(ExecutorSimOrder& cOrder)
{
    if(cOrder.iQueuePosition > 0)
    {
        if(_pLoadedData[_iCurrentDataIdx].iLastInTicks == cOrder.iPriceInTicks)
        {
            cOrder.iQueuePosition = cOrder.iQueuePosition - _pLoadedData[_iCurrentDataIdx].iTradeSize;

            if(cOrder.iQueuePosition < 0)
            {
                cOrder.iQueuePosition = 0;
            }
        }
    }
}

void ExecutorSim::updateOrderQueueFromSizeChange(ExecutorSimOrder& cOrder)
{
    if(cOrder.iQueuePosition > 0)
    {
        if(cOrder.iRemainQty > 0)
        {
            if(_pLoadedData[_iCurrentDataIdx].iBidInTicks == cOrder.iPriceInTicks)
            {
                if(_pLoadedData[_iCurrentDataIdx].iBidSize < cOrder.iQueuePosition)
                {
                    cOrder.iQueuePosition = _pLoadedData[_iCurrentDataIdx].iBidSize;
                }
            }
        }
        else
        {
            if(_pLoadedData[_iCurrentDataIdx].iAskInTicks == cOrder.iPriceInTicks)
            {
                if(_pLoadedData[_iCurrentDataIdx].iAskSize < cOrder.iQueuePosition)
                {
                    cOrder.iQueuePosition = _pLoadedData[_iCurrentDataIdx].iAskSize;
                }
            }
        }
    }
}

/*
void ExecutorSim::updateOrderQueuePosition(ExecutorSimOrder& cOrder)
{
    if(cOrder.iQueuePosition > 0)
    {
        if(cOrder.iRemainQty > 0)
        {
            if(_pLoadedData[_iCurrentDataIdx].iLastInTicks < cOrder.iPriceInTicks)
            {
                cOrder.iQueuePosition = 0;
            }

            if(cOrder.iQueuePosition > 0)
            {
                if(_pLoadedData[_iCurrentDataIdx].iBidInTicks < cOrder.iPriceInTicks)
                {
                    cOrder.iQueuePosition = 0;
                }
            }

            if(cOrder.iQueuePosition > 0)
            {
                if(_pLoadedData[_iCurrentDataIdx].iLastInTicks == cOrder.iPriceInTicks)
                {
                    cOrder.iTradingSizeSeen = cOrder.iTradingSizeSeen + _pLoadedData[_iCurrentDataIdx].iTradeSize;

                    if(_pLoadedData[_iCurrentDataIdx].iBidSize - _pLoadedData[_iCurrentDataIdx].iTradeSize < cOrder.iMinMarketSizeSeen)
                    {
                        cOrder.iQueueAdjustment = cOrder.iQueueAdjustment + _pLoadedData[_iCurrentDataIdx].iTradeSize;
                    }
                }

                if(_pLoadedData[_iCurrentDataIdx].iBidInTicks == cOrder.iPriceInTicks)
                {
                    if(_pLoadedData[_iCurrentDataIdx].iBidSize < cOrder.iMinMarketSizeSeen)
                    {
                        cOrder.iMinMarketSizeSeen = _pLoadedData[_iCurrentDataIdx].iBidSize;
                    }
                }

_cLogger << "iTradingSizeSeen " << cOrder.iTradingSizeSeen << " iMinMarketSizeSeen " << cOrder.iMinMarketSizeSeen << "\n";

                cOrder.iQueuePosition = cOrder.iMinMarketSizeSeen - cOrder.iTradingSizeSeen + cOrder.iQueueAdjustment;
            }
        }
        else
        {
            if(_pLoadedData[_iCurrentDataIdx].iLastInTicks > cOrder.iPriceInTicks)
            {
                cOrder.iQueuePosition = 0;
            }

            if(cOrder.iQueuePosition > 0)
            {
                if(_pLoadedData[_iCurrentDataIdx].iAskInTicks > cOrder.iPriceInTicks)
                {
                    cOrder.iQueuePosition = 0;
                }
            }

            if(cOrder.iQueuePosition > 0)
            {
                if(_pLoadedData[_iCurrentDataIdx].iLastInTicks == cOrder.iPriceInTicks)
                {
                    cOrder.iTradingSizeSeen = cOrder.iTradingSizeSeen + _pLoadedData[_iCurrentDataIdx].iTradeSize;

                    if(_pLoadedData[_iCurrentDataIdx].iAskSize - _pLoadedData[_iCurrentDataIdx].iTradeSize < cOrder.iMinMarketSizeSeen)
                    {
                        cOrder.iQueueAdjustment = cOrder.iQueueAdjustment + _pLoadedData[_iCurrentDataIdx].iTradeSize;
                    }
                }

                if(_pLoadedData[_iCurrentDataIdx].iAskInTicks == cOrder.iPriceInTicks)
                {
                    if(_pLoadedData[_iCurrentDataIdx].iAskSize < cOrder.iMinMarketSizeSeen)
                    {
                        cOrder.iMinMarketSizeSeen = _pLoadedData[_iCurrentDataIdx].iAskSize;
                    }

                    if(_pLoadedData[_iCurrentDataIdx].iLastInTicks == cOrder.iPriceInTicks)
                    {
                        if(_pLoadedData[_iCurrentDataIdx].iAskSize - _pLoadedData[_iCurrentDataIdx].iTradeSize < cOrder.iMinMarketSizeSeen)
                        {
                            cOrder.iMinMarketSizeSeen = cOrder.iMinMarketSizeSeen + _pLoadedData[_iCurrentDataIdx].iTradeSize;
                        }
                    }
                }

_cLogger << "iTradingSizeSeen " << cOrder.iTradingSizeSeen << " iMinMarketSizeSeen " << cOrder.iMinMarketSizeSeen << "\n";

                cOrder.iQueuePosition = cOrder.iMinMarketSizeSeen - cOrder.iTradingSizeSeen + cOrder.iQueueAdjustment;
            }
        }

        if(cOrder.iQueuePosition < 0)
        {
            cOrder.iQueuePosition = 0;
        }
    }

    _cLogger << "order updated queue pos is " << cOrder.iQueuePosition << "\n";
}
*/

long ExecutorSim::getOrderBestPrice(ExecutorSimOrder& cExecutorSimOrder, const GridData& cDataPoint)
{
    long iOrderBestPrice = 0;

    long iAdjustedAskInTicks = cDataPoint.iAskInTicks;
    long iAdjustedBidInTicks = cDataPoint.iBidInTicks;

/*
    if(cExecutorSimOrder.iMktAdjustQty != 0)
    { 
        if(cDataPoint.iAskInTicks == cExecutorSimOrder.iMktAdjustPriceInTicks)
        {
            iAdjustedAskSize = iAdjustedAskSize - cExecutorSimOrder.iMktAdjustQty;

            if(iAdjustedAskSize <= 0)
            {
                iAdjustedAskInTicks = iAdjustedAskInTicks + 1;
            }
        }
        else if(cDataPoint.iBidInTicks == cExecutorSimOrder.iMktAdjustPriceInTicks)
        {
            iAdjustedBidSize = iAdjustedBidSize - cExecutorSimOrder.iMktAdjustQty;

            if(iAdjustedBidSize <= 0)
            {
                iAdjustedBidInTicks = iAdjustedBidInTicks - 1;
            }
        }

        if(iAdjustedAskInTicks - iAdjustedBidInTicks > 1)
        {
            dAdjustedWeightMidInTicks = ((double)iAdjustedAskInTicks + (double)iAdjustedBidInTicks) / 2;
        }
        else
        {
            dAdjustedWeightMidInTicks = (double)iAdjustedBidInTicks + (double)iAdjustedBidSize / (double)(iAdjustedBidSize + iAdjustedAskSize); 
        }
    }
*/

    if(cExecutorSimOrder.iPendingRemainQty > 0)
    {
        if(_bIsLiquidator == false && _bIOC == false)
        {
            if(iAdjustedAskInTicks - iAdjustedBidInTicks > 1 && 
               _bJoinMidPrice == true)
            {
                iOrderBestPrice = iAdjustedBidInTicks + 1;
            }
            else
            {
                if(iAdjustedAskInTicks - iAdjustedBidInTicks > 2)
                {
                    if(cDataPoint.iBidSize > 10)
                    {
                        iOrderBestPrice = iAdjustedBidInTicks;
                    }
                    else
                    {
                        if(cExecutorSimOrder.iPriceInTicks != iAdjustedBidInTicks)
                        {
                            iOrderBestPrice = iAdjustedBidInTicks - 1;
                        }
                        else
                        {
                            iOrderBestPrice = cExecutorSimOrder.iPriceInTicks;
                        }
                    }
                }
                else
                {
                    iOrderBestPrice = iAdjustedBidInTicks;
                } 
            }
        }
        else
        {
            iOrderBestPrice = iAdjustedAskInTicks;
        }
    }
    else
    {
        if(_bIsLiquidator == false && _bIOC == false)
        {
            if(iAdjustedAskInTicks - iAdjustedBidInTicks > 1 &&
               _bJoinMidPrice == true)
            {
                iOrderBestPrice = iAdjustedAskInTicks - 1;
            }
            else
            {
                if(iAdjustedAskInTicks - iAdjustedBidInTicks > 2)
                {
                    if(cDataPoint.iAskSize > 10)
                    {
                        iOrderBestPrice = iAdjustedAskInTicks;
                    }
                    else
                    {
                        if(cExecutorSimOrder.iPriceInTicks != iAdjustedAskInTicks)
                        {
                            iOrderBestPrice = iAdjustedAskInTicks + 1;
                        }
                        else
                        {
                            iOrderBestPrice = cExecutorSimOrder.iPriceInTicks;
                        }
                    }
                }
                else
                {
                    iOrderBestPrice = iAdjustedAskInTicks;
                }
            }
        }
        else
        {
            iOrderBestPrice = iAdjustedBidInTicks;
        }
    }

    return iOrderBestPrice;
}

long ExecutorSim::getOrderMaxPrice(ExecutorSimOrder& cExecutorSimOrder, const GridData& cNewDataPoint)
{
    long iOrderMaxPrice = 0;

    if(cExecutorSimOrder.iPendingRemainQty > 0)
    {
        if(cNewDataPoint.iAskInTicks - cNewDataPoint.iBidInTicks > 1)      
        {
            iOrderMaxPrice = (cNewDataPoint.iAskInTicks + cNewDataPoint.iBidInTicks) / 2;
        }
        else
        {
            iOrderMaxPrice = cNewDataPoint.iAskInTicks;
        }
    }
    else
    {
        if(cNewDataPoint.iAskInTicks - cNewDataPoint.iBidInTicks > 1)      
        {
            iOrderMaxPrice = (cNewDataPoint.iAskInTicks + cNewDataPoint.iBidInTicks) / 2;
        }
        else
        {
            iOrderMaxPrice = cNewDataPoint.iBidInTicks;
        }
    }

    return iOrderMaxPrice;
}

void ExecutorSim::applyTimeToOrders(long iCurrentEpochTime)
{
    for(vector<vector<ExecutorSimOrder>>::iterator stratItr = _vStratgyOrders.begin();
        stratItr != _vStratgyOrders.end();
        stratItr++)
    {
        for(vector<ExecutorSimOrder>::iterator orderItr = stratItr->begin();
            orderItr != stratItr->end();)
        {
            while(orderItr->qOrderMsgHistorySecond.size() != 0 && iCurrentEpochTime - orderItr->qOrderMsgHistorySecond.front() > 1000000)
            {
                orderItr->qOrderMsgHistorySecond.pop_front();
            }

            if(orderItr->eState == ExecutorSimOrder::PENDING_SUBMIT_CREATION)
            {
                if(orderItr->iConfirmTime <= iCurrentEpochTime)
                {
                    GridData* _pLastDataPoint = pgetLastDataPoint();

                    bool bMarketPriceValid = true;
                    if(_bIsLiquidator == false)
                    {
                        if(_bIOC == true && _pLastDataPoint->iAskInTicks - _pLastDataPoint->iBidInTicks > _iIOCSpreadWidthLimit)
                        {
                            bMarketPriceValid = false;
                        }
                    }

                    if(bMarketPriceValid == true)
                    {
                        orderItr->eState = ExecutorSimOrder::PENDING_CREATION;
                        orderItr->iConfirmTime = orderItr->iConfirmTime + _iSubmitLatency;

                        orderItr->iPendingPriceInTicks = getOrderBestPrice(*orderItr, *_pLastDataPoint);

                        _cLogger << "submit new order for strategy " << orderItr->iPortfolioID << " qty " << orderItr->iPendingRemainQty << " pending price " << orderItr->iPendingPriceInTicks << " confirm time " << orderItr->iConfirmTime << std::endl ;

                        stringstream cStringStream;
                        cStringStream << "Submitting new order qty " << orderItr->iPendingRemainQty << " price " <<  orderItr->iPendingPriceInTicks  << ".";
                        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _sProduct, cStringStream.str());

                        checkPendingOrderConfirm(*orderItr, iCurrentEpochTime);
                    }
                    else
                    {
                        _cLogger << "ignore new order for strategy " << orderItr->iPortfolioID << " qty " << orderItr->iPendingRemainQty << " spread bigger than " << _iIOCSpreadWidthLimit << std::endl ;
                    }
                }
            }
            else if(orderItr->eState == ExecutorSimOrder::PENDING_SUBMIT_DELETE)
            {
                if(orderItr->iConfirmTime <= iCurrentEpochTime)
                {
                    orderItr->eState = ExecutorSimOrder::PENDING_DELETE;
                    orderItr->iConfirmTime = orderItr->iConfirmTime + _iAmendLatency;

                    _cLogger << "submit new order delete for strategy " << orderItr->iPortfolioID << " confirm time " << orderItr->iConfirmTime << "\n";

                    stringstream cStringStream;
                    cStringStream << "Deleting order.";
                    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _sProduct, cStringStream.str());

                    checkPendingOrderDelete(*orderItr, iCurrentEpochTime);
                }
            }
            else if(orderItr->eState == ExecutorSimOrder::PENDING_SUBMIT_CHANGE)
            {
                if(orderItr->iConfirmTime <= iCurrentEpochTime)
                {
                    orderItr->eState = ExecutorSimOrder::PENDING_CHANGE;
                    orderItr->iConfirmTime = orderItr->iConfirmTime + _iSubmitLatency;

                    _cLogger << "submit new order change for strategy " << orderItr->iPortfolioID << " new qty " << orderItr->iPendingRemainQty << " new price " << orderItr->iPendingPriceInTicks << " confirm time " << orderItr->iConfirmTime << "\n";

                    stringstream cStringStream;
                    cStringStream << "Replacing order qty " << orderItr->iPendingRemainQty << " price " <<  orderItr->iPendingPriceInTicks  << ".";
                    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _sProduct, cStringStream.str());

                    checkPendingOrderConfirm(*orderItr, iCurrentEpochTime);
                }
            }
            else if(orderItr->eState == ExecutorSimOrder::PENDING_UPDATE)
            {
                if(orderItr->iConfirmTime <= iCurrentEpochTime)
                {
                    GridData* _pLastDataPoint = pgetLastDataPoint();
                    changeOrderPrice(*orderItr, getOrderBestPrice(*orderItr, *_pLastDataPoint), orderItr->iConfirmTime);
                    checkPendingPriceOrderConfirm(*orderItr, iCurrentEpochTime);
                }
            }
            else if(orderItr->eState == ExecutorSimOrder::PENDING_CREATION)
            {
                checkPendingOrderConfirm(*orderItr, iCurrentEpochTime);
            }
            else if(orderItr->eState == ExecutorSimOrder::PENDING_DELETE)
            {
                checkPendingOrderDelete(*orderItr, iCurrentEpochTime);
            }
            else if(orderItr->eState == ExecutorSimOrder::PENDING_CHANGE)
            {
                checkPendingOrderConfirm(*orderItr, iCurrentEpochTime);
            }
            else if(orderItr->eState == ExecutorSimOrder::PENDING_PRICE_CHANGE)
            {
                checkPendingPriceOrderConfirm(*orderItr, iCurrentEpochTime);
            }

            if(orderItr->eState == ExecutorSimOrder::INACTIVE)
            {
                orderItr = stratItr->erase(orderItr);
            }
            else
            {
                orderItr++;
            }
        }
    }
}

void ExecutorSim::newSignal(TradeSignal cTradeSignal, long iNextSignalTimeStamp)
{
    _cLogger << "New Signal for strategy " << cTradeSignal.iPortfolioID  << " Time " << cTradeSignal.iEpochTimeStamp << " Desired Position " << cTradeSignal.iDesiredPos << " Signal State " << cTradeSignal.iSignalState << " Market Order " << cTradeSignal.bMarketOrder << " Next Signal Time Stamp " << iNextSignalTimeStamp << "\n";

    _cLogger << "Strategy " << cTradeSignal.iPortfolioID << " Pos: " << _vSimPortfolios[cTradeSignal.iPortfolioID].iPortfolioPosition << "\n";

    applyTimeToOrders(cTradeSignal.iEpochTimeStamp);

    _vSimPortfolios[cTradeSignal.iPortfolioID].cCurrentSignal = cTradeSignal;

    adjustOrderForSignal(cTradeSignal);

    if(_iTotalOrdersInMarket != 0)
    {   
        if(bloadDataForSignal(cTradeSignal.iEpochTimeStamp) == true)
        {
            // only keep on processing if we have more data
            while(true) 
            {
                bool bNeedMoreData = true;   

                for(;_iCurrentDataIdx < _iNumLoadedDataPoints;_iCurrentDataIdx++)
                {
                    // release aggregated update
//cerr << "iNextSignalTimeStamp is " << iNextSignalTimeStamp << "\n";
                    //loop through data
                    if(iNextSignalTimeStamp != 0 && iNextSignalTimeStamp < _pLoadedData[_iCurrentDataIdx].iEpochTimeStamp)
                    {
//cerr << "next signal time reached. market update time " << _cAggregatedUpdate.iEpochTimeStamp << "\n";
                        bNeedMoreData = false;
                        break;
                    }
                    else
                    {
                        if(_bLogMarketData == true)
                        { 
                            _cLogger << _pLoadedData[_iCurrentDataIdx].iEpochTimeStamp << "|" <<  _pLoadedData[_iCurrentDataIdx].iBidSize << "|" << _pLoadedData[_iCurrentDataIdx].iBidInTicks << "|" << _pLoadedData[_iCurrentDataIdx].iAskInTicks << "|" << _pLoadedData[_iCurrentDataIdx].iAskSize << "|" << _pLoadedData[_iCurrentDataIdx].iLastInTicks << "|" << _pLoadedData[_iCurrentDataIdx].iTradeSize << "|" << _pLoadedData[_iCurrentDataIdx].iAccumuTradeSize << "\n";
                        } 

                        if(_pLoadedData[_iCurrentDataIdx].iAskInTicks > _pLoadedData[_iCurrentDataIdx].iBidInTicks && _pLoadedData[_iCurrentDataIdx].iBidSize > 0 && _pLoadedData[_iCurrentDataIdx].iAskInTicks > 0)
                        {
                            if(_bIsTheo == false)
                            {
                                updateRemovedLiqFromMarket(_pLoadedData[_iCurrentDataIdx]);
                            }

                            // apply data to orders
                            applyTimeToOrders(_pLoadedData[_iCurrentDataIdx].iEpochTimeStamp);

                            for(vector<vector<ExecutorSimOrder>>::iterator stratItr = _vStratgyOrders.begin();
                                stratItr != _vStratgyOrders.end();
                                stratItr++)
                            {
                                for(vector<ExecutorSimOrder>::iterator orderItr = stratItr->begin();
                                    orderItr != stratItr->end();)
                                {
                                    checkOrderCross(*orderItr, _pLoadedData[_iCurrentDataIdx], _pLoadedData[_iCurrentDataIdx].iEpochTimeStamp, false);
                                    updatePassiveOrder(*orderItr, _pLoadedData[_iCurrentDataIdx]);

                                    if(orderItr->eState == ExecutorSimOrder::INACTIVE)
                                    {
    //cerr << "erasing deleted or filled order \n";
                                        orderItr = stratItr->erase(orderItr);
                                    }
                                    else
                                    {
    // To be included for changing order price on tick update
                                        if(orderItr->eState == ExecutorSimOrder::ACTIVE ||
                                           orderItr->eState == ExecutorSimOrder::PENDING_UPDATE)
                                        {
                                            long iOrderNewBestPrice = getOrderBestPrice(*orderItr, _pLoadedData[_iCurrentDataIdx]);

                                            if(iOrderNewBestPrice != orderItr->iPriceInTicks)
                                            {
                                                changeOrderPrice(*orderItr, iOrderNewBestPrice, _pLoadedData[_iCurrentDataIdx].iEpochTimeStamp);   
                                            }
                                        }
    // To be included for changing order price on tick update

                                        orderItr++;
                                    }
                                }
                            }

                            if(_iTotalOrdersInMarket == 0)
                            {
    //cerr << "all no more orders in the market. stop processing signal \n";
                                _iCurrentDataIdx++;
                                bNeedMoreData = false;
                                break;
                            }
                        }
                    }
                }

                if(bNeedMoreData == true)
                {
                    _cLogger << "Trying to load more data \n";
                    if(bloadMoreData() != true)
                    {
                        // reached end of data file
                        _cLogger << "end of data file reached. No more simulation \n";
                        break;
                    }
                }
                else
                {
                    // next signal time reached or no more orders in the market
//cerr  << "next signal time reach or no more orders in the market \n";
                    break;
                }
            }
        }
        else
        {
            _cLogger << "end of data file reached. No more simulation \n";
        }
    }
//    cerr << "signal processed \n";
}

void ExecutorSim::writeTransactionsToFile(const string& sDate)
{
    string sH5Date = sDate.substr(0,4) + "-" + sDate.substr(4,2) + "-" + sDate.substr(6,2);

    for(unsigned int i = 0; i < _vPortfolioSimTransactions.size(); i++)
    {
        for(vector<SimTransaction>::iterator itr = _vPortfolioSimTransactions[i].begin(); itr != _vPortfolioSimTransactions[i].end(); itr++)
        {
            strcpy(itr->sDate, sH5Date.c_str());
        }

        SimTransaction* pSimTransactionsArray = new SimTransaction[_vPortfolioSimTransactions[i].size()];
        std::copy(_vPortfolioSimTransactions[i].begin(), _vPortfolioSimTransactions[i].end(), pSimTransactionsArray);
        hsize_t cDim[] = {_vPortfolioSimTransactions[i].size()};
        DataSpace cSpace(sizeof(cDim)/sizeof(hsize_t), cDim);

        DataSet* pDataSet = new DataSet(_vSimPortfolios[i].pH5File->createDataSet(sDate, *(_vSimPortfolios[i].pTransactionDataType), cSpace));
        pDataSet->write(pSimTransactionsArray, *(_vSimPortfolios[i].pTransactionDataType));

        delete pSimTransactionsArray;
        delete pDataSet;
    }
}

double ExecutorSim::dgetFillRatio()
{
    if(_absTotalNewQty != 0)
    {
        return (double)_absTotalFillQty / (double)_absTotalNewQty;
    }
    else
    {
        return 0.0;
    }
}

}
