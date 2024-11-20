#include <fstream>
#include <signal.h>

#include "SchedulerBase.h"
#include "ErrorHandler.h"
#include "ContractAccount.h"
#include "SystemClock.h"
#include "WakeupEvent.h"
#include "FigureEvent.h"
#include "EngineEvent.h"
#include "../slsl/SLSL.h"
#include "../sl3l/SL3L.h"
#include "../slslUSFigure/SLSLUSFigure.h"
#include "../slrl/SLRL.h"
#include "../slrd/SLRD.h"
#include "../DataPrinter/DataPrinter.h"
#include "../DataRecorder/DataRecorder.h"

#include <boost/math/special_functions/round.hpp>

using std::cout;
using std::cerr;
using std::stringstream;
using std::ifstream;

namespace KO
{

SchedulerBase* SchedulerBase::_pInstance = NULL;

SchedulerBase::SchedulerBase(const string& sSimType, SchedulerConfig &cfg)
:_cSchedulerCfg(cfg)
{
    _sSimType = sSimType;

    cout.precision(20);
    cerr.precision(20);

    _cActualStartedTime = KOEpochTime(0,0);

    _pInstance = this;

    _eNextPendingManualCommand = NONE;
}

SchedulerBase::~SchedulerBase()
{
    delete _pTradeSignalMerger;
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

    ErrorHandler::GetInstance()->init(bisLiveTrading(), _cSchedulerCfg.sErrorWarningLogLevel, _cSchedulerCfg.sLogPath);

    _pStaticDataHandler.reset(new StaticDataHandler(_cSchedulerCfg.sFXRateFile, _cSchedulerCfg.sProductSpecFile, _cSchedulerCfg.sTickSizeFile, _cSchedulerCfg.sDate));

    _pTradeSignalMerger = new TradeSignalMerger(this);

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
        _cOrderActionLogger.openFile(_cSchedulerCfg.sLogPath + "/OrderActionLog.out", true, false);
    }
    else
    {
        _cOrderActionLogger.openFile(_cSchedulerCfg.sLogPath + "/OrderActionLog.out", false, false);
    }

    if(_cSchedulerCfg.bLogMarketData)
    {
        _cMarketDataLogger.openFile(_cSchedulerCfg.sLogPath + "/MarketDataLog.out", true, true);
    }
    else
    {
        _cMarketDataLogger.openFile(_cSchedulerCfg.sLogPath + "/MarketDataLog.out", false, true);
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
    
    registerTradeEngines();   

    loadTodaysFigure(_cSchedulerCfg.sDate); 

    for(vector<TradeEngineBasePtr>::iterator itr = _vTradeEngines.begin(); itr != _vTradeEngines.end(); itr++)
    {
        (*itr)->dayInit();
    }

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

    string sCurrency = _pStaticDataHandler->sGetCurrency(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange);
    string sContractDollarFXPair = sCurrency + "USD";
    pNewQuoteDataPtr->dRateToDollar = _pStaticDataHandler->dGetFXRate(sContractDollarFXPair);
    pNewQuoteDataPtr->sCurrency = sCurrency;
    pNewQuoteDataPtr->dTickSize = _pStaticDataHandler->dGetTickSize(sFullProductName);

    pNewQuoteDataPtr->dTradingFee = _pStaticDataHandler->dGetTradingFee(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange);
    pNewQuoteDataPtr->dContractSize = _pStaticDataHandler->dGetContractSize(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange);
    pNewQuoteDataPtr->sHCExchange = _pStaticDataHandler->sGetHCExchange(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange);

    if(_pStaticDataHandler->sGetProductTyep(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange) == "FUTURE")
    {
        pNewQuoteDataPtr->iMaxSpreadWidth = 5;
    }
    else if(_pStaticDataHandler->sGetProductTyep(pNewQuoteDataPtr->sRoot, pNewQuoteDataPtr->sExchange) == "FX")
    {
        pNewQuoteDataPtr->iMaxSpreadWidth = 1000000;
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

    pNewQuoteDataPtr->cLastUpdateTime = KOEpochTime();
    pNewQuoteDataPtr->bStalenessErrorTriggered = false;

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
    _vContractQuoteDatas[iProductIndex]->cLastUpdateTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
    _vContractQuoteDatas[iProductIndex]->bStalenessErrorTriggered = false;

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

void SchedulerBase::checkProductPriceStatus(KOEpochTime cCallTime)
{
    std::map<string, long> mLastExchangeUpdateTimes;
    std::map<string, bool> mExchangeOpen;

    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); ++i)
    {
        std::map<string, bool>::iterator itrExchangeOpen;
        itrExchangeOpen = mExchangeOpen.find(_vContractQuoteDatas[i]->sExchange);
        if(itrExchangeOpen == mExchangeOpen.end())
        {
            mExchangeOpen[_vContractQuoteDatas[i]->sExchange] = false;
        }

        if(cCallTime > _vContractQuoteDatas[i]->cMarketOpenTime &&
           cCallTime < _vContractQuoteDatas[i]->cMarketCloseTime)
        {
            mExchangeOpen[_vContractQuoteDatas[i]->sExchange] = mExchangeOpen[_vContractQuoteDatas[i]->sExchange] || true;
        }
        else
        {
            mExchangeOpen[_vContractQuoteDatas[i]->sExchange] = mExchangeOpen[_vContractQuoteDatas[i]->sExchange] || false;
        }

        if(cCallTime > _cActualStartedTime + KOEpochTime(90,0) &&
           cCallTime > _vContractQuoteDatas[i]->cMarketOpenTime + KOEpochTime(90,0) &&
           cCallTime < _vContractQuoteDatas[i]->cMarketCloseTime)
        {
            std::map<string,long>::iterator itr;
            itr = mLastExchangeUpdateTimes.find(_vContractQuoteDatas[i]->sExchange);
            if(itr != mLastExchangeUpdateTimes.end())
            {
                if(mLastExchangeUpdateTimes[_vContractQuoteDatas[i]->sExchange] < _vContractQuoteDatas[i]->cLastUpdateTime.sec())
                {
                    mLastExchangeUpdateTimes[_vContractQuoteDatas[i]->sExchange] = _vContractQuoteDatas[i]->cLastUpdateTime.sec();
                }
            }
            else
            {
                mLastExchangeUpdateTimes[_vContractQuoteDatas[i]->sExchange] = _vContractQuoteDatas[i]->cLastUpdateTime.sec();
            }

            if(_vContractQuoteDatas[i]->bStalenessErrorTriggered == false)
            {
                if(_vContractQuoteDatas[i]->cLastUpdateTime.igetPrintable() != 0)
                {
                    if((SystemClock::GetInstance()->cgetCurrentKOEpochTime() - _vContractQuoteDatas[i]->cLastUpdateTime).sec() > 3600)
                    {
                        _vContractQuoteDatas[i]->bStalenessErrorTriggered = true;
                        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", _vContractQuoteDatas[i]->sProduct, "Price staled for more than 1 hour");
                    }
                }
            }

            if(_vContractQuoteDatas[i]->iBidSize == 0 ||
               _vContractQuoteDatas[i]->iAskSize == 0 ||
               _vContractQuoteDatas[i]->dBestAsk - _vContractQuoteDatas[i]->dBestBid < 0.00000001)
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

    for(std::map<string, long>::iterator itr = mLastExchangeUpdateTimes.begin(); itr != mLastExchangeUpdateTimes.end(); ++itr)
    {
        if(mExchangeOpen[itr->first] == true)
        {
            if(cCallTime.sec() - itr->second > 300)
            {
                std::map<string, bool>::iterator triggeredItr;
                triggeredItr = _mExchangeStalenessTriggered.find(itr->first);

                bool bTriggered;    
                if(triggeredItr == _mExchangeStalenessTriggered.end())
                {
                    bTriggered = false;
                }
                else
                {
                    bTriggered = _mExchangeStalenessTriggered[itr->first];
                }

                if(bTriggered == false)
                {
                    stringstream cStringStream;
                    cStringStream << "Prices on " << itr->first << " staled for more than 5 mins";
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                    _mExchangeStalenessTriggered[itr->first] = true;
                }
            }
            else
            {
                _mExchangeStalenessTriggered[itr->first] = false;
            }
        }
    }
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

            exitScheduler();
        }
    }
    else
    {
        updateAllPnL();

        if(cCallTime.sec() % 3600 == 0)
        {
            _pTradeSignalMerger->takePosSnapShot();
        }

        checkProductPriceStatus(cCallTime);

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
                        if((*itr)->sgetEngineSlotName() == *slotItr)
                        {
                            (*itr)->manualHaltTrading();
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

                scheduledManualActionItr = _vScheduledManualActions.erase(scheduledManualActionItr);
            }
            else
            {
                scheduledManualActionItr++;
            }
        }

        if(_eNextPendingManualCommand == RESUME_ALL)
        {
            resumeAllTraders(0);
        }
        else if(_eNextPendingManualCommand == PATIENT_ALL)
        {
            patientLiqAllTraders(0);
        }
        else if(_eNextPendingManualCommand == LIMIT_ALL)
        {
            limLiqAllTraders(0);
        }
        else if(_eNextPendingManualCommand == FAST_ALL)
        {
            fastLiqAllTraders(0);
        }
        else if(_eNextPendingManualCommand == HALT_ALL)
        {
            haltAllTraders(0);
        }
        else if(_eNextPendingManualCommand == LIMIT_ALL_LIQUIDATOR)
        {
            limitLiqAllLiquidator(0);
        }
        else if(_eNextPendingManualCommand == FAST_ALL_LIQUIDATOR)
        {
            fastLiqAllLiquidator(0);
        }
        else if(_eNextPendingManualCommand == LIMIT_SLOTS_LIQUIDATOR)
        {
            limitLiqSlotsLiquidator(0);
        }
        else if(_eNextPendingManualCommand == FAST_SLOTS_LIQUIDATOR)
        {
            fastLiqSlotsLiquidator(0);
        }
        else if(_eNextPendingManualCommand == POS_PRINT)
        {
            posPrint(0);
        }
        else if(_eNextPendingManualCommand == LIMIT_PRODUCT)
        {
            limitLiquidateProduct(0);
        }
        else if(_eNextPendingManualCommand == FAST_PRODUCT)
        {
            fastLiquidateProduct(0);
        }
        else if(_eNextPendingManualCommand == PATIENT_PRODUCT)
        {
            patientLiquidateProduct(0);
        }
        else if(_eNextPendingManualCommand == RESET_ORDER_STATE)
        {
            resetOrderStateHandler(0);
        }
        else if(_eNextPendingManualCommand == PRINT_THEO_TARGET)
        {
            printTheoTargetHandler(0);
        }
        else if(_eNextPendingManualCommand == PRINT_SIGNAL)
        {
            printSignalHandler(0);
        }
        else if(_eNextPendingManualCommand == FORBID_LP)
        {
            addForbiddenFXLP(0);
        }
        _eNextPendingManualCommand = NONE;

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

                if(sEngineType.compare("SLSL") == 0)
                {
                    pNewTradeEngine.reset(new SLSL(sEngineRunTimePath, sEngineSlotName, cTradingStartTime, cTradingEndTime, this, _cSchedulerCfg.sDate, _sSimType));
                }
                else if(sEngineType.compare("DataPrinter") == 0)
                {
                    pNewTradeEngine.reset(new DataPrinter(sEngineRunTimePath, sEngineSlotName, cTradingStartTime, cTradingEndTime, this, _cSchedulerCfg.sDate, _sSimType));
                }
                else if(sEngineType.compare("DataRecorder") == 0)
                {
                    pNewTradeEngine.reset(new DataRecorder(sEngineRunTimePath, sEngineSlotName, cTradingStartTime, cTradingEndTime, this, _cSchedulerCfg.sDate, _sSimType));
                }
                else if(sEngineType.compare("SL3L") == 0)
                {
                    pNewTradeEngine.reset(new SL3L(sEngineRunTimePath, sEngineSlotName, cTradingStartTime, cTradingEndTime, this, _cSchedulerCfg.sDate, _sSimType));
                }
                else if(sEngineType.compare("SLSLUSFigure") == 0)
                {
                    pNewTradeEngine.reset(new SLSLUSFigure(sEngineRunTimePath, sEngineSlotName, cTradingStartTime, cTradingEndTime, this, _cSchedulerCfg.sDate, _sSimType));
                }
                else if(sEngineType.compare("SLRL") == 0)
                {
                    pNewTradeEngine.reset(new SLRL(sEngineRunTimePath, sEngineSlotName, cTradingStartTime, cTradingEndTime, this, _cSchedulerCfg.sDate, _sSimType));
                }
                else if(sEngineType.compare("SLRD") == 0)
                {
                    pNewTradeEngine.reset(new SLRD(sEngineRunTimePath, sEngineSlotName, cTradingStartTime, cTradingEndTime, this, _cSchedulerCfg.sDate, _sSimType));
                }
                else
                {
                    bEngineConfigValid = false;
                    stringstream cStringStream;
                    cStringStream << "Engine type " << sEngineType << " not registered \n";
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                }

                pNewTradeEngine->setOutputBasePath(_cSchedulerCfg.sOutputBasePath);

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

                    if(i == 0)
                    {
                        pNewTradeEngine->_iSlotID = _pTradeSignalMerger->registerTradingSlot(pNewTradeEngine->vContractQuoteDatas[0]->sProduct, sAccount, pNewTradeEngine->vContractQuoteDatas[0]->dContractSize, pNewTradeEngine->vContractQuoteDatas[0]->dRateToDollar, pNewTradeEngine->vContractQuoteDatas[0]->dTradingFee);
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

            if(sDelimiter.compare("BaseSignal") == 0)
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

void SchedulerBase::updateProductPnL(int iProductIndex)
{
    double dProductMidPrice = _vContractQuoteDatas[iProductIndex]->dWeightedMid;

    double dPnLInPrice = _vProductConsideration[iProductIndex];
    long iProductTotalPos = _vProductPos[iProductIndex] + _vProductLiquidationPos[iProductIndex];

    if(iProductTotalPos != 0)
    {
        dPnLInPrice = dPnLInPrice + iProductTotalPos * dProductMidPrice;
    }

    double dPnLInDollar = dPnLInPrice * _vContractQuoteDatas[iProductIndex]->dRateToDollar * _vContractQuoteDatas[iProductIndex]->dContractSize;

    if(_vContractQuoteDatas[iProductIndex]->sProduct.substr(0, 1) == "L")
    {
        dPnLInDollar = dPnLInDollar * 2;
    }

    double dFee = _vProductVolume[iProductIndex] * _vContractQuoteDatas[iProductIndex]->dTradingFee * _vContractQuoteDatas[iProductIndex]->dRateToDollar;
    double dFinalPnL = dPnLInDollar - dFee;

    stringstream cStringStream;
    cStringStream << "PNL: " << dFinalPnL << " Volume: " << _vProductVolume[iProductIndex] << " Position: " << iProductTotalPos;
    ErrorHandler::GetInstance()->newInfoMsg("HB", "ALL", _vContractQuoteDatas[iProductIndex]->sProduct, cStringStream.str());

    if(_vProductLiquidating[iProductIndex] == false)
    {
        if(dFinalPnL < -1 * _vProductStopLoss[iProductIndex])
        {
            _vProductAboveSLInSec[iProductIndex] = _vProductAboveSLInSec[iProductIndex] + 1;

            if(_vProductAboveSLInSec[iProductIndex] > 10)
            {
                _vProductLiquidating[iProductIndex] = true;
                cStringStream.str(std::string());
                cStringStream << "Liquidating " << _vContractQuoteDatas[iProductIndex]->sProduct << ". Stop Loss " << _vProductStopLoss[iProductIndex] << " reached for more than 10 seconds.";
                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", _vContractQuoteDatas[iProductIndex]->sProduct, cStringStream.str());

                for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
                {
                    if(_vContractQuoteDatas[iProductIndex]->sRoot == (*itr)->vContractAccount[0]->sgetRootSymbol())
                    {
                        (*itr)->manualLimitLiquidation();                    
                    }
                }
            }
        }
        else
        {
            _vProductAboveSLInSec[iProductIndex] = 0;
        }
    }
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
                string sSecond = "00";

                if(pNewFigure->sFigureName == "GBP_ConsumerPriceIndex")
                {
                    if(sHour == "06")
                    {
                        sHour = "07";
                    }
                    else if(sHour == "07")
                    {
                        sHour = "08";
                    }

                    sSecond = "01";
                }

                pNewFigure->cFigureTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromUTC(sFigureDay, sHour, sMinute, sSecond);

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
                else if(sActionType.compare("FLAT_FX_HITTING") == 0)
                {
                    _mProductFigureFlatFXHitting[sProduct] = vNewProductFigureActions;
                }
                else if(sActionType.compare("FLAT_FX_LONG_HITTING") == 0)
                {
                    _mProductFigureFlatFXLongHitting[sProduct] = vNewProductFigureActions;
                }
                else if(sActionType.compare("SLOW_FLAT") == 0)
                {
                    _mProductFigureSlowFlat[sProduct] = vNewProductFigureActions;
                }
                else if(sActionType.compare("FLAT_OFF") == 0)
                {
                    _mProductFigureFlatOff[sProduct] = vNewProductFigureActions;
                }
                else if(sActionType.compare("FLAT_BIG") == 0)
                {
                    _mProductFigureFlatBig[sProduct] = vNewProductFigureActions;
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
/*
     for(vector<TradeEngineBasePtr>::iterator itrEngine = _vTradeEngines.begin(); itrEngine != _vTradeEngines.end(); itrEngine++)
    {
        vector<string> vRegisteredFigureProducts = (*itrEngine)->vgetRegisterdFigureProducts();
        for(vector<string>::iterator itrFigureProducts = vRegisteredFigureProducts.begin();
            itrFigureProducts != vRegisteredFigureProducts.end();
            itrFigureProducts++)
        {
            if(*itrFigureProducts == "ES" || *itrFigureProducts == "ZN" || *itrFigureProducts == "ZF" || *itrFigureProducts == "ZT")
            { 
                KOEpochTime cFigureTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(_cSchedulerCfg.sDate, "19", "00", "00");
                KOEpochTime cPreFigureCallTime = cFigureTime - KOEpochTime(5 * 60,0);
                KOEpochTime cPostFigureCallTime = cFigureTime + KOEpochTime(250 * 60,0);
                addNewFigureCall((*itrEngine), "EUCLOSE", cPreFigureCallTime, cFigureTime, FigureAction::PATIENT);
                addNewFigureCall((*itrEngine), "EUCLOSE", cPostFigureCallTime, cFigureTime, FigureAction::PATIENT);
            }
        }
    } 
*/

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
                                if(sFigureName.compare("EUR_EurECBMainRefinancingOperationsRate") == 0)
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
                                if(sFigureName.compare("EUR_EurECBMainRefinancingOperationsRate") == 0)
                                {
                                    iPostFigureTimeOut = 150;
                                }
                                else if(sFigureName.compare("GBP_BoEInterestRateDecision") == 0)
                                {
                                    iPostFigureTimeOut = 120;
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
                                else if(sFigureName.compare("USD_NonfarmPayrolls") == 0)
                                {
                                    iPostFigureTimeOut = 1200;
                                }
                                else if(sFigureName.compare("GBP_ConsumerPriceIndex") == 0)
                                {
                                    iPostFigureTimeOut = 240;
                                }
                                else if(sFigureName.compare("USD_ConsumerPriceIndex") == 0)
                                {
                                    iPostFigureTimeOut = 1000;
                                }
                                else if(sFigureName.compare("EUR_GerHCOBManufacturingPMIPreliminar") == 0)
                                {
                                    iPostFigureTimeOut = 60;
                                }
                                else if(sFigureName.compare("GBP_S&PGlobalCIPSManufacturingPMIPreliminar") == 0)
                                {
                                    iPostFigureTimeOut = 60;
                                }

                                KOEpochTime cFigureTime = (*itr)->cFigureTime;

                                // pre figure time out is set to 20 mins when liquidating for figure
                                KOEpochTime cLimLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(25 * 60,0);
                                if(sFigureName.compare("GBP_ConsumerPriceIndex") == 0)
                                {
                                    cLimLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(0, 1);
                                }
                                if(sFigureName.compare("EUR_GerHCOBManufacturingPMIPreliminar") == 0)
                                {
                                    cLimLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(89 * 60 + 59, 0);
                                }
                                addNewFigureCall((*itrEngine), sFigureName, cLimLiqFigureCallTime, cFigureTime, FigureAction::LIM_LIQ);
                                stringstream cStringStream;
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat Figure lim liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cLimLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                cStringStream.str(std::string());
                                KOEpochTime cFastLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(5 * 60,0);
                                if(sFigureName.compare("GBP_ConsumerPriceIndex") == 0)
                                {
                                    cFastLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(0, 1);
                                }
                                if(sFigureName.compare("EUR_GerHCOBManufacturingPMIPreliminar") == 0)
                                {
                                    cFastLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(89 * 60 + 59, 0);
                                }
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

                    map<string, boost::shared_ptr< vector <string> > >::iterator productFlatOffFiguresItr = _mProductFigureFlatOff.find(*itrFigureProducts);
                    if(productFlatOffFiguresItr != _mProductFigureFlatOff.end())
                    {
                        boost::shared_ptr< vector <string> > pFlatOffFigures = productFlatOffFiguresItr->second;

                        for(vector<string>::iterator itrFlatOffFigures = pFlatOffFigures->begin();
                            itrFlatOffFigures != pFlatOffFigures->end();
                            itrFlatOffFigures++)
                        {
                            string sFigureName = *itrFlatOffFigures;

                            if(sFigureName.compare((*itr)->sFigureName) == 0)
                            {
                                long iPostFigureTimeOut = 2000;

                                KOEpochTime cFigureTime = (*itr)->cFigureTime;

                                // pre figure time out is set to 20 mins when liquidating for figure
                                KOEpochTime cLimLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(25 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cLimLiqFigureCallTime, cFigureTime, FigureAction::LIM_LIQ);
                                stringstream cStringStream;
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat Off Figure lim liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cLimLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                cStringStream.str(std::string());
                                KOEpochTime cFastLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(5 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cFastLiqFigureCallTime, cFigureTime, FigureAction::FAST_LIQ);
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat Off Figure fast liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cFastLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                cStringStream.str(std::string());
                                KOEpochTime cPostFigureCallTime = (*itr)->cFigureTime + KOEpochTime(iPostFigureTimeOut * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPostFigureCallTime, cFigureTime, FigureAction::FAST_LIQ);
                                cStringStream << "Added " << *itrFigureProducts << " Post Flat Off Figure call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPostFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                            }
                        }
                    }
                    else
                    {
                        stringstream cStringStream;
                        cStringStream << "No flat off figures defined for product " << *itrFigureProducts;
                        ErrorHandler::GetInstance()->newInfoMsg("0", (*itrEngine)->sgetEngineSlotName(), *itrFigureProducts, cStringStream.str());
                    }

                    map<string, boost::shared_ptr< vector <string> > >::iterator productFlatBigFiguresItr = _mProductFigureFlatBig.find(*itrFigureProducts);
                    if(productFlatBigFiguresItr != _mProductFigureFlatBig.end())
                    {
                        boost::shared_ptr< vector <string> > pFlatBigFigures = productFlatBigFiguresItr->second;

                        for(vector<string>::iterator itrFlatBigFigures = pFlatBigFigures->begin();
                            itrFlatBigFigures != pFlatBigFigures->end();
                            itrFlatBigFigures++)
                        {
                            string sFigureName = *itrFlatBigFigures;

                            if(sFigureName.compare((*itr)->sFigureName) == 0)
                            {
                                long iPostFigureTimeOut = 15;

                                KOEpochTime cFigureTime = (*itr)->cFigureTime;

                                // pre figure time out is set to 20 mins when liquidating for figure
                                KOEpochTime cLimLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(25 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cLimLiqFigureCallTime, cFigureTime, FigureAction::LIM_LIQ);
                                stringstream cStringStream;
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat Off Figure lim liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cLimLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                cStringStream.str(std::string());
                                KOEpochTime cFastLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(5 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cFastLiqFigureCallTime, cFigureTime, FigureAction::FAST_LIQ);
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat Off Figure fast liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cFastLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                cStringStream.str(std::string());
                                KOEpochTime cPostFigureCallTime = (*itr)->cFigureTime + KOEpochTime(iPostFigureTimeOut * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPostFigureCallTime, cFigureTime, FigureAction::FAST_LIQ);
                                cStringStream << "Added " << *itrFigureProducts << " Post Flat Off Figure call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPostFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                            }
                        }
                    }
                    else
                    {
                        stringstream cStringStream;
                        cStringStream << "No flat off figures defined for product " << *itrFigureProducts;
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
                                if(sFigureName.compare("EUR_EurECBMainRefinancingOperationsRate") == 0)
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

                    map<string, boost::shared_ptr< vector <string> > >::iterator productFlatFXHittingFiguresItr = _mProductFigureFlatFXHitting.find(*itrFigureProducts);
                    if(productFlatFXHittingFiguresItr != _mProductFigureFlatFXHitting.end())
                    {
                        boost::shared_ptr< vector <string> > pFlatFXHittingFigures = productFlatFXHittingFiguresItr->second;

                        for(vector<string>::iterator itrFlatFXHittingFigures = pFlatFXHittingFigures->begin();
                            itrFlatFXHittingFigures != pFlatFXHittingFigures->end();
                            itrFlatFXHittingFigures++)
                        {
                            string sFigureName = *itrFlatFXHittingFigures;

                            if(sFigureName.compare((*itr)->sFigureName) == 0)
                            {
                                KOEpochTime cFigureTime = (*itr)->cFigureTime;

                                // pre figure time out is set to 20 mins when liquidating for figure
                                KOEpochTime cPatientFigureCallTime = (*itr)->cFigureTime - KOEpochTime(15 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPatientFigureCallTime, cFigureTime, FigureAction::PATIENT);
                                stringstream cStringStream;
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat FX Hitting Figure patient call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPatientFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                                
                                KOEpochTime cLimLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(10 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cLimLiqFigureCallTime, cFigureTime, FigureAction::LIM_LIQ);
                                cStringStream.str(std::string());
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat FX Hitting Figure lim liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cLimLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                KOEpochTime cFastLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(5 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cFastLiqFigureCallTime, cFigureTime, FigureAction::FAST_LIQ);
                                cStringStream.str(std::string());
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat FX Hitting Figure fast liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cFastLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                KOEpochTime cPostFigureCallTime = (*itr)->cFigureTime + KOEpochTime(60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPostFigureCallTime, cFigureTime, FigureAction::NO_FIGURE);
                                cStringStream.str(std::string());
                                cStringStream << "Added " << *itrFigureProducts << " Post Flat FX Hitting Figure call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPostFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                            }
                        }
                    }
                    else
                    {
                        stringstream cStringStream;
                        cStringStream << "No flat FX hitting figures defined for product " << *itrFigureProducts;
                        ErrorHandler::GetInstance()->newInfoMsg("0", (*itrEngine)->sgetEngineSlotName(), *itrFigureProducts, cStringStream.str());
                    }

                    map<string, boost::shared_ptr< vector <string> > >::iterator productFlatFXLongHittingFiguresItr = _mProductFigureFlatFXLongHitting.find(*itrFigureProducts);
                    if(productFlatFXLongHittingFiguresItr != _mProductFigureFlatFXLongHitting.end())
                    {
                        boost::shared_ptr< vector <string> > pFlatFXLongHittingFigures = productFlatFXLongHittingFiguresItr->second;

                        for(vector<string>::iterator itrFlatFXLongHittingFigures = pFlatFXLongHittingFigures->begin();
                            itrFlatFXLongHittingFigures != pFlatFXLongHittingFigures->end();
                            itrFlatFXLongHittingFigures++)
                        {
                            string sFigureName = *itrFlatFXLongHittingFigures;

                            if(sFigureName.compare((*itr)->sFigureName) == 0)
                            {
                                KOEpochTime cFigureTime = (*itr)->cFigureTime;

                                // pre figure time out is set to 20 mins when liquidating for figure
                                KOEpochTime cPatientFigureCallTime = (*itr)->cFigureTime - KOEpochTime(15 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPatientFigureCallTime, cFigureTime, FigureAction::PATIENT);
                                stringstream cStringStream;
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat FX Hitting Figure patient call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPatientFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                                
                                KOEpochTime cLimLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(10 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cLimLiqFigureCallTime, cFigureTime, FigureAction::LIM_LIQ);
                                cStringStream.str(std::string());
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat FX Hitting Figure lim liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cLimLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                KOEpochTime cFastLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(5 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cFastLiqFigureCallTime, cFigureTime, FigureAction::FAST_LIQ);
                                cStringStream.str(std::string());
                                cStringStream << "Added " << *itrFigureProducts << " Pre Flat FX Hitting Figure fast liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cFastLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                KOEpochTime cPostFigureCallTime = (*itr)->cFigureTime + KOEpochTime(120,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPostFigureCallTime, cFigureTime, FigureAction::NO_FIGURE);
                                cStringStream.str(std::string());
                                cStringStream << "Added " << *itrFigureProducts << " Post Flat FX Hitting Figure call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPostFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                            }
                        }
                    }
                    else
                    {
                        stringstream cStringStream;
                        cStringStream << "No flat FX hitting figures defined for product " << *itrFigureProducts;
                        ErrorHandler::GetInstance()->newInfoMsg("0", (*itrEngine)->sgetEngineSlotName(), *itrFigureProducts, cStringStream.str());
                    }


                    map<string, boost::shared_ptr< vector <string> > >::iterator productSlowFlatFiguresItr = _mProductFigureSlowFlat.find(*itrFigureProducts);
                    if(productSlowFlatFiguresItr != _mProductFigureSlowFlat.end())
                    {
                        boost::shared_ptr< vector <string> > pSlowFlatFigures = productSlowFlatFiguresItr->second;

                        for(vector<string>::iterator itrSlowFlatFigures = pSlowFlatFigures->begin();
                            itrSlowFlatFigures != pSlowFlatFigures->end();
                            itrSlowFlatFigures++)
                        {
                            string sFigureName = *itrSlowFlatFigures;

                            if(sFigureName.compare((*itr)->sFigureName) == 0)
                            {
                                long iPostFigureTimeOut = 5;
                                if(sFigureName.compare("EUR_EurECBMainRefinancingOperationsRate") == 0)
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
                                else if(sFigureName.compare("USD_NonfarmPayrolls") == 0)
                                {
                                    iPostFigureTimeOut = 1200;
                                }
                                else if(sFigureName.compare("GBP_ConsumerPriceIndex") == 0)
                                {
                                    iPostFigureTimeOut = 1200;
                                }
                                else if(sFigureName.compare("USD_ConsumerPriceIndex") == 0)
                                {
                                    iPostFigureTimeOut = 1000;
                                }

                                KOEpochTime cFigureTime = (*itr)->cFigureTime;

                                // pre figure time out is set to 60 mins when liquidating for figure
                                KOEpochTime cPatientFigureCallTime = (*itr)->cFigureTime - KOEpochTime(75 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPatientFigureCallTime, cFigureTime, FigureAction::PATIENT);
                                stringstream cStringStream;
                                cStringStream << "Added " << *itrFigureProducts << " Pre Slow Flat Figure patient call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPatientFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                                
                                KOEpochTime cLimLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(60 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cLimLiqFigureCallTime, cFigureTime, FigureAction::LIM_LIQ);
                                cStringStream.str(std::string());
                                cStringStream << "Added " << *itrFigureProducts << " Pre Slow Flat Figure lim liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cLimLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                KOEpochTime cFastLiqFigureCallTime = (*itr)->cFigureTime - KOEpochTime(5 * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cFastLiqFigureCallTime, cFigureTime, FigureAction::FAST_LIQ);
                                cStringStream.str(std::string());
                                cStringStream << "Added " << *itrFigureProducts << " Pre Slow Flat Figure fast liq call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cFastLiqFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

                                KOEpochTime cPostFigureCallTime = (*itr)->cFigureTime + KOEpochTime(iPostFigureTimeOut * 60,0);
                                addNewFigureCall((*itrEngine), sFigureName, cPostFigureCallTime, cFigureTime, FigureAction::NO_FIGURE);
                                cStringStream.str(std::string());
                                cStringStream << "Added " << *itrFigureProducts << " Post Slow Flat Figure call " << sFigureName << " time " << cFigureTime.igetPrintable() << " call time " << cPostFigureCallTime.igetPrintable() << ".";
                                ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                            }
                        }
                    }
                    else
                    {
                        stringstream cStringStream;
                        cStringStream << "No slow flat figures defined for product " << *itrFigureProducts;
                        ErrorHandler::GetInstance()->newInfoMsg("0", (*itrEngine)->sgetEngineSlotName(), *itrFigureProducts, cStringStream.str());
                    }
                }
            }
        }
    }
}

void SchedulerBase::setResumeAllTraders(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = RESUME_ALL;
}

void SchedulerBase::setPatientLiqAllTraders(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = PATIENT_ALL;
}

void SchedulerBase::setLimLiqAllTraders(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = LIMIT_ALL;
}

void SchedulerBase::setFastLiqAllTraders(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = FAST_ALL;
}

void SchedulerBase::setHaltAllTraders(int aSignal)
{   
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = HALT_ALL;
}

void SchedulerBase::setLimitLiqSlotsLiquidator(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = LIMIT_SLOTS_LIQUIDATOR;
}

void SchedulerBase::setFastLiqSlotsLiquidator(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = FAST_SLOTS_LIQUIDATOR;
}

void SchedulerBase::setLimitLiqAllLiquidator(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = LIMIT_ALL_LIQUIDATOR;
}

void SchedulerBase::setFastLiqAllLiquidator(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = FAST_ALL_LIQUIDATOR;
}

void SchedulerBase::setPosPrint(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = POS_PRINT;
}

void SchedulerBase::setLimitLiquidateProduct(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = LIMIT_PRODUCT;
}

void SchedulerBase::setFastLiquidateProduct(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = FAST_PRODUCT;
}

void SchedulerBase::setPatientLiquidateProduct(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = PATIENT_PRODUCT;
}

void SchedulerBase::setResetOrderStateHandler(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = RESET_ORDER_STATE;
}

void SchedulerBase::setPrintTheoTargetHandler(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = PRINT_THEO_TARGET;
}

void SchedulerBase::setPrintSignalHandler(int aSignal)
{   
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = PRINT_SIGNAL;
}

void SchedulerBase::setAddForbiddenFXLP(int aSignal)
{
    (void) aSignal;
    _pInstance->_eNextPendingManualCommand = FORBID_LP;
}

void SchedulerBase::limLiqAllTraders(int aSignal)
{
    if(aSignal > 0 || (aSignal == 0 && _pInstance->bgetEngineManualHalted() == false))
    {
        for(unsigned int i = 0; i < _pInstance->_vProductLiquidating.size(); i++)
        {
            _pInstance->_vProductLiquidating[i] = true;
        }

        stringstream cStringStream;
        cStringStream << "Received signal to set engine to limit liquidation state.";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

        for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
        {
            (*itr)->manualLimitLiquidation();
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
            (*itr)->manualResumeTrading();
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
           (*itr)->manualPatientLiquidation();
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
            (*itr)->manualFastLiquidation();
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

void SchedulerBase::posPrint(int aSignal)
{
    (void) aSignal;
    stringstream cStringStream;
    cStringStream << "Received signal for pos print signal command.";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
    _pInstance->_pTradeSignalMerger->printConfigPos();
}

void SchedulerBase::limitLiqSlotsLiquidator(int aSignal)
{
    if(aSignal > 0 || (aSignal == 0 && _pInstance->bgetEngineManualHalted() == false))
    {
        stringstream cStringStream;
        cStringStream << "Received signal for limit liq slots liquidator.";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
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
            (*itr)->manualHaltTrading();
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
            (*itr)->manualHaltTrading();
        }
    }

    if(aSignal > 0)
    {
        _pInstance->setEngineManualHalted(true);
    }
}

void SchedulerBase::limitLiquidateProduct(int aSignal)
{
    (void) aSignal;
    stringstream cStringStream;
    cStringStream << "Received signal for limit liq product.";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    _pInstance->liquidatProduct(LIMIT_LIQ);
}

void SchedulerBase::fastLiquidateProduct(int aSignal)
{
    (void) aSignal;
    stringstream cStringStream;
    cStringStream << "Received signal for fast liq product.";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    _pInstance->liquidatProduct(FAST_LIQ);
}

void SchedulerBase::patientLiquidateProduct(int aSignal)
{
    (void) aSignal;
    stringstream cStringStream;
    cStringStream << "Received signal for patient liq product.";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    _pInstance->liquidatProduct(PATIENT_LIQ);
}

void SchedulerBase::resetOrderStateHandler(int aSignal)
{
    (void) aSignal;
    stringstream cStringStream;
    cStringStream << "Received signal for reset order state.";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    _pInstance->resetOrderState();
}

void SchedulerBase::printTheoTargetHandler(int aSignal)
{
    (void) aSignal;
    stringstream cStringStream;
    cStringStream << "Received signal for print theo target.";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    _pInstance->printTheoTarget();
}

void SchedulerBase::printTheoTarget()
{
    for(vector<TradeEngineBasePtr>::iterator itr = _vTradeEngines.begin(); itr != _vTradeEngines.end(); itr++)
    {
        (*itr)->printTheoTarget();
    }
}

void SchedulerBase::printSignalHandler(int aSignal)
{
    (void) aSignal;
    stringstream cStringStream;
    cStringStream << "Received signal for print theo target.";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    _pInstance->printSignal();
}

void SchedulerBase::printSignal()
{
    for(vector<TradeEngineBasePtr>::iterator itr = _vTradeEngines.begin(); itr != _vTradeEngines.end(); itr++)
    {
        (*itr)->printSignal();
    }
}

void SchedulerBase::addForbiddenFXLP(int aSignal)
{
    (void) aSignal;
    stringstream cStringStream;
    cStringStream << "Received signal for add forbidden FX LP.";
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    _pInstance->readForbiddenFXLPList();
}

void SchedulerBase::readForbiddenFXLPList()
{
    ifstream ifsForbiddenFXLPsFile (_cSchedulerCfg.sForbiddenFXLPsFile);

    _vForbiddenFXLPs.clear();

    if(ifsForbiddenFXLPsFile.is_open())
    {
        while(!ifsForbiddenFXLPsFile.eof())
        {
            char sNewLine[256];
            ifsForbiddenFXLPsFile.getline(sNewLine, sizeof(sNewLine));

            string sForbiddenFXLP = sNewLine;

            _vForbiddenFXLPs.push_back(sForbiddenFXLP);
        }
    }
}

void SchedulerBase::liquidatProduct(LiquidationType eLiquidationType)
{
    ifstream ifsLiquidationCommandFile (_cSchedulerCfg.sLiquidationCommandFile);

    if(ifsLiquidationCommandFile.is_open())
    {
        while(!ifsLiquidationCommandFile.eof())
        {
            char sNewLine[256];
            ifsLiquidationCommandFile.getline(sNewLine, sizeof(sNewLine));

            string sProductToBeLiquidated = sNewLine;

            for(unsigned int i = 0; i < _vContractQuoteDatas.size(); ++i)
            {
                if(_vContractQuoteDatas[i]->sRoot == sProductToBeLiquidated)
                {
                    _vProductLiquidating[i] = true;
                }
            }

            for(vector<TradeEngineBasePtr>::iterator itr = _pInstance->_vTradeEngines.begin(); itr != _pInstance->_vTradeEngines.end(); itr++)
            {
                if(sProductToBeLiquidated == (*itr)->vContractAccount[0]->sgetRootSymbol())
                {
                    if(eLiquidationType == LIMIT_LIQ)
                    {
                        (*itr)->manualLimitLiquidation();                    
                    }
                    else if(eLiquidationType == FAST_LIQ)
                    {
                        (*itr)->manualFastLiquidation();                    
                    }
                    else if(eLiquidationType == PATIENT_LIQ)
                    {
                        (*itr)->manualPatientLiquidation();                    
                    }
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

    maxSigAction.sa_handler = SchedulerBase::setLimLiqAllTraders;
    if (-1 == sigaction(SIGRTMAX, &maxSigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for lim liq signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max1SigAction;

    sigemptyset(&max1SigAction.sa_mask);
    max1SigAction.sa_flags = 0;

    max1SigAction.sa_handler = SchedulerBase::setResumeAllTraders;
    if (-1 == sigaction(SIGRTMAX-1, &max1SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for resume signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max2SigAction;

    sigemptyset(&max2SigAction.sa_mask);
    max2SigAction.sa_flags = 0;

    max2SigAction.sa_handler = SchedulerBase::setPatientLiqAllTraders;
    if (-1 == sigaction(SIGRTMAX-2, &max2SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for patient liq signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max3SigAction;

    sigemptyset(&max3SigAction.sa_mask);
    max3SigAction.sa_flags = 0;

    max3SigAction.sa_handler = SchedulerBase::setFastLiqAllTraders;
    if (-1 == sigaction(SIGRTMAX-3, &max3SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for fast liq signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max4SigAction;

    sigemptyset(&max4SigAction.sa_mask);
    max4SigAction.sa_flags = 0;

    max4SigAction.sa_handler = SchedulerBase::setHaltAllTraders;
    if (-1 == sigaction(SIGRTMAX-4, &max4SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for halt all signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max5SigAction;

    sigemptyset(&max5SigAction.sa_mask);
    max5SigAction.sa_flags = 0;

    max5SigAction.sa_handler = SchedulerBase::setPosPrint;
    if (-1 == sigaction(SIGRTMAX-5, &max5SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for pos print handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max7SigAction;

    sigemptyset(&max7SigAction.sa_mask);
    max7SigAction.sa_flags = 0;

    max7SigAction.sa_handler = SchedulerBase::setLimitLiqAllLiquidator;
    if (-1 == sigaction(SIGRTMAX-7, &max7SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for limit liq all liquidator signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max8SigAction;

    sigemptyset(&max8SigAction.sa_mask);
    max8SigAction.sa_flags = 0;

    max8SigAction.sa_handler = SchedulerBase::setFastLiqAllLiquidator;
    if (-1 == sigaction(SIGRTMAX-8, &max8SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for fast liq liquidator signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max10SigAction;

    sigemptyset(&max10SigAction.sa_mask);
    max10SigAction.sa_flags = 0;

    max10SigAction.sa_handler = SchedulerBase::setLimitLiqSlotsLiquidator;
    if (-1 == sigaction(SIGRTMAX-10, &max10SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for limit liq slots liquidator signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max11SigAction;

    sigemptyset(&max11SigAction.sa_mask);
    max11SigAction.sa_flags = 0;

    max11SigAction.sa_handler = SchedulerBase::setFastLiqSlotsLiquidator;
    if (-1 == sigaction(SIGRTMAX-11, &max11SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for fast liq slots liquidator signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max12SigAction;

    sigemptyset(&max12SigAction.sa_mask);
    max12SigAction.sa_flags = 0;

    max12SigAction.sa_handler = SchedulerBase::setLimitLiquidateProduct;
    if (-1 == sigaction(SIGRTMAX-12, &max12SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for limit liquidate product signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max13SigAction;

    sigemptyset(&max13SigAction.sa_mask);
    max13SigAction.sa_flags = 0;

    max13SigAction.sa_handler = SchedulerBase::setFastLiquidateProduct;
    if (-1 == sigaction(SIGRTMAX-13, &max13SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for fast liquidate product signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction max14SigAction;

    sigemptyset(&max14SigAction.sa_mask);
    max14SigAction.sa_flags = 0;

    max14SigAction.sa_handler = SchedulerBase::setResetOrderStateHandler;
    if (-1 == sigaction(SIGRTMAX-14, &max14SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for reset order state signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction minSigAction;

    sigemptyset(&minSigAction.sa_mask);
    minSigAction.sa_flags = 0;

    minSigAction.sa_handler = SchedulerBase::setPrintTheoTargetHandler;
    if (-1 == sigaction(SIGRTMIN, &minSigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for print target position signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction min1SigAction;

    sigemptyset(&min1SigAction.sa_mask);
    min1SigAction.sa_flags = 0;

    min1SigAction.sa_handler = SchedulerBase::setPrintSignalHandler;
    if (-1 == sigaction(SIGRTMIN+1, &min1SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for print signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction min2SigAction;

    sigemptyset(&min2SigAction.sa_mask);
    min2SigAction.sa_flags = 0;

    min2SigAction.sa_handler = SchedulerBase::setPatientLiquidateProduct;
    if (-1 == sigaction(SIGRTMIN+2, &min2SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for patient liquidate product signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    struct sigaction min3SigAction;

    sigemptyset(&min3SigAction.sa_mask);
    min3SigAction.sa_flags = 0;

    min3SigAction.sa_handler = SchedulerBase::setAddForbiddenFXLP;
    if (-1 == sigaction(SIGRTMIN+3, &min3SigAction, NULL))
    {
        stringstream cStringStream;
        cStringStream << "Failed to register for add Forbidden FX LP signal handler.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
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
    if(cCallTime > SystemClock::GetInstance()->cgetCurrentKOEpochTime())
    {
        WakeupEvent* pNewWakeupCall = new WakeupEvent(pTarget, cCallTime);
        _vStaticTimeEventQueue.push_back(pNewWakeupCall);
    }
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

void SchedulerBase::activateSlot(const string& sProduct, int iSlotID)
{
    _pTradeSignalMerger->activateSlot(sProduct, iSlotID);
}

void SchedulerBase::deactivateSlot(const string& sProduct, int iSlotID)
{
    _pTradeSignalMerger->deactivateSlot(sProduct, iSlotID);
}

void SchedulerBase::setSlotReady(const string& sProduct, int iSlotID)
{
    _pTradeSignalMerger->setSlotReady(sProduct, iSlotID);
}

void SchedulerBase::updateSlotSignal(const string& sProduct, int iSlotID, long iDesiredPos, int iSignalState, bool bMarketOrder)
{
    _pTradeSignalMerger->updateSlotSignal(sProduct, iSlotID, iDesiredPos, iSignalState, bMarketOrder); 
}

void SchedulerBase::onFill(const string& sProduct, long iFilledQty, double dPrice, bool bIsLiquidator)
{
    _pTradeSignalMerger->onFill(sProduct, iFilledQty, dPrice, bIsLiquidator);
}

}
