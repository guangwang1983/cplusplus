#ifndef SchedulerBase_H
#define SchedulerBase_H

#include "SimpleLogger.h"
#include "QuoteData.h"
#include "SchedulerConfig.h"
#include "TimeEvent.h"
#include "EngineEvent.h"
#include "Figures.h"
#include "StaticDataHandler.h"
#include "TimerCallbackInterface.h"
#include "../TradeSignalMerger/TradeSignalMerger.h"

#include <queue>

using std::vector;
using std::priority_queue;
using std::deque;
using std::pair;

namespace KO
{

class TradeEngineBase;
class ContractAccount;

typedef boost::shared_ptr<TradeEngineBase> TradeEngineBasePtr;

class TimeEventComparison
{
public:
    TimeEventComparison(){}

    inline bool operator() (TimeEvent* lhs, TimeEvent* rhs) const
    {
        if(lhs->cgetCallTime() != rhs->cgetCallTime())
        {
            return lhs->cgetCallTime() < rhs->cgetCallTime();
        }
        else
        {
            if(lhs->egetTimeEventType() != rhs->egetTimeEventType())
            {
                return lhs->egetTimeEventType() < rhs->egetTimeEventType();
            }
            else
            {
                if(lhs->egetTimeEventType() == TimeEvent::EngineEvent)
                {
                    EngineEvent* lhsEngineEvent = dynamic_cast<EngineEvent*>(lhs);
                    EngineEvent* rhsEngineEvent = dynamic_cast<EngineEvent*>(rhs);

                    return lhsEngineEvent->igetEventType() < rhsEngineEvent->igetEventType();
                }
                else
                {
                    return false;
                }
            }
        }
    }
};

class SchedulerBase : public TimerCallbackInterface
{

public:
    enum LiquidationType
    {
        PATIENT_LIQ,
        LIMIT_LIQ,
        FAST_LIQ
    };

    enum ManualCommandAction
    {
        RESUME_ALL,
        PATIENT_ALL,
        LIMIT_ALL,
        FAST_ALL,
        HALT_ALL,
        LIMIT_ALL_LIQUIDATOR,
        FAST_ALL_LIQUIDATOR,
        LIMIT_SLOTS_LIQUIDATOR,
        FAST_SLOTS_LIQUIDATOR,
        OFF_LIQUIDATOR,
        POS_PRINT,
        LIMIT_PRODUCT,
        FAST_PRODUCT,
        PATIENT_PRODUCT,
        RESET_ORDER_STATE,
        PRINT_THEO_TARGET,
        PRINT_SIGNAL,
        FORBID_LP,
        UNKNOWN_MANUAL_COMMAND,
        NONE
    };

    struct ProductRisk
    {
        long iProductCID;
        long iMaxOrderSize;
        long iGlobalLimit;
        unsigned int iNumOrderPerSecondPerModel;
        long iOrderPriceDeviationInTicks;
        long iPos;
    };

    typedef boost::shared_ptr<ProductRisk> ProductRiskPtr;

    SchedulerBase(const string& sSimType, SchedulerConfig &cfg);
    ~SchedulerBase();

    bool preCommonInit();    
    bool postCommonInit();

    QuoteData* pregisterProduct(string sFullProductName, InstrumentType eInstrumentType);

    virtual KOEpochTime cgetCurrentTime() = 0;

    virtual bool bisLiveTrading() = 0;

    void wakeup(KOEpochTime cCallTime);

    SchedulerConfig& _cSchedulerCfg;

    void sortTimeEvent();

    void addNewWakeupCall(KOEpochTime cCallTime, TimerCallbackInterface* pTarget);
    void addNewFigureCall(TradeEngineBase* pTargetEngine, FigureCallPtr pNewFigureCall);
    void addNewEngineCall(TradeEngineBase* pTargetEngine, EngineEvent::EngineEventType eEventType, KOEpochTime cCallTime);

    void activateSlot(const string& sProduct, int iSlotID);
    void deactivateSlot(const string& sProduct, int iSlotID);
    void setSlotReady(const string& sProduct, int iSlotID);

    void updateSlotSignal(const string& sProduct, int iSlotID, long iDesiredPos, int iSignalState, bool bMarketOrder); 

    virtual bool sendToExecutor(const string& sProduct, long iDesiredPos) = 0;
    virtual bool sendToLiquidationExecutor(const string& sProduct, long iDesiredPos) = 0;
    virtual void assignPositionToLiquidator(const string& sProduct, long iPosToLiquidate) = 0;
    virtual double dgetFillRatio(const string& sProduct) { (void)sProduct; return 0; };

    virtual void onFill(const string& sProduct, long iFilledQty, double dPrice, bool bIsLiquidator, InstrumentType eInstrumentType);

    virtual void updateAllPnL(){};
protected:
    void newPriceUpdate(long iProductIndex);

    void addNewFigureCall(TradeEngineBasePtr pTradeEngine, string sFigureName, KOEpochTime cCallTime, KOEpochTime cFigureTime, FigureAction::options eFigureAction);

    void registerTradeEngines();
    bool registerPriceForEngine(const string& sFullProductName, const string& sAccount, int iLimit, TradeEngineBasePtr pTargetEngine);    

    virtual void exitScheduler() = 0;

    string sgetRootFromKOSymbol(const string& sKOSymbol);

    void processTimeEvents(KOEpochTime cTimeNow);

    void updateProductPnL(int iProductIndex);

    void checkProductPriceStatus(KOEpochTime cCallTime);
    std::map<string, bool> _mExchangeStalenessTriggered;   
 
    string _sSimType;

    boost::shared_ptr<StaticDataHandler> _pStaticDataHandler;
 
    vector<QuoteData*> _vContractQuoteDatas;
    vector<TradeEngineBasePtr> _vTradeEngines;
    vector< vector <TradeEngineBasePtr> > _cProductEngineMap;
    vector< boost::shared_ptr<ContractAccount> > _vContractAccount;

    KOEpochTime _cTimerStart;
    KOEpochTime _cTimerEnd;
    KOEpochTime _cActualStartedTime;
    KOEpochTime _cShutDownTime;
    KOEpochTime _cSlotFirstWakeupCallTime;
    bool _bShutDownTimeReached;

    SimpleLogger _cOrderActionLogger;
    SimpleLogger _cMarketDataLogger;

    void setEngineManualHalted(bool bNewValue);
    bool bgetEngineManualHalted();
    bool _bEngineManualHalted;    

    TradeSignalMerger* _pTradeSignalMerger;

/* Event handling */
    KOEpochTime _cCurrentKOEpochTime;
    priority_queue<TimeEvent*, vector<TimeEvent*>, TimeEventComparison> _cDynamicTimeEventQueue;
    deque<TimeEvent*> _vStaticTimeEventQueue;
/* Event handling End */

/* Figure Handling */
    void loadFigureFile();
    void loadProductFigureActionFile();
    void loadTodaysFigure(string sDate);
    vector< boost::shared_ptr<Figure> > _vAllFigures;
    map<string, boost::shared_ptr< vector <string> > > _mProductFigureFlat;
    map<string, boost::shared_ptr< vector <string> > > _mProductFigureHalt;
    map<string, boost::shared_ptr< vector <string> > > _mProductFigurePatient;
    map<string, boost::shared_ptr< vector <string> > > _mProductFigureFlatHitting;
    map<string, boost::shared_ptr< vector <string> > > _mProductFigureFlatFXHitting;
    map<string, boost::shared_ptr< vector <string> > > _mProductFigureFlatFXLongHitting;
    map<string, boost::shared_ptr< vector <string> > > _mProductFigureSlowFlat;
    map<string, boost::shared_ptr< vector <string> > > _mProductFigureFlatOff;
    map<string, boost::shared_ptr< vector <string> > > _mProductFigureFlatBig;
/* Figure Handling End */

/* Singal Handling */
    void registerSignalHandlers();

    static void setLimLiqAllTraders(int aSignal);
    static void setResumeAllTraders(int aSignal);
    static void setPatientLiqAllTraders(int aSignal);
    static void setFastLiqAllTraders(int aSignal);
    static void setHaltAllTraders(int aSignal);
    static void setLimitLiqSlotsLiquidator(int aSignal);
    static void setFastLiqSlotsLiquidator(int aSignal);
    static void setLimitLiqAllLiquidator(int aSignal);
    static void setFastLiqAllLiquidator(int aSignal);
    static void setPosPrint(int aSignal);
    static void setLimitLiquidateProduct(int aSignal);
    static void setFastLiquidateProduct(int aSignal);
    static void setPatientLiquidateProduct(int aSignal);
    static void setResetOrderStateHandler(int aSignal);
    static void setPrintTheoTargetHandler(int aSignal);
    static void setPrintSignalHandler(int aSignal);
    static void setAddForbiddenFXLP(int aSignal);

    void limLiqAllTraders(int aSignal);
    void resumeAllTraders(int aSignal);
    void patientLiqAllTraders(int aSignal);
    void fastLiqAllTraders(int aSignal);
    void haltAllTraders(int aSignal);
    void limitLiqSlotsLiquidator(int aSignal);
    void fastLiqSlotsLiquidator(int aSignal);
    void limitLiqAllLiquidator(int aSignal);
    void fastLiqAllLiquidator(int aSignal);
    void posPrint(int aSignal);
    void limitLiquidateProduct(int aSignal);
    void fastLiquidateProduct(int aSignal);
    void patientLiquidateProduct(int aSignal);
    void resetOrderStateHandler(int aSignal);
    void printTheoTargetHandler(int aSignal);
    void printSignalHandler(int aSignal);
    void addForbiddenFXLP(int aSignal);

    static SchedulerBase* _pInstance;

    void liquidatProduct(LiquidationType eLiquidationType);
    void printTheoTarget();
    void printSignal();
    void readForbiddenFXLPList();
    virtual void resetOrderState() {}
/* Signal Handling End */

/* Scheduled trading action */
    void loadScheduledManualActionsFile();
    void loadScheduledSlotLiquidationFile();
    vector< pair<KOEpochTime, ManualCommandAction> > _vScheduledManualActions;
    vector< pair<KOEpochTime, vector<string> > > _vScheduledSlotLiquidation;
/* Scheduled trading action End */

    ManualCommandAction _eNextPendingManualCommand;

    vector<int> _vProductLiquidationPos;
    vector<int> _vProductPos;

    vector<double> _vProductConsideration;
    vector<int> _vProductVolume;
    vector<string> _vProductTradingStatus;

    vector<double> _vProductStopLoss;
    vector<int> _vProductAboveSLInSec;

    vector<int> _vProductLiquidating;

    vector<string> _vForbiddenFXLPs;
};


};


#endif
