#ifndef OrderParentInterface_H
#define OrderParentInterface_H

namespace KO
{

//static unsigned int igetNextOrderID()
//{
//    static int iNextOrderID = 0;
//    return iNextOrderID++;
//}

class OrderParentInterface
{
public:
    OrderParentInterface(){};

    virtual void orderConfirmHandler(int iOrderID) = 0;
    virtual void orderFillHandler(int iOrderID, long iFilledQty, double dPrice) = 0;
    virtual void orderRejectHandler(int iOrderID) = 0;
    virtual void orderDeleteHandler(int iOrderID) = 0;
    virtual void orderDeleteRejectHandler(int iOrderID) = 0;
    virtual void orderAmendRejectHandler(int iOrderID) = 0;
    virtual void orderCriticalErrorHandler(int iOrderID) = 0;
    virtual void orderUnexpectedConfirmHandler(int iOrderID) = 0;
    virtual void orderUnexpectedDeleteHandler(int iOrderID) = 0;
};

}

#endif
