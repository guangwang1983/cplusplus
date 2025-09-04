#include "TriangulationDummy.h"
#include "../EngineInfra/ErrorHandler.h"
#include "../EngineInfra/SystemClock.h"

namespace KO
{

TriangulationDummy::TriangulationDummy(const string& sEngineRunTimePath,
                                       const string& sEngineSlotName,
                                       KOEpochTime cTradingStartTime,
                                       KOEpochTime cTradingEndTime,
                                       SchedulerBase* pScheduler,
                                       const string& sTodayDate,
                                       const string& sSimType,
                                       KOEpochTime cSlotFirstWakeupCallTime)
:TradeEngineBase(sEngineRunTimePath, "TriangulationDummy", sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, sSimType)
{
    _cSlotFirstWakeupCallTime = cSlotFirstWakeupCallTime;
}

TriangulationDummy::~TriangulationDummy()
{

}

void TriangulationDummy::dayInit()
{
    TradeEngineBase::dayInit();

    KOEpochTime cEngineActualStartTime = _cTradingStartTime;

    if(_cSlotFirstWakeupCallTime > _cTradingStartTime)
    {
        cEngineActualStartTime = _cSlotFirstWakeupCallTime;
        stringstream cStringStream;
        cStringStream << "Engine late restart. First wake up call time is " << _cSlotFirstWakeupCallTime.igetPrintable();
        ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, "ALL", cStringStream.str());
    }
    KOEpochTime cEngineLiveDuration = _cTradingEndTime - cEngineActualStartTime;

    for(int i = 0; i < cEngineLiveDuration.sec();i++)
    {
        _pScheduler->addNewWakeupCall(cEngineActualStartTime + KOEpochTime(i,0), this);
    }
}

void TriangulationDummy::dayTrade()
{
    TradeEngineBase::dayTrade();

    stringstream cStringStream;
    cStringStream << "Activate slot in internaliser";
    ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, "ALL", cStringStream.str());

    activateSlot(vContractQuoteDatas[0]->sProduct, _iSlotID);
}

void TriangulationDummy::dayRun()
{
    TradeEngineBase::dayRun();
}

void TriangulationDummy::dayStop()
{
    TradeEngineBase::dayStop();
    deactivateSlot(vContractQuoteDatas[0]->sProduct, _iSlotID);
}

void TriangulationDummy::readFromStream(istream& is) 
{
    _pScheduler->addNewEngineCall(this, EngineEvent::TRADE, _cTradingStartTime + KOEpochTime(1,0));
}

void TriangulationDummy::receive(int iCID) 
{

}

void TriangulationDummy::wakeup(KOEpochTime cCallTime) 
{
    updateSlotSignal(vContractQuoteDatas[0]->sProduct, _iSlotID, 0, 3, false); 
    setSlotReady(vContractQuoteDatas[0]->sProduct, _iSlotID);
}

void TriangulationDummy::figureCall(KOEpochTime cCallTime, KOEpochTime cEventTime, const string& sFigureName, FigureAction::options eFigureAction, double dForecast, double dActual, bool bReleased) 
{

}

void TriangulationDummy::writeSpreadLog() 
{

}

}
