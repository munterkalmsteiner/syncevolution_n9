/*
 *  File:         iso8601.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  conversion from/to linear time scale.
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 *  2002-05-02 : luz : extracted from sysync_utils
 *
 */

#include "prefix_file.h"

#include "iso8601.h"
#include "stringutils.h"
#include "timezones.h"

#if defined(EXPIRES_AFTER_DATE) && !defined(FULLY_STANDALONE)
  // only if used in sysync context
  #include "sysync.h"
#endif


namespace sysync {

/// @brief convert ISO8601 to timestamp and timezone
/// @return number of successfully converted characters or 0 if no valid ISO8601 specification could be decoded
/// @param[in] aISOString input string in ISO8601
/// @param[out] aTimestamp representation of ISO time spec as is (no time zone conversions)
/// @param[out] aWithTime if set, time specification was found in input string
/// @param[out] aTimeContext:
///             TCTX_DURATION if ISO8601 is a duration format (PnDTnHnMnS.MS)
///             TCTX_UNKNOWN if ISO8601 does not include a time zone specification
///             TCTX_UTC if ISO8601 ends with the "Z" specifier
///             TCTX_DATEONLY if ISO8601 only contains a date, but no time
///             TCTX_OFFSCONTEXT(xx) if ISO8601 explicitly specifies a UTC offset
sInt16 ISO8601StrToTimestamp(cAppCharP aISOString, lineartime_t &aTimestamp, timecontext_t &aTimeContext)
{
  uInt16 y,m,d,hr,mi,s,ms; // unsigned because these can't be signed when parsing
  bool isExtended=false;
  sInt16 n,h,sg;

  n=0;
  // check if it might be duration
  sg=0; // duration sign
  // - sign
  if (*aISOString=='-') {
    sg=-1; // negative duration
    aISOString++; n++;
  }
  else if(*aISOString=='+') {
    sg=1; // positive duration
    aISOString++; n++;
  }
  if (*aISOString=='P') {
    if (sg==0) sg=1;
    aISOString++; n++;
  }
  else if (sg!=0)
    return 0; // we had a sign, but no 'P' -> invalid ISO8601 date
  // if y!=0 here, this is a duration
  if (sg!=0) {
    // duration
    aTimestamp = 0;
    aTimeContext = TCTX_UNKNOWN + TCTX_DURATION;
    bool timepart = false;
    // parse duration parts
    while (true) {
      if (*aISOString=='T' && !timepart) {
        timepart = true;
        aISOString++; n++;
      }
      #ifdef NO_FLOATS
      sInt32 part;
      h = StrToLong(aISOString,part);
      #else
      double part;
      h = StrToDouble(aISOString,part);
      #endif
      if (h>0) {
        aISOString+=h; n+=h;
        // this is a part, must be followed by a designator
        switch (*aISOString) {
          case 'Y':
            part*=365; goto days; // approximate year
          case 'M': // month or minute
            if (timepart) {
              aTimestamp += (lineartime_t)(part*sg*secondToLinearTimeFactor*SecsPerMin); // minute
              break;
            }
            else {
              part*=30; // approximate month
              goto days;
            }
          case 'W':
            part *= 7; goto days; // one week
          case 'D':
          days:
            aTimestamp += (lineartime_t)(part*sg*linearDateToTimeFactor);
            break;
          case 'H':
            aTimestamp += (lineartime_t)(part*sg*secondToLinearTimeFactor*SecsPerHour);
            break;
          case 'S':
            aTimestamp += (lineartime_t)(part*sg*secondToLinearTimeFactor);
            break;
          default:
            return 0; // bad designator, error
        }
        aISOString++; n++;
      } // if number
      else {
        // no more digits -> end of duration string
        return n; // number of chars processed
      }
    } // while
  } // if duration
  // try to parse date
  // - first should be 4 digit year
  h = StrToUShort(aISOString,y,4);
  if (h!=4) return 0; // no ISO8601 date
  aISOString+=h; n+=h;
  // - test for format
  if (*aISOString=='-') {
    isExtended=true;
    aISOString++; n++;
  }
  // - next must be 2 digit month
  h = StrToUShort(aISOString,m,2);
  if (h!=2) return 0; // no ISO8601 date
  aISOString+=h; n+=h;
  // - check separator in case of extended format
  if (isExtended) {
    if (*aISOString != '-') return 0; // missing separator, no ISO8601 date
    aISOString++; n++;
  }
  // - next must be 2 digit day
  h = StrToUShort(aISOString,d,2);
  if (h!=2) return 0; // no ISO8601 date
  aISOString+=h; n+=h;
  // convert date to timestamp
  aTimestamp = date2lineartime(y,m,d);
  // Now next must be "T" if time spec is included
  if (*aISOString!='T') {
    // date-only, floating
    aTimeContext = TCTX_DATEONLY|TCTX_UNKNOWN;
  }
  else {
    // parse time as well
    aISOString++; n++; // skip "T"
    mi=0; s=0; ms=0; // reset optional time components
    // - next must be 2 digit hour
    h = StrToUShort(aISOString,hr,2);
    if (h!=2) return 0; // no ISO8601 time, we need the hour, minimally
    aISOString+=h; n+=h;
    // - check separator in case of extended format
    if (isExtended) {
      if (*aISOString != ':') return 0; // missing separator, no ISO8601 time (Note: hour-only reduced precision does not exist for extended format)
      aISOString++; n++;
    }
    // - next must be 2 digit minute (or nothing for hour-only reduced precision basic format)
    h = StrToUShort(aISOString,mi,2);
    if (!isExtended && h==0) goto timeok;
    if (h!=2) return 0; // no ISO8601 time, must be 2 digits here
    aISOString+=h; n+=h;
    // - check separator in case of extended format
    if (isExtended) {
      if (*aISOString != ':') goto timeok; // no separator means hour:minute reduced precision for extended format
      aISOString++; n++;
    }
    // - next must be 2 digit second (or nothing for reduced precision without seconds)
    h = StrToUShort(aISOString,s,2);
    if (!isExtended && h==0) goto timeok; // no seconds is ok for basic format only (in extended format, the separator must be omitted as well, which is checked above)
    if (h!=2) return 0; // no ISO8601 time, must be 2 digits here
    aISOString+=h; n+=h;
    // optional fractions of seconds
    if (*aISOString=='.') {
      aISOString++; n++;
      h = StrToUShort(aISOString,ms,3);
      if (h==0) return 0; // invalid fraction specified
      aISOString+=h; n+=h;
      while (h<3) { ms *= 10; h++; } // make milliseconds
    }
  timeok:
    // add to timestamp
    aTimestamp += time2lineartime(hr,mi,s,ms);
    // check for zone specification
    h = ISO8601StrToContext(aISOString,aTimeContext);
    aISOString+=h; n+=h;
  }
  // return number of characters converted
  return n;
} // ISO8601StrToTimestamp




/// @brief convert ISO8601 zone offset to internal time zone
/// @return number of successfully converted characters or 0 if no valid ISO8601 time zone spec could be decoded
/// @param[in] aISOString input string in ISO8601 time zone offset format (or just "Z" for UTC)
/// @param[out] aTimeContext:
///             TCTX_UNKNOWN if no time zone specification is found
///             TCTX_UTC if "Z" specifier found
///             TCTX_OFFSCONTEXT(xx) if explicit UTC offset is found
sInt16 ISO8601StrToContext(cAppCharP aISOString, timecontext_t &aTimeContext)
{
  sInt16 n=0,h;
  sInt16 minoffs;
  uInt16 offs;

  aTimeContext = TCTX_UNKNOWN;
  bool western=false;
  // check for UTC special case
  if (*aISOString=='Z') {
    n++;
    aTimeContext = TCTX_UTC;
  }
  else {
    // check for time zone offset
    if (*aISOString!='+' && *aISOString!='-')
      return 0; // error, nothing converted
    western = *aISOString=='-'; // time is behind of UTC in the west
    aISOString++;
    n++;
    h=StrToUShort(aISOString,offs,2);
    if (h!=2)
      return 0; // not +HH format with 2 digits, nothing converted
    aISOString+=h; n+=h;
    // make minutes
    minoffs = offs*60;
    // hour offset ok, now check minutes
    if (*aISOString==':') { aISOString++; n++; }; // extended format
    // get minutes, if any
    h = StrToUShort(aISOString,offs,2);
    if (h==2) {
      // minute specified
      minoffs += offs; // add minutes
    }
    // adjust sign
    if (western)
      minoffs=-minoffs; // western offset is negative
    // return non-symbolic minute offset
    aTimeContext = TCTX_OFFSCONTEXT(minoffs);
  }
  // return number of characters converted
  return n;
} // ISO8601StrToContext



/// @brief convert timestamp to ISO8601 representation
/// @param[out] aISOString will receive ISO8601 formatted date/time
/// @param[in] aTimestamp representation of ISO time spec as is (no time zone conversions)
/// @param[in] aExtFormat if set, extended format is generated
/// @param[in] aWithFracSecs if set, fractional seconds are displayed (3 digits, milliseconds)
/// @param[in] aTimeContext:
///            TCTX_DURATION: timestamp is a duration and is shown in ISO8601 duration format (PnDTnHnMnS.MS)
///            TCTX_UNKNOWN : time is shown as relative format (no "Z", no explicit offset)
///            TCTX_TIMEONLY
///            TCTX_UTC     : time is shown with "Z" specifier
///            TCTX_DATEONLY: only date part is shown (of date or duration)
///            TCTX_OFFSCONTEXT(xx) : time is shown with explicit UTC offset (but not with "Z", even if offset is 0:00)
void TimestampToISO8601Str(string &aISOString, lineartime_t aTimestamp, timecontext_t aTimeContext, bool aExtFormat, bool aWithFracSecs)
{
  // check for duration (should be rendered even if value is 0)
  if (TCTX_IS_DURATION(aTimeContext)) {
    // render as duration
    // - sign
    if (aTimestamp<0) {
      aTimestamp=-aTimestamp;
      aISOString = "-";
    }
    else {
      aISOString.erase();
    }
    // - "P" duration designator
    aISOString += "P";
    // - days
    lineardate_t days = aTimestamp / linearDateToTimeFactor;
    if (days!=0) StringObjAppendPrintf(aISOString,"%ldD",(long)days);
    // - time part if needed
    if (!TCTX_IS_DATEONLY(aTimeContext) && lineartime2timeonly(aTimestamp)>(aWithFracSecs ? 0 : secondToLinearTimeFactor-1)) {
      aISOString += "T"; // we have a time part
      sInt16 h,m,s,ms;
      lineartime2time(aTimestamp,&h,&m,&s,&ms);
      if (h!=0) StringObjAppendPrintf(aISOString,"%hdH",h);
      if (m!=0) StringObjAppendPrintf(aISOString,"%hdM",m);
      if (aWithFracSecs && ms!=0) {
        // with milliseconds, implies seconds as well, even if they are zero
        StringObjAppendPrintf(aISOString,"%hd.%03hdS",s,ms);
      }
      else {
        // no milliseconds, show seconds if not zero
        if (s!=0) StringObjAppendPrintf(aISOString,"%hdS",s);
      }
    }
    else if(days==0) {
      aISOString="PT0S"; // no time part and no days, just display 0 seconds (and not negative, even if fraction might be)
    }
    // done
    return;
  }
  // no timestamp, no string
  if (aTimestamp==0) {
    aISOString.erase();
    return;
  }
  // Date if we want date
  bool hasDate=false;
  if (!TCTX_IS_TIMEONLY(aTimeContext)) {
    // we want the date part
    sInt16 y,m,d;
    lineartime2date(aTimestamp,&y,&m,&d);
    if (!TCTX_IS_DURATION(aTimeContext) || !(y==0 && m==0 && d==0)) {
      if (aExtFormat)
        StringObjPrintf(aISOString,"%04d-%02d-%02d",y,m,d); // Extended format
      else
        StringObjPrintf(aISOString,"%04d%02d%02d",y,m,d); // Basic format
      hasDate=true;
    }
  }
  else {
    aISOString.erase();
  }
  // Add time if we want time
  if (!TCTX_IS_DATEONLY(aTimeContext)) {
    // we want the time part
    // - add separator
    if (hasDate) aISOString+='T';
    // - now add the time
    sInt16 h,m,s,ms;
    lineartime2time(aTimestamp,&h,&m,&s,&ms);
    if (aExtFormat)
      StringObjAppendPrintf(aISOString,"%02hd:%02hd:%02hd",h,m,s); // Extended format
    else
      StringObjAppendPrintf(aISOString,"%02hd%02hd%02hd",h,m,s); // Basic format
    // - add fractions of the second if selected and not 0
    if (aWithFracSecs && (ms!=0)) {
      StringObjAppendPrintf(aISOString,".%03hd",ms); // 3 decimal fraction digits for milliseconds
    }
    if (!TCTX_IS_DURATION(aTimeContext)) {
      // add explicit time zone specification (or UTC "Z") if aTimecontext is a non-symbolic offset
      ContextToISO8601StrAppend(aISOString, aTimeContext, aExtFormat);
    }
  }
} // TimestampToISO8601Str



/// @brief append internal time zone as ISO8601 zone offset to string
/// @param[out] aISOString ISO8601 time zone spec will be appended to this string
/// @param[in] aTimeContext
/// @param[in] aExtFormat if set, extended format is generated
bool ContextToISO8601StrAppend(string &aISOString, timecontext_t aTimeContext, bool aExtFormat)
{
  // check for UTC special case
  if (TCTX_IS_UTC(aTimeContext)) {
    aISOString += 'Z';
    return true; // has time zone
  }
  // check if this is a resolved or a symbolic time zone
  if (TCTX_IS_TZ(aTimeContext))
    return false; // symbolic (includes unknown) - cannot append minute offset
  // offset specified, show it
  long moffs = TCTX_MINOFFSET(aTimeContext);
  bool minus = moffs<0;
  moffs = abs(moffs);
  long hoffs = moffs / MinsPerHour;
  moffs = moffs % MinsPerHour;
  StringObjAppendPrintf(aISOString, "%c%02ld", minus ? '-' : '+', hoffs);
  if (moffs!=0 || aExtFormat) {
    // minute specification required (always so for extended format)
    if (aExtFormat)
      aISOString+=':'; // add separator for extended format
    StringObjAppendPrintf(aISOString, "%02ld", moffs);
  }
  return true; // has time zone
} // ContextToISO8601StrAppend


} // namespace sysync

/* eof */
