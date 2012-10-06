/**
 *  @File     stdlogicds.h
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TStdLogicDS
 *    Standard database logic implementation, suitable for most (currently all)
 *    actual DS implementations, but takes as few assumptions about datastore
 *    so for vastly different sync patterns, this could be replaced by differnt locic
 *
 *    Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-09-15 : luz : created from custdbdatastore
 */
/*
 */

#ifndef TStdLogicDS_H
#define TStdLogicDS_H

// includes
#include "sysync.h"
#include "syncappbase.h"
#include "localengineds.h"

#ifdef MULTI_THREAD_DATASTORE
#include "platform_thread.h"
#endif

#if defined(CLIENT_USES_SERVER_DB)
  #define USES_SERVER_DB 1
  #define HAS_SERVER_DB 1
#else
  #ifdef SERVER_SUPPORT
    #define USES_SERVER_DB 1
  #endif
  #define HAS_SERVER_DB IS_SERVER
#endif

using namespace sysync;

namespace sysync {

// container for TSyncItem pointers
typedef std::list<sysync::TSyncItem *> TSyncItemPContainer; // contains data items


/// @brief standard logic datastore
/// - only called directly by TLocalEngineDS via logicXXXX virtuals.
/// - to perform actual access to implementation, this class calls its (mostly abstract)
///   implXXXX virtuals.
class TStdLogicDS: public TLocalEngineDS
{
  typedef TLocalEngineDS inherited;
private:
  bool fWriteStarted; ///< set if write has started
  #ifdef SYSYNC_CLIENT
  bool fEoC; ///< end of changes
  #if defined(CLIENT_USES_SERVER_DB) && !defined(SYSYNC_SERVER)
  TSyncItemPContainer fItems; ///< list of data items, used to simulate maps in server DB
  #endif
  #endif
  #ifdef SYSYNC_SERVER
  TSyncItemPContainer fItems; ///< list of data items
  uInt32 fNumRefOnlyItems;
  #endif
  // startSync/threading privates
  bool fInitializing;
  bool fStartInit;
  bool fMultiThread; // copied flag from sessionConfig

protected:

  /// @name dsSavedAdmin administrative data (anchors, timestamps, maps) as saved or to-be-saved
  /// @Note These will be loaded and saved be derived classes
  /// @Note Some of these will be updated from resp. @ref dsCurrentAdmin members at distinct events (suspend, session end, etc.)
  /// @Note Some of these will be updated during the session, but in a way that does NOT affect the anchoring of current/last session
  //
  /// @{
  // - TStdLogicDS is timestamp-based, so we save timestamps of the previous session
  lineartime_t fPreviousSyncTime;   ///< time of previous sync (used to generate local anchor string representing previous sync)
  /// @}

  /// @name dsCurrentAdmin current session's admin data (anchors, timestamps, maps)
  /// @Note These will be copied to @ref dsSavedAdmin members ONLY when a session completes successfully/suspends.
  /// @Note Admin data is NEVER directly saved or loaded from these
  /// @Note Derivates will update some of these at dssta_adminready with current time/anchor values
  //
  /// @{
  // - TStdLogicDS is timestamp-based, so we get timestamp to anchor this session
  lineartime_t fCurrentSyncTime;    ///< anchoring timestamp of this, currently running sync.
  /// @}


private:
  /// internally reset for re-use without re-creation
  void InternalResetDataStore(void);
public:
  /// constructor
  TStdLogicDS(
    TLocalDSConfig *aDSConfigP,
    sysync::TSyncSession *aSessionP,
    const char *aName,
    long aCommonSyncCapMask=0);
  virtual ~TStdLogicDS();

public:
  /// @name dsProperty property and state querying methods
  /// @{
  /// check is datastore is completely started.
  /// @param[in] aWait if set, call will not return until either started state is reached
  ///   or cannot be reached within the maximally allowed request processing time left.
  virtual bool isStarted(bool aWait);
  /// @}

protected:
  /// @name dsXXXX (usually abstract) virtuals defining the interface to derived datastore classes (implementation, api)
  ///   These are usually designed such that they should always call inherited::dsXXX to let the entire chain
  ///   of ancestors see the calls
  /// @{
  //
  /// reset datastore to a re-usable, like new-created state.
  virtual void dsResetDataStore(void) { InternalResetDataStore(); inherited::dsResetDataStore(); };
  /// abort datastore (no reset yet, everything is just frozen as it is)
  virtual void dsAbortDatastoreSync(TSyError aStatusCode, bool aLocalProblem);
  /// inform logic of coming state change
  virtual localstatus dsBeforeStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState);
  /// inform logic of happened state change
  virtual localstatus dsAfterStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState);
  #ifdef SYSYNC_SERVER
  // - called when a item in the sync set changes its localID (due to local DB internals)
  //   Datastore must make sure that possibly cached items get updated
  virtual void dsLocalIdHasChanged(const char *aOldID, const char *aNewID);
  #endif
  /// @}

  /// @name logicXXXX methods defining the interface to TLocalEngineDS.
  ///   Only these will be called by TLocalEnginDS
  /// @Note some of these are virtuals ONLY for being derived by superdatastore, NEVER by locic or other derivates
  ///   We use the SUPERDS_VIRTUAL macro for these, which is empty in case we don't have superdatastores, then
  ///   these can be non-virtual.
  /// @{
  //
  /// called to make admin data ready
  /// - might be called several times (auth retries at beginning of session)
  virtual localstatus logicMakeAdminReady(cAppCharP aDataStoreURI, cAppCharP aRemoteDBID);
  /// called to have all non-yet-generated sync commands as "to-be-resumed"
  virtual void logicMarkOnlyUngeneratedForResume(void);
  /// called to mark an already generated (but probably not sent or not yet statused) item
  /// as "to-be-resumed", by localID or remoteID (latter only in server case).
  /// @note This must be repeatable without side effects, as server must mark/save suspend state
  ///       after every request (and not just at end of session)
  virtual void logicMarkItemForResume(cAppCharP aLocalID, cAppCharP aRemoteID, bool aUnSent);
  /// called to mark an already sent item as "to-be-resent", e.g. due to temporary
  /// error status conditions, by localID or remoteID (latter only in server case).
  virtual void logicMarkItemForResend(cAppCharP aLocalID, cAppCharP aRemoteID);
  /// save status information required to possibly perform a resume (as passed to datastore with
  /// markOnlyUngeneratedForResume(), markItemForResume() and markItemForResend())
  /// (or, in case the session is really complete, make sure that no resume state is left)
  /// @note Must also save tempGUIDs (for server) and pending/unconfirmed maps (for client)
  virtual localstatus logicSaveResumeMarks(void);

  /// called to process incoming item operation
  /// @note Method must take ownership of syncitemP in all cases
  virtual bool logicProcessRemoteItem(
    TSyncItem *syncitemP,
    TStatusCommand &aStatusCommand,
    bool &aVisibleInSyncset, ///< on entry: tells if resulting item SHOULD be visible; on exit: set if processed item remains visible in the sync set.
    string *aGUID=NULL ///< GUID is stored here if not NULL
  );
  /// called to read a specified item from the server DB (not restricted to set of conflicting items)
  virtual bool logicRetrieveItemByID(
    TSyncItem &aSyncItem, ///< item to be filled with data from server. Local or Remote ID must already be set
    TStatusCommand &aStatusCommand ///< status, must be set on error or non-200-status
  );

  /// @}



  /// @name implXXXX methods defining the interface to TStdLogicDS.
  ///   Only these will be called by TLocalEngineDS
  /// @Note some of these are virtuals ONLY for being derived by superdatastore, NEVER by locic or other derivates
  ///   We use the SUPERDS_VIRTUAL macro for these, which is empty in case we don't have superdatastores, then
  ///   these can be non-virtual.
  /// @{
  //
  /// save status information required to possibly perform a resume (as passed to datastore with
  /// markOnlyUngeneratedForResume() and markItemForResume())
  /// (or, in case the session is really complete, make sure that no resume state is left)
  /// @note Must also save tempGUIDs (for server) and pending/unconfirmed maps (for client)
  virtual localstatus implSaveResumeMarks(void) = 0;
  // - called to have all non-yet-generated sync commands as "to-be-resumed"
  virtual void implMarkOnlyUngeneratedForResume(void) = 0;
  /// called to mark an already generated (but probably not sent or not yet statused) item
  /// as "to-be-resumed", by localID or remoteID (latter only in server case).
  /// @note This must be repeatable without side effects, as server must mark/save suspend state
  ///       after every request (and not just at end of session)
  virtual void implMarkItemForResume(cAppCharP aLocalID, cAppCharP aRemoteID, bool aUnSent) = 0;
  /// called to mark an already sent item as "to-be-resent", e.g. due to temporary
  /// error status conditions, by localID or remoteID (latter only in server case).
  virtual void implMarkItemForResend(cAppCharP aLocalID, cAppCharP aRemoteID) = 0;
  /// sync login (into this database)
  /// @note might be called several times (auth retries at beginning of session)
  /// @note must update the following state variables
  /// - in TLocalEngineDS: fLastRemoteAnchor, fLastLocalAnchor, fResumeAlertCode, fFirstTimeSync
  ///   - for client: fPendingAddMaps
  ///   - for server: fTempGUIDMap
  /// - in TStdLogicDS: fPreviousSyncTime, fCurrentSyncTime
  /// - in derived classes: whatever else belongs to dsSavedAdmin and dsCurrentAdmin state
  virtual localstatus implMakeAdminReady(
    const char *aDeviceID,    ///< @param[in] remote device URI (device ID)
    const char *aDatabaseID,  ///< @param[in] database ID
    const char *aRemoteDBID   ///< @param[in] database ID of remote device
  ) = 0;
  /// start data read
  /// @note: fSlowSync and fRefreshOnly must be valid before calling this method
  virtual localstatus implStartDataRead() = 0;
  /// get item from DB
  virtual localstatus implGetItem(
    bool &aEof, ///< set if no more items
    bool &aChanged, ///< if set on entry, only changed items will be reported, otherwise all will be returned and aChanged denotes if items has changed or not
    TSyncItem* &aSyncItemP ///< will receive the item
  ) = 0;
  /// end of read
  virtual localstatus implEndDataRead(void) = 0;
  /// start of write
  virtual localstatus implStartDataWrite(void) = 0;
  /// Returns true when DB can track syncop changes (i.e. having the DB report
  /// items as added again when stdlogic filters have decided they fell out of the syncset,
  /// and has announced this to the DB using implReviewReadItem().
  virtual bool implTracksSyncopChanges(void) { return false; }; // derived DB class needs to confirm true if
  /// review reported entry (allows post-processing such as map deleting)
  /// MUST be called after implStartDataWrite, before any actual writing,
  /// for each item obtained in implGetItem
  virtual localstatus implReviewReadItem(
    TSyncItem &aItem         ///< the item
  ) = 0;
  #ifdef SYSYNC_SERVER
  /// called to set maps.
  /// @note aLocalID or aRemoteID can be NULL - which signifies deletion of a map entry
  /// @note that this might be needed for clients accessing a server-style database as well
  virtual localstatus implProcessMap(cAppCharP aRemoteID, cAppCharP aLocalID) = 0;
  #endif
  /// called to read a specified item from the server DB (not restricted to set of conflicting items)
  virtual bool implRetrieveItemByID(
    TSyncItem &aSyncItem, ///< item to be filled with data from server. Local or Remote ID must already be set
    TStatusCommand &aStatusCommand ///< status, must be set on error or non-200-status
  ) = 0;
  /// process item (according to operation: add/delete/replace/map)
  virtual bool implProcessItem(
    TSyncItem *aItemP,         ///< the item
    TStatusCommand &aStatusCommand
  ) = 0;
  /// save end of session state
  virtual localstatus implSaveEndOfSession(bool aUpdateAnchors) = 0;
  /// end write sequence
  virtual bool implEndDataWrite(void) = 0;
  /// @}


private:
  /// @name dsHelpers
  ///   internal, private helper methods

  #ifdef SYSYNC_CLIENT
  /// called by dsBeforeStateChange to dssta_dataaccessstarted to make sure datastore is ready for being accessed.
  virtual localstatus startDataAccessForClient(void);
  #endif
  #ifdef SYSYNC_SERVER
  /// called by dsBeforeStateChange to dssta_dataaccessstarted to make sure datastore is ready for being accessed.
  virtual localstatus startDataAccessForServer(void);
  #endif

  /// @}



protected:
  // - helper to merge database version of an item with the passed version of the same item
  TMultiFieldItem *mergeWithDatabaseVersion(TSyncItem *aSyncItemP, bool &aChangedDBVersion, bool &aChangedNewVersion);
  #ifdef SYSYNC_SERVER
  // - check if conflicting item already exist in list of items-to-be-sent-to-client
  virtual TSyncItem *getConflictingItemByRemoteID(TSyncItem *syncitemP);
  virtual TSyncItem *getConflictingItemByLocalID(TSyncItem *syncitemP);
  // - called to check if content-matching item from server exists
  virtual TSyncItem *getMatchingItem(TSyncItem *syncitemP, TEqualityMode aEqMode);
  // - called to prevent item to be sent to client in subsequent logicGenerateSyncCommandsAsServer()
  //   item in question should be an item that was returned by getConflictingItemByRemoteID() or getMatchingItem()
  virtual void dontSendItemAsServer(TSyncItem *syncitemP);
  // - called to have additional item sent to remote (DB takes ownership of item)
  virtual void SendItemAsServer(TSyncItem *aSyncitemP);
private:
  // - end map operation (rollback if not aDoCommit)
  virtual bool MapFinishAsServer(
    bool aDoCommit,                // if not set, entire map operation must be undone
    TStatusCommand &aStatusCommand // status, must be set on error or non-200-status
  );
  /// called to generate sync sub-commands as client for remote server
  /// @return true if now finished for this datastore
  virtual bool logicGenerateSyncCommandsAsServer(
    TSmlCommandPContainer &aNextMessageCommands,
    TSmlCommand * &aInterruptedCommandP,
    cAppCharP aLocalIDPrefix
  );
  /// called for servers when receiving map from client
  /// @note aLocalID or aRemoteID can be NULL - which signifies deletion of a map entry
  virtual localstatus logicProcessMap(cAppCharP aLocalID, cAppCharP aRemoteID);
  #endif // SYSYNC_SERVER

  #ifdef SYSYNC_CLIENT
  /// called to generate sync sub-commands as server for remote client
  /// @return true if now finished for this datastore
  virtual bool logicGenerateSyncCommandsAsClient(
    TSmlCommandPContainer &aNextMessageCommands,
    TSmlCommand * &aInterruptedCommandP,
    cAppCharP aLocalIDPrefix
  );
  #endif // SYSYNC_CLIENT

  // - determine if this is a first time sync situation
  virtual bool isFirstTimeSync(void) { return fFirstTimeSync; };

protected:
  // - called for SyncML 1.1 if remote wants number of changes.
  //   Must return -1 no NOC value can be returned
  //   NOTE: we implement it here only for server, as it is not really needed
  //   for clients normally - if it is needed, client's agent must provide
  //   it as CustDBDatastore has no own list it can use to count in client case.
  virtual sInt32 getNumberOfChanges(void);

public:
  // Simple custom DB access interface methods
  // - returns true if database implementation can only update all fields of a record at once
  virtual bool dsReplaceWritesAllDBFields(void) { return false; } // we assume DB is smart enough
  #ifdef OBJECT_FILTERING
  // - returns true if DB implementation can filter during database fetch
  //   (otherwise, fetched items must be filtered after being read from DB)
  virtual bool dsFilteredFetchesFromDB(bool aFilterChanged=false) { return false; } // assume unfiltered data from DB
  #endif

private:
  /// internal stdlogic: start writing if not already started
  localstatus startDataWrite(void);
  /// internal stdlogic: end writing if not already ended
  localstatus endDataWrite(void);
public:
  // - must be called before starting a thread. If returns false, starting a thread now
  //   is not allowed and must be postponed.
  virtual bool startingThread(void) { return true; };
  // - must be called when a thread's activity has ended
  //   BUT THE CALL MUST BE FROM THE ENDING THREAD, not the main thread!
  virtual void endingThread(void) {};
  // - should be called before doing DB accesses that might be locked (e.g. because another thread is using the DB resources)
  virtual bool dbAccessLocked(void) { return false; };
  // - Actual start sync actions in DB. If server supports threaded init, this will
  //   be called in a sub-thread's context
  localstatus performStartSync(void);
  #ifdef MULTI_THREAD_DATASTORE
    TStatusCommand fStartSyncStatus; // a thread-private status command to store status ocurring during threaded startSync()
  #endif
private:
  // - can be called to check if performStartSync() should be terminated
  bool shouldExitStartSync(void);
  #ifdef MULTI_THREAD_DATASTORE
  bool threadedStartSync(void);
  TThreadObject fStartSyncThread; // the wrapper object for the startSync thread
  #endif
}; // TStdLogicDS


} // namespace sysync

#endif  // TStdLogicDS_H

// eof
