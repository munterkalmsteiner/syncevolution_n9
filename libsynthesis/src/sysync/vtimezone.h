/*
 *  File:         vtimezone.h
 *
 *  Author:       Beat Forster (bfo@synthesis.ch)
 *
 *  Parser/Generator routines for VTIMEZONE
 *
 *  Copyright (c) 2006-2011 by Synthesis AG + plan44.ch
 *
 *  2006-03-06 : bfo : created from exctracts of "rrules.h"
 *
 */

#ifndef VTIMEZONE_H
#define VTIMEZONE_H


// includes
#include "timezones.h"
#include "lineartime.h"
#include "debuglogger.h"


using namespace sysync;

namespace sysync {

// forward
class TDebugLogger;

bool VTIMEZONEtoTZEntry( const char*    aText, // VTIMEZONE string to be parsed
                         tz_entry      &t,
                         TDebugLogger*  aLogP);

/*! Convert VTIMEZONE string into internal context value */
bool VTIMEZONEtoInternal( const char*    aText,   ///< VTIMEZONE string to be parsed
                          timecontext_t &aContext,
                          GZones*        g,
                          TDebugLogger*  aLog= NULL,
                          string*        aTzidP= NULL ); ///< if not NULL, receives TZID as found in VTIMEZONE


/*! Convert internal context value into VTIMEZONE */
bool internalToVTIMEZONE( timecontext_t  aContext,
                          string        &aText,   ///< receives VTIMEZONE string
                          GZones*        g,
                          TDebugLogger*  aLog= NULL,
                          sInt32         testYear=  0,  // starting year
                          sInt32         untilYear= 0, // ending year
                          cAppCharP      aPrefIdent= NULL ); // preferred type of TZID


/*! Convert TZ/DAYLIGHT string into internal context value */
bool TzDaylightToContext( const char*    aText,     ///< DAYLIGHT property value to be parsed
                          timecontext_t  aStdOffs,  ///< Standard (non-DST) offset obtained from TZ
                          timecontext_t &aContext,  ///< receives context
                          GZones*        g,
                          timecontext_t  preferredCtx = TCTX_UNKNOWN, // preferred context, if rule matches more than one context
                          TDebugLogger*  aLog= NULL );


/*! Create DAYLIGHT string from context for a given sample time(year) */
bool ContextToTzDaylight( timecontext_t  aContext,
                          lineartime_t   aSampleTime, ///< specifies year for which we want to see a sample
                          string        &aText,       ///< receives DAYLIGHT string
                          timecontext_t &aStdOffs,    ///< receives standard (non-DST) offset for TZ
                          GZones*        g,
                          TDebugLogger*  aLog= NULL );


// ---- utility functions ---------------------------------------------------------
/*! <aStr> parsing:
 *  Get the string between "BEGIN:<value>\n" and "END:<value>\n"
 *  Default: First occurance
 */
string VStr( const string &aStr, const string &value, sInt32 aNth= 1 );


/*! <aStr> parsing:
 *  Get the value between "<key>:" and "\n"
 */
string VValue( const string &aStr, const string &key );


/*! Get the hour/minute string of <bias> */
string HourMinStr( int bias );



} // namespace sysync

#endif // VTIMEZONE_H

/* eof */
