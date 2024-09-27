#ifndef KOScheduler_H
#define KOScheduler_H

#include "SchedulerBase.h"
#include "SimulationExchange.h"
#include "HistoricDataRegister.h"

namespace KO
{

class KOScheduler : public SchedulerBase
{
public:
    KOScheduler(SchedulerConfig &cfg);
    ~KOScheduler();

    bool init();
    void run();

    void applyPriceUpdateOnTime(KOEpochTime cNextTimeStamp);

    bool bsubmitOrder(KOOrderPtr pOrder, double dPrice, long iQty);
    bool bchangeOrderPrice(KOOrderPtr pOrder, double dNewPrice);
    bool bchangeOrder(KOOrderPtr pOrder, double dNewPrice, long iNewQty);
    bool bdeleteOrder(KOOrderPtr pOrder);

    bool bisLiveTrading();

    void onConfirm(KOOrderPtr pOrder);
    void onDelete(KOOrderPtr pOrder);
    void onAmendReject(KOOrderPtr pOrder);
    void onReject(KOOrderPtr pOrder);
    void onCancelReject(KOOrderPtr pOrder);
    void onFill(KOOrderPtr pOrder, double dPrice, long iQty);

    KOEpochTime cgetCurrentTime();

    void addNewOrderConfirmCall(SimulationExchange* pSimulationExchange, KOOrderPtr pOrder, KOEpochTime cCallTime);
    void addNewPriceUpdateCall(KOEpochTime cCallTime);
private:
    boost::shared_ptr<HistoricDataRegister> _pHistoricDataRegister;
    boost::shared_ptr<SimulationExchange> _pSimulationExchange;
};

};

#endif
