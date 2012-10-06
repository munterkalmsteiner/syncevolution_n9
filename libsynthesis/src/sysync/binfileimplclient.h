/**
 *  @File     binfileimplclient.h
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


#ifndef BINFILEIMPLCLIENT_H
#define BINFILEIMPLCLIENT_H

// includes
#include "sysync.h"
#include "stdlogicagent.h"
#include "binfileimplds.h"
#include "engineinterface.h"
#include "syncclientbase.h"

#ifndef SYSYNC_CLIENT
  #error "binfileimplclient only makes sense with client datastores"
#endif

namespace sysync {


// Support for EngineModule common interface
// =========================================

#ifdef ENGINEINTERFACE_SUPPORT

// forward
class TBinfileAgentRootKey;
class TBinfileClientConfig;

// Engine module class
class TBinfileEngineInterface :
  public TClientEngineInterface
{
  typedef TClientEngineInterface inherited;
public:
  //%%%
protected:
  /// @brief Must be derived in engineBase derivates for generating root for the appropriate settings tree
  /// @return root settings object, or NULL if failure
  virtual TSettingsKeyImpl *newSettingsRootKey(void);

  #ifndef ENGINE_LIBRARY
  #ifdef RELEASE_VERSION
    #error "this is here for Q&D testing with outlook client only"
  #endif
  /// @brief returns a new application base.
  /// @note in engineInterface based targets, this is the replacement for the formerly
  ///       global newSyncAppBase() factory function.
  virtual TSyncAppBase *newSyncAppBase(void);
  #endif

}; // TBinfileEngineInterface


// extern for accessing these in TARGETSETTING script function
extern const TStructFieldInfo TargetFieldInfos[];
extern const sInt32 numTargetFieldInfos;


// Binfile based target key
class TBinfileTargetKey :
  public TStructFieldsKey
{
  typedef TStructFieldsKey inherited;

public:
  TBinfileTargetKey(
    TEngineInterface *aEngineInterfaceP,
    sInt32 aTargetIndex,
    TBinfileDBSyncTarget *aTargetP,
    TBinfileClientConfig *aBinfileClientConfigP,
    TBinfileDSConfig *aBinfileDSConfigP
  );
  virtual ~TBinfileTargetKey();

  TBinfileDSConfig *getBinfileDSConfig(void) { return fBinfileDSConfigP; };
  TBinfileDBSyncTarget *getTarget(void) { return fTargetP; };
  TBinfileClientConfig *getBinfileClientConfig(void) { return fBinfileClientConfigP; };
  sInt32 getProfileID() { return fTargetP ? fTargetP->remotepartyID : KEYVAL_ID_UNKNOWN; };

protected:
  // return ID of this key
  virtual TSyError GetKeyID(sInt32 &aID);

  // get table describing the fields in the struct
  virtual const TStructFieldInfo *getFieldsTable(void);
  virtual sInt32 numFields(void);
  // get actual struct base address
  virtual uInt8P getStructAddr(void);

private:
  // profile
  sInt32 fTargetIndex; // index for writing back profile
  TBinfileDBSyncTarget *fTargetP;
  TBinfileClientConfig *fBinfileClientConfigP;
  TBinfileDSConfig *fBinfileDSConfigP;
}; // TBinfileTargetKey



// Binfile based targets collection key
class TBinfileTargetsKey :
  public TSettingsKeyImpl
{
  typedef TSettingsKeyImpl inherited;

public:
  TBinfileTargetsKey(TEngineInterface *aEngineInterfaceP, sInt32 aProfileID);

protected:
  // targets can be opened only by dbtype-ID
  virtual TSyError OpenSubkey(
    TSettingsKeyImpl *&aSettingsKeyP,
    sInt32 aID, uInt16 aMode
  );
private:
  sInt32 fProfileID; // profile ID
  sInt32 fTargetIterator;
  TBinfileClientConfig *fBinfileClientConfigP;
}; // TBinfileTargetsKey


#ifdef AUTOSYNC_SUPPORT

// Binfile based autosync level key
class TBinfileASLevelKey :
  public TStructFieldsKey
{
  typedef TStructFieldsKey inherited;

public:
  TBinfileASLevelKey(
    TEngineInterface *aEngineInterfaceP,
    sInt32 aLevelIndex,
    TBinfileDBSyncProfile *aProfileP,
    TBinfileClientConfig *aBinfileClientConfigP
  );
  virtual ~TBinfileASLevelKey();

protected:
  // return ID of this key
  virtual TSyError GetKeyID(sInt32 &aID);

  // get table describing the fields in the struct
  virtual const TStructFieldInfo *getFieldsTable(void);
  virtual sInt32 numFields(void);
  // get actual struct base address
  virtual uInt8P getStructAddr(void);

private:
  // profile
  sInt32 fLevelIndex; // level index
  TBinfileDBSyncProfile *fProfileP;
  TBinfileClientConfig *fBinfileClientConfigP;
}; // TBinfileASLevelKey


// Binfile based autosync levels collection key
class TBinfileASLevelsKey :
  public TSettingsKeyImpl
{
  typedef TSettingsKeyImpl inherited;

public:
  TBinfileASLevelsKey(TEngineInterface *aEngineInterfaceP, TBinfileDBSyncProfile *aProfileP);

protected:
  // levels can be opened only by ID
  virtual TSyError OpenSubkey(
    TSettingsKeyImpl *&aSettingsKeyP,
    sInt32 aID, uInt16 aMode
  );
private:
  sInt32 fASLevelIterator;
  TBinfileDBSyncProfile *fProfileP;
  TBinfileClientConfig *fBinfileClientConfigP;
}; // TBinfileASLevelsKey

#endif // AUTOSYNC_SUPPORT


// extern for accessing these in PROFILESETTING script function
extern const TStructFieldInfo ProfileFieldInfos[];
extern const sInt32 numProfileFieldInfos;

// Binfile based profile key
class TBinfileProfileKey :
  public TStructFieldsKey
{
  typedef TStructFieldsKey inherited;

public:
  TBinfileProfileKey(
    TEngineInterface *aEngineInterfaceP,
    sInt32 aProfileIndex,
    TBinfileDBSyncProfile *aProfileP,
    TBinfileClientConfig *aBinfileClientConfigP
  );
  virtual ~TBinfileProfileKey();
  TBinfileClientConfig *getBinfileClientConfig(void) { return fBinfileClientConfigP; };
  TBinfileDBSyncProfile *getProfile(void) { return fProfileP; };
  sInt32 getProfileID() { return fProfileP ? fProfileP->profileID : KEYVAL_ID_UNKNOWN; };
protected:
  // return ID of this key
  virtual TSyError GetKeyID(sInt32 &aID);

  // open subkey by name (not by path!)
  // - this is the actual implementation
  virtual TSyError OpenSubKeyByName(
    TSettingsKeyImpl *&aSettingsKeyP,
    cAppCharP aName, stringSize aNameSize,
    uInt16 aMode
  );
  // get table describing the fields in the struct
  virtual const TStructFieldInfo *getFieldsTable(void);
  virtual sInt32 numFields(void);
  // get actual struct base address
  virtual uInt8P getStructAddr(void);

private:
  // profile
  sInt32 fProfileIndex; // index for writing back profile
  TBinfileDBSyncProfile *fProfileP;
  TBinfileClientConfig *fBinfileClientConfigP;
}; // TBinfileProfileKey



// Binfile based profiles collection key
class TBinfileProfilesKey :
  public TStructFieldsKey
{
  typedef TStructFieldsKey inherited;

public:
  TBinfileProfilesKey(TEngineInterface *aEngineInterfaceP);
  virtual ~TBinfileProfilesKey();
  TBinfileClientConfig *getBinfileClientConfig(void) { return fBinfileClientConfigP; };
  // set iterator
  void setNextProfileindex(sInt32 aProfileIndex) { fProfileIterator=aProfileIndex-1; };

protected:
  // profiles can be opened only by ID
  virtual TSyError OpenSubkey(
    TSettingsKeyImpl *&aSettingsKeyP,
    sInt32 aID, uInt16 aMode
  );
  virtual TSyError DeleteSubkey(sInt32 aID);

  // get table describing the fields in the struct
  virtual const TStructFieldInfo *getFieldsTable(void);
  virtual sInt32 numFields(void);
  // get actual struct base address
  virtual uInt8P getStructAddr(void);
private:
  // internal
  sInt32 fProfileIterator;
public:
  // binfileconfig
  TBinfileClientConfig *fBinfileClientConfigP;
  // flag how to call loadVarConfig
  bool fMayLooseOldCfg;
}; // TBinfileProfilesKey



// Binfile based log entry key
class TBinfileLogKey :
  public TStructFieldsKey
{
  typedef TStructFieldsKey inherited;

public:
  TBinfileLogKey(
    TEngineInterface *aEngineInterfaceP,
    TLogFileEntry *aLogEntryP,
    TBinfileClientConfig *aBinfileClientConfigP
  );
  virtual ~TBinfileLogKey();
  TBinfileClientConfig *getBinfileClientConfig(void) { return fBinfileClientConfigP; };
  TLogFileEntry *getLogEntry(void) { return fLogEntryP; };
protected:
  // get table describing the fields in the struct
  virtual const TStructFieldInfo *getFieldsTable(void);
  virtual sInt32 numFields(void);
  // get actual struct base address
  virtual uInt8P getStructAddr(void);

private:
  TLogFileEntry *fLogEntryP;
  TBinfileClientConfig *fBinfileClientConfigP;
}; // TBinfileLogKey


// Binfile based log entry collection key
class TBinfileLogsKey :
  public TSettingsKeyImpl
{
  typedef TSettingsKeyImpl inherited;

public:
  TBinfileLogsKey(TEngineInterface *aEngineInterfaceP);

protected:
  // targets can be opened only by dbtype-ID
  virtual TSyError OpenSubkey(
    TSettingsKeyImpl *&aSettingsKeyP,
    sInt32 aID, uInt16 aMode
  );
  virtual TSyError DeleteSubkey(sInt32 aID);
private:
  // binfileconfig
  TBinfileClientConfig *fBinfileClientConfigP;
  // iterator
  sInt32 fLogEntryIterator;
  // the log file
  TBinFile fLogFile;
}; // TBinfileLogsKey



// Binfile based client settings rootkey
class TBinfileAgentRootKey :
  public TSettingsRootKey
{
  typedef TSettingsRootKey inherited;

public:
  TBinfileAgentRootKey(TEngineInterface *aEngineInterfaceP);

protected:
  // open subkey by name (not by path!)
  virtual TSyError OpenSubKeyByName(
    TSettingsKeyImpl *&aSettingsKeyP,
    cAppCharP aName, stringSize aNameSize,
    uInt16 aMode
  );
}; // TBinfileAgentRootKey



#endif // ENGINEINTERFACE_SUPPORT


// Config
// ======

class TBinfileClientConfig:
  public TAgentConfig
{
  typedef TAgentConfig inherited;
public:
  TBinfileClientConfig(TConfigElement *aParentElement);
  virtual ~TBinfileClientConfig();
  // API for settings management
  // - initialize a new-created profile database
  void initProfileDb(void);
  // - create a new profile including appropriate targets
  sInt32 newProfile(const char *aProfileName, bool aSetDefaults, sInt32 aTemplateProfile=-1);
  // - init profile record with defaults
  static void initProfile(TBinfileDBSyncProfile &aProfile, const char *aName, bool aWithDefaults);
  // - write profile, returns index of profile
  sInt32 writeProfile(
    sInt32 aProfileIndex, // -1 if adding new profile
    TBinfileDBSyncProfile &aProfile // profile ID is set to new ID in aProfile
  );
  // - delete profile (and all of its targets)
  bool deleteProfile(
    sInt32 aProfileIndex
  );
  // - get number of existing profiles
  sInt32 numProfiles(void);
  // - get profile, returns index or -1 if no more profiles
  sInt32 getProfile(
    sInt32 aProfileIndex,
    TBinfileDBSyncProfile &aProfile
  );
  // - get profile index from name, returns index or -1 if no matching profile found
  sInt32 getProfileIndexByName(cAppCharP aProfileName);
  // - get profile by ID
  sInt32 getProfileByID(
    uInt32 aProfileID,
    TBinfileDBSyncProfile &aProfile
  );
  sInt32 getProfileIndex(uInt32 aProfileID);
  // - checks that all profiles are complete with targets for all configured datastores
  //   (in case of STD->PRO upgrade for example)
  void checkProfiles(void);
  // - checks that profile is complete with targets for all configured datastores
  //   (in case of STD->PRO upgrade for example)
  void checkProfile(sInt32 aProfileIndex);
  // - get profile ID by index, 0 if none found
  uInt32 getIDOfProfile(sInt32 aProfileIndex);
  // - check for feature enabled (profile or license dependent)
  bool isFeatureEnabled(TBinfileDBSyncProfile *aProfileP, uInt16 aFeatureNo); // with profile already loaded
  bool isFeatureEnabled(sInt32 aProfileIndex, uInt16 aFeatureNo); // by profile index
  // - check for readonly parts of profile settings
  bool isReadOnly(TBinfileDBSyncProfile *aProfileP, uInt8 aReadOnlyMask);
  bool isReadOnly(sInt32 aProfileIndex, uInt8 aReadOnlyMask);
  // - get last sync (earliest of lastSync of all sync-enabled targets), 0=never. Returns false if no enabled targets
  bool getProfileLastSyncTime(uInt32 aProfileID, lineartime_t &aLastSync, bool &aZapsServer, bool &aZapsClient);
  // - get last sync of target, 0=never. Returns false if target not enabled
  bool getTargetLastSyncTime(sInt32 aTargetIndex, lineartime_t &aLastSync, bool &aZapsServer, bool &aZapsClient, uInt32 &aDBID);
  bool getTargetLastSyncTime(TBinfileDBSyncTarget &aTarget, lineartime_t &aLastSync, bool &aZapsServer, bool &aZapsClient, uInt32 &aDBID);
  // - check if datastore of specified target is available in the given profile
  bool isTargetAvailable(TBinfileDBSyncProfile *aProfileP, uInt32 aLocalDBTypeID);
  // - find available target for profile by DB ID/name. Returns target index or -1 if
  //   DBTypeID not available in this profile/license or not implemented at all
  sInt32 findAvailableTargetIndexByDBInfo(
    TBinfileDBSyncProfile *aProfileP, // profile to search targets for
    uInt32 aLocalDBTypeID,
    const char *aLocalDBName // can be NULL if name does not matter
  );
  // - find or create target for profile by DB ID/name. creates target if not found existing already.
  sInt32 findOrCreateTargetIndexByDBInfo(
    uInt32 aProfileID, // profile to search targets for
    uInt32 aLocalDBTypeID,
    const char *aLocalDBName // can be NULL if name does not matter
  );
  // - find target for profile by DB ID/name. Returns target index or -1 if none found
  sInt32 findTargetIndexByDBInfo(
    uInt32 aProfileID, // profile to search targets for
    uInt32 aLocalDBTypeID,
    const char *aLocalDBName
  );
  // - find target for profile. Returns target index or -1 if none found
  sInt32 findTargetIndex(
    uInt32 aProfileID, // profile to search targets for
    sInt32 aTargetSeqNum // sequence number (0..n)
  );
  // - write target, returns index of target
  sInt32 writeTarget(
    sInt32 aTargetIndex,  // -1 if adding new target
    const TBinfileDBSyncTarget &aTarget
  );
  // - delete target
  bool deleteTarget(
    sInt32 aTargetIndex
  );
  // - get target info
  sInt32 getTarget(
    sInt32 aTargetIndex,
    TBinfileDBSyncTarget &aTarget
  );
  // - get path where to store binfiles
  void getBinFilesPath(string &aPath);
  // activtion switch (for making it inactive e.g. in server case)
  bool fBinfilesActive;
  // separate changelogs per profile
  bool fSeparateChangelogs;
  #ifndef HARDCODED_CONFIG
  // - configurable path where to store binfiles
  string fBinFilesPath;
  #endif
  // - if set, sync log statistics are saved to binfile
  bool fBinFileLog;
  // Binary Files
  TBinFile fProfileBinFile;
  TBinFile fTargetsBinFile;
  // -  cleanup changelog, pendingmap, pendingitem
  string relatedDBNameBase(cAppCharP aDBName, sInt32 aProfileID);
  void cleanChangeLogForTarget(sInt32 aTargetIndex, sInt32 aProfileID);
  void cleanChangeLogForDBname(cAppCharP aDBName, sInt32 aProfileID);
  void separateChangeLogsAndRelated(cAppCharP aDBName);
  // - called when app should save its persistent state
  virtual void saveAppState(void);
  // - MUST be called after creating config to load (or pre-load) variable parts of config
  //   such as binfile profiles. If aDoLoose==false, situations, where existing config
  //   is detected but cannot be re-used will return an error. With aDoLoose==true, config
  //   files etc. are created even if it means a loss of data.
  virtual localstatus loadVarConfig(bool aDoLoose=false);
  // open and close the settings databases
  localstatus openSettingsDatabases(bool aDoLoose);
  void closeSettingsDatabases(void);
private:
  // helper for migrating to separated changelogs/pendingmaps/pendingitem
  void separateDBFile(cAppCharP aDBName, cAppCharP aDBSuffix, sInt32 aProfileID);
protected:
  // check config elements
  #ifndef HARDCODED_CONFIG
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  virtual void clear();
  virtual void localResolve(bool aLastPass);
  // Client provisioning declarations
  CLIENTPROVISIONING_CLASSDECL
  // Autosync declarations
  CLIENTAUTOSYNC_CLASSDECL
}; // TBinfileClientConfig


// Binary file based session
class TBinfileImplClient: public TStdLogicAgent
{
  typedef TStdLogicAgent inherited;
  #ifdef ENGINEINTERFACE_SUPPORT
  friend class TBinFileAgentParamsKey;
  #endif
public:
  TBinfileImplClient(TSyncAppBase *aSyncAppBaseP, TSyncSessionHandle *aSyncSessionHandleP, cAppCharP aSessionID);
  /// check if active derived classes (in particular: customImplDS that CAN derive binfiles, but does not necessarily so)
  bool binfilesActive(void) { return fConfigP && fConfigP->fBinfilesActive; };
  // - selects a profile (returns false if profile not found)
  //   Note: This call must create and initialize all datastores that
  //         are to be synced with that profile.
  virtual localstatus SelectProfile(uInt32 aProfileSelector, bool aAutoSyncSession=false);
  virtual ~TBinfileImplClient();
  virtual void TerminateSession(void); // Terminate session, like destructor, but without actually destructing object itself
  virtual void ResetSession(void); // Resets session (but unlike TerminateSession, session might be re-used)
  void InternalResetSession(void); // static implementation for calling through virtual destructor and virtual ResetSession();
  #ifdef ENGINEINTERFACE_SUPPORT
  // set profileID to client session before doing first SessionStep
  virtual void SetProfileSelector(uInt32 aProfileSelector);
  /// @brief Get new session key to access details of this session
  virtual appPointer newSessionKey(TEngineInterface *aEngineInterfaceP);
  #endif // ENGINEINTERFACE_SUPPORT
  // - load remote connect params (syncml version, type, format and last nonce)
  //   Note: agents that can cache this information between sessions will load
  //   last info here.
  virtual void loadRemoteParams(void);
  // - save remote connect params for use in next session (if descendant implements it)
  virtual void saveRemoteParams(void);
  // special behaviour
  #ifndef GUARANTEED_UNIQUE_DEVICID
  virtual bool devidWithUserHash(void) { return (fRemoteFlags & remotespecs_devidWithUserHash)!=0; }; // include user name to make a hash-based pseudo-device ID when flag is set
  #endif
  // - handle custom put and result commands (for remote provisioning and IPP)
  virtual void processPutResultItem(bool aIsPut, const char *aLocUri, TSmlCommand *aPutResultsCommandP, SmlItemPtr_t aPutResultsItemP, TStatusCommand &aStatusCommand);
  #ifdef IPP_SUPPORT
  // - called to issue custom get and put commands
  virtual void issueCustomGetPut(bool aGotDevInf, bool aSentDevInf);
  #endif
  // binfile agent config
  TBinfileClientConfig *fConfigP;
  // unique ID to identify info record of remote party (profile for client, deviceentry for server)
  uInt32 fRemotepartyID;
  // - check remote devinf to detect special behaviour needed for some servers. Base class
  //   does not do anything on server level (configured rules are handled at session level)
  virtual localstatus checkRemoteSpecifics(SmlDevInfDevInfPtr_t aDevInfP, SmlDevInfDevInfPtr_t *aOverrideDevInfP);
  TBinfileDBSyncProfile fProfile;
  // - remote specific client behaviour flags
  uInt8 fRemoteFlags; // flags for remote specific behaviour (remotespecs_XXX)
private:
  // selected profile
  sInt32 fProfileIndex;
  bool fProfileDirty;
}; // TBinfileImplClient


#ifdef ENGINEINTERFACE_SUPPORT

// Support for EngineModule common interface
// =========================================

// client runtime parameters
class TBinFileAgentParamsKey :
  public TAgentParamsKey
{
  typedef TAgentParamsKey inherited;

public:
  TBinFileAgentParamsKey(TEngineInterface *aEngineInterfaceP, TSyncAgent *aClientSessionP);
  virtual ~TBinFileAgentParamsKey() {};

protected:
  // open subkey by name (not by path!)
  virtual TSyError OpenSubKeyByName(
    TSettingsKeyImpl *&aSettingsKeyP,
    cAppCharP aName, stringSize aNameSize,
    uInt16 aMode
  );
}; // TBinFileAgentParamsKey


#endif // ENGINEINTERFACE_SUPPORT

} // namespace sysync

#endif  // BINFILEIMPLCLIENT_H

// eof
