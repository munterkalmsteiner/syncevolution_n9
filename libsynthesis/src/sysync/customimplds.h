/**
 *  @File     customimpl.h
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TCustomImplDS
 *    Base class for customizable datastores (mainly extended DB mapping features
 *    common to all derived classes like ODBC, DBAPI etc.).
 *
 *    Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-12-05 : luz : separated from odbcapids
 */

#ifndef CUSTOMIMPLDS_H
#define CUSTOMIMPLDS_H


// includes
#include "stdlogicds.h"
#include "multifielditemtype.h"
#include "customimplagent.h"

#ifdef BASED_ON_BINFILE_CLIENT
  #include "binfileimplds.h"
#else
  #ifdef BINFILE_ALWAYS_ACTIVE
    #error "BINFILE_ALWAYS_ACTIVE is only possible when BASED_ON_BINFILE_CLIENT"
  #endif
#endif


using namespace sysync;

namespace sysync {

#ifdef SCRIPT_SUPPORT
// publish as derivates might need it
extern const TFuncTable CustomDSFuncTable2;
#endif

// the datastore config

// field mapping base class
class TFieldMapItem : public TConfigElement
{
  typedef TConfigElement inherited;
public:
  TFieldMapItem(const char *aElementName, TConfigElement *aParentElement);
  virtual bool isArray(void) { return false; };
  // - parser for extra attributes (for derived classes)
  virtual void checkAttrs(const char **aAttributes) { /* nop in base class */ };
  // - field mode, here in base class to simplify things
  bool readable; // used to read (for SELECT)
  bool writable; // used to write (for UPDATE,INSERT)
  //   - more detailed control
  bool for_insert;
  bool for_update;
  //   - map as parameter (rather than literally inserting field values in INSERT and UPDATE statements)
  bool as_param;
  //   - map as floating time field (will be written as-is, no conversion from/to DB time zone takes place)
  bool floating_ts;
  //   - if set, this field's value needs to be saved for finalisation run at the very end of the sync session
  bool needs_finalisation;
  // - set number (for having different sets in different statement parts)
  uInt16 setNo;
  // - name of the field in SQL statements is the name of the item
  // - field ID in MultiFieldItem
  sInt16 fid;
  // - type
  // Note: Fields that are separate date/time in the DB but one
  // field in MultiFieldItem can be specified as dbft_date FIRST and
  // then as dbft_timefordate in this list, causing time to be combined
  // with date.
  TDBFieldType dbfieldtype; // database field type
  // - field size
  uInt32 maxsize; // number of chars field can hold (if string or BLOB), 0=unlimited
  bool notruncate; // set if this field should never be sent truncated by the remote
}; // TFieldMapItem


typedef std::list<TFieldMapItem *> TFieldMapList;


#ifdef ARRAYDBTABLES_SUPPORT

class TFieldMappings;
class TCustomDSConfig;

// special field map item: Array map
class TFieldMapArrayItem : public TFieldMapItem
{
  typedef TFieldMapItem inherited;
public:
  TFieldMapArrayItem(TCustomDSConfig *aCustomDSConfigP, TConfigElement *aParentElement);
  virtual ~TFieldMapArrayItem();
  // properties
  // - the map for the fields in the array. fid of maps are used as base fid
  //   if they do not reference array fields
  TFieldMapList fArrayFieldMapList;
  // - config of related datastore
  TCustomDSConfig *fCustomDSConfigP;
  // - base TFieldMappings %%% not used any more, we take it from fCustomDSConfigP
  //TFieldMappings *fBaseFieldMappings;
  // - referenced fieldlist %%% not used any more, we take it from fBaseFieldMappings
  // TFieldListConfig *fFieldListP;
  #ifdef OBJECT_FILTERING
  // - filter expression:
  //   if filter evaluates as true, array is not written (or deleted at update)
  //   if array has no items on read, filter is applied to item with makepass()
  string fNoItemsFilter;
  #endif
  #ifdef SCRIPT_SUPPORT
  // array record's scripts
  string fInitScript;
  string fAfterReadScript;
  string fBeforeWriteScript;
  string fAfterWriteScript;
  string fFinishScript;
  #endif
  // - repeating params
  sInt16 fMaxRepeat; // if==0, unlimited repeats (for array fields only)
  sInt16 fRepeatInc; // increment per repetition
  bool fStoreEmpty; // if set, empty leaf fields will also be stored in the array
  // Methods
  virtual bool isArray(void) { return true; };
  virtual void clear();
protected:
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void localResolve(bool aLastPass);
  #ifdef SCRIPT_SUPPORT
  // process map scripts (resolve or rebuild them)
  bool fScriptsResolved;
  void ResolveArrayScripts(void);
  void expectScriptUnresolved(string &aTScript,sInt32 aLine, const TFuncTable *aContextFuncs);
  #endif
}; // TFieldMapArrayItem

#endif




class TCustomDSConfig;

class TFieldMappings: public TConfigElement
{
  typedef TConfigElement inherited;
public:
  TFieldMappings(const char* aName, TConfigElement *aParentElement);
  virtual ~TFieldMappings();
  // properties
  // - the map itself
  TFieldMapList fFieldMapList;
  #ifdef SCRIPT_SUPPORT
  // - script that evaluates the syncset options such as STARTDATE() and ENDDATE()
  //   and configures the DB such that the filtering takes place during fetch.
  //   Must return TRUE if DB level filter can perform requested options
  string fOptionFilterScript;
  // - main record's scripts
  string fInitScript;
  string fAfterReadScript;
  string fBeforeWriteScript;
  string fAfterWriteScript;
  string fFinishScript;
  string fFinalisationScript;
  #endif
  // - a reference to a field list
  TFieldListConfig *fFieldListP;
  virtual void clear();
  #ifdef SCRIPT_SUPPORT
  // processing of map scripts (resolve or rebuild them)
  virtual const TFuncTable *getDSFuncTableP(void);
  //%%% moved to fDSScriptsResolved: bool fScriptsResolved;
  //%%% moved to ResolveDSScripts: void ResolveMapScripts(void);
  #endif
protected:
  #ifdef SCRIPT_SUPPORT
  void expectScriptUnresolved(string &aTScript,sInt32 aLine, const TFuncTable *aContextFuncs);
  #endif
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void localResolve(bool aLastPass);
}; // TFieldMappings



class TCustomDSConfig:
  #ifdef BASED_ON_BINFILE_CLIENT
  public TBinfileDSConfig
  #else
  public TLocalDSConfig
  #endif
{
  #ifdef BASED_ON_BINFILE_CLIENT
  typedef TBinfileDSConfig inherited;
  #else
  typedef TLocalDSConfig inherited;
  #endif
public:
  TCustomDSConfig(const char* aName, TConfigElement *aParentElement);
  virtual ~TCustomDSConfig();
  // properties
  // - if set, multi-folder (datastore consisting of multiple folders, such as email)
  //   features are enabled for this datastore
  bool fMultiFolderDB;
  // - charset to be used in the data table
  TCharSets fDataCharSet;
  // - line end mode to be used in the data table for multiline data
  TLineEndModes fDataLineEndMode;
  // - if set, causes that data is read from DB first and then merged
  //   with updated fields. Not needed in normal DBs that can
  //   update a subset of all columns.
  bool fUpdateAllFields;
  // - Date/Time info
  bool fDataIsUTC; //%%% legacy flag for compatibility, superseded by fDataTimeZone
  timecontext_t fDataTimeZone; // time zone for storing/retrieving timestamps in DB
  bool fUserZoneOutput; // if set, all non-floating timestamps are moved to user time zone (probably from datatimezone)
  // - admin capability info
  bool fStoreSyncIdentifiers; // if set, database separately stores "last sync with data sent to remote" and "last suspend" identifiers (stored as string, not necessarily a date)
  #ifndef BINFILE_ALWAYS_ACTIVE
  bool fSyncTimeStampAtEnd; // if set, time point of sync is taken AFTER last write to DB (for single-user DBs like FMPro)
  bool fOneWayFromRemoteSupported; // if set, database has a separate "last sync with data sent to remote" timestamp
  bool fResumeSupport; // if set, admin tables have DS 1.2 support needed for resume (map entrytype, map flags, fResumeAlertCode, fLastSuspend, fLastSuspendIdentifier
  bool fResumeItemSupport; // if set, admin tables have support for storing data to resume a partially transferred item
  // - one-way support is always given for binfile based DS
  virtual bool isOneWayFromRemoteSupported() { return fOneWayFromRemoteSupported; }
  #endif // not BINFILE_ALWAYS_ACTIVE
  // - Database field to item field mappings
  TFieldMappings fFieldMappings;
  #ifdef SCRIPT_SUPPORT
  // provided to allow derivates to add API specific script functions to scripts called from CustomImplDS
  virtual const TFuncTable *getDSFuncTableP(void) { return &CustomDSFuncTable1; };
  // - script called after admin data is loaded (before any data access takes place)
  string fAdminReadyScript;
  // - script called after sync with this datastore is ended (before writing the log)
  string fSyncEndScript;
  // - context for resolving scripts in datastore context
  TScriptContext *fResolveContextP;
  // - resolve DS scripts, this may be called before entire DS config is resolved to allow map's accessing local vars
  bool fDSScriptsResolved;
  void ResolveDSScripts(void);
  virtual void apiResolveScripts(void) { /* nop here */ };
  #endif
  // factory functions for field map items
  virtual TFieldMapItem *newFieldMapItem(const char *aElementName, TConfigElement *aParentElement)
    { return new TFieldMapItem(aElementName,aParentElement); };
  #ifdef ARRAYDBTABLES_SUPPORT
  virtual TFieldMapArrayItem *newFieldMapArrayItem(TCustomDSConfig *aCustomDSConfig, TConfigElement *aParentElement)
    { return new TFieldMapArrayItem(aCustomDSConfig,aParentElement); };
  #endif
  // returns false for datastores that are not abstract, i.e. have a backend implementation (=all stdlogicds derivates)
  virtual bool isAbstractDatastore(void) { return false; }; // customimplds is the foundation for a implemented backend - so it is no longer abstract
protected:
  // Add (probably datastore-specific) limits such as MaxSize and NoTruncate to types
  virtual void addTypeLimits(TLocalEngineDS *aLocalDatastoreP, TSyncSession *aSessionP);
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void clear();
  virtual void localResolve(bool aLastPass);
}; // TCustomDSConfig


#ifndef BINFILE_ALWAYS_ACTIVE

// getitem phase
typedef enum {
  gph_deleted,
  gph_added_changed,
  gph_done
} TGetPhases;


/// Map flags
#define mapflag_useforresume          0x00000001 ///< set on map items that were marked for resume in the previous session
#define mapflag_pendingMapStatus      0x00000002 ///< set on pending maps that have been sent, but not seen status yet
#define mapflag_pendingStatus         0x00000004 ///< set on map items that have generated a add/update/move, but not seen status yet
#define mapflag_pendingDeleteStatus   0x00000008 ///< set on map items that have generated a delete, but not seen status yet
#define mapflag_pendingAddConfirm     0x00000010 ///< set on items that have generated a add op, and have not yet received full confirmation (=status for client, =map for server)
#define mapflag_resend                0x00000020 ///< set on items that should be re-sent to remote in next session


/// Map entry types
typedef enum {
  mapentry_invalid, ///< should not ever be present
  mapentry_normal, ///< normal localID to remoteID map item
  mapentry_tempidmap, ///< pseudo-map items that is not a local/remote maps, but saved local/adjustedLocal map (server only)
  mapentry_pendingmap, ///< pseudo-map item that represents pending maps to be sent after resume (client only)
  numMapEntryTypes
} TMapEntryType;

#ifdef SYDEBUG
extern const char * const MapEntryTypeNames[];
#endif

/// local in-memory map entry
/// @Note This mapentry can hold main maps, as well as tempGUID and pendingMap pseudo maps
typedef struct {
  TMapEntryType entrytype; ///< type of mapentry
  string localid; ///< localID
  string remoteid; ///< remoteID
  uInt32 mapflags; ///< mapflag_xxxx
  bool changed; ///< set for map items changed in this session
  bool deleted; ///< set for map items deleted in this session
  bool added; ///< set for map items added in this session
  bool markforresume; ///< set for map items that must be saved with mapflag_useforresume
  bool savedmark; ///< set for map items that are already saved with mapflag_useforresume (optimisation)
} TMapEntry;


// container for map entries
typedef list<TMapEntry> TMapContainer;

#endif // BINFILE_ALWAYS_ACTIVE


// local SyncSet entry
typedef struct {
  string localid;
  string containerid; // for multi-folder DBs
  bool isModified; // set if modified since last sync
  bool isModifiedAfterSuspend; // set if modified since last suspend
  // optional item contents (depends on implementation of ReadSyncSet())
  TMultiFieldItem *itemP;
} TSyncSetItem;


// container for sync set information
typedef list<TSyncSetItem *> TSyncSetList;

// container for finalisation
typedef list<TMultiFieldItem *> TMultiFieldItemList;


class TDBItemKey;

class TCustomImplDS:
  #ifdef BASED_ON_BINFILE_CLIENT
  public TBinfileImplDS
  #else
  public TStdLogicDS
  #endif
{
  #ifdef BASED_ON_BINFILE_CLIENT
  typedef TBinfileImplDS inherited;
  #else
  typedef TStdLogicDS inherited;
  #endif
  friend class TCustomDSfuncs;
  friend class TCustomCommonFuncs;
  friend class TDBItemKey;
  friend class TCustomDSTunnelKey;

private:
  void InternalResetDataStore(void); // reset for re-use without re-creation
protected:
  /// @name dsSavedAdmin administrative data (anchors, timestamps, maps) as saved or to-be-saved
  /// @Note These will be loaded and saved be derived classes
  /// @Note Some of these will be updated from resp. @ref dsCurrentAdmin members at distinct events (suspend, session end, etc.)
  /// @Note Some of these will be updated during the session, but in a way that does NOT affect the anchoring of current/last session
  //
  /// @{
  /// Reference time of previous sync which sent data to remote to compare modification dates against
  /// @note normally==fPreviousSyncTime, but can be end of sync for datastore that can't write modified timestamps at will
  #ifndef BASED_ON_BINFILE_CLIENT
  /// @note for BASED_ON_BINFILE_CLIENT case, these already exist at the binfile level, so we MUST NOT have them here again!!
  /// @note if binfile is there, but disabled, we still dont need these member vars.
  lineartime_t fPreviousToRemoteSyncCmpRef;
  /// Reference string used by database API level to determine modifications since last to-remote-sync
  string fPreviousToRemoteSyncIdentifier;
  /// Reference time of last suspend, needed to detect modifications that took place between last suspend and current resume
  lineartime_t fPreviousSuspendCmpRef;
  /// Reference string used by database API level to detect modifications that took place between last suspend and current resume
  string fPreviousSuspendIdentifier;
  /// @}

  /// @name dsCurrentAdmin current session's admin data (anchors, timestamps, maps)
  /// @Note These will be copied to @ref dsSavedAdmin members ONLY when a session completes successfully/suspends.
  /// @Note Admin data is NEVER directly saved or loaded from these
  /// @Note Derivates will update some of these at dssta_adminready with current time/anchor values
  //
  /// @{
  /// Reference time of current sync to compare modification dates against
  /// @note initially==fCurrentSyncTime, but might be set to end-of-session time for databases which cannot explicitly set modification timestamps
  lineartime_t fCurrentSyncCmpRef;
  /// Reference string returned by database API level identifying this session's time (for detecting changes taking place after this session)
  string fCurrentSyncIdentifier;
  /// @}
  #endif

public:
  TCustomImplDS(
    TCustomDSConfig *aConfigP,
    sysync::TSyncSession *aSessionP,
    const char *aName,
    uInt32 aCommonSyncCapMask=0);
  virtual void announceAgentDestruction(void);
  virtual void dsResetDataStore(void) { InternalResetDataStore(); inherited::dsResetDataStore(); };
  virtual ~TCustomImplDS();

  /// @name apiXXXX methods defining the interface from TCustomImplDS to TXXXApi actual API implementations
  /// @{
  //

  #ifndef BINFILE_ALWAYS_ACTIVE
  /// @brief Load admin data from database
  /// @param aDeviceID[in]       remote device URI (device ID)
  /// @param aDatabaseID[in]     local database ID
  /// @param aRemoteDBID[in]     database ID of remote device
  ///   Must search for existing target record matching the triple (aDeviceID,aDatabaseID,aRemoteDBID)
  ///   - if there is a matching record: load it
  ///   - if there is no matching record, set fFirstTimeSync=true. The implementation may already create a
  ///     new record with the key (aDeviceID,aDatabaseID,aRemoteDBID) and initialize it with the data from
  ///     the items as shown below. At least, fTargetKey must be set to a value that will allow apiSaveAdminData to
  ///     update the record. In case implementation chooses not create the record only in apiSaveAdminData, it must
  ///     buffer the triple (aDeviceID,aDatabaseID,aRemoteDBID) such that it is available at apiSaveAdminData.
  ///   If a record exists implementation must load the following items:
  ///   - fTargetKey        = some key value that can be used to re-identify the target record later at SaveAdminData.
  ///                         If the database implementation has other means to re-identify the target, this can be
  ///                         left unassigned.
  ///   - fLastRemoteAnchor = anchor string used by remote party for last session (and saved to DB then)
  ///   - fPreviousSyncTime = anchor (beginning of session) timestamp of last session.
  ///   - fPreviousToRemoteSyncCmpRef = Reference time to determine items modified since last time sending data to remote
  ///                         (or last changelog update in case of BASED_ON_BINFILE_CLIENT)
  ///   - fPreviousToRemoteSyncIdentifier = string identifying last session that sent data to remote
  ///                         (or last changelog update in case of BASED_ON_BINFILE_CLIENT). Needs only be saved
  ///                         if derived datastore cannot work with timestamps and has its own identifier.
  ///   - fMapTable         = list<TMapEntry> containing map entries. The implementation must load all map entries
  ///                         related to the current sync target identified by the triple of (aDeviceID,aDatabaseID,aRemoteDBID)
  ///                         or by fTargetKey. The entries added to fMapTable must have "changed", "added" and "deleted" flags
  ///                         set to false.
  ///   For resumable datastores:
  ///   - fMapTable         = In addition to the above, the markforresume flag must be saved in the mapflags
  //                          when it is not equal to the savedmark flag - independently of added/deleted/changed.
  ///   - fResumeAlertCode  = alert code of current suspend state, 0 if none
  ///   - fPreviousSuspendCmpRef = reference time of last suspend (used to detect items modified during a suspend / resume)
  ///   - fPreviousSuspendIdentifier = identifier of last suspend (used to detect items modified during a suspend / resume)
  ///                         (needs only be saved if derived datastore cannot work with timestamps and has
  ///                         its own identifier)
  ///   - fPendingAddMaps   = map<string,string>. The implementation must load all  all pending maps (client only) into
  ///                         fPendingAddMaps (and fUnconfirmedMaps must be left empty).
  ///   - fTempGUIDMap      = map<string,string>. The implementation must save all entries as temporary LUID to GUID mappings
  ///                         (server only)
  virtual localstatus apiLoadAdminData(
    const char *aDeviceID,    // remote device URI (device ID)
    const char *aDatabaseID,  // database ID
    const char *aRemoteDBID  // database ID of remote device
  ) = 0;
  /// @brief Save admin data to database
  /// @param[in] aSessionFinished if true, this is a end-of-session save (and not only a suspend save) - but not necessarily a successful one
  /// @param[in] aSuccessful if true, this is a successful end-of-session
  ///   Must save to the target record addressed at LoadAdminData() by the triple (aDeviceID,aDatabaseID,aRemoteDBID)
  ///   Implementation must save the following items:
  ///   - fLastRemoteAnchor = anchor string used by remote party for this session (and saved to DB then)
  ///   - fPreviousSyncTime = anchor (beginning of session) timestamp of this session.
  ///   - fPreviousToRemoteSyncCmpRef = Reference time to determine items modified since last time sending data to remote
  ///                         (or last changelog update in case of BASED_ON_BINFILE_CLIENT)
  ///   - fPreviousToRemoteSyncIdentifier = string identifying last session that sent data to remote
  ///                         (or last changelog update in case of BASED_ON_BINFILE_CLIENT). Needs only be saved
  ///                         if derived datastore cannot work with timestamps and has its own identifier.
  ///   - fMapTable         = list<TMapEntry> containing map entries. The implementation must save all map entries
  ///                         that have changed, are new or are deleted. See below for additional resume requirements.
  ///   For resumable datastores:
  ///   - fMapTable         = In addition to the above, the markforresume flag must be saved in the mapflags
  //                          when it is not equal to the savedmark flag - independently of added/deleted/changed.
  ///   - fResumeAlertCode  = alert code of current suspend state, 0 if none
  ///   - fPreviousSuspendCmpRef = reference time of last suspend (used to detect items modified during a suspend / resume)
  ///   - fPreviousSuspendIdentifier = identifier of last suspend (used to detect items modified during a suspend / resume)
  ///                         (needs only be saved if derived datastore cannot work with timestamps and has
  ///                         its own identifier)
  ///   - fPendingAddMaps and fUnconfirmedMaps = map<string,string>. The implementation must save all entries as
  ///                         pending maps (client only). localIDs might be temporary, so call dsFinalizeLocalID() to
  ///                         ensure these are final.
  ///   - fTempGUIDMap      = map<string,string>. The implementation must save all entries as temporary LUID to GUID mappings
  ///                         (server only)
  virtual localstatus apiSaveAdminData(bool aSessionFinished, bool aSuccessful) = 0;
  #endif // BINFILE_ALWAYS_ACTIVE

  /// allow early data access start (if datastore is configured for it)
  virtual localstatus apiEarlyDataAccessStart(void) { return LOCERR_OK; /* nop if not overridden */ };
  /// read sync set IDs and mod dates.
  /// @param[in] if set, all data fields are needed, so ReadSyncSet MAY
  ///   read items here already. Note that ReadSyncSet MAY read items here
  ///   even if aNeedAll is not set (if it is more efficient than reading
  ///   them separately afterwards).
  virtual localstatus apiReadSyncSet(bool aNeedAll) = 0;
  /// Zap all data in syncset (note that everything outside the sync set will remain intact)
  virtual localstatus apiZapSyncSet(void) = 0;
  virtual bool apiNeedSyncSetToZap(void) = 0; // derivate must define this so we can prepare the sync set before zapping if needed
  /// fetch record contents from DB by localID.
  virtual localstatus apiFetchItem(TMultiFieldItem &aItem, bool aReadPhase, TSyncSetItem *aSyncSetItemP) = 0;
  /// add new item to datastore, returns created localID
  virtual localstatus apiAddItem(TMultiFieldItem &aItem, string &aLocalID) = 0;
  /// update existing item in datastore, returns 404 if item not found
  virtual localstatus apiUpdateItem(TMultiFieldItem &aItem) = 0;
  /// delete existing item in datastore, returns 211 if not existing any more
  virtual localstatus apiDeleteItem(TMultiFieldItem &aItem) = 0;
  /// end of syncset reading phase
  virtual localstatus apiEndDataRead(void) = 0;
  /// start of write
  virtual localstatus apiStartDataWrite(void) = 0;
#ifdef __clang__
/// Same name as in TBinfileImplDS. Tell clang compiler to ignore that name clash,
/// it is okay here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif
  /// end DB data write sequence (but not yet admin data)
  virtual localstatus apiEndDataWrite(string &aThisSyncIdentifier) = 0;
#ifdef __clang__
#pragma clang diagnostic pop
#endif

  /// @}

  #ifdef DBAPI_TUNNEL_SUPPORT
  /// @name TunnelXXX methods allowing abstracted access to datastores from UIApi from within a tunnel session
  /// @{
  //

  virtual TSyError TunnelStartDataRead(cAppCharP aLastToken, cAppCharP aResumeToken);
  virtual TSyError TunnelReadNextItem(ItemID aID, appCharP *aItemData, sInt32 *aStatus, bool aFirst);
  virtual TSyError TunnelReadItem(cItemID aID, appCharP *aItemData);
  virtual TSyError TunnelEndDataRead();
  virtual TSyError TunnelStartDataWrite();
  virtual TSyError TunnelInsertItem(cAppCharP aItemData, ItemID aID);
  virtual TSyError TunnelUpdateItem(cAppCharP aItemData, cItemID aID, ItemID aUpdID);
  virtual TSyError TunnelMoveItem(cItemID aID, cAppCharP aNewParID);
  virtual TSyError TunnelDeleteItem(cItemID aID);
  virtual TSyError TunnelEndDataWrite(bool aSuccess, appCharP *aNewToken);
  virtual void     TunnelDisposeObj(void* aMemory);

  virtual TSyError TunnelReadNextItemAsKey(ItemID aID, KeyH aItemKey, sInt32 *aStatus, bool aFirst);
  virtual TSyError TunnelReadItemAsKey(cItemID aID, KeyH aItemKey);
  virtual TSyError TunnelInsertItemAsKey(KeyH aItemKey, ItemID aID);
  virtual TSyError TunnelUpdateItemAsKey(KeyH aItemKey, cItemID aID, ItemID aUpdID);

  virtual TSettingsKeyImpl *newTunnelKey(TEngineInterface *aEngineInterfaceP);

  // helpers
  void setupTunnelTypes(TSyncItemType *aItemTypeP=NULL);

  /// @}
  #endif



public:
  /// @name dsXXXX virtuals defined by TLocalEngineDS
  ///   These are usually designed such that they should always call inherited::dsXXX to let the entire chain
  ///   of ancestors see the calls
  /// @{
  //
  /// end of message handling
  virtual void dsEndOfMessage(void);
  /// inform logic of coming state change
  virtual localstatus dsBeforeStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState);
  /// inform logic of happened state change
  virtual localstatus dsAfterStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState);
  #ifndef BINFILE_ALWAYS_ACTIVE
  /// called to confirm a sync operation's completion (ok status from remote received)
  /// @note aSyncOp passed not necessarily reflects what was sent to remote, but what actually happened
  virtual void dsConfirmItemOp(TSyncOperation aSyncOp, cAppCharP aLocalID, cAppCharP aRemoteID, bool aSuccess, localstatus aErrorStatus=0);
  #endif // BINFILE_ALWAYS_ACTIVE

  /// @}


  /// @name dsHelpers private/protected helper routines
  /// @{
  //
private:
  /// private helper to prepare for apiSaveAdminData()
  localstatus SaveAdminData(bool aSessionFinished, bool aSuccessful);
  /// @}

  // agent
  TCustomImplAgent *fAgentP; // access to agent (casted fSessionP for convenience)
  // config (typed pointers for convenience)
  TCustomDSConfig *fConfigP;
  TCustomAgentConfig *fAgentConfigP;

protected:
  // some vars
  sInt16 fArrIdx;
  #ifdef SCRIPT_SUPPORT
  // - temp vars while running scripts
  string fParentKey;
  bool fWriting;
  bool fInserting;
  bool fDeleting;
  #endif
  #ifdef ARRAYDBTABLES_SUPPORT
  bool fHasArrayFields; // set if array table access needed
  #endif
  bool fNeedFinalisation; // set if fields which need finalisation exist in field mappings
  // script context
  #ifdef SCRIPT_SUPPORT
  TScriptContext *fScriptContextP;
  virtual void apiRebuildScriptContexts(void) { /* nop here */ };
  #endif
  // Simple custom DB access interface methods
  // - returns true if database implementation can only update all fields of a record at once
  virtual bool dsReplaceWritesAllDBFields(void);
  #ifndef BINFILE_ALWAYS_ACTIVE
  // - returns true if DB implementation supports resume (saving of resume marks, alert code, pending maps, tempGUIDs)
  virtual bool dsResumeSupportedInDB(void);
  // - returns true if DB implementation supports resuming in midst of a chunked item (can save fPIxxx.. and related admin data)
  virtual bool dsResumeChunkedSupportedInDB(void);
  #endif // not BINFILE_ALWAYS_ACTIVE
  #ifdef OBJECT_FILTERING
  // - returns true if DB implementation can also apply special filters like CGI-options
  //   /dr(x,y) etc. during fetching
  virtual bool dsOptionFilterFetchesFromDB(void);
  #endif

  /// @name implXXX methods used when based on StdLogicDS
  /// @{

  /// @brief sync login (into this database)
  /// @note also exists in BASED_ON_BINFILE_CLIENT, will do the job together with
  ///       inhertited binfile version (instead of using DB api)
  /// @note might be called several times (auth retries at beginning of session)
  /// @note must update the following state variables
  /// - in TLocalEngineDS: fLastRemoteAnchor, fLastLocalAnchor, fResumeAlertCode, fFirstTimeSync
  ///   - for client: fPendingAddMaps
  ///   - for server: fTempGUIDMap
  /// - in TStdLogicDS: fPreviousSyncTime, fCurrentSyncTime
  /// - in derived classes: whatever else belongs to dsSavedAdmin and dsCurrentAdmin state
  ///   (for example fTargetKey, fFolderKey)
  virtual localstatus implMakeAdminReady(
    const char *aDeviceID,    ///< remote device URI (device ID)
    const char *aDatabaseID,  ///< database ID
    const char *aRemoteDBID  ///< database ID of remote device
  );
  /// save end of session state
  /// @note also exists in BASED_ON_BINFILE_CLIENT, will do the job together with
  ///       inhertited binfile version (instead of using DB api)
  virtual localstatus implSaveEndOfSession(bool aUpdateAnchors);
  /// start data read
  /// @note: fSlowSync and fRefreshOnly must be valid before calling this method
  virtual localstatus implStartDataRead();
  /// end of read
  /// @note also exists in BASED_ON_BINFILE_CLIENT, will do the job together with
  ///       inhertited binfile version.
  virtual localstatus implEndDataRead(void);
  /// start of write
  /// @note also exists in BASED_ON_BINFILE_CLIENT, will do the job together with
  ///       inhertited binfile version.
  virtual localstatus implStartDataWrite(void);
  /// end write sequence
  /// @note also exists in BASED_ON_BINFILE_CLIENT, will do the job together with
  ///       inhertited binfile version.
  virtual bool implEndDataWrite(void);

  #ifdef BASED_ON_BINFILE_CLIENT
  /// when based on binfile client, we need the syncset loaded when binfile is active
  bool implNeedSyncSetToRetrieve(void) { return binfileDSActive(); };
  /// when based on binfile client, we can't track syncop changes (like having the DB report
  /// items as added again when stdlogic filters have decided they fell out of the syncset)
  virtual bool implTracksSyncopChanges(void) { return !binfileDSActive(); };
  #else
  bool implNeedSyncSetToRetrieve(void) { return false; }; // non-binfiles don't need the syncset to retrieve
  virtual bool implTracksSyncopChanges(void) { return true; }; // non-binfile custimpls are capable of this
  #endif


  #ifndef BINFILE_ALWAYS_ACTIVE
  /// get item from DB
  virtual localstatus implGetItem(
    bool &aEof,
    bool &aChanged,
    TSyncItem* &aSyncItemP
  );
  /// review reported entry (allows post-processing such as map deleting)
  /// MUST be called after implStartDataWrite, before any actual writing,
  /// for each item obtained in implGetItem
  virtual localstatus implReviewReadItem(
    TSyncItem &aItem         // the item
  );
  /// called to set maps.
  /// @note aLocalID or aRemoteID can be NULL - which signifies deletion of a map entry
  /// @note that this might be needed for clients accessing a server-style database as well
  virtual localstatus implProcessMap(cAppCharP aRemoteID, cAppCharP aLocalID);
  /// retrieve specified item from database
  virtual bool implRetrieveItemByID(
    TSyncItem &aItem,         // the item
    TStatusCommand &aStatusCommand
  );
  /// process item (according to operation: add/delete/replace - and for future: copy/move)
  virtual bool implProcessItem(
    TSyncItem *aItemP,         // the item
    TStatusCommand &aStatusCommand
  );
  /// called to mark an already generated (but probably not sent or not yet statused) item
  /// as "to-be-resumed", by localID or remoteID (latter only in server case).
  /// @note This must be repeatable without side effects, as server must mark/save suspend state
  ///       after every request (and not just at end of session)
  virtual void implMarkItemForResume(cAppCharP aLocalID, cAppCharP aRemoteID, bool aUnSent);
  /// called to mark an already sent item as "to-be-resent", e.g. due to temporary
  /// error status conditions, by localID or remoteID (latter only in server case).
  virtual void implMarkItemForResend(cAppCharP aLocalID, cAppCharP aRemoteID);
  /// called to have all non-yet-generated sync commands as "to-be-resumed"
  virtual void implMarkOnlyUngeneratedForResume(void);
  /// save status information required to possibly perform a resume (as passed to datastore with
  /// markOnlyUngeneratedForResume() and markItemForResume())
  /// (or, in case the session is really complete, make sure that no resume state is left)
  virtual localstatus implSaveResumeMarks(void);

  /// @}
  #endif // not BINFILE_ALWAYS_ACTIVE

  #ifdef BASED_ON_BINFILE_CLIENT
  /// @name methods used when based on BinfileImplDS
  /// @{

  #ifndef CHANGEDETECTION_AVAILABLE
    #error "CustomImplDS can be built only on BinFileImplDS with CHANGEDETECTION_AVAILABLE"
  #endif
  /// get first item's ID and modification status from the sync set
  /// @return false if no item found
  virtual bool getFirstItemInfo(localid_out_t &aLocalID, bool &aItemHasChanged);
  /// get next item's ID and modification status from the sync set.
  /// @return false if no item found
  virtual bool getNextItemInfo(localid_out_t &aLocalID, bool &aItemHasChanged);
  /// get first item from the sync set. Caller obtains ownership if aItemP is not NULL after return
  /// @return false if no item found
  virtual bool getFirstItem(TSyncItem *&aItemP);
  /// get next item from the sync set. Caller obtains ownership if aItemP is not NULL after return
  /// @return false if no item found
  virtual bool getNextItem(TSyncItem *&aItemP);
  /// get item by local ID from the sync set. Caller obtains ownership if aItemP is not NULL after return
  /// @return != LOCERR_OK  if item with specified ID is not found.
  virtual localstatus getItemByID(localid_t aLocalID, TSyncItem *&aItemP);
  /* no need to implement these here, calling API level directly from binfile is enough
  /// signal start of data write phase
  virtual localstatus apiStartDataWrite(void);
  /// signal end of data write phase
  virtual localstatus apiEndDataWrite(void);
  */
  /// update item by local ID in the sync set. Caller retains ownership of aItemP
  /// @return != LOCERR_OK  if item with specified ID is not found.
  virtual localstatus updateItemByID(localid_t aLocalID, TSyncItem *aItemP);
  /// delete item by local ID in the sync set.
  /// @return != LOCERR_OK if item with specified ID is not found.
  virtual localstatus deleteItemByID(localid_t aLocalID);
  /// create new item in the sync set. Caller retains ownership of aItemP.
  /// @return LOCERR_OK or error code.
  /// @param[out] aNewLocalID local ID assigned to new item
  /// @param[out] aReceiveOnly is set to true if local changes/deletion of this item should not be
  ///   reported to the server in normal syncs.
  virtual localstatus createItem(TSyncItem *aItemP,localid_out_t &aNewLocalID, bool &aReceiveOnly);
  /// zaps the entire datastore, returns LOCERR_OK if ok
  /// @return LOCERR_OK or error code.
  virtual localstatus zapDatastore(void);
  /// @}
  #endif // BASED_ON_BINFILE_CLIENT

protected:
  // - helper for getting a base field pointer (not resolved down to array element)
  TItemField *getMappedBaseFieldOrVar(TMultiFieldItem &aItem, sInt16 aFid);
  // - helper for getting a field pointer (local script var or item's field)
  TItemField *getMappedFieldOrVar(TMultiFieldItem &aItem, sInt16 aFid, sInt16 aRepOffset, bool aExistingOnly=false);
  // In-memory map and syncset access
  // - mark all map entries as deleted
  bool deleteAllMaps(void);
  // - delete syncset
  void DeleteSyncSet(bool aContentsOnly=false);
  // - get container ID for specified localid
  bool getContainerID(const char *aLocalID, string &aContainerID);
  // - delete sync set one by one
  localstatus zapSyncSetOneByOne(void);
  // - Queue the data needed for finalisation (usually - relational link updates)
  //   as a item copy with only finalisation-required fields
  void queueForFinalisation(TMultiFieldItem *aItemP);
public:
  // - get last to-remote sync time
  lineartime_t getPreviousToRemoteSyncCmpRef(void) { return fPreviousToRemoteSyncCmpRef; };
  lineartime_t getPreviousSuspendCmpRef(void) { return fPreviousSuspendCmpRef; };
  // - get syncset list
  TSyncSetList *getSyncSetList(void) { return &fSyncSetList; };
  // - find entry in sync set by localid
  TSyncSetList::iterator findInSyncSet(const char *aLocalID);
protected:
  #ifndef BINFILE_ALWAYS_ACTIVE
  // - find non-deleted map entry by local ID / entry type
  TMapContainer::iterator findMapByLocalID(const char *aLocalID,TMapEntryType aEntryType, bool aDeletedAsWell=false);
  // - find map entry by remote ID
  TMapContainer::iterator findMapByRemoteID(const char *aRemoteID);
  // - modify map, if remoteID or localID is NULL or empty, map item will be deleted (if it exists at all)
  void modifyMap(TMapEntryType aEntryType, const char *aLocalID, const char *aRemoteID, uInt32 aMapFlags, bool aDelete, uInt32 aClearFlags=0xFFFFFFFF);
  #endif // not BINFILE_ALWAYS_ACTIVE
  #ifdef SYSYNC_SERVER
  // - called when a item in the sync set changes its localID (due to local DB internals)
  //   Datastore must make sure that possibly cached items get updated
  virtual void dsLocalIdHasChanged(const char *aOldID, const char *aNewID);
  #endif // SYSYNC_SERVER
  // - target key (if needed by descendant)
  string fTargetKey;
  // - folder key (key value for subselecting in datastore, determined at implMakeAdminReady())
  string fFolderKey;
  // local list of local IDs/mod timestamps of current sync set for speedup and avoiding LEFT OUTER JOIN
  TSyncSetList fSyncSetList;
  // - iterator for reporting new and added items in GetItem
  TSyncSetList::iterator fSyncSetPos;
  // - list of items that must be processed in finalisation at end of sync
  TMultiFieldItemList fFinalisationQueue;
  #ifndef BINFILE_ALWAYS_ACTIVE
  // local map list
  TMapContainer fMapTable;
  // - iterator for reporting deleted items in GetItem
  TMapContainer::iterator fDeleteMapPos;
  bool fReportDeleted;
  TGetPhases fGetPhase; // phase of get
  bool fGetPhasePrepared; // set if phase is prepared (select or list iterator init)
  #endif // BINFILE_ALWAYS_ACTIVE
  #ifdef BASED_ON_BINFILE_CLIENT
  bool fSyncSetLoaded; // set if sync set is currently loaded
  localstatus makeSyncSetLoaded(bool aNeedAll);
  #endif // BASED_ON_BINFILE_CLIENT
  localstatus getItemFromSyncSetItem(TSyncSetItem *aSyncSetItemP, TSyncItem *&aItemP);
  bool fNoSingleItemRead; // if set, syncset list will also contain items
  bool fMultiFolderDB; // if set, we need the syncset list for finding container IDs later
  #ifdef SCRIPT_SUPPORT
  bool fOptionFilterTested;
  bool fOptionFilterWorksOnDBLevel; // set if option filters can be executed by DB
  #endif // SCRIPT_SUPPORT

  #ifdef DBAPI_TUNNEL_SUPPORT
  // Tunnel DB access support
  bool fTunnelReadStarted;
  bool generateTunnelItemData(bool aAssignedOnly, TMultiFieldItem *aItemP, string &aDataFields);
  bool parseTunnelItemData(TMultiFieldItem &aItem, const char *aItemData);
  TSyError TunnelReadNextItemInternal(ItemID aID, TSyncItem *&aItemP, sInt32 *aStatus, bool aFirst);
  TSyError TunnelInsertItemInternal(TMultiFieldItem *aItemP, ItemID aNewID);
  TSyError TunnelUpdateItemInternal(TMultiFieldItem *aItemP, cItemID aID, ItemID aUpdID);
  #endif // DBAPI_TUNNEL_SUPPORT

  #ifdef DBAPI_TEXTITEMS
  // Text item handling
  // - store itemdata field into named TItemField (derived DBApi store will override this with DB-mapped version)
  virtual bool storeField(
    const char *aName,
    const char *aParams,
    const char *aValue,
    TMultiFieldItem &aItem,
    uInt16 aSetNo,
    sInt16 aArrayIndex
  );
  // - parse itemdata into item (generic, using virtual storeField() for actually finding field by name)
  bool parseItemData(
    TMultiFieldItem &aItem,
    const char *aItemData,
    uInt16 aSetNo
  );
  // - generate text data for one field (common for Tunnel and DB API)
  bool generateItemFieldData(
    bool aAssignedOnly, TCharSets aDataCharSet, TLineEndModes aDataLineEndMode, timecontext_t aTimeContext,
    TItemField *aBasefieldP, cAppCharP aBaseFieldName, string &aDataFields
  );
  #endif // DBAPI_TEXTITEMS

  #if (defined(DBAPI_ASKEYITEMS) || defined(DBAPI_TUNNEL_SUPPORT)) && defined(ENGINEINTERFACE_SUPPORT)
  TDBItemKey *newDBItemKey(TMultiFieldItem *aItemP, bool aOwnsItem=false);
  #endif // (DBAPI_ASKEYITEMS or DBAPI_TUNNEL_SUPPORT) and ENGINEINTERFACE_SUPPORT

}; // TCustomImplDS


#ifdef DBAPI_TEXTITEMS

// helper to process params
// - if aParamName!=NULL, it searches for the value of the requested parameter and returns != NULL, NULL if none found
// - if aParamName==NULL, it scans until all params are skipped and returns end of params
cAppCharP paramScan(cAppCharP aParams,cAppCharP aParamName, string &aValue);

#endif // DBAPI_TEXTITEMS


#if (defined(DBAPI_ASKEYITEMS) || defined(DBAPI_TUNNEL_SUPPORT)) && defined(ENGINEINTERFACE_SUPPORT)

// key for access to a item using the settings key API
class TDBItemKey :
  public TItemFieldKey
{
  typedef TItemFieldKey inherited;
public:
  TDBItemKey(TEngineInterface *aEngineInterfaceP, TMultiFieldItem *aItemP, TCustomImplDS *aCustomImplDS, bool aOwnsItem=false) :
    inherited(aEngineInterfaceP),
    fCustomImplDS(aCustomImplDS),
    fItemP(aItemP),
    fOwnsItem(aOwnsItem)
  {};
  virtual ~TDBItemKey() { forgetItem(); };
  TMultiFieldItem *getItem(void) { return fItemP; };
  void setItem(TMultiFieldItem *aItemP, bool aPassOwner=false);

protected:

  // methods to actually access a TItemField
  virtual sInt16 getFidFor(cAppCharP aName, stringSize aNameSz);
  virtual bool getFieldNameFromFid(sInt16 aFid, string &aFieldName);
  virtual TItemField *getBaseFieldFromFid(sInt16 aFid);

  // the datastore
  TCustomImplDS *fCustomImplDS;
  // the item being accessed
  TMultiFieldItem *fItemP;
  bool fOwnsItem;
  // iterator
  TFieldMapList::iterator fIterator;

private:
  void forgetItem() { if (fOwnsItem && fItemP) { delete fItemP; } fItemP=NULL; };


}; // TDBItemKey

#endif // (DBAPI_ASKEYITEMS or DBAPI_TUNNEL_SUPPORT) and ENGINEINTERFACE_SUPPORT


#ifdef DBAPI_TUNNEL_SUPPORT

// tunnel DB API parameters
class TCustomDSTunnelKey :
  public TStructFieldsKey
{
  typedef TStructFieldsKey inherited;

public:
  TCustomDSTunnelKey(TEngineInterface *aEngineInterfaceP, TCustomImplDS *aCustomImplDsP);
  virtual ~TCustomDSTunnelKey();
  TCustomImplDS *getCustomImplDs(void) { return fCustomImplDsP; }

protected:
  // open subkey by name (not by path!)
  // - this is the actual implementation
  virtual TSyError OpenSubKeyByName(
    TSettingsKeyImpl *&aSettingsKeyP,
    cAppCharP aName, stringSize aNameSize,
    uInt16 aMode
  );
  // field access
  const TStructFieldInfo *getFieldsTable(void);
  sInt32 numFields(void);
  // fields
  TCustomImplDS *fCustomImplDsP;
}; // TCustomDSTunnelKey


#endif // DBAPI_TUNNEL_SUPPORT


} // namespace sysync

#endif  // CUSTOMIMPLDS_H

// eof
