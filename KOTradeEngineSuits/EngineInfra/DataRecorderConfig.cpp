#include <string>
#include <sstream>
#include "DataRecorderConfig.h"
#include <limits>
#include <iostream>
#include <fstream>

using namespace std;

namespace KO
{

DataRecorderConfig::DataRecorderConfig() 
{
    sDate = "";
    sFXRateFile = "";
    sTickSizeFile = "";
    sProductSpecFile = "";
    sOutputPath = "";

    _cAllOptions.add_options()("Date", boost::program_options::value<string>(&sDate), "Simulation Date");
    _cAllOptions.add_options()("Products", boost::program_options::value<vector<string> >(&vKOProducts), "Products");
    _cAllOptions.add_options()("FXRateFile", boost::program_options::value<string>(&sFXRateFile), "FX Rate File");
    _cAllOptions.add_options()("TickSizeFile", boost::program_options::value<string>(&sTickSizeFile), "Tick Size File");
    _cAllOptions.add_options()("ProductSpecFile", boost::program_options::value<string>(&sProductSpecFile), "Product Spec File");
    _cAllOptions.add_options()("ShutDownTime", boost::program_options::value<string>(&sShutDownTime), "Shut Down Time");

    _cAllOptions.add_options()("LocalWQExchanges", boost::program_options::value<vector<string> >(&vLocalWQExchanges), "Local WQ Exchange");
    _cAllOptions.add_options()("GlobalWQExchanges", boost::program_options::value<vector<string> >(&vGlobalWQExchanges), "Global WQ Exchange");
}

void DataRecorderConfig::loadCfgFile(string sConfigFileName)
{
    boost::program_options::variables_map vm;
	fstream cConfigFileStream;
    cConfigFileStream.open(sConfigFileName.c_str(), std::fstream::in);
    boost::program_options::store(boost::program_options::parse_config_file(cConfigFileStream, _cAllOptions, true), vm);
    boost::program_options::notify(vm);
}

void DataRecorderConfig::setupOptions(int argc, char *argv[])
{
    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, _cAllOptions, true), vm);
    boost::program_options::notify(vm);
}

};
