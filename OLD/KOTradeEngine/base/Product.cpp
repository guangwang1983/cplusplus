#include "Product.h"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/lexical_cast.hpp>

namespace KO
{
Product::Product()
:_bUseEXMA(false),
 _bUseWeightedStdev(false),
 _bUseSMA(false),
 _bUseStdev(false)
{
    _pEXMA = NULL;
	_pWeightedStdev = NULL;
	_pSMA = NULL;
	_pStdev = NULL;
}

Product::~Product()
{
    delete _pEXMA;
    delete _pWeightedStdev;
    delete _pSMA;
    delete _pStdev;
}

string Product::sgetProductName()
{
	return _sProductName;
}

Product::ProductType Product::egetProductType()
{
    return _eProductType;
}

void Product::useEXMA(long iEXMALength, bool bDailyUpdate, KOEpochTime cUpdateTime, int iUpdateFreqSeconds, KOEpochTime cEXMAStartTime, KOEpochTime cEXMAEndTime)
{
	_bUseEXMA = true;
	_iEXMALength = iEXMALength;
	_bEXMADailyUpdate = bDailyUpdate;
	_cEXMAUpdateTime = cUpdateTime;
	_iEXMAUpdateFreqSeconds = iUpdateFreqSeconds;
    _cEXMAStartTime = cEXMAStartTime;
    _cEXMAEndTime = cEXMAEndTime;
	_pEXMA = new EXMA(_iEXMALength);
}

void Product::useWeightedStdev(long iStdevLength, bool bDailyUpdate, KOEpochTime cUpdateTime, int iUpdateFreqSeconds, KOEpochTime cWeightedStdevStartTime, KOEpochTime cWeightedStdevEndTime)
{
	_bUseWeightedStdev = true;
	_iWeightedStdevLength = iStdevLength;
	_bWeightedStdevDailyUpdate = bDailyUpdate;
	_cWeightedStdevUpdateTime = cUpdateTime;
	_iWeightedStdevUpdateFreqSeconds = iUpdateFreqSeconds;
    _cWeightedStdevStartTime = cWeightedStdevStartTime;
    _cWeightedStdevEndTime = cWeightedStdevEndTime;
	_pWeightedStdev = new WeightedStdev(_iWeightedStdevLength);
}

void Product::useSMA(long iSMALength, bool bDailyUpdate, KOEpochTime cUpdateTime, int iUpdateFreqSeconds, KOEpochTime cSMAStartTime, KOEpochTime cSMAEndTime)
{
	_bUseSMA = true;
	_iSMALength = iSMALength;
	_bSMADailyUpdate = bDailyUpdate;
	_cSMAUpdateTime = cUpdateTime;
	_iSMAUpdateFreqSeconds = iUpdateFreqSeconds;
    _cSMAStartTime = cSMAStartTime;
    _cSMAEndTime = cSMAEndTime;
	_pSMA = new SMA(_iSMALength);
}

void Product::useStdev(long iStdevLength, bool bDailyUpdate, KOEpochTime cUpdateTime, int iUpdateFreqSeconds, KOEpochTime cStdevStartTime, KOEpochTime cStdevEndTime)
{
	_bUseStdev = true;
	_iStdevLength = iStdevLength;
	_bStdevDailyUpdate = bDailyUpdate;
	_cStdevUpdateTime = cUpdateTime;
	_iStdevUpdateFreqSeconds = iUpdateFreqSeconds;
    _cStdevStartTime = cStdevStartTime;
    _cStdevEndTime = cStdevEndTime;
	_pStdev = new Stdev(_iStdevLength);
}

bool Product::bgetEXMAValid()
{
    bool bResult = false;

    if(_pEXMA != NULL)
    {
        bResult = _pEXMA->bgetEXMAValid();
    }
    
	return bResult;
}

double Product::dgetEXMA()
{
    double dResult = 0.0;

    if(_pEXMA != NULL)
    {
        dResult = _pEXMA->dgetEXMA();
    }

	return dResult;
}

long Product::igetEXMANumDataPoints()
{
    double iResult = 0;

    if(_pEXMA != NULL)
    {
        iResult = _pEXMA->igetNumDataPoints();
    }

    return iResult;
}

void Product::setNewEXMA(double dNewEXMA, long iNumDataPoints)
{
    if(_pEXMA != NULL)
    {
    	_pEXMA->setNewEXMA(dNewEXMA, iNumDataPoints);
    }
}

void Product::applyEXMAAdjustment(double dNewAdjustment)
{
    if(_pEXMA != NULL)
    {
    	_pEXMA->applyAdjustment(dNewAdjustment);
    }
}

bool Product::bgetSMAValid()
{
    bool bResult = false;

    if(_pSMA != NULL)
    {
        bResult = _pSMA->bgetSMAValid();
    }

	return bResult;
}

double Product::dgetSMA()
{
    double dResult = 0.0;

    if(_pSMA != NULL)
    {
        dResult = _pSMA->dgetSMA();
    }

	return dResult;
}

void Product::dumpSMA(ostream& os)
{
    if(_pSMA != NULL)
    {
    	_pSMA->dumpSMA(os);
    }
}

void Product::applySMAAdjustment(double dNewAdjustment)
{
    if(_pSMA != NULL)
    {
    	_pSMA->applyAdjustment(dNewAdjustment);
    }
}

void Product::addNewDataToSMA(double dNewData)
{
    if(_pSMA != NULL)
    {
    	_pSMA->dnewData(dNewData);
    }
}

bool Product::bgetWeightedStdevValid()
{
    bool bResult = false;

    if(_pWeightedStdev != NULL)
    {
        bResult = _pWeightedStdev->bgetStdevValid();
    }

	return bResult;
}

double Product::dgetWeightedStdev()
{
    double dResult = 0.0;

    if(_pWeightedStdev != NULL)
    {
        dResult = _pWeightedStdev->dgetWeightedStdev();
    }

	return dResult;
}

void Product::applyWeightedStdevAdjustment(double dNewAdjustment)
{
    if(_pWeightedStdev != NULL)
    {
    	_pWeightedStdev->applyAdjustment(dNewAdjustment);
    }
}

double Product::dgetWeightedStdevEXMA()
{
    double dResult = 0.0;

    if(_pWeightedStdev != NULL)
    {
        dResult = _pWeightedStdev->dgetEXMA();
    }

	return dResult;
}

double Product::dgetWeightedStdevSqrdEXMA()
{
    double dResult = 0.0;

    if(_pWeightedStdev != NULL)
    {
        dResult = _pWeightedStdev->dgetSqrdEXMA();
    }

	return dResult;
}

long Product::igetWeightedStdevNumDataPoints()
{
    long iResult = 0;

    if(_pWeightedStdev != NULL)
    {
        iResult = _pWeightedStdev->igetNumDataPoint();
    }

    return iResult;
}

double Product::dgetWeightedStdevAdjustment()
{
    double dResult = 0.0;

    if(_pWeightedStdev != NULL)
    {
        dResult = _pWeightedStdev->dgetAdjustment();
    }

	return dResult;
}

void Product::dsetNewWeightedStdevEXMA(double dNewSprdEXMA, double dNewEXMA, long iNumDataPoints)
{
    if(_pWeightedStdev != NULL)
    {
    	_pWeightedStdev->dsetNewEXMA(dNewSprdEXMA, dNewEXMA, iNumDataPoints);
    }
}

bool Product::bgetStdevValid()
{	
    bool bResult = false;

    if(_pStdev != NULL)
    {
        bResult = _pStdev->bgetStdevValid();
    }

	return bResult;
}

double Product::dgetStdev()
{
    double dResult = 0.0;

    if(_pStdev != NULL)
    {
        dResult = _pStdev->dgetStdev();
    }

	return dResult;
}

void Product::applyStdevAdjustment(double dNewAdjustment)
{
    if(_pStdev != NULL)
    {
    	_pStdev->applyAdjustment(dNewAdjustment);
    }
}

void Product::addNewDataToStdev(double dNewData)
{
    if(_pStdev != NULL)
    {
    	_pStdev->dnewData(dNewData);
    }
}

void Product::dumpStdev(ostream& os)
{
    if(_pStdev != NULL)
    {
    	_pStdev->dumpStdev(os);
    }
}

double Product::dgetStdevAdjustment()
{
    double dResult = 0.0;

    if(_pStdev != NULL)
    {
        dResult = _pStdev->dgetAdjustment();
    }

	return dResult;
}

void Product::wakeup(KOEpochTime cT)
{
	if(_bUseEXMA)
	{
		if(_bEXMADailyUpdate)
		{
            if(cT == _cEXMAUpdateTime)
			{
				_pEXMA->dnewData(dgetWeightedMid());
			}
		}
		else
		{
            if(_cEXMAStartTime <= cT && cT <= _cEXMAEndTime)
            {
    			int iTimeInNumSeconds = cT.sec();
	    		int iModTimeInSeconds = iTimeInNumSeconds % _iEXMAUpdateFreqSeconds;
		    	if(iModTimeInSeconds == 0)
    			{
	    			_pEXMA->dnewData(dgetWeightedMid());
		    	}
            }
		}
	}

	if(_bUseSMA)
	{
		if(_bSMADailyUpdate)
		{
            if(cT == _cSMAUpdateTime)
			{
				_pSMA->dnewData(dgetWeightedMid());
			}
		}
		else
		{
            if(_cSMAStartTime <= cT && cT <= _cSMAEndTime)
            {
    			int iTimeInNumSeconds = cT.sec();
	    		int iModTimeInSeconds = iTimeInNumSeconds % _iSMAUpdateFreqSeconds;
		    	if(iModTimeInSeconds == 0)
    			{
	    			_pSMA->dnewData(dgetWeightedMid());
		    	}
            }
		}
	}

	if(_bUseWeightedStdev)
	{
		if(_bWeightedStdevDailyUpdate)
		{
            if(cT == _cWeightedStdevUpdateTime)
			{
				_pWeightedStdev->dnewData(dgetWeightedMid());
			}
		}
		else
		{
            if(_cWeightedStdevStartTime <= cT && cT <= _cWeightedStdevEndTime)
            {
    			int iTimeInNumSeconds = cT.sec();
	    		int iModTimeInSeconds = iTimeInNumSeconds % _iWeightedStdevUpdateFreqSeconds;
		    	if(iModTimeInSeconds == 0)
    			{
	    			_pWeightedStdev->dnewData(dgetWeightedMid());
                }
            }
		}
	}

	if(_bUseStdev)
	{
		if(_bStdevDailyUpdate)
		{
            if(cT == _cStdevUpdateTime)
			{
				_pStdev->dnewData(dgetWeightedMid());
			}
		}
		else
		{
            if(_cStdevStartTime <= cT && cT <= _cStdevEndTime)
            {
    			int iTimeInNumSeconds = cT.sec();
	    		int iModTimeInSeconds = iTimeInNumSeconds % _iStdevUpdateFreqSeconds;
		    	if(iModTimeInSeconds == 0)
    			{
	    			_pStdev->dnewData(dgetWeightedMid());
    			}
            }
		}
	}
}

}
