#include <sstream>
#include "SimPositionServer.h"
#include "ErrorHandler.h"

namespace KO
{

using namespace std;

SimPositionServer::SimPositionServer()
{

}

void SimPositionServer::newFill(const string& sProduct, const string& sAccount, long iFillQty)
{
    stringstream cStringStream;
    cStringStream << "Sim Position Server recevied new fill. Product: " << sProduct << " Account: " << sAccount << " Qty: " << iFillQty;
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    map<string, map<string, long> >::iterator productItr = _mProductAccountPos.find(sProduct);

    if(productItr == _mProductAccountPos.end())
    {
        productItr = _mProductAccountPos.insert(std::pair<string, map<string, long> >(sProduct, map<string, long>())).first;
    }

    map<string, long>::iterator accountItr = productItr->second.find(sAccount);

    if(accountItr == productItr->second.end())
    {
        accountItr = productItr->second.insert(std::pair<string, long>(sAccount, iFillQty)).first;
    }
    else
    {
        accountItr->second = accountItr->second + iFillQty;
    }
}

long SimPositionServer::igetPosition(const string& sProduct, const string& sAccount)
{
    long iResult = 0;

    map<string, map<string, long> >::iterator productItr = _mProductAccountPos.find(sProduct);

    if(productItr != _mProductAccountPos.end())
    {
        if(sAccount.compare("ALL") == 0)
        {
            for(map<string, long>::iterator accountItr = productItr->second.begin();
                accountItr != productItr->second.end();
                accountItr++)
            {
                iResult = iResult + accountItr->second;
            }
        }
        else
        {
            map<string, long>::iterator accountItr = productItr->second.find(sAccount);

            if(accountItr != productItr->second.end())
            {
                iResult = accountItr->second;
            }
        }
    }

    stringstream cStringStream;
    cStringStream << "Sim Position Server replied new position request. Product: " << sProduct << " Account: " << sAccount << " Result: " << iResult;
    ErrorHandler::GetInstance()->newInfoMsg("0", "ALL", "ALL", cStringStream.str());

    return iResult;
}

}
