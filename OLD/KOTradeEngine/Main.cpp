#include <string>
#include <sstream>
#include <iostream>
#include <ctime>
#include <stdlib.h>
#include <vector>
#include <string.h>
#include "EngineInfra/SimpleLogger.h"
#include "EngineInfra/SchedulerConfig.h"
#include "EngineInfra/HCScheduler.h"
#include "EngineInfra/KOScheduler.h"

#include <framework/ArgumentParser.h>
#include <base-lib/VelioSessionManager.h>

using namespace std;
using namespace KO;

string sToKOProductSymbol(const string& sHCProductSymbol, const string& sTodayDate)
{
    string sResult = "";

    if(sHCProductSymbol[sHCProductSymbol.length()-1] == '!')
    {
        stringstream cProductSymbolStream;
        cProductSymbolStream << sHCProductSymbol[0] << sHCProductSymbol[6] << sHCProductSymbol[10];
        sResult = cProductSymbolStream.str();
    }
    else if(sHCProductSymbol[3] == '/')
    {
        stringstream cProductSymbolStream;
        cProductSymbolStream << sHCProductSymbol.substr(0,3)  << sHCProductSymbol.substr(4,3);
        sResult = cProductSymbolStream.str();
    }
    else if(sHCProductSymbol.substr(0,3) == "BAX" || sHCProductSymbol.substr(0,3) == "CGB")
    {
        sResult = sHCProductSymbol.substr(0,4) + sHCProductSymbol.substr(5,1);
    }
    else
    {
        sResult = sHCProductSymbol;
    }

    return sResult;
}

int main(int argc, char *argv[]) 
{
    srand(time(0));

    if(strcmp(argv[1], "KOSim") == 0)
    {
        string sConfigFilePath = argv[2];

        SchedulerConfig cSchedulerConfig;
        cSchedulerConfig.loadCfgFile(sConfigFilePath);

        KOScheduler cKOScheduler(cSchedulerConfig);
        if(cKOScheduler.init())
        {
            cKOScheduler.run();         
        }
        else
        {
            cerr << "Failed to initialised KO Scheduler. No simulation running.\n";    
        }
    }
    else
    {
        ArgumentParser::parseArguments(argc, argv);

        VelioSessionManager zod;
        zod.initialize();
        ManifestParser *mf = zod.getManifestParser();

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

        string sConfigFilePath;
        sConfigFilePath.assign(mf->getProperty("LevelUp.configFile"));

        SchedulerConfig cSchedulerConfig;
        cSchedulerConfig.loadCfgFile(sConfigFilePath);

        HCScheduler* pModel = new HCScheduler(cSchedulerConfig, &zod, bIsLiveTrading);

        string sContract;
        string sKOMarket;
        string sHCMarket;
        string sFutureContractList = mf->getProperty("velio.model.futures");
        string sKOFutureMarketList = mf->getProperty("LevelUp.model.exchanges");
        string sHCFutureMarketList = mf->getProperty("HC.model.exchanges");

        std::istringstream cKOMarketStream (sKOFutureMarketList);
        std::istringstream cHCMarketStream (sHCFutureMarketList);
        std::istringstream cContractStream(sFutureContractList);

        while(std::getline(cKOMarketStream, sKOMarket, ',') &&
              std::getline(cContractStream, sContract, ',') &&
              std::getline(cHCMarketStream, sHCMarket, ','))
        {
            // Create and register a new model for the found contract.
            zod.addModel(sHCMarket, sContract, pModel);
            pModel->addInstrument(sKOMarket + "." + sToKOProductSymbol(sContract, cSchedulerConfig.sDate), sContract, KO::KO_FUTURE);
        }

        string sFXPair;
        string sKOFXVenue;
        string sHCFXVenue;
        string sFXPairList = mf->getProperty("velio.model.Ccys");
        string sKOFXVenueList = mf->getProperty("LevelUp.model.fx_venues");
        string sHCFXVenueList = mf->getProperty("HC.model.fx_venues");

        std::istringstream cKOFXVenueStream (sKOFXVenueList);
        std::istringstream cHCFXVenueStream (sHCFXVenueList);
        std::istringstream cFXPairStream(sFXPairList);

        while(std::getline(cKOFXVenueStream, sKOFXVenue, ',') &&
              std::getline(cFXPairStream, sFXPair, ',') &&
              std::getline(cHCFXVenueStream, sHCFXVenue, ','))
        {
            // Create and register a new model for the found contract.
            zod.addModel(sHCFXVenue, sFXPair, pModel);
            pModel->addInstrument(sKOFXVenue + "." + sToKOProductSymbol(sFXPair, cSchedulerConfig.sDate), sFXPair, KO::KO_FX);
        }

        pModel->KOInit();

        zod.run();
    }

	return 0;
}
