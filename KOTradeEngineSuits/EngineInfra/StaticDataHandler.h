#ifndef STATICDATAHANDLER_H
#define STATICDATAHANDLER_H

#include <string>
#include <map>
#include "KOEpochTime.h"
#include "QuoteData.h"

using std::string;
using std::map;

namespace KO
{

class StaticDataHandler
{
public:
    StaticDataHandler(string sFXRateFileName, string sProductSpecFileName, string sTickSizeFileName, string sFXArtificialSpreadFile, string sTodayDate);
    ~StaticDataHandler();

    string sGetRootSymbol(const string& sProduct, InstrumentType eInstrumentType);
    string sGetWQSymbol(const string& sProduct, InstrumentType eInstrumentType);
    string sGetCurrency(const string& sRootSymbol, const string& sExchange);
    string sGetHCExchange(const string& sRootSymbol, const string& sExchange);
    double dGetTradingFee(const string& sRootSymbol, const string& sExchange);
    double dGetFXRate(const string& sFXPair);
    double dGetTickSize(const string& sFullProductName);
    double dGetContractSize(const string& sRootSymbol, const string& sExchange);
    KOEpochTime cGetMarketOpenTime(const string& sRootSymbol, const string& sExchange);
    KOEpochTime cGetMarketCloseTime(const string& sRootSymbol, const string& sExchange);
    string sGetProductTyep(const string& sRootSymbol, const string& sExchange);
    long iGetNY4Latency(const string& sKOProduct);
    long iGetTelcityLatency(const string& sKOProduct);
    int iGetFXArticifialSpread(const string& sRootSymbol); 

private:
    string _sTodayDate;

    map<string, double> _mFXRate;
    map<string, double> _mTradingFee;
    map<string, string> _mCurrency;
    map<string, string> _mHCExchange;
    map<string, double> _mTickSize;
    map<string, double> _mContractSize;
    map<string, KOEpochTime> _mMarketOpenTime;
    map<string, KOEpochTime> _mMarketCloseTime;
    map<string, string> _mProductType;
    map<string, string> _mProductRIC;
    map<string, long> _mProductNY4Latency;
    map<string, long> _mProductTelcityLatency;
    map<string, int> _mFXArtificialSpread;
};

};

#endif
