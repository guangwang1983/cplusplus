#ifndef ExecutorSim_H
#define ExecutorSim_H

#include <string>
#include <cstring>
#include <vector>
#include <deque>
#include <H5Cpp.h>
#include <fstream>

#include "../EngineInfra/SimpleLogger.h"
#include "../EngineInfra/QuoteData.h"
#include "../EngineInfra/SchedulerBase.h"

using namespace H5;
using namespace std;

namespace KO
{

struct RemovedLiquidity
{
    long iPrice;
    long iSize;
};

struct SimTransaction
{
    long iEpochTimeStamp;
    char sProduct [12];
    long iQty;
    double dPrice;
    char sDate [12];
};

struct TradeSignal
{
    long iPortfolioID;
    long iEpochTimeStamp;
    long iDesiredPos;
    long iSignalState;
    bool bMarketOrder;
};

struct ExecutionPortfolio
{
    long                iPortfolioPosition;
    TradeSignal         cCurrentSignal;
    H5File*             pH5File;
    CompType*           pTransactionDataType;
    SchedulerBase*      pSchedulerBase;
};

struct ExecutorSimOrder
{
    ExecutorSimOrder()
    {
        eState = INACTIVE;        
    }

    enum State
    {
        INACTIVE,
        ACTIVE,
        PENDING_SUBMIT_CREATION,
        PENDING_CREATION,
        PENDING_SUBMIT_CHANGE,
        PENDING_CHANGE,
        PENDING_PRICE_CHANGE,
        PENDING_SUBMIT_DELETE,
        PENDING_UPDATE,
        PENDING_DELETE
    };
    
    long iPortfolioID;

    long iPriceInTicks;
    long iPendingPriceInTicks;

    long iRemainQty;
    long iPendingRemainQty;

    State eState;

    long iConfirmTime;

    long iQueuePosition;
    long iTradingSizeSeen;
    long iMinMarketSizeSeen;
    long iQueueAdjustment;

    bool bOrderCrossedMarket;

    bool bIsMarketOrder;

    deque<long> qOrderMsgHistorySecond; 
};

class ExecutorSim
{
public:
    enum SignalState
    {
        BUY,  //0
        SELL,  //1
        STOP,  //2
        FLAT,  //3
        FLAT_ALL_LONG,  //4
        FLAT_ALL_SHORT  //5
    };

    ExecutorSim(const string& sProduct, long iSubmitLatency, long iAmendLatency, double dTickSize, const string& sDataFile, long iExpoLimit, const string& sDate, const string& sLogPath, bool bWriteLog, bool bIsLiquidator, bool bLogMarketData, bool bIOC, int iIOCSpreadWidthLimit);
    ~ExecutorSim();

    const string& sgetProduct();

    int iaddPortfolio(H5File* pH5File, CompType* pTransactionDataType, SchedulerBase* pSchedulerBase);
    void newSignal(TradeSignal cTradeSignal, long iNextSignalTimeStamp);
    void writeTransactionsToFile(const string& sDate);

    void transferPosition(long iPortfolioID, long iAdditionalPosition);
    int igetPortfolioPosition(long iPortfolioID);

    double dgetFillRatio();

    // theo sim means no liquidity removing
    void setTheoreticalSim(bool bIsTheo);
private:
    string    _sDataFileName;
    H5File*   _pH5File;
    CompType* _pGridDataType;

    vector<vector<SimTransaction>> _vPortfolioSimTransactions;

    string _sProduct;
    vector<ExecutionPortfolio> _vSimPortfolios;
   
    GridData* _pLoadedData;
    long _iNumLoadedDataPoints;
    long _iCurrentDataIdx;
    long _iNextDataSetIdx;

    GridData _cPrevBatchLastPrint;

    deque<RemovedLiquidity> _qRemovedBidLiquidity;
    deque<RemovedLiquidity> _qRemovedAskLiquidity;

    long _iSubmitLatency;
    long _iAmendLatency;

    long _iTotalOrdersInMarket;
    double _dTickSize;
    long _iExpoLimit;

    bool _bIsLiquidator;

    vector<vector<ExecutorSimOrder>> _vStratgyOrders;

    SimpleLogger _cLogger;

    bool _bJoinMidPrice;

    long _absTotalNewQty;
    long _absTotalFillQty;

    bool _bLogMarketData;

    bool _bIOC;
    bool _bIsTheo;

    int _iIOCSpreadWidthLimit;

    long _iSecondMsgLimit;

    bool bloadHDF5(long iDataSetIdx);
    void findLastPrint(long iDataIdx);
    bool bloadDataForSignal(long iSignalTimeStamp);
    bool bloadMoreData();

    void adjustOrderForSignal(TradeSignal& cTradeSignal);
    void checkOrderCross(ExecutorSimOrder& cOrder, const GridData& cDataPoint, long iEventEpochTime, bool bIsAggresorOrder);
    void updatePassiveOrder(ExecutorSimOrder& cOrder, GridData& cNewDataPoint);
    void applyTimeToOrders(long iEpochTime);

    void setOrderQueuePosition(ExecutorSimOrder& cOrder);

    void updateOrderQueueFromTrade(ExecutorSimOrder& cOrder);
    void updateOrderQueueFromSizeChange(ExecutorSimOrder& cOrder);

    GridData* pgetLastDataPoint();
    void submitNewOrder(int iPortfolioID, long iNewQty, long iSubmitTime, bool bIsMarketOrder);
    void deleteOrder(ExecutorSimOrder& cExectorOrder, long iDeleteTime);
    void changeOrder(ExecutorSimOrder& cExectorOrder, long iNewQty, long iNewPriceInTicks, long iChangeTime);
    void changeOrderPrice(ExecutorSimOrder& cExectorOrder, long iNewPriceInTicks, long iChangeTime);
    void deleteAllPortfolioOrders(int iPortfolioID, long iDeleteTime);

    long igetOrderQty(const ExecutorSimOrder& cOrder);

    void checkPendingPriceOrderConfirm(ExecutorSimOrder& cOrder, long iEpochTime);
    void checkPendingOrderConfirm(ExecutorSimOrder& cOrder, long iEpochTime);
    void checkPendingOrderDelete(ExecutorSimOrder& cOrder, long iEpochTime);

    long calcuatedAdjustBidQty(long iMktPrice, long iMktQty);
    long calcuatedAdjustAskQty(long iMktPrice, long iMktQty);

    void updateRemovedLiqFromTrade(long iFillPrice, long iFilledQty);
    void updateRemovedLiqFromMarket(const GridData& cDataPoint);

    long getOrderBestPrice(ExecutorSimOrder& cExecutorSimOrder, const GridData& cNewDataPoint);
    long getOrderMaxPrice(ExecutorSimOrder& cExecutorSimOrder, const GridData& cNewDataPoint);
//    void updateOrderPrice(ExecutorSimOrder& cExecutorSimOrder, const GridData& cNewDataPoint, long iSubmitTime);

    void addNewFill(long iFillTimeStamp, int iPortfolioID, long iFilledQty, long iFillPriceInTicks);
};

}

#endif
