#include "GridDataGenerator.h"
#include "../EngineInfra/SchedulerBase.h"
#include <boost/math/special_functions/round.hpp>
#include "../EngineInfra/SystemClock.h"
#include <H5Cpp.h>

using namespace boost::posix_time;
using namespace H5;
using std::stringstream;

namespace KO
{
GridDataGenerator::GridDataGenerator(const string& sEngineRunTimePath,
                const string& sEngineSlotName,
                KOEpochTime cTradingStartTime,
                KOEpochTime cTradingEndTime,
                SchedulerBase* pScheduler,
                string sTodayDate,
                PositionServerConnection* pPositionServerConnection)
:TradeEngineBase(sEngineRunTimePath, "GridDataGenerator", sEngineSlotName, cTradingStartTime, cTradingEndTime, pScheduler, sTodayDate, pPositionServerConnection)
{
    _iStartTimeInSecond = cTradingStartTime.sec();
    _iNumberOfUpdateReceived = 0;

    _dLastLowBid = 0;
    _iLastLowBidInTicks = 0;
    _iLastLowBidSize = 0;

    _dLastHighBid = 0;
    _iLastHighBidInTicks = 0;
    _iLastHighBidSize = 0;

    _dLastHighAsk = 0;
    _iLastHighAskInTicks = 0;
    _iLastHighAskSize = 0;

    _dLastLowAsk = 0;
    _iLastLowAskInTicks = 0;
    _iLastLowAskSize = 0;

    _dVWAPConsideration = 0;
    _iVWAPVolume = 0;

    _dSumAllSpread = 0;
    _iNumberSpreadSample = 0;

    _cOutputStringStream.str("");
    _cOutputStringStream.precision(10);
}

GridDataGenerator::~GridDataGenerator()
{

}

void GridDataGenerator::dayInit()
{
    TradeEngineBase::dayInit();

    KOEpochTime cEngineLiveDuration = _cTradingEndTime - _cTradingStartTime;

    for(int i = 0; i < cEngineLiveDuration.sec();i++)
    {
        _pScheduler->addNewWakeupCall(_cTradingStartTime + KOEpochTime(i,0), this);
    }

 }

void GridDataGenerator::dayRun()
{
    TradeEngineBase::dayRun();
}

void GridDataGenerator::dayStop()
{
    TradeEngineBase::dayStop();

    if(_iNumberOfUpdateReceived != 0)
    {
        fstream ofsStatsFile;

        ofsStatsFile.open(_sEngineRunTimePath + "DataStats" + _sTodayDate, fstream::out);
        if(ofsStatsFile.is_open())
        {
            ofsStatsFile << _iNumberInvalidUpdates << " invalid updates received \n";
            ofsStatsFile << "Average spread is " << (_dSumAllSpread / _iNumberSpreadSample) << "\n";
        }

        ofsStatsFile.close();

        if(_vGridData.size() != 0)
        {
            string strDate = _sTodayDate;
            string strYear = strDate.substr(0, 4);
            string strMonth = strDate.substr(4, 2);
            string strDay = strDate.substr(6, 2);

            string sProductDisplayName = vContractQuoteDatas[0]->sProduct;

            if(sProductDisplayName.find("/") != string::npos)
            {   
                sProductDisplayName.replace(sProductDisplayName.find("/"), string("/").length(), "");
            }

            stringstream cStringStream;
            cStringStream << std::getenv("GRIDDATAPATH") << "/" 
                          << _sGridFileOutputPath << "/"
                          << sProductDisplayName << "_"
                          << strYear << "."
                          << strMonth << "."
                          << strDay << "_"
                          << _iGridFreq << "s"
                          << ".h5";
            string sH5GridFileName = cStringStream.str();

            long iNumGridPoint = _vGridData.size();

            GridData* pDataArray = new GridData[iNumGridPoint];

            std::copy(_vGridData.begin(), _vGridData.end(), pDataArray);

            hsize_t cDim[] = {iNumGridPoint};
            DataSpace cSpace(sizeof(cDim)/sizeof(hsize_t), cDim);
            H5File* pH5File = new H5File(sH5GridFileName, H5F_ACC_TRUNC);

            CompType cGridDataType(sizeof(GridData));
            cGridDataType.insertMember("EpochTimeStamp", HOFFSET(GridData, iEpochTimeStamp), PredType::NATIVE_LONG);
            cGridDataType.insertMember("Bid", HOFFSET(GridData, dBid), PredType::NATIVE_DOUBLE);
            cGridDataType.insertMember("BidSize", HOFFSET(GridData, iBidSize), PredType::NATIVE_LONG);
            cGridDataType.insertMember("Ask", HOFFSET(GridData, dAsk), PredType::NATIVE_DOUBLE);
            cGridDataType.insertMember("AskSize", HOFFSET(GridData, iAskSize), PredType::NATIVE_LONG);
            cGridDataType.insertMember("Last", HOFFSET(GridData, dLast), PredType::NATIVE_DOUBLE);
            cGridDataType.insertMember("AccumuTradeSize", HOFFSET(GridData, iAccumuTradeSize), PredType::NATIVE_LONG);
            cGridDataType.insertMember("LowBid", HOFFSET(GridData, dLowBid), PredType::NATIVE_DOUBLE);
            cGridDataType.insertMember("LowBidSize", HOFFSET(GridData, iLowBidSize), PredType::NATIVE_LONG);

            cGridDataType.insertMember("HighBid", HOFFSET(GridData, dHighBid), PredType::NATIVE_DOUBLE);
            cGridDataType.insertMember("HighBidSize", HOFFSET(GridData, iHighBidSize), PredType::NATIVE_LONG);
            cGridDataType.insertMember("HighAsk", HOFFSET(GridData, dHighAsk), PredType::NATIVE_DOUBLE);
            cGridDataType.insertMember("HighAskSize", HOFFSET(GridData, iHighAskSize), PredType::NATIVE_LONG);

            cGridDataType.insertMember("LowAsk", HOFFSET(GridData, dLowAsk), PredType::NATIVE_DOUBLE);
            cGridDataType.insertMember("LowAskSize", HOFFSET(GridData, iLowAskSize), PredType::NATIVE_LONG);
            cGridDataType.insertMember("VWAP", HOFFSET(GridData, dVWAP), PredType::NATIVE_DOUBLE);

            DataSet* pDataSet = new DataSet(pH5File->createDataSet("GridData", cGridDataType, cSpace));
            pDataSet->write(pDataArray, cGridDataType);

            delete pDataArray;
            delete pDataSet;
            delete pH5File;
        }

        if(_cOutputStringStream.str().compare("") != 0)
        {
            string strDate = _sTodayDate;
            string strYear = strDate.substr(0, 4);
            string strMonth = strDate.substr(4, 2);
            string strDay = strDate.substr(6, 2);

            string sProductDisplayName = vContractQuoteDatas[0]->sProduct;
            if(sProductDisplayName.find("/") != string::npos)
            {   
                sProductDisplayName.replace(sProductDisplayName.find("/"), string("/").length(), "");
            }

            stringstream cStringStream;
            cStringStream.str("");
            cStringStream << std::getenv("GRIDDATAPATH") << "/"
                          << _sGridFileOutputPath << "/"
                          << sProductDisplayName << "_"
                          << strYear << "."
                          << strMonth << "."
                          << strDay << "_"
                          << _iGridFreq << "s";

            fstream ofsGridFile;
            string sCSVGridFileName = cStringStream.str();

            ofsGridFile.open(sCSVGridFileName.c_str(), fstream::out);
            if(ofsGridFile.is_open())
            {
                ofsGridFile << "TIME,BIDSIZE,BID,ASK,ASKSIZE,LASTTRADE,TRADESIZE,LOWBIDSIZE,LOWBID,HIGHBIDSIZE,HIGHBID,HIGHASK,HIGHASKSIZE,LOWASK,LOWASKSIZE,VWAP\n";
                ofsGridFile << _cOutputStringStream.str();
            }		

            ofsGridFile.close();
        }
    }

}

void GridDataGenerator::readFromStream(istream& is)
{
    is >> _iGridFreq;
    is >> _sGridFileOutputPath;
}

void GridDataGenerator::receive(int iCID)
{
/*
    std::cerr << "Raw data received: " << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << ","
         << vContractQuoteDatas[0]->iBidSize << ","
         << vContractQuoteDatas[0]->dBestBid << ","
         << vContractQuoteDatas[0]->dBestAsk << ","
         << vContractQuoteDatas[0]->iAskSize << ","
         << vContractQuoteDatas[0]->dLastTradePrice << ","
         << vContractQuoteDatas[0]->iAccumuTradeSize << "\n";
*/

    if(_iGridFreq == 0)
    {
        bool bWriteUpdate = false;

        if(_iNumberOfUpdateReceived == 0)
        {
            bWriteUpdate = true;
        }
        else
        {
            if(iprevBidSize != vContractQuoteDatas[0]->iBidSize ||
               iprevBidInTicks != vContractQuoteDatas[0]->iBestBidInTicks ||
               iprevAskInTicks != vContractQuoteDatas[0]->iBestAskInTicks ||
               iprevAskSize != vContractQuoteDatas[0]->iAskSize ||
               iprevLastInTicks != vContractQuoteDatas[0]->iLastTradeInTicks ||
               iprevVolume != vContractQuoteDatas[0]->iAccumuTradeSize)
            {
                bWriteUpdate = true;
            }
        }
        
        if(bWriteUpdate == true)
        {
            _cOutputStringStream << SystemClock::GetInstance()->sToSimpleString(SystemClock::GetInstance()->cgetCurrentKOEpochTime()) << ","
                                 << vContractQuoteDatas[0]->iBidSize << ","
                                 << vContractQuoteDatas[0]->dBestBid << ","
                                 << vContractQuoteDatas[0]->dBestAsk << ","
                                 << vContractQuoteDatas[0]->iAskSize << ","
                                 << vContractQuoteDatas[0]->dLastTradePrice << ","
                                 << vContractQuoteDatas[0]->iAccumuTradeSize << ","
                                 << _iLastLowBidSize << ","
                                 << _dLastLowBid << ","
                                 << _iLastHighBidSize << ","
                                 << _dLastHighBid << ","
                                 << _dLastHighAsk << ","
                                 << _iLastHighAskSize << ","
                                 << _dLastLowAsk << ","
                                 << _iLastLowAskSize << ","
                                 << 0 << "\n";

            GridData cNewDataPoint;
            cNewDataPoint.iEpochTimeStamp = SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();
            cNewDataPoint.dBid = vContractQuoteDatas[0]->dBestBid;
            cNewDataPoint.iBidSize = vContractQuoteDatas[0]->iBidSize;
            cNewDataPoint.dAsk = vContractQuoteDatas[0]->dBestAsk;
            cNewDataPoint.iAskSize = vContractQuoteDatas[0]->iAskSize;
            cNewDataPoint.dLast = vContractQuoteDatas[0]->dLastTradePrice;
            cNewDataPoint.iAccumuTradeSize = vContractQuoteDatas[0]->iAccumuTradeSize;
            cNewDataPoint.dLowBid = _dLastLowBid;
            cNewDataPoint.iLowBidSize = _iLastLowBidSize;
            cNewDataPoint.dHighBid = _dLastHighBid;
            cNewDataPoint.iHighBidSize = _iLastHighBidSize;
            cNewDataPoint.dHighAsk = _dLastHighAsk;
            cNewDataPoint.iHighAskSize = _iLastHighAskSize;
            cNewDataPoint.dLowAsk = _dLastLowAsk;
            cNewDataPoint.iLowAskSize = _iLastLowAskSize;
            cNewDataPoint.dVWAP = 0;

            _vGridData.push_back(cNewDataPoint);

            iprevBidSize = vContractQuoteDatas[0]->iBidSize; 
            iprevBidInTicks = vContractQuoteDatas[0]->iBestBidInTicks;
            iprevAskInTicks = vContractQuoteDatas[0]->iBestAskInTicks;
            iprevAskSize = vContractQuoteDatas[0]->iAskSize;
            iprevLastInTicks = vContractQuoteDatas[0]->iLastTradeInTicks;
            iprevVolume = vContractQuoteDatas[0]->iAccumuTradeSize;
        }
    }
    else
    {
        if(vContractQuoteDatas[0]->iBidSize != 0 && vContractQuoteDatas[0]->iAskSize != 0)
        {
            if(vContractQuoteDatas[0]->iBestBidInTicks < vContractQuoteDatas[0]->iBestAskInTicks)
            {
/*
    std::cerr << "Valid data received: " << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << ","
         << vContractQuoteDatas[0]->iBidSize << ","
         << vContractQuoteDatas[0]->dBestBid << ","
         << vContractQuoteDatas[0]->dBestAsk << ","
         << vContractQuoteDatas[0]->iAskSize << ","
         << vContractQuoteDatas[0]->dLastTradePrice << ","
         << vContractQuoteDatas[0]->iAccumuTradeSize << "\n";
*/

                if(_iLastLowBidSize == 0 && _iLastHighAskSize == 0 &&
                   _iLastHighBidSize == 0 && _iLastLowAskSize == 0)
                {
                    _dLastLowBid = vContractQuoteDatas[0]->dBestBid;
                    _iLastLowBidInTicks = vContractQuoteDatas[0]->iBestBidInTicks;
                    _iLastLowBidSize = vContractQuoteDatas[0]->iBidSize;

                    _dLastHighBid = vContractQuoteDatas[0]->dBestBid;
                    _iLastHighBidInTicks = vContractQuoteDatas[0]->iBestBidInTicks;
                    _iLastHighBidSize = vContractQuoteDatas[0]->iBidSize;

                    _dLastHighAsk = vContractQuoteDatas[0]->dBestAsk;
                    _iLastHighAskInTicks = vContractQuoteDatas[0]->iBestAskInTicks;
                    _iLastHighAskSize = vContractQuoteDatas[0]->iAskSize;

                    _dLastLowAsk = vContractQuoteDatas[0]->dBestAsk;
                    _iLastLowAskInTicks = vContractQuoteDatas[0]->iBestAskInTicks;
                    _iLastLowAskSize = vContractQuoteDatas[0]->iAskSize;
                }
                else
                {
                    if(vContractQuoteDatas[0]->iBestBidInTicks < _iLastLowBidInTicks)
                    {
                        _iLastLowBidInTicks = vContractQuoteDatas[0]->iBestBidInTicks;
                        _dLastLowBid = vContractQuoteDatas[0]->dBestBid;
                        _iLastLowBidSize = vContractQuoteDatas[0]->iBidSize;
                    }
                    else if(vContractQuoteDatas[0]->iBestBidInTicks == _iLastLowBidInTicks)
                    {
                        if(vContractQuoteDatas[0]->iBidSize < _iLastLowBidSize)
                        {
                            _iLastLowBidSize = vContractQuoteDatas[0]->iBidSize;
                        }
                    }

                    if(vContractQuoteDatas[0]->iBestBidInTicks > _iLastHighBidInTicks)
                    {
                        _iLastHighBidInTicks = vContractQuoteDatas[0]->iBestBidInTicks;
                        _dLastHighBid = vContractQuoteDatas[0]->dBestBid;
                        _iLastHighBidSize = vContractQuoteDatas[0]->iBidSize;
                    }
                    else if(vContractQuoteDatas[0]->iBestBidInTicks == _iLastHighBidInTicks)
                    {
                        if(vContractQuoteDatas[0]->iBidSize > _iLastHighBidSize)
                        {
                            _iLastHighBidSize = vContractQuoteDatas[0]->iBidSize;
                        }
                    }

                    if(vContractQuoteDatas[0]->iBestAskInTicks > _iLastHighAskInTicks)
                    {
                        _iLastHighAskInTicks = vContractQuoteDatas[0]->iBestAskInTicks;
                        _dLastHighAsk = vContractQuoteDatas[0]->dBestAsk;
                        _iLastHighAskSize = vContractQuoteDatas[0]->iAskSize;
                    }
                    else if(vContractQuoteDatas[0]->iBestAskInTicks == _iLastHighAskInTicks)
                    {
                        if(vContractQuoteDatas[0]->iAskSize < _iLastHighAskSize)
                        {
                            _iLastHighAskSize = vContractQuoteDatas[0]->iAskSize;
                        }
                    }

                    if(vContractQuoteDatas[0]->iBestAskInTicks < _iLastLowAskInTicks)
                    {
                        _iLastLowAskInTicks = vContractQuoteDatas[0]->iBestAskInTicks;
                        _dLastLowAsk = vContractQuoteDatas[0]->dBestAsk;
                        _iLastLowAskSize = vContractQuoteDatas[0]->iAskSize;
                    }
                    else if(vContractQuoteDatas[0]->iBestAskInTicks == _iLastLowAskInTicks)
                    {
                        if(vContractQuoteDatas[0]->iAskSize > _iLastLowAskSize)
                        {
                            _iLastLowAskSize = vContractQuoteDatas[0]->iAskSize;
                        }
                    }
                }

                if(vContractQuoteDatas[0]->iTradeSize != 0)
                {
                    _dVWAPConsideration = _dVWAPConsideration + vContractQuoteDatas[0]->iTradeSize * vContractQuoteDatas[0]->dLastTradePrice;
                    _iVWAPVolume = _iVWAPVolume + vContractQuoteDatas[0]->iTradeSize;
               }
            }
            else
            {
/*
            std::cerr << "Invalid Update," << SystemClock::GetInstance()->sToSimpleString(SystemClock::GetInstance()->cgetCurrentKOEpochTime()) << ","
                                 << vContractQuoteDatas[0]->iBidSize << ","
                                 << vContractQuoteDatas[0]->dBestBid << ","
                                 << vContractQuoteDatas[0]->dBestAsk << ","
                                 << vContractQuoteDatas[0]->iAskSize << ","
                                 << vContractQuoteDatas[0]->dLastTradePrice << ","
                                 << vContractQuoteDatas[0]->iAccumuTradeSize << "\n";
*/

                _iNumberInvalidUpdates = _iNumberInvalidUpdates + 1;
            }
        }
        else
        {
/*
            std::cerr << "Invalid Update," << SystemClock::GetInstance()->sToSimpleString(SystemClock::GetInstance()->cgetCurrentKOEpochTime()) << ","
                                 << vContractQuoteDatas[0]->iBidSize << ","
                                 << vContractQuoteDatas[0]->dBestBid << ","
                                 << vContractQuoteDatas[0]->dBestAsk << ","
                                 << vContractQuoteDatas[0]->iAskSize << ","
                                 << vContractQuoteDatas[0]->dLastTradePrice << ","
                                 << vContractQuoteDatas[0]->iAccumuTradeSize << "\n";
*/

            _iNumberInvalidUpdates = _iNumberInvalidUpdates + 1;
        }
    }
                
    _iNumberOfUpdateReceived = _iNumberOfUpdateReceived + 1;
}

void GridDataGenerator::wakeup(KOEpochTime cCallTime)
{
    if(vContractQuoteDatas[0]->iBidSize != 0 && vContractQuoteDatas[0]->iAskSize != 0)
    {
        //check no cross price
        if(vContractQuoteDatas[0]->iBestBidInTicks < vContractQuoteDatas[0]->iBestAskInTicks)
        {
            _iNumberSpreadSample = _iNumberSpreadSample + 1;
            _dSumAllSpread = _dSumAllSpread + (vContractQuoteDatas[0]->dBestAsk - vContractQuoteDatas[0]->dBestBid);
        }
    }

    if(_iGridFreq != 0)
    {
        if((cCallTime.sec() - _iStartTimeInSecond) % _iGridFreq == 0)
        {
            // check zero size
            if(vContractQuoteDatas[0]->iBidSize != 0 && vContractQuoteDatas[0]->iAskSize != 0)
            {
                // check no cross price
                if(vContractQuoteDatas[0]->iBestBidInTicks < vContractQuoteDatas[0]->iBestAskInTicks)
                {
                    double dVWAP = vContractQuoteDatas[0]->dLastTradePrice;
                    if(_iVWAPVolume != 0)
                    {
                        dVWAP = _dVWAPConsideration / _iVWAPVolume;
                    }

                    if(_iLastLowBidSize == 0 && _iLastHighAskSize == 0)
                    {
                        _dLastLowBid = vContractQuoteDatas[0]->dBestBid;
                        _iLastLowBidInTicks = vContractQuoteDatas[0]->iBestBidInTicks;
                        _iLastLowBidSize = vContractQuoteDatas[0]->iBidSize;

                        _dLastHighBid = vContractQuoteDatas[0]->dBestBid;
                        _iLastHighBidInTicks = vContractQuoteDatas[0]->iBestBidInTicks;
                        _iLastHighBidSize = vContractQuoteDatas[0]->iBidSize;

                        _dLastHighAsk = vContractQuoteDatas[0]->dBestAsk;
                        _iLastHighAskInTicks = vContractQuoteDatas[0]->iBestAskInTicks;
                        _iLastHighAskSize = vContractQuoteDatas[0]->iAskSize;

                        _dLastLowAsk = vContractQuoteDatas[0]->dBestAsk;
                        _iLastLowAskInTicks = vContractQuoteDatas[0]->iBestAskInTicks;
                        _iLastLowAskSize = vContractQuoteDatas[0]->iAskSize;
                    }

/*
                    cerr << "Wakeup|" << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << ","
                                  << vContractQuoteDatas[0]->iBidSize << ","
                                  << vContractQuoteDatas[0]->dBestBid << ","
                                  << vContractQuoteDatas[0]->dBestAsk << ","
                                  << vContractQuoteDatas[0]->iAskSize << ","
                                  << vContractQuoteDatas[0]->dLastTradePrice << ","
                                  << vContractQuoteDatas[0]->iAccumuTradeSize << ","
                                  << _iLastLowBidSize << ","
                                  << _dLastLowBid << ","
                                  << _dLastHighAsk << ","
                                  << _iLastHighAskSize << ","
                                  << dVWAP << "\n";
*/

                    _cOutputStringStream << SystemClock::GetInstance()->sToSimpleString(cCallTime) << ","
                                         << vContractQuoteDatas[0]->iBidSize << ","
                                         << vContractQuoteDatas[0]->dBestBid << ","
                                         << vContractQuoteDatas[0]->dBestAsk << ","
                                         << vContractQuoteDatas[0]->iAskSize << ","
                                         << vContractQuoteDatas[0]->dLastTradePrice << ","
                                         << vContractQuoteDatas[0]->iAccumuTradeSize << ","
                                         << _iLastLowBidSize << ","
                                         << _dLastLowBid << ","
                                         << _iLastHighBidSize << ","
                                         << _dLastHighBid << ","
                                         << _dLastHighAsk << ","
                                         << _iLastHighAskSize << ","
                                         << _dLastLowAsk << ","
                                         << _iLastLowAskSize << ","
                                         << dVWAP << "\n";

                    GridData cNewDataPoint;
                    cNewDataPoint.iEpochTimeStamp = cCallTime.igetPrintable();
                    cNewDataPoint.dBid = vContractQuoteDatas[0]->dBestBid;
                    cNewDataPoint.iBidSize = vContractQuoteDatas[0]->iBidSize;
                    cNewDataPoint.dAsk = vContractQuoteDatas[0]->dBestAsk;
                    cNewDataPoint.iAskSize = vContractQuoteDatas[0]->iAskSize;
                    cNewDataPoint.dLast = vContractQuoteDatas[0]->dLastTradePrice;
                    cNewDataPoint.iAccumuTradeSize = vContractQuoteDatas[0]->iAccumuTradeSize;
                    cNewDataPoint.dLowBid = _dLastLowBid;
                    cNewDataPoint.iLowBidSize = _iLastLowBidSize;
                    cNewDataPoint.dHighBid = _dLastHighBid;
                    cNewDataPoint.iHighBidSize = _iLastHighBidSize;
                    cNewDataPoint.dHighAsk = _dLastHighAsk;
                    cNewDataPoint.iHighAskSize = _iLastHighAskSize;
                    cNewDataPoint.dLowAsk = _dLastLowAsk;
                    cNewDataPoint.iLowAskSize = _iLastLowAskSize;
                    cNewDataPoint.dVWAP = dVWAP;

                    _vGridData.push_back(cNewDataPoint);

                    _dLastLowBid = vContractQuoteDatas[0]->dBestBid;
                    _iLastLowBidInTicks = vContractQuoteDatas[0]->iBestBidInTicks;
                    _iLastLowBidSize = vContractQuoteDatas[0]->iBidSize;

                    _dLastHighBid = vContractQuoteDatas[0]->dBestBid;
                    _iLastHighBidInTicks = vContractQuoteDatas[0]->iBestBidInTicks;
                    _iLastHighBidSize = vContractQuoteDatas[0]->iBidSize;

                    _dLastHighAsk = vContractQuoteDatas[0]->dBestAsk;
                    _iLastHighAskInTicks  = vContractQuoteDatas[0]->iBestAskInTicks;
                    _iLastHighAskSize = vContractQuoteDatas[0]->iAskSize;

                    _dLastLowAsk = vContractQuoteDatas[0]->dBestAsk;
                    _iLastLowAskInTicks  = vContractQuoteDatas[0]->iBestAskInTicks;
                    _iLastLowAskSize = vContractQuoteDatas[0]->iAskSize;

                    _dVWAPConsideration = 0;
                    _iVWAPVolume = 0;
                }
            }
        }
    }
}

}
