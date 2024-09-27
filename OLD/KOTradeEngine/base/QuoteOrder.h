#ifndef QuoteOrder_H
#define QuoteOrder_H

#include <boost/shared_ptr.hpp>

#include "../EngineInfra/KOOrder.h"
#include "../EngineInfra/ContractAccount.h"

namespace KO
{

class QuoteOrder
{
public:
	QuoteOrder(boost::shared_ptr<ContractAccount> pContractAccount);

	bool bsubmitQuote(long iQty, long iPrice);
	bool bchangeQuotePrice(long iNewPrice);
	bool bchangeQuote(long iNewPrice, long iNewSize);
	bool bdeleteQuote();
    bool bquoteCanBeChanged();
	long igetOrderID();
	long igetOrderRemainQty();
	long igetOrderPrice();

private:
    boost::shared_ptr<ContractAccount> _pContractAccount;
	KOOrderPtr _pBasicOrder;
};

}

#endif /* QuoteOrder_H */
