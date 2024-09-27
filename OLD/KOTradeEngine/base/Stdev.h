#ifndef Stdev_H
#define Stdev_H

#include "SMA.h"
#include "boost/shared_ptr.hpp"

using std::ostream;

namespace KO
{

class Stdev
{
public:
	typedef boost::shared_ptr<SMA> SMAPtr;

	Stdev(long iSize);

	double dnewData(double dNewData);

	double dgetStdev();
	void applyAdjustment(double dNewAdjustment);

	bool bgetStdevValid();

	void dumpStdev(ostream& os);
	double dgetAdjustment();

	void clear();
private:
	long _iSize;
	double _dAdjustment;
	bool _bStdevValid;
	double _dStdev;

	SMAPtr _pSqrdAvg;
	SMAPtr _pDataAvg;
};

}

#endif /* Stdev_H */
