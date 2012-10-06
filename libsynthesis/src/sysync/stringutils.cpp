/*
 *  File:         stringutils.cpp
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

#include "prefix_file.h"
#include "stringutils.h"

// Consistent support for Linux, MacOSX CW & XCode
#if defined __GNUC__ || defined _MSC_VER
  #include <ctype.h>
#else
  #include <ctype>
#endif

#include <cstdio>
#include <cstring>
#include <stdarg.h>

using namespace std;

namespace sysync {



#ifdef NO_VSNPRINTF
// dummy (unsafe!!) map to vsprintf
int vsnprintf(char *s, int sz, const char *formatStr, va_list arg)
{
  return vsprintf(s,formatStr,arg);
} // vsnprintf
#endif

#ifdef NO_SNPRINTF
// own implementation (possibly unsafe, if vsnprintf is dummy)
int snprintf(char *s, int sz, const char *formatStr, ...)
{
  va_list args;

  va_start(args, formatStr);
  // now make the string
  int n=vsnprintf(s,sz,formatStr,args);
  va_end(args);
  return n;
} // snprintf
#endif


/// @brief Return passed pointer as C string, if pointer is NULL, empty string is returned
/// @param[in] aStr a C string or NULL
/// @return if aStr is NULL, returns empty string, aStr otherwise
const char *alwaysCStr(const char *aStr)
{
  return aStr ? aStr : "";
} // alwaysCStr


// Assign char ptr to string object
// NOTE: NULL is allowed and means empty string
void AssignString(string &aString, const char *aCharP)
{
  if (aCharP)
    aString=aCharP;
  else
    aString.erase();
} // AssignString


// Assign char ptr to char array of size aLen (note that we always leave room for terminator)
// NOTE: NULL is allowed and means empty string
void AssignCString(char *aCStr, const char *aCharP, size_t aLen)
{
  if (aCharP) {
    strncpy(aCStr,aCharP,aLen-1);
    aCStr[aLen-1]=0; // make sure it is terminated
  }
  else
    aCStr[0]=0;
} // AssignCString


// trim leading spaces/ctrls and everything including and after the next space/ctrl
void AssignTrimmedCString(char *aCStr, const char *aCharP, size_t aLen)
{
  if (aCharP) {
    // not empty, find first non-control/space
    while (*aCharP && (uInt8)(*aCharP)<=0x20) aCharP++;
    // now find end
    const char *e = aCharP;
    while (*e && (uInt8)(*e)>0x20) e++;
    // copy valid part of URL
    size_t n=e-aCharP;
    if (n>aLen-1) n=aLen-1; // limit to max buffer size
    strncpy(aCStr,aCharP,n);
    aCStr[n]=0; // make sure it is terminated
  }
  else
    aCStr[0]=0;
} // AssignTrimmedCString



// save mangled into C-string buffer
uInt16 assignMangledToCString(char *aCString, const char *aCode, uInt16 aMaxBytes, bool aIsName, uInt8 aMangleInc, const char *aSecondKey)
{
  uInt8 mangler=aMangleInc;
  uInt16 b,bytes=0;
  uInt8 c;
  const char *kp = aSecondKey && *aSecondKey ? aSecondKey : NULL; // need at least once char of key
  while(aCode && *aCode && bytes<aMaxBytes-1) {
    c = *(aCode++);
    if ((aIsName && (c>=0x20)) || (!aIsName && isalnum(c))) {
      // save only visible chars and spaces from name and alphanums from code
      *(aCString++)=
        c ^ mangler ^ (kp ? *(kp++) : 0); // add mangled
      if (kp && *kp==0) kp=aSecondKey; // wrap around
      mangler+=aMangleInc;
      bytes++;
    }
  }
  // add terminator in all cases
  *aCString++=mangler ^ (kp ? *(kp++) : 0);
  if (kp && *kp==0) kp=aSecondKey; // wrap around
  bytes++;
  // now add some garbage to disguise length of actual data
  b=bytes;
  while (b<aMaxBytes) {
    *aCString++ = b++ ^ mangler;
    mangler+=aMangleInc;
  }
  // return actual number of bytes (without disguising garbage)
  return bytes;
} // assignMangledToCString


// get unmangled into string
void getUnmangled(string &aString, const char *aMangled, uInt16 aMaxChars, uInt8 aMangleInc, const char *aSecondKey)
{
  uInt8 mangler=aMangleInc;
  uInt8 c;
  const char *kp = aSecondKey && *aSecondKey ? aSecondKey : NULL; // need at least once char of key
  aString.erase();
  while (aMaxChars-- > 0 && aMangled && (c=((uInt8)(*aMangled) ^ mangler ^ (kp ? *(kp++) : 0)))) {
    aString += (char)c;
    if (kp && *kp==0) kp=aSecondKey; // wrap around
    aMangled++;
    mangler+=aMangleInc;
  }
} // getUnmangled


// get unmangled into buffer
void getUnmangledAsBuf(appCharP aBuffer, uInt16 aBufSize,  cAppCharP aMangled, uInt8 aMangleInc, cAppCharP aSecondKey)
{
  uInt8 mangler=aMangleInc;
  uInt8 c;
  if (!aBuffer || aBufSize<1) return; // no buffer
  const char *kp = aSecondKey && *aSecondKey ? aSecondKey : NULL; // need at least once char of key
  aBuffer[--aBufSize]=0; // ultimate security terminator
  while (aBufSize-- > 0 && aMangled && (c=((uInt8)(*aMangled) ^ mangler ^ (kp ? *(kp++) : 0)))) {
    *(aBuffer++) = (char)c;
    if (kp && *kp==0) kp=aSecondKey; // wrap around
    aMangled++;
    mangler+=aMangleInc;
  }
  *(aBuffer) = 0; // terminate
} // getUnmangled



// case insensitive and whitespace trimming strcmp, NULL allowed as empty string input
sInt16 strutrimcmp(const char *s1, const char *s2)
{
  // skip whitespace on both strings first
  while (s1 && *s1 && isspace(*s1)) ++s1;
  while (s2 && *s2 && isspace(*s2)) ++s2;
  // count number of non-whitespaces
  sInt32 n1=0; while (s1 && s1[n1] && !isspace(s1[n1])) ++n1;
  sInt32 n2=0; while (s2 && s2[n2] && !isspace(s2[n2])) ++n2;
  // now compare
  return strucmp(s1,s2,n1,n2);
} // strutrimcmp


// case insensitive strcmp which allow wildcards ? and * in s2, NULL allowed as empty string input
sInt16 strwildcmp(const char *s1, const char *s2, size_t len1, size_t len2)
{
  // allow NULL as empty strings
  if (!s1) s1 = "";
  if (!s2) s2 = "";
  // s1>s2 : 1, s1==s2 : 0, s1<s2 : -1
  size_t i,j; // size_t instead of sInt32: BCPPB
  // calc number of chars we must compare
  size_t len = len1==0 ? len2 : (len2==0 ? len1 : (len1>len2 ? len2 : len1));
  for (i=0; (!len || i<len) && *s1 && *s2; i++) {
    // while both strings have chars and not len reached
    if (*s2=='*') {
      // zero to arbitrary number of chars
      s2++;
      j=0;
      if (*s2==0) return 0; // if asterisk is last in pattern, match is complete
      while (*s1) {
        // more pattern follows, recurse to compare the rest
        if (strwildcmp(s1,s2,len1 ? len1-i-j : 0,len2 ? len2-i-1 : 0)==0) {
          // rest matches now -> full match
          return 0;
        }
        // next attempt
        s1++;
        j++;
      }
      // no match if we get this far
      return 1;
    }
    else if (*s2!='?' && toupper(*s1)!=toupper(*s2))
      return toupper(*s1)>toupper(*s2) ? 1 : -1; // different and no wildcard in pattern at this place
    // next
    s1++;
    s2++;
  }
  // equal up to end of shorter string or reached len
  // - if both reached end or len
  if (len1 ? i==len1 : *s1==0) {
    // s1 has reached end, check special case that pattern ends with *, this would be ok
    if ((len==0 || i<len2) && *s2=='*') { s2++; i++; } // simply consume asterisk, if s2 is now at end as well, that's ok
    if (len2 ? i==len2 : *s2==0) return 0; // match only if s2 is now at end as well
  }
  // - not equal, longer string is larger
  //   (if not reached end of s1 or stopped before len1, s1 is longer)
  return (len1 ? i<len1 : *s1) ? 1 : -1;
} // strwildcmp


#ifdef SYNTHESIS_UNIT_TEST
bool test_strwildcmp(void)
{
  bool ok=true;
  int i;
  UNIT_TEST_CALL(i=strwildcmp("http://test.com/test","http*://test.com/test"),("i=%d",i),i==0,ok);
  UNIT_TEST_CALL(i=strwildcmp("https://test.com/test","http*://test.com/test"),("i=%d",i),i==0,ok);
  UNIT_TEST_CALL(i=strwildcmp("htt://test.com/test","http*://test.com/test"),("i=%d",i),i!=0,ok);
  UNIT_TEST_CALL(i=strwildcmp("http://test.com/test/10000","http*://test.com/test*"),("i=%d",i),i==0,ok);
  UNIT_TEST_CALL(i=strwildcmp("http://test.com/test","http*://test.com/test*"),("i=%d",i),i==0,ok);
  UNIT_TEST_CALL(i=strwildcmp("realGARBAGE","real"),("i=%d",i),i!=0,ok);
  UNIT_TEST_CALL(i=strwildcmp("realGARBAGE","real",4),("i=%d",i),i==0,ok);
  UNIT_TEST_CALL(i=strwildcmp("realGARBAGE","realTRASH",4,4),("i=%d",i),i==0,ok);
  UNIT_TEST_CALL(i=strwildcmp("one_two_two2_four_five","one*two2*five"),("i=%d",i),i==0,ok);
  UNIT_TEST_CALL(i=strwildcmp("one_two_two2_four_five","one*two2*five",12),("i=%d",i),i!=0,ok);
  UNIT_TEST_CALL(i=strwildcmp("one_two_two2_four_five","one*two2*five",12,8),("i=%d",i),i==0,ok);
  UNIT_TEST_CALL(i=strwildcmp("one_two_two2_four_five","one*two2*five",12,9),("i=%d",i),i==0,ok);
  UNIT_TEST_CALL(i=strwildcmp("one_two_two2_four_five","one*two2*five",12,10),("i=%d",i),i!=0,ok);
  UNIT_TEST_CALL(i=strwildcmp("P800","P?00"),("i=%d",i),i==0,ok);
  UNIT_TEST_CALL(i=strwildcmp("P810","P?00"),("i=%d",i),i!=0,ok);
  return ok;
}
#endif


// case insensitive strpos, returns -1 if not found
sInt32 strupos(const char *s, const char *pat, size_t slen, size_t patlen)
{
  size_t i,k;
  for (i=0; (!slen || i<slen) && s && *s; i++) {
    for (k=0; (!patlen || k<patlen) && pat && *pat; k++) {
      if (*(s+k)==0 || (slen && i+k>=slen))
        return -1; // pattern too long to possibly match at all
      if (toupper(*(s+k))!=toupper(*pat))
        break; // no match at this position
      pat++;
    }
    if (*pat==0 || (patlen && k>=patlen))
      return i; // pattern end reached and all matches so far -> found string here
    // try next position
    s++;
  }
  return -1; // not found in any position
} // strupos


// case insensitive strcmp, NULL allowed as empty string input
sInt16 strucmp(const char *s1, const char *s2, size_t len1, size_t len2)
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


// byte by byte strcmp, NULL allowed as empty string input
// Note: is compatible with standard strncmp if len2 is left unspecified
sInt16 strnncmp(const char *s1, const char *s2, size_t len1, size_t len2)
{
  // allow NULL as empty strings
  if (!s1) s1 = "";
  if (!s2) s2 = "";
  // s1>s2 : 1, s1==s2 : 0, s1<s2 : -1
  size_t i; // size_t instead of sInt32: BCPPB
  // calc number of chars we must compare
  size_t len = len1==0 ? len2 : (len2==0 ? len1 : (len1>len2 ? len2 : len1));
  for (i=0; (!len || i<len) && *s1 && *s2; i++) {
    // while both strings have chars and not len reached
    if (*s1!=*s2)
      return *s1>*s2 ? 1 : -1; // different
    // next
    s1++;
    s2++;
  }
  // equal up to end of shorter string or reached len
  // - if both reached end or len
  if ( ((len1 ? i==len1 : false) || *s1==0) && ((len2 ? i==len2 : false) || *s2==0) ) return 0;
  // - not equal, longer string is larger
  //   (if not reached end of s1 or stopped before len1, s1 is longer
  //    but note than len1 can be longer than actual length of s1, so we
  //    must check for *s1 to make sure we have really not reached end of s1)
  return (len1 ? i<len1 && *s1 : *s1) ? 1 : -1;
} // strnncmp


// parser for tag="value" type string format
cAppCharP nextTag(cAppCharP aTagString, string &aTag, string &aTagValue)
{
  size_t n;
  if (!aTagString) return NULL;
  // find start of next tag
  while (*aTagString==' ' || *aTagString==',') aTagString++;
  // find end of tag
  cAppCharP p = strchr(aTagString,'=');
  if (!p) return NULL; // no more tags
  n=p-aTagString; // size of tag
  // save tag name
  aTag.assign(aTagString,n);
  // check for value
  aTagValue.erase(); // default to none
  p++; // skip '='
  if (*p==0) return NULL; // no more tags
  // must start with single or double quote
  char q=*p; // quote char
  if (q!=0x22 && q!='\'') return p; // no value, next tag follows immediately
  p++; // skip starting quote
  while (*p) {
    if (*p=='\\') {
      // escape next char
      p++;
      if (*p==0) break; // no next char, string ends
    }
    else if (*p==q) {
      // end of string (unescaped closing quote)
      p++; // skip closing quote
      break;
    }
    // just take char as it is
    aTagValue+=*p++;
  }
  if (*p==';') p++; // skip semicolon if there is one
  if (*p==0) return NULL; // end of string, no more tags
  // p is now starting point for next tag
  return p;
} // nextTag


// direct conversion of value (usually from nextTag()) into uInt32
uInt32 uint32Val(cAppCharP aValue)
{
  uInt32 res=0;
  StrToULong(aValue, res);
  return res;
} // uint32Val


// direct conversion of value (usually from nextTag()) into uInt16
uInt16 uint16Val(cAppCharP aValue)
{
  uInt16 res=0;
  StrToUShort(aValue, res);
  return res;
} // uint16val


// direct conversion of value (usually from nextTag()) into bool
bool boolVal(cAppCharP aValue)
{
  bool res=false;
  StrToBool(aValue, res);
  return res;
} // boolVal



// returns true on successful conversion of string to bool
bool StrToBool(const char *aStr, bool &aBool)
{
  if (
    strutrimcmp(aStr,"yes")==0 ||
    strutrimcmp(aStr,"true")==0 ||
    strutrimcmp(aStr,"1")==0 ||
    strutrimcmp(aStr,"on")==0
  ) {
    aBool=true;
    return true;
  }
  else if (
    strutrimcmp(aStr,"no")==0 ||
    strutrimcmp(aStr,"false")==0 ||
    strutrimcmp(aStr,"0")==0 ||
    strutrimcmp(aStr,"off")==0
  ) {
    aBool=false;
    return true;
  }
  // error
  return false;
} // StrToBool


const char *tristateString(sInt8 aTristate)
{
  if (aTristate<0) return "default";
  else if (aTristate>0) return "Yes";
  else return "No";
}


const char *boolString(bool aBool)
{
  if (aBool) return "Yes";
  else return "No";
}




// returns true on successful conversion of string to enum
bool StrToEnum(const char * const aEnumNames[], sInt16 aNumEnums, sInt16 &aShort, const char *aStr, sInt16 aLen)
{
  const char *e;
  for (sInt16 k=0; k<aNumEnums; k++) {
    e=aEnumNames[k];
    if (!e) continue; // NULL entries are allowed in aEnumNames but will be skipped
    if (strucmp(aStr,e,aLen)==0) {
      aShort=k;
      return true;
    }
  }
  return false; // not found
} // StrToEnum


// makes hex char out of nibble
char NibbleToHexDigit(uInt8 aNibble)
{
  aNibble &= 0x0F;
  if (aNibble>9) return 'A'-0x0A+aNibble;
  else return '0'+aNibble;
} // NibbleToHexDigit


// returns number of successfully converted chars
sInt16 HexStrToUShort(const char *aStr, uInt16 &aShort, sInt16 aMaxDigits)
{
  // our own implementation
  char c;
  sInt16 n=0;
  aShort=0;
  //firstly check aMaxDigits to avoid accessing invalid value of 'aStr'
  //This will cause a memory warning checked by valgrind
  while ((n<aMaxDigits) && aStr && (c=*aStr++)) {
    if (!isxdigit(c)) break;
    aShort<<=4;
    aShort+=(toupper(c)-0x30);
    if (!isdigit(c)) aShort-=7;
    n++;
  }
  return n;
} // HexStrToUShort


// returns number of successfully converted chars
sInt16 HexStrToULong(const char *aStr, uInt32 &aLong, sInt16 aMaxDigits)
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
} // HexStrToULong


// returns number of successfully converted chars
sInt16 HexStrToULongLong(const char *aStr, uInt64 &aLongLong, sInt16 aMaxDigits)
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

sInt16 HexStrToUIntPtr(const char *aStr, uIntPtr &aIntPtr, sInt16 aMaxDigits)
{
#if __WORDSIZE == 64
  return HexStrToULongLong(aStr, aIntPtr, aMaxDigits);
#else
  uInt32 l;
  sInt16 n = HexStrToULong(aStr, l, aMaxDigits);
  aIntPtr = (uIntPtr)l;
  return n;
#endif
} // HexStrToUIntPtr

// returns number of successfully converted chars
sInt16 StrToUShort(const char *aStr, uInt16 &aShort, sInt16 aMaxDigits)
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
sInt16 StrToULongLong(const char *aStr, uInt64 &aLongLong, sInt16 aMaxDigits)
{
  // our own implementation
  char c;
  sInt16 n=0;
  aLongLong=0;
  while (aStr && (c=*aStr++) && (n<aMaxDigits)) {
    if (!isdigit(c)) break;
    aLongLong*=10l;
    aLongLong+=(c-0x30);
    n++;
  }
  return n;
} // StrToULongLong


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
sInt16 StrToShort(const char *aStr, sInt16 &aShort, sInt16 aMaxDigits)
{
  // our own implementation
  uInt16 temp;
  sInt16 n=0;
  bool neg=isMinus(aStr,n);
  n+=StrToUShort(aStr,temp,aMaxDigits-n);
  if (neg) aShort=-temp;
  else aShort=temp;
  return n;
} // StrToShort


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


// returns number of successfully converted chars
sInt16 StrToLongLong(const char *aStr, sInt64 &aLongLong, sInt16 aMaxDigits)
{
  // our own implementation
  uInt64 temp;
  sInt16 n=0;
  bool neg=isMinus(aStr,n);
  n+=StrToULongLong(aStr,temp,aMaxDigits-n);
  if (neg) aLongLong=-(sInt64)temp;
  else aLongLong=temp;
  return n;
} // StrToLongLong


#ifndef NO_FLOATS

// returns number of successfully converted chars
sInt16 StrToDouble(const char *aStr, double &aDouble, sInt16 aMaxDigits)
{
  // our own implementation
  char c;
  sInt16 n=0;
  bool neg=isMinus(aStr,n);
  sInt64 fraction=0;
  aDouble=0;
  while (aStr && (c=*aStr++) && (n<aMaxDigits)) {
    if (!isdigit(c)) {
      if (fraction || c!='.') break;
      fraction=10; // first digit after decimal point is 1/10
      n++;
      continue; // next
    }
    c-=0x30; // make 0..9
    if (fraction) {
      // within fractional part
      aDouble+=(double)c/fraction;
      fraction*=10;
    }
    else {
      // integer part
      aDouble*=10.0;
      aDouble+=c;
    }
    n++;
  }
  if (neg) aDouble=-aDouble;
  return n;
} // StrToDouble

#endif // NO_FLOATS


// returns number of C-string-escaped chars successfully converted to string
sInt16 CStrToStrAppend(const char *aStr, string &aString, bool aStopAtQuoteOrCtrl, char ignore)
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



// add a hex byte to a string
// Note: this is required because PalmOS sprintf does not correctly
//       work with %02X (always writes 4 digits!)
void AppendHexByte(string &aString, uInt8 aByte)
{
  aString += NibbleToHexDigit(aByte>>4);
  aString += NibbleToHexDigit(aByte);
} // AppendHexByte


// returns number of C-string-escaped chars successfully converted to string
sInt16 StrToCStrAppend(const char *aStr, string &aString, bool aAllow8Bit, char ignore)
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
      aString += "\\x";
      AppendHexByte(aString,c);
    }
    else {
      // as is
      aString+=c;
    }
  }
  // return number of converted chars
  return p-aStr;
} // CStrToStrAppend



// old-style C-formatted output into string object
void vStringObjPrintf(string &aStringObj, const char *aFormat, bool aAppend, va_list aArgs)
{
  #ifndef NO_VSNPRINTF
  const size_t bufsiz=128;
  #else
  const size_t bufsiz=2048;
  #endif
  ssize_t actualsize;
  char buf[bufsiz];

  buf[0]='\0';
  char *bufP = NULL;
  if (!aAppend) aStringObj.erase();
  #ifndef NO_VSNPRINTF
  // using aArgs in vsnprintf() is destructive, need a copy in
  // case we call the function a second time
  va_list args;
  va_copy(args, aArgs);
  #endif
  actualsize = vsnprintf(buf, bufsiz, aFormat, aArgs);
  #ifndef NO_VSNPRINTF
  if (actualsize>=(ssize_t)bufsiz) {
    // default buffer was too small, create bigger dynamic buffer
    bufP = new char[actualsize+1];
    actualsize = vsnprintf(bufP, actualsize+1, aFormat, args);
    if (actualsize>0) {
      aStringObj += bufP;
    }
    delete [] bufP;
  }
  else
  #endif
  {
    // small default buffer was big enough, add it
    if (actualsize<0) return; // abort, error
    aStringObj += buf;
  }
  #ifndef NO_VSNPRINTF
  va_end(args);
  #endif
} // vStringObjPrintf


// old-style C-formatted output into string object
void StringObjPrintf(string &aStringObj, const char *aFormat, ...)
{
  va_list args;

  va_start(args, aFormat);
  // now make the string
  vStringObjPrintf(aStringObj,aFormat,false,args);
  va_end(args);
} // StringObjPrintf


// old-style C-formatted output appending into string object
void StringObjAppendPrintf(string &aStringObj, const char *aFormat, ...)
{
  va_list args;

  va_start(args, aFormat);
  // now make the string
  vStringObjPrintf(aStringObj,aFormat,true,args);
  va_end(args);
} // StringObjAppendPrintf

} // namespace sysync

/* eof */

