/*
 *  File:         timeutil.cpp
 *
 *  Authors:      Lukas Zeller (luz@plan44.ch)
 *                Beat Forster (bfo@synthesis.ch)
 *
 *  ISO8601 / Lineartime functions
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 */

//#include "target_options.h"
#include "sync_include.h"

#if defined LINUX || defined MACOSX
  // problems with "trunc"
  #define  _ISOC99_SOURCE 1
  #include <math.h>
#endif

#ifdef _MSC_VER
  static int trunc( double d ) {
    return (int)( d-0.5 );
  }
#endif

#ifdef MACOSX
  #include <ctime>
#endif

#include <time.h>
#include "timeutil.h"
#include "stringutil.h"

using namespace std;

#ifdef __cplusplus
  namespace sysync {
#endif

typedef struct tm struct_tm;
typedef sInt32 lineardate_t;

const lineartime_t linearDateToTimeFactor= (secondToLinearTimeFactor*60*60*24);

// offset between algorithm base and 1970-01-01
const lineartime_t UnixToLineartimeOffset= 2440588*linearDateToTimeFactor;


// static version of time zone calculation
// should be replaced by a full functioning version, when used
static lineartime_t zoneOffsetAsSeconds()
{
  return   (1)*60*60; // for CET
//return (1+1)*60*60; // for CEST
} // zoneOffsetAsSeconds


// get system current date/time
// this very simple implementation is rather local time than UTC
lineartime_t utcNowAsLineartime()
{
  lineartime_t secs= time(NULL) - zoneOffsetAsSeconds();
  return       secs*secondToLinearTimeFactor + UnixToLineartimeOffset;
} // utcNowAsLineartime



static void lineardate2date( lineardate_t julDat,sInt16 *year, sInt16 *month, sInt16 *day )
{
  double C,E;
  sInt32 B,D,F;

  if (julDat<2299161) {
    B=0;
    C=julDat+1524;
  }
  else {
    B= (sInt32)(trunc((julDat-1867216.25)/36524.25));
    C=julDat+(B-trunc((double)B/4))+1525.0;
  } // if

  D=(sInt32)(trunc((C-122.1)/365.25));
  E= 365.0*D+trunc((double)D/4);
  F=(sInt32)(trunc((C-E)/30.6001));

  // return date
  *day  = (sInt16)(trunc(C-E+0.5)-trunc(30.6001*F));
  *month= (sInt16)(F-1-12*trunc((double)F/14));
  *year = (sInt16)(D-4715-trunc((double)(7+*month)/10.0));
} // lineardate2date



// convert lineartime to h,m,s,ms
static void lineartime2time( lineartime_t aTim, sInt16 *hour, sInt16 *min, sInt16 *sec, sInt16 *ms )
{
  // we have sub-seconds
  *ms  = aTim %  secondToLinearTimeFactor;
         aTim /= secondToLinearTimeFactor;

  *sec = aTim % 60; aTim /= 60;
  *min = aTim % 60; aTim /= 60;
  *hour= aTim % 24; // to make sure we don't convert date part
} // lineartime2time



// convert struct tm to linear time (in milliseconds)
static void lineartime2tm( lineartime_t aLt, struct_tm *tim )
{
  sInt16 y,mo,d, h,m,s, ms;

  // date
  lineardate2date( aLt/linearDateToTimeFactor, &y,&mo,&d );
  tim->tm_year= y-1900;
  tim->tm_mon = mo-1;
  tim->tm_mday= d;

  // time
  lineartime2time( aLt, &h,&m,&s, &ms );
  tim->tm_hour= h;
  tim->tm_min = m;
  tim->tm_sec = s;
} // tm2lineartime



// convert UTC lineartimestamp to ISO8601 string representation
// (and return how long expired)
sInt32 timeStampToISO8601( lineartime_t aTimeStamp, string &aString, bool dateOnly )
{
  aString.erase();
  if (aTimeStamp==0) return 0;

  struct_tm                  t;
  lineartime2tm( aTimeStamp,&t );

  // format as ISO8601 UTC
  StringObjAppendPrintf  ( aString,      "%04d%02d%02d", t.tm_year+1900, t.tm_mon+1, t.tm_mday );

  if (!dateOnly) {
    StringObjAppendPrintf( aString,  "T%02hd%02hd%02hd", t.tm_hour,      t.tm_min,   t.tm_sec  );
                           aString+= 'Z'; // add UTC designator
  } // if

  return 0;
} // timeStampToISO8601


#ifdef __cplusplus
  } // namespace
#endif

/* eof */

