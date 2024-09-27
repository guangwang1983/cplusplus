#include <iostream>
#include <math.h>
#include "Stdev.h"

using namespace std;

namespace KO
{

Stdev::Stdev(long iSize)
:_iSize(iSize),
 _dAdjustment(0),
 _bStdevValid(false),
 _dStdev(0)
{
	_pSqrdAvg.reset(new SMA(_iSize));
	_pDataAvg.reset(new SMA(_iSize));
}

double Stdev::dnewData(double dNewData)
{
	dNewData = dNewData - _dAdjustment;

	_pSqrdAvg->dnewData(dNewData * dNewData);
	_pDataAvg->dnewData(dNewData);
	int iNumOfDataPoints = _pSqrdAvg->igetNumDataPoints();

	if(iNumOfDataPoints > 1)
	{
		double dBiasFactor = (double)iNumOfDataPoints / ((double)iNumOfDataPoints - 1.0);
		_dStdev = sqrt((_pSqrdAvg->dgetSMA() - (_pDataAvg->dgetSMA() * _pDataAvg->dgetSMA())) * dBiasFactor);
	}
	else
	{
		_dStdev = 0;
	}

	_bStdevValid = _pSqrdAvg->bgetSMAValid() && _pDataAvg->bgetSMAValid();

	return _dStdev;
}

double Stdev::dgetStdev()
{
	return _dStdev;
}

void Stdev::applyAdjustment(double dNewAdjustment)
{
	_dAdjustment = _dAdjustment + dNewAdjustment;
}

bool Stdev::bgetStdevValid()
{
	return _bStdevValid;
}

void Stdev::clear()
{
	_pSqrdAvg->clear();
	_pDataAvg->clear();
}

double Stdev::dgetAdjustment()
{
	return _dAdjustment;
}

void Stdev::dumpStdev(ostream& os)
{
	_pDataAvg->dumpSMA(os);
}

}
