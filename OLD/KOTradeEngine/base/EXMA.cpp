#include "EXMA.h"
#include <iostream>

using namespace std;

namespace KO
{
EXMA::EXMA(long iSize)
:_iSize(iSize),
 _bEXMAValid(false),
 _dDecay( 2.0 / ((double)_iSize + 1.0) ),
 _iDataPointsStored(0),
 _dEXMA(0.0)
{
	
}

EXMA::EXMA(long iSize,double dDecay)
:_iSize(iSize),
 _bEXMAValid(false),
 _dDecay(dDecay),
 _iDataPointsStored(0),
 _dEXMA(0.0)
{
	
}
bool EXMA::bgetEXMAValid()
{
	return _bEXMAValid;
}
bool EXMA::bgetValidity()
{
	return _bEXMAValid;
}

long EXMA::igetNumDataPoints()
{
	if(_iDataPointsStored < _iSize)
	{
		return _iDataPointsStored;
	}
	else
	{
		return _iSize;
	}
}

double EXMA::dnewData(double dNewData)
{
	if(_iDataPointsStored == 0)
	{
		_dEXMA = dNewData;
	}
	else
	{
		_dEXMA = _dEXMA * (1.0 - _dDecay) + _dDecay * dNewData;
	}

	_iDataPointsStored = _iDataPointsStored + 1;

	_bEXMAValid = (_iDataPointsStored >= _iSize);

	return _dEXMA;
}

void EXMA::setNewEXMA(double dNewEXMA, long iNumDataPoints)
{
	_dEXMA = dNewEXMA;
	_iDataPointsStored = iNumDataPoints;

	_bEXMAValid = (_iDataPointsStored >= _iSize);
}

double EXMA::dgetEXMA()
{
	return _dEXMA;
}

void EXMA::clear()
{
	_iSize = 0;
	_bEXMAValid = false;
	_dDecay = 0.0;
	_iDataPointsStored = 0;
	_dEXMA = 0;
}

void EXMA::applyAdjustment(double dNewAdjustment)
{
	_dEXMA = _dEXMA + dNewAdjustment;
}

}
