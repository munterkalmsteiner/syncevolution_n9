/*
 *  TEngineSessionDispatch
 *    Server library specific descendant of TSyncSessionDispatch
 *
 *  Copyright (c) 2009-2011 by Synthesis AG + plan44.ch
 *
 *  2009-02-06 : luz : Created
 *
 */


#include "prefix_file.h"
#include "engine_server.h"
#include "enginesessiondispatch.h"


namespace sysync {


// write to platform's "console", whatever that is
void AppConsolePuts(const char *aText)
{
  // Just print to platform's console
  PlatformConsolePuts(aText);
} // AppConsolePuts


// TEngineServerCommConfig
// =======================


// config constructor
TEngineServerCommConfig::TEngineServerCommConfig(TConfigElement *aParentElementP) :
  TCommConfig("engineserver",aParentElementP)
{
  // do not call clear(), because this is virtual!
} // TEngineServerCommConfig::TEngineServerCommConfig


// config destructor
TEngineServerCommConfig::~TEngineServerCommConfig()
{
  // nop by now
} // TEngineServerCommConfig::~TEngineServerCommConfig


// init defaults
void TEngineServerCommConfig::clear(void)
{
  // init defaults
  fSessionIDCGIPrefix = "sessionid=";
  fSessionIDCGI = true;
  fBuffersRetryAnswer = false; // we don't know if the app driving the engine implements this, so default is off
  // clear inherited
  inherited::clear();
} // TEngineServerCommConfig::clear


#ifndef HARDCODED_CONFIG

// XPT transport config element parsing
bool TEngineServerCommConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"buffersretryanswer")==0)
    expectBool(fBuffersRetryAnswer);
  if (strucmp(aElementName,"sessionidcgiprefix")==0)
    expectString(fSessionIDCGIPrefix);
  if (strucmp(aElementName,"sessionidcgi")==0)
    expectBool(fSessionIDCGI);
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TEngineServerCommConfig::localStartElement

#endif


// resolve
void TEngineServerCommConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    // check for required settings
    // NOP for now
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TEngineServerCommConfig::localResolve



// TEngineSessionDispatch
// ======================


// constructor
TEngineSessionDispatch::TEngineSessionDispatch() :
  TSyncAppBase()
{
  // this is a server engine
  fIsServer = true;
} // TEngineSessionDispatch::TEngineSessionDispatch


// destructor
TEngineSessionDispatch::~TEngineSessionDispatch()
{
  fDeleting=true; // flag deletion to block calling critical (virtual) methods
  // clean up
  // %%%
} // TEngineSessionDispatch::~TEngineSessionDispatch


// Called from SyncML toolkit when a new SyncML message arrives
// - dispatches to session's StartMessage
Ret_t TEngineSessionDispatch::StartMessage(
  InstanceID_t aSmlWorkspaceID, // SyncML toolkit workspace instance ID
  VoidPtr_t aUserData, // pointer to a TSyncAgent descendant
  SmlSyncHdrPtr_t aContentP // SyncML tookit's decoded form of the <SyncHdr> element
) {
  TSyncSession *sessionP = static_cast<TSyncAgent *>(aUserData); // the server session
  SYSYNC_TRY {
    // let session handle details of StartMessage callback
    return sessionP->StartMessage(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException(sessionP,"StartMessage",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException(sessionP,"StartMessage",NULL);
  SYSYNC_ENDCATCH
} // TEngineSessionDispatch::StartMessage




// Test if message buffering is available
bool TEngineSessionDispatch::canBufferRetryAnswer(void)
{
  // basically, we can buffer, we do it if configured
  return dynamic_cast<TEngineServerCommConfig *>(getRootConfig()->fCommConfigP)->fBuffersRetryAnswer;
} // TEngineSessionDispatch::canBufferRetryAnswer



// Combine URI and session ID to make a RespURI according to transport
void TEngineSessionDispatch::generateRespURI(
  string &aRespURI,
  cAppCharP aLocalURI,
  cAppCharP aSessionID
)
{
  TEngineServerCommConfig *commCfgP = static_cast<TEngineServerCommConfig *>(getRootConfig()->fCommConfigP);
  if (aLocalURI && aSessionID && commCfgP && commCfgP->fSessionIDCGI) {
    // include session ID as CGI into RespURI
    aRespURI=aLocalURI;
    // see if there is already a sessionid in this localURI
    string::size_type n=aRespURI.find(commCfgP->fSessionIDCGIPrefix);
    if (n!=string::npos) {
      n+=commCfgP->fSessionIDCGIPrefix.size(); // char after prefix
      // is already there, replace value with new value
      string::size_type m=aRespURI.find_first_of("&?\n\r",n);
      if (m==string::npos)
        aRespURI.replace(n,999,aSessionID);
      else
        aRespURI.replace(n,m-n,aSessionID);
    }
    else {
      // no sessionID yet
      if (strchr(aLocalURI,'?')) {
        // already has CGI param
        aRespURI+="&amp;";
      }
      else {
        // is first CGI param
        aRespURI+='?';
      }
      // append session ID as CGI parameter
      aRespURI+=commCfgP->fSessionIDCGIPrefix;
      aRespURI+=aSessionID;
    }
  }
} // TEngineSessionDispatch::generateRespURI



// Handle exception happening while decoding commands for a session
Ret_t TEngineSessionDispatch::HandleDecodingException(TSyncSession *aSessionP, const char *aRoutine, exception *aExceptionP)
{
  #ifdef SYDEBUG
  // determine session name
  const char *sname = "<unknown>";
  SYSYNC_TRY {
    if (aSessionP) {
      sname = aSessionP->getLocalSessionID();
    }
  }
  SYSYNC_CATCH (...)
    sname = "<BAD aSessionP, caused exception>";
    aSessionP=NULL; // prevent attempt to write to session's log
  SYSYNC_ENDCATCH
  // determine routine name
  if (!aRoutine) aRoutine="<unspecified routine>";
  // show details
  if (aExceptionP) {
    // known exception
    // - show it in global log
    PDEBUGPRINTFX(DBG_ERROR,(
      "******** Exception in %s, sessionID=%s: %s",
      aRoutine,
      sname,
      aExceptionP->what()
    ));
    // - and also in session log
    #ifdef SYDEBUG
    if (aSessionP) {
      POBJDEBUGPRINTFX(aSessionP,DBG_ERROR,(
        "******** Warning: Exception in %s: %s",
        aRoutine,
        aExceptionP->what()
      ));
    }
    #endif
  }
  else {
    // unknown exception
    // - show it in global log
    PDEBUGPRINTFX(DBG_ERROR,(
      "******** Unknown Exception in %s, sessionID=%s",
      aRoutine,
      sname
    ));
    // - and also in session log
    #ifdef SYDEBUG
    if (aSessionP) {
      POBJDEBUGPRINTFX(aSessionP,DBG_ERROR,(
        "******** Warning: Unknown Exception in %s",
        aRoutine
      ));
    }
    #endif
  }
  #endif
  // try to kill session
  DEBUGPRINTFX(DBG_SESSION,("******** Exception aborts session"));
  aSessionP->AbortSession(412,true); // incomplete command
  // return error
  DEBUGPRINTFX(DBG_SESSION,("******** Exception: returning SML_ERR_UNSPECIFIC to abort smlProcessData"));
  return SML_ERR_UNSPECIFIC;
} // TEngineSessionDispatch::HandleDecodingException



// factory methods of Rootconfig
// =============================


// create default transport config
void TEngineServerRootConfig::installCommConfig(void)
{
  // engine API needs no config at this time, commconfig is a NOP dummy for now
  fCommConfigP=new TEngineServerCommConfig(this);
} // TEngineServerRootConfig::installCommConfig


#ifndef HARDCODED_CONFIG

bool TEngineServerRootConfig::parseCommConfig(const char **aAttributes, sInt32 aLine)
{
  // engine API needs no config at this time
  return false;
} // TEngineServerRootConfig::parseCommConfig

#endif



// TEngineServerSessionHandle
// ==========================

// Note: this is not a relative of TSyncSessionHandle, but only a container for TSyncAgent also
//       holding some engine-related status. TSyncAgent is run with a NULL TSyncSessionHandle
//       when called via engine, as all session dispatching is outside the engine.

TEngineServerSessionHandle::TEngineServerSessionHandle(TServerEngineInterface *aServerEngineInterface)
{
  fServerSessionP = NULL;
  fSmlInstanceID = 0;
  fServerSessionStatus = LOCERR_WRONGUSAGE;
  fServerEngineInterface = aServerEngineInterface;
}

TEngineServerSessionHandle::~TEngineServerSessionHandle()
{
  // remove the session if still existing
  if (fServerSessionP) delete fServerSessionP;
  fServerSessionP = NULL;
  // also release the toolkit instance
  fServerEngineInterface->getSyncAppBase()->freeSmlInstance(fSmlInstanceID);
  fSmlInstanceID=NULL;
}


// TServerEngineInterface
// ======================

/// @brief Open a session
/// @param aNewSessionH[out] receives session handle for all session execution calls
/// @param aSelector[in] selector, depending on session type.
/// @param aSessionName[in] a text name/id to identify the session. If NULL, session gets a standard ID based on time and memory location
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TServerEngineInterface::OpenSessionInternal(SessionH &aNewSessionH, uInt32 aSelector, cAppCharP aSessionName)
{
  TEngineSessionDispatch *sessionDispatchP = static_cast<TEngineSessionDispatch *>(getSyncAppBase());

  // check type of session
  if (aSelector == SESSIONSEL_DBAPI_TUNNEL) {
    // initiate a DBAPI tunnel session.
    /*
    #ifdef DBAPI_TUNNEL_SUPPORT
    #error "%%% tbi"
    // Create a new session, sessionName selects datastore
    sessionHandleP->fServerSessionStatus = sessionDispatchP->CreateTunnelSession(aSessionName);
    if (sessionHandleP->fServerSessionStatus==LOCERR_OK) {
      // return the session pointer as handle
      %%%aNewSessionH=clientBaseP->fClientSessionP;
    }
    #else
    return LOCERR_NOTIMP; // tunnel not implemented
    #endif
    */
    // %%% for now: not implemented
    return LOCERR_NOTIMP; // tunnel not implemented
  }
  else {
    // create a new server session
    TEngineServerSessionHandle *sessionHandleP = NULL;
    TSyncAgent *sessionP=NULL;
    SYSYNC_TRY {
      // - create a handle
      sessionHandleP = new TEngineServerSessionHandle(this);
      // - create session ID if none passed
      string SessionIDString;
      if (aSessionName && *aSessionName) {
        SessionIDString = aSessionName;
      }
      else {
        // - create unique server-side session ID
        //   format = aaaabbbbccccdddd
        //   - aaaa = low word of time(NULL) >> 1 (to make sure MSB is cleared)
        //   - dddd = high word of time(NULL)
        //   - bbbbcccc = memory address of session handle
        uInt64 sid =
          time(NULL);
        sid =
          ((sid >> 16) & 0xFFFF) + ((sid << 47) & 0x7FFF000000000000LL) + // aaaa00000000dddd
          ((((uIntPtr)sessionHandleP)&0xFFFFFFFF) << 16); // 0000bbbbcccc0000
        // - make a string of it
        StringObjPrintf(SessionIDString,"%llu",(long long unsigned)sid);
      }
      // - create session object
      sessionP =
        static_cast<TAgentConfig *>(sessionDispatchP->getRootConfig()->fAgentConfigP)
          ->CreateServerSession(NULL,SessionIDString.c_str());
      if (sessionP) {
        // assign to handle
        sessionHandleP->fServerSessionP = sessionP;
        sessionHandleP->fServerSessionStatus = LOCERR_OK;
        // also create a toolkit instance for the session (so we can start receiving data)
        if (!getSyncAppBase()->newSmlInstance(
          SML_XML,
          sessionDispatchP->getRootConfig()->fLocalMaxMsgSize * 2, // twice the message size
          sessionHandleP->fSmlInstanceID
        )) {
          // failed creating instance (must be memory problem)
          delete sessionP;
          return LOCERR_OUTOFMEM;
        }
        // link session with toolkit instance back and forth
        getSyncAppBase()->setSmlInstanceUserData(sessionHandleP->fSmlInstanceID,sessionP); // toolkit must know session (as userData)
        sessionP->setSmlWorkspaceID(sessionHandleP->fSmlInstanceID); // session must know toolkit workspace
        // created session ok
        aNewSessionH = (SessionH)sessionHandleP;
        return LOCERR_OK;
      }
    }
    SYSYNC_CATCH (...)
      // error creating session
      if (sessionHandleP) delete sessionHandleP;
      return LOCERR_EXCEPTION;
    SYSYNC_ENDCATCH
  }
  return LOCERR_WRONGUSAGE;
}


/// @brief open session specific runtime parameter/settings key
/// @note key handle obtained with this call must be closed BEFORE SESSION IS CLOSED!
/// @param aNewKeyH[out] receives the opened key's handle on success
/// @param aSessionH[in] session handle obtained with OpenSession
/// @param aMode[in] the open mode
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TServerEngineInterface::OpenSessionKey(SessionH aSessionH, KeyH &aNewKeyH, uInt16 aMode)
{
  if (!aSessionH) return LOCERR_WRONGUSAGE;
  TEngineServerSessionHandle *sessionHandleP = reinterpret_cast<TEngineServerSessionHandle *>(aSessionH);
  // create settings key for the session
  aNewKeyH = (KeyH)sessionHandleP->fServerSessionP->newSessionKey(this);
  // done
  return LOCERR_OK;
}


/// @brief Close a session
/// @note  terminates and destroys the session (if not already terminated)
/// @param aSessionH[in] session handle obtained with OpenSession
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TServerEngineInterface::CloseSession(SessionH aSessionH)
{
  if (!aSessionH) return LOCERR_WRONGUSAGE;
  TSyError sta = LOCERR_OK;
  TEngineServerSessionHandle *sessionHandleP = reinterpret_cast<TEngineServerSessionHandle *>(aSessionH);
  TSyncAgent *serverSessionP = sessionHandleP->fServerSessionP;
  if (serverSessionP) {
    // session still exists
    if (!serverSessionP->isAborted()) {
      // if not already aborted otherwise (e.g. by writing "abortstatus" in session key), let it be "timeout"
      serverSessionP->AbortSession(408, true);
    }
    SYSYNC_TRY {
      // - terminate (might hang a while until subthreads properly terminate)
      serverSessionP->TerminateSession();
      // - delete
      sessionHandleP->fServerSessionP = NULL; // consider deleted, whatever happens
      delete serverSessionP; // might hang until subthreads have terminated
    }
    SYSYNC_CATCH(...)
      sessionHandleP->fServerSessionP = NULL; // consider deleted, even if failed
      sta = LOCERR_EXCEPTION;
    SYSYNC_ENDCATCH
  }
  // forget session handle (and toolkit instance)
  delete sessionHandleP;
  // done
  return LOCERR_OK;
}


/// @brief Executes sync session or other sync related activity step by step
/// @param aSessionH[in] session handle obtained with OpenSession
/// @param aStepCmd[in/out] step command (STEPCMD_xxx):
///        - tells caller to send or receive data or end the session etc.
///        - instructs engine to suspend or abort the session etc.
/// @param aInfoP[in] pointer to a TEngineProgressInfo structure, NULL if no progress info needed
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TServerEngineInterface::SessionStep(SessionH aSessionH, uInt16 &aStepCmd,  TEngineProgressInfo *aInfoP)
{
  if (!aSessionH) return LOCERR_WRONGUSAGE;
  TEngineServerSessionHandle *sessionHandleP = reinterpret_cast<TEngineServerSessionHandle *>(aSessionH);
  TSyncAgent *serverSessionP = sessionHandleP->fServerSessionP;

  // preprocess general step codes
  switch (aStepCmd) {
    case STEPCMD_TRANSPFAIL :
      // directly abort
      serverSessionP->AbortSession(LOCERR_TRANSPFAIL,true);
      goto abort;
    case STEPCMD_TIMEOUT :
      // directly abort
      serverSessionP->AbortSession(408,true);
    abort:
      aStepCmd = STEPCMD_STEP; // convert to normal step
      break;
  }
  // let server session handle it
  sessionHandleP->fServerSessionStatus = serverSessionP->SessionStep(aStepCmd, aInfoP);
  // return step status
  return sessionHandleP->fServerSessionStatus;
} // TServerEngineInterface::SessionStep


/// @brief returns the SML instance for a given session handle
///   (internal helper to allow TEngineInterface to provide the access to the SyncML buffer)
InstanceID_t TServerEngineInterface::getSmlInstanceOfSession(SessionH aSessionH)
{
  if (!aSessionH) return 0; // something wrong with session handle -> no SML instance
  TEngineServerSessionHandle *sessionHandleP = reinterpret_cast<TEngineServerSessionHandle *>(aSessionH);
  TSyncAgent *serverSessionP = sessionHandleP->fServerSessionP;
  if (!serverSessionP) return 0; // something wrong with session handle -> no SML instance
  // return SML instance associated with that session
  return serverSessionP->getSmlWorkspaceID();
} // TServerEngineInterface::getSmlInstanceOfSession


} // namespace sysync

// eof
