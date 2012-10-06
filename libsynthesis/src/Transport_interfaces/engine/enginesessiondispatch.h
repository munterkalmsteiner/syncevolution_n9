/*
 *  TEngineSessionDispatch
 *    Server library specific descendant of TSyncSessionDispatch
 *
 *  Copyright (c) 2009-2011 by Synthesis AG + plan44.ch
 *
 *  2009-02-06 : luz : Created
 *
 */

#ifndef ENGINESESSIONDISPATCH_H
#define ENGINESESSIONDISPATCH_H

// required headers
#include "syncappbase.h"
#include "syncagent.h"


namespace sysync {


// Engine module class
class TServerEngineInterface :
  public TEngineInterface
{
  typedef TEngineInterface inherited;
public:
  // constructor
  TServerEngineInterface() {};

  // no appbase factory at this level (must be implemented in non-virtual descendants)
  virtual TSyncAppBase *newSyncAppBase(void) = 0;

  // Running a Server Sync Session
  // -----------------------------

  /// @brief Open a session
  /// @param aNewSessionH[out] receives session handle for all session execution calls
  /// @param aSelector[in] selector, depending on session type.
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

protected:

  /// @brief returns the SML instance for a given session handle
  virtual InstanceID_t getSmlInstanceOfSession(SessionH aSessionH);

}; // TServerEngineInterface



// engine client root config
class TEngineServerRootConfig : public TRootConfig
{
  typedef TRootConfig inherited;
public:
  TEngineServerRootConfig(TSyncAppBase *aSyncAppBaseP) : inherited(aSyncAppBaseP) {};
  // factory methods
  virtual void installCommConfig(void);
  // Config parsing
  #ifndef HARDCODED_CONFIG
  virtual bool parseCommConfig(const char **aAttributes, sInt32 aLine);
  #endif
}; // TEngineClientRootConfig


// engine server transport config
class TEngineServerCommConfig: public TCommConfig
{
  typedef TCommConfig inherited;
public:
  TEngineServerCommConfig(TConfigElement *aParentElementP);
  virtual ~TEngineServerCommConfig();
  // config vars
  // - session ID CGI config
  bool fSessionIDCGI;
  string fSessionIDCGIPrefix;
  // - indicates transport can buffer answer to client until next client response, so it can be resent in case we detect a client resend.
  bool fBuffersRetryAnswer;
protected:
  // check config elements
  #ifndef HARDCODED_CONFIG
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  virtual void clear();
  virtual void localResolve(bool aLastPass);
}; // TXPTCommConfig


// forward declarations
class TSyncAgent;
class TSyncSession;



// session handle for engine sessions
// containing engine-specific session status
class TEngineServerSessionHandle
{
public:
  TEngineServerSessionHandle(TServerEngineInterface *aServerEngineInterface);
  virtual ~TEngineServerSessionHandle();
  // the server engine interface
  TServerEngineInterface *fServerEngineInterface;
  // the session itself
  TSyncAgent *fServerSessionP;
  // the toolkit instance used by the session
  InstanceID_t fSmlInstanceID;
  // status of the session
  localstatus fServerSessionStatus;
};


// AppBase class for all server engines (libararies with API to build custom servers)
// Note: unlike non-engine servers, the server library bypasses TSyncSessionDispatch
//       completely. All actual session dispatch, thread locking, session timeout
//       stuff must be implemented outside the library for engine-based servers.
class TEngineSessionDispatch : public TSyncAppBase {
  typedef TSyncAppBase inherited;
public:
  // constructors/destructors
  TEngineSessionDispatch();
  virtual ~TEngineSessionDispatch();
  // handlers for SyncML toolkit callbacks
  // - Start/End Message: identifies Session, and creates new or assigns existing session
  Ret_t StartMessage(
    InstanceID_t aSmlWorkspaceID, // SyncML toolkit workspace instance ID
    VoidPtr_t aUserData, // pointer to a TSyncAgent descendant
    SmlSyncHdrPtr_t aContentP // SyncML tookit's decoded form of the <SyncHdr> element
  );
  // - Handle exception happening while decoding commands for a session
  virtual Ret_t HandleDecodingException(TSyncSession *aSessionP, const char *aRoutine, exception *aExceptionP);
  // test if message buffering is available
  virtual bool canBufferRetryAnswer(void);
  // combine URI and session ID to make a RespURI according to transport
  virtual void generateRespURI(
    string &aRespURI,
    cAppCharP aLocalURI,
    cAppCharP aSessionID
  );
}; // TEngineSessionDispatch

} // namespace sysync

#endif // ENGINESESSIONDISPATCH_H


// eof
