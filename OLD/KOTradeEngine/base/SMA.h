#ifndef SMA_H
#define SMA_H

#include <deque>
#include <iostream>
#include <fstream>
#include <stdio.h>

using std::ostream;

namespace KO
{

class SMA
{
public:
	SMA(unsigned long iSize);

	long igetNumDataPoints();
	bool bgetSMAValid();
	double dnewData(double dNewData);
	double dgetSMA();

	void dumpSMA(ostream& os);

	void clear();
	void applyAdjustment(double dNewAdjustment);

private:
	std::deque<double> _cDataQueue;

	unsigned long _iSize;
	bool _bSMAValid;

	double _dSum;
	double _dSMA;
};

}

#endif /* SMA_H */
