#ifndef OrderRiskChecker_H
#define OrderRiskChecker_H

#include <boost/shared_ptr.hpp>
#include "KOOrder.h"
#include "KOEpochTime.h"
#include <vector>
#include <deque>

using std::vector;
using std::pair;
using std::deque;

namespace KO
{

class SchedulerBase;

class OrderRiskChecker
{
public:
    static boost::shared_ptr<OrderRiskChecker> GetInstance();
    void assignScheduler(SchedulerBase* pScheduler);
    
    bool bsubmitOrder(KOOrderPtr pOrder, double dPrice, long iQty);
    bool bchangeOrderPrice(KOOrderPtr pOrder, double dNewPrice);
    bool bchangeOrder(KOOrderPtr pOrder, double dNewPrice, long iNewQty);
    bool bdeleteOrder(KOOrderPtr pOrder);
    void checkOrderStatus(KOOrderPtr pOrder);

    bool bcheckActionFrequency(int iTargetCID);
private:
    OrderRiskChecker();
    static boost::shared_ptr<OrderRiskChecker> _pInstance;

    SchedulerBase* _pScheduler;

    bool bcheckNewOrderRisk(KOOrderPtr pOrder, long iQty);
    bool bcheckAmendOrderRisk(KOOrderPtr pOrder, long iQty);
    bool bcheckPrice(KOOrderPtr pOrder, double dPrice, long iQty);

    vector<pair<int, boost::shared_ptr<deque<KOEpochTime> > > > _vOrderSubmitHistorty;
};

}

#endif /* OrderRiskChecker_H */
