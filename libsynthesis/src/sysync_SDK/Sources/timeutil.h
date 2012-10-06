/*
 *  File:         timeutil.h
 *
 *  Authors:      Lukas Zeller (luz@plan44.ch)
 *                Beat Forster (bfo@synthesis.ch)
 *
 *  ISO8601 / Lineartime functions
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#include "sync_dbapidef.h"

#ifdef __cplusplus
  #include <string>
  using namespace std;
#endif


#ifdef __cplusplus
  namespace sysync {
#endif


#if defined MACOSX || defined _WIN32 || defined LINUX
  typedef sInt64 lineartime_t;
#else
  #error unknown platform
#endif


#ifdef __cplusplus
  const lineartime_t secondToLinearTimeFactor = 1000; // unit is milliseconds

  // get system current date/time
  lineartime_t utcNowAsLineartime();

  // convert timestamp to ISO8601 string representation
  sInt32 timeStampToISO8601( lineartime_t aTimeStamp, string &aString, bool dateOnly= false );
#endif


#ifdef __cplusplus
  } // namespace
#endif

#endif /* TIMEUTIL_H */
/* eof */
