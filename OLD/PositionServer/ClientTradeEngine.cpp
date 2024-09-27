#include <stdlib.h>
#include "PositionServer.h"
#include "ClientTradeEngine.h"
#include "ServerLogger.h"

namespace KO
{

ClientTradeEngine::ClientTradeEngine(boost::asio::io_service& io_service, PositionServer* pPositionServer)
:_pPositionServer(pPositionServer),
 _cSocket(io_service),
 _bInactiveConnection(false)
{
    _sUnprocessedMsg = "";
}

bool ClientTradeEngine::bisConnectionActive()
{
    return !_bInactiveConnection;
}

void ClientTradeEngine::onReceive(const boost::system::error_code& error, size_t size)
{
    if((error == boost::asio::error::eof) ||
       (error == boost::asio::error::connection_reset))
    {
        _bInactiveConnection = true;

        stringstream cLogStream;
        cLogStream << "Client " << _iClientIndex << " disconnected";
        ServerLogger::GetInstance()->newInfoMsg(cLogStream.str());
    }
    else
    {
        _readBuf[size] = '\0';
        _pPositionServer->newTCPMessageReceived();
        _sUnprocessedMsg = _sUnprocessedMsg + _readBuf;

        stringstream cReplyMsgStream;

        long iLastDelimiterPoistion = -1;
        for(int i = 0; i < _sUnprocessedMsg.length(); i++)
        {
            if(_sUnprocessedMsg[i] == '|')
            {
                long iNewMessageLength = i - iLastDelimiterPoistion - 1;
                string sNewMsg = _sUnprocessedMsg.substr(iLastDelimiterPoistion+1, iNewMessageLength);

                iLastDelimiterPoistion = i;

                std::istringstream cMsgStream(sNewMsg);
                string sElement;

                bool bMsgParsed = true;
                while(std::getline(cMsgStream, sElement, ';'))
                {
                    if(sElement.compare("FILL") == 0)
                    {
                        string sMsgID;
                        string sProduct;
                        string sAccount;
                        long iQty;

                        bMsgParsed = bMsgParsed && std::getline(cMsgStream, sElement, ';');
                        sMsgID = sElement;

                        bMsgParsed = bMsgParsed && std::getline(cMsgStream, sElement, ';');
                        sProduct = sElement;

                        bMsgParsed = bMsgParsed && std::getline(cMsgStream, sElement, ';');
                        sAccount = sElement;

                        bMsgParsed = bMsgParsed && std::getline(cMsgStream, sElement, ';');
                        iQty = atoi(sElement.c_str());

                        if(bMsgParsed)
                        {
                            if(_pPositionServer->newFill(sMsgID, sProduct, sAccount, iQty))
                            {
                                stringstream cLogStream;
                                cLogStream << "New fill " << sMsgID << " for product " << sProduct << " account " << sAccount << " qty " << iQty;
                                ServerLogger::GetInstance()->newInfoMsg(cLogStream.str());
                            }
                            else
                            {
                                stringstream cLogStream;
                                cLogStream << "Ignore duplicated fill " << sMsgID << " for product " << sProduct << " account " << sAccount << " qty " << iQty;
                                ServerLogger::GetInstance()->newWarningMsg(cLogStream.str());
                            }
                              
                            stringstream cNewReply;
                            cNewReply << "FILLACK;" << sMsgID << "|"; 
                            if(cReplyMsgStream.str().length() + cNewReply.str().length() > 262144)
                            {
                                boost::asio::async_write(_cSocket, boost::asio::buffer(cReplyMsgStream.str()), boost::bind(&ClientTradeEngine::onSend, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
                                cReplyMsgStream.str("");
                            }

                            cReplyMsgStream << cNewReply.str();
                        }
                        else
                        {
                            break;
                        }
                    }
                    else if(sElement.compare("POS_REQ") == 0)
                    {
                        string sMsgID;
                        string sProduct;
                        string sAccount;

                        bMsgParsed = bMsgParsed && std::getline(cMsgStream, sElement, ';');
                        sMsgID = sElement;

                        bMsgParsed = bMsgParsed && std::getline(cMsgStream, sElement, ';');
                        sProduct = sElement;

                        bMsgParsed = bMsgParsed && std::getline(cMsgStream, sElement, ';');
                        sAccount = sElement;

                        if(bMsgParsed)
                        {
                            long iPosition = _pPositionServer->newPositionRequest(sProduct, sAccount);

                            stringstream cNewReply;
                            cNewReply << "POS;" << sMsgID << ";" << sProduct << ";" << sAccount << ";" << iPosition << "|"; 

                            if(cReplyMsgStream.str().length() + cNewReply.str().length() > 262144)
                            {
                                boost::asio::async_write(_cSocket, boost::asio::buffer(cReplyMsgStream.str()), boost::bind(&ClientTradeEngine::onSend, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
                                cReplyMsgStream.str("");
                            }

                            cReplyMsgStream << cNewReply.str();

                            stringstream cLogStream; 
                            cLogStream << "Position is " << iPosition << " for requested product " << sProduct << " account " << sAccount << " is " << iPosition;
                            ServerLogger::GetInstance()->newInfoMsg(cLogStream.str());
                        }
                        else
                        {
                            break;
                        }
                    }
                    else if(sElement.compare("") != 0)
                    {
                        bMsgParsed = false;
                        break;
                    }
                }

                if(bMsgParsed == false)
                {
                    stringstream cLogStream;
                    cLogStream << "Failed to parse client message: " << sNewMsg;
                    ServerLogger::GetInstance()->newErrorMsg(cLogStream.str());
                }
            }
        }

        if(iLastDelimiterPoistion != -1)
        {
            _sUnprocessedMsg = _sUnprocessedMsg.erase(0, iLastDelimiterPoistion+1);
        }

        if(cReplyMsgStream.str().length() > 0)
        {
            boost::asio::async_write(_cSocket, boost::asio::buffer(cReplyMsgStream.str()), boost::bind(&ClientTradeEngine::onSend, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        }

        _cSocket.async_read_some(boost::asio::buffer(_readBuf, _iMaxMsgLenth-1), boost::bind(&ClientTradeEngine::onReceive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
}

void ClientTradeEngine::onSend(const boost::system::error_code& error, size_t size)
{
    if(error)
    {
        _bInactiveConnection = true;
        stringstream cLogStream;
        cLogStream << "Sending reply failed. " << error.message() << "";
        ServerLogger::GetInstance()->newErrorMsg(cLogStream.str());
    }
}

boost::shared_ptr<ClientTradeEngine> ClientTradeEngine::cCreateInstance(boost::asio::io_service& io_service, PositionServer* pPositionServer)
{
    return boost::shared_ptr<ClientTradeEngine>(new ClientTradeEngine(io_service, pPositionServer));
}

tcp::socket& ClientTradeEngine::cgetSocket()
{
    return _cSocket;
}

void ClientTradeEngine::init()
{
    _cSocket.async_read_some(boost::asio::buffer(_readBuf, _iMaxMsgLenth-1), boost::bind(&ClientTradeEngine::onReceive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void ClientTradeEngine::setClientIndex(int index)
{
    _iClientIndex = index;
}

}
