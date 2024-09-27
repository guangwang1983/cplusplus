#ifndef SimulationExchange_H
#define SimulationExchange_H

#include "QuoteData.h"
#include "KOOrder.h"

using std::vector;

namespace KO
{

class KOScheduler;

class SimulationExchange 
{

public:
    SimulationExchange(KOScheduler* pScheduler);
    ~SimulationExchange();

    void addProductForSimulation(QuoteData* pQuoteData, long iOrderLatency);

    bool bsubmitOrder(KOOrderPtr pOrder, double dPrice, long iQty);
    bool bchangeOrderPrice(KOOrderPtr pOrder, double dNewPrice);
    bool bchangeOrder(KOOrderPtr pOrder, double dNewPrice, long iNewQty);
    bool bdeleteOrder(KOOrderPtr pOrder);

    void orderConfirmCallBack(KOOrderPtr pOrder);

    void newPriceUpdate(long iProductIndex);

private:
    bool bapplyOrderToMarketPrice(KOOrderPtr pOrder);
    bool bapplyPriceUpdateToOrder(KOOrderPtr pOrder);

    bool bfillOrder(KOOrderPtr pWorkingOrder, long iSizeTraded, bool bOrderCrossed, bool bAggressOrderAction);
    void resetQueue(KOOrderPtr pWorkingOrder);
    void updateQueue(KOOrderPtr pWorkingOrder);

    vector<QuoteData*>                    _vQuoteDatas;
    vector< vector <KOOrderPtr> >           _cWorkingOrderMap;
    vector<KOEpochTime>                     _vOrderLatencies;

    KOScheduler*                            _pKOScheduler;
};

}

#endif /* SimulationExchange_H */
