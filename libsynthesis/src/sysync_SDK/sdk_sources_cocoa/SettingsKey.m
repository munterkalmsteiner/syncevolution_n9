#import "SettingsKey.h"

#include <dlfcn.h>
#include "sdk_util.h" // for Plugin_Version()

// if defined, living instances are recorded in a set to monitor if all are closed properly (DEBUG only)
//#define SETTINGSKEY_MONITORINSTANCES 1


#if defined(SETTINGSKEY_MONITORINSTANCES) && defined(DEBUG)

NSMutableSet *openSettingsKeySet = nil;

void key_created(id aKey)
{
  if (!openSettingsKeySet) {
    openSettingsKeySet = [[NSMutableSet alloc] init];
  }
  [openSettingsKeySet addObject:[NSNumber numberWithUnsignedLong:(intptr_t)aKey]];
}

void key_deleted(id aKey)
{
  [openSettingsKeySet removeObject:[NSNumber numberWithUnsignedLong:(intptr_t)aKey]];
  NSMutableString *s = [NSMutableString string];
  for (id key in openSettingsKeySet) {
    id keyPtr = (id)[key unsignedLongValue];
    [s appendFormat:@"0x%lX(%d), ", (intptr_t)keyPtr, [keyPtr retainCount]];
  }
  NSLog(@"Still open keys: %@",s);
}

#define KEY_CREATED(k) key_created(k)
#define KEY_DELETED(k) key_deleted(k)
#define KEYNSLOG(...) DBGNSLOG(__VA_ARGS__)

#else
#define KEY_CREATED(k)
#define KEY_DELETED(k)
#define KEYNSLOG(...)
#endif


// wrapper for a SySync settings key (engine, session or item)
@implementation SettingsKey

- (id)initWithCI:(UI_Call_In)aCI andKeyHandle:(void *)aKeyH
{
  if ((self = [super init])) {
    fCI = aCI;
    fKeyH = aKeyH;
    fParentObj=nil;
  }
  KEY_CREATED(self);
  return self;
} // initWithCI


// have the settings key own another object and release that on close (usually the parent key)
- (void)ownParent:(id)aParentObj
{
  fParentObj = [aParentObj retain]; // own it
} // ownParent


// detach wrapper object from handle
- (void *)detachFromKeyHandle
{
  void *keyH = fKeyH;
  fKeyH = NULL;
  fCI = NULL;
  return keyH;
} // detachFromKeyHandle


// dealloc implicitly closes key (except if it was previously detached with -detachFromKeyHandle)
- (void)dealloc
{
  // close key
  if (fCI && fKeyH) {
    CloseKey_Func CloseKey= fCI->ui.CloseKey;
    if (CloseKey) {
      CloseKey(fCI,fKeyH);
      fKeyH=NULL;
    }
  }
  // close parent if one is assigned
  [fParentObj release];
  KEYNSLOG(@"Deallocating SettingsKey object = 0x%lX",(intptr_t)self);  
  KEY_DELETED(self);
  // done
  [super dealloc];
} // dealloc


// Access to keys

// note: this is explicitly implemented to make it compatible with 10.4 SDK and compilers
- (KeyH)keyH
{
  return fKeyH;
}

- (SettingsKey *)newOpenKeyByPath:(cAppCharP)aPath withMode:(uInt16)aMode err:(TSyError *)aErrP
{
  TSyError sta=LOCERR_OK;
  KeyH newKeyH=NULL;
  SettingsKey *newKey=nil; // no object by default
  OpenKeyByPath_Func OpenKeyByPath = fCI->ui.OpenKeyByPath;
  if (!OpenKeyByPath) {
    sta=LOCERR_NOTIMP;
  }
  else {
    sta = OpenKeyByPath(fCI,&newKeyH,fKeyH,aPath,aMode);
    if (sta==LOCERR_OK) {
      newKey=[[SettingsKey alloc] initWithCI:fCI andKeyHandle:newKeyH];
      KEYNSLOG(@"Opened settings key %s : SettingsKey object = 0x%lX",aPath,(intptr_t)newKey);
    }
  }
  if (aErrP) *aErrP=sta;
  return newKey;
} // newOpenKeyByPath


- (SettingsKey *)newOpenSubKeyByID:(sInt32)aID withMode:(uInt16)aMode err:(TSyError *)aErrP
{
  KeyH newKeyH=NULL;
  TSyError sta=LOCERR_OK;
  SettingsKey *newKey=nil; // no object by default
  OpenSubkey_Func OpenSubkey = fCI->ui.OpenSubkey;
  if (!OpenSubkey) {
    sta=LOCERR_NOTIMP;
  }
  else {
    sta = OpenSubkey(fCI,&newKeyH,fKeyH,aID,aMode);
    if (sta==LOCERR_OK) {
      newKey=[[SettingsKey alloc] initWithCI:fCI andKeyHandle:newKeyH];
      KEYNSLOG(@"Opened settings subkey by ID=%d (actual ID=%d): SettingsKey object = 0x%lX",aID,[newKey keyID],(intptr_t)newKey);
    }
  }
  if (aErrP) *aErrP=sta;
  return newKey;
} // newOpenSubKeyByID


- (sInt32)keyID
{
  sInt32 theID = KEYVAL_ID_UNKNOWN;
  GetKeyID_Func GetKeyID = fCI->ui.GetKeyID;
  if (!GetKeyID) return KEYVAL_ID_UNKNOWN;
  GetKeyID(fCI, fKeyH, &theID);
  return theID;
} // keyID


- (TSyError)deleteSubKeyByID:(sInt32)aID
{
  DeleteSubkey_Func DeleteSubkey = fCI->ui.DeleteSubkey;
  if (!DeleteSubkey) return LOCERR_NOTIMP;
  return DeleteSubkey(fCI, fKeyH, aID);
} // deleteSubKeyByID



// Modes

- (TSyError)setTextCharset:(uInt16)aCharSet andLineEnds:(uInt16)aLineEndMode isBigEndian:(bool)aBigEndian;
{
  SetTextMode_Func SetTextMode = fCI->ui.SetTextMode;
  if (!SetTextMode) return LOCERR_NOTIMP;
  return SetTextMode(fCI,fKeyH,aCharSet,aLineEndMode,aBigEndian);
} // setTextCharset

- (TSyError)setTimeMode:(uInt16)aTimeMode;
{
  SetTimeMode_Func SetTimeMode = fCI->ui.SetTimeMode;
  if (!SetTimeMode) return LOCERR_NOTIMP;
  return SetTimeMode(fCI,fKeyH,aTimeMode);
} // setTimeMode


// IDs

- (sInt32)valueIDByName:(cAppCharP)aValName
{
  GetValueID_Func GetValueID= fCI->ui.GetValueID;
  if (!GetValueID) return KEYVAL_ID_UNKNOWN;
  return GetValueID(fCI, fKeyH, aValName);
} // valueIDByName


#pragma mark access to values

// - raw

- (TSyError)valueByName:(cAppCharP)aValName asType:(uInt16)aValType
  intoBuffer:(appPointer)aBuffer ofSize:(memSize)aBufSize valSize:(memSize *)aValSizeP
{
  GetValue_Func GetValue= fCI->ui.GetValue;
  if (!GetValue) return LOCERR_NOTIMP;
  return GetValue(fCI, fKeyH, aValName, aValType, aBuffer, aBufSize, aValSizeP);
} // valueByName


- (TSyError)valueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex asType:(uInt16)aValType
  intoBuffer:(appPointer)aBuffer ofSize:(memSize)aBufSize valSize:(memSize *)aValSizeP
{
  GetValueByID_Func GetValueByID= fCI->ui.GetValueByID;
  if (!GetValueByID) return LOCERR_NOTIMP;
  return GetValueByID(fCI, fKeyH, aID, aArrayIndex, aValType, aBuffer, aBufSize, aValSizeP);
} // valueByID


- (TSyError)setValueByName:(cAppCharP)aValName asType:(uInt16)aValType
  fromBuffer:(cAppPointer)aBuffer ofSize:(memSize)aValSize
{
  SetValue_Func SetValue= fCI->ui.SetValue;
  if (!SetValue) return LOCERR_NOTIMP;
  return SetValue(fCI, fKeyH, aValName, aValType, aBuffer, aValSize);
} // setValueByName


- (TSyError)setValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex asType:(uInt16)aValType
  fromBuffer:(cAppPointer)aBuffer ofSize:(memSize)aValSize;
{
  SetValueByID_Func SetValueByID= fCI->ui.SetValueByID;
  if (!SetValueByID) return LOCERR_NOTIMP;
  return SetValueByID(fCI, fKeyH, aID, aArrayIndex, aValType, aBuffer, aValSize);
} // setValueByID


// - strings

// Note: it is important NOT to simplify the implementation by emulating the byName access by
//       trying to use valueIDByName and then valueByID, as some string values can ONLY be accessed
//       by name, and not by ID!

- (NSString *)stringValueByName:(cAppCharP)aValName
{
  const size_t stdBufSiz = 255;
  appCharP strP;
  size_t valSz;
  TSyError sta = LOCERR_OUTOFMEM;
  NSString *result = nil;

  // try with standard buffer size first
  strP = malloc(stdBufSiz);
  if (strP) {
    sta = [self valueByName:aValName asType:VALTYPE_TEXT intoBuffer:strP ofSize:stdBufSiz valSize:&valSz];
    if (sta==LOCERR_BUFTOOSMALL || sta==LOCERR_TRUNCATED) {
      // buffer too small to return anything, valSz contains needed size
      free(strP);
      if (sta==LOCERR_TRUNCATED) {
        // buffer too small to return all data, but truncated version was returned.
        // - measure overall data size (valSz only contains size of truncated data now!)
        sta = [self valueByName:aValName asType:VALTYPE_TEXT intoBuffer:NULL ofSize:0 valSize:&valSz];
      }
      // allocate a buffer that is large enough
      strP = malloc(valSz+1);
      if (strP!=NULL)
        sta = [self valueByName:aValName asType:VALTYPE_TEXT intoBuffer:strP ofSize:valSz+1 valSize:&valSz];
    }
    if (sta==LOCERR_OK) {
      // got value ok, now fill into NSString
      // Note: if strP contains invalid UTF8, this will return nil!
      result = [NSString stringWithCString:strP encoding:NSUTF8StringEncoding];
      #ifdef DEBUG
      if (!result) {
        DBGNSLOG(@"stringValueByName:%s returns string with incorrect UTF-8 encoding",aValName);
      }
      #endif
    }
    // return buffer
    free(strP);
  }
  #ifdef DEBUG
  if (sta!=LOCERR_OK) {
    DBGEXNSLOG(@"stringValueByName:%s returns err=%hd",aValName,sta);
  }
  #endif
  return result;
} // stringValueByName


- (NSString *)stringValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex
{
  return [self stringValueByID:aID arrayIndex:aArrayIndex withStatus:NULL];
} // intValueByID


- (NSString *)stringValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex withStatus:(TSyError *)aStatusP
{
  const size_t stdBufSiz = 255;
  appCharP strP;
  size_t valSz;
  TSyError sta = LOCERR_OUTOFMEM;
  NSString *result = nil;

  // try with standard buffer size first
  strP = malloc(stdBufSiz);
  if (strP) {
    sta = [self valueByID:aID arrayIndex:aArrayIndex asType:VALTYPE_TEXT intoBuffer:strP ofSize:stdBufSiz valSize:&valSz];
    if (sta==LOCERR_BUFTOOSMALL || sta==LOCERR_TRUNCATED) {
      // buffer too small to return anything, valSz contains needed size
      free(strP);
      if (sta==LOCERR_TRUNCATED) {
        // buffer too small to return all data, but truncated version was returned.
        // - measure overall data size (valSz only contains size of truncated data now!)
        sta = [self valueByID:aID arrayIndex:aArrayIndex asType:VALTYPE_TEXT intoBuffer:NULL ofSize:0 valSize:&valSz];
      }
      // allocate a buffer that is large enough
      strP = malloc(valSz+1);
      if (strP!=NULL)
        sta = [self valueByID:aID arrayIndex:aArrayIndex asType:VALTYPE_TEXT intoBuffer:strP ofSize:valSz+1 valSize:&valSz];
    }
    if (sta==LOCERR_OK) {
      // got value ok, now fill into NSString
      result = [NSString stringWithCString:strP encoding:NSUTF8StringEncoding];
    }
    // return buffer
    free(strP);
  }
  #ifdef DEBUG
  if (sta!=LOCERR_OK) {
    DBGEXNSLOG(@"stringValueByID:%ld arrayIndex:%ld returns err=%hd",(sIntArch)aID,(sIntArch)aArrayIndex,sta);
  }
  #endif
  if (aStatusP) *aStatusP=sta;
  return result;
} // stringValueByID


- (TSyError)setStringValueByName:(cAppCharP)aValName toValue:(NSString *)aString
{
  return [
    self setValueByName:aValName asType:VALTYPE_TEXT
    fromBuffer:[(aString ? aString : @"") cStringUsingEncoding:NSUTF8StringEncoding] ofSize:(uIntArch)-1 // autosize from C-String
  ];
} // setStringValueByName


- (TSyError)setStringValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex toValue:(NSString *)aString;
{
  return [
    self setValueByID:aID arrayIndex:aArrayIndex asType:VALTYPE_TEXT
    fromBuffer:[(aString ? aString : @"") cStringUsingEncoding:NSUTF8StringEncoding] ofSize:(uIntArch)-1 // autosize from C-String
  ];
} // setStringValueByID



// int32 values

- (sInt32)intValueByName:(cAppCharP)aValName
{
  sInt32 myVal=0;
  size_t myValSz;
  #ifdef DEBUG
  TSyError sta;
  sta =
  #endif
    [self valueByName:aValName asType:VALTYPE_INT32 intoBuffer:&myVal ofSize:sizeof(myVal) valSize:&myValSz];
  #ifdef DEBUG
  if (sta!=LOCERR_OK) {
    DBGEXNSLOG(@"intValueByName:%s returns err=%hd",aValName,sta);
  }
  #endif
  return myVal;
} // intValueByName


- (sInt32)intValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex
{
  return [self intValueByID:aID arrayIndex:aArrayIndex withStatus:NULL];
} // intValueByID


- (sInt32)intValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex withStatus:(TSyError *)aStatusP
{
  sInt32 myVal=0;
  size_t myValSz;
  TSyError sta;
  sta = [self valueByID:aID arrayIndex:aArrayIndex asType:VALTYPE_INT32 intoBuffer:&myVal ofSize:sizeof(myVal) valSize:&myValSz];
  #ifdef DEBUG
  if (sta!=LOCERR_OK) {
    DBGEXNSLOG(@"intValueByID:%ld arrayIndex:%ld returns err=%hd",(sIntArch)aID,(sIntArch)aArrayIndex,sta);
  }
  #endif
  if (aStatusP) *aStatusP=sta;
  return myVal;
} // intValueByID withStatus


- (TSyError)setIntValueByName:(cAppCharP)aValName toValue:(sInt32)aInt
{
  return [self setValueByName:aValName asType:VALTYPE_INT32 fromBuffer:&aInt ofSize:sizeof(aInt)];
} // setIntValueByName


- (TSyError)setIntValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex toValue:(sInt32)aInt
{
  return [self setValueByID:aID arrayIndex:aArrayIndex asType:VALTYPE_INT32 fromBuffer:&aInt ofSize:sizeof(aInt)];
} // setIntValueByID



// int64 values

- (sInt64)int64ValueByName:(cAppCharP)aValName
{
  sInt64 myVal=0;
  size_t myValSz;
  #ifdef DEBUG
  TSyError sta;
  sta =
  #endif
    [self valueByName:aValName asType:VALTYPE_INT64 intoBuffer:&myVal ofSize:sizeof(myVal) valSize:&myValSz];
  #ifdef DEBUG
  if (sta!=LOCERR_OK) {
    DBGEXNSLOG(@"int64ValueByName:%s returns err=%hd\n",aValName,sta);
  }
  #endif
  return myVal;
} // int64ValueByName


- (sInt64)int64ValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex
{
  return [self int64ValueByID:aID arrayIndex:aArrayIndex withStatus:NULL];
} // int64ValueByID


- (sInt64)int64ValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex withStatus:(TSyError *)aStatusP
{
  sInt64 myVal=0;
  size_t myValSz;
  TSyError sta;
  sta = [self valueByID:aID arrayIndex:aArrayIndex asType:VALTYPE_INT64 intoBuffer:&myVal ofSize:sizeof(myVal) valSize:&myValSz];
  #ifdef DEBUG
  if (sta!=LOCERR_OK) {
    DBGEXNSLOG(@"intValueByID:%ld arrayIndex:%ld returns err=%hd",(sIntArch)aID,(sIntArch)aArrayIndex,sta);
  }
  #endif
  if (aStatusP) *aStatusP=sta;
  return myVal;
} // int64ValueByID withStatus


- (TSyError)setInt64ValueByName:(cAppCharP)aValName toValue:(sInt64)aInt
{
  return [self setValueByName:aValName asType:VALTYPE_INT64 fromBuffer:&aInt ofSize:sizeof(aInt)];
} // setInt64ValueByName


- (TSyError)setInt64ValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex toValue:(sInt64)aInt
{
  return [self setValueByID:aID arrayIndex:aArrayIndex asType:VALTYPE_INT32 fromBuffer:&aInt ofSize:sizeof(aInt)];
} // setInt64ValueByID




// date values

// Note: Date values must be keys that have an ID. ID-less datetime keys cannot be used (but do not exist
//       in the engine, so the simplified implementation is ok here)

- (NSDate *)dateValueByName:(cAppCharP)aValName
{
  // get ID by name
  sInt32 valID = [self valueIDByName:aValName];
  // obtain by ID
  return [self dateValueByID:valID arrayIndex:0];
} // dateValueByName


- (NSDate *)dateValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex
{
  return [self dateValueByID:aID arrayIndex:aArrayIndex withStatus:NULL];
}


- (NSDate *)dateValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex withStatus:(TSyError *)aStatusP
{
  sInt64 myVal;
  size_t myValSz;
  TSyError sta;
  [self setTimeMode:TMODE_UNIXTIME_MS+TMODE_FLAG_UTC]; // unix epoch time in milliseconds
  sta = [self valueByID:aID arrayIndex:aArrayIndex asType:VALTYPE_TIME64 intoBuffer:&myVal ofSize:sizeof(myVal) valSize:&myValSz];
  if (aStatusP) *aStatusP=sta;
  if (sta!=LOCERR_OK)
    return nil; // no time
  else {
    NSDate *nsdat = [NSDate dateWithTimeIntervalSince1970:(NSTimeInterval)myVal/1000.0];
    DBGEXNSLOG(@"dateValueByID: myVal = %lld returned as NSDate = %@", myVal, nsdat);
    return nsdat;
  }
} // dateValueByID


- (TSyError)setDateValueByName:(cAppCharP)aValName toValue:(NSDate *)aDate
{
  // get ID by name
  sInt32 valID = [self valueIDByName:aValName];
  // obtain by ID
  return [self setDateValueByID:valID arrayIndex:0 toValue:aDate];
} // setDateValueByName


- (TSyError)setDateValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex  toValue:(NSDate *)aDate
{
  sInt64 myVal = 0; // no time
  if (aDate!=nil)
    myVal = [aDate timeIntervalSince1970]*1000.0; // milliseconds
  DBGEXNSLOG(@"setDateValueByID: aDate = %@, returned as myVal = %lld", aDate, myVal);
  [self setTimeMode:TMODE_UNIXTIME_MS+TMODE_FLAG_UTC]; // unix epoch time in milliseconds
  return [self setValueByID:aID arrayIndex:aArrayIndex asType:VALTYPE_TIME64 fromBuffer:&myVal ofSize:sizeof(myVal)];
} // setDateValue



#pragma mark basic KVC access to values


- (id)valueForKey:(NSString *)key
{
  // determine native value type
  uInt16 valtype;
  size_t myValSz;
  [self valueByName:[[key stringByAppendingString:@"" ".VALTYPE"] UTF8String]
    asType:VALTYPE_INT16 intoBuffer:&valtype ofSize:sizeof(valtype) valSize:&myValSz
  ];
  // handle according to type
  switch (valtype) {
    case VALTYPE_TEXT:
      return [self stringValueByName:[key UTF8String]];
    case VALTYPE_TIME64:
    case VALTYPE_TIME32:
      return [self dateValueByName:[key UTF8String]];
    case VALTYPE_INT64:
      return [NSNumber numberWithLongLong:[self int64ValueByName:[key UTF8String]]];
    case VALTYPE_INT32:
    case VALTYPE_INT16:
    case VALTYPE_INT8:
      return [NSNumber numberWithLong:[self intValueByName:[key UTF8String]]];
    default:
      return nil;
  }
}



- (void)setValue:(id)value forKey:(NSString *)key
{
  // handle according to object type
  if ([value isKindOfClass:[NSString class]]) {
    // set as string
    [self setStringValueByName:[key UTF8String] toValue:value];
    return;
  }
  else if ([value isKindOfClass:[NSDate class]]) {
    // set as date
    [self setDateValueByName:[key UTF8String] toValue:value];
    return;
  }
  else if ([value isKindOfClass:[NSNumber class]]) {
    [self setInt64ValueByName:[key UTF8String] toValue:[value longLongValue]];
    return;
  }
}





@end // SettingsKey
