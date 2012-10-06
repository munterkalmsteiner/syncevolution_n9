/**
 *  @File     customimplagent.cpp
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TCustomAgent
 *    Base class for agenst (servers or clients) with customizable datastores
 *    based on TCustomImplDS
 *
 *    Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-12-05 : luz : separated from odbcdbagent
 */


// includes
#include "sysync.h"
#include "multifielditem.h"
#include "mimediritemtype.h"

#include "customimplagent.h"
#include "customimplds.h"

namespace sysync {


// Charset names for DB charset specification
// Note: numCharSets and the TCharSets type enum is defined in sysync_utils
const char * const DBCharSetNames[numCharSets] = {
  "unknown",
  "ASCII",
  "ANSI",
  "ISO-8859-1",
  "UTF-8",
  "UTF-16",
  #ifdef CHINESE_SUPPORT
  "GB2312",
  "CP936",
  #endif
};


// database field type names
const char * const DBFieldTypeNames[numDBfieldTypes] = {
  "string",
  "blob",
  "date",
  "time",
  "timefordate",
  "timestamp",
  "dateonly",
  "zoneoffset_hours",
  "zoneoffset_mins",
  "zoneoffset_secs",
  "zonename",
  "numeric",
  "lineartime",
  "lineardate",
  "unixtime_s",
  "unixtime_ms",
  "unixtime_us",
  "unixdate_s",
  "unixdate_ms",
  "unixdate_us",
  "nsdate"
};



// Generic Utils
// =============

// integer in DB to lineartime conversion
lineartime_t dbIntToLineartime(sInt64 aDBInt, TDBFieldType aDbfty)
{
  switch (aDbfty) {
    case dbft_unixtime_s:
    case dbft_unixdate_s:
      // integer value representing UNIX epoch date in seconds
      return secondToLinearTimeFactor*aDBInt+UnixToLineartimeOffset;
    case dbft_nsdate_s:
      // integer value representing NSDate in seconds
      return secondToLinearTimeFactor*aDBInt+NSDateToLineartimeOffset;
    case dbft_unixtime_ms:
    case dbft_unixdate_ms:
      // integer value representing UNIX epoch date in milliseconds
      return aDBInt*secondToLinearTimeFactor/1000+UnixToLineartimeOffset;
    case dbft_unixdate_us:
    case dbft_unixtime_us:
      // integer value representing UNIX epoch time stamp in microseconds
      return aDBInt*secondToLinearTimeFactor/1000000+UnixToLineartimeOffset;
    case dbft_lineardate:
      // linear date as-is
      return aDBInt*linearDateToTimeFactor;
    case dbft_lineartime:
    default:
      // linear time as-is
      return aDBInt;
  }
} // dbIntToLineartime


// lineartime to integer in DB conversion
sInt64 lineartimeToDbInt(lineartime_t aLinearTime, TDBFieldType aDbfty)
{
  switch (aDbfty) {
    case dbft_unixtime_s:
    case dbft_unixdate_s:
      // integer value representing UNIX epoch date in seconds
      return (aLinearTime-UnixToLineartimeOffset)/secondToLinearTimeFactor;
    case dbft_nsdate_s:
      // integer value representing NSDate in seconds
      return (aLinearTime-NSDateToLineartimeOffset)/secondToLinearTimeFactor;
    case dbft_unixtime_ms:
    case dbft_unixdate_ms:
      // integer value representing UNIX epoch date in milliseconds
      return (aLinearTime-UnixToLineartimeOffset)*1000/secondToLinearTimeFactor;
    case dbft_unixdate_us:
    case dbft_unixtime_us:
      // integer value representing UNIX epoch time stamp in microseconds
      return (aLinearTime-UnixToLineartimeOffset)*1000000/secondToLinearTimeFactor;
    case dbft_lineardate:
      // linear date as-is
      return aLinearTime/linearDateToTimeFactor;
    case dbft_lineartime:
    default:
      // linear time as-is
      return aLinearTime;
  }
} // lineartimeToDbInt

// Config
// ======

TCustomAgentConfig::TCustomAgentConfig(TConfigElement *aParentElement) :
  #ifdef BASED_ON_BINFILE_CLIENT
  TBinfileClientConfig(aParentElement)
  #else
  TAgentConfig("CustomAgent",aParentElement)
  #endif
  #ifdef SCRIPT_SUPPORT
  , fResolverContext(NULL)
  #endif
{
  // nop so far
} // TCustomAgentConfig::TCustomAgentConfig


TCustomAgentConfig::~TCustomAgentConfig()
{
  clear();
} // TCustomAgentConfig::~TCustomAgentConfig


// init defaults
void TCustomAgentConfig::clear(void)
{
  // init defaults
  fCurrentDateIsUTC=false; // compatibility flag only, will set fCurrentDateTimeZone to TCTX_UTC at Resolve if set
  fCurrentDateTimeZone=TCTX_SYSTEM; // assume system local time
  fDataCharSet=chs_ansi; // assume ANSI, is probable for ODBC connection
  fDataLineEndMode=lem_dos; // default to CRLF, as this seems to be safest assumption
  // resolver context
  #ifndef BINFILE_ALWAYS_ACTIVE
  #ifdef SCRIPT_SUPPORT
  fLoginInitScript.erase();
  fLoginCheckScript.erase();
  fLoginFinishScript.erase();
  if (fResolverContext) {
    delete fResolverContext;
    fResolverContext=NULL;
  }
  #endif
  #endif // BINFILE_ALWAYS_ACTIVE
  // clear inherited
  inherited::clear();
} // TCustomAgentConfig::clear


#ifdef SCRIPT_SUPPORT


// Custom agent specific script functions
// ======================================


class TCustomAgentFuncs {
public:

  #ifndef BINFILE_ALWAYS_ACTIVE
  // Login context functions

  // integer CHECKAUTH(string user, string secret, integer secretismd5)
  // returns auth status of checking given user name and given secret against user/secret from remote
  static void func_CheckAuth(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get local side info
    string dbuser,dbsecret,nonce;
    bool secretismd5,authok;
    aFuncContextP->getLocalVar(0)->getAsString(dbuser);
    aFuncContextP->getLocalVar(1)->getAsString(dbsecret);
    secretismd5=aFuncContextP->getLocalVar(2)->getAsBoolean();
    // check against remote side info.
    TCustomImplAgent *agentP = static_cast<TCustomImplAgent *>(aFuncContextP->getCallerContext());
    if (agentP->fAuthType==sectyp_md5_V10 || agentP->fAuthType==sectyp_md5_V11)
      agentP->getAuthNonce(agentP->fAuthDevice,nonce);
    if (!secretismd5) {
      // local secret is clear text password, check against what was transmitted from remote
      authok=agentP->checkAuthPlain(
        dbuser.c_str(), // user name as specified by caller
        dbsecret.c_str(), // secret as specfied by caller (usually retrieved from DB)
        nonce.c_str(), // the nonce
        agentP->fAuthSecret, // the auth secret as transmitted from remote
        agentP->fAuthType // the type of the auth secret
      );
    }
    else {
      // local secret is b64(md5(user:password))
      authok=
        agentP->fAuthType==sectyp_md5_V11 && // auth secret from remote MUST be V1.1 type MD5
        agentP->checkMD5WithNonce(
          dbsecret.c_str(), // b64(md5(user:password)) as provided by caller to check against
          nonce.c_str(), // the nonce
          agentP->fAuthSecret // auth secret according to SyncML 1.1 (b64(md5(b64(md5(user:pw)):nonce))
        );
    }
    // return auth checking result
    aTermP->setAsInteger(authok ? 1 : 0);
  }; // func_CheckAuth


  // integer AUTHOK()
  // returns current auth ok status (standard checking enabled)
  static void func_AuthOK(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsInteger(
      static_cast<TCustomImplAgent *>(aFuncContextP->getCallerContext())->fStandardAuthOK ? 1 : 0
    );
  }; // func_AuthOK


  // string AUTHUSER()
  // returns name of user that tries to authenticate
  static void func_AuthUser(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsString(
      static_cast<TCustomImplAgent *>(aFuncContextP->getCallerContext())->fAuthUser
    );
  }; // func_AuthUser


  // SETUSERNAME(string username)
  // set user name that will be used to find user in DB
  // (but note that original username will be used to check auth with MD5 auth)
  static void func_SetUserName(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aFuncContextP->getLocalVar(0)->getAsString(
      static_cast<TCustomImplAgent *>(aFuncContextP->getCallerContext())->fUserName
    );
  }; // func_SetUserName


  // SETDOMAIN(string domainname)
  // set "domain" name that can be used to differentiate user domains
  // (mainly an xml2go requirement)
  static void func_SetDomain(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aFuncContextP->getLocalVar(0)->getAsString(
      static_cast<TCustomImplAgent *>(aFuncContextP->getCallerContext())->fDomainName
    );
  }; // func_SetDomain



  // string AUTHSTRING()
  // returns auth string (MD5 digest or plain text, according to AUTHTYPE())
  // sent by user trying to authenticate
  static void func_AuthString(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsString(
      static_cast<TCustomImplAgent *>(aFuncContextP->getCallerContext())->fAuthSecret
    );
  }; // func_AuthString


  // string AUTHDEVICEID()
  // returns remote device ID
  static void func_AuthDeviceID(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsString(
      static_cast<TCustomImplAgent *>(aFuncContextP->getCallerContext())->fAuthDevice
    );
  }; // func_AuthDeviceID


  // integer AUTHTYPE()
  // returns
  static void func_AuthType(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsInteger(
      (fieldinteger_t) static_cast<TCustomImplAgent *>(aFuncContextP->getCallerContext())->fAuthType
    );
  }; // func_AuthType


  // integer UNKNOWNDEVICE()
  // returns if device was not yet in device table (valid in fLoginCheckScript only)
  static void func_Unknowndevice(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsInteger(
      static_cast<TCustomImplAgent *>(aFuncContextP->getCallerContext())->fUnknowndevice ? 1 : 0
    );
  }; // func_Unknowndevice


  // string USERKEY()
  // returns user key
  static void func_UserKey(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsString(
      static_cast<TCustomImplAgent *>(aFuncContextP->getCallerContext())->fUserKey
    );
  }; // func_UserKey


  // SETUSERKEY(variant userkey)
  // set user key for this sync session
  static void func_SetUserKey(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aFuncContextP->getLocalVar(0)->getAsString(
      static_cast<TCustomImplAgent *>(aFuncContextP->getCallerContext())->fUserKey
    );
  }; // func_SetUserKey


  // string DEVICEKEY()
  // returns device key
  static void func_DeviceKey(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsString(
      static_cast<TCustomImplAgent *>(aFuncContextP->getCallerContext())->fDeviceKey
    );
  }; // func_DeviceKey


  // SETDEVICEKEY(variant devicekey)
  // set device key for this sync session
  static void func_SetDeviceKey(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aFuncContextP->getLocalVar(0)->getAsString(
      static_cast<TCustomImplAgent *>(aFuncContextP->getCallerContext())->fDeviceKey
    );
  }; // func_SetDeviceKey

  #endif // not BINFILE_ALWAYS_ACTIVE


  // timestamp DBINTTOTIMESTAMP(integer dbint,string dbfieldtype)
  // convert database integer to timestamp
  static void func_DBIntToTimestamp(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string tname,literal;
    aFuncContextP->getLocalVar(1)->getAsString(tname); // DB type string
    // search DB type, default to lineartime
    sInt16 ty;
    TDBFieldType dbfty=dbft_lineartime;
    if (StrToEnum(DBFieldTypeNames,numDBfieldTypes,ty,tname.c_str()))
      dbfty=(TDBFieldType)ty;
    // now set timestamp
    static_cast<TTimestampField *>(aTermP)->setTimestampAndContext(
      dbIntToLineartime(aFuncContextP->getLocalVar(0)->getAsInteger(), dbfty), // timestamp
      TCTX_UNKNOWN // unknown zone
    );
  }; // func_DBIntToTimestamp


  // integer TIMESTAMPTODBINT(timestamp ts,string dbfieldtype)
  // convert database integer to timestamp
  static void func_TimestampToDBInt(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string tname,literal;
    aFuncContextP->getLocalVar(1)->getAsString(tname); // DB type string
    // search DB type, default to lineartime
    sInt16 ty;
    TDBFieldType dbfty=dbft_lineartime;
    if (StrToEnum(DBFieldTypeNames,numDBfieldTypes,ty,tname.c_str()))
      dbfty=(TDBFieldType)ty;
    // now set timestamp
    aTermP->setAsInteger(
      lineartimeToDbInt(
        static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0))->getTimestampAs(TCTX_UNKNOWN),
        dbfty
      )
    );
  }; // func_TimestampToDBInt


  // timestamp CONVERTTODATAZONE(timestamp atime [,boolean doUnfloat])
  // returns timestamp converted to database time zone.
  // - If doUnfloat, floating timestamps will be fixed in the new zone w/o conversion of the timestamp itself.
  static void func_ConvertToDataZone(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get DB zone
    TCustomImplAgent *agentP = static_cast<TCustomImplAgent *>(aFuncContextP->getCallerContext());
    TCustomImplDS *datastoreP = static_cast<TCustomImplDS *>(agentP->fScriptContextDatastore);
    timecontext_t actual,tctx;
    if (datastoreP)
      tctx = static_cast<TCustomDSConfig *>(datastoreP->getDSConfig())->fDataTimeZone;
    else
      tctx = agentP->fConfigP->fCurrentDateTimeZone;
    // get timestamp
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));
    // convert and get actually resulting context back
    lineartime_t ts = tsP->getTimestampAs(tctx,&actual);
    // unfloat floats if selected
    if (aFuncContextP->getLocalVar(1)->getAsBoolean() && TCTX_IS_UNKNOWN(actual)) actual=tctx; // unfloat
    // assign it to result
    static_cast<TTimestampField *>(aTermP)->setTimestampAndContext(ts,actual);
  }; // func_ConvertToDataZone


}; // TCustomAgentFuncs


const uInt8 param_CheckAuth[] = { VAL(fty_string), VAL(fty_string), VAL(fty_integer) };
const uInt8 param_DBIntToTimestamp[] = { VAL(fty_integer), VAL(fty_string) };
const uInt8 param_TimestampToDBInt[] = { VAL(fty_timestamp), VAL(fty_string) };
const uInt8 param_ConvertToDataZone[] = { VAL(fty_timestamp), OPTVAL(fty_integer) };

const uInt8 param_oneString[] = { VAL(fty_string) };
const uInt8 param_oneInteger[] = { VAL(fty_integer) };
const uInt8 param_variant[] = { VAL(fty_none) };


#ifndef BINFILE_ALWAYS_ACTIVE

// builtin function table for login context
const TBuiltInFuncDef CustomAgentFuncDefs[numCustomAgentFuncs] = {
  { "AUTHOK", TCustomAgentFuncs::func_AuthOK, fty_integer, 0, NULL },
  { "CHECKAUTH", TCustomAgentFuncs::func_CheckAuth, fty_integer, 3, param_CheckAuth },
  { "AUTHUSER", TCustomAgentFuncs::func_AuthUser, fty_string, 0, NULL },
  { "SETUSERNAME", TCustomAgentFuncs::func_SetUserName, fty_none, 1, param_oneString },
  { "SETDOMAIN", TCustomAgentFuncs::func_SetDomain, fty_none, 1, param_oneString },
  { "AUTHSTRING", TCustomAgentFuncs::func_AuthString, fty_string, 0, NULL },
  { "AUTHDEVICEID", TCustomAgentFuncs::func_AuthDeviceID, fty_string, 0, NULL },
  { "AUTHTYPE", TCustomAgentFuncs::func_AuthType, fty_integer, 0, NULL },
  { "UNKNOWNDEVICE", TCustomAgentFuncs::func_Unknowndevice, fty_integer, 0, NULL },
  { "SETUSERKEY", TCustomAgentFuncs::func_SetUserKey, fty_none, 1, param_variant },
  { "SETDEVICEKEY", TCustomAgentFuncs::func_SetDeviceKey, fty_none, 1, param_variant },
};

#endif // not BINFILE_ALWAYS_ACTIVE


// builtin function defs for customImpl database and login contexts
const TBuiltInFuncDef CustomAgentAndDSFuncDefs[numCustomAgentAndDSFuncs] = {
  #ifndef BINFILE_ALWAYS_ACTIVE
  { "DEVICEKEY", TCustomAgentFuncs::func_DeviceKey, fty_string, 0, NULL },
  { "USERKEY", TCustomAgentFuncs::func_UserKey, fty_string, 0, NULL },
  #endif
  { "DBINTTOTIMESTAMP", TCustomAgentFuncs::func_DBIntToTimestamp, fty_timestamp, 2, param_DBIntToTimestamp },
  { "TIMESTAMPTODBINT", TCustomAgentFuncs::func_TimestampToDBInt, fty_integer, 2, param_TimestampToDBInt },
  { "CONVERTTODATAZONE", TCustomAgentFuncs::func_ConvertToDataZone, fty_timestamp, 2, param_ConvertToDataZone },
};


#ifndef BINFILE_ALWAYS_ACTIVE

// function table which is chained from login-context function table
const TFuncTable CustomAgentFuncTable2 = {
  sizeof(CustomAgentAndDSFuncDefs) / sizeof(TBuiltInFuncDef), // size of table
  CustomAgentAndDSFuncDefs, // table pointer
  NULL // no chain func
};


// chain from login context agent funcs to general agent funcs
extern const TFuncTable CustomDSFuncTable2;
static void *CustomAgentChainFunc(void *&aCtx)
{
  // caller context remains unchanged
  // -> no change needed
  // next table is Agent's general function table
  return (void *)&CustomAgentFuncTable2;
} // CustomAgentChainFunc

// function table for login context scripts
const TFuncTable CustomAgentFuncTable = {
  sizeof(CustomAgentFuncDefs) / sizeof(TBuiltInFuncDef), // size of table
  CustomAgentFuncDefs, // table pointer
  CustomAgentChainFunc // chain to general agent funcs.
};

#endif // not BINFILE_ALWAYS_ACTIVE


// chain from agent funcs to custom datastore funcs (when chained via CustomDSFuncTable1
extern const TFuncTable CustomDSFuncTable2;
static void *CustomDSChainFunc1(void *&aCtx)
{
  // caller context for datastore-level functions is the datastore pointer
  if (aCtx)
    aCtx = static_cast<TCustomImplAgent *>(aCtx)->fScriptContextDatastore;
  // next table is custom datastore's
  return (void *)&CustomDSFuncTable2;
} // CustomDSChainFunc1

// function table which is used by CustomImplDS scripts to access agent-level funcs and then chain
// back to datastore level funcs
const TFuncTable CustomDSFuncTable1 = {
  sizeof(CustomAgentAndDSFuncDefs) / sizeof(TBuiltInFuncDef), // size of agent's table
  CustomAgentAndDSFuncDefs, // table pointer to agent's general purpose (non login-context specific) funcs
  CustomDSChainFunc1 // chain to ODBC datastore level DB functions
};


#endif // SCRIPT_SUPPORT



// config element parsing
bool TCustomAgentConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  #ifndef BINFILE_ALWAYS_ACTIVE
  #ifdef SCRIPT_SUPPORT
  if (strucmp(aElementName,"logininitscript")==0)
    expectScript(fLoginInitScript,aLine,getAgentFuncTableP());
  else if (strucmp(aElementName,"logincheckscript")==0)
    expectScript(fLoginCheckScript,aLine,getAgentFuncTableP());
  else if (strucmp(aElementName,"loginfinishscript")==0)
    expectScript(fLoginFinishScript,aLine,getAgentFuncTableP());
  else
  #endif // SCRIPT_SUPPORT
  #endif // BINFILE_ALWAYS_ACTIVE
  // - session level Date/Time info
  if (
    strucmp(aElementName,"timestamputc")==0 || // old 2.1 compatible
    strucmp(aElementName,"timeutc")==0 // new 3.0 variant, unified with datastore level setting
  ) {
    // - warn for usage of old timeutc
    ReportError(false,"Warning: <timestamputc>/<timeutc> is deprecated - please use <datatimezone> instead",aElementName);
    expectBool(fCurrentDateIsUTC);
  }
  else if (strucmp(aElementName,"datatimezone")==0)
    expectTimezone(fCurrentDateTimeZone);
  // - session level charset and line ends
  else if (strucmp(aElementName,"datacharset")==0)
    expectEnum(sizeof(fDataCharSet),&fDataCharSet,DBCharSetNames,numCharSets);
  else if (strucmp(aElementName,"datalineends")==0)
    expectEnum(sizeof(fDataLineEndMode),&fDataLineEndMode,lineEndModeNames,numLineEndModes);
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TCustomAgentConfig::localStartElement


// resolve
void TCustomAgentConfig::localResolve(bool aLastPass)
{
  // convert legacy UTC flag to timezone setting
  if (fCurrentDateIsUTC)
    fCurrentDateTimeZone = TCTX_UTC;
  // Scripts etc.
  if (aLastPass) {
    #ifndef BINFILE_ALWAYS_ACTIVE
    #ifdef SCRIPT_SUPPORT
    // login scripting
    TScriptContext::resolveScript(getSyncAppBase(),fLoginInitScript,fResolverContext,NULL);
    TScriptContext::resolveScript(getSyncAppBase(),fLoginCheckScript,fResolverContext,NULL);
    TScriptContext::resolveScript(getSyncAppBase(),fLoginFinishScript,fResolverContext,NULL);
    ResolveAPIScripts();
    // - derivates' scripts are resolved by now, we can dispose of the resolver context
    //   NOTE: this is true
    if (fResolverContext) delete fResolverContext;
    fResolverContext=NULL;
    #endif
    #endif // not BINFILE_ALWAYS_ACTIVE
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TCustomAgentConfig::localResolve




/* public TCustomImplAgent members */


TCustomImplAgent::TCustomImplAgent(TSyncAppBase *aAppBaseP, TSyncSessionHandle *aSessionHandleP, cAppCharP aSessionID) :
  inherited(aAppBaseP, aSessionHandleP, aSessionID),
  fConfigP(NULL)
  #ifdef SCRIPT_SUPPORT
  ,fScriptContextDatastore(NULL)
  #endif
  #ifdef SCRIPT_SUPPORT
  ,fAgentContext(NULL)
  #endif
  #ifdef DBAPI_TUNNEL_SUPPORT
  ,fTunnelDatastoreP(NULL)
  #endif
{
  // get config for agent and save direct link to agent config for easy reference
  fConfigP = static_cast<TCustomAgentConfig *>(getRootConfig()->fAgentConfigP);
  #ifndef BINFILE_ALWAYS_ACTIVE
  #ifdef SCRIPT_SUPPORT
  // create login script context if there are scripts
  // Note: derivates might already have initialized fAgentContext here, that's why we
  //       now NULL it via ctor.
  TScriptContext::rebuildContext(getSyncAppBase(),fConfigP->fLoginInitScript,fAgentContext,this);
  TScriptContext::rebuildContext(getSyncAppBase(),fConfigP->fLoginCheckScript,fAgentContext,this);
  TScriptContext::rebuildContext(getSyncAppBase(),fConfigP->fLoginFinishScript,fAgentContext,this,true); // now build vars
  // Note: derivates will rebuild NOW, AFTER our rebuilds, in the derived constructor
  #endif
  #endif // BINFILE_ALWAYS_ACTIVE
  // Note: Datastores are already created from config
} // TCustomImplAgent::TCustomImplAgent


// destructor
TCustomImplAgent::~TCustomImplAgent()
{
  // make sure everything is terminated BEFORE destruction of hierarchy begins
  TerminateSession();
} // TCustomImplAgent::~TCustomImplAgent


// Terminate session
void TCustomImplAgent::TerminateSession()
{
  if (!fTerminated) {
    // Note that the following will happen BEFORE destruction of
    // individual datastores, so make sure datastores are already
    // independent of the agnet's ressources
    InternalResetSession();
    #ifdef SCRIPT_SUPPORT
    // get rid of login context
    if (fAgentContext) delete fAgentContext;
    #endif
    // Make sure datastores know that the agent will go down soon
    announceDestruction();
  }
  inherited::TerminateSession();
} // TCustomImplAgent::TerminateSession


// Reset session
void TCustomImplAgent::InternalResetSession(void)
{
  // reset all datastores now to make everything is done which might need the
  // Agent before it is destroyed
  // (Note: TerminateDatastores() will be called again by ancestors)
  TerminateDatastores();
} // TCustomImplAgent::InternalResetSession


// Virtual version
void TCustomImplAgent::ResetSession(void)
{
  // do my own stuff
  InternalResetSession();
  // let ancestor do its stuff
  inherited::ResetSession();
} // TCustomImplAgent::ResetSession



#ifdef DBAPI_TUNNEL_SUPPORT

// initialize session for DBAPI tunnel usage
localstatus TCustomImplAgent::InitializeTunnelSession(cAppCharP aDatastoreName)
{
  localstatus sta = LOCERR_OK;

  // do the minimum of profile selection needed to make DBs work
  sta = SelectProfile(TUNNEL_PROFILE_ID, false);
  if (sta==LOCERR_OK) {
    // find datastore to work with
    TLocalDSConfig *dsCfgP = getSessionConfig()->getLocalDS(aDatastoreName);
    if (!dsCfgP) {
      // no such datastore found
      sta = DB_NotFound;
    }
    else {
      // found config for given name, instantiate the datastore object
      fTunnelDatastoreP = static_cast<TCustomImplDS *>(dsCfgP->newLocalDataStore(this));
      if (!fTunnelDatastoreP)
        sta = DB_Error;
    }
  }
  // done
  return sta;
} // TCustomImplAgent::InitializeTunnelSession


// return the datastore initialized for tunnel access
TLocalEngineDS *TCustomImplAgent::getTunnelDS()
{
  return fTunnelDatastoreP;
} // TCustomImplAgent::getTunnelDS


#endif // DBAPI_TUNNEL_SUPPORT




#ifndef BINFILE_ALWAYS_ACTIVE

// check credential string
// Note: if authentication is successful, odbcDBServer session
//       saves the user key and device key for later reference in subsequent
//       DB accesses.
bool TCustomImplAgent::SessionLogin(const char *aUserName, const char *aAuthString, TAuthSecretTypes aAuthStringType, const char *aDeviceID)
{
  #ifdef BASED_ON_BINFILE_CLIENT
  // let binfile handle it if it is active
  if (binfilesActive()) {
    return inherited::SessionLogin(aUserName, aAuthString, aAuthStringType, aDeviceID);
  }
  #endif // BASED_ON_BINFILE_CLIENT

  bool authok = false;
  string nonce;

  if (!fConfigP) return false; // no config -> fail early (no need for cleanup)

  #ifdef SYSYNC_CLIENT
  if (IS_CLIENT) {
    #ifndef NO_LOCAL_DBLOGIN
    // check for possible client without need for local DB login
    if (fNoLocalDBLogin) {
      // just use local DB login name as user key (userkey is probably not needed anyway)
      fUserKey=fLocalDBUser;
      // accept as auth ok
      return true; // return early, no need for cleanup
    }
    #else
    // client without need for local login
    return true;
    //#warning "we could probably eliminate much more code here"
    #endif
  }
  #endif // SYSYNC_CLIENT
  // first step: set defaults
  fUserKey=aUserName; // user key is equal to user name
  fDeviceKey=aDeviceID; // device key is equal to device name
  #ifndef SCRIPT_SUPPORT
  #define DB_USERNAME aUserName
  #else
  #define DB_USERNAME fUserName.c_str()
  fDomainName.erase(); // no domain name
  // second step: run script to possibly grant auth before any other method is
  // needed at all
  // - set status vars that can be referenced by context funcs in script
  fStandardAuthOK=false; // not yet authorized, for AUTHOK() func
  fAuthUser=aUserName; // for AUTHUSER() func
  fUserName=aUserName; // copy into string var that can be modified by SETUSERNAME()
  fAuthSecret=aAuthString; // for AUTHSECRET() func
  fAuthDevice=aDeviceID; // for AUTHDEVICEID() func
  fAuthType=aAuthStringType; // for AUTHTYPE() func
  fUnknowndevice=true; // we do not know the device here already
  // now call and evaluate boolean result
  TItemField *resP=NULL;
  fScriptContextDatastore=NULL;
  if (!TScriptContext::executeWithResult(
    resP, // can be default result or NULL, will contain result or NULL if no result
    fAgentContext,
    fConfigP->fLoginInitScript,
    fConfigP->getAgentFuncTableP(),  // context function table
    this, // context data (myself)
    NULL, false, NULL, false
  )) {
    authok=false; // script failed, auth failed
    goto cleanup;
  }
  else {
    if (resP) {
      // explicit auth or reject at this stage
      SYSYNC_TRY {
        // - first let device register itself (and find devicekey possibly)
        CheckDevice(aDeviceID);
        // - now get auth result
        authok = resP->getAsBoolean();
      }
      SYSYNC_CATCH(exception &e)
        // log error
        PDEBUGPRINTFX(DBG_ERROR,("Exception during CheckDevice after initscript has accepted/rejected auth: %s",e.what()));
        // fail auth
        authok=false;
      SYSYNC_ENDCATCH
      delete resP;
      goto cleanup;
    }
    // otherwise, we haven't authorized yet
    authok=false;
  }
  #endif
  SYSYNC_TRY {
    // fourth step: get device info, if any
    CheckDevice(aDeviceID);
    // fifth step: check simpleauth (in base class)
    // NOTE: overrides any API level checks if simpleauth is set!
    if (TSyncSession::SessionLogin(aUserName,aAuthString,aAuthStringType,aDeviceID)) {
      authok=true;
      goto cleanup;
    }
    // sixth step: let DB API level check authorisation
    authok=CheckLogin(aUserName, DB_USERNAME, aAuthString, aAuthStringType, aDeviceID);
    #ifdef SCRIPT_SUPPORT
    // finalize login
    // - refresh auth status
    fStandardAuthOK=authok; // for AUTHOK() func
    // - call script (if any)
    fScriptContextDatastore=NULL;
    authok=TScriptContext::executeTest(
      authok, // default for no script, no result or script error is current auth
      fAgentContext,
      fConfigP->fLoginFinishScript,
      fConfigP->getAgentFuncTableP(),  // context function table
      (void *)this // context data (myself)
    );
    #endif
  }
  SYSYNC_CATCH(exception &e)
    // log error
    PDEBUGPRINTFX(DBG_ERROR,("Exception while trying to access DB for SessionLogin: %s",e.what()));
    authok=false;
  SYSYNC_ENDCATCH
cleanup:
  // clean up login stuff
  LoginCleanUp(); // let derived class clean up (like closing transactions etc.)
  // return status
  return authok;
} // TCustomImplAgent::SessionLogin

#endif // BINFILE_ALWAYS_ACTIVE


} // namespace sysync

/* end of TCustomImplAgent implementation */

// eof
