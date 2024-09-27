#ifndef PositionServerConnection_H
#define PositionServerConnection_H

#include "KOEpochTime.h"
#include "ErrorHandler.h"
#include "SimPositionServer.h"
#include <base-lib/VelioSessionManager.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

using std::vector;

namespace KO
{

class SchedulerBase;
class TradeEngineBase;

class PositionServerConnection
{
public:
    PositionServerConnection();
    bool bInit(const string& sServerAddress, SchedulerBase* pScheduler);
    void newFill(const string& sProduct, const string& sAccount, long iQty);
    void requestStartupPosition(const string& sProduct, const string& sAccount, TradeEngineBase* pParent); 
    void requestPosition(const string& sProduct, const string& sAccount, TradeEngineBase* pParent); 
    void wakeup(KOEpochTime cCallTime);

    void socketRead();
    void socketError();
    void socketClose();
private:
    struct NewPositionRequest
    {
        string sMessageID;
        string sProduct;
        string sAccount;
        bool   bIsStartUpPosition;
        int    iTimeoutThresh;
        TradeEngineBase* pParent;
        KOEpochTime cMsgSentTime;
    };

    struct NewFill
    {
        string sMessageID;
        string sProduct;
        string sAccount;
        long iQty;
        int iTimeoutThresh;
        TradeEngineBase* pParent;
        KOEpochTime cMsgSentTime;
    };

    void connectToServer(string& sError);
    string sgenerateMsgID(string sProduct, string sAccount);

    bool _bPosServerConnected;
    bool _bServerAddressValid;
    bool _bUseSimPosServer;

    string _sServerAddress;
    SimPositionServer cSimPositionServer;

    vector<boost::shared_ptr<NewFill> > _vUnsentFills;
    vector<boost::shared_ptr<NewPositionRequest> > _vUnsentPositionRequests;

    vector<boost::shared_ptr<NewFill> > _vPendingFills;
    vector<boost::shared_ptr<NewPositionRequest> > _vPendingPositionRequests;

    VelioSessionManager* _pVelioSessionManager;
    SchedulerBase* _pScheduler;

    int _iServerSocketID;

    static const char charList[62];

    bool _bConnectionTimedout;
    bool _bFailedToSendFill;
    bool _bFailedToSendPosReq;

    string _sUnprocessedMsg;

};

}

#endif
