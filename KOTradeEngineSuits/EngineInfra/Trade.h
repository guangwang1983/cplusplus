#ifndef Trade_H
#define Trade_H

#include <string>
#include "KOEpochTime.h"
#include "QuoteData.h"

using std::string;

namespace KO
{

struct Trade
{
	KOEpochTime                 cTradeTime;
	string 						sProduct;
	long                        iQty;
	double                      dPrice;
    InstrumentType              eInstrumentType;
};

class compareTradeByTime
{
public:
    compareTradeByTime(){}

    bool operator() (boost::shared_ptr<Trade> left, boost::shared_ptr<Trade> right) const
    {
		return (left->cTradeTime < right->cTradeTime);
    }
};

}

#endif /* Trade_H */
