#include "TradeEngineBase.h"
#include "ContractAccount.h"
#include "ErrorHandler.h"
#include "SystemClock.h"

using std::stringstream;
using std::cerr;

namespace KO
{

TradeEngineBase::TradeEngineBase(const string& sEngineRunTimePath,
                                 const string& sEngineType,
                                 const string& sEngineSlotName,
                                 KOEpochTime cTradingStartTime,
                                 KOEpochTime cTradingEndTime,
                                 SchedulerBase* pScheduler,
                                 string sTodayDate,
                                 const string& sSimType)
:_pScheduler(pScheduler),
 _sTodayDate(sTodayDate),
 _sEngineRunTimePath(sEngineRunTimePath),
 _sEngineType(sEngineType),
 _sEngineSlotName(sEngineSlotName),
 _cTradingStartTime(cTradingStartTime),
 _cTradingEndTime(cTradingEndTime),
 _bManualTradingOveride(false),
 _sSimType(sSimType)
{
}

TradeEngineBase::~TradeEngineBase()
{

}

void TradeEngineBase::receive(int iCID)
{
    (void) iCID;
}

void TradeEngineBase::dayInit()
{
    _eEngineState = INIT;
}

void TradeEngineBase::dayTrade()
{
    // leave empty because at engine base level, trade time doesnt matter
}

void TradeEngineBase::dayRun()
{
    if(_bManualTradingOveride == false)
    {
        _eEngineState = RUN;
    }

    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|DayRun|Acutal scheduled trading start time is " << _cTradingStartTime.igetPrintable() << "\n";
}

void TradeEngineBase::dayStop()
{
    _eEngineState = STOP;
}

bool TradeEngineBase::bisTrading()
{
    return (_eEngineState != INIT && _eEngineState != STOP);
}

bool TradeEngineBase::bisLiveTrading()
{
    return _pScheduler->bisLiveTrading();
}

const string& TradeEngineBase::sgetEngineSlotName()
{
    return _sEngineSlotName;
}

KOEpochTime TradeEngineBase::cgetTradingStartTime()
{
    return _cTradingStartTime;
}

KOEpochTime TradeEngineBase::cgetTradingEndTime()
{
    return _cTradingEndTime;
}

void TradeEngineBase::registerProductForFigure(string sProductName)
{
    _vRegisteredFigureProducts.push_back(sProductName);
}

vector<string> TradeEngineBase::vgetRegisterdFigureProducts()
{
    return _vRegisteredFigureProducts;
}

void TradeEngineBase::checkEngineState(KOEpochTime cCallTime)
{
    (void) cCallTime;
}

bool TradeEngineBase::bcheckDataStaleness(int iCID)
{
    (void) iCID;

    bool bResult = false;

    return bResult;
}

void TradeEngineBase::manualResumeTrading()
{
    _cLogger << "manual resume trading signal received \n";
    _bManualTradingOveride = false;
    resumeTrading();
}

void TradeEngineBase::manualHaltTrading()
{
    _cLogger << "manual halt trading signal received \n";
    _bManualTradingOveride = true;
    haltTrading();
}

void TradeEngineBase::manualPatientLiquidation()
{
    _cLogger << "manual patient signal received \n";
    _bManualTradingOveride = true;
    patientLiquidation();
}

void TradeEngineBase::manualLimitLiquidation()
{
    _cLogger << "manual limit liquidation signal received \n";
    _bManualTradingOveride = true;
    limitLiquidation();
}

void TradeEngineBase::manualFastLiquidation()
{
    _cLogger << "manual fast liquidation signal received \n";
    _bManualTradingOveride = true;
    fastLiquidation();
}

void TradeEngineBase::autoResumeTrading()
{
    if(_bManualTradingOveride == false)
    {
        resumeTrading();
    }
}

void TradeEngineBase::autoHaltTrading()
{
    if(_bManualTradingOveride == false)
    {
        haltTrading();
    }
}

void TradeEngineBase::autoPatientLiquidation()
{
    if(_bManualTradingOveride == false)
    {
        patientLiquidation();
    }
}

void TradeEngineBase::autoLimitLiquidation()
{
    if(_bManualTradingOveride == false)
    {
        limitLiquidation();
    }
}

void TradeEngineBase::autoFastLiquidation()
{
    if(_bManualTradingOveride == false)
    {
        fastLiquidation();
    }
}

void TradeEngineBase::resumeTrading()
{
    _eEngineState = RUN;

    for(unsigned int i = 0; i != vContractAccount.size(); i++)
    {
        vContractAccount[i]->resumeTrading();
    }
}

void TradeEngineBase::patientLiquidation()
{
    _eEngineState = PAT_LIQ;

    for(unsigned int i = 0; i != vContractAccount.size(); i++)
    {
        vContractAccount[i]->resumeTrading();
    }
}

void TradeEngineBase::haltTrading()
{
    _eEngineState = HALT;

    for(unsigned int i = 0; i != vContractAccount.size(); i++)
    {
        vContractAccount[i]->haltTrading();
    }
}

void TradeEngineBase::limitLiquidation()
{
    _eEngineState = LIM_LIQ;

    for(unsigned int i = 0; i != vContractAccount.size(); i++)
    {
        vContractAccount[i]->resumeTrading();
    }
}

void TradeEngineBase::fastLiquidation()
{
    _eEngineState = FAST_LIQ;

    for(unsigned int i = 0; i != vContractAccount.size(); i++)
    {
        vContractAccount[i]->resumeTrading();
    }
}

void TradeEngineBase::setOutputBasePath(string sOutputBasePath)
{
    _sOutputBasePath = sOutputBasePath;
    if(_sOutputBasePath[_sOutputBasePath.size()-1] == '/')
    {
        _sOutputBasePath.pop_back();
    }

}

void TradeEngineBase::activateSlot(const string& sProduct, int iSlotID)
{
    _pScheduler->activateSlot(sProduct, iSlotID);
}

void TradeEngineBase::deactivateSlot(const string& sProduct, int iSlotID)
{
    _pScheduler->deactivateSlot(sProduct, iSlotID);
}

void TradeEngineBase::setSlotReady(const string& sProduct, int iSlotID)
{
    _pScheduler->setSlotReady(sProduct, iSlotID);
}

void TradeEngineBase::updateSlotSignal(const string& sProduct, int iSlotID, long iDesiredPos, int iSignalState, bool bMarketOrder)
{
    _pScheduler->updateSlotSignal(sProduct, iSlotID, iDesiredPos, iSignalState, bMarketOrder);
}

void TradeEngineBase::figureBaseCall(KOEpochTime cCallTime, KOEpochTime cEventTime, const string& sFigureName, FigureAction::options eFigureAction)
{
    // load figure results
    this->figureCall(cCallTime, cEventTime, sFigureName, eFigureAction, 0, 0, false);
}

}
