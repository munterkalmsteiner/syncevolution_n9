/*
 *  File:         lineartime.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  conversion from/to linear time scale.
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 *  2002-04-14 : luz : created from pascal source (plani.ch)
 *
 */


#ifndef LINEARTIME_H
#define LINEARTIME_H

#include "prefix_file.h"
#include "generic_types.h"


#ifndef PLATFORM_LINEARTIMEDEF

// Standard lineartime_t definition as 64bit integer,
// in milliseconds since -4712-01-01 00:00:00
// --------------------------------------------------

#ifdef __cplusplus
  namespace sysync {
#endif

// Linear date and time types
typedef sInt32 lineardate_t;
typedef sInt64 lineartime_t;

// max and min constants
const lineartime_t  noLinearTime = 0x0; ///< undefined lineartime value
#ifdef _MSC_VER
const lineartime_t maxLinearTime = 0x7FFFFFFFFFFFFFFFi64; ///< maximum future lineartime (signed 64 bit)
#else
const lineartime_t maxLinearTime = 0x7FFFFFFFFFFFFFFFLL; ///< maximum future lineartime (signed 64 bit)
#endif

// date origin definition relative to algorithm's origin -4712-01-01 00:00:00
const lineardate_t linearDateOriginOffset=0; ///< offset between algorithm's origin (-4712-01-01) and lineardate_t's zero
const sInt16 linearDateOriginWeekday=1; ///< weekday of lineartime origin: Monday

// scaling of lineartime relative to seconds
const lineartime_t secondToLinearTimeFactor = 1000; ///< how many lineartime_t units make a seconds
const lineartime_t nanosecondsPerLinearTime = 1000000; ///< duration of one lineartime_t in nanoseconds


#ifdef __cplusplus
  } // namespace sysync
#endif

#endif // not PLATFORM_LINEARTIMEDEF


// the platform specific definitions of the time support
// Note: if PLATFORM_LINEARTIMEDEF is set, this must define lineartime_t and related
//       constants.
//       If it defines PLATFORM_LINEARDATE2DATE etc.,
//       implementation of these routines must be implemented platform-specific as well.
#include "platform_time.h"


#ifdef __cplusplus
  namespace sysync {
#endif


#ifdef SYSYNC_TOOL

// convert between different time formats and zones
int timeConv(int argc, const char *argv[]);

#endif

// Time context type definition. Defined here to avoid mutual inclusion need of
// this file and timezones.h.
typedef uInt32 timecontext_t; ///< define a time context (dateonly,time zone, etc.)


// Generic utility factors and routines
// ------------------------------------

/// useful time/date definitions
const int SecsPerMin = 60;
const int MinsPerHour= 60;
const int SecsPerHour= SecsPerMin*MinsPerHour;
const int HoursPerDay= 24;
const int DaysPerWk  =  7;


/// @brief conversion factor for lineardate_t to lineartime_t
const lineartime_t linearDateToTimeFactor = (secondToLinearTimeFactor*SecsPerHour*HoursPerDay);

/// @brief offset from lineartime_t to UNIX time(), which is based 1970-01-01 00:00:00
/// @Note units of this constants are still lineartime_t units and need
///       division by secondToLinearTimeFactor to get actual UNIX time in seconds
const lineartime_t UnixToLineartimeOffset =
  (
    2440588 // offset between algorithm base and 1970-01-01
    - linearDateOriginOffset // offset between lineardate_t base and algorithm base
  ) * linearDateToTimeFactor;

/// @brief offset from lineartime_t to NSDate reference time, which is based 2001-01-01 00:00:00
/// @Note units of this constants are still lineartime_t units and need
///       division by secondToLinearTimeFactor to get actual NSDate in seconds
const lineartime_t NSDateToLineartimeOffset =
  (
    2451911 // offset between algorithm base and 2001-01-01
    - linearDateOriginOffset // offset between lineardate_t base and algorithm base
  ) * linearDateToTimeFactor;


/// @brief convert date to linear date
/// @return specified date converted to lineardate_t (unit=days)
/// @param[in] aYear,aMonth,aDay : date specification
lineardate_t date2lineardate(sInt16 aYear, sInt16 aMonth, sInt16 aDay);

/// @brief convert date to linear time
/// @return specified date converted to lineartime_t (unit=lineartime units)
/// @param[in] aYear,aMonth,aDay : date specification
lineartime_t date2lineartime(sInt16 aYear, sInt16 aMonth, sInt16 aDay);

/// @brief convert time to linear time
/// @return specified time converted to lineartime_t units
/// @param[in] aMinute,aSecond,aMS : time specification
lineartime_t time2lineartime(sInt16 aHour, sInt16 aMinute, sInt16 aSecond, sInt16 aMS);

/// @brief convert lineardate to weekday
/// @return 0=sunday, 1=monday ... 6=saturday
/// @param[in] aLinearDate linear date (in days)
sInt16 lineardate2weekday(lineardate_t aLinearDate);

/// @brief convert lineartime to weekday
/// @return 0=sunday, 1=monday ... 6=saturday
/// @param[in] aLinearTime linear time (in lineartime_t units)
sInt16 lineartime2weekday(lineartime_t aLinearTime);

/// @brief convert lineardate to year/month/day
/// @param[in] aLinearDate linear date (in days)
/// @param[out] aYearP,aMonthP,aDayP : date components, may be NULL if component not needed
void lineardate2date(lineardate_t aLinearDate,sInt16 *aYearP, sInt16 *aMonthP, sInt16 *aDayP);

/// @brief convert lineartime to year/month/day
/// @param[in] aLinearTime linear time (in lineartime_t units)
/// @param[out] aYearP,aMonthP,aDayP : date components, may be NULL if component not needed
void lineartime2date(lineartime_t aLinearTime, sInt16 *aYearP, sInt16 *aMonthP, sInt16 *aDayP);

/// @brief get number of days in a month
/// @return number of days in month (28..31)
/// @param[in] aLinearDate linear date (in days)
sInt16 getMonthDays(lineardate_t aLinearDate);

/// @brief convert lineartime to h,m,s,ms
/// @param[in] aLinearTime linear time (in lineartime_t units)
/// @param[out] aHourP,aMinP,aSecP,aMSP : time components, may be NULL if component not needed
void lineartime2time(lineartime_t aLinearTime, sInt16 *aHourP, sInt16 *aMinP, sInt16 *aSecP, sInt16 *aMSP);

/// @brief convert seconds to linear time
/// @return number of lineartime_t units
/// @param[in] aSeconds a number of seconds
lineartime_t seconds2lineartime(sInt32 aSeconds);

/// @brief convert linear time to seconds
/// @return number of seconds
/// @param[in] aLinearTime lineartime_t units
sInt32 lineartime2seconds(lineartime_t aLinearTime);

/// @brief get time-only part of a linear time
/// @return time only in lineartime_t units since midnight
/// @param[in] aLinearTime a date/timestamp in lineartime_t units
lineartime_t lineartime2timeonly(lineartime_t aLinearTime);

/// @brief get date-only part of a linear time
/// @return date only in lineardate_t units (days)
/// @param[in] aLinearTime a date/timestamp in lineartime_t units
lineardate_t lineartime2dateonly(lineartime_t aLinearTime);

/// @brief get date-only part, but IN LINEARTIME
/// @return date only in lineartime_t units
/// @param[in] aLinearTime a date/timestamp in lineartime_t units
lineartime_t lineartime2dateonlyTime(lineartime_t aLinearTime);



// Implementation of the following routines is platform specific
// -------------------------------------------------------------

/// @brief fine resolution sleep support
/// @param[in] aHowLong desired time to wait in lineartime_t units
void sleepLineartime(lineartime_t aHowLong);


#ifdef __cplusplus
  } // namespace sysync
#endif

#endif // LINEARTIME_H

/* eof */
