/* 
 *  File:         platform_time.h
 *
 *  Platform specific time implementations
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 *  2004-11-15 : luz : extracted from lineartime.h
 */
 
#ifndef PLATFORM_TIME_H
#define PLATFORM_TIME_H

#include <ctime>

/* obsolete
using namespace std;

namespace sysync {

// define a type for it
typedef struct tm struct_tm;

// Windows and Linux/MacOSX have UNIX-time compatible origin
// which is 1970-01-01 00:00:00
// but as we have 64bit ints, we use our own linear time
// in milliseconds (=24*60*60*1000*lineardate)
typedef sInt32 lineardate_t;
// standard Linux/Win32/MacOSX
typedef sInt64 lineartime_t;
const lineartime_t  noLinearTime = 0x0;                // undefined lineartime value
const lineartime_t maxLinearTime = 0x7FFFFFFFFFFFFFFFLL; // signed 64 bit, defined as long long
// date origin definition relative to algorithm's origin -4712-01-01 00:00:00
const lineardate_t linearDateOriginOffset=0; // no offset
const sInt16 linearDateOriginWeekday=1; // Monday
// scaling of lineartime relative to seconds
const lineartime_t secondToLinearTimeFactor = 1000; // unit is milliseconds

// fine resolution sleep support
void sleepLineartime(lineartime_t aHowLong);

} // namespace sysync
*/

#endif // PLATFORM_TIME_H

/* eof */
