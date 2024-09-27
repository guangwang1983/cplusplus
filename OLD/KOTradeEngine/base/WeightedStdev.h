#ifndef WeightedStdev_H
#define WeightedStdev_H

#include "EXMA.h"
#include "boost/shared_ptr.hpp"

namespace KO
{

class WeightedStdev
{
public:
	typedef boost::shared_ptr<EXMA> EXMAPtr;

	WeightedStdev(long iSize);

	double dnewData(double dNewData);
	double dsetNewEXMA(double dNewSqrdEXMA, double dNewEXMA, long iNumDataPoints);

	double dgetWeightedStdev();
	void applyAdjustment(double dNewAdjustment);

	bool bgetStdevValid();

	void clear();

	double dgetEXMA();
	double dgetSqrdEXMA();
    long igetNumDataPoint();
	double dgetAdjustment();

private:
	long _iSize;
	double _dWeightedStdev;
	double _dAdjustment;

	EXMAPtr _pSqrdAvg;
	EXMAPtr _pDataAvg;

	bool _bStdevValid;

};

}

#endif /* WeightedStdev_H */
