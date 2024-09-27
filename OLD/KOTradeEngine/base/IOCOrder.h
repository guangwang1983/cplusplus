#ifndef IOCOrder_H
#define IOCOrder_H

#include <boost/shared_ptr.hpp>

#include "../EngineInfra/KOOrder.h"
#include "../EngineInfra/ContractAccount.h"

namespace KO
{

class IOCOrder
{
public:
	IOCOrder(boost::shared_ptr<ContractAccount> pContractAccount);

	bool bsubmitOrder(long iQty, long iPrice);
    long igetOrderID();
	long igetOrderRemainQty();
	long igetOrderPrice();

private:
    boost::shared_ptr<ContractAccount> _pContractAccount;
	KOOrderPtr _pBasicOrder;
};

}

#endif /* IOCOrder_H */
