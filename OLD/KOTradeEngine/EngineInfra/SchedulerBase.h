#ifndef SchedulerBase_H
#define SchedulerBase_H

#include "SimpleLogger.h"
#include "QuoteData.h"
#include "SchedulerConfig.h"
#include "KOOrder.h"
#include "TimeEvent.h"
#include "EngineEvent.h"
#include "Figures.h"
#include "StaticDataHandler.h"
#include "SimulationExchange.h"
#include "TimerCallbackInterface.h"
#include "PositionServerConnection.h"

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
            return lhs->egetTimeEventType() < rhs->egetTimeEventType();
        }
    }
};

class SchedulerBase : public TimerCallbackInterface
{

friend class OrderRiskChecker;

public:
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

    typedef boost::shared_ptr<ProductRisk> ProductRiskPtr;

    SchedulerBase(SchedulerConfig &cfg);
    ~SchedulerBase();

    bool preCommonInit();    
    bool postCommonInit();

    QuoteData* pregisterProduct(string sFullProductName, InstrumentType eInstrumentType);

    virtual KOEpochTime cgetCurrentTime() = 0;

    virtual bool bsubmitOrder(KOOrderPtr pOrder, double dPrice, long iQty) = 0;
    virtual bool bchangeOrderPrice(KOOrderPtr pOrder, double dNewPrice) = 0;
    virtual bool bchangeOrder(KOOrderPtr pOrder, double dNewPrice, long iNewQty) = 0;
    virtual bool bdeleteOrder(KOOrderPtr pOrder) = 0;

    virtual void registerPositionServerSocket(int socketID){};
    virtual void unregisterPositionServerSocket(int socketID){};

    void checkOrderStatus(KOOrderPtr pOrder);

    virtual bool bisLiveTrading() = 0;

    void wakeup(KOEpochTime cCallTime);

    SchedulerConfig& _cSchedulerCfg;

    void sortTimeEvent();

    void addNewWakeupCall(KOEpochTime cCallTime, TimerCallbackInterface* pTarget);
    void addNewFigureCall(TradeEngineBase* pTargetEngine, FigureCallPtr pNewFigureCall);
    void addNewEngineCall(TradeEngineBase* pTargetEngine, EngineEvent::EngineEventType eEventType, KOEpochTime cCallTime);

protected:
    bool bconnectPositionServer();

    void newPriceUpdate(long iProductIndex);
    void orderConfirmed(KOOrderPtr pOrder);
    void orderDeleted(KOOrderPtr pOrder);
    void orderFilled(KOOrderPtr pOrder, long iQty, double dPrice);
    void orderAmendRejected(KOOrderPtr pOrder);
    void orderDeleteRejected(KOOrderPtr pOrder);
    void orderRejected(KOOrderPtr pOrder);
    void orderDeletedUnexpectedly(KOOrderPtr pOrder);
    void orderConfirmedUnexpectedly(KOOrderPtr pOrder);

    void addNewFigureCall(TradeEngineBasePtr pTradeEngine, string sFigureName, KOEpochTime cCallTime, KOEpochTime cFigureTime, FigureAction::options eFigureAction);

    void registerTradeEngines();
    bool registerPriceForEngine(const string& sFullProductName, const string& sAccount, int iLimit, TradeEngineBasePtr pTargetEngine);    

    string sgetRootFromKOSymbol(const string& sKOSymbol);

    void processTimeEvents(KOEpochTime cTimeNow);

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
    map<string, boost::shared_ptr< vector <string> > > _mProductFigureHitting;
    map<string, boost::shared_ptr< vector <string> > > _mProductFigureHalt;
    map<string, boost::shared_ptr< vector <string> > > _mProductFigurePatient;
    map<string, boost::shared_ptr< vector <string> > > _mProductFigureFlatHitting;
/* Figure Handling End */

/* Risk Handling */
    map<string, ProductRiskPtr> _mRiskSetting;
    vector<ProductRiskPtr> _vGlobalPositions;
    bool bloadRiskFile();
/* Risk Handling End */

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
    static void offLiquidator(int aSignal);
    static void signalCommand(int aSignal);
    static void limitLiquidateProduct(int aSignal);
    static void fastLiquidateProduct(int aSignal);
    static SchedulerBase* _pInstance;

    void liquidatProduct(bool bIsLimit);
/* Signal Handling End */

/* Scheduled trading action */
    void loadScheduledManualActionsFile();
    void loadScheduledSlotLiquidationFile();
    vector< pair<KOEpochTime, ManualCommandAction> > _vScheduledManualActions;
    vector< pair<KOEpochTime, vector<string> > > _vScheduledSlotLiquidation;
/* Scheduled trading action End */

/* Position Server */
    PositionServerConnection _cPositionServerConnection;
/* Position Server End */
};


};


#endif
