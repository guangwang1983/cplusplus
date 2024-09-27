#include "ErrorHandler.h"
#include "SystemClock.h"

namespace KO
{

boost::shared_ptr<ErrorHandler> ErrorHandler::_pInstance = boost::shared_ptr<ErrorHandler>();

ErrorHandler::ErrorHandler()
{
    _eLogLevel = WARNING;
}

ErrorHandler::~ErrorHandler()
{
    if(ofsErrorWarningFile.is_open())
    {
        ofsErrorWarningFile.close();
    }
}

boost::shared_ptr<ErrorHandler> ErrorHandler::GetInstance()
{
    if(!_pInstance.get())
    {
        _pInstance.reset(new ErrorHandler);
    }

    return _pInstance;
}

void ErrorHandler::init(bool bIsAppend, string sLogLevel, string sLogPath)
{
    _iErrorMsgSeqNumber = 0;

    if(!ofsErrorWarningFile.is_open())
    {
        if(bIsAppend)
        {
            ofsErrorWarningFile.open(sLogPath + "/errorwarning.out", fstream::out | fstream::app);
        }
        else
        {
            ofsErrorWarningFile.open(sLogPath + "/errorwarning.out", fstream::out);
        }

        if(ofsErrorWarningFile.is_open())
        {
            ofsErrorWarningFile << "\nEngine Start" << std::endl;
        }
    }

    if(sLogLevel.compare("INFO") == 0)
    {
        _eLogLevel = INFO;
    }
    else if(sLogLevel.compare("DEBUG") == 0)
    {
        _eLogLevel = DEBUG;
    }
    else if(sLogLevel.compare("WARNING") == 0)
    {
        _eLogLevel = WARNING;
    }
    else if(sLogLevel.compare("ERROR") == 0)
    {
        _eLogLevel = ERROR;
    }
}

void ErrorHandler::newErrorMsg(const string& sEid, const string& sEngineName, const string& sProduct, const string& sMessage)
{
    if(ofsErrorWarningFile.is_open())
    {
        if(_eLogLevel >= ERROR)
        {
            ofsErrorWarningFile << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << ";"
                                << _iErrorMsgSeqNumber << ";"
                                << "ERROR;"
                                << sEid << ";"
                                << sEngineName << ";"
                                << sProduct << ";"
                                << sMessage << "\n";
            _iErrorMsgSeqNumber = _iErrorMsgSeqNumber + 1;
        }
    }
}

void ErrorHandler::newWarningMsg(const string& sEid, const string& sEngineName, const string& sProduct, const string& sMessage)
{
    if(ofsErrorWarningFile.is_open())
    {
        if(_eLogLevel >= WARNING)
        {
            ofsErrorWarningFile << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << ";"
                                << _iErrorMsgSeqNumber << ";"
                                << "WARNING;"
                                << sEid << ";"
                                << sEngineName << ";"
                                << sProduct << ";"
                                << sMessage << "\n";
            _iErrorMsgSeqNumber = _iErrorMsgSeqNumber + 1;
        }
    }
}

void ErrorHandler::newDebugMsg(const string& sEid, const string& sEngineName, const string& sProduct, const string& sMessage)
{
    if(ofsErrorWarningFile.is_open())
    {
        if(_eLogLevel >= DEBUG)
        {
            ofsErrorWarningFile << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << ";"
                                << _iErrorMsgSeqNumber << ";"
                                << "DEBUG;"
                                << sEid << ";"
                                << sEngineName << ";"
                                << sProduct << ";"
                                << sMessage << "\n";
            _iErrorMsgSeqNumber = _iErrorMsgSeqNumber + 1;
        }
    }
}

void ErrorHandler::newInfoMsg(const string& sEid, const string& sEngineName, const string& sProduct, const string& sMessage)
{
    if(ofsErrorWarningFile.is_open())
    {
        if(_eLogLevel >= INFO)
        {
            ofsErrorWarningFile << SystemClock::GetInstance()->cgetCurrentKOEpochTime().igetPrintable() << ";"
                                << _iErrorMsgSeqNumber << ";"
                                << "INFO;"
                                << sEid << ";"
                                << sEngineName << ";"
                                << sProduct << ";"
                                << sMessage << "\n";
            _iErrorMsgSeqNumber = _iErrorMsgSeqNumber + 1;
        }
    }
}

void ErrorHandler::flush()
{
    if(ofsErrorWarningFile.is_open())
    {
        ofsErrorWarningFile.flush();
    }
}

}
