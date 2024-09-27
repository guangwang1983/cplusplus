#ifndef INSTRUMENT_H
#define INSTRUMENT_H

#include "../EngineInfra/KOEpochTime.h"
#include "../EngineInfra/QuoteData.h"
#include "Product.h"

namespace KO
{

class Instrument : public Product
{
public:
	Instrument(const string& sProductName, int iCID, InstrumentType eInstrumentType, double dTickSize, long iMaxSpreadWidth, bool bUseRealSignalPrice);
	~Instrument();

	virtual double dgetWeightedMid();
    virtual double dgetBid();
    virtual double dgetAsk();
	virtual bool bgetPriceValid();

    InstrumentType egetInstrumentType();

	long igetBidSize();
	long igetBestBid();
	long igetBestAsk();
	long igetAskSize();

    double igetRealBestBid();
    double igetRealBestAsk();

    long igetLastTrade();
    long igetAccumuTradeSize();

	bool bgetBidTradingOut();
	bool bgetAskTradingOut();

    double dgetTickSize();

    int igetCID();

    const string& sgetProductName();

	void useTradingOut(double dWeightedMidTradingOutThresh);

	void newMarketUpdate(QuoteData* pNewMarketUpdate);

	void eodReset();

	virtual void wakeup(KOEpochTime cT);
private:
	void updatePriceValid();
	void updateWeightedMid();
	void updateTradingOut();

    InstrumentType _eInstrumentType;

	double _dTickSize;
    int _iCID;

	long _iBidSize;
	long _iBestBid;
	long _iBestAsk;
	long _iAskSize;

	bool _bPriceValid;

	double _dWeightedMid;

	bool _bCheckTradingOut;

	bool _bBidTradingOut;
	bool _bAskTradingOut;

    bool _bUseRealSignalPrice;

    long _iMaxSpreadWidth;

    double _dRealBestBid;
    double _dRealBestAsk;

    long _iBidAskSpread;

    long _iLastTrade;
    long _iAccumuTradeSize;


	double _dWeightedMidTradingOutThresh;
};

}

#endif /* INSTRUMENT_H */
