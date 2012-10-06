/*
 *  File:         stringutils.h
 *
 *  Authors:      Lukas Zeller (luz@plan44.ch)
 *                Beat Forster (bfo@synthesis.ch)
 *
 *  C++ string utils
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2002-05-01 : luz : extracted from sysync_utils
 */

#ifndef STRINGUTILS_H
#define STRINGUTILS_H

#include <string>
#ifdef __EPOC_OS__
  #include <stdio.h>
  #include <string.h>
#else
  #include <cstdio>
  #include <cstring>
#endif
#include <ctype.h>

#ifdef ANDROID
  #include <stdarg.h>
#endif

#include "generic_types.h"

using namespace std;

namespace sysync {

// PALM OS specifics
#ifdef __PALM_OS__
  // StrVPrintF handles a subset of ANSI vsprintf, but normal cases are available
  #define vsprintf StrVPrintF
  #define sprintf StrPrintF
  // PalmOS 3.5 does not have strncmp, use our own enhanced strnncmp
  #define strncmp sysync::strnncmp
  // PalmOS does not have vnsprintf, we need to simulate them
  #define NO_VSNPRINTF
  #define NO_SNPRINTF
#endif

#if defined(WINCE) || defined(_MSC_VER) || defined(__EPOC_OS__)
  // WinCE and Symbian do not have vsnprintf, as well as Visual Studio
  #define NO_VSNPRINTF
  #define NO_SNPRINTF
#endif

#ifdef NO_VSNPRINTF
int vsnprintf(char *s, int sz, const char *formatStr, va_list arg);
#endif
#ifdef NO_SNPRINTF
int snprintf(char *s, int sz, const char *formatStr, ...);
#endif


/// @brief Return passed pointer as C string, if pointer is NULL, empty string is returned
const char *alwaysCStr(const char *aStr);

// Assign char ptr to string object
// NOTE: NULL is allowed and means empty string
void AssignString(string &aString, const char *aCharP);
// Assign char ptr to char array
// NOTE: NULL is allowed and means empty string
void AssignCString(char *aCStr, const char *aCharP, size_t aLen);
// trim leading spaces/ctrls and everything including and after the next space/ctrl
// NOTE: NULL is allowed and means empty string
void AssignTrimmedCString(char *aCStr, const char *aCharP, size_t aLen);

// standard mangling increment
#define SYSER_NAME_MANGLEINC 43
// save mangled into C-string buffer
uInt16 assignMangledToCString(appCharP aCString, cAppCharP aCode, uInt16 aMaxBytes, bool aIsName, uInt8 aMangleInc=SYSER_NAME_MANGLEINC, cAppCharP aSecondKey=NULL);
// get unmangled into string
void getUnmangled(string &aString, cAppCharP aMangled, uInt16 aMaxChars=100, uInt8 aMangleInc=SYSER_NAME_MANGLEINC, cAppCharP aSecondKey=NULL);
// get unmangled into buffer
void getUnmangledAsBuf(appCharP aBuffer, uInt16 aBufSize,  cAppCharP aMangled, uInt8 aMangleInc=SYSER_NAME_MANGLEINC, cAppCharP aSecondKey=NULL);


// Note: NULL allowed as empty string input for struXXX
// case insensitive and whitespace trimming strcmp
sInt16 strutrimcmp(const char *s1, const char *s2);
// case insensitive strcmp which allow wildcards ? and * in s2, NULL allowed as empty string input
sInt16 strwildcmp(const char *s1, const char *s2, size_t len1=0, size_t len2=0);
// case insensitive strpos, returns -1 if not found
sInt32 strupos(const char *s, const char *pat, size_t slen=0, size_t patlen=0);
// case insensitive strcmp
sInt16 strucmp(const char *s1, const char *s2, size_t len1=0, size_t len2=0);
// byte by byte strcmp
// Note: is compatible with standard strncmp if len2 is left unspecified
sInt16 strnncmp(const char *s1, const char *s2, size_t len1=0, size_t len2=0);
// testing
#ifdef SYNTHESIS_UNIT_TEST
bool test_strwildcmp(void);
#endif


// parser for tag="value" type string format
cAppCharP nextTag(cAppCharP aTagString, string &aTag, string &aTagValue);
// direct conversion of value (usually from nextTag()) into uInt16
uInt16 uint16Val(cAppCharP aValue);
// direct conversion of value (usually from nextTag()) into uInt32
uInt32 uint32Val(cAppCharP aValue);
// direct conversion of value (usually from nextTag()) into bool
bool boolVal(cAppCharP aValue);


// returns true on successful conversion of string to bool
bool StrToBool(const char *aStr, bool &aBool);
// returns true on successful conversion of string to enum
bool StrToEnum(const char * const aEnumNames[], sInt16 aNumEnums, sInt16 &aShort, const char *aStr, sInt16 aLen=0);

const char *tristateString(sInt8 aTristate);
const char *boolString(bool aBool);


// return number of successfully converted chars
// Note: despite the names these functions take *fixed-size* integers
sInt16 StrToUShort(const char *aStr, uInt16 &aShort, sInt16 aMaxDigits=100);
sInt16 StrToShort(const char *aStr, sInt16 &aShort, sInt16 aMaxDigits=100);
sInt16 StrToULong(const char *aStr, uInt32 &aLong, sInt16 aMaxDigits=100);
sInt16 StrToLong(const char *aStr, sInt32 &aLong, sInt16 aMaxDigits=100);
sInt16 StrToULongLong(const char *aStr, uInt64 &aLongLong, sInt16 aMaxDigits=100);
sInt16 StrToLongLong(const char *aStr, sInt64 &aLongLong, sInt16 aMaxDigits=100);
sInt16 HexStrToUShort(const char *aStr, uInt16 &aShort, sInt16 aMaxDigits=100);
sInt16 HexStrToULong(const char *aStr, uInt32 &aLong, sInt16 aMaxDigits=100);
sInt16 HexStrToULongLong(const char *aStr, uInt64 &aLongLong, sInt16 aMaxDigits=100);
sInt16 HexStrToUIntPtr(const char *aStr, uIntPtr &aIntPtr, sInt16 aMaxDigits=100);
#ifndef NO_FLOATS
sInt16 StrToDouble(const char *aStr, double &aDouble, sInt16 aMaxDigits=100);
#endif

sInt16 CStrToStrAppend(const char *aStr, string &aString, bool aStopAtQuoteOrCtrl=false, char ignore='\0');
sInt16 StrToCStrAppend(const char *aStr, string &aString, bool aAllow8Bit=false, char ignore='\0');

// makes hex char out of nibble
char NibbleToHexDigit(uInt8 aNibble);
// add a hex byte to a string
void AppendHexByte(string &aString, uInt8 aByte);

// old-style C-formatted output into string object
void vStringObjPrintf(string &aStringObj, const char *aFormat, bool aAppend, va_list aArgs);

// old-style C-formatted output into string object
void StringObjPrintf(string &aStringObj, const char *aFormat, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 2, 3)))
#endif
    ;
void StringObjAppendPrintf(string &aStringObj, const char *aFormat, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 2, 3)))
#endif
    ;

} // namespace sysync

#endif // STRINGUTILS_H
