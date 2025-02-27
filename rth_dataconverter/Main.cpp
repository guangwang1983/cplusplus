#include <stdlib.h>
#include <string>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <exception>
#include <system_error>
#include <boost/math/special_functions/round.hpp>
#include <boost/program_options.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time_adjustor.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/local_timezone_defs.hpp>

#include <time.h>
#include <H5Cpp.h>


using namespace std;
using namespace H5;

enum FileFormat
{
	HC,
    HC_RAW,
    KO_RAW,
    RTH_DATA,
	TICKHISTORY,
	TRTHv2,
    ALT_TRTHv2,
	UNKNOWN
};

struct GridData
{
	long iEpochTimeStamp;
	double dBid;
    long iBidInTicks;
	long iBidSize;
	double dAsk;
    long iAskInTicks;
	long iAskSize;
	double dLast;
    long iLastInTicks;
    long iTradeSize;
	long iAccumuTradeSize;
    double dWeightedMid;
    double dWeightedMidInTicks;
};

string convertTickHistoryDate(const string& input)
{
    string sDay = input.substr(0,2);
    string sMonth = input.substr(3,3);
    string sYear = input.substr(7,4);
    
    if(sMonth == "JAN")
    {
        return sYear + "-01-" + sDay;
    }
    else if(sMonth == "FEB")
    {
        return sYear + "-02-" + sDay;
    }
    else if(sMonth == "MAR")
    {
        return sYear + "-03-" + sDay;
    }
    else if(sMonth == "APR")
    {
        return sYear + "-04-" + sDay;
    }
    else if(sMonth == "MAY")
    {
        return sYear + "-05-" + sDay;
    }
    else if(sMonth == "JUN")
    {
        return sYear + "-06-" + sDay;
    }
    else if(sMonth == "JUL")
    {
        return sYear + "-07-" + sDay;
    }
    else if(sMonth == "AUG")
    {
        return sYear + "-08-" + sDay;
    }
    else if(sMonth == "SEP")
    {
        return sYear + "-09-" + sDay;
    }
    else if(sMonth == "OCT")
    {
        return sYear + "-10-" + sDay;
    }
    else if(sMonth == "NOV")
    {
        return sYear + "-11-" + sDay;
    }
    else if(sMonth == "DEC")
    {
        return sYear + "-12-" + sDay;
    }
}

void convert_iso8601(const char *time_string, int ts_len, struct tm *tm_data)
{
	tzset();

	char temp[64];
	memset(temp, 0, sizeof(temp));
	strncpy(temp, time_string, ts_len);

	struct tm ctime;
	memset(&ctime, 0, sizeof(struct tm));
	strptime(temp, "%FT%T%z", &ctime);

	long ts = mktime(&ctime) - timezone;
	localtime_r(&ts, tm_data);
}

long iFromStringToEpoch(const string& sStringTime)
{
//	char date[] = "2019-01-28T08:01:06Z";
	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));
	convert_iso8601(sStringTime.c_str(), sStringTime.length(), &tm);

    long iUTC = (long) mktime(&tm);
	return iUTC;
}

boost::posix_time::ptime cCETToUTC(boost::posix_time::ptime cCETTime)
{
    typedef boost::date_time::eu_dst_trait<boost::gregorian::date> eu_dst_traits;
    typedef boost::date_time::dst_calc_engine<boost::gregorian::date, boost::posix_time::time_duration, eu_dst_traits> eu_dst_calc;
    typedef boost::date_time::local_adjustor<boost::posix_time::ptime, 1, eu_dst_calc> CET;

    boost::posix_time::ptime cUTCTime = CET::local_to_utc(cCETTime);

    return cUTCTime;
}

boost::posix_time::ptime cETToUTC(boost::posix_time::ptime cETTime)
{
    typedef boost::date_time::us_dst_trait<boost::gregorian::date> us_dst_traits;
    typedef boost::date_time::dst_calc_engine<boost::gregorian::date, boost::posix_time::time_duration, us_dst_traits> us_dst_calc;
    typedef boost::date_time::local_adjustor<boost::posix_time::ptime, -5, us_dst_calc> ET;

    boost::posix_time::ptime cUTCTime = ET::local_to_utc(cETTime);

    return cUTCTime;
}

long igetCETDiffUTC(const string& sDate)
{
    boost::gregorian::date cDate = boost::gregorian::from_undelimited_string(sDate);
    boost::posix_time::ptime cCETTime (cDate, boost::posix_time::time_duration(12,0,0,0));
    boost::posix_time::ptime cUTCTime = cCETToUTC(cCETTime);
    long iCETDiffUTC = (cCETTime - cUTCTime).total_seconds();

    return iCETDiffUTC;
}

long igetETDiffUTC(const string& sDate)
{
    boost::gregorian::date cDate = boost::gregorian::from_undelimited_string(sDate);
    boost::posix_time::ptime cETTime (cDate, boost::posix_time::time_duration(12,0,0,0));
    boost::posix_time::ptime cUTCTime = cETToUTC(cETTime);
    long iETDiffUTC = (cETTime - cUTCTime).total_seconds();

    return iETDiffUTC;
}

int main(int argc, char *argv[])
{
    string sDate;
 	string sInputFileName = "DEFAULT";
    double dReuterDataLatency = 0.0;
    double dTickSize = 0.0;
    long iTickLatency = 0;
    string sOutputTickFileName;
    vector<string> vOutputSecFileNames;
    vector<string> vOutputMinFileNames;
    vector<string> vOutputMinCETFileNames;
    vector<string> vOutput5MinFileNames;
    vector<long> vSecLatencies;
    vector<long> vMinLatencies;
    vector<long> v5MinLatencies;

    long iArtificialSpread = 0;
    bool bCheckData;

    bool bTickSizeAdjusted = false;
    double dPriceAdjustment = 1;

    boost::program_options::options_description _cAllOptions;
    _cAllOptions.add_options()("Date", boost::program_options::value<string>(&sDate), "Data Date");
    _cAllOptions.add_options()("InputFileName", boost::program_options::value<string>(&sInputFileName), "Input File Name");
    _cAllOptions.add_options()("ReuterDataLatency", boost::program_options::value<double>(&dReuterDataLatency), "ReuterDataLatency");
    _cAllOptions.add_options()("TickSize", boost::program_options::value<double>(&dTickSize), "Product Tick Size");
    _cAllOptions.add_options()("TickLatency", boost::program_options::value<long>(&iTickLatency), "Tick File Latency");
    _cAllOptions.add_options()("TickOutputFileName", boost::program_options::value<string>(&sOutputTickFileName), "Tick Output File Name");
    _cAllOptions.add_options()("SecOutputFileName", boost::program_options::value<vector<string>>(&vOutputSecFileNames), "Second Grid Output File Name");
    _cAllOptions.add_options()("MinCETOutputFileName", boost::program_options::value<vector<string>>(&vOutputMinCETFileNames), "Minute CET Grid Output File Name");
    _cAllOptions.add_options()("MinOutputFileName", boost::program_options::value<vector<string>>(&vOutputMinFileNames), "Minute Grid Output File Name");
    _cAllOptions.add_options()("5MinOutputFileName", boost::program_options::value<vector<string>>(&vOutput5MinFileNames), "5Minute Grid Output File Name");
    _cAllOptions.add_options()("SecLatency", boost::program_options::value<vector<long>>(&vSecLatencies), "Second Grid Latency");
    _cAllOptions.add_options()("MinLatency", boost::program_options::value<vector<long>>(&vMinLatencies), "Minute Grid Latency");
    _cAllOptions.add_options()("5MinLatency", boost::program_options::value<vector<long>>(&v5MinLatencies), "5Minute Grid Latency");

    _cAllOptions.add_options()("ArtificialSpread", boost::program_options::value<long>(&iArtificialSpread), "Artificial Spread");
    _cAllOptions.add_options()("CheckData", boost::program_options::value<bool>(&bCheckData), "Check Data");

    if(argc == 1)
    {
        cout << _cAllOptions;
        return 0;
    }

    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, _cAllOptions), vm);
    boost::program_options::notify(vm);

    bool bIsSXF = false;
    if(sInputFileName.find("SXF") != std::string::npos)
    {
        bIsSXF = true;
    }

    long iCETDiffUTC = igetCETDiffUTC(sDate);
    long iETDiffUTC = igetETDiffUTC(sDate);

	vector<GridData> vGridData;

    if(sInputFileName.find("_JY") == std::string::npos)
    {
        bTickSizeAdjusted = true;
    }

	const int constHCRawTimeStampClmn = 0;
	const int constHCRawTypeClmn = 1;
	const int constHCRawTradePriceClmn = 6;
	const int constHCRawTradeVolumeClmn = 7;
	const int constHCRawBidClmn = 2;
	const int constHCRawBidSizeClmn = 3;
	const int constHCRawAskClmn = 4;
	const int constHCRawAskSizeClmn = 5;
	const int constHCRawTradeQualifierClmn = 8;

	const int constKORawTimeStampClmn = 0;
	const int constKORawTypeClmn = 1;
	const int constKORawTradePriceClmn = 6;
	const int constKORawTradeVolumeClmn = 7;
	const int constKORawBidClmn = 3;
	const int constKORawBidSizeClmn = 2;
	const int constKORawAskClmn = 4;
	const int constKORawAskSizeClmn = 5;
	const int constKORawTradeQualifierClmn = 8;

	const int constHCTimeStampClmn = 2;
	const int constHCTypeClmn = 4;
	const int constHCTradePriceClmn = 5;
	const int constHCTradeVolumeClmn = 6;
	const int constHCBidClmn = 7;
	const int constHCBidSizeClmn = 8;
	const int constHCAskClmn = 9;
	const int constHCAskSizeClmn = 10;
	const int constHCTradeQualifierClmn = 11;

    const int constRTHDataTimeStampClmn = 3;
    const int constRTHDataTypeClmn = 5;
    const int constRTHDataTradePriceClmn = 6;
    const int constRTHDataTradeVolumeClmn = 7;
    const int constRTHDataBidClmn = 8;
    const int constRTHDataBidSizeClmn = 9;
    const int constRTHDataAskClmn = 10;
    const int constRTHDataAskSizeClmn = 11;
    const int constRTHDataTradeQualifierClmn = 12;

	const int constTRTHv2TimeStampClmn = 2;
	const int constTRTHv2TypeClmn = 4;
	const int constTRTHv2TradePriceClmn = 7;
	const int constTRTHv2TradeVolumeClmn = 8;
	const int constTRTHv2BidClmn = 11;
	const int constTRTHv2BidSizeClmn = 12;
	const int constTRTHv2AskClmn = 15;
	const int constTRTHv2AskSizeClmn = 16;
	const int constTRTHv2ExchangeTimeClmn = 20;
	const int constTRTHv2TradeQualifierClmn = 18;

	const int constAltTRTHv2TimeStampClmn = 2;
	const int constAltTRTHv2TypeClmn = 3;
	const int constAltTRTHv2TradePriceClmn = 6;
	const int constAltTRTHv2TradeVolumeClmn = 7;
	const int constAltTRTHv2BidClmn = 10;
	const int constAltTRTHv2BidSizeClmn = 11;
	const int constAltTRTHv2AskClmn = 14;
	const int constAltTRTHv2AskSizeClmn = 15;
	const int constAltTRTHv2ExchangeTimeClmn = 19;
	const int constAltTRTHv2TradeQualifierClmn = 17;

	const int constTickHistoryDateStampClmn = 1;
	const int constTickHistoryTimeStampClmn = 2;
	const int constTickHistoryTypeClmn = 4;
	const int constTickHistoryTradePriceClmn = 7;
	const int constTickHistoryTradeVolumeClmn = 8;
	const int constTickHistoryBidClmn = 11;
	const int constTickHistoryBidSizeClmn = 12;
	const int constTickHistoryAskClmn = 15;
	const int constTickHistoryAskSizeClmn = 16;
	const int constTickHistoryExchangeTimeClmn = 20;
	const int constTickHistoryTradeQualifierClmn = 18;

	long iCurrentTime = 0;
	long iExchangeTime = 0;
	double dBid = 0;
    long iBidInTicks = 0;
	long iBidSize = 0;
	double dAsk = 0;
    long iAskInTicks = 0;
	long iAskSize = 0;
	double dLast = 0;
    double iLastInTicks = 0;
    long iTradeSize = 0;
	long iAccumuTradeSize = 0;
    double dWeightedMid = 0;
    double dWeightedMidInTicks = 0;

	long iLatencyTotal = 0;
	long iNumLatencySamples = 0;

	long iCurrentLineNumber = 0;

	bool bWholeFileParsed = false;

    double dSpreadSum = 0;
    double dSqrdSpreadSum = 0;
    long iNumSpreadSample = 0;
 
    string sStartOfDayTime = sDate.substr(0,4) + "-" + sDate.substr(4,2) + "-" + sDate.substr(6,2) + "T00:00:01Z000000";
    long iStartOfDayTimeUTC = (iFromStringToEpoch(sStartOfDayTime.c_str()) - 10800) * 1000000;
	
	FileFormat eFileFormat = UNKNOWN;
	fstream ifsInputFile;
	ifsInputFile.exceptions(std::ifstream::failbit | std::ifstream::eofbit);

	ifsInputFile.open(sInputFileName.c_str(), fstream::in);
	if(ifsInputFile.is_open())
	{
        GridData cPrevTickDataPoint;
        cPrevTickDataPoint.iEpochTimeStamp = -1;
        long iFirstCrossTime = -1;
        bool bPriceCrossingTriggered = false;
        long iLastUpdateTime = -1;
        bool bFirstQuoteSeen = false;

		while(bWholeFileParsed == false)
		{
			char sNewLine [2048];
			
			try
			{
				ifsInputFile.getline(sNewLine, sizeof(sNewLine));
			}
			catch(std::ios_base::failure& e)
			{
				if(ifsInputFile.eof())
				{
					bWholeFileParsed = true;
					break;
				}
				if(ifsInputFile.fail())
				{
					cerr << "ERROR: failed to read line number: " << iCurrentLineNumber << ". Conversion stopped\n";
					break;
				}
			}
 
			if(strlen(sNewLine) != 0 && sNewLine[0] != '#')
			{
				if(iCurrentLineNumber == 0)
				{
					if(string(sNewLine).find("TickHistory Format") != std::string::npos)
					{
						eFileFormat = TICKHISTORY;	
					}
					else if(string(sNewLine).find("TRTHv2 Format") != std::string::npos)
					{
						eFileFormat = TRTHv2;
					}
					else if(string(sNewLine).find("HC Format") != std::string::npos)
					{
						eFileFormat = HC;
					}
					else if(string(sNewLine).find("Timestamp,Type") != std::string::npos)
					{
						eFileFormat = HC_RAW;
					}
                    else if(string(sNewLine).find("RTH Data") != std::string::npos)
					{
						eFileFormat = RTH_DATA;
					}   
                    else
                    {
                        eFileFormat = KO_RAW;
                    }
				}
				else if(eFileFormat != UNKNOWN)
				{
                    if(iCurrentLineNumber == 1)
                    {
                        if(eFileFormat == TRTHv2)
                        {
                            if(string(sNewLine).find(",+") == std::string::npos &&
                               string(sNewLine).find(",-") == std::string::npos)
                            {
                                eFileFormat = ALT_TRTHv2;
                            }
                        }
                    }

                    istringstream cDataLineStream(sNewLine);
                    string sElement;
                    int iColumnNumber = 0;
                    int iNumElement = 0;
		
                    string sTimeStamp = "";	
                    string sLineAction = "";
                    string sLineTradePrice = "";
                    string sLineTradeVolume = "";
                    string sLineBid = "";
                    string sLineBidSize = "";
                    string sLineAsk = "";
                    string sLineAskSize = "";
                    string sExchangeTimeStamp = "";
                    string sTradeQualifier = "";

                    while(!cDataLineStream.eof())
                    {
                        getline(cDataLineStream, sElement, ',');

                        iNumElement = iNumElement + 1;

                        if(eFileFormat == TRTHv2)
                        {
                            if(iColumnNumber == constTRTHv2TimeStampClmn)
                            {
                                sTimeStamp = sElement;
                            }
                            else if(iColumnNumber == constTRTHv2TypeClmn)
                            {
                                sLineAction = sElement;
                            }
                            else if (iColumnNumber == constTRTHv2TradePriceClmn)
                            {
                                if(sLineAction == "Trade")
                                {
                                    sLineTradePrice = sElement;
                                }
                            }
                            else if(iColumnNumber == constTRTHv2TradeVolumeClmn)
                            {
                                if(sLineAction == "Trade")
                                {
                                    sLineTradeVolume = sElement;
                                }	
                            }
                            else if(iColumnNumber == constTRTHv2BidClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineBid = sElement;	
                                }
                            }
                            else if(iColumnNumber == constTRTHv2BidSizeClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineBidSize = sElement;	
                                }
                            }
                            else if(iColumnNumber == constTRTHv2AskClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineAsk = sElement;	
                                }
                            }
                            else if(iColumnNumber == constTRTHv2AskSizeClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineAskSize = sElement;	
                                }
                            }
                            else if(iColumnNumber == constTRTHv2ExchangeTimeClmn)
                            {
                                sExchangeTimeStamp = sElement;
                            }
                            else if(iColumnNumber == constTRTHv2TradeQualifierClmn)
                            {
                                sTradeQualifier = sElement;
                            }
                        }
                        else if(eFileFormat == HC)
                        {
                            if(iColumnNumber == constHCTimeStampClmn)
                            {
                                sTimeStamp = sElement;
                            }
                            else if(iColumnNumber == constHCTypeClmn)
                            {
                                sLineAction = sElement;
                            }
                            else if (iColumnNumber == constHCTradePriceClmn)
                            {
                                if(sLineAction == "Trade")
                                {
                                    sLineTradePrice = sElement;
                                }
                            }
                            else if(iColumnNumber == constHCTradeVolumeClmn)
                            {
                                if(sLineAction == "Trade")
                                {
                                    sLineTradeVolume = sElement;
                                }	
                            }
                            else if(iColumnNumber == constHCBidClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineBid = sElement;	
                                }
                            }
                            else if(iColumnNumber == constHCBidSizeClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineBidSize = sElement;	
                                }
                            }
                            else if(iColumnNumber == constHCAskClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineAsk = sElement;	
                                }
                            }
                            else if(iColumnNumber == constHCAskSizeClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineAskSize = sElement;	
                                }
                            }
                            else if(iColumnNumber == constHCTradeQualifierClmn)
                            {
                                sTradeQualifier = sElement;
                            }
                        }
                        else if(eFileFormat == HC_RAW)
                        {
                            if(iColumnNumber == constHCRawTimeStampClmn)
                            {
                                sTimeStamp = sElement;
                            }
                            else if(iColumnNumber == constHCRawTypeClmn)
                            {
                                sLineAction = sElement;
                            }
                            else if (iColumnNumber == constHCRawTradePriceClmn)
                            {
                                if(sLineAction == "Trade")
                                {
                                    sLineTradePrice = sElement;
                                }
                            }
                            else if(iColumnNumber == constHCRawTradeVolumeClmn)
                            {
                                if(sLineAction == "Trade")
                                {
                                    sLineTradeVolume = sElement;
                                }	
                            }
                            else if(iColumnNumber == constHCRawBidClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineBid = sElement;	
                                }
                            }
                            else if(iColumnNumber == constHCRawBidSizeClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineBidSize = sElement;	
                                }
                            }
                            else if(iColumnNumber == constHCRawAskClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineAsk = sElement;	
                                }
                            }
                            else if(iColumnNumber == constHCRawAskSizeClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineAskSize = sElement;	
                                }
                            }
                            else if(iColumnNumber == constHCRawTradeQualifierClmn)
                            {
                                sTradeQualifier = sElement;
                            }
                        }
                        else if(eFileFormat == KO_RAW)
                        {
                            if(iColumnNumber == constKORawTimeStampClmn)
                            {
                                sTimeStamp = sElement;
                            }
                            else if(iColumnNumber == constKORawTypeClmn)
                            {
                                sLineAction = sElement;
                            }
                            else if (iColumnNumber == constKORawTradePriceClmn)
                            {
                                if(sLineAction == "Trade")
                                {
                                    sLineTradePrice = sElement;
                                }
                            }
                            else if(iColumnNumber == constKORawTradeVolumeClmn)
                            {
                                if(sLineAction == "Trade")
                                {
                                    sLineTradeVolume = sElement;
                                }	
                            }
                            else if(iColumnNumber == constKORawBidClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineBid = sElement;	
                                }
                            }
                            else if(iColumnNumber == constKORawBidSizeClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineBidSize = sElement;	
                                }
                            }
                            else if(iColumnNumber == constKORawAskClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineAsk = sElement;	
                                }
                            }
                            else if(iColumnNumber == constKORawAskSizeClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineAskSize = sElement;	
                                }
                            }
                            else if(iColumnNumber == constKORawTradeQualifierClmn)
                            {
                                sTradeQualifier = sElement;
                            }
                        }
                        else if(eFileFormat == RTH_DATA)
                        {
                            if(iColumnNumber == constRTHDataTimeStampClmn)
                            {
                                sTimeStamp = sElement;
                            }
                            else if(iColumnNumber == constRTHDataTypeClmn)
                            {
                                sLineAction = sElement;
                            }
                            else if (iColumnNumber == constRTHDataTradePriceClmn)
                            {
                                if(sLineAction == "Trade")
                                {
                                    sLineTradePrice = sElement;
                                }
                            }
                            else if(iColumnNumber == constRTHDataTradeVolumeClmn)
                            {
                                if(sLineAction == "Trade")
                                {
                                    sLineTradeVolume = sElement;
                                }	
                            }
                            else if(iColumnNumber == constRTHDataBidClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineBid = sElement;	
                                }
                            }
                            else if(iColumnNumber == constRTHDataBidSizeClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineBidSize = sElement;	
                                }
                            }
                            else if(iColumnNumber == constRTHDataAskClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineAsk = sElement;	
                                }
                            }
                            else if(iColumnNumber == constRTHDataAskSizeClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineAskSize = sElement;	
                                }
                            }
                            else if(iColumnNumber == constRTHDataTradeQualifierClmn)
                            {
                                sTradeQualifier = sElement;
                            }
                        }

                        else if(eFileFormat == ALT_TRTHv2)
                        {
                            if(iColumnNumber == constAltTRTHv2TimeStampClmn)
                            {
                                sTimeStamp = sElement;
                            }
                            else if(iColumnNumber == constAltTRTHv2TypeClmn)
                            {
                                sLineAction = sElement;
                            }
                            else if (iColumnNumber == constAltTRTHv2TradePriceClmn)
                            {
                                if(sLineAction == "Trade")
                                {
                                    sLineTradePrice = sElement;
                                }
                            }
                            else if(iColumnNumber == constAltTRTHv2TradeVolumeClmn)
                            {
                                if(sLineAction == "Trade")
                                {
                                    sLineTradeVolume = sElement;
                                }	
                            }
                            else if(iColumnNumber == constAltTRTHv2BidClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineBid = sElement;	
                                }
                            }
                            else if(iColumnNumber == constAltTRTHv2BidSizeClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineBidSize = sElement;	
                                }
                            }
                            else if(iColumnNumber == constAltTRTHv2AskClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineAsk = sElement;	
                                }
                            }
                            else if(iColumnNumber == constAltTRTHv2AskSizeClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineAskSize = sElement;	
                                }
                            }
                            else if(iColumnNumber == constAltTRTHv2ExchangeTimeClmn)
                            {
                                sExchangeTimeStamp = sElement;
                            }
                            else if(iColumnNumber == constAltTRTHv2TradeQualifierClmn)
                            {
                                sTradeQualifier = sElement;
                            }
                        }

                        else if(eFileFormat == TICKHISTORY)
                        {
                            if(iColumnNumber == constTickHistoryDateStampClmn)
                            {
                                sTimeStamp = convertTickHistoryDate(sElement);
                            }
                            else if(iColumnNumber == constTickHistoryTimeStampClmn)
                            {
                                sTimeStamp = sTimeStamp + "T" + sElement + "000Z";
                            }
                            else if(iColumnNumber == constTickHistoryTypeClmn)
                            {
                                sLineAction = sElement;
                            }
                            else if (iColumnNumber == constTickHistoryTradePriceClmn)
                            {
                                if(sLineAction == "Trade")
                                {
                                    sLineTradePrice = sElement;
                                }
                            }
                            else if(iColumnNumber == constTickHistoryTradeVolumeClmn)
                            {
                                if(sLineAction == "Trade")
                                {
                                    sLineTradeVolume = sElement;
                                }	
                            }
                            else if(iColumnNumber == constTickHistoryBidClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineBid = sElement;	
                                }
                            }
                            else if(iColumnNumber == constTickHistoryBidSizeClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineBidSize = sElement;	
                                }
                            }
                            else if(iColumnNumber == constTickHistoryAskClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineAsk = sElement;	
                                }
                            }
                            else if(iColumnNumber == constTickHistoryAskSizeClmn)
                            {
                                if(sLineAction == "Quote")
                                {
                                    sLineAskSize = sElement;	
                                }
                            }
                            else if(iColumnNumber == constTickHistoryExchangeTimeClmn)
                            {
                                sExchangeTimeStamp = sElement;
                            }
                            else if(iColumnNumber == constTickHistoryTradeQualifierClmn)
                            {
                                sTradeQualifier = sElement;
                            }
                        }

                        iColumnNumber = iColumnNumber + 1;
                    }

                    bool bLineValid = true;
                    if(eFileFormat == KO_RAW)
                    {
                        if(iNumElement > 9)
                        {
                            bLineValid = false;
                        }

                        if(sTimeStamp.size() > 16)
                        {
                            bLineValid = false;
                        }
                    }

                    if((sLineAction == "Trade" || sLineAction == "Quote") && sTradeQualifier.find("IND[MKT_ST_IND]") == std::string::npos && sTradeQualifier.find("Exhausted Bid and Ask") == std::string::npos && sTradeQualifier.find("   [MKT_ST_IND]") == std::string::npos && bLineValid)
                    {
                        if(sTimeStamp != "")
                        {
                            if(eFileFormat == KO_RAW)
                            {
                                iCurrentTime = stoll(sTimeStamp);
                            }
                            else if(eFileFormat == HC_RAW)
                            {
                                iCurrentTime = stoll(sTimeStamp) / (long long) 1000;
                            }
                            else
                            {
                                iCurrentTime = iFromStringToEpoch(sTimeStamp.substr(0,19) + "Z") * 1000000 + atoi(sTimeStamp.substr(20,6).c_str());
                            }

                            if(iCurrentTime < iStartOfDayTimeUTC)
                            {
                                continue;
                            }

                            bool bIgnoreUpdate = false;
                            if(bIsSXF == true)
                            {
                                long iETCurrentTimeT = iCurrentTime / 1000000 + iETDiffUTC;
                                boost::posix_time::ptime cETPTime = boost::posix_time::from_time_t(iETCurrentTimeT);
        
                                if(cETPTime.time_of_day().hours() == 9 &&
                                   cETPTime.time_of_day().minutes() >= 15 &&
                                   cETPTime.time_of_day().minutes() < 30)
                                {
                                    bIgnoreUpdate = true;
                                }
                            }

                            if(iCurrentTime < iLastUpdateTime)
                            {
                                bIgnoreUpdate = true;
                                cerr << "ERROR: Ignoring updates with backward time stamp " << iCurrentTime << "\n";
                            }

                            if(bIgnoreUpdate == false)
                            {
                                if(sLineAction == "Trade")
                                {
                                    if(sTradeQualifier.find("OFFBK") == std::string::npos &&
                                       sTradeQualifier.find("IRGCOND") == std::string::npos &&
                                       sTradeQualifier.find("ALIAS") == std::string::npos &&
                                       sTradeQualifier.find("S[ACT_FLAG1]") == std::string::npos &&
                                       sTradeQualifier.find("X[ACT_FLAG1]") == std::string::npos &&
                                       sTradeQualifier.find("I[ACT_FLAG1]") == std::string::npos &&
                                       sTradeQualifier.find("J[ACT_FLAG1]") == std::string::npos &&
                                       sTradeQualifier.find("K[ACT_FLAG1]") == std::string::npos &&
                                       sTradeQualifier.find("V[ACT_FLAG1]") == std::string::npos &&
                                       sTradeQualifier.find("G[ACT_FLAG1]") == std::string::npos &&
                                       sTradeQualifier.find("L[ACT_FLAG1]") == std::string::npos &&
                                       sTradeQualifier.find("SYS[GV3_TEXT]") == std::string::npos &&
                                       sTradeQualifier.find("CRA[GV3_TEXT]") == std::string::npos)
                                    {
                                        if(sLineTradePrice != "" && sLineTradeVolume != "")
                                        {
                                            dLast = atof(sLineTradePrice.c_str());

                                            if(bTickSizeAdjusted == false)
                                            {
                                                if(dLast > 500)
                                                {
                                                    // adjust tick size for crazy 6J reuter tick size error
                                                    dPriceAdjustment = 0.000001;
                                                }
                                                bTickSizeAdjusted = true;
                                            }
                                            dLast = dLast * dPriceAdjustment;
            
                                            iLastInTicks = boost::math::iround(dLast / dTickSize);
                                            iAccumuTradeSize = iAccumuTradeSize + atoi(sLineTradeVolume.c_str());

                                            if(iLastInTicks != 0)
                                            {
                                                //cerr << iCurrentTime << "|" << iExchangeTime << "|" << iBidSize << "|" << dBid << "|" << dAsk << "|" << iAskSize << "|" << dLast << "|" << iAccumuTradeSize << "\n";

                                                GridData cNewTickDataPoint;
                                                cNewTickDataPoint.iEpochTimeStamp = iCurrentTime;
                                                cNewTickDataPoint.dBid = dBid;
                                                cNewTickDataPoint.iBidInTicks = boost::math::iround(dBid / dTickSize);
                                                cNewTickDataPoint.iBidSize = iBidSize;
                                                cNewTickDataPoint.dAsk = dAsk;
                                                cNewTickDataPoint.iAskInTicks = boost::math::iround(dAsk / dTickSize);
                                                cNewTickDataPoint.iAskSize = iAskSize;
                                                cNewTickDataPoint.dLast = dLast;
                                                cNewTickDataPoint.iLastInTicks = iLastInTicks;
                                                cNewTickDataPoint.iTradeSize = atoi(sLineTradeVolume.c_str());
                                                cNewTickDataPoint.iAccumuTradeSize = iAccumuTradeSize;
                                                cNewTickDataPoint.dWeightedMid = dWeightedMid;    
                                                cNewTickDataPoint.dWeightedMidInTicks = dWeightedMidInTicks;    
     
                                                vGridData.push_back(cNewTickDataPoint);	

                                                if(cNewTickDataPoint.iAskInTicks < cNewTickDataPoint.iBidInTicks or cNewTickDataPoint.iAskInTicks == cNewTickDataPoint.iBidInTicks && bFirstQuoteSeen == true)
                                                {
                                                    if(iFirstCrossTime == -1)
                                                    {
                                                        iFirstCrossTime = iCurrentTime;
                                                    }
                                                    else
                                                    {
                                                        if(iCurrentTime - iFirstCrossTime > 10000000)
                                                        {
                                                            if(bPriceCrossingTriggered == false)
                                                            {
                                                                bPriceCrossingTriggered = true;
                                                                if(bCheckData)
                                                                {
                                                                    cerr << "Error: Price crossed in " << sInputFileName << " Time: " << iCurrentTime << "\n";
                                                                }
                                                            }
                                                        }
                                                    }

                                                }
                                                else
                                                {
                                                    iFirstCrossTime = -1;
                                                    bPriceCrossingTriggered = false;
                                                }

                                                if(bFirstQuoteSeen == true)
                                                {
                                                    if(abs(cNewTickDataPoint.dWeightedMidInTicks / cPrevTickDataPoint.dWeightedMidInTicks) > 1.10 or abs(cNewTickDataPoint.dWeightedMidInTicks / cPrevTickDataPoint.dWeightedMidInTicks) < 0.9)
                                                    {
    cerr << cNewTickDataPoint.dWeightedMidInTicks << "/" << cPrevTickDataPoint.dWeightedMidInTicks << "=" << (cNewTickDataPoint.dWeightedMidInTicks / cPrevTickDataPoint.dWeightedMidInTicks) << "\n";
                                                        if(bCheckData)
                                                        {
                                                            cerr << "Error: Mid price moved more than 10 percent in " << sInputFileName << " Time: " << iCurrentTime << "\n";
                                                        }
                                                    }
                                                }

                                                if(iLastUpdateTime != -1)
                                                {
                                                    if(cNewTickDataPoint.dWeightedMidInTicks != cPrevTickDataPoint.dWeightedMidInTicks)
                                                    {
                                                        long iStalenessThreshold = 7200000000;
                                                        if(iCurrentTime - iLastUpdateTime > iStalenessThreshold)
                                                        {
                                                            if(bCheckData)
                                                            {
                                                                cerr << "Error: Price staled for more than 2 hour in " << sInputFileName << " Time: " << iCurrentTime << "\n";
                                                            }
                                                        }
                                                    }
                                                }

                                                iLastUpdateTime = iCurrentTime;
                                                cPrevTickDataPoint = cNewTickDataPoint;
                                            }
                                        }
                                        else
                                        {
    //                                        cerr << "Warning: " << sInputFileName << " ignoring invalid trade line. Line number: " << iCurrentLineNumber << " sLineTradePrice: " << sLineTradePrice << " sLineTradeVolume: " << sLineTradeVolume << "\n";
    //                                        cerr << sNewLine << "\n";
                                        }
                                    }
                                }
                                else if(sLineAction == "Quote")
                                {
                                    if(iArtificialSpread != 0)
                                    {
                                        sLineBidSize = "1000000";
                                        sLineAskSize = "1000000";
                                    }

                                    if(sLineBid != "" || sLineBidSize != "" || sLineAsk != "" || sLineAskSize != "")
                                    {
                                        if(sLineBid != "")
                                        {
                                            dBid = atof(sLineBid.c_str());
                                            if(bTickSizeAdjusted == false)
                                            {
                                                if(dBid > 500)
                                                {
                                                    // adjust tick size for crazy 6J reuter tick size error
                                                    dPriceAdjustment = 0.000001;
                                                }
                                                bTickSizeAdjusted = true;
                                            }
                                            dBid = dBid * dPriceAdjustment;
                                            iBidInTicks = boost::math::iround(dBid / dTickSize);
                                        }

                                        if(sLineBidSize != "")
                                        {
                                            iBidSize = atoi(sLineBidSize.c_str());
                                        }
       
                                        if(sLineAsk != "")
                                        {
                                            dAsk = atof(sLineAsk.c_str());
                                            if(bTickSizeAdjusted == false)
                                            {
                                                if(dAsk > 500)
                                                {
                                                    // adjust tick size for crazy 6J reuter tick size error
                                                    dPriceAdjustment = 0.000001;
                                                }
                                                bTickSizeAdjusted = true;
                                            }
                                            dAsk = dAsk * dPriceAdjustment;
                                            iAskInTicks = boost::math::iround(dAsk / dTickSize);
                                        }

                                        if(sLineAskSize != "")
                                        {
                                            iAskSize = atoi(sLineAskSize.c_str());
                                        }

                                        //cerr << iCurrentTime << "|" << iExchangeTime << "|" << iBidSize << "|" << dBid << "|" << dAsk << "|" << iAskSize << "|" << dLast << "|" << iAccumuTradeSize << "\n";

                                        if(iBidInTicks != 0 && iAskInTicks != 0)
                                        {
                                            GridData cNewTickDataPoint;
                                            cNewTickDataPoint.iEpochTimeStamp = iCurrentTime;
                                            cNewTickDataPoint.dBid = dBid;
                                            cNewTickDataPoint.iBidInTicks = boost::math::iround(dBid / dTickSize);
                                            cNewTickDataPoint.iBidSize = iBidSize;
                                            cNewTickDataPoint.dAsk = dAsk;
                                            cNewTickDataPoint.iAskInTicks = boost::math::iround(dAsk / dTickSize);
                                            cNewTickDataPoint.iAskSize = iAskSize;
                                            cNewTickDataPoint.dLast = dLast;
                                            cNewTickDataPoint.iLastInTicks = iLastInTicks;
                                            cNewTickDataPoint.iTradeSize = 0;
                                            cNewTickDataPoint.iAccumuTradeSize = iAccumuTradeSize;

                                            if(iAskInTicks - iBidInTicks > 1)
                                            {
                                                dWeightedMidInTicks = (double)(iAskInTicks + iBidInTicks) / 2;
                                            }
                                            else
                                            {
                                                dWeightedMidInTicks = (double)iBidInTicks + (double)iBidSize / (double)(iBidSize + iAskSize);
                                            }

                                            dWeightedMid = dWeightedMidInTicks * dTickSize;
                                            cNewTickDataPoint.dWeightedMid = dWeightedMid;
                                            cNewTickDataPoint.dWeightedMidInTicks = dWeightedMidInTicks;

                                            vGridData.push_back(cNewTickDataPoint);

                                            if(cNewTickDataPoint.iAskInTicks < cNewTickDataPoint.iBidInTicks or cNewTickDataPoint.iAskInTicks == cNewTickDataPoint.iBidInTicks)
                                            {
                                                if(iFirstCrossTime == -1)
                                                {
                                                    iFirstCrossTime = iCurrentTime;
                                                }
                                                else
                                                {
                                                    if(iCurrentTime - iFirstCrossTime > 10000000)
                                                    {
                                                        if(bPriceCrossingTriggered == false)
                                                        {
                                                            bPriceCrossingTriggered = true;
                                                            if(bCheckData)
                                                            {
                                                                long iUtcDiff = igetCETDiffUTC(sDate);
                                                                boost::posix_time::ptime cCurrentime = boost::posix_time::from_time_t(iCurrentTime/1000000 + iUtcDiff);
                                                                if(cCurrentime.time_of_day().hours() < 21)
                                                                {
                                                                    cerr << "Error: Price crossed in " << sInputFileName << " Time: " << iCurrentTime << "\n";
                                                                }
                                                            }
                                                        }
                                                    }
                                                }

                                            }
                                            else
                                            {
                                                iFirstCrossTime = -1;
                                                bPriceCrossingTriggered = false;

                                                double dSpread = cNewTickDataPoint.iAskInTicks - cNewTickDataPoint.iBidInTicks;
                                                dSpreadSum = dSpreadSum + dSpread;
                                                dSqrdSpreadSum = dSqrdSpreadSum + dSpread * dSpread;
                                                iNumSpreadSample = iNumSpreadSample + 1;
                                            }

                                            if(bFirstQuoteSeen == true)
                                            {
                                                if(abs(cNewTickDataPoint.dWeightedMidInTicks / cPrevTickDataPoint.dWeightedMidInTicks) > 1.10 or abs(cNewTickDataPoint.dWeightedMidInTicks / cPrevTickDataPoint.dWeightedMidInTicks) < 0.9)
                                                {
    cerr << cNewTickDataPoint.dWeightedMidInTicks << "/" << cPrevTickDataPoint.dWeightedMidInTicks << "=" << (cNewTickDataPoint.dWeightedMidInTicks / cPrevTickDataPoint.dWeightedMidInTicks) << "\n";
                                                    if(bCheckData)
                                                    {
                                                        cerr << "Error: Mid price moved more than 10 percent in " << sInputFileName << " Time: " << iCurrentTime << "\n";
                                                    }
                                                }
                                            }

                                            if(iLastUpdateTime != -1)
                                            {
                                                if(cNewTickDataPoint.dWeightedMidInTicks != cPrevTickDataPoint.dWeightedMidInTicks)
                                                {
                                                    long iStalenessThreshold = 7200000000;
                                                    if(iCurrentTime - iLastUpdateTime > iStalenessThreshold)
                                                    {
                                                        if(bCheckData)
                                                        {
                                                            cerr << "Error: Price staled for more than 2 hour in " << sInputFileName << " Time: " << iCurrentTime << "\n";
                                                        }
                                                    }
                                                }
                                            }

                                            iLastUpdateTime = iCurrentTime;
                                            bFirstQuoteSeen = true;
                                            cPrevTickDataPoint = cNewTickDataPoint;
                                        }
                                    }
                                    else
                                    {
    //                                    cerr << "ERROR: " << sInputFileName << " Invalid quote line. Line number: " << iCurrentLineNumber << "\n";
                                    }
                                }
                            }
                        }
                        else
                        {
                            cerr << "ERROR: " << sInputFileName << " Invalid time stamp. Line number: " << iCurrentLineNumber << "\n";
                        }
                    }
//                    else
//                    {
//                        cerr << "ignoring line " + string(sNewLine) + "\n";
//                    }
				}
                else
                {
                    cerr << "ERROR: " << sInputFileName << " Unable to identify file format. No conversation \n";
                    break;
                }
			}

            iCurrentLineNumber = iCurrentLineNumber + 1;
		}

		if(bWholeFileParsed == true && vGridData.size() != 0)	
		{
            if(iArtificialSpread != 0)
            {
                double dAvgSpread = dSpreadSum / (double)iNumSpreadSample;
                long iStdSpread = sqrt(dSqrdSpreadSum / (double)(iNumSpreadSample)- (dAvgSpread * dAvgSpread));
                long iSquashThresh = dAvgSpread + iStdSpread * 3;

                cerr << "dAvgSpread " << dAvgSpread << "\n";
                cerr << "iStdSpread " << iStdSpread << "\n";
                cerr << "iSquashThresh " << iSquashThresh << "\n";

                bool bAdjustBidFirst = true;
                bool bAdjustBid = true;
                for(vector<GridData>::iterator itr = vGridData.begin(); itr != vGridData.end(); itr++)
                {
                    long iSpread = (*itr).iAskInTicks - (*itr).iBidInTicks;
                    //squash all ticks to target width for fx
                    if(true)
                    {
                        long iTicksToAllocate = iSpread - iArtificialSpread;
                        bAdjustBid = bAdjustBidFirst;
                        while(iTicksToAllocate > 0)
                        {
                            if(bAdjustBid)
                            {
                                (*itr).iBidInTicks = (*itr).iBidInTicks + 1;
                                iTicksToAllocate = iTicksToAllocate - 1;
                                bAdjustBid = false;
                            }
                            else
                            {
                                (*itr).iAskInTicks = (*itr).iAskInTicks - 1;
                                iTicksToAllocate = iTicksToAllocate - 1;
                                bAdjustBid = true;
                            }
                        }                                                        

                        if(bAdjustBidFirst == true)
                        {
                            bAdjustBidFirst = false;
                        }
                        else
                        {
                            bAdjustBidFirst = true;
                        }
                    
                        (*itr).dBid = (*itr).iBidInTicks * dTickSize;
                        (*itr).dAsk = (*itr).iAskInTicks * dTickSize;
                    }
                }
            }

			H5File* pH5TickFile = new H5File(sOutputTickFileName, H5F_ACC_TRUNC);

            CompType cGridDataType(sizeof(GridData));
            cGridDataType.insertMember("EpochTimeStamp", HOFFSET(GridData, iEpochTimeStamp), PredType::NATIVE_LONG);

            cGridDataType.insertMember("Bid", HOFFSET(GridData, dBid), PredType::NATIVE_DOUBLE);
            cGridDataType.insertMember("BidInTicks", HOFFSET(GridData, iBidInTicks), PredType::NATIVE_LONG);
            cGridDataType.insertMember("BidSize", HOFFSET(GridData, iBidSize), PredType::NATIVE_LONG);

            cGridDataType.insertMember("Ask", HOFFSET(GridData, dAsk), PredType::NATIVE_DOUBLE);
            cGridDataType.insertMember("AskInTicks", HOFFSET(GridData, iAskInTicks), PredType::NATIVE_LONG);
            cGridDataType.insertMember("AskSize", HOFFSET(GridData, iAskSize), PredType::NATIVE_LONG);

            cGridDataType.insertMember("Last", HOFFSET(GridData, dLast), PredType::NATIVE_DOUBLE);
            cGridDataType.insertMember("LastInTicks", HOFFSET(GridData, iLastInTicks), PredType::NATIVE_LONG);
            cGridDataType.insertMember("TradeSize", HOFFSET(GridData, iTradeSize), PredType::NATIVE_LONG);
            cGridDataType.insertMember("AccumumTradeSize", HOFFSET(GridData, iAccumuTradeSize), PredType::NATIVE_LONG);
            cGridDataType.insertMember("WeightedMid", HOFFSET(GridData, dWeightedMid), PredType::NATIVE_DOUBLE);
            cGridDataType.insertMember("WeightedMidInTicks", HOFFSET(GridData, dWeightedMidInTicks), PredType::NATIVE_DOUBLE);

            vector<vector<GridData>> vSecondGridDataSets;
            vector<vector<GridData>> vMinGridDataSets;
            vector<vector<GridData>> vMinCETGridDataSets;
            vector<vector<GridData>> v5MinGridDataSets;

            for(vector<string>::iterator itr = vOutputSecFileNames.begin();
                itr != vOutputSecFileNames.end();
                itr++)
            {
                vSecondGridDataSets.push_back(vector<GridData>());
                vMinGridDataSets.push_back(vector<GridData>());
                vMinCETGridDataSets.push_back(vector<GridData>());
                v5MinGridDataSets.push_back(vector<GridData>());
            }

            long currentTimeStampIndex = 0;    
            long iNumGridPointsGrouped = 0;
            vector<GridData>::iterator itrGroupStart;
            vector<GridData>::iterator itrGroupEnd;
            long iTotalGridPointsWritten = 0;
			for(vector<GridData>::iterator itr = vGridData.begin(); itr != vGridData.end(); itr++)
  			{
//				cerr << (*itr).iEpochTimeStamp << "|" << (*itr).iBidSize << "|" << (*itr).dBid << "|" << (*itr).dAsk << "|" << (*itr).iAskSize << "|" << (*itr).dLast << "|" << (*itr).iAccumuTradeSize << "\n";

				(*itr).iEpochTimeStamp = (*itr).iEpochTimeStamp - dReuterDataLatency + iTickLatency;
                if(itr == vGridData.begin())
                {
                    currentTimeStampIndex = (long)((*itr).iEpochTimeStamp / 60000000);
                    iNumGridPointsGrouped = 1;
                    itrGroupStart = vGridData.begin();
                    itrGroupEnd = vGridData.begin() + 1;
                }
                else
                {
                    // check grouped tick data
                    if((long)((*itr).iEpochTimeStamp / 60000000) != currentTimeStampIndex)
                    {
                        if(iNumGridPointsGrouped != 0)
                        {
                            GridData* pDataArray = new GridData[iNumGridPointsGrouped];
                            std::copy(itrGroupStart, itrGroupEnd, pDataArray);
                            hsize_t cDim[] = {iNumGridPointsGrouped};
                            DataSpace cSpace(sizeof(cDim)/sizeof(hsize_t), cDim);
                            DataSet* pDataSet = new DataSet(pH5TickFile->createDataSet(to_string(currentTimeStampIndex), cGridDataType, cSpace));
                            pDataSet->write(pDataArray, cGridDataType);
                            delete pDataArray;
                            delete pDataSet;
                        }

                        for(int i = currentTimeStampIndex + 1; i < (long)((*itr).iEpochTimeStamp / 60000000); i++)
                        {
                            hsize_t cDim[] = {0};
                            DataSpace cSpace(sizeof(cDim)/sizeof(hsize_t), cDim);
                            pH5TickFile->createDataSet(to_string(i), cGridDataType, cSpace);
                        }

                        currentTimeStampIndex = (long)((*itr).iEpochTimeStamp / 60000000);
                        iNumGridPointsGrouped = 1;
                        itrGroupStart = itrGroupEnd;
                        itrGroupEnd = itrGroupEnd + 1;
                    }
                    else
                    {
                        itrGroupEnd++;
                        iNumGridPointsGrouped++;
                    }   

                    vector<GridData>::iterator lastItr = itr - 1;

                    // check second grid data
                    for(int i = 0;i != vOutputSecFileNames.size();i++)
                    {
                        long lastPrintSecondIdx = (long)((*lastItr).iEpochTimeStamp + vSecLatencies[i] - iTickLatency) / 1000000;
                        long currentPrintSecondIdx = (long)((*itr).iEpochTimeStamp + vSecLatencies[i] - iTickLatency) / 1000000;
                        if(currentPrintSecondIdx != lastPrintSecondIdx)
                        {
                            for(long secondIdx = lastPrintSecondIdx+1;secondIdx <= currentPrintSecondIdx;secondIdx++)
                            {
                                GridData cNewSecondDataPoint;
                                cNewSecondDataPoint.iEpochTimeStamp = secondIdx * 1000000 ;
                                cNewSecondDataPoint.dBid = (*lastItr).dBid;
                                cNewSecondDataPoint.iBidInTicks = (*lastItr).iBidInTicks;
                                cNewSecondDataPoint.iBidSize = (*lastItr).iBidSize;
                                cNewSecondDataPoint.dAsk = (*lastItr).dAsk;
                                cNewSecondDataPoint.iAskInTicks = (*lastItr).iAskInTicks;
                                cNewSecondDataPoint.iAskSize = (*lastItr).iAskSize;
                                cNewSecondDataPoint.dLast = (*lastItr).dLast;
                                cNewSecondDataPoint.iLastInTicks = (*lastItr).iLastInTicks;
                                cNewSecondDataPoint.iTradeSize = (*lastItr).iTradeSize;
                                cNewSecondDataPoint.iAccumuTradeSize = (*lastItr).iAccumuTradeSize;
                                cNewSecondDataPoint.dWeightedMid = (*lastItr).dWeightedMid;
                                cNewSecondDataPoint.dWeightedMidInTicks = (*lastItr).dWeightedMidInTicks;

                                vSecondGridDataSets[i].push_back(cNewSecondDataPoint);
                            }
                        }
                    }

                    //check min grid data
                    for(int i = 0;i != vOutputMinFileNames.size();i++)
                    {
                        long lastPrintMinIdx = (long)((*lastItr).iEpochTimeStamp + vMinLatencies[i] - iTickLatency) / 60000000;
                        long currentPrintMinIdx = (long)((*itr).iEpochTimeStamp + vMinLatencies[i] - iTickLatency) / 60000000;
                        if(currentPrintMinIdx != lastPrintMinIdx)
                        {
                            for(long minIdx = lastPrintMinIdx+1;minIdx <= currentPrintMinIdx;minIdx++)
                            {
                                GridData cNewMinDataPoint;
                                cNewMinDataPoint.iEpochTimeStamp = minIdx * 60000000 ;
                                cNewMinDataPoint.dBid = (*lastItr).dBid;
                                cNewMinDataPoint.iBidInTicks = (*lastItr).iBidInTicks;
                                cNewMinDataPoint.iBidSize = (*lastItr).iBidSize;
                                cNewMinDataPoint.dAsk = (*lastItr).dAsk;
                                cNewMinDataPoint.iAskInTicks = (*lastItr).iAskInTicks;
                                cNewMinDataPoint.iAskSize = (*lastItr).iAskSize;
                                cNewMinDataPoint.dLast = (*lastItr).dLast;
                                cNewMinDataPoint.iLastInTicks = (*lastItr).iLastInTicks;
                                cNewMinDataPoint.iTradeSize = (*lastItr).iTradeSize;
                                cNewMinDataPoint.iAccumuTradeSize = (*lastItr).iAccumuTradeSize;
                                cNewMinDataPoint.dWeightedMid = (*lastItr).dWeightedMid;
                                cNewMinDataPoint.dWeightedMidInTicks = (*lastItr).dWeightedMidInTicks;

                                vMinGridDataSets[i].push_back(cNewMinDataPoint);
                            }
                        }
                    }

                    //check min cet grid data
                    for(int i = 0;i != vOutputMinCETFileNames.size();i++)
                    {
                        long lastPrintMinIdx = (long)((*lastItr).iEpochTimeStamp + vMinLatencies[i] - iTickLatency + iCETDiffUTC * 1000000) / 60000000;
                        long currentPrintMinIdx = (long)((*itr).iEpochTimeStamp + vMinLatencies[i] - iTickLatency + iCETDiffUTC * 1000000) / 60000000;
                        if(currentPrintMinIdx != lastPrintMinIdx)
                        {
                            for(long minIdx = lastPrintMinIdx+1;minIdx <= currentPrintMinIdx;minIdx++)
                            {
                                GridData cNewMinDataPoint;
                                cNewMinDataPoint.iEpochTimeStamp = minIdx * 60000000 ;
                                cNewMinDataPoint.dBid = (*lastItr).dBid;
                                cNewMinDataPoint.iBidInTicks = (*lastItr).iBidInTicks;
                                cNewMinDataPoint.iBidSize = (*lastItr).iBidSize;
                                cNewMinDataPoint.dAsk = (*lastItr).dAsk;
                                cNewMinDataPoint.iAskInTicks = (*lastItr).iAskInTicks;
                                cNewMinDataPoint.iAskSize = (*lastItr).iAskSize;
                                cNewMinDataPoint.dLast = (*lastItr).dLast;
                                cNewMinDataPoint.iLastInTicks = (*lastItr).iLastInTicks;
                                cNewMinDataPoint.iTradeSize = (*lastItr).iTradeSize;
                                cNewMinDataPoint.iAccumuTradeSize = (*lastItr).iAccumuTradeSize;
                                cNewMinDataPoint.dWeightedMid = (*lastItr).dWeightedMid;
                                cNewMinDataPoint.dWeightedMidInTicks = (*lastItr).dWeightedMidInTicks;

                                vMinCETGridDataSets[i].push_back(cNewMinDataPoint);
                            }
                        }
                    }

                    //check 5 min cet grid data
                    for(int i = 0;i != vOutputMinFileNames.size();i++)
                    {
                        long lastPrintMinIdx = (long)((*lastItr).iEpochTimeStamp + vMinLatencies[i] - iTickLatency + iCETDiffUTC * 1000000) / 300000000;
                        long currentPrintMinIdx = (long)((*itr).iEpochTimeStamp + vMinLatencies[i] - iTickLatency + iCETDiffUTC * 1000000) / 300000000;
                        if(currentPrintMinIdx != lastPrintMinIdx)
                        {
                            for(long minIdx = lastPrintMinIdx+1;minIdx <= currentPrintMinIdx;minIdx++)
                            {
                                GridData cNewMinDataPoint;
                                cNewMinDataPoint.iEpochTimeStamp = minIdx * 300000000;
                                cNewMinDataPoint.dBid = (*lastItr).dBid;
                                cNewMinDataPoint.iBidInTicks = (*lastItr).iBidInTicks;
                                cNewMinDataPoint.iBidSize = (*lastItr).iBidSize;
                                cNewMinDataPoint.dAsk = (*lastItr).dAsk;
                                cNewMinDataPoint.iAskInTicks = (*lastItr).iAskInTicks;
                                cNewMinDataPoint.iAskSize = (*lastItr).iAskSize;
                                cNewMinDataPoint.dLast = (*lastItr).dLast;
                                cNewMinDataPoint.iLastInTicks = (*lastItr).iLastInTicks;
                                cNewMinDataPoint.iTradeSize = (*lastItr).iTradeSize;
                                cNewMinDataPoint.iAccumuTradeSize = (*lastItr).iAccumuTradeSize;
                                cNewMinDataPoint.dWeightedMid = (*lastItr).dWeightedMid;
                                cNewMinDataPoint.dWeightedMidInTicks = (*lastItr).dWeightedMidInTicks;

                                v5MinGridDataSets[i].push_back(cNewMinDataPoint);
                            }
                        }
                    }

                }
			}

            if(iNumGridPointsGrouped != 0)
            {
                GridData* pDataArray = new GridData[iNumGridPointsGrouped];
                std::copy(itrGroupStart, itrGroupEnd, pDataArray);
                hsize_t cDim[] = {iNumGridPointsGrouped};
                DataSpace cSpace(sizeof(cDim)/sizeof(hsize_t), cDim);
                DataSet* pDataSet = new DataSet(pH5TickFile->createDataSet(to_string(currentTimeStampIndex), cGridDataType, cSpace));
                pDataSet->write(pDataArray, cGridDataType);
                delete pDataArray;
                delete pDataSet;
            }

            delete pH5TickFile;

            for(int i = 0;i != vOutputSecFileNames.size();i++)
            {
                H5File* pH5SecondFile = new H5File(vOutputSecFileNames[i], H5F_ACC_TRUNC);
                long iNumSecondGridPoint = vSecondGridDataSets[i].size();
                GridData* pSecondDataArray = new GridData[iNumSecondGridPoint];
                std::copy(vSecondGridDataSets[i].begin(), vSecondGridDataSets[i].end(), pSecondDataArray); 
                hsize_t cSecondDim[] = {iNumSecondGridPoint};
                DataSpace cSecSpace(sizeof(cSecondDim)/sizeof(hsize_t), cSecondDim);
                DataSet* pSecondDataSet = new DataSet(pH5SecondFile->createDataSet("GridData", cGridDataType, cSecSpace));
                pSecondDataSet->write(pSecondDataArray, cGridDataType);
                delete pSecondDataArray;
                delete pSecondDataSet;
                delete pH5SecondFile;
            }

            for(int i = 0;i != vOutputMinFileNames.size();i++)
            {
                H5File* pH5MinFile = new H5File(vOutputMinFileNames[i], H5F_ACC_TRUNC);
                long iNumMinGridPoint = vMinGridDataSets[i].size();
                GridData* pMinDataArray = new GridData[iNumMinGridPoint];
                std::copy(vMinGridDataSets[i].begin(), vMinGridDataSets[i].end(), pMinDataArray); 
                hsize_t cMinDim[] = {iNumMinGridPoint};
                DataSpace cMinSpace(sizeof(cMinDim)/sizeof(hsize_t), cMinDim);
                DataSet* pMinDataSet = new DataSet(pH5MinFile->createDataSet("GridData", cGridDataType, cMinSpace));
                pMinDataSet->write(pMinDataArray, cGridDataType);
                delete pMinDataArray;
                delete pMinDataSet;
                delete pH5MinFile;
            }

            for(int i = 0;i != vOutputMinCETFileNames.size();i++)
            {
                H5File* pH5MinCETFile = new H5File(vOutputMinCETFileNames[i], H5F_ACC_TRUNC);
                long iNumMinCETGridPoint = vMinCETGridDataSets[i].size();
                GridData* pMinCETDataArray = new GridData[iNumMinCETGridPoint];
                std::copy(vMinCETGridDataSets[i].begin(), vMinCETGridDataSets[i].end(), pMinCETDataArray); 
                hsize_t cMinCETDim[] = {iNumMinCETGridPoint};
                DataSpace cMinCETSpace(sizeof(cMinCETDim)/sizeof(hsize_t), cMinCETDim);
                DataSet* pMinCETDataSet = new DataSet(pH5MinCETFile->createDataSet("GridData", cGridDataType, cMinCETSpace));
                pMinCETDataSet->write(pMinCETDataArray, cGridDataType);
                delete pMinCETDataArray;
                delete pMinCETDataSet;
                delete pH5MinCETFile;
            }

            for(int i = 0;i != vOutput5MinFileNames.size();i++)
            {
                H5File* pH55MinFile = new H5File(vOutput5MinFileNames[i], H5F_ACC_TRUNC);
                long iNum5MinGridPoint = v5MinGridDataSets[i].size();
                GridData* p5MinDataArray = new GridData[iNum5MinGridPoint];
                std::copy(v5MinGridDataSets[i].begin(), v5MinGridDataSets[i].end(), p5MinDataArray); 
                hsize_t c5MinDim[] = {iNum5MinGridPoint};
                DataSpace c5MinSpace(sizeof(c5MinDim)/sizeof(hsize_t), c5MinDim);
                DataSet* p5MinDataSet = new DataSet(pH55MinFile->createDataSet("GridData", cGridDataType, c5MinSpace));
                p5MinDataSet->write(p5MinDataArray, cGridDataType);
                delete p5MinDataArray;
                delete p5MinDataSet;
                delete pH55MinFile;
            }
		}
	}
	else
	{
		cerr << "ERROR: " << sInputFileName << " Cannot open input file " << sInputFileName << ". No output generated \n";
	}

	return 0;
}
