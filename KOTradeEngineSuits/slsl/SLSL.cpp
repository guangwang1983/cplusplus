#include <stdlib.h>
#include <boost/math/special_functions/round.hpp>

#include "SLSL.h"
#include "../EngineInfra/ErrorHandler.h"
#include "../EngineInfra/SystemClock.h"

using namespace boost::posix_time;
using std::stringstream;

namespace KO
{

SLSL::SLSL(const string& sEngineRunTimePath,
           const string& sEngineSlotName,
           KOEpochTime cTradingStartTime,
           KOEpochTime cTradingEndTime,
           SchedulerBase* pScheduler,
           string sTodayDate,
           const string& sSimType,
           KOEpochTime cSlotFirstWakeupCallTime)
:SLBase(sEngineRunTimePath, "SLSL", sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, sSimType, cSlotFirstWakeupCallTime),
 _bSignalInstrumentStaled(false),
 _dLeaderWeight(1),
 _bUpdateStats(true)
{
    _bProductPositiveCorrelation = true;
    _pSignalInstrument = NULL;
    _dLastSignalMid = -1;

    _bIsBullishMom = false;
    _bIsBearishMom = false;
}

SLSL::~SLSL()
{
    if(_pSignalInstrument != NULL)
    {
        delete _pSignalInstrument;
    }
}

void SLSL::readFromStream(istream& is)
{
    string sQuotingStartTime = "";
    string sQuotingEndTime = "";

    while(!is.eof())
    {
        string sParam;
        is >> sParam;

        std::istringstream cParamStream (sParam);

        string sParamName;

        std::getline(cParamStream, sParamName, ':');

        if(sParamName == "Logging")
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            if(sValue == "0")
            {
                _bWriteLog = false;
                _bWriteSpreadLog = false;
            }
            else if(sValue == "1")
            {
                _bWriteLog = true;
                _bWriteSpreadLog = false;
            }
            else if(sValue == "2")
            {
                _bWriteLog = false;
                _bWriteSpreadLog = true;
            }
        }
        else if(sParamName == "SegVol")
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            if(sValue == "1")
            {
                _bIsSegVol = true;
            }
            else
            {
                _bIsSegVol = false;
            }
        }
        else if(sParamName == "ProductVola")
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            _iProductVolLength = atoi(sValue.c_str());
        }
        else if(sParamName == "SpreadVola")
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            _iVolLength = atoi(sValue.c_str());
        }
        else if(sParamName == "Drift")
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            _dDriftLength = atoi(sValue.c_str());
        }
        else if(sParamName == "Qty")
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            _iQuoteQty = atoi(sValue.c_str());
        }
        else if(sParamName.compare("ASC") == 0)
        {
            _bUseASC = true;

            string sASC;
            std::getline(cParamStream, sASC, ':');

            for(unsigned int i = 0; i < sASC.length(); i++)
            {
                _vASCSettings.push_back(atoi(sASC.substr(i,1).c_str()));
            }
        }
        else if(sParamName.compare("RealSignalPrice") == 0)
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            if(sValue == "0")
            {
                _bUseRealSignalPrice = false;
            }
            else
            {
                _bUseRealSignalPrice = true;
            }
        }
        else if(sParamName.compare("LeaderWeight") == 0)
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            _dLeaderWeight = stod(sValue);
        }
        else if(sParamName.compare("UpdateStats") == 0)
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            if(sValue == "0")
            {
                _bUpdateStats = false;
            }
            else
            {
                _bUpdateStats = true;
            }
        }
        else if(sParamName.compare("TargetCorr") == 0)
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            if(sValue == "0")
            {
                _bProductPositiveCorrelation = false;
            }
            else
            {
                _bProductPositiveCorrelation = true;
            }
        }
        else if(sParamName.compare("SignalType") == 0)
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            if(sValue == "SV")
            {
                _bIsSV = true;
            }
        }
    }

    loadTriggerSpace();

    _cPreviousCallTime = _cQuotingStartTime;

	registerFigure();
}

void SLSL::dayInit()
{
    setupLogger("SLSL");

    _cLogger << "Daily Strategy initialisation \n";		
    SLBase::dayInit();

    _iTotalSignalInstruUpdate = 0;

    _cLogger << "Engine quoting start time " << _cQuotingStartTime.igetPrintable() << "\n";
    _cLogger << "Engine quoting end time " << _cQuotingEndTime.igetPrintable() << "\n";

    if(!_pSignalInstrument)
    {
        _pSignalInstrument = new Instrument(vContractQuoteDatas[1]->sProduct, vContractQuoteDatas[1]->iCID, vContractQuoteDatas[1]->eInstrumentType, vContractQuoteDatas[1]->dTickSize, vContractQuoteDatas[1]->iMaxSpreadWidth, _bUseRealSignalPrice);
        _pSignalInstrument->useEXMA(_dDriftLength, false, KOEpochTime(0,0), 1, _cTradingStartTime, _cTradingEndTime);
        _pSignalInstrument->useWeightedStdev(_iStdevLength, false, KOEpochTime(0,0), 60, _cVolStartTime, _cVolEndTime, _bIsSV);

        _pProductInstrument = new SyntheticSpread("Product",
                                  _pQuoteInstrument,
                                  SyntheticSpread::WhiteAbs,
                                  _pSignalInstrument,
                                  SyntheticSpread::WhiteAbs,
                                  _dLeaderWeight,
                                  _bProductPositiveCorrelation,
                                  _bUseRealSignalPrice);
        _pProductInstrument->useWeightedStdev(_iProductStdevLength, false, KOEpochTime(0,0), 60, _cVolStartTime, _cVolEndTime, false);

        loadOvernightStats();
    }

    if(_pQuoteInstrument->bgetWeightedStdevValid())
    {
        _cLogger << "Quote stat is valid \n";
//        _cLogger << "Quote stat is valid: " << _pQuoteInstrument->dgetWeightedStdev() << "\n";
//        _cLogger << "_pQuoteInstrument->dgetWeightedStdevEXMA() " << _pQuoteInstrument->dgetWeightedStdevEXMA() << "\n";
//        _cLogger << "_pQuoteInstrument->dgetWeightedStdevSqrdEXMA() " << _pQuoteInstrument->dgetWeightedStdevSqrdEXMA() << "\n";
    }

    if(_pSignalInstrument->bgetWeightedStdevValid())
    {
        _cLogger << "Signal stat is valid \n";
        //_cLogger << "Signal stat is valid: " << _pSignalInstrument->dgetWeightedStdev() << "\n";
        //_cLogger << "_pSignalInstrument->dgetWeightedStdevEXMA() " << _pSignalInstrument->dgetWeightedStdevEXMA() << "\n";
        //_cLogger << "_pSignalInstrument->dgetWeightedStdevSqrdEXMA() " << _pSignalInstrument->dgetWeightedStdevSqrdEXMA() << "\n";
    }

    if(_pProductInstrument->bgetWeightedStdevValid())
    {
        _cLogger << "Product stat is valid \n";
        _cLogger << "Product stat is valid: " << _pProductInstrument->dgetWeightedStdev() << "\n";

        for(unsigned int i = 0; i < _vEntryStd.size(); i++)
        {
            _cLogger << "Trigger set " << i << " EntryThreshold " << _pProductInstrument->dgetWeightedStdev() * _vEntryStd[i] << " " << "ExitThreshold " << _pProductInstrument->dgetWeightedStdev() * _vExitStd[i] << "\n";
        }
//      _cLogger << "_pQuoteInstrument->dgetWeightedStdevEXMA() " << _pQuoteInstrument->dgetWeightedStdevEXMA() << "\n";
//      _cLogger << "_pQuoteInstrument->dgetWeightedStdevSqrdEXMA() " << _pQuoteInstrument->dgetWeightedStdevSqrdEXMA() << "\n";
    }

    _cLogger << "Daily strategy initialisation finished" << std::endl;		
}

void SLSL::dayTrade()
{
    _cLogger << "Daily strategy trade " << std::endl;

    SLBase::dayTrade();

    _cLogger << "Daily strategy trade finished " << std::endl;
}

void SLSL::dayRun()
{
    _cLogger << "Daily strategy run " << std::endl;

    _pSignalInstrument->newMarketUpdate(vContractQuoteDatas[1]);
    _dLastSignalMid = _pSignalInstrument->dgetWeightedMid();

    SLBase::dayRun();

    _cLogger << "Daily strategy run finished " << std::endl;
}

void SLSL::dayStop()
{
    _cLogger << "Daily strategy stop " << std::endl;		

    SLBase::dayStop();
//    _pFairValueExecution->setExecutionState(FairValueExecution::Off);

    loadRollDelta();

    if(_bUpdateStats == true)
    {
        if(_pScheduler->bisLiveTrading() == true)
        {
            saveOvernightStats(false);
        }
        else
        {
            if((SystemClock::GetInstance()->cgetCurrentKOEpochTime() - _cLastTimeAllProductsReady).igetPrintable() < 14400000000 && (SystemClock::GetInstance()->cgetCurrentKOEpochTime() - _cLastQuoteUpdateTime).igetPrintable() < 14400000000 && (SystemClock::GetInstance()->cgetCurrentKOEpochTime() - _cLastSignalUpdateTime).igetPrintable() < 14400000000)
            {
                saveOvernightStats(false);
            }
            else
            {
                saveOvernightStats(true);
            }
        }
    }

    _pQuoteInstrument->eodReset();	
    _pSignalInstrument->eodReset();	

    _cLogger << "Daily strategy stop finished " << std::endl;		

    _cLogger.closeFile();
    _cSpreadLogger.closeFile();
}

void SLSL::receive(int iCID)
{
    SLBase::receive(iCID);

    int iUpdateIndex = -1;

    if(vContractQuoteDatas[0]->iCID == iCID)
    {
        iUpdateIndex = 0;
    }
    else if(vContractQuoteDatas[1]->iCID == iCID)
    {
        iUpdateIndex = 1;
    }
	if(iUpdateIndex == 1)
	{
/*
cerr << "SIGNAL|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << vContractQuoteDatas[iUpdateIndex]->iBidSize
																						         << "|" << vContractQuoteDatas[iUpdateIndex]->dBestBid
																						         << "|" << vContractQuoteDatas[iUpdateIndex]->dBestAsk
																						         << "|" << vContractQuoteDatas[iUpdateIndex]->iAskSize << "\n";
*/
		// Singal instrument updated
		_pSignalInstrument->newMarketUpdate(vContractQuoteDatas[iUpdateIndex]);
		_iTotalSignalInstruUpdate = _iTotalSignalInstruUpdate + 1;
        _cLastSignalUpdateTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
/*
_cLogger << "SIGNAL|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << _pSignalInstrument->igetBidSize() 
																					             << "|" << _pSignalInstrument->igetBestBid() 
																					             << "|" << _pSignalInstrument->igetBestAsk() 
																					             << "|" << _pSignalInstrument->igetAskSize()
                                                                                                 << "|" << _pSignalInstrument->igetLastTrade()
                                                                                                 << "|" << _pSignalInstrument->igetAccumuTradeSize() << "\n";
*/
        if(_dLastSignalMid > 0)
        {
            if(_pSignalInstrument->dgetWeightedMid() > _dLastSignalMid * 1.1 ||
               _pSignalInstrument->dgetWeightedMid() < _dLastSignalMid * 0.9)
            {
                std::stringstream cStringStream;
                cStringStream << "Error: " << _pSignalInstrument->sgetProductName() << " received an update bigger or smaller than 10\%! New update: " << _pSignalInstrument->dgetWeightedMid() << " Last update " << _dLastSignalMid << " Time " << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();
                ErrorHandler::GetInstance()->newWarningMsg("0", "ALL", "ALL", cStringStream.str());
            }
        }

        _dLastSignalMid = _pSignalInstrument->dgetWeightedMid();
	}
}

void SLSL::updateStatistics(KOEpochTime cCallTime)
{
    _bTradingStatsValid = false;

    _pQuoteInstrument->wakeup(cCallTime);	
    _pSignalInstrument->wakeup(cCallTime);	

    if(_pQuoteInstrument->bgetEXMAValid() && _pQuoteInstrument->bgetWeightedStdevValid() && _pSignalInstrument->bgetEXMAValid() && _pSignalInstrument->bgetWeightedStdevValid())
    {
        _pProductInstrument->wakeup(cCallTime);		

        _dSpreadIndicator = _pProductInstrument->dgetWeightedMid();

        if(_pProductInstrument->bgetWeightedStdevValid())
        {
            _bIsBullishMom = false;
            _bIsBearishMom = false;

/*
            double dQuoteLeg = (_pQuoteInstrument->dgetWeightedMid() - _pQuoteInstrument->dgetEXMA()) / _pQuoteInstrument->dgetWeightedStdev();
            double dSignalLeg = (_pSignalInstrument->dgetWeightedMid() - _pSignalInstrument->dgetEXMA()) / _pSignalInstrument->dgetWeightedStdev();

            if(abs(dQuoteLeg) > 3 && abs(dSignalLeg) > 3)
            {
                _bIsBullishMom = true;
                _bIsBearishMom = true;

                if(dQuoteLeg > 0 && dSignalLeg > 0 && dSignalLeg > dQuoteLeg)
                {
                    _bIsBearishMom = false;
                }
                else if(dQuoteLeg < 0 && dSignalLeg < 0 && dSignalLeg < dQuoteLeg)
                {
                    _bIsBullishMom = false;
                }
            }
*/

            updateSignal();

            _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() 
                     << "|" << "ST"
                     << "|" << _pQuoteInstrument->dgetWeightedMid()
                     << "|" << _pSignalInstrument->dgetWeightedMid()
                     << "|" << _pQuoteInstrument->dgetEXMA()
                     << "|" << _pSignalInstrument->dgetEXMA()
                     << "|" << _pQuoteInstrument->dgetWeightedStdev()
                     << "|" << _pSignalInstrument->dgetWeightedStdev()
                     << "|" << _dSpreadIndicator << std::endl;

/*
            if(_dSpreadIndicator > _pProductInstrument->dgetWeightedStdev() * 5)
            {
                string sProductString = "";
                for(vector<QuoteData*>::iterator itr = vContractQuoteDatas.begin();
                    itr != vContractQuoteDatas.end();
                    itr++)
                {
                    if(itr == vContractQuoteDatas.begin())
                    {
                        sProductString = (*itr)->sRoot;
                    }
                    else
                    {
                        sProductString = sProductString + "_" + (*itr)->sRoot;
                    }
                }

                cerr << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " outlier spread value! " << _dSpreadIndicator << " - " << _pProductInstrument->dgetWeightedStdev() << " " << sProductString << "\n";
            }
            else if(_dSpreadIndicator < _pProductInstrument->dgetWeightedStdev() * -5)
            {
                string sProductString = "";
                for(vector<QuoteData*>::iterator itr = vContractQuoteDatas.begin();
                    itr != vContractQuoteDatas.end();
                    itr++)
                {
                    if(itr == vContractQuoteDatas.begin())
                    {
                        sProductString = (*itr)->sRoot;
                    }
                    else
                    {
                        sProductString = sProductString + "_" + (*itr)->sRoot;
                    }
                }

                cerr << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " outlier spread value! " << _dSpreadIndicator << " - " << _pProductInstrument->dgetWeightedStdev() << " " << sProductString << "\n";
            }
*/

            _bTradingStatsValid = true;
        }
        else
        {
            _cLogger << "Ignore wake up call. Spread statistic not valid. numDataPoint is " << _pProductInstrument->igetWeightedStdevNumDataPoints() << std::endl;
        }
    }
    else
    {
        _cLogger << "Ignore wake up call. Instrument statistic not valid. numDataPoint is " << _pQuoteInstrument->igetWeightedStdevNumDataPoints() << ". " << _iStdevLength << " points needed" << std::endl;
    }	

}

void SLSL::writeSpreadLog()
{
    if(_pQuoteInstrument != NULL && _pSignalInstrument != NULL && _pProductInstrument != NULL)
    {
        if(_pQuoteInstrument->igetBidSize() != 0 &&
           _pQuoteInstrument->igetAskSize() != 0 &&
           _pSignalInstrument->igetBidSize() != 0 &&
           _pSignalInstrument->igetAskSize() != 0)
        {
            _cSpreadLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable()  
                           << "|" << _pProductInstrument->dgetFrontLegPrice()
                           << "|" << _pProductInstrument->dgetBackLegPrice()
                           << "|" << _dLeaderWeight << "\n";
        }
    }
}

bool SLSL::bcheckAllProductsReady()
{
    bool bResult = false;
    if(_pQuoteInstrument != NULL && _pSignalInstrument != NULL && _pProductInstrument != NULL)
    {
        if(_pQuoteInstrument->bgetPriceValid() && _pSignalInstrument->bgetPriceValid())
        {
            bResult = true;
        }
        else
        {
            _cLogger << "Instrument prices not valid" << std::endl;
        }
    }
    else
    {
        _cLogger << "Instrument objects not ready" << std::endl;
    }

    return bResult;
}

void SLSL::wakeup(KOEpochTime cCallTime)
{
    SLBase::wakeup(cCallTime);
}

void SLSL::loadRollDelta()
{
    SLBase::loadRollDelta();

    map< pair<string, string>, double >::iterator itr;
    
    itr = _mDailyRolls.find(pair<string, string>(_sTodayDate, vContractQuoteDatas[1]->sExchange + "." + vContractQuoteDatas[1]->sProduct));
    if(itr != _mDailyRolls.end())
    {
        double dSignalRollDelta = itr->second;

        if(!_bUseRealSignalPrice)
        {
            long iSignalRollDeltaInTicks = boost::math::iround(dSignalRollDelta / _pSignalInstrument->dgetTickSize());

            if(_pSignalInstrument->sgetProductName().substr(0,2).compare("6E") == 0)
            {
                if(_pSignalInstrument->dgetTickSize() < 0.0001 - 0.000000002)
                {
                    iSignalRollDeltaInTicks = iSignalRollDeltaInTicks / 2;
                }
            }

            if(_pSignalInstrument->sgetProductName().substr(0,1).compare("L") == 0)
            {
                if(_pSignalInstrument->dgetTickSize() < 0.01 - 0.002)
                {
                    iSignalRollDeltaInTicks = iSignalRollDeltaInTicks / 2;
                }
            }

            _pSignalInstrument->applyEXMAAdjustment(iSignalRollDeltaInTicks);
            _pSignalInstrument->applyWeightedStdevAdjustment(iSignalRollDeltaInTicks);
            _cLogger << "Applying Signal Instrument Roll Delta " << iSignalRollDeltaInTicks << "\n";
        }
        else
        {
            _pSignalInstrument->applyEXMAAdjustment(dSignalRollDelta);
            _pSignalInstrument->applyWeightedStdevAdjustment(dSignalRollDelta);
            _cLogger << "Applying Signal Instrument Roll Delta " << dSignalRollDelta << "\n";
        }
    }
}

void SLSL::saveOvernightStats(bool bRemoveToday)
{
    stringstream cStringStream;
    cStringStream.precision(20);

    string sOvernightStatFileName = _sEngineRunTimePath + "overnightstats.cfg";

    boost::gregorian::date cTodayDate = boost::gregorian::from_undelimited_string(_sTodayDate);

    bool bCurrentDayWritten = false;

    for(map< boost::gregorian::date, boost::shared_ptr<DailyStat> >::iterator itr = _mDailyStats.begin(); itr != _mDailyStats.end(); ++itr)
    {
        if(itr->first != cTodayDate)
        {
            if(itr->first > cTodayDate && !bCurrentDayWritten)
            {
                if(_pQuoteInstrument->dgetEXMA() > 0.001 && _pSignalInstrument->dgetEXMA() > 0.001 && bRemoveToday == false)
                {
                    cStringStream << _sTodayDate << ","
                                  << _pQuoteInstrument->dgetEXMA() << ","
                                  << _pQuoteInstrument->igetEXMANumDataPoints() << ","
                                  << _pQuoteInstrument->dgetWeightedStdevEXMA() << ","
                                  << _pQuoteInstrument->dgetWeightedStdevSqrdEXMA() << ","
                                  << _pQuoteInstrument->igetWeightedStdevNumDataPoints() << ","
                                  << _pQuoteInstrument->dgetWeightedStdevAdjustment() << ","
                                  << _pSignalInstrument->dgetEXMA() << ","
                                  << _pSignalInstrument->igetEXMANumDataPoints() << ","
                                  << _pSignalInstrument->dgetWeightedStdevEXMA() << ","
                                  << _pSignalInstrument->dgetWeightedStdevSqrdEXMA() << ","
                                  << _pSignalInstrument->igetWeightedStdevNumDataPoints() << ","
                                  << _pSignalInstrument->dgetWeightedStdevAdjustment() << ","
                                  << _pProductInstrument->dgetWeightedStdevEXMA() << ","
                                  << _pProductInstrument->dgetWeightedStdevSqrdEXMA() << ","
                                  << _pProductInstrument->igetWeightedStdevNumDataPoints() << ","
                                  << _pProductInstrument->dgetWeightedStdevAdjustment() << ","
                                  << _dLastQuoteMid << "," 
                                  << _dLastSignalMid << "\n";
                }

                bCurrentDayWritten = true;
            }

            cStringStream << to_iso_string(itr->first) << ","
                          << itr->second->dQuoteInstrumentEXMA << ","
                          << itr->second->iQuoteInstrumentEXMANumDataPoints << ","
                          << itr->second->dQuoteInstrumentWeightedStdevEXMA << ","
                          << itr->second->dQuoteInstrumentWeightedStdevSqrdEXMA << ","
                          << itr->second->iQuoteInstrumentWeightedStdevNumDataPoints << ","
                          << itr->second->dQuoteInstrumentWeightedStdevAdjustment << ","
                          << itr->second->dSignalInstrumentEXMA << ","
                          << itr->second->iSignalInstrumentEXMANumDataPoints << ","
                          << itr->second->dSignalInstrumentWeightedStdevEXMA << ","
                          << itr->second->dSignalInstrumentWeightedStdevSqrdEXMA << ","
                          << itr->second->iSignalInstrumentWeightedStdevNumDataPoints << ","
                          << itr->second->dSignalInstrumentWeightedStdevAdjustment << ","
                          << itr->second->dProductWeightedStdevEXMA << ","
                          << itr->second->dProductWeightedStdevSqrdEXMA << ","
                          << itr->second->iProductWeightedStdevNumDataPoints << ","
                          << itr->second->dProductWeightedStdevAdjustment << ","
                          << itr->second->dLastQuoteMid << ","
                          << itr->second->dLastSignalMid << "\n";
        }
    }

    if(!bCurrentDayWritten)
    {
        if(_pQuoteInstrument->dgetEXMA() > 0.001 && _pSignalInstrument->dgetEXMA() > 0.001 && bRemoveToday == false)
        {
            cStringStream << _sTodayDate << ","
                          << _pQuoteInstrument->dgetEXMA() << ","
                          << _pQuoteInstrument->igetEXMANumDataPoints() << ","
                          << _pQuoteInstrument->dgetWeightedStdevEXMA() << ","
                          << _pQuoteInstrument->dgetWeightedStdevSqrdEXMA() << ","
                          << _pQuoteInstrument->igetWeightedStdevNumDataPoints() << ","
                          << _pQuoteInstrument->dgetWeightedStdevAdjustment() << ","
                          << _pSignalInstrument->dgetEXMA() << ","
                          << _pSignalInstrument->igetEXMANumDataPoints() << ","
                          << _pSignalInstrument->dgetWeightedStdevEXMA() << ","
                          << _pSignalInstrument->dgetWeightedStdevSqrdEXMA() << ","
                          << _pSignalInstrument->igetWeightedStdevNumDataPoints() << ","
                          << _pSignalInstrument->dgetWeightedStdevAdjustment() << ","
                          << _pProductInstrument->dgetWeightedStdevEXMA() << ","
                          << _pProductInstrument->dgetWeightedStdevSqrdEXMA() << ","
                          << _pProductInstrument->igetWeightedStdevNumDataPoints() << ","
                          << _pProductInstrument->dgetWeightedStdevAdjustment() << ","
                          << _dLastQuoteMid << ","
                          << _dLastSignalMid << "\n";
        }

        bCurrentDayWritten = true;
    }

	fstream ofsOvernightStatFile;

    const unsigned long length = 3000000;
    char buffer[length];
    ofsOvernightStatFile.rdbuf()->pubsetbuf(buffer, length);

	ofsOvernightStatFile.open(sOvernightStatFileName.c_str(), fstream::out);
	if(ofsOvernightStatFile.is_open())
	{
		ofsOvernightStatFile.precision(20);
        ofsOvernightStatFile << cStringStream.str();
		ofsOvernightStatFile.close();
	}
    else
    {
        stringstream cStringStream1;
        cStringStream1 << "Failed to update overnight stat file " << sOvernightStatFileName << ".";
        ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream1.str());

        _cLogger << "Failed to update overnight stat file " << sOvernightStatFileName << "\n";
    }

	boost::shared_ptr<DailyStat> pTodayStat (new DailyStat);
	pTodayStat->dQuoteInstrumentEXMA = _pQuoteInstrument->dgetEXMA();
    pTodayStat->iQuoteInstrumentEXMANumDataPoints = _pQuoteInstrument->igetEXMANumDataPoints();
	pTodayStat->dQuoteInstrumentWeightedStdevEXMA = _pQuoteInstrument->dgetWeightedStdevEXMA();
	pTodayStat->dQuoteInstrumentWeightedStdevSqrdEXMA = _pQuoteInstrument->dgetWeightedStdevSqrdEXMA();
    pTodayStat->iQuoteInstrumentWeightedStdevNumDataPoints = _pQuoteInstrument->igetWeightedStdevNumDataPoints();
	pTodayStat->dQuoteInstrumentWeightedStdevAdjustment  = _pQuoteInstrument->dgetWeightedStdevAdjustment();
	pTodayStat->dSignalInstrumentEXMA = _pSignalInstrument->dgetEXMA();
    pTodayStat->iSignalInstrumentEXMANumDataPoints = _pSignalInstrument->igetEXMANumDataPoints();
	pTodayStat->dSignalInstrumentWeightedStdevEXMA = _pSignalInstrument->dgetWeightedStdevEXMA();
	pTodayStat->dSignalInstrumentWeightedStdevSqrdEXMA = _pSignalInstrument->dgetWeightedStdevSqrdEXMA();
    pTodayStat->iSignalInstrumentWeightedStdevNumDataPoints = _pSignalInstrument->igetWeightedStdevNumDataPoints();
	pTodayStat->dSignalInstrumentWeightedStdevAdjustment = _pSignalInstrument->dgetWeightedStdevAdjustment();
	pTodayStat->dProductWeightedStdevEXMA = _pProductInstrument->dgetWeightedStdevEXMA();
	pTodayStat->dProductWeightedStdevSqrdEXMA = _pProductInstrument->dgetWeightedStdevSqrdEXMA();
    pTodayStat->iProductWeightedStdevNumDataPoints = _pProductInstrument->igetWeightedStdevNumDataPoints();
	pTodayStat->dProductWeightedStdevAdjustment = _pProductInstrument->dgetWeightedStdevAdjustment();
    pTodayStat->dLastQuoteMid = _dLastQuoteMid;
    pTodayStat->dLastSignalMid = _dLastSignalMid;

	_mDailyStats[cTodayDate] = pTodayStat;
}

void SLSL::loadOvernightStats()
{
	fstream ifsOvernightStatFile;
    string sOvernightStatFileName = _sEngineRunTimePath + "overnightstats.cfg";
	ifsOvernightStatFile.open(sOvernightStatFileName.c_str(), fstream::in);
	if(ifsOvernightStatFile.is_open())
	{
		while(!ifsOvernightStatFile.eof())
		{
			char sNewLine[512];
			ifsOvernightStatFile.getline(sNewLine, sizeof(sNewLine));

			if(strlen(sNewLine) != 0 && sNewLine[0] != '#')
			{
				boost::shared_ptr<DailyStat> pDailyStat (new DailyStat);

				std::istringstream cDailyStatStream(sNewLine);
				string sElement;
                boost::gregorian::date cDate;

                bool bDateIsValid = false;

				std::getline(cDailyStatStream, sElement, ',');

                try
                {
                    cDate = boost::gregorian::from_undelimited_string(sElement);
                    bDateIsValid = true;
                }
                catch (...)
                {
                    stringstream cStringStream;
                    cStringStream << "Invalid date in overnightstat.cfg: " << sElement << ".";
                    ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
                    bDateIsValid = false;
                }

                if(bDateIsValid == true)
                {
                    bool bStatValid = true;

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dQuoteInstrumentEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->iQuoteInstrumentEXMANumDataPoints = stol(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dQuoteInstrumentWeightedStdevEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dQuoteInstrumentWeightedStdevSqrdEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->iQuoteInstrumentWeightedStdevNumDataPoints = stol(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dQuoteInstrumentWeightedStdevAdjustment = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dSignalInstrumentEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->iSignalInstrumentEXMANumDataPoints = stol(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dSignalInstrumentWeightedStdevEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dSignalInstrumentWeightedStdevSqrdEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->iSignalInstrumentWeightedStdevNumDataPoints = stol(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dSignalInstrumentWeightedStdevAdjustment = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dProductWeightedStdevEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dProductWeightedStdevSqrdEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->iProductWeightedStdevNumDataPoints = stol(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dProductWeightedStdevAdjustment = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dLastQuoteMid = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dLastSignalMid = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    if(bStatValid)
                    {
                        _mDailyStats[cDate] = pDailyStat;
                        if(_bUpdateStats == false)
                        {
                            if(cDate == boost::gregorian::from_undelimited_string(_sTodayDate))
                            {
                                break;
                            }
                        }
                    }
                    else
                    {
                        stringstream cStringStream;
                        cStringStream << "Invalid overnight stat entry for date " << to_simple_string(cDate) << ".";
                    }
                }
			}
		}
	}

    boost::gregorian::date cStatDay;
    boost::gregorian::date cTodayDate = boost::gregorian::from_undelimited_string(_sTodayDate);

    if(_pScheduler->bisLiveTrading())
    {
        cStatDay = cTodayDate;
    }
    else
    {
        if(cTodayDate.day_of_week().as_number() != 1)
        {
            cStatDay = cTodayDate - boost::gregorian::date_duration(1);
        }
        else
        {
            cStatDay = cTodayDate - boost::gregorian::date_duration(3);
        }
    }

	map< boost::gregorian::date, boost::shared_ptr<DailyStat> >::iterator itr = _mDailyStats.end();
	for(int daysToLookback = 0; daysToLookback < 10; daysToLookback++)
	{
		itr = _mDailyStats.find(cStatDay);

		_cLogger << "Trying to load overnight stat from date " << to_iso_string(cStatDay) << "\n";
		if(itr != _mDailyStats.end())
		{
			break;
		}
		else
		{
			_cLogger << "Cannot found overnigth stat from date " << to_iso_string(cStatDay) << "\n";
		}

		int iTodayWeek = cStatDay.day_of_week().as_number();
		if(iTodayWeek != 1)
		{
			cStatDay = cStatDay - boost::gregorian::date_duration(1);
		}
		else
		{
			cStatDay = cStatDay - boost::gregorian::date_duration(3);
		}
	}

	if(itr != _mDailyStats.end())
	{
		_cLogger << "Loading overnight stat from date " << to_iso_string(cStatDay) << "\n";
		_cLogger << "Assign dQuoteInstrumentEXMA: " << itr->second->dQuoteInstrumentEXMA << "\n";
        _cLogger << "with " << itr->second->iQuoteInstrumentEXMANumDataPoints << " data points " << "\n";
		_pQuoteInstrument->setNewEXMA(itr->second->dQuoteInstrumentEXMA, itr->second->iQuoteInstrumentEXMANumDataPoints);
		_cLogger << "Assign dQuoteInstrumentWeightedStdevSqrdEXMA: " << itr->second->dQuoteInstrumentWeightedStdevSqrdEXMA << "\n";
		_cLogger << "Assign dQuoteInstrumentWeightedStdevEXMA: " << itr->second->dQuoteInstrumentWeightedStdevEXMA << "\n";
        _cLogger << "with " << itr->second->iQuoteInstrumentWeightedStdevNumDataPoints << " data points \n";

		_pQuoteInstrument->dsetNewWeightedStdevEXMA(itr->second->dQuoteInstrumentWeightedStdevSqrdEXMA, itr->second->dQuoteInstrumentWeightedStdevEXMA, itr->second->iQuoteInstrumentWeightedStdevNumDataPoints);
		_cLogger << "Assign dQuoteInstrumentWeightedStdevAdjustment: " << itr->second->dQuoteInstrumentWeightedStdevAdjustment << "\n";
		_pQuoteInstrument->applyWeightedStdevAdjustment(itr->second->dQuoteInstrumentWeightedStdevAdjustment);

		_cLogger << "Assign dSignalInstrumentEXMA: " << itr->second->dSignalInstrumentEXMA << "\n";
        _cLogger << "with " << itr->second->iSignalInstrumentEXMANumDataPoints << " data points \n";
		_pSignalInstrument->setNewEXMA(itr->second->dSignalInstrumentEXMA, itr->second->iSignalInstrumentEXMANumDataPoints);
		_cLogger << "Assign dSignalInstrumentWeightedStdevSqrdEXMA: " << itr->second->dSignalInstrumentWeightedStdevSqrdEXMA << "\n";
		_cLogger << "Assign dSignalInstrumentWeightedStdevEXMA: " << itr->second->dSignalInstrumentWeightedStdevEXMA << "\n";
        _cLogger << "with " << itr->second->iSignalInstrumentWeightedStdevNumDataPoints << " data points \n";
		_pSignalInstrument->dsetNewWeightedStdevEXMA(itr->second->dSignalInstrumentWeightedStdevSqrdEXMA, itr->second->dSignalInstrumentWeightedStdevEXMA, itr->second->iSignalInstrumentWeightedStdevNumDataPoints);
		_cLogger << "Assign dSignalInstrumentWeightedStdevAdjustment: " << itr->second->dSignalInstrumentWeightedStdevAdjustment << "\n";
		_pSignalInstrument->applyWeightedStdevAdjustment(itr->second->dSignalInstrumentWeightedStdevAdjustment);

		_cLogger << "Assign dProductWeightedStdevSqrdEXMA: " << itr->second->dProductWeightedStdevSqrdEXMA << "\n";
		_cLogger << "Assign dProductWeightedStdevEXMA: " << itr->second->dProductWeightedStdevEXMA << "\n";
        _cLogger << "with " << itr->second->iProductWeightedStdevNumDataPoints << " data points \n";
		_pProductInstrument->dsetNewWeightedStdevEXMA(itr->second->dProductWeightedStdevSqrdEXMA, itr->second->dProductWeightedStdevEXMA, itr->second->iProductWeightedStdevNumDataPoints);
		_cLogger << "Assign dProductWeightedStdevAdjustment: " << itr->second->dProductWeightedStdevAdjustment << "\n";
		_pProductInstrument->applyWeightedStdevAdjustment(itr->second->dProductWeightedStdevAdjustment);

        _cLogger << "Assign dLastQuoteMid: " << itr->second->dLastQuoteMid << "\n";
        _dLastQuoteMid = itr->second->dLastQuoteMid;

        _cLogger << "Assign dLastSignalMid: " << itr->second->dLastSignalMid << "\n";
        _dLastSignalMid = itr->second->dLastSignalMid;
	}
}

}
