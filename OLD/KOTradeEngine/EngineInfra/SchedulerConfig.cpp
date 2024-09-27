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
    sFigureFile = "";
    sFigureActionFile = "";
    sRiskFile = "";
    sScheduledManualActionsFile = "";
    sFXRateFile = "";
    sProductSpecFile = "";
    sScheduledSlotLiquidationFile = "";
    bRandomiseConfigs = true;
    bLogMarketData = false;
    sErrorWarningLogLevel = "WARNING";
    iUseLiveOrder = 0;
    sEngineName = "";
    sDate = "000000";
    sShutDownTime = "235959";

    _cAllOptions.add_options()("Date", boost::program_options::value<string>(&sDate), "Simulation Date");
    _cAllOptions.add_options()("Products", boost::program_options::value<vector<string> >(&vProducts), "Products");
    _cAllOptions.add_options()("ProductPriceLatencies", boost::program_options::value<vector<long> >(&vProductPriceLatencies), "Product Price Latencies");
    _cAllOptions.add_options()("ProductOrderLatencies", boost::program_options::value<vector<long> >(&vProductOrderLatencies), "Product Order Latencies");
    _cAllOptions.add_options()("TraderConfigs", boost::program_options::value<vector<string> >(&vTraderConfigs), "Trader Configs");
    _cAllOptions.add_options()("PositionServerAddress", boost::program_options::value<string>(&sPositionServerAddress), "Position Server Address");
    _cAllOptions.add_options()("FXRateFile", boost::program_options::value<string>(&sFXRateFile), "FX Rate File");
    _cAllOptions.add_options()("TickSizeFile", boost::program_options::value<string>(&sTickSizeFile), "Tick Size File");
    _cAllOptions.add_options()("ProductSpecFile", boost::program_options::value<string>(&sProductSpecFile), "Product Spec File");
    _cAllOptions.add_options()("FigureFile", boost::program_options::value<string>(&sFigureFile), "Figure File");
    _cAllOptions.add_options()("FigureActionFile", boost::program_options::value<string>(&sFigureActionFile), "Figure Action File");
    _cAllOptions.add_options()("RiskFile", boost::program_options::value<string>(&sRiskFile), "Risk File");
    _cAllOptions.add_options()("ScheduledManualActionsFile", boost::program_options::value<string>(&sScheduledManualActionsFile), "Scheduled Manual Actions File");
    _cAllOptions.add_options()("ScheduledSlotLiquidationFile", boost::program_options::value<string>(&sScheduledSlotLiquidationFile), "Scheduled Slot Liquidation File");
    _cAllOptions.add_options()("LiquidationCommandFile", boost::program_options::value<string>(&sLiquidationCommandFile), "Liquidation Command File");
    _cAllOptions.add_options()("ShutDownTime", boost::program_options::value<string>(&sShutDownTime), "Engine Shut Down Time");
    _cAllOptions.add_options()("LogMarketData", boost::program_options::value<bool>(&bLogMarketData), "Log Market Data");
    _cAllOptions.add_options()("RandomiseConfigs", boost::program_options::value<bool>(&bRandomiseConfigs), "Randomise Trading Configs");
    _cAllOptions.add_options()("ErrorWarningLogLevel", boost::program_options::value<string>(&sErrorWarningLogLevel), "Error Warning Log Level");
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
