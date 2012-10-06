/*
 *  File:         sysync_globs.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Global definitions/macros/constants
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-xx-xx : luz : created
 *
 */

#ifndef SYSYNC_GLOBS_H
#define SYSYNC_GLOBS_H

#include <stdio.h>

#include "generic_types.h"

// global error codes
#include "syerror.h"

// include sysync-independent part
#include "syncml_globs.h"

// include debug definitions
#include "sysync_debug.h"

// include global progress defs
#include "global_progress.h"


#ifdef __cplusplus
// we need some STL basics as we define types based on STL constructs
#include <string>
#include <list>
#include <map>

using namespace std;
namespace sysync {
#endif

// local status codes (normally generated from SyncML status + LOCAL_STATUS_CODE)
typedef TSyError localstatus;


// configuration switches (DEFAULT_xxx are defaults for corresponding globals)

// - time (in seconds) how long a session will be kept inactive before it times out
#define DEFAULT_SERVERSESSIONTIMEOUT (60*5) // 5 minutes
#define DEFAULT_CLIENTSESSIONTIMEOUT 20 // 20 seconds

// - log file format: datastore
//   - client
#define DEFAULT_LOG_LABELS_CLIENT "SyncEndTime\tUser\tSyncMLVers\tStatus\tSynctype\tSessionID\tRemote ID\tRemote Name\tRemote VersInfo\tDatastore\tLocAdded\tLocUpdated\tLocDeleted\tLocErrors\tRemAdded\tRemUpdated\tRemDeleted\tRemErrors\tBytesOut\tBytesIn\n\n"
#define DEFAULT_LOG_FORMAT_CLIENT "%seT\t%U\t%syV\t%sS\t%tS\t%iS\t%iR\t%nR\t%vR\t%nD\t%laI\t%luI\t%ldI\t%leI\t%raI\t%ruI\t%rdI\t%reI\t%doB\t%diB\n"
//   - server
#define DEFAULT_LOG_LABELS_SERVER "SyncEndTime\tUser\tSyncMLVers\tStatus\tSynctype\tSessionID\tRemote ID\tRemote Name\tRemote VersInfo\tDatastore\tLocAdded\tLocUpdated\tLocDeleted\tLocErrors\tRemAdded\tRemUpdated\tRemDeleted\tRemErrors\tSlowSyncMatches\tServerWon\tClientWon\tDuplicated\tBytesOut\tBytesIn\tSessionBytesOut\tSessionBytesIn\n\n"
#define DEFAULT_LOG_FORMAT_SERVER "%seT\t%U\t%syV\t%sS\t%tS\t%iS\t%iR\t%nR\t%vR\t%nD\t%laI\t%luI\t%ldI\t%leI\t%raI\t%ruI\t%rdI\t%reI\t%smI\t%scI\t%ccI\t%dcI\t%doB\t%diB\t%toB\t%tiB\n"

// - defines debug mask that is active by default
#define DEFAULT_DEBUG DBG_NORMAL
// - if 1, enables message dumps (if included)
#define DEFAULT_MSGDUMP 0
// - if 1, enables XML translation of messages (independent of MSGDUMP)
#define DEFAULT_XMLTRANSLATE 0
// - if 1, enables incoming message simulation (if included)
#define DEFAULT_SIMMSGREAD 0
// - if true, global log file is enabled
#define DEFAULT_GLOBALDEBUGLOGS false
// - if true, session-spcific log files are enabled by default
#define DEFAULT_SESSIONDEBUGLOGS true
// - hard-wired names for debug logs
#define CONFERRPREFIX "sysync_"
#define CONFERRSUFFIX "_cfgerr.log"
#define LOGNAMEPREFIX "sysync_"
#define LOGSUFFIX ".log"
#define MSGDUMPPREFIX "sysync_"
#define MSGDUMPINSUFFIX "_incoming.sml"
#define MSGDUMPOUTSUFFIX "_outgoing.sml"


// Expiry
#ifdef EXPIRES_AFTER_DATE
  // gcc does not like line continuations in DOS text files
  // scalar value representing date
  #define SCRAMBLED_EXPIRY_VALUE  ((EXPIRY_DAY+7l) + (EXPIRY_MONTH-1)*42l + (EXPIRY_YEAR-1720l)*12l*42l)
#endif
// define at least the max difference above formula can have to
// a real difference because of non-smooth increment with time
#define MAX_EXPIRY_DIFF 15 // 42-28(Feb) = 14


#if defined(EXPIRES_AFTER_DATE) || defined(EXPIRES_AFTER_DAYS) || defined(SYSER_REGISTRATION)
#define APP_CAN_EXPIRE
#else
#undef APP_CAN_EXPIRE
#endif

// Hack switches for testing with difficult clients
// - if defined, strings will be encoded as SML_PCDATA_OPAQUE
//#define SML_STRINGS_AS_OPAQUE 1
// - if defined, status for <Sync> command will be issued after
//   statuses for contained SyncOp commands.
//#define SYNCSTATUS_AT_SYNC_CLOSE 1


// general constants
// - used by compare functions to signal compare incompatibility
//   (i.e. non-orderable not-equal)
#define SYSYNC_NOT_COMPARABLE -999

// - maximum auth retries attempted (client) before giving up
//   Note: since 2.0.4.6 this includes the attitional "retry" required to request auth chal from the server
#define MAX_NORMAL_AUTH_RETRIES 2 // Note: retry only once (plus once for chal request) to avoid strict servers (such as COA-S) to lock accounts
#define MAX_SMART_AUTH_RETRIES 4 // when fSmartAuthRetry option is set, some additional retries happen after MAX_NORMAL_AUTH_RETRIES are exhausted
// - maximum auth attempts allowed (server) before session gets aborted
#define MAX_AUTH_ATTEMPTS 3
// - number of message resend retries (client)
#define MAX_MESSAGE_RESENDS 3
// - number of message resend retries before trying older protocol
#define SAME_PROTOCOL_RESENDS 2


// - switch on multithread support if we need it
#if defined(MULTI_THREAD_DATASTORE) || defined(MULTITHREAD_PIPESERVER)
  #define MULTI_THREAD_SUPPORT 1
#endif

// - buffer sizes
#define CONFIG_READ_BUFSIZ 3048 // size of buffer for XML config reading


// Max message size
#ifndef DEFAULT_MAXMSGSIZE
  #ifndef SYSYNC_SERVER
    // only client
    #define DEFAULT_MAXMSGSIZE 20000 // 20k now for DS 1.2 (we had 10k before 3.x)
  #else
    // server (or server and client)
    #define DEFAULT_MAXMSGSIZE 50000 // 50k should be enough
  #endif
#endif

// Max object size
#ifndef DEFAULT_MAXOBJSIZE
  #define DEFAULT_MAXOBJSIZE 4000000 // 4MB should be enough
#endif


// default identification strings
#define SYSYNC_OEM "Synthesis AG"
#define SYSYNC_SERVER_DEVID "SySync Server"
#define SYSYNC_CLIENT_DEVID "SySync Client"
#ifndef SYNCML_SERVER_DEVTYP
  #define SYNCML_SERVER_DEVTYP "server" // could also be "workstation"
#endif
#ifndef SYNCML_CLIENT_DEVTYP
  #define SYNCML_CLIENT_DEVTYP "workstation" // general case, could also be "handheld" or "pda"...
#endif

// SyncML SyncCap mask bits
#define SCAP_MASK_TWOWAY 0x0002         // Support of 'two-way sync' = 1
#define SCAP_MASK_TWOWAY_SLOW 0x0004    // Support of 'slow two-way sync' = 2
#define SCAP_MASK_ONEWAY_CLIENT 0x0008  // Support of 'one-way sync from client only' = 3
#define SCAP_MASK_REFRESH_CLIENT 0x0010 // Support of 'refresh sync from client only' = 4
#define SCAP_MASK_ONEWAY_SERVER 0x0020  // Support of 'one-way sync from server only' = 5
#define SCAP_MASK_REFRESH_SERVER 0x0040 // Support of 'refresh sync from server only' = 6
#define SCAP_MASK_SERVER_ALERTED 0x0080 // Support of 'server alerted sync' = 7
// - minimum needed for conformance
#define SCAP_MASK_MINIMAL (SCAP_MASK_TWOWAY | SCAP_MASK_TWOWAY_SLOW) // Support of 'server alerted sync' = 7
// - normal capabilities
#define SCAP_MASK_NORMAL (SCAP_MASK_MINIMAL | SCAP_MASK_ONEWAY_CLIENT | SCAP_MASK_REFRESH_CLIENT | SCAP_MASK_ONEWAY_SERVER | SCAP_MASK_REFRESH_SERVER)

// DMU / IPP locUris
#define IPP_PARAMS_LOCURI_BASE "./vendor/synthesis/ipp10/" // URI used to address DMU/IPP settings/subscription
#define IPP_PARAMS_LOCURI_CFG IPP_PARAMS_LOCURI_BASE "cfg" // IPP config data from server
#define IPP_PARAMS_LOCURI_REQ IPP_PARAMS_LOCURI_BASE "req" // IPP request from client
#define IPP_PARAMS_ITEM_METATYPE "text/plain" // meta type for IPP req and cfg PUT data items
#define IPP_REQ_ACTIVATE "activate" // activate-request
#define IPP_REQ_SUBSCRIBE "subscribe" // activate-request
#define IPP_REQ_CHECK "check" // check-request

// Remote provisioning
#define SETTINGS_LOCURI_BASE "./vendor/synthesis/settings10/" // URI used to address settings
#define SETTINGS_LOCURI_CFG SETTINGS_LOCURI_BASE "cfg" // settings config data from server
#define SETTINGS_ITEM_METATYPE "text/plain" // meta type for settings config data


// fatal errors
#ifdef __PALM_OS__
  #ifndef PlatFormFatalErr
    #define PlatFormFatalErr { ErrDisplay("PlatFormFatalErr called"); ErrThrow(999); }
  #endif
  #define PlatFormFatalThrow(x) { exception *eP=new x; ErrDisplay(eP->what()); ErrThrow(999); }
  #define PlatFormFatalReThrow { ErrDisplay("C++ re-throw attempted"); ErrThrow(999); }
#elif defined(ANDROID)
  #ifndef PlatFormFatalErr
    #define PlatFormFatalErr { __android_log_write( ANDROID_LOG_DEBUG, "exc", "PlatFormFatalErr called"); exit(999); }
  #endif
  #define PlatFormFatalThrow(x) { exception *eP=new x; __android_log_print( ANDROID_LOG_DEBUG, "exc", "C++ exception thrown: %s",eP->what()); exit(999); }
  #define PlatFormFatalReThrow { __android_log_write( ANDROID_LOG_DEBUG, "exc", "C++ re-throw attempted"); exit(999); }
#else
  #ifndef PlatFormFatalErr
    #define PlatFormFatalErr { printf("PlatFormFatalErr called"); exit(999); }
  #endif
  #define PlatFormFatalThrow(x) { exception *eP=new x; printf("C++ exception thrown: %s",eP->what()); exit(999); }
  #define PlatFormFatalReThrow { printf("C++ re-throw attempted"); exit(999); }
#endif


#ifdef ANDROID
  #define DYN_CAST static_cast
#else
  #define DYN_CAST dynamic_cast
#endif


// exceptions

#ifndef TARGET_HAS_EXCEPTIONS
  // define here depending on compiler
  // if not defined e.g. in target options
  #ifdef __EPOC_OS__
    // no exceptions in EPOC
    #define TARGET_HAS_EXCEPTIONS 0
  #elif defined(__MWERKS__)
    // in Coderwarrior it depends on compiler settings
    #if __option (exceptions)
      #define TARGET_HAS_EXCEPTIONS 1
    #else
      #define TARGET_HAS_EXCEPTIONS 0
    #endif
  #elif defined(WINCE) || defined(ANDROID)
    // no exceptions in eVC
    #define TARGET_HAS_EXCEPTIONS 0
  #else
    // otherwise generally assume yes
    #define TARGET_HAS_EXCEPTIONS 1
  #endif
#endif

#if TARGET_HAS_EXCEPTIONS
  // really throw
  #define SYSYNC_THROW(x) throw x
  #define SYSYNC_RETHROW throw
  #define SYSYNC_TRY try
  #define SYSYNC_CATCH(x) catch(x) {
  #define SYSYNC_ENDCATCH }
#else
  // global fatal error
  #define SYSYNC_THROW(x) PlatFormFatalThrow(x)
  #define SYSYNC_RETHROW PlatFormFatalReThrow
  #define SYSYNC_TRY
  #define SYSYNC_CATCH(x) if(false) { TSmlException e("",SML_ERR_UNSPECIFIC);
  #define SYSYNC_ENDCATCH }
#endif


// checked casts (non-checked when RTTI is not there)
#if defined(WINCE) || defined(__EPOC_OS__) || defined(ANDROID)
  // eVC + symbian has no RTTI, so we rely on having the right type and
  // do a static cast here
  #define GET_CASTED_PTR(dst,ty,src,msg) dst=static_cast<ty *>(src)
#else
  // use dynamic cast and check if pointer is not NULL
  #define GET_CASTED_PTR(dst,ty,src,msg) { dst = dynamic_cast<ty *>(src); if (!dst) { SYSYNC_THROW(TSyncException(msg)); } }
#endif


// global types

// library calling interface
#ifndef SYSYLIBCI
  #define SYSYLIBCI // default to compilers default calling interface
#endif

// extra1 values for pev_debug
#define PEV_DEBUG_FORCERETRY 1

#ifdef __cplusplus

// internal field types (item field types)
typedef enum {
  fty_string,
  fty_telephone,
  fty_integer,
  fty_timestamp,
  fty_date,
  fty_url,
  fty_multiline,
  fty_blob,
  fty_none, // note: this one is mostly internal use
  numFieldTypes
} TItemFieldTypes;


#ifdef SCRIPT_SUPPORT

// built-in function definition
class TItemField;
class TScriptContext;

typedef void (*TBuiltinFunc)(TItemField *&aTermP, TScriptContext *aFuncContextP);

typedef void* (*TTableChainFunc) (void *&aNextCallerContext);

typedef struct {
  // name of function
  cAppCharP fFuncName;
  // implementation
  TBuiltinFunc fFuncProc;
  // return type of function
  TItemFieldTypes fReturntype;
  // parameters
  sInt16 fNumParams;
  // list of parameter definition bytes
  const uInt8 *fParamTypes;
} TBuiltInFuncDef;

typedef struct {
  // number of functions
  uInt16 numFuncs;
  // actual functions
  const TBuiltInFuncDef *funcDefs;
  // chain function
  TTableChainFunc chainFunc;
} TFuncTable;


#endif // SCRIPT_SUPPORT

// progress event
class TLocalDSConfig; // forward

typedef struct {
  TProgressEventType eventtype;
  TLocalDSConfig *datastoreID; // config pointer is used as ID, NULL if global
  sInt32 extra; // extra info, such as error code or count for progress or # of added items
  sInt32 extra2; // extra info, such as total for progress or # of updated items
  sInt32 extra3; // extra info, such as # of deleted items
} TProgressEvent;

// Callbacks

/* %%% from old ages, probably obsolete
// - XML config data reader func type
typedef int SYSYLIBCI (*TXMLTextReadFunc)(
  appCharP aBuffer,
  bufferIndex aMaxSize,
  bufferIndex *aReadCharsP,
  void *aContext
);
// - text message output
typedef void SYSYLIBCI (*TTextMsgProc)(cAppCharP aMessage, void *aContext);
*/

// - progress event notification
typedef bool SYSYLIBCI (*TProgressEventFunc)(
  const TProgressEvent &aEvent,
  void *aContext
);

#endif // __cplusplus



// local status codes (normally generated from SyncML status + LOCAL_STATUS_CODE)
typedef uInt16 localstatus;

// SyncML encodings
// Note: SmlEncoding_t is defined in the RTK smldef.h
#define numSyncMLEncodings (SML_XML-SML_UNDEF+1)


// SyncML Versions
typedef enum {
  syncml_vers_unknown,
  syncml_vers_1_0,
  syncml_vers_1_1,
  syncml_vers_1_2,
  // number of enums
  numSyncMLVersions
} TSyncMLVersions;


// conflict resolution strategies
typedef enum {
  cr_duplicate,    // add conflicting counterpart to both databases
  cr_newer_wins,   // newer version wins (if date/version comparison is possible, like sst_duplicate otherwise)
  cr_server_wins,  // server version wins (and is written to client, with merge if enabled)
  cr_client_wins,  // client version wins (and is written to server, with merge if enabled)
  // number of enums
  numConflictStrategies
} TConflictResolution;


// package states
typedef enum {
  psta_idle,            // not started yet, no package sent or received
  psta_init,            // initialisation package
  psta_sync,            // sync package
  psta_initsync,        // combined initialisation and sync package
  psta_map,             // data update status / map
  psta_supplement,      // extra packages possibly needed at end of session
  // number of enums
  numPackageStates
} TPackageStates;


/// Sync operations
typedef enum {
  sop_wants_add,      ///< like add, but is still available for slowsync match
  sop_add,
  sop_wants_replace,  ///< like replace, but not yet conflict-checked
  sop_replace,
  sop_reference_only, ///< slowsync resume only: like sop_wants_add/sop_wants_replace, but ONLY for comparing with incoming add/replace from client and avoiding add when matching - NEVER send these to client
  sop_archive_delete,
  sop_soft_delete,
  sop_delete,
  sop_copy,
  sop_move, // new for DS 1.2
  sop_none, // should be last
  // number of enums
  numSyncOperations
} TSyncOperation;


// Sync modes (note: slow/refresh is a separate flag)
typedef enum {
  smo_twoway,
  smo_fromserver,
  smo_fromclient,
  // number of enums
  numSyncModes
} TSyncModes;


// filter identifiers
#define isFilterIdent(c) (isalnum(c) || c=='_' || c=='.')


#ifdef __cplusplus

// forwards
class TSmlCommand;

// container for TSmlCommand pointers
typedef std::list<TSmlCommand*> TSmlCommandPContainer; // contains sync commands

// string to string map
typedef std::map<string,string> TStringToStringMap; // string to string map

#endif

#ifdef __cplusplus
} // namespace sysync
#endif

#endif // SYSYNC_GLOBS_H

// eof

