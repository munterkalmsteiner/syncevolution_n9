/*
 *  File:    dbplugin_cocoa.m
 *
 *  Author:  Lukas Zeller (luz@plan44.ch)
 *
 *  Wrapper for writing Synthesis SyncML engine database plugins as
 *  Cocoa objects.
 *
 *  Copyright (c) 2008-2009 by Synthesis AG
 *
 */

#include "dbplugin_cocoa.h"

#define BuildNumber  0  // User defined build number


#define MyDB  "cocoaPlugin" /* example debug name */
#define MY_ID       42  /* example datastore context */

#define STRLEN      80  /* Max length of local string copies */


// Note: in case this source file is included from a wrapper .mm file or a static-only plugin.mm
//       to create statically linked plugins, only the part of the source which is specific to the
//       C++ namespace of the plugin is compiled, but not the ObjC part which is common to all
//       statically linked plugins and this must only exist once.
//       - A defined DB_PLUGIN_NAMESPACE here means that file generates C++ glue in a namespace for
//         linking the plugin statically
//       - A defined PLUGIN_BASECLASS here means that file generates only the ObjC baseclass
//         that must exist only once with statically linked plugins and is shared among all plugins.

#ifndef DB_PLUGIN_NAMESPACE

/* Show all fields/variables of item represented by aItemKey */
static void ShowFields(DB_Callback cb, KeyH aItemKey)
{
  const stringSize maxstr = 128;
  appChar fnam[maxstr];
  appChar fval[maxstr];
  appChar ftz[maxstr];
  uInt16 fvaltype;
  uInt16 farrsize;
  uInt16 arridx;
  bool fisarray;
  uIntArch valsize;
  TSyError err;
  uInt32 valueID,nameFlag,typeFlag,arrszFlag,tznamFlag;
  // set desired time mode
  cb->ui.SetTimeMode(cb, aItemKey, TMODE_LINEARTIME+TMODE_FLAG_FLOATING);
  // get flags that can be combined with valueID to get attributes of a value
  nameFlag = cb->ui.GetValueID(cb, aItemKey, VALNAME_FLAG VALSUFF_NAME);
  typeFlag = cb->ui.GetValueID(cb, aItemKey, VALNAME_FLAG VALSUFF_TYPE);
  arrszFlag = cb->ui.GetValueID(cb, aItemKey, VALNAME_FLAG VALSUFF_ARRSZ);
  tznamFlag = cb->ui.GetValueID(cb, aItemKey, VALNAME_FLAG VALSUFF_TZNAME);
  // iterate over all fields
  // - start iteration
  valueID = cb->ui.GetValueID(cb, aItemKey, VALNAME_FIRST);
  while (valueID != KEYVAL_ID_UNKNOWN && valueID != KEYVAL_NO_ID) {
    // get field name
    err = cb->ui.GetValueByID(cb,
      aItemKey,
      valueID + nameFlag,
      0,
      VALTYPE_TEXT,
      fnam,
      maxstr,
      &valsize
    );
    // get field type
    err = cb->ui.GetValueByID(cb,
      aItemKey,
      valueID + typeFlag,
      0,
      VALTYPE_INT16,
      &fvaltype,
      sizeof(fvaltype),
      &valsize
    );
    // check if array, and if array, get number of elements
    err = cb->ui.GetValueByID(cb,
      aItemKey,
      valueID + arrszFlag,
      0,
      VALTYPE_INT16,
      &farrsize,
      sizeof(farrsize),
      &valsize
    );
    fisarray = err==LOCERR_OK;

    if (!fisarray) {
      // single value
      err = cb->ui.GetValueByID(cb, aItemKey, valueID, 0, VALTYPE_TEXT, fval, maxstr, &valsize);
      if (err==LOCERR_OK) {
        if (fvaltype==VALTYPE_TIME64) {
          // for timestamps, get time zone name as well
          cb->ui.GetValueByID(cb, aItemKey, valueID+tznamFlag, 0, VALTYPE_TEXT, ftz, maxstr, &valsize);
          DEBUG_(cb, "- %-20s (VALTYPE=%2hd) = %s timezone=%s",fnam,fvaltype,fval,ftz);
        }
        else
          DEBUG_(cb, "- %-20s (VALTYPE=%2hd) = '%s'",fnam,fvaltype,fval);
      }
      else
        DEBUG_(cb, "- %-20s (VALTYPE=%2hd) : No value, error=%hd",fnam,fvaltype,err);
    }
    else {
      // array
      DEBUG_(cb, "- %-20s (VALTYPE=%2d) = Array with %d elements",fnam,fvaltype,farrsize);
      // show elements
      for (arridx=0; arridx<farrsize; arridx++) {
        err = cb->ui.GetValueByID(cb, aItemKey, valueID, arridx, VALTYPE_TEXT, fval, maxstr, &valsize);
        if (err==LOCERR_OK) {
          if (fvaltype==VALTYPE_TIME64) {
            // for timestamps, get time zone name as well
            cb->ui.GetValueByID(cb, aItemKey, valueID+tznamFlag, arridx, VALTYPE_TEXT, ftz, maxstr, &valsize);
            DEBUG_(cb, "           %20s[%3hd] = %s timezone=%s",fnam,arridx,fval,ftz);
          }
          else
            DEBUG_(cb, "           %20s[%3hd] = '%s'",fnam,arridx,fval);
        }
        else
          DEBUG_(cb, "           %20s[%3hd] : No value, error=%hd",fnam,arridx,err);
      }
    }
    // next value
    valueID = cb->ui.GetValueID(cb, aItemKey, VALNAME_NEXT);
  } // while more values
} /* ShowFields */


// MODULE CONTEXT
// ==============

@implementation CocoaPluginModule

- (id)initWithModuleName:(cAppCharP)aModuleName
  subName:(cAppCharP)aSubName
  contextName:(cAppCharP)aContextName
  andCB:(DB_Callback)aCB
{
  if ([super init]!=nil) {
    // save the name strings
    fModuleName = [NSString stringWithCString:aModuleName encoding:NSUTF8StringEncoding];
    fSubName = [NSString stringWithCString:aSubName encoding:NSUTF8StringEncoding];
    fContextName = [NSString stringWithCString:aContextName encoding:NSUTF8StringEncoding];
    // save the callback structure
    fCB = aCB;
  }
  return self;
}


- (void)dealloc
{
  // done
  [super dealloc];
}


// access to CB
- (DB_Callback)getCB
{
  return fCB;
}


// Debug routines for plugin to send messages into SyncML engine log

- (void)debugOut:(NSString *)aMessage
{
  DB_DebugPuts_Func DB_DebugPuts = fCB->DB_DebugPuts;
  if (!DB_DebugPuts) return; // not implemented, no output
  DB_DebugPuts(fCB->callbackRef, [aMessage cStringUsingEncoding:NSUTF8StringEncoding]);
}


- (void)debugOutExotic:(NSString *)aMessage
{
  DB_DebugPuts_Func DB_DebugExotic = fCB->DB_DebugExotic;
  if (!DB_DebugExotic) return; // not implemented, no output
  DB_DebugExotic(fCB->callbackRef, [aMessage cStringUsingEncoding:NSUTF8StringEncoding]);
}


- (void)debugBlockWithTag:(NSString *)aTag andDescription:(NSString *)aDesc andAttrs:(NSString *)aAttrs
{
  DB_DebugBlock_Func DB_DebugBlock = fCB->DB_DebugBlock;
  if (!DB_DebugBlock) return; // not implemented, no output
  DB_DebugBlock(fCB->callbackRef,
    [aTag cStringUsingEncoding:NSUTF8StringEncoding],
    [aDesc cStringUsingEncoding:NSUTF8StringEncoding],
    [aAttrs cStringUsingEncoding:NSUTF8StringEncoding]
  );
}


- (void)debugEndBlockWithTag:(NSString *)aTag
{
  DB_DebugEndBlock_Func DB_DebugEndBlock = fCB->DB_DebugEndBlock;
  if (!DB_DebugEndBlock) return; // not implemented, no output
  DB_DebugEndBlock(fCB->callbackRef,[aTag cStringUsingEncoding:NSUTF8StringEncoding]);
}


- (void)debugEndThread
{
  DB_DebugEndThread_Func DB_DebugEndThread = fCB->DB_DebugEndThread;
  if (!DB_DebugEndThread) return; // not implemented, no output
  DB_DebugEndThread(fCB->callbackRef);
}



// Dummy implementation - should be overridden in actual DB implementations in subclass

+ (int)buildNumber
{
  return 0;
}


- (cAppCharP)manufacturerName
{
  return "Synthesis AG";
}


- (cAppCharP)moduleName
{
  return "CocoaPluginModule";
}


- (bool)hasDeleteSyncSet
{
  return NO; // not by default, implementation must explicitly override if it really supports it
}


- (TSyError)pluginParams:(cAppCharP)aParams  fromEngineVersion:(long)aEngineVersion
{
  // base class supports no params
  // - return error if we get any params here
  return aParams==NULL || *aParams==0 ? LOCERR_OK : LOCERR_CFGPARSE;
}


// factory method for creating database context wrappers
- (CocoaPluginDB *)newPluginDBWithName:(cAppCharP)aName
  deviceKey:(cAppCharP)aDeviceKey
  userKey:(cAppCharP)aUserKey
  andCB:(DB_Callback)aCB
{
  // subclasses must return a subclass of CocoaPluginDB
  // this base class cannot return a db plugin
  return nil;
}


@end // CocoaPluginModule

#endif // not defined DB_PLUGIN_NAMESPACE

// Entry points
// ------------

#ifndef PLUGIN_BASECLASS
PLUGIN_NS_BEGIN

// create the module context
TSyError Module_CreateContext(
  CContext *aContext,
  cAppCharP aModuleName,
  cAppCharP aSubName,
  cAppCharP aContextName,
  DB_Callback aCB
) {
  // call factory C function to return suitable ObjC wrapper object
  // (which must be a derivate of CocoaPluginModule)
  // Note: for statically linked plugins, the factory method must exist in the plugin's namespace
  CocoaPluginModule *module = PLUGIN_NS_USE(newPluginModule)(aModuleName,aSubName,aContextName,aCB);
  if (module==nil) return DB_Full; // cannot allocate module context
  // return the object pointer as module context
  *aContext= (CContext)module;
  return LOCERR_OK;
} // Module_CreateContext


// Get the plug-in's version number
CVersion Module_Version(CContext mContext)
{
  // This must work with mContext=0
  // (therefore, buildNumber is a class method!)
  return Plugin_Version([CocoaPluginModule buildNumber]);
} // Module_Version



// Get the plug-in's capabilities
TSyError Module_Capabilities(CContext mContext, char** mCapabilities)
{
  CocoaPluginModule *module = (CocoaPluginModule *)mContext;
  char s[512]; // create local string
  sprintf(
    s,
    CA_Platform ":%s\n" // platform
    DLL_Info "\n" // DLL_Info
    CA_MinVersion ":1.4.0.0\n" // Min version (we need >=1.4.x because of AsKey)
    CA_Manufacturer ":%s\n" // Manufacturer
    CA_Description ":%s\n" // Module description
    CA_ItemAsKey ":true\n" // Note: cocoa based plugins ALWAYS use AsKey mechanism
    CA_DeleteSyncSet ":%s\n" // Only return yes if the implementation is non-dummy! Just having the routine is not enough!
    Plugin_Session ":no\n" // no session routines
    Plugin_DS_Admin ":no\n" // no datastore admin
    Plugin_DS_Data_Str ":no\n" // no datastore string routines (i.e. only AsKey)
    Plugin_DS_Adapt ":no\n", // no datastore adapt
    // the variable parts in the above
    MyPlatform(),
    [module manufacturerName],
    [module moduleName],
    [module hasDeleteSyncSet] ? "yes" : "no"
  );
  // create return string
  *mCapabilities= StrAlloc( s );
  return LOCERR_OK;
} // Module_Capabilities


// provide plugin parameters
TSyError Module_PluginParams(CContext mContext, cAppCharP mConfigParams, CVersion engineVersion)
{
  CocoaPluginModule *module = (CocoaPluginModule *)mContext;
  return [module pluginParams:mConfigParams fromEngineVersion:engineVersion];
} // Module_PluginParams



// Dispose memory created in the module context and passed back to engine
void Module_DisposeObj(CContext mContext, void* memory)
{
  StrDispose(memory);
} // Module_DisposeObj


// Delete module context
TSyError Module_DeleteContext(CContext mContext)
{
  CocoaPluginModule *module = (CocoaPluginModule *)mContext;
  // context ends, release the ObjC wrapper object
  [module release];
  return LOCERR_OK;
} // Module_DeleteContext



// SESSION CONTEXT (dummy, no wrapper implemented)

// Create a context for a new session
TSyError Session_CreateContext(CContext *sContext, cAppCharP sessionName, DB_Callback sCB)
{
  *sContext = 0; // Dummy context
  return LOCERR_OK;
} // Session_CreateContext


/*
TSyError Session_CheckDevice(
  long sContext, cAppCharP aDeviceID,
  char** sDevKey, char** nonce
) {
  // session context not implemented
  return LOCERR_NOTIMP;
} // Session_CheckDevice


TSyError Session_GetNonce(long sContext, char** nonce)
{
  // session context not implemented
  return LOCERR_NOTIMP;
} // Session_GetNonce


TSyError Session_SaveNonce(long sContext, cAppCharP nonce)
{
  // session context not implemented
  return LOCERR_NOTIMP;
} // Session_SaveNonce


TSyError Session_SaveDeviceInfo(long sContext, cAppCharP aDeviceInfo)
{
  // session context not implemented
  return LOCERR_NOTIMP;
} // Session_SaveDeviceInfo


// Get the plugin's DB time
TSyError Session_GetDBTime(long sContext, char** currentDBTime)
{
  // plugin does not have separate time
  *currentDBTime= NULL;
  return DB_NotFound;
} // Session_GetDBTime


int Session_PasswordMode(long sContext)
{
  // dummy; session context not implemented
  return Password_MD5_Nonce_IN;
} // Session_PasswordMode


TSyError Session_Login(
  long sContext, cAppCharP sUsername,
  char** sPassword, char** sUsrKey
) {
  // session context not implemented
  return LOCERR_NOTIMP;
} // Session_Login


TSyError Session_Logout(long sContext)
{
  // session context not implemented
  return LOCERR_NOTIMP;
} // Session_Logout


void Session_DisposeObj(long sContext, void* memory)
{
  // Should never be called (we have no session!), but if, it's a string to dispose
  StrDispose(memory);
} // Session_DisposeObj


void Session_ThreadMayChangeNow(long sContext)
{
  // NOP
} // Session_ThreadMayChangeNow


void Session_DispItems(long sContext, bool allFields, cAppCharP specificItem)
{
  // NOP
} // Session_DispItems
*/


// Delete a session context
TSyError Session_DeleteContext(CContext sContext)
{
  // NOP because we don't have a session context
  return LOCERR_OK;
} // Session_DeleteContext

PLUGIN_NS_END
#endif // not PLUGIN_BASECLASS


#ifndef DB_PLUGIN_NAMESPACE

// DATABASE CONTEXT
// ================


@implementation CocoaPluginDB

- (id)initWithName:(cAppCharP)aName
  inModule:(CocoaPluginModule *)aModule
  deviceKey:(cAppCharP)aDeviceKey
  userKey:(cAppCharP)aUserKey
  andCB:(DB_Callback)aCB
{
  if ([super init]!=nil) {
    // save the name strings
    fContextName = [NSString stringWithCString:aName encoding:NSUTF8StringEncoding];
    fDeviceKey = [NSString stringWithCString:aDeviceKey encoding:NSUTF8StringEncoding];
    fUserKey = [NSString stringWithCString:aUserKey encoding:NSUTF8StringEncoding];
    // save the callback structure
    fCB = aCB;
    // save the link to the module wrapper object
    fModule = [aModule retain]; // keep it as long as I live myself
  }
  return self;
}


- (void)dealloc
{
  // release the module
  [fModule release];
  // done
  [super dealloc];
}


// access to CB
- (DB_Callback)getCB
{
  return fCB;
}


// Debug routines for plugin to send messages into SyncML engine log

- (void)debugOut:(NSString *)aMessage
{
  DB_DebugPuts_Func DB_DebugPuts = fCB->DB_DebugPuts;
  if (!DB_DebugPuts) return; // not implemented, no output
  DB_DebugPuts(fCB->callbackRef, [aMessage cStringUsingEncoding:NSUTF8StringEncoding]);
}


- (void)debugOutExotic:(NSString *)aMessage
{
  DB_DebugPuts_Func DB_DebugExotic = fCB->DB_DebugExotic;
  if (!DB_DebugExotic) return; // not implemented, no output
  DB_DebugExotic(fCB->callbackRef, [aMessage cStringUsingEncoding:NSUTF8StringEncoding]);
}


- (void)debugBlockWithTag:(NSString *)aTag andDescription:(NSString *)aDesc andAttrs:(NSString *)aAttrs
{
  DB_DebugBlock_Func DB_DebugBlock = fCB->DB_DebugBlock;
  if (!DB_DebugBlock) return; // not implemented, no output
  DB_DebugBlock(fCB->callbackRef,
    [aTag cStringUsingEncoding:NSUTF8StringEncoding],
    [aDesc cStringUsingEncoding:NSUTF8StringEncoding],
    [aAttrs cStringUsingEncoding:NSUTF8StringEncoding]
  );
}


- (void)debugEndBlockWithTag:(NSString *)aTag
{
  DB_DebugEndBlock_Func DB_DebugEndBlock = fCB->DB_DebugEndBlock;
  if (!DB_DebugEndBlock) return; // not implemented, no output
  DB_DebugEndBlock(fCB->callbackRef,[aTag cStringUsingEncoding:NSUTF8StringEncoding]);
}


- (void)debugEndThread
{
  DB_DebugEndThread_Func DB_DebugEndThread = fCB->DB_DebugEndThread;
  if (!DB_DebugEndThread) return; // not implemented, no output
  DB_DebugEndThread(fCB->callbackRef);
}


// open a session key
- (SettingsKey *)newOpenSessionKeyWithMode:(uInt16)aMode err:(TSyError *)aErrP
{
  TSyError sta=LOCERR_OK;
  KeyH newKeyH=NULL;
  SettingsKey *newKey=nil; // no object by default
  OpenSessionKey_Func OpenSessionKey = fCB->ui.OpenSessionKey;
  if (!OpenSessionKey) {
    sta=LOCERR_NOTIMP;
  }
  else {
    // get session key of current session (implicit for DB plugins)
    sta = OpenSessionKey(fCB,NULL,&newKeyH,aMode);
    if (sta==LOCERR_OK) {
      newKey=[[SettingsKey alloc] initWithCI:fCB andKeyHandle:newKeyH];
    }
  }
  if (aErrP) *aErrP=sta;
  return newKey;
}


// Dummy implementation - should be overridden in actual DB implementations in subclass

- (sInt32)contextSupportRules:(cAppCharP)aContextRules
{
  // none supported
  return 0;
}


- (sInt32)filterSupportRules:(cAppCharP)aFilterRules
{
  // none supported
  return 0;
}


- (void)threadMayChangeNow
{
  // NOP
}


- (TSyError)startDataReadWithLastToken:(cAppCharP)aLastToken andResumeToken:(cAppCharP)aResumeToken
{
  return LOCERR_NOTIMP;
}


- (TSyError)readNextItemAsKey:(SettingsKey *)aItemKey
  itemIdP:(NSString **)aItemIdP parentIdP:(NSString **)aParentIdP
  statusP:(sInt32 *)aStatusP
  isFirst:(BOOL)aFirst
{
  // empty data set
  *aStatusP = ReadNextItem_EOF;
  return LOCERR_OK;
}


- (TSyError)readItemAsKey:(SettingsKey *)aItemKey
  itemID:(NSString *)aItemID parentID:(NSString *)aParentID
{
  return DB_NotFound;
}


- (TSyError)endDataRead
{
  return LOCERR_OK;
}


- (TSyError)startDataWrite
{
  return LOCERR_OK;
}


- (TSyError)insertItemAsKey:(SettingsKey *)aItemKey
  parentID:(NSString *)aParentID newItemIdP:(NSString **)aNewItemIdP
{
  return DB_Fatal;
}


- (TSyError)finalizeLocalID:(NSString **)aLocIDP
{
  return DB_NoContent; // no finalisation
}


- (TSyError)updateItemAsKey:(SettingsKey *)aItemKey
  itemIdP:(NSString **)aItemIdP parentIdP:(NSString **)aParentIdP
{
  return DB_NotFound;
}


- (TSyError)moveItem:(NSString *)aItemID
  fromParentID:(NSString *)aParentID toNewParent:(NSString *)aNewParentID
{
  return LOCERR_NOTIMP;
}


- (TSyError)deleteItem:(NSString *)aItemID parentID:(NSString *)aParentID
{
  return DB_NotFound;
}


- (TSyError)deleteSyncSet
{
  return LOCERR_NOTIMP; // means: deleting items not implemented (but may still do some work, like erasing all address book groups)
}


- (TSyError)endDataWriteWithSuccess:(BOOL)aSuccess andNewToken:(NSString **)aNewTokenP
{
  return LOCERR_OK;
}


@end // CocoaPluginDB

#endif // not defined DB_PLUGIN_NAMESPACE


// Entry points
// ------------

#ifndef PLUGIN_BASECLASS
PLUGIN_NS_BEGIN

// Create a database context
TSyError CreateContext(
  CContext* aContext,
  cAppCharP aContextName,
  DB_Callback aCB,
  cAppCharP sDevKey,
  cAppCharP sUsrKey
)
{
  // retrieve the module context from aCB
  if (!aCB) return DB_Fatal;
  CocoaPluginModule *module = (CocoaPluginModule *)aCB->mContext;
  if (module==nil) return DB_Fatal;
  // let module context create the DB context
  CocoaPluginDB *db = [module
    newPluginDBWithName:aContextName
    deviceKey:sDevKey
    userKey:sUsrKey
    andCB:aCB
  ];
  if (db==nil) return DB_Full;
  // return the object pointer as context
  *aContext= (CContext)db;
  return LOCERR_OK;
} // CreateContext



void DispItems(CContext aContext, bool allFields, cAppCharP specificItem)
{
  // NOP
} // DispItems



uInt32 ContextSupport(CContext aContext, cAppCharP aContextRules)
{
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  return [db contextSupportRules:aContextRules];
} // ContextSupport



uInt32 FilterSupport(CContext aContext, cAppCharP aFilterRules)
{
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  return [db filterSupportRules:aFilterRules];
} // FilterSupport



// Dispose memory created in the DB context and passed back to engine
void DisposeObj(CContext aContext, void* memory)
{
  StrDispose(memory);
} // DisposeObj


/*
TSyError LoadAdminData(
  CContext aContext, cAppCharP  aLocalDB,
  cAppCharP aRemoteDB, const char** adminData
) {
  // DB admin not implemeted
  return LOCERR_NOTIMP;
} // LoadAdminData



TSyError SaveAdminData(CContext aContext, cAppCharP adminData)
{
  // DB admin not implemeted
  return LOCERR_NOTIMP;
} // SaveAdminData



bool ReadNextMapItem(CContext aContext, MapID mID, bool aFirst)
{
  // DB admin not implemeted
  return false;
} // ReadNextMapItem



TSyError InsertMapItem(CContext aContext, MapID mID)
{
  // DB admin not implemeted
  return LOCERR_NOTIMP;
} // InsertMapItem



TSyError UpdateMapItem(CContext aContext, MapID mID)
{
  // DB admin not implemeted
  return LOCERR_NOTIMP;
} // UpdateMapItem



TSyError DeleteMapItem(CContext aContext, MapID mID)
{
  // DB admin not implemeted
  return LOCERR_NOTIMP;
} // DeleteMapItem
*/



void ThreadMayChangeNow(CContext aContext)
{
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  return [db threadMayChangeNow];
} // ThreadMayChangeNow



void WriteLogData(CContext aContext, cAppCharP logData)
{
  // not implemented
} // WriteLogData


/*
TSyError AdaptItem(
  CContext aContext, char** aItemData1, char** aItemData2,
  char** aLocalVars, int aIdentifier
) {
  return LOCERR_NOTIMP;
} // AdaptItem
*/


TSyError StartDataRead(
  CContext aContext,
  cAppCharP lastToken,
  cAppCharP resumeToken
) {
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  return [db startDataReadWithLastToken:lastToken andResumeToken:resumeToken];
} // StartDataRead


/*
TSyError ReadNextItem( CContext aContext, ItemID aID, char** aItemData, int* aStatus, bool aFirst )
{
  // Only XXXAsKey variants are supported
  return LOCERR_NOTIMP;
} // ReadNextItem
*/


TSyError ReadNextItemAsKey(
  CContext aContext,
  ItemID aID,
  KeyH aItemKey,
  sInt32 *aStatus,
  bool aFirst
) {
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  // pool for strings used in this call
  NSAutoreleasePool *itemPool = [[NSAutoreleasePool alloc] init];
  // prepare strings
  NSString *itemID = nil;
  NSString *parentID = nil;
  // create wrapper for the item key
  SettingsKey *itemKey = [[SettingsKey alloc] initWithCI:[db getCB] andKeyHandle:aItemKey];
  // call the method
  TSyError sta = [db readNextItemAsKey:itemKey itemIdP:&itemID parentIdP:&parentID statusP:aStatus isFirst:aFirst];
  // detach the key wrapper from the handle (must NOT close it when disposing the wrapper now!)
  [itemKey detachFromKeyHandle];
  // release the wrapper
  [itemKey release];
  if (sta==LOCERR_OK && itemID) {
    // convert back IDs
    aID->item  = StrAlloc([itemID cStringUsingEncoding:NSUTF8StringEncoding]);
    if (parentID)
      aID->parent= StrAlloc([parentID cStringUsingEncoding:NSUTF8StringEncoding]);
  }
  // release the pool
  [itemPool release];
  // return the status
  return sta;
} // ReadNextItemAsKey


/*
TSyError ReadItem(CContext aContext, ItemID aID, char** aItemData)
{
  // Only XXXAsKey variants are supported
  return LOCERR_NOTIMP;
} // ReadItem
*/


TSyError ReadItemAsKey(
  CContext aContext,
  cItemID aID,
  KeyH aItemKey
)
{
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  // pool for strings used in this call
  NSAutoreleasePool *itemPool = [[NSAutoreleasePool alloc] init];
  // prepare strings
  NSString *itemID = [NSString stringWithCString:aID->item encoding:NSUTF8StringEncoding];
  NSString *parentID = nil;
  if (aID->parent)
    parentID = [NSString stringWithCString:aID->parent encoding:NSUTF8StringEncoding];
  // create wrapper for the item key
  SettingsKey *itemKey = [[SettingsKey alloc] initWithCI:[db getCB] andKeyHandle:aItemKey];
  // call the method
  TSyError sta = [db readItemAsKey:itemKey itemID:itemID parentID:parentID];
  // detach the key wrapper from the handle (must NOT close it when disposing the wrapper now!)
  [itemKey detachFromKeyHandle];
  // release the wrapper
  [itemKey release];
  // release the pool
  [itemPool release];
  // return the status
  return sta;
} // ReadItemAsKey


TSyError ReadBlob(
  CContext aContext, cItemID aID,
  cAppCharP aBlobID,
  void** aBlkPtr, unsigned long* aBlkSize, unsigned long* aTotSize,
  bool aFirst, bool* aLast
) {
  // Not yet implemeted
  return LOCERR_NOTIMP;
} // ReadBlob


TSyError EndDataRead(CContext aContext)
{
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  return [db endDataRead];
} // EndDataRead



TSyError StartDataWrite(CContext aContext)
{
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  return [db startDataWrite];
} // StartDataWrite


/*
TSyError InsertItem(CContext aContext, cAppCharP aItemData, ItemID newID)
{
  // Only XXXAsKey variants are supported
  return LOCERR_NOTIMP;
} // InsertItem
*/


TSyError InsertItemAsKey(CContext aContext, KeyH aItemKey, ItemID newID)
{
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  // pool for strings used in this call
  NSAutoreleasePool *itemPool = [[NSAutoreleasePool alloc] init];
  // prepare strings
  // - item ID will be returned
  NSString *newItemID = nil;
  // - parent ID might be set from caller
  NSString *parentID = nil;
  if (newID->parent)
    parentID = [NSString stringWithCString:newID->parent encoding:NSUTF8StringEncoding];
  // create wrapper for the item key
  SettingsKey *itemKey = [[SettingsKey alloc] initWithCI:[db getCB] andKeyHandle:aItemKey];
  // call the method
  TSyError sta = [db insertItemAsKey:itemKey parentID:parentID newItemIdP:&newItemID];
  // detach the key wrapper from the handle (must NOT close it when disposing the wrapper now!)
  [itemKey detachFromKeyHandle];
  // release the wrapper
  [itemKey release];
  // convert back new ID
  if (sta==LOCERR_OK && newItemID) {
    newID->item  = StrAlloc([newItemID cStringUsingEncoding:NSUTF8StringEncoding]);
  }
  // release the pool
  [itemPool release];
  // return the status
  return sta;
} // InsertItemAsKey


TSyError FinalizeLocalID(CContext aContext, cItemID aID, ItemID updID)
{
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  // pool for strings used in this call
  NSAutoreleasePool *itemPool = [[NSAutoreleasePool alloc] init];
  // prepare strings
  NSString *itemID = [NSString stringWithCString:aID->item encoding:NSUTF8StringEncoding];
  // call the method
  TSyError sta = [db finalizeLocalID:&itemID];
  // convert back new ID
  if (sta==LOCERR_OK && updID) {
    sta = DB_Error;
    if (itemID) {
      const char *s = [itemID cStringUsingEncoding:NSUTF8StringEncoding];
      if (s) {
        // actually converted ID back
        updID->item = StrAlloc([itemID cStringUsingEncoding:NSUTF8StringEncoding]);
        sta = LOCERR_OK;
      }
    }
  }
  // release the pool
  [itemPool release];
  // return the status
  return sta;
} // FinalizeLocalID



/*
TSyError UpdateItem(
  CContext aContext, cAppCharP aItemData,
  const ItemID   aID, ItemID updID
) {
  // Only XXXAsKey variants are supported
  return LOCERR_NOTIMP;
} // UpdateItem
*/


TSyError UpdateItemAsKey(CContext aContext, KeyH aItemKey, cItemID aID, ItemID updID)
{
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  // pool for strings used in this call
  NSAutoreleasePool *itemPool = [[NSAutoreleasePool alloc] init];
  // prepare strings
  NSString *itemID = [NSString stringWithCString:aID->item encoding:NSUTF8StringEncoding];
  NSString *parentID = nil;
  if (aID->parent)
    parentID = [NSString stringWithCString:aID->parent encoding:NSUTF8StringEncoding];
  // create wrapper for the item key
  SettingsKey *itemKey = [[SettingsKey alloc] initWithCI:[db getCB] andKeyHandle:aItemKey];
  // call the method
  TSyError sta = [db updateItemAsKey:itemKey itemIdP:&itemID parentIdP:&parentID];
  // detach the key wrapper from the handle (must NOT close it when disposing the wrapper now!)
  [itemKey detachFromKeyHandle];
  // release the wrapper
  [itemKey release];
  // convert back new ID
  if (sta==LOCERR_OK) {
    updID->item  = StrAlloc([itemID cStringUsingEncoding:NSUTF8StringEncoding]);
    if (parentID)
      updID->parent= StrAlloc([parentID cStringUsingEncoding:NSUTF8StringEncoding]);
  }
  // release the pool
  [itemPool release];
  // return the status
  return sta;
} // UpdateItemAsKey


TSyError MoveItem(CContext aContext, cItemID aID, cAppCharP newParentID)
{
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  // pool for strings used in this call
  NSAutoreleasePool *itemPool = [[NSAutoreleasePool alloc] init];
  // prepare strings
  NSString *itemID = [NSString stringWithCString:aID->item encoding:NSUTF8StringEncoding];
  NSString *parentID = [NSString stringWithCString:aID->parent encoding:NSUTF8StringEncoding];
  NSString *newParentIDStr = [NSString stringWithCString:newParentID encoding:NSUTF8StringEncoding];
  // call the method
  TSyError sta = [db moveItem:itemID fromParentID:parentID toNewParent:newParentIDStr];
  // release the pool
  [itemPool release];
  // return the status
  return sta;
} // MoveItem



TSyError DeleteSyncSet(CContext aContext)
{
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  // pool for strings used in this call
  NSAutoreleasePool *itemPool = [[NSAutoreleasePool alloc] init];
  // call the method
  TSyError sta = [db deleteSyncSet];
  // release the pool
  [itemPool release];
  // return the status
  return sta;
} // DeleteSyncSet



TSyError DeleteItem(CContext aContext, cItemID aID)
{
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  // pool for strings used in this call
  NSAutoreleasePool *itemPool = [[NSAutoreleasePool alloc] init];
  // prepare strings
  NSString *itemID = [NSString stringWithCString:aID->item encoding:NSUTF8StringEncoding];
  NSString *parentID = nil;
  if (aID->parent)
    parentID = [NSString stringWithCString:aID->parent encoding:NSUTF8StringEncoding];
  // call the method
  TSyError sta = [db deleteItem:itemID parentID:parentID];
  // release the pool
  [itemPool release];
  // return the status
  return sta;
} // DeleteItem


TSyError WriteBlob(
  CContext aContext, cItemID aID,  cAppCharP aBlobID,
  void* aBlkPtr, unsigned long aBlkSize,  unsigned long aTotSize,
  bool aFirst, bool aLast
) {
  // Not yet implemeted
  return LOCERR_NOTIMP;
} // WriteBlob


TSyError DeleteBlob(CContext aContext, cItemID aID, cAppCharP aBlobID)
{
  // Not yet implemeted
  return LOCERR_NOTIMP;
} // DeleteBlob



TSyError EndDataWrite(CContext aContext, bool success, char** newToken)
{
  CocoaPluginDB *db = (CocoaPluginDB *)aContext;
  // pool for strings used in this call
  NSAutoreleasePool *itemPool = [[NSAutoreleasePool alloc] init];
  // prepare token string
  NSString *newTokenString = nil;
  // call the method
  TSyError sta = [db endDataWriteWithSuccess:success andNewToken:&newTokenString];
  // convert back new token
  if (sta==LOCERR_OK && newTokenString && newToken) {
    *newToken = StrAlloc([newTokenString cStringUsingEncoding:NSUTF8StringEncoding]);
  }
  // release the pool
  [itemPool release];
  // return the status
  return sta;
} // EndDataWrite



// Delete DB context
TSyError DeleteContext(CContext mContext)
{
  CocoaPluginDB *db = (CocoaPluginDB *)mContext;
  // context ends, release the ObjC wrapper object
  [db release];
  return LOCERR_OK;
} // DeleteContext

PLUGIN_NS_END
#endif // not PLUGIN_BASECLASS


/* eof */
