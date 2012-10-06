/*
 *  File:         rrules.h
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

#ifndef RRULES_H
#define RRULES_H

// includes
#ifndef FULLY_STANDALONE
#include "itemfield.h"
#include "debuglogger.h"
#else
#include <string>
#include "lineartime.h"
#endif


using namespace sysync;

namespace sysync {

#ifdef FULLY_STANDALONE
typedef sInt64 fieldinteger_t;
class TDebugLogger;
#endif

#ifdef SYSYNC_TOOL

// convert between RRULE and internal format
int rruleConv(int argc, const char *argv[]);

#endif

#ifdef MOBOSX
  // make this usable by the Todo+Cal app
  #define PUBLIC_ENTRY extern "C"
  #define PUBLIC_ENTRY_ATTR __attribute__((visibility("default")))
#else
  #define PUBLIC_ENTRY
  #define PUBLIC_ENTRY_ATTR
#endif



const int DaysOfWeek  = 7;
const int WeeksOfMonth= 5;


#ifdef SYNTHESIS_UNIT_TEST
// RRULE expansion tests
bool test_expand_rrule(void);
#endif


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
);


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
);

// Converts  vCalendar 1.0 RRULE string into internal recurrence representation
bool RRULE1toInternal(
  const char *aText, // RRULE string to be parsed
  lineartime_t dtstart, // reference date for parsing RRULE
  timecontext_t startcontext, // context of reference date, until will be in same context
  char &freq,
  char &freqmod,
  sInt16 &interval,
  fieldinteger_t &firstmask,
  fieldinteger_t &lastmask,
  lineartime_t &until,
  timecontext_t &untilcontext,
  TDebugLogger *aLogP
);

// Converts  vCalendar 2.0 RRULE string into internal recurrence representation
bool RRULE2toInternal(
  const char *aText, // RRULE string to be parsed
  lineartime_t dtstart, // reference date for parsing RRULE
  timecontext_t startcontext, // context of reference date, until will be in same context
  char &freq,
  char &freqmod,
  sInt16 &interval,
  fieldinteger_t &firstmask,
  fieldinteger_t &lastmask,
  lineartime_t &until,
  timecontext_t &untilcontext,
  TDebugLogger *aLogP,
  lineartime_t *aNewStartP = NULL // optional, if not NULL, conversion may return modified start date
);



/// @brief recurrence rule expansion status record
typedef struct {
  // recurrence definition parameters
  char freq;
  char freqmod;
  sInt16 interval;
  fieldinteger_t firstmask;
  fieldinteger_t lastmask;
  sInt16 weekstart; // 0=Su, 1=Mo...
  // bit anaalysis for speedup
  sInt16 singleFMaskBit; // if >=0, only this bit number is set in firstmask and none in lastmask.
  // start and end point of recurrence
  lineardate_t expansionStartDayOffset;
  lineartime_t expansionEnd;
  // internal status
  // - start point
  sInt16 startyear,startmonth,startday;
  lineartime_t starttime; // time part for re-adding after expansion
  // - current point of expansion
  lineartime_t cursor;
  sInt16 cursorWDay; // weekday
  sInt16 cursorMLen; // length of month
  // - set if started (i.e. advanced cursor to first valid recurrence)
  bool started;
} TRRuleExpandStatus;


/// @brief initialize expansion of RRule
PUBLIC_ENTRY void initRRuleExpansion(
  TRRuleExpandStatus &es,
  lineartime_t aDtstart,
  char aFreq, char aFreqmod,
  sInt16 aInterval,
  fieldinteger_t aFirstmask, fieldinteger_t aLastmask,
  lineartime_t aExpansionStart=noLinearTime,
  lineartime_t aExpansionEnd=noLinearTime
);

/// @brief get next occurrence
/// @return noLinearTime if no next occurrence exists, lineartime of next occurrence otherwise
PUBLIC_ENTRY lineartime_t getNextOccurrence(TRRuleExpandStatus &es);



/// @brief calculate end date of RRULE when count is specified
/// @return true if repeating, false if not repeating at all
/// @note returns until=noLinearTime for endless repeat (count=0)
/// @note this is made public as TodoZ iPhone app uses it for calendar recurrence expansion
PUBLIC_ENTRY bool endDateFromCount(
  lineartime_t &until,
  lineartime_t dtstart,
  char freq, char freqmod,
  sInt16 interval,
  fieldinteger_t firstmask,fieldinteger_t lastmask,
  sInt16 cnt, bool countsoccurrences,
  TDebugLogger *aLogP
) PUBLIC_ENTRY_ATTR;


/// @brief calculate number of recurrences from specified end date of RRULE
/// @return true if count could be calculated, false otherwise
/// @note returns until=noLinearTime for endless repeat (count=0)
bool countFromEndDate(
  sInt16 &cnt, bool countsoccurrences,
  lineartime_t dtstart,
  char freq, char freqmod,
  sInt16 interval,
  fieldinteger_t firstmask,fieldinteger_t lastmask,
  lineartime_t until,
  TDebugLogger *aLogP
);


// appends the firstmask and lastmask as numbers to the string.
// all values are increased by one prior to adding them to string.
void appendMaskAsNumbers(
  cAppCharP aPrefix, // if list is not empty, this will be appended in front of the list
  string &aString, // receives list string
  fieldinteger_t firstmask,
  fieldinteger_t lastmask
);

// returns the next directive for the ical format
bool getNextDirective(
  string &aString,
  const char *aText,
  int &aStart
);

// maps the byday rule into the masks
bool setWeekday(
  const string &byday,
  fieldinteger_t &firstmask,
  fieldinteger_t &lastmask,
  string::size_type &startIndex,
  const string::size_type &endIndex,
  bool allowSpecial
);

// maps a special weekday (+/- int WEEKDAY) into the masks
bool setSpecialWeekday(
  const string &byday,
  fieldinteger_t &firstmask,
  fieldinteger_t &lastmask,
  string::size_type &startIndex,
  const string::size_type &endIndex
);

// returns the index within the rrule weekday array for the next
// two chars of the supplied string starting at startIndex
sInt16 getWeekdayIndex(
  const string &byday,
  sInt16 startIndex
);

// set a day in month
bool setMonthDay(
  const string &bymonthday,
  fieldinteger_t &firstmask,
  fieldinteger_t &lastmask,
  string::size_type &startIndex,
  const string::size_type &endIndex
);

} // namespace sysync

#endif // RRULES_H

/* eof */
