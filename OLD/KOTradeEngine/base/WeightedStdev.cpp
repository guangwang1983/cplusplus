#include <iostream>
#include <math.h>
#include "WeightedStdev.h"

using namespace std;

namespace KO
{
WeightedStdev::WeightedStdev(long iSize)
:_iSize(iSize),
 _dAdjustment(0),
 _bStdevValid(false)
{
	_pSqrdAvg.reset(new EXMA(_iSize));
	_pDataAvg.reset(new EXMA(_iSize));
}

double WeightedStdev::dnewData(double dNewData)
{
	dNewData = dNewData - _dAdjustment;

	_pSqrdAvg->dnewData(dNewData * dNewData);
	_pDataAvg->dnewData(dNewData);
	int iNumOfDataPoints = _pSqrdAvg->igetNumDataPoints();

	if(iNumOfDataPoints > 1)
	{
		double dBiasFactor = (double)iNumOfDataPoints / ((double)iNumOfDataPoints - 1.0);
		_dWeightedStdev = sqrt((_pSqrdAvg->dgetEXMA() - (_pDataAvg->dgetEXMA() * _pDataAvg->dgetEXMA())) * dBiasFactor);
	}
	else
	{
		_dWeightedStdev = 0;
	}

	_bStdevValid = _pSqrdAvg->bgetEXMAValid() && _pDataAvg->bgetEXMAValid();

	return _dWeightedStdev;
}

double WeightedStdev::dsetNewEXMA(double dNewSqrdEXMA, double dNewEXMA, long iNumDataPoints)
{
	_pSqrdAvg->setNewEXMA(dNewSqrdEXMA, iNumDataPoints);
	_pDataAvg->setNewEXMA(dNewEXMA, iNumDataPoints);
	int iNumOfDataPoints = _pSqrdAvg->igetNumDataPoints();

	if(iNumOfDataPoints > 1)
	{
		double dBiasFactor = (double)iNumOfDataPoints / ((double)iNumOfDataPoints - 1.0);
		_dWeightedStdev = sqrt((_pSqrdAvg->dgetEXMA() - (_pDataAvg->dgetEXMA() * _pDataAvg->dgetEXMA())) * dBiasFactor);
	}
	else
	{
		_dWeightedStdev = 0;
	}

	_bStdevValid = _pSqrdAvg->bgetEXMAValid() && _pDataAvg->bgetEXMAValid();
	return _dWeightedStdev;
}

double WeightedStdev::dgetWeightedStdev()
{
	return _dWeightedStdev;
}

void WeightedStdev::applyAdjustment(double dNewAdjustment)
{
	_dAdjustment = _dAdjustment + dNewAdjustment;
}

bool WeightedStdev::bgetStdevValid()
{
	return _bStdevValid;
}

void WeightedStdev::clear()
{
	_pSqrdAvg->clear();
	_pDataAvg->clear();
}

double WeightedStdev::dgetEXMA()
{
	return _pDataAvg->dgetEXMA();
}

double WeightedStdev::dgetSqrdEXMA()
{
	return _pSqrdAvg->dgetEXMA();
}

long WeightedStdev::igetNumDataPoint()
{
    return _pSqrdAvg->igetNumDataPoints();
}

double WeightedStdev::dgetAdjustment()
{
	return _dAdjustment;
}

}
