#include <stdlib.h>
#include <boost/math/special_functions/round.hpp>
#include "../base/MultiLegProduct.h"
#include "SLRL.h"
#include "../EngineInfra/ErrorHandler.h"
#include "../EngineInfra/SystemClock.h"

using namespace boost::posix_time;
using std::stringstream;

namespace KO
{

SLRL::SLRL(const string& sEngineRunTimePath,
           const string& sEngineSlotName,
           KOEpochTime cTradingStartTime,
           KOEpochTime cTradingEndTime,
           SchedulerBase* pScheduler,
           string sTodayDate,
           const string& sSimType,
           KOEpochTime cSlotFirstWakeupCallTime)
:SLBase(sEngineRunTimePath, "SLRL", sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, sSimType, cSlotFirstWakeupCallTime),
 _bUpdateStats(true)
{


}

SLRL::~SLRL()
{
    for(unsigned int i = 0; i < _vInstruments.size(); i++)
    {
        delete _vInstruments[i];
    }
}

void SLRL::readFromStream(istream& is)
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
        else if(sParamName == "Weight")
        {
            std::getline(cParamStream, _sIndicatorStr, ':');
        }
        else if(sParamName == "RollAdjust")
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            std::istringstream cRollStream (sValue);
            string sRollAdjust;
            while(std::getline(cRollStream, sRollAdjust, ','))
            {
                _vRollAdjustment.push_back(stod(sRollAdjust));
            }
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
    }

    loadTriggerSpace();

    _cPreviousCallTime = _cQuotingStartTime;

	registerFigure();
}

void SLRL::dayInit()
{
    setupLogger("SLRL");

    _cLogger << "Daily Strategy initialisation \n";

    _cLogger << "Engine quoting start time " << _cQuotingStartTime.igetPrintable() << "\n";
    _cLogger << "Engine quoting end time " << _cQuotingEndTime.igetPrintable() << "\n";

    SLBase::dayInit();

    loadPrevRollDelta();

    _pQuoteInstrument->setRollParams(_vRollAdjustment[0],0);
    _pQuoteInstrument->useStaticStdev();

    for(unsigned int i = 1; i < vContractQuoteDatas.size(); i++)
    {
        Instrument* pNewInstrument = new Instrument(vContractQuoteDatas[i]->sExchange + "." + vContractQuoteDatas[i]->sProduct, vContractQuoteDatas[i]->iCID, vContractQuoteDatas[i]->eInstrumentType, vContractQuoteDatas[i]->dTickSize, vContractQuoteDatas[i]->iMaxSpreadWidth, true);
        pNewInstrument->setRollParams(_vRollAdjustment[i],0);
        pNewInstrument->useEXMA(_dDriftLength, false, KOEpochTime(0,0) , 1, _cTradingStartTime, _cTradingEndTime);
        pNewInstrument->useStaticStdev();

        _vInstruments.push_back(pNewInstrument);
        _vInstrumentsStaled.push_back(false);
        _vTotalInstrumentsUpdates.push_back(0);
        _vLastInstrumentsMid.push_back(-1);
        _vLastIndicatorUpdateTime.push_back(KOEpochTime());
    }

    loadIndicatorSettings();

    _pMultiLegProduct = new MultiLegProduct("Product", _vIndicators, _vIndicatorWeights);

    _pProductInstrument = new SyntheticSpread("Product",
                              _pQuoteInstrument,
                              SyntheticSpread::WhiteAbs,
                              _pMultiLegProduct,
                              SyntheticSpread::Simple,
                              1,
                              true,
                              true);
    _pProductInstrument->useStaticStdev();

    loadOvernightStats();

    if(_pQuoteInstrument->bgetWeightedStdevValid())
    {
        _cLogger << "Quote stat is valid \n";
    }

    for(unsigned int i = 0; i < _vIndicators.size(); i++)
    {
        if(_vIndicators[i]->bgetWeightedStdevValid())
        {
            _cLogger << "Indicator " << i << " stat is valid \n";
        }
    }

    if(_pProductInstrument->bgetWeightedStdevValid())
    {
        _cLogger << "Product stat is valid \n";
		_cLogger << "Product stat is valid: " << _pProductInstrument->dgetWeightedStdev() << "\n";

        for(unsigned int i = 0; i < _vEntryStd.size(); i++)
        {
            _cLogger << "Trigger set " << i << " EntryThreshold " << _pProductInstrument->dgetWeightedStdev() * _vEntryStd[i] << " " << "ExitThreshold " << _pProductInstrument->dgetWeightedStdev() * _vExitStd[i] << "\n";
        }
    }

    _cLogger << "Daily strategy initialisation finished" << std::endl;
}

void SLRL::dayTrade()
{
    _cLogger << "Daily strategy trade " << std::endl;

    SLBase::dayTrade();

    _cLogger << "Daily strategy trade finished " << std::endl;
}

void SLRL::dayRun()
{
    _cLogger << "Daily strategy run " << std::endl;

    for(unsigned int i = 0;i < vContractQuoteDatas.size(); i++)
    {
        if(i >= 1)
        {
            _vInstruments[i-1]->newMarketUpdate(vContractQuoteDatas[i]);
            _vLastInstrumentsMid[i-1] = _vInstruments[i-1]->dgetWeightedMid();
        }
    }

    SLBase::dayRun();

    _cLogger << "Daily strategy run finished " << std::endl;
}

void SLRL::dayStop()
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
            bool bAllProductUpdated = true;
            for(unsigned int i = 0; i < _vLastIndicatorUpdateTime.size(); i++)
            {
                if((SystemClock::GetInstance()->cgetCurrentKOEpochTime() - _vLastIndicatorUpdateTime[i]).igetPrintable() > 14400000000)
                {
                    bAllProductUpdated = false;
                    break;
                }
            }

            if(bAllProductUpdated == true)
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

    for(unsigned int i = 0; i < _vInstruments.size(); i++)
    {
        _vInstruments[i]->eodReset();
    }

    _cLogger << "Daily strategy stop finished " << std::endl;

    _cLogger.closeFile();
    _cSpreadLogger.closeFile();
}

void SLRL::loadIndicatorSettings()
{
    std::istringstream cIndicatorsStream(_sIndicatorStr);

    string sWeight;
    string sIndicator;    
    while(std::getline(cIndicatorsStream, sWeight, ',') &&
          std::getline(cIndicatorsStream, sIndicator, ','))
    {
        if(sIndicator.find("_") == string::npos)
        {
            _vIndicatorWeights.push_back(stod(sWeight));

            for(vector<Instrument*>::iterator itr = _vInstruments.begin();
                itr != _vInstruments.end();
                ++itr)
            {
                if(sIndicator == (*itr)->sgetProductName())
                {
                    _vIndicators.push_back(*itr);
                    break;
                }
            }
        }
        else
        {
            string sFrontLeg = sIndicator.substr(0, sIndicator.find("_"));
            string sBackLeg = sIndicator.substr(sIndicator.find("_") + 1);

            string sBackLegWeight = sBackLeg.substr(0,4);
            string sBackLegProduct = sBackLeg.substr(4);

            for(vector<Instrument*>::iterator itr = _vInstruments.begin();
                itr != _vInstruments.end();
                ++itr)
            {
                if(sFrontLeg == (*itr)->sgetProductName())
                {
                    _vIndicators.push_back(*itr);
                    break;
                }
            }

            for(vector<Instrument*>::iterator itr = _vInstruments.begin();
                itr != _vInstruments.end();
                ++itr)
            {
                if(sBackLegProduct == (*itr)->sgetProductName())
                {
                    _vIndicators.push_back(*itr);
                    break;
                }
            }

            _vIndicatorWeights.push_back(stod(sWeight));
            _vIndicatorWeights.push_back(-1.0 * stod(sWeight) * stod(sBackLegWeight));
        }
    }
}

void SLRL::receive(int iCID)
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

    if(iUpdateIndex >= 1)
    {
        int iIndicatorIdx = iUpdateIndex - 1;

        _vInstruments[iIndicatorIdx]->newMarketUpdate(vContractQuoteDatas[iUpdateIndex]);
        _vTotalInstrumentsUpdates[iIndicatorIdx] = _vTotalInstrumentsUpdates[iIndicatorIdx] + 1;        
        _vLastIndicatorUpdateTime[iIndicatorIdx] = SystemClock::GetInstance()->cgetCurrentKOEpochTime();

        if(_vLastInstrumentsMid[iIndicatorIdx] > 0)
        {
            if(_vInstruments[iIndicatorIdx]->dgetWeightedMid() > _vLastInstrumentsMid[iIndicatorIdx] * 1.1 ||
               _vInstruments[iIndicatorIdx]->dgetWeightedMid() < _vLastInstrumentsMid[iIndicatorIdx] * 0.9)
            {
                std::stringstream cStringStream;
                cStringStream << "Error: " << _vInstruments[iIndicatorIdx]->sgetProductName() << " received an update bigger or smaller than 10\%! New update: " << _vInstruments[iIndicatorIdx]->dgetWeightedMid() << " Last update " << _vLastInstrumentsMid[iIndicatorIdx] << " Time " << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();

                std::cerr << cStringStream.str() << "\n";
                ErrorHandler::GetInstance()->newWarningMsg("0", "ALL", "ALL", cStringStream.str());
            }
        }

        _vLastInstrumentsMid[iIndicatorIdx] = _vInstruments[iIndicatorIdx]->dgetWeightedMid();        

		if(!bcheckAllProductsReady())
		{
			_cLogger << "Ignore indicator price update\n";
		}
    }
}

void SLRL::updateStatistics(KOEpochTime cCallTime)
{
    _bTradingStatsValid = false;

    _pQuoteInstrument->wakeup(cCallTime);

    bool bAllInstrumentReady = _pQuoteInstrument->bgetEXMAValid() && _pQuoteInstrument->bgetWeightedStdevValid();
    for(unsigned int i = 0; i < _vInstruments.size(); i++)
    {
        _vInstruments[i]->wakeup(cCallTime);
        bAllInstrumentReady = bAllInstrumentReady && _vInstruments[i]->bgetEXMAValid() && _vInstruments[i]->bgetWeightedStdevValid();
    }

    if(bAllInstrumentReady)
    {
        _pMultiLegProduct->wakeup(cCallTime);
        _pProductInstrument->wakeup(cCallTime);		

        _dSpreadIndicator = _pProductInstrument->dgetWeightedMid();

        if(_pProductInstrument->bgetWeightedStdevValid())
        {
            _bIsBullishMom = false;
            _bIsBearishMom = false;

            updateSignal();

            _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();
            _cLogger << "|ST";
            _cLogger << "|" << _pQuoteInstrument->dgetWeightedMid();

            for(unsigned int i = 0; i < _vInstruments.size(); i++)
            {
                _cLogger << "|" << _vInstruments[i]->dgetWeightedMid();
            }

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

            _bTradingStatsValid = true;
        }
        else
        {
            _cLogger << "Ignore wake up call. Final statistic not valid. numDataPoint is " << _pProductInstrument->igetWeightedStdevNumDataPoints() << ". " << _iProductStdevLength << " required " << std::endl;
        }
    }
    else
    {
        _cLogger << "Ignore wake up call. Instrument statistic not valid. numDataPoint is " << _pQuoteInstrument->igetWeightedStdevNumDataPoints() << ". " << _iStdevLength << " required " << std::endl;
    }
}

void SLRL::writeSpreadLog()
{

}

bool SLRL::bcheckAllProductsReady()
{
    bool bResult = true;
    bResult = bResult && _pQuoteInstrument->bgetPriceValid();

    for(unsigned int i = 0; i < _vInstruments.size(); i++)
    {
        bResult = bResult && _vInstruments[i]->bgetPriceValid();
    }

    if(bResult == false)
    {
        _cLogger << "Instrument price not valid" << std::endl;
    }

    return bResult;
}

void SLRL::wakeup(KOEpochTime cCallTime)
{
    SLBase::wakeup(cCallTime);
}

void SLRL::loadRollDelta()
{

}

void SLRL::loadPrevRollDelta()
{
    fstream ifsRollDelta;
    string sRollDeltaFileName = _sEngineRunTimePath + "prevRolldelta.cfg";
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

                _vRollDelta.push_back(dRollDelta);
            }
        }
    }
    else
    {
//        stringstream cStringStream;
//        cStringStream << "Cannot find roll delta file " << sRollDeltaFileName << ".";
//        ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());

//        _cLogger << "Cannot find roll delta file " << sRollDeltaFileName << "\n";
    }

    for(unsigned int i = 0; i < vContractQuoteDatas.size(); i++)
    {
        int iIndicatorIndex = i;
        map< pair<string, string>, double >::iterator itr;

        itr = _mDailyRolls.find(pair<string, string>(_sTodayDate, vContractQuoteDatas[i]->sExchange + "." + vContractQuoteDatas[i]->sProduct));

        if(itr != _mDailyRolls.end())
        {
            double dIndicatorRollDelta = itr->second;

            _vInstruments[iIndicatorIndex]->applyEXMAAdjustment(dIndicatorRollDelta);
            _vInstruments[iIndicatorIndex]->applyWeightedStdevAdjustment(dIndicatorRollDelta);
            _cLogger << vContractQuoteDatas[i]->sExchange + "." + vContractQuoteDatas[i]->sProduct << " applying Roll Delta " << dIndicatorRollDelta << "\n";

            _vLastInstrumentsMid[iIndicatorIndex] = _vLastInstrumentsMid[iIndicatorIndex] + dIndicatorRollDelta;
        }
    }
}

void SLRL::saveOvernightStats(bool bRemoveToday)
{
    stringstream cStringStream;
    cStringStream.precision(20);
    string sOvernightStatFileName = _sEngineRunTimePath + "overnightstats.cfg";

    boost::gregorian::date cTodayDate = boost::gregorian::from_undelimited_string(_sTodayDate);
    bool bCurrentDayWritten = false;
    for(map<boost::gregorian::date, vector<DailyLegStat>>::iterator itr = _mDailyLegStats.begin(); itr != _mDailyLegStats.end(); ++itr)
    {
        boost::gregorian::date cKeyDate = itr->first;

        if(cKeyDate != cTodayDate)
        {
            if(cKeyDate > cTodayDate && !bCurrentDayWritten)
            {
                bool bAllProductNonZero = (_pQuoteInstrument->dgetEXMA() > 0.001);
                for(unsigned int i = 0;i < _vInstruments.size(); i++)
                {
                    bAllProductNonZero = bAllProductNonZero && (_vInstruments[i]->dgetEXMA() > 0.001);
                }

                if(bAllProductNonZero && bRemoveToday == false)
                {
                    cStringStream << _sTodayDate << ","
                                  << _pQuoteInstrument->dgetEXMA() << ","
                                  << _pQuoteInstrument->igetEXMANumDataPoints() << ","
                                  << _pQuoteInstrument->dgetStaticStdev()
                                  << "0,";

                    for(unsigned int i = 0;i < _vInstruments.size(); i++)
                    {
                        cStringStream << _vInstruments[i]->dgetEXMA() << ","
                                      << _vInstruments[i]->igetEXMANumDataPoints() << ","
                                      << _vInstruments[i]->dgetStaticStdev()
                                      << "0,";
                    }

                    cStringStream << "0,";
                    cStringStream << _pProductInstrument->dgetStaticStdev() << ","
                                  << _dLastQuoteMid << ",";

                    for(unsigned int i = 0; i < _vInstruments.size(); i++)
                    {
                        cStringStream << _vLastInstrumentsMid[i];
                        if(i == _vInstruments.size() - 1)
                        {
                            cStringStream << "\n";
                        }
                        else
                        {
                            cStringStream << ",";
                        }
                    }
                }

                bCurrentDayWritten = true;
            }

            cStringStream << to_iso_string(cKeyDate) << ","
                          << _mDailyTargetStats[cKeyDate].dLegEXMA << ","
                          << _mDailyTargetStats[cKeyDate].iLegEXMANumDataPoints << ","
                          << _mDailyTargetStats[cKeyDate].dLegStd << ","
                          << "0,";

            for(unsigned int i = 0; i < _mDailyLegStats[cKeyDate].size(); i ++)
            {
                cStringStream << _mDailyLegStats[cKeyDate][i].dLegEXMA << ","
                              << _mDailyLegStats[cKeyDate][i].iLegEXMANumDataPoints << ","
                              << _mDailyLegStats[cKeyDate][i].dLegStd << ","
                              << "0,";
            }

            cStringStream << "0,";
            cStringStream << _mDailySpreadStats[cKeyDate] << ",";

            cStringStream << _mDailyTargetMid[cKeyDate] << ",";

            for(unsigned int i = 0; i < _mDailyLegMid[cKeyDate].size(); i ++)
            {
                cStringStream << _mDailyLegMid[cKeyDate][i];
                if(i == _mDailyLegMid[cKeyDate].size() - 1)
                {
                    cStringStream << "\n";
                }
                else
                {
                    cStringStream << ",";
                }
            }
        }
    }

    if(!bCurrentDayWritten)
    {
        bool bAllProductNonZero = (_pQuoteInstrument->dgetEXMA() > 0.001);
        for(unsigned int i = 0;i < _vInstruments.size(); i++)
        {
            bAllProductNonZero = bAllProductNonZero && (_vInstruments[i]->dgetEXMA() > 0.001);
        }

        if(bAllProductNonZero && bRemoveToday == false)
        {
            cStringStream << _sTodayDate << ","
                          << _pQuoteInstrument->dgetEXMA() << ","
                          << _pQuoteInstrument->igetEXMANumDataPoints() << ","
                          << _pQuoteInstrument->dgetStaticStdev() << ","
                          << "0,";

            for(unsigned int i = 0;i < _vInstruments.size(); i++)
            {
                cStringStream << _vInstruments[i]->dgetEXMA() << ","
                              << _vInstruments[i]->igetEXMANumDataPoints() << ","
                              << _vInstruments[i]->dgetStaticStdev() << ","
                              << "0,";
            }

            cStringStream << "0,";
            cStringStream << _pProductInstrument->dgetStaticStdev() << ","
                          << _dLastQuoteMid << ",";

            for(unsigned int i = 0; i < _vInstruments.size(); i++)
            {
                cStringStream << _vLastInstrumentsMid[i];
                if(i == _vInstruments.size() - 1)
                {
                    cStringStream << "\n";
                }
                else
                {
                    cStringStream << ",";
                }
            }
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

    DailyLegStat cDailyTargetStat;
    vector<DailyLegStat> cDailyLegStat;
    double dSpreadStdev;

    cDailyTargetStat.dLegEXMA = _pQuoteInstrument->dgetEXMA();
    cDailyTargetStat.iLegEXMANumDataPoints = _pQuoteInstrument->igetEXMANumDataPoints();
    cDailyTargetStat.dLegStd = _pQuoteInstrument->dgetStaticStdev();
    
    for(unsigned int i = 0;i < _vInstruments.size(); i++)
    {
        DailyLegStat cLeg;
        cLeg.dLegEXMA = _vInstruments[i]->dgetEXMA();
        cLeg.iLegEXMANumDataPoints = _vInstruments[i]->igetEXMANumDataPoints();
        cLeg.dLegStd = _vInstruments[i]->dgetStaticStdev();
        cDailyLegStat.push_back(cLeg);
    }

    dSpreadStdev = _pProductInstrument->dgetStaticStdev();

    _mDailyTargetStats[cTodayDate] = cDailyTargetStat;
    _mDailyLegStats[cTodayDate] = cDailyLegStat;
    _mDailySpreadStats[cTodayDate] = dSpreadStdev;
    _mDailyTargetMid[cTodayDate] = _dLastQuoteMid;
    _mDailyLegMid[cTodayDate] = _vLastInstrumentsMid;     
}

void SLRL::loadOvernightStats()
{
    fstream ifsOvernightStatFile;
    string sOvernightStatFileName = _sEngineRunTimePath + "overnightstats.cfg";
    ifsOvernightStatFile.open(sOvernightStatFileName.c_str(), fstream::in);
	if(ifsOvernightStatFile.is_open())
	{
		while(!ifsOvernightStatFile.eof())
		{
			char sNewLine[4096];
			ifsOvernightStatFile.getline(sNewLine, sizeof(sNewLine));

			if(strlen(sNewLine) != 0 && sNewLine[0] != '#')
			{
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

                    DailyLegStat cTargetStat;

                    string sTargetEXMA;
                    string sTargetEXMANumDataPoints;
                    string sTargetStdev;
                    string sRollDelta;

                    bStatValid = bStatValid && std::getline(cDailyStatStream, sTargetEXMA, ',');
                    bStatValid = bStatValid && std::getline(cDailyStatStream, sTargetEXMANumDataPoints, ',');
                    bStatValid = bStatValid && std::getline(cDailyStatStream, sTargetStdev, ',');
                    bStatValid = bStatValid && std::getline(cDailyStatStream, sRollDelta, ',');

                    cTargetStat.dLegEXMA = stod(sTargetEXMA);
                    cTargetStat.iLegEXMANumDataPoints = atoi(sTargetEXMANumDataPoints.c_str());
                    cTargetStat.dLegStd = stod(sTargetStdev);

                    vector<DailyLegStat> vDailyLegStats;
                    for(unsigned int i = 0; i < _vInstruments.size(); i++)
                    {
                        DailyLegStat cLegStat;

                        string sLegEXMA;
                        string sLegEXMANumDataPoints;
                        string sLegStdev;

                        bStatValid = bStatValid && std::getline(cDailyStatStream, sLegEXMA, ',');
                        bStatValid = bStatValid && std::getline(cDailyStatStream, sLegEXMANumDataPoints, ',');
                        bStatValid = bStatValid && std::getline(cDailyStatStream, sLegStdev, ','); 
                        bStatValid = bStatValid && std::getline(cDailyStatStream, sRollDelta, ',');
     
                        cLegStat.dLegEXMA = stod(sLegEXMA);
                        cLegStat.iLegEXMANumDataPoints = atoi(sLegEXMANumDataPoints.c_str());
                        cLegStat.dLegStd = stod(sLegStdev);

                        vDailyLegStats.push_back(cLegStat);
                    }

                    string sSpreadEXMA;
                    string sSpreadStdev;

                    bStatValid = bStatValid && std::getline(cDailyStatStream, sSpreadEXMA, ',');
                    bStatValid = bStatValid && std::getline(cDailyStatStream, sSpreadStdev, ',');

                    double dTargetMid = 0;
                    string sTargetMid;
                    bStatValid = bStatValid && std::getline(cDailyStatStream, sTargetMid, ',');
                    dTargetMid = stod(sTargetMid);

                    vector<double> vDailyLegMid;
                    for(unsigned int i = 0; i < _vInstruments.size(); i++)
                    {
                        string sLegMid;
                        bStatValid = bStatValid && std::getline(cDailyStatStream, sLegMid, ',');
                        vDailyLegMid.push_back(stod(sLegMid));
                    }                    

                    if(bStatValid)
                    {
                        _mDailyTargetStats[cDate] = cTargetStat;
                        _mDailyLegStats[cDate] = vDailyLegStats;
                        _mDailySpreadStats[cDate] = stod(sSpreadStdev);
                        _mDailyTargetMid[cDate] = dTargetMid;
                        _mDailyLegMid[cDate] = vDailyLegMid;
                
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

    map< boost::gregorian::date, double >::iterator spreadStatItr = _mDailySpreadStats.end();

	for(int daysToLookback = 0; daysToLookback < 10; daysToLookback++)
	{
        spreadStatItr = _mDailySpreadStats.find(cStatDay);

        _cLogger << "Trying to load overnight stat from date " << to_iso_string(cStatDay) << "\n";
        if(spreadStatItr != _mDailySpreadStats.end())
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

    if(spreadStatItr != _mDailySpreadStats.end())
	{
		_cLogger << "Loading overnight stat from date " << to_iso_string(cStatDay) << "\n";

        DailyLegStat cTargetStats = _mDailyTargetStats[cStatDay];

		_cLogger << "Assign Target EXMA: " << cTargetStats.dLegEXMA << "\n";
        _cLogger << "with " << cTargetStats.iLegEXMANumDataPoints << " data points " << "\n";
		_pQuoteInstrument->setNewEXMA(cTargetStats.dLegEXMA, cTargetStats.iLegEXMANumDataPoints);
        _pQuoteInstrument->setStaticStdev(cTargetStats.dLegStd);

        vector<DailyLegStat> vIndicatorStats = _mDailyLegStats[cStatDay];

        for(unsigned int i = 0; i != vIndicatorStats.size(); i++)
        {
            _cLogger << "Assign Indicator " << i << " EXMA: " << vIndicatorStats[i].dLegEXMA << "\n";
            _cLogger << "with " << vIndicatorStats[i].iLegEXMANumDataPoints << " data points " << "\n";
            _vInstruments[i]->setNewEXMA(vIndicatorStats[i].dLegEXMA, vIndicatorStats[i].iLegEXMANumDataPoints);
            _cLogger << "Assign Indicator " << i << " Stdev: " << vIndicatorStats[i].dLegStd << "\n";

            _vInstruments[i]->setStaticStdev(vIndicatorStats[i].dLegStd);
        }

        double dSpreadStd = _mDailySpreadStats[cStatDay];

        _cLogger << "Assign Spread Stdev: " << dSpreadStd << "\n";

        _pProductInstrument->setStaticStdev(dSpreadStd);    

        _cLogger << "Assign Target Mid: " << _mDailyTargetMid[cStatDay] << "\n";
        _dLastQuoteMid = _mDailyTargetMid[cStatDay];

        vector<double> vIndicatorMid = _mDailyLegMid[cStatDay];
        for(unsigned int i = 0; i < vIndicatorMid.size(); i++)
        {
            _vLastInstrumentsMid.push_back(vIndicatorMid[i]);
        }
	}
}

}
