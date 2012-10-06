/*
 *  TSyncSession
 *    Represents an entire Synchronisation Session, possibly consisting
 *    of multiple SyncML-Toolkit "Sessions" (Message composition/de-
 *    composition) as well as multiple database synchronisations.
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-05-07 : luz : Created
 *
 */


#include "prefix_file.h"

#include "sysync.h"
#include "syncsession.h"
#include "syncagent.h"
#ifdef SUPERDATASTORES
  #include "superdatastore.h"
#endif
#ifdef SCRIPT_SUPPORT
  #include "scriptcontext.h"
#endif
#ifdef MULTI_THREAD_SUPPORT
  #include "platform_thread.h"
#endif

#include <xltdec.h>
#include <xltdevinf.h>

#ifndef SYNCSESSION_PART1_EXCLUDE

namespace sysync {

// enum names
// ----------

// SyncML version info
const char * const SyncMLVerProtoNames[numSyncMLVersions] = {
  "undefined",
  "SyncML/1.0",
  "SyncML/1.1",
  "SyncML/1.2"
};
const SmlVersion_t SmlVersionCodes[numSyncMLVersions] = {
  SML_VERS_UNDEF,
  SML_VERS_1_0,
  SML_VERS_1_1,
  SML_VERS_1_2
};
const char * const SyncMLVerDTDNames[numSyncMLVersions] = {
  "???",
  "1.0",
  "1.1",
  "1.2"
};
const char * const SyncMLDevInfNames[numSyncMLVersions] = {
  NULL,
  "./devinf10",
  "./devinf11",
  "./devinf12"
};
#ifndef HARDCODED_CONFIG
// version for use in config files
const char * const SyncMLVersionNames[numSyncMLVersions] = {
  "unknown",
  "1.0",
  "1.1",
  "1.2"
};
#endif


// auth type names for config
const char * const authTypeNames[numAuthTypes] = {
  "none",       // no authorisation
  "basic",      // basic (B64 encoded user pw string)
  "md5",        // Md5 encoded user:pw:nonce
};


// sync mode names
const char * const SyncModeNames[numSyncModes] = {
  "twoway",
  "fromserver",
  "fromclient"
};



#ifdef SYDEBUG
// package state names
const char * const PackageStateNames[numPackageStates] = {
  "idle",
  "init",
  "sync",
  "initsync",
  "map",
  "supplement"
};

// sync operations
const char * const SyncOpNames[numSyncOperations] = {
  "wants-add",
  "add",
  "wants-replace",
  "replace",
  "reference-only",
  "archive+delete",
  "soft-delete",
  "delete",
  "copy",
  "move",
  "[none]" // should be last
};

#endif

// sync mode descriptions
const char * const SyncModeDescriptions[numSyncModes] = {
  "two-way",
  "from server only",
  "from client only"
};


#ifdef SCRIPT_SUPPORT

// builtin functions for status-handling scripts

// integer STATUS()
static void func_Status(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TErrorFuncContext *errctxP = static_cast<TErrorFuncContext *>(aFuncContextP->getCallerContext());
  aTermP->setAsInteger(
    errctxP->statuscode
  );
} // func_Status


// void SETSTATUS(integer statuscode)
static void func_SetStatus(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TErrorFuncContext *errctxP = static_cast<TErrorFuncContext *>(aFuncContextP->getCallerContext());
  errctxP->newstatuscode=
    aFuncContextP->getLocalVar(0)->getAsInteger();
} // func_SetStatus


// void SETRESEND(boolean doresend)
static void func_SetResend(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TErrorFuncContext *errctxP = static_cast<TErrorFuncContext *>(aFuncContextP->getCallerContext());
  errctxP->resend=
    aFuncContextP->getLocalVar(0)->getAsBoolean();
} // func_SetResend


// void ABORTDATASTORE(integer statuscode)
static void func_AbortDatastore(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TErrorFuncContext *errctxP = static_cast<TErrorFuncContext *>(aFuncContextP->getCallerContext());
  if (errctxP->datastoreP) {
    errctxP->datastoreP->engAbortDataStoreSync(aFuncContextP->getLocalVar(0)->getAsInteger(),true); // we cause the abort locally
  }
} // func_AbortDatastore


// void STOPADDING()
static void func_StopAdding(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TErrorFuncContext *errctxP = static_cast<TErrorFuncContext *>(aFuncContextP->getCallerContext());
  if (errctxP->datastoreP) {
    errctxP->datastoreP->engStopAddingToRemote();
  }
} // func_StopAdding


// string SYNCOP()
// returns sync-operation as text
static void func_SyncOp(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TErrorFuncContext *errctxP = static_cast<TErrorFuncContext *>(aFuncContextP->getCallerContext());
  aTermP->setAsString(
    SyncOpNames[errctxP->syncop]
  );
} // func_SyncOp


const uInt8 param_OneInteger[] = { VAL(fty_integer) };
const uInt8 param_TwoIntegers[] = { VAL(fty_integer), VAL(fty_integer) };
const uInt8 param_OneString[] = { VAL(fty_string) };

const TBuiltInFuncDef ErrorFuncDefs[] = {
  { "STATUS", func_Status, fty_integer, 0, NULL },
  { "SETSTATUS", func_SetStatus, fty_none, 1, param_OneInteger },
  { "SETRESEND", func_SetResend, fty_none, 1, param_OneInteger },
  { "ABORTDATASTORE", func_AbortDatastore, fty_none, 1, param_OneInteger },
  { "STOPADDING", func_StopAdding, fty_none, 0, NULL },
  { "SYNCOP", func_SyncOp, fty_string, 0, NULL },
};

const TFuncTable ErrorFuncTable = {
  sizeof(ErrorFuncDefs) / sizeof(TBuiltInFuncDef), // size of table
  ErrorFuncDefs, // table pointer
  NULL // no chain func
};


// void SETSTATUS(integer statuscode)
static void func_GetPutResSetStatus(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TGetPutResultFuncContext *gprctxP = static_cast<TGetPutResultFuncContext *>(aFuncContextP->getCallerContext());
  gprctxP->statuscode=
    aFuncContextP->getLocalVar(0)->getAsInteger();
} // func_GetPutResSetStatus


// integer ISPUT()
static void func_IsPut(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TGetPutResultFuncContext *gprctxP = static_cast<TGetPutResultFuncContext *>(aFuncContextP->getCallerContext());
  aTermP->setAsBoolean(gprctxP->isPut);
} // func_IsPut


// string ITEMURI()
static void func_ItemURI(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TGetPutResultFuncContext *gprctxP = static_cast<TGetPutResultFuncContext *>(aFuncContextP->getCallerContext());
  aTermP->setAsString(gprctxP->itemURI);
} // func_ItemURI


// void SETITEMURI(string data)
static void func_SetItemURI(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TGetPutResultFuncContext *gprctxP = static_cast<TGetPutResultFuncContext *>(aFuncContextP->getCallerContext());
  aFuncContextP->getLocalVar(0)->getAsString(gprctxP->itemURI);
} // func_SetItemURI


// string ITEMDATA()
static void func_ItemData(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TGetPutResultFuncContext *gprctxP = static_cast<TGetPutResultFuncContext *>(aFuncContextP->getCallerContext());
  aTermP->setAsString(gprctxP->itemData);
} // func_ItemData


// void SETITEMDATA(string data)
static void func_SetItemData(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TGetPutResultFuncContext *gprctxP = static_cast<TGetPutResultFuncContext *>(aFuncContextP->getCallerContext());
  aFuncContextP->getLocalVar(0)->getAsString(gprctxP->itemData);
} // func_SetItemData


// string METATYPE()
static void func_MetaType(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TGetPutResultFuncContext *gprctxP = static_cast<TGetPutResultFuncContext *>(aFuncContextP->getCallerContext());
  aTermP->setAsString(gprctxP->metaType);
} // func_MetaType


// void SETMETATYPE(string data)
static void func_SetMetaType(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TGetPutResultFuncContext *gprctxP = static_cast<TGetPutResultFuncContext *>(aFuncContextP->getCallerContext());
  aFuncContextP->getLocalVar(0)->getAsString(gprctxP->metaType);
} // func_SetMetaType


// void ISSUEPUT(boolean allowFailure, boolean noResp)
// use ITEMURI, ITEMDATA and METATYPE to issue a PUT command
static void func_IssuePut(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TGetPutResultFuncContext *gprctxP = static_cast<TGetPutResultFuncContext *>(aFuncContextP->getCallerContext());
  if (gprctxP->canIssue) {
    TPutCommand *putcommandP = new TPutCommand(aFuncContextP->getSession());
    putcommandP->setMeta(newMetaType(gprctxP->metaType.c_str()));
    SmlItemPtr_t putItemP = putcommandP->addSourceLocItem(gprctxP->itemURI.c_str());
    // - add data to item
    putItemP->data = newPCDataString(gprctxP->itemData);
    // issue it
    if (aFuncContextP->getLocalVar(0)->getAsBoolean()) putcommandP->allowFailure(); // allow failure (4xx or 5xx status)
    aFuncContextP->getSession()->issueRootPtr(putcommandP,aFuncContextP->getLocalVar(1)->getAsBoolean());
  }
} // func_IssuePut


// void ISSUEGET(boolean allowFailure)
// use ITEMURI and METATYPE to issue a GET command
static void func_IssueGet(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TGetPutResultFuncContext *gprctxP = static_cast<TGetPutResultFuncContext *>(aFuncContextP->getCallerContext());
  if (gprctxP->canIssue) {
    TGetCommand *getcommandP = new TGetCommand(aFuncContextP->getSession());
    getcommandP->addTargetLocItem(gprctxP->itemURI.c_str());
    getcommandP->setMeta(newMetaType(gprctxP->metaType.c_str()));
    // issue it
    if (aFuncContextP->getLocalVar(0)->getAsBoolean()) getcommandP->allowFailure(); // allow failure (4xx or 5xx status)
    aFuncContextP->getSession()->issueRootPtr(getcommandP,false); // get with noResp does not make sense
  }
} // func_IssueGet


// void ISSUEALERT(boolean allowFailure, integer alertcode)
// use ITEMDATA to add an Alert item
static void func_IssueAlert(TItemField *&aTermP, TScriptContext *aFuncContextP)
{
  TGetPutResultFuncContext *gprctxP = static_cast<TGetPutResultFuncContext *>(aFuncContextP->getCallerContext());
  if (gprctxP->canIssue) {
    uInt16 alertcode = aFuncContextP->getLocalVar(1)->getAsInteger();
    TAlertCommand *alertCommandP = new TAlertCommand(aFuncContextP->getSession(),NULL,alertcode);
    // - add string data item
    alertCommandP->addItem(newStringDataItem(gprctxP->itemData.c_str()));
    // issue it
    if (aFuncContextP->getLocalVar(0)->getAsBoolean()) alertCommandP->allowFailure(); // allow failure (4xx or 5xx status)
    aFuncContextP->getSession()->issueRootPtr(alertCommandP,false); // Alert with noResp not supported
  }
} // func_IssueAlert




const TBuiltInFuncDef GetPutResultFuncDefs[] = {
  { "SETSTATUS", func_GetPutResSetStatus, fty_none, 1, param_OneInteger },
  { "ISPUT", func_IsPut, fty_integer, 0, NULL },
  { "ITEMURI", func_ItemURI, fty_string, 0, NULL },
  { "SETITEMURI", func_SetItemURI, fty_none, 1, param_OneString },
  { "ITEMDATA", func_ItemData, fty_string, 0, NULL },
  { "SETITEMDATA", func_SetItemData, fty_none, 1, param_OneString },
  { "METATYPE", func_MetaType, fty_string, 0, NULL },
  { "SETMETATYPE", func_SetMetaType, fty_none, 1, param_OneString },
  { "ISSUEPUT", func_IssuePut, fty_none, 2, param_TwoIntegers },
  { "ISSUEGET", func_IssueGet, fty_none, 1, param_OneInteger },
  { "ISSUEALERT", func_IssueAlert, fty_none, 2, param_TwoIntegers }
};

const TFuncTable GetPutResultFuncTable = {
  sizeof(GetPutResultFuncDefs) / sizeof(TBuiltInFuncDef), // size of table
  GetPutResultFuncDefs, // table pointer
  NULL // no chain func
};



#endif


#ifndef NO_REMOTE_RULES

// Remote Rule Config
// ==================

#define DONT_REJECT 0xFFFF

// config constructor
TRemoteRuleConfig::TRemoteRuleConfig(const char *aElementName, TConfigElement *aParentElementP) :
  TConfigElement(aElementName,aParentElementP)
{
  clear();
} // TRemoteRuleConfig::TRemoteRuleConfig


// config destructor
TRemoteRuleConfig::~TRemoteRuleConfig()
{
  if (fOverrideDevInfBufferP)
    smlFreeProtoElement(fOverrideDevInfBufferP);

  clear();
} // TRemoteRuleConfig::~TRemoteRuleConfig


// init defaults
void TRemoteRuleConfig::clear(void)
{
  // init defaults
  // - id
  fManufacturer.erase();
  fModel.erase();
  fOem.erase();
  fFirmwareVers.erase();
  fSoftwareVers.erase();
  fHardwareVers.erase();
  fDevId.erase();
  fDevTyp.erase();
  // - options
  fRejectStatusCode=DONT_REJECT; // not rejected
  fLegacyMode=-1; // set if remote is known legacy, so don't use new types
  fLenientMode=-1; // set if remote's SyncML should be handled leniently, i.e. not too strict checking where not absolutely needed
  fLimitedFieldLengths=-1; // set if remote has limited field lengths
  fDontSendEmptyProperties=-1; // set if remote does not want empty properties
  fDoQuote8BitContent=-1; // normally, only use QP for contents with EOLNs in vCard 2.1
  fDoNotFoldContent=-1; // normally, content must be folded in MIME-DIR
  fNoReplaceInSlowsync=-1; // normally, we are allowed to use Replace (as server) in slow sync
  fTreatRemoteTimeAsLocal=-1; // do not ignore time zone
  fTreatRemoteTimeAsUTC=-1; // do not ignore time zone
  fVCal10EnddatesSameDay=-1; // use default end date rendering
  fIgnoreDevInfMaxSize=-1; // do not ignore max field size in remote's devInf
  fIgnoreCTCap=-1; // do not ignore CTCap
  fDSPathInDevInf=-1; // use actual DS path as used in Alert for creating datastore devInf (needed for newer Nokia clients)
  fDSCgiInDevInf=-1; // also show CGI as used in Alert for creating datastore devInf (needed for newer Nokia clients)
  fForceUTC=-1; // automatic decision based on DevInf (SyncML 1.1) or just UTC for SyncML 1.0
  fForceLocaltime=-1;
  fTreatCopyAsAdd=-1;
  fCompleteFromClientOnly=-1;
  fRequestMaxTime=-1; // not defined
  fDefaultOutCharset=chs_unknown; // do not set the default output charset
  fDefaultInCharset=chs_unknown; // do not set the default input interpretation charset
  // - options that also have a configurable session default
  fUpdateClientDuringSlowsync=-1;
  fUpdateServerDuringSlowsync=-1;
  fAllowMessageRetries=-1;
  fStrictExecOrdering=-1;
  #ifndef MINIMAL_CODE
  fRemoteDescName.erase();
  #endif
  fSubRulesList.clear(); // no included subrules
  fSubRule = false; // normal rule by default
  // - rules are final by default
  fFinalRule = true;
  // no DevInf by default
  fOverrideDevInfP = NULL;
  fOverrideDevInfBufferP = NULL;
  // clear inherited
  inherited::clear();
} // TRemoteRuleConfig::clear


#ifndef HARDCODED_CONFIG

// remote rule config element parsing
bool TRemoteRuleConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  // - identification of remote (irrelevant for subrules)
  if (!fSubRule && strucmp(aElementName,"manufacturer")==0)
    expectString(fManufacturer);
  else if (!fSubRule && strucmp(aElementName,"model")==0)
    expectString(fModel);
  else if (!fSubRule && strucmp(aElementName,"oem")==0)
    expectString(fOem);
  else if (!fSubRule && strucmp(aElementName,"firmware")==0)
    expectString(fFirmwareVers);
  else if (!fSubRule && strucmp(aElementName,"software")==0)
    expectString(fSoftwareVers);
  else if (!fSubRule && strucmp(aElementName,"hardware")==0)
    expectString(fHardwareVers);
  else if (!fSubRule && strucmp(aElementName,"deviceid")==0)
    expectString(fDevId);
  else if (!fSubRule && strucmp(aElementName,"devicetype")==0)
    expectString(fDevTyp);
  // - options
  else if (strucmp(aElementName,"legacymode")==0)
    expectTristate(fLegacyMode);
  else if (strucmp(aElementName,"lenientmode")==0)
    expectTristate(fLenientMode);
  else if (strucmp(aElementName,"limitedfieldlengths")==0)
    expectTristate(fLimitedFieldLengths);
  else if (strucmp(aElementName,"noemptyproperties")==0)
    expectTristate(fDontSendEmptyProperties);
  else if (strucmp(aElementName,"quote8bitcontent")==0)
    expectTristate(fDoQuote8BitContent);
  else if (strucmp(aElementName,"nocontentfolding")==0)
    expectTristate(fDoNotFoldContent);
  else if (strucmp(aElementName,"noreplaceinslowsync")==0)
    expectTristate(fNoReplaceInSlowsync);
  else if (strucmp(aElementName,"treataslocaltime")==0)
    expectTristate(fTreatRemoteTimeAsLocal);
  else if (strucmp(aElementName,"treatasutc")==0)
    expectTristate(fTreatRemoteTimeAsUTC);
  else if (strucmp(aElementName,"autoenddateinclusive")==0)
    expectTristate(fVCal10EnddatesSameDay);
  else if (strucmp(aElementName,"ignoredevinfmaxsize")==0)
    expectTristate(fIgnoreDevInfMaxSize);
  else if (strucmp(aElementName,"ignorectcap")==0)
    expectTristate(fIgnoreCTCap);
  else if (strucmp(aElementName,"dspathindevinf")==0)
    expectTristate(fDSPathInDevInf);
  else if (strucmp(aElementName,"dscgiindevinf")==0)
    expectTristate(fDSCgiInDevInf);
  else if (strucmp(aElementName,"updateclientinslowsync")==0)
    expectTristate(fUpdateClientDuringSlowsync);
  else if (strucmp(aElementName,"updateserverinslowsync")==0)
    expectTristate(fUpdateServerDuringSlowsync);
  else if (strucmp(aElementName,"allowmessageretries")==0)
    expectTristate(fAllowMessageRetries);
  else if (strucmp(aElementName,"strictexecordering")==0)
    expectTristate(fStrictExecOrdering);
  else if (strucmp(aElementName,"treatcopyasadd")==0)
    expectTristate(fTreatCopyAsAdd);
  else if (strucmp(aElementName,"completefromclientonly")==0)
    expectTristate(fCompleteFromClientOnly);
  else if (strucmp(aElementName,"requestmaxtime")==0)
    expectInt32(fRequestMaxTime);
  else if (strucmp(aElementName,"outputcharset")==0)
    expectEnum(sizeof(fDefaultOutCharset),&fDefaultOutCharset,MIMECharSetNames,numCharSets);
  else if (strucmp(aElementName,"inputcharset")==0)
    expectEnum(sizeof(fDefaultInCharset),&fDefaultInCharset,MIMECharSetNames,numCharSets);
  else if (strucmp(aElementName,"rejectstatus")==0)
    expectUInt16(fRejectStatusCode);
  else if (strucmp(aElementName,"forceutc")==0)
    expectTristate(fForceUTC);
  else if (strucmp(aElementName,"forcelocaltime")==0)
    expectTristate(fForceLocaltime);
  else if (strucmp(aElementName,"overridedevinf")==0)
    expectString(fOverrideDevInfXML);
  // inclusion of subrules
  else if (strucmp(aElementName,"include")==0) {
    // <include rule=""/>
    expectEmpty();
    const char* nam = getAttr(aAttributes,"rule");
    if (!nam)
      return fail("<include> must specify \"rule\"");
    else {
      // find rule
      TRemoteRulesList::iterator pos;
      TSessionConfig *scfgP = static_cast<TSessionConfig *>(getParentElement());
      for(pos=scfgP->fRemoteRulesList.begin();pos!=scfgP->fRemoteRulesList.end();pos++) {
        if (strucmp(nam,(*pos)->getName())==0) {
          fSubRulesList.push_back(*pos);
          return true; // done
        }
      }
      return fail("rule '%s' for <include> not found (must be defined before included)",nam);
    }
  }
  // rule script. Note that this is special, as it is NOT resolved in the config, but
  // copied to the session first, as it might differ between sessions.
  #ifdef SCRIPT_SUPPORT
  else if (strucmp(aElementName,"rulescript")==0)
    expectScript(fRuleScriptTemplate,aLine,NULL,true); // late binding, no declarations allowed
  #endif
  #ifndef MINIMAL_CODE
  else if (strucmp(aElementName,"descriptivename")==0)
    expectString(fRemoteDescName);
  #endif
  // - final rule?
  else if (strucmp(aElementName,"finalrule")==0)
    expectBool(fFinalRule);
  // - not known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TRemoteRuleConfig::localStartElement

#endif // HARDCODED_CONFIG

#endif // NO_REMOTE_RULES


#endif // not SYNCSESSION_PART1_EXCLUDE
#ifndef SYNCSESSION_PART2_EXCLUDE


// Session Config
// ==============


// config constructor
TSessionConfig::TSessionConfig(const char *aElementName, TConfigElement *aParentElementP) :
  inherited(aElementName,aParentElementP)
{
  clear();
} // TSessionConfig::TSessionConfig


// config destructor
TSessionConfig::~TSessionConfig()
{
  clear();
} // TSessionConfig::~TSessionConfig


// init defaults
void TSessionConfig::clear(void)
{
  // init defaults
  #ifndef NO_REMOTE_RULES
  // - no remote rules
  TRemoteRulesList::iterator pos;
  for(pos=fRemoteRulesList.begin();pos!=fRemoteRulesList.end();pos++)
    delete *pos;
  fRemoteRulesList.clear();
  #endif
  // remove datastores
  TLocalDSList::iterator pos2;
  for(pos2=fDatastores.begin();pos2!=fDatastores.end();pos2++)
    delete *pos2;
  fDatastores.clear();
  // - no simple auth
  fSimpleAuthUser.erase();
  fSimpleAuthPassword.erase();
  // - medium timeout
  fSessionTimeout=60; // one minute, will be overridden by derived classes
  // - set default maximum SyncML version enabled
  fMaxSyncMLVersionSupported=MAX_SYNCML_VERSION;
  // - minimum is 1.0
  fMinSyncMLVersionSupported=syncml_vers_1_0;
  // - accept server-alerted codes by default
  fAcceptServerAlerted=true;
  // - defaults for remote-rule configurable behaviour
  fUpdateClientDuringSlowsync=false; // do not update client records during slowsync (but do it for first sync!)
  fUpdateServerDuringSlowsync=false; // do not update server records during NON-FIRST-TIME slowsync (but do it for first sync!)
  fAllowMessageRetries=true; // generally allow retries
  fCompleteFromClientOnly=false; // default to standard-compliant behaviour.
  fRequestMaxTime=0; // no limit by default
  fRequestMinTime=0; // no minimal request processing delay
  // - default value for flag to send property lists in CTCap
  fShowCTCapProps=true;
  // - default value for flag to send type/size in CTCap for SyncML 1.0 (disable as old clients like S55 crash on this)
  fShowTypeSzInCTCap10=false;
  if (IS_CLIENT) {
    // - Synthesis clients always behaved like that (sending 23:59:59), so we'll keep it as a default
    fVCal10EnddatesSameDay = true;
  }
  else {
    // - Many modern clients need the exclusive format (start of next day) to detect all-day events properly.
    //   Synthesis clients detect these fine as well, so not using 23:59:59 style by default is more
    //   compatible in general for a server.
    fVCal10EnddatesSameDay = false;
  }
  // traditionally Synthesis has folded content
  fDoNotFoldContent = false;
  // - default value for flag is "default" (depends on SyncML version)
  fEnumDefaultPropParams=-1;
  // - decide, whether multi-threading for the datastores will be used:
  //   As there are some problems with older Linux versions (e.g. Debian 3.0r2 stable) the
  //   default values are set for downwards compatibility Linux=false / all others=true
  // Multithreading can be switched of either by #define or by <fMultiThread> flag
  #if defined LINUX || !defined MULTI_THREAD_DATASTORE
    fMultiThread= false;
  #else
    fMultiThread= true;
  #endif
  // - do not wait for status of interrupted command by default (note: before 2.1.0.2, this was always true)
  fWaitForStatusOfInterrupted=false;
  // - accept delete commands for already deleted items with 200 (rather that 404 or 211)
  #ifdef SCTS_COMPATIBILITY_HACKS
  fDeletingGoneOK=false; // SCTS needs that
  #else
  fDeletingGoneOK=true; // makes more sense as it avoids unnecessary session aborts
  #endif
  // - abort if all items sent to remote fail
  fAbortOnAllItemsFailed=true; // note: does only apply in slow syncs now!
  // - default to system time
  fUserTimeContext=TCTX_SYSTEM;
  #ifdef SCRIPT_SUPPORT
  // - session init script
  fSessionInitScript.erase();
  // - status handling scripts
  fSentItemStatusScript.erase();
  fReceivedItemStatusScript.erase();
  // - session termination script
  fSessionFinishScript.erase();
  // - custom get handler
  fCustomGetHandlerScript.erase();
  // - custom get and put generators
  fCustomGetPutScript.erase();
  fCustomEndPutScript.erase();
  // - custom PUT and RESULT handler
  fCustomPutResultHandlerScript.erase();
  #endif
  #ifndef MINIMAL_CODE
  // - logfile
  fLogFileName.erase();
  if (IS_SERVER) {
    fLogFileFormat.assign(DEFAULT_LOG_FORMAT_SERVER);
    fLogFileLabels.assign(DEFAULT_LOG_LABELS_SERVER);
  }
  else {
    fLogFileFormat.assign(DEFAULT_LOG_FORMAT_CLIENT);
    fLogFileLabels.assign(DEFAULT_LOG_LABELS_CLIENT);
  }
  fLogEnabled=true;
  fDebugChunkMaxSize=0; // disabled
  #endif
  fRelyOnEarlyMaps=true; // we rely on early maps sent by clients for adds from the previous session
  // clear inherited
  inherited::clear();
} // TSessionConfig::clear


// get local DS config pointer by database name or dbTypeID
TLocalDSConfig *TSessionConfig::getLocalDS(const char *aName, uInt32 aDBTypeID)
{
  TLocalDSList::iterator pos;
  for(pos=fDatastores.begin();pos!=fDatastores.end();pos++) {
    if (aName==NULL) {
      if ((*pos)->fLocalDBTypeID==aDBTypeID) return *pos; // found by DBTypeID
    }
    else {
      if (strucmp((*pos)->getName(),aName)==0) return *pos; // found by name
    }
  }
  return NULL; // not found
} // TSessionConfig::getLocalDS


#ifndef HARDCODED_CONFIG

// server config element parsing
bool TSessionConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  #ifndef NO_REMOTE_RULES
  bool isSubRule = strucmp(aElementName,"subrule")==0;
  if (strucmp(aElementName,"remoterule")==0 || isSubRule) {
    // check for optional name attribute
    const char* nam = getAttr(aAttributes,"name");
    if (!nam) nam="unnamed";
    // create rule
    TRemoteRuleConfig *ruleP = new TRemoteRuleConfig(nam,this);
    ruleP->fSubRule = isSubRule;
    fRemoteRulesList.push_back(ruleP);
    expectChildParsing(*ruleP);
  }
  else
  #endif
  if (strucmp(aElementName,"sessiontimeout")==0)
    expectInt32(fSessionTimeout);
  else if (strucmp(aElementName,"requestmaxtime")==0)
    expectUInt32(fRequestMaxTime);
  else if (strucmp(aElementName,"requestmintime")==0)
    expectInt32(fRequestMinTime);
  else if (strucmp(aElementName,"simpleauthuser")==0)
    expectString(fSimpleAuthUser);
  else if (strucmp(aElementName,"simpleauthpw")==0)
    expectString(fSimpleAuthPassword);
  else if (strucmp(aElementName,"maxsyncmlversion")==0)
    expectEnum(sizeof(fMaxSyncMLVersionSupported),&fMaxSyncMLVersionSupported,SyncMLVersionNames,numSyncMLVersions);
  else if (strucmp(aElementName,"minsyncmlversion")==0)
    expectEnum(sizeof(fMinSyncMLVersionSupported),&fMinSyncMLVersionSupported,SyncMLVersionNames,numSyncMLVersions);
  else if (strucmp(aElementName,"acceptserveralerted")==0)
    expectBool(fAcceptServerAlerted);
  else if (strucmp(aElementName,"updateclientinslowsync")==0)
    expectBool(fUpdateClientDuringSlowsync);
  else if (strucmp(aElementName,"updateserverinslowsync")==0)
    expectBool(fUpdateServerDuringSlowsync);
  else if (strucmp(aElementName,"completefromclientonly")==0)
    expectBool(fCompleteFromClientOnly);
  else if (strucmp(aElementName,"allowmessageretries")==0)
    expectBool(fAllowMessageRetries);
  else if (strucmp(aElementName,"multithread")==0)
    expectBool(fMultiThread);
  else if (strucmp(aElementName,"waitforstatusofinterrupted")==0)
    expectBool(fWaitForStatusOfInterrupted);
  else if (strucmp(aElementName,"deletinggoneok")==0)
    expectBool(fDeletingGoneOK);
  else if (strucmp(aElementName,"abortonallitemsfailed")==0)
    expectBool(fAbortOnAllItemsFailed);
  else if (strucmp(aElementName,"showctcapproperties")==0)
    expectBool(fShowCTCapProps);
  else if (strucmp(aElementName,"showtypesizeinctcap10")==0)
    expectBool(fShowTypeSzInCTCap10);
  else if (strucmp(aElementName,"autoenddateinclusive")==0)
    expectBool(fVCal10EnddatesSameDay);
  else if (strucmp(aElementName,"donotfoldcontent")==0)
    expectBool(fDoNotFoldContent);
  else if (strucmp(aElementName,"enumdefaultpropparams")==0)
    expectTristate(fEnumDefaultPropParams); // Tristate!!!
  else if (strucmp(aElementName,"usertimezone")==0)
    expectTimezone(fUserTimeContext);
  #ifdef SCRIPT_SUPPORT
  else if (strucmp(aElementName,"sessioninitscript")==0)
    expectScript(fSessionInitScript,aLine,NULL);
  else if (strucmp(aElementName,"sentitemstatusscript")==0)
    expectScript(fSentItemStatusScript,aLine,&ErrorFuncTable);
  else if (strucmp(aElementName,"receiveditemstatusscript")==0)
    expectScript(fReceivedItemStatusScript,aLine,&ErrorFuncTable);
  else if (strucmp(aElementName,"sessionfinishscript")==0)
    expectScript(fSessionFinishScript,aLine,NULL);
  else if (strucmp(aElementName,"customgethandlerscript")==0)
    expectScript(fCustomGetHandlerScript,aLine,&GetPutResultFuncTable);
  else if (strucmp(aElementName,"customgetputscript")==0)
    expectScript(fCustomGetPutScript,aLine,&GetPutResultFuncTable);
  else if (strucmp(aElementName,"customendputscript")==0)
    expectScript(fCustomEndPutScript,aLine,&GetPutResultFuncTable);
  else if (strucmp(aElementName,"customputresulthandlerscript")==0)
    expectScript(fCustomPutResultHandlerScript,aLine,&GetPutResultFuncTable);
  #endif
  #ifndef MINIMAL_CODE
  // logfile
  else if (strucmp(aElementName,"logfile")==0)
    expectMacroString(fLogFileName);
  else if (strucmp(aElementName,"logformat")==0)
    expectCString(fLogFileFormat);
  else if (strucmp(aElementName,"loglabels")==0)
    expectCString(fLogFileLabels);
  else if (strucmp(aElementName,"logenabled")==0)
    expectBool(fLogEnabled);
  else if (strucmp(aElementName,"debugchunkmaxsize")==0)
    expectUInt32(fDebugChunkMaxSize);
  #endif
  else if (strucmp(aElementName,"relyonearlymaps")==0)
    expectBool(fRelyOnEarlyMaps);
  // - local datastores
  else if (strucmp(aElementName,"datastore")==0) {
    // definition of a new datastore
    const char* nam = getAttr(aAttributes,"name");
    if (!nam) {
      ReportError(true,"datastore missing 'name' attribute");
    }
    else {
      // get subtype attribute (some versions can have
      // different datastore types in same agent)
      const char* subtype = getAttr(aAttributes,"type");
      // create new named datastore
      TLocalDSConfig *datastorecfgP = newDatastoreConfig(nam,subtype,this);
      if (!datastorecfgP)
        ReportError(true,"datastore has unknown 'type' attribute");
      else {
        // - save in list
        fDatastores.push_back(datastorecfgP);
        // - let element handle parsing
        expectChildParsing(*datastorecfgP);
      }
    }
  }
  #ifdef SUPERDATASTORES
  // - superdatastore
  else if (strucmp(aElementName,"superdatastore")==0) {
    // definition of a new datastore
    const char* nam = getAttr(aAttributes,"name");
    if (!nam) {
      ReportError(true,"datastore missing 'name' attribute");
    }
    else {
      // create new named datastore
      TLocalDSConfig *datastorecfgP = new TSuperDSConfig(nam,this);
      // - save in list
      fDatastores.push_back(datastorecfgP);
      // - let element handle parsing
      expectChildParsing(*datastorecfgP);
    }
  }
  #endif
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TSessionConfig::localStartElement

#endif


// resolve
void TSessionConfig::localResolve(bool aLastPass)
{
  // resolve
  if (aLastPass) {
    #ifndef NO_REMOTE_RULES
    // - resolve rules and parse OverrideDevInf
    TRemoteRulesList::iterator pos;
    for(pos=fRemoteRulesList.begin();pos!=fRemoteRulesList.end();pos++) {
      (*pos)->Resolve(aLastPass);

      if (!(*pos)->fOverrideDevInfXML.empty()) {
        // SMLTK expects full SyncML message
        string buffer =
          "<SyncML><SyncHdr>"
          "<VerDTD>1.2</VerDTD>"
          "<VerProto>SyncML/1.2</VerProto>"
          "<SessionID>1</SessionID>"
          "<MsgID>1</MsgID>"
          "<Target><LocURI>foo</LocURI></Target>"
          "<Source><LocURI>bar</LocURI></Source>"
          "</SyncHdr>"
          "<SyncBody>"
          "<Results>"
          "<CmdID>1</CmdID>"
          "<MsgRef>1</MsgRef>"
          "<CmdRef>1</CmdRef>"
          "<Meta>"
          "<Type>application/vnd.syncml-devinf+xml</Type>"
          "</Meta>"
          "<Item>"
          "<Source>"
          "<LocURI>./devinf12</LocURI>"
          "</Source>"
          "<Data>"
          ;
        buffer += (*pos)->fOverrideDevInfXML;
        buffer +=
          "</Data>"
          "</Item>"
          "</Results>"
          "</SyncBody>"
          "</SyncML>";
        MemPtr_t xml = (unsigned char *)buffer.c_str();
        XltDecoderPtr_t decoder = NULL;
        SmlSyncHdrPtr_t hdr = NULL;
        Ret_t ret = xltDecInit(SML_XML,
                               xml + buffer.size(),
                               &xml,
                               &decoder,
                               &hdr);
        if (ret != SML_ERR_OK) {
          fRootElementP->setError(true, "initializing scanner for DevInf failed");
        } else {
          smlFreeProtoElement(hdr);
          SmlProtoElement_t element;
          VoidPtr_t content = NULL;
          ret = xltDecNext(decoder,
                           xml + buffer.size(),
                           &xml,
                           &element,
                           &content);
          if (ret != SML_ERR_OK) {
            fRootElementP->setError(true, "parsing of OverrideDevInf failed");
          } else if (element != SML_PE_RESULTS ||
                     !((SmlResultsPtr_t)content)->itemList ||
                     !((SmlResultsPtr_t)content)->itemList->item ||
                     !((SmlResultsPtr_t)content)->itemList->item->data ||
                     ((SmlResultsPtr_t)content)->itemList->item->data->contentType != SML_PCDATA_EXTENSION ||
                     ((SmlResultsPtr_t)content)->itemList->item->data->extension != SML_EXT_DEVINF) {
            fRootElementP->setError(true, "parsing of DevInf returned unexpected result");
            if (content)
              smlFreeProtoElement(content);
          } else {
            (*pos)->fOverrideDevInfP = (SmlDevInfDevInfPtr_t)((SmlResultsPtr_t)content)->itemList->item->data->content;
            (*pos)->fOverrideDevInfBufferP = content;
          }
        }
        if (decoder)
          xltDecTerminate(decoder);
      }
    }
    #endif
    TLocalDSList::iterator pos2;
    for(pos2=fDatastores.begin();pos2!=fDatastores.end();pos2++)
      (*pos2)->Resolve(aLastPass);
    #ifdef SCRIPT_SUPPORT
    TScriptContext *sccP = NULL;
    SYSYNC_TRY {
      // resolve all scripts in same context
      // - init scripts
      TScriptContext::resolveScript(getSyncAppBase(),fSessionInitScript,sccP,NULL);
      TScriptContext::resolveScript(getSyncAppBase(),fSentItemStatusScript,sccP,NULL);
      TScriptContext::resolveScript(getSyncAppBase(),fReceivedItemStatusScript,sccP,NULL);
      TScriptContext::resolveScript(getSyncAppBase(),fSessionFinishScript,sccP,NULL);
      TScriptContext::resolveScript(getSyncAppBase(),fCustomGetHandlerScript,sccP,NULL);
      TScriptContext::resolveScript(getSyncAppBase(),fCustomGetPutScript,sccP,NULL);
      TScriptContext::resolveScript(getSyncAppBase(),fCustomEndPutScript,sccP,NULL);
      TScriptContext::resolveScript(getSyncAppBase(),fCustomPutResultHandlerScript,sccP,NULL);
      // - forget this context
      if (sccP) delete sccP;
    }
    SYSYNC_CATCH (...)
      if (sccP) delete sccP;
      SYSYNC_RETHROW;
    SYSYNC_ENDCATCH
    #endif
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TSessionConfig::localResolve




// TSyncSession
// ============

// constructor
TSyncSession::TSyncSession(
  TSyncAppBase *aSyncAppBaseP, // the owning application base (dispatcher/client base)
  const char *aSessionID // a session ID
) :
  #ifdef SYDEBUG
  fSessionDebugLogs(0),
  #endif
  fTerminated(false),
  #ifdef SYDEBUG
  fSessionLogger(&fSessionZones),
  #endif
  fSyncAppBaseP(aSyncAppBaseP) // link to owning base (dispatcher/clienbase)
{
  // Inherit globally defined time zones
  // Note: this must be done as very first step as all time output routines will use the
  //       session zones
  fSessionZones = *(fSyncAppBaseP->getAppZones());
  // now mark used to avoid early timeout (will be marked again at InternalResetSession())
  SessionUsed();
  fLastRequestStarted=getSessionLastUsed(); // set this in case we terminate before StartMessage()
  fSessionStarted=fLastRequestStarted; // this is also the start of the session
  // show creation
  DEBUGPRINTFX(DBG_OBJINST,("++++++++ TSyncSession created"));
  // assign session ID to have debug ID on correct channel
  fLocalSessionID.assign(aSessionID);
  DEBUGPRINTFX(DBG_EXOTIC,("TSyncSession::TSyncSession: Session ID assigned"));
  // init and start profiling
  MP_SHOWCURRENT(DBG_PROFILE,"TSyncSession::TSyncSession: TSyncSession created");
  TP_INIT(fTPInfo);
  TP_START(fTPInfo,TP_general);
  DEBUGPRINTFX(DBG_EXOTIC,("TSyncSession::TSyncSession: Profiling initialized"));
  // set fields
  fEncoding = SML_UNDEF;
  fLocalAbortReason = true; // unless set otherwise
  fAbortReasonStatus = 0;
  fSessionIsBusy = false; // not busy by default
  fSmlWorkspaceID = 0; // no SyncML toolkit workspace ID yet
  fMaxRoomForData = getRootConfig()->fLocalMaxMsgSize; // rough init
  // other pointers
  #ifdef SCRIPT_SUPPORT
  fSessionScriptContextP = NULL;
  #endif
  fInterruptedCommandP = NULL;
  fIncompleteDataCommandP = NULL;
  #ifdef SYNCSTATUS_AT_SYNC_CLOSE
  fSyncCloseStatusCommandP=NULL;
  #endif
  // we do not know anything about remote datastores yet
  fRemoteDevInfKnown=false;
  fRemoteDataStoresKnown=false;
  fRemoteDataTypesKnown=false;
  fRemoteDevInfLock=false;
  // we have not sent any devinf to the remote yet
  fRemoteGotDevinf=false;
  fRemoteMustSeeDevinf=false;
  fCustomGetPutSent=false;
  // assume normal, full-featured session. Profile config or session progress might set this flag later
  fLegacyMode = false;
  fLenientMode = false;

  //initialize the conditonal variables to keep valgrind happy
  fNeedAuth = true;
  fRemoteRequestedAuth = auth_none;

  #ifdef SYDEBUG
  // initialize session debug logging
  fSessionDebugLogs=getRootConfig()->fDebugConfig.fSessionDebugLogs; /// init from config @todo: get rid of this special session level flag, handle it all via session logger's fDebugEnabled / getDbgMask()
  fSessionLogger.setEnabled(fSessionDebugLogs); // init from session-level flag @todo: get rid of this special session level flag, handle it all via session logger's fDebugEnabled / getDbgMask()
  fSessionLogger.setMask(getRootConfig()->fDebugConfig.fDebug); // init from config
  fSessionLogger.setOptions(&(getRootConfig()->fDebugConfig.fSessionDbgLoggerOptions));
  if (getRootConfig()->fDebugConfig.fLogSessionsToGlobal) {
    // pass session output to app global logger
    fSessionLogger.outputVia(getSyncAppBase()->getDbgLogger());
    // show start of log
    PDEBUGPRINTFX(DBG_HOT,("--------- START of embedded log for session ID '%s' ---------", fLocalSessionID.c_str()));
  }
  else {
    // use separate output for session logs
    fSessionLogger.installOutput(getSyncAppBase()->newDbgOutputter(false)); // install the output object (and pass ownership!)
    fSessionLogger.setDebugPath(getRootConfig()->fDebugConfig.fDebugInfoPath.c_str()); // base path
    const string &name = getRootConfig()->fDebugConfig.fSessionDbgLoggerOptions.fBasename;
    fSessionLogger.appendToDebugPath(name.empty() ?
                                     TARGETID :
                                     name.c_str());
    if (getRootConfig()->fDebugConfig.fSingleSessionLog) {
      getRootConfig()->fDebugConfig.fSessionDbgLoggerOptions.fAppend=true; // One single log - in this case, we MUST append to current log
      fSessionLogger.appendToDebugPath("_session"); // only single session log, always with the same name
    }
    else {
      if (getRootConfig()->fDebugConfig.fTimedSessionLogNames) {
        fSessionLogger.appendToDebugPath("_");
        string t;
        TimestampToISO8601Str(t, getSystemNowAs(TCTX_UTC), TCTX_UTC, false, false);
        fSessionLogger.appendToDebugPath(t.c_str());
      }
      fSessionLogger.appendToDebugPath("_s");
      fSessionLogger.appendToDebugPath(fLocalSessionID.c_str());
    }
  }
  fSessionLogger.DebugDefineMainThread();
  // initialize session level dump flags
  fDumpCount=0;
  fIgnoreIncomingCommands=false;
  fOutgoingXMLInstance=NULL;
  fIncomingXMLInstance=NULL;
  fXMLtranslate=getRootConfig()->fDebugConfig.fXMLtranslate; // initialize from config
  fMsgDump=getRootConfig()->fDebugConfig.fMsgDump; // initialize from config
  #endif

  // reset session at creation
  DEBUGPRINTFX(DBG_EXOTIC,("TSyncSession::TSyncSession: calling InternalResetSession"));
  InternalResetSessionEx(false);
  DEBUGPRINTFX(DBG_EXOTIC,("TSyncSession::TSyncSession: InternalResetSession called"));
  // show starting
  #ifndef ENGINE_LIBRARY
  // - don't show it here in library case as Agent must be ready as well to distribute event correctly
  SESSION_PROGRESS_EVENT(this,pev_sessionstart,NULL,0,0,0);
  #endif
  #ifdef SYDEBUG
  #if defined(SYSYNC_SERVER) && defined(SYSYNC_CLIENT)
    #define CAN_BE_TEXT "Server+Client"
  #elif defined(SYSYNC_SERVER)
    #define CAN_BE_TEXT "Server"
  #else
    #define CAN_BE_TEXT "Client"
  #endif
  if (PDEBUGTEST(DBG_HOT)) {
    // Show Session Start
    PDEBUGPRINTFX(DBG_HOT,(
      "==== %s Session started with SyncML (%s) Engine Version %d.%d.%d.%d",
      IS_SERVER ? "Server" : "Client",
      CAN_BE_TEXT,
      SYSYNC_VERSION_MAJOR,
      SYSYNC_VERSION_MINOR,
      SYSYNC_SUBVERSION,
      SYSYNC_BUILDNUMBER
    ));
    // show Product ID string
    PDEBUGPRINTFX(DBG_HOT,(
      "---- Hardcoded Product name: " CUST_SYNC_MODEL
    ));
    PDEBUGPRINTFX(DBG_HOT,(
      "---- Configured Model/Manufacturer: %s / %s",
      getSyncAppBase()->getModel().c_str(), getSyncAppBase()->getManufacturer().c_str()
    ));
    // show platform we're on
    string devid;
    getSyncAppBase()->getMyDeviceID(devid);
    PDEBUGPRINTFX(DBG_HOT,(
      "---- Running on " SYSYNC_PLATFORM_NAME ", URI/deviceID='%s'",
      devid.c_str()
    ));
    // show process and thread ID of the main session thread
    #ifdef MULTI_THREAD_SUPPORT
    PDEBUGPRINTFX(DBG_HOT,(
      "---- Process ID = %lu, Thread ID = %lu",
      myProcessID(),
      myThreadID()
    ));
    #endif
    // show platform details
    string dname,vers;
    // - as determined by engine itself
    getPlatformString(pfs_device_name,dname);
    getPlatformString(pfs_platformvers,vers);
    PDEBUGPRINTFX(DBG_HOT,(
      "---- Platform Hardware Name/Version = '%s', Firmware/OS Version = '%s'",
      dname.c_str(),
      vers.c_str()
    ));
    // - as configured
    PDEBUGPRINTFX(DBG_HOT,(
      "---- Configured Hardware Version = '%s', Firmware Version = '%s'",
      getSyncAppBase()->getHardwareVersion().c_str(),
      getSyncAppBase()->getFirmwareVersion().c_str()
    ));
    // show time zone infos
    lineartime_t tim;
    string z,ts;
    timecontext_t tctx;
    sInt16 offs;
    // - System local time and zone
    tctx = getRootConfig()->fSystemTimeContext;
    TzResolveMetaContext(tctx, getSessionZones()); // make non-meta
    TimeZoneContextToName(tctx, z, getSessionZones());
    tim = getSystemNowAs(tctx);
    StringObjTimestamp(ts,tim);
    TzResolveToOffset(tctx, offs, tim, false, getSessionZones());
    PDEBUGPRINTFX(DBG_HOT,(
      "---- System local time  : %s  (time zone '%s', offset %hd:%02hd hours east of UTC)",
      ts.c_str(),z.c_str(),
      (sInt16)(offs / MinsPerHour),
      (sInt16)abs(offs % MinsPerHour)
    ));
    PDEBUGPRINTFX(DBG_EXOTIC,("     Offset in Minutes east of UTC: %hd",offs));
    // - System time in UTC
    tim = getSystemNowAs(TCTX_UTC);
    StringObjTimestamp(ts,tim);
    PDEBUGPRINTFX(DBG_HOT,("---- System time in UTC : %s",ts.c_str()));
    // - make a winter and a summer test
    if (PDEBUGTEST(DBG_EXOTIC)) {
      sInt16 y,m,d;
      lineartime2date(tim,&y,&m,&d);
      d=1;m=2; // February 1st
      tim=date2lineartime(y,m,d);
      TzResolveToOffset(TCTX_SYSTEM, offs, tim, true, getSessionZones());
      PDEBUGPRINTFX(DBG_EXOTIC,(
        "---- System time zone offset per %04hd-02-01 = %hd:%02hd (=%hd mins)",
        y, (sInt16)(offs / MinsPerHour), (sInt16)abs(offs % MinsPerHour), offs
      ));
      d=1;m=8; // August 1st
      tim=date2lineartime(y,m,d);
      TzResolveToOffset(TCTX_SYSTEM, offs, tim, true, getSessionZones());
      PDEBUGPRINTFX(DBG_EXOTIC,(
        "---- System time zone offset per %04hd-08-01 = %hd:%02hd (=%hd mins)",
        y, (sInt16)(offs / MinsPerHour), (sInt16)abs(offs % MinsPerHour), offs
      ));
    }
  }
  #endif
  DebugShowCfgInfo();
} // TSyncSession::TSyncSession


// destructor
TSyncSession::~TSyncSession()
{
  // remove user data pointer because session does not exist any longer
  getSyncAppBase()->setSmlInstanceUserData(fSmlWorkspaceID,NULL);
  // make sure it is terminated (but normally it is already terminated here)
  TerminateSession();
  // debug
  DEBUGPRINTFX(DBG_OBJINST,("-------- TSyncSession almost destroyed (except implicit member destruction)"));
  #ifdef SYDEBUG
  if (getRootConfig()->fDebugConfig.fLogSessionsToGlobal) {
    // show end of embedded log
    PDEBUGPRINTFX(DBG_HOT,("--------- END of embedded log for session ID '%s' ---------", fLocalSessionID.c_str()));
  }
  fSessionLogger.DebugThreadOutputDone();
  #endif
} // TSyncSession::~TSyncSession


/// @brief terminate a session.
/// @Note: Termination is final - session cannot be restarted by RestartSession() after
//         calling this routine
void TSyncSession::TerminateSession(void)
{
  if (!fTerminated) {
    // save type of ending (before fAborted gets reset in InternalResetSession())
    bool normalend = !fAborted;
    bool allsuccess = isAllSuccess();
    // do this class' reset stuff
    DEBUGPRINTFX(DBG_EXOTIC,("TSyncSession::TerminateSession: calling InternalResetSession"));
    InternalResetSessionEx(true);
    DEBUGPRINTFX(DBG_EXOTIC,("TSyncSession::TerminateSession: InternalResetSession called"));
    #ifdef SCRIPT_SUPPORT
    // remove the session script context
    if (fSessionScriptContextP) {
      delete fSessionScriptContextP;
      fSessionScriptContextP = NULL;
    }
    #endif
    // remove all local datastores
    TLocalDataStorePContainer::iterator pos1;
    int n=fLocalDataStores.size();
    PDEBUGPRINTFX(DBG_EXOTIC,("Deleting %d datastores",n));
    for (pos1=fLocalDataStores.begin(); pos1!=fLocalDataStores.end(); ++pos1) {
      delete *pos1;
    }
    fLocalDataStores.clear(); // clear list
    // remove all local itemtypes
    TSyncItemTypePContainer::iterator pos2;
    for (pos2=fLocalItemTypes.begin(); pos2!=fLocalItemTypes.end(); ++pos2) {
      delete *pos2;
    }
    fLocalItemTypes.clear(); // clear list
    #ifdef SYDEBUG
    // save half-begun XML translations
    XMLTranslationOutgoingEnd();
    XMLTranslationIncomingEnd();
    #endif
    // stop and show profiling info
    TP_STOP(fTPInfo);
    #ifdef TIME_PROFILING
    if (getDbgMask() & DBG_PROFILE) {
      sInt16 i;
      uInt32 sy,us;
      PDEBUGPRINTFX(DBG_PROFILE,("Session CPU usage statistics: (system/user/total)"));
      // sections
      for (i=0; i<numTPTypes; i++) {
        sy = TP_GETSYSTEMMS(fTPInfo,(TTP_Types)i);
        us = TP_GETUSERMS(fTPInfo,(TTP_Types)i);
        PDEBUGPRINTFX(DBG_PROFILE,(
          "- %-20s : %10ld /%10ld /%10ld ms",
          TP_TypeNames[i],
          sy,
          us,
          sy+us
        ));
      }
      // total
      sy = TP_GETTOTALSYSTEMMS(fTPInfo);
      us = TP_GETTOTALUSERMS(fTPInfo);
      PDEBUGPRINTFX(DBG_PROFILE,(
        "- TOTAL                : %10ld /%10ld /%10ld ms",
        sy,
        us,
        sy+us
      ));
      // Real time
      uInt32 rt = TP_GETREALTIME(fTPInfo);
      PDEBUGPRINTFX(DBG_PROFILE,(
        "- Real Time            : %10ld ms",
        rt
      ));
      // % CPU
      PDEBUGPRINTFX(DBG_PROFILE,(
        "- CPU load             : %10ld promille",
        rt ? (sy+us)*1000/rt : 0
      ));
    }
    #endif
    MP_SHOWCURRENT(DBG_PROFILE,"TSyncSession deleting");
    // show ending (if not normal, then ending was already shown in AbortSession())
    if (normalend) {
      SESSION_PROGRESS_EVENT(this,pev_sessionend,NULL, allsuccess ? 0 : LOCERR_INCOMPLETE,0,0);
    }
    // is NOW terminated
    fTerminated = true;
    PDEBUGPRINTFX(DBG_EXOTIC,("Session is now terminated (but not yet deleted)"));
  } // if not already terminated
} // TSyncSession::TerminateSession


// Virtual version
void TSyncSession::ResetSession(void)
{
  // terminate sync of datastores
  TerminateDatastores();
  // reset internals
  DEBUGPRINTFX(DBG_EXOTIC,("TSyncSession::ResetSession: calling InternalResetSession"));
  InternalResetSessionEx(false);
  DEBUGPRINTFX(DBG_EXOTIC,("TSyncSession::ResetSession: InternalResetSession called"));
  // no ancestor to call
} // TSyncSession::ResetSession


/// @brief Announce destruction of descendant to all datastores which might have direct links to these descendants and must cancel those
/// @note must be called by derived class' destructors to allow datastores to detach from agent BEFORE descendant destructor has run
void TSyncSession::announceDestruction()
{
  // terminate sync with all datastores
  TLocalDataStorePContainer::iterator pos;
  for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
    // now let datastores cancel possible direct links to derived TSession
    (*pos)->announceAgentDestruction();
  }
} // TSyncSession::announceDestruction


// - terminate all datastores
void TSyncSession::TerminateDatastores(localstatus aAbortStatusCode)
{
  // terminate sync with all datastores
  TLocalDataStorePContainer::iterator pos;
  for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
    // terminate
    (*pos)->engTerminateDatastore(aAbortStatusCode);
  }
} // TSyncSession::TerminateDatastores


// - resets session and removes all datastores (local and remote)
void TSyncSession::ResetAndRemoveDatastores(void)
{
  // Must reset session before
  ResetSession();
  // remove all local datastores
  TLocalDataStorePContainer::iterator pos;
  for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
    // now actually delete them
    TLocalEngineDS *dsP = (*pos);
    if (dsP) {
      (*pos) = NULL; // to avoid double deletes
      delete dsP;
    }
  }
  fLocalDataStores.clear(); // remove pointers
} // TSyncSession::ResetAndRemoveDatastores



// reset session to inital state (new)
// - this is called at creation and destruction but can be called
//   also when an existing session needs to be restarted.
// - InternalResetSession() must be proof for being called more than
//   once in a row.
// - InternalResetSession() must also be callable from destructor
//   (care not to call other objects which will refer to the already
//   half-destructed session!)
void TSyncSession::InternalResetSessionEx(bool terminationCall)
{
  #ifdef SCRIPT_SUPPORT
  // call session termination script (ONCE!)
  if (terminationCall && !fTerminated) {
    TScriptContext::execute(
      fSessionScriptContextP,
      getSessionConfig()->fSessionFinishScript,
      NULL, // context's function table
      NULL // datastore pointer needed for context
    );
  }
  #endif

  // reset sync and datastores
  // - version not known in advance
  fSyncMLVersion=syncml_vers_unknown;
  // - immediately abort SYNC command in progress
  fLocalSyncDatastoreP = NULL;
  #ifndef NO_REMOTE_RULES
  // - no remote rules applied
  fActiveRemoteRules.clear();
  #endif
  // - set defaults for >=SyncML 1.1 features
  fRemoteWantsNOC = false; // no, unless requested
  fRemoteCanHandleUTC = false; // assume remote can not handle UTC time (note that for SyncML 1.0 this will be set to true later)
  fRemoteSupportsLargeObjects = false; // no large object support by default
  // - default options
  fTreatRemoteTimeAsLocal = false; // do not ignore time zone information from remote
  fTreatRemoteTimeAsUTC = false; // do not ignore time zone information from remote
  fIgnoreDevInfMaxSize = false; // do not ignore <maxsize> specification in CTCap
  fIgnoreCTCap = false; // do not ignore CTCap
  fDSPathInDevInf = true; // newer Nokias need this, as they expect the same path in devInf as they sent in alert
  fDSCgiInDevInf = true; // newer Nokias need this, as they expect the same path AND CGI in devInf as they sent in alert
  fReadOnly = false; // always disabled unless set by SessionLogin()
  // - init user time zone from setting. May be modified later using SETUSERTIMEZONE()
  fUserTimeContext = getSessionConfig()->fUserTimeContext;
  #ifdef SYDEBUG
  fSessionLogger.setEnabled(getRootConfig()->fDebugConfig.fSessionDebugLogs); // get default value
  #endif
  #ifndef MINIMAL_CODE
  fLogEnabled = getSessionConfig()->fLogEnabled;
  #endif
  #ifdef SCRIPT_SUPPORT
  // retain session variables if InternalResetSessionEx() is called more than once in the same session
  // (which is normal procedure in clients, where SelectProfile calls ResetSession)
  // Note: fSessionScriptContextP will be deleted in the destructor
  if (!fSessionScriptContextP) {
    if (!terminationCall && !fTerminated) {
      // prepare session-level scripts
      TScriptContext::rebuildContext(getSyncAppBase(),getSessionConfig()->fSessionInitScript,fSessionScriptContextP,this);
      TScriptContext::rebuildContext(getSyncAppBase(),getSessionConfig()->fSentItemStatusScript,fSessionScriptContextP,this);
      TScriptContext::rebuildContext(getSyncAppBase(),getSessionConfig()->fReceivedItemStatusScript,fSessionScriptContextP,this);
      TScriptContext::rebuildContext(getSyncAppBase(),getSessionConfig()->fSessionFinishScript,fSessionScriptContextP,this);
      TScriptContext::rebuildContext(getSyncAppBase(),getSessionConfig()->fCustomGetHandlerScript,fSessionScriptContextP,this);
      TScriptContext::rebuildContext(getSyncAppBase(),getSessionConfig()->fCustomGetPutScript,fSessionScriptContextP,this);
      TScriptContext::rebuildContext(getSyncAppBase(),getSessionConfig()->fCustomEndPutScript,fSessionScriptContextP,this);
      TScriptContext::rebuildContext(getSyncAppBase(),getSessionConfig()->fCustomPutResultHandlerScript,fSessionScriptContextP,this,true); // now instantiate vars
    }
  }
  #endif
  // - remove all remote datastores
  TRemoteDataStorePContainer::iterator pos1;
  for (pos1=fRemoteDataStores.begin(); pos1!=fRemoteDataStores.end(); ++pos1) {
    delete *pos1;
  }
  fRemoteDataStores.clear(); // clear list
  // - remove all remote itemtypes
  TSyncItemTypePContainer::iterator pos2;
  for (pos2=fRemoteItemTypes.begin(); pos2!=fRemoteItemTypes.end(); ++pos2) {
    delete *pos2;
  }
  fRemoteItemTypes.clear(); // clear list
  // reset basics
  SYSYNC_TRY {
    // empty command queues
    TSmlCommandPContainer::iterator pos;
    // - commands waiting for status
    for (pos=fStatusWaitCommands.begin(); pos!=fStatusWaitCommands.end(); ++pos) {
      #ifdef SYDEBUG
      TSmlCommand *cmdP = *pos;
      // show that command was not answered
      PDEBUGPRINTFX(DBG_PROTO,("Never received status for &html;<a name=\"SO_%ld_%ld\" href=\"#IO_%ld_%ld\">&html;command '%s'&html;</a>&html;, (outgoing MsgID=%ld, CmdID=%ld)",
        (long)cmdP->getMsgID(),
        (long)cmdP->getCmdID(),
        (long)cmdP->getMsgID(),
        (long)cmdP->getCmdID(),
        cmdP->getName(),
        (long)cmdP->getMsgID(),
        (long)cmdP->getCmdID()
      ));
      #endif
      // delete, if this is not the interrupted command.
      // Note: only the interrupted command can also be in the status queue.
      //       Other queues are exclusive owners of their commands.
      if (*pos != fInterruptedCommandP)
        delete *pos;
      else
        DEBUGPRINTF(("- prevented deleting because command is interrupted"));
    }
    fStatusWaitCommands.clear(); // clear list
    // - commands waiting for outgoing message to begin
    forgetHeaderWaitCommands();
    // - commands to be issued only after all commands in this message have
    //   been processed and answered by a status
    for (pos=fEndOfMessageCommands.begin(); pos!=fEndOfMessageCommands.end(); ++pos) {
      // show that command was not sent
      DEBUGPRINTF(("Never sent end-of-message command '%s', (outgoing MsgID=%ld, CmdID=%ld)",
        (*pos)->getName(),
        (long)(*pos)->getMsgID(),
        (long)(*pos)->getCmdID()
      ));
      // delete
      delete *pos;
    }
    fEndOfMessageCommands.clear(); // clear list
    // - commands to be sent in next message
    for (pos=fNextMessageCommands.begin(); pos!=fNextMessageCommands.end(); ++pos) {
      // show that command was not sent
      DEBUGPRINTF(("Never sent next-message command '%s', (outgoing MsgID=%ld, CmdID=%ld)",
        (*pos)->getName(),
        (long)(*pos)->getMsgID(),
        (long)(*pos)->getCmdID()
      ));
      // delete
      delete *pos;
    }
    fNextMessageCommands.clear(); // clear list
    // - commands to be sent in next package
    for (pos=fNextPackageCommands.begin(); pos!=fNextPackageCommands.end(); ++pos) {
      // show that command was not sent
      DEBUGPRINTF(("Never sent next-package command '%s', (outgoing MsgID=%ld, CmdID=%ld)",
        (*pos)->getName(),
        (long)(*pos)->getMsgID(),
        (long)(*pos)->getCmdID()
      ));
      // delete
      delete *pos;
    }
    fNextPackageCommands.clear(); // clear list
    // - interrupted command
    if (fInterruptedCommandP) {
      // show that command was not sent
      DEBUGPRINTF(("Never finished interrupted command '%s', (outgoing MsgID=%ld, CmdID=%ld)",
        fInterruptedCommandP->getName(),
        (long)fInterruptedCommandP->getMsgID(),
        (long)fInterruptedCommandP->getCmdID()
      ));
      delete fInterruptedCommandP;
      fInterruptedCommandP=NULL;
    }
    // - commands to be executed again at beginning of next message
    fDelayedExecSyncEnds=0;
    for (pos=fDelayedExecutionCommands.begin(); pos!=fDelayedExecutionCommands.end(); ++pos) {
      // show that command was not sent
      DEBUGPRINTF(("Never finished executing command '%s', (incoming MsgID=%ld, CmdID=%ld)",
        (*pos)->getName(),
        (long)(*pos)->getMsgID(),
        (long)(*pos)->getCmdID()
      ));
      // delete
      delete *pos;
    }
    fDelayedExecutionCommands.clear(); // clear list
    #ifdef SYNCSTATUS_AT_SYNC_CLOSE
    // make sure sync status is disposed
    if (fSyncCloseStatusCommandP) delete fSyncCloseStatusCommandP;
    fSyncCloseStatusCommandP=NULL;
    #endif
    // remove incomplete data command
    if (fIncompleteDataCommandP) delete fIncompleteDataCommandP;
    fIncompleteDataCommandP=NULL;
  }
  SYSYNC_CATCH (exception &e)
    #ifdef SYDEBUG
    DEBUGPRINTFX(DBG_ERROR,(
      "WARNING: Exception during InternalResetSession(): %s",
      e.what()
    ));
    #endif
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    #ifdef SYDEBUG
    DEBUGPRINTFX(DBG_ERROR,(
      "WARNING: Unknown Exception during InternalResetSession()"
    ));
    #endif
  SYSYNC_ENDCATCH
  // remember time of creation or last reset
  SessionUsed();
  // reset session status
  fIncomingState=psta_idle; // no incoming package status yet
  fCmdIncomingState=psta_idle;
  fOutgoingState=psta_idle; // no outgoing package status yet
  fRestarting=false;
  fNextMessageRequests=0; // no pending next message requests
  fFakeFinalFlag=false; // special flag to work around broken resume implementations
  fNewOutgoingPackage=true; // first message will be first in outgoing package
  fSessionAuthorized=false; // session not permanently authorized
  fMessageAuthorized=false; // message not authorized either
  fAuthFailures=0; // no failed attempts by remote so far
  fAuthRetries=0; // no failed attempts by myself so far
  fIncomingMsgID=0; // expected session to start with MsgID=1, so must be 0 now as it will be incremented at StartMessage()
  fOutgoingMsgID=0; // starting answers with MsgID=1, so must be 0 as it will be incremented before sending a new message
  fAborted=false; // not yet aborted
  fSuspended=false; // not being suspended yet
  fSuspendAlertSent=false; // no suspend alert sent so far
  fFailedDatastores=0; // none failed
  fErrorItemDatastores=0; // none generated or detected error items
  fInProgress=false; // not yet in progress
  fOutgoingStarted=false; // no outgoing message started yet
  fSequenceNesting=0; // no sequence command open
  fMaxOutgoingMsgSize=0; // no limit for outgoing messages so far
  fMaxOutgoingObjSize=0; // SyncML 1.1: no limit for outgoing objects so far
  fOutgoingMessageFull=false; // limit not yet reached
  // init special remote-dependent behaviour
  fLimitedRemoteFieldLengths=false; // assume remote has not generally limited field lenghts
  fDontSendEmptyProperties=false; // normally, empty properties will be sent
  fDoQuote8BitContent=false;
  fNoReplaceInSlowsync=false;
  fTreatRemoteTimeAsLocal=false;
  fTreatRemoteTimeAsUTC=false;
  fIgnoreDevInfMaxSize=false;
  fTreatCopyAsAdd=false;
  fStrictExecOrdering=true; // SyncML standard requires strict ordering (of statuses, but this implies execution of commands, too)
  fDefaultOutCharset=chs_utf8; // SyncML content is usually UTF-8
  fDefaultInCharset=chs_utf8; // SyncML content is usually UTF-8
  // defaults for possibly remote-dependent behaviour
  fCompleteFromClientOnly=getSessionConfig()->fCompleteFromClientOnly; // conform to standard by default
  fRequestMaxTime=getSessionConfig()->fRequestMaxTime;
  fRequestMinTime=getSessionConfig()->fRequestMinTime;
  fUpdateClientDuringSlowsync=getSessionConfig()->fUpdateClientDuringSlowsync;
  fUpdateServerDuringSlowsync=getSessionConfig()->fUpdateServerDuringSlowsync;
  fAllowMessageRetries=getSessionConfig()->fAllowMessageRetries;
  fShowCTCapProps=getSessionConfig()->fShowCTCapProps;
  fShowTypeSzInCTCap10=getSessionConfig()->fShowTypeSzInCTCap10;
  fVCal10EnddatesSameDay=getSessionConfig()->fVCal10EnddatesSameDay;
  fDoNotFoldContent=getSessionConfig()->fDoNotFoldContent;
  // tristates!!
  fEnumDefaultPropParams=getSessionConfig()->fEnumDefaultPropParams;
  #ifdef SCRIPT_SUPPORT
  // call session init script
  if (!terminationCall && !fTerminated) {
    TScriptContext::execute(
      fSessionScriptContextP,
      getSessionConfig()->fSessionInitScript,
      NULL, // context's function table
      NULL // datastore pointer needed for context
    );
  }
  #endif
} // TSyncSession::InternalResetSessionEx



#ifdef PROGRESS_EVENTS

// event generator
bool TSyncSession::NotifySessionProgressEvent(
  TProgressEventType aEventType,
  TLocalDSConfig *aDatastoreID,
  sInt32 aExtra1,
  sInt32 aExtra2,
  sInt32 aExtra3
)
{
  #ifdef ENGINE_LIBRARY
  // library build, session level events get queued and dispatched via sessionstep
  // - handle some events specially
  if (aEventType == pev_nop)
    return true; // just continue
  else {
    // - prepare info record
    TEngineProgressInfo info;
    info.eventtype = (uInt16)(aEventType);
    // - datastore ID, if any
    if (aDatastoreID != NULL)
      info.targetID = (sInt32)(aDatastoreID->fLocalDBTypeID);
    else
      info.targetID = 0;
    // - extras
    info.extra1 = aExtra1;
    info.extra2 = aExtra2;
    info.extra3 = aExtra3;
    // - handle it
    return HandleSessionProgressEvent(info);
  }
  #else
  // old monolithic build, pass to appbase which dispatches them to the app via callback
  return getSyncAppBase()->NotifyAppProgressEvent(aEventType,aDatastoreID,aExtra1,aExtra2,aExtra3);
  #endif
} // TSyncAppBase::NotifySessionProgressEvent

#endif // PROGRESS_EVENTS



// get root config pointer
// NOTE: we have moved this here because Palm linker
//       would have problems accessing it as syncsession.cpp is
//       large enough to have EndMessage >32k away from the
//       original position of this routine.
TRootConfig *TSyncSession::getRootConfig(void)
{
  return fSyncAppBaseP->getRootConfig();
} // TSyncSession::getRootConfig


// forget commands waiting to be sent when header is generated
void TSyncSession::forgetHeaderWaitCommands(void)
{
  // empty command queues
  TSmlCommandPContainer::iterator pos;
  // - commands waiting for outgoing message to begin
  for (pos=fHeaderWaitCommands.begin(); pos!=fHeaderWaitCommands.end(); ++pos) {
    #if SYDEBUG>1
    TSmlCommand *cmdP = *pos;
    // show that command was not answered
    DEBUGPRINTF(("Never sent command '%s', (outgoing MsgID=%ld, CmdID=%ld) because outgoing message never started",
      cmdP->getName(),
      (long)cmdP->getMsgID(),
      (long)cmdP->getCmdID()
    ));
    #endif
    // delete
    delete *pos;
  }
  fHeaderWaitCommands.clear(); // clear list
} // TSyncSession::forgetHeaderWaitCommands


// abort processing commands in this message
void TSyncSession::AbortCommandProcessing(TSyError aStatusCode)
{
  if (!fIgnoreIncomingCommands) {
    fIgnoreIncomingCommands=true; // do not process further commands
    fStatusCodeForIgnored=aStatusCode; // save status code
    PDEBUGPRINTFX(DBG_HOT,(
      "--------------- Ignoring all commands in this message (after %ld sec. request processing, %ld sec. total) with Status %hd (0=none) from here on",
      (long)((getSystemNowAs(TCTX_UTC)-getLastRequestStarted()) / (lineartime_t)secondToLinearTimeFactor),
      (long)((getSystemNowAs(TCTX_UTC)-getSessionStarted()) / (lineartime_t)secondToLinearTimeFactor),
      fStatusCodeForIgnored
    ));
  }
} // TSyncSession::AbortCommandProcessing


// suspend session (that is: flag abortion)
void TSyncSession::SuspendSession(TSyError aReason)
{
  // first check for suspend
  if (fSyncMLVersion>=syncml_vers_1_2) {
    // try suspend, only possible if not already suspended or aborted
    if (!fSuspended && !fAborted) {
      fSuspended=true; // trigger suspend
      fAbortReasonStatus = aReason;
      fLocalAbortReason = true;
      AbortCommandProcessing(514); // abort command processing, all subsequent commands will be ignored with "cancelled" status
      PDEBUGPRINTFX(DBG_ERROR,(
        "WARNING: Session locally flagged for suspend, reason=%hd",
        aReason
      ));
      CONSOLEPRINTF((
        "Session will be suspended due to local error code %hd\n",
        aReason
      ));
    }
  }
  else {
    // We can't suspend, we need to abort
    AbortSession(500,true,aReason);
  }
} // TSyncSession::SuspendSession


void TSyncSession::MarkSuspendAlertSent(bool aSent)
{
  fSuspendAlertSent = aSent;
}


// abort session (that is: flag abortion)
void TSyncSession::AbortSession(TSyError aStatusCode, bool aLocalProblem, TSyError aReason)
{
  // Catch the case that some inner routine, e.g. a plugin, detects a user suspend. It can
  // return LOCERR_USERSUSPEND then and will cause the engine instead of aborting.
  if (aStatusCode==LOCERR_USERSUSPEND) {
    SuspendSession(LOCERR_USERSUSPEND);
    return;
  }
  // make sure session gets aborted
  // BUT: do NOT reset yet. Reset would incorrectly abort message answering
  if (!fAborted && !fTerminated) {
    fAborted=true; // session aborted
    fAbortReasonStatus = aReason ? aReason : aStatusCode;
    fLocalAbortReason = aLocalProblem;
    fInProgress=false; // not in progress any more (will be deleted after end of request)
    PDEBUGBLOCKFMT(("SessionAbort","Aborting Session",
      "Status=%hd|ProblemSource=%s",
      fAbortReasonStatus,
      fLocalAbortReason ? "LOCAL" : "REMOTE"
    ));
    PDEBUGPRINTFX(DBG_ERROR,(
      "WARNING: Aborting Session with Reason Status %hd (%s problem) ***",
      fAbortReasonStatus,
      fLocalAbortReason ? "LOCAL" : "REMOTE"
    ));
    // In SyncML 1.1, we have a special status code to show that commands are not
    // executed any more due to cancellation of the session
    if (fSyncMLVersion>=syncml_vers_1_1)
      aStatusCode=514; // cancelled, command not completed
    AbortCommandProcessing(aStatusCode);
    // let all local datastores know
    TLocalDataStorePContainer::iterator pos;
    for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
      (*pos)->engAbortDataStoreSync(fAbortReasonStatus, fLocalAbortReason);
    }
    CONSOLEPRINTF((
      "Session aborted because of %s SyncML error code %hd\n",
      fLocalAbortReason ? "LOCAL" : "REMOTE",
      fAbortReasonStatus
    ));
    SESSION_PROGRESS_EVENT(
      this,
      pev_sessionend,
      NULL,
      getAbortReasonStatus(),
      0,0
    );
    PDEBUGENDBLOCK("SessionAbort");
  }
} // TSyncSession::AbortSession


// returns true if session was a complete success
bool TSyncSession::isAllSuccess(void)
{
  return fFailedDatastores==0 && fErrorItemDatastores==0;
} // TSyncSession::isAllSuccess


// let session know that datastore has failed
void TSyncSession::DatastoreFailed(TSyError aStatusCode, bool aLocalProblem)
{
  fFailedDatastores++;
  // %%% note that this is not perfect for server, as inactive datastores are possible
  if (fFailedDatastores>=fLocalDataStores.size()) {
    // all have failed by now, abort the session
    AbortSession(aStatusCode, aLocalProblem, aStatusCode);
  }
} // TSyncSession::DatastoreFailed


// set SyncML toolkit workspace ID
void TSyncSession::setSmlWorkspaceID(InstanceID_t aSmlWorkspaceID)
{
  fSmlWorkspaceID=aSmlWorkspaceID;
} // TSyncSession::setSmlWorkspaceID


// show some information about the config
void TSyncSession::DebugShowCfgInfo(void)
{
  #ifdef SYDEBUG
  string t;
  StringObjTimestamp(t,getRootConfig()->fConfigDate);
  // now write settings to log
  PDEBUGPRINTFX(DBG_HOT,(
    "==== Config file='%s', Last Change=%s",
    getSyncAppBase()->fConfigFilePath.c_str(),
    t.c_str()
  ));
  #ifndef HARDCODED_CONFIG
  PDEBUGPRINTFX(DBG_HOT,(
    "==== Config ID string='%s'",
    getRootConfig()->fConfigIDString.c_str()
  ));
  #endif
  #endif
} // TSyncSession::DebugShowCfgInfo


#ifndef MINIMAL_CODE

#include <errno.h>

// write to sync log file
void TSyncSession::WriteLogLine(const char *aLogline)
{
  if (getSessionConfig()->fLogFileName.empty()) return; // do not write without a path
  // open for append
  FILE * logfile=fopen(getSessionConfig()->fLogFileName.c_str(),"a");
  if (!logfile) {
    PDEBUGPRINTFX(DBG_ERROR,("**** Cannot write to logfile '%s' (errno=%ld)",getSessionConfig()->fLogFileName.c_str(),(long)errno));
    return; // cannot write
  }
  // check if we need to write labels first
  if (!getSessionConfig()->fLogFileLabels.empty()) {
    // check file size
    if (ftell(logfile)==0) {
      // we are at the beginning, print labels first
      fputs(getSessionConfig()->fLogFileLabels.c_str(),logfile);
    }
  }
  // now write log line
  fputs(aLogline,logfile);
  // close file
  fclose(logfile);
} // TSyncSession::WriteLogLine

#endif



// queue a SyncBody context command for issuing after incoming message has ended
void TSyncSession::queueForIssueRoot(
  TSmlCommand * &aSyncCommandP                  // the command
)
{
  fEndOfMessageCommands.push_back(aSyncCommandP);
  aSyncCommandP=NULL;
} // TSyncSession::queueForIssueRoot


// queue a SyncBody context command for issuing after incoming message has ended
void TSyncSession::issueNotBeforePackage(
  TPackageStates aPackageState,
  TSmlCommand *aSyncCommandP                  // the command
)
{
  if (fOutgoingState>=aPackageState) {
    issueRootPtr(aSyncCommandP);
  }
  else {
    PDEBUGPRINTFX(DBG_SESSION,("%s queued for next package",aSyncCommandP->getName()));
    fNextPackageCommands.push_back(aSyncCommandP);
  }
} // TSyncSession::issueNotBeforePackage


// issue a command in SyncBody context (uses session's interruptedCommand/NextMessageCommands)
bool TSyncSession::issueRootPtr(
  TSmlCommand *aSyncCommandP,                   // the command
  bool aNoResp,                                 // set if no response is wanted
  bool aIsOKSyncHdrStatus                       // set if this is sync hdr status
)
{
  // now issue
  return issuePtr(aSyncCommandP,fNextMessageCommands,fInterruptedCommandP,aNoResp,aIsOKSyncHdrStatus);
} // TSyncSession::issueRootPtr


// issue object passed as pointer (rather than pointer reference)
// normally used internally only
bool TSyncSession::issuePtr(
  TSmlCommand *aSyncCommandP,                   // the command
  TSmlCommandPContainer &aNextMessageCommands,  // the list to add the command if cannot be issued in this message
  TSmlCommand * &aInterruptedCommandP,          // where to store command ptr if it was interrupted
  bool aNoResp,                                 // set if no response is wanted
  bool aIsOKSyncHdrStatus                       // set if this is sync hdr status
)
{
  bool issued=true; // NULL issue is successful issue

  if (aSyncCommandP) {
    PDEBUGBLOCKFMT(("issue","issuing command",
      "Cmd=%s",
      aSyncCommandP->getName()
    ));
    SYSYNC_TRY {
      // check for not-to-be-sent commands
      if (aSyncCommandP->getDontSend()) {
        // command must not be sent, just silently discarded
        DEBUGPRINTFX(DBG_PROTO,("%s: not sent because fDontSend is set -> just delete",
          aSyncCommandP->getName()
        ));
        delete aSyncCommandP;
        issued=true; // counts as issued
        goto endissue;
      }
      // check if this commmand now triggers need to answer
      if (!aIsOKSyncHdrStatus) {
        fNeedToAnswer=true;
      }
      // check if outgoing message has already started. If not, queue command
      // for sending when message has started (client case, when status for
      // header must be evaluated before next header can be generated)
      if (!fOutgoingStarted) {
        fHeaderWaitCommands.push_back(aSyncCommandP);
        PDEBUGPRINTFX(DBG_SESSION+DBG_EXOTIC,(
          "Outgoing message header not yet generated, command '%s' queued",
          aSyncCommandP->getName()
        ));
        issued=true; // act as if issued
        goto endissue;
      }
      // check if this is a continuation of a suspended chunked item transfer
      // if yes, substitute the full data we currently have in the item (from
      // the database) with the buffered left-overs from the previous
      // session.
      aSyncCommandP->checkChunkContinuation();
      // check if message size restrictions or local buffer size
      // will prevent command from being sent now
      TSmlCommand *splitCmdP = NULL;
      if (!fOutgoingMessageFull) {
        #ifndef USE_SML_EVALUATION
          #error "This Implementation does not work any more without USE_SML_EVALUATION"
        #endif
        // check if enough room to send data
        sInt32 freeaftersend=aSyncCommandP->evalIssue(
          peekNextOutgoingCmdID(), // this will be the ID
          getOutgoingMsgID(),
          aNoResp
        );
        // - if room for new commands is smaller than expected message size
        sInt32 maxfree=getSmlWorkspaceFreeBytes();
        sInt32 sizetoend=maxfree-freeaftersend;
        #ifdef SYDEBUG
        PDEBUGPRINTFX(DBG_SESSION+DBG_EXOTIC,(
          "Command '%s': is %hd-th counted cmd, cmdsize(+tags needed to end msg)=%ld, available=%ld (maxfree=%ld, freeaftersend=%ld, notUsableBufferBytes()=%ld)",
          aSyncCommandP->getName(),
          fOutgoingCmds,
          (long)sizetoend, // size of command (+tags needed to end msg)
          (long)(maxfree-getNotUsableBufferBytes()),
          (long)maxfree,
          (long)freeaftersend,
          (long)getNotUsableBufferBytes()
        ));
        #endif
        #ifndef MINIMAL_CODE
        // - check for artifical debug chunking
        if (getSessionConfig()->fDebugChunkMaxSize && aSyncCommandP->isSyncOp()) {
          // always chunk commands over the configured size
          uInt32 cmdSize=getSmlWorkspaceFreeBytes()-freeaftersend;
          if (cmdSize>getSessionConfig()->fDebugChunkMaxSize) {
            // simulate a different free after send to force chunking
            sInt32 nfas=getNotUsableBufferBytes()-(cmdSize-getSessionConfig()->fDebugChunkMaxSize)+100;
            if (nfas<freeaftersend) {
              freeaftersend=nfas;
              PDEBUGPRINTFX(DBG_ERROR,("Attention: Debug Chunking enabled, freeaftersend adjusted to %ld",(long)freeaftersend));
            }
          }
        }
        #endif
        // - check if we can send this
        if (freeaftersend<=getNotUsableBufferBytes()) {
          // not enough space in this message
          PDEBUGPRINTFX(DBG_PROTO,(
            "command is %ld bytes too big to be sent as-is in this message",
            (long)(getNotUsableBufferBytes()-freeaftersend)
          ));
          bool sendable=false;
          // - first choice: try to split (available in SyncML 1.1 and later)
          if (getSyncMLVersion()>=syncml_vers_1_1) {
            // we can use moredata mechanism, try if it works for this command
            if (fOutgoingCmds>0 && sizetoend < getMaxOutgoingSize()/4) {
              // this is a small command, and not in best position
              // - command will most likely fit into next message, so end this message now
              fOutgoingMessageFull=true;
              sendable=true; // kind of "sendable" - as are confident it will be sendable in next message
              PDEBUGPRINTFX(DBG_PROTO,("command is less than 1/4 of maxmsgsize -> will likely fit into next message, so queue it"));
            }
            else {
              // - try to split
              //   reserve about 200 bytes for Meta Size
              splitCmdP = aSyncCommandP->splitCommand(getNotUsableBufferBytes()-freeaftersend+200);
              if (splitCmdP) {
                sendable=true;
              }
              else if (aSyncCommandP->canSplit()) {
                // command is basically splittable, but not right now - end message now and try in next
                fOutgoingMessageFull=true;
                sendable=true; // kind of "sendable" - just not now, but certainly later
              }
            }
          }
          // - second choice: try to shrink (mainly devInf)
          if (!sendable) {
            // gets sendable if we can shrink it to given size
            sendable = aSyncCommandP->shrinkCommand(getNotUsableBufferBytes()-freeaftersend);
            PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,(
              "shrinkCommand could %sshrink command by %ld bytes or more (which is needed to send it now)",
              sendable ? "" : "NOT ",
              (long)(getNotUsableBufferBytes()-freeaftersend)
            ));
          }
          // - third choice: send it later as first command
          if (!sendable) {
            if (fOutgoingCmds>0 && sizetoend<getMaxOutgoingSize()+300) {
              // not best position now, and not plain impossible (i.e. cmd<maxmsgsize+300)
              PDEBUGPRINTFX(DBG_PROTO,("command can fit into one message -> queue and hope we'll be able to send it as only command in subsequent message"));
              // - command will possibly fit into next message, so end this message now
              fOutgoingMessageFull=true;
              sendable=true; // kind of "sendable" - as we are hoping it will be sendable later
            }
            else {
              // this WAS already the best possible position to send or really too big
              // - we are in trouble, this command is unsendable
              PDEBUGPRINTFX(DBG_ERROR,("%s: Warning: command is too big to be sent at all -> discarded/mark for resend",
                aSyncCommandP->getName()
              ));
              // - try again in next session (data size might be smaller then, or remote might allow larger data at some time)
              aSyncCommandP->markForResend();
              // - just discard it for now
              delete aSyncCommandP;
              issued=false; // not issued (but also not queued again).
              goto endissue;
            }
          }
          // check if command was made sendable by splitting off a part
          if (splitCmdP) {
            // - queue command containing remaining data for issuing in next message
            aNextMessageCommands.push_back(splitCmdP);
            PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,("Split command, sending a chunk, queued rest of data for sending in next message"));
            // - Note: fOutgoingMessageFull is not set here (as first part of cmd must be sent first)
            //         but will be set below after issuing first part (due to splitCmdP!=NULL)
          }
        } // command too large to be sent now
      }
      // Queue if message is full, or if we have a interrupted command and
      // command to be sent is something other than status. This will make sure
      // incoming commands get their statuses before message is filled with other
      // explicitly generated commands
      if (fOutgoingMessageFull || (aInterruptedCommandP && aSyncCommandP->getCmdType()!=scmd_status)) {
        // - queue for issuing in next message
        aNextMessageCommands.push_back(aSyncCommandP);
        PDEBUGPRINTFX(DBG_HOT,(
          "No room for issueing in this message, command '%s' queued for next message",
          aSyncCommandP->getName()
        ));
        // - could not be issued, was queued
        issued=false;
      }
      else {
        // issue the command
        if (splitCmdP) fOutgoingMessageFull=true; // if we are sending a split command, message IS full after that!
        bool dodelete=true;
        if (aSyncCommandP->issue(getNextOutgoingCmdID(),fOutgoingMsgID,aNoResp)) {
          // command expects status and must be kept in list
          PDEBUGPRINTFX(DBG_HOT,("%s: issued as (outgoing MsgID=%ld, CmdID=%ld), now queueing for &html;<a name=\"IO_%ld_%ld\" href=\"#SO_%ld_%ld\">&html;status&html;</a>&html;",
            aSyncCommandP->getName(),
            (long)aSyncCommandP->getMsgID(),
            (long)aSyncCommandP->getCmdID(),
            (long)aSyncCommandP->getMsgID(),
            (long)aSyncCommandP->getCmdID(),
            (long)aSyncCommandP->getMsgID(),
            (long)aSyncCommandP->getCmdID()
          ));
          // - queue for status
          aSyncCommandP->setWaitingForStatus(true); // increment waiting for status count of this command
          fStatusWaitCommands.push_back(aSyncCommandP);
          dodelete=false;
        }
        else {
          // command does not expect status and can be deleted if it is finished
          PDEBUGPRINTFX(DBG_HOT,("%s: issued as (outgoing MsgID=%ld, CmdID=%ld), not waiting for status",
            aSyncCommandP->getName(),
            (long)aSyncCommandP->getMsgID(),
            (long)aSyncCommandP->getCmdID()
          ));
        }
        // Now as it is issued, count all "real" commands
        if (!aIsOKSyncHdrStatus) {
          // count outgoing "real" command, that is NOT SyncHdr OK status nor Alert 222 OK statu
          // Note that issuing a container command such as <sync> will have pre-decremented fOutgoingCmds
          // to compensate (container should not be counted)
          fOutgoingCmds++;
        }
        // test if finished issuing
        if (!aSyncCommandP->finished()) {
          // issuing was interrupted, continue at start of next message
          aInterruptedCommandP=aSyncCommandP; // remember
          dodelete=false;
          issued=false;
          PDEBUGPRINTFX(DBG_SESSION,("%s: issue not finished -> queued interrupted command",
            aSyncCommandP->getName()
          ));
        }
        // - delete if not queued
        if (dodelete) {
          PDEBUGPRINTFX(DBG_SESSION,("%s: issue finished and not waiting for status -> deleting command",
            aSyncCommandP->getName()
          ));
          delete aSyncCommandP;
        }
        // message size
        PDEBUGPRINTFX(DBG_PROTO,("Outgoing Message size is now %ld bytes",(long)getOutgoingMessageSize()));
      }
    }
    SYSYNC_CATCH (...)
      // exception during command issuing
      // - make sure command is deleted (as issue owns it now)
      delete aSyncCommandP;
      // make sure session gets aborted
      AbortSession(500,true); // local problem
      // close block
      PDEBUGENDBLOCK("issue");
      // re-throw
      SYSYNC_RETHROW;
    SYSYNC_ENDCATCH
  endissue:
    PDEBUGENDBLOCK("issue");
  } // if something to issue at all
  else {
    DEBUGPRINTFX(DBG_SESSION,("issuePtr called with NULL command"));
  }
  // return true if completely issued, false if interrupted or not issued at all
  return issued;
} // TSyncSession::issuePtr


// returns true if given number of bytes are transferable
// (not exceeding MaxMsgSize (in SyncML 1.0) or MaxObjSize (SyncML 1.1 and later)
bool TSyncSession::dataSizeTransferable(uInt32 aDataBytes)
{
  if (fSyncMLVersion<syncml_vers_1_1 || !fRemoteSupportsLargeObjects) {
    // SyncML 1.0: Data is transferable only if it is not more
    // than the room we had when the first syncop command in the message
    // was being sent
    return aDataBytes<=uInt32(fMaxRoomForData);
  }
  else {
    // SyncML 1.1 and later: Data is transferable if it is not more than
    // the MaxObjSize (if defined at all)
    return aDataBytes<=uInt32(fMaxOutgoingObjSize) || fMaxOutgoingObjSize==0;
  }
} // TSyncSession::dataSizeTransferable



#ifndef USE_SML_EVALUATION
  #error "This implementation requires USE_SML_EVALUATION"
#endif

// get how many bytes may not be used in the outgoing message buffer
// because of maxMsgSize restrictions
sInt32 TSyncSession::getNotUsableBufferBytes(void)
{
  if (fMaxOutgoingMsgSize) {
    // there is a limit
    sInt32 leavefree =
      getSmlWorkspaceFreeBytes() // what is free now
      + fOutgoingMsgSize // + what is already used = totally available
      - fMaxOutgoingMsgSize; // - max number to send = what we must leave free
    // if available space is less than what we may send, there's no need
    // to leave anything free.
    return leavefree > 0 ? leavefree : 0;
  }
  else {
    // limited only by workspace itself: no bytes need to be left free
    return 0;
  }
} // TSyncSession::getNotUsableBufferBytes


// get max size outgoing message may have (either defined by remote's maxmsgsize or local buffer space)
sInt32 TSyncSession::getMaxOutgoingSize(void)
{
  return fMaxOutgoingMsgSize>0 ? fMaxOutgoingMsgSize : getSmlWorkspaceFreeBytes();
} // TSyncSession::getMaxOutgoingSize


/// @brief mark all pending items for a datastore for resume
///   (those items that are in a session queue for being issued or getting status)
void TSyncSession::markPendingForResume(TLocalEngineDS *aForDatastoreP)
{
  TSmlCommandPContainer::iterator pos;
  // - commands not issued yet because header not yet generated
  for (pos=fHeaderWaitCommands.begin(); pos!=fHeaderWaitCommands.end(); ++pos) {
    (*pos)->markPendingForResume(aForDatastoreP,true); // unsent
  }
  // - commands not issued yet because they belong at the end of the message
  for (pos=fEndOfMessageCommands.begin(); pos!=fEndOfMessageCommands.end(); ++pos) {
    (*pos)->markPendingForResume(aForDatastoreP,true); // unsent
  }
  // - interrupted and next-message commands
  markPendingForResume(fNextMessageCommands,fInterruptedCommandP,aForDatastoreP);
  // - next package commands
  for (pos=fNextPackageCommands.begin(); pos!=fNextPackageCommands.end(); ++pos) {
    (*pos)->markPendingForResume(aForDatastoreP,true); // unsent
  }
  // - commands waiting for status
  for (pos=fStatusWaitCommands.begin(); pos!=fStatusWaitCommands.end(); ++pos) {
    (*pos)->markPendingForResume(aForDatastoreP,false); // these are already sent (except if they are waiting for chunk ok 213 status)!
  }
} // TSyncSession::markPendingForResume


/// @brief mark all pending items for a datastore for resume
///   (those items that are in a session queue for being issued or getting status)
void TSyncSession::markPendingForResume(
  TSmlCommandPContainer &aNextMessageCommands,
  TSmlCommand *aInterruptedCommandP,
  TLocalEngineDS *aForDatastoreP
)
{
  // - all those that are in an interrupted command
  if (aInterruptedCommandP) {
    aInterruptedCommandP->markPendingForResume(aForDatastoreP,true); // these are unsent
  }
  // - all those pending for next message
  TSmlCommandPContainer::iterator pos;
  for (pos=aNextMessageCommands.begin(); pos!=aNextMessageCommands.end(); ++pos) {
    (*pos)->markPendingForResume(aForDatastoreP,true); // these are unsent
  }
} // TSyncSession::markPendingForResume


// continue interrupted or prevented issue of root level commands
void TSyncSession::ContinuePackageRoot(void)
{
  // check if we have anything to send, and if so, reset the count
  if (fNextMessageCommands.size()>0 || fInterruptedCommandP) {
    #ifdef SYDEBUG
    if (fNextMessageRequests) {
      PDEBUGPRINTFX(DBG_PROTO,("Fulfilling %ld Next-Message-Request-Alerts 222 by sending commands now",(long)fNextMessageRequests));
    }
    #endif
    fNextMessageRequests=0; // sent something, request fulfilled
  }
  ContinuePackage(fNextMessageCommands,fInterruptedCommandP);
} // TSyncSession::ContinuePackageRoot


// continue interrupted or prevented issue in next package
void TSyncSession::ContinuePackage(
  TSmlCommandPContainer &aNextMessageCommands,
  TSmlCommand * &aInterruptedCommandP
)
{
  TSmlCommand *cmdP = aInterruptedCommandP;
  // - first restart interrupted command
  if (cmdP && !isAborted() && !isSuspending()) {
    // first check if interrupted command has received status
    if (cmdP->isWaitingForStatus() && getSessionConfig()->fWaitForStatusOfInterrupted) {
      // Command to be continued has not yet received status, so wait with continuing it
      PDEBUGPRINTFX(DBG_SESSION,(
        "Interrupted command '%s' (outgoing MsgID=%ld, CmdID=%ld) is still waiting for status -> do not continue nor send queued commands",
        cmdP->getName(),
        (long)cmdP->getMsgID(),
        (long)cmdP->getCmdID()
      ));
      // aInterruptedCommandP is still set, prevents sending of queued commands
    }
    else {
      PDEBUGPRINTFX(DBG_SESSION,("Sending command that was interrupted at end of last message"));
      // there is an interrupted command, continue it
      bool dodelete=true;
      // if an interrupted command must be continued, this can't be an OK for SyncHdr
      fNeedToAnswer=true;
      bool newIssue=false;
      // now continue issuing
      bool queueForStatus=cmdP->continueIssue(newIssue);
      // check if complete re-issuing is needed
      if (newIssue) {
        // command must be re-issued as if it was a new command
        PDEBUGPRINTFX(DBG_SESSION,("%s: continuing command by issuing anew",cmdP->getName()));
        // interruption is over for now, IssuePtr will set it again if interrupted again
        aInterruptedCommandP=NULL;
        // IssuePtr will take care of all needed status queueing and deleting command if finished etc.
        issuePtr(cmdP,aNextMessageCommands,aInterruptedCommandP);
        // never delete, as issuePtr will have done it if needed
        dodelete=false;
      }
      else {
        // no complete re-issuing needed, just check if we need to queue for status (again)
        if (queueForStatus) {
          // command expects status (again?) and must be kept in list
          PDEBUGPRINTFX(DBG_SESSION,("%s: continued, now queueing for status (again) as (outgoing MsgID=%ld, CmdID=%ld)",
            cmdP->getName(),
            (long)cmdP->getMsgID(),
            (long)cmdP->getCmdID()
          ));
          // - queue for status (note that every SyncML 1.1 chunk wants to see a status 213 in any case!)
          if (!cmdP->isWaitingForStatus()) {
            // only put to the queue again if not already waiting there
            fStatusWaitCommands.push_back(cmdP);
            cmdP->setWaitingForStatus(true);
          }
          else {
            // already waiting, so it's already in the queue - do not push it again
            PDEBUGPRINTFX(DBG_SESSION,("%s: continued command was already waiting for status, do not push again", cmdP->getName()));
          }
          dodelete=false;
        }
      }
      // test if completely issued now
      if (!cmdP->finished()) {
        // issuing was again interrupted, continue at start of next message
        // - keep aInterruptedCommandP pointer unchanged
        dodelete=false;
        PDEBUGPRINTFX(DBG_SESSION,("%s: continueIssue not finished -> queued interrupted command again",
          cmdP->getName()
        ));
      }
      else {
        // no interrupted command any more
        aInterruptedCommandP=NULL;
      }
      // delete if not queued
      if (dodelete) {
        PDEBUGPRINTFX(DBG_SESSION,("%s: continueIssue finished and not waiting for status -> deleting command",
          cmdP->getName()
        ));
        delete cmdP;
      }
    } // if status was received for interrupted command
  } // if interrupted command
  // send commands from queue (until interrupted again)
  #ifdef SYDEBUG
  if (!isAborted() && !isSuspending()  && !aInterruptedCommandP && aNextMessageCommands.size()>0) {
    PDEBUGPRINTFX(DBG_SESSION,("Sending %ld commands that didn't make it into last message",(long)aNextMessageCommands.size()));
  }
  #endif
  TSmlCommandPContainer::iterator pos;
  while (!isAborted() && !isSuspending() && !aInterruptedCommandP && !outgoingMessageFull()) {
    // first in list
    pos=aNextMessageCommands.begin();
    if (pos==aNextMessageCommands.end()) break; // done
    // take command out of the list
    cmdP=(*pos);
    aNextMessageCommands.erase(pos);
    // issue it (without luck, might land in the queue again --> %%% endless retry??)
    if (!issuePtr(cmdP,aNextMessageCommands,aInterruptedCommandP)) break;
  }
} // TSyncSession::ContinuePackage



// create, send and delete SyncHeader "command"
void TSyncSession::issueHeader(bool aNoResp)
{
  #ifdef SYDEBUG
  // Start output translation before issuing outgoing header
  XMLTranslationOutgoingStart();
  #endif
  if (IS_CLIENT) {
    #ifdef SYDEBUG
    // for client, document exchange starts with outgoing message
    // but for server, SyncML_Outgoing is started before SyncML_Incoming, as SyncML_Incoming ends first
    PDEBUGBLOCKDESC("SyncML_Outgoing","start of new outgoing message");
    PDEBUGPRINTFX(DBG_HOT,("=================> Started new outgoing message"));
    #endif
    #ifdef EXPIRES_AFTER_DATE
    // set 1/4 of the date here
    fCopyOfScrambledNow=((getSyncAppBase()->fScrambledNow)<<2)+503; // scramble again a little
    #endif
  }
  // create and send response header
  TSyncHeader *syncheaderP;
  MP_NEW(syncheaderP,DBG_OBJINST,"TSyncHeader",TSyncHeader(this,aNoResp));
  PDEBUGBLOCKFMT(("SyncHdr","SyncHdr generation","SyncMLVers=%s|OutgoingMsgID=%ld",SyncMLVerDTDNames[fSyncMLVersion],(long)fOutgoingMsgID));
  SYSYNC_TRY {
    // Note: do not use session's issue(), as this is designed for real commands, not headers
    if (syncheaderP->issue(0,fOutgoingMsgID)) {
      // - queue for status
      PDEBUGPRINTFX(DBG_PROTO,("SyncHdr: issued in MsgID=%ld, now queueing for status",(long)syncheaderP->getMsgID()));
      syncheaderP->setWaitingForStatus(true);
      fStatusWaitCommands.push_back(syncheaderP);
    }
    else {
      PDEBUGPRINTFX(DBG_PROTO,("SyncHdr: issued in MsgID=%ld, now deleting",(long)syncheaderP->getMsgID()));
      delete syncheaderP; // not used any more
    }
    DEBUGPRINTFX(DBG_PROTO,("Outgoing Message size is now %ld bytes",(long)getOutgoingMessageSize()));
    // - init number of commands in message
    fOutgoingCmds=0;
    // - now issue all commands that could not yet be sent because outgoing
    //   message was not started
    #ifdef SYDEBUG
    if (fHeaderWaitCommands.size()>0) {
      PDEBUGPRINTFX(DBG_SESSION,("Sending %ld commands that were queued because header was not yet generated",(long)fHeaderWaitCommands.size()));
    }
    #endif
    TSmlCommandPContainer::iterator pos;
    while (!fAborted) {
      // first in list
      pos=fHeaderWaitCommands.begin();
      if (pos==fHeaderWaitCommands.end()) break; // done
      // take command out of the list
      TSmlCommand *cmdP=(*pos);
      fHeaderWaitCommands.erase(pos);
      if (!cmdP) {
        DEBUGPRINTFX(DBG_ERROR,("There was a NULL command in the fHeaderWaitCommands queue?!?"));
        continue;
      }
      // issue now (as if all were OK for synchdr, because fNeedToAnswer may not be affected here!)
      if (!issuePtr(cmdP,fNextMessageCommands,fInterruptedCommandP,false,true)) {
        PDEBUGPRINTFX(DBG_SESSION,("Could not issue queued command"));
        break;
      }
    }
    PDEBUGENDBLOCK("SyncHdr");
  }
  SYSYNC_CATCH (...)
    PDEBUGENDBLOCK("SyncHdr");
    delete syncheaderP; // not used any more
    SYSYNC_RETHROW; // rethrow
  SYSYNC_ENDCATCH
} // TSyncSession::IssueHeader




// process a command (analyze and execute it).
// Ownership of command is passed to process() in all cases.
// Note: This method does not throw exceptions (catches all) and
//       is suitable for being called without further precautions from
//       SyncML toolkit callbacks. Exceptions are translated to
//       smlXXX error codes.
Ret_t TSyncSession::process(TSmlCommand *aSyncCommandP)
{
  if (aSyncCommandP) {
    SYSYNC_TRY {
      // first analyze it (before we can open the block, as we need to find cmdID fisrt)
      if (!aSyncCommandP->analyze(fIncomingState)) {
        // bad command
        PDEBUGPRINTFX(DBG_ERROR,("%s: command failed analyze() -> aborting session",aSyncCommandP->getName()));
        AbortSession(400,true); // local problem
        delete aSyncCommandP;
      }
      else {
        // command ok so far (has cmdid, so we can refer to it)
        PDEBUGBLOCKFMT(("processCmd","Processing incoming command",
          "Cmd=%s|IncomingMsgID=%ld|CmdID=%ld",
          aSyncCommandP->getName(),
          (long)aSyncCommandP->getMsgID(),
          (long)aSyncCommandP->getCmdID()
        ));
        SYSYNC_TRY {
          // - test if processing is enabled
          if (fIgnoreIncomingCommands && !aSyncCommandP->neverIgnore()) {
            // commands must be ignored (fatal error in previous command/header)
            PDEBUGPRINTFX(DBG_ERROR,("%s: IGNORED ",aSyncCommandP->getName()));
            // - still generate status (except if aborted status is 0, which means silent abort)
            //   but DO NOT generate status for status!
            if (fStatusCodeForIgnored!=0 && aSyncCommandP->getCmdType()!=scmd_status) {
              PDEBUGPRINTFX(DBG_ERROR,("    Sending status %hd for ignored command",fStatusCodeForIgnored));
              TStatusCommand *aStatusCmdP = aSyncCommandP->newStatusCommand(fStatusCodeForIgnored);
              issueRootPtr(aStatusCmdP);
            }
            // - delete unexecuted
            delete aSyncCommandP;
          }
          else if (
            fStrictExecOrdering &&
            fDelayedExecutionCommands.size()>0 &&
            aSyncCommandP->getCmdType()!=scmd_status &&
            aSyncCommandP->getCmdType()!=scmd_alert
          ) {
            // some commands have been delayed already -> delay all non-statuses and alerts as well
            PDEBUGPRINTFX(DBG_SESSION,("%s: command received after other commands needed to be delayed -> must be delayed, too",aSyncCommandP->getName()));
            // - put into delayed execution queue
            delayExecUntilNextRequest(aSyncCommandP);
          }
          else {
            // command is ok, execute it
            fCmdIncomingState=aSyncCommandP->getPackageState();
            if (aSyncCommandP->execute()) {
              // execution finished, can be deleted
              if (aSyncCommandP->finished()) {
                PDEBUGPRINTFX(DBG_SESSION,("%s: command finished execution -> deleting",aSyncCommandP->getName()));
                delete aSyncCommandP;
              }
              else {
                PDEBUGPRINTFX(DBG_SESSION,("%s: command NOT finished execution, NOT deleting now",aSyncCommandP->getName()));
              }
            }
            else {
              // command has not finished execution, must be retried after next incoming message
              PDEBUGPRINTFX(DBG_SESSION,("%s: command wants re-execution later -> queueing",aSyncCommandP->getName()));
              // - put into delayed execution queue
              delayExecUntilNextRequest(aSyncCommandP);
            }
          } // if not ignored
          // successfully processed
          PDEBUGENDBLOCK("processCmd");
        } // try
        SYSYNC_CATCH (...)
          PDEBUGENDBLOCK("processCmd");
          SYSYNC_RETHROW;
        SYSYNC_ENDCATCH
      } // if analyzed successfully
    }
    SYSYNC_CATCH (TSmlException &e)
      // Sml error exception somewhere in command processing
      // - make sure command is deleted (as issue owns it now)
      delete aSyncCommandP;
      PDEBUGPRINTFX(DBG_ERROR,("WARNING: process(cmd) SmlException: %s, smlerr=%hd",e.what(),e.getSmlError()));
      // - return SML error that caused this exception
      return e.getSmlError();
    SYSYNC_ENDCATCH
    SYSYNC_CATCH (TSyncException &e)
      // sync exception during command processing
      // - make sure command is deleted (as issue owns it now)
      delete aSyncCommandP;
      PDEBUGPRINTFX(DBG_ERROR,("WARNING: process(cmd) SyncException: %s, status=%hd",e.what(),e.status()));
      // - unspecific SyncML toolkit error, causes session to abort
      return SML_ERR_UNSPECIFIC;
    SYSYNC_ENDCATCH
    SYSYNC_CATCH (exception &e)
      // C++ exception during command processing
      // - make sure command is deleted (as issue owns it now)
      delete aSyncCommandP;
      PDEBUGPRINTFX(DBG_ERROR,("WARNING: process(cmd) Exception: %s",e.what()));
      // - unspecific SyncML toolkit error, causes session to abort
      return SML_ERR_UNSPECIFIC;
    SYSYNC_ENDCATCH
    SYSYNC_CATCH (...)
      // other exception during command processing
      // - make sure command is deleted (as issue owns it now)
      delete aSyncCommandP;
      PDEBUGPRINTFX(DBG_ERROR,("WARNING: process(cmd) unknown exception: -> cmd deleted"));
      // - unspecific SyncML toolkit error, causes session to abort
      return SML_ERR_UNSPECIFIC;
    SYSYNC_ENDCATCH
  }
  // successful
  return SML_ERR_OK;
} // TSyncSession::process


// process synchdr (analyze and execute it).
// Ownership of command is passed to process() in all cases.
// Note: May cause session reset if message sequence numbers are not ok.
Ret_t TSyncSession::processHeader(TSyncHeader *aSyncHdrP)
{
  if (aSyncHdrP) {
    PDEBUGBLOCKDESC("processHdr","Processing incoming SyncHdr");
    SYSYNC_TRY {
      // init some session vars
      // - do not ignore commands by default
      fIgnoreIncomingCommands=false;
      // - need to answer raises out of first non-synchdr-status command issue()d
      fNeedToAnswer=false; // no need yet
      // - initialize message status
      fMsgNoResp=false; // default to response for messages
      fIncomingMsgID++; // count this incoming message
      // init flag
      bool tryagain=false;
      do {
        // first analyze header
        if (!aSyncHdrP->analyze(fIncomingState)) {
          // bad command
          PDEBUGPRINTFX(DBG_ERROR,("%s: failed analyze() -> deleting",aSyncHdrP->getName()));
          delete aSyncHdrP;
        }
        else {
          // command is ok, execute it
          fCmdIncomingState=aSyncHdrP->getPackageState();
          PDEBUGBLOCKFMT(("SyncHdr","Processing incoming SyncHdr",
            "IncomingMsgID=%ld",
            (long)fIncomingMsgID
          ));
          SYSYNC_TRY {
            if (aSyncHdrP->execute()) {
              // show session info after processing header
              #ifdef SYDEBUG
              PDEBUGPRINTFX(DBG_HOT,("Incoming SyncHdr processed, incomingMsgID=%ld, SyncMLVers=%s",(long)fIncomingMsgID,SyncMLVerDTDNames[fSyncMLVersion]));
              PDEBUGPRINTFX(DBG_HOT,("- Session ID='%s'",fSynchdrSessionID.c_str()));
              PDEBUGPRINTFX(DBG_HOT,("- Source (Remote party): URI='%s' DisplayName='%s'",fRemoteURI.c_str(),fRemoteName.c_str()));
              PDEBUGPRINTFX(DBG_HOT,("- Response to be sent to URI='%s'",fRespondURI.empty() ? "[none specified, back to source]" : fRespondURI.c_str()));
              PDEBUGPRINTFX(DBG_HOT,("- Target (Local party) : URI='%s' DisplayName='%s'",fLocalURI.c_str(),fLocalName.c_str()));
              #endif
              CONSOLEPRINTF(("> SyncML message #%ld received from '%s'",(long)fIncomingMsgID,fRemoteURI.c_str()));
              // - UTC support is implied for SyncML 1.0 (as most devices support it, and
              //   there is was no way to signal it in 1.0).
              if (!fRemoteDevInfKnown && fSyncMLVersion==syncml_vers_1_0) fRemoteCanHandleUTC=true;
              // execution finished, can be deleted
              PDEBUGPRINTFX(DBG_SESSION,("%s: finished execution -> deleting",aSyncHdrP->getName()));
              delete aSyncHdrP;
              // now execute delayed commands (before executing new ones)
              PDEBUGPRINTFX(DBG_SESSION,("New message: Executing %ld delayed commands",(long)fDelayedExecutionCommands.size()));
              TSmlCommandPContainer::iterator pos=fDelayedExecutionCommands.begin();
              bool syncEndAfterSyncPackageEnd=false;
              while (pos!=fDelayedExecutionCommands.end()) {
                // execute again
                TSmlCommand *cmdP = (*pos);
                // command ok so far (has cmdid, so we can refer to it)
                PDEBUGBLOCKFMT(("executeDelayedCmd","Re-executing command from delayed queue",
                  "Cmd=%s|IncomingMsgID=%ld|CmdID=%ld",
                  cmdP->getName(),
                  (long)cmdP->getMsgID(),
                  (long)cmdP->getCmdID()
                ));
                SYSYNC_TRY {
                  fCmdIncomingState=cmdP->getPackageState();
                  if (cmdP->execute()) {
                    // check if this was a syncend which was now executed AFTER the end of the incoming sync package
                    if (cmdP->getCmdType()==scmd_syncend) {
                      fDelayedExecSyncEnds--; // count executed syncend
                      if (cmdP->getPackageState()!=fIncomingState)
                        syncEndAfterSyncPackageEnd=true; // remember that we had at least one
                    }
                    // execution finished, can be deleted
                    PDEBUGPRINTFX(DBG_SESSION,("%s: command finished execution -> deleting",cmdP->getName()));
                    delete cmdP;
                    // delete from queue and get next
                    pos=fDelayedExecutionCommands.erase(pos);
                  }
                  else {
                    // command has not finished execution, must be retried after next incoming message
                    PDEBUGPRINTFX(DBG_SESSION,("%s: command STILL NOT finished execution -> keep it (and all follwoing) in queue ",cmdP->getName()));
                    // keep this and all subsequent commands in the queue
                    PDEBUGENDBLOCK("executeDelayedCmd");
                    break;
                  }
                  PDEBUGENDBLOCK("executeDelayedCmd");
                } // try
                SYSYNC_CATCH (...)
                  PDEBUGENDBLOCK("executeDelayedCmd");
                  SYSYNC_RETHROW;
                SYSYNC_ENDCATCH
              }
              // check if all delayed commands are executed now
              if (fDelayedExecSyncEnds<=0 && syncEndAfterSyncPackageEnd) {
                // there was at least one queued syncend executed AFTER end of incoming sync package
                // This means that we must finalize the sync-from-remote phase for the datastores here
                // (as it was suppressed when the incoming sync package had ended)
                TLocalDataStorePContainer::iterator dspos;
                for (dspos=fLocalDataStores.begin(); dspos!=fLocalDataStores.end(); ++dspos) {
                  (*dspos)->engEndOfSyncFromRemote(true);
                }
              }
              else {
                PDEBUGPRINTFX(DBG_SESSION,("%ld delayed commands could not yet be executed and are left in the queue for next message",(long)fDelayedExecutionCommands.size()));
              }
              // now issue next package commands if any
              if (fNewOutgoingPackage) {
                PDEBUGPRINTFX(DBG_SESSION,("New package: Sending %ld commands that were generated earlier for this package",(long)fNextPackageCommands.size()));
                TSmlCommandPContainer::iterator nppos;
                for (nppos=fNextPackageCommands.begin(); nppos!=fNextPackageCommands.end(); nppos++) {
                  // issue it (might land in NextMessageCommands)
                  issueRootPtr((*nppos));
                }
                // done sending next package commands
                fNextPackageCommands.clear(); // clear list
              }
              // done, don't try again
              break;
            }
            else {
              // unexecutable SyncHdr
              // - could be resent message
              if (fMessageRetried) {
                // simply abort processing, let transport handle this
                PDEBUGENDBLOCK("processHdr");
                return LOCERR_RETRYMSG; // signal retry has happened
              }
              // - let agent decide what to do (and whether to try again executing the command)
              DEBUGPRINTFX(DBG_SESSION,("%s: Cannot be executed properly, trying to recover",aSyncHdrP->getName()));
              tryagain = syncHdrFailure(tryagain);
            } // synchdr not ok
          } // try
          SYSYNC_CATCH (...)
            PDEBUGENDBLOCK("SyncHdr");
            SYSYNC_RETHROW;
          SYSYNC_ENDCATCH
        }
      } while(tryagain);
      PDEBUGENDBLOCK("SyncHdr");
    }
    SYSYNC_CATCH (TSmlException &e)
      // Sml error exception somewhere in command processing
      // - make sure command is deleted (as issue owns it now)
      delete aSyncHdrP;
      PDEBUGPRINTFX(DBG_ERROR,("WARNING: synchdr SmlException: %s, smlerr=%hd",e.what(),e.getSmlError()));
      PDEBUGENDBLOCK("processHdr");
      // - return SML error that caused this exception
      return e.getSmlError();
    SYSYNC_ENDCATCH
    SYSYNC_CATCH (exception &e)
      // C++ exception somewhere in command processing
      // - make sure command is deleted (as issue owns it now)
      delete aSyncHdrP;
      PDEBUGPRINTFX(DBG_ERROR,("WARNING: synchdr exception: %s",e.what()));
      PDEBUGENDBLOCK("processHdr");
      // - return SML error that caused this exception
      return SML_ERR_UNSPECIFIC;
    SYSYNC_ENDCATCH
    SYSYNC_CATCH (...)
      // other exception during command processing
      // - make sure command is deleted (as issue owns it now)
      delete aSyncHdrP;
      PDEBUGPRINTFX(DBG_ERROR,("WARNING: synchdr unknown-class exception: -> deleted"));
      PDEBUGENDBLOCK("processHdr");
      // - unspecific SyncML toolkit error, causes session to abort
      return SML_ERR_UNSPECIFIC;
    SYSYNC_ENDCATCH
  }
  // successful
  PDEBUGENDBLOCK("processHdr");
  return SML_ERR_OK;
} // TSyncSession::processHeader

#ifdef __PALM_OS__
#pragma segment session2
#endif

// %%% integrate Results command here, too (that, is, make a common
//     ancestor for both TStatusCommand and TResultsCommand which
//     is then handled here in common).
// handle a status "command"
// Ownership of status is passed to handleStatus() in all cases.
// Note: This method does not throw exceptions (catches all) and
//       is suitable for being called without further precautions from
//       SyncML toolkit callbacks. Exceptions are translated to
//       smlXXX error codes.
Ret_t TSyncSession::handleStatus(TStatusCommand *aStatusCommandP)
{
  if (aStatusCommandP) {
    PDEBUGBLOCKDESC("processStatus","Processing incoming Status");
    SYSYNC_TRY {
      // analyze first
      if (!aStatusCommandP->analyze(fIncomingState)) {
        // bad command
        PDEBUGPRINTFX(DBG_SESSION,("%s: status failed analyze() -> deleting",aStatusCommandP->getName()));
        delete aStatusCommandP;
      }
      else {
        bool found=false;
        // status is ok, find matching command
        TSmlCommandPContainer::iterator pos;
        for (pos=fStatusWaitCommands.begin(); pos!=fStatusWaitCommands.end(); ++pos) {
          if ((*pos)->matchStatus(aStatusCommandP)) {
            PDEBUGPRINTFX(DBG_PROTO,("Found matching command '%s' for Status",(*pos)->getName()));
            (*pos)->setWaitingForStatus(false); // has received status
            found=true;
            if (fIgnoreIncomingCommands) {
              // ignore statuses, but remove waiting command from queue
              if ((*pos)->finished()) delete (*pos); // unfinished are owned otherwise and must not be deleted
              fStatusWaitCommands.erase(pos);
              PDEBUGPRINTFX(DBG_SESSION,("Status ignored, command considered done -> deleted"));
            }
            else {
              // let descendants know when we process a required status
              if ((*pos)->statusEssential()) {
                essentialStatusReceived();
              }
              // normally process status
              if ((*pos)->handleStatus(aStatusCommandP)) {
                PDEBUGPRINTFX(DBG_SESSION,("Status: processed, removed command '%s' from status wait queue",(*pos)->getName()));
                // done with command, remove from queue
                if ((*pos)->finished()) {
                  // - if this is an interrupted command, make sure to remove pointer
                  if ((*pos)==fInterruptedCommandP) fInterruptedCommandP=NULL;
                  // - delete command itself
                  //   NOTE; if not finished, command is owned otherwise and must
                  //   persist
                  PDEBUGPRINTFX(DBG_SESSION,("Status: command '%s' has handled status and allows to be deleted",(*pos)->getName()));
                  delete (*pos);
                }
                else {
                  PDEBUGPRINTFX(DBG_SESSION,("Status: command '%s' has handled status, but not finished() -> NOT deleted",(*pos)->getName()));
                }
                // - anyway, remove from list
                fStatusWaitCommands.erase(pos);
              }
              else {
                // command not yet acknowledged, keep in queue
                (*pos)->setWaitingForStatus(true); // is again waiting for a status
                PDEBUGPRINTFX(DBG_SESSION,("(intermediate) Status processed, command kept in queue, not deleted"));
              }
            } // else normal processing
            break; // exit for loop (iterator is not ok any more)
          }
        } // for
        if (!found) {
          // no matching command found
          PDEBUGPRINTFX(DBG_ERROR,("No command found for status -> ignoring"));
        }
        // now delete status
        delete aStatusCommandP;
      } // if analyzed successfully
    }
    SYSYNC_CATCH (TSmlException &e)
      // Sml error exception somewhere in command processing
      // - make sure command is deleted (as issue owns it now)
      delete aStatusCommandP;
      PDEBUGPRINTFX(DBG_ERROR,("WARNING: handleStatus SmlException: %s, smlerr=%hd",e.what(),e.getSmlError()));
      PDEBUGENDBLOCK("processStatus");
      // - return SML error that caused this exception
      return e.getSmlError();
    SYSYNC_ENDCATCH
    SYSYNC_CATCH (exception &e)
      // Sml error exception somewhere in command processing
      // - make sure command is deleted (as issue owns it now)
      delete aStatusCommandP;
      PDEBUGPRINTFX(DBG_ERROR,("WARNING: handleStatus exception: %s",e.what()));
      PDEBUGENDBLOCK("processStatus");
      // - unspecific SyncML toolkit error, causes session to abort
      return SML_ERR_UNSPECIFIC;
    SYSYNC_ENDCATCH
    SYSYNC_CATCH (...)
      // other exception during command processing
      // - make sure command is deleted (as issue owns it now)
      delete aStatusCommandP;
      PDEBUGPRINTFX(DBG_ERROR,("WARNING: handleStatus unknown-class exception: -> deleted"));
      PDEBUGENDBLOCK("processStatus");
      // - unspecific SyncML toolkit error, causes session to abort
      return SML_ERR_UNSPECIFIC;
    SYSYNC_ENDCATCH
  }
  // successful
  PDEBUGENDBLOCK("processStatus");
  return SML_ERR_OK;
} // TSyncSession::handleStatus


// Session level meta
SmlPcdataPtr_t TSyncSession::newHeaderMeta(void)
{
  SmlPcdataPtr_t metaP = NULL;

  // create meta for initialisation message only
  #ifdef SEND_MAXMSGSIZE_ON_INIT_ONLY
  if (fOutgoingState<=psta_init)
  #endif
  {
    metaP=newMeta();
    SmlMetInfMetInfPtr_t metinfP = smlPCDataToMetInfP(metaP);
    // - add max message size
    metinfP->maxmsgsize=newPCDataLong(getRootConfig()->fLocalMaxMsgSize);
    if (
      (getRootConfig()->fLocalMaxObjSize>0) &&
      (fSyncMLVersion>=syncml_vers_1_1)
    ) {
      // SyncML 1.1 has object size
      metinfP->maxobjsize=newPCDataLong(getRootConfig()->fLocalMaxObjSize);
    }
  }
  return metaP;
} // TSyncSession::newHeaderMeta


// create new SyncHdr structure for TSyncHeader command
// (here because all data for this is in session anyway)
// Called exclusively from TSyncHeader command
SmlSyncHdrPtr_t TSyncSession::NewOutgoingSyncHdr(bool aOutgoingNoResp)
{
  SmlSyncHdrPtr_t headerP;

  MP_SHOWCURRENT(DBG_PROFILE,"Start of outgoing message");
  // set response status for entire message
  fOutgoingNoResp=aOutgoingNoResp;
  // get new number for this message
  fOutgoingMsgID++;
  // reset message size counting
  fOutgoingMsgSize=0;
  // reset command ID
  fOutgoingCmdID=0;
  // now compose Sync Header from session vars
  // - create empty header
  headerP = SML_NEW(SmlSyncHdr_t);
  #ifdef EXPIRES_AFTER_DATE
  // prepare for a check, convert back to normal value * 4
  sInt32 scramblednow4 = (fCopyOfScrambledNow-503);
  #endif
  SYSYNC_TRY {
    // set proto element type to make it auto-disposable
    headerP->elementType=SML_PE_HEADER;
    // set version information
    headerP->version=newPCDataString(SyncMLVerDTDNames[fSyncMLVersion]);
    headerP->proto=newPCDataString(SyncMLVerProtoNames[fSyncMLVersion]);
    // set session ID
    headerP->sessionID=newPCDataString(fSynchdrSessionID);
    // set new message ID for this message
    #ifdef APP_CAN_EXPIRE
    // check for expiry again
    #ifdef EXPIRES_AFTER_DATE
    // - has hard expiry date
    if (
      (scramblednow4>SCRAMBLED_EXPIRY_VALUE*4)
      #ifdef SYSER_REGISTRATION
      && (!getSyncAppBase()->fRegOK) // only abort if no registration (but accept timed registration)
      #endif
    )
      getSyncAppBase()->fAppExpiryStatus=LOCERR_EXPIRED;
    #else
    // - no hard expiry date, just check if license is still valid
    if (
      !getSyncAppBase()->fRegOK || // no registered at all
      getSyncAppBase()->fDaysLeft==0 // or expired
    )
      getSyncAppBase()->fAppExpiryStatus=LOCERR_EXPIRED;
    #endif
    #endif
    headerP->msgID=newPCDataLong(fOutgoingMsgID);
    // flags
    headerP->flags=fOutgoingNoResp ? SmlNoResp_f : 0;
    // target (URI/Name of Remote party)
    // Note: Tsutomu Uenoyama (uenoyama@trl.mei.co.jp) sais in syncml feedback
    //       list that server RespURI behaviour should
    //       be reflecting received RespURI in target, so we do it:
    headerP->target=newLocation(
      fRespondURI.empty() ? fRemoteURI.c_str() : fRespondURI.c_str(),
      fRemoteName.c_str()
    );
    PDEBUGPRINTFX(DBG_PROTO,("Target (Remote URI) = '%s'",fRespondURI.empty() ? fRemoteURI.c_str() : fRespondURI.c_str()));
    // source (URI / User-Name of local party)
    // NOTE: The LocName must contain the name of the user and not
    //       the name of the device (this is a SyncML 1.0.1 correction
    //       to make MD5 auth implementable - we need a clear-text user name)
    headerP->source=newLocation(
      fLocalURI.c_str(),
      getUsernameForRemote() // user name for remote login, NULL if none available
    );
    // add respURI if local party cannot be responded to via normal URI
    // New added for T68i: do not send a RespURI for an aborted session
    if (!isAborted() && fMessageAuthorized)
      headerP->respURI=newResponseURIForRemote();
    else
      headerP->respURI=NULL;
    // add credentials if remote needs them
    headerP->cred=newCredentialsForRemote();
    // agent-specific meta
    headerP->meta=newHeaderMeta();
  }
  SYSYNC_CATCH (...)
    // make sure header is disposed
    smlFreeProtoElement(headerP);
    SYSYNC_RETHROW; // re-throw
  SYSYNC_ENDCATCH
  // return it
  return headerP;
} // TSyncSession::NewOutgoingSyncHdr


// delay command for execution at beginning of next received message
void TSyncSession::delayExecUntilNextRequest(TSmlCommand *aCommand)
{
  // push into delay queue
  fDelayedExecutionCommands.push_back(aCommand);
  // a delayed (=not processed) syncstart must clear the current fLocalSyncDatastoreP,
  // as it is not yet known for that <sync>. This causes syncops to receive a NULL datastore
  // at creation, so they must check for that and get it at execute().
  if (aCommand->getCmdType()==scmd_sync) {
    // delayed <sync> has no datastore (yet)
    fLocalSyncDatastoreP=NULL;
  }
  else if (aCommand->getCmdType()==scmd_syncend) {
    // count delayed syncends as they need special care later
    fDelayedExecSyncEnds++;
    // and forget current datastore - safety only, should be NULL here anyway
    fLocalSyncDatastoreP=NULL;
  }
} // TSyncSession::delayExecUntilNextRequest


// remote party requests next message by Alert 222
void TSyncSession::nextMessageRequest(void)
{
  // count the request
  fNextMessageRequests++;
  if (IS_SERVER) {
    // check if we have seen many requests but could not fulfil them
    if (fNextMessageRequests>3) {
      // check for resume that does not send us an empty Sync (Symbian client at TestFest 16)
      PDEBUGPRINTFX(DBG_ERROR,("Warning: More than 3 consecutive Alert 222 - looks like endless loop, check if we need to work around client implementation issues"));
      // - check datastores
      TLocalDataStorePContainer::iterator pos;
      for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
        // see if it is currently resuming
        TLocalEngineDS *ldsP = (*pos);
        if (ldsP->isResuming() && ldsP->getDSState()<dssta_serverseenclientmods) {
          // fake empty <sync> from client to get things going again
          // - create it
          SmlSyncPtr_t fakeSyncCmdP = (SmlSyncPtr_t)smlLibMalloc(sizeof(SmlSync_t));
          fakeSyncCmdP->elementType = SML_PE_SYNC_START;
          fakeSyncCmdP->cmdID=NULL; // none needed here
          fakeSyncCmdP->flags=0; // none
          fakeSyncCmdP->cred=NULL;
          fakeSyncCmdP->target=newLocation(ldsP->getName()); // client would target myself
          fakeSyncCmdP->source=newLocation(ldsP->getRemoteDBPath()); // client would target myself
          fakeSyncCmdP->meta=NULL; // no meta
          fakeSyncCmdP->noc=NULL; // no NOC
          // - have it processed like it was a real command
          PDEBUGPRINTFX(DBG_HOT+DBG_PROTO,("Probably client expects resume to continue without sending an empty <Sync> -> simulate one"));
          PDEBUGBLOCKFMT((
            "Resume_Sim_Sync","Simulated empty sync to get resume going",
            "datastore=%s",
            ldsP->getName()
          ));
          TStatusCommand *fakeStatusCmdP = new TStatusCommand(this);
          bool queueforlater=false;
          /* bool ok= */
          processSyncStart(
            fakeSyncCmdP,
            *fakeStatusCmdP,
            queueforlater // will be set if command must be queued for later re-execution
          );
          if (!queueforlater) {
            fNextMessageRequests=0; // reset that counter
            // and make sure we advance the sync session state
            // - now the real ugly hacking starts - we have to fake receiving a <final/>
            fFakeFinalFlag=true;
          }
          else {
            PDEBUGPRINTFX(DBG_ERROR,("simulated <Sync> can't be processed now, we'll try again later"));
          }
          // now just simulate a </sync>
          processSyncEnd(queueforlater);
          // now let this particular datastore "know" that sync-from-client is over now
          (*pos)->engEndOfSyncFromRemote(true); // fake "final"
          PDEBUGENDBLOCK("Resume_Sim_Sync");
        }
      }
    }
  } // server
} // TSyncSession::nextMessageRequest




// check if session must continue (for session-level reasons, that
// is without regarding sync state of server or client)
bool TSyncSession::sessionMustContinue(void) {
  // if there are delayed commands not yet executed after this message: session must go on
  if (!fDelayedExecutionCommands.empty()) {
    PDEBUGPRINTFX(DBG_SESSION,(
      "%ld commands in delayed-execution-queue -> session must continue",
      (long)fDelayedExecutionCommands.size()
    ));
    return true; // must continue
  }
  // if no status to wait for: session may be deleted now
  if (fStatusWaitCommands.empty()) return false;
  TSmlCommandPContainer::iterator pos;
  // show them
  #ifdef SYDEBUG
  for (pos=fStatusWaitCommands.begin(); pos!=fStatusWaitCommands.end(); ++pos) {
    TSmlCommand *cmdP = *pos;
    // show that command was not answered
    PDEBUGPRINTFX(DBG_PROTO,("- Not yet received %sstatus for command '%s', (outgoing MsgID=%ld, CmdID=%ld)",
      cmdP->statusEssential() ? "REQUIRED " : "",
      cmdP->getName(),
      (long)cmdP->getMsgID(),
      (long)cmdP->getCmdID()
    ));
  }
  #endif
  // check type of commands we miss status for
  bool mustgoon=false;
  for (pos=fStatusWaitCommands.begin(); pos!=fStatusWaitCommands.end(); ++pos) {
    TSmlCommand *cmdP = *pos;
    if (cmdP->statusEssential()) {
      // we need a status for at least one of these
      mustgoon=true;
      break;
    }
  }
  // if only one single status to wait for, check if it is
  // SyncHdr status; if no, session MUST continue
  // (otherwise, remote will not send status for SyncHdr alone)
  if (mustgoon) {
    PDEBUGPRINTFX(DBG_HOT,("SESSION CANNOT END - Not yet received REQUIRED status for some of %ld commands",(long)fStatusWaitCommands.size()));
  }
  return mustgoon;
} // TSyncSession::sessionMustContinue


// returns true if session has pending commands
bool TSyncSession::hasPendingCommands(void)
{
  return (!(
    fNextMessageCommands.size()==0 &&       // ..no commands to send in next message AND
    fDelayedExecutionCommands.size()==0 &&  // ..no commands to process in next message AND
    fInterruptedCommandP==NULL              // ..no interrupted outgoing commands
  ));
} // TSyncSession::hasPendingCommands


// finish outgoing Message, returns true if final message of package
bool TSyncSession::FinishMessage(bool aAllowFinal, bool aForceNonFinal)
{
  Ret_t err;
  bool final=true;

  // finish message if any
  if (fOutgoingStarted) {
    // there is an unfinished message, finish it
    // - final only if no commands waiting for next message
    //   and caller allows final
    final=
      !aForceNonFinal && (
        fAborted ||                             // if aborted, this is a final message, OR..
        (aAllowFinal && !hasPendingCommands())  // ..(if allowed final AND no pending commands)
      ); // ...THEN this is a final package
    PDEBUGPRINTFX(DBG_PROTO,(
      "Ending message with %s%ld next-message/%ld next-package commands: %sFINAL (%sfinal %sallowed by caller)",
      fInterruptedCommandP ? "interrupted command and " : "",
      (long)fNextMessageCommands.size(),
      (long)fNextPackageCommands.size(),
      final ? "" : "NOT ",
      fAborted ? "ABORTED, " : "",
      aAllowFinal ? "" : "not "
    ));
    // - close message now
    sInt32 bytesbeforeissue=getSmlWorkspaceFreeBytes();
    fOutgoingStarted=false; // done now
    fOutgoingMessageFull=false; // message finished, not full any more
    #ifdef SYDEBUG
    if (fXMLtranslate && fOutgoingXMLInstance)
      smlEndMessage(fOutgoingXMLInstance,final);
    // Now dump XML translation of outgoing message
    XMLTranslationOutgoingEnd();
    #endif
    if ((err=smlEndMessage(fSmlWorkspaceID, final))!=SML_ERR_OK) {
      SYSYNC_THROW(TSmlException("smlEndMessage",err));
    }
    incOutgoingMessageSize(bytesbeforeissue-getSmlWorkspaceFreeBytes());
    PDEBUGPRINTFX(DBG_PROTO,("Entire message size is now %ld Bytes",(long)getOutgoingMessageSize()));
    // if outgoing message was not final, prevent session end
    // except if session is aborted (and final flag is forced nonFinal e.g. when ending session because of serverBusy()
    if (!final && !fAborted) fInProgress=true;
    CONSOLEPRINTF(("< SyncML message #%ld sent to '%s'",(long)fOutgoingMsgID,fRespondURI.empty() ? fRemoteURI.c_str() : fRespondURI.c_str()));
    MP_SHOWCURRENT(DBG_PROFILE,"End of outgoing message");
    // dump it if configured
    #ifdef SYDEBUG
    DumpSyncMLMessage(true); // outgoing
    #endif
  }
  // if this message ends with <Final/>, next message will be in new package
  fNewOutgoingPackage=final;
  return final;
} // TSyncSession::FinishMessage


// get name of current encoding
const char *TSyncSession::getEncodingName(void)
{
  return SyncMLEncodingMIMENames[fEncoding];
} // TSyncSession::getEncodingName


// add current encoding spec to given (type-)string
void TSyncSession::addEncoding(string &aString)
{
  aString+=SYNCML_ENCODING_SEPARATOR;
  aString+=getEncodingName();
} // TSyncSession::addEncoding


// set encoding for session
void TSyncSession::setEncoding(SmlEncoding_t aEncoding)
{
  Ret_t err=smlSetEncoding(fSmlWorkspaceID,aEncoding);
  if (err==SML_ERR_OK) {
    fEncoding = aEncoding;
  }
} // TSyncSession::setEncoding


// find remote datastore by (remote party specified) URI
TRemoteDataStore *TSyncSession::findRemoteDataStore(const char *aDatastoreURI)
{
  TRemoteDataStorePContainer::iterator pos;
  TRemoteDataStore *bestMatchP=NULL;
  uInt16 bestNumMatched=0;
  // search for BEST match (most number of chars matched)
  for (pos=fRemoteDataStores.begin(); pos!=fRemoteDataStores.end(); ++pos) {
    // test for match
    if ((*pos)->isDatastore(aDatastoreURI) > bestNumMatched) {
      bestMatchP = *pos; // best so far, but check all
    }
  }
  return bestMatchP; // return NULL if no match found or best matching
} // TSyncSession::findRemoteDataStore


// - find local datastore by URI and separate identifying from optional part of URI
TLocalEngineDS *TSyncSession::findLocalDataStoreByURI(const char *aURI,string *aOptions, string *aIdentifyingURI)
{
  string dburi;

  // - get relative URI of requested database
  const char *dblocuri = SessionRelativeURI(aURI);
  // In this base class implementation, identification is path, options are CGI
  // - separate target address and CGI (if any)
  const char *optionsCGI=(const char *)strchr(dblocuri,'?');
  if (optionsCGI) {
    dburi.assign(dblocuri,optionsCGI-dblocuri);
    optionsCGI++; // skip '?'
    dblocuri=dburi.c_str();
    PDEBUGPRINTFX(DBG_PROTO,("Target Address CGI Options: %s",optionsCGI));
    if (aOptions) aOptions->assign(optionsCGI);
  }
  else {
    // no CGI contained
    if (aOptions) aOptions->erase();
  }
  // assign identifying part of URL now
  if (aIdentifyingURI) {
    aIdentifyingURI->assign(dblocuri);
  }
  // find datastore now
  DEBUGPRINTF(("Determined relative, identifying URI (w/o CGI): %s",dblocuri));
  return findLocalDataStore(dblocuri);
} // TSyncSession::findLocalDataStorebyURI


// find local datastore by (relative) URI
TLocalEngineDS *TSyncSession::findLocalDataStore(const char *aDatastoreURI)
{
  TLocalDataStorePContainer::iterator pos;
  for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
    // test for match (we do not do best-match search here because these are names
    // under our own control that do not contain slashes and hence no
    // mismatch possibilities like "/Calendar" and "/Calendar/Events" as
    // with Oracle server.
    if ((*pos)->isDatastore(aDatastoreURI))
      return (*pos); // found
  }
  return NULL; // none found
} // TSyncSession::findLocalDataStore


// - find local datastore by datastore handle (=config pointer)
TLocalEngineDS *TSyncSession::findLocalDataStore(void *aDSHandle)
{
  TLocalDataStorePContainer::iterator pos;
  for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
    // test for match
    TLocalEngineDS *ldsP = (*pos);
    if ((void *)(ldsP->getDSConfig())==aDSHandle)
      return (ldsP); // found
  }
  return NULL; // none found
} // TSyncSession::findLocalDataStore


TLocalEngineDS *TSyncSession::addLocalDataStore(TLocalDSConfig *aLocalDSConfigP)
{
  TLocalEngineDS *ldsP=aLocalDSConfigP->newLocalDataStore(this);
  fLocalDataStores.push_back(ldsP);
  return ldsP;
} // TSyncSession::addLocalDataStore



// - find local datatype by config pointer (used to avoid duplicating types
//   in session if used by more than a single datastore)
TSyncItemType *TSyncSession::findLocalType(TDataTypeConfig *aDataTypeConfigP)
{
  TSyncItemTypePContainer::iterator pos;
  for (pos=fLocalItemTypes.begin(); pos!=fLocalItemTypes.end(); ++pos) {
    // test for match
    if ((void *)((*pos)->getTypeConfig())==aDataTypeConfigP)
      return (*pos); // found
  }
  return NULL; // none found
} // TSyncSession::findLocalType


// - find implemented remote datatype by config pointer (and related datastore, if any)
TSyncItemType *TSyncSession::findRemoteType(TDataTypeConfig *aDataTypeConfigP, TSyncDataStore *aRelatedRemoteDS)
{
  TSyncItemTypePContainer::iterator pos;
  for (pos=fRemoteItemTypes.begin(); pos!=fRemoteItemTypes.end(); ++pos) {
    // test for match
    if ((void *)((*pos)->getTypeConfig())==aDataTypeConfigP) {
      // match only if related to same datastore or not related
      if (aRelatedRemoteDS == (*pos)->getRelatedDatastore())
        return (*pos); // found
    }
  }
  return NULL; // none found
} // TSyncSession::findRemoteType





// get new list of all local datastores
SmlDevInfDatastoreListPtr_t TSyncSession::newDevInfDataStoreList(bool aAlertedOnly, bool aWithoutCTCapProps)
{
  SmlDevInfDatastoreListPtr_t rootP,*insertpos;
  SmlDevInfDatastorePtr_t datastoreP;

  // no list at beginning
  rootP=NULL;
  insertpos = &rootP;

  // go through local datastore list
  TLocalDataStorePContainer::iterator pos1;
  for (pos1=fLocalDataStores.begin(); pos1!=fLocalDataStores.end(); ++pos1) {
    // check if we only want alerted datastore's info (client case)
    if (aAlertedOnly) {
      if (!(*pos1)->testState(dssta_clientsentalert))
        continue; // not alerted, do not show this one
    }
    // see if we have info at all
    datastoreP = (*pos1)->getDatastoreDevinf(IS_SERVER, aWithoutCTCapProps);
    if (datastoreP) {
      // create new list item
      (*insertpos) = SML_NEW(SmlDevInfDatastoreList_t);
      (*insertpos)->next = NULL;
      (*insertpos)->data = datastoreP;
      // set new insert position
      insertpos = &((*insertpos)->next);
    }
  }
  return rootP;
} // TSyncSession::newDevInfDataStoreList


// get common sync capabilities mask of this session (datastores might modify it)
uInt32 TSyncSession::getSyncCapMask(void)
{
  return
    SCAP_MASK_NORMAL |
    (getSessionConfig()->fAcceptServerAlerted ? SCAP_MASK_SERVER_ALERTED : 0);
} // TSyncSession::getSyncCapMask


// get new list of all local item types
SmlDevInfCtcapListPtr_t TSyncSession::newLocalCTCapList(bool aAlertedOnly, TLocalEngineDS *aOnlyForDS, bool aWithoutCTCapProps)
{
  SmlDevInfCtcapListPtr_t rootP,*insertpos;
  SmlDevInfCTCapPtr_t ctcapP;

  // no list at beginning
  rootP=NULL;
  insertpos = &rootP;
  bool showTy;
  TTypeVariantDescriptor variantDesc = NULL;

  // go through local datastore list
  // Note: rulematch types should normally not be shown, but the way to do that
  //       is not some testing here (which is almost impossible), but profiles
  //       defined for rulematch types should be made invisible.
  TSyncItemTypePContainer::iterator pos2;
  TLocalDataStorePContainer::iterator pos1;
  for (pos2=fLocalItemTypes.begin(); pos2!=fLocalItemTypes.end(); ++pos2) {
    showTy=true;
    // check for restriction to certain datastore
    if (aOnlyForDS) {
      // only show if specified datastore is using the type
      showTy = aOnlyForDS->doesUseType(*pos2, &variantDesc);
    }
    // see if datatype is used by any of the alerted datastores
    if (showTy && aAlertedOnly) {
      showTy=false;
      for (pos1=fLocalDataStores.begin(); pos1!=fLocalDataStores.end(); ++pos1) {
        // check if we only want alerted datastore's info (client case)
        if (!(*pos1)->testState(dssta_clientsentalert)) continue; // test next
        // see if datatype is used by this datastore
        if ((*pos1)->doesUseType(*pos2)) {
          showTy=true;
          break;
        }
      }
    }
    // now show if selected
    if (showTy) {
      // see if we have info at all
      ctcapP = (*pos2)->getCTCapDevInf(aOnlyForDS, variantDesc, aWithoutCTCapProps);
      if (ctcapP) {
        // create new list item
        (*insertpos) = SML_NEW(SmlDevInfCtcapList_t);
        (*insertpos)->next = NULL;
        (*insertpos)->data = ctcapP;
        // set new insert position
        insertpos = &((*insertpos)->next);
      }
    }
  }
  return rootP;
} // TSyncSession::newLocalCTCapList


// build DevInf of this session
SmlDevInfDevInfPtr_t TSyncSession::newDevInf(bool aAlertedOnly, bool aWithoutCTCapProps)
{
  SmlDevInfDevInfPtr_t devinfP;

  // Create empty DevInf
  devinfP = SML_NEW(SmlDevInfDevInf_t);
  // Fill in information for current session
  devinfP->verdtd=newPCDataString(SyncMLVerDTDNames[fSyncMLVersion]);
  // - identification of this SyncML implementation
  devinfP->man=newPCDataOptString(getSyncAppBase()->getManufacturer().c_str());
  devinfP->mod=newPCDataOptString(getSyncAppBase()->getModel().c_str());
  devinfP->oem=newPCDataOptString(getSyncAppBase()->getOEM());
  devinfP->swv=newPCDataOptString(getSyncAppBase()->getSoftwareVersion());
  // - identification of the device ID and type (server/client etc.)
  devinfP->devid=newPCDataString(getDeviceID().c_str());
  devinfP->devtyp=newPCDataString(getDeviceType().c_str());
  // - identification of the platform the software runs on
  devinfP->hwv=newPCDataOptString(getSyncAppBase()->getHardwareVersion().c_str());
  devinfP->fwv=newPCDataOptString(getSyncAppBase()->getFirmwareVersion().c_str());
  // Now get info for content capabilities
  if (fSyncMLVersion<syncml_vers_1_2) {
    // CTCap is global, get it without limitation to a datastore
    devinfP->ctcap=newLocalCTCapList(aAlertedOnly, NULL, aWithoutCTCapProps);
  }
  else
    devinfP->ctcap=NULL; // no global CTCap any more at devInf level
  // Now get info for datastores
  devinfP->datastore=newDevInfDataStoreList(aAlertedOnly, aWithoutCTCapProps);
  // SyncML 1.1 related flags
  devinfP->flags=0; // no SyncML 1.1 flags by default
  if (fSyncMLVersion>=syncml_vers_1_1) {
    // - we can always parse number of changes (whether we can make use of it is irrelevant)
    devinfP->flags |= SmlDevInfNOfM_f;
    // - check if we support UTC based time (implementations with no means to obtain time zone might not)
    if (canHandleUTC())
      devinfP->flags |= SmlDevInfUTC_f; // we can handle UTC
    // - we support large object
    devinfP->flags |= SmlDevInfLargeObject_f;
  }
  // Now get extensions info
  devinfP->ext=NULL;
  //  %%% tdb, optional
  // return
  return devinfP;
} // TSyncSession::newDevInf


// get devInf for this session (caller is passed ownership)
SmlItemPtr_t TSyncSession::getLocalDevInfItem(bool aAlertedOnly, bool aWithoutCTCapProps)
{
  // - create item with correct source and Meta information
  SmlItemPtr_t devinf = newItem();
  // %%% if strictly following example in SyncML protocol specs,
  //     source should not have a Displayname
  //fLocalDevInfItemP->source=newLocation(SYNCML_DEVINF_LOCURI,SYNCML_DEVINF_LOCNAME);
  // %%% if strictly following example in SyncML protocol specs,
  //     source should not have "./" prefix
  // %%% this is disputable, DCM expects ./, so we send it again now
  devinf->source=newLocation(SyncMLDevInfNames[fSyncMLVersion]);
  // - create meta type
  /* %%% if strictly following example in SyncML protocol specs, meta
   *     must be defined in the result/put command, not the individual item
  string metatype=SYNCML_DEVINF_META_TYPE;
  addEncoding(metatype);
  fLocalDevInfItemP->meta=newMetaType(metatype.c_str());
  %%% */
  // - create DevInf PCData
  devinf->data = SML_NEW(SmlPcdata_t);
  devinf->data->contentType=SML_PCDATA_EXTENSION;
  devinf->data->extension=SML_EXT_DEVINF;
  // - %%% assume length is not relevant for structured content (looks like in mgrutil.c)
  devinf->data->length=0;
  // - create and insert DevInf
  devinf->data->content = newDevInf(aAlertedOnly, aWithoutCTCapProps);
  // - done
  return devinf;
} // TSyncSession::getLocalDevInfItem


// analyze remote devinf delivered by Put or Get/Result commands
// or loaded from cache by loadRemoteDevInf()
localstatus TSyncSession::analyzeRemoteDevInf(
  SmlDevInfDevInfPtr_t aDevInfP
)
{
  localstatus sta = LOCERR_OK;
  PDEBUGBLOCKDESC("DevInf_Analyze","Analyzing remote devInf");
  if (!aDevInfP) {
    PDEBUGPRINTFX(DBG_ERROR,("Warning: No DevInf found (possible cause: improperly encoded devInf from remote)"));
    sta=400; // no devInf
    goto done;
  }
  else {
    // we have seen the devinf now
    fRemoteDevInfKnown=true;
    // analyze what device we have here
    PDEBUGPRINTFX(DBG_REMOTEINFO+DBG_HOT,(
      "Device ID='%" FMT_LENGTH(".50") "s', Type='%" FMT_LENGTH(".20") "s', Model='%" FMT_LENGTH(".50") "s'",
      FMT_LENGTH_LIMITED(50,smlPCDataToCharP(aDevInfP->devid)),
      FMT_LENGTH_LIMITED(20,smlPCDataToCharP(aDevInfP->devtyp)),
      FMT_LENGTH_LIMITED(50,smlPCDataToCharP(aDevInfP->mod))
    ));
    PDEBUGPRINTFX(DBG_REMOTEINFO+DBG_HOT,(
      "Manufacturer='%" FMT_LENGTH(".30") "s', OEM='%" FMT_LENGTH(".30") "s'",
      FMT_LENGTH_LIMITED(30,smlPCDataToCharP(aDevInfP->man)),
      FMT_LENGTH_LIMITED(30,smlPCDataToCharP(aDevInfP->oem))
    ));
    PDEBUGPRINTFX(DBG_REMOTEINFO+DBG_HOT,(
      "Softwarevers='%" FMT_LENGTH(".30") "s', Firmwarevers='%" FMT_LENGTH(".30") "s', Hardwarevers='%" FMT_LENGTH(".30") "s'",
      FMT_LENGTH_LIMITED(30,smlPCDataToCharP(aDevInfP->swv)),
      FMT_LENGTH_LIMITED(30,smlPCDataToCharP(aDevInfP->fwv)),
      FMT_LENGTH_LIMITED(30,smlPCDataToCharP(aDevInfP->hwv))
    ));
    #ifndef MINIMAL_CODE
    // get the devinf details 1:1
    fRemoteDevInf_devid=smlPCDataToCharP(aDevInfP->devid);
    fRemoteDevInf_devtyp=smlPCDataToCharP(aDevInfP->devtyp);
    fRemoteDevInf_mod=smlPCDataToCharP(aDevInfP->mod);
    fRemoteDevInf_man=smlPCDataToCharP(aDevInfP->man);
    fRemoteDevInf_oem=smlPCDataToCharP(aDevInfP->oem);
    fRemoteDevInf_swv=smlPCDataToCharP(aDevInfP->swv);
    fRemoteDevInf_fwv=smlPCDataToCharP(aDevInfP->fwv);
    fRemoteDevInf_hwv=smlPCDataToCharP(aDevInfP->hwv);
    // get the descriptive name of the device
    fRemoteDescName.assign(smlPCDataToCharP(aDevInfP->man));
    if (fRemoteDescName.size()>0) fRemoteDescName+=" ";
    fRemoteDescName.append(smlPCDataToCharP(aDevInfP->mod));
    // get extra info: "Type (HWV, FWV, SWV) Oem"
    fRemoteInfoString+=fRemoteDevInf_devtyp;
    fRemoteInfoString+=" (";
    fRemoteInfoString+=fRemoteDevInf_hwv;
    fRemoteInfoString+=", ";
    fRemoteInfoString+=fRemoteDevInf_fwv;
    fRemoteInfoString+=", ";
    fRemoteInfoString+=fRemoteDevInf_swv;
    fRemoteInfoString+=") ";
    fRemoteInfoString+=fRemoteDevInf_oem;
    #endif
    // Show SyncML version here (again)
    PDEBUGPRINTFX(DBG_REMOTEINFO+DBG_HOT,("SyncML Version: %s",SyncMLVerProtoNames[fSyncMLVersion]));
    // check SyncML 1.1 flags
    if (fSyncMLVersion>=syncml_vers_1_1) {
      // - check if remote can receive NOC (number of changes)
      fRemoteWantsNOC = (aDevInfP->flags & SmlDevInfNOfM_f);
      // - check if remote can handle UTC time
      fRemoteCanHandleUTC = (aDevInfP->flags & SmlDevInfUTC_f);
      // - check if remote supports large objects
      fRemoteSupportsLargeObjects = (aDevInfP->flags & SmlDevInfLargeObject_f);
      PDEBUGPRINTFX(DBG_REMOTEINFO+DBG_HOT,(
        "SyncML capability flags: wantsNOC=%s, canHandleUTC=%s, supportsLargeObjs=%s",
        aDevInfP->flags & SmlDevInfNOfM_f ? "Yes" : "No",
        aDevInfP->flags & SmlDevInfUTC_f ? "Yes" : "No",
        aDevInfP->flags & SmlDevInfLargeObject_f ? "Yes" : "No"
      ));
    }
    // detect remote specific server behaviour if needed
    SmlDevInfDevInfPtr_t CTCapDevInfP = aDevInfP;
    sta = checkRemoteSpecifics(aDevInfP, &CTCapDevInfP);
    if (sta!=LOCERR_OK) {
      remoteAnalyzed(); // analyzed to reject
      goto done;
    }
    // switch to DevInf provided by remote rules
    if (CTCapDevInfP != aDevInfP) {
      PDEBUGPRINTFX(DBG_REMOTEINFO,("using CTCaps provided in DevInf of remote rule"));
      aDevInfP = CTCapDevInfP;
    }
    // Types and datastores may not be changed/added if sync has allready started
    if (fRemoteDevInfLock) {
      // Sync already started, in "blind" mode or previously received devInf,
      // do not confuse things with changing devInf in mid-sync
      PDEBUGPRINTFX(DBG_ERROR,(
        "WARNING: Type and Datastore info in DevInf ignored because it came to late"
      ));
    }
    else {
      if (getSyncMLVersion()<syncml_vers_1_2) {
        // analyze CTCaps (content type capabilities)
        SmlDevInfCtcapListPtr_t ctlP = aDevInfP->ctcap;
        // loop through list
        PDEBUGBLOCKDESC("RemoteTypes", "Analyzing remote types listed in devInf level CTCap");
        if (fIgnoreCTCap) {
          // ignore CTCap
          if (ctlP) {
            PDEBUGPRINTFX(DBG_REMOTEINFO+DBG_HOT,("Remote rule prevents looking at CTCap"));
          }
        }
        else {
          while (ctlP) {
            if (ctlP->data) {
              // create appropriate remote data itemtypes
              if (TSyncItemType::analyzeCTCapAndCreateItemTypes(
                this,
                NULL, // this is the pre-DS1.2 style where CTCap is on devInf level
                ctlP->data, // CTCap
                fLocalItemTypes, // look up in local types for specialized classes
                fRemoteItemTypes // add new item types here
              )) {
                // we have CTCap info of at least one remote type
                fRemoteDataTypesKnown=true;
              }
              else {
                PDEBUGPRINTFX(DBG_ERROR,("CTCap could not be used (missing version)"));
                sta=500;
              }
            }
            // - go to next item
            ctlP=ctlP->next;
          } // while
        }
        PDEBUGENDBLOCK("RemoteTypes");
      } // if <DS1.2
      // now get datastores
      PDEBUGBLOCKDESC("RemoteDatastores", "Analyzing remote datastores");
      SmlDevInfDatastoreListPtr_t dslP = aDevInfP->datastore;
      while(dslP) {
        if (dslP->data) {
          // we have DataStore info of remote datastores
          fRemoteDataStoresKnown=true;
          // there is a DataStore entry, create RemoteDataStore for it
          TRemoteDataStore *datastoreP;
          MP_NEW(datastoreP,DBG_OBJINST,"TRemoteDataStore",TRemoteDataStore(this));
          PDEBUGBLOCKDESC("RemoteDSDevInf", "Registering remote Datastore from devInf");
          // let new datastore analyze the devinf data
          if (!datastoreP->setDatastoreDevInf(
            dslP->data,
            fLocalItemTypes,  // look up for datatypes here first
            fRemoteItemTypes  // but add types here
          )) {
            // invalid CTCap
            PDEBUGPRINTFX(DBG_ERROR,("Invalid DataStore devInf"));
            delete datastoreP; // forget invalid data store
            sta=500; // failed
          }
          else {
            // CTCap set successfully, new type created
            // - save it in the list of remote types
            fRemoteDataStores.push_back(datastoreP);
          }
          PDEBUGENDBLOCK("RemoteDSDevInf");
        }
        // - go to next item
        dslP=dslP->next;
      } // while
      PDEBUGENDBLOCK("RemoteDatastores");
    } // else sync not started yet
  }
  // give descendants possibility to do something with the analyzed data
  remoteAnalyzed();
  // ok
done:
  PDEBUGENDBLOCK("DevInf_Analyze");
  return sta;
} // TSyncSession::analyzeRemoteDevInf


#ifdef SYSYNC_SERVER

// Initialize Sync: set up datastores and types for server sync session
localstatus TSyncSession::initSync(
  const char *aLocalDatastoreURI,
  const char *aRemoteDatastoreURI
)
{
  localstatus sta = LOCERR_OK;

  // search for local datastore first
  string cgiOptions;
  // - search for datastore and obtain possible CGI;
  //   fallback to remote datastore URI is for Sony Ericsson C510,
  //   which sends an empty target (= local) URI (also needs
  //   to be done in Alert handling)
  fLocalSyncDatastoreP = findLocalDataStoreByURI(SessionRelativeURI((!aLocalDatastoreURI ||
                                                                     !aLocalDatastoreURI[0]) ?
                                                                    aRemoteDatastoreURI :
                                                                    aLocalDatastoreURI),&cgiOptions);
  if (!fLocalSyncDatastoreP) {
    // no such local datastore
    return 404;
  }
  // Local datastore is known here (fLocalSyncDatastoreP)
  // - now init for reception of syncops
  sta = fLocalSyncDatastoreP->engInitForSyncOps(aRemoteDatastoreURI);
  #ifdef SYNCML_TAF_SUPPORT
  if (sta==LOCERR_OK) {
    // - make sure that options are reparsed (TAF *might* change from Sync request to Sync request)
    sta = fLocalSyncDatastoreP->engParseOptions(
      cgiOptions.c_str(),
      true // we are parsing options from <sync> target URI
    );
  }
  #endif
  #ifdef OBJECT_FILTERING
  if (sta==LOCERR_OK) {
    // %%% check for DS 1.2 <filter> in <Sync> command as well (we do parse <filter> in <Alert> already)
    #if (!defined _MSC_VER || defined WINCE) && !defined(__GNUC__)
    #warning "tbd %%%: check for DS 1.2 <filter> in <Sync> command as well (we do parse <filter> in <Alert> already)"
    #endif
  }
  #endif
  #ifdef OBJECT_FILTERING
  // Show filter summary
  #ifdef SYDEBUG
  #ifdef SYNCML_TAF_SUPPORT
  PDEBUGPRINTFX(DBG_FILTER,("TAF   (temporary, INCLUSIVE) Filter : %s",fLocalSyncDatastoreP->fTargetAddressFilter.c_str()));
  #endif // SYNCML_TAF_SUPPORT
  PDEBUGPRINTFX(DBG_FILTER,("SyncSet (dynamic, EXCLUSIVE) Filter : %s",fLocalSyncDatastoreP->fSyncSetFilter.c_str()));
  #ifdef SYSYNC_TARGET_OPTIONS
  string ts;
  StringObjTimestamp(ts,fLocalSyncDatastoreP->fDateRangeStart);
  PDEBUGPRINTFX(DBG_FILTER,("Date Range Start                    : %s",fLocalSyncDatastoreP->fDateRangeStart ? ts.c_str() : "<none>"));
  StringObjTimestamp(ts,fLocalSyncDatastoreP->fDateRangeEnd);
  PDEBUGPRINTFX(DBG_FILTER,("Date Range End                      : %s",fLocalSyncDatastoreP->fDateRangeEnd ? ts.c_str() : "<none>"));
  #endif // SYSYNC_TARGET_OPTIONS
  #endif // SYDEBUG
  #endif // OBJECT_FILTERING
  // return status
  return sta;
} // TSyncSession::initSync

#endif // SYSYNC_SERVER



// end sync group (of client sync commands)
bool TSyncSession::processSyncEnd(bool &aQueueForLater)
{
  bool ok=true;

  // inform local
  if (fLocalSyncDatastoreP) {
    // let datastore process it
    ok=fLocalSyncDatastoreP->engProcessSyncCmdEnd(aQueueForLater);
  }
  // end Sync bracket
  #ifdef SYNCSTATUS_AT_SYNC_CLOSE
  // %%% status for sync command sent AFTER statuses for contained commands
  if (fSyncCloseStatusCommandP)
    issueRoot(fSyncCloseStatusCommandP);
  #endif
  // no local datastore active
  fLocalSyncDatastoreP=NULL;
  return ok;
} // TSyncSession::processSyncEnd



// process generic sync command item within Sync group
// - returns true (and unmodified or non-200-successful status) if
//   operation could be processed regularily
// - returns false (but probably still successful status) if
//   operation was processed with internal irregularities, such as
//   trying to delete non-existant item in datastore with
//   incomplete Rollbacks (which returns status 200 in this case!).
bool TSyncSession::processSyncOpItem(
  TSyncOperation aSyncOp,        // the operation
  SmlItemPtr_t aItemP,           // the item to be processed
  SmlMetInfMetInfPtr_t aMetaP,   // command-wide meta, if any
  TLocalEngineDS *aLocalSyncDatastore, // the local datastore for this syncop item
  TStatusCommand &aStatusCommand, // pre-set 200 status, can be modified in case of errors
  bool &aQueueForLater // must be set if item cannot be processed now, but must be processed later
)
{
  // assign datastore context (%%% note: some day we will get rid of this
  // "global" pointer to the active datastore by moving it into the <sync> command
  // object and installing a hierarchical command processor.)
  fLocalSyncDatastoreP=aLocalSyncDatastore;
  // Server mode: commands affect datastores currently in sync
  if (!fLocalSyncDatastoreP) {
    // sync generic command outside sync bracket -> error
    aStatusCommand.setStatusCode(403); // forbidden
    ADDDEBUGITEM(aStatusCommand,"Add/Copy/Replace/Delete unrelated to datastores");
    PDEBUGPRINTFX(DBG_ERROR,("Add/Copy/Replace/Delete unrelated to datastores"));
    // no success
    return false;
  }
  // check for aborted datastore
  if (fLocalSyncDatastoreP->CheckAborted(aStatusCommand)) return false;
  // check if we can process it now
  // Note: request time limit is active in server only.
  if (!fLocalSyncDatastoreP->engIsStarted(false) || RemainingRequestTime()<0) {
    aQueueForLater=true; // re-execute later...
    return true; // ...but otherwise ok
  }
  // process Sync operation sent by remote
  // - show
  PDEBUGPRINTFX(DBG_DATA,(
    "Remote sent %s-operation:",
    SyncOpNames[aSyncOp]
  ));
  PDEBUGPRINTFX(DBG_DATA,(
    "- Source: remoteID ='%s', remoteName='%s'",
    smlSrcTargLocURIToCharP(aItemP->source),
    smlSrcTargLocNameToCharP(aItemP->source)
  ));
  PDEBUGPRINTFX(DBG_DATA,(
    "- Target: localID  ='%s', remoteName='%s'",
    smlSrcTargLocURIToCharP(aItemP->target),
    smlSrcTargLocNameToCharP(aItemP->target)
  ));
  // now let datastore handle it
  bool regular = fLocalSyncDatastoreP->engProcessSyncOpItem(aSyncOp, aItemP, aMetaP, aStatusCommand);
  #ifdef SCRIPT_SUPPORT
  // let script check status code
  TErrorFuncContext errctx;
  errctx.statuscode = aStatusCommand.getStatusCode();
  errctx.newstatuscode = errctx.statuscode;
  errctx.syncop = aSyncOp;
  errctx.datastoreP = fLocalSyncDatastoreP;
  // call script
  regular =
    TScriptContext::executeTest(
      regular, // pass through regular status
      fSessionScriptContextP,
      getSessionConfig()->fReceivedItemStatusScript,
      &ErrorFuncTable,
      &errctx // caller context
    );
  // not completely handled, use possibly modified status code
  #ifdef SYDEBUG
  if (aStatusCommand.getStatusCode() != errctx.newstatuscode) {
    PDEBUGPRINTFX(DBG_ERROR,("Status: Session Script changed original status=%hd to %hd (original op was %s)",aStatusCommand.getStatusCode(),errctx.newstatuscode,SyncOpNames[errctx.syncop]));
  }
  #endif
  aStatusCommand.setStatusCode(errctx.newstatuscode);
  #endif
  // check status
  if (!regular) {
    localstatus sta=aStatusCommand.getStatusCode();
    if (sta>=300 && sta!=419) { // conflict resolved with server data is not an error
      PDEBUGPRINTFX(DBG_ERROR,(
        "processSyncOpItem: Error while processing item, status=%hd",
        aStatusCommand.getStatusCode()
      ));
      fLocalSyncDatastoreP->fLocalItemsError++; // count this as an error, as remote will see it as such
    }
    else {
      PDEBUGPRINTFX(DBG_DATA+DBG_HOT,(
        "processSyncOpItem: Irregularity while processing item, status=%hd",
        aStatusCommand.getStatusCode()
      ));
    }
  }
  // done
  return regular;
} // TSyncSession::processSyncOpItem


#endif // not SYNCSESSION_PART2_EXCLUDE
#ifndef SYNCSESSION_PART1_EXCLUDE


// generate challenge for session
SmlChalPtr_t TSyncSession::newSessionChallenge(void)
{
  string nonce;
  getNextNonce(fRemoteURI.c_str(),nonce);
  PDEBUGPRINTFX(DBG_PROTO,(
    "Challenge for next auth: AuthType=%s, Nonce='%s', binary %sallowed",
    authTypeSyncMLNames[requestedAuthType()],
    nonce.c_str(),
    getEncoding()==SML_WBXML ? "" : "NOT "
  ));
  return newChallenge(requestedAuthType(),nonce,getEncoding()==SML_WBXML);
} // TSyncSession::newSessionChallenge


// generate credentials (based on fRemoteNonce, fRemoteRequestedAuth, fRemoteRequestedAuthEnc)
SmlCredPtr_t TSyncSession::newCredentials(const char *aUser, const char *aPassword)
{
  SmlCredPtr_t credP = NULL;
  SmlMetInfMetInfPtr_t metinfP = NULL;
  uInt8 *authdata = NULL;
  void *tobefreed = NULL;
  uInt32 authdatalen=0;
  bool isbinary=false;
  uInt8 digest[16]; // for MD5 digest

  // create auth data
  // - build basic user/pw string
  string userpw;
  userpw.assign(aUser);
  userpw+=':';
  userpw.append(aPassword);
  // - code auth data
  switch (fRemoteRequestedAuth) {
    case auth_basic:
      // Note: this has been clarified in SyncML 1.1: even if B64 is inherent for
      // Basic auth, specifying B64 as format does NOT mean that user:pw is B64-ed twice,
      // but it is just an optional declaration of the inherent B64 format.
      #ifdef BASIC_AUTH_HAS_INHERENT_B64
      // %%% seems to be wrong according to SCTS...
      // Note that the b64 here is PART OF THE BASIC AUTH SCHEME
      // so format MUST NOT specify b64 again!
      fRemoteRequestedAuthEnc=fmt_chr;
      // make b64 of string
      authdata=(uInt8 *)b64::encode((const uInt8 *)userpw.c_str(), userpw.size(), &authdatalen);
      tobefreed=(void *)authdata; // remember to free at end of routine
      #else
      // basic auth is always b64 encoded
      fRemoteRequestedAuthEnc=fmt_b64;
      authdata=(uInt8 *)userpw.c_str();
      #endif
      break;
    case auth_md5:
      // Note that b64 encoding IS NOT part of the MD5 auth scheme.
      // Only if remote specifies b64 format in challenge, b64 encoding is applied
      if (fSyncMLVersion<syncml_vers_1_1) {
        // before 1.1, nonce was MD5-ed together with user/pw
        userpw+=':';
        userpw+=fRemoteNonce; // append nonce string, might contain NULs
      }
      // apply MD5
      md5::SYSYNC_MD5_CTX context;
      md5::Init (&context);
      md5::Update (&context, (unsigned const char *)userpw.c_str(), userpw.size());
      // get result
      md5::Final (digest, &context);
      // more if SyncML 1.1 or later
      if (fSyncMLVersion>=syncml_vers_1_1) {
        // starting with 1.1, nonce is added to b64ed-MD5 and the MD5ed again
        // - B64 it
        authdata=(uInt8*)b64::encode(digest, 16, &authdatalen);
        // - MD5 it while adding nonce
        md5::Init (&context);
        md5::Update (&context, authdata, authdatalen);
        b64::free((void *)authdata); // return buffer allocated by b64::encode
        // - important: add colon as nonce separator
        md5::Update (&context, (uInt8 *) ":", 1);
        // - also add nonce that will be used for checking later
        md5::Update (&context, (uInt8 *) fRemoteNonce.c_str(), fRemoteNonce.size());
        // - this is the MD5 auth value
        //   according to SyncML 1.1,
        //   "changes_for_syncml_represent_v11_20020215.pdf", Section 2.19
        md5::Final (digest, &context);
        // - according to the above mentioned section 2.19, MD5 auth is
        //   always b64 encoded, even in binary transports. This is a
        //   contradiction to discussion in syncml@yahoogroups, particularily
        //   a statement by Peter Thompson who stated that MD5 auth MUST NOT
        //   be b64 encoded in WBXML. Who knows???
        fRemoteRequestedAuthEnc=fmt_b64;
      } // syncml 1.1
      // auth data is 16 byte digest value in binary
      authdata=(uInt8 *)digest;
      authdatalen=16;
      isbinary=true;
      break;
    default : break;
  } // switch
  if (authdata) {
    // create cred
    credP = SML_NEW(SmlCred_t);
    // now add auth data (format if necessary)
    // - force b64 anyway if content is binary but transport isn't
    if (isbinary && getEncoding()==SML_XML) {
      fRemoteRequestedAuthEnc=fmt_b64;
      isbinary=false;
    }
    // - create formatted version of content. Use Opaque for binary content
    credP->data=newPCDataFormatted(authdata,authdatalen,fRemoteRequestedAuthEnc,isbinary);
    // create meta and get pointer
    credP->meta=newMeta();
    metinfP = smlPCDataToMetInfP(credP->meta);
    // add auth type meta
    metinfP->type=newPCDataString(authTypeSyncMLNames[fRemoteRequestedAuth]);
    // Note: aEncType==fmt_chr will not add format tag, as fmt_chr is the default
    metinfP->format=newPCDataFormat(fRemoteRequestedAuthEnc,false); // no format if default of fmt_chr
  }
  // free buffer
  if (tobefreed) b64::free(tobefreed);
  // return cred or NULL if none
  return credP;
} // TSyncSession::newCredentials


// check credentials
// Note: should be called even if there are no credentials, as we
//       need a Session login BEFORE generating status with next Nonce
bool TSyncSession::checkCredentials(const char *aUserName, const SmlCredPtr_t aCredP, TStatusCommand &aStatusCommand)
{
  TAuthTypes authtype=auth_basic; // default to basic
  char *tobefreed = NULL;
  const char *authdata = NULL;
  TFmtTypes authfmt = fmt_chr;
  bool authok = false;

  #ifdef EXPIRES_AFTER_DATE
  // check for hard expiry again
  if (
    (fCopyOfScrambledNow>(SCRAMBLED_EXPIRY_VALUE*4+503))
    #ifdef SYSER_REGISTRATION
    && (!getSyncAppBase()->fRegOK) // only abort if no registration (but accept timed registration)
    #endif
  ) {
    aStatusCommand.setStatusCode(401); // seems to be hacked
    return false;
  }
  #endif
  // Check type of credentials
  if (!aCredP) {
    // Anonymous login attempt
    authtype=auth_none;
  }
  else {
    SmlMetInfMetInfPtr_t metaP;
    if ((metaP=smlPCDataToMetInfP(aCredP->meta))!=NULL) {
      // look for type
      if (metaP->type) {
        // get type (otherwise default to auth-basic)
        const char *ty = smlPCDataToCharP(metaP->type);
        sInt16 t;
        if (StrToEnum(authTypeSyncMLNames,numAuthTypes,t,ty)) {
          authtype=(TAuthTypes)t;
        }
        else {
          // bad auth schema
          authtype=auth_none; // disable checking below
          aStatusCommand.setStatusCode(406); // unsupported optional feature (auth method)
          aStatusCommand.addItemString(ty); // identify bad auth method
        }
      }
      // look for format
      // - get format
      if (!smlPCDataToFormat(metaP->format,authfmt)) {
        authtype=auth_none; // disable checking below
        aStatusCommand.setStatusCode(415); // unsupported format
        aStatusCommand.addItemString(smlPCDataToCharP(metaP->format)); // identify bad format
      }
      // - handle format and get auth data according to auth type
      authdata = smlPCDataToCharP(aCredP->data); // get it as is
      // Now check auth
      switch (authtype) {
        case auth_none:
          // anonymous login attempt
          // - no special measure needed
          break;
        case auth_basic:
          // basic is always b64 encoded
          #ifdef SYDEBUG
          if (authfmt!=fmt_b64)
            PDEBUGPRINTFX(DBG_ERROR,("Auth-basic has no <format>b64 spec --> assumed b64 anyway"));
          #endif
          authfmt=fmt_b64; // basic is ALWAYS b64
          break;
        case auth_md5:
          // verify that we have the username in clear text for MD5 auth
          if (!aUserName || *aUserName==0)
          {
            // username missing, probably strict (bad) SyncML 1.0 conformance,
            // we need SyncML 1.0.1 corrected auth (MD5 w/o username is almost
            // impossible to process)
            authtype=auth_none; // disable checking below
            aStatusCommand.setStatusCode(415); // unsupported format
            ADDDEBUGITEM(aStatusCommand,"Missing clear-text username in Source LocName (SyncML 1.0.1)");
            PDEBUGPRINTFX(DBG_ERROR,("Missing clear-text username in Source LocName (SyncML 1.0.1)"));
            break;
          }
          // MD5 can come as binary (for WBXML)
          if (getEncoding()==SML_WBXML) {
            if (authfmt==fmt_chr || authfmt==fmt_bin) {
              // assume unencoded MD5 digest (16 bytes binary), make b64
              uInt32 l;
              tobefreed=b64::encode((uInt8 *)authdata,16,&l);
              authdata=tobefreed;
              // now authdata is b64 as well
              authfmt=fmt_b64;
            }
          }
          break;
      case numAuthTypes:
          // invalid type?!
          break;
      }
    } // if meta
  }
  #ifndef MINIMAL_CODE
  // save user name for later reference
  if (aUserName) fSyncUserName.assign(aUserName);
  else fSyncUserName.erase();
  #endif
  // check credentials
  if (authtype==auth_none) {
    // check if we can login anonymously
    // NOTE: do it anyway, even if !isAuthTypeAllowed() to make sure
    //       SessionLogin is called
    authok=checkCredentials(aUserName,NULL,auth_none);
    if (!authok || !isAuthTypeAllowed(auth_none)) {
      // anonymous login not possible, request credentials
      aStatusCommand.setStatusCode(407); // unauthorized, missing credentials
      PDEBUGPRINTFX(DBG_PROTO,("Authorization required but none found in SyncHdr, sending status 407 + chal"));
      // - add challenge
      aStatusCommand.setChallenge(newSessionChallenge());
      authok=false;
    }
  }
  else {
    // verify format (must be MD5 by now)
    if (authfmt!=fmt_b64) {
      aStatusCommand.setStatusCode(415); // unsupported format
    }
    else if (!authdata || !*authdata) {
      aStatusCommand.setStatusCode(400); // missing data, malformed request
    }
    else {
      // first check credentials
      // NOTE: This must be done first, to force calling SessionLogin
      //       in all cases
      authok=checkCredentials(aUserName,authdata,authtype);
      // now check result
      if (!isAuthTypeAllowed(authtype)) {
        aStatusCommand.setStatusCode(401); // we need another auth type, tell client which one
        authok=false; // anyway, reject
        PDEBUGPRINTFX(DBG_ERROR,("Authorization failed (wrong type of creds), sending 401 + chal"));
      }
      else if (!authok) {
        // auth type allowed, but auth itself not ok
        aStatusCommand.setStatusCode(401); // unauthorized, bad credentials
        PDEBUGPRINTFX(DBG_ERROR,("Authorization failed (invalid credentials) sending 401 + chal"));
      }
      if (!authok) {
        // - add challenge
        aStatusCommand.setChallenge(newSessionChallenge());
      }
    }
  }
  // free buffer if any
  if (tobefreed) b64::free(tobefreed);
  // make sure we see what config was used in the log
  DebugShowCfgInfo();
  PDEBUGPRINTFX(DBG_HOT,(
    "==== Authorisation %s with SyncML Engine Version %d.%d.%d.%d",
    authok ?  "successful" : "failed",
    SYSYNC_VERSION_MAJOR,
    SYSYNC_VERSION_MINOR,
    SYSYNC_SUBVERSION,
    SYSYNC_BUILDNUMBER
  ));
  if (IS_SERVER) {
    #ifdef SYSYNC_SERVER
    PDEBUGPRINTFX(DBG_HOT,(
      "==== SyncML URL used = '%s', username as sent by remote = '%s'",
      fInitialLocalURI.c_str(),
      fSyncUserName.c_str()
    ));
    #endif
  } // server
  // return result
  return authok;
} // TSyncSession::checkCredentials(SmlCredPtr_t...)


// check credential string
bool TSyncSession::checkCredentials(const char *aUserName, const char *aCred, TAuthTypes aAuthType)
{
  // now check auth
  if (aAuthType==auth_basic) {
    // basic auth allows extracting clear-text password
    string user,password;
    getAuthBasicUserPass(aCred,user,password);
    #ifndef MINIMAL_CODE
    fSyncUserName = user;
    #endif
    if (aUserName && !(user==aUserName)) {
      // username does not match LocName (should, in SyncML 1.0.1 and later)
      PDEBUGPRINTFX(DBG_PROTO,(
        "basic_auth encoded username (%s) does not match LocName username (%s)",
        user.c_str(),
        aUserName ? aUserName : "[NULL Username]"
      ));
    }
    // we have the password in clear text
    return SessionLogin(user.c_str(), password.c_str(), sectyp_clearpass, fRemoteURI.c_str());
  }
  else if (aAuthType==auth_md5) {
    // login user and device to the service
    // - this is normally implemented in derived classes
    return SessionLogin(aUserName, aCred, fSyncMLVersion>=syncml_vers_1_1 ? sectyp_md5_V11 : sectyp_md5_V10, fRemoteURI.c_str());
  }
  else if (aAuthType==auth_none) {
    // even if we have no login, do a "login" with empty credentials
    return SessionLogin("anonymous", NULL, sectyp_anonymous, fRemoteURI.c_str());
  }
  else {
    return false; // unknown auth, is not ok
  }
} // TSyncSession::checkCredentials(const char *...)



// Helper function:
// check plain user / password / nonce combination
// against given auth string.
bool TSyncSession::checkAuthPlain(
  const char *aUserName, const char *aPassWord, const char *aNonce, // given values
  const char *aAuthString, TAuthSecretTypes aAuthStringType // check against this
)
{
  string upw;

  #ifdef SYDEBUG
  PDEBUGPRINTFX(DBG_ADMIN,("Username                      = %s",aUserName));
  DEBUGPRINTFX(DBG_USERDATA+DBG_EXOTIC,("Password                      = %s",aPassWord));
  #endif
  if (aAuthStringType==sectyp_anonymous) {
    return (aPassWord==NULL || *aPassWord==0); // anonymous login ok if no password expected
  }
  else if (aAuthStringType==sectyp_clearpass) {
    return (strcmp(aAuthString,aPassWord)==0); // login ok if password matches
  }
  else {
    // must be MD5
    // - concatenate user:password
    upw = aUserName;
    upw+=':';
    upw.append(aPassWord);
    // depends on method
    if (aAuthStringType==sectyp_md5_V11) {
      // V1.1 requires MD5b64-ing user/pw before adding nonce
      MD5B64(upw.c_str(),upw.size(),upw);
    }
    // now check result
    return checkMD5WithNonce(upw.c_str(),aNonce,aAuthString);
  }
  // unknown auth secret type
  return false;
} // TSyncSession::checkAuthPlain


// Helper function:
// check MD5B64(user:pw) / nonce combination
// against given auth string.
bool TSyncSession::checkAuthMD5(
  const char *aUserName, const char *aMD5B64, const char *aNonce, // given values
  const char *aAuthString, TAuthSecretTypes aAuthStringType // check against this
)
{

  if (aAuthStringType==sectyp_md5_V11) {
    // we have a V11 authstring, check it against our MD5B64(user:pw)
    return checkMD5WithNonce(aMD5B64,aNonce,aAuthString);
  }
  else if (aAuthStringType==sectyp_clearpass) {
    // we must generate the MD5B64(user:pw) from clear text
    string myAuthString = aUserName;
    myAuthString+=':';
    myAuthString+=aAuthString;
    MD5B64(myAuthString.c_str(),myAuthString.size(),myAuthString);
    #ifdef SYDEBUG
    PDEBUGPRINTFX(DBG_ADMIN,("MD5B64(user:pw) stored in local DB     = %s",aMD5B64));
    PDEBUGPRINTFX(DBG_ADMIN,("calculated MD5B64(remoteuser:remotepw) = %s",myAuthString.c_str()));
    #endif
    // then we can directly compare them
    return myAuthString==aMD5B64;
  }
  else
    return false; // we cannot auth V1.0 MD5 against MD5B64(user:password)
} // checkAuthMD5


// Helper function:
// check V1.1 MD5 type auth against known md5userpass
// (B64 encoded MD5 digest of user:password) and nonce
// Note: This works only with V1.1-type credentials!!!!
bool TSyncSession::checkMD5WithNonce(
  const char *aStringBeforeNonce,
  const char *aNonce,
  const char *aMD5B64Creds
)
{
  string pattern; // pattern to match with

  // see if user/pw/nonce matches given MD5
  // - add nonce to prepared string
  //   For V1.0 this is "user:password"
  //   For >=V1.1 this is MD5B64("user:password")
  pattern = aStringBeforeNonce;
  pattern+=':';
  pattern.append(aNonce);
  // - MD5 and B64 entire thing (again)
  MD5B64(pattern.c_str(),pattern.size(),pattern);
  #ifdef SYDEBUG
  DEBUGPRINTFX(DBG_ADMIN ,("String before Nonce         = %s",aStringBeforeNonce));
  DEBUGPRINTFX(DBG_ADMIN ,("Nonce used                  = %s",aNonce));
  PDEBUGPRINTFX(DBG_ADMIN,("Locally calculated MD5B64   = %s",pattern.c_str()));
  PDEBUGPRINTFX(DBG_ADMIN,("Received MD5B64 from remote = %s",aMD5B64Creds));
  #endif
  // - now compare with given credentials
  return strnncmp(aMD5B64Creds,pattern.c_str(),pattern.size())==0;
} // TSyncSession::checkMD5WithNonce


// helper: get user/password out of basic credential string, returns false if bad cred
bool TSyncSession::getAuthBasicUserPass(const char *aBasicCreds, string &aUsername, string &aPassword)
{
  // - convert to user/pw string
  uInt32 userpwlen;
  uInt8 *userpw=b64::decode(aBasicCreds, 0, &userpwlen);
  bool ok=false;
  if (userpw) {
    // adjust length if already null terminated
    if (userpw[userpwlen-1]==0) userpwlen=strlen((char *)userpw);
    // extract plain-text username first
    const char *p=strchr((const char *)userpw,':');
    if (p) {
      // save user name
      aUsername.assign((const char *)userpw,p-(const char *)userpw);
      // save password
      aPassword.assign(p+1);
      ok=true;
    }
  }
  b64::free(userpw);
  return ok;
} // TSyncSession::getAuthBasicUserPass


// check credential string (clear text pw, MD5, etc.)
// This function is normally derived to provide checking of auth string
// Notes:
// - all auth requests are resolved using this function.
// - For pre-SyncML 1.1 MD5 auth, credentials are checkable only
//   against plain text passwords. It's up to the derived class to decide if
//   this is possible or not.
bool TSyncSession::SessionLogin(
  const char *aUserName,
  const char *aAuthString,
  TAuthSecretTypes aAuthStringType,
  const char *aDeviceID
)
{
  string nonce;
  // get config for session
  TSessionConfig *scP = getSessionConfig();
  // anonymous is always ok (because checking if anonymous allowed is done already)
  if (aAuthStringType==sectyp_anonymous) return true; // ok
  // check simple auth
  if (scP->fSimpleAuthUser.empty()) return false; // no simple auth
  // check user name
  if (strucmp(scP->fSimpleAuthUser.c_str(),aUserName)!=0) return false; // wrong user name
  // now check auth string
  if (aAuthStringType==sectyp_md5_V10 || aAuthStringType==sectyp_md5_V11) {
    // we need a nonce
    getAuthNonce(aDeviceID,nonce);
  }
  // now check
  return checkAuthPlain(
    scP->fSimpleAuthUser.c_str(),
    scP->fSimpleAuthPassword.c_str(),
    nonce.c_str(),
    aAuthString,
    aAuthStringType
  );
} // TSyncSession::SessionLogin



// check remote devinf to detect special behaviour needed for some clients (or servers). Base class
// does not do anything on server level (configured rules are handled at session level)
// - NOTE: aDevInfP can be NULL to specify that remote device has not sent any devInf at all
//         and this is a blind sync attempt (so best-guess workaround settings might apply)
localstatus TSyncSession::checkRemoteSpecifics(SmlDevInfDevInfPtr_t aDevInfP, SmlDevInfDevInfPtr_t *aOverrideDevInfP)
{
  #if defined(SYSER_REGISTRATION) || !defined(NO_REMOTE_RULES)
  localstatus sta = LOCERR_OK;
  #endif

  // check hard-coded restrictions
  if (aDevInfP && (
    false
    #ifdef REMOTE_RESTR_DEVID
    || strwildcmp(smlPCDataToCharP(aDevInfP->devid),REMOTE_RESTR_DEVID)!=0
    #endif
    #ifdef REMOTE_RESTR_MAN
    || strwildcmp(smlPCDataToCharP(aDevInfP->man),REMOTE_RESTR_MAN)!=0
    #endif
    #ifdef REMOTE_RESTR_MOD
    || strwildcmp(smlPCDataToCharP(aDevInfP->mod),REMOTE_RESTR_MOD)!=0
    #endif
    #ifdef REMOTE_RESTR_OEM
    || strwildcmp(smlPCDataToCharP(aDevInfP->oem),REMOTE_RESTR_OEM)!=0
    #endif
    #ifdef REMOTE_RESTR_URI
    || strwildcmp(fRemoteURI.c_str(),REMOTE_RESTR_URI)!=0
    #endif
  )) {
    PDEBUGPRINTFX(DBG_ERROR,("Software not allowed syncing with this remote party"));
    AbortSession(403,true);
    return 403;
  }

  // check license restrictions
  #ifdef SYSER_REGISTRATION
  sInt16 daysleft;
  string s;

  // - get restriction string from licensed info
  sta = getSyncAppBase()->getAppEnableInfo(daysleft, NULL, &s);
  string restrid,restrval;
  const char *p = s.c_str(); // start of license info string
  while (sta==LOCERR_OK && (p=getSyncAppBase()->getLicenseRestriction(p,restrid,restrval))!=NULL) {
    const char *restr=NULL;
    if (restrid=="u") { // URL
      // we can check the remote URL without having devinf
      restr=fRemoteURI.c_str();
    }
    else {
      if (restrid.size()==1) {
        // there is a restriction
        if (!aDevInfP) {
          // we cannot check these restrictions without having a devInf
          sta = LOCERR_BADREG;
          PDEBUGPRINTFX(DBG_ERROR,("License restriction needs devInf from remote but none found -> block sync"));
          break;
        }
        // we have devinf, we can check it
        switch (restrid[0]) {
          case 'i' : restr=smlPCDataToCharP(aDevInfP->devid); break;
          case 'm' : restr=smlPCDataToCharP(aDevInfP->man); break;
          case 't' : restr=smlPCDataToCharP(aDevInfP->mod); break;
          case 'o' : restr=smlPCDataToCharP(aDevInfP->oem); break;
        }
      }
    }
    // - now compare with wildcards allowed if we have anything to compare
    if (restr) sta = strwildcmp(restr,restrval.c_str())==0 ? LOCERR_OK : LOCERR_BADREG; // service unavailable
  }
  // - abort if not ok
  if (sta!=LOCERR_OK) {
    PDEBUGPRINTFX(DBG_ERROR,("License does not allow syncing with this remote party, status=%hd",sta));
    AbortSession(403,true,sta);
    return sta;
  }
  #endif // SYSER_REGISTRATION
  // check remote rules
  #ifndef NO_REMOTE_RULES
  PDEBUGBLOCKDESC("RemoteRules","Checking for remote rules");
  // get config for session
  TSessionConfig *scP = getSessionConfig();
  // look if we have matching rule(s) for this device
  TRemoteRulesList::iterator pos;
  for(pos=scP->fRemoteRulesList.begin();pos!=scP->fRemoteRulesList.end();pos++) {
    TRemoteRuleConfig *ruleP = *pos;
    // compare with devinf (or test for default-rule if aDevInfP is NULL
    if (
      !ruleP->fSubRule && // subrules never apply directly
      (ruleP->fManufacturer.empty() || (aDevInfP && strwildcmp(smlPCDataToCharP(aDevInfP->man),ruleP->fManufacturer.c_str())==0)) &&
      (ruleP->fModel.empty() || (aDevInfP && strwildcmp(smlPCDataToCharP(aDevInfP->mod),ruleP->fModel.c_str())==0)) &&
      (ruleP->fOem.empty() || (aDevInfP && strwildcmp(smlPCDataToCharP(aDevInfP->oem),ruleP->fOem.c_str())==0)) &&
      (ruleP->fFirmwareVers.empty() || (aDevInfP && ruleP->fFirmwareVers==smlPCDataToCharP(aDevInfP->fwv))) &&
      (ruleP->fSoftwareVers.empty() || (aDevInfP && ruleP->fSoftwareVers==smlPCDataToCharP(aDevInfP->swv))) &&
      (ruleP->fHardwareVers.empty() || (aDevInfP && ruleP->fHardwareVers==smlPCDataToCharP(aDevInfP->hwv))) &&
      (ruleP->fDevId.empty() || (aDevInfP && ruleP->fDevId==smlPCDataToCharP(aDevInfP->devid))) &&
      (ruleP->fDevTyp.empty() || (aDevInfP && ruleP->fDevTyp==smlPCDataToCharP(aDevInfP->devtyp)))
    ) {
      // found matching rule
      PDEBUGPRINTFX(DBG_HOT,("Found <remoterule> '%s' matching for this peer",ruleP->getName()));
      // remember it
      fActiveRemoteRules.push_back(ruleP);
      // add included subrules
      TRemoteRulesList::iterator spos;
      for(spos=ruleP->fSubRulesList.begin();spos!=ruleP->fSubRulesList.end();spos++) {
        fActiveRemoteRules.push_back(*spos);
        PDEBUGPRINTFX(DBG_HOT,("- rule also activates sub-rule '%s'",(*spos)->getName()));
      }
      // if this rule is final, don't check for further matches
      if (ruleP->fFinalRule) break;
    }
  }
  // process activated rules and subrules
  for(pos=fActiveRemoteRules.begin();pos!=fActiveRemoteRules.end();pos++) {
    // activate this rule
    TRemoteRuleConfig *ruleP = *pos;
    if (ruleP->fOverrideDevInfP && aOverrideDevInfP) {
      // processing in caller will continue with updated DevInf
      *aOverrideDevInfP = ruleP->fOverrideDevInfP;
    }
    // - apply options that have a value
    if (ruleP->fLegacyMode>=0) fLegacyMode = ruleP->fLegacyMode;
    if (ruleP->fLenientMode>=0) fLenientMode = ruleP->fLenientMode;
    if (ruleP->fLimitedFieldLengths>=0) fLimitedRemoteFieldLengths = ruleP->fLimitedFieldLengths;
    if (ruleP->fDontSendEmptyProperties>=0) fDontSendEmptyProperties = ruleP->fDontSendEmptyProperties;
    if (ruleP->fDoQuote8BitContent>=0) fDoQuote8BitContent = ruleP->fDoQuote8BitContent;
    if (ruleP->fDoNotFoldContent>=0) fDoNotFoldContent = ruleP->fDoNotFoldContent;
    if (ruleP->fNoReplaceInSlowsync>=0) fNoReplaceInSlowsync = ruleP->fNoReplaceInSlowsync;
    if (ruleP->fTreatRemoteTimeAsLocal>=0) fTreatRemoteTimeAsLocal = ruleP->fTreatRemoteTimeAsLocal;
    if (ruleP->fTreatRemoteTimeAsUTC>=0) fTreatRemoteTimeAsUTC = ruleP->fTreatRemoteTimeAsUTC;
    if (ruleP->fVCal10EnddatesSameDay>=0) fVCal10EnddatesSameDay = ruleP->fVCal10EnddatesSameDay;
    if (ruleP->fIgnoreDevInfMaxSize>=0) fIgnoreDevInfMaxSize = ruleP->fIgnoreDevInfMaxSize;
    if (ruleP->fIgnoreCTCap>=0) fIgnoreCTCap = ruleP->fIgnoreCTCap;
    if (ruleP->fDSPathInDevInf>=0) fDSPathInDevInf = ruleP->fDSPathInDevInf;
    if (ruleP->fDSCgiInDevInf>=0) fDSCgiInDevInf = ruleP->fDSCgiInDevInf;
    if (ruleP->fUpdateClientDuringSlowsync>=0) fUpdateClientDuringSlowsync = ruleP->fUpdateClientDuringSlowsync;
    if (ruleP->fUpdateServerDuringSlowsync>=0) fUpdateServerDuringSlowsync = ruleP->fUpdateServerDuringSlowsync;
    if (ruleP->fAllowMessageRetries>=0) fAllowMessageRetries = ruleP->fAllowMessageRetries;
    if (ruleP->fStrictExecOrdering>=0) fStrictExecOrdering = ruleP->fStrictExecOrdering;
    if (ruleP->fTreatCopyAsAdd>=0) fTreatCopyAsAdd = ruleP->fTreatCopyAsAdd;
    if (ruleP->fCompleteFromClientOnly>=0) fCompleteFromClientOnly = ruleP->fCompleteFromClientOnly;
    if (ruleP->fRequestMaxTime>=0) fRequestMaxTime = ruleP->fRequestMaxTime;
    if (ruleP->fDefaultOutCharset!=chs_unknown) fDefaultOutCharset = ruleP->fDefaultOutCharset;
    if (ruleP->fDefaultInCharset!=chs_unknown) fDefaultInCharset = ruleP->fDefaultInCharset;
    // - possibly override decisions that are otherwise made by session
    //   Note: this is not a single option because we had this before rule options were tristates.
    if (ruleP->fForceUTC>0) fRemoteCanHandleUTC=true;
    if (ruleP->fForceLocaltime>0) fRemoteCanHandleUTC=false;
    // - descriptive name for the device (for log)
    #ifndef MINIMAL_CODE
    if (!ruleP->fRemoteDescName.empty()) fRemoteDescName = ruleP->fRemoteDescName;
    #endif
    // - test for rejection
    if (ruleP->fRejectStatusCode!=DONT_REJECT) {
      // reject operation with this device
      sta = ruleP->fRejectStatusCode;
      PDEBUGPRINTFX(DBG_ERROR,("remote party rejected by <remoterule> '%s', status=%hd",ruleP->getName(),sta));
      AbortSession(sta,true);
      return sta;
    }
    // - execute rule script
    #ifdef SCRIPT_SUPPORT
    if (!ruleP->fRuleScriptTemplate.empty()) {
      // copy from template
      string ruleScript = ruleP->fRuleScriptTemplate;
      // resolve variable references
      TScriptContext::linkIntoContext(ruleScript,fSessionScriptContextP,this);
      // execute now
      PDEBUGPRINTFX(DBG_HOT,("Executing rulescript for rule '%s'",ruleP->getName()));
      TScriptContext::execute(
        fSessionScriptContextP,
        ruleScript,
        NULL, // context's function table
        NULL // datastore pointer needed for context
      );
    }
    #endif
  } // for all activated rules
  PDEBUGENDBLOCK("RemoteRules");
  // something failed in applying remote rules?
  if (sta!=LOCERR_OK)
    return sta;
  #endif // NO_REMOTE_RULES
  // Final adjustments
  #ifndef NO_REMOTE_RULES
  if (fActiveRemoteRules.empty())
  #endif
  {
    // no remote rule (none found or mechanism excluded by NO_REMOTE_RULES)
    if (!aDevInfP) {
      // no devinf -> blind sync attempt: apply best-guess workaround settings
      // Note that a blind sync attempt means that the remote party is at least partly non-compliant, as we always request a devInf!
      PDEBUGPRINTFX(DBG_ERROR,("No remote information available -> applying best-guess workaround behaviour options"));
      #ifndef MINIMAL_CODE
      // set device description
      fRemoteDescName = fRemoteName.empty() ? "[unknown remote]" : fRemoteName.c_str();
      fRemoteDescName += " (no devInf)";
      #endif // MINIMAL_CODE
      // switch on legacy behaviour (conservative preferred types)
      fLegacyMode = true;
      if (IS_CLIENT) {
        // Client case
        fRemoteCanHandleUTC = true; // Assume server can handle UTC (it is very improbable a server can't)
      }
      else {
        // Server case
        fRemoteCanHandleUTC = fSyncMLVersion==syncml_vers_1_0 ? true : false; // Assume client cannot handle UTC (it is likely a client can't, or at least can't properly, so localtime is safer)
        fLimitedRemoteFieldLengths = true; // assume limited client field length (almost all clients have limited length)
      }
    }
  }
  // show summary
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("Summary of all behaviour options (possibly modified by remote rule(s))"));
  #ifndef MINIMAL_CODE
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Remote Description        : %s",fRemoteDescName.c_str()));
  #endif
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Legacy mode               : %s",boolString(fLegacyMode)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Lenient mode              : %s",boolString(fLenientMode)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Limited Field Lengths     : %s",boolString(fLimitedRemoteFieldLengths)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Do not send empty props   : %s",boolString(fDontSendEmptyProperties)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Quote 8bit content        : %s",boolString(fDoQuote8BitContent)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Prevent Content Folding   : %s",boolString(fDoNotFoldContent)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- No replace in slowsync    : %s",boolString(fNoReplaceInSlowsync)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Treat remote TZ as local  : %s",boolString(fTreatRemoteTimeAsLocal)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Treat remote TZ as UTC    : %s",boolString(fTreatRemoteTimeAsUTC)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Use 23:59:59 end dates    : %s",boolString(fVCal10EnddatesSameDay)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Ignore field maxSize      : %s",boolString(fIgnoreDevInfMaxSize)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Ignore CTCap              : %s",boolString(fIgnoreCTCap)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- send DS path in devInf    : %s",boolString(fDSPathInDevInf)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- send DS CGI in devInf     : %s",boolString(fDSCgiInDevInf)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Update Client in slowsync : %s",boolString(fUpdateClientDuringSlowsync)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Update Server in slowsync : %s",boolString(fUpdateServerDuringSlowsync)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Allow message retries     : %s",boolString(fAllowMessageRetries)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Strict SyncML exec order  : %s",boolString(fStrictExecOrdering)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Treat copy like add       : %s",boolString(fTreatCopyAsAdd)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Complete From-Client-Only : %s",boolString(fCompleteFromClientOnly)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Remote can handle UTC     : %s",boolString(fRemoteCanHandleUTC)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Max Request time [sec]    : %ld",static_cast<long>(fRequestMaxTime)));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Content output charset    : %s",MIMECharSetNames[fDefaultOutCharset]));
  PDEBUGPRINTFX(DBG_HOT+DBG_REMOTEINFO,("- Content input charset     : %s",MIMECharSetNames[fDefaultInCharset]));
  // done
  return LOCERR_OK;
} // TSyncSession::checkRemoteSpecifics


#ifndef NO_REMOTE_RULES

// check if given rule (by name, or if aRuleName=NULL by rule pointer) is active
bool TSyncSession::isActiveRule(cAppCharP aRuleName, TRemoteRuleConfig *aRuleP)
{
  TRemoteRulesList::iterator pos;
  for(pos=fActiveRemoteRules.begin();pos!=fActiveRemoteRules.end();pos++) {
    if (
      (aRuleName==NULL && (*pos)==aRuleP) || // match by pointer...
      (strucmp(aRuleName,(*pos)->getName())==0) // ...or name
    )
      return true;
  }
  // no match
  return false;
} // TSyncSession::isActiveRule

#endif // NO_REMOTE_RULES


// access to config
TSessionConfig *TSyncSession::getSessionConfig(void)
{
  TSessionConfig *scP;
  GET_CASTED_PTR(scP,TSessionConfig,getSyncAppBase()->getRootConfig()->fAgentConfigP,DEBUGTEXT("no TSessionConfig","sss1"));
  return scP;
} // TSyncSession::getSessionConfig



// process a Map command in context of session
bool TSyncSession::processMapCommand(
  SmlMapPtr_t aMapCommandP,       // the map command contents
  TStatusCommand &aStatusCommand, // pre-set 200 status, can be modified in case of errors
  bool &aQueueForLater
)
{
  // if not overridden, we cannot process Map
  aStatusCommand.setStatusCode(403);
  ADDDEBUGITEM(aStatusCommand,"Map command not allowed in this context");
  return false; // failed
} // TSyncSession::processMapCommand


// called to issue custom get and put commands
// may issue custom get and put commands
void TSyncSession::issueCustomGetPut(bool aGotDevInf, bool aSentDevInf)
{
  #ifdef SCRIPT_SUPPORT
  // call script that might issue GETs and PUTs
  // - set up context
  TGetPutResultFuncContext ctx;
  ctx.isPut=false;
  ctx.canIssue=true;
  ctx.statuscode=0;
  ctx.itemURI.erase();
  ctx.itemData.erase();
  ctx.metaType.erase();
  // - execute
  TScriptContext::execute(
    fSessionScriptContextP,
    getSessionConfig()->fCustomGetPutScript,
    &GetPutResultFuncTable,
    &ctx // caller context
  );
  #endif
} // TSyncSession::issueCustomGetPut


// called to issue custom put commands at end of session
// may issue custom put commands (gets don't make sense at end of a session)
void TSyncSession::issueCustomEndPut(void)
{
  #ifdef SCRIPT_SUPPORT
  // call script that might issue GETs and PUTs
  // - set up context
  TGetPutResultFuncContext ctx;
  ctx.isPut=false;
  ctx.canIssue=true;
  ctx.statuscode=0;
  ctx.itemURI.erase();
  ctx.itemData.erase();
  ctx.metaType.erase();
  // - execute
  TScriptContext::execute(
    fSessionScriptContextP,
    getSessionConfig()->fCustomEndPutScript,
    &GetPutResultFuncTable,
    &ctx // caller context
  );
  #endif
} // TSyncSession::issueCustomEndPut





// called to process unknown get item, may return a Results command. Must set status to non-404 if get could be served
// (may be overridden by descendants, only called if no descendant can handle an item)
TResultsCommand *TSyncSession::processGetItem(const char *aLocUri, TGetCommand *aGetCommandP, SmlItemPtr_t aGetItemP, TStatusCommand &aStatusCommand)
{
  TResultsCommand *resultsCmdP = NULL;
  #ifdef SCRIPT_SUPPORT
  // first check if script handles it
  // - set up context
  TGetPutResultFuncContext ctx;
  ctx.isPut=false;
  ctx.canIssue=false;
  ctx.statuscode=aStatusCommand.getStatusCode();
  ctx.itemURI=aLocUri;
  ctx.itemData.erase();
  // - get meta type of item, if any
  AssignString(ctx.metaType,smlMetaTypeToCharP(smlPCDataToMetInfP(aGetItemP->meta)));
  if (ctx.metaType.empty()) {
    // none in item, get from command
    AssignString(ctx.metaType,smlMetaTypeToCharP(smlPCDataToMetInfP(aGetCommandP->getMeta())));
  }
  // - execute
  bool hasResult =
    TScriptContext::executeTest(
      false, // do not assume script handles the GET
      fSessionScriptContextP,
      getSessionConfig()->fCustomGetHandlerScript,
      &GetPutResultFuncTable,
      &ctx // caller context
    );
  // - update status, anyway
  aStatusCommand.setStatusCode(ctx.statuscode);
  // - create result, if script decides so
  if (hasResult) {
    // script returns true, so it has handled the GET command
    // - create a result command (%%% currently GET command itself does not carry src/targ URIs, only item does)
    resultsCmdP = new TResultsCommand(this,aGetCommandP,NULL,NULL);
    // - create data item
    SmlItemPtr_t resItemP = newItem();
    // - source is get item's URI reflected (if not changed by script)
    resItemP->source=newLocation(ctx.itemURI.c_str());
    // - data is just string
    resItemP->data = newPCDataString(ctx.itemData);
    // - add item to command
    resultsCmdP->addItem(resItemP);
    // - set result command meta if not empty string
    resultsCmdP->setMeta(newMetaType(ctx.metaType.c_str()));
    // get item handled, return
    return resultsCmdP;
  }
  #endif
  // look for ./devinf10 special case
  if (strucmp(aLocUri,SyncMLDevInfNames[getSyncMLVersion()])==0) {
    // status is ok
    aStatusCommand.setStatusCode(200);
    // prepare a devinf10 <Result>
    resultsCmdP = new TDevInfResultsCommand(this,aGetCommandP);
  }
  return resultsCmdP;
} // TSyncSession::processGetItem


// - put and results command processing
//   (may be overridden by descendants, only called if no descendant can handle an item)
void TSyncSession::processPutResultItem(bool aIsPut, const char *aLocUri, TSmlCommand *aPutResultsCommandP, SmlItemPtr_t aPutResultsItemP, TStatusCommand &aStatusCommand)
{
  localstatus sta = aStatusCommand.getStatusCode();
  #ifdef SCRIPT_SUPPORT
  // first check if script handles it
  // - set up context
  TGetPutResultFuncContext ctx;
  ctx.isPut=aIsPut;
  ctx.canIssue=false;
  ctx.statuscode=sta;
  ctx.itemURI=aLocUri;
  // - get data of item, if any
  smlPCDataToStringObj(aPutResultsItemP->data,ctx.itemData);
  // - get meta type of item, if any
  AssignString(ctx.metaType,smlMetaTypeToCharP(smlPCDataToMetInfP(aPutResultsItemP->meta)));
  if (ctx.metaType.empty()) {
    // none in item, get from command
    AssignString(ctx.metaType,smlMetaTypeToCharP(smlPCDataToMetInfP(aPutResultsCommandP->getMeta())));
  }
  // - execute
  bool hasProcessed =
    TScriptContext::executeTest(
      false, // do not assume script handles the PUT or RESULT
      fSessionScriptContextP,
      getSessionConfig()->fCustomPutResultHandlerScript,
      &GetPutResultFuncTable,
      &ctx // caller context
    );
  // update status code
  sta=ctx.statuscode;
  if (sta!=LOCERR_OK)
    aStatusCommand.setStatusCode(syncmlError(sta));
  // if processed, return now
  if (hasProcessed)
    return;
  #endif
  // check for ./devinfXX
  if (strucmp(aLocUri,SyncMLDevInfNames[getSyncMLVersion()])==0) {
    // remote is sending DevInf, receive it
    SmlDevInfDevInfPtr_t devinfP = smlPCDataToDevInfP(aPutResultsItemP->data);
    // save received devinf (if database supports it)
    saveRemoteDevInf(getRemoteURI(),devinfP);
    // analyze
    aStatusCommand.setStatusCode(200); // assume ok
    sta=analyzeRemoteDevInf(devinfP);
    if (sta!=LOCERR_OK)
      aStatusCommand.setStatusCode(syncmlError(sta));
  }
  else {
    // unknown
    PDEBUGPRINTFX(DBG_ERROR,(
      "Unknown %s-command with URI=%s received, returning default status=%hd",
      aIsPut ? "PUT" : "RESULTS",
      aLocUri,
      sta
    ));
  }
} // TSyncSession::processPutResultItem



// process an alert item in context of session
// Most handling takes place in derived classes,
// this base class only implements basic stuff
// - returns command to be issued after issuing status, NULL if none
TSmlCommand *TSyncSession::processAlertItem(
  uInt16 aAlertCode,   // alert code
  SmlItemPtr_t aItemP, // alert item to be processed (as one alert can have multiple items)
  SmlCredPtr_t aCredP, // alert cred element, if any
  TStatusCommand &aStatusCommand, // pre-set 200 status, can be modified in case of errors
  TLocalEngineDS *&aLocalDataStoreP // receives datastore pointer, if alert affects a datastore
)
{
  // no alert response command by default
  TSmlCommand *alertresponsecmdP=NULL;
  TLocalEngineDS *datastoreP;
  string optionsCGI,identifyingTargetURI;

  // dispatch numeric alerts
  switch (aAlertCode) {
    // sync alerts
    case 200:
    case 201:
    case 202:
    case 203:
    case 204:
    case 205:
    // Sync resume alert
    case 225: {
      // Synchronisation initialisation alerts
      if (allowAlertAfterMap() && fIncomingState==psta_map) {
        // reset to state that allows a sync to start
        PDEBUGPRINTFX(DBG_HOT,("process alert: restart sync"));
        fIncomingState = psta_init;
        fOutgoingState = psta_init;
        fRestarting = true;
      }

      // - test if context is ok
      if (fIncomingState!=psta_init && fIncomingState!=psta_initsync) {
        // Sync alert only allowed in init package or combined init/sync
        PDEBUGPRINTFX(DBG_ERROR,(
          "Sync Alert not allowed with incoming package state='%s'",
          PackageStateNames[fIncomingState]
        ));
        aStatusCommand.setStatusCode(403); // forbidden
        ADDDEBUGITEM(aStatusCommand,"Sync Alert only allowed in init package");
        return NULL; // no alert sent back
      }
      // find requested database by URI
      const char *target = smlSrcTargLocURIToCharP(aItemP->target);
      if (!target || !target[0]) {
        // same fallback for Sony Ericsson C510 as in
        // TSyncSession::initSync()
        target = smlSrcTargLocURIToCharP(aItemP->source);
      }
      datastoreP = findLocalDataStoreByURI(
        target, // target as sent from remote
        &optionsCGI, // options, if any
        &identifyingTargetURI // identifying part of URI (CGI removed)
      );
      if (!datastoreP) {
        // no such local datastore
        aStatusCommand.setStatusCode(404); // not found
      }
      else {
        // save alerted datastore pointer (will be returned to caller, which is TAlertCommand)
        aLocalDataStoreP=datastoreP;
        // get anchors
        const char *nextRemoteAnchor = smlMetaNextAnchorToCharP(smlPCDataToMetInfP(aItemP->meta));
        if (nextRemoteAnchor==NULL) nextRemoteAnchor=""; // some remotes may send NO anchor
        const char *lastRemoteAnchor = smlMetaLastAnchorToCharP(smlPCDataToMetInfP(aItemP->meta));
        if (lastRemoteAnchor==NULL) lastRemoteAnchor=""; // some remotes may send NO anchor
        // get URIs
        const char *targetURI = smlSrcTargLocURIToCharP(aItemP->target);
        const char *sourceURI = smlSrcTargLocURIToCharP(aItemP->source);
        // get Filter
        SmlFilterPtr_t targetFilter = aItemP->target ? aItemP->target->filter : NULL;
        if (fRestarting) {
          // reset datastore first
          datastoreP->engFinishDataStoreSync(LOCERR_OK);
        }
        // alert datastore of requested sync
        // - let datastore process alert and generate additional alert if needed
        //   NOTE: this might generate a PUT command if remote needs to see our
        //         devInf (config changed since last sync)
        alertresponsecmdP=datastoreP->engProcessSyncAlert(
          NULL,            // not as subdatastore
          aAlertCode,       // the alert code
          lastRemoteAnchor, // last anchor of client
          nextRemoteAnchor, // next anchor of client
          targetURI,        // target as sent from remote
          identifyingTargetURI.c_str(), // identifying part of URI (relative, options removed)
          optionsCGI.c_str(),           // extracted options (e.g. filtering) CGI
          targetFilter,     // DS 1.2 filter, if any (can be NULL if none)
          sourceURI,        // source URI
          aStatusCommand    // status that might be modified
        );
        // echo next anchor sent with item back in status
        // %%% specs say that only next anchor must be echoed, SCTS echoes both
        SmlItemPtr_t itemP = newItem(); // empty item
        // NOTE: anchor is MetInf, but is echoed in DATA part of item, not META!
        itemP->data = newMetaAnchor(nextRemoteAnchor,NULL); // only next (like specs)
        aStatusCommand.addItem(itemP); // add it to status
      }
      break;
    }
    case 224 :
      // Suspend alert
      SuspendSession(514);
      break;
    case 100 :
      // DISPLAY
      PDEBUGPRINTFX(DBG_HOT,(
        "---------------- DISPLAY ALERT (100): %s",
        smlPCDataToCharP(aItemP->data)
      ));
      // show it on the console
      CONSOLEPRINTF((
        "***** Message from Remote: %s",
        smlPCDataToCharP(aItemP->data)
      ));
      // callback to allow GUI clients to display the message
      if (!SESSION_PROGRESS_EVENT(this,pev_display100,NULL,uIntPtr(smlPCDataToCharP(aItemP->data)),0,0)) {
        // user answered no to our question "continue?"
        aStatusCommand.setStatusCode(514); // cancelled
        // Do NOT abort the session, so give the server a chance to do someting more sensible based on the 514 status.
        aStatusCommand.addItemString("User abort in response to Alert 100 message");
        PDEBUGPRINTFX(DBG_ERROR,("User abort after seeing Alert 100 message: %s",smlPCDataToCharP(aItemP->data)));
      }
      break;
    case 223:
      // Chunking error: missing end of chunk
      aStatusCommand.setStatusCode(223);
      aStatusCommand.addItemString("Missing end of chunk");
      PDEBUGPRINTFX(DBG_ERROR,(
        "Warning: Alert Code 223 -> Missing end of chunk for item localid='%s', remoteid='%s'",
        smlSrcTargLocURIToCharP(aItemP->target),
        smlSrcTargLocURIToCharP(aItemP->source)
      ));
      break;
    default :
      // unknown alert code
      aStatusCommand.setStatusCode(406);
      aStatusCommand.addItemString("Unimplemented Alert Code");
      PDEBUGPRINTFX(DBG_ERROR,("Unimplemented Alert Code %hd -> Status 406",aAlertCode));
      break;
  } // switch fAlertCode
  // return command generated (or NULL if none)
  return alertresponsecmdP;
} // TSyncSession::processAlertItem



#ifdef SYDEBUG
  #define XML_TRANSLATION_ENABLED
#else
  #undef XML_TRANSLATION_ENABLED
#endif

#ifdef SYDEBUG

void TSyncSession::XMLTranslationIncomingStart(void)
{
  // start translation instances
  #ifdef XML_TRANSLATION_ENABLED
  if (fXMLtranslate && !getRootConfig()->fDebugConfig.fDebugInfoPath.empty()) {
    DEBUGPRINTFX(DBG_EXOTIC,("Initializing incoming XML translation instance"))
    if (!getSyncAppBase()->newSmlInstance(
      SML_XML,
      getRootConfig()->fLocalMaxMsgSize * 3, // XML should not be more than 3 times larger than WBXML
      fIncomingXMLInstance
    )) {
      // if instance cannot be created, turn off XML translation to avoid crashes
      fXMLtranslate=false;
      PDEBUGPRINTFX(DBG_ERROR,("XML translation disabled because of lacking memory"))
    }
  }
  else
    fXMLtranslate=false;
  #endif
} // TSyncSession::XMLTranslationIncomingStart


void TSyncSession::XMLTranslationOutgoingStart(void)
{
  #ifdef XML_TRANSLATION_ENABLED
  // start translation instances
  if (fXMLtranslate && !getRootConfig()->fDebugConfig.fDebugInfoPath.empty()) {
    DEBUGPRINTFX(DBG_EXOTIC,("Initializing outgoing XML translation instance"))
    if (!getSyncAppBase()->newSmlInstance(
      SML_XML,
      getRootConfig()->fLocalMaxMsgSize * 3, // XML should not be more than 3 times larger than WBXML
      fOutgoingXMLInstance
    )) {
      // if instance cannot be created, turn off XML translation to avoid crashes
      fXMLtranslate=false;
      PDEBUGPRINTFX(DBG_ERROR,("XML translation disabled because of lacking memory"))
    }
  }
  else
    fXMLtranslate=false;
  #endif
} // TSyncSession::XMLTranslationOutgoingStart


/// @todo
/// rewrite this to use a TDbgOut object to write stuff
// finish and output XML translation of incoming traffic
void TSyncSession::XMLTranslationIncomingEnd(void)
{
  #ifdef XML_TRANSLATION_ENABLED
  if (fIncomingXMLInstance && fXMLtranslate) {
    // write XML translation of input and output to files
    DEBUGPRINTFX(DBG_EXOTIC,("XML translation enabled..."))
    MemPtr_t XMLtext;
    MemSize_t XMLsize;
    string fname;
    // - incoming
    // - get XML
    DEBUGPRINTFX(DBG_EXOTIC,("- Writing incoming XML translation"))
    XMLtext=NULL; XMLsize=0;
    if (smlLockReadBuffer(fIncomingXMLInstance,&XMLtext,&XMLsize)==SML_ERR_OK) {
      // save to file
      TDbgOut *dbgOutP = getSyncAppBase()->newDbgOutputter(false);
      if (dbgOutP) {
        // create base file name (trm = translated message)
        string dumpfilename;
        StringObjPrintf(dumpfilename,
          "%s_trm%03ld_%03ld_incoming",
          getDbgLogger()->getDebugPath(), // path + session log base name
          (long)getLastIncomingMsgID(),
          (long)++fDumpCount // to make sure it is unique even in case of retries
        );
        // open file in raw mode
        if (dbgOutP->openDbg(
          dumpfilename.c_str(),
          ".xml",
          dbgflush_none,
          false, // append to existing if any
          true // raw mode
        )) {
          // write out the entire message
          dbgOutP->putRawData(XMLtext, XMLsize);
          // close the file
          dbgOutP->closeDbg();
          // add a link into the session file to immediately get the file
          PDEBUGPRINTFX(DBG_HOT+DBG_PROTO,(
            "Incoming %sXML message msgID=%ld &html;<a href=\"%s_trm%03ld_%03ld_incoming.xml\" target=\"_blank\">&html;saved as XML translation&html;</a>&html;",
            getEncoding()==SML_XML ? "" : "WB",
            (long)getLastIncomingMsgID(),
            getDbgLogger()->getDebugFilename(), // session log base name
            (long)getLastIncomingMsgID(),
            (long)fDumpCount
          ));
        }
        else {
          PDEBUGPRINTFX(DBG_ERROR,("Cannot write <xmltranslate> file"));
        }
        delete dbgOutP;
      }
    }
  }
  if (fIncomingXMLInstance) {
    // finally free instance
    getSyncAppBase()->freeSmlInstance(fIncomingXMLInstance);
    fIncomingXMLInstance=NULL;
  }
  #endif
} // TSyncSession::XMLTranslationIncomingEnd


// finish and output XML translation of outgoing traffic
/// @todo
/// rewrite this to use a TDbgOut object to write stuff
void TSyncSession::XMLTranslationOutgoingEnd(void)
{
  #ifdef XML_TRANSLATION_ENABLED
  if (fOutgoingXMLInstance && fXMLtranslate) {
    // write XML translation of input and output to files
    DEBUGPRINTFX(DBG_EXOTIC,("XML translation enabled..."))
    MemPtr_t XMLtext;
    MemSize_t XMLsize;
    string fname;
    // - outgoing
    // write only if session is debug-enabled
    // - get XML
    DEBUGPRINTFX(DBG_EXOTIC,("- Writing outgoing XML translation"))
    XMLtext=NULL; XMLsize=0;
    if (smlLockReadBuffer(fOutgoingXMLInstance,&XMLtext,&XMLsize)==SML_ERR_OK) {
      // save to file
      TDbgOut *dbgOutP = getSyncAppBase()->newDbgOutputter(false);
      if (dbgOutP) {
        // create base file name (trm = translated message)
        string dumpfilename;
        StringObjPrintf(dumpfilename,
          "%s_trm%03ld_%03ld_outgoing",
          getDbgLogger()->getDebugPath(), // path + session log base name
          (long)getOutgoingMsgID(),
          (long)++fDumpCount // to make sure it is unique even in case of retries
        );
        // open file in raw mode
        if (dbgOutP->openDbg(
          dumpfilename.c_str(),
          ".xml",
          dbgflush_none,
          false, // append to existing if any
          true // raw mode
        )) {
          // write out the entire message
          dbgOutP->putRawData(XMLtext, XMLsize);
          // close the file
          dbgOutP->closeDbg();
          // add a link into the session file to immediately get the file
          PDEBUGPRINTFX(DBG_HOT+DBG_PROTO,(
            "Outgoing %sXML message msgID=%ld &html;<a href=\"%s_trm%03ld_%03ld_outgoing.xml\" target=\"_blank\">&html;saved as XML translation&html;</a>&html;",
            getEncoding()==SML_XML ? "" : "WB",
            (long)getOutgoingMsgID(),
            getDbgLogger()->getDebugFilename(), // session log base name
            (long)getOutgoingMsgID(),
            (long)fDumpCount
          ));
        }
        else {
          PDEBUGPRINTFX(DBG_ERROR,("Cannot write <xmltranslate> file"));
        }
        delete dbgOutP;
      }
    }
  }
  if (fOutgoingXMLInstance) {
    // finally free instance
    getSyncAppBase()->freeSmlInstance(fOutgoingXMLInstance);
    fOutgoingXMLInstance=NULL;
  }
  #endif
} // TSyncSession::XMLTranslationOutgoingEnd


// dump message from specified buffer
void TSyncSession::DumpSyncMLBuffer(MemPtr_t aBuffer, MemSize_t aBufSize, bool aOutgoing, Ret_t aDecoderError)
{
  #ifdef MSGDUMP
  // log message currently in SML buffer
  if (fMsgDump && !getRootConfig()->fDebugConfig.fDebugInfoPath.empty()) {
    TDbgOut *dbgOutP = getSyncAppBase()->newDbgOutputter(false);
    if (dbgOutP) {
      // create base file name
      string dumpfilename;
      // regular message,
      StringObjPrintf(dumpfilename,
        "%s_msg%03ld_%03ld_%sing",
        getDbgLogger()->getDebugPath(), // path + session log base name
        (long)(aOutgoing ? getOutgoingMsgID() : getLastIncomingMsgID()+1), // just generated msgID / expected next incoming ID
        (long)++fDumpCount, // to make sure it is unique even in case of retries
        aOutgoing ? "outgo" : "incom"
      );
      if (aDecoderError) {
        // append error code
        StringObjAppendPrintf(dumpfilename,"_ERR_0x%04X",aDecoderError);
      }
      // open file in raw mode
      if (dbgOutP->openDbg(
        dumpfilename.c_str(),
        getEncoding()==SML_XML ? ".xml" : ".wbxml",
        dbgflush_none,
        false, // append to existing if any
        true // raw mode
      )) {
        // write out the entire message
        dbgOutP->putRawData(aBuffer, aBufSize);
        // close the file
        dbgOutP->closeDbg();
        // add a link into the session file to immediately get the file if it is XML
        if (getEncoding()==SML_XML) {
          PDEBUGPRINTFX(DBG_HOT+DBG_PROTO,(
            "%sing XML message msgID=%ld &html;<a href=\"%s_msg%03ld_%03ld_%sing.xml\" target=\"_blank\">&html;dumped to file&html;</a>&html;",
            aOutgoing ? "Outgo" : "Incom",
            (long)getOutgoingMsgID(),
            getDbgLogger()->getDebugFilename(), // session log base name
            (long)(aOutgoing ? getOutgoingMsgID() : getLastIncomingMsgID()+1), // just generated msgID / expected next incoming ID
            (long)fDumpCount,
            aOutgoing ? "outgo" : "incom"
          ));
        }
      }
      else {
        PDEBUGPRINTFX(DBG_ERROR,("Cannot write <msgdump> file"));
      }
      delete dbgOutP;
    }
  }
  #endif // MSGDUMP
} // TSyncSession::DumpSyncMLBuffer


// Dump message in SML buffer to file
void TSyncSession::DumpSyncMLMessage(bool aOutgoing)
{
  // dump message if needed
  #ifdef MSGDUMP
  // log message currently in SML buffer
  if (fMsgDump && !getRootConfig()->fDebugConfig.fDebugInfoPath.empty()) {
    // peek into buffer
    MemPtr_t data;
    MemSize_t datasize;
    if (smlPeekMessageBuffer(getSmlWorkspaceID(), aOutgoing, &data, &datasize)==SML_ERR_OK) {
      DumpSyncMLBuffer(data,datasize,aOutgoing,SML_ERR_OK);
    }
  }
  #endif // MSGDUMP
} // TSyncSession::DumpSyncMLMessage


#endif // SYDEBUG


// SyncML Toolkit callback handlers
// ================================


// start of SyncML message
Ret_t TSyncSession::StartMessage(SmlSyncHdrPtr_t aContentP)
{
  #ifdef SYDEBUG
  fSessionLogger.DebugDefineMainThread();
  #endif
  SessionUsed(); // session used
  fLastRequestStarted = getSystemNowAs(TCTX_UTC); // request started
  fMessageRetried = false; // we assume no message retry
  MP_SHOWCURRENT(DBG_PROFILE,"Start of incoming Message");
  TP_START(fTPInfo,TP_general); // could be new thread
  #ifdef EXPIRES_AFTER_DATE
  if (IS_SERVER) {
    // set 1/4 of the date here
    fCopyOfScrambledNow=((getSyncAppBase()->fScrambledNow)<<2)+503; // scramble again a little
  }
  #endif // EXPIRES_AFTER_DATE
  // dump it if configured
  // Note: this must happen here before answer writing to the instance buffer starts, as otherwise
  //       the already consumed part of the buffer might get overwritten (the SyncML message header in this case).
  #ifdef SYDEBUG
  DumpSyncMLMessage(false); // incoming
  #endif
  if (IS_SERVER) {
    // for server, SyncML_Outgoing is started here, as SyncML_Incoming ends before SyncML_Outgoing
    // but for client, document exchange starts with outgoing message
    PDEBUGBLOCKDESC("SyncML_Outgoing","preparing for response before starting to analyze new incoming message");
  }
  PDEBUGBLOCKFMT(("SyncML_Incoming","Starting to analyze incoming message",
    "RequestNo=%ld|SySyncVers=%d.%d.%d.%d",(long)fSyncAppBaseP->requestCount(),
    SYSYNC_VERSION_MAJOR,
    SYSYNC_VERSION_MINOR,
    SYSYNC_SUBVERSION,
    SYSYNC_BUILDNUMBER
  ));
  PDEBUGPRINTFX(DBG_HOT,(
    "=================> Starting to analyze incoming message, SySync V%d.%d.%d.%d, RequestNo=%ld",
    SYSYNC_VERSION_MAJOR,
    SYSYNC_VERSION_MINOR,
    SYSYNC_SUBVERSION,
    SYSYNC_BUILDNUMBER,
    (long)fSyncAppBaseP->requestCount()
  ));
  #ifdef SYDEBUG
  // Start incoming translation before decoding header
  XMLTranslationIncomingStart();
  if (fXMLtranslate && fIncomingXMLInstance) {
    // Note: we must find the SyncML version in advance, as fSyncMLVersion is not yet valid here
    sInt16 hdrVers;
    StrToEnum(SyncMLVerDTDNames,numSyncMLVersions,hdrVers,smlPCDataToCharP(aContentP->version));
    smlStartMessageExt(fIncomingXMLInstance,aContentP,SmlVersionCodes[hdrVers]);
  }
  #endif
  // update encoding
  smlGetEncoding(fSmlWorkspaceID,&fEncoding);
  // create command
  TSyncHeader *syncheaderP;
  MP_NEW(syncheaderP,DBG_OBJINST,"TSyncHeader",TSyncHeader(this,aContentP));
  // execute it (special case for header)
  return processHeader(syncheaderP);
} // TSyncSession::StartMessage


// special entry point to prematurely abort processing of a incoming message
// and cause the necessary cleanup
void TSyncSession::CancelMessageProcessing(void)
{
  #ifdef SYDEBUG
  // Now dump XML translation of incoming message (as far as it was processed at all)
  XMLTranslationIncomingEnd();
  #endif
  // Show premature end of input processing
  PDEBUGPRINTFX(DBG_HOT,(
    "=================> Aborted processing message #%ld, request=%ld",
    (long)fIncomingMsgID,
    (long)fSyncAppBaseP->requestCount()
  ));
} // TSyncSession::CancelMessageProcessing


Ret_t TSyncSession::EndMessage(Boolean_t final)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlEndMessage(fIncomingXMLInstance,final);
  // Now dump XML translation of incoming message
  XMLTranslationIncomingEnd();
  #endif
  // End of incoming message
  PDEBUGPRINTFX(DBG_HOT,(
    "=================> Finished processing incoming message #%ld (%sfinal), request=%ld",
    (long)fIncomingMsgID,
    final ? "" : "not ",
    (long)fSyncAppBaseP->requestCount()
  ));
  // start outgoing message if not already done so
  // Note: this should NOT happen, as EVERY message from the remote should contain a SyncHdr status which
  //       should have started the message already
  if (!fOutgoingStarted) {
    PDEBUGPRINTFX(DBG_ERROR,("Warning: incoming message #%ld did not contain a SyncHdr status (protocol violation)",(long)fIncomingMsgID));
    // try to continue by simply ignoring - might not always work out (e.g. when authorisation is not yet complete, this will fail)
    issueHeader(false);
  }
  // forget pending continue requests
  if (final)
    fNextMessageRequests=0; // no pending next message requests when a message is final
  // make sure peer gets devInf Put if needed (only if it didn't issue a GET)
  // Note: do it here because we have processed all commands (alerts) now but
  //       server response alerts are still in the fEndOfMessageCommands queue.
  //       This ensures that clients gets PUT before it gets ALERTs.
  if (!fRemoteGotDevinf && mustSendDevInf()) {
    // remote has not got devinf and should see it
    if (!getRootConfig()->fNeverPutDevinf) {
      // PUT devinf now
      PDEBUGPRINTFX(DBG_PROTO,("Remote must see our changed devInf -> creating PUT command"));
      TDevInfPutCommand *putcmdP = new TDevInfPutCommand(this);
      issueRootPtr(putcmdP);
    }
    else {
      PDEBUGPRINTFX(DBG_PROTO,("Remote should see devinf, but PUT is suppressed: <neverputdevinf>"));
    }
  }
  // hook for placing custom GET and PUT
  if (!fCustomGetPutSent) {
    fCustomGetPutSent=true;
    issueCustomGetPut(fRemoteDevInfKnown,fRemoteGotDevinf);
  }
  // now issue all commands that may only be issued AFTER sending statuses for incoming commands
  TSmlCommandPContainer::iterator pos;
  while (true) {
    // first in list
    pos=fEndOfMessageCommands.begin();
    if (pos==fEndOfMessageCommands.end()) break; // done
    // take command out of the list
    TSmlCommand *cmdP=(*pos);
    fEndOfMessageCommands.erase(pos);
    PDEBUGPRINTFX(DBG_SESSION,("<--- Issuing command '%s' from EndOfMessage Queue",cmdP->getName()));
    // issue it (doesn't matter if cannot be sent with this message,
    //   it will then be moved into the fNextMessageCommands queue)
    issuePtr(cmdP,fNextMessageCommands,fInterruptedCommandP);
  }
  // now continue with package if it was discontinued in last message
  // %%% if (fNextMessageRequests>0) {
  // We have received a 222 Alert, so continue package now
  // %%% always continue, even if we didn't see a 222 alert
  ContinuePackageRoot();
  // %%% }
  // let client or server do what is needed
  if (fFakeFinalFlag) {
    PDEBUGPRINTFX(DBG_ERROR,("Warning: heavy workaround active - <final/> simulated to get resume without sync-from-client going"));
  }
  MessageEnded(final || fFakeFinalFlag);
  fFakeFinalFlag=false;
  #ifdef SYNCSTATUS_AT_SYNC_CLOSE
  // make sure sync status is disposed
  if (fSyncCloseStatusCommandP) delete fSyncCloseStatusCommandP;
  fSyncCloseStatusCommandP=NULL;
  #endif
  MP_SHOWCURRENT(DBG_PROFILE,"End of incoming message");
  // ok if no exception thrown
  return SML_ERR_OK;
} // TSyncSession::EndMessage



Ret_t TSyncSession::StartSync(SmlSyncPtr_t aContentP)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlStartSync(fIncomingXMLInstance,aContentP);
  #endif
  // create command object
  TSyncCommand *commandP = new TSyncCommand(this,fIncomingMsgID,aContentP);
  // process it
  return process(commandP);
} // TSyncSession::StartSync


Ret_t TSyncSession::EndSync(void)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlEndSync(fIncomingXMLInstance);
  #endif

  /* %%% old version: sync end is no command itself,
         makes queuing <sync> sequences for later processing
         impossible, so we made it be a separate command
  // process Sync End
  // %%% evtl. catch...
  PDEBUGPRINTFX(DBG_HOT,("End of <Sync> command"));
  // Note: do not call if previous Sync start might not have been processed
  if (!fIgnoreIncomingCommands) processSyncEnd();
  return SML_ERR_OK;
  */
  // create command object
  TSyncEndCommand *commandP = new TSyncEndCommand(this,fIncomingMsgID);
  // process it
  return process(commandP);
} // TSyncSession::EndSync


#ifdef ATOMIC_RECEIVE
Ret_t TSyncSession::StartAtomic(SmlAtomicPtr_t aContentP)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlStartAtomic(fIncomingXMLInstance,aContentP);
  #endif
  // NOTE from Specs: Nested Atomic commands are not legal. A nested Atomic
  //   command will generate an error 500 - command failed.
  // create command object
  // %%% create DUMMY command for now
  PDEBUGPRINTFX(DBG_HOT,("Start of Atomic bracket: return Status 406 unimplemented"));
  TUnimplementedCommand *commandP =
    new TUnimplementedCommand(
      this,
      fIncomingMsgID,
      aContentP->cmdID,
      0,
      scmd_copy,
      aContentP,
      406); // optional feature not supported
  // process it
  return process(commandP);
} // TSyncSession::StartAtomic

Ret_t TSyncSession::EndAtomic(void)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlEndAtomic(fIncomingXMLInstance);
  #endif
  // process Atomic end
  // %%% not implemented, just accept
  PDEBUGPRINTFX(DBG_HOT,("End of Atomic bracket"));
  return SML_ERR_OK;
} // TSyncSession::EndAtomic
#endif


#ifdef SEQUENCE_RECEIVE
Ret_t TSyncSession::StartSequence(SmlSequencePtr_t aContentP)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlStartSequence(fIncomingXMLInstance,aContentP);
  #endif
  // %%% later, implement a nestable command object and derive Sequence,Atomic and Sync
  //     from it. Similar to nested command creation, maintain a chain of nested commands;
  //     session will have a pointer to most recent nest and ALL commands will have a pointer
  //     to owning command (or NULL if they are on root level).
  // Sequence is trivial as SySync executes command in sequence anyway
  // - simply keep track of nesting
  fSequenceNesting++;
  PDEBUGPRINTFX(DBG_HOT,("Start of Sequence bracket, nesting level is now %hd",fSequenceNesting));
  // get cmdid
  sInt32 cmdid;
  StrToLong(smlPCDataToCharP(aContentP->cmdID),cmdid);
  // make status
  TStatusCommand *statusCmdP = new TStatusCommand(
    this,                                 // associated session (for callbacks)
    cmdid,                                // referred-to command ID
    scmd_sequence,                        // referred-to command type (scmd_xxx)
    (aContentP->flags & SmlNoResp_f)!=0,  // set if no-Resp
    200                                   // status code
  ); // issue ok status
  // - return status
  issueRootPtr(statusCmdP);
  // - ok
  return SML_ERR_OK;
} // TSyncSession::StartSequence


Ret_t TSyncSession::EndSequence(void)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlEndSequence(fIncomingXMLInstance);
  #endif
  // - keep track of nesting
  if (fSequenceNesting<1) {
    // error in nesting
    PDEBUGPRINTFX(DBG_HOT,("End of Sequence bracket, MISSING PRECEEDING SEQUENCE START -> aborting session"));
    AbortSession(400,true); // bad nesting is severe, abort session
  }
  else {
    // nesting ok
    fSequenceNesting--;
    PDEBUGPRINTFX(DBG_HOT,("End of Sequence bracket, nesting level is now %hd",fSequenceNesting));
  }
  return SML_ERR_OK;
} // TSyncSession::EndSequence
#endif


Ret_t TSyncSession::AddCmd(SmlAddPtr_t aContentP)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlAddCmd(fIncomingXMLInstance,aContentP);
  #endif
  // create SyncOp command object
  TSyncOpCommand *commandP = new TSyncOpCommand(
    this,
    fLocalSyncDatastoreP,
    fIncomingMsgID,
    sop_add,
    scmd_add,
    aContentP
  );
  // process it
  return process(commandP);
} // TSyncSession::AddCmd


Ret_t TSyncSession::AlertCmd(SmlAlertPtr_t aContentP)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlAlertCmd(fIncomingXMLInstance,aContentP);
  #endif
  // create command object
  TAlertCommand *commandP = new TAlertCommand(this,fIncomingMsgID,aContentP);
  // process it
  return process(commandP);
} // TSyncSession::AlertCmd


Ret_t TSyncSession::DeleteCmd(SmlDeletePtr_t aContentP)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlDeleteCmd(fIncomingXMLInstance,aContentP);
  #endif
  // determine type of delete
  TSyncOperation syncop;
  if (aContentP->flags & SmlArchive_f) syncop = sop_archive_delete;
  else if (aContentP->flags & SmlSftDel_f) syncop = sop_soft_delete;
  else syncop=sop_delete;
  // create SyncOp command object
  TSyncOpCommand *commandP = new TSyncOpCommand(
    this,
    fLocalSyncDatastoreP, // note that this one might be NULL in case previous sync command was delayed
    fIncomingMsgID,
    syncop,
    scmd_delete,
    aContentP
  );
  // process it
  return process(commandP);
} // TSyncSession::DeleteCmd


// process GET commands
Ret_t TSyncSession::GetCmd(SmlGetPtr_t aContentP)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlGetCmd(fIncomingXMLInstance,aContentP);
  #endif
  // create command object
  TGetCommand *commandP = new TGetCommand(this,fIncomingMsgID,aContentP);
  // process it
  return process(commandP);
} // TSyncSession::GetCmd


Ret_t TSyncSession::PutCmd(SmlPutPtr_t aContentP)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlPutCmd(fIncomingXMLInstance,aContentP);
  #endif
  // create command object
  TPutCommand *commandP = new TPutCommand(this,fIncomingMsgID,aContentP);
  // process it
  return process(commandP);
} // TSyncSession::PutCmd


#ifdef MAP_RECEIVE
Ret_t TSyncSession::MapCmd(SmlMapPtr_t aContentP)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlMapCmd(fIncomingXMLInstance,aContentP);
  #endif
  // create command object
  TMapCommand *commandP = new TMapCommand(this,fIncomingMsgID,aContentP);
  // process it
  return process(commandP);
} // TSyncSession::MapCmd
#endif


#ifdef RESULT_RECEIVE
Ret_t TSyncSession::ResultsCmd(SmlResultsPtr_t aContentP)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlResultsCmd(fIncomingXMLInstance,aContentP);
  #endif
  // create command object
  TResultsCommand *commandP = new TResultsCommand(this,fIncomingMsgID,aContentP);
  // process it
  return process(commandP);
} // TSyncSession::ResultsCmd
#endif


Ret_t TSyncSession::StatusCmd(SmlStatusPtr_t aContentP)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlStatusCmd(fIncomingXMLInstance,aContentP);
  #endif
  // create command object
  TStatusCommand *statuscommandP = new TStatusCommand(this,fIncomingMsgID,aContentP);
  // handle status (search for command that waits for this status)
  return handleStatus(statuscommandP);
} // TSyncSession::StatusCmd


Ret_t TSyncSession::ReplaceCmd(SmlReplacePtr_t aContentP)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlReplaceCmd(fIncomingXMLInstance,aContentP);
  #endif
  // create SyncOp command object
  TSyncOpCommand *commandP = new TSyncOpCommand(
    this,
    fLocalSyncDatastoreP,
    fIncomingMsgID,
    sop_replace,
    scmd_replace,
    aContentP
  );
  // process it
  return process(commandP);
} // TSyncSession::ReplaceCmd


#ifdef COPY_RECEIVE
Ret_t TSyncSession::CopyCmd(SmlReplacePtr_t aContentP)
{
  #ifdef SYDEBUG
  #ifdef COPY_SEND
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlCopyCmd(fIncomingXMLInstance,aContentP);
  #else
  #error "We will have incomplete XML translation when only COPY_RECEIVE is defined"
  #endif
  #endif
  // create SyncOp command object
  TSyncOpCommand *commandP = new TSyncOpCommand(
    this,
    fLocalSyncDatastoreP,
    fIncomingMsgID,
    fTreatCopyAsAdd ? sop_add : sop_copy,
    scmd_copy,
    aContentP
  );
  // process it
  return process(commandP);
} // TSyncSession::CopyCmd
#endif


Ret_t TSyncSession::MoveCmd(SmlReplacePtr_t aContentP)
{
  #ifdef SYDEBUG
  // generate XML translation
  if (fXMLtranslate && fIncomingXMLInstance)
    smlMoveCmd(fIncomingXMLInstance,aContentP);
  #endif
  // create SyncOp command object
  TSyncOpCommand *commandP = new TSyncOpCommand(
    this,
    fLocalSyncDatastoreP,
    fIncomingMsgID,
    sop_move,
    scmd_move,
    aContentP
  );
  // process it
  return process(commandP);
} // TSyncSession::MoveCmd

// - error handling
Ret_t TSyncSession::HandleError(void)
{
  // %%% tbd
  DEBUGPRINTFX(DBG_ERROR,("HandleError reached"));
  return SML_ERR_OK; // %%%
} // TSyncSession::HandleError



Ret_t TSyncSession::DummyHandler(const char* msg)
{
  //DEBUGPRINTFX(DBG_ERROR,("DummyHandler: msg=%s",msg));
  return SML_ERR_OK;
} // TSyncSession::DummyHandler



#ifdef ENGINEINTERFACE_SUPPORT

// Support for EngineModule common interface
// =========================================

// open subkey by name (not by path!)
// - this is the actual implementation
TSyError TSessionKey::OpenSubKeyByName(
  TSettingsKeyImpl *&aSettingsKeyP,
  cAppCharP aName, stringSize aNameSize,
  uInt16 aMode
) {
  #ifdef SCRIPT_SUPPORT
  if (strucmp(aName,"sessionvars",aNameSize)==0) {
    // note: if no session scripts are used, context does not exist and is NULL.
    //       TScriptVarKey does not crash with a NULL, so we can give ok here (but no session vars
    //       will be accessible).
    aSettingsKeyP = new TScriptVarKey(fEngineInterfaceP,fSessionP->getSessionScriptContext());
  }
  else
  #endif
    return inherited::OpenSubKeyByName(aSettingsKeyP,aName,aNameSize,aMode);
  // opened a key
  return LOCERR_OK;
} // TSessionKey::OpenSubKeyByName

#endif // ENGINEINTERFACE_SUPPORT


} // namespace sysync


// factory methods of Session Config
// =================================

// only one of XML2GO or SDK/Plugin can be on top of customagent
#ifdef XML2GO_SUPPORT
  #include "xml2goapids.h"
#elif defined(SDK_SUPPORT)
  #include "pluginapids.h"
#endif
// ODBC can be in-between if selected
#ifdef SQL_SUPPORT
  #include "odbcapids.h"
#endif


namespace sysync {

#ifndef HARDCODED_CONFIG

// create new datastore config by name
// returns NULL if none found
TLocalDSConfig *TSessionConfig::newDatastoreConfig(const char *aName, const char *aType, TConfigElement *aParentP)
{
  #ifdef XML2GO_SUPPORT
  if (aType && strucmp(aType,"xml2go")==0) {
    // xml2go enhanced datastore
    return new TXml2goDSConfig(aName,aParentP);
  }
  else
  #elif defined(SDK_SUPPORT)
  if (aType && strucmp(aType,"plugin")==0) {
    // APIDB enhanced datastore (on top of ODBC if SQL_SUPPORT is on)
    return new TPluginDSConfig(aName,aParentP);
  }
  else
  #endif
  #ifdef SQL_SUPPORT
  if (aType==0 || strucmp(aType,"odbc")==0 || strucmp(aType,"sql")==0)  {
    // ODBC enabled datastore
    return new TOdbcDSConfig(aName,aParentP);
  }
  else
  #endif
    return NULL; // unknown datastore
} // TSessionConfig::newDatastoreConfig

#endif // HARDCODED_CONFIG

bool TSyncSession::receivedSyncModeExtensions()
{
  TRemoteDataStorePContainer::iterator pos;
  for (pos=fRemoteDataStores.begin(); pos!=fRemoteDataStores.end(); ++pos) {
    set<string> modes;
    (*pos)->getSyncModes(modes);
    set<string>::const_iterator it;
    for (it=modes.begin(); it!=modes.end(); ++it) {
      const char *nptr = it->c_str();
      char *endptr;
      if (!*nptr) {
        // ignore empty mode
        continue;
      }
      long mode = strtol(nptr, &endptr, 10);
      // ignore trailing spaces
      while (isspace(*endptr)) {
        endptr++;
      }
      if (*endptr) {
        // non-standard character => found extensions
        return true;
      }

      if (mode > 32) {
        // Non-standed integer code!
        // Choosing 32 is somewhat random, not all of those
        // are really defined in the standard.
        return true;
      }
    }
  }
  return false;
}

} // namespace sysync

#endif // not SYNCSESSION_PART1_EXCLUDE

// eof
