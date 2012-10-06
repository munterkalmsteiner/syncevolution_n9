/**
 *  @File     pluginapiagent.h
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

#ifndef PLUGINAPIAGENT_H
#define PLUGINAPIAGENT_H

// includes

#ifdef SQL_SUPPORT
  #include "odbcapiagent.h"
#else
  #include "customimplagent.h"
#endif

namespace sysync {

// plugin module generic config
class TApiParamConfig: public TConfigElement
{
  typedef TConfigElement inherited;
public:
  TApiParamConfig(TConfigElement *aParentElement);
  virtual ~TApiParamConfig();
  // Assembled tag:value type config lines
  string fConfigString;
  virtual void clear();
protected:
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void localResolve(bool aLastPass);
private:
  void storeLastTag(void);
  string fLastTagName;
  string fLastTagValue;
}; // TApiParamConfig

} // namespace sysync

#include "pluginapids.h"

using namespace sysync;

namespace sysync {


// Session level callback adaptor functions
#ifdef SYDEBUG
extern "C" void SessionLogDebugPuts(void *aCallbackRef, const char *aText);
extern "C" void SessionLogDebugExotic(void *aCallbackRef, const char *aText);
extern "C" void SessionLogDebugBlock(void *aCallbackRef, const char *aTag, const char *aDesc, const char *aAttrText );
extern "C" void SessionLogDebugEndBlock(void *aCallbackRef, const char *aTag);
extern "C" void SessionLogDebugEndThread(void *aCallbackRef);
#endif // SYDEBUG
#ifdef ENGINEINTERFACE_SUPPORT
extern "C" TSyError SessionOpenSessionKey(void* aCB, SessionH aSessionH, KeyH *aKeyH, uInt16 aMode);
#endif // ENGINEINTERFACE_SUPPORT


// forward
class TPluginDSConfig;
#ifdef SCRIPT_SUPPORT
class TScriptContext;
#endif


// config
class TPluginAgentConfig :
  #ifdef SQL_SUPPORT
  public TOdbcAgentConfig
  #else
  public TCustomAgentConfig
  #endif
{
  #ifdef SQL_SUPPORT
  typedef TOdbcAgentConfig inherited;
  #else
  typedef TCustomAgentConfig inherited;
  #endif
public:
  TPluginAgentConfig(TConfigElement *aParentElement);
  virtual ~TPluginAgentConfig();
  // properties
  // - name of the login module
  string fLoginAPIModule;
  // - use Api session auth or not?
  bool fApiSessionAuth;
  // - use api device admin or not?
  bool fApiDeviceAdmin;
  // - the DB API config object
  TDB_Api_Config fDBApiConfig;
  // - generic module params
  TApiParamConfig fPluginParams;
  // - debug mask
  uInt16 fPluginDbgMask;
protected:
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void clear();
  virtual void localResolve(bool aLastPass);
public:
  #ifdef SYSYNC_CLIENT
  // create appropriate session (=agent) for this client
  virtual TSyncAgent *CreateClientSession(const char *aSessionID);
  #endif
  #ifdef SYSYNC_SERVER
  // create appropriate session (=agent) for this server
  virtual TSyncAgent *CreateServerSession(TSyncSessionHandle *aSessionHandle, const char *aSessionID);
  #endif
}; // TPluginAgentConfig


class TPluginApiAgent :
  #ifdef SQL_SUPPORT
  public TODBCApiAgent
  #else
  public TCustomImplAgent
  #endif
{
  #ifdef SQL_SUPPORT
  typedef TODBCApiAgent inherited;
  #else
  typedef TCustomImplAgent inherited;
  #endif
public:
  TPluginApiAgent(TSyncAppBase *aAppBaseP, TSyncSessionHandle *aSessionHandleP, cAppCharP aSessionID);
  virtual ~TPluginApiAgent();
  virtual void TerminateSession(void); // Terminate session, like destructor, but without actually destructing object itself
  virtual void ResetSession(void); // Resets session (but unlike TerminateSession, session might be re-used)
  void InternalResetSession(void); // static implementation for calling through virtual destructor and virtual ResetSession();
  // user authentication
  #ifdef SYSYNC_SERVER
  // - return auth type to be requested from remote
  virtual TAuthTypes requestedAuthType(void); // avoids MD5 when it cannot be checked
  // - get next nonce string top be sent to remote party for subsequent MD5 auth
  virtual void getNextNonce(const char *aDeviceID, string &aNextNonce);
  // - get nonce string, which is expected to be used by remote party for MD5 auth.
  virtual void getAuthNonce(const char *aDeviceID, string &aAuthNonce);
  #endif // SYSYNC_SERVER
  #ifndef BINFILE_ALWAYS_ACTIVE
  // - check device ID related stuff
  virtual void CheckDevice(const char *aDeviceID);
  // - remote device is analyzed, possibly save status
  virtual void remoteAnalyzed(void);
  // - check login for this session (everything else is done by CustomAgent's SessionLogin)
  virtual bool CheckLogin(const char *aOriginalUserName, const char *aModifiedUserName, const char *aAuthString, TAuthSecretTypes aAuthStringType, const char *aDeviceID);
  #endif // not BINFILE_ALWAYS_ACTIVE
  // - logout
  void LogoutApi(void);
  // current database date & time
  virtual lineartime_t getDatabaseNowAs(timecontext_t aTimecontext);
  // agent config
  TPluginAgentConfig *fPluginAgentConfigP;
  // set while Api access is locked because of a thread using it
  bool fApiLocked;
  // get API session object
  TDB_Api_Session *getDBApiSession() { return &fDBApiSession; };
protected:
  #ifdef SYSYNC_SERVER
  // - request end, used to clean up
  virtual void RequestEnded(bool &aHasData);
  #endif
private:
  TDB_Api_Session fDBApiSession;
}; // TPluginApiAgent

} // namespace sysync

#endif  // PLUGINAPIAGENT_H

// eof
