#include "ContractAccount.h"
#include <iostream>
#include <boost/math/special_functions/round.hpp>
#include "SystemClock.h"
#include "ErrorHandler.h"
#include "SchedulerBase.h"

using std::stringstream;

namespace KO
{

ContractAccount::ContractAccount()
{

}

ContractAccount::ContractAccount(SchedulerBase* pScheduler,
                                 TradeEngineBase* pParent, 
                                 QuoteData* pQuoteData,
                                 const string& sEngineSlotName, 
                                 const string& sProduct,
                                 const string& sExchange, 
                                 double dTickSize, 
                                 long iMaxOrderSize,
                                 long iLimit,
                                 double dPnlLimit,
                                 const string& sTradingAccount,
                                 bool bIsLiveTrading)
:_pParent(pParent),
 _pQuoteData(pQuoteData),
 _sEngineSlotName(sEngineSlotName),
 _sProduct(sProduct),
 _sExchange(sExchange),
 _dTickSize(dTickSize),
 _iMaxOrderSize(iMaxOrderSize),
 _iLimit(iLimit),
 _dPnlLimit(dPnlLimit),
 _sTradingAccount(sTradingAccount),
 _eAccountState(Trading),
 _bIsLiveTrading(bIsLiveTrading),
 _pScheduler(pScheduler)
{

}
   
ContractAccount::~ContractAccount()
{

}

const string& ContractAccount::sgetAccountProductName()
{
    return _sProduct;
}

const string& ContractAccount::sgetAccountExchangeName()
{
    return _sExchange;
}

const string& ContractAccount::sgetRootSymbol()
{
    return _pQuoteData->sRoot;
}

double ContractAccount::dgetTickSize()
{
    return _dTickSize;
}

const string& ContractAccount::sgetAccountName()
{
    return _sTradingAccount;
}

int ContractAccount::igetCID()
{
    return _pQuoteData->iCID;
}

void ContractAccount::resumeTrading()
{
    _eAccountState = Trading;
}

void ContractAccount::haltTrading()
{
    _eAccountState = Halt;
}

}
