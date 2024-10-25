#include "DataRecorder.h"
#include "../EngineInfra/SchedulerBase.h"
#include <boost/math/special_functions/round.hpp>
#include "../EngineInfra/SystemClock.h"
#include "../EngineInfra/ErrorHandler.h"

using namespace boost::posix_time;
using std::cerr;

namespace KO
{
DataRecorder::DataRecorder(const string& sEngineRunTimePath,
                const string& sEngineSlotName,
                KOEpochTime cTradingStartTime,
                KOEpochTime cTradingEndTime,
                SchedulerBase* pScheduler,
                string sTodayDate,
                const string& sSimType)
:TradeEngineBase(sEngineRunTimePath, "DataRecorder", sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, sSimType)
{
    _sTodayDate = sTodayDate;
    _cLastDataPointTime = cTradingStartTime;
    _iNumTimerCallsTriggered = 0;
}

DataRecorder::~DataRecorder()
{

}

void DataRecorder::dayInit()
{
    TradeEngineBase::dayInit();

    string year = _sTodayDate.substr(0,4);
    string month = _sTodayDate.substr(4,2);
    string day = _sTodayDate.substr(6,2);

    _cMarketDataLogger.openFile(_sEngineRunTimePath + "/" + vContractQuoteDatas[0]->sProduct + "_" + year + "-" + month + "-" + day, true, true);

    _iPrevAccumTradeVolume = 0;

    KOEpochTime cEngineLiveDuration = _cTradingEndTime - _cTradingStartTime;
    for(int i = 0; i < cEngineLiveDuration.sec();i++)
    {
        _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(i,0), this);
    }
}

void DataRecorder::dayRun()
{
    TradeEngineBase::dayRun();
}

void DataRecorder::dayStop()
{
    TradeEngineBase::dayStop();
}

void DataRecorder::readFromStream(istream& is)
{
    while(!is.eof())
    {
        string sParam;
        is >> sParam;

        std::istringstream cParamStream (sParam);

        string sParamName;

        std::getline(cParamStream, sParamName, ':');

        if(sParamName == "CheckDataStaled")
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            if(sValue == "0")
            {
                _bCheckDataStaled = false;
            }
            else if(sValue == "1")
            {
                _bCheckDataStaled = true;
            }
        }
        else if(sParamName == "FlushPosition")
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            _iFlushPosition = atoi(sValue.c_str());
        }
        else if(sParamName == "FlushBufferSec")
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            _iFlushBufferSec = atoi(sValue.c_str());
        }
    }
}

void DataRecorder::receive(int iCID)
{
/*
    Timestamp,Type,BidPrice,BidSize,AskPrice,AskSize,Price,Volume,Flags
    1723420800490000000,Quote,97.67, 1,97.68,49,,,
    1723420800529000000,Quote,97.67, 1,97.68,55,,,
    1723420800572000000,Quote,97.67, 5,97.68,55,,,
    1723428074995634003,Trade,,,,,97.67,1,0    
*/

    if(vContractQuoteDatas[0]->iTradeSize != 0)
    {
        // Trade
        _cMarketDataLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << ","
                           << "Trade,,,,,"
                           << vContractQuoteDatas[0]->dLastTradePrice << ","
                           << vContractQuoteDatas[0]->iTradeSize << ",0\n";

        _iPrevAccumTradeVolume = vContractQuoteDatas[0]->iAccumuTradeSize;
    }
    else
    {
        // Quote
        _cMarketDataLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << ","
                           << "Quote,"
                           << vContractQuoteDatas[0]->iBidSize << ","
                           << vContractQuoteDatas[0]->dBestBid << ","
                           << vContractQuoteDatas[0]->dBestAsk << ","
                           << vContractQuoteDatas[0]->iAskSize << ",,,\n";
    }

    _cLastDataPointTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
    _bDataStaledErrorTriggered = false;

    (void) iCID;
}

void DataRecorder::wakeup(KOEpochTime cCallTime)
{
    (void) cCallTime;

    if(_iNumTimerCallsTriggered > 300)
    {
        if((cCallTime - _cLastDataPointTime).sec() > 300)
        {
            if(_bDataStaledErrorTriggered == false)
            {
                stringstream cStringStream;
                cStringStream << "Data recorder for " << vContractQuoteDatas[0]->sRoot << " staled";
                ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());

                _bDataStaledErrorTriggered = true;
            }
        }
    }

    if((cCallTime - _cTradingStartTime).sec() % _iFlushBufferSec == _iFlushPosition)
    {
cerr << _sEngineSlotName << " flushing \n";
        _cMarketDataLogger.flush();
    }

    _iNumTimerCallsTriggered = _iNumTimerCallsTriggered + 1;
}

}
