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
    StaticDataHandler(string sFXRateFileName, string sProductSpecFileName, string sTickSizeFileName, string sTodayDate);
    ~StaticDataHandler();

    string sGetRootSymbol(const string& sProduct, InstrumentType eInstrumentType);
    string sGetCurrency(const string& sRootSymbol, const string& sExchange);
    string sGetHCExchange(const string& sRootSymbol, const string& sExchange);
    double dGetTradingFee(const string& sRootSymbol, const string& sExchange);
    double dGetFXRate(const string& sFXPair);
    double dGetTickSize(const string& sRootSymbol, const string& sExchange);
    double dGetContractSize(const string& sRootSymbol, const string& sExchange);
    KOEpochTime cGetMarketOpenTime(const string& sRootSymbol, const string& sExchange);
    KOEpochTime cGetMarketCloseTime(const string& sRootSymbol, const string& sExchange);
    string sGetProductTyep(const string& sRootSymbol, const string& sExchange);

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
};

};

#endif
