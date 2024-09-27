#ifndef MULTIPRODUCTPRICINGBASE_H
#define MULTIPRODUCTPRICINGBASE_H

#include "Instrument.h"
#include "WeightedStdev.h"

using std::vector;

namespace KO
{

class MultiProductPricingBase
{

public:
	~MultiProductPricingBase();

	virtual double dgetSpread() = 0;
	virtual double dgetQuoteBid(int iProductIndex, double dOffset) = 0;
	virtual double dgetQuoteOffer(int iProductIndex, double dOffset) = 0;
	
	double dgetSpreadWeightedStdev();
	bool bgetSpreadWeightedStdevValid();	

	void addNewProduct(boost::shared_ptr<Instrument> pInstrument);
	void wakeup(boost::posix_time::time_duration cT);

	void useWeightedStdev(long iStdevLength, bool bDailyUpdate, boost::posix_time::time_duration cUpdateTime, int iUpdateFreqSeconds);
	void applyWeightedStdevAdjustment(double dNewAdjustment);
	double dgetWeightedStdevEXMA();
	double dgetWeightedStdevSqrdEXMA();
    long igetWeightedStdevNumDataPoints();
	double dgetWeightedStdevAdjustment();
	void dsetNewWeightedStdevEXMA(double dNewSprdEXMA, double dNewEXMA, long iNumDataPoints);
	
protected:
	vector< boost::shared_ptr<Instrument> > _vInstruments;

	bool _bUseWeightedStdev;
	int _iWeightedStdevLength;
	bool _bWeightedStdevDailyUpdate;
	boost::posix_time::time_duration _cWeightedStdevUpdateTime;
	int _iWeightedStdevUpdateFreqSeconds;
	boost::shared_ptr<WeightedStdev> _pWeightedStdev;
};

}

#endif /* MULTIPRODUCTPRICINGBASE_H */
