#ifndef SystemClock_H
#define SystemClock_H

#include <boost/date_time/posix_time/posix_time.hpp>
#include <queue>
#include <deque>

#include "KOOrder.h"
#include "KOEpochTime.h"
#include "TimeEvent.h"
#include "EngineEvent.h"
#include "Figures.h"
#include "SimulationExchange.h"

using std::deque;

namespace KO
{

class SchedulerBase;

class SystemClock
{
public:
	static boost::shared_ptr<SystemClock> GetInstance();

    void init(string sTodayDate, SchedulerBase* pScheduler);

    string sToSimpleString(KOEpochTime cTime);

    KOEpochTime cCreateKOEpochTimeFromUTC(string sDate, string sHour, string sMinute, string sSecond);
    KOEpochTime cCreateKOEpochTimeFromCET(string sDate, string sHour, string sMinute, string sSecond);

    KOEpochTime cgetCurrentKOEpochTime();

    long igetTodayInInt();
	
private:
	SystemClock();

    boost::posix_time::ptime cCETToUTC(boost::posix_time::ptime cCETTime);

	static boost::shared_ptr<SystemClock> _pInstance;

    deque<TimeEvent*> _vStaticTimeEventQueue;

    KOEpochTime _cCETDiffUTC;

    SchedulerBase* _pScheduler;

    long _iTodayInInt;
};

}

#endif /* SystemEventClock_H */
