#include <string>
#include <sstream>
#include <iostream>
#include <ctime>
#include <stdlib.h>
#include <vector>
#include <string.h>
#include <H5Cpp.h>
#include "EngineInfra/SimpleLogger.h"
#include "EngineInfra/SchedulerConfig.h"
#include "EngineInfra/KOScheduler.h"
#include "EngineInfra/KOModelFactory.h"
#include <framework/ArgumentParser.h>
#include <base-lib/VelioSessionManager.h>
#include <time.h>

using namespace std;
using namespace KO;

int main(int argc, char *argv[]) 
{
    H5::Exception::dontPrint();

    srand(time(0));
    string sSimType = argv [1];
    string sConfigFilePath;

    if(sSimType != "BaseSignal" && sSimType != "Config")
    {
        ArgumentParser::parseArguments(argc, argv);
        VelioSessionManager sessionManager;
        sessionManager.initialize();
        ManifestParser *mf = sessionManager.getManifestParser();

        sConfigFilePath.assign(mf->getProperty("LevelUp.configFile"));

        bool bIsLiveTrading;
        string sIsSim = mf->getProperty("velio.model.backTestEnabled");
        if(sIsSim.compare("true") == 0)
        {
            bIsLiveTrading = false;
        }
        else
        {
            bIsLiveTrading = true;
        }

        if(bIsLiveTrading == true)
        {
            //adjust daily release fold date to today for production stats
            ///scratch/levelup/productionportfolio/DailyRelease20220623/aur/levelup_01_aur/KOConfig.cfg

            size_t pos = sConfigFilePath.find("DailyRelease");
            string sBefore = sConfigFilePath.substr(0,pos+12);
            string sAfter = sConfigFilePath.substr(pos+20);

            time_t now = time(0);
            struct tm  tstruct;
            char buf[80];
            tstruct = *localtime(&now);
            strftime(buf, sizeof(buf), "%Y%m%d", &tstruct);

            sConfigFilePath = sBefore + buf + sAfter;

            cerr << sConfigFilePath << "\n";
        }
    
        SchedulerConfig cSchedulerConfig;
        cSchedulerConfig.loadCfgFile(sConfigFilePath);

        string sIsConflationBook = mf->getProperty("velio.ibook.conflation");
        if(sIsConflationBook.compare("true") == 0)
        {
            cSchedulerConfig.bUseOnConflationDone = true;
        }
        else
        {
            cSchedulerConfig.bUseOnConflationDone = false;
        }

        KOModelFactory cKOModelFactory(cSchedulerConfig, bIsLiveTrading);
        sessionManager.registerModelFactory(&cKOModelFactory);

        cerr << cSchedulerConfig.sDate << "\n";

        sessionManager.run();
    }
    else
    {
        sConfigFilePath = argv[2];
        SchedulerConfig cSchedulerConfig;
        cSchedulerConfig.loadCfgFile(sConfigFilePath);

        KOScheduler cKOScheduler(sSimType, cSchedulerConfig);
        if(cKOScheduler.init())
        {
            cKOScheduler.run();         
        }
        else
        {
            cerr << "Failed to initialised KO Scheduler. No simulation running.\n";    
        }
    }

	return 0;
}
