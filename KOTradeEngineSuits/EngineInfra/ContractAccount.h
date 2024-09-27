#ifndef ContractAccount_H
#define ContractAccount_H

#include <string>
#include <deque>
#include "KOEpochTime.h"
#include "TradeEngineBase.h"
#include "OrderParentInterface.h"
#include "QuoteData.h"

using std::vector;
using std::deque;

namespace KO
{

class SchedulerBase;

class ContractAccount 
{

friend class SchedulerBase;

public:
    enum AccountState
    {
        Trading,
        Halt
    };

    ContractAccount();
    ContractAccount(SchedulerBase* pScheduler,
                    TradeEngineBase* pParent, 
                    QuoteData* pQuoteData,
                    const string& sEngineSlotName, 
                    const string& sProduct, 
                    const string& sExchange, 
                    double dTickSize, 
                    long iMaxOrderSize,
                    long iLimit,
                    double iPnlLimit,
                    const string& sTradingAccount,
                    bool bIsLiveTrading);
    ~ContractAccount();

    const string& sgetAccountProductName();
    const string& sgetAccountExchangeName();
    const string& sgetRootSymbol();
    double dgetTickSize();
    const string& sgetAccountName();
    int igetCID();

    void haltTrading();
    void resumeTrading();

private:
    TradeEngineBase* _pParent;
    QuoteData* _pQuoteData;

    string _sEngineSlotName;
    string _sProduct;
    string _sExchange;
    double _dTickSize;
    long   _iMaxOrderSize;
    long   _iLimit;
    double _dPnlLimit;
    string _sTradingAccount;

    AccountState _eAccountState;
    bool _bIsLiveTrading;

    SchedulerBase* _pScheduler;
};

}

#endif /* ContractAccount_H */
