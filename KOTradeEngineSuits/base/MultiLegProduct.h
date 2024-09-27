#ifndef MULTILEGPRODUCT_H
#define MULTILEGPRODUCT_H

#include "../EngineInfra/KOEpochTime.h"
#include "Product.h"
#include "Instrument.h"
#include <vector>

using namespace std;

namespace KO
{

class MultiLegProduct : public Product
{
public:
	MultiLegProduct(const string& sProductName, vector<Instrument*> vIndicators, vector<double> vIndicatorWeights);
	~MultiLegProduct();

	virtual double dgetWeightedMid();
    virtual double dgetBid();
    virtual double dgetAsk();
	virtual bool bgetPriceValid();

	virtual void wakeup(KOEpochTime cT);
private:
    vector<Instrument*>    _vIndicators;
    vector<double>          _vIndicatorWeights;
};

}

#endif /* MULTILEGPRODUCT_H */
