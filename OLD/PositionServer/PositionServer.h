#ifndef PositionServer_H
#define PositionServer_H

#include "ClientTradeEngine.h"
#include "KOEpochTime.h"

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/map.hpp>

using std::vector;
using std::map;
using std::string;

namespace KO
{

class PositionServer
{
public:
    PositionServer(boost::asio::io_service& io_service, long iPort);
    ~PositionServer();
    void newTCPMessageReceived();
    bool newFill(string sMsgID, string sProduct, string sAccount, long iQty);
    long newPositionRequest(string sProduct, string sAccount);

private:
    void openForNextClient();
    void onNewClient(ClientTradeEnginePtr pNewClient, const boost::system::error_code& error);

    tcp::acceptor _cAcceptor;
    vector<ClientTradeEnginePtr> vConnectedClients;

    vector<string> _mAckedFillIDs;
    map<string, long> _mProductAccountPos;

    deque<KOEpochTime> _qTCPReceivingHistorty;
};

};

#endif /* PositionServer_H */
