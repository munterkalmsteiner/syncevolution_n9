/**
 *  @File     engineinterface.cpp
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TEngineInterface - common interface to SySync engine for SDK
 *
 *    Copyright (c) 2007-2011 by Synthesis AG + plan44.ch
 *
 */


#include "prefix_file.h"

#ifdef ENGINEINTERFACE_SUPPORT

#include "engineinterface.h"
#include "syserial.h"
#include "syncappbase.h"
#include "SDK_util.h"

namespace sysync {


// TSettingsKeyImpl
// ================


TSettingsKeyImpl::TSettingsKeyImpl(
  TEngineInterface *aEngineInterfaceP
) :
  fEngineInterfaceP(aEngineInterfaceP),
  fImplicitParentKeyP(NULL)
{
  // init defaults from engine interface
  fCharSet = fEngineInterfaceP->fCharSet;
  fBigEndian = fEngineInterfaceP->fBigEndian;
  fLineEndMode = fEngineInterfaceP->fLineEndMode;
  // init hardcoded defaults
  fTimeMode = TMODE_LINEARTIME+TMODE_FLAG_FLOATING; // linear time (as floating, which means UTC for engine timestamps)
} // TSettingsKeyImpl::TSettingsKeyImpl


TSettingsKeyImpl::~TSettingsKeyImpl()
{
  // deleting object means closing key
  // - At this point (in baseclass), actual key implementation is closed
  //   already, so we can delete implicitly opened ancestors now recursively
  if (fImplicitParentKeyP) {
    delete fImplicitParentKeyP;
    fImplicitParentKeyP=NULL;
  }
} // TSettingsKeyImpl::TSettingsKeyImpl


// open subkey(chain) by path
// - walks down through needed subkeys
TSyError TSettingsKeyImpl::OpenKeyByPath(
  TSettingsKeyImpl *&aSettingsKeyP, // might be set even if overall open fails - caller must delete object passed back in case of failure
  cAppCharP aPath, uInt16 aMode,
  bool aImplicit // if set, this means that THIS key was implicitly opened, and should be implicitly closed as well
)
{
  TSyError sta;
  TSettingsKeyImpl *subKeyP=NULL;

  // as long as we haven't openend anything, make sure pointer returned is NULL
  aSettingsKeyP=NULL;
  // open by relative path, starting at myself
  if (aPath==NULL) return LOCERR_WRONGUSAGE;
  // strip leading separators
  while (*aPath==SETTINGSKEY_PATH_SEPARATOR) aPath++;
  // search end of string or next separator
  cAppCharP e = aPath;
  while (*e && *e!=SETTINGSKEY_PATH_SEPARATOR) e++;
  // open the element we have found
  sta = OpenSubKeyByName(aSettingsKeyP,aPath,e-aPath,aMode);
  if (sta == LOCERR_OK) {
    // immediate subkey opened successfully
    // - if this is an implicit open, link back to myself as I must be deleted when subkey is deleted
    if (aImplicit) {
      aSettingsKeyP->fImplicitParentKeyP = this;
    }
    // new opened key inherits base key's value formatting settings
    aSettingsKeyP->SetTextMode(fCharSet, fLineEndMode,fBigEndian);
    aSettingsKeyP->SetTimeMode(fTimeMode);
    // - check if chain continues
    while (*e==SETTINGSKEY_PATH_SEPARATOR) e++; // strip leading separators
    if (*e) {
      // another path element follows, open it recursively
      // - this is implicit in any case!
      sta = aSettingsKeyP->OpenKeyByPath(subKeyP,e,aMode,true);
      if (subKeyP != NULL) {
        // more subkeys were opened, possibly not entire chain as requested. But in
        // any case, we need to pass back the rightmost opened key in the path as this
        // must be deleted by the caller (causing all implicitly related objects to delete as well)
        aSettingsKeyP = subKeyP;
      }
    }
  }
  // return status
  return sta;
} // TSettingsKeyImpl::OpenKeyByPath


const uInt16 engineCharSets[numCharSets] = {
  CHS_UNKNOWN,
  CHS_ASCII,
  CHS_ANSI,
  CHS_ISO_8859_1,
  CHS_UTF8,
  CHS_UTF16,
  #ifdef CHINESE_SUPPORT
  CHS_GB2312,
  CHS_CP936
  #endif
};

const uInt16 engineLineEndModes[numLineEndModes] = {
  LEM_NONE,
  LEM_UNIX,
  LEM_MAC,
  LEM_DOS,
  LEM_CSTR,
  LEM_FILEMAKER
};


// Set text format parameters
TSyError TSettingsKeyImpl::SetTextMode(uInt16 aCharSet, uInt16 aLineEndMode, bool aBigEndian)
{
  // translate charset
  uInt16 chs;
  for (chs=0; chs<numCharSets; chs++) {
    if (engineCharSets[chs]==aCharSet) break;
  }
  if (chs==numCharSets) return LOCERR_BADPARAM;
  // translate lineendmode
  uInt16 lem;
  for (lem=0; lem<numLineEndModes; lem++) {
    if (engineLineEndModes[lem]==aLineEndMode) break;
  }
  if (lem==numLineEndModes) return LOCERR_BADPARAM;
  // assign them now they are checked ok
  fCharSet = (TCharSets)chs;
  fLineEndMode = (TLineEndModes)lem;
  // big endian flag
  fBigEndian = aBigEndian;
  return LOCERR_OK;
} // TSettingsKeyImpl::SetTextMode



// Reads a named value in specified format into passed memory buffer
TSyError TSettingsKeyImpl::GetValueByID(
  sInt32 aID, sInt32 aArrayIndex, uInt16 aValType,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TSyError sta;
  string valStr;
  cAppCharP txt;
  string convStr;
  sInt64 tempInt=0;
  lineartime_t tempTime;
  timecontext_t tctx;

  // get native type of value (and check validity of ID - invalid IDs must return VALTYPE_UNKNOWN)
  uInt16 valType = GetValueType(aID);
  if (valType==VALTYPE_UNKNOWN) return DB_NotFound;
  // direct return in case requested type matches native type or raw data (VALTYPE_BUF) is requested
  if (
    (aValType == VALTYPE_BUF) || // we want raw buffer data
    ((valType == aValType) &&
     (valType != VALTYPE_TEXT || (fCharSet==chs_utf8 && fLineEndMode==lem_cstr)) &&
     (valType != VALTYPE_TIME64 || ((fTimeMode & TMODE_MODEMASK)==TMODE_LINEARTIME)) )
  ) {
    // requested type&format matches native or we are requesting a buffer -> we can return native value directly
    if (aValType == VALTYPE_TEXT) {
      if (!aBuffer || aBufSize==0) {
        // only measure size
        return GetValueInternal(aID,aArrayIndex,NULL,0,aValSize);
      }
      else {
        // low level text routines do not set terminators for simplicity, so make sure we have one here
        // - request only max one char less than buf can hold, so we always have room for a terminator
        sta = GetValueInternal(aID,aArrayIndex,aBuffer,aBufSize-1,aValSize);
        // - make sure we have a NUL terminator at the very end of the buffer in all cases
        ((appCharP)aBuffer)[aBufSize-1] = 0; // ultimate terminator
        // - also make sure we have a terminator at the end of the actual string
        if (sta==LOCERR_OK && aValSize<aBufSize)
          ((appCharP)aBuffer)[aValSize] = 0;
        // signal LOCERR_TRUNCATED if ok, but buffer is not large enough
        if (sta==LOCERR_OK && aValSize>aBufSize-1) {
          sta = LOCERR_TRUNCATED;
          aValSize = aBufSize-1; // return actual size, not untruncated one
        }
        return sta;
      }
    }
    else {
      // non-text, simply return value
      sta = GetValueInternal(aID,aArrayIndex,aBuffer,aBufSize,aValSize);
      if (sta==LOCERR_OK && aBufSize && aBuffer && aValSize>aBufSize) {
        // not only measuring size, check for truncation
        if (aValType==VALTYPE_BUF) {
          // in case of buffer, we call this "truncated" (we don't know if the result is usable or not, depends on data itself)
          sta = LOCERR_TRUNCATED;
          aValSize = aBufSize; // return actual size, not untruncated one
        }
        else {
          // in other cases, too small buffer makes result unusable, so we don't call it "truncated"
          // AND: we return the needed buffer size
          sta = LOCERR_BUFTOOSMALL;
        }
      }
      return sta;
    }
  }
  // some kind of conversion needed
  // - get native size of value first
  memSize valSiz;
  sta = GetValueInternal(aID,aArrayIndex,NULL,0,valSiz);
  if (sta!=LOCERR_OK) return sta;
  // - allocate matching buffer
  memSize bufSiz = valSiz+1; // extra room for terminator
  uInt8P bufP = (uInt8P) malloc(bufSiz);
  bufP[valSiz]=0; // make sure we have a terminator
  if (bufP==NULL) return LOCERR_OUTOFMEM;
  // - get value in native type
  sta = GetValueInternal(aID,aArrayIndex,bufP,bufSiz,valSiz);
  if (sta==LOCERR_OK) {
    // value conversion matrix
    // - switch by native value type
    switch (valType) {
      // Timestamp native type (always VALTYPE_TIME64)
      case VALTYPE_TIME64:
        tempTime = *((lineartime_t *)bufP);
        tctx = (fTimeMode & TMODE_FLAG_UTC)==0 ? TCTX_SYSTEM : TCTX_UTC;
        // convert it from internal format (UTC), unless we request floating
        if ((fTimeMode & TMODE_FLAG_FLOATING)==0)
          TzConvertTimestamp(tempTime,TCTX_UTC,tctx,getEngineInterface()->getSyncAppBase()->getAppZones());
        else
          tctx = TCTX_UNKNOWN; // make sure that ISO8601 representation has no TZ info in floating mode
        // now return
        if (aValType==VALTYPE_TEXT) {
          // return time as text
          TimestampToISO8601Str(valStr, tempTime, tctx, false, false);
          txt = valStr.c_str();
          goto textConv;
        }
        // no time will return an error code
        if (tempTime==noLinearTime) {
          // no time
          sta=DB_NoContent; // indicates NO value
          break;
        }
        // return time as integer number
        switch (fTimeMode & TMODE_MODEMASK) {
          case TMODE_LINEARDATE:
            tempInt = tempTime / linearDateToTimeFactor;
            goto intConv;
          case TMODE_UNIXTIME_MS:
            tempInt = tempTime-UnixToLineartimeOffset; // lineartime_t (that is, milliseconds), but with unix epoch origin
            goto intConv;
          case TMODE_UNIXTIME:
            tempInt = (tempTime-UnixToLineartimeOffset)/secondToLinearTimeFactor;
            goto intConv;
          default :
            tempInt = tempTime;
            goto intConv;
        }
        break;
      // Integer native types
      case VALTYPE_INT8:
        tempInt = *((sInt8 *)bufP);
        goto intConv;
      case VALTYPE_INT16:
        tempInt = *((sInt16 *)bufP);
        goto intConv;
      case VALTYPE_INT32:
        tempInt = *((sInt32 *)bufP);
        goto intConv;
      case VALTYPE_INT64:
        tempInt = *((sInt64 *)bufP);
      intConv:
        // convert integer into desired target type
        switch (aValType) {
          case VALTYPE_INT8:
          case VALTYPE_ENUM:
            aValSize=1;
            if (aBufSize<aValSize) sta=LOCERR_BUFTOOSMALL;
            else *((sInt8 *)aBuffer) = tempInt;
            break;
          case VALTYPE_INT16:
            aValSize=2;
            if (aBufSize<aValSize) sta=LOCERR_BUFTOOSMALL;
            else *((sInt16 *)aBuffer) = tempInt;
            break;
          case VALTYPE_INT32:
          case VALTYPE_TIME32:
            aValSize=4;
            if (aBufSize<aValSize) sta=LOCERR_BUFTOOSMALL;
            else *((sInt32 *)aBuffer) = tempInt;
            break;
          case VALTYPE_INT64:
          case VALTYPE_TIME64:
            aValSize=8;
            if (aBufSize<aValSize) sta=LOCERR_BUFTOOSMALL;
            else *((sInt64 *)aBuffer) = tempInt;
            break;
          case VALTYPE_TEXT:
            StringObjPrintf(valStr,PRINTF_LLD,PRINTF_LLD_ARG(tempInt));
            txt = valStr.c_str();
            goto textConv;
            break;
        }
        break;
      // Text native type
      case VALTYPE_TEXT:
        txt = (cAppCharP)bufP;
      textConv:
        // text can only be returned as text (implicit Int/time scanning makes no sense)
        if (aValType!=VALTYPE_TEXT)
          sta = LOCERR_WRONGUSAGE; // conversion not allowed
        else {
          // converted to requested charset/lineend
          if (fCharSet==chs_utf16) {
            if (!appendUTF8ToUTF16ByteString(
              txt,
              convStr,
              fBigEndian,
              fLineEndMode,
              aBufSize>0 ? aBufSize-2 : 0 // max output size (but no limit if measuring actual size)
            ))
              sta = LOCERR_TRUNCATED;
            // return (possibly truncated) size
            aValSize=convStr.size();
            // this will be added as part of the content, and gives a 16bit NUL terminator
            convStr+=(char)0;
          }
          else {
            // 8-bit character encoding
            if (!appendUTF8ToString(
              txt,
              convStr,
              fCharSet,
              fLineEndMode,
              qm_none,
              aBufSize>0 ? aBufSize-1 : 0 // max output size (but no limit if measuring actual size)
            ))
              sta = LOCERR_TRUNCATED;
            // return (possibly truncated) size
            aValSize=convStr.size();
          }
          // if requested, return value into buffer
          if (aBufSize>0) {
            // copy including implicit NUL terminator BYTE (in case of UTF16, there is an extra explicit NUL char in the convStr itself to make a 16-bit NUL!)
            memcpy(aBuffer,(appPointer)convStr.c_str(),convStr.size()+1>aBufSize ? aBufSize : convStr.size()+1);
          }
          else {
            // measuring size always returns OK
            sta = LOCERR_OK;
          }
        }
        break;
    } // switch by native type
  } // if value returned

  // - return temp buffer
  free(bufP);
  // - return status
  return sta;
} // TSettingsKeyImpl::GetValueByID


// Writes a named value in specified format passed in memory buffer
TSyError TSettingsKeyImpl::SetValueByID(
  sInt32 aID, sInt32 aArrayIndex, uInt16 aValType,
  cAppPointer aBuffer, memSize aValSize
)
{
  TSyError sta = LOCERR_OK;
  sInt64 tempInt;
  lineartime_t tempTime;
  timecontext_t tctx;
  appPointer bP;
  memSize siz;
  string convStr;

  // get native type of value (and check validity of ID - invalid IDs must return VALTYPE_UNKNOWN)
  uInt16 valType = GetValueType(aID);
  if (valType==VALTYPE_UNKNOWN) return DB_NotFound;
  // measure value for strings
  if (aValType == VALTYPE_TEXT && aValSize == memSize(-1)) {
    if (fCharSet==chs_utf16)
      return LOCERR_NOTIMP; // measuring UTF-16 not supported yet
      //aValSize = wcslen((short *)aBuffer)/2;
    else
      aValSize = strlen((char *)aBuffer);
  }
  // we can set directly if input type matches native type
  if (
    (aValType == VALTYPE_BUF) ||
    ((valType == aValType) &&
     (valType != VALTYPE_TEXT || (fCharSet==chs_utf8 && fLineEndMode==lem_cstr)) &&
     (valType != VALTYPE_TIME64 || ((fTimeMode & TMODE_MODEMASK)==TMODE_LINEARTIME)) )
  ) {
    // presented type&format matches native or we are setting bytes 1:1 anyway -> we can set native value directly
    return SetValueInternal(aID,aArrayIndex,aBuffer,aValSize);
  }
  // some kind of conversion needed
  // - switch by presented value type
  switch (aValType) {
    // Set "no value"
    case VALTYPE_NULL:
      sta = SetValueInternal(aID,aArrayIndex,NULL,0);
      break;

    // Text presented
    case VALTYPE_TEXT:
      // first convert into internal text format
      if (fCharSet==chs_utf16) {
        appendUTF16AsUTF8((uInt16 *)aBuffer,aValSize/2,fBigEndian,convStr, true, true);
      }
      else {
        string s; s.assign((cAppCharP)aBuffer,aValSize);
        appendStringAsUTF8(s.c_str(), convStr, fCharSet, lem_cstr, true);
      }
      // possibly convert text to native
      switch (valType) {
        case VALTYPE_TIME64:
          // convert text to internal time format
          ISO8601StrToTimestamp(convStr.c_str(), tempTime, tctx);
          // if not floating, assume target timestamp is UTC, so ISO string w/o time zone is treated as system time for convenience
          if ((fTimeMode & TMODE_FLAG_FLOATING)==0)
            TzConvertTimestamp(tempTime, tctx, TCTX_UTC, getEngineInterface()->getSyncAppBase()->getAppZones(), TCTX_SYSTEM);
          siz=8; bP = &tempTime;
          break;
        case VALTYPE_INT8:
        case VALTYPE_INT16:
        case VALTYPE_INT32:
        case VALTYPE_INT64:
          // convert text to integer
          StrToLongLong(convStr.c_str(),tempInt);
          goto intConv;
        case VALTYPE_TEXT:
          siz=convStr.size();
          bP=(appPointer)convStr.c_str();
          break;
        default:
          // cannot return as text
          return LOCERR_WRONGUSAGE;
      }
      // set it
      sta = SetValueInternal(aID,aArrayIndex,bP,siz);
      break;

    // Timestamp presented
    case VALTYPE_TIME32:
      if (aValSize<4) return LOCERR_BUFTOOSMALL;
      tempTime = *((sInt32 *)aBuffer);
      goto timeConv;
    case VALTYPE_TIME64:
      if (aValSize<8) return LOCERR_BUFTOOSMALL;
      tempTime = *((sInt64 *)aBuffer);
    timeConv:
      switch (fTimeMode & TMODE_MODEMASK) {
        case TMODE_LINEARDATE:
          tempInt = tempTime * linearDateToTimeFactor;
          break;
        case TMODE_UNIXTIME_MS:
          tempInt = tempTime+UnixToLineartimeOffset;
          break;
        case TMODE_UNIXTIME:
          tempInt = secondToLinearTimeFactor*tempTime+UnixToLineartimeOffset;
          break;
        case TMODE_LINEARTIME:
        default :
          tempInt = tempTime;
          break;
      }
      tctx = (fTimeMode & TMODE_FLAG_UTC)==0 ? TCTX_SYSTEM : TCTX_UTC;
      // convert to UTC if needed
      if ((fTimeMode & TMODE_FLAG_FLOATING)==0)
        TzConvertTimestamp(tempInt,tctx,TCTX_UTC,getEngineInterface()->getSyncAppBase()->getAppZones());
      goto intConv;

    // Integer presented types
    case VALTYPE_INT8:
    case VALTYPE_ENUM:
      if (aValSize<1) return LOCERR_BUFTOOSMALL;
      tempInt = *((sInt8 *)aBuffer);
      goto intConv;
    case VALTYPE_INT16:
      if (aValSize<2) return LOCERR_BUFTOOSMALL;
      tempInt = *((sInt16 *)aBuffer);
      goto intConv;
    case VALTYPE_INT32:
      if (aValSize<4) return LOCERR_BUFTOOSMALL;
      tempInt = *((sInt32 *)aBuffer);
      goto intConv;
    case VALTYPE_INT64:
      if (aValSize<8) return LOCERR_BUFTOOSMALL;
      tempInt = *((sInt64 *)aBuffer);
    intConv:
      // convert integer into native type
      union {
        sInt64 buffer64;
        sInt32 buffer32;
        sInt16 buffer16;
        sInt8  buffer8;
      } buffer;
      bP = &buffer;
      switch (valType) {
        case VALTYPE_INT8:
          siz=1; buffer.buffer8 = tempInt;
          break;
        case VALTYPE_INT16:
          siz=2; buffer.buffer16 = tempInt;
          break;
        case VALTYPE_INT32:
          siz=4; buffer.buffer32 = tempInt;
          break;
        case VALTYPE_INT64:
        case VALTYPE_TIME64: // native timestamp
          siz=8; buffer.buffer64 = tempInt;
          break;
        default:
          // other types (like text) cannot set as integer
          return LOCERR_WRONGUSAGE; // conversion not allowed
          break;
      }
      // now write int/time value
      sta = SetValueInternal(aID,aArrayIndex,bP,siz);
      break;

  } // switch by presented type

  return sta;
} // TSettingsKeyImpl::SetValueByID



#define VALID_IDXOFFS_VALTYPE 0x100000
#define VALID_MASK_IDX        0x00FFFF


// helper for detecting generic field attribute access
bool TSettingsKeyImpl::checkFieldAttrs(cAppCharP aName, size_t &aBaseNameSize, sInt32 &aFldID)
{
  aBaseNameSize = strlen(aName);
  aFldID = 0; // basic
  if (aBaseNameSize >= strlen(VALSUFF_TYPE) &&
      strucmp(aName+aBaseNameSize-strlen(VALSUFF_TYPE),VALSUFF_TYPE)==0) {
    // value type
    aBaseNameSize-=strlen(VALSUFF_TYPE);
    aFldID += VALID_IDXOFFS_VALTYPE;
  }
  if (strucmp(aName,VALNAME_FLAG,aBaseNameSize)==0)
    return true; // asking for flag mask only
  return false; // aFldID is only the flag mask, caller must add base index
} // TSettingsKeyImpl::checkFieldAttrs



// helper for returning generic field attribute type
bool TSettingsKeyImpl::checkAttrValueType(sInt32 aID, uInt16 &aValType)
{
  if (aID>0 && (aID & VALID_IDXOFFS_VALTYPE)) {
    aValType = VALTYPE_INT16; // valtype is always uInt16
    return true;
  }
  return false;
} // TSettingsKeyImpl::checkAttrValueType



// helper for returning generic field attribute values
bool TSettingsKeyImpl::checkAttrValue(
  sInt32 aID, sInt32 aArrayIndex,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  if (aID>0 && (aID & VALID_IDXOFFS_VALTYPE)) {
    // return type, not value
    aValSize = 2;
    uInt16 valtype = GetValueType(aID & VALID_MASK_IDX);
    if (aBufSize>=aValSize) {
      memcpy(aBuffer,&valtype,aValSize);
    }
    return true;
  }
  return false;
} // TSettingsKeyImpl::checkAttrValue






// TReadOnlyInfoKey
// ================


// get value's ID (e.g. internal index)
sInt32 TReadOnlyInfoKey::GetValueID(cAppCharP aName)
{
  const TReadOnlyInfo *tblP = getInfoTable();

  sInt32 idx=0;
  size_t namsz;
  sInt32 fldID;
  if (checkFieldAttrs(aName,namsz,fldID))
    return fldID;
  while(tblP && idx<numInfos()) {
    if (strucmp(aName,tblP[idx].valName,namsz)==0) {
      return fldID+idx; // found
    }
    // next
    idx++;
  }
  // now check for iterator commands
  if (strucmp(aName,VALNAME_FIRST)==0) {
    fIterator=0;
    if (fIterator<numInfos()) return fIterator;
  }
  else if (strucmp(aName,VALNAME_NEXT)==0) {
    if (fIterator<numInfos()) fIterator++;
    if (fIterator<numInfos()) return fIterator;
  }
  // not found
  return KEYVAL_ID_UNKNOWN; // unknown
} // TReadOnlyInfoKey::GetValueID


// get value's native type
uInt16 TReadOnlyInfoKey::GetValueType(sInt32 aID)
{
  uInt16 valType;
  if (checkAttrValueType(aID,valType))
    return valType;
  if (aID>=numInfos()) return VALTYPE_UNKNOWN;
  return getInfoTable()[aID].valType;
} // TReadOnlyInfoKey::GetValueType


// get value
TSyError TReadOnlyInfoKey::GetValueInternal(
  sInt32 aID, sInt32 aArrayIndex,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  if (checkAttrValue(aID,aArrayIndex,aBuffer,aBufSize,aValSize))
    return LOCERR_OK;
  const TReadOnlyInfo *infoP = &(getInfoTable()[aID]);
  memSize siz = infoP->valSiz;
  if (siz==0 && infoP->valType==VALTYPE_TEXT)
    siz = strlen((cAppCharP)(infoP->valPtr));
  // return actual size
  aValSize = siz;
  // return requested amount of data or all we have, whatever is smaller
  if (siz>aBufSize)
    siz=aBufSize; // limit to buffer size (aBufSiz is compensated for NUL terminator already, so we can use memcpy for strings as well)
  memcpy(aBuffer,infoP->valPtr,siz);
  return LOCERR_OK;
} // TReadOnlyInfoKey::GetValueInternal



// TConfigVarKey
// =============


// get value's ID (config vars do NOT have a real ID!)
sInt32 TConfigVarKey::GetValueID(cAppCharP aName)
{
  size_t namsz;
  sInt32 fldID;
  checkFieldAttrs(aName,namsz,fldID);
  if (fldID!=0)
    return fldID; // is an attribute
  // value itself cannot be accessed by ID - so cache name for next call to getValueInternal
  fVarName = aName; // cache name
  return KEYVAL_NO_ID;
};


// get value's native type (all config vars are text)
uInt16 TConfigVarKey::GetValueType(sInt32 aID)
{
  uInt16 valType;
  if (checkAttrValueType(aID,valType))
    return valType;
  // value itself is always text
  return VALTYPE_TEXT;
} // TConfigVarKey::GetValueType


// get value
TSyError TConfigVarKey::GetValueInternal(
  sInt32 aID, sInt32 aArrayIndex,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
) {
  if (checkAttrValue(aID,aArrayIndex,aBuffer,aBufSize,aValSize))
    return LOCERR_OK;
  string s;
  if (!fEngineInterfaceP->getSyncAppBase()->getConfigVar(fVarName.c_str(),s))
    return DB_NotFound;
  // copy string into buffer
  aValSize=s.size();
  strncpy((appCharP)aBuffer,s.c_str(),aBufSize);
  return LOCERR_OK;
} // TConfigVarKey::GetValueInternal


// set value
TSyError TConfigVarKey::SetValueInternal(
  sInt32 aID, sInt32 aArrayIndex,
  cAppPointer aBuffer, memSize aValSize
) {
  if (!aBuffer) {
    // writing NULL means undefining variable
    fEngineInterfaceP->getSyncAppBase()->unsetConfigVar(fVarName.c_str());
    return LOCERR_OK;
  }
  string v; v.assign((cAppCharP)aBuffer,(size_t)aValSize); // copy because input could be unterminated string
  if (!fEngineInterfaceP->getSyncAppBase()->setConfigVar(fVarName.c_str(),v.c_str()))
    return DB_NotFound;
  return LOCERR_OK;
} // TConfigVarKey::SetValueInternal




// TStructFieldsKey
// ================


// static helper for procedural string readers
TSyError TStructFieldsKey::returnString(cAppCharP aReturnString, appPointer aBuffer, memSize aBufSize, memSize &aValSize)
{
  aValSize=strlen(aReturnString);
  if (aBufSize>=aValSize) {
    // copy string
    strncpy((appCharP)aBuffer,aReturnString,aValSize);
  }
  return LOCERR_OK;
} // returnString


// static helper for procedural int readers
TSyError TStructFieldsKey::returnInt(sInt32 aInt, memSize aIntSize, appPointer aBuffer, memSize aBufSize, memSize &aValSize)
{
  aValSize=aIntSize;
  if (aBufSize==0) return LOCERR_OK; // measuring size
  if (aBufSize<aIntSize) return LOCERR_BUFTOOSMALL;
  if (aValSize>=4)
    *((sInt32 *)aBuffer) = (sInt32)aInt;
  else if (aValSize>=2)
    *((sInt16 *)aBuffer) = (sInt16)aInt;
  else
    *((sInt8 *)aBuffer) = (sInt8)aInt;
  return LOCERR_OK;
} // returnInt


TSyError TStructFieldsKey::returnLineartime(lineartime_t aTime, appPointer aBuffer, memSize aBufSize, memSize &aValSize)
{
  aValSize=sizeof(lineartime_t);
  if (aBufSize==0) return LOCERR_OK; // measuring size
  if (aBufSize<aValSize) return LOCERR_BUFTOOSMALL;
  *((lineartime_t *)aBuffer) = aTime;
  return LOCERR_OK;
}




// get value's ID (e.g. internal index)
sInt32 TStructFieldsKey::GetValueID(cAppCharP aName)
{
  const TStructFieldInfo *tblP = getFieldsTable();

  sInt32 idx=0;
  size_t namsz;
  sInt32 fldID;
  if (checkFieldAttrs(aName,namsz,fldID))
    return fldID;
  while(tblP && idx<numFields()) {
    if (strucmp(aName,tblP[idx].valName,namsz)==0) {
      return fldID+idx; // found
    }
    // next
    idx++;
  }
  // now check for iterator commands
  if (strucmp(aName,VALNAME_FIRST)==0) {
    fIterator=0;
    if (fIterator<numFields()) return fIterator;
  }
  else if (strucmp(aName,VALNAME_NEXT)==0) {
    if (fIterator<numFields()) fIterator++;
    if (fIterator<numFields()) return fIterator;
  }
  // not found
  return KEYVAL_ID_UNKNOWN; // unknown
} // TStructFieldsKey::GetValueID


// get value's native type
uInt16 TStructFieldsKey::GetValueType(sInt32 aID)
{
  uInt16 valType;
  if (checkAttrValueType(aID,valType))
    return valType;
  if (aID>=numFields()) return VALTYPE_UNKNOWN;
  const TStructFieldInfo *fldinfoP = &(getFieldsTable()[aID]);
  valType = fldinfoP->valType;
  if (valType == VALTYPE_ENUM) {
    // VALTYPE_ENUM: determine integer type from actual size of variable
    switch (fldinfoP->valSiz) {
      default : return VALTYPE_INT8; // smallest causes least harm: use as default
      case 2  : return VALTYPE_INT16;
      case 4  : return VALTYPE_INT32;
      case 8  : return VALTYPE_INT64;
    }
  }
  else if (valType == VALTYPE_TEXT_OBFUS)
    return VALTYPE_TEXT; // obfuscated fields are text for SDK user
  else
    return valType; // just return specified type
} // TStructFieldsKey::GetValueType


// get value
TSyError TStructFieldsKey::GetValueInternal(
  sInt32 aID, sInt32 aArrayIndex,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  if (checkAttrValue(aID,aArrayIndex,aBuffer,aBufSize,aValSize))
    return LOCERR_OK;
  const TStructFieldInfo *fldinfoP = &(getFieldsTable()[aID]);
  // check for programmatic access to value
  if (fldinfoP->getValueProc) {
    // get value programmatically
    return fldinfoP->getValueProc(this,fldinfoP,aBuffer,aBufSize,aValSize);
  }
  // fetch value from structure
  memSize siz = fldinfoP->valSiz; // get (max) size
  if (siz==0) return DB_Forbidden; // read access not allowed
  uInt8P valPtr = getStructAddr()+fldinfoP->fieldOffs;
  // for text fields, measure actual size
  if (fldinfoP->valType==VALTYPE_TEXT)
    siz = strlen((cAppCharP)valPtr);
  // return actual size
  aValSize = siz;
  // return requested amount of data or all we have, whatever is smaller
  if (siz>aBufSize)
    siz=aBufSize; // limit to buffer size (aBufSiz is compensated for NUL terminator already, so we can use memcpy for strings as well)
  // for obfuscated fields, use special routine
  if (fldinfoP->valType==VALTYPE_TEXT_OBFUS && siz>0) {
    getUnmangledAsBuf((appCharP)aBuffer, siz, (cAppCharP)valPtr);
    // measure decoded
    aValSize = strlen((cAppCharP)aBuffer);
  }
  else
    memcpy(aBuffer,valPtr,siz);
  return LOCERR_OK;
} // TStructFieldsKey::GetValueInternal


// set value
TSyError TStructFieldsKey::SetValueInternal(
  sInt32 aID, sInt32 aArrayIndex,
  cAppPointer aBuffer, memSize aValSize
)
{
  if (!aBuffer) return LOCERR_WRONGUSAGE; // cannot handle NULL values
  if (aID & VALID_IDXOFFS_VALTYPE)
    return DB_Forbidden; // can't write type
  TSyError sta = LOCERR_OK;
  const TStructFieldInfo *fldinfoP = &(getFieldsTable()[aID]);
  // refuse writing to read-only fields
  if (!fldinfoP->writable) return DB_Forbidden;
  // check for programmatic access to value
  if (fldinfoP->setValueProc) {
    // set value programmatically
    return fldinfoP->setValueProc(this,fldinfoP,aBuffer,aValSize);
  }
  // write value to structure
  memSize siz = fldinfoP->valSiz; // get (max) size (for strings: buffer including NUL)
  if (siz==0) return DB_Forbidden; // write access not allowed
  uInt8P valPtr = getStructAddr()+fldinfoP->fieldOffs;
  // adjust usable size for strings
  if (fldinfoP->valType == VALTYPE_TEXT)
    siz--; // reserve one for terminator
  // check and signal truncation
  if (aValSize>siz)
    sta=LOCERR_TRUNCATED;
  else
    siz=aValSize;
  // copy data into struct
  if (fldinfoP->valType==VALTYPE_TEXT_OBFUS) {
    string v; v.assign((cAppCharP)aBuffer,aValSize);
    assignMangledToCString((appCharP)valPtr, v.c_str(), fldinfoP->valSiz, true); // always use entire buffer and fill it with garbage beyond end of actual data
  }
  else
    memcpy(valPtr,aBuffer,siz);
  // - struct modified, signal that (for derivates that might want to save the record on close)
  fDirty=true;
  // append terminator in case of text
  if (fldinfoP->valType == VALTYPE_TEXT)
    *(valPtr+siz)=0;
  // done
  return sta;
} // TStructFieldsKey::SetValueInternal



#ifdef SYSER_REGISTRATION

// Licensing key
// -------------

// Accessors for license text and code

// - read license text
static TSyError readLicenseText(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  // copy from config
  aValSize=aStructFieldsKeyP->getEngineInterface()->getSyncAppBase()->getRootConfig()->fLicenseName.size();
  if (aBufSize>0)
    strncpy((char *)aBuffer,aStructFieldsKeyP->getEngineInterface()->getSyncAppBase()->getRootConfig()->fLicenseName.c_str(),aBufSize);
  return LOCERR_OK;
} // readLicenseText


// - write license text
static TSyError writeLicenseText(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  // assign to config
  aStructFieldsKeyP->getEngineInterface()->getSyncAppBase()->getRootConfig()->fLicenseName.assign((cAppCharP)aBuffer,aValSize);
  /* DO NOT recalculate, only when setting code
  // recalculate app enabled status
  fEngineInterfaceP->getSyncAppBase()->isRegistered();
  */
  return LOCERR_OK;
} // writeLicenseText


// - write license code
static TSyError writeLicenseCode(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  // assign to config
  aStructFieldsKeyP->getEngineInterface()->getSyncAppBase()->getRootConfig()->fLicenseCode.assign((cAppCharP)aBuffer,aValSize);
  // recalculate all license dependent variables
  aStructFieldsKeyP->getEngineInterface()->getSyncAppBase()->isRegistered();
  // always ok
  return LOCERR_OK;
} // writeLicenseCode


// - read (and recalculate) registration status code
//   (i.e. status of currently installed license. Note that even if this returns non-ok, the app
//   might still be able to sync, e.g. due to a free demo period)
static TSyError readRegStatus(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  aValSize=2;
  if (aBufSize>=aValSize) {
    // recalculate all license dependent variables
    localstatus sta = aStructFieldsKeyP->getEngineInterface()->getSyncAppBase()->isRegistered();
    // copy from config
    *((uInt16*)aBuffer)=sta;
  }
  return LOCERR_OK;
} // readRegStatus


// - read (and recalculate) application enable status code
//   (i.e. if app can sync now. Note that this does not necessarily mean that a valid license is installed,
//   read "regStatus" for that).
static TSyError readEnabledStatus(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  aValSize=2;
  if (aBufSize>=aValSize) {
    // recalculate all license dependent variables and current enabled status
    // Note: includes checking for daysleft
    localstatus sta = aStructFieldsKeyP->getEngineInterface()->getSyncAppBase()->appEnableStatus();
    // copy from config
    *((uInt16*)aBuffer)=sta;
  }
  return LOCERR_OK;
} // readEnabledStatus


#if defined(EXPIRES_AFTER_DAYS) && defined(ENGINEINTERFACE_SUPPORT)

// - write first use version number
static TSyError writeFuv(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  uInt32 fuv = *((uInt32 *)aBuffer);
  if (fuv==0x53595359) {
    // start eval
    aStructFieldsKeyP->getEngineInterface()->getSyncAppBase()->updateFirstUseInfo(
      aStructFieldsKeyP->getEngineInterface()->getSyncAppBase()->fFirstUseDate,
      aStructFieldsKeyP->getEngineInterface()->getSyncAppBase()->fFirstUseVers
    );
  }
  else {
    // simply assign (usually values read from here in an earlier session and stored persistently outside)
    aStructFieldsKeyP->getEngineInterface()->getSyncAppBase()->fFirstUseVers = fuv;
  }
  // always ok
  return LOCERR_OK;
} // writeFuv

#endif // EXPIRES_AFTER_DAYS+ENGINEINTERFACE_SUPPORT



// macro simplifying typing in the table below
#define OFFS_SZ_AB(n) (offsetof(TSyncAppBase,n)), sizeof(dP_ab->n)
// dummy pointer needed for sizeof
static const TSyncAppBase *dP_ab=NULL;

// accessor table for licensing info
static const TStructFieldInfo LicensingFieldInfos[] =
{
  // valName, valType, writable, fieldOffs, valSiz
  // - license text and code (writable)
  { "licensetext", VALTYPE_TEXT, true, 0, 0, &readLicenseText, &writeLicenseText },
  { "licensecode", VALTYPE_TEXT, true, 0, 0, NULL, &writeLicenseCode },
  #if defined(EXPIRES_AFTER_DAYS) && defined(ENGINEINTERFACE_SUPPORT)
  // - read/write for eval
  { "fud", VALTYPE_INT32, true, OFFS_SZ_AB(fFirstUseDate) }, // first use date
  { "fuv", VALTYPE_INT32, true, OFFS_SZ_AB(fFirstUseVers), NULL, &writeFuv  }, // first use version
  #endif
  // - read-only info about licensing status from syncappbase
  { "regStatus", VALTYPE_INT16, false, 0, 0, &readRegStatus, NULL },
  { "enabledStatus", VALTYPE_INT16, false, 0, 0, &readEnabledStatus, NULL },
  { "regOK", VALTYPE_ENUM, false, OFFS_SZ_AB(fRegOK) },
  { "productCode", VALTYPE_ENUM, false, OFFS_SZ_AB(fRegProductCode) },
  { "productFlags", VALTYPE_ENUM, false, OFFS_SZ_AB(fRegProductFlags) },
  { "quantity", VALTYPE_ENUM, false, OFFS_SZ_AB(fRegQuantity) },
  { "licenseType", VALTYPE_ENUM, false, OFFS_SZ_AB(fRegLicenseType) },
  { "daysleft", VALTYPE_ENUM, false, OFFS_SZ_AB(fDaysLeft) },
};


// get table describing the fields in the struct
const TStructFieldInfo *TLicensingKey::getFieldsTable(void)
{
  return LicensingFieldInfos;
} // TLicensingKey::getFieldsTable

sInt32 TLicensingKey::numFields(void)
{
  return sizeof(LicensingFieldInfos)/sizeof(TStructFieldInfo);
} // TLicensingKey::numFields



// get actual struct base address
uInt8P TLicensingKey::getStructAddr(void)
{
  return (uInt8P)fEngineInterfaceP->getSyncAppBase(); // we are accessing SyncAppBase fields
} // TLicensingKey::getStructAddr


#endif // SYSER_REGISTRATION


// TEngineInterface
// ================

// constructor
TEngineInterface::TEngineInterface() :
  fAppBaseP(NULL)
{
  // init string modes to default
  fCharSet = chs_utf8;
  fLineEndMode = lem_cstr;
  fBigEndian = false; // default to Intel order
} // TEngineInterface


// constructor
TSyError TEngineInterface::Init()
{
  #ifdef DIRECT_APPBASE_GLOBALACCESS
  // global anchor only exists in old-style targets; pure engines do not have it any more
  sysync_glob_setanchor(this);
  #endif
  // create the appropriate SyncAppBase object
  fAppBaseP = newSyncAppBase();
  if (fAppBaseP==NULL)
    return LOCERR_UNDEFINED;
  // set myself as the Master
  fAppBaseP->fEngineInterfaceP = this;
  // connect the debug routines to the appbase logger
  #ifdef SYDEBUG
  if (fCI) {
    fCI->callbackRef       = fAppBaseP;
    fCI->DB_DebugPuts      = AppBaseLogDebugPuts;
    fCI->DB_DebugBlock     = AppBaseLogDebugBlock;
    fCI->DB_DebugEndBlock  = AppBaseLogDebugEndBlock;
    fCI->DB_DebugEndThread = AppBaseLogDebugEndThread;
    fCI->DB_DebugExotic    = AppBaseLogDebugExotic;
  }
  #endif
  // we have an appbase now
  return LOCERR_OK;
} // TEngineInterface::Init


TSyError TEngineInterface::Term()
{ // empty for the moment
  return LOCERR_OK;
} // TEnigneInterface::Term


// destructor
TEngineInterface::~TEngineInterface()
{
  // delete the SyncAppBase
  if (fAppBaseP) {
    delete fAppBaseP;
    fAppBaseP=NULL;
  }
  #ifdef DIRECT_APPBASE_GLOBALACCESS
  // global anchor only exists in old-style targets; pure engines do not have it any more
  sysync_glob_setanchor(NULL);
  #endif
} // ~TEngineInterface


// Set text format parameters
TSyError TEngineInterface::SetStringMode(uInt16 aCharSet, uInt16 aLineEndMode, bool aBigEndian)
{
  // translate charset
  uInt16 chs;
  for (chs=0; chs<numCharSets; chs++) {
    if (engineCharSets[chs]==aCharSet) break;
  }
  if (chs==numCharSets) return LOCERR_BADPARAM;
  // translate lineendmode
  uInt16 lem;
  for (lem=0; lem<numLineEndModes; lem++) {
    if (engineLineEndModes[lem]==aLineEndMode) break;
  }
  if (lem==numLineEndModes) return LOCERR_BADPARAM;
  // assign them now they are checked ok
  fCharSet = (TCharSets)chs;
  fLineEndMode = (TLineEndModes)lem;
  // big endian flag
  fBigEndian = aBigEndian;
  return LOCERR_OK;
} // TEngineInterface::SetStringMode



// conversion helper
// - converts byte stream (which can be an 8-bit char set or unicode in intel or motorola order)
//   to internal UTF-8, according to text mode set with SetStringMode()
cAppCharP TEngineInterface::makeAppString(cAppCharP aTextP, string &aString)
{
  // return as-is if input is UTF-8 already
  if (fCharSet==chs_utf8)
    return cAppCharP(aTextP);
  // convert into passed string
  if (fCharSet==chs_utf16) {
    appendUTF16AsUTF8((uInt16 *)aTextP,0x7FFFFFFF,fBigEndian,aString, true, true);
  }
  else {
    appendStringAsUTF8((cAppCharP)aTextP, aString, fCharSet, fLineEndMode, true);
  }
  // return pointer to just converted string
  return aString.c_str();
} // TEngineInterface::makeAppString



/// @brief init object, optionally passing XML config text in memory
/// @param aConfigXML[in] NULL if no external config needed, config text otherwise
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::InitEngineXML(cAppCharP aConfigXML)
{
  #ifndef CONSTANTXML_CONFIG
  return LOCERR_NOTIMP;
  #else
  // get the global context for pooling ressources like Java VM
  fAppBaseP->fApiInterModuleContext = fCI->gContext;
  // make sure we have string as UTF-8
  string tempStr;
  aConfigXML = makeAppString(aConfigXML, tempStr);
  // feed config string directly into engine
  return getSyncAppBase()->readXMLConfigConstant(aConfigXML);
  #endif
} // TEngineInterface::InitEngineXML


/// @brief init object, optionally passing a open FILE for reading config
/// @param aCfgFile[in] open file containing XML config
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::InitEngineFile(cAppCharP aConfigFilePath)
{
  // make sure we have string as UTF-8
  string tempStr;
  aConfigFilePath = makeAppString(aConfigFilePath, tempStr);
  // get the global context for pooling ressources like Java VM
  fAppBaseP->fApiInterModuleContext = fCI->gContext;
  // read config from a file
  string pathStr = aConfigFilePath;
  getSyncAppBase()->expandConfigVars(pathStr,2);
  // now parse config
  return getSyncAppBase()->readXMLConfigFile(pathStr.c_str());
} // TEngineInterface::InitEngineFile


/// @brief init object, optionally passing a callback for reading config
/// @param aReaderFunc[in] callback function which can deliver next chunk of XML config data
/// @param aContext[in] free context pointer passed back with callback
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::InitEngineCB(TXMLConfigReadFunc aReaderFunc, void *aContext)
{
  // get the global context for pooling ressources like Java VM
  fAppBaseP->fApiInterModuleContext = fCI->gContext;
  // now parse config
  return getSyncAppBase()->readXMLConfigStream(aReaderFunc, aContext);
} // TEngineInterface::InitEngineCB


/// @brief Open a session
/// @param aNewSessionH[out] receives session handle for all session execution calls
/// @param aSelector[in] selector, depending on session type. For multi-profile clients: profile ID to use
/// @param aSessionName[in] a text name/id to identify a session, useage depending on session type.
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::OpenSession(SessionH &aNewSessionH, uInt32 aSelector, cAppCharP aSessionName)
{
  // safety check - app must be enabled (initialized with config and ok license before we can open a session)
  localstatus sta=getSyncAppBase()->appEnableStatus();
  if (sta!=LOCERR_OK) return sta; // app not enabled (and maybe not initialized properly) - prevent any session opening
  // make sure we have string as UTF-8
  string tempStr;
  aSessionName = makeAppString(aSessionName, tempStr);

  return OpenSessionInternal(aNewSessionH, aSelector, aSessionName);
} // TEngineInterface::OpenSession

/// @brief internal implementation, derived in actual clients or servers
TSyError TEngineInterface::OpenSessionInternal(SessionH &aNewSessionH, uInt32 aSelector, cAppCharP aSessionName)
{
  // must be implemented in derived class
  return LOCERR_NOTIMP;
} // TEngineInterface::OpenSessionInternal


/// @brief open session specific runtime parameter/settings key
/// @note key handle obtained with this call must be closed BEFORE SESSION IS CLOSED!
/// @param aNewKeyH[out] receives the opened key's handle on success
/// @param aSessionH[in] session handle obtained with OpenSession
/// @param aMode[in] the open mode
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::OpenSessionKey(SessionH aSessionH, KeyH &aNewKeyH, uInt16 aMode)
{
  // no session key available in base class
  return DB_NotFound;
} // TEngineInterface::OpenSessionKey



/// @brief Close a session
/// @note  It depends on session type if this also destroys the session or if it may persist and can be re-opened.
/// @param aSessionH[in] session handle obtained with OpenSession
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::CloseSession(SessionH aSessionH)
{
  // must be implemented in derived class
  return LOCERR_NOTIMP;
} // TEngineInterface::CloseSession


/// @brief Executes next step of the session
/// @param aSessionH[in] session handle obtained with OpenSession
/// @param aStepCmd[in/out] step command (STEPCMD_xxx):
///        - tells caller to send or receive data or end the session etc.
///        - instructs engine to suspend or abort the session etc.
/// @param aInfoP[in] pointer to a TEngineProgressInfo structure, NULL if no progress info needed
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::SessionStep (SessionH aSessionH, uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  // must be implemented in derived class
  return LOCERR_NOTIMP;
} // TEngineInterface::SessionStep


/// @brief Get access to SyncML message buffer
/// @param aSessionH[in] session handle obtained with OpenSession
/// @param aForSend[in] send mode (true) / receive mode (false)
/// @param aBuffer[out] receives pointer to buffer (empty for receive, full for send)
/// @param aBufSize[out] receives size of empty or full buffer
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::GetSyncMLBuffer(SessionH aSessionH, bool aForSend, appPointer &aBuffer, memSize &aBufSize)
{
  Ret_t rc;
  MemSize_t bufSz; // Note: this is SML_TK definition of memsize!
  // get SML instance for this session (note that "session" could be a SAN checker as well)
  InstanceID_t myInstance = getSmlInstanceOfSession(aSessionH);
  if (myInstance==0)
    return LOCERR_WRONGUSAGE; // no instance, usage must be wrong
  // provide access to the buffer
  if (aForSend) {
    // we want to read the SyncML buffer
    rc=smlLockReadBuffer(
      myInstance,
      (unsigned char **)&aBuffer, // receives address of buffer containing SyncML to send
      &bufSz // receives size of SyncML to send
    );
    aBufSize = bufSz;
  }
  else {
    // we want to write the SyncML buffer
    rc=smlLockWriteBuffer(
      myInstance,
      (unsigned char **)&aBuffer, // receives address of buffer where received SyncML can be put
      &bufSz // capacity of the buffer
    );
    aBufSize = bufSz;
  }
  // check error
  if (rc!=SML_ERR_OK)
    return LOCERR_SMLFATAL; // problem with SML toolkit
  // done
  return LOCERR_OK;
} // TEngineInterface::GetSyncMLBuffer



/// @brief Return SyncML message buffer to engine
/// @param aSessionH[in] session handle obtained with OpenSession
/// @param aForSend[in] send mode (true) / receive mode (false)
/// @param aProcessed[in] number of bytes put into or read from the buffer
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::RetSyncMLBuffer(SessionH aSessionH, bool aForSend, memSize aProcessed)
{
  Ret_t rc;

  // get SML instance for this session (note that "session" could be a SAN checker as well)
  InstanceID_t myInstance = getSmlInstanceOfSession(aSessionH);
  if (myInstance==0)
    return LOCERR_WRONGUSAGE; // no instance, usage must be wrong
  // return buffer
  if (aForSend) {
    // we have read the SyncML from the buffer (and usually sent to the remote)
    rc=smlUnlockReadBuffer(
      myInstance,
      aProcessed  // number of bytes read from the buffer (and sent to the remote, hopefully...)
    );
  }
  else {
    // we have written some data (usually received from the remote) into the SyncML buffer
    rc=smlUnlockWriteBuffer(
      myInstance,
      aProcessed  // number of SyncML bytes put into the buffer (coming from the remote, usually...)
    );
  }
  // check error
  if (rc!=SML_ERR_OK)
    return LOCERR_SMLFATAL; // problem with SML toolkit
  // done
  return LOCERR_OK;
} // TEngineInterface::RetSyncMLBuffer


/// @brief Read data from SyncML message buffer
/// @param aSessionH[in] session handle obtained with OpenSession
/// @param aBuffer[in] pointer to buffer
/// @param aBufSize[in] size of buffer, maximum to be read
/// @param aMsgSize[out] size of data available in the buffer for read INCLUDING just returned data.
/// @note  If the aBufSize is too small to return all available data LOCERR_TRUNCATED will be returned, and the
///        caller can repeat calls to ReadSyncMLBuffer to get the next chunk.
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::ReadSyncMLBuffer(SessionH aSessionH, appPointer aBuffer, memSize aBufSize, memSize &aMsgSize)
{
  TSyError sta;
  appPointer bufP;
  memSize msgsiz;

  // get buffer for read
  sta = GetSyncMLBuffer(aSessionH,true,bufP,msgsiz);
  if (sta!=LOCERR_OK) return sta;
  // anyway: pass size of data in the buffer
  aMsgSize = msgsiz;
  // check if buffer passed is large enough
  if (aBufSize<msgsiz) {
    sta = LOCERR_TRUNCATED;
    // adjust number of bytes to copy
    msgsiz=aBufSize;
  }
  if (msgsiz>0) {
    // copy data
    memcpy(aBuffer,bufP,msgsiz);
  }
  // return SyncML buffer to engine
  RetSyncMLBuffer(aSessionH,true,msgsiz);
  // return status
  return sta;
} // TEngineInterface::ReadSyncMLBuffer


/// @brief Write data to SyncML message buffer
/// @param aSessionH[in] session handle obtained with OpenSession
/// @param aBuffer[in] pointer to buffer
/// @param aMsgSize[in] size of message to write to the buffer
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::WriteSyncMLBuffer(SessionH aSessionH, appPointer aBuffer, memSize aMsgSize)
{
  TSyError sta;
  appPointer bufP;
  memSize bufsiz;

  // get buffer for write
  sta = GetSyncMLBuffer(aSessionH,false,bufP,bufsiz);
  if (sta!=LOCERR_OK) return sta;
  // check if buffer large enough to write data
  if (aMsgSize>bufsiz) {
    // can't write data - message too large
    sta = LOCERR_BUFTOOSMALL;
    aMsgSize=0; // don't write anything
  }
  else {
    if (aMsgSize>0) {
      // copy data
      memcpy(bufP,aBuffer,aMsgSize);
    }
  }
  // return SyncML buffer to engine
  RetSyncMLBuffer(aSessionH,false,aMsgSize);
  // return status
  return sta;
} // TEngineInterface::WriteSyncMLBuffer





/// @brief open Settings key by path specification
/// @param aNewKeyH[out] receives the opened key's handle on success
/// @param aParentKeyH[in] NULL if path is absolute from root, handle to an open key for relative access
/// @param aPath[in] the path specification as null terminated string
/// @param aMode[in] the open mode
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::OpenKeyByPath (
  KeyH &aNewKeyH,
  KeyH aParentKeyH,
  cAppCharP aPath, uInt16 aMode
) {
  TSettingsKeyImpl *aBaseKeyP = NULL;
  TSettingsKeyImpl *aKeyP = NULL;
  bool implicit = false;
  TSyError sta;

  // make sure we have string as UTF-8
  string tempStr;
  aPath = makeAppString(aPath, tempStr);

  // determine base key
  if (aParentKeyH) {
    // already opened base key passed
    aBaseKeyP = reinterpret_cast<TSettingsKeyImpl *>(aParentKeyH);
    implicit = false; // we have the parent already, do not delete it implicitly
  }
  else {
    // we need to implicitly create the root key
    aBaseKeyP = newSettingsRootKey();
    implicit = true; // the root key is implicitly created and must be deleted implicitly as well
  }
  // open key
  if (implicit && (aPath==NULL || *aPath==0 || (*aPath==SETTINGSKEY_PATH_SEPARATOR && *(aPath+1)==0))) {
    // opening root requested
    aKeyP = aBaseKeyP;
    sta = LOCERR_OK;
  }
  else {
    // open subkey of current key
    sta = aBaseKeyP->OpenKeyByPath(aKeyP,aPath,aMode,implicit);
  }
  if (sta!=LOCERR_OK) {
    if (aKeyP) {
      // make sure all implicitly opened elements of the chain are deleted
      // (includes the TSettingsRootKey, if one was created above)
      delete aKeyP;
    }
  }
  else {
    // pass back pointer to opened key
    aNewKeyH = (KeyH)aKeyP;
  }
  return sta;
} // TEngineInterface::OpenKeyByPath



/// @brief open Settings subkey key by ID or iterating over all subkeys
/// @param aNewKeyH[out] receives the opened key's handle on success
/// @param aParentKeyH[in] handle to the parent key
/// @param aID[in] the ID of the subkey to open,
///                or KEYVAL_ID_FIRST/KEYVAL_ID_NEXT to iterate over existing subkeys
///                or KEYVAL_ID_NEW to create a new subkey
/// @param aMode[in] the open mode
/// @return LOCERR_OK on success, DB_NoContent when no more subkeys are found with
///         KEYVAL_ID_FIRST/KEYVAL_ID_NEXT
///         or any other SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::OpenSubkey(
  KeyH &aNewKeyH,
  KeyH aParentKeyH,
  sInt32 aID, uInt16 aMode
) {
  TSettingsKeyImpl *baseKeyP = NULL;
  TSettingsKeyImpl *keyP = NULL;
  TSyError sta;

  // We need a parent key
  if (!aParentKeyH) return LOCERR_WRONGUSAGE;
  // Get key
  baseKeyP = reinterpret_cast<TSettingsKeyImpl *>(aParentKeyH);
  // now open subkey
  sta = baseKeyP->OpenSubkey(keyP,aID,aMode);
  if (sta == LOCERR_OK) {
    // return key handle
    aNewKeyH = (KeyH)keyP;
  }
  return sta;
} // TEngineInterface::OpenSubkey


/// @brief delete Settings subkey key by ID
/// @param aID[in] the ID of the subkey to delete
/// @param aParentKeyH[in] handle to the parent key
/// @return LOCERR_OK on success
///         or any other SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::DeleteSubkey(KeyH aParentKeyH, sInt32 aID)
{
  // We need a parent key
  if (!aParentKeyH) return LOCERR_WRONGUSAGE;
  // Get key
  return reinterpret_cast<TSettingsKeyImpl *>(aParentKeyH)->DeleteSubkey(aID);
} // TEngineInterface::DeleteSubkey


/// @brief Get key ID of currently open key. Note that the Key ID is only locally unique within
///        the parent key.
/// @param aKeyH[in] an open key handle
/// @param aID[out] receives the ID of the open key, which can be used to re-access the
///        key within its parent using OpenSubkey()
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::GetKeyID(KeyH aKeyH, sInt32 &aID)
{
  if (!aKeyH) return LOCERR_WRONGUSAGE;
  return reinterpret_cast<TSettingsKeyImpl *>(aKeyH)->GetKeyID(aID);
} // TEngineInterface::GetKeyID




/// @brief Set text format parameters
/// @param aKeyH[in] an open key handle
/// @param aCharSet[in] charset (default is UTF-8 when SetTextMode() is not used)
/// @param aLineEndMode[in] line end mode (default is C-lineends of the platform (almost always LF) when SetTextMode() is not used)
/// @param aBigEndian[in] determines endianness of UTF16 text (defaults to little endian = intel order)
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::SetTextMode(KeyH aKeyH, uInt16 aCharSet, uInt16 aLineEndMode, bool aBigEndian)
{
  if (!aKeyH) return LOCERR_WRONGUSAGE;
  return reinterpret_cast<TSettingsKeyImpl *>(aKeyH)->SetTextMode(aCharSet,aLineEndMode,aBigEndian);
} // TEngineInterface::SetTextMode



/// @brief Set time format parameters
/// @param aKeyH[in] an open key handle
/// @param aTimeMode[in] time mode, see TMODE_xxx (default is platform's lineratime_t when SetTimeMode() is not used)
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::SetTimeMode(KeyH aKeyH, uInt16 aTimeMode)
{
  if (!aKeyH) return LOCERR_WRONGUSAGE;
  return reinterpret_cast<TSettingsKeyImpl *>(aKeyH)->SetTimeMode(aTimeMode);
} // TEngineInterface::SetTimeMode



/// @brief Closes a key opened by OpenKeyByPath() or OpenSubKey()
/// @param aKeyH[in] an open key handle. Will be invalid when call returns with LOCERR_OK. Do not re-use!
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::CloseKey(KeyH aKeyH)
{
  if (aKeyH) {
    // deleting key will close it (including all implicitly opened parents)
    delete reinterpret_cast<TSettingsKeyImpl *>(aKeyH);
  }
  return LOCERR_OK;
} // TEngineInterface::CloseKey


/// @brief get value's ID for use with Get/SetValueByID()
/// @return KEYVAL_ID_UNKNOWN when name is unknown,
///   KEYVAL_NO_ID when name is known, but no ID exists for it (ID access not possible)
///   ID of value otherwise
sInt32 TEngineInterface::GetValueID(KeyH aKeyH, cAppCharP aName)
{
  if (!aKeyH) return KEYVAL_ID_UNKNOWN;

  // make sure we have string as UTF-8
  string tempStr;
  aName = makeAppString(aName, tempStr);

  return reinterpret_cast<TSettingsKeyImpl *>(aKeyH)->GetValueID(aName);
} // TEngineInterface::GetValueID


/// @brief Reads a named value in specified format into passed memory buffer
/// @param aKeyH[in] an open key handle
/// @param aValueName[in] name of the value to read
/// @param aValType[in] desired return type, see VALTYPE_xxxx
/// @param aBuffer[in/out] buffer where to store the data
/// @param aBufSize[in] size of buffer in bytes (ALWAYS in bytes, even if value is Unicode string)
/// @param aValSize[out] actual size of value.
///        For VALTYPE_TEXT, size is string length (IN BYTES) excluding NULL terminator
///        Note that this will be set also when return value is LOCERR_BUFTOOSMALL, to indicate the required buffer size
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::GetValue(
  KeyH aKeyH, cAppCharP aValueName,
  uInt16 aValType,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  if (!aKeyH) return LOCERR_WRONGUSAGE;

  // GetValueID will convert aValueName to app format if needed
  sInt32 valId = GetValueID(aKeyH,aValueName);
  if (valId==KEYVAL_ID_UNKNOWN) return DB_NotFound;
  // directly call settingsKeyImpl, for cases where value has no ID
  return reinterpret_cast<TSettingsKeyImpl *>(aKeyH)->GetValueByID(
    valId,0,aValType,aBuffer,aBufSize,aValSize
  );
} // GetValue


/// @brief Reads value by ID in specified format into passed memory buffer
/// @param aKeyH[in] an open key handle
/// @param aID[in] ID of the value to read
/// @param aArrayIndex[in] 0-based array element index for array values.
/// @param aValType[in] desired return type, see VALTYPE_xxxx
/// @param aBuffer[in/out] buffer where to store the data
/// @param aBufSize[in] size of buffer in bytes (ALWAYS in bytes, even if value is Unicode string)
/// @param aValSize[out] actual size of value.
///        For VALTYPE_TEXT, size is string length (IN BYTES) excluding NULL terminator
///        Note that this will be set also when return value is LOCERR_BUFTOOSMALL, to indicate the required buffer size
/// @return LOCERR_OK on success, LOCERR_OUTOFRANGE when array index is out of range
///        SyncML or LOCERR_xxx error code on other failure
TSyError TEngineInterface::GetValueByID(
  KeyH aKeyH, sInt32 aID, sInt32 aArrayIndex,
  uInt16 aValType,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  if (!aKeyH) return LOCERR_WRONGUSAGE;
  if (aID==KEYVAL_NO_ID) return LOCERR_BADPARAM;
  return reinterpret_cast<TSettingsKeyImpl *>(aKeyH)->GetValueByID(
    aID,aArrayIndex,aValType,aBuffer,aBufSize,aValSize
  );
} // TEngineInterface::GetValueByID



/// @brief Writes a named value in specified format passed in memory buffer
/// @param aKeyH[in] an open key handle
/// @param aValueName[in] name of the value to write
/// @param aValType[in] type of value passed in, see VALTYPE_xxxx
/// @param aBuffer[in] buffer containing the data
/// @param aValSize[in] size of value. For VALTYPE_TEXT, size can be passed as -1 if string is null terminated
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::SetValue(
  KeyH aKeyH, cAppCharP aValueName,
  uInt16 aValType,
  cAppPointer aBuffer, memSize aValSize
) {
  if (!aKeyH) return LOCERR_WRONGUSAGE;

  // GetValueID will convert aValueName to app format if needed
  sInt32 valId = GetValueID(aKeyH,aValueName);
  if (valId==KEYVAL_ID_UNKNOWN) return DB_NotFound;
  return reinterpret_cast<TSettingsKeyImpl *>(aKeyH)->SetValueByID(
    valId,0,aValType,aBuffer,aValSize
  );
} // TEngineInterface::SetValue


/// @brief Writes a named value in specified format passed in memory buffer
/// @param aKeyH[in] an open key handle
/// @param aID[in] ID of the value to read
/// @param aArrayIndex[in] 0-based array element index for array values.
/// @param aValType[in] type of value passed in, see VALTYPE_xxxx
/// @param aBuffer[in] buffer containing the data
/// @param aValSize[in] size of value. For VALTYPE_TEXT, size can be passed as -1 if string is null terminated
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TEngineInterface::SetValueByID(
  KeyH aKeyH, sInt32 aID, sInt32 aArrayIndex,
  uInt16 aValType,
  cAppPointer aBuffer, memSize aValSize
)
{
  if (!aKeyH) return LOCERR_WRONGUSAGE;
  if (aID==KEYVAL_NO_ID) return LOCERR_BADPARAM;
  return reinterpret_cast<TSettingsKeyImpl *>(aKeyH)->SetValueByID(
    aID,aArrayIndex,aValType,aBuffer,aValSize
  );
} // TEngineInterface::SetValueByID



#ifdef DBAPI_TUNNEL_SUPPORT

// DBApi-like access to datastores
// ===============================

TSyError TEngineInterface::StartDataRead(SessionH aSessionH, cAppCharP lastToken, cAppCharP resumeToken)
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return LOCERR_WRONGUSAGE; // need properly opened tunnel session/datastore
  return ds->TunnelStartDataRead(lastToken,resumeToken);
} // StartDataRead


TSyError TEngineInterface::ReadNextItem(SessionH aSessionH, ItemID  aID, appCharP *aItemData, sInt32 *aStatus, bool  aFirst)
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return LOCERR_WRONGUSAGE; // need properly opened tunnel session/datastore
  return ds->TunnelReadNextItem(aID,aItemData,aStatus,aFirst);
} // ReadNextItem


TSyError TEngineInterface::ReadItem(SessionH aSessionH, cItemID aID, appCharP *aItemData)
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return LOCERR_WRONGUSAGE; // need properly opened tunnel session/datastore
  return ds->TunnelReadItem(aID,aItemData);
} // ReadItem


TSyError TEngineInterface::EndDataRead(SessionH aSessionH)
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return LOCERR_WRONGUSAGE; // need properly opened tunnel session/datastore
  return ds->TunnelEndDataRead();
} // EndDataRead


TSyError TEngineInterface::StartDataWrite(SessionH aSessionH)
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return LOCERR_WRONGUSAGE; // need properly opened tunnel session/datastore
  return ds->TunnelStartDataWrite();
} // StartDataWrite


TSyError TEngineInterface::InsertItem(SessionH aSessionH, cAppCharP aItemData, ItemID aID)
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return LOCERR_WRONGUSAGE; // need properly opened tunnel session/datastore
  return ds->TunnelInsertItem(aItemData,aID);
} // InsertItem


TSyError TEngineInterface::UpdateItem(SessionH aSessionH, cAppCharP aItemData, cItemID aID, ItemID updID )
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return LOCERR_WRONGUSAGE; // need properly opened tunnel session/datastore
  return ds->TunnelUpdateItem(aItemData,aID,updID);
} // UpdateItem


TSyError TEngineInterface::MoveItem(SessionH aSessionH, cItemID aID, cAppCharP newParID)
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return LOCERR_WRONGUSAGE; // need properly opened tunnel session/datastore
  return ds->TunnelMoveItem(aID,newParID);
} // MoveItem


TSyError TEngineInterface::DeleteItem(SessionH aSessionH, cItemID aID)
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return LOCERR_WRONGUSAGE; // need properly opened tunnel session/datastore
  return ds->TunnelDeleteItem(aID);
} // DeleteItem


TSyError TEngineInterface::EndDataWrite(SessionH aSessionH, bool success, appCharP *newToken)
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return LOCERR_WRONGUSAGE; // need properly opened tunnel session/datastore
  return ds->TunnelEndDataWrite(success,newToken);
} // EndDataWrite


void TEngineInterface::DisposeObj(SessionH aSessionH, void* memory)
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return; // need properly opened tunnel session/datastore
  return ds->TunnelDisposeObj(memory);
} // DisposeObj


// ---- asKey ----
TSyError TEngineInterface::ReadNextItemAsKey(SessionH aSessionH, ItemID  aID, KeyH aItemKey, sInt32 *aStatus, bool aFirst)
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return LOCERR_WRONGUSAGE; // need properly opened tunnel session/datastore
  return ds->TunnelReadNextItemAsKey(aID,aItemKey,aStatus,aFirst);
} // ReadNextItemAsKey


TSyError TEngineInterface::ReadItemAsKey(SessionH aSessionH, cItemID aID, KeyH aItemKey)
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return LOCERR_WRONGUSAGE; // need properly opened tunnel session/datastore
  return ds->TunnelReadItemAsKey(aID,aItemKey);
} // ReadItemAsKey


TSyError TEngineInterface::InsertItemAsKey(SessionH aSessionH, KeyH aItemKey, ItemID aID)
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return LOCERR_WRONGUSAGE; // need properly opened tunnel session/datastore
  return ds->TunnelInsertItemAsKey(aItemKey,aID);
} // InsertItemAsKey


TSyError TEngineInterface::UpdateItemAsKey(SessionH aSessionH, KeyH aItemKey, cItemID aID, ItemID updID)
{
  TLocalEngineDS *ds = reinterpret_cast<TSyncSession *>(aSessionH)->getTunnelDS();
  if (!ds) return LOCERR_WRONGUSAGE; // need properly opened tunnel session/datastore
  return ds->TunnelUpdateItemAsKey(aItemKey,aID,updID);
} // UpdateItemAsKey

#endif // DBAPI_TUNNEL_SUPPORT

TSyError TEngineInterface::debugPuts(cAppCharP aFile, int aLine, cAppCharP aFunction,
                                     int aDbgLevel, cAppCharP aPrefix,
                                     cAppCharP aText)
{
  #if defined(SYDEBUG)
  getSyncAppBase()->getDbgLogger()->DebugPuts(TDBG_LOCATION_ARGS(aFunction, aFile, aLine /*, aPrefix */)
                                              aDbgLevel, aText);
  return 0;
  #else
  return LOCERR_NOTIMP;
  #endif
} // debugPuts

#ifdef ENGINE_LIBRARY
#ifndef SIMPLE_LINKING

// Callback "factory" function implementation

static TSyError internal_ConnectEngine(
  bool aIsServer,
  UI_Call_In *aCIP,
  uInt16 aCallbackVersion, // if==0, engine creates new aCI
  CVersion *aEngVersionP,
  CVersion aPrgVersion,
  uInt16 aDebugFlags
)
{
  // create new engine
  TEngineModuleBase *engine = NULL;
  TSyError err = LOCERR_OK;
  if (aIsServer) {
    #ifdef SYSYNC_SERVER
    engine = newServerEngine();
    #else
    err = LOCERR_WRONGUSAGE;
    #endif
  }
  else {
    #ifdef SYSYNC_CLIENT
    engine = newClientEngine();
    #else
    err = LOCERR_WRONGUSAGE;
    #endif
  }
  if (err==LOCERR_OK) {
    // connect the engine
    if (aCallbackVersion!=0) {
      // valid aCIP passed in
      // - flag static
      engine->fCIisStatic= true;
      // - prepare callback and pass to engine
      (*aCIP)->callbackVersion = aCallbackVersion; // fill in the outside callback version
      engine->fCI = *aCIP;                         // engine uses the structure provided by the uiapp
      // - connect the engine
      err = engine->Connect("", aPrgVersion, aDebugFlags);
    }
    else {
      // no aCIP passed, let engine create one
      // - connect engine
      err = engine->Connect("", aPrgVersion, aDebugFlags);
      // - get CI from engine
      *aCIP = engine->fCI;
    }
    // - get the version
    if (aEngVersionP) *aEngVersionP = Plugin_Version(0);
  }
  return   err;
} // internal_ConnectEngine


// Client engine main entry point
TSyError SYSYNC_EXTERNAL(ConnectEngine)(
  UI_Call_In *aCI,
  CVersion   *aEngVersion,
  CVersion    aPrgVersion,
  uInt16      aDebugFlags
)
{
  return internal_ConnectEngine(false, aCI, 0, aEngVersion, aPrgVersion, aDebugFlags);
} // ConnectEngine


// The same, but coming in with a valid <aCI> */
TSyError SYSYNC_EXTERNAL(ConnectEngineS)(
  UI_Call_In  aCI,
  uInt16      aCallbackVersion,
  CVersion   *aEngVersion,
  CVersion    aPrgVersion,
  uInt16      aDebugFlags
)
{
  return internal_ConnectEngine(false, &aCI, aCallbackVersion, aEngVersion, aPrgVersion, aDebugFlags);
} // ConnectEngineS


// Server engine main entry point
TSyError SYSYNC_EXTERNAL_SRV(ConnectEngine)(
  UI_Call_In *aCI,
  CVersion   *aEngVersion,
  CVersion    aPrgVersion,
  uInt16      aDebugFlags
)
{
  return internal_ConnectEngine(true, aCI, 0, aEngVersion, aPrgVersion, aDebugFlags);
} // ConnectEngine


// The same, but coming in with a valid <aCI> */
TSyError SYSYNC_EXTERNAL_SRV(ConnectEngineS)(
  UI_Call_In  aCI,
  uInt16      aCallbackVersion,
  CVersion   *aEngVersion,
  CVersion    aPrgVersion,
  uInt16      aDebugFlags
)
{
  return internal_ConnectEngine(true, &aCI, aCallbackVersion, aEngVersion, aPrgVersion, aDebugFlags);
} // ConnectEngineS




static TSyError internal_DisconnectEngine(UI_Call_In aCI)
{
  TSyError err= LOCERR_OK;

  if (aCI && aCI->thisBase) { // structure must still exist
    // first get pointer to engine
    TEngineModuleBase *engine = static_cast<TEngineModuleBase *>(aCI->thisBase);
    err= engine->Disconnect();
    aCI->thisBase= NULL; // no longer valid
    // delete the engine (this also deletes the callback!)
    SYSYNC_TRY {
      delete engine;
    }
    SYSYNC_CATCH (...)
      err = LOCERR_EXCEPTION;
    SYSYNC_ENDCATCH
    // done
  } // if
  return err;
} // internal_DisconnectEngine


/* Entry point to disconnect client engine */
TSyError SYSYNC_EXTERNAL(DisconnectEngine)(UI_Call_In aCI)
{
  return internal_DisconnectEngine(aCI);
}


/* Entry point to disconnect server engine */
TSyError SYSYNC_EXTERNAL_SRV(DisconnectEngine)(UI_Call_In aCI)
{
  return internal_DisconnectEngine(aCI);
}


#endif // not SIMPLE_LINKING
#endif // ENGINE_LIBRARY


} // namespace sysync

/* end of TEngineInterface implementation */

#endif // ENGINEINTERFACE_SUPPORT

// eof
