/*
 *  TSyncSessionDispatch
 *    Abstract baseclass for session manager controlling
 *    instantiation and removal of TSySyncSession objects and
 *    re-connecting requests to sessions.
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef SYNCSESSIONDISPATCH_H
#define SYNCSESSIONDISPATCH_H

// general includes (SyncML tookit, windows, Clib)
#include "sysync.h"
#include "syncappbase.h"
#include "syncagent.h"



namespace sysync {

// forward declarations
class TSyncSession;
class TSyncAgent;
class TSyncSessionDispatch;


// Session handle, includes dispatcher-specific stuff for handling sessions
// (such as thread IDs, locks, thread termination mechanisms etc.)
class TSyncSessionHandle {
public:
  // the session
  TSyncAgent * fSessionP;
  // the app base
  TSyncAppBase * fAppBaseP;
  // - used for counting session for session limiting
  bool fOutdated;
  // - enter session (re-entrance avoidance)
  virtual bool EnterSession(sInt32 aMaxWaitTime) = 0;
  // - leave session
  virtual void LeaveSession(void) = 0;
  // - Enter, terminate and delete session. Must protect caller from exceptions
  //   when termination/deletion fails.
  //   NOTE: keeps session Enter()ed, to avoid any other thread to enter.
  virtual bool EnterAndTerminateSession(uInt16 aStatusCode);
  // - terminate and delete session. Must protect caller from exceptions
  //   when termination/deletion fails.
  //   NOTE: session must be entered already
  virtual bool TerminateSession(uInt16 aStatusCode);
  // constructor/destructor
  TSyncSessionHandle(TSyncAppBase *aAppBaseP);
  virtual ~TSyncSessionHandle();
}; // TSyncSessionHandle


// Container types
// - main session list
typedef std::map<std::string,TSyncSessionHandle*> TSyncSessionHandlePContainer; // contains sync sessions by sessionID-String
// - deletable sessions list
typedef std::list<TSyncSessionHandle*> TSyncSessionHandlePList;


/* TSyncSessionDispatch manages a list of active sync sessions
 * and distributes server requests to the appropriate session,
 * or creates a new session if needed.
 *
 * This is a global, singular object which is instantiated ONCE per server
 * instance. It receives calls from SyncML-Toolkit callbacks.
 *
 * The adding an deleting of sessions is done in a thread-safe way
 * as multiple request might be processed simultaneously in different
 * threads.
 */
class TSyncSessionDispatch : public TSyncAppBase {
  typedef TSyncAppBase inherited;
public:
  // constructors/destructors
  TSyncSessionDispatch();
  virtual ~TSyncSessionDispatch();
  // immediately kill all sessions
  void TerminateAllSessions(uInt16 aStatusCode);
  // handlers for SyncML toolkit callbacks
  // - Start Message: identifies Session, and creates new or assigns existing session
  Ret_t StartMessage(
    InstanceID_t aSmlWorkspaceID, // SyncML toolkit workspace instance ID
    VoidPtr_t aUserData, // user data, should be NULL (as StartMessage is responsible for setting userdata)
    SmlSyncHdrPtr_t aContentP // SyncML tookit's decoded form of the <SyncHdr> element
  );
  // - end of request (to make sure even incomplete SyncML messages get cleaned up properly)
  Ret_t EndRequest(InstanceID_t aSmlWorkspaceID, bool &aHasData, string &aRespURI, bool &aEOSession, uInt32 aReqBytes);
  // Answer resending
  // - buffer answer in the session's buffer if transport allows it
  Ret_t bufferAnswer(InstanceID_t aSmlWorkspaceID, MemPtr_t aAnswer, MemSize_t aAnswerSize);
  // - get buffered answer from the session's buffer if there is any
  void getBufferedAnswer(InstanceID_t aSmlWorkspaceID, MemPtr_t &aAnswer, MemSize_t &aAnswerSize);
  // Session handling
  // - Collect timed-out sessions and remove them from the session list
  void collectTimedOutSessions(TSyncSessionHandlePList &aDeletableSessions);
  // - Try to enter and delete the sessions passed in one by one
  void deleteListedSessions(TSyncSessionHandlePList &aDelSessionList);
  // - create new session
  TSyncSessionHandle *CreateAndEnterServerSession(cAppCharP aPredefinedSessionID=NULL);
  // - create new session handle (of correct TSessionHandle derivate for dispatcher used)
  virtual TSyncSessionHandle *CreateSessionHandle(void) = 0;
  // - extract sessionID from query string (docname or TargetURI)
  virtual bool extractSessionID(
    const char *aQueryString,
    string &aSessionID
  ) { return false; /* no sessionID found */ }
  // list currently active sessions to debug channel
  void dbgListSessions(void);
  // - number of sessions
  sInt32 numSessions(void) { return fSessions.size(); };
  #ifdef SYSYNC_TOOL
  // get or create a session for use with the diagnostic tool
  TSyncAgent *getSySyToolSession(void);
  #endif
protected:
  // must be implemented in derived class to make access to
  // fSessions thread-safe
  virtual void LockSessions(void) { /* dummy in base class */ };
  virtual void ReleaseSessions(void) {  /* dummy in base class */ };
  // remove session from internal session list and return it's handle object
  // Note: session list must be locked while calling RemoveSession
  TSyncSessionHandle *RemoveSession(TSyncSession *aSessionP) throw();
  // remove and kill session
  // Note: may not be called when session list is already locked
  void KillServerSession(TSyncAgent *aSessionP, uInt16 aStatusCode, const char *aMsg=NULL, uInt32 aErrorCode=0); // by pointer
  void KillSessionByInstance(InstanceID_t aSmlWorkspaceID, uInt16 aStatusCode, const char *aMsg=NULL, uInt32 aErrorCode=0); // by SyncML toolkit instance ID
  // Handle exception happening while decoding commands for a session
  virtual Ret_t HandleDecodingException(TSyncSession *aSessionP, const char *aRoutine, exception *aExceptionP=NULL);
  #ifndef NOWSM
  // lock to make SyncML toolkit instance creation/freeing thread safe
  virtual void LockToolkit(void) { /* dummy in base class */ };
  virtual void ReleaseToolkit(void) {  /* dummy in base class */ };
  #endif
private:
  // session map
  TSyncSessionHandlePContainer fSessions;
  #ifdef SYSYNC_TOOL
  TSyncSessionHandle *fToolSessionHP;
  #endif
}; // TSyncSessionDispatch


} // namespace sysync


#endif // SYNCSESSIONDISPATCH_H


// eof
