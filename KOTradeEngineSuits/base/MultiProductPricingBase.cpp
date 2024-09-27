#include "MultiProductPricingBase.h"

namespace KO
{

MultiProductPricingBase::~MultiProductPricingBase()
{
	_vInstruments.clear();
}

void MultiProductPricingBase::addNewProduct(boost::shared_ptr<Instrument> pInstrument)
{
	_vInstruments.push_back(pInstrument);
}

double MultiProductPricingBase::dgetSpreadWeightedStdev()
{
	return _pWeightedStdev->dgetWeightedStdev();
}

bool MultiProductPricingBase::bgetSpreadWeightedStdevValid()
{
	return _pWeightedStdev->bgetStdevValid();
}

void MultiProductPricingBase::useWeightedStdev(long iStdevLength, bool bDailyUpdate, boost::posix_time::time_duration cUpdateTime, int iUpdateFreqSeconds)
{
	_bUseWeightedStdev = true;
	_iWeightedStdevLength = iStdevLength;
	_bWeightedStdevDailyUpdate = bDailyUpdate;
	_cWeightedStdevUpdateTime = cUpdateTime;
	_iWeightedStdevUpdateFreqSeconds = iUpdateFreqSeconds;
	_pWeightedStdev.reset(new WeightedStdev(_iWeightedStdevLength));
}

void MultiProductPricingBase::applyWeightedStdevAdjustment(double dNewAdjustment)
{
	_pWeightedStdev->applyAdjustment(dNewAdjustment);
}

double MultiProductPricingBase::dgetWeightedStdevEXMA()
{
	return _pWeightedStdev->dgetEXMA();
}

double MultiProductPricingBase::dgetWeightedStdevSqrdEXMA()
{
	return _pWeightedStdev->dgetSqrdEXMA();
}

long MultiProductPricingBase::igetWeightedStdevNumDataPoints()
{
    return _pWeightedStdev->igetNumDataPoint();
}

double MultiProductPricingBase::dgetWeightedStdevAdjustment()
{
	return _pWeightedStdev->dgetAdjustment();
}

void MultiProductPricingBase::dsetNewWeightedStdevEXMA(double dNewSprdEXMA, double dNewEXMA, long iNumDataPoints)
{
	_pWeightedStdev->dsetNewEXMA(dNewSprdEXMA, dNewEXMA, iNumDataPoints);
}

void MultiProductPricingBase::wakeup(boost::posix_time::time_duration cT)
{
	if(_bUseWeightedStdev)
	{
		if(_bWeightedStdevDailyUpdate)
		{
			if(cT.hours() == _cWeightedStdevUpdateTime.hours() &&
			   cT.minutes() == _cWeightedStdevUpdateTime.minutes() &&
			   cT.seconds() == _cWeightedStdevUpdateTime.seconds())
			{
				_pWeightedStdev->dnewData(dgetSpread());
			}
		}
		else
		{
			int iTimeInNumMins = cT.total_seconds();
			int iModTimeInMins = iTimeInNumMins % _iWeightedStdevUpdateFreqSeconds;
			if(iModTimeInMins == 0)
			{
				_pWeightedStdev->dnewData(dgetSpread());
			}
		}
	}
}

}
