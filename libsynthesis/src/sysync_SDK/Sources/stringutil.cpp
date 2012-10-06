/*
 *  File:         stringutil.cpp
 *
 *  Authors:      Lukas Zeller (luz@plan44.ch)
 *                Beat Forster (bfo@synthesis.ch)
 *
 *  C++ string utils
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 */

#include "target_options.h"

#ifndef WITHOUT_SAN_1_1

#if defined __MACH__ && !defined __GNUC__ /* used for va_list support */
  #include <mw_stdarg.h>
#else
  #include <stdarg.h>
#endif

#ifdef __GNUC__
  #include <stdio.h>
#else
  #ifndef _MSC_VER
    #include <ctype>
  #endif
#endif

#include "stringutil.h"

#ifdef __cplusplus
  namespace sysync {
#endif


// case insensitive strcmp, NULL allowed as empty string input
sInt16 strucmp( cAppCharP s1, cAppCharP s2, size_t len1, size_t len2 )
{
  // allow NULL as empty strings
  if (!s1) s1 = "";
  if (!s2) s2 = "";
  // s1>s2 : 1, s1==s2 : 0, s1<s2 : -1
  size_t i;
  // calc number of chars we must compare
  size_t len = len1==0 ? len2 : (len2==0 ? len1 : (len1>len2 ? len2 : len1));
  for (i=0; (!len || i<len) && *s1 && *s2; i++) {
    // while both strings have chars and not len reached
    if (toupper(*s1)!=toupper(*s2))
      return toupper(*s1)>toupper(*s2) ? 1 : -1; // different
    // next
    s1++;
    s2++;
  }
  // equal up to end of shorter string or reached len
  // - if both reached end or len -> equal
  if ( ((len1 ? i==len1 : false) || *s1==0) && ((len2 ? i==len2 : false) || *s2==0) ) return 0;
  // - not equal, longer string is larger
  //   (if not reached end of s1 or stopped before len1, s1 is longer
  //    but note than len1 can be longer than actual length of s1, so we
  //    must check for *s1 to make sure we have really not reached end of s1)
  return (len1 ? i<len1 && *s1 : *s1) ? 1 : -1;
} // strucmp


// returns number of C-string-escaped chars successfully converted to string
sInt16 CStrToStrAppend( cAppCharP aStr, string &aString, bool aStopAtQuoteOrCtrl, char ignore )
{
  unsigned char c;
  const char *p=aStr;
  while ((c=*p++)) {
    // check for escape
    if (c=='\\') {
      // escape, check next
      c=*p++;
      if (!c) break; // unfinished escape sequence is ignored
      switch (c) {
        case 't' : c=0x09; break; // TAB
        case 'n' : c=0x0A; break; // line feed
        case 'r' : c=0x0D; break; // carriage return
        case '\r':
        case '\n':
          // continued line, swallow line end
          c=0;
          break;
        case 'x' :
          // hex char
          c=0; uInt16 n=0;
          while (isxdigit(*p) && n<2) {
            c=(c<<4) | (*p>'9' ? toupper(*p)-'A'+0x0A : *p-'0');
            p++; n++;
          }
          break;
      }
      // c is the char to add
    }
    else if (aStopAtQuoteOrCtrl && (c=='"' || c<0x20)) {
      // terminating char is NOT consumed
      p--;
      break; // stop here
    }
    // otherwise, ignore any control characters
    else if (c<0x20 && c!=ignore)
      continue;
    // add it to the result
    if (c) aString+=c;
  }
  // return number of converted chars
  return p-aStr;
} // CStrToStrAppend


// returns number of string-escaped chars successfully converted to C-string
sInt16 StrToCStrAppend( cAppCharP aStr, string &aString, bool aAllow8Bit, char ignore )
{
  unsigned char c;
  const char *p=aStr;
  while ((c=*p++)) {
    // check for specials
    if      (c==ignore) aString+= c;
    else if (c==0x09)   aString+="\\t";
    else if (c==0x0A)   aString+="\\n";
    else if (c==0x0D)   aString+="\\r";
    else if (c==0x22)   aString+="\\\"";
    else if (c=='\\')   aString+="\\\\"; // escape the backslash as well
    else if (c<0x20 || c==0x7F || (!aAllow8Bit && c>=0x80)) {
      StringObjAppendPrintf(aString,"\\x%02hX",(uInt16)c);
    }
    else {
      // as is
      aString+=c;
    }
  }
  // return number of converted chars
  return p-aStr;
} // StrToCStrAppend



// old-style C-formatted output into string object
static void vStringObjPrintf( string &aStringObj, cAppCharP aFormat, bool aAppend, va_list aArgs )
{
  #ifndef NO_VSNPRINTF
  const size_t bufsiz=128;
  #else
  const size_t bufsiz=2048;
  #endif
  int actualsize;
  char buf[bufsiz];

  buf[0]='\0';
  char *bufP = NULL;
  if (!aAppend) aStringObj.erase();
  actualsize = vsnprintf(buf, bufsiz, aFormat, aArgs);

  #ifndef NO_VSNPRINTF
  if (actualsize>=(int)bufsiz) {
    // default buffer was too small, create bigger dynamic buffer
    bufP = new char[actualsize+1];
    actualsize = vsnprintf(bufP, actualsize+1, aFormat, aArgs);
    if (actualsize>0) {
      aStringObj += bufP;
    }
    delete bufP;
  }
  else
  #endif

  {
    // small default buffer was big enough, add it
    if (actualsize<0) return; // abort, error
    aStringObj += buf;
  }
} // vStringObjPrintf


// old-style C-formatted output appending into string object
void StringObjAppendPrintf( string &aStringObj, cAppCharP aFormat, ... )
{
  va_list args;

  va_start(args, aFormat);
  // now make the string
  vStringObjPrintf(aStringObj,aFormat,true,args);
  va_end(args);
} // StringObjAppendPrintf


// returns number of successfully converted chars
sInt16 HexStrToULong( cAppCharP aStr, uInt32 &aLong, sInt16 aMaxDigits )
{
  // our own implementation
  char c;
  sInt16 n=0;
  aLong=0;

  while (aStr && (c=*aStr++) && (n<aMaxDigits)) {
    if (!isxdigit(c)) break;
    aLong*= 16;
    aLong+=(toupper(c)-0x30);
    if    (!isdigit(c)) aLong-=7;
    n++;
  } // while

  return n;
} // HexStrToUShort


// returns number of successfully converted chars
sInt16 HexStrToULongLong( cAppCharP aStr, uInt64 &aLongLong, sInt16 aMaxDigits )
{
  // our own implementation
  char c;
  sInt16 n=0;
  aLongLong=0;

  while (aStr && (c=*aStr++) && (n<aMaxDigits)) {
    if (!isxdigit(c)) break;
    aLongLong*= 16;
    aLongLong+=(toupper(c)-0x30);
    if    (!isdigit(c)) aLongLong-=7;
    n++;
  } // while

  return n;
} // HexStrToULongLong


sInt16 HexStrToUIntPtr( cAppCharP aStr, uIntPtr &aIntPtr, sInt16 aMaxDigits )
{
  #if __WORDSIZE == 64
    return HexStrToULongLong( aStr, aIntPtr, aMaxDigits );
  #else
    uInt32                            v= (uInt32)aIntPtr;
    sInt16 rslt= HexStrToULong( aStr, v, aMaxDigits );
    aIntPtr=                 (uIntPtr)v;
    
    return rslt;
  #endif
} // HexStrToUIntPtr

// helper
static bool isMinus(const char *&aStr, sInt16 &aNumRead)
{
  if (!aStr) return false; // not minus
  if (*aStr=='-') {
    aStr++;
    aNumRead++;
    return true; // is minus
  }
  else if (*aStr=='+') {
    aStr++;
    aNumRead++;
  }
  // is plus
  return false;
} // isMinus

// returns number of successfully converted chars
sInt16 StrToUShort( cAppCharP aStr, uInt16 &aShort, sInt16 aMaxDigits)
{
  // our own implementation
  char c;
  sInt16 n=0;
  aShort=0;
  while (aStr && (c=*aStr++) && (n<aMaxDigits)) {
    if (!isdigit(c)) break;
    aShort*=10;
    aShort+=(c-0x30);
    n++;
  }
  return n;
} // StrToUShort

// returns number of successfully converted chars
sInt16 StrToULong(const char *aStr, uInt32 &aLong, sInt16 aMaxDigits)
{
  // our own implementation
  char c;
  sInt16 n=0;
  aLong=0;
  while (aStr && (c=*aStr++) && (n<aMaxDigits)) {
    if (!isdigit(c)) break;
    aLong*=10l;
    aLong+=(c-0x30);
    n++;
  }
  return n;
} // StrToULong

// returns number of successfully converted chars
sInt16 StrToLong(const char *aStr, sInt32 &aLong, sInt16 aMaxDigits)
{
  // our own implementation
  uInt32 temp;
  sInt16 n=0;
  bool neg=isMinus(aStr,n);
  n+=StrToULong(aStr,temp,aMaxDigits-n);
  if (neg) aLong=-(sInt32)temp;
  else aLong=temp;
  return n;
} // StrToLong


#ifdef __cplusplus
  } // namespace
#endif

#endif //WITHOUT_SAN_1_1
/* eof */

