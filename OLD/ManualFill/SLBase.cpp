#include "SLBase.h"
#include <stdlib.h>
#include <ctime>
#include "../EngineInfra/OrderRiskChecker.h"
#include "../EngineInfra/ErrorHandler.h"
#include <boost/math/special_functions/round.hpp>
#include "../EngineInfra/SystemClock.h"

using namespace boost::posix_time;
using std::stringstream;
using std::cerr;

namespace KO
{

SLBase::SLBase(const string& sEngineRunTimePath,
               const string& sEngineType,
               const string& sEngineSlotName,
               KOEpochTime cTradingStartTime,
               KOEpochTime cTradingEndTime,
               SchedulerBase* pScheduler,
               const string& sTodayDate,
               PositionServerConnection* pPositionServerConnection)
:TradeEngineBase(sEngineRunTimePath, sEngineType, sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, pPositionServerConnection),
 _bQuoteInstrumentStatled(false),
 _bProductPositiveCorrelation(true),
 _bPositionInitialised(false),
 _bEstablishTheoPosition(false),
 _bIsQuoteTime(false),
 _bIsPatLiqTimeReached(false),
 _bIsLimLiqTimeReached(false),
 _bIsFastLiqTimeReached(false),
 _bValidStatsSeen(false),
 _bInvalidStatTriggered(false)
{
    _bWriteLog = false;
    _bWriteSpreadLog = false;

    _iQuoteTimeOffsetSeconds = 0;

    _iTheoBid = 0;
    _iTheoOffer = 0;
    _iTheoExitBid = 0;
    _iTheoExitOffer = 0;

    _eCurrentFigureAction = FigureAction::NO_FIGURE;

    _bTradingStatsValid = false;

    _bIsHittingStrategy = false;

    _bUseRealSignalPrice = false;

    _bIsSegVol = false;

    _bUseASC = false;
    _bASCBlocked = false;
    _bASCStoppedOut = false;

    _pQuoteInstrument = NULL;
    _pProductInstrument = NULL;
    _pFairValueExecution = NULL;

    _iPatLiqOffSet = 1800;

    _bEstablishTheoPosition = true;
}

SLBase::~SLBase()
{
    if(_pQuoteInstrument != NULL)
    {
        delete _pQuoteInstrument;
        _pQuoteInstrument = NULL;
    }

    if(_pProductInstrument != NULL)
    {
        delete _pProductInstrument;
        _pProductInstrument = NULL;
    }

    if(_pFairValueExecution != NULL)
    {
        delete _pFairValueExecution;
        _pFairValueExecution = NULL;
    }
}

void SLBase::setupLiqTime()
{
    long iLimLiqOffSet = 300;

    KOEpochTime cAbsLimLiqTime = _cTradingEndTime - KOEpochTime(1860,0);

    _cQuotingPatLiqTime = _cQuotingEndTime - KOEpochTime(_iPatLiqOffSet,0);
    _cQuotingLimLiqTime = _cQuotingEndTime - KOEpochTime(iLimLiqOffSet,0);
    _cQuotingFastLiqTime = _cQuotingEndTime - KOEpochTime(60,0);

    if(cAbsLimLiqTime < _cQuotingLimLiqTime)
    {
        
        _cQuotingLimLiqTime = cAbsLimLiqTime;
        _cQuotingFastLiqTime = _cQuotingLimLiqTime + KOEpochTime(300,0);
        _cQuotingPatLiqTime = _cQuotingLimLiqTime - KOEpochTime(600,0);
    }
}

void SLBase::setupLogger(const string& sStrategyName)
{
    string sLogFileName = _sEngineRunTimePath + sStrategyName + "-" + _sTodayDate;

    if(_pScheduler->bisLiveTrading())
    {
        _cLogger.openFile(sLogFileName, _bWriteLog, true);
    }
    else
    {
        _cLogger.openFile(sLogFileName, _bWriteLog, false);
    }

    string sSpreadLogFileName = _sEngineRunTimePath + sStrategyName + "-Spread-" + _sTodayDate;

    if(_pScheduler->bisLiveTrading())
    {
        _cSpreadLogger.openFile(sSpreadLogFileName, _bWriteSpreadLog, true);
    }
    else
    {
        _cSpreadLogger.openFile(sSpreadLogFileName, _bWriteSpreadLog, false);
    }
}

void SLBase::dayInit()
{
    TradeEngineBase::dayInit();

    _dSpreadIndicator = 0;
    _iPosition = 0;
    _iTheoPosition = 0;
    _iTotalQuoteInstruUpdate = 0;

    _vOnGoingEvents.clear();

    if(_bIsSegVol)
    {
        _cVolStartTime = _cQuotingStartTime;
        _cVolEndTime = _cQuotingEndTime;
    }
    else
    {
        _cVolStartTime = _cTradingStartTime;
        _cVolEndTime = _cTradingEndTime;
    }

    if(_cQuotingStartTime > SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(_sTodayDate, "10", "00", "00"))
    {
        if(_dDriftLength >= 10000)
        {
            _cLogger << "_bEstablishTheoPosition sets to true \n";
            _bEstablishTheoPosition = true;
        }
        else
        {
            _cLogger << "_bEstablishTheoPosition sets to false \n";
            _bEstablishTheoPosition = false;
        }
    }

    _iStdevLength = (_cVolEndTime.sec() - _cVolStartTime.sec()) / 60 * _iVolLength;
    _iProductStdevLength = (_cVolEndTime.sec() - _cVolStartTime.sec()) / 60 * _iProductVolLength;

    if(_pQuoteInstrument == NULL)
    {
        _pQuoteInstrument = new Instrument(vContractQuoteDatas[0]->sProduct, vContractQuoteDatas[0]->iCID, vContractQuoteDatas[0]->eInstrumentType, vContractQuoteDatas[0]->dTickSize, vContractQuoteDatas[0]->iMaxSpreadWidth, _bUseRealSignalPrice);
        _pQuoteInstrument->useTradingOut(0.2);
        _pQuoteInstrument->useEXMA(_dDriftLength, false, KOEpochTime(0,0), 1, _cVolStartTime, _cVolEndTime);
        _pQuoteInstrument->useWeightedStdev(_iStdevLength, false, KOEpochTime(0,0), 60, _cVolStartTime, _cVolEndTime);
    }

    _pFairValueExecution = new FairValueExecution(vContractAccount[0], _sEngineSlotName, _pQuoteInstrument, _iQuoteQty, _iPosition, this, &_cLogger); 


    KOEpochTime cEngineLiveDuration = _cTradingEndTime - _cTradingStartTime;

    for(int i = 0; i < cEngineLiveDuration.sec();i++)
    {
        _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(i,0), this);
    }

//    _pScheduler->addNewWakeupCall(_cTradingStartTime, this);
}

void SLBase::dayRun()
{
    TradeEngineBase::dayRun();

    _bPositionInitialised = vContractAccount[0]->bgetPositionInitialised();

    if(_bPositionInitialised == true)
    {
        _iPosition = vContractAccount[0]->igetCurrentPosition();
        _pFairValueExecution->setPosition(_iPosition);
        _cLogger << "Initialising strategy position to " << _iPosition << "\n";
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Position not initialised. Waiting for reply from position server before resume trading";
        ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());

        _cLogger << cStringStream.str() << "\n";
    }
}

void SLBase::dayStop()
{
    TradeEngineBase::dayStop();
}

void SLBase::receive(int iCID)
{
    TradeEngineBase::receive(iCID);

//    trc::compat::util::TimeVal start = trc::compat::util::TimeVal::now;
//    long iEpochStart = start.sec() *1000000 + start.usec();

    int iUpdateIndex = -1;

    if(vContractQuoteDatas[0]->iCID == iCID)
    {
        iUpdateIndex = 0;
    }
    else if(vContractQuoteDatas[1]->iCID == iCID)
    {
        iUpdateIndex = 1;
    }

	if(iUpdateIndex == 0)
	{
/*
_cLogger << "QUOTE|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << vContractQuoteDatas[iUpdateIndex]->iBidSize
            																					<< "|" << vContractQuoteDatas[iUpdateIndex]->dBestBid
																						        << "|" << vContractQuoteDatas[iUpdateIndex]->dBestAsk
																						        << "|" << vContractQuoteDatas[iUpdateIndex]->iAskSize 
                                                                                                << "|" << vContractQuoteDatas[iUpdateIndex]->dLastTradePrice 
                                                                                                << "|" << vContractQuoteDatas[iUpdateIndex]->iAccumuTradeSize 
                                                                                                << "|" << vContractQuoteDatas[iUpdateIndex]->iHighBidInTicks
                                                                                                << "|" << vContractQuoteDatas[iUpdateIndex]->iLowAskInTicks << "\n";
*/
		// Quote instrument updated
		_pQuoteInstrument->newMarketUpdate(vContractQuoteDatas[iUpdateIndex]);
		_iTotalQuoteInstruUpdate = _iTotalQuoteInstruUpdate + 1;
/*
_cLogger << "QUOTE|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << _pQuoteInstrument->igetBidSize() 
                                  												                << "|" << _pQuoteInstrument->igetBestBid() 
                    																	        << "|" << _pQuoteInstrument->igetBestAsk() 
                    																	        << "|" << _pQuoteInstrument->igetAskSize() 
                                                                                                << "|" << _pQuoteInstrument->igetLastTrade() 
                                                                                                << "|" << _pQuoteInstrument->igetAccumuTradeSize() << "\n";
*/
		if(bcheckAllProductsReady())
		{
            updateTheoPosition();
            calculateQuotePrice();
            _pFairValueExecution->newTheoQuotePrices(_iTheoBid, _iTheoOffer, _iTheoExitBid, _iTheoExitOffer, false);
            _pFairValueExecution->updateOrders();

            if(_pLiquidationOrder.get())
            {
                _pLiquidationOrder->checkOrderStop();
            }
		}
		else
		{
			_cLogger << "Ignore quote price update. Instrument prices invalid \n";
		}
	}

//    trc::compat::util::TimeVal end = trc::compat::util::TimeVal::now;
//    long iEpochEnd = end.sec() *1000000 + end.usec();

//    _cLogger << "receive function takes " << iEpochEnd - iEpochStart << "\n";
}

void SLBase::updateEngineStateOnTimer(KOEpochTime cCallTime)
{
    // update engine state base on time

    if(_cQuotingStartTime <= cCallTime && cCallTime < _cQuotingPatLiqTime)
    {
        if(!_bIsQuoteTime)
        {
            _cLogger << "Quoting start time reached \n";
            stringstream cStringStream;
            cStringStream << "Quoting start time reached";
            ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
        }

        _bIsQuoteTime = true;
    }
    else if(_cQuotingPatLiqTime <= cCallTime && cCallTime < _cQuotingLimLiqTime)
    {
        if(!_bIsPatLiqTimeReached)
        {
            _cLogger << "Patient liquidation time reached \n";
            stringstream cStringStream;
            cStringStream << "Patient liquidation time reached";
            ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
        }

        _bIsPatLiqTimeReached = true;
        autoPatientLiquidation();
    }
    else if(_cQuotingLimLiqTime <= cCallTime && cCallTime < _cQuotingFastLiqTime)
    {
        if(!_bIsLimLiqTimeReached)
        {
            _cLogger << "Limit liquidation time reached \n";
            stringstream cStringStream;
            cStringStream << "Limit liquidation time reached";
            ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
        }

        _bIsLimLiqTimeReached = true;
        autoLimitLiquidation();
    }
    else if(_cQuotingFastLiqTime <= cCallTime && cCallTime < _cQuotingEndTime)
    {
        if(!_bIsFastLiqTimeReached)
        {
            _cLogger << "Fast liquidation time reached \n";
            stringstream cStringStream;
            cStringStream << "Fast liquidation time reached";
            ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
        }

        _bIsFastLiqTimeReached = true;
        autoFastLiquidation();
    }
    else if(_cQuotingEndTime <= cCallTime)
    {
        if(_bIsQuoteTime == true)
        {
            _cLogger << "Quoting end time reached \n";
        }

        _bIsQuoteTime = false;
        _pFairValueExecution->setExecutionState(FairValueExecution::Off);
        autoHaltTrading();
    }
}

void SLBase::wakeup(KOEpochTime cCallTime)
{
    updateEngineStateOnTimer(cCallTime);

    if(_bValidStatsSeen == false)
    {
        if(cCallTime > _cTradingStartTime + KOEpochTime(300,0))
        {
            if(_bInvalidStatTriggered == false)
            {
                _bInvalidStatTriggered = true;

                stringstream cStringStream;
                cStringStream << "Engine stats invalid after 5 minutes of trading!";
                ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());            
            }
        }
        else
        {
            _bValidStatsSeen = _bTradingStatsValid;
        }
    }

    if(!bisLiveTrading())
    {
        writeSpreadLog();
    }

	if(bcheckAllProductsReady())
	{
        updateStatistics(cCallTime);

        if(_bTradingStatsValid == true)
        {
            if(_eEngineState == TradeEngineBase::HALT)
            {
                if(cCallTime.sec() % 60 == 0 && _pScheduler->bisLiveTrading())
                {
                    saveOvernightStats();
                }
            }

            if(_bPositionInitialised == false)
            {
                _bPositionInitialised = vContractAccount[0]->bgetPositionInitialised();

                if(_bPositionInitialised == true)
                {
                    _iPosition = vContractAccount[0]->igetCurrentPosition();
                    _pFairValueExecution->setPosition(_iPosition);
                    _cLogger << "Received position " << _iPosition << " from position server. Trading Resumed \n";
                }
            }
            else
            {
                if(_bIsQuoteTime == true)
                {
                    if(_eEngineState == TradeEngineBase::RUN)
                    {
                        if(_eCurrentFigureAction == FigureAction::NO_FIGURE ||
                           _eCurrentFigureAction == FigureAction::HITTING)
                        {
                            if(_bIsHittingStrategy)
                            {
                                if(_bEstablishTheoPosition)
                                {
                                    if(_iTheoPosition != _iPosition)
                                    {
                                        _cLogger << "Trying to establish theo position \n";
                                        _pFairValueExecution->setEstablishSegEntryPos(true);
                                    }
                                    else
                                    {
                                        _cLogger << "Theo position established \n";
                                        _pFairValueExecution->setEstablishSegEntryPos(false);
                                        _bEstablishTheoPosition = false;
                                    }
                                }

                                _pFairValueExecution->setExecutionState(FairValueExecution::IOC);
                            }
                            else
                            { 
                                if(_bEstablishTheoPosition)
                                {
                                    if(_iTheoPosition != _iPosition)
                                    {
                                        _cLogger << "Trying to establish theo position \n";
                                        _pFairValueExecution->setEstablishSegEntryPos(true);
                                    }
                                    else
                                    {
                                        _cLogger << "Theo position established \n";
                                        _pFairValueExecution->setEstablishSegEntryPos(false);
                                        _bEstablishTheoPosition = false;
                                    }
                                }

                                _pFairValueExecution->setExecutionState(FairValueExecution::Quoting);
                            }

                            _pFairValueExecution->newTheoQuotePrices(_iTheoBid, _iTheoOffer, _iTheoExitBid, _iTheoExitOffer, false);
                        }
                        else if(_eCurrentFigureAction == FigureAction::HALT_TRADING)
                        {
                            _pFairValueExecution->setExecutionState(FairValueExecution::Off);
                        }
                        else if(_eCurrentFigureAction == FigureAction::PATIENT)
                        {
                            if(_bIsHittingStrategy)
                            {
                                _pFairValueExecution->setExecutionState(FairValueExecution::PatientIOC);
                            }
                            else
                            {
                                _pFairValueExecution->setExecutionState(FairValueExecution::PatientQuoting);
                                _pFairValueExecution->newTheoQuotePrices(_iTheoBid, _iTheoOffer, _iTheoExitBid, _iTheoExitOffer, false);
                            }
                        }
                        else if(_eCurrentFigureAction == FigureAction::LIM_LIQ)
                        {
                            _pFairValueExecution->setExecutionState(FairValueExecution::Off);

                            limitLiqPosition();
                        }
                        else if(_eCurrentFigureAction == FigureAction::FAST_LIQ)
                        {
                            _pFairValueExecution->setExecutionState(FairValueExecution::Off);

                            fastLiqPosition();
                        }
                    }
                    else if(_eEngineState == TradeEngineBase::PAT_LIQ)
                    {
                        if(_eCurrentFigureAction == FigureAction::NO_FIGURE)
                        {
                            if(_bIsHittingStrategy)
                            {
                                _pFairValueExecution->setExecutionState(FairValueExecution::PatientIOC);
                            }
                            else
                            {
                                _pFairValueExecution->setExecutionState(FairValueExecution::PatientQuoting);
                                _pFairValueExecution->newTheoQuotePrices(_iTheoBid, _iTheoOffer, _iTheoExitBid, _iTheoExitOffer, false);
                            }
                        }
                        else if(_eCurrentFigureAction == FigureAction::HITTING)
                        {
                            if(_bIsHittingStrategy)
                            {
                                _pFairValueExecution->setExecutionState(FairValueExecution::PatientIOC);
                            }
                            else
                            {
                                _pFairValueExecution->setExecutionState(FairValueExecution::PatientHitting);
                                _pFairValueExecution->newTheoQuotePrices(_iTheoBid, _iTheoOffer, _iTheoExitBid, _iTheoExitOffer, false);
                            }
                        }
                        else if(_eCurrentFigureAction == FigureAction::HALT_TRADING)
                        {
                            _pFairValueExecution->setExecutionState(FairValueExecution::Off);
                        }
                        else if(_eCurrentFigureAction == FigureAction::PATIENT)
                        {
                            if(_bIsHittingStrategy)
                            {
                                _pFairValueExecution->setExecutionState(FairValueExecution::PatientIOC);
                            }
                            else
                            {
                                _pFairValueExecution->setExecutionState(FairValueExecution::PatientQuoting);
                                _pFairValueExecution->newTheoQuotePrices(_iTheoBid, _iTheoOffer, _iTheoExitBid, _iTheoExitOffer, false);
                            }
                        }
                        else if(_eCurrentFigureAction == FigureAction::LIM_LIQ)
                        {
                            _pFairValueExecution->setExecutionState(FairValueExecution::Off);

                            limitLiqPosition();
                        }
                        else if(_eCurrentFigureAction == FigureAction::FAST_LIQ)
                        {
                            _pFairValueExecution->setExecutionState(FairValueExecution::Off);

                            fastLiqPosition();
                        }
                    }
                    else if(_eEngineState == TradeEngineBase::HALT)
                    {
                        _pFairValueExecution->setExecutionState(FairValueExecution::Off);
                    }
                    else if(_eEngineState == TradeEngineBase::LIM_LIQ)
                    {
                        _pFairValueExecution->setExecutionState(FairValueExecution::Off);

                        if(_eCurrentFigureAction == FigureAction::FAST_LIQ)
                        {
                            fastLiqPosition();
                        }
                        else
                        {
                            limitLiqPosition();
                        }
                    }
                    else if(_eEngineState == TradeEngineBase::FAST_LIQ)
                    {
                        _pFairValueExecution->setExecutionState(FairValueExecution::Off);

                        fastLiqPosition();
                    }
                }

                _pFairValueExecution->wakeup();
            }
        }
    }
    else
    {
        _cLogger << "Ignore wake up call\n";
    }

/*
    KOEpochTime cNextCallTime = cCallTime + KOEpochTime(1,0);

    if(cNextCallTime < _cTradingEndTime)
    {
        SystemClock::GetInstance()->addNewWakeupCall(cNextCallTime, this);
        _cPreviousCallTime = cCallTime;
    }
*/

//    trc::compat::util::TimeVal end = trc::compat::util::TimeVal::now;
//    long iEpochEnd = end.sec() *1000000 + end.usec();

//    _cLogger << "wakeup function takes " << iEpochEnd - iEpochStart << "\n";
}

void SLBase::figureCall(KOEpochTime cCallTime, KOEpochTime cEventTime, const string& sFigureName, FigureAction::options eFigureAction)
{
    if(cCallTime < cEventTime)
    {
        // pre figure call
        _cLogger << "Pre figure call " << sFigureName << " time " << cCallTime.igetPrintable() << " Event time " << cEventTime.igetPrintable() << " Figure Action " << eFigureAction << "\n";
        boost::shared_ptr<OnGoingFigure> pOnGoingFigure (new OnGoingFigure);
        pOnGoingFigure->sFigureName = sFigureName;
        pOnGoingFigure->eFigureAction = eFigureAction;
        _vOnGoingEvents.push_back(pOnGoingFigure);
    }
    else
    {
        // post figure call
        _cLogger << "Post figure call " << sFigureName << " time " << cCallTime.igetPrintable() << " Event time " << cEventTime.igetPrintable() << " Figure Action " << eFigureAction << "\n";
        for(vector< boost::shared_ptr<OnGoingFigure> >::iterator onGoingFigureItr = _vOnGoingEvents.begin();
            onGoingFigureItr != _vOnGoingEvents.end();)
        {
            if((*onGoingFigureItr)->sFigureName.compare(sFigureName) == 0)
            {
                onGoingFigureItr = _vOnGoingEvents.erase(onGoingFigureItr);
            }
            else
            {
                onGoingFigureItr++;
            }
        }
    }

    if(_vOnGoingEvents.size() == 0)
    {
        _eCurrentFigureAction = FigureAction::NO_FIGURE;

        _cLogger << "resume quoting after figure \n";

        stringstream cStringStream;
        cStringStream << "resume quoting after figure";
        ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
    }
    else
    { 
        FigureAction::options eMaxFigureAction = FigureAction::HITTING;
        for(vector< boost::shared_ptr<OnGoingFigure> >::iterator onGoingFigureItr = _vOnGoingEvents.begin();
            onGoingFigureItr != _vOnGoingEvents.end();
            onGoingFigureItr++)
        {
            if((*onGoingFigureItr)->eFigureAction > eMaxFigureAction)
            {
                eMaxFigureAction = (*onGoingFigureItr)->eFigureAction;
            }
        }

        _eCurrentFigureAction = eMaxFigureAction;
        if(eMaxFigureAction == FigureAction::HITTING)
        {
            _cLogger << "swtich to hitting mode for figure \n";

            stringstream cStringStream;
            cStringStream << "switch to hitting mode for figure";
            ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
        }
        else if(eMaxFigureAction == FigureAction::PATIENT)
        {
            _cLogger << "swtich to patient hitting for figure \n";

            stringstream cStringStream;
            cStringStream << "switch to patient hitting mode for figure";
            ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
        }
        else if(eMaxFigureAction == FigureAction::HALT_TRADING)
        {
            _cLogger << "halt quoting for figure \n";

            stringstream cStringStream;
            cStringStream << "switch to halt mode for figure";
            ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
        }
        else if(eMaxFigureAction == FigureAction::LIM_LIQ)
        {
            _cLogger << "limit liquidation for figure \n";

            stringstream cStringStream;
            cStringStream << "switch to limit liquidation mode for figure";
            ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
        }
        else if(eMaxFigureAction == FigureAction::FAST_LIQ)
        {
            _cLogger << "Fast liquidation for figure \n";

            stringstream cStringStream;
            cStringStream << "switch to fast liquidation mode for figure";
            ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
        }
    }
}

void SLBase::updateTheoPosition()
{
    long iASC = _pProductInstrument->igetASC();

    if(_bUseASC == true)
    {
        if(_bASCBlocked == true)
        {
            if((_iPrevSignal > 0 && _pProductInstrument->dgetWeightedMid() < 0) ||
               (_iPrevSignal < 0 && _pProductInstrument->dgetWeightedMid() > 0))
            {
                _bASCBlocked = false;
            }
            else if(-1 * _pProductInstrument->dgetWeightedStdev() * _dExitStd < _pProductInstrument->dgetWeightedMid() &&
                    _pProductInstrument->dgetWeightedMid() < _pProductInstrument->dgetWeightedStdev() * _dExitStd)
            {
                _bASCBlocked = false;
            }
        }
        else
        {
            bool bASCTriggered = false;

            for(int i = 0; i < _vASCSettings.size(); i++)
            {
                if(iASC == _vASCSettings[i])
                {
                    bASCTriggered = true;
                    break;
                }
            }

            if(_pProductInstrument->dgetWeightedMid() >= _pProductInstrument->dgetWeightedStdev() * _dEntryStd)
            {
                if(bASCTriggered == true)
                {
                    if(_bASCStoppedOut == false)
                    {
                        _iTheoPosition = -1 * _iQuoteQty;
                    }
                }
                else
                {
                    _bASCBlocked = true;
                }
            }
            else if(_pProductInstrument->dgetWeightedMid() <= -1 * _pProductInstrument->dgetWeightedStdev() * _dEntryStd)
            {
                if(bASCTriggered == true)
                {
                    if(_bASCStoppedOut == false)
                    {
                        _iTheoPosition = _iQuoteQty;
                    }
                }
                else
                {
                    _bASCBlocked = true;
                }
            }
        }

        if(iASC == 1 || iASC == 3)
        {
            if(_pProductInstrument->dgetWeightedMid() >= _pProductInstrument->dgetWeightedStdev() * _dEntryStd * 4)
            {
                _iTheoPosition = 0;
                _bASCStoppedOut = true;
            }
            else if(_pProductInstrument->dgetWeightedMid() <= -1 * _pProductInstrument->dgetWeightedStdev() * _dEntryStd * 4)
            {
                _iTheoPosition = 0;
                _bASCStoppedOut = true;
            }
        }
    }
    else
    {
        if(_pProductInstrument->dgetWeightedMid() >= _pProductInstrument->dgetWeightedStdev() * _dEntryStd)
        {
            if(_bASCStoppedOut == false)
            {
                _iTheoPosition = -1 * _iQuoteQty;
            }
        }
        else if(_pProductInstrument->dgetWeightedMid() <= -1 * _pProductInstrument->dgetWeightedStdev() * _dEntryStd)
        {
            if(_bASCStoppedOut == false)
            {
                _iTheoPosition = _iQuoteQty;
            }
        }
    }

    if(_iTheoPosition > 0 && _pProductInstrument->dgetWeightedMid() >= _pProductInstrument->dgetWeightedStdev() * _dExitStd * -1)
    {
        _iTheoPosition = 0;
    }
    else if(_iTheoPosition < 0 && _pProductInstrument->dgetWeightedMid() <= _pProductInstrument->dgetWeightedStdev() * _dExitStd)
    {
        _iTheoPosition = 0;
    }

    if((_iPrevSignal > 0 && _pProductInstrument->dgetWeightedMid() < 0) ||
       (_iPrevSignal < 0 && _pProductInstrument->dgetWeightedMid() > 0))
    {
        _bASCStoppedOut = false;  
    }
    else if(_pProductInstrument->dgetWeightedMid() >= _pProductInstrument->dgetWeightedStdev() * _dExitStd * -1 && _pProductInstrument->dgetWeightedMid() <= _pProductInstrument->dgetWeightedStdev() * _dExitStd)
    {
        _bASCStoppedOut = false;  
    }

    _pFairValueExecution->setTheoPosition(_iTheoPosition);

    _iPrevSignal = _pProductInstrument->dgetWeightedMid();
}

void SLBase::calculateQuotePrice()
{
    double _dTheoExitBid = _pProductInstrument->dgetFrontQuoteBid(_dExitStd * -1);
    double _dTheoExitOffer = _pProductInstrument->dgetFrontQuoteOffer(_dExitStd * -1);
    double _dTheoBid = _pProductInstrument->dgetFrontQuoteBid(_dEntryStd);
    double _dTheoOffer = _pProductInstrument->dgetFrontQuoteOffer(_dEntryStd);


    if(_bUseRealSignalPrice)
    {
        _iTheoExitBid = floor(_dTheoExitBid / _pQuoteInstrument->dgetTickSize());
        _iTheoExitOffer = ceil(_dTheoExitOffer / _pQuoteInstrument->dgetTickSize());
        _iTheoBid = floor(_dTheoBid / _pQuoteInstrument->dgetTickSize());
        _iTheoOffer = ceil(_dTheoOffer / _pQuoteInstrument->dgetTickSize());

        if(_bASCStoppedOut)
        {
            _iTheoExitBid = _pQuoteInstrument->igetRealBestBid();
            _iTheoExitOffer = _pQuoteInstrument->igetRealBestAsk();
        }
    }
    else
    {
        _iTheoExitBid = floor(_dTheoExitBid);
        _iTheoExitOffer = ceil(_dTheoExitOffer);
        _iTheoBid = floor(_dTheoBid);
        _iTheoOffer = ceil(_dTheoOffer);

        if(_bASCStoppedOut)
        {
            _iTheoExitBid = _pQuoteInstrument->igetBestBid();
            _iTheoExitOffer = _pQuoteInstrument->igetBestAsk();
        }
    }
}

void SLBase::loadRollDelta()
{
    fstream ifsRollDelta;
    string sRollDeltaFileName = _sEngineRunTimePath + "RollDelta.cfg";
    ifsRollDelta.open(sRollDeltaFileName.c_str(), fstream::in);

    if(ifsRollDelta.is_open())
    {
        while(!ifsRollDelta.eof())
        {
            char sNewLine[512];
            ifsRollDelta.getline(sNewLine, sizeof(sNewLine));

            if(strlen(sNewLine) != 0 && sNewLine[0] != '#')
            {
                std::istringstream cDailyRollStream(sNewLine);
                string sElement;
                string sDate;
                string sProduct;
                double dRollDelta;             
 
                std::getline(cDailyRollStream, sElement, ',');
                sDate = sElement;
 
                std::getline(cDailyRollStream, sElement, ',');
                sProduct = sElement;

                std::getline(cDailyRollStream, sElement, ',');
                dRollDelta = atof(sElement.c_str());

                _mDailyRolls[pair<string, string>(sDate, sProduct)] = dRollDelta;
            }
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Cannot find roll delta file " << sRollDeltaFileName << ".";
        ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());

        _cLogger << "Cannot find roll delta file " << sRollDeltaFileName << "\n";
    }

    map< pair<string, string>, double >::iterator itr;
    
    itr = _mDailyRolls.find(pair<string, string>(_sTodayDate, vContractQuoteDatas[0]->sExchange + "." + vContractQuoteDatas[0]->sProduct));
    if(itr != _mDailyRolls.end())
    {
        double dQuoteRollDelta = itr->second;

        if(!_bUseRealSignalPrice)
        {
            long iQuoteRollDeltaInTicks = boost::math::iround(dQuoteRollDelta / _pQuoteInstrument->dgetTickSize());

            if(_pQuoteInstrument->sgetProductName().substr(0,2).compare("6E") == 0)
            {
                if(_pQuoteInstrument->dgetTickSize() < 0.0001 - 0.000000002)
                {
                    iQuoteRollDeltaInTicks = iQuoteRollDeltaInTicks / 2;
                }
            }
     
            if(_pQuoteInstrument->sgetProductName().substr(0,1).compare("L") == 0)
            {
                if(_pQuoteInstrument->dgetTickSize() < 0.01 - 0.002)
                {
                    iQuoteRollDeltaInTicks = iQuoteRollDeltaInTicks / 2;
                }
            }

            _pQuoteInstrument->applyEXMAAdjustment(iQuoteRollDeltaInTicks);
            _pQuoteInstrument->applyWeightedStdevAdjustment(iQuoteRollDeltaInTicks);
            _cLogger << "Applying Quote Instrument Roll Delta " << iQuoteRollDeltaInTicks << "\n";
        }
        else
        {
            _pQuoteInstrument->applyEXMAAdjustment(dQuoteRollDelta);
            _pQuoteInstrument->applyWeightedStdevAdjustment(dQuoteRollDelta);
            _cLogger << "Applying Quote Instrument Roll Delta " << dQuoteRollDelta << "\n";
        }
    }
}

void SLBase::orderConfirmHandler(int iOrderID)
{
    _pFairValueExecution->orderConfirmHandler(iOrderID);
}

void SLBase::orderUnexpectedConfirmHandler(int iOrderID)
{
    _pFairValueExecution->orderUnexpectedConfirmHandler(iOrderID);
}

void SLBase::orderFillHandler(int iOrderID, long iFilledQty, double dPrice)
{
    double dFillSpreadPrice = 0;

    if(!_bUseRealSignalPrice)
    {
        int iPriceInTicks = boost::math::iround(dPrice / _pQuoteInstrument->dgetTickSize());

        if(_pQuoteInstrument->sgetProductName().substr(0,2).compare("6E") == 0)
        {
            if(_pQuoteInstrument->dgetTickSize() < 0.0001 - 0.000000002)
            {
                iPriceInTicks = iPriceInTicks / 2;
            }
        }

        if(_pQuoteInstrument->sgetProductName().substr(0,1).compare("L") == 0)
        {
            if(_pQuoteInstrument->dgetTickSize() < 0.01 - 0.002)
            {
                iPriceInTicks = iPriceInTicks / 2;
            }
        }

        dFillSpreadPrice = _pProductInstrument->dgetPriceFromFrontLegPrice(iPriceInTicks);
    }
    else
    {
        dFillSpreadPrice = _pProductInstrument->dgetPriceFromFrontLegPrice(dPrice);
    }

    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|NEWFILL|" << iFilledQty << "|" << dPrice << "|" << dFillSpreadPrice << "\n";

    _iPosition = _iPosition + iFilledQty;
    _pFairValueExecution->orderFillHandler(iOrderID, iFilledQty, dPrice);

    _dRealisedPnL = _dRealisedPnL + iFilledQty * dPrice * -1;

    if(SystemClock::GetInstance()->cgetCurrentKOEpochTime() >= _cQuotingLimLiqTime)
    {
        if(_iPosition == 0)
        {
            _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|LIQCOST|" << (_dRealisedPnL - _dPnLAtLiq) << "\n";
        }
    }
}

void SLBase::manualFillHandler(string sProduct, long iFilledQty, double dPrice)
{
stringstream cStringStream;
cStringStream << "in manualFillHandler";
ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());

    double dFillSpreadPrice = 0;

    if(!_bUseRealSignalPrice)
    {
        int iPriceInTicks = boost::math::iround(dPrice / _pQuoteInstrument->dgetTickSize());

        if(_pQuoteInstrument->sgetProductName().substr(0,2).compare("6E") == 0)
        {
            if(_pQuoteInstrument->dgetTickSize() < 0.0001 - 0.000000002)
            {
                iPriceInTicks = iPriceInTicks / 2;
            }
        }

        if(_pQuoteInstrument->sgetProductName().substr(0,1).compare("L") == 0)
        {
            if(_pQuoteInstrument->dgetTickSize() < 0.01 - 0.002)
            {
                iPriceInTicks = iPriceInTicks / 2;
            }
        }

        dFillSpreadPrice = _pProductInstrument->dgetPriceFromFrontLegPrice(iPriceInTicks);
    }
    else
    {
        dFillSpreadPrice = _pProductInstrument->dgetPriceFromFrontLegPrice(dPrice);
    }

    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|NEWMANUALFILL|" << iFilledQty << "|" << dPrice << "|" << dFillSpreadPrice << "\n";

    _iPosition = _iPosition + iFilledQty;
    _pFairValueExecution->manualFillHandler(iFilledQty, dPrice);

    _dRealisedPnL = _dRealisedPnL + iFilledQty * dPrice * -1;
}

void SLBase::orderRejectHandler(int iOrderID)
{
    _pFairValueExecution->orderRejectHandler(iOrderID);
}

void SLBase::orderDeleteHandler(int iOrderID)
{
    _pFairValueExecution->orderDeleteHandler(iOrderID);
}

void SLBase::orderUnexpectedDeleteHandler(int iOrderID)
{
    _pFairValueExecution->orderUnexpectedDeleteHandler(iOrderID);
}

void SLBase::orderDeleteRejectHandler(int iOrderID)
{
    _pFairValueExecution->orderDeleteRejectHandler(iOrderID);
}

void SLBase::orderAmendRejectHandler(int iOrderID)
{
    _pFairValueExecution->orderAmendRejectHandler(iOrderID);
}

void SLBase::registerFigure()
{
    registerProductForFigure(vContractQuoteDatas[0]->sRoot);
}

void SLBase::limitLiqPosition()
{
    if(_pFairValueExecution->igetTotalOpenOrderQty() == 0)
    {
        long iLiquidationQty = -1 * _iPosition;

        if(_bIsHittingStrategy)
        {
            if(!_pIOCLiquidationOrder.get() || _pIOCLiquidationOrder->igetOrderRemainQty() == 0)
            {
                if(iLiquidationQty != 0)
                {
                    _pIOCLiquidationOrder.reset(new IOCOrder(vContractAccount[0]));

                    if(iLiquidationQty > 0)
                    {
                        if(_pIOCLiquidationOrder->bsubmitOrder(iLiquidationQty, _pQuoteInstrument->igetBestAsk()))
                        {
                            _cLogger << "New IOC liquidation order submitted \n";
                        }
                        else
                        {
                            _cLogger << "Failed to submit new IOC liquidation order \n";
                        }
                    }
                    else
                    {
                        if(_pIOCLiquidationOrder->bsubmitOrder(iLiquidationQty, _pQuoteInstrument->igetBestBid()))
                        {
                            _cLogger << "New order submitted \n";
                        }
                        else
                        {
                            _cLogger << "Failed to submit new order \n";
                        }
                    }
                }
            }
        }
        else
        {
            if(!_pLiquidationOrder.get() || _pLiquidationOrder->igetOrderRemainQty() == 0)
            {
                if(iLiquidationQty != 0)
                {
                    _pLiquidationOrder.reset(new HedgeOrder(vContractAccount[0], _pQuoteInstrument, &_cLogger));
                    if(_pLiquidationOrder->bsubmitOrder(iLiquidationQty, 0, 1, 100))
                    {
                        _cLogger << "limit liquidation order submitted \n";
                    }
                    else
                    {
                        _cLogger << "failed to submit limit liquidation order \n";
                    }
                }
            }
            else
            {
                if(iLiquidationQty == 0)
                {
                    _pLiquidationOrder->bdeleteOrder();
                }
                else
                {
                    if(_pLiquidationOrder->igetOrderRemainQty() != iLiquidationQty)
                    {
                        _pLiquidationOrder->bchangeHedgeOrderSize(iLiquidationQty);
                    }
                }
            }
        }
    }
}

void SLBase::fastLiqPosition()
{
    if((_iPosition > 0 && SystemClock::GetInstance()->cgetCurrentKOEpochTime().sec() % 2 == 1) ||
       (_iPosition < 0 && SystemClock::GetInstance()->cgetCurrentKOEpochTime().sec() % 2 == 0))
    {
        if(_pFairValueExecution->igetTotalOpenOrderQty() == 0)
        {
            long iLiquidationQty = -1 * _iPosition;

            if(_bIsHittingStrategy)
            {
                if(!_pIOCLiquidationOrder.get() || _pIOCLiquidationOrder->igetOrderRemainQty() == 0)
                {
                    if(iLiquidationQty != 0)
                    {
                        _pIOCLiquidationOrder.reset(new IOCOrder(vContractAccount[0]));

                        if(iLiquidationQty > 0)
                        {
                            if(_pIOCLiquidationOrder->bsubmitOrder(iLiquidationQty, _pQuoteInstrument->igetBestAsk()))
                            {
                                _cLogger << "New IOC liquidation order submitted \n";
                            }
                            else
                            {
                                _cLogger << "Failed to submit new IOC liquidation order \n";
                            }
                        }
                        else
                        {
                            if(_pIOCLiquidationOrder->bsubmitOrder(iLiquidationQty, _pQuoteInstrument->igetBestBid()))
                            {
                                _cLogger << "New order submitted \n";
                            }
                            else
                            {
                                _cLogger << "Failed to submit new order \n";
                            }
                        }
                    }
                }
            }
            else
            {
                if(!_pLiquidationOrder.get() || _pLiquidationOrder->igetOrderRemainQty() == 0)
                {
                    if(iLiquidationQty != 0)
                    {
                        _pLiquidationOrder.reset(new HedgeOrder(vContractAccount[0], _pQuoteInstrument, &_cLogger));
                        if(_pLiquidationOrder->bsubmitOrder(iLiquidationQty, 5, 1, 100))
                        {
                            _cLogger << "fast liquidation order submitted \n";
                        }
                        else
                        {
                            _cLogger << "failed to submit liquidation order \n";
                        }
                    }
                }
                else
                {
                    if(iLiquidationQty < 0)
                    {
                        if(_pLiquidationOrder->igetOrderRemainQty() != iLiquidationQty ||
                           _pLiquidationOrder->igetOrderPrice() <= _pQuoteInstrument->igetBestAsk())
                        {
                            _pLiquidationOrder->bchangeHedgeOrder(5, 1, 100, iLiquidationQty);
                        }
                    }
                    else if(iLiquidationQty == 0)
                    {
                        _pLiquidationOrder->bdeleteOrder();
                    }
                    else
                    {
                        if(_pLiquidationOrder->igetOrderRemainQty() != iLiquidationQty ||
                           _pLiquidationOrder->igetOrderPrice() >= _pQuoteInstrument->igetBestBid())
                        {
                            _pLiquidationOrder->bchangeHedgeOrder(5, 1, 100, iLiquidationQty);
                        }
                    }
                }
            }
        }
    }
}

}
