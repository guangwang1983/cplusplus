#include "DataPrinter.h"
#include "../EngineInfra/SchedulerBase.h"
#include <boost/math/special_functions/round.hpp>
#include "../EngineInfra/SystemClock.h"
#include <H5Cpp.h>

using namespace boost::posix_time;
using namespace H5;
using std::cerr;

namespace KO
{
DataPrinter::DataPrinter(const string& sEngineRunTimePath,
                const string& sEngineSlotName,
                KOEpochTime cTradingStartTime,
                KOEpochTime cTradingEndTime,
                SchedulerBase* pScheduler,
                string sTodayDate,
                const string& sSimType)
:TradeEngineBase(sEngineRunTimePath, "DataPrinter", sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, sSimType)
{
}

DataPrinter::~DataPrinter()
{

}

void DataPrinter::dayInit()
{
    TradeEngineBase::dayInit();
}

void DataPrinter::dayRun()
{
    TradeEngineBase::dayRun();
}

void DataPrinter::dayStop()
{
    TradeEngineBase::dayStop();
}

void DataPrinter::readFromStream(istream& is)
{
    (void) is;
}

void DataPrinter::receive(int iCID)
{
    cerr << SystemClock::GetInstance()->sToSimpleString(SystemClock::GetInstance()->cgetCurrentKOEpochTime()) << ","
         << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << ","
         << vContractQuoteDatas[0]->iBidSize << ","
         << vContractQuoteDatas[0]->dBestBid << ","
         << vContractQuoteDatas[0]->dBestAsk << ","
         << vContractQuoteDatas[0]->iAskSize << ","
         << vContractQuoteDatas[0]->dLastTradePrice << ","
         << vContractQuoteDatas[0]->iAccumuTradeSize << "\n";

    if(vContractQuoteDatas[0]->iBestBidInTicks >= vContractQuoteDatas[0]->iBestAskInTicks)
    {
        cerr << "ERROR: Data crossed!";
    }
    
    (void) iCID;
}

void DataPrinter::wakeup(KOEpochTime cCallTime)
{
    (void) cCallTime;
}

}
