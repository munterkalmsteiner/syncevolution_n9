/*
 *  File:         rrules.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Parser/Generator routines for vCalendar RRULES
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2004-11-23 : luz : created from exctracts from vcalendaritemtype.cpp
 *
 */
#include "prefix_file.h"

#include <stdarg.h>

#include "rrules.h"


#if defined(SYSYNC_TOOL)
  #include "syncappbase.h" // for CONSOLEPRINTF
#endif

using namespace sysync;

namespace sysync {


// Names of weekdays in RRULEs
const char* const RRULE_weekdays[ DaysOfWeek ] = {
  "SU",
  "MO",
  "TU",
  "WE",
  "TH",
  "FR",
  "SA"
};


// Support for SySync Diagnostic Tool
#ifdef SYSYNC_TOOL

// convert between RRULE and internal format
int rruleConv(int argc, const char *argv[])
{
  if (argc<0) {
    // help requested
    CONSOLEPRINTF(("  rrule <input mode> <output mode> <input> ..."));
    CONSOLEPRINTF(("    Convert between RRULE representations:"));
    CONSOLEPRINTF(("     mode \"i\" : internal format: startdate,freq,freqmod,interval,firstmask,lastmask,until"));
    CONSOLEPRINTF(("                (firstmask/lastmask in decimal or hex, until in ISO8601 format)"));
    CONSOLEPRINTF(("     mode \"t\" : output only, like \"i\" but also shows table of first 10 recurrence dates"));
    CONSOLEPRINTF(("     mode \"1\" : startdate,RRULE according to vCalendar 1.0 specs"));
    CONSOLEPRINTF(("     mode \"2\" : startdate,RRULE according to iCalendar (RFC2445) specs"));
    return EXIT_SUCCESS;
  }

  // check for argument
  if (argc!=3) {
    CONSOLEPRINTF(("3 arguments required"));
    return EXIT_FAILURE;
  }
  // mode
  char inmode,outmode;
  // internal representation
  lineartime_t start;
  char freq;
  char freqmod;
  sInt16 interval;
  fieldinteger_t firstmask;
  fieldinteger_t lastmask;
  lineartime_t until;
  // time zone stuff
  timecontext_t rulecontext=TCTX_UNKNOWN;
  timecontext_t untilcontext;
  GZones zones;
  // RRULE
  char isodate[50];
  char rulebuf[200];
  string rrule,iso;
  // get input mode
  inmode=tolower(*(argv[0]));
  if (inmode!='i' && inmode!='1' && inmode!='2') {
    CONSOLEPRINTF(("invalid input mode, must be i, 1 or 2"));
    return EXIT_FAILURE;
  }
  // get output mode
  outmode=tolower(*(argv[1]));
  if (outmode!='i' && outmode!='t' && outmode!='1' && outmode!='2') {
    CONSOLEPRINTF(("invalid output mode, must be i, t, 1 or 2"));
    return EXIT_FAILURE;
  }
  CONSOLEPRINTF((""));
  // reset params
  freq='0'; // frequency = none
  freqmod=' '; // frequency modifier
  interval=0; // interval
  firstmask=0; // day mask counted from the first day of the period
  lastmask=0; // day mask counted from the last day of the period
  until= noLinearTime; // last day
  // get input and convert to internal format (if needed)
  if (inmode=='i') {
    if (sscanf(argv[2],
      "%[^,],%c,%c,%hd,%lld,%lld,%s",
      rulebuf,
      &freq,
      &freqmod,
      &interval,
      &firstmask,
      &lastmask,
      isodate
    )!=7) {
      CONSOLEPRINTF(("invalid internal format input"));
      return EXIT_FAILURE;
    }
    // - convert dates
    ISO8601StrToTimestamp(rulebuf,start,rulecontext); // Start determines rule context
    ISO8601StrToTimestamp(isodate,until,untilcontext); // until will be converted to same context
    TzConvertTimestamp(until,untilcontext,rulecontext,&zones);
  }
  else if (inmode=='1') {
    if (sscanf(argv[2],"%[^,],%[^\n]",isodate,rulebuf)!=2) {
      CONSOLEPRINTF(("error, expected: startdate,RRULE"));
      return EXIT_FAILURE;
    }
    ISO8601StrToTimestamp(isodate,start,rulecontext);
    if (!RRULE1toInternal(
      rulebuf, // RRULE string to be parsed
      start, // reference date for parsing RRULE
      rulecontext, // time context of RRULE
      freq,
      freqmod,
      interval,
      firstmask,
      lastmask,
      until,
      untilcontext,
      NULL
    )) {
      CONSOLEPRINTF(("invalid/unsupported RRULE type 1 specification"));
      return EXIT_FAILURE;
    }
  }
  else if (inmode=='2') {
    if (sscanf(argv[2],"%[^,],%[^\n]",isodate,rulebuf)!=2) {
      CONSOLEPRINTF(("error, expected: startdate,RRULE"));
      return EXIT_FAILURE;
    }
    ISO8601StrToTimestamp(isodate,start,rulecontext);
    if (!RRULE2toInternal(
      rulebuf, // RRULE string to be parsed
      start, // reference date for parsing RRULE
      rulecontext, // time context of RRULE
      freq,
      freqmod,
      interval,
      firstmask,
      lastmask,
      until,
      untilcontext,
      NULL
    )) {
      CONSOLEPRINTF(("invalid/unsupported RRULE type 2 specification"));
      return EXIT_FAILURE;
    }
  }
  // convert to rrule (if needed) and output
  TimestampToISO8601Str(iso,start,rulecontext,true);
  if (outmode=='i' || outmode=='t') {
    TimestampToISO8601Str(rrule,until,TCTX_UTC,true);
    CONSOLEPRINTF((
      "Internal format (start,freq,freqmod,interval,firstmask,lastmask,until:\n%s,%c,%c,%hd,0x%llX,0x%llX,%s",
      iso.c_str(),
      freq,
      freqmod,
      interval,
      firstmask,
      lastmask,
      rrule.c_str()
    ));
    // extra info if we have 't' table mode
    lineartime_t occurrence;
    if (outmode=='t') {
      sInt16 cnt;
      for (cnt=1; cnt<=10; cnt++) {
        // calculate date of cnt-th recurrence
        if (!endDateFromCount(
          occurrence,
          start,
          freq, freqmod,
          interval,
          firstmask, lastmask,
          cnt, true, // counting occurrences
          NULL
        ))
          break;
        // see if limit already reached
        if (occurrence>until) {
          cnt=0;
          break;
        }
        // show this recurrence
        TimestampToISO8601Str(iso,occurrence,rulecontext,true);
        CONSOLEPRINTF((
          "%3d. occurrence at %s (%s)",
          cnt,
          iso.c_str(),
          RRULE_weekdays[lineartime2weekday(occurrence)]
        ));
      }
      // show end date if not all recurrences shown already
      if (cnt>10) {
        // calculate occurrence count
        if (countFromEndDate(
          cnt, true, // counting occurrences
          start,
          freq, freqmod,
          interval,
          firstmask, lastmask,
          until,
          NULL
        )) {
          if (cnt==0) {
            CONSOLEPRINTF((
              "last occurrence: <none> (repeating infinitely)",
              cnt,
              iso.c_str()
            ));
          }
          else {
            // convert back to date
            if (endDateFromCount(
              occurrence,
              start,
              freq, freqmod,
              interval,
              firstmask, lastmask,
              cnt, true, // counting occurrences
              NULL
            )) {
              TimestampToISO8601Str(iso,occurrence,rulecontext,true);
              CONSOLEPRINTF((
                "%3d./LAST occ.  at %s (%s)",
                cnt,
                iso.c_str(),
                RRULE_weekdays[lineartime2weekday(occurrence)]
              ));
            }
          }
        }
      }
    }
  }
  else if (outmode=='1') {
    if (!internalToRRULE1(
      rrule, // receives RRULE string
      freq,
      freqmod,
      interval,
      firstmask,
      lastmask,
      until,
      rulecontext,
      NULL
    )) {
      CONSOLEPRINTF(("Cannot show as RRULE type 1"));
      return EXIT_FAILURE;
    }
    CONSOLEPRINTF(("RRULE type 1 format (start,rrule):\n%s,%s",iso.c_str(),rrule.c_str()));
  }
  else if (outmode=='2') {
    if (!internalToRRULE2(
      rrule, // receives RRULE string
      freq,
      freqmod,
      interval,
      firstmask,
      lastmask,
      until,
      rulecontext,
      NULL
    )) {
      CONSOLEPRINTF(("Cannot show as RRULE type 2"));
      return EXIT_FAILURE;
    }
    CONSOLEPRINTF(("RRULE type 2 format (start,rrule):\n%s,%s",iso.c_str(),rrule.c_str()));
  }
  return EXIT_SUCCESS;
} // rruleConv

#endif // SYSYNC_TOOL



/*
  // sample of a field block:

  Offset=0: <field name="RR_FREQ" type="string" compare="conflict"/>
  - frequency codes:
    0 = none,
    s = secondly,
    m = minutely,
    h = hourly,
    D = daily,
    W = weekly,
    M = monthly,
    Y = yearly
  - frequency modifiers
    space = none
    s = by second list
    m = by minute list
    h = by hour list
    W = by weekday list
    D = by monthday list
    Y = by yearday list
    N = by weeknumber list
    M = by monthlist
    P = by setposlist

  Offset=1: <field name="RR_INTERVAL" type="integer" compare="conflict"/>
  - interval

  Offset=2: <field name="RR_FMASK" type="integer" compare="conflict"/>
  - bit-coded list of items according to frequency modifier,
    relative to the beginning of the interval
    - for seconds, minutes, hours: Bit0=first, Bit1=second....
    - for weekly: Bit0=Sun, Bit6=Sat
    - for monthly by weekday: Bit0=first Sun, Bit7=2nd Sun... Bit35=5th Sun
    - for monthly by day: Bit0=1st, Bit1=2nd, Bit31=31st.
    - for yearly by month: Bit0=jan, Bit1=feb,...
    - for yearly by weeknumber: Bit0=weekno1,....

  Offset=3: <field name="RR_LMASK" type="integer" compare="conflict"/>
  - bit coded list of items according to frequency modifier,
    relative to the end of the interval:
    - Bit0=last, Bit1=second last, Bit2=third last....
    - for weekly: Bit0=last Sun, Bit1=last Mon,... Bit7=second last Sun,...

  Offset=4: <field name="RR_END" type="timestamp" compare="conflict"/>
  - end date. This is calculated from DTSTART and count if incoming rule does
    not specify an end date

  Offset=5: <field name="DTSTART" type="timestamp" compare="always"/>
  - for reference only

*/

// Converts internal recurrence into vCalendar 1.0 RRULE string
bool internalToRRULE1(
  string &aString, // receives RRULE string
  char freq,
  char freqmod,
  sInt16 interval,
  fieldinteger_t firstmask,
  fieldinteger_t lastmask,
  lineartime_t until,
  timecontext_t untilcontext,
  TDebugLogger *aLogP
)
{
  // Now do the conversion
  string s;
  sInt16 i,j,k;
  fieldinteger_t m;
  bool repshown;
  aString.erase();
  // frequency and modifier
  switch (freq) {
    case '0' : return true; // no repetition
    case 'D' : aString+='D'; break;
    case 'W' : aString+='W'; break;
    case 'M' :
      aString+='M';
      if (freqmod=='W') aString+='P'; // by-(weekday)-position
      else if (freqmod=='D') aString+='D'; // by (month)day
      else goto incompat;
      break;
    case 'Y' :
      aString+='Y';
      if (freqmod=='M') aString+='M'; // by month
      // else if (freqmod=='Y') aString+='D'; // by yearday, %%%% not supported
      else goto incompat;
      break;
    default :
      goto incompat;
  } // switch freq
  // add interval
  if (interval<1) goto incompat;
  StringObjAppendPrintf(aString,"%hd",interval);
  // add modifiers
  switch (freqmod) {
    case 'W' :
      if (freq=='M') {
        // Monthly by weekday
        m = firstmask;
        for (i=0; i<2; i++) {
          // - from start and from end
          for (j=0; j<WeeksOfMonth; j++) {
            // - repetition
            repshown=false;
            // - append weekdays for this repetition
            for (k=0; k<DaysOfWeek; k++) {
              if (m & ((uInt64)1<<(k+(DaysOfWeek*j)))) {
                // show repetion before first day item
                if (!repshown) {
                  repshown=true;
                  StringObjAppendPrintf(aString," %d",j+1);
                  // - show if relative to beginning or end of month
                  if (i>0) aString+='-'; else aString+='+';
                }
                // show day
                aString+=' '; aString+=RRULE_weekdays[k];
              }
            }
          }
          // - switch to those that are relative to the end of the month
          m = lastmask;
        }
      }
      else {
        // weekly by weekday
        for (k=0; k<DaysOfWeek; k++) {
          if (firstmask & ((uInt64)1<<k)) { aString+=' '; aString+=RRULE_weekdays[k]; }
        }
      }
      break;
    case 'D' :
    case 'M' :
      // monthly by day number or
      // yearly by month number
      m = firstmask;
      for (i=0; i<2; i++) {
        // - show day numbers
        for (k=0; k<32; k++) {
          if (m & ((uInt64)1<<k)) {
            StringObjAppendPrintf(aString," %d",k+1);
            // show if relative to the end
            if (i>0) aString+='-';
          }
        }
        // - switch to those that are relative to the end of the month / year
        m = lastmask;
      }
      break;
    case ' ' :
      // no modifiers, that's ok as well
      break;
    default :
      goto incompat;
  } // switch freqmod
  // add end date (or #0 if no end date = endless)
  if (until!=noLinearTime) {
    // there is an end date, use it
    aString+=' ';
    TimestampToISO8601Str(s, until, untilcontext);
    aString+=s;
  }
  else {
    // there is no end date, repeat forever
    aString+=" #0";
  }
  // genereated a RRULE
  return true;
incompat:
  // incompatible, cannot be shown as vCal 1.0 RRULE
  aString.erase();
  return false; // no value generated
} // internalToRRULE1


// Converts internal recurrence into vCalendar 2.0 RRULE string
bool internalToRRULE2(
  string &aString, // receives RRULE string
  char freq,
  char freqmod,
  sInt16 interval,
  fieldinteger_t firstmask,
  fieldinteger_t lastmask,
  lineartime_t until,
  timecontext_t untilcontext,
  TDebugLogger *aLogP
)
{
  LOGDEBUGPRINTFX(aLogP,DBG_EXOTIC+DBG_GEN,(
    "InternalToRRULE2(): expanding freq=%c, freqmod=%c, interval=%hd, firstmask=%llX, lastmask=%llX",
    freq, freqmod, interval, (long long)firstmask, (long long)lastmask
  ));

  // Now do the conversion
  string s;
  sInt16 i,j,k;
  fieldinteger_t m;
  bool repshown;
  cAppCharP sep;
  aString.erase();
  // frequency and modifier
  switch (freq) {
    case '0' : return true; // no repetition
    case 'D' : aString+="FREQ=DAILY"; break;
    case 'W' : aString+="FREQ=WEEKLY"; break;
    case 'M' : aString+="FREQ=MONTHLY"; break;
    case 'Y' : aString+="FREQ=YEARLY"; break;
    default :
      goto incompat;
  } // switch freq
  // add interval
  if (interval<1) goto incompat;
  StringObjAppendPrintf(aString,";INTERVAL=%hd",interval);
  // add modifiers
  switch (freqmod) {
    case 'W' :
      sep=";BYDAY=";
      if (freq=='M') {
        // Monthly by weekday
        m = firstmask;
        for (i=0; i<2; i++) {
          // - from start and from end
          for (j=0; j<WeeksOfMonth; j++) {
            // - repetition
            repshown=false;
            // - append weekdays for this repetition
            for (k=0; k<DaysOfWeek; k++) {
              if (m & ((uInt64)1<<(k+(DaysOfWeek*j)))) {
                // show repetion before first day item
                aString+=sep;
                sep=",";
                if (!repshown) {
                  repshown=true;
                  if (i>0) aString+='-';
                  StringObjAppendPrintf(aString,"%d",j+1);
                  // - show if relative to beginning or end of month
                }
                // show day
                aString+=RRULE_weekdays[k];
              }
            }
          }
          // - switch to those that are relative to the end of the month
          m = lastmask;
        }
      }
      else {
        // weekly by weekday
        for (k=0; k<DaysOfWeek; k++) {
          if (firstmask & ((uInt64)1<<k)) {
            aString+=sep;
            sep=",";
            aString+=RRULE_weekdays[k];
          }
        }
      }
      break;
    case 'D' :
      // monthly by day number
      appendMaskAsNumbers(";BYMONTHDAY=", aString, firstmask, lastmask);
      break;
    case 'Y' :
      // yearday by day number
      appendMaskAsNumbers(";BYYEARDAY=", aString, firstmask, lastmask);
      break;
    case 'N' :
      // yearly by week number
      appendMaskAsNumbers(";BYWEEKNO=", aString, firstmask, lastmask);
      break;
    case 'M' :
      // yearly by month number
      appendMaskAsNumbers(";BYMONTH=", aString, firstmask, lastmask);
      break;
    case ' ' :
      // no modifiers, that's ok as well
      break;
    default :
      goto incompat;
  } // switch freqmod
  // add end date
  // Note: for correct iCalendar 2.0, this should be (and is, in mimedir) called with untilcontext=UTC unless it's date-only
  if (until!=noLinearTime) {
    // there is an end date, use it
    aString+=";UNTIL=";
    TimestampToISO8601Str(s, until, untilcontext);
    aString+=s;
  }

  // do output
  LOGDEBUGPRINTFX(aLogP,DBG_GEN,("generated rrule %s", aString.c_str()));

  // genereated a RRULE
  return true;
incompat:
  // incompatible, cannot be shown as vCal 1.0 RRULE
  aString.erase();
  return false; // no value generated
} // internalToRRULE2


// Converts  vCalendar 1.0 RRULE string into internal recurrence representation
bool RRULE1toInternal(
  const char *aText, // RRULE string to be parsed
  lineartime_t dtstart, // reference date for parsing RRULE
  timecontext_t startcontext, // context of reference date
  char &freq,
  char &freqmod,
  sInt16 &interval,
  fieldinteger_t &firstmask,
  fieldinteger_t &lastmask,
  lineartime_t &until,
  timecontext_t &untilcontext,
  TDebugLogger *aLogP
)
{
  string s;
  const char *p;
  sInt16 startwday;
  sInt16 startyear,startmonth,startday;
  bool fromstart;
  sInt16 cnt;

  // get elements of start point
  startwday=lineartime2weekday(dtstart); // get starting weekday
  lineartime2date(dtstart,&startyear,&startmonth,&startday); // year, month, day-in-month
  // do the conversion here
  p=aText;
  char c,c2;
  sInt16 j,k;
  fieldinteger_t m;
  // get frequency and modifier
  do c=*p++; while(c==' '); // next non-space
  if (c==0) goto incompat; // no frequency: incompatible -> parse error
  switch (c) {
    case 'D' : freq='D'; break;
    case 'W' : freq='W'; freqmod='W'; break;
    case 'M' :
      freq='M';
      // get modifier
      c=*p++;
      if (c=='P') freqmod='W'; // by weekday
      else if (c=='D') freqmod='D'; // by monthday
      else goto norep; // unknown modifier: cancel repetition (but no error)
      break;
    case 'Y' :
      freq='Y';
      // get modifier
      c=*p++;
      if (c=='M') freqmod='M'; // by monthlist
      else if (c=='D') freqmod='Y'; // by day-of-year list %%% not supported
      else goto norep; // unknown modifier: cancel repetition (but no error)
      break;
    default :
      goto incompat;
  }
  // get interval
  while (isdigit(*p)) { interval=interval*10+((*p++)-'0'); }
  if (interval==0) goto incompat; // no interval is incompatible -> parse error
  // get modifier(s) if any
  switch (freq) {
    case 'W' :
      // weekly may or may not have modifiers
      do {
        do c=*p++; while(c==' '); // next non-space
        if (isalpha(c)) {
          c2=*p++;
          if (!isalpha(c2)) goto incompat; // bad weekday syntax -> parse error
          for (k=0;k<DaysOfWeek;k++) if (toupper(c)==RRULE_weekdays[k][0] && toupper(c2)==RRULE_weekdays[k][1]) break;
          if (k==DaysOfWeek) goto incompat; // bad weekday -> parse error
          firstmask = firstmask | ((uInt64)1<<k); // add weekday
        }
        else {
          p--;
          break;
        }
      } while(true);
      // make sure start day is always in mask
      if (dtstart) firstmask |= ((uInt64)1<<startwday);
      break;
    case 'M' :
      fromstart=true; // default to specification relative to start
      if (freqmod=='W') {
        // monthly by weekday (position)
        j=0; // default to first occurrence until otherwise set
        do {
          do c=*p++; while(c==' '); // next non-space
          if (isdigit(c) && !isdigit(*p)) { // only single digit, otherwise it might be end date
            // get occurrence specs
            if (c>'5' || c<'1') goto incompat; // no more than 5 weeks in a month! -> parse error
            j=c-'1'; // first occurrence=0
            // check for '+' or '-'
            if (*p=='+') p++; // simply skip
            else if (!(fromstart=!(*p=='-'))) p++; // skip
          }
          else if (isalpha(c)) {
            // get weekday spec
            c2=*p++;
            if (!isalpha(c2)) goto incompat; // bad weekday syntax -> parse error
            for (k=0;k<DaysOfWeek;k++) if (toupper(c)==RRULE_weekdays[k][0] && toupper(c2)==RRULE_weekdays[k][1]) break;
            if (k==DaysOfWeek) goto incompat; // bad weekday -> parse error
            m=(uInt64)1<<(DaysOfWeek*j+k);
            if (fromstart) firstmask |= m; else lastmask |= m; // add weekday rep
          }
          else {
            // end of modifiers
            p--;
            break;
          }
        } while(true);
        // check if we need defaults
        if (!(firstmask | lastmask)) {
          if (!dtstart) goto norep; // cannot set, no start date (but no error)
          // determine the repetition in the month of this weekday
          j=(startday-1) / DaysOfWeek;
          firstmask = ((uInt64)1<<(DaysOfWeek*j+startwday)); // set nth repetition of current weekday
        }
      }
      else {
        // must be 'D' = monthly by day of month
        do {
          fromstart=true;
          do c=*p++; while(c==' '); // next non-space
          if (c=='L' && *p=='D') {
            // special case: "LD" means last day of month
            lastmask |= 1;
          }
          else if (isdigit(c) && (*p) && (!isdigit(*p) || !isdigit(*(p+1)))) { // more than two digits are end date
            // get day number
            k=c-'0';
            if (isdigit(*p)) k=k*10+((*p++)-'0');
            // check for '+' or '-'
            if (*p=='+') p++; // simply skip plus
            if (!(fromstart=!(*p=='-'))) p++; // skip minus and set fromstart to false
            if (k==0) goto incompat; // 0 is not allowed  -> parse error
            // set mask
            k--; // bits are 0 based
            m = (uInt64)1<<k;
            if (fromstart) firstmask |= m; else lastmask |= m; // add day
          }
          else {
            p--;
            break;
          }
        } while(true);
        // check if we need defaults
        if (!(firstmask | lastmask)) {
          if (!dtstart) goto norep; // cannot set, no start date (no error)
          // set current day number
          firstmask = ((uInt64)1<<(startday-1)); // set bit of current day-in-month
        }
      } // by day of month
      // monthly modifiers done
      break;
    case 'Y' :
      if (freqmod=='M') {
        // by monthlist
        do {
          do c=*p++; while (c==' '); // next non-space
          if (isdigit(c) && (*p) && (!isdigit(*p) || !isdigit(*(p+1)))) { // more than two digits are end date
            // get month number
            k=c-'0';
            if (isdigit(*p)) k=k*10+((*p++)-'0');
            if (k==0) goto incompat; // 0 is not allowed -> parse error
            // set mask
            k--; // bits are 0 based
            firstmask |= (uInt64)1<<k; // add month
          }
          else {
            p--;
            break;
          }
        } while(true);
        // check if we need defaults
        if (!firstmask) {
          if (!dtstart) goto norep; // cannot set, no start date (but no error)
          // set current month number
          firstmask = ((uInt64)1<<(startmonth-1)); // set bit of current day-in-month
        }
      }
      else {
        // by day of year, only supported if no list follows
        // - check if list follows
        do c=*p++; while (c==' '); // next non-space
        if (isdigit(c)) {
          if (*p) {
            if (!isdigit(*p)) goto norep; // single digit, is list, abort (but no error)
            if (*(p+1)) {
              if (!isdigit(*(p+1))) goto norep; // dual digit, is list, abort (but no error)
              if (*(p+2)) {
                if (!isdigit(*(p+2))) goto norep; // three digit, is list, abort (but no error)
              }
            }
          }
        }
        // if we get so far, it's either end date (>= 4 digits) or # or end of string
        --p; // we have fetched one to many
        // - make best try to convert to by-monthlist
        freqmod='M';
        firstmask = ((uInt64)1<<(startmonth-1)); // set bit of current day-in-month
      }
      // yearly modifiers done
      break;
  } // switch for modifier reading
  // get count or end date
  do c=*p++; while (c==' '); // next non-space
  cnt=2; // default to repeat once (=applied two times)
  if (c==0) goto calcenddate;
  else if (c=='#') {
    // count specified
    if (!StrToShort(p,cnt)) goto incompat; // bad count -> parse error
  calcenddate:
    if (!endDateFromCount(until,dtstart,freq,freqmod,interval,firstmask,lastmask,cnt,false,aLogP))
      goto norep;
    untilcontext = startcontext; // until is in same context as start
  } // count specified
  else {
    // must be end date, or spec is bad
    p--;
    if (ISO8601StrToTimestamp(p, until, untilcontext)==0)
      goto incompat; // bad end date -> parse error
  } // end date specified
  // parsed ok, now store it
  goto store;
norep:
  // no repetition (but no parse error generated)
  freq='0'; // frequency = none
  freqmod=' '; // frequency modifier
  interval=0; // interval
  firstmask=0; // day mask counted from the first day of the period
  lastmask=0; // day mask counted from the last day of the period
  until= noLinearTime; // last day
store:
  return true; // ok
incompat:
  // incompatible, value cannot be parsed usefully
  return false; // no value generated
} // RRULE1toInternal


/// @brief calculate end date of RRULE when count is specified
/// @return true if repeating, false if not repeating at all
/// @note returns until=noLinearTime for endless repeat (count=0)
bool endDateFromCount(
  lineartime_t &until,
  lineartime_t dtstart,
  char freq, char freqmod,
  sInt16 interval,
  fieldinteger_t firstmask,fieldinteger_t lastmask,
  sInt16 cnt, bool countsoccurrences,
  TDebugLogger *aLogP
)
{
  // count<=0 means endless
  if (cnt<=0) {
    until= noLinearTime; // forever, we don't need a start date for this
    return true; // ok
  }
  // check no-rep cases
  if (cnt<0) return false; // negative count, no repeat
  if (dtstart==noLinearTime) return false; // no start date, cannot calc end date -> no rep (but no error)
  if (interval<=0) return false; // interval=0 means no recurrence (but no error)
  // default to dtstart
  until = dtstart;
  // calculate elements of start point
  sInt16 startwday;
  sInt16 startyear,startmonth,startday;
  lineartime_t starttime;
  starttime = lineartime2timeonly(dtstart); // start time of day
  startwday = lineartime2weekday(dtstart); // get starting weekday
  lineartime2date(dtstart,&startyear,&startmonth,&startday); // year, month, day-in-month
  // calculate interval repetitions (which is what is needed for daily and RRULE v1 calculation)
  sInt16 ivrep = (cnt-1)*interval;
  // check if daily
  if (freq == 'D') {
    // Daily recurrence is same for occurrence and interval counts
    LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("endDateFromCount: daily calc - same in all cases"));
    until = dtstart+(ivrep*linearDateToTimeFactor);
    return true;
  }
  else if (!countsoccurrences) {
    // RRULE v1 interpretation of count (=number of repetitions of interval, not number of occurrences)
    // - determine end of recurrence interval (which is usually NOT a occurrence precisely)
    switch (freq)
    {
      case 'W':
        LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("endDateFromCount: simple weekly calc"));
        // v1 end date calc, we need to take into account possible masks, so result must be end of interval, not just start date+interval
        // weekly: end date is last day of target week
        until=dtstart+((ivrep*DaysOfWeek-startwday+6)*linearDateToTimeFactor);
        return true;
      case 'M':
        LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("endDateFromCount: simple monthly calc"));
        startmonth--; // make 0 based
        startmonth += ivrep+1; // add number of months plus one (as we want next month, and then go one day back to last day of month)
        startyear += startmonth / 12; // update years
        startmonth = startmonth % 12 + 1; // update month and make 1 based again
        // - calculate last day in month of occurrence
        until = (date2lineardate(startyear,startmonth,1)-1)*linearDateToTimeFactor+starttime;
        return true;
      case 'Y':
        LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("endDateFromCount: simple yearly calc"));
        // yearly: end date is end of end year
        until = (date2lineardate(startyear+ivrep,12,31))*linearDateToTimeFactor+starttime;
        return true;
    }
  }
  else {
    // v2 type counting, means that count specifies number of occurrences, not interval repetitions
    // requires more elaborate expansion
    // NOTE: this does not work without masks set, so we need to calculate the default masks if none are explicitly set
    // - we need the number of days in the month in most cases
    sInt16 lastday = getMonthDays(lineartime2dateonly(dtstart)); // number of days in this month
    sInt16 newYearsPassed;
    switch (freq)
    {
      case 'W':
        LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("endDateFromCount: full expansion weekly calc"));
        // - make sure we have a mask
        if (firstmask==0 && lastmask==0)
          firstmask = 1<<startwday; // set start day in mask
        // now calculate
        while (firstmask) {
          if (firstmask & ((uInt64)1<<startwday)) {
            // found an occurrence
            cnt--; // count it
            if (cnt<=0) break; // found all
          }
          // increment day
          until+=linearDateToTimeFactor;
          startwday++;
          if (startwday>6) {
            // new week starts
            startwday=0;
            // skip part of interval which has no occurrence
            until+=(interval-1)*7*linearDateToTimeFactor;
          }
        }
        return true;
      case 'M':
        if (freqmod=='W') {
          // monthly by weekday
          LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("endDateFromCount: full expansion of monthly by weekday"));
          // - make sure we have a mask
          if (firstmask==0 && lastmask==0)
            firstmask = (uInt64)1<<(startwday+7*((startday-1)/7)); // set start day in mask
          // - now calculate
          while (firstmask || lastmask) {
            // calculate which weeks we are in
            sInt16 fwk=(startday-1) / 7; // start is nth week of the month
            sInt16 lwk=(lastday-startday) / 7; // start is nth-last week of the month
            if (
              (firstmask & ((uInt64)1<<(startwday+7*fwk))) || // nth occurrence of weekday in month
              (lastmask & ((uInt64)1<<(startwday+7*lwk))) // nth-last occurrence of weekday in month
            ) {
              // found an occurrence
              cnt--; // count it
              if (cnt<=0) break; // found all
            }
            // increment day
            until+=linearDateToTimeFactor;
            startday++; // next day in month
            startwday++; if (startwday>6) startwday=0; // next day in the week
            // check for new month
            if (startday>lastday) {
              // new month starts
              sInt16 i=interval;
              while (true) {
                lastday = getMonthDays(lineartime2dateonly(until)); // number of days in next month
                startday = 1; // start at 1st of month again
                if (--i == 0) break; // done
                // skip entire next month
                until+=lastday*linearDateToTimeFactor; // advance by number of days in this month
              }
              // now recalculate weekday
              startwday=lineartime2weekday(until); // calculation continues here
            }
          }
        }
        else {
          // everything else, including no modifier, is treated as monthly by monthday
          LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("endDateFromCount: full expansion of monthly by monthday"));
          // - make sure we have a mask
          if (firstmask==0 && lastmask==0)
            firstmask = (uInt64)1<<(startday-1); // set start day in mask
          // - now calculate
          while (firstmask || lastmask) {
            if (
              (firstmask & ((uInt64)1<<(startday-1))) || // nth day in month
              (lastmask & ((uInt64)1<<(lastday-startday))) // nth-last day in month
            ) {
              // found an occurrence
              cnt--; // count it
              if (cnt<=0) break; // found all
            }
            // increment day
            until+=linearDateToTimeFactor;
            startday++; // next day in month
            if (startday>lastday) {
              // new month starts
              sInt16 i=interval;
              while (true) {
                lastday = getMonthDays(lineartime2dateonly(until)); // number of days in next month
                startday = 1; // start at 1st of month again
                if (--i == 0) break; // done
                // skip entire next month
                until+=lastday*linearDateToTimeFactor; // advance by number of days in this month
              }
            }
          }
        }
        return true;
      case 'Y':
        if (freqmod=='M') {
          // Yearly by month
          LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("endDateFromCount: full expansion of yearly by month"));
          // - make sure we have a mask
          if (firstmask==0 && lastmask==0)
            firstmask = (uInt64)1<<(startmonth-1); // set start month in mask
          // - do entire calculation on 1st of month such that we can be sure that day exists (unlike a Feb 30th or April 31th)
          until -= (startday-1)*linearDateToTimeFactor;
          // - now calculate
          newYearsPassed=0;
          // occurrence interval can be at most 4 years in the future (safety abort)
          while (newYearsPassed<=4) {
            if (firstmask & ((uInt64)1<<(startmonth-1))) {
              // possibly found an occurrence
              // - is an occurrence only if that day exists in the month
              if (startday<=lastday) {
                cnt--; // count it
                newYearsPassed = 0;
                if (cnt<=0) break; // found all
              }
            }
            // go to same day in next month (and skip months that don't have that day, like an 31st April or 30Feb
            until+=lastday*linearDateToTimeFactor;
            startmonth++;
            if (startmonth>12) {
              // new year starts
              startmonth=1;
              newYearsPassed++;
              // skip additional years (in month steps)
              for (sInt16 i=(interval-1)*12; i>0; i--) {
                lastday = getMonthDays(lineartime2dateonly(until)); // number of days in next month
                // skip month
                until+=lastday*linearDateToTimeFactor; // advance by number of days in this month
              }
            }
            // get size of next month to check
            lastday = getMonthDays(lineartime2dateonly(until)); // number of days in next month
          }
          // move back to start day
          until += (startday-1)*linearDateToTimeFactor;
        }
        else {
          // everything else, including no modifier, is treated as yearly on the same date (multiple occurrences per year not supported)
          LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("endDateFromCount: full expansion of yearly by yearday - NOT SUPPORTED with more than one day"));
          until = (date2lineardate(startyear+ivrep,startmonth,startday))*linearDateToTimeFactor+starttime;
        }
        return true;
    } // switch
  }
  // no recurrence
  return false;
} // endDateFromCount



#ifdef DEBUG
/*
// %%%% hack debug
#define SYNTHESIS_UNIT_TEST 1
#define UNIT_TEST_TITLE(a)
#define UNIT_TEST_CALL(x,p,t,v)
*/
#endif

#ifdef SYNTHESIS_UNIT_TEST

// helper to create lineartime parameters
static lineartime_t t(char *aTime)
{
  if (!aTime || *aTime==0)
    return noLinearTime;
  timecontext_t tctx;
  lineartime_t res;
  ISO8601StrToTimestamp(aTime, res, tctx);
  return res;
}

// helper to show lineartime results
static char buf[100];
char *s(lineartime_t aTime)
{
  if (aTime==noLinearTime)
    return "<none>";
  string str;
  TimestampToISO8601Str(str, aTime, TCTX_UNKNOWN, true, false);
  strcpy(buf,str.c_str());
  return buf;
}


// RRULE expansion tests
bool test_expand_rrule(void)
{
  bool ok=true;
  lineartime_t lt;
  TRRuleExpandStatus es;

  {
    UNIT_TEST_TITLE("Birthday 1");
    UNIT_TEST_CALL(initRRuleExpansion(es,t("2008-03-31T00:00:00"),'Y','M',1,0x4,0x0,t("2009-03-31T00:00:00"),t("2009-04-01T00:00:00")),("-none-"),true,ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2009-03-31T00:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t(""),ok); // generated occurrences (checked 3)

    UNIT_TEST_TITLE("Birthday 2");
    UNIT_TEST_CALL(initRRuleExpansion(es,t("1979-03-06T00:00:00"),'Y','M',1,0x4,0x0,t("2009-03-31T00:00:00"),t("2009-04-01T00:00:00")),("-none-"),true,ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t(""),ok); // no occurrences (checked 32)

    UNIT_TEST_TITLE("Birthday 3");
    UNIT_TEST_CALL(initRRuleExpansion(es,t("2006-03-13T00:00:00"),'Y','M',1,0x4,0x0,t("2009-02-23T00:00:00"),t("2009-03-31T00:00:00")),("-none-"),true,ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2009-03-13T00:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t(""),ok); // generated occurrences (checked 5)

    UNIT_TEST_TITLE("Pay day");
    UNIT_TEST_CALL(initRRuleExpansion(es,t("2006-06-20T00:00:00"),'M','D',1,0x80000,0x0,t("2009-02-23T00:00:00"),t("2009-03-31T00:00:00")),("-none-"),true,ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2009-03-20T00:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t(""),ok); // generated occurrences (checked 35)

    UNIT_TEST_TITLE("St. Patrick's Day");
    UNIT_TEST_CALL(initRRuleExpansion(es,t("2006-03-17T00:00:00"),'Y','M',1,0x4,0x0,t("2009-02-23T00:00:00"),t("2009-03-31T00:00:00")),("-none-"),true,ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2009-03-17T00:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t(""),ok); // generated occurrences (checked 5)

    // Expanding 'Change fish tank filter':
    UNIT_TEST_TITLE("Change fish tank filter");
    UNIT_TEST_CALL(initRRuleExpansion(es,t("2007-10-21T00:00:00"),'W','W',5,0x1,0x0,t("2009-02-23T00:00:00"),t("2009-03-31T00:00:00")),("-none-"),true,ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2009-03-29T00:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t(""),ok); // generated occurrences (checked 17)

    // Expanding 'Start of British Summer Time':
    UNIT_TEST_TITLE("Start of British Summer Time");
    UNIT_TEST_CALL(initRRuleExpansion(es,t("2003-03-30T00:00:00"),'M','W',12,0x0,0x1,t("2009-02-23T00:00:00"),t("2009-03-31T00:00:00")),("-none-"),true,ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2009-03-29T00:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t(""),ok); // generated occurrences (checked 8)

    UNIT_TEST_TITLE("Birthday 4");
    UNIT_TEST_CALL(initRRuleExpansion(es,t("1957-03-14T00:00:00"),'Y','M',1,0x4,0x0,t("2009-02-23T00:00:00"),t("2009-03-31T00:00:00")),("-none-"),true,ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2009-03-14T00:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t(""),ok); // generated occurrences (checked 54)

    UNIT_TEST_TITLE("Buy Lottery tickets");
    UNIT_TEST_CALL(initRRuleExpansion(es,t("2007-08-11T00:00:00"),'W','W',3,0x40,0x0,t("2009-02-23T00:00:00"),t("2009-03-31T00:00:00")),("-none-"),true,ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2009-02-28T00:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2009-03-21T00:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t(""),ok); // generated occurrences (checked 30)

    UNIT_TEST_TITLE("Recycling bin collection");
    UNIT_TEST_CALL(initRRuleExpansion(es,t("2007-08-17T00:00:00"),'W','W',2,0x20,0x0,t("2009-02-23T00:00:00"),t("2009-03-31T00:00:00")),("-none-"),true,ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2009-02-27T00:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2009-03-13T00:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2009-03-27T00:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t(""),ok); // generated occurrences (checked 44)

    UNIT_TEST_TITLE("test feb 29th");
    UNIT_TEST_CALL(initRRuleExpansion(es,t("2008-02-29T14:00:00"),'Y','M',1,0x0,0x0,t("2009-04-01T00:00:00"),t("2019-04-04T00:00:00")),("-none-"),true,ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2012-02-29T14:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2016-02-29T14:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t(""),ok);

    UNIT_TEST_TITLE("February and March 29th every year");
    UNIT_TEST_CALL(initRRuleExpansion(es,t("2008-02-29T14:00:00"),'Y','M',1,0x6,0x0,t("2009-04-01T00:00:00"),t("2016-04-04T00:00:00")),("-none-"),true,ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2010-03-29T14:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2011-03-29T14:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2012-02-29T14:00:00"),ok); // leap year
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2012-03-29T14:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2013-03-29T14:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2014-03-29T14:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2015-03-29T14:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2016-02-29T14:00:00"),ok); // leap year
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2016-03-29T14:00:00"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t(""),ok);

    UNIT_TEST_TITLE("suddenly expanding from start problem");
    UNIT_TEST_CALL(initRRuleExpansion(es,t("2007-08-17"),'W','W',2,0x20,0x0,t("2009-07-04"),t("2009-08-08")),("-none-"),true,ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2009-07-17"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t("2009-07-31"),ok);
    UNIT_TEST_CALL(lt = getNextOccurrence(es),("lt = %s",s(lt)),lt==t(""),ok);

  }
  return ok;
}

#endif // SYNTHESIS_UNIT_TEST



/// @brief initialize expansion of RRule
void initRRuleExpansion(
  TRRuleExpandStatus &es,
  lineartime_t aDtstart,
  char aFreq, char aFreqmod,
  sInt16 aInterval,
  fieldinteger_t aFirstmask, fieldinteger_t aLastmask,
  lineartime_t aExpansionStart,
  lineartime_t aExpansionEnd
)
{
  // %%% hardcoded for now
  es.weekstart = 0; // starts on sunday for now
  // save recurrence parameters
  es.freq = aFreq;
  es.freqmod = aFreqmod;
  es.interval = aInterval;
  es.firstmask = aFirstmask;
  es.lastmask = aLastmask;
  // analyze bits
  es.singleFMaskBit = -1;
  if (es.lastmask==0 && es.firstmask) {
    fieldinteger_t m = es.firstmask;
    es.singleFMaskBit = 0;
    while((m & 1)==0) { m>>=1; es.singleFMaskBit++; } // calculate bit number of next set bit
    if (m & ~1) es.singleFMaskBit = -1; // more bits to come - reset again
  }
  // init expansion parameters
  // - no valid occurrence yet
  es.started = false;
  // - elements of start point
  es.starttime = lineartime2timeonly(aDtstart); // start time of day
  lineartime2date(aDtstart,&es.startyear,&es.startmonth,&es.startday); // year, month, day-in-month
  // - cursor, initialized to start point
  es.cursor = aDtstart;
  es.cursorMLen = getMonthDays(lineartime2dateonly(es.cursor));
  es.cursorWDay = lineartime2weekday(es.cursor); // get cursor weekday
  // save expansion range
  es.expansionEnd = aExpansionEnd; // can be noLinearTime if no end is set
  es.expansionStartDayOffset = 0;
  if (aExpansionStart!=noLinearTime) {
    // specified start of expansion period, apply if later than beginning of recurrence
    es.expansionStartDayOffset = (lineartime2dateonly(aExpansionStart)-lineartime2dateonly(es.cursor));
    if (es.expansionStartDayOffset<0)
      es.expansionStartDayOffset = 0;
  }
  #ifdef SYNTHESIS_UNIT_TEST
  sInt16 y,m,d,h,mi,s,ms;
  lineartime2date(aExpansionStart, &y, &m, &d);
  lineartime2time(aExpansionStart, &h, &mi, &s, &ms);
  printf("  aExpansionStart = %04hd-%02hd-%02hd %02hd:%02hd:%02hd - es.expansionStartDayOffset=%ld\n",y,m,d,h,mi,s,es.expansionStartDayOffset);
  #endif
}


// advance cursor by given number of days
static void adjustCursor(TRRuleExpandStatus &es, lineardate_t aDays)
{
  // now days = number of days to advance
  es.cursor += aDays*linearDateToTimeFactor;
  // adjust weekday
  es.cursorWDay += aDays % DaysPerWk;
  if (es.cursorWDay<0) es.cursorWDay += DaysPerWk;
  else if (es.cursorWDay>=DaysPerWk) es.cursorWDay -= DaysPerWk;
  #ifdef SYNTHESIS_UNIT_TEST
  sInt16 y,m,d,h,mi,s,ms;
  lineartime2date(es.cursor, &y, &m, &d);
  lineartime2time(es.cursor, &h, &mi, &s, &ms);
  printf("  es.cursor = %04hd-%02hd-%02hd %02hd:%02hd:%02hd / Day=%hd, lt=%lld\n",y,m,d,h,mi,s,es.cursorWDay,es.cursor);
  #endif
}


static lineardate_t makeMultiple(lineardate_t d, sInt16 aMultiple, sInt16 aMaxRemainder=0)
{
  if (aMultiple>1) {
    // we need to make sure remainder is 0 and advance extra
    lineardate_t r = d % aMultiple; // remainder
    if (r>aMaxRemainder) {
      // round up to next multiple
      d += aMultiple - r;
    }
  }
  return d;
}


// simple and fast cursor increment
static void incCursor(TRRuleExpandStatus &es)
{
  es.cursor += linearDateToTimeFactor;
  es.cursorWDay += 1;
  if (es.cursorWDay>=DaysPerWk) es.cursorWDay=0;
}


static bool expansionEnd(TRRuleExpandStatus &es)
{
  if (es.expansionEnd && es.cursor>=es.expansionEnd) {
    // end of expansion, reset cursor
    es.cursor = noLinearTime;
    return true;
  }
  // not yet end of expansion
  return false;
}





static sInt16 monthDiff(sInt16 y1, sInt16 m1, sInt16 y2, sInt16 m2)
{
  return (y1-y2)*12 + (m1-m2);
}


static void monthAdd(sInt16 &y, sInt16 &m, sInt16 a)
{
  m += a;
  if (m>12) {
    sInt16 r = (m-1)/12; // years plus
    y += r; // add the years
    m -= (r*12); // remove from the months
  }
}




/// @brief get next occurrence
/// @return noLinearTime if no next occurrence exists, lineartime of next occurrence otherwise
lineartime_t getNextOccurrence(TRRuleExpandStatus &es)
{
  if (es.cursor==noLinearTime) return noLinearTime; // already done (or never started properly)
  if (es.interval<=0) return noLinearTime; // interval=0 means no recurrence (but no error)
  // if we are already started, we need to advance the cursor first, then check for match
  bool advanceFirst = es.started;
  // now calculate
  if (es.freq=='D') {
    // Daily: can be calculated directly
    if (!es.started) {
      // calculate first occurrence
      adjustCursor(es, makeMultiple(es.expansionStartDayOffset, es.interval)); // move cursor to first day
    }
    else {
      // calculate next occurrence
      adjustCursor(es, es.interval);
    }
  } // D
  else if (es.freq=='W') {
    // Note: interval of weekly recurrences starts at sunday,
    // so recurrence starting at a Thu, scheduled for every two weeks on Mo and Thu will have:
    // 1st week: Thu, 2nd week: nothing, 3rd week: Mo,Thu, 4rd week: nothing, 5th week: Mo,Thu...
    if (!es.started) {
      // make sure we have a mask
      if (es.firstmask==0)
        es.firstmask = 1<<es.cursorWDay; // set start day in mask
      // advance cursor to first day of expansion period
      // - go back to start of very first week covered by the recurrence
      sInt16 woffs = es.cursorWDay-es.weekstart; // how many days back to next week start from dtstart
      if (woffs<0) woffs+=DaysPerWk; // we want to go BACK to previous week start
      adjustCursor(es, -woffs); // back to previous start of week (=start of first week covered by recurrence)
      lineardate_t d = es.expansionStartDayOffset+woffs; // days from beginning of first week to beginning day of expansion
      // if we exceed reach of mask in current interval, round up to beginning of next interval
      d = makeMultiple(d,es.interval*DaysPerWk,DaysPerWk-1); // expand into next interval if needed
      adjustCursor(es, d); // now advance cursor to
      if (expansionEnd(es)) goto done; // could by beyond current expanding scope due to interval jump
    }
    // calculate first/next occurrence (if not first occurrence, do not check initially but advance first)
    while(advanceFirst || !(
      es.firstmask & ((uInt64)1<<es.cursorWDay)
    )) {
      advanceFirst = false; // next time we need to check
      // - next day is next candidate
      incCursor(es); // next day
      if (es.cursorWDay==es.weekstart) {
        // a new week has started, we need to skip non-active weeks first
        adjustCursor(es, (es.interval-1)*DaysPerWk);
      }
    }
    // found occurrence
  } // W
  else if (es.freq=='M') {
    if (!es.started) {
      // make sure we have a mask
      if (es.firstmask==0 && es.lastmask==0) {
        if (es.freqmod=='W') {
          es.firstmask = (uInt64)1<<(es.cursorWDay+DaysPerWk*((es.startday-1)/DaysPerWk)); // set start weekday in mask
        }
        else {
          es.firstmask = (uInt64)1<<(es.startday-1); // set start monthday in mask
        }
      }
      // For all monthly repeats: find first month to apply freqmod to
      if (es.expansionStartDayOffset) {
        // does not necessarily start in cursor month
        // - move cursor to where we want to begin earliest
        adjustCursor(es, es.expansionStartDayOffset);
        // - components of the expansion start
        sInt16 y,m;
        lineartime2date(es.cursor,&y,&m,&es.startday); // year, month, day-in-month
        // - how many months since start
        sInt16 numMonths = monthDiff(y,m,es.startyear,es.startmonth);
        sInt16 monthMod = numMonths % es.interval;
        if (monthMod>0) {
          // need to go into subsequent month
          numMonths += es.interval-monthMod; // months to next interval start
          // entire month is after expansion start, so begin with 1st
          es.startday = 1;
        }
        monthAdd(es.startyear,es.startmonth,numMonths);
        // startYear/month/day now on first candidate
        // - calculate linear start
        es.cursor = date2lineartime(es.startyear, es.startmonth, es.startday);
        if (expansionEnd(es)) goto done; // could by beyond current expanding scope due to interval jump
        // - calculate last day in this month
        es.cursorMLen = getMonthDays(lineartime2dateonly(es.cursor));
        // - make sure cursor weekday is correct
        es.cursorWDay = lineartime2weekday(es.cursor);
      }
    }
    // apply freqmod
    if (es.freqmod=='W') {
      // monthly by weekday
      // - calculate first/next occurrence (if not first occurrence, do not check initially but advance first)
      while(true) {
        // calculate which weeks we are in
        sInt16 fwk=(es.startday-1) / DaysPerWk; // start is nth week of the month
        sInt16 lwk=(es.cursorMLen-es.startday) / DaysPerWk; // start is nth-last week of the month
        // check if we found an occurrence
        if (!advanceFirst) {
          if (
            (es.firstmask & ((uInt64)1<<(es.cursorWDay+DaysPerWk*fwk))) || // nth occurrence of weekday in month
            (es.lastmask & ((uInt64)1<<(es.cursorWDay+DaysPerWk*lwk))) // nth-last occurrence of weekday in month
          )
            break;
        }
        advanceFirst = false; // next time we need to check
        // - next day-in-month is next candidate
        es.cursorWDay++; if (es.cursorWDay>=DaysPerWk) es.cursorWDay=0; // next day in the week
        es.startday++;
        if (es.startday>es.cursorMLen) {
          // end of month reached
          // - goto next relevant month
          monthAdd(es.startyear,es.startmonth,es.interval);
          // - first day
          es.startday=1;
          // - advance cursor into new month
          es.cursor = date2lineartime(es.startyear, es.startmonth, es.startday);
          // - get length of this month
          es.cursorMLen = getMonthDays(lineartime2dateonly(es.cursor));
          // - make sure cursor weekday is correct
          es.cursorWDay = lineartime2weekday(es.cursor);
        }
      }
      // found occurrence, update cursor
      es.cursor = date2lineartime(es.startyear, es.startmonth, es.startday)+es.starttime;
    } // MW
    else {
      // monthly by monthday list
      // - calculate first/next occurrence (if not first occurrence, do not check initially but advance first)
      while(advanceFirst || !(
        (es.firstmask & ((uInt64)1<<(es.startday-1))) || // nth day in month
        (es.lastmask & ((uInt64)1<<(es.cursorMLen-es.startday))) // nth-last day in month
      )) {
        advanceFirst = false; // next time we need to check
        if (es.singleFMaskBit>=0) {
          // - next candidate is indicated by single mask bit
          if (es.startday<es.singleFMaskBit+1)
            es.startday = es.singleFMaskBit+1; // jump to start day
          else
            es.startday = es.cursorMLen+1; // jump to end of month
        }
        else {
          // - next day-in-month is next candidate
          es.startday++;
        }
        if (es.startday>es.cursorMLen) {
          // end of month reached
          // - goto next relevant month
          monthAdd(es.startyear,es.startmonth,es.interval);
          // - first day
          es.startday=1;
          // - advance cursor into new month
          es.cursor = date2lineartime(es.startyear, es.startmonth, es.startday);
          // - get length of this month
          es.cursorMLen = getMonthDays(lineartime2dateonly(es.cursor));
        }
      }
      // found occurrence, update cursor
      es.cursor = date2lineartime(es.startyear, es.startmonth, es.startday)+es.starttime;
    } // MD
  } // M
  else if (es.freq=='Y') {
    if (!es.started) {
      // Make sure we have a mask or cancel freqmod entirely
      if (es.firstmask==0 && es.lastmask==0) {
        // no mask at all, only startmonth counts anyway -> revert to simple direct calculation (cancel freqmod)
        es.freqmod = 0;
      }
      else if (es.singleFMaskBit>=0 && es.singleFMaskBit+1==es.startmonth) {
        // mask with single bit on start date month -> can revert to simple diect calculation (cancel freqmod)
        es.freqmod = 0; // no month list parsing needed
      }
      // For all yearly repeats: find first year to apply freqmod to
      if (es.expansionStartDayOffset) {
        // does not necessarily start in cursor year
        // - move cursor to where we want to begin earliest
        adjustCursor(es, es.expansionStartDayOffset);
        // - year of the expansion start
        sInt16 y,m,d;
        lineartime2date(es.cursor,&y,&m,&d); // year, month, day-in-month
        // - make sure cursor can be a candidate (i.e. is not before start)
        if (es.freqmod=='M') {
          if (d>es.startday)
            monthAdd(y, m, 1); // no occurrence possible in this month, next candidate is in next month
        }
        else {
          if (m>es.startmonth || (m==es.startmonth && d>es.startday))
            y++; // no occurrence possible this year, next candidate is in next year
        }
        // - make sure we are in a year at the beginning of the interval
        sInt16 numYears = y-es.startyear;
        sInt16 yearMod = numYears % es.interval;
        if (yearMod>0) {
          // need to go into subsequent year
          numYears += es.interval-yearMod; // months to next interval start
          m = 1; // start with Jan again (only used below when checking monthlist i.e. freqmod==M)
        }
        es.startyear += numYears;
        if (es.freqmod=='M') {
          // month-by-month checking
          es.startmonth = m;
        }
        // startYear/month/day now on first candidate
        // - calculate linear start
        es.cursor = date2lineartime(es.startyear, es.startmonth, 1)+es.starttime;
        es.cursorMLen = getMonthDays(lineartime2dateonly(es.cursor));
        adjustCursor(es, es.startday-1);
        if (expansionEnd(es)) goto done; // could by beyond current expanding scope due to interval jump
        // - calculate last day in this month
      }
    }
    // apply freqmod
    if (es.freqmod=='M') {
      // yearly by month list (and we really have a month list, not only a single month bit)
      // - calculate first/next occurrence (if not first occurrence, do not check initially but advance first)
      int yearsscanned = 0;
      do {
        while(advanceFirst || !(
          (es.firstmask & ((uInt64)1<<(es.startmonth-1))) || // nth month
          (es.lastmask & ((uInt64)1<<(12-es.startmonth))) // nth-last month
        )) {
          advanceFirst = false; // next time we need to check
          // - next month is next candidate
          es.startmonth++;
          if (es.startmonth>12) {
            // end of year reached
            // - goto next relevant year
            es.startyear += es.interval;
            yearsscanned ++;
            // - first month again
            es.startmonth = 1;
          }
        }
        // found possible occurrence (could be a day that does not exist in the month), update cursor
        es.cursor = date2lineartime(es.startyear, es.startmonth, 1)+es.starttime;
        es.cursorMLen = getMonthDays(lineartime2dateonly(es.cursor));
        adjustCursor(es, es.startday-1);
        // repeat until day actually exists
        if (es.startday>es.cursorMLen) {
          // disable checking for match again
          advanceFirst = true;
        }
      } while(advanceFirst && yearsscanned<5);
      // found occurrence
    } // YM
    else {
      // yearly on same date
      while (advanceFirst || es.startday>es.cursorMLen ) {
        advanceFirst = false; // next time we need to check
        // just calculate next candiate
        es.startyear += es.interval;
        // update cursor and calculate
        es.cursor = date2lineartime(es.startyear, es.startmonth, 1)+es.starttime;
        es.cursorMLen = getMonthDays(lineartime2dateonly(es.cursor));
        adjustCursor(es, es.startday-1);
      }
      // found occurrence
    } // Yx
  } // Y
  else {
    // unknown = nonexpandable recurrence
    // - return start date as first occurrence to make sure it does not get invisible
    if (es.started) {
      // subsequent occurrence requested: not available
      es.cursor = noLinearTime;
    }
  }
  // stop expansion if end of expansion period reached
  expansionEnd(es);
done:
  // done
  es.started = true;
  return es.cursor;
} // getNthNextOccurrence








/// @brief calculate number of recurrences from specified end date of RRULE
/// @return true if count could be calculated, false otherwise
/// @param[in] dtstart, until : must be in the correct context to evaluate weekday rules
/// @param[in] countsoccurrences : if true, counting occurrences (rather than intervals)
/// @note returns until=noLinearTime for endless repeat (count=0)
bool countFromEndDate(
  sInt16 &cnt, bool countsoccurrences,
  lineartime_t dtstart,
  char freq, char freqmod,
  sInt16 interval,
  fieldinteger_t firstmask,fieldinteger_t lastmask,
  lineartime_t until,
  TDebugLogger *aLogP
)
{
  if (dtstart==noLinearTime || interval<1) return false; // no start date or interval, cannot calc count
  if (until==noLinearTime) {
    // endless repeat
    cnt=0;
    return true;
  }
  // iterate until end date is reached
  cnt=1;
  lineartime_t occurrence=noLinearTime;
  // break after 500 recurrences
  while (cnt<500) {
    if (!endDateFromCount(
      occurrence,
      dtstart,
      freq,freqmod,
      interval,
      firstmask,lastmask,
      cnt,
      countsoccurrences,
      aLogP
    ))
      return false; // error, cannot calc end date
    if (occurrence>until) {
      // no more occurrences
      cnt--;
      if (cnt==0) return false; // if not endless, but no occurrence found in range -> not repeating
      return true; // return count
    }
    // next occurrence
    cnt++;
  }
  // 500 and more recurrences count as endless
  cnt=0;
  return true;
} // countFromEndDate



// Converts  vCalendar 2.0 RRULE string into internal recurrence representation
bool RRULE2toInternal(
  const char *aText, // RRULE string to be parsed
  lineartime_t dtstart, // reference date for parsing RRULE
  timecontext_t startcontext, // context of reference date
  char &freq,
  char &freqmod,
  sInt16 &interval,
  fieldinteger_t &firstmask,
  fieldinteger_t &lastmask,
  lineartime_t &until,
  timecontext_t &untilcontext,
  TDebugLogger *aLogP,
  lineartime_t *aNewStartP
)
{
  #ifdef SYDEBUG
  string abc;
  LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("RRULE2toInternal(): start analyzing '%s'", aText));
  #endif

  string temp;
  const char *p;
  char s [50];
  sInt16 startwday;
  sInt16 startyear,startmonth,startday;

  string::size_type endIndex = 0;
  string::size_type startIndex = 0;
  uInt16 cnt = 0;
  size_t pos;
  bool calculateEndDate = false; // indicator that end date calc is required as last step
  bool calculateFirstOccurrence = false; // indicator that aNewStartP should be adjusted to show the first occurrence
  string key,value,byday,bymonthday,bymonth;

  // get elements of start point
  startwday=lineartime2weekday(dtstart); // get starting weekday
  lineartime2date(dtstart,&startyear,&startmonth,&startday); // year, month, day-in-month
  until= noLinearTime;  // initialize end point, if not available

  // do the conversion here
  p=aText;
  int start = 0;
  // get freq
  if (!getNextDirective(temp, p, start)) {
    // failed
    goto incompat;
  }
  // analyze freq
  if (temp == "FREQ=YEARLY") {
    freq='Y';
  }
  else if (temp == "FREQ=MONTHLY") {
    freq='M';
  }
  else if (temp == "FREQ=WEEKLY") {
    freq='W';
  }
  else if (temp == "FREQ=DAILY") {
    freq='D';
  }
  else {
    goto incompat;
  }

  LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- found frequency %s which maps to freq %c", temp.c_str(), freq));

  // init vars
  cnt = 0;

  // set interval to 1
  interval = 1;
  freqmod = ' ';
  firstmask = 0;
  lastmask = 0;
  // get next directives
  while (getNextDirective(temp, p, start)) {
    LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- found next directive %s", temp.c_str()));
    // split
    pos = temp.find("=");
    if (pos == string::npos || pos == 0 || pos == (temp.length() - 1)) {
      goto incompat;
    }
    key = temp.substr(0, pos);
    value = temp.substr(pos + 1, temp.length() - 1);
    LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- extracted key/value %s/%s", key.c_str(), value.c_str()));
    // check for until
    if (key == "UNTIL") {
      if (ISO8601StrToTimestamp(value.c_str(), until, untilcontext)==0)
        goto incompat; // invalid end date
      calculateEndDate = false;
    }
    // check for count
    else if (key == "COUNT") {
      // convert count
      for (int i = 0, ii = value.length(); i < ii; ++i) {
        if (isdigit(value[i])) {
          cnt = cnt * 10 + ((value[i]) - '0');
        }
      }
      // check if no count or one only
      if (cnt <= 1)
        goto norep; // no repetition
      // we need to recalculate the enddate from count
      calculateEndDate = true;
    }
    // check for interval
    else if (key == "INTERVAL") {
      // convert interval
      interval = 0;
      for (int i = 0, ii = value.length(); i < ii; ++i) {
        if (isdigit(value[i])) {
          interval = interval * 10 + ((value[i]) - '0');
        }
      }
      if (interval == 0)
        goto incompat; // invalid interval
    }
    // just copy all supported BYxxx rules into vars
    else if (key == "BYDAY")
      byday = value;
    else if (key == "BYMONTHDAY")
      bymonthday = value;
    else if (key == "BYMONTH")
      bymonth = value;
    // ignore week start
    else if (key == "WKST") {
      // nop
    }
    else
      goto incompat; // unknown directive
  } // while

  #ifdef SYDEBUG
  LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- dtstart weekday is %i", startwday));
  LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- extracted interval %u", interval));
  LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- extracted interval (2nd time) %u", interval));
  LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- extracted count %u", cnt));
  LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- end date calc required? %s", calculateEndDate ? "true" : "false"));
  LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- extracted byday %s", byday.c_str()));
  LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- extracted bymonthday %s", bymonthday.c_str()));
  LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- extracted bymonth %s", bymonth.c_str()));
  TimestampToISO8601Str(abc,until,startcontext,false,false);
  LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- extracted until %s", abc.c_str()));
  #endif

  // Generally, we only support one BYxxx, not combinations. However, FREQ=YEARLY + BYMONTH + BYDAY is common
  // to express start and end times of DST in VTIMEZONEs, so we convert these into equivalent FREQ=MONTHLY
  if (freq=='Y' && !byday.empty() && !bymonth.empty() && bymonth.find(",")==string::npos) {
    // yearly by (single!) month and by day
    // - get month as indicated by BYMONTH
    sInt16 newStartMonth = 0;
    if (StrToShort(bymonth.c_str(), newStartMonth)>0 && newStartMonth) {
      // - check if BYMONTH is just redundant and references same month as start date
      bool convertToMonthly = startmonth>0 && startmonth==newStartMonth;
      if (!convertToMonthly && aNewStartP) {
        // not redundant, but we can return a new start date
        convertToMonthly = true;
        // calculate the new start date
        startmonth = newStartMonth;
        dtstart = date2lineartime(startyear, startmonth, startday) + lineartime2timeonly(dtstart);
        LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- moved recurrence origin to %04hd-%02hd-%02hd",startyear, startmonth, startday));
        // pass it back to caller
        calculateFirstOccurrence = true; // need to fine tune at the end
        *aNewStartP = dtstart;
      }
      if (convertToMonthly) {
        LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- converted FREQ=YEARLY BYMONTH into MONTHLY"));
        freq = 'M'; // convert from YEARLY to MONTHLY
        interval = 12*interval; // but with 12 months interval -> equivalent
        bymonth.erase(); // forget BYMONTH clause
      }
    }
  } // if FREQ=YEARLY + BYMONTH + BYDAY
  // check freq
  endIndex = 0;
  startIndex = 0;
  switch (freq) {
    case 'D' :
      // make weekly if byday and nothing else is set
      if (!(byday == "") && bymonth == "" && bymonthday == "") {
        freq='W';
        freqmod='W';
        // search separator ','
        while ((endIndex = byday.find(",", startIndex)) != string::npos) {
          // set masks for the specific day
          if (!setWeekday(byday, firstmask, lastmask, startIndex, endIndex, false))
            goto incompat;
          // set new start
          startIndex = endIndex + 1;
          if (startIndex >= byday.length())
            break;
        }
        // check if anything is behind endindex
        if (endIndex == string::npos && startIndex < byday.length()) {
          endIndex = byday.length();
          if (!setWeekday(byday, firstmask, lastmask, startIndex, endIndex, false))
            goto incompat;
        }
      }
      else {
        // we don't support byday and anything else at the same time
        if (!byday.empty())
          goto incompat;
        // ok, no mod
        freqmod = ' ';
      }
      break;
    case 'W' :
      // we don't support month or monthday within weekly
      if (!bymonth.empty() || !bymonthday.empty())
        goto incompat;
      // set weekly and days
      freqmod='W';
      if (!byday.empty()) {
        // search separator ','
        while ((endIndex = byday.find(",", startIndex)) != string::npos) {
          // set masks for the specific day
          if (!setWeekday(byday, firstmask, lastmask, startIndex, endIndex, false))
            goto incompat;
          // set new start
          startIndex = endIndex + 1;
          if (startIndex >= byday.length())
            break;
        }
        // check if anything is behind endindex
        if (endIndex == string::npos && startIndex < byday.length()) {
          endIndex = byday.length();
          if (!setWeekday(byday, firstmask, lastmask, startIndex, endIndex, false))
            goto incompat;
        }
      }
      break;
    case 'M' :
      // we don't support by month in monthly
      if (!bymonth.empty())
        goto incompat;
      // we don't support byday and bymonthday at the same time
      if (!byday.empty() && !bymonthday.empty())
        goto incompat;
      // check if bymonthday
      if (!bymonthday.empty()) {
        freqmod = 'D';
        // search separator ','
        while ((endIndex = bymonthday.find(",", startIndex)) != string::npos) {
          // set masks for the specific day
          if (!setMonthDay(bymonthday, firstmask, lastmask, startIndex, endIndex))
            goto incompat;
          // set new start
          startIndex = endIndex + 1;
          if (startIndex >= bymonthday.length())
            break;
        }
        // check if anything is behind endindex
        if (endIndex == string::npos && startIndex < bymonthday.length()) {
          endIndex = bymonthday.length();
          if (!setMonthDay(bymonthday, firstmask, lastmask, startIndex, endIndex))
            goto incompat;
        }
      }
      // or if by weekday
      else if (!byday.empty()) {
        freqmod = 'W';
        // search separator ','
        while ((endIndex = byday.find(",", startIndex)) != string::npos) {
          // set masks for the specific day
          if (!setWeekday(byday, firstmask, lastmask, startIndex, endIndex, true))
            goto incompat;
          // set new start
          startIndex = endIndex + 1;
          if (startIndex >= byday.length())
            break;
        }
        // check if anything is behind endindex
        if (endIndex == string::npos && startIndex < byday.length()) {
          endIndex = byday.length();
          if (!setWeekday(byday, firstmask, lastmask, startIndex, endIndex, true))
            goto incompat;
        }
      }
      else {
        // fine, no mod
        freqmod = ' ';
      }
      break;
    case 'Y' :
      if (
        byday == "" ||
        (byday.length() == 2 && byday[0] == RRULE_weekdays[startwday][0] &&
        byday[1] == RRULE_weekdays[startwday][1])
      ) {
        temp.erase();
        sprintf(s, "%hd", startday);
        temp.append(s);
        LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- bymonthday checking %s against %s", temp.c_str(), bymonthday.c_str()));
        if (bymonthday == "" || bymonthday == temp) {
          temp.erase();
          sprintf(s, "%hd", startmonth);
          temp.append(s);
          LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- bymonth checking %s against %s", temp.c_str(), bymonth.c_str()));
          if (bymonth == "" || bymonth == temp) {
            // this should usually be ' ' but the vcard conversion has a bug and requires 'M'
            freqmod = 'M';
            lastmask = 0;
            firstmask = 0;
          }
          else
            goto incompat;
        }
        else
          goto incompat;
      }
      else
        goto incompat;
      break;
    default :
      LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- strange self-set freq %c, rule was %s", freq, aText));
      break;
  } // switch

  // calc end date, assumption/make sure: cnt > 1
  if (calculateEndDate) {
    LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- calculating end date now"));
    if (!endDateFromCount(until,dtstart,freq,freqmod,interval,firstmask,lastmask,cnt,true,aLogP))
      goto norep;
    untilcontext = startcontext; // until is in same context as start
  }
  // calculate exact date of first occurrence
  if (calculateFirstOccurrence && aNewStartP) {
    TRRuleExpandStatus es;
    initRRuleExpansion(es, dtstart, freq, freqmod, interval, firstmask, lastmask, dtstart, noLinearTime);
    dtstart = getNextOccurrence(es);
    if (LOGDEBUGTEST(aLogP,DBG_PARSE+DBG_EXOTIC)) {
      sInt16 ny,nm,nd;
      lineartime2date(dtstart,&ny,&nm,&nd);
      LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- adjusted first occurrence date to %04hd-%02hd-%02hd",ny, nm, nd));
    }
    *aNewStartP = dtstart;
  }
  // parsed ok, now store it
  goto store;
norep:
  // no repetition (but no parse error generated)
  freq='0'; // frequency = none
  freqmod=' '; // frequency modifier
  interval=0; // interval
  firstmask=0; // day mask counted from the first day of the period
  lastmask=0; // day mask counted from the last day of the period
  until= noLinearTime; // last day
store:

  #ifdef SYDEBUG
  LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("- leaving with freq %c, freqmod %c, interval %hd, firstmask %ld, lastmask %ld", freq, freqmod, interval, (long)firstmask, (long)lastmask));
  TimestampToISO8601Str(abc,until,untilcontext,false,false);
  LOGDEBUGPRINTFX(aLogP,DBG_PARSE+DBG_EXOTIC,("RRULE2toInternal(): extracted until '%s'", abc.c_str()));
  #endif

  return true; // ok
incompat:
  // incompatible, value cannot be parsed usefully
  return false; // no value generated
} // RRULE2toInternal



// appends the firstmask and lastmask as numbers to the string.
// all values are increased by one prior to adding them to string.
void appendMaskAsNumbers(
  cAppCharP aPrefix, // if list is not empty, this will be appended in front of the list
  string &aString, // receives list string
  fieldinteger_t firstmask,
  fieldinteger_t lastmask
)
{
  fieldinteger_t m = firstmask;
  int i,k;
  for (i=0; i<2; i++) {
    // - show day numbers
    for (k=0; k<32; k++) {
      if (m & ((uInt64)1<<k)) {
        aString+=aPrefix;
        aPrefix=","; // prefix for further elements is now colon.
        if (i>0) aString+='-';
        StringObjAppendPrintf(aString,"%d",k+1);
      }
    }
    // - switch to those that are relative to the end of the month / year
    m = lastmask;
  }
} // appendMaskAsNumbers


// returns the next directive for the ical format
bool getNextDirective(
  string &aString,
  const char *aText,
  int &aStart
)
{
  // check
  if (*aText == 0) {
    return false;
  }

  char c;
  // check start
  if (aStart > 0) {
    // skip to start
    for (int i = 0; (i < aStart) && ((c = *aText) != 0); ++i) aText++;
  }
  else {
    // skip all spaces
    while ((c = *aText) == ' ') {
       aText++;
    }
  }
  // erase string, set counter
  aString.erase();
  uInt16 counter = 0;
  // append text
  c = *aText++; aStart++;
  while (c != 0 && c != ';') {
    aString.append(1, c);
    ++counter;
    c = *aText++; aStart++;
  }
  // empty?
  if (counter == 0)
    return false;
  // remove trailing whitespace
  sInt16 i = counter - 1;
  while (i >= 0 && aString[i] == ' ') {
    aString.erase(i, 1);
    --i;
  }
  // check length
  if (i == -1)
    return false;
  // fine
  return true;
} // getNextDirective



// maps the byday rule into the masks
bool setWeekday(
  const string &byday,
  fieldinteger_t &firstmask,
  fieldinteger_t &lastmask,
  string::size_type &startIndex,
  const string::size_type &endIndex,
  bool allowSpecial
)
{
  // remove leading spaces
  while (byday[startIndex] == ' ' && startIndex < endIndex) {
    ++startIndex;
  }
  // check if digit or sign
  if (isdigit(byday[startIndex]) || byday[startIndex] == '+' || byday[startIndex] == '-') {
    // check if numbers before week days are allowed
    if (!allowSpecial) {
      return false;
    }
    // special treatment
    return setSpecialWeekday(byday, firstmask, lastmask, startIndex, endIndex);
  }
  // get index of weekday array
  sInt16 weekdayIndex = getWeekdayIndex(byday, startIndex);
  if (weekdayIndex == -1) {
    return false;
  }
  // put into mask
  if (!allowSpecial) {
    firstmask |= ((uInt64)1<<weekdayIndex);
    return true;
  }
  for (int i= 0; i<WeeksOfMonth; i++) { // do it for all weeks of the month
    firstmask |= ((uInt64)1<<weekdayIndex);
    weekdayIndex+= DaysOfWeek;
  } // for
  return true;
} // setWeekday



// maps a special weekday (+/- int WEEKDAY) into the masks
bool setSpecialWeekday(
  const string &byday,
  fieldinteger_t &firstmask,
  fieldinteger_t &lastmask,
  string::size_type &startIndex,
  const string::size_type &endIndex
)
{
  // indicator for negative values
  bool isNegative;
  // the value
  sInt16 val = 0;

  // check sign
  if (byday[startIndex] == '-') {
    isNegative = true;
    ++startIndex;
  }
  else if (byday[startIndex] == '+') {
    isNegative = false;
    ++startIndex;
  }
  else {
    isNegative = false;
  }
  // convert to number
  while (isdigit(byday[startIndex])) {
    val = val * 10 + ((byday[startIndex++]) - '0');
  }
  // remove leading spaces
  while (byday[startIndex] == ' ' && startIndex < endIndex) {
    ++startIndex;
  }
  // make sure there's enough space
  if (startIndex >= endIndex - 1) {
    return false;
  }
  // get index for weekday
  sInt16 index = getWeekdayIndex(byday, startIndex);
  if (index == -1) {
    return false;
  }
  // put into mask
  if (isNegative) {
    lastmask |= (((uInt64)1<<index)<<((val - 1) * DaysOfWeek));
  }
  else {
    firstmask |= (((uInt64)1<<index)<<((val - 1) * DaysOfWeek));
  }
  return true;
} // setSpecialWeekday


// returns the index within the rrule weekday array for the next
// two chars of the supplied string starting at startIndex
sInt16 getWeekdayIndex(
  const string &byday,
  sInt16 startIndex
)
{
  // search
  for (sInt16 i = 0; i < DaysOfWeek; ++i)
  {
    if (RRULE_weekdays[i][0] == byday[startIndex] && RRULE_weekdays[i][1] == byday[startIndex + 1])
      return i;
  }
  // not found
  return -1;
} // getWeekdayIndex


// set a day in month
bool setMonthDay(
  const string &bymonthday,
  fieldinteger_t &firstmask,
  fieldinteger_t &lastmask,
  string::size_type &startIndex,
  const string::size_type &endIndex
)
{
  // indicator for negative values
  bool isNegative;
  // the value
  sInt16 val=0;

  // check sign
  if (bymonthday[startIndex] == '-') {
    isNegative = true;
    ++startIndex;
  }
  else if (bymonthday[startIndex] == '+') {
    isNegative = false;
    ++startIndex;
  }
  else {
    isNegative = false;
  }
  // convert to number
  for (string::size_type i = startIndex; i < endIndex; ++i) {
    if (isdigit(bymonthday[i])) {
      val = val * 10 + ((bymonthday[i]) - '0');
    }
    else {
      return false;
    }
  }
  // put into mask
  if (isNegative) {
    lastmask |= ((uInt64)1<<(val - 1));
  }
  else {
    firstmask |= ((uInt64)1<<(val - 1));
  }
  return true;
} // setMonthDay

} // namespace sysync

/* eof */
