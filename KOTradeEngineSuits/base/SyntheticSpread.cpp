#include "SyntheticSpread.h"
#include "Instrument.h"

namespace KO
{
SyntheticSpread::SyntheticSpread(const string& sProductName, Product* pFrontLeg, LegPriceType eFrontLegPriceType, Product* pBackLeg, LegPriceType eBackLegPriceType, double dBackLegWeight, bool bPositiveCorrelation, bool bUseRealSignalPrice)
:_pFrontLeg(pFrontLeg),
 _eFrontLegPriceType(eFrontLegPriceType),
 _pBackLeg(pBackLeg),
 _eBackLegPriceType(eBackLegPriceType),
 _dBackLegWeight(dBackLegWeight),
 _bPositiveCorrelation(bPositiveCorrelation),
 _bUseRealSignalPrice(bUseRealSignalPrice)
{
	_sProductName = sProductName;
    _eProductType = Product::SYTHETIC_SPREAD;
}

SyntheticSpread::~SyntheticSpread()
{

}

long SyntheticSpread::igetASC()
{
    // case1 = Target Dominate, Same Direction
    // case2 = Leader Dominate, Same Direction
    // case3 = Target Dominate, Opposite Direction
    // case4 = Leader Dominate, Opposite Direction

    long iResult = 0;

    double dFrontLegPrice = dgetFrontLegPrice();
    double dBackLegPrice = dgetBackLegPrice();

    dBackLegPrice = dBackLegPrice * _dBackLegWeight;

    if(_bPositiveCorrelation)
    {
        if( (dFrontLegPrice > 0 && dBackLegPrice > 0) ||
            (dFrontLegPrice < 0 && dBackLegPrice < 0) )
        {
            if(fabs(dFrontLegPrice) > fabs(dBackLegPrice))
            {
                iResult = 1;
            }
            else
            {
                iResult = 2;
            }
        }
        else
        {
            if(fabs(dFrontLegPrice) > fabs(dBackLegPrice))
            {
                iResult = 3;
            }
            else
            {
                iResult = 4;
            }
        }
    }
    else
    {
        if( (dFrontLegPrice > 0 && dBackLegPrice > 0) ||
            (dFrontLegPrice < 0 && dBackLegPrice < 0) )
        {
            if(fabs(dFrontLegPrice) > fabs(dBackLegPrice))
            {
                iResult = 3;
            }
            else
            {
                iResult = 4;
            }
        }
        else
        {
            if(fabs(dFrontLegPrice) > fabs(dBackLegPrice))
            {
                iResult = 1;
            }
            else
            {
                iResult = 2;
            }
        }
    }

    return iResult;
}

double SyntheticSpread::dgetFrontLegPrice()
{
    double dFrontPrice = _pFrontLeg->dgetWeightedMid();
    double dResult = 0;

    if(_eFrontLegPriceType == WhiteAbsNoDetrend)
    {
        dResult = dFrontPrice / _pFrontLeg->dgetWeightedStdev();
    }
    else if(_eFrontLegPriceType == WhiteAbs)
    {
        dResult = (dFrontPrice - _pFrontLeg->dgetEXMA()) / _pFrontLeg->dgetWeightedStdev();
    }
    else if(_eBackLegPriceType == Simple)
    {
        dResult = dFrontPrice;
    }

    return dResult;
}

double SyntheticSpread::dgetBackLegPrice()
{
    double dBackPrice = _pBackLeg->dgetWeightedMid();
    double dResult = 0;

    if(_eBackLegPriceType == WhiteAbsNoDetrend)
    {
        dResult = dBackPrice / _pBackLeg->dgetWeightedStdev();
    }
    else if(_eBackLegPriceType == WhiteAbs)
    {
        dResult = (dBackPrice - _pBackLeg->dgetEXMA()) / _pBackLeg->dgetWeightedStdev();
    }
    else if(_eBackLegPriceType == Simple)
    {
        dResult = dBackPrice;
    }

    return dResult;
}

double SyntheticSpread::dgetWeightedMid()
{
    return dgetPriceFromFrontLegPrice(_pFrontLeg->dgetWeightedMid());
}

double SyntheticSpread::dgetBid()
{
    double dFrontLegPrice = _pFrontLeg->dgetBid();

    if(!_bUseRealSignalPrice)
    {
        if(_pFrontLeg->egetProductType() == Product::INSTRUMENT)
        {
            Instrument* pFrontLegInstrument = static_cast<Instrument*>(_pFrontLeg);

            if(pFrontLegInstrument->sgetProductName().substr(0,2).compare("6E") == 0)
            {
                if(pFrontLegInstrument->dgetTickSize() < 0.0001 - 0.000000002)
                {
                    dFrontLegPrice = dFrontLegPrice / 2;
                }
            }
     
            if(pFrontLegInstrument->sgetProductName().substr(0,1).compare("L") == 0)
            {
                if(pFrontLegInstrument->dgetTickSize() < 0.01 - 0.002)
                {
                    dFrontLegPrice = dFrontLegPrice / 2;
                }
            }
        }
    }

    return dgetPriceFromFrontLegPrice(dFrontLegPrice);
}

double SyntheticSpread::dgetAsk()
{
    double dFrontLegPrice = _pFrontLeg->dgetAsk();

    if(!_bUseRealSignalPrice)
    {
        if(_pFrontLeg->egetProductType() == Product::INSTRUMENT)
        {
            Instrument* pFrontLegInstrument = static_cast<Instrument*>(_pFrontLeg);

            if(pFrontLegInstrument->sgetProductName().substr(0,2).compare("6E") == 0)
            {
                if(pFrontLegInstrument->dgetTickSize() < 0.0001 - 0.000000002)
                {
                    dFrontLegPrice = dFrontLegPrice / 2;
                }
            } 
             
            if(pFrontLegInstrument->sgetProductName().substr(0,1).compare("L") == 0)
            {
                if(pFrontLegInstrument->dgetTickSize() < 0.01 - 0.002)
                {
                    dFrontLegPrice = dFrontLegPrice / 2;
                }
            }
        }
    }

    return dgetPriceFromFrontLegPrice(dFrontLegPrice);
}

double SyntheticSpread::dgetPriceFromFrontLegPrice(double dFrontPrice)
{
    double dFrontLegPrice;
    double dBackLegPrice;

    if(_eFrontLegPriceType == WhiteAbsNoDetrend)
    {
        dFrontLegPrice = dFrontPrice / _pFrontLeg->dgetWeightedStdev();
    }
    else if(_eFrontLegPriceType == WhiteAbs)
    {
        dFrontLegPrice = (dFrontPrice - _pFrontLeg->dgetEXMA()) / _pFrontLeg->dgetWeightedStdev();
    }
    else if(_eFrontLegPriceType == Simple)
    {
        dFrontLegPrice = dFrontPrice;
    }

    if(_eBackLegPriceType == WhiteAbsNoDetrend)
    {
        dBackLegPrice = _pBackLeg->dgetWeightedMid() / _pBackLeg->dgetWeightedStdev();
    }
    else if(_eBackLegPriceType == WhiteAbs)
    {
        dBackLegPrice = (_pBackLeg->dgetWeightedMid() - _pBackLeg->dgetEXMA()) / _pBackLeg->dgetWeightedStdev();
    }
    else if(_eBackLegPriceType == Simple)
    {
        dBackLegPrice = _pBackLeg->dgetWeightedMid();
    }

    dBackLegPrice = dBackLegPrice * _dBackLegWeight;

    double dSpread;

    if(_bPositiveCorrelation)
    {
        dSpread = dFrontLegPrice - dBackLegPrice;
    }
    else
    {
        dSpread = dFrontLegPrice + dBackLegPrice;
    }

    return dSpread;
}

bool SyntheticSpread::bgetPriceValid()
{
	return(_pFrontLeg->bgetPriceValid() && _pBackLeg->bgetPriceValid());
}

void SyntheticSpread::wakeup(KOEpochTime cT)
{
	Product::wakeup(cT);
}

double SyntheticSpread::dgetFrontQuoteBid(double dOffsetStdev)
{
	double dBackLegPrice;

	if(_eBackLegPriceType == WhiteAbsNoDetrend)
	{
		dBackLegPrice = _pBackLeg->dgetWeightedMid() / _pBackLeg->dgetWeightedStdev();
	}
	else if(_eBackLegPriceType == WhiteAbs)
	{
		dBackLegPrice = (_pBackLeg->dgetWeightedMid() - _pBackLeg->dgetEXMA()) / _pBackLeg->dgetWeightedStdev();
	}
    else if(_eBackLegPriceType == Simple)
    {
        dBackLegPrice = _pBackLeg->dgetWeightedMid();
    }

	if(!_bPositiveCorrelation)
	{
		dBackLegPrice = -1 * dBackLegPrice;
	}

    dBackLegPrice = dBackLegPrice * _dBackLegWeight;

	double dQuoteBid = 0;

	if(_eFrontLegPriceType == WhiteAbsNoDetrend)
	{
		dQuoteBid = (dBackLegPrice - dOffsetStdev * _pWeightedStdev->dgetWeightedStdev()) * _pFrontLeg->dgetWeightedStdev();
	}
	else if(_eFrontLegPriceType == WhiteAbs)
	{
		dQuoteBid = (dBackLegPrice - dOffsetStdev * _pWeightedStdev->dgetWeightedStdev()) * _pFrontLeg->dgetWeightedStdev() + _pFrontLeg->dgetEXMA();
	}
    else if(_eFrontLegPriceType == Simple)
    {
        dQuoteBid = dBackLegPrice - dOffsetStdev;
    }

    if(!_bUseRealSignalPrice)
    {
        if(_pFrontLeg->egetProductType() == Product::INSTRUMENT)
        {
            Instrument* pQuoteInstrument = static_cast<Instrument*>(_pFrontLeg);

            if(pQuoteInstrument->sgetProductName().substr(0,2).compare("6E") == 0)
            {
                if(pQuoteInstrument->dgetTickSize() < 0.0001 - 0.000000002)
                {
                    dQuoteBid = dQuoteBid * 2;
                }
            }
         
            if(pQuoteInstrument->sgetProductName().substr(0,1).compare("L") == 0)
            {
                if(pQuoteInstrument->dgetTickSize() < 0.01 - 0.002)
                {
                    dQuoteBid = dQuoteBid * 2;
                }
            }
        }
    }

	return dQuoteBid;
}

double SyntheticSpread::dgetFrontQuoteOffer(double dOffsetStdev)
{
	double dBackLegPrice;

	if(_eBackLegPriceType == WhiteAbsNoDetrend)
	{
		dBackLegPrice = _pBackLeg->dgetWeightedMid() / _pBackLeg->dgetWeightedStdev();
	}
	else if(_eBackLegPriceType == WhiteAbs)
	{
		dBackLegPrice = (_pBackLeg->dgetWeightedMid() - _pBackLeg->dgetEXMA()) / _pBackLeg->dgetWeightedStdev();
	}
    else if(_eBackLegPriceType == Simple)
    {
        dBackLegPrice = _pBackLeg->dgetWeightedMid();
    }

	if(!_bPositiveCorrelation)
	{
		dBackLegPrice = -1 * dBackLegPrice;
	}

    dBackLegPrice = dBackLegPrice * _dBackLegWeight;

	double dQuoteOffer = 0;

	if(_eFrontLegPriceType == WhiteAbsNoDetrend)
	{
		dQuoteOffer = (dBackLegPrice + dOffsetStdev * _pWeightedStdev->dgetWeightedStdev()) * _pFrontLeg->dgetWeightedStdev();
	}
	else if(_eFrontLegPriceType == WhiteAbs)
	{
		dQuoteOffer = (dBackLegPrice + dOffsetStdev * _pWeightedStdev->dgetWeightedStdev()) * _pFrontLeg->dgetWeightedStdev() + _pFrontLeg->dgetEXMA();
	}
    else if(_eFrontLegPriceType == Simple)
    {
        dQuoteOffer = dBackLegPrice + dOffsetStdev;
    }

    if(!_bUseRealSignalPrice)
    {
        if(_pFrontLeg->egetProductType() == Product::INSTRUMENT)
        {
            Instrument* pQuoteInstrument = static_cast<Instrument*>(_pFrontLeg);

            if(pQuoteInstrument->sgetProductName().substr(0,2).compare("6E") == 0)
            {
                if(pQuoteInstrument->dgetTickSize() < 0.0001 - 0.000000002)
                {
                    dQuoteOffer = dQuoteOffer * 2;
                }
            }

            if(pQuoteInstrument->sgetProductName().substr(0,1).compare("L") == 0)
            {
                if(pQuoteInstrument->dgetTickSize() < 0.01 - 0.002)
                {
                    dQuoteOffer = dQuoteOffer * 2;
                }
            }
        }
    }

	return dQuoteOffer;
}

}
