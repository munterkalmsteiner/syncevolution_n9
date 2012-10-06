/**
 *  @File     binfileimplds.cpp
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TBinfileImplDS
 *    Represents a client datastore implementation which has target management
 *    (and optionally change log) based on binary files
 *
 *    Copyright (c) 2003-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-09-30 : luz : created from TBinfileImplDS
 */
/*
 */

// includes
#include "prefix_file.h"
#include "sysync.h"
#include "binfileimplclient.h"
#include "binfileimplds.h"
#include <cstddef>

#if defined(BINFILE_ALWAYS_ACTIVE) && defined(SYSYNC_SERVER)
  #error "BINFILE_ALWAYS_ACTIVE is not compatible with server-enabled builds"
#endif


namespace sysync {


#ifdef SCRIPT_SUPPORT

// Script Functions
// ================


class TBFDSfuncs {
public:

  static TItemField *fieldFromStructField(cAppCharP aFieldName, const TStructFieldInfo *aStructFieldInfos, sInt32 aNumFields, uInt8P aStructAddr, TScriptContext *aFuncContextP)
  {
    TItemField *fldP = NULL;
    // search for name
    for (sInt32 i=0; i<aNumFields; i++) {
      const TStructFieldInfo *info = &aStructFieldInfos[i];
      if (strucmp(info->valName, aFieldName)==0 && info->valSiz>0) {
        // found field
        if (info->valType==VALTYPE_TEXT) {
          // text
          fldP = newItemField(fty_string, aFuncContextP->getSessionZones());
          fldP->setAsString((cAppCharP)(aStructAddr+info->fieldOffs));
          return fldP;
        }
        else if (info->valType==VALTYPE_TIME64) {
          // timestamp
          fldP = newItemField(fty_timestamp, aFuncContextP->getSessionZones());
          static_cast<TTimestampField *>(fldP)->setTimestampAndContext(
            *((lineartime_t *)(aStructAddr+info->fieldOffs)),
            TCTX_UTC // internal timestamps are UTC
          );
          return fldP;
        }
        else {
          // all other types are treated as integers
          fieldinteger_t fint = 0;
          switch(info->valSiz) {
            case 1 : fint = *((uInt8 *)(aStructAddr+info->fieldOffs)); break;
            case 2 : fint = *((uInt16 *)(aStructAddr+info->fieldOffs)); break;
            case 4 : fint = *((uInt32 *)(aStructAddr+info->fieldOffs)); break;
            case 8 : fint = *((uInt64 *)(aStructAddr+info->fieldOffs)); break;
          }
          fldP = newItemField(fty_integer, aFuncContextP->getSessionZones());
          fldP->setAsInteger(fint);
          return fldP;
        }
      }
    }
    // no such field, return an unassigned field
    fldP=newItemField(fty_none, aFuncContextP->getSessionZones());
    fldP->unAssign(); // make it (already is...) unassigned
    return fldP;
  } // fieldFromStructField


  // variant PROFILESETTING(string settingsfieldname)
  // returns data from profile settings (like /profiles/n does)
  static void func_ProfileSetting(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string varname;
    TBinfileImplDS *dsP = static_cast<TBinfileImplDS *>(aFuncContextP->getCallerContext());
    // get name
    aFuncContextP->getLocalVar(0)->getAsString(varname);
    // get value
    aTermP = fieldFromStructField(
      varname.c_str(),
      ProfileFieldInfos, numProfileFieldInfos,
      (uInt8P)&(static_cast<TBinfileImplClient *>(dsP->getSession())->fProfile),
      aFuncContextP
    );
  }; // func_ProfileSetting


  // variant TARGETSETTING(string settingsfieldname)
  // returns data from target settings (like /profiles/n/targets/dbid/settingsfieldname does)
  static void func_TargetSetting(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string varname;
    TBinfileImplDS *dsP = static_cast<TBinfileImplDS *>(aFuncContextP->getCallerContext());
    // get name
    aFuncContextP->getLocalVar(0)->getAsString(varname);
    // get value
    aTermP = fieldFromStructField(
      varname.c_str(),
      TargetFieldInfos, numTargetFieldInfos,
      (uInt8P)&(dsP->fTarget),
      aFuncContextP
    );
  }; // func_TargetSetting


}; // TBFDSfuncs


const uInt8 param_OneStr[] = { VAL(fty_string) };

const TBuiltInFuncDef BinfileDBFuncDefs[] = {
  { "TARGETSETTING", TBFDSfuncs::func_TargetSetting, fty_none, 1, param_OneStr },
  { "PROFILESETTING", TBFDSfuncs::func_ProfileSetting, fty_none, 1, param_OneStr },
};


// chain to general ClientDB functions
static void *BinfileClientDBChainFunc(void *&aCtx)
{
  // caller context remains unchanged
  // -> no change needed
  // next table is general ClientDB func table
  return (void *)&ClientDBFuncTable;
} // BinfileClientDBChainFunc


// function table for client-only script functions
const TFuncTable BinfileClientDBFuncTable = {
  sizeof(BinfileDBFuncDefs) / sizeof(TBuiltInFuncDef), // size of table
  BinfileDBFuncDefs, // table pointer
  BinfileClientDBChainFunc // chain to general agent funcs.
};


#endif // SCRIPT_SUPPORT


// Config
// ======

TBinfileDSConfig::TBinfileDSConfig(const char* aName, TConfigElement *aParentElement) :
  TLocalDSConfig(aName,aParentElement)
{
  // nop so far
  clear();
} // TBinfileDSConfig::TBinfileDSConfig


TBinfileDSConfig::~TBinfileDSConfig()
{
  // nop so far
} // TBinfileDSConfig::~TBinfileDSConfig


// init defaults
void TBinfileDSConfig::clear(void)
{
  // Only active in clients by default
  fBinfileDSActive = IS_CLIENT;
  // change detection by CRC is enabled by default only for builds that CAN'T have DB-side detection
  #ifdef CHANGEDETECTION_AVAILABLE
  fCRCChangeDetection = false; // normally, DB can report changes. CRC checking can be enabled as an option
  fCRCPseudoChangeDetection = false; // normally, DB reports changes correctly. Verifying changes can be enabled as an option
  #endif
  // init defaults
  fLocalDBPath.erase();
  fDSAvailFlag=0;
  // set default according to former build-time target setting
  #ifdef SYNCTIME_IS_ENDOFSESSION
  fCmpRefTimeStampAtEnd = true;
  #else
  fCmpRefTimeStampAtEnd = false;
  #endif
  // clear inherited
  inherited::clear();
} // TBinfileDSConfig::clear


#ifndef HARDCODED_CONFIG

// config element parsing
bool TBinfileDSConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"dbpath")==0)
    expectString(fLocalDBPath);
  else if (strucmp(aElementName,"dsavailflag")==0)
    expectUInt16(fDSAvailFlag);
  else if (strucmp(aElementName,"cmptimestampatend")==0)
    expectBool(fCmpRefTimeStampAtEnd);
  else if (strucmp(aElementName,"binfiledsactive")==0)
    expectBool(fBinfileDSActive);
  #ifdef CHANGEDETECTION_AVAILABLE
  else if (strucmp(aElementName,"crcchangedetection")==0)
    expectBool(fCRCChangeDetection);
  else if (strucmp(aElementName,"pseudochangedetection")==0)
    expectBool(fCRCPseudoChangeDetection);
  #endif
  // - none known here
  else
    return TLocalDSConfig::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TBinfileDSConfig::localStartElement

#endif

// resolve
void TBinfileDSConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    // check for required settings
    #ifdef CHANGEDETECTION_AVAILABLE
    // - pseudochange detection would interfere with regular CRC based change detection and is also completely useless then
    if (fCRCChangeDetection) fCRCPseudoChangeDetection = false; // just switch it off, makes no sense
    #endif
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TBinfileDSConfig::localResolve



// checks if datastore is available for given profile. If aProfileP is NULL,
// general availability is checked (profile might allow more or less)
bool TBinfileDSConfig::isAvailable(TBinfileDBSyncProfile *aProfileP)
{
  #ifndef DSFLAGS_ALWAYS_AVAILABLE
  // all datastores are always available
  return true;
  #else
  // check if available
  // - %%%later: check if license allows using this datastore
  // %%%
  // - if it's one of the always available datastores, always return true
  if (fDSAvailFlag & DSFLAGS_ALWAYS_AVAILABLE) return true;
  // - check if it's enabled in the profile
  if (aProfileP==NULL) return false; // no profile
  return (aProfileP->dsAvailFlags & fDSAvailFlag)!=0;
  #endif
} // TBinfileDSConfig::isAvailable



#ifndef DEFAULT_SYNCMODE
#define DEFAULT_SYNCMODE smo_twoway
#endif

// init default target for datastore
void TBinfileDSConfig::initTarget(
  TBinfileDBSyncTarget &aTarget,
  uInt32 aRemotepartyID,
  const char *aRemoteName, // defaults to name of datastore
  bool aEnabled // enabled?
)
{
  //set to zeros to avoid memory warnings
  memset(&aTarget, 0, sizeof(aTarget));
  // link to the profile
  aTarget.remotepartyID=aRemotepartyID;
  // enabled or not?
  aTarget.enabled=aEnabled;
  // force slow sync (slow sync is used anyway, but shows this to user)
  aTarget.forceSlowSync=true;
  // sync mode
  aTarget.syncmode=DEFAULT_SYNCMODE;
  // reset all options
  aTarget.limit1=0;
  aTarget.limit2=0;
  aTarget.extras=0;
  // database identification
  aTarget.localDBTypeID=fLocalDBTypeID; // what target database
  AssignCString(aTarget.localDBPath,fLocalDBPath.c_str(),localDBpathMaxLen);
  #if defined(DESKTOP_CLIENT) || TARGETS_DB_VERSION>4
  AssignCString(aTarget.localContainerName,NULL,localDBpathMaxLen);
  #endif
  // link to configuration is by datastore name
  AssignCString(aTarget.dbname,getName(),dbnamelen);
  // default name for remote datastore is same as local one
  if (!aRemoteName) aRemoteName=getName();
  AssignCString(aTarget.remoteDBpath,aRemoteName,remoteDBpathMaxLen);
  // - init new target record's variables (will be updated after sync)
  aTarget.lastSync=0; // no last sync date yet
  aTarget.lastChangeCheck=0;
  aTarget.lastSuspendModCount=0; /// @note: before DS 1.2 enginem this was used as "lastModCount"
  aTarget.resumeAlertCode=0; // no resume yet
  aTarget.lastTwoWayModCount=0;
  aTarget.remoteAnchor[0]=0; // no anchor yet
  // Version 6 and later
  #if TARGETS_DB_VERSION>5
  aTarget.dummyIdentifier1[0]=0;
  aTarget.dummyIdentifier2[0]=0;
  AssignCString(aTarget.remoteDBdispName, getName(), dispNameMaxLen); // default to local name
  aTarget.filterCapDesc[0]=0;
  aTarget.remoteFilters[0]=0;
  aTarget.localFilters[0]=0;
  #endif
} // initTarget




/*
 * Implementation of TBinfileImplDS
 */

TBinfileImplDS::TBinfileImplDS(
  TBinfileDSConfig *aConfigP,
  sysync::TSyncSession *aSessionP,
  const char *aName,
  uInt32 aCommonSyncCapMask
) :
  TStdLogicDS(aConfigP, aSessionP, aName, aCommonSyncCapMask)
{
  // no changelog loaded yet (needed here because InternalResetDataStore will test it)
  fLoadedChangeLog=NULL;
  fLoadedChangeLogEntries=0; // just to make sure
  fHasPendingChanges=false;
  fPreviousToRemoteModCount=0;
  fPreviousSuspendModCount=0;
  fCurrentModCount=0;
  // now do internal reset
  InternalResetDataStore();
  // save config pointer
  fConfigP=aConfigP;
  // no target loaded yet
  fTargetIndex=-1;
} // TBinfileImplDS::TBinfileImplDS



void TBinfileImplDS::dsResetDataStore(void)
{
  // Do my normal internal reset
  InternalResetDataStore();
  // Let ancestor initialize
  inherited::dsResetDataStore();
  if (binfileDSActive()) {
    // And now force MaxGUIDSize to what this datastore can handle
    fMaxGUIDSize=BINFILE_MAXGUIDSIZE;
  }
} // TBinfileImplDS::dsResetDataStore


void TBinfileImplDS::InternalResetDataStore(void)
{
  // forget preflight
  fPreflighted=false;
  // forget loaded changelog
  forgetChangeLog();
  // no pending changes known yet
  fHasPendingChanges=0;
  // unknown number of changes
  fNumberOfLocalChanges=-1;
} // TBinfileImplDS::InternalResetDataStore


TBinfileImplDS::~TBinfileImplDS()
{
  InternalResetDataStore();
} // TBinfileImplDS::~TBinfileImplDS


// called when message processing (and probably thread) ends
// (can be used to finalize things that cannot continue until next request
// such as code that cannot be called from different threads)
void TBinfileImplDS::dsEndOfMessage(void)
{
  // %%% save between messages in debug
  #ifdef SYDEBUG
  //endWrite();
  #endif
  // let ancestor do things
  inherited::dsEndOfMessage();
} // TBinfileImplDS::dsEndOfMessage


/// inform logic of coming state change
localstatus TBinfileImplDS::dsBeforeStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState)
{
  return inherited::dsBeforeStateChange(aOldState, aNewState);
} // TBinfileImplDS::dsBeforeStateChange


/// inform logic of happened state change
localstatus TBinfileImplDS::dsAfterStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState)
{
  if (aNewState == dssta_completed) {
    // free resources which are not valid when restarting,
    // in particular the change log
    InternalResetDataStore();
  }
  return inherited::dsAfterStateChange(aOldState, aNewState);
} // TBinfileImplDS::dsAfterStateChange


// - init Sync Parameters for datastore
//   Derivates might override this to pre-process and modify parameters
//   (such as adding client settings as CGI to remoteDBPath)
bool TBinfileImplDS::dsSetClientSyncParams(
  TSyncModes aSyncMode,
  bool aSlowSync,
  const char *aRemoteDBPath,
  const char *aDBUser,
  const char *aDBPassword,
  const char *aLocalPathExtension,
  const char *aRecordFilterQuery,
  bool aFilterInclusive
)
{
  if (binfileDSActive()) {
    #ifdef AUTOSYNC_SUPPORT
    string s;
    bool cgi=false;
    // add autosync options if this is an autosync
    if (fConfigP->fAutosyncAlerted || fConfigP->fAutosyncForced) {
      if (!fConfigP->fAutosyncPathCGI.empty()) {
        s=aRemoteDBPath;
        cgi = s.find('?')!=string::npos;
        if (!cgi) { s+='?'; cgi=true; }
        s += fConfigP->fAutosyncPathCGI;
        aRemoteDBPath=s.c_str();
      }
    }
    #endif
  }
  return inherited::dsSetClientSyncParams(aSyncMode,aSlowSync,aRemoteDBPath,aDBUser,aDBPassword,aLocalPathExtension,aRecordFilterQuery,aFilterInclusive);
} // TBinfileImplDS::dsSetClientSyncParams



// BinFile DB implementation specific routines
// ===========================================


// clean up change log
// - removes chgl_deleted entries that are older or
//   same age as indicated by aOldestSyncModCount)
// - finalizes localids
localstatus TBinfileImplDS::changeLogPostflight(uInt32 aOldestSyncModCount)
{
  // review all logentries
  TChangeLogEntry logentry;
  uInt32 logindex=0;
  while (true) {
    if (fChangeLog.readRecord(logindex,&logentry)!=BFE_OK) break; // seen all
    if (
      (logentry.flags & chgl_deleted) &&
      (logentry.modcount<=aOldestSyncModCount)
    ) {
      // all peers (in all profiles related to this changelog, which is only one with the separated changelogs!)
      // have seen this delete already, so this one should be deleted
      // - delete the record, another one will appear at this index
      fChangeLog.deleteRecord(logindex);
    }
    else {
      // no delete, finalize localid (possible only with string localIDs)
      #ifndef NUMERIC_LOCALIDS
      localid_out_t locID = logentry.dbrecordid;
      if (dsFinalizeLocalID(locID)) {
        // update log entry
        ASSIGN_LOCALID_TO_FLD(logentry.dbrecordid,LOCALID_OUT_TO_IN(locID));
        fChangeLog.updateRecord(logindex, &logentry);
      }
      #endif
      // no delete, advance index
      logindex++;
    }
  }
  return LOCERR_OK;
} // TBinfileImplDS::changeLogPostflight


// zap changelog. Should be called if datastore as a entiety was replaced
// by another datatstore (or created new)
void TBinfileImplDS::zapChangeLog(void)
{
  // forget cached changelog if any
  forgetChangeLog();
  // make sure changelog file is open
  if (openChangeLog()) {
    // kill all entries
    fChangeLog.truncate(0);
    // reset modcount
    fChgLogHeader.modcount = 0;
    fChgLogHeader.lastChangeCheckIdentifier[0] = 0;
    fChgLogHeader.lastChangeCheck = noLinearTime;
    // make sure header is written
    fChangeLog.setExtraHeaderDirty();
    fChangeLog.flushHeader();
  }
  PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("zapChangeLog: erased changelog"));
} // TBinfileImplDS::zapChangeLog




// update change log record contents
static uInt32 changelogUpdateFunc(uInt32 aOldVersion, uInt32 aNewVersion, void *aOldRecordData, void *aNewRecordData, uInt32 aOldSize)
{
  if (aOldVersion<LOWEST_CHANGELOG_DB_VERSION || aOldVersion>CHANGELOG_DB_VERSION) return 0; // unknown old version or too new version, cannot update
  if (aNewVersion!=CHANGELOG_DB_VERSION) return 0; // cannot update to other version than current
  // create default values for profile
  if (aOldRecordData && aNewRecordData) {
    // update records
    TChangeLogEntry *chglogEntryP = (TChangeLogEntry *)aNewRecordData;
    // check for special case that old record is V2 or V3 and was compiled without CHANGEDETECTION_AVAILABLE
    uInt16 crc = 0;
    if ((aOldVersion==2 || aOldVersion==3) && aOldSize!=offsetof(TChangeLogEntry,modcount_created)) {
      // this means that the records were compiled without CHANGEDETECTION_AVAILABLE and had a
      // dataCRC field between modcount and flags
      //#if defined(WINCE) ||
      #if defined(__PALM_OS__)
      #error "This code must be verified to check if upgrade works as intended with targets that did not have CHANGEDETECTION_AVAILABLE"
      #endif
      // - offset of the CRC field we already have
      int o = offsetof(TChangeLogEntry,flags);
      // - copy up to the CRC which is already there
      memcpy(aNewRecordData,aOldRecordData,o);
      // - get the CRC which is already there to init the new V5 DB field below
      crc = *((uInt16 *)((uInt8 *)aOldRecordData+o));
      // - also copy the flags
      chglogEntryP->flags = *((uInt8 *)((uInt8 *)aOldRecordData+o+sizeof(uInt16))); // next after dataCRC
      chglogEntryP->dummy = *((uInt8 *)((uInt8 *)aOldRecordData+o+sizeof(uInt16)+sizeof(uInt8))); // next after flags
    }
    else {
      // copy old data - beginning of record is identical
      memcpy(aNewRecordData,aOldRecordData,aOldSize);
    }
    // now initialize fields that old version didn't have
    if (aOldVersion<4) {
      // init new version 4 fields
      chglogEntryP->modcount_created = 0; // assume created before changelogging history started
    }
    if (aOldVersion<5) {
      // init new V5 dataCRC; if we are updating from V2 or V3 (which is handled above) we'll assign the previous CRC here, 0 otherwise
      chglogEntryP->dataCRC = crc;
    }
  }
  else if (aNewRecordData) {
    // update extra header
    TChangeLogHeader *extraHeaderP = (TChangeLogHeader *)aNewRecordData;
    if (aOldVersion<3) {
      // header has got new fields between v2 and v3
      // Note: these will be updated with data from target fields after actually opening the changelog
      //       Just init them now
      extraHeaderP->lastChangeCheckIdentifier[0] = 0;
      extraHeaderP->lastChangeCheck = noLinearTime;
    }
    // updated header ok
    return sizeof(TChangeLogHeader);
  }
  // updated ok (or updateable ok if no data pointers provided)
  // - return size of new record
  return sizeof(TChangeLogEntry);
} // changelogUpdateFunc


// returns true if we had a valid changelog
bool TBinfileImplDS::openChangeLog(void)
{
  if (fChangeLog.isOpen()) return true; // was already open
  // open and possibly migrate the changelog
  TBinfileImplClient *bfcP = static_cast<TBinfileImplClient *>(fSessionP);
  TBinfileClientConfig *bfcfgP = static_cast<TBinfileClientConfig *>(fSessionP->getSessionConfig());
  // open changelog. Name is datastore name with _clg.bfi suffix
  string changelogname = bfcfgP->relatedDBNameBase(getName(), bfcP->fRemotepartyID);
  changelogname += CHANGELOG_DB_SUFFIX;
  fChangeLog.setFileInfo(changelogname.c_str(),CHANGELOG_DB_VERSION,CHANGELOG_DB_ID,sizeof(TChangeLogEntry));
  if (fChangeLog.open(sizeof(TChangeLogHeader),&fChgLogHeader,changelogUpdateFunc)!=BFE_OK) {
    // can't open changelog - check if we might need need migration from united changelogs to separated
    if (bfcfgP->fSeparateChangelogs) {
      // check if we have the old united changelog and migrate everything if so
      string unitedchangelogname = bfcfgP->relatedDBNameBase(getName(), -1); // united name
      unitedchangelogname += CHANGELOG_DB_SUFFIX;
      fChangeLog.setFileInfo(unitedchangelogname.c_str(),CHANGELOG_DB_VERSION,CHANGELOG_DB_ID,sizeof(TChangeLogEntry));
      if (fChangeLog.open(sizeof(TChangeLogHeader),&fChgLogHeader,changelogUpdateFunc)==BFE_OK) {
        // the old unified changelog is there - we need to do a full migration now
        // - close it
        fChangeLog.close();
        // - perform migration for this DB
        PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("openChangeLog: auto-migrating from common to profile-separated changelog for this datastore"));
        bfcfgP->separateChangeLogsAndRelated(getName());
        // - recursively call myself, now the profile specific log should be there
        return openChangeLog();
      }
      // we're not migrating (we could not open the changelog because this is a new profile!)
      // - restore the per-profile name for creation of new log below
      fChangeLog.setFileInfo(changelogname.c_str(),CHANGELOG_DB_VERSION,CHANGELOG_DB_ID,sizeof(TChangeLogEntry));
    }
    // create new change log or overwrite incompatible one
    // - init changelog header fields
    fChgLogHeader.modcount=0;
    //set all bytes to zero to avoid memory warnings
    memset(fChgLogHeader.lastChangeCheckIdentifier, 0, changeIndentifierMaxLen);
    fChgLogHeader.lastChangeCheck = noLinearTime;
    // - create new changelog
    fChangeLog.create(sizeof(TChangeLogEntry),sizeof(TChangeLogHeader),&fChgLogHeader,true);
    PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("openChangeLog: changelog did not exist (or bad version) -> created new"));
    return false; // changelog is new, so we need a slow sync
  }
  else {
    // check if opening was an upgrade from version 2
    if (fChangeLog.getFoundVersion()<3) {
      // version 3 has introduced saving the last-check date/identifiers in the changelog header,
      // before that the identifiers used where (wrongly) those of the target. So copy
      // the target info to the new header now. This gives perfect results only for single
      // profile use, but existing products before update to v3 were single profile so we can
      // safely assume this will give a smooth transition (without re-send-everything effects).
      #if TARGETS_DB_VERSION>5
      // - dummyIdentifier1 is the former lastSyncIdentifier and contains the token from the
      //   last change check towards the DB (in case sync identifiers are available at all)
      AssignCString(fChgLogHeader.lastChangeCheckIdentifier,fTarget.dummyIdentifier1,changeIndentifierMaxLen);
      #endif
      // - lastChangeCheck is the former lastTwoWaySync and contains the timestamp of the last change check towards the DB
      fChgLogHeader.lastChangeCheck = fTarget.lastChangeCheck;
      // - make sure it gets written back
      fChangeLog.setExtraHeaderDirty();
      fChangeLog.flushHeader();
      PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("openChangeLog: upgraded changelog from V2 to V4 (new header, new modcount_created)"));
    }
  }
  return true; // changelog already existed, so we assume it's up-to-date
} // TBinfileImplDS::openChangeLog


// returns true if we had a valid pending map file
bool TBinfileImplDS::openPendingMaps(void)
{
  if (fPendingMaps.isOpen()) return true; // was already open
  openChangeLog(); // must be open, because it checks for migration of pendingmaps as well
  string pendingmapsname = static_cast<TBinfileClientConfig *>(fSessionP->getSessionConfig())->relatedDBNameBase(getName(), fTarget.remotepartyID);
  pendingmapsname += PENDINGMAP_DB_SUFFIX;
  fPendingMaps.setFileInfo(pendingmapsname.c_str(),PENDINGMAP_DB_VERSION,PENDINGMAP_DB_ID, sizeof(TPendingMapEntry));
  PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,("openPendingMaps: file name='%s'",pendingmapsname.c_str()));
  if (fPendingMaps.open(sizeof(TPendingMapHeader),&fPendingMapHeader)!=BFE_OK) {
    // create new pending maps or overwrite incompatible one
    // - bind to remote party (in case of combined changelog, we only have a single pendingmap file, and it is valid only for ONE remote party)
    fPendingMapHeader.remotepartyID = static_cast<TBinfileImplClient *>(fSessionP)->fRemotepartyID;
    // - create new changelog
    fPendingMaps.create(sizeof(TPendingMapEntry),sizeof(TPendingMapHeader),&fPendingMapHeader,true);
    PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("openPendingMaps: pending maps did not exist (or bad version) -> created new"));
    return false; // pending map file is new, so we have no already pending maps
  }
  return true; // pending map file already existed, so we assume it's up-to-date
} // TBinfileImplDS::openPendingMaps


// get reference time to be used as reference date for this sync's modifications
// Note: this time marks the beginning of the sync session. If possible
//       datastore should mark all modifications with this time, even if
//       they occur later during the sync session. Only if setting the
//       modified date explicitly is not possible, setting fCmpRefTimeStampAtEnd
//       (default = defined(SYNCTIME_IS_ENDOFSESSION)) can be used to make sure
//       that modifications made during the session are not detected as changed
//       for the next session.
lineartime_t TBinfileImplDS::getThisSyncModRefTime(void)
{
  return
    #ifdef SYNCTIME_IS_LOCALTIME
    makeLocalTimestamp(fCurrentSyncTime); // make local time for comparison
    #else
    fCurrentSyncTime;
    #endif
} // TBinfileImplDS::getThisSyncModRefTime


// get reference time to be used in case datastore implementation wants to compare
// with the time of last sync (last two-way sync, that is!)
lineartime_t TBinfileImplDS::getLastSyncModRefTime(void)
{
  return
    #ifdef SYNCTIME_IS_LOCALTIME
    makeLocalTimestamp(fPreviousSyncTime); // make local time for comparison
    #else
    fPreviousSyncTime;
    #endif
} // TBinfileImplDS::getLastSyncModRefTime



// update change log using CRC checksum comparison before syncing
// Note: Don't call before types are ok (we need TSyncItems)
localstatus TBinfileImplDS::changeLogPreflight(bool &aValidChangelog)
{
  localstatus sta = LOCERR_OK;
  aValidChangelog = false;
  bferr err = BFE_OK;
  TChangeLogEntry *existingentries = NULL; // none yet
  uInt32 numexistinglogentries;
  bool foundone;
  uInt32 seen = 0;
  uInt32 logindex;
  TChangeLogEntry newentry;
  //set zeros to avoid memory warnings
  memset(&newentry, 0, sizeof(newentry));
  TSyncItem *itemP = NULL;
  localid_out_t itemLocalID;
  uInt16 dataCRC = 0;
  bool itemIsModified;

  // just in case: make sure we don't have a changelog loaded here
  forgetChangeLog();
  // no changes detected yet
  fNumberOfLocalChanges=0;
  // make sure we have the change log DB open
  openChangeLog();
  // - get saved modcount for this database and increment for this preflight
  fChgLogHeader.modcount += 1;
  fCurrentModCount = fChgLogHeader.modcount;
  // - update date of last check to start of sync - everything happening during or after this sync is for next session
  //   Note: this is used only if not CRC_CHANGE_DETECTION
  fChgLogHeader.lastChangeCheck = fCurrentSyncTime;
  // change log header is dirty
  fChangeLog.setExtraHeaderDirty();
  PDEBUGBLOCKCOLL("changeLogPreflight");
  #ifdef SYDEBUG
  string lsd;
  StringObjTimestamp(lsd,fPreviousSyncTime);
  PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,(
    "changeLogPreflight: %sfCurrentModCount=%ld, fPreviousToRemoteModCount=%ld, fPreviousSyncTime(UTC)=%s",
    isResuming() ? "RESUMING, " : "",
    (long)fCurrentModCount,
    (long)fPreviousToRemoteModCount,
    lsd.c_str()
  ));
  #endif
  // - save header
  err = fChangeLog.flushHeader();
  if (err!=BFE_OK) goto done;
  // - we don't need the changelog to be updated when all we do is refresh from server
  if (isRefreshOnly()) goto done; // done ok
  // - load entire existing changelog into memory
  numexistinglogentries = fChangeLog.getNumRecords(); // logentries that are already there
  PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("changeLogPreflight: at start, changelog has %ld entries",(long)numexistinglogentries));
  if (numexistinglogentries>0) {
    // - allocate array for all existing entries (use sysync_malloc because e.g. on PalmOS this uses special funcs to allow > 64k)
    existingentries = (TChangeLogEntry *)sysync_malloc(sizeof(TChangeLogEntry)*numexistinglogentries);
    if (!existingentries) { err=BFE_MEMORY; goto done; } // not enough memory
    // - read all entries
    fChangeLog.readRecord(0,existingentries,numexistinglogentries);
    // Mark all not-yet-deleted in the log as delete candidate
    // (those that still exist will be get the candidate mark removed below)
    for (logindex=0;logindex<numexistinglogentries;logindex++) {
      // set as delete candidate if not already marked deleted
      if (!(existingentries[logindex].flags & chgl_deleted))
        existingentries[logindex].flags = existingentries[logindex].flags | chgl_delete_candidate; // mark as delete candidate
    }
  }
  // Now update the changelog using CRC checks
  // loop through entire database
  if (CRC_CHANGE_DETECTION) {
    // we do change detection via CRC comparison
    #ifdef RECORDHASH_FROM_DBAPI
    // - DB can deliver CRC directly
    foundone = getFirstItemCRC(itemLocalID,dataCRC);
    #else
    // - We need to get the item and calculate the CRC here
    foundone = getFirstItem(itemP);
    #endif
  }
  else {
    #ifdef CHANGEDETECTION_AVAILABLE
    // - the DB layer can report changes directly
    foundone = getFirstItemInfo(itemLocalID,itemIsModified);
    #else
    // error, should never happen because without CHANGEDETECTION_AVAILABLE, CRC_CHANGE_DETECTION should be true
    return LOCERR_WRONGUSAGE;
    #endif
  }
  while (foundone) {
    // report event to allow progress display, use existing number as approx for total # of items
    ++seen;
    DB_PROGRESS_EVENT(this,pev_preparing,seen,numexistinglogentries,0);
    // process now
    // - get local ID
    localid_t localid;
    #ifdef RECORDHASH_FROM_DBAPI
    // with or without CRC, we have got the ID into itemLocalID already
    localid=LOCALID_OUT_TO_IN(itemLocalID);
    #else
    if (CRC_CHANGE_DETECTION) {
      // with CRC, we have got the entire item, so we need to get the ID out of that
      STR_TO_LOCALID(itemP->getLocalID(),localid);
    }
    else {
      // without CRC, we have got the ID into itemLocalID already
      localid=LOCALID_OUT_TO_IN(itemLocalID);
    }
    #endif // not RECORDHASH_FROM_DBAPI
    // show item info found in DB
    #ifdef SYDEBUG
    string sl;
    LOCALID_TO_STRING(localid,sl);
    if (CRC_CHANGE_DETECTION) {
      #ifdef RECORDHASH_FROM_DBAPI
      PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("changeLogPreflight: item #%ld : localid=%s, dataCRC=0x%04hX, NOC=%ld ",(long)seen,sl.c_str(),(long)fNumberOfLocalChanges,dataCRC));
      #else
      PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("changeLogPreflight: item #%ld : localid=%s, NOC=%ld ",(long)seen,sl.c_str(),(long)fNumberOfLocalChanges));
      #endif
    }
    else {
      PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,(
        "changeLogPreflight: item #%ld : localid=%s, database says item %s modified",
        (long)seen,sl.c_str(),
        itemIsModified ? "IS" : "is NOT"
      ));
    }
    #endif // SYDEBUG
    // - search for already existing changelog entry for this uniqueID
    //   (prevent searching those that we have created in this preflight)
    bool chgentryexists=false; // none found yet
    TChangeLogEntry *currentEntryP = NULL; // no entry yet
    for (logindex=0; logindex<numexistinglogentries; logindex++) {
      if (LOCALID_EQUAL(existingentries[logindex].dbrecordid,localid)) {
        // found
        chgentryexists = true;
        currentEntryP = &(existingentries[logindex]);
        // - remove the deletion candidate flag if it was set
        if (currentEntryP->flags & chgl_delete_candidate) {
          currentEntryP->flags &= ~chgl_delete_candidate; // remove candidate flag
        }
        // found
        if (CRC_CHANGE_DETECTION) {
          PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
            "- found in changelog at index=%ld, flags=0x%02hX, modcount=%ld, modcount_created=%ld, saved CRC=0x%04hX",
            (long)logindex,
            (uInt16)currentEntryP->flags,
            (long)currentEntryP->modcount,
            (long)currentEntryP->modcount_created,
            currentEntryP->dataCRC
          ));
        }
        else {
          PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
            "- found in changelog at index=%ld, flags=0x%02hX, modcount=%ld, modcount_created=%ld",
            (long)logindex,
            (uInt16)currentEntryP->flags,
            (long)currentEntryP->modcount,
            (long)currentEntryP->modcount_created
          ));
        }
        break;
      }
    }
    // - create new record
    if (!chgentryexists) {
      // set unique ID, init other fields
      ASSIGN_LOCALID_TO_FLD(newentry.dbrecordid,localid);
      // - just init these, will be updated with real values below
      newentry.flags = 0;
      newentry.dataCRC = 0;
      newentry.modcount = 0;
      newentry.modcount_created = 0;
      PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,("- does not yet exist in changelog, created new"));
      // new entry is now current entry
      currentEntryP = &newentry;
    }
    // now check what to do
    if (CRC_CHANGE_DETECTION) {
      #ifndef RECORDHASH_FROM_DBAPI
      // we need a CRC but don't have it precalculated from the DB layer
      dataCRC=itemP->getDataCRC(0,true); // start new CRC, do not include eqm_none fields
      #endif // not RECORDHASH_FROM_DBAPI
    }
    // - check if new or changed
    if (chgentryexists && !(currentEntryP->flags & chgl_deleted)) {
      // entry exists (and is not a deleted one), could be changed
      if (CRC_CHANGE_DETECTION) {
        // - check CRC to calculate itemIsModified
        itemIsModified = existingentries[logindex].dataCRC!=dataCRC;
        if (itemIsModified) {
          PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
            "- item has changed (current CRC=0x%04hX, old CRC=0x%04hX)",
            dataCRC,
            currentEntryP->dataCRC
          ));
          // has changed since last time checked by preflight (but only those! There might be more items
          // changed since last sync or resume, but these ALREADY have a modcount in the changelog that
          // flags them such).
          // So this is the place to reset chgl_modbysync (which marks items changed by a sync and not from outside)
          currentEntryP->flags &= ~chgl_modbysync; // detecting a real change here cancels the mod-by-sync flag set for sync-added/changed entries
          // update CRC and modification count
          currentEntryP->dataCRC=dataCRC;
          currentEntryP->modcount=fCurrentModCount; // update modification count
          // this is a local change for this session
          fNumberOfLocalChanges++; // for suspend: those that detect a change here were modified AFTER last suspend, so always count them

        }
      }
      else {
        // - DB has reported change status into itemIsModified already
        if (itemIsModified) {
          PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,("- item has changed (according to what database says)"));
          // has changed since last time checked by preflight (but only those! There might be more items
          // changed since last sync or resume, but these ALREADY have a modcount in the changelog that
          // flags them such).
          // So this is the place to reset chgl_modbysync (which marks items changed by a sync and not from outside)
          currentEntryP->flags &= ~chgl_modbysync; // detecting a real change here cancels the mod-by-sync flag set for sync-added/changed entries
          // update modification count
          currentEntryP->modcount=fCurrentModCount;
          // this is a local change for this session
          fNumberOfLocalChanges++; // for suspend: those that detect a change here were modified AFTER last suspend, so always count them
        }
      }
      if (!itemIsModified) {
        // no change detected since last preflight (but still, this could be a change to report to the server)
        if (isResuming()) {
          // if resuming - only those count that are marked for resume
          if (currentEntryP->flags & chgl_markedforresume)
            fNumberOfLocalChanges++; // in resumes: only count those that are actually marked for resume
        }
        else {
          // not resuming - all existing ones count if this is a slow sync,
          // otherwise, those modified since last to-remote-sync count as well (although not modified since last preflight!)
          if (fSlowSync || currentEntryP->modcount>fPreviousToRemoteModCount) fNumberOfLocalChanges++;
        }
      }
    }
    else {
      // entry does not exist (or existed as deleted entry) -> means that this record was (maybe: re-)added new
      // (since last preflight, which always means also AFTER last suspend!)
      // - is modified now
      currentEntryP->modcount = fCurrentModCount;
      // - is (re)created now: set the creation modcount, this is used to detect a client-side newly added item
      currentEntryP->modcount_created = fCurrentModCount;
      // - no flags set
      currentEntryP->flags = 0;
      // - update CRC to current value if CRC is in use
      if (CRC_CHANGE_DETECTION) {
        currentEntryP->dataCRC = dataCRC;
      }
      else {
        currentEntryP->dataCRC = 0; // clean it for cosmetic reasons only
      }
      // create if entry is new
      if (!chgentryexists) {
        // this is a new, additional entry (and not a resurrected deleted one)
        fChangeLog.newRecord(currentEntryP);
        PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("- item was newly added (no entry existed in changelog before)"));
      }
      else {
        // the entry itself existed, but was a deleted entry. We're now re-using that one
        PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("- item has re-appeared (i.e. item with same local uniqueID that was once before deleted is now here again)"));
      }
      // this is a local change for this session (even for resume - we need to add newly added ones in resume!)
      fNumberOfLocalChanges++;
    }
    // Next item
    if (CRC_CHANGE_DETECTION) {
      #ifdef RECORDHASH_FROM_DBAPI
      foundone = getNextItemCRC(itemLocalID,dataCRC);
      #else
      // forget this one
      delete itemP;
      // check next
      foundone = getNextItem(itemP);
      #endif
    }
    else {
      #ifdef CHANGEDETECTION_AVAILABLE
      foundone = getNextItemInfo(itemLocalID,itemIsModified);
      #endif
    }
  } // while all records in DB
  // check that we've terminated the list because we've really seen all items and not
  // because of an error reading an item (which would cause false deletes)
  if (isDBError(lastDBError())) {
    sta=510; // database error
    goto error;
  }
  // now find and update delete candidates
  // (That is, all entries in the log that have no DB record associated any more)
  // Note: we only search logentries that were here already before the preflight,
  //       because new ones never are delete candidates
  for (logindex=0;logindex<numexistinglogentries;logindex++) {
    // check if delete candidate
    if (existingentries[logindex].flags & chgl_delete_candidate) {
      // Note: delete candidates are never already chgl_deleted
      // - update entry
      existingentries[logindex].flags &= ~chgl_delete_candidate; // no candidate...
      existingentries[logindex].flags |= chgl_deleted; // ..but really deleted
      existingentries[logindex].dataCRC=0; // no CRC any more
      existingentries[logindex].modcount=fCurrentModCount; // deletion detected now
      PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("changeLogPreflight: item with logindex=%ld was not found in datastore -> mark deleted",(long)logindex));
      // this is a local change for this session
      if (!isResuming() || (existingentries[logindex].flags & chgl_markedforresume))
        fNumberOfLocalChanges++; // in resumes: only count those that are actually marked for resume
    }
  }
  // successfully updated in memory, now write changed entries back to binfile
  // Note: entries created during this preflight are already saved. Only in-memory modifications to pre-existing ones need to be saved.
  #ifdef SYDEBUG
  DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,("changeLogPreflight: saving %ld existing entries",(long)numexistinglogentries));
  if (DEBUGTEST(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC)) {
    for (uInt32 si=0; si<numexistinglogentries; si++) {
      #ifdef NUMERIC_LOCALIDS
      DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
        "%ld : localID=%ld, flags=0x%X, modcount=%ld, modcount_created=%ld",
        (long)si,
        existingentries[si].dbrecordid,
        (int)existingentries[si].flags,
        (long)existingentries[si].modcount,
        (long)existingentries[si].modcount_created
      ));
      #else
      DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
        "%ld : localID='%s', flags=0x%X, modcount=%ld, modcount_created=%ld",
        (long)si,
        existingentries[si].dbrecordid,
        (int)existingentries[si].flags,
        (long)existingentries[si].modcount,
        (long)existingentries[si].modcount_created
      ));
      #endif
    }
  }
  #endif
  // - write back all existing entries
  fChangeLog.updateRecord(0,existingentries,numexistinglogentries);
  // - now we can confirm we have a valid changelog
  aValidChangelog=true;
  DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,("changeLogPreflight: seen=%ld, fNumberOfLocalChanges=%ld",(long)seen,(long)fNumberOfLocalChanges));
done:
  sta = err==BFE_OK ? LOCERR_OK : (err==BFE_MEMORY ? LOCERR_OUTOFMEM : LOCERR_UNDEFINED);
error:
  // release buffered changelog
  if (existingentries) sysync_free(existingentries);
  // return state
  PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("changeLogPreflight: numberOfLocalChanges=%ld, status=%hd (binfileerr=%hd)%s",(long)fNumberOfLocalChanges,sta,err,lastDBErrorText().c_str()));
  PDEBUGENDBLOCK("changeLogPreflight");
  return sta;
} // TBinfileImplDS::changeLogPreflight



// Simple DB access interface methods
// ==================================

// load target settings record for this datastore
localstatus TBinfileImplDS::loadTarget(bool aCreateIfMissing, cAppCharP aRemoteDBID)
{
  uInt32 remotepartyID = static_cast<TBinfileImplClient *>(fSessionP)->fRemotepartyID;
  TBinFile *targetsBinFileP = &(static_cast<TBinfileImplClient *>(fSessionP)->fConfigP->fTargetsBinFile);
  // for server or version with syncrequests in config, we must try to load
  // the target record or create one if it is missing
  if (fTargetIndex<0) {
    // try to find target by fRemotepartyID and targetDB code/name
    uInt32 maxidx=targetsBinFileP->getNumRecords();
    uInt32 recidx;
    for (recidx=0; recidx<maxidx; recidx++) {
      // - get record
      if (targetsBinFileP->readRecord(recidx,&fTarget)==BFE_OK) {
        // check if this is my target
        if (
          fTarget.remotepartyID == remotepartyID &&
          fTarget.localDBTypeID == fConfigP->fLocalDBTypeID &&
          strucmp(fTarget.localDBPath,fConfigP->fLocalDBPath.c_str(),localDBpathMaxLen)==0
        ) {
          // this is the target record for our DB, now get it (mark it busy)
          fTargetIndex=recidx;
          return LOCERR_OK; // target found
        }
      }
    }
    // target not found
    if (aCreateIfMissing) {
      // create new target record
      // - init with defaults
      fConfigP->initTarget(fTarget,remotepartyID,aRemoteDBID,true); // enabled if created here!
      // - save new record
      uInt32 ti;
      targetsBinFileP->newRecord(ti,&fTarget);
      fTargetIndex = ti;
      return LOCERR_OK; // created and loaded now
    }
    return 404; // not found
  }
  return LOCERR_OK; // already loaded
} // TBinfileImplDS::loadTarget



// - called for SyncML 1.1 if remote wants number of changes.
//   Must return -1 if no NOC value can be returned
//   NOTE: we implement it here only for server, as it is not really needed
//   for clients normally - if it is needed, client's agent must provide
//   it in derived class as StdLogicDS has no own list it can use to count
//   in client case.
sInt32 TBinfileImplDS::getNumberOfChanges(void)
{
  if (binfileDSActive() && IS_CLIENT) {
    // for client case with active binfile, we return the locally computed count
    // (for server, the entire list of changes is loaded by the baseclass
    // before NOC is needed, so the baseclass has the more accurate count
    // which takes filtering etc. into account).
    return fNumberOfLocalChanges;
  }
  // otherwise, let base class handle it (server and client w/o binfile)
  return inherited::getNumberOfChanges();
}

bool TBinfileImplDS::hasPendingChangesForNextSync()
{
  if (binfileDSActive() && IS_CLIENT) {
    // shortcut?
    if (fHasPendingChanges) {
      PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,("sync session known to be incomplete"));
      return true;
    }
    // also check change log, just to be sure;
    // note that new entries in the change log that were
    // added during the current sync session are not yet loaded,
    // and that entries in memory may still need to be flushed
    // to disk
    if (fLoadedChangeLog) {
      for (uInt32 si=0; si<fLoadedChangeLogEntries; si++) {
        if (fLoadedChangeLog[si].modcount > fCurrentModCount) {
#ifdef NUMERIC_LOCALIDS
          PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
            "%ld : localID=%ld, flags=0x%X, modcount=%ld, modcount_created=%ld => pending change",
            (long)si,
            fLoadedChangeLog[si].dbrecordid,
            (int)fLoadedChangeLog[si].flags,
            (long)fLoadedChangeLog[si].modcount,
            (long)fLoadedChangeLog[si].modcount_created
          ));
#else
          PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
            "%ld : localID='%s', flags=0x%X, modcount=%ld, modcount_created=%ld => pending change",
            (long)si,
            fLoadedChangeLog[si].dbrecordid,
            (int)fLoadedChangeLog[si].flags,
            (long)fLoadedChangeLog[si].modcount,
            (long)fLoadedChangeLog[si].modcount_created
          ));
#endif
          return true;
        }
      }
    }
    return false;
  }
  return inherited::getNumberOfChanges();
}



/// sync login (into this database)
/// @note might be called several times (auth retries at beginning of session)
/// @note must update the following saved AND current state variables
/// - in TLocalEngineDS: fLastRemoteAnchor, (fLastLocalAnchor), fResumeAlertCode, fFirstTimeSync
///   - for client: fPendingAddMaps
/// - in TStdLogicDS: fPreviousSyncTime, fCurrentSyncTime
/// - in TBinfileImplDS: ??? /// @todo document these
/// - in derived classes: whatever else belongs to dsSavedAdmin and dsCurrentAdmin state
localstatus TBinfileImplDS::implMakeAdminReady(
  cAppCharP aDeviceID,    // remote device URI (device ID)
  cAppCharP aDatabaseID,  // database ID
  cAppCharP aRemoteDBID  // database ID of remote device
)
{
  if (!binfileDSActive()) return LOCERR_WRONGUSAGE; // must be active when called at all

  localstatus sta=LOCERR_OK; // assume ok

  PDEBUGBLOCKDESCCOLL("implMakeAdminReady","Loading target info and pending maps");
  // - init defaults
  fLastRemoteAnchor.erase();
  fPreviousSyncTime=0;
  fFirstTimeSync=false; // assume not first time


  #if !defined(PRECONFIGURED_SYNCREQUESTS)
  // when sync params are in binfiles, target must be present by now - make sure it is loaded
  sta=loadTarget(false);
  // target info must already be present by now (loaded at session's SelectProfile)
  if (sta!=LOCERR_OK || fTargetIndex<0) {
    // problem loading target record
    sta = sta ? sta : 404;
    PDEBUGPRINTFX(DBG_ERROR,("Error %d loading target record",sta));
    PDEBUGENDBLOCK("implMakeAdminReady");
    return sta;
  }
  // we have the target in the fTarget member
  #else
  // e.g. for clients without syncrequests in config, target record might not exist here, so we allow creating it
  sta=loadTarget(true, aRemoteDBID);
  #endif
  // Now fTarget has valid target info
  // - if we don't have any remote anchor stored, this must be a first time sync
  if (*(fTarget.remoteAnchor)==0)
    fFirstTimeSync=true;
  // - get last anchor
  if (fTarget.forceSlowSync) {
    // - forget last anchor
    fLastRemoteAnchor.erase(); // make sure we get a slow sync
  }
  else {
    // - get last anchor
    fLastRemoteAnchor.assign(fTarget.remoteAnchor);
  }
  // - get last resume info
  fPreviousSuspendModCount = fTarget.lastSuspendModCount; /// @note: before DS 1.2 engine this was used as "lastModCount"
  fResumeAlertCode = fTarget.resumeAlertCode;
  // - get last sync time and changelog cursor
  fPreviousToRemoteModCount = fTarget.lastTwoWayModCount; // reference is when we've last full-synced!
  fPreviousSyncTime = fTarget.lastSync;
  PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,(
    "implMakeAdminReady(binfile): ResumeAlertCode=%hd, PreviousSuspendModCount=%ld, PreviousToRemoteModCount=%ld, LastRemoteAnchor='%s'",
    fResumeAlertCode,
    (long)fPreviousSuspendModCount,
    (long)fPreviousToRemoteModCount,
    fLastRemoteAnchor.c_str()
  ));
  // determine time of this sync (will be overridden in case of BASED_ON_BINFILE_CLIENT)
  fCurrentSyncTime=getSession()->getSystemNowAs(TCTX_UTC); // NOW !
  // do some more things if we are starting sync now
  if (!fPreflighted) {
    if (!openChangeLog()) {
      // changelog did not exist yet
      // - force slow sync
      fTarget.forceSlowSync=true; // set target flag to force slowsync even if we repeat this
      fLastRemoteAnchor.erase();
      fPreviousSyncTime = noLinearTime;
      fPreviousToRemoteModCount = 0;
      fPreviousSuspendModCount = 0;
      // - no compare references yet
      fPreviousToRemoteSyncCmpRef = noLinearTime;
      fPreviousSuspendIdentifier.erase();
    }
    else {
      // Get token and date representing last update of this changelog (last preflight)
      // Note: towards the database, we only check for changes since the last preflight (the fPreviousToRemoteSyncCmpRef is semantically
      //       incorrect when TCustomImplDS is used with BASED_ON_BINFILE_CLIENT on top of binfile).
      //       The separation between changes since last-to-remote sync and last resume is done based on the changelog modcounts.
      // - get date of last check
      fPreviousToRemoteSyncCmpRef=fChgLogHeader.lastChangeCheck;
      fPreviousSuspendCmpRef=fPreviousToRemoteSyncCmpRef; // DB on top of binfile only needs one reference time, which is the last changelog check time.
      #ifdef SYDEBUG
      string lsd;
      StringObjTimestamp(lsd,fPreviousToRemoteSyncCmpRef);
      PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("- last preflight update (fPreviousToRemoteSyncCmpRef) at %s",lsd.c_str()));
      #endif // SYDEBUG
      #if TARGETS_DB_VERSION>=6
      // - DB api level change detection identifiers
      fPreviousToRemoteSyncIdentifier.assign(fChgLogHeader.lastChangeCheckIdentifier);
      fPreviousSuspendIdentifier = fPreviousToRemoteSyncIdentifier; // DB on top of binfile only needs one reference time, which is the last changelog check time.
      PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("- last preflight update (fPreviousToRemoteSyncIdentifier) is '%s'",fPreviousToRemoteSyncIdentifier.c_str()));
      #endif // TARGETS_DB_VERSION>=6
    }
  }
  // get pending maps anyway (even if not resuming there might be pending maps)
  if(openPendingMaps()) {
    // there is a pending map file, check if these are really our maps
    // Note: with separated changelogs, this should be always the case!
    PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
      "implMakeAdminReady: remotePartyID of pendingmaps=%ld, current profile's remotepartyID=%ld",
      (long)fPendingMapHeader.remotepartyID,
      (long)static_cast<TBinfileImplClient *>(fSessionP)->fRemotepartyID
    ));
    if (fPendingMapHeader.remotepartyID == static_cast<TBinfileImplClient *>(fSessionP)->fRemotepartyID) {
      // these are our maps, load them
      TPendingMapEntry pme;
      string localid;
      for (uInt32 i=0; i<fPendingMaps.getNumRecords(); i++) {
        fPendingMaps.readRecord(i,&pme);
        // store in localEngineDS' list
        LOCALID_TO_STRING(pme.dbrecordid,localid);
        fPendingAddMaps[localid]=pme.remoteID;
      }
      PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("implMakeAdminReady: loaded %ld pending add maps",(long)fPendingMaps.getNumRecords()));
    }
  }
  if (fResumeAlertCode) {
    // get pending item only if we have a resume state
    // - prep file
    TBinFile pendingItemFile;
    TPendingItemHeader pendingItemHeader;
    string fname = static_cast<TBinfileClientConfig *>(fSessionP->getSessionConfig())->relatedDBNameBase(getName(), fTarget.remotepartyID);
    fname += PENDINGITEM_DB_SUFFIX;
    pendingItemFile.setFileInfo(fname.c_str(),PENDINGITEM_DB_VERSION,PENDINGITEM_DB_ID,0); // no expected record size
    PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
      "opening pending item file: file name='%s'",
      fname.c_str()
    ));
    if (pendingItemFile.open(sizeof(TPendingItemHeader),&pendingItemHeader)==BFE_OK) {
      // we have a pending item file
      if (pendingItemHeader.remotepartyID == static_cast<TBinfileImplClient *>(fSessionP)->fRemotepartyID) {
        // this is our pending item, load it
        // - transfer header data
        fPartialItemState = pendingItemHeader.piState; // note: we always have the pi_state_loaded... state here
        fLastItemStatus = pendingItemHeader.lastItemStatus;
        fLastSourceURI = pendingItemHeader.lastSourceURI;
        fLastTargetURI = pendingItemHeader.lastTargetURI;
        fPITotalSize = pendingItemHeader.totalSize;
        fPIUnconfirmedSize = pendingItemHeader.unconfirmedSize;
        fPIStoredSize = pendingItemHeader.storedSize;
        // - load the data if any
        if (fPIStoredDataP && fPIStoredDataAllocated) smlLibFree(fPIStoredDataP);
        fPIStoredDataP=NULL;
        fPIStoredDataAllocated=false;
        if (fPIStoredSize) {
          fPIStoredDataP=smlLibMalloc(fPIStoredSize+1); // one for safety null terminator
          if (fPIStoredDataP) {
            fPIStoredDataAllocated=true;
            // get the data
            pendingItemFile.readRecord(0,fPIStoredDataP,1);
            *((uInt8 *)fPIStoredDataP+fPIStoredSize)=0; // safety null terminator
          }
          else {
            PDEBUGPRINTFX(DBG_ERROR,("Cannot allocate buffer (%ld bytes) for pendingitem",(long)fPIStoredSize));
            fPIStoredSize=0;
          }
        }
      }
      PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("implMakeAdminReady: loaded pending item with %ld data bytes",(long)fPIStoredSize));
      // done
      pendingItemFile.close();
    }
  }
  // done
  PDEBUGENDBLOCK("implMakeAdminReady");
  return sta;
} // TBinfileImplDS::implMakeAdminReady


localstatus TBinfileImplDS::implStartDataRead()
{
  if (!binfileDSActive()) return LOCERR_WRONGUSAGE; // must be active when called at all

  // init reading of all records
  // - start at beginning of log
  fLogEntryIndex=0;
  return LOCERR_OK;
} // TBinfileImplDS::implStartDataRead


/* %%% seems obsolete - never called from anywhere

#ifdef OBJECT_FILTERING

// Test Filters
bool TBinfileImplDS::testFilters(TMultiFieldItem *aItemP)
{
  return
    // generally suitable item for this datastore
    aItemP->testFilter(fLocalDBFilter.c_str()) &&
    // and passing current sync set filter
    aItemP->testFilter(fSyncSetFilter.c_str()) &&
    // and not invisible
    (
      getDSConfig()->fInvisibleFilter.empty() || // no invisible filter means visible
      !aItemP->testFilter(getDSConfig()->fInvisibleFilter.c_str()) // failing invisible filter means visible too
    );
} // TBinfileImplDS::testFilters

#endif

*/

/// @brief called to have all non-yet-generated sync commands as "to-be-resumed"
void TBinfileImplDS::implMarkOnlyUngeneratedForResume(void)
{
  if (!binfileDSActive()) return; // must be active when called at all

  TChangeLogEntry *chglogP;

  // simply return aEof when just refreshing
  if (fRefreshOnly) return;
  // make sure we have the changelog in memory
  loadChangeLog();
  // check if more records
  uInt32 logEntryIndex=fLogEntryIndex;
  DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,("implMarkOnlyUngeneratedForResume: total=%ld, already generated=%ld",(long)fLoadedChangeLogEntries,(long)logEntryIndex));
  // Note: already generated entries had their flag cleared when they were generated.
  //       Some of them might already be marked now again by early markItemForResume() due to unsuccessful status
  // for all remaining records, check if they must be marked or not
  // (depends on current marked state as well if this is a resumed session already)
  while (logEntryIndex<fLoadedChangeLogEntries) {
    bool markforresume=true; // assume we must mark it
    // get ptr to entry
    chglogP=&fLoadedChangeLog[logEntryIndex];
    // advance to next
    logEntryIndex++;
    // if resuming, check if we must include this item at all
    if (isResuming()) {
      // we are resuming, only report those that have the mark-for-resume flag set
      if (!(chglogP->flags & chgl_markedforresume)) {
        // not marked for resume, but check if it has changed after the last suspend
        if (chglogP->modcount<=fPreviousSuspendModCount) {
          // is not marked for resume AND has not changed since last suspend
          markforresume=false; // not to be included in resume
        }
      }
    }
    if (markforresume) {
      // this item would have been reported in THIS session
      // now check if this should be marked for resume for NEXT session
      if (fSlowSync) {
        // slow sync mode
        if (chglogP->flags & chgl_deleted) {
          // skip deleted in slow sync
          markforresume=false; // not to be included in resume
        }
      }
      else if (!fSlowSync) {
        // prevent ANY reporting of items marked as receiveOnly in normal sync (but send them in slow sync!)
        if (chglogP->flags & chgl_receive_only) {
          // skip receive-only items in normal sync. So deleting or changing them locally will not send them
          // to the server. However after a slow sync, existing local items will be send (and possibly added,
          // if not already there) to the server
          markforresume=false; // not to be included in resume
        }
        else {
          // mark for resume if change to be reported
          markforresume=chglogP->modcount>fPreviousToRemoteModCount;
        }
      }
    }
    // now apply change of chgl_markedforresume to actual changelog entry
    #ifdef NUMERIC_LOCALIDS
    DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
      "implMarkOnlyUngeneratedForResume: localID=%ld new markforresume=%d, old markforresume=%d",
      (long)chglogP->dbrecordid,
      (int)markforresume,
      (int)((chglogP->flags & chgl_markedforresume)!=0)
    ));
    #else
    DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
      "implMarkOnlyUngeneratedForResume: localID='%s' new markforresume=%d, old markforresume=%d",
      chglogP->dbrecordid,
      (int)markforresume,
      (int)((chglogP->flags & chgl_markedforresume)!=0)
    ));
    #endif
    // update flag
    if (markforresume)
      chglogP->flags |= chgl_markedforresume;
    else
      chglogP->flags &= ~chgl_markedforresume;
    // Note: changelog will be saved at SaveAdminData
  }
} // TBinfileImplDS::implMarkOnlyUngeneratedForResume


// called to mark an already generated (but unsent or sent but not yet statused) item
// as "to-be-resumed", by localID or remoteID (latter only in server case).
void TBinfileImplDS::implMarkItemForResume(cAppCharP aLocalID, cAppCharP aRemoteID, bool aUnSent)
{
  if (!binfileDSActive()) return; // must be active when called at all

  // make sure we have the changelog in memory
  loadChangeLog();
  localid_out_t locID;
  STR_TO_LOCALID(aLocalID,locID);
  // search for item by localID
  uInt32 i;
  for (i=0; i<fLoadedChangeLogEntries; i++) {
    if (LOCALID_EQUAL(fLoadedChangeLog[i].dbrecordid,LOCALID_OUT_TO_IN(locID))) {
      // found - mark it for resume
      DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
        "implMarkItemForResume: localID='%s': marking changelog entry for resume=1, old markforresume=%d",
        aLocalID,
        (int)((fLoadedChangeLog[i].flags & chgl_markedforresume)!=0)
      ));
      fLoadedChangeLog[i].flags |= chgl_markedforresume;
      break;
    }
  }
  #ifdef SYDEBUG
  if (i>=fLoadedChangeLogEntries) {
    DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
      "implMarkItemForResume: localID='%s': changelog entry not found",
      aLocalID
    ))
  }
  #endif
} // TBinfileImplDS::implMarkItemForResume


// called to mark an already sent item as "to-be-resent", e.g. due to temporary
// error status conditions, by localID or remoteID (latter only in server case).
void TBinfileImplDS::implMarkItemForResend(cAppCharP aLocalID, cAppCharP aRemoteID)
{
  if (!binfileDSActive()) return; // must be active when called at all

  // make sure we have the changelog in memory
  loadChangeLog();
  localid_out_t locID;
  STR_TO_LOCALID(aLocalID,locID);
  // search for item by localID
  uInt32 i;
  for (i=0; i<fLoadedChangeLogEntries; i++) {
    if (LOCALID_EQUAL(fLoadedChangeLog[i].dbrecordid,LOCALID_OUT_TO_IN(locID))) {
      // found - mark it for resume
      DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
        "implMarkItemForResend: localID='%s': marking changelog entry for resend",
        aLocalID
      ));
      fLoadedChangeLog[i].flags |= chgl_resend;
      break;
    }
  }
  #ifdef SYDEBUG
  if (i>=fLoadedChangeLogEntries) {
    DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
      "implMarkItemForResend: localID='%s': changelog entry not found",
      aLocalID
    ))
  }
  #endif
} // TBinfileImplDS::implMarkItemForResend




// Get next item from database
localstatus TBinfileImplDS::implGetItem(
  bool &aEof,
  bool &aChanged, // if set on entry, only changed ones will be reported, otherwise all will be returned and aChanged contains flag if entry has changed or not
  TSyncItem* &aSyncItemP
)
{
  if (!binfileDSActive()) return LOCERR_WRONGUSAGE; // must be active when called at all

  localstatus sta=LOCERR_OK;
  TSyncItem *myitemP=NULL;
  TChangeLogEntry *chglogP;
  bool onlyChanged = aChanged;

  aEof=true;

  // Update change log
  // Note: we cannot do it earlier as we need item types setup correctly
  // which is not the case before here!
  if (!fPreflighted) {
    fPreflighted=true;
    // find and update (if not refreshing) changelog for this database
    bool normal=false;
    sta=changeLogPreflight(normal);
    if (sta!=LOCERR_OK)
      return sta; // database error
  }
  // simply return aEof when just refreshing
  if (fRefreshOnly) return LOCERR_OK;
  // make sure we have the changelog in memory
  loadChangeLog();
  do {
    if (fLogEntryIndex<fLoadedChangeLogEntries) {
      // there is a log entry, get it
      chglogP = &fLoadedChangeLog[fLogEntryIndex];
      // advance to next
      fLogEntryIndex++;
      // if resuming, check if we must include this item at all
      if (isResuming()) {
        // we are resuming, only report those that have the mark-for-resume flag set
        if (!(chglogP->flags & chgl_markedforresume)) {
          // not marked for resume, but check if it has changed after the last suspend
          // NOTE: this catches those added after last suspend as well, which is IMPORTANT
          //       (otherwise, they would not get detected later, as we do not differentiate
          //       adds and replaces (unlike in server, where new adds will be shown in the
          //       next session after the resumed one)
          if (chglogP->modcount<=fPreviousSuspendModCount) {
            // is not marked for resume AND has not changed (or was added) since last suspend
            continue; // not for resume - check next
          }
          else {
            // New behaviour here from 3.1.5.2 onwards:
            // SyncML DS 1.2.1 explicitly FORBIDS that changes happening after suspending
            // are included in a resume. So we have to post-pone these. For that we must artificially
            // mark them changed such that they will be detected in next non-resume (but not before).
            // Note that this also affects other profiles as there is only one single changelog -
            // however, this is not a problem because the records detected new or changed now
            // will inevitably be new or changed in all other profiles as well. So we can
            // safely touch these record's modcount w/o any noticeable side effects in other profiles.
            PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Detected item changed/added after suspend -> postpone reporting it for next non-resumed session"));
            // - update the mod count such that this record will be detected again in the next non-resumed session
            //   (fCurrentModCount marks entries changed in this session, +1 makes sure these will be detected in the NEXT session)
            chglogP->modcount=fCurrentModCount+1;
            // also update the creation stamp for new added item
            if(chglogP->modcount_created > fPreviousToRemoteModCount) {
              chglogP->modcount_created=fCurrentModCount+1;
            }
            // - mark it "modified by sync" to prevent it being sent during resume
            chglogP->flags |= chgl_modbysync;
          }
        }
        /* NOTE: 2007-12-17, luz: I initially added chgl_modbysync just to prevent that adds from server
                 occurred in a suspended part of the session would be sent back to the
                 server in the resume (as these DO get marked for resume). However, this
                 is not a good strategy because the server cannot know which of its adds
                 were actually successful, so the client MUST send them back to the
                 server so the server can tell which items must be sent (possibly again)
                 and which are definitely already stored in the client.
                 See correspondence with Faisal from Oracle around 2007-12-17.
                 So the chgl_modbysync flag is maintained for now, but not used
                 for anything. */
        /* NOTE: 2008-01-11, luz: THE ABOVE IS WRONG! The server CAN know which adds were
                 successful: those for which it has received a map in the suspended session
                 or a begin-of-session-map in the current session (or both). So we SHOULD
                 suppress those items that have chgl_modbysync set. */
        // we are resuming, prevent reporting those back that were added or changed
        // by one of the suspended earlier parts of this session
        if (chglogP->flags & chgl_modbysync) {
          PDEBUGPRINTFX(DBG_ADMIN+DBG_HOT,("Item not sent because it was added/changed by remote in suspended previous session or by user on device after suspend"));
          continue; // these must NOT be reported back during a resumed sync
        }
      }
      else {
        // make sure we clear the resume mark on ALL entries, even those not reported
        // (as the marks are all invalid)
        chglogP->flags &= ~chgl_markedforresume;
        // for a non-resumed slow sync, also clear all resend flags
        if (fSlowSync) chglogP->flags &= ~chgl_resend;
      }
      // At this point, the current entry is a candidate for being reported
      // (not excplicitly excluded)
      // - now check if and how to report it
      if (fSlowSync) {
        // slow sync mode
        // - skip deleted in slow sync, but report all others
        if (chglogP->flags & chgl_deleted) {
          continue; // check next
        }
      }
      else {
        // prevent ANY reporting of items marked as receiveOnly in normal sync (but send them in slow sync!)
        if (chglogP->flags & chgl_receive_only) {
          // skip receive-only items in normal sync. So deleting or changing them locally will not send them
          // to the server. However after a slow sync, existing local items will be send (and possibly added,
          // if not already there) to the server
          continue;
        }
        // mark those as "changed" which have really changed or have the resend flag set
        bool hasChanged=
          (chglogP->modcount>fPreviousToRemoteModCount) || // change detected
          (chglogP->flags & chgl_resend); // or marked for resend e.g. due to error in last session
        #ifdef SYDEBUG
        if (chglogP->flags & chgl_resend) {
          PDEBUGPRINTFX(DBG_ADMIN+DBG_HOT,(
            "Item treated as changed because resend-flag was set"
          ));
        }
        #endif
        // clear resend flag now - it is processed
        chglogP->flags &= ~chgl_resend;
        // skip unchanged ones if only changed ones are to be reported
        if (!hasChanged && onlyChanged)
          continue; // unchanged, do not report
        // always report changed status in aChanged
        aChanged=hasChanged;
      }
      // this entry is to be reported
      // - now check how to report
      if (chglogP->flags & chgl_deleted) {
        // deleted: only report if it is also changed (i.e. delete detected since last sync)
        // Note: this can only happen when caller requests to see all records, not only changed,
        //       which usually only occurs in slowsync where deletes are NOT reported anyway.
        //       However: when caller must dynamically filter the syncset, it will request all records
        //       even in normal syncs and then this can happen.
        if (!aChanged)
          continue; // delete was reported earlier, don't report it again
        // deleted, we cannot get it from the DB, create a empty item
        myitemP = new TSyncItem();
        // add ID
        ASSIGN_LOCALID_TO_ITEM(*myitemP,chglogP->dbrecordid);
        // deleted, syncop is delete
        myitemP->setSyncOp(sop_delete);
      }
      else {
        // get contents from the DB
        myitemP=NULL; // in case getItemByID should abort before pointer assigned
        sta = getItemByID(chglogP->dbrecordid,myitemP);
        if ((sta!=LOCERR_OK) || !myitemP) {
          // error getting record
          if (sta==404) {
            // record seems to have vanished between preflight and now
            PDEBUGPRINTFX(DBG_ERROR,("Record does not exist any more in database%s -> ignore",lastDBErrorText().c_str()));
            // simply don't include in sync set - next preflight will detect it deleted
            if (myitemP) delete myitemP; // delete in case we have some half-filled record here
            continue; // try next
          }
          else {
            // record does not exist
            PDEBUGPRINTFX(DBG_ERROR,("Error getting Record from DB%s -> Status %hd",lastDBErrorText().c_str(),sta));
            goto error;
          }
        }
        // detect wheter the item is new added or changed
        if(chglogP->modcount_created > fPreviousToRemoteModCount) {
          // Added
          myitemP->setSyncOp(sop_add);
        }
        else {
          // Not added (changed or just reported because we want all records reported)
          // - if enabled, also verify change by checking CRC before reporting it (unless this is a slow sync)
          if (CRC_DETECT_PSEUDOCHANGES && aChanged) {
            // check if really changed using CRC, but only...
            // ...if not slow sync (all items must be reported)
            // ...if change was detected in this session's preflight. If the change was detected earlier and is still
            //    pending (i.e. newer than the last sync), this means that this change might have failed to be applied
            //    in a previous sync and thus must be reported again (but as CRC was updated in the last run,
            //    it would be suppressed). This is a compromise that minimizes pseudochanges normally, but cannot
            //    entirely prevent them. In other words: the first attempt to report a pseudo-change is
            //    suppressed, but in case this sync fails, subsequent syncs will report it.
            uInt16 newDataCRC = myitemP->getDataCRC(0,true);
            PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
              "CRC comparison for pseudo-change detection: old CRC=0x%hX, new CRC=0x%hX, recordModCount=%u, currentModCount=%u",
              chglogP->dataCRC, newDataCRC, chglogP->modcount, fCurrentModCount
            ));
            if (chglogP->dataCRC==newDataCRC && !fSlowSync && chglogP->modcount==fCurrentModCount) {
              // none of the relevant fields have changed -> don't report the item
              PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("Not reporting localID='%s' as changed because CRC detected this as a pseudo-change.",myitemP->getLocalID()));
              aChanged = false; // even if it gets reported, it does not count as changed any more
              if (onlyChanged) {
                // we don't need to report this at all, as we only report changed ones
                delete myitemP; // delete the item
                continue; // don't report, try next
              }
              // report as unchanged
            }
            else {
              // change will now be reported, so update CRC to what we report now
              // Note: the problem with this is that in case the sync does not succeed now, the CRC is already updated and would
              //   trigger pseudo-change detection in the next session. Therefore, pseudo-change detection is only active for
              //   changes newly detected during this sync.
              chglogP->dataCRC = newDataCRC;
            }
          } // CRC_DETECT_PSEUDOCHANGES
          // - report as replace (changed or not)
          myitemP->setSyncOp(sop_replace);
        }
        // make sure item has the localid which was used to retrieve it
        ASSIGN_LOCALID_TO_ITEM(*myitemP,chglogP->dbrecordid);
      }
      // - make sure IDs are ok
      myitemP->clearRemoteID(); // client never has a remote ID
      // record found
      // - items that do go out into the engine must not any longer be marked for resume (from this session)
      //   Note: they may get marked by receiving unsuccessful status for them before session ends!
      chglogP->flags &= ~chgl_markedforresume;
      DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,("Reporting localID='%s', flags=0x%x as sop=%s",myitemP->getLocalID(),(int)chglogP->flags,SyncOpNames[myitemP->getSyncOp()]));
      aEof=false;
      break;
    }
    else {
      // no more records
      break;
    }
  } while(true); // loop until record found or EOF
  // set item to return
  aSyncItemP = myitemP;
  // ok
  return LOCERR_OK;
error:
  // general database error
  PDEBUGPRINTFX(DBG_ERROR,("implGetItem error %hd%s",sta,lastDBErrorText().c_str()));
  aSyncItemP=NULL;
  return sta;
} // TBinfileImplDS::implGetItem


// end of read
localstatus TBinfileImplDS::implEndDataRead(void)
{
  if (!binfileDSActive()) return LOCERR_WRONGUSAGE; // must be active when called at all

  // pass it on to the DB api (usually dummy for traditional binfile derivates, but
  // needed for customimplds)
  return apiEndDataRead();
} // TBinfileImplDS::implEndDataRead




/// forget changelog in memory
void TBinfileImplDS::forgetChangeLog(void)
{
  if (fLoadedChangeLog==NULL) return; // ok, already gone
  PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,("forgetChangeLog: DISCARDING already loaded changelog with %ld entries",(long)fLoadedChangeLogEntries));
  sysync_free(fLoadedChangeLog);
  fLoadedChangeLog=NULL;
  fLoadedChangeLogEntries=0;
} // TBinfileImplDS::forgetChangeLog


/// load changelog into memory for quick access
void TBinfileImplDS::loadChangeLog(void)
{
  // if already loaded, simply return
  if (fLoadedChangeLog) return; // ok, already there
  // allocate memory for it
  fLoadedChangeLogEntries=fChangeLog.getNumRecords();
  if (fLoadedChangeLogEntries>0) {
    // (use sysync_malloc because e.g. on PalmOS this uses special funcs to allow > 64k)
    fLoadedChangeLog =
      (TChangeLogEntry *)sysync_malloc(sizeof(TChangeLogEntry)*fLoadedChangeLogEntries);
    if (fLoadedChangeLog) {
      // now load it
      if (fChangeLog.readRecord(0,fLoadedChangeLog,fLoadedChangeLogEntries)==BFE_OK) {
        PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,("loadChangeLog: loaded changelog with %ld entries",(long)fLoadedChangeLogEntries));
      }
      else {
        // cannot load from file, discard allocated buffer
        forgetChangeLog();
      }
    }
    else {
      // not enough memory, cannot load
      fLoadedChangeLogEntries=0;
    }
  }
} // TBinfileImplDS::loadChangeLog


#define ZAP_FORCES_SLOWSYNC 1

// start of write
localstatus TBinfileImplDS::implStartDataWrite(void)
{
  if (!binfileDSActive()) return LOCERR_WRONGUSAGE; // must be active when called at all

  localstatus sta = LOCERR_OK;

  // latest chance to do preflight in case GetItem was never called
  if (!fPreflighted) {
    fPreflighted=true;
    // find and update (if not refreshing) changelog for this database
    bool normal=false;
    changeLogPreflight(normal);
    if (!normal) return LOCERR_UNDEFINED; // failure
  }
  // let api layer do it's stuff
  sta = apiStartDataWrite();
  if (sta!=LOCERR_OK) return sta; // failed
  // check if we need to zap the datastore first
  // Note: only do it if not resuming!
  if (fSlowSync && fRefreshOnly && !isResuming()) {
    // yes, zap data
    sta = zapDatastore();
    if (sta!=LOCERR_OK) {
      preventResuming(); // only half-zapped, prevent resuming (we must retry zapping ALL and then do a full slow sync anyway)
      return sta; // failed
    }
    // now, either zap the changelog and set this datastore to slowsync in all
    // other profiles or flag all entries as deleted during this sync.
    #ifdef ZAP_FORCES_SLOWSYNC
    // we force a slowsync, zap the changelog
    fChangeLog.truncate(0);
    // forget the anchor to force a slow sync anyway
    fTarget.remoteAnchor[0]=0; // (only to make sure. If sync completes successfully, this will be updated anyway in SaveAnchor)
    // for combined changelog, we need to take care of other profiles
    if (!static_cast<TBinfileClientConfig *>(fSessionP->getSessionConfig())->fSeparateChangelogs) {
      // zap the anchors in all other profiles for this datastore
      // because we have deleted the (common) changelog. Note that this also resets our own
      // target's anchor (again)
      // get target DB
      TBinFile *targetsBinFileP = &(static_cast<TBinfileImplClient *>(fSessionP)->fConfigP->fTargetsBinFile);
      TBinfileDBSyncTarget target;
      // - loop trough all targets
      for (uInt32 ti=0; ti<targetsBinFileP->getNumRecords(); ti++) {
        targetsBinFileP->readRecord(ti,&target);
        if (
          (target.localDBTypeID == fTarget.localDBTypeID) && // same datastoreID
          (strucmp(target.localDBPath,fTarget.localDBPath)==0) // ..and name
        ) {
          // this is a target of this datastore, remove saved anchor
          // to irreversibely force a slowsync next time
          target.remoteAnchor[0]=0;
          targetsBinFileP->updateRecord(ti,&target);
        }
      }
    } // if combined changelog
    #endif
  }
  // Load changelog as we need quick access
  loadChangeLog();
  #ifndef ZAP_FORCES_SLOWSYNC
  if (fLoadedChangeLogEntries>0) {
    if (fSlowSync && fRefreshOnly) {
      // database was zapped, all existing changelog entries must be set to deleted
      // by this sync session.
      // (This is important for combined changelog, for separate changelog these entries
      // get obsolete anyway after this sync, but updating them for consistency is still good)
      for (uInt32 k=0; k<fLoadedChangeLogEntries; k++) {
        fLoadedChangeLog[k].modcount=fCurrentModCount;
        fLoadedChangeLog[k].dataCRC=0;
        fLoadedChangeLog[k].flags=chgl_deleted;
      }
    }
  }
  #endif
  // done
  return sta;
} // TBinfileImplDS::implStartDataWrite


// - retrieve specified item from database
bool TBinfileImplDS::implRetrieveItemByID(
  TSyncItem &aItem,         // the item
  TStatusCommand &aStatusCommand
)
{
  if (!binfileDSActive()) return LOCERR_WRONGUSAGE; // must be active when called at all
  // %%% not so nice as we need to copy it once
  TSyncItem *itemP=NULL;
  // read item by local ID
  localid_t localid;
  localstatus sta;
  STR_TO_LOCALID(aItem.getLocalID(),localid);
  // get item now
  if ((sta=getItemByID(localid,itemP))!=LOCERR_OK) {
    aStatusCommand.setStatusCode(sta); // report status
    aStatusCommand.addItemString(lastDBErrorText().c_str());
    DEBUGPRINTFX(DBG_ERROR,("Item not found in database"));
    return false;
  }
  if (itemP) {
    // make sure item has the localid which was used to retrieve it
    ASSIGN_LOCALID_TO_ITEM(*itemP,localid);
    // and also the same syncop
    itemP->setSyncOp(aItem.getSyncOp());
    // now copy retrieved contents to original item
    aItem = *itemP;
    delete itemP;
  }
  // ok
  return true;
} // TBinfileImplDS::implRetrieveItemByID


// process single item (but does not consume it)
bool TBinfileImplDS::implProcessItem(
  TSyncItem *aItemP,         // the item
  TStatusCommand &aStatusCommand
)
{
  if (!binfileDSActive()) return LOCERR_WRONGUSAGE; // must be active when called at all

  localid_out_t newid;
  TSyError statuscode;
  localstatus sta;
  bool ok;
  bool receiveOnly=false; // default to normal two-way
  bool reportAsChangedInNextSync=false;
  bool reportAsDeletedInNextSync=false;
  TMultiFieldItem *augmentedItemP = NULL;

  SYSYNC_TRY {
    // - get op
    TSyncOperation sop = aItemP->getSyncOp();
    // - get localid
    localid_t localid;
    STR_TO_LOCALID(aItemP->getLocalID(),localid);
    // - now perform op
    switch (sop) {
      // %%% note: sop_copy is now implemented by read/add sequence
      //     in localdatatstore, but will be moved here later possibly
      case sop_add :
        // add record
        if ((sta=createItem(aItemP,newid,receiveOnly))!=LOCERR_OK) {
          PDEBUGPRINTFX(DBG_ERROR,("cannot create record in database (sta=%hd)",sta));
          // check special "needs merge" case
          if (sta==DB_Conflict) {
            // DB has detected item conflicts with data already stored in the database and
            // request merging current data from the backend with new data before storing.
            // - get the ID of the record to merge with
            localid = LOCALID_OUT_TO_IN(newid);
            ASSIGN_LOCALID_TO_ITEM(*aItemP,localid);
            // - merge with database contents
            bool changedDBVersion, changedNewVersion;
            augmentedItemP = mergeWithDatabaseVersion(aItemP, changedDBVersion, changedNewVersion);
            if (augmentedItemP==NULL)
              sta = DB_Error; // no item found, DB error
            else {
              // store augmented version back to DB only if modified
              if (changedDBVersion) {
                STR_TO_LOCALID(augmentedItemP->getLocalID(),localid);
                sta = updateItemByID(localid,augmentedItemP);
                // we'll need to sync it back in the next session,
                // because the server's copy is now older than the
                // client's copy
                if (sta == LOCERR_OK) reportAsChangedInNextSync = true;
              }
              else {
                sta = LOCERR_OK;
              }
            }
            // The server now has two items where the client only has
            // one. If we were to tell the server the same local ID
            // for both of the server's items, it would get confused.
            // For example, the Synthesis engine then sent a Delete
            // request with empty client and server ID.
            //
            // Instead, let the rest of the code below proceed (to get
            // the state of the existing local item right), and then
            // create a unique, fake local ID that we assign to the
            // item and copy into the change log as "deleted item".
            if (sta == LOCERR_OK) reportAsDeletedInNextSync = true;
          }
          else {
            statuscode=sta;
            goto error; // check errors
          }
        }
        // set status
        statuscode=201; // added
        // set new localID into item
        localid = LOCALID_OUT_TO_IN(newid);
        ASSIGN_LOCALID_TO_ITEM(*aItemP,localid);
        break;
      case sop_replace :
        // change record
        if ((sta=updateItemByID(localid,aItemP))!=LOCERR_OK) {
          PDEBUGPRINTFX(DBG_ERROR,("cannot update record in database (sta=%hd)",sta));
          statuscode=sta;
          goto error; // check errors
        }
        // set status
        statuscode=200; // replaced
        break;
      case sop_delete :
        // delete record
        if ((sta=deleteItemByID(localid))!=LOCERR_OK) {
          PDEBUGPRINTFX(DBG_ERROR,("cannot delete record in database (sta=%hd)",sta));
          statuscode=sta; // not found
          goto error; // check errors
        }
        // set status
        statuscode=200;
        break;
      default :
        PDEBUGPRINTFX(DBG_ERROR,("Unknown sync-op in TBinfileImplDS::ProcessItem"));
        statuscode=501; // not implemented
        goto error;
    } // switch(sop)
    // update changelog
    uInt16 crc=0; // none unless we need it
    if (CRC_CHANGE_DETECTION || CRC_DETECT_PSEUDOCHANGES) {
      // - calc new data CRC if not deleted record
      if (sop!=sop_delete) {
        // - get new CRC. Note: to avoid differences in written and readback
        //   data to cause "changed" records, we read the item from the DB again
        //   altough this needs a little extra CPU performance
        #ifdef RECORDHASH_FROM_DBAPI
        if (getItemCRCByID(localid,crc)!=LOCERR_OK) {
          // we don't find the item with that localID.
          // Probably it has got another localID due to DB-internal reasons
          // (such as POOM changing a recurring appointment)
          // - handle this as if it was ok. Next session's preflight will find
          //   that the item is gone and will send a delete to server, and
          //   will also find a new item (this one under new localid) and add
          //   this to the server.
          crc=0;
          DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("Item has probably changed its localid during replace, CRC gets invalid"));
        }
        #else
        TSyncItem *readbackItemP=NULL;
        if (getItemByID(localid,readbackItemP)!=LOCERR_OK) {
          // we don't find the item with that localID.
          // Probably it has got another localID due to DB-internal reasons
          // (such as POOM changing a recurring appointment)
          // - handle this as if it was ok. Next session's preflight will find
          //   that the item is gone and will send a delete to server, and
          //   will also find a new item (this one under new localid) and add
          //   this to the server.
          crc=0;
          DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("Item has probably changed its localid during replace, CRC gets invalid"));
        }
        else {
          // Note: we don't need to set localID in item as it is not used in the CRC
          crc = readbackItemP->getDataCRC(0,true); // start new CRC, do not include eqm_none fields
        }
        if (readbackItemP) delete readbackItemP;
        #endif // not RECORDHASH_FROM_DBAPI
      }
    }
    // - search changelog entry
    sInt32 logindex=-1;
    TChangeLogEntry newentry;
    TChangeLogEntry *affectedentryP = &newentry;
    memset(&newentry, 0, sizeof(newentry));
    for (uInt32 i=0; i<fLoadedChangeLogEntries; i++) {
      if (LOCALID_EQUAL(fLoadedChangeLog[i].dbrecordid,localid)) {
        logindex=i;
        affectedentryP=&(fLoadedChangeLog[i]);
        break;
      }
    }
    // now affectedentryP points to where we need to apply the changed crc and modcount
    // if logindex<0 we need to add the entry to the dbfile afterwards
    affectedentryP->dataCRC=crc;
    if (reportAsChangedInNextSync) {
      // make sure NEXT sync will catch this again, as stored version is different
      // from what we received (merged with pre-existing duplicate)
      affectedentryP->modcount=fCurrentModCount+1;
      fHasPendingChanges=true;
    }
    else {
      // just current mod count
      affectedentryP->modcount=fCurrentModCount;
    }
    if (sop==sop_delete)
      affectedentryP->flags |= chgl_deleted;
    // set receiveOnly flag if needed. Note that this flag, once set, is never deleted (so item stays write only)
    if (receiveOnly)
      affectedentryP->flags |= chgl_receive_only;
    // set special flag which prevents that this change gets sent back in a resume as a
    // "modified or added after last suspend" type record (which it technically is, but we
    // use the flag to prevent that it is sent back if this session is suspended and resumed later.
    // Note that real adds and changes happening during suspend will also get this flag set
    // (but also receive a modcount>fCurrentModCount that makes sure these will be reported in
    // the next non-resumed session).
    affectedentryP->flags |= chgl_modbysync;
    // add to changelog DB if needed
    if (logindex<0) {
      // new added item
      ASSIGN_LOCALID_TO_FLD(affectedentryP->dbrecordid,localid);
      // also record the time this entry was created
      affectedentryP->modcount_created=fCurrentModCount;
      // save it
      #ifdef NUMERIC_LOCALIDS
      DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
        "new entry %ld : localID=%ld, flags=0x%X, modcount=modcount_created=%ld, new dataCRC=0x%hX",
        (long)fChangeLog.getNumRecords(),
        affectedentryP->dbrecordid,
        (int)affectedentryP->flags,
        (long)affectedentryP->modcount,
        affectedentryP->dataCRC
      ));
      #else
      DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
        "new entry %ld : localID='%s', flags=0x%X, modcount=modcount_created=%ld, new dataCRC=0x%hX",
        (long)fChangeLog.getNumRecords(),
        affectedentryP->dbrecordid,
        (int)affectedentryP->flags,
        (long)affectedentryP->modcount,
        affectedentryP->dataCRC
      ));
      #endif
      // Note: changeLogPostflight() will check these for temporary localids and finalize them when needed
      fChangeLog.newRecord(affectedentryP);
    }
    if (reportAsDeletedInNextSync) {
      fHasPendingChanges=true;
      #ifdef NUMERIC_LOCALIDS
      // assumes that valid local IDs are positive
      // and can hold values in the range of -1 to -RAND_MAX-1
      localid_t fakelocalid = -(rand() & 0x7FFFFFFF) - 1;
      #else
      char fakelocalid[STRING_LOCALID_MAXLEN];
      static unsigned int fakeidcounter;
      sprintf(fakelocalid, "fake-%u-%d", ++fakeidcounter, rand());
      #endif
      // Checking for collisions with other fake IDs would be nice,
      // but isn't easy because new entries in fChangeLog are not
      // in memory. We have to trust the random number generator
      // and the sequence counter (for string IDs).
      memset(&newentry, 0, sizeof(newentry));
      ASSIGN_LOCALID_TO_FLD(newentry.dbrecordid,fakelocalid);
      ASSIGN_LOCALID_TO_ITEM(*aItemP,fakelocalid);
      newentry.flags = chgl_deleted;
      // send during next sync
      newentry.modcount_created =
        newentry.modcount = fCurrentModCount+1;
      #ifdef NUMERIC_LOCALIDS
      DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
        "fake new entry %ld : localID=%ld, flags=0x%X, modcount=modcount_created=%ld, new dataCRC=0x%hX",
        (long)fChangeLog.getNumRecords(),
        newentry.dbrecordid,
        (int)newentry.flags,
        (long)newentry.modcount,
        newentry.dataCRC
      ));
      #else
      DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
        "fake new entry %ld : localID='%s', flags=0x%X, modcount=modcount_created=%ld, new dataCRC=0x%hX",
        (long)fChangeLog.getNumRecords(),
        newentry.dbrecordid,
        (int)newentry.flags,
        (long)newentry.modcount,
        newentry.dataCRC
      ));
      #endif
      fChangeLog.newRecord(&newentry);
    }
    // done
    ok=true;
    goto done;
  }
  SYSYNC_CATCH (exception &e)
    statuscode=500;
    DEBUGPRINTFX(DBG_ERROR,("******** Exception in TBinfileImplDS::ProcessItem: %s (status=%ld)",e.what(), (long)statuscode));
    ok=false;
    goto done;
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    DEBUGPRINTFX(DBG_ERROR,("******** Exception in TBinfileImplDS::ProcessItem"));
    statuscode=500;
    ok=false;
    goto done;
  SYSYNC_ENDCATCH
error:
  // report OS specific error codes as item text back to the originator
  ok=false;
  PDEBUGPRINTFX(DBG_ERROR,("Database Error --> SyncML status %ld%s",(long)statuscode,lastDBErrorText().c_str()));
  //aStatusCommand.addItemString(lastDBErrorDbgText().c_str());
done:
  delete augmentedItemP;
  aStatusCommand.setStatusCode(statuscode);
  return ok;
} // TBinfileImplDS::implProcessItem


// called to confirm a sync operation's completion (status from remote received)
// @note aSyncOp passed not necessarily reflects what was sent to remote, but what actually happened
void TBinfileImplDS::dsConfirmItemOp(TSyncOperation aSyncOp, cAppCharP aLocalID, cAppCharP aRemoteID, bool aSuccess, localstatus aErrorStatus)
{
  if (binfileDSActive()) {
    // Nothing to do here, even successful deletes must not delete changelog entry (this will be done
    // by changeLogPostFlight() for those enties that have been reported as deleted in all profiles!)
  }
  // - let inherited know as well
  inherited::dsConfirmItemOp(aSyncOp, aLocalID, aRemoteID, aSuccess, aErrorStatus);
} // TBinfileImplDS::confirmItemOp


// - save status information required to possibly perform a resume (as passed to datastore with
//   implMarkOnlyUngeneratedForResume() and implMarkItemForResume())
//   (or, in case the session is really complete, make sure that no resume state is left)
localstatus TBinfileImplDS::implSaveResumeMarks(void)
{
  if (!binfileDSActive()) return LOCERR_WRONGUSAGE; // must be active when called at all

  // update modcount reference of last suspend
  fPreviousSuspendModCount = fCurrentModCount;
  // save admin data now
  return SaveAdminData(true,false); // end of session, but not successful
} // TBinfileImplDS::implSaveResumeMarks



// log datastore sync result
// - Called at end of sync with this datastore
void TBinfileImplDS::dsLogSyncResult(void)
{
  // Note: binfile logs can be active even if binfiles layer otherwise is not active
  TBinfileClientConfig *clientCfgP = static_cast<TBinfileClientConfig *>(fSessionP->getSessionConfig());
  if (clientCfgP->fBinFileLog) {
    // writing binfile logs enabled
    TBinFile logFile;
    // Open logfile
    // - get base path
    string filepath;
    clientCfgP->getBinFilesPath(filepath);
    filepath += LOGFILE_DB_NAME;
    // - open or create
    logFile.setFileInfo(filepath.c_str(),LOGFILE_DB_VERSION,LOGFILE_DB_ID,sizeof(TLogFileEntry));
    if (logFile.open(0,NULL,NULL)!=BFE_OK) {
      // create new one or overwrite incompatible one
      logFile.create(sizeof(TLogFileEntry),0,NULL,true);
    }
    // append new record
    // - create record
    TLogFileEntry logInfo;
    logInfo.time = fCurrentSyncTime; // current sync's time
    logInfo.status = getAbortStatusCode(); // reason for abort (0 if ok)
    logInfo.mode =
      (fSlowSync ? (fFirstTimeSync ? 2 : 1) : 0) +
      (fResuming ? 10 : 0);
    logInfo.dirmode = fSyncMode; // sync direction mode
    logInfo.infoID = 0; // none
    logInfo.dbID = fConfigP->fLocalDBTypeID; // ID of DB (to allow fetching name)
    logInfo.profileID = static_cast<TBinfileImplClient *>(fSessionP)->fRemotepartyID; // ID of profile (to allow fetching name)
    logInfo.locAdded = fLocalItemsAdded;
    logInfo.locUpdated = fLocalItemsUpdated;
    logInfo.locDeleted = fLocalItemsDeleted;
    logInfo.remAdded = fRemoteItemsAdded;
    logInfo.remUpdated = fRemoteItemsUpdated;
    logInfo.remDeleted = fRemoteItemsDeleted;
    logInfo.inBytes = fIncomingDataBytes;
    logInfo.outBytes = fOutgoingDataBytes;
    logInfo.locRejected = fLocalItemsError;
    logInfo.remRejected = fRemoteItemsError;
    // - save it
    logFile.newRecord(&logInfo);
    // close the logfile again
    logFile.close();
  }
} // TBinfileImplDS::dsLogSyncResult


// save end of session state
// Note that in BASED_ON_BINFILE_CLIENT case, this is derived in customimplds and will
// be called by the derivate after doing customimpl specific stuff.
localstatus TBinfileImplDS::implSaveEndOfSession(bool aUpdateAnchors)
{
  if (!binfileDSActive()) return LOCERR_WRONGUSAGE; // must be active when called at all

  // update TCustomImplDS dsSavedAdmin variables (other levels have already updated their variables
  if (aUpdateAnchors) {
    if (!fRefreshOnly || fSlowSync) {
      // This was really a two-way sync or we implicitly know that
      // we are now in sync with remote (like after one-way-from-remote refresh = reload local)
      fPreviousToRemoteModCount = fCurrentModCount;
    }
    // updating anchor means invalidating last Suspend
    fPreviousSuspendCmpRef = fPreviousToRemoteSyncCmpRef; // setting to current reference can do less harm than setting it to zero
    fPreviousSuspendModCount=0;
  }
  // save admin data now
  localstatus sta=SaveAdminData(true,aUpdateAnchors); // end of session
  if (sta==LOCERR_OK) {
    // finalize admin data stuff now
    uInt32 oldestmodcount=0xFFFFFFFF;
    // do a postFlight to remove unused entries from the changelog
    if (static_cast<TBinfileClientConfig *>(fSessionP->getSessionConfig())->fSeparateChangelogs) {
      // Each profile has it's own changelog, so just delete entries that are older than this profile's last sync or resume
      if (fTarget.lastSuspendModCount!=0)
        oldestmodcount = fTarget.lastSuspendModCount;
      if (fTarget.lastTwoWayModCount!=0 && fTarget.lastTwoWayModCount<oldestmodcount)
        oldestmodcount = fTarget.lastTwoWayModCount;
    }
    else {
      // Combined changelog for all profiles, need to keep deleted markers for other profiles
      // - find oldest sync modcount
      TBinFile *targetsBinFileP = &(static_cast<TBinfileImplClient *>(fSessionP)->fConfigP->fTargetsBinFile);
      uInt32 maxidx = targetsBinFileP->getNumRecords();
      uInt32 idx;
      TBinfileDBSyncTarget target;
      for (idx=0; idx<maxidx; idx++) {
        targetsBinFileP->readRecord(idx,&target);
        if (
          // Note: %%% this is same mechanism as for changelog filenames
          //       Not ok if multiple databases go with the same datastore.getName()
          strucmp(target.dbname,getName())==0
        ) {
          // target for this database found, check if modcount is older
          if (target.lastSuspendModCount!=0 && target.lastSuspendModCount<oldestmodcount)
            oldestmodcount=target.lastSuspendModCount; // older one found
          if (target.lastTwoWayModCount!=0 && target.lastTwoWayModCount<oldestmodcount)
            oldestmodcount=target.lastTwoWayModCount; // older one found
        }
      }
    } // combined changelog
    // For both types: Now do cleanup of all deleted entries that we don't need any more
    sta=changeLogPostflight(oldestmodcount);
  }
  // done
  return sta;
} // TBinfileImplDS::implSaveEndOfSession


// - end write with commit
bool TBinfileImplDS::implEndDataWrite(void)
{
  if (!binfileDSActive()) return LOCERR_WRONGUSAGE; // must be active when called at all

  // Call apiEndDataWrite variant which is possibly implemented in
  // datastores which were designed as direct derivates of binfileds.
  // Note that in BASED_ON_BINFILE_CLIENT case, implEndDataWrite() is
  // derived by customimplds and will call the other apiEndDataWrite(cmpRef)
  // variant, and then call this inherited method.
  return apiEndDataWrite()!=LOCERR_OK;
} // TBinfileImplDS::implEndDataWrite



// private helper to prepare for apiSaveAdminData()
localstatus TBinfileImplDS::SaveAdminData(bool aSessionFinished, bool aSuccessful)
{
  PDEBUGBLOCKDESCCOLL("SaveAdminData","Saving changelog, target and map info");
  // save and free cached changelog anyway
  if (fLoadedChangeLog) {
    #ifdef SYDEBUG
    PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,("SaveAdminData: saving changelog with %ld entries",(long)fLoadedChangeLogEntries));
    if (DEBUGTEST(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC)) {
      for (uInt32 si=0; si<fLoadedChangeLogEntries; si++) {
        #ifdef NUMERIC_LOCALIDS
        PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
          "%ld : localID=%ld, flags=0x%X, modcount=%ld, modcount_created=%ld",
          (long)si,
          fLoadedChangeLog[si].dbrecordid,
          (int)fLoadedChangeLog[si].flags,
          (long)fLoadedChangeLog[si].modcount,
          (long)fLoadedChangeLog[si].modcount_created
        ));
        #else
        PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
          "%ld : localID='%s', flags=0x%X, modcount=%ld, modcount_created=%ld",
          (long)si,
          fLoadedChangeLog[si].dbrecordid,
          (int)fLoadedChangeLog[si].flags,
          (long)fLoadedChangeLog[si].modcount,
          (long)fLoadedChangeLog[si].modcount_created
        ));
        #endif
      }
    }
    #endif
    fChangeLog.updateRecord(0,fLoadedChangeLog,fLoadedChangeLogEntries);
    forgetChangeLog();
  }
  // save the "last changelog update" identifier into the changelog header
  //   Note: fPreviousToRemoteSyncCmpRef/fPreviousToRemoteSyncIdentifier represent last update of changelog
  //   in binfileds, rather than last to-remote-sync as in non-binfile based setups.
  // - update the identifier (token for StartDataRead)
  AssignCString(fChgLogHeader.lastChangeCheckIdentifier, fPreviousToRemoteSyncIdentifier.c_str(),changeIndentifierMaxLen);
  // - update compare reference time for next preflight if fCmpRefTimeStampAtEnd is set (default = defined(SYNCTIME_IS_ENDOFSESSION))
  //   Note: preflight has already set fChgLogHeader.lastChangeCheck to the beginning of the sync
  //         Update it here only if synctime must be end of session.
  if (fConfigP->fCmpRefTimeStampAtEnd) {
    fChgLogHeader.lastChangeCheck = getSession()->getSystemNowAs(TCTX_UTC); // NOW ! (again);
  }
  fChangeLog.setExtraHeaderDirty();
  fChangeLog.flushHeader();
  // save other admin data
  TBinFile *targetsBinFileP = &(static_cast<TBinfileImplClient *>(fSessionP)->fConfigP->fTargetsBinFile);
  // update target fields
  // - update anchor
  AssignCString(fTarget.remoteAnchor,fLastRemoteAnchor.c_str(),remoteAnchorMaxLen);
  PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("SaveAdminData: saving remote anchor = '%s'",fLastRemoteAnchor.c_str()));
  // - update this sync time (last sync field in DB) and modcount
  fTarget.lastSync=fPreviousSyncTime;
  fTarget.lastTwoWayModCount=fPreviousToRemoteModCount;
  // Note: compare times and identifiers are not needed any more (these are now in the changelog),
  //       But we assigne some for dbg purposes.
  // - update the last changelog check time.
  fTarget.lastChangeCheck=fPreviousToRemoteSyncCmpRef;
  #if TARGETS_DB_VERSION>=6
  // - identifiers (tokens for StartDataRead)
  PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,("SaveAdminData: saving target sync token %s", fPreviousToRemoteSyncIdentifier.c_str()));
  AssignCString(fTarget.dummyIdentifier1,fPreviousToRemoteSyncIdentifier.c_str(),remoteAnchorMaxLen); // former lastSyncIdentifier
  AssignCString(fTarget.dummyIdentifier2,NULL,remoteAnchorMaxLen); // former lastSuspendIdentifier, not needed, make empty
  // store remote datastore's display name (is empty if we haven't got one from the remote via devInf)
  if (getRemoteDatastore()) {
    AssignCString(fTarget.remoteDBdispName,getRemoteDatastore()->getDisplayName(),dispNameMaxLen);
  }
  #endif
  // check for other target record updates needed at end of session
  if (aSessionFinished && aSuccessful) {
    // - reset mode back to normal, that is after a forced reload of client OR server
    //   we assume that two-way sync will be what we want next.
    if (fTarget.forceSlowSync) {
      fTarget.syncmode = smo_twoway;
    }
    fTarget.forceSlowSync=false;
  }
  // special operations needed depending on suspend state
  fTarget.resumeAlertCode=fResumeAlertCode;
  if (fResumeAlertCode==0) {
    fTarget.lastSuspendModCount = 0;
  }
  else {
    /// @note: lastSuspendModCount is the same target field that previously was called "lastModCount"
    fTarget.lastSuspendModCount = fPreviousSuspendModCount;
    if (!static_cast<TBinfileClientConfig *>(fSessionP->getSessionConfig())->fSeparateChangelogs) {
      // Combined changelogs: make sure that resume alert codes of all other profile's targets for this datastore are erased
      // (because in a single changelog there is only one set of markforresume flags and single pendingmap+pendingitem files)
      TBinfileDBSyncTarget otherTarget;
      memset(&otherTarget, 0, sizeof(otherTarget));
      for (sInt32 ti=0; ti<sInt32(targetsBinFileP->getNumRecords()); ti++) {
        if (ti!=fTargetIndex) {
          // get that target
          targetsBinFileP->readRecord(ti,&otherTarget);
          if (
            (otherTarget.localDBTypeID == fTarget.localDBTypeID) && // same datastoreID
            (strucmp(otherTarget.localDBPath,fTarget.localDBPath)==0) && // ..and name
            (otherTarget.resumeAlertCode!=0) // ..and has a saved suspend state
          ) {
            // same datastore, but different profile is also suspended -> new suspend cancels old one
            DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,("SaveAdminData: cancelled resumeAlertCode in profile=%ld",(long)otherTarget.remotepartyID));
            otherTarget.resumeAlertCode=0;
            otherTarget.lastSuspendModCount=0;
            targetsBinFileP->updateRecord(ti,&otherTarget);
          }
        }
      }
    }
  }
  PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI,("SaveAdminData: lastTwoWayModCount = %ld, lastSuspendModCount = %ld", (long)fPreviousToRemoteModCount, (long)fPreviousSuspendModCount));
  // save pending maps (anyway, even if not suspended)
  openPendingMaps(); // make sure we have the pendingmap file open now
  fPendingMaps.truncate(0); // delete current contents
  TStringToStringMap::iterator spos;
  TPendingMapEntry pme;
  localid_t localid;
  // - pending maps now belong to us!
  //   Note: they always do with separated changelogs
  fPendingMapHeader.remotepartyID = static_cast<TBinfileImplClient *>(fSessionP)->fRemotepartyID;
  fPendingMaps.setExtraHeaderDirty();
  // - now pending maps (unsent ones)
  DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_DETAILS,("SaveAdminData: saving %ld entries from fPendingAddMap to fPendingMaps binfile",(long)fPendingAddMaps.size()));
  for (spos=fPendingAddMaps.begin();spos!=fPendingAddMaps.end();spos++) {
    string locID = (*spos).first;
    dsFinalizeLocalID(locID); // pending maps might have non-final ID, so give datastore implementation to return finalized version
    STR_TO_LOCALID(locID.c_str(),localid); ASSIGN_LOCALID_TO_FLD(pme.dbrecordid,localid);
    AssignCString(pme.remoteID,(*spos).second.c_str(),BINFILE_MAXGUIDSIZE+1);
    fPendingMaps.newRecord(&pme);
  }
  // - now pending maps (sent, but not seen status yet)
  DEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_DETAILS,("SaveAdminData: saving %ld entries from fUnconfirmedMaps to fPendingMaps binfile",(long)fUnconfirmedMaps.size()));
  for (spos=fUnconfirmedMaps.begin();spos!=fUnconfirmedMaps.end();spos++) {
    STR_TO_LOCALID((*spos).first.c_str(),localid); ASSIGN_LOCALID_TO_FLD(pme.dbrecordid,localid);
    AssignCString(pme.remoteID,(*spos).second.c_str(),BINFILE_MAXGUIDSIZE+1);
    fPendingMaps.newRecord(&pme);
  }
  // - save them
  fPendingMaps.close();
  // Save last item info and possibly partial item data
  TPendingItemHeader pendingItemHeader;
  memset((void*)&pendingItemHeader,0,sizeof(pendingItemHeader)); // make nicely empty by default
  bool saveit=false;
  // - profile ID where this pending item belongs to
  pendingItemHeader.remotepartyID = static_cast<TBinfileImplClient *>(fSessionP)->fRemotepartyID;
  // determine if and what to save
  if (fPartialItemState==pi_state_none) {
    pendingItemHeader.piState = pi_state_none;
    pendingItemHeader.storedSize=0; // no data to store
    saveit=true;
  }
  else if (fPartialItemState==pi_state_save_incoming || fPartialItemState==pi_state_save_outgoing) {
    // - last item info
    pendingItemHeader.lastItemStatus = fLastItemStatus;
    AssignCString(pendingItemHeader.lastSourceURI,fLastSourceURI.c_str(),BINFILE_MAXGUIDSIZE+1);
    AssignCString(pendingItemHeader.lastTargetURI,fLastTargetURI.c_str(),BINFILE_MAXGUIDSIZE+1);
    // - partial item info
    pendingItemHeader.totalSize = fPITotalSize;
    pendingItemHeader.unconfirmedSize = fPIUnconfirmedSize;
    if (fPartialItemState==pi_state_save_incoming) {
      // store incoming
      pendingItemHeader.piState = pi_state_loaded_incoming;
      pendingItemHeader.storedSize=fPIStoredSize;
    }
    else if (fPartialItemState==pi_state_save_outgoing) {
      // store outgoing
      pendingItemHeader.piState = pi_state_loaded_outgoing;
      pendingItemHeader.storedSize=fPIStoredSize;
    }
    else
      pendingItemHeader.storedSize=0; // nothing to store
    saveit=true;
  }
  if (saveit) {
    TBinFile pendingItemFile;
    string fname = static_cast<TBinfileClientConfig *>(fSessionP->getSessionConfig())->relatedDBNameBase(getName(), fTarget.remotepartyID);
    fname += PENDINGITEM_DB_SUFFIX;
    pendingItemFile.setFileInfo(fname.c_str(),PENDINGITEM_DB_VERSION,PENDINGITEM_DB_ID,0);
    PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_EXOTIC,(
      "SaveAdminData: creating pending item file: file name='%s', storing %ld bytes",
      fname.c_str(),
      (long)pendingItemHeader.storedSize
    ));
    PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_DETAILS,(
      "SaveAdminData: saved pending item: src='%s', targ='%s', laststatus=%hd, pistate=%d, total=%ld, unconfirmed=%ld, stored=%ld",
      pendingItemHeader.lastSourceURI,
      pendingItemHeader.lastTargetURI,
      pendingItemHeader.lastItemStatus,
      pendingItemHeader.piState,
      (long)pendingItemHeader.totalSize,
      (long)pendingItemHeader.unconfirmedSize,
      (long)pendingItemHeader.storedSize
    ));
    bferr bfe=pendingItemFile.create(
      pendingItemHeader.storedSize, // record size = size of data chunk to be buffered
      sizeof(TPendingItemHeader), // extra header size
      &pendingItemHeader, // extra header data
      true // overwrite existing
    );
    if (bfe==BFE_OK) {
      // created successfully, store data if any
      if (pendingItemHeader.storedSize && fPIStoredDataP) {
        // we have data to store
        uInt32 newIndex;
        bfe=pendingItemFile.newRecord(newIndex,fPIStoredDataP);
      }
      // close file
      pendingItemFile.close();
    }
    if (bfe!=BFE_OK) {
      PDEBUGPRINTFX(DBG_ERROR,("Error writing pending item file, bferr=%hd",bfe));
    }
  }
  PDEBUGPRINTFX(DBG_ADMIN+DBG_DBAPI+DBG_DETAILS,("SaveAdminData: fTargetIndex %d resumeAlertCode = %hd, lastSuspendModCount = %ld",fTargetIndex,fResumeAlertCode,(long)fTarget.lastSuspendModCount));
  // update the target record
  if (fTargetIndex>=0) {
    targetsBinFileP->updateRecord(fTargetIndex,&fTarget);
    fTargetIndex=-1; // invalid now
  }
  PDEBUGENDBLOCK("SaveAdminData");
  // ok
  return LOCERR_OK;
} // TBinfileImplDS::SaveAdminData


/* end of TBinfileImplDS implementation */

} // namespace sysync
// eof
