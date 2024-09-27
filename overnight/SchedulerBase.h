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
        UNKNOWN_MANUAL_COMMAND
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

    struct OvernightPos
    {
        long iPos;
        double dPrice;
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

    virtual void onFill(const string& sProduct, long iFilledQty, double dPrice, bool bIsLiquidator);

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

    void loadOvernightPos();
    void writeOvernightResult();

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
    map<string, boost::shared_ptr< vector <string> > > _mProductFigureSlowFlat;
    map<string, boost::shared_ptr< vector <string> > > _mProductFigureFlatOff;
/* Figure Handling End */

/* Singal Handling */
    void registerSignalHandlers();
    static void limLiqAllTraders(int aSignal);
    static void resumeAllTraders(int aSignal);
    static void patientLiqAllTraders(int aSignal);
    static void fastLiqAllTraders(int aSignal);
    static void haltAllTraders(int aSignal);
    static void limitLiqSlotsLiquidator(int aSignal);
    static void fastLiqSlotsLiquidator(int aSignal);
    static void limitLiqAllLiquidator(int aSignal);
    static void fastLiqAllLiquidator(int aSignal);
    static void posPrint(int aSignal);
    static void limitLiquidateProduct(int aSignal);
    static void fastLiquidateProduct(int aSignal);
    static void patientLiquidateProduct(int aSignal);
    static void resetOrderStateHandler(int aSignal);
    static void printTheoTargetHandler(int aSignal);
    static void printSignalHandler(int aSignal);
    static SchedulerBase* _pInstance;

    void liquidatProduct(LiquidationType eLiquidationType);
    void printTheoTarget();
    void printSignal();
    virtual void resetOrderState() {}
/* Signal Handling End */

/* Scheduled trading action */
    void loadScheduledManualActionsFile();
    void loadScheduledSlotLiquidationFile();
    vector< pair<KOEpochTime, ManualCommandAction> > _vScheduledManualActions;
    vector< pair<KOEpochTime, vector<string> > > _vScheduledSlotLiquidation;
/* Scheduled trading action End */

    vector<int> _vProductLiquidationPos;
    vector<int> _vProductPos;

    vector<double> _vProductConsideration;
    vector<int> _vProductVolume;

    vector<double> _vProductStopLoss;
    vector<int> _vProductAboveSLInSec;

    vector<int> _vProductLiquidating;

    map<string, OvernightPos> _mDailyOvernightPos;
};


};


#endif
