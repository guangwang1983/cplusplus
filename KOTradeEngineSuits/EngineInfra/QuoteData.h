#ifndef QuoteData_H
#define QuoteData_H

#include "KOEpochTime.h"
#include <boost/date_time/posix_time/posix_time.hpp>

using std::string;

namespace KO
{

enum InstrumentType
{
    KO_FUTURE,
    KO_FX
};

struct GridData
{
    long iEpochTimeStamp;
    double dBid;
    long iBidInTicks;
    long iBidSize;
    double dAsk;
    long iAskInTicks;
    long iAskSize;
    double dLast;
    long iLastInTicks;
    long iTradeSize;
    long iAccumuTradeSize;
    double dWeightedMid;
    double dWeightedMidInTicks;
};

struct QuoteData
{
    KOEpochTime                     cControlUpdateTime;
    string                          sProduct;
    string                          sHCProduct;
    string                          sTBProduct;

    string                          sRoot;

    int                             iCID;

    string                          sExchange;
    string                          sHCExchange;

    double                          dTickSize;
    double                          dContractSize;

    long                            iMaxSpreadWidth;

    long                            iBidSize;
    long                            iAskSize;

    double                          dBestBid;
    double                          dBestAsk;
    long                            iBestBidInTicks;
    long                            iBestAskInTicks;

    long                            iPrevBidInTicks;
    long                            iPrevAskInTicks;
    long                            iPrevBidSize;
    long                            iPrevAskSize;

    double                          dLastTradePrice;
    long                            iLastTradeInTicks;
    long                            iTradeSize;
    long                            iAccumuTradeSize;

    double                          dWeightedMid;
    double                          dWeightedMidInTicks;

    double                          dClose;
    double                          dRefPrice;

    double                          dRateToDollar;
    string                          sCurrency;

    double                          dTradingFee;

    string                          sTradeType;

    InstrumentType                  eInstrumentType;

    bool                            bPriceValid;
    bool                            bPriceInvalidTriggered;
    KOEpochTime                     cPriceInvalidTime;

    KOEpochTime                     cMarketOpenTime;
    KOEpochTime                     cMarketCloseTime;

    bool                            bIsLocalProduct;
    int                             iProductMaxRisk;
    int                             iProductExpoLimit;

    bool                            bStalenessErrorTriggered;
    KOEpochTime                     cLastUpdateTime;

    bool                            bDataSubscribed;
};

}

#endif /* QuoteData_H */
