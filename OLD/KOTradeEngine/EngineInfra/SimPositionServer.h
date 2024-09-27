#ifndef SimPositionServer_H
#define SimPositionServer_H

#include <map>
#include <string>
#include <boost/shared_ptr.hpp>

using std::string;
using std::map;

namespace KO
{

class SimPositionServer 
{
public:
	SimPositionServer();
    void newFill(const string& sProduct, const string& sAccount, long iFillQty);
    long igetPosition(const string& sProduct, const string& sAccount);

private:
    map<string, map<string, long> > _mProductAccountPos;
};

}

#endif /* SimPositionServer_H */
