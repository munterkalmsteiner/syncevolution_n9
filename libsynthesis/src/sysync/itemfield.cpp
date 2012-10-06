/*
 *  File:         itemfield.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TItemField
 *    Abstract class, holds a single field value
 *  TStringField, TIntegerField, TTimeStampField etc.
 *    Implementations of field types
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-08-08 : luz : created
 *
 */

// includes
#include "prefix_file.h"
#include "sysync.h"
#include "itemfield.h"
#include "multifielditemtype.h"

#if defined(CHECKSUM_CHANGELOG) && !defined(RECORDHASH_FROM_DBAPI)
#include "sysync_crc16.h"
#endif

using namespace sysync;


namespace sysync {

// type names
cAppCharP const ItemFieldTypeNames[numFieldTypes] = {
  "string",
  "telephone",
  "integer",
  "timestamp",
  "date",
  "url",
  "multiline",
  "blob",
  "none"
};

// corresponding SyncML devInf <DataType>
const TPropDataTypes devInfPropTypes[numFieldTypes] = {
  proptype_chr,       // string -> Character
  proptype_phonenum,  // telephone -> Phone number
  proptype_int,       // integer -> Integer
  proptype_datetime,  // Timestamp -> Date and time of day
  proptype_datetime,  // Date -> Date and time of day
  proptype_chr,       // URL -> Character
  proptype_text,      // multiline -> plain text
  proptype_bin,       // BLOB -> Binary
  proptype_unknown,   // none -> unknown
};



/*
 * Implementation of TItemField
 */


TItemField::TItemField() :
  fAssigned(false) // unassigned at creation
{
} // TItemField::TItemField


TItemField::~TItemField()
{
} // TItemField::~TItemField


#ifdef SYDEBUG

// show field contents as string for debug output
size_t TItemField::StringObjFieldAppend(string &s, uInt16 aMaxStrLen)
{
  if (isUnassigned())
    s+="<unassigned>";
  else if (isEmpty())
    s+="<empty>";
  else
    appendToString(s, aMaxStrLen);
  return 0; // non-strings do not have a real size
} // TItemField::StringObjFieldAppend

#endif



// append (default to appending string value of other field)
void TItemField::append(TItemField &aItemField)
{
  string s;
  aItemField.getAsString(s);
  appendString(s);
} // TItemField::append


// generic cross-type assignment via string format
TItemField& TItemField::operator=(TItemField &aItemField)
{
  string s;

  if (aItemField.isUnassigned()) unAssign(); // copy unassigned status
  else if (aItemField.isEmpty()) assignEmpty(); // copy empty status
  else {
    aItemField.getAsString(s);
    setAsString(s);
  }
  return *this;
} // TItemField::operator=


// get any field as integer
fieldinteger_t TItemField::getAsInteger(void)
{
  // convert to integer number
  string s;
  fieldinteger_t i;
  getAsString(s);
  if (!
    #ifndef NO64BITINT
    StrToLongLong(s.c_str(),i)
    #else
    StrToLong(s.c_str(),i)
    #endif
  )
    i=0; // defined value
  return i;
} // TItemField::getAsInteger


// get integer as numeric string
void TItemField::setAsInteger(fieldinteger_t aInteger)
{
  string s;
  #ifndef NO64BITINT
  LONGLONGTOSTR(s,PRINTF_LLD_ARG(aInteger));
  #else
  StringObjPrintf(s,"%ld",(sInt32)aInteger);
  #endif
  // set as string
  setAsString(s);
} // TItemField::setAsInteger


// set as string, max number of chars = aLen
void TItemField::setAsString(cAppCharP aString, size_t aLen)
{
  if (!aString)
    assignEmpty();
  else {
    string t(aString,aLen);
    // now call basic setter
    setAsString(t.c_str());
  }
} // TItemField::setAsString


size_t TItemField::getStringSize(void)
{
  if (getType()==fty_none) return 0; // empty/unassigned field has no size (avoid unneeded getAsString)
  string s;
  getAsString(s);
  return s.size();
} // TItemField::getStringSize


bool TItemField::contains(TItemField &aItemField, bool aCaseInsensitive)
{
  string s;
  aItemField.getAsString(s);
  return findInString(s.c_str(), aCaseInsensitive)>=0;
}


#ifdef STREAMFIELD_SUPPORT

// reset stream (start reading and/or writing at specified position)
void TItemField::resetStream(size_t aPos)
{
  if (aPos==0)
    fStreamPos=0; // optimized short-cut
  else {
    size_t sz=getStreamSize();
    if (aPos>sz)
      fStreamPos=sz;
    else
      fStreamPos=aPos;
  }
} // TItemField::resetStream


// read from stream
size_t TItemField::readStream(void *aBuffer, size_t aMaxBytes)
{
  string s;
  getAsString(s);
  size_t sz=s.size();
  if (fStreamPos>sz) fStreamPos=sz;
  if (fStreamPos+aMaxBytes > sz) aMaxBytes=sz-fStreamPos;
  if (aMaxBytes>0) {
    // copy memory
    memcpy(aBuffer,s.c_str()+fStreamPos,aMaxBytes);
  }
  // return number of chars actually read
  fStreamPos+=aMaxBytes;
  return aMaxBytes;
} // TItemField::readStream


// write to stream
size_t TItemField::writeStream(void *aBuffer, size_t aNumBytes)
{
  if (aNumBytes==0) return 0;
  if (fStreamPos!=0) {
    // not replacing entire contents, need read-modify-write of string
    string s;
    getAsString(s);
    if (fStreamPos>s.size()) fStreamPos=s.size();
    s.resize(fStreamPos);
    s.append((cAppCharP)aBuffer,aNumBytes);
    setAsString(s);
  }
  else {
    setAsString((cAppCharP)aBuffer,aNumBytes);
  }
  fStreamPos+=aNumBytes;
  return aNumBytes;
} // TItemField::writeStream

#endif


/* end of TItemField implementation */


#ifdef ARRAYFIELD_SUPPORT
/*
 * Implementation of TArrayField
 */


// constructor
TArrayField::TArrayField(TItemFieldTypes aLeafFieldType, GZones *aGZonesP)
{
  fLeafFieldType=aLeafFieldType;
  fGZonesP = aGZonesP;
  // field for index==0 always exists
  fFirstField = newItemField(fLeafFieldType,fGZonesP,false);
} // TArrayField::TArrayField


// destructor
TArrayField::~TArrayField()
{
  // make sure leaf fields (except idx==0) are gone
  unAssign();
  // and kill firstfield
  delete fFirstField;
} // TArrayField::~TArrayField



bool TArrayField::elementsBasedOn(TItemFieldTypes aFieldType) const
{
  return fFirstField->isBasedOn(aFieldType);
} // TArrayField::elementsBasedOn



#ifdef SYDEBUG

// show field contents as string for debug output
size_t TArrayField::StringObjFieldAppend(string &s, uInt16)
{
  if (!isAssigned())
    return inherited::StringObjFieldAppend(s,0); // let ancestor show
  StringObjAppendPrintf(s,"<array with %ld elements>",(long int)(arraySize()));
  return 0;
} // TArrayField::StringObjFieldAppend

#endif


// get field from array (creates new if index is larger than current array size)
TItemField *TArrayField::getArrayField(sInt16 aArrIdx, bool aExistingOnly)
{
  TItemField *fldP = NULL;
  // check index, negative index means array field itself
  if (aArrIdx<0) return this;
  // check if we have that field already
  if (aArrIdx<arraySize()) {
    // pointer array is large enough
    fldP = fArray[aArrIdx];
    if (fldP==NULL) {
      // but element does not exist yet, create field for it
      if (aArrIdx==0)
        fldP = fFirstField;
      else
        fldP = newItemField(fLeafFieldType,fGZonesP,false);
      fArray[aArrIdx]=fldP;
    }
  }
  else if (aExistingOnly) {
    // element does not exist and we want not to create new elements:
    return NULL;
  }
  else {
    // element does not exist yet, create new ones up to requested index
    fArray.reserve(aArrIdx+1);
    for (sInt16 idx=arraySize(); idx<=aArrIdx; idx++) {
      fArray.push_back(NULL);
    }
    // actually create last field only (and use firstField if that happens to be idx==0)
    if (aArrIdx==0)
      fldP = fFirstField;
    else
      fldP = newItemField(fLeafFieldType,fGZonesP,false);
    fArray[aArrIdx]=fldP;
  }
  // return field
  return fldP;
} // TArrayField::getArrayField


#if defined(CHECKSUM_CHANGELOG) && !defined(RECORDHASH_FROM_DBAPI)
// calc CRC over all fields
uInt16 TArrayField::getDataCRC(uInt16 crc)
{
  for (sInt16 idx=0; idx<arraySize(); idx++) {
    crc=getArrayField(idx)->getDataCRC(crc);
  }
  return crc;
} // TArrayField::getDataCRC
#endif


// clear all leaf fields
void TArrayField::unAssign(void)
{
  for (sInt16 idx=0; idx<arraySize(); idx++) {
    if (fArray[idx]) {
      if (idx==0)
        fArray[idx]->unAssign(); // first is always kept, it is the fFirstField, so only unassign to remove content
      else
        delete fArray[idx];
      fArray[idx]=NULL;
    }
  }
  // clear list now
  fArray.clear();
  // clear flag as well that could be set in case of an explicitly assigned empty array
  fAssigned = false;
} // TArrayField::unAssign


// assign
TItemField& TArrayField::operator=(TItemField &aItemField)
{
  // assignment of empty (could be EMPTY or UNASSIGNED) must be handled by base class
  if (aItemField.isEmpty()) return TItemField::operator=(aItemField);
  // handle array-to-array assignments
  if (aItemField.isArray()) {
    // delete my current contents
    unAssign();
    // copy leaf fields from other field
    for (sInt16 idx=0; idx<aItemField.arraySize(); idx++) {
      // assign array members
      *(getArrayField(idx)) = *(((TItemField *)(&aItemField))->getArrayField(idx));
    }
  }
  else {
    // non-array (non-empty) assigned to array: just assign value to first element
    unAssign();
    *(getArrayField(0)) = aItemField;
  }
  return *this;
} // TArrayField::operator=


// append values (only assigned ones)
// - if other field is an array, too, all elements of other array
//   will be appended at end of this array.
// - if other field is a scalar, it's value will be appended to
//   the array.
void TArrayField::append(TItemField &aItemField)
{
  if (aItemField.isArray()) {
    // append elements of another array to this array
    for (sInt16 idx=0; idx<aItemField.arraySize(); idx++) {
      // append all array members to this array
      // Note: calling append recursively!
      append(*(aItemField.getArrayField(idx)));
    }
  }
  else {
    // append value of another field to this array
    // - do not append unassigned fields
    if (aItemField.isAssigned()) {
      *(getArrayField(arraySize())) = aItemField;
    }
  }
} // TArrayField::append



// append string to array = append string as last element of array
void TArrayField::appendString(cAppCharP aString, size_t aMaxChars)
{
  getArrayField(arraySize())->setAsString(aString,aMaxChars);
} // TArrayField::appendString


// contains for arrays means "contains in any of the elements"
// (if aItemField is an array as well, this means that every element must be
// contained somewhere in my own array)
bool TArrayField::contains(TItemField &aItemField, bool aCaseInsensitive)
{
  bool contained = false;
  if (aItemField.isArray()) {
    contained=true;
    // array: all array elements must be contained in at least one of my elements
    for (sInt16 idx=0; idx<aItemField.arraySize(); idx++) {
      if (!contains(*(aItemField.getArrayField(idx)),aCaseInsensitive)) {
        // one of the elements of aItemField is not contained in myself -> not contained
        contained = false;
        break;
      }
    }
  }
  else {
    // leaf element: must be contained in at least one of my elements
    contained = false;
    for (sInt16 idx=0; idx<arraySize(); idx++) {
      if (getArrayField(idx)->contains(aItemField,aCaseInsensitive)) {
        // the value of aItemField is contained in one of my elements -> contained
        contained = true;
        break;
      }
    }
  }
  return contained;
} // TItemField::contains



// compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem,
// SYSYNC_NOT_COMPARABLE if not comparable at all or not equal and no ordering known
// Note: array fields are only comparable with other array fields
sInt16 TArrayField::compareWith(TItemField &aItemField, bool aCaseInsensitive)
{
  if (!aItemField.isArray()) return SYSYNC_NOT_COMPARABLE;
  // get sizes of arrays
  sInt16 mysz = arraySize();
  sInt16 othersz = aItemField.arraySize();
  sInt16 commonsz = mysz>othersz ? othersz : mysz;
  // compare common array elements
  for (sInt16 idx=0; idx<commonsz; idx++) {
    sInt16 res = getArrayField(idx)->compareWith(*(aItemField.getArrayField(idx)), aCaseInsensitive);
    if (res!=0) return res; // all non-equal return
  }
  // all compared fields are equal
  if (mysz==othersz) return 0; // same size : equal
  // larger array is greater
  return mysz>othersz ? 1 : -1;
} // TArrayField::compareWith

/* end of TArrayField implementation */
#endif



/*
 * Implementation of TStringField
 */


TStringField::TStringField()
{
  #ifdef STREAMFIELD_SUPPORT
  fBlobProxyP=NULL;
  #endif
} // TStringField::TStringField


TStringField::~TStringField()
{
  #ifdef STREAMFIELD_SUPPORT
  // remove proxy if any
  TBlobProxy::unlink(fBlobProxyP);
  #endif
} // TStringField::~TStringField


#if defined(CHECKSUM_CHANGELOG) && !defined(RECORDHASH_FROM_DBAPI)

// changelog support: calculate CRC over contents
uInt16 TStringField::getDataCRC(uInt16 crc)
{
  // CRC over characters in the string
  return sysync_crc16_block(getCStr(),getStringSize(),crc);
} // TStringField::getDataCRC

#endif


// assignment
TItemField& TStringField::operator=(TItemField &aItemField)
{
  DELETEPROXY; // forget old value and old proxy as well
  // handle myself only if other is string field as well
  if (aItemField.isBasedOn(fty_string)) {
    // copy fields 1:1
    const TStringField *sfP = static_cast<const TStringField *>(&aItemField);
    fString=sfP->fString;
    fAssigned=sfP->fAssigned;
    #ifdef STREAMFIELD_SUPPORT
    fBlobProxyP=sfP->fBlobProxyP; // copy proxy as well
    if (fBlobProxyP) fBlobProxyP->link(); // link again, now both fields use the proxy
    #endif
  }
  else
    TItemField::operator=(aItemField); // generic cross-type assignment (via string)
  return *this;
} // TStringField::operator=


// get size of string
size_t TStringField::getStringSize(void)
{
  #ifdef STREAMFIELD_SUPPORT
  return getStreamSize();
  #else
  return fString.size();
  #endif
} // TStringField::getStringSize


// normalized string is contents without leading or trailing control chars or spaces
void TStringField::getAsNormalizedString(string &aString)
{
  size_t nsiz = getStringSize();
  // we need the actual string to do this
  PULLFROMPROXY;
  // find first non-control or non-WSP char
  size_t start = 0;
  while (start<nsiz && ((uInt8)fString[start])<=' ') start++;
  // find last non-control or non-WSP char
  while (nsiz>0 && ((uInt8)fString[nsiz-1])<=' ') nsiz--;
  // take string without any leading or trailing white space or other control chars
  aString.assign(fString,start,nsiz-start);
} // TStringField::getAsNormalizedString




#ifdef SYDEBUG

// show field contents as string for debug output
size_t TStringField::StringObjFieldAppend(string &s, uInt16 aMaxStrLen)
{
  size_t n = 0;
  // empty or unassigned is handled by base class
  if (isEmpty()) { return inherited::StringObjFieldAppend(s,aMaxStrLen); }
  // with proxy installed, do not pull the string
  if (PROXYINSTALLED) {
    s+="<proxy installed>";
    n=0; // unknown size at this time
  }
  else {
    // no proxy installed
    s+='"';
    // - check if display length is sufficient
    size_t i;
    n = getStringSize();
    if (aMaxStrLen==0 || n<=10 || n<=size_t(aMaxStrLen-2)) {
      // strings below 11 chars are always shown in full
      s.append(fString);
    }
    else {
      i = (aMaxStrLen-5)/2; // half of the size that can be displayed
      s.append(fString,0,i);
      s.append("...");
      s.append(fString,n-i,i);
    }
    s+='"';
  }
  // return actual string size (not shortened one!)
  return n;
} // TStringField::StringObjFieldAppend

#endif




#ifdef STREAMFIELD_SUPPORT

// set blob loader proxy (ownership is passed to field)
void TStringField::setBlobProxy(TBlobProxy *aBlobProxyP)
{
  if (fBlobProxyP==aBlobProxyP) return; // same proxy, just ignore
  // unlink previous proxy, if any
  TBlobProxy::unlink(fBlobProxyP);
  // assign new proxy
  fBlobProxyP=aBlobProxyP;
  // assigning a proxy means that the field is now assigned
  fAssigned=true;
} // TStringField::setBlobProxy


// pull entire string from proxy and forget proxy
void TStringField::pullFromProxy(void)
{
  if (fBlobProxyP) {
    const size_t bufsiz=4096;
    cAppCharP bufP = new char[bufsiz];
    resetStream();
    size_t by;
    SYSYNC_TRY {
      do {
        by=fBlobProxyP->readBlobStream(this, fStreamPos, (void *)bufP, bufsiz);
        fString.append(bufP,by);
      } while (by==bufsiz);
    }
    SYSYNC_CATCH(exception &e)
      // avoid crashing session if proxy pull fails
      fString="Server error while getting data from proxy object: ";
      fString+=e.what();
    SYSYNC_ENDCATCH
    delete bufP;
    // proxy is no longer needed
    TBlobProxy::unlink(fBlobProxyP);
  }
} // TStringField::pullFromProxy


// return size of stream
size_t TStringField::getStreamSize(void)
{
  if (fBlobProxyP)
    SYSYNC_TRY {
      return fBlobProxyP->getBlobSize(this); // return size of entire blob
    }
    SYSYNC_CATCH(exception &e)
      return 0; // cannot return actual size
    SYSYNC_ENDCATCH
  else
    return fString.size(); // just return size of already stored string
} // TStringField::getStreamSize


// read as stream
size_t TStringField::readStream(void *aBuffer, size_t aMaxBytes)
{
  if (fBlobProxyP) {
    SYSYNC_TRY {
      // let proxy handle this
      return fBlobProxyP->readBlobStream(this, fStreamPos, aBuffer, aMaxBytes);
    }
    SYSYNC_CATCH(...)
      // do not return anything
      return 0; // do not return anything
    SYSYNC_ENDCATCH
  }
  else {
    // read from string itself
    size_t sz=fString.size();
    if (fStreamPos>sz) fStreamPos=sz;
    if (fStreamPos+aMaxBytes > sz) aMaxBytes=sz-fStreamPos;
    if (aMaxBytes>0) {
      // copy memory
      memcpy(aBuffer,fString.c_str()+fStreamPos,aMaxBytes);
    }
    // return number of chars actually read
    fStreamPos+=aMaxBytes;
    return aMaxBytes;
  }
} // TStringField::readStream


// write as stream
size_t TStringField::writeStream(void *aBuffer, size_t aNumBytes)
{
  if (aNumBytes==0) return 0;
  if (fStreamPos>fString.size()) fStreamPos=fString.size();
  fString.resize(fStreamPos);
  fString.append((cAppCharP)aBuffer,aNumBytes);
  fStreamPos+=aNumBytes;
  return aNumBytes;
} // TStringField::writeStream

#endif



// set as string, max number of chars to assign = aLen
void TStringField::setAsString(cAppCharP aString, size_t aLen)
{
  DELETEPROXY; // forget old value and old proxy as well
  if (!aString)
    fString.erase();
  else
    fString.assign(aString,aLen);
  stringWasAssigned();
} // TStringField::setAsString


// append to string
void TStringField::appendString(cAppCharP aString, size_t aMaxChars)
{
  PULLFROMPROXY; // make sure we have all chars
  if (aString) {
    fString.append(aString,aMaxChars);
  }
  stringWasAssigned();
} // TStringField::appendString


// merge field contents into this field
bool TStringField::merge(TItemField &aItemField, const char aSep)
{
  bool mergedsomething=false;

  if (aItemField.isBasedOn(fty_string)) {
    TStringField *sfP = static_cast<TStringField *>(&aItemField);
    #ifdef STREAMFIELD_SUPPORT
    pullFromProxy(); // make sure we have all chars
    sfP->pullFromProxy(); // make sure we have all chars
    #endif
    if (aSep) {
      // take each aSep separated part of source (aItemField), and if it is not
      // yet part of target (this), then add it
      string::size_type j,i=0;
      string part;
      do {
        // extract part from source
        j=sfP->fString.find(aSep,i);
        if (j==string::npos)
          part.assign(sfP->fString,i,sfP->fString.size()-i);
        else
          part.assign(sfP->fString,i,j-i);
        // see if it is contained in target already
        if (!part.empty() && fString.find(part)==string::npos) {
          // not contained, add
          if (!fString.empty()) fString+=aSep;
          inherited::appendString(part);
          mergedsomething=true;
        }
        // check next part
        i=j+1; // char after last separator
      } while (j!=string::npos);
    }
    else {
      // no intelligent separator based merge, just append if not equal
      if (sfP->fString != fString) {
        inherited::appendString(sfP->fString);
        mergedsomething=true;
      }
    }
  }
  return mergedsomething;
} // TStringField::merge


// compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem,
// SYSYNC_NOT_COMPARABLE if not equal and no ordering known or if field
// types do not match.
// Note: ordering may NOT be age relevant; it just means that an ordering
// for this field type exists.
// Note: Both fields must be assigned. NO TEST IS DONE HERE!
sInt16 TStringField::compareWith(TItemField &aItemField, bool aCaseInsensitive)
{
  sInt16 result;
  PULLFROMPROXY;
  if (aItemField.isBasedOn(fty_string)) {
    TStringField *sfP = static_cast<TStringField *>(&aItemField);
    #ifdef STREAMFIELD_SUPPORT
    sfP->pullFromProxy(); // make sure we have all chars
    #endif
    // direct compare possible, return strcmp
    if (aCaseInsensitive)
      result=strucmp(fString.c_str(),sfP->fString.c_str());
    else
      result=strcmp(fString.c_str(),sfP->fString.c_str());
  }
  else {
    // convert other field to string
    string s;
    aItemField.getAsString(s);
    if (aCaseInsensitive)
      result=strucmp(fString.c_str(),s.c_str());
    else
      result=strcmp(fString.c_str(),s.c_str());
  }
  return result >0 ? 1 : (result<0 ? -1 : 0);
} // TStringField::compareWith


// check if specified field is shortened version of this one
bool TStringField::isShortVers(TItemField &aItemField, sInt32 aOthersMax)
{
  if (!aItemField.isBasedOn(fty_string)) return false; // different types
  TStringField *sfP = static_cast<TStringField *>(&aItemField);
  #ifdef STREAMFIELD_SUPPORT
  pullFromProxy(); // make sure we have all chars
  sfP->pullFromProxy(); // make sure we have all chars
  #endif
  // same type
  // - if other field is empty, it does not count as a shortened version in any case
  if (sfP->isEmpty()) return false;
  // - show if other field is shortened version of this one
  return (
    aOthersMax!=FIELD_OPT_MAXSIZE_NONE && // other field is limited (if not, cannot be short version)
    (aOthersMax==FIELD_OPT_MAXSIZE_UNKNOWN || uInt32(aOthersMax)==sfP->getStringSize()) && // size is unknown or other value is max size
    findInString(sfP->fString.c_str())==0 // other value is contained at beginning of this value
  );
} // TStringField::isShortVers


// - check if String is contained in value and returns position
sInt16 TStringField::findInString(cAppCharP aString, bool aCaseInsensitive)
{
  PULLFROMPROXY;
  if (aString==NULL || *aString==0)
    return fString.empty() ? 0 : -1; // if I am empty myself,treat empty string as contained at beginning
  // no empty reference string
  if (!aCaseInsensitive) {
    return fString.find(aString);
  }
  else {
    return strupos(fString.c_str(),aString,fString.size());
  }
};


/* end of TStringField implementation */


/*
 * Implementation of TBlobField
 */


TBlobField::TBlobField()
{
  // nothing known about contents yet
  fHasEncoding = enc_none;
  fWantsEncoding = enc_none;
  fCharset = chs_utf8;
} // TBlobField::TBlobField


TBlobField::~TBlobField()
{
} // TBlobField::~TBlobField


// assignment
TItemField& TBlobField::operator=(TItemField &aItemField)
{
  DELETEPROXY; // forget old value and old proxy as well
  // assignment of empty (could be EMPTY or UNASSIGNED) must be handled by base class
  if (aItemField.isEmpty()) return TItemField::operator=(aItemField);
  // handle non-empty myself if other is based on string (blob is just a enhanced string)
  if (aItemField.isBasedOn(fty_string)) {
    // assign string portions
    TStringField::operator=(aItemField);
    if(aItemField.isBasedOn(fty_blob)) {
      // other is a blob, copy blob specific options
      const TBlobField *bfP = static_cast<const TBlobField *>(&aItemField);
      fHasEncoding=bfP->fHasEncoding;
      fWantsEncoding=bfP->fWantsEncoding;
      fCharset=bfP->fCharset;
    }
    else {
      // reset blob options to default for string content
      fHasEncoding = enc_none;
      fWantsEncoding = enc_none;
      fCharset = chs_utf8;
    }
  }
  else
    TItemField::operator=(aItemField); // generic cross-type assignment (via string)
  return *this;
} // TBlobField::operator=



#ifdef SYDEBUG

// debug support
size_t TBlobField::StringObjFieldAppend(string &s, uInt16 aMaxStrLen)
{
  // empty or unassigned is handled by base class
  if (isEmpty()) { return inherited::StringObjFieldAppend(s, aMaxStrLen); }
  // avoid pulling a proxy for debug
  if (!PROXYINSTALLED) {
    appendToString(s); // show standard string which is pseudo-content
    return getStringSize(); // return actual size of BLOB
  }
  else
    return inherited::StringObjFieldAppend(s, aMaxStrLen); // with proxy installed, base class will show that
} // TBlobField::StringObjFieldAppend

#endif // SYDEBUG


/* end of TBlobField implementation */



/*
 * Implementation of TTelephoneField
 */


TTelephoneField::TTelephoneField()
{
} // TTelephoneField::TTelephoneField


TTelephoneField::~TTelephoneField()
{
} // TTelephoneField::~TTelephoneField


void TTelephoneField::getAsNormalizedString(string &aString)
{
  cAppCharP p = getCStr();
  char c;
  aString.erase();
  // only returns alphanum and +,*,#, rest is filtered out
  while ((c=*p++)!=0) {
    if (isalnum(c) || c=='+' || c=='#' || c=='*') aString+=c;
  }
} // TTelephoneField::getAsNormalizedString


// compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem,
// SYSYNC_NOT_COMPARABLE if not equal and no ordering known or if field
// types do not match.
// Note: ordering may NOT be age relevant; it just means that an ordering
// for this field type exists.
// Note: Both fields must be assigned. NO TEST IS DONE HERE!
sInt16 TTelephoneField::compareWith(TItemField &aItemField, bool aCaseInsensitive)
{
  if (!aItemField.isBasedOn(fty_telephone)) {
    // if other field is not telephone, try normal string comparison
    return TStringField::compareWith(aItemField, aCaseInsensitive);
  }
  // compare possible, return strcmp of normalized numbers
  TTelephoneField *tfP = static_cast<TTelephoneField *>(&aItemField);
  string s1,s2;
  getAsNormalizedString(s1);
  tfP->getAsNormalizedString(s2);
  if (s1==s2) return 0; // equal
  else return SYSYNC_NOT_COMPARABLE; // not equal, ordering makes no sense for tel numbers
} // TTelephoneField::compareWith


/* end of TTelephoneField implementation */


/*
 * Implementation of TTelephoneField
 */

TMultilineField::TMultilineField()
{
} // TMultilineField::TMultilineField


TMultilineField::~TMultilineField()
{
} // TMultilineField::~TMultilineField


// compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem,
// SYSYNC_NOT_COMPARABLE if not equal and no ordering known or if field
// types do not match.
// Note: ordering may NOT be age relevant; it just means that an ordering
// for this field type exists.
// Note: Both fields must be assigned. NO TEST IS DONE HERE!
sInt16 TMultilineField::compareWith(TItemField &aItemField, bool aCaseInsensitive)
{
  if (!aItemField.isBasedOn(fty_multiline)) {
    // if other field is not multiline, try normal string comparison
    return TStringField::compareWith(aItemField);
  }
  // compare possible, return strcmp of normalized texts
  TMultilineField *mfP = static_cast<TMultilineField *>(&aItemField);
  string s1,s2;
  getAsNormalizedString(s1);
  mfP->getAsNormalizedString(s2);
  // compare
  sInt8 result;
  if (aCaseInsensitive)
    result=strucmp(s1.c_str(),s2.c_str());
  else
    result=strcmp(s1.c_str(),s2.c_str());
  return result >0 ? 1 : (result<0 ? -1 : 0);
} // TMultilineField::compareWith


/* end of TMultilineField implementation */

/*
 * Implementation of TURLField
 */

TURLField::TURLField()
{
} // TURLField::TURLField


TURLField::~TURLField()
{
} // TURLField::~TURLField



void TURLField::stringWasAssigned(void)
{
  // post-process string that was just assigned
  string proto;
  if (!fString.empty()) {
    // make sure we have a URL with protocol
    splitURL(fString.c_str() ,&proto, NULL, NULL, NULL, NULL, NULL, NULL);
    if (proto.empty()) {
      // no protocol set, but string not empty --> assume http
      fString.insert(0, "http://");
    }
  }
  inherited::stringWasAssigned();
} // TURLField::stringWasAssigned


/* end of TURLField implementation */



/*
 * Implementation of TTimestampField
 */


TTimestampField::TTimestampField(GZones *aGZonesP)
{
  fGZonesP = aGZonesP;
  fTimestamp=0;
  fTimecontext=TCTX_UNKNOWN;
} // TTimestampField::TTimestampField


TTimestampField::~TTimestampField()
{
} // TTimestampField::~TTimestampField


#if defined(CHECKSUM_CHANGELOG) && !defined(RECORDHASH_FROM_DBAPI)

// changelog support: calculate CRC over contents
uInt16 TTimestampField::getDataCRC(uInt16 crc)
{
  // calculate CRC such that only a effective change in the time zone will cause a
  // CRC change: TCTX_SYSTEM must be converted to UTC as changing the system time zone
  // should not re-sync the entire calendar
  lineartime_t crcts = fTimestamp;
  timecontext_t crcctx = fTimecontext;
  if (TCTX_IS_SYSTEM(crcctx)) {
    if (TzConvertTimestamp(crcts,crcctx,TCTX_UTC,fGZonesP))
      crcctx=TCTX_UTC; // use UTC representation for CRC (= same as in 3.0)
  }
  // try to avoid re-sync when upgrading from 3.0 to 3.1
  if (TCTX_IS_UTC(crcctx)) crcctx=0; // this is what we had in 3.0 for non-floating timestamps
  // CRC over timestamp itself and zone offset
  crc=sysync_crc16_block(&crcts,sizeof(crcts),crc);
  return sysync_crc16_block(&crcctx,sizeof(crcctx),crc);
} // TTimestampField::getDataCRC

#endif


// assignment
TItemField& TTimestampField::operator=(TItemField &aItemField)
{
  // assignment of empty (could be EMPTY or UNASSIGNED) must be handled by base class
  if (aItemField.isEmpty()) return TItemField::operator=(aItemField);
  // handle non-empty myself
  if (aItemField.isBasedOn(fty_timestamp)) {
    const TTimestampField *sfP = static_cast<const TTimestampField *>(&aItemField);
    fTimestamp=sfP->fTimestamp;
    fTimecontext=sfP->fTimecontext;
    fAssigned=sfP->fAssigned;
  }
  else if (aItemField.getCalcType()==fty_integer) {
    setAsInteger(aItemField.getAsInteger());
  }
  else
    TItemField::operator=(aItemField); // generic cross-type assignment (via string)
  return *this;
} // TTimestampField::operator=


// compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem,
// SYSYNC_NOT_COMPARABLE if not equal and no ordering known or if field
// types do not match.
// Note: ordering may NOT be age relevant; it just means that an ordering
// for this field type exists.
// Note: Both fields must be assigned. NO TEST IS DONE HERE!
sInt16 TTimestampField::compareWith(TItemField &aItemField, bool aCaseInsensitive)
{
  sInt16 res;
  lineartime_t cmpval;
  timecontext_t cmpcontext;

  // determine value to compare with
  if (aItemField.isBasedOn(fty_timestamp)) {
    cmpval=static_cast<TTimestampField *>(&aItemField)->fTimestamp;
    cmpcontext=static_cast<TTimestampField *>(&aItemField)->fTimecontext;
  }
  else {
    // not same type
    if (aItemField.getCalcType()==fty_integer) {
      // use second argument as lineartime units number
      cmpval=aItemField.getAsInteger();
      cmpcontext=fTimecontext; // treat number as timestamp value in my own context
    }
    else {
      // use second argument as ISO8601 string
      string s;
      aItemField.getAsString(s);
      ISO8601StrToTimestamp(s.c_str(), cmpval, cmpcontext);
      if (TCTX_IS_UNKNOWN(cmpcontext)) cmpcontext=fTimecontext; // treat unqualified ISO timestamp in my own context
    }
  }
  // convert compare value into my own context (if not already so)
  if (!TzConvertTimestamp(cmpval,cmpcontext,fTimecontext,fGZonesP))
    return SYSYNC_NOT_COMPARABLE; // contexts are not compatible to compare
  // compare possible, return strcmp-type result
  res=fTimestamp==cmpval ? 0 : (fTimestamp>cmpval ? 1 : -1);
  if (res!=0) {
    // greater/less-type result is valid only if both sides are not empty
    if (fTimestamp==0) return SYSYNC_NOT_COMPARABLE; // empty value, cannot compare
    if (cmpval==0) return SYSYNC_NOT_COMPARABLE; // empty value, cannot compare
  }
  return res;
} // TTimestampField::compareWith


#ifdef SYDEBUG

// debug support
size_t TTimestampField::StringObjFieldAppend(string &s, uInt16 aMaxStrLen)
{
  // empty or unassigned is handled by base class
  if (isEmpty()) { return inherited::StringObjFieldAppend(s, aMaxStrLen); }

  string str;
  timecontext_t tctx;
  TimestampToISO8601Str(str,fTimestamp,fTimecontext,true,true);
  s.append(str);
  tctx=fTimecontext;
  if (TCTX_IS_TZ(tctx)) {
    // symbolic time zone not represented in ISO8601, show extra
    s.append(" (");
    if (TCTX_IS_UNKNOWN(tctx)) {
      s.append("floating");
    }
    else {
      if (TCTX_IS_SYSTEM(tctx)) {
        // is the system zone
        s.append("System ");
        // resolve from meta to symbolic zone
        TzResolveMetaContext(tctx,fGZonesP);
      }
      if (!TCTX_IS_BUILTIN(tctx)) {
        s.append("imported ");
      }
      s.append("TZ: ");
      // show time zone name
      TimeZoneContextToName(tctx,str,fGZonesP);
      s.append(str);
    }
    s.append(")");
  }
  // size not known
  return 0;
} // TTimestampField::StringObjFieldAppend

#endif // SYDEBUG


// set timestamp from ISO8601 string
void TTimestampField::setAsString(cAppCharP aString)
{
  // - set context if contained in ISO string, otherwise leave it as TCTX_UNKNOWN
  setAsISO8601(aString, TCTX_UNKNOWN, false);
} // TTimestampField::setAsString


// get timestamp as ISO8601 string (default representation)
void TTimestampField::getAsString(string &aString)
{
  // use default rendering
  // - show with current context, show UTC with Z but other zones without zone specifier
  getAsISO8601(aString,TCTX_UNKNOWN,true,false,false,false);
} // getAsString




/// @brief add a delta time to the timestamp
/// @param aDeltaTime[in] : delta time value in lineartime_t units
void TTimestampField::addTime(lineartime_t aDeltaTime)
{
  fTimestamp=fTimestamp+aDeltaTime;
  fAssigned=true;
} // TTimestampField::addTime


/// @brief get time context
/// @return minute offset east of UTC, returns 0 for floating timestamps (and UTC, of course)
sInt16 TTimestampField::getMinuteOffset(void)
{
  sInt16 moffs;

  if (TzResolveToOffset(fTimecontext, moffs, fTimestamp, false, fGZonesP))
    return moffs; // found offset
  else
    return 0; // no offset (e.g. floating timestamp)
} // TTimestampField::getMinuteOffset


/// @brief test for floating time (=time not in a specified zone context)
/// @return true if context is TCTX_UNKNOWN
bool TTimestampField::isFloating(void)
{
  return TCTX_IS_UNKNOWN(fTimecontext);
} // TTimestampField::isFloating


/// @brief make timestamp floating (i.e. remove time zone info from context)
void TTimestampField::makeFloating(void)
{
  // rendering flags from original context, new zone is UNKNOWN (=floating)
  fTimecontext = TCTX_JOIN_RFLAGS_TZ(fTimecontext, TCTX_UNKNOWN);
} // TTimestampField::makeFloating


/// @brief test for duration
/// @return true if context has TCTX_DURATION rendering flag set
bool TTimestampField::isDuration(void)
{
  return TCTX_IS_DURATION(fTimecontext);
} // TTimestampField::isDuration


/// @brief make timestamp a duration (also implies making it floating)
void TTimestampField::makeDuration(void)
{
  // rendering flags from original context, new zone is UNKNOWN (=floating)
  fTimecontext |= TCTX_DURATION;
  makeFloating();
} // TTimestampField::makeDuration


/// @brief get timestamp converted to a specified time context
/// @param aTargetContext[in] : requests output context for timestamp.
///        Use TCTX_UNKNOWN to get timestamp as-is.
///        If timestamp is floating, it will always be returned as-is
/// @param aActualContext[out] : if not NULL, the actual context of the returned value
///        will be returned here. This might be another
//         context than specified with aTargetContext depending on floating/notime status.
/// @return timestamp in lineartime
lineartime_t TTimestampField::getTimestampAs(timecontext_t aTargetContext, timecontext_t *aActualContext)
{
  // default context is that of the field (will be overwritten if result is moved to another context)
  if (aActualContext) *aActualContext = fTimecontext; // current field's context
  // if no time set or TCTX_UNKNOWN specified, just return the timestamp as-is
  if (fTimestamp==noLinearTime || TCTX_IS_UNKNOWN(aTargetContext)) {
    return
      TCTX_IS_DATEONLY(aTargetContext) ?
      lineartime2dateonlyTime(fTimestamp) : // truncated to date-only
      fTimestamp; // as-is
  }
  // convert to the requested context
  lineartime_t ts = fTimestamp;
  if (TCTX_IS_DATEONLY(fTimecontext)) {
    // is date only, connot convert to another zone, just return date part (and original context)
    return lineartime2dateonlyTime(ts);
  }
  else {
    // datetime or time
    if (!TzConvertTimestamp(ts,fTimecontext,aTargetContext,fGZonesP)) {
      // cannot convert, but we return unconverted timestamp if we can also return the actual context
      if (aActualContext)
        return fTimestamp; // return as-is
      else
        return noLinearTime; // invalid conversion, can't return a value
    }
  }
  // return target context as actual context
  if (aActualContext) *aActualContext = aTargetContext;
  // return timestamp, possibly truncated to date-only or time-only if requested
  if (TCTX_IS_DATEONLY(aTargetContext))
    return lineartime2dateonlyTime(ts);
  else if (TCTX_IS_TIMEONLY(aTargetContext))
    return lineartime2timeonly(ts);
  else
    return ts; // date and time
} // TTimestampField::getTimestampAs



/// @brief get timestamp as ISO8601 string.
/// @param aISOString[out] : timestamp in ISO8601 format
/// @param aTargetContext[in] : requests output context for timestamp. Use TCTX_UNKNOWN to show timestamp (or dateonly or timeonly) as-is.
/// @param aShowWithZ[in] : if set and timezone is UTC, value will be shown with "Z" appended
/// @param aShowWithZone[in] : if set and timestamp is not floating, zone offset will be appended in +-xx:xx form
/// @param aExtFormat[in] : if set, ISO8601 extended format is used
/// @param aWithFracSecs[in] : if set, fractions of seconds will be shown (millisecond resolution)
void TTimestampField::getAsISO8601(string &aISOString, timecontext_t aTargetContext, bool aShowWithZ, bool aShowWithZone, bool aExtFormat, bool aWithFracSecs)
{
  // get time in requested context
  lineartime_t ts = getTimestampAs(aTargetContext);
  // if target context was set to TCTX_UNKNOWN, this means that we want to use the stored context
  if (aTargetContext==TCTX_UNKNOWN) // really explicitly TCTX_UNKNOWN without any rendering flags!
    aTargetContext = fTimecontext;
  // return empty string if no timestamp or conversion impossible (but show empty duration as such!)
  if (ts==noLinearTime && !TCTX_IS_DURATION(fTimecontext)) {
    aISOString.erase();
    return;
  }
  // check if time zone should be included or not
  if (!TCTX_IS_UNKNOWN(aTargetContext)) {
    if (
      TCTX_IS_UTC(aTargetContext) ?
      !aShowWithZ :
      !aShowWithZone
    ) {
      // prevent showing with time zone
      aTargetContext = TCTX_JOIN_RFLAGS_TZ(aTargetContext,TCTX_UNKNOWN);
    }
  }
  // now convert
  TimestampToISO8601Str(aISOString, ts, aTargetContext, aExtFormat, aWithFracSecs);
} // TTimestampField::getAsISO8601



/// @brief move timestamp to specified context (i.e. convert the timestamp value from current to
///        specified context). Floating timestamps cannot and will not be moved.
/// @param aNewcontext[in] : context to move timestamp to.
///                          timestamp will be converted to represent the same point in time in the new context
/// @param aSetUnmovables : if set, non-movable timestamps will be just assigned the new context,
//                          that is floating timestamps will be bound to specified context or
//                          non-floating timestamps will be made floating if new context is TCTX_UNKNOWN
bool TTimestampField::moveToContext(timecontext_t aNewcontext, bool aSetUnmovables)
{
  bool ok=true;
  if (isFloating() || fTimestamp==noLinearTime) {
    // floating and empty timestamp cannot be moved
    if (aSetUnmovables) {
      // bind floating timestamp to specified zone
      fTimecontext = aNewcontext;
    }
    else
      ok=false; // floating/empty timestamps can only be fixed, not moved
  }
  else {
    // timestamp is not floating or empty
    if (TCTX_IS_UNKNOWN(aNewcontext)) {
      if (aSetUnmovables)
        fTimecontext = aNewcontext; // make floating
      else
        ok = false;
    }
    else {
      // new context is not floating and old context isn't, either -> convert
      ok = TzConvertTimestamp(fTimestamp, fTimecontext, aNewcontext, fGZonesP);
    }
  }
  if (ok)
    fTimecontext = aNewcontext; // we are now in the new context
  return ok;
} // TTimestampField::moveToContext



/// @brief set timestamp from ISO8601 string.
/// @return true if successful
/// @param aISOString[in] : timestamp in ISO8601 basic or extended format, optionally including Z or +xx:xx zone specifier
/// @param aDefaultContext[in] : timezone context to use when ISO8601 does not specify a zone context or when aIgnoreZone is true
/// @param aIgnoreZone[in] : if set, timezone specification contained in ISO8601 is ignored. Resulting time context will be aDefaultContext (or TCTX_UNKNOWN for date-only)
bool TTimestampField::setAsISO8601(cAppCharP aISOString, timecontext_t aDefaultContext, bool aIgnoreZone)
{
  bool ok = ISO8601StrToTimestamp(aISOString, fTimestamp, fTimecontext);
  if (ok) {
    fAssigned=true;
    // if timestamp has unknown zone because it is a date-only or a duration, do NOT assign the default zone!
    if (aIgnoreZone || (TCTX_IS_UNKNOWN(fTimecontext) && !TCTX_IS_DATEONLY(fTimecontext) && !TCTX_IS_DURATION(fTimecontext))) {
      // set default context
      fTimecontext = aDefaultContext;
    }
  }
  else
    assignEmpty();
  // in all cases, this is now assigned (but probably empty if string was not ok)
  return ok;
} // TTimestampField::setAsISO8601



#ifdef EMAIL_FORMAT_SUPPORT

static cAppCharP  const rfc822_weekdays[7] = {
  "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};

const size_t rfc822maxMonthLen = 3;
static cAppCharP  const rfc822_months[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


/// @brief get timestamp as RFC(2)822 style date
/// @param aRFC822String[out] : timestamp in RFC(2)822 format
/// @param aTargetContext[in] : requests output context for timestamp. Use TCTX_UNKNOWN to show timestamp as-is.
/// @param aShowWithZone[in] : if set and timestamp is not floating, zone offset will be shown
void TTimestampField::getAsRFC822date(string &aRFC822String, timecontext_t aTargetContext, bool aShowWithZone)
{
  // get time in requested context
  lineartime_t ts = getTimestampAs(aTargetContext);
  // if target context was set to TCTX_UNKNOWN, this means that we want to use the stored context
  if (TCTX_IS_UNKNOWN(aTargetContext))
    aTargetContext = fTimecontext;
  // get elements
  sInt16 y,mo,d,h,mi,s,ms,w, moffs;
  lineartime2date(ts,&y,&mo,&d);
  lineartime2time(ts,&h,&mi,&s,&ms);
  w = lineartime2weekday(ts);
  // now print
  StringObjPrintf(
    aRFC822String,
    "%s, %02hd %s %04hd %02hd:%02hd:%02hd",
    rfc822_weekdays[w],
    d,rfc822_months[mo-1],y,
    h,mi,s
  );
  if (aShowWithZone && !TCTX_IS_UNKNOWN(aTargetContext)) {
    // get offset
    TzResolveToOffset(aTargetContext, moffs, ts, false, fGZonesP);
    StringObjAppendPrintf(
      aRFC822String,
      " %c%02hd%02hd",
      moffs>=0 ? '+' : '-',
      (uInt16)(abs(moffs) / MinsPerHour),
      (uInt16)(abs(moffs) % MinsPerHour)
    );
  }
} // TTimestampField::getAsRFC822date



/// @brief set timestamp as RFC(2)822 style date
/// @return true if successful
/// @param aRFC822String[in] : timestamp in RFC(2)822 format
/// @param aDefaultContext[in] : timezone context to use when RFC822 date does not specify a time zone
/// @param aIgnoreZone[in] : if set, timezone specification contained in input string is ignored. Resulting time context will be aDefaultContext
bool TTimestampField::setAsRFC822date(cAppCharP aRFC822String, timecontext_t aDefaultContext, bool aIgnoreZone)
{
  cAppCharP p;
  size_t inpSiz = strlen(aRFC822String);
  cAppCharP eot=aRFC822String+inpSiz;
  sInt16 minoffs=0;
  // check for weekday
  p = (cAppCharP) memchr(aRFC822String,',',inpSiz);
  if (p)
    p++; // skip comma
  else
    p=aRFC822String; // start at beginning
  // scan elements
  char month[rfc822maxMonthLen+1];
  sInt16 y,mo,d,h,m,s;
  s=0; // optional second
  // scan day
  while (p<eot && *p==' ') p++;
  if (p+2>eot) return false; // string too short
  p+=StrToShort(p,d,2);
  // scan month
  while (p<eot && *p==' ') p++;
  uInt8 mi=0;
  while (p<eot && *p!=' ' && mi<rfc822maxMonthLen) month[mi++]=*p++;
  month[mi]=0;
  // scan year
  while (p<eot && *p==' ') p++;
  if (p+4>eot) return false; // string too short
  p+=StrToShort(p,y,4);
  // scan time
  while (p<eot && *p==' ') p++;
  if (p+2>eot) return false; // string too short
  p+=StrToShort(p,h,2);
  if (p+3>eot || *p++ !=':') return false; // one for colon, two for minutes
  p+=StrToShort(p,m,2);
  if (p+3<=eot && *p==':') { // one for colon, two for minutes
    // optional second
    p++;
    p+=StrToShort(p,s,2);
  }
  // convert month
  for (mo=0; mo<12; mo++) if (strucmp(month,rfc822_months[mo])==0) break;
  if (mo==12) return false; // bad format
  mo++; // make month number
  // make timestamp
  fTimestamp = date2lineartime(y,mo,d) + time2lineartime(h,m,s,0);
  // check time zone
  bool neg=false;
  while (p<eot && *p==' ') p++;
  if (!aIgnoreZone) {
    aIgnoreZone=true; // assume invalid zone spec
    // check for numeric time zone
    if (*p=='-' || *p=='+') {
      // there is a timezone and we don't want to ignore it
      if (*p=='-') neg=true;
      if (p>=eot) return false; // string too short
      p++;
      sInt16 tzh,tzm;
      // scan timezone
      if (
        p+4<=eot &&
        StrToShort(p,tzh,2)==2 &&
        StrToShort(p+2,tzm,2)==2
      )
      {
        p+=4;
        // set time zone
        minoffs=((tzh*MinsPerHour)+tzm);
        if (neg) minoffs = -minoffs;
        // make zone
        fTimecontext = TCTX_OFFSCONTEXT(minoffs);
        aIgnoreZone=false; // zone spec valid
      }
    }
    else if (isalpha(*p)) {
      // could be time zone name (if not, ignore zone spec)
      aIgnoreZone = !TimeZoneNameToContext(p,fTimecontext,fGZonesP);
    }
  }
  // if no valid zone, use default
  if (aIgnoreZone) {
    // assume default time
    fTimecontext = aDefaultContext;
  }
  return true;
} // RFC822dateToTimeStamp


#endif // EMAIL_FORMAT_SUPPORT



/* end of TTimestampField implementation */


/*
 * Implementation of TDateField
 */


TDateField::TDateField(GZones *aGZonesP) :
  TTimestampField(aGZonesP)
{
  fTimecontext |= TCTX_DATEONLY;
} // TDateField::TDateField


TDateField::~TDateField()
{
} // TDateField::~TDateField


// compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem,
// SYSYNC_NOT_COMPARABLE if not equal and no ordering known or if field
// types do not match.
// Note: ordering may NOT be age relevant; it just means that an ordering
// for this field type exists.
// Note: Both fields must be assigned. NO TEST IS DONE HERE!
sInt16 TDateField::compareWith(TItemField &aItemField, bool aCaseInsensitive)
{
  sInt16 res;

  // check comparison with non-date
  if (!aItemField.isBasedOn(fty_timestamp))
    return TTimestampField::compareWith(aItemField); // handles all comparisons with other types
  // date comparison is context independent
  TTimestampField *sfP = static_cast<TTimestampField *>(&aItemField);
  // compare possible, return strcmp-type result
  // - convert to date only
  lineardate_t a =      getTimestampAs(TCTX_UNKNOWN+TCTX_DATEONLY) / linearDateToTimeFactor;
  lineardate_t b = sfP->getTimestampAs(TCTX_UNKNOWN+TCTX_DATEONLY) / linearDateToTimeFactor;
  // - compare dates
  res=a==b ? 0 : (a>b ? 1 : -1);
  if (res!=0) {
    // greater/less-type result is valid only if both sides are not empty
    if (     isEmpty()) return SYSYNC_NOT_COMPARABLE; // empty value, cannot compare
    if (sfP->isEmpty()) return SYSYNC_NOT_COMPARABLE; // empty value, cannot compare
  }
  return res;
} // TDateField::compareWith


// get date as ISO8601 string (default representation)
void TDateField::getAsString(string &aString)
{
  // use default rendering
  // - show with current context, but always date-only, no Z or zone info
  inherited::getAsISO8601(aString,TCTX_UNKNOWN+TCTX_DATEONLY,false,false,false,false);
} // TDateField::getAsString


// set date from ISO8601 string (default interpretation)
void TDateField::setAsString(cAppCharP aString)
{
  // default interpretation is floating date
  // - parse timestamp, ignore timezone
  if (setAsISO8601(aString, TCTX_UNKNOWN+TCTX_DATEONLY, true)) {
    // truncate to date-only
    fTimestamp = lineartime2dateonlyTime(fTimestamp);
  }
  // in all cases, this is now assigned (but probably empty)
  fAssigned=true;
} // TDateField::setAsString



/* end of TDateField implementation */


/*
 * Implementation of TIntegerField
 */


TIntegerField::TIntegerField()
{
  fInteger=0;
  fEmpty=true;
} // TIntegerField::TIntegerField


TIntegerField::~TIntegerField()
{
} // TIntegerField::~TIntegerField


#if defined(CHECKSUM_CHANGELOG) && !defined(RECORDHASH_FROM_DBAPI)

// changelog support: calculate CRC over contents
uInt16 TIntegerField::getDataCRC(uInt16 crc)
{
  // CRC over integer number
  return sysync_crc16_block(&fInteger,sizeof(fInteger),crc);
} // TIntegerField::getDataCRC

#endif


// assignment
TItemField& TIntegerField::operator=(TItemField &aItemField)
{
  // assignment of empty (could be EMPTY or UNASSIGNED) must be handled by base class
  if (aItemField.isEmpty()) return TItemField::operator=(aItemField);
  // handle non-empty myself
  if (aItemField.isBasedOn(fty_integer)) {
    const TIntegerField *sfP = static_cast<const TIntegerField *>(&aItemField);
    fInteger=sfP->fInteger;
    fEmpty=sfP->fEmpty;
    fAssigned=sfP->fAssigned;
  }
  else if (aItemField.getCalcType()==fty_integer) {
    fInteger=aItemField.getAsInteger();
    fEmpty=false;
    fAssigned=aItemField.isAssigned();
  }
  else
    TItemField::operator=(aItemField); // generic cross-type assignment (via string)
  return *this;
} // TIntegerField::operator=


// compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem,
// SYSYNC_NOT_COMPARABLE if not equal and no ordering known or if field
// types do not match.
// Note: ordering may NOT be age relevant; it just means that an ordering
// for this field type exists.
// Notes: - Both fields must be assigned. NO TEST IS DONE HERE!
//        - emptyness is not taken into account (empty counts as 0 here)
sInt16 TIntegerField::compareWith(TItemField &aItemField, bool aCaseInsensitive)
{
  if (aItemField.getCalcType()!=fty_integer) return TItemField::compareWith(aItemField); // handles generic comparisons
  fieldinteger_t otherInt = aItemField.getAsInteger();
  // compare possible, return strcmp-type result
  return fInteger==otherInt ? 0 : (fInteger>otherInt ? 1 : -1);
} // TIntegerField::compareWith


// get integer as integer
fieldinteger_t TIntegerField::getAsInteger(void)
{
  return fInteger;
} // TIntegerField::getAsInteger


// set integer
void TIntegerField::setAsInteger(fieldinteger_t aInteger)
{
  fInteger=aInteger;
  fAssigned=true;
  fEmpty=false;
} // TIntegerField::setAsInteger



// set integer from numeric string
void TIntegerField::setAsString(cAppCharP aString)
{
  // check for hex
  bool ok;
  if (strucmp(aString,"0x",2)==0) {
    // hex number
    #ifndef NO64BITINT
    ok = HexStrToULongLong(aString+2,*((uInt64 *)&fInteger));
    #else
    ok = HexStrToULong(aString+2,*((uInt32 *)&fInteger));
    #endif
  }
  else {
    // decimal integer number
    #ifndef NO64BITINT
    ok = StrToLongLong(aString,fInteger);
    #else
    ok = StrToLong(aString,fInteger);
    #endif
  }
  if (!ok)
    fInteger=0; // defined value
  // setting with a non-integer value makes the integer empty (but having integer value 0)
  fEmpty = !ok;
  // in all cases, this is now assigned (but probably with the default=0)
  fAssigned=true;
} // TIntegerField::setAsString


// get integer as numeric string
void TIntegerField::getAsString(string &aString)
{
  if (fEmpty)
    aString.erase();
  else {
    #ifndef NO64BITINT
    LONGLONGTOSTR(aString,PRINTF_LLD_ARG(fInteger));
    #else
    StringObjPrintf(aString,"%ld",(sInt32)fInteger);
    #endif
  }
} // TIntegerField::getAsString


/* end of TIntegerField implementation */


// factory function
TItemField *newItemField(const TItemFieldTypes aType, GZones *aGZonesP, bool aAsArray)
{
  #ifdef ARRAYFIELD_SUPPORT
  if (aAsArray) {
    return new TArrayField(aType,aGZonesP);
  }
  else
  #endif
  {
    switch (aType) {
      case fty_string: return new TStringField;
      case fty_telephone: return new TTelephoneField;
      case fty_integer: return new TIntegerField;
      case fty_timestamp: return new TTimestampField(aGZonesP);
      case fty_date: return new TDateField(aGZonesP);
      case fty_url: return new TURLField;
      case fty_multiline: return new TMultilineField;
      case fty_blob: return new TBlobField;
      case fty_none: return new TItemField; // base class, can represent EMPTY and UNASSIGNED
      default: return NULL;
    }
  }
} // newItemField


#ifdef ENGINEINTERFACE_SUPPORT

// TItemFieldKey
// =============

// get value's ID (e.g. internal index)
sInt32 TItemFieldKey::GetValueID(cAppCharP aName)
{
  // check special suffixes
  size_t namsz = strlen(aName);
  sInt32 fldID = 0; // basic
  if (namsz >= strlen(VALSUFF_TZNAME) &&
      strucmp(aName+namsz-strlen(VALSUFF_TZNAME),VALSUFF_TZNAME)==0) {
    // time zone name as string requested
    namsz-=7;
    fldID += VALID_FLAG_TZNAME;
  }
  else if (namsz >= strlen(VALSUFF_TZOFFS) &&
           strucmp(aName+namsz-strlen(VALSUFF_TZOFFS),VALSUFF_TZOFFS)==0) {
    // time zone offset in minutes
    namsz-=7;
    fldID += VALID_FLAG_TZOFFS;
  }
  else if (namsz >= strlen(VALSUFF_NORM) &&
           strucmp(aName+namsz-strlen(VALSUFF_NORM),VALSUFF_NORM)==0) {
    // normalized value
    namsz-=5;
    fldID += VALID_FLAG_NORM;
  }
  else if (namsz >= strlen(VALSUFF_ARRSZ) &&
           strucmp(aName+namsz-strlen(VALSUFF_ARRSZ),VALSUFF_ARRSZ)==0) {
    // array size
    namsz-=10;
    fldID += VALID_FLAG_ARRSIZ;
  }
  else if (namsz >= strlen(VALSUFF_NAME) &&
           strucmp(aName+namsz-strlen(VALSUFF_NAME),VALSUFF_NAME)==0) {
    // value name
    namsz-=8;
    fldID += VALID_FLAG_VALNAME;
  }
  else if (namsz >= strlen(VALSUFF_TYPE) &&
           strucmp(aName+namsz-strlen(VALSUFF_TYPE),VALSUFF_TYPE)==0) {
    // value type
    namsz-=8;
    fldID += VALID_FLAG_VALTYPE;
  }
  // check if this is only a query for the flags alone
  if (strucmp(aName,VALNAME_FLAG,namsz)==0)
    return fldID; // return flags alone
  // get fid for given name
  sInt16 fid = getFidFor(aName,namsz);
  if (fid==VARIDX_UNDEFINED)
    return KEYVAL_ID_UNKNOWN; // unknown field
  else
    return fldID + (sInt32)((uInt16)fid); // return FID in lo word, flags in highword
} // TItemFieldKey::GetValueID



// get value's native type
uInt16 TItemFieldKey::GetValueType(sInt32 aID)
{
  // fid is lower 16 bits of aID (and gets negative if bit15 of aID is set)
  sInt16 fid;
  *((uInt16 *)(&fid)) = aID & VALID_MASK_FID; // assign without sign extension
  // now get base field
  TItemField *baseFieldP = getBaseFieldFromFid(fid);
  if (baseFieldP) {
    // field exists
    // - check special queries
    if (aID & VALID_FLAG_ARRSIZ)
      return VALTYPE_INT16; // size is a 16-bit integer, regardless of field type
    else if (aID & VALID_FLAG_VALNAME)
      return VALTYPE_TEXT; // name of the value is text
    else if (aID & VALID_FLAG_VALTYPE)
      return VALTYPE_INT16; // VALTYPE_XXX are 16-bit integers
    else if (aID & VALID_FLAG_NORM)
      return VALTYPE_TEXT; // normalized value is text
    // - otherwise, valtype depends on field type
    TItemFieldTypes fty = baseFieldP->getElementType();
    // return native type
    switch (fty) {
      case fty_none:
        return VALTYPE_UNKNOWN;
      case fty_blob:
        return VALTYPE_BUF;
      case fty_string:
      case fty_multiline:
      case fty_telephone:
      case fty_url:
        return VALTYPE_TEXT;
      case fty_integer:
        return VALTYPE_INT64;
      case fty_timestamp:
      case fty_date:
        if (aID & VALID_FLAG_TZNAME)
          return VALTYPE_TEXT; // time zone name is a text
        else if (aID & VALID_FLAG_TZOFFS)
          return VALTYPE_INT16; // minute offset is 16-bit integer
        else
          return VALTYPE_TIME64; // time values
    case numFieldTypes:
        // invalid
        break;
    } // switch
  }
  // no field, no type
  return VALTYPE_UNKNOWN;
} // TItemFieldKey::GetValueType


// helper for getting a leaf field pointer (interpretation of aRepOffset depends on array field or not)
TItemField *TItemFieldKey::getFieldFromFid(sInt16 aFid, sInt16 aRepOffset, bool aExistingOnly)
{
  TItemField *fieldP=NULL;
  // get field (or base field)
  #ifdef ARRAYFIELD_SUPPORT
  fieldP = getBaseFieldFromFid(aFid);
  if (!fieldP) return NULL; // no field
  if (fieldP->isArray()) {
    // use aRepOffset as array index
    fieldP = fieldP->getArrayField(aRepOffset,aExistingOnly);
  }
  else
  #endif
  {
    // use aRepOffset as fid offset
    #ifdef SCRIPT_SUPPORT
    if (aFid<0) aRepOffset=0; // locals are never offset
    #endif
    fieldP = getBaseFieldFromFid(aFid+aRepOffset);
  }
  return fieldP;
} // TItemFieldKey::getFieldFromFid




// get value
TSyError TItemFieldKey::GetValueInternal(
  sInt32 aID, sInt32 aArrayIndex,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  string sval;
  TItemField *fieldP;
  // fid is lower 16 bits of aID (and gets negative if bit15 of aID is set)
  sInt16 fid;
  *((uInt16 *)(&fid)) = aID & VALID_MASK_FID; // assign without sign extension

  // check for special queries not actually accessing a leaf field
  if (aID & VALID_FLAG_ARRSIZ) {
    // return array size (returns DB_NotFound for non-arrays)
    fieldP = getBaseFieldFromFid(fid);
    // if not an array, return DB_NotFound
    if (!fieldP || !fieldP->isArray())
      return DB_NotFound; // there is no array size for non-arrays
    // return size
    aValSize=sizeof(sInt16);
    // value if enough room
    if (aBufSize>=aValSize)
      *((sInt16 *)aBuffer)=fieldP->arraySize();
    // ok
    return LOCERR_OK;
  }
  else if (aID & VALID_FLAG_VALNAME) {
    // only return name of field
    if (!getFieldNameFromFid(fid,sval)) return DB_NotFound;
    if (aBufSize>=sval.size())
      memcpy(aBuffer,sval.c_str(),sval.size()); // copy name data
    aValSize=sval.size();
    return LOCERR_OK;
  }
  else if (aID & VALID_FLAG_VALTYPE) {
    // only return VALTYPE of field
    uInt16 valtype = GetValueType(aID & ~VALID_FLAG_VALTYPE);
    aValSize = sizeof(valtype);
    if (aBufSize>=aValSize)
      memcpy(aBuffer,&valtype,aValSize); // copy valtype uInt16
    return LOCERR_OK;
  }
  // Get actual data - we need the leaf field (unless we pass <0 for aArrayIndex, which accesses the array base field)
  fieldP = getFieldFromFid(fid, aArrayIndex, true); // existing array elements only
  if (!fieldP) {
    // array instance does not exist
    return LOCERR_OUTOFRANGE;
  }
  else {
    // leaf field (or explicitly requested array base field) exists
    if (!fieldP->isAssigned()) return DB_NotFound; // no content found because none assigned
    if (fieldP->isEmpty()) return DB_NoContent; // empty
    if (fieldP->isArray()) return LOCERR_OUTOFRANGE; // no real access for array base field possible - if we get here it means non-empty
    // assigned and not empty, return actual value
    TItemFieldTypes fty = fieldP->getType();
    appPointer valPtr = NULL;
    size_t sz;
    fieldinteger_t intVal;
    lineartime_t ts;
    timecontext_t tctx;
    sInt16 minOffs;
    TTimestampField *tsFldP;
    if (aID & VALID_FLAG_NORM) {
      // for all field types: get normalized string value
      fieldP->getAsNormalizedString(sval);
      aValSize = sval.size();
      valPtr = (appPointer)sval.c_str();
    }
    else {
      switch (fty) {
        case fty_timestamp:
        case fty_date:
          tsFldP = static_cast<TTimestampField *>(fieldP);
          if (aID & VALID_FLAG_TZNAME) {
            // time zone name is a text
            sval.erase(); // no zone
            tctx = tsFldP->getTimeContext();
            if (!TCTX_IS_UNKNOWN(tctx) || TCTX_IS_DURATION(tctx) || TCTX_IS_DATEONLY(tctx)) {
              // has a zone (or is duration/dateonly), get name
              TimeZoneContextToName(tctx, sval, tsFldP->getGZones());
            }
            aValSize = sval.size();
            valPtr = (appPointer)sval.c_str();
          }
          else if (aID & VALID_FLAG_TZOFFS) {
            // minute offset as 16-bit integer
            aValSize = sizeof(sInt16);
            if (
              tsFldP->isFloating() ||
              !TzResolveToOffset(tsFldP->getTimeContext(), minOffs, tsFldP->getTimestampAs(TCTX_UNKNOWN), false, tsFldP->getGZones())
            )
              return DB_NotFound; // cannot get minute offset or floating timestamp -> no result
            // return minute offset
            valPtr = &minOffs;
          }
          else {
            // return timestamp itself (either as-is or in UTC, if TMODE_FLAG_FLOATING not set)
            aValSize = sizeof(lineartime_t);
            if (fTimeMode & TMODE_FLAG_FLOATING)
              ts = tsFldP->getTimestampAs(TCTX_UNKNOWN); // as-is
            else
              ts = tsFldP->getTimestampAs(TCTX_UTC,&tctx); // always as UTC, will be converted to SYSTEM possibly by caller
            // return timestamp
            valPtr = &ts;
          }
          break;
        case fty_integer:
          aValSize = sizeof(fieldinteger_t);
          intVal = fieldP->getAsInteger();
          valPtr = &intVal;
          break;
        case fty_none:
          aValSize = 0;
          break;
        case fty_blob:
          static_cast<TBlobField *>(fieldP)->getBlobDataPtrSz(valPtr,sz);
          aValSize = sz;
          break;
        case fty_string:
        case fty_multiline:
        case fty_telephone:
        case fty_url:
          aValSize = fieldP->getStringSize();
          valPtr = (appPointer)static_cast<TStringField *>(fieldP)->getCStr();
          break;
      case numFieldTypes:
          // invalid
          break;
      } // switch
    }
    // now we have valid aValSize and valPtr
    if (aBuffer && aBufSize>=aValSize) {
      // copy value data
      memcpy(aBuffer,valPtr,aValSize);
    }
  }
  // ok
  return LOCERR_OK;
} // TItemFieldKey::GetValueInternal



// set value
TSyError TItemFieldKey::SetValueInternal(
  sInt32 aID, sInt32 aArrayIndex,
  cAppPointer aBuffer, memSize aValSize
)
{
  TItemField *fieldP;
  // fid is lower 16 bits of aID (and gets negative if bit15 of aID is set)
  sInt16 fid;
  *((uInt16 *)(&fid)) = aID & VALID_MASK_FID; // assign without sign extension

  // check for special array size query
  if (aID & VALID_FLAG_ARRSIZ) {
    // array size is not writable
    return DB_NotAllowed;
  }
  // now get leaf field
  fieldP = getFieldFromFid(fid, aArrayIndex, false); // create element if needed
  if (!fieldP) {
    // should not happen
    return DB_Error;
  }
  else {
    // leaf field exists, set value
    fWritten = true;
    TItemFieldTypes fty = fieldP->getType();
    fieldinteger_t intVal;
    lineartime_t ts;
    timecontext_t tctx;
    sInt16 minOffs;
    string sval;
    TTimestampField *tsFldP;
    // treat setting normalized value like setting as string
    if (aID & VALID_FLAG_NORM)
      fty = fty_string; // treat like string
    // handle NULL (empty) case
    if (aBuffer==0) {
      // buffer==NULL means NULL value
      fieldP->assignEmpty();
      return LOCERR_OK;
    }
    // now handle according to type
    switch (fty) {
      case fty_timestamp:
      case fty_date:
        tsFldP = static_cast<TTimestampField *>(fieldP);
        if (aID & VALID_FLAG_TZNAME) {
          // set time zone by name
          sval.assign((cAppCharP)aBuffer,aValSize);
          tctx = TCTX_UNKNOWN;
          if (!sval.empty()) {
            // convert
            if (!TimeZoneNameToContext(sval.c_str(), tctx, tsFldP->getGZones()))
              return LOCERR_BADPARAM; // bad timezone name
          }
          // set context
          tsFldP->setTimeContext(tctx);
        }
        else if (aID & VALID_FLAG_TZOFFS) {
          // minute offset as 16-bit integer
          minOffs = *((sInt16 *)aBuffer);
          // set context
          tsFldP->setTimeContext(TCTX_MINOFFSET(minOffs));
        }
        else {
          // set timestamp itself (either as-is or in UTC, if TMODE_FLAG_FLOATING not set)
          ts = *((lineartime_t *)aBuffer);
          if ((fTimeMode & TMODE_FLAG_FLOATING)==0)
            tsFldP->setTimestampAndContext(ts,TCTX_UTC); // incoming timestamp is UTC, set it as such
          else
            tsFldP->setTimestamp(ts); // just set timestamp as-is and don't touch rest
        }
        break;
      case fty_integer:
        intVal = *((fieldinteger_t *)aBuffer);
        fieldP->setAsInteger(intVal);
        break;
      case fty_blob:
        static_cast<TBlobField *>(fieldP)->setBlobDataPtrSz((void *)aBuffer,aValSize);
        break;
      case fty_string:
      case fty_multiline:
      case fty_telephone:
      case fty_url:
        fieldP->setAsString((cAppCharP)aBuffer,aValSize);
        break;
      case fty_none:
      case numFieldTypes:
        // invalid
        break;
    } // switch
  }
  // ok
  return LOCERR_OK;
} // TItemFieldKey::SetValueInternal


#endif // ENGINEINTERFACE_SUPPORT



} // namespace sysync

// eof
