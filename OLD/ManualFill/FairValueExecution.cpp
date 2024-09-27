#include "../EngineInfra/SchedulerBase.h"
#include "FairValueExecution.h"
#include "../EngineInfra/ErrorHandler.h"
#include "../EngineInfra/KOEpochTime.h"
#include "../EngineInfra/SystemClock.h"
#include <boost/math/special_functions/round.hpp>

using std::stringstream;

namespace KO
{

FairValueExecution::FairValueExecution(boost::shared_ptr<ContractAccount> pTargetAccount, string& sEngineSlotName, Instrument* pInstrument, long iQuoteQty, long lInitPosition, TradeEngineBase* pParent, SimpleLogger* pLogger)
:_iQuoteQty(iQuoteQty),
 _iTheoPosition(0),
 _iPosition(lInitPosition),
 _sEngineSlotName(sEngineSlotName),
 _pTargetAccount(pTargetAccount),
 _pInstrument(pInstrument),
 _eExecutionState(Off),
 _eHittingSignalState(FLAT),
 _pLogger(pLogger),
 _pParent(pParent)
{
    _iPrevBidPrice = 0;
    _iPrevOfferPrice = 500000;
    _iPrevBidExitPrice = 0;
    _iPrevOfferExitPrice = 500000;

    _iCurrentBidPrice = 0;
    _iCurrentOfferPrice = 500000;
    _iCurrentBidExitPrice = 0;
    _iCurrentOfferExitPrice = 500000;

    _iTheoBid = 0;
    _iTheoOffer = 0;
    _iTheoExitBid = 0;
    _iTheoExitOffer = 0;

    _bEstablishSegEntryPos = false;
}

FairValueExecution::~FairValueExecution()
{

}

void FairValueExecution::setPosition(long iNewPosition)
{
    _iPosition = iNewPosition;
}

void FairValueExecution::setTheoPosition(long iNewTheoPosition)
{
    _iTheoPosition = iNewTheoPosition;
}

void FairValueExecution::setEstablishSegEntryPos(bool bEstablishSegEntryPos)
{
    _bEstablishSegEntryPos = bEstablishSegEntryPos;
}

void FairValueExecution::updateOrders()
{
     if(_eExecutionState == Quoting || _eExecutionState == PatientQuoting ||
        _eExecutionState == Hitting || _eExecutionState == PatientHitting)
    {
        if(_eExecutionState == PatientHitting || _eExecutionState == PatientQuoting)
        {
            pullOpenSellQuote();
            pullOpenBuyQuote();
        }

        if(_iTheoPosition > 0)
        {
            pullExitSellQuote();
            pullOpenSellQuote();

            if(igetOpenSellQuoteQty() == 0 && igetCloseSellQuoteQty() == 0)
            {
                if(_iPosition < 0)
                {
                    long iNewCloseBuyOrderQty = -1 * _iPosition;

                    if(igetCloseBuyQuoteQty() == 0)
                    {
                        // submit new close buy quote
                        (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Submitting new close buy quote. qty " << iNewCloseBuyOrderQty << " price " << _iCurrentBidExitPrice << "\n";
                        if(bsubmitQuote(iNewCloseBuyOrderQty, _iCurrentBidExitPrice, CLOSE_BUY))
                        {
                            (*_pLogger) << "New order submitted \n";
                        }
                        else
                        {
                            (*_pLogger) << "Failed to submit new order \n";
                        }
                    }
                    else
                    {
                        if((igetCloseBuyQuoteQty() != iNewCloseBuyOrderQty || _pCloseBuyQuote->igetOrderPrice() != _iCurrentBidExitPrice) && _pCloseBuyQuote->bquoteCanBeChanged())
                        {
                            // amend current close buy quote
                            (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Amending existing close buy quote id " << _pCloseBuyQuote->igetOrderID() << ". qty " << iNewCloseBuyOrderQty << " price " << _iCurrentBidExitPrice << "\n";

                            if(bamendQuote(iNewCloseBuyOrderQty, _iCurrentBidExitPrice, CLOSE_BUY))
                            {
                                (*_pLogger) << "Order amend submitted \n";
                            }
                            else
                            {
                                (*_pLogger) << "Fail to submit order amend \n";
                            }
                        }
                    }
                }

                if(_eExecutionState == Quoting || _eExecutionState == Hitting)
                {
                    long iNewOpenBuyOrderQty = _iTheoPosition - (_iPosition + igetCloseBuyQuoteQty());

                    if(iNewOpenBuyOrderQty != 0)
                    {
                        if(igetOpenBuyQuoteQty() == 0)
                        {
                            (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Submitting new open buy quote. qty " << iNewOpenBuyOrderQty << " price " << _iCurrentBidPrice << "\n";

                            if(bsubmitQuote(iNewOpenBuyOrderQty, _iCurrentBidPrice, OPEN_BUY))
                            {
                                (*_pLogger) << "New order submitted \n";
                            }
                            else
                            {
                                (*_pLogger) << "Failed to submit new order \n";
                            }
                        }
                        else
                        {

                            if((igetOpenBuyQuoteQty() != iNewOpenBuyOrderQty || _pOpenBuyQuote->igetOrderPrice() != _iCurrentBidPrice) && _pOpenBuyQuote->bquoteCanBeChanged())
                            {
                                (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Amending existing open buy quote id " << _pOpenBuyQuote->igetOrderID() << ". qty " << iNewOpenBuyOrderQty << " price " << _iCurrentBidPrice << "\n";

                                if(bamendQuote(iNewOpenBuyOrderQty, _iCurrentBidPrice, OPEN_BUY))
                                {
                                    (*_pLogger) << "Order amend submitted \n";
                                }
                                else
                                {
                                    (*_pLogger) << "Failed to submit order amend \n";
                                }
                            }
                        }
                    }
                }
            }
        }
        else if(_iTheoPosition < 0)
        {
            pullExitBuyQuote();
            pullOpenBuyQuote();

            if(igetOpenBuyQuoteQty() == 0 && igetCloseBuyQuoteQty() == 0)
            {
                if(_iPosition > 0)
                {
                    long iNewCloseSellOrderQty = -1 * _iPosition;
               
                    if(igetCloseSellQuoteQty() == 0) 
                    {
                        // submit new close sell quote
                        (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Submitting new close sell quote. qty " << iNewCloseSellOrderQty << " price " << _iCurrentOfferExitPrice << "\n";
                        if(bsubmitQuote(iNewCloseSellOrderQty, _iCurrentOfferExitPrice, CLOSE_SELL))
                        {
                            (*_pLogger) << "New order submitted \n";
                        }
                        else
                        {
                            (*_pLogger) << "Failed to submit new order \n";
                        }
                    }
                    else
                    {
                        if((igetCloseSellQuoteQty() != iNewCloseSellOrderQty || _pCloseSellQuote->igetOrderPrice() != _iCurrentOfferExitPrice) && _pCloseSellQuote->bquoteCanBeChanged())
                        {
                            // amend current close sell quote
                            (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Amending existing close sell quote id " << _pCloseSellQuote->igetOrderID() << ". qty " << iNewCloseSellOrderQty << " price " << _iCurrentOfferExitPrice << "\n";

                            if(bamendQuote(iNewCloseSellOrderQty, _iCurrentOfferExitPrice, CLOSE_SELL))
                            {
                                (*_pLogger) << "Order amend submitted \n";
                            }
                            else
                            {
                                (*_pLogger) << "Failed to submit order amend \n";
                            }
                        }
                    }
                }

                if(_eExecutionState == Quoting || _eExecutionState == Hitting)
                {
                    long iNewOpenSellOrderQty = _iTheoPosition - (_iPosition + igetCloseSellQuoteQty());

                    if(_iCurrentOfferPrice != 0)
                    {
                        if(iNewOpenSellOrderQty != 0)
                        {
                            if(igetOpenSellQuoteQty() == 0)
                            {
                                (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Submitting new open sell quote. qty " << iNewOpenSellOrderQty << " price " << _iCurrentOfferPrice << "\n";

                                if(bsubmitQuote(iNewOpenSellOrderQty, _iCurrentOfferPrice, OPEN_SELL))
                                {
                                    (*_pLogger) << "New order submitted \n";
                                }
                                else
                                {
                                    (*_pLogger) << "Failed to submit new order \n";
                                }
                            }
                            else
                            {
                                if((igetOpenSellQuoteQty() != iNewOpenSellOrderQty || _pOpenSellQuote->igetOrderPrice() != _iCurrentOfferPrice) && _pOpenSellQuote->bquoteCanBeChanged())
                                {
                                    (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Amending existing open sell quote id " << _pOpenSellQuote->igetOrderID() << ". qty " << iNewOpenSellOrderQty << " price " << _iCurrentOfferPrice << "\n";

                                    if(bamendQuote(iNewOpenSellOrderQty, _iCurrentOfferPrice, OPEN_SELL))
                                    {
                                        (*_pLogger) << "Order amend submitted \n";
                                    }
                                    else
                                    {
                                        (*_pLogger) << "Failed to submit order amend \n";
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else if(_iTheoPosition == 0)
        {
            pullOpenBuyQuote();
            pullOpenSellQuote();

            if(_iPosition > 0)
            {
                pullExitBuyQuote();

                if(igetOpenBuyQuoteQty() == 0 && igetOpenSellQuoteQty() == 0 && igetCloseBuyQuoteQty() == 0)
                {
                    long iNewCloseSellOrderQty = -1 * _iPosition;
               
                    if(igetCloseSellQuoteQty() == 0) 
                    {
                        // submit new close sell quote
                        (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Submitting new close sell quote. qty " << iNewCloseSellOrderQty << " price " << _iCurrentOfferExitPrice << "\n";
                        if(bsubmitQuote(iNewCloseSellOrderQty, _iCurrentOfferExitPrice, CLOSE_SELL))
                        {
                            (*_pLogger) << "New order submitted \n";
                        }
                        else
                        {
                            (*_pLogger) << "Failed to submit new order \n";
                        }
                    }
                    else
                    {
                        if((igetCloseSellQuoteQty() != iNewCloseSellOrderQty || _pCloseSellQuote->igetOrderPrice() != _iCurrentOfferExitPrice) && _pCloseSellQuote->bquoteCanBeChanged())
                        {
                            // amend current close sell quote
                            (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Amending existing close sell quote id " << _pCloseSellQuote->igetOrderID() << ". qty " << iNewCloseSellOrderQty << " price " << _iCurrentOfferExitPrice << "\n";

                            if(bamendQuote(iNewCloseSellOrderQty, _iCurrentOfferExitPrice, CLOSE_SELL))
                            {
                                (*_pLogger) << "Order amend submitted \n";
                            }
                            else
                            {
                                (*_pLogger) << "Failed to submit order amend \n";
                            }
                        }
                    }
                }
            }
            else if(_iPosition < 0)
            {
                pullExitSellQuote();

                if(igetOpenBuyQuoteQty() == 0 && igetOpenSellQuoteQty() == 0 && igetCloseSellQuoteQty() == 0)
                {
                    long iNewCloseBuyOrderQty = -1 * _iPosition;

                    if(igetCloseBuyQuoteQty() == 0)
                    {
                        // submit new close buy quote
                        (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Submitting new close buy quote. qty " << iNewCloseBuyOrderQty << " price " << _iCurrentBidExitPrice << "\n";
                        if(bsubmitQuote(iNewCloseBuyOrderQty, _iCurrentBidExitPrice, CLOSE_BUY))
                        {
                            (*_pLogger) << "New order submitted \n";
                        }
                        else
                        {
                            (*_pLogger) << "Failed to submit new order \n";
                        }
                    }
                    else
                    {
                        if((igetCloseBuyQuoteQty() != iNewCloseBuyOrderQty || _pCloseBuyQuote->igetOrderPrice() != _iCurrentBidExitPrice) && _pCloseBuyQuote->bquoteCanBeChanged())
                        {
                            // amend current close buy ioc
                            (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Amending existing close buy quote id " << _pCloseBuyQuote->igetOrderID() << ". qty " << iNewCloseBuyOrderQty << " price " << _iCurrentBidExitPrice << "\n";

                            if(bamendQuote(iNewCloseBuyOrderQty, _iCurrentBidExitPrice, CLOSE_BUY))
                            {
                                (*_pLogger) << "Order amend submitted \n";
                            }
                            else
                            {
                                (*_pLogger) << "Fail to submit order amend \n";
                            }
                        }
                    }
                }
            }
        }
    }
    else if(_eExecutionState == IOC || _eExecutionState == PatientIOC)
    {
        if(_iTheoPosition > 0)
        {
            if(igetOpenSellIOCQty() == 0 && igetCloseSellIOCQty() == 0)
            {
                if(_iPosition < 0)
                {
                    long iNewCloseBuyOrderQty = -1 * _iPosition;

                    if(igetCloseBuyIOCQty() == 0)
                    {
                        if(_iTheoExitBid >= _pInstrument->igetBestAsk())
                        {
                            // submit new close buy ioc
                            (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Submitting new close buy ioc. qty " << iNewCloseBuyOrderQty << " price " << _pInstrument->igetBestAsk() << "\n";
                            if(bsubmitIOC(iNewCloseBuyOrderQty, _pInstrument->igetBestAsk(), CLOSE_BUY))
                            {
                                (*_pLogger) << "New order submitted \n";
                            }
                            else
                            {
                                (*_pLogger) << "Failed to submit new order \n";
                            }
                        }
                    }
                }

                if(_eExecutionState == IOC)
                {
                    long iNewOpenBuyOrderQty = _iTheoPosition - (_iPosition + igetCloseBuyIOCQty());

                    if(iNewOpenBuyOrderQty != 0)
                    {
                        if(igetOpenBuyIOCQty() == 0)
                        {
                            if(_iTheoBid >= _pInstrument->igetBestAsk())
                            {
                                (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Submitting new open buy ioc. qty " << iNewOpenBuyOrderQty << " price " << _pInstrument->igetBestAsk() << "\n";

                                if(bsubmitIOC(iNewOpenBuyOrderQty, _pInstrument->igetBestAsk(), OPEN_BUY))
                                {
                                    (*_pLogger) << "New order submitted \n";
                                }
                                else
                                {
                                    (*_pLogger) << "Failed to submit new order \n";
                                }
                            }
                        }
                    }
                }
            }
        }
        else if(_iTheoPosition < 0)
        {
            if(igetOpenBuyIOCQty() == 0 && igetCloseBuyIOCQty() == 0)
            {
                if(_iPosition > 0)
                {
                    long iNewCloseSellOrderQty = -1 * _iPosition;

                    if(igetCloseSellIOCQty() == 0)
                    {
                        if(_iTheoExitOffer <= _pInstrument->igetBestBid())
                        {
                            // submit new close sell ioc
                            (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Submitting new close sell ioc. qty " << iNewCloseSellOrderQty << " price " << _pInstrument->igetBestBid() << "\n";
                            if(bsubmitIOC(iNewCloseSellOrderQty, _pInstrument->igetBestBid(), CLOSE_SELL))
                            {
                                (*_pLogger) << "New order submitted \n";
                            }
                            else
                            {
                                (*_pLogger) << "Failed to submit new order \n";
                            }
                        }
                    }
                }

                if(_eExecutionState == IOC)
                {
                    long iNewOpenSellOrderQty = _iTheoPosition - (_iPosition + igetCloseSellIOCQty());

                    if(_iCurrentOfferPrice != 0)
                    {
                        if(iNewOpenSellOrderQty != 0)
                        {
                            if(igetOpenSellIOCQty() == 0)
                            {
                                if(_iTheoOffer <= _pInstrument->igetBestBid())
                                {
                                    (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Submitting new open sell ioc. qty " << iNewOpenSellOrderQty << " price " << _pInstrument->igetBestBid() << "\n";

                                    if(bsubmitIOC(iNewOpenSellOrderQty, _pInstrument->igetBestBid(), OPEN_SELL))
                                    {
                                        (*_pLogger) << "New order submitted \n";
                                    }
                                    else
                                    {
                                        (*_pLogger) << "Failed to submit new order \n";
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else if(_iTheoPosition == 0)
        {
            if(_iPosition > 0)
            {
                if(igetOpenBuyIOCQty() == 0 && igetOpenSellIOCQty() == 0 && igetCloseBuyIOCQty() == 0)
                {
                    long iNewCloseSellOrderQty = -1 * _iPosition;

                    if(igetCloseSellIOCQty() == 0)
                    {
                        if(_iTheoExitOffer <= _pInstrument->igetBestBid())
                        {
                            // submit new close sell ioc
                            (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Submitting new close sell ioc. qty " << iNewCloseSellOrderQty << " price " << _pInstrument->igetBestBid() << "\n";
                            if(bsubmitIOC(iNewCloseSellOrderQty, _pInstrument->igetBestBid(), CLOSE_SELL))
                            {
                                (*_pLogger) << "New order submitted \n";
                            }
                            else
                            {
                                (*_pLogger) << "Failed to submit new order \n";
                            }
                        }
                    }
                }
            }
            else if(_iPosition < 0)
            {
                if(igetOpenBuyIOCQty() == 0 && igetOpenSellIOCQty() == 0 && igetCloseSellIOCQty() == 0)
                {
                    long iNewCloseBuyOrderQty = -1 * _iPosition;

                    if(igetCloseBuyIOCQty() == 0)
                    {
                        if(_iTheoExitBid >= _pInstrument->igetBestAsk())
                        {
                            // submit new close buy ioc
                            (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Submitting new close buy ioc. qty " << iNewCloseBuyOrderQty << " price " << _pInstrument->igetBestAsk() << "\n";
                            if(bsubmitIOC(iNewCloseBuyOrderQty, _pInstrument->igetBestAsk(), CLOSE_BUY))
                            {
                                (*_pLogger) << "New order submitted \n";
                            }
                            else
                            {
                                (*_pLogger) << "Failed to submit new order \n";
                            }
                        }
                    }
                }
            }
        }
    }
}

void FairValueExecution::wakeup()
{
    (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable()
                << "|" << _iCurrentBidPrice
                << "|" << _iCurrentOfferPrice
                << "|" << _iCurrentBidExitPrice
                << "|" << _iCurrentOfferExitPrice
                << "|" << igetOpenBuyQuoteQty()  
                << "|" << igetOpenSellQuoteQty() 
                << "|" << igetCloseBuyQuoteQty() 
                << "|" << igetCloseSellQuoteQty()
                << "|" << _iPosition << "\n";

    if(_eExecutionState == Off)
    {
        pullAllQuotes();
    }
    else
    {
        updateOrders();
    }
}

void FairValueExecution::orderConfirmHandler(int iOrderID)
{
    string sOrderType = "Unknown order";
    lookUpOrder(sOrderType, iOrderID);

    (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " " << sOrderType << " id " << iOrderID << " entry confirmed \n";
}

void FairValueExecution::orderFillHandler(int iOrderID, long iFilledQty, double dPrice)
{
    _iPosition = _iPosition + iFilledQty;

    int iPriceInTicks = boost::math::iround(dPrice / _pInstrument->dgetTickSize());

    string sOrderType = "Unknown order";
    lookUpOrder(sOrderType, iOrderID);

    (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " " << sOrderType << " id " << iOrderID << " filled. qty " << iFilledQty << " price " << dPrice << " priceInTicks is " << iPriceInTicks << "\n";

    (*_pLogger) << "New position is " << _iPosition << "\n";
}

void FairValueExecution::orderRejectHandler(int iOrderID)
{
    string sOrderType = "Unknown order";
    lookUpOrder(sOrderType, iOrderID);

    (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " " << sOrderType << " id " << iOrderID << " rejected \n";
}

void FairValueExecution::orderDeleteHandler(int iOrderID)
{
    string sOrderType = "Unknown order";
    lookUpOrder(sOrderType, iOrderID);
    (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " " << sOrderType << " id " << iOrderID << " deleted \n";
}

void FairValueExecution::orderDeleteRejectHandler(int iOrderID)
{
    string sOrderType = "Unknown order";
    lookUpOrder(sOrderType, iOrderID);
    (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " " << sOrderType << " id " << iOrderID << " delete rejected \n";
}

void FairValueExecution::orderAmendRejectHandler(int iOrderID)
{
    string sOrderType = "Unknown order";
    lookUpOrder(sOrderType, iOrderID);
    (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " " << sOrderType << " id " << iOrderID << " amend rejected \n";
}

void FairValueExecution::orderUnexpectedConfirmHandler(int iOrderID)
{
    string sOrderType = "Unknown order";
    QuoteOrderPtr pResultOrder = lookUpOrder(sOrderType, iOrderID);

    if(pResultOrder.get())
    {    
        (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " " << sOrderType << " id " << iOrderID << " amended and confirmed from outside the engine! Order price " << pResultOrder->igetOrderPrice() << " Order Qty " << pResultOrder->igetOrderRemainQty() << "\n";
    }
    else
    {
        (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " " << sOrderType << " id " << iOrderID << " amended and confirmed from outside the engine!" << "\n";
    }
}

void FairValueExecution::orderUnexpectedDeleteHandler(int iOrderID)
{
    string sOrderType = "Unknown order";
    lookUpOrder(sOrderType, iOrderID);

    (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " " << sOrderType << " id " << iOrderID << " deleted from outside the engine.\n";
}

void FairValueExecution::orderCriticalErrorHandler(int iOrderID)
{

}

FairValueExecution::ExecutionState FairValueExecution::egetExecutionState()
{
    return _eExecutionState;
}

void FairValueExecution::setExecutionState(ExecutionState eExecutionState)
{
    _eExecutionState = eExecutionState;
}

void FairValueExecution::newTheoQuotePrices(double dNewTheoOpenBuyPrice, double dNewTheoOpenSellPrice, double dNewTheoCloseBuyPrice, double dNewTheoCloseSellPrice, bool bApplyNewPrices)
{
    _iTheoBid = floor(dNewTheoOpenBuyPrice);
    _iTheoOffer = ceil(dNewTheoOpenSellPrice);
    _iTheoExitBid = floor(dNewTheoCloseBuyPrice); 
    _iTheoExitOffer = ceil(dNewTheoCloseSellPrice);

    adjustTheoQuotePrices();

    if(bApplyNewPrices)
    {
        updateQuotePrice();
    }
}

void FairValueExecution::adjustTheoQuotePrices()
{
    double dRefPrice = (_pInstrument->igetBestBid() + _pInstrument->igetBestAsk()) / 2;

    if(_pInstrument->igetAccumuTradeSize() != 0)
    {
        dRefPrice = (double)_pInstrument->igetLastTrade();
    }

    long iLimitBid = ceil(dRefPrice * 0.9975);
    long iLimitOffer = floor(dRefPrice * 1.0025);

    // recalculate exit bid quote
    if(_iTheoExitBid >= _pInstrument->igetBestAsk())
    {
        if(_pInstrument->egetInstrumentType() == KO_FUTURE)
        {
            if(_pInstrument->bgetAskTradingOut())
            {
                _iTheoExitBid = _pInstrument->igetBestAsk();
            }
            else
            {
                _iTheoExitBid = _pInstrument->igetBestAsk() - 1;
            }
        }
    }
    else 
    {
        if(_iTheoExitBid < iLimitBid)
        {
            _iTheoExitBid = iLimitBid;
        }
    }

    // recalculate exit offer quote
    if(_iTheoExitOffer <= _pInstrument->igetBestBid())
    {
        if(_pInstrument->egetInstrumentType() == KO_FUTURE)
        {
            if(_pInstrument->bgetBidTradingOut())
            {
                _iTheoExitOffer = _pInstrument->igetBestBid();
            }
            else
            {
                _iTheoExitOffer = _pInstrument->igetBestBid() + 1;
            }
        }
    }
    else 
    {
        if(_iTheoExitOffer > iLimitOffer)
        {
            _iTheoExitOffer = iLimitOffer;
        }
    }
//(*_pLogger) << "after recalc _iTheoExitOffer: " << _iTheoExitOffer << "\n";

    // recalculate open bid quote
    if(_iTheoBid >= _pInstrument->igetBestAsk())
    {
        if(_pInstrument->egetInstrumentType() == KO_FUTURE)
        {
            if(_pInstrument->bgetAskTradingOut())
            {
                _iTheoBid = _pInstrument->igetBestAsk();
            }
            else
            {
                _iTheoBid = _pInstrument->igetBestAsk() - 1;
            }
        }
    }
    else 
    {
        if(_iTheoBid < iLimitBid)
        {
            _iTheoBid = iLimitBid;
        }
    }

    if(_iPosition < 0)
    {
        if(_iTheoBid == _iTheoExitBid)
        {
            _iTheoBid = _iTheoExitBid - 1;
        }
    }

    // recalculate open offer quote
    if(_iTheoOffer <= _pInstrument->igetBestBid())
    {
        if(_pInstrument->egetInstrumentType() == KO_FUTURE)
        {
            if(_pInstrument->bgetBidTradingOut())
            {
                _iTheoOffer = _pInstrument->igetBestBid();
            }
            else
            {
                _iTheoOffer = _pInstrument->igetBestBid() + 1;
            }
        }
    }
    else
    {
        if(_iTheoOffer > iLimitOffer)
        {
            _iTheoOffer = iLimitOffer;
        }
    }

    if(_iPosition > 0)
    {
        if(_iTheoOffer == _iTheoExitOffer)
        {
            _iTheoOffer = _iTheoExitOffer + 1;
        }
    }

    // Now Adjust the quote price so that order is only amended when it is 2 tick with the current price
    if(_iCurrentBidPrice == 0)
    {
        _iCurrentBidPrice = _iTheoBid;
    }
    else
    {
        if(_iTheoBid < _iCurrentBidPrice)
        {
            _iPrevBidPrice = _iCurrentBidPrice;
            _iCurrentBidPrice = _iTheoBid;
        }
        else
        {
            if(_iTheoBid > _pInstrument->igetBestBid() - 2 || _iCurrentBidPrice > _pInstrument->igetBestBid() - 2)
            {
                if(_iCurrentBidPrice != _iTheoBid)
                {
                    _iPrevBidPrice = _iCurrentBidPrice;
                    _iCurrentBidPrice = _iTheoBid;
                }
            }
        }
    }

    if(_iCurrentOfferPrice == 500000)
    {
        _iCurrentOfferPrice = _iTheoOffer;
    }
    else
    {
        if(_iTheoOffer > _iCurrentOfferPrice)
        {
            _iPrevOfferPrice = _iCurrentOfferPrice;
            _iCurrentOfferPrice = _iTheoOffer;
        }
        else
        {
            if(_iTheoOffer < _pInstrument->igetBestAsk() + 2 || _iCurrentOfferPrice < _pInstrument->igetBestAsk() + 2)
            {
                if(_iCurrentOfferPrice != _iTheoOffer)
                {
                    _iPrevOfferPrice = _iCurrentOfferPrice;
                    _iCurrentOfferPrice = _iTheoOffer;
                }
            }
        }
    }

    if(_iCurrentBidExitPrice == 0)
    {
        _iCurrentBidExitPrice = _iTheoExitBid;
    }
    else
    {
        if(_iTheoExitBid < _iCurrentBidExitPrice)
        {
            _iPrevBidExitPrice = _iCurrentBidExitPrice;
            _iCurrentBidExitPrice = _iTheoExitBid;
        }
        else
        {
            if(_iTheoExitBid > _pInstrument->igetBestBid() - 2 || _iCurrentBidExitPrice > _pInstrument->igetBestBid() - 2)
            {
                if(_iCurrentBidExitPrice != _iTheoExitBid)
                {
                    _iPrevBidExitPrice = _iCurrentBidExitPrice;
                    _iCurrentBidExitPrice = _iTheoExitBid;
                }
            }
        }
    }

    if(_iCurrentOfferExitPrice == 500000)
    {
        _iCurrentOfferExitPrice = _iTheoExitOffer;
    }
    else
    {
        if(_iTheoExitOffer > _iCurrentOfferExitPrice)
        {
            _iPrevOfferExitPrice = _iCurrentOfferExitPrice;
            _iCurrentOfferExitPrice = _iTheoExitOffer;
        }
        else
        {
            if(_iTheoExitOffer < _pInstrument->igetBestAsk() + 2 || _iCurrentOfferExitPrice < _pInstrument->igetBestAsk() + 2)
            {
                if(_iCurrentOfferExitPrice != _iTheoExitOffer)
                {
                    _iPrevOfferExitPrice = _iCurrentOfferExitPrice;
                    _iCurrentOfferExitPrice = _iTheoExitOffer;
                }
            }
        }
    }

    if(_bEstablishSegEntryPos)
    {
        if(_iTheoPosition > _iPosition)
        {
            _iCurrentBidPrice = _pInstrument->igetBestBid();
        }

        if(_iTheoPosition < _iPosition)
        {
            _iCurrentOfferPrice = _pInstrument->igetBestAsk();
        }
    }
}


FairValueExecution::QuoteOrderPtr FairValueExecution::lookUpOrder(string& sOutputOrderType, int iOrderID)
{
    sOutputOrderType = "Unknown order";
    QuoteOrderPtr pResultOrder;

    bool bOrderFound = false;

    if(_pOpenBuyQuote.get() && _pOpenBuyQuote->igetOrderID() == iOrderID)
    {
        sOutputOrderType = "Open buy quote";
        pResultOrder = _pOpenBuyQuote;

        bOrderFound = true;
    }

    if(!bOrderFound)
    {
        if(_pOpenSellQuote.get() && _pOpenSellQuote->igetOrderID() == iOrderID)
        {
            sOutputOrderType = "Open sell quote";
            pResultOrder = _pOpenSellQuote;

            bOrderFound = true;
        }
    }

    if(!bOrderFound)
    {
        if(_pCloseBuyQuote.get() && _pCloseBuyQuote->igetOrderID() == iOrderID)
        {
            sOutputOrderType = "Close buy quote";
            pResultOrder = _pCloseBuyQuote;

            bOrderFound = true;
        }
    }

    if(!bOrderFound)
    {
        if(_pCloseSellQuote.get() && _pCloseSellQuote->igetOrderID() == iOrderID)
        {
            sOutputOrderType = "Close sell quote";
            pResultOrder = _pCloseSellQuote;

            bOrderFound = true;
        }
    }
/*
    if(!bOrderFound)
    {
        if(_pIOCOpenBuyOrder.get() && _pIOCOpenBuyOrder->igetOrderID() == iOrderID)
        {
            sOutputOrderType = "Open buy ioc";
            pResultOrder = _pIOCOpenBuyOrder;

            bOrderFound = true;
        }
    }

    if(!bOrderFound)
    {
        if(_pIOCOpenSellOrder.get() && _pIOCOpenSellOrder->igetOrderID() == iOrderID)
        {
            sOutputOrderType = "Open sell ioc";
            pResultOrder = _pIOCOpenSellOrder;

            bOrderFound = true;
        }
    }

    if(!bOrderFound)
    {
        if(_pIOCCloseBuyOrder.get() && _pIOCCloseBuyOrder>igetOrderID() == iOrderID)
        {
            sOutputOrderType = "Close buy ioc";
            pResultOrder = _pIOCCloseBuyOrder;

            bOrderFound = true;
        }
    }

    if(!bOrderFound)
    {
        if(_pIOCCloseSellOrder.get() && _pIOCCloseSellOrder->igetOrderID() == iOrderID)
        {
            sOutputOrderType = "Close sell ioc";
            pResultOrder = _pIOCCloseSellOrder;

            bOrderFound = true;
        }
    }
*/
    if(!bOrderFound)
    {
        sOutputOrderType = "Unknown order";
        pResultOrder.reset();
    }

    return pResultOrder;
}

void FairValueExecution::pullAllQuotes()
{
    pullOpenBuyQuote();
    pullExitBuyQuote();
    pullOpenSellQuote();
    pullExitSellQuote();
}

void FairValueExecution::pullAllOpenQuotes()
{
    pullOpenBuyQuote();
    pullOpenSellQuote();
}

void FairValueExecution::pullOpenBuyQuote()
{
    if(igetOpenBuyQuoteQty() != 0 && _pOpenBuyQuote->bquoteCanBeChanged())
    {
        (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Deleting open buy quote \n";
        if(_pOpenBuyQuote->bdeleteQuote())
        {
            (*_pLogger) << "Order delete submitted \n";
        }
        else
        {
            (*_pLogger) << "Failed to submit order delete \n";
        }
    }
}

void FairValueExecution::pullOpenSellQuote()
{
    if(igetOpenSellQuoteQty() != 0 && _pOpenSellQuote->bquoteCanBeChanged())
    {
        (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Deleting open sell quote \n";
        if(_pOpenSellQuote->bdeleteQuote())
        {
            (*_pLogger) << "Order delete submitted \n";
        }
        else
        {
            (*_pLogger) << "Failed to submit order delete \n";
        }
    }
}

void FairValueExecution::pullExitBuyQuote()
{
    if(igetCloseBuyQuoteQty() != 0 && _pCloseBuyQuote->bquoteCanBeChanged())
    {
        (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Deleting close buy quote \n";
        if(_pCloseBuyQuote->bdeleteQuote())
        {
            (*_pLogger) << "Order delete submitted \n";
        }
        else
        {
            (*_pLogger) << "Failed to submit order delete \n";
        }
    }
}

void FairValueExecution::pullExitSellQuote()
{
    if(igetCloseSellQuoteQty() != 0 && _pCloseSellQuote->bquoteCanBeChanged())
    {
        (*_pLogger) << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << " Deleting close sell quote \n";
        if(_pCloseSellQuote->bdeleteQuote())
        {
            (*_pLogger) << "Order delete submitted \n";
        }
        else
        {
            (*_pLogger) << "Failed to submit order delete \n";
        }
    }
}

void FairValueExecution::updateQuotePrice()
{
    if(_iCurrentBidExitPrice > _iPrevBidExitPrice || _iCurrentBidPrice > _iPrevBidPrice)
    {
        // amend sell orders first, as bid orders are improving to avoid self trade
        updateSellQuotePrice();
        updateBuyQuotePrice();
    }
    else
    {
        // amend buy orders first, as offer orders are improving to avoid self trade
        updateBuyQuotePrice();
        updateSellQuotePrice();
    }
}

void FairValueExecution::updateBuyQuotePrice()
{
    if(igetCloseBuyQuoteQty() != 0)
    {
        if(_pCloseBuyQuote->igetOrderPrice() != _iCurrentBidExitPrice)
        {
            if(_pCloseBuyQuote->bquoteCanBeChanged())
            {
                long iAmendSubmitTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();
                if(_pCloseBuyQuote->bchangeQuotePrice(_iCurrentBidExitPrice))
                {
                    (*_pLogger) << iAmendSubmitTime << " amended close buy quote id " << _pCloseBuyQuote->igetOrderID() << " from " << _pCloseBuyQuote->igetOrderPrice() << " to " << _iCurrentBidExitPrice << "\n";
                }
                else
                {
                    (*_pLogger) << iAmendSubmitTime << " failed to amend close buy quote id " << _pCloseBuyQuote->igetOrderID() << "\n";
                }
            }
        }
    }

    if(igetOpenBuyQuoteQty() != 0)
    {
        if(_pOpenBuyQuote->igetOrderPrice() != _iCurrentBidPrice)
        {
            if(_pOpenBuyQuote->bquoteCanBeChanged())
            {
                long iAmendSubmitTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();
                if(_pOpenBuyQuote->bchangeQuotePrice(_iCurrentBidPrice))
                {
                    (*_pLogger) << iAmendSubmitTime << " amended open buy quote id " << _pOpenBuyQuote->igetOrderID() << " from " << _pOpenBuyQuote->igetOrderPrice() << " to " << _iCurrentBidPrice << "\n";
                }
                else
                {
                    (*_pLogger) << iAmendSubmitTime << " failed to amend open buy quote id " << _pOpenBuyQuote->igetOrderID() << "\n";
                }
            }
        }
    }
}

void FairValueExecution::updateSellQuotePrice()
{
    if(igetCloseSellQuoteQty() != 0)
    {
        if(_pCloseSellQuote->igetOrderPrice() != _iCurrentOfferExitPrice)
        {
            if(_pCloseSellQuote->bquoteCanBeChanged())
            {
                long iAmendSubmitTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();
                if(_pCloseSellQuote->bchangeQuotePrice(_iCurrentOfferExitPrice))
                {
                    (*_pLogger) << iAmendSubmitTime << " amended close sell quote id " << _pCloseSellQuote->igetOrderID() << " from " << _pCloseSellQuote->igetOrderPrice() << " to " << _iCurrentOfferExitPrice << "\n";
                }
                else
                {
                    (*_pLogger) << iAmendSubmitTime << " failed to amend close sell order id " << _pCloseSellQuote->igetOrderID() << "\n";
                }
            }
        }
    }

    if(igetOpenSellQuoteQty() != 0)
    {
        if(_pOpenSellQuote->igetOrderPrice() != _iCurrentOfferPrice)
        {
            if(_pOpenSellQuote->bquoteCanBeChanged())
            {
                long iAmendSubmitTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable();
                if(_pOpenSellQuote->bchangeQuotePrice(_iCurrentOfferPrice))
                {
                    (*_pLogger) << iAmendSubmitTime << " amended open sell quote id " << _pOpenSellQuote->igetOrderID() << " from " << _pOpenSellQuote->igetOrderPrice() << " to " << _iCurrentOfferPrice << "\n";
                }
                else
                {
                    (*_pLogger) << iAmendSubmitTime << " failed to amend open sell order id " << _pOpenSellQuote->igetOrderID() << "\n";
                }
            }
        }
    }
}

bool FairValueExecution::bsubmitIOC(long iNetPosChange, long iOrderPrice, OrderType eOrderType)
{
    bool bOrderSubmitted = false;

    if(iNetPosChange != 0)
    {
        if(eOrderType == OPEN_BUY)
        {
            _pIOCOpenBuyOrder.reset(new IOCOrder(_pTargetAccount));
            bOrderSubmitted = _pIOCOpenBuyOrder->bsubmitOrder(iNetPosChange, iOrderPrice);
        }
        else if(eOrderType == OPEN_SELL)
        {
            _pIOCOpenSellOrder.reset(new IOCOrder(_pTargetAccount));
            bOrderSubmitted = _pIOCOpenSellOrder->bsubmitOrder(iNetPosChange, iOrderPrice);
        }
        else if(eOrderType == CLOSE_BUY)
        {
            _pIOCCloseBuyOrder.reset(new IOCOrder(_pTargetAccount));
            bOrderSubmitted = _pIOCCloseBuyOrder->bsubmitOrder(iNetPosChange, iOrderPrice);
        }
        else if(eOrderType == CLOSE_SELL)
        {
            _pIOCCloseSellOrder.reset(new IOCOrder(_pTargetAccount));
            bOrderSubmitted = _pIOCCloseSellOrder->bsubmitOrder(iNetPosChange, iOrderPrice);
        }
    }

    return bOrderSubmitted;
}

bool FairValueExecution::bsubmitQuote(long iNetPosChange, long iOrderPrice, OrderType eOrderType)
{
    bool bOrderSubmitted = false;

    if(iNetPosChange != 0)
    {
        if(eOrderType == OPEN_BUY)
        {
            _pOpenBuyQuote.reset(new QuoteOrder(_pTargetAccount));
            bOrderSubmitted = _pOpenBuyQuote->bsubmitQuote(iNetPosChange, iOrderPrice);
        }
        else if(eOrderType == OPEN_SELL)
        {
            _pOpenSellQuote.reset(new QuoteOrder(_pTargetAccount));
            bOrderSubmitted = _pOpenSellQuote->bsubmitQuote(iNetPosChange, iOrderPrice);
        }
        else if(eOrderType == CLOSE_BUY)
        {
            _pCloseBuyQuote.reset(new QuoteOrder(_pTargetAccount));
            bOrderSubmitted = _pCloseBuyQuote->bsubmitQuote(iNetPosChange, iOrderPrice);
        }
        else if(eOrderType == CLOSE_SELL)
        {
            _pCloseSellQuote.reset(new QuoteOrder(_pTargetAccount));
            bOrderSubmitted = _pCloseSellQuote->bsubmitQuote(iNetPosChange, iOrderPrice);
        }
    }

    return bOrderSubmitted;
}

bool FairValueExecution::bamendQuote(long iNetPosChange, long iOrderPrice, OrderType eOrderType)
{
    bool bOrderAmended = false;

    if(iNetPosChange != 0)
    {
        if(eOrderType == OPEN_BUY)
        {
            bOrderAmended = _pOpenBuyQuote->bchangeQuote(iOrderPrice, iNetPosChange);
        }
        else if(eOrderType == OPEN_SELL)
        {
            bOrderAmended = _pOpenSellQuote->bchangeQuote(iOrderPrice, iNetPosChange);
        }
        else if(eOrderType == CLOSE_BUY)
        {
            bOrderAmended = _pCloseBuyQuote->bchangeQuote(iOrderPrice, iNetPosChange);
        }
        else if(eOrderType == CLOSE_SELL)
        {
            bOrderAmended = _pCloseSellQuote->bchangeQuote(iOrderPrice, iNetPosChange);
        }
    }

    return bOrderAmended;
}

long FairValueExecution::igetOpenBuyQuoteQty()
{
    long iResult = 0;

    if(_pOpenBuyQuote.get())
    {
        iResult = _pOpenBuyQuote->igetOrderRemainQty();
    }

    return iResult;
}

long FairValueExecution::igetOpenSellQuoteQty()
{
    long iResult = 0;

    if(_pOpenSellQuote.get())
    {
        iResult = _pOpenSellQuote->igetOrderRemainQty();
    }

    return iResult;
}

long FairValueExecution::igetCloseBuyQuoteQty()
{
    long iResult = 0;

    if(_pCloseBuyQuote.get())
    {
        iResult = _pCloseBuyQuote->igetOrderRemainQty();
    }

    return iResult;
}

long FairValueExecution::igetCloseSellQuoteQty()
{
    long iResult = 0;

    if(_pCloseSellQuote.get())
    {
        iResult = _pCloseSellQuote->igetOrderRemainQty();
    }

    return iResult;
}

long FairValueExecution::igetOpenBuyIOCQty()
{
    long iResult = 0;

    if(_pIOCOpenBuyOrder.get())
    {
        iResult = _pIOCOpenBuyOrder->igetOrderRemainQty();
    }

    return iResult;
}

long FairValueExecution::igetOpenSellIOCQty()
{
    long iResult = 0;

    if(_pIOCOpenSellOrder.get())
    {
        iResult = _pIOCOpenSellOrder->igetOrderRemainQty();
    }

    return iResult;
}

long FairValueExecution::igetCloseBuyIOCQty()
{
    long iResult = 0;

    if(_pIOCCloseBuyOrder.get())
    {
        iResult = _pIOCCloseBuyOrder->igetOrderRemainQty();
    }

    return iResult;
}

long FairValueExecution::igetCloseSellIOCQty()
{
    long iResult = 0;

    if(_pIOCCloseSellOrder.get())
    {
        iResult = _pIOCCloseSellOrder->igetOrderRemainQty();
    }

    return iResult;
}

long FairValueExecution::igetTotalOpenOrderQty()
{
    return igetOpenBuyQuoteQty() + igetOpenSellQuoteQty() + igetCloseBuyQuoteQty() + igetCloseSellQuoteQty() + igetOpenBuyIOCQty() + igetOpenSellIOCQty() + igetCloseBuyIOCQty() + igetCloseSellIOCQty();
}

}
