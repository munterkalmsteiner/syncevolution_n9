/*
 *  File:    sample_dbplugin1.mm
 *
 *  Author:  Lukas Zeller (luz@plan44.ch)
 *
 *  Sample for statically linked cocoa plugin libraries (as needed for iPhoneOS).
 *
 *  Copyright (c) 2011 plan44.ch
 *
 */

// The following defines the name of the plugin to be created.
#define DB_PLUGIN_NAMESPACE iPhone_dbplugin2

// include actual source, but as this is a C++ file, the inclusion will also
// compile as C++ and can contain namespaces

// the generic cocoa interface
#include "../sdk_sources_cocoa/dbplugin_cocoa.m"

// From here on, the code is independent of dylib or static plugin. For making a dylib plugin out of this,
// just copy everything below into a .m file.

// (therefore the code below is an copy of sample_dbplugin.m, the sample plugin for the Mac OS X
// version of the sysync SDK, with only the object names enumerated to distinguish the 4 sample
// plugins that need to co-exist in the one any only Objective C namespace)

#import "sample_dbplugin2.h"

PLUGIN_NS_BEGIN

// the factory function
CocoaPluginModule *newPluginModule(
  cAppCharP aModuleName,
  cAppCharP aSubName,
  cAppCharP aContextName,
  DB_Callback aCB
) {
  return [[SamplePluginModule2 alloc]
    initWithModuleName:aModuleName
    subName:aSubName
    contextName:aContextName
    andCB:aCB
  ];
} // newPluginModule

PLUGIN_NS_END


@implementation SamplePluginModule2

- (id)initWithModuleName:(cAppCharP)aModuleName
  subName:(cAppCharP)aSubName
  contextName:(cAppCharP)aContextName
  andCB:(DB_Callback)aCB
{
  if ([
    super initWithModuleName:aModuleName
    subName:aSubName
    contextName:aContextName
    andCB:aCB
  ]!=nil) {
    // allocate an autorelease pool for the module context
    [self debugOut:@"Module context created"];
    // %%% todo: initialize module context
  }
  return self;
}


- (void)dealloc
{
  [self debugOut:@"Module context destroyed"];
  // %%% todo: deallocate any memory allocated within the context

  // done
  [super dealloc];
}


+ (int)buildNumber
{
  return 0;
}


- (cAppCharP)manufacturerName
{
  return "plan44.ch";
}


- (cAppCharP)moduleName
{
  return "Sample Plugin 2";
}


- (TSyError)pluginParams:(cAppCharP)aParams  fromEngineVersion:(long)aEngineVersion
{
  //%%% todo: parse plugin params if we support any
  // for now: supports no params
  // - return error if we get any params here
  return aParams==NULL || *aParams==0 ? LOCERR_OK : LOCERR_CFGPARSE;
}


// factory method for creating database context wrappers
- (CocoaPluginDB *)newPluginDBWithName:(cAppCharP)aName
  deviceKey:(cAppCharP)aDeviceKey
  userKey:(cAppCharP)aUserKey
  andCB:(DB_Callback)aCB
{
  return [[SamplePluginDB2 alloc]
    initWithName:aName
    inModule:self
    deviceKey:aDeviceKey
    userKey:aUserKey
    andCB:aCB
  ];
}

@end // SamplePluginModule2



@implementation SamplePluginDB2

- (id)initWithName:(cAppCharP)aName
  inModule:(CocoaPluginModule *)aModule
  deviceKey:(cAppCharP)aDeviceKey
  userKey:(cAppCharP)aUserKey
  andCB:(DB_Callback)aCB;
{
  if ([
    super initWithName:aName
    inModule:aModule
    deviceKey:aDeviceKey
    userKey:aUserKey
    andCB:aCB
  ]!=nil) {
    [self debugOut:@"DB context created"];
    //%%% todo: initialize DB context level stuff if there is any
  }
  return self;
}


- (void)dealloc
{
  [self debugOut:@"DB context destroyed"];
  //%%% todo: clean up DB context level stuff
  // done
  [super dealloc];
}



- (sInt32)contextSupportRules:(cAppCharP)aContextRules
{
  [self debugOut:[NSString stringWithFormat:
    @"Context support rules: %@",
    [NSString stringWithCString:aContextRules encoding:NSUTF8StringEncoding]
  ]];
  // %%% todo: parse context support rules

  // for now none supported
  return 0;
}


- (sInt32)filterSupportRules:(cAppCharP)aFilterRules
{
  [self debugOut:[NSString stringWithFormat:
    @"Filter support rules: %@",
    [NSString stringWithCString:aFilterRules encoding:NSUTF8StringEncoding]
  ]];
  // %%% todo: parse filters

  // for now none supported
  return 0;
}


- (void)threadMayChangeNow
{
  [self debugOut:@"threadMayChangeNow"];
  // %%% todo: make sure changing thread does not cause problems
}


- (TSyError)startDataReadWithLastToken:(cAppCharP)aLastToken andResumeToken:(cAppCharP)aResumeToken
{
  [self debugOut:[NSString stringWithFormat:
    @"startDataReadWithLastToken:%@ andResumeToken:%@",
    [NSString stringWithCString:aLastToken encoding:NSUTF8StringEncoding],
    [NSString stringWithCString:aResumeToken encoding:NSUTF8StringEncoding]
  ]];
  //%%% todo: actually prepare everything needed for readNextItemAsKey
  //%%% for now: just return OK
  return LOCERR_OK;
}


- (TSyError)readNextItemAsKey:(SettingsKey *)aItemKey
  itemIdP:(NSString **)aItemIdP parentIdP:(NSString **)aParentIdP
  statusP:(sInt32 *)aStatusP
  isFirst:(BOOL)aFirst
{
  TSyError sta = LOCERR_OK;

  [self debugOut:[NSString stringWithFormat:@"readNextItemAsKey: aFirst=%d", aFirst]];
  //%%% todo: actually read first or next item from DB
  //%%% for now: just simulate empty DB
  *aStatusP = ReadNextItem_EOF;
  sta = LOCERR_OK;
  // done
  return sta;
}



- (TSyError)readItemAsKey:(SettingsKey *)aItemKey
  itemID:(NSString *)aItemID parentID:(NSString *)aParentID
{
  TSyError sta = LOCERR_OK;

  [self debugOut:[NSString stringWithFormat:
    @"readItemAsKey: itemID=%@ parentID=%@",
    aItemID, aParentID
  ]];
  //%%% todo: read item from DB
  //%%% for now just return "not found" status (our simulated DB is empty)
  sta = DB_NotFound;
  // done
  return sta;
}


- (TSyError)endDataRead
{
  [self debugOut:@"endDataRead"];
  //%%% todo: get rid of ressources only needed during readNextItemAsKey calls (such as cached list of items etc.)
  //%%% for now: just ok
  return LOCERR_OK;
}


- (TSyError)startDataWrite
{
  [self debugOut:@"startDataWrite"];
  //%%% todo: prepare DB for writing
  //%%% for now: just ok
  return LOCERR_OK;
}


- (TSyError)insertItemAsKey:(SettingsKey *)aItemKey
  parentID:(NSString *)aParentID newItemIdP:(NSString **)aNewItemIdP
{
  TSyError sta = LOCERR_OK;

  [self debugOut:[NSString stringWithFormat:
    @"insertItemAsKey: parentID=%@",
    aParentID
  ]];
  //%%% todo: insert item into DB and return new ID
  //%%% for now just return DB error (our simulated DB is always empty and can't be written)
  sta = DB_Error;
  // done
  return sta;
}


#ifdef HAS_FINALIZE_LOCALID
// check if insert ID is temporary, and if so return real (persistent)
- (TSyError)finalizeLocalID:(NSString **)aLocIDP
{
  [self debugOutExotic:[NSString stringWithFormat:
    @"finalizeLocalID: tempLocID=%@",
    *aLocIDP
  ]];
  //%%% todo: check if this is a temporary ID that needs to be finalized
  //    Note: this is for DBs which save all changes at once at endDataWriteWithSuccess
  //          and receive persistent IDs only then. For those, insertItemAsKey can
  //          return temporary items which will be finalized using this call.
  //%%% for now: no finalisation needed
  return DB_NoContent;
}
#endif



- (TSyError)updateItemAsKey:(SettingsKey *)aItemKey
  itemIdP:(NSString **)aItemIdP parentIdP:(NSString **)aParentIdP
{
  TSyError sta = LOCERR_OK;

  [self debugOut:[NSString stringWithFormat:
    @"updateItemAsKey: parentID=%@, itemID=%@",
    *aParentIdP, *aItemIdP
  ]];
  //%%% todo: update item in DB
  //%%% for now just return DB error (our simulated DB is always empty and can't be written)
  sta = DB_Error;
  // done
  return sta;
}


- (TSyError)moveItem:(NSString *)aItemID
  fromParentID:(NSString *)aParentID toNewParent:(NSString *)aNewParentID
{
  [self debugOut:@"moveItem"];
  // %%% todo: actually move item to new parent (if possible)
  return LOCERR_NOTIMP;
}


- (TSyError)deleteItem:(NSString *)aItemID parentID:(NSString *)aParentID
{
  TSyError sta = LOCERR_OK;

  [self debugOut:[NSString stringWithFormat:
    @"deleteItem: parentID=%@, itemID=%@",
    aParentID, aItemID
  ]];
  //%%% todo: delete item from DB
  //%%% for now just return DB error (our simulated DB is always empty and can't be written)
  // done
  return sta;
}


- (TSyError)deleteSyncSet
{
  TSyError sta = LOCERR_OK;

  [self debugOut:@"deleteAllItems"];
  //%%% todo:delete all items from the DB which belong to the sync set (=usually all, unless we have filters)
  // done
  return sta;
}



- (TSyError)endDataWriteWithSuccess:(BOOL)aSuccess andNewToken:(NSString **)aNewTokenP
{
  TSyError sta = LOCERR_OK;

  [self debugOut:[NSString stringWithFormat:@"endDataWriteWithSuccess:%d", aSuccess]];
  //%%% todo: if needed, commit changes to DB
  //%%% todo: THEN create "this sync" token (after all changes are saved)
  //%%% for now we just return the absolute timestamp of now
  sInt64 ll = CFAbsoluteTimeGetCurrent();
  *aNewTokenP = [NSString stringWithFormat:@"%lld",ll];
  // done
  return sta;
}



@end // SamplePluginDB2



/* eof */