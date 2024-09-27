#include "SMA.h"

using namespace std;

namespace KO
{
SMA::SMA(unsigned long iSize)
:_iSize(iSize),
 _bSMAValid(false),
 _dSum(0.0),
 _dSMA(0.0)
{

}

long SMA::igetNumDataPoints()
{
	return _cDataQueue.size();
}

bool SMA::bgetSMAValid()
{
	return _bSMAValid;
}

double SMA::dgetSMA()
{
	return _dSMA;
}

void SMA::clear()
{
	_cDataQueue.clear();

	_bSMAValid = false;
	
	_dSum = 0;

	_dSMA = 0;
}

double SMA::dnewData(double dNewData)
{
	_cDataQueue.push_back(dNewData);
	_dSum = _dSum + dNewData;

	if(_cDataQueue.size() > _iSize)
	{
		double dRemovedData = _cDataQueue[0];
		_dSum = _dSum - dRemovedData;
		_cDataQueue.pop_front();
	}

    if(_cDataQueue.size() != 0)
    {
    	_dSMA = _dSum / (double)_cDataQueue.size();
    }
    else
    {
        _dSMA = 0;
    }

	_bSMAValid = (_cDataQueue.size() == _iSize);

	return _dSMA;
}

void SMA::dumpSMA(ostream& os)
{
	for(deque<double>::iterator itr = _cDataQueue.begin(); itr != _cDataQueue.end(); itr++)
	{
		os << *itr << "\n";
	}
}

void SMA::applyAdjustment(double dNewAdjustment)
{
	_dSum = 0;

	for(unsigned long i = 0; i < _cDataQueue.size(); ++i)
	{
		_cDataQueue[i] = _cDataQueue[i] + dNewAdjustment;
		_dSum = _dSum + _cDataQueue[i];
	}

    if(_cDataQueue.size() != 0)
    {
    	_dSMA = _dSum / (double)_cDataQueue.size();
    }
    else
    {
        _dSMA = 0;
    }
}

};
