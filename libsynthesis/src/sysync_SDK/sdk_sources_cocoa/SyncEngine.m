#import "SyncEngine.h"

#include <dlfcn.h>
#include "sdk_util.h" // for Plugin_Version()


// wrapper for a SyncSession
@implementation SyncSession

- (id)initWithCI:(UI_Call_In)aCI andSessionHandle:(SessionH)aSessionH;
{
  if ([super init]!=nil) {
    fCI = aCI;
    fSessionH = aSessionH;
    // also prepare tunnel wrapper
    fTW.tCB = fCI;
    fTW.tContext = fSessionH;
  }
  return self;
} // initWithCI


// dealloc implicitly closes session
- (void)dealloc
{
  // close key
  if (fCI) {
    CloseSession_Func CloseSession= fCI->ui.CloseSession;
    if (CloseSession) {
      CloseSession(fCI,fSessionH);
      fSessionH=NULL;
    }
  }
  // done
  [super dealloc];
} // dealloc


// open a new session key
- (SettingsKey *)newOpenSessionKeyWithMode:(uInt16)aMode err:(TSyError *)aErrP
{
  TSyError sta=LOCERR_OK;
  KeyH newKeyH=NULL;
  SettingsKey *newKey=nil; // no object by default
  OpenSessionKey_Func OpenSessionKey = fCI->ui.OpenSessionKey;
  if (!OpenSessionKey) {
    sta=LOCERR_NOTIMP;
  }
  else {
    sta = OpenSessionKey(fCI,fSessionH,&newKeyH,aMode);
    if (sta==LOCERR_OK) {
      newKey=[[SettingsKey alloc] initWithCI:fCI andKeyHandle:newKeyH];
    }
  }
  if (aErrP) *aErrP=sta;
  return newKey;
}


- (TSyError)sessionStepWithCmd:(uInt16 *)aStepCmdP andProgressInfo:(TEngineProgressInfo *)aInfoP
{
  SessionStep_Func SessionStep = fCI->ui.SessionStep;
  if (!SessionStep) return LOCERR_NOTIMP;
  return SessionStep(fCI,fSessionH,aStepCmdP,aInfoP);
}

- (TSyError)getSyncMLBufferForSend:(BOOL)aForSend buffer:(appPointer *)aBufferPP bufSize:(memSize *)aBufSizeP
{
  GetSyncMLBuffer_Func GetSyncMLBuffer = fCI->ui.GetSyncMLBuffer;
  if (!GetSyncMLBuffer) return LOCERR_NOTIMP;
  return GetSyncMLBuffer(fCI,fSessionH,aForSend,aBufferPP,aBufSizeP);
}


- (TSyError)retSyncMLBufferForSend:(BOOL)aForSend processed:(memSize)aProcessed;
{
  RetSyncMLBuffer_Func RetSyncMLBuffer = fCI->ui.RetSyncMLBuffer;
  if (!RetSyncMLBuffer) return LOCERR_NOTIMP;
  return RetSyncMLBuffer(fCI,fSessionH,aForSend,aProcessed);
}


// writing to session log
- (void)debugLog:(NSString *)aMessage
{
  TSyError sta; 
  SettingsKey *sessionKey = [self newOpenSessionKeyWithMode:0 err:&sta];
  if (sta==LOCERR_OK) {
    [sessionKey setStringValueByName:"debugMsg" toValue:aMessage];
    [sessionKey release];
  }
}


- (void)errorLog:(NSString *)aMessage
{
  TSyError sta; 
  SettingsKey *sessionKey = [self newOpenSessionKeyWithMode:0 err:&sta];
  if (sta==LOCERR_OK) {
    [sessionKey setStringValueByName:"errorMsg" toValue:aMessage];
    [sessionKey release];
  }
}




// Tunnel DB API

- (TSyError)startDataReadWithLastToken:(cAppCharP)aLastToken andResumeToken:(cAppCharP)aResumeToken
{
  SDR_Func StartDataRead = fCI->dt.StartDataRead;
  if (!StartDataRead) return LOCERR_NOTIMP;
  // call
  return StartDataRead((CContext)&fTW,aLastToken,aResumeToken);
}


- (TSyError)readNextItemAsKey:(SettingsKey *)aItemKey
  itemIdP:(NSString **)aItemIdP parentIdP:(NSString **)aParentIdP
  statusP:(sInt32 *)aStatusP
  isFirst:(BOOL)aFirst
{
  RdNItemKFunc ReadNextItemAsKey = fCI->dt.ReadNextItemAsKey;
  if (!ReadNextItemAsKey) return LOCERR_NOTIMP;
  DisposeProc DisposeObj = fCI->dt.DisposeObj;
  if (!DisposeObj) return LOCERR_NOTIMP;
  // check params
  if (!aItemIdP || !aStatusP) return LOCERR_WRONGUSAGE;
  // prepare item ID
  ItemID_Struct itemID;
  itemID.parent = NULL;
  itemID.item = NULL;
  *aItemIdP = nil;
  if (aParentIdP) *aParentIdP = nil;
  // call
  TSyError sta = ReadNextItemAsKey((CContext)&fTW,&itemID,[aItemKey keyH],aStatusP,aFirst);
  // retrieve result strings
  if (sta==LOCERR_OK) {
    // get strings
    if (itemID.item) *aItemIdP = [NSString stringWithUTF8String:itemID.item];
    if (itemID.parent && aParentIdP) *aParentIdP = [NSString stringWithUTF8String:itemID.parent];
  }
  // dispose strings (if not NULL)
  DisposeObj((CContext)&fTW,itemID.item);
  DisposeObj((CContext)&fTW,itemID.parent);
  return sta;
}


- (TSyError)readNextItemAsText:(NSString **)aTextP
  itemIdP:(NSString **)aItemIdP parentIdP:(NSString **)aParentIdP
  statusP:(sInt32 *)aStatusP
  isFirst:(BOOL)aFirst
{
  RdNItemSFunc ReadNextItem = fCI->dt.ReadNextItem;
  if (!ReadNextItem) return LOCERR_NOTIMP;
  DisposeProc DisposeObj = fCI->dt.DisposeObj;
  if (!DisposeObj) return LOCERR_NOTIMP;
  // check params
  if (!aItemIdP || !aStatusP) return LOCERR_WRONGUSAGE;
  // prepare item ID
  ItemID_Struct itemID;
  itemID.parent = NULL;
  itemID.item = NULL;
  *aItemIdP = nil;
  if (aParentIdP) *aParentIdP = nil;
  // call
  appCharP itemData = NULL;
  TSyError sta = ReadNextItem((CContext)&fTW,&itemID,&itemData,aStatusP,aFirst);
  // retrieve result strings
  if (sta==LOCERR_OK) {
    // get strings
    if (itemID.item) *aItemIdP = [NSString stringWithUTF8String:itemID.item];
    if (itemID.parent && aParentIdP) *aParentIdP = [NSString stringWithUTF8String:itemID.parent];
    if (itemData && aTextP) *aTextP = [NSString stringWithUTF8String:itemData];
  }
  // dispose strings (if not NULL)
  DisposeObj((CContext)&fTW,itemID.item);
  DisposeObj((CContext)&fTW,itemID.parent);
  DisposeObj((CContext)&fTW,itemData);
  return sta;
}





- (TSyError)readItemAsKey:(SettingsKey *)aItemKey
  itemID:(NSString *)aItemID parentID:(NSString *)aParentID
{
  Rd_ItemKFunc ReadItemAsKey = fCI->dt.ReadItemAsKey;
  if (!ReadItemAsKey) return LOCERR_NOTIMP;
  // prepare item ID
  ItemID_Struct itemID;
  itemID.parent = aParentID ? (appCharP)[aParentID UTF8String] : NULL;
  if (!aItemID) return LOCERR_WRONGUSAGE;
  itemID.item = (appCharP)[aItemID UTF8String];
  // call
  return ReadItemAsKey((CContext)&fTW,&itemID,[aItemKey keyH]);
}


- (TSyError)readItemAsText:(NSString **)aTextP
  itemID:(NSString *)aItemID parentID:(NSString *)aParentID
{
  Rd_ItemSFunc ReadItem = fCI->dt.ReadItem;
  if (!ReadItem) return LOCERR_NOTIMP;
  DisposeProc DisposeObj = fCI->dt.DisposeObj;
  if (!DisposeObj) return LOCERR_NOTIMP;
  // prepare item ID
  ItemID_Struct itemID;
  itemID.parent = aParentID ? (appCharP)[aParentID UTF8String] : NULL;
  if (!aItemID) return LOCERR_WRONGUSAGE;
  itemID.item = (appCharP)[aItemID UTF8String];
  // call
  appCharP itemData = NULL;
  TSyError sta = ReadItem((CContext)&fTW,&itemID,&itemData);
  if (sta==LOCERR_OK) {
    // get strings
    if (itemData && aTextP) *aTextP = [NSString stringWithUTF8String:itemData];
  }
  // dispose strings (if not NULL)
  DisposeObj((CContext)&fTW,itemData);
  return sta;
}




- (TSyError)endDataRead
{
  EDR_Func EndDataRead = fCI->dt.EndDataRead;
  if (!EndDataRead) return LOCERR_NOTIMP;
  // call
  return EndDataRead((CContext)&fTW);
}


- (TSyError)startDataWrite
{
  return LOCERR_NOTIMP;
}

- (TSyError)insertItemAsKey:(SettingsKey *)aItemKey
  parentID:(NSString *)aParentID newItemIdP:(NSString **)aNewItemIdP
{
  return LOCERR_NOTIMP;
}

//%%%- (TSyError)finalizeLocalID:(NSString **)aLocIDP;

- (TSyError)updateItemAsKey:(SettingsKey *)aItemKey
  itemIdP:(NSString **)aItemIdP parentIdP:(NSString **)aParentIdP
{
  return LOCERR_NOTIMP;
}

- (TSyError)moveItem:(NSString *)aItemID
  fromParentID:(NSString *)aParentID toNewParent:(NSString *)aNewParentID
{
  return LOCERR_NOTIMP;
}


- (TSyError)deleteItem:(NSString *)aItemID parentID:(NSString *)aParentID
{
  return LOCERR_NOTIMP;
}


//%%%- (TSyError)deleteSyncSet;

- (TSyError)endDataWriteWithSuccess:(BOOL)aSuccess andNewToken:(NSString **)aNewTokenP
{
  return LOCERR_NOTIMP;
}



@end // SyncSession



@implementation SyncEngine

- (id)initWithLibraryPath:(NSString *)aLibraryPath andDebugFlags:(uInt16)aDebugFlags andEntryPointPrefix:(NSString *)aEntryPointPrefix
{
  TSyError sta=DB_NotFound;

  // init members
  fDLL = NULL;
  DisconnectEngine_Var = NULL;

  // empty prefix if none specified
  if (aEntryPointPrefix==nil) aEntryPointPrefix = @"";
  // init superclass and DLL
  if (self == [super init]) {
    ConnectEngine_Func ConnectEngine_Var = NULL;
    #ifdef SYSYNC_ENGINE_STATIC
    // static: just directly use the entry point.
    // if prefix contains "srv", we want a server engine
    if ([aEntryPointPrefix rangeOfString:@"srv"].location!=NSNotFound) {
      // server
      ConnectEngine_Var = &SySync_srv_ConnectEngine;
      DisconnectEngine_Var = &SySync_srv_DisconnectEngine;
    }
    else {
      // client
      ConnectEngine_Var = &ConnectEngine;
      DisconnectEngine_Var = &DisconnectEngine;
    }
    #else
    // dynamic: open the DLL, query the entry points
    fDLL = dlopen([aLibraryPath UTF8String], RTLD_LAZY);
    if (fDLL==NULL) {
      // try with .dylib ending
      fDLL = dlopen([[aLibraryPath stringByAppendingString:@".dylib"] UTF8String], RTLD_LAZY);
    }
    if (fDLL!=NULL) {
      // found the DLL, now get connect entry point (if none specified, use default)
      ConnectEngine_Var = dlsym(fDLL,[[aEntryPointPrefix stringByAppendingString:@"ConnectEngine"] UTF8String]);
      // and also remember the disconnect entry point
      DisconnectEngine_Var = dlsym(fDLL,[[aEntryPointPrefix stringByAppendingString:@"DisconnectEngine"] UTF8String]);
    }
    else {
      DBGNSLOG(@"dlopen() failed: %s\n",dlerror());
    }
    #endif
    // connect to the library
    if (ConnectEngine_Var!=NULL) {
      // got the entry, call it
      sta = ConnectEngine_Var(&fCI, &fEngineAPIVersion, Plugin_Version(0), aDebugFlags);
      if (!fCI) sta=LOCERR_NOTIMP;
    }
    // now engine should be accessible
    if (sta==LOCERR_OK) {
      // prepare engine text mode
      [self setStringCharset:CHS_UTF8 andLineEnds:LEM_UNIX isBigEndian:true];

      // get and print version
      SettingsKey *engineInfo = [self newOpenKeyByPath:"/engineinfo" withMode:0 err:&sta];
      if (sta==LOCERR_OK) {
        DBGNSLOG(@"SySync engine version = '%@'\n", [engineInfo stringValueByName:"version"]);
        DBGNSLOG(@"SySync engine platform = '%@'\n", [engineInfo stringValueByName:"platform"]);
        DBGNSLOG(@"library product name = '%@'\n", [engineInfo stringValueByName:"name"]);
        DBGNSLOG(@"version comment = '%@'\n", [engineInfo stringValueByName:"comment"]);
        [engineInfo release];
      }
    }
  }
  else
    sta=LOCERR_OUTOFMEM; // probably...
  // kill or go
  if (sta!=LOCERR_OK) {
    [self dealloc];
    return nil;
  }
  // ok
  return self;
} // initWithLibraryPath



- (id)initWithLibraryPath:(NSString *)aLibraryPath andDebugFlags:(uInt16)aDebugFlags
{
  // using standard entry point for client library
  return [self initWithLibraryPath:aLibraryPath andDebugFlags:aDebugFlags andEntryPointPrefix:nil];
} // initWithLibraryPath



// Dealloc

- (void)dealloc
{
  // disconnect engine
  #ifdef SYSYNC_ENGINE_STATIC
  if (DisconnectEngine_Var) {
    DisconnectEngine_Var(fCI);
  }
  #else
  if (fDLL!=NULL) {
    // call disconnect
    if (DisconnectEngine_Var) {
      // call disconnect in engine
      DisconnectEngine_Var(fCI);
    }
    // unload the DLL
    dlclose(fDLL);
    fDLL=NULL;
  }
  #endif
  // no longer valid
  fCI = NULL;
  DisconnectEngine_Var = NULL;
  // done
  [super dealloc];
}



// General setup

- (TSyError)setStringCharset:(uInt16)aCharSet andLineEnds:(uInt16)aLineEndMode isBigEndian:(bool)aBigEndian
{
  SetStringMode_Func SetStringMode = fCI->ui.SetStringMode;
  if (!SetStringMode) return LOCERR_NOTIMP;
  return SetStringMode(fCI,aCharSet,aLineEndMode,aBigEndian);
} // setStringCharset


// Init with config

- (TSyError)initEngineFile:(NSString *)aPath
{
  InitEngineFile_Func InitEngineFile = fCI->ui.InitEngineFile;
  if (!InitEngineFile) return LOCERR_NOTIMP;
  return InitEngineFile(fCI, [aPath UTF8String]);
} // initEngineFile


// Create a session

- (SyncSession *)newOpenSessionWithSelector:(uInt32)aSelector andSessionName:(cAppCharP)aSessionName err:(TSyError *)aErrP
{
  TSyError sta=LOCERR_OK;
  SessionH newSessionH=NULL;
  SyncSession *newSession=nil; // no object by default
  OpenSession_Func OpenSession = fCI->ui.OpenSession;
  if (!OpenSession) {
    sta=LOCERR_NOTIMP;
  }
  else {
    sta = OpenSession(fCI,&newSessionH,aSelector,aSessionName);
    if (sta==LOCERR_OK) {
      newSession=[[SyncSession alloc] initWithCI:fCI andSessionHandle:newSessionH];
    }
  }
  if (aErrP) *aErrP=sta;
  return newSession;
}



// Settings access

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
    sta = OpenKeyByPath(fCI,&newKeyH,NULL,aPath,aMode);
    if (sta==LOCERR_OK) {
      newKey=[[SettingsKey alloc] initWithCI:fCI andKeyHandle:newKeyH];
      DBGNSLOG(@"Opened settings key %s : SettingsKey object = 0x%lX",aPath,(intptr_t)newKey);
    }
  }
  if (aErrP) *aErrP=sta;
  return newKey;
} // newOpenKeyByPath



@end // SyncEngine