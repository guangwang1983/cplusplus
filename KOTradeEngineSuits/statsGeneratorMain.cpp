#include <fstream>
#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include <glob.h>
#include <H5Cpp.h>
#include <cstdio>
#include <map>
#include <sys/stat.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time_adjustor.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/local_timezone_defs.hpp>

#include "base/EXMA.h"
#include "base/WeightedStdev.h"
#include "EngineInfra/QuoteData.h"
#include "EngineInfra/KOEpochTime.h"
#include "EngineInfra/SystemClock.h"

using namespace H5;
using namespace KO;
using namespace std;

struct WhitenDataPoint
{
    long iEpochTimeStamp;
    double dValue;
};

struct DailyLegStat
{
    double dEXMA;
    long   iEXMANumDataPoints;
    double dStdevEXMA;
    double dStdevSqrdEXMA;
    long   iStdevNumDataPoints;
    double dStdevAdjustment;
    double dLastMid;
};

void saveOvernightStats(string sStatFileName, map<boost::gregorian::date, DailyLegStat> mDailyLegStats)
{
    stringstream cStringStream;
    cStringStream.precision(20);

    for(map< boost::gregorian::date, DailyLegStat>::iterator itr = mDailyLegStats.begin(); itr != mDailyLegStats.end(); ++itr)
    {
        cStringStream << to_iso_string(itr->first) << ","
                      << itr->second.dEXMA << ","
                      << itr->second.iEXMANumDataPoints << ","
                      << itr->second.dStdevEXMA << ","
                      << itr->second.dStdevSqrdEXMA << ","
                      << itr->second.iStdevNumDataPoints << ","
                      << itr->second.dStdevAdjustment << ","
                      << itr->second.dLastMid << "\n";
    }

    const unsigned long length = 3000000;
    char buffer[length];
    fstream ofsOvernightStatFile;
    ofsOvernightStatFile.rdbuf()->pubsetbuf(buffer, length);

    ofsOvernightStatFile.open(sStatFileName.c_str(), fstream::out);
    if(ofsOvernightStatFile.is_open())
    {
        ofsOvernightStatFile.precision(20);
        ofsOvernightStatFile << cStringStream.str();
        ofsOvernightStatFile.close();
    }
    else
    {
        std::cerr << "Failed to update overnight stat file " << sStatFileName << ".";
    }
}

void loadOvernightStats(string sStatFileName, map<boost::gregorian::date, DailyLegStat>& mDailyLegStats)
{
    fstream ifsOvernightStatFile;
    ifsOvernightStatFile.open(sStatFileName.c_str(), fstream::in);
    if(ifsOvernightStatFile.is_open())
    {
        while(!ifsOvernightStatFile.eof())
        {
            char sNewLine[4096];
            ifsOvernightStatFile.getline(sNewLine, sizeof(sNewLine));

            if(strlen(sNewLine) != 0 && sNewLine[0] != '#')
            {
                std::istringstream cDailyStatStream(sNewLine);
                string sElement;
                boost::gregorian::date cDate;

                bool bDateIsValid = false;

                std::getline(cDailyStatStream, sElement, ',');
                try
                {
                    cDate = boost::gregorian::from_undelimited_string(sElement);
                    bDateIsValid = true;
                }
                catch (...)
                {
                    cerr << "Invalid date in overnightstats.cfg: " << sElement << ".";

                    bDateIsValid = false;
                }

                if(bDateIsValid == true)
                {
                    bool bStatValid = true;

                    DailyLegStat cLegStat;

                    string sEXMA;
                    string sEXMANumDataPoints;
                    string sStdevEXMA;
                    string sStdevSqrdEXMA;
                    string sStdevNumDataPoints;
                    string sStdevAdjustment;
                    string sLastMid;

                    bStatValid = bStatValid && std::getline(cDailyStatStream, sEXMA, ',');
                    bStatValid = bStatValid && std::getline(cDailyStatStream, sEXMANumDataPoints, ',');
                    bStatValid = bStatValid && std::getline(cDailyStatStream, sStdevEXMA, ',');
                    bStatValid = bStatValid && std::getline(cDailyStatStream, sStdevSqrdEXMA, ',');
                    bStatValid = bStatValid && std::getline(cDailyStatStream, sStdevNumDataPoints, ',');
                    bStatValid = bStatValid && std::getline(cDailyStatStream, sStdevAdjustment, ',');
                    bStatValid = bStatValid && std::getline(cDailyStatStream, sLastMid, ',');

                    cLegStat.dEXMA = stod(sEXMA);
                    cLegStat.iEXMANumDataPoints = atoi(sEXMANumDataPoints.c_str());
                    cLegStat.dStdevEXMA = stod(sStdevEXMA);
                    cLegStat.dStdevSqrdEXMA = stod(sStdevSqrdEXMA);
                    cLegStat.iStdevNumDataPoints = atoi(sStdevNumDataPoints.c_str());
                    cLegStat.dStdevAdjustment = stod(sStdevAdjustment);
                    cLegStat.dLastMid = stod(sLastMid);

                    if(bStatValid == true)
                    {
                        mDailyLegStats[cDate] = cLegStat;
                    } 
                }
            }
        }
    }
}

void loadSecGridData(string sDataFileName, GridData*& pOutputData, long& iDataLength)
{
    H5File cFile (sDataFileName, H5F_ACC_RDONLY);
    DataSet cDataSet = cFile.openDataSet("GridData");

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

    hsize_t cDim[1];
    DataSpace cDataSpace = cDataSet.getSpace();
    cDataSpace.getSimpleExtentDims(cDim);

    iDataLength = cDim[0];
    pOutputData = new GridData [iDataLength];
    cDataSet.read(pOutputData, cGridDataType);
}

int main(int argc, char *argv[])
{
    H5::Exception::dontPrint();

    string sKORootSymbol;
    string sUnderlineContract;
    string sLocation;
    string sDate;
    string sDriftLength;
    string sVolLength;
    long iDriftLength;
    long iVolLength;
    string sStartTime;
    string sEndTime;
    string sRunPath;
    string sTempFilePath;

    map< boost::gregorian::date, DailyLegStat > mDailyLegStats;

    boost::program_options::options_description _cAllOptions;
    _cAllOptions.add_options()("KORootSymbol", boost::program_options::value<string>(&sKORootSymbol), "KO Root Symbol");
    _cAllOptions.add_options()("UnderlineContract", boost::program_options::value<string>(&sUnderlineContract), "Underline Contract");
    _cAllOptions.add_options()("Location", boost::program_options::value<string>(&sLocation), "Location");
    _cAllOptions.add_options()("Date", boost::program_options::value<string>(&sDate), "Date to be simulated");
    _cAllOptions.add_options()("Drift", boost::program_options::value<string>(&sDriftLength), "Drift Length in Seconds");
    _cAllOptions.add_options()("SpreadVola", boost::program_options::value<string>(&sVolLength), "Volatility Length in Days");
    _cAllOptions.add_options()("StartTime", boost::program_options::value<string>(&sStartTime), "Start Time for Stats HHMMSS");
    _cAllOptions.add_options()("EndTime", boost::program_options::value<string>(&sEndTime), "End Time for Stats HHMMSS");
    _cAllOptions.add_options()("RunPath", boost::program_options::value<string>(&sRunPath), "Run Path");
    _cAllOptions.add_options()("TempFilePath", boost::program_options::value<string>(&sTempFilePath), "Temp File Path");

    if(argc == 1)
    {
        cout << _cAllOptions;
        return 0;
    }

    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, _cAllOptions), vm);
    boost::program_options::notify(vm);

    iDriftLength = atoi(sDriftLength.c_str());
    iVolLength = atoi(sVolLength.c_str());

    KOEpochTime cStartTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(sDate, sStartTime.substr(0,2), sStartTime.substr(2,2), sStartTime.substr(4,2));
    KOEpochTime cEndTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(sDate, sEndTime.substr(0,2), sEndTime.substr(2,2), sEndTime.substr(4,2));
    long iRealVolLength = (cEndTime.sec() - cStartTime.sec()) / 60 * iVolLength;

    string sYear = sDate.substr(0, 4);

    string sEODStatsFileName = sRunPath + "/" + sStartTime + "_" + sEndTime + "_overnightstats.csv";

    loadOvernightStats(sEODStatsFileName, mDailyLegStats);

    string sWhitenFileName = sRunPath + "/" + sStartTime + "_" + sEndTime + "_whiten_1m_" + sYear + ".h5";

    // prepare data
    GridData* pData = NULL;
    long iDataLength = 0;
    string sDataFileBaseName = sLocation + "_" + sUnderlineContract + "_" + sDate.substr(0, 4) + "." + sDate.substr(4, 2) + "." + sDate.substr(6, 2) + "_1s.h5";

    string sDataFile = string(getenv("HDF5SECONDDATAREADPATH")) + "/" + sLocation + "/" + sKORootSymbol + "/" + sDataFileBaseName;
    struct stat buffer;
    if(stat(sDataFile.c_str(), &buffer) == 0)
    {
        loadSecGridData(sDataFile, pData, iDataLength);
    }
    else
    {
        std::cerr << "Cannot find date file " << sDataFile << ". No stats generated \n";
    }

    // Init stats 
    EXMA cEXMA(iDriftLength);
    WeightedStdev cWeightedStdev(iRealVolLength);
    double dLastMid;

    boost::gregorian::date cStatDay;
    boost::gregorian::date cTodayDate = boost::gregorian::from_undelimited_string(sDate);

    if(cTodayDate.day_of_week().as_number() != 1)
    {
        cStatDay = cTodayDate - boost::gregorian::date_duration(1);
    }
    else
    {
        cStatDay = cTodayDate - boost::gregorian::date_duration(3);
    }

    map<boost::gregorian::date, DailyLegStat>::iterator statItr = mDailyLegStats.end();

    for(int daysToLookback = 0; daysToLookback < 10; daysToLookback++)
    {
        statItr = mDailyLegStats.find(cStatDay);

        if(statItr != mDailyLegStats.end())
        {
            cEXMA.setNewEXMA(statItr->second.dEXMA, statItr->second.iEXMANumDataPoints);
//cerr << statItr->second.dEXMA << " " << statItr->second.iEXMANumDataPoints << "\n";
            cWeightedStdev.dsetNewEXMA(statItr->second.dStdevSqrdEXMA, statItr->second.dStdevEXMA, statItr->second.iStdevNumDataPoints);
            cWeightedStdev.applyAdjustment(statItr->second.dStdevAdjustment);
            break;
        }
        else
        {
            cerr << "Cannot found overnigth stat from date " << to_iso_string(cStatDay) << "\n";
        }

        int iTodayWeek = cStatDay.day_of_week().as_number();
        if(iTodayWeek != 1)
        {
            cStatDay = cStatDay - boost::gregorian::date_duration(1);
        }
        else
        {
            cStatDay = cStatDay - boost::gregorian::date_duration(3);
        }
    }

//cerr.precision(20);

    vector<WhitenDataPoint> vOutputData;
    for(int i = 0; i < iDataLength; i++)
    {
        bool bDataValid = true;
        bDataValid = bDataValid && pData[i].iEpochTimeStamp >= cStartTime.igetPrintable();
        bDataValid = bDataValid && pData[i].iEpochTimeStamp <= cEndTime.igetPrintable();
        bDataValid = bDataValid && pData[i].iBidSize > 0;
        bDataValid = bDataValid && pData[i].iAskSize > 0;
        bDataValid = bDataValid && pData[i].dAsk - pData[i].dBid > 0.00000001;

        if(sKORootSymbol[0] == 'X')
        {
            bDataValid = bDataValid && pData[i].iAccumuTradeSize != 0;
            bDataValid = bDataValid && pData[i].iAskInTicks - pData[i].iBidInTicks <= 5;
        }

        if(bDataValid)
        {
            cEXMA.dnewData(pData[i].dWeightedMid);
            dLastMid = pData[i].dWeightedMid;
            if((pData[i].iEpochTimeStamp / 1000000) % 60 == 0 &&
                cEXMA.bgetEXMAValid() == true)
            {
//cerr << pData[i].iEpochTimeStamp << " RH3 adding " << pData[i].dWeightedMid << " to stdev \n";
                cWeightedStdev.dnewData(pData[i].dWeightedMid);

                double dWhiten = 0;
                if(cWeightedStdev.bgetStdevValid() == true)
                {
                    dWhiten = (pData[i].dWeightedMid - cEXMA.dgetEXMA()) / cWeightedStdev.dgetWeightedStdev();
                }

                WhitenDataPoint cNewWhitenDataPoint;
                cNewWhitenDataPoint.iEpochTimeStamp = pData[i].iEpochTimeStamp;
                cNewWhitenDataPoint.dValue = dWhiten;
                
                vOutputData.push_back(cNewWhitenDataPoint);
            }
        }
    }

    // load roll delta
    double dRollDelta = 0;
    string sRollDeltaFileName = sTempFilePath + "/RollDelta.csv";
    fstream ifsRollDelta;
    ifsRollDelta.open(sRollDeltaFileName.c_str(), fstream::in);

    if(ifsRollDelta.is_open())
    {
        char sNewLine[512];
        ifsRollDelta.getline(sNewLine, sizeof(sNewLine));

        string sRollDelta = sNewLine;
        sRollDelta.erase(remove(sRollDelta.begin(), sRollDelta.end(), '\n'), sRollDelta.end());
        
        dRollDelta = stod(sRollDelta);
    }
    else
    {
        std::cerr << "Cannot find RollDelat.csv file. No roll delta applied\n";
    }

    cEXMA.applyAdjustment(dRollDelta);
    cWeightedStdev.applyAdjustment(dRollDelta);

    if(mDailyLegStats.find(cTodayDate) != mDailyLegStats.end())
    {
        mDailyLegStats[cTodayDate].dEXMA = cEXMA.dgetEXMA();
        mDailyLegStats[cTodayDate].iEXMANumDataPoints = cEXMA.igetNumDataPoints();
        mDailyLegStats[cTodayDate].dStdevEXMA = cWeightedStdev.dgetEXMA();
        mDailyLegStats[cTodayDate].dStdevSqrdEXMA = cWeightedStdev.dgetSqrdEXMA();
        mDailyLegStats[cTodayDate].iStdevNumDataPoints = cWeightedStdev.igetNumDataPoint();
        mDailyLegStats[cTodayDate].dStdevAdjustment = cWeightedStdev.dgetAdjustment();
        mDailyLegStats[cTodayDate].dLastMid = dLastMid;
    }
    else
    {
        DailyLegStat cNewLegStat;
        cNewLegStat.dEXMA  = cEXMA.dgetEXMA();
        cNewLegStat.iEXMANumDataPoints = cEXMA.igetNumDataPoints();
        cNewLegStat.dStdevEXMA = cWeightedStdev.dgetEXMA();
        cNewLegStat.dStdevSqrdEXMA = cWeightedStdev.dgetSqrdEXMA();
        cNewLegStat.iStdevNumDataPoints = cWeightedStdev.igetNumDataPoint();
        cNewLegStat.dStdevAdjustment = cWeightedStdev.dgetAdjustment();
        cNewLegStat.dLastMid = dLastMid;

        mDailyLegStats[cTodayDate] = cNewLegStat;
    }

    if(pData != NULL)
    {
/*
        // write down data
        H5File cH5File;
        try
        {
            cH5File = H5File(sWhitenFileName, H5F_ACC_RDWR);
        }
        catch(FileIException not_found_error)
        {
            cH5File = H5File(sWhitenFileName, H5F_ACC_EXCL);
        }

        bool bDataSetExists = false;
        try
        {
            cH5File.openDataSet(sDate);
            bDataSetExists = true;
        }
        catch(FileIException not_found_error)
        {
        }

        if(bDataSetExists == true)
        {
            H5Ldelete(cH5File.getId(), sDate.c_str(), H5P_DEFAULT);
            cH5File.close();
            cH5File = H5File(sWhitenFileName, H5F_ACC_RDWR);
        }
        
        WhitenDataPoint* pWhitenDataPoints = new WhitenDataPoint[vOutputData.size()];
        std::copy(vOutputData.begin(), vOutputData.end(), pWhitenDataPoints);
        hsize_t cDim[] = {vOutputData.size()};
        DataSpace cSpace(sizeof(cDim)/sizeof(hsize_t), cDim);

        CompType cWhitenDataType(sizeof(WhitenDataPoint));
        cWhitenDataType.insertMember("TIME", HOFFSET(WhitenDataPoint, iEpochTimeStamp), PredType::NATIVE_LONG);
        cWhitenDataType.insertMember("WHITEN", HOFFSET(WhitenDataPoint, dValue), PredType::NATIVE_DOUBLE);

        DataSet* pDataSet = new DataSet(cH5File.createDataSet(sDate, cWhitenDataType, cSpace));
        pDataSet->write(pWhitenDataPoints, cWhitenDataType);

        delete pDataSet;
        delete pWhitenDataPoints;
        delete pData;
*/
        saveOvernightStats(sEODStatsFileName, mDailyLegStats);
    }
}
