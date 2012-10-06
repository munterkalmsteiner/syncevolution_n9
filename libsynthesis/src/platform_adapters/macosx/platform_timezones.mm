/*
 *  File:         platform_timezones.mm
 *
 *  Author:       Lukas Zeller
 *
 *  Time zone dependent routines for Mac OS X
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 *  2009-04-02 : Created by Lukas Zeller
 *
 */

#include "prefix_file.h"
#include "timezones.h"

#import <Foundation/NSTimeZone.h>

namespace sysync {

/* Note: this is ugly poor man's implementation and only uses plain Unix facilities to hopefully
 *       find the time zone (by name).
 *       The plan is to enhance this using Foundation APIs later
 */



/*! @brief platform specific loading of time zone definitions
 *  @return true if this list is considered complete (i.e. no built-in zones should be used additionally)
 *  @param[in/out] aGZones : the GZones object where system zones should be loaded into
 */
bool loadSystemZoneDefinitions(GZones* aGZones)
{
  // load zones from system here
  // ...
  // return true if this list is considered complete (i.e. no built-in zones should be used additionally)
  return false; // we need the built-in zones
} // loadSystemZoneDefinitions


/*! @brief we use this callback to add and log libical time zone handling
 *
 * The advantage is that this handling can be logged. The disadvantage
 * is that these time zones cannot be used in the configuration. Builtin
 * time zones (if any) have to be used there.
 */
void finalizeSystemZoneDefinitions(GZones* aGZones)
{
  /* nop for now */
} // finalizeSystemZoneDefinitions


// Get system time zone information via NSTimeZone
static bool getSystemTimeZone(string &aZoneName, sInt16 &aStdBias, sInt16 &aDstBias, lineartime_t &aStdStart, lineartime_t &aDstStart)
{
  NSTimeZone *sysZone = [NSTimeZone systemTimeZone];

  // get name
  // Note: we use the oldfashioned tzset() way to make sure we don't break what already works
  //       Probably we could construct the same string using abbreviationForDate:
  tzset();
  aZoneName = tzname[0];
  if (strcmp(aZoneName.c_str(),tzname[1])!=0) {
    aZoneName += "/";
    aZoneName += tzname[1];
  }
  // get info per now
  bool isDaylightNow = [sysZone isDaylightSavingTime];
  sInt32 utcOffsetNow = [sysZone secondsFromGMT];
  // next transition (away from current state)
  NSDate *nextTransition = [sysZone nextDaylightSavingTimeTransition];
  // the transition after that
  NSDate *backTransition = [sysZone nextDaylightSavingTimeTransitionAfterDate:[nextTransition dateByAddingTimeInterval:24*60*60]];
  // depends on current daylight or not
  if (isDaylightNow) {
    // DST active, next transition is start of STD
    sInt32 daylightOffset = (sInt32)[sysZone daylightSavingTimeOffset];
    aDstBias = daylightOffset/60;
    aStdBias = (utcOffsetNow-daylightOffset)/60;
    aStdStart = (lineartime_t)([nextTransition timeIntervalSince1970]*1000.0)+UnixToLineartimeOffset + (aStdBias+aDstBias)*60*secondToLinearTimeFactor;
    aDstStart = (lineartime_t)([backTransition timeIntervalSince1970]*1000.0)+UnixToLineartimeOffset + aStdBias*60*secondToLinearTimeFactor;
  }
  else {
    // STD active, next transition is start of DST
    // Note: backTransition IS NO LONGER (iOS 4.2.1 at least!) in DST, so use a time one day after to-DST transition which MUST be in DST!
    sInt32 daylightOffset = (sInt32)[sysZone daylightSavingTimeOffsetForDate:[nextTransition dateByAddingTimeInterval:24*60*60]];
    aStdBias = utcOffsetNow/60;
    aDstBias = daylightOffset/60;
    aDstStart = (lineartime_t)([nextTransition timeIntervalSince1970]*1000.0)+UnixToLineartimeOffset + aStdBias*60*secondToLinearTimeFactor;
    aStdStart = (lineartime_t)([backTransition timeIntervalSince1970]*1000.0)+UnixToLineartimeOffset + (aStdBias+aDstBias)*60*secondToLinearTimeFactor;
  }
  return true;
} // getSystemTimeZone



/*! @brief get current system time zone
 *  @return true if successful
 *  @param[out] aContext : the time zone context representing the current system time zone.
 *  @param[in] aGZones : the GZones object.
 */
bool getSystemTimeZoneContext(timecontext_t &aContext, GZones* aGZones)
{
  tz_entry t;
  t.name = "";
  t.bias = 0;
  t.biasDST = 0;
  t.dst.wMonth= 0;
  sInt16 y;

  lineartime_t stdTime, dstTime;
  if (getSystemTimeZone( t.name, t.bias,t.biasDST, stdTime,dstTime )) {
    Get_tChange( stdTime, t.std, y );
    Get_tChange( dstTime, t.dst, y );
  }
  // search entry, first by rule
  return ContextForEntry(aContext, t, false, aGZones);
} // getSystemTimeZoneContext



} // namespace sysync

/* eof */