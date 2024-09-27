#include "HistoricDataRegister.h"
#include "SystemClock.h"
#include "KOScheduler.h"
#include <boost/math/special_functions/round.hpp>
#include <H5Cpp.h>
#include <sys/stat.h>

using namespace H5;
using std::cerr;
using std::stringstream;

namespace KO
{

HistoricDataRegister::HistoricDataRegister(KOScheduler* pScheduler, const string& sDate)
:_pScheduler(pScheduler),
 _sDate(sDate)
{

}

HistoricDataRegister::~HistoricDataRegister()
{
    for(unsigned int i = 0; i < _vData.size(); i++)
    {
        delete _vData[i];
    }
}

void HistoricDataRegister::psubscribeNewProduct(QuoteData* pNewQuoteData, KOEpochTime cPriceLatency)
{
    _vRegisteredProducts.push_back(pNewQuoteData);

    map<string, KOEpochTime>::iterator itr = _vProductLatencyMap.find(pNewQuoteData->sProduct);
    if(itr == _vProductLatencyMap.end())
    {
        _vProductLatencyMap[pNewQuoteData->sProduct] = cPriceLatency;
    }

    _vProductUpdated.push_back(false);
}

bool HistoricDataRegister::loadData()
{
    bool bResult = true;

    for(unsigned int i = 0; i < _vRegisteredProducts.size(); i++)
    {
        stringstream cStringStream;
        cStringStream << std::getenv("GRIDDATAPATH") << "/GRID60FLAT/" << _vRegisteredProducts[i]->sRoot << "/" << _vRegisteredProducts[i]->sProduct << "_" << _sDate.substr(0,4) << "." << _sDate.substr(4,2) << "." << _sDate.substr(6,2) << "_1s.h5";
        string sDataFile = cStringStream.str();

        struct stat buffer;
        if(stat (sDataFile.c_str(), &buffer) == 0)
        {
            H5File cFile (sDataFile, H5F_ACC_RDONLY);
            DataSet cDataSet = cFile.openDataSet("GridData");

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
                _vDataTimeStamp[i].push_back(KOEpochTime(0, pData[index].iEpochTimeStamp) + _vProductLatencyMap[_vRegisteredProducts[i]->sProduct]);
                _pScheduler->addNewPriceUpdateCall(KOEpochTime(0, pData[index].iEpochTimeStamp) + _vProductLatencyMap[_vRegisteredProducts[i]->sProduct]);
            }
        }
        else
        {
            cerr << "Failed to find HDF5 data file " << sDataFile << "\n";
            bResult = bResult && false;
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
                cNextTimeStamp = _vDataTimeStamp[i][_vNextUpdateIndex[i]];
            }
            else
            {
                if(_vDataTimeStamp[i][_vNextUpdateIndex[i]] < cNextTimeStamp)
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
        if(_vDataTimeStamp[i][_vNextUpdateIndex[i]] == cNextTimeStamp)
        {
            _vRegisteredProducts[i]->cControlUpdateTime = cNextTimeStamp;
            _vRegisteredProducts[i]->iBidSize = _vData[i][_vNextUpdateIndex[i]].iBidSize;
            _vRegisteredProducts[i]->iAskSize = _vData[i][_vNextUpdateIndex[i]].iAskSize;

            _vRegisteredProducts[i]->dBestBid = _vData[i][_vNextUpdateIndex[i]].dBid;
            _vRegisteredProducts[i]->dBestAsk = _vData[i][_vNextUpdateIndex[i]].dAsk;

            _vRegisteredProducts[i]->iBestBidInTicks = boost::math::iround(_vRegisteredProducts[i]->dBestBid / _vRegisteredProducts[i]->dTickSize);
            _vRegisteredProducts[i]->iBestAskInTicks = boost::math::iround(_vRegisteredProducts[i]->dBestAsk / _vRegisteredProducts[i]->dTickSize);

            _vRegisteredProducts[i]->dLastTradePrice = _vData[i][_vNextUpdateIndex[i]].dLast;
            _vRegisteredProducts[i]->iLastTradeInTicks = boost::math::iround(_vRegisteredProducts[i]->dLastTradePrice / _vRegisteredProducts[i]->dTickSize); 

            _vRegisteredProducts[i]->iTradeSize = _vData[i][_vNextUpdateIndex[i]].iAccumuTradeSize - _vRegisteredProducts[i]->iAccumuTradeSize;
            _vRegisteredProducts[i]->iAccumuTradeSize = _vData[i][_vNextUpdateIndex[i]].iAccumuTradeSize;

            _vRegisteredProducts[i]->dLowBid = _vData[i][_vNextUpdateIndex[i]].dLowBid;
            _vRegisteredProducts[i]->iLowBidInTicks = boost::math::iround(_vRegisteredProducts[i]->dLowBid / _vRegisteredProducts[i]->dTickSize);
            _vRegisteredProducts[i]->iLowBidSize = _vData[i][_vNextUpdateIndex[i]].iLowBidSize;

            _vRegisteredProducts[i]->dHighBid = _vData[i][_vNextUpdateIndex[i]].dHighBid;
            _vRegisteredProducts[i]->iHighBidInTicks = boost::math::iround(_vRegisteredProducts[i]->dHighBid / _vRegisteredProducts[i]->dTickSize);
            _vRegisteredProducts[i]->iHighBidSize = _vData[i][_vNextUpdateIndex[i]].iHighBidSize;

            _vRegisteredProducts[i]->dLowAsk = _vData[i][_vNextUpdateIndex[i]].dLowAsk;
            _vRegisteredProducts[i]->iLowAskInTicks = boost::math::iround(_vRegisteredProducts[i]->dLowAsk / _vRegisteredProducts[i]->dTickSize);
            _vRegisteredProducts[i]->iLowAskSize = _vData[i][_vNextUpdateIndex[i]].iLowAskSize;

            _vRegisteredProducts[i]->dHighAsk = _vData[i][_vNextUpdateIndex[i]].dHighAsk;
            _vRegisteredProducts[i]->iHighAskInTicks = boost::math::iround(_vRegisteredProducts[i]->dHighAsk / _vRegisteredProducts[i]->dTickSize);
            _vRegisteredProducts[i]->iHighAskSize = _vData[i][_vNextUpdateIndex[i]].iHighAskSize;

            _vNextUpdateIndex[i] = _vNextUpdateIndex[i] + 1;

            _vProductUpdated[i] = true;
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
