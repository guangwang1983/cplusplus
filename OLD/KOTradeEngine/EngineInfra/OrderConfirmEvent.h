#ifndef OrderEvent_H
#define OrderEvent_H

#include "TimeEvent.h"
#include "SimulationExchange.h"
#include "KOOrder.h"

namespace KO
{

class OrderConfirmEvent : public TimeEvent
{
public:
	OrderConfirmEvent(SimulationExchange* pSimulationExchange, KOOrderPtr pOrder, KOEpochTime cCallTime);
	~OrderConfirmEvent();

	void makeCall();
private:
	SimulationExchange* _pSimulationExchange;
	KOOrderPtr _pOrder;
};

}

#endif /* OrderEvent_H */
