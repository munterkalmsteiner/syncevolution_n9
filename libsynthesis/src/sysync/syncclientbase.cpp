/*
 *  TSyncClientBase
 *    Abstract baseclass for client
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 */

#include "prefix_file.h"

#include "sysync.h"
#include "syncclientbase.h"
#include "syncagent.h"

namespace sysync {

#ifdef ENGINEINTERFACE_SUPPORT

// Support for EngineModule common interface
// =========================================

#ifndef ENGINE_LIBRARY

#warning "using ENGINEINTERFACE_SUPPORT in old-style appbase-rooted environment. Should be converted to real engine usage later"

// Engine factory function for non-Library case
ENGINE_IF_CLASS *newClientEngine(void)
{
  // For real engine based targets, newClientEngine must create a target-specific derivate
  // of the client engine, which then has a suitable newSyncAppBase() method to create the
  // appBase. For old-style environment, a generic TClientEngineInterface is ok, as this
  // in turn calls the global newSyncAppBase() which then returns the appropriate
  // target specific appBase.
  return new TClientEngineInterface;
} // newClientEngine

/// @brief returns a new application base.
TSyncAppBase *TClientEngineInterface::newSyncAppBase(void)
{
  // For not really engine based targets, the appbase factory function is
  // a global routine (for real engine targets, it is a true virtual of
  // the engineInterface, implemented in the target's leaf engineInterface derivate.
  // - for now, use the global appBase creator routine
  return sysync::newSyncAppBase(); // use global factory function
} // TClientEngineInterface::newSyncAppBase

#else

// EngineInterface methods
// -----------------------


/// @brief Open a session
/// @param aNewSessionH[out] receives session handle for all session execution calls
/// @param aSelector[in] selector, depending on session type. For multi-profile clients: profile ID to use
/// @param aSessionName[in] a text name/id to identify a session, useage depending on session type.
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TClientEngineInterface::OpenSessionInternal(SessionH &aNewSessionH, uInt32 aSelector, cAppCharP aSessionName)
{
  TSyncClientBase *clientBaseP = static_cast<TSyncClientBase *>(getSyncAppBase());
  localstatus sta = LOCERR_WRONGUSAGE;

  // No client session may exist when opening a new one
  if (clientBaseP->fClientSessionP)
    return LOCERR_WRONGUSAGE;
  // check type of session
  if (aSelector == SESSIONSEL_DBAPI_TUNNEL) {
    // initiate a DBAPI tunnel session.
    #ifdef DBAPI_TUNNEL_SUPPORT
    // Create a new session, sessionName selects datastore
    sta = clientBaseP->CreateTunnelSession(aSessionName);
    if (sta==LOCERR_OK) {
      // return the session pointer as handle
      aNewSessionH=(SessionH)clientBaseP->fClientSessionP;
    }
    else {
      // error: make sure it is deleted in case it was half-constructed
      if (clientBaseP->fClientSessionP) {
        delete clientBaseP->fClientSessionP;
        clientBaseP->fClientSessionP = NULL;
      }
    }
    #else
    return LOCERR_NOTIMP; // tunnel not implemented
    #endif
  }
  else if ((aSelector & ~SESSIONSEL_PROFILEID_MASK) == SESSIONSEL_CLIENT_AS_CHECK) {
    // special autosync-checking "session"
    #ifdef AUTOSYNC_SUPPORT
    // %%% tbi
    return LOCERR_NOTIMP; // %%% not implemented for now
    #else
    return LOCERR_NOTIMP; // no Autosync in this engine
    #endif
  }
  else {
    // Create a new session
    sta = clientBaseP->CreateSession();
    // Pass profile ID
    if (sta==LOCERR_OK) {
      clientBaseP->fClientSessionP->SetProfileSelector(aSelector & SESSIONSEL_PROFILEID_MASK);
      // return the session pointer as handle
      aNewSessionH=(SessionH)clientBaseP->fClientSessionP;
    }
  }
  // done
  return sta;
} // TClientEngineInterface::OpenSessionInternal


/// @brief open session specific runtime parameter/settings key
/// @note key handle obtained with this call must be closed BEFORE SESSION IS CLOSED!
/// @param aNewKeyH[out] receives the opened key's handle on success
/// @param aSessionH[in] session handle obtained with OpenSession
/// @param aMode[in] the open mode
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TClientEngineInterface::OpenSessionKey(SessionH aSessionH, KeyH &aNewKeyH, uInt16 aMode)
{
  // %%% add autosync-check session case here
  // must be current session's handle (for now - however
  // future engines might allow multiple concurrent client sessions)
  if (aSessionH != (SessionH)static_cast<TSyncClientBase *>(getSyncAppBase())->fClientSessionP)
    return LOCERR_WRONGUSAGE; // something wrong with that handle
  // get client session pointer
  TSyncAgent *clientSessionP = static_cast<TSyncAgent *>((void *)aSessionH);
  // create settings key for the session
  aNewKeyH = (KeyH)clientSessionP->newSessionKey(this);
  // done
  return LOCERR_OK;
} // TClientEngineInterface::OpenSessionKey


/// @brief Close a session
/// @note  It depends on session type if this also destroys the session or if it may persist and can be re-opened.
/// @param aSessionH[in] session handle obtained with OpenSession
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TClientEngineInterface::CloseSession(SessionH aSessionH)
{
  TSyncClientBase *clientBaseP = static_cast<TSyncClientBase *>(getSyncAppBase());

  // %%% add autosync-check session case here
  // Nothing to do if no client session exists or none is requested for closing
  if (!clientBaseP->fClientSessionP || !aSessionH)
    return LOCERR_OK; // nop
  // must be current session's handle
  if (aSessionH != (SessionH)clientBaseP->fClientSessionP)
    return LOCERR_WRONGUSAGE; // something wrong with that handle
  // terminate running session (if any)
  clientBaseP->KillClientSession(LOCERR_USERABORT); // closing while session in progress counts as user abort
  // done
  return LOCERR_OK;
} // TClientEngineInterface::CloseSession


/// @brief Executes next step of the session
/// @param aSessionH[in] session handle obtained with OpenSession
/// @param aStepCmd[in/out] step command (STEPCMD_xxx):
///        - tells caller to send or receive data or end the session etc.
///        - instructs engine to suspend or abort the session etc.
/// @param aInfoP[in] pointer to a TEngineProgressInfo structure, NULL if no progress info needed
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TClientEngineInterface::SessionStep(SessionH aSessionH, uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  TSyncClientBase *clientBaseP = static_cast<TSyncClientBase *>(getSyncAppBase());

  // %%% add autosync-check session case here
  // must be current session's handle (for now - however
  // future engines might allow multiple concurrent client sessions)
  if (aSessionH != (SessionH)clientBaseP->fClientSessionP)
    return LOCERR_WRONGUSAGE; // something wrong with that handle
  // get client session pointer
  TSyncAgent *clientSessionP = static_cast<TSyncAgent *>((void *)aSessionH);
  // let client session handle it
  return clientSessionP->SessionStep(aStepCmd, aInfoP);
} // TClientEngineInterface::SessionStep


/// @brief returns the SML instance for a given session handle
///   (internal helper to allow TEngineInterface to provide the access to the SyncML buffer)
InstanceID_t TClientEngineInterface::getSmlInstanceOfSession(SessionH aSessionH)
{
  TSyncClientBase *clientBaseP = static_cast<TSyncClientBase *>(getSyncAppBase());

  // %%% add autosync-check session case here
  // must be current session's handle (for now - however
  // future engines might allow multiple concurrent client sessions)
  if (aSessionH != (SessionH)clientBaseP->fClientSessionP)
    return 0; // something wrong with session handle -> no SML instance
  // get client session pointer
  TSyncAgent *clientSessionP = static_cast<TSyncAgent *>((void *)aSessionH);
  // return SML instance associated with that session
  return clientSessionP->getSmlWorkspaceID();
} // TClientEngineInterface::getSmlInstanceOfSession

TSyError TClientEngineInterface::debugPuts(cAppCharP aFile, int aLine, cAppCharP aFunction,
                                           int aDbgLevel, cAppCharP aPrefix,
                                           cAppCharP aText)
{
  #if defined(SYDEBUG)
  static_cast<TSyncClientBase *>(getSyncAppBase())->getDbgLogger()->DebugPuts(TDBG_LOCATION_ARGS(aFunction, aFile, aLine /* aPrefix */)
                                                                              aDbgLevel, aText);
  return 0;
  #else
  return LOCERR_NOTIMP;
  #endif
}

#endif // ENGINE_LIBRARY

#endif // ENGINEINTERFACE_SUPPORT



// TSyncClientBase
// ===============

#ifdef DIRECT_APPBASE_GLOBALACCESS
// Only for old-style targets that still use a global anchor

TSyncClientBase *getClientBase(void)
{
  return static_cast<TSyncClientBase *>(getSyncAppBase());
} // getClientBase

#endif // DIRECT_APPBASE_GLOBALACCESS


// constructor
TSyncClientBase::TSyncClientBase() :
  TSyncAppBase(),
  fClientSessionP(NULL)
{
  // this is a client engine
  fIsServer = false;
} // TSyncClientBase::TSyncClientBase


// destructor
TSyncClientBase::~TSyncClientBase()
{
  // kill session, if any
  KillClientSession();
  fDeleting=true; // flag deletion to block calling critical (virtual) methods
  // delete client session, if any
  SYSYNC_TRY {
    // %%%%%%%% tdb...
  }
  SYSYNC_CATCH (...)
  SYSYNC_ENDCATCH
} // TSyncClientBase::~TSyncClientBase


// Called from SyncML toolkit when a new SyncML message arrives
// - dispatches to session's StartMessage
Ret_t TSyncClientBase::StartMessage(
  InstanceID_t aSmlWorkspaceID, // SyncML toolkit workspace instance ID
  VoidPtr_t aUserData, // pointer to a TSyncAgent descendant
  SmlSyncHdrPtr_t aContentP // SyncML tookit's decoded form of the <SyncHdr> element
) {
  TSyncSession *sessionP = static_cast<TSyncAgent *>(aUserData); // the client session
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
} // TSyncClientBase::StartMessage



#ifdef DBAPI_TUNNEL_SUPPORT

// - create a new client DBApi tunnel session
localstatus TSyncClientBase::CreateTunnelSession(cAppCharP aDatastoreName)
{
  localstatus sta;

  // create an ordinary session
  sta = CreateSession();
  if (sta==LOCERR_OK) {
    // determine which datastore to address
    sta = fClientSessionP->InitializeTunnelSession(aDatastoreName);
  }
  // done
  return sta;
} // TSyncClientBase::CreateTunnelSession

#endif // DBAPI_TUNNEL_SUPPORT



// create a new client session
localstatus TSyncClientBase::CreateSession(void)
{
  // remove any possibly existing old session first
  KillClientSession();
  // get config
  //TAgentConfig *configP = static_cast<TAgentConfig *>(getSyncAppBase()->getRootConfig()->fAgentConfigP);
  // create a new client session of appropriate type
  // - use current time as session ID (only for logging purposes)
  string s;
  LONGLONGTOSTR(s,PRINTF_LLD_ARG(getSystemNowAs(TCTX_UTC)));
  fClientSessionP = static_cast<TAgentConfig *>(fConfigP->fAgentConfigP)->CreateClientSession(s.c_str());
  if (!fClientSessionP) return LOCERR_UNDEFINED;
  // check expiry here
  return appEnableStatus();
} // TSyncClientBase::CreateSession


// initialize the (already created) client session and link it with the SML toolkit
localstatus TSyncClientBase::InitializeSession(uInt32 aProfileID, bool aAutoSyncSession)
{
  // session must be created before with CreateSession()
  if (!fClientSessionP)
    return LOCERR_WRONGUSAGE;
  // initialitze session
  return fClientSessionP->InitializeSession(aProfileID, aAutoSyncSession);
}


// create a message into the instance buffer
localstatus TSyncClientBase::generateRequest(bool &aDone)
{
  return getClientSession()->NextMessage(aDone);
} // TSyncClientBase::generateRequest



// clear all unprocessed or unsent data from SML workspace
void TSyncClientBase::clrUnreadSmlBufferdata(void)
{
  InstanceID_t myInstance = getClientSession()->getSmlWorkspaceID();
  MemPtr_t p;
  MemSize_t s;

  smlLockReadBuffer(myInstance,&p,&s);
  smlUnlockReadBuffer(myInstance,s);
} // TSyncClientBase::clrUnreadSmlBufferdata



// process message in the instance buffer
localstatus TSyncClientBase::processAnswer(void)
{
  return fClientSessionP->processAnswer();
}


// - extract hostname from an URI according to transport
void TSyncClientBase::extractHostname(const char *aURI, string &aHostName)
{
  string port;
  splitURL(aURI,NULL,&aHostName,NULL,NULL,NULL,&port,NULL);
  // keep old semantic: port included in aHostName
  if (!port.empty()) {
    aHostName += ':';
    aHostName += port;
  }
} // TSyncClientBase::extractHostname


// - extract document name from an URI according to transport
void TSyncClientBase::extractDocumentInfo(const char *aURI, string &aDocName)
{
  string query;
  splitURL(aURI,NULL,NULL,&aDocName,NULL,NULL,NULL,&query);
  // keep old semantic: query part of aDocName
  if (!query.empty()) {
    aDocName += '?';
    aDocName += query;
  }
} // TSyncClientBase::extractDocumentInfo


// - extract protocol name from an URI according to transport
void TSyncClientBase::extractProtocolname(const char *aURI, string &aProtocolName)
{
  splitURL(aURI,&aProtocolName,NULL,NULL,NULL,NULL,NULL,NULL);
} // TSyncClientBase::extractProtocolname


// delete and unlink current session from SML toolkit
void TSyncClientBase::KillClientSession(localstatus aStatusCode)
{
  if (fClientSessionP) {
    // Abort session
    if (aStatusCode) fClientSessionP->AbortSession(aStatusCode,true);
    // remove instance of that session
    freeSmlInstance(fClientSessionP->getSmlWorkspaceID());
    fClientSessionP->setSmlWorkspaceID(0); // make sure it isn't set any more
    // delete session itself
    TSyncAgent *clientP = fClientSessionP;
    fClientSessionP=NULL;
    delete clientP;
  }
} // TSyncClientBase::KillSession

} // namespace sysync

// eof
