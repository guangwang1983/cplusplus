#ifndef SLBase_H
#define SLBase_H

#include "../EngineInfra/TradeEngineBase.h"
#include "../base/Instrument.h"
#include "../base/SyntheticSpread.h"
#include "../EngineInfra/Figures.h"
#include "../EngineInfra/KOEpochTime.h"

namespace KO
{

class SLBase : public TradeEngineBase
{

struct OnGoingFigure
{
    string sFigureName;
    FigureAction::options eFigureAction;
};

struct TradeSignal
{
    long iPortfolioID;
    long iEpochTimeStamp;
    long iDesiredPos;
    long iSignalState;
    bool bMarketOrder;
};

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

    SLBase(const string& sEngineRunTimePath,
           const string& sEngineType,
           const string& sEngineSlotName,
           KOEpochTime cTradingStartTime,
           KOEpochTime cTradingEndTime,
           SchedulerBase* pScheduler,
           const string& sTodayDate,
           const string& sSimType);

	virtual ~SLBase();
	
    virtual void dayInit();
    virtual void dayTrade();
    virtual void dayRun();
    virtual void dayStop();
	
	virtual void readFromStream(istream& is) =  0;
	virtual void receive(int iCID);
	virtual void wakeup(KOEpochTime cCallTime);
	virtual void figureCall(KOEpochTime cCallTime, KOEpochTime cEventTime, const string& sFigureName, FigureAction::options eFigureAction, double dForecast, double dActual, bool bReleased);

    virtual void writeSpreadLog() = 0;

protected:
    void setupLogger(const string& sStrategyName);

    virtual void updateStatistics(KOEpochTime cCallTime) = 0;
    virtual bool bcheckAllProductsReady() = 0;
	virtual void saveOvernightStats(bool bRemoveToday) = 0;
    virtual void loadOvernightStats() = 0;
    virtual void loadRollDelta();

    void updateEngineStateOnTimer(KOEpochTime cCallTime);

    void updateSignal();

	void registerFigure();

    void loadTriggerSpace();

    long iEpochTimeStamp;
    long iDesiredPos;
    bool bExecutionStopped;
    bool bMarketOrder;

    void saveSignal(long iTriggerIdx, long iInputEpochTimeStamp, long iInputDesiredPos, int iInputSignalState, bool bInputMarketOrder);

    virtual void printTheoTarget() override;
    virtual void printSignal() override;

    KOEpochTime _cPreviousCallTime;
	KOEpochTime _cQuotingStartTime;
	KOEpochTime _cQuotingEndTime;
    KOEpochTime _cQuotingPatLiqTime;
    KOEpochTime _cQuotingLimLiqTime;
    KOEpochTime _cQuotingFastLiqTime;

	Instrument*           _pQuoteInstrument;
    SyntheticSpread*      _pProductInstrument;

    bool _bUseRealSignalPrice;

    bool _bQuoteInstrumentStatled;

	map<pair<string, string>, double > _mDailyRolls;

    bool _bProductPositiveCorrelation;

	int _iQuoteQty;

	double _dExecHittingThreshold;

	bool   _bWriteLog;
    bool   _bWriteSpreadLog;

    bool   _bIsSegVol;
	long   _iVolLength;
    long   _iProductVolLength;
	long   _dDriftLength;

    vector<double> _vEntryStd;
    vector<double> _vExitStd;
    vector<string> _vTriggerID;

    vector<long> _vTheoPositions;
    vector<SignalState> _vSignalStates;
    vector<bool> _vIsMarketOrders;
    vector<vector<TradeSignal>> _vTriggerTradeSignals;

    long _iStdevLength;
    long _iProductStdevLength;

    KOEpochTime _cVolStartTime;
    KOEpochTime _cVolEndTime;

	double _dSpreadIndicator;

	long _iTotalQuoteInstruUpdate;

	vector< boost::shared_ptr<OnGoingFigure> > _vOnGoingEvents;

	long _iQuoteTimeOffsetSeconds;

    bool _bTradingStatsValid;

    bool _bIsQuoteTime;
    bool _bIsPatLiqTimeReached;
    bool _bIsLimLiqTimeReached;
    bool _bIsFastLiqTimeReached;
    long _iPatLiqOffSet;

    bool _bValidStatsSeen;
    bool _bInvalidStatTriggered;

    bool _bUseASC;
    vector<long> _vASCSettings;
    bool _bASCBlocked;
    double _iPrevSignal;
    bool _bASCStoppedOut;

    FigureAction::options _eCurrentFigureAction;

    SimpleLogger _cSpreadLogger;

    vector<string> _vSignalStateText;

    double _dLastQuoteMid;

    bool _bIOC;

    bool _bIsSV;

    KOEpochTime _cLastTimeAllProductsReady;
    KOEpochTime _cLastQuoteUpdateTime;

    bool _bIsBullishMom;
    bool _bIsBearishMom;

    bool _bIsTrading;
};

}

#endif /* SLBase_H */
