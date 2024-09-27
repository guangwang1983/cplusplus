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
    long iBidSize;
    double dAsk;
    long iAskSize;
    double dLast;
    long iAccumuTradeSize;

    double dHighBid;
    long iHighBidSize;

    double dLowBid;
    long iLowBidSize;

    double dLowAsk;
    long iLowAskSize;

    double dHighAsk;
    long iHighAskSize;

    double dVWAP;
};

struct QuoteData
{
    KOEpochTime                     cControlUpdateTime;
    string                          sProduct;
    string                          sHCProduct;

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

    double                          dLastTradePrice;
    long                            iLastTradeInTicks;
    long                            iTradeSize;
    long                            iAccumuTradeSize;

    double                          dLowAsk;
    long                            iLowAskInTicks;
    long                            iLowAskSize;

    double                          dHighAsk;
    long                            iHighAskInTicks;
    long                            iHighAskSize;

    double                          dHighBid;
    long                            iHighBidInTicks;
    long                            iHighBidSize;

    double                          dLowBid;
    long                            iLowBidInTicks;
    long                            iLowBidSize;

    double                          dClose;
    double                          dRefPrice;

    double                          dRateToDollar;

    double                          dTradingFee;

    string                          sTradeType;

    InstrumentType                  eInstrumentType;

    bool                            bPriceValid;
    bool                            bPriceInvalidTriggered;
    KOEpochTime                     cPriceInvalidTime;

    KOEpochTime                     cMarketOpenTime;
    KOEpochTime                     cMarketCloseTime;

/****************** HC insturment data *************************/
    short                           iGatewayID;
    int64_t                         iSecurityID;
/***************************************************************/
};

}

#endif /* QuoteData_H */
