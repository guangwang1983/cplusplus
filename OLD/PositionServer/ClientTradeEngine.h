#ifndef ClientTradeEngine_H
#define ClientTradeEngine_H

#include <ctime>
#include <iostream>
#include <string>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

using namespace std;
using boost::asio::ip::tcp;

namespace KO
{

class PositionServer;

class ClientTradeEngine : public boost::enable_shared_from_this<ClientTradeEngine>
{
public:
    static boost::shared_ptr<ClientTradeEngine> cCreateInstance(boost::asio::io_service& io_service, PositionServer* pPositionServer);

    tcp::socket& cgetSocket();
    void init();
    void setClientIndex(int index);
    bool bisConnectionActive();

private:
    ClientTradeEngine(boost::asio::io_service& io_service, PositionServer* pPositionServer);
    void onReceive(const boost::system::error_code& error, size_t size);
    void onSend(const boost::system::error_code& error, size_t size);

    PositionServer* _pPositionServer;

    tcp::socket _cSocket;

    static const int _iMaxMsgLenth = 500;
    char _readBuf[_iMaxMsgLenth];
    string _sUnprocessedMsg;

    int _iClientIndex;

    bool _bInactiveConnection;
};

typedef boost::shared_ptr<ClientTradeEngine> ClientTradeEnginePtr;

};

#endif /* ClientTradeEngine_H */
