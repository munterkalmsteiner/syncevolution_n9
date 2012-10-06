/*
 *  File:         lineartime.c
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
#include "prefix_file.h"
#include "sync_include.h"
#include "lineartime.h"
#include "timezones.h"

#if defined(SYSYNC_TOOL)
  #include "syncappbase.h" // for CONSOLEPRINTF
  #include "vtimezone.h" // for CONSOLEPRINTF
#endif

namespace sysync {

// Support for SySync Diagnostic Tool
#ifdef SYSYNC_TOOL

// convert between different time formats and zones
int timeConv(int argc, const char *argv[])
{
  if (argc<0) {
    // help requested
    CONSOLEPRINTF(("  time <input zone> <output zone> [<input in ISO8601 or lineartime>]"));
    CONSOLEPRINTF(("    Convert between time zone representations:"));
    CONSOLEPRINTF(("     special input zone names: "));
    CONSOLEPRINTF(("     - \"now\"       : input is current system time (no <input> required)"));
    CONSOLEPRINTF(("     - \"floating\"  : floating time (when no zone from ISO8601 input"));
    CONSOLEPRINTF(("     - \"vtimezone\" : output zone as VTIMEZONE"));
    return EXIT_SUCCESS;
  }

  // check for argument
  if (argc<2 || argc>3) {
    CONSOLEPRINTF(("2 or 3 arguments required"));
    return EXIT_FAILURE;
  }
  // mode
  string inzone,outzone,s,z;
  // internal representation
  lineartime_t intime;
  sInt16 minOffs;
  timecontext_t incontext,outcontext;
  GZones zones;
  // get input mode
  inzone=argv[0];
  outzone=argv[1];
  // check special "now" case
  if (strucmp(inzone.c_str(),"now")==0) {
    // input time is current time in system zone
    incontext = TCTX_SYSTEM;
    intime = getSystemNowAs(incontext,&zones);
  }
  else {
    // get input context from name
    if (!TimeZoneNameToContext(inzone.c_str(),incontext,&zones))
      incontext=TCTX_UNKNOWN;
    // input time from 3rd argument
    if (argc!=3) {
      CONSOLEPRINTF(("input time required as 3rd argument"));
      return EXIT_FAILURE;
    }
    // try to parse as ISO8601
    timecontext_t inpctx;
    uInt16 n=ISO8601StrToTimestamp(argv[2],intime,inpctx);
    if (n==0 || argv[2][n]!=0) {
      // no ISO, read as lineartime
      inpctx=TCTX_UNKNOWN;
      n=StrToLongLong(argv[2],intime);
      if (n==0) {
        CONSOLEPRINTF(("input time must be either ISO8601 or decimal lineartime units"));
        return EXIT_FAILURE;
      }
    }
    if (!TCTX_IS_UNKNOWN(inpctx))
      incontext=inpctx;
  }
  // get output context
  if (strucmp(outzone.c_str(),"vtimezone")==0) {
    // show input time zone as VTIMEZONE and DAYLIGHT (for current date)
    intime = getSystemNowAs(incontext,&zones);
    internalToVTIMEZONE(incontext,z,&zones);
    CONSOLEPRINTF(("Input time zone represented as VTIMEZONE :\n\nBEGIN:VTIMEZONE\n%sEND:VTIMEZONE\n",z.c_str()));
    timecontext_t stdoffs;
    ContextToTzDaylight(incontext,intime,z,stdoffs,&zones);
    s.erase(); ContextToISO8601StrAppend(s, stdoffs, true);
    CONSOLEPRINTF(("Input time zone represented as TZ/DAYLIGHT :\n\nTZ:%s\nDAYLIGHT:%s\n",s.c_str(),z.c_str()));
  }
  else if (!TimeZoneNameToContext(outzone.c_str(),outcontext,&zones))
    outcontext=TCTX_UNKNOWN;
  // now show
  CONSOLEPRINTF((""));
  // - input
  TimestampToISO8601Str(s, intime, incontext, true, true);
  TimeZoneContextToName(incontext, z, &zones);
  TzResolveToOffset(incontext, minOffs, intime, false, &zones);
  CONSOLEPRINTF(("Input  : %-25s (%+03hd:%02hd - '%s')", s.c_str(), minOffs/MinsPerHour, abs(minOffs)%MinsPerHour, z.c_str()));
  // - convert to output
  if (!TzConvertTimestamp(intime,incontext,outcontext,&zones)) {
    CONSOLEPRINTF(("input zone cannot be converted to output zone"));
    return EXIT_FAILURE;
  }
  else {
    // - input
    TimestampToISO8601Str(s, intime, outcontext, true, true);
    TimeZoneContextToName(outcontext, z, &zones);
    TzResolveToOffset(outcontext, minOffs, intime, false, &zones);
    CONSOLEPRINTF(("Output : %-25s (%+03hd:%02hd - '%s')", s.c_str(), minOffs/MinsPerHour, abs(minOffs)%MinsPerHour, z.c_str()));
  }
  return EXIT_SUCCESS;
} // timeConv

#endif // SYSYNC_TOOL




#ifndef PLATFORM_LROUND
// use generic implementation of lround
static sInt32 lround(double x) {
  sInt32 l;
  l=(sInt32)(x+0.5);
  return l;
}
#endif

#ifndef PLATFORM_TRUNC
// use generic implementation of trunc
static double trunc(double x) {
  sInt64 ll;
  ll=(sInt64)(x);
  return (double)ll;
}
#endif


#ifndef PLATFORM_DATE2LINEARDATE

// helper: Returns the biggest integer smaller than x
static sInt32 lfloor(double x) {
  #if ( defined __MACH__ && defined __GNUC__ ) || defined _MSC_VER // XCode or Visual Studio
    if (x<0) x= x-1;
    return lround( x-0.5 );
  #else
    return (sInt32)floor( x );
  #endif
} // lfloor

// convert date to linear date (generic version using our internal scale)
/* procedure calcEphTime(year:integer;month,day:byte;hour:single;var julDat:double);
    { Berechnet Julianisches Datum aus Weltzeit } */
lineardate_t date2lineardate(sInt16 aYear, sInt16 aMonth, sInt16 aDay)
{
  // use custom algorithm that goes back to year -4712...

  /* const MinYear= -4712; */
  const sInt32 MinYear = -4712;

  /* var a,b:integer; */
  sInt32 a,b;

  /* if aMonth<3 then begin year:=year-1; aMonth:=aMonth+12; end; */
  if (aMonth<3) { aYear--; aMonth+=12; }
  /* if (aYear<1582) or
    ((aYear=1582) and (aMonth<10)) or
    ((aYear=1582) and (aMonth=10) and (aDay<15)) */
  if (
    aYear<1582 ||
    (aYear==1582 && aMonth<10) ||
    (aYear==1582 && aMonth==10 && aDay<15)
  ) {
    // julian
    /* then b:=0    { julianisch } */
    b=0;
  }
  else {
    // gregorian
    /* else begin a:=floor(aYear/100); b:=2-a+floor(a/4) end;    { gregorianisch } */
    a=lfloor(aYear/100);
    b=2-a+lfloor(a/4);
  }
  // now calc julian date
  /*JulDat:=floor(365.25*(aYear-minYear)+1E-6)+round(30.6*(aMonth-3))+aDay+b+58.5;
    JulDat:=JulDat+hour/24; */
  return(
    (lfloor(365.25*(aYear-MinYear)+1E-6)+lround(30.6*(aMonth-3))+aDay+b+59)
    - linearDateOriginOffset // apply offset used for this target platform
  );
} // date2lineardate

#endif // PLATFORM_DATE2LINEARDATE


// convert date to linear time
lineartime_t date2lineartime(sInt16 aYear, sInt16 aMonth, sInt16 aDay)
{
  return date2lineardate(aYear,aMonth,aDay) * linearDateToTimeFactor;
} // date2lineartime


// convert time to linear time
lineartime_t time2lineartime(sInt16 aHour, sInt16 aMinute, sInt16 aSecond, sInt16 aMS)
{
  lineartime_t ti =
    ((((lineartime_t)aHour)*60 +
      (lineartime_t)aMinute)*60 +
     (lineartime_t)aSecond)*secondToLinearTimeFactor;
  if (secondToLinearTimeFactor==1000)
    ti+=aMS;
  return ti;
} // time2lineartime


// convert lineardate to weekday
// 0=sunday, 1=monday ... 6=saturday
sInt16 lineardate2weekday(lineardate_t aLinearDate)
{
  return (aLinearDate+linearDateOriginWeekday) % 7;
} // lineardate2weekday


// convert lineartime to weekday
// 0=sunday, 1=monday ... 6=saturday
sInt16 lineartime2weekday(lineartime_t aLinearTime)
{
  // juldat seems to be sunday-based :-)
  return lineardate2weekday(aLinearTime / linearDateToTimeFactor);
} // lineardate2weekday


// get number of days in a month
sInt16 getMonthDays(lineardate_t aLinearDate)
{
  sInt16 y,m,d;
  lineardate_t ld;

  // get year and month of given date
  lineardate2date(aLinearDate,&y,&m,&d);
  // get first of this month
  ld = date2lineardate(y,m,1);
  // calculate next month
  m++;
  if (m>12) { m=1; y++; }
  // return difference between 1st of current and 1st of next month = number of days in month
  return date2lineardate(y,m,1) - ld;
} // getMonthDays



#ifndef PLATFORM_LINEARDATE2DATE

// convert lineardate to year/month/day
/*
procedure calcDat(zeitZone,julDat:double;var year:integer;var month,day,hour,min:byte;var sec:single);
{ Berechnet das Kalenderdatum und Weltzeit aus Julianischem Datum }
*/
void lineardate2date(lineardate_t aLinearDate,sInt16 *aYearP, sInt16 *aMonthP, sInt16 *aDayP)
{
  // custom algorithm

  /*
  var JD0,JD,C,E:double;
      B,D,F:integer;
      hh:single;
  */
  double C,E;
  sInt32 B,D,F;

  // apply offset correction
  aLinearDate+=linearDateOriginOffset;

  // no time, no "correction" for date change at noon
  /* JD:=julDat+zeitZone/24;
     JD0:=sInt32(Jd+0.5); */
  /*
  if JD0<2299161 then begin
    B:=0;
    C:=JD0+1524;
  end
  else begin
    B:=trunc((JD0-1867216.25)/36524.25);
    C:=Jd0+(B-trunc(B/4))+1525.0;
  end;
  */
  if (aLinearDate<2299161) {
    B=0;
    C=aLinearDate+1524;
  }
  else {
    B=(sInt32)(trunc((aLinearDate-1867216.25)/36524.25));
    C=aLinearDate+(B-trunc((double)B/4))+1525.0;
  }
  /*
  D:=trunc((C-122.1)/365.25);
  E:=365.0*D+trunc(D/4);
  F:=trunc((C-E)/30.6001);
  day:=trunc(C-E+0.5)-trunc(30.6001*F);
  month:=F-1-12*trunc(F/14);
  year:=D-4715-trunc((7+month)/10);
  */
  D=(sInt32)(trunc((C-122.1)/365.25));
  E=365.0*D+trunc((double)D/4);
  F=(sInt32)(trunc((C-E)/30.6001));
  // return date
  sInt16 month = (sInt16)(F-1-12*trunc((double)F/14));
  if (aDayP)   *aDayP=(sInt16)(trunc(C-E+0.5)-trunc(30.6001*F));
  if (aMonthP) *aMonthP=month;
  if (aYearP)  *aYearP=(sInt16)(D-4715-trunc((double)(7+month)/10.0));
  // no time
  /*
  hh:=24*(JD+0.5-JD0);
  hour:=trunc(hh);
  hh:=(hh-hour)*60;
  min:=trunc(hh);
  hh:=(hh-min)*60;
  sec:=trunc(hh);
  */
} // lineardate2date

#endif // PLATFORM_LINEARDATE2DATE


// convert lineartime to year/month/day
void lineartime2date(lineartime_t aLinearTime, sInt16 *aYearP, sInt16 *aMonthP, sInt16 *aDayP)
{
  lineardate2date(lineartime2dateonly(aLinearTime), aYearP, aMonthP, aDayP);
} // lineartime2date


// convert lineartime to h,m,s,ms
void lineartime2time(lineartime_t aLinearTime, sInt16 *aHourP, sInt16 *aMinP, sInt16 *aSecP, sInt16 *aMSP)
{
  if (aLinearTime<0) {
    // negative time, create wrap around to make sure time remains positive
    aLinearTime = lineartime2timeonly(aLinearTime);
  }
  if (secondToLinearTimeFactor==1) {
    // no sub-seconds
    if (aMSP) *aMSP = 0;
  }
  else {
    // we have sub-seconds
    if (aMSP) *aMSP = aLinearTime % secondToLinearTimeFactor;
    aLinearTime /= secondToLinearTimeFactor;
  }
  if (aSecP) *aSecP = aLinearTime % 60;
  aLinearTime /= 60;
  if (aMinP) *aMinP = aLinearTime % 60;
  aLinearTime /= 60;
  if (aHourP) *aHourP = aLinearTime % 24; // to make sure we don't convert date part
} // lineartime2time


// convert seconds to linear time
lineartime_t seconds2lineartime(sInt32 aSeconds)
{
  return aSeconds*secondToLinearTimeFactor;
} // seconds2lineartime


// convert linear time to seconds
sInt32 lineartime2seconds(lineartime_t aLinearTime)
{
  return aLinearTime/secondToLinearTimeFactor;
} // lineartime2seconds


// get time-only part of a linear time
lineartime_t lineartime2timeonly(lineartime_t aLinearTime)
{
  //return aLinearTime % linearDateToTimeFactor;
  return aLinearTime-lineartime2dateonlyTime(aLinearTime);
} // lineartime2timeonly


// get date-only part of a linear time
lineardate_t lineartime2dateonly(lineartime_t aLinearTime)
{
  return aLinearTime/linearDateToTimeFactor - (aLinearTime<0 ? 1 : 0);
} // lineartime2dateonly


// get date-only part of a linear time, in lineartime_t units
lineartime_t lineartime2dateonlyTime(lineartime_t aLinearTime)
{
  lineartime_t ts = lineartime2dateonly(aLinearTime);
  ts *= linearDateToTimeFactor;
  return ts;
} // lineartime2dateonlyTime


} // namespace sysync

/* eof */
