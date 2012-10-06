/*
 *  TSyncAppBase
 *    Base class for SySync applications, is supposed to exist
 *    as singular object only, manages "global" things such
 *    as config reading and session dispatching
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 *  2002-03-06 : luz : Created
 */

#include "prefix_file.h"

#include "sysync.h"
#include "syncappbase.h"
#include "scriptcontext.h"
#include "iso8601.h"
#include "syncagent.h"
#include "multifielditem.h" // in case we have no scripts...

#ifdef SYSER_REGISTRATION
#include "syserial.h"
#endif

#ifdef DIRECT_APPBASE_GLOBALACCESS
#include "sysync_glob_vars.h"
#endif



#include "global_progress.h" // globally accessible progress event posting

#ifndef ENGINE_LIBRARY

// can be called globally to post progress events
extern "C" int GlobalNotifyProgressEvent (
  TProgressEventType aEventType,
  sInt32 aExtra1,
  sInt32 aExtra2,
  sInt32 aExtra3
)
{
  #ifdef PROGRESS_EVENTS
  TSyncAppBase *baseP = getExistingSyncAppBase();
  if (baseP) {
    return baseP->NotifyAppProgressEvent(aEventType,NULL,aExtra1,aExtra2,aExtra3);
  }
  #endif
  return true; // not aborted
} // GlobalNotifyProgressEvent

#endif // not ENGINE_LIBRARY


namespace sysync {

#ifndef HARDCODED_CONFIG

// SyncML encoding names for end user config
const char * const SyncMLEncodingNames[numSyncMLEncodings] = {
  "undefined",
  "wbxml",
  "xml"
};

#endif

// SyncML encoding names for MIME type
const char * const SyncMLEncodingMIMENames[numSyncMLEncodings] = {
  "undefined",
  SYNCML_ENCODING_WBXML,
  SYNCML_ENCODING_XML
};


#if !defined(HARDCODED_CONFIG) || defined(ENGINEINTERFACE_SUPPORT)

// platform string names for accessing them as config variables
const char * const PlatformStringNames[numPlatformStrings] = {
  "platformvers", // version string of the current platform
  "globcfg_path", // global system-wide config path (such as C:\Windows or /etc)
  "loccfg_path", // local config path (such as exedir or user's dir)
  "defout_path", // default path to writable directory to write logs and other output by default
  "temp_path", // path where we can write temp files
  "exedir_path", // path to directory where executable resides
  "userdir_path", // path to the user's home directory for user-visible documents and files
  "appdata_path", // path to the user's preference directory for this application
  "prefs_path", // path to directory where all application prefs reside (not just mine)
  "device_uri", // URI of the device (as from getDeviceInfo)
  "device_name", // Name of the device (as from getDeviceInfo)
  "user_name", // Name of the currently logged in user
};

#endif


#ifdef ENGINEINTERFACE_SUPPORT

  #ifdef DIRECT_APPBASE_GLOBALACCESS
  // Only for old-style targets that still use a global anchor

  #ifdef ENGINE_LIBRARY
    #error "Engine Library may NOT access appBase via global anchor any more!"
  #endif


  // With enginemodulebase support, the global anchor is the enginebase module,
  // not syncappbase itself.

  #define GET_SYNCAPPBASE (((TEngineInterface *)sysync_glob_anchor())->getSyncAppBase())

  // get access to existing Sync app base object, NULL if none (i.e. if no TEngineInterface)
  TSyncAppBase *getExistingSyncAppBase(void)
  {
    TEngineInterface *eng = (TEngineInterface *)sysync_glob_anchor();
    return eng ? eng->getSyncAppBase() : NULL;
  } // getExistingSyncAppBase

  // global function, returns pointer to (singular) app base
  // object. With EngineInterface, this is ALWAYS a member of
  // TEngineInterface, so we create the global engineinterface if we don't have it
  // already.
  TSyncAppBase *getSyncAppBase(void)
  {
    TSyncAppBase *appBase = getExistingSyncAppBase();
    if (appBase) return appBase;
    // no appBase yet, we must create and anchor the TEngineInterface
    #ifdef SYSYNC_CLIENT
    ENGINE_IF_CLASS *engine = sysync::newClientEngine();
    #else
    ENGINE_IF_CLASS *engine = sysync::newServerEngine();
    #endif
    // we must init the engine to trigger creation of the appbase!
    if (engine) engine->Init();
    return GET_SYNCAPPBASE;
  } // getSyncAppBase

  TEngineInterface *getEngineInterface(void)
  {
    return (TEngineInterface *)sysync_glob_anchor();
  } // getEngineInterface

  // free Sync Session Dispatcher
  void freeSyncAppBase(void)
  {
    TEngineInterface *eng = getEngineInterface();
    if (eng) {
      // kills interface, and also kills syncappbase in the process
      delete eng;
    }
  }

  #endif // DIRECT_APPBASE_GLOBALACCESS


#else // ENGINEINTERFACE_SUPPORT

  // Old style without enginemodulebase: the global anchor is syncappbase

  #define GET_SYNCAPPBASE ((TSyncAppBase *)sysync_glob_anchor())


  // global function, returns pointer to (singular) app base
  // object. If not yet existing, a new app base is created
  TSyncAppBase *getSyncAppBase(void)
  {
    if (!sysync_glob_anchor()) {
      // no dispatcher exists, create new one
      // (using function which will create derived app
      // specific dispatcher)
      sysync_glob_setanchor(newSyncAppBase());
    }
    // dispatcher now exists, use it
    return GET_SYNCAPPBASE;
  } // getSyncAppBase


  // get access to existing Sync app base object, NULL if none
  TSyncAppBase *getExistingSyncAppBase(void)
  {
    return GET_SYNCAPPBASE;
  } // getExistingSyncAppBase


  // free Sync Session Dispatcher
  void freeSyncAppBase(void)
  {
    if (sysync_glob_anchor()) {
      // object exists, kill it now
      delete GET_SYNCAPPBASE;
      sysync_glob_setanchor(NULL);
    }
  }

#endif // not ENGINEINTERFACE_SUPPORT


#ifdef SYDEBUG

// static routines for accessing appbase logs from UI_Call_In/DB_Callback

extern "C" void AppBaseLogDebugPuts(void *aCallbackRef, const char *aText)
{
  if (aCallbackRef) {
    POBJDEBUGPUTSX(static_cast<TSyncAppBase *>(aCallbackRef),DBG_DBAPI+DBG_PLUGIN,aText);
  }
} // AppBaseLogDebugPuts


extern "C" void AppBaseLogDebugExotic(void *aCallbackRef, const char *aText)
{
  if (aCallbackRef) {
    POBJDEBUGPUTSX(static_cast<TSyncAppBase *>(aCallbackRef),DBG_DBAPI+DBG_PLUGIN+DBG_EXOTIC,aText);
  }
} // AppBaseLogDebugExotic


extern "C" void AppBaseLogDebugBlock(void *aCallbackRef, const char *aTag, const char *aDesc, const char *aAttrText )
{
  if (aCallbackRef) {
    bool collapsed=false;
    if (aTag && aTag[0]=='-') { aTag++; collapsed=true; }
    static_cast<TSyncAppBase *>(aCallbackRef)->getDbgLogger()->DebugOpenBlock(TDBG_LOCATION_NONE aTag,aDesc,collapsed,"%s",aAttrText);
  }
} // AppBaseLogDebugBlock


extern "C" void AppBaseLogDebugEndBlock(void *aCallbackRef, const char *aTag)
{
  if (aCallbackRef) {
    if (aTag && aTag[0]=='-') aTag++;
    static_cast<TSyncAppBase *>(aCallbackRef)->getDbgLogger()->DebugCloseBlock(TDBG_LOCATION_NONE aTag);
  }
} // AppBaseLogDebugEndBlock


extern "C" void AppBaseLogDebugEndThread(void *aCallbackRef)
{
  if (aCallbackRef) {
    static_cast<TSyncAppBase *>(aCallbackRef)->getDbgLogger()->DebugThreadOutputDone(true); // remove thread record for global threads
  }
} // AppBaseLogDebugEndThread

#endif


// root config constructor
TRootConfig::TRootConfig(TSyncAppBase *aSyncAppBaseP) :
  TRootConfigElement(aSyncAppBaseP),
  fCommConfigP(NULL),
  fAgentConfigP(NULL),
  fDatatypesConfigP(NULL)
  #ifdef SCRIPT_SUPPORT
  , fScriptConfigP(NULL)
  #endif
  #ifdef SYDEBUG
  , fDebugConfig("debug",this) // init static debug config member
  #endif
{
  // config date is unknown so far
  fConfigDate=0;
  #ifndef HARDCODED_CONFIG
  // string for identifying config file in logs
  fConfigIDString="<none>";
  #endif
} // TRootConfig::TRootConfig


TRootConfig::~TRootConfig()
{
  // clear linked
  if (fAgentConfigP) delete fAgentConfigP;
  if (fDatatypesConfigP) delete fDatatypesConfigP;
  if (fCommConfigP) delete fCommConfigP;
  #ifdef SCRIPT_SUPPORT
  if (fScriptConfigP) delete fScriptConfigP;
  #endif
} // TRootConfig::~TRootConfig


void TRootConfig::clear(void)
{
  // root config variables
  // - init PUT suppression
  fNeverPutDevinf=false;
  // - init message size
  fLocalMaxMsgSize=DEFAULT_MAXMSGSIZE;
  fLocalMaxObjSize=DEFAULT_MAXOBJSIZE;
  // - system time zone
  fSystemTimeContext=TCTX_SYSTEM; // default to automatic detection
  #ifdef ENGINEINTERFACE_SUPPORT
  // - default identification
  fMan.clear();
  fMod.clear();
  #endif
  // - init device limit
  #ifdef CUSTOMIZABLE_DEVICES_LIMIT
  fConcurrentDeviceLimit=CONCURRENT_DEVICES_LIMIT;
  #endif
  // remove, (re-)create and clear linked config branches (debug last to allow dbg output while clearing)
  // - global scripting config
  #ifdef SCRIPT_SUPPORT
  if (fScriptConfigP) delete fScriptConfigP;
  fScriptConfigP= new TScriptConfig(this);
  if (fScriptConfigP) fScriptConfigP->clear();
  #endif
  // - communication config
  if (fCommConfigP) delete fCommConfigP;
  installCommConfig();
  if (fCommConfigP) fCommConfigP->clear();
  // - datatypes registry config
  if (fDatatypesConfigP) delete fDatatypesConfigP;
  installDatatypesConfig();
  if (fDatatypesConfigP) fDatatypesConfigP->clear();
  // - agent config
  if (fAgentConfigP) delete fAgentConfigP;
  installAgentConfig();
  if (fAgentConfigP) fAgentConfigP->clear();
  // clear embedded debug config (as the last action)
  #ifdef SYDEBUG
  fDebugConfig.clear();
  #endif
  // clear inherited
  inherited::clear();
} // TRootConfig::clear


// save app state (such as settings in datastore configs etc.)
void TRootConfig::saveAppState(void)
{
  if (fAgentConfigP) fAgentConfigP->saveAppState();
  if (fDatatypesConfigP) fDatatypesConfigP->saveAppState();
  if (fCommConfigP) fCommConfigP->saveAppState();
} // TRootConfig::saveAppState


// MUST be called after creating config to load (or pre-load) variable parts of config
// such as binfile profiles. If aDoLoose==false, situations, where existing config
// is detected but cannot be re-used will return an error. With aDoLoose==true, config
// files etc. are created even if it means a loss of data.
localstatus TRootConfig::loadVarConfig(bool aDoLoose)
{
  // only agent may load variable config
  if (fAgentConfigP)
    return fAgentConfigP->loadVarConfig(aDoLoose);
  else
    return LOCERR_NOCFG; // no config yet
} // TRootConfig::loadVarConfig




#ifndef HARDCODED_CONFIG

// root config element parsing
bool TRootConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // debug
  if (strucmp(aElementName,"debug")==0) {
    #ifdef SYDEBUG
    // let static TDebugConfig member handle it
    expectChildParsing(fDebugConfig);
    #else
    ReportError(false,"No debugging features available in this version");
    expectAll(); // no debug code, simply ignore settings
    #endif
  }
  // config ID/date
  else if (strucmp(aElementName,"configdate")==0)
    expectTimestamp(fConfigDate);
  else if (strucmp(aElementName,"configidstring")==0)
    expectMacroString(fConfigIDString);
  else
  #ifdef ENGINEINTERFACE_SUPPORT
  // - product identification (in devInf)
  if (strucmp(aElementName,"manufacturer")==0)
    expectMacroString(fMan);
  else if (strucmp(aElementName,"model")==0)
    expectMacroString(fMod);
  else if (strucmp(aElementName,"HardwareVersion")==0)
    expectMacroString(fHwV);
  else if (strucmp(aElementName,"FirmwareVersion")==0)
    expectMacroString(fFwV);
  else if (strucmp(aElementName,"DeviceType")==0)
    expectMacroString(fDevTyp);
  else
  #endif
  // config variables
  if (strucmp(aElementName,"configvar")==0) {
    const char *nam = getAttr(aAttributes,"name");
    if (!nam)
      return fail("Missing 'name' attribute in 'configvar'");
    string val;
    if (!getAttrExpanded(aAttributes,"value",val,false))
      return fail("Missing 'value' attribute in 'configvar'");
    getSyncAppBase()->setConfigVar(nam,val.c_str());
    expectEmpty();
  }
  // options
  else if (strucmp(aElementName,"neverputdevinf")==0)
    expectBool(fNeverPutDevinf);
  else if (strucmp(aElementName,"maxmsgsize")==0)
    expectUInt32(fLocalMaxMsgSize);
  else if (strucmp(aElementName,"maxobjsize")==0)
    expectUInt32(fLocalMaxObjSize);
  else if (strucmp(aElementName,"maxconcurrentsessions")==0) {
    #ifdef CUSTOMIZABLE_DEVICES_LIMIT
    expectInt32(fConcurrentDeviceLimit);
    #else
    expectAll(); // no configurable limit, simply ignore contents
    #endif
  }
  // time zones
  else if (strucmp(aElementName,"definetimezone")==0)
    expectVTimezone(getSyncAppBase()->getAppZones()); // definition of custom time zone
  else if (strucmp(aElementName,"systemtimezone")==0)
    expectTimezone(fSystemTimeContext);
  // license
  else if (strucmp(aElementName,"licensename")==0) {
    #ifdef SYSER_REGISTRATION
    expectString(fLicenseName);
    #else
    expectAll(); // simply ignore contents for versions w/o registration
    #endif
  }
  else if (strucmp(aElementName,"licensecode")==0) {
    #ifdef SYSER_REGISTRATION
    expectString(fLicenseCode);
    #else
    expectAll(); // simply ignore contents for versions w/o registration
    #endif
  }
  #ifdef SCRIPT_SUPPORT
  else if (strucmp(aElementName,"scripting")==0) {
    // let linked TScriptConfig handle it
    expectChildParsing(*fScriptConfigP);
  }
  #endif
  else
  // Agent: Server or client
  if (
    (IS_CLIENT && strucmp(aElementName,"client")==0) ||
    (IS_SERVER && strucmp(aElementName,"server")==0)
  ) {
    // Agent config
    if (!fAgentConfigP) return false;
    return parseAgentConfig(aAttributes, aLine);
  }
  // Transport
  else if (strucmp(aElementName,"transport")==0) {
    // transport config
    if (!fCommConfigP) return false;
    if (!parseCommConfig(aAttributes, aLine)) {
      // not a transport we can understand, simply ignore
      expectAll();
    }
  }
  else if (strucmp(aElementName,"datatypes")==0) {
    // datatypes (type registry) config
    if (!fDatatypesConfigP) return false;
    return parseDatatypesConfig(aAttributes, aLine);
  }
  else {
    // invalid element
    return false;
  }
  // ok
  return true;
} // TRootConfig::localStartElement
#endif


// resolve (finish after all data is parsed)
void TRootConfig::localResolve(bool aLastPass)
{
  // make sure static debug element is resolved so
  // possible debug information created by resolving other
  // elements go to the correct locations/files
  // Note: in XML configs, the debug element is resolved immediately
  //       after parsing (has fResolveImmediately set) and will
  //       not be re-resolved here unless the element was not parsed at all.
  #if defined(SYDEBUG) && !defined(SYSYNC_TOOL)
  // - for SysyTool, do not resolve here as we don't want to see all of the
  //   DBG blurb on the screen created by resolving the config
  fDebugConfig.Resolve(aLastPass);
  #endif
  // set zone for system if one was defined explicitly
  if (!TCTX_IS_SYSTEM(fSystemTimeContext) && !TCTX_IS_UNKNOWN(fSystemTimeContext)) {
    getSyncAppBase()->getAppZones()->predefinedSysTZ = fSystemTimeContext;
    getSyncAppBase()->getAppZones()->ResetCache(); // make sure next query for SYSTEM tz will get new set zone
  }

  // MaxMessagesize must have a reasonable size
  if (fLocalMaxMsgSize<512) {
    SYSYNC_THROW(TConfigParseException("<maxmsgsize> must be at least 512 bytes"));
  }
  // make sure we have the registration info vars updated
  #ifdef APP_CAN_EXPIRE
  getSyncAppBase()->updateAppExpiry();
  #elif defined(SYSER_REGISTRATION)
  getSyncAppBase()->isRegistered();
  #endif
  // make sure linked elements are resolved
  #ifdef SCRIPT_SUPPORT
  if (fScriptConfigP) fScriptConfigP->Resolve(aLastPass);
  #endif
  if (fAgentConfigP) fAgentConfigP->Resolve(aLastPass);
  if (fDatatypesConfigP) fDatatypesConfigP->Resolve(aLastPass);
  if (fCommConfigP) fCommConfigP->Resolve(aLastPass);
  // finally, get rid of macros (all scripts are now read)
  #ifdef SCRIPT_SUPPORT
  if (fScriptConfigP && aLastPass) fScriptConfigP->clearmacros();
  #endif
  #if defined(SYDEBUG) && defined(SYSYNC_TOOL)
  // - for SysyTool, resolve now, where resolving dbg output has gone /dev/null already
  fDebugConfig.Resolve(aLastPass);
  #endif
  // try to resolve variable config here, but without forcing new one
  if (aLastPass) {
    loadVarConfig(false);
  }
} // TRootConfig::localResolve


// Base datatype config

// init defaults
void TDataTypeConfig::clear(void)
{
  // clear properties
  fTypeName.erase(); // no type
  fTypeVersion.erase(); // no version
  #ifdef ZIPPED_BINDATA_SUPPORT
  fZippedBindata=false;
  fZipCompressionLevel=-1; // valid range is 0-9, invalid value will select Z_DEFAULT_COMPRESSION
  #endif
  fBinaryParts=false; // no binary parts
  fUseUTF16=false; // no UTF-16/Unicode translation
  fMSBFirst=false; // default to Intel byte order for UTF16
  // clear inherited
  inherited::clear();
} // TMIMEDirTypeConfig::clear


#ifdef CONFIGURABLE_TYPE_SUPPORT

// config element parsing
bool TDataTypeConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"typestring")==0)
    expectString(fTypeName);
  else if (strucmp(aElementName,"versionstring")==0)
    expectString(fTypeVersion);
  else if (strucmp(aElementName,"binaryparts")==0)
    expectBool(fBinaryParts);
  #ifdef ZIPPED_BINDATA_SUPPORT
  else if (strucmp(aElementName,"zippedbindata")==0)
    expectBool(fZippedBindata);
  else if (strucmp(aElementName,"zipcompressionlevel")==0)
    expectInt16(fZipCompressionLevel);
  #endif
  else if (strucmp(aElementName,"unicodedata")==0)
    expectBool(fUseUTF16);
  else if (strucmp(aElementName,"bigendian")==0)
    expectBool(fMSBFirst);
  // - none known here
  else
    return false; // base class is TConfigElement
  // ok
  return true;
} // TDataTypeConfig::localStartElement


// resolve
void TDataTypeConfig::localResolve(bool aLastPass)
{
  // check
  if (aLastPass) {
    // Note: type strings might be set explicitly or implicitly by derived classes
    // Note2: all types must have a version (SCTS will fail without). For example
    //        Starfish's text/plain notes have Version "1.0"
    if (fTypeName.empty() || fTypeVersion.empty() )
      SYSYNC_THROW(TConfigParseException("datatype must have non-empty 'typestring' and 'versionstring'"));
  }
} // TDataTypeConfig::localResolve

#endif



// Datatype registry

TDatatypesConfig::TDatatypesConfig(const char* aName, TConfigElement *aParentElement) :
  TConfigElement(aName,aParentElement)
{
  clear();
} // TDatatypesConfig::TDatatypesConfig


TDatatypesConfig::~TDatatypesConfig()
{
  clear();
} // TDatatypesConfig::~TDatatypesConfig


// init defaults
void TDatatypesConfig::clear(void)
{
  // remove datatypes
  TDataTypesList::iterator pos;
  for(pos=fDataTypesList.begin();pos!=fDataTypesList.end();pos++)
    delete *pos;
  fDataTypesList.clear();
  // clear inherited
  inherited::clear();
} // TDatatypesConfig::clear


TDataTypeConfig *TDatatypesConfig::getDataType(const char *aName)
{
  TDataTypesList::iterator pos;
  for(pos=fDataTypesList.begin();pos!=fDataTypesList.end();pos++) {
    if (strucmp((*pos)->getName(),aName)==0) {
      // found
      return *pos;
    }
  }
  return NULL; // not found
} // TDatatypesConfig::getDataType


#ifdef CONFIGURABLE_TYPE_SUPPORT

// config element parsing
bool TDatatypesConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"datatype")==0) {
    // definition of a new data type
    // - get name
    const char *nam = getAttr(aAttributes,"name");
    if (!nam)
      return fail("Missing 'name' attribute in 'datatype'");
    // - get basetype
    const char *basetype = getAttr(aAttributes,"basetype");
    if (!basetype)
      return fail("Missing 'basetype' attribute in 'datatype'");
    // - call global function to get correct TDataTypeConfig derived object
    TDataTypeConfig *datatypeP = getSyncAppBase()->getRootConfig()->newDataTypeConfig(nam,basetype,this);
    if (!datatypeP)
      return fail("Unknown basetype '%s'",basetype);
    // - save in list
    fDataTypesList.push_back(datatypeP);
    // - let element handle parsing
    expectChildParsing(*datatypeP);
  }
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TDatatypesConfig::localStartElement

#endif


// resolve
void TDatatypesConfig::localResolve(bool aLastPass)
{
  // resolve all types in list
  TDataTypesList::iterator pos;
  for(pos=fDataTypesList.begin();pos!=fDataTypesList.end();pos++)
    (*pos)->Resolve(aLastPass);
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TDatatypesConfig::localResolve



#ifdef SYDEBUG

// debug config constructor
TDebugConfig::TDebugConfig(const char *aElementName, TConfigElement *aParentElementP) :
  TConfigElement(aElementName,aParentElementP)
{
  // do not call clear(), because this is virtual!
} // TDebugConfig::TDebugConfig


void TDebugConfig::clear(void)
{
  // set defaults
  fGlobalDbgLoggerOptions.clear(); // set logger options to defaults
  fSessionDbgLoggerOptions.clear(); // set logger options to defaults
  fDebug = DEFAULT_DEBUG; // if <>0 (and #defined SYDEBUG), debug output is generated, value is used as mask
  fMsgDump = DEFAULT_MSGDUMP; // if set (and #defined MSGDUMP), messages sent and received are logged;
  fSingleGlobLog = false; // create a separate global log per app start
  fSingleSessionLog = false; // create separate session logs
  fTimedSessionLogNames = true; // add session start time into file name
  fLogSessionsToGlobal = false; // use separate file(s) for session log
  fXMLtranslate = DEFAULT_XMLTRANSLATE; // if set, communication will be translated to XML and logged
  fSimMsgRead = DEFAULT_SIMMSGREAD; // if set (and #defined SIMMSGREAD), simulated input with "i_" prefixed incoming messages are supported
  fGlobalDebugLogs = DEFAULT_GLOBALDEBUGLOGS;
  fSessionDebugLogs = DEFAULT_SESSIONDEBUGLOGS;
  if (getPlatformString(pfs_defout_path,fDebugInfoPath))
    makeOSDirPath(fDebugInfoPath);
  else
    fDebugInfoPath.erase(); // none
  // make sure that defaults are applied a first time NOW, BEFORE reading first config
  localResolve(true);
  #ifndef SYSYNC_TOOL
  // and make sure final resolve takes place early when <debug> element finishes parsing
  // (but not for SYSYNC_TOOL, where we want no debug output at all during config read & resolve!)
  fResolveImmediately = true;
  #endif
  // clear inherited
  inherited::clear();
} // TDebugConfig::clear


// resolve (finish after all data is parsed)
void TDebugConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    #ifdef SYDEBUG
    // we have debug
    #ifndef HARDCODED_CONFIG
      // XML config - user options settings are parsed into fSessionDbgLoggerOptions
      // - by default, global logging has same options as configured for session...
      fGlobalDbgLoggerOptions = fSessionDbgLoggerOptions;
      if (fLogSessionsToGlobal) {
        // ... and these settings are used as they are, if session and global
        // logging are identical...
      } else {
        // ...but when they are different, we have a few hard-coded things for global logging:
      #ifdef MULTITHREAD_PIPESERVER
        // - for pipe server, global logs should be per-thread
        fGlobalDbgLoggerOptions.fFlushMode=dbgflush_flush; // flush every log line
        fGlobalDbgLoggerOptions.fSubThreadMode=dbgsubthread_separate; // separate per thread
        fGlobalDbgLoggerOptions.fTimestampForAll=true; // timestamp for every message
      #else
        if (IS_SERVER) {
          // - global log for ISAPI/XPT *Servers* is always in openclose mode (possibly multiple processes accessing it)
          fGlobalDbgLoggerOptions.fFlushMode=dbgflush_openclose; // open and close for every log line
        }
        fGlobalDbgLoggerOptions.fSubThreadMode=dbgsubthread_linemix; // mix in one file
        #ifdef MULTI_THREAD_SUPPORT
          fGlobalDbgLoggerOptions.fThreadIDForAll=true; // thread ID for each message
        #endif
      #endif
      }
    #endif
    // initialize global debug logging options
    getSyncAppBase()->fAppLogger.setMask(fDebug); // set initial debug mask from config
    getSyncAppBase()->fAppLogger.setEnabled(fGlobalDebugLogs); // init from config
    getSyncAppBase()->fAppLogger.setOptions(&fGlobalDbgLoggerOptions);
    // install outputter, but only if not yet installed in an earlier invocation. We only want ONE log per engine instantiation!
    if (!getSyncAppBase()->fAppLogger.outputEstablished()) {
      getSyncAppBase()->fAppLogger.installOutput(getSyncAppBase()->newDbgOutputter(true)); // install the output object (and pass ownership!)
      getSyncAppBase()->fAppLogger.setDebugPath(fDebugInfoPath.c_str()); // global log all in one file
      getSyncAppBase()->fAppLogger.appendToDebugPath(fGlobalDbgLoggerOptions.fBasename.empty() ?
                                                     TARGETID :
                                                     fGlobalDbgLoggerOptions.fBasename.c_str());
      if (fSingleGlobLog) {
        // One single log - in this case, we MUST append to current log
        fGlobalDbgLoggerOptions.fAppend=true;
      }
      else {
        // create a new global log for each app start
        getSyncAppBase()->fAppLogger.appendToDebugPath("_");
        string t;
        TimestampToISO8601Str(t, getSyncAppBase()->getSystemNowAs(TCTX_UTC), TCTX_UTC, false, false);
        getSyncAppBase()->fAppLogger.appendToDebugPath(t.c_str());
        getSyncAppBase()->fAppLogger.appendToDebugPath("_global");
      }
    }
    // define this as the main thread
    getSyncAppBase()->fAppLogger.DebugDefineMainThread();
    #endif
  }
}; // TDebugConfig::localResolve


#ifndef HARDCODED_CONFIG

// debug option (combination) names
const char * const debugOptionNames[numDebugOptions] = {
  // current categories
  "hot",
  "error",
  "data",
  "admin",
  "syncml",
  "remoteinfo",
  "parse",
  "generate",
  "rtk_sml",
  "rtk_xpt",
  "session",
  "lock",
  "objinst",
  "transp",
  "scripts",
  "profiling",
  "rest",

  // flags mostly (not always) used in combination with some of the basic categories
  "userdata",
  "dbapi",
  "plugin",
  "filter",
  "match",
  "conflict",
  "details",
  "exotic",
  "expressions",

  // useful sets
  "all",
  "minimal",
  "normal",
  "extended",
  "maximal",
  "db",
  "syncml_rtk",

  // old ones
  "items",
  "cmd",
  "devinf",
  "dataconf"
};

const uInt32 debugOptionMasks[numDebugOptions] = {
  // current categories
  DBG_HOT,
  DBG_ERROR,
  DBG_DATA,
  DBG_ADMIN,
  DBG_PROTO,
  DBG_REMOTEINFO,
  DBG_PARSE,
  DBG_GEN,
  DBG_RTK_SML,
  DBG_RTK_XPT,
  DBG_SESSION,
  DBG_LOCK,
  DBG_OBJINST,
  DBG_TRANSP,
  DBG_SCRIPTS,
  DBG_PROFILE,
  DBG_REST,

  // flags mostly (not always) used in combination with some of the basic categories
  DBG_USERDATA,
  DBG_DBAPI,
  DBG_PLUGIN,
  DBG_FILTER,
  DBG_MATCH,
  DBG_CONFLICT,
  DBG_DETAILS,
  DBG_EXOTIC,
  DBG_SCRIPTEXPR,

  // useful sets
  DBG_ALL,
  DBG_MINIMAL,
  DBG_NORMAL,
  DBG_EXTENDED,
  DBG_MAXIMAL,
  DBG_ALLDB,
  DBG_RTK_SML+DBG_RTK_XPT,

  // old names that are mapped to new masks
  DBG_DATA, // formerly: DBG_ITEMS
  DBG_PROTO, // formerly: DBG_CMD
  DBG_REMOTEINFO, // formerly: DBG_DEVINF
  DBG_PARSE+DBG_GEN // formerly: DBG_DATACONV
};



uInt32 TDebugConfig::str2DebugMask(const char **aAttributes)
{
  expectEmpty(); // enable may not have content
  // process arguments
  const char* dbgopt = getAttr(aAttributes,"option");
  if (!dbgopt) {
    ReportError(false,"debug enable/disable, missing 'option' attribute");
  }
  sInt16 k;
  if (StrToEnum(debugOptionNames,numDebugOptions,k,dbgopt)) {
    return debugOptionMasks[k];
  }
  ReportError(false,"unknown debug option '%s'",dbgopt);
  return 0;
} // TDebugConfig::str2DebugMask


// debug config element parsing
bool TDebugConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"mask")==0)
    expectUInt32(fDebug);
  else if (strucmp(aElementName,"enable")==0)
    fDebug = fDebug | str2DebugMask(aAttributes);
  else if (strucmp(aElementName,"disable")==0)
    fDebug = fDebug & ~str2DebugMask(aAttributes);
  else if (strucmp(aElementName,"logpath")==0)
    expectPath(fDebugInfoPath);
  else if (strucmp(aElementName,"msgdump")==0)
    expectBool(fMsgDump);
  else if (strucmp(aElementName,"xmltranslate")==0)
    expectBool(fXMLtranslate);
  else if (strucmp(aElementName,"simmsgread")==0)
    expectBool(fSimMsgRead);
  else if (strucmp(aElementName,"sessionlogs")==0)
    expectBool(fSessionDebugLogs);
  else if (strucmp(aElementName,"globallogs")==0)
    expectBool(fGlobalDebugLogs);
  // options for TDebugLogger
  else if (strucmp(aElementName,"logformat")==0)
    expectEnum(sizeof(fSessionDbgLoggerOptions.fOutputFormat),&fSessionDbgLoggerOptions.fOutputFormat,DbgOutFormatNames,numDbgOutFormats);
  else if (strucmp(aElementName,"folding")==0)
    expectEnum(sizeof(fSessionDbgLoggerOptions.fFoldingMode),&fSessionDbgLoggerOptions.fFoldingMode,DbgFoldingModeNames,numDbgFoldingModes);
  // source link settings always available, even if feature is not:
  // allows adding the setting to configs unconditionally, without
  // triggering config parser errors
  else if (strucmp(aElementName,"sourcelink")==0)
    expectEnum(sizeof(fSessionDbgLoggerOptions.fSourceLinkMode),&fSessionDbgLoggerOptions.fSourceLinkMode,DbgSourceModeNames,numDbgSourceModes);
  else if (strucmp(aElementName,"sourcebase")==0)
    expectPath(fSessionDbgLoggerOptions.fSourceRootPath);
  else if (strucmp(aElementName,"indentstring")==0)
    expectCString(fSessionDbgLoggerOptions.fIndentString);
  else if (strucmp(aElementName,"fileprefix")==0)
    expectRawString(fSessionDbgLoggerOptions.fCustomPrefix);
  else if (strucmp(aElementName,"filesuffix")==0)
    expectRawString(fSessionDbgLoggerOptions.fCustomSuffix);
  else if (strucmp(aElementName,"filename")==0)
    expectRawString(fSessionDbgLoggerOptions.fBasename);
  else if (strucmp(aElementName,"logflushmode")==0)
    expectEnum(sizeof(fSessionDbgLoggerOptions.fFlushMode),&fSessionDbgLoggerOptions.fFlushMode,DbgFlushModeNames,numDbgFlushModes);
  else if (strucmp(aElementName,"appendtoexisting")==0)
    expectBool(fSessionDbgLoggerOptions.fAppend);
  else if (strucmp(aElementName,"timestamp")==0)
    expectBool(fSessionDbgLoggerOptions.fTimestampStructure);
  else if (strucmp(aElementName,"timestampall")==0)
    expectBool(fSessionDbgLoggerOptions.fTimestampForAll);
  else if (strucmp(aElementName,"showthreadid")==0)
    expectBool(fSessionDbgLoggerOptions.fThreadIDForAll);
  else if (strucmp(aElementName,"subthreadmode")==0)
    expectEnum(sizeof(fSessionDbgLoggerOptions.fSubThreadMode),&fSessionDbgLoggerOptions.fSubThreadMode,DbgSubthreadModeNames,numDbgSubthreadModes);
  else if (strucmp(aElementName,"subthreadbuffersize")==0)
    expectUInt32(fSessionDbgLoggerOptions.fSubThreadBufferMax);
  else if (strucmp(aElementName,"singlegloballog")==0)
    expectBool(fSingleGlobLog);
  else if (strucmp(aElementName,"singlesessionlog")==0)
    expectBool(fSingleSessionLog);
  else if (strucmp(aElementName,"timedsessionlognames")==0)
    expectBool(fTimedSessionLogNames);
  else if (strucmp(aElementName,"logsessionstoglobal")==0)
    expectBool(fLogSessionsToGlobal);
  else
    return false; // invalid element
  return true;
} // TDebugConfig::localStartElement

#endif
#endif


#ifndef ENGINE_LIBRARY

// in engine library, all output must be in context of an engineInterface/appBase

// Debug output routines
//
// If a fDebugLogOutputFunc callback is defined,
// it will be used for output, otherwise global gDebugLogPath
// file will be written

uInt32 getDbgMask(void)
{
  #ifdef SYDEBUG
  TSyncAppBase *appBase = getExistingSyncAppBase();
  if (!appBase) return 0; // no appbase -> no debug
  return appBase->getDbgMask();
  #else
  return 0;
  #endif
} // getDebugMask


TDebugLogger *getDbgLogger(void)
{
  #ifdef SYDEBUG
  TSyncAppBase *appBase = getExistingSyncAppBase();
  if (!appBase) return NULL; // no appbase -> no debuglogger
  return appBase->getDbgLogger();
  #else
  return NULL;
  #endif
} // getDbgLogger


// non-class DebugPuts
void DebugPuts(uInt32 mask, const char *text)
{
  #ifdef SYDEBUG
  // use global debug channel of appBase for non-object-context output
  TSyncAppBase *appBase = getExistingSyncAppBase();
  if (!appBase || appBase->getDbgMask()==0) return; // no appbase or debug off -> no output
  TDebugLogger *dbgLogger = appBase->getDbgLogger();
  if (!dbgLogger) return;
  dbgLogger->DebugPuts(mask,text,0,false);
  #endif
} // DebugPuts


// non-class print to debug channel
void DebugVPrintf(uInt32 mask, const char *format, va_list args)
{
  #ifdef SYDEBUG
  // use global debug channel of appBase for non-object-context output
  TSyncAppBase *appBase = getExistingSyncAppBase();
  if (!appBase || appBase->getDbgMask()==0) return; // no appbase or debug off -> no output
  TDebugLogger *dbgLogger = appBase->getDbgLogger();
  if (!dbgLogger) return;
  dbgLogger->DebugVPrintf(mask,format,args);
  #endif
} // DebugVPrintf


// non-class print to debug channel
void DebugPrintf(const char *text, ...)
{
  #ifdef SYDEBUG
  va_list args;
  if (PDEBUGMASK) {
    va_start(args, text);
    DebugVPrintf(DBG_TRANSP, text,args);
    va_end(args);
  } // if (PDEBUGMASK)
  #endif
} // DebugPrintf


void smlLibPrint(const char *text, ...)
{
  #ifdef SYDEBUG
  va_list args;
  va_start(args, text);
  DebugVPrintf(DBG_RTK_SML,text,args);
  va_end(args);
  #endif
} // smlLibPrint


void smlLibVprintf(const char *format, va_list va)
{
  #ifdef SYDEBUG
  DebugVPrintf(DBG_RTK_SML,format,va);
  #endif
} // smlLibVprintf


// entry point for SyncML-Toolkit with
// #define TRACE_TO_STDOUT
void localOutput(const char *aFormat, va_list aArgs)
{
  #ifdef SYDEBUG
  NCDEBUGVPRINTFX(DBG_RTK_SML,aFormat,aArgs);
  #endif
}


#endif // not ENGINE_LIBRARY


// Console printout. Only enabled when defined(CONSOLEINFO)

// non-class print to console
void ConsolePrintf(const char *text, ...)
{
  #ifdef CONSOLEINFO
  const sInt16 maxmsglen=1024;
  char msg[maxmsglen];
  va_list args;

  msg[0]='\0';
  va_start(args, text);
  // assemble the message string
  vsnprintf(msg, maxmsglen, text, args);
  va_end(args);
  // write the string
  ConsolePuts(msg);
  #endif
} // sysyncConsolePrintf


// non-class Console output
void ConsolePuts(const char *text)
{
  #ifdef CONSOLEINFO
  // show on app's console equivalent
  AppConsolePuts(text);
  #endif // console enabled
} // sysyncConsolePuts



// TSyncAppBase
// ============


/* SyncML toolkit callback function declarations */

extern "C" {
  /* message callbacks */
  static Ret_t smlStartMessageCallback(InstanceID_t id, VoidPtr_t userData, SmlSyncHdrPtr_t pContent);
  static Ret_t smlEndMessageCallback(InstanceID_t id, VoidPtr_t userData, Boolean_t final);
  /* grouping commands */
  static Ret_t smlStartSyncCallback(InstanceID_t id, VoidPtr_t userData, SmlSyncPtr_t pContent);
  static Ret_t smlEndSyncCallback(InstanceID_t id, VoidPtr_t userData);
  #ifdef ATOMIC_RECEIVE  /* these callbacks are NOT included in the Toolkit lite version */
    static Ret_t smlStartAtomicCallback(InstanceID_t id, VoidPtr_t userData, SmlAtomicPtr_t pContent);
    static Ret_t smlEndAtomicCallback(InstanceID_t id, VoidPtr_t userData);
  #endif
  #ifdef SEQUENCE_RECEIVE
    static Ret_t smlStartSequenceCallback(InstanceID_t id, VoidPtr_t userData, SmlSequencePtr_t pContent);
    static Ret_t smlEndSequenceCallback(InstanceID_t id, VoidPtr_t userData);
  #endif
  /* Sync Commands */
  static Ret_t smlAddCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlAddPtr_t pContent);
  static Ret_t smlAlertCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlAlertPtr_t pContent);
  static Ret_t smlDeleteCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlDeletePtr_t pContent);
  static Ret_t smlGetCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlGetPtr_t pContent);
  static Ret_t smlPutCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlPutPtr_t pContent);
  #ifdef MAP_RECEIVE
    static Ret_t smlMapCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlMapPtr_t pContent);
  #endif
  #ifdef RESULT_RECEIVE
    static Ret_t smlResultsCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlResultsPtr_t pContent);
  #endif
  static Ret_t smlStatusCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlStatusPtr_t pContent);
  static Ret_t smlReplaceCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlReplacePtr_t pContent);
  /* othe commands */
  #ifdef COPY_RECEIVE  /* these callbacks are NOT included in the Toolkit lite version */
    static Ret_t smlCopyCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlCopyPtr_t param);
  #endif
  static Ret_t smlMoveCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlMovePtr_t param);
  #ifdef EXEC_RECEIVE
    static Ret_t smlExecCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlExecPtr_t pContent);
  #endif
  #ifdef SEARCH_RECEIVE
    static Ret_t smlSearchCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlSearchPtr_t pContent);
  #endif
  /* Other Callbacks */
  static Ret_t smlHandleErrorCallback(InstanceID_t id, VoidPtr_t userData);
  static Ret_t smlTransmitChunkCallback(InstanceID_t id, VoidPtr_t userData);

  /* print callback */
  void smlPrintCallback(String_t outputString);
} // extern "C" declaration


// constructor
TSyncAppBase::TSyncAppBase() :
  fIsServer(false),
  fDeleting(false),
  fConfigP(NULL),
  fRequestCount(0),
  #if defined(PROGRESS_EVENTS) && !defined(ENGINE_LIBRARY)
  fProgressEventFunc(NULL),
  #endif
  #ifdef ENGINEINTERFACE_SUPPORT
  fEngineInterfaceP(NULL),
  #else
  fMasterPointer(NULL),
  #endif
  fApiInterModuleContext(0)
  // reset all callbacks
  #ifdef SYDEBUG
  // app logger
  ,fAppLogger(&fAppZones)
  #endif
{
  // at the moment of creation, this is now the SyncAppBase
  // (set it here already to allow getSyncAppBase() from derived constructors)
  #ifdef ENGINEINTERFACE_SUPPORT
    #ifdef DIRECT_APPBASE_GLOBALACCESS
      getEngineInterface()->setSyncAppBase(this); // set the link so getSyncAppBase() via EngineInterface works immediately
    #endif
  #else
    #ifdef ENGINE_LIBRARY
      #error "DIRECT_APPBASE_GLOBALACCESS is not allowed with ENGINE_LIBRARY"
    #endif
    sysync_glob_setanchor(this);
  #endif
  #ifdef MD5_TEST_FUNCS
  md5::dotest();
  #endif
  // init profiling
  TP_INIT(fTPInfo);
  TP_START(fTPInfo,TP_general);
  #ifdef EXPIRES_AFTER_DATE
  // - get current time
  sInt16 y,m,d;
  lineartime2date(getSystemNowAs(TCTX_UTC),&y,&m,&d);
  // - calculate scrambled version thereof
  fScrambledNow =
    ((sInt32)y-1720l)*12l*42l+
    ((sInt32)m-1)*42l+
    ((sInt32)d+7l);
  /*
  #define SCRAMBLED_EXPIRY_VALUE \
   (EXPIRY_DAY+7)+ \
   (EXPIRY_MONTH-1)*42+ \
   (EXPIRY_YEAR-1720)*12*42
  */
  #endif
  #ifdef APP_CAN_EXPIRE
  fAppExpiryStatus = LOCERR_OK; // do not compare here, would be too easy
  #endif
  #if defined(SYDEBUG) && defined(HARDCODED_CONFIG)
  fConfigFilePath = "<hardcoded config>";
  #endif
  #ifdef SYSER_REGISTRATION
  // make sure that license is standard (no phone home, no expiry) in case there is NO license at all
  fRegLicenseType = 0;
  fRelDuration = 0;
  fRegDuration = 0;
  #if defined(EXPIRES_AFTER_DAYS) && defined(ENGINE_LIBRARY)
  fFirstUseDate = 0;
  fFirstUseVers = 0;
  #endif
  #endif

  // TODO: put this somewhere where the return code can be checked and reported to the user of TSyncAppBase
  fAppZones.initialize();
} // TSyncAppBase::TSyncAppBase


// destructor
TSyncAppBase::~TSyncAppBase()
{
  fDeleting=true; // flag deletion to block calling critical (virtual) methods
  #if !defined(ENGINEINTERFACE_SUPPORT) || defined(DIRECT_APPBASE_GLOBALACCESS)
  sysync_glob_setanchor(NULL);
  #endif
  // stop and show profiling info
  TP_STOP(fTPInfo);
  #ifdef TIME_PROFILING
  if (PDEBUGMASK & DBG_PROFILE) {
    sInt16 i;
    PDEBUGPRINTFX(DBG_PROFILE,("Non-Session CPU usage statistics: (system/user)"));
    // sections
    for (i=0; i<numTPTypes; i++) {
      PNCDEBUGPRINTFX(DBG_PROFILE,(
        "- %-20s : (%10ld/%10ld) ms",
        TP_TypeNames[i],
        TP_GETSYSTEMMS(fTPInfo,(TTP_Types)i),
        TP_GETUSERMS(fTPInfo,(TTP_Types)i)
      ));
    }
    // total
    PNCDEBUGPRINTFX(DBG_PROFILE,(
      "- %-20s : (%10ld/%10ld) ms",
      "TOTAL",
      TP_GETTOTALSYSTEMMS(fTPInfo),
      TP_GETTOTALUSERMS(fTPInfo)
    ));
  }
  #endif
  // delete the config now
  #ifdef SYDEBUG
  // - but first make sure applogger does not refer to it any more
  fAppLogger.setOptions(NULL);
  #endif
  // - now delete
  if (fConfigP) delete fConfigP;
} // TSyncAppBase::~TSyncAppBase



#ifndef HARDCODED_CONFIG

// get config variables
bool TSyncAppBase::getConfigVar(cAppCharP aVarName, string &aValue)
{
  // look up in user-defined variables first
  TStringToStringMap::iterator pos = fConfigVars.find(aVarName);
  if (pos!=fConfigVars.end()) {
    aValue = (*pos).second;
    return true; // found in user defined vars
  }
  // check some globals
  if (strucmp(aVarName,"version")==0) {
    // version
    aValue = SYSYNC_FULL_VERSION_STRING;
    return true;
  }
  if (strucmp(aVarName,"hexversion")==0) {
    // string-comparable version as 8-digit hex MMmmrrbb (Major, minor, rev, build)
    StringObjPrintf(aValue,"%02X%02X%02X%02X",SYSYNC_VERSION_MAJOR,SYSYNC_VERSION_MINOR,SYSYNC_SUBVERSION,SYSYNC_BUILDNUMBER);
    return true;
  }
  if (strucmp(aVarName,"manufacturer")==0) {
    aValue = getManufacturer();
    return true;
  }
  if (strucmp(aVarName,"model")==0) {
    aValue = getModel();
    return true;
  }
  if (strucmp(aVarName,"variant")==0) {
    // variant
    #if SYSER_VARIANT_CODE==SYSER_VARIANT_STD
    aValue = "STD";
    #elif SYSER_VARIANT_CODE==SYSER_VARIANT_PRO
    aValue  ="PRO";
    #elif SYSER_VARIANT_CODE==SYSER_VARIANT_CUSTOM
    aValue = "CUSTOM";
    #elif SYSER_VARIANT_CODE==SYSER_VARIANT_DEMO
    aValue = "DEMO";
    #else
    aValue = "unknown";
    #endif
    return true;
  }
  if (strucmp(aVarName,"productcode")==0) {
    #ifdef SYSER_PRODUCT_CODE
    StringObjPrintf(aValue,"%d",SYSER_PRODUCT_CODE);
    #else
    aValue = "unknown";
    #endif
    return true;
  }
  if (strucmp(aVarName,"extraid")==0) {
    #ifdef SYSER_PRODUCT_CODE
    StringObjPrintf(aValue,"%d",SYSER_EXTRA_ID);
    #else
    aValue = "unknown";
    #endif
    return true;
  }
  if (strucmp(aVarName,"platformname")==0) {
    aValue = SYSYNC_PLATFORM_NAME;
    return true;
  }
  // look up in platform strings
  int plsId;
  for (plsId=0; plsId<numPlatformStrings; plsId++) {
    if (strucmp(aVarName,PlatformStringNames[plsId])==0) {
      // get platform string
      return getPlatformString((TPlatformStringID)plsId,aValue);
    }
  }
  // not found
  return false;
} // TSyncAppBase::getConfigVar


// set config variable
bool TSyncAppBase::setConfigVar(cAppCharP aVarName, cAppCharP aNewValue)
{
  // set user-defined config variable
  fConfigVars[aVarName] = aNewValue;
  return true;
} // TSyncAppBase::setConfigVar


// undefine config variable
bool TSyncAppBase::unsetConfigVar(cAppCharP aVarName)
{
  TStringToStringMap::iterator pos = fConfigVars.find(aVarName);
  if (pos!=fConfigVars.end()) {
    // exist, remove from map
    fConfigVars.erase(pos);
    return true;
  }
  // did not exist
  return false;
} // TSyncAppBase::unsetConfigVar


// expand config vars in string
bool TSyncAppBase::expandConfigVars(string &aString, sInt8 aCfgVarExp, TConfigElement *aCfgElement, cAppCharP aElementName)
{
  string::size_type n,n2;

  if (aCfgVarExp<=0 || aString.empty()) return true; // no expansion
  if (aCfgElement && !aElementName) aElementName="*Unknown*";
  n=0;
  while(true) {
    n = aString.find("$(",n);
    if (n==string::npos)
      break; // no more macros
    // found macro name start - now search end
    n+=2; // position after $( lead-in
    n2 = aString.find(")",n);
    if (n2!=string::npos) {
      // macro name found
      string vn,vv;
      vn.assign(aString,n,n2-n); // name
      if (getConfigVar(vn.c_str(),vv)) {
        if (aCfgVarExp==2) {
          // check for recursion loop
          vn.insert(0,"$("); vn.append(")");
          if (vv.find(vn,0)!=string::npos) {
            if (aCfgElement) aCfgElement->ReportError(true,"Recursive config variable $(%s) in <%s>",vn.c_str(),aElementName);
            n=n2+1; // do not expand
            continue;
          }
        }
        // found value - substitute
        n-=2; // substitute beginning with leadin
        n2+=1; // include closing paranthesis
        aString.replace(n,n2-n,vv);
        if (aCfgVarExp<2) {
          // do not allow recursive macro expansion
          n+=vv.size(); // continue searching past substituted chars
        }
      }
      else {
        // not found - leave macro as-is
        if (aCfgElement) aCfgElement->ReportError(false,"Undefined config variable $(%s) in <%s>",vn.c_str(),aElementName);
        n=n2+1; // continue search after closing paranthesis
      }
    }
    else {
      if (aCfgElement) aCfgElement->ReportError(false,"Unterminated $(xxx)-style config variable in <%s>",aElementName);
    }
  }
  return true;
} // TSyncAppBase::expandConfigVars

#endif // not HARDCODED_CONFIG

localstatus TSyncAppBase::finishConfig()
{
#ifdef SYDEBUG
  fAppZones.getDbgLogger = getDbgLogger();
#endif
  fAppZones.loggingStarted();
  return LOCERR_OK;
}

#ifdef HARDCODED_CONFIG


localstatus TSyncAppBase::initHardcodedConfig(void)
{
  localstatus err;

  // initialize for receiving new config
  fConfigP->clear();
  // now call initializer in derived root config
  err=fConfigP->createHardcodedConfig();
  if (err!=LOCERR_OK) return err;
  // make sure it gets all resolved
  fConfigP->ResolveAll();
  // is ok now
  return finishConfig();
} // TSyncAppBase::initHardcodedConfig


#else


// report config errors
void TSyncAppBase::ConferrPrintf(const char *text, ...)
{
  const sInt16 maxmsglen=1024;
  char msg[maxmsglen];
  va_list args;

  msg[0]='\0';
  va_start(args, text);
  // assemble the message string
  vsnprintf(msg, maxmsglen, text, args);
  va_end(args);
  // output config errors
  ConferrPuts(msg);
} // TSyncAppBase::ConferrPrintf


// report config errors to appropriate channel
void TSyncAppBase::ConferrPuts(const char *msg)
{
  #ifdef ENGINE_LIBRARY
    // engine variant
    string filename;
    // - get config var to see where we should put config errors
    if (!getConfigVar("conferrpath", filename))
      return; // no output defined for config errors
    // a config error path is defined
    if (strucmp(filename.c_str(),"console")==0) {
      // put message directly to what is supposed to be the console
      #ifdef ANDROID
      __android_log_write( ANDROID_LOG_DEBUG, "ConferrPuts", msg );
      #else
      AppConsolePuts(msg);
      #endif
      return; // done
    }
  #else // ENGINE_LIBRARY
    // old variant - output to predefined path
    #ifdef CONSOLEINFO
    ConsolePuts(msg);
    return;
    #elif defined(__PALM_OS__)
    return; // PalmOS has no file output
    #endif
    // prepare file name
    string filename;
    if (!getPlatformString(pfs_defout_path,filename)) return;
    makeOSDirPath(filename);
    filename+=CONFERRPREFIX;
    filename+=TARGETID;
    filename+=CONFERRSUFFIX;
  #endif // not ENGINE_LIBRARY
  #ifndef __PALM_OS__
  // now write to file
  FILE * logfile=fopen(filename.c_str(),"a");
  if (logfile) {
    string ts;
    StringObjTimestamp(ts,getSystemNowAs(TCTX_SYSTEM));
    ts.append(": ");
    fputs(ts.c_str(),logfile);
    fputs(msg,logfile);
    fputs("\n",logfile);
    fclose(logfile);
  }
  #endif // __PALM_OS__
} // TSyncAppBase::ConferrPuts


/* config reading */

// XML parser's "userdata"
typedef struct {
  XML_Parser parser;
  TRootConfigElement *rootconfig;
} TXMLUserData;


// prototypes
extern "C" {
  static void startElement(void *userData, const char *name, const char **atts);
  static void charData(void *userData, const XML_Char *s, int len);
  static void endElement(void *userData, const char *name);
}

static localstatus checkErrors(TRootConfigElement *aRootConfigP,XML_Parser aParser)
{
  const char *errmsg = aRootConfigP->getErrorMsg();
  if (errmsg) {
    aRootConfigP->getSyncAppBase()->ConferrPrintf(
      "%s at line %ld col %ld",
      errmsg,
      (sInt32)XML_GetCurrentLineNumber(aParser),
      (sInt32)XML_GetCurrentColumnNumber(aParser)
    );
    aRootConfigP->resetError();
    return aRootConfigP->getFatalError(); // return when fatal
  }
  else
    return LOCERR_OK; // no (fatal) error
} // checkErrors


// callback for expat
static void startElement(void *userData, const char *name, const char **atts)
{
  TRootConfigElement *cfgP = static_cast<TXMLUserData *>(userData)->rootconfig;
  XML_Parser parser = static_cast<TXMLUserData *>(userData)->parser;
  SYSYNC_TRY {
    cfgP->startElement(name,atts,XML_GetCurrentLineNumber(parser));
  }
  SYSYNC_CATCH (exception &e)
    cfgP->ReportError(true,"Exception in StartElement: %s",e.what());
  SYSYNC_ENDCATCH
  // check for errors
  checkErrors(cfgP,parser);
} // startElement


// callback for expat
static void charData(void *userData, const XML_Char *s, int len)
{
  TRootConfigElement *cfgP = static_cast<TXMLUserData *>(userData)->rootconfig;
  XML_Parser parser = static_cast<TXMLUserData *>(userData)->parser;
  SYSYNC_TRY {
    cfgP->charData(s,len);
  }
  SYSYNC_CATCH (exception &e)
    cfgP->ReportError(true,"Exception in charData: %s",e.what());
  SYSYNC_ENDCATCH
  // check for errors
  checkErrors(cfgP,parser);
} // charData



// callback for expat
static void endElement(void *userData, const char *name)
{
  TRootConfigElement *cfgP = static_cast<TXMLUserData *>(userData)->rootconfig;
  XML_Parser parser = static_cast<TXMLUserData *>(userData)->parser;
  SYSYNC_TRY {
    cfgP->endElement(name);
  }
  SYSYNC_CATCH (exception &e)
    cfgP->ReportError(true,"Exception in endElement: %s",e.what());
  SYSYNC_ENDCATCH
  // check for errors
  checkErrors(cfgP,parser);
} // endElement


// config stream reading
localstatus TSyncAppBase::readXMLConfigStream(TXMLConfigReadFunc aReaderFunc, void *aContext)
{
  localstatus fatalerr;

  // clear (reset to default) all config
  TP_DEFIDX(last);
  TP_SWITCH(last,fTPInfo,TP_configread);
  MP_SHOWCURRENT(DBG_HOT,"start reading config");

  // initialize for new config
  fConfigP->clear();
  fConfigP->ResetParsing();
  // read XML
  appChar buf[CONFIG_READ_BUFSIZ];
  sInt16 done;
  // - create parser
  XML_Parser parser = XML_ParserCreate(NULL);
  SYSYNC_TRY {
    // init user data struct
    TXMLUserData userdata;
    userdata.parser=parser;
    userdata.rootconfig=fConfigP;
    // pass pointer to root config here
    XML_SetUserData(parser, &userdata);
    XML_SetElementHandler(parser, startElement, endElement);
    XML_SetCharacterDataHandler(parser, charData);
    do {
      bufferIndex len=0;
      // callback reader func
      if (!(aReaderFunc)(buf,CONFIG_READ_BUFSIZ,&len,aContext)) {
        fConfigP->setFatalError(LOCERR_CFGREAD); // this is also fatal
        ConferrPrintf(
          "Error reading from config"
        );
        break;
      }
      // %%% changed stop criterium to smooth EngineInterface API
      //done = len < CONFIG_READ_BUFSIZ;
      done = len == 0;
      if (!XML_Parse(parser, buf, len, done)) {
        fConfigP->setFatalError(LOCERR_CFGPARSE); // this is also fatal
        ConferrPrintf(
          "%s at line %ld col %ld",
          XML_ErrorString(XML_GetErrorCode(parser)),
          (sInt32)XML_GetCurrentLineNumber(parser),
          (sInt32)XML_GetCurrentColumnNumber(parser)
        );
        break;
      }
      else
        if (fConfigP->getFatalError()) break;
    } while (!done);
    XML_ParserFree(parser);
    if (!fConfigP->getFatalError()) {
      // now resolve
      fConfigP->ResolveAll();
      // display resolve error, if any
      const char *msg = fConfigP->getErrorMsg();
      if (msg) {
        // this is fatal only if error was thrown in Resolve.
        // Some warnings might be set with ReportError and
        // will not cause abort.
        ConferrPrintf(msg);
      }
    }
    // check if ok or not
    if ((fatalerr=fConfigP->getFatalError())!=LOCERR_OK) {
      // config failed, reset
      // %%% do not clear the config here
      //fConfigP->clear();
      ConferrPrintf(
        "Fatal error %hd, no valid configuration could be read from XML file",
        (sInt16)fatalerr
      );
      TP_START(fTPInfo,last);
      return fatalerr;
    }
  }
  SYSYNC_CATCH (...)
    XML_ParserFree(parser);
    fConfigP->clear();
    fConfigP->setFatalError(LOCERR_CFGPARSE); // this is also fatal
    ConferrPrintf(
      "Exception while parsing XML, no valid configuration"
    );
    TP_START(fTPInfo,last);
    return LOCERR_CFGPARSE;
  SYSYNC_ENDCATCH
  TP_START(fTPInfo,last);
  #if defined(APP_CAN_EXPIRE) && defined(RELEASE_YEAR) && defined(SYSER_REGISTRATION)
  if (fAppExpiryStatus==LOCERR_TOONEW) {
    ConferrPrintf(
      "License is invalid for software released on or after %04d-%02d-01",
      (fRelDuration / 12) + 2000,
      (fRelDuration) % 12 + 1
    );
  }
  #endif
  #ifdef APP_CAN_EXPIRE
  if (fAppExpiryStatus!=LOCERR_OK) {
    if (fAppExpiryStatus==LOCERR_EXPIRED) {
      ConferrPrintf("Time-limited License expired");
    } else {
      ConferrPrintf("Missing or bad License Information");
    }
    ConferrPrintf("Please contact Synthesis AG to obtain new license information");
    fConfigP->setFatalError(fAppExpiryStatus); // this is also fatal
    return fAppExpiryStatus;
  }
  #endif
  #ifdef SYSER_REGISTRATION
  // check if config should have locked sections with a certain CRC value
  sInt16 daysleft;
  uInt32 shouldcrc;
  string s;
  bool ok=false;
  // - get restriction string from licensed info
  ok = getAppEnableInfo(daysleft, NULL, &s)==LOCERR_OK;
  string restrid,restrval;
  const char *p = s.c_str(); // start of license info string
  while (ok && (p=getLicenseRestriction(p,restrid,restrval))!=NULL) {
    if (restrid=="l") { // lock CRC
      StrToULong(restrval.c_str(),shouldcrc);
      ok=shouldcrc==fConfigP->getConfigLockCRC();
    }
  }
  if (!ok) {
    ConferrPrintf("Locked config sections are not valid");
    fConfigP->setFatalError(LOCERR_BADREG); // this is also fatal
    return LOCERR_BADREG;
  }
  #endif
  // ok if done
  MP_SHOWCURRENT(DBG_HOT,"finished reading config");
  return finishConfig();
} // TSyncAppBase::readXMLConfigStream


#ifdef CONSTANTXML_CONFIG

// stream reader for reading from compiled-in text constant
static int _CALLING_ ConstantReader(
  sysync::appCharP aBuffer,
  sysync::bufferIndex aMaxSize,
  sysync::bufferIndex *aReadCharsP,
  void *aContext // const char **
)
{
  // get cursor
  const char *readptr = *((const char **)aContext);
  // read from constant
  if (!readptr) return false;
  size_t len = strlen(readptr);
  if (len>aMaxSize) len=aMaxSize;
  // - copy
  if (len>0) strncpy(aBuffer,readptr,len);
  // - update cursor
  *((const char **)aContext)=readptr+len;
  *aReadCharsP=len; // return number of chars actually read
  return true; // successful
} // ConstantReader


// config file reading from hard-wired Constant
localstatus TSyncAppBase::readXMLConfigConstant(const char *aConstantXML)
{
  const char *aCursor = aConstantXML;
  return readXMLConfigStream(&ConstantReader, &aCursor);
  #ifdef SYDEBUG
  // signal where config came from
  fConfigFilePath="<XML read from string constant>";
  #endif
} // TSyncAppBase::readXMLConfigConstant


#endif


// stream reader for cfile
static int _CALLING_ CFileReader(
  appCharP aBuffer,
  bufferIndex aMaxSize,
  bufferIndex *aReadCharsP,
  void *aContext // FILE *
)
{
  FILE *cfgfile = (FILE *)aContext;
  // read from file
  size_t len = fread(aBuffer, 1, aMaxSize, cfgfile);
  if (len<=0) {
    if (!feof(cfgfile))
      return appFalse; // not EOF, other error: failed
    len=0; // nothing read, end of file
  }
  *aReadCharsP=len; // return number of chars actually read
  return appTrue; // successful
} // CFileReader


// config file reading from C file
localstatus TSyncAppBase::readXMLConfigCFile(FILE *aCfgFile)
{
  return readXMLConfigStream(&CFileReader, (void *)aCfgFile);
} // TSyncAppBase::readXMLConfigCFile


// read config from file path
localstatus TSyncAppBase::readXMLConfigFile(cAppCharP aFilePath)
{
  localstatus fatalerr;

  // open file
  FILE* cfgfile=fopen(aFilePath,"r");
  if (cfgfile) {
    // yes, there is a config file. Get its date
    getRootConfig()->fConfigDate = getFileModificationDate(aFilePath);
    // now read the config file (and possibly override fConfigDate)
    if ((fatalerr=readXMLConfigCFile(cfgfile))!=LOCERR_OK) {
      fclose(cfgfile);
      PDEBUGPRINTFX(DBG_ERROR,(
        "==== Fatal Error %hd reading config file '%s'",
        fatalerr,
        aFilePath
      ));
      return fatalerr; // config file with fatal errors
    }
    fclose(cfgfile);
    #if defined(SYDEBUG) && !defined(SYSYNC_TOOL)
    string t;
    StringObjTimestamp(t,getRootConfig()->fConfigDate);
    // now write settings to log
    PDEBUGPRINTFX(DBG_HOT,(
      "==== Config file '%s' read: Last Config Change=%s, Debug=0x%08lX, Lock=%ld",
      aFilePath,
      t.c_str(),
      (long)PDEBUGMASK,
      (long)fConfigP->getConfigLockCRC()
    ));
    PDEBUGPRINTFX(DBG_HOT,(
      "==== Config file ID = '%s'",
      getRootConfig()->fConfigIDString.c_str()
    ));
    #endif
    CONSOLEPRINTF(("- Config file read from '%s'",aFilePath));
    #ifdef SYDEBUG
    if (getRootConfig()->fDebugConfig.fSessionDebugLogs || getRootConfig()->fDebugConfig.fGlobalDebugLogs) {
      CONSOLEPRINTF(("- Debug log path: %s",getRootConfig()->fDebugConfig.fDebugInfoPath.c_str()));
    }
    // signal where config came from
    fConfigFilePath=aFilePath;
    #endif
    // config found
    return LOCERR_OK;
  }
  else {
    DEBUGPRINTFX(DBG_ERROR,("==== No Config file found under '%s'",aFilePath));
    // reset config to defaults
    fConfigP->clear();
    return LOCERR_NOCFGFILE;
  }
} // TSyncAppBase::readXMLConfigFile


#ifndef ENGINE_LIBRARY

// standard reading of config on predefined paths
localstatus TSyncAppBase::readXMLConfigStandard(const char *aConfigFileName, bool aOnlyGlobal, bool aAbsolute)
{
  localstatus fatalerr;

  SYSYNC_TRY {
    // - get path where config file should be
    string cfgfilename;
    sInt16 attempt= aOnlyGlobal ? 1 : 0; // if only global, don't look in exe dir (e.g. for ISAPI modules)
    while (true) {
      // count attempt
      attempt++;
      if (aAbsolute) {
        if (attempt>1) {
          // no config file found
          return LOCERR_NOCFGFILE;
        }
        // just use path as it is
        cfgfilename=aConfigFileName;
      }
      else {
        // try paths
        if (attempt==1) {
          // first check if there's a local copy
          if (!getPlatformString(pfs_loccfg_path,cfgfilename))
            continue; // none found, try next
        }
        else if (attempt==2) {
          // if no local config file, look for a global one
          if (!getPlatformString(pfs_globcfg_path,cfgfilename))
            continue; // none found, try next
        }
        else {
          CONSOLEPRINTF(("- No config file found, using default settings"));
          // reset config to defaults
          fConfigP->clear();
          return LOCERR_NOCFGFILE; // no config
        }
        // add file name
        makeOSDirPath(cfgfilename,false);
        cfgfilename+=aConfigFileName;
      }
      fatalerr=readXMLConfigFile(cfgfilename.c_str());
      if (fatalerr==LOCERR_OK)
        break; // config found, ok
    } // attempt loop
  } // try
  SYSYNC_CATCH (...)
    ConferrPrintf("Fatal Error (exception) while reading config file");
    return LOCERR_CFGREAD;
  SYSYNC_ENDCATCH
  return LOCERR_OK; // ok
} // TSyncAppBase::readXMLConfigStandard

#endif // ENGINE_LIBRARY

#endif // HARDCODED_CONFIG


/* progress sevent notification */

#if defined(PROGRESS_EVENTS) && !defined(ENGINE_LIBRARY)

// event generator
bool TSyncAppBase::NotifyAppProgressEvent(
  TProgressEventType aEventType,
  TLocalDSConfig *aDatastoreID,
  sInt32 aExtra1,
  sInt32 aExtra2,
  sInt32 aExtra3
)
{
  TProgressEvent theevent;

  if (fProgressEventFunc) {
    // there is a progress event callback
    // - prepare event
    theevent.eventtype=aEventType;
    theevent.datastoreID=aDatastoreID;
    theevent.extra=aExtra1;
    theevent.extra2=aExtra2;
    theevent.extra3=aExtra3;
    // - invoke callback (returns false if aborted)
    return fProgressEventFunc(
      theevent,
      fProgressEventContext
    );
  }
  // if no callback, never abort
  return true; // ok, no abort
} // TSyncAppBase::NotifyAppProgressEvent

#endif // non-engine progress events



/* logfile outputs */


/* SyncML toolkit callback handlers */


Ret_t TSyncAppBase::EndMessage(VoidPtr_t userData, Boolean_t aFinal)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->EndMessage(aFinal);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"EndMessage",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"EndMessage",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::EndMessage


Ret_t TSyncAppBase::StartSync(VoidPtr_t userData, SmlSyncPtr_t aContentP)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->StartSync(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"StartSync",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"StartSync",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::StartSync


Ret_t TSyncAppBase::EndSync(VoidPtr_t userData)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->EndSync();
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"EndSync",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"EndSync",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::EndSync


#ifdef SEQUENCE_RECEIVE
Ret_t TSyncAppBase::StartSequence(VoidPtr_t userData, SmlSequencePtr_t aContentP)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->StartSequence(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"StartSequence",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"StartSequence",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::StartSequence


Ret_t TSyncAppBase::EndSequence(VoidPtr_t userData)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->EndSequence();
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"EndSequence",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"EndSequence",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::EndSequence
#endif


#ifdef ATOMIC_RECEIVE
Ret_t TSyncAppBase::StartAtomic(VoidPtr_t userData, SmlAtomicPtr_t aContentP)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->StartAtomic(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"StartAtomic",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"StartAtomic",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::StartAtomic


Ret_t TSyncAppBase::EndAtomic(VoidPtr_t userData)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->EndAtomic();
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"EndAtomic",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"EndAtomic",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::EndAtomic
#endif


Ret_t TSyncAppBase::AddCmd(VoidPtr_t userData, SmlAddPtr_t aContentP)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->AddCmd(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"AddCmd",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"AddCmd",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::AddCmd


Ret_t TSyncAppBase::AlertCmd(VoidPtr_t userData, SmlAlertPtr_t aContentP)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->AlertCmd(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"AlertCmd",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"AlertCmd",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::AlertCmd


Ret_t TSyncAppBase::DeleteCmd(VoidPtr_t userData, SmlDeletePtr_t aContentP)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->DeleteCmd(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"DeleteCmd",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"DeleteCmd",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::DeleteCmd


Ret_t TSyncAppBase::GetCmd(VoidPtr_t userData, SmlGetPtr_t aContentP)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->GetCmd(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"GetCmd",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"GetCmd",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::GetCmd


Ret_t TSyncAppBase::PutCmd(VoidPtr_t userData, SmlPutPtr_t aContentP)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->PutCmd(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"PutCmd",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"PutCmd",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::PutCmd


#ifdef MAP_RECEIVE
Ret_t TSyncAppBase::MapCmd(VoidPtr_t userData, SmlMapPtr_t aContentP)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->MapCmd(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"MapCmd",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"MapCmd",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::MapCmd
#endif


#ifdef RESULT_RECEIVE
Ret_t TSyncAppBase::ResultsCmd(VoidPtr_t userData, SmlResultsPtr_t aContentP)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->ResultsCmd(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"ResultsCmd",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"ResultsCmd",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::ResultsCmd
#endif


Ret_t TSyncAppBase::StatusCmd(VoidPtr_t userData, SmlStatusPtr_t aContentP)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->StatusCmd(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"StatusCmd",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"StatusCmd",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::StatusCmd


Ret_t TSyncAppBase::ReplaceCmd(VoidPtr_t userData, SmlReplacePtr_t aContentP)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->ReplaceCmd(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"ReplaceCmd",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"ReplaceCmd",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::ReplaceCmd


#ifdef COPY_RECEIVE
Ret_t TSyncAppBase::CopyCmd(VoidPtr_t userData, SmlCopyPtr_t aContentP)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->CopyCmd(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"CopyCmd",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"CopyCmd",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::CopyCmd
#endif


Ret_t TSyncAppBase::MoveCmd(VoidPtr_t userData, SmlMovePtr_t aContentP)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->MoveCmd(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"MoveCmd",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"MoveCmd",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::MoveCmd


/* Other Callbacks */
Ret_t TSyncAppBase::HandleError(VoidPtr_t userData)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  // call Session
  SYSYNC_TRY {
    return ((TSyncSession *) userData)->HandleError();
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException((TSyncSession *)userData,"HandleError",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException((TSyncSession *)userData,"HandleError",NULL);
  SYSYNC_ENDCATCH
} // TSyncAppBase::HandleError



// %%%%
Ret_t TSyncAppBase::DummyHandler(VoidPtr_t userData, const char* msg)
{
  if (!userData) return SML_ERR_WRONG_PARAM;
  if (userData) {
    // session is attached
    SYSYNC_TRY {
      return ((TSyncSession *) userData)->DummyHandler(msg);
    }
    SYSYNC_CATCH (exception &e)
      return HandleDecodingException((TSyncSession *)userData,"DummyHandler",&e);
    SYSYNC_ENDCATCH
    SYSYNC_CATCH (...)
      return HandleDecodingException((TSyncSession *)userData,"DummyHandler",NULL);
    SYSYNC_ENDCATCH
  }
  else {
    DEBUGPRINTFX(DBG_HOT,("DummyHandler (without session attached): msg=%s",msg));
    return SML_ERR_OK;
  }
} // TSyncAppBase::DummyHandler


/* SyncML toolkit callback address table */

static const SmlCallbacks_t mySmlCallbacks = {
  /* message callbacks */
  smlStartMessageCallback,
  smlEndMessageCallback,
  /* grouping commands */
  smlStartSyncCallback,
  smlEndSyncCallback,
  #ifdef ATOMIC_RECEIVE  /* these callbacks are NOT included in the Toolkit lite version */
    smlStartAtomicCallback,
    smlEndAtomicCallback,
  #endif
  #ifdef SEQUENCE_RECEIVE
    smlStartSequenceCallback,
    smlEndSequenceCallback,
  #endif
  /* Sync Commands */
  smlAddCmdCallback,
  smlAlertCmdCallback,
  smlDeleteCmdCallback,
  smlGetCmdCallback,
  smlPutCmdCallback,
  #ifdef MAP_RECEIVE
    smlMapCmdCallback,
  #endif
  #ifdef RESULT_RECEIVE
    smlResultsCmdCallback,
  #endif
  smlStatusCmdCallback,
  smlReplaceCmdCallback,
  /* other commands */
  #ifdef COPY_RECEIVE  /* these callbacks are NOT included in the Toolkit lite version */
    smlCopyCmdCallback,
  #endif
  #ifdef EXEC_RECEIVE
    smlExecCmdCallback,
  #endif
  #ifdef SEARCH_RECEIVE
    smlSearchCmdCallback,
  #endif
  smlMoveCmdCallback,
  /* Other Callbacks */
  smlHandleErrorCallback,
  smlTransmitChunkCallback
}; /* sml_callbacks struct */


/* Context record to find back to appbase and store userData */

typedef struct {
  TSyncAppBase *appBaseP;
  void *userDataP;
} TSmlContextDataRec;


/* SyncML toolkit callback implementations */

// macros to simplify access to contex
#define GET_APPBASE(x) (((TSmlContextDataRec *)x)->appBaseP)
#define GET_USERDATA(x) (((TSmlContextDataRec *)x)->userDataP)

/* message callbacks */
static Ret_t smlStartMessageCallback(InstanceID_t id, VoidPtr_t userData, SmlSyncHdrPtr_t pContent) { return GET_APPBASE(userData)->StartMessage(id,GET_USERDATA(userData),pContent); }
static Ret_t smlEndMessageCallback(InstanceID_t id, VoidPtr_t userData, Boolean_t final) { return GET_APPBASE(userData)->EndMessage(GET_USERDATA(userData),final); }
/* grouping commands */
static Ret_t smlStartSyncCallback(InstanceID_t id, VoidPtr_t userData, SmlSyncPtr_t pContent) { return GET_APPBASE(userData)->StartSync(GET_USERDATA(userData),pContent); }
static Ret_t smlEndSyncCallback(InstanceID_t id, VoidPtr_t userData) { return GET_APPBASE(userData)->EndSync(GET_USERDATA(userData)); }
#ifdef ATOMIC_RECEIVE  /* these callbacks are NOT included in the Toolkit lite version */
  static Ret_t smlStartAtomicCallback(InstanceID_t id, VoidPtr_t userData, SmlAtomicPtr_t pContent) { return GET_APPBASE(userData)->StartAtomic(GET_USERDATA(userData),pContent); }
  static Ret_t smlEndAtomicCallback(InstanceID_t id, VoidPtr_t userData) { return GET_APPBASE(userData)->EndAtomic(GET_USERDATA(userData)); }
#endif
#ifdef SEQUENCE_RECEIVE
  static Ret_t smlStartSequenceCallback(InstanceID_t id, VoidPtr_t userData, SmlSequencePtr_t pContent) { return GET_APPBASE(userData)->StartSequence(GET_USERDATA(userData),pContent); }
  static Ret_t smlEndSequenceCallback(InstanceID_t id, VoidPtr_t userData) { return GET_APPBASE(userData)->EndSequence(GET_USERDATA(userData)); }
#endif
/* Sync Commands */
static Ret_t smlAddCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlAddPtr_t pContent) { return GET_APPBASE(userData)->AddCmd(GET_USERDATA(userData),pContent); }
static Ret_t smlAlertCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlAlertPtr_t pContent) { return GET_APPBASE(userData)->AlertCmd(GET_USERDATA(userData),pContent); }
static Ret_t smlDeleteCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlDeletePtr_t pContent) { return GET_APPBASE(userData)->DeleteCmd(GET_USERDATA(userData),pContent); }
static Ret_t smlGetCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlGetPtr_t pContent) { return GET_APPBASE(userData)->GetCmd(GET_USERDATA(userData),pContent); }
static Ret_t smlPutCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlPutPtr_t pContent) { return GET_APPBASE(userData)->PutCmd(GET_USERDATA(userData),pContent); }
#ifdef MAP_RECEIVE
  static Ret_t smlMapCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlMapPtr_t pContent) { return GET_APPBASE(userData)->MapCmd(GET_USERDATA(userData),pContent); }
#endif
#ifdef RESULT_RECEIVE
  static Ret_t smlResultsCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlResultsPtr_t pContent) { return GET_APPBASE(userData)->ResultsCmd(GET_USERDATA(userData),pContent); }
#endif
static Ret_t smlStatusCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlStatusPtr_t pContent) { return GET_APPBASE(userData)->StatusCmd(GET_USERDATA(userData),pContent); }
static Ret_t smlReplaceCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlReplacePtr_t pContent) { return GET_APPBASE(userData)->ReplaceCmd(GET_USERDATA(userData),pContent); }
/* other commands */
#ifdef COPY_RECEIVE  /* these callbacks are NOT included in the Toolkit lite version */
  static Ret_t smlCopyCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlCopyPtr_t pContent) { return GET_APPBASE(userData)->CopyCmd(GET_USERDATA(userData),pContent); }
#endif
static Ret_t smlMoveCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlMovePtr_t pContent) { return GET_APPBASE(userData)->MoveCmd(GET_USERDATA(userData),pContent); }
#ifdef EXEC_RECEIVE
  static Ret_t smlExecCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlExecPtr_t pContent) { /*%%%tbd return GET_APPBASE(userData)->ExecCmd(GET_USERDATA(userData),pContent); */ return SML_ERR_INVALID_OPTIONS; }
#endif
#ifdef SEARCH_RECEIVE
  static Ret_t smlSearchCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlSearchPtr_t pContent) { /*%%%tbd return GET_APPBASE(userData)->SearchCmd(GET_USERDATA(userData),pContent); */ return SML_ERR_INVALID_OPTIONS; }
#endif
/* Other Callbacks */
static Ret_t smlHandleErrorCallback(InstanceID_t id, VoidPtr_t userData) { return GET_APPBASE(userData)->DummyHandler(GET_USERDATA(userData),"ErrorCallback"); }
static Ret_t smlTransmitChunkCallback(InstanceID_t id, VoidPtr_t userData) { /*%%%tdb return GET_APPBASE(userData)->TransmitChunk(GET_USERDATA(userData),pContent); */ return SML_ERR_INVALID_OPTIONS; }


/* end callback implementations */


/* RTK interfacing */


Ret_t TSyncAppBase::setSmlInstanceUserData(
  InstanceID_t aInstanceID,
  void *aUserDataP
)
{
  void *ctxP = NULL;
  if (smlGetUserData(aInstanceID,&ctxP)==SML_ERR_OK && ctxP) {
    static_cast<TSmlContextDataRec *>(ctxP)->userDataP=aUserDataP;
    return SML_ERR_OK;
  }
  return SML_ERR_MGR_INVALID_INSTANCE_INFO; // invalid instance (has no TSmlContextDataRec in userData)
} // TSyncAppBase::setSmlInstanceUserData


Ret_t TSyncAppBase::getSmlInstanceUserData(
  InstanceID_t aInstanceID,
  void **aUserDataPP
)
{
  void *ctxP = NULL;
  if (smlGetUserData(aInstanceID,&ctxP)==SML_ERR_OK && ctxP) {
    *aUserDataPP = static_cast<TSmlContextDataRec *>(ctxP)->userDataP;
    return SML_ERR_OK;
  }
  return SML_ERR_MGR_INVALID_INSTANCE_INFO; // invalid instance (has no TSmlContextDataRec in userData)
} // TSyncAppBase::setSmlInstanceUserData




// create new SyncML toolkit instance
bool TSyncAppBase::newSmlInstance(
  SmlEncoding_t aEncoding,
  sInt32 aWorkspaceMem,
  InstanceID_t &aInstanceID
)
{
  SmlInstanceOptions_t myInstanceOptions;
  Ret_t err;

  #ifndef NOWSM
    #error "Only NOWSM version is supported any more"
  #endif
  // Set options
  // - encoding
  myInstanceOptions.encoding=aEncoding;
  // - total size of instance buffer
  //  (must have room for both incoming and outgoing message if instance
  //  is used for both)
  myInstanceOptions.workspaceSize=aWorkspaceMem;
  // - maximum outgoing message size
  myInstanceOptions.maxOutgoingSize=0; // %%% disabled for now, can be set later with setMaxOutgoingSize()
  // - create user data record
  TSmlContextDataRec *smlContextRecP = new TSmlContextDataRec;
  if (!smlContextRecP) return false;
  smlContextRecP->appBaseP=this; // pointer to find back to this syncappbase w/o the help of global vars
  smlContextRecP->userDataP=NULL; // userData will be SySyncSession pointer, but now session is not yet determined
  // - now instantiate (thread-safe!)
  err=smlInitInstance(
    &mySmlCallbacks, // callbacks
    &myInstanceOptions,
    smlContextRecP,
    &aInstanceID // where to store the instance ID
  );
  // - return instance or NULL if failed
  if (err==SML_ERR_OK) {
    DEBUGPRINTFX(DBG_RTK_SML,("////////////// sml Instance created, id(=instanceInfoPtr)=0x%08lX",(long)aInstanceID));
    return true; // success
  }
  else {
    DEBUGPRINTFX(DBG_ERROR,("************ smlInitInstance returned 0x%hX",(sInt16)err));
    aInstanceID=NULL; // none
    return false; // failed
  }
} // TSyncAppBase::newSmlInstance


void TSyncAppBase::freeSmlInstance(InstanceID_t aInstance)
{
  // forget instance now
  // - accept no-instance
  if (aInstance==NULL) return;
  // - there is an instance
  // - free context record
  void *ctxP;
  if (smlGetUserData(aInstance,&ctxP)==SML_ERR_OK && ctxP)
    delete static_cast<TSmlContextDataRec *>(ctxP);
  // - free instance itself
  Ret_t err=smlTerminateInstance(aInstance);
  DEBUGPRINTFX(DBG_RTK_SML,("////////////// sml Instance freed, id(=instanceInfoPtr)=0x%08lX, err=0x%hX",(long)aInstance,(sInt16)err));
  #ifdef SYDEBUG
  if (err!=SML_ERR_OK) {
    DEBUGPRINTFX(DBG_ERROR,("smlTerminateInstance returned 0x%hX",(sInt16)err));
  }
  #endif
} // TSyncAppBase::freeSmlInstance


// determine encoding from beginning of SyncML message data
SmlEncoding_t TSyncAppBase::encodingFromData(cAppPointer aData, memSize aDataSize)
{
  SmlEncoding_t enc = SML_UNDEF;
  if (aData && aDataSize>=5) {
    // check for WBXML intro sequences
    // Only SyncML 2.0 (OMA-TS-DS_Syntax-V2_0-20090212-C.doc, Paragraph 5.4, WBXML Usage) specifies it explicitly:
    //   For the purposes of OMA-DS, WBXML 1.1, WBXML 1.2 and WBXML 1.3 are functionally equivalent, and
    //   all MUST be accepted in implementations that support WBXML. Effectively, this merely requires the
    //   WBXML parser to accept 01, 02 or 03 as the first byte of the document.
    // About WBXML Versions, the WireShark Wiki http://wiki.wireshark.org/WAP_Binary_XML says:
    //   The initial WBXML 1.0 specification was not adopted. It is significantly different from the subsequent WBXML
    //   versions (1.1, 1.2 and 1.3). Those subsequent versions are almost identical.
    // Now check for valid WBXML versions: First byte is vvvvmmmm where v=major version-1, m=minor version
    uInt8 wbxmlvers = *((cUInt8P)aData);
    if (
      wbxmlvers=='\x01' || // WBXML 1.1
      wbxmlvers=='\x02' || // WBXML 1.2
      wbxmlvers=='\x03'    // WBXML 1.3
    ) {
      // WBXML version ok, check FPI
      // - a WBXML integer defines the FPI (multi-byte, MSByte first, Bit 7 is continuation flag, Bit 6..0 are data. LSByte has Bit7=0)
      cAppCharP fpi = ((cAppCharP)aData)+1;
      if (
        (*fpi=='\x01') || // FPI==1 means "unknown public identifier". Some Funambol servers use this
        (memcmp(fpi,"\x00\x00",2)==0) || // Public identifier as string
        (memcmp(fpi,"\xA4\x01",2)==0) || // Public identifier 0x1201 for SyncML 1.2
        (memcmp(fpi,"\x9F\x53",2)==0) || // Public identifier 0x0FD3 for SyncML 1.1
        (memcmp(fpi,"\x9F\x51",2)==0)    // Public identifier 0x0FD1 for SyncML 1.0
      )
        enc=SML_WBXML;
    }
    // check XML now if not detected WBXML.
    if (enc==SML_UNDEF) {
      // could be XML
      // - skip UTF-8 BOM if there is one
      cUInt8P p = (cUInt8P)aData;
      if (p[0]==0xEF && p[1]==0xBB && p[2]==0xBF)
        p+=3; // skip the BOM
      // now check for XML
      if (strnncmp((cAppCharP)p,"<?xml",5)==0 ||
                strnncmp((cAppCharP)p,"<SyncML",7)==0)
                enc=SML_XML;
    }
  }
  return enc;
} // TSyncAppBase::encodingFromData


// determine encoding from Content-Type: header value
SmlEncoding_t TSyncAppBase::encodingFromContentType(cAppCharP aTypeString)
{
  sInt16 enc = SML_UNDEF;
  if (aTypeString) {
    stringSize l=strlen(SYNCML_MIME_TYPE SYNCML_ENCODING_SEPARATOR);
    if (strucmp(aTypeString,SYNCML_MIME_TYPE SYNCML_ENCODING_SEPARATOR,l)==0) {
      // is SyncML, check encoding
      cAppCharP p = strchr(aTypeString,';');
      sInt16 cl = p ? p-aTypeString-l : 0; // length of encoding string (charset could be appended here)
      StrToEnum(SyncMLEncodingMIMENames,numSyncMLEncodings,enc,aTypeString+l,cl);
    }
  }
  return static_cast<SmlEncoding_t>(enc);
} // TSyncAppBase::encodingFromContentType




// save app state (such as settings in datastore configs etc.)
void TSyncAppBase::saveAppState(void)
{
  if (fConfigP) fConfigP->saveAppState();
} // TSyncAppBase::saveAppState


// manufacturer of overall solution (can be configured, while OEM is fixed to Synthesis)
string TSyncAppBase::getManufacturer(void)
{
  #ifdef ENGINEINTERFACE_SUPPORT
  if (fConfigP && !(fConfigP->fMan.empty()))
    return fConfigP->fMan;
  string s;
  if (getConfigVar("custommanufacturer", s)) {
    return s;
  }
  #endif
  // if no string configured, return default
  return CUST_SYNC_MAN;
} // TSyncAppBase::getManufacturer


// model (application name) of overall solution
string TSyncAppBase::getModel(void)
{
  #ifdef ENGINEINTERFACE_SUPPORT
  if (fConfigP && !(fConfigP->fMod.empty()))
    return fConfigP->fMod;
  string s;
  if (getConfigVar("custommodel", s)) {
    return s;
  }
  #endif
  // if no string configured, return default
  return CUST_SYNC_MODEL;
} // TSyncAppBase::getModel


// hardware version
string TSyncAppBase::getHardwareVersion(void)
{
  string s;
  #ifdef ENGINEINTERFACE_SUPPORT
  if (fConfigP && !(fConfigP->fHwV.empty())) {
    return fConfigP->fHwV;
  }
  if (getConfigVar("customhardwareversion", s)) {
    return s;
  }
  #endif
  // if no string configured, return default
  getPlatformString(pfs_device_name, s);
  return s;
} // TSyncAppBase::getHardwareVersion


// firmware version (depends a lot on the context - OS version?)
string TSyncAppBase::getFirmwareVersion(void)
{
  string s;
  #ifdef ENGINEINTERFACE_SUPPORT
  if (fConfigP && !(fConfigP->fFwV.empty())) {
    return fConfigP->fFwV;
  }
  if (getConfigVar("customfirmwareversion", s)) {
    return s;
  }
  #endif
  // if no string configured, return default
  getPlatformString(pfs_platformvers, s);
  return s;
} // TSyncAppBase::getHardwareVersion


// hardware type (PDA, PC, ...)
string TSyncAppBase::getDevTyp()
{
  #ifdef ENGINEINTERFACE_SUPPORT
  string s;
  if (fConfigP && !(fConfigP->fDevTyp.empty())) {
    return fConfigP->fDevTyp;
  }
  if (getConfigVar("customdevicetype", s)) {
    return s;
  }
  #endif
  // if no string configured, return default
  if (isServer())
    return SYNCML_SERVER_DEVTYP;
  else
    return SYNCML_CLIENT_DEVTYP;
} // TSyncAppBase::getDevTyp


// device ID (can be customized using "customdeviceid" config variable)
// Returns true if deviceID is guaranteed unique
bool TSyncAppBase::getMyDeviceID(string &devid)
{
  #ifdef ENGINEINTERFACE_SUPPORT
  if (getConfigVar("customdeviceid", devid)) {
    return true; // custom device ID is assumed to be guaranteed unique
  }
  #endif
  // use device ID as determined by platform adapters
  return getLocalDeviceID(devid);
} // TSyncAppBase::getMyDeviceID



#ifdef APP_CAN_EXPIRE

void TSyncAppBase::updateAppExpiry(void)
{
  // this is the basic check. Some other checks
  // are spread in various files to disguise checking a little
  #if defined(EXPIRES_AFTER_DAYS) || defined(SYSER_REGISTRATION)
  // check soft expiry
  fAppExpiryStatus = appEnableStatus();
  // check hard expiry only if demo, that is, if no valid license is installed
  if (fAppExpiryStatus==LOCERR_OK && fDaysLeft>=0 && !fRegOK)
  #endif
  {
    fAppExpiryStatus =
      #ifdef EXPIRES_AFTER_DATE
      // check hard expiry date
      fScrambledNow>SCRAMBLED_EXPIRY_VALUE ?
        LOCERR_EXPIRED : LOCERR_OK;
      #else
      // no hard expiry, just ok if enabled
      LOCERR_OK;
      #endif
  }
} // TSyncAppBase::updateAppExpiry

#endif



#ifdef SYSER_REGISTRATION

// checks if registered (must be implemented in base class)
// returns LOCERR_EXPIRED, LOCERR_TOONEW or LOCERR_BADREG if not registered correctly
localstatus TSyncAppBase::isRegistered(void)
{
  #if !defined(HARDCODED_CONFIG) || defined(ENGINEINTERFACE_SUPPORT)
  // we have licensing in the config file or using engine interface, check it
  return fConfigP ? checkRegInfo(fConfigP->fLicenseName.c_str(),fConfigP->fLicenseCode.c_str(),false) : LOCERR_BADREG;
  #else
  // no license checking at this level (maybe overriden method provides check)
  return LOCERR_EXPIRED;
  #endif
} // TSyncAppBase::isRegistered


// get (entire) registration string
void TSyncAppBase::getRegString(string &aString)
{
  #if !defined(HARDCODED_CONFIG) || defined(ENGINEINTERFACE_SUPPORT)
  // we have licensing in the config file or set via engine interface, use it
  if (fConfigP)
    aString=fConfigP->fLicenseName;
  else
  #endif
  {
    aString.erase();
  }
} // TSyncAppBase::getRegString


#endif


// safety checks
#if !defined(APP_CAN_EXPIRE) && defined(RELEASE_VERSION) && !defined(NEVER_EXPIRES_IS_OK)
  #error "Warning: Release version that never expires!"
#endif


#ifdef APP_CAN_EXPIRE

// make sure we have a valid variant code for the target
#ifndef SYSER_VARIANT_CODE
  #error "SYSER_VARIANT_CODE must be defined in target_options.h"
#endif

// check enable status of application
localstatus TSyncAppBase::appEnableStatus(void)
{
  // safety check - app w/o initialized config is NOT enabled
  // Note: agentconfig tested here, but all other config sections are created in clear() at the same time
  if (!fConfigP || !(fConfigP->fAgentConfigP)) {
    return LOCERR_WRONGUSAGE;
  }
  // check registration (which will disable normal expiry)
  #ifdef SYSER_REGISTRATION
  localstatus regsta = isRegistered(); // ok if registered
  #else
    #ifndef APP_CAN_EXPIRE
    localstatus regsta = LOCERR_OK; // not registerable, not exprining - just run forever
    #ifndef NEVER_EXPIRES_IS_OK
    #error "WARNING: Completely unlimited operation w/o license or expiry - is this intended??"
    #endif // not NEVER_EXPIRES_IS_OK
    #else
    localstatus regsta = LOCERR_BADREG; // not registerable, assume no license, must be eval which expires
    #endif // APP_CAN_EXPIRE
  #endif
  localstatus sta = regsta;
  // check expiry (only if registration has not already defined one)
  #ifdef APP_CAN_EXPIRE
    #ifdef NO_LICENSE_UNTIL_HARDEXPIRY
    // we don't need a valid license code until hard expiry date hits
    // so if we have a failure now, let the hard expiry decide
    if (regsta!=LOCERR_OK) {
      #ifdef SYSER_REGISTRATION
      fDaysLeft=1; // simulate expiring tomorrow
      #endif
      sta = LOCERR_OK; // ok if we find no hard expiry later
      // then let hard expiry decide
      #ifndef EXPIRES_AFTER_DATE
      #error "WARNING: NO_LICENSE_UNTIL_HARDEXPIRY without EXPIRES_AFTER_DATE - running forever w/o license - indended that way?"
      #endif
    }
    #endif
    sInt32 td = lineartime2dateonly(getSystemNowAs(TCTX_UTC));
    #if defined(EXPIRES_AFTER_DAYS) || defined(NO_LICENSE_UNTIL_HARDEXPIRY)
    if (regsta==LOCERR_BADREG || regsta==LOCERR_WRONGPROD) {
      // not registered (for this product), check if we are in evaluation period
      #ifdef EXPIRES_AFTER_DATE
      // check hard expiry first
      if (fScrambledNow>SCRAMBLED_EXPIRY_VALUE) {
        #ifdef SYSER_REGISTRATION
        fDaysLeft=0; // hard-expired
        #endif
        sta = LOCERR_EXPIRED;
      }
      else
      #endif
      {
        #ifdef EXPIRES_AFTER_DAYS
        // (bfo found that we need to check for > demo days too, as
        // otherwise some clever guys could install with the
        // clock 20 years in the future and then set the clock back)
        uInt32 vers;
        lineardate_t firstuse;
        getFirstUseInfo(SYSER_VARIANT_CODE,firstuse,vers);
        sInt32 d = firstuse+EXPIRES_AFTER_DAYS - td;
        fDaysLeft = d>0 && d<=EXPIRES_AFTER_DAYS ? d : 0;
        sta = d>0 ? LOCERR_OK : LOCERR_EXPIRED;
        #else
        // Do NOT change the status to expired! - just pass on the license error status
        // which might be ok here in case we have a NO_LICENSE_UNTIL_HARDEXPIRY
        //sta = LOCERR_EXPIRED;
        #endif
      }
    }
    #endif
  #else
    fDaysLeft=-1; // App cannot expire, no limit
  #endif
  return sta;
} // TSyncAppBase::appEnableStatus


// get registration information to display (and check internally)
localstatus TSyncAppBase::getAppEnableInfo(sInt16 &aDaysLeft, string *aRegnameP, string *aRegInternalsP)
{
  #ifndef SYSER_REGISTRATION
  // no registration
  if (aRegnameP) aRegnameP->erase();
  if (aRegInternalsP) aRegInternalsP->erase();
  aDaysLeft = -1; // no expiring license (altough hardexpiry might still apply)
  return appEnableStatus();
  #else
  // we have registration
  localstatus sta = appEnableStatus();
  if (sta!=LOCERR_OK) {
    if (aRegnameP) aRegnameP->erase();
    if (aRegInternalsP) aRegInternalsP->erase();
    return sta; // not enabled
  }
  // do an extra check here
  aDaysLeft = fDaysLeft;
  if (fDaysLeft==0) SYSYNC_THROW(exception()); // exit app, as fDaysLeft must be > 0 or -1 here
  // get strings if requested
  if (aRegnameP || aRegInternalsP) {
    string s;
    // get entire string
    getRegString(s);
    // check for separation into a visible (name) and and invisible (email or license restriction) part
    size_t n,m;
    size_t l=s.size();
    // - first priority: license restrictions in form  ::x=something
    n=s.find("::");
    if (n!=string::npos) {
      // found a license restriction
      m=n; // everthing before special separator belongs to name, rest to internals
    }
    else {
      // - second priority: a simple email address at the end of the string
      n=s.rfind('@');
      if (n==string::npos) {
        n=l; m=l; // no email, entire string is name
      }
      else {
        n=s.rfind(' ',n);
        if (n==string::npos) {
          n=l; m=l; // no email, entire string is name
        }
        else {
          m=n+1; // do not return separating space in either name nor internals
        }
      }
    }

    // now remove spaces at the beginning to avoid
    // invisible registration info
    int i = s.find_first_not_of(' ');
    if (i==string::npos) i=0; // no non-space -> start at beginning
    /*
    int  i;
    for (i= 0; i<n; i++) {
      if (s.find( ' ',i )!=i) break; // search for the first char, which is not ' '
    } // if
    */

    // now assign
    if (aRegnameP) aRegnameP->assign(s, i,n);
    if (aRegInternalsP) {
       if (m<l) aRegInternalsP->assign(s,m,l-m);
       else aRegInternalsP->erase();
    }
  }
  return sta;
  #endif
} // TSyncAppBase::getAppEnableInfo

#endif // APP_CAN_EXPIRE



#ifdef EXPIRES_AFTER_DAYS

// make sure we have a valid variant code for the target
#ifndef SYSER_VERSCHECK_MASK
  #error "SYSER_VERSCHECK_MASK must be defined in target_options.h"
#endif



/// @brief get time and version of first use. Version is included to allow re-evaluating after version changes significantly
void TSyncAppBase::getFirstUseInfo(uInt8 aVariant, lineardate_t &aFirstUseDate, uInt32 &aFirstUseVers)
{
  #ifndef ENGINEINTERFACE_SUPPORT
  // no first use date in base class, is overridden in derived platform-specific appBase classes.
  aFirstUseDate=0;
  aFirstUseVers=0;
  #else
  // use values stored into engine via settings keys
  aFirstUseDate=fFirstUseDate;
  aFirstUseVers=fFirstUseVers;
  #endif
} // TSyncAppBase::getFirstUseInfo


/// @brief update first use info to allow for repeated eval when user installs an all-new version
/// @return true if update was needed
/// @param[in] aVariant variant context to perform update
/// @param[in,out] aFirstUseDate date when this variant and version was used first.
///   Will be updated
/// @param[in,out] aFirstUseVers version that relates to aFirstUseDate.
///   Will be updated to current version if version check allows re-starting demo period.
bool TSyncAppBase::updateFirstUseInfo(lineardate_t &aFirstUseDate, uInt32 &aFirstUseVers)
{
  // update if current version is significantly newer than what we used last time
  if (
    aFirstUseDate==0 ||
    ((aFirstUseVers & SYSER_VERSCHECK_MASK) <
     (SYSYNC_VERSION_UINT32 & SYSER_VERSCHECK_MASK))
  ) {
    // current version is different in a relevant part of the version number, so reset first use
    aFirstUseDate = getSystemNowAs(TCTX_UTC) / linearDateToTimeFactor;
    aFirstUseVers = SYSYNC_VERSION_UINT32;
    return true;
  }
  // no updates
  return false;
} // TSyncAppBase::updateFirstUseInfo

#endif // EXPIRES_AFTER_DAYS


#ifdef SYSER_REGISTRATION

// helper to get next restriction out of license string
const char * TSyncAppBase::getLicenseRestriction(const char *aInfo, string &aID, string &aVal)
{
  const char *p;
  char c;
  bool quotedval=false;

  // if no info or at end of string, return NULL
  if (!aInfo || *aInfo==0) return NULL;
  // find next restriction
  aInfo=strstr(aInfo,"::");
  if (!aInfo) return NULL; // no more restrictions found
  // get ID of restriction
  aInfo+=2; // skip ::
  p=strchr(aInfo,'=');
  if (!p) return NULL; // no more restrictions found
  // - assign ID
  aID.assign(aInfo,p-aInfo);
  aInfo=p+1; // skip =
  // get value of restriction
  aVal.erase();
  if (*aInfo==0x22) {
    // starts with doublequote -> treat as quoted value
    quotedval=true;
    aInfo++;
  }
  while ((c=*aInfo)) {
    aInfo++;
    if (quotedval) {
      // quoted string ends with " and can contain backslash escapes
      if (c==0x22) break; // done after closing quote
      if (c=='\\') {
        if (*aInfo==0) break; // quote without char following, stop here
        c=*aInfo++; // get next char
      }
    }
    else {
      // unquoted string ends with next space
      if (isspace(c)) break;
    }
    // save char
    aVal+=c;
  }
  // return where to continue scanning for next item
  return aInfo;
} // TSyncAppBase::getLicenseRestriction


// checks registration code for CRC ok and compare with
// predefined constants for product code etc.
localstatus TSyncAppBase::checkRegInfo(const char *aRegKey, const char *aRegCode, bool aMangled)
{
  uInt32 infocrc;

  // extract information from registration code
  localstatus regSta = LOCERR_OK;
  fRelDuration=0;
  // do not allow registration text less than 12 chars
  if (strlen(aRegKey)<12)
    regSta=LOCERR_BADREG; // too short code is a bad code
  else if (!getSySerialInfo(
    aRegCode, // code
    fRegProductFlags,
    fRegProductCode,
    fRegLicenseType,
    fRegQuantity,
    fRegDuration,
    fLicCRC, // CRC as included in the license code
    infocrc,
    aMangled
  ))
    regSta=LOCERR_BADREG; // no code is a bad code
  if (regSta==LOCERR_OK) {
    // code is basically ok, check standard info
    if (!(
      // product code
      (
        (fRegProductCode == SYSER_PRODUCT_CODE_MAIN)
        #ifdef SYSER_PRODUCT_CODE_ALT1
        || (fRegProductCode == SYSER_PRODUCT_CODE_ALT1)
        #endif
        #ifdef SYSER_PRODUCT_CODE_ALT2
        || (fRegProductCode == SYSER_PRODUCT_CODE_ALT2)
        #endif
        #ifdef SYSER_PRODUCT_CODE_ALT3
        || (fRegProductCode == SYSER_PRODUCT_CODE_ALT3)
        #endif
        #ifdef SYSER_PRODUCT_CODE_ALT4
        || (fRegProductCode == SYSER_PRODUCT_CODE_ALT4)
        #endif
        #ifdef SYSER_PRODUCT_CODE_ALT5
        || (fRegProductCode == SYSER_PRODUCT_CODE_ALT5)
        #endif
        #ifdef SYSER_PRODUCT_CODE_ALT6
        || (fRegProductCode == SYSER_PRODUCT_CODE_ALT6)
        #endif
        #ifdef SYSER_PRODUCT_CODE_ALT7
        || (fRegProductCode == SYSER_PRODUCT_CODE_ALT7)
        #endif
        #ifdef SYSER_PRODUCT_CODE_ALT8
        || (fRegProductCode == SYSER_PRODUCT_CODE_ALT8)
        #endif
        #ifdef SYSER_PRODUCT_CODE_ALT9
        || (fRegProductCode == SYSER_PRODUCT_CODE_ALT9)
        #endif
      )
      // special SYSER_PRODFLAG_MAXRELDATE product flag
      && (
        ((SYSER_NEEDED_PRODUCT_FLAGS & SYSER_PRODFLAG_MAXRELDATE)==0) || // either maxreldate flag is not required...
        (fRegProductFlags & SYSER_PRODFLAG_MAXRELDATE) || // ..or it is present..
        (fRegDuration!=0) // ..or there is a time limit, which means that the even if the flag is required, product can run with a timed license without the flag
      )
      // test other product flags (all except SYSER_PRODFLAG_MAXRELDATE)
      && ((fRegProductFlags & (SYSER_NEEDED_PRODUCT_FLAGS & ~SYSER_PRODFLAG_MAXRELDATE)) == (SYSER_NEEDED_PRODUCT_FLAGS & ~SYSER_PRODFLAG_MAXRELDATE))
      && ((fRegProductFlags & SYSER_FORBIDDEN_PRODUCT_FLAGS) == 0)
    ))
      regSta=LOCERR_WRONGPROD;
    // anyway, check if CRC is ok
    if (!(fLicCRC == addNameToCRC(infocrc,aRegKey,aMangled))) {
      regSta=LOCERR_BADREG; // bad CRC is bad registration (and has precedence over wrong product)
      // make sure we do not enable exotic functionality
      fRegProductFlags=0;
      fRegLicenseType=0;
      fRegQuantity=1;
      fRegDuration=0;
      fRelDuration=0;
    }
  }
  // Now if we have OK, app is registered, but maybe limited
  // by time or max release date.
  if (regSta==LOCERR_OK) {
    // - check max release date limit first
    if (fRegProductFlags & SYSER_PRODFLAG_MAXRELDATE) {
      // duration is a release duration
      // - in case this is a max release date limited license, this
      //   excludes time limited - max release date licenses are always permanent
      fRelDuration = fRegDuration;
      fRegDuration = 0;
      // Now check if this build has a hard-coded release date
      #ifdef RELEASE_YEAR
      // license valid only up to release date specified, check that
      // if (fRegDuration > (RELEASE_YEAR-2000)*12+RELEASE_MONTH-1)  // plain
      if (
        fRelDuration &&
        (fRelDuration*3+48 <= 3*((RELEASE_YEAR-2000)*12+RELEASE_MONTH-1)+48)
      ) { // a bit disguised
        // license is not valid any more for a build as new as this one
        fRegOK=false;
        return LOCERR_TOONEW;
      }
      #endif
    }
    // - check for expired license now
    //   Note: we don't check demo period (days after first use) here
    if (fRegDuration!=0) {
      lineardate_t ending = date2lineardate(fRegDuration/12+2000,fRegDuration%12+1,1);
      sInt32 d=ending-lineartime2dateonly(getSystemNowAs(TCTX_UTC));
      fDaysLeft = d>0 ? d : 0;
      if (d<=0) {
        // license has expired
        fRegOK=false;
        return LOCERR_EXPIRED;
      }
    }
    else
      fDaysLeft=-1; // unlimited by registration
  }
  // if it is not ok here, registration code is bad
  fRegOK=regSta==LOCERR_OK; // save for further reference
  return regSta; // return status
} // TSyncAppBase::checkRegInfo


// checks if current license is properly activated
// - note: must be called at a time when internet connection is available
localstatus TSyncAppBase::checkLicenseActivation(lineardate_t &aLastcheck, uInt32 &aLastcrc)
{
  bool ok = true;

  // - check if already activated
  if (
    fRegLicenseType && (
      (aLastcrc != fLicCRC) ||  // different license than at last activation
      (aLastcheck==0) || // never checked at all
      (aLastcheck+180 < lineartime2dateonly(getSystemNowAs(TCTX_UTC))) // not checked in the last half year
    )
  ) {
    // re-check needed
    ok = checkLicenseType(fRegLicenseType);
    // update check date even if we fail (but only if we checked once before)
    // to prevent failed sync sessions when reg server has a problem
    if (ok || aLastcheck) {
      aLastcheck = lineartime2dateonly(getSystemNowAs(TCTX_UTC));
    }
    // update CRC only if we could validate the new CRC
    if (ok) {
      aLastcrc = fLicCRC;
    }
  }
  // show it to user
  if (!ok) {
    #ifndef ENGINE_LIBRARY
    APP_PROGRESS_EVENT(this,pev_error,NULL,LOCERR_BADREG,0,0);
    #endif
  }
  // return status
  return ok ? LOCERR_OK : LOCERR_BADREG;
} // TSyncAppBase::checkLicenseActivation

#endif


#ifdef CONCURRENT_DEVICES_LIMIT

// check if session count is exceeded
void TSyncAppBase::checkSessionCount(sInt32 aSessionCount, TSyncSession *aSessionP)
{
  // Check if session must be busy (due to licensing restrictions)
  // - some minor arithmetic to hide actual comparison with limit
  #ifdef SYSER_REGISTRATION
  // number of users from license or hardcoded limit, whichever is lower
  sInt32 scrambledlimit = 3*(CONCURRENT_DEVICES_LIMIT!=0 && CONCURRENT_DEVICES_LIMIT<fRegQuantity ? CONCURRENT_DEVICES_LIMIT : fRegQuantity)+42;
  #else
  // only hardcoded limit
  sInt32 scrambledlimit = 3*CONCURRENT_DEVICES_LIMIT+42;
  #endif
  if (
    // compare with hard limit (if zero, count is unlimited)
    ((aSessionCount<<1)+aSessionCount+42>=scrambledlimit && scrambledlimit!=42)
    #ifdef CUSTOMIZABLE_DEVICES_LIMIT
    // compare with configurable limit, if not set to zero (=unlimited)
    || (((aSessionCount<<2) > fConfigP->fConcurrentDeviceLimit*4) && fConfigP->fConcurrentDeviceLimit)
    #endif
  ) {
    // make session busy (not really responding)
    aSessionP->setSessionBusy(true);
    PDEBUGPRINTFX(DBG_HOT,("***** Limit of concurrent sessions reached, server response is 'busy'"));
    // info
    CONSOLEPRINTF(("- concurrent session limit reached, rejecting session with BUSY status"));
  }
} // TSyncAppBase::checkSessionCount

#endif


// factory methods of Rootconfig
// =============================


// Create datatypes registry - default to Multifield-capable version
void TRootConfig::installDatatypesConfig(void)
{
  fDatatypesConfigP = new TMultiFieldDatatypesConfig(this); // default
} // TSyncAppBase::newDatatypesConfig


#ifndef HARDCODED_CONFIG

bool TRootConfig::parseDatatypesConfig(const char **aAttributes, sInt32 aLine)
{
  // currently, only one type of datatype registry exists per app, so we do not
  // need to check attributes
  expectChildParsing(*fDatatypesConfigP);
  return true;
} // TRootConfig::parseDatatypesConfig

#endif


} // namespace sysync



#ifdef CONFIGURABLE_TYPE_SUPPORT

#ifdef MIMEDIR_SUPPORT
  #include "vcarditemtype.h"
  #include "vcalendaritemtype.h"
#endif
#ifdef TEXTTYPE_SUPPORT
  #include "textitemtype.h"
#endif
#ifdef RAWTYPE_SUPPORT
  #include "rawdataitemtype.h"
#endif
#ifdef DATAOBJ_SUPPORT
  #include "dataobjtype.h"
#endif


namespace sysync {

// create new datatype config by name
// returns NULL if none found
TDataTypeConfig *TRootConfig::newDataTypeConfig(const char *aName, const char *aBaseType, TConfigElement *aParentP)
{
  #ifdef MIMEDIR_SUPPORT
  if (strucmp(aBaseType,"mimedir")==0)
    return new TMIMEDirTypeConfig(aName,aParentP);
  else if (strucmp(aBaseType,"vcard")==0)
    return new TVCardTypeConfig(aName,aParentP);
  else if (strucmp(aBaseType,"vcalendar")==0)
    return new TVCalendarTypeConfig(aName,aParentP);
  else
  #endif
  #ifdef TEXTTYPE_SUPPORT
  if (strucmp(aBaseType,"text")==0)
    return new TTextTypeConfig(aName,aParentP);
  else
  #endif
  #ifdef RAWTYPE_SUPPORT
  if (strucmp(aBaseType,"raw")==0)
    return new TRawDataTypeConfig(aName,aParentP);
  else
  #endif
  #ifdef DATAOBJ_SUPPORT
  if (strucmp(aBaseType,"dataobj")==0)
    return new TDataObjConfig(aName,aParentP);
  else
  #endif
    return NULL; // unknown basetype
} // TRootConfig::newDataTypeConfig


// create new profile config by name
// returns NULL if none found
TProfileConfig *TRootConfig::newProfileConfig(const char *aName, const char *aTypeName, TConfigElement *aParentP)
{
  // create profiles by name
  #ifdef MIMEDIR_SUPPORT
  if (strucmp(aTypeName,"mimeprofile")==0)
    return new TMIMEProfileConfig(aName,aParentP);
  else
  #endif
  #ifdef TEXTTYPE_SUPPORT
  if (strucmp(aTypeName,"textprofile")==0)
    return new TTextProfileConfig(aName,aParentP);
  else
  #endif
    return NULL; // unknown profile
} // TRootConfig::newProfileConfig

} // namespace sysync

#endif




// only one of XML2GO or SDK/Plugin can be on top of customagent
#ifdef XML2GO_SUPPORT
  #include "xml2goapiagent.h"
#elif defined(SDK_SUPPORT)
  #include "pluginapiagent.h"
#endif
// ODBC can be in-between if selected
#ifdef SQL_SUPPORT
  #include "odbcapiagent.h"
#endif


namespace sysync {

// Create agent config - default to customImpl based agent
void TRootConfig::installAgentConfig(void)
{
  #if defined(XML2GO_SUPPORT)
  fAgentConfigP = new TXml2goAgentConfig(this); // xml2go (possibly on top of ODBC)
  #elif defined(SDK_SUPPORT)
  fAgentConfigP = new TPluginAgentConfig(this); // plugin/SDK (possibly on top of ODBC)
  #elif defined(SQL_SUPPORT)
  fAgentConfigP = new TOdbcAgentConfig(this); // ODBC only
  #else
  fAgentConfigP = NULL; // none
  #endif
} // TSyncAppBase::installAgentConfig


#ifndef HARDCODED_CONFIG

bool TRootConfig::parseAgentConfig(const char **aAttributes, sInt32 aLine)
{
  const char *typenam = getAttr(aAttributes,"type");
  bool parseit=false;

  #ifdef XML2GO_SUPPORT
  if (strucmp(typenam,"xml2go")==0) parseit=true;
  #endif
  #ifdef SDK_SUPPORT
  if (strucmp(typenam,"plugin")==0) parseit=true;
  #endif
  #ifdef SQL_SUPPORT
  if (strucmp(typenam,"odbc")==0 || strucmp(typenam,"sql")==0) parseit=true;
  #endif
  if (parseit) {
    expectChildParsing(*fAgentConfigP);
  }
  return parseit;
} // TRootConfig::parseAgentConfig

#endif


// Support for SySync Diagnostic Tool
#ifdef SYSYNC_TOOL

#include <errno.h>

// special RTK instance just for translating WBXML to XML


#define GET_XMLOUTINSTANCE(u) ((InstanceID_t)GET_USERDATA(u))

#define ERRCHK(CMDNAME,GENFUNC) { Ret_t r = GENFUNC; if (r!=SML_ERR_OK) CONSOLEPRINTF(("Error converting %s to XML -> XML output truncated!",CMDNAME)); }

// SyncML toolkit callback implementations for sysytool debug decoder

// userData must be instance_id of XML instance to generate XML message into

static Ret_t sysytoolStartMessageCallback(InstanceID_t id, VoidPtr_t userData, SmlSyncHdrPtr_t pContent)
{
  // Note: we must find the SyncML version in advance, as fSyncMLVersion is not yet valid here
  sInt16 hdrVers;
  StrToEnum(SyncMLVerDTDNames,numSyncMLVersions,hdrVers,smlPCDataToCharP(pContent->version));
  smlStartMessageExt(GET_XMLOUTINSTANCE(userData),pContent,SmlVersionCodes[hdrVers]);
  return SML_ERR_OK;
}

static Ret_t sysytoolEndMessageCallback(InstanceID_t id, VoidPtr_t userData, Boolean_t final)
{
  ERRCHK("End Of Message",smlEndMessage(GET_XMLOUTINSTANCE(userData),final));
  return SML_ERR_OK;
}

static Ret_t sysytoolStartSyncCallback(InstanceID_t id, VoidPtr_t userData, SmlSyncPtr_t pContent)
{
  ERRCHK("<Sync>",smlStartSync(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}

static Ret_t sysytoolEndSyncCallback(InstanceID_t id, VoidPtr_t userData)
{
  ERRCHK("</Sync>",smlEndSync(GET_XMLOUTINSTANCE(userData)));
  return SML_ERR_OK;
}


#ifdef ATOMIC_RECEIVE  /* these callbacks are NOT included in the Toolkit lite version */

static Ret_t sysytoolStartAtomicCallback(InstanceID_t id, VoidPtr_t userData, SmlAtomicPtr_t pContent)
{
  ERRCHK("<Atomic>",smlStartAtomic(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}

static Ret_t sysytoolEndAtomicCallback(InstanceID_t id, VoidPtr_t userData)
{
  ERRCHK("</Atomic>",smlEndAtomic(GET_XMLOUTINSTANCE(userData)));
  return SML_ERR_OK;
}

#endif

#ifdef SEQUENCE_RECEIVE

static Ret_t sysytoolStartSequenceCallback(InstanceID_t id, VoidPtr_t userData, SmlSequencePtr_t pContent)
{
  ERRCHK("<Sequence>",smlStartSequence(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}

static Ret_t sysytoolEndSequenceCallback(InstanceID_t id, VoidPtr_t userData)
{
  ERRCHK("</Sequence>",smlEndSequence(GET_XMLOUTINSTANCE(userData)));
  return SML_ERR_OK;
}

#endif

static Ret_t sysytoolAddCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlAddPtr_t pContent)
{
  ERRCHK("<Add>",smlAddCmd(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}

static Ret_t sysytoolAlertCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlAlertPtr_t pContent)
{
  ERRCHK("<Alert>",smlAlertCmd(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}


static Ret_t sysytoolDeleteCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlDeletePtr_t pContent)
{
  ERRCHK("<Delete>",smlDeleteCmd(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}

static Ret_t sysytoolGetCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlGetPtr_t pContent)
{
  ERRCHK("<Get>",smlGetCmd(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}

static Ret_t sysytoolPutCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlPutPtr_t pContent)
{
  ERRCHK("<Put>",smlPutCmd(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}

#ifdef MAP_RECEIVE
static Ret_t sysytoolMapCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlMapPtr_t pContent)
{
  ERRCHK("<Map>",smlMapCmd(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}
#endif

#ifdef RESULT_RECEIVE
static Ret_t sysytoolResultsCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlResultsPtr_t pContent)
{
  ERRCHK("<Results>",smlResultsCmd(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}
#endif

static Ret_t sysytoolStatusCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlStatusPtr_t pContent)
{
  ERRCHK("<Status>",smlStatusCmd(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}

static Ret_t sysytoolReplaceCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlReplacePtr_t pContent)
{
  ERRCHK("<Replace>",smlReplaceCmd(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}

#ifdef COPY_RECEIVE
static Ret_t sysytoolCopyCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlCopyPtr_t pContent)
{
  ERRCHK("<Copy>",smlCopyCmd(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}
#endif

static Ret_t sysytoolMoveCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlMovePtr_t pContent)
{
  ERRCHK("<Move>",smlMoveCmd(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}

#ifdef EXEC_RECEIVE
static Ret_t sysytoolExecCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlExecPtr_t pContent)
{
  ERRCHK("<Exec>",smlExecCmd(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}
#endif


#ifdef SEARCH_RECEIVE
static Ret_t sysytoolSearchCmdCallback(InstanceID_t id, VoidPtr_t userData, SmlSearchPtr_t pContent)
{
  ERRCHK("<Search>",smlSearchCmd(GET_XMLOUTINSTANCE(userData),pContent));
  return SML_ERR_OK;
}
#endif


static Ret_t sysytoolHandleErrorCallback(InstanceID_t id, VoidPtr_t userData)
{
  return SML_ERR_OK;
}

static Ret_t sysytoolTransmitChunkCallback(InstanceID_t id, VoidPtr_t userData)
{
  return SML_ERR_INVALID_OPTIONS;
}



static const SmlCallbacks_t sysyncToolCallbacks = {
  /* message callbacks */
  sysytoolStartMessageCallback,
  sysytoolEndMessageCallback,
  /* grouping commands */
  sysytoolStartSyncCallback,
  sysytoolEndSyncCallback,
  #ifdef ATOMIC_RECEIVE  /* these callbacks are NOT included in the Toolkit lite version */
    sysytoolStartAtomicCallback,
    sysytoolEndAtomicCallback,
  #endif
  #ifdef SEQUENCE_RECEIVE
    sysytoolStartSequenceCallback,
    sysytoolEndSequenceCallback,
  #endif
  /* Sync Commands */
  sysytoolAddCmdCallback,
  sysytoolAlertCmdCallback,
  sysytoolDeleteCmdCallback,
  sysytoolGetCmdCallback,
  sysytoolPutCmdCallback,
  #ifdef MAP_RECEIVE
    sysytoolMapCmdCallback,
  #endif
  #ifdef RESULT_RECEIVE
    sysytoolResultsCmdCallback,
  #endif
  sysytoolStatusCmdCallback,
  sysytoolReplaceCmdCallback,
  /* other commands */
  #ifdef COPY_RECEIVE  /* these callbacks are NOT included in the Toolkit lite version */
    sysytoolCopyCmdCallback,
  #endif
  #ifdef EXEC_RECEIVE
    sysytoolExecCmdCallback,
  #endif
  #ifdef SEARCH_RECEIVE
    sysytoolSearchCmdCallback,
  #endif
  sysytoolMoveCmdCallback,
  /* Other Callbacks */
  sysytoolHandleErrorCallback,
  sysytoolTransmitChunkCallback
}; /* sml_callbacks struct */


// WBXML to XML conversion
int wbxmlConv(int argc, const char *argv[])
{
  if (argc<0) {
    // help requested
    CONSOLEPRINTF(("  wbxml2xml <wbxml binary message file> [<xml output file>]"));
    CONSOLEPRINTF(("    Converts to XML using SyncML-Toolkit"));
    CONSOLEPRINTF(("    If no output file is specified, input file name with suffix '.xml' is used"));
    return EXIT_SUCCESS;
  }

  // check for argument
  if (argc<1 || argc>2) {
    CONSOLEPRINTF(("1 or 2 arguments required"));
    return EXIT_FAILURE;
  }

  // open input file
  FILE *inFile = fopen(argv[0],"rb");
  if (!inFile) {
    CONSOLEPRINTF(("Error opening input file '%s', error=%d",argv[0],errno));
    return EXIT_FAILURE;
  }

  // prepare a instance for decoding the WBXML
  InstanceID_t wbxmlInInstance;
  if (!getSyncAppBase()->newSmlInstance(
    SML_WBXML,
    500*1024, // 500k should be waaaaay enough
    wbxmlInInstance
  )) {
    CONSOLEPRINTF(("Error creating WBXML parser"));
    return EXIT_FAILURE;
  }
  // install the sysytool callbacks (default are the normal appbase ones)
  smlSetCallbacks(wbxmlInInstance, &sysyncToolCallbacks);

  // prepare instance for generating XML output
  InstanceID_t xmlOutInstance;
  if (!getSyncAppBase()->newSmlInstance(
    SML_XML,
    500*1024, // 500k should be waaaaay enough
    xmlOutInstance
  )) {
    CONSOLEPRINTF(("Error creating XML generator"));
    return EXIT_FAILURE;
  }
  // link output instance as userdata into input instance
  getSyncAppBase()->setSmlInstanceUserData(wbxmlInInstance, xmlOutInstance);

  // read WBXML original file into workspace
  MemPtr_t wbxmlBufP;
  MemSize_t wbxmlBufSiz, wbxmlDataSiz;
  Ret_t rc;
  rc = smlLockWriteBuffer(wbxmlInInstance, &wbxmlBufP, &wbxmlBufSiz);
  if (rc!=SML_ERR_OK) {
    CONSOLEPRINTF(("Error getting WBXML buffer, err=%d",rc));
    return EXIT_FAILURE;
  }
  wbxmlDataSiz = fread(wbxmlBufP,1,wbxmlBufSiz,inFile);
  if (wbxmlDataSiz==0) {
    CONSOLEPRINTF(("No data in WBXML input file or error, err=%d",errno));
    return EXIT_FAILURE;
  }
  fclose(inFile);
  CONSOLEPRINTF(("Read %ld bytes from WBXML input file '%s'",wbxmlDataSiz,argv[0]));
  smlUnlockWriteBuffer(wbxmlInInstance,wbxmlDataSiz);

  // decode
  do {
    rc = smlProcessData(wbxmlInInstance, SML_NEXT_COMMAND);
  } while (rc==SML_ERR_CONTINUE);
  if (rc!=SML_ERR_OK) {
    CONSOLEPRINTF(("Error while decoding WBXML document, rc=%d",rc));
  }
  else {
    CONSOLEPRINTF(("Successfully completed decoding WBXML document"));
  }

  // write to output file
  // - determine name
  string outFileName;
  outFileName = argv[0];
  if (argc>1)
    outFileName = argv[1];
  else
    outFileName += ".xml";
  // save output
  FILE *outFile = fopen(outFileName.c_str(),"w");
  if (!outFile) {
    CONSOLEPRINTF(("Error opening output file '%s', error=%d",outFileName.c_str(),errno));
    return EXIT_FAILURE;
  }
  MemPtr_t xmlBufP;
  MemSize_t xmlDataSiz;
  rc = smlLockReadBuffer(xmlOutInstance,&xmlBufP,&xmlDataSiz);
  if (rc!=SML_ERR_OK) {
    CONSOLEPRINTF(("Error reading decoded XML from converter WBXML document, rc=%d",rc));
    return EXIT_FAILURE;
  }
  CONSOLEPRINTF(("Writing %ld bytes of XML translation to '%s'",xmlDataSiz,outFileName.c_str()));
  fwrite(xmlBufP,1,xmlDataSiz,outFile);
  fclose(outFile);
  // done
  return EXIT_SUCCESS;
} // wbxmlConv

#endif // SYSYNC_TOOL



} // namespace sysync

// eof
