#include "StaticDataHandler.h"
#include "ErrorHandler.h"
#include "SystemClock.h"
#include <fstream>
#include <utility>
#include <sstream>
#include <string.h>

using namespace std;

namespace KO
{

StaticDataHandler::StaticDataHandler(string sFXRateFileName, string sProductSpecFileName, string sTickSizeFileName, string sFXArtificialSpreadFile, string sTodayDate)
:_sTodayDate(sTodayDate)
{
    // add default USDUSD FX pair
    _mFXRate.insert(std::pair<string, double>("USDUSD", 1));

    // load fx file
    ifstream ifsFXFile (sFXRateFileName.c_str());

    if(ifsFXFile.is_open())
    {
        while(!ifsFXFile.eof())
        {
            char sNewLine[2048];
            ifsFXFile.getline(sNewLine, sizeof(sNewLine));

            if(sNewLine[0] != '#' && strlen(sNewLine) != 0)
            {
                std::istringstream cFXLineStream(sNewLine);
                string sElement;

                string sFXPairName;
                string sFXRate;
                double dFXRate;

                int index = 0;
                while (std::getline(cFXLineStream, sElement, ','))
                {
                    if(index == 0)
                    {
                        sFXPairName = sElement;
                    }
                    else if(index == 1)
                    {
                        sFXRate = sElement;
                        dFXRate = stod(sFXRate);
                    }

                    index = index + 1;
                }

                _mFXRate.insert(std::pair<string, double>(sFXPairName, dFXRate));
            }
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Cannot load fx rate file " << sFXRateFileName << ".";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    // load tick size
    ifstream ifsTickSizeFile (sTickSizeFileName.c_str());

    if(ifsTickSizeFile.is_open())
    {
        while(!ifsTickSizeFile.eof())
        {
            char sNewLine[2048];
            ifsTickSizeFile.getline(sNewLine, sizeof(sNewLine));

            if(sNewLine[0] != '#' && strlen(sNewLine) != 0)
            {
                std::istringstream cTickSizeLineStream(sNewLine);
                string sElement;

                string sRootSymbol;
                string sTickSize;
                double dTickSize;

                int index = 0;
                while (std::getline(cTickSizeLineStream, sElement, ','))
                {
                    if(index == 0)
                    {
                        sRootSymbol = sElement;
                    }
                    else if(index == 1)
                    {
                        sTickSize = sElement;
                        dTickSize = stod(sTickSize);
                    }

                    index = index + 1;
                }

                _mTickSize.insert(std::pair<string, double>(sRootSymbol, dTickSize));
            }
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Cannot load tick size file " << sTickSizeFileName << ".";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    // load static data file
    ifstream ifsProductSpecFile (sProductSpecFileName.c_str());

    if(ifsProductSpecFile.is_open())
    {
        while(!ifsProductSpecFile.eof())
        {
            char sNewLine[2048];
            ifsProductSpecFile.getline(sNewLine, sizeof(sNewLine));

            if(sNewLine[0] != '#' && strlen(sNewLine) != 0)
            {
                std::istringstream cProductSpecStream(sNewLine);
                string sElement;

                string sRootSymbol;
                string sTradingFee;
                string sCurrency;
                string sContractSize;
                string sMarketOpenTime;
                string sMarketCloseTime;
                string sExchange;
                string sHCExchange;
                string sDescription;
                string sProductType;
                string sRIC;

                long iNY4Latency;
                long iTelcityLatency;

                double dTradingFee;
                double dContractSize;
                KOEpochTime cMarketOpenTime;
                KOEpochTime cMarketCloseTime;

                int index = 0;
                while (std::getline(cProductSpecStream, sElement, ','))
                {
                    if(index == 0)
                    {
                        sRootSymbol = sElement;
                        if(sRootSymbol == "ROOT")
                        {
                            break;
                        }
                    }
                    else if(index == 1)
                    {
                        sContractSize = sElement;
                        dContractSize = stod(sContractSize);
                    }
                    else if(index == 2)
                    {
                        sCurrency = sElement;
                    }
                    else if(index == 3)
                    {
                        sTradingFee = sElement;
                        dTradingFee = stod(sTradingFee);
                    }
                    else if(index == 7)
                    {
                        sMarketOpenTime = sElement;
                        cMarketOpenTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(_sTodayDate, sMarketOpenTime.substr(0,2), sMarketOpenTime.substr(2,2), sMarketOpenTime.substr(4,2));
                    }
                    else if(index == 8)
                    {
                        sMarketCloseTime = sElement;
                        cMarketCloseTime = SystemClock::GetInstance()->cCreateKOEpochTimeFromCET(_sTodayDate, sMarketCloseTime.substr(0,2), sMarketCloseTime.substr(2,2), sMarketCloseTime.substr(4,2));
                    }
                    else if(index == 9)
                    {
                        sExchange = sElement;
                    }
                    else if(index == 10)
                    {
                        sHCExchange = sElement;
                    }
                    else if(index == 11)
                    {
                        sDescription = sElement;
                    }
                    else if(index == 12)
                    {
                        sProductType = sElement;
                    }   
                    else if(index == 13)
                    {
                        sRIC = sElement;
                    }   
                    else if(index == 14)
                    {
                        iNY4Latency = atoi(sElement.c_str());
                    }   
                    else if(index == 15)
                    {
                        iTelcityLatency = atoi(sElement.c_str());
                    }   

                    index = index + 1;
                }

                string sMapKey = sExchange + "." + sRootSymbol;

                _mTradingFee.insert(std::pair<string, double>(sMapKey, dTradingFee));
                _mCurrency.insert(std::pair<string, string>(sMapKey, sCurrency));
                _mHCExchange.insert(std::pair<string, string>(sMapKey, sHCExchange));
                _mContractSize.insert(std::pair<string, double>(sMapKey, dContractSize));
                _mMarketOpenTime.insert(std::pair<string, KOEpochTime>(sMapKey, cMarketOpenTime));
                _mMarketCloseTime.insert(std::pair<string, KOEpochTime>(sMapKey, cMarketCloseTime));
                _mProductType.insert(std::pair<string, string>(sMapKey, sProductType));
                _mProductRIC.insert(std::pair<string, string>(sMapKey, sRIC));

                _mProductNY4Latency.insert(std::pair<string, long>(sMapKey, iNY4Latency));
                _mProductTelcityLatency.insert(std::pair<string, long>(sMapKey, iTelcityLatency));
            }
        }
    }
    else
    {
        stringstream cStringStream;
        cStringStream << "Cannot load product spec file " << sProductSpecFileName << "\n";
        ErrorHandler::GetInstance()->newErrorMsg("0", "ALL", "ALL", cStringStream.str());
    }

    // load fx spread file
    ifstream ifsFXArtificialSpreadFile (sFXArtificialSpreadFile.c_str());
    cerr << sFXArtificialSpreadFile << "\n";
    if(ifsFXArtificialSpreadFile.is_open())
    {
cerr << "file opened \n";
        while(!ifsFXArtificialSpreadFile.eof())
        {
            char sNewLine[2048];
            ifsFXArtificialSpreadFile.getline(sNewLine, sizeof(sNewLine));

            if(sNewLine[0] != '#' && strlen(sNewLine) != 0)
            {
                std::istringstream cFXSpreadLineStream(sNewLine);
                string sElement;

                string sRootSymbol;
                int iSpreadWidth;

                int index = 0;
                while (std::getline(cFXSpreadLineStream, sElement, ','))
                {
                    if(index == 0)
                    {
                        sRootSymbol = sElement;
                    }
                    else if(index == 1)
                    {
                        iSpreadWidth = atoi(sElement.c_str());
                    }

                    index = index + 1;
                }

                _mFXArtificialSpread.insert(std::pair<string, int>(sRootSymbol, iSpreadWidth));
            }
        }
    }

}

StaticDataHandler::~StaticDataHandler()
{

}

long StaticDataHandler::iGetNY4Latency(const string& sKOProduct)
{
    string sMapKey = sKOProduct;
    map<string, long>::iterator itr = _mProductNY4Latency.find(sMapKey);
    if(itr != _mProductNY4Latency.end())
    {
        return itr->second;
    }
    else
    {
        return 0;
    }

}

long StaticDataHandler::iGetTelcityLatency(const string& sKOProduct)
{
    string sMapKey = sKOProduct;
    map<string, long>::iterator itr = _mProductTelcityLatency.find(sMapKey);
    if(itr != _mProductTelcityLatency.end())
    {
        return itr->second;
    }
    else
    {
        return 0;
    }

}

string StaticDataHandler::sGetRootSymbol(const string& sProduct, InstrumentType eInstrumentType)
{
    string sResult = "";

    std::size_t iDotPos = sProduct.find(".");
    string sContract = "";

    if(iDotPos == std::string::npos)
    {
        sContract = sProduct;
    }
    else
    {
        sContract = sProduct.substr(iDotPos+1);
    }

    if(eInstrumentType == KO_FX)
    {
        sResult = sContract;
    }
    else 
    {
        int iContractNameLength = sContract.length();
        int iRootSymbolLength = iContractNameLength - 2;
        sResult = sContract.substr(0, iRootSymbolLength);
    }

    return sResult;
}

string StaticDataHandler::sGetWQSymbol(const string& sProduct, InstrumentType eInstrumentType)
{
    string sResult = "";

    if(eInstrumentType == KO_FUTURE)
    {
        int iProductNameLength = sProduct.length();
        int iRootSymbolLength = iProductNameLength - 2;
        string sExchangePlustRoot = sProduct.substr(0, iRootSymbolLength);

        map<string, string>::iterator itr = _mProductRIC.find(sExchangePlustRoot);
        if(itr != _mProductRIC.end())
        {
            string sRICCode = itr->second;
            if(sRICCode == "YM")
            {
                sRICCode = "1" + sRICCode;
            }

            if(sRICCode == "ED")
            {
                string sYear = sProduct.substr(iRootSymbolLength+1, 1);
                string sMonth = sProduct.substr(iRootSymbolLength, 1);
                if(atoi(sYear.c_str()) > 3)
                {
                    sResult = sRICCode + sMonth + "2" + sYear;
                }
                else
                {
                    sResult = sRICCode + sProduct.substr(iRootSymbolLength, 2);
                }
            }
            else
            {
                sResult = sRICCode + sProduct.substr(iRootSymbolLength, 2);
            }
        }
    }
    else
    {
        string sFXPair = sProduct.substr(4);
        sResult = sFXPair.substr(0, 3) + "/" + sFXPair.substr(3);
    }

    return sResult;

}

string StaticDataHandler::sGetCurrency(const string& sRootSymbol, const string& sExchange)
{
    string sAdjustedExchange = sExchange;

    string sMapKey = sAdjustedExchange + "." + sRootSymbol;
    map<string, string>::iterator itr = _mCurrency.find(sMapKey);
    if(itr != _mCurrency.end())
    {
        return itr->second;
    }
    else
    {
        return "";
    }
}

string StaticDataHandler::sGetHCExchange(const string& sRootSymbol, const string& sExchange)
{
    string sAdjustedExchange = sExchange;

    string sMapKey = sAdjustedExchange + "." + sRootSymbol;
    map<string, string>::iterator itr = _mHCExchange.find(sMapKey);
    if(itr != _mHCExchange.end())
    {
        return itr->second;
    }
    else
    {
        return "";
    }
}

double StaticDataHandler::dGetContractSize(const string& sRootSymbol, const string& sExchange)
{
    string sAdjustedExchange = sExchange;

    string sMapKey = sAdjustedExchange + "." + sRootSymbol;
    map<string, double>::iterator itr = _mContractSize.find(sMapKey);
    if(itr != _mContractSize.end())
    {
        return itr->second;
    }
    else
    {
        return 0;
    }
}

double StaticDataHandler::dGetTradingFee(const string& sRootSymbol, const string& sExchange)
{
    string sAdjustedExchange = sExchange;

    string sMapKey = sAdjustedExchange + "." + sRootSymbol;
    map<string, double>::iterator itr = _mTradingFee.find(sMapKey);
    if(itr != _mTradingFee.end())
    {
        return itr->second;
    }
    else
    {
        return 10000;
    }
}

double StaticDataHandler::dGetFXRate(const string& sFXPair)
{
    map<string, double>::iterator itr = _mFXRate.find(sFXPair);
    if(itr != _mFXRate.end())
    {
        return itr->second;
    }
    else
    {   
        return 0;
    }
}

int StaticDataHandler::iGetFXArticifialSpread(const string& sRootSymbol)
{
    map<string, int>::iterator itr = _mFXArtificialSpread.find(sRootSymbol);
    if(itr != _mFXArtificialSpread.end())
    {
        return itr->second;
    }
    else
    {   
        return 0;
    }
}

double StaticDataHandler::dGetTickSize(const string& sFullProductName)
{
    map<string, double>::iterator itr = _mTickSize.find(sFullProductName);
    if(itr != _mTickSize.end())
    {
        return itr->second;
    }
    else
    {
        return 0;
    }
}

KOEpochTime StaticDataHandler::cGetMarketOpenTime(const string& sRootSymbol, const string& sExchange)
{
    string sAdjustedExchange = sExchange;

    string sMapKey = sAdjustedExchange + "." + sRootSymbol;
    map<string, KOEpochTime>::iterator itr = _mMarketOpenTime.find(sMapKey);
    if(itr != _mMarketOpenTime.end())
    {
        return itr->second;
    }
    else
    {
        return KOEpochTime();
    }
}

KOEpochTime StaticDataHandler::cGetMarketCloseTime(const string& sRootSymbol, const string& sExchange)
{
    string sAdjustedExchange = sExchange;

    string sMapKey = sAdjustedExchange + "." + sRootSymbol;
    map<string, KOEpochTime>::iterator itr = _mMarketCloseTime.find(sMapKey);
    if(itr != _mMarketCloseTime.end())
    {
        return itr->second;
    }
    else
    {
        return KOEpochTime();
    }
}

string StaticDataHandler::sGetProductTyep(const string& sRootSymbol, const string& sExchange)
{
    string sAdjustedExchange = sExchange;

    string sMapKey = sAdjustedExchange + "." + sRootSymbol;

    map<string, string>::iterator itr = _mProductType.find(sMapKey);
    if(itr != _mProductType.end())
    {
        return itr->second;
    }
    else
    {
        return "";
    }
}

}
