#ifndef TradeSignalMerger_H
#define TradeSignalMerger_H

#include <vector>
#include <map>
#include "../EngineInfra/Trade.h"

using namespace std;

namespace KO
{

class SchedulerBase;

class TradeSignalMerger
{

public:

    struct SlotSignal
    {
        string sSlotName;
        long iPendingDesiredPos;
        long iDesiredPos;
        long iPrevDesiredPos;
        int iSignalState;
        bool bMarketOrder;
        long iPos;
        bool bReady;
        bool bActivate;

        long getWorkingQty()
        {
            return iDesiredPos - iPos;
        }

        long getPrevWorkingQty()
        {
            return iPrevDesiredPos - iPos;
        }
    };

    TradeSignalMerger(SchedulerBase* pScheduler);
    ~TradeSignalMerger();

    void writeEoDResult(const string& sResultPath, const string& sTodayDate);

    int registerTradingSlot(const string& sProduct, const string& sSlotName, double dContractSize, double dDollarRate, double dTradingFee, double dDominatorUSDRate);

    void setSlotReady(const string& sProduct, long iSlotID);
    void activateSlot(const string& sProduct, long iSlotID);
    void deactivateSlot(const string& sProduct, long iSlotID);
    void updateSlotSignal(const string& sProduct, long iSlotID, long iDesiredPos, int iSignalState, bool bMarketOrder);
    void onFill(const string& sProduct, int iQty, double dPrice, bool bIsLiquidationFill, InstrumentType eInstrumentType);

    void takePosSnapShot();

    void printConfigPos();

private:
    void fullyFillAllOrders(vector<vector<SlotSignal>::iterator>& vOrderItrs);
    void prorataFillAllOrders(vector<vector<SlotSignal>::iterator>& vOrderItrs, long iTotalFillQty);
    void unwantedFillAllOrders(vector<vector<SlotSignal>::iterator>& vOrderItrs, long iTotalFillQty);

    void aggregateAndSend(const string& sProduct);
    
    SchedulerBase* _pScheduler;

    map<string, vector<SlotSignal>> _mSlotSignals;
    map<string, double> _mProductToContractSize;
    map<string, double> _mProductToDollarRate;
    map<string, double> _mProductFXDominatorUSDRate;
    map<string, double> _mProductToTradingFee;
    map<string, bool> _mProductPrintPos;
   
    vector<Trade> _vTotalTrades;

    int _iNextSlotID;

    stringstream _cInstradayPosSnapshot;
};

}

#endif /* TradeSignalMerger_H */
