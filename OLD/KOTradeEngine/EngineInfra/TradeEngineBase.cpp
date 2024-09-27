#include "TradeEngineBase.h"
#include "ContractAccount.h"
#include "ErrorHandler.h"
#include "SystemClock.h"

using std::stringstream;
using std::cerr;

namespace KO
{

TradeEngineBase::TradeEngineBase(const string& sEngineRunTimePath,
                                 const string& sEngineType,
                                 const string& sEngineSlotName,
                                 KOEpochTime cTradingStartTime,
                                 KOEpochTime cTradingEndTime,
                                 SchedulerBase* pScheduler,
                                string sTodayDate,
                                PositionServerConnection* pPositionServerConnection)
:_pScheduler(pScheduler),
 _sTodayDate(sTodayDate),
 _sEngineRunTimePath(sEngineRunTimePath),
 _sEngineType(sEngineType),
 _sEngineSlotName(sEngineSlotName),
 _cTradingStartTime(cTradingStartTime),
 _cTradingEndTime(cTradingEndTime),
 _iNumOrderCriticalErrorSeen(0),
 _bLiveOrderAtTradingEndTime(false),
 _bManualTradingOveride(false),
 _pPositionServerConnection(pPositionServerConnection)
{

}

TradeEngineBase::~TradeEngineBase()
{

}

void TradeEngineBase::receive(int iCID)
{

}

void TradeEngineBase::assignStartupPosition(string sProduct, string sAccount, long iPosition, bool bTimeout)
{
    for(unsigned int i = 0; i != vContractAccount.size(); i++)
    {
        if(vContractAccount[i]->sgetAccountProductName().compare(sProduct) == 0 &&
           vContractAccount[i]->sgetAccountName().compare(sAccount) == 0)
        {
            if(bTimeout == false)
            {
                vContractAccount[i]->setInitialPosition(iPosition);

                stringstream cStringStream;
                cStringStream << "Received start up position " << iPosition << " for account " << sAccount << " product " << sProduct;
                ErrorHandler::GetInstance()->newInfoMsg("0", sAccount, sProduct, cStringStream.str());
            }
            else
            {
                _pPositionServerConnection->requestStartupPosition(vContractAccount[i]->sgetAccountProductName(), vContractAccount[i]->sgetAccountName(), this);
            }

            break;
        }
    }
}

void TradeEngineBase::dayInit()
{
    _eEngineState = INIT;
}

void TradeEngineBase::dayRun()
{
    if(_bManualTradingOveride == false)
    {
        _eEngineState = RUN;
    }

    _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|DayRun|Acutal scheduled trading start time is " << _cTradingStartTime.igetPrintable() << "\n";

    if(SystemClock::GetInstance()->cgetCurrentKOEpochTime() < _cTradingStartTime + KOEpochTime(120,0))
    {
        for(unsigned int i = 0;i != vContractAccount.size(); i++)
        {
            vContractAccount[i]->setInitialPosition(0);
        }

        _cLogger << "Engine started normally. Initialising position to 0. \n";
    }
    else
    {
        for(unsigned int i = 0;i != vContractAccount.size(); i++)
        {
            _pPositionServerConnection->requestStartupPosition(vContractAccount[i]->sgetAccountProductName(), vContractAccount[i]->sgetAccountName(), this);
        }

        _cLogger << "Engine started intraday. Requesting postion from position server \n";
    }
}

void TradeEngineBase::dayStop()
{
    _eEngineState = STOP;

    string sTransactionFileName = _sEngineRunTimePath + "TransactionsLog" + _sTodayDate + ".out";
    stringstream cStringStream;
    cStringStream.precision(10);

    cStringStream << "TIME;PRODUCT;QTY;PRICE\n";

    vector< boost::shared_ptr<Trade> > vTotalTrades;
    long iTotalyDailyOrderActionCount = 0;

    for(unsigned int i = 0;i != vContractAccount.size(); i++)
    {
        vContractAccount[i]->retrieveDailyTrades(vTotalTrades);
        iTotalyDailyOrderActionCount = vContractAccount[i]->igetDailyOrderActionCount() + iTotalyDailyOrderActionCount;
    }

    compareTradeByTime cCompareTradeByTime;
    std::sort(vTotalTrades.begin(), vTotalTrades.end(), cCompareTradeByTime);

    for(unsigned int i = 0; i != vTotalTrades.size(); i++)
    {
        string sTimeStamp = SystemClock::GetInstance()->sToSimpleString(vTotalTrades[i]->cTradeTime);

        cStringStream << sTimeStamp << ";"
                      << vTotalTrades[i]->sProduct << ";"
                      << vTotalTrades[i]->iQty << ";"
                      << vTotalTrades[i]->dPrice << "\n";
    }

    fstream fsTransactionLogFile;
    const unsigned int length = 50000;
    char buffer[length];
    fsTransactionLogFile.rdbuf()->pubsetbuf(buffer, length);

    fsTransactionLogFile.open(sTransactionFileName.c_str(), fstream::out);

    if(fsTransactionLogFile.is_open())
    {
        fsTransactionLogFile.precision(10);
        fsTransactionLogFile << cStringStream.str();
        fsTransactionLogFile.close();
    }
    else
    {
        cerr << "Error Failed to create TransactionsLog" + _sTodayDate + ".out file \n";
    }

    string sOrderActioniStatFileName = _sEngineRunTimePath + "OrderActionStat.out";
    map<string, long> mDailyOrderActionCount;
    fstream ifsOrderActionStatFile;
    ifsOrderActionStatFile.open(sOrderActioniStatFileName.c_str(), fstream::in);
    if(ifsOrderActionStatFile.is_open())
    {
        while(!ifsOrderActionStatFile.eof())
        {
            char sNewLine[512];
            ifsOrderActionStatFile.getline(sNewLine, sizeof(sNewLine));

            if(strlen(sNewLine) != 0 && sNewLine[0] != '#')
            {
                std::istringstream cDailyOrderActionStatStream(sNewLine);
                string sElement;

                boost::gregorian::date cDate;
                long iOrderActionCount;

                std::getline(cDailyOrderActionStatStream, sElement, ';');
                try
                {
                    cDate = boost::gregorian::from_undelimited_string(sElement);
                }
                catch (...)
                {
                    cerr << "Error in OrderActionStat.out file on date " << _sTodayDate << " invalid date: " << sElement << "\n";
                }

                std::getline(cDailyOrderActionStatStream, sElement, ';');
                iOrderActionCount = atoi(sElement.c_str());
               
                mDailyOrderActionCount[_sTodayDate] = iOrderActionCount; 
            }
        }
    }

    mDailyOrderActionCount[_sTodayDate] = iTotalyDailyOrderActionCount;

    fstream ofsOrderActionStatFile;
    ofsOrderActionStatFile.rdbuf()->pubsetbuf(buffer, length);
    ofsOrderActionStatFile.open(sOrderActioniStatFileName.c_str(), fstream::out);
    if(ofsOrderActionStatFile.is_open())
    {
        for(map<string, long>::iterator itr = mDailyOrderActionCount.begin();
            itr != mDailyOrderActionCount.end();
            ++itr)
        {
            ofsOrderActionStatFile << itr->first << ";" << itr->second << "\n";
        }

        ofsOrderActionStatFile.close();
    }
}

void TradeEngineBase::orderCriticalErrorHandler(int iOrderID)
{
    if(_iNumOrderCriticalErrorSeen < 75)
    {
        _iNumOrderCriticalErrorSeen = _iNumOrderCriticalErrorSeen + 1;

        if(_iNumOrderCriticalErrorSeen >= 75)
        {
            manualHaltTrading();

            stringstream cStringStream;
            cStringStream << _iNumOrderCriticalErrorSeen << " external order critical errors received. " << _sEngineSlotName << " in halt mode.";
            ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, vContractQuoteDatas[0]->sProduct, cStringStream.str());
        }
    }
}

bool TradeEngineBase::bisTrading()
{
    return (_eEngineState != INIT && _eEngineState != STOP);
}

bool TradeEngineBase::bisLiveTrading()
{
    return _pScheduler->bisLiveTrading();
}

const string& TradeEngineBase::sgetEngineSlotName()
{
    return _sEngineSlotName;
}

KOEpochTime TradeEngineBase::cgetTradingStartTime()
{
    return _cTradingStartTime;
}

KOEpochTime TradeEngineBase::cgetTradingEndTime()
{
    return _cTradingEndTime;
}

void TradeEngineBase::registerProductForFigure(string sProductName)
{
    _vRegisteredFigureProducts.push_back(sProductName);
}

vector<string> TradeEngineBase::vgetRegisterdFigureProducts()
{
    return _vRegisteredFigureProducts;
}

void TradeEngineBase::checkEngineState(KOEpochTime cCallTime)
{
    for(unsigned int i = 0; i != vContractAccount.size(); i++)
    {
        vContractAccount[i]->checkOrderStatus();
    }

    if((cCallTime.sec() - _cTradingStartTime.sec()) % 30 == 0)
    {
        for(unsigned int i = 0; i != vContractAccount.size(); i++)
        {
            double dPrevPnL = vContractAccount[i]->dGetLastPnL();
            double dCurrentPnL = vContractAccount[i]->dGetCurrentPnL(1);
            long iTradingVolume = vContractAccount[i]->iGetCurrentVolume();
            double iPosition = vContractAccount[i]->igetCurrentPosition();        

            stringstream cStringStream;
            cStringStream << "PNL: " << dCurrentPnL << " Volume: " << iTradingVolume << " PNL Diff: " << dCurrentPnL - dPrevPnL << " Position: " << iPosition;
            ErrorHandler::GetInstance()->newInfoMsg("HB", _sEngineSlotName, vContractQuoteDatas[i]->sProduct, cStringStream.str());
        }
    }

    if(cCallTime > _cTradingEndTime - KOEpochTime(240,0))
    {
        if(_bLiveOrderAtTradingEndTime == false)
        {
            for(unsigned int i = 0; i != vContractAccount.size(); i++)
            {
                if(vContractAccount[i]->pgetAllLiveOrders()->size() != 0)
                {
                    _bLiveOrderAtTradingEndTime = true;    

                    stringstream cStringStream;
                    cStringStream << "Live orders found towards to trading end time!";
                    ErrorHandler::GetInstance()->newErrorMsg("0", _sEngineSlotName, vContractQuoteDatas[i]->sProduct, cStringStream.str());
                }
            }
        }
    }
}

bool TradeEngineBase::bcheckDataStaleness(int iCID)
{
    bool bResult = false;

    return bResult;
}

void TradeEngineBase::manualResumeTrading()
{
    _cLogger << "manual resume trading signal received \n";
    _iNumOrderCriticalErrorSeen = 0;
    _bManualTradingOveride = false;
    resumeTrading();
}

void TradeEngineBase::manualHaltTrading()
{
    _cLogger << "manual halt trading signal received \n";
    _bManualTradingOveride = true;
    haltTrading();
}

void TradeEngineBase::manualPatientLiquidation()
{
    _cLogger << "manual patient signal received \n";
    _bManualTradingOveride = true;
    patientLiquidation();
}

void TradeEngineBase::manualLimitLiquidation()
{
    _cLogger << "manual limit liquidation signal received \n";
    _bManualTradingOveride = true;
    limitLiquidation();
}

void TradeEngineBase::manualFastLiquidation()
{
    _cLogger << "manual fast liquidation signal received \n";
    _bManualTradingOveride = true;
    fastLiquidation();
}

void TradeEngineBase::autoResumeTrading()
{
    if(_bManualTradingOveride == false)
    {
        resumeTrading();
    }
}

void TradeEngineBase::autoHaltTrading()
{
    if(_bManualTradingOveride == false)
    {
        haltTrading();
    }
}

void TradeEngineBase::autoPatientLiquidation()
{
    if(_bManualTradingOveride == false)
    {
        patientLiquidation();
    }
}

void TradeEngineBase::autoLimitLiquidation()
{
    if(_bManualTradingOveride == false)
    {
        limitLiquidation();
    }
}

void TradeEngineBase::autoFastLiquidation()
{
    if(_bManualTradingOveride == false)
    {
        fastLiquidation();
    }
}

void TradeEngineBase::resumeTrading()
{
    _eEngineState = RUN;

    for(unsigned int i = 0; i != vContractAccount.size(); i++)
    {
        vContractAccount[i]->resumeTrading();
    }
}

void TradeEngineBase::patientLiquidation()
{
    _eEngineState = PAT_LIQ;

    for(unsigned int i = 0; i != vContractAccount.size(); i++)
    {
        vContractAccount[i]->resumeTrading();
    }
}

void TradeEngineBase::haltTrading()
{
    _eEngineState = HALT;

    for(unsigned int i = 0; i != vContractAccount.size(); i++)
    {
        vContractAccount[i]->haltTrading();
    }
}

void TradeEngineBase::limitLiquidation()
{
    _eEngineState = LIM_LIQ;

    for(unsigned int i = 0; i != vContractAccount.size(); i++)
    {
        vContractAccount[i]->resumeTrading();
    }
}

void TradeEngineBase::fastLiquidation()
{
    _eEngineState = FAST_LIQ;

    for(unsigned int i = 0; i != vContractAccount.size(); i++)
    {
        vContractAccount[i]->resumeTrading();
    }
}

}
