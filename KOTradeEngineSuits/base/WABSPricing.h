#ifndef WABSPRICING_H
#define WABSPRICING_H

#include "MultiProductPricingBase.h"

namespace KO
{

class WABSPricing : public MultiProductPricingBase
{
public:
	double dgetSpread();
	double dgetQuoteBid(int iProductIndex, double dOffsetStdev);
	double dgetQuoteOffer(int iProductIndex, double dOffsetStdev);
};

}

#endif /* WABSPRICING_H */
