#include "quickfix/FileStore.h"
#include "quickfix/FileLog.h"
#include "quickfix/SocketInitiator.h"
#include "quickfix/Session.h"
#include "quickfix/SessionSettings.h"
#include "quickfix/Application.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <string>
#include <iostream>
#include "EngineInfra/SchedulerConfig.h"
#include "EngineInfra/QuickFixScheduler.h"

using namespace std;
using namespace KO;

bool isComment( const std::string& line )
{
  if( line.size() == 0 )
    return false;

  return line[0] == '#';
}

bool isSection( const std::string& line )
{
  if( line.size() == 0 )
    return false;

  return line[0] == '[' && line[line.size()-1] == ']';
}

bool isKeyValue( const std::string& line )
{
  return line.find( '=' ) != std::string::npos;
}

int main( int argc, char** argv )
{
    // TODO: add simulation code

    std::string sEngineMode = argv[1];
    std::string sConfigFilePath = argv[2];
    std::string sFixConfigFileName;        

    SchedulerConfig cSchedulerConfig;
    cSchedulerConfig.loadCfgFile(sConfigFilePath);

    if(sEngineMode == "LIVE")
    {
        sFixConfigFileName = cSchedulerConfig.sFixConfigFileName;

        QuickFixScheduler cQuickFixScheduler(cSchedulerConfig, true);

        std::cerr << sFixConfigFileName << "\n";

        FIX::SessionSettings cFixSettings(sFixConfigFileName);
        std::cerr << "1\n";
        FIX::FileStoreFactory cFixStoreFactory(cFixSettings);
        std::cerr << "2\n";
        FIX::FileLogFactory cFixLogFactory(cFixSettings);
        std::cerr << "3\n";

        std::ifstream fstream( sFixConfigFileName.c_str() );
        char buffer[1024];
        std::string line;
        while( fstream.getline(buffer, sizeof(buffer)) )
        {
            line = string_strip( buffer );
            if( isComment(line) )
            {
              continue;
            }
            else if( isSection(line) )
            {
                std::cerr << "is section \n";
            }
            else if( isKeyValue(line) )
            {
                std::cerr << "is key value \n";
            }
        }

        std::cerr << cFixSettings << "\n";

        FIX::SocketInitiator cFixSocketInitiator(cQuickFixScheduler, cFixStoreFactory, cFixSettings, cFixLogFactory /*optional*/);

        std::cerr << "4\n";
        cFixSocketInitiator.start();
        std::cerr << "5\n";
        
        boost::posix_time::ptime cPrevTime = boost::posix_time::microsec_clock::local_time();
        while(true)
        {
            boost::posix_time::ptime cNowTime = boost::posix_time::microsec_clock::local_time();

            int iPrevSec = cPrevTime.time_of_day().seconds();
            int iNowSec = cNowTime.time_of_day().seconds();

            cPrevTime = cNowTime;

            if(iPrevSec != iNowSec)
            {
                cQuickFixScheduler.onTimer();
            }

            if(cQuickFixScheduler.bgetMarketDataSessionLoggedOn() == true)
            {
                if(cQuickFixScheduler.bgetMarketDataSubscribed() == false)
                {
                    // need to test reconnect and market data resubscription?
                    cQuickFixScheduler.init();
                }
            }
        
            if(cQuickFixScheduler.bschedulerFinished())
            {
                break;
            } 
        }

        cFixSocketInitiator.stop();
    }
    
    return 0;
}
