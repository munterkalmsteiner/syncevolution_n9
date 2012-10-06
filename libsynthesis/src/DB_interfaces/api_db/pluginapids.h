/**
 *  @File     pluginapids.h
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TPluginApiDS
 *    Plugin based datastore API implementation
 *
 *    Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-10-06 : luz : created from apidbdatastore
 */


#ifndef PLUGINAPIDS_H
#define PLUGINAPIDS_H


// includes

#ifdef SQL_SUPPORT
  #include "odbcapids.h"
  #undef SDK_ONLY_SUPPORT
#else
  #include "customimplds.h"
  #define SDK_ONLY_SUPPORT 1
#endif

#include "dbapi.h"
#include "pluginapiagent.h"


using namespace sysync;

namespace sysync {

// single field mapping
class TApiFieldMapItem:
  #ifdef SDK_ONLY_SUPPORT
  public TFieldMapItem
  #else
  public TODBCFieldMapItem
  #endif
{
  #ifdef SDK_ONLY_SUPPORT
  typedef TFieldMapItem inherited;
  #else
  typedef TODBCFieldMapItem inherited;
  #endif
public:
  TApiFieldMapItem(const char *aElementName, TConfigElement *aParentElement);
  // - parser for extra attributes (for derived classes)
  virtual void checkAttrs(const char **aAttributes);
  // properties
}; // TApiFieldMapItem


#ifdef ARRAYDBTABLES_SUPPORT
// array mapping
class TApiFieldMapArrayItem:
  #ifdef SDK_ONLY_SUPPORT
  public TFieldMapArrayItem
  #else
  public TODBCFieldMapArrayItem
  #endif
{
  #ifdef SDK_ONLY_SUPPORT
  typedef TFieldMapArrayItem inherited;
  #else
  typedef TODBCFieldMapArrayItem inherited;
  #endif
public:
  TApiFieldMapArrayItem(TCustomDSConfig *aCustomDSConfigP, TConfigElement *aParentElement);
  // - parser for extra attributes (for derived classes)
  virtual void checkAttrs(const char **aAttributes);
  // properties
}; // TApiFieldMapArrayItem
#endif



class TPluginDSConfig:
  #ifdef SDK_ONLY_SUPPORT
  public TCustomDSConfig
  #else
  public TOdbcDSConfig
  #endif
{
  #ifdef SDK_ONLY_SUPPORT
  typedef TCustomDSConfig inherited;
  #else
  typedef TOdbcDSConfig inherited;
  #endif
public:
  TPluginDSConfig(const char* aName, TConfigElement *aParentElement);
  virtual ~TPluginDSConfig();
  // properties
  /// name of the DB API modules
  /// @{
  string fDBAPIModule_Data; ///< name of the module which handles data - if fDataModuleAlsoHandlesAdmin is set this is used for admin as well
  string fDBAPIModule_Admin; ///< name of the module which handles admin
  /// @}
  /// flag to handle admin in same API module as data
  /// @Note this is only needed for old-style configs where we had only ONE module
  /// per datastore for both admin and data. Now the normal way is to configure
  /// either  fDBAPIModule_Data or fDBAPIModule_Admin to select plugin handling of
  /// either data or admin independently.
  bool fDataModuleAlsoHandlesAdmin;
  // wants startDataRead() called as early as possible
  bool fEarlyStartDataRead;
  // - config object for API module
  TDB_Api_Config fDBApiConfig_Data;
  TDB_Api_Config fDBApiConfig_Admin;
  // - generic module params
  TApiParamConfig fPluginParams_Admin;
  TApiParamConfig fPluginParams_Data;
  // - debug flags
  uInt16 fPluginDbgMask_Data;
  uInt16 fPluginDbgMask_Admin;
  // capabilities of connected plugin
  bool fItemAsKey; // supports items as key
  bool fHasDeleteSyncSet; // implements deleting sync set using DeleteSyncSet()
  // public methods
  // - create appropriate datastore from config, calls addTypeSupport as well
  virtual TLocalEngineDS *newLocalDataStore(TSyncSession *aSessionP);
  // factory functions for field map items
  virtual TFieldMapItem *newFieldMapItem(const char *aElementName, TConfigElement *aParentElement)
    { return new TApiFieldMapItem(aElementName,aParentElement); };
  #ifdef ARRAYDBTABLES_SUPPORT
  virtual TFieldMapArrayItem *newFieldMapArrayItem(TCustomDSConfig *aCustomDSConfigP, TConfigElement *aParentElement)
    { return new TApiFieldMapArrayItem(aCustomDSConfigP,aParentElement); };
  #endif
protected:
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void clear();
  virtual void localResolve(bool aLastPass);
private:
}; // TPluginDSConfig


// forward
class TPluginAgentConfig;
class TPluginApiAgent;
#if defined(DBAPI_ASKEYITEMS) && defined(ENGINEINTERFACE_SUPPORT)
class TDBItemKey;
#endif // DBAPI_ASKEYITEMS + ENGINEINTERFACE_SUPPORT


class TPluginApiDS:
  #ifdef SDK_ONLY_SUPPORT
  public TCustomImplDS
  #else
  public TODBCApiDS
  #endif
{
  #ifdef SDK_ONLY_SUPPORT
  typedef TCustomImplDS inherited;
  #else
  typedef TODBCApiDS inherited;
  #endif
  friend class TApiBlobProxy;
private:
  void InternalResetDataStore(void); // reset for re-use without re-creation
public:
  TPluginApiDS(
    TPluginDSConfig *aConfigP,
    sysync::TSyncSession *aSessionP,
    const char *aName,
    uInt32 aCommonSyncCapMask=0);
  virtual void announceAgentDestruction(void);
  virtual void dsResetDataStore(void) { InternalResetDataStore(); inherited::dsResetDataStore(); };
  virtual ~TPluginApiDS();

  // override TSyncDataStore: the plugin must be able to return 404
  // when an item is not found during delete
  virtual bool dsDeleteDetectsItemPresence() const { return true; }

  #ifndef BINFILE_ALWAYS_ACTIVE
  /// @name apiXXXX methods defining the interface from TCustomImplDS to TXXXApi actual API implementations
  /// @{
  //
  /// @brief Load admin data from ODBC database
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
  );
  /// @brief Save admin data to ODBC database
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
  ///                         pending maps (client only). Note that fPendingAddMaps might contain temporary localIDs,
  ///                         so call dsFinalizeLocalID() to ensure these are converted to final before saving.
  ///   - fTempGUIDMap      = map<string,string>. The implementation must save all entries as temporary LUID to GUID mappings
  ///                         (server only)
  virtual localstatus apiSaveAdminData(bool aSessionFinished, bool aSuccessful);
  /// read sync set IDs and mod dates.
  /// @param[in] if set, all data fields are needed, so ReadSyncSet MAY
  ///   read items here already. Note that ReadSyncSet MAY read items here
  ///   even if aNeedAll is not set (if it is more efficient than reading
  ///   them separately afterwards).
  #endif // not BINFILE_ALWAYS_ACTIVE

  /// perform early data access start (if datastore requests it by setting fEarlyStartDataRead config flag)
  virtual localstatus apiEarlyDataAccessStart(void);
  /// read the sync set
  virtual localstatus apiReadSyncSet(bool aNeedAll);
  /// Zap all data in syncset (note that everything outside the sync set will remain intact)
  virtual localstatus apiZapSyncSet(void);
  virtual bool apiNeedSyncSetToZap(void);
  /// fetch record contents from DB by localID.
  virtual localstatus apiFetchItem(TMultiFieldItem &aItem, bool aReadPhase, TSyncSetItem *aSyncSetItemP);
  /// add new item to datastore, returns created localID
  virtual localstatus apiAddItem(TMultiFieldItem &aItem, string &aLocalID);
  /// update existing item in datastore, returns 404 if item not found
  virtual localstatus apiUpdateItem(TMultiFieldItem &aItem);
  /// delete existing item in datastore, returns 211 if not existing any more
  virtual localstatus apiDeleteItem(TMultiFieldItem &aItem);
  /// end of syncset read
  virtual localstatus apiEndDataRead(void) { return LOCERR_OK; /* NOP at this time */ };
  /// start of write
  virtual localstatus apiStartDataWrite(void);
  /// end DB data write sequence (but not yet admin data)
  virtual localstatus apiEndDataWrite(string &aThisSyncIdentifier);
  /// @}

  /// @name dsXXXX virtuals defined by TLocalEngineDS
  ///   These are usually designed such that they should always call inherited::dsXXX to let the entire chain
  ///   of ancestors see the calls
  /// @{
  //
  /// end of message handling
  // virtual void dsEndOfMessage(void);
  #ifdef OBJECT_FILTERING
  /// test expression filtering capability of datastore
  /// @return true if DB implementation can filter the standard filters
  /// (LocalDBFilter, TargetFilter and InvisibleFilter) during database fetch
  /// otherwise, fetched items will be filtered after being read from DB.
  virtual bool dsFilteredFetchesFromDB(bool aFilterChanged=false);
  /// test special option filtering capability of datastore
  /// @return true if DB implementation can also apply special filters like CGI-options
  /// /dr(x,y) etc. during fetching
  virtual bool dsOptionFilterFetchesFromDB(void);
  #endif
  /// alert possible thread change
  virtual void dsThreadMayChangeNow(void);
  /// returns true if DB implementation supports resume (saving of resume marks, alert code, pending maps, tempGUIDs)
  virtual bool dsResumeSupportedInDB(void);
  #ifdef SYSYNC_CLIENT
  /// finalize local ID (for datastores that can't efficiently produce these at insert)
  virtual bool dsFinalizeLocalID(string &aLocalID);
  #endif
  /// @}


  // - must be called before starting a thread. If returns false, starting a thread now
  //   is not allowed and must be postponed.
  virtual bool startingThread(void);
  // - must be called when a thread's activity has ended
  //   BUT THE CALL MUST BE FROM THE ENDING THREAD, not the main thread!
  virtual void endingThread(void);
  // - should be called before doing DB accesses that might be locked (e.g. because another thread is using the DB resources)
  virtual bool dbAccessLocked(void);
  // - log datastore sync result, Called at end of sync with this datastore
  virtual void dsLogSyncResult(void);
private:
  // - connect data handling part of plugin. Returns LOCERR_NOTIMPL when no data plugin is selected
  TSyError connectDataPlugin(void);
  // - prepare for reading syncset (is called early when fEarlyStartDataRead is set, otherwise from within apiReadSyncSet)
  localstatus apiPrepareReadSyncSet(void);
  // - alert possible thread change to plugins
  //   Does not check if API is locked or not, see dsThreadMayChangeNow()
  void ThreadMayChangeNow(void);
  #ifdef DBAPI_TEXTITEMS
  // Text item handling
  // - store itemdata field into mapped TItemField
  virtual bool storeField(
    cAppCharP aName,
    cAppCharP aParams,
    cAppCharP aValue,
    TMultiFieldItem &aItem,
    uInt16 aSetNo,
    sInt16 aArrayIndex
  );
  // - parse itemdata into item using DB mappings
  bool parseDBItemData(
    TMultiFieldItem &aItem,
    cAppCharP aItemData,
    uInt16 aSetNo
  );
  // - create itemdata from mapped fields
  bool generateDBItemData(
    bool aAssignedOnly,
    TMultiFieldItem &aItem,
    uInt16 aSetNo,
    string &aDataFields
  );
  #endif
  // - post process item after reading from DB (run script)
  bool postReadProcessItem(TMultiFieldItem &aItem, uInt16 aSetNo);
  // - pre-process item before writing to DB (run script)
  bool preWriteProcessItem(TMultiFieldItem &aItem);
  // - send BLOBs of this item one by one
  bool writeBlobs(
    bool aAssignedOnly,
    TMultiFieldItem &aItem,
    uInt16 aSetNo
  );
  // - delete BLOBs of this item one by one
  bool deleteBlobs(
    bool aAssignedOnly,
    TMultiFieldItem &aItem,
    uInt16 aSetNo
  );
private:
  // agent (typed pointers for convenience)
  TPluginApiAgent *fPluginAgentP;
  // config (typed pointers for convenience)
  TPluginAgentConfig *fPluginAgentConfigP;
  // the actual API access instances
  TDB_Api fDBApi_Data; ///< access to data
  TDB_Api fDBApi_Admin; ///< access to admin
  // config pointer
  TPluginDSConfig *fPluginDSConfigP;
  // filter testing
  bool fAPICanFilter;
  bool fAPIFiltersTested;
}; // TPluginApiDS


#ifdef STREAMFIELD_SUPPORT

// proxy for loading blobs
class TApiBlobProxy : public TBlobProxy
{
  typedef TBlobProxy inherited;
public:
  TApiBlobProxy(TPluginApiDS *aApiDsP, bool aIsStringBLOB, const char *aBlobID, const char *aParentID);
  virtual ~TApiBlobProxy();
  // - returns size of entire blob
  virtual size_t getBlobSize(TStringField *aFieldP);
  // - read from Blob from specified stream position and update stream pos
  virtual size_t readBlobStream(TStringField *aFieldP, size_t &aPos, void *aBuffer, size_t aMaxBytes);
  // - dependency on a local ID
  virtual void setParentLocalID(const char *aParentLocalID) { fParentObjectID=aParentLocalID; };
private:
  // fetch BLOB from DPAPI
  void fetchBlob(size_t aNeededSize, bool aNeedsTotalSize, bool aNeedsAllData);
  // Vars
  TPluginApiDS *fApiDsP; // datastore which can be asked to retrieve data
  bool fIsStringBLOB; // if set, the blob must be treated as a string (applying DB charset conversions)
  string fBlobID; // object ID of blob
  string fParentObjectID; // id of parent object (will be updated in case parent signals ID change)
  size_t fBlobSize; // total size of the BLOB
  bool fBlobSizeKnown; // set if fBlobSize is valid (can also be 0 for KNOWN empty blob)
  size_t fFetchedSize; // how much we have already retrieved (and is in fBlobBuffer)
  size_t fBufferSize; // how much room we have in the buffer
  uInt8P fBlobBuffer; // buffer for blob already retrieved (NULL if nothing yet)
}; // TApiBlobProxy

#endif


} // namespace sysync

#endif  // PLUGINAPIDS_H

// eof
