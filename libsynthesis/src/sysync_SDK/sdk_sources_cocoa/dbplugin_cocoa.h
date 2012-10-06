/*
 *  File:    dbplugin_cocoa.h
 *
 *  Author:  Lukas Zeller (luz@plan44.ch)
 *
 *  Wrapper for writing Synthesis SyncML engine database plugins as
 *  Cocoa objects.
 *
 *  Copyright (c) 2008-2011 by Synthesis AG + plan44.ch
 *
 */

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#include "sync_include.h"   // include general SDK definitions
#include "sync_dbapidef.h"  // include the interface file and utilities
#include "SDK_util.h"       // include SDK utilities
#include "sync_dbapi.h"

#include "SettingsKey.h"

// macro used to wrap namespace around entry points for use in statically linked plugins (as needed for iPhoneOS)
// in this case, .m files are not directly compiled, but included from a mydatastore_plugin_wrapper.mm C++ file
// which also defines DB_PLUGIN_NAMESPACE first.
// To compile the common baseclass for all statically linked plugins, dbplugin_cocoa.m must be compiled with
// PLUGIN_BASECLASS defined (done by dbplugin_cocoa_wrapper.mm)
#ifndef PLUGIN_BASECLASS
#ifdef DB_PLUGIN_NAMESPACE
  #define PLUGIN_NS_BEGIN namespace DB_PLUGIN_NAMESPACE {
  #define PLUGIN_NS_END   }
  #define PLUGIN_NS_USE(sym) DB_PLUGIN_NAMESPACE::sym
#else
  #define PLUGIN_NS_BEGIN
  #define PLUGIN_NS_END
  #define PLUGIN_NS_USE(sym) sym
#endif
#endif // not PLUGIN_BASECLASS


// forward
@class CocoaPluginModule;
@class CocoaPluginDB;


#ifndef PLUGIN_BASECLASS
PLUGIN_NS_BEGIN

// Prototype for the module factory C function
// (must be implemented in a subclass)
CocoaPluginModule *newPluginModule(
  cAppCharP aModuleName,
  cAppCharP aSubName,
  cAppCharP aContextName,
  DB_Callback aCB
);

PLUGIN_NS_END
#endif // not PLUGIN_BASECLASS

// Wrapper for the module context
@interface CocoaPluginModule : NSObject
{
  // names
  NSString *fModuleName;
  NSString *fSubName;
  NSString *fContextName;
  // the callback structure
  DB_Callback fCB;
}
- (id)initWithModuleName:(cAppCharP)aModuleName
  subName:(cAppCharP)aSubName
  contextName:(cAppCharP)aContextName
  andCB:(DB_Callback)aCB;
- (void)dealloc;
// module level debug routines
- (void)debugOut:(NSString *)aMessage;
- (void)debugOutExotic:(NSString *)aMessage;
- (void)debugBlockWithTag:(NSString *)aTag andDescription:(NSString *)aDesc andAttrs:(NSString *)aAttrs;
- (void)debugEndBlockWithTag:(NSString *)aTag;
- (void)debugEndThread;
// access to CB
- (DB_Callback)getCB;
// Module identification
+ (int)buildNumber;
- (cAppCharP)manufacturerName;
- (cAppCharP)moduleName;
// Module capabilities
- (bool)hasDeleteSyncSet;
// parsing plugin parameters
- (TSyError)pluginParams:(cAppCharP)aParams  fromEngineVersion:(long)aEngineVersion;
// creating database context in this module's context
- (CocoaPluginDB *)newPluginDBWithName:(cAppCharP)aName
  deviceKey:(cAppCharP)aDeviceKey
  userKey:(cAppCharP)aUserKey
  andCB:(DB_Callback)aCB;
@end // CocoaPluginModule


// Wrapper for the DB context
@interface CocoaPluginDB : NSObject
{
  // names
  NSString *fContextName;
  NSString *fDeviceKey;
  NSString *fUserKey;
  // link to module context
  CocoaPluginModule *fModule;
  // the callback structure
  DB_Callback fCB;
}
- (id)initWithName:(cAppCharP)aName
  inModule:(CocoaPluginModule *)aModule
  deviceKey:(cAppCharP)aDeviceKey
  userKey:(cAppCharP)aUserKey
  andCB:(DB_Callback)aCB;
- (void)dealloc;
// DB level debug routines
- (void)debugOut:(NSString *)aMessage;
- (void)debugOutExotic:(NSString *)aMessage;
- (void)debugBlockWithTag:(NSString *)aTag andDescription:(NSString *)aDesc andAttrs:(NSString *)aAttrs;
- (void)debugEndBlockWithTag:(NSString *)aTag;
- (void)debugEndThread;
// access to CB
- (DB_Callback)getCB;
// access to session key
- (SettingsKey *)newOpenSessionKeyWithMode:(uInt16)aMode err:(TSyError *)aErrP;
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
- (TSyError)finalizeLocalID:(NSString **)aLocIDP;
- (TSyError)updateItemAsKey:(SettingsKey *)aItemKey
  itemIdP:(NSString **)aItemIdP parentIdP:(NSString **)aParentIdP;
- (TSyError)moveItem:(NSString *)aItemID
  fromParentID:(NSString *)aParentID toNewParent:(NSString *)aNewParentID;
- (TSyError)deleteItem:(NSString *)aItemID parentID:(NSString *)aParentID;
- (TSyError)deleteSyncSet;
- (TSyError)endDataWriteWithSuccess:(BOOL)aSuccess andNewToken:(NSString **)aNewTokenP;
@end // CocoaPluginDB



