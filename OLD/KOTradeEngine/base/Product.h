#ifndef PRODUCT_H
#define PRODUCT_H

#include "../EngineInfra/KOEpochTime.h"
#include "WeightedStdev.h"
#include "EXMA.h"
#include "Stdev.h"
#include "SMA.h"

using std::ostream;
using std::string;

namespace KO
{

class Product
{
public:

    enum ProductType
    {
        INSTRUMENT,
        SYTHETIC_SPREAD,
        NONE
    };

	Product();
	~Product();

	string sgetProductName();
    ProductType egetProductType();

	virtual double dgetWeightedMid() = 0;
	virtual double dgetBid() = 0;
	virtual double dgetAsk() = 0;
	virtual bool bgetPriceValid() = 0;

	void useEXMA(long iEXMALength, bool bDailyUpdate, KOEpochTime cUpdateTime, int iUpdateFreqSeconds, KOEpochTime cEXMAStartTime, KOEpochTime cEXMAEndTime);
	void useWeightedStdev(long iStdevLength, bool bDailyUpdate, KOEpochTime cUpdateTime, int iUpdateFreqSeconds, KOEpochTime cWeightedStdevStartTime, KOEpochTime cWeightedStdevEndTime);

	bool bgetEXMAValid();
	double dgetEXMA();
    long igetEXMANumDataPoints();
	void setNewEXMA(double dNewEXMA, long iNumDataPoints);
	void applyEXMAAdjustment(double dNewAdjustment);

	bool bgetWeightedStdevValid();
	double dgetWeightedStdev();
	void applyWeightedStdevAdjustment(double dNewAdjustment);
	double dgetWeightedStdevEXMA();
	double dgetWeightedStdevSqrdEXMA();
    long igetWeightedStdevNumDataPoints();
	double dgetWeightedStdevAdjustment();	
	void dsetNewWeightedStdevEXMA(double dNewSprdEXMA, double dNewEXMA, long iNumDataPoints);

	void useSMA(long iSMALength, bool bDailyUpdate, KOEpochTime cUpdateTime, int iUpdateFreqSeconds, KOEpochTime cSMAStartTime, KOEpochTime cSMAEndTime);
	void useStdev(long iStdevLength, bool bDailyUpdate, KOEpochTime cUpdateTime, int iUpdateFreqSeconds, KOEpochTime cStdevStartTime, KOEpochTime cStdevEndTime);

	bool bgetSMAValid();
	double dgetSMA();
	void dumpSMA(ostream& os);
	void applySMAAdjustment(double dNewAdjustment);
	void addNewDataToSMA(double dNewData);

	bool bgetStdevValid();
	double dgetStdev();
	void applyStdevAdjustment(double dNewAdjustment);
	void addNewDataToStdev(double dNewData);
	void dumpStdev(ostream& os);
	double dgetStdevAdjustment();	

	virtual void wakeup(KOEpochTime cT);
protected:
	string _sProductName;
    ProductType _eProductType;

	bool _bUseEXMA;
	int _iEXMALength;
	bool _bEXMADailyUpdate;
	KOEpochTime _cEXMAUpdateTime;
	int _iEXMAUpdateFreqSeconds;
	EXMA* _pEXMA;
    KOEpochTime _cEXMAStartTime;
    KOEpochTime _cEXMAEndTime;

	bool _bUseWeightedStdev;
	int _iWeightedStdevLength;
	bool _bWeightedStdevDailyUpdate;
	KOEpochTime _cWeightedStdevUpdateTime;
	int _iWeightedStdevUpdateFreqSeconds;
    WeightedStdev* _pWeightedStdev;
    KOEpochTime _cWeightedStdevStartTime;
    KOEpochTime _cWeightedStdevEndTime;

	bool _bUseSMA;
	int _iSMALength;
	bool _bSMADailyUpdate;
	KOEpochTime _cSMAUpdateTime;
	int _iSMAUpdateFreqSeconds;
	SMA* _pSMA;
    KOEpochTime _cSMAStartTime;
    KOEpochTime _cSMAEndTime;

	bool _bUseStdev;
	int _iStdevLength;
	bool _bStdevDailyUpdate;
	KOEpochTime _cStdevUpdateTime;
	int _iStdevUpdateFreqSeconds;
	Stdev* _pStdev;
    KOEpochTime _cStdevStartTime;
    KOEpochTime _cStdevEndTime;
};

}

#endif /* PRODUCT_H */
