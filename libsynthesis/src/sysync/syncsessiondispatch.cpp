/*
 *  TSyncSessionDispatch
 *    Global object, manages instantiation and removal of
 *    TSyncSession objects, connects requests to sessions,
 *    Interfaces between C-coded SyncML toolkit and SySync
 *    C++ framework.
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-05-07 : luz : Created
 *
 */

#include "prefix_file.h"

// %%%% Note: CW8.2 has bugs in SEH when optimization is turned on (even if we don't use SEH
//      here, we might call routines with SEH so be careful here)
#pragma optimization_level 0

#include "sysync.h"
#include "syncsessiondispatch.h"



// TSyncSessionHandle
// ==================

// constructor
TSyncSessionHandle::TSyncSessionHandle(TSyncAppBase *aAppBaseP) :
  fAppBaseP(aAppBaseP),
  fSessionP(NULL),
  fOutdated(false)
{
  OBJDEBUGPRINTFX(fAppBaseP,DBG_OBJINST,("++++++++ TSyncSessionHandle created"));
  MP_SHOWCURRENT(DBG_OBJINST,"TSyncSessionHandle created");
  // nop
} // TSyncSessionHandle::TSyncSessionHandle


// destructor
TSyncSessionHandle::~TSyncSessionHandle()
{
  if (fSessionP) {
    POBJDEBUGPRINTFX(fAppBaseP,DBG_ERROR,("TSyncSessionHandle deleted with unterminated session (deleting now as well...)"));
    MP_DELETE(DBG_OBJINST,"fSessionP",fSessionP);
  }
  fSessionP=NULL;
  MP_SHOWCURRENT(DBG_OBJINST,"TSyncSessionHandle deleting");
  OBJDEBUGPRINTFX(fAppBaseP,DBG_OBJINST,("-------- TSyncSessionHandle destroyed"));
} // TSyncSessionHandle::~TSyncSessionHandle



// terminate and delete session. Must protect caller from exceptions
// when termination/deletion fails.
// NOTE: keeps session Enter()ed, to avoid any other thread to enter.
bool TSyncSessionHandle::EnterAndTerminateSession(uInt16 aStatusCode)
{
  // no locks here, so just call TerminateSession
  return TerminateSession(aStatusCode);
} // TSyncSessionHandle::EnterAndTerminateSession



// terminate and delete session. Must protect caller from exceptions
// when termination/deletion fails.
// NOTE: keeps session Enter()ed, to avoid any other thread to enter.
bool TSyncSessionHandle::TerminateSession(uInt16 aStatusCode)
{
  if (fSessionP) {
    // - Set abort code if not zero (not aborted, regular termination)
    if (aStatusCode) fSessionP->AbortSession(aStatusCode,true); // assume local cause
    // - terminate the session, which causes datastores to be terminated as well
    fSessionP->TerminateSession();
    // - now actually delete
    TSyncSession *delP = fSessionP;
    fSessionP=NULL; // make sure we can't delete again under any circumstance
    delete delP;
    POBJDEBUGPRINTFX(fAppBaseP,DBG_TRANSP,("TerminateSession(%hd): deleted session successfully", aStatusCode));
  }
  else {
    POBJDEBUGPRINTFX(fAppBaseP,DBG_TRANSP+DBG_EXOTIC,("TerminateSession(%hd) - no op, session was already terminated/deleted before", aStatusCode));
  }
  return true;
} // TSyncSessionHandle::TerminateSession


// TSyncSessionDispatch
// ====================


// Support for SySync Diagnostic Tool
#ifdef SYSYNC_TOOL

// get or create a session for use with the diagnostic tool
TSyncAgent *TSyncSessionDispatch::getSySyToolSession(void)
{
  TSyncAgent *sessionP=NULL; // the session (new or existing found in fSessions)

  if (fToolSessionHP) {
    sessionP = fToolSessionHP->fSessionP;
  }
  else {
    // - create session object with given ID
    fToolSessionHP = CreateSessionHandle();
    sessionP = static_cast<TAgentConfig *>(fConfigP->fAgentConfigP)->CreateServerSession(fToolSessionHP,"SySyTool");
    fToolSessionHP->fSessionP=sessionP;
  }
  return sessionP;
}

#endif // SYSYNC_TOOL



// constructor
TSyncSessionDispatch::TSyncSessionDispatch() :
  TSyncAppBase()
{
  // this is a server engine
  fIsServer = true;
  // other init
  #ifdef SYSYNC_TOOL
  fToolSessionHP=NULL; // no tool session yet
  #endif
} // TSyncSessionDispatch::TSyncSessionDispatch



// immediately kill all sessions
void TSyncSessionDispatch::TerminateAllSessions(uInt16 aStatusCode)
{
  try {
    TSyncSessionHandlePContainer::iterator pos;
    for (pos=fSessions.begin(); pos!=fSessions.end(); ++pos) {
      if ((pos->second)->fSessionP) {
        (pos->second)->fSessionP->AbortSession(aStatusCode,true);
        (pos->second)->fSessionP->ResetSession(); // reset properly before deleting
        // delete the session handle including the session
        MP_DELETE(DBG_OBJINST,"leftover session",(pos->second));
      }
    }
    // delete the session list
    fSessions.clear();
  }
  catch (...)
  {
  }
} // TSyncSessionDispatch::TerminateAllSessions





// destructor
TSyncSessionDispatch::~TSyncSessionDispatch()
{
  fDeleting=true; // flag deletion to block calling critical (virtual) methods
  // delete all session handles
  // NOTE: fSessionslock is already gone as it is owned by
  //       derived class
  TerminateAllSessions(500); // server error
  #ifdef SYSYNC_TOOL
  // delete tool session
  if (fToolSessionHP) {
    fToolSessionHP->fSessionP->ResetSession();
    MP_DELETE(DBG_OBJINST,"leftover session",(fToolSessionHP));
    fToolSessionHP=NULL;
  }
  #endif
} // TSyncSessionDispatch::~TSyncSessionDispatch



// Called from SyncML toolkit when a new SyncML message arrives
// - finds appropriate session or creates new one
// - dispatches to session's StartMessage
Ret_t TSyncSessionDispatch::StartMessage(
  InstanceID_t aSmlWorkspaceID, // SyncML toolkit workspace instance ID
  VoidPtr_t aUserData, // user data, contains NULL or char* to transport-layer supported session ID
  SmlSyncHdrPtr_t aContentP // SyncML tookit's decoded form of the <SyncHdr> element
) {
  TSyncAgent *sessionP=NULL; // the session (new or existing found in fSessions)
  TSyncSessionHandle *sessionHP=NULL;
  Ret_t err;

  try {
    // obtain session (existing or new)
    // - lock access to list
    //   NOTE: Basic policy is to keep sessions locked only while executing code
    //         that cannot throw a SE or a C++ exception, as unlocking too much
    //         (in a general exception catcher) makes much troubles. All Code
    //         that might throw should be executed ONLY with session list released!
    LockSessions();
    // - now get iterator
    TSyncSessionHandlePContainer::iterator pos=fSessions.end();
    // - get SessionID, either from transport (aUserData) or from TargetURI
    string sessionID;
    AssignString(sessionID,(const char *)aUserData);
    if (!sessionID.empty()) {
      // - transport has provided a sessionID
      PDEBUGPRINTFX(DBG_PROTO,(
        "TSyncSessionDispatch::StartMessage called with transport layer session ID='%s'",
        sessionID.c_str()
      ));
    }
    else {
      // - try to get sessionID from TargetURI
      if (extractSessionID(smlSrcTargLocURIToCharP(aContentP->target),sessionID)) {
        PDEBUGPRINTFX(DBG_PROTO,(
          "TSyncSessionDispatch::StartMessage found session ID in Target.LocURI='%s'",
          sessionID.c_str()
        ));
      }
    }
    // - process session ID
    if (!sessionID.empty()) {
      // we seem to have a session ID
      pos=fSessions.find(sessionID);
      #ifdef SYDEBUG
      if (pos!=fSessions.end()) {
        PDEBUGPRINTFX(DBG_HOT,(
          "Session found by SessionID=%s",
          sessionID.c_str()
        ));
      }
      #endif
    }
    // now enter existing session or create new one
    if (pos!=fSessions.end()) {
      // found existing session:
      // - get handle
      sessionHP = (*pos).second;
      // - get session object pointer
      sessionP = sessionHP->fSessionP;
      PDEBUGPRINTFX(DBG_SESSION,("Entering found session..."));
      // Note: We have the session list locked, so we should not wait here for entering to avoid locking the entire server
      // - try entering
      int k=0;
      while (true) {
        if (sessionHP->EnterSession(0))
          break; // successfully entered
        // could not enter. Temporarily release sessions list and wait 10 seconds
        ReleaseSessions();
        if (k>60) {
          // 60*10 = 600 sec = 10 min waited, give up
          PDEBUGPRINTFX(DBG_ERROR,("Found session is locked for >%d seconds, giving up -> SML_ERR_UNSPECIFIC",k*10));
          return SML_ERR_UNSPECIFIC;
        }
        PDEBUGPRINTFX(DBG_LOCK,("Session could not be entered after waiting %d seconds, keep trying",k*10));
        // wait
        sleepLineartime(10*secondToLinearTimeFactor);
        // we need to re-find the session here, as we had given others control over the session list
        LockSessions();
        pos=fSessions.find(sessionID);
        if (pos==fSessions.end()) {
          ReleaseSessions();
          PDEBUGPRINTFX(DBG_HOT,(
            "SessionID=%s has disappeared from sessions list while we were waiting to enter it --> SML_ERR_UNSPECIFIC",
            sessionID.c_str()
          ));
          return SML_ERR_UNSPECIFIC;
        }
        // - get handle again
        sessionHP = (*pos).second;
        k++;
      }
      // Session is entered, so we can release the list (no other thread will be able to enter same session)
      ReleaseSessions();
      // Show that we entered the session
      PDEBUGPRINTFX(DBG_SESSION,("Session entered"));
    } // if session already exists
    else {
      // we need a new session
      // Note: session list is still locked here
      PDEBUGPRINTFX(DBG_TRANSP+DBG_EXOTIC,("No session found, will need new one"));
      // - count sessions not belonging to this client
      sInt32 othersessioncount = 0;
      for (pos=fSessions.begin();pos!=fSessions.end();pos++) {
        // - get handle
        sessionHP = (*pos).second;
        // - get session
        sessionP=sessionHP->fSessionP;
        // check if session is for same device as current request
        if (strcmp(smlSrcTargLocURIToCharP(aContentP->source),sessionP->getRemoteURI())!=0) {
          // other device, count if not outdated
          if (!sessionHP->fOutdated) othersessioncount++;
        }
        else {
          // this device, set outdated flag to prevent inactive sessions from being
          // counted and limiting sessions
          sessionHP->fOutdated=true;
        }
      } // for
      PDEBUGPRINTFX(DBG_SESSION+DBG_EXOTIC,(
        "Found %ld sessions not related to device '%s'",
        (sInt32)othersessioncount,
        smlSrcTargLocURIToCharP(aContentP->source)
      ));
      // - get sessions that need to be deleted
      TSyncSessionHandlePList delList;
      collectTimedOutSessions(delList);
      // - now we can release the sessions list
      ReleaseSessions();
      // - actually delete them while session list is again unlocked
      deleteListedSessions(delList);
      // Now create new session
      sessionHP = CreateAndEnterServerSession(NULL); // have sessionID generated
      // - get session object pointer
      sessionP = sessionHP->fSessionP;
      #ifdef CONCURRENT_DEVICES_LIMIT
      // - now check session count (makes session busy if licensed session count is exceeded)
      //   Note: othersessioncount does not include our own session
      checkSessionCount(othersessioncount,sessionP);
      #endif
    }
    // now we should have a sessionP
    if (sessionP) {
      // Now let session handle SyncML header (start of message)
      // - sessionP and sessionHP are valid here
      // - assign pointers to allow direct
      //   routing and calling between session and SyncML toolkit
      err=setSmlInstanceUserData(aSmlWorkspaceID,sessionP); // toolkit must know session (as userData)
      if (err!=SML_ERR_OK) throw(TSmlException("setSmlInstanceUserData",err));
      // - let session know workspace ID
      sessionP->setSmlWorkspaceID(aSmlWorkspaceID); // session must know toolkit workspace
      // let session handle details of StartMessage callback
      return sessionP->StartMessage(aContentP);
    }
    else {
      PDEBUGPRINTFX(DBG_HOT,(
        "No session could be created --> SML_ERR_UNSPECIFIC",
        sessionID.c_str()
      ));
      return SML_ERR_UNSPECIFIC;
    }
  }
  catch (exception &e) {
    return HandleDecodingException(sessionP,"StartMessage",&e);
  }
  catch (...) {
    return HandleDecodingException(sessionP,"StartMessage",NULL);
  }
} // TSyncSessionDispatch::StartMessage



/// @brief Collect timed-out sessions and remove them from the session list
/// @param aDeletableSessions to-be deleted sessions will be appended to this list
/// @note session list must be locked before call!
void TSyncSessionDispatch::collectTimedOutSessions(TSyncSessionHandlePList &aDeletableSessions)
{
  TSyncAgent *sessionP=NULL; // the session (new or existing found in fSessions)
  TSyncSessionHandle *sessionHP=NULL;
  TAgentConfig *serverconfigP=NULL;

  // get agent config
  GET_CASTED_PTR(serverconfigP,TAgentConfig,fConfigP->fAgentConfigP,"missing agent (server) config");
  // - find timed-out sessions and count sessions not belonging to this client
  TSyncSessionHandlePContainer::iterator pos;
  for (pos=fSessions.begin();pos!=fSessions.end();pos++) {
    // - get handle
    sessionHP = (*pos).second;
    // - get session
    sessionP=sessionHP->fSessionP;
    // - check
    if (getSystemNowAs(TCTX_UTC) > sessionP->getSessionLastUsed()+serverconfigP->getSessionTimeout()) {
      // this session is too old, queue it for killing
      aDeletableSessions.push_back(sessionHP);
    }
  } // for
  // now remove the outdated sessions from the main session list
  // to make sure no other process can possibly access them
  // - session list is locked once here
  TSyncSessionHandlePList::iterator delpos;
  DEBUGPRINTFX(DBG_SESSION,(
    "Now removing %ld outdated sessions from the session list",
    (sInt32)aDeletableSessions.size()
  ));
  for (delpos=aDeletableSessions.begin();delpos!=aDeletableSessions.end();delpos++) {
    // - get handle
    sessionHP = (*delpos);
    // - remove session handle from the list
    //   NOTE: session list is already locked here, RemoveSession
    //         MUST be called with locked session list!
    if (!(sessionHP == RemoveSession(sessionHP->fSessionP)))
      throw (TSyncException("invalid linked session/sessionhandle pair"));
  }
} // collectTimedOutSessions


/// @brief Try to enter and delete the sessions passed in one by one
/// @note
/// - listed sessions must not be part of the sessions list any more
/// - on deletion failure, "zombie" sessions will be re-inserted into the session list (to get deleted once again later)
/// - intended for implementations without a session thread (XPT, ISAPI, not pipe)
void TSyncSessionDispatch::deleteListedSessions(TSyncSessionHandlePList &aDelSessionList)
{
  TSyncAgent *sessionP=NULL;
  TSyncSessionHandle *sessionHP=NULL;
  TAgentConfig *serverconfigP=NULL;

  // get agent config
  GET_CASTED_PTR(serverconfigP,TAgentConfig,fConfigP->fAgentConfigP,"missing agent (server) config");

  TSyncSessionHandlePList::iterator delpos;
  for (delpos=aDelSessionList.begin();delpos!=aDelSessionList.end();delpos++) {
    try {
      // - get handle and session
      sessionHP = (*delpos);
      sessionP = sessionHP->fSessionP;
      if (!sessionP) continue; // seems to be already deleted
      // - terminate the session
      #ifdef SYDEBUG
      string deletedsessionid = sessionP->getLocalSessionID();
      #endif
      PDEBUGPRINTFX(DBG_HOT,(
        "Terminating timed-out (older than %lld milliseconds) session '%s'...",
        serverconfigP->getSessionTimeout(),
        sessionP->getLocalSessionID()
      ));
      // - now actually delete
      if (sessionHP->EnterAndTerminateSession(408)) { // request timeout
        PDEBUGPRINTFX(DBG_PROTO,("Terminated and deleted timed-out session '%s'",deletedsessionid.c_str()));
        // - delete session handle itself
        delete sessionHP;
      } else {
        PDEBUGPRINTFX(DBG_ERROR,("Could NOT properly terminate/delete timed-out session '%s' now --> becomes a ZOMBIE",deletedsessionid.c_str()));
        // We cannot safely get rid of this session now, so we must put it back to the queue
        if (sessionHP->fSessionP) {
          // make it used again, so it will timeout later again
          sessionHP->fSessionP->SessionUsed();
          // re-insert into queue
          LockSessions();
          // however, if we now see that there is NO other session running, we'll risk crashing here and delete the session
          if (fSessions.size()==0) {
            // we'll do no harm to other sessions because there are none - try to hard-kill the session
            PDEBUGPRINTFX(DBG_ERROR,("No non-ZOMBIE sessions running -> risking to delete this ZOMBIE",deletedsessionid.c_str()));
            delete sessionHP;
          }
          else {
            fSessions[deletedsessionid]=sessionHP;
            PDEBUGPRINTFX(DBG_HOT,(
              "ZOMBIE session ID='%s' is now again in session list, waiting once more for timeout and hopefully clean deletion later",
              deletedsessionid.c_str()
            ));
          }
          ReleaseSessions();
        }
        else {
          PDEBUGPRINTFX(DBG_ERROR,("Termination failed, but sessionP gone nevertheless"));
        }
      }
    }
    catch (...) {
      PDEBUGPRINTFX(DBG_ERROR,("******** Exception while trying to kill outdated session"));
    }
  } // for
} // deleteListedSessions


/// Create new session, clean up timed-out sessions
/// @param aPredefinedSessionID : predefined sessionID, if NULL, internal ID will be generated
TSyncSessionHandle *TSyncSessionDispatch::CreateAndEnterServerSession(cAppCharP aPredefinedSessionID)
{
  TSyncAgent *sessionP=NULL; // the session (new or existing found in fSessions)
  TSyncSessionHandle *sessionHP=NULL;
  TAgentConfig *serverconfigP=NULL;

  // - create new session instance
  //   NOTE: session list is unlocked here already
  try {
    #ifdef APP_CAN_EXPIRE
    // Check expiry (note that this check is for a server running without restart)
    // - get current time
    lineartime_t nw = getSystemNowAs(TCTX_UTC);
    #ifdef EXPIRES_AFTER_DATE
    #ifdef SYSER_REGISTRATION
    if (!fRegOK) // only abort if no registration (but accept timed registration)
    #endif
    {
      sInt16 y,mo,d;
      lineartime2date(nw,&y,&mo,&d);
      // - calculate scrambled version thereof
      fScrambledNow=
        (y-1720)*12*42+
        (mo-1)*42+
        (d+7);
      if (fScrambledNow>SCRAMBLED_EXPIRY_VALUE)
        fAppExpiryStatus = LOCERR_EXPIRED; // hard expiry
    }
    #endif
    #ifdef SYSER_REGISTRATION
    // - check if timed license has expired
    if (fRegDuration && date2lineartime(fRegDuration/12+2000,fRegDuration%12+1,1)<nw)
      fAppExpiryStatus = LOCERR_EXPIRED; // timed license expiry
    #endif
    #endif
    // - create a session handle
    PDEBUGPRINTFX(DBG_PROTO,("Now creating new session"));
    sessionHP = CreateSessionHandle();
    // - enter processing for this session (will be left at EndRequest)
    if (!sessionHP->EnterSession(1000)) {
      PDEBUGPRINTFX(DBG_ERROR,("New created session cannot be entered"));
      return NULL;
    }
    try {
      // Determine session ID now
      string SessionIDString;
      if (aPredefinedSessionID) {
        SessionIDString=aPredefinedSessionID;
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
          ((((uIntPtr)sessionHP)&0xFFFFFFFF) << 16); // 0000bbbbcccc0000
        // - make a string of it
        StringObjPrintf(SessionIDString,"%lld",sid);
      }
      #ifdef CONCURRENT_DEVICES_LIMIT
      // some minor arithmetic to hide actual comparison with limit
      #ifdef SYSER_REGISTRATION
      // number of users from license or hardcoded limit, whichever is lower
      sInt32 scrambledlimit = 3*(CONCURRENT_DEVICES_LIMIT!=0 && CONCURRENT_DEVICES_LIMIT<fRegQuantity ? CONCURRENT_DEVICES_LIMIT : fRegQuantity)+42;
      #else
      // only hardcoded limit
      sInt32 scrambledlimit = 3*CONCURRENT_DEVICES_LIMIT+42;
      #endif
      #endif
      // - create session object with given ID
      sessionP = static_cast<TAgentConfig *>(fConfigP->fAgentConfigP)->CreateServerSession(sessionHP,SessionIDString.c_str());
      sessionHP->fSessionP=sessionP;
      // debug
      PDEBUGPRINTFX(DBG_HOT,(
        "Session created: local session ID='%s', not yet in session list",
        SessionIDString.c_str()
      ));
      // info
      CONSOLEPRINTF(("\nStarted new SyncML session (server id=%s)",SessionIDString.c_str()));
      // - add it to session map
      LockSessions();
      fSessions[SessionIDString]=sessionHP;
      DEBUGPRINTFX(DBG_HOT,(
        "Session ID='%s' now in session list, total # of sessions now: %ld",
        sessionP->getLocalSessionID(),
        (sInt32)fSessions.size()
      ));
      ReleaseSessions();
    }
    catch (...) {
      sessionHP->LeaveSession();
      delete sessionHP;
      throw; // re-throw
    }
  }
  // session list is not locked here
  catch (exception &e) {
    PDEBUGPRINTFX(DBG_HOT,("******** Exception: Cannot create Session: %s",e.what()));
    return NULL;
  }
  catch (...) {
    PDEBUGPRINTFX(DBG_HOT,("******** Unknown exception: Cannot create Session"));
    return NULL;
  }
  // return session pointer (session is entered)
  return sessionHP;
} // TSyncSessionDispatch::CreateAndEnterServerSession


// called by owner of Session dispatcher to signal end of
// a request. Responsible for finishing sending answers and
// cleaning up the session if needed
Ret_t TSyncSessionDispatch::EndRequest(InstanceID_t aSmlWorkspaceID, bool &aHasData, string &aRespURI, bool &aEOSession, uInt32 aReqBytes)
{
  TSyncAgent *serverSessionP=NULL; // the session
  Ret_t err;

  // In case of a totally wrong request, this method may be
  // called when no session is attached to the smlWorkspace
  aEOSession=true; // default to ending session
  try {
    err=getSmlInstanceUserData(aSmlWorkspaceID,(void **)&serverSessionP);
    if (err==SML_ERR_OK && serverSessionP) {
      // Important: instance and session must remain attached until session either continues
      //            running or is deleted.
      // Normal case: there IS a session attached
      DEBUGPRINTFX(DBG_SESSION,("Request ended with session attached, calling TSyncAgent::EndRequest"));
      if (serverSessionP->EndRequest(aHasData,aRespURI,aReqBytes)) {
        // TSyncSession::EndRequest returns true when session is done and must be removed
        PDEBUGPRINTFX(DBG_SESSION,("TSyncAgent::EndRequest returned true -> terminating and deleting session now"));
        // - take session out of session list
        LockSessions();
        TSyncSessionHandle *sessionHP = RemoveSession(serverSessionP);
        ReleaseSessions();
        // - delete session now (could cause AV in extreme case)
        if (sessionHP) {
          // - now actually delete
          if (sessionHP->EnterAndTerminateSession(0)) { // normal termination of session
            PDEBUGPRINTFX(DBG_PROTO,("Terminated and deleted session"));
          } else {
            PDEBUGPRINTFX(DBG_ERROR,("Could NOT properly terminate/delete session"));
          }
          // - delete handle (including session lock, if any)
          delete sessionHP;
        }
        else {
          PDEBUGPRINTFX(DBG_ERROR,("Very strange case: Session has no sessionhandle any more -> just delete session"));
          delete serverSessionP;
        }
        // safety: session no longer exists, make sure instance has no longer a pointer
        // (altough deleting the session should already have caused nulling the pointer by now)
        setSmlInstanceUserData(aSmlWorkspaceID,NULL);
      }
      else {
        // session is not finished, just leave lock as next message might come from another thread
        PDEBUGPRINTFX(DBG_SESSION,("TSyncAgent::EndRequest returned false -> just leave session"));
        serverSessionP->getSessionHandle()->LeaveSession();
        aEOSession=false; // session does not end
        // remove session's reference to this workspace as next request might be decoded in a different workspace
        // (BUT NOT BEFORE HERE, to make sure the link still exists for the delete case above!)
        serverSessionP->setSmlWorkspaceID(NULL);
        // Note: session remains linked from instance's userData
      }
    }
    else {
      // end of request that could not be assiged a session at all
      DEBUGPRINTFX(DBG_HOT,("Request ended with no session attached"));
    }
    #ifdef SYDEBUG
    dbgListSessions();
    #endif
    return SML_ERR_OK;
  }
  catch (exception &e) {
    aHasData=false;
    return HandleDecodingException(serverSessionP,"EndRequest",&e);
  }
  catch (...) {
    aHasData=false;
    return HandleDecodingException(serverSessionP,"EndRequest",NULL);
  }
} // TSyncSessionDispatch::EndRequest


// list currently active sessions
void TSyncSessionDispatch::dbgListSessions(void)
{
  #ifdef SYDEBUG
  string ts;
  TSyncSession *sP;
  if (PDEBUGTEST(DBG_SESSION)) {
    LockSessions();
    try {
      // View list of active sessions
      TSyncSessionHandlePContainer::iterator pos;
      PDEBUGPRINTFX(DBG_SESSION,("-------------------------------------------"));
      PDEBUGPRINTFX(DBG_SESSION,("Active Sessions (%d):",fSessions.size()));
      for (pos=fSessions.begin(); pos!=fSessions.end(); pos++) {
        sP =(*pos).second->fSessionP;
        if (sP) {
          // handle has a session
          StringObjTimestamp(ts,sP->getSessionLastUsed());
          PDEBUGPRINTFX(DBG_SESSION,(
            "- %s :Remote URI=%s, Local URI=%s, LastUsed=%s",
            (*pos).first.c_str(),
            sP->getRemoteURI(),
            sP->getLocalURI(),
            ts.c_str()
          ));
        }
        else {
          // Strange error: handle without session
          PDEBUGPRINTFX(DBG_SESSION,(
           "- %s : <error: session object already deleted>",
           (*pos).first.c_str()
         ));
        }
      }
      PDEBUGPRINTFX(DBG_SESSION,("-------------------------------------------"));
      ReleaseSessions(); // make sure list does not remain blocked
    }
    catch (...) {
      ReleaseSessions(); // make sure list does not remain blocked
    }
  }
  #endif
} // TSyncSessionDispatch::dbgListSessions


// buffer answer in the session's buffer if instance still has a session attached at all
Ret_t TSyncSessionDispatch::bufferAnswer(InstanceID_t aSmlWorkspaceID, MemPtr_t aAnswer, MemSize_t aAnswerSize)
{
  TSyncAgent *serverSessionP=NULL; // the session
  Ret_t err;

  err=getSmlInstanceUserData(aSmlWorkspaceID,(void **)&serverSessionP);
  if ((err==SML_ERR_OK) && serverSessionP) {
    // there is a session attached, buffer
    err=serverSessionP->bufferAnswer(aAnswer,aAnswerSize);
  }
  return err;
} // TSyncSessionDispatch::bufferAnswer


// get buffered answer from the session's buffer if there is any
void TSyncSessionDispatch::getBufferedAnswer(InstanceID_t aSmlWorkspaceID, MemPtr_t &aAnswer, MemSize_t &aAnswerSize)
{
  TSyncAgent *serverSessionP=NULL; // the session
  Ret_t err;

  err=getSmlInstanceUserData(aSmlWorkspaceID,(void **)&serverSessionP);
  if (err==SML_ERR_OK && serverSessionP) {
    serverSessionP->getBufferedAnswer(aAnswer,aAnswerSize);
  }
  else {
    aAnswer=NULL;
    aAnswerSize=0;
  }
} // TSyncSessionDispatch::getBufferedAnswer



/* Session abort */

// called by owner or derivate of Session dispatcher when
// session must be aborted due to error in request processing.
// Kills session currently assigned to specified workspace
// Note: may not be called when session list is already locked
void TSyncSessionDispatch::KillSessionByInstance(InstanceID_t aSmlWorkspaceID, uInt16 aStatusCode, const char *aMsg, uInt32 aErrorCode)
{
  TSyncAgent *sessionP;

  // In case of a totally bad request, this method may be
  // called when no session is attached to the smlWorkspace
  Ret_t err=getSmlInstanceUserData(aSmlWorkspaceID,(void **)&sessionP);
  if (err==SML_ERR_OK) {
    // there IS a session attached
    KillServerSession(sessionP,aStatusCode,aMsg,aErrorCode); // get rid of session
  }
  else {
    // there is no session at all
    DEBUGPRINTFX(DBG_HOT,("TSyncSessionDispatch::KillSessionByInstance: smlInstance has no session to be killed"));
  }
} // TSyncSessionDispatch::KillSessionByInstance


// remove and kill session
// Note: may not be called when session list is already locked
void TSyncSessionDispatch::KillServerSession(TSyncAgent *aSessionP, uInt16 aStatusCode, const char *aMsg, uInt32 aErrorCode)
{
  if (aSessionP) {
    LockSessions();
    // make sure session log shows the problem
    #ifdef SYDEBUG
    OBJDEBUGPRINTFX(aSessionP,DBG_ERROR,(
      "******* Warning: Terminating Session with Statuscode=%hd because: %s (Errorcode=%ld)",
      aStatusCode,
      aMsg ? aMsg : "<unknown>",
      aErrorCode
    ));
    #endif
    // remove session
    TSyncSessionHandle *sessionHP = RemoveSession(aSessionP); // remove session from session list
    ReleaseSessions();
    // terminate and delete session
    if (sessionHP) {
      // - terminate and delete session
      DEBUGPRINTFX(DBG_SESSION,("TSyncSessionDispatch::KillServerSession: terminating session first..."));
      if (sessionHP->EnterAndTerminateSession(aStatusCode)) {
        DEBUGPRINTFX(DBG_SESSION,("TSyncSessionDispatch::KillServerSession: ...terminated, now deleting session handle"));
      } else {
        DEBUGPRINTFX(DBG_ERROR,("TSyncSessionDispatch::KillServerSession: ...Session could NOT be deleted, left in memory"));
      }
      // - delete session handle itself
      delete sessionHP;
      DEBUGPRINTFX(DBG_HOT,("TSyncSessionDispatch::KillServerSession: finished"));
    }
    else {
      // - session not found
      DEBUGPRINTFX(DBG_ERROR,("TSyncSessionDispatch::KillServerSession: no session found, session pointer seems invalid"));
    }
  }
  else {
    DEBUGPRINTFX(DBG_ERROR,("TSyncSessionDispatch::KillServerSession: ******* Tried to kill NULL sessionP"));
  }
} // TSyncSessionDispatch::KillServerSession


// Handle exception happening while decoding commands for a session
Ret_t TSyncSessionDispatch::HandleDecodingException(TSyncSession *aSessionP, const char *aRoutine, exception *aExceptionP)
{
  #ifdef SYDEBUG
  // determine session name
  const char *sname = "<unknown>";
  try {
    if (aSessionP) {
      sname = aSessionP->getLocalSessionID();
    }
  }
  catch (...) {
    sname = "<BAD aSessionP, caused exception>";
    aSessionP=NULL; // prevent attempt to write to session's log
  }
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
  DEBUGPRINTFX(DBG_SESSION,("******** Exception aborts session: calling KillServerSession"));
  KillServerSession(static_cast<TSyncAgent *>(aSessionP),412,"Decoding Exception");
  // return error
  DEBUGPRINTFX(DBG_SESSION,("******** Exception: returning SML_ERR_UNSPECIFIC to abort smlProcessData"));
  return SML_ERR_UNSPECIFIC;
} // TSyncSessionDispatch::HandleDecodingException


// remove session by pointer from session list. If none found, return NULL
// NOTES:
// - must be called with session list locked!!!
// - does not throw
TSyncSessionHandle *TSyncSessionDispatch::RemoveSession(TSyncSession *aSessionP) throw()
{
  TSyncSessionHandle *foundhandleP = NULL;

  // remove session from Dispatcher
  // - locate session
  DEBUGPRINTFX(DBG_SESSION,("TSyncSessionDispatch::RemoveSession called..."));
  TSyncSessionHandlePContainer::iterator pos;
  for (pos=fSessions.begin(); pos!=fSessions.end(); ++pos) {
    TSyncSessionHandle *sessionHP = pos->second;
    if (sessionHP && (sessionHP->fSessionP == aSessionP)) {
      // - erase in list
      fSessions.erase(pos);
      DEBUGPRINTFX(DBG_SESSION,("...TSyncSessionDispatch::RemoveSession succeeded"));
      // - return handle
      foundhandleP=sessionHP;
      break; // done
    }
  }
  // - unlock access to list
  return foundhandleP;
} // TSyncSessionDispatch::RemoveSession



// eof
