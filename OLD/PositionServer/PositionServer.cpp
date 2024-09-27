#include "PositionServer.h"
#include "ServerLogger.h"
#include <boost/date_time/posix_time/posix_time.hpp>

namespace KO
{

PositionServer::PositionServer(boost::asio::io_service& io_service, long iPort)
: _cAcceptor(io_service, tcp::endpoint(tcp::v4(), iPort))
{
    stringstream cLogStream;
    openForNextClient();
    cLogStream << "Ready accepting clients at port " << iPort;
    ServerLogger::GetInstance()->newInfoMsg(cLogStream.str());
}

PositionServer::~PositionServer()
{

}

void PositionServer::newTCPMessageReceived()
{
    boost::posix_time::ptime epoch(boost::gregorian::date(1970,1,1));
    boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
    KOEpochTime cKONow (0, (now - epoch).total_microseconds());
   
    _qTCPReceivingHistorty.push_back(cKONow);

    while(_qTCPReceivingHistorty.size() != 0 && _qTCPReceivingHistorty.front() <= cKONow - KOEpochTime(1,0))
    {
        _qTCPReceivingHistorty.pop_front();
    }

    if(_qTCPReceivingHistorty.size() >= 1000)
    {
        stringstream cLogStream;
        cLogStream << "More than 1000 TCP messages received in 1 seconds!";
        ServerLogger::GetInstance()->newErrorMsg(cLogStream.str());    
    }
}

void PositionServer::openForNextClient()
{
    ClientTradeEnginePtr pNewClient = ClientTradeEngine::cCreateInstance(_cAcceptor.get_io_service(), this);

    _cAcceptor.async_accept(pNewClient->cgetSocket(), boost::bind(&PositionServer::onNewClient, this, pNewClient, boost::asio::placeholders::error));
}

void PositionServer::onNewClient(ClientTradeEnginePtr pNewClient, const boost::system::error_code& error)
{
    if (!error)
    {
        pNewClient->init();
        vConnectedClients.push_back(pNewClient);
        pNewClient->setClientIndex(vConnectedClients.size());

        stringstream cLogStream;
        cLogStream << "New client connection " << vConnectedClients.size();
        ServerLogger::GetInstance()->newInfoMsg(cLogStream.str());
    }
    else
    {
        for(vector<ClientTradeEnginePtr>::iterator itr = vConnectedClients.begin();
            itr != vConnectedClients.end();)
        {
            if((*itr)->bisConnectionActive() == false)
            {
                itr = vConnectedClients.erase(itr);
            }
            else
            {
                itr++;
            }
        }
    }

    openForNextClient();
}

bool PositionServer::newFill(string sMsgID, string sProduct, string sAccount, long iQty)
{
    bool bResult = false;

    bool bFillIDExists = false;
    for(std::vector<string>::iterator itr = _mAckedFillIDs.begin(); itr != _mAckedFillIDs.end(); itr++)
    {
        if(sMsgID.compare(*itr) == 0)
        {
            bFillIDExists = true;
            break;
        }
    }

    if(bFillIDExists == false)
    {
        std::string sKey = sProduct + "-" + sAccount;

        map<string, long>::iterator posItr = _mProductAccountPos.find(sKey);

        if(posItr != _mProductAccountPos.end())
        {
            _mProductAccountPos[sKey] = _mProductAccountPos[sKey] + iQty;
        }
        else
        {
            _mProductAccountPos.insert(std::pair<string, long>(sKey, iQty));
        }

        _mAckedFillIDs.push_back(sMsgID);

        bResult = true;
    }
    else
    {
        bResult = false;
    }

    return bResult;
}

long PositionServer::newPositionRequest(std::string sProduct, std::string sAccount)
{
    long iResult = 0;

    if(sAccount.compare("ALL") == 0)
    {
        for(map<string, long>::iterator posItr = _mProductAccountPos.begin();
            posItr != _mProductAccountPos.end();
            posItr++)
        {
            string sPosKey = posItr->first;
            std::istringstream cPosKeyStream (sPosKey);

            string sItrProduct;
            string sItrAccount;
            std::getline(cPosKeyStream, sItrProduct, '-');
            std::getline(cPosKeyStream, sItrAccount, '-');

            if(sItrProduct.compare(sProduct) == 0)
            {
                iResult = iResult + posItr->second;
            }
        }        
    }
    else
    {
        string sKey = sProduct + "-" + sAccount;

        map<string, long>::iterator posItr = _mProductAccountPos.find(sKey);

        if(posItr != _mProductAccountPos.end())
        {
            iResult = posItr->second;
        }
    }

    return iResult;
}

}
