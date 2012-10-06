/**
 *  @File     pluginapiagent.cpp
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TPluginApiAgent
 *    Plugin based agent (client or server session) API implementation
 *
 *    Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-10-06 : luz : created from apidbagent
 */

// includes
#include "pluginapiagent.h"
#include "pluginapids.h"

#ifdef SYSER_REGISTRATION
#include "syserial.h"
#endif

#ifdef ENGINEINTERFACE_SUPPORT
#include "engineentry.h"
#endif

namespace sysync {


// Callback adaptor functions for session level logging
// Note: appbase level logging functions are now in syncappbase.cpp as these are
//       needed even without DB plugins


#ifdef SYDEBUG

extern "C" void SessionLogDebugPuts(void *aCallbackRef, const char *aText)
{
  if (aCallbackRef) {
    POBJDEBUGPUTSX(static_cast<TSyncSession *>(aCallbackRef),DBG_DBAPI+DBG_PLUGIN,aText);
  }
} // SessionLogDebugPuts


extern "C" void SessionLogDebugExotic(void *aCallbackRef, const char *aText)
{
  if (aCallbackRef) {
    POBJDEBUGPUTSX(static_cast<TSyncSession *>(aCallbackRef),DBG_DBAPI+DBG_PLUGIN+DBG_EXOTIC,aText);
  }
} // SessionLogDebugExotic


extern "C" void SessionLogDebugBlock(void *aCallbackRef, const char *aTag, const char *aDesc, const char *aAttrText )
{
  if (aCallbackRef) {
    bool collapsed=false;
    if (aTag && aTag[0]=='-') { aTag++; collapsed=true; }
    static_cast<TSyncSession *>(aCallbackRef)->getDbgLogger()->DebugOpenBlock(TDBG_LOCATION_NONE aTag,aDesc,collapsed,"%s",aAttrText);
  }
} // SessionLogDebugBlock


extern "C" void SessionLogDebugEndBlock(void *aCallbackRef, const char *aTag)
{
  if (aCallbackRef) {
    if (aTag && aTag[0]=='-') aTag++;
    static_cast<TSyncSession *>(aCallbackRef)->getDbgLogger()->DebugCloseBlock(TDBG_LOCATION_NONE aTag);
  }
} // SessionLogDebugEndBlock


extern "C" void SessionLogDebugEndThread(void *aCallbackRef)
{
  if (aCallbackRef) {
    static_cast<TSyncSession *>(aCallbackRef)->getDbgLogger()->DebugThreadOutputDone(false); // leave session thread record live until session dies
  }
} // SessionLogDebugEndThread

#endif


#ifdef ENGINEINTERFACE_SUPPORT

extern "C" TSyError SessionOpenSessionKey(void* aCB, SessionH aSessionH, KeyH *aKeyH, uInt16 aMode)
{
  // Note: aSessionH must be NULL, as we are implicitly in a session context and cannot specify the session
  if (!aCB || !aKeyH || aSessionH!=NULL) return LOCERR_BADPARAM;
  DB_Callback cb = static_cast<DB_Callback>(aCB);
  if (!cb->callbackRef) return LOCERR_BADPARAM;
  // create settings key for the session
  TSyncSession *sessionP = static_cast<TSyncSession *>(cb->callbackRef);
  *aKeyH = static_cast<KeyH>(sessionP->newSessionKey(sessionP->getSyncAppBase()->fEngineInterfaceP));
  // done
  return LOCERR_OK;
} // SessionOpenSessionKey

#endif // ENGINEINTERFACE_SUPPORT



// TApiParamConfig
// ===============

TApiParamConfig::TApiParamConfig(TConfigElement *aParentElement) :
  TConfigElement("plugin_config",aParentElement)
{
  // nop so far
} // TApiParamConfig::TPluginAgentConfig


TApiParamConfig::~TApiParamConfig()
{
  clear();
} // TApiParamConfig::~TApiParamConfig



// init defaults
void TApiParamConfig::clear(void)
{
  // init defaults
  fConfigString.erase();
  fLastTagName.erase();
  fLastTagValue.erase();
  // clear inherited
  inherited::clear();
} // TApiParamConfig::clear


// store last tag's value
void TApiParamConfig::storeLastTag(void)
{
  // init defaults
  if (!fLastTagName.empty()) {
    fConfigString.append(fLastTagName);
    fConfigString+=':';
    StrToCStrAppend(fLastTagValue.c_str(),fConfigString,true); // allow 8-bit chars to be represented as-is (no \xXX escape needed)
    fConfigString+="\r\n"; // CRLF at end
  }
  // done, erase them
  fLastTagName.erase();
  fLastTagValue.erase();
} // TApiParamConfig::storeLastTag



// config element parsing
bool TApiParamConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // all elements will be passed 1:1 into DBAPI
  // - make sure previous element is stored
  storeLastTag();
  // - remember name of this tag
  fLastTagName=aElementName;
  // - contents is always a string
  expectString(fLastTagValue);
  // ok
  return true;
} // TApiParamConfig::localStartElement


// resolve
void TApiParamConfig::localResolve(bool aLastPass)
{
  // make sure previous element is stored, if any
  storeLastTag();
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TApiParamConfig::localResolve


// TPluginAgentConfig
// ==================

TPluginAgentConfig::TPluginAgentConfig(TConfigElement *aParentElement) :
  inherited(aParentElement),
  fPluginParams(this)
{
  // nop so far
} // TPluginAgentConfig::TPluginAgentConfig


TPluginAgentConfig::~TPluginAgentConfig()
{
  clear();
  // - disconnect from the API module
  fDBApiConfig.Disconnect();
} // TPluginAgentConfig::~TPluginAgentConfig


// init defaults
void TPluginAgentConfig::clear(void)
{
  // init defaults
  // - API Module
  fLoginAPIModule.erase();
  // - use api session auth or not?
  fApiSessionAuth=false;
  // - use api device admin or not?
  fApiDeviceAdmin=false;
  // - default to use all debug flags set (if debug for plugin is enabled at all)
  fPluginDbgMask=0xFFFF;
  // - clear plugin params
  fPluginParams.clear();
  // clear inherited
  inherited::clear();
} // TPluginAgentConfig::clear


// config element parsing
bool TPluginAgentConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  // - API connection
  if (strucmp(aElementName,"plugin_module")==0)
    expectMacroString(fLoginAPIModule);
  else if (strucmp(aElementName,"plugin_sessionauth")==0)
    expectBool(fApiSessionAuth);
  else if (strucmp(aElementName,"plugin_deviceadmin")==0)
    expectBool(fApiDeviceAdmin);
  else if (strucmp(aElementName,"plugin_params")==0)
    expectChildParsing(fPluginParams);
  else if (strucmp(aElementName,"plugin_debugflags")==0)
    expectUInt16(fPluginDbgMask);
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TPluginAgentConfig::localStartElement


// resolve
void TPluginAgentConfig::localResolve(bool aLastPass)
{
  // resolve plugin specific config leaf
  fPluginParams.Resolve(aLastPass);
  if (aLastPass) {
    if (!fLoginAPIModule.empty()) {
      // Determine if we may use non-built-in plugins
      bool allowDLL= true; // by default, it is allowed, but if PLUGIN_DLL is not set, it will be disabled anyway.
      #if defined(SYSER_REGISTRATION) && !defined(DLL_PLUGINS_ALWAYS_ALLOWED)
      // license flags present and DLL plugins not generally allowed:
      // -> license decides if DLL is allowed
      allowDLL= (getSyncAppBase()->fRegProductFlags & SYSER_PRODFLAG_SERVER_SDKAPI)!=0;
      // warn about DLL not allowed ONLY if this build actually supports DLL plugins
      #if defined(PLUGIN_DLL)
      if (!allowDLL) {
        SYSYNC_THROW(TConfigParseException("License does not allow using <datastore type=\"plugin\">"));
      }
      #endif // DLL support available in the code at all
      #endif // DLL plugins not generally allowed (or DLL support not compiled in)
      // Module level debug goes to appbase (global) log
      DB_Callback cb= &fDBApiConfig.fCB.Callback;
      cb->callbackRef       = getSyncAppBase();
      #ifdef ENGINEINTERFACE_SUPPORT
      cb->thisBase          = getSyncAppBase()->fEngineInterfaceP;
      #endif
      #ifdef SYDEBUG
      cb->debugFlags= (getSyncAppBase()->getRootConfig()->fDebugConfig.fGlobalDebugLogs) &&
                      PDEBUGTEST(DBG_ADMIN+DBG_DBAPI+DBG_PLUGIN) ? fPluginDbgMask : 0;
      cb->DB_DebugPuts      = AppBaseLogDebugPuts;
      cb->DB_DebugBlock     = AppBaseLogDebugBlock;
      cb->DB_DebugEndBlock  = AppBaseLogDebugEndBlock;
      cb->DB_DebugEndThread = AppBaseLogDebugEndThread;
      cb->DB_DebugExotic    = AppBaseLogDebugExotic;
      #endif
      // check for required settings
      if (fDBApiConfig.Connect(fLoginAPIModule.c_str(), getSyncAppBase()->fApiInterModuleContext, "", false, allowDLL)!=LOCERR_OK)
        SYSYNC_THROW(TConfigParseException("Cannot connect to session handler module specified in <plugin_module>"));
      // now pass plugin-specific config
      if (fDBApiConfig.PluginParams(fPluginParams.fConfigString.c_str())!=LOCERR_OK)
        SYSYNC_THROW(TConfigParseException("Module does not understand params passed in <plugin_params>"));
    }
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TPluginAgentConfig::localResolve



// TPluginApiAgent
// ===============


TPluginApiAgent::TPluginApiAgent(TSyncAppBase *aAppBaseP, TSyncSessionHandle *aSessionHandleP, cAppCharP aSessionID) :
  inherited(aAppBaseP, aSessionHandleP, aSessionID),
  fPluginAgentConfigP(NULL),
  fApiLocked(false)
{
  // get config for agent and save direct link to agent config for easy reference
  fPluginAgentConfigP = static_cast<TPluginAgentConfig *>(getRootConfig()->fAgentConfigP);
  // Note: Datastores are already created from config
  if (fPluginAgentConfigP->fDBApiConfig.Connected()) {
    TSyError dberr;
    DB_Callback cb= &fDBApiSession.fCB.Callback;
    cb->callbackRef      = this; // the session
    #ifdef ENGINEINTERFACE_SUPPORT
    cb->thisBase          = getSyncAppBase()->fEngineInterfaceP;
    #endif
    #ifdef SYDEBUG
    // Agent Context level debug goes to session log
    cb->debugFlags       = getDbgMask() ? 0xFFFF : 0;
    cb->DB_DebugPuts     = SessionLogDebugPuts;
    cb->DB_DebugBlock    = SessionLogDebugBlock;
    cb->DB_DebugEndBlock = SessionLogDebugEndBlock;
    cb->DB_DebugEndThread= SessionLogDebugEndThread;
    cb->DB_DebugExotic   = SessionLogDebugExotic;
    #endif // SYDEBUG
    #ifdef ENGINEINTERFACE_SUPPORT
    // Data module can use Get/SetValue for "AsKey" routines and for session script var access
    // Note: these are essentially context free and work without a global call-in structure
    //       (which is not necessarily there, for example in no-library case)
    CB_Connect_KeyAccess(cb); // connect generic key access routines
    // Version of OpenSessionKey that implicitly opens a key for the current session (DB plugins
    // do not have a session handle, as their use is always implicitly in a session context).
    cb->ui.OpenSessionKey = SessionOpenSessionKey;
    #endif // ENGINEINTERFACE_SUPPORT
    // Now create the context for running this session
    // - use session ID as session name
    dberr=fDBApiSession.CreateContext(getLocalSessionID(),fPluginAgentConfigP->fDBApiConfig);
    if (dberr!=LOCERR_OK)
      SYSYNC_THROW(TSyncException("Error creating context for API session module",dberr));
  }
} // TPluginApiAgent::TPluginApiAgent


TPluginApiAgent::~TPluginApiAgent()
{
  // make sure everything is terminated BEFORE destruction of hierarchy begins
  TerminateSession();
} // TPluginApiAgent::~TPluginApiAgent


// Terminate session
void TPluginApiAgent::TerminateSession()
{
  if (!fTerminated) {
    // Make sure datastores know that the agent will go down soon (and give them opportunity to disconnect contexts)
    announceDestruction();
    // Note that the following will happen BEFORE destruction of
    // individual datastores, so make sure datastore have their inherited stuff (e.g. ODBC)
    // stuff finished before disposing environment
    InternalResetSession();
    // now we can destroy the session level API context (no connected DB context exist any more)
    fDBApiSession.DeleteContext();
  }
  inherited::TerminateSession();
} // TPluginApiAgent::TerminateSession



// Reset session
void TPluginApiAgent::InternalResetSession(void)
{
  // reset all datastores now to make sure all Plugin access is over (including log writing)
  // before the Plugin Agent is destroyed.
  // (Note: TerminateDatastores() will be called again by ancestors)
  TerminateDatastores();
  // logout api
  LogoutApi();
} // TPluginApiAgent::InternalResetSession


// Virtual version
void TPluginApiAgent::ResetSession(void)
{
  // do my own stuff
  InternalResetSession();
  // let ancestor do its stuff
  TStdLogicAgent::ResetSession();
} // TPluginApiAgent::ResetSession


// get time of DB server
lineartime_t TPluginApiAgent::getDatabaseNowAs(timecontext_t aTimecontext)
{
  #ifndef SDK_ONLY_SUPPORT
  // only handle here if we are in charge - otherwise let ancestor handle it
  if (!fPluginAgentConfigP->fApiDeviceAdmin) return inherited::getDatabaseNowAs(aTimecontext);
  #endif

  lineartime_t dbtime;

  #if defined RELEASE_VERSION && !defined _MSC_VER
    //#warning "GZones should not be passed NULL here"
  #endif
  if (fDBApiSession.GetDBTime( dbtime, NULL )!=LOCERR_OK) {
    dbtime=getSystemNowAs(TCTX_UTC); // just use time of this machine
  }
  // return it
  return dbtime;
} // TPluginApiAgent::getDatabaseNowAs



#ifdef SYSYNC_SERVER

// info about requested auth type
TAuthTypes TPluginApiAgent::requestedAuthType(void)
{
  // if not checking via api, let inherited decide
  if (!fPluginAgentConfigP->fApiSessionAuth) return inherited::requestedAuthType();
  // get default
  TAuthTypes auth = TSyncAgent::requestedAuthType();
  // depending on possibilities of used module, we need to enforce basic
  int pwmode=fDBApiSession.PasswordMode();
  if (pwmode==Password_ClrText_IN || (pwmode==Password_MD5_OUT && getSyncMLVersion()<syncml_vers_1_1)) {
    // we need clear text password to login, so we can only do basic auth
    auth=auth_basic; // force basic auth, this can be checked
  }
  // return
  return auth;
} // TPluginApiAgent::requestedAuthType


// - get nonce string, which is expected to be used by remote party for MD5 auth.
void TPluginApiAgent::getAuthNonce(const char *aDeviceID, string &aAuthNonce)
{
  if (!fPluginAgentConfigP->fApiDeviceAdmin) {
    // no device ID or no persistent nonce, use method of ancestor
    inherited::getAuthNonce(aDeviceID,aAuthNonce);
  }
  else {
    // we have a persistent nonce
    DEBUGPRINTFX(DBG_ADMIN,("getAuthNonce: current auth nonce='%s'",fLastNonce.c_str()));
    aAuthNonce=fLastNonce.c_str();
  }
} // TPluginApiAgent::getAuthNonce


// get next nonce (to be sent to remote party for next auth of that remote party with us)
// Must generate and persistently save a new nonce string which will be sent in
// this session to the remote, and which will be used in the next session for
// authenticating the remote device.
void TPluginApiAgent::getNextNonce(const char *aDeviceID, string &aNextNonce)
{
  if (!fPluginAgentConfigP->fApiDeviceAdmin) {
    // no persistent nonce, use method of ancestor
    inherited::getNextNonce(aDeviceID,aNextNonce);
  }
  else {
    // plugin is responsible for storing a persistent nonce per device
    // - check if plugin wants to generate the nonce itself
    TDB_Api_Str apiNonce;
    if (fDBApiSession.GetNonce(apiNonce)==LOCERR_OK) {
      // plugin has generated a nonce, use it
      aNextNonce = apiNonce.c_str();
    }
    else {
      // plugin does not generate nonce itself, use our own generator
      // - create new one (pure 7bit ASCII)
      generateNonce(aNextNonce,aDeviceID,getSystemNowAs(TCTX_UTC)+rand());
    }
    // - save it such that next CheckDevice() can retrieve it as fLastNonce (see below)
    TSyError sta=fDBApiSession.SaveNonce(aNextNonce.c_str());
    if (sta!=LOCERR_OK) {
      PDEBUGPRINTFX(DBG_ERROR,("getNextNonce: SaveNonce failed with error = %hd",sta));
    }
  }
} // TPluginApiAgent::getNextNonce

#endif // SYSYNC_CLIENT


#ifndef BINFILE_ALWAYS_ACTIVE


// check device related stuff
// - Must retrieve device record for device identified by aDeviceID.
//   - If no such record exists, create a new one and set:
//     - fLastNonce to empty string
//   - If the record exists, the following fields must be retrieved from the DB:
//     - fLastNonce : opaque string used for MD5 auth
//   - In any case, the routine should set:
//     - fDeviceKey : identifier that can be used in other calls (like LoadAdminData/SaveAdminData
//       or dsLogSyncResult) to refer to this device
void TPluginApiAgent::CheckDevice(const char *aDeviceID)
{
  // do it here only if we have device info stuff implemented in ApiDB
  if (!fPluginAgentConfigP->fApiDeviceAdmin) {
    // let inherited process it
    inherited::CheckDevice(aDeviceID);
  }
  else {
    // Plugin will do the device checking and has stored the nonce generated in the previous session
    TDB_Api_Str lastNonce;
    TDB_Api_Str deviceKey;
    TSyError sta = fDBApiSession.CheckDevice(aDeviceID,deviceKey,lastNonce);
    if (sta!=LOCERR_OK)
      SYSYNC_THROW(TSyncException("TPluginApiAgent: checkdevice failed",sta));
    #ifdef SYSYNC_SERVER
    if (IS_SERVER) {
      // save last nonce
      fLastNonce = lastNonce.c_str();
    }
    #endif // SYSYNC_SERVER
    // device "key" is set to deviceID. Plugin internally maintains its key needed for storing Nonce or DevInf
    fDeviceKey = deviceKey.c_str();
  } // if device table implemented in ApiDB
} // TPluginApiAgent::CheckDevice


// Device is now known as detailed as possible (i.e. we have the device information, if it was possible to get it)
// The implementation can now save some of this information for logging and tracking purposes:
// - getRemoteURI()         : string, remote Device URI (usually IMEI or other globally unique ID)
// - getRemoteDescName()    : string, remote Device's descriptive name (constructed from DevInf <man> and <mod>, or
//                                       as set by <remoterule>'s <descriptivename>.
// - getRemoteInfoString()  : string, Remote Device Version Info ("Type (HWV, FWV, SWV) Oem")
// - fUserKey               : string, identifies user
// - fDeviceKey             : string, identifies device
// - fDomainName            : string, identifies domain (aka clientID, aka enterpriseID)
// - fRemoteDevInf_mod      : string, model name from remote devinf
// - fRemoteDevInf_man      : string, manufacturer name from remote devinf
// - fRemoteDevInf_oem      : string, OEM name from remote devinf
// - fRemoteDevInf_fwv      : string, firmware version from remote devinf
// - fRemoteDevInf_swv      : string, software version from remote devinf
// - fRemoteDevInf_hwv      : string, hardware version from remote devinf
// fDeviceKey is normally used to re-identify the record in the database that was found or created at CheckDevice.
void TPluginApiAgent::remoteAnalyzed(void)
{
  // save basic device info used for associating targets and logentries with a device
  if (fPluginAgentConfigP->fApiDeviceAdmin) {
    // save device info stuff
    string devInf,val;
    devInf.erase();
    // save as properties
    // - internals
    devInf += "REMOTE_URI:";
    storeUTF8ToString(getRemoteURI(),val,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode);
    StrToCStrAppend(val.c_str(),devInf,true); // allow 8-bit chars to be represented as-is (no \xXX escape needed)
    devInf += "\r\nREMOTE_DESC:";
    storeUTF8ToString(getRemoteDescName(),val,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode);
    StrToCStrAppend(val.c_str(),devInf,true); // allow 8-bit chars to be represented as-is (no \xXX escape needed)
    devInf += "\r\nREMOTE_INFO:";
    storeUTF8ToString(getRemoteInfoString(),val,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode);
    StrToCStrAppend(val.c_str(),devInf,true); // allow 8-bit chars to be represented as-is (no \xXX escape needed)
    #ifdef SCRIPT_SUPPORT
    devInf += "\r\nDOMAIN:";
    storeUTF8ToString(fDomainName.c_str(),val,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode);
    StrToCStrAppend(val.c_str(),devInf,true); // allow 8-bit chars to be represented as-is (no \xXX escape needed)
    #endif
    // - strictly from DevInf
    devInf += "\r\nMOD:";
    storeUTF8ToString(fRemoteDevInf_mod.c_str(),val,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode);
    StrToCStrAppend(val.c_str(),devInf,true); // allow 8-bit chars to be represented as-is (no \xXX escape needed)
    devInf += "\r\nMAN:";
    storeUTF8ToString(fRemoteDevInf_man.c_str(),val,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode);
    StrToCStrAppend(val.c_str(),devInf,true); // allow 8-bit chars to be represented as-is (no \xXX escape needed)
    devInf += "\r\nOEM:";
    storeUTF8ToString(fRemoteDevInf_oem.c_str(),val,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode);
    StrToCStrAppend(val.c_str(),devInf,true); // allow 8-bit chars to be represented as-is (no \xXX escape needed)
    devInf += "\r\nFWV:";
    storeUTF8ToString(fRemoteDevInf_fwv.c_str(),val,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode);
    StrToCStrAppend(val.c_str(),devInf,true); // allow 8-bit chars to be represented as-is (no \xXX escape needed)
    devInf += "\r\nSWV:";
    storeUTF8ToString(fRemoteDevInf_swv.c_str(),val,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode);
    StrToCStrAppend(val.c_str(),devInf,true); // allow 8-bit chars to be represented as-is (no \xXX escape needed)
    devInf += "\r\nHWV:";
    storeUTF8ToString(fRemoteDevInf_hwv.c_str(),val,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode);
    StrToCStrAppend(val.c_str(),devInf,true); // allow 8-bit chars to be represented as-is (no \xXX escape needed)
    devInf += "\r\n"; // last line end
    // now store
    TSyError sta = fDBApiSession.SaveDeviceInfo(devInf.c_str());
    if (sta!=LOCERR_OK)
      SYSYNC_THROW(TSyncException("TPluginApiAgent: SaveDeviceInfo failed",sta));
  } // if device table implemented in ApiDB
  else {
    // let inherited process it
    inherited::remoteAnalyzed();
  }
} // TPluginApiAgent::remoteAnalyzed



// check login information
bool TPluginApiAgent::CheckLogin(const char *aOriginalUserName, const char *aModifiedUserName, const char *aAuthString, TAuthSecretTypes aAuthStringType, const char *aDeviceID)
{
  bool authok=false;
  string nonce;

  // - get nonce (if we have a device table, we should have read it by now)
  if (aAuthStringType==sectyp_md5_V10 || aAuthStringType==sectyp_md5_V11)
    getAuthNonce(aDeviceID,nonce);

  if (!fPluginAgentConfigP->fDBApiConfig.Connected() || !fPluginAgentConfigP->fApiSessionAuth) {
    // let inherited handle it (if direct ancestor is TCustomImplAgent, we'll fail here)
    return inherited::CheckLogin(aOriginalUserName, aModifiedUserName, aAuthString, aAuthStringType, aDeviceID);
  }
  // api auth required
  TDB_Api_Str userKey;
  TDB_Api_Str dbSecret;
  int pwmode = fDBApiSession.PasswordMode();
  // check anonymous login
  if (aAuthStringType==sectyp_anonymous) {
    if (pwmode==Password_ClrText_IN || pwmode==Password_MD5_Nonce_IN) {
      // modes that pass in credential string - pass empty credential string to signal "anonymous"
      authok = fDBApiSession.Login(aModifiedUserName,"",userKey)==LOCERR_OK;
    }
    else {
      // modes that get credential string from API - check that API returns empty string to signal "anonymous" login is ok
      authok = fDBApiSession.Login(aModifiedUserName,dbSecret,userKey)==LOCERR_OK;
      authok = authok && dbSecret.empty(); // returned secret must be empty to allow anonymous login
    }
  }
  // check if we can auth at all
  else if (pwmode == Password_ClrText_IN) {
    // we can auth only if we have a cleartext password from the remote
    if (aAuthStringType!=sectyp_clearpass) return false; // auth not possible
    // login with clear text password
    authok = fDBApiSession.Login(aModifiedUserName,aAuthString,userKey)==LOCERR_OK;
  }
  else if (pwmode == Password_MD5_Nonce_IN) {
    if (aAuthStringType==sectyp_clearpass) {
        std::string p1, p2, p3;
        if (nonce == "") getAuthNonce(aDeviceID, nonce);
        p1 = aModifiedUserName;
        p1 += ":";
        p1 += aAuthString;
        MD5B64(p1.c_str(), p1.length(), p2);
        p2 += ":";
        p2 += nonce;
        MD5B64(p2.c_str(), p2.length(), p3);
        authok = fDBApiSession.Login(aModifiedUserName,p3.c_str(), userKey)==LOCERR_OK;
    } else {
    // login with MD5( MD5( user:pwd ):nonce )
    authok = fDBApiSession.Login(aModifiedUserName,aAuthString,userKey)==LOCERR_OK;
    }
  }
  else {
    if (pwmode == Password_MD5_OUT) {
      // if the database has MD5, we can auth basic and SyncML 1.1 MD5, but not SyncML 1.0 MD5
      if (aAuthStringType==sectyp_md5_V10) return false; // auth with SyncML 1.0 MD5 not possible
    }
    // get Secret and preliminary authok (w/o password check yet) from database
    authok = fDBApiSession.Login(aModifiedUserName,dbSecret,userKey)==LOCERR_OK;
    if (authok) {
      // check secret
      if (pwmode == Password_ClrText_OUT) {
        // we have the clear text password, check against what was transmitted from remote
        authok = checkAuthPlain(aOriginalUserName,dbSecret.c_str(),nonce.c_str(),aAuthString,aAuthStringType);
      }
      else {
        // whe have the MD5 of user:password, check against what was transmitted from remote
        // Note: this can't work with non-V1.1 type creds
        authok = checkAuthMD5(aOriginalUserName,dbSecret.c_str(),nonce.c_str(),aAuthString,aAuthStringType);
      }
    }
  }
  if (!authok) {
    // failed
    PDEBUGPRINTFX(DBG_ADMIN,(
      "==== api authentication for '%s' failed",
      aModifiedUserName
    ));
    return false;
  }
  else {
    // save user key as returned from dbapi login()
    fUserKey = userKey.c_str();
    // authenticated successfully, sessionID is set
    PDEBUGPRINTFX(DBG_ADMIN,(
      "==== api authentication for '%s' (userkey='%s') successful",
      aModifiedUserName,
      fUserKey.c_str()
    ));
    return true;
  }
} // TPluginApiAgent::CheckLogin

#endif // not BINFILE_ALWAYS_ACTIVE


// - logout api
void TPluginApiAgent::LogoutApi(void)
{
  if (fPluginAgentConfigP->fDBApiConfig.Connected() && fPluginAgentConfigP->fApiSessionAuth) {
    // log out from session
    fDBApiSession.Logout();
  }
} // TPluginApiAgent::LogoutApi





#ifdef SYSYNC_SERVER

void TPluginApiAgent::RequestEnded(bool &aHasData)
{
  // first let ancestors finish their stuff
  // - this will include calling RequestEnded() in all datastores
  inherited::RequestEnded(aHasData);

  // let my API know as well, thread might change for next request (but not necessarily does!)
  if (fPluginAgentConfigP->fDBApiConfig.Connected()) {
    if (!fApiLocked)
      fDBApiSession.ThreadMayChangeNow();
  }

} // TPluginApiAgent::RequestEnded

#endif


// factory methods of Agent config
// ===============================

#ifdef SYSYNC_CLIENT

TSyncAgent *TPluginAgentConfig::CreateClientSession(const char *aSessionID)
{
  // return appropriate client session
  MP_RETURN_NEW(TPluginApiAgent,DBG_HOT,"TPluginApiAgent",TPluginApiAgent(getSyncAppBase(),NULL,aSessionID));
} // TPluginAgentConfig::CreateClientSession

#endif

#ifdef SYSYNC_SERVER

TSyncAgent *TPluginAgentConfig::CreateServerSession(TSyncSessionHandle *aSessionHandle, const char *aSessionID)
{
  // return XML2GO or ODBC-server session
  MP_RETURN_NEW(TPluginApiAgent,DBG_HOT,"TPluginApiAgent",TPluginApiAgent(getSyncAppBase(),aSessionHandle,aSessionID));
} // TPluginAgentConfig::CreateServerSession

#endif

} // namespace sysync

/* end of TPluginApiAgent implementation */

// eof
