#include <stdlib.h>
#include <boost/math/special_functions/round.hpp>

#include "SL3L.h"
#include "../EngineInfra/ErrorHandler.h"
#include "../EngineInfra/SystemClock.h"

using namespace boost::posix_time;
using std::stringstream;

namespace KO
{

SL3L::SL3L(const string& sEngineRunTimePath,
           const string& sEngineSlotName,
           KOEpochTime cTradingStartTime,
           KOEpochTime cTradingEndTime,
           SchedulerBase* pScheduler,
           string sTodayDate,
           const string& sSimType,
           KOEpochTime cSlotFirstWakeupCallTime)
:SLBase(sEngineRunTimePath, "SL3L", sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, sSimType, cSlotFirstWakeupCallTime),
 _bSpreadFrontInstrumentStaled(false),
 _bSpreadBackInstrumentStaled(false),
 _bSpreadPositiveCorrelation(true),
 _dLeaderWeight(1),
 _bUpdateStats(true)
{
    _bProductPositiveCorrelation = true;
    _pSpreadFrontInstrument = NULL;
    _pSpreadBackInstrument = NULL;
    _pSpreadInstrument = NULL;

    _dLastSpreadFrontMid = -1;
    _dLastSpreadBackMid = -1;

    _bIsBullishMom = false;
    _bIsBearishMom = false;
}

SL3L::~SL3L()
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

void SL3L::readFromStream(istream& is)
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
        else if(sParamName == "TargetCorr")
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            if(sValue == "1")
            {
                _bProductPositiveCorrelation = true;
            }
            else
            {
                _bProductPositiveCorrelation = false;
            }
        }
        else if(sParamName == "IndicatorCorr")
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            if(sValue == "1")
            {
                _bSpreadPositiveCorrelation = true;
            }
            else
            {
                _bSpreadPositiveCorrelation = false;
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
        else if(sParamName.compare("IOC") == 0)
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            if(sValue == "0")
            {
                _bIOC = false;
            }
            else
            {
                _bIOC = true;
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

void SL3L::dayInit()
{
    setupLogger("SL3L");

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
        _pSpreadFrontInstrument->useWeightedStdev(_iStdevLength, false, KOEpochTime(0,0), 60, _cVolStartTime, _cVolEndTime, _bIsSV);

        _pSpreadBackInstrument = new Instrument(vContractQuoteDatas[2]->sProduct, vContractQuoteDatas[2]->iCID, vContractQuoteDatas[2]->eInstrumentType, vContractQuoteDatas[2]->dTickSize, vContractQuoteDatas[1]->iMaxSpreadWidth, _bUseRealSignalPrice);
        _pSpreadBackInstrument->useEXMA(_dDriftLength, false, KOEpochTime(0,0) , 1, _cTradingStartTime, _cTradingEndTime);
        _pSpreadBackInstrument->useWeightedStdev(_iStdevLength, false, KOEpochTime(0,0), 60, _cVolStartTime, _cVolEndTime, _bIsSV);

        _pSpreadInstrument = new SyntheticSpread((vContractQuoteDatas[1]->sProduct + "-" + vContractQuoteDatas[2]->sProduct),
                                 _pSpreadFrontInstrument,
                                 SyntheticSpread::WhiteAbs,
                                 _pSpreadBackInstrument,
                                 SyntheticSpread::WhiteAbs,
                                 1,
                                 _bSpreadPositiveCorrelation,
                                 _bUseRealSignalPrice);
        _pSpreadInstrument->useEXMA(_dDriftLength, false, KOEpochTime(0,0) , 1, _cTradingStartTime, _cTradingEndTime);
        _pSpreadInstrument->useWeightedStdev(_iStdevLength, false, KOEpochTime(0,0), 60, _cVolStartTime, _cVolEndTime, _bIsSV);

        if(_bIsSV == true)
        {
            _pProductInstrument = new SyntheticSpread("Product",
                                      _pQuoteInstrument,
                                      SyntheticSpread::WhiteAbs,
                                      _pSpreadInstrument,
                                      SyntheticSpread::WhiteAbs,
                                      _dLeaderWeight,
                                      _bProductPositiveCorrelation,
                                      _bUseRealSignalPrice);
        }
        else
        {
            _pProductInstrument = new SyntheticSpread("Product",
                                      _pQuoteInstrument,
                                      SyntheticSpread::WhiteAbs,
                                      _pSpreadInstrument,
                                      SyntheticSpread::Simple,
                                      _dLeaderWeight,
                                      _bProductPositiveCorrelation,
                                      _bUseRealSignalPrice);
        }
    
        _pProductInstrument->useWeightedStdev(_iProductStdevLength, false, KOEpochTime(0,0), 60, _cVolStartTime, _cVolEndTime, false);

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
		_cLogger << "Product stat is valid: " << _pProductInstrument->dgetWeightedStdev() << "\n";

        for(unsigned int i = 0; i < _vEntryStd.size(); i++)
        {
            _cLogger << "Trigger set " << i << " EntryThreshold " << _pProductInstrument->dgetWeightedStdev() * _vEntryStd[i] << " " << "ExitThreshold " << _pProductInstrument->dgetWeightedStdev() * _vExitStd[i] << "\n";
        }

//		_cLogger << "_pQuoteInstrument->dgetWeightedStdevEXMA() " << _pQuoteInstrument->dgetWeightedStdevEXMA() << "\n";
//		_cLogger << "_pQuoteInstrument->dgetWeightedStdevSqrdEXMA() " << _pQuoteInstrument->dgetWeightedStdevSqrdEXMA() << "\n";
    }

    _cLogger << "Daily strategy initialisation finished" << std::endl;
}

void SL3L::dayTrade()
{
    _cLogger << "Daily strategy trade " << std::endl;

    SLBase::dayTrade();

    _cLogger << "Daily strategy trade finished " << std::endl;
}

void SL3L::dayRun()
{
    _cLogger << "Daily strategy run " << std::endl;

    _pSpreadFrontInstrument->newMarketUpdate(vContractQuoteDatas[1]);
    _dLastSpreadFrontMid = _pSpreadFrontInstrument->dgetWeightedMid();
    _pSpreadBackInstrument->newMarketUpdate(vContractQuoteDatas[2]);
    _dLastSpreadBackMid = _pSpreadBackInstrument->dgetWeightedMid();

    SLBase::dayRun();

    _cLogger << "Daily strategy run finished " << std::endl;
}

void SL3L::dayStop()
{
    _cLogger << "Daily strategy stop " << std::endl;

    SLBase::dayStop();

    loadRollDelta();

    if(_bUpdateStats == true)
    {
        if(_pScheduler->bisLiveTrading() == true)
        {
            saveOvernightStats(false);
        }
        else
        {
            if((SystemClock::GetInstance()->cgetCurrentKOEpochTime() - _cLastTimeAllProductsReady).igetPrintable() < 14400000000 && (SystemClock::GetInstance()->cgetCurrentKOEpochTime() - _cLastQuoteUpdateTime).igetPrintable() < 14400000000 && (SystemClock::GetInstance()->cgetCurrentKOEpochTime() - _cLastSpreadFrontUpdateTime).igetPrintable() < 14400000000 && (SystemClock::GetInstance()->cgetCurrentKOEpochTime() - _cLastSpreadBackUpdateTime).igetPrintable() < 14400000000)
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
    _pSpreadFrontInstrument->eodReset();
    _pSpreadBackInstrument->eodReset();

    _cLogger << "Daily strategy stop finished " << std::endl;

    _cLogger.closeFile();
    _cSpreadLogger.closeFile();
}

void SL3L::receive(int iCID)
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
std::cerr << "SPREAD FRONT|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << vContractQuoteDatas[iUpdateIndex]->iBidSize
                                                                                                       << "|" << vContractQuoteDatas[iUpdateIndex]->dBestBid
                                                                                                       << "|" << vContractQuoteDatas[iUpdateIndex]->dBestAsk
                                                                                                       << "|" << vContractQuoteDatas[iUpdateIndex]->iAskSize << "\n";
*/

		// Spread Front instrument updated
		_pSpreadFrontInstrument->newMarketUpdate(vContractQuoteDatas[iUpdateIndex]);
		_iTotalSpreadFrontInstruUpdate = _iTotalSpreadFrontInstruUpdate + 1;
        _cLastSpreadFrontUpdateTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
/*
_cLogger << "SPREAD FRONT|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << _pSpreadFrontInstrument->igetBidSize() 
        														          						       << "|" << _pSpreadFrontInstrument->igetBestBid() 
		        																			           << "|" << _pSpreadFrontInstrument->igetBestAsk() 
				        																	           << "|" << _pSpreadFrontInstrument->igetAskSize()
                                                                                                       << "|" << _pSpreadFrontInstrument->igetLastTrade()
                                                                                                       << "|" << _pSpreadFrontInstrument->igetAccumuTradeSize()<< "\n";
*/

        if(_dLastSpreadFrontMid > 0)
        {
            if(_pSpreadFrontInstrument->dgetWeightedMid() > _dLastSpreadFrontMid * 1.1 ||
               _pSpreadFrontInstrument->dgetWeightedMid() < _dLastSpreadFrontMid * 0.9)
            {
                std::stringstream cStringStream;
                cStringStream << "Error: " << _pSpreadFrontInstrument->sgetProductName() << " received an update bigger or smaller than 10\%! New update: " << _pSpreadFrontInstrument->dgetWeightedMid() << " Last update " << _dLastSpreadFrontMid << " Time " << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();

                std::cerr << cStringStream.str() << "\n";
                ErrorHandler::GetInstance()->newWarningMsg("0", "ALL", "ALL", cStringStream.str());
            }
        }

        _dLastSpreadFrontMid = _pSpreadFrontInstrument->dgetWeightedMid();        

		if(bcheckAllProductsReady())
		{
//            updateTheoPosition();
//            calculateQuotePrice();
//            _pFairValueExecution->newTheoQuotePrices(_iTheoBid, _iTheoOffer, _iTheoExitBid, _iTheoExitOffer, false);
//            _pFairValueExecution->updateOrders();
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
        _cLastSpreadBackUpdateTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
/*
_cLogger << "SPREAD BACK|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << _pSpreadBackInstrument->igetBidSize() 
           														          					          << "|" << _pSpreadBackInstrument->igetBestBid() 
																					                  << "|" << _pSpreadBackInstrument->igetBestAsk() 
																					                  << "|" << _pSpreadBackInstrument->igetAskSize()
                                                                                                      << "|" << _pSpreadBackInstrument->igetLastTrade()
                                                                                                      << "|" << _pSpreadBackInstrument->igetAccumuTradeSize()<< "\n";
*/

        if(_dLastSpreadBackMid > 0)
        {
            if(_pSpreadBackInstrument->dgetWeightedMid() > _dLastSpreadBackMid * 1.1 ||
               _pSpreadBackInstrument->dgetWeightedMid() < _dLastSpreadBackMid * 0.9)
            {
                std::stringstream cStringStream;
                cStringStream << "Error: " << _pSpreadBackInstrument->sgetProductName() << " received an update bigger or smaller than 10\%! New update: " << _pSpreadBackInstrument->dgetWeightedMid() << " Last update " << _dLastSpreadBackMid << " Time " << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();

                std::cerr << cStringStream.str() << "\n";
                ErrorHandler::GetInstance()->newWarningMsg("0", "ALL", "ALL", cStringStream.str());
            }
        }

        _dLastSpreadBackMid = _pSpreadBackInstrument->dgetWeightedMid();        
	}
}

void SL3L::updateStatistics(KOEpochTime cCallTime)
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

/*
            double dQuoteLeg = (_pQuoteInstrument->dgetWeightedMid() - _pQuoteInstrument->dgetEXMA()) / _pQuoteInstrument->dgetWeightedStdev();
*/
            double dFrontLeg = (_pSpreadFrontInstrument->dgetWeightedMid() - _pSpreadFrontInstrument->dgetEXMA()) / _pSpreadFrontInstrument->dgetWeightedStdev();
            double dBackLeg = (_pSpreadBackInstrument->dgetWeightedMid() - _pSpreadBackInstrument->dgetEXMA()) / _pSpreadBackInstrument->dgetWeightedStdev();

            if(dFrontLeg > 3 && dBackLeg > 3)
            {
                if(abs((dFrontLeg - dBackLeg) / (dFrontLeg + dBackLeg)) < 0.5)
                {
                    _bIsBullishMom = true;
                    _bIsBearishMom = false;
                } 
            }
            else if(dFrontLeg < -3 && dBackLeg < -3)
            {
                if(abs((dFrontLeg - dBackLeg) / (dFrontLeg + dBackLeg)) < 0.5)
                {
                    _bIsBearishMom = true;
                    _bIsBullishMom = false;
                } 
            }
            else
            {
                if(-1 < dFrontLeg && dFrontLeg < 1 && -1 < dBackLeg && dBackLeg < 1)
                {
                    _bIsBearishMom = false;
                    _bIsBullishMom = false;
                }
            }

            if(_bIsBullishMom == true)
            {
                _cLogger << "GE in Bullish momentum \n";
            }

            if(_bIsBearishMom == true)
            {
                _cLogger << "GE in Bearish momentum \n";
            }

/*
            double dSpreadLeg;
            if(_bIsSV == true)
            {
                dSpreadLeg = (_pSpreadInstrument->dgetWeightedMid() - _pSpreadInstrument->dgetEXMA()) / _pSpreadInstrument->dgetWeightedStdev();
            }
            else
            {
                dSpreadLeg = dFrontLeg - dBackLeg;
            }

            if(abs(dQuoteLeg) > 3 && abs(dFrontLeg) > 3 && abs(dBackLeg) > 3)
            {
                _bIsBullishMom = true;
                _bIsBearishMom = true;

                if(dQuoteLeg > 0 && dSpreadLeg > 0 && dSpreadLeg > dQuoteLeg)
                {
                    _bIsBearishMom = false;
                }
                else if(dQuoteLeg < 0 && dSpreadLeg < 0 && dSpreadLeg < dQuoteLeg)
                {
                    _bIsBullishMom = false;
                }
            }
*/

            updateSignal();

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

            if(_bIOC == true)
            {
                _cLogger << "|" << _pProductInstrument->dgetAsk();
                _cLogger << "|" << _dSpreadIndicator;
                _cLogger << "|" << _pProductInstrument->dgetBid() << std::endl;
            }
            else
            {
                _cLogger << "|" << _dSpreadIndicator << std::endl;
            }

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
        _cLogger << "Ignore wake up call. Instrument statistic not valid. numDataPoint is " << _pQuoteInstrument->igetWeightedStdevNumDataPoints() << std::endl;
    }
}

void SL3L::writeSpreadLog()
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

bool SL3L::bcheckAllProductsReady()
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

void SL3L::wakeup(KOEpochTime cCallTime)
{
    SLBase::wakeup(cCallTime);
}

void SL3L::loadRollDelta()
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

        _dLastSpreadFrontMid = _dLastSpreadFrontMid + dSpreadFrontRollDelta;
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

        _dLastSpreadBackMid = _dLastSpreadBackMid + dSpreadBackRollDelta;
    }
}

void SL3L::saveOvernightStats(bool bRemoveToday)
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
                if(_pQuoteInstrument->dgetEXMA() > 0.001 && _pSpreadFrontInstrument->dgetEXMA() > 0.001 && _pSpreadBackInstrument->dgetEXMA() > 0.001 && bRemoveToday == false)
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
                                  << _pSpreadBackInstrument->dgetWeightedStdevAdjustment() << ",";

                    if(_bIsSV == true)
                    {
                        cStringStream << _pSpreadInstrument->dgetEXMA() << ","
                                      << _pSpreadInstrument->igetEXMANumDataPoints() << ",";
                    }

                    cStringStream << _pSpreadInstrument->dgetWeightedStdevEXMA() << ","
                                  << _pSpreadInstrument->dgetWeightedStdevSqrdEXMA() << ","
                                  << _pSpreadInstrument->igetWeightedStdevNumDataPoints() << ","
                                  << _pSpreadInstrument->dgetWeightedStdevAdjustment() << ","
                                  << _pProductInstrument->dgetWeightedStdevEXMA() << ","
                                  << _pProductInstrument->dgetWeightedStdevSqrdEXMA() << ","
                                  << _pProductInstrument->igetWeightedStdevNumDataPoints() << ","
                                  << _pProductInstrument->dgetWeightedStdevAdjustment() << ","
                                  << _dLastQuoteMid << ","
                                  << _dLastSpreadFrontMid << ","
                                  << _dLastSpreadBackMid << "\n";
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
                          << itr->second->dSpreadBackInstrumentWeightedStdevAdjustment << ",";
            if(_bIsSV == true)
            {
                cStringStream << itr->second->dSpreadInstrumentEXMA << ","
                              << itr->second->iSpreadInstrumentEXMANumDataPoints << ",";
            }
            
            cStringStream << itr->second->dSpreadInstrumentWeightedStdevEXMA << ","
                          << itr->second->dSpreadInstrumentWeightedStdevSqrdEXMA << ","
                          << itr->second->iSpreadInstrumentWeightedStdevNumDataPoints << ","
                          << itr->second->dSpreadInstrumentWeightedStdevAdjustment << ","
                          << itr->second->dProductWeightedStdevEXMA << ","
                          << itr->second->dProductWeightedStdevSqrdEXMA << ","
                          << itr->second->iProductWeightedStdevNumDataPoints << ","
                          << itr->second->dProductWeightedStdevAdjustment << ","
                          << itr->second->dLastQuoteMid << ","
                          << itr->second->dLastSpreadFrontMid << ","
                          << itr->second->dLastSpreadBackMid << "\n";
        }
    }

    if(!bCurrentDayWritten)
    {
        if(_pQuoteInstrument->dgetEXMA() > 0.001 && _pSpreadFrontInstrument->dgetEXMA() > 0.001 && _pSpreadBackInstrument->dgetEXMA() > 0.001 && bRemoveToday == false)
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
                          << _pSpreadBackInstrument->dgetWeightedStdevAdjustment() << ",";

            if(_bIsSV == true)
            {
                cStringStream << _pSpreadInstrument->dgetEXMA() << ","
                              << _pSpreadInstrument->igetEXMANumDataPoints() << ",";
            }

            cStringStream << _pSpreadInstrument->dgetWeightedStdevEXMA() << ","
                          << _pSpreadInstrument->dgetWeightedStdevSqrdEXMA() << ","
                          << _pSpreadInstrument->igetWeightedStdevNumDataPoints() << ","
                          << _pSpreadInstrument->dgetWeightedStdevAdjustment() << ","
                          << _pProductInstrument->dgetWeightedStdevEXMA() << ","
                          << _pProductInstrument->dgetWeightedStdevSqrdEXMA() << ","
                          << _pProductInstrument->igetWeightedStdevNumDataPoints() << ","
                          << _pProductInstrument->dgetWeightedStdevAdjustment() << ","
                          << _dLastQuoteMid << ","
                          << _dLastSpreadFrontMid << ","
                          << _dLastSpreadBackMid << "\n";
        }

        bCurrentDayWritten = true;
    }

	fstream ofsOvernightStatFile;

    const unsigned long length = 4000000;
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
    if(_bIsSV == true)
    {
        pTodayStat->dSpreadInstrumentEXMA = _pSpreadInstrument->dgetEXMA();
        pTodayStat->iSpreadInstrumentEXMANumDataPoints = _pSpreadInstrument->igetEXMANumDataPoints();
    }
    pTodayStat->dSpreadInstrumentWeightedStdevEXMA = _pSpreadInstrument->dgetWeightedStdevEXMA();
    pTodayStat->dSpreadInstrumentWeightedStdevSqrdEXMA = _pSpreadInstrument->dgetWeightedStdevSqrdEXMA();
    pTodayStat->iSpreadInstrumentWeightedStdevNumDataPoints = _pSpreadInstrument->igetWeightedStdevNumDataPoints();
    pTodayStat->dSpreadInstrumentWeightedStdevAdjustment = _pSpreadInstrument->dgetWeightedStdevAdjustment();
    pTodayStat->dProductWeightedStdevEXMA = _pProductInstrument->dgetWeightedStdevEXMA();
    pTodayStat->dProductWeightedStdevSqrdEXMA = _pProductInstrument->dgetWeightedStdevSqrdEXMA();
    pTodayStat->iProductWeightedStdevNumDataPoints = _pProductInstrument->igetWeightedStdevNumDataPoints();
    pTodayStat->dProductWeightedStdevAdjustment = _pProductInstrument->dgetWeightedStdevAdjustment();
    pTodayStat->dLastQuoteMid = _dLastQuoteMid;
    pTodayStat->dLastSpreadFrontMid = _dLastSpreadFrontMid;
    pTodayStat->dLastSpreadBackMid = _dLastSpreadBackMid;
    

    _mDailyStats[cTodayDate] = pTodayStat;
}

void SL3L::loadOvernightStats()
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
                    cStringStream << "Error in slot " << _sEngineSlotName << " on date " << _sTodayDate << " invalid date in overnightstats.cfg: " << sElement << ".";
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
                        pDailyStat->iQuoteInstrumentEXMANumDataPoints = stol(sElement.c_str());
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
                        pDailyStat->iQuoteInstrumentWeightedStdevNumDataPoints = stol(sElement.c_str());
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
                        pDailyStat->dSpreadFrontInstrumentEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->iSpreadFrontInstrumentEXMANumDataPoints = stol(sElement.c_str());
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dSpreadFrontInstrumentWeightedStdevEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dSpreadFrontInstrumentWeightedStdevSqrdEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->iSpreadFrontInstrumentWeightedStdevNumDataPoints = stol(sElement.c_str());
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dSpreadFrontInstrumentWeightedStdevAdjustment = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dSpreadBackInstrumentEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->iSpreadBackInstrumentEXMANumDataPoints = stol(sElement.c_str());
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dSpreadBackInstrumentWeightedStdevEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dSpreadBackInstrumentWeightedStdevSqrdEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->iSpreadBackInstrumentWeightedStdevNumDataPoints = stol(sElement.c_str());
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dSpreadBackInstrumentWeightedStdevAdjustment = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    if(_bIsSV == true)
                    {
                        try
                        {
                            pDailyStat->dSpreadInstrumentEXMA = stod(sElement);
                            bStatValid = bStatValid && true;
                        }
                        catch(exception e)  
                        {
                            bStatValid = bStatValid && false;
                        }

                        std::getline(cDailyStatStream, sElement, ',');
                        try
                        {
                            pDailyStat->iSpreadInstrumentEXMANumDataPoints = stol(sElement.c_str());
                            bStatValid = bStatValid && true;
                        }
                        catch(exception e)
                        {
                            bStatValid = bStatValid && false;
                        }

                        std::getline(cDailyStatStream, sElement, ',');
                    }

                    try
                    {
                        pDailyStat->dSpreadInstrumentWeightedStdevEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dSpreadInstrumentWeightedStdevSqrdEXMA = stod(sElement);
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->iSpreadInstrumentWeightedStdevNumDataPoints = stol(sElement.c_str());
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dSpreadInstrumentWeightedStdevAdjustment = stod(sElement);
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
                        pDailyStat->iProductWeightedStdevNumDataPoints = stol(sElement.c_str());
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
                        pDailyStat->dLastQuoteMid = atof(sElement.c_str());
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dLastSpreadFrontMid = atof(sElement.c_str());
                        bStatValid = bStatValid && true;
                    }
                    catch(exception e)
                    {
                        bStatValid = bStatValid && false;
                    }

                    std::getline(cDailyStatStream, sElement, ',');
                    try
                    {
                        pDailyStat->dLastSpreadBackMid = atof(sElement.c_str());
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

   		_cLogger << "Assign dSpreadFrontInstrumentEXMA: " << itr->second->dSpreadFrontInstrumentEXMA << "\n";
        _cLogger << "with " << itr->second->iSpreadFrontInstrumentEXMANumDataPoints << " data points " << "\n";
		_pSpreadFrontInstrument->setNewEXMA(itr->second->dSpreadFrontInstrumentEXMA, itr->second->iSpreadFrontInstrumentEXMANumDataPoints);
		_cLogger << "Assign dSpreadFrontInstrumentWeightedStdevSqrdEXMA: " << itr->second->dSpreadFrontInstrumentWeightedStdevSqrdEXMA << "\n";
		_cLogger << "Assign dSpreadFrontInstrumentWeightedStdevEXMA: " << itr->second->dSpreadFrontInstrumentWeightedStdevEXMA << "\n";
        _cLogger << "with " << itr->second->iSpreadFrontInstrumentWeightedStdevNumDataPoints << " data points \n";
		_pSpreadFrontInstrument->dsetNewWeightedStdevEXMA(itr->second->dSpreadFrontInstrumentWeightedStdevSqrdEXMA, itr->second->dSpreadFrontInstrumentWeightedStdevEXMA, itr->second->iSpreadFrontInstrumentWeightedStdevNumDataPoints);
		_cLogger << "Assign dSpreadFrontInstrumentWeightedStdevAdjustment: " << itr->second->dSpreadFrontInstrumentWeightedStdevAdjustment << "\n";
		_pSpreadFrontInstrument->applyWeightedStdevAdjustment(itr->second->dSpreadFrontInstrumentWeightedStdevAdjustment);

		_cLogger << "Assign dSpreadBackInstrumentEXMA: " << itr->second->dSpreadBackInstrumentEXMA << "\n";
        _cLogger << "with " << itr->second->iSpreadBackInstrumentEXMANumDataPoints << " data points " << "\n";
		_pSpreadBackInstrument->setNewEXMA(itr->second->dSpreadBackInstrumentEXMA, itr->second->iSpreadBackInstrumentEXMANumDataPoints);
		_cLogger << "Assign dSpreadBackInstrumentWeightedStdevSqrdEXMA: " << itr->second->dSpreadBackInstrumentWeightedStdevSqrdEXMA << "\n";
		_cLogger << "Assign dSpreadBackInstrumentWeightedStdevEXMA: " << itr->second->dSpreadBackInstrumentWeightedStdevEXMA << "\n";
        _cLogger << "with " << itr->second->iSpreadBackInstrumentWeightedStdevNumDataPoints << " data points \n";
		_pSpreadBackInstrument->dsetNewWeightedStdevEXMA(itr->second->dSpreadBackInstrumentWeightedStdevSqrdEXMA, itr->second->dSpreadBackInstrumentWeightedStdevEXMA, itr->second->iSpreadBackInstrumentWeightedStdevNumDataPoints);
		_cLogger << "Assign dSpreadBackInstrumentWeightedStdevAdjustment: " << itr->second->dSpreadBackInstrumentWeightedStdevAdjustment << "\n";
		_pSpreadBackInstrument->applyWeightedStdevAdjustment(itr->second->dSpreadBackInstrumentWeightedStdevAdjustment);

        if(_bIsSV == true)
        {
            _cLogger << "Assign dSpreadInstrumentEXMA: " << itr->second->dSpreadInstrumentEXMA << "\n";
            _cLogger << "with " << itr->second->iSpreadInstrumentEXMANumDataPoints << " data points " << "\n";
            _pSpreadInstrument->setNewEXMA(itr->second->dSpreadInstrumentEXMA, itr->second->iSpreadInstrumentEXMANumDataPoints);
        }

		_cLogger << "Assign dSpreadInstrumentWeightedStdevSqrdEXMA: " << itr->second->dSpreadInstrumentWeightedStdevSqrdEXMA << "\n";
		_cLogger << "Assign dSpreadInstrumentWeightedStdevEXMA: " << itr->second->dSpreadInstrumentWeightedStdevEXMA << "\n";
      _cLogger << "with " << itr->second->iSpreadInstrumentWeightedStdevNumDataPoints << " data points \n";
		_pSpreadInstrument->dsetNewWeightedStdevEXMA(itr->second->dSpreadInstrumentWeightedStdevSqrdEXMA, itr->second->dSpreadInstrumentWeightedStdevEXMA, itr->second->iSpreadInstrumentWeightedStdevNumDataPoints);
		_cLogger << "Assign dSpreadInstrumentWeightedStdevAdjustment: " << itr->second->dSpreadInstrumentWeightedStdevAdjustment << "\n";
		_pSpreadInstrument->applyWeightedStdevAdjustment(itr->second->dSpreadInstrumentWeightedStdevAdjustment);

		_cLogger << "Assign dProductWeightedStdevSqrdEXMA: " << itr->second->dProductWeightedStdevSqrdEXMA << "\n";
		_cLogger << "Assign dProductWeightedStdevEXMA: " << itr->second->dProductWeightedStdevEXMA << "\n";
        _cLogger << "Assign dProductWeightedStdevNumDataPoints " << itr->second->iProductWeightedStdevNumDataPoints << "\n";
       _cLogger << "with " << itr->second->iProductWeightedStdevNumDataPoints << " data points \n";
		_pProductInstrument->dsetNewWeightedStdevEXMA(itr->second->dProductWeightedStdevSqrdEXMA, itr->second->dProductWeightedStdevEXMA, itr->second->iProductWeightedStdevNumDataPoints);
		_cLogger << "Assign dProductWeightedStdevAdjustment: " << itr->second->dProductWeightedStdevAdjustment << "\n";
		_pProductInstrument->applyWeightedStdevAdjustment(itr->second->dProductWeightedStdevAdjustment);
        _cLogger << "_pProductInstrument->dgetWeightedStdev() is " << _pProductInstrument->dgetWeightedStdev() << "\n";

        _cLogger << "Assign dLastQuoteMid: " << itr->second->dLastQuoteMid << "\n";
        _dLastQuoteMid = itr->second->dLastQuoteMid;

        _cLogger << "Assign dLastSpreadFrontMid: " << itr->second->dLastSpreadFrontMid << "\n";
        _dLastSpreadFrontMid = itr->second->dLastSpreadFrontMid;

        _cLogger << "Assign dLastSpreadBackMid: " << itr->second->dLastSpreadBackMid << "\n";
        _dLastSpreadBackMid = itr->second->dLastSpreadBackMid;
	}
}

}
