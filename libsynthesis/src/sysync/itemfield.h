/*
 *  File:         itemfield.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TItemField
 *    Abstract class, holds a single field value
 *  TStringField, TIntegerField, TTelephoneField, TTimeStampField etc.
 *    Implementations of field types
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-08-08 : luz : created
 *
 */

#ifndef ItemField_H
#define ItemField_H

// includes
#include "sysync_globs.h"
#include "sysync_utils.h"

#include "engineinterface.h"

#include <string>

#ifdef ARRAYFIELD_SUPPORT
#include <vector>
#endif

namespace sysync {

extern cAppCharP const ItemFieldTypeNames[numFieldTypes];

extern const TPropDataTypes devInfPropTypes[numFieldTypes];


#ifndef NO64BITINT
typedef sInt64 fieldinteger_t;
#define StrToFieldinteger(s,i) StrToLongLong(s,i)
#else
typedef sInt32 fieldinteger_t;
#define StrToFieldinteger(s,i) StrToLong(s,i)
#endif

// our own implementation for dynamic casts for Items without needing RTTI
#define ITEMFIELD_DYNAMIC_CAST_PTR(ty,tyid,src) (src->isBasedOn(tyid) ? static_cast<ty *>(src) : NULL)


// basically abstract class, but can be used to represent EMPTY and ASSIGNED values
class TItemField
{
public:
  TItemField();
  virtual ~TItemField();
  #ifdef ARRAYFIELD_SUPPORT
  // check array
  virtual bool isArray(void) const { return false; }
  virtual TItemField *getArrayField(sInt16 aArrIdx, bool /* aExistingOnly */=false) { return aArrIdx==0 ? this : NULL; };
  virtual sInt16 arraySize(void) const { return 1; } // non-array has one element
  virtual TItemFieldTypes getElementType(void) const { return getType(); } // non array element is same as base type
  #else
  // non-virtual versions if we have no array fields at all
  bool isArray(void) const { return false; }
  TItemField *getArrayField(sInt16 aArrIdx, bool aExistingOnly=false) { return aArrIdx==0 ? this : NULL; };
  sInt16 arraySize(void) const { return 1; } // non-array has one element
  TItemFieldTypes getElementType(void) const { return getType(); }
  #endif
  // changelog support
  #if defined(CHECKSUM_CHANGELOG) && !defined(RECORDHASH_FROM_DBAPI)
  virtual uInt16 getDataCRC(uInt16 crc=0) { return crc; }; // base class is always empty
  #endif
  // access to type
  virtual TItemFieldTypes getType(void) const { return fty_none; } // no real type
  virtual TItemFieldTypes getCalcType(void) const { return getType(); };
  virtual bool isBasedOn(TItemFieldTypes aFieldType) const { return aFieldType==fty_none; };
  virtual bool elementsBasedOn(TItemFieldTypes aFieldType) const { return isBasedOn(aFieldType); };
  // dependency on a local ID
  virtual void setParentLocalID(cAppCharP /* aParentLocalID */) { /* nop */ };
  // access to field contents
  #ifdef STREAMFIELD_SUPPORT
  // - stream interface (default implementation accesses string representation of field)
  virtual void resetStream(size_t aPos=0);
  virtual size_t getStreamSize(void) { return getStringSize(); };
  virtual size_t getStreamPos(void) const { return fStreamPos; };
  virtual size_t readStream(void *aBuffer, size_t aMaxBytes);
  virtual size_t writeStream(void *aBuffer, size_t aNumBytes);
  void appendStream(void) { resetStream(getStreamSize()); };
  #endif
  virtual bool hasProxy(void) { return false; }; // normal fields don't have a proxy
  // - as string
  virtual void setAsString(cAppCharP /* aString */) { fAssigned=true; }; // basic setter, this one must be derived for all non-string descendants
  virtual void setAsString(cAppCharP aString, size_t aLen);
  virtual void setAsString(const string &aString) { setAsString(aString.c_str(),aString.size()); };
  virtual void getAsString(string &aString) { aString.erase(); }; // empty string by default
  virtual void appendToString(string &aString, size_t aMaxLen=0) { string s; getAsString(s); if (aMaxLen) aString.append(s,0,aMaxLen); else aString.append(s); }; // generic append
  virtual cAppCharP getCStr(void) { return NULL; } // only real strings can return CStr (used for PalmOS optimization)
  virtual void getAsNormalizedString(string &aString) { getAsString(aString); };
  virtual size_t getStringSize(void);
  // - as boolean (default: non-empty is true)
  virtual bool getAsBoolean(void) { return !isEmpty(); };
  virtual void setAsBoolean(bool aBool) { if (aBool) setAsString("1"); else assignEmpty(); };
  // - as integer
  virtual fieldinteger_t getAsInteger(void);
  virtual void setAsInteger(fieldinteger_t aInteger);
  // some string operations
  // - append string/char
  virtual void appendString(cAppCharP /* aString */, size_t /* aMaxChars */) { /* nop */ };
  void appendString(cAppCharP aString) { appendString(aString,strlen(aString)); };
  void appendString(const string &aString) { appendString(aString.c_str(),aString.size()); };
  void appendChar(const char aChar) { appendString(&aChar,1); };
  // - check if specified value is contained in myself
  virtual bool contains(TItemField &aItemField, bool aCaseInsensitive);
  // - find string in contents
  virtual sInt16 findInString(cAppCharP /* aString */, bool /* aCaseInsensitive */) { return -1; };
  // - check if specified field is shortened version of this one
  virtual bool isShortVers(TItemField & /* aItemField */, sInt32 /* aOthersMax */) { return false; };
  // - assignment
  virtual bool isAssigned(void) { return fAssigned; };
  bool isUnassigned(void) { return !isAssigned(); };
  virtual void unAssign(void) { fAssigned=false; };
  // empty is NOT always same as unassigned; e.g. for strings empty can be empty string assigned
  virtual bool isEmpty(void) { return getType()==fty_none ? true : isUnassigned(); }
  // - make assigned, but empty (explicit "" string assigned)
  virtual void assignEmpty(void) { fAssigned=true; };
  // assignment
  virtual TItemField& operator=(TItemField &aItemField);
  // field comparison
  bool operator==(TItemField &aItem) { return compareWith(aItem)==0; }
  bool operator!=(TItemField &aItem) { return compareWith(aItem)!=0; }
  bool operator>(TItemField &aItem) { return compareWith(aItem)>0; }
  bool operator>=(TItemField &aItem) { return compareWith(aItem)>=0; }
  bool operator<(TItemField &aItem) { return compareWith(aItem)<0; }
  bool operator<=(TItemField &aItem) { return compareWith(aItem)<=0; }
  // append (default to appending string value of other field)
  virtual void append(TItemField &aItemField);
  // merge fields
  virtual bool merge(TItemField & /* aItemField */, const char /* aSep */=0) { return false; /* nop */ };
  // compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem,
  // SYSYNC_NOT_COMPARABLE if not comparable at all or not equal and no ordering known
  virtual sInt16 compareWith(TItemField & /* aItemField */, bool =false /* aCaseInsensitive */) { return SYSYNC_NOT_COMPARABLE; };
  // debug support
  #ifdef SYDEBUG
  virtual size_t StringObjFieldAppend(string &s, uInt16 aMaxStrLen); // show field contents as string for debug output
  #endif
protected:
  // assigned flag
  bool fAssigned;
  #ifdef STREAMFIELD_SUPPORT
  // stream position
  size_t fStreamPos;
  #endif
}; // TItemField

typedef TItemField *TItemFieldP;



#ifdef ARRAYFIELD_SUPPORT

typedef std::vector<TItemField *> TFieldArray;

// array field, contains a list of fields of TItemFields
class TArrayField : public TItemField
{
  typedef TItemField inherited;
public:
  TArrayField(TItemFieldTypes aLeafFieldType, GZones *aGZonesP);
  virtual ~TArrayField();
  // check array
  virtual bool isArray(void) const { return true; }
  virtual TItemField *getArrayField(sInt16 aArrIdx, bool aExistingOnly=false);
  virtual sInt16 arraySize(void) const { return fArray.size(); } // return size of array
  // changelog support
  #if defined(CHECKSUM_CHANGELOG) && !defined(RECORDHASH_FROM_DBAPI)
  virtual uInt16 getDataCRC(uInt16 crc=0);
  #endif
  // access to type
  virtual TItemFieldTypes getType(void) const { return fty_none; } // array has no type
  virtual TItemFieldTypes getElementType(void) const { return fLeafFieldType; } // type of leaf fields (accessible even if array is empty)
  virtual bool elementsBasedOn(TItemFieldTypes aFieldType) const;
  // some string operations
  // - assignment
  virtual bool isAssigned(void) { return !isEmpty() || fAssigned; }; // empty, but explicitly assigned so is assigned as well
  virtual void unAssign(void); // clear all elements and clear fAssigned
  // - append string to array = append string as last element of array
  virtual void appendString(cAppCharP aString, size_t aMaxChars);
  // empty is array with no elements
  virtual bool isEmpty(void) { return arraySize()==0; };
  // - make assigned
  virtual void assignEmpty(void) { unAssign(); fAssigned = true; }; // assigning emptyness must make isAssigned true!
  // assignment
  virtual TItemField& operator=(TItemField &aItemField);
  // check if specified value is contained in myself
  virtual bool contains(TItemField &aItemField, bool aCaseInsensitive);
  // append (default to appending value of other field as a new array element)
  virtual void append(TItemField &aItemField);
  // merge fields
  virtual bool merge(TItemField & /* aItemField */, const char /* aSep */=0) { return false; /* nop */ };
  // compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem,
  // SYSYNC_NOT_COMPARABLE if not comparable at all or not equal and no ordering known
  virtual sInt16 compareWith(TItemField &aItemField, bool aCaseInsensitive=false);
  // debug support
  #ifdef SYDEBUG
  virtual size_t StringObjFieldAppend(string &s, uInt16); // show field contents as string for debug output
  #endif
protected:
  // type of contained leaf fields
  TItemFieldTypes fLeafFieldType;
  // first field, is instantiated with array to allow type comparisons
  TItemField *fFirstField;
  // Zones for fields
  GZones *fGZonesP;
  // actual field vector
  TFieldArray fArray;
}; // TArrayField
#endif


#ifndef STREAMFIELD_SUPPORT
  #define PULLFROMPROXY
  #define DELETEPROXY
  #define PROXYINSTALLED false
#else
  #define PULLFROMPROXY pullFromProxy()
  #define DELETEPROXY setBlobProxy(NULL)
  #define PROXYINSTALLED (fBlobProxyP!=NULL)

// forward
class TStringField;

// abstract Proxy object for Blob contents, allows loading a BLOB from DB at the moment it is
// actually required.
// Intended to be derived by database implementations
class TBlobProxy
{
public:
  TBlobProxy() { fUsage=1; } // initial creator also gets owner
  virtual ~TBlobProxy() {};
  // - returns size of entire blob
  virtual size_t getBlobSize(TStringField *aFieldP) = 0;
  // - read from Blob from specified stream position and update stream pos
  virtual size_t readBlobStream(TStringField *aFieldP, size_t &aPos, void *aBuffer, size_t aMaxBytes) = 0;
  // - dependency on a local ID
  virtual void setParentLocalID(cAppCharP aParentLocalID) { /* nop */ };
  // - usage control
  void link(void) { fUsage++; };
  static void unlink(TBlobProxy *&aProxyP) { if (aProxyP) { if (--(aProxyP->fUsage) == 0) delete aProxyP; aProxyP=NULL; } };
private:
  uInt16 fUsage;
}; // TBlobProxy

#endif


// string field.
// Note that a string can also be a string of binary data,
// not only a NUL-terminated string (as used in derived blob class)
class TStringField: public TItemField
{
  typedef TItemField inherited;
  friend class TBlobProxy;
public:
  TStringField();
  virtual ~TStringField();
  // access to type
  virtual TItemFieldTypes getType(void) const { return fty_string; };
  virtual bool isBasedOn(TItemFieldTypes aFieldType) const { return aFieldType==fty_string ? true : TItemField::isBasedOn(aFieldType); };
  // changelog support
  #if defined(CHECKSUM_CHANGELOG) && !defined(RECORDHASH_FROM_DBAPI)
  virtual uInt16 getDataCRC(uInt16 crc=0);
  #endif
  // assignment
  virtual TItemField& operator=(TItemField &aItemField);
  // access to normalized version of content
  virtual void getAsNormalizedString(string &aString);
  // access to field contents
  #ifdef STREAMFIELD_SUPPORT
  // - set blob loader proxy (ownership is passed to field)
  void setBlobProxy(TBlobProxy *aBlobProxyP);
  // - stream interface
  virtual size_t getStreamSize(void);
  virtual size_t readStream(void *aBuffer, size_t aMaxBytes);
  virtual size_t writeStream(void *aBuffer, size_t aNumBytes);
  virtual bool hasProxy(void) { return fBlobProxyP!=NULL; };
  // - stream field proxies actually can have dependencies on a local ID
  virtual void setParentLocalID(cAppCharP aParentLocalID) { if (fBlobProxyP) fBlobProxyP->setParentLocalID(aParentLocalID); };
  #endif
  // - as string
  virtual void setAsString(cAppCharP aString) { DELETEPROXY; if (aString) fString=aString; else fString.erase(); stringWasAssigned(); };
  virtual void setAsString(const string &aString) { DELETEPROXY; fString=aString; stringWasAssigned(); }; // works even if string contains NULs
  virtual void setAsString(cAppCharP aString, size_t aLen);
  virtual void getAsString(string &aString) { PULLFROMPROXY; aString=fString; };
  virtual void appendToString(string &aString, size_t aMaxLen=0) { PULLFROMPROXY; if (aMaxLen) aString.append(fString,0,aMaxLen); else aString.append(fString); };
  virtual cAppCharP getCStr(void) { PULLFROMPROXY; return fString.c_str(); } // string can return CStr (used for PalmOS optimization)
  virtual size_t getStringSize(void); // can cause proxied values to be retrieved, so use with care with BLOBS
  virtual void unAssign(void) { DELETEPROXY; fString.erase(); TItemField::unAssign(); };
  // empty test
  virtual bool isEmpty(void) { return isUnassigned() || getStringSize()==0; }
  virtual void assignEmpty(void) { DELETEPROXY; fString.erase(); TItemField::assignEmpty(); };
  // some string operations
  // - append string
  virtual void appendString(cAppCharP aString, size_t aMaxChars);
  // - check if specified field is shortened version of this one
  virtual bool isShortVers(TItemField &aItemField, sInt32 aOthersMax);
  // - check if String is contained in value and returns position
  virtual sInt16 findInString(cAppCharP aString, bool aCaseInsensitive=false);
  // merge fields
  virtual bool merge(TItemField &aItemField, const char aSep=0);
  // compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem
  // SYSYNC_NOT_COMPARABLE if not equal and no ordering known
  virtual sInt16 compareWith(TItemField &aItemField, bool aCaseInsensitive=false);
  #ifdef STREAMFIELD_SUPPORT
  void pullFromProxy(void);
  #endif
  // debug support
  #ifdef SYDEBUG
  virtual size_t StringObjFieldAppend(string &s, uInt16 aMaxStrLen);
  #endif
protected:
  virtual void stringWasAssigned(void) { fAssigned=true; }; // post-process string that was just assigned
  #ifdef STREAMFIELD_SUPPORT
  TBlobProxy *fBlobProxyP;
  #endif
  string fString;
}; // TStringField


// BLOB field. This is a string field but has many direct access functions
// disabled, contents should only be read using stream interface,
// comparing is not possible
class TBlobField: public TStringField
{
  typedef TStringField inherited;
public:
  TBlobField();
  virtual ~TBlobField();
  // access to type
  virtual TItemFieldTypes getType(void) const { return fty_blob; };
  virtual bool isBasedOn(TItemFieldTypes aFieldType) const { return aFieldType==fty_blob ? true : TStringField::isBasedOn(aFieldType); };
  // changelog support
  //  Note: uses inherited implementation of TStringField
  // assignment
  virtual TItemField& operator=(TItemField &aItemField);
  // - as string
  virtual void getAsString(string &aString) { StringObjPrintf(aString,"<BLOB size=%ld>", long(getStringSize())); }; // must be read with stream interface
  void getBlobAsString(string &aString) { TStringField::getAsString(aString); }; // must use this one to get as string
  // - as Pointer/Data
  void getBlobDataPtrSz(void *&aPtr, size_t &aSize) { aPtr=(void *)TStringField::getCStr(); aSize=TStringField::getStringSize(); };
  void setBlobDataPtrSz(void *aPtr, size_t aSize) { setAsString((cAppCharP)aPtr, aSize); };
  // - test
  virtual bool isEmpty(void) { return isUnassigned(); } // do not test for empty string as this would cause BLOB to be read
  // some string operations
  // - check if specified field is shortened version of this one
  virtual bool isShortVers(TItemField & /* aItemField */, sInt32 /* aOthersMax */) { return false; }; // cannot compare
  // - check if String is contained in value and returns position
  virtual sInt16 findInString(cAppCharP /* aString */, bool /* aCaseInsensitive */) { return -1; }; // cannot search
  // merge fields
  virtual bool merge(TItemField & /* aItemField */, const char /* aSep */=0) { return false; }; // cannot merge
  // compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem
  // SYSYNC_NOT_COMPARABLE if not equal and no ordering known
  virtual sInt16 compareWith(TItemField & /* aItemField */, bool /* aCaseInsensitive */) { return SYSYNC_NOT_COMPARABLE; }; // cannot compare
  // make contents and encoding/charset valid
  void makeContentsValid(void) { PULLFROMPROXY; };
  // debug support
  #ifdef SYDEBUG
  virtual size_t StringObjFieldAppend(string &s, uInt16 aMaxStrLen); // show field contents as string for debug output
  #endif
  // extra info about BLOB contents
  TEncodingTypes fHasEncoding;
  TEncodingTypes fWantsEncoding;
  TCharSets fCharset;
}; // TBlobField


// telephone number string field
// compares normalized version of number text (only +*# and digits)
class TTelephoneField: public TStringField
{
  typedef TStringField inherited;
public:
  TTelephoneField();
  virtual ~TTelephoneField();
  // access to type
  virtual TItemFieldTypes getType(void) const { return fty_telephone; };
  virtual TItemFieldTypes getCalcType(void) const { return fty_string; };
  virtual bool isBasedOn(TItemFieldTypes aFieldType) const { return aFieldType==fty_telephone ? true : TStringField::isBasedOn(aFieldType); };
  // assignment
  virtual TItemField& operator=(TItemField &aItemField) { return TStringField::operator=(aItemField); };
  // access to normalized version of content
  virtual void getAsNormalizedString(string &aString);
  // compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem
  // SYSYNC_NOT_COMPARABLE if not equal and no ordering known
  virtual sInt16 compareWith(TItemField &aItemField, bool aCaseInsensitive=false);
}; // TTelephoneField


// multiline string field
// compares without any leading or trailing whitespace, linefeeds, controlchars
class TMultilineField: public TStringField
{
  typedef TStringField inherited;
public:
  TMultilineField();
  virtual ~TMultilineField();
  // access to type
  virtual TItemFieldTypes getType(void) const { return fty_multiline; };
  virtual TItemFieldTypes getCalcType(void) const { return fty_string; };
  virtual bool isBasedOn(TItemFieldTypes aFieldType) const { return aFieldType==fty_multiline ? true : TStringField::isBasedOn(aFieldType); };
  // assignment
  virtual TItemField& operator=(TItemField &aItemField) { return TStringField::operator=(aItemField); };
  // compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem
  // SYSYNC_NOT_COMPARABLE if not equal and no ordering known
  virtual sInt16 compareWith(TItemField &aItemField, bool aCaseInsensitive=false);
}; // TMultilineField


// URL field
// normalizes URL (appends http:// if no protocol part specified)
class TURLField: public TStringField
{
  typedef TStringField inherited;
public:
  TURLField();
  virtual ~TURLField();
  // access to type
  virtual TItemFieldTypes getType(void) const { return fty_url; };
  virtual TItemFieldTypes getCalcType(void) const { return fty_string; };
  virtual bool isBasedOn(TItemFieldTypes aFieldType) const { return aFieldType==fty_url ? true : TStringField::isBasedOn(aFieldType); };
  // assignment
  virtual TItemField& operator=(TItemField &aItemField) { return TStringField::operator=(aItemField); };
  //%%%virtual void setAsString(cAppCharP aString);
  virtual void stringWasAssigned(void); // post-process string that was just assigned
}; // TURLField


class TTimestampField: public TItemField
{
  typedef TItemField inherited;
public:
  TTimestampField(GZones *aGZonesP);
  virtual ~TTimestampField();
  // access to type
  virtual TItemFieldTypes getType(void) const { return fty_timestamp; };
  virtual TItemFieldTypes getCalcType(void) const { return fty_integer; };
  virtual bool isBasedOn(TItemFieldTypes aFieldType) const { return aFieldType==fty_timestamp ? true : TItemField::isBasedOn(aFieldType); };
  // assignment
  virtual TItemField& operator=(TItemField &aItemField);
  // changelog support
  #if defined(CHECKSUM_CHANGELOG) && !defined(RECORDHASH_FROM_DBAPI)
  virtual uInt16 getDataCRC(uInt16 crc=0);
  #endif
  // access to field contents
  virtual void setAsString(cAppCharP aString);
  virtual void getAsString(string &aString);
  virtual fieldinteger_t getAsInteger(void) { return (fieldinteger_t)fTimestamp; };
  virtual void setAsInteger(fieldinteger_t aInteger) { fTimestamp = (lineartime_t)aInteger; }; // does not touch the timecontext!
  virtual void unAssign(void) { fTimestamp=noLinearTime; fTimecontext=TCTX_UNKNOWN; TItemField::unAssign(); };
  // empty test (zero timestamp means empty, unless it is a duration)
  virtual bool isEmpty(void) { return isUnassigned() || (fTimestamp==noLinearTime && !TCTX_IS_DURATION(fTimecontext)); }
  virtual void assignEmpty(void) { fTimestamp=noLinearTime; fTimecontext=TCTX_UNKNOWN; TItemField::assignEmpty(); };
  // compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem
  // SYSYNC_NOT_COMPARABLE if not equal and no ordering known
  virtual sInt16 compareWith(TItemField &aItemField, bool aCaseInsensitive=false);
  // type specific access and utilities
  /// @brief add a delta time to the timestamp
  /// @param aDeltaTime[in] : delta time value in lineartime_t units
  void addTime(lineartime_t aDeltaTime);
  /// @brief get time context
  /// @return time context
  timecontext_t getTimeContext(void) { return fTimecontext; }
  /// @brief get time context
  /// @return minute offset east of UTC, returns 0 for floating timestamps (and UTC, of course)
  sInt16 getMinuteOffset(void);
  /// @brief test for floating time (=time not in a specified zone context)
  /// @return true if context is TCTX_UNKNOWN
  bool isFloating(void);
  /// @brief make timestamp floating (i.e. remove time zone info from context)
  void makeFloating(void);
  /// @brief test for duration
  /// @return true if context has TCTX_DURATION rendering flag set
  bool isDuration(void);
  /// @brief make timestamp a duration (also implies making it floating)
  void makeDuration(void);
  /// @brief get timestamp converted to a specified time context
  /// @param aTargetContext[in] : requests output context for timestamp.
  ///        Use TCTX_UNKNOWN to get timestamp as-is.
  ///        If timestamp is floating, it will always be returned as-is
  /// @param aActualContext[out] : if not NULL, the actual context of the returned value
  ///        will be returned here. This might be another
  //         context than specified with aTargetContext depending on floating/notime status.
  /// @return timestamp in lineartime
  lineartime_t getTimestampAs(timecontext_t aTargetContext, timecontext_t *aActualContext=NULL);
  /// @brief get timestamp as ISO8601 string.
  /// @param aISOString[out] : timestamp in ISO8601 format
  /// @param aTargetContext[in] : requests output context for timestamp. Use TCTX_UNKNOWN to show timestamp as-is.
  /// @param aShowWithZ[in] : if set and timezone is UTC, value will be shown with "Z" appended
  /// @param aShowWithZone[in] : if set and timestamp is not floating, zone offset will be appended in +-xx:xx form
  /// @param aExtFormat[in] : if set, ISO8601 extended format is used
  /// @param aWithFracSecs[in] : if set, fractions of seconds will be shown (millisecond resolution)
  void getAsISO8601(string &aISOString, timecontext_t aTargetContext, bool aShowWithZ=true, bool aShowWithZone=false, bool aExtFormat=false, bool aWithFracSecs=false);
  /// @brief set timestamp value and context
  /// @param aTimestamp[in] : timestamp to set
  /// @param aTimecontext[in] : context to set
  void setTimestampAndContext(lineartime_t aTimestamp, timecontext_t aTimecontext) { setTimestamp(aTimestamp); setTimeContext(aTimecontext); };
  /// @brief set timestamp value without context
  /// @param aTimestamp[in] : timestamp to set
  void setTimestamp(lineartime_t aTimestamp) { fTimestamp=aTimestamp; fAssigned=true; };
  /// @brief set timestamp value and context
  /// @param aTimecontext[in] : context to set (timestamp will not be touched or converted)
  virtual void setTimeContext(timecontext_t aTimecontext) { fTimecontext=aTimecontext; fAssigned=true; };
  /// @brief move timestamp to specified context (i.e. convert the timestamp value from current to
  ///        specified context). Floating timestamps cannot and will not be moved.
  /// @param aNewcontext[in] : context to move timestamp to.
  ///                          timestamp will be converted to represent the same point in time in the new context
  /// @param aSetUnmovables : if set, non-movable timestamps will be just assigned the new context,
  //                          that is floating timestamps will be bound to specified context or
  //                          non-floating timestamps will be made floating if new context is TCTX_UNKNOWN
  bool moveToContext(timecontext_t aNewcontext, bool aSetUnmovables=false);
  /// @brief set timestamp from ISO8601 string.
  /// @return true if successful
  /// @param aISOString[in] : timestamp in ISO8601 basic or extended format, optionally including Z or +xx:xx zone specifier
  /// @param aDefaultContext[in] : timezone context to use when ISO8601 does not specify a zone context or when aIgnoreZone is true
  /// @param aIgnoreZone[in] : if set, timezone specification contained in ISO8601 is ignored. Resulting time context will be aDefaultContext
  bool setAsISO8601(cAppCharP aISOString, timecontext_t aDefaultContext=TCTX_UNKNOWN, bool aIgnoreZone=false);
  #ifdef EMAIL_FORMAT_SUPPORT
  /// @brief get timestamp as RFC(2)822 style date
  /// @param aRFC822String[out] : timestamp in RFC(2)822 format
  /// @param aTargetContext[in] : requests output context for timestamp. Use TCTX_UNKNOWN to show timestamp as-is.
  /// @param aShowWithZone[in] : if set and timestamp is not floating, zone offset will be shown
  void getAsRFC822date(string &aRFC822String, timecontext_t aTargetContext, bool aShowWithZone=false);
  /// @brief set timestamp as RFC(2)822 style date
  /// @return true if successful
  /// @param aRFC822String[in] : timestamp in RFC(2)822 format
  /// @param aDefaultContext[in] : timezone context to use when RFC822 date does not specify a time zone
  /// @param aIgnoreZone[in] : if set, timezone specification contained in input string is ignored. Resulting time context will be aDefaultContext
  bool setAsRFC822date(cAppCharP aRFC822String, timecontext_t aDefaultContext=TCTX_UNKNOWN, bool aIgnoreZone=false);
  #endif // EMAIL_FORMAT_SUPPORT
  // debug support
  #ifdef SYDEBUG
  virtual size_t StringObjFieldAppend(string &s, uInt16 aMaxStrLen);
  #endif
  GZones *getGZones(void) { return fGZonesP; };
protected:
  lineartime_t fTimestamp;    // timestamp in context indicated by fTimecontext
  timecontext_t fTimecontext; // context/options of timestamp
  GZones *fGZonesP; // zones
}; // TTimestampField


class TDateField: public TTimestampField
{
  typedef TTimestampField inherited;
public:
  TDateField(GZones *aGZonesP);
  virtual ~TDateField();
  // assignment
  virtual TItemField& operator=(TItemField &aItemField) { return TTimestampField::operator=(aItemField); };
  // access to type
  virtual TItemFieldTypes getType(void) const { return fty_date; };
  virtual TItemFieldTypes getCalcType(void) const { return fty_integer; };
  virtual bool isBasedOn(TItemFieldTypes aFieldType) const { return aFieldType==fty_date ? true : TTimestampField::isBasedOn(aFieldType); };
  // access to field contents
  virtual void setAsString(cAppCharP aString);
  virtual void getAsString(string &aString);
  // compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem
  // SYSYNC_NOT_COMPARABLE if not equal and no ordering known
  virtual sInt16 compareWith(TItemField &aItemField, bool aCaseInsensitive=false);
  /// @brief set timestamp value and context
  /// @param aTimecontext[in] : context to set (timestamp will not be touched or converted)
  virtual void setTimeContext(timecontext_t aTimecontext) { fTimecontext=aTimecontext | TCTX_DATEONLY; fAssigned=true; };
}; // TDateField


class TIntegerField: public TItemField
{
  typedef TItemField inherited;
public:
  TIntegerField();
  virtual ~TIntegerField();
  // access to type
  virtual TItemFieldTypes getType(void) const { return fty_integer; };
  virtual bool isBasedOn(TItemFieldTypes aFieldType) const { return aFieldType==fty_integer ? true : TItemField::isBasedOn(aFieldType); };
  // assignment
  virtual TItemField& operator=(TItemField &aItemField);
  // changelog support
  #if defined(CHECKSUM_CHANGELOG) && !defined(RECORDHASH_FROM_DBAPI)
  virtual uInt16 getDataCRC(uInt16 crc=0);
  #endif
  // access to field contents
  // - as string
  virtual void setAsString(cAppCharP aString);
  virtual void getAsString(string &aString);
  virtual void unAssign(void) { fInteger=0; TItemField::unAssign(); };
  // - as boolean (empty is false, zero value is false, other values are true)
  virtual bool getAsBoolean(void) { return !(isEmpty() || fInteger==0); };
  virtual void setAsBoolean(bool aBool) { fAssigned=true; fEmpty=false; if (aBool) fInteger=1; else fInteger=0; };
  virtual fieldinteger_t getAsInteger(void);
  virtual void setAsInteger(fieldinteger_t aInteger);
  // empty test and assignment
  virtual bool isEmpty(void) { return isUnassigned() || fEmpty; };
  virtual void assignEmpty(void) { fInteger=0; fEmpty=true; TItemField::assignEmpty(); };
  // compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem
  // SYSYNC_NOT_COMPARABLE if not equal and no ordering known
  virtual sInt16 compareWith(TItemField &aItemField, bool aCaseInsensitive=false);
protected:
  fieldinteger_t fInteger; // integer value
  bool fEmpty; // extra empty flag
}; // TIntegerField



// factory function
TItemField *newItemField(const TItemFieldTypes aType, GZones *aGZonesP, bool aAsArray=false);


#ifdef ENGINEINTERFACE_SUPPORT


// special flags coded into value ID
#define VALID_FLAG_TZNAME  0x010000
#define VALID_FLAG_TZOFFS  0x020000
#define VALID_FLAG_ARRSIZ  0x040000
#define VALID_FLAG_VALNAME 0x080000
#define VALID_FLAG_VALTYPE 0x100000
#define VALID_FLAG_NORM    0x200000
#define VALID_MASK_FID     0x00FFFF


// key for access to a item using the settings key API
class TItemFieldKey :
  public TSettingsKeyImpl
{
  typedef TSettingsKeyImpl inherited;
public:
  TItemFieldKey(TEngineInterface *aEngineInterfaceP) :
    inherited(aEngineInterfaceP),
    fWritten(false)
  {};

  // get value's ID (e.g. internal index)
  virtual sInt32 GetValueID(cAppCharP aName);

  bool isWritten(void) { return fWritten; };
protected:

  // get value's native type
  virtual uInt16 GetValueType(sInt32 aID);

  // get value
  virtual TSyError GetValueInternal(
    sInt32 aID, sInt32 aArrayIndex,
    appPointer aBuffer, memSize aBufSize, memSize &aValSize
  );

  // set value
  virtual TSyError SetValueInternal(
    sInt32 aID, sInt32 aArrayIndex,
    cAppPointer aBuffer, memSize aValSize
  );

  // flag that will be set on first write access
  bool fWritten;

  // abstract methods to actually access a TItemField
  virtual sInt16 getFidFor(cAppCharP aName, stringSize aNameSz) = 0;
  virtual TItemField *getBaseFieldFromFid(sInt16 aFid) = 0;
  virtual bool getFieldNameFromFid(sInt16 aFid, string &aFieldName) { return false; /* no name */ };

private:
  // utility
  TItemField *getFieldFromFid(sInt16 aFid, sInt16 aRepOffset, bool aExistingOnly=false);


}; // TItemFieldKey

#endif // ENGINEINTERFACE_SUPPORT


} // namespace sysync

#endif  // ItemField_H

// eof
