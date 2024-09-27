#ifndef ServerLogger_H
#define ServerLogger_H

#include <string>
#include <fstream>
#include <boost/shared_ptr.hpp>

using namespace std;

namespace KO
{

class ServerLogger
{
public:
    ~ServerLogger();
    static boost::shared_ptr<ServerLogger> GetInstance();

    void newErrorMsg(const string& sMessage);
    void newWarningMsg(const string& sMessage);
    void newDebugMsg(const string& sMessage);
    void newInfoMsg(const string& sMessage);

private:
    ServerLogger();
    long igetCurrentEpochTime();

    static boost::shared_ptr<ServerLogger> _pInstance;

    fstream ofsErrorWarningFile;
    long _iErrorMsgSeqNumber;
};

}

#endif /* ServerLogger_H */
