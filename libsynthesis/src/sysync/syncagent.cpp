/*
 *  File:         syncagent.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TSyncAgent:   Provides functionality to run client or server
 *                sessions.
 *                Unifies former TSyncClient and TSyncServer
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 *  2009-09-30 : luz : created from syncclient.cpp and syncserver.cpp
 *
 */


// includes
#include "prefix_file.h"
#include "sysync.h"
#include "syncagent.h"
#include "syncappbase.h"

#ifdef HARD_CODED_SERVER_URI
  #include "syserial.h"
#endif

// includes that can't be in .h due to circular references
#ifdef SYSYNC_SERVER
#include "syncsessiondispatch.h"
#endif
#ifdef SYSYNC_CLIENT
#include "syncclientbase.h"
#endif

#ifdef SYSYNC_TOOL
#include <errno.h>
#endif


namespace sysync {


#ifdef SYSYNC_TOOL

// Support for SySync Diagnostic Tool
// ==================================


// test login into database
int testLogin(int argc, const char *argv[])
{
  if (argc<0) {
    // help requested
    CONSOLEPRINTF(("  login [<username> <password>] [<deviceid>]"));
    CONSOLEPRINTF(("    test login to database with syncml user/password and optional deviceid"));
    return EXIT_SUCCESS;
  }

  TSyncSession *sessionP = NULL;
  const char *username = NULL;
  const char *password = NULL;
  const char *deviceid = "sysytool_test";

  // check for argument
  if (argc<2) {
    // no user/password, test anonymous login
    if (argc>0) deviceid = argv[0];
  }
  else {
    // login with user/password
    username = argv[0];
    password = argv[1];
    if (argc>2) {
      // explicit device ID
      deviceid = argv[2];
    }
  }

  // get session to work with
  sessionP =
    static_cast<TSyncSessionDispatch *>(getSyncAppBase())->getSySyToolSession();

  bool authok = false;

  // try login
  if (username) {
    // real login with user and password
    authok = sessionP->SessionLogin(username, password, sectyp_clearpass, deviceid);
  }
  else {
    // anonymous - do a "login" with empty credentials
    authok = sessionP->SessionLogin("anonymous", NULL, sectyp_anonymous, deviceid);
  }

  if (authok) {
    CONSOLEPRINTF(("+++++ Successfully authorized"));
  }
  else {
    CONSOLEPRINTF(("----- Authorisation failed"));
  }

  return authok;
} // testLogin


// convert user data into internal format and back
int convertData(int argc, const char *argv[])
{
  if (argc<0) {
    // help requested
    CONSOLEPRINTF(("  convert <datastore name> <data file, vcard or vcalendar etc.> [<explicit input type>] [<output type>]"));
    CONSOLEPRINTF(("    Convert data to internal format of specified datastore and back"));
    return EXIT_SUCCESS;
  }

  TSyncSession *sessionP = NULL;
  const char *datastore = NULL;
  const char *rawfilename = NULL;
  const char *inputtype = NULL;
  const char *outputtype = NULL;

  // check for argument
  if (argc<2) {
    CONSOLEPRINTF(("required datatype name and raw file name arguments"));
    return EXIT_FAILURE;
  }
  datastore = argv[0];
  rawfilename = argv[1];
  if (argc>=3) {
    // third arg is explicit input type
    inputtype=argv[2];
  }
  outputtype=inputtype; // default to input type
  if (argc>=4) {
    // fourth arg is explicit output type
    outputtype=argv[3];
  }

  // get session to work with
  sessionP =
    static_cast<TSyncSessionDispatch *>(getSyncAppBase())->getSySyToolSession();
  // configure session
  sessionP->fRemoteCanHandleUTC = true; // run generator and parser in UTC enabled mode


  // switch mimimal debugging on
  sessionP->getDbgLogger()->setMask(sessionP->getDbgLogger()->getMask() | (DBG_PARSE+DBG_GEN));

  // find datastore
  TLocalEngineDS *datastoreP = sessionP->findLocalDataStore(datastore);
  TSyncItemType *inputtypeP = NULL;
  TSyncItemType *outputtypeP = NULL;
  if (!datastoreP) {
    CONSOLEPRINTF(("datastore type '%s' not found",datastore));
    return EXIT_FAILURE;
  }

  // find input type
  if (inputtype) {
    // search in datastore
    inputtypeP=datastoreP->getReceiveType(inputtype,NULL);
  }
  else {
    // use preferred rx type
    inputtypeP=datastoreP->getPreferredRxItemType();
  }
  if (!inputtypeP) {
    CONSOLEPRINTF(("input type not found"));
    return EXIT_FAILURE;
  }
  // find output type
  if (outputtype) {
    // search in datastore
    outputtypeP=datastoreP->getSendType(outputtype,NULL);
  }
  else {
    // use preferred rx type
    outputtypeP=datastoreP->getPreferredTxItemType();
  }
  if (!outputtypeP) {
    CONSOLEPRINTF(("output type not found"));
    return EXIT_FAILURE;
  }
  // prepare type usage
  if (inputtypeP==outputtypeP)
    inputtypeP->initDataTypeUse(datastoreP, true, true);
  else {
    inputtypeP->initDataTypeUse(datastoreP, false, true);
    outputtypeP->initDataTypeUse(datastoreP, true, false);
  }

  // now open file and read data item
  FILE *infile;
  size_t insize=0;
  uInt8 *databuffer;

  infile = fopen(rawfilename,"rb");
  if (!infile) {
    CONSOLEPRINTF(("Cannot open input file '%s' (%d)",rawfilename,errno));
    return EXIT_FAILURE;
  }
  // - get size of file
  fseek(infile,0,SEEK_END);
  insize=ftell(infile);
  fseek(infile,0,SEEK_SET);
  // - create buffer of appropriate size
  databuffer = new uInt8[insize];
  if (!databuffer) {
    CONSOLEPRINTF(("Not enough memory to read input file '%s' (%d)",rawfilename,errno));
    return EXIT_FAILURE;
  }
  // - read data
  if (fread(databuffer,1,insize,infile)<insize) {
    CONSOLEPRINTF(("Error reading input file '%s' (%d)",rawfilename,errno));
    return EXIT_FAILURE;
  }
  CONSOLEPRINTF(("\nNow converting into internal field representation\n"));
  // create a sml item
  TStatusCommand statusCmd(sessionP);
  SmlItemPtr_t smlitemP = newItem();
  smlitemP->data=newPCDataStringX(databuffer,true,insize);
  delete[] databuffer;
  // create and fill a Sync item
  TSyncItem *syncitemP = inputtypeP->newSyncItem(
    smlitemP, // SyncML toolkit item Data to be converted into SyncItem
    sop_replace, // the operation to be performed with this item
    fmt_chr,    // assume default (char) format
    inputtypeP, // target myself
    datastoreP, // local datastore
    statusCmd // status command that might be modified in case of error
  );
  // forget SyncML version
  smlFreeItemPtr(smlitemP);
  if (!syncitemP) {
    CONSOLEPRINTF(("Error converting input file to internal format (SyncML status code=%hd)",statusCmd.getStatusCode()));
    return EXIT_FAILURE;
  }

  CONSOLEPRINTF(("\nNow copying item and convert back to transport format\n"));

  // make new for output type
  TSyncItem *outsyncitemP = outputtypeP->newSyncItem(
    outputtypeP, // target myself
    datastoreP  // local datastore
  );
  // copy data
  outsyncitemP->replaceDataFrom(*syncitemP);
  delete syncitemP;
  // convert back
  smlitemP=outputtypeP->newSmlItem(
    outsyncitemP,   // the syncitem to be represented as SyncML
    datastoreP // local datastore
  );
  if (!syncitemP) {
    CONSOLEPRINTF(("Could not convert back item data"));
    return EXIT_FAILURE;
  }

  // forget converted back item
  smlFreeItemPtr(smlitemP);

  return EXIT_SUCCESS;
} // convertData

#endif // SYSYNC_TOOL




#ifdef PRECONFIGURED_SYNCREQUESTS

// Implementation of TSyncReqConfig
// ================================

// config for databases to sync with
TSyncReqConfig::TSyncReqConfig(TLocalDSConfig *aLocalDSCfg, TConfigElement *aParentElement) :
  TConfigElement("syncrequest",aParentElement),
  fLocalDSConfig(aLocalDSCfg)
{
  clear();
} // TSyncReqConfig::TSyncReqConfig


TSyncReqConfig::~TSyncReqConfig()
{
  // nop so far
} // TSyncReqConfig::~TSyncReqConfig


// init defaults
void TSyncReqConfig::clear(void)
{
  // init defaults
  // - local client datatstore subselection path or CGI (such as "test" in "contact/test")
  fLocalPathExtension.erase();
  // - remote server DB layer auth
  fDBUser.erase();
  fDBPassword.erase();
  // - remote server datastore path
  fServerDBPath.erase();
  // - sync mode
  fSyncMode=smo_twoway;
  fSlowSync=false; // default to non-slow
  // - DS 1.2 filtering parameters
  fRecordFilterQuery.erase();
  fFilterInclusive=false;
  // clear inherited
  inherited::clear();
} // TSyncReqConfig::clear


// config element parsing
bool TSyncReqConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"localpathextension")==0)
    expectString(fLocalPathExtension);
  else if (strucmp(aElementName,"dbuser")==0)
    expectString(fDBUser);
  else if (strucmp(aElementName,"dbpassword")==0)
    expectString(fDBPassword);
  else if (strucmp(aElementName,"dbpath")==0)
    expectString(fServerDBPath);
  else if (strucmp(aElementName,"syncmode")==0)
    expectEnum(sizeof(fSyncMode),&fSyncMode,SyncModeNames,numSyncModes);
  else if (strucmp(aElementName,"slowsync")==0)
    expectBool(fSlowSync);
  else if (strucmp(aElementName,"recordfilter")==0)
    expectString(fRecordFilterQuery);
  else if (strucmp(aElementName,"filterinclusive")==0)
    expectBool(fFilterInclusive);

  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TSyncReqConfig::localStartElement


// resolve
void TSyncReqConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    // check for required settings
    // %%% tbd
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TSyncReqConfig::localResolve


// create appropriate type of local datastore from config and init sync parameters
TLocalEngineDS *TSyncReqConfig::initNewLocalDataStore(TSyncSession *aSessionP)
{
  // - create appropriate type of localdsP
  TLocalEngineDS *localdsP = fLocalDSConfig->newLocalDataStore(aSessionP);
  // - set parameters
  localdsP->dsSetClientSyncParams(
    fSyncMode,
    fSlowSync,
    fServerDBPath.c_str(),
    fDBUser.c_str(),
    fDBPassword.c_str(),
    fLocalPathExtension.c_str(),
    fRecordFilterQuery.c_str(),
    fFilterInclusive
  );
  return localdsP;
} // TSyncReqConfig::initNewLocalDataStore

#endif // PRECONFIGURED_SYNCREQUESTS


// Implementation of TAgentConfig
// ==============================


TAgentConfig::TAgentConfig(const char* aName, TConfigElement *aParentElement) :
  inherited(aName,aParentElement)
{
  clear();
} // TAgentConfig::TAgentConfig


TAgentConfig::~TAgentConfig()
{
  clear();
} // TAgentConfig::~TAgentConfig


// init defaults
void TAgentConfig::clear(void)
{
  // clear inherited
  inherited::clear();
  // Note: we always clear both client and server fields - even if we'll only use one set later
  #ifdef SYSYNC_CLIENT
  // init client auth defaults (note that these MUST correspond with the defaults set by loadRemoteParams() !!!
  fAssumedServerAuth=auth_none; // start with no auth
  fAssumedServerAuthEnc=fmt_chr; // start with char encoding
  fAssumedNonce.erase(); // start with no nonce
  fPreferSlowSync=true;
  // auth retry options (mainly for stupid servers like SCTS)
  #ifdef SCTS_COMPATIBILITY_HACKS
  fNewSessionForAuthRetry=false;
  fNoRespURIForAuthRetry=false;
  #else
  fNewSessionForAuthRetry=true; // all production Synthesis clients had it hardcoded (ifdeffed) this way until 2.9.8.7
  fNoRespURIForAuthRetry=true; // all production Synthesis clients had it hardcoded (ifdeffed) this way until 2.9.8.7
  #endif
  fSmartAuthRetry=true; // try to be smart and try different auth retry (different from fNewSessionForAuthRetry/fNoRespURIForAuthRetry) if first attempts fail
  // other defaults
  fPutDevInfAtSlowSync=true; // smartner server needs it, and it does not harm so we have it on by default
  #ifndef NO_LOCAL_DBLOGIN
  fLocalDBUser.erase();
  fLocalDBPassword.erase();
  fNoLocalDBLogin=false;
  #endif
  #ifdef PRECONFIGURED_SYNCREQUESTS
  fEncoding=SML_XML; // default to more readable XML
  fServerUser.erase();
  fServerPassword.erase();
  fServerURI.erase();
  fTransportUser.erase();
  fTransportPassword.erase();
  fSocksHost.erase();
  fProxyHost.erase();
  fProxyUser.erase();
  fProxyPassword.erase();
  // remove sync db specifications
  TSyncReqList::iterator pos;
  for(pos=fSyncRequests.begin();pos!=fSyncRequests.end();pos++)
    delete *pos;
  fSyncRequests.clear();
  #endif
  // clear inherited
  inherited::clear();
  // modify timeout after inherited sets it
  fSessionTimeout=DEFAULT_CLIENTSESSIONTIMEOUT;
  // SyncML version support
  fAssumedServerVersion=MAX_SYNCML_VERSION; // try with highest version we support
  fMaxSyncMLVersionSupported=MAX_SYNCML_VERSION; // support what we request (overrides session default)
  #endif
  #ifdef SYSYNC_SERVER
  // init server defaults
  fRequestedAuth = auth_md5;
  fRequiredAuth = auth_md5;
  fAutoNonce = true;
  fConstantNonce.erase();
  fExternalURL.erase();
  fMaxGUIDSizeSent = 32; // reasonable size, but prevent braindamaged Exchange-size IDs to be sent
  fUseRespURI = true;
  fRespURIOnlyWhenDifferent = true;
  // modify timeout after inherited sets it
  fSessionTimeout=DEFAULT_SERVERSESSIONTIMEOUT;
  #endif
} // TAgentConfig::clear


#ifndef HARDCODED_CONFIG

// config element parsing
bool TAgentConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  if (IS_CLIENT) {
    // check the client elements
    #ifdef SYSYNC_CLIENT
    // - defaults for starting a session
    if (strucmp(aElementName,"defaultsyncmlversion")==0)
      expectEnum(sizeof(fAssumedServerVersion),&fAssumedServerVersion,SyncMLVersionNames,numSyncMLVersions);
    else if (strucmp(aElementName,"defaultauth")==0)
      expectEnum(sizeof(fAssumedServerAuth),&fAssumedServerAuth,authTypeNames,numAuthTypes);
    else if (strucmp(aElementName,"defaultauthencoding")==0)
      expectEnum(sizeof(fAssumedServerAuthEnc),&fAssumedServerAuthEnc,encodingFmtSyncMLNames,numFmtTypes);
    else if (strucmp(aElementName,"defaultauthnonce")==0)
      expectString(fAssumedNonce);
    else if (strucmp(aElementName,"preferslowsync")==0)
      expectBool(fPreferSlowSync);
    else if (strucmp(aElementName,"newsessionforretry")==0)
      expectBool(fNewSessionForAuthRetry);
    else if (strucmp(aElementName,"originaluriforretry")==0)
      expectBool(fNoRespURIForAuthRetry);
    else if (strucmp(aElementName,"smartauthretry")==0)
      expectBool(fSmartAuthRetry);
    // - other options
    else if (strucmp(aElementName,"putdevinfatslowsync")==0)
      expectBool(fPutDevInfAtSlowSync);
    else if (strucmp(aElementName,"fakedeviceid")==0)
      expectString(fFakeDeviceID);
    else
    #ifndef NO_LOCAL_DBLOGIN
    if (strucmp(aElementName,"localdbuser")==0)
      expectString(fLocalDBUser);
    else if (strucmp(aElementName,"localdbpassword")==0)
      expectString(fLocalDBPassword);
    else if (strucmp(aElementName,"nolocaldblogin")==0)
      expectBool(fNoLocalDBLogin);
    else
    #endif
    // serverURL is always available to allow define fixed URL in config that can't be overridden in profiles
    if (strucmp(aElementName,"serverurl")==0)
      expectString(fServerURI);
    else
    #ifdef PRECONFIGURED_SYNCREQUESTS
    if (strucmp(aElementName,"syncmlencoding")==0)
      expectEnum(sizeof(fEncoding),&fEncoding,SyncMLEncodingNames,numSyncMLEncodings);
    else if (strucmp(aElementName,"serveruser")==0)
      expectString(fServerUser);
    else if (strucmp(aElementName,"serverpassword")==0)
      expectString(fServerPassword);
    else if (strucmp(aElementName,"sockshost")==0)
      expectString(fSocksHost);
    else if (strucmp(aElementName,"proxyhost")==0)
      expectString(fProxyHost);
    else if (strucmp(aElementName,"proxyuser")==0)
      expectString(fProxyUser);
    else if (strucmp(aElementName,"proxypassword")==0)
      expectString(fProxyPassword);
    else if (strucmp(aElementName,"transportuser")==0)
      expectString(fTransportUser);
    else if (strucmp(aElementName,"transportpassword")==0)
      expectString(fTransportPassword);
    // - Sync DB specification
    else if (strucmp(aElementName,"syncrequest")==0) {
      // definition of a new datastore
      const char* nam = getAttr(aAttributes,"datastore");
      if (!nam) {
        ReportError(true,"syncrequest missing 'datastore' attribute");
      }
      else {
        // search datastore
        TLocalDSConfig *localDSCfgP = getLocalDS(nam);
        if (!localDSCfgP)
          return fail("unknown local datastore '%s' specified",nam);
        // create new syncDB config linked to that datastore
        TSyncReqConfig *syncreqcfgP = new TSyncReqConfig(localDSCfgP,this);
        // - save in list
        fSyncRequests.push_back(syncreqcfgP);
        // - let element handle parsing
        expectChildParsing(*syncreqcfgP);
      }
    }
    else
    #endif
      // - none known here
      return inherited::localStartElement(aElementName,aAttributes,aLine);
    #endif // SYSYNC_CLIENT
  }
  else {
    #ifdef SYSYNC_SERVER
    // check the server elements
    if (strucmp(aElementName,"requestedauth")==0)
      expectEnum(sizeof(fRequestedAuth),&fRequestedAuth,authTypeNames,numAuthTypes);
    else if (strucmp(aElementName,"requiredauth")==0)
      expectEnum(sizeof(fRequiredAuth),&fRequiredAuth,authTypeNames,numAuthTypes);
    // here to maintain compatibility with old pre 1.0.5.3 config files
    else if (strucmp(aElementName,"reqiredauth")==0)
      expectEnum(sizeof(fRequiredAuth),&fRequiredAuth,authTypeNames,numAuthTypes);
    else if (strucmp(aElementName,"autononce")==0)
      expectBool(fAutoNonce);
    else if (strucmp(aElementName,"constantnonce")==0)
      expectString(fConstantNonce);
    else if (strucmp(aElementName,"externalurl")==0)
      expectString(fExternalURL);
    else if (strucmp(aElementName,"maxguidsizesent")==0)
      expectUInt16(fMaxGUIDSizeSent);
    else if (strucmp(aElementName,"sendrespuri")==0)
      expectBool(fUseRespURI);
    else if (strucmp(aElementName,"respurionlywhendifferent")==0)
      expectBool(fRespURIOnlyWhenDifferent);
    // - none known here
    else
      return inherited::localStartElement(aElementName,aAttributes,aLine);
    #endif // SYSYNC_SERVER
  }
  // ok
  return true;
} // TAgentConfig::localStartElement

#endif // HARDCODED_CONFIG


// resolve
void TAgentConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    if (IS_CLIENT) {
      #ifdef SYSYNC_CLIENT
      #ifdef PRECONFIGURED_SYNCREQUESTS
      // - resolve requests
      TSyncReqList::iterator pos;
      for(pos=fSyncRequests.begin();pos!=fSyncRequests.end();pos++)
        (*pos)->Resolve(aLastPass);
      #endif
      #endif // SYSYNC_CLIENT
    }
    else {
      #ifdef SYSYNC_SERVER
      if (!fAutoNonce && fConstantNonce.empty())
        ReportError(false,"Warning: 'constantnonce' should be defined when 'autononce' is not set");
      #endif // SYSYNC_SERVER
    }
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TAgentConfig::localResolve



// Implementation of TSyncAgent
// =============================


// constructor
TSyncAgent::TSyncAgent(
  TSyncAppBase *aAppBaseP,
  TSyncSessionHandle *aSessionHandleP,
  const char *aSessionID // a session ID
) :
  TSyncSession(aAppBaseP,aSessionID)
{
  // General
  #ifdef ENGINE_LIBRARY
  // init the flags which are set by STEPCMD_SUSPEND, STEPCMD_ABORT and STEPCMD_TRANSPFAIL
  fAbortRequested = false;
  fSuspendRequested = false;
  fEngineSessionStatus = LOCERR_WRONGUSAGE;
  #ifdef NON_FULLY_GRANULAR_ENGINE
  // - erase the list of queued progress events
  fProgressInfoList.clear();
  fPendingStepCmd = 0; // none pending
  #endif // NON_FULLY_GRANULAR_ENGINE
  // - issue session start event here (in non-engine case this is done in TSyncSession constructor)
  SESSION_PROGRESS_EVENT(this,pev_sessionstart,NULL,0,0,0);
  #endif // ENGINE_LIBRARY
  #ifdef SYSYNC_SERVER
  // reset data counts
  fIncomingBytes = 0;
  fOutgoingBytes = 0;
  #endif
  fRestartSyncOnce = false;
  fRestartingSync = false;

  // Specific for Client or Server
  if (IS_CLIENT) {
    #ifdef SYSYNC_CLIENT
    #ifdef HARD_CODED_SERVER_URI
    fNoCRCPrefixLen = 0;
    #endif
    #ifdef ENGINE_LIBRARY
    // engine
    fClientEngineState = ces_idle;
    #endif
    // reset session now to get correct initial state
    InternalResetSession();
    // restart with session numbering at 1 (incremented before use)
    fClientSessionNo = 0;
    #endif // SYSYNC_CLIENT
  }
  else {
    #ifdef SYSYNC_SERVER
    // init answer buffer
    fBufferedAnswer = NULL;
    fBufferedAnswerSize = 0;
    #ifdef ENGINE_LIBRARY
    // engine
    fServerEngineState = ses_needdata;
    fRequestSize = 0;
    #endif
    // init own stuff
    InternalResetSession();
    // save session handle
    fSessionHandleP = aSessionHandleP; // link to handle
    // get config defaults
    TAgentConfig *configP = static_cast<TAgentConfig *>(aAppBaseP->getRootConfig()->fAgentConfigP);
    fUseRespURI = configP->fUseRespURI;
    // create all locally available datastores from config
    TLocalDSList::iterator pos;
    for (pos=configP->fDatastores.begin(); pos!=configP->fDatastores.end(); pos++) {
      // create the datastore
      addLocalDataStore(*pos);
    }
    #endif // SYSYNC_SERVER
  }
} // TSyncAgent::TSyncAgent


// destructor
TSyncAgent::~TSyncAgent()
{
  if (IS_CLIENT) {
    // make sure everything is terminated BEFORE destruction of hierarchy begins
    TerminateSession();
  }
  else {
    #ifdef SYSYNC_SERVER
    // forget any buffered answers
    bufferAnswer(NULL,0);
    // reset session
    InternalResetSession();
    // show session data transfer
    PDEBUGPRINTFX(DBG_HOT,(
      "Session data transfer statistics: incoming bytes=%ld, outgoing bytes=%ld",
      (long)fIncomingBytes,
      (long)fOutgoingBytes
    ));
    // DO NOT remove session from dispatcher here,
    //   this is the task of the dispatcher itself!
    CONSOLEPRINTF(("Terminated SyncML session (server id=%s)\n",getLocalSessionID()));
    // show end of session in global level
    POBJDEBUGPRINTFX(getSyncAppBase(),DBG_HOT,(
      "TSyncAgent::~TSyncAgent: Deleted SyncML session (local session id=%s)",
      getLocalSessionID()
    ));
    #endif // SYSYNC_SERVER
  }
} // TSyncAgent::~TSyncAgent


// Terminate session
void TSyncAgent::TerminateSession()
{
  #ifdef SYSYNC_CLIENT
  if (IS_CLIENT && !fTerminated) {
    InternalResetSession();
    #ifdef ENGINE_LIBRARY
    // switch state to done to prevent any further activity via SessionStep()
    fClientEngineState = ces_done;
    #endif
  }
  #endif // SYSYNC_CLIENT
  inherited::TerminateSession();
} // TSyncAgent::TerminateSession




void TSyncAgent::InternalResetSession(void)
{
  if (IS_CLIENT) {
    #ifdef SYSYNC_CLIENT
    // use remote URI as specified to start a session
    fRespondURI = fRemoteURI;
    #ifdef HARD_CODED_SERVER_URI
    #if defined(CUSTOM_URI_SUFFIX) && !defined(HARD_CODED_SERVER_URI_LEN)
    #error "HARD_CODED_SERVER_URI_LEN must be defined when using CUSTOM_URI_SUFFIX"
    #endif
    #ifdef HARD_CODED_SERVER_URI_LEN
    // only part of URL must match (max HARD_CODED_SERVER_URI_LEN chars will be added, less if URI is shorter)
    fServerURICRC = addNameToCRC(SYSER_CRC32_SEED, fRemoteURI.c_str()+fNoCRCPrefixLen, false, HARD_CODED_SERVER_URI_LEN);
    #else
    // entire URL (except prefix) must match
    fServerURICRC = addNameToCRC(SYSER_CRC32_SEED, fRemoteURI.c_str()+fNoCRCPrefixLen, false);
    #endif
    #endif
    // set SyncML version
    // Note: will be overridden with call to loadRemoteParams()
    fSyncMLVersion = syncml_vers_unknown; // unknown
    // will be cleared to suppress automatic use of DS 1.2 SINCE/BEFORE filters
    // (e.g. for date range in func_SetDaysRange())
    fServerHasSINCEBEFORE = true;
    // no outgoing alert 222 sent so far
    fOutgoingAlert222Count = 0;
    #endif
  }
  else {
    #ifdef SYSYNC_SERVER
    // %%% remove this as soon as Server is 1.1 compliant
    //fSyncMLVersion=syncml_vers_1_0; // only accepts 1.0 for now %%%%
    #endif
  }
} // TSyncAgent::InternalResetSession


// Virtual version
void TSyncAgent::ResetSession(void)
{
  // let ancestor do its stuff
  TSyncSession::ResetSession();
  // do my own stuff (and probably modify settings of ancestor!)
  InternalResetSession();
} // TSyncAgent::ResetSession


bool TSyncAgent::MessageStarted(SmlSyncHdrPtr_t aContentP, TStatusCommand &aStatusCommand, bool aBad)
{
  if (IS_CLIENT) {
    #ifdef SYSYNC_CLIENT
    return ClientMessageStarted(aContentP,aStatusCommand,aBad);
    #endif // SYSYNC_CLIENT
  }
  else {
    #ifdef SYSYNC_SERVER
    return ServerMessageStarted(aContentP,aStatusCommand,aBad);
    #endif // SYSYNC_SERVER
  }
}

void TSyncAgent::MessageEnded(bool aIncomingFinal)
{
  if (IS_CLIENT) {
    #ifdef SYSYNC_CLIENT
    ClientMessageEnded(aIncomingFinal);
    #endif // SYSYNC_CLIENT
  }
  else {
    #ifdef SYSYNC_SERVER
    ServerMessageEnded(aIncomingFinal);
    #endif // SYSYNC_SERVER
  }
}


// starting with engine version 2.0.8.7 a client's device ID (in devinf) is no longer
// a constant string, but the device's unique ID
string TSyncAgent::getDeviceID(void)
{
  if (fLocalURI.empty()) {
    if (IS_SERVER)
      return SYSYNC_SERVER_DEVID; // return default ID
    else
      return SYSYNC_CLIENT_DEVID; // return default ID
  }
  else
    return fLocalURI;
} // TSyncAgent::getDeviceID


// ask syncappbase for device type
string TSyncAgent::getDeviceType(void)
{
  // taken from configuration or default for engine type (client/server)
  return getSyncAppBase()->getDevTyp();
} // TSyncAgent::getDeviceType


#ifdef SYSYNC_CLIENT

// initialize the client session and link it with the SML toolkit
localstatus TSyncAgent::InitializeSession(uInt32 aProfileSelector, bool aAutoSyncSession)
{
  localstatus sta;

  // Select profile now (before creating instance, as encoding is dependent on profile)
  sta=SelectProfile(aProfileSelector, aAutoSyncSession);
  if (sta) return sta;
  // Start a SyncML toolkit instance now and set the encoding from config
  InstanceID_t myInstance;
  if (!getSyncAppBase()->newSmlInstance(
    getEncoding(),
    getRootConfig()->fLocalMaxMsgSize * 2, // twice the message size
    myInstance
  )) {
    return LOCERR_SMLFATAL;
  }
  // let toolkit know the session pointer
  if (getSyncAppBase()->setSmlInstanceUserData(myInstance,this)!=SML_ERR_OK) // toolkit must know session (as userData)
    return LOCERR_SMLFATAL;
  // remember the instance myself
  setSmlWorkspaceID(myInstance); // session must know toolkit workspace
  // done
  return LOCERR_OK;
} // TSyncAgent::InitializeSession



// select a profile (returns false if profile not found)
// Note: base class just tries to retrieve information from
//       config
localstatus TSyncAgent::SelectProfile(uInt32 aProfileSelector, bool aAutoSyncSession)
{
  #ifndef PRECONFIGURED_SYNCREQUESTS
  // no profile settings in config -> error
  return LOCERR_NOCFG;
  #else
  // get profile settings from config
  TAgentConfig *configP = static_cast<TAgentConfig *>(getRootConfig()->fAgentConfigP);
  // - get encoding
  fEncoding=configP->fEncoding; // SyncML encoding
  // - set server access details
  fRemoteURI=configP->fServerURI; // Remote URI = Server URI
  fTransportUser=configP->fTransportUser; // transport layer user (e.g. HTTP auth)
  fTransportPassword=configP->fTransportPassword; // transport layer password (e.g. HTTP auth)
  fServerUser=configP->fServerUser; // Server layer authentification user name
  fServerPassword=configP->fServerPassword; // Server layer authentification password
  #ifndef NO_LOCAL_DBLOGIN
  fLocalDBUser=configP->fLocalDBUser; // Local DB authentification user name (empty if local DB is single user)
  fNoLocalDBLogin=configP->fNoLocalDBLogin; // if set, no local DB auth takes place, but fLocalDBUser is used as userkey (depending on DB implementation)
  fLocalDBPassword=configP->fLocalDBPassword; // Local DB authentification password
  #endif
  fProxyHost=configP->fProxyHost; // Proxy host
  fSocksHost=configP->fSocksHost; // Socks host
  fProxyUser=configP->fProxyUser;
  fProxyPassword=configP->fProxyPassword;
  // Reset session after profile change
  // and also remove any datastores we might have
  ResetAndRemoveDatastores();
  // if tunnel, that's all for now
  if (aProfileSelector==TUNNEL_PROFILE_ID) return LOCERR_OK;
  // - create and init datastores needed for this session from config
  //   Note: probably config has no sync requests, but they are created later
  //         programmatically
  TSyncReqList::iterator pos;
  for (pos=configP->fSyncRequests.begin(); pos!=configP->fSyncRequests.end(); pos++) {
    // create and init the datastore
    fLocalDataStores.push_back(
      (*pos)->initNewLocalDataStore(this)
    );
  }
  // create "new" session ID (derivates will do this better)
  fClientSessionNo++;
  return LOCERR_OK;
  #endif
} // TSyncAgent::SelectProfile


// make sure we are logged in to local datastore
localstatus TSyncAgent::LocalLogin(void)
{
  #ifndef NO_LOCAL_DBLOGIN
  if (!fNoLocalDBLogin && !fLocalDBUser.empty()) {
    // check authorisation (login to correct user) in local DB
    if (!SessionLogin(fLocalDBUser.c_str(),fLocalDBPassword.c_str(),sectyp_clearpass,fRemoteURI.c_str())) {
      return localError(401); // done & error
    }
  }
  #endif
  return LOCERR_OK;
} // TSyncAgent::LocalLogin



// process message in the instance buffer
localstatus TSyncAgent::processAnswer(void)
{
  InstanceID_t myInstance = getSmlWorkspaceID();
  Ret_t err;

  // now process data
  DEBUGPRINTF(("===> now calling smlProcessData"));
  #ifdef SYDEBUG
  MemPtr_t data = NULL;
  MemSize_t datasize;
  smlPeekMessageBuffer(getSmlWorkspaceID(), false, &data, &datasize);
  #endif
  fIgnoreMsgErrs=false;
  err=smlProcessData(
    myInstance,
    SML_ALL_COMMANDS
  );
  if (err) {
    // dump the message that failed to process
    #ifdef SYDEBUG
    if (data) DumpSyncMLBuffer(data,datasize,false,err);
    #endif
    if (!fIgnoreMsgErrs) {
      PDEBUGPRINTFX(DBG_ERROR,("===> smlProcessData failed, returned 0x%hX",(sInt16)err));
      // other problem or already using SyncML 1.0 --> error
      return LOCERR_PROCESSMSG;
    }
  }
  // now check if this is a session restart
  if (isStarting()) {
    // this is still the beginning of a session
    return LOCERR_SESSIONRST;
  }
  return LOCERR_OK;
} // TSyncAgentBase::processAnswer

bool TSyncAgent::restartSync()
{
  if (IS_CLIENT) {
    // Restarting needs to be done if:
    // - all datastores support restarting a sync (= multiple read/write cycles)
    //   on client and server side (expected not be set if the engine itself on
    //   either side doesn't support it, so that is not checked separately)
    // - no datastore has failed in current iteration
    // - client has pending changes:
    //   - server temporarily rejected a change, queued for resending
    //   - an item added by the server was merged with another
    //     item locally (might have an updated queued, need to send delete)
    //   - change on client failed temporarily
    //   - the app or a datastore asked for a restart via the "restartsync"
    //     session variable
    if (!getenv("LIBSYNTHESIS_NO_RESTART")) {
      bool restartPossible=true; // ... unless proven otherwise below
      bool restartNecessary=fRestartSyncOnce; // one reason for restarting: requested by app
      int numActive = 0;

      // clear the flag after we checked it
      fRestartSyncOnce=false;
      if (fRestartSyncOnce)
        PDEBUGPRINTFX(DBG_SESSION,("try to restart sync as requested by app"));

      for (TLocalDataStorePContainer::iterator pos=fLocalDataStores.begin();
           pos!=fLocalDataStores.end();
           ++pos) {
        TLocalEngineDS *localDS = *pos;
        if (localDS->isActive()) {
          numActive++;
          if (!localDS->canRestart()) {
            PDEBUGPRINTFX(DBG_SESSION,("cannot restart, %s does not support it",
                                       localDS->getName()));
            restartPossible=false;
            break;
          }
          if (localDS->isAborted()) {
            PDEBUGPRINTFX(DBG_SESSION,("cannot restart, %s faileed",
                                       localDS->getName()));
            restartPossible=false;
            break;
          }
          if (!localDS->getRemoteDatastore() ||
              !localDS->getRemoteDatastore()->canRestart()) {
            PDEBUGPRINTFX(DBG_SESSION,("cannot restart, remote datastore %s matching with %s does not support it",
                                       localDS->getRemoteDatastore()->getName(),
                                       localDS->getName()));
            restartPossible=false;
            break;
          }
        }
        // check for pending local changes in the client
        if (localDS->numUnsentMaps() > 0) {
          PDEBUGPRINTFX(DBG_SESSION,("try to restart, %s has pending map entries",
                                     localDS->getName()));
          restartNecessary=true;
        }
        // TODO: detect temporarily failed items (server sent an add/update/delete
        // that we had to reject temporarily and where we expect the server to
        // resend the request)
        // detect pending changes (for example, 409 handling in binfile client)
        if (localDS->hasPendingChangesForNextSync()) {
          PDEBUGPRINTFX(DBG_SESSION,("try to restart, %s has pending changes",
                                     localDS->getName()));
          restartNecessary=true;
        }
      }

      return restartPossible && restartNecessary;
    }
  }

  return false;
}

// let session produce (or finish producing) next message into
// SML workspace
// - returns aDone if no answer needs to be sent (=end of session)
// - returns 0 if successful
// - returns SyncML status code if unsucessfully aborted session
localstatus TSyncAgent::NextMessage(bool &aDone)
{
  TLocalDataStorePContainer::iterator pos;
  TSyError status;

  TP_START(fTPInfo,TP_general); // could be new thread
  // default to not continuing
  aDone=true;
  #ifdef PROGRESS_EVENTS
  // check for user suspend
  if (!SESSION_PROGRESS_EVENT(this,pev_suspendcheck,NULL,0,0,0)) {
    SuspendSession(LOCERR_USERSUSPEND);
  }
  #endif
  // done if session was aborted by last received commands
  if (isAborted()) return getAbortReasonStatus(); // done & error
  // check package state
  if (fOutgoingState==psta_idle) {
    // if suspended here, we'll just stop - nothing has happened yet
    if (isSuspending()) {
      AbortSession(fAbortReasonStatus,true);
      return getAbortReasonStatus();
    }
    // start of an entirely new client session
    #ifdef HARD_CODED_SERVER_URI
    // extra check to limit hacking
    if (fServerURICRC != SERVER_URI_CRC) {
      // someone has tried to change the URI
      DEBUGPRINTFX(DBG_ERROR,("hardcoded Server URI CRC mismatch"));
      return LOCERR_LIMITED; // user will not know what this means, but we will
    }
    #endif
    // - check if we have client requests
    if (fLocalDataStores.size()<1) {
      PDEBUGPRINTFX(DBG_ERROR,("No datastores defined to sync with"));
      return LOCERR_NOCFG;
    }
    // %%% later, we could probably load cached info about
    //     server requested auth, devinf etc. here
    // use remote URI as specified to start a session
    fRespondURI=fRemoteURI;
    // get default params for sending first message to remote
    // Note: may include remote flag settings that influence creation of my own ID below, that's why we do it now here
    loadRemoteParams();
    // get info about my own URI, whatever that is
    #ifndef HARDCODED_CONFIG
    if (!static_cast<TAgentConfig *>(getRootConfig()->fAgentConfigP)->fFakeDeviceID.empty()) {
      // return fake Device ID if we have one defined in the config file (useful for testing)
      fLocalURI = static_cast<TAgentConfig *>(getRootConfig()->fAgentConfigP)->fFakeDeviceID;
    }
    else
    #endif
    {
      if (!getSyncAppBase()->getMyDeviceID(fLocalURI) || devidWithUserHash()) {
        // Device ID is not really unique, make a hash including user name to make it pseudo-unique
        // create MD5 hash from non-unique ID and user name
        // Note: when compiled with GUARANTEED_UNIQUE_DEVICID, devidWithUserHash() is always false.
        md5::SYSYNC_MD5_CTX context;
        uInt8 digest[16]; // for MD5 digest
        md5::Init (&context);
        // - add what we got for ID
        md5::Update (&context, (uInt8 *)fLocalURI.c_str(), fLocalURI.size());
        // - add user name, if any
        if (fLocalURI.size()>0) md5::Update (&context, (uInt8 *)fServerUser.c_str(), fServerUser.size());
        // - done
        md5::Final (digest, &context);
        // now make hex string of that
        fLocalURI = devidWithUserHash() ? 'x' : 'X'; // start with X to document this special case (lowercase = forced by remoteFlag)
        for (int n=0; n<16; n++) {
          AppendHexByte(fLocalURI,digest[n]);
        }
      }
    }
    // get my own name (if any)
    getPlatformString(pfs_device_name,fLocalName);
    // override some of these if not set by loadRemoteParams()
    if (fSyncMLVersion==syncml_vers_unknown)
      fSyncMLVersion=static_cast<TAgentConfig *>(getRootConfig()->fAgentConfigP)->fAssumedServerVersion;
    if (fRemoteRequestedAuth==auth_none)
      fRemoteRequestedAuth=static_cast<TAgentConfig *>(getRootConfig()->fAgentConfigP)->fAssumedServerAuth;
    if (fRemoteRequestedAuthEnc==fmt_chr)
      fRemoteRequestedAuthEnc=static_cast<TAgentConfig *>(getRootConfig()->fAgentConfigP)->fAssumedServerAuthEnc;
    if (fRemoteNonce.empty())
      fRemoteNonce=static_cast<TAgentConfig *>(getRootConfig()->fAgentConfigP)->fAssumedNonce;

    // we are not yet authenticated for the entire session
    fNeedAuth=true;
    // now ready for init
    fOutgoingState=psta_init; // %%%% could also set psta_initsync for combined init/sync
    fIncomingState=psta_idle; // remains idle until first answer SyncHdr with OK status is received
    fInProgress=true; // assume in progress
    // set session ID string
    StringObjPrintf(fSynchdrSessionID,"%hd",(sInt16)fClientSessionNo);
    // now we have a session id, can now display debug stuff
    #ifdef SYDEBUG
    string t;
    StringObjTimestamp(t,getSystemNowAs(TCTX_SYSTEM));
    PDEBUGPRINTFX(DBG_HOT,("\n[%s] =================> Starting new client session",t.c_str()));
    #endif
    // - make sure we are logged into the local database (if needed)
    status=LocalLogin();
    if (status!=LOCERR_OK) return status;
    // create header for first message no noResp
    issueHeader(false);
  }
  else {
    // check for proper end of session (caused by MessageEnded analysis)
    if (!fInProgress) {
      // give an opportunity to let make outgoing message end and flush xml end message
      FinishMessage(true, false);
      // end sync in all datastores (save anchors etc.)
      PDEBUGPRINTFX(DBG_PROTO,("Successful end of session -> calling engFinishDataStoreSync() for datastores now"));
      for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos)
        (*pos)->engFinishDataStoreSync(); // successful end
      PDEBUGPRINTFX(DBG_PROTO,("Session not any more in progress: NextMessage() returns OK status=0"));
      return LOCERR_OK; // done & ok
    }
  }
  // check expired case
  #ifdef APP_CAN_EXPIRE
  if (getClientBase()->fAppExpiryStatus!=LOCERR_OK) {
    PDEBUGPRINTFX(DBG_ERROR,("Evaluation Version expired - Please contact Synthesis AG for release version"));
    return getClientBase()->fAppExpiryStatus; // payment required, done & error
  }
  #endif
  if (fOutgoingState==psta_init || fOutgoingState==psta_initsync) {
    // - if suspended in init, nothing substantial has happened already, so just exit
    if (isSuspending() && fOutgoingState==psta_init) {
      AbortSession(fAbortReasonStatus,true);
      return getAbortReasonStatus();
    }
    // - prepare Alert(s) for databases to sync
    bool anyfirstsyncs=false;
    bool anyslowsyncs=false;
    TLocalEngineDS *localDS;
    for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
      // prepare alert (Note: datastore may be run by a superdatastore)
      localDS = *pos;
      status=localDS->engPrepareClientSyncAlert();
      if (status!=LOCERR_OK) {
        // local database error
        return localError(status); // not found
      }
      if (localDS->fFirstTimeSync) anyfirstsyncs=true;
      if (localDS->fSlowSync) anyslowsyncs=true;
    }
    // send devinf in Put command right away with init message if either...
    // - mustSendDevInf() returns true signalling an external condition that suggests sending devInf (like changed config)
    // - any datastore is doing first time sync
    // - fPutDevInfAtSlowSync is true and any datastore is doing slow sync
    // - not already sent (can be true here in later sync cycles)
    if (!fRemoteGotDevinf &&
        (mustSendDevInf() ||
         anyfirstsyncs ||
         (anyslowsyncs && static_cast<TAgentConfig *>(getRootConfig()->fAgentConfigP)->fPutDevInfAtSlowSync))
    ) {
      TDevInfPutCommand *putcmdP = new TDevInfPutCommand(this);
      issueRootPtr(putcmdP);
    }
    // try to load devinf from cache (only if we don't know it already)
    if (!fRemoteDataStoresKnown || !fRemoteDataTypesKnown) {
      SmlDevInfDevInfPtr_t devinfP;
      if (loadRemoteDevInf(getRemoteURI(),devinfP)) {
        // we have cached devinf, analyze it now
        analyzeRemoteDevInf(devinfP);
      }
    }
    // GET the server's info if server didn't send it and we haven't cached at least the datastores
    if (!fRemoteDataStoresKnown) {
      // if we know datastores here, but not types, this means that remote does not have
      // CTCap, so it makes no sense to issue a GET again.
      #ifndef NO_DEVINF_GET
      PDEBUGPRINTFX(DBG_REMOTEINFO,("Nothing known about server, request DevInf using GET command"));
      TGetCommand *getcommandP = new TGetCommand(this);
      getcommandP->addTargetLocItem(SyncMLDevInfNames[fSyncMLVersion]);
      string devinftype=SYNCML_DEVINF_META_TYPE;
      addEncoding(devinftype);
      getcommandP->setMeta(newMetaType(devinftype.c_str()));
      ISSUE_COMMAND_ROOT(this,getcommandP);
      #endif
    }
    // - create Alert(s) for databases to sync
    for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
      // create alert for non-subdatastores
      localDS = *pos;
      if (!localDS->isSubDatastore()) {
        TAlertCommand *alertcmdP;
        status = localDS->engGenerateClientSyncAlert(alertcmdP);
        if (status!=0) {
          // local database error
          return status; // not found
        }
        ///%%%% unneeded (probably got here by copy&paste accidentally): if (localDS->fFirstTimeSync) anyfirstsyncs=true;
        // issue alert
        issueRootPtr(alertcmdP);
      }
    }
    // append sync phase if we have combined init/sync
    if (fOutgoingState==psta_initsync) {
      fOutgoingState=psta_sync;
      fRestarting=false;
    }
  }
  // process sync/syncop/map generating phases after init
  if (!isSuspending()) {
    // normal, session continues
    if (fOutgoingState==psta_sync) {
      // hold back sync until server has finished first package (init or initsync)
      if (fIncomingState==psta_sync || fIncomingState==psta_initsync) {
        // start sync for alerted datastores
        for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
          // Note: some datastores might be aborted due to unsuccessful alert.
          if ((*pos)->isActive()) {
            // prepare engine for sync (%%% new routine in 3.2.0.3, summarizing engInitForSyncOps() and
            // switching to dssta_dataaccessstarted, i.e. loading sync set), but do in only once
            if (!((*pos)->testState(dssta_syncsetready))) {
              // not yet started
              status = (*pos)->engInitForClientSync();
              if (status!=LOCERR_OK ) {
                // failed
                if (status!=LOCERR_DATASTORE_ABORT) {
                  AbortSession(status,true);
                  return getAbortReasonStatus();
                }
              }
            }
            // start or continue (which is largely nop, as continuing works via unfinished sync command)
            // generating sync items
            (*pos)->engClientStartOfSyncMessage();
          }
        }
      }
    }
    else if (fOutgoingState==psta_map) {
      // hold back map until server has started sync at least (incominstate >=psta_sync)
      // NOTE: This is according to the specs, which says that client can begin
      //   with Map/update status package BEFORE sync package from server is
      //   completely received.
      // NOTE: Starfish server expects this and generates a few 222 alerts
      //   if we wait here, but then goes to map as well
      //   (so (fIncomingState==psta_map)-version works as well here!
      // %%%% other version: wait until server has started map phase as well
      // %%%% if (fIncomingState==psta_map) {
      if (fIncomingState>=psta_sync) {
        // start map for synced datastores
        for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
          // Note: some datastores might be aborted due to unsuccessful alert.
          if ((*pos)->isActive()) {
            // now call datastore to generate map command if not already done
            (*pos)->engClientStartOfMapMessage(fIncomingState<psta_map);
          }
        }
      }
    }
    else if (fOutgoingState==psta_supplement) {
      // we are waiting for the server to complete a pending phase altough we are already done
      // with everything we want to send.
      // -> just generate a Alert 222 and wait for server to complete
      PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,("Client finished so far, but needs to wait in supplement outgoing state until server finishes phase"));
    }
    else if (fOutgoingState!=psta_init) {
      // NOTE: can be psta_init because "if" begins again after psta_init checking
      //       to allow psta_init appending psta_sync for combined init/sync
      // no known state
      return 9999; // %%%%%
    }
  } // if not suspended

  // security only: exit here if session got aborted in between
  if (isAborted())
    return getAbortReasonStatus(); // done & error
  if (!fInProgress)
    return 9999; // that's fine with us
  // now, we know that we will (most probably) send a message, so default for aDone is false from now on
  aDone=false;
  bool outgoingfinal;

  // check for suspend
  if (isSuspending()) {
    // make sure we send a Suspend Alert
    TAlertCommand *alertCmdP = new TAlertCommand(this,NULL,(uInt16)224);
    // - we just put local and remote URIs here
    SmlItemPtr_t itemP = newItem();
    itemP->target = newLocation(fRemoteURI.c_str());
    itemP->source = newLocation(fLocalURI.c_str());
    alertCmdP->addItem(itemP);
    ISSUE_COMMAND_ROOT(this,alertCmdP);
    // outgoing message is final, regardless of any session state
    outgoingfinal=true;
    MarkSuspendAlertSent(true);
  }
  else {
    // Determine if package can be final and if we need an 222 Alert
    // NOTE: if any commands were interruped or not sent due to outgoing message
    //       size limits, FinishMessage() will prevent final anyway, so no
    //       separate checking for enOfSync or endOfMap is needed.
    // - can finalize message when server has at least started answering current package
    //   OR if this is the first message (probably repeatedly) sent
    outgoingfinal = fIncomingState >= fOutgoingState || fIncomingState==psta_idle;
    if (outgoingfinal) {
      // allow early success here in case of nothing to respond, and nothing pending
      // StarFish server does need this...
      if (!fNeedToAnswer) {
        if (hasPendingCommands()) {
          // we have pending commands, cannot be final message
          outgoingfinal=false;
        }
        else {
          // no pending commands -> we're done now
          PDEBUGPRINTFX(DBG_PROTO,("Early end of session (nothing to send to server any more) -> calling engFinishDataStoreSync() for datastores now"));
          // - end sync in all datastores (save anchors etc.)
          for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos)
            (*pos)->engFinishDataStoreSync(); // successful end
          PDEBUGPRINTFX(DBG_PROTO,("Session not any more in progress: NextMessage() returns OK status=0"));
          // done & ok
          aDone=true;
          return LOCERR_OK;
        }
      }
    }
  }
  /* A dummy alert indicates this is a message with only alert222 request*/
  bool dummyAlert = false;
  if (!outgoingfinal) {
    // - send Alert 222 if we need to continue package but have nothing to send
    //   (or ALWAYS_CONTINUE222 defined)
    #ifndef ALWAYS_CONTINUE222
    if (!fNeedToAnswer)
    #endif
    {
      /* End-less loop detection
       * Some servers will never end and triggers client sends
       * ALERT222 forever. Detect this scenario and abort the session if
       * detected.
       * It is still valid for the server to use ALERT222 to "keep-alive" the
       * connection.
       * Therefore the loop detection criteria is:
       * - Nothing to send except the 222 Alert (!fNeedToAnswer)
       * - 5 adjacent 222 alerts within 20 seconds
       * - no status for an actual sync op command received (fOutgoingAlert222Count will be reset by those)
       *   because a server sending pending status in small chunks could also trigger the detector otherwise
       */
      if (!fNeedToAnswer) {
        dummyAlert = true;
        if (fOutgoingAlert222Count++ == 0) {
          // start of 222 loop detection time
          fLastOutgoingAlert222 = getSystemNowAs(TCTX_UTC);
        } else if (fOutgoingAlert222Count > 5) {
          lineartime_t curTime = getSystemNowAs(TCTX_UTC);
          if (curTime - fLastOutgoingAlert222 < 20*secondToLinearTimeFactor) {
            PDEBUGPRINTFX(DBG_ERROR,(
              "Warning: More than 5 consecutive Alert 222 within 20 seconds- "
              "looks like endless loop, abort session"
            ));
            AbortSession(400, false);
            return getAbortReasonStatus();
          } else {
            fOutgoingAlert222Count = 0;
          }
        }
      }
      // not final, and nothing to answer otherwise: create alert-Command to request more info
      TAlertCommand *alertCmdP = new TAlertCommand(this,NULL,(uInt16)222);
      // %%% not clear from spec what has to be in item for 222 alert code
      //     but there MUST be an Item for the Alert command according to SyncML TK
      // - we just put local and remote URIs here
      SmlItemPtr_t itemP = newItem();
      itemP->target = newLocation(fRemoteURI.c_str());
      itemP->source = newLocation(fLocalURI.c_str());
      alertCmdP->addItem(itemP);
      ISSUE_COMMAND_ROOT(this,alertCmdP);
    }
  }
  // We send a response with no dummy alert, so reset the alert detector
  if (!dummyAlert) {
    fOutgoingAlert222Count = 0;
  }

  // Normally the package will be closed normally when the client is
  // ready. The exception is if we might want to restart the sync
  // session. This is checked below.
  bool finalprevented = false;

  // send custom end-of session puts or restart session
  if (!isSuspending() && outgoingfinal && fOutgoingState==psta_map) {
    // End of outgoing map package; let custom PUTs which may transmit some session statistics etc. happen now
    // TODO: this code is not reached when fOutgoingState==psta_map is skipped
    // by ClientMessageEnded() (see "All datastores in from-client-only mode, and no need to answer: skip map phase").
    // A bug? For restarting a sync, the problem is avoided by
    // disabling the standard-compliant "skip map phase" in favor
    // of "let client enter map phase" mode (fCompleteFromClientOnly).
    issueCustomEndPut();

    if (restartSync()) {
      // don't allow <Final> if we are going to restart sync in
      // ClientMessageEnded(), also remember that we did that
      finalprevented=true;
      // flag for TSyncAgent::ClientMessageEnded()
      fRestartingSync=true;
    }
  }
  // message complete, now finish it
  FinishMessage(
    outgoingfinal, // allowed if possible
    finalprevented // final not allowed when restarting sync
  );
  // Note, now fNewOutgoingPackage is set (by FinishMessage())
  // if next message will be responded to with a new package

  // debug info
  #ifdef SYDEBUG
  if (PDEBUGMASK & DBG_SESSION) {
    PDEBUGPRINTFX(DBG_SESSION,(
      "---> NextMessage, outgoing state='%s', incoming state='%s'",
      PackageStateNames[fOutgoingState],
      PackageStateNames[fIncomingState]
    ));
    for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
      // Show state of local datastores
      PDEBUGPRINTFX(DBG_SESSION,(
        "Local Datastore '%s': %sState=%s, %s%s sync, %s%s",
        (*pos)->getName(),
        (*pos)->isAborted() ? "ABORTED - " : "",
        (*pos)->getDSStateName(),
        (*pos)->isResuming() ? "RESUMED " : "",
        (*pos)->fSlowSync ? "SLOW" : "normal",
        SyncModeDescriptions[(*pos)->fSyncMode],
        (*pos)->fServerAlerted ? ", Server-Alerted" : ""
      ));
    }
  }
  PDEBUGENDBLOCK("SyncML_Outgoing");
  if (getLastIncomingMsgID()>0) {
    // we have already received an incoming message, so we have started an "SyncML_Incoming" blocks sometime
    PDEBUGENDBLOCK("SyncML_Incoming"); // terminate debug block of previous incoming message as well
  }
  #endif
  // ok
  return LOCERR_OK; // ok
} // TSyncAgent::NextMessage


// called after successful decoding of an incoming message
bool TSyncAgent::ClientMessageStarted(SmlSyncHdrPtr_t aContentP, TStatusCommand &aStatusCommand, bool aBad)
{
  // message not authorized by default
  fMessageAuthorized=false;

  // Check information from SyncHdr
  if (
    aBad ||
    (!(fSynchdrSessionID==smlPCDataToCharP(aContentP->sessionID))) ||
    (!(fLocalURI==smlSrcTargLocURIToCharP(aContentP->target)))
  ) {
    // bad response
    PDEBUGPRINTFX(DBG_ERROR,(
      "Bad SyncHeader from Server. Syntax %s, SessionID (rcvd/correct) = '%s' / '%s', LocalURI (rcvd/correct) = '%s' / '%s'",
      aBad ? "ok" : "BAD",
      smlPCDataToCharP(aContentP->sessionID),
      fSynchdrSessionID.c_str(),
      smlSrcTargLocURIToCharP(aContentP->target),
      fLocalURI.c_str()
    ));
    aStatusCommand.setStatusCode(400); // bad response/request
    AbortSession(400,true);
    return false;
  }
  // check for suspend: if we are suspended at this point, this means that we have sent the Suspend Alert already
  // in the previous message (due to user suspend request), so we can now terminate the session
  if (isSuspending() && isSuspendAlertSent()) {
    AbortSession(514,true,LOCERR_USERSUSPEND);
    return false;
  }
  // - RespURI (remote URI to respond to)
  if (aContentP->respURI) {
    fRespondURI=smlPCDataToCharP(aContentP->respURI);
    DEBUGPRINTFX(DBG_PROTO,("RespURI set to = '%s'",fRespondURI.c_str()));
  }
  // authorization check
  // Note: next message will be started not before status for last one
  //       has been processed. Commands issued before will automatically
  //       be queued by issuePtr()
  // %%% none for now
  fSessionAuthorized=true;
  fMessageAuthorized=true;
  // returns false on BAD header (but true on wrong/bad/missing cred)
  return true;
} // TSyncAgent::ClientMessageStarted


bool TSyncAgent::checkAllFromClientOnly()
{
  bool allFromClientOnly=false;
  // Note: the map phase will not take place, if all datastores are in
  //       send-to-server-only mode and we are not in non-conformant old
  //       synthesis-compatible fCompleteFromClientOnly mode.
  if (!fCompleteFromClientOnly) {
    // let all local datastores know that message has ended
    allFromClientOnly=true;
    for (TLocalDataStorePContainer::iterator pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
      // check sync modes
      if ((*pos)->isActive() && (*pos)->getSyncMode()!=smo_fromclient) {
        allFromClientOnly=false;
        break;
      }
    }
  }
  return allFromClientOnly;
}

// determines new package states and sets fInProgress
void TSyncAgent::ClientMessageEnded(bool aIncomingFinal)
{
  TLocalDataStorePContainer::iterator pos;

  // show status before processing
  PDEBUGPRINTFX(DBG_SESSION,(
    "MessageEnded starts   : old outgoing state='%s', old incoming state='%s', %sNeedToAnswer",
    PackageStateNames[fOutgoingState],
    PackageStateNames[fIncomingState],
    fNeedToAnswer ? "" : "NO "
  ));
  bool allFromClientOnly=false;
  // process exceptions
  if (fAborted) {
    PDEBUGPRINTFX(DBG_ERROR,("***** Session is flagged 'aborted' -> MessageEnded ends package and session"));
    fOutgoingState=psta_idle;
    fIncomingState=psta_idle;
    fInProgress=false;
  } // if aborted
  else if (!fMessageAuthorized) {
    // not authorized messages will just be ignored, so
    // nothing changes in states
    // %%% this will probably not really work, as we would need to repeat the last
    //     message in this (unlikely) case that fMessageAuthorized is not set for
    //     a non-first message (first message case is handled in handleHeaderStatus)
    DEBUGPRINTFX(DBG_ERROR,("***** received Message not authorized, ignore and DONT end package"));
    fInProgress=true;
  }
  else {
    fInProgress=true; // assume we need to continue
    allFromClientOnly=checkAllFromClientOnly();
    // new outgoing state is determined by the incomingState of this message
    // (which is the answer to the <final/> message of the previous outgoing package)
    if (fNewOutgoingPackage && fIncomingState!=psta_idle) {
      // last message sent was an end-of-package, so next will be a new package
      if (fIncomingState==psta_init) {
        // server has responded (or is still responding) to our finished init,
        // so client enters sync state now (but holds back sync until server
        // has finished init)
        fOutgoingState=psta_sync;
        fRestarting=false;
      }
      else if (fIncomingState==psta_sync || fIncomingState==psta_initsync) {
        // server has started (or already finished) sending statuses for our
        // <sync> or its own <sync>
        // client can enter map state (but holds back maps until server
        // has finished sync/initsync). In case of allFromClientOnly, we skip the map phase
        // but only if there is no need to answer.
        // Otherwise, this is most probably an old (pre 2.9.8.2) Synthesis server that has
        // sent an empty <sync> (and the status for it has set fNeedToAnswer), so we still
        // go to map phase.
        if (allFromClientOnly && !fNeedToAnswer) {
          fOutgoingState=psta_supplement; // all datastores are from-client-only, skip map phase
          PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,("All datastores in from-client-only mode, and no need to answer: skip map phase"));
        }
        else {
          fOutgoingState=psta_map; // Some datastores do from-server-only or twoway, so we need a map phase
          allFromClientOnly=false; // do not skip map phase
        }
      }
      else {
        // map is finished as well, we might need extra packages just to
        // finish getting results for map commands
        fOutgoingState=psta_supplement;
      }
    }
    // New incoming state is simply derived from the incoming state of
    // this message
    if (fRestartingSync) {
      PDEBUGPRINTFX(DBG_HOT,("MessageEnded: restart sync"));
      fOutgoingState=psta_init;
      fIncomingState=psta_init;
    } else if (aIncomingFinal && fIncomingState!=psta_idle) {
      if (fIncomingState==psta_init) {
        // switch to sync
        fIncomingState=psta_sync;
      }
      else if (fIncomingState==psta_sync || fIncomingState==psta_initsync) {
        // check what to do
        if (allFromClientOnly) {
          // no need to answer and allFromClientOnly -> this is the end of the session
          fIncomingState=psta_supplement;
          fInProgress=false; // normally, at end of map answer, we are done
        }
        else {
          fIncomingState=psta_map;
        }
      }
      else {
        // end of a map phase - end of session (if no fNeedToAnswer)
        fIncomingState=psta_supplement;
        // this only ALLOWS ending the session, but it will continue as long
        // as more than OK for SyncHdr (fNeedToAnswer) must be sent
        fInProgress=false; // normally, at end of map answer, we are done
      }
    }
    // continue anyway as long as we need to answer
    if (fNeedToAnswer) fInProgress=true;
  }
  // show states
  PDEBUGPRINTFX(DBG_HOT,(
    "MessageEnded finishes : new outgoing state='%s', new incoming state='%s', %sNeedToAnswer",
    PackageStateNames[fOutgoingState],
    PackageStateNames[fIncomingState],
    fNeedToAnswer ? "" : "NO "
  ));
  // let all local datastores know that message has ended
  for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
    TLocalEngineDS *localDS = *pos;
    // let them know
    localDS->engEndOfMessage();
    if (fRestartingSync) {
      // finish current session as far as the datastore is concerned
      localDS->engFinishDataStoreSync(LOCERR_OK);

      // and start again
      localDS->changeState(dssta_adminready);
      localDS->fFirstTimeSync = false;

      // unsetting flow sync leads to new sync mode:
      // - slow sync -> two-way sync or one-way if data direction was limited
      // - refresh sync -> one-way sync in same direction
      // - one-way sync -> do it again
      localDS->fSlowSync = false;
    }
    // Show state of local datastores
    PDEBUGPRINTFX(DBG_HOT,(
      "Local Datastore '%s': %sState=%s, %s%s sync, %s%s",
      localDS->getName(),
      localDS->isAborted() ? "ABORTED - " : "",
      localDS->getDSStateName(),
      localDS->isResuming() ? "RESUMED " : "",
      localDS->isSlowSync() ? "SLOW" : "normal",
      SyncModeDescriptions[localDS->getSyncMode()],
      localDS->fServerAlerted ? ", Server-Alerted" : ""
    ));
  }
  fRestartingSync = false;
  // thread might end here, so stop profiling
  TP_STOP(fTPInfo);
} // TSyncAgent::ClientMessageEnded


// get credentials/username to authenticate with remote party, NULL if none
SmlCredPtr_t TSyncAgent::newCredentialsForRemote(void)
{
  if (fNeedAuth) {
    // generate cretentials from username/password
    PDEBUGPRINTFX(DBG_PROTO+DBG_USERDATA,("Authenticating with server as user '%s'", fServerUser.c_str()));
    PDEBUGPRINTFX(DBG_PROTO+DBG_USERDATA+DBG_EXOTIC,("- using nonce '%s'", fRemoteNonce.c_str()));
    // NOTE: can be NULL when fServerRequestedAuth is auth_none
    return newCredentials(
      fServerUser.c_str(),
      fServerPassword.c_str()
    );
  }
  else {
    // already authorized, no auth needed
    return NULL;
  }
} // TSyncAgent::newCredentialsForRemote


// get client base
TSyncClientBase *TSyncAgent::getClientBase(void)
{
  return static_cast<TSyncClientBase *>(getSyncAppBase());
} // TSyncAgent::getClientBase


// retry older protocol, returns false if no older protocol to try
bool TSyncAgent::retryOlderProtocol(bool aSameVersionRetry, bool aOldMessageInBuffer)
{
  if (fIncomingState==psta_idle) {
    // if we have not started a session yet and not using oldest protocol already,
    // we want to retry with next older SyncML version
    if (aSameVersionRetry) {
      // just retry same version
      PDEBUGPRINTFX(DBG_PROTO,("Retrying session start with %s",SyncMLVerProtoNames[fSyncMLVersion]));
    }
    else if (fSyncMLVersion>getSessionConfig()->fMinSyncMLVersionSupported) {
      // next lower
      fSyncMLVersion=(TSyncMLVersions)(((uInt16)fSyncMLVersion)-1);
      PDEBUGPRINTFX(DBG_PROTO,("Server does not support our SyncML version, trying with %s",SyncMLVerProtoNames[fSyncMLVersion]));
    }
    else {
      // cannot retry
      return false;
    }
    // retry
    retryClientSessionStart(aOldMessageInBuffer);
    return true;
  }
  // session already started or no older protocol to try
  return false;
} // TSyncAgent::retryOlderProtocol


// prepares client session such that it will do a retry to start a session
// (but keeping already received auth/nonce/syncML-Version state)
void TSyncAgent::retryClientSessionStart(bool aOldMessageInBuffer)
{
  TAgentConfig *configP = static_cast<TAgentConfig *>(getRootConfig()->fAgentConfigP);

  // now restarting
  PDEBUGPRINTFX(DBG_HOT,("=================> Retrying Client Session Start"));
  bool newSessionForAuthRetry = configP->fNewSessionForAuthRetry;
  bool noRespURIForAuthRetry = configP->fNoRespURIForAuthRetry;
  // check if we should use modified behaviour (smart retries)
  if (configP->fSmartAuthRetry && fAuthRetries>MAX_NORMAL_AUTH_RETRIES) {
    if (newSessionForAuthRetry) {
      // if we had new session for retry, switch to in-session retry now
      newSessionForAuthRetry = false;
      noRespURIForAuthRetry = false;
    }
    else {
      // if we had in-session retry, try new session retry now
      newSessionForAuthRetry = true;
      noRespURIForAuthRetry = true;
    }
    PDEBUGPRINTFX(DBG_PROTO,("Smart retry with modified behaviour: newSessionForAuthRetry=%d, noRespURIForAuthRetry=%d",newSessionForAuthRetry,noRespURIForAuthRetry));
  }
  // now retry
  if (newSessionForAuthRetry) {
    // Notes:
    // - must apparently be disabled for SCTS 3.1.2 and possibly Mightyphone
    // - must be enabled e.g for for Magically Server
    // Create new session ID
    StringObjPrintf(fSynchdrSessionID,"%hd",(sInt16)++fClientSessionNo);
    // restart message counting at 1
    fIncomingMsgID=0;
    fOutgoingMsgID=0;
    // we must terminate the block here when we reset fIncomingMsgID, as NextMessage
    // only closes the incoming block when fIncomingMsgID>0
    PDEBUGENDBLOCK("SyncML_Incoming");
  }
  if (noRespURIForAuthRetry) {
    // Notes:
    // - must apparently be switched on for Starfish.
    // - must apparently be switched off for SCTS 3.1.2.
    // make sure we send next msg to the original URL
    fRespondURI=fRemoteURI;
  }
  // - make sure status for SyncHdr will not be generated!
  forgetHeaderWaitCommands();
  // check if we have already started next outgoing message
  if (!fOutgoingStarted) {
    if (aOldMessageInBuffer) {
      // make sure we start with a fresh output buffer
      // Note: This usually only occur when we are not currently parsing
      //       part of the buffer. If we are parsing, the remaining incoming
      //       message gets cleared as well.
      getClientBase()->clrUnreadSmlBufferdata();
    }
    // start a new message
    issueHeader(false);
  }
  else {
    if (aOldMessageInBuffer) {
      PDEBUGPRINTFX(DBG_ERROR,("Warning - restarting session with old message in output buffer"));
    }
  }
  // - make sure subsequent commands (most probably statuses for Alerts)
  //   don't get processed
  AbortCommandProcessing(0); // silently discard all further commands
  // - make sure possible processing errors do not abort the session
  fIgnoreMsgErrs = true;
} // TSyncAgent::retryClientSessionStart


#endif // SYSYNC_CLIENT

#ifdef SYSYNC_SERVER

// undefine these only for tests. Introduced to find problem with T68i
#define RESPURI_ONLY_WHEN_NEEDED

// create a RespURI string. If none needed, return NULL
SmlPcdataPtr_t TSyncAgent::newResponseURIForRemote(void)
{
  if (IS_CLIENT) {
    return NULL;
  }
  // do it in a transport-independent way, therefore let dispatcher do it
  string respURI; // empty string
  if (fUseRespURI) {
    getSyncAppBase()->generateRespURI(
      respURI,  // remains unaffected if no RespURI could be calculated
      fInitialLocalURI.c_str(), // initial URI used by remote to send first message
      fLocalSessionID.c_str()  // server generated unique session ID
    );
    // Omit RespURI if local URI as seen by client is identical
    if (getServerConfig()->fRespURIOnlyWhenDifferent) {
      // create RespURI only if different from original URI
      if (respURI==fLocalURI) {
        respURI.erase();
        DEBUGPRINTFX(DBG_SESSION,(
          "Generated RespURI and sourceLocURI are equal (%s)-> RespURI omitted",
          fLocalURI.c_str()
        ));
      }
    }
  }
  // Note: returns NULL if respURI is empty string
  return newPCDataOptString(respURI.c_str());
} // newResponseURIForRemote


// called after successful decoding of an incoming message
bool TSyncAgent::ServerMessageStarted(SmlSyncHdrPtr_t aContentP, TStatusCommand &aStatusCommand, bool aBad)
{
  // message not authorized by default
  fMessageAuthorized=false;

  // Get information from SyncHdr which is needed for answers
  // - session ID to be used for responses
  fSynchdrSessionID=smlPCDataToCharP(aContentP->sessionID);
  // - local URI (as seen by remote client)
  fLocalURI=smlSrcTargLocURIToCharP(aContentP->target);
  fLocalName=smlSrcTargLocNameToCharP(aContentP->target);
  // - also remember URI to which first message was sent
  // %%% note: incoming ID is not a criteria, because it might be >1 due to
  //     client retrying something which it thinks is for the same session
  //if (fIncomingMsgID==1) {
  if (fOutgoingMsgID==0) {
    // this is the first message, remember first URI used to contact server
    // (or set preconfigured string from <externalurl>)
    if (getServerConfig()->fExternalURL.empty())
      fInitialLocalURI=fLocalURI; // use what client sends to us
    else
      fInitialLocalURI=getServerConfig()->fExternalURL; // use preconfigured URL
    // Many clients, including SCTS send the second login attempt with a MsgID>1,
    // and depending on how they handle RespURI, they might get a new session for that
    // -> so, just handle the case that a new session does not start with MsgID=1
    if (fIncomingMsgID>1) {
      PDEBUGPRINTFX(DBG_ERROR,(
        "New session gets first message with MsgID=%ld (should be 1). Might be due to retries, adjusting OutgoingID as well",
        (long)fIncomingMsgID
      ));
      fOutgoingMsgID=fIncomingMsgID-1; // to make it match what client expects
    }
  }
  // - remote URI
  fRemoteURI=smlSrcTargLocURIToCharP(aContentP->source);
  fRemoteName=smlSrcTargLocNameToCharP(aContentP->source);
  // - RespURI (remote URI to respond to, if different from source)
  fRespondURI.erase();
  if (aContentP->respURI) {
    fRespondURI=smlPCDataToCharP(aContentP->respURI);
    DEBUGPRINTFX(DBG_PROTO,("RespURI specified = '%s'",fRespondURI.c_str()));
  }
  if (fRespondURI==fRemoteURI) fRespondURI.erase(); // if specified but equal to remote: act as if not specified
  // More checking if header was ok
  if (aBad) {
    // bad header, only do what is needed to get a status back to client
    fSessionAuthorized=false;
    fIncomingState=psta_init;
    fOutgoingState=psta_init;
    fNewOutgoingPackage=true;
    // issue header to make sure status can be sent back to client
    if (!fMsgNoResp)
      issueHeader(false); // issue header, do not prevent responses
  }
  else {
    // check busy (or expired) case
    if (serverBusy()) {
      #ifdef APP_CAN_EXPIRE
      if (getSyncAppBase()->fAppExpiryStatus!=LOCERR_OK) {
        aStatusCommand.setStatusCode(511); // server failure (expired)
        aStatusCommand.addItemString("License expired or invalid");
        PDEBUGPRINTFX(DBG_ERROR,("License expired or invalid - Please contact Synthesis AG to obtain license"));
      }
      else
      #endif
      {
        aStatusCommand.setStatusCode(101); // busy
      }
      issueHeader(false); // issue header, do not prevent responses
      AbortSession(0,true); // silently discard rest of commands
      return false; // header not ok
    }
    // now check what state we are in
    if (fIncomingState==psta_idle) {
      // Initialize
      // - session-wide authorization not yet there
      fSessionAuthorized=false;
      fMapSeen=false;
      // - session has started, we are processing first incoming
      //   package and generating first outgoing package
      //   (init, possibly changed to combined init/sync by <sync> in this package)
      fIncomingState=psta_init;
      fOutgoingState=psta_init;
      fNewOutgoingPackage=true;
    }
    // authorization check
    if (fIncomingState>=psta_init) {
      // now check authorization
      if (!fSessionAuthorized) {
        // started, but not yet permanently authorized
        fMessageAuthorized=checkCredentials(
          smlSrcTargLocNameToCharP(aContentP->source), // user name in clear text according to SyncML 1.0.1
          aContentP->cred, // actual credentials
          aStatusCommand
        );
        // NOTE: aStatusCommand has now the appropriate status and chal (set by checkCredentials())
        // if credentials do not match, stop processing commands (but stay with the session)
        if (!fMessageAuthorized) {
          AbortCommandProcessing(aStatusCommand.getStatusCode());
          PDEBUGPRINTFX(DBG_PROTO,("Authorization failed with status %hd, stop command processing",aStatusCommand.getStatusCode()));
        }
        // now determine if authorization is permanent or not
        if (fMessageAuthorized) {
          fAuthFailures=0; // reset count
          if (messageAuthRequired()) {
            // each message needs autorisation again (or no auth at all)
            // - 200 ok, next message needs authorization again (or again: none)
            fSessionAuthorized=false; // no permanent authorization
            aStatusCommand.setStatusCode(200);
            // - add challenge for next auth (different nonce)
            aStatusCommand.setChallenge(newSessionChallenge());
            PDEBUGPRINTFX(DBG_PROTO,("Authorization ok, but required again for subsequent messages: 200 + chal"));
          }
          else {
            // entire session is authorized
            fSessionAuthorized=true; // permanent authorization
            // - 212 authentication accepted (or 200 if none is reqired at all)
            aStatusCommand.setStatusCode(requestedAuthType()==auth_none ? 200 : 212);
            // - add challenge for next auth (in next session, but as we support carry
            //   forward via using sessionID, we need to send one here as well)
            aStatusCommand.setChallenge(newSessionChallenge());
            PDEBUGPRINTFX(DBG_PROTO,("Authorization accepted: 212"));
          }
        }
      } // authorisation check
      else {
        // already authorized from previous message
        PDEBUGPRINTFX(DBG_PROTO,("Authorization ok from previous request: 200"));
        fMessageAuthorized=true;
      }
      // Start response message AFTER auth check, to allow issueHeader
      // to check auth state and customize the header accordingly (no
      // RespURI for failed auth for example)
      if (!fMsgNoResp) {
        issueHeader(false); // issue header, do not prevent responses
      }
    } // if started at least
  } // if not aBad
  // return startmessage status
  // debug info
  #ifdef SYDEBUG
  if (PDEBUGMASK & DBG_SESSION) {
    PDEBUGPRINTFX(DBG_SESSION,(
      "---> MessageStarted, Message %sauthorized, incoming state='%s', outgoing state='%s'",
      fMessageAuthorized ? "" : "NOT ",
      PackageStateNames[fIncomingState],
      PackageStateNames[fOutgoingState]
    ));
    TLocalDataStorePContainer::iterator pos;
    for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
      // Show state of local datastores
      PDEBUGPRINTFX(DBG_SESSION,(
        "Local Datastore '%s': State=%s, %s%s sync, %s%s",
        (*pos)->getName(),
        (*pos)->getDSStateName(),
        (*pos)->isResuming() ? "RESUMED " : "",
        (*pos)->fSlowSync ? "SLOW" : "normal",
        SyncModeDescriptions[(*pos)->fSyncMode],
        (*pos)->fServerAlerted ? ", Server-Alerted" : ""
      ));
    }
  }
  #endif
  // final check for too many auth failures
  if (!fMessageAuthorized) {
    #ifdef NO_NONCE_OLD_BEAHVIOUR
    AbortSession(aStatusCommand.getStatusCode(),true); // local error
    // avoid special treatment of non-authorized message, we have aborted, this is enough
    fMessageAuthorized=true;
    #else
    // Unsuccessful auth, count this
    fAuthFailures++;
    PDEBUGPRINTFX(DBG_ERROR,(
      "Authorization failed %hd. time, (any reason), sending status %hd",
      fAuthFailures,
      aStatusCommand.getStatusCode()
    ));
    // - abort session after too many auth failures
    if (fAuthFailures>=MAX_AUTH_ATTEMPTS) {
      PDEBUGPRINTFX(DBG_ERROR,("Too many (>=%d) failures, aborting session",MAX_AUTH_ATTEMPTS));
      AbortSession(400,true);
    }
    #endif
  }
  // returns false on BAD header (but true on wrong/bad/missing cred)
  return true;
} // TSyncAgent::ServerMessageStarted


void TSyncAgent::ServerMessageEnded(bool aIncomingFinal)
{
  bool alldone;
  TPackageStates newoutgoingstate,newincomingstate;
  TLocalDataStorePContainer::iterator pos;
  bool allFromClientOnly=false;

  // Incoming message ends here - what is following are commands initiated by the server
  // not directly related to a incoming command.
  PDEBUGENDBLOCK("SyncML_Incoming");
  // assume that outgoing package is NOT finished, so outgoing state does not change
  newoutgoingstate=fOutgoingState;
  // new incoming state depends on whether this message is final or not
  if ((aIncomingFinal || (fIncomingState==psta_supplement)) && fMessageAuthorized) {
    // Note: in supplement state, incoming final is not relevant (may or may not be present, there is
    //       no next phase anyway
    // find out if this is a shortened session (no map phase) due to
    // from-client-only in all datastores
    allFromClientOnly=checkAllFromClientOnly();
    // determine what package comes next
    switch (fIncomingState) {
      case psta_init :
        newincomingstate=psta_sync;
        break;
      case psta_sync :
      case psta_initsync :
        // end of sync phase means end of session if all datastores are in from-client-only mode
        if (allFromClientOnly) {
          PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,("All datastores in from-client-only mode: don't expect map phase from client"));
          newincomingstate=psta_supplement;
        }
        else {
          newincomingstate=psta_map;
        }
        break;
      case psta_map :
      case psta_supplement : // supplement state does not exit automatically
        // after map, possibly some supplement status/alert 222 messages are needed from client
        newincomingstate=psta_supplement;
        break;
      default:
        // by default, back to idle
        newincomingstate=psta_idle;
        break;
    } // switch
  }
  else {
    // not final or not authorized: no change in state
    newincomingstate=fIncomingState;
  }
  // show status before processing
  PDEBUGPRINTFX(DBG_SESSION,(
    "---> MessageEnded starts   : old incoming state='%s', old outgoing state='%s', %sNeedToAnswer",
    PackageStateNames[fIncomingState],
    PackageStateNames[fOutgoingState],
    fNeedToAnswer ? "" : "NO "
  ));
  // process
  if (isAborted()) {
    // actual aborting has already taken place
    PDEBUGPRINTFX(DBG_ERROR,("***** Session is flagged 'aborted' -> MessageEnded ends package and session"));
    newoutgoingstate=psta_idle;
    newincomingstate=psta_idle;
    fInProgress=false;
  } // if aborted
  else if (isSuspending()) {
    // only flagged for suspend - but datastores are not yet aborted, do it now
    AbortSession(514,true,getAbortReasonStatus());
    PDEBUGPRINTFX(DBG_ERROR,("***** Session is flagged 'suspended' -> MessageEnded ends package and session"));
    newoutgoingstate=psta_idle;
    newincomingstate=psta_idle;
    fInProgress=false;
  }
  else if (!fMessageAuthorized) {
    // not authorized messages will just be ignored, no matter if final or not,
    // so outgoing will NEVER be final on non-authorized messages
    // %%% before 1.0.4.9, this was fInProgress=true
    //  DEBUGPRINTFX(DBG_ERROR,("***** Message not authorized, ignore and DONT end package, session continues"));
    //  fInProgress=true;
    PDEBUGPRINTFX(DBG_ERROR,("***** Message not authorized, ignore msg and terminate session"));
    fInProgress=false;
  }
  else {
    // determine if session continues living or not
    // - if in other than idle state, session will continue
    fInProgress =
      (newincomingstate!=psta_idle) || // if not idle, we'll continue
      !fMessageAuthorized; // if not authorized, we'll continue as well (retrying auth)
    // Check if we need to send an Alert 222 to get more messages of this package
    if (!aIncomingFinal) {
      // not end of incoming package
      #ifndef ALWAYS_CONTINUE222
      if (!fNeedToAnswer)
      #endif
      {
        #ifdef COMBINE_SYNCANDMAP
        // %%% make sure session gets to an end in case combined sync/map was used
        if (fMapSeen && fIncomingState==psta_map && fOutgoingState==psta_map) {
          DEBUGPRINTFX(DBG_HOT,("********** Incoming, non-final message in (combined)map state needs no answer -> force end of outgoing package"));
          newoutgoingstate=psta_idle;
        }
        else
        #endif
        {
          // detected 222-loop on init here: when we have nothing to answer in init
          // and nothing is alerted -> break session
          // %%% not sure if this is always ok
          if (fIncomingState<=psta_init) {
            PDEBUGPRINTFX(DBG_ERROR,("############## Looks like if we were looping in an init-repeat loop -> force final"));
            fInProgress=false;
            fOutgoingState=psta_idle;
          }
          else {
            // not final, and nothing to answer otherwise: create alert-Command to request more info
            TAlertCommand *alertCmdP = new TAlertCommand(this,NULL,(uInt16)222);
            // %%% not clear from spec what has to be in item for 222 alert code
            //     but there MUST be an Item for the Alert command according to SyncML TK
            // - we just put local and remote URIs here
            SmlItemPtr_t itemP = newItem();
            itemP->target = newLocation(fRemoteURI.c_str());
            itemP->source = newLocation(fLocalURI.c_str());
            alertCmdP->addItem(itemP);
            ISSUE_COMMAND_ROOT(this,alertCmdP);
          }
        }
      }
    }
    else {
      // end of package, finish processing package
      if (fIncomingState==psta_init) {
        // - try to load devinf from cache (only if we don't have both datastores and type info already)
        if (!fRemoteDataStoresKnown || !fRemoteDataTypesKnown) {
          SmlDevInfDevInfPtr_t devinfP;
          TStatusCommand dummystatus(this);
          if (loadRemoteDevInf(getRemoteURI(),devinfP)) {
            // we have cached devinf, analyze it now
            localstatus sta = analyzeRemoteDevInf(devinfP);
            PDEBUGPRINTFX(DBG_ERROR,("devInf from Cache could not be analyzed: error=%hd",sta));
          }
        }
        // - if no DevInf for remote datastores cached or received yet,
        //   issue GET for it now
        if (!fRemoteDataStoresKnown) {
          // if we know datastores here, but not types, this means that remote does not have
          // CTCap, so it makes no sense to issue a GET again.
          #ifndef NO_DEVINF_GET
          // end of initialisation package, but datastores not known yet
          // (=no DevInf Put received) --> ask for devinf now
          PDEBUGPRINTFX(DBG_REMOTEINFO,("No DevInf received or cached, request DevInf using GET command"));
          TGetCommand *getcommandP = new TGetCommand(this);
          getcommandP->addTargetLocItem(SyncMLDevInfNames[fSyncMLVersion]);
          string devinftype=SYNCML_DEVINF_META_TYPE;
          addEncoding(devinftype);
          getcommandP->setMeta(newMetaType(devinftype.c_str()));
          ISSUE_COMMAND_ROOT(this,getcommandP);
          #endif
        }
      }
    }
    // make sure syncing local datastores get informed of end-of-<Sync>-message
    if (fIncomingState==psta_sync || fIncomingState==psta_initsync) {
      // end of an incoming message of the Sync Package
      // - let all local datastores know, this is now the time to generate
      //   <sync> commands, if needed
      // Note: if there are SyncEnd commands delayed, this means that this is
      //       not yet the time to start <sync> commands. Instead, when all
      //       queued SyncEnd commands are executed later, engEndOfSyncFromRemote()
      //       will be called with the endOfAllSyncCommands flag true instead
      //       of now.
      for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
        (*pos)->engEndOfSyncFromRemote(aIncomingFinal && !delayedSyncEndsPending());
      }
    }
    // Detect outgoing package state transitions
    // - init to sync
    if (fOutgoingState==psta_init && newincomingstate>psta_init) {
      // new outgoing state is sync.
      // Note: In combined init&sync mode, sync command received in init state
      //       will set outgoing state from init to init-sync while processing message,
      //       so no transition needs to be detected here
      newoutgoingstate=psta_sync;
      fRestarting=false;
    }
    // - sync to map
    else if (
      (fOutgoingState==psta_sync || fOutgoingState==psta_initsync) && // outgoing is sync..
      (newincomingstate>=psta_initsync) && // ..and incoming has finished sync
      !allFromClientOnly // ..and this is not a session with all datastores doing from-client-only
    ) {
      // outgoing message belongs to Sync package
      // - ask all local datastores if they are finished with sync command generation
      alldone=true;
      for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
        alldone = alldone && (*pos)->isSyncDone();
      }
      if (alldone) {
        // outgoing state changes to map (or supplement if all datastores are from-client-only
        PDEBUGPRINTFX(DBG_HOT,("All datastores are done with generating <Sync>"));
        newoutgoingstate=psta_map;
        #ifdef COMBINE_SYNCANDMAP
        // %%% it seems as if 9210 needs combined Sync/Map package and
        if (fMapSeen) {
          // prevent FINAL to be sent at end of message
          DEBUGPRINTFX(DBG_HOT,("********** Combining outgoing sync and map-response packages into one"));
          fOutgoingState=psta_map;
        }
        #endif
      }
    }
    // - map (or from-client-only sync) to idle
    else if (
      (fOutgoingState==psta_map && newincomingstate==psta_supplement) ||
      (allFromClientOnly && (fOutgoingState==psta_sync || fOutgoingState==psta_initsync))
    ) {
      // we are going back to idle now
      newoutgoingstate=psta_idle;
      // session ends if it doesn't need to continue for session-level reasons
      if (!sessionMustContinue()) {
        PDEBUGPRINTFX(DBG_HOT,("Session completed, now let datastores terminate all sync operations"));
        for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
          // finished with Map: end of sync
          (*pos)->engFinishDataStoreSync(); // successful
        }
        // session ends now
        fInProgress=false;
        // let custom PUTs which may transmit some session statistics etc. happen now
        issueCustomEndPut();
      }
    }
    // - if no need to answer (e.g. nothing to send back except OK status for SyncHdr),
    //   session is over now (as well)
    if (!fNeedToAnswer) fInProgress=false;
  } // else
  // Now finish outgoing message
  #ifdef DONT_FINAL_BAD_AUTH_ATTEMPTS
  // - PREVENT final flag after failed auth attempts
  if(FinishMessage(
    fOutgoingState!=newoutgoingstate || fOutgoingState==psta_idle, // final when state changed or idle
    !fMessageAuthorized || serverBusy() // busy or unauthorized prevent final flag at any rate
  ))
  #else
  // - DO set final flag after failed auth attempts
  if(FinishMessage(
    !fMessageAuthorized || fOutgoingState!=newoutgoingstate || fOutgoingState==psta_idle, // final when state changed or idle
    serverBusy() // busy prevents final flag at any rate
  ))
  #endif
  {
    // outgoing state HAS changed
    fOutgoingState=newoutgoingstate;
  }
  // Now update incoming state
  fIncomingState=newincomingstate;
  // show states
  PDEBUGPRINTFX(DBG_HOT,(
    "---> MessageEnded finishes : new incoming state='%s', new outgoing state='%s', %sNeedToAnswer",
    PackageStateNames[fIncomingState],
    PackageStateNames[fOutgoingState],
    fNeedToAnswer ? "" : "NO "
  ));
  // let all local datastores know that message has ended
  for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
    // let them know
    (*pos)->engEndOfMessage();
    // Show state of local datastores
    PDEBUGPRINTFX(DBG_HOT,(
      "Local Datastore '%s': State=%s, %s%s sync, %s%s",
      (*pos)->getName(),
      (*pos)->getDSStateName(),
      (*pos)->isResuming() ? "RESUMED " : "",
      (*pos)->fSlowSync ? "SLOW" : "normal",
      SyncModeDescriptions[(*pos)->fSyncMode],
      (*pos)->fServerAlerted ? ", Server-Alerted" : ""
    ));
  }
  // End of outgoing message
  PDEBUGPRINTFX(DBG_HOT,(
    "=================> Finished generating outgoing message #%ld, request=%ld",
    (long)fOutgoingMsgID,
    (long)getSyncAppBase()->requestCount()
  ));
  PDEBUGENDBLOCK("SyncML_Outgoing");
} // TSyncAgent::ServerMessageEnded


void TSyncAgent::RequestEnded(bool &aHasData)
{
  // to make sure, finish any unfinished message
  FinishMessage(true); // final allowed, as this is an out-of-normal-order case anyway
  // if we need to answer, we have data
  // - SyncML specs 1.0.1 says that server must always respond, even if message
  //   contains of a Status for the SyncHdr only
  aHasData=true;
  // %%% first drafts of 1.0.1 said that SyncHdr Status only messages must not be sent...
  // aHasData=fNeedToAnswer; // %%%

  // now let all datastores know that request processing ends here (so they might
  // prepare for a thread switch)
  // terminate sync with all datastores
  TLocalDataStorePContainer::iterator pos;
  for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
    // now let them know that request has ended
    (*pos)->engRequestEnded();
  }
} // TSyncAgent::RequestEnded


// Called at end of Request, returns true if session must be deleted
// returns flag if data to be returned. If response URI was specified
// different, it is returned in aRespURI, otherwise aRespURI is empty.
bool TSyncAgent::EndRequest(bool &aHasData, string &aRespURI, uInt32 aReqBytes)
{
  // count incoming data
  fIncomingBytes+=aReqBytes;
  // let client or server do what is needed
  if (fMessageRetried) {
    // Message processing cancelled
    CancelMessageProcessing();
    // Nothing happened
    // - but count bytes
    fOutgoingBytes+=fBufferedAnswerSize;
    PDEBUGPRINTFX(DBG_HOT,(
      "========= Finished retried request with re-sending buffered answer (session %sin progress), incoming bytes=%ld, outgoing bytes=%ld",
      fInProgress ? "" : "NOT ",
      (long)aReqBytes,
      (long)fBufferedAnswerSize
    ));
    aHasData=false; // we do not have data in the sml instance (but we have/had some in the retry re-send buffer)
  }
  else {
    // end request
    RequestEnded(aHasData);
    // count bytes
    fOutgoingBytes+=getOutgoingMessageSize();
    PDEBUGPRINTFX(DBG_HOT,(
      "========= Finished request (session %sin progress), processing time=%ld msec, incoming bytes=%ld, outgoing bytes=%ld",
      fInProgress ? "" : "NOT ",
      (long)((getSystemNowAs(TCTX_UTC)-getLastRequestStarted()) * nanosecondsPerLinearTime / 1000000),
      (long)aReqBytes,
      (long)getOutgoingMessageSize()
    ));
    // return RespURI (is empty if none specified or equal to message source URI)
    aRespURI = fRespondURI;
  }
  if (!fInProgress) {
    // terminate datastores here already in case we are not in progress any more
    // here. If any of the datastores are in progress at this point, this is a
    // protocol violation, and therefore we return a 400.
    // Note: resetting the session later will also call TerminateDatastores, but then
    //       with a 408 (which is misleading when the session ends here due to protocol
    //       problem.
    TerminateDatastores(400);
  }
  //%%% moved to happen before end of SyncML_Outgoing
  //PDEBUGENDBLOCK("SyncML_Incoming");
  if (fRequestMinTime>0) {
    // make sure we spent enough time with this request, if not, artificially extend time
    // - get number of seconds already spent
    sInt32 t =
      (sInt32)((getSystemNowAs(TCTX_UTC)-getLastRequestStarted()) / (lineartime_t)secondToLinearTimeFactor);
    // - delay if needed
    if (t<fRequestMinTime) {
      PDEBUGPRINTFX(DBG_HOT,(
        "requestmintime is set to %ld seconds, we have spent only %ld seconds so far -> sleeping %ld seconds",
        (long)fRequestMinTime,
        (long)t,
        (long)fRequestMinTime-t
      ));
      CONSOLEPRINTF(("  ...delaying response by %ld seconds because requestmintime is set to %ld",(long)fRequestMinTime,(long)(fRequestMinTime-t)));
      sleepLineartime((lineartime_t)(fRequestMinTime-t)*secondToLinearTimeFactor);
    }
  }
  // thread might end here, so stop profiling
  TP_STOP(fTPInfo);
  #ifdef SYDEBUG
  // we are not the main thread any longer
  getDbgLogger()->DebugThreadOutputDone();
  #endif
  // return true if session is not in progress any more
  return(!fInProgress);
} // TSyncAgent::EndRequest


// buffer answer in the session's buffer if transport allows it
Ret_t TSyncAgent::bufferAnswer(MemPtr_t aAnswer, MemSize_t aAnswerSize)
{
  // get rid of previous buffered answer
  if (fBufferedAnswer)
    delete[] fBufferedAnswer;
  fBufferedAnswer=NULL;
  fBufferedAnswerSize=0;
  // save new answer (if not empty)
  if (aAnswer && aAnswerSize) {
    // allocate buffer
    fBufferedAnswer = new unsigned char[aAnswerSize];
    // copy data
    if (!fBufferedAnswer) return SML_ERR_NOT_ENOUGH_SPACE;
    memcpy(fBufferedAnswer,aAnswer,aAnswerSize);
    // save size
    fBufferedAnswerSize=aAnswerSize;
  }
  return SML_ERR_OK;
} // TSyncAgent::bufferAnswer


// get buffered answer from the session's buffer if there is any
void TSyncAgent::getBufferedAnswer(MemPtr_t &aAnswer, MemSize_t &aAnswerSize)
{
  aAnswer=fBufferedAnswer;
  aAnswerSize=fBufferedAnswerSize;
  PDEBUGPRINTFX(DBG_HOT,(
    "Buffered answer read from session: %ld bytes",
    fBufferedAnswerSize
  ));
} // TSyncAgent::getBufferedAnswer


// returns remaining time for request processing [seconds]
sInt32 TSyncAgent::RemainingRequestTime(void)
{
  if (IS_CLIENT) {
    // clients don't process requests, so there's no limit
    return 0x7FFFFFFF; // "infinite"
  }
  else {
    // if no request timeout specified, use session timeout
    sInt32 t = fRequestMaxTime ? fRequestMaxTime : getSessionConfig()->fSessionTimeout;
    // calculate number of remaining seconds
    return
      t==0 ?
        0x7FFFFFFF : // "infinite"
        t - (sInt32)((getSystemNowAs(TCTX_UTC)-getLastRequestStarted()) / (lineartime_t)secondToLinearTimeFactor);
  }
} // TSyncAgent::RemainingRequestTime





// process a Map command in context of server session
bool TSyncAgent::processMapCommand(
  SmlMapPtr_t aMapCommandP,      // the map command contents
  TStatusCommand &aStatusCommand, // pre-set 200 status, can be modified in case of errors
  bool &aQueueForLater
)
{
  bool allok=false; // assume not ok
  localstatus sta;

  // remember that this session has seen a map command already
  fMapSeen=true;
  // Detecting a map command in supplement incomin state indicates a
  // client like funambol that send to many <final/> in pre-map phases
  // (such as in 222-Alert messages). So we reset the session state back
  // to incoming/outgoing map to correct this client bug
  if (fIncomingState==psta_supplement) {
    // back to map phase, as client apparently IS still in map phase, despite too many
    // <final/> sent
    PDEBUGPRINTFX(DBG_ERROR,(
      "Warning: detected <Map> command after end of Map phase - buggy client sent too many <final/>. Re-entering map phase to compensate"
    ));
    fIncomingState=psta_map;
    fOutgoingState=psta_map;
  }
  // find database(s)
  // - get relative URI of requested database
  const char *targetdburi = smlSrcTargLocURIToCharP(aMapCommandP->target);
  TLocalEngineDS *datastoreP = findLocalDataStoreByURI(targetdburi);
  if (!datastoreP) {
    // no such local datastore
    aStatusCommand.setStatusCode(404); // not found
  }
  else {
    // local datastore found
    // - maps can be processed when we are at least ready for early (chached by client from previous session) maps
    if (datastoreP->testState(dssta_syncmodestable)) {
      // datastore is ready
      PDEBUGBLOCKFMT(("ProcessMap", "Processing items from Map command", "datastore=%s", targetdburi));
      allok=true; // assume all ok
      SmlMapItemListPtr_t nextnode = aMapCommandP->mapItemList;
      while (nextnode) {
        POINTERTEST(nextnode->mapItem,("MapItemList node w/o MapItem"));
        PDEBUGPRINTFX(DBG_HOT,(
          "Mapping remoteID='%s' to localID='%s'",
          smlSrcTargLocURIToCharP(nextnode->mapItem->source),
          smlSrcTargLocURIToCharP(nextnode->mapItem->target)
        ));
        sta = datastoreP->engProcessMap(
          #ifdef DONT_STRIP_PATHPREFIX_FROM_REMOTEIDS
          smlSrcTargLocURIToCharP(nextnode->mapItem->source),
          #else
          relativeURI(smlSrcTargLocURIToCharP(nextnode->mapItem->source)),
          #endif
          relativeURI(smlSrcTargLocURIToCharP(nextnode->mapItem->target))
        );
        if (sta!=LOCERR_OK) {
          PDEBUGPRINTFX(DBG_ERROR,("    Mapping FAILED!"));
          aStatusCommand.setStatusCode(sta);
          allok=false;
          break;
        }
        // next mapitem
        nextnode=nextnode->next;
      } // while more mapitems
      // terminate Map command
      allok=datastoreP->MapFinishAsServer(allok,aStatusCommand);
      PDEBUGENDBLOCK("ProcessMap");
    }
    else {
      // we must queue the command for later execution
      aQueueForLater=true;
      allok=true; // ok for now, we'll re-execute this later
    }
  } // database found
  return allok;
} // TSyncAgent::processMapCommand


// get next nonce string top be sent to remote party for subsequent MD5 auth
void TSyncAgent::getNextNonce(const char *aDeviceID, string &aNextNonce)
{
  fLastNonce.erase();
  if (getServerConfig()->fAutoNonce) {
    // generate nonce out of source ref and session ID
    // This scheme can provide nonce carrying forward between
    // sessions by initializing lastNonce with the srcRef/sessionid-1
    // assuming client to use nonce from last session.
    sInt32 sid;
    // use current day as nonce varying number
    sid = time(NULL) / 3600 / 24;
    generateNonce(fLastNonce,aDeviceID,sid);
  }
  else {
    // get constant nonce (if empty, this is NO nonce)
    fLastNonce=getServerConfig()->fConstantNonce;
  }
  // return new nonce
  DEBUGPRINTFX(DBG_PROTO,("getNextNonce: created nonce='%s'",fLastNonce.c_str()));
  aNextNonce=fLastNonce;
} // TSyncAgent::getNextNonce


// - get nonce string for specified deviceID
void TSyncAgent::getAuthNonce(const char *aDeviceID, string &aAuthNonce)
{
  // if no device ID, use session default nonce
  if (!aDeviceID) {
    TSyncSession::getAuthNonce(aDeviceID,fLastNonce);
  }
  else {
    // Basic nonce mechanism needing no per-device storage:
    // - we have no stored last nonce, but we can re-create nonce used
    //   for last session with this device by the used algorithm
    if (getServerConfig()->fAutoNonce) {
      if (fLastNonce.empty()) {
        // none available, produce new one
        sInt32 sid;
        // use current day as nonce varying number
        sid = time(NULL) / 3600 / 24;
        generateNonce(fLastNonce,aDeviceID,sid);
      }
    }
    else {
      // return constant nonce
      fLastNonce=getServerConfig()->fConstantNonce;
    }
  }
  DEBUGPRINTFX(DBG_PROTO,("getAuthNonce: current auth nonce='%s'",fLastNonce.c_str()));
  aAuthNonce=fLastNonce;
} // TSyncAgent::getAuthNonce



// info about server status
bool TSyncAgent::serverBusy(void)
{
  // return flag (which might have been set by some connection
  // limit code in sessiondispatch).
  // When app is expired, all server sessions are busy anyway
  #ifdef APP_CAN_EXPIRE
  return fSessionIsBusy || (getSyncAppBase()->fAppExpiryStatus!=LOCERR_OK);
  #else
  return fSessionIsBusy;
  #endif
} // TSyncAgent::serverBusy


// access to config
TAgentConfig *TSyncAgent::getServerConfig(void)
{
  TAgentConfig *scP;
  GET_CASTED_PTR(scP,TAgentConfig,getSyncAppBase()->getRootConfig()->fAgentConfigP,DEBUGTEXT("no TAgentConfig","ss1"));
  return scP;
} // TSyncAgent::getServerConfig


#endif // SYSYNC_SERVER


// info about requested auth type
TAuthTypes TSyncAgent::requestedAuthType(void)
{
  if (IS_SERVER) {
    #ifdef SYSYNC_SERVER
    return getServerConfig()->fRequestedAuth;
    #endif
  }
  else {
    return auth_none; // client does not require auth
  }
} // TSyncAgent::requestedAuthType


// check if auth type is allowed
bool TSyncAgent::isAuthTypeAllowed(TAuthTypes aAuthType)
{
  if (IS_SERVER) {
    #ifdef SYSYNC_SERVER
    return aAuthType>=getServerConfig()->fRequiredAuth;
    #endif
  }
  else {
    return true; // client accepts any auth
  }
} // TSyncAgent::isAuthTypeAllowed


// called when incoming SyncHdr fails to execute
bool TSyncAgent::syncHdrFailure(bool aTryAgain)
{
  if (IS_CLIENT) {
    // do not try to re-execute the header, just let message processing fail;
    // this will cause the client's main loop to try using an older protocol
    return false;
  }
  else {
    #ifdef SYSYNC_SERVER
    if (!aTryAgain) {
      // not already retried executing
      // special case: header failed to execute, this means that session must be reset
      // - Reset session (aborts all DB transactions etc.)
      ResetSession();
      PDEBUGPRINTFX(DBG_ERROR,("Trying to recover SyncHdr failure: =========== Session restarted ====================="));
      // - now all session infos are gone except this command which is owned by
      //   this function alone. Execute it again.
      aTryAgain=true;
    }
    else {
      // special special case: header failed to execute the second time
      DEBUGPRINTFX(DBG_ERROR,("Fatal internal problem, SyncHdr execution failed twice"));
      aTryAgain=false; // just to make sure
      SYSYNC_THROW(TSyncException("SyncHdr fatal execution problem"));
    }
    return aTryAgain;
    #endif
  }
} // TSyncAgent::syncHdrFailure


// handle status received for SyncHdr, returns false if not handled
bool TSyncAgent::handleHeaderStatus(TStatusCommand *aStatusCmdP)
{
  if (IS_CLIENT) {
    #ifdef SYSYNC_CLIENT
    TAgentConfig *configP = static_cast<TAgentConfig *>(getRootConfig()->fAgentConfigP);
    bool handled=true;
    const char *txt;
    SmlMetInfMetInfPtr_t chalmetaP=NULL;
    SmlChalPtr_t chalP;

    // first evaluate possible challenge in header status
    chalP = aStatusCmdP->getStatusElement()->chal;
    if (chalP) {
      chalmetaP = smlPCDataToMetInfP(chalP->meta);
      if (chalmetaP) {
        sInt16 ty;
        // - get auth type
        if (!chalmetaP->type) AbortSession(401,true); // missing auth, but no type
        txt = smlPCDataToCharP(chalmetaP->type);
        PDEBUGPRINTFX(DBG_PROTO,("Remote requests auth type='%s'",txt));
        if (StrToEnum(authTypeSyncMLNames,numAuthTypes,ty,txt))
          fRemoteRequestedAuth=(TAuthTypes)ty;
        else {
          AbortSession(406,true); // unknown auth type, not supported
          goto donewithstatus;
        }
        // - get auth format
        if (!smlPCDataToFormat(chalmetaP->format, fRemoteRequestedAuthEnc)) {
          AbortSession(406,true); // unknown auth format, not supported
          goto donewithstatus;
        }
        // - get next nonce
        if (chalmetaP->nextnonce) {
          // decode B64
          uInt32 l;
          uInt8 *nonce = b64::decode(smlPCDataToCharP(chalmetaP->nextnonce), 0, &l);
          fRemoteNonce.assign((char *)nonce,l);
          b64::free(nonce);
        }
        // - show
        PDEBUGPRINTFX(DBG_PROTO,(
          "Next Cred will have type='%s' and format='%s' and use nonce='%s'",
          authTypeNames[fRemoteRequestedAuth],
          encodingFmtNames[fRemoteRequestedAuthEnc],
          fRemoteNonce.c_str()
        ));
      }
      /* %%% do not save here already, we don't know if SyncML version is ok
             moved to those status code cases below that signal
      // let descendant possibly save auth params
      saveRemoteParams();
      */
    }
    // now evaluate status code
    switch (aStatusCmdP->getStatusCode()) {
      case 101: // Busy
        // Abort
        AbortSession(101,false);
        break;
      case 212: // authentication accepted for entire session
        fNeedAuth=false; // no need for further auth
        PDEBUGPRINTFX(DBG_PROTO,("Remote accepted authentication for entire session"));
      case 200: // authentication accepted for this message
        // if this is the first authorized message we get an OK for the synchdr, this is
        // also the first incoming message that is really processed as init message
        if (fIncomingState==psta_idle && fMessageAuthorized) {
          // first incoming is expected to be same as first outgoing (init or initsync)
          fIncomingState=fOutgoingState;
          PDEBUGPRINTFX(DBG_PROTO,("Authenticated successfully with remote server"));
        }
        else {
          PDEBUGPRINTFX(DBG_PROTO,("Authentication with server ok for this message"));
        }
        // let descendant possibly save auth params
        saveRemoteParams();
        break;
      case 501: // handle a "command not implemented" for the SyncHdr like 513 (indication that server does not like our header)
      case 400: // ..and 400 as well (sync4j case, as it seems)
      case 513: // bad protocol version
      case 505: // bad DTD version (NextHaus/DeskNow case)
        // try with next lower protocol
        PDEBUGENDBLOCK("processStatus"); // done processing status
        if (!retryOlderProtocol()) {
          // no older SyncML protocol we can try --> abort
          AbortSession(513,false); // server does not know any of our SyncML versions
        }
        break;
      case 401: // bad authentication
        // Bad authorisation
        if (fAuthRetries==0)
          // if first attempt is rejected with "bad", we conclude that the
          // last attempt was carrying auth data and was not a attempt to get challenge
          // from server. Therefore we count this as two tries (one get chal, one really failing)
          fAuthRetries=2;
        else
          fAuthRetries++; // just count attempt to auth
        /* %%% no longer required, is tested below at authfail:
        if (fAuthRetries>MAX_AUTH_RETRIES) {
          AbortSession(401,false); // abort session, too many retries
          break;
        }
        */
        // Treat no nonce like empty nonce to make sure that a server (like SySync old versions...)
        // that does not send a nonce at all does not get auth with some old, invalid nonce string included.
        if (chalmetaP && chalmetaP->nextnonce==NULL) fRemoteNonce.erase();
        // otherwise treat like 407
        goto authfail;
      case 407: // authentication required
        // new since 2.0.4.6: count this as well (normally this happens once when sending
        // no auth to the server to force it to send us auth chal first).
        fAuthRetries++;
      authfail:
        PDEBUGPRINTFX(DBG_ERROR,("Authentication failed (status=%hd) with remote server",aStatusCmdP->getStatusCode()));
        // Auth fail after we have received a valid response for the init message indicates protocol messed up
        if (fIncomingState!=psta_idle) {
          AbortSession(400,true); // error in protocol handling from remote
          break;
        }
        // Check if smart retries (with modified in-session vs out-of-session behaviour) are enabled
        if (!configP->fSmartAuthRetry && fAuthRetries>MAX_NORMAL_AUTH_RETRIES) {
          fAuthRetries = MAX_SMART_AUTH_RETRIES+1; // skip additional smart retries
        }
        // Missing or bad authorisation, evaluate chal
        if (!chalmetaP || fAuthRetries>MAX_SMART_AUTH_RETRIES) {
          #ifdef SYDEBUG
          if (!chalmetaP) {
            PDEBUGPRINTFX(DBG_ERROR,("Bad auth but no challenge in response status -> can't work - no retry"));
          }
          #endif
          AbortSession(aStatusCmdP->getStatusCode(),false); // retries exhausted or no retry possible (no chal) -> stop session
          break;
        }
        // let descendant possibly save auth params
        saveRemoteParams();
        // modify session for re-start
        PDEBUGENDBLOCK("processStatus"); // done processing status
        retryClientSessionStart(false); // no previously sent message in the buffer
        break;
      default:
        handled=false; // could not handle status
    } // switch
  donewithstatus:
    // Anyway, reception of status for header enables generation of next message header
    // (plus already generated commands such as status for response header)
    if (!fMsgNoResp && !isAborted()) {
      // issue header now if not already issued above
      if (!fOutgoingStarted) {
        // interrupt status processing block here as issueHeader will do a start-of-message PDEBUGBLOCK
        PDEBUGENDBLOCK("processStatus");
        issueHeader(false);
        PDEBUGBLOCKDESC("processStatus","finishing processing incoming SyncHdr Status");
      }
    }
    // return handled status
    return handled;
    #endif // SYSYNC_SERVER
  }
  else {
    // nothing special
    return inherited::handleHeaderStatus(aStatusCmdP);
  }
} // TSyncAgent::handleHeaderStatus


// - start sync group (called in client or server roles)
bool TSyncAgent::processSyncStart(
  SmlSyncPtr_t aSyncP,           // the Sync element
  TStatusCommand &aStatusCommand, // pre-set 200 status, can be modified in case of errors
  bool &aQueueForLater // will be set if command must be queued for later (re-)execution
)
{
  if (IS_CLIENT) {
    #ifdef SYSYNC_CLIENT
    if (fIncomingState!=psta_sync && fIncomingState!=psta_initsync) {
      aStatusCommand.setStatusCode(403); // forbidden in this context
      PDEBUGPRINTFX(DBG_ERROR,("Sync command not allowed outside of sync phase (-> 403)"));
      AbortSession(400,true);
      return false;
    }
    // just find appropriate database, must be already initialized for sync!
    // determine local database to sync with (target)
    TLocalEngineDS *datastoreP = findLocalDataStoreByURI(smlSrcTargLocURIToCharP(aSyncP->target));
    if (!datastoreP) {
      // no such local datastore
      PDEBUGPRINTFX(DBG_ERROR,("Sync command for unknown DS locURI '%s' (-> 404)",smlSrcTargLocURIToCharP(aSyncP->target)));
      aStatusCommand.setStatusCode(404); // not found
      return false;
    }
    else {
      // save the pointer, will e.g. be used to route subsequent server commands
      fLocalSyncDatastoreP=datastoreP;
      // let local datastore know
      return fLocalSyncDatastoreP->engProcessSyncCmd(aSyncP,aStatusCommand,aQueueForLater);
    }
    return true;
    #endif // SYSYNC_CLIENT
  }
  else {
    #ifdef SYSYNC_SERVER
    // Init datastores for sync
    localstatus sta = initSync(
      smlSrcTargLocURIToCharP(aSyncP->target),  // local datastore
      smlSrcTargLocURIToCharP(aSyncP->source)  // remote datastore
    );
    if (sta!=LOCERR_OK) {
      aStatusCommand.setStatusCode(sta);
      return false;
    }
    // let local datastore prepare for sync as server
    // - let local process sync command
    bool ok=fLocalSyncDatastoreP->engProcessSyncCmd(aSyncP,aStatusCommand,aQueueForLater);
    // Note: ok means that the sync command is addressing existing datastores. However,
    //       it does not mean that the actual processing is already executed; aQueueForLater
    //       could be set!
    // if ok and not queued: update package states
    if (ok) {
      if (fIncomingState==psta_init || fIncomingState==psta_initsync) {
        // detected sync command in init package -> this is combined init/sync
        #ifdef SYDEBUG
        if (fIncomingState==psta_init)
          DEBUGPRINTFX(DBG_HOT,("<Sync> started init package -> switching to combined init/sync"));
        #endif
        // - set new incoming state
        fIncomingState=psta_initsync;
        // - also update outgoing state, if it is in init package
        if (fOutgoingState==psta_init)
          fOutgoingState=psta_initsync;
      }
      else if (fCmdIncomingState!=psta_sync) {
        DEBUGPRINTFX(DBG_ERROR,(
          "<Sync> found in wrong incoming package state '%s' -> aborting session",
          PackageStateNames[fCmdIncomingState]
        ));
        aStatusCommand.setStatusCode(403); // forbidden
        fLocalSyncDatastoreP->engAbortDataStoreSync(403,true); // abort, local problem
        ok=false;
      }
      else {
        // - show sync start
        DEBUGPRINTFX(DBG_HOT,(
          "<Sync> started, cmd-incoming state='%s', incoming state='%s', outgoing state='%s'",
          PackageStateNames[fCmdIncomingState],
          PackageStateNames[fIncomingState],
          PackageStateNames[fOutgoingState]
        ));
      }
    }
    return ok;
    #endif // SYSYNC_SERVER
  }
} // TSyncAgent::processSyncStart




#ifdef ENGINEINTERFACE_SUPPORT

// Support for EngineModule common interface
// =========================================


/// @brief Get new session key to access details of this session
appPointer TSyncAgent::newSessionKey(TEngineInterface *aEngineInterfaceP)
{
  return new TAgentParamsKey(aEngineInterfaceP,this);
} // TSyncAgent::newSessionKey


#ifdef ENGINE_LIBRARY

TSyError TSyncAgent::SessionStep(uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  #ifdef NON_FULLY_GRANULAR_ENGINE
  // pre-process step command and generate pseudo-steps to empty progress event queue
  // preprocess general step codes
  switch (aStepCmd) {
    case STEPCMD_TRANSPFAIL :
      // directly abort
      AbortSession(LOCERR_TRANSPFAIL,true);
      goto abort;
    case STEPCMD_ABORT :
      // directly abort
      AbortSession(LOCERR_USERABORT,true);
    abort:
      // also set the flag so subsequent progress events will result the abort status
      fAbortRequested=true;
      aStepCmd = STEPCMD_STEP; // convert to normal step
      goto step;
    case STEPCMD_SUSPEND :
      // directly suspend
      SuspendSession(LOCERR_USERSUSPEND);
      // also set the flag so subsequent pev_suspendcheck events will result the suspend status
      fSuspendRequested=true;
      aStepCmd = STEPCMD_OK; // this is a out-of-order step, and always just returns STEPCMD_OK.
      fEngineSessionStatus = 0; // ok for now, subsequent steps will perform the actual suspend
      goto done; // no more action for now
    case STEPCMD_STEP :
    step:
    // first just return all queued up progress events
    if (fProgressInfoList.size()>0) {
      // get first element in list
      TEngineProgressInfoList::iterator pos = fProgressInfoList.begin();
      // pass it back to caller if caller is interested
      if (aInfoP) {
        *aInfoP = *pos; // copy progress event
      }
      // delete progress event from list
      fProgressInfoList.erase(pos);
      // that's it for now, engine state does not change, wait for next step
      aStepCmd = STEPCMD_PROGRESS;
      return LOCERR_OK;
    }
    else if (fPendingStepCmd != 0) {
      // now return previously generated step command
      // Note: engine is already in the new state matching fPendingStepCmd
      aStepCmd = fPendingStepCmd;
      fEngineSessionStatus = fPendingStatus;
      fPendingStepCmd=0; // none pending any more
      fPendingStatus=0;
      return fEngineSessionStatus; // return pending status now
    }
    // all progress events are delivered, now we can do the real work
  }
  #endif // NON_FULLY_GRANULAR_ENGINE
  // Now perform the actual step
  if (IS_CLIENT) {
    #ifdef SYSYNC_CLIENT
    fEngineSessionStatus = ClientSessionStep(aStepCmd,aInfoP);
    #endif // SYSYNC_CLIENT
  }
  else {
    #ifdef SYSYNC_SERVER
    fEngineSessionStatus = ServerSessionStep(aStepCmd,aInfoP);
    #endif // SYSYNC_SERVER
  }
  #ifdef NON_FULLY_GRANULAR_ENGINE
  // make sure caller issues STEPCMD_STEP to get all pending progress events
  if (fProgressInfoList.size()>0) {
    // save pending step command for returning later
    fPendingStepCmd = aStepCmd;
    fPendingStatus = fEngineSessionStatus;
    // return request for more steps instead
    aStepCmd = STEPCMD_OK;
    fEngineSessionStatus = LOCERR_OK;
  }
  #endif // NON_FULLY_GRANULAR_ENGINE
done:
  // return step status
  return fEngineSessionStatus;
} // TSyncAgent::SessionStep


#ifdef PROGRESS_EVENTS

bool TSyncAgent::HandleSessionProgressEvent(TEngineProgressInfo aProgressInfo)
{
  // handle some events specially
  if (aProgressInfo.eventtype==pev_suspendcheck)
    return !(fSuspendRequested);
  else {
    // engine progress record that needs to be queued
    // - check for message
    if (aProgressInfo.eventtype==pev_display100) {
      // this is a pointer to a string, save it separately
      // extra1 is a pointer to the message text
      // - save it for retrieval via SessionKey
      fAlertMessage = (cAppCharP)(aProgressInfo.extra1);
      // - don't pass pointer
      aProgressInfo.extra1 = 0;
    }
    // queue progress event
    fProgressInfoList.push_back(aProgressInfo);
  }
  return !(fAbortRequested);
} // TSyncAgent::HandleSessionProgressEvent

#endif // PROGRESS_EVENTS

#endif // ENGINE_LIBRARY


#ifdef SYSYNC_SERVER

// Server implementation
// ---------------------

#ifndef ENGINE_LIBRARY

// dummy server engine support to allow AsKey from plugins

#warning "using ENGINEINTERFACE_SUPPORT in old-style appbase-rooted environment. Should be converted to real engine usage later"

// Engine factory function for non-Library case
ENGINE_IF_CLASS *newServerEngine(void)
{
  // For real engine based targets, newServerEngine must create a target-specific derivate
  // of the server engine, which then has a suitable newSyncAppBase() method to create the
  // appBase. For old-style environment, a generic TServerEngineInterface is ok, as this
  // in turn calls the global newSyncAppBase() which then returns the appropriate
  // target specific appBase. Here we just return a dummy server engine base.
  return new TDummyServerEngineInterface;
} // newServerEngine

/// @brief returns a new application base.
TSyncAppBase *TDummyServerEngineInterface::newSyncAppBase(void)
{
  // For not really engine based targets, the appbase factory function is
  // a global routine (for real engine targets, it is a true virtual of
  // the engineInterface, implemented in the target's leaf engineInterface derivate.
  // - for now, use the global appBase creator routine
  return sysync::newSyncAppBase(); // use global factory function
} // TDummyServerEngineInterface::newSyncAppBase

#else // old style

// Real server engine support

/// @brief Executes next step of the session
/// @param aStepCmd[in/out] step command (STEPCMD_xxx):
///        - tells caller to send or receive data or end the session etc.
///        - instructs engine to abort or time out the session etc.
/// @param aInfoP[in] pointer to a TEngineProgressInfo structure, NULL if no progress info needed
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TSyncAgent::ServerSessionStep(uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  uInt16 stepCmdIn = aStepCmd;
  localstatus sta = LOCERR_WRONGUSAGE;

  // init default response
  aStepCmd = STEPCMD_ERROR; // error
  if (aInfoP) {
    aInfoP->eventtype=PEV_NOP;
    aInfoP->targetID=0;
    aInfoP->extra1=0;
    aInfoP->extra2=0;
    aInfoP->extra3=0;
  }

  // if session is already aborted, no more steps are required
  if (isAborted()) {
    fServerEngineState = ses_done; // we are done
  }

  // handle pre-processed step command according to current engine state
  switch (fServerEngineState) {

    // Done state
    case ses_done :
      // session done, nothing happens any more
      aStepCmd = STEPCMD_DONE;
      sta = LOCERR_OK;
      break;

    // Waiting for SyncML request data
    case ses_needdata:
      switch (stepCmdIn) {
        case STEPCMD_GOTDATA : {
          // got data, check content type
          MemPtr_t data = NULL;
          smlPeekMessageBuffer(getSmlWorkspaceID(), false, &data, &fRequestSize); // get request size
          SmlEncoding_t enc = TSyncAppBase::encodingFromData(data, fRequestSize);
          if (getEncoding()==SML_UNDEF) {
            // no encoding known so far - use what we found from looking at data
            PDEBUGPRINTFX(DBG_ERROR,(
              "Incoming data had no or invalid content type, Determined encoding by looking at data: %s",
              SyncMLEncodingNames[enc]
            ));
            setEncoding(enc);
          }
          else if (getEncoding()!=enc) {
            // already known encoding does not match actual encoding
            PDEBUGPRINTFX(DBG_ERROR,(
              "Warning: Incoming data encoding mismatch: expected=%s, found=%s",
              SyncMLEncodingNames[getEncoding()],
              SyncMLEncodingNames[enc]
            ));
          }
          if (getEncoding()==SML_UNDEF) {
            // if session encoding is still unknown at this point, reject data as non-SyncML
            PDEBUGPRINTFX(DBG_ERROR,("Incoming data is not SyncML"));
            sta = LOCERR_BADCONTENT; // bad content type
            aStepCmd = STEPCMD_ERROR;
            // Note: we do not abort the session here - app could have a retry strategy and re-enter
            //       this step with better data
            break;
          }
          // content type ok - switch to processing mode
          fServerEngineState = ses_processing;
          aStepCmd = STEPCMD_OK;
          sta = LOCERR_OK;
          break;
        }
      } // switch stepCmdIn for ses_needdata
      break;

    // Waiting until SyncML answer data is sent
    // (only when session needs to continue, otherwise we are in ses_done)
    case ses_dataready:
      switch (stepCmdIn) {
        case STEPCMD_SENTDATA :
          // sent data, now wait for next request
          fServerEngineState = ses_needdata;
          aStepCmd = STEPCMD_NEEDDATA;
          sta = LOCERR_OK;
          break;
      } // switch stepCmdIn for ses_dataready
      break;


    // Ready for generation steps
    case ses_generating:
      switch (stepCmdIn) {
        case STEPCMD_STEP :
          sta = ServerGeneratingStep(aStepCmd,aInfoP);
          break;
      } // switch stepCmdIn for ses_generating
      break;

    // Ready for processing steps
    case ses_processing:
      switch (stepCmdIn) {
        case STEPCMD_STEP :
          sta = ServerProcessingStep(aStepCmd,aInfoP);
          break;
      } // switch stepCmdIn for ses_processing
      break;

  case numServerEngineStates:
      // invalid
      break;

  } // switch fServerEngineState

  // done
  return sta;
} // TSyncAgent::ServerSessionStep




// Step that processes SyncML request data
TSyError TSyncAgent::ServerProcessingStep(uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  localstatus sta = LOCERR_WRONGUSAGE;
  InstanceID_t myInstance = getSmlWorkspaceID();
  Ret_t rc;

  // now process next command
  PDEBUGPRINTFX(DBG_EXOTIC,("Calling smlProcessData(NEXT_COMMAND)"));
  #ifdef SYDEBUG
  MemPtr_t data = NULL;
  MemSize_t datasize;
  smlPeekMessageBuffer(getSmlWorkspaceID(), false, &data, &datasize);
  #endif
  rc=smlProcessData(
    myInstance,
    SML_NEXT_COMMAND
  );
  if (rc==SML_ERR_CONTINUE) {
    // processed ok, but message not completely processed yet
    // - engine state remains as is
    aStepCmd = STEPCMD_OK; // ok w/o progress %%% for now, progress is delivered via queue in next step
    sta = LOCERR_OK;
  }
  else if (rc==SML_ERR_OK) {
    // message completely processed
    // - switch engine state to generating answer message (if any)
    aStepCmd = STEPCMD_OK;
    fServerEngineState = ses_generating;
    sta = LOCERR_OK;
  }
  else if (rc==LOCERR_RETRYMSG) {
    // server has detected that this message is a retry - report this to the app such that app can
    // first discard the instance buffer (consume everything in it)
    if (smlLockReadBuffer(myInstance,&data,&datasize)==SML_ERR_OK)
      smlUnlockReadBuffer(myInstance,datasize);
    // indicate that transport must resend the previous response
    PDEBUGPRINTFX(DBG_ERROR,(
      "Incoming message was identified as a retry - report STEPCMD_RESENDDATA - caller must resent last response"
    ));
    aStepCmd = STEPCMD_RESENDDATA;
    fServerEngineState = ses_dataready;
  }
  else {
    // processing failed
    PDEBUGPRINTFX(DBG_ERROR,("===> smlProcessData failed, returned 0x%hX",(sInt16)rc));
    // dump the message that failed to process
    #ifdef SYDEBUG
    if (data) DumpSyncMLBuffer(data,datasize,false,rc);
    #endif
    // abort the session (causing proper error events to be generated and reported back)
    AbortSession(LOCERR_PROCESSMSG, true);
    // session is now done
    fServerEngineState = ses_done;
    // step by itself is ok - let app continue stepping (to restart session or complete abort)
    aStepCmd = STEPCMD_OK;
    sta = LOCERR_OK;
  }
  // done
  return sta;
} // TSyncAgent::ServerProcessingStep



// Step that generates (rest of) SyncML answer data at end of request
TSyError TSyncAgent::ServerGeneratingStep(uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  bool done, hasdata;
  string respURI;

  // finish request
  done = EndRequest(hasdata, respURI, fRequestSize);
  // check different exit points
  if (hasdata) {
    // there is data to be sent
    aStepCmd = STEPCMD_SENDDATA;
    fServerEngineState = ses_dataready;
  }
  else {
    // no more data to send
    aStepCmd = STEPCMD_OK; // need one more step to finish
  }
  // in any case, if done, all susequent steps will return STEPCMD_DONE
  if (done) {
    // Session is done
    TerminateSession();
    // subsequent steps will all return STEPCMD_DONE
    fServerEngineState = ses_done;
  }
  // request reset
  fRequestSize = 0;

  // finished generating outgoing message
  // - make sure read pointer is set (advanced in case incoming
  //   message had trailing garbage) to beginning of generated
  //   answer. With incoming message being clean SyncML without
  //   garbage, this call is not needed, however with garbage
  //   it is important because otherwise outgoing message
  //   would have that garbage inserted before actual message
  //   start.
  smlReadOutgoingAgain(getSmlWorkspaceID());

  // return status
  return LOCERR_OK;
} // TSyncAgent::ServerGeneratingStep

#endif // ENGINE_LIBRARY

#endif // SYSYNC_SERVER


#ifdef SYSYNC_CLIENT

#ifdef ENGINE_LIBRARY

// Client implementation
// ---------------------

/// @brief Executes next step of the session
/// @param aStepCmd[in/out] step command (STEPCMD_xxx):
///        - tells caller to send or receive data or end the session etc.
///        - instructs engine to suspend or abort the session etc.
/// @param aInfoP[in] pointer to a TEngineProgressInfo structure, NULL if no progress info needed
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TSyncAgent::ClientSessionStep(uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  uInt16 stepCmdIn = aStepCmd;
  localstatus sta = LOCERR_WRONGUSAGE;

  // init default response
  aStepCmd = STEPCMD_ERROR; // error
  if (aInfoP) {
    aInfoP->eventtype=PEV_NOP;
    aInfoP->targetID=0;
    aInfoP->extra1=0;
    aInfoP->extra2=0;
    aInfoP->extra3=0;
  }

  // if session is already aborted, no more steps are required
  if (isAborted()) {
    fClientEngineState = ces_done; // we are done
  }

  // handle pre-processed step command according to current engine state
  switch (fClientEngineState) {

    // Idle state
    case ces_done : {
      // session done, nothing happens any more
      aStepCmd = STEPCMD_DONE;
      sta = LOCERR_OK;
      break;
    }

    case ces_idle : {
      // in idle, we can only start a session
      switch (stepCmdIn) {
        case STEPCMD_CLIENTSTART:
        case STEPCMD_CLIENTAUTOSTART:
          // initialize a new session
          sta = InitializeSession(fProfileSelectorInternal,stepCmdIn==STEPCMD_CLIENTAUTOSTART);
          if (sta!=LOCERR_OK) break;
          // engine is now ready, start generating first request
          fClientEngineState = ces_generating;
          // ok with no status
          aStepCmd = STEPCMD_OK;
          break;
      } // switch stepCmdIn for ces_idle
      break;
    }

    // Ready for generation steps
    case ces_generating: {
      switch (stepCmdIn) {
        case STEPCMD_STEP :
          sta = ClientGeneratingStep(aStepCmd,aInfoP);
          break;
      } // switch stepCmdIn for ces_generating
      break;
    }

    // Ready for processing steps
    case ces_processing: {
      switch (stepCmdIn) {
        case STEPCMD_STEP :
          sta = ClientProcessingStep(aStepCmd,aInfoP);
          break;
      } // switch stepCmdIn for ces_processing
      break;
    }

    // Waiting for SyncML data
    case ces_needdata: {
      switch (stepCmdIn) {
        case STEPCMD_GOTDATA : {
          // got data, now start processing it
          SESSION_PROGRESS_EVENT(this,pev_recvend,NULL,0,0,0);
          // check content type now
          MemPtr_t data = NULL;
          MemSize_t datasize;
          if (smlPeekMessageBuffer(getSmlWorkspaceID(), false, &data, &datasize) != SML_ERR_OK) {
            // SyncML TK has a problem when asked to store an empty message:
            // it then returns SML_ERR_WRONG_USAGE in smlPeekMessageBuffer and
            // leaves datasize unset. Happened after an application bug.
            //
            // Avoid undefined behavior and proceed without data (easier than
            // introducing an additional, untested error path).
            data = NULL;
            datasize = 0;
          }

          // check content type
          SmlEncoding_t enc = TSyncAppBase::encodingFromData(data, datasize);
          if (enc!=getEncoding()) {
            PDEBUGPRINTFX(DBG_ERROR,("Incoming data is not SyncML"));
            sta = LOCERR_BADCONTENT; // bad content type
            #ifdef SYDEBUG
            if (data) DumpSyncMLBuffer(data,datasize,false,SML_ERR_UNSPECIFIC);
            #endif
            // abort the session (causing proper error events to be generated and reported back)
            AbortSession(sta, true);
            // session is now done
            fClientEngineState = ces_done;
            aStepCmd = STEPCMD_ERROR;
            break;
          }
          // content type ok - switch to processing mode
          fIgnoreMsgErrs=false; // do not ignore errors by default
          fClientEngineState = ces_processing;
          aStepCmd = STEPCMD_OK;
          sta = LOCERR_OK;
          break;
        }
        case STEPCMD_RESENDDATA :
          // instead of having received new data, the network layer has found it needs to re-send the data.
          // performing the STEPCMD_RESENDDATA just generates a new send start event, but otherwise no engine action
          fClientEngineState = ces_resending;
          aStepCmd = STEPCMD_RESENDDATA; // return the same step command, to differentiate it from STEPCMD_SENDDATA
          SESSION_PROGRESS_EVENT(this,pev_sendstart,NULL,0,0,0);
          sta = LOCERR_OK;
          break;
      } // switch stepCmdIn for ces_needdata
      break;
    }

    // Waiting until SyncML data is sent
    case ces_dataready:
    case ces_resending: {
      switch (stepCmdIn) {
        case STEPCMD_SENTDATA :
          // allowed in dataready or resending state
          // sent (or re-sent) data, now request answer data
          SESSION_PROGRESS_EVENT(this,pev_sendend,NULL,0,0,0);
          fClientEngineState = ces_needdata;
          aStepCmd = STEPCMD_NEEDDATA;
          sta = LOCERR_OK;
          break;
      } // switch stepCmdIn for ces_dataready
      break;
    }

    case numClientEngineStates: {
      // invalid
      break;
    }

  } // switch fClientEngineState

  // done
  return sta;
} // TSyncAgent::ClientSessionStep



// Step that generates SyncML data
TSyError TSyncAgent::ClientGeneratingStep(uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  localstatus sta = LOCERR_WRONGUSAGE;
  bool done;

  //%%% at this time, generate next message in one step
  sta = NextMessage(done);
  if (done) {
    // done with session, with or without error
    fClientEngineState = ces_done; // blocks any further activity with the session
    aStepCmd = STEPCMD_DONE;
    // terminate session to provoke all end-of-session progress events
    TerminateSession();
  }
  else if (sta==LOCERR_OK) {
    // finished generating outgoing message
    // - make sure read pointer is set (advanced in case incoming
    //   message had trailing garbage) to beginning of generated
    //   answer. With incoming message being clean SyncML without
    //   garbage, this call is not needed, however with garbage
    //   it is important because otherwise outgoing message
    //   would have that garbage inserted before actual message
    //   start.
    smlReadOutgoingAgain(getSmlWorkspaceID());
    // next is sending request to server
    fClientEngineState = ces_dataready;
    aStepCmd = STEPCMD_SENDDATA;
    SESSION_PROGRESS_EVENT(this,pev_sendstart,NULL,0,0,0);
  }
  // return status
  return sta;
} // TSyncAgent::ClientGeneratingStep



// Step that processes SyncML data
TSyError TSyncAgent::ClientProcessingStep(uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  InstanceID_t myInstance = getSmlWorkspaceID();
  Ret_t rc;
  localstatus sta = LOCERR_WRONGUSAGE;

  // now process next command
  PDEBUGPRINTFX(DBG_EXOTIC,("Calling smlProcessData(NEXT_COMMAND)"));
  #ifdef SYDEBUG
  MemPtr_t data = NULL;
  MemSize_t datasize;
  smlPeekMessageBuffer(getSmlWorkspaceID(), false, &data, &datasize);
  #endif
  rc=smlProcessData(
    myInstance,
    SML_NEXT_COMMAND
  );
  if (rc==SML_ERR_CONTINUE) {
    // processed ok, but message not completely processed yet
    // - engine state remains as is
    aStepCmd = STEPCMD_OK; // ok w/o progress %%% for now, progress is delivered via queue in next step
    sta = LOCERR_OK;
  }
  else if (rc==SML_ERR_OK) {
    // message completely processed
    // - switch engine state to generating next message (if any)
    aStepCmd = STEPCMD_OK;
    fClientEngineState = ces_generating;
    sta = LOCERR_OK;
  }
  else {
    // processing failed
    PDEBUGPRINTFX(DBG_ERROR,("===> smlProcessData failed, returned 0x%hX",(sInt16)rc));
    // dump the message that failed to process
    #ifdef SYDEBUG
    if (data) DumpSyncMLBuffer(data,datasize,false,rc);
    #endif
    if (!fIgnoreMsgErrs) {
      // abort the session (causing proper error events to be generated and reported back)
      AbortSession(LOCERR_PROCESSMSG, true);
      // session is now done
      fClientEngineState = ces_done;
    }
    else {
      // we must ignore errors e.g. because of session restart and go back to generate next message
      fClientEngineState = ces_generating;
    }
    // anyway, step by itself is ok - let app continue stepping (to restart session or complete abort)
    aStepCmd = STEPCMD_OK;
    sta = LOCERR_OK;
  }
  // now check if this is a session restart
  if (sta==LOCERR_OK && isStarting()) {
    // this is still the beginning of a session, which means
    // that we are restarting the session and caller should close
    // possibly open communication with the server before sending the next message
    aStepCmd = STEPCMD_RESTART;
  }
  // done
  return sta;
} // TSyncAgent::ClientProcessingStep

#endif // ENGINE_LIBRARY

#endif // SYSYNC_CLIENT



// Session runtime settings key
// ---------------------------

// Constructor
TAgentParamsKey::TAgentParamsKey(TEngineInterface *aEngineInterfaceP, TSyncAgent *aAgentP) :
  inherited(aEngineInterfaceP,aAgentP),
  fAgentP(aAgentP)
{
} // TAgentParamsKey::TAgentParamsKey



// - read local session ID
static TSyError readLocalSessionID(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  return TStructFieldsKey::returnString(
    mykeyP->fAgentP->getLocalSessionID(),
    aBuffer,aBufSize,aValSize
  );
} // readLocalSessionID


// - read initial local URI
static TSyError readInitialLocalURI(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  return TStructFieldsKey::returnString(
    mykeyP->fAgentP->getInitialLocalURI(),
    aBuffer,aBufSize,aValSize
  );
} // readInitialLocalURI


// - read abort status
static TSyError readAbortStatus(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  return TStructFieldsKey::returnInt(
    mykeyP->fAgentP->getAbortReasonStatus(),
    sizeof(TSyError),
    aBuffer,aBufSize,aValSize
  );
} // readAbortStatus



// - write abort status, which means aborting a session
TSyError writeAbortStatus(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  // abort the session
  TSyError sta = *((TSyError *)aBuffer);
  mykeyP->fAgentP->AbortSession(sta, true);
  return LOCERR_OK;
} // writeAbortStatus



// - read content type string
static TSyError readContentType(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  string contentType = SYNCML_MIME_TYPE;
  mykeyP->fAgentP->addEncoding(contentType);
  return TStructFieldsKey::returnString(
    contentType.c_str(),
    aBuffer,aBufSize,aValSize
  );
} // readContentType


// - write content type string
static TSyError writeContentType(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  string contentType((cAppCharP)aBuffer,aValSize);
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  mykeyP->fAgentP->setEncoding(TSyncAppBase::encodingFromContentType(contentType.c_str()));
  return LOCERR_OK;
} // writeContentType


// - read connection URL
static TSyError readConnectURI(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  return TStructFieldsKey::returnString(
    mykeyP->fAgentP->getSendURI(),
    aBuffer,aBufSize,aValSize
  );
} // readConnectURI


// - read host part of connection URL
static TSyError readConnectHost(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  string host, port;
  splitURL(mykeyP->fAgentP->getSendURI(),NULL,&host,NULL,NULL,NULL,&port,NULL);
  // old semantic of splitURL was to include port in host string,
  // continue doing that
  if (!port.empty()) {
    host += ':';
    host += port;
  }
  return TStructFieldsKey::returnString(
    host.c_str(),
    aBuffer,aBufSize,aValSize
  );
} // readConnectHost


// - read document part of connection URL
static TSyError readConnectDoc(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  string doc, query;
  splitURL(mykeyP->fAgentP->getSendURI(),NULL,NULL,&doc,NULL,NULL,NULL,&query);
  // old semantic of splitURL was to include query in document string,
  // continue doing that
  if (!query.empty()) {
    doc += '?';
    doc += query;
  }
  return TStructFieldsKey::returnString(
    doc.c_str(),
    aBuffer,aBufSize,aValSize
  );
} // readConnectDoc


// - time when session was last used
static TSyError readLastUsed(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  // return it
  return TStructFieldsKey::returnLineartime(mykeyP->fAgentP->getSessionLastUsed(), aBuffer, aBufSize, aValSize);
} // readLastUsed


// - server only: check session timeout
static TSyError readTimedOut(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  // check if session has timed out
  bool timedout = mykeyP->fAgentP->getSessionLastUsed()+mykeyP->fAgentP->getSessionConfig()->getSessionTimeout() < mykeyP->fAgentP->getSystemNowAs(TCTX_UTC);
  // return it
  return TStructFieldsKey::returnInt(timedout, sizeof(bool), aBuffer, aBufSize, aValSize);
} // readTimedOut


// - show if this is a server (for DB plugins)
static TSyError readIsServer(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  return TStructFieldsKey::returnInt(mykeyP->fAgentP->getSyncAppBase()->isServer(), sizeof(bool), aBuffer, aBufSize, aValSize);
} // readIsServer


#ifdef SYSYNC_SERVER

// - server only: read respURI enable flag
static TSyError readSendRespURI(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  return TStructFieldsKey::returnInt(mykeyP->fAgentP->fUseRespURI, sizeof(bool), aBuffer, aBufSize, aValSize);
} // readSendRespURI


// - write respURI enable flag
static TSyError writeSendRespURI(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  mykeyP->fAgentP->fUseRespURI = *((uInt8P)aBuffer);
  return LOCERR_OK;
} // writeSendRespURI


#endif // SYSYNC_SERVER


#ifdef SYSYNC_CLIENT

// - write (volatile, write-only) password for running this session
// (for cases where we don't want to rely on binfile storage for sensitive password data)
TSyError writeSessionPassword(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  mykeyP->fAgentP->setServerPassword((cAppCharP)aBuffer, aValSize);
  return LOCERR_OK;
} // writeSessionPassword


#ifdef ENGINE_LIBRARY
// - read display alert
static TSyError readDisplayAlert(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  return TStructFieldsKey::returnString(
    mykeyP->fAgentP->fAlertMessage.c_str(),
    aBuffer,aBufSize,aValSize
  );
} // readDisplayAlert
#endif

#endif // SYSYNC_CLIENT

static TSyError readRestartSync(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  return TStructFieldsKey::returnInt(mykeyP->fAgentP->fRestartSyncOnce, sizeof(bool), aBuffer, aBufSize, aValSize);
} // readRestartsync


// - write respURI enable flag
static TSyError writeRestartSync(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  mykeyP->fAgentP->fRestartSyncOnce = *((uInt8P)aBuffer);
  return LOCERR_OK;
} // writeRestartSync


// - write error message into session log
TSyError writeErrorMsg(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  string msg;
  msg.assign((cAppCharP)aBuffer, aValSize);
  POBJDEBUGPRINTFX(mykeyP->fAgentP,DBG_ERROR,("external Error: %s",msg.c_str()));
  return LOCERR_OK;
} // writeErrorMsg


// - write debug message into session log
TSyError writeDebugMsg(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  TAgentParamsKey *mykeyP = static_cast<TAgentParamsKey *>(aStructFieldsKeyP);
  string msg;
  msg.assign((cAppCharP)aBuffer, aValSize);
  POBJDEBUGPRINTFX(mykeyP->fAgentP,DBG_HOT,("external Message: %s",msg.c_str()));
  return LOCERR_OK;
} // writeDebugMsg




// accessor table for server session key
static const TStructFieldInfo ServerParamFieldInfos[] =
{
  // valName, valType, writable, fieldOffs, valSiz
  { "localSessionID", VALTYPE_TEXT, false, 0, 0, &readLocalSessionID, NULL },
  { "initialLocalURI", VALTYPE_TEXT, false, 0, 0, &readInitialLocalURI, NULL },
  { "abortStatus", VALTYPE_INT16, true, 0, 0, &readAbortStatus, &writeAbortStatus },
  { "contenttype", VALTYPE_TEXT, true, 0, 0, &readContentType, &writeContentType },
  { "connectURI", VALTYPE_TEXT, false, 0, 0, &readConnectURI, NULL },
  { "connectHost", VALTYPE_TEXT, false, 0, 0, &readConnectHost, NULL },
  { "connectDoc", VALTYPE_TEXT, false, 0, 0, &readConnectDoc, NULL },
  { "timedout", VALTYPE_INT8, false, 0, 0, &readTimedOut, NULL },
  { "lastused", VALTYPE_TIME64, false, 0, 0, &readLastUsed, NULL },
  { "isserver", VALTYPE_INT8, false, 0, 0, &readIsServer, NULL },
  #ifdef SYSYNC_SERVER
  { "sendrespuri", VALTYPE_INT8, true, 0, 0, &readSendRespURI, &writeSendRespURI },
  #endif
  #ifdef SYSYNC_CLIENT
  { "sessionPassword", VALTYPE_TEXT, true, 0, 0, NULL, &writeSessionPassword },
  #ifdef ENGINE_LIBRARY
  { "displayalert", VALTYPE_TEXT, false, 0, 0, &readDisplayAlert, NULL },
  #endif
  #endif
  // write into debug log
  { "errorMsg", VALTYPE_TEXT, true, 0, 0, NULL, &writeErrorMsg },  
  { "debugMsg", VALTYPE_TEXT, true, 0, 0, NULL, &writeDebugMsg }, 
  // restart sync
  { "restartsync", VALTYPE_INT8, true, 0, 0, &readRestartSync, &writeRestartSync }
};

// get table describing the fields in the struct
const TStructFieldInfo *TAgentParamsKey::getFieldsTable(void)
{
  return ServerParamFieldInfos;
} // TAgentParamsKey::getFieldsTable

sInt32 TAgentParamsKey::numFields(void)
{
  return sizeof(ServerParamFieldInfos)/sizeof(TStructFieldInfo);
} // TAgentParamsKey::numFields

// get actual struct base address
uInt8P TAgentParamsKey::getStructAddr(void)
{
  // prepared for accessing fields in server session object
  return (uInt8P)fAgentP;
} // TAgentParamsKey::getStructAddr


// open subkey by name (not by path!)
TSyError TAgentParamsKey::OpenSubKeyByName(
  TSettingsKeyImpl *&aSettingsKeyP,
  cAppCharP aName, stringSize aNameSize,
  uInt16 aMode
) {
  #ifdef DBAPI_TUNNEL_SUPPORT
  if (strucmp(aName,"tunnel",aNameSize)==0) {
    // get tunnel datastore pointer
    TLocalEngineDS *ds = fAgentP->getTunnelDS();
    if (!ds) return LOCERR_WRONGUSAGE;
    // opens current session's tunnel key
    aSettingsKeyP = ds->newTunnelKey(fEngineInterfaceP);
  }
  else
  #endif
    return inherited::OpenSubKeyByName(aSettingsKeyP,aName,aNameSize,aMode);
  // opened a key
  return LOCERR_OK;
} // TAgentParamsKey::OpenSubKeyByName


#endif // ENGINEINTERFACE_SUPPORT

} // namespace sysync

// eof
