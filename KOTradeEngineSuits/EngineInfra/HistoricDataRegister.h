#ifndef HistoricDataRegister_H
#define HistoricDataRegister_H

#include "KOEpochTime.h"
#include "QuoteData.h"

using std::string;
using std::map;
using std::vector;
using std::ifstream;

namespace KO
{

class KOScheduler;

class HistoricDataRegister
{
public:
    HistoricDataRegister(KOScheduler* pScheduler, const string& sDate, const string& sHistoricDataPath, const string& sSimLocation, bool bUse100ms);
    ~HistoricDataRegister();

    bool loadData();    
    void psubscribeNewProduct(QuoteData* pNewQuoteData, int iArtificialSpread);
    KOEpochTime cgetNextUpdateTime();
    void applyNextUpdate(KOEpochTime cNextTimeStamp); 
    bool bproductHasNewUpdate(int iProductIndex);

private:

    struct HistoricUpdate
    {
        KOEpochTime cTime;
        double dBid;
        long iBidSize;
        double dAsk;
        long iAskSize;
        double dLast;
        long iAccumuTradeSize;
        double dVwap;
        bool bHasMoreData;
    };

    vector<QuoteData*> _vRegisteredProducts;

    vector<bool> _vProductUpdated;

    vector<int> _vArtificialSpread;

    KOScheduler* _pScheduler;
    string _sDate;
    string _sHistoricDataPath;
    string _sSimLocation;

    bool _bUse100ms;

    /************ text file data loading ******************/
    vector<boost::shared_ptr<ifstream> > _vDataFileStreams;
    vector<boost::shared_ptr<HistoricUpdate> > _vNextUpdateForProducts;
    /******************************************************/

    /************ h5 file data loading ********************/
    vector<GridData*> _vData;
    vector< vector<KOEpochTime> > _vDataTimeStamp;
    vector<long> _vNextUpdateIndex;
    vector<long> _vNumberDataPoint;
    /******************************************************/
};

}

#endif
