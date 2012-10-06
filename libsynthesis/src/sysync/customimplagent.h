/**
 *  @File     customimplagent.h
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TCustomImplAgent
 *    Base class for agenst (servers or clients) with customizable datastores
 *    based on TCustomImplDS
 *
 *    Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-12-05 : luz : separated from odbcdbagent
 */

#ifndef CUSTOMIMPLAGENT_H
#define CUSTOMIMPLAGENT_H

// includes

#include "stdlogicagent.h"
#include "stdlogicds.h"
#include "multifielditem.h"
#include "scriptcontext.h"

#ifdef BASED_ON_BINFILE_CLIENT
  #include "binfileimplclient.h"
#endif

using namespace sysync;

namespace sysync {

#ifdef SCRIPT_SUPPORT

// publish as derivates might need it
extern const TFuncTable CustomAgentFuncTable;
extern const TFuncTable CustomAgentFuncTable2;
extern const TFuncTable CustomDSFuncTable1;
// - define count here as derivates will access the funcdefs table directly
const int numCustomAgentFuncs=14;
extern const TBuiltInFuncDef CustomAgentFuncDefs[numCustomAgentFuncs];
#ifndef BINFILE_ALWAYS_ACTIVE
  const int numCustomAgentAndDSFuncs=5;
#else
  const int numCustomAgentAndDSFuncs=3;
#endif
extern const TBuiltInFuncDef CustomAgentAndDSFuncDefs[numCustomAgentAndDSFuncs];

#endif


// config names for character sets
extern const char * const DBCharSetNames[numCharSets];

// database field types
typedef enum {
  dbft_string,
  dbft_blob, // binary contents of string will be used, no translations
  dbft_date,
  dbft_time,
  dbft_timefordate, // to add time to a date
  dbft_timestamp,
  dbft_dateonly, // timestamp, but carrying only a date, so no zone correction is applied
  dbft_uctoffsfortime_hours, // to add UTC offset to a time or timestamp
  dbft_uctoffsfortime_mins, // to add UTC offset to a time or timestamp
  dbft_uctoffsfortime_secs, // to add UTC offset to a time or timestamp
  dbft_zonename, // to get zone name of a timestamp
  dbft_numeric,  // string value is used w/o any quotes in SQL
  dbft_lineartime, // linear time as-is
  dbft_lineardate, // linear date as-is
  dbft_unixtime_s, // integer value representing UNIX epoch time stamp in seconds
  dbft_unixtime_ms, // integer value representing UNIX epoch time stamp in milliseconds
  dbft_unixtime_us, // integer value representing UNIX epoch time stamp in microseconds
  dbft_unixdate_s,  // integer value representing UNIX epoch date in seconds
  dbft_unixdate_ms, // integer value representing UNIX epoch date in milliseconds
  dbft_unixdate_us,  // integer value representing UNIX epoch date in microseconds
  dbft_nsdate_s, // integer value representing NSDate (seconds since 2001-01-01 00:00)
  numDBfieldTypes
} TDBFieldType;

extern const char * const DBFieldTypeNames[numDBfieldTypes];

// integer in DB <-> lineartime conversions
lineartime_t dbIntToLineartime(sInt64 aDBInt, TDBFieldType aDbfty);
sInt64 lineartimeToDbInt(lineartime_t aLinearTime, TDBFieldType aDbfty);


// forward
class TCustomDSConfig;
#ifdef SCRIPT_SUPPORT
class TScriptContext;
#endif


class TCustomAgentConfig:
  #ifdef BASED_ON_BINFILE_CLIENT
  public TBinfileClientConfig
  #else
  public TAgentConfig
  #endif
{
  #ifdef BASED_ON_BINFILE_CLIENT
  typedef TBinfileClientConfig inherited;
  #else
  typedef TAgentConfig inherited;
  #endif
public:
  TCustomAgentConfig(TConfigElement *aParentElement);
  virtual ~TCustomAgentConfig();
  // properties
  #ifdef SCRIPT_SUPPORT
  // - login scripts
  string fLoginInitScript; // called to initialize variables and early check for auth
  string fLoginCheckScript; // called to check permission for all users in question
  string fLoginFinishScript; // called when login is finished (failed or successful)
  #endif // SCRIPT_SUPPORT
  // - Date/Time info
  bool fCurrentDateIsUTC; //%%% legacy flag for compatibility, superseded by fCurrentDateTimeZone
  timecontext_t fCurrentDateTimeZone; // time zone for storing/retrieving timestamps in DB
  // - charset to be used in the data table
  TCharSets fDataCharSet;
  // - line end mode to be used in the data table for multiline data
  TLineEndModes fDataLineEndMode;
  #ifdef SCRIPT_SUPPORT
  // provided to allow derivates to add API specific script functions to scripts called from customagent
  virtual const TFuncTable *getAgentFuncTableP(void) {
    #ifndef BASED_ON_BINFILE_CLIENT
    return &CustomAgentFuncTable;
    #else
    return NULL;
    #endif
  };
  TScriptContext *fResolverContext;
  #endif
protected:
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void clear();
  virtual void localResolve(bool aLastPass);
  #ifdef SCRIPT_SUPPORT
  virtual void ResolveAPIScripts(void) { /* NOP */ };
  #endif
}; // TCustomAgentConfig


class TCustomImplDS;

class TCustomImplAgent:
  #ifdef BASED_ON_BINFILE_CLIENT
  public TBinfileImplClient
  #else
  public TStdLogicAgent
  #endif
{
  friend class TCustomCommonFuncs;
  friend class TCustomAgentFuncs;
  #ifdef BASED_ON_BINFILE_CLIENT
  typedef TBinfileImplClient inherited;
  #else
  typedef TStdLogicAgent inherited;
  #endif
public:
  TCustomImplAgent(TSyncAppBase *aAppBaseP, TSyncSessionHandle *aSessionHandleP, cAppCharP aSessionID);
  virtual ~TCustomImplAgent();
  virtual void TerminateSession(void); // Terminate session, like destructor, but without actually destructing object itself
  virtual void ResetSession(void); // Resets session (but unlike TerminateSession, session might be re-used)
  void InternalResetSession(void); // static implementation for calling through virtual destructor and virtual ResetSession();
  // Custom agent config
  TCustomAgentConfig *fConfigP;
  // Needed for general script support
  #ifdef SCRIPT_SUPPORT
  TCustomImplDS *fScriptContextDatastore; // needed because datastore chains agent's script funcs
  #endif // SCRIPT_SUPPORT
  #ifdef DBAPI_TUNNEL_SUPPORT
  // Initialize a datastore tunnel session
  virtual localstatus InitializeTunnelSession(cAppCharP aDatastoreName);
  virtual TLocalEngineDS *getTunnelDS();
  #endif
  #ifndef BASED_ON_BINFILE_CLIENT
  // if binfiles are not compiled in, they are always inactive (otherwise binfileclient parent defines this method)
  bool binfilesActive(void) { return false; };  
  #endif
  // Login and device management only if not exclusively based on binfile client
  #ifndef BINFILE_ALWAYS_ACTIVE
  // - login for this session
  virtual bool SessionLogin(const char *aUserName, const char *aAuthString, TAuthSecretTypes aAuthStringType, const char *aDeviceID);
  // - clean up after all login activity is over (including finishscript)
  virtual void LoginCleanUp(void) { /* NOP at this level */ };
  // - get user key
  const char *getUserKey(void) { return fUserKey.c_str(); };
  // status vars, valid during login context script execution only!
  #ifdef SCRIPT_SUPPORT
  bool fStandardAuthOK; // result of standard auth checking
  string fUserName; // possibly modified user name used to find user in DB
  string fDomainName; // domain name possibly derived from LOCALURI or similar
  const char *fAuthUser; // original username as sent by remote
  const char *fAuthSecret; // secret from remote (normally, MD5 of user:password)
  const char *fAuthDevice; // device ID
  TAuthSecretTypes fAuthType; // set if V11-type secret
  bool fUnknowndevice; // set if device is unknown so far
  #endif // SCRIPT_SUPPORT
  // session vars
  string fUserKey; // user key obtained at SessionLogin
  string fDeviceKey; // device key obtained at SessionLogin
protected:
  // - check device ID related stuff
  virtual void CheckDevice(const char *aDeviceID) { /* NOP here */ };
  // - check user/pw (part of SessionLogin process)
  virtual bool CheckLogin(const char *aOriginalUserName, const char *aModifiedUserName, const char *aAuthString, TAuthSecretTypes aAuthStringType, const char *aDeviceID) { return false; /* cannot login */ };
  // - remote device is analyzed, possibly save status
  virtual void remoteAnalyzed(void) { /* NOP at this level */ };
  // script contexts
  #endif // BINFILE_ALWAYS_ACTIVE
  #ifdef SCRIPT_SUPPORT
  TScriptContext *fAgentContext;
  #endif // SCRIPT_SUPPORT
  #ifdef DBAPI_TUNNEL_SUPPORT
  TCustomImplDS *fTunnelDatastoreP;
  #endif // DBAPI_TUNNEL_SUPPORT
}; // TCustomImplAgent


} // namespace sysync

#endif  // CUSTOMIMPLAGENT_H

// eof
