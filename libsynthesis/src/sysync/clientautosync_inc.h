/*
 *  File:         clientautosync_inc.h
 *
 */

#ifdef IPP_SUPPORT
// we need the platform-specific TCP-IP implementation
extern "C" {
  #include "xptitcp.h"
}
#endif

// we need the xpt client base if we directly do TCP/IP
#if defined(IPP_SUPPORT) || !defined(ENGINE_LIBRARY)
  #include "xptclientbase.h"
#endif

#ifdef SERVERALERT_SUPPORT
  #include "san.h"
#endif


// define to debug TCP open/close balance
//#define TCP_LEAK_DEBUG 1


#ifdef GAL_CLIENT_SUPPORT

#define GAL_STRING_MAX 80

// globals
typedef struct {
  // GAL settings
  bool galUpdated;
  char url[GAL_STRING_MAX];
  char domain[GAL_STRING_MAX];
} TGALInfo;

extern TGALInfo gGALInfo;

#endif


// we need some system status routins

// these routines must be implemented in the application
// - get power status (percentage) of the system, returns false if none available
bool getPowerStatus(uInt8 &aBattLevel, bool &aACPowered, void *aMasterPointer);
// - get free memory status (percentage free) of the system, returns false if none available
bool getMemFreeStatus(uInt8 &aMemLevel, void *aMasterPointer);
// - check if device is cradled
bool isDeviceCradled(void *aMasterPointer);


namespace sysync {

// Autosync modes
// note: %%% eventually, SMP GUI still depends on this order - PPC is fixed as per 2006-03-09
typedef enum {
  autosync_ipp, // Intelligent Push&Poll (DMU)
  autosync_timed, // timed sync (scheduled sync)
  autosync_none, // no autosync
  autosync_serveralerted, // SAN - SyncML server alerted sync
  num_autosync_modes
} TAutosyncModes;


// Autosync status
#define autosync_status_wrongday 0x01
#define autosync_status_wrongtime 0x02
#define autosync_status_lowmem 0x04
#define autosync_status_lowbat 0x08
#define autosync_status_disabled 0x10 // disabled by interval=0 (for example)
#define autosync_status_notconnectable 0x20
#define autosync_status_notsubscribed 0x40
#define autosync_status_notlicensed 0x80 // IPP/DMU without license




// Autosync condition level settings record
// NOTE!!!!!: If this struct is changed, this implies a PROFILE_DB_VERSION change!!!
typedef struct {
  TAutosyncModes Mode;
  uInt16 StartDayTime; // [min from midnight] when to start autosync
  uInt16 EndDayTime; // [min from midnight] when to stop autosync
  uInt8 WeekdayMask; // [Bits: 0=sun, 1=mon .. 6=sat] days when autosync is active
  uInt8 ChargeLevel; // percent of battery charge required to activate
  uInt8 MemLevel; // percent of memory that must be free for doing autosync
  uInt8 Flags; // additional condition flags - reserved for future use
} TAutoSyncLevel;


// IPP methods
#define IPP_METHOD_DMU0 0 // simple poll, open specification, used by Toffa SyncWiseEnterprise
#define IPP_METHOD_DMUS2G 1 // proprietary s2g method 1 protocol, confidential spec
#define IPP_METHOD_OMP 2 // Oracle Mobile Push


// Autosync IPP settings record
// NOTE!!!!!: If this struct is changed, this implies a PROFILE_DB_VERSION change!!!
const uInt16 maxippidsiz=64;
const uInt16 maxipppathsiz=64;
typedef struct {
  char srv[maxipppathsiz];
  uInt16 port;
  uInt16 period;
  char path[maxipppathsiz];
  char id[maxippidsiz];
  uInt8 method;
  // new PROFILE_DB_VERSION>=6 fields
  char cred[maxipppathsiz];
  uInt16 maxinterval;
  char timedds[maxipppathsiz];
} TIPPSettings;


typedef enum {
  ippstate_unknown,
  ippstate_idle,
  ippstate_activated,
  ippstate_subscribed,
  num_ipp_states
} TIPPActivationState;


typedef enum {
  ippconvstage_login,
  ippconvstage_subscribe,
  ippconvstage_polling
} TIPPConvStage;


// AutoSync states
typedef enum {
  autosync_idle, // no activity
  autosync_start, // start selected autosync mechanism
  autosync_timedsync, // start pure scheduled sync (calc when we need next sync)
  autosync_timedsync_wait, // waiting for scheduled sync to occur
  autosync_ipp_connectwait, // start waiting until connecting again
  autosync_ipp_connectwaiting, // waiting for connecting again
  autosync_ipp_resubscribe, // cause a timed sync to re-subscribe to IPP
  autosync_alert_implicit, // implicit alert
  autosync_ipp_connect, // connect to server
  autosync_ipp_sendrequest, // send request to server
  autosync_ipp_answerwait, // connected, waiting for answer from IPP server
  autosync_ipp_disconnect, // disconnect from the server
  autosync_ipp_endanswer,  // disconnect after a IPP answer
  autosync_ipp_processanswer, // process collected IPP answer
  num_autosync_states
} TAutoSyncStates;


// IPP parameters
#define IPP_MIN_RECONNECTWAIT 3 // [seconds]
#define IPP_MAX_RECONNECTWAIT 1800  // [seconds] = half an hour
#define IPP_FORCEDOWN_RETRIES 3 // force down connection after 3 failed reconnects
#define IPP_RESUBSCRIBE_RETRIES 5 // re-subscribe after 5 failed reconnects
#define IPP_REACTIVATE_RETRIES 7 // after 7 failed reconnects with resubscription, force a reactivation
#define IPP_BLOCK_RETRIES 9 // after 9 failed reconnects, do not do anything any more
#define IPP_MIN_RESUBSCRIBEWAIT 180 // [seconds] wait minimally 3 minutes until re-trying subcription in case previous sync session did not deliver subscription params
#define IPP_OMP_COMMAND_TIMEOUT 10 // [seconds] wait x seconds for non-poll OMP command to get answer
#define IPP_OMP_RELOGIN_RETRIES 2 // fall back to re-login after 2 failed attempts

// Autosync parameters
#define AUTOSYNC_MAX_ALERTRETRIES 5 // retry max 5 times to start a sync session again after a failed alert
#define AUTOSYNC_FORCEDOWN_RETRIES 2 // after 2 retries, bring down connection first before re-trying
#define AUTOSYNC_MIN_ALERTRETRY 10 // retry after 10,20,40,80 and 160 secs
#define AUTOSYNC_BATTCHECK_INTERVAL 60 // check battery level every minute
#define AUTOSYNC_MEMCHECK_INTERVAL 60 // check memory every minute
#define AUTOSYNC_CONNCHECK_INTERVAL 10 // check connection status every 10 seconds
#define AUTOSYNC_ENVERRSDISPLAY_SECS 15 // show battery/mem errors 15 seconds after failed timed autosync

#ifdef IPP_SUPPORT
// applies the value to the IPP parameter identified by aTag
// This can be called from client provisioning code or from a sync session's GET
// command RESULT (if the server supports IPP autoconfig)
void ipp_setparam(const char *aTag, const char *aValue, TIPPSettings &aIPPSettings);
#endif

#ifdef TCP_LEAK_DEBUG
  #define TCP_LEAKCOUNTER uInt32 fTcpLeakCounter;
#else
  #define TCP_LEAKCOUNTER
#endif

#ifdef IPP_SUPPORT
  #define IPP_VARS \
    uInt16 fAutosyncRetries; /* retry count for IPP connection attempts */ \
    Socket_t fAutosyncIPPSocket; /* the socket used for IPP */ \
    string fAutosyncIPPAnswer; /* the IPP answer is collected here */ \
    TIPPActivationState fIPPState; /* the IPP activation state */ \
    TIPPConvStage fConversationStage; /* conversation stage within IPP poll connection (OMG only at this time) */ \
    TLocalDSList::iterator fDSCfgIterator; /* iterator for datastores (used for SUBSCRIBE in OMP) */ \
    string fOMPCookie; /* cookie for OMP */ \
    uInt32 fHeaderLength; /* header length */ \
    uInt32 fContentLength; /* content length */
  #define IPP_METHODS
#else
  #define IPP_METHODS
  #define IPP_VARS
#endif

#ifdef SERVERALERT_SUPPORT
  #define SAN_VARS \
    uInt32 fAutosyncSANSessionID; /* Session ID as requested by SAN */
  #define SAN_METHODS \
    /* - analyzes server alert (SAN) message */ \
    localstatus autosync_processSAN(bool aLocalPush, void *aPushMsg, uInt32 aPushMsgSz, cAppCharP aHeaders, sInt32 &aAlertedProfile, UI_Mode &aUIMode);
#else
  #define SAN_METHODS
  #define SAN_VARS
#endif



// macro to declare autosync methods and fields in agent config
#ifndef AUTOSYNC_SUPPORT
#define CLIENTAUTOSYNC_CLASSDECL
#else
#define CLIENTAUTOSYNC_CLASSDECL \
public: \
  /* Autosync methods */ \
  /* - init autosync mechanism */ \
  void autosync_init(lineartime_t aLastAlert); \
  /* - set profile */ \
  void autosync_setprofile(sInt32 aProfileIndex, bool aHasChanged); \
  /* must be called to obtain DMU PUT command data to send to SyncML server */ \
  void autosync_get_putrequest(string &aReq); \
  /* - should be called when environmental params have changed */ \
  void autosync_condchanged(void); \
  /* - check if app may quit */ \
  bool autosync_canquit(lineartime_t &aRestartAt, bool aEnabled); \
  /* - check if we need a sync now */ \
  bool autosync_check(bool aActive); \
  /* - get status information about autosync */ \
  void autosync_status( \
    TAutosyncModes &aAutosyncMode, /* currently active mode */ \
    lineartime_t &aNextSync, /* 0 if no scheduled nextsync */ \
    lineartime_t &aLastAlert, /* 0 if no alert seen so far */ \
    uInt8 &aWhyNotFlags /* why-no-autosync status flags */ \
  ); \
  /* - returns true if we need to perform a sync session now */ \
  void autosync_stop(void); \
  /* - performs next step in the state machine */ \
  void autosync_step(bool aActive); \
  /* - forget or activate syncs alerted for autosync in DS config */ \
  void autosync_ackAlert(bool aAck); \
  /* - confirm that autosync has taken place (clears alerted flag) */ \
  void autosync_confirm(bool aUnsuccessful=false); \
  /* - parse ipp params */ \
  void ipp_setparam(const char *aTag, const char *aValue, TIPPSettings &aIPPSettings); \
  IPP_METHODS \
  SAN_METHODS \
  /* - profile record */ \
  PROFILERECORD fAutosyncProfile; \
  sInt32 fAutosyncProfileLastidx; /* -1 if no profile selected before */ \
private: \
  /* alert a datastore by remote database name */ \
  bool alertDbByName(const char *aRemoteDBName, const char *aFilterCGI, bool aForced, uInt16 syncAlert=0, bool aAlwaysStart=false); \
  /* alert a datastore by generic type flag */ \
  bool alertDbByAppType(uInt16 aDSAvailFlag, bool aForced, bool aAlwaysStart=false); \
  /* private vars needed for autosync */ \
  /* - state vars */ \
  sInt16 fCurrentLevelIndex; /* index of currently active autosync level, -1 if none */ \
  TAutosyncModes fAutoSyncMode; /* currently active autosync mode */ \
  TAutosyncModes fNewMode; /* new target mode */ \
  lineartime_t fNextCheckAt; /* when we need to check for mode change or timed sync next, 0 if immediately */ \
  lineartime_t fNextWakeupCheckAt; /* when we need to wakeup app to check levels, 0 if immediately */ \
  uInt8 fWhyNotFlags; /* reason why we cannot run autosync now */ \
  lineartime_t fEnvCheckedAt; /* when we last checked the mem/batt environmental factors */ \
  lineartime_t fNextSyncAt; /* when next sync will take place */ \
  TAutoSyncStates fAutosyncState; /* current state machine state */ \
  bool fAutosyncAlerted; /* set when some datastores get alerted, cleared when autosync_confirm() */ \
  lineartime_t fAutosyncLastStateChange; /* time when state changed last time */ \
  uInt16 fUnsuccessfulAlerts; /* if > 0, we had unsuccessful alerts - delay re-alerting */ \
  IPP_VARS \
  SAN_VARS \
  bool fCurrentlyCradled; /* the current IPP connection is assuming cradled operation */ \
  /* timed sync */ \
  lineartime_t fAutosyncLastAlert; /* time when last autosync was alerted */ \
  TCP_LEAKCOUNTER
#endif


// macro to declare autosync fields in DB (target) config
#ifndef AUTOSYNC_SUPPORT
#define CLIENTAUTOSYNC_CLASSDECL_DB
#else
#define CLIENTAUTOSYNC_CLASSDECL_DB \
public: \
  bool fAutosyncAlerted; /* set if server requested sync of this datastore */ \
  bool fAutosyncForced; /* set if server requests sync of this datastore even if target is disabled */ \
  uInt16 fAutosyncAlertCode; /* if not 0, this is the alert code requested e.g. by a SAN initiated autosync */ \
  string fAutosyncPathCGI; /* the CGI to be appended to the database path */
#endif

} // namespace sysync


/* eof */
