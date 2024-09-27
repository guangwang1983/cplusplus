#include "ServerLogger.h"
#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace KO
{

boost::shared_ptr<ServerLogger> ServerLogger::_pInstance = boost::shared_ptr<ServerLogger>();

ServerLogger::ServerLogger()
{
    _iErrorMsgSeqNumber = 0;

    if(!ofsErrorWarningFile.is_open())
    {
        ofsErrorWarningFile.open("PositionServerLog.out", fstream::out | fstream::app);
        if(ofsErrorWarningFile.is_open())
        {
            ofsErrorWarningFile << igetCurrentEpochTime() << ";Server Start" << std::endl;
        }
    }
}

ServerLogger::~ServerLogger()
{
    if(ofsErrorWarningFile.is_open())
    {
        ofsErrorWarningFile.close();
    }
}

boost::shared_ptr<ServerLogger> ServerLogger::GetInstance()
{
    if(!_pInstance.get())
    {
        _pInstance.reset(new ServerLogger);
    }

    return _pInstance;
}

long ServerLogger::igetCurrentEpochTime()
{
    boost::posix_time::ptime cCurrentTime = boost::posix_time::microsec_clock::local_time();
    boost::posix_time::ptime cEpoch(boost::gregorian::date(1970,1,1));
    return (cCurrentTime - cEpoch).total_microseconds();
}

void ServerLogger::newErrorMsg(const string& sMessage)
{
    if(ofsErrorWarningFile.is_open())
    {
        ofsErrorWarningFile << igetCurrentEpochTime() << ";"
                            << _iErrorMsgSeqNumber << ";"
                            << "ERROR;"
                            << sMessage << std::endl;
        _iErrorMsgSeqNumber = _iErrorMsgSeqNumber + 1;
    }
}

void ServerLogger::newWarningMsg(const string& sMessage)
{
    if(ofsErrorWarningFile.is_open())
    {
        ofsErrorWarningFile << igetCurrentEpochTime() << ";"
                            << _iErrorMsgSeqNumber << ";"
                            << "WARNING;"
                            << sMessage << std::endl;
        _iErrorMsgSeqNumber = _iErrorMsgSeqNumber + 1;
    }
}

void ServerLogger::newDebugMsg(const string& sMessage)
{
    if(ofsErrorWarningFile.is_open())
    {
        ofsErrorWarningFile << igetCurrentEpochTime() << ";"
                            << _iErrorMsgSeqNumber << ";"
                            << "DEBUG;"
                            << sMessage << std::endl;
        _iErrorMsgSeqNumber = _iErrorMsgSeqNumber + 1;
    }
}

void ServerLogger::newInfoMsg(const string& sMessage)
{
    if(ofsErrorWarningFile.is_open())
    {
        ofsErrorWarningFile << igetCurrentEpochTime() << ";"
                            << _iErrorMsgSeqNumber << ";"
                            << "INFO;"
                            << sMessage << std::endl;
        _iErrorMsgSeqNumber = _iErrorMsgSeqNumber + 1;
    }
}

}

