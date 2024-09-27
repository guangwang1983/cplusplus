#ifndef HedgeOrder_H
#define HedgeOrder_H

#include "../EngineInfra/KOOrder.h"
#include "../EngineInfra/ContractAccount.h"
#include "../EngineInfra/SimpleLogger.h"

#include "Instrument.h"

namespace KO
{

class HedgeOrder
{
public:
    HedgeOrder(boost::shared_ptr<ContractAccount> pContractAccount, Instrument* pInstrument, SimpleLogger* pLogger);

    bool bsubmitOrder(long iQty, long iEntryOffSet, long iStopDistance, long iMaxStopDistance);
	bool bchangeHedgeOrderPrice(long iEntryOffSet, long iStopDistance, long iMaxStopDistance);
	bool bchangeHedgeOrderSize(long iNewSize);
	bool bchangeHedgeOrder(long iEntryOffSet, long iStopDistance, long iMaxStopDistance, long iNewSize);
    bool borderCanBeChanged();
    bool bdeleteOrder();
    long igetOrderID();
    long igetOrderRemainQty();
    long igetOrderPrice();
    long igetOrderStopPrice();
    long igetOrderMaxStopPrice();
	void checkOrderStop();

private:
    bool bchangeOrderPrice(long iNewPrice);
    bool bchangeOrder(long iNewPrice, long iNewSize);
    long iCalculateMarketStop(bool bResetEntryPrice);

    boost::shared_ptr<ContractAccount> _pContractAccount;
	Instrument* _pInstrument;

    KOOrderPtr _pBasicOrder;
    long _iOrderEntryPrice;
	long _iMaxStopPrice;
	long _iStopPrice;
    long _iOrderEntryQty;

    SimpleLogger* _pLogger;
};

}

#endif /* HedgeOrder_H */
