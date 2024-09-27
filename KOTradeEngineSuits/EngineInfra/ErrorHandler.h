#ifndef ErrorHandler_H
#define ErrorHandler_H

#include <string>
#include <fstream>
#include <boost/shared_ptr.hpp>

using std::string;
using std::fstream;

namespace KO
{

class ErrorHandler
{
public:

    enum LogLevel
    {
        ERROR,
        WARNING,
        DEBUG,
        INFO
    };

    ~ErrorHandler();
    static boost::shared_ptr<ErrorHandler> GetInstance();

    void init(bool bIsAppend, string sLogLevel, string sLogPath);
    void newErrorMsg(const string& sEid, const string& sEngineName, const string& sProduct, const string& sMessage); 
    void newWarningMsg(const string& sEid, const string& sEngineName, const string& sProduct, const string& sMessage); 
    void newDebugMsg(const string& sEid, const string& sEngineName, const string& sProduct, const string& sMessage); 
    void newInfoMsg(const string& sEid, const string& sEngineName, const string& sProduct, const string& sMessage); 
    void flush();

private:
    ErrorHandler();
    static boost::shared_ptr<ErrorHandler> _pInstance;

    fstream ofsErrorWarningFile;
    long _iErrorMsgSeqNumber;

    LogLevel _eLogLevel;
};

}

#endif /* ErrorHandler_H */
