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

    string              sFixConfigFileName;
   
    string              sLogPath; 
    string              sOutputBasePath;

    string              sHistoricDataPath;

    string              sFigureFile;
    string              sFigureActionFile;
    string              sFigureActionExecutionFile;
    string              sScheduledManualActionsFile;
    string              sFXRateFile;
    string              sTickSizeFile;
    string              sProductSpecFile;
    string              sScheduledSlotLiquidationFile;
    string              sLiquidationCommandFile;
    string              sForbiddenFXLPsFile;
    string              sFXArtificialSpreadFile;

    vector<string>      vTraderConfigs;

    bool                bRandomiseConfigs;

    bool                bLogMarketData;
    string              sEngineName;
    string              sDate;
    string              sShutDownTime;

    string              sErrorWarningLogLevel;

    vector <string>     vProducts;
    vector <string>     vHCProducts;
    vector <string>     vTBProducts;
    vector <bool>       vIsLocalProducts;
    vector <long>       vProductMaxRisk;
    vector <long>       vProductExpoLimit;
    vector <long>       vProductStopLoss;

    vector<string>      vFXSubProducts;

    string              sTradingLocation;
    bool                bUse100ms;

    string              sExecutorSimCfgFile;

    bool                bUseOnConflationDone;

    void setupOptions(int argc, char *argv[]);

private:
	boost::program_options::options_description _cAllOptions;
};

};

#endif
