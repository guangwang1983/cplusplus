#include <iostream>
#include <string>
#include <string.h>
#include <fstream>
#include <vector>
#include <stdlib.h>
#include <sstream>
#include <boost/program_options.hpp>
#include <H5Cpp.h>
#include <algorithm>
#include <sys/stat.h>
#include <glob.h>
#include "ExecutorSim/ExecutorSim.h"

using namespace H5;
using namespace std;
using namespace KO;

struct signalCompare
{
    bool operator()(const TradeSignal& left, const TradeSignal& right)
    {
        return left.iEpochTimeStamp < right.iEpochTimeStamp;
    }
}signalCompareObj;

void loadNewSignalFile(const string& sSignalFileName, vector<TradeSignal>& vCombinedSignals, long iPortfolioID, string sDate)
{
    H5File cTradeSignalH5File (sSignalFileName, H5F_ACC_RDONLY);
    CompType cTradeSignalType(sizeof(TradeSignal));

    cTradeSignalType.insertMember("PORTFOLIO_ID", HOFFSET(TradeSignal, iPortfolioID), PredType::NATIVE_LONG);
    cTradeSignalType.insertMember("TIME", HOFFSET(TradeSignal, iEpochTimeStamp), PredType::NATIVE_LONG);
    cTradeSignalType.insertMember("DESIRED_POS", HOFFSET(TradeSignal, iDesiredPos), PredType::NATIVE_LONG);
    cTradeSignalType.insertMember("SIGNAL_STATE", HOFFSET(TradeSignal, iSignalState), PredType::NATIVE_LONG);
    cTradeSignalType.insertMember("MARKET_ORDER", HOFFSET(TradeSignal, bMarketOrder), PredType::NATIVE_B8);

    bool bDataSetExists = false;
    DataSet cDataSet;

    try
    {
        cDataSet = cTradeSignalH5File.openDataSet(sDate);
        bDataSetExists = true;         
    }
    catch(FileIException& not_found_error)
    {
        //cerr << "DataSet " << iDataSetIdx << " not found \n";
    }

    if(bDataSetExists == true)
    {
        hsize_t cDim[1];
        DataSpace cDataSpace = cDataSet.getSpace();
        cDataSpace.getSimpleExtentDims(cDim);

        if(cDim[0] > 0)
        {
            TradeSignal cLoadedSignalArray [cDim[0]];
            cDataSet.read(cLoadedSignalArray, cTradeSignalType);
            vector<TradeSignal> vNewSignals(cLoadedSignalArray, cLoadedSignalArray+cDim[0]);

            for(vector<TradeSignal>::iterator itr = vNewSignals.begin(); itr != vNewSignals.end();itr++)
            {
                itr->iPortfolioID = iPortfolioID;
            }

            vCombinedSignals.insert(vCombinedSignals.end(), vNewSignals.begin(), vNewSignals.end());
        }
    }
}

int main(int argc, char *argv[])
{
    H5::Exception::dontPrint();

    string sSimConfigPrefix;
    string sDate;
    string sDailyTradingContract;
    long iSubmitLatency;
    long iAmendLatency;
    string sDataPath;
    double dDailyTickSize;
    vector<string> vSignalFileDirs;
    vector<string> vOutputFileDirs;
    string sSimLocation;
    string sKORootSymbol;
    string sLogPath;
    bool bWriteLog;
    bool bLogMarketData;
    int iIOCSpreadWidthLimit;
    int iArticifialSpread;
    
    boost::program_options::options_description _cAllOptions;
    _cAllOptions.add_options()("SimConfigPrefix", boost::program_options::value<string>(&sSimConfigPrefix), "Prefix of configs to be simulated");
    _cAllOptions.add_options()("Date", boost::program_options::value<string>(&sDate), "Date to be simulated");
    _cAllOptions.add_options()("DailyTradingContract", boost::program_options::value<string>(&sDailyTradingContract), "Daily trading contract");
    _cAllOptions.add_options()("DailyTickSize", boost::program_options::value<double>(&dDailyTickSize), "Daily tick size");
    _cAllOptions.add_options()("SubmitLatency", boost::program_options::value<long>(&iSubmitLatency), "Submit Latency");
    _cAllOptions.add_options()("AmendLatency", boost::program_options::value<long>(&iAmendLatency), "Amend Latency");
    _cAllOptions.add_options()("DataPath", boost::program_options::value<string>(&sDataPath), "Data Path");
    _cAllOptions.add_options()("KORootSymbol", boost::program_options::value<string>(&sKORootSymbol), "KO Root Symbol");
    _cAllOptions.add_options()("SimLocation", boost::program_options::value<string>(&sSimLocation), "Sim Location");

    _cAllOptions.add_options()("LogPath", boost::program_options::value<string>(&sLogPath), "Log Path");
    _cAllOptions.add_options()("WriteLog", boost::program_options::value<bool>(&bWriteLog), "Write Log");
    _cAllOptions.add_options()("LogMarketData", boost::program_options::value<bool>(&bLogMarketData), "Log Market Data");
    _cAllOptions.add_options()("IOCSpreadWidthLimit", boost::program_options::value<int>(&iIOCSpreadWidthLimit), "IOC Spread Width Limit");
    _cAllOptions.add_options()("ArticifialSpread", boost::program_options::value<int>(&iArticifialSpread), "IOC Spread Width Limit");

    if(argc == 1)
    {
        cout << _cAllOptions;
        return 0;
    }

    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, _cAllOptions), vm);
    boost::program_options::notify(vm);

    // find all configs dir
    glob_t cGlobResult;
    glob((sSimConfigPrefix + ".*").c_str(), GLOB_TILDE, NULL, &cGlobResult);
    for(unsigned int i = 0; i < cGlobResult.gl_pathc; i++)
    {
        vSignalFileDirs.push_back(cGlobResult.gl_pathv[i]);
        vOutputFileDirs.push_back(cGlobResult.gl_pathv[i]);
    }

    // prepare hdf5 result files
    CompType cSimTransactionType(sizeof(SimTransaction));

    cSimTransactionType.insertMember("TIME", HOFFSET(SimTransaction, iEpochTimeStamp), PredType::NATIVE_LONG);

    hid_t strtype = H5Tcopy (H5T_C_S1);
    H5Tset_size (strtype, 12);
    cSimTransactionType.insertMember("PRODUCT", HOFFSET(SimTransaction, sProduct), strtype);
    cSimTransactionType.insertMember("QTY", HOFFSET(SimTransaction, iQty), PredType::NATIVE_LONG);
    cSimTransactionType.insertMember("PRICE", HOFFSET(SimTransaction, dPrice), PredType::NATIVE_DOUBLE);

    hid_t strtype1 = H5Tcopy (H5T_C_S1);
    H5Tset_size(strtype1, 12);
    cSimTransactionType.insertMember("DATE", HOFFSET(SimTransaction, sDate), strtype1);
    
    string sYear = sDate.substr(0,4);
    
    vector<H5File> vResultH5Files;
    for(vector<string>::iterator itr = vOutputFileDirs.begin();
        itr != vOutputFileDirs.end();
        itr++)
    {
        H5File cNewH5File;

        try
        {
            cNewH5File = H5File(*itr + "/transactions" + sYear + ".h5", H5F_ACC_RDWR);
        }
        catch(FileIException& not_found_error)
        {
            cNewH5File = H5File(*itr + "/transactions" + sYear + ".h5", H5F_ACC_EXCL);
        }

        bool bDataSetExists = false;
        try
        {
            cNewH5File.openDataSet(sDate);    
            bDataSetExists = true;
        }
        catch(FileIException& not_found_error)
        {
        }
        
        if(bDataSetExists == true)
        {
            H5Ldelete(cNewH5File.getId(), sDate.c_str(), H5P_DEFAULT);
            cNewH5File.close();
            cNewH5File = H5File (*itr + "/transactions" + sYear + ".h5", H5F_ACC_RDWR);
        }

        vResultH5Files.push_back(cNewH5File);
    }

    // prepare data
    string sDataFileBaseName = sSimLocation + "_" + sDailyTradingContract + "_" + sDate.substr(0, 4) + "." + sDate.substr(4, 2) + "." + sDate.substr(6, 2) + "_tick.h5";
 
    string sDataFile = string(getenv("HDF5TICKDATAREADPATH")) + "/" + sSimLocation + "/" + sKORootSymbol + "/" + sDataFileBaseName;

    struct stat buffer;
    if(stat(sDataFile.c_str(), &buffer) == 0)
    {
        bool bIOC = false;
        if(sKORootSymbol[0] == 'C')
        {
            bIOC = true;
        }

        ExecutorSim cExecutorSim(sDailyTradingContract, iSubmitLatency, iAmendLatency, dDailyTickSize, sDataFile, 999999999, sDate, sLogPath, bWriteLog, false, bLogMarketData, bIOC, iIOCSpreadWidthLimit, iArticifialSpread);
        cExecutorSim.setTheoreticalSim(true);

        // load signals
        vector<TradeSignal> vCombinedSignals;
        for(unsigned int signalFileIdx = 0; signalFileIdx < vSignalFileDirs.size(); signalFileIdx++)
        {
            int iPortfolioID = cExecutorSim.iaddPortfolio(&(vResultH5Files[signalFileIdx]), &cSimTransactionType, NULL);

            string sSignalFileName = vSignalFileDirs[signalFileIdx] + "/tradesignals" + sYear + ".h5";
            if(stat(sSignalFileName.c_str(), &buffer) == 0 && buffer.st_size > 45)
            {
                loadNewSignalFile(sSignalFileName, vCombinedSignals, iPortfolioID, sDate);
            }
            else
            {
                std::cerr << "Error: SignalSimulator failed to find trade signal file " << sSignalFileName << "\n";
            }
        }

        sort(vCombinedSignals.begin(), vCombinedSignals.end(), signalCompareObj);

        for(vector<TradeSignal>::iterator signalItr = vCombinedSignals.begin();
            signalItr != vCombinedSignals.end();
            signalItr++)
        {
            // zero means no more signal
            long nextSignalTimeStamp = 0;

            if(signalItr + 1 != vCombinedSignals.end())
            {
                nextSignalTimeStamp = (*(signalItr+1)).iEpochTimeStamp;
            }

            cExecutorSim.newSignal(*signalItr, nextSignalTimeStamp);
        }

        cExecutorSim.writeTransactionsToFile(sDate);
    }
    else
    {
        std::cerr << "Cannot find date file " << sDataFile << ". No simulation running \n";
    }
}
