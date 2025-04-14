#ifndef KOScheduler_H
#define KOScheduler_H

#include "SchedulerBase.h"
#include "HistoricDataRegister.h"
#include "../ExecutorSim/ExecutorSim.h"

namespace KO
{

class KOScheduler : public SchedulerBase
{
public:
    KOScheduler(const string& sSimType, SchedulerConfig &cfg);
    ~KOScheduler();

    bool init();
    void run();

    void setFXSim(bool bFXSim);

    void applyPriceUpdateOnTime(KOEpochTime cNextTimeStamp);

    bool bisLiveTrading();

    KOEpochTime cgetCurrentTime();

    void addNewPriceUpdateCall(KOEpochTime cCallTime);

    bool sendToExecutor(const string& sProduct, long iDesiredPos);
    bool sendToLiquidationExecutor(const string& sProduct, long iDesiredPos);
    void assignPositionToLiquidator(const string& sProduct, long iPosToLiquidate);

    double dgetFillRatio(const string& sProduct);

    void onFill(const string& sProduct, long iFilledQty, double dPrice, bool bIsLiquidator, InstrumentType eInstrumentType);

    virtual void updateAllPnL();
private:
    void initExecutorSims();

    void exitScheduler();

    // id for the executor simulator
    long _iPortfolioID;

    boost::shared_ptr<HistoricDataRegister> _pHistoricDataRegister;
    vector<ExecutorSim*> _vSimExecutors;
    vector<ExecutorSim*> _vLiqExecutors;

    vector<KOEpochTime> _vFirstOrderTime;
   
    bool _bExitSchedulerFlag;

    bool _bFXSim;
};

};

#endif
