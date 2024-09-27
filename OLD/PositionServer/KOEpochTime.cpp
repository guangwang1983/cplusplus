#include "KOEpochTime.h"
#include <boost/date_time/posix_time/posix_time.hpp>

namespace KO
{

KOEpochTime::KOEpochTime()
:_iSecond(0),
 _iMicroSec(0)
{

}

KOEpochTime::KOEpochTime(long iSecond, long iMicroSec)
{
    long iPrintable = iSecond * 1000000 + iMicroSec;
    _iSecond = iPrintable / 1000000;
    _iMicroSec = iPrintable % 1000000;
}

long KOEpochTime::igetPrintable()
{
    return _iSecond * 1000000 + _iMicroSec;
}

KOEpochTime KOEpochTime::operator-(KOEpochTime rhs)
{
    long iPrintableDiff = this->igetPrintable() - rhs.igetPrintable();
    return KOEpochTime(iPrintableDiff / 1000000, iPrintableDiff % 1000000);
}

KOEpochTime KOEpochTime::operator+(KOEpochTime rhs)
{
    long iPrintableDiff = this->igetPrintable() + rhs.igetPrintable();
    return KOEpochTime(iPrintableDiff / 1000000, iPrintableDiff % 1000000);
}

bool KOEpochTime::operator<(KOEpochTime rhs)
{
    return this->igetPrintable() < rhs.igetPrintable();
}

bool KOEpochTime::operator<=(KOEpochTime rhs)
{
    return this->igetPrintable() <= rhs.igetPrintable();
}

bool KOEpochTime::operator>(KOEpochTime rhs)
{
    return this->igetPrintable() > rhs.igetPrintable();
}

bool KOEpochTime::operator>=(KOEpochTime rhs)
{
    return this->igetPrintable() > rhs.igetPrintable();
}

bool KOEpochTime::operator!=(KOEpochTime rhs)
{
    return this->igetPrintable() != rhs.igetPrintable();
}

bool KOEpochTime::operator==(KOEpochTime rhs)
{
    return this->igetPrintable() == rhs.igetPrintable();
}

}
