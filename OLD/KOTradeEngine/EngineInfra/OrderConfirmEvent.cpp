#include "OrderConfirmEvent.h"

namespace KO
{

OrderConfirmEvent::OrderConfirmEvent(SimulationExchange* pSimulationExchange, KOOrderPtr pOrder, KOEpochTime cCallTime)
:TimeEvent(cCallTime),
 _pSimulationExchange(pSimulationExchange),
 _pOrder(pOrder)
{
	_eTimeEventType = TimeEvent::OrderConfirmEvent;	
}

OrderConfirmEvent::~OrderConfirmEvent()
{

}

void OrderConfirmEvent::makeCall()
{
	_pSimulationExchange->orderConfirmCallBack(_pOrder);
}

}
