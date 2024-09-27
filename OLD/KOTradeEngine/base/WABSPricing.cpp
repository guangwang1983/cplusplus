#include "WABSPricing.h"

namespace KO
{

double WABSPricing::dgetSpread()
{
	double dSpread;

	double dQuoteNormDelta = (_vInstruments[0]->dgetWeightedMid() - _vInstruments[0]->dgetEXMA()) / _vInstruments[0]->dgetWeightedStdev();
        double dSignalNormDelta = (_vInstruments[1]->dgetWeightedMid() - _vInstruments[1]->dgetEXMA()) / _vInstruments[1]->dgetWeightedStdev();

	dSpread = dQuoteNormDelta - dSignalNormDelta;

	return dSpread;
}

double WABSPricing::dgetQuoteBid(int iProductIndex, double dOffsetStdev)
{
	double dQuoteBid = 0;

	if(iProductIndex == 0)
	{
		dQuoteBid = floor(((_vInstruments[1]->dgetWeightedMid() - _vInstruments[1]->dgetEXMA()) / _vInstruments[1]->dgetWeightedStdev() 
			    - dOffsetStdev * _pWeightedStdev->dgetWeightedStdev()) * _vInstruments[0]->dgetWeightedStdev() + _vInstruments[0]->dgetEXMA());
	}
	else if(iProductIndex == 1)
	{
		dQuoteBid = floor(((_vInstruments[0]->dgetWeightedMid() - _vInstruments[0]->dgetEXMA()) / _vInstruments[0]->dgetWeightedStdev() 
			    + dOffsetStdev * _pWeightedStdev->dgetWeightedStdev()) * _vInstruments[1]->dgetWeightedStdev() + _vInstruments[1]->dgetEXMA());
	}

	return dQuoteBid;
}

double WABSPricing::dgetQuoteOffer(int iProductIndex, double dOffsetStdev)
{
	double dQuoteOffer = 0;

	if(iProductIndex == 0)
	{
		dQuoteOffer = ceil(((_vInstruments[1]->dgetWeightedMid() - _vInstruments[1]->dgetEXMA()) / _vInstruments[1]->dgetWeightedStdev() 
			    + dOffsetStdev * _pWeightedStdev->dgetWeightedStdev()) * _vInstruments[0]->dgetWeightedStdev() + _vInstruments[0]->dgetEXMA());
	}
	else if(iProductIndex == 1)
	{
		dQuoteOffer = ceil(((_vInstruments[0]->dgetWeightedMid() - _vInstruments[0]->dgetEXMA()) / _vInstruments[0]->dgetWeightedStdev() 
			    - dOffsetStdev * _pWeightedStdev->dgetWeightedStdev()) * _vInstruments[1]->dgetWeightedStdev() + _vInstruments[1]->dgetEXMA());
	}

	return dQuoteOffer;
}

}
