#ifndef DATARECORDER_CONFIG_H
#define DATARECORDER_CONFIG_H

#include <string>
#include <vector>
#include <boost/program_options.hpp>

using std::string;
using std::vector;

namespace KO 
{

class DataRecorderConfig 
{
public:
    DataRecorderConfig();

	void loadCfgFile(string sConfigFileName);
    
    string              sDate;

    string              sFXRateFile;
    string              sTickSizeFile;
    string              sProductSpecFile;
    string              sOutputPath;
    string              sShutDownTime;

    vector<string>      vKOProducts;    
    vector<string>      vLocalWQExchanges;
    vector<string>      vGlobalWQExchanges;

    void setupOptions(int argc, char *argv[]);

private:
	boost::program_options::options_description _cAllOptions;
};

};

#endif
