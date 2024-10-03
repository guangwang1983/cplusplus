#include <string>
#include <sstream>
#include "SchedulerConfig.h"
#include <limits>
#include <iostream>
#include <fstream>

using namespace std;

namespace KO
{

SchedulerConfig::SchedulerConfig() 
{
    sHistoricDataPath = "";
    sFigureFile = "";
    sFigureActionFile = "";
    sScheduledManualActionsFile = "";
    sFXRateFile = "";
    sProductSpecFile = "";
    sScheduledSlotLiquidationFile = "";
    bRandomiseConfigs = true;
    bLogMarketData = false;
    sErrorWarningLogLevel = "WARNING";
    sEngineName = "";
    sDate = "000000";
    sShutDownTime = "235959";
    sExecutorSimCfgFile = "";
    bUse100ms = false;

    _cAllOptions.add_options()("Date", boost::program_options::value<string>(&sDate), "Simulation Date");
    _cAllOptions.add_options()("Products", boost::program_options::value<vector<string> >(&vProducts), "Products");
    _cAllOptions.add_options()("HCProducts", boost::program_options::value<vector<string> >(&vHCProducts), "HCProducts");
    _cAllOptions.add_options()("TBProducts", boost::program_options::value<vector<string> >(&vTBProducts), "TBProducts");
    _cAllOptions.add_options()("IsLocalProducts", boost::program_options::value<vector<bool> >(&vIsLocalProducts), "IsLocalProducts");
    _cAllOptions.add_options()("ProductMaxRisk", boost::program_options::value<vector<long> >(&vProductMaxRisk), "ProductMaxRisk");
    _cAllOptions.add_options()("ProductExpoLimit", boost::program_options::value<vector<long> >(&vProductExpoLimit), "ProductExpoLimit");
    _cAllOptions.add_options()("ProductStopLoss", boost::program_options::value<vector<long> >(&vProductStopLoss), "ProductStopLoss");

    _cAllOptions.add_options()("FXSubProducts", boost::program_options::value<vector<string> >(&vFXSubProducts), "FXSubProducts");

    _cAllOptions.add_options()("TradingLocation", boost::program_options::value<string>(&sTradingLocation), "Trading Location");
    _cAllOptions.add_options()("Use100ms", boost::program_options::value<bool>(&bUse100ms), "Use 100ms data");

    _cAllOptions.add_options()("TraderConfigs", boost::program_options::value<vector<string> >(&vTraderConfigs), "Trader Configs");
    _cAllOptions.add_options()("FXRateFile", boost::program_options::value<string>(&sFXRateFile), "FX Rate File");
    _cAllOptions.add_options()("TickSizeFile", boost::program_options::value<string>(&sTickSizeFile), "Tick Size File");
    _cAllOptions.add_options()("ProductSpecFile", boost::program_options::value<string>(&sProductSpecFile), "Product Spec File");
    _cAllOptions.add_options()("FigureFile", boost::program_options::value<string>(&sFigureFile), "Figure File");
    _cAllOptions.add_options()("HistoricDataPath", boost::program_options::value<string>(&sHistoricDataPath), "Historic Data Path");
    _cAllOptions.add_options()("OutputBasePath", boost::program_options::value<string>(&sOutputBasePath), "Output Base Path");
    _cAllOptions.add_options()("LogPath", boost::program_options::value<string>(&sLogPath), "Log path in config mode");
    _cAllOptions.add_options()("FigureActionFile", boost::program_options::value<string>(&sFigureActionFile), "Figure Action File");
    _cAllOptions.add_options()("ScheduledManualActionsFile", boost::program_options::value<string>(&sScheduledManualActionsFile), "Scheduled Manual Actions File");
    _cAllOptions.add_options()("ScheduledSlotLiquidationFile", boost::program_options::value<string>(&sScheduledSlotLiquidationFile), "Scheduled Slot Liquidation File");
    _cAllOptions.add_options()("LiquidationCommandFile", boost::program_options::value<string>(&sLiquidationCommandFile), "Liquidation Command File");
    _cAllOptions.add_options()("ForbiddenFXLPsFile", boost::program_options::value<string>(&sForbiddenFXLPsFile), "Forbidden FX LPs File");

    _cAllOptions.add_options()("ShutDownTime", boost::program_options::value<string>(&sShutDownTime), "Engine Shut Down Time");
    _cAllOptions.add_options()("LogMarketData", boost::program_options::value<bool>(&bLogMarketData), "Log Market Data");
    _cAllOptions.add_options()("RandomiseConfigs", boost::program_options::value<bool>(&bRandomiseConfigs), "Randomise Trading Configs");
    _cAllOptions.add_options()("ErrorWarningLogLevel", boost::program_options::value<string>(&sErrorWarningLogLevel), "Error Warning Log Level");
    _cAllOptions.add_options()("ExecutorSimCfgFile", boost::program_options::value<string>(&sExecutorSimCfgFile), "Executor Simulator Config File");

    _cAllOptions.add_options()("FixConfigFileName", boost::program_options::value<string>(&sFixConfigFileName), "Fix Config File");
}

void SchedulerConfig::loadCfgFile(string sConfigFileName)
{
    boost::program_options::variables_map vm;
	fstream cConfigFileStream;
    cConfigFileStream.open(sConfigFileName.c_str(), std::fstream::in);
    boost::program_options::store(boost::program_options::parse_config_file(cConfigFileStream, _cAllOptions, true), vm);
    boost::program_options::notify(vm);
}

void SchedulerConfig::setupOptions(int argc, char *argv[])
{
    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, _cAllOptions, true), vm);
    boost::program_options::notify(vm);
}

};
