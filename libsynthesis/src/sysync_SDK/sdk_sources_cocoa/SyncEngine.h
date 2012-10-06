/* SyncEngine */

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#include "syerror.h"
#include "sync_dbapidef.h"
#include "SettingsKey.h"


// wrapper for a sync session
@interface SyncSession : NSObject
{
  // the call-in structure
  UI_Call_In fCI;
  struct TunnelWrapper fTW;
  // the session handle
  SessionH fSessionH;
}
- (id)initWithCI:(UI_Call_In)aCI andSessionHandle:(SessionH)aSessionH;
- (void)dealloc;
// access to session key
- (SettingsKey *)newOpenSessionKeyWithMode:(uInt16)aMode err:(TSyError *)aErrP;
// run the session
- (TSyError)sessionStepWithCmd:(uInt16 *)aStepCmdP andProgressInfo:(TEngineProgressInfo *)aInfoP;
// read and write the SyncML messages
- (TSyError)getSyncMLBufferForSend:(BOOL)aForSend buffer:(appPointer *)aBuffer bufSize:(memSize *)aBufSize;
- (TSyError)retSyncMLBufferForSend:(BOOL)aForSend processed:(memSize)aProcessed;
// writing to session log
- (void)debugLog:(NSString *)aMessage;
- (void)errorLog:(NSString *)aMessage;
// Tunnel DB API
- (TSyError)startDataReadWithLastToken:(cAppCharP)aLastToken andResumeToken:(cAppCharP)aResumeToken;
- (TSyError)readNextItemAsKey:(SettingsKey *)aItemKey
  itemIdP:(NSString **)aItemIdP parentIdP:(NSString **)aParentIdP
  statusP:(sInt32 *)aStatusP
  isFirst:(BOOL)aFirst;

- (TSyError)readNextItemAsText:(NSString **)aTextP
  itemIdP:(NSString **)aItemIdP parentIdP:(NSString **)aParentIdP
  statusP:(sInt32 *)aStatusP
  isFirst:(BOOL)aFirst;


- (TSyError)readItemAsKey:(SettingsKey *)aItemKey
  itemID:(NSString *)aItemID parentID:(NSString *)aParentID;

- (TSyError)readItemAsText:(NSString **)aTextP
  itemID:(NSString *)aItemID parentID:(NSString *)aParentID;


- (TSyError)endDataRead;
- (TSyError)startDataWrite;
- (TSyError)insertItemAsKey:(SettingsKey *)aItemKey
  parentID:(NSString *)aParentID newItemIdP:(NSString **)aNewItemIdP;
//%%%- (TSyError)finalizeLocalID:(NSString **)aLocIDP;
- (TSyError)updateItemAsKey:(SettingsKey *)aItemKey
  itemIdP:(NSString **)aItemIdP parentIdP:(NSString **)aParentIdP;
- (TSyError)moveItem:(NSString *)aItemID
  fromParentID:(NSString *)aParentID toNewParent:(NSString *)aNewParentID;
- (TSyError)deleteItem:(NSString *)aItemID parentID:(NSString *)aParentID;
//%%%- (TSyError)deleteSyncSet;
- (TSyError)endDataWriteWithSuccess:(BOOL)aSuccess andNewToken:(NSString **)aNewTokenP;
@end


// wrapper object for the SySync Engine
@interface SyncEngine : NSObject
{
  // the DLL interface
  void *fDLL;
  CVersion fEngineAPIVersion;
  // SySync interface
  UI_Call_In fCI; // call-in structure pointer
  // disconnect entry point
  DisconnectEngine_Func DisconnectEngine_Var;
}
// init and cleanup
- (id)initWithLibraryPath:(NSString *)aLibraryPath andDebugFlags:(uInt16)aDebugFlags;
- (id)initWithLibraryPath:(NSString *)aLibraryPath andDebugFlags:(uInt16)aDebugFlags andEntryPointPrefix:(NSString *)aEntryPointPrefix;
- (void)dealloc;
// Engine init
- (TSyError)setStringCharset:(uInt16)aCharSet andLineEnds:(uInt16)aLineEndMode isBigEndian:(bool)aBigEndian;
- (TSyError)initEngineFile:(NSString *)aPath;
// sessions
- (SyncSession *)newOpenSessionWithSelector:(uInt32)aSelector andSessionName:(cAppCharP)aSessionName err:(TSyError *)aErrP;
// Settings access
- (SettingsKey *)newOpenKeyByPath:(cAppCharP)aPath withMode:(uInt16)aMode err:(TSyError *)aErrP;

@end
