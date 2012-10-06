/**
 *  @File     odbcapids.cpp
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

#ifdef SQL_SUPPORT

// includes
#include "sysync.h"
#include "odbcapids.h"
#include "odbcapiagent.h"


// sanity check for current implementation - either SQLite or ODBC must be enabled
#if !defined(ODBCAPI_SUPPORT) && !defined(SQLITE_SUPPORT)
  #error "ODBC or SQLite API must be enabled"
#endif


namespace sysync {

// special ID mode names
const char * const SpecialIDModeNames[numSpecialIDModes] = {
  "none",
  "unixmsrnd6"
};



#ifdef SCRIPT_SUPPORT

class TODBCDSfuncs {
public:

  // ODBC datastore specific script functions
  // ========================================

  // SETSQLFILTER(string filter)
  static void func_SetSQLFilter(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aFuncContextP->getLocalVar(0)->getAsString(
      static_cast<TODBCApiDS *>(aFuncContextP->getCallerContext())->fSQLFilter
    );
  }; // func_SetSQLFilter


  // ADDBYUPDATING(string idtoupdate)
  static void func_AddByUpdating(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TODBCApiDS *dsP = static_cast<TODBCApiDS *>(aFuncContextP->getCallerContext());
    // only if inserting, convert to updating an existing record
    if (dsP->fInserting) {
      string localid;
      aFuncContextP->getLocalVar(0)->getAsString(localid);
      aFuncContextP->fParentContextP->fTargetItemP->setLocalID(localid.c_str());
      // switch off inserting from here on, such that UPDATE SQL will be used
      dsP->fInserting = false;
    }
  }; // func_AddByUpdating


  #ifdef SQLITE_SUPPORT
  // integer SQLITELASTID()
  // get ROWID created by last insert
  static void func_SQLGetLastID(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TODBCApiDS *dsP = static_cast<TODBCApiDS *>(aFuncContextP->getCallerContext());
    if (dsP->fUseSQLite && dsP->fSQLiteP) {
      aTermP->setAsInteger(
        sqlite3_last_insert_rowid(dsP->fSQLiteP)
      );
    }
    else {
      aTermP->unAssign();
    }
  }; // func_SQLGetLastID
  #endif // SQLITE_SUPPORT

}; // TODBCDSfuncs


const uInt8 param_OneStr[] = { VAL(fty_string) };

// builtin function table for datastore level
const TBuiltInFuncDef ODBCDSFuncDefs[] = {
  { "SETSQLFILTER", TODBCDSfuncs::func_SetSQLFilter, fty_none, 1, param_OneStr },
  { "ADDBYUPDATING", TODBCDSfuncs::func_AddByUpdating, fty_none, 1, param_OneStr },
  #ifdef SQLITE_SUPPORT
  { "SQLITELASTID", TODBCDSfuncs::func_SQLGetLastID, fty_integer, 0, NULL },
  #endif
};


// chain to generic local datastore funcs
static void *ODBCDSChainFunc2(void *&aCtx)
{
  // context pointer for datastore-level funcs is the datastore
  // -> no change needed
  // next table is customimplds's (and then the localdatastore's from there)
  return (void *)&CustomDSFuncTable2;
} // ODBCDSChainFunc

const TFuncTable ODBCDSFuncTable2 = {
  sizeof(ODBCDSFuncDefs) / sizeof(TBuiltInFuncDef), // size of table
  ODBCDSFuncDefs, // table pointer
  ODBCDSChainFunc2 // chain generic DB functions
};



#endif // SCRIPT_SUPPORT



// Config
// ======

TOdbcDSConfig::TOdbcDSConfig(const char* aName, TConfigElement *aParentElement) :
  inherited(aName,aParentElement)
{
  // nop so far
  clear();
  // except ensure we have a decent rand()
  srand((uInt32)time(NULL));
} // TOdbcDSConfig::TOdbcDSConfig


TOdbcDSConfig::~TOdbcDSConfig()
{
  // nop so far
} // TOdbcDSConfig::~TOdbcDSConfig


// init defaults
void TOdbcDSConfig::clear(void)
{
  // init defaults
  // - commit modes
  fCommitItems=false; // commit item changes all at end of write by default
  // - retries
  fInsertRetries=0; // none
  fUpdateRetries=0; // none
  // - folder key
  #ifdef HAS_SQL_ADMIN
  fFolderKeySQL.erase();
  // - Sync target table
  fGetSyncTargetSQL.erase();
  fNewSyncTargetSQL.erase();
  fUpdateSyncTargetSQL.erase();
  fDeleteSyncTargetSQL.erase();
  fSyncTimestamp=true;
  #ifdef OLD_1_0_5_CONFIG_COMPATIBLE
  // Compatibility with old config files
  // - layout of map table
  fMapTableName.erase();
  fRemoteIDMapField.erase();
  fLocalIDMapField.erase();
  fStringLocalID=true;
  fTargetKeyMapField.erase();
  fStringTargetkey=true;
  // - layout of data table
  fLocalTableName.erase();
  // - name of folderkey subselection field in data table, empty if none
  fFolderKeyField.erase();
  fStringFolderkey=true;
  // - name of field which contains Local ID (GUID)
  fLocalIDField.erase();
  // - mod date/time
  fModifiedDateField.erase();
  fModifiedTimeField.erase();
  #endif // OLD_1_0_5_CONFIG_COMPATIBLE
  // Map table access, new version
  // - SQL to fetch all map entries of a target
  fMapFetchAllSQL.erase();
  // - SQL to insert new map entry by localid
  fMapInsertSQL.erase();
  // - SQL to update existing map entry by localid
  fMapUpdateSQL.erase();
  // - SQL to delete a map entry by localid
  fMapDeleteSQL.erase();
  #endif // HAS_SQL_ADMIN
  // Data table access, new version
  // - SQL to fetch local ID and timestamp from data table
  fLocalIDAndTimestampFetchSQL.erase();
  fModifiedTimestamp=true;
  // - SQL to fetch actual data fields from data table. SQL result must
  //   contain mapped fields in the same order as they appear in <fieldmap>
  fDataFetchSQL.erase();
  // - SQL to insert new data in a record, including modification date
  fDataInsertSQL.erase();
  // - SQL to update actual data in a record, including modification date
  fDataUpdateSQL.erase();
  // - SQL to delete a record
  fDataDeleteSQL.erase();
  // - SQL statement(s) to zap entire sync set
  fDataZapSQL.erase();
  // Data table options
  // - try to translate filters to SQL if possible
  fFilterOnDBLevel=false; // don't try
  // - quoting mode for string literals
  fQuotingMode=qm_duplsingle; // default to what was hard-coded before it became configurable in 2.1.1.5
  // - Obtaining ID for new records
  fDetermineNewIDOnce=false;
  fObtainNewIDAfterInsert=false;
  fObtainNewLocalIDSql.erase();
  #ifdef SCRIPT_SUPPORT
  fLocalIDScript.erase();
  #endif
  fMinNextID=1000000;
  fSpecialIDMode=sidm_none;
  fInsertReturnsID=false;
  fInsertIDAsOutParam=false;
  fIgnoreAffectedCount=false;
  fLastModDBFieldType=dbft_timestamp; // default to what we always had before 3.1 engine for ODBC
  // SQLite support
  #ifdef SQLITE_SUPPORT
  fSQLiteFileName.erase();
  fSQLiteBusyTimeout=15; // 15 secs
  #endif
  // clear inherited
  inherited::clear();
} // TOdbcDSConfig::clear


// config element parsing
bool TOdbcDSConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // commit modes
  if (strucmp(aElementName,"commititems")==0)
    expectBool(fCommitItems);
  // retries
  else if (strucmp(aElementName,"insertretries")==0)
    expectInt16(fInsertRetries);
  else if (strucmp(aElementName,"updateretries")==0)
    expectInt16(fUpdateRetries);
  #ifdef HAS_SQL_ADMIN
  // - folder key
  else if (strucmp(aElementName,"folderkeysql")==0)
    expectString(fFolderKeySQL);
  // - Sync target table handling
  else if (strucmp(aElementName,"synctargetgetsql")==0)
    expectString(fGetSyncTargetSQL);
  else if (strucmp(aElementName,"synctargetnewsql")==0)
    expectString(fNewSyncTargetSQL);
  else if (strucmp(aElementName,"synctargetupdatesql")==0)
    expectString(fUpdateSyncTargetSQL);
  else if (strucmp(aElementName,"synctargetdeletesql")==0)
    expectString(fDeleteSyncTargetSQL);
  else if (strucmp(aElementName,"synctimestamp")==0)
    expectBool(fSyncTimestamp);
  #ifdef OLD_1_0_5_CONFIG_COMPATIBLE
  // Compatibility with old config files
  // - layout of map table
  else if (strucmp(aElementName,"maptablename")==0)
    expectString(fMapTableName);
  else if (strucmp(aElementName,"remoteidmapfield")==0)
    expectString(fRemoteIDMapField);
  else if (strucmp(aElementName,"localidmapfield")==0)
    expectString(fLocalIDMapField);
  else if (strucmp(aElementName,"stringlocalid")==0)
    expectBool(fStringLocalID);
  else if (strucmp(aElementName,"targetkeymapfield")==0)
    expectString(fTargetKeyMapField);
  else if (strucmp(aElementName,"stringtargetkey")==0)
    expectBool(fStringTargetkey);
  // - layout of data table
  else if (strucmp(aElementName,"datatablename")==0) {
    ReportError(false,"Warning: old-style config - use full SQL statements with %%xx replacement sequences instead");
    expectString(fLocalTableName);
  }
  else if (strucmp(aElementName,"folderkeyfield")==0)
    expectString(fFolderKeyField);
  else if (strucmp(aElementName,"stringfolderkey")==0)
    expectBool(fStringFolderkey);
  else if (strucmp(aElementName,"localidfield")==0)
    expectString(fLocalIDField);
  else if (strucmp(aElementName,"moddatefield")==0)
    expectString(fModifiedDateField);
  else if (strucmp(aElementName,"modtimefield")==0)
    expectString(fModifiedTimeField);
  // Note: was never documented so will be phased out right now
  // else if (strucmp(aElementName,"genselectcond")==0)
  //   expectString(fGeneralSelectCondition);
  #endif // OLD_1_0_5_CONFIG_COMPATIBLE
  // Map table access, new version
  else if (strucmp(aElementName,"selectmapallsql")==0)
    expectString(fMapFetchAllSQL);
  else if (strucmp(aElementName,"insertmapsql")==0)
    expectString(fMapInsertSQL);
  else if (strucmp(aElementName,"updatemapsql")==0)
    expectString(fMapUpdateSQL);
  else if (strucmp(aElementName,"deletemapsql")==0)
    expectString(fMapDeleteSQL);
  #endif // HAS_SQL_ADMIN
  // Data table access, new version
  else if (strucmp(aElementName,"quotingmode")==0)
    expectEnum(sizeof(fQuotingMode),&fQuotingMode,quotingModeNames,numQuotingModes);
  else if (strucmp(aElementName,"dbcanfilter")==0)
    expectBool(fFilterOnDBLevel);
  else if (strucmp(aElementName,"selectidandmodifiedsql")==0)
    expectString(fLocalIDAndTimestampFetchSQL);
  else if (strucmp(aElementName,"modtimestamp")==0)
    expectBool(fModifiedTimestamp);
  else if (strucmp(aElementName,"selectdatasql")==0)
    expectString(fDataFetchSQL);
  else if (strucmp(aElementName,"insertdatasql")==0)
    expectString(fDataInsertSQL);
  else if (strucmp(aElementName,"updatedatasql")==0)
    expectString(fDataUpdateSQL);
  else if (strucmp(aElementName,"deletedatasql")==0)
    expectString(fDataDeleteSQL);
  else if (strucmp(aElementName,"zapdatasql")==0)
    expectString(fDataZapSQL);
  // - Obtaining ID for new records
  else if (strucmp(aElementName,"determineidonce")==0)
    expectBool(fDetermineNewIDOnce);
  else if (strucmp(aElementName,"obtainidafterinsert")==0)
    expectBool(fObtainNewIDAfterInsert);
  else if (strucmp(aElementName,"obtainlocalidsql")==0)
    expectString(fObtainNewLocalIDSql);
  else if (strucmp(aElementName,"minnextid")==0)
    expectInt32(fMinNextID);
  #ifdef SCRIPT_SUPPORT
  else if (strucmp(aElementName,"localidscript")==0)
    expectScript(fLocalIDScript, aLine, getDSFuncTableP());
  #endif
  else if (strucmp(aElementName,"specialidmode")==0)
    expectEnum(sizeof(fSpecialIDMode),&fSpecialIDMode,SpecialIDModeNames,numSpecialIDModes);
  else if (strucmp(aElementName,"insertreturnsid")==0)
    expectBool(fInsertReturnsID);
  else if (strucmp(aElementName,"idasoutparam")==0) // %%% possibly obsolete, replaced by %pkos and %pkoi
    expectBool(fInsertIDAsOutParam);
  else if (strucmp(aElementName,"ignoreaffectedcount")==0)
    expectBool(fIgnoreAffectedCount);
  else if (strucmp(aElementName,"lastmodfieldtype")==0)
    expectEnum(sizeof(fLastModDBFieldType),&fLastModDBFieldType,DBFieldTypeNames,numDBfieldTypes);
  // - SQLite support
  #ifdef SQLITE_SUPPORT
  else if (strucmp(aElementName,"sqlitefile")==0)
    expectMacroString(fSQLiteFileName);
  else if (strucmp(aElementName,"sqlitebusytimeout")==0)
    expectUInt32(fSQLiteBusyTimeout);
  #endif
  // - field mappings
  else if (strucmp(aElementName,"fieldmap")==0) {
    // check reference argument
    const char* ref = getAttr(aAttributes,"fieldlist");
    if (!ref) {
      ReportError(true,"fieldmap missing 'fieldlist' attribute");
    }
    else {
      // look for field list
      TMultiFieldDatatypesConfig *mfcfgP =
        dynamic_cast<TMultiFieldDatatypesConfig *>(getSyncAppBase()->getRootConfig()->fDatatypesConfigP);
      if (!mfcfgP) throw TConfigParseException("no multifield config");
      TFieldListConfig *cfgP = mfcfgP->getFieldList(ref);
      if (!cfgP)
        return fail("fieldlist '%s' not defined for fieldmap",ref);
      // - store field list reference in map
      fFieldMappings.fFieldListP=cfgP;
      // - let element handle parsing
      expectChildParsing(fFieldMappings);
    }
  }
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TOdbcDSConfig::localStartElement


// resolve
void TOdbcDSConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    #ifndef ODBC_UNICODE
    // make sure we don't try to use UTF-16
    if (fDataCharSet==chs_utf16)
      SYSYNC_THROW(TConfigParseException("UTF-16 wide character set can only be used on ODBC-Unicode enabled platforms"));
    #endif
    #ifdef SQLITE_SUPPORT
    // %%% NOP at this time
    #endif
    #ifdef OLD_1_0_5_CONFIG_COMPATIBLE
    // generate new settings from old ones
    if (fLocalIDAndTimestampFetchSQL.empty()) {
      // SELECT localid,modifieddate(,modifiedtime)...
      fLocalIDAndTimestampFetchSQL="SELECT ";
      fLocalIDAndTimestampFetchSQL+=fLocalIDField;
      fLocalIDAndTimestampFetchSQL+=", ";
      fLocalIDAndTimestampFetchSQL += fModifiedDateField;
      if (!fModifiedTimestamp && !fModifiedTimeField.empty()) {
        fLocalIDAndTimestampFetchSQL += ", ";
        fLocalIDAndTimestampFetchSQL += fModifiedTimeField;
      }
      // ...FROM datatable
      fLocalIDAndTimestampFetchSQL+=" FROM ";
      fLocalIDAndTimestampFetchSQL+=fLocalTableName;
      // ...WHERE folderkey=x...
      fLocalIDAndTimestampFetchSQL+=" WHERE ";
      fLocalIDAndTimestampFetchSQL+=fFolderKeyField;
      fLocalIDAndTimestampFetchSQL+='=';
      quoteStringAppend("%f",fLocalIDAndTimestampFetchSQL,fStringFolderkey ? qm_backslash : qm_none);
      // ...AND filterclause
      fLocalIDAndTimestampFetchSQL+="%AF";
      PDEBUGPRINTFX(DBG_EXOTIC,(" <selectidandmodifiedsql>%s</selectidandmodifiedsql>",fLocalIDAndTimestampFetchSQL.c_str()));
    }
    if (fDataFetchSQL.empty()) {
      // SELECT %N FROM datatable WHERE localid=%k AND folderkey=%f
      fDataFetchSQL="SELECT %N FROM ";
      fDataFetchSQL+=fLocalTableName;
      fDataFetchSQL+=" WHERE ";
      fDataFetchSQL+=fLocalIDField;
      fDataFetchSQL+='=';
      quoteStringAppend("%k",fDataFetchSQL,fStringLocalID ? qm_backslash : qm_none);
      fDataFetchSQL+=" AND ";
      fDataFetchSQL+=fFolderKeyField;
      fDataFetchSQL+='=';
      quoteStringAppend("%f",fDataFetchSQL,fStringFolderkey ? qm_backslash : qm_none);
      PDEBUGPRINTFX(DBG_EXOTIC,(" <selectdatasql>%s</selectdatasql>",fDataFetchSQL.c_str()));
    }
    if (fDataInsertSQL.empty()) {
      // INSERT INTO datatable (keyfield,modtimestamp,folderkey,%N) VALUES (%k,%M,%f,%v)
      fDataInsertSQL="INSERT INTO ";
      fDataInsertSQL+=fLocalTableName;
      fDataInsertSQL+=" (";
      if (fDetermineNewIDOnce || !fObtainNewIDAfterInsert) {
        // insert key only if previously generated separately
        fDataInsertSQL+=fLocalIDField;
        fDataInsertSQL+=", ";
      }
      fDataInsertSQL += fModifiedDateField;
      if (!fModifiedTimestamp && !fModifiedTimeField.empty()) {
        fDataInsertSQL += ", ";
        fDataInsertSQL += fModifiedTimeField;
      }
      if (!fFolderKeyField.empty()) {
        fDataInsertSQL += ", ";
        fDataInsertSQL+=fFolderKeyField;
      }
      fDataInsertSQL+=", %N) VALUES (";
      if (fDetermineNewIDOnce || !fObtainNewIDAfterInsert) {
        // insert key only if previously generated separately
        quoteStringAppend("%k",fDataInsertSQL,fStringLocalID ? qm_backslash : qm_none);
        fDataInsertSQL += ", ";
      }
      if (!fModifiedTimestamp) {
        fDataInsertSQL += "%dM";
        if (!fModifiedTimeField.empty()) fDataInsertSQL += ", %tM";
      }
      else {
        fDataInsertSQL += "%M";
      }
      if (!fFolderKeyField.empty()) {
        fDataInsertSQL += ", %f";
      }
      fDataInsertSQL+=", %v)";
      PDEBUGPRINTFX(DBG_EXOTIC,(" <insertdatasql>%s</insertdatasql>",fDataInsertSQL.c_str()));
    }
    if (fDataUpdateSQL.empty()) {
      // UPDATE datatable SET modtimestamp=%M, %V WHERE localid=%k
      fDataUpdateSQL="UPDATE ";
      fDataUpdateSQL+=fLocalTableName;
      fDataUpdateSQL+=" SET ";
      if (!fModifiedTimestamp) {
        fDataUpdateSQL += fModifiedDateField;
        fDataUpdateSQL += "=%dM";
        if (!fModifiedTimeField.empty()) {
          fDataUpdateSQL += ", ";
          fDataUpdateSQL += fModifiedTimeField;
          fDataUpdateSQL += "=%tM";
        }
      }
      else {
        fDataUpdateSQL += fModifiedDateField;
        fDataUpdateSQL += "=%M";
      }
      fDataUpdateSQL += ", %V WHERE ";
      fDataUpdateSQL+=fLocalIDField;
      fDataUpdateSQL += "=";
      quoteStringAppend("%k",fDataUpdateSQL,fStringLocalID ? qm_backslash : qm_none);
      PDEBUGPRINTFX(DBG_EXOTIC,(" <updatedatasql>%s</updatedatasql>",fDataUpdateSQL.c_str()));
    }
    if (fDataDeleteSQL.empty()) {
      // DELETE FROM datatable WHERE localid=%k
      fDataDeleteSQL="DELETE FROM ";
      fDataDeleteSQL+=fLocalTableName;
      fDataDeleteSQL+=" WHERE ";
      fDataDeleteSQL+=fLocalIDField;
      fDataDeleteSQL+="=";
      quoteStringAppend("%k",fDataDeleteSQL,fStringLocalID ? qm_backslash : qm_none);
      PDEBUGPRINTFX(DBG_EXOTIC,(" <deletedatasql>%s</deletedatasql>",fDataDeleteSQL.c_str()));
    }
    if (fDataZapSQL.empty() && !fLocalTableName.empty()) {
      // DELETE FROM datatable
      fDataZapSQL="DELETE FROM ";
      fDataZapSQL+=fLocalTableName;
      // ...WHERE folderkey=x...
      fDataZapSQL+=" WHERE ";
      fDataZapSQL+=fFolderKeyField;
      fDataZapSQL+='=';
      quoteStringAppend("%f",fDataZapSQL,fStringFolderkey ? qm_backslash : qm_none);
      // ...AND filterclause
      fDataZapSQL+="%AF";
      PDEBUGPRINTFX(DBG_EXOTIC,(" <zapdatasql>%s</zapdatasql>",fDataZapSQL.c_str()));
    }
    // Map table
    if (fMapFetchAllSQL.empty()) {
      // SELECT localid,remoteid FROM maptable WHERE targetkey=%t
      fMapFetchAllSQL="SELECT ";
      fMapFetchAllSQL+=fLocalIDMapField;
      fMapFetchAllSQL+=", ";
      fMapFetchAllSQL+=fRemoteIDMapField;
      fMapFetchAllSQL+=" FROM ";
      fMapFetchAllSQL+=fMapTableName;
      fMapFetchAllSQL+=" WHERE ";
      fMapFetchAllSQL+=fTargetKeyMapField;
      fMapFetchAllSQL+="=";
      quoteStringAppend("%t",fMapFetchAllSQL,fStringTargetkey ? qm_backslash : qm_none);
      PDEBUGPRINTFX(DBG_EXOTIC,(" <selectmapallsql>%s</selectmapallsql>",fMapFetchAllSQL.c_str()));
    }
    if (fMapInsertSQL.empty()) {
      // INSERT INTO maptable (targetkey,localid,remoteid) VALUES (%t,%k,%r)
      fMapInsertSQL="INSERT INTO ";
      fMapInsertSQL+=fMapTableName;
      fMapInsertSQL+=" (";
      fMapInsertSQL+=fTargetKeyMapField;
      fMapInsertSQL+=", ";
      fMapInsertSQL+=fLocalIDMapField;
      fMapInsertSQL+=", ";
      fMapInsertSQL+=fRemoteIDMapField;
      fMapInsertSQL+=") VALUES (";
      quoteStringAppend("%t",fMapInsertSQL,fStringTargetkey ? qm_backslash : qm_none);
      fMapInsertSQL+=", ";
      quoteStringAppend("%k",fMapInsertSQL,fStringLocalID ? qm_backslash : qm_none);
      fMapInsertSQL+=", '%r')";
      PDEBUGPRINTFX(DBG_EXOTIC,(" <insertmapsql>%s</insertmapsql>",fMapInsertSQL.c_str()));
    }
    if (fMapUpdateSQL.empty()) {
      // UPDATE maptable SET remoteid=%r WHERE targetkey=%t AND localid=%k
      fMapUpdateSQL="UPDATE ";
      fMapUpdateSQL+=fMapTableName;
      fMapUpdateSQL+=" SET ";
      fMapUpdateSQL+=fRemoteIDMapField;
      fMapUpdateSQL+="='%r' WHERE "; // is always a string
      fMapUpdateSQL+=fTargetKeyMapField;
      fMapUpdateSQL+="=";
      quoteStringAppend("%t",fMapUpdateSQL,fStringTargetkey ? qm_backslash : qm_none);
      fMapUpdateSQL+=" AND ";
      fMapUpdateSQL+=fLocalIDMapField;
      fMapUpdateSQL+="=";
      quoteStringAppend("%k",fMapUpdateSQL,fStringLocalID ? qm_backslash : qm_none);
      PDEBUGPRINTFX(DBG_EXOTIC,(" <updatemapsql>%s</updatemapsql>",fMapUpdateSQL.c_str()));
    }
    if (fMapDeleteSQL.empty()) {
      // DELETE FROM maptable WHERE targetkey=%t AND localid=%k
      fMapDeleteSQL="DELETE FROM ";
      fMapDeleteSQL+=fMapTableName;
      fMapDeleteSQL+=" WHERE ";
      fMapDeleteSQL+=fTargetKeyMapField;
      fMapDeleteSQL+="=";
      quoteStringAppend("%t",fMapDeleteSQL,fStringTargetkey ? qm_backslash : qm_none);
      // up to here, this is a DELETE all for one target, so we
      // can use this to extend fDeleteSyncTargetSQL
      fDeleteSyncTargetSQL+=" %GO ";
      fDeleteSyncTargetSQL+=fMapDeleteSQL;
      // now add clause for single item
      fMapDeleteSQL+=" AND ";
      fMapDeleteSQL+=fLocalIDMapField;
      fMapDeleteSQL+="=";
      quoteStringAppend("%k",fMapDeleteSQL,fStringLocalID ? qm_backslash : qm_none);
      PDEBUGPRINTFX(DBG_EXOTIC,(" <deletemapsql>%s</deletemapsql>",fMapDeleteSQL.c_str()));
    }
    #endif
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TOdbcDSConfig::localResolve


#ifdef SCRIPT_SUPPORT
void TOdbcDSConfig::apiResolveScripts(void)
{
  // resolve scripts which are API specific (in SAME ORDER AS IN apiRebuildScriptContexts()!)
  TScriptContext::resolveScript(getSyncAppBase(),fLocalIDScript,fResolveContextP,NULL);
}
#endif


// - create appropriate datastore from config, calls addTypeSupport as well
TLocalEngineDS *TOdbcDSConfig::newLocalDataStore(TSyncSession *aSessionP)
{
  // Synccap defaults to normal set supported by the engine by default
  TLocalEngineDS *ldsP;
  if (IS_CLIENT) {
    ldsP = new TODBCApiDS(this,aSessionP,getName(),aSessionP->getSyncCapMask() & ~(isOneWayFromRemoteSupported() ? 0 : SCAP_MASK_ONEWAY_SERVER));
  }
  else {
    ldsP = new TODBCApiDS(this,aSessionP,getName(),aSessionP->getSyncCapMask() & ~(isOneWayFromRemoteSupported() ? 0 : SCAP_MASK_ONEWAY_CLIENT));
  }
  // do common stuff
  addTypes(ldsP,aSessionP);
  // return
  return ldsP;
} // TOdbcDSConfig::newLocalDataStore



// Field Map item
// ==============

TODBCFieldMapItem::TODBCFieldMapItem(const char *aElementName, TConfigElement *aParentElement) :
  inherited(aElementName,aParentElement)
{
  #ifdef STREAMFIELD_SUPPORT
  fReadBlobSQL.erase();
  fKeyFieldName.erase();
  #endif
} // TODBCFieldMapItem::TODBCFieldMapItem


void TODBCFieldMapItem::checkAttrs(const char **aAttributes)
{
  #ifdef STREAMFIELD_SUPPORT
  AssignString(fReadBlobSQL,getAttr(aAttributes,"readblobsql"));
  AssignString(fKeyFieldName,getAttr(aAttributes,"keyfield"));
  #endif
  inherited::checkAttrs(aAttributes);
} // TODBCFieldMapItem::checkAttrs



#ifdef ARRAYDBTABLES_SUPPORT

// array container
// ===============

TODBCFieldMapArrayItem::TODBCFieldMapArrayItem(TCustomDSConfig *aCustomDSConfigP, TConfigElement *aParentElement) :
  inherited(aCustomDSConfigP,aParentElement)
{
  clear();
} // TODBCFieldMapArrayItem::TODBCFieldMapArrayItem


TODBCFieldMapArrayItem::~TODBCFieldMapArrayItem()
{
  // nop so far
  clear();
} // TODBCFieldMapArrayItem::~TODBCFieldMapArrayItem


// init defaults
void TODBCFieldMapArrayItem::clear(void)
{
  // init defaults
  // - clear values
  fSelectArraySQL.erase();
  fInsertElementSQL.erase();
  fDeleteArraySQL.erase();
  fAlwaysCleanArray=false;
  // clear inherited
  inherited::clear();
} // TODBCFieldMapArrayItem::clear


// config element parsing
bool TODBCFieldMapArrayItem::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  if (strucmp(aElementName,"selectarraysql")==0)
    expectString(fSelectArraySQL);
  else if (strucmp(aElementName,"deletearraysql")==0)
    expectString(fDeleteArraySQL);
  else if (strucmp(aElementName,"insertelementsql")==0)
    expectString(fInsertElementSQL);
  else if (strucmp(aElementName,"alwaysclean")==0)
    expectBool(fAlwaysCleanArray);
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TODBCFieldMapArrayItem::localStartElement


// resolve
void TODBCFieldMapArrayItem::localResolve(bool aLastPass)
{
  if (aLastPass) {
    // check for required settings
    // %%% tbd
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TODBCFieldMapArrayItem::localResolve

#endif // ARRAYDBTABLES_SUPPORT


/*
 * Implementation of TODBCApiDS
 */


// constructor
TODBCApiDS::TODBCApiDS(
  TOdbcDSConfig *aConfigP,
  sysync::TSyncSession *aSessionP,
  const char *aName,
  uInt32 aCommonSyncCapMask
) :
  inherited(aConfigP,aSessionP, aName, aCommonSyncCapMask)
  #ifdef ODBCAPI_SUPPORT
  ,fODBCConnectionHandle(SQL_NULL_HANDLE) // no connection yet
  ,fODBCReadStatement(SQL_NULL_HANDLE) // no active statements either
  ,fODBCWriteStatement(SQL_NULL_HANDLE)
  #ifdef SCRIPT_SUPPORT
  ,fODBCScriptStatement(SQL_NULL_HANDLE)
  #endif
  #endif // ODBCAPI_SUPPORT
  #ifdef SQLITE_SUPPORT
  ,fSQLiteP(NULL)
  ,fSQLiteStmtP(NULL)
  ,fStepRc(SQLITE_OK)
  #endif
{
  // save pointer to config record
  fConfigP=aConfigP;
  // make a local copy of the typed agent pointer
  fAgentP=static_cast<TODBCApiAgent *>(fSessionP);
  // make a local copy of the typed agent config pointer
  fAgentConfigP = dynamic_cast<TOdbcAgentConfig *>(
    aSessionP->getRootConfig()->fAgentConfigP
  );
  if (!fAgentConfigP) throw TSyncException(DEBUGTEXT("TODBCApiDS finds no AgentConfig","odds7"));
  // init other stuff
  fODBCAdminData=false; // not known yet if we'll get admin data (target, map) from ODBC or not
  // - SQLite support
  #ifdef SQLITE_SUPPORT
  fUseSQLite = !fConfigP->fSQLiteFileName.empty(); // if we have a SQLite file name, use it instead of ODBC
  #endif
  // clear rest
  InternalResetDataStore();
} // TODBCApiDS::TODBCApiDS


TODBCApiDS::~TODBCApiDS()
{
  InternalResetDataStore();
} // TODBCApiDS::~TODBCApiDS


/// @brief called while agent is still fully ok, so we must clean up such that later call of destructor does NOT access agent any more
void TODBCApiDS::announceAgentDestruction(void)
{
  // reset myself
  InternalResetDataStore();
  // make sure we don't access the agent any more
  engTerminateDatastore();
  fAgentP = NULL;
  // call inherited
  inherited::announceAgentDestruction();
} // TODBCApiDS::announceAgentDestruction


/// @brief called to reset datastore
/// @note must be safe to be called multiple times and even after announceAgentDestruction()
void TODBCApiDS::InternalResetDataStore(void)
{
  #ifdef OBJECT_FILTERING
  fFilterExpressionTested=false; // not tested yet
  fFilterWorksOnDBLevel=true; // but assume it works
  #endif
  #ifdef SCRIPT_SUPPORT
  // no SQL filter in place yet
  fSQLFilter.erase();
  #endif
  #ifdef SQLITE_SUPPORT
  // clean up SQLite
  if (fSQLiteP) {
    // stop it in case something is still running
    sqlite3_interrupt(fSQLiteP);
    // if we have a statement, finalize it
    if (fSQLiteStmtP) {
      // discard it
      sqlite3_finalize(fSQLiteStmtP);
      fSQLiteStmtP=NULL;
    }
    int sqrc = sqlite3_close(fSQLiteP);
    if (sqrc!=SQLITE_OK) {
      PDEBUGPRINTFX(DBG_ERROR,("Error closing SQLite data file: sqlite3_close() returns %d",sqrc));
    }
    else {
      PDEBUGPRINTFX(DBG_DBAPI,("Closed SQLite data file"));
    }
    fSQLiteP=NULL;
  }
  #endif
  // Clear map table and sync set lists
  if (fAgentP) {
    #ifdef ODBCAPI_SUPPORT
    // no active statements any more
    if (fODBCReadStatement) {
      SafeSQLFreeHandle(SQL_HANDLE_STMT,fODBCReadStatement);
      fODBCReadStatement=SQL_NULL_HANDLE;
    }
    if (fODBCWriteStatement) {
      SafeSQLFreeHandle(SQL_HANDLE_STMT,fODBCWriteStatement);
      fODBCWriteStatement=SQL_NULL_HANDLE;
    }
    // make sure connection is closed (and open transaction rolled back)
    fAgentP->closeODBCConnection(fODBCConnectionHandle);
    #endif // ODBCAPI_SUPPORT
    // clear parameter maps
    resetSQLParameterMaps();
  }
} // TODBCApiDS::InternalResetDataStore


#ifdef SCRIPT_SUPPORT
// called to rebuild script context for API level scripts in the datastore context
// Note: rebuild order must be SAME AS resolve order in apiResolveScripts()
void TODBCApiDS::apiRebuildScriptContexts(void)
{
  // local ID generation script
  TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fConfigP->fLocalIDScript,fScriptContextP,fSessionP);
} // TODBCApiDS::apiRebuildScriptContexts
#endif


#ifdef ODBCAPI_SUPPORT

// - get (DB level) ODBC handle
SQLHDBC TODBCApiDS::getODBCConnectionHandle(void)
{
  if (fODBCConnectionHandle==SQL_NULL_HANDLE && fAgentP) {
    // get a handle
    PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("Datastore %s does not own a DB connection yet -> pulling connection from session level",getName()));
    fODBCConnectionHandle = fAgentP->pullODBCConnectionHandle();
  }
  PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("Datastore %s: using connection handle 0x%lX",getName(),(uIntArch)fODBCConnectionHandle));
  return fODBCConnectionHandle;
} // TODBCApiDS::getODBCConnectionHandle


// - check for connection-level error
void TODBCApiDS::checkConnectionError(SQLRETURN aResult)
{
  if (!fAgentP) return;
  if (fODBCConnectionHandle!=SQL_NULL_HANDLE) {
    // check on local connection
    fAgentP->checkODBCError(aResult,SQL_HANDLE_DBC,fODBCConnectionHandle);
  }
  // check on session level
  fAgentP->checkConnectionError(aResult);
} // TODBCApiDS::checkConnectionError

#endif // ODBCAPI_SUPPORT


// helper for mapping field names
bool TODBCApiDS::addFieldNameList(string &aSQL, bool aForWrite, bool aForUpdate, bool aAssignedOnly, TMultiFieldItem *aItemP, TFieldMapList &fml, uInt16 aSetNo, char aFirstSep)
{
  TFieldMapList::iterator pos;
  TODBCFieldMapItem *fmiP;

  bool doit;
  bool allfields=true; // all fields listed (no array fields)

  for (pos=fml.begin(); pos!=fml.end(); pos++) {
    fmiP = (TODBCFieldMapItem *)*pos;
    if (!fmiP->isArray()) {
      doit=
        aSetNo==fmiP->setNo &&
        (aForWrite
          ? (fmiP->writable && ((aForUpdate && fmiP->for_update) || (!aForUpdate && fmiP->for_insert)))
          : fmiP->readable
            #ifdef STREAMFIELD_SUPPORT
            && (fmiP->fReadBlobSQL.empty() || !fmiP->fKeyFieldName.empty()) // only read fields here that don't use a proxy or read a key FOR the proxy
            #endif

        );
      if (doit && aAssignedOnly && aForWrite && aItemP) {
        // check base field is assigned
        // This should cover all cases (remember that MimeDirItem assigns empty values
        // to all "available" fields before parsing data)
        // - array fields are "assigned" if they have an element assigned or
        //   been called assignEmpty()
        // - field blocks addressed by repeating properties will either be assigned all
        //   or not at all, so we can safely check base field ID only
        // - negative FIDs except VARIDX_UNDEFINED address local vars, which
        //   should be assigned only if they may also be written
        TItemField *fieldP=getMappedFieldOrVar(*aItemP,fmiP->fid,0,true); // existing only
        if (!fieldP || !(fieldP->isAssigned())) doit=false; // avoid writing unassigned or non-existing (array) fields
      }
      if (doit) {
        // add comma
        if (aFirstSep) { aSQL+=aFirstSep; aSQL+=' '; }
        aFirstSep=','; // further names are always comma separated
        #ifdef STREAMFIELD_SUPPORT
        if (fmiP->readable && !aForWrite && !fmiP->fKeyFieldName.empty()) {
          // the actual field contents will be read via proxy, but we read the record key here instead
          aSQL+=fmiP->fKeyFieldName;
        }
        else
        #endif
        {
          // just add field name
          aSQL+=fmiP->fElementName;
        }
      }
    }
    else
      allfields=false; // at least one array field
  }
  return allfields;
} // TODBCApiDS::addFieldNameList


// helper for mapping field values
// - returns aNoData=true if there is no field (e.g. array index too high) for any of the values
// - returns false if not all fields could be listed (i.e. there are array fields)
bool TODBCApiDS::addFieldNameValueList(string &aSQL, bool aAssignedOnly, bool &aNoData, bool &aAllEmpty, TMultiFieldItem &aItem ,bool aForUpdate, TFieldMapList &fml, uInt16 aSetNo, sInt16 aRepOffset, char aFirstSep)
{
  TFieldMapList::iterator pos;
  TODBCFieldMapItem *fmiP;
  bool allfields=true;
  bool doit;

  aNoData = true; // assume no data
  aAllEmpty = true; // assume empty fields only
  for (pos=fml.begin(); pos!=fml.end(); pos++) {
    fmiP = (TODBCFieldMapItem *)*pos;
    if (fmiP->isArray()) {
      allfields=false; // at least one array field
    }
    else if (aSetNo==fmiP->setNo && fmiP->writable && ((aForUpdate && fmiP->for_update) || (!aForUpdate && fmiP->for_insert)) ) {
      doit=true;
      if (aAssignedOnly) {
        // check base field is assigned (see notes in addFieldNameList())
        TItemField *fieldP=getMappedFieldOrVar(aItem,fmiP->fid,0,true); // get existing fields only (for arrays)
        if (!fieldP || !(fieldP->isAssigned())) doit=false; // avoid writing unassigned fields
      }
      if (doit) {
        // add comma
        if (aFirstSep) { aSQL+=aFirstSep; aSQL+=' '; }
        aFirstSep=','; // further names are always comma separated
        // add field name and equal sign if for update
        if (aForUpdate) {
          // add field name
          aSQL+=fmiP->fElementName;
          aSQL+='=';
        }
        // check how to add field value
        if (fmiP->as_param || fmiP->dbfieldtype==dbft_blob) {
          // either explicitly marked as param or BLOB: pass value as in-param
          addSQLParameterMap(true,false,param_field,fmiP,&aItem,aRepOffset); // in-param
          // add parameter placeholder into SQL
          aSQL+='?';
        }
        else {
          // add field value as literal
          // - get base field
          sInt16 fid=fmiP->fid;
          if (fid==VARIDX_UNDEFINED) return false; // field does not exist
          #ifndef SCRIPT_SUPPORT
          // check index before using it (should not be required, as map indices are resolved
          if (!aItem.getItemType()->isFieldIndexValid(fid)) return false; // field does not exist
          #endif
          if (appendFieldsLiteral(aItem,fid,aRepOffset,*fmiP,aSQL)) aAllEmpty=false;
        }
      }
    }
  }
  aNoData=false; // data for all non-array fields found
  return allfields;
} // TODBCApiDS::addFieldNameValueList


// helper for filling ODBC results into mapped fields
// - if mapped field is an array, aRepOffset will be used as array index
// - if mapped field is not an array, aRepOffset will be added to the base fid
void TODBCApiDS::fillFieldsFromSQLResult(SQLHSTMT aStatement, sInt16 &aColIndex, TMultiFieldItem &aItem, TFieldMapList &fml, uInt16 aSetNo, sInt16 aRepOffset)
{
  sInt16 fid;
  TFieldMapList::iterator pos;
  TODBCFieldMapItem *fmiP;
  bool notnull=false; // default to empty

  // get data for all mapped fields
  for (pos=fml.begin(); pos!=fml.end(); pos++) {
    fmiP = (TODBCFieldMapItem *)*pos;
    try {
      if (
        !fmiP->isArray() &&
        aSetNo==fmiP->setNo &&
        fmiP->readable
      ) {
        // was specified in SELECT, so we can read it
        if ((fid=fmiP->fid)!=VARIDX_UNDEFINED) {
          TItemField *fieldP;
          TDBFieldType dbfty = fmiP->dbfieldtype;
          // single field mapping
          fieldP=getMappedFieldOrVar(aItem,fid,aRepOffset);
          if (fieldP) {
            #ifdef STREAMFIELD_SUPPORT
            if (!fmiP->fReadBlobSQL.empty()) {
              // retrieve key for this field if map specifies a "keyfield"
              string key;
              #ifdef SQLITE_SUPPORT
              // - always for data access
              if (fUseSQLite) {
                if (!fmiP->fKeyFieldName.empty()) {
                  key = (const char *)sqlite3_column_text(fSQLiteStmtP,aColIndex-1);
                  // read something, next column
                  aColIndex++;
                }
              }
              else
              #endif
              {
                #ifdef ODBCAPI_SUPPORT
                if (!fmiP->fKeyFieldName.empty()) {
                  fAgentP->getColumnValueAsString(aStatement,aColIndex,key,chs_ascii);
                  // read something, next column
                  aColIndex++;
                }
                #endif // ODBCAPI_SUPPORT
              }
              // use proxy for this field
              if (!fieldP->isBasedOn(fty_string)) {
                throw TSyncException("<readblobsql> allowed for string or BLOB fields only!");
              }
              // create and install a proxy for this field
              PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,(
                "Installed proxy for '%s' to retrieve data later when needed. MasterKey='%s', 'DetailKey='%s'",
                fmiP->getName(),
                aItem.getLocalID(),
                key.c_str()
              ));
              TODBCFieldProxy *odbcProxyP = new TODBCFieldProxy(this,fmiP,aItem.getLocalID(),key.c_str());
              // attach it to the string or blob field
              static_cast<TStringField *>(fieldP)->setBlobProxy(odbcProxyP);
            }
            else
            #endif // STREAMFIELD_SUPPORT
            {
              // Note: this will also select fields that may not
              //   be available for the current remote party, but these
              //   will be suppressed by the TMultiFieldItemType descendant.
              //   As different items COULD theoretically use different
              //   formats, filtering here already would be complicated.
              //   so we live with reading probably more fields than really
              //   needed.
              #ifdef SQLITE_SUPPORT
              // - always for data access
              if (fUseSQLite) {
                // SQLite
                notnull = fAgentP->getSQLiteColValueAsField(
                  fSQLiteStmtP, // local sqlite statement
                  aColIndex-1, // SQLITE colindex starts at 0, not 1 like in ODBC
                  dbfty,
                  fieldP,
                  fConfigP->fDataCharSet,
                  fmiP->floating_ts ? TCTX_UNKNOWN : fConfigP->fDataTimeZone,
                  fConfigP->fUserZoneOutput
                );
              }
              else
              #endif
              {
                #ifdef ODBCAPI_SUPPORT
                // ODBC
                notnull=fAgentP->getColumnValueAsField(
                  aStatement,
                  aColIndex,
                  dbfty,
                  fieldP,
                  fConfigP->fDataCharSet, // real charset, including UTF16
                  fmiP->floating_ts ? TCTX_UNKNOWN : fConfigP->fDataTimeZone,
                  fConfigP->fUserZoneOutput
                );
                #endif // ODBCAPI_SUPPORT
              }
              // next column
              aColIndex++;
            }
          } // if field available
          else {
            // this can happen with array maps mapping to field blocks with a too high fMaxRepeat
            // it should not happen with array fields.
            throw TSyncException(DEBUGTEXT("FATAL: Field in map not found in item","odds6"));
          }
        } // if fid specified
        else {
          // no field mapped, skip column
          aColIndex++;
        }
      } // if readable, not array and correct set number (and thus SELECTed) field
    }
    catch (exception &e) {
      PDEBUGPRINTFX(DBG_ERROR,(
        "fillFieldsFromSQLResult field='%s', colindex=%hd, fid=%hd%s, setno=%hd, dbfty=%s,failed: %s",
        fmiP->fElementName.c_str(),
        aColIndex,
        fmiP->fid,
        fmiP->isArray() ? " (array)" : "",
        fmiP->setNo,
        DBFieldTypeNames[fmiP->dbfieldtype],
        e.what()
      ));
      throw;
    }
  } // field loop
} // TODBCApiDS::fillFieldsFromSQLResult



#ifdef ARRAYDBTABLES_SUPPORT

// read an array field
// NOTE: non-array fields are already read when this is called
void TODBCApiDS::readArray(SQLHSTMT aStatement, TMultiFieldItem &aItem, TFieldMapArrayItem *aMapItemP)
{
  string sql;
  sInt16 colindex,i;
  uInt16 setno;

  TODBCFieldMapArrayItem *fmaiP = dynamic_cast<TODBCFieldMapArrayItem *>(aMapItemP);
  if (!fmaiP) return; // do nothing
  #ifdef SCRIPT_SUPPORT
  // process init script
  fArrIdx=0; // start at array index=0
  fParentKey=aItem.getLocalID();
  fWriting=false;
  fInserting=false;
  fDeleting=false;
  fAgentP->fScriptContextDatastore=this;
  if (!TScriptContext::executeTest(
    true, // read array if script returns nothing or no script present
    fScriptContextP, // context
    fmaiP->fInitScript, // the script
    fConfigP->getDSFuncTableP(),fAgentP, // funcdefs/context
    &aItem,true // target item, writeable
  )) {
    // prevented array reading by script returning false
    return; // do nothing for this array
  }
  #else
  fArrIdx=0; // start at array index=0
  #endif // SCRIPT_SUPPORT
  // process SQL statement(s)
  i=0;
  while (getNextSQLStatement(fmaiP->fSelectArraySQL,i,sql,setno)) {
    // Apply substitutions
    resetSQLParameterMaps();
    DoDataSubstitutions(sql,fmaiP->fArrayFieldMapList,setno,false,false,&aItem,fArrIdx*fmaiP->fRepeatInc);
    prepareSQLStatement(aStatement, sql.c_str(),true,"reading array");
    // - prepare parameters
    bindSQLParameters(aStatement,true);
    // Execute
    try {
      execSQLStatement(aStatement,sql,true,NULL,true);
    }
    catch (exception &e) {
      PDEBUGPRINTFX(DBG_DATA+DBG_DBAPI,(
        "readarray:execSQLStatement sql='%s' failed: %s",
        sql.c_str(),
        e.what()
      ));
      throw;
    }
    // get out params
    saveAndCleanupSQLParameters(aStatement,true);
    // Fetch
    // - fetch result(s) now, at most fMaxRepeat
    while (fmaiP->fMaxRepeat==0 || fArrIdx<fmaiP->fMaxRepeat) {
      colindex=1; // ODBC style, fillFieldsFromSQLResult will adjust for SQLite if needed
      if (!fetchNextRow(aStatement,true)) break; // all available rows fetched
      // now fill data into array's mapped fields (array fields or field blocks)
      fillFieldsFromSQLResult(aStatement,colindex,aItem,fmaiP->fArrayFieldMapList,setno,fArrIdx*fmaiP->fRepeatInc);
      #ifdef SCRIPT_SUPPORT
      // process afterread script
      fAgentP->fScriptContextDatastore=this;
      if (!TScriptContext::execute(fScriptContextP,fmaiP->fAfterReadScript,fConfigP->getDSFuncTableP(),fAgentP,&aItem,true))
        throw TSyncException("<afterreadscript> failed");
      #endif
      // increment array index/count
      fArrIdx++;
    }
    // no more records
    finalizeSQLStatement(aStatement,true);
    // save size of array in "sizefrom" field (if it's not an array)
    TItemField *fldP = getMappedBaseFieldOrVar(aItem,fmaiP->fid);
    if (fldP && !fldP->isArray())
      fldP->setAsInteger(fArrIdx);
    // make pass filter if there are no items in the array
    #ifdef OBJECT_FILTERING
    if (fArrIdx==0) aItem.makePassFilter(fmaiP->fNoItemsFilter.c_str()); // item is made pass filter, if possible
    #endif
    #ifdef SCRIPT_SUPPORT
    // process finish script, can perform more elaborated stuff that makePassFilter can
    fAgentP->fScriptContextDatastore=this;
    if (!TScriptContext::execute(fScriptContextP,fmaiP->fFinishScript,fConfigP->getDSFuncTableP(),fAgentP,&aItem,true))
      throw TSyncException("<finishscript> failed");
    #endif
  }
} // TODBCApiDS::readArray


// write or delete an array field
// NOTE: non-array fields are already written when this is called
void TODBCApiDS::writeArray(bool aDelete, bool aInsert, SQLHSTMT aStatement, TMultiFieldItem &aItem, TFieldMapArrayItem *aMapItemP)
{
  string sql;
  sInt16 i;
  uInt16 setno;
  bool done, allempty;

  TODBCFieldMapArrayItem *fmaiP = dynamic_cast<TODBCFieldMapArrayItem *>(aMapItemP);
  PDEBUGPRINTFX(DBG_DATA+DBG_DBAPI+DBG_EXOTIC,("Writing Array, fmaiP=0x%lX",(long)fmaiP));
  if (!fmaiP) return; // do nothing
  PDEBUGPRINTFX(DBG_DATA+DBG_DBAPI+DBG_EXOTIC,("Writing Array"));
  // Check initscript first. If it returns false, we do not insert anything
  bool doit;
  #ifdef SCRIPT_SUPPORT
  // process init script
  fArrIdx=0; // start at array index=0
  fParentKey=aItem.getLocalID();
  fWriting=true;
  fInserting=aInsert;
  fDeleting=aDelete;
  fAgentP->fScriptContextDatastore=this;
  doit = TScriptContext::executeTest(
    true, // write array if script returns nothing or no script present
    fScriptContextP, // context
    fmaiP->fInitScript, // the script
    &ODBCDSFuncTable1,fAgentP, // funcdefs/context
    &aItem,true // target item, writeable
  );
  #else
  doit=true; // no script, always do it
  fArrIdx=0; // start at array index=0
  #endif // SCRIPT_SUPPORT
  PDEBUGPRINTFX(DBG_DATA+DBG_DBAPI+DBG_EXOTIC,("Writing Array: doit=%d, aDelete=%d, aInsert=%d",(int) doit,(int) aDelete,(int) aInsert));
  // check if we need delete first
  if (doit && aDelete) {
    i=0;
    while (getNextSQLStatement(fmaiP->fDeleteArraySQL,i,sql,setno)) {
      // apply data substitutions
      resetSQLParameterMaps();
      DoDataSubstitutions(sql,fmaiP->fArrayFieldMapList,setno,true,false,&aItem,0);
      // - prepare parameters
      prepareSQLStatement(aStatement, sql.c_str(),true,"array erase");
      bindSQLParameters(aStatement,true);
      // issue
      execSQLStatement(aStatement,sql,true,NULL,true);
      // get out params
      saveAndCleanupSQLParameters(aStatement,true);
      // done
      finalizeSQLStatement(aStatement,true);
    }
  }
  // check if we need to insert the array elements
  doit=doit && aInsert;
  #ifdef OBJECT_FILTERING
  if (doit) {
    // test filter
    doit =
      fmaiP->fNoItemsFilter.empty() || // no filter, always generate
      !aItem.testFilter(fmaiP->fNoItemsFilter.c_str()); // if item passes filter, array should not be written
  }
  #endif
  if (doit) {
    // do array write
    sInt32 numElements=0; // unlimited to begin with
    // - see if we have a sizefrom field
    TItemField *fldP = getMappedBaseFieldOrVar(aItem,fmaiP->fid);
    if (fldP) {
      if (fldP->isArray())
        numElements=fldP->arraySize(); // number of detail records to write is size of this array
      else
        numElements=fldP->getAsInteger(); // number of detail records to write are specified in integer variable
    }
    else {
      // entire array (=up to INSERT statement that would contain only empty fields)
      // Note: we cannot use the size of a single array field, as there might be
      //       more than one with different sizes. End of array is when
      //       DoDataSubstitutions() returns allempty==true
      numElements=REP_ARRAY; // this is a big number
    }
    // - limit to maxrepeat
    if (fmaiP->fMaxRepeat!=0 && numElements>fmaiP->fMaxRepeat)
      numElements=fmaiP->fMaxRepeat;
    // - now write
    #ifdef SCRIPT_SUPPORT
    fDeleting=false; // no longer deleting records
    #endif
    while (fArrIdx<numElements) {
      #ifdef SCRIPT_SUPPORT
      // process beforewrite script, end of array if it returns false
      fAgentP->fScriptContextDatastore=this;
      if (!TScriptContext::executeTest(true,fScriptContextP,fmaiP->fBeforeWriteScript,fConfigP->getDSFuncTableP(),fAgentP,&aItem,true))
        goto endarray;
      #endif
      // perform insert statement(s)
      i=0;
      bool firststatement=true;
      while (getNextSQLStatement(fmaiP->fInsertElementSQL,i,sql,setno)) {
        // do substitutions and find out if empty
        resetSQLParameterMaps();
        DoDataSubstitutions(sql,fmaiP->fArrayFieldMapList,setno,true,false,&aItem,fArrIdx*fmaiP->fRepeatInc,&allempty,&done);
        // stop here if FIRST STATEMENT in (unlimited) array insert is all empty
        if (firststatement && fmaiP->fMaxRepeat==0 && allempty)
          goto endarray;
        // store if not empty or empty storage allowed
        if (fmaiP->fStoreEmpty || !allempty) {
          // - prepare parameters
          prepareSQLStatement(aStatement, sql.c_str(),true,"array element insert");
          bindSQLParameters(aStatement,true);
          // issue
          execSQLStatement(aStatement,sql,false,NULL,true);
          // get out params
          saveAndCleanupSQLParameters(aStatement,true);
          // no more records
          finalizeSQLStatement(aStatement,true);
        }
        firststatement=false;
      }
      #ifdef SCRIPT_SUPPORT
      // process afterwrite script, end of array if it returns false
      fAgentP->fScriptContextDatastore=this;
      if (!TScriptContext::executeTest(true,fScriptContextP,fmaiP->fAfterWriteScript,fConfigP->getDSFuncTableP(),fAgentP,&aItem,true))
        goto endarray;
      #endif
      // next
      fArrIdx++;
    } // while
  endarray:
    #ifdef SCRIPT_SUPPORT
    // process finish script
    fAgentP->fScriptContextDatastore=this;
    if (!TScriptContext::execute(fScriptContextP,fmaiP->fFinishScript,fConfigP->getDSFuncTableP(),fAgentP,&aItem,true))
      throw TSyncException("<finishscript> failed");
    #endif
    ;
  }
} // TODBCApiDS::writeArray


// read array fields.
// - if aStatement is passed NULL, routine will allocate/destroy its
//   own statement for reading the array fields
void TODBCApiDS::readArrayFields(SQLHSTMT aStatement, TMultiFieldItem &aItem, TFieldMapList &fml)
{
  TFieldMapList::iterator pos;

  // get data for all array table fields
  for (pos=fml.begin(); pos!=fml.end(); pos++) {
    if ((*pos)->isArray()) {
      readArray(aStatement,aItem,(TFieldMapArrayItem *)*pos);
    }
  }
} // TODBCApiDS::readArrayFields


void TODBCApiDS::writeArrayFields(SQLHSTMT aStatement, TMultiFieldItem &aItem, TFieldMapList &fml, bool aInsert)
{
  TFieldMapList::iterator pos;

  // write all array table fields
  for (pos=fml.begin(); pos!=fml.end(); pos++) {
    if ((*pos)->isArray()) {
      // delete first, then insert
      writeArray(!aInsert || static_cast<TODBCFieldMapArrayItem *>(*pos)->fAlwaysCleanArray,true,aStatement,aItem,(TFieldMapArrayItem *)*pos);
    }
  }
} // TODBCApiDS::writeArrayFields


void TODBCApiDS::deleteArrayFields(SQLHSTMT aStatement, TMultiFieldItem &aItem, TFieldMapList &fml)
{
  TFieldMapList::iterator pos;

  // delete all array table fields
  for (pos=fml.begin(); pos!=fml.end(); pos++) {
    if ((*pos)->isArray()) {
      // only delete, no insert
      writeArray(true,false,aStatement,aItem,(TFieldMapArrayItem *)*pos);
    }
  }
} // TODBCApiDS::deleteArrayFields

#endif // ARRAYDBTABLES_SUPPORT



// append field value(s) as single literal to SQL text
// - returns true if field(s) were not empty
// - even non-existing or empty field will append at least NULL or '' to SQL
bool TODBCApiDS::appendFieldsLiteral(TMultiFieldItem &aItem, sInt16 aFid, sInt16 aRepOffset,TODBCFieldMapItem &aFieldMapping, string &aSQL)
{
  string val;
  bool notempty=false;

  TItemField *fieldP;

  // get mapped item field or local script variable
  fieldP=getMappedFieldOrVar(aItem,aFid,aRepOffset,true); // existing (array fields) only
  // now process single field
  if (!fieldP) goto novalue; // no data -> empty
  notempty=appendFieldValueLiteral(*fieldP, aFieldMapping.dbfieldtype, aFieldMapping.maxsize, aFieldMapping.floating_ts, aSQL);
  return notempty;
novalue:
  // no value was produced
  aSQL+="NULL";
  return false;
} // TODBCApiDS::appendFieldsLiteral


// append field value as literal to SQL text
// - returns true if field(s) were not empty
// - even non-existing or empty field will append at least NULL or '' to SQL
bool TODBCApiDS::appendFieldValueLiteral(TItemField &aField,TDBFieldType aDBFieldType, uInt32 aMaxSize, bool aIsFloating, string &aSQL)
{
  return
    fAgentP->appendFieldValueLiteral(
      aField, aDBFieldType, aMaxSize, aSQL,
      fConfigP->sqlPrepCharSet(),
      fConfigP->fDataLineEndMode,
      fConfigP->fQuotingMode,
      aIsFloating ? TCTX_UNKNOWN : fConfigP->fDataTimeZone,
      fRecordSize
    );
} // TODBCApiDS::appendFieldValueLiteral


// issue single (or multiple) data update statements
// NOTE: returns false if no rows affected. If no information about rows affected
//       is found, returns false if all statements return SQL_NODATA.
//       If config fIgnoreAffectedCount is set, always returns true (unless we have an error).
bool TODBCApiDS::IssueDataWriteSQL(
  SQLHSTMT aStatement,
  const string &aSQL,
  const char *aComment,
  bool aForUpdate,
  TFieldMapList &aFieldMapList,   // field map list for %N,%V and %v
  TMultiFieldItem *aItemP         // item to read values and localid from
)
{
  uInt16 setno=0; // default to 0
  string sql;
  bool d,hasdata=false;
  SQLLEN affectedRows,totalAffected=-1;

  sInt16 i=0;

  // add field values
  fRecordSize=0; // reset count, appendFieldValueLiteral will update size
  while (getNextSQLStatement(aSQL,i,sql,setno)) {
    // - do substitutions
    resetSQLParameterMaps();
    PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("SQL before substitutions: %s",aSQL.c_str()));
    DoDataSubstitutions(
      sql,  // string to apply substitutions to
      aFieldMapList,  // field map list for %N,%V and %v
      setno, // set number
      true, // for write
      aForUpdate, // for update?
      aItemP // item to read values and localid from
    );
    // - prepare parameters
    prepareSQLStatement(aStatement, sql.c_str(),true,aComment);
    bindSQLParameters(aStatement,true);
    // - execute it
    d = execSQLStatement(aStatement,sql,true,NULL,true);
    // - get number of affected rows
    #ifdef SQLITE_SUPPORT
    if (fUseSQLite) {
      affectedRows = sqlite3_changes(fSQLiteP);
    }
    else
    #endif
    {
      #ifdef ODBCAPI_SUPPORT
      SQLRETURN res = SQLRowCount(aStatement,&affectedRows);
      if (res!=SQL_SUCCESS && res!=SQL_SUCCESS_WITH_INFO)
        affectedRows = -1; // unknown
      #else
      affectedRows=0; // no API, no rows affected
      #endif
    }
    // - count affected rows
    if (totalAffected<0)
      totalAffected = affectedRows;
    else if (affectedRows>0)
      totalAffected += affectedRows;
    PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("IssueDataWriteSQL: Statement reports %ld affected rows - totalAffected=%ld",affectedRows,totalAffected));
    // get out params
    saveAndCleanupSQLParameters(aStatement,true);
    // done with statement
    finalizeSQLStatement(aStatement,true);
    hasdata = hasdata || d;
  }
  if (fConfigP->fIgnoreAffectedCount)
    return true; // assume always successful
  else if (totalAffected<0)
    return hasdata; // no reliable info about affected rows - use hasdata instead
  else
    return totalAffected>0; // assume modified something if any row was affected
} // TODBCApiDS::IssueDataWriteSQL


#ifdef HAS_SQL_ADMIN

// issue single (or multiple) map access statements
// NOTE: if all return SQL_NODATA, function will return false.
bool TODBCApiDS::IssueMapSQL(
  SQLHSTMT aStatement,
  const string &aSQL,
  const char *aComment,
  TMapEntryType aEntryType,
  const char *aLocalID,
  const char *aRemoteID,
  uInt32 aMapFlags
)
{
  uInt16 setno=0; // default to 0
  string sql;
  bool d,hasdata=false;

  sInt16 i=0;
  while (getNextSQLStatement(aSQL,i,sql,setno)) {
    // - do substitutions
    DoMapSubstitutions(sql,aEntryType,aLocalID,aRemoteID,aMapFlags);
    // - execute it
    d = execSQLStatement(aStatement,sql,true,aComment,false);
    hasdata = hasdata || d;
  }
  return hasdata;
} // TODBCApiDS::IssueMapSQL

#endif // HAS_SQL_ADMIN


// execute SQL statement as-is, without any substitutions
bool TODBCApiDS::execSQLStatement(SQLHSTMT aStatement, string &aSQL, bool aNoDataAllowed, const char *aComment, bool aForData)
{
  #ifdef ODBCAPI_SUPPORT
  SQLRETURN res;
  #endif

  // show what statement will be executed
  #ifdef SYDEBUG
  if (aComment && PDEBUGTEST(DBG_DBAPI)) {
    PDEBUGPRINTFX(DBG_DBAPI,("SQL for %s:",aComment));
    PDEBUGPUTSX(DBG_DBAPI,aSQL.c_str());
  }
  #endif
  // avoid executing empty statement
  if (aSQL.empty()) return true; // "ok", nothing to execute
  #ifdef SQLITE_SUPPORT
  if (fUseSQLite && aForData) {
    // execute (possibly already prepared) statement in SQLite
    if (!fSQLiteStmtP) {
      // not yet prepared, do it now
      fAgentP->prepareSQLiteStatement(aSQL.c_str(),fSQLiteP,fSQLiteStmtP);
    }
    // do first step
    fStepRc = sqlite3_step(fSQLiteStmtP);
    // clean up right now if we don't have data
    if (fStepRc!=SQLITE_ROW) {
      fStepRc=sqlite3_finalize(fSQLiteStmtP);
      fSQLiteStmtP=NULL;
      fAgentP->checkSQLiteError(fStepRc,fSQLiteP);
    }
    // check if we MUST have data here
    if (!aNoDataAllowed) {
      if (fStepRc!=SQLITE_ROW) {
        // we're not happy because we have no data
        return false;
      }
    }
    return true; // ok
  }
  #endif
  {
    #ifdef ODBCAPI_SUPPORT
    // execute statement
    TP_DEFIDX(li);
    TP_SWITCH(li,fSessionP->fTPInfo,TP_database);
    #ifdef ODBC_UNICODE
    if (fConfigP->fDataCharSet==chs_utf16) {
      // aSQL is UTF-8 here
      // make UCS2 string to pass to wide char API
      string wSQL;
      appendUTF8ToUTF16ByteString(aSQL.c_str(), wSQL, ODBC_BIGENDIAN);
      wSQL += (char)0; // together with the implicit 8-bit NUL terminator, this makes a 16-bit NUL terminator
      // execute it with the "W" version
      res = SafeSQLExecDirectW(
        aStatement,
        (SQLWCHAR *)wSQL.c_str(),
        wSQL.size()/2 // actual number of Unicode chars
      );
    }
    else
    #endif // ODBC_UNICODE
    {
      res = SafeSQLExecDirect(
        aStatement,
        (SQLCHAR *)aSQL.c_str(),
        aSQL.size()
      );
    }
    TP_START(fSessionP->fTPInfo,li);
    // check
    if (aNoDataAllowed)
      return fAgentP->checkStatementHasData(res,aStatement);
    else {
      fAgentP->checkStatementError(res,aStatement);
      return true;
    }
    #endif // ODBCAPI_SUPPORT
  }
  // should never happen
  return false;
} // TODBCApiDS::execSQLStatement


// get next statment from statement list
// - may begin with %GO(setno)
bool TODBCApiDS::getNextSQLStatement(const string &aSQL, sInt16 &aStartAt, string &aOneSQL, uInt16 &aSetNo)
{
  aSetNo=0; // default to set 0
  string::size_type e;
  bool foundone=false;
  sInt32 n=aSQL.size();

  // check for %GO
  while (aStartAt<n) {
    // avoid "compare" here as it is not consistent in Linux g++ bastring.h and MSL
    if (strnncmp(aSQL.c_str()+aStartAt,"%GO",3)==0) {
      aStartAt+=3;
      // check for statement number spec
      const char *p = aSQL.c_str()+aStartAt;
      const char *q=p;
      if (*p=='(') {
        p++;
        // get set number
        if (sscanf(p,"%hd",&aSetNo)!=1) aSetNo=0;
        // search closing paranthesis
        while (*p) { if (*p++==')') break; }
      }
      // skip set no specs
      aStartAt+=p-q;
    }
    // - skip spaces
    while (aStartAt<sInt32(aSQL.size())) {
      if (!isspace(aSQL[aStartAt])) break;
      aStartAt++;
    }
    // aStart now points to beginning of next statement
    // - determine end of next statement
    e=aSQL.find("%GO",aStartAt);
    if (e==string::npos) {
      // last statement
      e=aSQL.size(); // set to end of string
    }
    if (e>string::size_type(aStartAt)) {
      // Not-empty statement
      aOneSQL.assign(aSQL,aStartAt,e-aStartAt);
      // Next statement starts here
      aStartAt=e;
      // statement found
      foundone=true;
      break;
    }
  } // while more in input and empty statement
  return foundone;
} // TODBCApiDS::getNextSQLStatement


// inform logic of coming state change
localstatus TODBCApiDS::dsBeforeStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState)
{
  // let inherited do its stuff as well
  return inherited::dsBeforeStateChange(aOldState,aNewState);
} // TODBCApiDS::dsBeforeStateChange



// inform logic of happened state change
localstatus TODBCApiDS::dsAfterStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState)
{
  // let inherited do its stuff as well
  return inherited::dsAfterStateChange(aOldState,aNewState);
} // TODBCApiDS::dsAfterStateChange


#ifdef ODBCAPI_SUPPORT

// Routine mapping
#define lineartimeToLiteralAppend lineartimeToODBCLiteralAppend

#endif // ODBCAPI_SUPPORT


#ifdef HAS_SQL_ADMIN

// log datastore sync result
// - Called at end of sync with this datastore
void TODBCApiDS::dsLogSyncResult(void)
{
  uInt16 setno=0; // default to 0
  string sql;
  sInt16 i=0;

  // if we have a SQL statement and logging of this session is enabled, log
  if (fSessionP->logEnabled() && !fAgentConfigP->fWriteLogSQL.empty()) {
    // execute SQL statement for logging
    try {
      SQLHSTMT statement=fAgentP->newStatementHandle(getODBCConnectionHandle());
      try {
        while (getNextSQLStatement(fAgentConfigP->fWriteLogSQL,i,sql,setno)) {
          // - do substitutions
          DoLogSubstitutions(sql,false);
          execSQLStatement(statement,sql,true,"Log Entry Write",false);
          finalizeSQLStatement(statement,false);
        }
        // release the statement handle
        SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
        // commit the transaction (this is called AFTER the data transactions probably have been
        // rolled back in EndDataWrite
        SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_COMMIT);
      }
      catch (exception &e) {
        // release the statement handle
        SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
        throw;
      }
    }
    catch (exception &e) {
      // release the statement handle
      PDEBUGPRINTFX(DBG_ERROR,("Failed to issue Log SQL statement: %s",e.what()));
    }
  }
  // let ancestor process
  TStdLogicDS::dsLogSyncResult();
} // TODBCApiDS::dsLogSyncResult


// Do logfile SQL/plaintext substitutions
void TODBCApiDS::DoLogSubstitutions(string &aLog,bool aPlaintext)
{
  string s;
  size_t i;

  // logging
  if (!aPlaintext) {
    // only for SQL text
    // Note: we need to do free form strings here to ensure correct DB string escaping
    // %rD  Datastore remote path
    StringSubst(aLog,"%rD",getRemoteDBPath(),3,chs_ascii,lem_cstr,fConfigP->fQuotingMode);
    // %nR  Remote name: [Manufacturer ]Model")
    StringSubst(aLog,"%nR",fSessionP->getRemoteDescName(),3,chs_ascii,lem_cstr,fConfigP->fQuotingMode);
    // %vR  Remote Device Version Info ("Type (HWV, FWV, SWV) Oem")
    StringSubst(aLog,"%vR",fSessionP->getRemoteInfoString(),3,chs_ascii,lem_cstr,fConfigP->fQuotingMode);
    // %U User Name
    StringSubst(aLog,"%U",fSessionP->getSyncUserName(),2,chs_ascii,lem_cstr,fConfigP->fQuotingMode);
    // %lD  Datastore local path (complete with all CGI)
    StringSubst(aLog,"%lD",getRemoteViewOfLocalURI(),3,chs_ascii,lem_cstr,fConfigP->fQuotingMode);
    // %iR  Remote Device ID (URI)
    StringSubst(aLog,"%iR",fSessionP->getRemoteURI(),3,chs_ascii,lem_cstr,fConfigP->fQuotingMode);
    // %dT  Sync date
    i=0; while((i=aLog.find("%dT",i))!=string::npos) {
      s.erase();
      fAgentP->lineartimeToLiteralAppend(fCurrentSyncTime, s, true, false, TCTX_UTC, fConfigP->fDataTimeZone);
      aLog.replace(i,3,s); i+=s.size();
    }
    // %tT  Sync time
    i=0; while((i=aLog.find("%tT",i))!=string::npos) {
      s.erase();
      fAgentP->lineartimeToLiteralAppend(fCurrentSyncTime, s, false, true, TCTX_UTC, fConfigP->fDataTimeZone);
      aLog.replace(i,3,s); i+=s.size();
    }
    // %T Sync timestamp
    i=0; while((i=aLog.find("%T",i))!=string::npos) {
      s.erase();
      fAgentP->lineartimeToLiteralAppend(fCurrentSyncTime, s, true, true, TCTX_UTC, fConfigP->fDataTimeZone);
      aLog.replace(i,2,s); i+=s.size();
    }
    // %ssT Sync start time timestamp
    i=0; while((i=aLog.find("%ssT",i))!=string::npos) {
      s.erase();
      fAgentP->lineartimeToLiteralAppend(fSessionP->getSessionStarted(), s, true, true, TCTX_UTC, fConfigP->fDataTimeZone);
      aLog.replace(i,4,s); i+=s.size();
    }
    // %seT Sync end time timestamp
    i=0; while((i=aLog.find("%seT",i))!=string::npos) {
      s.erase();
      fAgentP->lineartimeToLiteralAppend(getEndOfSyncTime(), s, true, true, TCTX_UTC, fConfigP->fDataTimeZone);
      aLog.replace(i,4,s); i+=s.size();
    }
  }
  // - let ancestor process its own substitutions
  TStdLogicDS::DoLogSubstitutions(aLog,aPlaintext);
  // %t Target Key
  // %f Folder Key
  // %u User key
  // %d device key
  // - let standard SQL substitution handle this
  DoSQLSubstitutions(aLog);
} // TODBCApiDS::DoLogSubstitutions


// do substitutions for map table access
void TODBCApiDS::DoMapSubstitutions(
  string &aSQL,                   // string to apply substitutions to
  TMapEntryType aEntryType,       // the entry type
  const char *aLocalID,           // local ID
  const char *aRemoteID,          // remote ID
  uInt32 aMapFlags                // map flags
)
{
  // can be empty, but we don't do NULLs
  if (!aLocalID) aLocalID="";
  if (!aRemoteID) aRemoteID="";
  // %k = data key (local ID)
  StringSubst(aSQL,"%k",aLocalID,2,-1,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
  // %r = remote ID (always a string)
  StringSubst(aSQL,"%r",aRemoteID,2,-1,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
  // %x = flags (a uInt32)
  StringSubst(aSQL,"%x",(sInt32)aMapFlags,2);
  // %e = entry type (a small number, uInt8 is enough)
  StringSubst(aSQL,"%e",(sInt32)aEntryType,2);
  // now substitute standard: %f=folderkey, %u=userkey, %t=targetkey
  DoSQLSubstitutions(aSQL);
} // TODBCApiDS::DoMapSubstitutions


#endif // HAS_SQL_ADMIN


// Do the SQL substitutions common for all SQL statements
void TODBCApiDS::DoSQLSubstitutions(string &aSQL)
{
  string::size_type i;

  // let session substitute session level stuff first
  fAgentP->DoSQLSubstitutions(aSQL);
  StringSubst(aSQL,"%f",fFolderKey,2,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
  StringSubst(aSQL,"%t",fTargetKey,2,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
  // ID generator
  // - new ID
  i=0; while((i=aSQL.find("%X",i))!=string::npos) {
    // - generate new one
    nextLocalID(fConfigP->fSpecialIDMode,SQL_NULL_HANDLE);
    // - use it
    aSQL.replace(i,2,fLastGeneratedLocalID);
    i+=fLastGeneratedLocalID.size();
  }
  // last used ID
  StringSubst(aSQL,"%x",fLastGeneratedLocalID,2,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
} // TODBCApiDS::DoSQLSubstitutions




typedef struct {
  const char *substTag;
  int substCode;
} TSubstHandlerDef;

typedef enum {
  dsh_datakey,
  dsh_datakey_outparam_string,
  dsh_datakey_outparam_integer,
  dsh_fieldnamelist,
  dsh_fieldnamelist_a,
  dsh_namevaluelist,
  dsh_namevaluelist_a,
  dsh_valuelist,
  dsh_valuelist_a,
  dsh_param,
  dsh_field,
  dsh_recordsize,
  dsh_moddate,
  dsh_modtime,
  dsh_moddatetime,
  dsh_andfilter,
  dsh_wherefilter,
  // number of enums
  dsh_NUMENTRIES
} TDataSubstHandlers;

static TSubstHandlerDef DataSubstHandlers[dsh_NUMENTRIES] = {
  { "k", dsh_datakey },
  { "pkos", dsh_datakey_outparam_string },
  { "pkoi", dsh_datakey_outparam_integer },
  { "N", dsh_fieldnamelist },
  { "aN", dsh_fieldnamelist_a },
  { "V", dsh_namevaluelist },
  { "aV", dsh_namevaluelist_a },
  { "v", dsh_valuelist },
  { "av", dsh_valuelist_a },
  { "p(", dsh_param },
  { "d(", dsh_field },
  { "S", dsh_recordsize },
  { "dM", dsh_moddate },
  { "tM", dsh_modtime },
  { "M", dsh_moddatetime },
  { "AF", dsh_andfilter },
  { "WF", dsh_wherefilter }
};


// do substitutions for data table access
void TODBCApiDS::DoDataSubstitutions(
  string &aSQL,                   // string to apply substitutions to
  TFieldMapList &aFieldMapList,   // field map list for %N,%V and %v
  uInt16 aSetNo,                  // Map Set number
  bool aForWrite,                 // set if read-enabled or write-enabled fields are shown in %N,%V and %v
  bool aForUpdate,                // set if for update (only assigned fields will be shown if !fUpdateAllFields)
  TMultiFieldItem *aItemP,        // item to read values and localid from
  sInt16 aRepOffset,              // array index/repeat offset
  bool *aAllEmptyP,               // true if all %V or %v empty
  bool *aDoneP                    // true if no data at specified aRepOffset
)
{
  string::size_type i,j,k,m,m2,n;
  string s,s2;
  string inStr;
  int handlerid;

  // default settings of flags
  bool done=true; // default, in case %V or %v is missing (which could limit repetitions for arrays)
  bool allempty=false; // not all empty unless we see that %V has all empty fields

  // substitute one by one in the order the escape sequences appear in the SQL string
  // (this is important for correct parameter mapping!!)
  i=0;
  inStr=aSQL;
  aSQL.erase(); // will be rebuilt later
  while((i=inStr.find("%",i))!=string::npos) {
    // potential escape sequence found
    // - search in table
    for (handlerid=0; handlerid<dsh_NUMENTRIES; handlerid++) {
      // - get compare length
      n = strlen(DataSubstHandlers[handlerid].substTag);
      if (strnncmp(inStr.c_str()+i+1,DataSubstHandlers[handlerid].substTag,n)==0) {
        // found handler
        // - i = index of % lead-in
        n+=1;
        // - n = size of matched sequence
        s.erase();
        // - s = replacement string. If set to non-empty, n chars at i will be replaced by it
        // Now process
        bool upc=false,lowc=false,asci=false;
        TDBFieldType dbfty=dbft_numeric;
        sInt16 ty,idx;
        sInt32 sz=0;
        sInt16 arrindex=0;
        TItemField *fldP;
        int substCode = DataSubstHandlers[handlerid].substCode;
        //DEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("- found %%%s (%ld chars)",DataSubstHandlers[substCode].substTag,(long)n));
        switch (substCode) {
          case dsh_datakey:
            if (aItemP) s=aItemP->getLocalID();
            break;
          case dsh_datakey_outparam_string:
            // generated key output (string key)
            addSQLParameterMap(false,true,param_localid_str,NULL,aItemP,aRepOffset);
            goto paramsubst;
          case dsh_datakey_outparam_integer:
            // generated key output (integer key)
            addSQLParameterMap(false,true,param_localid_int,NULL,aItemP,aRepOffset);
            goto paramsubst;
          case dsh_fieldnamelist:
          case dsh_fieldnamelist_a:
            //  %N and %aN = field name list
            addFieldNameList(s,aForWrite,aForUpdate,substCode!=dsh_fieldnamelist_a && !fConfigP->fUpdateAllFields && aForUpdate && aItemP,aItemP,aFieldMapList,aSetNo);
            break;
          case dsh_namevaluelist:
          case dsh_namevaluelist_a:
            if (!aItemP) break; // needs an item
            //  %V,%aV = data field=value list
            addFieldNameValueList(s,substCode!=dsh_namevaluelist_a && !fConfigP->fUpdateAllFields && aForUpdate,done,allempty,*aItemP,true,aFieldMapList,aSetNo,aRepOffset,allempty);
            break;
          case dsh_valuelist:
          case dsh_valuelist_a:
            if (!aItemP) break; // needs an item
            //  %v,%av = data value list
            addFieldNameValueList(s,substCode!=dsh_valuelist_a && !fConfigP->fUpdateAllFields && aForUpdate,done,allempty,*aItemP,false,aFieldMapList,aSetNo,aRepOffset,allempty);
            break;
          case dsh_param:
            // handle with global routine used in different substitution places
            // i=start of % sequence, n=size of matched basic sequence = %+identifier+( = %p(
            if (!fAgentP->ParseParamSubst(
              inStr,i,n,
              fParameterMaps,
              aItemP
              #ifdef SCRIPT_SUPPORT
              ,fScriptContextP
              #endif
            )) break;
            // i=start of % sequence, n=size of entire sequence including params and closing paranthesis
          paramsubst:
            s="?";
            break;
          case dsh_field:
            // init options
            upc=false;
            lowc=false;
            asci=false;
            // skip lead-in
            j=i+n;
            // find closing paranthesis
            k = inStr.find(")",j);
            if (k==string::npos) { i=j; n=0; break; } // no closing paranthesis
            m = k; // assume end of name is here
            m2 = k;
            // look for dbfieldtype
            dbfty=dbft_numeric; // default to numeric (no quotes, to be compatible with old %d() definition)
            m2 = inStr.find(",",j);
            if (m2!=string::npos) {
              // get dbfieldtype
              if (StrToEnum(DBFieldTypeNames,numDBfieldTypes,ty,inStr.c_str()+m2+1,k-m2-1))
                dbfty=(TDBFieldType)ty;
            }
            else
              m2=k; // set end of name to closing paranthesis again
            #ifdef ARRAYFIELD_SUPPORT
            // also look for array index
            m = inStr.find("#",j);
            if (m!=string::npos && m<m2) {
              // get array index
              StrToShort(inStr.c_str()+m+1,arrindex);
            }
            else
              m=m2; // set end of name to closing paranthesis or comma again
            #endif
            // extract options, if any
            if (inStr[j]=='[') {
              do {
                j++;
                if (inStr.size()<=j) break;
                if (inStr[j]==']') { j++; break; }; // end of options
                // check options
                if (inStr[j]=='u') upc=true;
                else if (inStr[j]=='l') lowc=true;
                else if (inStr[j]=='a') asci=true;
              } while(true);
            }
            // extract name (without possible array index or dbfieldtype)
            s.assign(inStr,j,m-j);
            // find field
            if (!aItemP) { i=k+1; n=0; break; } // no item, no action
            #ifdef SCRIPT_SUPPORT
            if (fScriptContextP) {
              idx=fScriptContextP->getIdentifierIndex(OBJ_AUTO, aItemP->getFieldDefinitions(),s.c_str());
              fldP=fScriptContextP->getFieldOrVar(aItemP,idx,arrindex);
            }
            else
            #endif
              fldP = aItemP->getArrayField(s.c_str(),arrindex,true);
            if (!fldP) { i=k+1; n=0; break; } // field not found, no action
            // produce DB literal
            sz=0;
            s.erase();
            fAgentP->appendFieldValueLiteral(
              *fldP,
              dbfty,
              0, // unlimited
              s,
              asci ? chs_ascii : fConfigP->sqlPrepCharSet(),
              fConfigP->fDataLineEndMode,
              fConfigP->fQuotingMode,
              fConfigP->fDataTimeZone,
              sz
            );
            // apply options
            if (upc) StringUpper(s);
            else if (lowc) StringLower(s);
            fRecordSize+=s.size();
            // set substitution parameters
            n=k+1-i;
            break;
          case dsh_recordsize:
            if (aItemP) StringObjPrintf(s,"%ld",(long)fRecordSize);
            break;
          #ifdef ODBCAPI_SUPPORT
          case dsh_moddate:
            //  %dM = modified date
            fAgentP->lineartimeToODBCLiteralAppend(fCurrentSyncTime,s,true,false,TCTX_UTC,fConfigP->fDataTimeZone);
            break;
          case dsh_modtime:
            //  %tM = modified time
            fAgentP->lineartimeToODBCLiteralAppend(fCurrentSyncTime,s,false,true,TCTX_UTC,fConfigP->fDataTimeZone);
            break;
          #endif // ODBCAPI_SUPPORT
          case dsh_moddatetime:
            //  %M = modified datetimestamp
            #ifdef SQLITE_SUPPORT
            if (fUseSQLite) {
              // integer timestamp modes
              fAgentP->lineartimeToIntLiteralAppend(fCurrentSyncTime,s,fConfigP->fLastModDBFieldType,TCTX_UTC,fConfigP->fDataTimeZone);
            }
            #ifdef ODBCAPI_SUPPORT
            else
            #endif
            #endif
            #ifdef ODBCAPI_SUPPORT
            {
              if (fConfigP->fLastModDBFieldType==dbft_timestamp)
                fAgentP->lineartimeToODBCLiteralAppend(fCurrentSyncTime,s,true,true,TCTX_UTC,fConfigP->fDataTimeZone);
              else
                fAgentP->lineartimeToIntLiteralAppend(fCurrentSyncTime,s,fConfigP->fLastModDBFieldType,TCTX_UTC,fConfigP->fDataTimeZone);
            }
            #endif // ODBCAPI_SUPPORT
            ; // make sure we can't else out the break
            break;
          #ifdef OBJECT_FILTERING
          case dsh_andfilter:
            if (fFilterWorksOnDBLevel && fFilterExpressionTested)
              appendFiltersClause(s,"AND");
            break;
          case dsh_wherefilter:
            if (fFilterWorksOnDBLevel && fFilterExpressionTested)
              appendFiltersClause(s,"WHERE");
            break;
          #endif
          default:
            i+=n; // continue parsing after unhandled tag
            n=0; // no replacement
            break;
        } // switch
        // apply substitution if any (n>0)
        // - i=pos of %, n=size of text to be replaced, s=replacement string
        // Note: if n==0, i must point to where parsing should continue
        if (n>0) {
          // get everything up to this % sequence
          string intermediateStr;
          intermediateStr.assign(inStr,0,i);
          // have non-processed sequences processed for the other substitution possibilities now
          //   %f=folderkey, %u=userkey, %d=devicekey, %t=targetkey, %C=domain
          //   Note: it is important that %t and %d is checked here, after %d(), %tL and %tS above!!
          //DEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("Intermediate before applying basic substitutions: '%s'",intermediateStr.c_str()));
          DoSQLSubstitutions(intermediateStr);
          //DEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("Intermediate after applying basic substitutions: '%s'",intermediateStr.c_str()));
          // add it to the output string
          aSQL+=intermediateStr;
          // add the substitution (will NOT BE PROCESSED AGAIN!!)
          aSQL+=s;
          // have the rest of the inStr processed
          inStr.erase(0,i+n); // remove already processed stuff and escape sequence now
          // reset parsing position in inStr
          i=0;
          //DEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("- substitution with '%s' done, SQL so far: '%s'",s.c_str(),aSQL.c_str()));
          //DEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("Remaining input string to process: '%s'",inStr.c_str()));
        }
        // sequence substitution done
        break;
      }
    } // for
    if (handlerid>=dsh_NUMENTRIES) {
      // unknown sequence
      // - check if this is an explicit literal % (two % in sequence)
      if (inStr[i+1]=='%') {
        // yes, replace %% by % in output
        inStr.replace(i,2,"%");
        i+=1;
      }
      else {
        // no, just leave % in input as it is
        i+=1;
      }
    }
  } // while % found in string
  // process rest of inStr (not containing any of the above sequences, but probably some from
  // parent implementation
  // have non-processed sequences processed for the other substitution possibilities now
  //   %f=folderkey, %u=userkey, %d=devicekey, %t=targetkey, %C=domain
  //   Note: it is important that %t and %d is checked here, after %d(), %tL and %tS above!!
  DoSQLSubstitutions(inStr);
  // Append end of inStr to output
  aSQL+=inStr;
  // return flags
  if (aAllEmptyP) *aAllEmptyP=allempty;
  if (aDoneP) *aDoneP=done;
} // TODBCApiDS::DoDataSubstitutions



// insert additional data selection conditions, if any. Returns true if something appended
bool TODBCApiDS::appendFiltersClause(string &aSQL, const char *linktext)
{
  #ifndef OBJECT_FILTERING
  return false; // no filters
  #else

  if (
    (!fConfigP->fFilterOnDBLevel || (fLocalDBFilter.empty() && fSyncSetFilter.empty() && fConfigP->fInvisibleFilter.empty()))
    #ifdef SCRIPT_SUPPORT
    && fSQLFilter.empty()
    #endif
  )
    return false; // no filter clause needed
  // some conditions, add link text
  if (linktext) { aSQL+=' '; aSQL+=linktext; aSQL+=' '; }
  linktext=NULL; // link within paranthesis
  #ifdef SCRIPT_SUPPORT
  // - Explicit SQL filter
  if (!fSQLFilter.empty()) {
    if (linktext) aSQL+=linktext;
    linktext=" AND ";
    aSQL+='(';
    aSQL+=fSQLFilter;
    aSQL+=')';
  }
  #endif
  // - hardcoded filter conditions
  if (!fLocalDBFilter.empty()) {
    if (linktext) aSQL+=linktext;
    linktext=" AND ";
    aSQL+='(';
    if (!appendFilterConditions(aSQL,fLocalDBFilter))
      throw TSyncException("<localdbfilter> incompatible with SQL database");
    aSQL+=')';
  }
  // - filter invisible items
  if (!fConfigP->fInvisibleFilter.empty()) {
    if (linktext) aSQL+=linktext;
    linktext=" AND ";
    aSQL+="NOT (";
    if (!appendFilterConditions(aSQL,fConfigP->fInvisibleFilter))
      throw TSyncException("<invisiblefilter> incompatible with SQL database");
    aSQL+=')';
  }
  // - sync set filter conditions
  if (!fSyncSetFilter.empty()) {
    if (linktext) aSQL+=linktext;
    linktext=" AND ";
    aSQL+='(';
    if (!appendFilterConditions(aSQL,fSyncSetFilter))
      throw TSyncException("sync set filter incompatible with SQL database");
    aSQL+=')';
  }
  return true; // something appended
  #endif // OBJECT_FILTERING
} // TODBCApiDS::appendFiltersClause



// reset all mapped parameters
void TODBCApiDS::resetSQLParameterMaps(void)
{
  #ifdef SQLITE_SUPPORT
  if (fUseSQLite) {
    // resetting the parameter map finalizes any possibly running statement
    if (fSQLiteStmtP) {
      sqlite3_finalize(fSQLiteStmtP);
      fSQLiteStmtP=NULL;
    }
  }
  #endif
  fAgentP->resetSQLParameterMaps(fParameterMaps);
} // TODBCApiDS::resetSQLParameterMaps


// add parameter definition to the datastore level parameter list
void TODBCApiDS::addSQLParameterMap(
  bool aInParam, bool aOutParam,
  TParamMode aParamMode,
  TODBCFieldMapItem *aFieldMapP,
  TMultiFieldItem *aItemP,
  sInt16 aRepOffset
)
{
  TParameterMap map;

  // assign basics
  map.inparam=aInParam;
  map.outparam=aOutParam;
  map.parammode=aParamMode;
  map.mybuffer=false;
  map.ParameterValuePtr=NULL;
  map.BufferLength=0;
  map.StrLen_or_Ind=SQL_NULL_DATA; // note that this is not zero (but -1)
  map.itemP=aItemP;
  map.outSiz=NULL;
  // get things from field map
  if (aFieldMapP) {
    map.fieldP=getMappedFieldOrVar(*aItemP,aFieldMapP->fid,aRepOffset);
    map.maxSize=aFieldMapP->maxsize;
    map.dbFieldType=aFieldMapP->dbfieldtype;
  }
  else {
    map.fieldP=NULL;
    map.maxSize=0;
    map.dbFieldType=dbft_string;
  }
  // save in list
  fParameterMaps.push_back(map);
} // TODBCApiDS::addSQLParameterMap



// prepare SQL statment as far as needed for parameter binding
void TODBCApiDS::prepareSQLStatement(SQLHSTMT aStatement, cAppCharP aSQL, bool aForData, cAppCharP aComment)
{
  // show what statement will be executed
  #ifdef SYDEBUG
  if (aComment && aSQL && PDEBUGTEST(DBG_DBAPI)) {
    PDEBUGPRINTFX(DBG_DBAPI,("SQL for %s:",aComment));
    PDEBUGPUTSX(DBG_DBAPI,aSQL);
  }
  #endif

  #ifdef SQLITE_SUPPORT
  if (fUseSQLite && aForData) {
    // bind params
    fAgentP->prepareSQLiteStatement(
      aSQL,
      fSQLiteP,
      fSQLiteStmtP
    );
  }
  #endif
} // TODBCApiDS::prepareSQLStatement


// bind parameters (and values for IN-Params) to the statement
void TODBCApiDS::bindSQLParameters(SQLHSTMT aStatement, bool aForData)
{
  #ifdef SQLITE_SUPPORT
  if (fUseSQLite && aForData) {
    // bind params
    fAgentP->bindSQLiteParameters(
      fSessionP,
      fSQLiteStmtP,
      fParameterMaps,
      fConfigP->fDataCharSet,
      fConfigP->fDataLineEndMode
    );
  }
  else
  #endif
  {
    #ifdef ODBCAPI_SUPPORT
    fAgentP->bindSQLParameters(
      fSessionP,
      aStatement,
      fParameterMaps,
      fConfigP->fDataCharSet, // actual charset, including utf16, bindSQLiteParameters handles UTF16 case
      fConfigP->fDataLineEndMode
    );
    #endif
  }
} // TODBCApiDS::bindSQLParameters


// save out parameter values and clean up
void TODBCApiDS::saveAndCleanupSQLParameters(
  SQLHSTMT aStatement,
  bool aForData
)
{
  #ifdef SQLITE_SUPPORT
  if (fUseSQLite && aForData) {
    // NOP here because SQLite has no out params
    return;
  }
  else
  #endif
  {
    #ifdef ODBCAPI_SUPPORT
    fAgentP->saveAndCleanupSQLParameters(
      fSessionP,
      aStatement,
      fParameterMaps,
      fConfigP->fDataCharSet, // actual charset, including utf16, saveAndCleanupSQLParameters handles UTF16 case
      fConfigP->fDataLineEndMode
    );
    #endif
  }
} // TODBCApiDS::bindSQLParameters


// Fetch next row from SQL statement, returns true if there is any
bool TODBCApiDS::fetchNextRow(SQLHSTMT aStatement, bool aForData)
{
  #ifdef SQLITE_SUPPORT
  if (fUseSQLite && aForData) {
    int rc = fStepRc;
    if (rc!=SQLITE_ROW) {
      if (rc==SQLITE_OK) {
        // last step was ok, we need to do next step first
        rc = sqlite3_step(fSQLiteStmtP);
      }
    }
    // clean up right now if we don't have data
    if (rc!=SQLITE_ROW) {
      rc=sqlite3_finalize(fSQLiteStmtP);
      fStepRc = rc;
      fSQLiteStmtP=NULL;
      fAgentP->checkSQLiteError(rc,fSQLiteP);
      return false; // no more data
    }
    else {
      // we are reporting data now, make sure we fetch data in next call
      fStepRc=SQLITE_OK;
    }
    // more data found
    return true; // ok
  }
  else
  #endif
  #ifdef ODBCAPI_SUPPORT
  {
    if (aStatement==SQL_NULL_HANDLE)
      return false; // no statement, nothing fetched
    SQLRETURN res=SafeSQLFetch(aStatement);
    // check for end
    return fAgentP->checkStatementHasData(res,aStatement);
  }
  #else
    return false; // no API, no result
  #endif // ODBCAPI_SUPPORT
} // TODBCApiDS::fetchNextRow





// SQL statement complete, finalize it
void TODBCApiDS::finalizeSQLStatement(SQLHSTMT aStatement, bool aForData)
{
  #ifdef SQLITE_SUPPORT
  if (fUseSQLite && aForData) {
    // finalize if not already done
    if (fSQLiteStmtP) {
      sqlite3_finalize(fSQLiteStmtP);
      fSQLiteStmtP=NULL;
    }
  }
  else
  #endif
  {
    #ifdef ODBCAPI_SUPPORT
    // just close cursor of the statement
    SafeSQLCloseCursor(aStatement);
    #endif // ODBCAPI_SUPPORT
  }
} // TODBCApiDS::prepareSQLStatement



#ifdef OBJECT_FILTERING

// - returns true if DB implementation can filter the standard filters
//   (LocalDBFilter, TargetFilter and InvisibleFilter) during database fetch
//   - otherwise, fetched items will be filtered after being read from DB.
bool TODBCApiDS::dsFilteredFetchesFromDB(bool aFilterChanged)
{
  // can do filtering while fetching from DB, such that items that get (in)visible
  // because of changed filter conditions are correctly detected as added/deleted
  if (aFilterChanged || !fFilterExpressionTested) {
    fFilterExpressionTested=true;
    if (fConfigP->fFilterOnDBLevel) {
      // try to dummy-append the filterclause to check if this will work
      string s;
      try {
        appendFiltersClause(s,"");
        fFilterWorksOnDBLevel=true;
      }
      catch(exception &e) {
        PDEBUGPRINTFX(DBG_FILTER+DBG_HOT,("%s -> filter will be applied to fetched records",e.what()));
        fFilterWorksOnDBLevel=false;
      }
    }
    else
      fFilterWorksOnDBLevel=false; // reject all DB level filtering
  }
  // if we can filter, that's sufficient
  if (fFilterWorksOnDBLevel) return true;
  // otherwise, let ancestor test
  return inherited::dsFilteredFetchesFromDB(aFilterChanged);
} // TODBCApiDS::dsfilteredFetchesFromDB


// - appends logical condition to SQL from filter string
bool TODBCApiDS::appendFilterConditions(string &aSQL, const string &aFilter)
{
  const char *p=aFilter.c_str();
  return appendFilterTerm(aSQL,p,p+aFilter.size());
} // TODBCApiDS::appendFilterConditions


// - appends logical condition term to SQL from filter string
bool TODBCApiDS::appendFilterTerm(string &aSQL, const char *&aPos, const char *aStop)
{
  char c=0;
  const char *st;
  string str,cmp,val;
  bool result;
  sInt16 fid;
  bool specialValue;
  bool caseInsensitive;

  // determine max length
  if (aStop==NULL) aStop=aPos+strlen(aPos);
  // empty expression is ok
  do {
    result=true;
    // process simple term (<ident><op><value>)
    // - get first non-space
    while (aPos<=aStop) {
      c=*aPos;
      if (c!=' ') break;
      aPos++;
    }
    // Term starts here, first char is c, aPos points to it
    // - check subexpression paranthesis
    if (c=='(') {
      // boolean term is grouped subexpression
      aSQL+='(';
      aPos++;
      result=appendFilterTerm(aSQL,aPos,aStop);
      // check if matching paranthesis
      if (*(aPos++)!=')') {
        PDEBUGPRINTFX(DBG_EXOTIC,("Filter expression syntax error at: %s",--aPos));
        aPos=aStop; // skip rest
        return false; // always fail
      }
      aSQL+=')';
    }
    else if (c==0) {
      // empty term, is ok
      return true;
    }
    else {
      // must be simple boolean term
      // - remember start of ident
      st=aPos;
      // - search end of ident
      while (isFilterIdent(c)) c=*(++aPos);
      // - c/aPos=char after ident, get ident
      str.assign(st,aPos-st);
      // - check for subscript index
      uInt16 subsIndex=0; // no index (is 1-based in DS 1.2 filter specs)
      if (c=='[') {
        // expect numeric index
        aPos++; // next
        aPos+=StrToUShort(aPos,subsIndex);
        if (*aPos!=']') {
          PDEBUGPRINTFX(DBG_ERROR,("Filter expression error (missing \"]\") at: %s",--aPos));
          return false; // syntax error, does not pass
        }
        c=*(++aPos); // process next after subscript
      }
      // - get operator
      while (isspace(c)) c=*(++aPos);
      if (c==':') c=*(++aPos); // ignore assign-to-make-true modifier
      specialValue = c=='*'; // special-value modifier
      if (specialValue) c=*(++aPos);
      caseInsensitive = c=='^'; // case insensitive modifier
      if (caseInsensitive) c=*(++aPos);
      aPos++; // one char at least
      cmp.erase();
      bool cmplike=c=='%' || c=='$';
      if (cmplike) cmp= (c=='%') ? " LIKE " : " NOT LIKE "; // "contains" special case, use SQL LIKE %val%
      else {
        cmp+=c; // simply use first char as is
        if (c=='>' || c=='<') {
          if (*aPos=='=' || *aPos=='>') {
            c=*aPos++;
            cmp+=c; // >=, <= and <>
          }
        }
      }
      // - get comparison value
      while (isspace(c)) c=*(++aPos);
      st=aPos; // should start here
      while (aPos<aStop && *aPos!='&' && *aPos!='|' && *aPos!=')') aPos++;
      // - assign value string
      if (cmplike) val='%'; else val.erase();
      val.append(st,aPos-st);
      if (cmplike) val+='%';
      // Now we have str=field identifier, cmp=operator, val=value string
      // - check if directly addressing DB field
      if (strucmp(str.c_str(),"D.",2)==0) {
        // this directly refers to a DB field
        // - translate for special values
        if (specialValue) {
          if (val=="N" || val=="E") {
            val=" NULL";
            caseInsensitive=false; // not needed for NULL
            cmp="IS";
            if (cmp!="=") cmp+=" NOT";
          }
        }
        if (caseInsensitive) {
          aSQL+="LOWER(";
          aSQL.append(str.c_str()+2);
          aSQL+=')';
          aSQL+=cmp; // operator
          aSQL+="LOWER(";
          aSQL+=val;
          aSQL+=')';
        }
        else {
          aSQL.append(str.c_str()+2);
          aSQL+=cmp; // operator
          aSQL+=val; // value must be specified in DB syntax (including quotes for strings)
        }
      }
      else {
        // get field ID for that ident (can be -1 if none found)
        TMultiFieldItemType *mfitP = dynamic_cast<TMultiFieldItemType *>(getLocalSendType());
        if (mfitP)
          fid=mfitP->getFilterIdentifierFieldIndex(str.c_str(),subsIndex);
        else
          return false; // field not known, bad syntax
        if (fid==FID_NOT_SUPPORTED) return false; // unknown field
        // search for map for that field id to obtain DB field name
        TFieldMapList &fml = fConfigP->fFieldMappings.fFieldMapList;
        TFieldMapList::iterator mainpos;
        TFieldMapList::iterator pos;
        bool mainfound=false;
        for (pos=fml.begin(); pos!=fml.end(); pos++) {
          // check field id, but search in setNo==0 only (fields of other sets are
          // not available when SELECTing syncset.
          if ((*pos)->setNo==0 && fid==(*pos)->fid) {
            if ((*pos)->isArray()) return false; // array fields cannot be used in filters
            // found
            if (mainfound) break; // second match just breaks loop (time for date, possibly)
            // main
            mainpos=pos; // save position of main field
            mainfound=true;
            // search for further fields with same fid (possibly needed for timefordate)
          }
        }
        if (!mainfound) {
          PDEBUGPRINTFX(DBG_EXOTIC,("Could not find DB field for fid=%hd",fid));
          return false; // no such field
        }
        // - translate for special values
        if (specialValue) {
          if ((*mainpos)->dbfieldtype==dbft_string && val=="E") {
            // Empty is not NULL for strings
            val="''";
          }
          else if (val=="N" || val=="E") {
            // for other types, NULL and EMPTY are the same
            val="NULL";
            if (cmp!="=") cmp=" IS NOT ";
            else cmp=" IS ";
          }
          // now append (no check for caseInsensitive needed as NULL check does not need it
          aSQL+=(*mainpos)->fElementName;
          aSQL+=cmp;
          aSQL+=val;
        }
        else {
          // create field to convert value string correctly
          TItemField *valfldP = newItemField(
            mfitP->getFieldDefinition(fid)->type,
            getSessionZones()
          );
          // assign value as string
          valfldP->setAsString(val.c_str());
          // check for special case when timestamp is mapped to separate date/time fields
          if (
            pos!=fml.end() && ( // if there is another map with the same fid
              ((*mainpos)->dbfieldtype==dbft_date && (*pos)->dbfieldtype==dbft_timefordate)
            )
          ) {
            // separate date & time
            aSQL+='(';
            if (cmp=="=" || cmp=="<>") {
              // (datefield=val AND timefield=val)
              // (datefield<>val OR timefield<>val)
              aSQL+=(*mainpos)->fElementName;
              aSQL+=cmp;
              appendFieldValueLiteral(*valfldP, (*mainpos)->dbfieldtype,(*mainpos)->maxsize, (*mainpos)->floating_ts, aSQL);
              if (cmp=="=")
                aSQL+=" AND ";
              else
                aSQL+=" OR ";
              // - time field
              aSQL+=(*pos)->fElementName;
              aSQL+=cmp;
              appendFieldValueLiteral(*valfldP, (*pos)->dbfieldtype,(*pos)->maxsize, (*mainpos)->floating_ts, aSQL);
            }
            else {
              // (datefield >< val OR (datefield = val AND timefield >< val))
              aSQL+=(*mainpos)->fElementName;
              aSQL+=cmp;
              appendFieldValueLiteral(*valfldP, (*mainpos)->dbfieldtype,(*mainpos)->maxsize, (*mainpos)->floating_ts, aSQL);
              aSQL+=" OR (";
              aSQL+=(*mainpos)->fElementName;
              aSQL+='=';
              appendFieldValueLiteral(*valfldP, (*mainpos)->dbfieldtype,(*mainpos)->maxsize, (*mainpos)->floating_ts, aSQL);
              aSQL+=" AND ";
              // - time field
              aSQL+=(*pos)->fElementName;
              aSQL+=cmp;
              appendFieldValueLiteral(*valfldP, (*pos)->dbfieldtype,(*pos)->maxsize, (*mainpos)->floating_ts, aSQL);
              aSQL+=')';
            }
            aSQL+=')';
          }
          else {
            // standard case
            // field ><= val
            if (caseInsensitive) {
              aSQL+="LOWER(";
              aSQL+=(*mainpos)->fElementName;
              aSQL+=')';
              aSQL+=cmp; // operator
              aSQL+="LOWER(";
              appendFieldValueLiteral(*valfldP, (*mainpos)->dbfieldtype,(*mainpos)->maxsize, (*mainpos)->floating_ts, aSQL);
              aSQL+=')';
            }
            else {
              aSQL+=(*mainpos)->fElementName;
              aSQL+=cmp;
              appendFieldValueLiteral(*valfldP, (*mainpos)->dbfieldtype,(*mainpos)->maxsize, (*mainpos)->floating_ts, aSQL);
            }
          }
          // field can be deleted now
          delete valfldP;
        } // not special value
      } // refers to field
    } // simple boolean term
    // now check logical operators
    // - skip spaces
    c=*aPos++;
    while (c==' ') c=*(++aPos);
    // - check char at aPos
    if (c=='|') aSQL+=" OR ";
    else if (c=='&') aSQL+= " AND ";
    else {
      aPos--; // let caller check it
      break; // end of logical term
    }
  } while (true); // process terms until all done
  // conversion ok
  return true;
} // TODBCApiDS::appendFilterTerm


#endif // OBJECT_FILTERING


#ifdef ODBCAPI_SUPPORT

// get one or two successive columns as time stamp depending on config
void TODBCApiDS::getColumnsAsTimestamp(
  SQLHSTMT aStatement,
  sInt16 &aColNumber, // will be updated by 1 or 2
  bool aCombined,
  lineartime_t &aTimestamp,
  timecontext_t aTargetContext
)
{
  lineartime_t t;

  if (aCombined) {
    // combined date/time
    fAgentP->getColumnValueAsTimestamp(aStatement,aColNumber++,aTimestamp);
  }
  else {
    // date, then time
    fAgentP->getColumnValueAsDate(aStatement,aColNumber++,aTimestamp);
    fAgentP->getColumnValueAsTime(aStatement,aColNumber++,t);
    aTimestamp+=t; // add time to date
  }
  // convert to target zone requested
  if (!TCTX_IS_UNKNOWN(aTargetContext)) {
    TzConvertTimestamp(aTimestamp,fConfigP->fDataTimeZone,aTargetContext,getSessionZones(),TCTX_UNKNOWN);
  }
} // TODBCApiDS::getColumnsAsTimestamp

#endif // ODBCAPI_SUPPORT


// - get a column as integer based timestamp
lineartime_t TODBCApiDS::dbIntToLineartimeAs(
  sInt64 aDBInt, TDBFieldType aDbfty,
  timecontext_t aTargetContext
)
{
  lineartime_t ts = dbIntToLineartime(aDBInt, aDbfty);
  // convert to target zone requested
  if (!TCTX_IS_UNKNOWN(aTargetContext)) {
    TzConvertTimestamp(ts,fConfigP->fDataTimeZone,aTargetContext,getSessionZones(),TCTX_UNKNOWN);
  }
  return ts;
} // TODBCApiDS::dbIntToLineartimeAs



// create local ID with special algorithm
void TODBCApiDS::createLocalID(string &aLocalID,TSpecialIDMode aSpecialIDMode)
{
  lineartime_t unixms;
  switch (aSpecialIDMode) {
    case sidm_unixmsrnd6:
      // ID is generated by UNIX time in milliseconds, with 6 digits of random
      // appended.
      // - get Unix time in milliseconds:
      unixms =
        (getSession()->getSystemNowAs(TCTX_SYSTEM)-UnixToLineartimeOffset) // unix time in lineartime_t units
        *(1000/secondToLinearTimeFactor); // convert into ms
      unixms*=1000000; // room for 6 more digits
      // - add random number between 0 and 999999
      unixms+=(sInt32)rand()*1000000/RAND_MAX;
      // - make numeric ID out of this
      StringObjPrintf(aLocalID,"%lld",(long long)unixms);
      break;
    default:
      aLocalID="<error>";
  }
} // TODBCApiDS::createLocalID


#ifdef HAS_SQL_ADMIN

// update sync target
localstatus TODBCApiDS::updateSyncTarget(SQLHSTMT aStatement, bool aSessionFinished)
{
  string sql,s;
  localstatus sta=LOCERR_OK;

  resetSQLParameterMaps();
  sql = fConfigP->fUpdateSyncTargetSQL;
  sInt32 i;
  // Suspend/Resume
  // - alert code for next resume
  StringSubst(sql,"%SUA",fResumeAlertCode,4);
  // - suspend reference time
  i=0; while((i=sql.find("%dSU",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(fPreviousSuspendCmpRef, s, true, false, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,4,s); i+=s.size();
  }
  // - suspend reference time
  i=0; while((i=sql.find("%tSU",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(fPreviousSuspendCmpRef, s, false, true, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,4,s); i+=s.size();
  }
  // - suspend reference time
  i=0; while((i=sql.find("%SU",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(fPreviousSuspendCmpRef, s, true, true, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,3,s); i+=s.size();
  }
  // - last suspend identifier (for derived datastores that might need another token than time)
  StringSubst(sql,"%iSU",fPreviousSuspendIdentifier,4,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
  // - anchor
  StringSubst(sql,"%A",fLastRemoteAnchor,2,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
  // Suspend/Resume in mid-chunk
  // - lastitem
  StringSubst(sql,"%pSU",fLastSourceURI,4,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
  StringSubst(sql,"%pTU",fLastTargetURI,4,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
  // - lastitemstatus
  StringSubst(sql,"%pSt",fLastItemStatus,4);
  // - partial item State/Mode
  long pista;
  if (fPartialItemState==pi_state_save_incoming)
    pista=(long)pi_state_loaded_incoming;
  else if (fPartialItemState==pi_state_save_outgoing)
    pista=(long)pi_state_loaded_outgoing;
  else
    pista=(long)pi_state_none;
  StringSubst(sql,"%pM",pista,3);
  // - size info
  StringSubst(sql,"%pTS",fPITotalSize,4);
  StringSubst(sql,"%pUS",fPIUnconfirmedSize,4);
  StringSubst(sql,"%pSS",fPIStoredSize,4);
  // - buffered data
  i=0; while((i=sql.find("%pDAT",i))!=string::npos) {
    sql.replace(i,5,"?"); i+=1;
    // now bind data to param
    TParameterMap map;
    map.inparam=true;
    map.outparam=false;
    map.parammode=param_buffer;
    map.outSiz=NULL;
    map.dbFieldType=dbft_blob;
    // - pass the buffer
    map.mybuffer=false; // not owned by ODBC api
    if (fPIStoredDataP) {
      map.BufferLength=fPIStoredSize;
      map.StrLen_or_Ind=fPIStoredSize;
      map.ParameterValuePtr=fPIStoredDataP; // the BLOB data to store
    }
    else {
      map.BufferLength=0;
      map.StrLen_or_Ind=SQL_NULL_DATA; // no data
      map.ParameterValuePtr=NULL;
    }
    // save in list
    fParameterMaps.push_back(map);
  }
  // - last to remote sync reference date
  //   Note: If we DON'T HAVE fSyncTimeStampAtEnd, but HAVE fOneWayFromRemoteSupported,
  //         we MUST NOT store the reference time here, but save save session start time.
  //         The reference time will be saved under %dRS
  lineartime_t dltime;
  if (!fConfigP->fSyncTimeStampAtEnd && fConfigP->fOneWayFromRemoteSupported)
    dltime = fPreviousSyncTime;
  else
    dltime = fPreviousToRemoteSyncCmpRef;
  i=0; while((i=sql.find("%dL",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(dltime, s, true, false, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,3,s); i+=s.size();
  }
  // - last to remote sync reference time
  i=0; while((i=sql.find("%tL",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(dltime, s, false, true, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,3,s); i+=s.size();
  }
  // - last to remote sync reference date/timestamp
  i=0; while((i=sql.find("%L",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(dltime, s, true, true, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,2,s); i+=s.size();
  }
  // - last sync session start date
  i=0; while((i=sql.find("%dS",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(fPreviousSyncTime, s, true, false, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,3,s); i+=s.size();
  }
  // - last sync session start time
  i=0; while((i=sql.find("%tS",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(fPreviousSyncTime, s, false, true, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,3,s); i+=s.size();
  }
  // - last sync session start date/timestamp
  i=0; while((i=sql.find("%S",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(fPreviousSyncTime, s, true, true, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,2,s); i+=s.size();
  }
  // - last sync with data to remote date
  i=0; while((i=sql.find("%dRL",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(fPreviousToRemoteSyncCmpRef, s, true, false, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,4,s); i+=s.size();
  }
  // - last sync with data to remote time
  i=0; while((i=sql.find("%tRL",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(fPreviousToRemoteSyncCmpRef, s, false, true, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,4,s); i+=s.size();
  }
  // - last sync with data to remote date/timestamp
  i=0; while((i=sql.find("%RL",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(fPreviousToRemoteSyncCmpRef, s, true, true, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,3,s); i+=s.size();
  }
  // - last sync with data to remote identifier (for derived datastores that might need another token than time)
  StringSubst(sql,"%iRL",fPreviousToRemoteSyncIdentifier,4,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
  // - last anchor for sync with data to remote date
  //   Note: this is only needed if we DON'T HAVE fSyncTimeStampAtEnd, but HAVE fOneWayFromRemoteSupported,
  //         because then the %L cannot be used as reference time (it must save session time)
  i=0; while((i=sql.find("%dRS",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(fPreviousToRemoteSyncCmpRef, s, true, false, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,4,s); i+=s.size();
  }
  // - last server anchor with data to remote time
  i=0; while((i=sql.find("%tRS",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(fPreviousToRemoteSyncCmpRef, s, false, true, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,4,s); i+=s.size();
  }
  // - last server anchor with data to remote date/timestamp
  i=0; while((i=sql.find("%RS",i))!=string::npos) {
    s.erase();
    fAgentP->lineartimeToLiteralAppend(fPreviousToRemoteSyncCmpRef, s, true, true, TCTX_UTC, fConfigP->fDataTimeZone);
    sql.replace(i,3,s); i+=s.size();
  }
  // now substitute standard: %f=folderkey, %u=userkey, %d=devicekey, %t=targetkey
  // - Note: it is important that %t,%d is checked here, after %dL, %dS, %tL and %tS above!!
  DoSQLSubstitutions(sql);
  // - bind possible params
  bindSQLParameters(aStatement,false);
  // - issue
  execSQLStatement(aStatement,sql,false,"updating anchor/lastsync",false);
  return LOCERR_OK;
} // TODBCApiDS::updateSyncTarget



// update map changes from memory list into actual map table
localstatus TODBCApiDS::updateODBCMap(SQLHSTMT aStatement, bool aSessionFinishedSuccessfully)
{
  string sql,s;
  localstatus sta=LOCERR_OK;
  TMapContainer::iterator pos;

  // now save the entire list differentially
  pos=fMapTable.begin();
  DEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("updateODBCMap: internal map table has %ld entries (normal and others)",fMapTable.size()));
  while (pos!=fMapTable.end()) {
    DEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
      "updateODBCMap: entryType=%s, localid='%s', remoteID='%s', mapflags=0x%lX, changed=%d, deleted=%d, added=%d, markforresume=%d, savedmark=%d",
      MapEntryTypeNames[(*pos).entrytype],
      (*pos).localid.c_str(),
      (*pos).remoteid.c_str(),
      (*pos).mapflags,
      (int)(*pos).changed,
      (int)(*pos).deleted,
      (int)(*pos).added,
      (int)(*pos).markforresume,
      (int)(*pos).savedmark
    ));
    try {
      // check if item has changed since map table was read, or if its markforresume has changed
      // or if this is a successful end of a session, when we can safely assume that any pending maps
      // are from adds to the client that have never reached the client (otherwise, we'd have got
      // a map for it, even if the add was in a previous session or session attempt)
      if (
        (*pos).changed || (*pos).added || (*pos).deleted || // update of DB needed
        ((*pos).markforresume!=(*pos).savedmark) // mark for resume changed
      ) {
        // make sure it does not get written again if not really modified again
        (*pos).changed=false;
        // update new mapflags w/o changing mapflag_useforresume in the actual flags (as we still need it while session goes on)
        uInt32 newmapflags = (*pos).mapflags & ~mapflag_useforresume;
        if ((*pos).markforresume)
          newmapflags |= mapflag_useforresume;
        // remember last saved state
        (*pos).savedmark=(*pos).markforresume;
        // do something!
        if ((*pos).deleted) {
          if (!(*pos).added) {
            // delete this entry (only needed if it was not also added since last save - otherwise, map entry was never saved to the DB yet)
            IssueMapSQL(aStatement,fConfigP->fMapDeleteSQL,"deleting a map entry",(*pos).entrytype,(*pos).localid.c_str(),NULL,newmapflags);
          }
          // now remove it from the list, such that we don't try to delete it again
          TMapContainer::iterator delpos=pos++; // that's the next to have a look at
          fMapTable.erase(delpos); // remove it now
          continue; // pos is already updated
        } // deleted
        else if ((*pos).added) {
          // add a new entry
          IssueMapSQL(aStatement,fConfigP->fMapInsertSQL,"inserting a map entry",(*pos).entrytype,(*pos).localid.c_str(),(*pos).remoteid.c_str(),newmapflags);
          // is now added, don't add again later
          (*pos).added=false;
        }
        else {
          // explicitly changed or needs update because of resume mark or pendingmap flag
          // change existing entry
          IssueMapSQL(aStatement,fConfigP->fMapUpdateSQL,"changing a map entry",(*pos).entrytype,(*pos).localid.c_str(),(*pos).remoteid.c_str(),newmapflags);
        }
      } // if something changed
      // anyway - reset mark for resume, it must be reconstructed before next save
      (*pos).markforresume=false;
      // next
      pos++;
    }
    // catch exceptions, but nevertheless continue writing
    catch (exception &e) {
      PDEBUGPRINTFX(DBG_ERROR,("******** TODBCApiDS::updateODBCMap exception: %s",e.what()));
      sta=510;
      break;
    }
    catch (...) {
      DEBUGPRINTFX(DBG_ERROR,("******** TODBCApiDS::updateODBCMap unknown exception"));
      sta=510;
      break;
    }
  } // while
  return sta;
} // TODBCApiDS::updateMap


#endif // HAS_SQL_ADMIN


// called when message processing
void TODBCApiDS::dsEndOfMessage(void)
{
  string msg,state;

  #ifdef ODBCAPI_SUPPORT
  SQLRETURN res;
  // commit data if not anyway committed at end of every item
  if (
    fODBCConnectionHandle!=SQL_NULL_HANDLE &&
    !fConfigP->fCommitItems
  ) {
    DEBUGPRINTFX(DBG_DATA+DBG_DBAPI,("dsEndOfMessage - committing transactions at end of message"));
    res=SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_COMMIT);
    if (fAgentP->getODBCError(res,msg,state,SQL_HANDLE_DBC,getODBCConnectionHandle())) {
      DEBUGPRINTFX(DBG_ERROR,("dsEndOfMessage: SQLEndTran failed: %s",msg.c_str()));
    }
  }
  #endif // ODBCAPI_SUPPORT

  // let ancestor do things
  inherited::dsEndOfMessage();
} // TODBCApiDS::dsEndOfMessage



// Simple DB API access interface methods


// Zap all data in syncset (note that everything outside the sync set will remain intact)
localstatus TODBCApiDS::apiZapSyncSet(void)
{
  localstatus sta = LOCERR_OK;

  try {
    string sql=fConfigP->fDataZapSQL;
    if (sql.empty()) {
      // we have no statement that can zap everything at once, so we'll have to do
      // use generic one by one syncset deletion
      sta = zapSyncSetOneByOne();
    }
    else {
      // we have SQL statement(s) for zapping, use it
      sInt16 i=0;
      uInt16 setno;
      while (getNextSQLStatement(fConfigP->fDataZapSQL,i,sql,setno)) {
        DoDataSubstitutions(sql,fConfigP->fFieldMappings.fFieldMapList,0,false,false);
        // - issue
        prepareSQLStatement(fODBCWriteStatement, sql.c_str(), true, "slow-refresh from remote: deleting all records in sync set first");
        execSQLStatement(fODBCWriteStatement, sql, true, NULL, true);
        finalizeSQLStatement(fODBCWriteStatement, true);
      } // while more statements
    }
  }
  catch (exception &e) {
    PDEBUGPRINTFX(DBG_ERROR,("apiZapSyncSet exception: %s",e.what()));
    return 510;
  }
  // done
  return sta;
} // TODBCApiDS::apiZapSyncSet


// read sync set IDs and mod dates.
// - If aNeedAll is set, all data fields are needed, so apiReadSyncSet MAY
//   read items here already. Note that apiReadSyncSet MAY read items here
//   even if aNeedAll is not set (if it is more efficient than reading
//   them separately afterwards).
localstatus TODBCApiDS::apiReadSyncSet(bool aNeedAll)
{
  string sql;
  localstatus sta = LOCERR_OK;

  // get field map list
  TFieldMapList &fml = fConfigP->fFieldMappings.fFieldMapList;
  #ifdef SQLITE_SUPPORT
  // start reading
  if (fUseSQLite) {
    // open the SQLite file
    PDEBUGPRINTFX(DBG_DBAPI,("Opening SQLite3 file '%s'",fConfigP->fSQLiteFileName.c_str()));
    int rc = sqlite3_open(fConfigP->fSQLiteFileName.c_str(), &fSQLiteP);
    fAgentP->checkSQLiteError(rc,fSQLiteP);
    // set the database (lock) timeout
    rc = sqlite3_busy_timeout(fSQLiteP, fConfigP->fSQLiteBusyTimeout*1000);
    fAgentP->checkSQLiteError(rc,fSQLiteP);
  }
  else
  #endif
  #ifdef ODBCAPI_SUPPORT
  {
    fODBCReadStatement = fAgentP->newStatementHandle(getODBCConnectionHandle());
  }
  #else
    return 510; // no API -> DB error
  #endif // ODBCAPI_SUPPORT
  #ifdef SCRIPT_SUPPORT
  // process mappings init script
  fWriting=false;
  fInserting=false;
  fDeleting=false;
  fAgentP->fScriptContextDatastore=this;
  if (!TScriptContext::executeTest(true,fScriptContextP,fConfigP->fFieldMappings.fInitScript,fConfigP->getDSFuncTableP(),fAgentP))
    throw TSyncException("<initscript> failed");
  #endif
  // read list of all local IDs that are in the current sync set
  DeleteSyncSet();
  #ifdef SYDEBUG
  string ts;
  StringObjTimestamp(ts,getPreviousToRemoteSyncCmpRef());
  PDEBUGPRINTFX(DBG_DATA,(
    "Now reading local sync set: last sync to remote was at %s",
    ts.c_str()
  ));
  #endif
  // we don't need to load the syncset if we are only refreshing from remote
  // but we also must load it if we can't zap without it on slow refresh
  // or if the syncset is needed to retrieve items
  if (!fRefreshOnly || (fSlowSync && apiNeedSyncSetToZap()) || implNeedSyncSetToRetrieve()) {
    // %%% add checking for aNeedAll and decide to use another (tbd) SQL to get all
    //     records with all fields with this single query
    // - SELECT localid,modifieddate(,modifiedtime) FROM data WHERE <folderkey, filter conds, other conds>
    //   NOTE: this must always be a SINGLE statement (no %GO() are allowed) !
    //   Expects 2 or 3 (when date&time are separate) columns: localid,modifieddate(,modifiedtime)
    sql=fConfigP->fLocalIDAndTimestampFetchSQL;
    DoDataSubstitutions(sql,fml,0,false);
    // - prepare
    prepareSQLStatement(fODBCReadStatement, sql.c_str(), true, "reading of all localIDs/moddates in sync set");
    // - issue
    execSQLStatement(fODBCReadStatement, sql, true, NULL, true);
    // - fetch data
    while (fetchNextRow(fODBCReadStatement, true)) {
      // get local ID and mod date
      TSyncSetItem *syncsetitemP = new TSyncSetItem;
      if (!syncsetitemP) throw TSyncException(DEBUGTEXT("cannot allocate new syncsetitem","odds12"));
      syncsetitemP->isModified=false;
      syncsetitemP->isModifiedAfterSuspend=false;
      lineartime_t lastmodified = 0;
      #ifdef SQLITE_SUPPORT
      if (fUseSQLite) {
        // SQLite
        sInt16 col=0; // SQLite has 0 based column index
        // - localid
        syncsetitemP->localid = (const char *)sqlite3_column_text(fSQLiteStmtP,col++);
        // - modified timestamp
        lastmodified =  dbIntToLineartimeAs(sqlite3_column_int64(fSQLiteStmtP,col++), fConfigP->fLastModDBFieldType, TCTX_UTC);
      }
      else
      #endif
      {
        #ifdef ODBCAPI_SUPPORT
        // ODBC
        sInt16 col=1; // ODBC has 1 based column index
        fAgentP->getColumnValueAsString(fODBCReadStatement, col++, syncsetitemP->localid, chs_ascii);
        // get modified timestamp
        if (fConfigP->fLastModDBFieldType==dbft_timestamp)
          getColumnsAsTimestamp(fODBCReadStatement, col, fConfigP->fModifiedTimestamp, lastmodified, TCTX_UTC);
        else {
          uInt32 u;
          fAgentP->getColumnValueAsULong(fODBCReadStatement, col, u);
          lastmodified = dbIntToLineartimeAs(u, fConfigP->fLastModDBFieldType, TCTX_UTC);
        }
        #endif // ODBCAPI_SUPPORT
      }
      // compare now
      syncsetitemP->isModified = lastmodified > getPreviousToRemoteSyncCmpRef();
      syncsetitemP->isModifiedAfterSuspend = lastmodified > getPreviousSuspendCmpRef();
      #ifdef SYDEBUG
      StringObjTimestamp(ts,lastmodified);
      PDEBUGPRINTFX(DBG_DATA+DBG_EXOTIC,(
        "read local item info in sync set: localid='%s', last modified %s%s%s",
        syncsetitemP->localid.c_str(),
        ts.c_str(),
        syncsetitemP->isModified ? " -> MODIFIED since last sync" : "",
        syncsetitemP->isModifiedAfterSuspend ? " AND since last suspend" : ""
      ));
      #endif
      // %%% for now, we do not read item contents yet
      syncsetitemP->itemP=NULL; // no item data
      // save ID in list
      fSyncSetList.push_back(syncsetitemP);
    }
    // - no more records
    finalizeSQLStatement(fODBCReadStatement, true);
  } // not refreshing
  PDEBUGPRINTFX(DBG_DATA,(
    "Fetched %ld items from database (not necessarily all visible in SyncSet!)",
    (long)fSyncSetList.size()
  ));
  return sta;
} // TODBCApiDS::apiReadSyncSet


// fetch actual record from DB by localID
localstatus TODBCApiDS::apiFetchItem(TMultiFieldItem &aItem, bool aReadPhase, TSyncSetItem *aSyncSetItemP)
{
  // decide what statement to use
  SQLHSTMT statement = SQL_NULL_HANDLE;
  #ifdef SQLITE_SUPPORT
  if (!fUseSQLite)
  #endif
  {
    #ifdef ODBCAPI_SUPPORT
    if (aReadPhase)
      statement = fODBCReadStatement; // we can reuse the statement that is already here
    else
      statement = fAgentP->newStatementHandle(getODBCConnectionHandle());
    #endif // ODBCAPI_SUPPORT
  }
  // get field map list
  TFieldMapList &fml = fConfigP->fFieldMappings.fFieldMapList;
  // assume ok
  localstatus sta=LOCERR_OK;
  // now fetch
  try {
    string sql;
    TMultiFieldItem *myitemP = (TMultiFieldItem *)&aItem;
    // execute statements needed to fetch record data
    sInt16 i=0;
    uInt16 setno;
    while (getNextSQLStatement(fConfigP->fDataFetchSQL,i,sql,setno)) {
      // - something like: SELECT %N FROM datatable WHERE localid=%k AND folderkey=%f
      resetSQLParameterMaps();
      DoDataSubstitutions(sql,fml,setno,false,false,myitemP); // item needed for %k
      // - prepare parameters
      prepareSQLStatement(statement, sql.c_str(), true, "getting item data");
      bindSQLParameters(statement,true);
      // - issue
      execSQLStatement(statement,sql,true,NULL,true);
      // fetch
      if (!fetchNextRow(statement,true)) {
        // - close cursor anyway
        finalizeSQLStatement(statement,true);
        // No data for select statement
        // This is ignored for all setno except setno=0
        if (setno==0) {
          // this record cannot be found
          sta=404; // not found
          break; // no need to execute further statements
        } // setno==0, that is, non-optional data
        // if optional data is not present, just NOP
      } // has no data
      else {
        // - fill item with fields from SQL query
        sInt16 col=1;
        fillFieldsFromSQLResult(statement,col,*myitemP,fml,setno,0);
        // close cursor of main fetch
        finalizeSQLStatement(statement,true);
        // get out params
        saveAndCleanupSQLParameters(statement,true);
      }
    } // while more statements
    // Finish reading record (if one fetched at all, and not converted to delete etc.)
    if (sta==LOCERR_OK) {
      #ifdef ARRAYDBTABLES_SUPPORT
      // - also read array fields from auxiliary tables, if any
      if (fHasArrayFields) {
        // get data from linked array tables as well
        readArrayFields(
          statement,
          *myitemP,
          fml
        );
      }
      #endif
      #ifdef SCRIPT_SUPPORT
      // - finally process afterread script of entire record
      fArrIdx=0; // base item
      fParentKey=myitemP->getLocalID();
      fAgentP->fScriptContextDatastore=this;
      if (!TScriptContext::execute(fScriptContextP,fConfigP->fFieldMappings.fAfterReadScript,fConfigP->getDSFuncTableP(),fAgentP,myitemP,true))
        throw TSyncException("<afterreadscript> failed");
      #endif
    }
    #ifdef SQLITE_SUPPORT
    if (!fUseSQLite)
    #endif
    {
      #ifdef ODBCAPI_SUPPORT
      // dispose statement handle if we have allocated it
      if (!aReadPhase) SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
      #endif
    }
  }
  catch (...) {
    #ifdef SQLITE_SUPPORT
    if (!fUseSQLite)
    #endif
    {
      #ifdef ODBCAPI_SUPPORT
      // dispose statement handle if we have allocated it
      if (!aReadPhase) SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
      #endif
    }
    // re-throw
    throw;
  }
  // return status
  return sta;
} // TODBCApiDS::apiFetchItem


/// end of syncset reading phase
localstatus TODBCApiDS::apiEndDataRead(void)
{
  #ifdef ODBCAPI_SUPPORT
  SQLRETURN res=SQL_SUCCESS;
  #endif
  localstatus sta = LOCERR_OK;

  #ifdef ODBCAPI_SUPPORT
  // release the statement handle (if any)
  try {
    DEBUGPRINTFX(DBG_DATA+DBG_DBAPI,("EndDataRead: committing read phase"));
    if (fODBCReadStatement!=SQL_NULL_HANDLE) {
      res=SafeSQLFreeHandle(SQL_HANDLE_STMT,fODBCReadStatement);
      fODBCReadStatement=SQL_NULL_HANDLE;
      checkConnectionError(res);
      // Commit the transaction (to make sure a new one begins when starting to write)
      SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_COMMIT);
      checkConnectionError(res);
    }
  }
  catch (exception &e) {
    PDEBUGPRINTFX(DBG_ERROR,("******** EndDataRead exception: %s",e.what()));
    sta=510;
  }
  #endif // ODBCAPI_SUPPORT
  return sta;
} // TODBCApiDS::apiEndDataRead


// start of data write
localstatus TODBCApiDS::apiStartDataWrite(void)
{
  // create statement handle for writing if we don't have one already
  #ifdef SQLITE_SUPPORT
  if (!fUseSQLite)
  #endif
  {
    #ifdef ODBCAPI_SUPPORT
    // for ODBC only
    if (fODBCWriteStatement==SQL_NULL_HANDLE)
      fODBCWriteStatement = fAgentP->newStatementHandle(getODBCConnectionHandle());
    #endif
  }
  return LOCERR_OK;
} // TODBCApiDS::apiStartDataWrite


// generate new LocalID into fLastGeneratedLocalID.
// - can be used with %X,%x escape in SQL statements.
void TODBCApiDS::nextLocalID(TSpecialIDMode aMode,SQLHSTMT aStatement)
{
  if (aMode!=sidm_none) {
    createLocalID(fLastGeneratedLocalID,aMode);
  }
  else {
    if (fConfigP->fDetermineNewIDOnce) {
      // we use incremented starting value
      StringObjPrintf(fLastGeneratedLocalID,"%ld",(long)fNextLocalID++);
    }
    else {
      #ifdef SCRIPT_SUPPORT
      // first check for script
      if (!fConfigP->fLocalIDScript.empty()) {
        // call script tro obtain new localID
        TItemField *resP=NULL;
        fAgentP->fScriptContextDatastore=this;
        if (!TScriptContext::executeWithResult(
          resP, // can be default result or NULL, will contain result or NULL if no result
          fScriptContextP,
          fConfigP->fLocalIDScript,
          fConfigP->getDSFuncTableP(),  // context function table
          fAgentP, // context data (myself)
          NULL, false, NULL, false
        ))
          throw TSyncException("<localidscript> failed");
        if (resP) {
          // get ID
          resP->getAsString(fLastGeneratedLocalID);
          delete resP;
        }
      }
      #endif
      // get new statement if none was passed
      string sql=fConfigP->fObtainNewLocalIDSql;
      // obtainNewLocalIDSql can be empty, for example if ID is returned in an
      // out param from the inserting statement(s) or generated by fLocalIDScript
      if (!sql.empty()) {
        #ifdef SQLITE_SUPPORT
        if (fUseSQLite) {
          throw TSyncException("SQLite does not support <obtainlocalidsql>");
        }
        else
        #endif
        {
          #ifdef ODBCAPI_SUPPORT
          // there is a statement to execute
          SQLHSTMT statement=aStatement;
          if (!statement) statement=fAgentP->newStatementHandle(getODBCConnectionHandle());
          // issue the SQL
          try {
            // execute query BEFORE insert to get unique ID value for new record
            DoSQLSubstitutions(sql);
            execSQLStatement(statement,sql,true,"getting local ID for new record",true);
            // - fetch result row
            SQLRETURN res=SafeSQLFetch(statement);
            fAgentP->checkStatementError(res,statement);
            // - get value of first field as string
            if (!fAgentP->getColumnValueAsString(statement,1,fLastGeneratedLocalID,chs_ascii)) {
              throw TSyncException("Failed getting new localID");
            }
            // close cursor (for re-using statement handle, this is needed)
            SafeSQLCloseCursor(statement);
          }
          catch (...) {
            if (!aStatement) SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
            throw;
          }
          // get rid of local statement
          if (!aStatement) SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
          #endif
        }
      }
    }
  }
  PDEBUGPRINTFX(DBG_DATA,("Generated new localID: %s",fLastGeneratedLocalID.c_str()));
} // TODBCApiDS::nextLocalID


// private helper: start writing item
void TODBCApiDS::startWriteItem(void)
{
  // make sure transaction is complete if we are in item commit mode
  #ifdef ODBCAPI_SUPPORT
  SQLRETURN res;
  if (fConfigP->fCommitItems) {
    PDEBUGPRINTFX(DBG_DATA+DBG_DBAPI,("startWriteItem: commititems=true, item processing starts with new transaction"));
    res=SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_COMMIT);
    checkConnectionError(res);
  }
  #endif
} // TODBCApiDS::startWriteItem


// private helper: end writing item
void TODBCApiDS::endWriteItem(void)
{
  #ifdef ODBCAPI_SUPPORT
  SQLRETURN res;
  if (fConfigP->fCommitItems) {
    PDEBUGPRINTFX(DBG_DATA+DBG_DBAPI,("endWriteItem: commititems=true, commit changes to DB"));
    res=SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_COMMIT);
    checkConnectionError(res);
  }
  #endif
} // TODBCApiDS::endWriteItem



// add new item to datastore, returns created localID
localstatus TODBCApiDS::apiAddItem(TMultiFieldItem &aItem, string &aLocalID)
{
  localstatus sta=LOCERR_OK;

  startWriteItem();

  // get field map list
  TFieldMapList &fml = fConfigP->fFieldMappings.fFieldMapList;
  try {
    // determine ID
    aLocalID.erase(); // make sure we don't assign bogus ID (fObtainNewIDAfterInsert case)
    if (fConfigP->fSpecialIDMode==sidm_none && !fConfigP->fObtainNewIDAfterInsert) {
      // No repeatable ID creator, and not obtain-after-insert: do it once here
      // - create next local ID using standard method
      nextLocalID(sidm_none,fODBCWriteStatement);
      // - use it
      aLocalID=fLastGeneratedLocalID;
    } // if no special algorithm for IDs (such as system time/random based)
    // add new record
    sInt16 rep=0;
    do {
      try {
        // - make sure we have a new ID in case it is time based
        if (fConfigP->fSpecialIDMode!=sidm_none) {
          // - create next local ID using special method
          nextLocalID(fConfigP->fSpecialIDMode,fODBCWriteStatement);
          // - use it
          aLocalID=fLastGeneratedLocalID;
        }
        // - assign localID to be used (empty here if fObtainNewIDAfterInsert=true)
        aItem.setLocalID(aLocalID.c_str());
        #ifdef SCRIPT_SUPPORT
        // - process beforewrite script
        fWriting=true;
        fInserting=true; // might be reset by ADDBYUPDATING()
        fDeleting=false;
        fAgentP->fScriptContextDatastore=this;
        if (!TScriptContext::execute(fScriptContextP,fConfigP->fFieldMappings.fBeforeWriteScript,fConfigP->getDSFuncTableP(),fAgentP,&aItem,true))
          throw TSyncException("<beforewritescript> failed");
        if (!fInserting) {
          // inserting was turned off by beforewritescript, in particular by ADDBYUPDATING()
          // -> perform an update using the localid set by ADDBYUPDATING()
          PDEBUGPRINTFX(DBG_DATA+DBG_DBAPI,("Updating existing DB row '%s' instead of inserting (ADDBYUPDATING)",aItem.getLocalID()));
          // - return localID to caller (for caller, this looks like a regular add)
          aLocalID = aItem.getLocalID();
          // - perform update instead of insert
          if (!IssueDataWriteSQL(
            fODBCWriteStatement,
            fConfigP->fDataUpdateSQL,
            "updating instead of inserting",
            true, // for update
            fml, // field map
            &aItem // item to read values and localid from
          )) {
            // not found in data table - do a normal add (even if ADDBYUPDATING() wanted an update)
            fInserting = true; // revert back to inserting
            PDEBUGPRINTFX(DBG_ERROR,("Updating existing not possible -> reverting to insert"));
          }
        }
        // check if we still need to insert
        if (fInserting)
        #endif
        {
          // Normal insert
          // - issue statement(s)
          #ifdef ODBCAPI_SUPPORT
          bool hasdata=
          #endif
          IssueDataWriteSQL(
            fODBCWriteStatement,
            fConfigP->fDataInsertSQL,
            "adding record",
            false, // not for update
            fml, // field map
            &aItem // item to read values and localid from
          );
          // copy ID in case we've got it via an output param
          aLocalID=aItem.getLocalID();
          #ifdef SQLITE_SUPPORT
          if (!fUseSQLite)
          #endif
          {
            #ifdef ODBCAPI_SUPPORT
            if (fConfigP->fInsertReturnsID) {
              // insert statement should return ID of record inserted
              if (hasdata) {
                SQLRETURN res=SafeSQLFetch(fODBCWriteStatement);
                if (fAgentP->checkStatementHasData(res,fODBCWriteStatement)) {
                  // get id (must be in first result column of first and only result row)
                  if (!fAgentP->getColumnValueAsString(fODBCWriteStatement,1,fLastGeneratedLocalID,chs_ascii)) {
                    throw TSyncException("Failed getting localID from <datainsertsql> result set");
                  }
                  // - use it
                  aLocalID=fLastGeneratedLocalID;
                }
                else
                  hasdata=false;
                // - close cursor anyway
                SafeSQLCloseCursor(fODBCWriteStatement);
              }
              if (!hasdata)
                throw TSyncException("<datainsertsql> statement did not return insert ID");
            }
            #endif // ODBCAPI_SUPPORT
          }
        } // normal insert
        // done ok
        break;
      }
      catch (TSyncException &e) {
        // error while issuing write
        if (++rep>fConfigP->fInsertRetries) throw; // error, let it show
        PDEBUGPRINTFX(DBG_DATA+DBG_DBAPI,(
          "Insert failed with localid='%s': %s --> retrying",
          aLocalID.c_str(),
          e.what()
        ));
        // repeat once more
        continue;
      }
    } while(true);
    // check if we want to use local ID created implicitly by inserting row
    #ifdef SCRIPT_SUPPORT
    if (fInserting) // only if really inserting (but not in ADDBYUPDATING() case)
    #endif
    {
      if (!fConfigP->fInsertReturnsID && fConfigP->fSpecialIDMode==sidm_none && fConfigP->fObtainNewIDAfterInsert) {
        // determine new ID AFTER insert
        #ifdef SQLITE_SUPPORT
        if (fUseSQLite) {
          // For SQLite, localID is always autocreated ROWID, get it now
          StringObjPrintf(aLocalID,"%lld",sqlite3_last_insert_rowid(fSQLiteP));
          PDEBUGPRINTFX(DBG_DBAPI,("sqlite3_last_insert_rowid() returned %s",aLocalID.c_str()));
        }
        else
        #endif // SQLITE_SUPPORT
        {
          #ifdef ODBCAPI_SUPPORT
          // get local ID using standard method (SQL statement to get ID)
          nextLocalID(sidm_none,fODBCWriteStatement);
          // - use it
          aLocalID=fLastGeneratedLocalID;
          #endif // ODBCAPI_SUPPORT
        }
      }
      // make sure localID is known now
      aItem.setLocalID(aLocalID.c_str());
    } // determine ID of inserted record
    // also write array fields, if any
    #ifdef ARRAYDBTABLES_SUPPORT
    if (fHasArrayFields) {
      // write data to linked array tables as well
      // - now write
      writeArrayFields(
        fODBCWriteStatement,
        aItem,
        fml,
        #ifdef SCRIPT_SUPPORT
        fInserting // if really performed insert (i.e. not ADDBYUPDATING()), child record cleaning is not necessary (but can be forced by fAlwaysCleanArray)
        #else
        true // is an insert, so child record cleaning not necessary (but can be forced by fAlwaysCleanArray)
        #endif
      );
    }
    #endif
    #ifdef SCRIPT_SUPPORT
    // process oevrall afterwrite script
    // Note: fInserting is still valid from above (and can be false in ADDBYUPDATING() case)
    fWriting=true;
    fDeleting=false;
    fAgentP->fScriptContextDatastore=this;
    if (!TScriptContext::execute(fScriptContextP,fConfigP->fFieldMappings.fAfterWriteScript,fConfigP->getDSFuncTableP(),fAgentP,&aItem,true))
      throw TSyncException("<afterwritescript> failed");
    #endif
  }
  catch (...) {
    endWriteItem();
    throw;
  }
  // end writing
  endWriteItem();
  return sta;
} // TODBCApiDS::apiAddItem


// update existing item in datastore, returns 404 if item not found
localstatus TODBCApiDS::apiUpdateItem(TMultiFieldItem &aItem)
{
  localstatus sta=LOCERR_OK;

  startWriteItem();
  // get field map list
  TFieldMapList &fml = fConfigP->fFieldMappings.fFieldMapList;
  try {
    // update record with this localID
    #ifdef SCRIPT_SUPPORT
    // - process beforewrite script
    fWriting=true;
    fInserting=false;
    fDeleting=false;
    fAgentP->fScriptContextDatastore=this;
    if (!TScriptContext::execute(fScriptContextP,fConfigP->fFieldMappings.fBeforeWriteScript,fConfigP->getDSFuncTableP(),fAgentP,&aItem,true))
      throw TSyncException("<beforewritescript> failed");
    #endif
    // - issue
    if (!IssueDataWriteSQL(
      fODBCWriteStatement,
      fConfigP->fDataUpdateSQL,
      "replacing record",
      true, // for update
      fml, // field map
      &aItem // item to read values and localid from
    )) {
      // not found in data table
      sta=404;
    }
    else {
      // also write array fields, if any
      #ifdef ARRAYDBTABLES_SUPPORT
      if (fHasArrayFields) {
        // write data to linked array tables as well
        writeArrayFields(
          fODBCWriteStatement,
          aItem, // item to read values from
          fml,
          false // is not an insert, update needs erasing first in all cases!
        );
      }
      #endif
      #ifdef SCRIPT_SUPPORT
      // process oevrall afterwrite script
      fWriting=true;
      fInserting=false;
      fDeleting=false;
      fAgentP->fScriptContextDatastore=this;
      if (!TScriptContext::execute(fScriptContextP,fConfigP->fFieldMappings.fAfterWriteScript,fConfigP->getDSFuncTableP(),fAgentP,&aItem,true))
        throw TSyncException("<afterwritescript> failed");
      #endif
    }
  }
  catch (...) {
    endWriteItem();
    throw;
  }
  // end writing
  endWriteItem();
  return sta;
} // TODBCApiDS::apiUpdateItem


// delete existing item in datastore, returns 211 if not existing any more
localstatus TODBCApiDS::apiDeleteItem(TMultiFieldItem &aItem)
{
  localstatus sta=LOCERR_OK;

  startWriteItem();
  // get field map list
  TFieldMapList &fml = fConfigP->fFieldMappings.fFieldMapList;
  try {
    // - issue statements for delete (could be UPDATE or DELETE)
    if (!IssueDataWriteSQL(
      fODBCWriteStatement,
      fConfigP->fDataDeleteSQL,
      "deleting record",
      false, // not for update
      fml, // field map
      &aItem // item to read values and localid from
    )) {
      sta=211; // item not deleted, was not there any more
    }
    else {
      // also delete array fields, if any
      #ifdef ARRAYDBTABLES_SUPPORT
      // get data from linked array tables as well
      deleteArrayFields(
        fODBCWriteStatement,
        aItem,
        fConfigP->fFieldMappings.fFieldMapList
      );
      #endif
      #ifdef SCRIPT_SUPPORT
      // process overall afterwrite script
      fWriting=true;
      fInserting=false;
      fDeleting=true;
      fAgentP->fScriptContextDatastore=this;
      if (!TScriptContext::execute(fScriptContextP,fConfigP->fFieldMappings.fAfterWriteScript,fConfigP->getDSFuncTableP(),fAgentP,&aItem,true))
        throw TSyncException("<afterwritescript> failed");
      #endif
    }
  }
  catch (...) {
    endWriteItem();
    throw;
  }
  // end writing
  endWriteItem();
  return sta;
} // TODBCApiDS::apiDeleteItem


/// @brief Load admin data from ODBC database
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
///   - fPreviousToRemoteSyncIdentifier = string identifying last session that sent data to remote (needs only be saved
///                         if derived datastore cannot work with timestamps and has its own identifier).
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
localstatus TODBCApiDS::apiLoadAdminData(
  const char *aDeviceID,    // remote device URI (device ID)
  const char *aDatabaseID,  // database ID
  const char *aRemoteDBID   // database ID of remote device
)
{
  #ifndef HAS_SQL_ADMIN
  return 510; // must use plugin, no ODBC
  #else // HAS_SQL_ADMIN
  localstatus sta=0; // assume ok
  string sql;

  // ODBC based target/map
  fODBCAdminData=true;
  // determine Folder key, if any
  sta=LOCERR_OK;
  // - determine folder name, if any
  string bname,foldername;
  analyzeName(aDatabaseID,&bname,&foldername);
  // - search on foldername
  SQLRETURN res;
  SQLHSTMT statement=fAgentP->newStatementHandle(getODBCConnectionHandle());
  try {
    // get SQL
    sql = fConfigP->fFolderKeySQL;
    if (!sql.empty()) {
      // substitute: %F = foldername
      StringSubst(sql,"%F",foldername,2,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
      DoSQLSubstitutions(sql); // for %u = userkey, %d=devicekey
      // issue
      execSQLStatement(statement,sql,true,"getting folder key",false);
      // - fetch result row
      res=SafeSQLFetch(statement);
      if (!fAgentP->checkStatementHasData(res,statement)) {
        // No data: user does not have permission for this folder
        PDEBUGPRINTFX(DBG_ERROR,("apiLoadAdminData: User has no permission for acessing folder '%s'",foldername.c_str()));
        sta=403;
      }
      else {
        // get folder key
        fAgentP->getColumnValueAsString(statement,1,fFolderKey,chs_ascii);
        PDEBUGPRINTFX(DBG_ADMIN,("apiLoadAdminData: Folder key is '%s'",fFolderKey.c_str()));
      }
      SafeSQLCloseCursor(statement);
    } // if folderkeySQL
    else {
      // no folderkey, just copy user key
      fFolderKey=fAgentP->getUserKey();
    }
    if (sta==LOCERR_OK) {
      // now determine sync target key
      bool madenew=false;
      do {
        // get SQL
        sql=fConfigP->fGetSyncTargetSQL;
        // substitute standard: %f=folderkey, %u=userkey, %d=devicekey, %t=targetkey
        DoSQLSubstitutions(sql);
        // substitute specific: %D=deviceid, %P=devicedbpath
        StringSubst(sql,"%D",aDeviceID,2,-1,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
        StringSubst(sql,"%P",aRemoteDBID,2,-1,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
        // issue
        execSQLStatement(statement,sql,true,"getting target info",false);
        // - fetch result row
        res=SafeSQLFetch(statement);
        if (!fAgentP->checkStatementHasData(res,statement)) {
          if (madenew)
            throw TSyncException("apiLoadAdminData: new created SyncTarget is not accessible");
          // target does not yet exist, create new
          DEBUGPRINTFX(DBG_ADMIN,("apiLoadAdminData: Target does not yet exist, create new"));
          SafeSQLCloseCursor(statement);
          // create new sync target entry
          sql=fConfigP->fNewSyncTargetSQL;
          // substitute standard: %f=folderkey, %u=userkey, %d=devicekey, %t=targetkey
          DoSQLSubstitutions(sql);
          // substitute specific: %D=deviceid, %P=devicedbpath
          StringSubst(sql,"%D",aDeviceID,2,-1,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
          StringSubst(sql,"%P",aRemoteDBID,2,-1,fConfigP->sqlPrepCharSet(),fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
          // now issue
          execSQLStatement(statement,sql,false,"creating new target info",false);
          // try to read again
          madenew=true;
          continue;
        }
        else {
          // get data: IN ASCENDING COL ORDER!!!
          sInt16 col=1;
          fAgentP->getColumnValueAsString(statement,col++,fTargetKey,chs_ascii);
          fAgentP->getColumnValueAsString(statement,col++,fLastRemoteAnchor,chs_ascii);
          PDEBUGPRINTFX(DBG_ADMIN,("apiLoadAdminData: Target key is '%s'",fTargetKey.c_str()));
          PDEBUGPRINTFX(DBG_ADMIN,("apiLoadAdminData: Saved Remote Sync Anchor is '%s'",fLastRemoteAnchor.c_str()));
          // first sync if target record is new or if remote anchor is (still) empty
          // NOTE: this is because only a successful sync makes next attempt non-firsttime!
          fFirstTimeSync=madenew || fLastRemoteAnchor.empty();
          // last to remote sync reference date/time OR start of last session date/time
          // Note: If we DON'T HAVE fSyncTimeStampAtEnd, but HAVE fOneWayFromRemoteSupported,
          //       this is NOT the reference time here, but the session start time;
          //       the reference time is saved separately (see below)
          lineartime_t dltime;
          getColumnsAsTimestamp(statement,col,fConfigP->fSyncTimestamp,dltime, TCTX_UTC);
          if (!fConfigP->fSyncTimeStampAtEnd && fConfigP->fOneWayFromRemoteSupported)
            fPreviousSyncTime = dltime;
          else {
            fPreviousToRemoteSyncCmpRef = dltime;
          }
          // In case we DON'T HAVE fSyncTimeStampAtEnd, and DON'T HAVE fOneWayFromRemoteSupported,
          // the above loaded fPreviousToRemoteSyncCmpRef is the ONLY time we get, so for this
          // case we won't get separate session start time below, so initialize it here.
          fPreviousSyncTime = dltime;
          // check for separate start-of-session time
          if (fConfigP->fSyncTimeStampAtEnd) {
            // @note For new V3.0 architecture, this is now a bit the wrong way around, as
            //   we only need the anchor (fPreviousSyncTime) and a reference for the
            //   last-to-remote sync (fPreviousToRemoteSyncCmpRef). However, to remain compatible with
            //   existing DB schemas we need to swap values a bit.
            // In case database cannot explicitly set modification timestamps, we need the start-of-session
            // (which is used for creation of anchor for remote) and we need a comparison reference.
            // - override anchor time with separately stored value
            getColumnsAsTimestamp(statement,col,fConfigP->fSyncTimestamp,fPreviousSyncTime, TCTX_UTC);
          }
          // get date and time of last two-way-sync if database has it stored separately
          // Note: when we have fSyncTimeStampAtEnd, fPreviousToRemoteSyncCmpRef is already set here,
          //       but we'll get it again (it is redundantly stored if we have both fSyncTimeStampAtEnd and fPreviousToRemoteSyncCmpRef)
          // - get from DB if available
          if (fConfigP->fOneWayFromRemoteSupported) {
            // one-way from remote is supported: we have a separate date for last sync that sent changes to remote
            // - override fPreviousToRemoteSyncCmpRef (when one-way-from-remote is supported, this is not
            //   necessarily same as start of last session (fPreviousSyncTime) any more)
            getColumnsAsTimestamp(statement,col,fConfigP->fSyncTimestamp,fPreviousToRemoteSyncCmpRef, TCTX_UTC);
            if (fConfigP->fSyncTimeStampAtEnd) {
              // Note: for V3.0, this may be redundant and already loaded in case we also have fSyncTimeStampAtEnd
              getColumnsAsTimestamp(statement,col,fConfigP->fSyncTimestamp,fPreviousToRemoteSyncCmpRef, TCTX_UTC);
            }
          }
          // get opaque reference identifier from DB, if it stores them
          // (otherwise, implMakeAdminReady will create ISO timestamp strings as standard identifiers)
          if (fConfigP->fStoreSyncIdentifiers) {
            // we have stored the identifier (and do now know its meaning, it's just a string that
            // the DB implementation needs to sort out changed items in the datastore
            fAgentP->getColumnValueAsString(statement,col++,fPreviousToRemoteSyncIdentifier,chs_ascii);
          }
          // Summarize
          #ifdef SYDEBUG
          string ts;
          StringObjTimestamp(ts,fPreviousSyncTime);
          DEBUGPRINTFX(DBG_ADMIN,("apiLoadAdminData: start time of last sync is: %s",ts.c_str()));
          StringObjTimestamp(ts,fPreviousToRemoteSyncCmpRef);
          DEBUGPRINTFX(DBG_ADMIN,("apiLoadAdminData: compare reference time of last-to-remote-sync is: %s",ts.c_str()));
          DEBUGPRINTFX(DBG_ADMIN,("apiLoadAdminData: stored reference identifier of last-to-remote-sync is: '%s'",fPreviousToRemoteSyncIdentifier.c_str()));
          #endif
          // Resume support if there
          if (fConfigP->fResumeSupport) {
            // next column is alert code for resuming
            uInt32 templong;
            fAgentP->getColumnValueAsULong(statement,col++,templong);
            fResumeAlertCode=templong;
            PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("apiLoadAdminData: fResumeAlertCode is %hd",fResumeAlertCode));
            // next column(s) is/are last suspend date/time
            getColumnsAsTimestamp(statement,col,fConfigP->fSyncTimestamp,fPreviousSuspendCmpRef, TCTX_UTC);
            #ifdef SYDEBUG
            StringObjTimestamp(ts,fPreviousSuspendCmpRef);
            PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("apiLoadAdminData: compare reference time of last suspend is %s",ts.c_str()));
            #endif
            // Get last suspend identifier from DB
            if (fConfigP->fStoreSyncIdentifiers) {
              // next line is opaque string idenifier of last sync with transfer to remote
              fAgentP->getColumnValueAsString(statement,col++,fPreviousSuspendIdentifier,chs_ascii);
              PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("apiLoadAdminData: stored reference identifier of last suspend is '%s'",fPreviousSuspendIdentifier.c_str()));
            }
            // get partial item suspend data, if any:
            // - Source,Target,LastStatus,PIState,Totalsize,Unconfirmedsize,StoredSize,BLOB
            if (fConfigP->fResumeItemSupport) {
              // - last item URIs
              fAgentP->getColumnValueAsString(statement,col++,fLastSourceURI,chs_ascii);
              fAgentP->getColumnValueAsString(statement,col++,fLastTargetURI,chs_ascii);
              // - last status
              fAgentP->getColumnValueAsULong(statement,col++,templong); fLastItemStatus=templong;
              // - partial item state
              fAgentP->getColumnValueAsULong(statement,col++,templong); fPartialItemState=(TPartialItemState)templong;
              // - sizes
              fAgentP->getColumnValueAsULong(statement,col++,fPITotalSize);
              fAgentP->getColumnValueAsULong(statement,col++,fPIUnconfirmedSize);
              fAgentP->getColumnValueAsULong(statement,col++,fPIStoredSize);
              // - the BLOB data itself
              string blob;
              fAgentP->getColumnValueAsString(statement,col++,blob,chs_ascii,true);
              // - move it into data
              if (fPIStoredDataP && fPIStoredDataAllocated) smlLibFree(fPIStoredDataP);
              fPIStoredDataAllocated=false;
              fPIStoredDataP=smlLibMalloc(blob.size()+1); // plus terminator for string interpretation
              if (fPIStoredDataP) {
                fPIStoredDataAllocated=true;
                smlLibMemcpy(fPIStoredDataP,blob.c_str(),blob.size());
                if (fPIStoredSize>blob.size()) fPIStoredSize=blob.size(); // security
                *((uInt8 *)fPIStoredDataP+fPIStoredSize)=0; // string terminator
              }
              PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
                "apiLoadAdminData: partial item resume info: src='%s', targ='%s', lastStatus=%hd, state=%hd, totalSz=%ld, unconfSz=%ld, storedSz=%ld",
                fLastSourceURI.c_str(),
                fLastTargetURI.c_str(),
                fLastItemStatus,
                (TSyError)fPartialItemState,
                fPITotalSize,
                fPIUnconfirmedSize,
                fPIStoredSize
              ));
            }
          }
          SafeSQLCloseCursor(statement);
          // now read current map entries of target
          sql=fConfigP->fMapFetchAllSQL;
          DoSQLSubstitutions(sql); // only folderkey,targetkey,userkey is available
          // - issue
          execSQLStatement(statement,sql,true,"reading map into internal list",false);
          // - fetch results
          do {
            res=SafeSQLFetch(statement);
            // check for end
            if (!fAgentP->checkStatementHasData(res,statement)) break;
            // get map entry
            TMapEntry entry;
            fAgentP->getColumnValueAsString(statement,1,entry.localid,chs_ascii);
            fAgentP->getColumnValueAsString(statement,2,entry.remoteid,chs_ascii);
            if (fConfigP->fResumeSupport) {
              // we have a separate entry type
              uInt32 et;
              fAgentP->getColumnValueAsULong(statement,3,et);
              if (et>=numMapEntryTypes)
                entry.entrytype = mapentry_invalid;
              else
                entry.entrytype = (TMapEntryType)et;
              // and some map flags
              fAgentP->getColumnValueAsULong(statement,4,entry.mapflags);
            }
            else {
              entry.mapflags=0; // no flags stored
              entry.entrytype=mapentry_normal; // normal entry
            }
            PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
              "read map entry (type=%s): localid='%s', remoteid='%s', mapflags=0x%lX",
              MapEntryTypeNames[entry.entrytype],
              entry.localid.c_str(),
              entry.remoteid.c_str(),
              entry.mapflags
            ));
            // save entry in list
            entry.changed=false; // not yet changed
            entry.added=false; // already there
            // remember saved state of suspend mark
            entry.markforresume=false; // not yet marked for this session (mark of last session is in mapflag_useforresume!)
            entry.savedmark=entry.mapflags & mapflag_useforresume;
            // IMPORTANT: non-normal entries must be saved as deleted in the main map - they will be re-activated at the
            // next save if needed
            entry.deleted = entry.entrytype!=mapentry_normal; // only normal ones may be saved as existing in the main map
            // save to main map list anyway to allow differential SQL updates to map table (instead of writing everything all the time)
            fMapTable.push_back(entry);
            // now save special maps to extra lists according to type
            // Note: in the main map, these are marked deleted. Before the next saveAdminData, these will
            //       be re-added (=re-activated) from the extra lists if they still exist.
            switch (entry.entrytype) {
              #ifdef SYSYNC_SERVER
              case mapentry_tempidmap:
                if (IS_SERVER)
                  fTempGUIDMap[entry.remoteid]=entry.localid; // tempGUIDs are accessed by remoteID=tempID
                break;
              #endif
              #ifdef SYSYNC_CLIENT
              case mapentry_pendingmap:
                if (IS_CLIENT)
                  fPendingAddMaps[entry.localid]=entry.remoteid;
                break;
              #endif
            }
          } while(true);
          // no more records
          SafeSQLCloseCursor(statement);
          // done
          break;
        } // target record found
      } while(true);
    } // if ok
    if (sta==LOCERR_OK) {
      // determine how to create new IDs for database
      fNextLocalID=-1; // no global ID, must be determined at actual insert
      // for some databases w/o usable ID variable (FMPro), starting point for ids
      // might be some "SELECT MAX()" query, which would be executed here
      if (fConfigP->fDetermineNewIDOnce) {
        uInt32 maxid;
        SQLRETURN res;
        sql=fConfigP->fObtainNewLocalIDSql.c_str();
        DoSQLSubstitutions(sql);
        execSQLStatement(statement,sql,true,"Next-to-be-used localID (determined once in makeAdminReady)",false);
        // - fetch result row
        res=SafeSQLFetch(statement);
        fAgentP->checkStatementError(res,statement);
        // - get value of first field as long
        if (fAgentP->getColumnValueAsULong(statement,1,maxid)) {
          // next ID found
          if (maxid<fConfigP->fMinNextID)
            maxid=fConfigP->fMinNextID; // don't use value smaller than defined minimum
        }
        else {
          // no max value found, start at MinNextID
          maxid=fConfigP->fMinNextID;
        }
        SafeSQLCloseCursor(statement);
        // now assign
        fNextLocalID=maxid;
        #ifdef SYDEBUG
        PDEBUGPRINTFX(DBG_ADMIN,("apiLoadAdminData: next local ID is %ld",fNextLocalID));
        #endif
      }
    } // if ok
    // release the statement handle
    SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
  }
  catch (...) {
    // release the statement handle
    SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
    throw;
  }
  PDEBUGPRINTFX(DBG_ADMIN,("apiLoadAdminData: Number of map entries read = %ld",fMapTable.size()));
  return sta;
  #endif // HAS_SQL_ADMIN
} // TODBCApiDS::apiLoadAdminData


/// @brief Save admin data to ODBC database
/// @param[in] aSessionFinished if true, this is a end-of-session save (and not only a suspend save)
///   Must save to the target record addressed at LoadAdminData() by the triple (aDeviceID,aDatabaseID,aRemoteDBID)
///   Implementation must save the following items:
///   - fLastRemoteAnchor = anchor string used by remote party for this session (and saved to DB then)
///   - fPreviousSyncTime = anchor (beginning of session) timestamp of this session.
///   - fPreviousToRemoteSyncCmpRef = Reference time to determine items modified since last time sending data to remote
///   - fPreviousToRemoteSyncIdentifier = string identifying last session that sent data to remote (needs only be saved
///                         if derived datastore cannot work with timestamps and has its own identifier).
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
localstatus TODBCApiDS::apiSaveAdminData(bool aSessionFinished, bool aSuccessful)
{
  #ifndef HAS_SQL_ADMIN
  return 510; // must use plugin, no ODBC
  #else
  SQLRETURN res;
  localstatus sta=LOCERR_OK;
  try {
    // if data is not handled by ODBC, we might have no write statement here, so open one
    if (fODBCWriteStatement==SQL_NULL_HANDLE)
      fODBCWriteStatement = fAgentP->newStatementHandle(getODBCConnectionHandle());
    // now save target head record admin data
    sta=updateSyncTarget(fODBCWriteStatement,aSessionFinished && aSuccessful);
    if (sta==LOCERR_OK) {
      // then apply Map modifications (target detail records)
      sta=updateODBCMap(fODBCWriteStatement,aSessionFinished && aSuccessful);
    }
    // dispose of Write statement handle
    if (fODBCWriteStatement!=SQL_NULL_HANDLE && aSessionFinished) {
      DEBUGPRINTFX(DBG_DBAPI,("SaveAdminData: End of session - Freeing Write statement handle..."));
      res=SafeSQLFreeHandle(SQL_HANDLE_STMT,fODBCWriteStatement);
      fODBCWriteStatement=SQL_NULL_HANDLE;
      checkConnectionError(res);
    }
    // commit transaction
    if (fODBCConnectionHandle!=SQL_NULL_HANDLE) {
      // commit DB transaction
      DEBUGPRINTFX(DBG_DBAPI,("SaveAdminData(commit): Committing DB transaction..."));
      res=SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_COMMIT);
      checkConnectionError(res);
    }
  }
  catch (exception &e) {
    PDEBUGPRINTFX(DBG_ERROR,("******** SaveAdminData exception: %s",e.what()));
    sta=510; // failed, DB error
  }
  // return status
  return sta;
  #endif // HAS_SQL_ADMIN
} // TODBCApiDS::apiSaveAdminData


// - end DB data write sequence (but not yet admin data)
localstatus TODBCApiDS::apiEndDataWrite(string &aThisSyncIdentifier)
{
  // we do not have a separate sync identifier
  aThisSyncIdentifier.erase();
  // make sure we commit the transaction here in case admin data is not in ODBC
  #ifdef ODBCAPI_SUPPORT
  try {
    SQLRETURN res;
    if (fODBCWriteStatement!=SQL_NULL_HANDLE) {
      DEBUGPRINTFX(DBG_DBAPI,("EndDBDataWrite: we have no ODBC admin data, freeing Write statement handle now..."));
      res=SafeSQLFreeHandle(SQL_HANDLE_STMT,fODBCWriteStatement);
      fODBCWriteStatement=SQL_NULL_HANDLE;
      checkConnectionError(res);
    }
    // commit DB transaction
    if (fODBCConnectionHandle!=SQL_NULL_HANDLE) {
      DEBUGPRINTFX(DBG_DBAPI,("EndDBDataWrite: ...and commit DB transaction"));
      res=SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_COMMIT);
      checkConnectionError(res);
    }
  }
  catch (exception &e) {
    PDEBUGPRINTFX(DBG_ERROR,("******** EndDBDataWrite exception: %s",e.what()));
    return 510;
  }
  #endif // ODBCAPI_SUPPORT
  return LOCERR_OK;
} // TODBCApiDS::apiEndDataWrite


/* end of TODBCApiDS implementation */


#ifdef STREAMFIELD_SUPPORT

// TODBCFieldProxy
// ===============

TODBCFieldProxy::TODBCFieldProxy(
  TODBCApiDS *aODBCdsP,
  TODBCFieldMapItem *aFieldMapP,
  const char *aMasterKey,
  const char *aDetailKey
)
{
  // save values
  fODBCdsP = aODBCdsP;
  fFieldMapP = aFieldMapP;
  fMasterKey = aMasterKey;
  fDetailKey = aDetailKey;
  fFetched = false;
  fValue.erase();
} // TODBCFieldProxy::TODBCFieldProxy


TODBCFieldProxy::~TODBCFieldProxy()
{
  // nop at this time
} // TODBCFieldProxy::~TODBCFieldProxy


// fetch BLOB from DPAPI
void TODBCFieldProxy::fetchBlob(void)
{
  if (!fFetched) {
    // if do not have anything yet and need something, read it now
    string sql = fFieldMapP->fReadBlobSQL;
    // the only possible substitutions are %k and %K
    StringSubst(sql,"%k",fMasterKey,2); // the localID of the entire record
    StringSubst(sql,"%K",fDetailKey,2); // the key of this specific array element
    // fetch now
    SQLHSTMT statement = 0;
    #ifdef SQLITE_SUPPORT
    if (!fODBCdsP->fUseSQLite)
    #endif
    {
      #ifdef ODBCAPI_SUPPORT
      statement=fODBCdsP->fAgentP->newStatementHandle(fODBCdsP->getODBCConnectionHandle());
      #endif
    }
    try {
      fODBCdsP->prepareSQLStatement(statement, sql.c_str(), true, "Proxy Field Fetch");
      fODBCdsP->execSQLStatement(statement, sql, true, NULL, true);
      if (!fODBCdsP->fetchNextRow(statement, true)) {
        // no data
        fValue.erase();
      }
      else {
        // get data
        #ifdef SQLITE_SUPPORT
        if (fODBCdsP->fUseSQLite) {
          size_t siz = sqlite3_column_bytes(fODBCdsP->fSQLiteStmtP,0);
          if (siz>0) {
            if (fFieldMapP->dbfieldtype==dbft_blob)
              fValue.assign((cAppCharP)sqlite3_column_blob(fODBCdsP->fSQLiteStmtP,0),siz);
            else
              appendStringAsUTF8((const char *)sqlite3_column_text(fODBCdsP->fSQLiteStmtP,0), fValue, fODBCdsP->fConfigP->sqlPrepCharSet(), lem_cstr); // Convert to app-charset (UTF8) and C-type lineends
          }
          else {
            fValue.erase();
          }
        }
        else
        #endif
        {
          #ifdef ODBCAPI_SUPPORT
          // pass in actual charset (including utf16 - getColumnValueAsString handles UTF16 case internally)
          fODBCdsP->fAgentP->getColumnValueAsString(statement,1,fValue,fODBCdsP->fConfigP->fDataCharSet,fFieldMapP->dbfieldtype==dbft_blob);
          #endif
        }
      }
      // fetched now
      fFetched=true;
      // done
      fODBCdsP->finalizeSQLStatement(statement,true);
      // release the statement handle
      #ifdef SQLITE_SUPPORT
      if (!fODBCdsP->fUseSQLite)
      #endif
      {
        #ifdef ODBCAPI_SUPPORT
        SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
        #endif
      }
    }
    catch (exception &e) {
      // release the statement handle
      #ifdef SQLITE_SUPPORT
      if (!fODBCdsP->fUseSQLite)
      #endif
      {
        #ifdef ODBCAPI_SUPPORT
        SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
        #endif
      }
      throw;
    }
  }
} // TODBCFieldProxy::fetchBlob


// returns size of entire blob
size_t TODBCFieldProxy::getBlobSize(TStringField *aFieldP)
{
  fetchBlob();
  return fValue.size();
} // TODBCFieldProxy::getBlobSize


// read from Blob from specified stream position and update stream pos
size_t TODBCFieldProxy::readBlobStream(TStringField *aFieldP, size_t &aPos, void *aBuffer, size_t aMaxBytes)
{
  if (!fFetched) {
    // we need to read the body
    fetchBlob();
  }
  // now copy from our value
  if (aPos>fValue.size()) return 0;
  if (aPos+aMaxBytes>fValue.size()) aMaxBytes=fValue.size()-aPos;
  if (aMaxBytes==0) return 0;
  // copy data from ODBC answer buffer to caller's buffer
  memcpy(aBuffer,fValue.c_str()+aPos,aMaxBytes);
  aPos+= aMaxBytes;
  return aMaxBytes; // return number of bytes actually read
} // TODBCFieldProxy::readBlobStream

#endif // STREAMFIELD_SUPPORT


} // namespace

#endif // SQL_SUPPORT

// eof
