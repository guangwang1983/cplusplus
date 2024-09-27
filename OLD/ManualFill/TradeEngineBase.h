#ifndef TradeEngineBase_H
#define TradeEngineBase_H

#include <fstream>
#include <vector>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include "KOEpochTime.h"
#include "SchedulerBase.h"
#include "QuoteData.h"
#include "OrderParentInterface.h"
#include "Figures.h"
#include "SimpleLogger.h"
#include "TimerCallbackInterface.h"
#include "PositionServerConnection.h"

using std::istream;
using std::vector;

namespace KO
{

class ContractAccount;

class TradeEngineBase : public OrderParentInterface,
                        public TimerCallbackInterface
{

friend class SchedulerBase;

public:

    enum EngineState
    {
        INIT,
        RUN,
        STOP,
        HALT,
        LIM_LIQ,
        FAST_LIQ,
        PAT_LIQ
    };

    TradeEngineBase(const string& sEngineRunTimePath,
                    const string& sEngineType,
                    const string& sEngineSlotName,
                    KOEpochTime cTradingStartTime,
                    KOEpochTime cTradingEndTime,
                    SchedulerBase* pScheduler,
                    string sTodayDate,
                    PositionServerConnection* pPositionServerConnection);

    ~TradeEngineBase();

    virtual void readFromStream(istream& is) = 0;
    virtual void receive(int iCID) = 0;
    virtual void wakeup(KOEpochTime cCallTime){};
    virtual void figureCall(KOEpochTime cCallTime, KOEpochTime cEventTime, const string& sFigureName, FigureAction::options eFigureAction){};
    virtual void externalSignalHandler() {};
    virtual void positionRequestCallback(string sProduct, string sAccount, long iPosition, bool bTimeout){}; 

    void assignStartupPosition(string sProduct, string sAccount, long iPosition, bool bTimeout);
 
    void checkEngineState(KOEpochTime cCallTime);

    vector<QuoteData*> vContractQuoteDatas;

    virtual void dayInit();
    virtual void dayRun();
    virtual void dayStop();
   
    void manualResumeTrading();
    void manualHaltTrading();
    void manualPatientLiquidation();
    void manualLimitLiquidation();
    void manualFastLiquidation();
 
    void autoResumeTrading();
    void autoHaltTrading();
    void autoPatientLiquidation();
    void autoLimitLiquidation();
    void autoFastLiquidation();

    bool bisTrading();
    bool bisLiveTrading();

    const string& sgetEngineSlotName();

    virtual void orderConfirmHandler(int iOrderID){};
    virtual void orderFillHandler(int iOrderID, long iFilledQty, double dPrice){};
    virtual void manualFillHandler(string sProduct, long iFilledQty, double dPrice){};
    virtual void orderRejectHandler(int iOrderID){};
    virtual void orderDeleteHandler(int iOrderID){};
    virtual void orderDeleteRejectHandler(int iOrderID){};
    virtual void orderAmendRejectHandler(int iOrderID){};
    virtual void orderUnexpectedConfirmHandler(int iOrderID){};
    virtual void orderUnexpectedDeleteHandler(int iOrderID){};

    void orderCriticalErrorHandler(int iOrderID);

    bool bcheckDataStaleness(int iCID);

    KOEpochTime cgetTradingStartTime();
    KOEpochTime cgetTradingEndTime();

    vector<string> vgetRegisterdFigureProducts();

    vector< pair<string, KOEpochTime> > vgetRegisterdFigures();

protected:
    void registerProductForFigure(string sProductName);

    vector<string> _vRegisteredFigureProducts;

    vector< boost::shared_ptr<ContractAccount> > vContractAccount;

    SchedulerBase* _pScheduler;

    string _sTodayDate;

    EngineState _eEngineState;   
 
    string _sEngineRunTimePath;
    string _sEngineType;
    string _sEngineSlotName;

    KOEpochTime _cTradingStartTime;
    KOEpochTime _cTradingEndTime;

    bool _bManualTradingOveride;

    SimpleLogger _cLogger;

    PositionServerConnection* _pPositionServerConnection;
 
private:
    void resumeTrading();
    void haltTrading();
    void patientLiquidation();
    void limitLiquidation();
    void fastLiquidation();

    int _iNumOrderCriticalErrorSeen;

    bool _bLiveOrderAtTradingEndTime;
};

}

#endif /* TradeEngineBase_H */
