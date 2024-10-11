#include <fstream>
#include <string>
#include <boost/program_options.hpp>
#include <glob.h>
#include <H5Cpp.h>
#include <cstdio>
#include <sys/stat.h>
#include <cstdlib>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time_adjustor.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/local_timezone_defs.hpp>

using namespace H5;
using namespace std;

std::string random_string( size_t length )
{
    auto randchar = []() -> char
    {
        const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[ rand() % max_index ];
    };
    std::string str(length,0);
    std::generate_n( str.begin(), length, randchar );
    return str;
}

struct SimTransaction
{
    long iEpochTimeStamp;
    char sProduct [12];
    long iQty;
    double dPrice;
    char sDate [12];
};

boost::posix_time::ptime cCETToUTC(boost::posix_time::ptime cCETTime)
{
    typedef boost::date_time::eu_dst_trait<boost::gregorian::date> eu_dst_traits;
    typedef boost::date_time::dst_calc_engine<boost::gregorian::date, boost::posix_time::time_duration, eu_dst_traits> eu_dst_calc;
    typedef boost::date_time::local_adjustor<boost::posix_time::ptime, 1, eu_dst_calc> CET;

    boost::posix_time::ptime cUTCTime = CET::local_to_utc(cCETTime);

    return cUTCTime;
}

void loadAllTransactions(const string& sAbsConfigPath, vector<string>& vDates, vector<SimTransaction>& vTotalTransactions, const CompType& cSimTransactionType)
{
    string sLoadedYear = "";
    H5File *pConfigTxH5;

    bool bSizeReserved = false;
    for(vector<string>::iterator itr = vDates.begin();
        itr != vDates.end();
        itr++)
    {
        string sCurrentYear = (*itr).substr(0,4);

        if(sCurrentYear != sLoadedYear)
        {
            if(sLoadedYear != "")
            {
                pConfigTxH5->close();
                delete pConfigTxH5;
            }

            string sConfigTxH5FileName = sAbsConfigPath + "/transactions" + sCurrentYear + ".h5";

            try
            {
                std::cout << "Loading config tx file " << sConfigTxH5FileName << "\n";
                pConfigTxH5 = new H5File (sConfigTxH5FileName, H5F_ACC_RDONLY);
                sLoadedYear = sCurrentYear;
            }
            catch(FileIException not_found_error)
            {
                std::cerr << "Error: Cannot find config tx file " << sConfigTxH5FileName << "\n";
                sLoadedYear = "";
            }
        }

        if(sLoadedYear != "")
        {
            boost::gregorian::date _cTodayDate = boost::gregorian::from_undelimited_string(*itr);
            boost::posix_time::ptime cCETTime (_cTodayDate, boost::posix_time::time_duration(12,0,0,0));
            boost::posix_time::ptime cUTCTime = cCETToUTC(cCETTime);

            long iCETDiffUTC = (long)(cCETTime - cUTCTime).total_seconds() * 1000000000;

            bool bDataSetOpened = false;
            DataSet cDataSet;
            try
            {
                cDataSet = pConfigTxH5->openDataSet("/" + *itr);
                bDataSetOpened = true;
            }   
            catch(FileIException not_found_error)
            {
                std::cerr << "Warning: No transaction table found for " << sAbsConfigPath << " " << *itr << "\n";
            }

            if(bDataSetOpened == true)
            {
                hsize_t cDim[1];
                DataSpace cDataSpace = cDataSet.getSpace();
                cDataSpace.getSimpleExtentDims(cDim);

                if(cDim[0] > 0)
                {
                    if(bSizeReserved == false)
                    {
                        vTotalTransactions.reserve(cDim[0] * 3);
                        bSizeReserved = true;
                    }

                    SimTransaction* pLoadedData = new SimTransaction [cDim[0]];
                    cDataSet.read(pLoadedData, cSimTransactionType);

                    for(int i = 0; i < cDim[0]; i++)
                    {
                        long iNewEpochTimeStamp = pLoadedData[i].iEpochTimeStamp + iCETDiffUTC;
                        long iNewEpochTimeStampSec = iNewEpochTimeStamp / 1000000000;

                        if(boost::posix_time::from_time_t(iNewEpochTimeStampSec).date() == _cTodayDate)
                        {
                            pLoadedData[i].iEpochTimeStamp = iNewEpochTimeStamp;
                        }
                    }

                    vector<SimTransaction> vNewTransactions(pLoadedData, pLoadedData + cDim[0]);

                    vTotalTransactions.insert(vTotalTransactions.end(), vNewTransactions.begin(), vNewTransactions.end());

                    delete pLoadedData;
                }
            }
        }
    }

    if(sLoadedYear != "")
    {
        pConfigTxH5->close();
        delete pConfigTxH5;   
    }
}

int main(int argc, char *argv[])
{
    H5::Exception::dontPrint();

    string sDatesFile;
    string sBaseSignalPath;
    string sOutputFileName;
    bool bDirectionalTX;
    bool bUseMemDisk;

    boost::program_options::options_description _cAllOptions;
    _cAllOptions.add_options()("DatesFile", boost::program_options::value<string>(&sDatesFile), "Contains list of date which contains tx to be merged");
    _cAllOptions.add_options()("BaseSignalPath", boost::program_options::value<string>(&sBaseSignalPath), "Abs path where the tx base signal is to be updated");
    _cAllOptions.add_options()("OutputFileName", boost::program_options::value<string>(&sOutputFileName), "Output file name");
    _cAllOptions.add_options()("DirectionalTX", boost::program_options::value<bool>(&bDirectionalTX), "reading Directional TX");
    _cAllOptions.add_options()("UseMemDisk", boost::program_options::value<bool>(&bUseMemDisk)->default_value(true), "reading Directional TX");

    if(argc == 1)
    {
        cout << _cAllOptions;
        return 0;
    }

    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, _cAllOptions), vm);
    boost::program_options::notify(vm);

    if(sOutputFileName == "")
    {
        sOutputFileName = "tx.h5";
    }

    string sProductName;
    string sModelName;
    string sModelPath;

    size_t iEndPos = sBaseSignalPath.rfind("/configs") - 1;
    size_t iStartPos = sBaseSignalPath.rfind("/", iEndPos);
    sModelName = sBaseSignalPath.substr(iStartPos+1, iEndPos-iStartPos);
    sModelPath = sBaseSignalPath.substr(0, iEndPos + 1);

    iStartPos = sModelName.find("_",3);
    iEndPos = sModelName.find(".",4);
    long iProductNameLength = iEndPos - iStartPos - 1;
    sProductName = sModelName.substr(iStartPos+1, iProductNameLength);

    std::size_t iPos = sBaseSignalPath.rfind("/B");
    string sBaseSignalName = sBaseSignalPath.substr(iPos+1, 5);

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

    // load dates file
    vector<string> vDates;

    fstream ifsDatesFile;
    ifsDatesFile.open(sDatesFile, fstream::in);

    if(ifsDatesFile.is_open())
    {
        while(!ifsDatesFile.eof())
        {
            char sNewLine[1024];
            ifsDatesFile.getline(sNewLine, sizeof(sNewLine));

            if(strlen(sNewLine) != 0 && sNewLine[0] != '#')
            {
                vDates.push_back(sNewLine);
            }
        }
    }
    else
    {
        cerr << "Cannot find dates file " << sDatesFile << ". Exit\n";
        return 1;
    }

    H5File* pInputTXH5File = NULL;
    try
    {
        pInputTXH5File = new H5File(sModelPath + "/basesignals/" + sBaseSignalName + "/" + sOutputFileName, H5F_ACC_RDWR);
    }
    catch(FileIException not_found_error)
    {
        pInputTXH5File = NULL;    
    }

    H5File* pOutputTXH5File = NULL;
    string sTempTXFileName = "new file";
    try
    {
        string sTempDir;

        if(bUseMemDisk == true)
        {
            sTempDir = "/mnt/ram-disk/" + random_string(10);
            string sCommand = "mkdir -p " + sTempDir;
            system(sCommand.c_str());
        }
        else
        {
            sTempDir = sModelPath + "/basesignals/" + sBaseSignalName;
        }

        sTempTXFileName = sTempDir + "/" + sOutputFileName + ".new";
        pOutputTXH5File = new H5File(sTempTXFileName, H5F_ACC_EXCL);
    }
    catch(FileIException not_found_error)
    {
        cerr << "Cannot create temp file " + sTempTXFileName + ". Exit. \n";
        return 1;
    }

    // loop through all configs

    fstream ifsSpaceFile;
    ifsSpaceFile.open(sModelPath + "/basesignals/" + sBaseSignalName + "/tradesignalspace.cfg", fstream::in);

    if(ifsSpaceFile.is_open())
    {
        while(!ifsSpaceFile.eof())
        {
            char sNewLine[1024];
            ifsSpaceFile.getline(sNewLine, sizeof(sNewLine));

            if(strlen(sNewLine) != 0 && sNewLine[0] != '#')
            {
                std::size_t iPos = string(sNewLine).find(";");
                string sConfigName = string(sNewLine).substr(0, iPos);

                vector<SimTransaction> vTotalTransactions;

                string sDataSetName = sBaseSignalName + "." + sConfigName;
                SimTransaction* pTransactionsArray;
                long iNumExistingTransactions;

                long iMinDateUTC = 0;

                string sConfigDir = "configs";
                if(bDirectionalTX == true)
                {
                    sConfigDir = "directionalconfigs";
                }

                loadAllTransactions(sModelPath + "/" + sConfigDir + "/" + sBaseSignalName + "." + sConfigName + "/", vDates, vTotalTransactions, cSimTransactionType); 

cerr << sModelPath + "/" + sConfigDir + "/" + sBaseSignalName + "." + sConfigName << " loaded \n";
                if(pInputTXH5File != NULL)
                {
                    try
                    {
                        Group cProductGroup = pInputTXH5File->openGroup(sProductName);
                        DataSet cDataSet = cProductGroup.openDataSet(sDataSetName);

                        hsize_t cDim[1];
                        DataSpace cDataSpace = cDataSet.getSpace();
                        cDataSpace.getSimpleExtentDims(cDim);

                        iNumExistingTransactions = cDim[0];
                        if(iNumExistingTransactions != 0)
                        {
                            pTransactionsArray = new SimTransaction[vTotalTransactions.size() + iNumExistingTransactions];
                            cDataSet.read(pTransactionsArray, cSimTransactionType);
                            string sMinDate = pTransactionsArray[iNumExistingTransactions-1].sDate;
                            sMinDate = sMinDate.substr(0,4) + sMinDate.substr(5,2) + sMinDate.substr(8,2);
                            boost::posix_time::ptime cResultPtime(boost::gregorian::from_undelimited_string(sMinDate) + boost::gregorian::date_duration(1));
                            boost::posix_time::ptime cEpoch(boost::gregorian::date(1970,1,1));
                            iMinDateUTC = (cResultPtime - cEpoch).total_seconds() * (long)1000000000;
                        }
                        else
                        {
                            pTransactionsArray = new SimTransaction[vTotalTransactions.size()];
                        }
                    }
                    catch(FileIException not_found_error)
                    {
                        iNumExistingTransactions = 0;
                        pTransactionsArray = new SimTransaction[vTotalTransactions.size()];
                    }
                }
                else
                {
                    iNumExistingTransactions = 0;
                    pTransactionsArray = new SimTransaction[vTotalTransactions.size()];
                }

                long iNumActualNewTransactions = vTotalTransactions.size();
                vector<SimTransaction>::iterator itrCopyBegin = vTotalTransactions.begin();
                while(itrCopyBegin != vTotalTransactions.end())
                {
                    if((*itrCopyBegin).iEpochTimeStamp >= iMinDateUTC)
                    {
                        break;
                    }
                    else
                    {
                        iNumActualNewTransactions = iNumActualNewTransactions - 1;
                        itrCopyBegin++;
                    }
                }

                if(iNumActualNewTransactions > 0)
                {
                    std::copy(itrCopyBegin, vTotalTransactions.end(), pTransactionsArray + iNumExistingTransactions);
                }

                Group* pProductGroup = NULL;
                Group cProductGroup;
                try
                {
                    cProductGroup = pOutputTXH5File->openGroup(sProductName);
                }
                catch(FileIException not_found_error)
                {
                    pProductGroup = new Group(pOutputTXH5File->createGroup("/" + sProductName));
                    cProductGroup = *pProductGroup;
                }

                hsize_t cDim[] = {iNumActualNewTransactions + iNumExistingTransactions};
                DataSpace cSpace(sizeof(cDim)/sizeof(hsize_t), cDim);
                DataSet* pDataSet = new DataSet(cProductGroup.createDataSet(sDataSetName, cSimTransactionType, cSpace));

                pDataSet->write(pTransactionsArray, cSimTransactionType);
                delete pTransactionsArray;

                delete pDataSet;
                if(pProductGroup != NULL)
                {
                    delete pProductGroup;
                }
            }
        }
    }

    if(pInputTXH5File != NULL)
    {
        pInputTXH5File->close();
        delete pInputTXH5File;
    }

    if(pOutputTXH5File != NULL)
    {
        pOutputTXH5File->close();
        delete pOutputTXH5File;
    }

    system(("mv " + sTempTXFileName + " " + sModelPath + "/basesignals/" + sBaseSignalName + "/" + sOutputFileName).c_str());
}
