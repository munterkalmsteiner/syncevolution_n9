/* SettingsKey */

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#include "syerror.h"
#include "sync_dbapidef.h"


// DBGNSLog helper macro, only included when DEBUG is #defined
#ifndef DBGNSLOG
#ifdef DEBUG
#define DBGNSLOG(...) NSLog(__VA_ARGS__)
#else
#define DBGNSLOG(...)
#endif
#endif

#ifndef DBGEXNSLOG
#if defined(DEBUG) && (DEBUG>1)
#define DBGEXNSLOG(...) NSLog(__VA_ARGS__)
#else
#define DBGEXNSLOG(...)
#endif
#endif



#ifdef __cplusplus
using namespace sysync;
#endif

// wrapper for a SySync settings key (engine, session or item)
@interface SettingsKey : NSObject
{
  // the call-in structure
  UI_Call_In fCI;
  // the engine key handle
  KeyH fKeyH;
  id fParentObj;
}

- (id)initWithCI:(UI_Call_In)aCI andKeyHandle:(void *)aKeyH;
- (void)dealloc;
// management
- (void)ownParent:(id)aParentObj;
- (void *)detachFromKeyHandle;
// Access to keys
- (KeyH)keyH;
- (SettingsKey *)newOpenKeyByPath:(cAppCharP)aPath withMode:(uInt16)aMode err:(TSyError *)aErrP;
- (SettingsKey *)newOpenSubKeyByID:(sInt32)aID withMode:(uInt16)aMode err:(TSyError *)aErrP;
- (sInt32)keyID;
- (TSyError)deleteSubKeyByID:(sInt32)aID;
// access to values
// - modes
- (TSyError)setTextCharset:(uInt16)aCharSet andLineEnds:(uInt16)aLineEndMode isBigEndian:(bool)aBigEndian;
- (TSyError)setTimeMode:(uInt16)aTimeMode;
// - id
- (sInt32)valueIDByName:(cAppCharP)aValName;
// - raw values
- (TSyError)valueByName:(cAppCharP)aValName asType:(uInt16)aValType intoBuffer:(appPointer)aBuffer ofSize:(memSize)aBufSize valSize:(memSize *)aValSizeP;
- (TSyError)valueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex asType:(uInt16)aValType intoBuffer:(appPointer)aBuffer ofSize:(memSize)aBufSize valSize:(memSize *)aValSizeP;
- (TSyError)setValueByName:(cAppCharP)aValName asType:(uInt16)aValType fromBuffer:(cAppPointer)aBuffer ofSize:(memSize)aValSize;
- (TSyError)setValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex asType:(uInt16)aValType fromBuffer:(cAppPointer)aBuffer ofSize:(memSize)aValSize;
// - strings
- (NSString *)stringValueByName:(cAppCharP)aValName;
- (NSString *)stringValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex;
- (NSString *)stringValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex withStatus:(TSyError *)aStatusP;
- (TSyError)setStringValueByName:(cAppCharP)aValName toValue:(NSString *)aString;
- (TSyError)setStringValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex toValue:(NSString *)aString;
// - 32 bit integers
- (sInt32)intValueByName:(cAppCharP)aValName;
- (sInt32)intValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex;
- (sInt32)intValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex withStatus:(TSyError *)aStatusP;
- (TSyError)setIntValueByName:(cAppCharP)aValName toValue:(sInt32)aInt;
- (TSyError)setIntValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex toValue:(sInt32)aInt;
// - 64 bit integers
- (sInt64)int64ValueByName:(cAppCharP)aValName;
- (sInt64)int64ValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex;
- (sInt64)int64ValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex withStatus:(TSyError *)aStatusP;
- (TSyError)setInt64ValueByName:(cAppCharP)aValName toValue:(sInt64)aInt;
- (TSyError)setInt64ValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex toValue:(sInt64)aInt;
// - date/time
- (NSDate *)dateValueByName:(cAppCharP)aValName;
- (NSDate *)dateValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex;
- (NSDate *)dateValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex withStatus:(TSyError *)aStatusP;
- (TSyError)setDateValueByName:(cAppCharP)aValName toValue:(NSDate *)aDate;
- (TSyError)setDateValueByID:(sInt32)aID arrayIndex:(sInt32)aArrayIndex  toValue:(NSDate *)aDate;
// - KVC
- (id)valueForKey:(NSString *)key;
- (void)setValue:(id)value forKey:(NSString *)key;
@end // SettingsKey
