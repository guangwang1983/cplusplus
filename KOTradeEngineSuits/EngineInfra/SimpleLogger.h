#ifndef SimpleLogger_H
#define SimpleLogger_H

#include <iostream>
#include <fstream>

using std::string;
using std::fstream;

namespace KO
{

class SimpleLogger
{

public:
	SimpleLogger();
	~SimpleLogger();

	void openFile(string sFullLogName, bool bIsLogging, bool bIsAppend);
    void flush();
	void closeFile();

	SimpleLogger& operator << (const unsigned int iContent)
	{
		if(_bIsLogging)
		{
		    _fLogFile << iContent;
		}

		return (*this);
	}

	SimpleLogger& operator << (const int iContent)
	{
		if(_bIsLogging)
		{
		    _fLogFile << iContent;
		}

		return (*this);
	}

	SimpleLogger& operator << (const unsigned long iContent)
	{
		if(_bIsLogging)
		{
		    _fLogFile << iContent;
		}

		return (*this);
	}

	SimpleLogger& operator << (const long iContent)
	{
		if(_bIsLogging)
		{
		    _fLogFile << iContent;
		}

		return (*this);
	}

	SimpleLogger& operator << (const double dContent)
	{
	    if(_bIsLogging)
		{
		    _fLogFile << dContent;
		}

		return (*this);
	}

	SimpleLogger& operator << (const bool bContent)
	{
	    if(_bIsLogging)
		{
		    _fLogFile << bContent;
		}

		return (*this);
	}

	SimpleLogger& operator << (const string& sContent)
	{
	    if(_bIsLogging)
		{
		    _fLogFile << sContent;
		}

		return (*this);
	}

	SimpleLogger& operator << (const char* sContent)
	{
		if(_bIsLogging)
		{
			_fLogFile << sContent;
		}

		return (*this);
	}

	typedef std::basic_ostream<char, std::char_traits<char> > CoutType;
	typedef CoutType& (*StandardEndLine)(CoutType&);

	SimpleLogger& operator << (StandardEndLine cContent)
	{
		if(_bIsLogging)
		{
			_fLogFile << cContent;
		}

		return (*this);
	}

private:
    char _buffer[1000000];

	fstream _fLogFile;
	bool _bIsLogging;

	bool _bAddTimeStamp;
};





}

#endif /* SimpleLogger_H */
