/*
 *  File:         stringutil.h
 *
 *  Authors:      Lukas Zeller (luz@plan44.ch)
 *                Beat Forster (bfo@synthesis.ch)
 *
 *  C++ string utils
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef STRINGUTIL_H
#define STRINGUTIL_H

#include <string>
#include "target_options.h"
#include "generic_types.h"

using namespace std;

#ifdef __cplusplus
  namespace sysync {
#endif


// case insensitive strcmp, NULL allowed as empty string input
sInt16 strucmp( cAppCharP s1, cAppCharP s2, size_t len1=0, size_t len2=0 );

// returns number of C-string-escaped chars successfully converted to string
sInt16 CStrToStrAppend( cAppCharP aStr, string &aString, bool aStopAtQuoteOrCtrl=false,
                                                         char ignore='\0' );

// returns number of string-escaped chars successfully converted to C-string
sInt16 StrToCStrAppend( cAppCharP aStr, string &aString, bool aAllow8Bit=false,
                                                         char ignore='\0' );

// old-style C-formatted output into string object
void   StringObjAppendPrintf( string &aStringObj, cAppCharP aFormat, ... );

sInt16    StrToULong    ( cAppCharP aStr, uInt32  &aLong,     sInt16 aMaxDigits= 100 );
sInt16    StrToLong     ( cAppCharP aStr, sInt32  &aLong,     sInt16 aMaxDigits= 100 );
sInt16    StrToUShort   ( cAppCharP aStr, uInt16  &aShort,    sInt16 aMaxDigits= 100 );
sInt16 HexStrToULong    ( cAppCharP aStr, uInt32  &aLong,     sInt16 aMaxDigits= 100 );
sInt16 HexStrToULongLong( cAppCharP aStr, uInt64  &aLongLong, sInt16 aMaxDigits= 100 );
sInt16 HexStrToUIntPtr  ( cAppCharP aStr, uIntPtr &aIntPtr,   sInt16 aMaxDigits= 100 );


#ifdef __cplusplus
  } // namespace
#endif

#endif // STRINGUTIL_H
