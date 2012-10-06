/*
 *  File:         SuperDataStore.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TSuperDataStore
 *    "Virtual" datastore consisting of an union of other
 *    datastores, for example a vCal datastore based on
 *    two separate vEvent and vTodo datastores.
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 *  2002-08-05 : luz : created
 *
 */

#ifndef SuperDataStore_H
#define SuperDataStore_H

// includes
#include "configelement.h"
#include "syncitem.h"
#include "localengineds.h"

#ifdef SUPERDATASTORES


using namespace sysync;


namespace sysync {

class TSuperDataStore; // forward

#ifndef OBJECT_FILTERING
  #error "SUPERDATASTORE needs OBJECT_FILTERING"
#endif


// sub-datastore (contained datastore) link config
class TSubDSLinkConfig: public TConfigElement
{
  typedef TConfigElement inherited;
public:
  TSubDSLinkConfig(TLocalDSConfig *aLocalDSConfigP, TConfigElement *aParentElement);
  virtual ~TSubDSLinkConfig();
  // properties
  // - filter applied to test if incoming item is to be processed
  //   by this datastore
  string fDispatchFilter;
  // - GUID prefix to create super-GUIDs out of individual datastore GUIDs
  string fGUIDPrefix;
  // - linked datastore's config
  TLocalDSConfig *fLinkedDSConfigP;
  // - reset config to defaults
  virtual void clear();
protected:
  // check config elements
  #ifndef HARDCODED_CONFIG
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
}; // TSubDSLinkConfig


typedef std::list<TSubDSLinkConfig *> TSubDSConfigList;


// sub-datastore link
typedef struct {
  TLocalEngineDS *fDatastoreLinkP;
  TSubDSLinkConfig *fDSLinkConfigP;
  bool fStartPending;
} TSubDatastoreLink;

typedef std::list<TSubDatastoreLink> TSubDSLinkList;



// super datastore config
class TSuperDSConfig: public TLocalDSConfig
{
  typedef TLocalDSConfig inherited;
public:
  TSuperDSConfig(const char* aName, TConfigElement *aParentElement);
  virtual ~TSuperDSConfig();
  // properties
  // - contained subdatastores
  TSubDSConfigList fSubDatastores;
  // public methods
  // - create appropriate datastore from config, including creating links to subdatastores
  virtual TLocalEngineDS *newLocalDataStore(TSyncSession *aSessionP);
  // - reset config to defaults
  virtual void clear();
protected:
  // check config elements
  #ifndef HARDCODED_CONFIG
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  virtual void localResolve(bool aLastPass);
}; // TSuperDSConfig



class TSuperDataStore: public TLocalEngineDS
{
  typedef TLocalEngineDS inherited;
private:
  void InternalResetDataStore(void); // reset for re-use without re-creation
public:
  TSuperDataStore(TSuperDSConfig *aDSConfigP, TSyncSession *aSessionP, const char *aName, uInt32 aCommonSyncCapMask=0);
  virtual ~TSuperDataStore();
  virtual void dsResetDataStore(void) { InternalResetDataStore(); inherited::dsResetDataStore(); };
  // add links to subdatastores
  void addSubDatastoreLink(TSubDSLinkConfig *aDSLinkConfigP, TLocalEngineDS *aDatastoreP);
  // abort
  virtual void engAbortDataStoreSync(TSyError aReason, bool aLocalProblem, bool aResumable=true);
  virtual bool isAborted(void); // test abort status
  // Sync operation Methods overriding TLocalEngineDS (which need some distribution to subdatastores)
  // - process Sync alert for a datastore
  virtual TAlertCommand *engProcessSyncAlert(
    TSuperDataStore *aAsSubDatastoreOf, // if acting as subdatastore
    uInt16 aAlertCode,                  // the alert code
    const char *aLastRemoteAnchor,      // last anchor of client
    const char *aNextRemoteAnchor,      // next anchor of client
    const char *aTargetURI,             // target URI as sent by remote, no processing at all
    const char *aIdentifyingTargetURI,  // target URI that was used to identify datastore
    const char *aTargetURIOptions,      // option string contained in target URI
    SmlFilterPtr_t aTargetFilter,       ///< DS 1.2 filter, NULL if none
    const char *aSourceURI,             // source URI
    TStatusCommand &aStatusCommand      // status that might be modified
  );
  // - process status received for sync alert
  virtual bool engHandleAlertStatus(TSyError aStatusCode);
  // - Set remote datastore for local
  virtual void engSetRemoteDatastore(TRemoteDataStore *aRemoteDatastoreP);
  // - set Sync types needed for sending local data to remote DB
  virtual void setSendTypeInfo(
    TSyncItemType *aLocalSendToRemoteTypeP,
    TSyncItemType *aRemoteReceiveFromLocalTypeP);
  // - set Sync types needed for receiving remote data in local DB
  virtual void setReceiveTypeInfo(
    TSyncItemType *aLocalReceiveFromRemoteTypeP,
    TSyncItemType *aRemoteSendToLocalTypeP);
  // - init usage of datatypes set with setSendTypeInfo/setReceiveTypeInfo
  virtual localstatus initDataTypeUse(void);
  // - SYNC command bracket start (check credentials if needed)
  virtual bool engProcessSyncCmd(SmlSyncPtr_t aSyncP,TStatusCommand &aStatusCommand,bool &aQueueForLater);
  // - SYNC command bracket end (but another might follow in next message)
  virtual bool engProcessSyncCmdEnd(bool &aQueueForLater);
  // - called to process incoming item operation.
  //   Method must take ownership of syncitemP in all cases
  virtual bool engProcessRemoteItem(
    TSyncItem *syncitemP,
    TStatusCommand &aStatusCommand
  );
  #ifdef SYSYNC_SERVER
  // - called to process map commands from client to server
  virtual localstatus engProcessMap(cAppCharP aRemoteID, cAppCharP aLocalID);
  #endif
  // - must return true if this datastore is finished with <sync>
  //   (if all datastores return true,
  //   session is allowed to finish sync packet with outgoing message
  virtual bool isSyncDone(void);
  // - called at very end of sync session, when everything is done
  virtual void engFinishDataStoreSync(localstatus aErrorStatus=LOCERR_OK);

  // Abstracts of TLocalEngineDS
protected:
  // check if all subdatastores can restart
  bool canRestart();
  // obtain Sync Cap mask, must be lowest common mask of all subdatastores
  virtual uInt32 getSyncCapMask(void);
  // intersection of all sync mode sets in the subdatastores
  virtual bool syncModeSupported(const std::string &mode);
  virtual void getSyncModes(set<string> &modes);
  // Internal events during sync for derived classes
  // Note: local DB authorisation must be established already before calling these
  // - prepares for Sync with this datastore
  // - updates fLastRemoteAnchor,fNextRemoteAnchor,fLastLocalAnchor,fNextLocalAnchor
  // - called at sync alert, obtains anchor information from local DB
  //   (abstract, must be implemented in derived class)
  virtual localstatus engInitSyncAnchors(
    cAppCharP aDatastoreURI,      // local datastore URI
    cAppCharP aRemoteDBID         // ID of remote datastore (to find session information in local DB)
  );
  // - called at start of first <Sync> command (prepare DB for reading/writing)
  virtual bool startSync(TStatusCommand &aStatusCommand);
  /// check is datastore is completely started.
  /// @param[in] aWait if set, call will not return until either started state is reached
  ///   or cannot be reached within the maximally allowed request processing time left.
  virtual bool engIsStarted(bool aWait);
  // - only dummy, creates error if called
  virtual bool logicRetrieveItemByID(
    TSyncItem &aSyncItem, // item to be filled with data from server. Local or Remote ID must already be set
    TStatusCommand &aStatusCommand // status, must be set on error or non-200-status
  );
  // - only dummy, creates error if called
  virtual bool logicProcessRemoteItem(
    TSyncItem *syncitemP,
    TStatusCommand &aStatusCommand,
    bool &aVisibleInSyncset, // on entry: tells if resulting item SHOULD be visible; on exit: set if processed item remains visible in the sync set.
    string *aGUID=NULL // GUID is stored here if not NULL
  );
  // - returns true if DB implementation can filter during database fetch
  //   (otherwise, fetched items will be filtered after being read from DB)
  virtual bool engFilteredFetchesFromDB(bool aFilterChanged=false);
  // - called for SyncML 1.1 if remote wants number of changes.
  //   Must return -1 if no NOC value can be returned
  virtual sInt32 getNumberOfChanges(void);
  // - called to show sync statistics in debug log and on console
  SUPERDS_VIRTUAL void showStatistics(void);
  // - called to generate sync sub-commands
  //   Returns true if now finished for this datastore
  //   also sets fState to dss_syncdone(server)/dss_syncready(client) when finished
  virtual bool engGenerateSyncCommands(
    TSmlCommandPContainer &aNextMessageCommands,
    TSmlCommand * &aInterruptedCommandP,
    const char *aLocalIDPrefix
  );
  // Suspend/Resume
  // - helper to save resume state either at end of request or explicitly at reception of a "suspend"
  //   (overridden only by superdatastore)
  virtual localstatus engSaveSuspendState(bool aAnyway);
  // - returns true if DB implementation supports resume (saving of resume marks, alert code, pending maps, tempGUIDs)
  virtual bool dsResumeSupportedInDB(void);
  #ifdef SYSYNC_CLIENT
  // Client only: initialize Sync alert for datastore according to Parameters set with dsSetClientSyncParams()
  /// @note initializes anchors and makes calls to isFirstTimeSync() valid
  virtual localstatus engPrepareClientSyncAlert(void);
  // Client only: init engine for client sync (superdatastore aware)
  virtual localstatus engInitForClientSync(void);
  // Client only: called to generate Map items
  // - Returns true if now finished for this datastore
  // - also sets fState to dss_done when finished
  virtual bool engGenerateMapItems(TMapCommand *aMapCommandP, cAppCharP aLocalIDPrefix);
  // Client only: returns number of unsent map items
  virtual sInt32 numUnsentMaps(void);
  /// client: called to generate sync sub-commands to be sent to server
  /// Returns true if now finished for this datastore
  /// also sets fState to dss_syncdone(server)/dss_syncready(client) when finished
  virtual bool logicGenerateSyncCommandsAsClient(
    TSmlCommandPContainer &aNextMessageCommands,
    TSmlCommand * &aInterruptedCommandP,
    const char *aLocalIDPrefix=NULL
  ) { return LOCERR_NOTIMP; };
  // Client only: called to mark maps confirmed, that is, we have received ok status for them
  virtual void engMarkMapConfirmed(cAppCharP aLocalID, cAppCharP aRemoteID);
  #endif // SYSYNC_CLIENT
  #ifdef SYSYNC_SERVER
  // Server only:
  /// server: called to generate sync sub-commands to be sent to client
  /// Returns true if now finished for this datastore
  /// also sets fState to dss_syncdone(server)/dss_syncready(client) when finished
  virtual bool logicGenerateSyncCommandsAsServer(
    TSmlCommandPContainer &aNextMessageCommands,
    TSmlCommand * &aInterruptedCommandP,
    const char *aLocalIDPrefix=NULL
  ) { return LOCERR_NOTIMP; };
  // - only dummy, creates error if called
  virtual localstatus logicProcessMap(cAppCharP aRemoteID, cAppCharP aLocalID) { return false; }; // dummy, should never be called
  // - called at end of request processing in server
  virtual void engRequestEnded(void);
  // Dummies, should never be called in Superdatastore, as all DB processing takes
  // place in subdatastores
  // - check if conflicting item already exist in list of items-to-be-sent-to-client
  virtual TSyncItem *getConflictingItemByRemoteID(TSyncItem *syncitemP) { return NULL; };
  virtual TSyncItem *getConflictingItemByLocalID(TSyncItem *syncitemP) { return NULL; };
  // - called to check if content-matching item from server exists
  virtual TSyncItem *getMatchingItem(TSyncItem *syncitemP, TEqualityMode aEqMode) { return NULL; };
  // - called to prevent item to be sent to client in subsequent engGenerateSyncCommands()
  //   item in question should be an item that was returned by getConflictingItemByRemoteID() or getMatchingItem()
  virtual void dontSendItemAsServer(TSyncItem *syncitemP) {};
  // - called when a item in the sync set changes its localID (due to local DB internals)
  //   NOTE: this is not called for superdatastores, only real datastores, so it's NOP here
  virtual void dsLocalIdHasChanged(const char *aOldID, const char *aNewID) {};
  // - called to have additional item sent to remote (DB takes ownership of item)
  virtual void SendItemAsServer(TSyncItem *aSyncitemP) {};
  #endif // SYSYNC_SERVER
  /// called to have all non-yet-generated sync commands as "to-be-resumed"
  virtual void logicMarkOnlyUngeneratedForResume(void) {};
  /// called to mark an already generated (but probably not sent or not yet statused) item
  /// as "to-be-resumed", by localID or remoteID (latter only in server case).
  /// @note This must be repeatable without side effects, as server must mark/save suspend state
  ///       after every request (and not just at end of session)
  virtual void logicMarkItemForResume(cAppCharP aLocalID, cAppCharP aRemoteID, bool aUnSent) {};
  /// called to mark an already generated (but probably not sent or not yet statused) item
  /// as "to-be-resumed", by localID or remoteID (latter only in server case).
  /// @note This must be repeatable without side effects, as server must mark/save suspend state
  ///       after every request (and not just at end of session)
  virtual void logicMarkItemForResend(cAppCharP aLocalID, cAppCharP aRemoteID) {};
  /// save status information required to possibly perform a resume (as passed to datastore with
  /// markOnlyUngeneratedForResume() and markItemForResume())
  /// (or, in case the session is really complete, make sure that no resume state is left)
  /// @note Must also save tempGUIDs (for server) and pending/unconfirmed maps (for client)
  virtual localstatus logicSaveResumeMarks(void) { return LOCERR_NOTIMP; }; // must be derived (or avoided, as in superdatastore)
public:
  /// remove prefix for given subDatastore
  /// @param[in] aIDWithPrefix points to ID with prefix
  /// @return NULL if not found
  /// @return pointer to first char in aIDWithPrefix which is not part of the prefix
  cAppCharP removeSubDSPrefix(cAppCharP aIDWithPrefix, TLocalEngineDS *aLocalDatastoreP);
private:
  // find subdatastore which matches prefix of given localID
  TSubDatastoreLink *findSubLinkByLocalID(const char *aLocalID);
  // find subdatastore which can accept item data
  TSubDatastoreLink *findSubLinkByData(TSyncItem &aSyncItem);
  /** @deprecated moved to localEngineDS
  // combined first-time sync flag (collected from subdatastores)
  bool fFirstTimeSync;
  */
  // this flag is set at alert and cleared after all subdatastores have fully initialized
  bool fSuperStartPending;
  // list of subdatastores
  TSubDSLinkList fSubDSLinks;
  // currently generating subdatastore
  TSubDSLinkList::iterator fCurrentGenDSPos;
}; // TSuperDataStore

} // namespace sysync

#endif // SUPERDATASTORES

#endif  // SuperDataStore_H

// eof
