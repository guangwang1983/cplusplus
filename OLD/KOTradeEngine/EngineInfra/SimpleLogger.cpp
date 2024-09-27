#include "SimpleLogger.h"

namespace KO
{

SimpleLogger::SimpleLogger()
:_bIsLogging(false),
 _bAddTimeStamp(false)
{
}

void SimpleLogger::openFile(string sFullLogName, bool bIsLogging, bool bIsAppend)
{
	_bIsLogging = bIsLogging;

	if(_bIsLogging)
	{
        _fLogFile.rdbuf()->pubsetbuf(_buffer, 2000000);

        if(bIsAppend)
        {
    		_fLogFile.open(sFullLogName.c_str(), fstream::out | fstream::app);
        }
        else
        {
            _fLogFile.open(sFullLogName.c_str(), fstream::out);
        }
		_fLogFile.precision(15);

		_bIsLogging = bIsLogging;
	}
}

void SimpleLogger::flush()
{
    if(_bIsLogging)
    {
        _fLogFile.flush();
    }
}

void SimpleLogger::closeFile()
{
    if(_bIsLogging)
    {
    	_fLogFile.close();
    }
}

SimpleLogger::~SimpleLogger()
{
    if(_bIsLogging)
    {
        _fLogFile.close();
    }
}

}
