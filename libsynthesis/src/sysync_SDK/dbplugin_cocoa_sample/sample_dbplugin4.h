/*
 *  File:    sample_dbplugin4.h
 *
 *  Author:  Lukas Zeller (luz@plan44.ch)
 *
 *  Sample for statically linked cocoa plugin libraries (as needed for iPhoneOS).
 *
 *  Copyright (c) 2011 plan44.ch
 *
 */

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#import "dbplugin_cocoa.h"


@interface SamplePluginModule4 : CocoaPluginModule
{
  //%%% todo: add member variables here
}
- (id)initWithModuleName:(cAppCharP)aModuleName
  subName:(cAppCharP)aSubName
  contextName:(cAppCharP)aContextName
  andCB:(DB_Callback)aCB;
- (void)dealloc;
// Module identification
+ (int)buildNumber;
- (cAppCharP)manufacturerName;
- (cAppCharP)moduleName;
// parsing plugin parameters
- (TSyError)pluginParams:(cAppCharP)aParams  fromEngineVersion:(long)aEngineVersion;
// creating database context in this module's context
- (CocoaPluginDB *)newPluginDBWithName:(cAppCharP)aName
  deviceKey:(cAppCharP)aDeviceKey
  userKey:(cAppCharP)aUserKey
  andCB:(DB_Callback)aCB;
@end // SamplePluginModule4


// Wrapper for the DB context
@interface SamplePluginDB4 : CocoaPluginDB
{
  //%%% todo: add member variables here
}
- (id)initWithName:(cAppCharP)aName
  inModule:(CocoaPluginModule *)aModule
  deviceKey:(cAppCharP)aDeviceKey
  userKey:(cAppCharP)aUserKey
  andCB:(DB_Callback)aCB;
- (void)dealloc;
// Context features
- (sInt32)contextSupportRules:(cAppCharP)aContextRules;
- (sInt32)filterSupportRules:(cAppCharP)aFilterRules;
// Thread change alert
- (void)threadMayChangeNow;
// Read phase
- (TSyError)startDataReadWithLastToken:(cAppCharP)aLastToken andResumeToken:(cAppCharP)aResumeToken;
- (TSyError)readNextItemAsKey:(SettingsKey *)aItemKey
  itemIdP:(NSString **)aItemIdP parentIdP:(NSString **)aParentIdP
  statusP:(sInt32 *)aStatusP
  isFirst:(BOOL)aFirst;
- (TSyError)readItemAsKey:(SettingsKey *)aItemKey
  itemID:(NSString *)aItemID parentID:(NSString *)aParentID;
- (TSyError)endDataRead;
// Write phase
- (TSyError)startDataWrite;
- (TSyError)insertItemAsKey:(SettingsKey *)aItemKey
  parentID:(NSString *)aParentID newItemIdP:(NSString **)aNewItemIdP;
#ifdef HAS_FINALIZE_LOCALID
- (TSyError)finalizeLocalID:(NSString **)aLocIDP;
#endif
- (TSyError)updateItemAsKey:(SettingsKey *)aItemKey
  itemIdP:(NSString **)aItemIdP parentIdP:(NSString **)aParentIdP;
- (TSyError)moveItem:(NSString *)aItemID
  fromParentID:(NSString *)aParentID toNewParent:(NSString *)aNewParentID;
- (TSyError)deleteItem:(NSString *)aItemID parentID:(NSString *)aParentID;
- (TSyError)deleteSyncSet;
- (TSyError)endDataWriteWithSuccess:(BOOL)aSuccess andNewToken:(NSString **)aNewTokenP;
@end // SamplePluginDB4
