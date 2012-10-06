/*
 *  File:         SyncCommand.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TSmlCommand, TXXXCmd....
 *    Wrapper classes for SyncML Commands and the associated SyncML
 *    Toolkit mechanics.
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-05-30 : luz : created
 *
 */

// includes
#include "prefix_file.h"

#include "synccommand.h"
#include "syncsession.h"

// %%%% debug hack switch
//#define DEBUG_XMLRESPONSE_FOR_WBXML 1
//#undef DEBUG_XMLRESPONSE_FOR_WBXML

// Status for Results is not needed according to SyncML 1.0.1
// specs but 9210 seems to need it.
// Note: future versions of the standard will require
// status for all commands, so this is going to be the default
#define RESULTS_SENDS_STATUS 1

// Map statuses are sent immediately, even if they will be part
// of sync-updates-to-client package (instead of map-acknowledge)
// This seems to be the normal case
#define MAP_STATUS_IMMEDIATE 1


#ifndef SYNCCOMMAND_PART1_EXCLUDE

using namespace sysync;


/* command name list, used for cmdRef */

const char * const SyncCommandNames[numSmlCommandTypes] = {
  "SyncHdr",
  "Sync",
  "Sync", // note this is actually SyncEnd, but as we send this as cmdRef, it MUST be named "Sync" as well
  "Add",
  "Alert",
  "Delete",
  "Get",
  "Put",
  "Map",
  "Results",
  "Status",
  "Replace",
  "Copy",
  "Move",
  "Sequence",
  "Atomic",
  "[unknown]"
};


/*
 * Implementation of TSmlCommand
 */

/* public TSmlCommand members */


// base constructor (to be called by all derived constructors)
TSmlCommand::TSmlCommand(
  TSmlCommandTypes aCmdType, // the command type
  bool aOutgoing,            // set if this is a outgoing command (to avoid confusion)
  TSyncSession *aSessionP,   // associated session (for callbacks)
  uInt32 aMsgID                // (optional, for receiving only) the Message ID of the command
)
{
  // save info
  fCmdType=aCmdType;
  fMsgID=aMsgID;
  fSessionP=aSessionP;
  fOutgoing=aOutgoing;
  // init
  fWaitingForStatus=0; // not yet waiting for any statuses
  fCmdID=0; // no ID yet, will be either set at issue() or read at StartProcessing()
  fNoResp=false; // respond by default
  fDontSend=false; // send by default
  fEvalMode=false; // no eval
  fPrepared=false;
  fAllowFailure=false;
  // debug
  DEBUGPRINTFX(DBG_PROTO,("Created command '%s' (%s)", getName(), fOutgoing ? "outgoing" : "incoming"));
} // TSmlCommand::TSmlCommand


// get name of command
const char *TSmlCommand::getName(void)
{
  return SyncCommandNames[fCmdType];
} // TSmlCommand::getName


#ifdef SYDEBUG
TDebugLogger *TSmlCommand::getDbgLogger(void)
{
  // commands log to session's logger
  return fSessionP ? fSessionP->getDbgLogger() : NULL;
} // TSmlCommand::getDbgLogger

uInt32 TSmlCommand::getDbgMask(void)
{
  if (!fSessionP) return 0; // no session, no debug
  return fSessionP->getDbgMask();
} // TSmlCommand::getDbgMask
#endif


TSyncAppBase *TSmlCommand::getSyncAppBase(void)
{
  return fSessionP ? fSessionP->getSyncAppBase() : NULL;
} // TSmlCommand::getSyncAppBase


// get name of certain command
const char *TSmlCommand::getNameOf(TSmlCommandTypes aCmdType)
{
  return SyncCommandNames[aCmdType];
} // TSmlCommand::getNameOf


// start processing a command
void TSmlCommand::StartProcessing(
  SmlPcdataPtr_t aCmdID, // ID of command
  Flag_t aFlags // flags of command
)
{
  // get command ID
  StrToULong(smlPCDataToCharP(aCmdID),fCmdID);
  // get noResp state
  fNoResp=(aFlags & SmlNoResp_f)!=0;
  PDEBUGPRINTFX(DBG_HOT,("Started processing Command '%s' (incoming MsgID=%ld, CmdID=%ld)%s",getName(),(long)fMsgID,(long)fCmdID,fNoResp ? ", noResp" : ""));
} // TSmlCommand::StartProcessing



// - Prepare for issuing, evaluate size of command
sInt32 TSmlCommand::evalIssue(
  uInt32 aAsCmdID,     // command ID to be used
  uInt32 aInMsgID,     // message ID in which command is being issued
  bool aNoResp // issue without wanting response
)
{
  MemSize_t res;
  bool ok;

  fPrepared=false; // force re-prepare

  // start evaluation run
  smlStartEvaluation(fSessionP->getSmlWorkspaceID());

  fEvalMode=true;
  SYSYNC_TRY {
    // - call issue to get size
    ok=issue(aAsCmdID,aInMsgID,aNoResp);
    // - back to normal mode
    fEvalMode=false;
    smlEndEvaluation(fSessionP->getSmlWorkspaceID(),&res);
    if (!ok) res=-1; // no room, error
    /* %%% hack to force unsendable-sized syncop commands:
    #ifdef RELEASE_VERSION
    #error "%%%%remove this debuh hack!!!"
    #endif
    if (dynamic_cast<TSyncOpCommand *>(this)!=NULL) {
      // always oversized
      res=0;
    }
    */
  }
  SYSYNC_CATCH (...)
    fEvalMode=false;
    smlEndEvaluation(fSessionP->getSmlWorkspaceID(),&res);
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH

  // return available space after sending this command
  return res;
} // TSmlCommand::evalIssue



void TSmlCommand::PrepareIssue(
  SmlPcdataPtr_t *aCmdID, // ID of command
  Flag_t *aFlags // flags of command
)
{
  if (!fPrepared) {
    fPrepared=true;
    // set Command ID
    if (aCmdID) {
      // remove old
      if (*aCmdID) smlFreePcdata(*aCmdID);
      // create new
      *aCmdID=newPCDataLong(fCmdID);
    }
    // add NoResp flag if requested
    if (aFlags)
      *aFlags |= fNoResp ? SmlNoResp_f : 0;
  }
  if (!fEvalMode) {
    #ifdef SYDEBUG
    if (aCmdID) {
      if (*aCmdID==NULL)
        SYSYNC_THROW(TSyncException("No Command ID set at evalIssue() but requested one at issue()"));
    }
    #endif
    // save workspace size immediately before sending to calc message size
    fBytesbefore=fSessionP->getSmlWorkspaceFreeBytes();
    #ifdef DEBUG_XMLRESPONSE_FOR_WBXML
    // %%% debug hack %%% force output to be XML
    smlSetEncoding(fSessionP->getSmlWorkspaceID(),SML_XML);
    #endif
  }
} // TSmlCommand::PrepareIssue


#ifndef USE_SML_EVALUATION

// get (approximated) message size required for sending it
uInt32 TSmlCommand::messageSize(void)
{
  // default, should be enough for most commands
  return DEFAULTCOMMANDSIZE;
}

#endif

// finalizes issuing a command (updates message size)
void TSmlCommand::FinalizeIssue(void)
{
  fSessionP->incOutgoingMessageSize(fBytesbefore-fSessionP->getSmlWorkspaceFreeBytes()); // update msg size
  #ifdef DEBUG_XMLRESPONSE_FOR_WBXML
  // %%% debug hack %%% force input to be WBXML again
  smlSetEncoding(fSessionP->getSmlWorkspaceID(),SML_WBXML);
  #endif
} // TSmlCommand::FinalizeIssue


// finalizes issuing a command (updates message size)
bool TSmlCommand::queueForResponse(void)
{
  return (!fNoResp && !fSessionP->fOutgoingNoResp);
} // TSmlCommand::queueForStatus


// returns true if command must be put to the waiting-for-status queue.
// If false, command can be deleted
bool TSmlCommand::issue(
  uInt32 aAsCmdID,     // command ID to be used
  uInt32 aInMsgID,     // message ID in which command is being issued
  bool aNoResp
)
{
  // set command ID and Message ID (for further compare with incoming statuses)
  fCmdID=aAsCmdID;
  fMsgID=aInMsgID;
  fNoResp=aNoResp;
  return fEvalMode; // evaluation ok, but base class commands cannot be issued
} // TSmlCommand::issue


// execute command (perform real actions, generate status)
// returns true if command has executed and can be deleted
bool TSmlCommand::execute(void)
{
  // non-derived execute (such as map received by client): protocol error
  TStatusCommand *statusCmdP=newStatusCommand(400);
  ISSUE_COMMAND_ROOT(fSessionP,statusCmdP);
  // done
  return false;
} // TSmlCommand::execute


// - test if command matches status
bool TSmlCommand::matchStatus(TStatusCommand *aStatusCmdP)
{
  // match if cmdID and msgID are the same
  return (
    (fMsgID==aStatusCmdP->fRefMsgID) &&
    (fCmdID==aStatusCmdP->fRefCmdID)
  );
} // TSmlCommand::matchStatus


// handle status received for previously issued command
// returns true if done, false if command must be kept in the status queue
bool TSmlCommand::handleStatus(TStatusCommand *aStatusCmdP)
{
  // base class just handles common cases
  TSyError statuscode = aStatusCmdP->getStatusCode();
  if (statuscode<200) {
    // informational
    switch (statuscode) {
      case 101:
        // in progress, wait for final status
        POBJDEBUGPRINTFX(fSessionP,DBG_HOT,("Status: 101: In progress, keep waiting for final status"));
        return false; // keep in queue
        //break;
      default:
        // unknown
        POBJDEBUGPRINTFX(fSessionP,DBG_ERROR,("Status: %hd: unknown informational status -> accepted",statuscode));
        return true;
    }
  }
  else if (statuscode<300) {
    if (statuscode==202) {
      // accepted for processing
      POBJDEBUGPRINTFX(fSessionP,DBG_PROTO,("Status: 202: accepted for processing, keep waiting for final status"));
      return false; // keep in queue
    }
    // successful
    POBJDEBUGPRINTFX(fSessionP,DBG_PROTO,("Status: %hd: successful --> accept as ok",statuscode));
    return true; // done with command
  }
  else if (statuscode<400) {
    // redirection
    // %%% - we cannot handle them, abort for now
    POBJDEBUGPRINTFX(fSessionP,DBG_ERROR,("Status: %hd: redirected --> we cannot handle this, abort session",statuscode));
    fSessionP->AbortSession(412,false,statuscode); // other party's fault: incomplete command
    return true; // done with command
  }
  else if (statuscode==418) {
    POBJDEBUGPRINTFX(fSessionP,DBG_PROTO,("Status: 418: already existed on peer --> accept as ok"));
    return true; // done with command
  }
  else if (statuscode<500) {
    // originator exception (we sent some bad stuff)
    POBJDEBUGPRINTFX(fSessionP,DBG_ERROR,("Status: %hd: originator exception",statuscode));
    if (!fAllowFailure) fSessionP->AbortSession(500,false,statuscode); // our fault
    return true; // done with command
  }
  else {
    // must be recipient exception
    POBJDEBUGPRINTFX(fSessionP,DBG_ERROR,("Status: %hd: recipient exception",statuscode));
    if (!fAllowFailure) fSessionP->AbortSession(statuscode,false,statuscode); // show other party's reason for error
    return true; // done with command
  }
  // just to make sure
  return true; // done with command
} // TSmlCommand::handleStatus



// generate status depending on fNoResp and session's fMsgNoResp
TStatusCommand *TSmlCommand::newStatusCommand(TSyError aStatusCode, const char *aStringItem)
{
  TStatusCommand *statusCmdP = new TStatusCommand(fSessionP,this,aStatusCode);
  if (aStringItem)
    statusCmdP->addItemString(aStringItem);
  return statusCmdP;
} // TSmlCommand::newStatusCommand


// generate and send status depending on fNoResp and session's fMsgNoResp
void TSmlCommand::issueStatusCommand(TSyError aStatusCode)
{
  // issuePtr can handle NULL in case no status was generated...
  fSessionP->issueRootPtr(newStatusCommand(aStatusCode));
} // TSmlCommand::issueStatusCommand



TSmlCommand::~TSmlCommand()
{
  PDEBUGPRINTFX(DBG_PROTO,("Deleted command '%s' (%s MsgID=%ld, CmdID=%ld)",getName(),fOutgoing ? "outgoing" : "incoming", (long)fMsgID,(long)fCmdID));
} // TSmlCommand::~TSmlCommand

/* end of TSmlCommand implementation */



/*
 * Implementation of TSyncHeader
 */

/* public TSyncHeader members */


// constructor for receiving Sync header
TSyncHeader::TSyncHeader(
  TSyncSession *aSessionP,              // associated session (for callbacks)
  SmlSyncHdrPtr_t aSyncHdrElementP      // associated SyncHdr content element
) :
  TSmlCommand(scmd_synchdr,false,aSessionP)
{
  // save element
  fSyncHdrElementP = aSyncHdrElementP;
} // TSyncHeader::TSyncHeader


// constructor for sending SyncHdr
TSyncHeader::TSyncHeader(
  TSyncSession *aSessionP,        // associated session (for callbacks)
  bool aOutgoingNoResp            // if true, entire message will request no responses
) :
  TSmlCommand(scmd_synchdr,true,aSessionP)
{
  // prevent two simultaneous messages
  if (fSessionP->fOutgoingStarted)
    SYSYNC_THROW(TSyncException("Tried to start new message before finishing previous"));
  // let session create the structure (using session vars for info)
  fSyncHdrElementP=fSessionP->NewOutgoingSyncHdr(aOutgoingNoResp);
  // now outgoing message IS started
  fSessionP->fOutgoingStarted=true;
} // TSyncHeader::TSyncHeader


// returns true if command must be put to the waiting-for-status queue.
// If false, command can be deleted
bool TSyncHeader::issue(
  uInt32 aAsCmdID,     // command ID to be used
  uInt32 aInMsgID,     // message ID in which command is being issued
  bool aNoResp
)
{
  // prepare basic stuff
  TSmlCommand::issue(0,aInMsgID,false);
  // now issue
  if (fSyncHdrElementP) {
    // issue command with SyncML toolkit, no CmdID or flags to set here
    PrepareIssue(NULL,NULL);
    if (!fEvalMode) {
      Ret_t err;
      #ifdef SYDEBUG
      if (fSessionP->fXMLtranslate && fSessionP->fOutgoingXMLInstance) {
        err=smlStartMessageExt(fSessionP->fOutgoingXMLInstance,fSyncHdrElementP,SmlVersionCodes[fSessionP->fSyncMLVersion]);
        if (err!=SML_ERR_OK) {
          // problem with XML translation
          PDEBUGPRINTFX(DBG_ERROR,("XML translation disabled due to sml error=%04hX",err));
          fSessionP->fXMLtranslate=false;
        }
      }
      #endif
      if ((err=smlStartMessageExt(fSessionP->getSmlWorkspaceID(),fSyncHdrElementP,SmlVersionCodes[fSessionP->fSyncMLVersion]))!=SML_ERR_OK) {
        SYSYNC_THROW(TSmlException("smlStartMessage",err));
      }
      FinalizeIssue();
      // we don't need the status structure any more, free (and NULL ptr) now
      FreeSmlElement();
    }
    else
      // just evaluate size
      return smlStartMessageExt(fSessionP->getSmlWorkspaceID(),fSyncHdrElementP,SmlVersionCodes[fSessionP->fSyncMLVersion])==SML_ERR_OK;
  }
  else {
    DEBUGPRINTFX(DBG_ERROR,("*** Tried to issue NULL synchdr"));
  }
  // header will normally receive status
  return queueForResponse();
} // TSyncHeader::issue


// handle status received for previously issued command
// returns true if done, false if command must be kept in the status queue
bool TSyncHeader::handleStatus(TStatusCommand *aStatusCmdP)
{
  // status for SyncHdr
  if (!fSessionP->handleHeaderStatus(aStatusCmdP)) {
    // session could not handle item
    return TSmlCommand::handleStatus(aStatusCmdP);
  }
  // status handled
  return true; // done with command
} // TSyncHeader::handleStatus


// execute command (perform real actions, generate status)
// returns true if command has executed and can be deleted
// NOTE: synchdr is a special case: if it returns FALSE,
//       session must be reset and synchdr must be re-excecuted
bool TSyncHeader::execute(void)
{
  const char *verDTD;
  const char *verProto;
  TStatusCommand *statusCmdP=NULL;
  sInt16 statuscode=400;
  bool hdrok=true;

  if (!fSyncHdrElementP) SYSYNC_THROW(TSyncException("empty header"));
  if (!fSessionP) SYSYNC_THROW(TSyncException("missing fSessionP"));
  // first get message ID to be able to generate statuses
  fMsgID=0; // none by default
  if (StrToULong(smlPCDataToCharP(fSyncHdrElementP->msgID),fMsgID)) {
    // got msgID, check if ok
    if (
      fMsgID==uInt32(fSessionP->fIncomingMsgID-1) &&
      fSessionP->fAllowMessageRetries // check if we want to allow this (7250 for example seems to retry messages)
    ) {
      // this seems to be a transport-level retry of the previous message
      PDEBUGPRINTFX(DBG_ERROR,("*********** WARNING: Remote resent MsgID %ld",(long)fMsgID));
      // set back incoming ID
      fSessionP->fIncomingMsgID = fMsgID;
      // -> we should resend the previous answer again.
      if (
        fSessionP->getSyncAppBase()->canBufferRetryAnswer()
      ) {
        // we can resend answers
        PDEBUGPRINTFX(DBG_ERROR,("last answer buffered -> sending it again"));
        fSessionP->fMessageRetried=true;
        return false;
      }
      else {
        // use not foolproof poor man's method
        // %%% This will fail if the previous message received has
        //     changed datastore/session/package states, but will work if
        //     the retry is within a phase (probable case for long sessions)
        PDEBUGPRINTFX(DBG_ERROR,("No buffered answer to resend -> just process msg again (WARNING: possibly messes up session state)"));
      }
    }
    if (fMsgID<uInt32(fSessionP->fIncomingMsgID)) {
      // bad Message ID (lower than previous): forget previous session, start new one
      PDEBUGPRINTFX(DBG_ERROR,("Bad incoming MsgID %ld, expected >=%ld -> Aborting previous Session, starting new",(long)fMsgID,(long)fSessionP->fIncomingMsgID));
      return false; // session must be restarted, this command re-executed
    }
  }
  else {
    // header without session ID is bad
    statuscode=500;
    statusCmdP=newStatusCommand(statuscode);
    // log file entry
    PDEBUGPRINTFX(DBG_ERROR,("Missing incoming MsgID -> Aborting Session"));
    hdrok=false; // cannot start
  }
  // Assign session-var anyway
  fSessionP->fIncomingMsgID=fMsgID;
  // now check header for conformance
  SYSYNC_TRY {
    // get info out of SyncHdr
    if (hdrok) {
      // get noResp flag (which is valid for the entire message)
      fNoResp=(fSyncHdrElementP->flags & SmlNoResp_f)!=0;
      fSessionP->fMsgNoResp=fNoResp; // copy to session flag
      // test SyncML version compatibility
      verProto=smlPCDataToCharP(fSyncHdrElementP->proto);
      verDTD=smlPCDataToCharP(fSyncHdrElementP->version);
      sInt16 ver;
      // find version
      for (ver=1; ver<numSyncMLVersions; ver++) {
        if (strcmp(verProto,SyncMLVerProtoNames[ver])==0) {
          // known version found
          // - set it only for server, client keeps preset version (will change it when
          //   a 513 is detected)
          if (IS_SERVER) fSessionP->fSyncMLVersion=(TSyncMLVersions)ver;
          break;
        }
      }
      // - Protocol Version
      TSyncMLVersions maxver = fSessionP->getSessionConfig()->fMaxSyncMLVersionSupported;
      TSyncMLVersions minver = fSessionP->getSessionConfig()->fMinSyncMLVersionSupported;
      if (
        ver<minver ||
        ver>=numSyncMLVersions ||
        ver>maxver ||
        (IS_SERVER && fSessionP->fSyncMLVersion!=syncml_vers_unknown && fSessionP->fSyncMLVersion!=ver)
      ) {
        // unsupported protocol version (or different than in first message): Status 513
        // - Make sure we have a valid SyncML version
        if (
          fSessionP->fSyncMLVersion==syncml_vers_unknown ||
          fSessionP->fSyncMLVersion>maxver
        ) {
          // use highest version we know for the answering message
          fSessionP->fSyncMLVersion = maxver;
        }
        else if (fSessionP->fSyncMLVersion<minver) {
          // use lowest enabled version for answering message
          fSessionP->fSyncMLVersion = minver;
        }
        // - Set status
        statuscode=513;
        statusCmdP=newStatusCommand(statuscode);
        // - add version(s) we support in data item
        string vs;
        for (sInt16 v=minver; v<=maxver; v++) {
          if (!vs.empty()) vs+=", ";
          vs+=SyncMLVerProtoNames[v];
        }
        statusCmdP->addItemString(vs.c_str());
        // - log file entry
        PDEBUGPRINTFX(DBG_ERROR,("Unsupported or changing verProto %s -> Aborting Session",verProto));
        hdrok=false; // bad header
      }
      // protocol version known, check if DTD matches
      else if (strcmp(verDTD,SyncMLVerDTDNames[ver])!=0) {
        // wrong DTD version for this protocol version: Status 505
        statuscode=505;
        statusCmdP=newStatusCommand(statuscode);
        statusCmdP->addItemString(SyncMLVerDTDNames[ver]);
        // log file entry
        PDEBUGPRINTFX(DBG_ERROR,(
          "Wrong verDTD %s, expected %s -> Aborting Session",
          verDTD,
          SyncMLVerDTDNames[ver]
        ));
        hdrok=false; // bad header
      }
      // if ok so far, continue header processing
      if (hdrok) {
        PDEBUGPRINTFX(DBG_HOT,("Started Processing of message #%ld (%s)",(long)fSessionP->fIncomingMsgID,SyncMLVerProtoNames[ver]));
        // take a look at SyncHdr Meta
        SmlMetInfMetInfPtr_t metaP=smlPCDataToMetInfP(fSyncHdrElementP->meta);
        if (metaP) {
          // max (outgoing) message size
          if (metaP->maxmsgsize) {
            smlPCDataToLong(metaP->maxmsgsize,fSessionP->fMaxOutgoingMsgSize);
            PDEBUGPRINTFX(DBG_REMOTEINFO,("MaxMsgSize found in SyncHdr: %ld -> set for outgoing msgs",(long)fSessionP->fMaxOutgoingMsgSize));
          }
          // max (outgoing) object size (SyncML 1.1 only)
          if (metaP->maxobjsize) {
            smlPCDataToLong(metaP->maxobjsize,fSessionP->fMaxOutgoingObjSize);
            PDEBUGPRINTFX(DBG_REMOTEINFO,("MaxObjSize found in SyncHdr: %ld",(long)fSessionP->fMaxOutgoingObjSize));
          }
        }
        // call server/client specific message start in derived classes
        statusCmdP=newStatusCommand(200); // prepare OK default status (will possibly be modified by MessageStarted())
        hdrok=fSessionP->MessageStarted(fSyncHdrElementP,*statusCmdP);
      }
      else {
        //#warning "comment the next line to have server respond like pre-1.0.8.29"
        // call client/server specific message start routine, but with error flag
        // Note: we already have a status command (containing an error)
        fSessionP->MessageStarted(fSyncHdrElementP,*statusCmdP,true);
      }
    }
    // complete and send status, if any
    if (statusCmdP) {
      statusCmdP->addTargetRef(fSessionP->fLocalURI.c_str());
      statusCmdP->addSourceRef(fSessionP->fRemoteURI.c_str());
      // issue as SyncHdr status (if successful status)
      TStatusCommand *cmdP = statusCmdP; statusCmdP=NULL;
      statuscode=cmdP->getStatusCode(); // get actual status code
      fSessionP->issueRootPtr(cmdP,false,statuscode==200);
    }
    // set session vars according to header success or failure
    // - ignore all further incoming commands when header is not ok
    //   NOTE: bad cred will NOT (any longer, SyncFest #5) be flagged as bad header
    //         but instead MessageStarted may have aborted command processing
    if (!hdrok) fSessionP->AbortSession(400,true); // bad request
    // free this one now, is not needed any more
    FreeSmlElement();
  }
  SYSYNC_CATCH (...)
    // make sure owned objects in local scope are deleted
    if (statusCmdP) delete statusCmdP;
    // re-throw
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
  // done with command, delete it now: return true
  return true;
} // TSyncHeader::execute



void TSyncHeader::FreeSmlElement(void)
{
  // remove SyncML toolkit element(s)
  FREEPROTOELEMENT(fSyncHdrElementP);
} // TSyncHeader::FreeSmlElement


TSyncHeader::~TSyncHeader()
{
  // free command elements, if any (use explicit invocation as this is a destructor)
  TSyncHeader::FreeSmlElement();
} // TSyncHeader::~TSyncHeader


/* end of TSyncHeader implementation */



/*
 * Implementation of TSyncCommand
 */


// constructor for sending Sync Command
TSyncCommand::TSyncCommand(
  TSyncSession *aSessionP,              // associated session (for callbacks)
  TLocalEngineDS *aLocalDataStoreP,    // local datastore
  TRemoteDataStore *aRemoteDataStoreP   // remote datastore
) :
  TSmlCommand(scmd_sync,true,aSessionP),
  fInterruptedCommandP(NULL)
{
  // save params
  fLocalDataStoreP=aLocalDataStoreP;
  fRemoteDataStoreP=aRemoteDataStoreP;
  // create internal sync element
  fSyncElementP = SML_NEW(SmlSync_t);
  // set proto element type to make it auto-disposable
  fSyncElementP->elementType=SML_PE_SYNC_START;
  // Cmd ID is now empty (will be set when issued)
  fSyncElementP->cmdID=NULL;
  // default to no flags (noResp is set at issue, if at all)
  fSyncElementP->flags=0;
  // set source and target
  fSyncElementP->target=newLocation(fRemoteDataStoreP->getFullName()); // remote is target for Sync command
  fSyncElementP->source=newLocation(fLocalDataStoreP->getRemoteViewOfLocalURI()); // local is source of Sync command
  // no optional elements for now
  fSyncElementP->cred=NULL; // %%% no database level auth yet at all
  fSyncElementP->meta=NULL; // %%% no search grammar for now
  // add number of changes for SyncML 1.1 if remote supports it
  fSyncElementP->noc=NULL; // default to none
  if (aSessionP->fRemoteWantsNOC) {
    sInt32 noc = fLocalDataStoreP->getNumberOfChanges();
    if (noc>=0) {
      // we have a valid NOC value, add it
      fSyncElementP->noc=newPCDataLong(noc);
    }
  }
  // not yet in progress (as not yet issued)
  fInProgress=false;
} // TSyncCommand::TSyncCommand


// constructor for receiving Sync Command
TSyncCommand::TSyncCommand(
  TSyncSession *aSessionP,              // associated session (for callbacks)
  uInt32 aMsgID,                          // the Message ID of the command
  SmlSyncPtr_t aSyncElementP           // associated Sync content element
) :
  TSmlCommand(scmd_sync,false,aSessionP,aMsgID),
  fInterruptedCommandP(NULL)
{
  // save sync element
  fSyncElementP = aSyncElementP;
  fInProgress=false; // just in case...
  // no params
  fLocalDataStoreP=NULL;
  fRemoteDataStoreP=NULL;
} // TSyncCommand::TSyncCommand


// handle status received for previously issued command
// returns true if done, false if command must be kept in the status queue
bool TSyncCommand::handleStatus(TStatusCommand *aStatusCmdP)
{
  // catch those codes that do not abort entire session
  TSyError statuscode = aStatusCmdP->getStatusCode();
  bool handled=false;
  switch (statuscode) {
    case 404: // datastore not found
    case 403: // forbidden
    case 406: // bad mode
    case 415: // type(s) not supported
    case 422: // bad CGI
    case 510: // datastore error
    case 512: // sync failed
    case 514: // cancelled
      if (fLocalDataStoreP) {
        // there is a local datastore to abort
        fLocalDataStoreP->engAbortDataStoreSync(statuscode,false); // remote problem
      }
      if (fInProgress) {
        // make sure sync command is finished now
        fInProgress=false;
        // Note that setting fInProgress to false
        // is only completely safe in issue().
        // Here it is allowed as we KNOW that syncCommand
        // is root level, so session's handleStatus()
        // will properly clear the fInterruptedCommandP
        // (this would not work if this command was nested)
      }
      handled = true;
      break;
    default:
      handled=TSmlCommand::handleStatus(aStatusCmdP);
      break;
  }
  return handled;
} // TSyncCommand::handleStatus


// mark any syncitems (or other data) for resume. Called for pending commands
// when a Suspend alert is received or whenever a resumable state must be saved
void TSyncCommand::markPendingForResume(TLocalEngineDS *aForDatastoreP, bool aUnsent)
{
  // only act if this is for our local datastore and unsent
  // (sent ones will be in the status wait queue, and will be found there)
  if (aUnsent && fLocalDataStoreP==aForDatastoreP) {
    fSessionP->markPendingForResume(
      fNextMessageCommands,
      fInterruptedCommandP,
      fLocalDataStoreP
    );
  }
} // TSyncCommand::markPendingForResume


// analyze command (but do not yet execute)
bool TSyncCommand::analyze(TPackageStates aPackageState)
{
  TSmlCommand::analyze(aPackageState);
  // get Command ID and flags
  if (fSyncElementP) {
    StartProcessing(fSyncElementP->cmdID,fSyncElementP->flags);
    return true;
  }
  else return false; // no proto element, bad command
} // TSyncCommand::analyze


// execute command (perform real actions, generate status)
// returns true if command has executed and can be deleted
bool TSyncCommand::execute(void)
{
  TStatusCommand *statusCmdP=NULL;
  bool queueforlater = false;

  if (fSyncElementP)
  SYSYNC_TRY {
    // determine database to be synced (target LocURI)
    // - generate default OK status
    statusCmdP = newStatusCommand(200);
    // - add source and target refs from item
    statusCmdP->addSourceRef(smlSrcTargLocURIToCharP(fSyncElementP->source));
    statusCmdP->addTargetRef(smlSrcTargLocURIToCharP(fSyncElementP->target));
    PDEBUGPRINTFX(DBG_HOT,(
      "Processing Sync, Source='%s', Target='%s'",
      smlSrcTargLocURIToCharP(fSyncElementP->source),
      smlSrcTargLocURIToCharP(fSyncElementP->target)
    ));
    // - check for MaxObjSize here
    SmlMetInfMetInfPtr_t metaP=smlPCDataToMetInfP(fSyncElementP->meta);
    if (metaP && metaP->maxobjsize) {
      smlPCDataToLong(metaP->maxobjsize,fSessionP->fMaxOutgoingObjSize);
      PDEBUGPRINTFX(DBG_REMOTEINFO,("MaxObjSize found in Sync command: %ld",(long)fSessionP->fMaxOutgoingObjSize));
    }
    // - let session do the processing
    queueforlater=false;
    fSessionP->processSyncStart(
      fSyncElementP,
      *statusCmdP,
      queueforlater // will be set if command must be queued for later re-execution
    );
    if (queueforlater) {
      // we don't need the status now
      delete statusCmdP;
    }
    else {
      // Sync command execution completed, send (or save) status now
      #ifdef SYNCSTATUS_AT_SYNC_CLOSE
      // %%% don't send, just save for being sent at </Sync>
      fSessionP->fSyncCloseStatusCommandP=statusCmdP;
      #else
      // - issue status for item
      ISSUE_COMMAND_ROOT(fSessionP,statusCmdP);
      #endif
      /* %%% no need to abort session, failing sync command stops sync with this
             datastore anyway
      // make sure session gets aborted when Sync is not successful
      if (!ok) {
        fSessionP->AbortSession(500,true);
      }
      */
      // free element
      FreeSmlElement();
    }
  }
  SYSYNC_CATCH (...)
    // make sure owned objects in local scope are deleted
    if (statusCmdP) delete statusCmdP;
    // re-throw
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
  // return true if command has fully executed
  return !queueforlater;
} // TSyncCommand::execute


// returns true if command must be put to the waiting-for-status queue.
// If false, command can be deleted
bool TSyncCommand::issue(
  uInt32 aAsCmdID,     // command ID to be used
  uInt32 aInMsgID,     // message ID in which command is being issued
  bool aNoResp
)
{
  // prepare basic stuff
  TSmlCommand::issue(aAsCmdID,aInMsgID,aNoResp);
  // now issue
  if (fSyncElementP) {
    // generate (first) opening (or evaluate)
    if (fEvalMode) return generateOpen();
    generateOpen();
    // generate commands, update fInProgress
    generateCommandsAndClose();
    // Make sure this is not counted as command (will be counted in issuePtr(), so
    // decrement here). Note that this must be done AFTER calling generateCommandsAndClose(),
    // because generating needs the correct count (and this <sync> is not yet included by now!)
    fSessionP->fOutgoingCmds--; // we're now one below the real count, but that will be compensated when returning to issuePtr
  }
  else {
    DEBUGPRINTFX(DBG_ERROR,("*** Tried to issue NULL sync"));
  }
  // return true if command must be queued for status/result response reception
  return queueForResponse();
} // TSyncCommand::issue


// - test if completely issued (must be called after issue() and continueIssue())
bool TSyncCommand::finished(void)
{
  // not finished as long there are more syncOps to send;
  // incoming are always finished after executing
  return (!fInProgress || !fOutgoing);
} // TSyncCommand::finished


// generate opening <Sync> bracket
bool TSyncCommand::generateOpen(void)
{
  // issue command Start <Sync> with SyncML toolkit
  fPrepared=false; // force re-preparing in all cases (as this might be continuing a command)
  PrepareIssue(&fSyncElementP->cmdID,&fSyncElementP->flags);
  if (!fEvalMode) {
    #ifdef SYDEBUG
    if (fSessionP->fXMLtranslate && fSessionP->fOutgoingXMLInstance)
      smlStartSync(fSessionP->fOutgoingXMLInstance,fSyncElementP);
    #endif
    Ret_t err;
    if ((err=smlStartSync(fSessionP->getSmlWorkspaceID(),fSyncElementP))!=SML_ERR_OK) {
      SYSYNC_THROW(TSmlException("smlStartSync",err));
    }
    FinalizeIssue();
    // show debug
    PDEBUGBLOCKFMT(("sync","Opened Sync command bracket",
      "Reopen=%s|SourceURI=%s|TargetURI=%s|IncomingMsgID=%ld|CmdID=%ld",
      fInProgress ? "yes" : "no",
      smlSrcTargLocURIToCharP(fSyncElementP->source),
      smlSrcTargLocURIToCharP(fSyncElementP->target),
      (long)fMsgID,
      (long)fCmdID
    ));
    PDEBUGPRINTFX(DBG_HOT,(
      "%sOpened <Sync> bracket, Source='%s', Target='%s' as (outgoing MsgID=%ld, CmdID=%ld)",
      fInProgress ? "Re-" : "",
      smlSrcTargLocURIToCharP(fSyncElementP->source),
      smlSrcTargLocURIToCharP(fSyncElementP->target),
      (long)fMsgID,
      (long)fCmdID
    ));
    // calculate (approx) max free space for data
    fSessionP->fMaxRoomForData =
      fSessionP->getSmlWorkspaceFreeBytes()-fSessionP->getNotUsableBufferBytes()- // what is free
      DEFAULTCOMMANDSIZE; // minus a standard command size
    // now we are in progress
    fInProgress=true;
    return true;
  }
  else
    return smlStartSync(fSessionP->getSmlWorkspaceID(),fSyncElementP)==SML_ERR_OK;
} // TSyncCommand::generateOpen


// generate commands for inside of the <Sync> bracket
void TSyncCommand::generateCommandsAndClose(void)
{
  // generate new commands only if message is not full already
  if (!fSessionP->outgoingMessageFull()) {
    // check for unassigned fLocalDataStoreP as this seems to happen sometimes in the cmdline client
    PPOINTERTEST(fLocalDataStoreP,("Warning: fLocalDataStoreP==NULL, cannot generate commands -> empty <sync> command"));
    if (fLocalDataStoreP) {
      fInProgress = !(
        fLocalDataStoreP->engGenerateSyncCommands
        (
          fNextMessageCommands,
          fInterruptedCommandP
        )
      );
    }
  }
  // issue command End </Sync> with SyncML toolkit
  // - save workspace size immediately before sending to calc message size
  fBytesbefore=fSessionP->getSmlWorkspaceFreeBytes();
  #ifdef SYDEBUG
  if (fSessionP->fXMLtranslate && fSessionP->fOutgoingXMLInstance)
    smlEndSync(fSessionP->fOutgoingXMLInstance);
  #endif
  Ret_t err;
  if ((err=smlEndSync(fSessionP->getSmlWorkspaceID()))!=SML_ERR_OK) {
    SYSYNC_THROW(TSmlException("smlEndSync",err));
  }
  FinalizeIssue();
  PDEBUGPRINTFX(DBG_HOT,(
    "Closed </Sync> bracket, %sfinal",
    fInProgress ? "NOT " : ""
  ));
  PDEBUGENDBLOCK("sync");
} // TSyncCommand::generateCommandsAndClose


// - continue issuing command. This is called by session at start of new message
//   when finished() returned false after issue()/continueIssue()
//   (which causes caller of finished() to put command into fInteruptedCommands queue)
//   returns true if command should be queued for status (again?)
//   NOTE: continueIssue must make sure that it gets a new CmdID/MsgID because
//   the command itself is issued multiple times
bool TSyncCommand::continueIssue(bool &aNewIssue)
{
  aNewIssue=false; // never issue anew!
  if (!fInProgress) return false; // done, don't queue for status again
  // command is in progress, re-open a <sync> bracket in this message
  // - get new CmdID/MsgID
  fCmdID = fSessionP->getNextOutgoingCmdID();
  fMsgID = fSessionP->getOutgoingMsgID();
  // - now issue open
  generateOpen();
  // first try to execute queued sub-commands that could not be sent in last message
  fSessionP->ContinuePackage(
    fNextMessageCommands,
    fInterruptedCommandP
  );
  // then generate more commands if needed (updates fInProgress)
  generateCommandsAndClose();
  // new sync command must be queued for status again
  return true;
} // TSyncCommand::continueIssue


void TSyncCommand::FreeSmlElement(void)
{
  // remove SyncML toolkit element(s)
  FREEPROTOELEMENT(fSyncElementP);
} // TResultsCommand::FreeSmlElement


TSyncCommand::~TSyncCommand()
{
  // free command elements, if any (use explicit invocation as this is a destructor)
  TSyncCommand::FreeSmlElement();
  // forget any queued sub commands
  TSmlCommandPContainer::iterator pos;
  for (pos=fNextMessageCommands.begin(); pos!=fNextMessageCommands.end(); ++pos) {
    // show that command was not sent
    DEBUGPRINTFX(DBG_ERROR,("Never sent prepared Sub-Command '%s', (outgoing MsgID=%ld, CmdID=%ld)",
      (*pos)->getName(),
      (long)(*pos)->getMsgID(),
      (long)(*pos)->getCmdID()
    ));
    // delete
    delete *pos;
  }
  fNextMessageCommands.clear(); // clear list
  // - interrupted command
  // NOTE: interrupted subcommands may NOT exist in any of the main command
  //       queues, because these OWN the commands and will delete them
  //       at ResetSession().
  if (fInterruptedCommandP) {
    // show that command was not sent
    DEBUGPRINTFX(DBG_ERROR,("Never finished interrupted sub-command '%s', (outgoing MsgID=%ld, CmdID=%ld)",
      fInterruptedCommandP->getName(),
      (long)fInterruptedCommandP->getMsgID(),
      (long)fInterruptedCommandP->getCmdID()
    ));
    delete fInterruptedCommandP;
    fInterruptedCommandP=NULL;
  }
} // TSyncCommand::~TSyncCommand


/* end of TSyncCommand implementation */


/*
 * Implementation of TSyncEndCommand
 */


// constructor for receiving Sync Command
TSyncEndCommand::TSyncEndCommand(
  TSyncSession *aSessionP,              // associated session (for callbacks)
  uInt32 aMsgID                         // the Message ID of the command
) :
  TSmlCommand(scmd_syncend,false,aSessionP,aMsgID)
{
  // nop so far
} // TSyncEndCommand::TSyncEndCommand


// execute command (perform real actions, generate status)
// returns true if command has executed and can be deleted
bool TSyncEndCommand::execute(void)
{
  bool queueforlater=false;

  // let session do the appropriate processing
  fSessionP->processSyncEnd(queueforlater);
  // return true if command has fully executed
  return !queueforlater;
} // TSyncEndCommand::execute


TSyncEndCommand::~TSyncEndCommand()
{
} // TSyncEndCommand::~TSyncEndCommand


/* end of TSyncEndCommand implementation */



/*
 * Implementation of TAlertCommand
 */


// constructor for sending Alert
TAlertCommand::TAlertCommand(
  TSyncSession *aSessionP,              // associated session (for callbacks)
  TLocalEngineDS *aLocalDataStoreP,     // local datastore
  uInt16 aAlertCode                     // Alert code to send
) :
  TSmlCommand(scmd_alert,true,aSessionP)
{
  // save datastore
  fLocalDataStoreP = aLocalDataStoreP;
  // save Alert Code
  fAlertCode = aAlertCode;
  // create internal alert element
  fAlertElementP = SML_NEW(SmlAlert_t);
  // set proto element type to make it auto-disposable
  fAlertElementP->elementType=SML_PE_ALERT;
  // Cmd ID is now empty (will be set when issued)
  fAlertElementP->cmdID=NULL;
  // default to no flags (noResp is set at issue, if at all)
  fAlertElementP->flags=0;
  // data is alert code
  fAlertElementP->data=newPCDataLong(fAlertCode);
  // no optional elements for now
  fAlertElementP->cred=NULL;
  fAlertElementP->itemList=NULL;
} // TAlertCommand::TAlertCommand


// constructor for receiving Alert
TAlertCommand::TAlertCommand(
  TSyncSession *aSessionP,              // associated session (for callbacks)
  uInt32 aMsgID,                          // the Message ID of the command
  SmlAlertPtr_t aAlertElementP          // associated Alert content element
) :
  TSmlCommand(scmd_alert,false,aSessionP,aMsgID)
{
  // save alert element
  fAlertElementP = aAlertElementP;
  // no params
  fLocalDataStoreP=NULL;
} // TAlertCommand::TAlertCommand


// analyze command (but do not yet execute)
bool TAlertCommand::analyze(TPackageStates aPackageState)
{
  TSmlCommand::analyze(aPackageState);
  // get Command ID and flags
  if (fAlertElementP) {
    StartProcessing(fAlertElementP->cmdID,fAlertElementP->flags);
    return true;
  }
  else return false; // no proto element, bad command
} // TAlertCommand::analyze


// execute command (perform real actions, generate status)
// returns true if command has executed and can be deleted
bool TAlertCommand::execute(void)
{
  TStatusCommand *statusCmdP=NULL;
  TSmlCommand *alertresponsecmdP=NULL;

  SYSYNC_TRY {
    // get alert code
    sInt32 temp;
    if (!smlPCDataToLong(fAlertElementP->data,temp)) {
      // non-numeric alert
      PDEBUGPRINTFX(DBG_ERROR,(
        "Non-Integer Alert Data: '%s', cannot handle",
        smlPCDataToCharP(fAlertElementP->data)
      ));
      statusCmdP = newStatusCommand(400,"Alert Data not understood");
    }
    else {
      fAlertCode=temp;
      PDEBUGPRINTFX(DBG_HOT,(
        "Code=%hd. %s Cred. Analyzing Items",
        fAlertCode,
        fAlertElementP->cred ? "has" : "No"
      ));
      // intercept special codes
      if (fAlertCode==221) {
        // %%% tdb
        PDEBUGPRINTFX(DBG_ERROR,("*********** RESULT ALERT 221 received in bad context"));
        statusCmdP = newStatusCommand(400,"221 Alert in bad context received");
      } else if (fAlertCode==222) {
        // request next message in packet
        PDEBUGPRINTFX(DBG_HOT,("Next Message in Packet Alert (222): flag sending waiting commands"));
        // - acknowledge 222 alert
        statusCmdP = newStatusCommand(200);
        // - add source and target refs
        statusCmdP->addTargetRef(fSessionP->getLocalURI());
        statusCmdP->addSourceRef(fSessionP->getRemoteURI());
        // - issue status for item (but not being sent alone with SyncHdr status)
        //   (treat it like ok-synchdr-status = NOT to be sent if there's no real command following)
        { TSmlCommand* p=statusCmdP; statusCmdP=NULL; fSessionP->issueRootPtr(p,false,true); }
        // - signal occurrence of 222 alert to session
        fSessionP->nextMessageRequest();
      } else {
        // normal alert, walk through items
        SmlItemListPtr_t nextitemP = fAlertElementP->itemList;
        while (nextitemP) {
          if (nextitemP->item) {
            // - check for MaxObjSize here
            SmlMetInfMetInfPtr_t metaP=smlPCDataToMetInfP(nextitemP->item->meta);
            if (metaP && metaP->maxobjsize) {
              smlPCDataToLong(metaP->maxobjsize,fSessionP->fMaxOutgoingObjSize);
              PDEBUGPRINTFX(DBG_REMOTEINFO,("MaxObjSize found in Alert command: %ld",(long)fSessionP->fMaxOutgoingObjSize));
            }
            // process alert item with common code
            // - generate default OK status
            statusCmdP = newStatusCommand(200);
            // - add source and target refs from item
            statusCmdP->addSourceRef(smlSrcTargLocURIToCharP(nextitemP->item->source));
            statusCmdP->addTargetRef(smlSrcTargLocURIToCharP(nextitemP->item->target));
            PDEBUGPRINTFX(DBG_HOT,(
              "- Processing Alert Item (code=%hd), Source='%s', Target='%s'",
              fAlertCode,
              smlSrcTargLocURIToCharP(nextitemP->item->source),
              smlSrcTargLocURIToCharP(nextitemP->item->target)
            ));
            // - let session process the alert item
            alertresponsecmdP =
              fSessionP->processAlertItem(
                fAlertCode,
                nextitemP->item,
                fAlertElementP->cred,
                *statusCmdP,
                fLocalDataStoreP // receives involved datastore (Note: if Alert cmd has multiple items, this will finally contain only the datastore of the last item, others might differ)
              );
            // - issue status for item
            ISSUE_COMMAND_ROOT(fSessionP,statusCmdP);
            // - issue result (e.g. acknowledge alert) for item, if any
            if (alertresponsecmdP) {
              fSessionP->queueForIssueRoot(alertresponsecmdP);
            }
          }
          // next
          nextitemP=nextitemP->next;
        } // while
      } // normal alert code with item
    } // numeric alert code
    // free element
    FreeSmlElement();
  }
  SYSYNC_CATCH (...)
    // make sure owned objects in local scope are deleted
    if (statusCmdP) delete statusCmdP;
    if (alertresponsecmdP) delete alertresponsecmdP;
    // re-throw
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
  // done with command, delete it now: return true
  return true;
} // TAlertCommand::execute


// returns true if command must be put to the waiting-for-status queue.
// If false, command can be deleted
bool TAlertCommand::issue(
  uInt32 aAsCmdID,     // command ID to be used
  uInt32 aInMsgID,     // message ID in which command is being issued
  bool aNoResp
)
{
  // prepare basic stuff
  TSmlCommand::issue(aAsCmdID,aInMsgID,aNoResp);
  // now issue
  if (fAlertElementP) {
    // issue command with SyncML toolkit
    PrepareIssue(&fAlertElementP->cmdID,&fAlertElementP->flags);
    if (!fEvalMode) {
      #ifdef SYDEBUG
      if (fSessionP->fXMLtranslate && fSessionP->fOutgoingXMLInstance)
        smlAlertCmd(fSessionP->fOutgoingXMLInstance,fAlertElementP);
      #endif
      Ret_t err;
      if ((err=smlAlertCmd(fSessionP->getSmlWorkspaceID(),fAlertElementP))!=SML_ERR_OK) {
        SYSYNC_THROW(TSmlException("smlAlertCmd",err));
      }
      FinalizeIssue();
      // show debug
      #ifdef SYDEBUG
      PDEBUGPRINTFX(DBG_HOT,("Alert Code %s sent",
        smlPCDataToCharP(fAlertElementP->data)
      ));
      // normal alert, walk through items
      SmlItemListPtr_t nextitemP = fAlertElementP->itemList;
      while (nextitemP) {
        if (nextitemP->item) {
          // show alert item
          PDEBUGPRINTFX(DBG_HOT,(
            "- Alert Item: Source='%s', Target='%s'",
            smlSrcTargLocURIToCharP(nextitemP->item->source),
            smlSrcTargLocURIToCharP(nextitemP->item->target)
          ));
        }
        // next
        nextitemP=nextitemP->next;
        /// @note %%% we should not send more than one item for now!
        if (nextitemP)
          DEBUGPRINTFX(DBG_ERROR,("More than one item - handleStatus is not prepared for that!"));
      } // while
      #endif
      // we don't need the status structure any more, free (and NULL ptr) now
      FreeSmlElement();
    }
    else
      return smlAlertCmd(fSessionP->getSmlWorkspaceID(),fAlertElementP)==SML_ERR_OK;
  }
  else {
    DEBUGPRINTFX(DBG_ERROR,("*** Tried to issue NULL alert"));
  }
  // return true if command must be queued for status/result response reception
  return queueForResponse();
} // TAlertCommand::issue


bool TAlertCommand::statusEssential(void)
{
  // Alert 222 status is not essential
  return !(fAlertCode==222);
} // TAlertCommand::statusEssential


// handle status received for previously issued command
// returns true if done, false if command must be kept in the status queue
bool TAlertCommand::handleStatus(TStatusCommand *aStatusCmdP)
{
  /// @note: This is a simplified implementation which KNOWS that
  ///   we do not send Alerts with more than one item. If we did one
  ///   day, we'd have to extend this implementation.
  // if this alert command was created by a datastore, let it handle it
  if (fLocalDataStoreP) {
    // pass to local datastore
    if (fLocalDataStoreP->engHandleAlertStatus(aStatusCmdP->getStatusCode()))
      return true; // datastore handled it
  }
  // let SmlCommand handle it
  return TSmlCommand::handleStatus(aStatusCmdP);
} // TAlertCommand::handleStatus


// add a String Item to the alert
void TAlertCommand::addItemString(
  const char *aItemString // item string to be added
)
{
  if (fAlertElementP && aItemString) {
    addItem(newStringDataItem(aItemString));
  }
} // TAlertCommand::addItemString


// add an Item to the alert
void TAlertCommand::addItem(
  SmlItemPtr_t aItemP // existing item data structure, ownership is passed to Alert
)
{
  if (fAlertElementP)
    addItemToList(aItemP,&(fAlertElementP->itemList));
} // TAlertCommand::addItem


void TAlertCommand::FreeSmlElement(void)
{
  // remove SyncML toolkit element(s)
  FREEPROTOELEMENT(fAlertElementP);
} // TResultsCommand::FreeSmlElement


TAlertCommand::~TAlertCommand()
{
  // free command elements, if any (use explicit invocation as this is a destructor)
  TAlertCommand::FreeSmlElement();
} // TAlertCommand::~TAlertCommand


/* end of TAlertCommand implementation */





/*
 * Implementation of TUnimplementedCommand
 */


// constructor for receiving command
TUnimplementedCommand::TUnimplementedCommand(
  TSyncSession *aSessionP,        // associated session (for callbacks)
  uInt32 aMsgID,                    // the Message ID of the command
  SmlPcdataPtr_t aCmdID,          // command ID (as contents are unknown an cannot be analyzed)
  Flag_t aFlags,                  // flags to get fNoResp
  TSmlCommandTypes aCmdType,      // command type (for name)
  void *aContentP,                // associated command content element
  TSyError aStatusCode                 // status code to be returned on execution
) :
  TSmlCommand(aCmdType,false,aSessionP,aMsgID)
{
  // get command ID
  StrToULong(smlPCDataToCharP(aCmdID),fCmdID);
  // save noResp
  fNoResp=(aFlags & SmlNoResp_f)!=0;
  // forget element
  smlFreeProtoElement(aContentP);
  // save status type
  fStatusCode=aStatusCode;
} // TUnimplementedCommand::TUnimplementedCommand


// execute command (perform real actions, generate status)
// returns true if command has executed and can be deleted
bool TUnimplementedCommand::execute(void)
{
  TStatusCommand *statusCmdP=NULL;

  DEBUGPRINTFX(DBG_HOT,("UNIMPLEMENTED, started dummy processing, (incoming MsgID=%ld, CmdID=%ld)",(long)fMsgID,(long)fCmdID));
  SYSYNC_TRY {
    statusCmdP = newStatusCommand(fStatusCode);
    statusCmdP->addItemString("Unimplemented Command");
    ISSUE_COMMAND_ROOT(fSessionP,statusCmdP);
  }
  SYSYNC_CATCH (...)
    // make sure owned objects in local scope are deleted
    if (statusCmdP) delete statusCmdP;
    // re-throw
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
  // done with command, delete it now: return true
  return true;
} // TUnimplementedCommand::execute


/* end of TUnimplementedCommand implementation */



/*
 * Implementation of TSyncOpCommand
 */

// constructor for receiving command
TSyncOpCommand::TSyncOpCommand(
  TSyncSession *aSessionP,    // associated session (for callbacks)
  TLocalEngineDS *aDataStoreP,   // the local datastore this syncop belongs to
  uInt32 aMsgID,                // the Message ID of the command
  TSyncOperation aSyncOp,     // the Sync operation (sop_xxx)
  TSmlCommandTypes aCmdType,  // the command type (scmd_xxx)
  SmlGenericCmdPtr_t aSyncOpElementP  // associated syncml protocol element
) :
  TSmlCommand(aCmdType,false,aSessionP,aMsgID)
{
  // save datastore
  fDataStoreP=aDataStoreP;
  // save operation type
  fSyncOp=aSyncOp;
  // save element
  fSyncOpElementP = aSyncOpElementP;
  // no remainder to be sent as next chunk
  fChunkedItemSize = 0;
  fIncompleteData = false;
  // no suspended part of item
  fStoredSize=0;
  fUnconfirmedSize=0;
  fLastChunkSize=0;
} // TSyncOpCommand::TSyncOpCommand


// constructor for sending command
TSyncOpCommand::TSyncOpCommand(
  TSyncSession *aSessionP,    // associated session (for callbacks)
  TLocalEngineDS *aDataStoreP, // datastore from which command originates
  TSyncOperation aSyncOp,     // sync operation (=command type)
  SmlPcdataPtr_t aMetaP       // meta for entire command (passing owner)
) :
  TSmlCommand(scmd_unknown,true,aSessionP)
{
  // save datastore
  fDataStoreP=aDataStoreP;
  #ifndef USE_SML_EVALUATION
  // no items yet
  fItemSizes=0;
  #endif
  // no remainder to be sent as next chunk
  fChunkedItemSize = 0;
  fIncompleteData = false;
  // no suspended part of item
  fStoredSize=0;
  fUnconfirmedSize=0;
  fLastChunkSize=0;
  // command type not yet determined
  fSyncOp=aSyncOp;
  // create internal alert element
  fSyncOpElementP = SML_NEW(SmlGenericCmd_t);
  // set default to make sure it is disposable (will be adjusted when items are added)
  fSyncOpElementP->elementType=SML_PE_GENERIC;
  // Cmd ID is now empty (will be set when issued)
  fSyncOpElementP->cmdID=NULL;
  // default to no flags (noResp is set at issue, if at all)
  fSyncOpElementP->flags=0;
  // insert meta, if any
  fSyncOpElementP->meta=aMetaP;
  // no optional elements for now
  fSyncOpElementP->cred=NULL;
  fSyncOpElementP->itemList=NULL;
  // determine command type
  switch (fSyncOp) {
    case sop_wants_add:
    case sop_add:
      fSyncOpElementP->elementType=SML_PE_ADD;
      fCmdType=scmd_add;
      break;
    case sop_copy:
      fSyncOpElementP->elementType=SML_PE_COPY;
      fCmdType=scmd_copy;
      break;
    case sop_move:
      fSyncOpElementP->elementType=SML_PE_MOVE;
      fCmdType=scmd_move;
      break;
    case sop_wants_replace:
    case sop_replace:
      fSyncOpElementP->elementType=SML_PE_REPLACE;
      fCmdType=scmd_replace;
      break;
    case sop_archive_delete:
      fSyncOpElementP->flags |= SmlArchive_f;
      goto dodelete;
    case sop_soft_delete:
      fSyncOpElementP->flags |= SmlSftDel_f;
    case sop_delete:
    dodelete:
      fSyncOpElementP->elementType=SML_PE_DELETE;
      fCmdType=scmd_delete;
      break;
    default:
      SYSYNC_THROW(TSyncException("Invalid SyncOp in TSyncOpCommand"));
      //break;
  } // switch
} // TSyncOpCommand::TSyncOpCommand


#ifndef USE_SML_EVALUATION

// get (approximated) message size required for sending it
uInt32 TSyncOpCommand::messageSize(void)
{
  // default, should be enough for most commands
  return TSmlCommand::messageSize()+fItemSizes;
}

#endif


// handle status received for previously issued command
// returns true if done, false if command must be kept in the status queue
bool TSyncOpCommand::handleStatus(TStatusCommand *aStatusCmdP)
{
  // check if this is a split command
  if (fIncompleteData) {
    // must be 213
    if (aStatusCmdP->getStatusCode()==213) return true;
    // something wrong, we did not get a 213
    PDEBUGPRINTFX(DBG_ERROR,("chunked object received status %hd instead of 213 --> aborting session",aStatusCmdP->getStatusCode()));
    // reason for aborting is incomplete command (reception)
    fSessionP->AbortSession(412,true); // local problem
  }
  else {
    // end of non-split SyncOp means that possibly saved
    // parts of partial outgoing item are now obsolete
    if (fDataStoreP->fPartialItemState==pi_state_save_outgoing) {
      fDataStoreP->fPartialItemState=pi_state_none; // not any more
      PDEBUGPRINTFX(DBG_PROTO+DBG_EXOTIC,("chunked object received final status %hd --> forget partial item data now",aStatusCmdP->getStatusCode()));
      // - forget stored data in case we have any
      if (fDataStoreP->fPIStoredDataP) {
        if (fDataStoreP->fPIStoredDataAllocated)
          smlLibFree(fDataStoreP->fPIStoredDataP);
        fDataStoreP->fPIStoredDataP=NULL;
        fDataStoreP->fPIStoredDataAllocated=false; // not any more
      }
      // - clear URIs
      fDataStoreP->fLastSourceURI.erase();
      fDataStoreP->fLastTargetURI.erase();
      // - clear other flags
      fDataStoreP->fLastItemStatus=0; // none left to send
      fDataStoreP->fPITotalSize=0;
      fDataStoreP->fPIUnconfirmedSize=0;
      fDataStoreP->fPIStoredSize=0;
    }
  }
  // let datastore handle this
  // Note: as fDataStoreP is linked to the generating datastore, this will never be
  //       a superdatastore. engHandleSyncOpStatus() is prepared to convert
  //       superID prefixed localIDs back to subDS's localID
  bool handled=false;
  if (fDataStoreP) {
    handled=fDataStoreP->engHandleSyncOpStatus(aStatusCmdP,this);
  }
  if (!handled) {
    // let base class handle it
    handled=TSmlCommand::handleStatus(aStatusCmdP);
  }
  return handled;
} // TSyncOpCommand::handleStatus


// mark any syncitems (or other data) for resume. Called for pending commands
// when a Suspend alert is received or whenever a resumable state must be saved
void TSyncOpCommand::markPendingForResume(TLocalEngineDS *aForDatastoreP, bool aUnsent)
{
  // only act if this is for our local datastore, and only outgoing ones!
  if (fOutgoing && fDataStoreP==aForDatastoreP && fSyncOpElementP) {
    // go through all of my items
    SmlItemListPtr_t itemListP = fSyncOpElementP->itemList;
    SmlItemPtr_t itemP;
    while (itemListP) {
      itemP=itemListP->item;
      if (itemP) {
        // call datastore to mark this one for resume
        fDataStoreP->engMarkItemForResume(
          smlSrcTargLocURIToCharP(itemP->source), // source for outgoing items is localID
          smlSrcTargLocURIToCharP(itemP->target), // target for outgoing items is remoteID
          fIncompleteData ? true : aUnsent // if not completely sent yet, always treat it as unsent!
        );
      }
      itemListP=itemListP->next;
    }
  }
} // TSyncOpCommand::markPendingForResume



// mark item for resend in next sync session (if possible)
void TSyncOpCommand::markForResend(void)
{
  if (fOutgoing && fDataStoreP) {
    // go through all of my items
    SmlItemListPtr_t itemListP = fSyncOpElementP->itemList;
    SmlItemPtr_t itemP;
    while (itemListP) {
      itemP=itemListP->item;
      if (itemP) {
        // call datastore to mark this one for resume
        fDataStoreP->engMarkItemForResend(
          smlSrcTargLocURIToCharP(itemP->source), // source for outgoing items is localID
          smlSrcTargLocURIToCharP(itemP->target) // target for outgoing items is remoteID
        );
      }
      itemListP=itemListP->next;
    }
  }
} // TSyncOpCommand::markForResend



// let item update the partial item state for suspend
// Note: this may only be called for an item that is actually the partial item
void TSyncOpCommand::updatePartialItemState(TLocalEngineDS *aForDatastoreP)
{
  if (!fOutgoing && fDataStoreP==aForDatastoreP && fDataStoreP->dsResumeChunkedSupportedInDB()) {
    // save info for only partially received item
    fDataStoreP->fLastItemStatus = 0; // no status sent yet
    // pass current item data
    // - get last item data
    SmlItemListPtr_t itemnodeP=fSyncOpElementP->itemList;
    while (itemnodeP) {
      if (!itemnodeP->next) {
        // this is the last item
        fDataStoreP->fPartialItemState=pi_state_save_incoming;
        // - get total expected size
        fDataStoreP->fPITotalSize=0;
        SmlMetInfMetInfPtr_t metaP = smlPCDataToMetInfP(itemnodeP->item->meta);
        if (!metaP || !metaP->size) {
          // item has no meta or no meta-size, so size must be in meta of command
          metaP = smlPCDataToMetInfP(fSyncOpElementP->meta);
        }
        if (metaP) smlPCDataToULong(metaP->size, fDataStoreP->fPITotalSize);
        // - get data
        if (itemnodeP->item && itemnodeP->item->data) {
          // dispose current contents if these were separately allocated
          // (otherwise, we can just overwrite the pointer)
          if (fDataStoreP->fPIStoredDataP && fDataStoreP->fPIStoredDataAllocated)
            smlLibFree(fDataStoreP->fPIStoredDataP);
          // datastore does NOT get owner of the data!
          fDataStoreP->fPIStoredDataAllocated=false; // we still own that data, session must NOT try to dispose!
          fDataStoreP->fPIStoredSize=itemnodeP->item->data->length;
          fDataStoreP->fPIStoredDataP=itemnodeP->item->data->content;
        }
        // - get amount of stored data that is unconfirmed
        //   (=the size of the most recently received chunk. This is unconfirmed
        //   until the next chunk arrives, because we don't know before that if the
        //   status 213 has reached the sender)
        fDataStoreP->fPIUnconfirmedSize=fLastChunkSize;
        // summarize
        PDEBUGPRINTFX(DBG_ADMIN,(
          "State of partially received item: total size=%ld, received=%ld, unconfirmed=%ld",
          (long)fDataStoreP->fPITotalSize,
          (long)fDataStoreP->fPIStoredSize,
          (long)fDataStoreP->fPIUnconfirmedSize
        ));
        // done
        break;
      }
      itemnodeP=itemnodeP->next;
    } // while
  }
} // TSyncOpCommand::updatePartialItemState


// get source (localID) of sent command
cAppCharP TSyncOpCommand::getSourceLocalID(void)
{
  if (!fSyncOpElementP) return NULL;
  if (!fSyncOpElementP->itemList) return NULL;
  if (!fSyncOpElementP->itemList->item) return NULL;
  return smlSrcTargLocURIToCharP(fSyncOpElementP->itemList->item->source);
} // TSyncOpCommand::getSourceLocalID


// get target (remoteID) of sent command
cAppCharP TSyncOpCommand::getTargetRemoteID(void)
{
  if (!fSyncOpElementP) return NULL;
  if (!fSyncOpElementP->itemList) return NULL;
  if (!fSyncOpElementP->itemList->item) return NULL;
  return smlSrcTargLocURIToCharP(fSyncOpElementP->itemList->item->target);
} // TSyncOpCommand::getTargetRemoteID


// add an Item to the sync op command
void TSyncOpCommand::addItem(
  SmlItemPtr_t aItemP // existing item data structure, ownership is passed to SyncOp command
)
{
  // add item
  if (fSyncOpElementP) {
    addItemToList(aItemP,&(fSyncOpElementP->itemList));
    #ifndef USE_SML_EVALUATION
    fItemSizes +=
      ITEMOVERHEADSIZE +
      strlen(smlSrcTargLocURIToCharP(aItemP->target)) +
      strlen(smlSrcTargLocNameToCharP(aItemP->target)) +
      strlen(smlSrcTargLocURIToCharP(aItemP->source)) +
      strlen(smlSrcTargLocNameToCharP(aItemP->source));
    SmlMetInfMetInfPtr_t metaP = smlPCDataToMetInfP(aItemP->meta);
    if (metaP) fItemSizes+=strlen(smlPCDataToCharP(metaP->type));
    if (aItemP->data) fItemSizes+=aItemP->data->length;
    #endif
  }
} // TSyncOpCommand::addItem



// helper: add dataPos EMI to meta
static void addDataPos(SmlMetInfMetInfPtr_t aMetaP, uInt32 aDataPos)
{
  // - find end of emi list or existing datapos
  SmlPcdataListPtr_t *emiPP = &(aMetaP->emi);
  /* version that searches for existing datapos and will replace it
  while (*emiPP!=NULL) {
    if (strucmp(smlPCDataToCharP((*emiPP)->data),"datapos=",8)==0) {
      // found existing datapos
      break;
    }
    emiPP = &((*emiPP)->next);
  }
  if (*emiPP==NULL) {
    // - add new EMI
    *emiPP=(SmlPcdataListPtr_t)smlLibMalloc(sizeof(SmlPcdataList_t));
    (*emiPP)->next=NULL;
  }
  else {
    // - replace existing
    smlFreePcdata((*emiPP)->data);
  }
  */
  while (*emiPP!=NULL) emiPP = &((*emiPP)->next);
  // - add new EMI list element
  *emiPP=(SmlPcdataListPtr_t)smlLibMalloc(sizeof(SmlPcdataList_t));
  (*emiPP)->next=NULL;
  // - set new data
  string s;
  StringObjPrintf(s,"datapos=%ld",(long)aDataPos);
  (*emiPP)->data = newPCDataString(s.c_str(),s.size());
} // addDataPos



void TSyncOpCommand::saveAsPartialItem(SmlItemPtr_t aItemP)
{
  // - forget stored data in case we have any
  if (fDataStoreP->fPIStoredDataP) {
    if (fDataStoreP->fPIStoredDataAllocated)
      smlLibFree(fDataStoreP->fPIStoredDataP);
    fDataStoreP->fPIStoredDataP=NULL;
    fDataStoreP->fPIStoredDataAllocated=false; // not any more
  }
  // save only when we can actually store chunk resume data
  if (fDataStoreP->dsResumeChunkedSupportedInDB()) {
    // - save URIs
    fDataStoreP->fLastSourceURI=smlSrcTargLocURIToCharP(aItemP->source);
    fDataStoreP->fLastTargetURI=smlSrcTargLocURIToCharP(aItemP->target);
    // - copy current item's data now (before the split!)
    fDataStoreP->fPIStoredDataAllocated=true; // separately allocated buffer, not connected with item any more
    fDataStoreP->fPIStoredSize=aItemP->data->length; // length of unsplitted item's buffer
    fDataStoreP->fPIStoredDataP=smlLibMalloc(aItemP->data->length);
    smlLibMemcpy((uInt8 *)fDataStoreP->fPIStoredDataP,aItemP->data->content,aItemP->data->length);
    // - set other flags
    fDataStoreP->fLastItemStatus=0; // none left to send
    fDataStoreP->fPartialItemState=pi_state_save_outgoing;
    fDataStoreP->fPITotalSize=fChunkedItemSize;
    fDataStoreP->fPIUnconfirmedSize=fDataStoreP->fPIStoredSize; // just a copy
  }
} // TSyncOpCommand::saveAsPartialItem


// - possibly substitute data with previous session's buffered left-overs from a chunked transfer
//   for resuming a chunked item transfer.
bool TSyncOpCommand::checkChunkContinuation(void)
{
  if (fChunkedItemSize==0 && fDataStoreP && fDataStoreP->fPartialItemState==pi_state_loaded_outgoing) {
    // this is a so far unchunked item, and there is
    // to-be-resent data from the previously suspended session
    // - check if the command contains the partially sent item
    SmlItemListPtr_t itemListP = fSyncOpElementP->itemList;
    while (itemListP) {
      if (
        itemListP->item &&
        strcmp(smlSrcTargLocURIToCharP(itemListP->item->source),fDataStoreP->fLastSourceURI.c_str())==0 &&
        strcmp(smlSrcTargLocURIToCharP(itemListP->item->target),fDataStoreP->fLastTargetURI.c_str())==0
      ) {
        // detected to-be-continued chunked item
        PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,(
          "Resuming sending chunked item Source='%s', Target='%s' -> replacing data with rest from suspended session",
          fDataStoreP->fLastSourceURI.c_str(),
          fDataStoreP->fLastTargetURI.c_str()
        ));
        // Do not send meta size on all but first chunk
        SmlMetInfMetInfPtr_t metinfP = itemListP->item->meta ? (SmlMetInfMetInfPtr_t)(itemListP->item->meta->content) : NULL;
        if (metinfP && metinfP->size) {
          smlFreePcdata(metinfP->size);
          metinfP->size=NULL;
        }
        // But make sure item knows the original total size (prevents splitCommand to add meta size again)
        fChunkedItemSize = fDataStoreP->fPITotalSize;
        // now add dataPos as <meta><EMI>: fPITotalSize-fPIStoredSize
        // - make sure we have meta
        if (!metinfP) {
          itemListP->item->meta=newMeta();
          metinfP=(SmlMetInfMetInfPtr_t)(itemListP->item->meta->content);
        }
        // - add it (total - what remains to be sent)
        addDataPos(metinfP,fDataStoreP->fPITotalSize-fDataStoreP->fPIStoredSize);
        // update buffered data info
        if (itemListP->item->data) {
          // substitute original data with buffered rest of data from last session
          // (so in case the item has changed in the local DB, we'll still send the original data of
          // which the receiver already has seen a part)
          PDEBUGPRINTFX(DBG_PROTO+DBG_DETAILS,(
            "original data size of item=%ld, total size at time of suspend=%ld, rest being sent now=%ld",
            (long)itemListP->item->data->length, // current total size
            (long)fDataStoreP->fPITotalSize, // total size at time of suspend
            (long)fDataStoreP->fPIStoredSize // what we will send now
          ));
          // dispose current data
          smlLibFree(itemListP->item->data->content);
          // set new data from last session
          itemListP->item->data->length=fDataStoreP->fPIStoredSize;
          itemListP->item->data->content=fDataStoreP->fPIStoredDataP;
          // data is now owned by item
          fDataStoreP->fPIStoredDataP=NULL;
          fDataStoreP->fPIStoredSize=0;
          fDataStoreP->fPIStoredDataAllocated=false;
        }
        else {
          PDEBUGPRINTFX(DBG_ERROR,("WARNING: internal error: to-be-resumed item contains no data?"));
        }
        fDataStoreP->fPartialItemState=pi_state_none; // consumed now
        fDataStoreP->fPITotalSize=0;
        fDataStoreP->fPIUnconfirmedSize=0;
        // substitution of item data done
        // (don't check other items - we have ONE item only, anyway in the current implementation)
        return true;
      }
      // next item
      itemListP=itemListP->next;
    }
  }
  // no chunk continuation
  return false;
} // TSyncOpCommand::checkChunkContinuation


// check if split is possible at all
bool TSyncOpCommand::canSplit(void)
{
  // Sync Op commands can be split when remote supports <moredata/> mechanism
  return fSessionP->fRemoteSupportsLargeObjects;
}


// - try to split (e.g. by SyncML 1.1 moredata mechanism) command by reducing
//   original command by at least aReduceByBytes and generating a second
//   command containing the rest of the data
//   Returns NULL if split is not possible or not worth trying in the current situation
//   (e.g.: only ridiculously small part would remain)
TSmlCommand *TSyncOpCommand::splitCommand(sInt32 aReduceByBytes)
{
  SmlItemListPtr_t *itemListPP;
  TSyncOpCommand *remainingDataCmdP=NULL;

  // nothing remaining so far
  SmlItemListPtr_t remainingItems=NULL;
  // check other conditions
  if (!fSyncOpElementP) return NULL;
  // now find where we should split
  do {
    // start of list
    itemListPP = &(fSyncOpElementP->itemList);
    // check if any items
    if (*itemListPP==NULL) break; // no more items
    // find last item
    while ((*itemListPP)->next) itemListPP = &((*itemListPP)->next);
    // get it's data
    SmlItemPtr_t itemP = (*itemListPP)->item;
    SmlPcdataPtr_t dataP = itemP->data;
    // check if we should split this item
    if (
      canSplit() && // remote can handle large objects (that is: chunked transfers)
      dataP && // there is data
      dataP->contentType!=SML_PCDATA_EXTENSION && // it's not an extension
      dataP->length > aReduceByBytes+MIN_SPLIT_DATA // remaining length of first part is large enough to make splitting worth doing now
    ) {
      // split within this item
      // Prepare original item
      // - make sure we have meta
      if (!itemP->meta)
        itemP->meta = newMeta(); // create meta as we haven't got one yet
      SmlMetInfMetInfPtr_t metinfP = (SmlMetInfMetInfPtr_t)(itemP->meta->content);
      // - set size if this is the first chunk
      if (metinfP->size) smlFreePcdata(metinfP->size);
      if (fChunkedItemSize==0) {
        // first chunk
        // - set dataPos to 0
        addDataPos(metinfP,0);
        // Note: we don't need to buffer when sending the first chunk.
        // If we get suspended before the second chunk is being generated, the item will
        // start over from beginning anyway (and we can use data from the DB).
        fChunkedItemSize=dataP->length;
        metinfP->size=newPCDataLong(fChunkedItemSize);
        PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,(
          "Item data too big (%ld) - must be chunked using <MoreData/>, %ld bytes to be sent later",
          (long)fChunkedItemSize,
          (long)aReduceByBytes
        ));
        #ifdef SYDEBUG
        // %%% should not happen, but I'm not sure it really won't
        if (fDataStoreP->fPartialItemState==pi_state_loaded_outgoing) {
          PDEBUGPRINTFX(DBG_ERROR,("WARNING - internal error: Splitting new item apparently before previous session's item has been resent"));
        }
        #endif
      }
      else {
        // this item is NOT the first chunk -> save stuff we need to
        PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,(
          "Remaining data still too big (%ld/%ld) - must be chunked again, %ld bytes to be sent later",
          (long)dataP->length,
          (long)fChunkedItemSize,
          (long)aReduceByBytes
        ));
        // Note: in this case, the item already has a dataPos - no need to add one here!
        // Resume the item in the middle rather than resending it in full.
        // IMPORTANT: It is essential not to store the fLastSourceURI/fLastTargetURI
        //   on the first chunk, as it then probably still contains info from
        //   the previous session.
        saveAsPartialItem(itemP);
      }
      // - set <moredata>
      itemP->flags |= SmlMoreData_f;
      // - create new data for original item (original data is in dataP)
      itemP->data = newPCDataStringX(
        (uInt8 *)dataP->content, // original data
        dataP->contentType==SML_PCDATA_OPAQUE, // opaque if original was opaque
        dataP->length-aReduceByBytes // reduced length
      );
      // Create additional item for next message
      // - new item
      SmlItemPtr_t nextItemP = newItem();
      // - with meta
      nextItemP->meta = newMeta();
      SmlMetInfMetInfPtr_t nextMetinfP = (SmlMetInfMetInfPtr_t)(nextItemP->meta->content);
      // - SyncML 1.1.1: The <Size> element MUST only be specified in the first chunk of the item.
      nextMetinfP->size=NULL; // no size in remaining items
      // - copy other meta
      if (metinfP->maxobjsize) nextMetinfP->format=newPCDataString(smlPCDataToCharP(metinfP->maxobjsize)); // We NEED this one of ZIPPED_BINDATA_SUPPORT!
      if (metinfP->format) nextMetinfP->format=newPCDataString(smlPCDataToCharP(metinfP->format));
      if (metinfP->type) nextMetinfP->type=newPCDataString(smlPCDataToCharP(metinfP->type));
      if (metinfP->mark) nextMetinfP->mark=newPCDataString(smlPCDataToCharP(metinfP->mark));
      //%%% if we ever decide to carry over EMI here, make sure we don't copy <EMI>dataPos=x</EMI>, as
      //    addDataPos() code relies on no dataPos being present when called.
      //if (metinfP->emi) nextMetinfP->emi=newPCDataString(smlPCDataToCharP(metinfP->emi)); // %%% not needed yet
      if (metinfP->version) nextMetinfP->version=newPCDataString(smlPCDataToCharP(metinfP->version));
      // - copy source and target
      nextItemP->target=newOptLocation(smlSrcTargLocURIToCharP(itemP->target),smlSrcTargLocNameToCharP(itemP->target));
      nextItemP->source=newOptLocation(smlSrcTargLocURIToCharP(itemP->source),smlSrcTargLocNameToCharP(itemP->source));
      // - copy flags except <moredata>
      nextItemP->flags = itemP->flags & ~SmlMoreData_f;
      // - now copy rest of data
      nextItemP->data = newPCDataStringX(
        (uInt8 *)dataP->content + dataP->length-aReduceByBytes, // original data minus what we have in original item
        dataP->contentType==SML_PCDATA_OPAQUE, // opaque if original was opaque
        aReduceByBytes // reduced length = remaining bytes
      );
      // - original data can now be disposed
      smlFreePcdata(dataP);
      // Add correct data position to the next item now.
      addDataPos(nextMetinfP,fChunkedItemSize-aReduceByBytes);
      // Now insert new item containing remaining data to beginning of list for next chunk
      SmlItemListPtr_t ilP = SML_NEW(SmlItemList_t);
      ilP->next=remainingItems;
      ilP->item=nextItemP;
      remainingItems=ilP;
      // count reduction
      aReduceByBytes=0;
      // done
      break;
    }
    else {
      // Split between items
      fChunkedItemSize=0; // no chunking in progress now
      // - get last itemlist element
      SmlItemListPtr_t ilP = *itemListPP;
      // - cut it out of original list
      *itemListPP=NULL;
      // - put it at the BEGINNING of the remaining items list
      ilP->next=remainingItems;
      remainingItems=ilP;
      // - count reduction
      aReduceByBytes -= dataP ? dataP->length : 0;
      // loop to check second-last item
    }
    // repeat as long as reduction goal is not met
  } while (aReduceByBytes>0);
  // check if any items left in original command
  if (fSyncOpElementP->itemList == NULL) {
    // none left in original list, can't split
    // - put back remaining list to original item
    fSyncOpElementP->itemList = remainingItems;
    // - cannot split
    return NULL;
  }
  // now create new command containing the remaining items
  if (remainingItems) {
    if (aReduceByBytes<=0) {
      // duplicate meta
      SmlPcdataPtr_t splitMetaP = copyMeta(fSyncOpElementP->meta);
      // make new command
      remainingDataCmdP = new TSyncOpCommand(
        fSessionP,      // associated session (for callbacks)
        fDataStoreP,    // datastore from which command originates
        fSyncOp,        // sync operation (=command type)
        splitMetaP      // meta for entire command (passing owner)
      );
      // now pass remaining items to new command
      remainingDataCmdP->fSyncOpElementP->itemList = remainingItems;
      // next command must know that it part of a chunked transfer
      remainingDataCmdP->fChunkedItemSize = fChunkedItemSize;
      // original command must know that it must expect a 213 status
      fIncompleteData = true;
    }
    else {
      // remaining items not used, delete them
      smlFreeItemList(remainingItems);
    }
  }
  // return split-off command
  return remainingDataCmdP;
} // TSyncOpCommand::splitCommand


// - test if completely issued (must be called after issue() and continueIssue())
bool TSyncOpCommand::finished(void)
{
  return
    (fOutgoing) || // outgoing commands are always finished (new split mechanism actually splits the command in two separate commands)
    (!fOutgoing && fSessionP->fIncompleteDataCommandP!=this); // not part of an incoming chunked data transfer
} // TSyncOpCommand::finished


// analyze command (but do not yet execute)
bool TSyncOpCommand::analyze(TPackageStates aPackageState)
{
  TSmlCommand::analyze(aPackageState);
  // get Command ID and flags
  if (fSyncOpElementP) {
    StartProcessing(fSyncOpElementP->cmdID,fSyncOpElementP->flags);
    return true;
  }
  else return false; // no proto element, bad command
} // TSyncOpCommand::analyze


// Adds data chunk from specified command to this (incomplete) command
// includes checking if sync op and item source/target match and
// checks for size match
#define MISSING_END_OF_CHUNK_ERROR 223 // End of Data for chunked object not received

localstatus TSyncOpCommand::AddNextChunk(SmlItemPtr_t aNextChunkItem, TSyncOpCommand *aCmdP)
{

  SmlItemListPtr_t incompletenode;
  SmlItemPtr_t incompleteitem;
  SmlPcdataPtr_t newdata;
  localstatus sta;

  // find last item in this command
  incompletenode = fSyncOpElementP->itemList;
  if (!incompletenode) return 412; // incomplete command
  while (incompletenode && incompletenode->next) incompletenode=incompletenode->next;
  incompleteitem = incompletenode->item;
  // check for data integrity
  if (!incompleteitem) return 412;
  if (!incompleteitem->data) return 412;
  if (!aNextChunkItem) return 412;
  if (!aNextChunkItem->data) return 412;
  // check for item compatibility
  // - commands must have same sync op
  if (getSyncOp() != aCmdP->getSyncOp())
    goto missingeoc; // not a status - will be converted to alert
  // - must have same source and target URI
  if (
    strcmp(smlSrcTargLocURIToCharP(incompleteitem->source),smlSrcTargLocURIToCharP(aNextChunkItem->source))!=0 ||
    strcmp(smlSrcTargLocURIToCharP(incompleteitem->target),smlSrcTargLocURIToCharP(aNextChunkItem->target))!=0
  )
    goto missingeoc; // not a status - will be converted to alert
  // - must have same PCData type and not extension
  if (
    incompleteitem->data->contentType != aNextChunkItem->data->contentType ||
    aNextChunkItem->data->contentType == SML_PCDATA_EXTENSION
  )
    return 400; // originator error, bad request
  // append data from next chunk to incomplete item
  newdata = SML_NEW(SmlPcdata_t);
  // - same content type as previous data had
  newdata->contentType = incompleteitem->data->contentType;
  newdata->contentType = incompleteitem->data->contentType;
  // - set new size
  newdata->length = incompleteitem->data->length + aNextChunkItem->data->length;
  // - remember size of chunk we're adding now in the incomplete command
  aCmdP->fLastChunkSize = aNextChunkItem->data->length;
  // - allocate new data for updated item (including an extra NUL terminator)
  newdata->content=smlLibMalloc(newdata->length+1);
  if (newdata->content==NULL)
    return 413; // request entity too large
  // - copy existing content
  smlLibMemcpy(newdata->content,incompleteitem->data->content,incompleteitem->data->length);
  // - add new chunk
  smlLibMemcpy((uInt8 *)newdata->content+incompleteitem->data->length,aNextChunkItem->data->content,aNextChunkItem->data->length);
  // - set the NUL terminator in case item is parsed as C string
  *(((uInt8 *)newdata->content)+newdata->length)=0;
  // - free old data in item
  smlFreePcdata(incompleteitem->data);
  // - insert updated data
  incompleteitem->data = newdata;
  // Now check if we are complete
  if ((aNextChunkItem->flags & SmlMoreData_f) == 0) {
    // end of chunk, verify size
    // - get total size from meta (of original item)
    SmlMetInfMetInfPtr_t metaP = smlPCDataToMetInfP(incompleteitem->meta);
    if (!metaP || !metaP->size) {
      // item has no meta or no meta-size, so size must be in meta of command
      metaP = smlPCDataToMetInfP(fSyncOpElementP->meta);
      if (!metaP) {
        PDEBUGPRINTFX(DBG_ERROR,("Chunked item has no meta -> no size found"));
        return 424; // no meta: Size mismatch
      }
    }
    sInt32 expectedSize;
    if (!smlPCDataToLong(metaP->size, expectedSize)) {
      PDEBUGPRINTFX(DBG_ERROR,("Chunked item had no or invalid meta <size> in first chunk"));
      return 424; // bad size string: size mismatch
    }
    // - check for size match
    if (incompleteitem->data->length != expectedSize) {
      // check if size mismatch could be due to duplicate transmit of unconfirmed data
      if (fUnconfirmedSize == uInt32(incompleteitem->data->length - expectedSize)) {
        // size mismatch is exactly what could be caused by duplicate transmit
        PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,(
          "Detected duplicate chunk in reassembled item due to resume without dataPos (%ld bytes at %ld) -> adjusting",
          (long)fUnconfirmedSize,
          (long)fStoredSize-fUnconfirmedSize
        ));
        // - cut out duplicate
        smlLibMemmove(
          ((uInt8 *)incompleteitem->data->content+fStoredSize-fUnconfirmedSize), // to end of confirmed data
          ((uInt8 *)incompleteitem->data->content+fStoredSize), // from end of stored data
          incompleteitem->data->length-fStoredSize // everything between end of stored data and end of item
        );
        incompleteitem->data->length = expectedSize; // adjust length
        *((uInt8 *)incompleteitem->data->content+expectedSize)=0; // add new safety terminator
        fUnconfirmedSize=0; // no data unconfirmed any more
      }
      else {
        PDEBUGPRINTFX(DBG_ERROR,(
          "Chunked item has wrong size (%ld, expected=%ld) after reassembly",
          (long)incompleteitem->data->length,
          (long)expectedSize
        ));
        return 424; // size mismatch
      }
    }
    // successfully reassembled
    sta=0;
  }
  else {
    // not yet completely reassembled
    sta=213;
  }
  // get MsgID and CmdID from added chunk's commnd
  fCmdID = aCmdP->getCmdID();
  fMsgID = aCmdP->getMsgID();
  // copy actual cmdID tag (to make sure we don't get the wrong CmdID when re-processing the command later)
  smlFreePcdata(fSyncOpElementP->cmdID); // ged rid of current ID
  fSyncOpElementP->cmdID = smlPcdataDup(aCmdP->fSyncOpElementP->cmdID); // copy ID from last added chunk
  // return status code
  return sta;
missingeoc:
  // missing end of chunk
  TAlertCommand *alertCmdP = new TAlertCommand(fSessionP, NULL, (uInt16)223);
  SmlItemPtr_t alertItemP = newItem();
  alertItemP->target=newOptLocation(smlSrcTargLocURIToCharP(incompleteitem->source));
  alertItemP->source=newOptLocation(smlSrcTargLocURIToCharP(incompleteitem->target));
  alertCmdP->addItem(alertItemP);
  // issue the alert
  fSessionP->issueRootPtr(alertCmdP);
  // signal failure of reassembling chunked item
  return MISSING_END_OF_CHUNK_ERROR;
} // TSyncOpCommand::AddNextChunk


// execute command (perform real actions, generate status)
// returns true if command has executed and can be deleted
bool TSyncOpCommand::execute(void)
{
  TStatusCommand *statusCmdP=NULL;
  SmlItemListPtr_t *itemnodePP, thisitemnode;
  localstatus sta;
  TSyncOpCommand *incompleteCmdP;
  bool queueforlater,processitem;
  bool nostatus;
  SmlItemListPtr_t tobequeueditems=NULL;

  SYSYNC_TRY {
    // get datastore pointer if we do not have one yet
    if (fDataStoreP==NULL) {
      // in case we did not get the pointer at creation - that is, when the enclosing
      // <sync> was delayed - we must now get datastore as set by the current <sync> command
      fDataStoreP = fSessionP->fLocalSyncDatastoreP;
      if (fDataStoreP==NULL) {
        statusCmdP=newStatusCommand(404); // no datastore, we can't process items for it
        ISSUE_COMMAND_ROOT(fSessionP,statusCmdP);
        return true; // command executed
      }
    }
    // get command meta if any
    SmlMetInfMetInfPtr_t cmdmetaP=smlPCDataToMetInfP(fSyncOpElementP->meta);
    // process items
    DEBUGPRINTFX(DBG_HOT,("command started processing"));
    itemnodePP=&(fSyncOpElementP->itemList);
    while (*itemnodePP) {
      queueforlater=false; // do no queue by default
      processitem=true; // process by default
      nostatus=false; // set to true if someone else is responsible for updating fLastItemStatus
      thisitemnode = *itemnodePP;
      // no result nor status so far
      statusCmdP=NULL;
      // check for NULL item
      if (!thisitemnode->item) {
        PDEBUGPRINTFX(DBG_ERROR,("command with NULL item"));
        statusCmdP=newStatusCommand(400); // protocol error
        ISSUE_COMMAND_ROOT(fSessionP,statusCmdP);
        return true; // command executed
      }
      // check if we are resuming a partially transmitted item (but MUST have data)
      if (
        fSessionP->fIncompleteDataCommandP==NULL &&
        thisitemnode->item->data && // item must have data
        fDataStoreP->fPartialItemState==pi_state_loaded_incoming &&
        strcmp(smlSrcTargLocURIToCharP(thisitemnode->item->source),fDataStoreP->fLastSourceURI.c_str())==0 &&
        strcmp(smlSrcTargLocURIToCharP(thisitemnode->item->target),fDataStoreP->fLastTargetURI.c_str())==0
      ) {
        // this is the last item sent in the previously suspended session
        if (fDataStoreP->fPIStoredSize==0) {
          // No item was left only partially received from the suspended session.
          // But we must handle the special case  where the final status of the last item sent
          // in the suspended session did not reach the recipient, so we now see the last chunk again.
          // In this case, simply re-issue the status and ignore the contents
          if (fDataStoreP->fLastItemStatus) {
            // item already processed, just repeat sending the status
            PDEBUGPRINTFX(DBG_PROTO,("Received last chunk for already processed item -> just resending status %hd",fDataStoreP->fLastItemStatus));
            statusCmdP = newStatusCommand(fDataStoreP->fLastItemStatus);
            processitem=false; // do not further process the item, just status
            fDataStoreP->fPartialItemState=pi_state_none; // chunked transfer from last session finally done
          }
        }
        else {
          // we have a partially received item left from the suspended session
          // continue it
          PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,(
            "Resuming receiving chunked item Source='%s', Target='%s'",
            fDataStoreP->fLastSourceURI.c_str(),
            fDataStoreP->fLastTargetURI.c_str()
          ));
          // simulate a meta size from suspended session's saved item size
          // - create meta if none there
          if (!thisitemnode->item->meta)
            thisitemnode->item->meta = newMeta();
          // - now check/generate meta size
          SmlMetInfMetInfPtr_t metinfP = (SmlMetInfMetInfPtr_t)(thisitemnode->item->meta->content);
          if (metinfP->size) {
            #ifdef __MWERKS__
            #warning "%%% maybe use presence of meta size to know that this is NOT resuming, that is as an implicit dataPos=0"
            #endif
            /* if I understand correctly, size MUST NOT be present on any but the first chunk.
               If that's correct, we could discard the saved partial item from a previous session
               in case we see a size (because this would mean the item is retransmitted as a whole
               and sender does not support resuming a partial item */
            // item has meta size, check if correct
            uInt32 thissize;
            if (
              !smlPCDataToULong(metinfP->size, thissize) ||
              thissize!=fDataStoreP->fPITotalSize
            )
            {
              PDEBUGPRINTFX(DBG_ERROR,(
                "Resumed item size does not match (expected=%ld, found=%ld)",
                (long)fDataStoreP->fPITotalSize,
                (long)thissize
              ));
              statusCmdP = newStatusCommand(424);
              processitem=false; // do not further process the item
            }
          }
          else {
            // item has no meta size, create it now
            metinfP->size=newPCDataLong(fDataStoreP->fPITotalSize);
          }
          SmlPcdataPtr_t newdata=NULL;
          uInt32 dataPos=0;
          if (processitem) {
            // save sizes confirmed/unconfirmed data in this command object
            // (for possible reassembly adjustment when command is complete)
            fUnconfirmedSize=fDataStoreP->fPIUnconfirmedSize;
            fStoredSize=fDataStoreP->fPIStoredSize;
            // Determine Data position
            // - check if we have it in EMI (Synthesis-Oracle enhancement)
            //   <EMI xmlns='syncml:metinf'>datapos=NUM</EMI>
            SmlPcdataListPtr_t emiP = metinfP->emi; // start with item meta EMI
            bool foundDataPos=false;
            for (uInt16 i=0; i<2; i++) {
              while (emiP) {
                if (emiP->data) {
                  const char *p = smlPCDataToCharP(emiP->data);
                  if (strucmp(p,"datapos=",8)==0) {
                    // correct lead-in, get number now
                    if (StrToULong(p+8,dataPos)>0) {
                      PDEBUGPRINTFX(DBG_PROTO+DBG_DETAILS,("found <meta><EMI>datapos=%ld",(long)dataPos));
                      foundDataPos=true; // found dataPos
                      break;
                    }
                    else {
                      PDEBUGPRINTFX(DBG_ERROR,("invalid number in <meta><EMI>datapos, ignoring it"));
                    }
                  }
                }
                // check next
                emiP=emiP->next;
              }
              // done if found
              if (foundDataPos) break;
              // try with cmd meta
              if (!cmdmetaP) break; // we don't have a command meta, no point to search
              emiP = cmdmetaP->emi; // cmd meta EMI
            }
            if (foundDataPos) {
              // we have a dataPos value
              // - no unconfirmed size any more for this item (but we still need the datastore-level vars
              //   below to do the copying
              fUnconfirmedSize=0;
              // - confirmed size is the data position now
              fStoredSize=dataPos;
            }
            else {
              // without known dataPos, we assume all received data confirmed for now
              // and will adjust at end of item if needed
              dataPos = fDataStoreP->fPIStoredSize;
            }
            // - check integrity
            if (dataPos>fDataStoreP->fPIStoredSize) {
              PDEBUGPRINTFX(DBG_ERROR,(
                "Data position invalid (max allowed=%ld, found=%ld)",
                (long)fDataStoreP->fPIStoredSize,
                (long)dataPos
              ));
              statusCmdP = newStatusCommand(412);
              processitem=false; // do not further process the item
            }
            if (processitem) {
              // - combine new and old data
              newdata = SML_NEW(SmlPcdata_t);
              // - same content type
              newdata->contentType = thisitemnode->item->data->contentType;
              // - set new size
              newdata->length = dataPos + thisitemnode->item->data->length;
              // - allocate new data for updated item (including an extra NUL terminator)
              newdata->content=smlLibMalloc(newdata->length+1);
              if (newdata->content==NULL) {
                statusCmdP = newStatusCommand(413);
                processitem=false; // do not further process the item
              }
            }
          }
          if (processitem) {
            // copy already received content up to dataPos
            smlLibMemcpy(newdata->content,fDataStoreP->fPIStoredDataP,dataPos);
            // forget data stored at DS level
            fDataStoreP->fPIStoredSize=0;
            if (fDataStoreP->fPIStoredDataP) {
              // only free if not owned by an item
              if (fDataStoreP->fPIStoredDataAllocated)
                smlLibFree(fDataStoreP->fPIStoredDataP);
            }
            fDataStoreP->fPIStoredDataP=NULL;
            // append content just received in this item
            smlLibMemcpy((uInt8 *)newdata->content+dataPos,thisitemnode->item->data->content,thisitemnode->item->data->length);
            // - set the NUL terminator in case item is parsed as C string
            *(((uInt8 *)newdata->content)+newdata->length)=0;
            // - free old data in item
            smlFreePcdata(thisitemnode->item->data);
            // - insert new data
            thisitemnode->item->data = newdata;
          }
          /* No, we need to keep pi_state_loaded_incoming until we get
          // Finished processing left-overs from last session
          fDataStoreP->fPartialItemState=pi_state_none;
          */
          // Now we can treat this as if it was the first (or only) chunk of a non-resumed item
        } // if we have partial data received in last session
      } // if item is the same as last item in suspended session
      if (processitem) {
        processitem=false; // only process further if we detect complete data
        // check if this item is complete (not chunked by <moredata>)
        if (thisitemnode->item->flags & SmlMoreData_f) {
          // no, it's only a chunk
          if (thisitemnode->item->data==NULL) {
            PDEBUGPRINTFX(DBG_ERROR,("Chunked item has no <data>"));
            statusCmdP->setStatusCode(412);
            // do not further process the command, exit item loop
            break;
          }
          // - return appropriate status
          statusCmdP = newStatusCommand(213); // chunked item accepted and buffered
          // - no items may follow the chunked one
          if (thisitemnode->next) {
            PDEBUGPRINTFX(DBG_ERROR,("Chunked item had additional items after the chunked one"));
            statusCmdP->setStatusCode(400);
            // do not further process the command, exit item loop
            break;
          }
          if (fSessionP->fIncompleteDataCommandP==NULL) {
            // This is the first chunk, save this as the original command (as it contains all meta)
            fSessionP->fIncompleteDataCommandP = this;
            // make sure that already executed items will not get saved
            *itemnodePP=NULL; // disconnect not-yet-executed items from executed ones
            smlFreeItemList(fSyncOpElementP->itemList); // free executed items
            fSyncOpElementP->itemList = thisitemnode; // put not-yet-executed, partial item into command
            // make sure moredata flag is not set on reassembled item
            thisitemnode->item->flags &= ~SmlMoreData_f;
            // save size of the chunk
            fLastChunkSize=thisitemnode->item->data->length;
          }
          else {
            // this is a chunk in the middle, combine its data
            // with the already buffered incomplete command
            // and update MsgID/CmdID. If source and target do not match,
            // statusCmdP will set to appropriate error code
            sta=fSessionP->fIncompleteDataCommandP->AddNextChunk(thisitemnode->item,this);
            if (sta) statusCmdP->setStatusCode(sta); // set error if any
          }
          PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,("<Moredata/> set: Chunk (%ld bytes) received (plus possible carry over from suspend) and buffered",(long)fLastChunkSize));
        }
        else {
          // Complete item or end of chunked item
          processitem=true;
          incompleteCmdP = fSessionP->fIncompleteDataCommandP;
          if (incompleteCmdP) {
            // end of chunked item, add final chunk and check for errors
            sta=incompleteCmdP->AddNextChunk(thisitemnode->item,this);
            if (sta==MISSING_END_OF_CHUNK_ERROR) {
              // command should have been end of chunked data, but wasn't.
              // Alert 223 is already issued.
              // - dispose of failed incomplete command
              delete incompleteCmdP;
              fSessionP->fIncompleteDataCommandP=NULL;
              // - process item as new command
            }
            else if (sta==0) {
              // successfully reassembled command, execute it now
              PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,("Last Chunk received (%ld bytes), item is reassembled",(long)fLastChunkSize));
              // - first remove global link to it to avoid recursion
              fSessionP->fIncompleteDataCommandP=NULL;
              // - execute now (and pass ownership)
              //   issues appropriate statuses, so we don't need to deal with it;
              //   in fact, we must not touch fLastItemStatus because we don't
              //   know the status
              nostatus=true;
              fSessionP->process(incompleteCmdP);
              // - this item is processed now, continue in loop if there are more items
              processitem=false; // do not process the item normally
              // Note: statusCmdP is NULL here, so no extra status will be issued for the
              //       item (process() should have already caused appropriate statuses to
              //       be generated.
            }
            else {
              // some error with this item, generate status
              statusCmdP = newStatusCommand(sta);
              PDEBUGPRINTFX(DBG_ERROR,("Adding next chunk to item failed with status=%hd",sta));
              // - dispose of failed incomplete command
              delete incompleteCmdP;
              fSessionP->fIncompleteDataCommandP=NULL;
              // do not process anything any further
              processitem=false; // do not process the item normally
              fDataStoreP->fLocalItemsError++; // count this as an error
            }
          }
          else {
            // save size of the chunk (if item has ANY data at all)
            fLastChunkSize=thisitemnode->item->data ? thisitemnode->item->data->length : 0;
          }
          if (processitem) {
            // - get remote and local IDs of item
            const char *remoteID=smlSrcTargLocURIToCharP(thisitemnode->item->source);
            const char *localID=smlSrcTargLocURIToCharP(thisitemnode->item->target);
            // prepare OK status for item
            statusCmdP = newStatusCommand(200);
            // let session process the item
            PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,(
              "Item (syncop: %s) started processing, remoteID='%s', localID='%s'",
              SyncOpNames[(sInt16)fSyncOp],
              remoteID,
              localID
            ));
            queueforlater=false;
            if (fSessionP->processSyncOpItem(
              fSyncOp,               // the operation
              thisitemnode->item,    // the item to be processed
              cmdmetaP,              // command-wide meta, if any
              fDataStoreP,           // related datastore pointer
              *statusCmdP,           // pre-set 200 status, can be modified in case of errors
              queueforlater          // set if processing of item must be done later
            )) {
              // fully ok (no internal change of requested operations or
              // silent acceptance of deleting non-existant item)
            }
            else {
              // Internal processing of items showed some irregularity, but
              // status to remote peer can still be ok (for example when trying
              // to delete non-existant item in datastore with incomplete rollbacks,
              // processSyncOpItem() will return false, but Status=200.
              PDEBUGPRINTFX(DBG_PROTO,(
                "Irregularity in execution of item, status=%hd",
                statusCmdP->getStatusCode()
              ));
            }
          } // if processitem
        } // if complete item
      } // if processitem
      // now generate status or queue for later
      // - remember as last item for possible suspend and resume
      fDataStoreP->fLastSourceURI = smlSrcTargLocURIToCharP(thisitemnode->item->source);
      fDataStoreP->fLastTargetURI = smlSrcTargLocURIToCharP(thisitemnode->item->target);
      if (queueforlater) {
        PDEBUGPRINTFX(DBG_PROTO,(
          "Item could not be processed completely now -> will be queued for later processing"
        ));
        // item processing could not complete, we must queue this and all other items
        // in this command for later processing. However, re-assembling chunked
        // items must proceed as normal.
        fDataStoreP->fLastItemStatus = 0; // no status sent yet
        // Therefore, we move this item from the original list to a list of to-be-queued items
        addItemToList(
          thisitemnode->item, // the item
          &tobequeueditems // place pointer to next node here
        );
        // cut item out of original list to make sure it does not get disposed with it
        // - itemnodePP points to pointer to list start or a "next" pointer in a node
        //   (which in turn points to thisitemnode). Now let it point to the next item node.
        // - this also advance processing to the next item
        *itemnodePP = thisitemnode->next;
        // - dispose of the item node itself
        thisitemnode->next=NULL; // disconnect subsequent nodes
        thisitemnode->item=NULL; // disconnect item (which is now in tobequeueditems list)
        smlFreeItemList(thisitemnode); // dispose of node
      }
      else {
        // count incoming net data
        if (thisitemnode->item->data) {
          fDataStoreP->fIncomingDataBytes+=thisitemnode->item->data->length;
        }
        // item processed
        // - remember status (final only) for possible suspend and resume
        sta= statusCmdP ? statusCmdP->getStatusCode() : 0;
        if (!nostatus && sta!=213) {
          // final status received, save it for possible resend
          fDataStoreP->fLastItemStatus = sta;
          // but forget data stored at DS level
          fDataStoreP->fPIStoredSize=0;
          if (fDataStoreP->fPIStoredDataP) {
            // only free if not owned by an item
            if (fDataStoreP->fPIStoredDataAllocated)
              smlLibFree(fDataStoreP->fPIStoredDataP);
          }
          fDataStoreP->fPIStoredDataP=NULL;
          // make sure it gets saved
          fDataStoreP->fPartialItemState = pi_state_save_incoming;
        }
        // - issue status for it
        if (statusCmdP) {
          // add source and target refs of item
          statusCmdP->addTargetRef(smlSrcTargLocURIToCharP(thisitemnode->item->target)); // add target ref
          statusCmdP->addSourceRef(smlSrcTargLocURIToCharP(thisitemnode->item->source)); // add source ref
          // issue
          ISSUE_COMMAND_ROOT(fSessionP,statusCmdP);
        }
        // advance to next item in list
        itemnodePP = &(thisitemnode->next);
      }
    } // item loop
    // free this one in advance (only if command is finished)
    if (finished() && !tobequeueditems) FreeSmlElement();
    // update item list in command for queuing if needed
    if (tobequeueditems) {
      // there are to-be-queued items, insert them instead of the original item list
      // - delete current item list (only contains already processed and statused items)
      smlFreeItemList(fSyncOpElementP->itemList);
      // - insert to be queued items instead
      fSyncOpElementP->itemList=tobequeueditems;
    }
  }
  SYSYNC_CATCH (...)
    // make sure owned objects in local scope are deleted
    if (statusCmdP) delete statusCmdP;
    // re-throw
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
  // if not all items could be executed, command must be queued for later re-execution.
  // (all successfully executed items are already deleted from the item list)
  return !tobequeueditems;
} // TSyncOpCommand::execute


// returns true if command must be put to the waiting-for-status queue.
// If false, command does not need to be put into waiting-for-status queue
// and can be deleted if finished() returns true
bool TSyncOpCommand::issue(
  uInt32 aAsCmdID,     // command ID to be used
  uInt32 aInMsgID,     // message ID in which command is being issued
  bool aNoResp       // dummy here, because Status has always no response
)
{
  // prepare basic stuff
  TSmlCommand::issue(aAsCmdID,aInMsgID,aNoResp);
  // now issue
  if (fSyncOpElementP) {
    // issue command with SyncML toolkit (no flags)
    PrepareIssue(&fSyncOpElementP->cmdID,&fSyncOpElementP->flags); // CmdID and flags
    Ret_t err;
    InstanceID_t wspid = fSessionP->getSmlWorkspaceID();
    switch (fCmdType) {
      case scmd_add:
        #ifdef SYDEBUG
        if (fSessionP->fXMLtranslate && fSessionP->fOutgoingXMLInstance && !fEvalMode)
          smlAddCmd(fSessionP->fOutgoingXMLInstance,fSyncOpElementP);
        #endif
        err=smlAddCmd(wspid,fSyncOpElementP);
        break;
      #ifdef COPY_SEND
      case scmd_copy:
        #ifdef SYDEBUG
        if (fSessionP->fXMLtranslate && fSessionP->fOutgoingXMLInstance && !fEvalMode)
          smlCopyCmd(fSessionP->fOutgoingXMLInstance,fSyncOpElementP);
        #endif
        err=smlCopyCmd(wspid,fSyncOpElementP);
        break;
      #endif
      case scmd_move:
        #ifdef SYDEBUG
        if (fSessionP->fXMLtranslate && fSessionP->fOutgoingXMLInstance && !fEvalMode)
          smlMoveCmd(fSessionP->fOutgoingXMLInstance,fSyncOpElementP);
        #endif
        err=smlMoveCmd(wspid,fSyncOpElementP);
        break;
      case scmd_replace:
        #ifdef SYDEBUG
        if (fSessionP->fXMLtranslate && fSessionP->fOutgoingXMLInstance && !fEvalMode)
          smlReplaceCmd(fSessionP->fOutgoingXMLInstance,fSyncOpElementP);
        #endif
        err=smlReplaceCmd(wspid,fSyncOpElementP);
        break;
      case scmd_delete:
        #ifdef SYDEBUG
        if (fSessionP->fXMLtranslate && fSessionP->fOutgoingXMLInstance && !fEvalMode)
          smlDeleteCmd(fSessionP->fOutgoingXMLInstance,fSyncOpElementP);
        #endif
        err=smlDeleteCmd(wspid,fSyncOpElementP);
        break;
      default:
        SYSYNC_THROW(TSyncException("TSyncOpCommand:issue, bad command type"));
    }
    if (!fEvalMode) {
      if (err!=SML_ERR_OK) {
        SYSYNC_THROW(TSmlException("smlAdd/Copy/Replace/DeleteCmd",err));
      }
      FinalizeIssue();
      // show items
      SmlItemListPtr_t itemP = fSyncOpElementP->itemList;
      while (itemP) {
        // count net data sent
        sInt32 itemlen=0;
        if (itemP->item && itemP->item->data) {
          itemlen=itemP->item->data->length;
          fDataStoreP->fOutgoingDataBytes+=itemlen;
        }
        // show item source and target
        PDEBUGPRINTFX(DBG_PROTO,(
          "Item remoteID='%s', localID='%s', datasize=%ld",
          smlSrcTargLocURIToCharP(itemP->item->target),
          smlSrcTargLocURIToCharP(itemP->item->source),
          (long)itemlen
        ));
        if (fChunkedItemSize>0 && !fIncompleteData) {
          #ifdef SYDEBUG
          PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,(
            "Last Chunk (%ld bytes) of large object (%ld total) sent now - retaining it in case of implicit suspend",
            (long)itemlen,
            (long)fChunkedItemSize
          ));
          #endif
          if (itemlen>0)
            saveAsPartialItem(itemP->item);
        }
        // next
        itemP=itemP->next;
      }
      // we don't need the data any more, but we should keep the source and target IDs until we have the status
      // - get rid of data
      if (fSyncOpElementP->itemList) {
        // - free possible extra items (shouldn't be any - we always send single item per command)
        smlFreeItemList(fSyncOpElementP->itemList->next);
        fSyncOpElementP->itemList->next=NULL;
        // - free data and meta part of this item, but not target and source info
        if (fSyncOpElementP->itemList->item) {
          smlFreePcdata(fSyncOpElementP->itemList->item->meta);
          fSyncOpElementP->itemList->item->meta=NULL;
          smlFreePcdata(fSyncOpElementP->itemList->item->data);
          fSyncOpElementP->itemList->item->data=NULL;
        }
      }
    }
    else
      return err==SML_ERR_OK; // ok if evaluation had no error
  }
  else {
    DEBUGPRINTFX(DBG_ERROR,("*** Tried to issue NULL status"));
  }
  // sync op commands do need status
  return true; // queue for status
} // TSyncOpCommand::issue


void TSyncOpCommand::FreeSmlElement(void)
{
  // remove SyncML toolkit element(s)
  FREEPROTOELEMENT(fSyncOpElementP);
} // TSyncOpCommand::FreeSmlElement


TSyncOpCommand::~TSyncOpCommand()
{
  // free command elements, if any (use explicit invocation as this is a destructor)
  TSyncOpCommand::FreeSmlElement();
} // TSyncOpCommand::~TSyncOpCommand


/* end of TSyncOpCommand implementation */


#endif // SYNCCOMMAND_PART1_EXCLUDE
#ifndef SYNCCOMMAND_PART2_EXCLUDE

/*
 * Implementation of TMapCommand
 */


// constructor for receiving Map command
TMapCommand::TMapCommand(
  TSyncSession *aSessionP,  // associated session (for callbacks)
  uInt32 aMsgID,              // the Message ID of the command
  SmlMapPtr_t aMapElementP  // associated syncml protocol element
) :
  TSmlCommand(scmd_map,false,aSessionP,aMsgID)
{
  // save element
  fMapElementP = aMapElementP;
  fInProgress=false; // just in case...
  // no params
  fLocalDataStoreP=NULL;
  fRemoteDataStoreP=NULL;
} // TMapCommand::TMapCommand


#ifdef SYSYNC_SERVER
// Server only receives Maps

// analyze command (but do not yet execute)
bool TMapCommand::analyze(TPackageStates aPackageState)
{
  TSmlCommand::analyze(aPackageState);
  // get Command ID and flags
  if (fMapElementP) {
    StartProcessing(fMapElementP->cmdID,0);
    return true;
  }
  else return false; // no proto element, bad command
} // TMapCommand::analyze


// execute command (perform real actions, generate status)
// returns true if command has executed and can be deleted
bool TMapCommand::execute(void)
{
  TStatusCommand *statusCmdP=NULL;
  bool queueforlater;

  SYSYNC_TRY {
    // prepare a status
    statusCmdP = newStatusCommand(200);
    // let session actually process the command
    queueforlater=false;
    fSessionP->processMapCommand(fMapElementP,*statusCmdP,queueforlater);
    // check if done or queued for later
    if (queueforlater) {
      delete statusCmdP;
    }
    else {
      // now issue status, if any
      if (statusCmdP) {
        statusCmdP->addTargetRef(smlSrcTargLocURIToCharP(fMapElementP->target)); // add target ref
        statusCmdP->addSourceRef(smlSrcTargLocURIToCharP(fMapElementP->source)); // add source ref
        #ifdef MAP_STATUS_IMMEDIATE
        // issue status right now (might cause map statuses
        // returned in sync-updates-to-client package #4 instead of map-confirm #6
        ISSUE_COMMAND_ROOT(fSessionP,statusCmdP);
        #else
        // issue status only if outgoing package is map-response
        fSessionP->issueNotBeforePackage(psta_map,statusCmdP);
        #endif
      }
      // free this one in advance
      FreeSmlElement();
    }
  }
  SYSYNC_CATCH (...)
    // make sure owned objects in local scope are deleted
    if (statusCmdP) delete statusCmdP;
    // re-throw
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
  // return true if command has fully executed
  return !queueforlater;
} // TMapCommand::execute

#endif

#ifdef SYSYNC_CLIENT

// Client only sends maps

// constructor for sending MAP Command
TMapCommand::TMapCommand(
  TSyncSession *aSessionP,              // associated session (for callbacks)
  TLocalEngineDS *aLocalDataStoreP,    // local datastore
  TRemoteDataStore *aRemoteDataStoreP   // remote datastore
) :
  TSmlCommand(scmd_map,true,aSessionP)
{
  // save params
  fLocalDataStoreP=aLocalDataStoreP;
  fRemoteDataStoreP=aRemoteDataStoreP;
  // create new internal map element
  fMapElementP=NULL; // none yet
  generateEmptyMapElement();
  // not yet in progress (as not yet issued)
  fInProgress=false;
} // TMapCommand::TMapCommand


// generate empty map element
void TMapCommand::generateEmptyMapElement(void)
{
  // free possibly still existing map element
  if (fMapElementP) FreeSmlElement();
  // create internal map element
  fMapElementP = SML_NEW(SmlMap_t);
  // set proto element type to make it auto-disposable
  fMapElementP->elementType=SML_PE_MAP;
  // Cmd ID is now empty (will be set when issued)
  fMapElementP->cmdID=NULL;
  // set source and target
  fMapElementP->target=newLocation(fRemoteDataStoreP->getFullName()); // remote is target for Map command
  fMapElementP->source=newLocation(fLocalDataStoreP->getRemoteViewOfLocalURI()); // local is source of Map command
  // no optional elements for now
  fMapElementP->cred=NULL; // %%% no database level auth yet at all
  fMapElementP->meta=NULL; // %%% no meta for now
  // map itemlist is empty
  fMapElementP->mapItemList=NULL;
  #ifndef USE_SML_EVALUATION
  // no item sizes
  fItemSizes=0;
  #endif
} // TMapCommand::generateEmptyMapElement


// returns true if command must be put to the waiting-for-status queue.
// If false, command can be deleted (except if it is not finished() )
bool TMapCommand::issue(
  uInt32 aAsCmdID,     // command ID to be used
  uInt32 aInMsgID,     // message ID in which command is being issued
  bool aNoResp
)
{
  // prepare basic stuff
  TSmlCommand::issue(aAsCmdID,aInMsgID,aNoResp);
  // now issue
  if (fMapElementP) {
    // add as many Map items as possible, update fInProgress
    // BUT DONT DO THAT IN EVAL MODE (would be recursive, as generateMapItems()
    // will call evalIssue() which will call this routine....)
    if (!fEvalMode) generateMapItems();
    // issue command, but only if there are any items at all
    if (fMapElementP->mapItemList) {
      // now issue map command
      PrepareIssue(&fMapElementP->cmdID);
      if (!fEvalMode) {
        #ifdef SYDEBUG
        if (fSessionP->fXMLtranslate && fSessionP->fOutgoingXMLInstance)
          smlMapCmd(fSessionP->fOutgoingXMLInstance,fMapElementP);
        #endif
        Ret_t err;
        if ((err=smlMapCmd(fSessionP->getSmlWorkspaceID(),fMapElementP))!=SML_ERR_OK) {
          SYSYNC_THROW(TSmlException("smlMapCmd",err));
        }
        FinalizeIssue();
        // show debug
        DEBUGPRINTFX(DBG_HOT,(
          "Generated Map command, Source='%s', Target='%s'",
          smlSrcTargLocURIToCharP(fMapElementP->source),
          smlSrcTargLocURIToCharP(fMapElementP->target)
        ));
      }
      else
        return smlMapCmd(fSessionP->getSmlWorkspaceID(),fMapElementP)==SML_ERR_OK;
    }
    else {
      if (fEvalMode) return true; // evaluated nothing, is ok!
      DEBUGPRINTFX(DBG_PROTO,("Suppressed generating empty map command"));
      return false; // do not queue, delete command now
    }
    // keep smlElement, as we need it to mark confirmed map items
    // at handleStatus().
  }
  else {
    DEBUGPRINTFX(DBG_ERROR,("*** Tried to issue NULL sync"));
  }
  // return true if command must be queued for status/result response reception
  return queueForResponse();
} // TMapCommand::issue



// - test if completely issued (must be called after issue() and continueIssue())
bool TMapCommand::finished(void)
{
  // not finished as long there are more syncOps to send
  return (!fInProgress || !fOutgoing);
} // TMapCommand::finished


// - continue issuing command. This is called by session at start of new message
//   when finished() returned false after issue()/continueIssue()
//   (which causes caller of finished() to put command into fInteruptedCommands queue)
//   returns true if command should be queued for status (again?)
//   NOTE: continueIssue must make sure that it gets a new CmdID/MsgID because
//   the command itself is issued multiple times
bool TMapCommand::continueIssue(bool &aNewIssue)
{
  aNewIssue=false; // never issue anew!
  if (!fInProgress) return false; // done, don't queue for status again
  // we can generate next map command only after we have received status for this one
  if (isWaitingForStatus()) return true; // still in progress, still waiting for status
  // command is in progress, continue with another <Map> command in this message
  // - create new Map command
  generateEmptyMapElement();
  // - issue again with new command ID
  fCmdID = fSessionP->getNextOutgoingCmdID();
  fMsgID = fSessionP->getOutgoingMsgID();
  // new map command must be queued for status again
  // but only if something was actually issued (map command issue can
  // be nop if there are no map items present)
  return issue(fCmdID,fMsgID,fNoResp);
} // TMapCommand::continueIssue


// add as many Map items as possible, update fInProgress
void TMapCommand::generateMapItems(void)
{
  // let datastore add Map items (possibly none)
  fInProgress = !(
    fLocalDataStoreP->engGenerateMapItems(this,NULL)
  );
} // TMapCommand::generateMapItems


// add a Map Item to the map command
void TMapCommand::addMapItem(const char *aLocalID, const char *aRemoteID)
{
  // make item
  SmlMapItemPtr_t mapitemP = SML_NEW(SmlMapItem_t);
  mapitemP->target=newLocation(aRemoteID);
  mapitemP->source=newLocation(aLocalID);
  #ifndef USE_SML_EVALUATION
  // update size
  fItemSizes +=
    ITEMOVERHEADSIZE +
    strlen(aLocalID) +
    strlen(aRemoteID);
  #endif
  // add item to item list
  SmlMapItemListPtr_t *mapItemListPP = &(fMapElementP->mapItemList);
  // find last itemlist pointer
  while (*mapItemListPP) {
    mapItemListPP=&((*mapItemListPP)->next);
  }
  // aItemListPP now points to a NULL pointer which must be replaced by addr of new ItemList entry
  *mapItemListPP = SML_NEW(SmlMapItemList_t);
  (*mapItemListPP)->next=NULL;
  (*mapItemListPP)->mapItem=mapitemP; // insert new item
} // TMapCommand::addItem


// remove last added map item from the command
void TMapCommand::deleteLastMapItem(void)
{
  // make item
  #ifndef USE_SML_EVALUATION
  #error "Not implemented for old non-eval method any more"
  #endif
  // kill last item in item list
  SmlMapItemListPtr_t *mapItemListPP = &(fMapElementP->mapItemList);
  // find last itemlist pointer pointing to an item
  while (*mapItemListPP && (*mapItemListPP)->next) {
    mapItemListPP=&((*mapItemListPP)->next);
  }
  // aItemListPP now points to the pointer which points to the last item list element (or is NULL if no items there)
  if (*mapItemListPP) {
    // delete rest of list, which consists of one item only
    smlFreeMapItemList(*mapItemListPP);
    // NULL link in previous item or beginning of list
    *mapItemListPP=NULL;
  }
} // TMapCommand::deleteLastMapItem



// handle status received for previously issued command
// returns true if done, false if command must be kept in the status queue
bool TMapCommand::handleStatus(TStatusCommand *aStatusCmdP)
{
  bool handled=false;
  if (aStatusCmdP->getStatusCode()==200) {
    handled=true; // is ok
    // mark maps confirmed
    if (fLocalDataStoreP) {
      // go through all of my items
      SmlMapItemListPtr_t mapItemListP = fMapElementP->mapItemList;
      SmlMapItemPtr_t mapItemP;
      while (mapItemListP) {
        mapItemP=mapItemListP->mapItem;
        if (mapItemP) {
          // call datastore to mark this map confirmed (no need to be sent again in next session / resume)
          fLocalDataStoreP->engMarkMapConfirmed(
            smlSrcTargLocURIToCharP(mapItemP->source), // source for outgoing mapitem is localID
            smlSrcTargLocURIToCharP(mapItemP->target) // target for outgoing mapitem is remoteID
          );
        }
        mapItemListP=mapItemListP->next;
      }
    }
  }
  // anyway, we can now get rid of the map items in the command
  if (fMapElementP) FreeSmlElement();
  // check if base class must handle it
  if(!handled) {
    // let base class handle it
    handled=TSmlCommand::handleStatus(aStatusCmdP);
  }
  return handled;
} // TMapCommand::handleStatus

#endif // client-only map implementation


// Common for Client and Server

#ifndef USE_SML_EVALUATION

// get (approximated) message size required for sending it
uInt32 TMapCommand::messageSize(void)
{
  // return size of command so far
  return TSmlCommand::messageSize()+fItemSizes;
} // TMapCommand::messageSize

#endif


void TMapCommand::FreeSmlElement(void)
{
  // remove SyncML toolkit element(s)
  FREEPROTOELEMENT(fMapElementP);
} // TMapCommand::FreeSmlElement


TMapCommand::~TMapCommand()
{
  // free command elements, if any (use explicit invocation as this is a destructor)
  TMapCommand::FreeSmlElement();
} // TMapCommand::TMapCommand


/* end of TMapCommand implementation */

/*
 * Implementation of TGetCommand
 */

// constructor for receiving command
TGetCommand::TGetCommand(
  TSyncSession *aSessionP,  // associated session (for callbacks)
  uInt32 aMsgID,              // the Message ID of the command
  SmlGetPtr_t aGetElementP  // associated syncml protocol element
) :
  TSmlCommand(scmd_get,false,aSessionP,aMsgID)
{
  // save element
  fGetElementP = aGetElementP;
} // TGetCommand::TGetCommand


// constructor for sending command
TGetCommand::TGetCommand(
  TSyncSession *aSessionP              // associated session (for callbacks)
) :
  TSmlCommand(scmd_get,true,aSessionP)
{
  // create internal get element
  fGetElementP = SML_NEW(SmlGet_t);
  // set proto element type to make it auto-disposable
  fGetElementP->elementType=SML_PE_GET;
  // Cmd ID is now empty (will be set when issued)
  fGetElementP->cmdID=NULL;
  // default to no flags (noResp is set at issue, if at all)
  fGetElementP->flags=0;
  // no optional elements for now
  fGetElementP->cred=NULL;
  fGetElementP->lang=NULL;
  fGetElementP->meta=NULL;
  fGetElementP->itemList=NULL;
} // TGetCommand::TGetCommand


// set Meta of the get command
void TGetCommand::setMeta(
  SmlPcdataPtr_t aMetaP // existing meta data structure, ownership is passed to Get
)
{
  if (fGetElementP)
    fGetElementP->meta=aMetaP;
} // TGetCommand::setMeta


// add an Item to the get command
void TGetCommand::addItem(
  SmlItemPtr_t aItemP // existing item data structure, ownership is passed to Get
)
{
  if (fGetElementP)
    addItemToList(aItemP,&(fGetElementP->itemList));
} // TGetCommand::addItem


// add a target specification Item to the get command
void TGetCommand::addTargetLocItem(
  const char *aTargetURI,
  const char *aTargetName
)
{
  SmlItemPtr_t itemP = newItem();

  itemP->target = newLocation(aTargetURI,aTargetName);
  addItem(itemP);
} // TGetCommand::addTargetLocItem



// analyze command (but do not yet execute)
bool TGetCommand::analyze(TPackageStates aPackageState)
{
  TSmlCommand::analyze(aPackageState);
  // get Command ID and flags
  if (fGetElementP) {
    StartProcessing(fGetElementP->cmdID,fGetElementP->flags);
    return true;
  }
  else return false; // no proto element, bad command
} // TGetCommand::analyze


// execute command (perform real actions, generate status)
// returns true if command has executed and can be deleted
bool TGetCommand::execute(void)
{
  TStatusCommand *statusCmdP=NULL;
  TResultsCommand *resultsCmdP=NULL;
  SmlItemListPtr_t thisitemnode,nextitemnode;

  SYSYNC_TRY {
    // process items
    thisitemnode=fGetElementP->itemList;
    while (thisitemnode) {
      nextitemnode = thisitemnode->next;
      // no result nor status so far
      statusCmdP=NULL;
      resultsCmdP=NULL;
      // get Target LocURI
      if (!thisitemnode->item) SYSYNC_THROW(TSyncException("Get with NULL item"));
      string locURI=smlSrcTargLocURIToCharP(thisitemnode->item->target);
      DEBUGPRINTFX(DBG_HOT,("processing item with locURI=%s",locURI.c_str()));
      // let session (and descendants) process it
      // - default to "not found" status
      statusCmdP = newStatusCommand(404);
      resultsCmdP=fSessionP->processGetItem(locURI.c_str(),this,thisitemnode->item,*statusCmdP);
      // now complete and issue status
      statusCmdP->addTargetRef(locURI.c_str()); // add target ref
      ISSUE_COMMAND_ROOT(fSessionP,statusCmdP);
      // issue results, if any
      if (resultsCmdP) {
        ISSUE_COMMAND_ROOT(fSessionP,resultsCmdP);
      }
      // next item
      thisitemnode=nextitemnode;
    }
    // free this one in advance
    FreeSmlElement();
  }
  SYSYNC_CATCH (...)
    // make sure owned objects in local scope are deleted
    if (statusCmdP) delete statusCmdP;
    if (resultsCmdP) delete resultsCmdP;
    // re-throw
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
  // done with command, delete it now: return true
  return true;
} // TGetCommand::execute


// returns true if command must be put to the waiting-for-status queue.
// If false, command can be deleted
bool TGetCommand::issue(
  uInt32 aAsCmdID,     // command ID to be used
  uInt32 aInMsgID,     // message ID in which command is being issued
  bool aNoResp
)
{
  // prepare basic stuff
  TSmlCommand::issue(aAsCmdID,aInMsgID,aNoResp);
  // now issue
  if (fGetElementP) {
    // issue command with SyncML toolkit
    PrepareIssue(&fGetElementP->cmdID,&fGetElementP->flags);
    if (!fEvalMode) {
      #ifdef SYDEBUG
      if (fSessionP->fXMLtranslate && fSessionP->fOutgoingXMLInstance)
        smlGetCmd(fSessionP->fOutgoingXMLInstance,fGetElementP);
      #endif
      Ret_t err;
      if ((err=smlGetCmd(fSessionP->getSmlWorkspaceID(),fGetElementP))!=SML_ERR_OK) {
        SYSYNC_THROW(TSmlException("smlGetCmd",err));
      }
      FinalizeIssue();
      // we don't need the status structure any more, free (and NULL ptr) now
      FreeSmlElement();
    }
    else
      return smlGetCmd(fSessionP->getSmlWorkspaceID(),fGetElementP)==SML_ERR_OK;
  }
  else {
    DEBUGPRINTFX(DBG_ERROR,("*** Tried to issue NULL get"));
  }
  // return true if command must be queued for status/result response reception
  return queueForResponse();
} // TGetCommand::issue


bool TGetCommand::statusEssential(void)
{
  // get status is not essential in lenient mode
  return !(fSessionP->fLenientMode);
} // TGetCommand::statusEssential


// handle status received for previously issued command
// returns true if done, false if command must be kept in the status queue
bool TGetCommand::handleStatus(TStatusCommand *aStatusCmdP)
{
  // base class just handles common cases
  TSyError statuscode = aStatusCmdP->getStatusCode();
  if (statuscode>=500) {
    // %%% for SCTS, which rejects GET with status 500 in some cases
    POBJDEBUGPRINTFX(fSessionP,DBG_ERROR,("Status: %hd: 5xx for get is passed w/o error",statuscode));
    return true;
  }
  // let ancestor analyze
  return TSmlCommand::handleStatus(aStatusCmdP);
} // TGetCommand::handleStatus




void TGetCommand::FreeSmlElement(void)
{
  // remove SyncML toolkit element(s)
  FREEPROTOELEMENT(fGetElementP);
} // TGetCommand::FreeSmlElement


TGetCommand::~TGetCommand()
{
  // free command elements, if any (use explicit invocation as this is a destructor)
  TGetCommand::FreeSmlElement();
} // TGetCommand::~TGetCommand


/* end of TGetCommand implementation */

/*
 * Implementation of TPutCommand
 */


// constructor for receiving command
TPutCommand::TPutCommand(
  TSyncSession *aSessionP,  // associated session (for callbacks)
  uInt32 aMsgID,              // the Message ID of the command
  SmlGetPtr_t aPutElementP  // associated syncml protocol element
) :
  TSmlCommand(scmd_put,false,aSessionP,aMsgID)
{
  // save element
  fPutElementP = aPutElementP;
} // TPutCommand::TPutCommand


// constructor for sending command
TPutCommand::TPutCommand(
  TSyncSession *aSessionP              // associated session (for callbacks)
) :
  TSmlCommand(scmd_put,true,aSessionP)
{
  // create internal get element
  fPutElementP = SML_NEW(SmlPut_t);
  // set proto element type to make it auto-disposable
  fPutElementP->elementType=SML_PE_PUT;
  // Cmd ID is now empty (will be set when issued)
  fPutElementP->cmdID=NULL;
  // default to no flags (noResp is set at issue, if at all)
  fPutElementP->flags=0;
  // no optional elements for now
  fPutElementP->cred=NULL;
  fPutElementP->lang=NULL;
  fPutElementP->meta=NULL;
  fPutElementP->itemList=NULL;
} // TPutCommand::TPutCommand


// add Meta to the get command
void TPutCommand::setMeta(
  SmlPcdataPtr_t aMetaP // existing meta data structure, ownership is passed to Get
)
{
  if (fPutElementP)
    fPutElementP->meta=aMetaP;
} // TPutCommand::setMeta


// add an Item to the get command
void TPutCommand::addItem(
  SmlItemPtr_t aItemP // existing item data structure, ownership is passed to Get
)
{
  if (fPutElementP)
    addItemToList(aItemP,&(fPutElementP->itemList));
} // TPutCommand::addItem


// add a source specification Item to the put command and return a pointer
// to add data to it
SmlItemPtr_t TPutCommand::addSourceLocItem(
  const char *aTargetURI,
  const char *aTargetName
)
{
  SmlItemPtr_t itemP = newItem();

  itemP->source = newLocation(aTargetURI,aTargetName);
  addItem(itemP);
  return itemP;
} // TPutCommand::addSourceLocItem


// analyze command (but do not yet execute)
bool TPutCommand::analyze(TPackageStates aPackageState)
{
  TSmlCommand::analyze(aPackageState);
  // get Command ID and flags
  if (fPutElementP) {
    StartProcessing(fPutElementP->cmdID,fPutElementP->flags);
    return true;
  }
  else return false; // no proto element, bad command
} // TPutCommand::analyze


// execute command (perform real actions, generate status)
// returns true if command has executed and can be deleted
bool TPutCommand::execute(void)
{
  TStatusCommand *statusCmdP=NULL;
  SmlItemListPtr_t thisitemnode,nextitemnode;

  SYSYNC_TRY {
    // process items
    thisitemnode=fPutElementP->itemList;
    while (thisitemnode) {
      nextitemnode = thisitemnode->next;
      // no result nor status so far
      statusCmdP=NULL;
      if (!thisitemnode->item) SYSYNC_THROW(TSyncException("Put with NULL item"));
      // get Source LocURI
      string locURI=smlSrcTargLocURIToCharP(thisitemnode->item->source);
      DEBUGPRINTFX(DBG_HOT,("processing item with locURI=%s",locURI.c_str()));
      // let session (and descendants) process it
      // - default to not allowed to put status
      statusCmdP = newStatusCommand(403);
      fSessionP->processPutResultItem(true,locURI.c_str(),this,thisitemnode->item,*statusCmdP);
      // now complete and issue status
      statusCmdP->addSourceRef(locURI.c_str()); // add source ref
      ISSUE_COMMAND_ROOT(fSessionP,statusCmdP);
      // next item
      thisitemnode=nextitemnode;
    }
    // free element
    FreeSmlElement();
  }
  SYSYNC_CATCH (...)
    // make sure owned objects in local scope are deleted
    if (statusCmdP) delete statusCmdP;
    // re-throw
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
  // done with command, delete it now: return true
  return true;
} // TPutCommand::execute


// returns true if command must be put to the waiting-for-status queue.
// If false, command can be deleted
bool TPutCommand::issue(
  uInt32 aAsCmdID,     // command ID to be used
  uInt32 aInMsgID,     // message ID in which command is being issued
  bool aNoResp
)
{
  // prepare basic stuff
  TSmlCommand::issue(aAsCmdID,aInMsgID,aNoResp);
  // now issue
  if (fPutElementP) {
    // issue command with SyncML toolkit
    PrepareIssue(&fPutElementP->cmdID,&fPutElementP->flags);
    if (!fEvalMode) {
      #ifdef SYDEBUG
      if (fSessionP->fXMLtranslate && fSessionP->fOutgoingXMLInstance)
        smlPutCmd(fSessionP->fOutgoingXMLInstance,fPutElementP);
      #endif
      Ret_t err;
      if ((err=smlPutCmd(fSessionP->getSmlWorkspaceID(),fPutElementP))!=SML_ERR_OK) {
        SYSYNC_THROW(TSmlException("smlPutCmd",err));
      }
      FinalizeIssue();
      // we don't need the status structure any more, free (and NULL ptr) now
      FreeSmlElement();
    }
    else
      return smlPutCmd(fSessionP->getSmlWorkspaceID(),fPutElementP)==SML_ERR_OK;
  }
  else {
    DEBUGPRINTFX(DBG_ERROR,("*** Tried to issue NULL put"));
  }
  // return true if command must be queued for status/result response reception
  return queueForResponse();
} // TPutCommand::issue


void TPutCommand::FreeSmlElement(void)
{
  // remove SyncML toolkit element(s)
  FREEPROTOELEMENT(fPutElementP);
} // TPutCommand::FreeSmlElement


TPutCommand::~TPutCommand()
{
  // free command elements, if any (use explicit invocation as this is a destructor)
  TPutCommand::FreeSmlElement();
} // TPutCommand::~TPutCommand


/* end of TPutCommand implementation */


/*
 * Implementation of TStatusCommand
 */


// constructor for generating status
TStatusCommand::TStatusCommand(
  TSyncSession *aSessionP,        // associated session (for callbacks)
  TSmlCommand *aCommandP,         // command this status refers to
  TSyError aStatusCode                 // status code
) :
  TSmlCommand(scmd_status,true,aSessionP)
{
  // save status code
  fStatusCode=aStatusCode;
  // get reference info
  if (aCommandP) {
    // referring to a command
    fRefMsgID=aCommandP->getMsgID();
    fRefCmdID=aCommandP->getCmdID();
    fRefCmdType=aCommandP->getCmdType();
    // - set flag to suppress sending this status when NoResp is set
    fDontSend= aCommandP->getNoResp() || fSessionP->getMsgNoResp();
  }
  else {
    // referring to SyncHdr of current message
    fRefMsgID=fSessionP->fIncomingMsgID;
    fRefCmdID=0; // "command" ID of SyncHdr is 0
    fRefCmdType=scmd_synchdr; // is SyncHdr
    // - set flag to suppress sending this status when NoResp is set
    fDontSend=fSessionP->getMsgNoResp();
  }
  // create internal status element
  fStatusElementP = SML_NEW(SmlStatus_t);
  // set proto element type to make it auto-disposable
  fStatusElementP->elementType=SML_PE_STATUS;
  // Cmd ID is now empty (will be set at Issue to ensure correct sequential ordering in case this status is not immediately sent)
  fStatusElementP->cmdID=NULL;
  // status refers to message of specified command
  fStatusElementP->msgRef=newPCDataLong(fRefMsgID);
  // status refers to ID of specified command
  fStatusElementP->cmdRef=newPCDataLong(fRefCmdID);
  // add name
  fStatusElementP->cmd=newPCDataString(getNameOf(fRefCmdType));
  // no status code yet (will be added at issue())
  fStatusElementP->data=NULL;
  // no optional elements for now
  fStatusElementP->targetRefList=NULL;
  fStatusElementP->sourceRefList=NULL;
  fStatusElementP->cred=NULL;
  fStatusElementP->chal=NULL;
  fStatusElementP->itemList=NULL;
} // TStatusCommand::TStatusCommand


// %%%% (later obsolete) constructor for generating status w/o having a command object
TStatusCommand::TStatusCommand(
  TSyncSession *aSessionP,        // associated session (for callbacks)
  uInt32 aRefCmdID,                  // referred-to command ID
  TSmlCommandTypes aRefCmdType,   // referred-to command type (scmd_xxx)
  bool aNoResp,                   // set if no-Resp
  TSyError aStatusCode                 // status code
) :
  TSmlCommand(scmd_status,true,aSessionP)
{
  // save status code
  fStatusCode=aStatusCode;
  // get reference info
  fRefMsgID=fSessionP->fIncomingMsgID;
  fRefCmdID=aRefCmdID;
  fRefCmdType=aRefCmdType;
  // - set flag to suppress sending this status when NoResp is set
  fDontSend=aNoResp;
  // create internal status element
  fStatusElementP = SML_NEW(SmlStatus_t);
  // set proto element type to make it auto-disposable
  fStatusElementP->elementType=SML_PE_STATUS;
  // Cmd ID is now empty (will be set at Issue to ensure correct sequential ordering in case this status is not immediately sent)
  fStatusElementP->cmdID=NULL;
  // status refers to message of specified command
  fStatusElementP->msgRef=newPCDataLong(fRefMsgID);
  // status refers to ID of specified command
  fStatusElementP->cmdRef=newPCDataLong(fRefCmdID);
  // add name
  fStatusElementP->cmd=newPCDataString(getNameOf(fRefCmdType));
  // no status code yet (will be added at issue())
  fStatusElementP->data=NULL;
  // no optional elements for now
  fStatusElementP->targetRefList=NULL;
  fStatusElementP->sourceRefList=NULL;
  fStatusElementP->cred=NULL;
  fStatusElementP->chal=NULL;
  fStatusElementP->itemList=NULL;
} // TStatusCommand::TStatusCommand


// constructor for receiving status
TStatusCommand::TStatusCommand(
  TSyncSession *aSessionP,        // associated session (for callbacks)
  uInt32 aMsgID,                    // the Message ID of the command
  SmlStatusPtr_t aStatusElementP  // associated STATUS command content element
) :
  TSmlCommand(scmd_status,false,aSessionP,aMsgID)
{
  // save element
  fStatusElementP = aStatusElementP;
} // TStatusCommand::TStatusCommand


// analyze status
bool TStatusCommand::analyze(TPackageStates aPackageState)
{
  TSmlCommand::analyze(aPackageState);
  // get Command ID and flags
  if (fStatusElementP) {
    StartProcessing(fStatusElementP->cmdID,0);
    // get status code
    sInt32 temp;
    if (smlPCDataToLong(fStatusElementP->data,temp)) fStatusCode=temp;
    else {
      PDEBUGPRINTFX(DBG_ERROR,("Malformed status code: '%s'",smlPCDataToCharP(fStatusElementP->data)));
      return false; // bad status
    }
    // get message reference
    if (!fStatusElementP->msgRef) {
      // no MsgRef -> must assume 1 (according to SyncML specs)
      fRefMsgID=1;
      DEBUGPRINTFX(DBG_PROTO,("No MsgRef in Status, assumed 1"));
    }
    else if (!smlPCDataToULong(fStatusElementP->msgRef,fRefMsgID)) {
      PDEBUGPRINTFX(DBG_ERROR,("Malformed message reference: '%s'",smlPCDataToCharP(fStatusElementP->msgRef)));
      return false; // bad status
    }
    // get command reference
    if (!smlPCDataToULong(fStatusElementP->cmdRef,fRefCmdID)) {
      PDEBUGPRINTFX(DBG_ERROR,("Malformed command reference: '%s'",smlPCDataToCharP(fStatusElementP->msgRef)));
      return false; // bad status
    }
    #ifdef SYDEBUG
    // warn if error (don't treat slow sync status or conflict indication as errors)
    // - 418 = item already exits: sent by Funambol server when both client and
    //   and server have a new item which is considered identical by the server
    //   (must be really identical, minor difference will lead to a merged item
    //   which is sent back to the client without the 418). See Moblin Bugzilla #4599.
    if (fStatusCode>=300 && fStatusCode!=508 && fStatusCode!=419 && fStatusCode!=418) {
      PDEBUGPRINTFX(DBG_ERROR,(
        "WARNING: RECEIVED NON-OK STATUS %hd for &html;<a name=\"SO_%ld_%ld\" href=\"#IO_%ld_%ld\">&html;command '%s'&html;</a>&html; (outgoing MsgID=%ld, CmdID=%ld)",
        fStatusCode,
        (long)fRefMsgID,
        (long)fRefCmdID,
        (long)fRefMsgID,
        (long)fRefCmdID,
        smlPCDataToCharP(fStatusElementP->cmd),
        (long)fRefMsgID,
        (long)fRefCmdID
      ));
    }
    else {
      // show what we have received
      PDEBUGPRINTFX(DBG_HOT,(
        "RECEIVED STATUS %hd for for &html;<a name=\"SO_%ld_%ld\" href=\"#IO_%ld_%ld\">&html;command '%s'&html;</a>&html; (outgoing MsgID=%ld, CmdID=%ld)",
        fStatusCode,
        (long)fRefMsgID,
        (long)fRefCmdID,
        (long)fRefMsgID,
        (long)fRefCmdID,
        smlPCDataToCharP(fStatusElementP->cmd),
        (long)fRefMsgID,
        (long)fRefCmdID
      ));
    }
    // - source and target refs
    if (PDEBUGMASK & DBG_HOT) {
      SmlTargetRefListPtr_t targetrefP = fStatusElementP->targetRefList;
      while (targetrefP) {
        // target ref available
        PDEBUGPRINTFX(DBG_HOT,("- TargetRef (remoteID) = '%s'",smlPCDataToCharP(targetrefP->targetRef)));
        // next
        targetrefP=targetrefP->next;
      }
      SmlSourceRefListPtr_t sourcerefP = fStatusElementP->sourceRefList;
      while (sourcerefP) {
        // target ref available
        PDEBUGPRINTFX(DBG_HOT,("- SourceRef (localID) = '%s'",smlPCDataToCharP(sourcerefP->sourceRef)));
        // next
        sourcerefP=sourcerefP->next;
      }
    }
    // - optional items
    if (PDEBUGMASK & DBG_ERROR) {
      SmlItemListPtr_t itemlistP =  fStatusElementP->itemList;
      while (itemlistP) {
        // item available
        PDEBUGPRINTFX(DBG_HOT,("- Item data = %s",smlItemDataToCharP(itemlistP->item)));
        // next
        itemlistP=itemlistP->next;
      }
    }
    #endif
    return true; // good status
  }
  else return false; // no proto element, bad command
} // TStatusCommand::analyze



// returns true if command must be put to the waiting-for-status queue.
// If false, command can be deleted
bool TStatusCommand::issue(
  uInt32 aAsCmdID,     // command ID to be used
  uInt32 aInMsgID,     // message ID in which command is being issued
  bool aNoResp       // dummy here, because Status has always no response
)
{
  // now issue
  if (fStatusElementP) {
    // prepare basic stuff
    TSmlCommand::issue(aAsCmdID,aInMsgID,true);
    // set status code in structure (but only once, in case we are evaluating before issuing)
    if (fStatusElementP->data==NULL) fStatusElementP->data=newPCDataLong(fStatusCode);
    // Status is meant to have a CmdID. SyncML 1.0 docs say no, but this was corrected in 1.0.1
    // issue command with SyncML toolkit (no flags)
    PrepareIssue(&fStatusElementP->cmdID,NULL); // CmdID (SyncML 1.0.1 conformant)
    if (!fEvalMode) {
      // PrepareIssue(NULL,NULL); // no CmdID, no flags
      #ifdef SYDEBUG
      if (fSessionP->fXMLtranslate && fSessionP->fOutgoingXMLInstance)
        smlStatusCmd(fSessionP->fOutgoingXMLInstance,fStatusElementP);
      #endif
      Ret_t err;
      if ((err=smlStatusCmd(fSessionP->getSmlWorkspaceID(),fStatusElementP))!=SML_ERR_OK) {
        SYSYNC_THROW(TSmlException("smlStatusCmd",err));
      }
      FinalizeIssue();
      // show debug
      #ifdef SYDEBUG
      // - warning for non-ok (don't treat slow sync status as error)
      if (fStatusCode>=300 && fStatusCode!=508) {
        PDEBUGPRINTFX(DBG_ERROR,("WARNING: Non-OK Status %hd returned to remote!",fStatusCode));
      }
      // - what was issued
      PDEBUGPRINTFX(DBG_HOT,("Status Code %s issued for Cmd=%s, (incoming MsgID=%s, CmdID=%s)",
        smlPCDataToCharP(fStatusElementP->data),
        smlPCDataToCharP(fStatusElementP->cmd),
        smlPCDataToCharP(fStatusElementP->msgRef),
        smlPCDataToCharP(fStatusElementP->cmdRef)
      ));
      // - source and target refs
      if (PDEBUGTEST(DBG_HOT)) {
        SmlTargetRefListPtr_t targetrefP = fStatusElementP->targetRefList;
        while (targetrefP) {
          // target ref available
          PDEBUGPRINTFX(DBG_HOT,("- TargetRef (localID) = '%s'",smlPCDataToCharP(targetrefP->targetRef)))
          // next
          targetrefP=targetrefP->next;
        }
        SmlSourceRefListPtr_t sourcerefP = fStatusElementP->sourceRefList;
        while (sourcerefP) {
          // target ref available
          PDEBUGPRINTFX(DBG_HOT,("- SourceRef (remoteID) = '%s'",smlPCDataToCharP(sourcerefP->sourceRef)))
          // next
          sourcerefP=sourcerefP->next;
        }
      }
      #endif
      // we don't need the status structure any more, free (and NULL ptr) now
      FreeSmlElement();
    }
    else
      return smlStatusCmd(fSessionP->getSmlWorkspaceID(),fStatusElementP)==SML_ERR_OK;
  }
  else {
    DEBUGPRINTFX(DBG_ERROR,("*** Tried to issue NULL status"));
  }
  // status does NOT await any answer, and can be deleted after issuing
  return false; // don't queue for status
} // TStatusCommand::issue


// add a target Ref to the status (no op if ref is NULL)
void TStatusCommand::addTargetRef(
  const char *aTargetRef // Target LocURI of an item this status applies to
)
{
  SmlTargetRefListPtr_t *targetreflistPP;
  if (fStatusElementP && aTargetRef && *aTargetRef) {
    // Note: empty refs will not be added
    targetreflistPP=&(fStatusElementP->targetRefList);
    // find last targetreflist pointer
    while (*targetreflistPP) {
      targetreflistPP=&((*targetreflistPP)->next);
    }
    // targetreflistPP now points to a NULL pointer which must be replaced by addr of new targetreflist entry
    *targetreflistPP = SML_NEW(SmlTargetRefList_t);
    (*targetreflistPP)->next=NULL;
    (*targetreflistPP)->targetRef=
      newPCDataString(aTargetRef);
  }
} // TStatusCommand::addTargetRef


// add a source Ref to the status (no op if ref is NULL)
void TStatusCommand::addSourceRef(
  const char *aSourceRef // Target LocURI of an item this status applies to
)
{
  SmlSourceRefListPtr_t *sourcereflistPP;
  if (fStatusElementP && aSourceRef && *aSourceRef) {
    // Note: empty refs will not be added
    sourcereflistPP=&(fStatusElementP->sourceRefList);
    // find last sourcereflist pointer
    while (*sourcereflistPP) {
      sourcereflistPP=&((*sourcereflistPP)->next);
    }
    // sourcereflistPP now points to a NULL pointer which must be replaced by addr of new sourcereflist entry
    *sourcereflistPP = SML_NEW(SmlSourceRefList_t);
    (*sourcereflistPP)->next=NULL;
    (*sourcereflistPP)->sourceRef=
      newPCDataString(aSourceRef);
  }
} // TStatusCommand::addSourceRef


// add a String Item to the status (only if not empty)
void TStatusCommand::addItemString(
  const char *aItemString // item string to be added
)
{
  if (fStatusElementP && aItemString && *aItemString) {
    addItem(newStringDataItem(aItemString));
  }
} // TStatusCommand::addItemString


// add an Item to the status
void TStatusCommand::addItem(
  SmlItemPtr_t aItemP // existing item data structure, ownership is passed to Status
)
{
  if (fStatusElementP)
    addItemToList(aItemP,&(fStatusElementP->itemList));
} // TStatusCommand::addItem


// move items from another status to this one (passes ownership of items and deletes them from original)
void TStatusCommand::moveItemsFrom(TStatusCommand *aStatusCommandP)
{
  if (fStatusElementP && aStatusCommandP) {
    // pass item list to new owner
    fStatusElementP->itemList = aStatusCommandP->fStatusElementP->itemList;
    // remove items from original
    aStatusCommandP->fStatusElementP->itemList = NULL;
  }
} // TStatusCommand::moveItemsFrom



// add Challenge to status
// challenge data structure ownership is passed to Status
void TStatusCommand::setChallenge(SmlChalPtr_t aChallengeP)
{
  if (fStatusElementP) {
    if (fStatusElementP->chal) {
      // get rid of old challenge
      smlFreeChalPtr(fStatusElementP->chal);
      fStatusElementP->chal=NULL;
    }
    // set new
    fStatusElementP->chal=aChallengeP;
  }
} // TStatusCommand::setChallenge


// set Status code
void TStatusCommand::setStatusCode(TSyError aStatusCode)
{
  fStatusCode = aStatusCode;
} // TStatusCommand::setStatusCode


void TStatusCommand::FreeSmlElement(void)
{
  // remove SyncML toolkit element(s)
  FREEPROTOELEMENT(fStatusElementP);
} // TResultsCommand::FreeSmlElement


TStatusCommand::~TStatusCommand()
{
  // free command elements, if any (use explicit invocation as this is a destructor)
  TStatusCommand::FreeSmlElement();
} // TStatusCommand::TStatusCommand


/* end of TStatusCommand implementation */


/*
 * Implementation of TResultsCommand
 */

// constructor for receiving command
TResultsCommand::TResultsCommand(
  TSyncSession *aSessionP,  // associated session (for callbacks)
  uInt32 aMsgID,              // the Message ID of the command
  SmlResultsPtr_t aResultsElementP  // associated syncml protocol element
) :
  TSmlCommand(scmd_results,false,aSessionP,aMsgID)
{
  // save element
  fResultsElementP = aResultsElementP;
} // TResultsCommand::TResultsCommand


// constructor for generating results
TResultsCommand::TResultsCommand(
  TSyncSession *aSessionP,        // associated session (for callbacks)
  TSmlCommand *aCommandP,         // command these results refer to
  const char *aTargetRef,         // target reference
  const char *aSourceRef          // source reference
) :
  TSmlCommand(scmd_results,true,aSessionP)
{
  // create internal results element
  fResultsElementP = SML_NEW(SmlResults_t);
  // set proto element type to make it auto-disposable
  fResultsElementP->elementType=SML_PE_RESULTS;
  // Cmd ID is now empty (will be set when issued)
  fResultsElementP->cmdID=NULL;
  // results refer to message of specified command
  fResultsElementP->msgRef=newPCDataLong(aCommandP->getMsgID());
  // results refer to ID of specified command
  fResultsElementP->cmdRef=newPCDataLong(aCommandP->getCmdID());
  // no meta for now
  fResultsElementP->meta=NULL;
  // target reference if specified
  if (aTargetRef) fResultsElementP->targetRef=newPCDataString(aTargetRef);
  else fResultsElementP->targetRef=NULL;
  // source reference if specified
  if (aSourceRef) fResultsElementP->sourceRef=newPCDataString(aSourceRef);
  else fResultsElementP->sourceRef=NULL;
  // no items for now
  fResultsElementP->itemList=NULL;
} // TResultsCommand::TResultsCommand



// returns true if command must be put to the waiting-for-status queue.
// If false, command can be deleted
bool TResultsCommand::issue(
  uInt32 aAsCmdID,     // command ID to be used
  uInt32 aInMsgID,     // message ID in which command is being issued
  bool aNoResp       // issue without wanting response
)
{
  // prepare
  TSmlCommand::issue(aAsCmdID,aInMsgID,aNoResp);
  // now issue
  if (fResultsElementP) {
    // issue command with SyncML toolkit (no flags)
    PrepareIssue(&fResultsElementP->cmdID,NULL);
    if (!fEvalMode) {
      #ifdef SYDEBUG
      if (fSessionP->fXMLtranslate && fSessionP->fOutgoingXMLInstance)
        smlResultsCmd(fSessionP->fOutgoingXMLInstance,fResultsElementP);
      #endif
      Ret_t err;
      if ((err=smlResultsCmd(fSessionP->getSmlWorkspaceID(),fResultsElementP))!=SML_ERR_OK) {
        SYSYNC_THROW(TSmlException("smlResultsCmd",err));
      }
      FinalizeIssue();
      // show debug
      DEBUGPRINTFX(DBG_HOT,("results issued for (incoming MsgID=%s, CmdID=%s)",
        smlPCDataToCharP(fResultsElementP->msgRef),
        smlPCDataToCharP(fResultsElementP->cmdRef)
      ));
      // we don't need the results structure any more, free (and NULL ptr) now
      FreeSmlElement();
    }
    else
      return smlResultsCmd(fSessionP->getSmlWorkspaceID(),fResultsElementP)==SML_ERR_OK;
  }
  else {
    DEBUGPRINTFX(DBG_ERROR,("*** Tried to issue NULL result"));
  }
  // SyncML 1.0: result does NOT await any answer, and can be deleted after issuing
  // SyncML 1.0.1: all commands will receive a status (except Status)
  return true; // queue for status/result
} // TResultsCommand::issue


// add a String Item to the results
void TResultsCommand::addItemString(
  const char *aItemString // item string to be added
)
{
  if (fResultsElementP && aItemString) {
    addItem(newStringDataItem(aItemString));
  }
} // TResultsCommand::addItemString


// add an Item to the results
void TResultsCommand::addItem(
  SmlItemPtr_t aItemP // existing item data structure, ownership is passed to Results
)
{
  if (fResultsElementP)
    addItemToList(aItemP,&(fResultsElementP->itemList));
} // TResultsCommand::addItem


// set Meta of the results command
void TResultsCommand::setMeta(
  SmlPcdataPtr_t aMetaP // existing meta data structure, ownership is passed to Get
)
{
  if (fResultsElementP)
    fResultsElementP->meta=aMetaP;
} // TResultsCommand::setMeta




// analyze command (but do not yet execute)
bool TResultsCommand::analyze(TPackageStates aPackageState)
{
  TSmlCommand::analyze(aPackageState);
  // get Command ID and flags
  if (fResultsElementP) {
    StartProcessing(fResultsElementP->cmdID,0);
    return true;
  }
  else return false; // no proto element, bad command
} // TResultsCommand::analyze


// execute command (perform real actions, generate status)
// returns true if command has executed and can be deleted
bool TResultsCommand::execute(void)
{
  TStatusCommand *statusCmdP=NULL;
  SmlItemListPtr_t thisitemnode,nextitemnode;

  // process items
  thisitemnode=fResultsElementP->itemList;
  while (thisitemnode) {
    nextitemnode = thisitemnode->next;
    if (!thisitemnode->item) SYSYNC_THROW(TSyncException("Results with NULL item"));
    // get Source LocURI
    string locURI=smlSrcTargLocURIToCharP(thisitemnode->item->source);
    DEBUGPRINTFX(DBG_HOT,("processing item with locURI=%s",locURI.c_str()));
    // let session (and descendants) process it
    // - default to ok status
    statusCmdP = newStatusCommand(200);
    fSessionP->processPutResultItem(false,locURI.c_str(),this,thisitemnode->item,*statusCmdP);
    #ifdef RESULTS_SENDS_STATUS
    // now complete and issue status
    statusCmdP->addSourceRef(locURI.c_str()); // add source ref
    ISSUE_COMMAND_ROOT(fSessionP,statusCmdP);
    #else
    // suppress (forget) status
    delete statusCmdP;
    #endif
    // next item
    thisitemnode=nextitemnode;
  }
  // free element
  FreeSmlElement();
  // done with command, delete it now: return true
  return true;
} // TResultsCommand::execute


void TResultsCommand::FreeSmlElement(void)
{
  // remove SyncML toolkit element(s)
  FREEPROTOELEMENT(fResultsElementP);
} // TResultsCommand::FreeSmlElement


TResultsCommand::~TResultsCommand()
{
  // free command elements, if any (use explicit invocation as this is a destructor)
  TResultsCommand::FreeSmlElement();
} // TResultsCommand::TResultsCommand


/* end of TResultsCommand implementation */


/*
 * Implementation of TDevInfResultsCommand
 */


// constructor for generating devInf results
TDevInfResultsCommand::TDevInfResultsCommand(
  TSyncSession *aSessionP,        // associated session (for callbacks)
  TSmlCommand *aCommandP          // command these results refer to
) :
  /* %%% if strictly following example in SyncML protocol specs
   *     DevInf results don't have a source or target ref, only the item has
  TResultsCommand(aSessionP,aCommandP,SYNCML_DEVINF_LOCURI,NULL)
   */
  TResultsCommand(aSessionP,aCommandP,NULL,NULL)
{
  // standard result is now created
  // - create meta type
  /* %%% if strictly following example in SyncML protocol specs, meta
   *     must be defined in the result/put command, not the individual item
   */
  string metatype=SYNCML_DEVINF_META_TYPE;
  fSessionP->addEncoding(metatype);
  fResultsElementP->meta=newMetaType(metatype.c_str());
  // - add local DevInf item to result
  //   Note: explicit GET always returns entire devInf (all datastores)
  addItem(fSessionP->getLocalDevInfItem(false,false));
} // TDevInfResultsCommand::TDevInfResultsCommand


// - try to shrink command by at least aReduceByBytes
//   Returns false if shrink is not possible
bool TDevInfResultsCommand::shrinkCommand(sInt32 aReduceByBytes)
{
  // measure current size (with current CmdID/MsgID)
  sInt32 origfree = evalIssue(fCmdID,fMsgID,fNoResp);
  // extract original devInf
  SmlItemPtr_t origDevInf = fResultsElementP->itemList->item;
  // remove it from the item list
  fResultsElementP->itemList->item = NULL;
  smlFreeItemList(fResultsElementP->itemList);
  fResultsElementP->itemList = NULL;
  // create reduced version of the devInf
  // - only alerted datastores if initialisation is complete (i.e. we KNOW which datastores the sync is about)
  // - no CTCap property lists
  SmlItemPtr_t reducedDevInf =
    fSessionP->getLocalDevInfItem(
      fSessionP->getIncomingState()>psta_init, // alerted only if init complete
      true // no CTCap property lists
    );
  // insert it into command
  addItem(reducedDevInf);
  // measure again
  sInt32 nowfree = evalIssue(fCmdID,fMsgID,fNoResp);
  // decide if we are successful
  if (nowfree-origfree>=aReduceByBytes) {
    // new devinf matches size requirement
    smlFreeItemPtr(origDevInf); // discard original devInf
    return true; // shrink successful
  }
  else {
    // new reduced devInf is still too big
    fResultsElementP->itemList->item = origDevInf; // restore oiginal devInf
    smlFreeItemPtr(reducedDevInf); // discard reduced one
    return false; // cannot shrink enough
  }
} // TDevInfResultsCommand::shrinkCommand



// mark devinf as sent when issuing command
bool TDevInfResultsCommand::issue(
  uInt32 aAsCmdID,     // command ID to be used
  uInt32 aInMsgID,     // message ID in which command is being issued
  bool aNoResp
)
{
  fSessionP->remoteGotDevinf();
  return TResultsCommand::issue(aAsCmdID,aInMsgID,aNoResp);
} // TDevInfResultsCommand::issue


void TDevInfResultsCommand::FreeSmlElement(void)
{
  // remove SyncML toolkit element(s)
  // - result might already be freed!
  if (fResultsElementP) {
    FREEPROTOELEMENT(fResultsElementP);
  }
} // TDevInfResultsCommand::FreeSmlElement


TDevInfResultsCommand::~TDevInfResultsCommand()
{
  // free command elements, if any (use explicit invocation as this is a destructor)
  TDevInfResultsCommand::FreeSmlElement();
  // NOTE: fResultsElementP is NULLed so invocation of base class destuctor does no harm
} // TDevInfResultsCommand::TDevInfResultsCommand


/* end of TDevInfResultsCommand implementation */


/*
 * Implementation of TDevInfResultsCommand
 */


// constructor for generating devinf Put command
TDevInfPutCommand::TDevInfPutCommand(
  TSyncSession *aSessionP         // associated session (for callbacks)
) :
  TPutCommand(aSessionP)
{
  // standard put is now created
  // - create meta type
  /* %%% if strictly following example in SyncML protocol specs, meta
   *     must be defined in the result/put command, not the individual item
   */
  string metatype=SYNCML_DEVINF_META_TYPE;
  fSessionP->addEncoding(metatype);
  fPutElementP->meta=newMetaType(metatype.c_str());
  // - add local DevInf item to result
  //   Note: PUT of server only returns alerted datastore's devInf
  addItem(fSessionP->getLocalDevInfItem(IS_SERVER,false));
} // TDevInfPutCommand::TDevInfPutCommand


// mark devinf as sent when issuing command
bool TDevInfPutCommand::issue(
  uInt32 aAsCmdID,     // command ID to be used
  uInt32 aInMsgID,     // message ID in which command is being issued
  bool aNoResp
)
{
  fSessionP->remoteGotDevinf();
  return TPutCommand::issue(aAsCmdID,aInMsgID,aNoResp);
} // TDevInfPutCommand::issue


void TDevInfPutCommand::FreeSmlElement(void)
{
  // remove SyncML toolkit element(s)
  // - result might already be freed!
  if (fPutElementP) {
    FREEPROTOELEMENT(fPutElementP);
  }
} // TDevInfPutCommand::FreeSmlElement


TDevInfPutCommand::~TDevInfPutCommand()
{
  // free command elements, if any (use explicit invocation as this is a destructor)
  TDevInfPutCommand::FreeSmlElement();
  // NOTE: fResultsElementP is NULLed so invocation of base class destuctor does not harm
} // TDevInfPutCommand::~TDevInfPutCommand

/* end of TDevInfPutCommand implementation */

#endif // SYNCCOMMAND_PART2_EXCLUDE

// eof
