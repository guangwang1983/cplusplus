#ifndef EXMA_H
#define EXMA_H

namespace KO
{

class EXMA
{
public:
	EXMA(long iSize);
	EXMA(long iSize, double dDecay);
	bool bgetEXMAValid();
	bool bgetValidity();
	long igetNumDataPoints();
	double dnewData(double dNewData);
	void setNewEXMA(double dNewEXMA, long iNumDataPoints);
	double dgetEXMA();
	void clear();
	void applyAdjustment(double dNewAdjustment);

private:
	long _iSize;
	bool _bEXMAValid;
	double _dDecay;

	long _iDataPointsStored;

	double _dEXMA;
};

}

#endif /* EXMA_H */
