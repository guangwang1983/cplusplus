#include "Instrument.h"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/math/special_functions/round.hpp>

namespace KO
{
Instrument::Instrument(const string& sProductName, int iCID, InstrumentType eInstrumentType, double dTickSize, long iMaxSpreadWidth, bool bUseRealSignalPrice)
:_eInstrumentType(eInstrumentType),
 _dTickSize(dTickSize),
 _iCID(iCID),
 _iBidSize(0),
 _iBestBid(0),
 _iBestAsk(0),
 _iAskSize(0),
 _bPriceValid(false),
 _dWeightedMid(0.0),
 _bCheckTradingOut(false),
 _bBidTradingOut(false),
 _bAskTradingOut(false),
 _bUseRealSignalPrice(bUseRealSignalPrice),
 _iMaxSpreadWidth(iMaxSpreadWidth)
{
	_sProductName = sProductName;
    _eProductType = Product::INSTRUMENT;
	_bUseEXMA = false;
	_bUseWeightedStdev = false;
	_bUseSMA = false;
	_bUseStdev = false;
}

Instrument::~Instrument()
{

}

InstrumentType Instrument::egetInstrumentType()
{
    return _eInstrumentType;
}

void Instrument::eodReset()
{
	_iBidSize = 0;
	_iBestBid = 0;
	_iBestAsk = 0;
	_iAskSize = 0;
	_dWeightedMid = 0.0;
	_bPriceValid = false;
}

long Instrument::igetBidSize()
{
	return _iBidSize;
}

long Instrument::igetBestBid()
{
	return _iBestBid;
}

long Instrument::igetBestAsk()
{
	return _iBestAsk;
}

long Instrument::igetAskSize()
{
	return _iAskSize;
}

double Instrument::igetRealBestBid()
{
    return _dRealBestBid;
}

double Instrument::igetRealBestAsk()
{
    return _dRealBestAsk;
}

long Instrument::igetLastTrade()
{
    return _iLastTrade;
}

long Instrument::igetAccumuTradeSize()
{
    return _iAccumuTradeSize;
}

double Instrument::dgetWeightedMid()
{
	return _dWeightedMid;
}

double Instrument::dgetBid()
{
    if(_bUseRealSignalPrice)
    {
        return igetRealBestBid();
    }
    else
    {
        return igetBestBid();
    }
}

double Instrument::dgetAsk()
{
    if(_bUseRealSignalPrice)
    {
        return igetRealBestAsk();
    }
    else
    {
        return igetBestAsk();
    }
}

bool Instrument::bgetPriceValid()
{
	return _bPriceValid;
}

bool Instrument::bgetBidTradingOut()
{
	return _bBidTradingOut;
}

bool Instrument::bgetAskTradingOut()
{
	return _bAskTradingOut;
}

int Instrument::igetCID()
{
    return _iCID;
}

double Instrument::dgetTickSize()
{
    return _dTickSize;
}

const string& Instrument::sgetProductName()
{
    return _sProductName;
}

void Instrument::useTradingOut(double dWeightedMidTradingOutThresh)
{
	_bCheckTradingOut = true;
	_dWeightedMidTradingOutThresh = dWeightedMidTradingOutThresh;
}

void Instrument::newMarketUpdate(QuoteData* pNewPriceData)
{
	_iBidSize = pNewPriceData->iBidSize;

    _iBestBid = pNewPriceData->iBestBidInTicks;
    _iBestAsk = pNewPriceData->iBestAskInTicks;

    _dRealBestBid = pNewPriceData->dBestBid;
    _dRealBestAsk = pNewPriceData->dBestAsk;

	_iAskSize = pNewPriceData->iAskSize;

    _iBidAskSpread = _iBestAsk - _iBestBid;

    _iLastTrade = pNewPriceData->iLastTradeInTicks;
    _iAccumuTradeSize = pNewPriceData->iAccumuTradeSize;

	updatePriceValid();
	updateWeightedMid();

	if(_bCheckTradingOut)
	{
		updateTradingOut();
	}
}

void Instrument::updatePriceValid()
{
	_bPriceValid = _iBidSize != 0 && _iAskSize != 0 && _iBidAskSpread > 0 && _iBidAskSpread <= _iMaxSpreadWidth;
}

void Instrument::updateWeightedMid()
{
	if(_bPriceValid)
	{
		if(_iBidAskSpread > 1)
		{
			_dWeightedMid = (double)(_iBestAsk + _iBestBid) / 2.0;
		}
		else
		{
			_dWeightedMid = (double)_iBestBid + (double)_iBidSize / (double)(_iBidSize + _iAskSize);
		}

        if(_bUseRealSignalPrice)
        {
            _dWeightedMid = _dWeightedMid * _dTickSize;
        }
        else
        {
            if(_sProductName.substr(0,2).compare("6E") == 0)
            {
                if(_dTickSize < 0.0001 - 0.000000002)
                {
                    _dWeightedMid = _dWeightedMid / 2;
                }
            }

            if(_sProductName.substr(0,1).compare("L") == 0)
            {
                if(_dTickSize < 0.01 - 0.002)
                {
                    _dWeightedMid = _dWeightedMid / 2;
                }
            }
        }
	}
}

void Instrument::updateTradingOut()
{
	if(_iBidAskSpread > 1)
	{
		_bBidTradingOut = false;
		_bAskTradingOut = false;
	}
	else
	{
        double _dWeightedMidInTicks;

        if(_bUseRealSignalPrice)
        {
            _dWeightedMidInTicks = _dWeightedMid / _dTickSize;
        }
        else
        {
            _dWeightedMidInTicks = _dWeightedMid;
        }

		if(_dWeightedMidInTicks - (double)_iBestBid < _dWeightedMidTradingOutThresh)
		{
			_bBidTradingOut = true;
		}
		else
		{
			_bBidTradingOut = false;
		}

		if((double)_iBestAsk - _dWeightedMidInTicks < _dWeightedMidTradingOutThresh)
		{
			_bAskTradingOut = true;
		}
		else
		{
			_bAskTradingOut = false;
		}
	}

}

void Instrument::wakeup(KOEpochTime cT)
{
	Product::wakeup(cT);
}

}
