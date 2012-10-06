/**
 *  @File     odbcapids.h
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TODBCApiDS
 *    ODBC based datastore API implementation
 *    @Note Currently also contains what will later become TCustomImplDS
 *
 *    Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-09-16 : luz : created from odbcdbdatastore
 */

#include "prefix_file.h"

#ifndef ODBCAPIDS_H
#define ODBCAPIDS_H

#ifdef SQL_SUPPORT

// includes
#include "odbcdb.h"
#include "stdlogicds.h"
#include "multifielditem.h"
#include "odbcapiagent.h"


using namespace sysync;

namespace sysync {

#ifdef SCRIPT_SUPPORT
// publish as derivates might need it
extern const TFuncTable ODBCDSFuncTable2;
#endif


// the datastore config


// special ID generation modes
typedef enum {
  sidm_none,            // none
  sidm_unixmsrnd6       // UNIX milliseconds and 6-digit random
} TSpecialIDMode;
const sInt16 numSpecialIDModes = sidm_unixmsrnd6-sidm_none+1;




// ODBC variant of field map
class TODBCFieldMapItem : public TFieldMapItem
{
  typedef TFieldMapItem inherited;
public:
  TODBCFieldMapItem(const char *aElementName, TConfigElement *aParentElement);
  // - parser for extra attributes (for derived classes)
  virtual void checkAttrs(const char **aAttributes);
  #ifdef STREAMFIELD_SUPPORT
  // properties
  string fReadBlobSQL; // read field when needed trough field proxy
  string fKeyFieldName; // if set, this field name will be used in %N when reading the record rather than the actual data field -> to get record key for array elements for proxies
  #endif
}; // TODBCFieldMapItem


#ifdef ARRAYDBTABLES_SUPPORT

// special field map item: Array map
class TODBCFieldMapArrayItem : public TFieldMapArrayItem
{
  typedef TFieldMapArrayItem inherited;
public:
  TODBCFieldMapArrayItem(TCustomDSConfig *aCustomDSConfigP, TConfigElement *aParentElement);
  virtual ~TODBCFieldMapArrayItem();
  // SQL properties
  //   %k = record key of associated main table record
  //   %i = index of current element (0 based)
  //   %N = field name list for INSERT and SELECT
  //   %V = value list for INSERT
  //   %d([opt]fieldname#arrayindex)
  //   %dM,%tM,%M = modified date/time/timestamp
  //   %f=folderkey, %u=userkey, %t=targetkey
  //   %S = size of record (sum of all field literal sizes)
  // - SQL for selecting all elements of the array
  //   NOTE: %N must be first in (and is probably entire) SELECT field list!
  string fSelectArraySQL;
  // - SQL for clearing the array, must clear all subrecords of the array
  string fDeleteArraySQL;
  bool fAlwaysCleanArray; // if set, fDeleteArraySQL will be executed even if master record is inserted new
  // - SQL to insert a new array element
  string fInsertElementSQL;
  // Methods
  virtual void clear();
protected:
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void localResolve(bool aLastPass);
}; // TODBCFieldMapArrayItem

#endif



class TOdbcDSConfig: public TCustomDSConfig
{
  typedef TCustomDSConfig inherited;
public:
  TOdbcDSConfig(const char* aName, TConfigElement *aParentElement);
  virtual ~TOdbcDSConfig();
  // properties
  // - set this if every item modification needs to be committed separately
  //   (for DBs such as Achil where any insert will block the size accumulation
  //   record).
  bool fCommitItems;
  // - define how many times an insert is retried in case of ODBC error, 0=no retry
  sInt16 fInsertRetries;
  // - define how many times an update is retried in case of ODBC error, 0=no retry
  sInt16 fUpdateRetries;
  #ifdef HAS_SQL_ADMIN
  // - statement to obtain folder key from Database, empty if none
  //   replacements: %F=foldername, %u=userkey, %d=devicekey
  //   Note: see fFolderKeyField and %f substitute to use folderkey for subselecting data
  string fFolderKeySQL;
  // - sqls for handling Sync Target Table
  //   - replacements: %f=folderkey, %u=userkey, %D=deviceID, %P=deviceDBpath
  string fGetSyncTargetSQL; // must return SYNCTARGETKEY,ANCHOR,LASTSYNC
  string fNewSyncTargetSQL;
  //   - additional replacements: %L=last sync timestamp, %dL=last sync date, %tL=last sync time
  string fUpdateSyncTargetSQL;
  string fDeleteSyncTargetSQL; // should also delete all associated map entries!
  bool fSyncTimestamp; // if set, target table SELECT expects single timestamp, date & time otherwise
  #ifdef OLD_1_0_5_CONFIG_COMPATIBLE
  // Map table access, old version
  // - layout of map table
  string fMapTableName; // name
  string fRemoteIDMapField; // remote ID
  string fLocalIDMapField; // local ID
  bool fStringLocalID; // if set, local ID is quoted as string literal in SQL
  string fTargetKeyMapField; // target key
  bool fStringTargetkey; // if set, target key is quoted as string literal in SQL
  // Data table access, old version
  // - name of table containing the data
  string fLocalTableName;
  /* never documented (only used in achil), so not supported any more
  // - additional constant insert values (appended to INSERT field/value lists, resp.)
  //   %d([opt]fieldname#arrayindex)
  //   %dM,%tM,%M = modified date/time/timestamp
  //   %f=folderkey, %u=userkey, %t=targetkey
  //   %S = size of record (sum of all field literal sizes)
  string fConstInsFields;
  string fConstInsValues;
  // - additional constant update terms (appended to UPDATE SET list)
  //   %d([opt]fieldname#arrayindex)
  //   %dM,%tM,%M = modified date/time/timestamp
  //   %f=folderkey, %u=userkey, %t=targetkey
  //   %S = size of record (sum of all field literal sizes)
  string fConstUpdates;
  */
  // - name of folderkey subselection field in data table, empty if none
  string fFolderKeyField;
  bool fStringFolderkey; // if set, folder key is quoted as string literal in SQL
  // - name of field which contains Local ID (GUID)
  string fLocalIDField;
  // - modified date & time fields
  string fModifiedDateField; // this field is used as modified date or datetime
  string fModifiedTimeField; // if set, this field is used as modified time
  // - additional select clause (to be used for all selects BUT NOT INSERTS!)
  //   NOTE: can contain %x-type substitutions (see DoSQLSubstitutions())
  // Note: was never documented so will be phased out right now
  // string fGeneralSelectCondition;
  #endif // OLD_1_0_5_CONFIG_COMPATIBLE
  // Map table access, new version
  // Substitutions:
  //   from agent's DoSQLSubstitutions:
  //     %u = userkey
  //     %d = devicekey
  //   from datastore's DoSQLSubstitutions:
  //     %f = folderkey
  //     %t = targetkey
  //   from DoMapSubstitutions:
  //     %e = entry type
  //     %k = data key (local ID)
  //     %r = remote ID (always a string)
  // - SQL to fetch all map entries of a target: order= localid,remoteid
  string fMapFetchAllSQL;
  // - SQL to insert new map entry by localid
  string fMapInsertSQL;
  // - SQL to update existing map entry by localid
  string fMapUpdateSQL;
  // - SQL to delete a map entry by localid
  string fMapDeleteSQL;
  // Data table access, new version
  // Substitutions:
  //   from agent's DoSQLSubstitutions:
  //     %u = userkey
  //     %d = devicekey
  //   from datastore's DoSQLSubstitutions:
  //     %f = folderkey
  //     %t = targetkey
  //   from DoDataSubstitutions:
  //     %dM,%tM,%M = modified date,time,timestamp
  //     %d([opts]fieldname#arrindex) = contents of field, opts l=lowercase, u=uppercase, a=ascii-only
  //     %S = size of all string field literal sizes
  //     %AF = filter conditions preceeded by AND (can be empty)
  //     %WF = filter conditions preceeded by WHERE (can be empty)
  //     %k = data key (local ID)
  //     %N = data field name list
  //     %aN = data field name list with all field, regardless of possible unassigned state of fields in update statements
  //     %V = data field=value list
  //     %aV = data field=value list with all field
  //     %v = data value list
  //     %aV = data value list with all field
  //   for Array fields only:
  //     %i = 0 based index of array elements
  #endif // HAS_SQL_ADMIN
  // - single(!) SQL statement to fetch local ID and timestamp from data table
  string fLocalIDAndTimestampFetchSQL;
  bool fModifiedTimestamp; // if set, above statement returns separate date and time
  // Note: the following data access SQL strings may consist of multiple SQL statements
  //       separated by %GO or %GO(setno). Default setno=0 without preceeding %GO(setno)
  // - SQL statement(s) to fetch actual data fields from data table. SQL result must
  //   contain mapped fields in the same order as they appear in <fieldmap>
  //   Note: Statement(s) executed with setno==0 returning no data are interpreted
  //         as "record does not exist (any more)". Statements with setno!=0 returning
  //         no data are just ignored (assuming optional data not present)
  string fDataFetchSQL;
  // - SQL statement(s) to insert new data in a record, including modification date
  string fDataInsertSQL;
  // - SQL statement(s) to update actual data in a record, including modification date
  string fDataUpdateSQL;
  // - SQL statement(s) to delete a record
  string fDataDeleteSQL;
  // - SQL statement(s) to zap entire sync set
  string fDataZapSQL;
  // Data table options
  // - try to translate filters to SQL if possible
  bool fFilterOnDBLevel;
  // - Literal string quoting mode
  TQuotingModes fQuotingMode;
  // Local ID generation
  // - if set, fObtainNextIDSql is executed only ONCE per write phase, and
  //   IDs generated by incrementing the ID by 1 for every record added.
  //   This is NOT MULTI-USER-SAFE, but can be useful for slow DBs like FMPro
  //   Setting this excludes fObtainNextIDAfterInsert
  bool fDetermineNewIDOnce;
  // - if set, fObtainNewLocalIDSql is used AFTER inserting without a GUID
  //   to obtain the GUID generated by the DB (such as MS-SQL autoincrement)
  bool fObtainNewIDAfterInsert;
  // - This SQL query is used to obtain the local ID
  //   for adds. It is expected to return the ID in the first result column.
  //   - if (fDetermineNextIDOnce), it is executed once at implimplSyncLogin,
  //     further IDs are generated by incrementing it (for single-User DBs only!)
  //   - if (fObtainNextIDAfterInsert), this is executed AFTER inserting
  //     without a local ID specified in INSERT statement to determine DB-assigned ID
  string fObtainNewLocalIDSql;
  #ifdef SCRIPT_SUPPORT
  // - This script is called when a new localID must be generated (fObtainNextIDAfterInsert == false)
  //   before adding a record. If it returns non-undefined, the return value is used as localID
  string fLocalIDScript;
  #endif
  // - if This specifies the minimum ID to be used if fObtainNextIDSql
  //   returns something lower. This is also dangerous, but also needed
  //   for FMPro to keep FMPro generated IDs far (much lower) from Sync
  //   generated IDs
  sInt32 fMinNextID;
  // - specifies special ID calculation method
  TSpecialIDMode fSpecialIDMode;
  // - if set, the fDataInsertSQL statement is expected to return the new ID as
  //   the first column of the result set (for example if insert is implemented
  //   with a stored procedue)
  bool fInsertReturnsID;
  bool fInsertIDAsOutParam;
  // - if set, SQLRowCount result is ignored and write statements are always assumed to be successful
  //   (unless they fail with an error, of course)
  bool fIgnoreAffectedCount;
  // - type of the last-modified date column
  TDBFieldType fLastModDBFieldType;
  // SQLite support
  #ifdef SQLITE_SUPPORT
  // - name of the SQLite file that contains the user data for this datastore
  string fSQLiteFileName;
  // - busy timeout, 0 = no wait
  uInt32 fSQLiteBusyTimeout;
  #endif
  // public methods
  // - return charset used to create SQL statments (will return UTF-8 when using UTF-16/UCS-2
  //   to allow SQL statement construction in 8-bit mode, and only converting before
  //   actually calling ODBC
  TCharSets sqlPrepCharSet(void) { return fDataCharSet==chs_utf16 ? chs_utf8 : fDataCharSet; };
  // - create appropriate datastore from config, calls addTypeSupport as well
  virtual TLocalEngineDS *newLocalDataStore(TSyncSession *aSessionP);
  #ifdef SCRIPT_SUPPORT
  // provided to allow derivates to add API specific script functions to scripts called from CustomImplDS
  virtual const TFuncTable *getDSFuncTableP(void) { return &ODBCDSFuncTable1; };
  #endif
  // factory functions for field map items
  virtual TFieldMapItem *newFieldMapItem(const char *aElementName, TConfigElement *aParentElement)
    { return new TODBCFieldMapItem(aElementName,aParentElement); };
  #ifdef ARRAYDBTABLES_SUPPORT
  virtual TFieldMapArrayItem *newFieldMapArrayItem(TCustomDSConfig *aCustomDSConfigP, TConfigElement *aParentElement)
    { return new TODBCFieldMapArrayItem(aCustomDSConfigP,aParentElement); };
  #endif
protected:
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void clear();
  virtual void localResolve(bool aLastPass);
  #ifdef SCRIPT_SUPPORT
  virtual void apiResolveScripts(void);
  #endif
}; // TOdbcDSConfig




class TODBCApiDS: public TCustomImplDS
{
  friend class TODBCDSfuncs;
  friend class TODBCCommonFuncs;
  friend class TODBCFieldProxy;
  typedef TCustomImplDS inherited;
private:
  void InternalResetDataStore(void); // reset for re-use without re-creation
public:
  TODBCApiDS(
    TOdbcDSConfig *aConfigP,
    sysync::TSyncSession *aSessionP,
    const char *aName,
    uInt32 aCommonSyncCapMask=0);
  virtual void announceAgentDestruction(void);
  virtual void dsResetDataStore(void) { InternalResetDataStore(); inherited::dsResetDataStore(); };
  virtual ~TODBCApiDS();

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
  ///                         pending maps (client only).  Note that fPendingAddMaps might contain temporary localIDs,
  ///                         so call dsFinalizeLocalID() to ensure these are converted to final before saving.
  ///   - fTempGUIDMap      = map<string,string>. The implementation must save all entries as temporary LUID to GUID mappings
  ///                         (server only)
  virtual localstatus apiSaveAdminData(bool aSessionFinished, bool aSuccessful);
  /// read sync set IDs and mod dates.
  /// @param[in] if set, all data fields are needed, so ReadSyncSet MAY
  ///   read items here already. Note that ReadSyncSet MAY read items here
  ///   even if aNeedAll is not set (if it is more efficient than reading
  ///   them separately afterwards).
  virtual localstatus apiReadSyncSet(bool aNeedAll);
  /// Zap all data in syncset (note that everything outside the sync set will remain intact)
  virtual localstatus apiZapSyncSet(void);
  virtual bool apiNeedSyncSetToZap(void) { return fConfigP->fDataZapSQL.empty(); }; // we need it if we don't have a zap SQL
  /// fetch record contents from DB by localID.
  virtual localstatus apiFetchItem(TMultiFieldItem &aItem, bool aReadPhase, TSyncSetItem *aSyncSetItemP);
  /// add new item to datastore, returns created localID
  virtual localstatus apiAddItem(TMultiFieldItem &aItem, string &aLocalID);
  /// update existing item in datastore, returns 404 if item not found
  virtual localstatus apiUpdateItem(TMultiFieldItem &aItem);
  /// delete existing item in datastore, returns 211 if not existing any more
  virtual localstatus apiDeleteItem(TMultiFieldItem &aItem);
  /// end of syncset reading phase
  virtual localstatus apiEndDataRead(void);
  /// start of write
  virtual localstatus apiStartDataWrite(void);
  /// end DB data write sequence (but not yet admin data)
  virtual localstatus apiEndDataWrite(string &aThisSyncIdentifier);

  /// @}


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
  // log datastore sync result
  #ifdef HAS_SQL_ADMIN
  // - Called at end of sync with this datastore
  virtual void dsLogSyncResult(void);
  #endif // HAS_SQL_ADMIN
  /// @}


  /// @name dsHelpers private/protected helper routines
  /// @{
  //
private:
  #ifdef HAS_SQL_ADMIN
  /// update sync target head record
  localstatus updateSyncTarget(SQLHSTMT aStatement, bool aSessionFinishedSuccessfully);
  /// update map changes from memory list into actual map table
  localstatus updateODBCMap(SQLHSTMT aStatement, bool aSessionFinishedSuccessfully);
  // - log substitutions
  virtual void DoLogSubstitutions(string &aLog,bool aPlaintext);
  #endif // HAS_SQL_ADMIN

  /// @}


  // Publics for context funcs
  // - generate new LocalID into fLastGeneratedLocalID.
  void nextLocalID(TSpecialIDMode aMode,SQLHSTMT aStatement);
  // agent
  TODBCApiAgent *fAgentP; // access to agent (casted fSessionP for convenience)
  // config (typed pointers for convenience)
  TOdbcDSConfig *fConfigP;
  TOdbcAgentConfig *fAgentConfigP;

protected:
  // some vars
  string fLastGeneratedLocalID;
  #ifdef SCRIPT_SUPPORT
  string fSQLFilter; // run-time calculated filter expression for SELECT WHERE clause
  SQLHSTMT fODBCScriptStatement; // statement used for executing SQL in scripts
  virtual void apiRebuildScriptContexts(void);
  #endif
  #ifdef OBJECT_FILTERING
  // - returns true if DB implementation can filter the standard filters
  //   (LocalDBFilter, TargetFilter and InvisibleFilter) during database fetch
  //   - otherwise, fetched items will be filtered after being read from DB.
  virtual bool dsFilteredFetchesFromDB(bool aFilterChanged=false);
  #endif
protected:
  #ifdef ODBCAPI_SUPPORT
  // ODBC
  // - get (session level) ODBC handle
  SQLHDBC getODBCConnectionHandle(void);
  // - check for connection-level error
  void checkConnectionError(SQLRETURN aResult);
  #ifdef HAS_SQL_ADMIN
  // - issue single (or multiple) map access statements
  //   NOTE: if all return SQL_NODATA, function will return false.
  bool IssueMapSQL(
    SQLHSTMT aStatement,
    const string &aSQL,
    const char *aComment,
    TMapEntryType aEntryType,
    const char *aLocalID,
    const char *aRemoteID,
    uInt32 aMapFlags
  );
  // - do substitutions for map table access
  void DoMapSubstitutions(
    string &aSQL,                   // string to apply substitutions to
    TMapEntryType aEntryType,       // the entry type
    const char *aLocalID,           // local ID
    const char *aRemoteID,          // remote ID
    uInt32 aMapFlags                // map flags
  );
  #endif // HAS_SQL_ADMIN
  #endif // ODBCAPI_SUPPORT
  // Generic SQL
  // - get next statement and setno from statement list (may begin with %GO(setno))
  bool getNextSQLStatement(const string &aSQL, sInt16 &aStartAt, string &aOneSQL, uInt16 &aSetNo);
  // - execute single SQL statements
  bool execSQLStatement(SQLHSTMT aStatement, string &aSQL, bool aNoDataAllowed, const char *aComment, bool aForData);
  // - issue single (or multiple) data write (or delete) statements
  //   NOTE: if all return SQL_NODATA, function will return false.
  bool IssueDataWriteSQL(
    SQLHSTMT aStatement,
    const string &aSQL,
    const char *aComment,
    bool aForUpdate, // for update
    TFieldMapList &aFieldMapList,   // field map list for %N,%V and %v
    TMultiFieldItem *aItemP         // item to read values and localid from
  );
  // - do standard substitutions (%x-type) in a SQL string
  void DoSQLSubstitutions(string &aSQL);
  // insert additional data selection conditions, if any.
  bool appendFiltersClause(string &aSQL, const char *linktext);
public:
  // - do substitutions for data table access
  void DoDataSubstitutions(
    string &aSQL,                   // string to apply substitutions to
    TFieldMapList &aFieldMapList,   // field map list for %N,%V and %v
    uInt16 aSetNo,                  // Map Set number
    bool aForWrite,                 // set if read-enabled or write-enabled fields are shown in %N,%V and %v
    bool aForUpdate=false,          // set if for update (only assigned fields will be shown if !fUpdateAllFields)
    TMultiFieldItem *aItemP=NULL,   // item to read values and localid from
    sInt16 aRepOffset=0,            // array index/repeat offset
    bool *aAllEmptyP=NULL,          // true if all %V or %v empty
    bool *aDoneP=NULL               // true if no data at specified aRepOffset
  );
  // Parameter handling
  // - reset all mapped parameters
  void resetSQLParameterMaps(void);
  // - add parameter definition to the datastore level parameter list
  void addSQLParameterMap(
    bool aInParam, bool aOutParam,
    TParamMode aParamMode,
    TODBCFieldMapItem *aFieldMapP,
    TMultiFieldItem *aItemP,
    sInt16 aRepOffset
  );
  // - prepare SQL statment as far as needed for parameter binding
  void prepareSQLStatement(SQLHSTMT aStatement, cAppCharP aSQL, bool aForData, cAppCharP aComment);
  // - bind parameters (and values for IN-Params) to the statement
  void bindSQLParameters(SQLHSTMT aStatement, bool aForData);
  // - save out parameter values and clean up
  void saveAndCleanupSQLParameters(SQLHSTMT aStatement, bool aForData);
  // - fetch next row from SQL statement, returns true if there is any
  bool fetchNextRow(SQLHSTMT aStatement, bool aForData);
  // - SQL statement complete, finalize it
  void finalizeSQLStatement(SQLHSTMT aStatement, bool aForData);
protected:
  #ifdef OBJECT_FILTERING
  // - convert filter expression into SQL WHERE clause condition
  bool appendFilterConditions(string &aSQL, const string &aFilter);
  // - appends logical condition term to SQL from filter string
  bool appendFilterTerm(string &aSQL, const char *&aPos, const char *aStop);
  #endif
  #ifdef ODBCAPI_SUPPORT
  // - get one or two successive columns as time stamp depending on config
  void getColumnsAsTimestamp(
    SQLHSTMT aStatement,
    sInt16 &aColNumber, // will be updated by 1 or 2
    bool aCombined,
    lineartime_t &aTimestamp,
    timecontext_t aTargetContext
  );
  #endif
  // - get a column as integer based timestamp
  lineartime_t dbIntToLineartimeAs(
    sInt64 aDBInt, TDBFieldType aDbfty,
    timecontext_t aTargetContext
  );
  // - helpers for mapping field values
  bool addFieldNameList(string &aSQL, bool aForWrite, bool aForUpdate, bool aAssignedOnly, TMultiFieldItem *aItemP, TFieldMapList &fml,uInt16 aSetNo, char aFirstSep=0);
  bool addFieldNameValueList(string &aSQL, bool aAssignedOnly, bool &aNoData, bool &aAllEmpty, TMultiFieldItem &aItem, bool aForUpdate,TFieldMapList &fml, uInt16 aSetNo, sInt16 aRepOffset=0, char aFirstSep=0);
  void fillFieldsFromSQLResult(SQLHSTMT aStatement, sInt16 &aColIndex, TMultiFieldItem &aItem, TFieldMapList &fml, uInt16 aSetNo, sInt16 aRepOffset=0);
  // - add field value as literal to SQL text
  bool appendFieldsLiteral(TMultiFieldItem &aItem, sInt16 aFid, sInt16 aRepOffset,TODBCFieldMapItem &aFieldMapping, string &aSQL);
public:
  bool appendFieldValueLiteral(TItemField &aField,TDBFieldType aDBFieldType, uInt32 aMaxSize, bool aIsFloating, string &aSQL);
protected:
  #ifdef ARRAYDBTABLES_SUPPORT
  // array table field access basic functions
  void readArray(SQLHSTMT aStatement, TMultiFieldItem &aItem, TFieldMapArrayItem *aMapItemP);
  void writeArray(bool aDelete, bool aInsert, SQLHSTMT aStatement, TMultiFieldItem &aItem, TFieldMapArrayItem *aMapItemP);
  // accessing all array fields of a item
  void readArrayFields(SQLHSTMT aStatement, TMultiFieldItem &aItem, TFieldMapList &fml);
  void writeArrayFields(SQLHSTMT aStatement, TMultiFieldItem &aItem, TFieldMapList &fml, bool aInsert);
  void deleteArrayFields(SQLHSTMT aStatement, TMultiFieldItem &aItem, TFieldMapList &fml);
  #endif
  // other private utils
  // - private helper: start writing item
  void startWriteItem(void);
  // - private helper: end writing item
  void endWriteItem(void);
  // - create local ID with special algorithm
  void createLocalID(string &aLocalID,TSpecialIDMode aSpecialIDMode);
protected:
  // Fully ODBC based DB
  bool fODBCAdminData; // set if admin data is actually in ODBC tables (and not managed in derived class' implementation)
  // StartDataRead/GetItem vars
  sInt32 fNextLocalID;
  // Parameters
  TParameterMapList fParameterMaps;
  // Sum of all string sizes per record
  sInt32 fRecordSize;
  // Filtering
  #ifdef OBJECT_FILTERING
  bool fFilterExpressionTested;
  bool fFilterWorksOnDBLevel; // set if filter can be executed by DB
  #endif
  // ODBC
  #ifdef ODBCAPI_SUPPORT
  SQLHDBC fODBCConnectionHandle;
  #endif
  // Note: %%% we need the statements as dummies even if ODBC is not compiled in
  SQLHSTMT fODBCReadStatement;
  SQLHSTMT fODBCWriteStatement;
  // SQLite
  #ifdef SQLITE_SUPPORT
  bool fUseSQLite;
  sqlite3 *fSQLiteP;
  sqlite3_stmt *fSQLiteStmtP;
  int fStepRc;
  #endif
}; // TODBCApiDS


#ifdef STREAMFIELD_SUPPORT

// proxy for loading blobs
class TODBCFieldProxy : public TBlobProxy
{
  typedef TBlobProxy inherited;
public:
  TODBCFieldProxy(TODBCApiDS *aODBCdsP, TODBCFieldMapItem *aFieldMapP, const char *aMasterKey, const char *aDetailKey);
  virtual ~TODBCFieldProxy();
  // - returns size of entire blob
  virtual size_t getBlobSize(TStringField *aFieldP);
  // - read from Blob from specified stream position and update stream pos
  virtual size_t readBlobStream(TStringField *aFieldP, size_t &aPos, void *aBuffer, size_t aMaxBytes);
  // - dependency on a local ID
  virtual void setParentLocalID(const char *aParentLocalID) { fMasterKey=aParentLocalID; };
private:
  // fetch BLOB from DPAPI
  void fetchBlob(void);
  // Vars
  TODBCApiDS *fODBCdsP; // datastore which can be asked to retrieve data
  TODBCFieldMapItem *fFieldMapP; // field map item config (contains SQL needed to fetch BLOB field)
  bool fIsStringBLOB; // if set, the blob must be treated as a string (applying DB charset conversions)
  string fMasterKey; // key of master (main) record containing the field to be retrieved
  string fDetailKey; // optional key of detail record, available only when "keyfield" attribute is present in <map>
  bool fFetched; // already fetched (if value is empty, fBlobBuffer may still be NULL)
  string fValue; // string or binary value
}; // TODBCFieldProxy

#endif


} // namespace sysync

#endif // ODBCAPIDS_H

#endif // SQL_SUPPORT

// eof
