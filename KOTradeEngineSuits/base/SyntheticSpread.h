#ifndef SYNTHETICSPREAD_H
#define SYNTHETICSPREAD_H

#include "../EngineInfra/KOEpochTime.h"
#include "Product.h"

namespace KO
{

class SyntheticSpread : public Product
{
public:
	enum LegPriceType
	{
		WhiteAbsNoDetrend,
		WhiteAbs,
        Simple
	};

	SyntheticSpread(const string& sProductName, Product* pFrontLeg, LegPriceType eFrontLegPriceType, Product* pBackLeg, LegPriceType eBackLegPriceType, double dBackLegWeight, bool bPositiveCorrelation, bool bUseRealSignalPrice);
	~SyntheticSpread();

	virtual double dgetWeightedMid();
	virtual double dgetBid();
	virtual double dgetAsk();
	virtual bool bgetPriceValid();

    double dgetFrontLegPrice();
    double dgetBackLegPrice();

	virtual void wakeup(KOEpochTime cT);

	double dgetFrontQuoteBid(double dOffsetStdev);
	double dgetFrontQuoteOffer(double dOffsetStdev);

    double dgetPriceFromFrontLegPrice(double dFrontPrice);

    long igetASC();
private:

	Product* _pFrontLeg;
	LegPriceType			   _eFrontLegPriceType;

	Product* _pBackLeg;
	LegPriceType			   _eBackLegPriceType;
    double                     _dBackLegWeight;

	bool 					   _bPositiveCorrelation;
    bool                       _bUseRealSignalPrice;
};

}

#endif /* SPREAD_H */
