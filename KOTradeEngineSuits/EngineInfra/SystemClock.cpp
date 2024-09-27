#include "SystemClock.h"
#include "SchedulerBase.h"
#include <boost/date_time/local_time_adjustor.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/local_timezone_defs.hpp>
#include <sys/time.h>

namespace KO
{

boost::shared_ptr<SystemClock> SystemClock::_pInstance = boost::shared_ptr<SystemClock>();

SystemClock::SystemClock()
{
    _cCETDiffUTC = KOEpochTime(0);
}

boost::shared_ptr<SystemClock> SystemClock::GetInstance()
{
	if(!_pInstance.get())
	{
		_pInstance.reset(new SystemClock);

	}

	return _pInstance;
}

void SystemClock::init(string sTodayDate, SchedulerBase* pScheduler)
{
    _pScheduler = pScheduler;
    _iTodayInInt = atoi(sTodayDate.c_str());
    boost::gregorian::date _cTodayDate = boost::gregorian::from_undelimited_string(sTodayDate);
    boost::posix_time::ptime cCETTime (_cTodayDate, boost::posix_time::time_duration(12,0,0,0));
    boost::posix_time::ptime cUTCTime = cCETToUTC(cCETTime);
    _cCETDiffUTC = KOEpochTime((cCETTime - cUTCTime).total_seconds(),0);
}

long SystemClock::igetTodayInInt()
{
    return _iTodayInInt;
}

boost::posix_time::ptime SystemClock::cCETToUTC(boost::posix_time::ptime cCETTime)
{
    typedef boost::date_time::eu_dst_trait<boost::gregorian::date> eu_dst_traits;
    typedef boost::date_time::dst_calc_engine<boost::gregorian::date, boost::posix_time::time_duration, eu_dst_traits> eu_dst_calc;
    typedef boost::date_time::local_adjustor<boost::posix_time::ptime, 1, eu_dst_calc> CET;

    boost::posix_time::ptime cUTCTime = CET::local_to_utc(cCETTime);    
    
    return cUTCTime;
}

KOEpochTime SystemClock::cgetCurrentKOEpochTime()
{
    return _pScheduler->cgetCurrentTime();
}

KOEpochTime SystemClock::cCreateKOEpochTimeFromCET(string sDate, string sHour, string sMinute, string sSecond)
{
    if(_cCETDiffUTC.igetPrintable() == 0)
    {
        boost::gregorian::date cTodayDate = boost::gregorian::from_undelimited_string(sDate);
        boost::posix_time::ptime cCETTime (cTodayDate, boost::posix_time::time_duration(12,0,0,0));
        boost::posix_time::ptime cUTCTime = cCETToUTC(cCETTime);
        _cCETDiffUTC = KOEpochTime((cCETTime - cUTCTime).total_seconds(),0);
    }

    boost::posix_time::ptime cResultPtime(boost::gregorian::from_undelimited_string(sDate), boost::posix_time::time_duration(atoi(sHour.c_str()), atoi(sMinute.c_str()), atoi(sSecond.c_str())));
    boost::posix_time::ptime cEpoch(boost::gregorian::date(1970,1,1));

    KOEpochTime cResult = KOEpochTime((cResultPtime - cEpoch).total_seconds(), 0) - _cCETDiffUTC;

    return cResult;
}

KOEpochTime SystemClock::cCreateKOEpochTimeFromUTC(string sDate, string sHour, string sMinute, string sSecond)
{
    boost::posix_time::ptime cResultPtime(boost::gregorian::from_undelimited_string(sDate), boost::posix_time::time_duration(atoi(sHour.c_str()), atoi(sMinute.c_str()), atoi(sSecond.c_str())));
    boost::posix_time::ptime cEpoch(boost::gregorian::date(1970,1,1));

    KOEpochTime cResult = KOEpochTime((cResultPtime - cEpoch).total_seconds(), 0);

    return cResult;
}

string SystemClock::sToSimpleString(KOEpochTime cTime)
{
    KOEpochTime cLocalTime = cTime + _cCETDiffUTC;

    boost::posix_time::ptime cResult = boost::posix_time::from_time_t(cLocalTime.sec());
    boost::posix_time::time_duration cFractionalSeconds(0,0,0,cLocalTime.microsec());
    cResult = cResult + cFractionalSeconds;

    if(cResult.time_of_day().fractional_seconds() == 0)
    {
        return boost::posix_time::to_simple_string(cResult) + ".000000";
    }
    else
    {
        return boost::posix_time::to_simple_string(cResult);
    }
}

}
