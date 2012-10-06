/**
 *  @File     localengineds.h
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TLocalEngineDS
 *    Abstraction of the local datastore - interface class to the
 *    sync engine.
 *
 *    Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-09-15 : luz : created from localdatastore
 */

#ifndef LocalEngineDS_H
#define LocalEngineDS_H

// includes
#include "configelement.h"
#include "syncitem.h"
#include "syncdatastore.h"
#include "itemfield.h"
// more includes after config classes definitions


using namespace sysync;


namespace sysync {

extern const char * const conflictStrategyNames[];

#ifdef SCRIPT_SUPPORT
// publish as derivates might need it
extern const TFuncTable DBFuncTable;
#endif

class TLocalEngineDS; // forward


// datatype list entry
class TAdditionalDataType
{
public:
  TDataTypeConfig *datatypeconfig;
  bool forRead;
  bool forWrite;
  TTypeVariantDescriptor variantDescP;
  #ifndef NO_REMOTE_RULES
  string remoteRuleMatch; // remote rule match string
  #endif
};

// datatypes list
typedef std::list<TAdditionalDataType> TAdditionalTypesList;


#ifndef NO_REMOTE_RULES

// rule match type list entry
typedef struct {
  TSyncItemType *itemTypeP;
  cAppCharP ruleMatchString;
} TRuleMatchTypeEntry;

// container types
typedef std::list<TRuleMatchTypeEntry> TRuleMatchTypesContainer; // contains rule match item types

#endif


// type support configuration of that database
class TTypeSupportConfig : public TConfigElement
{
  typedef TConfigElement inherited;
public:
  TTypeSupportConfig(const char* aName, TConfigElement *aParentElement);
  virtual ~TTypeSupportConfig();
  // properties
  // - preferred data types
  TDataTypeConfig *fPreferredTx;
  TTypeVariantDescriptor fPrefTxVariantDescP;
  TDataTypeConfig *fPreferredRx;
  TTypeVariantDescriptor fPrefRxVariantDescP;
  // - preferred data type for blind and legacy mode sync attempts
  TDataTypeConfig *fPreferredLegacy;
  // - other data types
  TAdditionalTypesList fAdditionalTypes;
  #ifndef NO_REMOTE_RULES
  // - types that are selected when matching with a remote rule
  TAdditionalTypesList fRuleMatchTypes;
  #endif
  // public methods
  virtual void clear();
  #ifdef HARDCODED_CONFIG
  // add type support
  bool addTypeSupport(
    cAppCharP aTypeName,
    bool aForRead,
    bool aForWrite,
    bool aPreferred,
    cAppCharP aVariant,
    cAppCharP aRuleMatch=NULL
  );
  #endif
protected:
  // check config elements
  #ifndef HARDCODED_CONFIG
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  virtual void localResolve(bool aLastPass);
}; // TTypeSupportConfig


#if defined(SCRIPT_SUPPORT) && defined(SYSYNC_CLIENT)
// publish func table for chaining
extern const TFuncTable ClientDBFuncTable;
#endif

typedef list<string> TStringList;

// local datastore config
class TLocalDSConfig: public TConfigElement
{
  typedef TConfigElement inherited;
public:
  TLocalDSConfig(const char* aName, TConfigElement *aParentElement);
  virtual ~TLocalDSConfig();
  // properties
  // - ID to identify datastore instance in callbacks and derived
  //   classes implementing multiple interface variants (such as PalmDbDatastore)
  //   and to relate client (profile) target settings with datastores
  uInt32 fLocalDBTypeID;
  // - supported datatypes
  TTypeSupportConfig fTypeSupport;
  // - conflict strategy
  TConflictResolution fConflictStrategy;
  TConflictResolution fSlowSyncStrategy;
  TConflictResolution fFirstTimeStrategy;
  // - options
  bool fReadOnly;          // if set, datastore will not write any data (only maps) to local DB (but fake successful status to remote)
  bool fCanRestart;        // if set, then the datastore is able to participate in multiple sync sessions; in other words after a successful read/write cycle it is possible to restart at the reading phase
  set<string> fSyncModes;  // all additional, non-standard sync modes
#ifndef HARDCODED_CONFIG
  string fSyncModeBuffer;  // only used during XML config parsing
#endif
  bool fReportUpdates;     // if set(normal case), updates of server items will be sent to client (can be set to false for example for emails)
  bool fResendFailing;     // if set, items that receive a failure status from the remote will be resent in the next session (if DS 1.2 suspend marks supported by the DB)
  bool fDeleteWins;        // if set, in a replace/delete conflict the delete wins (also see DELETEWINS())
  #ifdef SYSYNC_SERVER
  bool fTryUpdateDeleted;  // if set, in a client update with server delete conflict, server tries to update the already deleted item (in case it is just invisible)
  bool fAlwaysSendLocalID; // always send localID to clients (which gives them opportunity to remap IDs on Replace)
  TStringList fAliasNames; // list of aliases for this datastore
  #endif // SYSYNC_SERVER
  uInt32 fMaxItemsPerMessage; // if >0, limits the number of items sent per SyncML message (useful in case of slow datastores where collecting data might exceed client timeout)
  #ifdef OBJECT_FILTERING
  // filtering
  // - filter applied to items coming from remote party, non-matching
  //   items will be rejected with 415 (unknown format) status
  //   unless fSilentlyDiscardUnaccepted is set to true (then we accept and discard silently)
  //   (outgoing items will be made pass this filter with makePassFilter)
  string fRemoteAcceptFilter;
  bool fSilentlyDiscardUnaccepted;
  // - filter will be applied to items read from the local database,
  //   IN ADDITION to possibly specified target address filtering
  string fLocalDBFilterConf;
  // - filter applied to incoming items with makePassFilter
  //   when there is no fSyncSetFilter or when applying fSyncSetFilter
  //   with makePassFilter still does not make item pass.
  string fMakePassFilter;
  // - filter to check if item is to be treated as "invisible",
  //   also applied to incoming items to make them invisible
  //   (archive-delete)
  //   %%%%% these notes are from earlier syncVisibility stuff that never
  //   %%%%% worked:
  //   - for delete items going to remote, fSyncVisibiliy=false means that Soft Delete should be
  //   - for delete items coming from remote, fSyncVisibility=false means that Archive delete is requested
  string fInvisibleFilter;
  // - filter applied to incoming items if they do pass fInvisibleFilter (i.e. would be invisible)
  string fMakeVisibleFilter;
  // - DS 1.2 Filter support (<filter> allowed in Alert, <filter-rx>/<filterCap> shown in devInf)
  bool fDS12FilterSupport;
  // - Set if date range support is available in this datastore
  bool fDateRangeSupported;
  #endif
  #ifdef SCRIPT_SUPPORT
  // General DB scripts
  string fDBInitScript;
  string fSentItemStatusScript;
  string fReceivedItemStatusScript;
  string fAlertScript;
  string fDBFinishScript;
  #ifdef SYSYNC_CLIENT
  string fAlertPrepScript;
  // function table
  virtual const TFuncTable *getClientDBFuncTable(void) { return &ClientDBFuncTable; };
  #endif // SYSYNC_CLIENT
  #endif // SCRIPT_SUPPORT
  #ifndef MINIMAL_CODE
  // display name
  string fDisplayName;
  #endif
  // public methods
  // - create appropriate datastore from config, calls addTypeSupport as well
  virtual TLocalEngineDS *newLocalDataStore(TSyncSession *aSessionP) = 0;
  // - add type support to datatstore from config
  virtual void addTypes(TLocalEngineDS *aDatastore, TSyncSession *aSessionP);
  // - add (probably datastore-specific) limits such as MaxSize and NoTruncate to types
  //   (called by addTypes), usually derived as type limits come from DS implementation
  virtual void addTypeLimits(TLocalEngineDS *aLocalDatastoreP, TSyncSession *aSessionP);
  // - reset config to defaults
  virtual void clear();
  // - check for alias names
  uInt16 isDatastoreAlias(cAppCharP aDatastoreURI);
  // - returns true for datastores that are abstract, i.e. don't have a backend implementation (like superdatastores, or non-derived localengineds)
  virtual bool isAbstractDatastore(void) { return true; }; // pure localengineds is abstract. First derivate towards backend (stdlogicds) will override this with "false".
protected:
  // check config elements
  #ifndef HARDCODED_CONFIG
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  virtual void localResolve(bool aLastPass);
}; // TLocalDSConfig


// datastore config list
typedef std::list<TLocalDSConfig *> TLocalDSList;


// forward
class TAlertCommand;
class TRemoteDataStore;
class TSyncOpCommand;

/// @brief Local Engine Datastore States
/// @Note these states are strictly in sequence as they happen during a sync session, so
///       we can use < and > comparisons to check stage of sync process
typedef enum {
  dssta_idle,                     ///< client&server, inactive, idle (after reset or creation)
  dssta_clientparamset,           ///< client only, client has received params for syncing (dsSetClientSyncParams())
  dssta_adminready,               ///< client&server, administration data ready (all last & next timestamps/anchors/ids/maps available)
  dssta_clientsentalert,          ///< client only, client has sent alert for sync to server
  dssta_serveralerted,            ///< server only, server has received alert for sync from client
  dssta_serveransweredalert,      ///< server only, server has sent status and response alert for client
  dssta_clientalertstatused,      ///< client only, client has received status for sync alert
  dssta_clientalerted,            ///< client only, client has received response alert from server
  dssta_syncmodestable,           ///< client&server, sync mode is now stable (all alerts and alert statuses are exchanged and processed), server is ready for early maps
  dssta_dataaccessstarted,        ///< client&server, user data access has started (e.g. loading of sync set in progress)
  dssta_syncsetready,             ///< client&server, sync set is ready and can be accessed with logicXXXX sync op calls
  dssta_clientsyncgenstarted,     ///< client only, generation of sync command(s) in client for server has started (possibly, we send some pending maps first)
  dssta_serverseenclientmods,     ///< server only, server has seen all client modifications now
  dssta_serversyncgenstarted,     ///< server only, generation of sync command(s) in server for client has started
  dssta_syncgendone,              ///< client&server, generation of sync command(s) is complete
  dssta_dataaccessdone,           ///< client&server, user data access has ended (no more user data reads or writes, but still maps)
  dssta_clientmapssent,           ///< client only, all maps have been sent to server
  dssta_admindone,                ///< client&server, administration data access done (all anchors, timestamps, maps and stuff saved)
  dssta_completed,                ///< client&server, sync session complete (not necessarily successfully, check isAborted()!), including showing logs etc.
  numDSStates
} TLocalEngineDSState;


typedef enum {
  pi_state_none,             ///< no partial item
  pi_state_loaded_incoming,  ///< we have an incoming item loaded from admin data (but not changed)
  pi_state_loaded_outgoing,  ///< we have an outgoing item loaded from admin data (but not changed)
  pi_state_save_incoming,    ///< we must save the incoming item at next suspend
  pi_state_save_outgoing     ///< we must save the outgoing item at next suspend
} TPartialItemState;


} // end namespace sysync


// additional includes here as they need config classes already defined
#include "synccommand.h"
#include "syncsession.h"


namespace sysync {

#ifdef SCRIPT_SUPPORT
class TScriptContext;
class TLDSfuncs;
#endif

// for engXXXX methods that need only be virtual in case we have superdatastores
#ifdef SUPERDATASTORES
  #define SUPERDS_VIRTUAL virtual
  class TSuperDataStore; // forward declaration
#else
  #define SUPERDS_VIRTUAL
  #define TSuperDataStore TLocalEngineDS // to allow parameter for engProcessSyncAlert()
#endif

/// @brief Local datastore engine and abstraction of actual implementation
/// - session and commands only call non-virtual engXXXX members of this class to
///   perform any operations on the datastore
/// - to perform actual access logic (which might be implemented in different flavors),
///   this class calls virtual (usually abstract) logicXXXX members, which are
///   implemented in derived classes.
class TLocalEngineDS: public TSyncDataStore
{
  typedef TSyncDataStore inherited;

  friend class TLDSfuncs;
  friend class TMFTypeFuncs;
  friend class TSyncSession;
  friend class TSyncCommand;
  friend class TSyncOpCommand;
  friend class TMultiFieldItemType;
  friend class TTextProfileHandler;
  friend class TSyncAgent;
  #ifdef SUPERDATASTORES
  friend class TSuperDataStore;
  #endif

protected:
  /// @name dsSyncState member fields defining the sync state of the datastore
  //
  /// @{
  TLocalEngineDSState fLocalDSState;  ///< internal state of the datastore sync process
  localstatus fAbortStatusCode;       ///< status code when engAbortDatastoreSync() was called
  bool fLocalAbortCause;              ///< flag signalling if abort cause was local or remote
  bool fRemoteAddingStopped;          ///< set when no more add commands should be sent to remote (e.g. device full)
  localstatus fAlertCode;             ///< alert code in use by this datastore (for scripts and for suspend)
  /// @}

  /// @name dsSyncMode member fields defining the sync mode of the datastore
  /// @Note These get valid for the first time in dssta_clientinit (for client) or
  ///       dssta_serveralerted (for server), but might change again due to
  ///       detection of slowsync or resume until dssta_syncmodestable is reached
  //
  /// @{
  TSyncModes fSyncMode;         ///< basic Sync mode (twoway, fromclient, fromserver)
  bool fForceSlowSync;          ///< set if external reason wants to force a slow sync even if it is not needed
  bool fSlowSync;               ///< set if slow sync or refresh
  bool fRefreshOnly;            ///< set if local data is refreshed from remote only, that is, no local changes will be sent to remote (can be set independently of apparent fSyncMode)
  bool fReadOnly;               ///< if set, datastore will not write any user data (but fake successful status to remote)
  bool fReportUpdates;          ///< if NOT set, datastore will not report updates to client (e.g. for email)
  bool fServerAlerted;          ///< set if sync was server alerted
  bool fResuming;               ///< set if this is a resume of a previous session
  #ifdef SUPERDATASTORES
  TSuperDataStore *fAsSubDatastoreOf;   ///< if set, this points to the superdatastore, and MUST NOT receive any commands from the session directly
  #endif
  /// @}

  /// @name dsSavedAdmin administrative data (anchors, timestamps, maps) as saved or to-be-saved
  /// @Note These will be loaded and saved be derived classes
  /// @Note Some of these will be updated from resp. @ref dsCurrentAdmin members at distinct events (suspend, session end, etc.)
  /// @Note Some of these will be updated during the session, but in a way that does NOT affect the anchoring of current/last session
  //
  /// @{
  // - anchors, valid at dssta_adminready
  string fLastRemoteAnchor;     ///< last remote anchor (saved remote's next anchor in DB at end of last session)
  string fLastLocalAnchor;      ///< last local anchor (saved local next anchor in DB at end of last session) @note this will be generated as
  // - suspend state
  uInt16 fResumeAlertCode;      ///< alert code saved at suspend of previous session, 0 if none
  bool fPreventResuming;        ///< can be set by running session to prevent that session can be resumed (e.g. in case of incomplete zapping at "reload device")
  // - other state info
  bool fFirstTimeSync;          ///< set to true if this is the first sync
  // - item ID lists (maps)
  #ifdef SYSYNC_SERVER
  TStringToStringMap fTempGUIDMap;      ///< container for temp GUID to real GUID mapping
  #endif
  #ifdef SYSYNC_CLIENT
  TStringToStringMap fPendingAddMaps;   ///< container for map items to be sent to server: fPendingAddMaps[localid]=remoteid; Note: might contain temporary localIDs that must be converted to final ones before using in <map> or saving with dsFinalizeLocalID()
  TStringToStringMap fUnconfirmedMaps;  ///< container for map items already sent to server, but not yet confirmed: fUnconfirmedMaps[localid]=remoteid;
  TStringToStringMap fLastSessionMaps;  ///< container for map items already confirmed, but still needed for duplicate checking:: fLastSessionMaps[localid]=remoteid;
  #endif
  // - information about last item sent or received in previously suspended session
  TSyError fLastItemStatus;               ///< final status sent for last item in previously suspended session.
  string fLastSourceURI;                ///< Source LocURI of last item sent in previously suspended session.
  string fLastTargetURI;                ///< Target LocURI of partial item
  // - resuming chunked item data (send or receive)
  TPartialItemState fPartialItemState;  ///< state of partial item vars
  uInt32 fPITotalSize;                  ///< total size of the partially transmitted item, 0 if none
  uInt32 fPIUnconfirmedSize;            ///< size of already received, but probably repeated data - or size of data pending to be (re-)sent
  uInt32 fPIStoredSize;                 ///< total size of stored data (some of it might not be confirmed)
  appPointer fPIStoredDataP;            ///< stored data, allocated with smlLibMalloc().
  bool fPIStoredDataAllocated;          ///< set if stored data was allocated by the session. Otherwise, it is owned by the currently incomplete command

  /// @}

  /// @name dsCurrentAdmin current session's admin data (anchors, timestamps, maps)
  /// @Note These will be copied to @ref dsSavedAdmin members ONLY when a session completes successfully/suspends.
  /// @Note Admin data is NEVER directly saved or loaded from these
  /// @Note Derivates will update some of these at dssta_adminready with current time/anchor values
  //
  /// @{

  // - anchors, valid at dssta_adminready
  string fNextRemoteAnchor;     ///< next remote anchor (sent at Alert by remote)
  string fNextLocalAnchor;      ///< local anchor of this session
  /// @}

  /// @name dsFiltering item and syncset filtering
  /// @Note syncset restriction filtering expressions and limits
  ///       must be evaluated by derived datastore implementations
  //
  /// @{
  #ifdef OBJECT_FILTERING
  /// dynamic filter, if set and the datastore cannot handle it directly, all records need to be read
  /// this filter is good for filter expressions that might change between sync sessions
  string fSyncSetFilter;
  /// static filter, only applied to already read items, no need to filter all items
  string fLocalDBFilter;
  #ifdef SYNCML_TAF_SUPPORT
  /// real SyncML-like Target Address Filtering (TAF), set from <sync> commands
  string fTargetAddressFilter;
  /// internal SyncML-like Target Address Filtering (TAF), set internally by scripts
  string fIntTargetAddressFilter;
  #endif // SYNCML_TAF_SUPPORT
  #ifdef SYSYNC_TARGET_OPTIONS
  // Date range
  lineartime_t fDateRangeStart; ///< date range start, 0=none, must be evaluated by derived datastore implementations
  lineartime_t fDateRangeEnd; ///< date range end, 0=none, must be evaluated by derived datastore implementations
  /// size limit, -1=none, must be evaluated by derived datastore implementations
  fieldinteger_t fSizeLimit;
  /// max item count, 0=none (for example for emails)
  uInt32 fMaxItemCount;
  /// no attachments (for example emails)
  bool fNoAttachments;
  /// datastore options, can be evaluated in scripts, obtained from /o() CGI in database path
  string fDBOptions; // free database options
  #endif // SYSYNC_TARGET_OPTIONS
  // - filtering needs, initialized by initPostFetchFiltering()
  bool fTypeFilteringNeeded;
  bool fFilteringNeeded;
  bool fFilteringNeededForAll;
  #endif // OBJECT_FILTERING
  /// @}

  /// @name dsItemProcessing item processing parameters and options
  /// @note these can be influenced by various script functions during the session
  //
  /// @{
  /// conflict strategy for this datastore in this session
  /// @note can be modified by datastoreinitscript
  TConflictResolution fSessionConflictStrategy;
  fieldinteger_t fItemSizeLimit; ///< size limit for item being processed or generated (initally=fSizeLimit) but can be changed by scripts
  TSyncOperation fEchoItemOp; ///< if not sop_none, processed item will be echoed back to remote
  TSyncOperation fCurrentSyncOp; ///< current sync-operation
  TConflictResolution fItemConflictStrategy; ///< conflict strategy for currently processed item
  bool fForceConflict; ///< if set, a conflict will be forced
  bool fDeleteWins; ///< if set, in a replace/delete conflict delete will win (regardless of strategy)
  bool fPreventAdd; ///< if set, attempt to add item from remote will cause no add but delete of remote item
  bool fIgnoreUpdate; ///< if set, attempt to update item from remote will be ignored (only adds, also implicit ones) are executed)
  sInt16 fRejectStatus; ///< if >=0, incoming item will be discarded with this status code (0=silently)
  #ifdef SCRIPT_SUPPORT
  /// @{ Type Script support (actual scripts are in the datatypes, but contexts are here as
  /// same datatype can be used in parallel in different datastores)
  TScriptContext *fSendingTypeScriptContextP;
  TScriptContext *fReceivingTypeScriptContextP; ///< @note can be NULL if sending and receiving uses the same context
  /// @}
  /// @{ generic datastore-level scripts
  TScriptContext *fDataStoreScriptContextP; // for executing status check scripts
  /// @}
  #endif
  /// @}


  /// @name dsCountStats item counting and statistics
  /// @{
  //
  /// number of changes remote will send (valid if >=0), SyncML 1.1
  sInt32 fRemoteNumberOfChanges;
  /// @{ net data transferred (item data, without any SyncML protocol overhead, meta etc.)
  sInt32 fIncomingDataBytes;
  sInt32 fOutgoingDataBytes;
  /// }@
  /// @{ locally performed ops
  sInt32 fLocalItemsAdded;
  sInt32 fLocalItemsUpdated;
  sInt32 fLocalItemsDeleted;
  sInt32 fLocalItemsError; // items that caused an error to be sent to the remote (and SHOULD be sent again by the remote, or abort the session)
  /// }@
  // @{ remotely performed ops
  sInt32 fRemoteItemsAdded;
  sInt32 fRemoteItemsUpdated;
  sInt32 fRemoteItemsDeleted;
  sInt32 fRemoteItemsError; // items that had a remote error (and will be resent later)
  /// }@
  #ifdef SYSYNC_SERVER
  // @{ conflicts
  sInt32 fConflictsServerWins;
  sInt32 fConflictsClientWins;
  sInt32 fConflictsDuplicated;
  /// slow sync matches
  sInt32 fSlowSyncMatches;
  #endif
  /// @{ item transmission counts
  sInt32 fItemsSent;
  sInt32 fItemsReceived;
  /// }@
  /// @}

private:
  /// @name dsOther other members needed for handling the sync
  /// @{
  /// datastore config pointer
  TLocalDSConfig *fDSConfigP;
  #ifdef SYSYNC_CLIENT
  // parameters for syncing with remote DB as client
  // - local
  string fLocalDBPath;              ///< client, entire path to the local database, which might include subpaths/CGI to subselect records in the local DB
  // - remote
  string fDBUser;                   ///< client, DB layer user name
  string fDBPassword;               ///< client, DB layer password
  string fRemoteRecordFilterQuery;  ///< client, record level filter query
  bool fRemoteFilterInclusive;      ///< client, inclusive filter flag
  #endif // SYSYNC_CLIENT
  string fRemoteDBPath;             ///< server and client, path of remote DB, for documentary purposes
  string fIdentifyingDBName;        ///< server and client, name with which the to-be-synced DB is identified (important in case of aliases in server)
  /// remote datastore involved, valid after processSyncCmdAsServer()
  TRemoteDataStore *fRemoteDatastoreP;
  /// Remote view if local URI
  /// - Server case: URI how remote has accessed local, for sending Sync commands back and to
  ///   create devInf datastore name when devInf is queried AFTER being alerted from client
  /// - Client case: URI of local DB (only used to match commands coming back)
  string fRemoteViewOfLocalURI;
  /// time when datastore terminated sync, for statistics only
  lineartime_t fEndOfSyncTime;
  // default types of remote datastore for sending/receiving during Sync
  TSyncItemType *fRemoteReceiveFromLocalTypeP;  ///< type used by remote to receive from local
  TSyncItemType *fRemoteSendToLocalTypeP;       ///< type used by remote to send to local
  TSyncItemType *fLocalSendToRemoteTypeP;       ///< type used by local to send to remote
  TSyncItemType *fLocalReceiveFromRemoteTypeP;  ///< type used by local to receive from remote
  /// @}

private:
  /// reset for re-use without re-creation, used by constructor as well
  void InternalResetDataStore(void);
public:
  /// constructor
  TLocalEngineDS(TLocalDSConfig *aDSConfigP, TSyncSession *aSessionP, cAppCharP aName, uInt32 aCommonSyncCapMask=0);
  /// destructor
  virtual ~TLocalEngineDS();

  /// @name dsProperty property and state querying methods
  /// @{
  #ifndef MINIMAL_CODE
  // - return display name (descriptive)
  virtual const char *getDisplayName(void)
    { return fDSConfigP->fDisplayName.empty() ? getName() : fDSConfigP->fDisplayName.c_str(); };
  #endif
  TLocalEngineDSState getDSState(void) { return fLocalDSState; }
  #ifdef SYDEBUG
  cAppCharP getDSStateName(void);
  static cAppCharP getDSStateName(TLocalEngineDSState aState);
  bool dbgTestState(TLocalEngineDSState aMinState, bool aNeedExactMatch=false)
    { return aNeedExactMatch ? fLocalDSState==aMinState : fLocalDSState>=aMinState; };
  #endif
  // get name with which this datastore was identified by the remote
  cAppCharP getIdentifyingName(void) { return fIdentifyingDBName.c_str(); }
  /// calculate Sync mode and flags from given alert code
  /// @note does not affect DS state in any way, nor checks if DS can handle this code.
  ///   (this will be done only when calling setSyncMode())
  static localstatus getSyncModeFromAlertCode(TSyError aAlertCode, TSyncModes &aSyncMode, bool &aIsSlowSync, bool &aIsServerAlerted);
  /// determine if this is a first time sync situation
  bool isFirstTimeSync(void) { return fFirstTimeSync; };
  /// check if sync is started (e.g. background loading of syncset)
  virtual bool isStarted(bool aWait) { return fLocalDSState>=dssta_dataaccessstarted; }; // single thread version is started when data access is started (and never waits)
  // called for >=SyncML 1.1 if remote wants number of changes.
  // Must return -1 if no NOC value can be returned
  virtual sInt32 getNumberOfChanges(void) { return -1; /* no NOC supported */ };
  /// Called at end of sync to determine whether the store already knows
  /// that it has more changes for the server in the next sync session.
  /// For example, the TBinFileImplDS looks at its change log to determine that.
  virtual bool hasPendingChangesForNextSync() { return false; }
  /// test abort status, datastore is aborted also when session is just suspended
  bool isAborted(void);
  /// abort status code with local error code prefix if cause was local
  TSyError getAbortStatusCode(void) { return fLocalAbortCause ? localError(fAbortStatusCode) : syncmlError(fAbortStatusCode); };
  /// test if active
  bool isActive(void) { return fLocalDSState!=dssta_idle && !isAborted(); }; // test active-in-sync status
  /// Returns true if this datastore is done with <sync> commands (but not necessarily done with maps)
  virtual bool isSyncDone(void);
  lineartime_t getEndOfSyncTime() { return fEndOfSyncTime; }; // documentary only!
  // - sync mode
  TSyncModes getSyncMode(void) { return fSyncMode; }; ///< get sync mode
  bool isSlowSync(void) { return fSlowSync; }; ///< true if slow sync
  bool isResuming(void) { return fResuming; }; ///< true if resuming a previous session
  bool isRefreshOnly(void) { return fRefreshOnly; }; ///< true if only refreshing with data from remote (no send to remote)
  bool isReadOnly(void) { return fReadOnly; }; ///< true if only reading from local datastore (and ignoring any updates from remote)
  /// get remote datastore related to this datastore
  TRemoteDataStore *getRemoteDatastore(void) { return fRemoteDatastoreP; };
  /// return remote view of local URI (might be different from what we might think it is locally)
  cAppCharP getRemoteViewOfLocalURI(void) { return fRemoteViewOfLocalURI.c_str(); };
  /// check if this datastore is accessible with given URI
  /// @note By default, local datastore type is addressed with
  ///       first path element of URI, rest of path might be used
  ///       by derivates to subselect data folders etc.
  /// @note returns number of characters matched to allow searching to
  ///       detect "better" matches (Oracle server case with /Calendar AND /Calendar/Events)
  virtual uInt16 isDatastore(const char *aDatastoreURI);
  /// returns true if this datastore is currently in use as subdatastore of a superdatastore
  #ifdef SUPERDATASTORES
  bool isSubDatastore(void) { return fAsSubDatastoreOf!=NULL; };
  #else
  bool isSubDatastore(void) { return false; };
  #endif
  // - data type information
  TSyncItemType *getLocalSendType(void) { return fLocalSendToRemoteTypeP; };            ///< type used by local to send to remote
  TSyncItemType *getLocalReceiveType(void) { return fLocalReceiveFromRemoteTypeP; };    ///< type used by local to receive from remote
  TSyncItemType *getRemoteSendType(void) { return fRemoteSendToLocalTypeP; };           ///< type used by remote to send to local
  TSyncItemType *getRemoteReceiveType(void) { return fRemoteReceiveFromLocalTypeP; };   ///< type used by remote to receive from local
  /// get remote DB path
  cAppCharP getRemoteDBPath(void) { return fRemoteDBPath.c_str(); };
  #ifdef SYSYNC_CLIENT
  /// get local DB path
  cAppCharP getLocalDBPath(void) { return fLocalDBPath.c_str(); };
  #endif
  /// get datastore config pointer
  TLocalDSConfig *getDSConfig(void) { return fDSConfigP; };
  /// datastore options
  fieldinteger_t getItemSizeLimit(void) { return fItemSizeLimit; };
  bool getNoAttachments(void) {
    #ifdef SYSYNC_TARGET_OPTIONS
    return fNoAttachments;
    #else
    return false;
    #endif
  };

  /// @}

  /// @name engXXXX methods defining the interface to other engine components (session, commands)
  ///   Only these will be called by the engine - the engine will no longer call directly into logicXXXX
  /// @Note some of these are virtuals ONLY for being derived by superdatastore, NEVER by locic or other derivates
  ///   We use the SUPERDS_VIRTUAL macro for these, which is empty in case we don't have superdatastores, then
  ///   these can be non-virtual.
  /// @{
  //
  /// reset datastore, but does not end sync nicely
  /// @note is virtual because derived from TSyncDataStore
  virtual void engResetDataStore(void);
  /// flag that sync with this datastore is aborted. Datastore is not yet terminated by this.
  /// @note calling engAbortDatastoreSync should not cause large activities - all terminating
  ///  activity should be in engTerminateSync(), which will be called later
  virtual void engAbortDataStoreSync(TSyError aReason, bool aLocalProblem, bool aResumable=true);
  /// Finish all activity with this datastore
  /// @note functionality is that of former TLocalDataStore::endOfSync()
  /// @note if not yet aborted with other reason, datastore now gets aborted with given aErrorStatus
  /// @note the datastore should perform all activity needed to end the sync
  SUPERDS_VIRTUAL void engFinishDataStoreSync(localstatus aErrorStatus=LOCERR_OK);
  /// Terminate all activity with this datastore
  /// @note may be called repeatedly, relevant shutdown code is executed only once
  void engTerminateDatastore(localstatus aAbortStatusCode=408); // default to timeout
  /// called at end of incoming message for all localEngineDS (super or not)
  /// @note is not virtual, as superdatastore MUST NOT implement it
  void engEndOfMessage(void) { dsEndOfMessage(); }; // simply call protected virtual chain
  /// force slow sync
  void engForceSlowSync(void) { fForceSlowSync=true; };
  /// set refresh only mode (do not send to remote)
  void engSetRefreshOnly(bool b) { fRefreshOnly=b; };
  /// set read only mode (do not receive from remote)
  void engSetReadOnly(bool b) { fReadOnly=b; };
  /// can be called to avoid further ADD commands to be sent to remote (device full case, e.g.)
  void engStopAddingToRemote(void) { fRemoteAddingStopped=true; };
  /// can be called to prevent that current session can be resumed later (e.g. when user aborts zapping datastore in client)
  void preventResuming(void) { fPreventResuming=true; }
  #ifdef OBJECT_FILTERING
  /// returns true if DB implementation can filter the standard filters
  /// (LocalDBFilter, TargetFilter and InvisibleFilter) during database fetch -
  /// otherwise, fetched items will be filtered after being read from DB.
  /// Defined as abstract here to make sure derived datastores do implement filtering
  SUPERDS_VIRTUAL bool engFilteredFetchesFromDB(bool aFilterChanged=false) { return dsFilteredFetchesFromDB(aFilterChanged); };
  #endif
  // called for >=SyncML 1.1 if remote wants number of changes.
  // Must return -1 if no NOC value can be returned
  SUPERDS_VIRTUAL sInt32 engGetNumberOfChanges(void) { return getNumberOfChanges(); };
  /// Set remote datastore for local datastore
  /// (must be called before engProcessSyncCmd and engGenerateSyncCmd)
  SUPERDS_VIRTUAL void engSetRemoteDatastore(TRemoteDataStore *aRemoteDatastoreP);

  #ifdef SYSYNC_CLIENT
  /// initialize Sync alert for datastore according to Parameters set with dsSetClientSyncParams()
  /// @note initializes anchors and makes calls to isFirstTimeSync() valid
  SUPERDS_VIRTUAL localstatus engPrepareClientSyncAlert(void);
  /// internal helper to be called by engPrepareClientSyncAlert() from this class and from superdatastore
  localstatus engPrepareClientDSForAlert(void);
  /// generate Sync alert for datastore
  /// @note this could be repeatedly called due to auth failures at beginning of session
  /// @note this is a NOP for subDatastores (should not be called in this case, anyway)
  localstatus engGenerateClientSyncAlert(TAlertCommand *&aAlertCommandP);
  // Init engine for client sync (superdatastore aware)
  // - determine types to exchange
  // - make sync set ready
  SUPERDS_VIRTUAL localstatus engInitForClientSync(void);
  // - non-superdatastore aware base functionality
  localstatus engInitDSForClientSync(void);
  #endif
  /// Internal events during sync for derived classes
  /// @note local DB authorisation must be established already before calling these
  /// - cause loading of all session anchoring info and other admin data (logicMakeAdminReady())
  ///   fLastRemoteAnchor,fLastLocalAnchor,fNextLocalAnchor; isFirstTimeSync() will be valid after the call
  /// - in case of superdatastore, consolidates the anchor information from the subdatastores
  SUPERDS_VIRTUAL localstatus engInitSyncAnchors(
    cAppCharP aDatastoreURI,      ///< local datastore URI
    cAppCharP aRemoteDBID         ///< ID of remote datastore (to find session information in local DB)
  );
  /// process Sync alert for a datastore
  SUPERDS_VIRTUAL TAlertCommand *engProcessSyncAlert(
    TSuperDataStore *aAsSubDatastoreOf, ///< if acting as subdatastore, this is the pointer to the superdatastore
    uInt16 aAlertCode,                  ///< the alert code
    const char *aLastRemoteAnchor,      ///< last anchor of client
    const char *aNextRemoteAnchor,      ///< next anchor of client
    const char *aTargetURI,             ///< target URI as sent by remote, no processing at all
    const char *aIdentifyingTargetURI,  ///< target URI that was used to identify datastore
    const char *aTargetURIOptions,      ///< option string contained in target URI
    SmlFilterPtr_t aTargetFilter,       ///< DS 1.2 filter, NULL if none
    const char *aSourceURI,             ///< source URI
    TStatusCommand &aStatusCommand      ///< status that might be modified
  );
  /// process status received for sync alert
  SUPERDS_VIRTUAL bool engHandleAlertStatus(TSyError aStatusCode);
  /// parse target address options
  /// @note this will be called BEFORE engInitForSyncOps()
  localstatus engParseOptions(
    const char *aTargetURIOptions,      ///< option string contained in target URI
    bool aFromSyncCommand=false         ///< must be set when parsing options from <sync> target URI
  );
  /// reset all filter settings
  void resetFiltering(void);
  /// process SyncML 1.2 style filter
  localstatus engProcessDS12Filter(SmlFilterPtr_t aTargetFilter);
  /// initialize reception of syncop commands for datastore
  /// @note previously, this was implemented as initLocalDatastoreSync in syncsession
  localstatus engInitForSyncOps(
    const char *aRemoteDatastoreURI ///< URI of remote datastore
  );
  /// called from <sync> command to generate sync sub-commands to be sent to remote
  /// Returns true if now finished for this datastore
  /// also changes state to dssta_syncgendone when all sync commands have been generated
  SUPERDS_VIRTUAL bool engGenerateSyncCommands(
    TSmlCommandPContainer &aNextMessageCommands,
    TSmlCommand * &aInterruptedCommandP,
    const char *aLocalIDPrefix=NULL
  );
  #ifdef SYSYNC_CLIENT
  /// client only: called whenever outgoing Message of Sync Package starts
  /// @note should start a sync command for all alerted datastores
  SUPERDS_VIRTUAL void engClientStartOfSyncMessage(void);
  /// Client only: called whenever outgoing Message starts that may contain <Map> commands
  /// @param[in] aNotYetInMapPackage set if we are still in sync-from-server package
  /// @note usually, client starts sending maps while still receiving syncops from server
  void engClientStartOfMapMessage(bool aNotYetInMapPackage);
  /// Client only: returns number of unsent map items
  SUPERDS_VIRTUAL sInt32 numUnsentMaps(void);
  #endif // SYSYNC_CLIENT
  #ifdef SYSYNC_SERVER
  /// server only: called whenever we should start a sync command for all alerted datastores
  /// if not already started
  void engServerStartOfSyncMessage(void);
  /// called to process map commands from client to server
  /// @note aLocalID or aRemoteID can be NULL - which signifies deletion of a map entry
  SUPERDS_VIRTUAL localstatus engProcessMap(cAppCharP aRemoteID, cAppCharP aLocalID);
  #endif // SYSYNC_SERVER
  /// check is datastore is completely started.
  /// @param[in] aWait if set, call will not return until either started state is reached
  ///   or cannot be reached within the maximally allowed request processing time left.
  SUPERDS_VIRTUAL bool engIsStarted(bool aWait) { return isStarted(aWait); };
  /// SYNC command bracket start (check credentials if needed)
  SUPERDS_VIRTUAL bool engProcessSyncCmd(
    SmlSyncPtr_t aSyncP,                // the Sync element
    TStatusCommand &aStatusCommand,     // status that might be modified
    bool &aQueueForLater // will be set if command must be queued for later (re-)execution
  );
  /// SYNC command bracket end (but another might follow in next message)
  SUPERDS_VIRTUAL bool engProcessSyncCmdEnd(bool &aQueueForLater);
  /// called whenever Message of Sync Package ends or after last queued Sync command is executed
  void engEndOfSyncFromRemote(bool aEndOfAllSyncCommands);
  /// process SyncML SyncOp command for this datastore
  /// @return true if regular processing (status can still be non-200!)
  /// @note this routine does the common pre-processing like conversion to internal item format
  ///       and type checking. The actual work is then done by @ref engProcessRemoteItemAsClient
  ///       or @ref engProcessRemoteItemAsServer
  bool engProcessSyncOpItem(
    TSyncOperation aSyncOp,        // the operation
    SmlItemPtr_t aItemP,           // the item to be processed
    SmlMetInfMetInfPtr_t aMetaP,   // command-wide meta, if any
    TStatusCommand &aStatusCommand // pre-set 200 status, can be modified in case of errors
  );

  /// @{
  /// process sync operation from remote with specified sync item
  /// (according to current sync mode of local datastore)
  /// @note
  /// - returns true (and unmodified or non-200-successful status) if
  ///   operation could be processed regularily
  /// - returns false (but probably still successful status) if
  ///   operation was processed with internal irregularities, such as
  ///   trying to delete non-existant item in datastore with
  ///   incomplete Rollbacks (which returns status 200 in this case!).
  #ifdef SYSYNC_CLIENT
  bool engProcessRemoteItemAsClient(
    TSyncItem *aSyncItemP, ///< the item to be processed
    TStatusCommand &aStatusCommand ///< status, must be set to correct status code (ok / error)
  );
  #endif
  #ifdef SYSYNC_SERVER
  bool engProcessRemoteItemAsServer(
    TSyncItem *aSyncItemP, ///< the item to be processed
    TStatusCommand &aStatusCommand ///< status, must be set to correct status code (ok / error)
  );
  #endif
  SUPERDS_VIRTUAL bool engProcessRemoteItem(
    TSyncItem *syncitemP,
    TStatusCommand &aStatusCommand
  );
  /// handle status of sync operation
  bool engHandleSyncOpStatus(TStatusCommand *aStatusCmdP,TSyncOpCommand *aSyncOpCmdP);
  /// called to mark maps confirmed, that is, we have received ok status for them
  #ifdef SYSYNC_CLIENT
  SUPERDS_VIRTUAL void engMarkMapConfirmed(cAppCharP aLocalID, cAppCharP aRemoteID);
  /// called to generate Map items
  /// @note Returns true if now finished for this datastore
  /// @note also sets fState to done when finished
  SUPERDS_VIRTUAL bool engGenerateMapItems(TMapCommand *aMapCommandP, cAppCharP aLocalIDPrefix);
  /// Client only: Check if the remoteid was used by an add command not
  /// fully mapped&confirmed in the previous session
  bool isAddFromLastSession(cAppCharP aRemoteID);
  #endif
  /// called to mark an already generated (but probably not sent or not yet statused) item
  /// as "to-be-resumed", by localID or remoteID (latter only in server case).
  /// @note This must be repeatable without side effects, as server must mark/save suspend state
  ///       after every request (and not just at end of session)
  void engMarkItemForResume(cAppCharP aLocalID, cAppCharP aRemoteID, bool aUnSent);
  // - called to mark an already generated (but probably not sent or not yet statused) item
  //   as "to-be-resent", by localID or remoteID (latter only in server case).
  void engMarkItemForResend(cAppCharP aLocalID, cAppCharP aRemoteID);


  // - Set types to be used for sending and receiving items
  SUPERDS_VIRTUAL void setSendTypeInfo(
    TSyncItemType *aLocalSendToRemoteTypeP,
    TSyncItemType *aRemoteReceiveFromLocalTypeP
  );
  SUPERDS_VIRTUAL void setReceiveTypeInfo(
    TSyncItemType *aLocalReceiveFromRemoteTypeP,
    TSyncItemType *aRemoteSendToLocalTypeP
  );
  // - init usage of datatypes set with setSendTypeInfo/setReceiveTypeInfo
  SUPERDS_VIRTUAL localstatus initDataTypeUse(void);
  // "modern" filtering (in contrast to the "old" fSyncSetFilter et.al. stuff)
  // - init filtering and check if needed (sets fFilteringNeeded and fFilteringNeededForAll)
  void initPostFetchFiltering(void);
  // - check if item passes filter and probably apply some modifications to it
  bool postFetchFiltering(TSyncItem *aSyncItemP);
  // add filter keywords and property names to filterCap
  void addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps);
  #ifdef OBJECT_FILTERING
  /* %%% outdated
  // - converts CGI-style filter string to internal filter string syntax
  //   and expands special functions
  const char *filterCGIToString(cAppCharP aCGI, string &aFilter);
  */
  /// @brief parse "syncml:filtertype-cgi" filter, convert into internal filter syntax
  ///  and possibly sets some special filter options (fDateRangeStart, fDateRangeEnd)
  ///  based on "filterkeywords" available for the type passed (DS 1.2).
  ///  For parsing DS 1.1/1.0 TAF-style filters, aItemType can be NULL, no type-specific
  ///  filterkeywords can be parsed then.
  /// @return pointer to next character after processing (usually points to terminator)
  /// @param[in] aCGI the NUL-terminated filter string
  /// @param[in] aItemTypeP if not NULL, this is the item type the filter applies to
  const char *parseFilterCGI(cAppCharP aCGI, TSyncItemType *aItemTypeP, string &aFilter);
  /// @brief check single filter term for DS 1.2 filterkeywords.
  /// @return true if term still needs to be added to normal filter expression, false if term will be handled otherwise
  virtual bool checkFilterkeywordTerm(
    cAppCharP aIdent, bool aAssignToMakeTrue,
    cAppCharP aOp, bool aCaseInsensitive,
    cAppCharP aVal, bool aSpecialValue,
    TSyncItemType *aItemTypeP
  );
  // - called to check if incoming item passes acception filters
  bool isAcceptable(TSyncItem *aSyncItemP, TStatusCommand &aStatusCommand);
  // - called to make incoming item pass sync set filtering
  bool makePassSyncSetFilter(TSyncItem *aSyncItemP);
  /// @brief called to make incoming item visible
  /// @return true if now visible
  bool makeVisible(TSyncItem *aSyncItemP);
  /// @brief called to make incoming item INvisible
  /// @return true if now INvisible
  bool makeInvisible(TSyncItem *aSyncItemP);
  #endif
  // log datastore sync result
  // - Called at end of sync with this datastore
  virtual void dsLogSyncResult(void);
  // add support for more data types
  // (for programatically creating local datastores from specialized TSyncSession derivates)
  void addTypeSupport(TSyncItemType *aItemTypeP,bool aForRx=true, bool aForTx=true);
  #ifndef NO_REMOTE_RULES
  // add data type that overrides normal type selection if string matches active remote rule
  void addRuleMatchTypeSupport(TSyncItemType *aItemTypeP,cAppCharP aRuleMatchString);
  #endif
  // get usage variant for a specified type usage
  virtual TTypeVariantDescriptor getVariantDescForType(TSyncItemType *aItemTypeP);
  #ifdef SYSYNC_SERVER
  /// called at end of request processing in server (derived only by superdatastore)
  SUPERDS_VIRTUAL void engRequestEnded(void);
  /// end map operation (derived class might want to rollback)
  bool MapFinishAsServer(
    bool aDoCommit,                // if not set, entire map operation must be undone
    TStatusCommand &aStatusCommand // status, must be set on error or non-200-status
  ) { return true; }
  #endif
  /// @}

  #ifdef DBAPI_TUNNEL_SUPPORT
  /// @name TunnelXXX methods allowing abstracted access to datastores from UIApi from within a tunnel session
  /// @{
  //

  virtual TSyError TunnelStartDataRead(cAppCharP aLastToken, cAppCharP aResumeToken) { return LOCERR_NOTIMP; };
  virtual TSyError TunnelReadNextItem(ItemID aID, appCharP *aItemData, sInt32 *aStatus, bool aFirst) { return LOCERR_NOTIMP; };
  virtual TSyError TunnelReadItem(cItemID aID, appCharP *aItemData) { return LOCERR_NOTIMP; };
  virtual TSyError TunnelEndDataRead() { return LOCERR_NOTIMP; };
  virtual TSyError TunnelStartDataWrite() { return LOCERR_NOTIMP; };
  virtual TSyError TunnelInsertItem(cAppCharP aItemData, ItemID aID) { return LOCERR_NOTIMP; };
  virtual TSyError TunnelUpdateItem(cAppCharP aItemData, cItemID aID, ItemID aUpdID) { return LOCERR_NOTIMP; };
  virtual TSyError TunnelMoveItem(cItemID aID, cAppCharP aNewParID) { return LOCERR_NOTIMP; };
  virtual TSyError TunnelDeleteItem(cItemID aID) { return LOCERR_NOTIMP; };
  virtual TSyError TunnelEndDataWrite(bool aSuccess, appCharP *aNewToken) { return LOCERR_NOTIMP; };
  virtual void     TunnelDisposeObj(void* aMemory) { return; };

  virtual TSyError TunnelReadNextItemAsKey(ItemID aID, KeyH aItemKey, sInt32 *aStatus, bool aFirst) { return LOCERR_NOTIMP; };
  virtual TSyError TunnelReadItemAsKey(cItemID aID, KeyH aItemKey) { return LOCERR_NOTIMP; };
  virtual TSyError TunnelInsertItemAsKey(KeyH aItemKey, ItemID aID) { return LOCERR_NOTIMP; };
  virtual TSyError TunnelUpdateItemAsKey(KeyH aItemKey, cItemID aID, ItemID aUpdID) { return LOCERR_NOTIMP; };

  virtual TSettingsKeyImpl *newTunnelKey(TEngineInterface *) { return NULL; };

  /// @}
  #endif

  /// @name dsXXXX (usually abstract) virtuals defining the interface to derived datastore classes (logic, implementation, api)
  ///   These are usually designed such that they should always call inherited::dsXXX to let the entire chain
  ///   of ancestors see the calls
  /// @{
  //
public:
  /// alert possible thread change
  virtual void dsThreadMayChangeNow(void) {}; // nop at this level
  #ifdef SYSYNC_CLIENT
  /// set Sync Parameters. Derivates might override this to pre-process and modify parameters
  /// (such as adding client settings as CGI to remoteDBPath)
  virtual bool dsSetClientSyncParams(
    TSyncModes aSyncMode,
    bool aSlowSync,
    const char *aRemoteDBPath,
    const char *aDBUser = NULL,
    const char *aDBPassword = NULL,
    const char *aLocalPathExtension = NULL,
    const char *aRecordFilterQuery = NULL,
    bool aFilterInclusive = false
  );
  #endif // SYSYNC_CLIENT
protected:
  /// reset datastore to a re-usable, like new-created state.
  virtual void dsResetDataStore(void) {};
  /// make sure datastore does not need agent any more.
  /// @note Called while agent is still fully ok, so we must clean up such
  /// that later call of destructor does NOT access agent any more
  virtual void announceAgentDestruction(void) { engTerminateDatastore(); };
  /// abort datastore (no reset yet, everything is just frozen as it is)
  virtual void dsAbortDatastoreSync(TSyError aStatusCode, bool aLocalProblem) {}; // nop here
  /// inform everyone of coming state change
  virtual localstatus dsBeforeStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState);
  /// inform everyone of happened state change
  virtual localstatus dsAfterStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState);
  #ifdef SYSYNC_CLIENT
  /// finalize local ID (for datastores that can't efficiently produce these at insert)
  virtual bool dsFinalizeLocalID(string &aLocalID) { return false; /* no change, ID is ok */};
  #endif
  #ifdef OBJECT_FILTERING
  /// tests if DB implementation can filter the standard filters
  /// (LocalDBFilter, TargetFilter and InvisibleFilter) during database fetch -
  /// otherwise, fetched items will be filtered after being read from DB.
  /// Defined as abstract here to make sure derived datastores do implement filtering
  /// @param[in] aFilterChanged if true, this signals to the datastore that filter expression might have changed in between
  virtual bool dsFilteredFetchesFromDB(bool aFilterChanged=false) { return false; }; // datastores can't do this by default
  /// returns true if DB implementation can also apply special filters like CGI-options
  /// /dr(x,y) etc. during fetching
  virtual bool dsOptionFilterFetchesFromDB(void) { return false; }; // datastores can't do this by default
  #endif
  /// returns true if DB implementation supports resume (saving of resume marks, alert code, pending maps, tempGUIDs)
  virtual bool dsResumeSupportedInDB(void) { return false; };
  /// returns true if DB implementation supports resuming in midst of a chunked item (can save fPIxxx.. and related admin data)
  virtual bool dsResumeChunkedSupportedInDB(void) { return false; };
  /// called when a item in the sync set changes its localID (due to local DB internals)
  /// Datastore must make sure that possibly cached items get updated
  virtual void dsLocalIdHasChanged(const char *aOldID, const char *aNewID);
  /// called when request processing ends
  virtual void dsEndOfMessage(void) {}; // nop at this level
  /// called at end of request processing in server
  /// @note handling of suspend state saving and calling dsThreadMayChangeNow MUST NOT
  ///   be implemented here - engEndOfMessage() takes care of this
  virtual void dsRequestEnded(void) {}; // nop at this level
  /// called to confirm a sync operation's completion (ok status from remote received)
  /// @note aSyncOp passed not necessarily reflects what was sent to remote, but what actually happened
  virtual void dsConfirmItemOp(TSyncOperation aSyncOp, cAppCharP aLocalID, cAppCharP aRemoteID, bool aSuccess, localstatus aErrorStatus=0);
  /// @}

protected:
  /// @name logicXXXX (usually abstract) virtuals defining the interface to the datastore logic implementation layer
  ///   These will never be called directly by the engine, but only from engXXX members of this class
  /// @{
  //
  /// - might be called several times (auth retries at beginning of session)
  virtual localstatus logicMakeAdminReady(cAppCharP aDataStoreURI, cAppCharP aRemoteDBID) { return LOCERR_NOTIMP; };
  /// returns true if logic layer has running threads in the background
  virtual bool logicHasBackgroundThreads(void) { return false; }; // single thread never has background
  /// returns true if DB implementation cannot guarantee complete
  /// rollback of all writes (e.g. if DB must commit at end of message)
  /// @todo rollbacks are obsolete, kick this one out
  virtual bool incompleteRollbacksPossible(void) { return false; }

  /// get conflict resolution strategy.
  virtual TConflictResolution getConflictStrategy(bool aForSlowSync, bool aForFirstTime=false);
  #ifdef SYSYNC_SERVER
  /// check if conflicting item already exist in list of items-to-be-sent-to-client
  virtual TSyncItem *getConflictingItemByRemoteID(TSyncItem *syncitemP) = 0;
  virtual TSyncItem *getConflictingItemByLocalID(TSyncItem *syncitemP) = 0;
  /// called to check if content-matching item from server exists
  virtual TSyncItem *getMatchingItem(TSyncItem *syncitemP, TEqualityMode aEqMode) = 0;
  /// called to prevent item to be sent to client in subsequent engGenerateSyncCommands()
  //  item in question should be an item that was returned by getConflictingItemByRemoteID() or getMatchingItem()
  virtual void dontSendItemAsServer(TSyncItem *syncitemP) = 0;
  /// called to have additional item sent to remote (DB takes ownership of item)
  virtual void SendItemAsServer(TSyncItem *aSyncitemP) = 0;
  /// called for servers when receiving map from client
  virtual localstatus logicProcessMap(cAppCharP aRemoteID, cAppCharP aLocalID) = 0;
  #endif
  /// called to process incoming item operation.
  /// Method must take ownership of syncitemP in all cases
  virtual bool logicProcessRemoteItem(
    TSyncItem *syncitemP,
    TStatusCommand &aStatusCommand,
    bool &aVisibleInSyncset, // on entry: tells if resulting item SHOULD be visible; on exit: set if processed item remains visible in the sync set.
    string *aGUID=NULL // GUID is stored here if not NULL
  ) = 0;
  /// called to read a specified item from the server DB (not restricted to set of conflicting items)
  virtual bool logicRetrieveItemByID(
    TSyncItem &aSyncItem, ///< item to be filled with data from server. Local or Remote ID must already be set
    TStatusCommand &aStatusCommand ///< status, must be set on error or non-200-status
  ) = 0;


  #ifdef SYSYNC_CLIENT
  /// client: called to generate sync sub-commands to be sent to server
  /// Returns true if now finished for this datastore
  /// also sets fState to dss_syncdone(server)/dss_syncready(client) when finished
  virtual bool logicGenerateSyncCommandsAsClient(
    TSmlCommandPContainer &aNextMessageCommands,
    TSmlCommand * &aInterruptedCommandP,
    const char *aLocalIDPrefix=NULL
  ) = 0;
  #endif
  #ifdef SYSYNC_SERVER
  /// server: called to generate sync sub-commands to be sent to client
  /// Returns true if now finished for this datastore
  /// also sets fState to dss_syncdone(server)/dss_syncready(client) when finished
  virtual bool logicGenerateSyncCommandsAsServer(
    TSmlCommandPContainer &aNextMessageCommands,
    TSmlCommand * &aInterruptedCommandP,
    const char *aLocalIDPrefix=NULL
  ) = 0;
  #endif
  /// called to have all non-yet-generated sync commands as "to-be-resumed"
  virtual void logicMarkOnlyUngeneratedForResume(void) = 0;
  /// called to mark an already generated (but probably not sent or not yet statused) item
  /// as "to-be-resumed", by localID or remoteID (latter only in server case).
  /// @note This must be repeatable without side effects, as server must mark/save suspend state
  ///       after every request (and not just at end of session)
  virtual void logicMarkItemForResume(cAppCharP aLocalID, cAppCharP aRemoteID, bool aUnSent) = 0;
  /// called to mark an already sent item as "to-be-resent", e.g. due to temporary
  /// error status conditions, by localID or remoteID (latter only in server case).
  virtual void logicMarkItemForResend(cAppCharP aLocalID, cAppCharP aRemoteID) = 0;
  /// save status information required to possibly perform a resume (as passed to datastore with
  /// markOnlyUngeneratedForResume() and markItemForResume())
  /// (or, in case the session is really complete, make sure that no resume state is left)
  /// @note Must also save tempGUIDs (for server) and pending/unconfirmed maps (for client)
  virtual localstatus logicSaveResumeMarks(void) { return LOCERR_NOTIMP; }; // must be derived (or avoided, as in superdatastore)
  /// do log substitutions
  virtual void DoLogSubstitutions(string &aLog,bool aPlaintext);
  /// @}

  /// @name dsHelpers private/protected helper routines
  ///   These will never be called directly by the engine, but only from engXXX members of this class
  /// @{
  //
#ifdef SUPERDATASTORES
protected:
#else
private:
#endif
  /// change datastore state, calls logic layer before and after change
  localstatus changeState(TLocalEngineDSState aNewState, bool aForceOnError=false);
  /// test datastore state for minimal or exact state
  bool testState(TLocalEngineDSState aMinState, bool aNeedExactMatch=false);
private:
  /// get Alert code for current Sync State
  uInt16 getSyncStateAlertCode(bool aServerAlerted=false, bool aClientMinimal=false);
  /// initializes Sync state variables
  localstatus setSyncMode(bool aAsClient, TSyncModes aSyncMode, bool aIsSlowSync, bool aIsServerAlerted);
  /// initializes Sync state variables and returns error if alert is not supported
  localstatus setSyncModeFromAlertCode(uInt16 aAlertCode, bool aAsClient);
  /// check if aborted and if yes, set status with appropriate error code
  bool CheckAborted(TStatusCommand &aStatusCommand);
  /// called to show sync statistics in debug log and on console
  SUPERDS_VIRTUAL void showStatistics(void);
  /// Parse CGI option
  #ifdef SYSYNC_TARGET_OPTIONS
  cAppCharP parseOption(
    const char *aOptName,
    const char *aArguments,
    bool aFromSyncCommand
  );
  #endif
  #ifndef NO_REMOTE_RULES
  /// rule match types container
  TRuleMatchTypesContainer fRuleMatchItemTypes; // contains rule match item types
  #endif
protected:
  #ifdef SYSYNC_SERVER
  /// for sending GUIDs (Add command), generate temp GUID which conforms to maxguidsize of remote datastore if needed
  void adjustLocalIDforSize(string &aLocalID, sInt32 maxguidsize, sInt32 prefixsize);
  /// for received GUIDs (Map command), obtain real GUID (might be temp GUID due to maxguidsize restrictions)
  void obtainRealLocalID(string &aLocalID);
  /// helper to cause database version of an item (as identified by aSyncItemP's ID) to be sent to client
  /// (aka "force a conflict")
  TSyncItem *SendDBVersionOfItemAsServer(TSyncItem *aSyncItemP);
  #endif // SYSYNC_SERVER
  /// helper to save resume state either at end of request or explicitly at reception of a "suspend"
  SUPERDS_VIRTUAL localstatus engSaveSuspendState(bool aAnyway);
  /// Returns true if type information is sufficient to create items to be sent to remote party
  bool canCreateItemForRemote(void) { return (fLocalSendToRemoteTypeP && fRemoteReceiveFromLocalTypeP); };
  /// create SyncItem suitable for being sent from local to remote
  TSyncItem *newItemForRemote(
    uInt16 aExpectedTypeID ///< typeid of expected type
  );
  /// helper for derived classes to generate sync op commands
  TSyncOpCommand *newSyncOpCommand(
    TSyncItem *aSyncItemP, // the sync item
    TSyncItemType *aSyncItemTypeP, // the sync item type
    cAppCharP aLocalIDPrefix // prefix for localID (can be NULL for none)
  );
  /// return pure relative (item) URI (removes absolute part or ./ prefix)
  /// @note this one is virtual because it is defined in TSyncDataStore
  virtual cAppCharP DatastoreRelativeURI(cAppCharP aURI);
  /// obtain new datastore info, returns NULL if none available
  /// @note this one is virtual because it is defined in TSyncDataStore
  virtual SmlDevInfDatastorePtr_t newDevInfDatastore(bool aAsServer, bool aWithoutCTCapProps);
  /// obtain sync capabilities list for datastore
  /// @note this one is virtual because it is defined in TSyncDataStore
  virtual SmlDevInfSyncCapPtr_t newDevInfSyncCap(uInt32 aSyncCapMask);
  /// analyze database name
  void analyzeName(
    const char *aDatastoreURI,
    string *aBaseNameP,
    string *aTableNameP=NULL,
    string *aCGIP=NULL
  );
  /// get DB specific error code for last routine call that returned !=LOCERR_OK
  /// @return platform specific DB error code
  virtual uInt32 lastDBError(void) { return 0; };
  virtual bool isDBError(uInt32 aErrCode) { return aErrCode!=0; } // standard implementation assumes 0=ok
  /// get error message text showing lastDBError for dbg log, or empty string if none
  /// @return platform specific DB error text
  virtual string lastDBErrorText(void);
  /// @}
}; // TLocalEngineDS

} // namespace sysync

#endif  // LocalEngineDS_H

// eof
