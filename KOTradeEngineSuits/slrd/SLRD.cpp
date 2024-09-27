#include <stdlib.h>
#include <boost/math/special_functions/round.hpp>
#include "../base/MultiLegProduct.h"
#include "SLRD.h"
#include "../EngineInfra/ErrorHandler.h"
#include "../EngineInfra/SystemClock.h"

using namespace boost::posix_time;
using std::stringstream;

namespace KO
{

SLRD::SLRD(const string& sEngineRunTimePath,
           const string& sEngineSlotName,
           KOEpochTime cTradingStartTime,
           KOEpochTime cTradingEndTime,
           SchedulerBase* pScheduler,
           string sTodayDate,
           const string& sSimType)
:SDBase(sEngineRunTimePath, "SLRD", sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, sSimType),
 _bUpdateStats(true)
{
    _iDiffLimit = 0;
    _dTimeDecayExit = true;
}

SLRD::~SLRD()
{
    for(unsigned int i = 0; i < _vInstruments.size(); i++)
    {
        delete _vInstruments[i];
    }
}

void SLRD::readFromStream(istream& is)
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
/*
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
*/
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
        else if(sParamName.compare("TimeDecayExit") == 0)
        {
            string sValue;
            std::getline(cParamStream, sValue, ':');

            if(sValue == "0")
            {
                _dTimeDecayExit = false;
            }
            else
            {
                _dTimeDecayExit = true;
            }
        }
    }

    loadTriggerSpace();

    _cPreviousCallTime = _cQuotingStartTime;

	registerFigure();
}

void SLRD::dayInit()
{
    setupLogger("SLRD");

    _cLogger << "Daily Strategy initialisation \n";

    _cLogger << "Engine quoting start time " << _cQuotingStartTime.igetPrintable() << "\n";
    _cLogger << "Engine quoting end time " << _cQuotingEndTime.igetPrintable() << "\n";

    SDBase::dayInit();

    for(unsigned int i = 0; i < vContractQuoteDatas.size(); i++)
    {
        Instrument* pNewInstrument = new Instrument(vContractQuoteDatas[i]->sExchange + "." + vContractQuoteDatas[i]->sProduct, vContractQuoteDatas[i]->iCID, vContractQuoteDatas[i]->eInstrumentType, vContractQuoteDatas[i]->dTickSize, vContractQuoteDatas[i]->iMaxSpreadWidth, true);

//        pNewInstrument->setRollParams(_vRollAdjustment[i],0);
//        pNewInstrument->useEXMA(_dDriftLength, false, KOEpochTime(0,0) , 1, _cTradingStartTime, _cTradingEndTime);
//        pNewInstrument->useEXMA(_dDriftLength / 300, false, KOEpochTime(0,0) , 1, _cTradingStartTime, _cTradingEndTime);
        pNewInstrument->useEXMA(_dDriftLength / 60, false, KOEpochTime(0,0) , 1, _cTradingStartTime, _cTradingEndTime);
        pNewInstrument->useStaticStdev();

        _vInstruments.push_back(pNewInstrument);
        _vInstrumentsStaled.push_back(false);
        _vTotalInstrumentsUpdates.push_back(0);
        _vLastInstrumentsMid.push_back(-1);
    }

    loadDirectionalSettings();
    loadOvernightStats();

    _cLogger << "Daily strategy initialisation finished" << std::endl;
}

void SLRD::dayTrade()
{
    _cLogger << "Daily strategy trade " << std::endl;

    SDBase::dayTrade();

    _cLogger << "Daily strategy trade finished " << std::endl;
}

void SLRD::dayRun()
{
    _cLogger << "Daily strategy run " << std::endl;

    SDBase::dayRun();

    _cLogger << "Daily strategy run finished " << std::endl;
}

void SLRD::dayStop()
{
    _cLogger << "Daily strategy stop " << std::endl;

    SDBase::dayStop();

    loadRollDelta();

    if(_bUpdateStats == true)
    {
        saveOvernightStats();
    }

    for(unsigned int i = 0; i < _vInstruments.size(); i++)
    {
        _vInstruments[i]->eodReset();
    }

    _cLogger << "Daily strategy stop finished " << std::endl;

    _cLogger.closeFile();

    _cSpreadLogger.closeFile();
}

void SLRD::loadDirectionalSettings()
{
    fstream cDirectionSettingFileStream;
    cDirectionSettingFileStream.open(_sEngineRunTimePath + "/dailydirectional.cfg");

    char sNewLine[1024];

    int iNumberIndicators = 0;

    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));

    iNumberIndicators = atoi(sNewLine);

    for(int i = 0; i < iNumberIndicators; i++)
    {
        cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));

        std::istringstream cIndicatorStream(sNewLine);

        string sElement;

        std::getline(cIndicatorStream, sElement, '=');

        string sIndicatorKey;
        // get products 
        std::getline(cIndicatorStream, sElement, '=');
        string sProduct = sElement.substr(0, sElement.find(";"));
        if(sProduct.find("_") == std::string::npos)
        {
            int iInstrumentIdx = 0;
            for(vector<Instrument*>::iterator itr = _vInstruments.begin();
                itr != _vInstruments.end();
                ++itr)
            {
                if((*itr)->sgetProductName() == sProduct)
                {
                    sIndicatorKey = to_string(iInstrumentIdx);
                    break;
                }
                iInstrumentIdx = iInstrumentIdx + 1;
            }
        }
        else
        {
            string sLeg = sProduct.substr(0, sProduct.find("_"));
            int iInstrumentIdx = 0;
            for(vector<Instrument*>::iterator itr = _vInstruments.begin();
                itr != _vInstruments.end();
                ++itr)
            {
                if((*itr)->sgetProductName() == sLeg)
                {
                    sIndicatorKey = to_string(iInstrumentIdx) + "-";
                    break;
                }
                iInstrumentIdx = iInstrumentIdx + 1;
            }

            sLeg = sProduct.substr(sProduct.find("_") + 1);
            string sBackLegWeight = sLeg.substr(0,4);
            sLeg = sLeg.substr(4);

            iInstrumentIdx = 0;
            for(vector<Instrument*>::iterator itr = _vInstruments.begin();
                itr != _vInstruments.end();
                ++itr)
            {
                if((*itr)->sgetProductName() == sLeg)
                {
                    sIndicatorKey = sIndicatorKey + sBackLegWeight + "*" + to_string(iInstrumentIdx);
                    break;
                }
                iInstrumentIdx = iInstrumentIdx + 1;
            }
        }

        // get diff settings
        std::getline(cIndicatorStream, sElement, '=');
        std::istringstream cDiffStream(sElement);

        vector<int> vNewDiffSetting;

        while(!cDiffStream.eof())
        {
            string sDiff;
            std::getline(cDiffStream, sDiff, ',');
            if(sDiff.length() > 0)
            {
                int iDiff = (int)stod(sDiff);
//                iDiff = iDiff;
                if(iDiff > _iDiffLimit)
                {
                    _iDiffLimit = iDiff;
                } 

                vNewDiffSetting.push_back(iDiff);
            }
        }

        _vIndictorsDiff.push_back(deque<double>());
        _vIndictorDiffSettings.push_back(pair<string, vector<int>>(sIndicatorKey, vNewDiffSetting));
    }

    // ------------------------------
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    // GRID
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    // HORIZON
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    
    std::istringstream cHorizonStream(sNewLine);
    string sHorizon;
    std::getline(cHorizonStream, sHorizon, ':');
    std::getline(cHorizonStream, sHorizon, ':');
    _iTimeDecayHorizon = atoi(sHorizon.c_str());

    // Target
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    // Predictor type
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    // -------------------
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));

    // Num Regression Weights
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    // Regression Weights
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));

    std::istringstream cWeightStream(sNewLine);
    
    string sRegressionBias;
    std::getline(cWeightStream, sRegressionBias, ' '); // ignore the first value
    _dRegressionBias = stod(sRegressionBias);

    string sWeight;
    while(!cWeightStream.eof())
    {
        std::getline(cWeightStream, sWeight, ' ');
        if(sWeight.length() > 0)
        {
            _vPCAOutputWeights.push_back(stod(sWeight));
        }
    }

    // Matrix dimentions
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    string sMatrixDimensions = sNewLine;

    int iNumMatrixRows = stod(sMatrixDimensions.substr(0, sMatrixDimensions.find(" ")));
    int iNumMatrixColumns = stod(sMatrixDimensions.substr(sMatrixDimensions.find(" ")+1));

    for(int i = 0; i < iNumMatrixRows; i++)
    {
        _vPCAMatrix.push_back(vector<double>());
        cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
        
        std::istringstream cMatrixRowStream(sNewLine);

        for(int j = 0; j < iNumMatrixColumns; j++)
        {
            string sValue;
            std::getline(cMatrixRowStream, sValue, ' ');
            if(sValue.length() > 0)
            {
                _vPCAMatrix[i].push_back(stod(sValue));
            }
        }
    }

    // XMeans
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    std::istringstream cInputMeanStream(sNewLine);
    while(!cInputMeanStream.eof())
    {
        string sMean;
        std::getline(cInputMeanStream, sMean, ' ');
        if(sMean.length() > 0)
        {
            _vInputMeans.push_back(stod(sMean));
        }
    }

    // XStdds
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    std::istringstream cInputStdStream(sNewLine);
    while(!cInputStdStream.eof())
    {
        string sStd;
        std::getline(cInputStdStream, sStd, ' ');
        if(sStd.length() > 0)
        {
            _vInputStdevs.push_back(stod(sStd));
        }
    }

    // MAE
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    // MSE
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    // CD_All
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));

    // CD_FreqBound
//    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    // CD_FreqBound_StdTrigger
//    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));

    // PredMEAN
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    // PredSTD
    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));

    std::istringstream cPredSTDStream(sNewLine);
    string sPredSTD;
    std::getline(cPredSTDStream, sPredSTD, ':');
    std::getline(cPredSTDStream, sPredSTD, ':');
    _dSignalStdev = stod(sPredSTD);

    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));

    // trigger values
//    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));

/*
    int iNumTriggers = stod(sNewLine);
    for(int i = 0; i < iNumTriggers; i++)
    {
        cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    }
*/

/*
    int iNumTriggers = atoi(sNewLine);

    for(int i = 0; i < iNumTriggers; i++)
    {
        cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));

        std::istringstream cTriggerStream(sNewLine);
        string sTriggerPercent;
        string sTriggerValue;
        std::getline(cTriggerStream, sTriggerPercent, ':');
        std::getline(cTriggerStream, sTriggerValue, ':');

        int iTriggerPercent = atoi(sTriggerPercent.c_str());
        double dTriggerValue = stod(sTriggerValue);

        _mTriggerDict[iTriggerPercent] = dTriggerValue;

//cerr << "adding trigger " << iTriggerPercent << " " << dTriggerValue << "\n";
    }
*/

/*
    for(unsigned int i = 0; i < _vEntryStd.size(); i++)
    {
        int iTriggerKey = _vEntryStd[i];
        while(_mTriggerDict.find(iTriggerKey) == _mTriggerDict.end())
        {
            iTriggerKey = iTriggerKey + 1;
//cerr << "adjusting trigger to " << iTriggerKey << "\n";
            if(iTriggerKey == 101)
            {
                break;
            }
        }

        if(iTriggerKey != 101)
        {
            _vEntryStd[i] = _mTriggerDict[iTriggerKey];
        }
        else
        {
            _vEntryStd[i] = 1000;
        }
//cerr << "entry " << i << " is " << _vEntryStd[i] << "\n";
    }
*/

    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    int iNumStds = stod(sNewLine);
    for(int i = 0; i < iNumStds; i++)
    {
        cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
        std::istringstream cDriftStream(sNewLine);
        string sProduct;
        string sValue;
        std::getline(cDriftStream, sProduct, ':');
        std::getline(cDriftStream, sProduct, ':');
        std::getline(cDriftStream, sValue, ':');

        for(unsigned int j = 0; j < _vInstruments.size(); j++)
        {
            if(_vInstruments[j]->sgetProductName() == sProduct)
            {
                _vInstruments[j]->setStaticStdev(stod(sValue));
            }
        }
    }

    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    int iNumDrifts = stod(sNewLine);
    for(int i = 0; i < iNumDrifts; i++)
    {
        cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
        std::istringstream cDriftStream(sNewLine);
        string sProduct;
        string sValue;
        std::getline(cDriftStream, sProduct, ':');
        std::getline(cDriftStream, sProduct, ':');
        std::getline(cDriftStream, sValue, ':');

        for(unsigned int j = 0; j < _vInstruments.size(); j++)
        {
            if(_vInstruments[j]->sgetProductName() == sProduct)
            {
                _vInstruments[j]->setNewEXMA(stod(sValue), 9999999);
            }
        }
    }

    cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
    int iNumRollAdjusts = stod(sNewLine);
    for(int i = 0; i < iNumRollAdjusts; i++)
    {
        cDirectionSettingFileStream.getline(sNewLine, sizeof(sNewLine));
        std::istringstream cDriftStream(sNewLine);
        string sProduct;
        string sValue;
        std::getline(cDriftStream, sProduct, ':');
        std::getline(cDriftStream, sProduct, ':');
        std::getline(cDriftStream, sValue, ':');

        for(unsigned int j = 0; j < _vInstruments.size(); j++)
        {
            if(_vInstruments[j]->sgetProductName() == sProduct)
            {
                _vInstruments[j]->setRollParams(stod(sValue), 0);
            }
        }
    }
}

void SLRD::receive(int iCID)
{
    SDBase::receive(iCID);

	int iUpdateIndex = -1;

    for(unsigned int i = 0;i < vContractQuoteDatas.size(); i++)
    {
        if(vContractQuoteDatas[i]->iCID == iCID)
        {
            iUpdateIndex = i;
            break;
        }
    }

    int iIndicatorIdx = iUpdateIndex;

    _vInstruments[iIndicatorIdx]->newMarketUpdate(vContractQuoteDatas[iUpdateIndex]);
    _vTotalInstrumentsUpdates[iIndicatorIdx] = _vTotalInstrumentsUpdates[iIndicatorIdx] + 1;        

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
/*
_cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|" << _vInstruments[iIndicatorIdx]->igetBidSize() 
                                                                                 << "|" << _vInstruments[iIndicatorIdx]->igetBestBid() 
                                                                                 << "|" << _vInstruments[iIndicatorIdx]->igetBestAsk() 
                                                                                 << "|" << _vInstruments[iIndicatorIdx]->igetAskSize() 
                                                                                  << "|" << _vInstruments[iIndicatorIdx]->dgetWeightedMid() << "\n";
*/
    _vLastInstrumentsMid[iIndicatorIdx] = _vInstruments[iIndicatorIdx]->dgetWeightedMid();        

    if(!bcheckAllProductsReady())
    {
        _cLogger << "Ignore indicator price update\n";
    }
}

void SLRD::updateStatistics(KOEpochTime cCallTime)
{
    bool printdata = false;
    if(cCallTime.igetPrintable() == 1572854820000000 || cCallTime.igetPrintable() == 1572854760000000)
    {
        printdata = true;
    }
    printdata = false;
    if(printdata)
    {
        cerr << "Time is " << cCallTime.igetPrintable() << "\n";
    }
 
    _bTradingStatsValid = false;
    bool bAllInstrumentReady = true;

    for(unsigned int i = 0; i < _vInstruments.size(); i++)
    {
//        _vInstruments[i]->wakeup(cCallTime);       

//        if(cCallTime.sec() % 300 == 0)
//        {
//            _vInstruments[i]->wakeup(cCallTime);       
//        }

        if(cCallTime.sec() % 60 == 0)
        {
            _vInstruments[i]->wakeup(cCallTime);       
        }
       
        bAllInstrumentReady = bAllInstrumentReady && _vInstruments[i]->bgetEXMAValid() && _vInstruments[i]->bgetWeightedStdevValid();
    }

    if(bAllInstrumentReady)
    {
        vector<double> vPCAInputs;

        int iDiffIdx = 0;
        for(vector<pair<string, vector<int>>>::iterator itr = _vIndictorDiffSettings.begin(); itr != _vIndictorDiffSettings.end(); itr++)
        {
            if(itr->first.find("-") == std::string::npos)
            {
                int idx = atoi(itr->first.c_str());
                double dNewIndicatorValue = (_vInstruments[idx]->dgetWeightedMid() - _vInstruments[idx]->dgetEXMA()) / _vInstruments[idx]->dgetWeightedStdev();

                _vIndictorsDiff[iDiffIdx].push_back(dNewIndicatorValue);

                if(printdata == true)
                {
                    cerr << "indicator " << itr->first << " " << _vInstruments[idx]->sgetProductName() << " " << _vInstruments[idx]->dgetWeightedMid() << " " << _vInstruments[idx]->dgetEXMA() << " " << _vInstruments[idx]->dgetWeightedStdev() << " = " << dNewIndicatorValue << "\n"; 
                }
            }
            else
            {
                int front_idx = atoi(itr->first.substr(0,itr->first.find("-")).c_str());
                int back_idx = atoi(itr->first.substr(itr->first.find("*")+1).c_str());

                double back_weight = stod(itr->first.substr(itr->first.find("-")+1, 4));
                double frontLeg = (_vInstruments[front_idx]->dgetWeightedMid() - _vInstruments[front_idx]->dgetEXMA()) / _vInstruments[front_idx]->dgetWeightedStdev();

                double backLeg = (_vInstruments[back_idx]->dgetWeightedMid() - _vInstruments[back_idx]->dgetEXMA()) / _vInstruments[back_idx]->dgetWeightedStdev();

                _vIndictorsDiff[iDiffIdx].push_back(frontLeg - back_weight * backLeg);

                if(printdata == true)
                {
                    cerr << "spread front indicator " << front_idx << " " << _vInstruments[front_idx]->sgetProductName() << " " << _vInstruments[front_idx]->dgetWeightedMid() << " " << _vInstruments[front_idx]->dgetEXMA() << " " << _vInstruments[front_idx]->dgetWeightedStdev() << " = " << frontLeg << "\n"; 
                    cerr << "spread back indicator " << back_idx << " " << _vInstruments[back_idx]->sgetProductName() << " " << _vInstruments[back_idx]->dgetWeightedMid() << " " << _vInstruments[back_idx]->dgetEXMA() << " " << _vInstruments[back_idx]->dgetWeightedStdev() << " = " << backLeg << "\n"; 
                    cerr << "spread " << frontLeg - back_weight * backLeg << "\n";
                }
            }

            if(printdata == true)
            {
                cerr << "_iDiffLimit " << _iDiffLimit << "\n";
            }

            if((int)_vIndictorsDiff[iDiffIdx].size() > _iDiffLimit)
            {
                _vIndictorsDiff[iDiffIdx].pop_front();
            }

            for(vector<int>::iterator diffSettingItr = itr->second.begin(); diffSettingItr != itr->second.end(); diffSettingItr++)
            {
                if(printdata == true)
                {
                    cerr << "processing diff " << *diffSettingItr << "\n";
                    cerr << "_vIndictorsDiff[iDiffIdx] size is " << _vIndictorsDiff[iDiffIdx].size() << "\n";
                }

                if((int)_vIndictorsDiff[iDiffIdx].size() <  *diffSettingItr + 1)
                {
                    double dDiff = _vIndictorsDiff[iDiffIdx].back() - _vIndictorsDiff[iDiffIdx].front();
                    vPCAInputs.push_back(dDiff);
                    if(printdata == true)
                    {
                        cerr << "adding new (incompleted) diff to " << iDiffIdx << " diff setting " << *diffSettingItr << " " << dDiff << "\n";
                    }
                }
                else
                {   
                    if(printdata == true)
                    {
                        cerr << "size of indictor diff " << iDiffIdx << " is " << _vIndictorsDiff[iDiffIdx].size() << "\n";
                    }

                    double dDiff = _vIndictorsDiff[iDiffIdx].back() - *(_vIndictorsDiff[iDiffIdx].end() - (*diffSettingItr) - 1);
                    vPCAInputs.push_back(dDiff);

                    if(printdata == true)
                    {
                        cerr << "adding new diff to " << iDiffIdx << " diff setting " << *diffSettingItr << " " << dDiff << "\n";
                    }
                }
            }
        
            iDiffIdx++;
        }

        for(unsigned int i = 0; i < vPCAInputs.size(); i++)
        {
            vPCAInputs[i] = (vPCAInputs[i] - _vInputMeans[i]) / _vInputStdevs[i];
            if(printdata == true)
            {
                cerr << "PCA input " << i << " " << vPCAInputs[i] << " = " << vPCAInputs[i] << " - " << _vInputMeans[i] << " / " << _vInputStdevs[i] << "\n"; 
            }
        }

        vector<double> vPCAOutput = vmultiplyPCAMatrix(vPCAInputs);

        _dDirectionSignal = _dRegressionBias;
        for(unsigned int i = 0; i < vPCAOutput.size(); i++)
        {
            _dDirectionSignal += _vPCAOutputWeights[i] * vPCAOutput[i];
        }

        updateSignal();

        _cLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();
        _cLogger << "|ST";

        for(unsigned int i = 0; i < _vInstruments.size(); i++)
        {
            _cLogger << "|" << _vInstruments[i]->igetBestBid();
            _cLogger << "|" << _vInstruments[i]->igetBestAsk();
            _cLogger << "|" << _vInstruments[i]->dgetWeightedMid();
        }

        _cLogger << "|" << _dDirectionSignal << std::endl;
        
        _bTradingStatsValid = true;
    }
    else
    {
        _cLogger << "Ignore wake up call. Instrument statistic not valid. numDataPoint is " << _vInstruments[0]->igetWeightedStdevNumDataPoints() << ". " << _iStdevLength << " required " << std::endl;
    }
}

void SLRD::updateSignal()
{
    for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vEntryStd.size(); iTriggerIdx++)
    {
//        double dBuyEntryThresh = _vEntryStd[iTriggerIdx];
//        double dSellEntryThresh = -1 * _vEntryStd[iTriggerIdx];

        double dBuyEntryThresh = _dSignalStdev * _vEntryStd[iTriggerIdx];
        double dSellEntryThresh = -1 * _dSignalStdev * _vEntryStd[iTriggerIdx];
      
        // exit 1: only exit when signel opposite trigger. no time decay
        // exit -1: only exit when time decay runs out unless signal crosses the other trigger
        
_cLogger << "dBuyEntryThresh " << dBuyEntryThresh << "\n";
_cLogger << "_vEntryStd[" << iTriggerIdx << "] " << _vEntryStd[iTriggerIdx] << "\n";
_cLogger << "_vExitStd[" << iTriggerIdx << "] " << _vExitStd[iTriggerIdx] << "\n";

//        double dBuyExitThresh = dBuyEntryThresh - (_vEntryStd[iTriggerIdx] * (1 + _vExitStd[iTriggerIdx]));
//        double dSellExitThresh = dSellEntryThresh + (_vEntryStd[iTriggerIdx] * (1 + _vExitStd[iTriggerIdx]));

        double dBuyExitThresh = dBuyEntryThresh - _dSignalStdev * (_vEntryStd[iTriggerIdx] * (1 + _vExitStd[iTriggerIdx]));
        double dSellExitThresh = dSellEntryThresh + _dSignalStdev * (_vEntryStd[iTriggerIdx] * (1 + _vExitStd[iTriggerIdx]));

//_cLogger << iTriggerIdx << " dBuyEntryThresh " << dBuyEntryThresh;
//_cLogger << iTriggerIdx << " dSellEntryThresh " << dBuyEntryThresh;
//_cLogger << "Signal " << _dDirectionSignal << "\n";

_cLogger << "dBuyExitThresh " << dBuyExitThresh << "\n";
_cLogger << "dSellExitThresh " << dSellExitThresh << "\n";

_cLogger << "_vExitStd[" << iTriggerIdx << "]" << _vExitStd[iTriggerIdx] << "\n";

_cLogger << "_iTimeDecayHorizon " << _iTimeDecayHorizon << "\n";

        if(_dDirectionSignal >= dBuyEntryThresh)
        {
            _vSignalStates[iTriggerIdx] = BUY;
            _vIsMarketOrders[iTriggerIdx] = false;
            _vTheoPositions[iTriggerIdx] = _iQuoteQty;
            _vSignalTimeElapsed[iTriggerIdx] = 0;
        }
        else if(_dDirectionSignal <= dSellEntryThresh)
        {
            _vSignalStates[iTriggerIdx] = SELL;
            _vIsMarketOrders[iTriggerIdx] = false;
            _vTheoPositions[iTriggerIdx] = -1 * _iQuoteQty;
            _vSignalTimeElapsed[iTriggerIdx] = 0;
        }

/*
        if(_dDirectionSignal >= dBuyEntryThresh)
        {
            _vSignalStates[iTriggerIdx] = BUY;
            _vIsMarketOrders[iTriggerIdx] = false;
            _vTheoPositions[iTriggerIdx] = _iQuoteQty;
            _vSignalTimeElapsed[iTriggerIdx] = 0;
        }
        else if(_dDirectionSignal <= dSellEntryThresh)
        {
            _vSignalStates[iTriggerIdx] = SELL;
            _vIsMarketOrders[iTriggerIdx] = false;
            _vTheoPositions[iTriggerIdx] = -1 * _iQuoteQty;
            _vSignalTimeElapsed[iTriggerIdx] = 0;
        }
        else
        {
            if(_vSignalTimeElapsed[iTriggerIdx] > _iTimeDecayHorizon * 3)
            {
_cLogger << "Time limit reached \n";
                if(dSellExitThresh < _dDirectionSignal && _dDirectionSignal < dBuyExitThresh)
                {
                    _vSignalStates[iTriggerIdx] = FLAT;
                    _vIsMarketOrders[iTriggerIdx] = false;
                }
                else if(_dDirectionSignal < dBuyExitThresh)
                {
                    _vSignalStates[iTriggerIdx] = FLAT_ALL_LONG;
                    _vIsMarketOrders[iTriggerIdx] = false;
                }
                else if(_dDirectionSignal > dSellExitThresh)
                {
                    _vSignalStates[iTriggerIdx] = FLAT_ALL_SHORT;  
                    _vIsMarketOrders[iTriggerIdx] = false;
                }
            }
            else
            {
                if(_vSignalPrevStates[iTriggerIdx] == BUY)
                {
                    _vSignalStates[iTriggerIdx] = FLAT_ALL_SHORT;
                    _vIsMarketOrders[iTriggerIdx] = false;
                }
                else if(_vSignalPrevStates[iTriggerIdx] == SELL)
                {
                    _vSignalStates[iTriggerIdx] = FLAT_ALL_LONG;
                    _vIsMarketOrders[iTriggerIdx] = false;
                }
                else
                {
                    _vSignalStates[iTriggerIdx] = _vSignalPrevStates[iTriggerIdx];
                    _vIsMarketOrders[iTriggerIdx] = false;
                }
            }
        }
*/

        _vSignalPrevStates[iTriggerIdx] = _vSignalStates[iTriggerIdx];

//_cLogger << "_vSignalTimeElapsed[" << iTriggerIdx << "]" << _vSignalTimeElapsed[iTriggerIdx] << "\n";
//_cLogger << "_iTimeDecayHorizon " << _iTimeDecayHorizon << "\n";

    } 
}

vector<double> SLRD::vmultiplyPCAMatrix(vector<double>& vInputVector)
{
    int iInputColumns = vInputVector.size();
    int iMatrixColumns = _vPCAMatrix[0].size();
      
    vector<double> vOutput(iMatrixColumns, 0);

    for(int iMatrixColumnIdx = 0; iMatrixColumnIdx < iMatrixColumns; iMatrixColumnIdx++)
    { 
        for(int iInputColumnIdx = 0; iInputColumnIdx < iInputColumns; iInputColumnIdx++)
        {
            vOutput[iMatrixColumnIdx] += vInputVector[iInputColumnIdx] * _vPCAMatrix[iInputColumnIdx][iMatrixColumnIdx];
        }
    }

    return vOutput;
}

void SLRD::writeSpreadLog()
{

}

bool SLRD::bcheckAllProductsReady()
{
    bool bResult = true;

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

void SLRD::wakeup(KOEpochTime cCallTime)
{
//    if(cCallTime.sec() % 10 == 0)
//    {
//cerr << "in wakeup call " << cCallTime.igetPrintable() << "\n";
        SDBase::wakeup(cCallTime);
//    }    

    for(unsigned int iTriggerIdx = 0; iTriggerIdx != _vSignalTimeElapsed.size(); iTriggerIdx++)
    {
        _vSignalTimeElapsed[iTriggerIdx] = _vSignalTimeElapsed[iTriggerIdx] + 1;
    }
}

void SLRD::loadRollDelta()
{
    SDBase::loadRollDelta();

    for(unsigned int i = 1; i < vContractQuoteDatas.size(); i++)
    {
        int iIndicatorIndex = i - 1;
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

void SLRD::saveOvernightStats()
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
                cStringStream << _sTodayDate << ",";

                for(unsigned int i = 0;i < _vInstruments.size(); i++)
                {
                    cStringStream << _vInstruments[i]->dgetEXMA() << ","
                                  << _vInstruments[i]->igetEXMANumDataPoints() << ","
                                  << _vInstruments[i]->dgetStaticStdev()
                                  << "0,";
                }

                // just write 0 for product std which is not used
                cStringStream << "0,"
                              << "0,";

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

                bCurrentDayWritten = true;
            }

            cStringStream << to_iso_string(cKeyDate) << ",";

            for(unsigned int i = 0; i < _mDailyLegStats[cKeyDate].size(); i ++)
            {
                cStringStream << _mDailyLegStats[cKeyDate][i].dLegEXMA << ","
                              << _mDailyLegStats[cKeyDate][i].iLegEXMANumDataPoints << ","
                              << _mDailyLegStats[cKeyDate][i].dLegStd << ","
                              << "0,";
            }

            cStringStream << "0,";
            cStringStream << "0,";
            
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
        cStringStream << _sTodayDate << ",";

        for(unsigned int i = 0;i < _vInstruments.size(); i++)
        {
            cStringStream << _vInstruments[i]->dgetEXMA() << ","
                          << _vInstruments[i]->igetEXMANumDataPoints() << ","
                          << _vInstruments[i]->dgetStaticStdev() << ","
                          << "0,";
        }

        // jsut write 0 for product std which is not used
        cStringStream << "0,"
                      << "0,";

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

    vector<DailyLegStat> cDailyLegStat;

    for(unsigned int i = 0;i < _vInstruments.size(); i++)
    {
        DailyLegStat cLeg;
        cLeg.dLegEXMA = _vInstruments[i]->dgetEXMA();
        cLeg.iLegEXMANumDataPoints = _vInstruments[i]->igetEXMANumDataPoints();
        cLeg.dLegStd = _vInstruments[i]->dgetStaticStdev();
        cDailyLegStat.push_back(cLeg);
    }

    _mDailyLegStats[cTodayDate] = cDailyLegStat;
    _mDailyLegMid[cTodayDate] = _vLastInstrumentsMid;
}

void SLRD::loadOvernightStats()
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

                    vector<DailyLegStat> vDailyLegStats;
                    for(unsigned int i = 0; i < _vInstruments.size(); i++)
                    {
                        DailyLegStat cLegStat;

                        string sLegEXMA;
                        string sLegEXMANumDataPoints;
                        string sLegStdev;
                        string sRollDelta;

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

                    vector<double> vDailyLegMid;
                    for(unsigned int i = 0; i < _vInstruments.size(); i++)
                    {
                        string sLegMid;
                        bStatValid = bStatValid && std::getline(cDailyStatStream, sLegMid, ',');
                        vDailyLegMid.push_back(stod(sLegMid));
                    }

                    if(bStatValid)
                    {
                        _mDailyLegStats[cDate] = vDailyLegStats;
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

    map< boost::gregorian::date, vector<DailyLegStat> >::iterator LegStatItr = _mDailyLegStats.end();

    for(int daysToLookback = 0; daysToLookback < 10; daysToLookback++)
    {
        LegStatItr = _mDailyLegStats.find(cStatDay);

        _cLogger << "Trying to load overnight stat from date " << to_iso_string(cStatDay) << "\n";
        if(LegStatItr != _mDailyLegStats.end())
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

    if(LegStatItr != _mDailyLegStats.end())
    {
        _cLogger << "Loading overnight stat from date " << to_iso_string(cStatDay) << "\n";

        vector<DailyLegStat> vIndicatorStats = _mDailyLegStats[cStatDay];

        for(unsigned int i = 0; i != vIndicatorStats.size(); i++)
        {
//cerr << "Assign Indicator " << i << " EXMA: " << vIndicatorStats[i].dLegEXMA << "\n";
//cerr << "with " << vIndicatorStats[i].iLegEXMANumDataPoints << " data points " << "\n";

//            _cLogger << "Assign Indicator " << i << " EXMA: " << vIndicatorStats[i].dLegEXMA << "\n";
//            _cLogger << "with " << vIndicatorStats[i].iLegEXMANumDataPoints << " data points " << "\n";
//            _vInstruments[i]->setNewEXMA(vIndicatorStats[i].dLegEXMA, vIndicatorStats[i].iLegEXMANumDataPoints);
//            _vInstruments[i]->setNewEXMA(0, 0);
//            _cLogger << "Assign Indicator " << i << " Stdev: " << vIndicatorStats[i].dLegStd << "\n";

//            _vInstruments[i]->setStaticStdev(vIndicatorStats[i].dLegStd);
        }

        vector<double> vIndicatorMid = _mDailyLegMid[cStatDay];
        for(unsigned int i = 0; i < vIndicatorMid.size(); i++)
        {
            _vLastInstrumentsMid.push_back(vIndicatorMid[i]);
        }
    }
}

}
