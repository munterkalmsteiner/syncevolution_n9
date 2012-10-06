/**
 *  @File     stdlogicds.cpp
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

// includes
#include "prefix_file.h"

#include "sysync.h"
#include "stdlogicds.h"
#include "multifielditem.h"
#include "multifielditemtype.h"

using namespace sysync;

namespace sysync {

/*
 * Implementation of TStdLogicDS
 */

/* public TStdLogicDS members */


TStdLogicDS::TStdLogicDS(
  TLocalDSConfig *aDSConfigP,
  sysync::TSyncSession *aSessionP,
  const char *aName,
  long aCommonSyncCapMask
) :
  TLocalEngineDS(aDSConfigP, aSessionP, aName, aCommonSyncCapMask)
  #ifdef MULTI_THREAD_DATASTORE
  ,fStartSyncStatus(aSessionP) // a thread-private status command to store status ocurring during threaded startDataAccessForServer()
  #endif
{
  InternalResetDataStore();
  fMultiThread= fSessionP->getSessionConfig()->fMultiThread; // get comfort pointer
} // TStdLogicDS::TStdLogicDS


TStdLogicDS::~TStdLogicDS()
{
  InternalResetDataStore();
} // TStdLogicDS::~TStdLogicDS


void TStdLogicDS::InternalResetDataStore(void)
{
  // reset
  fFirstTimeSync=false;
  fWriteStarted=false;
  fPreviousSyncTime=0;
  fCurrentSyncTime=0;

  #ifdef MULTI_THREAD_DATASTORE // combined ifdef/flag
  if (fMultiThread) {
    // make sure background processing aborts if it is in progress
    fStartSyncThread.terminate(); // request soft termination of thread
    if (!fStartSyncThread.waitfor()) {
      // has not already terminated
      PDEBUGPRINTFX(DBG_HOT,("******** Waiting for background thread to terminate"));
      fStartSyncThread.waitfor(-1); // wait forever or until thread really terminates
      PDEBUGPRINTFX(DBG_HOT,("******** Background thread terminated"));
    } // if
  } // if
  #endif
  // we are not initializing
  fInitializing=false;
  // no start init request yet
  fStartInit=false;
  if(HAS_SERVER_DB) {
    #ifdef USES_SERVER_DB
    // remove all items
    TSyncItemPContainer::iterator pos;
    for (pos=fItems.begin(); pos!=fItems.end(); ++pos) {
      delete *pos;
    }
    fItems.clear(); // clear list
    #endif
  }
  if (IS_SERVER) {
    #ifdef SYSYNC_SERVER
    fNumRefOnlyItems=0;
    #endif
  }
} // TStdLogicDS::InternalResetDataStore


// Internal events during sync to access local database
// ====================================================


// called to make admin data ready
localstatus TStdLogicDS::logicMakeAdminReady(cAppCharP aDataStoreURI, cAppCharP aRemoteDBID)
{
  PDEBUGBLOCKFMTCOLL(("MakeAdminReady","Making Admin Data ready to check sync anchors","localDB=%s|remoteDB=%s",aDataStoreURI,aRemoteDBID));
  // init local anchor strings, because it will not be set on impl level
  // (impl level only uses timestamps for local anchor)
  // These will be derived from timestamps below when implMakeAdminReady() is successful
  fLastLocalAnchor.erase();
  fNextLocalAnchor.erase();
  // Updates the following state variables
  // - from TLocalEngineDS: fLastRemoteAnchor, fLastLocalAnchor, fResumeAlertCode, fFirstTimeSync
  //   - for client: fPendingAddMaps
  //   - for server: fTempGUIDMap
  // - from TStdLogicDS: fPreviousSyncTime, fCurrentSyncTime
  // - from derived classes: whatever else belongs to dsSavedAdmin and dsCurrentAdmin state
  localstatus sta = implMakeAdminReady(
    fSessionP->getRemoteURI(),          // remote device/server URI (device ID)
    aDataStoreURI,                      // entire relative URI of local datastore
    aRemoteDBID                         // database ID of remote device/server (=path as sent by remote)
  );
  if (sta==LOCERR_OK) {
    // TStdLogicDS requires implementation to store timestamps to make the local anchors
    // - regenerate last session's local anchor string from timestamp
    TimestampToISO8601Str(fLastLocalAnchor,fPreviousSyncTime,TCTX_UTC,false,false);
    // - create this session's local anchor string from timestamp
    TimestampToISO8601Str(fNextLocalAnchor,fCurrentSyncTime,TCTX_UTC,false,false);
    // - check if config has changed since last sync
    if (fFirstTimeSync || fPreviousSyncTime<=fSessionP->getRootConfig()->fConfigDate) {
      // remote should see our (probably changed) devInf
      PDEBUGPRINTFX(DBG_PROTO,("First time sync or config changed since last sync -> remote should see our devinf"));
      fSessionP->remoteMustSeeDevinf();
    }
    // empty saved anchors if first time sync (should be empty anyway, but...)
    if (fFirstTimeSync) {
      fLastLocalAnchor.empty();
      fLastRemoteAnchor.empty();
    }
  }
  PDEBUGENDBLOCK("MakeAdminReady");
  return sta;
} // TStdLogicDS::logicMakeAdminReady



// start writing if not already started
localstatus TStdLogicDS::startDataWrite()
{
  localstatus sta = LOCERR_OK;

  if (!fWriteStarted) {
    sta = implStartDataWrite();
    fWriteStarted = sta==LOCERR_OK; // must be here to prevent recursion as startDataWrite might be called implicitly below
    if (HAS_SERVER_DB) {
      #ifdef USES_SERVER_DB
      // server-type DB needs post-processing to update map entries (client and server case)
      TSyncItemPContainer::iterator pos;
      if (sta==LOCERR_OK) {
        // Now allow post-processing of all reported items
        for (pos=fItems.begin(); pos!=fItems.end(); ++pos) {
          localstatus sta2 = implReviewReadItem(**pos);
          if (sta2!=LOCERR_OK) sta = sta2;
        }
      }
      #endif
    }
    if (IS_CLIENT && HAS_SERVER_DB) {
      #ifdef USES_SERVER_DB
      /// @todo we don't need the items to remain that long at all - not even for CLIENT_USES_SERVER_DB case
      fItems.clear(); // empty list
      #endif
    }
  }
  DEBUGPRINTFX(DBG_DATA,("startDataWrite called, status=%hd", sta));
  return sta;
} // TStdLogicDS::startDataWrite


// end writing
localstatus TStdLogicDS::endDataWrite(void)
{
  localstatus sta=LOCERR_OK;

  DEBUGPRINTFX(DBG_DATA,(
    "endDataWrite called, write %s started",
    fWriteStarted ? "is" : "not"
  ));
  // if we commit a write, the session is ok, and we can clear the resume state
  if (fWriteStarted) {
    sta = implEndDataWrite();
    // ended now
    fWriteStarted=false;
  }
  return sta;
} // TStdLogicDS::endDataWrite


// - read specific item from database
//   Data and missing ID information is filled in from local database
bool TStdLogicDS::logicRetrieveItemByID(
  TSyncItem &aSyncItem, // item to be filled with data from server. Local or Remote ID must already be set
  TStatusCommand &aStatusCommand // status, must be set on error or non-200-status
)
{
  // simply call implementation
  return implRetrieveItemByID(aSyncItem,aStatusCommand);
} // TLocalEngineDS::logicRetrieveItemByID


/// check is datastore is completely started.
/// @param[in] aWait if set, call will not return until either started state is reached
///   or cannot be reached within the maximally allowed request processing time left.
bool TStdLogicDS::isStarted(bool aWait)
{
  #ifdef SYSYNC_SERVER
  if (IS_SERVER) {
    // only server has threaded datastores so far
    if (aWait && fInitializing) {
      localstatus sta = startDataAccessForServer();
      if (sta!=LOCERR_OK) {
        SYSYNC_THROW(TSyncException("startDataAccessForServer failed (when called from isStarted)", sta));
      }
    }
  }
  #endif
  // if initialisation could not be completed in the first startDataAccessForServer() call
  // we are not started.
  return !fInitializing && inherited::isStarted(aWait);
} // TStdLogicDS::isStarted


/// called to mark an already generated (but probably not sent or not yet statused) item
/// as "to-be-resumed", by localID or remoteID (latter only in server case).
/// @note This must be repeatable without side effects, as server must mark/save suspend state
///       after every request (and not just at end of session)
void TStdLogicDS::logicMarkItemForResume(cAppCharP aLocalID, cAppCharP aRemoteID, bool aUnSent)
{
  implMarkItemForResume(aLocalID, aRemoteID, aUnSent);
} // TStdLogicDS::logicMarkItemForResume


/// called to mark an already sent item as "to-be-resent", e.g. due to temporary
/// error status conditions, by localID or remoteID (latter only in server case).
void TStdLogicDS::logicMarkItemForResend(cAppCharP aLocalID, cAppCharP aRemoteID)
{
  implMarkItemForResend(aLocalID, aRemoteID);
} // TStdLogicDS::logicMarkItemForResend


/// save status information required to possibly perform a resume (as passed to datastore with
/// markOnlyUngeneratedForResume(), markItemForResume() and markItemForResend())
/// (or, in case the session is really complete, make sure that no resume state is left)
/// @note Must also save tempGUIDs (for server) and pending/unconfirmed maps (for client)
localstatus TStdLogicDS::logicSaveResumeMarks(void)
{
  PDEBUGBLOCKFMTCOLL(("SaveResumeMarks","let implementation save resume info","datastore=%s",getName()));
  localstatus sta = implSaveResumeMarks();
  PDEBUGENDBLOCK("SaveResumeMarks");
  return sta;
} // TStdLogicDS::logicSaveResumeMarks





// - called for SyncML 1.1 if remote wants number of changes.
//   Must return -1 if no NOC value can be returned
sInt32 TStdLogicDS::getNumberOfChanges(void)
{
  if (IS_SERVER) {
    #ifdef SYSYNC_SERVER
    // for server, number of changes is the number of items in the item list
    // minus those that are for reference only (in a slow sync resume)
    return fItems.size()-fNumRefOnlyItems;
    #endif
  }
  else {
    // for client, derived class must provide it, or we'll return the default here (=no NOC)
    // Note: for client-only builds, this methods does not exist in StdLogicDS and thus
    //       inherited is always used
    return inherited::getNumberOfChanges();
  }
} // TStdLogicDS::getNumberOfChanges


#ifdef SYSYNC_SERVER

// Server case
// ===========

// Actual start sync actions in DB. If server supports threaded init, this will
// be called in a sub-thread's context
localstatus TStdLogicDS::performStartSync(void)
{
  localstatus sta = LOCERR_OK;
  TP_DEFIDX(li);
  TP_SWITCH(li,fSessionP->fTPInfo,TP_database);
  sta = implStartDataRead();
  fNumRefOnlyItems=0;
  if (sta==LOCERR_OK) {
    // now get data from DB
    if (!isRefreshOnly() || (isSlowSync() && isResuming())) {
      // not only updating from client, so read all items now
      // Note: for a resumed slow updating from client only, we need the
      //   currently present syncset as well as we need it to detect
      //   re-sent items
      bool eof;
      bool changed;
      PDEBUGBLOCKFMTCOLL(("GetItems","Read items from DB implementation","datastore=%s",getName()));
      do {
        // check if external request to terminate loop
        if (shouldExitStartSync()) {
          PDEBUGPRINTFX(DBG_ERROR,("performStartSync aborted by external request"));
          PDEBUGENDBLOCK("GetItems");
          TP_START(fSessionP->fTPInfo,li);
          return 510;
        }
        // try to read item
        TSyncItem *myitemP=NULL;
        // report all items in syncset, not only changes if we need to filter
        changed=!fFilteringNeededForAll; // let GetItem
        // now fetch next item
        sta = implGetItem(eof,changed,myitemP);
        if (sta!=LOCERR_OK) {
          implEndDataRead(); // terminate reading
          PDEBUGENDBLOCK("GetItems");
          TP_START(fSessionP->fTPInfo,li);
          return sta;
        }
        else {
          // read successful, test for eof
          if (eof) break; // reading done
          if (fSlowSync) changed=true; // all have changed (just in case GetItem does not return clean result here)
          // NOTE: sop can be sop_reference_only ONLY in case of server resuming a slowsync
          TSyncOperation sop=myitemP->getSyncOp();
          // check if we need to do some filtering to determine final syncop
          // NOTE: call postFetchFiltering even in case we do not actually
          //       need filtering (but we might need making item pass acceptance filter!)
          if (sop!=sop_delete && sop!=sop_soft_delete && sop!=sop_archive_delete) {
            // we need to post-fetch filter the item first
            bool passes=postFetchFiltering(myitemP);
            if (!passes) {
              // item was changed and does not pass now -> might be fallen out of the sync set now.
              // Only when the DB is capable of tracking items fallen out of the sync set (i.e. bring them up as adds
              // later should they match the filter criteria again), we can implement removing based
              // on filter criteria. Otherwise, these are simply ignored.
              if (implTracksSyncopChanges() && !fSlowSync && (sop==sop_wants_replace)) {
                // item already exists on remote but falls out of syncset now: delete
                // NOTE: This works only if reviewReadItem() is correctly implemented
                //       and checks for items that are deleted after being reported
                //       as replace to delete their local map entry (which makes
                //       them add candidates again)
                sop=sop_delete;
                myitemP->cleardata(); // also get rid of unneeded data
              }
              else
                sop=sop_none; // ignore all others (especially adds or slowsync replaces)
            }
            else {
              // item passes = belongs to sync set
              if (sop==sop_wants_replace && !changed && !fSlowSync) {
                // exists but has not changed since last sync
                sop=sop_none; // ignore for now
              }
            }
          }
          // check if we should use that item
          if (sop==sop_none) {
            delete myitemP;
            continue; // try next from DB
          }
          // set final sop now
          myitemP->setSyncOp(sop);
          // %%% these are just-in-case tests for sloppy db interface
          // - adjust operation for slowsync
          if (fSlowSync) {
            if (sop==sop_delete || sop==sop_soft_delete || sop==sop_archive_delete) {
              // do not process deleted items during slow sync at all
              delete myitemP; // forget it
              continue; // Read next item
            }
            else {
              // must be add or replace, will be an add by default (if unmatched)
              // - set it to sop_wants_add to signal that this item was not matched yet!
              myitemP->setRemoteID(""); // forget remote ID, is unknown in slow sync anyway
              if (sop!=sop_reference_only) // if reference only (resumed slowsync), keep it as is
                myitemP->setSyncOp(sop_wants_add); // flag it unmatched
            }
          }
          // - now add it to my local list
          fItems.push_back(myitemP);
          if (sop==sop_reference_only)
            fNumRefOnlyItems++; // count these to avoid them being shown in NOC
        }
      } while (true); // exit by break
      PDEBUGENDBLOCK("GetItems");
    } // not from client only
    // end reading
    sta=implEndDataRead();
    // show items
    PDEBUGPRINTFX(DBG_HOT,("%s: number of local items involved in %ssync = %ld",getName(), fSlowSync ? "slow " : "",(long)fItems.size()));
    CONSOLEPRINTF(("  %ld local items are new/changed/deleted for this sync",(long)fItems.size()));
    if (PDEBUGTEST(DBG_DATA+DBG_DETAILS)) {
      PDEBUGBLOCKFMTCOLL(("SyncSet","Items involved in Sync","datastore=%s",getName()));
      for (TSyncItemPContainer::iterator pos=fItems.begin();pos!=fItems.end();pos++) {
        TSyncItem *syncitemP = (*pos);
        PDEBUGPRINTFX(DBG_DATA,(
          "SyncOp=%-20s: LocalID=%15s RemoteID=%15s",
          SyncOpNames[syncitemP->getSyncOp()],
          syncitemP->getLocalID(),
          syncitemP->getRemoteID()
        ));
      }
      PDEBUGENDBLOCK("SyncSet");
    }
    // initiate writing and cause reviewing of items now,
    // before any of them can be modified by sync process
    // Notes:
    // - startDataWrite() includes zapping the sync set
    //   in slow refresh only sessions (not resumed, see next note)
    // - In case of resumed slow refresh from remote, the sync set
    //   will not get zapped again, because some items are already there
    if (sta==LOCERR_OK)
      sta = startDataWrite(); // private helper
    // test if ok
    if (sta != LOCERR_OK) {
      // failed
      engAbortDataStoreSync(sta,true);
      return sta;
    }
    TP_START(fSessionP->fTPInfo,li);
    return sta;
  } // startDataRead successful
  else {
    TP_START(fSessionP->fTPInfo,li);
    return sta;
  }
} // TStdLogicDS::performStartSync


#ifdef MULTI_THREAD_DATASTORE

// function executed by thread
static uInt32 StartSyncThreadFunc(TThreadObject *aThreadObject, uIntArch aParam)
{
  // parameter passed is pointer to datastore
  TStdLogicDS *datastoreP = static_cast<TStdLogicDS *>((void *)aParam);
  // now call routine that actually performs datastore start
  uInt32 exitCode = (uInt32) (datastoreP->performStartSync());
  // thread is about to end (has ended for the Impl and Api levels), inform datastore
  // and post necessary ThreadMayChangeNow() calls
  datastoreP->endingThread();
  // return
  return exitCode;
} // StartSyncThreadFunc


bool TStdLogicDS::threadedStartSync(void)
{
  // do this in a separate thread if requested
  // Note: ThreadMayChangeNow() has been posted already by startingThread()
  PDEBUGPRINTFX(DBG_HOT,("******* starting background thread for reading sync set..."));
  fStartSyncStatus.setStatusCode(200); // assume ok
  if (!fStartSyncThread.launch(StartSyncThreadFunc,(uIntArch)this)) { // pass datastoreP as param
    // starting thread failed
    PDEBUGPRINTFX(DBG_ERROR,("******* Failed starting background thread for reading sync set"));
    return false;
  }
  #ifdef SYDEBUG
  // show link to thread log
  PDEBUGPRINTFX(DBG_HOT,(
    "******* started &html;<a href=\"%s_%lu%s\" target=\"_blank\">&html;background thread id=%lu&html;</a>&html; for reading sync set",
    getDbgLogger()->getDebugFilename(), // href base
    fStartSyncThread.getid(), // plus thread
    getDbgLogger()->getDebugExt(), // plus extension
    fStartSyncThread.getid()
  ));
  #endif
  return true; // started ok
} // TStdLogicDS::threadedStartSync


// can be called to check if performStartSync() should be terminated
bool TStdLogicDS::shouldExitStartSync(void)
{
  // threaded version, check if termination flag was set
  return fMultiThread && fStartSyncThread.terminationRequested();
} // TStdLogicDS::shouldExitStartSync

#else // MULTI_THREAD_DATASTORE

// can be called to check if performStartSync() should be terminated
bool TStdLogicDS::shouldExitStartSync(void)
{
  // nonthread version, is never the case
  return false;
} // TStdLogicDS::shouldExitStartSync

#endif // MULTI_THREAD_DATASTORE


// called by dsBeforeStateChange to dssta_dataaccessstarted to make sure datastore is
// getting ready for being accessed. Also called by isStarted(true) when starting
// up took longer than one request max time for threaded datastores
localstatus TStdLogicDS::startDataAccessForServer(void)
{
  localstatus sta = LOCERR_OK;

  DEBUGPRINTFX(DBG_HOT,("TStdLogicDS::startDataAccessForServer"));
  if (!fInitializing) {
    // The datastore has not started initializing yet
    // - start initialisation now
    fWriteStarted=false;
    // - read all records from DB right now if server data is used at all
    DEBUGPRINTFX(DBG_DATA,("- number of items in list before StartDataRead = %ld",(long)fItems.size()));
    // now we can initialize the conflict resolution mode for this session
    /// @todo move this to localengineds, at point where we get dssta_syncmodestable
    fSessionConflictStrategy=getConflictStrategy(fSlowSync,fFirstTimeSync);
    // prepare for read
    #ifdef SCRIPT_SUPPORT
    // - call DB init script, which might add extra filters depending on options and remoterule
    TScriptContext::execute(
      fDataStoreScriptContextP,
      getDSConfig()->fDBInitScript,
      &DBFuncTable, // context's function table
      this // datastore pointer needed for context
    );
    // - datastoreinitscript might abort the sync with this datastore, check for that and exit if so
    if (isAborted()) {
      return getAbortStatusCode();
    }
    #endif
    // - init post fetch filtering, sets fFilteringNeededForAll and fFilteringNeeded correctly
    initPostFetchFiltering();
    // - now we can start reading (fFilteringNeededForAll can be checked by StartDataRead)
    fInitializing=true; // we enter the initialisation phase now
    fStartInit=true; // and we want to start the init
  }
  // try starting init now (possibly repeats until it can be done)
  if (fStartInit) {
    PDEBUGPRINTFX(DBG_DATA,( "MultiThread %sabled", fMultiThread ? "en":"dis" ));
    #ifdef MULTI_THREAD_DATASTORE // combined define and flag
    if (fMultiThread) {
      // start init thread
      if (startingThread()) {
        // we may start a thread here
        if (threadedStartSync())
          fStartInit= false; // starting done now
      } // if
    } // if
    #endif
    if (!fMultiThread) {
      // Just perform initialisation
      sta = performStartSync();
      fStartInit=false; // starting done now
      fInitializing=false; // initialisation is already complete here
    } // if
  }

  #ifdef MULTI_THREAD_DATASTORE // combined define and flag
  if (fMultiThread) {
    // wait for started initialisation to finish within time we have left for this request
    if (!fStartInit && fInitializing) {
      // initialisation started but not ended so far: wait for it until done or defined request time passed
      sInt32 t=fSessionP->RemainingRequestTime();
      if (fStartSyncThread.waitfor(t<0 ? 0 : t * 1000)) {
        // background thread has terminated
        sta = fStartSyncThread.exitcode();
        PDEBUGPRINTFX(DBG_HOT,("******* background thread for startSync() terminated with exit code=%ld, status sta=%hd", fStartSyncThread.exitcode(),sta));
        // initialisation is now complete
        fInitializing=false;
      } // if
    } // if
  } // if
  #endif

  // now complete initialisation if not still initializing in background
  if (!fStartInit && !fInitializing) {
    // finished background processing
    if (sta==LOCERR_OK) {
      // quick test: if number of items is > than allowed maxid of remote datatstore,
      // sync is unlikely to succeed
      if (getRemoteDatastore()->getMaxID()<(long)fItems.size()) {
        // this will not work, warn (but no longer abort session, as Siemens S55 guys don't like that)
        CONSOLEPRINTF((
          "Warning: Synchronisation involves more items (%ld) than client can possibly manage (%ld",
          (long)fItems.size(),
          (long)getRemoteDatastore()->getMaxID()
        ));
        PDEBUGPRINTFX(DBG_ERROR,(
          "Warning: Synchronisation involves more items (%ld) than client can possibly manage (%ld)",
          (long)fItems.size(),
          (long)getRemoteDatastore()->getMaxID()
        ));
      }
    }
    // return status of initialisation
    return sta;
  }
  else {
    // background processing still in progress
    // - if we are still processing in background, init is ok so far
    return LOCERR_OK;
  }
} // TStdLogicDS::startDataAccessForServer


// called to check if conflicting replace or delete command from server exists
TSyncItem *TStdLogicDS::getConflictingItemByRemoteID(TSyncItem *syncitemP)
{
  // search for conflicting item by remoteID
  TSyncItemPContainer::iterator pos;
  for (pos=fItems.begin(); pos!=fItems.end(); ++pos) {
    if (strcmp((*pos)->getRemoteID(),syncitemP->getRemoteID())==0) {
      // same LUID exists in data from server
      PDEBUGPRINTFX(DBG_DATA+DBG_CONFLICT,(
        "TStdLogicDS::getConflictingItemByRemoteID, found RemoteID='%s', LocalID='%s', syncop=%s",
        syncitemP->getRemoteID(),
        syncitemP->getLocalID(),
        SyncOpNames[syncitemP->getSyncOp()]
      ));
      return (*pos); // return pointer to item in question
    }
  }
  PDEBUGPRINTFX(DBG_DATA+DBG_CONFLICT,("TStdLogicDS::getConflictingItemByRemoteID, no conflicting item"));
  return NULL;
} // TStdLogicDS::getConflictingItemByRemoteID



// called to check if conflicting item (with same localID) already exists in the list of items
// to be sent to the server
TSyncItem *TStdLogicDS::getConflictingItemByLocalID(TSyncItem *syncitemP)
{
  // search for conflicting item by localID
  TSyncItemPContainer::iterator pos;
  for (pos=fItems.begin(); pos!=fItems.end(); ++pos) {
    if (strcmp((*pos)->getLocalID(),syncitemP->getLocalID())==0) {
      // same LUID exists in data from server
      PDEBUGPRINTFX(DBG_DATA+DBG_CONFLICT,(
        "TStdLogicDS::getConflictingItemByLocalID, found RemoteID='%s', LocalID='%s', syncop=%s",
        syncitemP->getRemoteID(),
        syncitemP->getLocalID(),
        SyncOpNames[syncitemP->getSyncOp()]
      ));
      return (*pos); // return pointer to item in question
    }
  }
  PDEBUGPRINTFX(DBG_DATA+DBG_CONFLICT,("TStdLogicDS::getConflictingItemByLocalID, no conflicting item"));
  return NULL;
} // TStdLogicDS::getConflictingItemByLocalID



// called to check if content-matching item from server exists for slow sync
TSyncItem *TStdLogicDS::getMatchingItem(TSyncItem *syncitemP, TEqualityMode aEqMode)
{
  // search for content matching item
  TSyncItemPContainer::iterator pos;
  for (pos=fItems.begin(); pos!=fItems.end(); ++pos) {
    DEBUGPRINTFX(DBG_DATA+DBG_MATCH+DBG_EXOTIC,(
      "comparing (this) local item localID='%s' with incoming (other) item remoteID='%s'",
      (*pos)->getLocalID(),
      syncitemP->getRemoteID()
    ));
    if ((*pos)->compareWith(
      *syncitemP,aEqMode,this
      #ifdef SYDEBUG
      ,PDEBUGTEST(DBG_DATA+DBG_MATCH+DBG_EXOTIC) // only show comparison if exotic AND match is enabled
      #endif
    )==0) {
      // items match in content
      // - check if item is not already matched
      if ((*pos)->getSyncOp()!=sop_wants_add && (*pos)->getSyncOp()!=sop_reference_only) {
        // item has already been matched before, so don't match it again
        DEBUGPRINTFX(DBG_DATA,(
          "TStdLogicDS::getMatchingItem, match but already used -> skip it: remoteID='%s' = localID='%s'",
          syncitemP->getRemoteID(),
          (*pos)->getLocalID()
        ));
      }
      else {
        // item has not been matched yet (wannabe add or reference-only), return it now
        PDEBUGPRINTFX(DBG_DATA+DBG_MATCH+DBG_HOT,(
          "TStdLogicDS::getMatchingItem, found remoteID='%s' is equal in content with localID='%s'",
          syncitemP->getRemoteID(),
          (*pos)->getLocalID()
        ));
        return (*pos); // return pointer to item in question
      }
    }
  }
  PDEBUGPRINTFX(DBG_DATA+DBG_MATCH,("TStdLogicDS::getMatchingItem, no matching item"));
  return NULL;
} // TStdLogicDS::getMatchingItem


// - called to prevent item to be sent to client in subsequent generateSyncCommands()
//   item in question should be an item that was returned by getConflictingItemByRemoteID() or getMatchingItem()
void TStdLogicDS::dontSendItemAsServer(TSyncItem *syncitemP)
{
  PDEBUGPRINTFX(DBG_DATA+DBG_EXOTIC,("Preventing localID='%s' to be sent to client",syncitemP->getLocalID()));
  syncitemP->setSyncOp(sop_none); // anyway, set to none
  // delete from list as we don't need it any more
  TSyncItemPContainer::iterator pos;
  for (pos=fItems.begin(); pos!=fItems.end(); ++pos) {
    if (*pos == syncitemP) {
      // it is in our list
      PDEBUGPRINTFX(DBG_DATA+DBG_HOT,("Item with localID='%s' will NOT be sent to client (slowsync match / duplicate prevention)",syncitemP->getLocalID()));
      delete *pos; // delete item itself
      fItems.erase(pos); // remove from list
      break;
    }
  }
} // TStdLogicDS::dontSendItemAsServer


// - called when a item in the sync set changes its localID (due to local DB internals)
//   Datastore must make sure that possibly cached items get updated
// - NOTE: derivates must take care of updating map entries as well!
void TStdLogicDS::dsLocalIdHasChanged(const char *aOldID, const char *aNewID)
{
  PDEBUGPRINTFX(DBG_DATA,("TStdLogicDS::dsLocalIdHasChanged"));
  // update in loaded list of items
  TSyncItemPContainer::iterator pos;
  for (pos=fItems.begin(); pos!=fItems.end(); ++pos) {
    if (strcmp((*pos)->getLocalID(),aOldID)==0) {
      // found item, change it's local ID now
      (*pos)->setLocalID(aNewID);
      // make sure internal dependencies get updated
      (*pos)->updateLocalIDDependencies();
      // done
      break;
    }
  }
  // let base class do what is needed to update the item itself
  inherited::dsLocalIdHasChanged(aOldID, aNewID);
} // TStdLogicDS::dsLocalIdHasChanged



// - called to have additional item sent to remote
void TStdLogicDS::SendItemAsServer(TSyncItem *aSyncitemP)
{
  // add to list of changes
  fItems.push_back(aSyncitemP);
} // TStdLogicDS::SendItemAsServer


// - end map operation (derived class might want to rollback)
bool TStdLogicDS::MapFinishAsServer(
  bool aDoCommit,                // if not set, entire map operation must be undone
  TStatusCommand &aStatusCommand // status, must be set on error or non-200-status
)
{
  // unsuccessful Map will cause rollback of entire datastore transaction
  if (!aDoCommit) {
    // bad, abort session
    engAbortDataStoreSync(510,true); // data store failed, local problem
  }
  return true;
} // TStdLogicDS::MapFinishAsServer


// - called to let server generate sync commands for client
//   Returns true if now finished (or aborted) for this datastore
//   also sets fState to dss_syncdone when finished
bool TStdLogicDS::logicGenerateSyncCommandsAsServer(
  TSmlCommandPContainer &aNextMessageCommands,
  TSmlCommand * &aInterruptedCommandP,
  cAppCharP aLocalIDPrefix
)
{
  bool alldone=false;
  bool ignoreitem;
  uInt32 itemcount=0;
  // send as many as possible from list of local modifications
  // sop_want_replace can only be sent if state is already dss_syncfinish
  TSyncItemPContainer::iterator pos;
  pos = fItems.begin(); // first item
  TSyncItemType *itemtypeP = getRemoteReceiveType();
  POINTERTEST(itemtypeP,("TStdLogicDS::logicGenerateSyncCommandsAsServer: fRemoteReceiveFromLocalTypeP undefined"));
  #ifdef SYDEBUG
  if (fMaxItemCount> 0) {
    PDEBUGPRINTFX(DBG_DATA+DBG_HOT,(
      "Info: Max number of items to be sent in this session is limited to %ld (already sent by now=%ld)",
      (long)fMaxItemCount,
      (long)fItemsSent
    ));
  }
  #endif
  while (
    !isAborted() && // not aborted
    (getDSConfig()->fMaxItemsPerMessage==0 || itemcount<getDSConfig()->fMaxItemsPerMessage) && // max item count per message not reached or not active
    !fSessionP->outgoingMessageFull() && // message not full
    aNextMessageCommands.size()==0 // no commands already queued for next message
  ) {
    // get item to process
    if (pos == fItems.end()) {
      alldone=true;
      break;
    }
    //
    TSyncItem *syncitemP = (*pos);
    // get sync op to perform
    TSyncOperation syncop=syncitemP->getSyncOp();
    // check if we can send the item now (for replaces, we need ALWAYS to wait until client has finished sending)
    // Note: usually sync engine will not start generating before dssta_serverseenclientmods anyway, but...
    if (syncop==sop_wants_replace && !testState(dssta_serverseenclientmods)) {
      // cannot be sent now, take next
      pos++;
      continue;
    }
    // check if we should ignore this item
    ignoreitem = syncop==sop_reference_only; // ignore anyway if reference only
    // further check if not already ignored
    if (!ignoreitem) {
      // - check if adding is still allowed
      if (fRemoteAddingStopped && (syncop==sop_wants_add || syncop==sop_add)) {
        // adding to remote has been stopped, discard add items
        PDEBUGPRINTFX(DBG_DATA,(
          "Suppressed add for item localID='%s' (fRemoteAddingStopped)",
          syncitemP->getLocalID()
        ));
        ignoreitem=true;
      }
      #ifdef SYNCML_TAF_SUPPORT
      // check other reasons to prevent further adds
      if (syncop==sop_wants_add || syncop==sop_add) {
        // - check if max number of items has already been reached
        if (fMaxItemCount!=0 && fItemsSent>=fMaxItemCount) {
          PDEBUGPRINTFX(DBG_DATA,(
            "Suppressed add for item localID='%s' (max item count=%ld reached)",
            syncitemP->getLocalID(),
            fMaxItemCount
          ));
          ignoreitem=true;
        }
        // - check if item passes possible TAF
        /// %%% (do not filter replaces, as these would not get reported again in the next session)
        /// @todo: the above is no longer true as we can now have them re-sent in next session,
        ///        so this must be changed later!!!
        if (!(
          syncitemP->testFilter(fTargetAddressFilter.c_str()) &&
          syncitemP->testFilter(fIntTargetAddressFilter.c_str())
        )) {
          PDEBUGPRINTFX(DBG_DATA+DBG_HOT,(
            "Item localID='%s' does not pass INCLUSIVE filter (TAF) -> Suppressed adding",
            syncitemP->getLocalID()
          ));
          ignoreitem=true;
        }
      }
      #endif
    }
    // Now discard if ignored
    if (ignoreitem) {
      // remove item from list
      TSyncItemPContainer::iterator temp_pos = pos++; // make copy and set iterator to next
      fItems.erase(temp_pos); // now entry can be deleted (N.M. Josuttis, pg204)
      // delete item itself
      delete syncitemP;
      // test next
      continue;
    }
    // create sync op command (may return NULL in case command cannot be created, e.g. for MaxObjSize limitations)
    TSyncOpCommand *syncopcmdP = newSyncOpCommand(syncitemP,itemtypeP,aLocalIDPrefix);
    // erase item from list
    delete syncitemP;
    pos = fItems.erase(pos);
    // issue command now
    // - Note that when command is split, issuePtr returns true, but we still may NOT generate new commands
    //   as the message is already full now. That's why the while contains a check for message full and aNextMessageCommands size
    //   (was not the case before 2.1.0.2, which could cause that the first chunk of a subsequent command
    //   would be sent before the third..nth chunk of the previous command).
    TSmlCommand *cmdP = syncopcmdP;
    syncopcmdP=NULL;
    // possibly, we have a NULL command here (e.g. in case it could not be generated due to MaxObjSize restrictions)
    if (cmdP) {
      if (!fSessionP->issuePtr(cmdP,aNextMessageCommands,aInterruptedCommandP)) {
        alldone=false; // issue failed (no room in message), not finished so far
        break;
      }
      // count item sent
      fItemsSent++; // overall counter for statistics
      itemcount++; // per message counter
      // send event (but no check for abort)
      DB_PROGRESS_EVENT(this,pev_itemsent,fItemsSent,getNumberOfChanges(),0);
    }
  }; // while not aborted and not message full
  // we are not done until all aNextMessageCommands are also out
  // Note: this must be specially checked because we now have SyncML 1.1 chunked commands.
  //   Those issue() fine, but leave a next chunk in the aNextMessageCommands queue.
  if (alldone && aNextMessageCommands.size()>0) {
    alldone=false;
  }
  // finished when we have done all
  return (alldone || isAborted());
} // TStdLogicDS::logicGenerateSyncCommandsAsServer


// called for servers when receiving map from client
localstatus TStdLogicDS::logicProcessMap(cAppCharP aRemoteID, cAppCharP aLocalID)
{
  // simply call implementation
  return implProcessMap(aRemoteID, aLocalID);
} // TStdLogicDS::logicProcessMap


#endif // SYSYNC_SERVER



#ifdef SYSYNC_CLIENT

// Client Case
// ===========

// called by dsBeforeStateChange to dssta_dataaccessstarted to make sure datastore is ready for being accessed.
localstatus TStdLogicDS::startDataAccessForClient(void)
{
  DEBUGPRINTFX(DBG_HOT,("TStdLogicDS::startDataAccessForClient"));
  // init
  fWriteStarted=false;
  fEoC=false; // not all changes seen yet
  // prepare for read
  #ifdef SCRIPT_SUPPORT
  // - call DB init script, which might add extra filters depending on options and remoterule
  TScriptContext::execute(
    fDataStoreScriptContextP,
    getDSConfig()->fDBInitScript,
    &DBFuncTable, // context's function table
    this // datastore pointer needed for context
  );
  // - datastoreinitscript might abort the sync with this datastore, check for that and exit if so
  if (isAborted()) {
    return getAbortStatusCode();
  }
  #endif
  // - init post fetch filtering, sets fFilteringNeededForAll and fFilteringNeeded correctly
  initPostFetchFiltering();
  // - prepare for read
  localstatus sta=implStartDataRead();
  return sta;
} // TStdLogicDS::startDataAccessForClient


// called to generate sync sub-commands as client for remote server
// @return true if now finished for this datastore
bool TStdLogicDS::logicGenerateSyncCommandsAsClient(
  TSmlCommandPContainer &aNextMessageCommands,
  TSmlCommand * &aInterruptedCommandP,
  cAppCharP aLocalIDPrefix
)
{
  localstatus sta = LOCERR_OK;
  bool alldone=true;
  // send as many changed items as possible
  TSyncItemType *itemtypeP = getRemoteReceiveType();
  POINTERTEST(itemtypeP,("TStdLogicDS::logicGenerateSyncCommandsAsClient: fRemoteReceiveFromLocalTypeP undefined"));
  while (!fEoC && !isAborted() && !fSessionP->outgoingMessageFull() && aNextMessageCommands.size()==0) {
    // get next item from DB
    TSyncItem *syncitemP = NULL;
    #ifdef OBJECT_FILTERING
    bool changed=!fFilteringNeededForAll; // set if we need all records for later filtering
    #else
    bool changed=true; // without filters, always let DB check if modified
    #endif
    sta = implGetItem(fEoC,changed,syncitemP);
    if (sta!=LOCERR_OK) {
      // fatal error
      implEndDataRead(); // terminate reading (error does not matter)
      engAbortDataStoreSync(sta, true); // local problem
      return false; // not complete
    }
    // read successful, test for EoC (end of changes)
    if (fEoC) break; // reading done
    if (fSlowSync) changed=true; // all have changed (just in case GetItem does not return clean result here)
    // get sync op to perform
    TSyncOperation syncop=syncitemP->getSyncOp();
    if (syncop!=sop_delete && syncop!=sop_soft_delete && syncop!=sop_archive_delete) {
      #ifdef OBJECT_FILTERING
      // Filtering
      // - call this anyway (makes sure item is made conformant to remoteAccept filter, even if
      //   fFilteringNeeded is not set)
      bool passes=postFetchFiltering(syncitemP);
      if (fFilteringNeeded && !passes) {
        // item was changed and does not pass now -> might be fallen out of the sync set now.
        // Only when the DB is capable of tracking items fallen out of the sync set (i.e. bring them up as adds
        // later should they match the filter criteria again), we can implement removing based
        // on filter criteria. Otherwise, these are simply ignored.
        if (implTracksSyncopChanges() && !fSlowSync && changed && (syncop==sop_replace || syncop==sop_wants_replace)) {
          // item already exists on remote but falls out of syncset now: delete
          // NOTE: This works only if reviewReadItem() is correctly implemented
          //       and checks for items that are deleted after being reported
          //       as replace to delete their local map entry (which makes
          //       them add candidates again)
          syncop = sop_delete;
          syncitemP->cleardata(); // also get rid of unneeded data
        }
        else
          syncop = sop_none; // ignore all others (especially adds or slowsync replaces)
      }
      else
      #endif
      {
        // item passes (or no filters anyway) -> belongs to sync set
        if ((syncop==sop_replace || syncop==sop_wants_replace) && !changed && !fSlowSync) {
          // exists but has not changed since last sync
          syncop=sop_none; // ignore for now
        }
      }
    }
    // check if we should use that item
    if (syncop==sop_none) {
      delete syncitemP;
      continue; // try next from DB
    }
    // set final syncop now
    syncitemP->setSyncOp(syncop);
    // create sync op command
    TSyncOpCommand *syncopcmdP = newSyncOpCommand(syncitemP,itemtypeP,aLocalIDPrefix);
    #ifdef CLIENT_USES_SERVER_DB
    // save item, we need it later for post-processing and Map simulation
    fItems.push_back(syncitemP);
    #else
    // delete item, not used any more
    delete syncitemP;
    #endif
    // issue command now
    // - Note that when command is split, issuePtr returns true, but we still may NOT generate new commands
    //   as the message is already full now. That's why the while contains a check for message full and aNextMessageCommands size
    //   (was not the case before 2.1.0.2, which could cause that the first chunk of a subsequent command
    //   would be sent before the third..nth chunk of the previous command).
    TSmlCommand *cmdP = syncopcmdP;
    syncopcmdP=NULL;
    // possibly, we have a NULL command here (e.g. in case it could not be generated due to MaxObjSize restrictions)
    if (cmdP) {
      if (!fSessionP->issuePtr(cmdP,aNextMessageCommands,aInterruptedCommandP)) {
        alldone=false; // issue failed (no room in message), not finished so far
        break;
      }
      // count item sent
      fItemsSent++;
      // send event and check for abort
      #ifdef PROGRESS_EVENTS
      if (!DB_PROGRESS_EVENT(this,pev_itemsent,fItemsSent,getNumberOfChanges(),0)) {
        implEndDataRead(); // terminate reading
        fSessionP->AbortSession(500,true,LOCERR_USERABORT);
        return false; // error
      }
      // check for "soft" suspension
      if (!SESSION_PROGRESS_EVENT(fSessionP,pev_suspendcheck,NULL,0,0,0)) {
        fSessionP->SuspendSession(LOCERR_USERSUSPEND);
      }
      #endif
    }
  }; // while not aborted
  // we are not done until all aNextMessageCommands are also out
  // Note: this must be specially checked because we now have SyncML 1.1 chunked commands.
  //   Those issue() fine, but leave a next chunk in the aNextMessageCommands queue.
  if (alldone && aNextMessageCommands.size()>0) {
    alldone=false;
  }
  // done if we are now ready for sync or if aborted
  return ((alldone && fEoC) || isAborted());
} // TStdLogicDS::logicGenerateSyncCommandsAsClient


#endif // SYSYNC_CLIENT


/// @brief called to have all not-yet-generated sync commands as "to-be-resumed"
void TStdLogicDS::logicMarkOnlyUngeneratedForResume(void)
{
  if (IS_SERVER) {
    #ifdef SYSYNC_SERVER
    // we do not maintain the map/bookmark list at this level, so
    // derived class (ODBC, BinFile etc.) must make sure that their list is
    // clean (no marks from previous sessions) before calling this inherited version
    implMarkOnlyUngeneratedForResume();
    // Now add those that we have already received from the implementation
    TSyncItemPContainer::iterator pos;
    for (pos = fItems.begin(); pos != fItems.end(); ++pos) {
      // let datastore mark these unprocessed
      TSyncItem *syncitemP = (*pos);
      // mark it for resume by ID
      logicMarkItemForResume(syncitemP->getLocalID(),syncitemP->getRemoteID(),true); // these are unsent
    }
    #endif // SYSYNC_SERVER
  }
  else {
    #ifdef SYSYNC_CLIENT
    // we do not maintain the map/bookmark list at this level, so
    // derived class (ODBC, BinFile etc.) must make sure that their list is
    // clean (no marks from previous sessions) before calling this inherited version
    implMarkOnlyUngeneratedForResume();
    // in client case, fItems does not contain ungenerated/unprocessed items
    // so we don't have anything more do here for now
    #endif // SYSYNC_CLIENT
  }
} // TStdLogicDS::logicMarkOnlyUngeneratedForResume



/// helper to merge database version of an item with the passed version of the same item;
/// does age comparison by default, with "local side wins" as fallback
TMultiFieldItem *TStdLogicDS::mergeWithDatabaseVersion(TSyncItem *aSyncItemP, bool &aChangedDBVersion, bool &aChangedNewVersion)
{
  aChangedDBVersion = false;
  aChangedNewVersion = false;

  TStatusCommand dummy(fSessionP);
  TMultiFieldItem *dbVersionItemP = (TMultiFieldItem *)newItemForRemote(aSyncItemP->getTypeID());
  if (!dbVersionItemP) return NULL;
  // - set IDs
  dbVersionItemP->setLocalID(aSyncItemP->getLocalID());
  dbVersionItemP->setRemoteID(aSyncItemP->getRemoteID());
  // - result is always a replace (item exists in DB)
  dbVersionItemP->setSyncOp(sop_wants_replace);
  // - try to get from DB
  bool ok=logicRetrieveItemByID(*dbVersionItemP,dummy);
  if (ok && dummy.getStatusCode()!=404) {
    // item found in DB, merge with original item
    // TODO (?): make this configurable
    TConflictResolution crstrategy = cr_newer_wins;

    if (crstrategy==cr_newer_wins) {
      sInt16 cmpRes = aSyncItemP->compareWith(
        *dbVersionItemP,
        eqm_nocompare,this
        #ifdef SYDEBUG
        ,PDEBUGTEST(DBG_CONFLICT+DBG_DETAILS) // show age comparisons only if we want to see details
        #endif
      );
      if (cmpRes==-1) crstrategy=cr_server_wins;
      else crstrategy=cr_client_wins;
      PDEBUGPRINTFX(DBG_DATA,(
        "Newer item determined: %s",
        crstrategy==cr_client_wins ?
        "Incoming item is newer and wins" :
        "DB item is newer and wins"
      ));
      if (crstrategy==cr_client_wins) {
        aSyncItemP->mergeWith(*dbVersionItemP, aChangedNewVersion, aChangedDBVersion, this);
      } else {
        dbVersionItemP->mergeWith(*aSyncItemP, aChangedDBVersion, aChangedNewVersion, this);
      }
      PDEBUGPRINTFX(DBG_DATA,(
        "Merged incoming item (%s,relevant,%smodified) with version from database (%s,%s,%smodified)",
        crstrategy==cr_client_wins ? "winning" : "loosing",
        aChangedNewVersion ? "" : "NOT ",
        crstrategy==cr_server_wins ? "winning" : "loosing",
        aChangedDBVersion ? "to-be-replaced" : "to-be-left-unchanged",
        aChangedDBVersion ? "" : "NOT "
      ));
    }
  }
  else {
    // no item found, we cannot force a conflict
    PDEBUGPRINTFX(DBG_ERROR,("Could not retrieve database version of item, DB status code = %hd",dummy.getStatusCode()));
    delete dbVersionItemP;
    dbVersionItemP=NULL;
    return NULL;
  }
  return dbVersionItemP;
} // TStdLogicDS::mergeWithDatabaseVersion





// called to process incoming item operation
// Method takes ownership of syncitemP in all cases
bool TStdLogicDS::logicProcessRemoteItem(
  TSyncItem *syncitemP,
  TStatusCommand &aStatusCommand,
  bool &aVisibleInSyncset, // on entry: tells if resulting item SHOULD be visible; on exit: set if processed item remains visible in the sync set.
  string *aGUID // GUID is stored here if not NULL
) {
  localstatus sta=LOCERR_OK;
  bool irregular=false;
  bool shouldbevisible=aVisibleInSyncset;
  string datatext;

  TP_DEFIDX(li);
  TP_SWITCH(li,fSessionP->fTPInfo,TP_database);
  SYSYNC_TRY {
    // assume item will stay visible in the syncset after processing
    aVisibleInSyncset=true;
    // start writing if not already started
    sta=startDataWrite();
    if (sta!=LOCERR_OK) {
      aStatusCommand.setStatusCode(sta);
    }
    else {
      // show
      DEBUGPRINTFX(DBG_DATA,(
        "TStdLogicDS::logicProcessRemoteItem starting, SyncOp=%s, RemoteID='%s', LocalID='%s'",
        SyncOpNames[syncitemP->getSyncOp()],
        syncitemP->getRemoteID(),
        syncitemP->getLocalID()
      ));
      // now perform action
      if (syncitemP->getSyncOp()==sop_replace || syncitemP->getSyncOp()==sop_wants_replace) {
        // check if we should read before writing
        TMultiFieldItem *mfiP;
        GET_CASTED_PTR(mfiP,TMultiFieldItem,syncitemP,"");
        bool replacewritesallfields = dsReplaceWritesAllDBFields();
        bool mightcontaincutoff=false;
        // - see if we would pass sync set filter as is
        #ifdef OBJECT_FILTERING
        aVisibleInSyncset = mfiP->testFilter(fSyncSetFilter.c_str());
        bool invisible =
          !getDSConfig()->fInvisibleFilter.empty() && // has an invisible filter
          mfiP->testFilter(getDSConfig()->fInvisibleFilter.c_str()); // and passes it -> invisible
        bool visibilityok = (aVisibleInSyncset && !invisible) == shouldbevisible; // check if visibility is correct
        if (visibilityok && !replacewritesallfields && aVisibleInSyncset) // avoid expensive check if we have to read anyway
        #endif
          mightcontaincutoff = mfiP->getItemType()->mayContainCutOffData(mfiP->getTargetItemType());
        // - if not, we must read item from the DB first and then
        //   test again.
        if (
          #ifdef OBJECT_FILTERING
          !visibilityok ||
          #endif
          replacewritesallfields ||
          mightcontaincutoff ||
          fIgnoreUpdate // if we may not update items, only add them, then we must check first if item exists in DB
        ) {
          // the item we are replacing might contain cut-off data or
          // needs otherwise to be modified based on current contents
          // we should therefore read item from DB first
          // - create new empty TMultiFieldItem
          TMultiFieldItem *refitemP =
            (TMultiFieldItem *) newItemForRemote(ity_multifield);
          if (IS_CLIENT) {
            // Client: retrieve by local ID
            refitemP->clearRemoteID(); // not known
            refitemP->setLocalID(syncitemP->getLocalID()); // make sure we retrieve by local ID
          }
          else {
            // Server: retrieve by remote ID
            refitemP->clearLocalID(); // make sure we retrieve by remote ID
            refitemP->setRemoteID(syncitemP->getRemoteID()); // make sure we retrieve by remote ID
          }
          refitemP->setSyncOp(sop_replace);
          #ifdef OBJECT_FILTERING
          PDEBUGPRINTFX(DBG_DATA,(
            "TStdLogicDS: Need read-modify-write (cause: %s%s%s%s) -> retrieve original item from DB",
            !visibilityok ? "visibility_not_ok " : "",
            replacewritesallfields ? "replace_writes_all_fields " : "",
            mightcontaincutoff ? "might_contain_cutoff_data " : "",
            fIgnoreUpdate ? "ignoreUpdate " : ""
          ));
          #else
          PDEBUGPRINTFX(DBG_DATA,(
            "TStdLogicDS: Need read-modify-write (cause: %s%s%s) -> retrieve original item from DB",
            replacewritesallfields ? "replace_writes_all_fields " : "",
            mightcontaincutoff ? "might_contain_cutoff_data " : "",
            fIgnoreUpdate ? "ignoreUpdate " : ""
          ));
          #endif
          if (implRetrieveItemByID(*refitemP,aStatusCommand)) {
            #ifdef SYDEBUG
            if (PDEBUGTEST(DBG_DATA)) {
              PDEBUGPRINTFX(DBG_DATA,("TStdLogicDS: Retrieved item"));
              if (PDEBUGTEST(DBG_DATA+DBG_DETAILS)) refitemP->debugShowItem(); // show item retrieved
            }
            #endif
            if (fIgnoreUpdate) {
              // updates may not be executed at all, and be simply ignored
              aStatusCommand.setStatusCode(200); // fake ok.
              PDEBUGPRINTFX(DBG_DATA+DBG_HOT,("TStdLogicDS: fIgnoreUpdate set, update command not executed but answered with status 200"));
              irregular=true;
              sta=LOCERR_OK;
              goto processed;
            }
            // we got the item to be replaced
            // - now modify it:
            //   - only modify available fields
            //   - perform cutoff prevention
            //   - do not just copy assigned fields (but all those that are available)
            //   - IF replace can write individual fields, then transfer unassigned status
            //     (also for non-availables!) to avoid that datastore needs to write back
            //     values that are already there.
            refitemP->replaceDataFrom(*syncitemP,true,true,false,!replacewritesallfields);
            #ifdef SYDEBUG
            if (PDEBUGTEST(DBG_DATA)) {
              PDEBUGPRINTFX(DBG_DATA,("TStdLogicDS: Item updated with contents from remote"));
              if (PDEBUGTEST(DBG_DATA+DBG_EXOTIC)) refitemP->debugShowItem(); // show item retrieved
            }
            #endif
            #ifdef OBJECT_FILTERING
            // - make sure item will pass sync set filter NOW
            makePassSyncSetFilter(refitemP);
            // - make sure item will be visible NOW
            makeVisible(refitemP);
            #ifdef SYDEBUG
            if (PDEBUGTEST(DBG_DATA)) {
              PDEBUGPRINTFX(DBG_DATA,("TStdLogicDS: Made visible and pass sync set filter"));
              if (PDEBUGTEST(DBG_DATA+DBG_EXOTIC)) refitemP->debugShowItem(); // show item retrieved
            }
            #endif
            #endif
            // - get rid of original
            delete syncitemP;
            // - use new item for further processing
            syncitemP=refitemP;
          }
          else {
            // failed retrieving item: switch to add
            sta = aStatusCommand.getStatusCode();
            if (sta==404 || sta==410) {
              // this is an irregularity for a client (but it's perfectly
              // normal case for server, as client may use replace for adds+replaces)
              if (IS_CLIENT && !syncitemP->hasRemoteID()) {
                // - we cannot handle this properly, we have no remoteID, so report error to server
                PDEBUGPRINTFX(DBG_ERROR,("TStdLogicDS: Item not found, but cannot switch to add because no RemoteID is known, Status=%hd",aStatusCommand.getStatusCode()));
              }
              else {
                if (fPreventAdd) {
                  // prevent implicit add -> return sta as is
                }
                else {
                  // - switch to add if remote sent remoteID along so we can properly map
                  syncitemP->setSyncOp(sop_add);
                  PDEBUGPRINTFX(DBG_DATA,("TStdLogicDS: RetrieveItem: not found (Status=%hd) --> adding instead",sta));
                  if (IS_SERVER) {
                    #ifdef SYSYNC_SERVER
                    // server: make sure we delete the old item from the map table (as we *know* the item is gone - should it reappear under a different localID, it'll be re-added)
                    implProcessMap(syncitemP->getRemoteID(),NULL);
                    #endif
                  }
                  else {
                    // client: we can handle it, as we know the remoteID
                    irregular=true; // irregular only for client
                  }
                  sta=LOCERR_OK; // ok for further processing, anyway
                }
              }
            }
            else {
              // this is a fatal error, report it
              PDEBUGPRINTFX(DBG_ERROR,("TStdLogicDS: RetrieveItem failed, Status=%hd",sta));
            }
            // get rid of reference item
            delete refitemP;
          }
        }
      }
      if (sta==LOCERR_OK) {
        // make sure that added items will pass sync set filters. If we can't make them pass
        // for DS 1.2 exclusive filters we must generate a delete so we must remember the itempassed status)
        #ifdef OBJECT_FILTERING
        if (syncitemP->getSyncOp()==sop_add || syncitemP->getSyncOp()==sop_wants_add) {
          // - make sure new item will pass sync set filter when re-read from DB
          aVisibleInSyncset=makePassSyncSetFilter(syncitemP);
          // - also make sure new item has correct visibility status
          if (shouldbevisible)
            makeVisible(syncitemP); // make visible
          else
            aVisibleInSyncset = !makeInvisible(syncitemP); // make invisible
        }
        #endif
        // Now let derived class process the item
        if (!implProcessItem(syncitemP,aStatusCommand))
          sta = aStatusCommand.getStatusCode(); // not successful, get error status code
      }
      // perform special case handling
      if (sta!=LOCERR_OK) {
        // irregular, special case handling
        switch (syncitemP->getSyncOp()) {
          case sop_wants_add : // to make sure
          case sop_add :
            if (sta==418) {
              // 418: item already exists, this is kind of a conflict
              if (isResuming() || fIgnoreUpdate || IS_CLIENT) {
                // - in a client, this should not happen (prevented via checking against pending maps before
                //   this routine is called) - if it still does just report the status back
                // - in a resume, this can happen and the add should be ignored (se we return the status 418)
                // - if updates are to be ignored, don't try update instead (and report 418)
                // --> just return the status as-is
              }
              else {
                // in normal sync in the server case, this can happen when a previous session
                // was aborted (and already applied adds not rolled back)
                // --> reprocess it as a replace
                PDEBUGPRINTFX(DBG_DATA,("to-be-added item already exists -> trying replace (=conflict resolved by client winning)"));
                // - switch to replace
                syncitemP->setSyncOp(sop_replace);
                irregular=true;
                // - process again
                if (implProcessItem(syncitemP,aStatusCommand)) {
                  aStatusCommand.setStatusCode(208); // client has won
                }
                else {
                  // failed, return status
                  sta = aStatusCommand.getStatusCode();
                }
              }
            }
            break;
          case sop_replace :
            if (sta==404 || sta==410) {
              // this is an irregularity for a client (but it's perfectly
              // normal case for server, as client may use replace for adds+replaces)
              if (IS_CLIENT && !syncitemP->hasRemoteID()) {
                // - we cannot handle this properly, we have no remoteID, so report error to server
                PDEBUGPRINTFX(DBG_ERROR,("to-be-replaced item not found, but cannot switch to add because no RemoteID is known, Status=%hd",sta));
              }
              else {
                if (fPreventAdd) {
                  // prevent implicit add -> return status as is
                }
                else {
                  // - switch to add if remote sent remoteID along so we can properly map
                  syncitemP->setSyncOp(sop_add);
                  if (IS_SERVER) {
                    #ifdef SYSYNC_SERVER
                    // - make sure we delete the old item from the map table (as we *know* the item is gone - should it reappear under a different localID, it'll be re-added)
                    implProcessMap(syncitemP->getRemoteID(),NULL);
                    #endif
                  }
                  else {
                    // we can handle it, as we know the remoteID
                    irregular=true;
                  }
                  PDEBUGPRINTFX(DBG_DATA,("to-be-replaced item not found (Status=%hd) --> adding instead",sta));
                  // - process again (note that we are re-using the status command that might
                  //   already have a text item with an OS errir if something failed before)
                  sta=LOCERR_OK; // forget previous status
                  if (!implProcessItem(syncitemP,aStatusCommand))
                    sta=aStatusCommand.getStatusCode(); // not successful, get error status code
                }
              }
            }
            break;
          case sop_delete :
          case sop_archive_delete :
          case sop_soft_delete :
            if (sta==404 || sta==410) {
              if (fSessionP->getSessionConfig()->fDeletingGoneOK) {
                // 404/410: item not found, could be because previous aborted session has
                // already committed deletion of that item -> behave as if delete was ok
                PDEBUGPRINTFX(DBG_DATA,("to-be-deleted item was not found, but do NOT report %hd",sta));
                aStatusCommand.setStatusCode(200);
                irregular=true;
                sta = LOCERR_OK; // this is ok, item is deleted already
              }
            }
            break;
          default :
            SYSYNC_THROW(TSyncException("Unknown sync op in TStdLogicDS::logicProcessRemoteItem"));
        } // switch
      } // if not ok
    processed:
      if (sta==LOCERR_OK) {
        PDEBUGPRINTFX(DBG_DATA,(
          "- Operation %s performed (%sregular), Remote ID=%s Local ID=%s, status=%hd",
          SyncOpNames[syncitemP->getSyncOp()],
          irregular ? "ir" : "",
          syncitemP->getRemoteID(),
          syncitemP->getLocalID(),
          aStatusCommand.getStatusCode()
        ));
        // return GUID if string ptr was passed
        if (aGUID) {
          (*aGUID)=syncitemP->getLocalID();
        }
      }
      else {
        PDEBUGPRINTFX(DBG_ERROR,(
          "- Operation %s failed with SyncML status=%hd",
          SyncOpNames[syncitemP->getSyncOp()],
          sta
        ));
      }
    } // if startDataWrite ok
    // anyway, we are done with this item, delete it now
    delete syncitemP;
    TP_START(fSessionP->fTPInfo,li);
    // done, return regular/irregular status
    return (sta==LOCERR_OK) && !irregular;
  }
  SYSYNC_CATCH (...)
    // delete the item
    if (syncitemP) delete syncitemP;
    TP_START(fSessionP->fTPInfo,li);
    // re-throw
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
  #ifndef __BORLANDC__
  return false; // BCPPB: unreachable code
  #endif
} // TStdLogicDS::logicProcessRemoteItem



// Abort datastore sync
void TStdLogicDS::dsAbortDatastoreSync(TSyError aReason, bool aLocalProblem)
{
  // call anchestor
  inherited::dsAbortDatastoreSync(aReason, aLocalProblem);
} // TStdLogicDS::dsAbortDatastoreSync


// inform logic of coming state change
localstatus TStdLogicDS::dsBeforeStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState)
{
  localstatus sta=LOCERR_OK;

  if (aNewState==dssta_dataaccessstarted) {
    // start data access
    if (IS_CLIENT) {
      #ifdef SYSYNC_CLIENT
      sta = startDataAccessForClient();
      #endif
    }
    else {
      #ifdef SYSYNC_SERVER
      sta = startDataAccessForServer();
      #endif
    }
  }
  if (IS_CLIENT) {
    if (aNewState==dssta_syncgendone) {
      // when client has done sync gen, start writing
      sta = startDataWrite();
    }
  } // client
  if (aNewState==dssta_completed && !isAborted()) {
    // finish writing data now anyway
    endDataWrite();
    // we must save anchors at the moment we shift from any state to dssta_completed
    PDEBUGPRINTFX(DBG_ADMIN,("TStdLogicDS: successfully completed, save anchors now"));
    // update our level's state
    fPreviousSyncTime = fCurrentSyncTime;
    // let implementation update their state and save it
    sta=implSaveEndOfSession(true);
    if (sta!=LOCERR_OK) {
      PDEBUGPRINTFX(DBG_ERROR,("TStdLogicDS: Could not save session completed status, err=%hd",sta));
    }
  }
  if (aNewState==dssta_idle && aOldState>=dssta_syncsetready) {
    // again: make sure data is written anyway (if already done this is a NOP)
    endDataWrite();
  }
  // abort on error
  if (sta!=LOCERR_OK) return sta;
  // let inherited do its stuff as well
  return inherited::dsBeforeStateChange(aOldState,aNewState);
} // TStdLogicDS::dsBeforeStateChange


// inform logic of happened state change
localstatus TStdLogicDS::dsAfterStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState)
{
  localstatus sta=LOCERR_OK;

  if (aNewState==dssta_dataaccessdone) {
    // finish writing data now
    endDataWrite();
  }
  // abort on error
  if (sta!=LOCERR_OK) return sta;
  // let inherited do its stuff as well
  return inherited::dsAfterStateChange(aOldState,aNewState);
} // TStdLogicDS::dsAfterStateChange


/** @deprecated obsolete, replaced by stuff in dsBeforeStateChange()
// called at very end of sync session, when all map commands are done, too
// Note: is also called before deleting a datastore (so aborted sessions
//   can do cleanup and/or statistics display as well)
void TStdLogicDS::endOfSync(bool aRegular)
{
  // save new sync anchor now
  // NOTE: gets called even if not active
  if (fState!=dss_idle) {
    PDEBUGPRINTFX(DBG_ADMIN,("TStdLogicDS::endOfSync, %sregular end of sync session",aRegular ? "" : "ir"));
    if (!aRegular) fRollback=true; // do not write irregular ends
    // datastore was active in sync, end it now
    if (!fRollback) {
      fRollback=!startWrite();
      if (!fRollback) {
        fRollback=!SaveAnchor(fNextRemoteAnchor.c_str());
      }
    }
    #ifdef SYDEBUG
    if (fRollback)
      DEBUGPRINTFX(DBG_ERROR,("************** Datastore error, rolling back transaction"));
    #endif
    // if session is complete, we can't resume it any more, so clear that status now
    if (!fRollback) {
      fResumeAlertCode=0; // no resume
    }
    // end writing now, sync is done
    endWrite();
  }
  // let ancestor do its things
  TLocalEngineDS::endOfSync(aRegular);
} // TStdLogicDS::endOfSync

*/

} // namespace sysync

/* end of TStdLogicDS implementation */

// eof
