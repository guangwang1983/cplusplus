#include "KOEpochTime.h"
#include <boost/date_time/posix_time/posix_time.hpp>

namespace KO
{

KOEpochTime::KOEpochTime()
:_iSecond(0),
 _iMicroSec(0),
 _iPrintable(0)
{

}

KOEpochTime::KOEpochTime(long iSecond, long iMicroSec)
{
    _iPrintable = iSecond * 1000000 + iMicroSec;
    _iSecond = _iPrintable / 1000000;
    _iMicroSec = _iPrintable % 1000000;
}

KOEpochTime::KOEpochTime(long iPrintable)
{
    _iPrintable = iPrintable;
    _iSecond = iPrintable / 1000000;
    _iMicroSec = iPrintable % 1000000;
}

long KOEpochTime::igetPrintable()
{
    return _iPrintable;
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
