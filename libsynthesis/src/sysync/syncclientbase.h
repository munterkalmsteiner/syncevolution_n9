/*
 *  TSyncClientBase
 *    Abstract baseclass for client
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef SYNCCLIENTBASE_H
#define SYNCCLIENTBASE_H

// general includes (SyncML tookit, windows, Clib)
#include "sysync.h"
#include "syncappbase.h"
#include "syncagent.h"



namespace sysync {

// forward declarations
class TSyncSession;
class TSyncAgent;
class TSyncClientBase;

#ifdef ENGINEINTERFACE_SUPPORT

// Support for EngineModule common interface
// =========================================


// Engine module class
class TClientEngineInterface:
  public TEngineInterface
{
  typedef TEngineInterface inherited;
public:
  // constructor
  TClientEngineInterface() {};

  #ifndef ENGINE_LIBRARY

  // appbase factory (based on old appbase-root method) is here
  virtual TSyncAppBase *newSyncAppBase(void);

  #else

  // Running a Client Sync Session
  // -----------------------------

  /// @brief Open a session
  /// @param aNewSessionH[out] receives session handle for all session execution calls
  /// @param aSelector[in] selector, depending on session type. For multi-profile clients: profile ID to use
  /// @param aSessionName[in] a text name/id to identify a session, useage depending on session type.
  /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
  virtual TSyError OpenSessionInternal(SessionH &aNewSessionH, uInt32 aSelector, cAppCharP aSessionName);

  /// @brief open session specific runtime parameter/settings key
  /// @note key handle obtained with this call must be closed BEFORE SESSION IS CLOSED!
  /// @param aNewKeyH[out] receives the opened key's handle on success
  /// @param aSessionH[in] session handle obtained with OpenSession
  /// @param aMode[in] the open mode
  /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
  virtual TSyError OpenSessionKey(SessionH aSessionH, KeyH &aNewKeyH, uInt16 aMode);

  /// @brief Close a session
  /// @note  It depends on session type if this also destroys the session or if it may persist and can be re-opened.
  /// @param aSessionH[in] session handle obtained with OpenSession
  /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
  virtual TSyError CloseSession(SessionH aSessionH);

  /// @brief Executes sync session or other sync related activity step by step
  /// @param aSessionH[in] session handle obtained with OpenSession
  /// @param aStepCmd[in/out] step command (STEPCMD_xxx):
  ///        - tells caller to send or receive data or end the session etc.
  ///        - instructs engine to suspend or abort the session etc.
  /// @param aInfoP[in] pointer to a TEngineProgressInfo structure, NULL if no progress info needed
  /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
  virtual TSyError SessionStep(SessionH aSessionH, uInt16 &aStepCmd,  TEngineProgressInfo *aInfoP = NULL);

  virtual TSyError debugPuts(cAppCharP aFile, int aLine, cAppCharP aFunction,
                             int aDbgLevel, cAppCharP aLinePrefix,
                             cAppCharP aText);

protected:

  /// @brief returns the SML instance for a given session handle
  virtual InstanceID_t getSmlInstanceOfSession(SessionH aSessionH);

  #endif // ENGINE_LIBRARY

}; // TClientEngineInterface

#endif // ENGINEINTERFACE_SUPPORT



#ifdef DIRECT_APPBASE_GLOBALACCESS
// access to client base
TSyncClientBase *getClientBase(void);
#endif


/* TSyncClientBase is a singular object that can set-up and
 * start Sync client sessions.
 * It also receives calls from SyncML toolkit callbacks and
 * can route them to the active TSyncSession
 */
class TSyncClientBase : public TSyncAppBase
{
  typedef TSyncAppBase inherited;
#ifdef ENGINEINTERFACE_SUPPORT
friend class TClientEngineInterface;
#endif

public:
  // constructors/destructors
  TSyncClientBase();
  virtual ~TSyncClientBase();
  // Session flow entry points
  // - create a new client session
  localstatus CreateSession(void);
  #ifdef DBAPI_TUNNEL_SUPPORT
  // - create a new client DBApi tunnel session
  localstatus CreateTunnelSession(cAppCharP aDatastoreName);
  #endif
  // - initialize client session and link it with the SML toolkit
  //   (session must be created with CreateSession() before)
  localstatus InitializeSession(uInt32 aProfileID, bool aAutoSyncSession=false);
  // - create a message into the instance buffer, returns aDone==true if none needed any more
  localstatus generateRequest(bool &aDone);
  // - clear unread data in the instance buffer (received and to-be-sent)
  void clrUnreadSmlBufferdata(void);
  // - process message in the instance buffer
  localstatus processAnswer(void);
  // - get current session
  TSyncAgent *getClientSession(void) { return fClientSessionP; };
  // - remove and kill current session
  void KillClientSession(localstatus aStatusCode=0);
  // handlers for SyncML toolkit callbacks
  // - Start/End Message: identifies Session, and creates new or assigns existing session
  Ret_t StartMessage(
    InstanceID_t aSmlWorkspaceID, // SyncML toolkit workspace instance ID
    VoidPtr_t aUserData, // pointer to a TSyncAgent descendant
    SmlSyncHdrPtr_t aContentP // SyncML tookit's decoded form of the <SyncHdr> element
  );
  // Session handling
  // - extract hostname from an URI according to transport
  virtual void extractHostname(const char *aURI, string &aHostName);
  // - extract document info (name/auth) from an URI according to transport
  void extractDocumentInfo(const char *aURI, string &aDocName);
  // - extract protocol name from an URI according to transport
  virtual void extractProtocolname(const char *aURI, string &aProtocolName);
protected:
  // Handle exception happening while decoding commands for session
  // (Former KillSession)
  virtual Ret_t HandleDecodingException(TSyncSession * /* aSessionP */, const char * /* aRoutine */, exception * /* aExceptionP */=NULL) { return SML_ERR_UNSPECIFIC; /* %%%% nop so far */ };
private:
  // the current session
  TSyncAgent *fClientSessionP;
}; // TSyncClientBase


} // namespace sysync


#endif // SYNCCLIENTBASE_H


// eof
