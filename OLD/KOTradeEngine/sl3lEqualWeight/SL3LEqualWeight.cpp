#include <stdlib.h>
#include <boost/math/special_functions/round.hpp>

#include "SL3LEqualWeight.h"
#include "../EngineInfra/ErrorHandler.h"
#include "../EngineInfra/SystemClock.h"

using namespace boost::posix_time;
using std::stringstream;

namespace KO
{

SL3LEqualWeight::SL3LEqualWeight(const string& sEngineRunTimePath,
           const string& sEngineSlotName,
           KOEpochTime cTradingStartTime,
           KOEpochTime cTradingEndTime,
           SchedulerBase* pScheduler,
           string sTodayDate,
           PositionServerConnection* pPositionServerConnection)
:SLBase(sEngineRunTimePath, "SL3LEqualWeight", sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, pPositionServerConnection),
 _bSpreadFrontInstrumentStaled(false),
 _bSpreadBackInstrumentStaled(false),
 _bSpreadPositiveCorrelation(true),
 _bProductPostiveCorrelation(true),
 _dLeaderWeight(1),
 _bUpdateStats(true)
{
    _pSpreadFrontInstrument = NULL;
    _pSpreadBackInstrument = NULL;
    _pSpreadInstrument = NULL;
}

SL3LEqualWeight::~SL3LEqualWeight()
{
    if(_pSpreadFrontInstrument != NULL)
    {
        delete _pSpreadFrontInstrument;
    }

    if(_pSpreadBackInstrument != NULL)
    {
        delete _pSpreadBackInstrument;
    }

    if(_pSpreadInstrument != NULL)
    {
        delete _pSpreadInstrument;
    }
}

void SL3LEqualWeight::readFromStream(istream& is)
{
    int iSpreadOption;

	is >> iSpreadOption;

    _bWriteLog = (iSpreadOption == 1);
    _bWriteSpreadLog = (iSpreadOption == 2);

    is >> _bIsSegVol;

	is >> _bProductPostiveCorrelation;

	is >> _bSpreadPositiveCorrelation;

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

void SL3LEqualWeight::dayInit()
{
    setupLogger("SL3LEqualWeight");

    _cLogger << "Daily Strategy initialisation \n";

    SLBase::dayInit();

    _iTotalSpreadFrontInstruUpdate = 0;
    _iTotalSpreadBackInstruUpdate = 0;

    _cLogger << "Engine quoting start time " << _cQuotingStartTime.igetPrintable() << "\n";
    _cLogger << "Engine quoting end time " << _cQuotingEndTime.igetPrintable() << "\n";

    if(_pSpreadFrontInstrument == NULL)
    {
        _pSpreadFrontInstrument = new Instrument(vContractQuoteDatas[1]->sProduct, vContractQuoteDatas[1]->iCID, vContractQuoteDatas[1]->eInstrumentType, vContractQuoteDatas[1]->dTickSize, vContractQuoteDatas[1]->iMaxSpreadWidth, _bUseRealSignalPrice);
        _pSpreadFrontInstrument->useEXMA(_dDriftLength, false, KOEpochTime(0,0) , 1, _cTradingStartTime, _cTradingEndTime);
        _pSpreadFrontInstrument->useWeightedStdev(_iStdevLength, false, KOEpochTime(0,0), 60, _cVolStartTime, _cVolEndTime);

        _pSpreadBackInstrument = new Instrument(vContractQuoteDatas[2]->sProduct, vContractQuoteDatas[2]->iCID, vContractQuoteDatas[2]->eInstrumentType, vContractQuoteDatas[2]->dTickSize, vContractQuoteDatas[1]->iMaxSpreadWidth, _bUseRealSignalPrice);
        _pSpreadBackInstrument->useEXMA(_dDriftLength, false, KOEpochTime(0,0) , 1, _cTradingStartTime, _cTradingEndTime);
        _pSpreadBackInstrument->useWeightedStdev(_iStdevLength, false, KOEpochTime(0,0), 60, _cVolStartTime, _cVolEndTime);

        _pSpreadInstrument = new SyntheticSpread((vContractQuoteDatas[1]->sProduct + "-" + vContractQuoteDatas[2]->sProduct),
                                 _pSpreadFrontInstrument,
                                 SyntheticSpread::WhiteAbs,
                                 _pSpreadBackInstrument,
                                 SyntheticSpread::WhiteAbs,
                                 1,
                                 _bSpreadPositiveCorrelation,
                                 _bUseRealSignalPrice);
        _pSpreadInstrument->useWeightedStdev(_iStdevLength, false, KOEpochTime(0,0), 60, _cVolStartTime, _cVolEndTime);

        _pProductInstrument = new SyntheticSpread("Product",
                                  _pQuoteInstrument,
                                  SyntheticSpread::WhiteAbs,
                                  _pSpreadInstrument,
                                  SyntheticSpread::Simple,
                                  _dLeaderWeight,
                                  _bProductPostiveCorrelation,
                                  _bUseRealSignalPrice);
        _pProductInstrument->useWeightedStdev(_iProductStdevLength, false, KOEpochTime(0,0), 60, _cVolStartTime, _cVolEndTime);

        loadOvernightStats();
    }

    if(_pQuoteInstrument->bgetWeightedStdevValid())
    {
        _cLogger << "Quote stat is valid \n";
//		_cLogger << "Quote stat is valid: " << _pQuoteInstrument->dgetWeightedStdev() << "\n";
//		_cLogger << "_pQuoteInstrument->dgetWeightedStdevEXMA() " << _pQuoteInstrument->dgetWeightedStdevEXMA() << "\n";
//		_cLogger << "_pQuoteInstrument->dgetWeightedStdevSqrdEXMA() " << _pQuoteInstrument->dgetWeightedStdevSqrdEXMA() << "\n";
    }

    if(_pSpreadInstrument->bgetWeightedStdevValid())
    {
        _cLogger << "Spread stat is valid \n";
//		_cLogger << "Spread stat is valid: " << _pSpreadInstrument->dgetWeightedStdev() << "\n";
//		_cLogger << "_pQuoteInstrument->dgetWeightedStdevEXMA() " << _pQuoteInstrument->dgetWeightedStdevEXMA() << "\n";
//		_cLogger << "_pQuoteInstrument->dgetWeightedStdevSqrdEXMA() " << _pQuoteInstrument->dgetWeightedStdevSqrdEXMA() << "\n";
    }

    if(_pProductInstrument->bgetWeightedStdevValid())
    {
        _cLogger << "Product stat is valid \n";
		_cLogger << "Product stat is valid: " << _pProductInstrument->dgetWeightedStdev() << "\n"
        		 << "EntryThreshold " << _pProductInstrument->dgetWeightedStdev() * _dEntryStd << " "
				 << "ExitThreshold " << _pProductInstrument->dgetWeightedStdev() * _dExitStd << "\n";
//		_cLogger << "_pQuoteInstrument->dgetWeightedStdevEXMA() " << _pQuoteInstrument->dgetWeightedStdevEXMA() << "\n";
//		_cLogger << "_pQuoteInstrument->dgetWeightedStdevSqrdEXMA() " << _pQuoteInstrument->dgetWeightedStdevSqrdEXMA() << "\n";
    }

    _cLogger << "Daily strategy initialisation finished" << std::endl;
}

void SL3LEqualWeight::dayRun()
{
    _cLogger << "Daily strategy run " << std::endl;

    SLBase::dayRun();

    _cLogger << "Daily strategy run finished " << std::endl;
}

void SL3LEqualWeight::dayStop()
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
    _pSpreadFrontInstrument->eodReset();
    _pSpreadBackInstrument->eodReset();

    _cLogger << "Daily strategy stop finished " << std::endl;

    _cLogger.closeFile();
    _cSpreadLogger.closeFile();
}

void SL3LEqualWeight::receive(int iCID)
{
    SLBase::receive(iCID);

	int iUpdateIndex = -1;

    for(unsigned int i = 0;i < vContractQuoteDatas.size(); i++)
    {
        if(vContractQuoteDatas[i]->iCID == iCID)
        {
            iUpdateIndex = i;
            break;
        }
    }

	if(iUpdateIndex == 1)
	{
/*
_cLogger << "SPREAD FRONT|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << vContractQuoteDatas[iUpdateIndex]->iBidSize
                                                                                                       << "|" << vContractQuoteDatas[iUpdateIndex]->dBestBid
                                                                                                       << "|" << vContractQuoteDatas[iUpdateIndex]->dBestAsk
                                                                                                       << "|" << vContractQuoteDatas[iUpdateIndex]->iAskSize << "\n";
*/
		// Spread Front instrument updated
		_pSpreadFrontInstrument->newMarketUpdate(vContractQuoteDatas[iUpdateIndex]);
		_iTotalSpreadFrontInstruUpdate = _iTotalSpreadFrontInstruUpdate + 1;
/*
_cLogger << "SPREAD FRONT|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << _pSpreadFrontInstrument->igetBidSize() 
        														          						       << "|" << _pSpreadFrontInstrument->igetBestBid() 
		        																			           << "|" << _pSpreadFrontInstrument->igetBestAsk() 
				        																	           << "|" << _pSpreadFrontInstrument->igetAskSize()
                                                                                                       << "|" << _pSpreadFrontInstrument->igetLastTrade()
                                                                                                       << "|" << _pSpreadFrontInstrument->igetAccumuTradeSize()<< "\n";
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
			_cLogger << "Ignore spread front price update\n";
		}
	}
	else if(iUpdateIndex == 2)
	{
/*
_cLogger << "SPREAD BACK|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << vContractQuoteDatas[iUpdateIndex]->iBidSize
                                                                                                      << "|" << vContractQuoteDatas[iUpdateIndex]->dBestBid
                                                                                                      << "|" << vContractQuoteDatas[iUpdateIndex]->dBestAsk        
                                                                                                      << "|" << vContractQuoteDatas[iUpdateIndex]->iAskSize << "\n";
*/
		// Spread Back instrument updated
		_pSpreadBackInstrument->newMarketUpdate(vContractQuoteDatas[iUpdateIndex]);
		_iTotalSpreadBackInstruUpdate = _iTotalSpreadBackInstruUpdate + 1;
/*
_cLogger << "SPREAD BACK|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << _pSpreadBackInstrument->igetBidSize() 
           														          					          << "|" << _pSpreadBackInstrument->igetBestBid() 
																					                  << "|" << _pSpreadBackInstrument->igetBestAsk() 
																					                  << "|" << _pSpreadBackInstrument->igetAskSize()
                                                                                                      << "|" << _pSpreadBackInstrument->igetLastTrade()
                                                                                                      << "|" << _pSpreadBackInstrument->igetAccumuTradeSize()<< "\n";
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
			_cLogger << "Ignore spread back price update\n";
		}
	}
}

void SL3LEqualWeight::updateStatistics(KOEpochTime cCallTime)
{
    _bTradingStatsValid = false;

    _pQuoteInstrument->wakeup(cCallTime);
    _pSpreadFrontInstrument->wakeup(cCallTime);
    _pSpreadBackInstrument->wakeup(cCallTime);

    if(_pSpreadFrontInstrument->bgetEXMAValid() &&
       _pSpreadFrontInstrument->bgetWeightedStdevValid() &&	
       _pSpreadBackInstrument->bgetEXMAValid() &&
       _pSpreadBackInstrument->bgetWeightedStdevValid())
    {
        _pSpreadInstrument->wakeup(cCallTime);
    }	

    if(_pQuoteInstrument->bgetEXMAValid() &&
       _pQuoteInstrument->bgetWeightedStdevValid() && 
       _pSpreadInstrument->bgetWeightedStdevValid())
    {
        _pProductInstrument->wakeup(cCallTime);		

        _dSpreadIndicator = _pProductInstrument->dgetWeightedMid();

        if(_pProductInstrument->bgetWeightedStdevValid())
        {
            updateTheoPosition();
            calculateQuotePrice();            

            _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();
            _cLogger << "|ST";
            _cLogger << "|" << _pQuoteInstrument->dgetWeightedMid();
            _cLogger << "|" << _pSpreadFrontInstrument->dgetWeightedMid();
            _cLogger << "|" << _pSpreadBackInstrument->dgetWeightedMid();
            _cLogger << "|" << _pQuoteInstrument->dgetEXMA();
            _cLogger << "|" << _pSpreadFrontInstrument->dgetEXMA();
            _cLogger << "|" << _pSpreadBackInstrument->dgetEXMA();
            _cLogger << "|" << _pQuoteInstrument->dgetWeightedStdev();
            _cLogger << "|" << _pSpreadFrontInstrument->dgetWeightedStdev();
            _cLogger << "|" << _pSpreadBackInstrument->dgetWeightedStdev();
            _cLogger << "|" << _dSpreadIndicator
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
        _cLogger << "Ignore wake up call. Instrument statistic not valid. numDataPoint is " << _pQuoteInstrument->igetWeightedStdevNumDataPoints() << std::endl;
    }
}

void SL3LEqualWeight::writeSpreadLog()
{
    if(_pQuoteInstrument != NULL && _pSpreadFrontInstrument != NULL && _pSpreadBackInstrument != NULL && _pProductInstrument != NULL)
    {
        if(_pQuoteInstrument->igetBidSize() != 0 &&
           _pQuoteInstrument->igetAskSize() != 0 &&
           _pSpreadFrontInstrument->igetBidSize() != 0 &&
           _pSpreadFrontInstrument->igetAskSize() != 0 &&
           _pSpreadBackInstrument->igetBidSize() != 0 &&
           _pSpreadBackInstrument->igetAskSize() != 0)
        {
            _cSpreadLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable()
                           << "|" << _pProductInstrument->dgetFrontLegPrice()
                           << "|" << _pProductInstrument->dgetBackLegPrice()
                           << "|" << _dLeaderWeight << "\n";
        }
    }
}

bool SL3LEqualWeight::bcheckAllProductsReady()
{
   bool bResult = false;

    if(_pQuoteInstrument != NULL &&
       _pSpreadFrontInstrument != NULL &&
       _pSpreadBackInstrument != NULL &&
       _pSpreadInstrument != NULL &&
       _pProductInstrument != NULL)
    {
        if(_pQuoteInstrument->bgetPriceValid() && _pSpreadFrontInstrument->bgetPriceValid() && _pSpreadBackInstrument->bgetPriceValid())
        {
            bResult = true;
        }
        else
        {
            _cLogger << "Instrument price not valid" << std::endl;
        }
    }
    else
    {
        _cLogger << "Instrument object not ready" << std::endl;
    }

    return bResult;
}

void SL3LEqualWeight::wakeup(KOEpochTime cCallTime)
{
    SLBase::wakeup(cCallTime);
}

void SL3LEqualWeight::loadRollDelta()
{
    SLBase::loadRollDelta();

    map< pair<string, string>, double >::iterator itr;

    itr = _mDailyRolls.find(pair<string, string>(_sTodayDate, vContractQuoteDatas[1]->sExchange + "." + vContractQuoteDatas[1]->sProduct));
    if(itr != _mDailyRolls.end())
    {
        double dSpreadFrontRollDelta = itr->second;

        if(!_bUseRealSignalPrice)
        {
            long iSpreadFrontRollDeltaInTicks = boost::math::iround(dSpreadFrontRollDelta / _pSpreadFrontInstrument->dgetTickSize());

            if(_pSpreadFrontInstrument->sgetProductName().substr(0,2).compare("6E") == 0)
            {
                if(_pSpreadFrontInstrument->dgetTickSize() < 0.0001 - 0.000000002)
                {
                    iSpreadFrontRollDeltaInTicks = iSpreadFrontRollDeltaInTicks / 2;
                }
            }
     
            if(_pSpreadFrontInstrument->sgetProductName().substr(0,1).compare("L") == 0)
            {
                if(_pSpreadFrontInstrument->dgetTickSize() < 0.01 - 0.002)
                {
                    iSpreadFrontRollDeltaInTicks = iSpreadFrontRollDeltaInTicks / 2;
                }
            }

            _pSpreadFrontInstrument->applyEXMAAdjustment(iSpreadFrontRollDeltaInTicks);
            _pSpreadFrontInstrument->applyWeightedStdevAdjustment(iSpreadFrontRollDeltaInTicks);
            _cLogger << "Applying Spread Front Instrument Roll Delta " << iSpreadFrontRollDeltaInTicks << "\n";
        }
        else
        {
            _pSpreadFrontInstrument->applyEXMAAdjustment(dSpreadFrontRollDelta);
            _pSpreadFrontInstrument->applyWeightedStdevAdjustment(dSpreadFrontRollDelta);
            _cLogger << "Applying Spread Front Instrument Roll Delta " << dSpreadFrontRollDelta << "\n";
        }
    }

    itr = _mDailyRolls.find(pair<string, string>(_sTodayDate, vContractQuoteDatas[2]->sExchange + "." + vContractQuoteDatas[2]->sProduct));
    if(itr != _mDailyRolls.end())
    {
        double dSpreadBackRollDelta = itr->second;

        if(!_bUseRealSignalPrice)
        {
            long iSpreadBackRollDeltaInTicks = boost::math::iround(dSpreadBackRollDelta / _pSpreadBackInstrument->dgetTickSize());

            if(_pSpreadBackInstrument->sgetProductName().substr(0,2).compare("6E") == 0)
            {
                if(_pSpreadBackInstrument->dgetTickSize() < 0.0001 - 0.000000002)
                {
                    iSpreadBackRollDeltaInTicks = iSpreadBackRollDeltaInTicks / 2;
                }
            }

            if(_pSpreadFrontInstrument->sgetProductName().substr(0,1).compare("L") == 0)
            {
                if(_pSpreadBackInstrument->dgetTickSize() < 0.01 - 0.002)
                {
                    iSpreadBackRollDeltaInTicks = iSpreadBackRollDeltaInTicks / 2;
                }
            }

            _pSpreadBackInstrument->applyEXMAAdjustment(iSpreadBackRollDeltaInTicks);
            _pSpreadBackInstrument->applyWeightedStdevAdjustment(iSpreadBackRollDeltaInTicks);
            _cLogger << "Applying Spread Back Instrument Roll Delta " << iSpreadBackRollDeltaInTicks << "\n";
        }
        else
        {
            _pSpreadBackInstrument->applyEXMAAdjustment(dSpreadBackRollDelta);
            _pSpreadBackInstrument->applyWeightedStdevAdjustment(dSpreadBackRollDelta);
            _cLogger << "Applying Spread Back Instrument Roll Delta " << dSpreadBackRollDelta << "\n";
        }
    }
}

void SL3LEqualWeight::saveOvernightStats()
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
                              << _pSpreadFrontInstrument->dgetEXMA() << ","
                              << _pSpreadFrontInstrument->igetEXMANumDataPoints() << ","
                              << _pSpreadFrontInstrument->dgetWeightedStdevEXMA() << ","
                              << _pSpreadFrontInstrument->dgetWeightedStdevSqrdEXMA() << ","
                              << _pSpreadFrontInstrument->igetWeightedStdevNumDataPoints() << ","
                              << _pSpreadFrontInstrument->dgetWeightedStdevAdjustment() << ","
                              << _pSpreadBackInstrument->dgetEXMA() << ","
                              << _pSpreadBackInstrument->igetEXMANumDataPoints() << ","
                              << _pSpreadBackInstrument->dgetWeightedStdevEXMA() << ","
                              << _pSpreadBackInstrument->dgetWeightedStdevSqrdEXMA() << ","
                              << _pSpreadBackInstrument->igetWeightedStdevNumDataPoints() << ","
                              << _pSpreadBackInstrument->dgetWeightedStdevAdjustment() << ","
                              << _pSpreadInstrument->dgetWeightedStdevEXMA() << ","
                              << _pSpreadInstrument->dgetWeightedStdevSqrdEXMA() << ","
                              << _pSpreadInstrument->igetWeightedStdevNumDataPoints() << ","
                              << _pSpreadInstrument->dgetWeightedStdevAdjustment() << ","
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
                          << itr->second->dSpreadFrontInstrumentEXMA << ","
                          << itr->second->iSpreadFrontInstrumentEXMANumDataPoints << ","
                          << itr->second->dSpreadFrontInstrumentWeightedStdevEXMA << ","
                          << itr->second->dSpreadFrontInstrumentWeightedStdevSqrdEXMA << ","
                          << itr->second->iSpreadFrontInstrumentWeightedStdevNumDataPoints << ","
                          << itr->second->dSpreadFrontInstrumentWeightedStdevAdjustment << ","
                          << itr->second->dSpreadBackInstrumentEXMA << ","
                          << itr->second->iSpreadBackInstrumentEXMANumDataPoints << ","
                          << itr->second->dSpreadBackInstrumentWeightedStdevEXMA << ","
                          << itr->second->dSpreadBackInstrumentWeightedStdevSqrdEXMA << ","
                          << itr->second->iSpreadBackInstrumentWeightedStdevNumDataPoints << ","
                          << itr->second->dSpreadBackInstrumentWeightedStdevAdjustment << ","
                          << itr->second->dSpreadInstrumentWeightedStdevEXMA << ","
                          << itr->second->dSpreadInstrumentWeightedStdevSqrdEXMA << ","
                          << itr->second->iSpreadInstrumentWeightedStdevNumDataPoints << ","
                          << itr->second->dSpreadInstrumentWeightedStdevAdjustment << ","
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
                      << _pSpreadFrontInstrument->dgetEXMA() << ","
                      << _pSpreadFrontInstrument->igetEXMANumDataPoints() << ","
                      << _pSpreadFrontInstrument->dgetWeightedStdevEXMA() << ","
                      << _pSpreadFrontInstrument->dgetWeightedStdevSqrdEXMA() << ","
                      << _pSpreadFrontInstrument->igetWeightedStdevNumDataPoints() << ","
                      << _pSpreadFrontInstrument->dgetWeightedStdevAdjustment() << ","
                      << _pSpreadBackInstrument->dgetEXMA() << ","
                      << _pSpreadBackInstrument->igetEXMANumDataPoints() << ","
                      << _pSpreadBackInstrument->dgetWeightedStdevEXMA() << ","
                      << _pSpreadBackInstrument->dgetWeightedStdevSqrdEXMA() << ","
                      << _pSpreadBackInstrument->igetWeightedStdevNumDataPoints() << ","
                      << _pSpreadBackInstrument->dgetWeightedStdevAdjustment() << ","
                      << _pSpreadInstrument->dgetWeightedStdevEXMA() << ","
                      << _pSpreadInstrument->dgetWeightedStdevSqrdEXMA() << ","
                      << _pSpreadInstrument->igetWeightedStdevNumDataPoints() << ","
                      << _pSpreadInstrument->dgetWeightedStdevAdjustment() << ","
                      << _pProductInstrument->dgetWeightedStdevEXMA() << ","
                      << _pProductInstrument->dgetWeightedStdevSqrdEXMA() << ","
                      << _pProductInstrument->igetWeightedStdevNumDataPoints() << ","
                      << _pProductInstrument->dgetWeightedStdevAdjustment() << "\n";

        bCurrentDayWritten = true;
    }

	fstream ofsOvernightStatFile;

    const unsigned long length = 5000000;
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
    pTodayStat->dSpreadFrontInstrumentEXMA = _pSpreadFrontInstrument->dgetEXMA();
    pTodayStat->iSpreadFrontInstrumentEXMANumDataPoints = _pSpreadFrontInstrument->igetEXMANumDataPoints();
    pTodayStat->dSpreadFrontInstrumentWeightedStdevEXMA = _pSpreadFrontInstrument->dgetWeightedStdevEXMA();
    pTodayStat->dSpreadFrontInstrumentWeightedStdevSqrdEXMA = _pSpreadFrontInstrument->dgetWeightedStdevSqrdEXMA();
    pTodayStat->iSpreadFrontInstrumentWeightedStdevNumDataPoints = _pSpreadFrontInstrument->igetWeightedStdevNumDataPoints();
    pTodayStat->dSpreadFrontInstrumentWeightedStdevAdjustment = _pSpreadFrontInstrument->dgetWeightedStdevAdjustment();
    pTodayStat->dSpreadBackInstrumentEXMA = _pSpreadBackInstrument->dgetEXMA();
    pTodayStat->iSpreadBackInstrumentEXMANumDataPoints = _pSpreadBackInstrument->igetEXMANumDataPoints();
    pTodayStat->dSpreadBackInstrumentWeightedStdevEXMA = _pSpreadBackInstrument->dgetWeightedStdevEXMA();
    pTodayStat->dSpreadBackInstrumentWeightedStdevSqrdEXMA = _pSpreadBackInstrument->dgetWeightedStdevSqrdEXMA();
    pTodayStat->iSpreadBackInstrumentWeightedStdevNumDataPoints = _pSpreadBackInstrument->igetWeightedStdevNumDataPoints();
    pTodayStat->dSpreadBackInstrumentWeightedStdevAdjustment = _pSpreadBackInstrument->dgetWeightedStdevAdjustment();
    pTodayStat->dSpreadInstrumentWeightedStdevEXMA = _pSpreadInstrument->dgetWeightedStdevEXMA();
    pTodayStat->dSpreadInstrumentWeightedStdevSqrdEXMA = _pSpreadInstrument->dgetWeightedStdevSqrdEXMA();
    pTodayStat->iSpreadInstrumentWeightedStdevNumDataPoints = _pSpreadInstrument->igetWeightedStdevNumDataPoints();
    pTodayStat->dSpreadInstrumentWeightedStdevAdjustment = _pSpreadInstrument->dgetWeightedStdevAdjustment();
    pTodayStat->dProductWeightedStdevEXMA = _pProductInstrument->dgetWeightedStdevEXMA();
    pTodayStat->dProductWeightedStdevSqrdEXMA = _pProductInstrument->dgetWeightedStdevSqrdEXMA();
    pTodayStat->iProductWeightedStdevNumDataPoints = _pProductInstrument->igetWeightedStdevNumDataPoints();
    pTodayStat->dProductWeightedStdevAdjustment = _pProductInstrument->dgetWeightedStdevAdjustment();

    _mDailyStats[cTodayDate] = pTodayStat;
}

void SL3LEqualWeight::loadOvernightStats()
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
                    cStringStream << "Error in slot " << _sEngineSlotName << " on date " << _sTodayDate << " invalid date in OvernightStat.cfg: " << sElement << ".";
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
                    pDailyStat->dSpreadFrontInstrumentEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->iSpreadFrontInstrumentEXMANumDataPoints = atoi(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dSpreadFrontInstrumentWeightedStdevEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dSpreadFrontInstrumentWeightedStdevSqrdEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->iSpreadFrontInstrumentWeightedStdevNumDataPoints = atoi(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dSpreadFrontInstrumentWeightedStdevAdjustment = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dSpreadBackInstrumentEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->iSpreadBackInstrumentEXMANumDataPoints = atoi(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dSpreadBackInstrumentWeightedStdevEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dSpreadBackInstrumentWeightedStdevSqrdEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->iSpreadBackInstrumentWeightedStdevNumDataPoints = atoi(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dSpreadBackInstrumentWeightedStdevAdjustment = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dSpreadInstrumentWeightedStdevEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dSpreadInstrumentWeightedStdevSqrdEXMA = atof(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->iSpreadInstrumentWeightedStdevNumDataPoints = atoi(sElement.c_str());

                    std::getline(cDailyStatStream, sElement, ',');
                    if(sElement.compare("") == 0)
                    {
                        bStatValid = bStatValid && false;
                    }
                    else
                    {
                        bStatValid = bStatValid && true;
                    }
                    pDailyStat->dSpreadInstrumentWeightedStdevAdjustment = atof(sElement.c_str());

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
                        cStringStream << "Error in slot " << _sEngineSlotName << ". Invalid overnight stat entry for date " << to_simple_string(cDate) << ".";
                        ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
                    }
                }
			}
		}
	}
    else
    {
        stringstream cStringStream;
        cStringStream << "Failed to open overnight stat file " << sOvernightStatFileName << ".";
        ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());

        _cLogger << "Failed to open overnight stat file " << sOvernightStatFileName << "\n";
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
//		_cLogger << "Loading overnight stat from date " << to_iso_string(cStatDay) << "\n";
//		_cLogger << "Assign dQuoteInstrumentEXMA: " << itr->second->dQuoteInstrumentEXMA << "\n";
//      _cLogger << "with " << itr->second->iQuoteInstrumentEXMANumDataPoints << " data points " << "\n";
		_pQuoteInstrument->setNewEXMA(itr->second->dQuoteInstrumentEXMA, itr->second->iQuoteInstrumentEXMANumDataPoints);
//		_cLogger << "Assign dQuoteInstrumentWeightedStdevSqrdEXMA: " << itr->second->dQuoteInstrumentWeightedStdevSqrdEXMA << "\n";
//		_cLogger << "Assign dQuoteInstrumentWeightedStdevEXMA: " << itr->second->dQuoteInstrumentWeightedStdevEXMA << "\n";
//      _cLogger << "with " << itr->second->iQuoteInstrumentWeightedStdevNumDataPoints << " data points \n";
		_pQuoteInstrument->dsetNewWeightedStdevEXMA(itr->second->dQuoteInstrumentWeightedStdevSqrdEXMA, itr->second->dQuoteInstrumentWeightedStdevEXMA, itr->second->iQuoteInstrumentWeightedStdevNumDataPoints);
//		_cLogger << "Assign dQuoteInstrumentWeightedStdevAdjustment: " << itr->second->dQuoteInstrumentWeightedStdevAdjustment << "\n";
		_pQuoteInstrument->applyWeightedStdevAdjustment(itr->second->dQuoteInstrumentWeightedStdevAdjustment);

//		_cLogger << "Assign dSpreadFrontInstrumentEXMA: " << itr->second->dSpreadFrontInstrumentEXMA << "\n";
//      _cLogger << "with " << itr->second->iSpreadFrontInstrumentEXMANumDataPoints << " data points " << "\n";
		_pSpreadFrontInstrument->setNewEXMA(itr->second->dSpreadFrontInstrumentEXMA, itr->second->iSpreadFrontInstrumentEXMANumDataPoints);
//		_cLogger << "Assign dSpreadFrontInstrumentWeightedStdevSqrdEXMA: " << itr->second->dSpreadFrontInstrumentWeightedStdevSqrdEXMA << "\n";
//		_cLogger << "Assign dSpreadFrontInstrumentWeightedStdevEXMA: " << itr->second->dSpreadFrontInstrumentWeightedStdevEXMA << "\n";
//      _cLogger << "with " << itr->second->iSpreadFrontInstrumentWeightedStdevNumDataPoints << " data points \n";
		_pSpreadFrontInstrument->dsetNewWeightedStdevEXMA(itr->second->dSpreadFrontInstrumentWeightedStdevSqrdEXMA, itr->second->dSpreadFrontInstrumentWeightedStdevEXMA, itr->second->iSpreadFrontInstrumentWeightedStdevNumDataPoints);
//		_cLogger << "Assign dSpreadFrontInstrumentWeightedStdevAdjustment: " << itr->second->dSpreadFrontInstrumentWeightedStdevAdjustment << "\n";
		_pSpreadFrontInstrument->applyWeightedStdevAdjustment(itr->second->dSpreadFrontInstrumentWeightedStdevAdjustment);

//		_cLogger << "Assign dSpreadBackInstrumentEXMA: " << itr->second->dSpreadBackInstrumentEXMA << "\n";
//      _cLogger << "with " << itr->second->iSpreadBackInstrumentEXMANumDataPoints << " data points " << "\n";
		_pSpreadBackInstrument->setNewEXMA(itr->second->dSpreadBackInstrumentEXMA, itr->second->iSpreadBackInstrumentEXMANumDataPoints);
//		_cLogger << "Assign dSpreadBackInstrumentWeightedStdevSqrdEXMA: " << itr->second->dSpreadBackInstrumentWeightedStdevSqrdEXMA << "\n";
//		_cLogger << "Assign dSpreadBackInstrumentWeightedStdevEXMA: " << itr->second->dSpreadBackInstrumentWeightedStdevEXMA << "\n";
//      _cLogger << "with " << itr->second->iSpreadBackInstrumentWeightedStdevNumDataPoints << " data points \n";
		_pSpreadBackInstrument->dsetNewWeightedStdevEXMA(itr->second->dSpreadBackInstrumentWeightedStdevSqrdEXMA, itr->second->dSpreadBackInstrumentWeightedStdevEXMA, itr->second->iSpreadBackInstrumentWeightedStdevNumDataPoints);
//		_cLogger << "Assign dSpreadBackInstrumentWeightedStdevAdjustment: " << itr->second->dSpreadBackInstrumentWeightedStdevAdjustment << "\n";
		_pSpreadBackInstrument->applyWeightedStdevAdjustment(itr->second->dSpreadBackInstrumentWeightedStdevAdjustment);

//		_cLogger << "Assign dSpreadInstrumentWeightedStdevSqrdEXMA: " << itr->second->dSpreadInstrumentWeightedStdevSqrdEXMA << "\n";
//		_cLogger << "Assign dSpreadInstrumentWeightedStdevEXMA: " << itr->second->dSpreadInstrumentWeightedStdevEXMA << "\n";
//      _cLogger << "with " << itr->second->iSpreadInstrumentWeightedStdevNumDataPoints << " data points \n";
		_pSpreadInstrument->dsetNewWeightedStdevEXMA(itr->second->dSpreadInstrumentWeightedStdevSqrdEXMA, itr->second->dSpreadInstrumentWeightedStdevEXMA, itr->second->iSpreadInstrumentWeightedStdevNumDataPoints);
//		_cLogger << "Assign dSpreadInstrumentWeightedStdevAdjustment: " << itr->second->dSpreadInstrumentWeightedStdevAdjustment << "\n";
		_pSpreadInstrument->applyWeightedStdevAdjustment(itr->second->dSpreadInstrumentWeightedStdevAdjustment);

//		_cLogger << "Assign dProductWeightedStdevSqrdEXMA: " << itr->second->dProductWeightedStdevSqrdEXMA << "\n";
//		_cLogger << "Assign dProductWeightedStdevEXMA: " << itr->second->dProductWeightedStdevEXMA << "\n";
//      _cLogger << "with " << itr->second->iProductWeightedStdevNumDataPoints << " data points \n";
		_pProductInstrument->dsetNewWeightedStdevEXMA(itr->second->dProductWeightedStdevSqrdEXMA, itr->second->dProductWeightedStdevEXMA, itr->second->iProductWeightedStdevNumDataPoints);
//		_cLogger << "Assign dProductWeightedStdevAdjustment: " << itr->second->dProductWeightedStdevAdjustment << "\n";
		_pProductInstrument->applyWeightedStdevAdjustment(itr->second->dProductWeightedStdevAdjustment);
	}
}

}
