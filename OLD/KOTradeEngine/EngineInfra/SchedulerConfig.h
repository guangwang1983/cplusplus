#ifndef SCHEDULER_CONFIG_H
#define SCHEDULER_CONFIG_H

#include <string>
#include <vector>
#include <boost/program_options.hpp>

using std::string;
using std::vector;

namespace KO 
{

class SchedulerConfig 
{
public:
    SchedulerConfig();

	void loadCfgFile(string sConfigFileName);

    string              sFigureFile;
    string              sFigureActionFile;
    string              sRiskFile;
    string              sScheduledManualActionsFile;
    string              sFXRateFile;
    string              sTickSizeFile;
    string              sProductSpecFile;
    string              sScheduledSlotLiquidationFile;
    string              sLiquidationCommandFile;

    vector<string>      vTraderConfigs;

    bool                bRandomiseConfigs;

    bool                bLogMarketData;
    int                 iUseLiveOrder;
    string              sEngineName;
    string              sDate;
    string              sShutDownTime;

    string              sPositionServerAddress;

    string              sErrorWarningLogLevel;

    vector <string>     vProducts;
    vector <long>       vProductPriceLatencies;
    vector <long>       vProductOrderLatencies;

    void setupOptions(int argc, char *argv[]);

private:
	boost::program_options::options_description _cAllOptions;
};

};

#endif
