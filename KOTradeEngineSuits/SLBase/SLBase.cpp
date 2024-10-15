#include "SLBase.h"
#include <stdlib.h>
#include <ctime>
#include "../EngineInfra/ErrorHandler.h"
#include <boost/math/special_functions/round.hpp>
#include "../EngineInfra/SystemClock.h"
#include <string>
#include <H5Cpp.h>
#include <boost/program_options.hpp>

using namespace H5;
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
               const string& sSimType)
:TradeEngineBase(sEngineRunTimePath, sEngineType, sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, sSimType),
 _bQuoteInstrumentStatled(false),
 _bProductPositiveCorrelation(true),
 _bIsQuoteTime(false),
 _bIsPatLiqTimeReached(false),
 _bIsLimLiqTimeReached(false),
 _bIsFastLiqTimeReached(false),
 _bValidStatsSeen(false),
 _bInvalidStatTriggered(false)
{
    H5Eset_auto2(H5E_DEFAULT, NULL, NULL);

    _bWriteLog = false;
    _bWriteSpreadLog = false;

    _iQuoteTimeOffsetSeconds = 0;

    _eCurrentFigureAction = FigureAction::NO_FIGURE;

    _bTradingStatsValid = false;

    _bUseRealSignalPrice = true;

    _bIsSegVol = false;

    _bUseASC = false;
    _bASCBlocked = false;

    _pQuoteInstrument = NULL;
    _pProductInstrument = NULL;

    _iPatLiqOffSet = 1800;

    _vSignalStateText.push_back("BUY");
    _vSignalStateText.push_back("SELL");
    _vSignalStateText.push_back("STOP");
    _vSignalStateText.push_back("FLAT");
    _vSignalStateText.push_back("FLAT_ALL_LONG");
    _vSignalStateText.push_back("FLAT_ALL_SHORT");

    _dLastQuoteMid = -1;

    _bIOC = false;

    _bIsSV = false;

    _bIsTrading = true;

    _bIsBullishMom = false;
    _bIsBearishMom = false;
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

void SLBase::loadTriggerSpace()
{
    // set up triggers
    fstream cTriggerFileStream;
    if(_sSimType == "Config" || _sSimType == "Production")
    {
        cTriggerFileStream.open(_sEngineRunTimePath + "/tradesignal.cfg");
    }
    else if(_sSimType == "BaseSignal")
    {
        cTriggerFileStream.open(_sEngineRunTimePath + "/tradesignalspace.cfg");
    } 

    while(!cTriggerFileStream.eof())
    {
        char sNewLine[128];
        cTriggerFileStream.getline(sNewLine, sizeof(sNewLine));

        if(strlen(sNewLine) != 0 && sNewLine[0] != '#')
        {
            std::istringstream cTriggerStream(sNewLine);

            string sElement;

            std::getline(cTriggerStream, sElement, ';');
            _vTriggerID.push_back(sElement);

            std::getline(cTriggerStream, sElement, ';');
            _vEntryStd.push_back(stod(sElement));

            std::getline(cTriggerStream, sElement, ';');
            _vExitStd.push_back(stod(sElement));

            std::getline(cTriggerStream, sElement, ';');
            string sQuotingStartTime = sElement;           
            _cQuotingStartTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(_sTodayDate, sQuotingStartTime.substr(0,2), sQuotingStartTime.substr(2,2), sQuotingStartTime.substr(4,2)); 
            if(_cQuotingStartTime < _cTradingStartTime)
            {
                stringstream cStringStream;
                cStringStream << "Quoting start time " << _cQuotingStartTime.igetPrintable() << " smaller than trading start time " << _cTradingStartTime.igetPrintable() << "! Reset it to trading start time";
                ErrorHandler::GetInstance()->newWarningMsg("0", _sEngineSlotName, "ALL", cStringStream.str());
                _cQuotingStartTime = _cTradingStartTime;
            }

            std::getline(cTriggerStream, sElement, ';');
            string sQuotingEndTime = sElement;            
            _cQuotingEndTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(_sTodayDate, sQuotingEndTime.substr(0,2), sQuotingEndTime.substr(2,2), sQuotingEndTime.substr(4,2));
            if(_cQuotingEndTime >= _cTradingEndTime)
            {
                stringstream cStringStream;
                cStringStream << "Quoting end time " << _cQuotingEndTime.igetPrintable() << " greater than trading end time " << _cTradingEndTime.igetPrintable() << "! Reset it to trading end time";
                ErrorHandler::GetInstance()->newWarningMsg("0", _sEngineSlotName, "ALL", cStringStream.str());
                _cQuotingEndTime = _cTradingEndTime - KOEpochTime(1,0);
            }

            _pScheduler->addNewEngineCall(this, EngineEvent::TRADE, _cQuotingStartTime);

            std::getline(cTriggerStream, sElement, ';');
            string sPatTimeOffSet = sElement;            

            std::getline(cTriggerStream, sElement, ';');
            string sLimTimeOffSet = sElement;            

            std::getline(cTriggerStream, sElement, ';');
            string sFastTimeOffSet = sElement;            

            _cQuotingPatLiqTime = _cQuotingEndTime - KOEpochTime(atoi(sPatTimeOffSet.c_str()), 0);
            _cQuotingLimLiqTime = _cQuotingEndTime - KOEpochTime(atoi(sLimTimeOffSet.c_str()), 0);
            _cQuotingFastLiqTime = _cQuotingEndTime - KOEpochTime(atoi(sFastTimeOffSet.c_str()), 0);
        }
    }
    cTriggerFileStream.close();
}

void SLBase::dayInit()
{
    TradeEngineBase::dayInit();

    for(unsigned int i = 0; i < _vEntryStd.size(); i++)
    {
        _vTheoPositions.push_back(0);
        _vSignalStates.push_back(STOP);
        _vIsMarketOrders.push_back(false);
        _vTriggerTradeSignals.push_back(vector<TradeSignal>());
    }

    _dSpreadIndicator = 0;
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

    _iStdevLength = (_cVolEndTime.sec() - _cVolStartTime.sec()) / 60 * _iVolLength;
    _iProductStdevLength = (_cVolEndTime.sec() - _cVolStartTime.sec()) / 60 * _iProductVolLength;

    if(_pQuoteInstrument == NULL)
    {
        _pQuoteInstrument = new Instrument(vContractQuoteDatas[0]->sProduct, vContractQuoteDatas[0]->iCID, vContractQuoteDatas[0]->eInstrumentType, vContractQuoteDatas[0]->dTickSize, vContractQuoteDatas[0]->iMaxSpreadWidth, _bUseRealSignalPrice);
        _pQuoteInstrument->setIsLiveTrading(bisLiveTrading());
        _pQuoteInstrument->useTradingOut(0.2);
        _pQuoteInstrument->useEXMA(_dDriftLength, false, KOEpochTime(0,0), 1, _cVolStartTime, _cVolEndTime);
        _pQuoteInstrument->useWeightedStdev(_iStdevLength, false, KOEpochTime(0,0), 60, _cVolStartTime, _cVolEndTime, _bIsSV);
    }

    KOEpochTime cEngineLiveDuration = _cTradingEndTime - _cTradingStartTime;
    for(int i = 0; i < cEngineLiveDuration.sec();i++)
    {
        if(_pScheduler->_cSchedulerCfg.bUse100ms == true)
        {
            for(int j = 0; j < 10; j++)
            {
                _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(i, j*100000), this);
            }
        }
        else
        {
            _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(i,0), this);
        }
    }
}

void SLBase::dayTrade()
{
    TradeEngineBase::dayTrade();

    if(_sSimType == "Config")
    {
        activateSlot(vContractQuoteDatas[0]->sProduct, _iSlotID);
    }
}

void SLBase::dayRun()
{
    TradeEngineBase::dayRun();
}

void SLBase::dayStop()
{
    TradeEngineBase::dayStop();

    CompType cTradeSignalType(sizeof(TradeSignal));

    cTradeSignalType.insertMember("PORTFOLIO_ID", HOFFSET(TradeSignal, iPortfolioID), PredType::NATIVE_LONG);
    cTradeSignalType.insertMember("TIME", HOFFSET(TradeSignal, iEpochTimeStamp), PredType::NATIVE_LONG);
    cTradeSignalType.insertMember("DESIRED_POS", HOFFSET(TradeSignal, iDesiredPos), PredType::NATIVE_LONG);
    cTradeSignalType.insertMember("SIGNAL_STATE", HOFFSET(TradeSignal, iSignalState), PredType::NATIVE_LONG);
    cTradeSignalType.insertMember("MARKET_ORDER", HOFFSET(TradeSignal, bMarketOrder), PredType::NATIVE_B8);

    string sYear = _sTodayDate.substr(0, 4);

    for(unsigned int iTriggerIdx = 0; iTriggerIdx < _vTriggerID.size(); iTriggerIdx++)
    {
        string sTradeSignalH5FileName;

        if(_sSimType == "BaseSignal")
        {
            sTradeSignalH5FileName = _sOutputBasePath + "." + _vTriggerID[iTriggerIdx] + "/tradesignals" + sYear + ".h5";
        }
        else
        {
            sTradeSignalH5FileName = _sEngineRunTimePath + "tradesignals" + _sTodayDate + ".h5";
        }

        H5File cTradeSignalH5File;

        try
        {
            cTradeSignalH5File = H5File (sTradeSignalH5FileName, H5F_ACC_RDWR);
        }
        catch(FileIException& not_found_error)
        {
            cTradeSignalH5File = H5File (sTradeSignalH5FileName, H5F_ACC_EXCL);
        }

        TradeSignal* pTradeSignalArray = new TradeSignal[_vTriggerTradeSignals[iTriggerIdx].size()];
        std::copy(_vTriggerTradeSignals[iTriggerIdx].begin(), _vTriggerTradeSignals[iTriggerIdx].end(), pTradeSignalArray);
        hsize_t cDim[] = {_vTriggerTradeSignals[iTriggerIdx].size()};
        DataSpace cSpace(sizeof(cDim)/sizeof(hsize_t), cDim);

        bool bDataSetExists = false;
        try
        {
            cTradeSignalH5File.openDataSet(_sTodayDate);
            bDataSetExists = true;
        }
        catch(FileIException& not_found_error)
        {

        }

        if(bDataSetExists == true)
        {
            H5Ldelete(cTradeSignalH5File.getId(), _sTodayDate.c_str(), H5P_DEFAULT);
            cTradeSignalH5File.close();
            cTradeSignalH5File = H5File (sTradeSignalH5FileName, H5F_ACC_RDWR);
        }

        DataSet* pDataSet = new DataSet(cTradeSignalH5File.createDataSet(_sTodayDate, cTradeSignalType, cSpace));
        pDataSet->write(pTradeSignalArray, cTradeSignalType);

        delete pTradeSignalArray;
        delete pDataSet;

        cTradeSignalH5File.close();
    }
}

void SLBase::receive(int iCID)
{
    TradeEngineBase::receive(iCID);

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
cerr << "QUOTE|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << vContractQuoteDatas[iUpdateIndex]->iBidSize
            																					<< "|" << vContractQuoteDatas[iUpdateIndex]->dBestBid
																						        << "|" << vContractQuoteDatas[iUpdateIndex]->dBestAsk
																						        << "|" << vContractQuoteDatas[iUpdateIndex]->iAskSize 
                                                                                                << "|" << vContractQuoteDatas[iUpdateIndex]->dLastTradePrice 
                                                                                                << "|" << vContractQuoteDatas[iUpdateIndex]->iAccumuTradeSize  << "\n";
*/
		// Quote instrument updated
		_pQuoteInstrument->newMarketUpdate(vContractQuoteDatas[iUpdateIndex]);
		_iTotalQuoteInstruUpdate = _iTotalQuoteInstruUpdate + 1;
        _cLastQuoteUpdateTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
/*
_cLogger << "QUOTE|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << _pQuoteInstrument->igetBidSize() 
                                  												                << "|" << _pQuoteInstrument->igetBestBid() 
                    																	        << "|" << _pQuoteInstrument->igetBestAsk() 
                    																	        << "|" << _pQuoteInstrument->igetAskSize() 
                                                                                                << "|" << _pQuoteInstrument->igetLastTrade() 
                                                                                                << "|" << _pQuoteInstrument->igetAccumuTradeSize() << "\n";
*/
        if(_dLastQuoteMid > 0)
        {
            if(_pQuoteInstrument->dgetWeightedMid() > _dLastQuoteMid * 1.1 ||
               _pQuoteInstrument->dgetWeightedMid() < _dLastQuoteMid * 0.9)
            {
                std::stringstream cStringStream;
                cStringStream << "Error: " << _pQuoteInstrument->sgetProductName() << " received an update bigger or smaller than 10\%! New update: " << _pQuoteInstrument->dgetWeightedMid() << " Last update " << _dLastQuoteMid << " Time " << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();
                ErrorHandler::GetInstance()->newWarningMsg("0", "ALL", "ALL", cStringStream.str());
            }
        }

        _dLastQuoteMid = _pQuoteInstrument->dgetWeightedMid();
    }
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

            if(_sSimType == "Config")
            {
                deactivateSlot(vContractQuoteDatas[0]->sProduct, _iSlotID);
            }
        }

        _bIsQuoteTime = false;
        autoHaltTrading();
    }
}

void SLBase::saveSignal(long iTriggerIdx, long iInputEpochTimeStamp, long iInputDesiredPos, int iInputSignalState, bool bInputMarketOrder)
{
    if(_vTriggerTradeSignals[iTriggerIdx].size() == 0 ||
       (_vTriggerTradeSignals[iTriggerIdx].end()-1)->iDesiredPos != iInputDesiredPos ||
       (_vTriggerTradeSignals[iTriggerIdx].end()-1)->iSignalState != iInputSignalState ||
       (_vTriggerTradeSignals[iTriggerIdx].end()-1)->bMarketOrder != bInputMarketOrder)
    {
        _vTriggerTradeSignals[iTriggerIdx].push_back(TradeSignal());
        (_vTriggerTradeSignals[iTriggerIdx].end()-1)->iPortfolioID = 0;
        (_vTriggerTradeSignals[iTriggerIdx].end()-1)->iEpochTimeStamp = iInputEpochTimeStamp;
        (_vTriggerTradeSignals[iTriggerIdx].end()-1)->iDesiredPos = iInputDesiredPos;
        (_vTriggerTradeSignals[iTriggerIdx].end()-1)->iSignalState = iInputSignalState;
        (_vTriggerTradeSignals[iTriggerIdx].end()-1)->bMarketOrder = bInputMarketOrder;
    }
       
    if(_sSimType == "Config")
    {
        updateSlotSignal(vContractQuoteDatas[0]->sProduct, _iSlotID, iInputDesiredPos, iInputSignalState, bInputMarketOrder);
    }
}

void SLBase::printTheoTarget()
{
    if(_bIsQuoteTime == true)
    {
        if(_eCurrentFigureAction == FigureAction::NO_FIGURE) 
        {
            if(bcheckAllProductsReady())
            {
                if(_vSignalStates[0] == BUY || _vSignalStates[0] == SELL)
                {
                    stringstream cStringStream;
                    cStringStream << "Theo Position: " << _vTheoPositions[0];
                    ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
                }
            }
        }
    }
}

void SLBase::printSignal()
{
    if(_bIsQuoteTime == true)
    {
        if(bcheckAllProductsReady())
        {
            double dSignal = _pProductInstrument->dgetWeightedMid() / _pProductInstrument->dgetWeightedStdev();

            stringstream cStringStream;
            cStringStream << "Signal: " << dSignal;
            ErrorHandler::GetInstance()->newInfoMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
        }
    }
}

void SLBase::wakeup(KOEpochTime cCallTime)
{
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
        _cLastTimeAllProductsReady = cCallTime;
    }
    else
    {
        _cLogger << "Ignore wake up call\n";
    }

    if(_bTradingStatsValid == true)
    {
        if(_eEngineState == TradeEngineBase::HALT)
        {
            if(cCallTime.sec() % 60 == 0 && _pScheduler->bisLiveTrading())
            {
                saveOvernightStats(false);
            }
        }

        if(_bIsQuoteTime == true)
        {
            if(_eEngineState == TradeEngineBase::RUN)
            {
                if(_eCurrentFigureAction == FigureAction::NO_FIGURE) 
                {
                    if(bcheckAllProductsReady())
                    {
                        for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)
                        {
                            stringstream cStringStream;
                            cStringStream << cCallTime.igetPrintable() << " " << _vTriggerID[iTriggerIdx];
                            if(_vSignalStates[iTriggerIdx] == STOP)
                            {
                                cStringStream << " STOP";
                            }
                            else if(_vSignalStates[iTriggerIdx] == FLAT_ALL_LONG)
                            {
                                cStringStream << " FLAT_ALL_LONG";
                            }
                            else if(_vSignalStates[iTriggerIdx] == FLAT_ALL_SHORT)
                            {
                                cStringStream << " FLAT_ALL_SHORT";
                            }
                            else
                            {
                                cStringStream << " " << _vTheoPositions[iTriggerIdx];
                            }
                            cStringStream << " BEST" << std::endl;
                            _cLogger << cStringStream.str();

                            saveSignal(iTriggerIdx, cCallTime.igetPrintable(), _vTheoPositions[iTriggerIdx], _vSignalStates[iTriggerIdx], false);
                        }
                    }
                }
                else if(_eCurrentFigureAction == FigureAction::HALT_TRADING)
                {
                    for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)
                    {
                        stringstream cStringStream;
                        cStringStream << cCallTime.igetPrintable() << " " << _vTriggerID[iTriggerIdx];
                        cStringStream << " STOP";
                        cStringStream << " BEST" << std::endl;
                        _cLogger << cStringStream.str();

                        saveSignal(iTriggerIdx, cCallTime.igetPrintable(), _vTheoPositions[iTriggerIdx], STOP, false);
                    }
                }
                else if(_eCurrentFigureAction == FigureAction::PATIENT)
                {
                    for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)
                    {
                        stringstream cStringStream;
                        cStringStream << cCallTime.igetPrintable() << " " << _vTriggerID[iTriggerIdx];

                        SignalState eAdjustedSignalState;
                        if(_vSignalStates[iTriggerIdx] == BUY)
                        {
                            cStringStream << " FLAT_ALL_SHORT";
                            eAdjustedSignalState = FLAT_ALL_SHORT;
                        }
                        else if(_vSignalStates[iTriggerIdx] == SELL)
                        {
                            cStringStream << " FLAT_ALL_LONG";
                            eAdjustedSignalState = FLAT_ALL_LONG;
                        }
                        else if(_vSignalStates[iTriggerIdx] == STOP)
                        {
                            cStringStream << " STOP";
                            eAdjustedSignalState = STOP;
                        }
                        else if(_vSignalStates[iTriggerIdx] == FLAT)
                        {
                            cStringStream << " 0";
                            eAdjustedSignalState = FLAT;
                        }
                        else if(_vSignalStates[iTriggerIdx] == FLAT_ALL_LONG)
                        {
                            cStringStream << " FLAT_ALL_LONG";
                            eAdjustedSignalState = FLAT_ALL_LONG;
                        }
                        else if(_vSignalStates[iTriggerIdx] == FLAT_ALL_SHORT)
                        {
                            cStringStream << " FLAT_ALL_SHORT";
                            eAdjustedSignalState = FLAT_ALL_SHORT;
                        }

                        cStringStream << " BEST" << std::endl;
                        _cLogger << cStringStream.str();    

                        saveSignal(iTriggerIdx, cCallTime.igetPrintable(), _vTheoPositions[iTriggerIdx], eAdjustedSignalState, false);
                    }
                }
                else if(_eCurrentFigureAction == FigureAction::LIM_LIQ)
                {
                    for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)
                    {
                        stringstream cStringStream;
                        cStringStream << cCallTime.igetPrintable() << " " << _vTriggerID[iTriggerIdx];
                        cStringStream << " 0";
                        cStringStream << " BEST" << std::endl;
                        _cLogger << cStringStream.str();

                        saveSignal(iTriggerIdx, cCallTime.igetPrintable(), 0, FLAT, false);
                    }
                }
                else if(_eCurrentFigureAction == FigureAction::FAST_LIQ)
                {
                    for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)               
                    {
                        stringstream cStringStream;
                        cStringStream << cCallTime.igetPrintable() << " " << _vTriggerID[iTriggerIdx];
                        cStringStream << " 0";
                        cStringStream << " MARKET" << std::endl;
                        _cLogger << cStringStream.str();

                        saveSignal(iTriggerIdx, cCallTime.igetPrintable(), 0, FLAT, true);
                    }
                }
            }
            else if(_eEngineState == TradeEngineBase::PAT_LIQ)
            {
                if(_eCurrentFigureAction == FigureAction::NO_FIGURE)
                {
                    for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)
                    {
                        stringstream cStringStream;
                        cStringStream << cCallTime.igetPrintable() << " " << _vTriggerID[iTriggerIdx];

                        SignalState eAdjustedSignalState;
                        if(_vSignalStates[iTriggerIdx] == BUY)
                        {
                            cStringStream << " FLAT_ALL_SHORT";
                            eAdjustedSignalState = FLAT_ALL_SHORT;
                        }
                        else if(_vSignalStates[iTriggerIdx] == SELL)
                        {
                            cStringStream << " FLAT_ALL_LONG";
                            eAdjustedSignalState = FLAT_ALL_LONG;
                        }
                        else if(_vSignalStates[iTriggerIdx] == STOP)
                        {
                            cStringStream << " STOP";
                            eAdjustedSignalState = STOP;
                        }
                        else if(_vSignalStates[iTriggerIdx] == FLAT)
                        {
                            cStringStream << " 0";
                            eAdjustedSignalState = FLAT;
                        }
                        else if(_vSignalStates[iTriggerIdx] == FLAT_ALL_LONG)
                        {
                            cStringStream << " FLAT_ALL_LONG";
                            eAdjustedSignalState = FLAT_ALL_LONG;
                        }
                        else if(_vSignalStates[iTriggerIdx] == FLAT_ALL_SHORT)
                        {
                            cStringStream << " FLAT_ALL_SHORT";
                            eAdjustedSignalState = FLAT_ALL_SHORT;
                        }

                        cStringStream << " BEST" << std::endl;
                        _cLogger << cStringStream.str();

                        saveSignal(iTriggerIdx, cCallTime.igetPrintable(), _vTheoPositions[iTriggerIdx], eAdjustedSignalState, false);
                    }
                }
                else if(_eCurrentFigureAction == FigureAction::HALT_TRADING)
                {
                    for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)
                    {
                        stringstream cStringStream;
                        cStringStream << cCallTime.igetPrintable() << " " << _vTriggerID[iTriggerIdx];
                        cStringStream << " STOP";
                        cStringStream << " BEST" << std::endl;
                        _cLogger << cStringStream.str();

                        saveSignal(iTriggerIdx, cCallTime.igetPrintable(), _vTheoPositions[iTriggerIdx], STOP, false);
                    }
                }
                else if(_eCurrentFigureAction == FigureAction::PATIENT)
                {
                    for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)               
                    {
                        stringstream cStringStream;
                        cStringStream << cCallTime.igetPrintable() << " " << _vTriggerID[iTriggerIdx];

                        SignalState eAdjustedSignalState;
                        if(_vSignalStates[iTriggerIdx] == BUY)
                        {
                            cStringStream << " FLAT_ALL_SHORT";
                            eAdjustedSignalState = FLAT_ALL_SHORT;
                        }
                        else if(_vSignalStates[iTriggerIdx] == SELL)
                        {
                            cStringStream << " FLAT_ALL_LONG";
                            eAdjustedSignalState = FLAT_ALL_LONG;
                        }
                        else if(_vSignalStates[iTriggerIdx] == STOP)
                        {
                            cStringStream << " STOP";
                            eAdjustedSignalState = STOP;
                        }
                        else if(_vSignalStates[iTriggerIdx] == FLAT)
                        {
                            cStringStream << " 0";
                            eAdjustedSignalState = FLAT;
                        }
                        else if(_vSignalStates[iTriggerIdx] == FLAT_ALL_LONG)
                        {
                            cStringStream << " FLAT_ALL_LONG";
                            eAdjustedSignalState = FLAT_ALL_LONG;
                        }
                        else if(_vSignalStates[iTriggerIdx] == FLAT_ALL_SHORT)
                        {
                            cStringStream << " FLAT_ALL_SHORT";
                            eAdjustedSignalState = FLAT_ALL_SHORT;
                        }

                        cStringStream << " BEST" << std::endl;
                        _cLogger << cStringStream.str();

                        saveSignal(iTriggerIdx, cCallTime.igetPrintable(), _vTheoPositions[iTriggerIdx], eAdjustedSignalState, false);
                    }
                }
                else if(_eCurrentFigureAction == FigureAction::LIM_LIQ)
                {
                    for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)
                    {
                        stringstream cStringStream;
                        cStringStream << cCallTime.igetPrintable() << " " << _vTriggerID[iTriggerIdx];
                        cStringStream << " 0";
                        cStringStream << " BEST" << std::endl;
                        _cLogger << cStringStream.str();

                        saveSignal(iTriggerIdx, cCallTime.igetPrintable(), 0, FLAT, false);
                    }
                }
                else if(_eCurrentFigureAction == FigureAction::FAST_LIQ)
                {
                    for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)
                    {
                        stringstream cStringStream;
                        cStringStream << cCallTime.igetPrintable() << " " << _vTriggerID[iTriggerIdx];
                        cStringStream << " 0";
                        cStringStream << " MARKET" << std::endl;
                        _cLogger << cStringStream.str();

                        saveSignal(iTriggerIdx, cCallTime.igetPrintable(), 0, FLAT, true);
                    }
                }
            }
            else if(_eEngineState == TradeEngineBase::HALT)
            {
                for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)
                {
                    stringstream cStringStream;
                    cStringStream << cCallTime.igetPrintable() << " " << _vTriggerID[iTriggerIdx];
                    cStringStream << " STOP";
                    cStringStream << " BEST" << std::endl;
                    _cLogger << cStringStream.str();

                    saveSignal(iTriggerIdx, cCallTime.igetPrintable(), _vTheoPositions[iTriggerIdx], STOP, false);
                }
            }
            else if(_eEngineState == TradeEngineBase::LIM_LIQ)
            {
                if(_eCurrentFigureAction == FigureAction::FAST_LIQ)
                {
                    for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)
                    {
                        stringstream cStringStream;
                        cStringStream << cCallTime.igetPrintable() << " " << _vTriggerID[iTriggerIdx];
                        cStringStream << " 0";
                        cStringStream << " MARKET" << std::endl;
                        _cLogger << cStringStream.str();

                        saveSignal(iTriggerIdx, cCallTime.igetPrintable(), 0, FLAT, true);
                    }
                }
                else
                {
                    for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)
                    {
                        stringstream cStringStream;
                        cStringStream << cCallTime.igetPrintable() << " " << _vTriggerID[iTriggerIdx];
                        cStringStream << " 0";
                        cStringStream << " BEST" << std::endl;
                        _cLogger << cStringStream.str();

                        saveSignal(iTriggerIdx, cCallTime.igetPrintable(), 0, FLAT, false);
                    }
                }
            }
            else if(_eEngineState == TradeEngineBase::FAST_LIQ)
            {
                for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)
                {
                    stringstream cStringStream;
                    cStringStream << cCallTime.igetPrintable() << " " << _vTriggerID[iTriggerIdx];
                    cStringStream << " 0";
                    cStringStream << " MARKET" << std::endl;
                    _cLogger << cStringStream.str();

                    saveSignal(iTriggerIdx, cCallTime.igetPrintable(), 0, FLAT, true);
                }
            }
        }
    }

    setSlotReady(vContractQuoteDatas[0]->sProduct, _iSlotID);

    updateEngineStateOnTimer(cCallTime);
}

void SLBase::figureCall(KOEpochTime cCallTime, KOEpochTime cEventTime, const string& sFigureName, FigureAction::options eFigureAction, double dForecast, double dActual, bool bReleased)
{
    (void) dForecast;
    (void) dActual;
    (void) bReleased;

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
        FigureAction::options eMaxFigureAction = FigureAction::HALT_TRADING;
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
        if(eMaxFigureAction == FigureAction::PATIENT)
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

void SLBase::updateSignal()
{
/*
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
                    _eSignalState = SELL;
                    _bIsMarketOrder = false;
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
                    _eSignalState = BUY;
                    _bIsMarketOrder = false;
                }
                else
                {
                    _bASCBlocked = true;
                }
            }
        }
    }
    else
    {
        if(_pProductInstrument->dgetWeightedMid() >= _pProductInstrument->dgetWeightedStdev() * _dEntryStd)
        {
            _eSignalState = SELL;
            _bIsMarketOrder = false;
            _iTheoPosition = -1 * _iQuoteQty;
        }
        else if(_pProductInstrument->dgetWeightedMid() <= -1 * _pProductInstrument->dgetWeightedStdev() * _dEntryStd)
        {
            _eSignalState = BUY;
            _bIsMarketOrder = false;
            _iTheoPosition = _iQuoteQty;
        }
    }
*/

    for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)
    {
        double dBuyEntryThresh = -1 * _pProductInstrument->dgetWeightedStdev() * _vEntryStd[iTriggerIdx];
        double dBuyExitThresh = -1 * _pProductInstrument->dgetWeightedStdev() * _vExitStd[iTriggerIdx];
        double dSellEntryThresh = _pProductInstrument->dgetWeightedStdev() * _vEntryStd[iTriggerIdx];
        double dSellExitThresh = _pProductInstrument->dgetWeightedStdev() * _vExitStd[iTriggerIdx];

        if(_bIOC == false)
        {
            if(_pProductInstrument->dgetWeightedMid() <= dBuyEntryThresh)
            {
                if(_bIsTrading == true)
                {
                    if(_bIsBearishMom == false)
                    {
                        _vSignalStates[iTriggerIdx] = BUY;
                        _vIsMarketOrders[iTriggerIdx] = false;
                        _vTheoPositions[iTriggerIdx] = _iQuoteQty;
                    }
                    else
                    {
                        _vSignalStates[iTriggerIdx] = FLAT;
                        _vIsMarketOrders[iTriggerIdx] = false;
                        _vTheoPositions[iTriggerIdx] = 0;
                    }
                }
                else
                {
                    _vSignalStates[iTriggerIdx] = FLAT;
                    _vIsMarketOrders[iTriggerIdx] = false;
                    _vTheoPositions[iTriggerIdx] = 0;
                }
            }
            else if(dBuyEntryThresh < _pProductInstrument->dgetWeightedMid() &&
                    _pProductInstrument->dgetWeightedMid() < dBuyExitThresh) 
            {
                if(_bIsBearishMom == false)
                {
                    _vSignalStates[iTriggerIdx] = FLAT_ALL_SHORT;
                    _vIsMarketOrders[iTriggerIdx] = false;
                }
                else
                {
                    _vSignalStates[iTriggerIdx] = FLAT;
                    _vIsMarketOrders[iTriggerIdx] = false;
                    _vTheoPositions[iTriggerIdx] = 0;
                }
            }
            else if(dBuyExitThresh <= _pProductInstrument->dgetWeightedMid() &&
                    _pProductInstrument->dgetWeightedMid() <= dSellExitThresh)
            {
                _vSignalStates[iTriggerIdx] = FLAT;
                _vIsMarketOrders[iTriggerIdx] = false;
                _vTheoPositions[iTriggerIdx] = 0;
            }
            else if(dSellExitThresh < _pProductInstrument->dgetWeightedMid() &&
                    _pProductInstrument->dgetWeightedMid() < dSellEntryThresh)
            {
                if(_bIsBullishMom == false)
                {
                    _vSignalStates[iTriggerIdx] = FLAT_ALL_LONG;
                    _vIsMarketOrders[iTriggerIdx] = false;
                }
                else
                {
                    _vSignalStates[iTriggerIdx] = FLAT;
                    _vIsMarketOrders[iTriggerIdx] = false;
                    _vTheoPositions[iTriggerIdx] = 0;
                }
            }
            else if(dSellEntryThresh <= _pProductInstrument->dgetWeightedMid())
            {
                if(_bIsTrading == true)
                {
                    if(_bIsBullishMom == false)
                    {
                        _vSignalStates[iTriggerIdx] = SELL;
                        _vIsMarketOrders[iTriggerIdx] = false;
                        _vTheoPositions[iTriggerIdx] = -1 * _iQuoteQty;
                    }
                    else
                    {
                        _vSignalStates[iTriggerIdx] = FLAT;
                        _vIsMarketOrders[iTriggerIdx] = false;
                        _vTheoPositions[iTriggerIdx] = 0;
                    }
                }
                else
                {
                    _vSignalStates[iTriggerIdx] = FLAT;
                    _vIsMarketOrders[iTriggerIdx] = false;
                    _vTheoPositions[iTriggerIdx] = 0;
                }
            }
        }
        else
        {
            if(_pProductInstrument->dgetAsk() <= dBuyEntryThresh)
            {
                if(_bIsTrading == true)
                {
                    if(_bIsBearishMom == false)
                    {
                        _vSignalStates[iTriggerIdx] = BUY;
                        _vIsMarketOrders[iTriggerIdx] = false;
                        _vTheoPositions[iTriggerIdx] = _iQuoteQty;
                    }
                    else
                    {
                        _vSignalStates[iTriggerIdx] = FLAT;
                        _vIsMarketOrders[iTriggerIdx] = false;
                        _vTheoPositions[iTriggerIdx] = 0;
                    }
                }
                else
                {
                    _vSignalStates[iTriggerIdx] = FLAT;
                    _vIsMarketOrders[iTriggerIdx] = false;
                    _vTheoPositions[iTriggerIdx] = 0;
                }
            }
            else if(dSellEntryThresh <= _pProductInstrument->dgetBid())
            {
                if(_bIsTrading == true)
                {
                    if(_bIsBullishMom == false)
                    {
                        _vSignalStates[iTriggerIdx] = SELL;
                        _vIsMarketOrders[iTriggerIdx] = false;
                        _vTheoPositions[iTriggerIdx] = -1 * _iQuoteQty;
                    }
                    else
                    {
                        _vSignalStates[iTriggerIdx] = FLAT;
                        _vIsMarketOrders[iTriggerIdx] = false;
                        _vTheoPositions[iTriggerIdx] = 0;
                    }
                }
                else
                {
                    _vSignalStates[iTriggerIdx] = FLAT;
                    _vIsMarketOrders[iTriggerIdx] = false;
                    _vTheoPositions[iTriggerIdx] = 0;
                }
            }
            else if(dBuyExitThresh <= _pProductInstrument->dgetBid() &&
                    dSellExitThresh >= _pProductInstrument->dgetAsk())
            {
                _vSignalStates[iTriggerIdx] = FLAT;
                _vIsMarketOrders[iTriggerIdx] = false;
                _vTheoPositions[iTriggerIdx] = 0;
            }
            else if(dBuyExitThresh <= _pProductInstrument->dgetBid())
            {
                if(_bIsBullishMom == false)
                {
                    _vSignalStates[iTriggerIdx] = FLAT_ALL_LONG;
                    _vIsMarketOrders[iTriggerIdx] = false;
                }
                else
                {
                    _vSignalStates[iTriggerIdx] = FLAT;
                    _vIsMarketOrders[iTriggerIdx] = false;
                    _vTheoPositions[iTriggerIdx] = 0;
                }
            }
            else if(dSellExitThresh >= _pProductInstrument->dgetAsk())
            {
                if(_bIsBearishMom == false)
                {
                    _vSignalStates[iTriggerIdx] = FLAT_ALL_SHORT;
                    _vIsMarketOrders[iTriggerIdx] = false;
                }
                else
                {
                    _vSignalStates[iTriggerIdx] = FLAT;
                    _vIsMarketOrders[iTriggerIdx] = false;
                    _vTheoPositions[iTriggerIdx] = 0;
                }
            }
        }
    }

    _iPrevSignal = _pProductInstrument->dgetWeightedMid();
}

void SLBase::loadRollDelta()
{
    fstream ifsRollDelta;
    string sRollDeltaFileName = _sEngineRunTimePath + "rolldelta.cfg";
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
                dRollDelta = stod(sElement);

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

        _dLastQuoteMid = _dLastQuoteMid + dQuoteRollDelta;
    }
}

void SLBase::registerFigure()
{
    registerProductForFigure(vContractQuoteDatas[0]->sRoot);
}

}
