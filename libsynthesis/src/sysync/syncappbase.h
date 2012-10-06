/*
 *  TSyncAppBase
 *    Base class for SySync applications, is supposed to exist
 *    as singular object only, manages "global" things such
 *    as config reading and session dispatching
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 */

#ifndef SYNCAPPBASE_H
#define SYNCAPPBASE_H

#include "configelement.h"
#include "profiling.h"
#include "debuglogger.h"
#include "syncitemtype.h"
#include "engineinterface.h"

#include "global_progress.h"

// expat if not hardcoded config
#ifndef HARDCODED_CONFIG
# ifdef HAVE_EXPAT
#include <expat.h>
# elif defined(HAVE_SYS_XMLTOK)
#include <xmltok/xmlparse.h>
# else /* HAVE_BUILTIN_XMLTOK */
#include "xmlparse.h"
# endif
#endif


namespace sysync {


#ifdef SYSYNC_TOOL
// WBXML to XML conversion
int wbxmlConv(int argc, const char *argv[]);
#endif

// XML config doc name (can be overridden in target_options if needed)
#ifndef XMLCONFIG_DOCNAME
  #define XMLCONFIG_DOCNAME "sysync_config"
#endif
#ifndef XMLCONFIG_DOCVERSION
  #define XMLCONFIG_DOCVERSION "1.0"
#endif


// progress event posting macros
// Note: engine libraries only have session level progress events
#ifdef PROGRESS_EVENTS
  #ifndef ENGINE_LIBRARY
    #define APP_PROGRESS_EVENT(a,e,d,x,y,z) a->NotifyAppProgressEvent(e,d,x,y,z)
  #endif
  #define SESSION_PROGRESS_EVENT(s,e,d,x,y,z) s->NotifySessionProgressEvent(e,d,x,y,z)
  #define DB_PROGRESS_EVENT(d,e,x,y,z) d->getSession()->NotifySessionProgressEvent(e,d->getDSConfig(),x,y,z)
#else
  #ifndef ENGINE_LIBRARY
    #define APP_PROGRESS_EVENT(a,e,d,x,y,z) true
  #endif
  #define SESSION_PROGRESS_EVENT(s,e,d,x,y,z) true
  #define DB_PROGRESS_EVENT(s,e,x,y,z) true
#endif

// non-class print to console (#ifdef CONSOLEINFO)
extern "C" void ConsolePrintf(const char *text, ...);
extern "C" void ConsolePuts(const char *text);

// direct print to app's console, whatever that is
// NOTE: implemented in derived xxxx_app.cpp
void AppConsolePuts(const char *aText);


#ifndef HARDCODED_CONFIG
// debug option (combination) names
const sInt16 numDebugOptions = 37;
extern const char * const debugOptionNames[numDebugOptions];
extern const uInt32 debugOptionMasks[numDebugOptions];
// non-class print to report config errors
void ConferrPrintf(const char *text, ...);
#endif
// provides additional context info
void writeDebugContextInfo(FILE *logfile);

extern "C" {
  /* moved to sysync_debug.h to make them accessible by just importing it
  // non-class debug output functions
  void DebugVPrintf(const char *format, va_list args);
  void DebugPrintf(const char *text, ...);
  void DebugPuts(const char *text);
  uInt16 getDebugMask(void);
  */
  // entry point for XPT part of SyncML-Toolkit with
  // #define TRACE_TO_STDOUT
  // functionally equal to DebugPrintf
  void localOutput(const char *aFormat, va_list aArgs);
  #ifdef NOWSM
  // entry points for SML part of SyncML-Toolkit
  void smlLibPrint(const char *text, ...);
  void smlLibVprintf(const char *format, va_list va);
  #endif
}


// static routines for accessing appbase logs from UI_Call_In/DB_Callback
#ifdef SYDEBUG
extern "C" void AppBaseLogDebugPuts(void *aCallbackRef, const char *aText);
extern "C" void AppBaseLogDebugExotic(void *aCallbackRef, const char *aText);
extern "C" void AppBaseLogDebugBlock(void *aCallbackRef, const char *aTag, const char *aDesc, const char *aAttrText );
extern "C" void AppBaseLogDebugEndBlock(void *aCallbackRef, const char *aTag);
extern "C" void AppBaseLogDebugEndThread(void *aCallbackRef);
#endif


#ifndef HARDCODED_CONFIG
extern const char * const SyncMLEncodingNames[];
#endif
extern const char * const SyncMLEncodingMIMENames[];


// forward declarations
class TSyncSession;
class TSyncAppBase;

class TAgentConfig;
class TDatatypesConfig;
class TSyncDataStore;
class TCommConfig;
class TDataTypeConfig;
class TSyncItemType;
class TRootConfig;

#ifdef SCRIPT_SUPPORT
class TScriptConfig;
#endif

// prototype for dispatcher creation function
// Note: This function must be implemented in the derived TSyncAppBase
//       class' .cpp file. This allows different SySync Apps to be
//       implemented by simply including the appropriate TSyncAppBase
//       derivate. THIS FUNCTION IS NOT IMPLEMENTED in SyncAppBase.cpp!
TSyncAppBase *newSyncAppBase(void);

#ifdef ENGINEINTERFACE_SUPPORT

// only as an intermediate legacy solution we still grant appBase direct access

#ifdef DIRECT_APPBASE_GLOBALACCESS
// get access to Sync app base object, creates new one if none exists
TSyncAppBase *getSyncAppBase(void);
// get access to existing Sync app base object, NULL if none
TSyncAppBase *getExistingSyncAppBase(void);
// We also need a way to access the engineInterface if we are in old AppBase code
TEngineInterface *getEngineInterface(void);
// free Sync app base object - which means that engineInterface is deleted
void freeSyncAppBase(void);
#endif

#else

// get access to Sync app base object, creates new one if none exists
TSyncAppBase *getSyncAppBase(void);
// get access to existing Sync app base object, NULL if none
TSyncAppBase *getExistingSyncAppBase(void);
// free Sync app base object
void freeSyncAppBase(void);

#endif


// communication (transport) config object
class TCommConfig : public TConfigElement
{
private:
  typedef TConfigElement inherited;

public:
  TCommConfig(const char *aElementName, TConfigElement *aParentElementP) :
    TConfigElement(aElementName,aParentElementP) {};
  // nothing special so far
}; // TCommConfig


/* %%% this was a useless intermediate class. We now call the unilib config TAgentConfig
       (union of TClientConfig and TServerConfig)
// agent configuration (an agent is a server or a client)
class TAgentConfig : public TConfigElement
{
  typedef TConfigElement inherited;
public:
  TAgentConfig(const char *aElementName, TConfigElement *aParentElementP) :
    TConfigElement(aElementName,aParentElementP) {};
  // - MUST be called after creating config to load (or pre-load) variable parts of config
  //   such as binfile profiles. If aDoLoose==false, situations, where existing config
  //   is detected but cannot be re-used will return an error. With aDoLoose==true, config
  //   files etc. are created even if it means a loss of data.
  virtual localstatus loadVarConfig(bool aDoLoose=false) { return LOCERR_OK; }
}; // TAgentConfig
*/


// single data type configuration abstract class
// Note: derived instances are created using the global newDataTypeConfig() function
class TDataTypeConfig : public TConfigElement
{
  typedef TConfigElement inherited;
public:
  TDataTypeConfig(const char *aElementName, TConfigElement *aParentElementP) :
    TConfigElement(aElementName,aParentElementP) {};
  // properties
  // - type name string
  string fTypeName;
  // - type version string
  string fTypeVersion;
  #ifdef ZIPPED_BINDATA_SUPPORT
  // if flag is set, payload data will be
  bool fZippedBindata;
  sInt16 fZipCompressionLevel;
  #endif
  // if flag is set, we can use binary blocks within text (how this is done depends on actual type)
  bool fBinaryParts;
  // Unicode payload settings
  bool fUseUTF16; // 16-bit unicode rather than UTF-8
  bool fMSBFirst; // byte order for unicode
  // create Sync Item Type of appropriate type from config
  virtual TSyncItemType *newSyncItemType(TSyncSession *aSessionP, TSyncDataStore *aDatastoreP) = 0;
  // get a descriptor for selecting a variant of a datatype (if any), NULL=no variant with this name
  virtual TTypeVariantDescriptor getVariantDescriptor(const char *aVariantName) { return NULL; };
protected:
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void localResolve(bool aLastPass);
  #endif
  virtual void clear();
}; // TDataTypeConfig


// datatypes list
typedef std::list<TDataTypeConfig *> TDataTypesList;

// datatypes registry configuration
class TDatatypesConfig : public TConfigElement
{
  typedef TConfigElement inherited;
public:
  TDatatypesConfig(const char *aElementName, TConfigElement *aParentElementP);
  virtual ~TDatatypesConfig();
  // properties
  // - list of registered data types
  TDataTypesList fDataTypesList;
  // public methods
  TDataTypeConfig *getDataType(const char *aName);
protected:
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  // resolve is needed also for hardcoded types!
  virtual void localResolve(bool aLastPass);
public:
  virtual void clear();
}; // TDatatypesConfig



#ifdef SYDEBUG
// debug Config element
class TDebugConfig : public TConfigElement
{
  typedef TConfigElement inherited;
public:
  // create root config
  TDebugConfig(const char *aElementName, TConfigElement *aParentElementP);
  virtual void clear(void);
  // the debug info path
  string fDebugInfoPath;
  // global template for TDebugLogger options
  TDbgOptions fGlobalDbgLoggerOptions;
  TDbgOptions fSessionDbgLoggerOptions;
  // other global debug options
  // - if <>0 (and #defined SYDEBUG), debug output is generated, value is used as mask
  uInt32 fDebug;
  // if set (and #defined MSGDUMP), messages sent and received are logged;
  bool fMsgDump;
  // if set, communication will be translated to XML and logged
  bool fXMLtranslate;
  // if set (and #defined SIMMSGREAD), simulated input with "i_" prefixed incoming messages are supported
  bool fSimMsgRead;
  // if set, session debug logs are enabled by default (but can be disabled by session later)
  bool fSessionDebugLogs;
  // if set, global log file is enabled
  bool fGlobalDebugLogs;
  // if set, only one single global log file is created (instead of one for every start of the app)
  bool fSingleGlobLog;
  // if set, only one single session log file is created (instead of one for every session)
  bool fSingleSessionLog;
  // if set, ISO8601 timestamp will be added as part of the session log filename
  bool fTimedSessionLogNames;
  // if set, session logs will be embedded into global log. Note: only reliably works in unthreaded environments
  bool fLogSessionsToGlobal;
protected:
  #ifndef HARDCODED_CONFIG
  // parsing
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  // resolving (finishing)
  virtual void localResolve(bool aLastPass);
private:
  uInt32 str2DebugMask(const char **aAttributes);
}; // TDebugConfig
#endif

class TProfileConfig; // forward
class TAgentConfig; // forward

// root Config element
class TRootConfig : public TRootConfigElement
{
  typedef TRootConfigElement inherited;
public:
  // create root config
  TRootConfig(TSyncAppBase *aSyncAppBaseP);
  virtual ~TRootConfig();
  // - factory methods for main config aspects
  virtual void installCommConfig(void) = 0;
  virtual void installDatatypesConfig(void);
  virtual void installAgentConfig(void);
  // - parsing of main config aspects
  #ifndef HARDCODED_CONFIG
  virtual bool parseCommConfig(const char **aAttributes, sInt32 aLine) = 0;
  virtual bool parseDatatypesConfig(const char **aAttributes, sInt32 aLine);
  virtual bool parseAgentConfig(const char **aAttributes, sInt32 aLine);
  #endif
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  // - profile configs (MIME profiles, text profiles, etc.)
  virtual TProfileConfig *newProfileConfig(const char *aName, const char *aTypeName, TConfigElement *aParentP);
  virtual TDataTypeConfig *newDataTypeConfig(const char *aName, const char *aBaseType, TConfigElement *aParentP);
  #endif
  // - hardcoded types
  #ifdef HARDCODED_TYPE_SUPPORT
  virtual void createHardcodedTypes(TDatatypesConfig *aDatatypesConfig) = 0;
  #ifndef NO_REMOTE_RULES
  #error "%%%% warning: hardcoded type with remoterule support is not tested yet. Should work with old addProperty() calls propertyGroup is 0 by default which should disable grouping"
  #endif
  #endif
  // - hardcoded other config
  #ifdef HARDCODED_CONFIG
  // setup config elements (must be implemented in derived RootConfig class)
  virtual localstatus createHardcodedConfig(void) = 0;
  #endif
  // Clear config
  virtual void clear(void);
  // save app state (such as settings in datastore configs etc.)
  virtual void saveAppState(void);
  // MUST be called after creating config to load (or pre-load) variable parts of config
  // such as binfile profiles. If aDoLoose==false, situations, where existing config
  // is detected but cannot be re-used will return an error. With aDoLoose==true, config
  // files etc. are created even if it means a loss of data.
  virtual localstatus loadVarConfig(bool aDoLoose=false);
  // date when config has last changed
  lineartime_t fConfigDate;
  #ifndef HARDCODED_CONFIG
  // string for identifying config file in logs
  string fConfigIDString;
  #endif
  // flag to suppress sending devinf to clients that do not request it
  bool fNeverPutDevinf;
  // transport/environment config
  TCommConfig *fCommConfigP;
  // Agent (Server or Client session) config
  TAgentConfig *fAgentConfigP;
  // datatypes config
  TDatatypesConfig *fDatatypesConfigP;
  #ifdef SCRIPT_SUPPORT
  // global script definitions
  TScriptConfig *fScriptConfigP;
  #endif
  // embedded debug config
  #ifdef SYDEBUG
  TDebugConfig fDebugConfig;
  #endif
  #ifdef CUSTOMIZABLE_DEVICES_LIMIT
  // active session limit
  sInt32 fConcurrentDeviceLimit;
  #endif
  #ifdef ENGINEINTERFACE_SUPPORT
  // for engine libraries, MAN/MOD/HwV/DevTyp can be configured
  string fMan;
  string fMod;
  string fHwV;
  string fFwV;
  string fDevTyp;
  #endif
  #if defined(SYSER_REGISTRATION) && (!defined(HARDCODED_CONFIG) || defined(ENGINEINTERFACE_SUPPORT))
  // licensing via config file or engine interface is possible
  string fLicenseName;
  string fLicenseCode;
  #endif
  // SyncML encoder/decoder parameters
  uInt32 fLocalMaxMsgSize; // my own maxmsgsize
  uInt32 fLocalMaxObjSize; // my own maxobjsize, if 0, large object support is disabled
  // - System time context (usually TCTX_SYSTEM, but might be explicitly set if system TZ info is not available)
  timecontext_t fSystemTimeContext;
protected:
  #ifndef HARDCODED_CONFIG
  // parsing
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  // resolving (finishing)
  virtual void localResolve(bool aLastPass);
private:
}; // TRootConfig


} // namespace sysync

// now include rest (some of the config classes are
// required by these includes already)
#include "sysync.h"
#include "syncsession.h"

namespace sysync {


/* TSyncAppBase is a global, singular object which is instantiated ONCE
 * per application.
 */
class TSyncAppBase {
  friend class TDebugConfig;
public:
  // constructors/destructors
  TSyncAppBase();
  virtual ~TSyncAppBase();
  // request count
  sInt32 requestCount(void) { return fRequestCount; };
  sInt32 incRequestCount(void) { return ++fRequestCount; };
  // config access
  TRootConfig *getRootConfig(void) { return fConfigP; };
  // - combine URI and session ID to make a RespURI according to transport
  virtual void generateRespURI(
    string & /* aRespURI */,
    cAppCharP /* aLocalURI */,
    cAppCharP /* aSessionID */
  ) { /* nop, no RespURI */ };
  #ifdef HARDCODED_CONFIG
  // create hardcoded config (root config element knows how)
  localstatus initHardcodedConfig(void);
  #else
  // config error printing
  virtual void ConferrPuts(const char *msg);
  void ConferrPrintf(const char *text, ...);
  // config reading
  localstatus readXMLConfigStream(TXMLConfigReadFunc aReaderFunc, void *aContext);
  virtual localstatus configStatus(void) { if (!fConfigP) return LOCERR_NOCFG; return fConfigP->getFatalError(); };
  #ifdef CONSTANTXML_CONFIG
  localstatus readXMLConfigConstant(const char *aConstantXML);
  #endif
  localstatus readXMLConfigCFile(FILE *aCfgFile);
  localstatus readXMLConfigFile(cAppCharP aFilePath);
  #ifndef ENGINE_LIBRARY
  localstatus readXMLConfigStandard(const char *aConfigFileName, bool aOnlyGlobal, bool aAbsolute=false);
  #endif
  #endif // HARDCODED_CONFIG
  #ifdef APP_CAN_EXPIRE
  localstatus fAppExpiryStatus; // LOCERR_OK = ok, LOCERR_BADREG, LOCERR_TOONEW or LOCERR_EXPIRED = bad
  #endif
  #ifndef HARDCODED_CONFIG
  // access to config variables
  bool getConfigVar(cAppCharP aVarName, string &aValue);
  bool setConfigVar(cAppCharP aVarName, cAppCharP aNewValue);
  bool unsetConfigVar(cAppCharP aVarName);
  bool expandConfigVars(string &aString, sInt8 aCfgVarExp, TConfigElement *aCfgElement=NULL, cAppCharP aElementName=NULL);
  #endif
  #ifdef SYDEBUG
  // path where config came from
  string fConfigFilePath;
  // access to logging for session
  TDebugLogger *getDbgLogger(void) { return &fAppLogger; };
  uInt32 getDbgMask(void) { return fAppLogger.getMask(); };
  // factory function for debug output channel
  #ifndef NO_C_FILES
  virtual TDbgOut *newDbgOutputter(bool aGlobal) { return new TStdFileDbgOut; };
  #else
  virtual TDbgOut *newDbgOutputter(bool aGlobal) = 0;
  #endif
  #endif // SYDEBUG
  // app time zones
  GZones *getAppZones(void) { return &fAppZones; };
  // Profiling
  TP_DEFINFO(fTPInfo)
  // Handle exception happening while decoding commands for a session
  virtual Ret_t HandleDecodingException(TSyncSession *aSessionP, const char *aRoutine, exception *aExceptionP=NULL) = 0;
  // access to SyncML toolkit
  bool newSmlInstance(SmlEncoding_t aEncoding, sInt32 aWorkspaceMem, InstanceID_t &aInstanceID);
  Ret_t getSmlInstanceUserData(InstanceID_t aInstanceID, void **aUserDataPP);
  Ret_t setSmlInstanceUserData(InstanceID_t aInstanceID, void *aUserDataP);
  void freeSmlInstance(InstanceID_t aInstance);
  // determine encoding from beginning of SyncML message data
  static SmlEncoding_t encodingFromData(cAppPointer aData, memSize aDataSize);
  // determine encoding from Content-Type: header value
  static SmlEncoding_t encodingFromContentType(cAppCharP aTypeString);
  // virtual handlers for SyncML toolkit callbacks, must be separately derived for server/client cases
  // - Start/End Message: derived method in server case actually creates session
  virtual Ret_t StartMessage(
    InstanceID_t aSmlWorkspaceID, // SyncML toolkit workspace instance ID
    VoidPtr_t aUserData, // user data, should be NULL (as StartMessage is responsible for setting userdata)
    SmlSyncHdrPtr_t aContentP // SyncML tookit's decoded form of the <SyncHdr> element
  ) = 0;
  // test if message buffering is available
  virtual bool canBufferRetryAnswer(void) { return false; }; // must be set in server bases
  // non-virtual handlers, are always called with userData containing valid session pointer
  // - end of message
  Ret_t EndMessage(VoidPtr_t userData, Boolean_t aFinal);
  // - grouping commands
  Ret_t StartSync(VoidPtr_t userData, SmlSyncPtr_t aContentP);
  Ret_t EndSync(VoidPtr_t userData);
  Ret_t StartSequence(VoidPtr_t userData, SmlSequencePtr_t aContentP);
  Ret_t EndSequence(VoidPtr_t userData);
  Ret_t StartAtomic(VoidPtr_t userData, SmlAtomicPtr_t aContentP);
  Ret_t EndAtomic(VoidPtr_t userData);
  // - sync commands
  Ret_t AddCmd(VoidPtr_t userData, SmlAddPtr_t aContentP);
  Ret_t AlertCmd(VoidPtr_t userData, SmlAlertPtr_t aContentP);
  Ret_t DeleteCmd(VoidPtr_t userData, SmlDeletePtr_t aContentP);
  Ret_t GetCmd(VoidPtr_t userData, SmlGetPtr_t aContentP);
  Ret_t PutCmd(VoidPtr_t userData, SmlPutPtr_t aContentP);
  #ifdef MAP_RECEIVE
  Ret_t MapCmd(VoidPtr_t userData, SmlMapPtr_t aContentP);
  #endif
  #ifdef RESULT_RECEIVE
  Ret_t ResultsCmd(VoidPtr_t userData, SmlResultsPtr_t aContentP);
  #endif
  Ret_t StatusCmd(VoidPtr_t userData, SmlStatusPtr_t aContentP);
  Ret_t ReplaceCmd(VoidPtr_t userData, SmlReplacePtr_t aContentP);
  Ret_t CopyCmd(VoidPtr_t userData, SmlCopyPtr_t aContentP);
  Ret_t MoveCmd(VoidPtr_t userData, SmlMovePtr_t aContentP);
  // - error handling
  Ret_t HandleError(VoidPtr_t userData);
  Ret_t DummyHandler(VoidPtr_t userData, const char* msg);
  // flag to indicated deletion in progess (blocking virtuals for debug info)
  #ifdef SYSER_REGISTRATION
  // somewhat scattered within object to make reverse engineering harder
  uInt16 fRegProductCode; // updated by checkRegInfo
  uInt8 fRegLicenseType; // updated by checkRegInfo
  #endif
  // convenience version for getting time
  lineartime_t getSystemNowAs(timecontext_t aContext) { return sysync::getSystemNowAs(aContext,getAppZones()); };
protected:
  // Server or client
  bool fIsServer;
  // Application custom time zones
  GZones fAppZones;
  // Destruction flag
  bool fDeleting;
  // config
  TRootConfig *fConfigP;
  #ifndef HARDCODED_CONFIG
  // user-defined config variables
  TStringToStringMap fConfigVars;
  #endif
  // request count
  sInt32 fRequestCount; // count of requests
public:
  // this is called to control behaviour for builds that can be client OR server
  bool isServer(void) { return fIsServer; };
  #ifdef SYSER_REGISTRATION
  // somewhat scattered within object to make reverse engineering harder
  bool fRegOK; // updated by checkRegInfo, used to disable hard-coded-expiry
  #endif
  #if defined(PROGRESS_EVENTS) && !defined(ENGINE_LIBRARY)
  // callback for progress events
  TProgressEventFunc fProgressEventFunc;
  void *fProgressEventContext;
  // event generator
  bool NotifyAppProgressEvent(
    TProgressEventType aEventType,
    TLocalDSConfig *aDatastoreID=NULL,
    sInt32 aExtra1=0,
    sInt32 aExtra2=0,
    sInt32 aExtra3=0
  );
  #endif // non-engine progress events
  #ifdef ENGINEINTERFACE_SUPPORT
  // owning engineInterface
  TEngineInterface *fEngineInterfaceP;
  #else
  // "master" pointer, to allow callbacks to refer to "master" object which creates appbase
  void *fMasterPointer;
  #endif
  // save app state (such as settings in datastore configs etc.)
  virtual void saveAppState(void);
public:
  // - inter-module context (needed for pooling global per-process ressources like Java VM)
  CContext fApiInterModuleContext;
  #ifdef SYSER_REGISTRATION
  // somewhat scattered within object to make reverse engineering harder
  uInt8 fRegProductFlags; // updated by checkRegInfo
  uInt16 fRegQuantity; // updated by checkRegInfo
  sInt16 fDaysLeft; // updated by appEnableStatus() and/or checkRegInfo(). 0=expired, -1=not limited
  #endif
  // identification
  // - identification of the application (constant for monolithic builds, configurable for engine library)
  string getManufacturer();
  string getModel();
  string getHardwareVersion();
  string getFirmwareVersion();
  // - device type, only used for clients
  string getDevTyp();
  // - device ID, can be customized via "customdeviceid" config var
  bool getMyDeviceID(string &devid);
  // - hardwired information (cannot change, always identifying Synthesis engine and its version
  cAppCharP getOEM(void) { return SYSYNC_OEM; } // hardwired, not configurable in target options
  cAppCharP getSoftwareVersion(void) { return SYSYNC_FULL_VERSION_STRING; } // hardwired to real version number
  // expiry checking
  #ifdef EXPIRES_AFTER_DATE
  sInt32 fScrambledNow; // scrambled now, set at syncappbase creation
  #endif
  #if defined(APP_CAN_EXPIRE) || defined(SYSER_REGISTRATION)
  // check if app is enabled
  localstatus appEnableStatus(void);
  // get registration information to display
  localstatus getAppEnableInfo(sInt16 &aDaysLeft, string *aRegnameP=NULL, string *aRegInternalsP=NULL);
  #else
  localstatus appEnableStatus(void) { return LOCERR_OK; } // app is always enabled
  //#warning "WARNING: non-expiring app - is this intended?"
  #endif
  #ifdef APP_CAN_EXPIRE
  // updates fAppExpiryStatus according to registration and hardcoded expiry date
  void updateAppExpiry(void);
  #endif
  #ifdef SYSER_REGISTRATION
  // updated by checkRegInfo
  uInt8 fRegDuration;
  uInt8 fRelDuration;
  uInt32 fLicCRC;
  #if defined(EXPIRES_AFTER_DAYS) && defined(ENGINEINTERFACE_SUPPORT)
  lineardate_t fFirstUseDate;
  uInt32 fFirstUseVers;
  #endif
  // checks if registered (must be implemented in base class)
  // returns LOCERR_EXPIRED, LOCERR_TOONEW or LOCERR_BADREG if not registered correctly
  virtual localstatus isRegistered(void);
  // checks and saves registration info passed. Returns true if registration is ok
  // (note: derived class must implement saving)
  virtual localstatus checkAndSaveRegInfo(const char *aRegKey, const char *aRegCode) { return LOCERR_BADREG; } /* no implementation */
  // checks registration code for CRC ok and compare with
  // predefined constants for product code etc.
  virtual localstatus checkRegInfo(const char *aRegKey, const char *aRegCode, bool aMangled=false);
  // extended license type depending check (such as activation)
  virtual bool checkLicenseType(uInt16 aLicenseType) { return aLicenseType==0; }; // defaults to ok for type==0
  // checks if current license is properly activated
  localstatus checkLicenseActivation(lineardate_t &aLastcheck, uInt32 &aLastcrc);
  // scan for license restrictions
  static const char * getLicenseRestriction(const char *aInfo, string &aID, string &aVal);
  // get registration string
  virtual void getRegString(string &aString);
  #endif
  // - check for Feature enabled
  virtual bool isFeatureEnabled(uInt16 aFeatureNo) { return false; /* none enabled by default */ };
  #ifdef CONCURRENT_DEVICES_LIMIT
  // check if session count is exceeded
  void checkSessionCount(sInt32 aSessionCount, TSyncSession *aSessionP);
  #endif
  #ifdef EXPIRES_AFTER_DAYS
  // gets information of first use for a given variant of the software.
  virtual void getFirstUseInfo(uInt8 aVariant, lineardate_t &aFirstUseDate, uInt32 &aFirstUseVers);
  // update first use info to allow for repeated eval when user installs an all-new version
  bool updateFirstUseInfo(lineardate_t &aFirstUseDate, uInt32 &aFirstUseVers);
  #endif
private:
  // to be executed after reading config stream or hard-coded config
  localstatus finishConfig();

  // debug logging
  #ifdef SYDEBUG
  TDebugLogger fAppLogger; // the logger
  #endif
}; // TSyncAppBase


} // namespace sysync

#endif // SYNCAPPBASE_H


// eof
