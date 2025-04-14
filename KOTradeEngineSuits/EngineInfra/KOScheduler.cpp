#include <boost/program_options.hpp>
#include "KOScheduler.h"
#include "ErrorHandler.h"
#include "SystemClock.h"
#include "PriceUpdateEvent.h"

using std::stringstream;
using std::cerr;

namespace KO
{

KOScheduler::KOScheduler(const string& sSimType, SchedulerConfig &cfg)
:SchedulerBase(sSimType, cfg)
{
    _bExitSchedulerFlag = false;
    _bFXSim = false;
}

KOScheduler::~KOScheduler()
{
}

void KOScheduler::setFXSim(bool bFXSim)
{
    _bFXSim = bFXSim;
}

bool KOScheduler::init()
{
    bool bResult = true;

    preCommonInit();

    _pHistoricDataRegister.reset(new HistoricDataRegister(this, _cSchedulerCfg.sDate, "/dat/matterhorn/data/seconddata/hdf5", _cSchedulerCfg.sTradingLocation, _cSchedulerCfg.bUse100ms));

    for(unsigned int i = 0; i < _cSchedulerCfg.vProducts.size(); ++i)
    {
        InstrumentType eInstrumentType = KO_FUTURE;
        if(_cSchedulerCfg.vProducts[i][0] == 'C')
        {
            eInstrumentType = KO_FX;
        }

        QuoteData* pNewQuoteDataPtr = pregisterProduct(_cSchedulerCfg.vProducts[i], eInstrumentType);
        pNewQuoteDataPtr->iCID = i;
        pNewQuoteDataPtr->iProductExpoLimit = _cSchedulerCfg.vProductExpoLimit[i];
        _pHistoricDataRegister->psubscribeNewProduct(pNewQuoteDataPtr);

        _vFirstOrderTime.push_back(KOEpochTime());
        _vProductLiquidationPos.push_back(0);
        _vProductPos.push_back(0);

        _vProductConsideration.push_back(0.0);
        _vProductVolume.push_back(0);

        _vProductStopLoss.push_back(_cSchedulerCfg.vProductStopLoss[i]);
        _vProductAboveSLInSec.push_back(0);

        _vProductLiquidating.push_back(false);

        stringstream cStringStream;
        cStringStream << "Registered product " << _cSchedulerCfg.vProducts[i] << ".";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _cSchedulerCfg.vProducts[i], cStringStream.str());
    }

    bResult = _pHistoricDataRegister->loadData();

    postCommonInit();

    KOEpochTime cEngineLiveDuration = _cTimerEnd - _cTimerStart;

    for(int i = 0; i < cEngineLiveDuration.sec();i++)
    {
        addNewWakeupCall(_cTimerStart + KOEpochTime(i,0), this);
    }

    sortTimeEvent();

    initExecutorSims();

    return bResult;
}

void KOScheduler::initExecutorSims()
{
    vector<string> vProducts;
    vector<double> vTickSizes;
    vector<long> vSubmitLatencies;
    vector<long> vAmendLatencies;
    vector<string> vDataFiles;
    vector<long> vExpoLimit;
    string sLogPath;
    bool bWriteLog;
    bool bLogMarketData;
    vector<bool> bIOCs;
    vector<int> iIOCSpreadWidthLimit;

    boost::program_options::options_description cAllOptions;
    cAllOptions.add_options()("Contracts", boost::program_options::value<vector<string>>(&vProducts), "Contracts to trade");
    cAllOptions.add_options()("SubmitLatencies", boost::program_options::value<vector<long>>(&vSubmitLatencies), "Submit Latencies");
    cAllOptions.add_options()("AmendLatencies", boost::program_options::value<vector<long>>(&vAmendLatencies), "Amend Latencies");
    cAllOptions.add_options()("TickSizes", boost::program_options::value<vector<double>>(&vTickSizes), "Tick Sizes");

    cAllOptions.add_options()("ExpoLimit", boost::program_options::value<vector<long>>(&vExpoLimit), "ExpoLimit");

    cAllOptions.add_options()("DataFiles", boost::program_options::value<vector<string>>(&vDataFiles), "Data Files");
    cAllOptions.add_options()("LogPath", boost::program_options::value<string>(&sLogPath), "Log Path");
    cAllOptions.add_options()("WriteLog", boost::program_options::value<bool>(&bWriteLog), "Write Log");
    cAllOptions.add_options()("LogMarketData", boost::program_options::value<bool>(&bLogMarketData), "Log Market Data");
    cAllOptions.add_options()("IOC", boost::program_options::value<vector<bool>>(&bIOCs), "IOC Orders");
    cAllOptions.add_options()("IOCSpreadWidthLimit", boost::program_options::value<vector<int>>(&iIOCSpreadWidthLimit), "IOC Spread Width Limit");

    boost::program_options::variables_map vm;
    fstream cConfigFileStream;
    cConfigFileStream.open(_cSchedulerCfg.sExecutorSimCfgFile.c_str(), std::fstream::in);
    boost::program_options::store(boost::program_options::parse_config_file(cConfigFileStream, cAllOptions, true), vm);
    boost::program_options::notify(vm);

    for(unsigned int i = 0; i < vProducts.size(); i ++)
    {
        ExecutorSim* pNewExecutorSim = new ExecutorSim(vProducts[i], vSubmitLatencies[i],  vAmendLatencies[i], vTickSizes[i], vDataFiles[i], 1000000, _cSchedulerCfg.sDate, sLogPath, bWriteLog, false, bLogMarketData, bIOCs[i], iIOCSpreadWidthLimit[i]);

        _iPortfolioID = pNewExecutorSim->iaddPortfolio(NULL, NULL, this);
        _vSimExecutors.push_back(pNewExecutorSim);

        ExecutorSim* pNewLiqExecutorSim = new ExecutorSim(vProducts[i], vSubmitLatencies[i], vAmendLatencies[i], vTickSizes[i], vDataFiles[i], 1000000, _cSchedulerCfg.sDate, sLogPath, bWriteLog, true, bLogMarketData, bIOCs[i], iIOCSpreadWidthLimit[i]);

        _iPortfolioID = pNewLiqExecutorSim->iaddPortfolio(NULL, NULL, this);
        _vLiqExecutors.push_back(pNewLiqExecutorSim);
    }
}

void KOScheduler::run()
{
    while(true)
    {
        KOEpochTime cNextTimeStamp = _pHistoricDataRegister->cgetNextUpdateTime();

        if(cNextTimeStamp != KOEpochTime())
        {
            processTimeEvents(cNextTimeStamp);
        }
        else
        {
            break;
        }

        if(_bExitSchedulerFlag == true)
        {
            break;
        }
    }

    processTimeEvents(SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(_cSchedulerCfg.sDate, "23", "59", "59"));

    _pTradeSignalMerger->writeEoDResult(_cSchedulerCfg.sLogPath, _cSchedulerCfg.sDate);

    for(vector<ExecutorSim*>::iterator itr = _vSimExecutors.begin();
        itr != _vSimExecutors.end();
        itr++)
    {
        delete *itr;
    }

    for(vector<ExecutorSim*>::iterator itr = _vLiqExecutors.begin();
        itr != _vLiqExecutors.end();
        itr++)
    {
        delete *itr;
    }

    std::cout << "Simulation finished \n";
}

void KOScheduler::exitScheduler()
{
    _bExitSchedulerFlag = true;
}

void KOScheduler::applyPriceUpdateOnTime(KOEpochTime cNextTimeStamp)
{
    _pHistoricDataRegister->applyNextUpdate(cNextTimeStamp);

    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); ++i)
    {
        if(_pHistoricDataRegister->bproductHasNewUpdate(i))
        {
            newPriceUpdate(i);
        }
    }
}

KOEpochTime KOScheduler::cgetCurrentTime()
{
    return _cCurrentKOEpochTime;
}

bool KOScheduler::bisLiveTrading()
{
    return false;
}

void KOScheduler::addNewPriceUpdateCall(KOEpochTime cCallTime)
{
    PriceUpdateEvent* pNewPriceUpdateCall = new PriceUpdateEvent(this, cCallTime);
    _vStaticTimeEventQueue.push_back(pNewPriceUpdateCall);
}

void KOScheduler::assignPositionToLiquidator(const string& sProduct, long iPosToLiquidate)
{
    // for fx trading
    string sAdjustedProduct = sProduct;
    if(_bFXSim == true)
    {
        if(sAdjustedProduct.find("6B") != std::string::npos)
        {
            sAdjustedProduct = "GBPUSD";
        }
        else if(sAdjustedProduct.find("6E") != std::string::npos)
        {
            sAdjustedProduct = "EURUSD";
        }
    }

    for(unsigned int i = 0; i < _vSimExecutors.size(); i++)
    {
        if(_vSimExecutors[i]->sgetProduct() == sAdjustedProduct)
        {
            long iLiquidatorPos = _vLiqExecutors[i]->igetPortfolioPosition(_iPortfolioID);

            if(iLiquidatorPos != iPosToLiquidate)
            {
                long iPosDelta = iPosToLiquidate - iLiquidatorPos;

                if(iPosDelta != 0)
                {
                    _vLiqExecutors[i]->transferPosition(_iPortfolioID, iPosDelta);
                    _vSimExecutors[i]->transferPosition(_iPortfolioID, iPosDelta * -1);

                    stringstream cStringStream;
                    cStringStream << "Transfered " << iPosDelta << " lots to fast liquidator";
                    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sAdjustedProduct, cStringStream.str());

                    long iProductIdx = 0;    
                    for(unsigned int index = 0; index < _vContractQuoteDatas.size(); index++)
                    {
                        if(_vContractQuoteDatas[index]->sProduct == sAdjustedProduct)
                        {
                            iProductIdx = index;
                            break;
                        }
                    }

                    _vProductLiquidationPos[iProductIdx] = _vProductLiquidationPos[iProductIdx] + iPosDelta;
                    _vProductPos[iProductIdx] = _vProductPos[iProductIdx] - iPosDelta;
                }
            }

            break;
        }
    }
}

bool KOScheduler::sendToLiquidationExecutor(const string& sProduct, long iDesiredPos)
{
    // for fx trading
    string sAdjustedProduct = sProduct;
    if(_bFXSim == true)
    {
        if(sAdjustedProduct.find("6B") != std::string::npos)
        {
            sAdjustedProduct = "GBPUSD";
        }
        else if(sAdjustedProduct.find("6E") != std::string::npos)
        {
            sAdjustedProduct = "EURUSD";
        }
    }

    int iProductIdx = -1;

    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
    {
        if(_vContractQuoteDatas[i]->sProduct == sAdjustedProduct)
        {
            iProductIdx = i;
            break;
        }
    }

    if(iProductIdx != -1)
    {
        long iAdjustedDesiredPos = iDesiredPos;

        int iPosDelta = iDesiredPos - _vProductLiquidationPos[iProductIdx];
        if(iPosDelta > _vContractQuoteDatas[iProductIdx]->iProductExpoLimit)
        {
            iAdjustedDesiredPos = _vContractQuoteDatas[iProductIdx]->iProductExpoLimit + _vProductLiquidationPos[iProductIdx];
            iPosDelta = _vContractQuoteDatas[iProductIdx]->iProductExpoLimit;
        }
        else if(iPosDelta < -1 * _vContractQuoteDatas[iProductIdx]->iProductExpoLimit)
        {
            iAdjustedDesiredPos = -1 * _vContractQuoteDatas[iProductIdx]->iProductExpoLimit + _vProductLiquidationPos[iProductIdx];
            iPosDelta = -1 * _vContractQuoteDatas[iProductIdx]->iProductExpoLimit;
        }

        for(vector<ExecutorSim*>::iterator itr = _vLiqExecutors.begin();
            itr != _vLiqExecutors.end();
            itr++)
        {
            if((*itr)->sgetProduct() == sAdjustedProduct)
            {
                TradeSignal cNewTradeSignal;
                cNewTradeSignal.iPortfolioID = _iPortfolioID;
                cNewTradeSignal.iEpochTimeStamp = SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();
                cNewTradeSignal.iDesiredPos = iAdjustedDesiredPos;

                if(iAdjustedDesiredPos > 0)
                {
                    // BUY
                    cNewTradeSignal.iSignalState = 0;
                }
                else if(iAdjustedDesiredPos < 0)
                {
                    // SELL
                    cNewTradeSignal.iSignalState = 1;
                }
                else
                {
                    // FLAT
                    cNewTradeSignal.iSignalState = 3;
                }

                cNewTradeSignal.bMarketOrder = true;
                (*itr)->newSignal(cNewTradeSignal, cNewTradeSignal.iEpochTimeStamp + 1000000);
                break;
            }
        }
    }

    return true;
}

double KOScheduler::dgetFillRatio(const string& sProduct)
{
    double dResult = 0.0;

    for(vector<ExecutorSim*>::iterator itr = _vSimExecutors.begin();
        itr != _vSimExecutors.end();
        itr++)
    {
        if((*itr)->sgetProduct() == sProduct)
        {
            dResult = (*itr)->dgetFillRatio();
        }
    }

    return dResult;
}

bool KOScheduler::sendToExecutor(const string& sProduct, long iDesiredPos)
{       
    // for fx trading
    string sAdjustedProduct = sProduct;
    if(_bFXSim == true)
    {
        if(sAdjustedProduct.find("6B") != std::string::npos)
        {
            sAdjustedProduct = "GBPUSD";
        }
        else if(sAdjustedProduct.find("6E") != std::string::npos)
        {
            sAdjustedProduct = "EURUSD";
        }
    }

    int iProductIdx = -1;

    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
    {
        if(_vContractQuoteDatas[i]->sProduct == sAdjustedProduct)
        {
            iProductIdx = i;
            break;
        }
    }

    if(iProductIdx != -1)
    {
        long iProductExpoLimit = _vContractQuoteDatas[iProductIdx]->iProductExpoLimit;

        if(_vFirstOrderTime[iProductIdx].igetPrintable() == 0)
        {
            _vFirstOrderTime[iProductIdx] = cgetCurrentTime();
        }

        if(_bFXSim != true)
        {
            if((cgetCurrentTime() - _vFirstOrderTime[iProductIdx]).igetPrintable() < 300000000)
            {
                iProductExpoLimit = iProductExpoLimit / 5;
            }
        }

        long iAdjustedDesiredPos = iDesiredPos;

        long iPosDelta = iDesiredPos - _vProductPos[iProductIdx];
        if(iPosDelta > iProductExpoLimit)
        {
            iAdjustedDesiredPos = iProductExpoLimit + _vProductPos[iProductIdx];
            iPosDelta = iProductExpoLimit;
        }
        else if(iPosDelta < -1 * iProductExpoLimit)
        {
            iAdjustedDesiredPos = -1 * iProductExpoLimit + _vProductPos[iProductIdx];
            iPosDelta = -1 * iProductExpoLimit;
        }

        stringstream cStringStream;
        cStringStream << "Target Update Qty: " << iDesiredPos << " Adjusted Qty " << iAdjustedDesiredPos << ".";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", sAdjustedProduct, cStringStream.str());

        for(vector<ExecutorSim*>::iterator itr = _vSimExecutors.begin();
            itr != _vSimExecutors.end();
            itr++)
        {
            if((*itr)->sgetProduct() == sAdjustedProduct)
            {
                TradeSignal cNewTradeSignal;
                cNewTradeSignal.iPortfolioID = _iPortfolioID;
                cNewTradeSignal.iEpochTimeStamp = SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();
                cNewTradeSignal.iDesiredPos = iAdjustedDesiredPos;

                if(iAdjustedDesiredPos > 0)
                {
                    // BUY
                    cNewTradeSignal.iSignalState = 0;
                }
                else if(iAdjustedDesiredPos < 0)
                {
                    // SELL
                    cNewTradeSignal.iSignalState = 1;
                }
                else
                {
                    // FLAT
                    cNewTradeSignal.iSignalState = 3;
                }

                cNewTradeSignal.bMarketOrder = false;
                (*itr)->newSignal(cNewTradeSignal, cNewTradeSignal.iEpochTimeStamp + 1000000);
                break;
            }
        }
    }

    return true;
}

void KOScheduler::onFill(const string& sProduct, long iFilledQty, double dPrice, bool bIsLiquidator, InstrumentType eInstrumentType)
{
    int iProductIdx = -1;

    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
    {
        if(_vContractQuoteDatas[i]->sProduct == sProduct)
        {
            iProductIdx = i;
            break;
        }
    }

    if(iProductIdx != -1)
    {
        if(bIsLiquidator == false)
        {
            _vProductPos[iProductIdx] = _vProductPos[iProductIdx] + iFilledQty;
        }
        else
        {
            _vProductLiquidationPos[iProductIdx] = _vProductLiquidationPos[iProductIdx] + iFilledQty;
        }

        _vProductConsideration[iProductIdx] = _vProductConsideration[iProductIdx] - (double)iFilledQty * dPrice;
        _vProductVolume[iProductIdx] = _vProductVolume[iProductIdx] + abs(iFilledQty);

        stringstream cStringStream;
        cStringStream << "iFilledQty: " << iFilledQty << " dPrice: " << dPrice << "\n";
        ErrorHandler::GetInstance()->newInfoMsg("HB", "ALL", "ALL", cStringStream.str());

        // for fx trading
        string sAdjustedFillProduct = sProduct;
        if(sAdjustedFillProduct == "GBPUSD" && _bFXSim == true)
        {
            for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
            {
                if(_vContractQuoteDatas[i]->sProduct.find("6B") != std::string::npos)
                {
                    sAdjustedFillProduct = _vContractQuoteDatas[i]->sProduct;
                    break;
                }
            }
        }
        else if(sAdjustedFillProduct == "EURUSD" && _bFXSim == true)
        {
            for(unsigned int i = 0; i < _vContractQuoteDatas.size(); i++)
            {
                if(_vContractQuoteDatas[i]->sProduct.find("6E") != std::string::npos)
                {
                    sAdjustedFillProduct = _vContractQuoteDatas[i]->sProduct;
                    break;
                }
            }
        }

        _pTradeSignalMerger->onFill(sAdjustedFillProduct, iFilledQty, dPrice, bIsLiquidator, eInstrumentType);
    }
}

void KOScheduler::updateAllPnL()
{
    for(unsigned int i = 0;i < _vContractQuoteDatas.size();i++)
    {
        if(_vProductVolume[i] != 0)
        {
            updateProductPnL(i);
        }
    }
}

};
