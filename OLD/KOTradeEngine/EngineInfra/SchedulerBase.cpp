#include <fstream>
#include <signal.h>

#include "SchedulerBase.h"
#include "ErrorHandler.h"
#include "OrderRiskChecker.h"
#include "ContractAccount.h"
#include "SystemClock.h"
#include "WakeupEvent.h"
#include "FigureEvent.h"
#include "EngineEvent.h"
#include "../GridDataGenerator/GridDataGenerator.h"
#include "../slsl/SLSL.h"
#include "../sl3lEqualWeight/SL3LEqualWeight.h"
#include "../Liquidator/Liquidator.h"
#include "../OrderTest/OrderTest.h"
#include "../DataPrinter/DataPrinter.h"

#include <boost/math/special_functions/round.hpp>

using std::cout;
using std::cerr;
using std::stringstream;
using std::ifstream;

namespace KO
{

SchedulerBase* SchedulerBase::_pInstance = NULL;

SchedulerBase::SchedulerBase(SchedulerConfig &cfg)
:_cSchedulerCfg(cfg)
{
    cout.precision(20);
    cerr.precision(20);

    _cActualStartedTime = KOEpochTime(0,0);

    _pInstance = this;
}

SchedulerBase::~SchedulerBase()
{

}

void SchedulerBase::setEngineManualHalted(bool bNewValue)
{
    _bEngineManualHalted = bNewValue;
}

bool SchedulerBase::bgetEngineManualHalted()
{
    return _bEngineManualHalted;
}

string SchedulerBase::sgetRootFromKOSymbol(const string& sKOSymbol)
{
    long sSymbolLength = sKOSymbol.length();
    return sKOSymbol.substr(0, sSymbolLength - 2);
}

bool SchedulerBase::preCommonInit()
{
    bool bResult = true;

    SystemClock::GetInstance()->init(_cSchedulerCfg.sDate, this);

    ErrorHandler::GetInstance()->init(bisLiveTrading(), _cSchedulerCfg.sErrorWarningLogLevel);

    bResult = _cPositionServerConnection.bInit(_cSchedulerCfg.sPositionServerAddress, this);

    _pStaticDataHandler.reset(new StaticDataHandler(_cSchedulerCfg.sFXRateFile, _cSchedulerCfg.sProductSpecFile, _cSchedulerCfg.sTickSizeFile, _cSchedulerCfg.sDate));

    string sHour;
    string sMinute;
    string sSecond;
    string sMilliSecond;

    sHour = _cSchedulerCfg.sShutDownTime.substr (0,2);
    sMinute = _cSchedulerCfg.sShutDownTime.substr (2,2);
    sSecond = _cSchedulerCfg.sShutDownTime.substr (4,2);
    _cShutDownTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(_cSchedulerCfg.sDate, sHour, sMinute, sSecond);
    _bShutDownTimeReached = false;

    if(!bisLiveTrading() && _cSchedulerCfg.sErrorWarningLogLevel.compare("INFO") == 0)
    {
        _cOrderActionLogger.openFile("OrderActionLog.out", true, false);
    }
    else
    {
        _cOrderActionLogger.openFile("OrderActionLog.out", false, false);
    }

    if(_cSchedulerCfg.bLogMarketData)
    {
        _cMarketDataLogger.openFile("MarketDataLog.out", true, true);
    }
    else
    {
        _cMarketDataLogger.openFile("MarketDataLog.out", false, true);
    }    

    registerSignalHandlers();

    return bResult;
}

bool SchedulerBase::postCommonInit()
{
    bool bResult = true;

    loadFigureFile();
    loadProductFigureActionFile();

    loadScheduledManualActionsFile();
    loadScheduledSlotLiquidationFile();

    OrderRiskChecker::GetInstance()->assignScheduler(this);

    registerTradeEngines();   

    loadTodaysFigure(_cSchedulerCfg.sDate); 

    for(vector<TradeEngineBasePtr>::iterator itr = _vTradeEngines.begin(); itr != _vTradeEngines.end(); itr++)
    {
        (*itr)->dayInit();
    }

    sortTimeEvent();

    return bResult;
}

QuoteData* SchedulerBase::pregisterProduct(string sFullProductName, InstrumentType eInstrumentType)
{
    QuoteData* pNewQuoteDataPtr (new QuoteData);

    std::size_t iDotPos = sFullProductName.find(".");

    if(iDotPos == std::string::npos)
    {
        pNewQuoteDataPtr->sExchange = "";
        pNewQuoteDataPtr->sProduct = sFullProductName;
    }
    else
    {
        pNewQuoteDataPtr->sExchange = sFullProductName.substr(0, iDotPos);
        pNewQuoteDataPtr->sProduct = sFullProductName.substr(iDotPos+1);
    }

    pNewQuoteDataPtr->eInstrumentType = eInstrumentType;

    pNewQuoteDataPtr->sRoot = _pStaticDataHandler->sGetRootSymbol(sFullProductName, eInstrumentType);

    string sCurreny = _pStaticDataHandler->sGetCurrency(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange);
    string sContractDollarFXPair = sCurreny + "USD";
    pNewQuoteDataPtr->dRateToDollar = _pStaticDataHandler->dGetFXRate(sContractDollarFXPair);
    pNewQuoteDataPtr->dTickSize = _pStaticDataHandler->dGetTickSize(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange);

    pNewQuoteDataPtr->dTradingFee = _pStaticDataHandler->dGetTradingFee(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange);
    pNewQuoteDataPtr->dContractSize = _pStaticDataHandler->dGetContractSize(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange);
    pNewQuoteDataPtr->sHCExchange = _pStaticDataHandler->sGetHCExchange(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange);

    if(_pStaticDataHandler->sGetProductTyep(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange) == "FUTURE")
    {
        pNewQuoteDataPtr->iMaxSpreadWidth = 5;
    }
    else if(_pStaticDataHandler->sGetProductTyep(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange) == "FX")
    {
        pNewQuoteDataPtr->iMaxSpreadWidth = 50;
    }

    pNewQuoteDataPtr->iBidSize = 0;
    pNewQuoteDataPtr->iAskSize = 0;
    pNewQuoteDataPtr->dBestBid = 0;
    pNewQuoteDataPtr->dBestAsk = 0;
    pNewQuoteDataPtr->iBestBidInTicks = 0;
    pNewQuoteDataPtr->iBestAskInTicks = 0;
    pNewQuoteDataPtr->dLastTradePrice = 0;
    pNewQuoteDataPtr->dClose = 0;
    pNewQuoteDataPtr->dRefPrice = 0;
    pNewQuoteDataPtr->iTradeSize = 0;
    pNewQuoteDataPtr->iAccumuTradeSize = 0;

    pNewQuoteDataPtr->bPriceValid = true;
    pNewQuoteDataPtr->bPriceInvalidTriggered = false;
    pNewQuoteDataPtr->cPriceInvalidTime = KOEpochTime();

    pNewQuoteDataPtr->cMarketOpenTime = _pStaticDataHandler->cGetMarketOpenTime(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange);
    pNewQuoteDataPtr->cMarketCloseTime = _pStaticDataHandler->cGetMarketCloseTime(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange);

    _vContractQuoteDatas.push_back(pNewQuoteDataPtr);
    _cProductEngineMap.push_back(vector<TradeEngineBasePtr>());

    stringstream cStringStream;
    cStringStream << "Registered product " << pNewQuoteDataPtr->sProduct << ".";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", pNewQuoteDataPtr->sProduct, cStringStream.str());

    return pNewQuoteDataPtr;
}

void SchedulerBase::newPriceUpdate(long iProductIndex)
{
    for(vector<TradeEngineBasePtr>::iterator itr = _cProductEngineMap[iProductIndex].begin();
        itr != _cProductEngineMap[iProductIndex].end();
        itr++)
    {
        if((*itr)->bisTrading())
        {
            (*itr)->receive(_vContractQuoteDatas[iProductIndex]->iCID);
        }
    }

    if(_cSchedulerCfg.bLogMarketData)
    {
        _cMarketDataLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() 
                           << "|" << _vContractQuoteDatas[iProductIndex]->sProduct 
                           << "|" << _vContractQuoteDatas[iProductIndex]->iBidSize
                           << "|" << _vContractQuoteDatas[iProductIndex]->iBestBidInTicks
                           << "|" << _vContractQuoteDatas[iProductIndex]->iBestAskInTicks
                           << "|" << _vContractQuoteDatas[iProductIndex]->iAskSize
                           << "|" << _vContractQuoteDatas[iProductIndex]->iLastTradeInTicks
                           << "|" << _vContractQuoteDatas[iProductIndex]->iAccumuTradeSize << "\n";
    }
}

void SchedulerBase::orderConfirmed(KOOrderPtr pOrder)
{
    pOrder->changeOrderstat(KOOrder::ACTIVE);
    pOrder->_pParent->orderConfirmHandler(pOrder->igetOrderID());
}

void SchedulerBase::orderDeleted(KOOrderPtr pOrder)
{
    pOrder->changeOrderstat(KOOrder::INACTIVE);
    pOrder->_pParent->orderDeleteHandler(pOrder->igetOrderID());
}

void SchedulerBase::orderDeletedUnexpectedly(KOOrderPtr pOrder)
{
    pOrder->changeOrderstat(KOOrder::INACTIVE);
    pOrder->_pParent->orderUnexpectedDeleteHandler(pOrder->igetOrderID());
}

void SchedulerBase::orderConfirmedUnexpectedly(KOOrderPtr pOrder)
{
    pOrder->changeOrderstat(KOOrder::ACTIVE);
    pOrder->_pParent->orderUnexpectedConfirmHandler(pOrder->igetOrderID());
}

void SchedulerBase::orderFilled(KOOrderPtr pOrder, long iQty, double dPrice)
{
    pOrder->_pParent->orderFillHandler(pOrder->igetOrderID(), iQty, dPrice);
    
    _cPositionServerConnection.newFill(pOrder->sgetOrderProductName(), pOrder->sgetOrderAccount(), iQty);
}

void SchedulerBase::orderAmendRejected(KOOrderPtr pOrder)
{
    pOrder->changeOrderstat(KOOrder::ACTIVE);
    pOrder->_pParent->orderAmendRejectHandler(pOrder->igetOrderID());
}

void SchedulerBase::orderDeleteRejected(KOOrderPtr pOrder)
{
    pOrder->changeOrderstat(KOOrder::ACTIVE);
    pOrder->_pParent->orderDeleteRejectHandler(pOrder->igetOrderID());
}

void SchedulerBase::orderRejected(KOOrderPtr pOrder)
{
    pOrder->changeOrderstat(KOOrder::INACTIVE);
    pOrder->_pParent->orderRejectHandler(pOrder->igetOrderID());
}

void SchedulerBase::wakeup(KOEpochTime cCallTime)
{
    if(_cActualStartedTime == KOEpochTime(0,0))
    {
        _cActualStartedTime = cCallTime;
    }

    if(cCallTime >= _cShutDownTime)
    {
        if(_bShutDownTimeReached == false)
        {
            _bShutDownTimeReached = true;

            for(vector<TradeEngineBasePtr>::iterator itr = _vTradeEngines.begin(); itr != _vTradeEngines.end(); ++itr)
            {
                if((*itr)->_eEngineState != TradeEngineBase::STOP)
                {
                    (*itr)->dayStop();
                }
            }

            stringstream cStringStream;
            cStringStream << "Engine shut down time reached";
            ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

            exit(0);
        }
    }
    else
    {
        _cPositionServerConnection.wakeup(cCallTime);

        for(unsigned int i = 0; i < _vContractQuoteDatas.size(); ++i)
        {
            if(cCallTime > _cActualStartedTime + KOEpochTime(90,0) &&
               cCallTime > _vContractQuoteDatas[i]->cMarketOpenTime + KOEpochTime(90,0) &&
               cCallTime < _vContractQuoteDatas[i]->cMarketCloseTime)
            {
                if(_vContractQuoteDatas[i]->iBidSize == 0 ||
                   _vContractQuoteDatas[i]->iAskSize == 0 ||
                   _vContractQuoteDatas[i]->iBestAskInTicks - _vContractQuoteDatas[i]->iBestBidInTicks <= 0)
                {
                    if(_vContractQuoteDatas[i]->bPriceValid == true)
                    {
                        _vContractQuoteDatas[i]->bPriceValid = false;
                        _vContractQuoteDatas[i]->cPriceInvalidTime = cCallTime;
                    }
                    else
                    {
                        if((cCallTime - _vContractQuoteDatas[i]->cPriceInvalidTime).sec() > 100)
                        {
                            if(_vContractQuoteDatas[i]->bPriceInvalidTriggered == false)
                            {
                                _vContractQuoteDatas[i]->bPriceInvalidTriggered = true;

                                stringstream cStringStream;
                                cStringStream << "Invalid price for more than 100 seconds " << _vContractQuoteDatas[i]->iBidSize << "|" << _vContractQuoteDatas[i]->dBestBid << "|" << _vContractQuoteDatas[i]->iBestBidInTicks << "|" << _vContractQuoteDatas[i]->iBestAskInTicks << "|" << _vContractQuoteDatas[i]->dBestAsk << "|" << _vContractQuoteDatas[i]->iAskSize;
                                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", _vContractQuoteDatas[i]->sProduct, cStringStream.str());
                            }
                        }
                    }
                }
                else
                {
                    if(_vContractQuoteDatas[i]->bPriceValid == false)
                    {
                        _vContractQuoteDatas[i]->bPriceValid = true;
                        _vContractQuoteDatas[i]->bPriceInvalidTriggered = false;
                        _vContractQuoteDatas[i]->cPriceInvalidTime = KOEpochTime();

                        stringstream cStringStream;
                        cStringStream << "Price recovered from invalid state";
                        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _vContractQuoteDatas[i]->sProduct, cStringStream.str());
                    }
                }
            }
        }

        for(vector<TradeEngineBasePtr>::iterator itr = _vTradeEngines.begin(); itr != _vTradeEngines.end(); ++itr)
        {
            (*itr)->checkEngineState(cCallTime);
        }

        for(vector< pair<KOEpochTime, vector<string> > >::iterator scheduledSlotLiquidationItr = _vScheduledSlotLiquidation.begin();scheduledSlotLiquidationItr != _vScheduledSlotLiquidation.end();)
        {
            if(scheduledSlotLiquidationItr->first <= cCallTime)
            {
                for(vector<string>::iterator slotItr = scheduledSlotLiquidationItr->second.begin();
                    slotItr != scheduledSlotLiquidationItr->second.end();
                    slotItr++)
                {
                    for(vector<TradeEngineBasePtr>::iterator itr = _vTradeEngines.begin(); itr != _vTradeEngines.end(); itr++)
                    {
                        if((*itr)->_sEngineType == "Liquidator")
                        {
                            (*itr)->manualResumeTrading();
                            Liquidator* pLiquidator = dynamic_cast<Liquidator*>((*itr).get());
                            pLiquidator->liquidateSlot(*slotItr);
                        }
                        else
                        {
                            if((*itr)->sgetEngineSlotName() == *slotItr)
                            {
                                (*itr)->manualHaltTrading();
                            }
                        }
                    }
                }

                scheduledSlotLiquidationItr = _vScheduledSlotLiquidation.erase(scheduledSlotLiquidationItr);
            }
            else
            {
                scheduledSlotLiquidationItr++;
            }
        }

        for(vector< pair<KOEpochTime, ManualCommandAction> >::iterator scheduledManualActionItr = _vScheduledManualActions.begin();scheduledManualActionItr != _vScheduledManualActions.end();)
        {
            if(scheduledManualActionItr->first <= cCallTime)
            {
                if(scheduledManualActionItr->second == RESUME_ALL)
                {
                    resumeAllTraders(0);
                }
                else if(scheduledManualActionItr->second == PATIENT_ALL)
                {
                    patientLiqAllTraders(0);
                }
                else if(scheduledManualActionItr->second == LIMIT_ALL)
                {
                    limLiqAllTraders(0);
                }
                else if(scheduledManualActionItr->second == FAST_ALL)
                {
                    fastLiqAllTraders(0);
                }
                else if(scheduledManualActionItr->second == HALT_ALL)
                {
                    haltAllTraders(0);
                }
                else if(scheduledManualActionItr->second == LIMIT_ALL_LIQUIDATOR)
                {
                    limitLiqAllLiquidator(0);
                }
                else if(scheduledManualActionItr->second == FAST_ALL_LIQUIDATOR)
                {
                    fastLiqAllLiquidator(0);
                }
                else if(scheduledManualActionItr->second == LIMIT_SLOTS_LIQUIDATOR)
                {
                    limitLiqSlotsLiquidator(0);
                }
                else if(scheduledManualActionItr->second == FAST_SLOTS_LIQUIDATOR)
                {
                    fastLiqSlotsLiquidator(0);
                }
                else if(scheduledManualActionItr->second == OFF_LIQUIDATOR)
                {
                    offLiquidator(0);
                }

                scheduledManualActionItr = _vScheduledManualActions.erase(scheduledManualActionItr);
            }
            else
            {
                scheduledManualActionItr++;
            }
        }

        _cMarketDataLogger.flush();
        _cOrderActionLogger.flush();
        ErrorHandler::GetInstance()->flush();
    }
}

void SchedulerBase::registerTradeEngines()
{
    if(_cSchedulerCfg.bRandomiseConfigs == true)
    {
        std::random_shuffle (_cSchedulerCfg.vTraderConfigs.begin(), _cSchedulerCfg.vTraderConfigs.end());
    }

    for(vector<string>::iterator it = _cSchedulerCfg.vTraderConfigs.begin();
        it != _cSchedulerCfg.vTraderConfigs.end();
        ++it)
    {
        string sConfigFile = *it;

        ifstream isConfig(sConfigFile.c_str());

        if(isConfig.is_open())
        {
            string sEngineRunTimePath = "";

            size_t iFound = sConfigFile.rfind("/");

            if(iFound != string::npos)
            {
                sEngineRunTimePath = sConfigFile.substr(0,iFound+1);
            }

            // Reading in all the standard engine parameters
            string sDelimiter = "";
            isConfig >> sDelimiter;
            TradeEngineBasePtr pNewTradeEngine;
            string sEngineType = "";
            string sEngineSlotName = "";
            string sTradingStartTime;
            string sTradingEndTime;
            bool bEngineConfigValid = true;

            if(sDelimiter.compare("Engine") == 0)
            {
                isConfig >> sEngineType;
                isConfig >> sEngineSlotName;
                isConfig >> sTradingStartTime;
                isConfig >> sTradingEndTime;

                string sHour;
                string sMinute;
                string sSecond;
                string sMilliSecond;

                sHour = sTradingStartTime.substr (0,2);
                sMinute = sTradingStartTime.substr (2,2);
                sSecond = sTradingStartTime.substr (4,2);
                KOEpochTime cTradingStartTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(_cSchedulerCfg.sDate, sHour, sMinute, sSecond);
                cTradingStartTime = cTradingStartTime;

                sHour = sTradingEndTime.substr (0,2);
                sMinute = sTradingEndTime.substr (2,2);
                sSecond = sTradingEndTime.substr (4,2);
                KOEpochTime cTradingEndTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(_cSchedulerCfg.sDate, sHour.c_str(), sMinute.c_str(), sSecond.c_str());
                cTradingEndTime = cTradingEndTime;

                if(sEngineType.compare("GridDataGenerator") == 0)
                {
                    pNewTradeEngine.reset(new GridDataGenerator(sEngineRunTimePath, sEngineSlotName, cTradingStartTime, cTradingEndTime, this, _cSchedulerCfg.sDate, &_cPositionServerConnection));
                }
                else if(sEngineType.compare("SLSL") == 0)
                {
                    pNewTradeEngine.reset(new SLSL(sEngineRunTimePath, sEngineSlotName, cTradingStartTime, cTradingEndTime, this, _cSchedulerCfg.sDate, &_cPositionServerConnection));
                }
                else if(sEngineType.compare("DataPrinter") == 0)
                {
                    pNewTradeEngine.reset(new DataPrinter(sEngineRunTimePath, sEngineSlotName, cTradingStartTime, cTradingEndTime, this, _cSchedulerCfg.sDate, &_cPositionServerConnection));
                }
                else if(sEngineType.compare("SL3LEqualWeight") == 0)
                {
                    pNewTradeEngine.reset(new SL3LEqualWeight(sEngineRunTimePath, sEngineSlotName, cTradingStartTime, cTradingEndTime, this, _cSchedulerCfg.sDate, &_cPositionServerConnection));
                }
                else if(sEngineType.compare("Liquidator") == 0)
                {
                    pNewTradeEngine.reset(new Liquidator(sEngineRunTimePath, sEngineSlotName, cTradingStartTime, cTradingEndTime, this, _cSchedulerCfg.sDate, &_cPositionServerConnection));
                }
                else if(sEngineType.compare("OrderTest") == 0)
                {
                    pNewTradeEngine.reset(new OrderTest(sEngineRunTimePath, sEngineSlotName, cTradingStartTime, cTradingEndTime, this, _cSchedulerCfg.sDate, &_cPositionServerConnection));
                }
                else
                {
                    bEngineConfigValid = false;
                    stringstream cStringStream;
                    cStringStream << "Engine type " << sEngineType << " not registered \n";
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                }

                if(it == _cSchedulerCfg.vTraderConfigs.begin())
                {
                    _cTimerStart = cTradingStartTime;
                    _cTimerEnd = cTradingEndTime;
                }
                else
                {
                    if(cTradingStartTime < _cTimerStart)
                    {
                        _cTimerStart = cTradingStartTime;
                    }

                    if(cTradingEndTime > _cTimerEnd)
                    {
                        _cTimerEnd = cTradingEndTime;
                    }
                }

                addNewEngineCall(pNewTradeEngine.get(), EngineEvent::RUN, cTradingStartTime);
                addNewEngineCall(pNewTradeEngine.get(), EngineEvent::STOP, cTradingEndTime);
            }
            else
            {
                bEngineConfigValid = false;
                stringstream cStringStream;
                cStringStream << "Config file " << sConfigFile << " format incorrect. Exepect key word \"Engine\". Received " << sDelimiter << ".";
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
            }

            isConfig >> sDelimiter;

            if(sDelimiter.compare("Products") == 0)
            {
                int iNumProds = 0;
                isConfig >> iNumProds;

                for(int i = 0; i < iNumProds; i++)
                {
                    string sProduct = "";
                    string sAccount = "";
                    int    iLimit = 0;

                    isConfig >> sProduct;
                    isConfig >> sAccount;
                    isConfig >> iLimit;

                    if(!registerPriceForEngine(sProduct, sAccount, iLimit, pNewTradeEngine))
                    {
                        bEngineConfigValid = false;
                        stringstream cStringStream;
                        cStringStream << "Cannot register product " << sProduct << " account " << sAccount << " for trade engine " << sEngineSlotName << ".";
                        ErrorHandler::GetInstance()->newErrorMsg("0", sAccount, sProduct, cStringStream.str());
                    }
                }
            }
            else
            {
                bEngineConfigValid = false;
                stringstream cStringStream;
                cStringStream << "Config file " << sConfigFile << " format incorrect. Exepect key word \"Products\". Received " << sDelimiter << ".";
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
            }

            isConfig >> sDelimiter;

            if(sDelimiter.compare("EngineParam") == 0)
            {
                pNewTradeEngine->readFromStream(isConfig);
            }
            else
            {
                bEngineConfigValid = false;
                stringstream cStringStream;
                cStringStream << "Config file " << sConfigFile << " format incorrect. Exepect key word \"EngineParam\". Received " << sDelimiter << ".";
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
            }

            if(bEngineConfigValid)
            {
                _vTradeEngines.push_back(pNewTradeEngine);
            }
            else
            {
                stringstream cStringStream;
                cStringStream << "Incorrect format for Config file " << sConfigFile << ". No engine is created";
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
            }
        }
        else
        {
            stringstream cStringStream;
            cStringStream << "Cannot find config file " << sConfigFile << " no engine is created.";
            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
        }
    }
}

bool SchedulerBase::registerPriceForEngine(const string& sFullProductName, const string& sAccount, int iLimit, TradeEngineBasePtr pTargetEngine)
{
    bool bResult = false;

    string sExchange = "";
    string sProduct = "";

    std::size_t iDotPos = sFullProductName.find(".");
    if(iDotPos == std::string::npos)
    {
        sExchange = "";
        sProduct = sFullProductName;
    }
    else
    {
        sExchange = sFullProductName.substr(0, iDotPos);
        sProduct = sFullProductName.substr(iDotPos+1);
    }

    int iTargetIndex = -1;
    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
    {
        if(_vContractQuoteDatas[i]->sProduct.compare(sProduct) == 0 &&
           _vContractQuoteDatas[i]->sExchange.compare(sExchange) == 0)
        {
            iTargetIndex = i;
            bResult = true;
            break;
        }
    }

    if(iTargetIndex != -1)
    {
        long iProductMaxOrderSize = 10000;
        long iMaxProductPnLLoss = -1000000;

        _cProductEngineMap[iTargetIndex].push_back(pTargetEngine);
        pTargetEngine->vContractQuoteDatas.push_back(_vContractQuoteDatas[iTargetIndex]);

        boost::shared_ptr<ContractAccount> pNewContractAccount;

        pNewContractAccount.reset(new ContractAccount(this,
                                                      pTargetEngine.get(),
                                                      _vContractQuoteDatas[iTargetIndex],
                                                      pTargetEngine->sgetEngineSlotName(),
                                                      sProduct,
                                                      _vContractQuoteDatas[iTargetIndex]->sExchange,
                                                      _vContractQuoteDatas[iTargetIndex]->dTickSize,
                                                      iProductMaxOrderSize,
                                                      iLimit,
                                                      iMaxProductPnLLoss,
                                                      sAccount,
                                                      bisLiveTrading()));
        pTargetEngine->vContractAccount.push_back(pNewContractAccount);
        _vContractAccount.push_back(pNewContractAccount);
    }

    return bResult;
}

void SchedulerBase::addNewFigureCall(TradeEngineBasePtr pTradeEngine, string sFigureName, KOEpochTime cCallTime, KOEpochTime cFigureTime, FigureAction::options eFigureAction)
{
    FigureCallPtr pNewFigureCall (new FigureCall);
    pNewFigureCall->_sFigureName = sFigureName;
    pNewFigureCall->_cCallTime = cCallTime;
    pNewFigureCall->_cFigureTime = cFigureTime;
    pNewFigureCall->_eFigureAction = eFigureAction;

    addNewFigureCall(pTradeEngine.get(), pNewFigureCall);
}

bool SchedulerBase::bloadRiskFile()
{
    bool bResult = false;

    ifstream ifsRiskFile(_cSchedulerCfg.sRiskFile.c_str());

    if(ifsRiskFile.is_open())
    {
        bResult = true;

        while(!ifsRiskFile.eof())
        {
            char sNewLine[256];
            ifsRiskFile.getline(sNewLine, sizeof(sNewLine));

            if(sNewLine[0] != '#' && strlen(sNewLine) != 0)
            {
                ProductRiskPtr pNewRiskPostion (new ProductRisk);
                pNewRiskPostion->iPos = 0;

                string sProductName = "";

                std::istringstream cRiskLineStream(sNewLine);
                string sElement;

                std::getline(cRiskLineStream, sElement, ' ');
                sProductName = sElement;

                std::getline(cRiskLineStream, sElement, ' ');
                pNewRiskPostion->iGlobalLimit = atoi(sElement.c_str());

                std::getline(cRiskLineStream, sElement, ' ');
                pNewRiskPostion->iMaxOrderSize = atoi(sElement.c_str());

                std::getline(cRiskLineStream, sElement, ' '); 
                pNewRiskPostion->iNumOrderPerSecondPerModel = atoi(sElement.c_str());

                std::getline(cRiskLineStream, sElement, ' ');
                pNewRiskPostion->iOrderPriceDeviationInTicks = atoi(sElement.c_str());
                
                _mRiskSetting.insert(std::pair<string, ProductRiskPtr>(sProductName, pNewRiskPostion));
            }
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Cannot load risk file " << _cSchedulerCfg.sRiskFile << ".";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    return bResult;
}

void SchedulerBase::loadFigureFile()
{
    ifstream ifsFigureFile (_cSchedulerCfg.sFigureFile.c_str());

    if(ifsFigureFile.is_open())
    {
        while(!ifsFigureFile.eof())
        {
            char sNewLine[256];
            ifsFigureFile.getline(sNewLine, sizeof(sNewLine));

            if(sNewLine[0] != '#' && strlen(sNewLine) != 0)
            {
                boost::shared_ptr<Figure> pNewFigure (new Figure);

                std::istringstream cFigureLineStream(sNewLine);
                string sElement;

                std::getline(cFigureLineStream, sElement, ';');
                pNewFigure->sFigureName = sElement;

                std::getline(cFigureLineStream, sElement, ';');
                string sFigureDay = sElement.c_str();
                pNewFigure->sFigureDay = sFigureDay;

                std::getline(cFigureLineStream, sElement, ';');

                string sHour = sElement.substr(0,2);
                string sMinute = sElement.substr(3,2);

                pNewFigure->cFigureTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromUTC(sFigureDay, sHour, sMinute, "00");

                _vAllFigures.push_back(pNewFigure);
            }
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Cannot load figure file " << _cSchedulerCfg.sFigureFile << ".";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void SchedulerBase::loadProductFigureActionFile()
{
    ifstream ifsFigureActionFile (_cSchedulerCfg.sFigureActionFile.c_str());

    if(ifsFigureActionFile.is_open())
    {
        while(!ifsFigureActionFile.eof())
        {
            char sNewLine[2048];
            ifsFigureActionFile.getline(sNewLine, sizeof(sNewLine));

            if(sNewLine[0] != '#' && strlen(sNewLine) != 0)
            {
                std::istringstream cFigureActionLineStream(sNewLine);
                string sElement;

                string sProduct;
                string sActionType;
                boost::shared_ptr< vector <string> > vNewProductFigureActions (new vector<string> ());

                int index = 0;
                while (std::getline(cFigureActionLineStream, sElement, ';'))
                {
                    if(index == 0)
                    {
                        sProduct = sElement;
                    }
                    else if(index == 1)
                    {
                        sActionType = sElement;
                    }
                    else
                    {
                        vNewProductFigureActions->push_back(sElement);
                    }

                    index = index + 1;
                }

                if(sActionType.compare("HALT") == 0)
                {
                    _mProductFigureHalt[sProduct] = vNewProductFigureActions;
                }
                else if(sActionType.compare("HITTING") == 0)
                {
                    _mProductFigureHitting[sProduct] = vNewProductFigureActions;
                }
                else if(sActionType.compare("FLAT") == 0)
                {
                    _mProductFigureFlat[sProduct] = vNewProductFigureActions;
                }
                else if(sActionType.compare("PATIENT") == 0)
                {
                    _mProductFigurePatient[sProduct] = vNewProductFigureActions;
                }
                else if(sActionType.compare("FLAT_HITTING") == 0)
                {
                    _mProductFigureFlatHitting[sProduct] = vNewProductFigureActions;
                }
            }
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Cannot load figure action file " << _cSchedulerCfg.sFigureActionFile << ".";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void SchedulerBase::loadTodaysFigure(string sDate)
{
    for(vector< boost::shared_ptr<Figure> >::iterator itr = _vAllFigures.begin(); itr != _vAllFigures.end(); ++itr)
    {
        if((*itr)->sFigureDay.compare(sDate) == 0)
        {
            for(vector<TradeEngineBasePtr>::iterator itrEngine = _vTradeEngines.begin(); itrEngine != _vTradeEngines.end(); itrEngine++)
            {
                vector<string> vRegisteredFigureProducts = (*itrEngine)->vgetRegisterdFigureProducts();
                for(vector<string>::iterator itrFigureProducts = vRegisteredFigureProducts.begin();
                    itrFigureProducts != vRegisteredFigureProducts.end();
                    itrFigureProducts++)
                {
                    map<string, boost::shared_ptr< vector <string> > >::iterator productHittingFiguresItr = _mProductFigureHitting.find(*itrFigureProducts);
                    if(productHittingFiguresItr != _mProductFigureHitting.end())
                    {
                        boost::shared_ptr< vector <string> > pHittingFigures = productHittingFiguresItr->second;

                        for(vector<string>::iterator itrHittingFigures = pHittingFigures->begin();
                            itrHittingFigures != pHittingFigures->end();
                            itrHittingFigures++)
                        {
                            string sFigureName = *itrHittingFigures;

                            if(sFigureName.compare((*itr)->sFigureName) == 0)
                            {
                                long iPostFigureTimeOut = 1;
                                if(sFigureName.compare("EUR_EurECBInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 150;
                                }
                                else if(sFigureName.compare("GBP_BoEInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 50;
                                }
                                else if(sFigureName.compare("GBP_BankofEnglandQuarterlyInflationReport") == 0)
                                {
                                    iPostFigureTimeOut = 140;
                                }
                                else if(sFigureName.compare("USD_FedInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 200;
                                }
                                else if(sFigureName.compare("USD_FOMCMinutes") == 0)
                                {
                                    iPostFigureTimeOut = 200;
                                }
                                else if(sFigureName.compare("EUR_EurCentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("EUR_GerCentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("GBP_CentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("USD_CentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("NZD_RBNZRateStatement") == 0)
                                {
                                    iPostFigureTimeOut = 200;
                                }
                                else if(sFigureName.compare("CHF_SNBInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 1000;
                                }
                                else if(sFigureName.compare("CAD_BoCInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 600;
                                }


                                KOEpochTime cFigureTime = (*itr)->cFigureTime;

                                // pre figure time out is set to 5 mins
                                KOEpochTime cPreFigureCallTime = (*itr)->cFigureTime - KOEpochTime(300,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPreFigureCallTime, cFigureTime, FigureAction::HITTING);
                                stringstream cStringStream;
                                cStringStream << "Added " << *itrFigureProducts << " Pre Hitting Figure call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPreFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                cStringStream.str(std::string());
                                KOEpochTime cPostFigureCallTime = (*itr)->cFigureTime + KOEpochTime(iPostFigureTimeOut * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPostFigureCallTime, cFigureTime, FigureAction::HITTING);
                                cStringStream << "Added " << *itrFigureProducts << " Post Hitting Figure call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPostFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                            }
                        }
                    }
                    else
                    {
                        stringstream cStringStream;
                        cStringStream << "No hitting figures defined for product " << *itrFigureProducts;
                        ErrorHandler::GetInstance()->newInfoMsg("0", (*itrEngine)->sgetEngineSlotName(), *itrFigureProducts, cStringStream.str());
                    }

                    map<string, boost::shared_ptr< vector <string> > >::iterator productPatientFiguresItr = _mProductFigurePatient.find(*itrFigureProducts);
                    if(productPatientFiguresItr != _mProductFigurePatient.end())
                    {
                        boost::shared_ptr< vector <string> > pPatientFigures = productPatientFiguresItr->second;

                        for(vector<string>::iterator itrPatientFigures = pPatientFigures->begin();
                            itrPatientFigures != pPatientFigures->end();
                            itrPatientFigures++)
                        {
                            string sFigureName = *itrPatientFigures;

                            if(sFigureName.compare((*itr)->sFigureName) == 0)
                            {
                                long iPostFigureTimeOut = 1;
                                if(sFigureName.compare("EUR_EurECBInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 150;
                                }
                                else if(sFigureName.compare("GBP_BoEInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 50;
                                }
                                else if(sFigureName.compare("GBP_BankofEnglandQuarterlyInflationReport") == 0)
                                {
                                    iPostFigureTimeOut = 140;
                                }
                                else if(sFigureName.compare("USD_FedInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 200;
                                }
                                else if(sFigureName.compare("USD_FOMCMinutes") == 0)
                                {
                                    iPostFigureTimeOut = 200;
                                }
                                else if(sFigureName.compare("EUR_EurCentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("EUR_GerCentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("GBP_CentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("USD_CentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("NZD_RBNZRateStatement") == 0)
                                {
                                    iPostFigureTimeOut = 200;
                                }
                                else if(sFigureName.compare("CHF_SNBInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 1000;
                                }
                                else if(sFigureName.compare("CAD_BoCInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 600;
                                }


                                KOEpochTime cFigureTime = (*itr)->cFigureTime;

                                // pre figure time out is set to 5 mins
                                KOEpochTime cPreFigureCallTime = (*itr)->cFigureTime - KOEpochTime(300,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPreFigureCallTime, cFigureTime, FigureAction::PATIENT);
                                stringstream cStringStream;
                                cStringStream << "Added " << *itrFigureProducts << " Pre Patient Figure call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPreFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                cStringStream.str(std::string());
                                KOEpochTime cPostFigureCallTime = (*itr)->cFigureTime + KOEpochTime(iPostFigureTimeOut * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPostFigureCallTime, cFigureTime, FigureAction::PATIENT);
                                cStringStream << "Added " << *itrFigureProducts << " Post Patient Figure call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPostFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                            }
                        }
                    }
                    else
                    {
                        stringstream cStringStream;
                        cStringStream << "No patient figures defined for product " << *itrFigureProducts;
                        ErrorHandler::GetInstance()->newInfoMsg("0", (*itrEngine)->sgetEngineSlotName(), *itrFigureProducts, cStringStream.str());
                    }

                    map<string, boost::shared_ptr< vector <string> > >::iterator productFlatFiguresItr = _mProductFigureFlat.find(*itrFigureProducts);
                    if(productFlatFiguresItr != _mProductFigureFlat.end())
                    {
                        boost::shared_ptr< vector <string> > pFlatFigures = productFlatFiguresItr->second;

                        for(vector<string>::iterator itrFlatFigures = pFlatFigures->begin();
                            itrFlatFigures != pFlatFigures->end();
                            itrFlatFigures++)
                        {
                            string sFigureName = *itrFlatFigures;

                            if(sFigureName.compare((*itr)->sFigureName) == 0)
                            {
                                long iPostFigureTimeOut = 20;
                                if(sFigureName.compare("EUR_EurECBInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 150;
                                }
                                else if(sFigureName.compare("GBP_BoEInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 50;
                                }
                                else if(sFigureName.compare("GBP_BankofEnglandQuarterlyInflationReport") == 0)
                                {
                                    iPostFigureTimeOut = 140;
                                }
                                else if(sFigureName.compare("USD_FedInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 200;
                                }
                                else if(sFigureName.compare("USD_FOMCMinutes") == 0)
                                {
                                    iPostFigureTimeOut = 200;
                                }
                                else if(sFigureName.compare("EUR_EurCentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("EUR_GerCentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("GBP_CentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("USD_CentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("NZD_RBNZRateStatement") == 0)
                                {
                                    iPostFigureTimeOut = 200;
                                }
                                else if(sFigureName.compare("CHF_SNBInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 1000;
                                }
                                else if(sFigureName.compare("CAD_BoCInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 600;
                                }


                                KOEpochTime cFigureTime = (*itr)->cFigureTime;

                                // pre figure time out is set to 20 mins when liquidating for figure
                                KOEpochTime cLimLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(25 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cLimLiqFigureCallTime, cFigureTime, FigureAction::LIM_LIQ);
                                stringstream cStringStream;
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat Figure lim liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cLimLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                cStringStream.str(std::string());
                                KOEpochTime cFastLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(5 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cFastLiqFigureCallTime, cFigureTime, FigureAction::FAST_LIQ);
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat Figure fast liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cFastLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                cStringStream.str(std::string());
                                KOEpochTime cPostFigureCallTime = (*itr)->cFigureTime + KOEpochTime(iPostFigureTimeOut * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPostFigureCallTime, cFigureTime, FigureAction::FAST_LIQ);
                                cStringStream << "Added " << *itrFigureProducts << " Post Flat Figure call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPostFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                            }
                        }
                    }
                    else
                    {
                        stringstream cStringStream;
                        cStringStream << "No flat figures defined for product " << *itrFigureProducts;
                        ErrorHandler::GetInstance()->newInfoMsg("0", (*itrEngine)->sgetEngineSlotName(), *itrFigureProducts, cStringStream.str());
                    }

                    map<string, boost::shared_ptr< vector <string> > >::iterator productHaltFiguresItr = _mProductFigureHalt.find(*itrFigureProducts);
                    if(productHaltFiguresItr != _mProductFigureHalt.end())
                    {
                        boost::shared_ptr< vector <string> > pHaltFigures = productHaltFiguresItr->second;

                        for(vector<string>::iterator itrHaltFigures = pHaltFigures->begin();
                            itrHaltFigures != pHaltFigures->end();
                            itrHaltFigures++)
                        {
                            string sFigureName = *itrHaltFigures;

                            if(sFigureName.compare((*itr)->sFigureName) == 0)
                            {
                                long iPostFigureTimeOut = 5;
                                if(sFigureName.compare("EUR_EurECBInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 150;
                                }
                                else if(sFigureName.compare("GBP_BoEInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 50;
                                }
                                else if(sFigureName.compare("GBP_BankofEnglandQuarterlyInflationReport") == 0)
                                {
                                    iPostFigureTimeOut = 140;
                                }
                                else if(sFigureName.compare("USD_FedInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 200;
                                }
                                else if(sFigureName.compare("USD_FOMCMinutes") == 0)
                                {
                                    iPostFigureTimeOut = 200;
                                }
                                else if(sFigureName.compare("EUR_EurCentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("EUR_GerCentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("GBP_CentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("USD_CentralBankSpeech") == 0)
                                {
                                    iPostFigureTimeOut = 20;
                                }
                                else if(sFigureName.compare("NZD_RBNZRateStatement") == 0)
                                {
                                    iPostFigureTimeOut = 200;
                                }
                                else if(sFigureName.compare("CHF_SNBInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 1000;
                                }
                                else if(sFigureName.compare("CAD_BoCInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 600;
                                }

                                KOEpochTime cFigureTime = (*itr)->cFigureTime;

                                KOEpochTime cPreFigureCallTime = (*itr)->cFigureTime - KOEpochTime(5 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPreFigureCallTime, cFigureTime, FigureAction::HALT_TRADING);
                                stringstream cStringStream;
                                cStringStream << "Added " << *itrFigureProducts << " Pre Halt Figure call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPreFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                cStringStream.str(std::string());
                                KOEpochTime cPostFigureCallTime = (*itr)->cFigureTime + KOEpochTime(iPostFigureTimeOut * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPostFigureCallTime, cFigureTime, FigureAction::HALT_TRADING);
                                cStringStream << "Added " << *itrFigureProducts << " Post Halt Figure call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPostFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                            }
                        }
                    }
                    else
                    {
                        stringstream cStringStream;
                        cStringStream << "No halt figures defined for product " << *itrFigureProducts;
                        ErrorHandler::GetInstance()->newInfoMsg("0", (*itrEngine)->sgetEngineSlotName(), *itrFigureProducts, cStringStream.str());
                    }

                    map<string, boost::shared_ptr< vector <string> > >::iterator productFlatHittingFiguresItr = _mProductFigureFlatHitting.find(*itrFigureProducts);
                    if(productFlatHittingFiguresItr != _mProductFigureFlatHitting.end())
                    {
                        boost::shared_ptr< vector <string> > pFlatHittingFigures = productFlatHittingFiguresItr->second;

                        for(vector<string>::iterator itrFlatHittingFigures = pFlatHittingFigures->begin();
                            itrFlatHittingFigures != pFlatHittingFigures->end();
                            itrFlatHittingFigures++)
                        {
                            string sFigureName = *itrFlatHittingFigures;

                            if(sFigureName.compare((*itr)->sFigureName) == 0)
                            {
                                KOEpochTime cFigureTime = (*itr)->cFigureTime;

                                // pre figure time out is set to 20 mins when liquidating for figure
                                KOEpochTime cPatientFigureCallTime = (*itr)->cFigureTime - KOEpochTime(15 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPatientFigureCallTime, cFigureTime, FigureAction::PATIENT);
                                stringstream cStringStream;
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat Hitting Figure patient call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPatientFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                                
                                KOEpochTime cLimLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(10 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cLimLiqFigureCallTime, cFigureTime, FigureAction::LIM_LIQ);
                                cStringStream.str(std::string());
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat Hitting Figure lim liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cLimLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                KOEpochTime cFastLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(5 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cFastLiqFigureCallTime, cFigureTime, FigureAction::FAST_LIQ);
                                cStringStream.str(std::string());
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat Hitting Figure fast liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cFastLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                KOEpochTime cPostFigureCallTime = (*itr)->cFigureTime + KOEpochTime(4,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPostFigureCallTime, cFigureTime, FigureAction::NO_FIGURE);
                                cStringStream.str(std::string());
                                cStringStream << "Added " << *itrFigureProducts << " Post Flat Hitting Figure call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPostFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                            }
                        }
                    }
                    else
                    {
                        stringstream cStringStream;
                        cStringStream << "No flat hitting figures defined for product " << *itrFigureProducts;
                        ErrorHandler::GetInstance()->newInfoMsg("0", (*itrEngine)->sgetEngineSlotName(), *itrFigureProducts, cStringStream.str());
                    }


                }
            }
        }
    }
}

void SchedulerBase::limLiqAllTraders(int aSignal)
{
    if(aSignal > 0 || (aSignal == 0 && _pInstance->bgetEngineManualHalted() == false))
    {
        stringstream cStringStream;
        cStringStream << "Received signal to set engine to limit liquidation state.";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

        for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
        {
            if((*itr)->_sEngineType != "Liquidator")
            {
                (*itr)->manualLimitLiquidation();
            }
            else
            {
                Liquidator* pLiquidator = dynamic_cast<Liquidator*>((*itr).get());
                pLiquidator->onOffSignal();
                (*itr)->manualHaltTrading();
            }
        }
    }

    if(aSignal > 0)
    {
        _pInstance->setEngineManualHalted(true);
    }
}

void SchedulerBase::resumeAllTraders(int aSignal)
{
    if(aSignal > 0 || (aSignal == 0 && _pInstance->bgetEngineManualHalted() == false))
    {
        stringstream cStringStream;
        cStringStream << "Received signal to set engine to resume state.";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

        for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
        {
            if((*itr)->_sEngineType != "Liquidator")
            {
                (*itr)->manualResumeTrading();
            }
            else
            {
                Liquidator* pLiquidator = dynamic_cast<Liquidator*>((*itr).get());
                pLiquidator->onOffSignal();
                (*itr)->manualHaltTrading();
            }
        }
    }

    if(aSignal > 0)
    {
        _pInstance->setEngineManualHalted(false);
    }
}

void SchedulerBase::patientLiqAllTraders(int aSignal)
{
    if(aSignal > 0 || (aSignal == 0 && _pInstance->bgetEngineManualHalted() == false))
    {
        stringstream cStringStream;
        cStringStream << "Received signal to set engine to patient liquidation state.";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

        for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
        {
            if((*itr)->_sEngineType != "Liquidator")
            {
                (*itr)->manualPatientLiquidation();
            }
            else
            {
                Liquidator* pLiquidator = dynamic_cast<Liquidator*>((*itr).get());
                pLiquidator->onOffSignal();
                (*itr)->manualHaltTrading();
            }
        }
    }
}

void SchedulerBase::fastLiqAllTraders(int aSignal)
{
    if(aSignal > 0 || (aSignal == 0 && _pInstance->bgetEngineManualHalted() == false))
    {
        stringstream cStringStream;
        cStringStream << "Received signal to set engine to fast liquidation state.";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

        for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
        {
            if((*itr)->_sEngineType != "Liquidator")
            {
                (*itr)->manualFastLiquidation();
            }
            else
            {
                Liquidator* pLiquidator = dynamic_cast<Liquidator*>((*itr).get());
                pLiquidator->onOffSignal();
                (*itr)->manualHaltTrading();
            }
        }
    }

    if(aSignal > 0)
    {
        _pInstance->setEngineManualHalted(true);
    }
}

void SchedulerBase::haltAllTraders(int aSignal)
{
    if(aSignal > 0 || (aSignal == 0 && _pInstance->bgetEngineManualHalted() == false))
    {
        stringstream cStringStream;
        cStringStream << "Received signal to set engine to halt state.";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

        for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
        {
            (*itr)->manualHaltTrading();
        }
    }

    if(aSignal > 0)
    {
        _pInstance->setEngineManualHalted(true);
    }
}

void SchedulerBase::signalCommand(int aSignal)
{
    stringstream cStringStream;
    cStringStream << "Received signal for signal command.";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
    {
        (*itr)->externalSignalHandler();
    }
}

void SchedulerBase::limitLiqSlotsLiquidator(int aSignal)
{
    if(aSignal > 0 || (aSignal == 0 && _pInstance->bgetEngineManualHalted() == false))
    {
        stringstream cStringStream;
        cStringStream << "Received signal for limit liq slots liquidator.";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

        for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
        {
            if((*itr)->_sEngineType == "Liquidator")
            {
                (*itr)->manualResumeTrading();
                Liquidator* pLiquidator = dynamic_cast<Liquidator*>((*itr).get());
                pLiquidator->onLimitLiqSlotsSignal();
            }
        }
    }

    if(aSignal > 0)
    {
        _pInstance->setEngineManualHalted(true);
    }
}

void SchedulerBase::fastLiqSlotsLiquidator(int aSignal)
{
    if(aSignal > 0 || (aSignal == 0 && _pInstance->bgetEngineManualHalted() == false))
    {
        stringstream cStringStream;
        cStringStream << "Received signal for fast liq slots liquidator.";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

        for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
        {
            if((*itr)->_sEngineType == "Liquidator")
            {
                (*itr)->manualResumeTrading();
                Liquidator* pLiquidator = dynamic_cast<Liquidator*>((*itr).get());
                pLiquidator->onFastLiqSlotsSignal();
            }
        }
    }

    if(aSignal > 0)
    {
        _pInstance->setEngineManualHalted(true);
    }
}

void SchedulerBase::limitLiqAllLiquidator(int aSignal)
{
    if(aSignal > 0 || (aSignal == 0 && _pInstance->bgetEngineManualHalted() == false))
    {
        stringstream cStringStream;
        cStringStream << "Received signal for limit liq all liquidator.";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

        for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
        {
            if((*itr)->_sEngineType == "Liquidator")
            {
                (*itr)->manualResumeTrading();
                Liquidator* pLiquidator = dynamic_cast<Liquidator*>((*itr).get());
                pLiquidator->onLimitLiqAllSignal();
            }
            else
            {
                (*itr)->manualHaltTrading();
            }
        }
    }

    if(aSignal > 0)
    {
        _pInstance->setEngineManualHalted(true);
    }
}

void SchedulerBase::fastLiqAllLiquidator(int aSignal)
{
    if(aSignal > 0 || (aSignal == 0 && _pInstance->bgetEngineManualHalted() == false))
    {
        stringstream cStringStream;
        cStringStream << "Received signal for fast liq all liquidator.";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

        for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
        {
            if((*itr)->_sEngineType == "Liquidator")
            {
                (*itr)->manualResumeTrading();
                Liquidator* pLiquidator = dynamic_cast<Liquidator*>((*itr).get());
                pLiquidator->onFastLiqAllSignal();
            }
            else
            {
                (*itr)->manualHaltTrading();
            }
        }
    }

    if(aSignal > 0)
    {
        _pInstance->setEngineManualHalted(true);
    }
}

void SchedulerBase::offLiquidator(int aSignal)
{
    stringstream cStringStream;
    cStringStream << "Received signal for off liquidator.";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
    {
        if((*itr)->_sEngineType == "Liquidator")
        {
            Liquidator* pLiquidator = dynamic_cast<Liquidator*>((*itr).get());
            pLiquidator->onOffSignal();
            (*itr)->manualHaltTrading();
        }
    }
}

void SchedulerBase::limitLiquidateProduct(int aSignal)
{
    stringstream cStringStream;
    cStringStream << "Received signal for limit liq product.";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    _pInstance->liquidatProduct(true);
}

void SchedulerBase::fastLiquidateProduct(int aSignal)
{
    stringstream cStringStream;
    cStringStream << "Received signal for fast liq product.";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    _pInstance->liquidatProduct(false);
}

void SchedulerBase::liquidatProduct(bool bIsLimit)
{
    ifstream ifsLiquidationCommandFile (_cSchedulerCfg.sLiquidationCommandFile);

    if(ifsLiquidationCommandFile.is_open())
    {
        while(!ifsLiquidationCommandFile.eof())
        {
            char sNewLine[256];
            ifsLiquidationCommandFile.getline(sNewLine, sizeof(sNewLine));

            string sProductToBeLiquidated = sNewLine;

             for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
            {
                if((*itr)->_sEngineType != "Liquidator")
                {
                    if(sProductToBeLiquidated == (*itr)->vContractAccount[0]->sgetRootSymbol())
                    {
                        if(bIsLimit)
                        {
                            (*itr)->manualLimitLiquidation();                    
                        }
                        else
                        {
                            (*itr)->manualFastLiquidation();                    
                        }
                    }
                }
                else
                {
                    Liquidator* pLiquidator = dynamic_cast<Liquidator*>((*itr).get());
                    pLiquidator->onOffSignal();
                    (*itr)->manualHaltTrading();
                }
            }
        }
    }
}

void SchedulerBase::registerSignalHandlers()
{
    struct sigaction maxSigAction;

    sigemptyset(&maxSigAction.sa_mask);
    maxSigAction.sa_flags = 0;

    maxSigAction.sa_handler = SchedulerBase::limLiqAllTraders;
    if (-1 == sigaction(SIGRTMAX, &maxSigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for lim liq signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max1SigAction;

    sigemptyset(&max1SigAction.sa_mask);
    max1SigAction.sa_flags = 0;

    max1SigAction.sa_handler = SchedulerBase::resumeAllTraders;
    if (-1 == sigaction(SIGRTMAX-1, &max1SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for resume signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max2SigAction;

    sigemptyset(&max2SigAction.sa_mask);
    max2SigAction.sa_flags = 0;

    max2SigAction.sa_handler = SchedulerBase::patientLiqAllTraders;
    if (-1 == sigaction(SIGRTMAX-2, &max2SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for patient liq signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max3SigAction;

    sigemptyset(&max3SigAction.sa_mask);
    max3SigAction.sa_flags = 0;

    max3SigAction.sa_handler = SchedulerBase::fastLiqAllTraders;
    if (-1 == sigaction(SIGRTMAX-3, &max3SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for fast liq signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max4SigAction;

    sigemptyset(&max4SigAction.sa_mask);
    max4SigAction.sa_flags = 0;

    max4SigAction.sa_handler = SchedulerBase::haltAllTraders;
    if (-1 == sigaction(SIGRTMAX-4, &max4SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for halt all signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max5SigAction;

    sigemptyset(&max5SigAction.sa_mask);
    max5SigAction.sa_flags = 0;

    max5SigAction.sa_handler = SchedulerBase::signalCommand;
    if (-1 == sigaction(SIGRTMAX-5, &max5SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for signal command handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max7SigAction;

    sigemptyset(&max7SigAction.sa_mask);
    max7SigAction.sa_flags = 0;

    max7SigAction.sa_handler = SchedulerBase::limitLiqAllLiquidator;
    if (-1 == sigaction(SIGRTMAX-7, &max7SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for limit liq all liquidator signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max8SigAction;

    sigemptyset(&max8SigAction.sa_mask);
    max8SigAction.sa_flags = 0;

    max8SigAction.sa_handler = SchedulerBase::fastLiqAllLiquidator;
    if (-1 == sigaction(SIGRTMAX-8, &max8SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for fast liq liquidator signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max9SigAction;

    sigemptyset(&max9SigAction.sa_mask);
    max9SigAction.sa_flags = 0;

    max9SigAction.sa_handler = SchedulerBase::offLiquidator;
    if (-1 == sigaction(SIGRTMAX-9, &max9SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for off liquidator signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max10SigAction;

    sigemptyset(&max10SigAction.sa_mask);
    max10SigAction.sa_flags = 0;

    max10SigAction.sa_handler = SchedulerBase::limitLiqSlotsLiquidator;
    if (-1 == sigaction(SIGRTMAX-10, &max10SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for limit liq slots liquidator signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max11SigAction;

    sigemptyset(&max11SigAction.sa_mask);
    max11SigAction.sa_flags = 0;

    max11SigAction.sa_handler = SchedulerBase::fastLiqSlotsLiquidator;
    if (-1 == sigaction(SIGRTMAX-11, &max11SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for fast liq slots liquidator signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max12SigAction;

    sigemptyset(&max12SigAction.sa_mask);
    max12SigAction.sa_flags = 0;

    max12SigAction.sa_handler = SchedulerBase::limitLiquidateProduct;
    if (-1 == sigaction(SIGRTMAX-12, &max12SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for limit liquidate product signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max13SigAction;

    sigemptyset(&max13SigAction.sa_mask);
    max13SigAction.sa_flags = 0;

    max13SigAction.sa_handler = SchedulerBase::fastLiquidateProduct;
    if (-1 == sigaction(SIGRTMAX-13, &max13SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for fast liquidate product signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void SchedulerBase::checkOrderStatus(boost::shared_ptr<KO::KOOrder> pOrder)
{
    int iProductIndex = -1;

    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
    {
        if(_vContractQuoteDatas[i]->iCID == pOrder->igetProductCID())
        {
            iProductIndex = i;
            break;
        }
    }

    if(pOrder->_eOrderState == KOOrder::PENDINGCREATION || pOrder->_eOrderState == KOOrder::PENDINGCHANGE || pOrder->_eOrderState == KOOrder::PENDINGDELETE)
    {
        long iNumSecondsWithoutReply = (SystemClock::GetInstance()->cgetCurrentKOEpochTime() - pOrder->_cPendingRequestTime).sec();

        if(iNumSecondsWithoutReply > 10 && pOrder->_bOrderNoReplyTriggered == false)
        {
            pOrder->_bOrderNoReplyTriggered = true;
            stringstream cStringStream;
            cStringStream << "Config " << pOrder->_sSlot << " order " << pOrder->_iOrderID << " no reply received for pending request for " << iNumSecondsWithoutReply << " seconds.";
            ErrorHandler::GetInstance()->newErrorMsg("3.1", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
        }
    }


    if(pOrder->_eOrderState == KOOrder::ACTIVE)
    {
        if(pOrder->igetOrderRemainQty() > 0)
        {
            if(_vContractQuoteDatas[iProductIndex]->iBestAskInTicks < pOrder->igetOrderPriceInTicks() &&
               _vContractQuoteDatas[iProductIndex]->bPriceValid == true)
            {
                if(pOrder->_bOrderNoFill == false)
                {
                    pOrder->_bOrderNoFill = true;
                    pOrder->_cOrderNoFillTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
                }
                else
                {
                    if((SystemClock::GetInstance()->cgetCurrentKOEpochTime() - pOrder->_cOrderNoFillTime).sec() > 2)
                    {
                        if(pOrder->_bOrderNoFillTriggered == false)
                        {
                            pOrder->_bOrderNoFillTriggered = true;
                            stringstream cStringStream;
                            cStringStream << "Config " << pOrder->_sSlot << " order " << pOrder->_iOrderID << " market price " << _vContractQuoteDatas[iProductIndex]->dBestAsk << " smaller than buy order price " << pOrder->dgetOrderPrice() << " for more than 5 seconds.";
                            ErrorHandler::GetInstance()->newErrorMsg("3.1", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
                        }
                    }
                }
            }
            else
            {
                pOrder->_bOrderNoFill = false;
                pOrder->_cOrderNoFillTime = KOEpochTime();
                pOrder->_bOrderNoFillTriggered = false;
            }
        }
        else if(pOrder->igetOrderRemainQty() < 0)
        {
            if(_vContractQuoteDatas[iProductIndex]->iBestBidInTicks > pOrder->igetOrderPriceInTicks() &&
               _vContractQuoteDatas[iProductIndex]->bPriceValid == true)
            {
                 if(pOrder->_bOrderNoFill == false)
                {
                    pOrder->_bOrderNoFill = true;
                    pOrder->_cOrderNoFillTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
                }
                else
                {
                    if((SystemClock::GetInstance()->cgetCurrentKOEpochTime() - pOrder->_cOrderNoFillTime).sec() > 2)
                    {
                        if(pOrder->_bOrderNoFillTriggered == false)
                        {
                            pOrder->_bOrderNoFillTriggered = true;

                            stringstream cStringStream;
                            cStringStream << "Config " << pOrder->_sSlot << " order " << pOrder->_iOrderID << " market price " << _vContractQuoteDatas[iProductIndex]->dBestBid << " bigger than sell order price " << pOrder->dgetOrderPrice() << " for more than 5 seconds.";
                            ErrorHandler::GetInstance()->newErrorMsg("3.1", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
                        }
                    }
                }
            }
            else
            {
                pOrder->_bOrderNoFill = false;
                pOrder->_cOrderNoFillTime = KOEpochTime();
                pOrder->_bOrderNoFillTriggered = false;
            }
        }
    }
}

void SchedulerBase::loadScheduledManualActionsFile()
{
    ifstream ifsScheduledManualActionsFile (_cSchedulerCfg.sScheduledManualActionsFile.c_str());

    if(ifsScheduledManualActionsFile.is_open())
    {
        while(!ifsScheduledManualActionsFile.eof())
        {
            char sNewLine[2048];
            ifsScheduledManualActionsFile.getline(sNewLine, sizeof(sNewLine));

            if(sNewLine[0] != '#' && strlen(sNewLine) != 0)
            {
                std::istringstream cScheduledManualActionLineStream(sNewLine);
                string sElement;

                KOEpochTime cActionTime;
                ManualCommandAction eAction;

                string sActionTime;
                string sAction;

                int index = 0;
                while (std::getline(cScheduledManualActionLineStream, sElement, ';'))
                {
                    if(index == 0)
                    {
                        sActionTime = sElement;

                        string sHour = sElement.substr(0,2);
                        string sMinute = sElement.substr(2,2);
                        string sSecond = sElement.substr(4,2);

                        cActionTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(_cSchedulerCfg.sDate, sHour, sMinute, sSecond);
                    }
                    else if(index == 1)
                    {
                        sAction = sElement;

                        if(sElement.compare("RESUME_ALL") == 0)
                        {
                            eAction = RESUME_ALL;
                        }
                        else if(sElement.compare("PATIENT_ALL") == 0)
                        {
                            eAction = PATIENT_ALL;
                        }
                        else if(sElement.compare("LIMIT_ALL") == 0)
                        {
                            eAction = LIMIT_ALL;
                        }
                        else if(sElement.compare("FAST_ALL") == 0)
                        {
                            eAction = FAST_ALL;
                        }
                        else if(sElement.compare("HALT_ALL") == 0)
                        {
                            eAction = HALT_ALL;
                        }
                        else if(sElement.compare("LIMIT_ALL_LIQUIDATOR") == 0)
                        {
                            eAction = LIMIT_ALL_LIQUIDATOR;
                        }
                        else if(sElement.compare("FAST_ALL_LIQUIDATOR") == 0)
                        {
                            eAction = FAST_ALL_LIQUIDATOR;
                        }
                        else if(sElement.compare("LIMIT_SLOTS_LIQUIDATOR") == 0)
                        {
                            eAction = LIMIT_SLOTS_LIQUIDATOR;
                        }
                        else if(sElement.compare("FAST_SLOTS_LIQUIDATOR") == 0)
                        {
                            eAction = FAST_SLOTS_LIQUIDATOR;
                        }
                        else if(sElement.compare("OFF_LIQUIDATOR") == 0)
                        {
                            eAction = OFF_LIQUIDATOR;
                        }
                        else
                        {
                            eAction = UNKNOWN_MANUAL_COMMAND;
                        }
                    }

                    index = index + 1;
                }

                if(eAction != UNKNOWN_MANUAL_COMMAND)
                {
                    stringstream cStringStream;
                    cStringStream << "Adding new scheduled manual action " << sActionTime << " " << sAction << ".";
                    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                    _vScheduledManualActions.push_back(std::make_pair(cActionTime, eAction));
                }
                else
                {
                    stringstream cStringStream;
                    cStringStream << "Ignore unknown scheduled manual action command! " << sNewLine;
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str()); 
                }
            }
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Cannot load scheduled manual action file " << _cSchedulerCfg.sScheduledManualActionsFile << ".";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }
}

void SchedulerBase::loadScheduledSlotLiquidationFile()
{
    ifstream ifsScheduledSlotLiquidationFile (_cSchedulerCfg.sScheduledSlotLiquidationFile.c_str());

    if(ifsScheduledSlotLiquidationFile.is_open())
    {
        while(!ifsScheduledSlotLiquidationFile.eof())
        {
            char sNewLine[20480];
            ifsScheduledSlotLiquidationFile.getline(sNewLine, sizeof(sNewLine));

            if(sNewLine[0] != '#' && strlen(sNewLine) != 0)
            {
                std::istringstream cScheduledSlotLiquidationLineStream(sNewLine);
                string sElement;

                KOEpochTime cLiquidationTime;

                string sLiquidationTime;
                vector<string> vSlotsToBeLiquidated;

                int index = 0;
                while (std::getline(cScheduledSlotLiquidationLineStream, sElement, ';'))
                {
                    if(index == 0)
                    {
                        sLiquidationTime = sElement;

                        string sHour = sElement.substr(0,2);
                        string sMinute = sElement.substr(2,2);
                        string sSecond = sElement.substr(4,2);

                        cLiquidationTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(_cSchedulerCfg.sDate, sHour.c_str(), sMinute.c_str(), sSecond.c_str());
                    }
                    else
                    {
                        string sSlot;
                        sSlot = sElement;
                        vSlotsToBeLiquidated.push_back(sSlot);

                        stringstream cStringStream;
                        cStringStream << "Adding new scheduled slot liquidation " << sLiquidationTime << " " << sSlot << ".";
                        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                    }

                    index = index + 1;
                }

                _vScheduledSlotLiquidation.push_back(std::make_pair(cLiquidationTime, vSlotsToBeLiquidated));
            }
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Cannot load scheduled slot liquidation file " << _cSchedulerCfg.sScheduledSlotLiquidationFile << ".";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

}

void SchedulerBase::sortTimeEvent()
{
    TimeEventComparison cComparison;
    stable_sort(_vStaticTimeEventQueue.begin(), _vStaticTimeEventQueue.end(), cComparison);
}

void SchedulerBase::processTimeEvents(KOEpochTime cTimeNow)
{
    vector<TimeEvent*> vEventTriggered;

    while(!_cDynamicTimeEventQueue.empty())
    {
        if(_cDynamicTimeEventQueue.top()->checkTime(cTimeNow))
        {
            TimeEvent* pTimeEvent = _cDynamicTimeEventQueue.top();
            _cDynamicTimeEventQueue.pop();
            vEventTriggered.push_back(pTimeEvent);
        }
        else
        {
            //since the list is sorted, we know we have processed all the figures for this time
            break;
        }
    }

    while(_vStaticTimeEventQueue.size() > 0 && _vStaticTimeEventQueue.front()->cgetCallTime() <= cTimeNow)
    {
        vEventTriggered.push_back(_vStaticTimeEventQueue.front());
        _vStaticTimeEventQueue.pop_front();
    }

    TimeEventComparison cComparison;
    sort(vEventTriggered.begin(), vEventTriggered.end(), cComparison);

    for(vector<TimeEvent*>::iterator itr = vEventTriggered.begin();
        itr != vEventTriggered.end();
        itr++)
    {
        _cCurrentKOEpochTime = (*itr)->cgetCallTime();
        (*itr)->makeCall();
        delete *itr;
    }

    _cCurrentKOEpochTime = cTimeNow;
}

void SchedulerBase::addNewWakeupCall(KOEpochTime cCallTime, TimerCallbackInterface* pTarget)
{
    WakeupEvent* pNewWakeupCall = new WakeupEvent(pTarget, cCallTime);
    _vStaticTimeEventQueue.push_back(pNewWakeupCall);
}

void SchedulerBase::addNewFigureCall(TradeEngineBase* pTargetEngine, FigureCallPtr pNewFigureCall)
{
    FigureEvent* pNewFigureEventCall = new FigureEvent(pTargetEngine, pNewFigureCall);
    _vStaticTimeEventQueue.push_back(pNewFigureEventCall);
}

void SchedulerBase::addNewEngineCall(TradeEngineBase* pTargetEngine, EngineEvent::EngineEventType eEventType, KOEpochTime cCallTime)
{
    EngineEvent* pNewEngineCall = new EngineEvent(pTargetEngine, eEventType, cCallTime);
    _vStaticTimeEventQueue.push_back(pNewEngineCall);
}

}
