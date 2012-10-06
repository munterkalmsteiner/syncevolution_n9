/*
 *  TSyncSession
 *    Represents an entire Synchronisation Session, possibly consisting
 *    of multiple SyncML-Toolkit "Sessions" (Message composition/de-
 *    composition) as well as multiple database synchronisations.
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef SYNC_SESSION_H
#define SYNC_SESSION_H

// general includes (SyncML tookit, windows, Clib)
#include "sysync.h"

// specific includes
#include "syncappbase.h"
#include "localengineds.h"
#include "remotedatastore.h"
#include "profiling.h"
#include "scriptcontext.h"


namespace sysync {

extern const char * const SyncMLVerProtoNames[numSyncMLVersions];
extern const SmlVersion_t SmlVersionCodes[numSyncMLVersions];
extern const char * const SyncMLVerDTDNames[numSyncMLVersions];
extern const char * const SyncMLDevInfNames[numSyncMLVersions];
#ifndef HARDCODED_CONFIG
extern const char * const SyncMLVersionNames[numSyncMLVersions];
#endif

extern const char * const authTypeNames[numAuthTypes];

extern const char * const SyncModeNames[numSyncModes];

#ifdef SYDEBUG
extern const char * const PackageStateNames[numPackageStates];
extern const char * const SyncOpNames[numSyncOperations];
#endif

extern const char * const SyncModeDescriptions[numSyncModes];


// secret type for SessionLogin
typedef enum {
  // Note: changes here will change AUTHTYPE() script func API in ODBC-Agent!
  sectyp_anonymous, // anonymous
  sectyp_clearpass, // clear text password
  sectyp_md5_V10, // SyncML V1.0 MD5
  sectyp_md5_V11 // SyncML V1.1 MD5
} TAuthSecretTypes;


// minimal free message size required to end message
// %%% rough approx, should always be enough
#define SIZEFORMESSAGEEND 200


// Container types
typedef std::list<TRemoteDataStore*> TRemoteDataStorePContainer; // contains data stores



// forward declaration
class TSyncAppBase;
class TSmlCommand;
class TStatusCommand;
class TSyncHeader;
class TLocalEngineDS;
class TRemoteDataStore;
class TRootConfig;


#ifdef SCRIPT_SUPPORT

// publish as derivates might need it
extern const TFuncTable ErrorFuncTable;

typedef struct {
  TSyError statuscode;
  TSyError newstatuscode;
  bool resend;
  TLocalEngineDS *datastoreP;
  TSyncOperation syncop;
} TErrorFuncContext;

typedef struct {
  bool isPut;
  bool canIssue;
  TSyError statuscode;
  string itemURI;
  string itemData;
  string metaType;
} TGetPutResultFuncContext;

#endif // SCRIPT_SUPPORT


#ifndef NO_REMOTE_RULES

class TRemoteRuleConfig; // forward

typedef std::list<TRemoteRuleConfig *> TRemoteRulesList;

// remote party special rule
class TRemoteRuleConfig: public TConfigElement
{
  typedef TConfigElement inherited;
public:
  TRemoteRuleConfig(const char *aElementName, TConfigElement *aParentElementP);
  virtual ~TRemoteRuleConfig();
  // properties
  // - identification of remote
  string fManufacturer;
  string fModel;
  string fOem;
  string fFirmwareVers;
  string fSoftwareVers;
  string fHardwareVers;
  string fDevId;
  string fDevTyp;
  // - options specific for that remote party (0=false, 1=true, -1=unspecified)
  sInt8 fLegacyMode; // set if remote is known legacy, so don't use new types
  sInt8 fLenientMode; // set if remote's SyncML should be handled leniently, i.e. not too strict checking where not absolutely needed
  sInt8 fLimitedFieldLengths; // set if remote has limited field lengths
  sInt8 fDontSendEmptyProperties; // set if remote does not want empty properties
  sInt8 fDoQuote8BitContent; // set if 8-bit chars should generally be encoded with QP in MIME-DIR
  sInt8 fDoNotFoldContent; // set if content should not be folded in MIME-DIR
  sInt8 fNoReplaceInSlowsync; // do not use Replace (as server) in slow sync (this is to COMPLETELY avoid replaces being sent during slowsync, for clients that crash on that such as old 9210)
  sInt8 fTreatRemoteTimeAsLocal; // treat remote time as localtime even if it carries different time zone information ("Z" suffix or zone spec)
  sInt8 fTreatRemoteTimeAsUTC; // treat remote time as UTC even if it carries different time zone information (no suffix or zone spec)
  sInt8 fVCal10EnddatesSameDay; // send end date-only values (like DTEND) as last time unit of previous day (i.e. 23:59:59, inclusive) instead of midnight of next day (exclusive, like in iCalendar 2.0)
  sInt8 fIgnoreDevInfMaxSize; // ignore <maxsize> specification in CTCap (when device has bad specs like in E90 for example)
  sInt8 fIgnoreCTCap; // ignore entire ctcap
  sInt8 fDSPathInDevInf; // use actual DS path as used in Alert for creating datastore devInf (needed for newer Nokia clients)
  sInt8 fDSCgiInDevInf; // also show CGI as used in Alert for creating datastore devInf (needed for newer Nokia clients)
  sInt8 fUpdateClientDuringSlowsync; // do not update client records (due to merge) in slowsync (However, updates can still occur in first-time sync and if server wins conflict)
  sInt8 fUpdateServerDuringSlowsync; // do not update server records during NON-FIRST-TIME slowsync (but do it for first sync!)
  sInt8 fAllowMessageRetries; // allow that client sends same message ID again (retry attempt)
  sInt8 fStrictExecOrdering; // requires strict SyncML-standard ordering of status responses
  sInt8 fTreatCopyAsAdd; // treat COPY like ADD (needed for Calmeno/Weblicon clients)
  sInt8 fCompleteFromClientOnly; // perform complete from-client-only session (non conformant, Synthesis before 2.9.8.2 style)
  sInt32 fRequestMaxTime; // max time [seconds] allowed for processing a single request, 0=unlimited, -1=not specified
  TCharSets fDefaultOutCharset; // default charset for generation
  TCharSets fDefaultInCharset; // default charset for input interpretation
  TSyError fRejectStatusCode; // if >=0, attempt to connect will always be rejected with given status code
  sInt8 fForceUTC; // force sending time in UTC (overrides SyncML 1.1 <utc/> devInf flag)
  sInt8 fForceLocaltime; // force sending time in localtime (overrides SyncML 1.1 <utc/> devInf flag)
  #ifndef MINIMAL_CODE
  string fRemoteDescName; // descriptive name of remote
  #endif
  #ifdef SCRIPT_SUPPORT
  string fRuleScriptTemplate; // template for rule script
  #endif
  // DevInf in XML format which is to be used instead of the one sent by peer.
  // If set, it is evaluated after identifying the peer based on the DevInf
  // that it has sent and before applying other remote rule workarounds.
  // XML DevInf directly from XML config.
  string fOverrideDevInfXML;
  SmlDevInfDevInfPtr_t fOverrideDevInfP;
  VoidPtr_t fOverrideDevInfBufferP;
  // list of subrules to activate
  TRemoteRulesList fSubRulesList;
  // flag if this is a final rule (if matches, no more rules will be checked)
  bool fFinalRule;
  // flag if this is a subrule (cannot match by itself)
  bool fSubRule;
protected:
  // check config elements
  #ifndef HARDCODED_CONFIG
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  virtual void clear();
}; // TRemoteRuleConfig



#endif // NO_REMOTE_RULES

// session config
class TSessionConfig: public TConfigElement
{
  typedef TConfigElement inherited;
public:
  TSessionConfig(const char *aElementName, TConfigElement *aParentElementP);
  virtual ~TSessionConfig();
  // Properties
  // - session timeout (in seconds)
  sInt32 fSessionTimeout;
  // - Maximally supported SyncML version
  TSyncMLVersions fMaxSyncMLVersionSupported;
  // - Minimally supported SyncML version
  TSyncMLVersions fMinSyncMLVersionSupported;
  // - support of server alerted sync codes
  bool fAcceptServerAlerted;
  #ifndef NO_REMOTE_RULES
  // - list of remote rules
  TRemoteRulesList fRemoteRulesList;
  #endif
  // - simple auth
  string fSimpleAuthUser;
  string fSimpleAuthPassword;
  // - local datastores
  TLocalDSList fDatastores;
  #ifndef MINIMAL_CODE
  // - logfile
  string fLogFileName;
  string fLogFileFormat;
  string fLogFileLabels;
  bool fLogEnabled;
  uInt32 fDebugChunkMaxSize;
  #endif
  bool fRelyOnEarlyMaps; // if set, we rely on early maps sent by clients for adds from the previous session
  // defaults for remote-rule configurable behaviour
  bool fUpdateClientDuringSlowsync; // do not update client records (due to merge) in slowsync (However, updates can still occur in first-time sync and if server wins conflict)
  bool fUpdateServerDuringSlowsync; // do not update server records during NON-FIRST-TIME slowsync (but do it for first sync!)
  bool fAllowMessageRetries; // allow that client sends same message ID again (retry attempt)
  uInt32 fRequestMaxTime; // max time [seconds] allowed for processing a single request, 0=unlimited
  sInt32 fRequestMinTime; // min time [seconds] spent until returning answer (for debug purposes, 0=no minimum time)
  bool fCompleteFromClientOnly; // perform complete from-client-only session (non conformant, Synthesis before 2.9.8.2 style)
  // default value for flag to send property lists in CTCap
  bool fShowCTCapProps;
  // default value for flag to send type/size in CTCap for SyncML 1.0 (disable as old clients like S55 crash on this)
  bool fShowTypeSzInCTCap10;
  // default value for sending end date-only values (like DTEND) as last time unit of previous day (i.e. 23:59:59, inclusive)
  // instead of midnight of next day (exclusive, like in iCalendar 2.0)
  bool fVCal10EnddatesSameDay;
  // instead of folding long lines (as required by the standard) use one line per property
  bool fDoNotFoldContent;
  // - set if we should show default parameter in mimo_old types as list of <propparam>s for each value
  //   Note that this is a tristate: 0=no, 1=yes, -1=auto (=yes for <SyncML 1.2, no for >=SyncML 1.2,
  //   thus making it work for Nokia 7610 (1.1) as well as E-Series like E90)
  sInt8 fEnumDefaultPropParams;
  // decides whether multi-threading for the datastores will be used
  bool fMultiThread;
  // defines if the engine waits with continuing interrupted commands until previous part received status
  bool fWaitForStatusOfInterrupted;
  // accept delete commands for already deleted items with 200 (rather that 404 or 211)
  bool fDeletingGoneOK;
  // abort if all items sent to remote fail
  bool fAbortOnAllItemsFailed;
  // - Session user time context (what time zone the current session's user is in, for clients w/o TZ/UTC support)
  timecontext_t fUserTimeContext;
  #ifdef SCRIPT_SUPPORT
  // session init script
  string fSessionInitScript;
  // Error status handling scripts
  string fSentItemStatusScript;
  string fReceivedItemStatusScript;
  // session init script
  string fSessionFinishScript;
  // custom GET command handler script
  string fCustomGetHandlerScript;
  // custom GET and PUT command generator scripts
  string fCustomGetPutScript;
  string fCustomEndPutScript;
  // custom PUT and RESULT handler script
  string fCustomPutResultHandlerScript;
  #endif
  // public methods
  TLocalDSConfig *getLocalDS(const char *aName, uInt32 aDBTypeID=0);
  lineartime_t getSessionTimeout(void) { return fSessionTimeout * secondToLinearTimeFactor; };
  // - MUST be called after creating config to load (or pre-load) variable parts of config
  //   such as binfile profiles. If aDoLoose==false, situations, where existing config
  //   is detected but cannot be re-used will return an error. With aDoLoose==true, config
  //   files etc. are created even if it means a loss of data.
  virtual localstatus loadVarConfig(bool aDoLoose=false) { return LOCERR_OK; }
protected:
  // check config elements
  #ifndef HARDCODED_CONFIG
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual TLocalDSConfig *newDatastoreConfig(const char *aName, const char *aType,TConfigElement *aParentP);
  #endif
  virtual void clear();
  virtual void localResolve(bool aLastPass);
}; // TSessionConfig


// forward
class TLocalEngineDS;

// Container types
typedef std::list<TLocalEngineDS*> TLocalDataStorePContainer; // contains local data stores


// Sync session
class TSyncSession {
  friend class TSmlCommand;
  friend class TSyncHeader;
  friend class TAlertCommand;
  friend class TSyncCommand;
  friend class TStatusCommand;
  friend class TSyncOpCommand;
  friend class TRemoteDataStore;
  friend class TLocalEngineDS;
  #ifdef SUPERDATASTORES
  friend class TSuperDataStore;
  #endif
public:
  // constructors/destructors
  TSyncSession(
    TSyncAppBase *aSyncAppBaseP, // the owning application base (dispatcher/client base)
    const char *aSessionID // a session ID
  );
  virtual ~TSyncSession();
  /// @brief terminate a session.
  /// @Note: Termination is final - session cannot be restarted by RestartSession() after
  ///        calling this routine
  virtual void TerminateSession(void);
  // Announce destruction of descendant to all datastores which might have direct links to these descendants and must cancel those
  void announceDestruction(void); ///< must be called by derived class' destructors to allow datastores to detach from agent BEFORE descendant destructor has run
  // Reset session
  virtual void ResetSession(void); ///< resets session as if created totally new. Descendants must rollback any pending database transactions etc.
  void InternalResetSessionEx(bool terminationCall); // static implementation for calling through virtual destructor and virtual ResetSession();
  #ifdef DBAPI_TUNNEL_SUPPORT
  // Initialize a datastore tunnel session
  virtual localstatus InitializeTunnelSession(cAppCharP aDatastoreName) { return LOCERR_NOTIMP; }; // is usually implemented in customimplagent, as it depends on DBApi architecture
  virtual TLocalEngineDS *getTunnelDS() { return NULL; }; // is usually implemented in customimplagent
  #endif // DBAPI_TUNNEL_SUPPORT
  #ifdef PROGRESS_EVENTS
  // Create Session level progress event
  bool NotifySessionProgressEvent(
    TProgressEventType aEventType,
    TLocalDSConfig *aDatastoreID,
    sInt32 aExtra1,
    sInt32 aExtra2,
    sInt32 aExtra3
  );
  // Handle (or dispatch) Session level progress event
  virtual bool HandleSessionProgressEvent(TEngineProgressInfo aProgressInfo) { return true; }; // no handling by default
  #endif // PROGRESS_EVENTS
  // called when incoming SyncHdr fails to execute
  virtual bool syncHdrFailure(bool aTryAgain) = 0;
  // Abort session
  void AbortSession(TSyError aStatusCode, bool aLocalProblem, TSyError aReason=0); // resets session and sets aborted flag to prevent further processing of message
  void MarkSuspendAlertSent(bool aSent);
  // Suspend session
  void SuspendSession(TSyError aReason);
  // Session status
  bool isAborted(void) { return fAborted; }; // test abort status
  bool isSuspending(void) { return fSuspended; }; // test if flagged for suspend
  bool isSuspendAlertSent (void) { return fSuspendAlertSent; }; // test if suspend alert was already sent
  bool isAllSuccess(void); // test if session was completely successful
  void DatastoreFailed(TSyError aStatusCode, bool aLocalProblem=false); // let session know that datastore has failed
  void DatastoreHadErrors(void) { fErrorItemDatastores++; }; // let session know that sync was ok, but some items had errors
  bool outgoingMessageFull(void) { return fOutgoingMessageFull; }; // test if outgoing full
  bool isInterrupedCmdPending(void) { return fInterruptedCommandP!=NULL; };
  bool getIncomingState(void) { return fIncomingState; };
  // stop processing commands in this message
  void AbortCommandProcessing(TSyError aStatusCode); // all further commands in message will be answered with given status
  // returns remaining time for request processing [seconds]
  virtual sInt32 RemainingRequestTime(void) { return 0x7FFFFFFF; }; // quasi infinite
  // forget commands waiting to be sent when header is generated
  void forgetHeaderWaitCommands(void);
  // SyncML toolkit workspace access
  void setSmlWorkspaceID(InstanceID_t aSmlWorkspaceID);
  InstanceID_t getSmlWorkspaceID(void) { return fSmlWorkspaceID; };
  const char *getEncodingName(void); // encoding suffix in MIME type
  SmlEncoding_t getEncoding(void) { return fEncoding; }; // current encoding
  void setEncoding(SmlEncoding_t aEncoding); // set encoding for session
  void addEncoding(string &aString); // add current encoding spec to given (type-)string
  sInt32 getSmlWorkspaceFreeBytes(void) { return ((sInt32) smlGetFreeBuffer(fSmlWorkspaceID)); };
  #ifdef ENGINEINTERFACE_SUPPORT
  /// @brief Get new session key to access details of this session
  virtual appPointer newSessionKey(TEngineInterface *aEngineInterfaceP) = 0;
  #endif // ENGINEINTERFACE_SUPPORT
  // session handling
  // - get session owner (dispatcher/clientbase)
  TSyncAppBase *getSyncAppBase(void) { return fSyncAppBaseP; }
  // - get time when session was last used
  lineartime_t getSessionLastUsed(void) { return fSessionLastUsed; };
  // - get time when session was started and ended
  lineartime_t getSessionStarted(void) { return fSessionStarted; };
  // - get time when last request started processing
  lineartime_t getLastRequestStarted(void) { return fLastRequestStarted; };
  // - update last used time
  void SessionUsed(void) { fSessionLastUsed=getSystemNowAs(TCTX_UTC); };
  // - session custom time zones object access
  GZones *getSessionZones(void) { return &fSessionZones; };
  // - convenience version for getting time
  lineartime_t getSystemNowAs(timecontext_t aContext) { return sysync::getSystemNowAs(aContext,getSessionZones()); };
  // debug and log printing (should NOT be virtual, so that they can be used in destructors)
  void DebugShowCfgInfo(void); // show some information about the config
  //%%%void LogPrintf(const char *text, ...);
  //%%%void LogPuts(const char *text);
  // properties
  TSyncMLVersions getSyncMLVersion(void) { return fSyncMLVersion; };
  const char *getLocalURI(void) { return fLocalURI.c_str(); };
  const char *getInitialLocalURI(void) { return fInitialLocalURI.c_str(); };
  const char *getRemoteURI(void) { return fRemoteURI.c_str(); };
  const char *getSynchdrSessionID(void) { return fSynchdrSessionID.c_str(); };
  const char *getLocalSessionID(void) { return fLocalSessionID.c_str(); };
  localstatus getAbortReasonStatus(void) { return fLocalAbortReason ? localError(fAbortReasonStatus) : syncmlError(fAbortReasonStatus); };
  #ifndef MINIMAL_CODE
  const char *getRemoteInfoString(void) { return fRemoteInfoString.c_str(); };
  const char *getRemoteDescName(void) { return fRemoteDescName.c_str(); };
  const char *getSyncUserName(void) { return fSyncUserName.c_str(); };
  #endif // MINIMAL_CODE
  sInt32 getLastIncomingMsgID(void) { return fIncomingMsgID; };
  void setSessionBusy(bool aBusy) { fSessionIsBusy=aBusy; }; // make session behave busy generally
  bool getReadOnly(void) { return fReadOnly; }; // read-only option
  void setReadOnly(bool aReadOnly) { fReadOnly=aReadOnly; }; // read-only option
  // - check if we can handle UTC time (devices without time zone might override this)
  virtual bool canHandleUTC(void) { return true; }; // assume yes
  // helpers
  // - get session relative URI
  const char *SessionRelativeURI(const char *aURI)
    { return relativeURI(aURI,getLocalURI()); };
  // - add local datastore from config
  TLocalEngineDS *addLocalDataStore(TLocalDSConfig *aLocalDSConfigP);
  // - add local type
  void addLocalItemType(TSyncItemType *aItemTypeP)
    { fLocalItemTypes.push_back(aItemTypeP); };
  // - find local datatype by config pointer (used to avoid duplicating types
  //   in session if used by more than a single datastore)
  TSyncItemType *findLocalType(TDataTypeConfig *aDataTypeConfigP);
  // - find implemented remote datatype by config pointer (and related datastore, if any)
  TSyncItemType *findRemoteType(TDataTypeConfig *aDataTypeConfigP, TSyncDataStore *aRelatedRemoteDS);
  // internal processing events implemented in derived classes
  // - message start and end
  virtual bool MessageStarted(SmlSyncHdrPtr_t aContentP, TStatusCommand &aStatusCommand, bool aBad=false)=0;
  virtual void MessageEnded(bool aIncomingFinal)=0;
  // - get command item processing, may return a Results command. Must set status to non-404 if get could be served
  virtual TResultsCommand *processGetItem(const char *aLocUri, TGetCommand *aGetCommandP, SmlItemPtr_t aGetItemP, TStatusCommand &aStatusCommand);
  // - put and results command processing
  virtual void processPutResultItem(bool aIsPut, const char *aLocUri, TSmlCommand *aPutResultsCommandP, SmlItemPtr_t aPutResultsItemP, TStatusCommand &aStatusCommand);
  // - alert processing
  virtual TSmlCommand *processAlertItem(
    uInt16 aAlertCode,   // alert code
    SmlItemPtr_t aItemP, // alert item to be processed (as one alert can have multiple items)
    SmlCredPtr_t aCredP, // alert cred element, if any
    TStatusCommand &aStatusCommand, // pre-set 200 status, can be modified in case of errors
    TLocalEngineDS *&aLocalDataStoreP // receives datastore pointer, if alert affects a datastore
  );
  // - handle status received for SyncHdr, returns false if not handled
  virtual bool handleHeaderStatus(TStatusCommand * /* aStatusCmdP */) { return false; } // no special handling by default
  // - map operation
  virtual bool processMapCommand(
    SmlMapPtr_t aMapCommandP,       // the map command contents
    TStatusCommand &aStatusCommand, // pre-set 200 status, can be modified in case of errors
    bool &aQueueForLater
  );
  // Sync processing (command group)
  // - start sync group
  virtual bool processSyncStart(
    SmlSyncPtr_t aSyncP,           // the Sync element
    TStatusCommand &aStatusCommand, // pre-set 200 status, can be modified in case of errors
    bool &aQueueForLater // will be set if command must be queued for later (re-)execution
  ) = 0;
  // - end sync group
  virtual bool processSyncEnd(bool &aQueueForLater);  // end of sync group
  // - process generic sync command item within Sync group
  //   - returns true (and unmodified or non-200-successful status) if
  //     operation could be processed regularily
  //   - returns false (but probably still successful status) if
  //     operation was processed with internal irregularities, such as
  //     trying to delete non-existant item in datastore with
  //     incomplete Rollbacks (which returns status 200 in this case!).
  bool processSyncOpItem(
    TSyncOperation aSyncOp,        // the operation
    SmlItemPtr_t aItemP,           // the item to be processed
    SmlMetInfMetInfPtr_t aMetaP,   // command-wide meta, if any
    TLocalEngineDS *aLocalSyncDatastore, // the local datastore for this syncop item
    TStatusCommand &aStatusCommand, // pre-set 200 status, can be modified in case of errors
    bool &aQueueForLater // must be set if item cannot be processed now, but must be processed later
  );
  // message handling
  // - get current size of message
  sInt32 getOutgoingMessageSize(void) { return fOutgoingMsgSize; }; // returns currently assembled message size
  // - get byte statistics (only implemented in server so far)
  virtual uInt32 getIncomingBytes(void) { return 0; };
  virtual uInt32 getOutgoingBytes(void) { return 0; };
  // - get how many bytes may not be used in the outgoing message buffer
  //   because of maxMsgSize restrictions
  sInt32 getNotUsableBufferBytes(void);
  // - get max size outgoing message may have (either defined by remote's maxmsgsize or local buffer space)
  sInt32 getMaxOutgoingSize(void);
  // - returns true if given number of bytes are transferable
  //   (not exceeding MaxMsgSize (in SyncML 1.0) or MaxObjSize (SyncML 1.1 and later)
  bool dataSizeTransferable(uInt32 aDataBytes);
  // - update outgoing message size
  void incOutgoingMessageSize(sInt32 aIncrement) { fOutgoingMsgSize+=aIncrement; };
  // - get message-global noResp status
  bool getMsgNoResp(void) { return fMsgNoResp; }
  // - get next outgoing command ID
  sInt32 getNextOutgoingCmdID(void) { return (++fOutgoingCmdID); }
  // - get next outgoing command ID without actually consuming it
  sInt32 peekNextOutgoingCmdID(void) { return (fOutgoingCmdID+1); }
  // - get current outgoing message ID
  sInt32 getOutgoingMsgID(void) { return fOutgoingMsgID; }
  // command handling
  // - issue a command (and put it to status queue if it expects a result)
  bool issue(TSmlCommand * &aSyncCommandP,
    TSmlCommandPContainer &aNextMessageCommands,
    TSmlCommand * &aInterruptedCommandP,
    bool aNoResp=false, bool aIsOKSyncHdrStatus=false
  );
  bool issuePtr(TSmlCommand *aSyncCommandP, TSmlCommandPContainer &aNextMessageCommands,
    TSmlCommand * &aInterruptedCommandP, bool aNoResp=false, bool aIsOKSyncHdrStatus=false);
  // - issue a command in SyncBody context (uses session's interruptedCommand/NextMessageCommands)
  bool issueRoot(TSmlCommand * &aSyncCommandP,
    bool aNoResp=false, bool aIsOKSyncHdrStatus=false
  );
  bool issueRootPtr(TSmlCommand *aSyncCommandP,
    bool aNoResp=false, bool aIsOKSyncHdrStatus=false
  );
  // queue a SyncBody context command for issuing after incoming message
  // has been processed (and answers generated)
  void queueForIssueRoot(
    TSmlCommand * &aSyncCommandP // the command
  );
  // issue a command, but queue it if outgoing package has not begun yet
  void issueNotBeforePackage(
    TPackageStates aPackageState,
    TSmlCommand *aSyncCommandP // the command
  );
  // - session continuation and status
  void nextMessageRequest(void);
  bool sessionMustContinue(void);
  virtual void essentialStatusReceived(void) { /* NOP here */ };
  void delayExecUntilNextRequest(TSmlCommand *aCommand);
  bool delayedSyncEndsPending(void) { return fDelayedExecSyncEnds>0; };
  // - continue interrupted or prevented issue in next package
  void ContinuePackageRoot(void);
  void ContinuePackage(
    TSmlCommandPContainer &aNextMessageCommands,
    TSmlCommand * &aInterruptedCommandP
  );
  // - mark all pending items for a datastore for resume
  //   (those items that are in a session queue for being issued or getting status)
  void markPendingForResume(TLocalEngineDS *aForDatastoreP);
  void markPendingForResume(
    TSmlCommandPContainer &aNextMessageCommands,
    TSmlCommand *aInterruptedCommandP,
    TLocalEngineDS *aForDatastoreP
  );
  // access to session info from commands
  bool mustSendDevInf(void) { return fRemoteMustSeeDevinf; };
  // - access DevInf (session owned)
  SmlItemPtr_t getLocalDevInfItem(bool aAlertedOnly, bool aWithoutCTCapProps);
  // - analyze devinf of remote party (can be derived to add client or server specific analysis)
  virtual localstatus analyzeRemoteDevInf(
    SmlDevInfDevInfPtr_t aDevInfP
  );
  // - get possibly cached devinf for specified device, passes ownership of
  //   created devinf structure to caller
  //   returns false if no devinf could be loaded
  virtual bool loadRemoteDevInf(const char * /* aDeviceID */, SmlDevInfDevInfPtr_t & /* aDevInfP */) { return false; };
  // - save devinf to cache for specified device
  //   return false if devinf cannot be cached
  virtual bool saveRemoteDevInf(const char * /* aDeviceID */, SmlDevInfDevInfPtr_t /* aDevInfP */) { return false; };
  // - finish outgoing Message, returns true if final message of package
  bool FinishMessage(bool aAllowFinal, bool aForceNonFinal=false);
  // - returns true if session has pending commands
  bool hasPendingCommands(void);
  // - incoming message processing aborted (EndMessage will not get called, clean up)
  void CancelMessageProcessing(void);
  // entry points for SyncML Toolkit callbacks
  // - message handling
  Ret_t StartMessage(SmlSyncHdrPtr_t aContentP);
  Ret_t EndMessage(Boolean_t final);
  // - grouping commands
  Ret_t StartSync(SmlSyncPtr_t aContentP);
  Ret_t EndSync(void);
  #ifdef ATOMIC_RECEIVE
  Ret_t StartAtomic(SmlAtomicPtr_t aContentP);
  Ret_t EndAtomic(void);
  #endif
  #ifdef SEQUENCE_RECEIVE
  Ret_t StartSequence(SmlSequencePtr_t aContentP);
  Ret_t EndSequence(void);
  #endif
  // - commands
  Ret_t AddCmd(SmlAddPtr_t aContentP);
  Ret_t AlertCmd(SmlAlertPtr_t aContentP);
  Ret_t DeleteCmd(SmlDeletePtr_t aContentP);
  Ret_t GetCmd(SmlGetPtr_t aContentP);
  Ret_t PutCmd(SmlPutPtr_t aContentP);
  #ifdef MAP_RECEIVE
  Ret_t MapCmd(SmlMapPtr_t aContentP);
  #endif
  #ifdef RESULT_RECEIVE
  Ret_t ResultsCmd(SmlResultsPtr_t aContentP);
  #endif
  Ret_t StatusCmd(SmlStatusPtr_t aContentP);
  Ret_t ReplaceCmd(SmlReplacePtr_t aContentP);
  #ifdef COPY_RECEIVE
  Ret_t CopyCmd(SmlReplacePtr_t aContentP);
  #endif
  Ret_t MoveCmd(SmlReplacePtr_t aContentP);
  // - error handling
  Ret_t HandleError(void);
  Ret_t DummyHandler(const char* msg);
  // - Other Callbacks: routed directly to appropriate session, error if none
  #ifdef SYNCSTATUS_AT_SYNC_CLOSE
  TStatusCommand *fSyncCloseStatusCommandP;
  #endif
  #ifdef SYDEBUG
  // Message dump
  void DumpSyncMLMessage(bool aOutgoing);
  void DumpSyncMLBuffer(MemPtr_t aBuffer, MemSize_t aBufSize, bool aOutgoing, Ret_t aDecoderError);
  uInt32 fDumpCount;
  // XML translations of communication
  // - recoding instances
  InstanceID_t fOutgoingXMLInstance,fIncomingXMLInstance;
  // - routines
  void XMLTranslationIncomingStart(void);
  void XMLTranslationOutgoingStart(void);
  void XMLTranslationIncomingEnd(void);
  void XMLTranslationOutgoingEnd(void);
  // - flags
  bool fXMLtranslate; // dump XML translation of SyncML traffic
  bool fMsgDump; // dump raw SyncML messages
  #endif // SYDEBUG
  // log writing
  #ifndef MINIMAL_CODE
  void WriteLogLine(const char *aLogline);
  bool logEnabled(void) { return fLogEnabled; };
  #endif // MINIMAL_CODE
  // current database date & time (defaults to system time)
  virtual lineartime_t getDatabaseNowAs(timecontext_t aContext) { return getSystemNowAs(aContext); };
  // devinf
  void remoteGotDevinf(void) { fRemoteGotDevinf=true; };
  void remoteMustSeeDevinf(void) { fRemoteMustSeeDevinf=true; };
  // config access
  TRootConfig *getRootConfig(void);
  // access to logging for session
  #ifdef SYDEBUG
  TDebugLogger *getDbgLogger(void) { return &fSessionLogger; };
  uInt32 getDbgMask(void) { return fSessionDebugLogs ? fSessionLogger.getMask() : 0; };
  #endif // SYDEBUG
  // Remote-specific options, will be set up by checkClient/ServerSpecifics()
  bool fLimitedRemoteFieldLengths; // if set, all fields will be assumed to have limited, but unknown field length (used for cut-off detection)
  bool fDontSendEmptyProperties; // if set, no empty properties will be sent to client
  bool fDoQuote8BitContent; // set if 8-bit chars should generally be encoded with QP in MIME-DIR
  bool fDoNotFoldContent; // set if content should not be folded in MIME-DIR
  bool fNoReplaceInSlowsync; // prevent replace commands totally at slow sync
  bool fTreatRemoteTimeAsLocal; // treat remote time as localtime even if it carries different time zone information ("Z" suffix or zone spec)
  bool fTreatRemoteTimeAsUTC; // treat remote time as UTC even if it carries different time zone information (no suffix or zone spec)
  bool fVCal10EnddatesSameDay; // send end date-only values (like DTEND) as last time unit of previous day (i.e. 23:59:59, inclusive) instead of midnight of next day (exclusive, like in iCalendar 2.0)
  bool fIgnoreDevInfMaxSize; // ignore <maxsize> specification in CTCap (when device has bad specs like in E90 for example)
  bool fIgnoreCTCap; // ignore entire ctcap
  bool fDSPathInDevInf; // use actual DS path as used in Alert for creating datastore devInf (needed for newer Nokia clients)
  bool fDSCgiInDevInf; // also show CGI as used in Alert for creating datastore devInf (needed for newer Nokia clients)
  bool fUpdateClientDuringSlowsync; // prevent updates of client records during non-first-time slow sync
  bool fUpdateServerDuringSlowsync; // do not update server records during NON-FIRST-TIME slowsync (but do it for first sync!)
  bool fAllowMessageRetries; // allow that client sends same message ID again (retry attempt)
  bool fStrictExecOrdering; // if set (=default, SyncML standard requirement), statuses are sent in order of incoming commands (=execution is ordered)
  bool fTreatCopyAsAdd; // treat copy commands as if they were adds
  bool fCompleteFromClientOnly; // perform complete from-client-only session (non conformant, Synthesis before 2.9.8.2 style)
  sInt32 fRequestMaxTime; // max time [seconds] allowed for processing a single request, 0=unlimited
  sInt32 fRequestMinTime; // min time [seconds] spent until returning answer (for debug purposes, 0=no minimum time)
  TCharSets fDefaultOutCharset; // default charset for output generation
  TCharSets fDefaultInCharset; // default charset for input interpretation
  #ifndef NO_REMOTE_RULES
  bool isActiveRule(cAppCharP aRuleName, TRemoteRuleConfig *aRuleP=NULL); // check if given rule (by name, or if aRuleName=NULL by rule pointer) is active
  TRemoteRulesList fActiveRemoteRules; // list of remote rules currently active in this session
  #endif // NO_REMOTE_RULES
  // legacy mode
  bool fLegacyMode; // if set, remote will see the types marked preferred="legacy" in devInf as preferred types, not the regular preferred ones
  // lenient mode
  bool fLenientMode; // if set, enine is less strict in checking (e.g. client-side anchor checking, terminating session while some status missing etc.)
  #ifdef EXPIRES_AFTER_DATE
  // copy of scrambled now
  sInt32 fCopyOfScrambledNow;
  #endif // EXPIRES_AFTER_DATE
  // Sync datastores
  // - find local datastore by URI and separate identifying from optional part of URI
  TLocalEngineDS *findLocalDataStoreByURI(const char *aURI,string *aOptions=NULL, string *aIdentifyingURI=NULL);
  // - find local datastore by relative path (may not contain any CGI)
  TLocalEngineDS *findLocalDataStore(const char *aDatastoreURI);
  // - find local datastore by datastore handle (=config pointer)
  TLocalEngineDS *findLocalDataStore(void *aDSHandle);
  // - find remote datastore by (remote party specified) URI
  TRemoteDataStore *findRemoteDataStore(const char *aDatastoreURI);
  // Profiling
  TP_DEFINFO(fTPInfo)
  // access to config
  TSessionConfig *getSessionConfig(void);
  #ifdef SCRIPT_SUPPORT
  // access to session script context
  TScriptContext *getSessionScriptContext(void) { return fSessionScriptContextP; };
  #endif // SCRIPT_SUPPORT
  // unprotected options
  // - set if we should send property lists in CTCap
  bool fShowCTCapProps;
  // - set if we should send type/size in CTCap for SyncML 1.0 (disabled by default as old clients like S55 crash on this)
  bool fShowTypeSzInCTCap10;
  // - set if we should show default parameter in mimo_old types as list of <propparam>s for each value
  //   Note that this is a tristate: 0=no, 1=yes, -1=auto (=yes for <SyncML 1.2, no for >=SyncML 1.2,
  //   thus making it work for Nokia 7610 (1.1) as well as E-Series like E90)
  sInt8 fEnumDefaultPropParams;
  #ifdef SYDEBUG
  /// @todo
  /// fSessionDebugLogs should be removed (but this needs rewriting of the XML and SML dumpers)
  // - set if debug log for this session is enabled
  bool fSessionDebugLogs;
  #endif // SYDEBUG
  #ifndef MINIMAL_CODE
  // - se if normal log for this session is enabled
  bool fLogEnabled; // real log file enabled
  #endif // MINIMAL_CODE
  // - remote options (SyncML 1.1)
  bool fRemoteWantsNOC; // remote wants number-of-changes info
  bool fRemoteCanHandleUTC; // remote can handle UTC time
  bool fRemoteSupportsLargeObjects; // remote can handle large object splitting/reassembly
  // - object size handling
  sInt16 fOutgoingCmds; // number of outgoing commands in message, but NOT counting SyncHdr status and Alert 222 status (which are ALWAYS there even in an otherwise empty message)
  sInt32 fMaxRoomForData; // max room for data (free bytes available for data when startin a <sync> command)
  // - Session user time context
  timecontext_t fUserTimeContext;
protected:
  // Session control
  // - terminate all datastores
  void TerminateDatastores(localstatus aAbortStatusCode=408);
  // - remove all datastores
  void ResetAndRemoveDatastores(void);
  // - session layer credential checking
  bool checkCredentials(const char *aUserName, const SmlCredPtr_t aCredP, TStatusCommand &aStatusCommand);
  bool checkCredentials(const char *aUserName, const char *aCred, TAuthTypes aAuthType);
  // - session layer challenge
  SmlChalPtr_t newSessionChallenge(void);
  // datastore and type vars
  // - list of local datastores
  TLocalDataStorePContainer fLocalDataStores;
  // - list of remote (client-side) datastores
  TRemoteDataStorePContainer fRemoteDataStores;
  bool receivedSyncModeExtensions(); // any of the remote datastores in fRemoteDataStores
                                     // had custom sync modes
  // - list of local content types
  TSyncItemTypePContainer fLocalItemTypes;
  // - list of remote item types
  TSyncItemTypePContainer fRemoteItemTypes;
  // - Local Database currently targeted by a Sync command, NULL if none
  //   Note: This must be set correctly whenever sync commands (and </sync> syncend) are processed
  //         This can be during actual receiving them, OR while processing them from the fDelayedExecutionCommands
  //         queue.
  TLocalEngineDS *fLocalSyncDatastoreP;
  // set if we have received DevInf for remote DataStores / CTCap
  bool fRemoteDevInfKnown; // remote devInf known
  bool fRemoteDataStoresKnown; // data stores known
  bool fRemoteDataTypesKnown; // CTCap known
  bool fRemoteDevInfLock; // set after starting sync according to devInf we had to prevent in-sync devInf changes
  // see if we have sent or should send DevInf to remote
  bool fRemoteGotDevinf; // set if we sent a Put or Result containig DevInf
  bool fRemoteMustSeeDevinf; // set if we should force (Put) devinf to remote
  bool fCustomGetPutSent; // set if custom get/put has been sent to remote
  // DevInf
  // - get new sml list of all datastores (owner of list is transferred, but items are still owned by datastore
  SmlDevInfDatastoreListPtr_t newDevInfDataStoreList(bool aAlertedOnly, bool aWithoutCTCapProps);
  SmlDevInfCtcapListPtr_t newLocalCTCapList(bool aAlertedOnly, TLocalEngineDS *aOnlyForDS, bool aWithoutCTCapProps);
  // - get new DevInf for this session (as Result for GET or item for PUT)
  virtual SmlDevInfDevInfPtr_t newDevInf(bool aAlertedOnly, bool aWithoutCTCapProps);
  // - called to issue custom get and put commands
  virtual void issueCustomGetPut(bool aGotDevInf, bool aSentDevInf);
  virtual void issueCustomEndPut(void);
  // Sync processing
  // - prepare for sending and receiving Sync commands
  localstatus initSync(
    const char *aLocalDatastoreURI,
    const char *aRemoteDatastoreURI
  );
  // Command processing
  // - process a command (analyze and execute it),
  //   exception-free for simple call from smlCallback adaptors
  Ret_t process(TSmlCommand *aSyncCommandP);
  // - handle incoming status
  //   exception-free for simple call from smlCallback adaptors
  Ret_t handleStatus(TStatusCommand *aStatusCommandP);
  // helpers for derived classes
  // - create, send and delete SyncHeader "command"
  void issueHeader(bool aNoResp=false);
  // - process the SyncHeader "command"
  Ret_t processHeader(TSyncHeader *aSyncHdrP);
  // Helpers for commands
  // - create new SyncHdr structure for TSyncHeader command
  //   (here because all data for this is in session anyway)
  SmlSyncHdrPtr_t NewOutgoingSyncHdr(bool aOutgoingNoResp=false);
  // virtuals for overriding in specialized session derivates
  // - device ID must be handled on session level as it might depend on session runtime conditions
  //   (like special pseudo-unique ID for Oracle servers when basic id is not unique etc.)
  virtual string getDeviceID(void)=0;
  virtual string getDeviceType(void)=0; // abstract, must be client or server
  // - get new response URI to be sent to remote party for subsequent messages TO local party
  virtual SmlPcdataPtr_t newResponseURIForRemote(void) { return NULL; }; // no RespURI by default
  // Authorisation
  // - required authentication type and mode
  virtual TAuthTypes requestedAuthType(void) = 0; // get preferred authentication type for authentication of remote party
  virtual bool isAuthTypeAllowed(TAuthTypes aAuthType) = 0; // test if auth type is allowed for authentication by remote party
  virtual bool messageAuthRequired(void) { return false; }; // no message-by-message auth by default
  // - get credentials/username to authenticate with remote party, NULL if none
  virtual SmlCredPtr_t newCredentialsForRemote(void) { return NULL; }; // normally (server case), none
  virtual const char * getUsernameForRemote(void) { return NULL; }; // normally (server case), none
  // - generate credentials (based on fRemoteNonce, fRemoteRequestedAuth, fRemoteRequestedAuthEnc)
  SmlCredPtr_t newCredentials(const char *aUser, const char *aPassword);
public:
  // - URI to send outgoing message to
  virtual const char *getSendURI(void) { return ""; }; // none by default (and server)
  // - get common sync capabilities mask of this session (datastores might modify it)
  virtual uInt32 getSyncCapMask(void);
  // - check credentials, login to server
  virtual bool SessionLogin(const char *aUserName, const char *aAuthString, TAuthSecretTypes aAuthStringType, const char *aDeviceID);
protected:
  // - get next nonce string top be sent to remote party for subsequent MD5 auth
  virtual void getNextNonce(const char * /* aDeviceID */, string &aNextNonce) { aNextNonce.erase(); }; // empty nonce
public:
  // - get nonce string for specified device
  virtual void getAuthNonce(const char * /* aDeviceID */, string &aAuthNonce) { aAuthNonce.erase(); };
  // - check auth helpers
  bool checkAuthPlain(
    const char *aUserName, const char *aPassWord, const char *aNonce, // given values
    const char *aAuthString, TAuthSecretTypes aAuthStringType // check against this
  );
  bool checkAuthMD5(
    const char *aUserName, const char *aMD5B64, const char *aNonce, // given values
    const char *aAuthString, TAuthSecretTypes aAuthStringType // check against this
  );
  bool checkMD5WithNonce(
    const char *aStringBeforeNonce, const char *aNonce, // given input
    const char *aMD5B64Creds // credential string to check
  );
protected:
  // - helper functions (for use be derived classes)
  bool getAuthBasicUserPass(const char *aBasicCreds, string &aUsername, string &aPassword);
  // - load remote connect params (syncml version, type, format and last nonce)
  //   Note: agents that can cache this information between sessions will load
  //   last info here.
  virtual void loadRemoteParams(void)
    { fSyncMLVersion=syncml_vers_unknown; fRemoteRequestedAuth=auth_none; fRemoteRequestedAuthEnc=fmt_chr; fRemoteNonce.erase(); }; // static defaults
  // - save remote connect params for use in next session (if descendant implements it)
  virtual void saveRemoteParams(void) { /* nop */ };
  // - Session level meta
  virtual SmlPcdataPtr_t newHeaderMeta(void);
  // - check remote devinf to detect special behaviour needed for some clients. Base class
  //   does not do anything on server level (configured rules are handled at session level)
  virtual localstatus checkRemoteSpecifics(SmlDevInfDevInfPtr_t aDevInfP, SmlDevInfDevInfPtr_t *aOverrideDevInfP);
  // - remote device is analyzed, possibly save status
  virtual void remoteAnalyzed(void) { /* nop */ };
  // - tell session whether it may accept an <Alert> in the map
  //   phase and restart the sync
  virtual bool allowAlertAfterMap() { return false; }
  // SyncML Toolkit interface
  InstanceID_t fSmlWorkspaceID; // SyncML toolkit workspace instance ID
  SmlEncoding_t fEncoding;      // Current encoding type in SyncML toolkit instance
  // Session custom time zones
  GZones fSessionZones;
  // session timing
  lineartime_t fSessionLastUsed; // time when session was last used
  lineartime_t fSessionStarted; // time when session was started
  lineartime_t fLastRequestStarted; // time when last request was received
  // session busy status (used for session count limiting normally)
  bool fSessionIsBusy;
  // session type
  // - SyncML protocol version
  TSyncMLVersions fSyncMLVersion;
  // - outgoing authorisation
  TAuthTypes fRemoteRequestedAuth; // type of auth requested by the remote
  TFmtTypes fRemoteRequestedAuthEnc; // type of encoding requested by the remote
  string fRemoteNonce; // next nonce to be used to authenticate with remote
  bool fNeedAuth; // set if we need to authorize to remote for next message
  // session ID vars
  string fSynchdrSessionID; // SyncML-protocol ID of this sync session (client generated)
  string fLocalURI; // local party URI
  string fInitialLocalURI; // local URI used in first message (or preconfigured with <externalurl>)
  string fLocalName; // local party optional name
  string fRemoteURI; // remote party URI (remote deviceID or URL)
  string fRemoteName; // remote party optional name
  string fRespondURI; // remote party URI to send response to
  string fLocalSessionID; // locally generated session ID (server generated)
  #ifndef MINIMAL_CODE
  string fRemoteDescName; // descriptive name of remote (set from DevInf and probably adjusted by remoterule)
  string fRemoteInfoString; // remote party information string (from DevInf)
  string fSyncUserName; // remote user name
  // 1:1 devInf details
  string fRemoteDevInf_devid;
  string fRemoteDevInf_devtyp;
  string fRemoteDevInf_mod;
  string fRemoteDevInf_man;
  string fRemoteDevInf_oem;
  string fRemoteDevInf_swv;
  string fRemoteDevInf_fwv;
  string fRemoteDevInf_hwv;
  #endif // MINIMAL_CODE
  #ifdef SCRIPT_SUPPORT
  // Session level script context
  TScriptContext *fSessionScriptContextP;
  #endif // SCRIPT_SUPPORT
  // Session options
  bool fReadOnly;
  // Session state vars
  // - incoming authorisation
  bool fSessionAuthorized; // session is (permanently) authorized, that is, further messages do not need authorization
  bool fMessageAuthorized; // this message is authorized
  sInt16 fAuthFailures; // count of failed authentication attempts by remote in a row (normally, server case), will cause abort if too many
  sInt16 fAuthRetries; // count of failed authentication attempts by myself at remote (normally, client case)
  // - session state
  TPackageStates fIncomingState; // incoming package state
  TPackageStates fCmdIncomingState; // while executing commands: state when command was received (actual might be different due to queueing)
  TPackageStates fOutgoingState; // outgoing package state
  bool fRestarting; // Set to true in TSyncSession::processAlertItem() while processing the first Alert from a
                    // client which requests another sync cycle. Applies to all further Alerts, cleared
                    // when entering fOutgoingState==psta_sync again.
  bool fFakeFinalFlag; // special flag to work around broken resume implementations
  bool fNewOutgoingPackage; // set if first outgoing message in outgoing package
  bool fNeedToAnswer; // set if an answer to currently processed message is needed (will be set by issuing of first non-synchdr-status)
  sInt32 fIncomingMsgID; // last incoming message ID (0 if none received yet)
  sInt32 fOutgoingMsgID; // last outgoing message ID (0 if none sent yet)
  bool fMessageRetried; // if set (by TSyncHeader::execute()) we have received a retried message and should resend the last answer
  bool fAborted; // if set, session is being aborted (and will be deleted at EndRequest)
  bool fSuspended; // if set, session is being suspended (stopped processing commands, will send Suspend Alert to remote at next opportunity)
  bool fSuspendAlertSent; // if set, session has sent a suspend alert to the remote party
  uInt16 fFailedDatastores;
  uInt16 fErrorItemDatastores;
  TSyError fAbortReasonStatus; // if fAborted, this contains a status code what command has aborted the session
  bool fLocalAbortReason; // if aborted, this signals if aborted due to local or remote reason
  bool fInProgress; // if set, session is in progress and must persist beyond this request
  // incoming Message status
  bool fMsgNoResp; // if set, current message MUST not be responded to. Suppresses all status sendig attempts
  bool fIgnoreIncomingCommands; // if set, commands dispatched will be ignored
  TSyError fStatusCodeForIgnored; // if fIgnoreIncomingCommands is set, this status code will be used to reply all incoming commands
  // - incoming data from a <moredata> split data item
  TSyncOpCommand *fIncompleteDataCommandP;
  // outgoing message status
  sInt32 fOutgoingCmdID; // last outgoing command ID (0 if none generated yet)
  bool fOutgoingStarted; // started preparing an outgoing message
  bool fOutgoingNoResp; // outgoing message does not want response at all
  // termination flag - set when TerminateSession() has finished executing
  bool fTerminated; // session is terminated (finally, not restartable!)
private:
  // debug logging
  #ifdef SYDEBUG
  TDebugLogger fSessionLogger; // the logger
  #endif // SYDEBUG
  // internal vars
  TSyncAppBase *fSyncAppBaseP; // the owning application base (dispatcher/client base)
  /* %%% prepared, to be implemented. Currently constant limits
  sInt32 fMaxIncomingMsgSize; // limit for incoming message, if<>0, causes MaxMsgSize Meta on outgoing SyncHdr
  sInt32 fMaxIncomingObjSize; // limit for incoming objects, if<>0, causes MaxObjSize Meta on outgoing SyncHdr
  */
  sInt32 fMaxOutgoingMsgSize; // max size of outgoing message, 0 if unlimited
  sInt32 fMaxOutgoingObjSize; // SyncML 1.1: max size of outgoing object, 0 if unlimited
  sInt32 fOutgoingMsgSize; // current size of outgoing message
  bool fOutgoingMessageFull; // outgoing message is full, message must be finished and sent
  // context-free command queues
  // - sent commands waiting for status
  TSmlCommandPContainer fStatusWaitCommands;
  // - received commands that could not be executed immediately
  TSmlCommandPContainer fDelayedExecutionCommands;
  sInt32 fDelayedExecSyncEnds;
  // - commands that must be queued until SyncHdr is generated
  TSmlCommandPContainer fHeaderWaitCommands;
  // SyncBody-context command queues
  // - commands to be issued only after all commands in this message have
  //   been processed and answered by a status
  TSmlCommandPContainer fEndOfMessageCommands;
  // - commands waiting for being sent in next outgoing message
  TSmlCommandPContainer fNextMessageCommands;
  // - commands waiting for being sent in next outgoing package
  TSmlCommandPContainer fNextPackageCommands;
  // - outgoing command that was interrupted by end of message and must be continued in next message
  TSmlCommand *fInterruptedCommandP;
  // - counter that gets incremented once per Alert 222 and decremented when package contents get sent
  uInt32 fNextMessageRequests;
  // - sequence nesting level
  sInt32 fSequenceNesting;
}; // TSyncSession


// macros
#define ISSUE_COMMAND(sp,c,l1,l2) { TSmlCommand* p=c; c=NULL; sp->issuePtr(p,l1,l2); }
#define ISSUE_COMMAND_ROOT(sp,c) { TSmlCommand* p=c; c=NULL; sp->issueRootPtr(p); }


#ifdef ENGINEINTERFACE_SUPPORT

// Support for EngineModule common interface
// =========================================

// session runtime parameters (such as access to session script vars)
class TSessionKey :
  public TStructFieldsKey
{
  typedef TStructFieldsKey inherited;
public:
  TSessionKey(TEngineInterface *aEngineInterfaceP, TSyncSession *aSessionP) :
    inherited(aEngineInterfaceP),
    fSessionP(aSessionP)
  {};
  virtual ~TSessionKey() {};

protected:
  // open subkey by name (not by path!)
  // - this is the actual implementation
  virtual TSyError OpenSubKeyByName(
    TSettingsKeyImpl *&aSettingsKeyP,
    cAppCharP aName, stringSize aNameSize,
    uInt16 aMode
  );

  // the associated sync session
  TSyncSession *fSessionP;
}; // TSessionKey

#endif // ENGINEINTERFACE_SUPPORT

} // namespace sysync


#endif // SYNC_SESSION_H


// eof
