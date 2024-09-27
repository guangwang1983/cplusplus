#include <iostream>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include "PositionServer.h"

using namespace std;
using namespace KO;

void ServerThreadFunc(boost::asio::io_service& io_service)
{
    io_service.run();
}

int main(int argc, char *argv[])
{
    try
    {
        long iPortNumber = atoi(argv[1]);
        string sShutDownTime = argv[2];

        string sHour = sShutDownTime.substr(0,2);
        string sMinute = sShutDownTime.substr(2,2);
        string sSecond = sShutDownTime.substr(4,2);

        boost::posix_time::time_duration cShutDownTime (atoi(sHour.c_str()), atoi(sMinute.c_str()), atoi(sSecond.c_str()));

        boost::asio::io_service io_service;
        PositionServer cPositionServer(io_service, iPortNumber);

        boost::thread server_thread(ServerThreadFunc, boost::ref(io_service));

        while(cShutDownTime > boost::posix_time::microsec_clock::local_time().time_of_day())
        {
            boost::this_thread::sleep(boost::posix_time::seconds(10));
        }

        io_service.stop();

        server_thread.join();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
