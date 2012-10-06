/**
 *  @File     binfileimplds.h
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


#ifndef BINFILEIMPLDS_H
#define BINFILEIMPLDS_H

// includes
#include "binfile.h" // platform specific header (using binfilebase)

#include "stdlogicds.h"
#include "multifielditem.h"

// defines that allow using the generic method implementations
// for provisioning and autosync
#define CLIENTAGENTCONFIG TBinfileClientConfig
#define CLIENTDSCONFIG TBinfileDSConfig
#define PROFILERECORD TBinfileDBSyncProfile
#define TARGETRECORD TBinfileDBSyncTarget

#include "clientprovisioning_inc.h"
#include "clientautosync_inc.h"


namespace sysync {

// Defines
// =======

#ifndef ANDROID
#pragma pack(push,4) // 32bit
#endif

// specific sync
const uInt16 dbnamelen=32;

// Local IDs
// =========

// local ID typedefs and Macros
#ifdef NUMERIC_LOCALIDS
  // numeric local IDs
  typedef NUMERIC_LOCALIDS localid_t;
  typedef localid_t localid_out_t;
  #define LOCALID_EQUAL(a,b) (a==b)
  #if NUMERIC_LOCALID_SZ==2
    #define STR_TO_LOCALID(s,i) StrToUShort(s,i)
  #elif NUMERIC_LOCALID_SZ==4
    #define STR_TO_LOCALID(s,i) StrToULong(s,i)
  #elif NUMERIC_LOCALID_SZ==8
    #define STR_TO_LOCALID(s,i) StrToULongLong(s,i)
  #else
    #error "unknown size of numeric local ID"
  #endif
  #define ASSIGN_LOCALID_TO_FLD(a,b) (a=b)
  #define LOCALID_OUT_TO_IN(out) out
  #if NUMERIC_LOCALID_SZ<=4
    #define LOCALID_TO_STRING(i,s) { StringObjPrintf(s,"%lu",(uInt32)i); }
  #else
    #define LOCALID_TO_STRING(i,s) { StringObjPrintf(s,"%llu",(uInt64)i); }
  #endif
  #define ASSIGN_LOCALID_TO_ITEM(it,i) { string s; LOCALID_TO_STRING(i,s); (it).setLocalID(s.c_str()); }
#else
  // string local IDs
  const uInt16 maxidlen = STRING_LOCALID_MAXLEN;
  typedef char *localid_t;
  typedef string localid_out_t;
  #define LOCALID_EQUAL(a,b) (strnncmp(a,b,maxidlen)==0)
  #define STR_TO_LOCALID(s,i) (i=(char *)s)
  #define LOCALID_TO_STRING(i,s) s=i
  #define ASSIGN_LOCALID_TO_FLD(a,b) AssignCString(a,b,maxidlen)
  #define LOCALID_OUT_TO_IN(out) ((char *)out.c_str())
  #define ASSIGN_LOCALID_TO_ITEM(it,i) (it).setLocalID(i)
#endif


// User log
// ========

// Note: Engine 3.1.5.1 and later have the fBinFileLog which can be turned on to have binfileds
//       write the log records automatically. With ENGINEINTERFACE_SUPPORT, the /synclogs
//       settingskey can be used to access the log records.
//       In 3.0 clients, implementation of filling and displaying the log binfile is platform dependent
//       simply using this typedef to have a common format.

#ifdef SETTINGS_BFI_PREFIX
  // custom settings name prefix
  #define LOGFILE_DB_NAME SETTINGS_BFI_PREFIX "log.bfi"
#else
  #define LOGFILE_DB_NAME "sysylog.bfi"
#endif
#define LOGFILE_DB_ID 8
#define LOGFILE_DB_VERSION 3

typedef struct {
  lineartime_t time; ///< timestamp for this log entry
  uInt32 dbID; ///< the datastore involved
  uInt32 profileID; ///< id of the profile
  localstatus status;
  uInt16 mode; ///< sync mode: 0=normal, 1=slow, 2=first time, +10 = resumed
  sInt32 infoID; ///< ressource id of text describing error (only valid with platform-specific log file filling, not available via /synclogs)
  uInt32 locAdded;
  uInt32 locUpdated;
  uInt32 locDeleted;
  uInt32 remAdded;
  uInt32 remUpdated;
  uInt32 remDeleted;
  uInt32 inBytes;
  uInt32 outBytes;
  // V2 fields start here
  uInt32 locRejected;
  uInt32 remRejected;
  // V3 fields start here
  uInt16 dirmode; /// <sync direction: 0=twoway, 1=from server, 2=from client (According to TSyncModes)
} TLogFileEntry;



// Changelog
// =========
#ifdef CHECKSUM_CHANGELOG

// Changelog file name
#ifdef SETTINGS_BFI_PREFIX
  // custom settings name prefix
  #define CHANGELOG_DB_SUFFIX "_clg_" SETTINGS_BFI_PREFIX ".bfi"
#else
  #define CHANGELOG_DB_SUFFIX "_clg.bfi"
#endif
#define CHANGELOG_DB_ID 4
#define LOWEST_CHANGELOG_DB_VERSION 2 // note: step from V2 to V3 was only change in header
#define CHANGELOG_DB_VERSION 5 // dataCRC added as new field (was present for non-changedetection targets before)


const uInt16 changeIndentifierMaxLen=128;

// changelog flag bits
const uInt8 chgl_deleted=0x80;
const uInt8 chgl_delete_candidate=0x40;
const uInt8 chgl_receive_only=0x20;
const uInt8 chgl_markedforresume=0x10;
const uInt8 chgl_resend=0x08;
const uInt8 chgl_modbysync=0x04;
//%%%never used, NOW CONFLICTS WITH chgl_resend and chgl_noresume: const uInt8 chgl_category_mask=0x0F;

// single changelog entry

typedef struct {
  // UniqueID of the database record
  #ifdef NUMERIC_LOCALIDS
  localid_t dbrecordid;
  #else
  char dbrecordid[maxidlen];
  #endif
  // modification count of last modification
  uInt32 modcount;
  /* Note: only DB versions V2 and earlier could possibly have this field. V3 and V4 never had it
     as they were never used with CHANGEDETECTION_AVAILABLE not defined. V5 adds it unconditionally.
  #ifndef CHANGEDETECTION_AVAILABLE
  // CRC of the record's data after the last modification
  uInt16 dataCRC;
  #endif
  */
  // flags
  uInt8 flags;
  uInt8 dummy;

  // Version 3 has no new fields in records, only in header

  // Version 4 fields start here
  // ===========================
  // - creation timestamp for the record
  uInt32 modcount_created;

  // Version 5 fields start here
  // ===========================
  // CRC of the record's data after the last modification
  uInt16 dataCRC;

} TChangeLogEntry;


// changelog header for one database
typedef struct {
  // modification count source (will be used to mark modified records)
  uInt32 modcount;
  // V3 additions
  // - global change tracking anchors
  char lastChangeCheckIdentifier[changeIndentifierMaxLen];  // string field to save identifier for last changelog update
  lineartime_t lastChangeCheck; // reference time for last changelog update
} TChangeLogHeader;

#endif

// Pending Maps database
// =====================

// Pending Maps file name
#ifdef SETTINGS_BFI_PREFIX
  // custom settings name prefix
  #define PENDINGMAP_DB_SUFFIX "_pmap_" SETTINGS_BFI_PREFIX ".bfi"
#else
  #define PENDINGMAP_DB_SUFFIX "_pmap.bfi"
#endif
#define PENDINGMAP_DB_ID 7
#define PENDINGMAP_DB_VERSION 1


// define a max GUID size
#define BINFILE_MAXGUIDSIZE 63

// Pending Map entry
typedef struct {
  // UniqueID of the database record
  #ifdef NUMERIC_LOCALIDS
  localid_t dbrecordid;
  #else
  char dbrecordid[maxidlen];
  #endif
  // RemoteID
  char remoteID[BINFILE_MAXGUIDSIZE+1];
} TPendingMapEntry;

// pendingmap header for one database
typedef struct {
  // profile ID where these maps belong to
  uInt32 remotepartyID;
} TPendingMapHeader;


// Pending Item database
#ifdef SETTINGS_BFI_PREFIX
  // custom settings name prefix
  #define PENDINGITEM_DB_SUFFIX "_pitm_" SETTINGS_BFI_PREFIX ".bfi"
#else
  #define PENDINGITEM_DB_SUFFIX "_pitm.bfi"
#endif
#define PENDINGITEM_DB_ID 8
#define PENDINGITEM_DB_VERSION 1


// pendingitem header for one database
typedef struct {
  // profile ID where these maps belong to
  uInt32 remotepartyID;
  // pending item info
  TSyError lastItemStatus;
  char lastSourceURI[BINFILE_MAXGUIDSIZE+1];
  char lastTargetURI[BINFILE_MAXGUIDSIZE+1];
  TPartialItemState piState;
  uInt32 totalSize;
  uInt32 unconfirmedSize;
  uInt32 storedSize; // = size of one and only record in this binfile
} TPendingItemHeader;



// Sync Target database
// ====================

#if defined(CLIENTFEATURES_2008)
  // new feature clients have version 6
  #define TARGETS_DB_VERSION 6
  #define LOWEST_TARGETS_DB_VERSION 3 // upgrading from V3 up is possible
#elif defined(DESKTOP_CLIENT)
  // desktop clients started with version 5
  #define TARGETS_DB_VERSION 5
  #define LOWEST_TARGETS_DB_VERSION 5 // we started anew at Version 5 for outlook client
#else
  // 2007/2008 production clients are still version 4
  #define TARGETS_DB_VERSION 4
  #define LOWEST_TARGETS_DB_VERSION 3  // upgrading from V3 up is possible
#endif
#define TARGETS_DB_ID 1
#ifdef SETTINGS_BFI_PREFIX
  // custom settings name prefix
  #define TARGETS_DB_NAME SETTINGS_BFI_PREFIX "targ.bfi"
#else
  #define TARGETS_DB_NAME "sysytarg.bfi"
#endif


const uInt16 dispNameMaxLen=64;
const uInt16 filterCapDescMaxLen=512;
const uInt16 filterExprMaxLen=128;

const uInt16 remoteDBpathMaxLen=128;
#ifdef DESKTOP_CLIENT
const uInt16 localDBpathMaxLen=128;
#else
const uInt16 localDBpathMaxLen=32;
#endif
const uInt16 remoteAnchorMaxLen=64;
typedef struct {
  // parent remotepartyID
  uInt32 remotepartyID;
  // db name (selects datastore from config and changelog file name)
  char dbname[dbnamelen];
  // user options
  bool enabled; // enabled for sync?
  bool forceSlowSync; // if set, a slow sync is forced, reset after successful sync
  TSyncModes syncmode; // mode of sync
  // user extras
  sInt32 limit1; // in email: max incoming message body size
  sInt32 limit2; // in email: max incoming attachment size
  uInt32 extras; // reserved for future use
  // SyncML target info
  // - local database app involved
  uInt32 localDBTypeID;
  //   - database name / entryID
  char localDBPath[localDBpathMaxLen];
  // %%% add category sub-selector later
  // - remote database path
  char remoteDBpath[remoteDBpathMaxLen];
  // - time of last sync
  lineartime_t lastSync;
  //%%%lineartime_t lastTwoWaySync;
  lineartime_t lastChangeCheck; // (formerly lastTwoWaySync) reference time for last changelog update
  // - modification count of last sync and suspend (refers to changelog)
  uInt32 lastSuspendModCount; /// @note: before DS 1.2 enginem this was used as "lastModCount"
  uInt32 lastTwoWayModCount;
  // - last remote (server) anchor
  char remoteAnchor[remoteAnchorMaxLen];

  // Version 4 fields start here
  // ===========================
  // - resume alert code if last session was suspended
  uInt16 resumeAlertCode;
  // - spare
  uInt16 dummy1;

  // Version 5 fields start here (%%%currently desktop only, mobile is still version 4)
  // ===========================
  #if defined(DESKTOP_CLIENT) || TARGETS_DB_VERSION>4 // unite desktop and mobile client
  //   - container name / entryID
  char localContainerName[localDBpathMaxLen];
  #endif
  #if !defined(DESKTOP_CLIENT) && TARGETS_DB_VERSION>4
    #error "Make sure to unite desktop and mobile versions - desktop already is version 5. Note that localDBpathMaxLen is shorter in mobile; check if 128 is really needed for desktop!"
  #endif

  // Version 6 fields start here
  // ===========================
  #if TARGETS_DB_VERSION>5
  // storage for non-time sync identifiers in case DB on top is customImplDS
  char dummyIdentifier1[remoteAnchorMaxLen];  // (formerly lastSyncIdentifier) not used any more - single identifier in changelog is sufficient
  char dummyIdentifier2[remoteAnchorMaxLen];  // (formerly lastSuspendIdentifier) not used any more - single identifier in changelog is sufficient
  // remote datastore info (retrieved from devInf, if possible)
  char remoteDBdispName[dispNameMaxLen]; // display name of remote datastore
  char filterCapDesc[filterCapDescMaxLen]; // description string of remote filter capabilities
  // remote filter expression (in internal format)
  char remoteFilters[filterExprMaxLen];
  // local (client side) filter expression (in internal format)
  char localFilters[filterExprMaxLen];

  #endif // TARGETS_DB_VERSION > 5

  // Version 7 fields %%%will%%% start here
  // ===========================
  #if TARGETS_DB_VERSION>6
  #error "target versions > 6 not yet supported! - please check TARGETS_DB_VERSION"
  #endif // TARGETS_DB_VERSION > 6


} TBinfileDBSyncTarget;

#define TARGETS_DB_VERSION_4_SZ sizeof(TBinfileDBSyncTarget)
#define TARGETS_DB_VERSION_3_SZ offsetof(TBinfileDBSyncTarget,resumeAlertCode)


// Sync Profile database
// =====================

#ifdef SETTINGS_BFI_PREFIX
  // custom settings name prefix
  #define PROFILE_DB_NAME SETTINGS_BFI_PREFIX "prof.bfi"
#else
  #define PROFILE_DB_NAME "sysyprof.bfi"
#endif

#ifndef ENHANCED_PROFILES_2004
  #error "non-enhanced profiles and profile version <6 no longer supported!"
#endif

#define PROFILE_DB_ID 2
#define PROFILE_DB_VERSION 9
// lowest profile DB version still supported for automatic upgrading
#define LOWEST_PROFILE_DB_VERSION 4


// read-only flags
const uInt8 rdonly_URI = 0x01;  // server URI
const uInt8 rdonly_URIpath = 0x02; // URI path suffix
const uInt8 rdonly_protocol = 0x04; // protocol (transport http/https flag as well as SyncML version)
const uInt8 rdonly_dbpath = 0x08; // DB path names
const uInt8 rdonly_profile = 0x10; // profile may not be deleted or renamed (but non-readonly options can be set)
const uInt8 rdonly_proxy = 0x20; // proxy settings (except proxy auth settings).
// - Bit 6+7 reserved

// feature flags
const uInt8 ftrflg_autosync = 0x01;  // autosync enabled
const uInt8 ftrflg_dmu = 0x02;  // DMU enabled
const uInt8 ftrflg_range = 0x04; // Range filtering enabled (APP_FTR_EVENTRANGE etc...)

// datastore availability flags
const uInt16 dsavail_all = 0xFFFF; // all implemented datastores
const uInt16 dsavail_contacts = 0x0001; // contacts
const uInt16 dsavail_events = 0x0002; // events
const uInt16 dsavail_tasks = 0x0004; // tasks
const uInt16 dsavail_memos = 0x0008; // notes/memos
const uInt16 dsavail_emails = 0x0010; // emails

// remote-specifics flags
const uInt8 remotespecs_devidWithUserHash = 0x01; // device ID must always include the user name
const uInt8 remotespecs_noXTypeParams = 0x02; // vCard should not contain any X-Synthesis-RefX and other X-hhh type tags
const uInt8 remotespecs_noDS12Filters = 0x04; // we should not use DS 1.2 SINCE/BEFORE filters
// - Bit 3..7 reserved



// field sizes
const uInt16 maxurisiz=128;
const uInt16 maxupwsiz=64;
const uInt16 maxnamesiz=42;
const uInt16 maxnoncesiz=64;
const uInt16 maxpathsiz=64;
const uInt16 profiledatasiz=256;

typedef enum {
  transp_proto_uri, // protocol included in URI
  transp_proto_http, // HTTP protocol
  transp_proto_https, // HTTPS protocol
  transp_proto_wsp, // WSP protocol
  transp_proto_obex_irda, // OBEX/IRDA protocol
  transp_proto_obex_bt, // OBEX/BT protocol
  transp_proto_obex_tcp, // OBEX/TCP protocol
  num_transp_protos // number of protocols
} TTransportProtocols;

// NOTE!!!: changing number of levels implies a change of PROFILE_DB_VERSION!!
#define NUM_AUTOSYNC_LEVELS 3 // we provide 3 levels of autosync

typedef struct {
  // Unique ID
  uInt32 profileID; // internal unique ID of the profile (selects targets by their remotepartyID)
  // PROFILE_DB_VERSION <=4 settings
  char profileName[maxnamesiz];
  // - encoding (note that release client might not allow XML)
  SmlEncoding_t encoding;
  // - Old-style Server URI, misused for several other things
  //   Note: for hard-coded server URI, this is also misused as companyID or secure flag:
  //   - ONE_e2g: serverURI = companyID
  //   - SCM: serverURI[0] = secure flag (HTTPS)
  char serverURI[maxurisiz];
  // - User name
  char serverUser[maxupwsiz];
  // - password
  char serverPassword[maxupwsiz];
  // - Transport user
  char transportUser[maxupwsiz];
  // - Transport password
  char transportPassword[maxupwsiz];
  // - Socks Host
  char socksHost[maxurisiz];
  // - Proxy Host
  char proxyHost[maxurisiz];
  // Internals
  // - session ID
  uInt8 sessionID;
  // - last connection's parameters (used as default for next session)
  TSyncMLVersions lastSyncMLVersion;
  TAuthTypes lastAuthMethod;
  TFmtTypes lastAuthFormat;
  char lastNonce[maxnoncesiz];
  // - for registration checking
  lineardate_t firstuse; // copy of a global setting, so we can compare and see if someone has tampered with

  // Additional PROFILE_DB_VERSION 5 settings
  // - Enhanced server addressing
  //   - possibly some path extension
  char URIpath[maxpathsiz];
  //   - protocol selector (0=free-form from URI, 1..n = reserved for fixed protocols)
  TTransportProtocols protocol;
  //   - read-only flags (rdonly_xxx)
  uInt8 readOnlyFlags;
  //   - flags for special remote-specific behaviour (remotespecs_XXX) - was reserved until 2.5.0.68 / 2.9.8.10.
  uInt8 remoteFlags;
  //   - profile feature flags (ftrflg_xxx)
  uInt8 featureFlags;
  // - Local Database profile name
  char localDBProfileName[localDBpathMaxLen];
  // - Proxy usage options
  bool useProxy; // use configured proxies
  bool useConnectionProxy; // using proxy from current connection, if available
  // - Auto-Sync levels (3 levels, first match overrides lower level settings)
  TAutoSyncLevel AutoSyncLevel[NUM_AUTOSYNC_LEVELS];
  // - Datastore available flags (for clients with variable datastores)
  uInt16 dsAvailFlags; // dsavail_xxx flags
  //   - Timed sync settings
  uInt16 TimedSyncMobilePeriod; // [min] how often to sync when mobile (battery, wireless)
  uInt16 TimedSyncCradledPeriod; // [min] how often to sync when cradled (ac powered, wired)
  //   - IPP settings (not available in all clients, but present in all profile records
  TIPPSettings ippSettings;

  // Additional PROFILE_DB_VERSION 8 settings
  // - Proxy user
  char proxyUser[maxupwsiz];
  // - Proxy password
  char proxyPassword[maxupwsiz];

  // Additional PROFILE_DB_VERSION 9 settings
  // - transport related profile flags
  uInt32 transpFlags;
  // - general profile flags
  uInt32 profileFlags;
  // - general profile ints
  uInt32 profileExtra1;
  uInt32 profileExtra2;
  // - general purpose profile data (intended for SDK based app's usage)
  uInt8 profileData[profiledatasiz];

} TBinfileDBSyncProfile;

#define PROFILE_DB_VERSION_7_SZ offsetof(TBinfileDBSyncProfile,proxyUser)
#define PROFILE_DB_VERSION_8_SZ offsetof(TBinfileDBSyncProfile,transpFlags)





#if LOWEST_PROFILE_DB_VERSION<6
// old structs and consts used to access fields of profiles older than PROFILE_DB_VERSION 6

const uInt16 o_maxurisiz=128;
const uInt16 o_maxupwsiz=32;
const uInt16 o_maxnamesiz=42;
const uInt16 o_maxnoncesiz=64;
const uInt16 o_maxpathsiz=64;

typedef struct {
  char srv[maxipppathsiz];
  uInt16 port;
  uInt16 period;
  char path[maxipppathsiz];
  char id[maxippidsiz];
  uInt8 method;
} o_TIPPSettings;

typedef struct {
  // Unique ID
  uInt32 profileID; // internal unique ID of the profile (selects targets by their remotepartyID)
  // PROFILE_DB_VERSION <=4 settings
  char profileName[o_maxnamesiz];
  // - encoding (note that release client might not allow XML)
  SmlEncoding_t encoding;
  // - Old-style Server URI, misused for several other things
  //   Note: for hard-coded server URI, this is also misused as companyID or secure flag:
  //   - ONE_e2g: serverURI = companyID
  //   - SCM: serverURI[0] = secure flag (HTTPS)
  char serverURI[o_maxurisiz];
  // - User name
  char serverUser[o_maxupwsiz];
  // - password
  char serverPassword[o_maxupwsiz];
  // - Transport user
  char transportUser[o_maxupwsiz];
  // - Transport password
  char transportPassword[o_maxupwsiz];
  // - Socks Host
  char socksHost[o_maxurisiz];
  // - Proxy Host
  char proxyHost[o_maxurisiz];
  // Internals
  // - session ID
  uInt8 sessionID;
  // - last connection's parameters (used as default for next session)
  TSyncMLVersions lastSyncMLVersion;
  TAuthTypes lastAuthMethod;
  TFmtTypes lastAuthFormat;
  char lastNonce[o_maxnoncesiz];
  // - for registration checking
  lineardate_t firstuse; // copy of a global setting, so we can compare and see if someone has tampered with
  // Additional PROFILE_DB_VERSION 5 settings
  // - Enhanced server addressing
  //   - possibly some path extension
  char URIpath[o_maxpathsiz];
  //   - protocol selector (0=free-form from URI, 1..n = reserved for fixed protocols)
  TTransportProtocols protocol;
  //   - read-only flags (Bit 0=URI, 1=URIpath, 2=protocol, 3..7 reserved)
  uInt8 readOnlyFlags;
  //   - 2 bytes reserved for future use
  uInt16 reserved1;
  // - Local Database profile name
  char localDBProfileName[localDBpathMaxLen];
  // - Proxy usage options
  bool useProxy; // use configured proxies
  bool useConnectionProxy; // using proxy from current connection, if available
  // - Auto-Sync levels (3 levels, first match overrides lower level settings)
  TAutoSyncLevel AutoSyncLevel[NUM_AUTOSYNC_LEVELS];
  //   - reserved
  uInt8 reserved2; // reserved
  uInt8 reserved3; // reserved
  //   - Timed sync settings
  uInt16 TimedSyncMobilePeriod; // [min] how often to sync when mobile (battery, wireless)
  uInt16 TimedSyncCradledPeriod; // [min] how often to sync when cradled (ac powered, wired)
  //   - IPP settings (not available in all clients, but present in all profile records
  o_TIPPSettings ippSettings;
} o_TBinfileDBSyncProfile;

#endif


// versioned record sizes
#define PROFILE_DB_VERSION_5_SZ sizeof(o_TBinfileDBSyncProfile)
#define PROFILE_DB_VERSION_4_SZ offsetof(o_TBinfileDBSyncProfile,URIpath)


#ifndef ANDROID
#pragma pack(pop)
#endif

#ifndef CHANGEDETECTION_AVAILABLE
#define CRC_CHANGE_DETECTION true
#define CRC_DETECT_PSEUDOCHANGES false
#else
#define CRC_CHANGE_DETECTION (fConfigP->fCRCChangeDetection)
#define CRC_DETECT_PSEUDOCHANGES (fConfigP->fCRCPseudoChangeDetection)
#endif


// datastore config
// ================

#ifdef SCRIPT_SUPPORT
// publish func table for chaining
extern const TFuncTable BinfileClientDBFuncTable;
#endif // SCRIPT_SUPPORT

class TBinfileDSConfig: public TLocalDSConfig
{
  typedef TLocalDSConfig inherited;
public:
  TBinfileDSConfig(const char* aName, TConfigElement *aParentElement);
  virtual ~TBinfileDSConfig();
  // properties
  // - activtion switch (for making it inactive e.g. in server case)
  bool fBinfileDSActive;
  #ifdef CHANGEDETECTION_AVAILABLE
  // - we have change detection, but we can be set to enable change detection by CRC,
  //   or verifying DB reported changes by CRC to avoid pseudo-changes (only touched, but not modified records).
  bool fCRCChangeDetection;
  bool fCRCPseudoChangeDetection;
  #endif
  // - identifies local Database related to this datastore
  string fLocalDBPath;
  // - flag that corresponds with profile.dsAvailFlags / dsavail_xxx
  uInt16 fDSAvailFlag;
  // - if set, the compare reference time for changelog updates is set to end-of-session time
  //   Note: before 3.2.0.32, this was hardcoded via SYNCTIME_IS_ENDOFSESSION. Now SYNCTIME_IS_ENDOFSESSION
  //         only defines the default value for this setting
  bool fCmpRefTimeStampAtEnd;
  // public methods
  // - init a default target for this datastore
  virtual void initTarget(
    TBinfileDBSyncTarget &aTarget, // target record to initialize
    uInt32 aRemotepartyID,
    const char *aRemoteName=NULL, // defaults to name of datastore
    bool aEnabled=false // enabled?
  );
  // - checks if datastore is available for given profile. If aProfileP is NULL,
  //   general availability is checked (profile might allow more or less)
  bool isAvailable(TBinfileDBSyncProfile *aProfileP);
  // - one-way support is always given for binfile based DS
  virtual bool isOneWayFromRemoteSupported() { return true; }
  #ifdef SCRIPT_SUPPORT
  // function table
  virtual const TFuncTable *getClientDBFuncTable(void) { return &BinfileClientDBFuncTable; };
  #endif // SCRIPT_SUPPORT
protected:
  // check config elements
  #ifndef HARDCODED_CONFIG
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  virtual void clear();
  virtual void localResolve(bool aLastPass);
  // Autosync declarations
  CLIENTAUTOSYNC_CLASSDECL_DB
}; // TBinfileDSConfig



class TBinfileImplDS: public TStdLogicDS
{
  typedef TStdLogicDS inherited;
private:
  void InternalResetDataStore(void); // reset for re-use without re-creation
protected:
  /// @name dsSavedAdmin administrative data (anchors, timestamps, maps) as saved or to-be-saved
  /// @Note These will be loaded and saved be derived classes
  /// @Note Some of these will be updated from resp. @ref dsCurrentAdmin members at distinct events (suspend, session end, etc.)
  /// @Note Some of these will be updated during the session, but in a way that does NOT affect the anchoring of current/last session
  //
  /// @{
  /// Reference time of previous changelog update (preflight)
  /// @note the name of the variable indicates "previous to remote sync" time because in BASED_ON_BINFILE_CLIENT case where
  //        TCustomImplDS sits on top of binfile, it expects that instance var name (because in non binfile-based cases,
  //        that is in fact the "previous to remote sync" date. With the intermediate binfile changelog however, only
  //        changes since last preflight (changelog update) need to be reported.
  lineartime_t fPreviousToRemoteSyncCmpRef;
  /// Reference string used by database API level (in BASED_ON_BINFILE_CLIENT case) to detect changes
  string fPreviousToRemoteSyncIdentifier;
  /// modification count of last sync target's modifications
  uInt32 fPreviousToRemoteModCount;
  /// Reference time of last suspend, needed to detect modifications that took place between last suspend and current resume
  lineartime_t fPreviousSuspendCmpRef;
  /// Reference string used by database API level (in BASED_ON_BINFILE_CLIENT case) to detect changes after last suspend
  string fPreviousSuspendIdentifier;
  /// modification count used to determine modifications since last to-remote-sync
  uInt32 fPreviousSuspendModCount;
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
  /// modification count identifying this session's time (for detecting changes taking place after this session)
  uInt32 fCurrentModCount;
  /// Reference string returned by database API level identifying this session's time (for detecting changes taking place after this session)
  string fCurrentSyncIdentifier;
  /// @}

public:
  TBinfileImplDS(
    TBinfileDSConfig *aConfigP,
    sysync::TSyncSession *aSessionP,
    const char *aName,
    uInt32 aCommonSyncCapMask=0);
  virtual ~TBinfileImplDS();
  virtual void announceAgentDestruction(void) { /** @todo nop for now */ };
  virtual void dsResetDataStore(void);

  // - called from TBinFileDBAgent::SelectProfile(), should return false if local datastore is not accessible
  virtual bool localDatastorePrep(void) { return true; }
  // - set Sync Parameters. Derivate adds CGI to aRemoteDBPath
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


  // Target vars
  sInt32 fTargetIndex;
  TBinfileDBSyncTarget fTarget;
protected:
  /// called for SyncML 1.1 if remote wants number of changes.
  /// Must return -1 no NOC value can be returned
  virtual sInt32 getNumberOfChanges(void);
  /// checks change log for already known changes
  virtual bool hasPendingChangesForNextSync();

  // Simple custom DB access interface methods
  /// sync login (into this database)
  /// @note might be called several times (auth retries at beginning of session)
  /// @note must update the following state variables
  /// - in TLocalEngineDS: fLastRemoteAnchor, fLastLocalAnchor, fResumeAlertCode, fFirstTimeSync
  ///   - for client: fPendingAddMaps
  /// - in TStdLogicDS: fPreviousSyncTime, fCurrentSyncTime
  /// - in derived classes: whatever else belongs to dsSavedAdmin and dsCurrentAdmin state
  virtual localstatus implMakeAdminReady(
    const char *aDeviceID,    ///< remote device URI (device ID)
    const char *aDatabaseID,  ///< database ID
    const char *aRemoteDBID  ///< database ID of remote device
  );
  /// start data read
  /// @note: fSlowSync and fRefreshOnly must be valid before calling this method
  virtual localstatus implStartDataRead();
  /// get item from DB
  virtual localstatus implGetItem(
    bool &aEof,
    bool &aChanged,
    TSyncItem* &aSyncItemP
  );
  /// end of read
  virtual localstatus implEndDataRead(void);
  /// start of write
  virtual localstatus implStartDataWrite(void);
  /// review reported entry (allows post-processing such as map deleting)
  /// MUST be called after implStartDataWrite, before any actual writing,
  /// for each item obtained in implGetItem
  virtual localstatus implReviewReadItem(
    TSyncItem &aItem         // the item
  ) { return true; }; // nop here
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
  /// save end of session state
  virtual localstatus implSaveEndOfSession(bool aUpdateAnchors);
  /// end write sequence
  virtual bool implEndDataWrite(void);

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
  /// called to confirm a sync operation's completion (ok status from remote received)
  /// @note aSyncOp passed not necessarily reflects what was sent to remote, but what actually happened
  virtual void dsConfirmItemOp(TSyncOperation aSyncOp, cAppCharP aLocalID, cAppCharP aRemoteID, bool aSuccess, localstatus aErrorStatus=0);
  /// returns true if DB implementation supports resume (saving of resume marks, alert code, pending maps)
  virtual bool dsResumeSupportedInDB(void) { return true; }; // resume supported
  /// returns true if DB implementation supports resuming in midst of a chunked item (can save fPIxxx.. and related admin data)
  virtual bool dsResumeChunkedSupportedInDB(void) { return true; };
  /// saves user log information about the sync session
  virtual void dsLogSyncResult(void);

  /// @}

protected:
  // Routines to be implemented by derived classes to actually access
  // local databases
  #ifdef RECORDHASH_FROM_DBAPI
  /// get first item's ID and CRC from the sync set.
  /// @return false if no item found
  virtual bool getFirstItemCRC(localid_out_t &aLocalID, uInt16 &aItemCRC) = 0;
  /// get next item's ID and CRC from the sync set.
  /// @return false if no item found
  virtual bool getNextItemCRC(localid_out_t &aLocalID, uInt16 &aItemCRC) = 0;
  #else
  /// get first item from the sync set. Caller obtains ownership if aItemP is not NULL after return
  /// @return false if no item found
  virtual bool getFirstItem(TSyncItem *&aItemP) = 0;
  /// get next item from the sync set. Caller obtains ownership if aItemP is not NULL after return
  /// @return false if no item found
  virtual bool getNextItem(TSyncItem *&aItemP) = 0;
  #endif // RECORDHASH_FROM_DBAPI
  #ifdef CHANGEDETECTION_AVAILABLE
  /// get first item's ID and modification status from the sync set
  /// @return false if no item found
  virtual bool getFirstItemInfo(localid_out_t &aLocalID, bool &aItemHasChanged) = 0;
  /// get next item's ID and modification status from the sync set.
  /// @return false if no item found
  virtual bool getNextItemInfo(localid_out_t &aLocalID, bool &aItemHasChanged) = 0;
  #endif // CHANGEDETECTION_AVAILABLE
  /// get item by local ID from the sync set. Caller obtains ownership if aItemP is not NULL after return
  /// @return != LOCERR_OK  if item with specified ID is not found.
  virtual localstatus getItemByID(localid_t aLocalID, TSyncItem *&aItemP) = 0;
  #ifdef RECORDHASH_FROM_DBAPI
  /// get specified item's CRC as calculated by DB
  /// @return != LOCERR_OK  if item with specified ID is not found.
  virtual localstatus getItemCRCByID(localid_t aLocalID, uInt16 &aItemCRC) = 0;
  #endif // RECORDHASH_FROM_DBAPI
  /// end of syncset reading phase (especially for customimplds on top of binfileds)
  virtual localstatus apiEndDataRead(void) { return LOCERR_OK; };
  /// signal start of data write phase
  virtual localstatus apiStartDataWrite(void) { return LOCERR_OK; };
  /// signal end of data write phase
  virtual localstatus apiEndDataWrite(void) { return LOCERR_OK; };
  /// update item by local ID in the sync set. Caller retains ownership of aItemP
  /// @return != LOCERR_OK  if item with specified ID is not found.
  virtual localstatus updateItemByID(localid_t aLocalID, TSyncItem *aItemP) = 0;
  /// delete item by local ID in the sync set.
  /// @return != LOCERR_OK if item with specified ID is not found.
  virtual localstatus deleteItemByID(localid_t aLocalID) = 0;
  /// create new item in the sync set. Caller retains ownership of aItemP.
  /// @return LOCERR_OK or error code.
  /// @param[out] aNewLocalID local ID assigned to new item
  /// @param[out] aReceiveOnly is set to true if local changes/deletion of this item should not be
  ///   reported to the server in normal syncs.
  virtual localstatus createItem(TSyncItem *aItemP,localid_out_t &aNewLocalID, bool &aReceiveOnly) = 0;
  /// zaps the entire datastore, returns LOCERR_OK if ok
  /// @return LOCERR_OK or error code.
  virtual localstatus zapDatastore(void) = 0;
  /// returns timestamp of this sync session (must be used to set mod-timestamp on added and changed records)
  lineartime_t getThisSyncTime(void) { return fCurrentSyncTime; }
  /// get reference time or count to be used as reference date for this sync's modifications
  lineartime_t getThisSyncModRefTime(void);
  uInt32 getThisSyncModCount(void) { return fCurrentModCount; }
  /// get reference time or count to be used in case datastore implementation wants to compare
  /// with the time or count of last sync (last two-way sync, that is!)
  lineartime_t getLastSyncModRefTime(void);
  uInt32 getLastSyncModCount(void) { return fPreviousToRemoteModCount; }
  /// zaps changelog. Should be called if datastore as a entiety was replaced
  /// by another datatstore (or created new)
  void zapChangeLog(void);
  /// check if active derived classes (in particular: customImplDS that CAN derive binfiles, but does not necessarily so)
  bool binfileDSActive(void) { return fConfigP && fConfigP->fBinfileDSActive; };
private:
  /// load changelog into memory for quick access
  void loadChangeLog(void);
  /// forget changelog in memory
  void forgetChangeLog(void);
  /// private helper to prepare for apiSaveAdminData()
  localstatus SaveAdminData(bool aSessionFinished, bool aSuccessful);
  /// load target record for this datastore
  localstatus loadTarget(bool aCreateIfMissing, cAppCharP aRemoteDBID=NULL);
  /* %%% seems obsolete - never called from anywhere
  // private utils
  #ifdef OBJECT_FILTERING
  // - Test Filters
  bool testFilters(TMultiFieldItem *aItemP);
  #endif
  */
  // sync anchor, time and changelog cursor (modcount)
  #ifdef CHECKSUM_CHANGELOG
  /// change log
  TBinFile fChangeLog; ///< change log binfile
  TChangeLogHeader fChgLogHeader; ///< change log header
  // - returns true if we had a valid changelog
  bool openChangeLog(void);
  // - returns true if we had a valid pending map file
  bool openPendingMaps(void);
  // - update change log before syncing. Don't call before types are ok (we need TSyncItems)
  localstatus changeLogPreflight(bool &aValidChangelog);
  // - clean up change log after syncing
  localstatus changeLogPostflight(uInt32 aOldestSyncModCount);
  // - vars
  bool fPreflighted; // set if preflight (changelog update) was done
  // - entire change log, loaded into memory for quick reference during write phase
  TChangeLogEntry *fLoadedChangeLog;
  uInt32 fLoadedChangeLogEntries;
  // - true if there are known pending changes for the next sync
  //   (necessary for hasPendingChangesForNextSync()
  //   because not all of the change log is always in memory)
  bool fHasPendingChanges;
  #endif // CHECKSUM_CHANGELOG
  /// pending maps for Resume
  TBinFile fPendingMaps; ///< pending map binfile
  TPendingMapHeader fPendingMapHeader; ///< pending map header
  // number of local changes
  sInt32 fNumberOfLocalChanges;
  // GetItem vars
  uInt32 fLogEntryIndex;
  // config (typed pointer for convenience)
  TBinfileDSConfig *fConfigP;
}; // TBinfileImplDS

} // namespace sysync

#endif  // BINFILEIMPLDS_H

// eof
