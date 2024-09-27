#ifndef KOEpochTime_H
#define KOEpochTime_H

#include <string>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace KO
{

class KOEpochTime
{
public:
    KOEpochTime();

    KOEpochTime(long iSecond, long iMicroSec);
    KOEpochTime(long iPrintable);

    long igetPrintable();

    KOEpochTime operator-(KOEpochTime rhs);
    KOEpochTime operator+(KOEpochTime rhs);
    bool operator<(KOEpochTime rhs);
    bool operator<=(KOEpochTime rhs);
    bool operator>(KOEpochTime rhs);
    bool operator>=(KOEpochTime rhs);
    bool operator!=(KOEpochTime rhs);
    bool operator==(KOEpochTime rhs);

    long sec()
    {
        return _iSecond;
    }

    long microsec()
    {
        return _iMicroSec;
    }

private:
    long _iSecond;
    long _iMicroSec;
    long _iPrintable;
};

}

#endif /* KOEpochTime_H */
