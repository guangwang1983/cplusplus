#include "SchedulerBase.h"
#include "TradeEngineBase.h"
#include "PositionServerConnection.h"
#include "SystemClock.h"
#include <base-lib/VelioSessionManager.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

using std::stringstream;
using std::cerr;

namespace KO
{

const char PositionServerConnection::charList[62] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};

PositionServerConnection::PositionServerConnection()
{
    _bPosServerConnected = false;
    _bUseSimPosServer = false;
    _bConnectionTimedout = false;
    _bFailedToSendFill = false;
    _bFailedToSendPosReq = false;

    _sUnprocessedMsg = "";
}

bool PositionServerConnection::bInit(const string& sServerAddress, SchedulerBase* pScheduler)
{
    bool bResult = false;

    _sServerAddress = sServerAddress;
    _pScheduler = pScheduler;

    string sError;
    connectToServer(sError);

    bResult = _bPosServerConnected;

    if(!bResult)
    {
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", sError);
    }

    return bResult;
}

string PositionServerConnection::sgenerateMsgID(string sProduct, string sAccount)
{
    string sResult = "";

    sResult = sAccount + "-" + sProduct + "-";

    for(int i = 0; i < 10; i++)
    {
        sResult = sResult + charList[rand() % 62];
    }

    return sResult;
}

void PositionServerConnection::socketRead()
{
    int iNumByteToRead = 262144;

    char receivebuf [iNumByteToRead];
    memset(receivebuf, '\0', iNumByteToRead);

    if(read(_iServerSocketID, receivebuf, iNumByteToRead-1) == 0)
    {
        stringstream cErrorMsgStream;
        cErrorMsgStream << "Position server connection is down. Failed to read from socket.";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cErrorMsgStream.str());

        _bPosServerConnected = false;
        close(_iServerSocketID);
    }
    else
    {
        _sUnprocessedMsg = _sUnprocessedMsg + receivebuf;

        for(int i = 0; i < _sUnprocessedMsg.length(); i++)
        {
            if(_sUnprocessedMsg[i] == '|')
            {
                string sNewMsg = _sUnprocessedMsg.substr(0, i);
                _sUnprocessedMsg = _sUnprocessedMsg.erase(0, i+1);
                i = 0;

                std::istringstream cMsgStream(sNewMsg);
                string sElement;
                bool bMsgParsed = true;
                while(std::getline(cMsgStream, sElement, ';'))
                {
                    if(sElement.compare("POS") == 0)
                    {
                        string sReplyMsgID = "";
                        string sReplyProduct = "";
                        string sReplyAccount = "";
                        long iReplyPosition = 0;

                        bMsgParsed = bMsgParsed && std::getline(cMsgStream, sElement, ';');
                        sReplyMsgID = sElement;
                        bMsgParsed = bMsgParsed && std::getline(cMsgStream, sElement, ';');
                        sReplyProduct = sElement;
                        bMsgParsed = bMsgParsed && std::getline(cMsgStream, sElement, ';');
                        sReplyAccount = sElement;
                        bMsgParsed = bMsgParsed && std::getline(cMsgStream, sElement, ';');
                        iReplyPosition = atoi(sElement.c_str());

                        if(!bMsgParsed)
                        {
                            break;
                        }
                        else
                        {
                            bool bPendingRequestFound = false;
                            for(vector<boost::shared_ptr<NewPositionRequest> >::iterator itr = _vPendingPositionRequests.begin();
                                itr != _vPendingPositionRequests.end();)
                            {
                                if((*itr)->sMessageID.compare(sReplyMsgID) == 0)
                                {
                                    if((*itr)->sProduct.compare(sReplyProduct) == 0)
                                    {
                                        if((*itr)->sAccount.compare(sReplyAccount) == 0)
                                        {
                                            if((*itr)->bIsStartUpPosition)
                                            {
                                                (*itr)->pParent->assignStartupPosition((*itr)->sProduct, (*itr)->sAccount, iReplyPosition, false);
                                            }
                                            else
                                            {
                                                (*itr)->pParent->positionRequestCallback((*itr)->sProduct, (*itr)->sAccount, iReplyPosition, false);
                                            }
                                        }
                                        else
                                        {
                                            stringstream cErrorMsgStream;
                                            cErrorMsgStream << "Failed to match account for position request reply " << sReplyMsgID << ". Requested account: " << (*itr)->sAccount << " Replied Account: " << sReplyAccount;
                                            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cErrorMsgStream.str());
                                        }
                                    }
                                    else
                                    {
                                        stringstream cErrorMsgStream;
                                        cErrorMsgStream << "Failed to match product for position request reply " << sReplyMsgID << ". Requested product: " << (*itr)->sProduct << " Replied product: " << sReplyProduct;
                                        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cErrorMsgStream.str());
                                    }

                                    bPendingRequestFound = true;
                                    itr = _vPendingPositionRequests.erase(itr);
                                    _bConnectionTimedout = false;
                                    break;
                                }
                                else
                                {
                                    itr++;
                                }
                            }

                            if(bPendingRequestFound == false)
                            {
                                stringstream cWarningMsgStream;
                                cWarningMsgStream << "Discard unknown position request reply. Message ID " << sReplyMsgID;
                                ErrorHandler::GetInstance()->newWarningMsg("0", "ALL", "ALL", cWarningMsgStream.str());
                            }
                        }
                    }
                    else if(sElement.compare("FILLACK") == 0)
                    {
                        string sReplyMsgID = "";

                        bMsgParsed = bMsgParsed && std::getline(cMsgStream, sElement, ';');
                        sReplyMsgID = sElement;

                        if(!bMsgParsed)
                        {
                            break;
                        }
                        else
                        {
                            bool bPendingFillFound = false;
                            for(vector<boost::shared_ptr<NewFill> >::iterator itr = _vPendingFills.begin();
                                itr != _vPendingFills.end();)
                            {
                                if((*itr)->sMessageID.compare(sReplyMsgID) == 0)
                                {
                                    bPendingFillFound = true;
                                    _vPendingFills.erase(itr);
                                    _bConnectionTimedout = false;
                                    break;
                                }
                                else
                                {
                                    itr++;
                                }
                            }

                            if(bPendingFillFound == false)
                            {
                                stringstream cWarningMsgStream;
                                cWarningMsgStream << "Discard unknown fill acknowledgement. Message ID " << sReplyMsgID;
                                ErrorHandler::GetInstance()->newWarningMsg("0", "ALL", "ALL", cWarningMsgStream.str());
                            }
                        }
                    }
                    else if(sElement.length() > 0)
                    {
                        break;
                    }
                }

                if(bMsgParsed == false)
                {
                    stringstream cErrorMsgStream;
                    cErrorMsgStream << "Failed to parse position server message: " << sNewMsg << "\n";
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cErrorMsgStream.str());
                }
            }
        }
    }
}

void PositionServerConnection::socketError()
{
    stringstream cErrorMsgStream; 
    cErrorMsgStream << "Unknow position server connection socket error. ";
    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cErrorMsgStream.str());
}

void PositionServerConnection::socketClose()
{
    _bPosServerConnected = false;
    close(_iServerSocketID);

    stringstream cErrorMsgStream; 
    cErrorMsgStream << "Position server connection is down. Socket closed. ";
    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cErrorMsgStream.str());
}

void PositionServerConnection::newFill(const string& sProduct, const string& sAccount, long iQty)
{
    boost::shared_ptr<NewFill> pNewFill (new NewFill);
    pNewFill->sMessageID = sgenerateMsgID(sProduct, sAccount);
    pNewFill->sProduct = sProduct;
    pNewFill->sAccount = sAccount;
    pNewFill->iTimeoutThresh = 5;
    pNewFill->iQty = iQty;
    pNewFill->cMsgSentTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
    _vUnsentFills.push_back(pNewFill);
}

void PositionServerConnection::requestStartupPosition(const string& sProduct, const string& sAccount, TradeEngineBase* pParent)
{
    boost::shared_ptr<NewPositionRequest> pNewPositionRequest (new NewPositionRequest);
    pNewPositionRequest->sMessageID = sgenerateMsgID(sProduct, sAccount);
    pNewPositionRequest->sProduct = sProduct;
    pNewPositionRequest->sAccount = sAccount;
    pNewPositionRequest->bIsStartUpPosition = true;
    pNewPositionRequest->pParent = pParent;
    pNewPositionRequest->iTimeoutThresh = 5;
    pNewPositionRequest->cMsgSentTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
    _vUnsentPositionRequests.push_back(pNewPositionRequest);
}

void PositionServerConnection::requestPosition(const string& sProduct, const string& sAccount, TradeEngineBase* pParent)
{
    boost::shared_ptr<NewPositionRequest> pNewPositionRequest (new NewPositionRequest);
    pNewPositionRequest->sMessageID = sgenerateMsgID(sProduct, sAccount);
    pNewPositionRequest->sProduct = sProduct;
    pNewPositionRequest->sAccount = sAccount;
    pNewPositionRequest->bIsStartUpPosition = false;
    pNewPositionRequest->pParent = pParent;
    pNewPositionRequest->iTimeoutThresh = 5;
    pNewPositionRequest->cMsgSentTime = SystemClock::GetInstance()->cgetCurrentKOEpochTime();
    _vUnsentPositionRequests.push_back(pNewPositionRequest);
}

void PositionServerConnection::wakeup(KOEpochTime cCallTime)
{
    if(!_bUseSimPosServer)
    {
        if(!_bPosServerConnected)
        {
            string sError;

            if(cCallTime.igetPrintable() % 300 == 0)
            {
                connectToServer(sError);
            }
        }

        for(vector<boost::shared_ptr<NewFill> >::iterator itr = _vPendingFills.begin();
            itr != _vPendingFills.end();)
        {
            if(SystemClock::GetInstance()->cgetCurrentKOEpochTime() > (*itr)->cMsgSentTime + KOEpochTime((*itr)->iTimeoutThresh, 0))
            {
                if(_bConnectionTimedout == false)
                {
                    _bConnectionTimedout = true;
                    stringstream cStringStream;
                    cStringStream << "Fill acknowledgement " << (*itr)->sMessageID << " timed out.";
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                }

                _vUnsentFills.push_back(*itr);
                itr = _vPendingFills.erase(itr);
            }
            else
            {
                itr++;
            }
        }

        for(vector<boost::shared_ptr<NewPositionRequest> >::iterator itr = _vPendingPositionRequests.begin();
            itr != _vPendingPositionRequests.end();)
        {
            if(SystemClock::GetInstance()->cgetCurrentKOEpochTime() > (*itr)->cMsgSentTime + KOEpochTime((*itr)->iTimeoutThresh, 0))
            {
                if(_bConnectionTimedout == false)
                {
                    _bConnectionTimedout = true;
                    stringstream cStringStream;
                    cStringStream << "Position request " << (*itr)->sMessageID << " timed out.";
                    ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                }

                if((*itr)->bIsStartUpPosition)
                {
                    (*itr)->pParent->assignStartupPosition((*itr)->sProduct, (*itr)->sAccount, 0, true);
                }
                else
                {
                    (*itr)->pParent->positionRequestCallback((*itr)->sProduct, (*itr)->sAccount, 0, true);
                }

                itr = _vPendingPositionRequests.erase(itr);
            }
            else
            {
                itr++;
            }
        }

        if(_bPosServerConnected)
        {
            long iNumByteSent = 0;

            stringstream cFillMsgStream;
           
            vector<boost::shared_ptr<NewFill> > vPackagedFills;
            while(_vUnsentFills.size() > 0)
            {
                stringstream cNewReply;
                cNewReply << "FILL;" << _vUnsentFills.back()->sMessageID << ";" << _vUnsentFills.back()->sProduct << ";" << _vUnsentFills.back()->sAccount << ";" << _vUnsentFills.back()->iQty << "|";
                vPackagedFills.push_back(_vUnsentFills.back());
                _vUnsentFills.pop_back();

                if(cFillMsgStream.str().length() + cNewReply.str().length() > 262144)
                {
                    cFillMsgStream << "\0";
                    iNumByteSent = write(_iServerSocketID, cFillMsgStream.str().c_str(), cFillMsgStream.str().length());

                    if(iNumByteSent > 0)
                    {
                        _bFailedToSendFill = false;
                        _vPendingFills.insert(_vPendingFills.end(), vPackagedFills.begin(), vPackagedFills.end());
                    }
                    else
                    {
                        _vUnsentFills.insert(_vUnsentFills.end(), vPackagedFills.begin(), vPackagedFills.end());

                        if(_bFailedToSendFill == false)
                        {
                            _bFailedToSendFill = true;
                            stringstream cStringStream;
                            cStringStream << "Failed to send fill messages to position server.";
                            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                        }
                        break;
                    }

                    vPackagedFills.clear();
                    cFillMsgStream.str("");
                }

                cFillMsgStream << cNewReply.str();                 
            }

            if(cFillMsgStream.str().length() != 0)
            {
                cFillMsgStream << "\0";
                iNumByteSent = write(_iServerSocketID, cFillMsgStream.str().c_str(), cFillMsgStream.str().length());

                if(iNumByteSent > 0)
                {                            
                    _bFailedToSendFill = false;
                    _vPendingFills.insert(_vPendingFills.end(), vPackagedFills.begin(), vPackagedFills.end());
                }
                else
                {
                    _vUnsentFills.insert(_vUnsentFills.end(), vPackagedFills.begin(), vPackagedFills.end());

                    if(_bFailedToSendFill == false)
                    {
                        _bFailedToSendFill = true;
                        stringstream cStringStream;
                        cStringStream << "Failed to send fill messages to position server.";
                        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                    }
                }
            }

            vPackagedFills.clear();
            cFillMsgStream.str("");

            if(_bPosServerConnected)
            {
                stringstream cPosRequestMsgStream;
                vector<boost::shared_ptr<NewPositionRequest> > vPackagedPosRequests;
                while(_vUnsentPositionRequests.size() != 0)
                {
                    stringstream cNewReply;
                    cNewReply << "POS_REQ;" << _vUnsentPositionRequests.back()->sMessageID << ";" << _vUnsentPositionRequests.back()->sProduct << ";" << _vUnsentPositionRequests.back()->sAccount << "|";

                    vPackagedPosRequests.push_back(_vUnsentPositionRequests.back());
                    _vUnsentPositionRequests.pop_back();

                    if(cPosRequestMsgStream.str().length() + cNewReply.str().length() > 262144)
                    {
                        cPosRequestMsgStream << "\0";
                        iNumByteSent = write(_iServerSocketID, cPosRequestMsgStream.str().c_str(), cPosRequestMsgStream.str().length());

                         if(iNumByteSent > 0)
                        {
                            _bFailedToSendPosReq = false;
                            _vPendingPositionRequests.insert(_vPendingPositionRequests.end(), vPackagedPosRequests.begin(), vPackagedPosRequests.end());
                        }
                        else
                        {
                            _vUnsentPositionRequests.insert(_vUnsentPositionRequests.end(), _vUnsentPositionRequests.begin(), _vUnsentPositionRequests.end());

                            if(_bFailedToSendPosReq == false)
                            {
                                _bFailedToSendPosReq = true;
                                stringstream cStringStream;
                                cStringStream << "Failed to send position request messages to position server.";
                                ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                            }
                            break;
                        }

                        vPackagedPosRequests.clear();
                        cPosRequestMsgStream.str("");
                    }

                    cPosRequestMsgStream << cNewReply.str();
                }

                if(cPosRequestMsgStream.str().length() != 0)
                {
                    cPosRequestMsgStream << "\0";

                    iNumByteSent = write(_iServerSocketID, cPosRequestMsgStream.str().c_str(), cPosRequestMsgStream.str().length());

                     if(iNumByteSent > 0)
                    {
                        _bFailedToSendPosReq = false;
                        _vPendingPositionRequests.insert(_vPendingPositionRequests.end(), vPackagedPosRequests.begin(), vPackagedPosRequests.end());
                    }
                    else
                    {
                        _vUnsentPositionRequests.insert(_vUnsentPositionRequests.end(), _vUnsentPositionRequests.begin(), _vUnsentPositionRequests.end());

                        if(_bFailedToSendPosReq == false)
                        {
                            _bFailedToSendPosReq = true;
                            stringstream cStringStream;
                            cStringStream << "Failed to send position request messages to position server.";
                            ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
                        }
                    }
                }

                vPackagedPosRequests.clear();
                cPosRequestMsgStream.str("");
            }
        }
    }
    else
    {
        for(vector<boost::shared_ptr<NewFill> >::iterator itr = _vUnsentFills.begin();
                itr != _vUnsentFills.end();
                itr++)
        {
            cSimPositionServer.newFill((*itr)->sProduct, (*itr)->sAccount, (*itr)->iQty); 
        }
        _vUnsentFills.clear();

        for(vector<boost::shared_ptr<NewPositionRequest> >::iterator itr = _vUnsentPositionRequests.begin();
                itr != _vUnsentPositionRequests.end();
                itr++)
        {
            long iPostion = cSimPositionServer.igetPosition((*itr)->sProduct, (*itr)->sAccount);
    
            if((*itr)->bIsStartUpPosition)
            {
                (*itr)->pParent->assignStartupPosition((*itr)->sProduct, (*itr)->sAccount, iPostion, false);
            }
            else
            {
                (*itr)->pParent->positionRequestCallback((*itr)->sProduct, (*itr)->sAccount, iPostion, false);
            }
        }
        _vUnsentPositionRequests.clear();
    }
}

void PositionServerConnection::connectToServer(string& sError)
{
    sError = "";

    if(_sServerAddress.compare("SimPositionServer") == 0)
    {
        _bUseSimPosServer = true;
        _bPosServerConnected = true;
    }
    else
    {
        _bUseSimPosServer = false;
        int iPortSeparatorPos = _sServerAddress.find(":");
        string sIPAddress = _sServerAddress.substr(0, iPortSeparatorPos);
        string sPort = _sServerAddress.substr(iPortSeparatorPos+1, _sServerAddress.length() - (iPortSeparatorPos + 1));

        _iServerSocketID = socket(AF_INET, SOCK_STREAM, 0);

        struct sockaddr_in clientAddress;
        struct hostent* client;

        char shostName [256];
        gethostname(shostName, 256);

        client = gethostbyname(shostName);

        bzero((char *) &clientAddress, sizeof(clientAddress));

        clientAddress.sin_family = AF_INET;
        bcopy((char *)client->h_addr, (char *)&clientAddress.sin_addr.s_addr, client->h_length);

        long iClientPortNumber = 20000 + rand() % 10000;

        if(iClientPortNumber != atoi(sPort.c_str()))
        {
            clientAddress.sin_port = htons(iClientPortNumber);

            long iBindResult = bind(_iServerSocketID, (struct sockaddr *) &clientAddress, sizeof(clientAddress));

            if(iBindResult == 0)
            {
                struct sockaddr_in serv_addr;
                struct hostent* server;

                server = gethostbyname(sIPAddress.c_str());

                bzero((char *) &serv_addr, sizeof(serv_addr));

                serv_addr.sin_family = AF_INET;
                bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);

                serv_addr.sin_port = htons(atoi(sPort.c_str()));

                long iResult = connect(_iServerSocketID, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

                if(iResult < 0) 
                {
                    _bPosServerConnected = false;

                    stringstream cStringStream;
                    cStringStream << "Cannot connect to position server. Address: " << _sServerAddress << ". Error: " << strerror(errno);
                    sError = cStringStream.str();
                    close(_iServerSocketID);
                }
                else
                {
                    _bPosServerConnected = true;

                    _bConnectionTimedout = false;
                    _bFailedToSendFill = false;
                    _bFailedToSendPosReq = false;

                    _pScheduler->registerPositionServerSocket(_iServerSocketID);

                    stringstream cStringStream;
                    cStringStream << "Position server connected at address: " << _sServerAddress;
                    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());
                }
            }
        }
    }
}

}
