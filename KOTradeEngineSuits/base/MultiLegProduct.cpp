#include "MultiLegProduct.h"
#include "Instrument.h"

namespace KO
{
MultiLegProduct:: MultiLegProduct(const string& sProductName, vector<Instrument*> vIndicators, vector<double> vIndicatorWeights)
{
	_sProductName = sProductName;
    _vIndicators = vIndicators;
    _vIndicatorWeights = vIndicatorWeights;
}

MultiLegProduct::~MultiLegProduct()
{

}

double MultiLegProduct::dgetWeightedMid()
{
    double dResult = 0;
    for(unsigned int i = 0; i < _vIndicators.size(); i++)
    {
        double dIndicatorPrice = (_vIndicators[i]->dgetWeightedMid() - _vIndicators[i]->dgetEXMA()) / _vIndicators[i]->dgetWeightedStdev();
        dResult = dResult + _vIndicatorWeights[i] * dIndicatorPrice;
    }

    return dResult;
}

double MultiLegProduct::dgetBid()
{
    return 0;
}

double MultiLegProduct::dgetAsk()
{
    return 0;
}

bool MultiLegProduct::bgetPriceValid()
{
    bool bResult = true;

    for(unsigned int i = 0; i < _vIndicators.size(); i++)
    {
        bResult = bResult && _vIndicators[i]->bgetPriceValid();
    }

    return bResult;
}

void MultiLegProduct::wakeup(KOEpochTime cT)
{
	Product::wakeup(cT);
}

}
