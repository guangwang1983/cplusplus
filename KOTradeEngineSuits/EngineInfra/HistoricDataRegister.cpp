#include "HistoricDataRegister.h"
#include "SystemClock.h"
#include "KOScheduler.h"
#include <boost/math/special_functions/round.hpp>
#include <H5Cpp.h>
#include <sys/stat.h>
#include <cstdlib>

using namespace H5;
using std::cerr;
using std::stringstream;

namespace KO
{

HistoricDataRegister::HistoricDataRegister(KOScheduler* pScheduler, const string& sDate, const string& sHistoricDataPath, const string& sSimLocation, bool bUse100ms)
:_pScheduler(pScheduler),
 _sDate(sDate),
 _sHistoricDataPath(sHistoricDataPath),
 _sSimLocation(sSimLocation),
 _bUse100ms(bUse100ms)
{

}

HistoricDataRegister::~HistoricDataRegister()
{
    for(unsigned int i = 0; i < _vData.size(); i++)
    {
        delete _vData[i];
    }
}

void HistoricDataRegister::psubscribeNewProduct(QuoteData* pNewQuoteData)
{
    _vRegisteredProducts.push_back(pNewQuoteData);

    _vProductUpdated.push_back(false);
}

bool HistoricDataRegister::loadData()
{
    bool bResult = true;
    for(unsigned int i = 0; i < _vRegisteredProducts.size(); i++)
    {
        stringstream cStringStream;
//        cStringStream << std::getenv("HDF5SECONDDATAREADPATH") << "/" << _sSimLocation << "/" << _vRegisteredProducts[i]->sExchange << "." << _vRegisteredProducts[i]->sRoot << "/" << _sSimLocation << "_" << _vRegisteredProducts[i]->sProduct << "_" << _sDate.substr(0,4) << "." << _sDate.substr(4,2) << "." << _sDate.substr(6,2) << "_1s.h5";
        cStringStream << _sHistoricDataPath << "/" << _sSimLocation << "/" << _vRegisteredProducts[i]->sExchange << "." << _vRegisteredProducts[i]->sRoot << "/" << _sSimLocation << "_" << _vRegisteredProducts[i]->sProduct << "_" << _sDate.substr(0,4) << "." << _sDate.substr(4,2) << "." << _sDate.substr(6,2);

        if(_bUse100ms)
        {
             cStringStream << "_100ms.h5";
        }
        else
        {
             cStringStream << "_1s.h5";
        }

        string sDataFile = cStringStream.str();

		cerr << sDataFile << "\n";

        struct stat buffer;
        if(stat (sDataFile.c_str(), &buffer) == 0)
        {
            try
            {
                H5File cFile (sDataFile, H5F_ACC_RDONLY);
                DataSet cDataSet = cFile.openDataSet("GridData");

                CompType cGridDataType(sizeof(GridData));
                cGridDataType.insertMember("EpochTimeStamp", HOFFSET(GridData, iEpochTimeStamp), PredType::NATIVE_LONG);

                cGridDataType.insertMember("Bid", HOFFSET(GridData, dBid), PredType::NATIVE_DOUBLE);
                cGridDataType.insertMember("BidInTicks", HOFFSET(GridData, iBidInTicks), PredType::NATIVE_LONG);
                cGridDataType.insertMember("BidSize", HOFFSET(GridData, iBidSize), PredType::NATIVE_LONG);

                cGridDataType.insertMember("Ask", HOFFSET(GridData, dAsk), PredType::NATIVE_DOUBLE);
                cGridDataType.insertMember("AskInTicks", HOFFSET(GridData, iAskInTicks), PredType::NATIVE_LONG);
                cGridDataType.insertMember("AskSize", HOFFSET(GridData, iAskSize), PredType::NATIVE_LONG);

                cGridDataType.insertMember("Last", HOFFSET(GridData, dLast), PredType::NATIVE_DOUBLE);
                cGridDataType.insertMember("LastInTicks", HOFFSET(GridData, iLastInTicks), PredType::NATIVE_LONG);
                cGridDataType.insertMember("TradeSize", HOFFSET(GridData, iTradeSize), PredType::NATIVE_LONG);
                cGridDataType.insertMember("AccumumTradeSize", HOFFSET(GridData, iAccumuTradeSize), PredType::NATIVE_LONG);
                cGridDataType.insertMember("WeightedMid", HOFFSET(GridData, dWeightedMid), PredType::NATIVE_DOUBLE);
                cGridDataType.insertMember("WeightedMidInTicks", HOFFSET(GridData, dWeightedMidInTicks), PredType::NATIVE_DOUBLE);

                hsize_t cDim[1];
                DataSpace cDataSpace = cDataSet.getSpace();
                cDataSpace.getSimpleExtentDims(cDim);

                GridData* pData = new GridData [cDim[0]];
                cDataSet.read(pData, cGridDataType);

                _vData.push_back(pData);
                _vNextUpdateIndex.push_back(0);
                _vNumberDataPoint.push_back(cDim[0]);

                _vDataTimeStamp.push_back(vector<KOEpochTime>());
                for(unsigned long index = 0; index < cDim[0]; index++)
                {
                    _vDataTimeStamp[i].push_back(KOEpochTime(0, pData[index].iEpochTimeStamp));
                    _pScheduler->addNewPriceUpdateCall(KOEpochTime(0, pData[index].iEpochTimeStamp));
                }
            }
            catch (...)
            {
                cerr << "Error loading HDF5 data file " << sDataFile << "\n";
                bResult = bResult && false;
                exit(0);
            }
        }
        else
        {
            cerr << "Failed to find HDF5 data file " << sDataFile << "\n";
            bResult = bResult && false;
            exit(0);
        }
    }

    return bResult;
}

KOEpochTime HistoricDataRegister::cgetNextUpdateTime()
{
    KOEpochTime cNextTimeStamp = KOEpochTime();
    
    for(unsigned int i = 0; i < _vData.size(); ++i)
    {
        if(_vNextUpdateIndex[i] < _vNumberDataPoint[i])
        {
            if(cNextTimeStamp == KOEpochTime())
            {
                if(_vDataTimeStamp[i].size() > 0)
                {
                    cNextTimeStamp = _vDataTimeStamp[i][_vNextUpdateIndex[i]];
                }
            }
            else
            {
                if(_vDataTimeStamp[i].size() > 0 &&
                   _vDataTimeStamp[i][_vNextUpdateIndex[i]] < cNextTimeStamp)
                {
                    cNextTimeStamp = _vDataTimeStamp[i][_vNextUpdateIndex[i]];
                }
            }
        }
    }

    return cNextTimeStamp;
}

void HistoricDataRegister::applyNextUpdate(KOEpochTime cNextTimeStamp)
{
    for(unsigned int i = 0; i < _vData.size(); ++i)
    {
        if(_vDataTimeStamp[i].size() > 0 &&
           _vDataTimeStamp[i][_vNextUpdateIndex[i]] == cNextTimeStamp)
        {

            if(_vData[i][_vNextUpdateIndex[i]].iBidSize != 0 &&
               _vData[i][_vNextUpdateIndex[i]].iAskSize != 0 &&
               _vData[i][_vNextUpdateIndex[i]].iBidInTicks != 0 &&
               _vData[i][_vNextUpdateIndex[i]].iAskInTicks != 0)
            {
                _vRegisteredProducts[i]->cControlUpdateTime = cNextTimeStamp;
                _vRegisteredProducts[i]->iBidSize = _vData[i][_vNextUpdateIndex[i]].iBidSize;
                _vRegisteredProducts[i]->iAskSize = _vData[i][_vNextUpdateIndex[i]].iAskSize;

                _vRegisteredProducts[i]->dBestBid = _vData[i][_vNextUpdateIndex[i]].dBid;
                _vRegisteredProducts[i]->dBestAsk = _vData[i][_vNextUpdateIndex[i]].dAsk;

                _vRegisteredProducts[i]->iBestBidInTicks = _vData[i][_vNextUpdateIndex[i]].iBidInTicks;
                _vRegisteredProducts[i]->iBestAskInTicks = _vData[i][_vNextUpdateIndex[i]].iAskInTicks;

                _vRegisteredProducts[i]->dLastTradePrice = _vData[i][_vNextUpdateIndex[i]].dLast;
                _vRegisteredProducts[i]->iLastTradeInTicks = _vData[i][_vNextUpdateIndex[i]].iLastInTicks; 

                _vRegisteredProducts[i]->iTradeSize = _vData[i][_vNextUpdateIndex[i]].iTradeSize;
                _vRegisteredProducts[i]->iAccumuTradeSize = _vData[i][_vNextUpdateIndex[i]].iAccumuTradeSize;

                _vRegisteredProducts[i]->dWeightedMid = _vData[i][_vNextUpdateIndex[i]].dWeightedMid;
                _vRegisteredProducts[i]->dWeightedMidInTicks = _vData[i][_vNextUpdateIndex[i]].dWeightedMidInTicks;

                _vProductUpdated[i] = true;
            }
            else
            {
                _vProductUpdated[i] = false;
            }

            while(_vDataTimeStamp[i][_vNextUpdateIndex[i]] <= cNextTimeStamp)
            {
                _vNextUpdateIndex[i] = _vNextUpdateIndex[i] + 1;
                if(_vNextUpdateIndex[i] == (int)_vDataTimeStamp[i].size())
                {
                    break;
                }
            }

//            _vNextUpdateIndex[i] = _vNextUpdateIndex[i] + 1;
        }
        else
        {
            _vProductUpdated[i] = false;
        }
    }
}

bool HistoricDataRegister::bproductHasNewUpdate(int iProductIndex)
{
    return _vProductUpdated[iProductIndex];
}

}
