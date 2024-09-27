#ifndef TradeEngineBase_H
#define TradeEngineBase_H

#include <fstream>
#include <vector>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include "KOEpochTime.h"
#include "SchedulerBase.h"
#include "QuoteData.h"
#include "Figures.h"
#include "SimpleLogger.h"
#include "TimerCallbackInterface.h"

using std::istream;
using std::vector;

namespace KO
{

class ContractAccount;

class TradeEngineBase : public TimerCallbackInterface
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
                    const string& sSimType);

    ~TradeEngineBase();

    virtual void readFromStream(istream& is) = 0;
    virtual void receive(int iCID) = 0;
    virtual void wakeup(KOEpochTime cCallTime){ (void)cCallTime;};
    virtual void figureCall(KOEpochTime cCallTime, KOEpochTime cEventTime, const string& sFigureName, FigureAction::options eFigureAction, double dForcast, double dActual, bool bReleased){(void)cCallTime; (void)cEventTime; (void)sFigureName; (void)eFigureAction;(void)dForcast, (void)dActual; (void)bReleased;};
    void figureBaseCall(KOEpochTime cCallTime, KOEpochTime cEventTime, const string& sFigureName, FigureAction::options eFigureAction);
    virtual void externalSignalHandler() {};
    virtual void positionRequestCallback(string sProduct, string sAccount, long iPosition, bool bTimeout){(void)sProduct; (void)sAccount; (void)iPosition; (void)bTimeout;}; 

    void checkEngineState(KOEpochTime cCallTime);

    vector<QuoteData*> vContractQuoteDatas;

    virtual void dayInit();
    virtual void dayRun();
    virtual void dayTrade();
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

    void setOutputBasePath(string sOutputBasePath);

    const string& sgetEngineSlotName();

    bool bcheckDataStaleness(int iCID);

    virtual void printTheoTarget() {};
    virtual void printSignal() {};

    KOEpochTime cgetTradingStartTime();
    KOEpochTime cgetTradingEndTime();

    vector<string> vgetRegisterdFigureProducts();

    vector< pair<string, KOEpochTime> > vgetRegisterdFigures();

protected:
    void registerProductForFigure(string sProductName);

    void updateSlotSignal(const string& sProduct, int iSlotID, long iDesiredPos, int iSignalState, bool bMarketOrder);

    void activateSlot(const string& sProduct, int iSlotID);
    void deactivateSlot(const string& sProduct, int iSlotID);
    void setSlotReady(const string& sProduct, int iSlotID);


    vector<string> _vRegisteredFigureProducts;

    vector< boost::shared_ptr<ContractAccount> > vContractAccount;

    SchedulerBase* _pScheduler;

    string _sTodayDate;

    EngineState _eEngineState;   

    string _sOutputBasePath;
    string _sEngineRunTimePath;
    string _sEngineType;
    string _sEngineSlotName;

    KOEpochTime _cTradingStartTime;
    KOEpochTime _cTradingEndTime;

    bool _bManualTradingOveride;

    string _sSimType;
    SimpleLogger _cLogger;

    bool _bSubmitRealOrder;
    int _iSlotID;

private:
    void resumeTrading();
    void haltTrading();
    void patientLiquidation();
    void limitLiquidation();
    void fastLiquidation();
};

}

#endif /* TradeEngineBase_H */
