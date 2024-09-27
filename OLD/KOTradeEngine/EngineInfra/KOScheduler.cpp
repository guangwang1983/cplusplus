#include "KOScheduler.h"
#include "ErrorHandler.h"
#include "SystemClock.h"
#include "OrderConfirmEvent.h"
#include "PriceUpdateEvent.h"

using std::stringstream;
using std::cerr;

namespace KO
{

KOScheduler::KOScheduler(SchedulerConfig &cfg)
:SchedulerBase(cfg)
{

}

KOScheduler::~KOScheduler()
{

}

bool KOScheduler::init()
{
    bool bResult = true;

    preCommonInit();

    _pHistoricDataRegister.reset(new HistoricDataRegister(this, _cSchedulerCfg.sDate));
    _pSimulationExchange.reset(new SimulationExchange(this));

    for(unsigned int i = 0; i < _cSchedulerCfg.vProducts.size(); ++i)
    {
        QuoteData* pNewQuoteDataPtr = pregisterProduct(_cSchedulerCfg.vProducts[i], KO_FUTURE);
        pNewQuoteDataPtr->iCID = i;

        _pHistoricDataRegister->psubscribeNewProduct(pNewQuoteDataPtr, KOEpochTime(0,_cSchedulerCfg.vProductPriceLatencies[i]));
        _pSimulationExchange->addProductForSimulation(pNewQuoteDataPtr, _cSchedulerCfg.vProductOrderLatencies[i]);

        bool bRiskFileLoaded = bloadRiskFile();
        if(bRiskFileLoaded)
        {
/*
            map<string, ProductRiskPtr>::iterator riskItr = _mRiskSetting.find(sRootSymbol);
            riskItr->second->iProductCID = i;

            if(riskItr != _mRiskSetting.end())
            {
                _vGlobalPositions.push_back(riskItr->second);
            }
            else
            {
                stringstream cStringStream;
                cStringStream << "No risk parameter defined for product.";
                ErrorHandler::GetInstance()->newWarningMsg("0", "ALL", _cSchedulerCfg.vProducts[i], cStringStream.str());
            }
*/
        }

        stringstream cStringStream;
        cStringStream << "Registered product " << _cSchedulerCfg.vProducts[i] << ".";
        ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", _cSchedulerCfg.vProducts[i], cStringStream.str());
    }

    bResult = _pHistoricDataRegister->loadData();

    postCommonInit();

    KOEpochTime cEngineLiveDuration = _cTimerEnd - _cTimerStart;

    for(int i = 0; i < cEngineLiveDuration.sec();i++)
    {
        addNewWakeupCall(_cTimerStart + KOEpochTime(i,0), this);
    }

    return bResult;
}

void KOScheduler::run()
{
    while(true)
    {
        KOEpochTime cNextTimeStamp = _pHistoricDataRegister->cgetNextUpdateTime();

        if(cNextTimeStamp != KOEpochTime())
        {
            processTimeEvents(cNextTimeStamp);
        }
        else
        {
            break;
        }

//        applyPriceUpdateOnTime(cNextTimeStamp);
    }

    std::cout << "Simulation finished \n";
}

void KOScheduler::applyPriceUpdateOnTime(KOEpochTime cNextTimeStamp)
{
    _pHistoricDataRegister->applyNextUpdate(cNextTimeStamp);

    for(unsigned int i = 0; i < _vContractQuoteDatas.size(); ++i)
    {
        if(_pHistoricDataRegister->bproductHasNewUpdate(i))
        {
            _pSimulationExchange->newPriceUpdate(i);
            newPriceUpdate(i);
        }
    }
}

bool KOScheduler::bsubmitOrder(KOOrderPtr pOrder, double dPrice, long iQty)
{
    bool bResult = false;

    pOrder->setIsKOOrder(true);

    bResult = _pSimulationExchange->bsubmitOrder(pOrder, dPrice, iQty);

    if(bResult == true)
    {
        pOrder->changeOrderstat(KOOrder::PENDINGCREATION);
        if(!bisLiveTrading())
        {
            _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REQUEST|SUBMIT|KOID:" << pOrder->igetOrderID() << "|Product:" << pOrder->sgetOrderProductName() << "|Price:" << dPrice << "|Qty:" << iQty << "\n";
        }
    }
    else
    {
        if(!bisLiveTrading())
        {
            _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REPLY|SUBMIT_REJECT|KOID:" << pOrder->igetOrderID() << "\n";
        }
    }

    return bResult;    
}

bool KOScheduler::bchangeOrderPrice(KOOrderPtr pOrder, double dNewPrice)
{
    bool bResult = false;

    bResult = _pSimulationExchange->bchangeOrderPrice(pOrder, dNewPrice);

    if(bResult == true)
    {
        pOrder->changeOrderstat(KOOrder::PENDINGCHANGE);

        if(!bisLiveTrading())
        {
            _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REQUEST|CHANGE_PRICE|KOID:" << pOrder->igetOrderID() << "|NewPrice:" << dNewPrice << "\n";
        }
    }
    else
    {
        if(!bisLiveTrading())
        {
            _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REPLY|CHANGE_PRICE_REJECTE|KOID:" << pOrder->igetOrderID() << "\n";
        }
    }

    return bResult;
}

bool KOScheduler::bchangeOrder(KOOrderPtr pOrder, double dNewPrice, long iNewQty)
{
    bool bResult = false;

    bResult = _pSimulationExchange->bchangeOrder(pOrder, dNewPrice, iNewQty);

    if(bResult == true)
    {
        pOrder->changeOrderstat(KOOrder::PENDINGCHANGE);

        if(!bisLiveTrading())
        {
            _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REQUEST|CHANGE|KOID:" << pOrder->igetOrderID() << "|NewPrice:" << dNewPrice << "|NewQty:" << iNewQty << "\n";
        }
    }
    else
    {
        if(!bisLiveTrading())
        {
            _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REPLY|CHANGE_REJECTE|KOID:" << pOrder->igetOrderID() << "\n";
        }
    }

    return bResult;
}

bool KOScheduler::bdeleteOrder(KOOrderPtr pOrder)
{
    bool bResult = false;

    bResult = _pSimulationExchange->bdeleteOrder(pOrder);

    if(bResult == true)
    {
        pOrder->changeOrderstat(KOOrder::PENDINGDELETE);

        if(!bisLiveTrading())
        {
            _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REQUEST|DELETE|KOID:" << pOrder->igetOrderID() << "\n";
        }
    }
    else
    {
        if(!bisLiveTrading())
        {
            _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REPLY|DELETE_REJECTE|KOID:" << pOrder->igetOrderID() << "\n";
        }
    }

    return bResult;
}

KOEpochTime KOScheduler::cgetCurrentTime()
{
    return _cCurrentKOEpochTime;
}

bool KOScheduler::bisLiveTrading()
{
    return false;
}

void KOScheduler::onConfirm(KOOrderPtr pOrder)
{
    _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REPLY|CONFIRM|KOID:" << pOrder->igetOrderID() << "\n";

    if(pOrder->_eOrderState == KOOrder::PENDINGCREATION || pOrder->_eOrderState == KOOrder::PENDINGCHANGE)
    {
        orderConfirmed(pOrder);
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Unexpected ACCEPTED received! KOOrder ID " << pOrder->igetOrderID() << " order state is " << pOrder->_eOrderState << ".";
        ErrorHandler::GetInstance()->newWarningMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());

        orderConfirmedUnexpectedly(pOrder);
    }
}

void KOScheduler::onDelete(KOOrderPtr pOrder)
{
    _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REPLY|DELETE|KOID:" << pOrder->igetOrderID() << "\n";

    if(pOrder->_eOrderState == KOOrder::PENDINGDELETE)
    {
        orderDeleted(pOrder);
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Unexpected CANCELLED received! KOOrder ID " << pOrder->igetOrderID() << " order state is " << pOrder->_eOrderState << ".";
        ErrorHandler::GetInstance()->newWarningMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());

        orderDeletedUnexpectedly(pOrder);
    }
}

void KOScheduler::onAmendReject(KOOrderPtr pOrder)
{
    _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REPLY|REPLACE_REJECT|KOID:" << pOrder->igetOrderID() << "\n";

    if(pOrder->_eOrderState == KOOrder::PENDINGCHANGE)
    {
        orderAmendRejected(pOrder);

        pOrder->_pParent->orderCriticalErrorHandler(pOrder->igetOrderID());
        stringstream cStringStream;
        cStringStream << "Amend rejected for KOOrder ID " << pOrder->igetOrderID() << ".";
        ErrorHandler::GetInstance()->newWarningMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Unexpected REPLACE_REJECTED received! KOOrder ID " << pOrder->igetOrderID() << " order state is " << pOrder->_eOrderState << ".";
        ErrorHandler::GetInstance()->newWarningMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
    }
}

void KOScheduler::onReject(KOOrderPtr pOrder)
{
    _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REPLY|REJECT|KOID:" << pOrder->igetOrderID() << "\n";

    if(pOrder->_eOrderState == KOOrder::PENDINGCREATION)
    {
        orderRejected(pOrder);
        pOrder->_pParent->orderCriticalErrorHandler(pOrder->igetOrderID());

        stringstream cStringStream;
        cStringStream << "Submit rejected for KOOrder ID " << pOrder->igetOrderID() << ".";
        ErrorHandler::GetInstance()->newWarningMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Unexpected REJECTED received! KOOrder ID " << pOrder->igetOrderID() << " order state is " << pOrder->_eOrderState << ".";
        ErrorHandler::GetInstance()->newWarningMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
    }

}

void KOScheduler::onCancelReject(KOOrderPtr pOrder)
{
    _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REPLY|DELET_REJECT|KOID:" << pOrder->igetOrderID() << "\n";

    if(pOrder->_eOrderState == KOOrder::PENDINGDELETE)
    {
        orderDeleteRejected(pOrder);

        stringstream cStringStream;
        cStringStream << "Cancel rejected for KOOrder ID " << pOrder->igetOrderID() << ".";
        ErrorHandler::GetInstance()->newWarningMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Unexpected CANCEL_REJECTED received! KOOrder ID " << pOrder->igetOrderID() << " order state is " << pOrder->_eOrderState << ".";
        ErrorHandler::GetInstance()->newWarningMsg("0", pOrder->_sAccount, pOrder->sgetOrderProductName(), cStringStream.str());
    }
}

void KOScheduler::onFill(KOOrderPtr pOrder, double dPrice, long iQty)
{
    _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REPLY|FILL|KOID:" << pOrder->igetOrderID() << "|Price:" << dPrice << "|Qty:" << iQty << "\n";

    orderFilled(pOrder, iQty, dPrice);

    if(pOrder->_iOrderConfirmedQty == 0)
    {
        _cOrderActionLogger << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << "|REPLY|DELETE|KOID:" << pOrder->igetOrderID() << "\n";
        orderDeleted(pOrder);
    }
}

void KOScheduler::addNewOrderConfirmCall(SimulationExchange* pSimulationExchange, KOOrderPtr pOrder, KOEpochTime cCallTime)
{
    OrderConfirmEvent* pNewOrderConfirm = new OrderConfirmEvent(pSimulationExchange, pOrder, cCallTime);
    _cDynamicTimeEventQueue.push(pNewOrderConfirm);
}

void KOScheduler::addNewPriceUpdateCall(KOEpochTime cCallTime)
{
    PriceUpdateEvent* pNewPriceUpdateCall = new PriceUpdateEvent(this, cCallTime);
    _vStaticTimeEventQueue.push_back(pNewPriceUpdateCall);
}

};
