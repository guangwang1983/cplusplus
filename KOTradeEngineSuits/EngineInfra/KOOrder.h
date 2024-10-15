#ifndef KOOrder_H
#define KOOrder_H

#include <string>

#include "KOEpochTime.h"
#include "OrderParentInterface.h"
#include <boost/shared_ptr.hpp>
#include "QuoteData.h"
#include <deque>

using std::string;

namespace KO
{

class KOOrder
{
public:
    enum OrderState
    {
        INACTIVE,
        ACTIVE,
        PENDINGCREATION,
        PENDINGCHANGE,
        PENDINGDELETE
    };

    friend class QuickFixScheduler;
    friend class QuickFixSchedulerFXMultiBook;
    friend class KOScheduler;
    friend class SchedulerBase;
    friend class SimulationExchange;

    KOOrder(const string& iPendingOrderID, int iCID, double dTickSize, bool bIsIOC, const string& sProduct, const string& sAccount, const string& sExchange, InstrumentType eInstrumentType, OrderParentInterface* pParent);

    InstrumentType egetInstrumentType();

    int igetProductCID();

    const string& sgetPendingOrderID();
    const string& sgetConfirmedOrderID();
    const string& sgetTBOrderID();

    long igetOrderOrgQty();
    long igetOrderRemainQty();
    double dgetOrderPrice();
    long igetOrderPriceInTicks();
    bool bgetIsIOC();
    const string& sgetOrderAccount();
    const string& sgetOrderProductName();
    const string& sgetOrderExchange();
    OrderState egetOrderstate();
    bool borderCanBeChanged();
    
protected:

    InstrumentType _eInstrumentType;

    OrderState _eOrderState;

    string _sPendingOrderID;
    string _sConfirmedOrderID;
    string _sTBOrderID;

    int _iCID;
    bool _bIsIOC;
    string _sProduct;
    string _sExchange;
    string _sAccount;
    double _dTickSize;
    OrderParentInterface* _pParent;

    KOEpochTime _cPendingRequestTime;

    long _iOrderOrgQty;
    long _iOrderRemainQty;
    double _dOrderPrice;
    long _iOrderPriceInTicks;

    bool _bOrderNoReplyTriggered;

    bool _bOrderNoFill;
    KOEpochTime _cOrderNoFillTime;    
    bool _bOrderNoFillTriggered;

    std::deque<KOEpochTime> _qOrderMessageHistory;
};

typedef boost::shared_ptr<KOOrder> KOOrderPtr;

}

#endif
