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
           PositionServerConnection* pPositionServerConnection)
:SLBase(sEngineRunTimePath, "SLSL", sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, pPositionServerConnection),
 _bSignalInstrumentStaled(false),
 _bProductPositiveCorrelation(true),
 _dLeaderWeight(1),
 _bUpdateStats(true)
{
    _pSignalInstrument = NULL;
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
    int iSpreadOption;
    is >> iSpreadOption;

    _bWriteLog = (iSpreadOption == 1);
    _bWriteSpreadLog = (iSpreadOption == 2);

    is >> _bIsSegVol;

	is >> _iVolLength;

    _iProductVolLength = _iVolLength;

	is >> _dDriftLength;

	is >> _dEntryStd;
	is >> _dExitStd;

	is >> _iQuoteQty;

    string sQuotingStartTime = "";
    string sQuotingEndTime = "";

    is >> sQuotingStartTime;

    is >> sQuotingEndTime;

    if(sQuotingStartTime == "")
    {
        _cQuotingStartTime = _cTradingStartTime;
    }
    else
    {
        string sHour = sQuotingStartTime.substr (0,2);
        string sMinute = sQuotingStartTime.substr (2,2);
        string sSecond = sQuotingStartTime.substr (4,2);

        _cQuotingStartTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(_sTodayDate, sHour, sMinute, sSecond);

        if(_cQuotingStartTime < _cTradingStartTime)
        {
            stringstream cStringStream;
            cStringStream << "Quoting start time " << _cQuotingStartTime.igetPrintable() << " smaller than trading start time " << _cTradingStartTime.igetPrintable() << "! Reset it to trading start time";
            ErrorHandler::GetInstance()->newWarningMsg("0", _sEngineSlotName, "ALL", cStringStream.str());        
            _cQuotingStartTime = _cTradingStartTime;
        }
    }

    _cPreviousCallTime = _cQuotingStartTime;

    if(sQuotingEndTime == "")
    {
        _cQuotingEndTime = _cTradingEndTime;
    }
    else
    {
        string sHour = sQuotingEndTime.substr (0,2);
        string sMinute = sQuotingEndTime.substr (2,2);
        string sSecond = sQuotingEndTime.substr (4,2);

        _cQuotingEndTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(_sTodayDate, sHour, sMinute, sSecond);

        if(_cQuotingEndTime > _cTradingEndTime)
        {
            stringstream cStringStream;
            cStringStream << "Quoting end time " << _cQuotingEndTime.igetPrintable() << " greater than trading end time " << _cTradingEndTime.igetPrintable() << "! Reset it to trading end time";
            ErrorHandler::GetInstance()->newWarningMsg("0", _sEngineSlotName, "ALL", cStringStream.str());
            _cQuotingEndTime = _cTradingEndTime;
        }
    }

    while(!is.eof())
    {
        string sParamNameValue;
        is >> sParamNameValue;
        std::istringstream cParamStream (sParamNameValue);

        string sParamName;
        string sParamValue;

        bool bParamParseResult = std::getline(cParamStream, sParamName, ':') && std::getline(cParamStream, sParamValue, ':');

        if(bParamParseResult)
        {
            if(sParamName.compare("ASC") == 0)
            {
                _bUseASC = true;

                string sASC = sParamValue;
                for(int i = 0; i < sASC.length(); i++)
                {
                    _vASCSettings.push_back(atoi(sASC.substr(i,1).c_str()));
                }
            }
            else if(sParamName.compare("RealSignalPrice") == 0)
            {
                if(sParamValue.compare("0") == 0)
                {
                    _bUseRealSignalPrice = false;
                }
                else
                {
                    _bUseRealSignalPrice = true;
                }
            }
            else if(sParamName.compare("IOC") == 0)
            {
                if(sParamValue.compare("0") == 0)
                {
                    _bIsHittingStrategy = false;
                }
                else
                {
                    _bIsHittingStrategy = true;
                }
            }
            else if(sParamName.compare("LeaderWeight") == 0)
            {
                _dLeaderWeight = atof(sParamValue.c_str());
            }
            else if(sParamName.compare("UpdateStats") == 0)
            {
                if(sParamValue.compare("0") == 0)
                {
                    _bUpdateStats = false;
                }   
                else
                {
                    _bUpdateStats = true;
                } 
            }
            else if(sParamName.compare("PositiveCorrelation") == 0)
            {
                if(sParamValue.compare("0") == 0)
                {
                    _bProductPositiveCorrelation = false;
                }
                else
                {
                    _bProductPositiveCorrelation = true;
                }
            }
            else if(sParamName.compare("PatientLiqLength") == 0)
            {
                _iPatLiqOffSet = atoi(sParamValue.c_str());
            }
            else if(sParamName.compare("ProductStdLength") == 0)
            {
                _iProductVolLength = atoi(sParamValue.c_str());
            }
        }
        else
        {
            if(sParamNameValue.compare("") != 0)
            {
                stringstream cStringStream;
                cStringStream << "Failed to parse paramater name and value pair " << sParamNameValue;
                ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, "ALL", cStringStream.str());
            }
        }
    }

    setupLiqTime();
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
        _pSignalInstrument->useWeightedStdev(_iStdevLength, false, KOEpochTime(0,0), 60, _cVolStartTime, _cVolEndTime);

        _pProductInstrument = new SyntheticSpread("Product",
                                  _pQuoteInstrument,
                                  SyntheticSpread::WhiteAbs,
                                  _pSignalInstrument,
                                  SyntheticSpread::WhiteAbs,
                                  _dLeaderWeight,
                                  _bProductPositiveCorrelation,
                                  _bUseRealSignalPrice);
        _pProductInstrument->useWeightedStdev(_iProductStdevLength, false, KOEpochTime(0,0), 60, _cVolStartTime, _cVolEndTime);

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
        _cLogger << "Product stat is valid: " << _pProductInstrument->dgetWeightedStdev() << "\n"
                 << "EntryThreshold " << _pProductInstrument->dgetWeightedStdev() * _dEntryStd << " "
                 << "ExitThreshold " << _pProductInstrument->dgetWeightedStdev() * _dExitStd << "\n";
//      _cLogger << "_pQuoteInstrument->dgetWeightedStdevEXMA() " << _pQuoteInstrument->dgetWeightedStdevEXMA() << "\n";
//      _cLogger << "_pQuoteInstrument->dgetWeightedStdevSqrdEXMA() " << _pQuoteInstrument->dgetWeightedStdevSqrdEXMA() << "\n";
    }

    _cLogger << "Daily strategy initialisation finished" << std::endl;		
}

void SLSL::dayRun()
{
    _cLogger << "Daily strategy run " << std::endl;

    SLBase::dayRun();

    _cLogger << "Daily strategy run finished " << std::endl;
}

void SLSL::dayStop()
{
    _cLogger << "Daily strategy stop " << std::endl;		

    SLBase::dayStop();
    _pFairValueExecution->setExecutionState(FairValueExecution::Off);

    loadRollDelta();

    if(_bUpdateStats == true)
    {
        saveOvernightStats();
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
_cLogger << "SIGNAL|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << vContractQuoteDatas[iUpdateIndex]->iBidSize
																						         << "|" << vContractQuoteDatas[iUpdateIndex]->dBestBid
																						         << "|" << vContractQuoteDatas[iUpdateIndex]->dBestAsk
																						         << "|" << vContractQuoteDatas[iUpdateIndex]->iAskSize << "\n";
*/
		// Singal instrument updated
		_pSignalInstrument->newMarketUpdate(vContractQuoteDatas[iUpdateIndex]);
		_iTotalSignalInstruUpdate = _iTotalSignalInstruUpdate + 1;
/*
_cLogger << "SIGNAL|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << _pSignalInstrument->igetBidSize() 
																					             << "|" << _pSignalInstrument->igetBestBid() 
																					             << "|" << _pSignalInstrument->igetBestAsk() 
																					             << "|" << _pSignalInstrument->igetAskSize()
                                                                                                 << "|" << _pSignalInstrument->igetLastTrade()
                                                                                                 << "|" << _pSignalInstrument->igetAccumuTradeSize() << "\n";
*/
		if(bcheckAllProductsReady())
		{
            updateTheoPosition();
            calculateQuotePrice();
            _pFairValueExecution->newTheoQuotePrices(_iTheoBid, _iTheoOffer, _iTheoExitBid, _iTheoExitOffer, false);
            _pFairValueExecution->updateOrders();
		}
		else
		{
			_cLogger << "Ignore leader price update\n";
		}
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
            updateTheoPosition();
            calculateQuotePrice();

            _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() 
                     << "|" << "ST"
                     << "|" << _pQuoteInstrument->dgetWeightedMid()
                     << "|" << _pSignalInstrument->dgetWeightedMid()
                     << "|" << _pQuoteInstrument->dgetEXMA()
                     << "|" << _pSignalInstrument->dgetEXMA()
                     << "|" << _pQuoteInstrument->dgetWeightedStdev()
                     << "|" << _pSignalInstrument->dgetWeightedStdev()
                     << "|" << _dSpreadIndicator 
                     << "|" << _iTheoBid
                     << "|" << _iTheoOffer
                     << "|" << _iTheoExitBid
                     << "|" << _iTheoExitOffer
                     << "|" << _iTheoPosition
                     << "|" << _iPosition << std::endl;

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

void SLSL::saveOvernightStats()
{
    stringstream cStringStream;
    cStringStream.precision(20);

    string sOvernightStatFileName = _sEngineRunTimePath + "OvernightStat.cfg";

    boost::gregorian::date cTodayDate = boost::gregorian::from_undelimited_string(_sTodayDate);

    bool bCurrentDayWritten = false;

    for(map< boost::gregorian::date, boost::shared_ptr<DailyStat> >::iterator itr = _mDailyStats.begin(); itr != _mDailyStats.end(); ++itr)
    {
        if(itr->first != cTodayDate)
        {
            if(itr->first > cTodayDate && !bCurrentDayWritten)
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
                              << _pProductInstrument->dgetWeightedStdevAdjustment() << "\n";

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
                          << itr->second->dProductWeightedStdevAdjustment << "\n";
        }
    }

    if(!bCurrentDayWritten)
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
                      << _pProductInstrument->dgetWeightedStdevAdjustment() << "\n";

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
        stringstream cStringStream;
        cStringStream << "Failed to update overnight stat file " << sOvernightStatFileName << ".";
        ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());

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

	_mDailyStats[cTodayDate] = pTodayStat;
}

void SLSL::loadOvernightStats()
{
	fstream ifsOvernightStatFile;
    string sOvernightStatFileName = _sEngineRunTimePath + "OvernightStat.cfg";
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
                    cStringStream << "Invalid date in OvernightStat.cfg: " << sElement << ".";
                    ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
                    bDateIsValid = false;
                }

                if(bDateIsValid == true)
                {
                    bool bStatValid = true;

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dQuoteInstrumentEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->iQuoteInstrumentEXMANumDataPoints = atoi(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dQuoteInstrumentWeightedStdevEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dQuoteInstrumentWeightedStdevSqrdEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->iQuoteInstrumentWeightedStdevNumDataPoints = atoi(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dQuoteInstrumentWeightedStdevAdjustment = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dSignalInstrumentEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->iSignalInstrumentEXMANumDataPoints = atoi(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dSignalInstrumentWeightedStdevEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dSignalInstrumentWeightedStdevSqrdEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->iSignalInstrumentWeightedStdevNumDataPoints = atoi(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dSignalInstrumentWeightedStdevAdjustment = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dProductWeightedStdevEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dProductWeightedStdevSqrdEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->iProductWeightedStdevNumDataPoints = atoi(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dProductWeightedStdevAdjustment = atof(sElement.c_str());

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
	}
}

}
