/**
 *  @File     binfileimplclient.cpp
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TBinfileImplClient
 *    Represents a client session (agent) that saves profile, target, resume info
 *    and optionally changelog in TBinFile binary files
 *
 *    Copyright (c) 2003-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-09-30 : luz : created from TBinfileImplClient
 */
/*
 */

// includes
#include "prefix_file.h"
#include "binfileimplclient.h"
#include "binfileimplds.h"
#include "syserial.h"
#include <cstddef>


namespace sysync {


// Support for EngineModule common interface
// =========================================

#ifdef ENGINEINTERFACE_SUPPORT

#ifndef ENGINE_LIBRARY
#ifdef RELEASE_VERSION
  #error "this is here for Q&D testing with outlook client only"
#endif
// factory function implementation - declared in TEngineInterface
ENGINE_IF_CLASS *newClientEngine(void)
{
  return new TBinfileEngineInterface;
} // newEngine

#ifdef RELEASE_VERSION
  #error "this is only here for Q&D testing with outlook client - remove it later, global factory function may no longer be used by engineInterface targets!!"
#else
/// @brief returns a new application base.
TSyncAppBase *TBinfileEngineInterface::newSyncAppBase(void)
{
  return sysync::newSyncAppBase(); // use global factory function
} // TBinfileEngineInterface::newSyncAppBase
#endif

#endif // not ENGINE_LIBRARY



// create appropriate root key
TSettingsKeyImpl *TBinfileEngineInterface::newSettingsRootKey(void)
{
  return new TBinfileAgentRootKey(this); // return base class which can return some engine infos
} // TBinfileEngineInterface::newSettingsRootKey



// Target key
// ----------

// constructor
TBinfileTargetKey::TBinfileTargetKey(
  TEngineInterface *aEngineInterfaceP,
  sInt32 aTargetIndex,
  TBinfileDBSyncTarget *aTargetP,
  TBinfileClientConfig *aBinfileClientConfigP,
  TBinfileDSConfig *aBinfileDSConfigP
) :
  inherited(aEngineInterfaceP),
  fTargetIndex(aTargetIndex),
  fTargetP(aTargetP),
  fBinfileClientConfigP(aBinfileClientConfigP),
  fBinfileDSConfigP(aBinfileDSConfigP)
{
  // nop
} // TBinfileTargetKey::TBinfileTargetKey


// destructor - close key
TBinfileTargetKey::~TBinfileTargetKey()
{
  // closing key
  if (fTargetP) {
    if (fDirty && fTargetIndex>=0) {
      // write back changed record
      fBinfileClientConfigP->writeTarget(fTargetIndex,*fTargetP);
    }
    // now delete the target record
    delete fTargetP;
  }
} // TBinfileTargetKey::~TBinfileTargetKey


// return ID of current key
TSyError TBinfileTargetKey::GetKeyID(sInt32 &aID)
{
  aID = fTargetP ? fTargetP->localDBTypeID : KEYVAL_ID_UNKNOWN;
  return LOCERR_OK;
} // TBinfileTargetKey::GetKeyID


// - read display name
static TSyError readDispName(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  // get from config
  TBinfileTargetKey *targetKeyP = static_cast<TBinfileTargetKey *>(aStructFieldsKeyP);
  cAppCharP dispName = "";
  #ifndef MINIMAL_CODE
  dispName = targetKeyP->getBinfileDSConfig()->fDisplayName.c_str();
  #endif
  if (*dispName==0) {
    // no display name, get technical name instead
    dispName = targetKeyP->getBinfileDSConfig()->getName();
  }
  return TStructFieldsKey::returnString(dispName, aBuffer, aBufSize, aValSize);
} // readDispName


// - read path where binfiles are, for plugin config files passed around engine
static TSyError readBinFileDirTarget(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TBinfileTargetKey *targetKeyP = static_cast<TBinfileTargetKey *>(aStructFieldsKeyP);
  // get info
  string binFilesDir;
  targetKeyP->getBinfileClientConfig()->getBinFilesPath(binFilesDir);
  return TStructFieldsKey::returnString(binFilesDir.c_str(), aBuffer, aBufSize, aValSize);
} // readBinFileDirTarget


// - read availability flag for this datastore
static TSyError readIsAvailable(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TBinfileTargetKey *targetKeyP = static_cast<TBinfileTargetKey *>(aStructFieldsKeyP);
  // - get profile by ID
  bool avail = false;
  if (targetKeyP && targetKeyP->getTarget()) {
    TBinfileDBSyncProfile profile;
    targetKeyP->getBinfileClientConfig()->getProfileByID(targetKeyP->getProfileID(),profile);
    avail = targetKeyP->getBinfileClientConfig()->isTargetAvailable(&profile,targetKeyP->getTarget()->localDBTypeID);
  }
  return TStructFieldsKey::returnInt(avail, sizeof(avail), aBuffer, aBufSize, aValSize);
} // readIsAvailable


// - read danger flags for this datastore (will return them even if not enabled)
static TSyError readTargetDangerFlags(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TBinfileTargetKey *targetKeyP = static_cast<TBinfileTargetKey *>(aStructFieldsKeyP);
  bool zapsServer, zapsClient;
  lineartime_t lastSync;
  uInt32 dbid;
  // get info
  uInt8 danger=0;
  TBinfileDBSyncTarget *targetP;
  if (targetKeyP && (targetP=targetKeyP->getTarget())) {
    targetKeyP->getBinfileClientConfig()->getTargetLastSyncTime(*targetP, lastSync, zapsServer, zapsClient, dbid);
    danger =
      (zapsServer ? DANGERFLAG_WILLZAPSERVER : 0) +
      (zapsClient ? DANGERFLAG_WILLZAPCLIENT : 0);
  }
  return TStructFieldsKey::returnInt(danger, sizeof(danger), aBuffer, aBufSize, aValSize);
} // readTargetDangerFlags


// - write a readonly bitmask (rdonly_xxx), if this returns LOCERR_OK, readonly is enabled
static TSyError writeTargetReadOnlyCheck(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  TBinfileTargetKey *targetKeyP = static_cast<TBinfileTargetKey *>(aStructFieldsKeyP);
  // get readonly mask to check for
  uInt16 readOnlyMask = *((uInt16 *)aBuffer);
  // check in profile
  bool rdOnly=false;
  if (targetKeyP && targetKeyP->getTarget()) {
    TBinfileDBSyncProfile profile;
    targetKeyP->getBinfileClientConfig()->getProfileByID(targetKeyP->getProfileID(),profile);
    rdOnly = targetKeyP->getBinfileClientConfig()->isReadOnly(
      &profile,
      readOnlyMask
    );
  }
  // if feature is enabled for this profile, return LOCERR_OK, DB_NoContent otherwise
  return rdOnly ? LOCERR_OK : DB_NoContent;
} // writeTargetReadOnlyCheck


// - write a feature number (APPFTR_xxx), if this returns LOCERR_OK, feature is available, otherwise not
static TSyError writeTargetFeatureCheck(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  TBinfileTargetKey *targetKeyP = static_cast<TBinfileTargetKey *>(aStructFieldsKeyP);
  // get feature to check for
  uInt16 featureNo = *((uInt16 *)aBuffer);
  // extra check for those that only make sense together with certain fDSAvailFlag
  bool avail=true;
  uInt16 dsAvailFlags = targetKeyP->getBinfileDSConfig()->fDSAvailFlag;
  if (featureNo==APP_FTR_EVENTRANGE && (dsAvailFlags & dsavail_events)==0)
    avail=false; // event range makes sense for events only
  if (featureNo==APP_FTR_EMAILRANGE && (dsAvailFlags & dsavail_emails)==0)
    avail=false; // email range makes sense for emails only
  // check on profile (and global) level
  if (avail && targetKeyP && targetKeyP->getTarget()) {
    TBinfileDBSyncProfile profile;
    targetKeyP->getBinfileClientConfig()->getProfileByID(targetKeyP->getProfileID(),profile);
    avail = targetKeyP->getBinfileClientConfig()->isFeatureEnabled(
      &profile,
      featureNo
    );
  }
  // if feature is enabled for this profile, return LOCERR_OK, DB_NoContent otherwise
  return avail ? LOCERR_OK : DB_NoContent;
} // writeTargetFeatureCheck


// macro simplifying typing in the table below
#define OFFS_SZ_TG(n) (offsetof(TBinfileDBSyncTarget,n)), sizeof(dP_tg->n)
// dummy pointer needed for sizeof
static const TBinfileDBSyncTarget *dP_tg=NULL;

// accessor table for target key and for TARGETSETTING() script function
const TStructFieldInfo TargetFieldInfos[] =
{
  // valName, valType, writable, fieldOffs, valSiz
  { "profileID", VALTYPE_INT32, false, OFFS_SZ_TG(remotepartyID) }, // read-only profile ID
  { "enabled", VALTYPE_ENUM, true, OFFS_SZ_TG(enabled) },
  { "forceslow", VALTYPE_ENUM, true, OFFS_SZ_TG(forceSlowSync) },
  { "syncmode", VALTYPE_ENUM, true, OFFS_SZ_TG(syncmode) },
  { "limit1", VALTYPE_INT32, true, OFFS_SZ_TG(limit1) },
  { "limit2", VALTYPE_INT32, true, OFFS_SZ_TG(limit2) },
  { "extras", VALTYPE_INT32, true, OFFS_SZ_TG(extras) },
  { "localpath", VALTYPE_TEXT, true, OFFS_SZ_TG(localDBPath) },
  { "remotepath", VALTYPE_TEXT, true, OFFS_SZ_TG(remoteDBpath) },
  #if defined(DESKTOP_CLIENT) || TARGETS_DB_VERSION>4
  { "localcontainer", VALTYPE_TEXT, true, OFFS_SZ_TG(localContainerName) },
  #endif
  // read-only status info
  { "dbname", VALTYPE_TEXT, false, OFFS_SZ_TG(dbname) },
  { "lastSync", VALTYPE_TIME64, false, OFFS_SZ_TG(lastSync) },
  { "lastChangeCheck", VALTYPE_TIME64, false, OFFS_SZ_TG(lastChangeCheck) },
  { "resumeAlertCode", VALTYPE_INT16, true, OFFS_SZ_TG(resumeAlertCode) },
  // programmatic danger flags (will sync zap client or server data?)
  { "dangerFlags", VALTYPE_INT8, false, 0,0, &readTargetDangerFlags, NULL },
  // special programmatic availability and feature checks
  { "isAvailable", VALTYPE_ENUM, false, 0,0, &readIsAvailable, NULL }, // datastore availability
  { "checkForReadOnly", VALTYPE_INT16, true, 0,0, NULL, &writeTargetReadOnlyCheck }, // target level readonly check
  { "checkForFeature", VALTYPE_INT16, true, 0,0, NULL, &writeTargetFeatureCheck }, // target level feature check
  // display name from config
  { "dispName", VALTYPE_TEXT, false, 0,0, &readDispName, NULL },
  // path where binfiles are, for plugin config files passed around engine
  { "binfilesDir", VALTYPE_TEXT, false, 0,0, &readBinFileDirTarget, NULL },
  // new fields of TARGETS_DB_VERSION 6 and beyond
  #if TARGETS_DB_VERSION>5
  { "remoteDispName", VALTYPE_TEXT, false, OFFS_SZ_TG(remoteDBdispName) },
  // filtering
  { "filterCapDesc", VALTYPE_TEXT, false, OFFS_SZ_TG(filterCapDesc) },
  { "remoteFilters", VALTYPE_TEXT, true, OFFS_SZ_TG(remoteFilters) },
  { "localFilters", VALTYPE_TEXT, true, OFFS_SZ_TG(localFilters) },
  #endif
};
const sInt32 numTargetFieldInfos = sizeof(TargetFieldInfos)/sizeof(TStructFieldInfo);

// get table describing the fields in the struct
const TStructFieldInfo *TBinfileTargetKey::getFieldsTable(void)
{
  return TargetFieldInfos;
} // TBinfileTargetKey::getFieldsTable

sInt32 TBinfileTargetKey::numFields(void)
{
  return numTargetFieldInfos;
} // TBinfileTargetKey::numFields



// get actual struct base address
uInt8P TBinfileTargetKey::getStructAddr(void)
{
  return (uInt8P)fTargetP;
} // TBinfileTargetKey::getStructAddr


// Profiles container key
// ----------------------

// constructor
TBinfileTargetsKey::TBinfileTargetsKey(TEngineInterface *aEngineInterfaceP, sInt32 aProfileID) :
  inherited(aEngineInterfaceP),
  fProfileID(aProfileID),
  fTargetIterator(-1)
{
  // get pointer to BinFileClientConfig
  fBinfileClientConfigP =
    static_cast<TBinfileClientConfig *>(
      aEngineInterfaceP->getSyncAppBase()->getRootConfig()->fAgentConfigP
    );
} // TBinfileTargetsKey::TBinfileTargetsKey


// target can be opened only by (dbtype-)ID
TSyError TBinfileTargetsKey::OpenSubkey(
  TSettingsKeyImpl *&aSettingsKeyP,
  sInt32 aID, uInt16 aMode
)
{
  TBinfileDBSyncTarget *targetP = new TBinfileDBSyncTarget;
  TSyError sta = LOCERR_OK;
  sInt32 targetIndex = -1;
  // safety check to see if targets are open at all
  if (!fBinfileClientConfigP->fTargetsBinFile.isOpen())
    return LOCERR_WRONGUSAGE;
  while(true) {
    switch (aID) {
      case KEYVAL_ID_FIRST:
        fTargetIterator = 0; // go to first
        goto getnthtarget;
        // then fetch next
      case KEYVAL_ID_NEXT:
        // increment
        fTargetIterator++;
      getnthtarget:
        // get n-th target as indicated by iterator
        if (fTargetIterator>=0)
          targetIndex = fBinfileClientConfigP->findTargetIndex(fProfileID,fTargetIterator);
        if (targetIndex>=0)
          goto gettarget; // next target found in iteration, get it
        // no more targets found
        sta=DB_NoContent; // no more targets
        break;
      default:
        // get target by ID
        if (aID<0) {
          sta=LOCERR_WRONGUSAGE;
          break;
        }
        // find target by dbtypeid
        targetIndex = fBinfileClientConfigP->findTargetIndexByDBInfo(fProfileID,aID,NULL);
      gettarget:
        // get target by index
        targetIndex = fBinfileClientConfigP->getTarget(targetIndex,*targetP);
        // check error
        if (targetIndex<0)
          sta=DB_NotFound; // target not found
        break;
    }
    if (sta==LOCERR_OK && targetIndex>=0) {
      // we have loaded a target, create subkey handler and pass data
      /* %%% old: search by name
      // - find related datastore config (by dbname)
      TBinfileDSConfig *dsCfgP = static_cast<TBinfileDSConfig *>(
        fBinfileClientConfigP->getLocalDS(targetP->dbname)
      );
      */
      // - find related datastore config (by dbtypeid)
      TBinfileDSConfig *dsCfgP = static_cast<TBinfileDSConfig *>(
        fBinfileClientConfigP->getLocalDS(NULL,targetP->localDBTypeID)
      );
      if (dsCfgP==NULL) {
        // this target entry is not configured in the config -> skip it while iterating,
        // or return "DB error" if explicitly addressed
        // - re-enter iteration if iterating
        if (aID==KEYVAL_ID_FIRST)
          aID=KEYVAL_ID_NEXT;
        if (aID==KEYVAL_ID_NEXT)
          continue; // re-iterate
        // otherwise return DB error to signal inconsistency between target and config
        sta=DB_Error;
      }
      else {
        aSettingsKeyP = new TBinfileTargetKey(fEngineInterfaceP,targetIndex,targetP,fBinfileClientConfigP,dsCfgP);
        if (aSettingsKeyP)
          targetP=NULL; // ownership passed
        else
          sta=LOCERR_OUTOFMEM; // cannot create object
      }
    }
    // done, no re-iteration needed
    break;
  } // while true
  // get rid of it if we couldn't pass it to the subkey handler
  if (targetP)
    delete targetP;
  // done
  return sta;
} // TBinfileTargetsKey::OpenSubkey



#ifdef AUTOSYNC_SUPPORT

// Autosync level key
// ------------------

// constructor
TBinfileASLevelKey::TBinfileASLevelKey(
  TEngineInterface *aEngineInterfaceP,
  sInt32 aLevelIndex,
  TBinfileDBSyncProfile *aProfileP,
  TBinfileClientConfig *aBinfileClientConfigP
) :
  inherited(aEngineInterfaceP),
  fLevelIndex(aLevelIndex),
  fProfileP(aProfileP),
  fBinfileClientConfigP(aBinfileClientConfigP)
{
} // TBinfileASLevelKey::TBinfileASLevelKey


// destructor - close key
TBinfileASLevelKey::~TBinfileASLevelKey()
{
  // closing key
  // NOP here
} // TBinfileASLevelKey::~TBinfileASLevelKey


// return ID of current key
TSyError TBinfileASLevelKey::GetKeyID(sInt32 &aID)
{
  aID = fLevelIndex; // ID is level index
  return LOCERR_OK;
} // TBinfileASLevelKey::GetKeyID


// macro simplifying typing in the table below
#define OFFS_SZ_AS(n) (offsetof(TAutoSyncLevel,n)), sizeof(dP_as->n)
// dummy pointer needed for sizeof
static const TAutoSyncLevel *dP_as=NULL;

// accessor table for profiles
static const TStructFieldInfo ASLevelFieldInfos[] =
{
  // valName, valType, writable, fieldOffs, valSiz
  { "mode", VALTYPE_ENUM, true, OFFS_SZ_AS(Mode) },
  { "startDayTime", VALTYPE_INT16, true, OFFS_SZ_AS(StartDayTime) },
  { "endDayTime", VALTYPE_INT16, true, OFFS_SZ_AS(EndDayTime) },
  { "weekdayMask", VALTYPE_INT8, true, OFFS_SZ_AS(WeekdayMask) },
  { "chargeLevel", VALTYPE_INT8, true, OFFS_SZ_AS(ChargeLevel) },
  { "memLevel", VALTYPE_INT8, true, OFFS_SZ_AS(MemLevel) },
  { "flags", VALTYPE_INT8, true, OFFS_SZ_AS(Flags) },
};


// get table describing the fields in the struct
const TStructFieldInfo *TBinfileASLevelKey::getFieldsTable(void)
{
  return ASLevelFieldInfos;
} // TBinfileASLevelKey::getFieldsTable

sInt32 TBinfileASLevelKey::numFields(void)
{
  return sizeof(ASLevelFieldInfos)/sizeof(TStructFieldInfo);
} // TBinfileASLevelKey::numFields



// get actual struct base address
uInt8P TBinfileASLevelKey::getStructAddr(void)
{
  return (uInt8P)&(fProfileP->AutoSyncLevel[fLevelIndex]);
} // TBinfileASLevelKey::getStructAddr


// Autosync levels container key
// -----------------------------

// constructor
TBinfileASLevelsKey::TBinfileASLevelsKey(TEngineInterface *aEngineInterfaceP, TBinfileDBSyncProfile *aProfileP) :
  inherited(aEngineInterfaceP),
  fASLevelIterator(-1),
  fProfileP(aProfileP)
{
  // get pointer to BinFileClientConfig
  fBinfileClientConfigP =
    static_cast<TBinfileClientConfig *>(
      aEngineInterfaceP->getSyncAppBase()->getRootConfig()->fAgentConfigP
    );
} // TBinfileASLevelsKey::TBinfileASLevelsKey


// autosync levels can be opened only by ID
TSyError TBinfileASLevelsKey::OpenSubkey(
  TSettingsKeyImpl *&aSettingsKeyP,
  sInt32 aID, uInt16 aMode
)
{
  TSyError sta = LOCERR_OK;

  switch (aID) {
    case KEYVAL_ID_FIRST:
      fASLevelIterator = 0; // go to first
      goto getlevel;
      // then fetch next
    case KEYVAL_ID_NEXT:
      // increment
      fASLevelIterator++;
    getlevel:
      // use iterator as ID (ID=level index)
      aID = fASLevelIterator;
    default:
      // open specified ID
      if (aID<0) {
        sta=LOCERR_WRONGUSAGE;
      }
      else if (aID>=NUM_AUTOSYNC_LEVELS)
        sta=DB_NoContent; // no more autosync levels
      else {
        fASLevelIterator = aID;
        aSettingsKeyP = new TBinfileASLevelKey(fEngineInterfaceP,aID,fProfileP,fBinfileClientConfigP);
      }
      break;
  }
  // done
  return sta;
} // TBinfileASLevelsKey::OpenSubkey

#endif // AUTOSYNC_SUPPORT


// Profile key
// -----------

// constructor
TBinfileProfileKey::TBinfileProfileKey(
  TEngineInterface *aEngineInterfaceP,
  sInt32 aProfileIndex, // if <0, aProfileP is the running session's profile record
  TBinfileDBSyncProfile *aProfileP,
  TBinfileClientConfig *aBinfileClientConfigP
) :
  inherited(aEngineInterfaceP),
  fProfileIndex(aProfileIndex),
  fProfileP(aProfileP),
  fBinfileClientConfigP(aBinfileClientConfigP)
{
} // TBinfileProfileKey::TBinfileProfileKey


// destructor - close key
TBinfileProfileKey::~TBinfileProfileKey()
{
  // closing key
  if (fProfileP) {
    if (fDirty && fProfileIndex>=0) {
      // write back changed record
      fBinfileClientConfigP->writeProfile(fProfileIndex,*fProfileP);
    }
    // now delete the profile record if it is not a running session's (because then it is just passed)
    if (fProfileIndex>=0)
      delete fProfileP;
  }
} // TBinfileProfileKey::~TBinfileProfileKey


// return ID of current key
TSyError TBinfileProfileKey::GetKeyID(sInt32 &aID)
{
  aID = getProfileID();
  return LOCERR_OK;
} // TBinfileProfileKey::GetKeyID


// open subkey by name (not by path!)
// - this is the actual implementation
TSyError TBinfileProfileKey::OpenSubKeyByName(
  TSettingsKeyImpl *&aSettingsKeyP,
  cAppCharP aName, stringSize aNameSize,
  uInt16 aMode
) {
  if (fProfileP && strucmp(aName,"targets",aNameSize)==0)
    aSettingsKeyP = new TBinfileTargetsKey(fEngineInterfaceP,fProfileP->profileID);
  #ifdef AUTOSYNC_SUPPORT
  else if (fProfileP && strucmp(aName,"autosynclevels",aNameSize)==0)
    aSettingsKeyP = new TBinfileASLevelsKey(fEngineInterfaceP,fProfileP);
  #endif
  else
    return inherited::OpenSubKeyByName(aSettingsKeyP,aName,aNameSize,aMode);
  // opened a key
  return LOCERR_OK;
} // TBinfileProfileKey::OpenSubKeyByName


// - read path where binfiles are, for plugin config files passed around engine
static TSyError readBinFileDirProfile(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TBinfileProfileKey *profileKeyP = static_cast<TBinfileProfileKey *>(aStructFieldsKeyP);
  // get info
  string binFilesDir;
  profileKeyP->getBinfileClientConfig()->getBinFilesPath(binFilesDir);
  return TStructFieldsKey::returnString(binFilesDir.c_str(), aBuffer, aBufSize, aValSize);
} // readBinFileDirProfile



// - read danger flags for the profile (combined danger of all enabled datastores)
static TSyError readDangerFlags(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TBinfileProfileKey *profileKeyP = static_cast<TBinfileProfileKey *>(aStructFieldsKeyP);
  bool zapsServer, zapsClient;
  lineartime_t lastSync;
  // get info
  uInt8 danger=0;
  profileKeyP->getBinfileClientConfig()->getProfileLastSyncTime(
    profileKeyP->getProfileID(),
    lastSync, zapsServer, zapsClient
  );
  danger =
    (zapsServer ? DANGERFLAG_WILLZAPSERVER : 0) +
    (zapsClient ? DANGERFLAG_WILLZAPCLIENT : 0);
  return TStructFieldsKey::returnInt(danger, sizeof(danger), aBuffer, aBufSize, aValSize);
} // readDangerFlags



// - write a feature number (APPFTR_xxx), if this returns LOCERR_OK, feature is available, otherwise not
static TSyError writeFeatureCheck(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  TBinfileProfileKey *profileKeyP = static_cast<TBinfileProfileKey *>(aStructFieldsKeyP);
  // get feature to check for
  uInt16 featureNo = *((uInt16 *)aBuffer);
  // if feature is enabled for this profile, return LOCERR_OK, DB_NoContent otherwise
  return
    profileKeyP->getBinfileClientConfig()->isFeatureEnabled(
      profileKeyP->getProfile(),
      featureNo
    )
    ? LOCERR_OK : DB_NoContent;
} // writeFeatureCheck


// - write a readonly bitmask (rdonly_xxx), if this returns LOCERR_OK, readonly is enabled
static TSyError writeReadOnlyCheck(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  TBinfileProfileKey *profileKeyP = static_cast<TBinfileProfileKey *>(aStructFieldsKeyP);
  // get readonly mask to check for
  uInt16 readOnlyMask = *((uInt16 *)aBuffer);
  // check in profile
  bool rdOnly=
    profileKeyP->getBinfileClientConfig()->isReadOnly(
      profileKeyP->getProfile(),
      readOnlyMask
    );
  // if feature is enabled for this profile, return LOCERR_OK, DB_NoContent otherwise
  return rdOnly ? LOCERR_OK : DB_NoContent;
} // writeReadOnlyCheck


// macro simplifying typing in the table below
#define OFFS_SZ_PF(n) (offsetof(TBinfileDBSyncProfile,n)), sizeof(dP_pf->n)
// dummy pointer needed for sizeof
static const TBinfileDBSyncProfile *dP_pf=NULL;

// accessor table for profiles and for PROFILESETTING script function
const TStructFieldInfo ProfileFieldInfos[] =
{
  // valName, valType, writable, fieldOffs, valSiz
  { "profileName", VALTYPE_TEXT, true, OFFS_SZ_PF(profileName) },
  { "protocol", VALTYPE_ENUM, true, OFFS_SZ_PF(protocol) },
  { "serverURI", VALTYPE_TEXT, true, OFFS_SZ_PF(serverURI) },
  { "URIpath", VALTYPE_TEXT, true, OFFS_SZ_PF(URIpath) },
  { "serverUser", VALTYPE_TEXT, true, OFFS_SZ_PF(serverUser) },
  { "serverPassword", VALTYPE_TEXT_OBFUS, true, OFFS_SZ_PF(serverPassword) },
  { "transportUser", VALTYPE_TEXT, true, OFFS_SZ_PF(transportUser) },
  { "transportPassword", VALTYPE_TEXT_OBFUS, true, OFFS_SZ_PF(transportPassword) },
  { "socksHost", VALTYPE_TEXT, true, OFFS_SZ_PF(socksHost) },
  { "proxyHost", VALTYPE_TEXT, true, OFFS_SZ_PF(proxyHost) },
  { "proxyUser", VALTYPE_TEXT, true, OFFS_SZ_PF(proxyUser) },
  { "proxyPassword", VALTYPE_TEXT_OBFUS, true, OFFS_SZ_PF(proxyPassword) },
  { "encoding", VALTYPE_ENUM, true, OFFS_SZ_PF(encoding) },
  { "lastAuthMethod", VALTYPE_ENUM, true, OFFS_SZ_PF(lastAuthMethod) },
  { "lastNonce", VALTYPE_TEXT, true, OFFS_SZ_PF(lastNonce) },
  { "syncmlvers", VALTYPE_ENUM, true, OFFS_SZ_PF(lastSyncMLVersion) },
  { "useProxy", VALTYPE_ENUM, true, OFFS_SZ_PF(useProxy) },
  { "useConnectionProxy", VALTYPE_ENUM, true, OFFS_SZ_PF(useConnectionProxy) },
  { "transpFlags", VALTYPE_INT32, true, OFFS_SZ_PF(transpFlags) },
  { "profileFlags", VALTYPE_INT32, true, OFFS_SZ_PF(profileFlags) },
  // generic fields for app specific usage
  { "profileExtra1", VALTYPE_INT32, true, OFFS_SZ_PF(profileExtra1) },
  { "profileExtra2", VALTYPE_INT32, true, OFFS_SZ_PF(profileExtra2) },
  { "profileData", VALTYPE_BUF, true, OFFS_SZ_PF(profileData) },
  // feature/availability/extras flags
  { "dsAvailFlags", VALTYPE_INT16, true, OFFS_SZ_PF(dsAvailFlags) },
  { "readOnlyFlags", VALTYPE_INT8, true, OFFS_SZ_PF(readOnlyFlags) },
  { "remoteFlags", VALTYPE_INT8, false, OFFS_SZ_PF(remoteFlags) },
  { "featureFlags", VALTYPE_INT8, true, OFFS_SZ_PF(featureFlags) },
  // programmatic danger flags (will sync zap client or server data?)
  { "dangerFlags", VALTYPE_INT8, false, 0,0, &readDangerFlags, NULL },
  // special programmatic feature check
  { "checkForFeature", VALTYPE_INT16, true, 0,0, NULL, &writeFeatureCheck },
  { "checkForReadOnly", VALTYPE_INT16, true, 0,0, NULL, &writeReadOnlyCheck },
  // path where binfiles are, for plugin config files passed around engine
  { "binfilesDir", VALTYPE_TEXT, false, 0,0, &readBinFileDirProfile, NULL },
  #ifdef AUTOSYNC_SUPPORT
  // autosync
  { "timedSyncMobile", VALTYPE_INT16, true, OFFS_SZ_PF(TimedSyncMobilePeriod) },
  { "timedSyncCradled", VALTYPE_INT16, true, OFFS_SZ_PF(TimedSyncCradledPeriod) },
  #endif
};
const sInt32 numProfileFieldInfos = sizeof(ProfileFieldInfos)/sizeof(TStructFieldInfo);


// get table describing the fields in the struct
const TStructFieldInfo *TBinfileProfileKey::getFieldsTable(void)
{
  return ProfileFieldInfos;
} // TBinfileProfileKey::getFieldsTable

sInt32 TBinfileProfileKey::numFields(void)
{
  return numProfileFieldInfos;
} // TBinfileProfileKey::numFields



// get actual struct base address
uInt8P TBinfileProfileKey::getStructAddr(void)
{
  return (uInt8P)fProfileP;
} // TBinfileProfileKey::getStructAddr


// Profiles container key
// ----------------------

// constructor
TBinfileProfilesKey::TBinfileProfilesKey(TEngineInterface *aEngineInterfaceP) :
  inherited(aEngineInterfaceP), fProfileIterator(-1)
{
  // get pointer to BinFileClientConfig (can be NULL in case engine was never initialized with a config)
  fBinfileClientConfigP =
    static_cast<TBinfileClientConfig *>(
      aEngineInterfaceP->getSyncAppBase()->getRootConfig()->fAgentConfigP
    );
  // default to not loosing or upgrading config
  fMayLooseOldCfg=false;
} // TBinfileProfilesKey::TBinfileProfilesKey


// destructor
TBinfileProfilesKey::~TBinfileProfilesKey()
{
  // make sure all settings are saved
  if (
    fBinfileClientConfigP && // it's possible that we get there before engine was ever initialized!
    (fBinfileClientConfigP->fProfileBinFile.isOpen() || fBinfileClientConfigP->fTargetsBinFile.isOpen())
  ) {
    fBinfileClientConfigP->closeSettingsDatabases();
    fBinfileClientConfigP->openSettingsDatabases(false);
  }
} // TBinfileProfilesKey::~TBinfileProfilesKey



// profiles can be opened only by ID
TSyError TBinfileProfilesKey::OpenSubkey(
  TSettingsKeyImpl *&aSettingsKeyP,
  sInt32 aID, uInt16 aMode
)
{
  TBinfileDBSyncProfile *profileP = NULL;
  TSyError sta = LOCERR_OK;

  // safety check to see if config is here and profiles are open
  if (!fBinfileClientConfigP || !(fBinfileClientConfigP->fProfileBinFile.isOpen()))
    return LOCERR_WRONGUSAGE;
  // now create a profile record (which will be passed to subkey on success)
  profileP = new TBinfileDBSyncProfile;
  // check what to do
  switch (aID) {
    case KEYVAL_ID_NEW:
      // create a new empty profile
      fProfileIterator = fBinfileClientConfigP->newProfile("empty profile", false);
      // now get it
      goto getprofile;
    case KEYVAL_ID_NEW_DEFAULT:
      // create new profile with default values
      fProfileIterator = fBinfileClientConfigP->newProfile("default profile", true);
      // now get it
      goto getprofile;
    case KEYVAL_ID_NEW_DUP:
      // create duplicate of last opened profile (if any)
      fProfileIterator = fBinfileClientConfigP->newProfile("duplicated profile", false, fProfileIterator);
      // now get it
      goto getprofile;
    case KEYVAL_ID_FIRST:
      fProfileIterator = 0; // go to first
      goto getprofile;
      // then fetch next
    case KEYVAL_ID_NEXT:
      // increment
      fProfileIterator++;
    getprofile:
      // get profile by index
      if (fProfileIterator>=0)
        fProfileIterator=fBinfileClientConfigP->getProfile(fProfileIterator,*profileP);
      goto checkerror;
    default:
      if (aID<0) {
        sta=LOCERR_WRONGUSAGE;
        break;
      }
      // open by ID
      fProfileIterator=fBinfileClientConfigP->getProfileByID(aID,*profileP);
    checkerror:
      // check error
      if (fProfileIterator<0)
        sta=DB_NoContent; // no more profiles
      break;
  }
  if (sta==LOCERR_OK && fProfileIterator>=0) {
    // we have loaded a profile, create subkey handler and pass data
    aSettingsKeyP = new TBinfileProfileKey(fEngineInterfaceP,fProfileIterator,profileP,fBinfileClientConfigP);
    if (aSettingsKeyP)
      profileP=NULL; // ownership passed
    else
      sta=LOCERR_OUTOFMEM; // cannot create object
  }
  // get rid of it if we couldn't pass it to the subkey handler
  if (profileP)
    delete profileP;
  // done
  return sta;
} // TBinfileProfilesKey::OpenSubkey


// delete profile by ID
TSyError TBinfileProfilesKey::DeleteSubkey(sInt32 aID)
{
  sInt32 profileIndex = fBinfileClientConfigP->getProfileIndex(aID);
  if (profileIndex<0) return DB_NotFound;
  fBinfileClientConfigP->deleteProfile(profileIndex);
  fProfileIterator = -1; // invalidate iterator
  return LOCERR_OK;
} // TBinfileProfilesKey::DeleteSubkey


// - write a feature number (APPFTR_xxx), if this returns LOCERR_OK, feature is available, otherwise not
static TSyError writeGlobalFeatureCheck(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  // get feature to check for
  uInt16 featureNo = *((uInt16 *)aBuffer);
  // if feature is enabled for this profile, return LOCERR_OK, DB_NoContent otherwise
  return
    aStructFieldsKeyP->getEngineInterface()->getSyncAppBase()->isFeatureEnabled(featureNo)
    ? LOCERR_OK : DB_NoContent;
} // writeGlobalFeatureCheck

// - write provisioning string
static TSyError writeProvisioningString(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  TBinfileProfilesKey *profilesKeyP = static_cast<TBinfileProfilesKey *>(aStructFieldsKeyP);
  // execute the provisioning string
  // Note: if a new profile is created, the iterator is set such that KEYVAL_ID_NEXT
  //       will return the profile just added/updated by provisioning
  sInt32 touchedProfile;
  cAppCharP p=cAppCharP(aBuffer);
  TSyError sta = LOCERR_CFGPARSE;
  while (p && *p) {
    // push this line
    if (profilesKeyP->getBinfileClientConfig()->executeProvisioningString(p, touchedProfile)) {
      // parsed ok
      sta = LOCERR_OK; // at least one line is ok
      profilesKeyP->setNextProfileindex(touchedProfile); // such that KEYVAL_ID_NEXT will get the new/modified profile
    }
    // search for next line
    // - skip to end
    while (*p && *p!=0x0A && *p!=0x0D) ++p;
    // - skip line end
    while (*p==0x0A || *p==0x0D) ++p;
  }
  return sta;
} // writeProvisioningString

// - read registration status code
static TSyError readVarCfgStatus(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  aValSize=2;
  if (aBufSize>=aValSize) {
    // get ptr
    TBinfileProfilesKey *profilesKeyP = static_cast<TBinfileProfilesKey *>(aStructFieldsKeyP);
    // sanity check
    if (!(profilesKeyP->fBinfileClientConfigP))
      return LOCERR_WRONGUSAGE; // it's possible that we get there before engine was ever initialized!
    // try to access variable part of config (profiles/targets)
    localstatus sta = profilesKeyP->fBinfileClientConfigP->loadVarConfig(profilesKeyP->fMayLooseOldCfg);
    if (sta==LOCERR_OK) {
      // make sure profiles have all targets currently found in config
      profilesKeyP->fBinfileClientConfigP->checkProfiles();
    }
    // copy from config
    *((uInt16*)aBuffer)=sta;
  }
  return LOCERR_OK;
} // readVarCfgStatus


// - read "overwrite" flag
static TSyError readMayLooseOldConfig(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  return TStructFieldsKey::returnInt(static_cast<TBinfileProfilesKey *>(aStructFieldsKeyP)->fMayLooseOldCfg, 1, aBuffer, aBufSize, aValSize);
} // readMayLooseOldConfig


// - write "overwrite" flag
static TSyError writeMayLooseOldConfig(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  // set flag
  static_cast<TBinfileProfilesKey *>(aStructFieldsKeyP)->fMayLooseOldCfg =
    *((uInt8 *)aBuffer);
  return LOCERR_OK;
} // writeMayLooseOldConfig



// does not work at this time, but intention would be to get rid of the warning below
// #pragma GCC diagnostic ignored "-Wno-invalid-offsetof"

// accessor table for profiles
static const TStructFieldInfo ProfilesFieldInfos[] =
{
  // valName, valType, writable, fieldOffs, valSiz
  { "settingsstatus", VALTYPE_INT16, false, 0, 0, &readVarCfgStatus, NULL },
  //{ "overwrite", VALTYPE_INT8, true, offsetof(TBinfileProfilesKey,fMayLooseOldCfg), 1 }, //%%% this use of offsetof is not clean, so we use a getter/setter instead
  { "overwrite", VALTYPE_INT8, true, 0, 0, &readMayLooseOldConfig, &writeMayLooseOldConfig },
  { "provisioningstring", VALTYPE_TEXT, true, 0, 0, NULL, &writeProvisioningString },
  { "checkForFeature", VALTYPE_INT16, true, 0, 0, NULL, &writeGlobalFeatureCheck }, // global level feature check
};

// get table describing the fields in the struct
const TStructFieldInfo *TBinfileProfilesKey::getFieldsTable(void)
{
  return ProfilesFieldInfos;
} // TBinfileProfilesKey::getFieldsTable

sInt32 TBinfileProfilesKey::numFields(void)
{
  return sizeof(ProfilesFieldInfos)/sizeof(TStructFieldInfo);
} // TBinfileProfilesKey::numFields

// get actual struct base address
uInt8P TBinfileProfilesKey::getStructAddr(void)
{
  return (uInt8P)this;
} // TBinfileProfilesKey::getStructAddr



// Log entry key
// -------------

// constructor
TBinfileLogKey::TBinfileLogKey(
  TEngineInterface *aEngineInterfaceP,
  TLogFileEntry *aLogEntryP,
  TBinfileClientConfig *aBinfileClientConfigP
) :
  inherited(aEngineInterfaceP),
  fLogEntryP(aLogEntryP),
  fBinfileClientConfigP(aBinfileClientConfigP)
{
} // TBinfileLogKey::TBinfileLogKey


// destructor - close key
TBinfileLogKey::~TBinfileLogKey()
{
  // closing key
  // - dispose log entry memory
  if (fLogEntryP) delete fLogEntryP;
} // TBinfileLogKey::~TBinfileLogKey


// - read DB display name related to this log entry
static TSyError readLogDispName(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TBinfileLogKey *logentryKeyP = static_cast<TBinfileLogKey *>(aStructFieldsKeyP);

  // get config of datastore via DBID
  TLocalDSConfig *dscfgP = logentryKeyP->getBinfileClientConfig()->getLocalDS(NULL,logentryKeyP->getLogEntry()->dbID);
  cAppCharP dispName = "";
  #ifndef MINIMAL_CODE
  dispName = dscfgP->fDisplayName.c_str();
  #endif
  if (*dispName==0) {
    // no display name, get technical name instead
    dispName = dscfgP->getName();
  }
  return TStructFieldsKey::returnString(dispName, aBuffer, aBufSize, aValSize);
} // readLogDispName


// - read DB display name related to this log entry
static TSyError readLogProfileName(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TBinfileLogKey *logentryKeyP = static_cast<TBinfileLogKey *>(aStructFieldsKeyP);

  // get profile data via profile ID
  TBinfileDBSyncProfile profile;
  logentryKeyP->getBinfileClientConfig()->getProfileByID(logentryKeyP->getLogEntry()->profileID,profile);
  return TStructFieldsKey::returnString(profile.profileName, aBuffer, aBufSize, aValSize);
} // readLogProfileName




// macro simplifying typing in the table below
#define OFFS_SZ_LOG(n) (offsetof(TLogFileEntry,n)), sizeof(dP_log->n)
// dummy pointer needed for sizeof
static const TLogFileEntry *dP_log=NULL;

// accessor table for log entries
static const TStructFieldInfo LogEntryFieldInfos[] =
{
  // valName, valType, writable, fieldOffs, valSiz
  // - direct access
  { "time", VALTYPE_TIME64, false, OFFS_SZ_LOG(time) },
  { "dbtypeid", VALTYPE_INT32, false, OFFS_SZ_LOG(dbID) },
  { "profileid", VALTYPE_INT32, false, OFFS_SZ_LOG(profileID) },
  { "status", VALTYPE_INT16, false, OFFS_SZ_LOG(status) },
  { "mode", VALTYPE_INT16, false, OFFS_SZ_LOG(mode) },
  { "dirmode", VALTYPE_INT16, false, OFFS_SZ_LOG(dirmode) },
  { "locAdded", VALTYPE_INT32, false, OFFS_SZ_LOG(locAdded) },
  { "locUpdated", VALTYPE_INT32, false, OFFS_SZ_LOG(locUpdated) },
  { "locDeleted", VALTYPE_INT32, false, OFFS_SZ_LOG(locDeleted) },
  { "remAdded", VALTYPE_INT32, false, OFFS_SZ_LOG(remAdded) },
  { "remUpdated", VALTYPE_INT32, false, OFFS_SZ_LOG(remUpdated) },
  { "remDeleted", VALTYPE_INT32, false, OFFS_SZ_LOG(remDeleted) },
  { "inBytes", VALTYPE_INT32, false, OFFS_SZ_LOG(inBytes) },
  { "outBytes", VALTYPE_INT32, false, OFFS_SZ_LOG(outBytes) },
  { "locRejected", VALTYPE_INT32, false, OFFS_SZ_LOG(locRejected) },
  { "remRejected", VALTYPE_INT32, false, OFFS_SZ_LOG(remRejected) },
  // - procedural convenience
  { "dispName", VALTYPE_TEXT, false, 0, 0, &readLogDispName, NULL },
  { "profileName", VALTYPE_TEXT, false, 0, 0, &readLogProfileName, NULL },
};


// get table describing the fields in the struct
const TStructFieldInfo *TBinfileLogKey::getFieldsTable(void)
{
  return LogEntryFieldInfos;
} // TBinfileLogKey::getFieldsTable

sInt32 TBinfileLogKey::numFields(void)
{
  return sizeof(LogEntryFieldInfos)/sizeof(TStructFieldInfo);
} // TBinfileLogKey::numFields



// get actual struct base address
uInt8P TBinfileLogKey::getStructAddr(void)
{
  return (uInt8P)(fLogEntryP);
} // TBinfileLogKey::getStructAddr



// Log entries container key
// -------------------------

// constructor
TBinfileLogsKey::TBinfileLogsKey(TEngineInterface *aEngineInterfaceP) :
  inherited(aEngineInterfaceP),
  fLogEntryIterator(-1)
{
  fBinfileClientConfigP =
    static_cast<TBinfileClientConfig *>(
      aEngineInterfaceP->getSyncAppBase()->getRootConfig()->fAgentConfigP
    );
  // open the log file
  // - get base path
  string filepath;
  fBinfileClientConfigP->getBinFilesPath(filepath);
  filepath += LOGFILE_DB_NAME;
  // - try to open
  fLogFile.setFileInfo(filepath.c_str(),LOGFILE_DB_VERSION,LOGFILE_DB_ID,sizeof(TLogFileEntry));
  fLogFile.open(0,NULL,NULL);
  // Note: errors are not checked here, as no logfile is ok. We'll check before trying to read with isOpen()
} // TBinfileLogsKey::TBinfileLogsKey


// log entries can be opened only by reverse index (0=newest...n=oldest)
TSyError TBinfileLogsKey::OpenSubkey(
  TSettingsKeyImpl *&aSettingsKeyP,
  sInt32 aID, uInt16 aMode
)
{
  // if there is no logfile, we have no entries
  if (!fLogFile.isOpen()) return DB_NotFound;
  switch (aID) {
    case KEYVAL_ID_FIRST:
      fLogEntryIterator = 0; // go to latest (last in file!)
      goto getnthentry;
      // then fetch next
    case KEYVAL_ID_NEXT:
      // increment
      fLogEntryIterator++;
      goto getnthentry;
    default:
      fLogEntryIterator=aID;
    getnthentry:
      // get n-th-last log entry as indicated by iterator
      // - check index range
      sInt32 numrecs = fLogFile.getNumRecords();
      if (fLogEntryIterator<0) return LOCERR_WRONGUSAGE;
      if (fLogEntryIterator>=numrecs) return DB_NoContent;
      // - retrieve
      TLogFileEntry *entryP = new TLogFileEntry;
      if (fLogFile.readRecord(numrecs-1-fLogEntryIterator,entryP) != BFE_OK) {
        delete entryP;
        return DB_Fatal;
      }
      // - create key and return it (loaded entry gets owned by key)
      aSettingsKeyP = new TBinfileLogKey(fEngineInterfaceP,entryP,fBinfileClientConfigP);
  }
  return LOCERR_OK;
} // TBinfileLogsKey::OpenSubkey


// delete profile by ID
TSyError TBinfileLogsKey::DeleteSubkey(sInt32 aID)
{
  // log can only be cleared entirely
  if (aID!=KEYVAL_ID_ALL) return LOCERR_WRONGUSAGE;
  // erase log
  fLogFile.truncate(0);
  return LOCERR_OK;
} // TBinfileLogsKey::DeleteSubkey



// Binfile Agent root key
// ----------------------

// Constructor
TBinfileAgentRootKey::TBinfileAgentRootKey(TEngineInterface *aEngineInterfaceP) :
  inherited(aEngineInterfaceP)
{
} // TBinfileAgentRootKey::TBinfileAgentRootKey


// open subkey by name (not by path!)
// - this is the actual implementation
TSyError TBinfileAgentRootKey::OpenSubKeyByName(
  TSettingsKeyImpl *&aSettingsKeyP,
  cAppCharP aName, stringSize aNameSize,
  uInt16 aMode
) {
  if (strucmp(aName,"profiles",aNameSize)==0) {
    // allow accessing profiles only for active binfiles
    if (
      static_cast<TBinfileClientConfig *>(
        fEngineInterfaceP->getSyncAppBase()->getRootConfig()->fAgentConfigP
      )->fBinfilesActive
    ) {
      aSettingsKeyP = new TBinfileProfilesKey(fEngineInterfaceP);
    }
    else {
      // cannot access profiles of inactive binfile layer
      return LOCERR_WRONGUSAGE;
    }
  }
  else if (strucmp(aName,"synclogs",aNameSize)==0)
    aSettingsKeyP = new TBinfileLogsKey(fEngineInterfaceP);
  else
    return inherited::OpenSubKeyByName(aSettingsKeyP,aName,aNameSize,aMode);
  // opened a key
  return LOCERR_OK;
} // TBinfileAgentRootKey::OpenSubKeyByName


// Client runtime settings key
// ---------------------------


// Constructor
TBinFileAgentParamsKey::TBinFileAgentParamsKey(TEngineInterface *aEngineInterfaceP, TSyncAgent *aClientSessionP) :
  inherited(aEngineInterfaceP,aClientSessionP)
{
} // TBinFileAgentParamsKey::TBinFileAgentParamsKey


// open subkey by name (not by path!)
TSyError TBinFileAgentParamsKey::OpenSubKeyByName(
  TSettingsKeyImpl *&aSettingsKeyP,
  cAppCharP aName, stringSize aNameSize,
  uInt16 aMode
) {
  if (strucmp(aName,"profile",aNameSize)==0) {
    // get binfileclient session pointer
    TBinfileImplClient *bfclientP = static_cast<TBinfileImplClient *>(fAgentP);
    // opens current session's active profile
    aSettingsKeyP = new TBinfileProfileKey(
      fEngineInterfaceP,
      -1, // signals passing active session's profile
      &bfclientP->fProfile, // pointer to the current session's profile
      bfclientP->fConfigP // the config
    );
  }
  else
    return inherited::OpenSubKeyByName(aSettingsKeyP,aName,aNameSize,aMode);
  // opened a key
  return LOCERR_OK;
} // TBinFileAgentParamsKey::OpenSubKeyByName


#endif // ENGINEINTERFACE_SUPPORT




// Config
// ======

TBinfileClientConfig::TBinfileClientConfig(TConfigElement *aParentElement) :
  TAgentConfig("BinFileDBClient",aParentElement)
{
} // TBinfileClientConfig::TBinfileClientConfig


TBinfileClientConfig::~TBinfileClientConfig()
{
  clear();
} // TBinfileClientConfig::~TBinfileClientConfig


// init defaults
void TBinfileClientConfig::clear(void)
{
  // Only active in clients by default
  fBinfilesActive = IS_CLIENT;
  #ifndef HARDCODED_CONFIG
  // init defaults
  fSeparateChangelogs = true; // for engine libraries with full config, use separated changelogs by default (auto-migration w/o side effects is built-in)
  fBinFilesPath.erase();
  #else
  fSeparateChangelogs = false; // for traditional hard-coded clients like WinMobile and PalmOS, use pre 3.4.0.10 behaviour (uses less memory)
  #endif
  fBinFileLog=false;
  // - clear inherited
  inherited::clear();
} // TBinfileClientConfig::clear


#ifndef HARDCODED_CONFIG

// config element parsing
bool TBinfileClientConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  // - binfiles path
  if (strucmp(aElementName,"binfilespath")==0)
    expectMacroString(fBinFilesPath);
  else if (strucmp(aElementName,"binfilelog")==0)
    expectBool(fBinFileLog);
  else if (strucmp(aElementName,"binfilesactive")==0)
    expectBool(fBinfilesActive);
  else if (strucmp(aElementName,"separatechangelogs")==0)
    expectBool(fSeparateChangelogs);
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TBinfileClientConfig::localStartElement

#endif


// update profile record contents
static uInt32 profileUpdateFunc(uInt32 aOldVersion, uInt32 aNewVersion, void *aOldRecordData, void *aNewRecordData, uInt32 aOldSize)
{
  if (aOldVersion<LOWEST_PROFILE_DB_VERSION || aOldVersion>PROFILE_DB_VERSION) return 0; // unknown old or newer than current version, cannot update
  if (aNewVersion!=PROFILE_DB_VERSION) return 0; // cannot update to other version than current
  // create default values for profile
  if (aOldRecordData && aNewRecordData) {
    TBinfileDBSyncProfile *profileP = (TBinfileDBSyncProfile *)aNewRecordData;
    // make an empty default profile
    TBinfileClientConfig::initProfile(*profileP, "dummy", true);
    #if (PROFILE_DB_VERSION>=6) && (LOWEST_PROFILE_DB_VERSION<6)
    // copy in old version's data
    if (aOldVersion<=5) {
      // between 5 and 6, sizes of user and pw fields have all changed in size, so we need
      // to copy field-by-field
      o_TBinfileDBSyncProfile *oP=(o_TBinfileDBSyncProfile *)aOldRecordData;
      // now copy
      profileP->profileID = oP->profileID;
      AssignCString(profileP->profileName,oP->profileName,maxnamesiz);
      profileP->encoding = oP->encoding;
      AssignCString(profileP->serverURI,oP->serverURI,maxurisiz);
      AssignCString(profileP->serverUser,oP->serverUser,maxupwsiz);
      memcpy(profileP->serverPassword,oP->serverPassword,o_maxupwsiz);
      AssignCString(profileP->transportUser,oP->transportUser,maxupwsiz);
      memcpy(profileP->transportPassword,oP->transportPassword,o_maxupwsiz);
      AssignCString(profileP->socksHost,oP->socksHost,maxurisiz);
      AssignCString(profileP->proxyHost,oP->proxyHost,maxurisiz);
      profileP->sessionID = oP->sessionID;
      profileP->lastSyncMLVersion = oP->lastSyncMLVersion;
      profileP->lastAuthMethod = oP->lastAuthMethod;
      profileP->lastAuthFormat = oP->lastAuthFormat;
      AssignCString(profileP->lastNonce,oP->lastNonce,maxnoncesiz);
      profileP->firstuse = oP->firstuse;
      if (aOldVersion>=5) { // can only be OldVersion==5
        // copy additional PROFILE_DB_VERSION 5 settings
        AssignCString(profileP->URIpath,oP->URIpath,maxpathsiz);
        profileP->protocol = oP->protocol;
        profileP->readOnlyFlags = oP->readOnlyFlags;
        AssignCString(profileP->localDBProfileName,oP->localDBProfileName,localDBpathMaxLen);
        profileP->useProxy = oP->useProxy;
        profileP->useConnectionProxy = oP->useConnectionProxy;
        // - Auto-Sync levels (3 levels, first match overrides lower level settings)
        profileP->AutoSyncLevel[0]=oP->AutoSyncLevel[0];
        profileP->AutoSyncLevel[1]=oP->AutoSyncLevel[1];
        profileP->AutoSyncLevel[2]=oP->AutoSyncLevel[2];
        profileP->TimedSyncMobilePeriod = oP->TimedSyncMobilePeriod;
        profileP->TimedSyncCradledPeriod = oP->TimedSyncCradledPeriod;
        //   - IPP settings are not copied, they have changed a bit between 6 and 7
      }
    }
    #endif
    if (aOldVersion<7) {
      // as ippsettings have changed between 6 and 7, do not copy them, but erase them
      #if PROFILE_DB_VERSION>=5
      profileP->ippSettings.id[0]=0;
      profileP->ippSettings.cred[0]=0;
      profileP->ippSettings.srv[0]=0;
      profileP->ippSettings.method=0;
      #endif
    }
    // From Version 7 onwards, only some fields were added at the end, so simple copy is ok
    if (aOldVersion==7) {
      memcpy(aNewRecordData,aOldRecordData,PROFILE_DB_VERSION_7_SZ);
    }
    if (aOldVersion==8) {
      memcpy(aNewRecordData,aOldRecordData,PROFILE_DB_VERSION_8_SZ);
    }
    /* when we have PROFILE_DB_VERSION > 9
    if (aOldVersion==9) {
      memcpy(aNewRecordData,aOldRecordData,PROFILE_DB_VERSION_9_SZ);
    }
    */
  }
  // updated ok (or updateable ok if no data pointers provided)
  return sizeof(TBinfileDBSyncProfile);
} // profileUpdateFunc


// update target record contents
static uInt32 targetUpdateFunc(uInt32 aOldVersion, uInt32 aNewVersion, void *aOldRecordData, void *aNewRecordData, uInt32 aOldSize)
{
  if (aOldVersion<LOWEST_TARGETS_DB_VERSION || aOldVersion>TARGETS_DB_VERSION) return 0; // unknown old version, cannot update
  if (aNewVersion!=TARGETS_DB_VERSION) return 0; // cannot update to other version than current
  // create default values for profile
  if (aOldRecordData && aNewRecordData) {
    TBinfileDBSyncTarget *targetP = (TBinfileDBSyncTarget *)aNewRecordData;
    // copy old data - beginning of record is identical
    memcpy(aNewRecordData,aOldRecordData,aOldSize);
    // now initialize fields that old version didn't have
    if (aOldVersion<4) {
      // init new version 4 fields
      targetP->resumeAlertCode = 0;
      // also wipe out version 3 "lastModCount", which now becomes "lastSuspendModCount"
      targetP->lastSuspendModCount = 0;
    }
    #if TARGETS_DB_VERSION >= 5
    if (aOldVersion<5) {
      // init new version 5 fields
      AssignCString(targetP->localContainerName,NULL,localDBpathMaxLen);
    }
    #endif
    #if TARGETS_DB_VERSION >= 6
    if (aOldVersion<6) {
      // init new version 6 fields
      AssignCString(targetP->dummyIdentifier1,NULL,remoteAnchorMaxLen);
      AssignCString(targetP->dummyIdentifier2,NULL,remoteAnchorMaxLen);
      AssignCString(targetP->remoteDBdispName,NULL,dispNameMaxLen);
      AssignCString(targetP->filterCapDesc,NULL,filterCapDescMaxLen);
      AssignCString(targetP->remoteFilters,NULL,filterExprMaxLen);
      AssignCString(targetP->localFilters,NULL,filterExprMaxLen);
    }
    #endif
  }
  // updated ok (or updateable ok if no data pointers provided)
  // - return size of new record
  return sizeof(TBinfileDBSyncTarget);
} // targetUpdateFunc


// get path where to store binfiles
void TBinfileClientConfig::getBinFilesPath(string &aPath)
{
  #ifndef HARDCODED_CONFIG
  if (!fBinFilesPath.empty())
    aPath = fBinFilesPath;
  else
  #endif
  {
    // use appdata directory as defined by the current platform/OS
    if (!getPlatformString(pfs_appdata_path,aPath)) {
      // we have no path
      aPath.erase();
    }
  }
  // make OS path of it (if not empty) and create directory if not existing yet
  if (!aPath.empty()) {
    // make sure it exists
    makeOSDirPath(aPath,true);
  }
}; // TBinfileClientConfig::getBinFilesPath


// open settings databases
localstatus TBinfileClientConfig::openSettingsDatabases(bool aDoLoose)
{
  if (!fBinfilesActive) {
    // databases can be opened only with active binfiles layer
    return LOCERR_WRONGUSAGE;
  }
  else {
    // safe for calling more than once
    if (fProfileBinFile.isOpen() && fTargetsBinFile.isOpen())
      return LOCERR_OK; // already open - ok
    // open profile and targets databases
    // - get base path
    string basepath;
    getBinFilesPath(basepath);
    string usedpath;
    bool newprofiles=false;
    bferr err;
    // - profiles
    usedpath=basepath + PROFILE_DB_NAME;
    fProfileBinFile.setFileInfo(usedpath.c_str(),PROFILE_DB_VERSION,PROFILE_DB_ID,sizeof(TBinfileDBSyncProfile));
    err = fProfileBinFile.open(0,NULL,profileUpdateFunc);
    if (err!=BFE_OK) {
      // create new one or overwrite incompatible one if allowed
      if (aDoLoose || err!=BFE_BADVERSION) {
        err=fProfileBinFile.create(sizeof(TBinfileDBSyncProfile),0,NULL,true);
        newprofiles=true;
      }
      else {
        // would create new file due to bad (newer or non-upgradeable older) version
        return LOCERR_CFGPARSE; // this is kind of a config parsing error
      }
    }
    // - targets
    usedpath=basepath + TARGETS_DB_NAME;
    fTargetsBinFile.setFileInfo(usedpath.c_str(),TARGETS_DB_VERSION,TARGETS_DB_ID,sizeof(TBinfileDBSyncTarget));
    err = fTargetsBinFile.open(0,NULL,targetUpdateFunc);
    if (err!=BFE_OK || newprofiles) {
      // create new one or overwrite incompatible one
      // also ALWAYS create new targets if we HAVE created new profiles
      if (aDoLoose || newprofiles || err!=BFE_BADVERSION) {
        err=fTargetsBinFile.create(sizeof(TBinfileDBSyncTarget),0,NULL,true);
      }
      else {
        // would create new file due to bad (newer or non-upgradeable older) version
        return LOCERR_CFGPARSE; // this is kind of a config parsing error
      }
    }
    return err;
  }
} // TBinfileClientConfig::openSettingsDatabases


// close settings databases
void TBinfileClientConfig::closeSettingsDatabases(void)
{
  // open profile and targets databases
  fProfileBinFile.close();
  fTargetsBinFile.close();
} // TBinfileClientConfig::closeSettingsDatabases


// resolve
void TBinfileClientConfig::localResolve(bool aLastPass)
{
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TBinfileClientConfig::localResolve



// MUST be called after creating config to load (or pre-load) variable parts of config
// such as binfile profiles. If aDoLoose==false, situations, where existing config
// is detected but cannot be re-used will return an error. With aDoLoose==true, config
// files etc. are created even if it means a loss of data.
localstatus TBinfileClientConfig::loadVarConfig(bool aDoLoose)
{
  // let inherited to it's stuff
  localstatus err=inherited::loadVarConfig(aDoLoose);
  if (fBinfilesActive) {
    // now do my own stuff
    if (err==LOCERR_OK) {
      err=openSettingsDatabases(aDoLoose);
    }
  }
  // return status
  return err;
} // TBinfileClientConfig::loadVarConfig



// save app state (such as settings in datastore configs etc.)
void TBinfileClientConfig::saveAppState(void)
{
  if (fBinfilesActive) {
    // close and re-open the settings binfiles to make sure their
    // contents is permanently saved
    closeSettingsDatabases();
    openSettingsDatabases(false);
  }
} // TBinfileClientConfig::saveAppState


// API for settings management


// initialize a new-created profile database
void TBinfileClientConfig::initProfileDb(void)
{
  // create default profile
  newProfile("Default",true);
} // TBinfileClientConfig::initProfileDb


// Defaults for profile
#ifndef DEFAULT_ENCODING
#define DEFAULT_ENCODING SML_WBXML
#endif
#ifndef DEFAULT_SERVER_URI
#define DEFAULT_SERVER_URI NULL
#endif
#ifndef DEFAULT_URI_PATH
#define DEFAULT_URI_PATH NULL
#endif
#ifndef DEFAULT_SERVER_USER
#define DEFAULT_SERVER_USER NULL
#endif
#ifndef DEFAULT_SERVER_PASSWD
#define DEFAULT_SERVER_PASSWD NULL
#endif
#ifndef DEFAULT_TRANSPORT_USER
#define DEFAULT_TRANSPORT_USER NULL
#endif
#ifndef DEFAULT_TRANSPORT_PASSWD
#define DEFAULT_TRANSPORT_PASSWD NULL
#endif
#ifndef DEFAULT_SOCKS_HOST
#define DEFAULT_SOCKS_HOST NULL
#endif
#ifndef DEFAULT_PROXY_HOST
#define DEFAULT_PROXY_HOST NULL
#endif
#ifndef DEFAULT_PROXY_USER
#define DEFAULT_PROXY_USER NULL
#endif
#ifndef DEFAULT_PROXY_PASSWD
#define DEFAULT_PROXY_PASSWD NULL
#endif
#ifndef DEFAULT_DATASTORES_ENABLED
#define DEFAULT_DATASTORES_ENABLED false
#endif
#ifndef DEFAULT_LOCALDB_PROFILE
#define DEFAULT_LOCALDB_PROFILE NULL
#endif
#ifndef DEFAULT_USESYSTEMPROXY
#define DEFAULT_USESYSTEMPROXY true
#endif

// create new profile with targets for all configured datastores
// - returns index of new profile
sInt32 TBinfileClientConfig::newProfile(const char *aProfileName, bool aSetDefaults, sInt32 aTemplateProfile)
{
  TBinfileDBSyncProfile profile;
  TBinfileDBSyncTarget target;
  sInt32 profileIndex;
  TBinfileDBSyncProfile templateprofile;
  TBinfileDBSyncTarget templatetarget;

  // create profile record
  initProfile(profile,aProfileName,aSetDefaults);
  #ifdef EXPIRES_AFTER_DAYS
  // copy date of first use (to see if someone tried to tamper with...)
  uInt32 vers;
  getSyncAppBase()->getFirstUseInfo(SYSER_VARIANT_CODE,profile.firstuse,vers);
  #endif
  // copy from template profile, if any
  if (aTemplateProfile>=0) {
    aTemplateProfile=getProfile(aTemplateProfile,templateprofile);
  }
  if (aTemplateProfile>=0) {
    // copy user config
    profile.encoding=templateprofile.encoding;
    #ifndef HARD_CODED_SERVER_URI
    strncpy(profile.serverURI,templateprofile.serverURI,maxurisiz); // if hardcoded, don't copy from template
    #endif
    strncpy(profile.serverUser,templateprofile.serverUser,maxupwsiz);
    strncpy(profile.transportUser,templateprofile.transportUser,maxupwsiz);
    strncpy(profile.socksHost,templateprofile.socksHost,maxurisiz);
    strncpy(profile.proxyHost,templateprofile.proxyHost,maxurisiz);
    strncpy(profile.proxyUser,templateprofile.proxyUser,maxupwsiz);
    // additional proxy flags
    profile.useProxy=templateprofile.useProxy;
    profile.useConnectionProxy=templateprofile.useConnectionProxy;
    // improved URI settings
    strncpy(profile.URIpath,templateprofile.URIpath,maxpathsiz);
    profile.protocol=templateprofile.protocol;
    // feature flags
    profile.readOnlyFlags = 0; // no read-only flags by default
    profile.featureFlags=templateprofile.featureFlags; // inherit features
    profile.dsAvailFlags=templateprofile.dsAvailFlags; // inherit datastore availability
    // extras
    profile.transpFlags=templateprofile.transpFlags; // inherit transport related flags
    profile.profileFlags=templateprofile.profileFlags; // inherit general profile flags
    // Note: do not copy profileExtra1/2 and profileData - as these are too app specific
    // local DB profile (not used in PPC, only for Outlook client)
    strncpy(profile.localDBProfileName,templateprofile.localDBProfileName,localDBpathMaxLen);
    // autosync settings
    profile.AutoSyncLevel[0]=templateprofile.AutoSyncLevel[0];
    profile.AutoSyncLevel[1]=templateprofile.AutoSyncLevel[1];
    profile.AutoSyncLevel[2]=templateprofile.AutoSyncLevel[2];
    // timed sync settings
    profile.TimedSyncMobilePeriod=templateprofile.TimedSyncMobilePeriod;
    profile.TimedSyncCradledPeriod=templateprofile.TimedSyncCradledPeriod;
    /* %%% do not copy IPP settings, these should be provisioned by the SyncML server
    // IPP settings (not available in all clients, but present in all profile records
    profile.ippSettings=templateprofile.ippSettings;
    */
  }
  #ifndef HARD_CODED_SERVER_URI
  // override with config-defined fixed server URL, if any
  if (!fServerURI.empty()) {
    strncpy(profile.serverURI,fServerURI.c_str(),maxurisiz);
    profile.readOnlyFlags = rdonly_URI; // make URI readonly
  }
  #endif
  // save profile record
  profileIndex=writeProfile(-1,profile);
  // create a target for each configured datastore
  // Note: create target also for not available datastores
  TLocalDSList::iterator pos;
  for (pos=fDatastores.begin();pos!=fDatastores.end();pos++) {
    if ((*pos)->isAbstractDatastore()) continue; // don't try to create targets for abstract datastores (like superdatastores)
    // non-abstract datastores at this point are always binfile-based by definition (this is a binfileimplclient, and this is one of its datastores)
    TBinfileDSConfig *cfgP = static_cast<TBinfileDSConfig *>(*pos);
    cfgP->initTarget(target,profile.profileID,aSetDefaults ? NULL : "",aSetDefaults && DEFAULT_DATASTORES_ENABLED); // remote datastore names default to local ones, empty if not default
    // copy from template
    if (aTemplateProfile>=0) {
      // find matching template target
      sInt32 tti=findTargetIndexByDBInfo(
        templateprofile.profileID, // profile to search targets for
        cfgP->fLocalDBTypeID,
        NULL // can be NULL if name does not matter
      );
      if (tti>=0) {
        getTarget(tti,templatetarget);
        // copy user config
        strncpy(target.remoteDBpath,templatetarget.remoteDBpath,remoteDBpathMaxLen);
        target.syncmode=templatetarget.syncmode;
        target.limit1=templatetarget.limit1;
        target.limit2=templatetarget.limit2;
        target.extras=templatetarget.extras;
      }
    }
    // save target record
    writeTarget(-1,target);
  }
  return profileIndex;
} // TBinfileClientConfig::newProfile


// helper
static void AssignDefault(char *aCStr, bool aWithDefaults, const char *aDefault, size_t aLen)
{
  if (aWithDefaults)
    AssignCString(aCStr,aDefault,aLen);
  else
    AssignCString(aCStr,NULL,aLen);
} // AssignDefault


// init empty profile record
void TBinfileClientConfig::initProfile(TBinfileDBSyncProfile &aProfile, const char *aName, bool aWithDefaults)
{
  // - wipe it to all zeroes to make sure we don't save any garbage under no circumstances
  memset((void *)&aProfile,0,sizeof(TBinfileDBSyncProfile));
  // - Name
  if (aName==0) aName="<untitled>";
  strncpy(aProfile.profileName,aName,maxnamesiz);
  // - encoding defaults to WBXML
  aProfile.encoding=DEFAULT_ENCODING;
  AssignDefault(aProfile.serverURI,aWithDefaults, DEFAULT_SERVER_URI, maxurisiz);
  // - suffix (for hardcoded base URI versions)
  AssignDefault(aProfile.URIpath,aWithDefaults, DEFAULT_URI_PATH, maxpathsiz);
  #ifdef PROTOCOL_SELECTOR
  aProfile.protocol=PROTOCOL_SELECTOR;
  #else
  aProfile.protocol=transp_proto_uri;
  #endif
  aProfile.readOnlyFlags=0; // all read&write so far
  aProfile.remoteFlags=0; // no remote specifics known so far
  #ifdef FTRFLAGS_ALWAYS_AVAILABLE
  aProfile.featureFlags=FTRFLAGS_ALWAYS_AVAILABLE; // set default availability
  #else
  aProfile.featureFlags=0; // no special feature flags
  #endif
  #ifdef DSFLAGS_ALWAYS_AVAILABLE
  aProfile.dsAvailFlags=DSFLAGS_ALWAYS_AVAILABLE; // set default availability
  #else
  aProfile.dsAvailFlags=0; // no extra DS enabled
  #endif
  // - User name
  AssignDefault(aProfile.serverUser,aWithDefaults, DEFAULT_SERVER_USER, maxupwsiz);
  // - password, mangled
  assignMangledToCString(aProfile.serverPassword, (const char *)(aWithDefaults ? DEFAULT_SERVER_PASSWD : NULL), maxupwsiz,true);
  // - Transport user
  AssignDefault(aProfile.transportUser,aWithDefaults, DEFAULT_TRANSPORT_USER, maxupwsiz);
  // - Transport password, mangled
  assignMangledToCString(aProfile.transportPassword, (const char *)(aWithDefaults ? DEFAULT_TRANSPORT_PASSWD : NULL), maxupwsiz,true);
  // - Socks Host
  AssignDefault(aProfile.socksHost,aWithDefaults, DEFAULT_SOCKS_HOST, maxurisiz);
  // - Proxy Host
  AssignDefault(aProfile.proxyHost,aWithDefaults, DEFAULT_PROXY_HOST, maxurisiz);
  // - local DB profile name
  AssignDefault(aProfile.localDBProfileName,true,DEFAULT_LOCALDB_PROFILE,localDBpathMaxLen);
  // - additional proxy flags
  aProfile.useProxy=aProfile.proxyHost[0]!=0; // use if a default proxy is defined
  aProfile.useConnectionProxy=DEFAULT_USESYSTEMPROXY; // use proxy from connection if available
  // - proxy user
  AssignDefault(aProfile.proxyUser,aWithDefaults, DEFAULT_PROXY_USER, maxupwsiz);
  // - proxy password, mangled
  assignMangledToCString(aProfile.proxyPassword, (const char *)(aWithDefaults ? DEFAULT_PROXY_PASSWD : NULL), maxupwsiz,true);
  // - extra flags
  aProfile.transpFlags = 0;
  aProfile.profileFlags = 0;
  // - general purpose reserved fields
  aProfile.profileExtra1 = 0;
  aProfile.profileExtra2 = 0;
  memset(&aProfile.profileData, 0, profiledatasiz);
  // - automatic sync scheduling
  aProfile.AutoSyncLevel[0].Mode=autosync_none; // not enabled
  aProfile.AutoSyncLevel[0].StartDayTime=8*60; // 8:00 AM
  aProfile.AutoSyncLevel[0].EndDayTime=17*60; // 5:00 PM (17:00)
  aProfile.AutoSyncLevel[0].WeekdayMask=0x3E; // Mo-Fr
  aProfile.AutoSyncLevel[0].ChargeLevel=60; // 60% battery level needed
  aProfile.AutoSyncLevel[0].MemLevel=10; // 10% memory must be free
  aProfile.AutoSyncLevel[0].Flags=0; // no flags so far
  aProfile.AutoSyncLevel[1].Mode=autosync_none; // not enabled
  aProfile.AutoSyncLevel[1].StartDayTime=0;
  aProfile.AutoSyncLevel[1].EndDayTime=0;
  aProfile.AutoSyncLevel[1].WeekdayMask=0;
  aProfile.AutoSyncLevel[1].ChargeLevel=0;
  aProfile.AutoSyncLevel[1].MemLevel=0;
  aProfile.AutoSyncLevel[1].Flags=0;
  aProfile.AutoSyncLevel[2].Mode=autosync_none; // not enabled
  aProfile.AutoSyncLevel[2].StartDayTime=0;
  aProfile.AutoSyncLevel[2].EndDayTime=0;
  aProfile.AutoSyncLevel[2].WeekdayMask=0;
  aProfile.AutoSyncLevel[2].ChargeLevel=0;
  aProfile.AutoSyncLevel[2].MemLevel=0;
  aProfile.AutoSyncLevel[2].Flags=0;
  // Timed sync settings
  aProfile.TimedSyncMobilePeriod=2*60; // every 2 hours
  aProfile.TimedSyncCradledPeriod=15; // every 15 minutes
  // IPP settings
  aProfile.ippSettings.srv[0]=0;
  aProfile.ippSettings.port=0;
  aProfile.ippSettings.period=5*60; // 5 mins
  aProfile.ippSettings.path[0]=0;
  aProfile.ippSettings.id[0]=0;
  aProfile.ippSettings.method=0; // none defined
  aProfile.ippSettings.cred[0]=0;
  aProfile.ippSettings.maxinterval=0;
  aProfile.ippSettings.timedds[0]=0;
  // Internals
  // - session ID
  aProfile.sessionID=0;
  // - last connection's parameters (used as default for next session)
  aProfile.lastSyncMLVersion=syncml_vers_unknown;
  aProfile.lastAuthMethod=auth_none;
  aProfile.lastAuthFormat=fmt_chr;
  AssignCString(aProfile.lastNonce,NULL,maxnoncesiz);
} // initProfile


// checks that all profiles are complete with targets for all configured datastores
// (in case of STD->PRO upgrade for example)
void TBinfileClientConfig::checkProfiles(void)
{
  for (sInt32 pidx=0; pidx<numProfiles(); pidx++) {
    checkProfile(pidx);
  }
} // TBinfileClientConfig::checkProfiles


// checks that profile is complete with targets for all configured datastores
// (in case of STD->PRO upgrade for example)
void TBinfileClientConfig::checkProfile(sInt32 aProfileIndex)
{
  TBinfileDBSyncProfile profile;

  // get profile record
  if (getProfile(aProfileIndex,profile)>=0) {
    // make sure a target exists for each configured datastore
    // Note: also creates targets for currently unavailable datastores (profile.dsAvailFlags, license...)
    TLocalDSList::iterator pos;
    for (pos=fDatastores.begin();pos!=fDatastores.end();pos++) {
      TBinfileDSConfig *cfgP = static_cast<TBinfileDSConfig *>(*pos);
      // find matching template target
      findOrCreateTargetIndexByDBInfo(
        profile.profileID, // profile to search targets for
        cfgP->fLocalDBTypeID,
        NULL // can be NULL if name does not matter
      );
    }
  }
} // TBinfileClientConfig::newProfile



// - write profile, returns index of profile
sInt32 TBinfileClientConfig::writeProfile(
  sInt32 aProfileIndex, // -1 if adding new profile
  TBinfileDBSyncProfile &aProfile // profile ID is set to new ID in aProfile
)
{
  uInt32 newindex=aProfileIndex;
  if (aProfileIndex<0) {
    // set new unqiue ID
    aProfile.profileID=fProfileBinFile.getNextUniqueID();
    // create new record
    fProfileBinFile.newRecord(newindex,&aProfile);
    // make sure header is up to date (in case we terminate improperly)
    fProfileBinFile.flushHeader();
  }
  else {
    // find record
    fProfileBinFile.updateRecord(aProfileIndex,&aProfile);
  }
  // return index of data
  return newindex;
} // TBinfileClientConfig::writeProfile


// - delete profile (and all of its targets)
bool TBinfileClientConfig::deleteProfile(sInt32 aProfileIndex)
{
  // remove all targets
  sInt32 idx;
  uInt32 profileID = getIDOfProfile(aProfileIndex);
  if (profileID==0) return false;
  do {
    // find first (still existing) target for this profile
    idx = findTargetIndex(profileID,0);
    if (idx<0) break; // no more targets
    // if we have separate changelogs or this is the last profile, also delete targets' changelogs
    if (fSeparateChangelogs || fProfileBinFile.getNumRecords()<=1) {
      // clean related changelogs
      cleanChangeLogForTarget(idx,profileID);
    }
    // delete target
    deleteTarget(idx);
  } while (true);
  // remove profile itself
  if (fProfileBinFile.deleteRecord(aProfileIndex)!=BFE_OK) return false;
  if (fProfileBinFile.getNumRecords()==0) {
    // last profile deleted, remove file itself to clean up as much as possible
    fProfileBinFile.closeAndDelete();
    fProfileBinFile.open(); // re-open = re-create
  }
  else {
    // make sure header is up to date (in case we terminate improperly)
    fProfileBinFile.flushHeader();
  }
  // return
  return true;
} // TBinfileClientConfig::deleteProfile


// - get number of existing profiles
sInt32 TBinfileClientConfig::numProfiles(void)
{
  return fProfileBinFile.getNumRecords();
} // TBinfileClientConfig::numProfiles


// - get profile, returns index or -1 if no more profiles
sInt32 TBinfileClientConfig::getProfile(
  sInt32 aProfileIndex,
  TBinfileDBSyncProfile &aProfile
)
{
  if (fProfileBinFile.readRecord(aProfileIndex,&aProfile)!=BFE_OK)
    return -1;
  return aProfileIndex;
} // TBinfileClientConfig::getProfile


// - get index of profile by ID
sInt32 TBinfileClientConfig::getProfileIndex(uInt32 aProfileID)
{
  TBinfileDBSyncProfile profile;
  return getProfileByID(aProfileID,profile);
} // TBinfileClientConfig::getProfileIndex


// - get profile by ID, returns index or -1 if no profile found
//   Note: aProfile might be writte even if profile is not found
sInt32 TBinfileClientConfig::getProfileByID(
  uInt32 aProfileID,
  TBinfileDBSyncProfile &aProfile
)
{
  sInt32 aIndex=0;
  while (true) {
    aIndex = getProfile(aIndex,aProfile);
    if (aIndex<0) break; // not found
    // check ID
    if (aProfile.profileID == aProfileID) break;
    // next profile
    aIndex++;
  }
  return aIndex;
} // TBinfileClientConfig::getProfile


// - get profile index from name, returns index or -1 if no matching profile found
sInt32 TBinfileClientConfig::getProfileIndexByName(cAppCharP aProfileName)
{
  sInt32 np = numProfiles();
  sInt32 pi = np;
  TBinfileDBSyncProfile profile;
  while (pi>0) {
    pi--;
    getProfile(pi,profile);
    if (strucmp(profile.profileName,aProfileName)==0) {
      return pi; // found, return index
    }
  }
  return -1; // not found
} // TBinfileClientConfig::getProfileByName


// - get ID of profile from index, 0 if none found
uInt32 TBinfileClientConfig::getIDOfProfile(sInt32 aProfileIndex)
{
  // get profile ID
  TBinfileDBSyncProfile profile;
  if (fProfileBinFile.readRecord(aProfileIndex,&profile)!=BFE_OK)
    return 0;
  return profile.profileID;
} // TBinfileClientConfig::getIDOfProfile



// - get last sync (earliest of lastSync of all sync-enabled targets), 0=never
bool TBinfileClientConfig::getProfileLastSyncTime(uInt32 aProfileID, lineartime_t &aLastSync, bool &aZapsServer, bool &aZapsClient)
{
  sInt32 i,targetIndex;
  uInt32 dbid;
  lineartime_t targlast;
  aLastSync=maxLinearTime;
  aZapsServer=false;
  aZapsClient=false;
  bool zs,zc;
  i=0;
  do {
    targetIndex=findTargetIndex(aProfileID,i++);
    if (targetIndex<0) break;
    // get last sync of this target
    if (getTargetLastSyncTime(targetIndex,targlast,zs,zc,dbid)) {
      // enabled target
      if (targlast<aLastSync) aLastSync=targlast;
      aZapsServer = aZapsServer || zs;
      aZapsClient = aZapsClient || zc;
    }
  } while(true);
  return aLastSync!=maxLinearTime; // at least one enabled target
} // TBinfileClientConfig::getProfileLastSyncTime


// - get last sync (earliest of lastSync of all sync-enabled targets), 0=never
bool TBinfileClientConfig::getTargetLastSyncTime(sInt32 aTargetIndex, lineartime_t &aLastSync, bool &aZapsServer, bool &aZapsClient, uInt32 &aDBID)
{
  // get target
  TBinfileDBSyncTarget target;
  if (fTargetsBinFile.readRecord(aTargetIndex,&target)!=BFE_OK)
    return false; // no such target
  return getTargetLastSyncTime(target, aLastSync, aZapsServer, aZapsClient, aDBID);
} // TBinfileClientConfig::getTargetLastSyncTime


// - get last sync (earliest of lastSync of all sync-enabled targets), 0=never
bool TBinfileClientConfig::getTargetLastSyncTime(TBinfileDBSyncTarget &aTarget, lineartime_t &aLastSync, bool &aZapsServer, bool &aZapsClient, uInt32 &aDBID)
{
  // returns info even if not enabled
  // Note: if suspended, zapping has already occurred (and has been confirmed, so don't report it here)
  aZapsServer=aTarget.syncmode==smo_fromclient && (aTarget.forceSlowSync || *(aTarget.remoteAnchor)==0) && aTarget.resumeAlertCode==0;
  aZapsClient=aTarget.syncmode==smo_fromserver && (aTarget.forceSlowSync || *(aTarget.remoteAnchor)==0) && aTarget.resumeAlertCode==0;
  // return info anyway
  aLastSync=aTarget.lastSync;
  aDBID=aTarget.localDBTypeID;
  return aTarget.enabled;
} // TBinfileClientConfig::getTargetLastSyncTime



// - check for feature enabled (profile or license dependent)
bool TBinfileClientConfig::isFeatureEnabled(sInt32 aProfileIndex, uInt16 aFeatureNo)
{
  TBinfileDBSyncProfile profile;
  if (getProfile(aProfileIndex,profile)<0)
    return isFeatureEnabled((TBinfileDBSyncProfile *)NULL, aFeatureNo); // no profile, get general availablity
  else
    return isFeatureEnabled(&profile, aFeatureNo); // check if available in this profile
} // TBinfileClientConfig::isFeatureEnabled


// - check for feature enabled (profile or license dependent)
bool TBinfileClientConfig::isFeatureEnabled(TBinfileDBSyncProfile *aProfileP, uInt16 aFeatureNo)
{
  // first check for general (local) availability
  bool locAvail;
  switch (aFeatureNo) {
    #ifdef CGI_SERVER_OPTIONS
    // we have the range options
    case APP_FTR_EVENTRANGE:
    case APP_FTR_EMAILRANGE:
      locAvail=true; break;
    #endif
    default:
      locAvail=false;
  }
  // if not locally available anyway - check for general (global) availability
  if (!locAvail && !getSyncAppBase()->isFeatureEnabled(aFeatureNo))
    return false; // this feature is not enabled at all
  #ifdef FTRFLAGS_ALWAYS_AVAILABLE
  // check for profile-level enabling
  // - get flag for feature No
  uInt8 ftrflag=0;
  switch (aFeatureNo) {
    case APP_FTR_AUTOSYNC:
      ftrflag=ftrflg_autosync; break;
    case APP_FTR_IPP:
      ftrflag=ftrflg_dmu; break;
    case APP_FTR_EVENTRANGE:
    case APP_FTR_EMAILRANGE:
      ftrflag=ftrflg_range; break;
    default : return true; // not a profile-level feature, but it is globally on -> is enabled
  }
  // - check if generally enabled even if not in profile flags
  if ((ftrflag & FTRFLAGS_ALWAYS_AVAILABLE)!=0)
    return true; // always available (hardcoded)
  // - now it depends on the profile flags
  return (aProfileP && (aProfileP->featureFlags & ftrflag)!=0);
  #else
  // no profile level feature enabling - everything that is globally enabled is also enabled in this profile
  return true;
  #endif
} // TBinfileClientConfig::isFeatureEnabled


// - check for feature enabled (profile or license dependent)
bool TBinfileClientConfig::isReadOnly(sInt32 aProfileIndex, uInt8 aReadOnlyMask)
{
  TBinfileDBSyncProfile profile;
  if (getProfile(aProfileIndex,profile)<0)
    return true; // make all read-only if we have no profile
  else
    return isReadOnly(&profile, aReadOnlyMask); // check if available in this profile
} // TBinfileClientConfig::isFeatureEnabled


// - check for readonly (profile or license dependent)
bool TBinfileClientConfig::isReadOnly(TBinfileDBSyncProfile *aProfileP, uInt8 aReadOnlyMask)
{
  // check special hardcoded cases
  #ifdef HARD_CODED_DBNAMES
  if (aReadOnlyMask & rdonly_dbpath) return true; // with hardcoded DB names, these are ALWAYS readonly
  #endif
  #ifdef HARD_CODED_SERVER_URI
  if (aReadOnlyMask & rdonly_URI) return true; // with hardcoded URI, this is ALWAYS readonly
  #endif

  // just check profile flags
  if (!aProfileP) return true; // make all read-only if we have no profile
  return aProfileP->readOnlyFlags & aReadOnlyMask; // readonly if flag is set in profile
} // TBinfileClientConfig::isReadOnly


// - check if datastore of specified target is available
//   in the given profile
bool TBinfileClientConfig::isTargetAvailable(
  TBinfileDBSyncProfile *aProfileP,
  uInt32 aLocalDBTypeID
) {
  TLocalDSList::iterator pos;
  for (pos=fDatastores.begin();pos!=fDatastores.end();pos++) {
    TBinfileDSConfig *cfgP = static_cast<TBinfileDSConfig *>(*pos);
    if (
      cfgP->fLocalDBTypeID==aLocalDBTypeID
    ) {
      // right datastore config found
      // - return it's availability status
      return cfgP->isAvailable(aProfileP);
    }
  }
  // datastore not found -> not available
  return false;
} // TBinfileClientConfig::isTargetAvailable


// - find available target for profile by DB ID/name. Returns target index or -1 if
//   DBTypeID not available in this profile/license or not implemented at all
sInt32 TBinfileClientConfig::findAvailableTargetIndexByDBInfo(
  TBinfileDBSyncProfile *aProfileP, // profile to search targets for
  uInt32 aLocalDBTypeID,
  const char *aLocalDBName // can be NULL if name does not matter
)
{
  if (!isTargetAvailable(aProfileP,aLocalDBTypeID)) return -1;
  if (!aProfileP) return -1; // no profile, no target
  return findOrCreateTargetIndexByDBInfo(aProfileP->profileID,aLocalDBTypeID,aLocalDBName);
} // TBinfileClientConfig::findAvailableTargetIndexByDBInfo


// - find target for profile by DB ID/name. Returns target index or -1 if none found
sInt32 TBinfileClientConfig::findOrCreateTargetIndexByDBInfo(
  uInt32 aProfileID, // profile to search targets for
  uInt32 aLocalDBTypeID,
  const char *aLocalDBName // can be NULL if name does not matter
)
{
  sInt32 targidx;
  TBinfileDBSyncTarget target;

  targidx=findTargetIndexByDBInfo(aProfileID,aLocalDBTypeID,aLocalDBName);
  if (targidx<0) {
    // does not exist yet, create it now
    TLocalDSList::iterator pos;
    for (pos=fDatastores.begin();pos!=fDatastores.end();pos++) {
      if ((*pos)->isAbstractDatastore()) continue; // only non-abstract datastores are guaranteed binfileds and have a target
      TBinfileDSConfig *cfgP = static_cast<TBinfileDSConfig *>(*pos);
      if (
        cfgP->fLocalDBTypeID==aLocalDBTypeID &&
        (aLocalDBName==NULL || cfgP->fLocalDBPath==aLocalDBName)
      ) {
        // right datastore found
        cfgP->initTarget(target,aProfileID,NULL,DEFAULT_DATASTORES_ENABLED); // init default
        // write new target and get a index back
        targidx=writeTarget(-1,target);
        // done
        break;
      }
    }
  }
  return targidx;
} // findOrCreateTargetIndexByDBInfo


// - find target for profile by DB ID/name. Returns target index or -1 if none found
sInt32 TBinfileClientConfig::findTargetIndexByDBInfo(
  uInt32 aProfileID, // profile to search targets for
  uInt32 aLocalDBTypeID,
  const char *aLocalDBName // can be NULL if name does not matter
)
{
  // now search targets
  TBinfileDBSyncTarget target;
  sInt32 maxidx = fTargetsBinFile.getNumRecords();
  sInt32 targidx;
  for (targidx=0; targidx<maxidx; targidx++) {
    fTargetsBinFile.readRecord(targidx,&target);
    if (
      target.remotepartyID == aProfileID &&
      target.localDBTypeID==aLocalDBTypeID &&
      (aLocalDBName==NULL || strucmp(target.localDBPath,aLocalDBName)==0)
    ) {
      // target found, return its index
      return targidx;
    }
  }
  // none found
  return -1;
} // TBinfileClientConfig::findTargetIndexByDBInfo


// - find n-th target for profile. Returns target index or -1 if none found
sInt32 TBinfileClientConfig::findTargetIndex(
  uInt32 aProfileID, // profile to search targets for
  sInt32 aTargetSeqNum // sequence number (0..n)
)
{
  // now search targets
  TBinfileDBSyncTarget target;
  sInt32 maxidx = fTargetsBinFile.getNumRecords();
  sInt32 targidx;
  for (targidx=0; targidx<maxidx; targidx++) {
    fTargetsBinFile.readRecord(targidx,&target);
    if (target.remotepartyID == aProfileID) {
      // belongs to that profile
      if (aTargetSeqNum<=0) {
        // n-th target found, return its index
        return targidx;
      }
      aTargetSeqNum--; // next in sequence
    }
  }
  // none found
  return -1;
} // TBinfileClientConfig::findTargetIndex


// - write target for profile, returns index of target
sInt32 TBinfileClientConfig::writeTarget(
  sInt32 aTargetIndex,  // -1 if adding new target
  const TBinfileDBSyncTarget &aTarget
)
{
  uInt32 newindex;

  if (aTargetIndex<0) {
    // create new record
    fTargetsBinFile.newRecord(newindex,&aTarget);
    // make sure header is up to date (in case we terminate improperly)
    fTargetsBinFile.flushHeader();
    return newindex;
  }
  else {
    // update record
    fTargetsBinFile.updateRecord(aTargetIndex,&aTarget);
    return aTargetIndex;
  }
} // TBinfileClientConfig::writeTarget


// - delete target
bool TBinfileClientConfig::deleteTarget(
  sInt32 aTargetIndex
)
{
  if (fTargetsBinFile.deleteRecord(aTargetIndex)!=BFE_OK) return false;
  if (fTargetsBinFile.getNumRecords()==0) {
    // last target deleted, remove file itself to clean up as much as possible
    fTargetsBinFile.closeAndDelete();
    fTargetsBinFile.open(); // re-open = re-create
  }
  else {
    // make sure header is up to date (in case we terminate improperly)
    fTargetsBinFile.flushHeader();
  }
  return true; // ok
} // TBinfileClientConfig::deleteTarget


// - get target info
sInt32 TBinfileClientConfig::getTarget(
  sInt32 aTargetIndex,
  TBinfileDBSyncTarget &aTarget
)
{
  if (aTargetIndex<0) return -1; // no valid target index, do not even try to read
  if (fTargetsBinFile.readRecord(aTargetIndex,&aTarget)!=BFE_OK)
    return -1;
  return aTargetIndex;
} // TBinfileClientConfig::getTarget


// get base name for profile-dependent names
string TBinfileClientConfig::relatedDBNameBase(cAppCharP aDBName, sInt32 aProfileID)
{
  string basefilename;
  // prepare base name
  getBinFilesPath(basefilename);
  basefilename += aDBName;
  if (fSeparateChangelogs && aProfileID>=0) {
    StringObjAppendPrintf(basefilename, "_%d",aProfileID);
  }
  return basefilename;
}


// completely clear changelog and pending maps for specified target
void TBinfileClientConfig::cleanChangeLogForTarget(sInt32 aTargetIndex, sInt32 aProfileID)
{
  TBinfileDBSyncTarget target;
  getTarget(aTargetIndex,target);
  cleanChangeLogForDBname(target.dbname, aProfileID);
} // TBinfileClientConfig::cleanChangeLogForTarget


// completely clear changelog and pending maps for specified database name and profile
void TBinfileClientConfig::cleanChangeLogForDBname(cAppCharP aDBName, sInt32 aProfileID)
{
  // open changelog. Name is datastore name with _XXX_clg.bfi suffix (XXX=profileID, if negative, combined changelog/maps/pendingitem will be deleted)
  string basefilename = relatedDBNameBase(aDBName, aProfileID);
  string filename;
  TBinFile binfile;
  // delete changelog
  filename = basefilename + CHANGELOG_DB_SUFFIX;
  binfile.setFileInfo(filename.c_str(),CHANGELOG_DB_VERSION,CHANGELOG_DB_ID,0);
  binfile.closeAndDelete();
  // delete pending maps
  filename = basefilename + PENDINGMAP_DB_SUFFIX;
  binfile.setFileInfo(filename.c_str(),PENDINGMAP_DB_VERSION,PENDINGMAP_DB_ID,0);
  binfile.closeAndDelete();
  // delete pending item
  filename = basefilename + PENDINGITEM_DB_SUFFIX;
  binfile.setFileInfo(filename.c_str(),PENDINGITEM_DB_VERSION,PENDINGITEM_DB_ID,0);
  binfile.closeAndDelete();
} // TBinfileClientConfig::cleanChangeLogForDBname



void TBinfileClientConfig::separateDBFile(cAppCharP aDBName, cAppCharP aDBSuffix, sInt32 aProfileID)
{
  TBinFile sourceFile;
  TBinFile targetFile;
  string sourceName = relatedDBNameBase(aDBName, -1) + aDBSuffix; // without profile ID in name
  string targetName = relatedDBNameBase(aDBName, aProfileID) + aDBSuffix; // with profile ID in name
  sourceFile.setFileInfo(sourceName.c_str(), 0, 0, 0);
  targetFile.setFileInfo(targetName.c_str(), 0, 0, 0);
  targetFile.createAsCopyFrom(sourceFile);
}


// separate changelogs and other related files into separate files for each profile
void TBinfileClientConfig::separateChangeLogsAndRelated(cAppCharP aDBName)
{
  // iterate over all profiles
  TBinfileDBSyncProfile profile;
  sInt32 idx = 0;
  // set up original basename
  while (true) {
    idx = getProfile(idx,profile);
    if (idx<0) break; // no more profiles
    // copy all dependent files of that profile and the given database
    // - copy changelog
    separateDBFile(aDBName,CHANGELOG_DB_SUFFIX,profile.profileID);
    // - copy pendingmaps
    separateDBFile(aDBName,PENDINGMAP_DB_SUFFIX,profile.profileID);
    // - copy pendingitem
    separateDBFile(aDBName,PENDINGITEM_DB_SUFFIX,profile.profileID);
    // next profile
    idx++;
  }
  // - delete original files that were shared between profiles
  cleanChangeLogForDBname(aDBName,-1);
}




// remote provisioning (using generic code in include file)
#include "clientprovisioning_inc.cpp"
// Autosync mechanisms (using generic code in include file)
#include "clientautosync_inc.cpp"

/*
 * Implementation of TBinfileImplClient
 */

/* public TBinfileImplClient members */


TBinfileImplClient::TBinfileImplClient(TSyncAppBase *aSyncAppBaseP, TSyncSessionHandle *aSyncSessionHandleP, cAppCharP aSessionID) :
  TStdLogicAgent(aSyncAppBaseP, aSyncSessionHandleP, aSessionID),
  fConfigP(NULL)
{
  // get config for agent
  TRootConfig *rootcfgP = aSyncAppBaseP->getRootConfig();
  // - save direct link to agent config for easy reference
  fConfigP = static_cast<TBinfileClientConfig *>(rootcfgP->fAgentConfigP);
  // - make profile invalid
  fProfileIndex=-1;
  fProfileDirty=false;
  // Note: Datastores are already created from config
} // TBinfileImplClient::TBinfileImplClient


TBinfileImplClient::~TBinfileImplClient()
{
  // make sure everything is terminated BEFORE destruction of hierarchy begins
  TerminateSession();
} // TBinfileImplClient::~TBinfileImplClient


// Terminate session
void TBinfileImplClient::TerminateSession()
{
  if (!fTerminated && fConfigP->fBinfilesActive) {
    // save profile changes
    if (fProfileIndex>=0 && fProfileDirty) {
      fConfigP->fProfileBinFile.updateRecord(fProfileIndex,&fProfile);
      fProfileDirty=false;
      fProfileIndex=-1;
    }
    // now reset session
    InternalResetSession();
    // make sure all data is flushed
    fConfigP->fProfileBinFile.flushHeader();
    fConfigP->fTargetsBinFile.flushHeader();
  }
  inherited::TerminateSession();
} // TBinfileImplClient::TerminateSession



#ifdef ENGINEINTERFACE_SUPPORT

// set profileID to client session before doing first SessionStep
void TBinfileImplClient::SetProfileSelector(uInt32 aProfileSelector)
{
  if (aProfileSelector==0)
    fProfileSelectorInternal = DEFAULT_PROFILE_ID;
  else
    fProfileSelectorInternal = fConfigP->getProfileIndex(aProfileSelector);
} // TBinfileImplClient::SetProfileSelector


/// @brief Get new session key to access details of this session
appPointer TBinfileImplClient::newSessionKey(TEngineInterface *aEngineInterfaceP)
{
  return new TBinFileAgentParamsKey(aEngineInterfaceP,this);
} // TBinfileImplClient::newSessionKey

#endif // ENGINEINTERFACE_SUPPORT


// Reset session
void TBinfileImplClient::InternalResetSession(void)
{
  // reset all datastores now to make sure all DB activity is done
  // before we possibly close session-global databases
  // (Note: TerminateDatastores() will be called again by TSyncSession)
  TerminateDatastores();
  fRemoteFlags=0; // no remote-specifics enabled so far
} // TBinfileImplClient::InternalResetSession


// Virtual version
void TBinfileImplClient::ResetSession(void)
{
  if (fConfigP->fBinfilesActive) {
    // do my own stuff
    InternalResetSession();
  }
  // let ancestor do its stuff
  TStdLogicAgent::ResetSession();
} // TBinfileImplClient::ResetSession


// - load remote connect params (syncml version, type, format and last nonce)
//   Note: agents that can cache this information between sessions will load
//   last info here.
void TBinfileImplClient::loadRemoteParams(void)
{
  if (!fConfigP->fBinfilesActive || fProfileIndex<0) {
    // not active or no profile loaded, let ancestor handle case
    TStdLogicAgent::loadRemoteParams();
  }
  else {
    // copy profile cached values
    fSyncMLVersion=
      fProfile.lastSyncMLVersion >= numSyncMLVersions ?
      syncml_vers_unknown : // if profile has higher version than we support, reset to unknown
      fProfile.lastSyncMLVersion;
    fRemoteRequestedAuth=
      fProfile.lastAuthMethod >= numAuthTypes ?
      auth_md5 : // if auth is unknown, reset to MD5
      fProfile.lastAuthMethod;
    fRemoteRequestedAuthEnc=
      fProfile.lastAuthFormat >= numFmtTypes ?
      fmt_b64 : // if format is unknown, reset to B64
      fProfile.lastAuthFormat;
    fRemoteNonce=fProfile.lastNonce;
    fRemoteFlags=fProfile.remoteFlags; // init remote specific behaviour flags from profile
  }
  // modify some behaviour based on remote flags
  if (fRemoteFlags & remotespecs_noDS12Filters)
    fServerHasSINCEBEFORE = false; // prevent auto-generated SINCE/BEFORE
} // TBinfileImplClient::loadRemoteParams


// - save remote connect params for use in next session (if descendant implements it)
void TBinfileImplClient::saveRemoteParams(void)
{
  if (fConfigP->fBinfilesActive && fProfileIndex>=0) {
    // save values to profile
    // - SyncML version (save it only if it is "better" than what we knew so far)
    if (fSyncMLVersion > fProfile.lastSyncMLVersion) {
      fProfile.lastSyncMLVersion=fSyncMLVersion;
    }
    fProfile.lastAuthMethod=fRemoteRequestedAuth;
    fProfile.lastAuthFormat=fRemoteRequestedAuthEnc;
    fProfile.remoteFlags=fRemoteFlags; // save remote-specific behaviour flags
    fProfileDirty=true;
    AssignCString(fProfile.lastNonce,fRemoteNonce.c_str(),maxnoncesiz);
  }
} // TBinfileImplClient::saveRemoteParams


// check remote devinf to detect special behaviour needed for some servers.
localstatus TBinfileImplClient::checkRemoteSpecifics(SmlDevInfDevInfPtr_t aDevInfP, SmlDevInfDevInfPtr_t *aOverrideDevInfP)
{
  if (fConfigP->fBinfilesActive && aDevInfP) {
    // check for some specific servers we KNOW they need special treatment
    uInt8 setFlags = 0;
    uInt8 clearFlags = 0;
    if (strnncmp(smlPCDataToCharP(aDevInfP->devid),"OracleSyncServer")==0) {
      // Oracle OCS
      // - unqiue ID even if device might have non-unique ones
      // - no X-nnn type params
      // - no DS 1.2 <filter>
      PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("Detected Oracle OCS Server - suppress dynamic X-nnnn TYPE params, and <filter>"));
      setFlags |= remotespecs_noXTypeParams+remotespecs_noDS12Filters;
      #ifndef GUARANTEED_UNIQUE_DEVICID
      // - Oracle OCS needs unique deviceID/locURI under ALL circumstances as they do
      //   session tracking by locURI, so we always use a hash of deviceID + username
      //   as visible deviceID
      PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("OCS with device that has not guaranteed unique ID - use user+devid hash for Source LocURI"));
      setFlags |= remotespecs_devidWithUserHash;
      #else
      clearFlags |= remotespecs_devidWithUserHash;
      #endif
    }
    else if (strnncmp(smlPCDataToCharP(aDevInfP->mod), "Beehive", 7)==0) {
      // Oracle Beehive, <mod> starts with "Beehive", i.e. "Beehive R1"
      // - no DS 1.2 <filter>
      PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("Detected Oracle Beehive - suppress <filter>"));
      setFlags |= remotespecs_noDS12Filters;
      clearFlags |= remotespecs_noXTypeParams+remotespecs_devidWithUserHash;
    }
    else {
      // make sure oracle-specific flags are cleared for all other server types
      clearFlags |= remotespecs_noXTypeParams+remotespecs_noDS12Filters+remotespecs_devidWithUserHash;
    }
    // now apply
    sInt8 newFlags = (fRemoteFlags & ~clearFlags) | setFlags;
    if (newFlags != fRemoteFlags) {
      // change of remotespecs_devidWithUserHash needs session restart
      bool needrestart = (newFlags & remotespecs_devidWithUserHash) != (fRemoteFlags & remotespecs_devidWithUserHash);
      // remote flags need to be changed
      fRemoteFlags = newFlags;
      // save them into profile
      saveRemoteParams();
      // restart session when needed
      if (needrestart) {
        // - abort session, but have main loop retry starting it
        PDEBUGPRINTFX(DBG_ERROR,("Warning: must restart session with changed remotespecs_devidWithUserHash"));
        AbortSession(LOCERR_RESTART,true);
      }
    }
  }
  // let session handle other details
  return inherited::checkRemoteSpecifics(aDevInfP, aOverrideDevInfP);
} // TBinfileImplClient::checkRemoteSpecifics


#ifdef IPP_SUPPORT

// generates custom PUT in case IPP/DMU is enabled to request settings
void TBinfileImplClient::issueCustomGetPut(bool aGotDevInf, bool aSentDevInf)
{
  if (fConfigP->fBinfilesActive) {
    // get autosync PUT req string
    string req;
    fConfigP->autosync_get_putrequest(req);
    // now create a PUT
    if (!req.empty()) {
      TPutCommand *putcommandP = new TPutCommand(this);
      putcommandP->setMeta(newMetaType(IPP_PARAMS_ITEM_METATYPE));
      SmlItemPtr_t putItemP = putcommandP->addSourceLocItem(IPP_PARAMS_LOCURI_REQ);
      // - add data to item
      putItemP->data = newPCDataString(req.c_str());
      putcommandP->allowFailure(); // do not abort session in case server does not understand the command
      ISSUE_COMMAND_ROOT(this,putcommandP);
    }
  }
  // let ancestors issue their custom gets and puts, if any
  inherited::issueCustomGetPut(aGotDevInf, aSentDevInf);
} // TBinfileImplClient::issueCustomGetPut

#endif // IPP_SUPPORT


// handler of custom IPP/DMU put and result commands
void TBinfileImplClient::processPutResultItem(bool aIsPut, const char *aLocUri, TSmlCommand *aPutResultsCommandP, SmlItemPtr_t aPutResultsItemP, TStatusCommand &aStatusCommand)
{
  if (fConfigP->fBinfilesActive) {
    #ifdef IPP_SUPPORT
    // check for DMU specials
    if (strucmp(relativeURI(aLocUri),relativeURI(IPP_PARAMS_LOCURI_CFG))==0) {
      // get DMU string
      const char *ippstring = smlItemDataToCharP(aPutResultsItemP);
      PDEBUGPRINTFX(DBG_HOT,("received IPP config string: %s",ippstring));
      // process it
      string tag,value;
      while (ippstring && *ippstring) {
        ippstring=nextTag(ippstring,tag,value);
        // set ipp params in current profile
        fConfigP->ipp_setparam(tag.c_str(),value.c_str(),fProfile.ippSettings);
        // make sure autosync profile copy gets updated as well
        if (fConfigP->fAutosyncProfileLastidx==fProfileIndex) {
          fConfigP->fAutosyncProfile = fProfile; // copy profile into autosync
          fConfigP->autosync_condchanged(); // do immediately check settings at next autosync step
        }
        // make sure profile gets saved
        fProfileDirty=true;
      }
      // is ok
      aStatusCommand.setStatusCode(200); // is ok
      aStatusCommand.dontSend(); // ..but do not send it (%%% as session would then end with error 9999)
    }
    else
    #endif
    #ifdef SETTINGS_PROVISIONING_VIA_PUT
    // check for provisioning strings arriving via PUT command
    if (strucmp(relativeURI(aLocUri),relativeURI(SETTINGS_LOCURI_CFG))==0) {
      // get provisioning string
      const char *provstring = smlItemDataToCharP(aPutResultsItemP);
      PDEBUGPRINTFX(DBG_HOT,("received settings provisioning string: %s",provstring));
      // process it
      // - but first save current profile
      fConfigP->writeProfile(fProfileIndex,fProfile);
      // - now modify profile (may include the current profile)
      sInt32 activeprofile;
      bool provok = fConfigP->executeProvisioningString(provstring, activeprofile);
      // - reload current profile (possibly modified)
      fConfigP->getProfile(fProfileIndex,fProfile);
      // is ok
      aStatusCommand.setStatusCode(provok ? 200 : 400); // ok or bad request
      aStatusCommand.dontSend(); // ..but do not send it (%%% as session would then end with error 9999)
    }
    else
    #endif
    {
      // let ancestors process it
      inherited::processPutResultItem(aIsPut,aLocUri,aPutResultsCommandP,aPutResultsItemP,aStatusCommand);
    }
  }
  else {
    // let ancestors process it
    inherited::processPutResultItem(aIsPut,aLocUri,aPutResultsCommandP,aPutResultsItemP,aStatusCommand);
  }
} // TBinfileImplClient::processPutResultItem





#ifdef PROTOCOL_SELECTOR
// Names of protocols according to TTransportProtocols
const char * const Protocol_Names[num_transp_protos] = {
  "", // none, protocol contained in URI
  "http://",
  "https://",
  "wsp://", // WSP protocol
  "OBEX:IRDA", // OBEX/IRDA protocol
  "OBEX:BT", // OBEX/BT protocol
  "OBEX://", // OBEX/TCP protocol
};
#endif


// - selects a profile (returns LOCERR_NOCFG if profile not found)
//   Note: This call must create and initialize all datastores that
//         are to be synced with that profile.
localstatus TBinfileImplClient::SelectProfile(uInt32 aProfileSelector, bool aAutoSyncSession)
{
  uInt32 recidx,maxidx;
  // detect special tunnel session's selection
  bool tunnel = aProfileSelector==TUNNEL_PROFILE_ID;
  // select profile if active
  if (fConfigP->fBinfilesActive) {
    if (tunnel) {
      aProfileSelector=DEFAULT_PROFILE_ID;
    }
    // Note: profile database has already been opened in config resolve()
    if (aProfileSelector==DEFAULT_PROFILE_ID) {
      // default is first one
      aProfileSelector=0;
    }
    // try to load profile
    if (fConfigP->fProfileBinFile.readRecord(aProfileSelector,&fProfile)!=BFE_OK)
      goto defaultprofile; // use default
    // Now we have the profile
    fProfileIndex=aProfileSelector;
    fRemotepartyID=fProfile.profileID;
    // Set session parameters
    #ifdef SYNCML_ENCODING_OVERRIDE
    fEncoding = SYNCML_ENCODING_OVERRIDE; // we use a globally defined encoding
    #else
    fEncoding = fProfile.encoding; // we use the profile's encoding
    #endif

    #ifdef HARD_CODED_SERVER_URI
    // - Hard coded base URL of server
    fRemoteURI=DEFAULT_SERVER_URI; // always use fixed URI
    #else
    // - configured URL
    //   check config-level fixed URI first
    if (!fConfigP->fServerURI.empty()) {
      // override server URL from fixed value in config
      fRemoteURI = fConfigP->fServerURI; // config
      if (strucmp(fProfile.serverURI,fRemoteURI.c_str())!=0) {
        PDEBUGPRINTFX(DBG_ERROR,("Warning - <serverurl> config overrides/overwrites server URL in profile"));
        // re-adjust profile
        AssignCString(fProfile.serverURI, fConfigP->fServerURI.c_str(), maxurisiz); // copy predefined URI back into profile
        fProfile.readOnlyFlags |= rdonly_URI; // make it read-only
      }
    }
    else {
      // use URL from profile
      fRemoteURI=fProfile.serverURI;
    }
    #endif // not HARD_CODED_SERVER_URI
    // check for URI that is a template and need inserts:
    size_t n;
    // - %%% check future other inserts here, \u should be last because if there's no \u, standard
    //   CUSTOM_URI_SUFFIX mechanism may apply in else branch
    // - \u for URIpath
    n = fRemoteURI.find("\\u");
    if (n!=string::npos) {
      // URIPath may only not contain any special chars that might help to inject different server URLs
      string up = fProfile.URIpath;
      if (up.find_first_of(":/?.&=%,;")!=string::npos)
        fRemoteURI.erase(n, 2); // no insert, invalid chars in URIpath
      else
        fRemoteURI.replace(n, 2, up); // insert URIPath instead of \u
    }
    #ifdef CUSTOM_URI_SUFFIX
    else {
      // Only if original URI is not a template
      // - append custom URI suffix stored in serverURI field of profile (if one is set)
      if (*(fProfile.URIpath))
      {
        // - append delimiter first if one defined (CUSTOM_URI_SUFFIX not NULL)
        const char *p=CUSTOM_URI_SUFFIX;
        if (p) fRemoteURI.append(p);
        // - now append custom URI suffix
        fRemoteURI.append(fProfile.URIpath);
      }
    }
    #endif // CUSTOM_URI_SUFFIX
    #ifdef PROTOCOL_SELECTOR
    fRemoteURI.insert(0,Protocol_Names[fProfile.protocol]);
    fNoCRCPrefixLen=strlen(Protocol_Names[fProfile.protocol]);
    #endif // PROTOCOL_SELECTOR
    fServerUser=fProfile.serverUser;
    getUnmangled(fServerPassword,fProfile.serverPassword,maxupwsiz);
    // - HTTP auth
    fTransportUser=fProfile.transportUser;
    getUnmangled(fTransportPassword,fProfile.transportPassword,maxupwsiz);
    // - proxy
    fSocksHost.erase(); // default to none
    fProxyHost.erase();
    fProxyUser.erase();
    fProxyPassword.erase();
    #ifdef PROXY_SUPPORT
    if (fProfile.useProxy)
    {
      fSocksHost=fProfile.socksHost;
      fProxyHost=fProfile.proxyHost;
      fProxyUser=fProfile.proxyUser;
      getUnmangled(fProxyPassword,fProfile.proxyPassword,maxupwsiz);;
      PDEBUGPRINTFX(DBG_TRANSP,("Sync Profile contains active proxy settings: http=%s, socks=%s, proxyuser=%s",fProxyHost.c_str(), fSocksHost.c_str(), fProxyUser.c_str()));
    }
    #endif // PROXY_SUPPORT
    // check for forced legacy mode
    fLegacyMode = fProfile.profileFlags & PROFILEFLAG_LEGACYMODE;
    // check for lenient mode
    fLenientMode = fProfile.profileFlags & PROFILEFLAG_LENIENTMODE;
    // - get and increment session ID and save for next session
    //   Note: as auth retries will increment the ID as well, we inc by 5
    //         to avoid repeating the ID too soon
    fProfile.sessionID+=5;
    fProfileDirty=true;
    fClientSessionNo=fProfile.sessionID;
    // Note: loadRemoteParams will fetch the cached params from fProfile
    // Reset session after profile change (especially fRemoteURI)
    // and also remove any datastores we might have
    ResetAndRemoveDatastores();
    // in case of tunnel, don't touch datastores
    if (tunnel) return LOCERR_OK;
    // Now iterate trough associated target records and create datastores
    maxidx=fConfigP->fTargetsBinFile.getNumRecords();
    TBinfileDBSyncTarget target;
    for (recidx=0; recidx<maxidx; recidx++) {
      // - get record
      if (fConfigP->fTargetsBinFile.readRecord(recidx,&target)==BFE_OK) {
        // check if this one of my targets
        if (target.remotepartyID == fRemotepartyID)
        {
          // get datastore config
          #ifdef HARDCODED_CONFIG
          // - traditional method by name for old monolythic builds (to make sure we don't break anything)
          TBinfileDSConfig *binfiledscfgP = static_cast<TBinfileDSConfig *>(
            getSessionConfig()->getLocalDS(target.dbname)
          );
          #else
          // - newer, engine based targets should use the DBtypeID instead, to allow change of DB name without loosing config
          TBinfileDSConfig *binfiledscfgP = static_cast<TBinfileDSConfig *>(
            getSessionConfig()->getLocalDS(NULL,target.localDBTypeID)
          );
          #endif
          // check if we have config and if this DS is available now (profile.dsAvailFlags, DSFLAGS_ALWAYS_AVAILABLE, evtl. license...)
          if (binfiledscfgP && binfiledscfgP->isAvailable(&fProfile)) {
            // check if this DB must be synced
            bool syncit=false;
            #ifdef AUTOSYNC_SUPPORT
            if (aAutoSyncSession) {
              // target enable status is not relevant, but autosync alert is
              syncit =
                binfiledscfgP->fAutosyncForced ||
                (binfiledscfgP->fAutosyncAlerted && target.enabled);
            }
            else {
              // normal session
              syncit = target.enabled && target.remoteDBpath[0]!=0; // and remote DB path specified
              binfiledscfgP->fAutosyncAlerted=false; // this is NOT an autosync session
              binfiledscfgP->fAutosyncForced=false;
            }
            #else
            syncit = target.enabled && target.remoteDBpath[0]!=0; // and remote DB path specified
            #endif
            if (syncit)
            {
              TBinfileImplDS *binfiledsP=NULL;
              if (binfiledscfgP) {
                // create datastore
                binfiledsP = static_cast<TBinfileImplDS *>(binfiledscfgP->newLocalDataStore(this));
              }
              if (binfiledsP) {
                // copy target info to datastore for later access during sync
                binfiledsP->fTargetIndex=recidx;
                binfiledsP->fTarget=target;
                // determine sync mode / flags to use
                TSyncModes myMode;
                bool mySlow;
                #ifdef AUTOSYNC_SUPPORT
                if (aAutoSyncSession && binfiledscfgP->fAutosyncAlertCode!=0) {
                  // syncmode provided from auto sync alert (e.g. SAN)
                  bool myIsSA;
                  TLocalEngineDS::getSyncModeFromAlertCode(
                    binfiledscfgP->fAutosyncAlertCode,
                    myMode,
                    mySlow,
                    myIsSA
                  );
                }
                else
                #endif
                {
                  // take it from config
                  myMode = target.syncmode;
                  mySlow = target.forceSlowSync;
                }
                // clean change logs if...
                // ...we have separate changelog or this is the only profile
                // ...this is NOT a resumable session (resumable not necessarily means that it WILL be resumed)
                // ...we're about to slow sync
                if (mySlow && target.resumeAlertCode==0 && (fConfigP->fSeparateChangelogs || fConfigP->fProfileBinFile.getNumRecords()==1)) {
                  fConfigP->cleanChangeLogForDBname(target.dbname,fProfile.profileID);
                }
                // set non-BinFile specific parameters (note that this call might
                // be to a derivate which uses additional info from fTarget to set sync params)
                binfiledsP->dsSetClientSyncParams(
                  myMode,
                  mySlow,
                  target.remoteDBpath,
                  NULL, // DB user
                  NULL, // DB password
                  NULL, // local path extension
                  // %%% add filters here later!!!
                  NULL, // filter query
                  false // filter inclusive
                );
                // prepare local datastore (basic init can be done here) and check availability
                if (binfiledsP->localDatastorePrep()) {
                  // add to datastores for this sync
                  fLocalDataStores.push_back(binfiledsP);
                }
                else {
                  // silently discard (do not sync it)
                  PDEBUGPRINTFX(DBG_ERROR,("Local Database for datastore '%s' prepares not ok -> not synced",binfiledsP->getName()));
                  // show event (alerted for no database)
                  SESSION_PROGRESS_EVENT(
                    this,
                    pev_error,
                    binfiledscfgP,
                    LOCERR_LOCDBNOTRDY,0,0
                  );
                  delete binfiledsP;
                }
              }
            } // if target DB enabled for sync
          } // if we have a datastore config for this target
        } // if target belongs to this profile
      } // if we can read the target record
    } // for all target records
    // ok if at least one datastore enabled;
    #ifdef ENGINE_LIBRARY
    // For engine library, check for a non-empty URL makes no
    // sense any more, because the app on top of libsynthesis may know how
    // to contact the server without an URL (for example, via some transport
    // which doesn't need a parameter)
    return fLocalDataStores.size()>0 ? LOCERR_OK : LOCERR_NOCFG;
    #else
    // for classic synthesis builds (winmobile, palmos) the app relies on
    // the check for a non-empty URL, so we keep it for these legacy build cases
    return fLocalDataStores.size()>0 && fRemoteURI.size()>0 ? LOCERR_OK : LOCERR_NOCFG;
    #endif
  } // active
defaultprofile:
  return inherited::SelectProfile(aProfileSelector, aAutoSyncSession);
} // TBinfileImplClient::SelectProfile

/* end of TBinfileImplClient implementation */

} // namespace sysync




// eof
