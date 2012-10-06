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

#ifndef SyncCommand_H
#define SyncCommand_H

// includes
#include "sysync.h"



using namespace sysync;


namespace sysync {


// forward
class TSyncDataStore;
class TLocalEngineDS;
class TRemoteDataStore;

// default command size
#define DEFAULTCOMMANDSIZE 200;


// Command Types (Note: not only strictly commands, but all
// SyncML protocol items that need command-like processin
// are wrapped in a command, too, e.g. "SyncHdr"
typedef enum {
  scmd_synchdr,
  scmd_sync,
  scmd_syncend,
  scmd_add,
  scmd_alert,
  scmd_delete,
  scmd_get,
  scmd_put,
  scmd_map,
  scmd_results,
  scmd_status,
  scmd_replace,
  scmd_copy,
  scmd_move,
  scmd_sequence,
  scmd_atomic,
  scmd_unknown,
  numSmlCommandTypes
} TSmlCommandTypes;

// forward
class TStatusCommand;
class TSyncSession;
class TSyncAppBase;


// abstract base command class
class TSmlCommand
{
public:
  // constructor for ALL commands
  TSmlCommand(
    TSmlCommandTypes aCmdType,    // the command type
    bool aOutgoing,               // set if this is a outgoing command (to avoid confusion)
    TSyncSession *aSessionP,      // associated session (for callbacks)
    uInt32 aMsgID=0                 // the Message ID of the command (0=none yet)
  );
  virtual ~TSmlCommand();
  // methods
  #ifdef SYDEBUG
  TDebugLogger *getDbgLogger(void);
  uInt32 getDbgMask(void);
  #endif
  // - get session owner (dispatcher/clientbase)
  TSyncAppBase *getSyncAppBase(void);
  #ifndef USE_SML_EVALUATION
  // - get (approximated) message size required for sending it
  virtual uInt32 messageSize(void);
  #endif
  // - analyze command (but do not yet execute)
  virtual bool analyze(TPackageStates aPackageState) { fPackageState=aPackageState; return true; }; // returns false if command is bad and cannot be executed
  // - execute command (perform real actions, generate status)
  virtual bool execute(void); // returns true if command could execute, false if it must be queued for later finishing (next message)
  // - get number of bytes that will be still available in the workspace after
  //   sending this command.
  sInt32 evalIssue(
    uInt32 aAsCmdID,     // command ID to be used
    uInt32 aInMsgID,     // message ID in which command is being issued
    bool aNoResp=false // issue without wanting response
  );
  // - issue (send) command to remote party
  //   returns true if command must be put to the waiting-for-status queue.
  //   If returns false, command can be deleted
  virtual bool issue(
    uInt32 aAsCmdID,     // command ID to be used
    uInt32 aInMsgID,     // message ID in which command is being issued
    bool aNoResp=false // issue without wanting response
  );
  // - try to split (SyncML 1.1 moredata mechanism) command by reducing
  //   original command by at least aReduceByBytes and generating a second
  //   command containing the rest of the data
  //   Returns NULL if split is not possible
  virtual TSmlCommand *splitCommand(sInt32 /* aReduceByBytes */) { return NULL; /* normal commands can't be split */ };
  virtual bool canSplit(void) { return false; /* normal commands can't be split */ };
  // - try to shrink command by at least aReduceByBytes
  //   Returns false if shrink is not possible
  virtual bool shrinkCommand(sInt32 /* aReduceByBytes */) { return false; /* normal commands can't be shrunk */ };
  // - possibly substitute data with previous session's buffered left-overs from a chunked transfer
  //   for resuming a chunked item transfer.
  virtual bool checkChunkContinuation(void) { return false; /* normal commands can't split and so can't continue */ };
  // - test if completely issued (must be called after issue() and continueIssue())
  //   or completely executed (must be called after execute() to see if command can be deleted)
  virtual bool finished(void) { return true; }; // normal commands finish after issuing/executing
  // - continue issuing command. This is called by session at start of new message
  //   when finished() returned false after issue()/continueIssue()
  //   (which causes caller of finished() to put command into fInteruptedCommands queue)
  //   returns true if command should be queued for status (again?)
  //   NOTE: continueIssue must make sure that it gets a new CmdID/MsgID in case
  //   the command itself was NOT yet issued (normally this does not happen, but...)
  virtual bool continueIssue(bool & /* aNewIssue */) { return true; }
  // - test if command matches status
  bool matchStatus(TStatusCommand *aStatusCmdP);
  // - handle status received for previously issued command
  //   returns true if done, false if command must be kept in the status queue
  virtual bool handleStatus(TStatusCommand *aStatusCmdP);
  // - mark any syncitems (or other data) for resume. Called for pending commands
  //   when a Suspend alert is received or whenever a resumable state must be saved
  virtual void markPendingForResume(TLocalEngineDS *aForDatastoreP, bool aUnsent) { /* nop */ };
  // - mark item for resend in next sync session (if possible)
  virtual void markForResend(void) { /* nop */ };
  // - generate status depending on fNoResp and session's fMsgNoResp
  //   NOTE: always returns a Status (NoResp is handled by not SENDING status)
  TStatusCommand *newStatusCommand(TSyError aStatusCode, const char *aStringItem=NULL);
  // properties
  bool getNoResp(void) { return fNoResp; }; // noResp condition
  bool getDontSend(void) { return fDontSend; }; // send inhibited condition
  void dontSend(void) { fDontSend=true; }; // prevent sending this command
  void allowFailure(void) { fAllowFailure=true; }; // allow failure (don't abort session if this command returns 4xx or 5xx status)
  const char * getName(void); // name of command
  uInt32 getCmdID(void) { return fCmdID; }; // ID of command
  uInt32 getMsgID(void) { return fMsgID; }; // Message ID
  TSmlCommandTypes getCmdType(void) { return fCmdType; }; // get command type
  bool isWaitingForStatus(void) { return fWaitingForStatus>0; };
  void setWaitingForStatus(bool w) { if (w) fWaitingForStatus++; else if (fWaitingForStatus>0) fWaitingForStatus--; };
  TPackageStates getPackageState(void) { return fPackageState; };
  virtual SmlPcdataPtr_t getMeta(void) { return NULL; };
  virtual bool isSyncOp(void) { return false; };
  virtual bool neverIgnore(void) { return false; }; // normal commands should be ignored when in fIgnoreIncomingCommands state
  virtual bool statusEssential(void) { return true; }; // normal commands MUST receive status
protected:
  // helper methods for derived classes
  // - get name of certain command
  const char *getNameOf(TSmlCommandTypes aCmdType);
  // - start processing a command
  void StartProcessing(
    SmlPcdataPtr_t aCmdID, // ID of command
    Flag_t aFlags // flags of command
  );
  // - prepare elements for issuing
  void PrepareIssue(
    SmlPcdataPtr_t *aCmdID=NULL, // ID of command
    Flag_t *aFlags=NULL // flags of command
  );
  void FinalizeIssue(void); // finalizes issuing a command (updates message size)
  // returns true if command must be queued for status or result response
  virtual bool queueForResponse(void);
  // - generate and issue status depending on fNoResp and session's fMsgNoResp
  void issueStatusCommand(TSyError aStatusCode);
  // - free smlProtoElement structure (if any). Includes precautions if structure contains
  //   parts that are not owned by the command itself
  virtual void FreeSmlElement(void) { /* nop in base class */ };
  // debug
  #ifdef SYDEBUG
  // print to (session-related) debug output channel
  void DebugPrintf(const char *text, ...);
  #endif
  // session
  TSyncSession *fSessionP;
  // command info
  TSmlCommandTypes fCmdType;
  TPackageStates fPackageState; // incoming or outgoing package state of this command (may be different from current state for delayed commands)
  bool fOutgoing; // set if outgoing command
  bool fNoResp;
  uInt32 fCmdID;  // command ID
  uInt32 fMsgID;  // message ID
  // message size calculation
  uInt32 fBytesbefore;
  // commands that should not be sent (e.g. a "response" to a NoResp command)
  bool fDontSend;
  // set to signal to issue that this is a size evaluation issuing, not a real one
  bool fEvalMode;
  // set if PrepareIssue() was already called
  bool fPrepared;
  // set if command may fail with status 4xx or 5xx without aborting the session
  bool fAllowFailure;
private:
  uInt16 fWaitingForStatus; // count for how many statuses a command is waiting
}; // TSmlCommand


// Unimplemented command
class TUnimplementedCommand: public TSmlCommand
{
  typedef TSmlCommand inherited;
public:
  // constructor for generating devinf results
  TUnimplementedCommand(
    TSyncSession *aSessionP,        // associated session (for callbacks)
    uInt32 aMsgID,                    // the Message ID of the command
    SmlPcdataPtr_t aCmdID,          // command ID (as contents are unknown an cannot be analyzed)
    Flag_t aFlags,                  // flags to get fNoResp
    TSmlCommandTypes aCmdType=scmd_unknown,      // command type (for name)
    void *aContentP=NULL,           // associated command content element
    TSyError aStatusCode=406             // status code to be returned on execution (default = unimplemented option 406)
  );
  virtual bool execute(void);
private:
  TSyError fStatusCode;
}; // TUnimplementedCommand


// Sync Header "command"
class TSyncHeader: public TSmlCommand
{
  typedef TSmlCommand inherited;
public:
  // constructor for receiving SyncHdr
  TSyncHeader(
    TSyncSession *aSessionP,              // associated session (for callbacks)
    SmlSyncHdrPtr_t aSyncHdrElementP=NULL // associated SyncHdr content element
  );
  // constructor for sending SyncHdr
  TSyncHeader(
    TSyncSession *aSessionP,              // associated session (for callbacks)
    bool aOutgoingNoResp=false            // if true, entire message will request no responses
  );
  virtual ~TSyncHeader();
  virtual bool execute(void);
  virtual bool issue(
    uInt32 aAsCmdID,     // command ID to be used (is dummy=0 for SyncHdr)
    uInt32 aInMsgID,     // message ID in which command is being issued
    bool aNoResp=false // issue without wanting response FOR ENTIRE message
  ); // returns false, because command can be deleted immediately after issue()
  virtual bool statusEssential(void) { return false; }; // header status is not essential (per se - of course it is for login)
  // - handle status received for previously issued command
  //   returns true if done, false if command must be kept in the status queue
  virtual bool handleStatus(TStatusCommand *aStatusCmdP);
protected:
  virtual void FreeSmlElement(void);
  SmlSyncHdrPtr_t fSyncHdrElementP;
}; // TSyncHeader


// Alert  Command
class TAlertCommand: public TSmlCommand
{
  typedef TSmlCommand inherited;
public:
  // constructor for receiving Alert
  TAlertCommand(
    TSyncSession *aSessionP,              // associated session (for callbacks)
    uInt32 aMsgID,                        // the Message ID of the command
    SmlAlertPtr_t aAlertElementP          // associated Alert content element
  );
  // constructor for sending Alert
  TAlertCommand(
    TSyncSession *aSessionP,              // associated session (for callbacks)
    TLocalEngineDS *aLocalDataStoreP,     // local datastore
    uInt16 aAlertCode                     // Alert code to send
  );
  virtual ~TAlertCommand();
  virtual bool analyze(TPackageStates aPackageState);
  virtual bool execute(void);
  virtual bool issue(
    uInt32 aAsCmdID,     // command ID to be used (is dummy=0 for SyncHdr)
    uInt32 aInMsgID,     // message ID in which command is being issued
    bool aNoResp=false // issue without wanting response
  ); // returns false, because command can be deleted immediately after issue()
  virtual bool statusEssential(void); // depends on alert code if it is essential
  // - add a String Item
  void addItemString(const char *aItemString); // item string to be added
  // - add an Item
  void addItem(SmlItemPtr_t aItemP); // existing item data structure, ownership is passed to Command
  // - handle status received for previously issued command
  //   returns true if done, false if command must be kept in the status queue
  virtual bool handleStatus(TStatusCommand *aStatusCmdP);
protected:
  virtual void FreeSmlElement(void);
  SmlAlertPtr_t fAlertElementP;
  uInt16 fAlertCode; // alert code
  // involved datastore
  TLocalEngineDS *fLocalDataStoreP;
}; // TAlertCommand



// Sync Command
class TSyncCommand: public TSmlCommand
{
  typedef TSmlCommand inherited;
public:
  // constructor for receiving Sync
  TSyncCommand(
    TSyncSession *aSessionP,              // associated session (for callbacks)
    uInt32 aMsgID,                          // the Message ID of the command
    SmlSyncPtr_t aSyncElementP=NULL       // associated Sync content element
  );
  // constructor for sending Sync
  TSyncCommand(
    TSyncSession *aSessionP,              // associated session (for callbacks)
    TLocalEngineDS *aLocalDataStoreP,    // local datastore
    TRemoteDataStore *aRemoteDataStoreP   // remote datastore
  );
  virtual ~TSyncCommand();
  virtual bool analyze(TPackageStates aPackageState);
  virtual bool execute(void);
  virtual bool issue(
    uInt32 aAsCmdID,     // command ID to be used (is dummy=0 for SyncHdr)
    uInt32 aInMsgID,     // message ID in which command is being issued
    bool aNoResp=false // issue without wanting response
  );
  // - test if completely issued (must be called after issue() and continueIssue())
  //   or completely executed (must be called after execute() to see if command can be deleted)
  virtual bool finished(void); // normal command finish
  // - continue issuing command.
  virtual bool continueIssue(bool &aNewIssue);
  // - handle status received for previously issued command
  //   returns true if done, false if command must be kept in the status queue
  virtual bool handleStatus(TStatusCommand *aStatusCmdP);
  // - mark any syncitems (or other data) for resume. Called for pending commands
  //   when a Suspend alert is received or whenever a resumable state must be saved
  virtual void markPendingForResume(TLocalEngineDS *aForDatastoreP, bool aUnsent);
protected:
  virtual void FreeSmlElement(void);
  SmlSyncPtr_t fSyncElementP;
private:
  // internals
  bool generateOpen(void);
  void generateCommandsAndClose(void);
  bool fInProgress; // set if <Sync> bracket must be re-opened in next message (=not finished)
  // variables for sync needing more than one message
  TSmlCommandPContainer fNextMessageCommands;
  TSmlCommand *fInterruptedCommandP;
  // involved datastores and mode
  TLocalEngineDS *fLocalDataStoreP;
  TRemoteDataStore *fRemoteDataStoreP;
}; // TSyncCommand


// Sync End Command (closing <sync> bracket: </sync>)
// Note: used for receiving only, when sending, the closing
//       bracket is part of TSyncCommand production process.
class TSyncEndCommand: public TSmlCommand
{
  typedef TSmlCommand inherited;
public:
  // constructor for receiving </sync>
  TSyncEndCommand(
    TSyncSession *aSessionP,              // associated session (for callbacks)
    uInt32 aMsgID                         // the Message ID of the command
  );
  virtual ~TSyncEndCommand();
  virtual bool execute(void);
}; // TSyncEndCommand



// bytes required for empty item (approx)
  /*
<MapItem><Target><LocURI></LocURI></Target><Source><LocURI></LocURI></Source></MapItem>
  */
#define ITEMOVERHEADSIZE 90

// minimum size of data for a first split
#define MIN_SPLIT_DATA 200

// SyncOp (Add/Delete/Replace/Copy/Move) command
class TSyncOpCommand: public TSmlCommand
{
  typedef TSmlCommand inherited;
public:
  // constructor for receiving SyncOP command
  TSyncOpCommand(
    TSyncSession *aSessionP,        // associated session (for callbacks)
    TLocalEngineDS *aDataStoreP,   // the local datastore this syncop belongs to
    uInt32 aMsgID,                    // the Message ID of the command
    TSyncOperation aSyncOp,         // the Sync operation (sop_xxx)
    TSmlCommandTypes aCmdType,      // the command type (scmd_xxx)
    SmlGenericCmdPtr_t aSyncOpElementP=NULL  // associated syncml protocol element
  );
  // constructor for sending command
  TSyncOpCommand(
    TSyncSession *aSessionP,      // associated session (for callbacks)
    TLocalEngineDS *aDataStoreP, // datastore from which command originates
    TSyncOperation aSyncOp,       // sync operation (=command type)
    SmlPcdataPtr_t aMetaP         // meta for entire command (passing owner)
  );
  virtual ~TSyncOpCommand();
  virtual bool isSyncOp() { return true; };
  virtual bool analyze(TPackageStates aPackageState);
  virtual bool execute(void);
  #ifndef USE_SML_EVALUATION
  // - get (approximated) message size required for sending it
  virtual uInt32 messageSize(void);
  #endif
  // - add an Item to an existing Sync op command
  void addItem(SmlItemPtr_t aItemP); // existing item data structure, ownership is passed to SyncOpCmd
  // returns true if command must be put to the waiting-for-status queue.
  // If false, command can be deleted
  bool issue(
    uInt32 aAsCmdID,     // command ID to be used
    uInt32 aInMsgID,     // message ID in which command is being issued
    bool aNoResp       // dummy here, because Status has always no response
  );
  // - try to split (e.g using SyncML 1.1 moredata mechanism) command by reducing
  //   original command by at least aReduceByBytes and generating a second
  //   command containing the rest of the data
  //   Returns NULL if split is not possible
  virtual TSmlCommand *splitCommand(sInt32 aReduceByBytes);
  virtual bool canSplit(void);
  // - possibly substitute data with previous session's buffered left-overs from a chunked transfer
  //   for resuming a chunked item transfer.
  virtual bool checkChunkContinuation(void);
  // - test if completely issued (must be called after issue() and continueIssue())
  //   or completely executed (must be called after execute() to see if command can be deleted)
  virtual bool finished(void); // normal command finish
  /* %%% not used any more with new splitCommand
  // - continue issuing command. This is called by session at start of new message
  //   when finished() returned false after issue()/continueIssue()
  //   (which causes caller of finished() to put command into fInteruptedCommands queue)
  //   returns true if command should be queued for status (again?)
  //   NOTE: continueIssue must make sure that it gets a new CmdID/MsgID in case
  //   the command itself was NOT yet issued (normally this does not happen, but...)
  virtual bool continueIssue(bool &aNewIssue);
  */
  // - handle status received for previously issued command
  //   returns true if done, false if command must be kept in the status queue
  virtual bool handleStatus(TStatusCommand *aStatusCmdP);
  // - mark any unstatused mapitems for resume. Called for pending commands
  //   when a Suspend alert is received or whenever a resumable state must be saved
  virtual void markPendingForResume(TLocalEngineDS *aForDatastoreP, bool aUnsent);
  // - mark item for resend in next sync session (if possible)
  virtual void markForResend(void);
  // - let item update the partial item state for suspend
  void updatePartialItemState(TLocalEngineDS *aForDatastoreP);
  // - get sync op
  TSyncOperation getSyncOp(void) { return fSyncOp; };
  // - get source (localID) of sent command
  cAppCharP getSourceLocalID(void);
  // - get target (remoteID) of sent command
  cAppCharP getTargetRemoteID(void);
protected:
  localstatus AddNextChunk(SmlItemPtr_t aNextChunkItem, TSyncOpCommand *aCmdP);
  void saveAsPartialItem(SmlItemPtr_t aItemP);
  virtual void FreeSmlElement(void);
  SmlGenericCmdPtr_t fSyncOpElementP;
  #ifndef USE_SML_EVALUATION
  uInt32 fItemSizes; // accumulated item size
  #endif
  TSyncOperation fSyncOp; // Sync operation (sop_xxx)
  TLocalEngineDS *fDataStoreP;
  // SyncML 1.1 data segmentation
  uInt32 fChunkedItemSize; // size of object currently being chunked
  bool fIncompleteData; // set for commands that do not tranfer the final chunk (and must be answered with 213 status)
  // SyncML 1.2 resuming a chunked item transmission
  uInt32 fStoredSize; // from previous session
  uInt32 fUnconfirmedSize; // from previous session
  uInt32 fLastChunkSize; // size of last chunk received in THIS session (=probably unconfirmed reception, will probably be sent again from remote)
}; // TSyncOpCommand



// Map command
class TMapCommand: public TSmlCommand
{
  typedef TSmlCommand inherited;
public:
  // Common for client and server
  virtual ~TMapCommand();
  #ifndef USE_SML_EVALUATION
  // - get (approximated) message size required for sending it
  virtual uInt32 messageSize(void);
  #endif
  // - constructor for receiving MAP command
  TMapCommand(
    TSyncSession *aSessionP,        // associated session (for callbacks)
    uInt32 aMsgID,                    // the Message ID of the command
    SmlMapPtr_t aMapElementP        // associated syncml protocol element
  );
  #ifdef SYSYNC_SERVER
  // Server implementation
  virtual bool analyze(TPackageStates aPackageState);
  virtual bool execute(void);
  #endif // SYSYNC_SERVER
  #ifdef SYSYNC_CLIENT
  // Client implementation
  // - constructor for sending MAP
  TMapCommand(
    TSyncSession *aSessionP,              // associated session (for callbacks)
    TLocalEngineDS *aLocalDataStoreP,    // local datastore
    TRemoteDataStore *aRemoteDataStoreP   // remote datastore
  );
  virtual bool issue(
    uInt32 aAsCmdID,     // command ID to be used (is dummy=0 for SyncHdr)
    uInt32 aInMsgID,     // message ID in which command is being issued
    bool aNoResp=false // issue without wanting response
  );
  // - test if completely issued (must be called after issue() and continueIssue())
  //   or completely executed (must be called after execute() to see if command can be deleted)
  virtual bool finished(void); // normal command finish
  // - continue issuing command.
  virtual bool continueIssue(bool &aNewIssue);
  // - add map item
  void addMapItem(const char *aLocalID, const char *aRemoteID);
  // - remove last added map item from the command
  void deleteLastMapItem(void);
  // - handle status received for previously issued command
  //   returns true if done, false if command must be kept in the status queue
  virtual bool handleStatus(TStatusCommand *aStatusCmdP);
  #endif // SYSYNC_CLIENT
protected:
  virtual void FreeSmlElement(void);
  SmlMapPtr_t fMapElementP;
private:
  // internals
  #ifdef SYSYNC_CLIENT
  void generateEmptyMapElement(void);
  void generateMapItems(void);
  #endif
  bool fInProgress; // set if <Map> command must be continued in next message (=not finished)
  // involved datastores and mode
  TLocalEngineDS *fLocalDataStoreP;
  TRemoteDataStore *fRemoteDataStoreP;
  #ifndef USE_SML_EVALUATION
  // size
  uInt32 fItemSizes; // accumulated item size
  #endif
}; // TMapCommand



// Get command
class TGetCommand: public TSmlCommand
{
  typedef TSmlCommand inherited;
public:
  // constructor for receiving GET command
  TGetCommand(
    TSyncSession *aSessionP,        // associated session (for callbacks)
    uInt32 aMsgID,                    // the Message ID of the command
    SmlGetPtr_t aGetElementP=NULL   // associated GET command content element
  );
  // constructor for sending command
  TGetCommand(
    TSyncSession *aSessionP         // associated session (for callbacks)
  );
  // add an Item to the get command
  void addItem(
    SmlItemPtr_t aItemP // existing item data structure, ownership is passed to Get
  );
  // set Meta of the get command
  void setMeta(
    SmlPcdataPtr_t aMetaP // existing meta data structure, ownership is passed to Get
  );
  // get meta of the get command
  virtual SmlPcdataPtr_t getMeta(void) { return fGetElementP==NULL ? NULL : fGetElementP->meta; };
  // add a target specification Item to the get command
  void addTargetLocItem(
    const char *aTargetURI,
    const char *aTargetName=NULL
  );
  virtual ~TGetCommand();
  virtual bool analyze(TPackageStates aPackageState);
  virtual bool execute(void);
  virtual bool issue(
    uInt32 aAsCmdID,     // command ID to be used
    uInt32 aInMsgID,     // message ID in which command is being issued
    bool aNoResp=true  // status does not want response by default
  ); // returns true if command must be put to the waiting-for-status queue. If false, command can be deleted
  virtual bool statusEssential(void); // can be essential or not
  // - handle status received for previously issued command
  //   returns true if done, false if command must be kept in the status queue
  virtual bool handleStatus(TStatusCommand *aStatusCmdP);
protected:
  virtual void FreeSmlElement(void);
  SmlGetPtr_t fGetElementP;
}; // TGetCommand


// Put command
class TPutCommand: public TSmlCommand
{
  typedef TSmlCommand inherited;
public:
  // constructor for receiving PUT command
  TPutCommand(
    TSyncSession *aSessionP,        // associated session (for callbacks)
    uInt32 aMsgID,                    // the Message ID of the command
    SmlGetPtr_t aPutElementP=NULL   // associated PUT command content element
  );
  // constructor for sending command
  TPutCommand(
    TSyncSession *aSessionP         // associated session (for callbacks)
  );
  // add an Item to the put command
  void addItem(
    SmlItemPtr_t aItemP // existing item data structure, ownership is passed to Get
  );
  // set Meta of the put command
  void setMeta(
    SmlPcdataPtr_t aMetaP // existing meta data structure, ownership is passed to Get
  );
  // get meta of the put command
  virtual SmlPcdataPtr_t getMeta(void) { return fPutElementP==NULL ? NULL : fPutElementP->meta; };
  // add a source specification + data Item to the Put command
  SmlItemPtr_t addSourceLocItem(
    const char *aTargetURI,
    const char *aTargetName=NULL
  );
  virtual ~TPutCommand();
  virtual bool analyze(TPackageStates aPackageState);
  virtual bool execute(void);
  virtual bool issue(
    uInt32 aAsCmdID,     // command ID to be used
    uInt32 aInMsgID,     // message ID in which command is being issued
    bool aNoResp=true  // status does not want response by default
  ); // returns true if command must be put to the waiting-for-status queue. If false, command can be deleted
protected:
  virtual void FreeSmlElement(void);
  SmlPutPtr_t fPutElementP;
}; // TPutCommand


// Status command
class TStatusCommand: public TSmlCommand
{
  typedef TSmlCommand inherited;
  friend class TSmlCommand; // command may look into status
public:
  // constructor for generating status
  TStatusCommand(
    TSyncSession *aSessionP,        // associated session (for callbacks)
    TSmlCommand *aCommandP,         // command this status refers to, NULL=response to message header
    TSyError aStatusCode                 // status code
  );
  // constructor for generating status w/o having a command object (e.g. dummy)
  TStatusCommand(
    TSyncSession *aSessionP,        // associated session (for callbacks)
    uInt32 aRefCmdID=0,                // referred-to command ID
    TSmlCommandTypes aRefCmdType=scmd_unknown,   // referred-to command type (scmd_xxx)
    bool aNoResp=true,              // set if no-Resp
    TSyError aStatusCode=0               // status code
  );
  // constructor for receiving status
  TStatusCommand(
    TSyncSession *aSessionP,        // associated session (for callbacks)
    uInt32 aMsgID,                    // the Message ID of the command
    SmlStatusPtr_t aStatusElementP=NULL   // associated STATUS command content element
  );
  virtual ~TStatusCommand();
  virtual bool analyze(TPackageStates aPackageState); // analyze status
  virtual bool execute(void) { return true; }; // cannot be executed
  #ifndef USE_SML_EVALUATION
  // - get (approximated) message size required for sending it
  virtual uInt32 messageSize(void) { return fDontSend ? 0 : TSmlCommand::messageSize(); };
  #endif
  virtual bool issue(
    uInt32 aAsCmdID,     // command ID to be used
    uInt32 aInMsgID,     // message ID in which command is being issued
    bool aNoResp=true  // status does not want response by default
  ); // returns true if command must be put to the waiting-for-status queue. If false, command can be deleted
  // status building tools
  // - add a target Ref
  void addTargetRef(const char *aTargetRef); // Target LocURI of an item this status applies to
  // - add a source Ref to an existing status
  void addSourceRef(const char *aSourceRef); // Source LocURI of an item this status applies to
  // - add a String Item to an existing status
  void addItemString(const char *aItemString); // item string to be added
  // - add an Item to an existing status
  void addItem(SmlItemPtr_t aItemP); // existing item data structure, ownership is passed to Status
  // - move items from another status to this one (passes ownership of items and deletes them from original)
  void moveItemsFrom(TStatusCommand *aStatusCommandP);
  // - set Status code
  void setStatusCode(TSyError aStatusCode);
  // - set Challenge of an existing status (overwrites existing one, if any)
  void setChallenge(SmlChalPtr_t aChallengeP); // existing challenge data structure, ownership is passed to Status
  // properties
  // - get status code
  TSyError getStatusCode(void) { return fStatusCode; };
  // - get status Sml Element
  const SmlStatusPtr_t getStatusElement(void) { return fStatusElementP; }
protected:
  virtual void FreeSmlElement(void);
  SmlStatusPtr_t fStatusElementP;
  // info
  TSyError fStatusCode;  // status code
  uInt32 fRefMsgID;   // referenced message ID
  uInt32 fRefCmdID;   // referenced command ID
  TSmlCommandTypes fRefCmdType;  // referenced command type
}; // TStatusCommand


// Results command
class TResultsCommand: public TSmlCommand
{
  typedef TSmlCommand inherited;
public:
  // constructor for generating results
  TResultsCommand(
    TSyncSession *aSessionP,        // associated session (for callbacks)
    TSmlCommand *aCommandP,         // command this status refers to
    const char *aTargetRef,         // target reference
    const char *aSourceRef          // source reference
  );
  // constructor for receiving command
  TResultsCommand(
    TSyncSession *aSessionP,  // associated session (for callbacks)
    uInt32 aMsgID,              // the Message ID of the command
    SmlResultsPtr_t aResultsElementP  // associated syncml protocol element
  );
  virtual ~TResultsCommand();
  virtual bool neverIgnore() { return true; }; // result for devInf we requested should never be ignored
  virtual bool analyze(TPackageStates aPackageState);
  virtual bool execute(void);
  virtual bool issue(
    uInt32 aAsCmdID,     // command ID to be used
    uInt32 aInMsgID,     // message ID in which command is being issued
    bool aNoResp=false // issue without wanting response
  ); // returns true if command must be put to the waiting-for-status queue. If false, command can be deleted
  // results building tools
  // - add a String Item to results
  void addItemString(const char *aItemString); // item string to be added
  // - add an Item to results
  void addItem(SmlItemPtr_t aItemP); // existing item data structure, ownership is passed to Status
  // set Meta of the results command
  void setMeta(
    SmlPcdataPtr_t aMetaP // existing meta data structure, ownership is passed to Get
  );
  // get meta of the results command
  virtual SmlPcdataPtr_t getMeta(void) { return fResultsElementP==NULL ? NULL : fResultsElementP->meta; };
protected:
  virtual void FreeSmlElement(void);
  SmlResultsPtr_t fResultsElementP;
}; // TResultsCommand


// DevInf Results command (specialized TResultsCommand derivate for sending devInf)
class TDevInfResultsCommand: public TResultsCommand
{
  typedef TResultsCommand inherited;
public:
  // constructor for generating devinf results
  TDevInfResultsCommand(
    TSyncSession *aSessionP,        // associated session (for callbacks)
    TSmlCommand *aCommandP          // command this status refers to
  );
  virtual ~TDevInfResultsCommand();
  virtual bool issue(
    uInt32 aAsCmdID,     // command ID to be used
    uInt32 aInMsgID,     // message ID in which command is being issued
    bool aNoResp
  );
  virtual bool statusEssential(void) { return false; }; // results status is not essential
  // - try to shrink command by at least aReduceByBytes
  //   Returns false if shrink is not possible
  virtual bool shrinkCommand(sInt32 aReduceByBytes);
protected:
  virtual void FreeSmlElement(void);
}; // TDevInfResultsCommand


// DevInf Put command (specialized TPutCommand derivate for sending devInf)
class TDevInfPutCommand: public TPutCommand
{
  typedef TPutCommand inherited;
public:
  // constructor for generating devinf Put
  TDevInfPutCommand(
    TSyncSession *aSessionP        // associated session (for callbacks)
  );
  virtual ~TDevInfPutCommand();
  virtual bool issue(
    uInt32 aAsCmdID,     // command ID to be used
    uInt32 aInMsgID,     // message ID in which command is being issued
    bool aNoResp
  );
  virtual bool statusEssential(void) { return false; }; // devInf Put status is not essential
protected:
  virtual void FreeSmlElement(void);
}; // TDevInfPutCommand


} // namespace sysync

#endif  // SyncCommand_H

// eof
