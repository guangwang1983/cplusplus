#ifndef SLBase_H
#define SLBase_H

#include "../EngineInfra/TradeEngineBase.h"
#include "../base/Instrument.h"
#include "../base/QuoteOrder.h"
#include "../base/IOCOrder.h"
#include "../base/HedgeOrder.h"
#include "../base/SyntheticSpread.h"
#include "../base/FairValueExecution.h"
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

public:
    enum OrderType
    {
        OPEN_BUY,
        OPEN_SELL,
        CLOSE_BUY,
        CLOSE_SELL
    };

    SLBase(const string& sEngineRunTimePath,
           const string& sEngineType,
           const string& sEngineSlotName,
           KOEpochTime cTradingStartTime,
           KOEpochTime cTradingEndTime,
           SchedulerBase* pScheduler,
           const string& sTodayDate,
           PositionServerConnection* pPositionServerConnection);

	virtual ~SLBase();
	
    virtual void dayInit();
    virtual void dayRun();
    virtual void dayStop();
	
	virtual void readFromStream(istream& is) =  0;
	virtual void receive(int iCID);
	virtual void wakeup(KOEpochTime cCallTime);
	void figureCall(KOEpochTime cCallTime, KOEpochTime cEventTime, const string& sFigureName, FigureAction::options eFigureAction);

    virtual void orderConfirmHandler(int iOrderID);
    virtual void orderFillHandler(int iOrderID, long iFilledQty, double dPrice);
    virtual void orderRejectHandler(int iOrderID);
    virtual void orderDeleteHandler(int iOrderID);
    virtual void orderDeleteRejectHandler(int iOrderID);
    virtual void orderAmendRejectHandler(int iOrderID);
    virtual void orderUnexpectedConfirmHandler(int iOrderID);
    virtual void orderUnexpectedDeleteHandler(int iOrderID);

    virtual void writeSpreadLog() = 0;

protected:
    void setupLiqTime();
    void setupLogger(const string& sStrategyName);

    virtual void updateStatistics(KOEpochTime cCallTime) = 0;
    virtual bool bcheckAllProductsReady() = 0;
	virtual void saveOvernightStats() = 0;
	virtual void loadOvernightStats() = 0;
    virtual void loadRollDelta();

    void updateEngineStateOnTimer(KOEpochTime cCallTime);
    void updateHittingSignal();
    void limitLiqPosition();
    void fastLiqPosition();

    void updateTheoPosition();
	void calculateQuotePrice();
	void registerFigure();

    FairValueExecution* _pFairValueExecution;

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

    long _iTheoBid;
    long _iTheoOffer;
    long _iTheoExitBid;
    long _iTheoExitOffer;

	int _iQuoteQty;

	double _dExecHittingThreshold;

	bool   _bWriteLog;
    bool   _bWriteSpreadLog;

    bool   _bIsSegVol;
	long   _iVolLength;
    long   _iProductVolLength;
	long   _dDriftLength;
	double _dEntryStd;
	double _dExitStd;

    bool _bIsHittingStrategy;

    long _iStdevLength;
    long _iProductStdevLength;

    KOEpochTime _cVolStartTime;
    KOEpochTime _cVolEndTime;

    long _iTheoPosition;
	long _iPosition;
    double _dRealisedPnL;
    double _dPnLAtLiq;

    bool _bPositionInitialised;

	double _dSpreadIndicator;

	long _iTotalQuoteInstruUpdate;

	vector< boost::shared_ptr<OnGoingFigure> > _vOnGoingEvents;

	long _iQuoteTimeOffsetSeconds;

    bool _bEstablishTheoPosition;

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

    boost::shared_ptr<HedgeOrder> _pLiquidationOrder;
    boost::shared_ptr<IOCOrder> _pIOCLiquidationOrder;

    SimpleLogger _cSpreadLogger;
};

}

#endif /* SLBase_H */
