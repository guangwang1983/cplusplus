#include "OrderTest.h"
#include "../EngineInfra/KOScheduler.h"
#include "../EngineInfra/SystemClock.h"

using std::cerr;
using std::cout;

namespace KO
{

OrderTest::OrderTest(const string& sEngineRunTimePath,
                     const string& sEngineSlotName,
                     KOEpochTime cTradingStartTime,
                     KOEpochTime cTradingEndTime,
                     SchedulerBase* pScheduler,
                     string sTodayDate,
                     PositionServerConnection* pPositionServerConnection)
:TradeEngineBase(sEngineRunTimePath, "OrderTest", sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, pPositionServerConnection)
{

}

OrderTest::~OrderTest()
{

}

void OrderTest::dayInit()
{
cerr << "adding order test wake up call \n";
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2157, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2159, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2160, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2161, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2162, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2163, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2164, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2165, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2166, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2167, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2168, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2169, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2170, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2171, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2172, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2173, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2174, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2175, 0), this);
    _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(2176, 0), this);

    TradeEngineBase::dayInit();
}

void OrderTest::dayRun()
{
    TradeEngineBase::dayRun();
}

void OrderTest::dayStop()
{
    TradeEngineBase::dayStop();
}

void OrderTest::readFromStream(istream& is)
{

}

void OrderTest::receive(int iCID)
{
    int iUpdateIndex = -1;

    for(unsigned int i = 0;i < vContractQuoteDatas.size(); i++)
    {
        if(vContractQuoteDatas[i]->iCID == iCID)
        {
            iUpdateIndex = i;
            break;
        }
    }
/*    
    cout << to_simple_string(vContractQuoteDatas[iUpdateIndex]->cControlUpdateTime) << "|"
         << vContractQuoteDatas[iUpdateIndex]->iBidSize << "|"
         << vContractQuoteDatas[iUpdateIndex]->dBestBid << "|"
         << vContractQuoteDatas[iUpdateIndex]->dBestAsk << "|"
         << vContractQuoteDatas[iUpdateIndex]->iAskSize << "\n";
*/
}

void OrderTest::wakeup(KOEpochTime cCallTime)
{
cerr << "in OrderTest::wakeup call time is " << cCallTime.igetPrintable() << "\n";
    if(cCallTime == _cTradingStartTime + KOEpochTime(2157, 0))
    {
        cout << cCallTime.igetPrintable() << " 1st wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 1 submitted \n";
        }
        else
        {
            cout << "failed to submit order 1\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 2 submitted \n";
        }
        else
        {
            cout << "failed to submit order 2\n";
        }


        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 3 submitted \n";
        }
        else
        {
            cout << "failed to submit order 3\n";
        }


        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 4 submitted \n";
        }
        else
        {
            cout << "failed to submit order 4\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 5 submitted \n";
        }
        else
        {
            cout << "failed to submit order 5\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 6 submitted \n";
        }
        else
        {
            cout << "failed to submit order 6\n";
        }
    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2159, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 10 submitted \n";
        }
        else
        {
            cout << "failed to submit order 10\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 11 submitted \n";
        }
        else
        {
            cout << "failed to submit order 11\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 12 submitted \n";
        }
        else
        {
            cout << "failed to submit order 12\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 13 submitted \n";
        }
        else
        {
            cout << "failed to submit order 13\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2160, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 14 submitted \n";
        }
        else
        {
            cout << "failed to submit order 14\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 15 submitted \n";
        }
        else
        {
            cout << "failed to submit order 15\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 16 submitted \n";
        }
        else
        {
            cout << "failed to submit order 16\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 17 submitted \n";
        }
        else
        {
            cout << "failed to submit order 17\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2161, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 18 submitted \n";
        }
        else
        {
            cout << "failed to submit order 18\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 19 submitted \n";
        }
        else
        {
            cout << "failed to submit order 19\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 20 submitted \n";
        }
        else
        {
            cout << "failed to submit order 20\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 21 submitted \n";
        }
        else
        {
            cout << "failed to submit order 21\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2162, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 22 submitted \n";
        }
        else
        {
            cout << "failed to submit order 22\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 23 submitted \n";
        }
        else
        {
            cout << "failed to submit order 23\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 24 submitted \n";
        }
        else
        {
            cout << "failed to submit order 24\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 25 submitted \n";
        }
        else
        {
            cout << "failed to submit order 25\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2163, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 26 submitted \n";
        }
        else
        {
            cout << "failed to submit order 26\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 26 submitted \n";
        }
        else
        {
            cout << "failed to submit order 26\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 27 submitted \n";
        }
        else
        {
            cout << "failed to submit order 27\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 28 submitted \n";
        }
        else
        {
            cout << "failed to submit order 28\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2164, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 29 submitted \n";
        }
        else
        {
            cout << "failed to submit order 29\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 30 submitted \n";
        }
        else
        {
            cout << "failed to submit order 30\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 31 submitted \n";
        }
        else
        {
            cout << "failed to submit order 31\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 32 submitted \n";
        }
        else
        {
            cout << "failed to submit order 32\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2165, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 33 submitted \n";
        }
        else
        {
            cout << "failed to submit order 33\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 34 submitted \n";
        }
        else
        {
            cout << "failed to submit order 34\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 35 submitted \n";
        }
        else
        {
            cout << "failed to submit order 35\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 36 submitted \n";
        }
        else
        {
            cout << "failed to submit order 36\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2166, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 37 submitted \n";
        }
        else
        {
            cout << "failed to submit order 37\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 38 submitted \n";
        }
        else
        {
            cout << "failed to submit order 38\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 39 submitted \n";
        }
        else
        {
            cout << "failed to submit order 39\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 40 submitted \n";
        }
        else
        {
            cout << "failed to submit order 40\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2167, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 41 submitted \n";
        }
        else
        {
            cout << "failed to submit order 41\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 42 submitted \n";
        }
        else
        {
            cout << "failed to submit order 42\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 43 submitted \n";
        }
        else
        {
            cout << "failed to submit order 43\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 44 submitted \n";
        }
        else
        {
            cout << "failed to submit order 44\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2168, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 45 submitted \n";
        }
        else
        {
            cout << "failed to submit order 45\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 46 submitted \n";
        }
        else
        {
            cout << "failed to submit order 46\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 47 submitted \n";
        }
        else
        {
            cout << "failed to submit order 47\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 48 submitted \n";
        }
        else
        {
            cout << "failed to submit order 48\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2169, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 49 submitted \n";
        }
        else
        {
            cout << "failed to submit order 49\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 50 submitted \n";
        }
        else
        {
            cout << "failed to submit order 50\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 51 submitted \n";
        }
        else
        {
            cout << "failed to submit order 51\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 52 submitted \n";
        }
        else
        {
            cout << "failed to submit order 52\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2170, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 53 submitted \n";
        }
        else
        {
            cout << "failed to submit order 53\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 54 submitted \n";
        }
        else
        {
            cout << "failed to submit order 54\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 55 submitted \n";
        }
        else
        {
            cout << "failed to submit order 55\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 56 submitted \n";
        }
        else
        {
            cout << "failed to submit order 56\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2171, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 57 submitted \n";
        }
        else
        {
            cout << "failed to submit order 57\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 58 submitted \n";
        }
        else
        {
            cout << "failed to submit order 58\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 59 submitted \n";
        }
        else
        {
            cout << "failed to submit order 59\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 60 submitted \n";
        }
        else
        {
            cout << "failed to submit order 60\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2172, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 61 submitted \n";
        }
        else
        {
            cout << "failed to submit order 61\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 62 submitted \n";
        }
        else
        {
            cout << "failed to submit order 62\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 63 submitted \n";
        }
        else
        {
            cout << "failed to submit order 63\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 64 submitted \n";
        }
        else
        {
            cout << "failed to submit order 64\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2173, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 65 submitted \n";
        }
        else
        {
            cout << "failed to submit order 65\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 66 submitted \n";
        }
        else
        {
            cout << "failed to submit order 66\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 67 submitted \n";
        }
        else
        {
            cout << "failed to submit order 67\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 68 submitted \n";
        }
        else
        {
            cout << "failed to submit order 68\n";
        }

    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2174, 0))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " submitting sell order with size 15 \n";

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 69 submitted \n";
        }
        else
        {
            cout << "failed to submit order 69\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 70 submitted \n";
        }
        else
        {
            cout << "failed to submit order 70\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 71 submitted \n";
        }
        else
        {
            cout << "failed to submit order 71\n";
        }

        pOrder = vContractAccount[0]->psubmitOrder(-10, 163.90);
        if(pOrder.get())
        {
            cout << "Order 72 submitted \n";
        }
        else
        {
            cout << "failed to submit order 72\n";
        }

    }



/*
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2158, 498))
    {
        cout << cCallTime.igetPrintable() << " 2nd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " amend sell order with new price 163.91 \n";

        vContractAccount[0]->bchangeOrderPrice(pOrder, 163.91);

        if(pOrder.get())
        {
            cout << "Order amended \n";
        }
        else
        {
            cout << "failed to amend order \n";
        }
    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2168, 755))
    {
        cout << cCallTime.igetPrintable() << " 3rd wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " amend sell order with new price 163.90 \n";

        vContractAccount[0]->bchangeOrderPrice(pOrder, 163.90);

        if(pOrder.get())
        {
            cout << "Order amended \n";
        }
        else
        {
            cout << "failed to amend order \n";
        }
    }
    else if(cCallTime == _cTradingStartTime + KOEpochTime(2168, 756))
    {
        cout << cCallTime.igetPrintable() << " 4th wake up call for " << _sEngineSlotName << "\n";
        cout << cCallTime.igetPrintable() << " amend sell order with new price 163.89 \n";

        vContractAccount[0]->bchangeOrderPrice(pOrder, 163.89);

        if(pOrder.get())
        {
            cout << "Order amended \n";
        }
        else
        {
            cout << "failed to amend order \n";
        }
    }
*/
}

void OrderTest::orderConfirmHandler(int iOrderID)
{
    cout << "Order " << iOrderID << " confirmed \n";
}

void OrderTest::orderFillHandler(int iOrderID, long iFilledQty, double dPrice)
{
    cout << "Order " << iOrderID << " filled. qty " << iFilledQty << " price " << dPrice << "\n";
}

void OrderTest::orderRejectHandler(int iOrderID)
{
    cout << "Order " << iOrderID << " rejected \n";
}

void OrderTest::orderDeleteHandler(int iOrderID)
{
    cout << "Order " << iOrderID << " deleted \n";
}

void OrderTest::orderDeleteRejectHandler(int iOrderID)
{
    cout << "Order " << iOrderID << " delete request rejected \n";
}

void OrderTest::orderAmendRejectHandler(int iOrderID)
{
    cout << "Order " << iOrderID << " amend request rejected \n";
}

}
