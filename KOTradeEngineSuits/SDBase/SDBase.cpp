#include "SDBase.h"
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

SDBase::SDBase(const string& sEngineRunTimePath,
               const string& sEngineType,
               const string& sEngineSlotName,
               KOEpochTime cTradingStartTime,
               KOEpochTime cTradingEndTime,
               SchedulerBase* pScheduler,
               const string& sTodayDate,
               const string& sSimType)
:TradeEngineBase(sEngineRunTimePath, sEngineType, sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, sSimType),
 _bProductPositiveCorrelation(true),
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

    _eCurrentFigureAction = FigureAction::NO_FIGURE;

    _bTradingStatsValid = false;

    _bUseRealSignalPrice = true;

    _bIsSegVol = false;

    _bUseASC = false;
    _bASCBlocked = false;

    _iPatLiqOffSet = 1800;

    _vSignalStateText.push_back("BUY");
    _vSignalStateText.push_back("SELL");
    _vSignalStateText.push_back("STOP");
    _vSignalStateText.push_back("FLAT");
    _vSignalStateText.push_back("FLAT_ALL_LONG");
    _vSignalStateText.push_back("FLAT_ALL_SHORT");

    _dLastQuoteMid = -1;

    _bIOC = false;
}

SDBase::~SDBase()
{
}

void SDBase::setupLogger(const string& sStrategyName)
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

void SDBase::loadTriggerSpace()
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

            _vSignalTimeElapsed.push_back(0);
            _vSignalPrevStates.push_back(STOP);

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
cerr << "out loadTriggerSpace \n";
}

void SDBase::dayInit()
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

void SDBase::dayTrade()
{
    TradeEngineBase::dayTrade();

    if(_sSimType == "Config")
    {
        activateSlot(vContractQuoteDatas[0]->sProduct, _iSlotID);
    }
}

void SDBase::dayRun()
{
    TradeEngineBase::dayRun();
}

void SDBase::dayStop()
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

void SDBase::receive(int iCID)
{
    TradeEngineBase::receive(iCID);
}

void SDBase::updateEngineStateOnTimer(KOEpochTime cCallTime)
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

void SDBase::saveSignal(long iTriggerIdx, long iInputEpochTimeStamp, long iInputDesiredPos, int iInputSignalState, bool bInputMarketOrder)
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

void SDBase::wakeup(KOEpochTime cCallTime)
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
                saveOvernightStats();
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

void SDBase::figureCall(KOEpochTime cCallTime, KOEpochTime cEventTime, const string& sFigureName, FigureAction::options eFigureAction)
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

void SDBase::loadRollDelta()
{
}

void SDBase::registerFigure()
{
    registerProductForFigure(vContractQuoteDatas[0]->sRoot);
}

}
