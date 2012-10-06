/**
 *  @File     localengineds.cpp
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TLocalEngineDS
 *    Abstraction of the local datastore - interface class to the
 *    sync engine.
 *
 *    Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-09-15 : luz : created from localdatastore
 */

// includes
#include "prefix_file.h"
#include "sysync.h"
#include "localengineds.h"
#include "syncappbase.h"
#include "scriptcontext.h"
#include "superdatastore.h"
#include "syncagent.h"


using namespace sysync;

namespace sysync {

#ifdef SYDEBUG

cAppCharP const LocalDSStateNames[numDSStates] = {
  "idle",
  "client_initialized",
  "admin_ready",
  "client_sent_alert",
  "server_alerted",
  "server_answered_alert",
  "client_alert_statused",
  "client_alerted",
  "sync_mode_stable",
  "data_access_started",
  "sync_set_ready",
  "client_sync_gen_started",
  "server_seen_client_mods",
  "server_sync_gen_started",
  "sync_gen_done",
  "data_access_done",
  "client_maps_sent",
  "admin_done",
  "completed"
};
#endif


#ifdef OBJECT_FILTERING

// add a new expression to an existing filter
static void addToFilter(const char *aNewFilter, string &aFilter, bool aORChain=false)
{
  if (aNewFilter && *aNewFilter) {
    // just assign if current filter expression is empty
    if (aFilter.empty())
      aFilter = aNewFilter;
    else {
      // construct new filter
      string newFilter;
      StringObjPrintf(newFilter,"(%s)%c(%s)",aFilter.c_str(),aORChain ? '|' : '&',aNewFilter);
      aFilter = newFilter;
    }
  }
} // addToFilter

#endif

#ifdef SCRIPT_SUPPORT

// Script functions
// ================

class TLDSfuncs {
public:

  #ifdef OBJECT_FILTERING
  #ifdef SYNCML_TAF_SUPPORT

  // string GETCGITARGETFILTER()
  // returns current CGI-specified target address filter expression
  static void func_GetCGITargetFilter(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TLocalEngineDS *dsP = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext());
    aTermP->setAsString(dsP->fTargetAddressFilter.c_str());
  }; // func_GetCGITargetFilter


  // string GETTARGETFILTER()
  // returns current internal target address filter expression
  static void func_GetTargetFilter(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TLocalEngineDS *dsP = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext());
    aTermP->setAsString(dsP->fIntTargetAddressFilter.c_str());
  }; // func_GetTargetFilter


  // SETTARGETFILTER(string filter)
  // sets (overwrites) the internal target address filter
  static void func_SetTargetFilter(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TLocalEngineDS *dsP = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext());
    aFuncContextP->getLocalVar(0)->getAsString(dsP->fIntTargetAddressFilter);
  }; // func_SetTargetFilter


  // ADDTARGETFILTER(string filter)
  // adds a filter expression to the existing internal targetfilter (automatically paranthesizing and adding AND)
  static void func_AddTargetFilter(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string f;
    TLocalEngineDS *dsP = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext());
    aFuncContextP->getLocalVar(0)->getAsString(f);
    addToFilter(f.c_str(),dsP->fIntTargetAddressFilter,false); // AND-chaining
  }; // func_AddTargetFilter

  #endif // SYNCML_TAF_SUPPORT


  // string GETFILTER()
  // returns current sync set filter expression
  static void func_GetFilter(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TLocalEngineDS *dsP = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext());
    aTermP->setAsString(dsP->fSyncSetFilter.c_str());
  }; // func_GetFilter


  // SETFILTER(string filter)
  // sets (overwrites) the current sync set filter
  static void func_SetFilter(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TLocalEngineDS *dsP = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext());
    aFuncContextP->getLocalVar(0)->getAsString(dsP->fSyncSetFilter);
    dsP->engFilteredFetchesFromDB(true); // update filter dependencies
  }; // func_SetFilter


  // ADDFILTER(string filter)
  // adds a filter expression to the existing (dynamic) targetfilter (automatically paranthesizing and adding AND)
  static void func_AddFilter(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string f;
    TLocalEngineDS *dsP = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext());
    aFuncContextP->getLocalVar(0)->getAsString(f);
    addToFilter(f.c_str(),dsP->fSyncSetFilter,false); // AND-chaining
    dsP->engFilteredFetchesFromDB(true); // update filter dependencies
  }; // func_AddFilter


  // ADDSTATICFILTER(string filter)
  // adds a filter expression to the existing (static) localdbfilter (automatically paranthesizing and adding AND)
  static void func_AddStaticFilter(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string f;
    TLocalEngineDS *dsP = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext());
    aFuncContextP->getLocalVar(0)->getAsString(f);
    addToFilter(f.c_str(),dsP->fLocalDBFilter,false); // AND-chaining
    dsP->engFilteredFetchesFromDB(true); // update filter dependencies
  }; // func_AddStaticFilter

  #endif // OBJECT_FILTERING

  #ifdef SYSYNC_TARGET_OPTIONS

  // string DBOPTIONS()
  // returns current DB options
  static void func_DBOptions(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TLocalEngineDS *dsP = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext());
    aTermP->setAsString(dsP->fDBOptions.c_str());
  }; // func_DBOptions


  // integer DBHANDLESOPTS()
  // returns true if database can completely handle options like /dr() and /li during fetching
  static void func_DBHandlesOpts(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsBoolean(
      static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->dsOptionFilterFetchesFromDB()
    );
  }; // func_DBHandlesOpts


  // timestamp STARTDATE()
  // returns startdate if one is set in datastore
  static void func_StartDate(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    lineartime_t d = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->fDateRangeStart;
    TTimestampField *resP = static_cast<TTimestampField *>(aTermP);
    if (d==noLinearTime)
      resP->assignEmpty();
    else
      resP->setTimestampAndContext(d,TCTX_UTC);
  }; // func_StartDate


  // SETSTARTDATE(timestamp startdate)
  // sets startdate for datastore
  static void func_SetStartDate(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));
    timecontext_t tctx;

    static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->fDateRangeStart =
      tsP->getTimestampAs(TCTX_UTC,&tctx); // floating will also be treated as UTC
  }; // func_SetStartDate


  // timestamp ENDDATE()
  // returns enddate if one is set in datastore
  static void func_EndDate(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    lineartime_t d = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->fDateRangeEnd;
    TTimestampField *resP = static_cast<TTimestampField *>(aTermP);
    if (d==noLinearTime)
      resP->assignEmpty();
    else
      resP->setTimestampAndContext(d,TCTX_UTC);
  }; // func_EndDate


  // SETENDDATE(timestamp startdate)
  // sets enddate for datastore
  static void func_SetEndDate(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));
    timecontext_t tctx;

    static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->fDateRangeEnd =
      tsP->getTimestampAs(TCTX_UTC,&tctx); // floating will also be treated as UTC
  }; // func_SetEndDate


  // integer DEFAULTSIZELIMIT()
  // returns limit set for all items in this datastore (the /li(xxx) CGI option value)
  static void func_DefaultLimit(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    fieldinteger_t i = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->fSizeLimit;
    if (i<0)
      aTermP->unAssign(); // no limit
    else
      aTermP->setAsInteger(i);
  }; // func_DefaultLimit


  // SETDEFAULTSIZELIMIT(integer limit)
  // sets limit for all items in this datastore (the /li(xxx) CGI option value)
  static void func_SetDefaultLimit(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->fSizeLimit =
      aFuncContextP->getLocalVar(0)->getAsInteger();
  }; // func_SetDefaultLimit


  // integer NOATTACHMENTS()
  // returns true if attachments should be suppressed (/na CGI option)
  static void func_NoAttachments(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsBoolean(
      static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->fNoAttachments
    );
  }; // func_NoAttachments


  // SETNOATTACHMENTS(integer flag)
  // if true, attachments will be suppressed (/na CGI option)
  static void func_SetNoAttachments(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->fNoAttachments =
      aFuncContextP->getLocalVar(0)->getAsBoolean();
  }; // func_SetNoAttachments


  // integer MAXITEMCOUNT()
  // returns item count limit (0=none) as set by /max(n) CGI option
  static void func_MaxItemCount(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsInteger(
      static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->fMaxItemCount
    );
  }; // func_MaxItemCount


  // SETMAXITEMCOUNT(integer maxcount)
  // set item count limit (0=none) as set by /max(n) CGI option
  static void func_SetMaxItemCount(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->fMaxItemCount =
      aFuncContextP->getLocalVar(0)->getAsInteger();
  }; // func_SetMaxItemCount

  #endif // SYSYNC_TARGET_OPTIONS


  // integer SLOWSYNC()
  // returns true if we are in slow sync
  static void func_SlowSync(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsBoolean(
      static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->isSlowSync()
    );
  }; // func_SlowSync


  // FORCESLOWSYNC()
  // force a slow sync (like with /na CGI option)
  static void func_ForceSlowSync(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->engForceSlowSync();
  }; // func_ForceSlowSync



  // integer ALERTCODE()
  // returns the alert code as currently know by datastore (might change from normal to slow while processing)
  static void func_AlertCode(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsInteger(
      static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->fAlertCode
    );
  }; // func_AlertCode


  // SETALERTCODE(integer maxcount)
  // set the alert code (makes sense in alertscript to modify the incoming code to something different)
  static void func_SetAlertCode(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->fAlertCode =
      aFuncContextP->getLocalVar(0)->getAsInteger();
  }; // func_SetAlertCode



  // integer REFRESHONLY()
  // returns true if sync is only refreshing local (note that alert code might be different, as local
  // refresh can take place without telling the remote so, for compatibility with clients that do not support the mode)
  static void func_RefreshOnly(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsBoolean(
      static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->isRefreshOnly()
    );
  }; // func_RefreshOnly


  // SETREFRESHONLY(integer flag)
  // modifies the refresh only flag (one way sync from remote to local only)
  // Note that clearing this flag when a client has alerted one-way will probably lead to an error
  static void func_SetRefreshOnly(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->engSetRefreshOnly(
      aFuncContextP->getLocalVar(0)->getAsBoolean()
    );
  }; // func_SetRefreshOnly


  // integer READONLY()
  // returns true if sync is read-only (only reading from local datastore)
  static void func_ReadOnly(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsBoolean(
      static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->isReadOnly()
    );
  }; // func_ReadOnly


  // SETREADONLY(integer flag)
  // modifies the read only flag (only reading from local datastore)
  static void func_SetReadOnly(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->engSetReadOnly(
      aFuncContextP->getLocalVar(0)->getAsBoolean()
    );
  }; // func_SetReadOnly



  // integer FIRSTTIMESYNC()
  // returns true if we are in first time slow sync
  static void func_FirstTimeSync(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsBoolean(
      static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->isFirstTimeSync()
    );
  }; // func_FirstTimeSync


  // void SETCONFLICTSTRATEGY(string strategy)
  // sets conflict strategy for this session
  static void func_SetConflictStrategy(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // convert to syncop
    string s;
    aFuncContextP->getLocalVar(0)->getAsString(s);
    sInt16 strategy;
    StrToEnum(conflictStrategyNames,numConflictStrategies, strategy, s.c_str());
    static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->fSessionConflictStrategy =
      (TConflictResolution) strategy;
  }; // func_SetConflictStrategy


  // string DBNAME()
  // returns name of DB
  static void func_DBName(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsString(
      static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->getName()
    );
  }; // func_DBName

  // void ABORTDATASTORE(integer statuscode)
  static void func_AbortDatastore(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->engAbortDataStoreSync(aFuncContextP->getLocalVar(0)->getAsInteger(),true); // we cause the abort locally
  } // func_AbortDatastore

  // string LOCALDBNAME()
  // returns name of local DB with which it was identified for the sync
  static void func_LocalDBName(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsString(
      static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext())->getIdentifyingName()
    );
  }; // func_LocalDBName


  // string REMOTEDBNAME()
  // returns remote datastore's full name (as used by the remote in <sync> command, may contain subpath and CGI)
  static void func_RemoteDBName(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TLocalEngineDS *dsP = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext());
    aTermP->setAsString(dsP->getRemoteDatastore()->getFullName());
  }; // func_RemoteDBName


  #ifdef SYSYNC_CLIENT

  // ADDTARGETCGI(string cgi)
  // adds CGI to the target URI. If target URI already contains a ?, string will be just
  // appended, otherwise a ? is added, then the new CGI.
  // Note: if string to be added is already contained, it will not be added again
  static void func_AddTargetCGI(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TLocalEngineDS *dsP = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext());
    // Add extra CGI specified
    string cgi;
    aFuncContextP->getLocalVar(0)->getAsString(cgi);
    addCGItoString(dsP->fRemoteDBPath,cgi.c_str(),true);
  }; // func_AddTargetCGI


  // SETRECORDFILTER(string filter, boolean inclusive)
  // Sets record level filter expression for remote
  static void func_SetRecordFilter(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TLocalEngineDS *dsP = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext());
    // put into record level filter
    if (dsP->getSession()->getSyncMLVersion()>=syncml_vers_1_2) {
      // DS 1.2: use <filter>
      aFuncContextP->getLocalVar(0)->getAsString(dsP->fRemoteRecordFilterQuery);
      dsP->fRemoteFilterInclusive = aFuncContextP->getLocalVar(1)->getAsBoolean();
    }
    else if (!aFuncContextP->getLocalVar(0)->isEmpty()) {
      // DS 1.1 and below and not empty filter: add as cgi
      string filtercgi;
      if (aFuncContextP->getLocalVar(1)->getAsBoolean())
        filtercgi = "/tf("; // exclusive, use TAF
      else
        filtercgi = "/fi("; // inclusive, use sync set filter
      aFuncContextP->getLocalVar(0)->appendToString(filtercgi);
      filtercgi += ')';
      addCGItoString(dsP->fRemoteDBPath,filtercgi.c_str(),true);
    }
  }; // func_SetRecordFilter


  // SETDAYSRANGE(integer daysbefore, integer daysafter)
  // Sets type of record filter
  static void func_SetDaysRange(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TLocalEngineDS *dsP = static_cast<TLocalEngineDS *>(aFuncContextP->getCallerContext());
    // get params
    int daysbefore = aFuncContextP->getLocalVar(0)->getAsInteger();
    int daysafter = aFuncContextP->getLocalVar(1)->getAsInteger();
    // depending on SyncML version, create a SINCE/BEFORE filter or use the /dr(x,y) syntax
    if (dsP->getSession()->getSyncMLVersion()>=syncml_vers_1_2 && static_cast<TSyncAgent *>(dsP->getSession())->fServerHasSINCEBEFORE) {
      // use the SINCE/BEFORE syntax
      // BEFORE&EQ;20070808T000000Z&AND;SINCE&EQ;20070807T000000Z
      lineartime_t now = getSystemNowAs(TCTX_UTC,aFuncContextP->getSessionZones());
      string ts;
      // AND-chain with possibly existing filter
      cAppCharP sep = "";
      if (!dsP->fRemoteRecordFilterQuery.empty())
        sep = "&AND;";
      if (daysbefore>=0) {
        dsP->fRemoteRecordFilterQuery += sep;
        dsP->fRemoteRecordFilterQuery += "SINCE&EQ;";
        TimestampToISO8601Str(ts,now-daysbefore*linearDateToTimeFactor,TCTX_UTC,false,false);
        dsP->fRemoteRecordFilterQuery += ts;
        sep = "&AND;";
      }
      if (daysafter>=0) {
        dsP->fRemoteRecordFilterQuery += sep;
        dsP->fRemoteRecordFilterQuery += "BEFORE&EQ;";
        TimestampToISO8601Str(ts,now+daysafter*linearDateToTimeFactor,TCTX_UTC,false,false);
        dsP->fRemoteRecordFilterQuery += ts;
      }
    }
    else {
      // use the /dr(-x,y) syntax
      string rangecgi;
      StringObjPrintf(rangecgi,"/dr(%ld,%ld)",(long int)(-daysbefore),(long int)(daysafter));
      addCGItoString(dsP->fRemoteDBPath,rangecgi.c_str(),true);
    }
  }; // func_SetDaysRange


  #endif // SYSYNC_CLIENT

}; // TLDSfuncs

const uInt8 param_FilterArg[] = { VAL(fty_string) };
const uInt8 param_DateArg[] = { VAL(fty_timestamp) };
const uInt8 param_IntArg[] = { VAL(fty_integer) };
const uInt8 param_StrArg[] = { VAL(fty_string) };
const uInt8 param_OneInteger[] = { VAL(fty_integer) };

const TBuiltInFuncDef DBFuncDefs[] = {
  #ifdef OBJECT_FILTERING
  #ifdef SYNCML_TAF_SUPPORT
  { "GETCGITARGETFILTER", TLDSfuncs::func_GetCGITargetFilter, fty_string, 0, NULL },
  { "GETTARGETFILTER", TLDSfuncs::func_GetTargetFilter, fty_string, 0, NULL },
  { "SETTARGETFILTER", TLDSfuncs::func_SetTargetFilter, fty_none, 1, param_FilterArg },
  { "ADDTARGETFILTER", TLDSfuncs::func_AddTargetFilter, fty_none, 1, param_FilterArg },
  #endif
  { "GETFILTER", TLDSfuncs::func_GetFilter, fty_string, 0, NULL },
  { "SETFILTER", TLDSfuncs::func_SetFilter, fty_none, 1, param_FilterArg },
  { "ADDFILTER", TLDSfuncs::func_AddFilter, fty_none, 1, param_FilterArg },
  { "ADDSTATICFILTER", TLDSfuncs::func_AddStaticFilter, fty_none, 1, param_FilterArg },
  #endif
  #ifdef SYSYNC_TARGET_OPTIONS
  { "DBOPTIONS", TLDSfuncs::func_DBOptions, fty_string, 0, NULL },
  { "STARTDATE", TLDSfuncs::func_StartDate, fty_timestamp, 0, NULL },
  { "ENDDATE", TLDSfuncs::func_EndDate, fty_timestamp, 0, NULL },
  { "SETSTARTDATE", TLDSfuncs::func_SetStartDate, fty_none, 1, param_DateArg },
  { "SETENDDATE", TLDSfuncs::func_SetEndDate, fty_none, 1, param_DateArg },
  { "MAXITEMCOUNT", TLDSfuncs::func_MaxItemCount, fty_integer, 0, NULL },
  { "SETMAXITEMCOUNT", TLDSfuncs::func_SetMaxItemCount, fty_none, 1, param_IntArg },
  { "NOATTACHMENTS", TLDSfuncs::func_NoAttachments, fty_integer, 0, NULL },
  { "SETNOATTACHMENTS", TLDSfuncs::func_SetNoAttachments, fty_none, 1, param_IntArg },
  { "DEFAULTSIZELIMIT", TLDSfuncs::func_DefaultLimit, fty_integer, 0, NULL },
  { "SETDEFAULTSIZELIMIT", TLDSfuncs::func_SetDefaultLimit, fty_none, 1, param_IntArg },
  { "DBHANDLESOPTS", TLDSfuncs::func_DBHandlesOpts, fty_integer, 0, NULL },
  #endif
  { "ALERTCODE", TLDSfuncs::func_AlertCode, fty_integer, 0, NULL },
  { "SETALERTCODE", TLDSfuncs::func_SetAlertCode, fty_none, 1, param_IntArg },
  { "SLOWSYNC", TLDSfuncs::func_SlowSync, fty_integer, 0, NULL },
  { "FORCESLOWSYNC", TLDSfuncs::func_ForceSlowSync, fty_none, 0, NULL },
  { "REFRESHONLY", TLDSfuncs::func_RefreshOnly, fty_integer, 0, NULL },
  { "SETREFRESHONLY", TLDSfuncs::func_SetRefreshOnly, fty_none, 1, param_IntArg },
  { "READONLY", TLDSfuncs::func_ReadOnly, fty_integer, 0, NULL },
  { "SETREADONLY", TLDSfuncs::func_SetReadOnly, fty_none, 1, param_IntArg },
  { "FIRSTTIMESYNC", TLDSfuncs::func_FirstTimeSync, fty_integer, 0, NULL },
  { "SETCONFLICTSTRATEGY", TLDSfuncs::func_SetConflictStrategy, fty_none, 1, param_StrArg },
  { "DBNAME", TLDSfuncs::func_DBName, fty_string, 0, NULL },
  { "LOCALDBNAME", TLDSfuncs::func_LocalDBName, fty_string, 0, NULL },
  { "REMOTEDBNAME", TLDSfuncs::func_RemoteDBName, fty_string, 0, NULL },
  { "ABORTDATASTORE", TLDSfuncs::func_AbortDatastore, fty_none, 1, param_OneInteger },
};

// functions for all datastores
const TFuncTable DBFuncTable = {
  sizeof(DBFuncDefs) / sizeof(TBuiltInFuncDef), // size of table
  DBFuncDefs, // table pointer
  NULL // no chain func
};


#ifdef SYSYNC_CLIENT

const uInt8 param_OneStr[] = { VAL(fty_string) };
const uInt8 param_OneInt[] = { VAL(fty_integer) };
const uInt8 param_TwoInt[] = { VAL(fty_integer), VAL(fty_integer) };
const uInt8 param_SetRecordFilter[] = { VAL(fty_string), VAL(fty_integer) };

const TBuiltInFuncDef ClientDBFuncDefs[] = {
  { "ADDTARGETCGI", TLDSfuncs::func_AddTargetCGI, fty_none, 1, param_OneStr },
  { "SETRECORDFILTER", TLDSfuncs::func_SetRecordFilter, fty_none, 2, param_SetRecordFilter },
  { "SETDAYSRANGE", TLDSfuncs::func_SetDaysRange, fty_none, 2, param_TwoInt },
};


// chain to general DB functions
static void *ClientDBChainFunc(void *&aCtx)
{
  // caller context remains unchanged
  // -> no change needed
  // next table is general DS func table
  return (void *)&DBFuncTable;
} // ClientDBChainFunc


// function table for client-only script functions
const TFuncTable ClientDBFuncTable = {
  sizeof(ClientDBFuncDefs) / sizeof(TBuiltInFuncDef), // size of table
  ClientDBFuncDefs, // table pointer
  ClientDBChainFunc // chain to general agent funcs.
};


#endif // SYSYNC_CLIENT

#endif // SCRIPT_SUPPORT




// config
// ======

// conflict strategy names
// bfo: Problems with XCode (expicit qualification), already within namespace ?
//const char * const sysync::conflictStrategyNames[numConflictStrategies] = {
const char * const conflictStrategyNames[numConflictStrategies] = {
  "duplicate",    // add conflicting counterpart to both databases
  "newer-wins",   // newer version wins (if date/version comparison is possible, like sst_duplicate otherwise)
  "server-wins",  // server version wins (and is written to client)
  "client-wins"   // client version wins (and is written to server)
};


// type support config
TTypeSupportConfig::TTypeSupportConfig(const char* aName, TConfigElement *aParentElement) :
  TConfigElement(aName,aParentElement)
{
  clear();
} // TTypeSupportConfig::TTypeSupportConfig


TTypeSupportConfig::~TTypeSupportConfig()
{
  clear();
} // TTypeSupportConfig::~TTypeSupportConfig


// init defaults
void TTypeSupportConfig::clear(void)
{
  // init defaults
  fPreferredTx = NULL;
  fPreferredRx = NULL;
  fPreferredLegacy = NULL;
  fAdditionalTypes.clear();
  #ifndef NO_REMOTE_RULES
  fRuleMatchTypes.clear();
  #endif
  // clear inherited
  inherited::clear();
} // TTypeSupportConfig::clear


#ifdef HARDCODED_CONFIG

// add type support
bool TTypeSupportConfig::addTypeSupport(
  cAppCharP aTypeName,
  bool aForRead,
  bool aForWrite,
  bool aPreferred,
  cAppCharP aVariant,
  cAppCharP aRuleMatch
) {
  // search datatype
  TDataTypeConfig *typecfgP =
    static_cast<TRootConfig *>(getRootElement())->fDatatypesConfigP->getDataType(aTypeName);
  if (!typecfgP) return false;
  // search variant
  TTypeVariantDescriptor variantDescP = NULL;
  if (aVariant && *aVariant)
    variantDescP = typecfgP->getVariantDescriptor(aVariant);
  // now add datatype
  if (aPreferred) {
    // - preferred
    if (aForRead) {
      if (!fPreferredRx) {
        fPreferredRx=typecfgP; // set it
        fPrefRxVariantDescP=variantDescP;
      }
    }
    if (aForWrite) {
      if (!fPreferredTx) {
        fPreferredTx=typecfgP; // set it
        fPrefTxVariantDescP=variantDescP;
      }
    }
  } // if preferred
  else {
    // - additional
    TAdditionalDataType adt;
    adt.datatypeconfig=typecfgP;
    adt.forRead=aForRead;
    adt.forWrite=aForWrite;
    adt.variantDescP=variantDescP; // variant of that type
    #ifndef NO_REMOTE_RULES
    if (aRuleMatch) {
      // this is a rulematch type (which overrides normal type selection mechanism)
      AssignString(atd.remoteRuleMatch,aRuleMatch); // remote rule match string
      fRuleMatchTypes.push_back(adt); // save it in the list
    }
    else
    #endif
    {
      // standard type
      fAdditionalTypes.push_back(adt); // save it in the list
    }
  }
  return true;
} // TTypeSupportConfig::addTypeSupport

#else

// config element parsing
bool TTypeSupportConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"use")==0) {
    expectEmpty(); // datatype usage specs may not have
    // process arguments
    const char* nam = getAttr(aAttributes,"datatype");
    if (!nam)
      return fail("use must have 'datatype' attribute");
    // search datatype
    TDataTypeConfig *typecfgP =
      static_cast<TRootConfig *>(getRootElement())->fDatatypesConfigP->getDataType(nam);
    if (!typecfgP)
      return fail("unknown datatype '%s' specified",nam);
    #ifndef NO_REMOTE_RULES
    // get rulematch string, if any
    cAppCharP ruleMatch = getAttr(aAttributes,"rulematch");
    #endif
    // convert variant
    TTypeVariantDescriptor variantDescP=NULL; // no variant descriptor by default
    cAppCharP variant = getAttr(aAttributes,"variant");
    if (variant) {
      // get a type-specific descriptor which describes the variant of a type to be used with this datastore
      variantDescP = typecfgP->getVariantDescriptor(variant);
      if (!variantDescP)
        return fail("unknown variant '%s' specified",variant);
    }
    // convert mode
    bool rd=true,wr=true;
    const char* mode = getAttr(aAttributes,"mode");
    if (mode) {
      rd=false;
      wr=false;
      while (*mode) {
        if (tolower(*mode)=='r') rd=true;
        else if (tolower(*mode)=='w') wr=true;
        else {
          ReportError(true,"invalid mode '%c'",*mode);
          return true;
        }
        // next char
        mode++;
      }
      if (!rd && !wr)
        return fail("mode must specify 'r', 'w' or 'rw' at least");
    }
    // get preferred
    bool preferred=false;
    const char* pref = getAttr(aAttributes,"preferred");
    if (pref) {
      if (!StrToBool(pref, preferred)) {
        if (strucmp(pref,"legacy")==0) {
          // this is the preferred type for blind and legacy mode sync attempts
          fPreferredLegacy=typecfgP; // remember (note that there is only ONE preferred type, mode is ignored)
          preferred=false; // not officially preferred
        }
        else
          return fail("bad value for 'preferred'");
      }
    }
    // now add datatype
    if (preferred) {
      // - preferred
      if (rd) {
        if (fPreferredRx)
          return fail("preferred read type already defined");
        else {
          fPreferredRx=typecfgP; // set it
          fPrefRxVariantDescP=variantDescP;
        }
      }
      if (wr) {
        if (fPreferredTx)
          return fail("preferred write type already defined");
        else {
          fPreferredTx=typecfgP; // set it
          fPrefTxVariantDescP=variantDescP;
        }
      }
    } // if preferred
    else {
      // - additional
      TAdditionalDataType adt;
      adt.datatypeconfig=typecfgP;
      adt.forRead=rd;
      adt.forWrite=wr;
      adt.variantDescP=variantDescP;
      #ifndef NO_REMOTE_RULES
      if (ruleMatch) {
        // this is a rulematch type (which overrides normal type selection mechanism)
        AssignString(adt.remoteRuleMatch,ruleMatch); // remote rule match string
        fRuleMatchTypes.push_back(adt); // save it in the list
      }
      else
      #endif
      {
        // standard type
        fAdditionalTypes.push_back(adt); // save it in the list
      }
    }
  }
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TTypeSupportConfig::localStartElement

#endif


// resolve
void TTypeSupportConfig::localResolve(bool aLastPass)
{
  #ifndef HARDCODED_CONFIG
  if (aLastPass) {
    // check for required settings
    if (!fPreferredTx || !fPreferredRx)
      SYSYNC_THROW(TConfigParseException("'typesupport' must contain at least one preferred type for read and write"));
  }
  #endif
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TTypeSupportConfig::localResolve




// datastore config
TLocalDSConfig::TLocalDSConfig(const char* aName, TConfigElement *aParentElement) :
  TConfigElement(aName,aParentElement),
  fTypeSupport("typesupport",this)
{
  clear();
} // TLocalDSConfig::TLocalDSConfig


TLocalDSConfig::~TLocalDSConfig()
{
  // nop so far
} // TLocalDSConfig::~TLocalDSConfig


// init defaults
void TLocalDSConfig::clear(void)
{
  // init defaults
  // - conflict resolution strategy
  fConflictStrategy=cr_newer_wins;
  fSlowSyncStrategy=cr_newer_wins;
  fFirstTimeStrategy=cr_newer_wins;
  // options
  fLocalDBTypeID=0;
  fReadOnly=false;
  fCanRestart=false;
  fReportUpdates=true;
  fDeleteWins=false; // replace wins over delete by default
  fResendFailing=true; // resend failing items in next session by default
  #ifdef SYSYNC_SERVER
  fTryUpdateDeleted=false; // no attempt to update already deleted items (assuming they are invisible only)
  fAlwaysSendLocalID=false; // off as it used to be not SCTS conformant (but would give clients chances to remap IDs)
  #endif
  fMaxItemsPerMessage=0; // no limit
  #ifdef OBJECT_FILTERING
  // - filters
  fRemoteAcceptFilter.erase();
  fSilentlyDiscardUnaccepted=false;
  fLocalDBFilterConf.erase();
  fMakePassFilter.erase();
  fInvisibleFilter.erase();
  fMakeVisibleFilter.erase();
  // - DS 1.2 Filter support (<filter> allowed in Alert, <filter-rx>/<filterCap> shown in devInf)
  fDS12FilterSupport=false; // off by default, as clients usually don't have it
  // - Set if date range support is available in this datastore
  fDateRangeSupported=false;
  #endif
  #ifdef SCRIPT_SUPPORT
  fDBInitScript.erase();
  fSentItemStatusScript.erase();
  fReceivedItemStatusScript.erase();
  fAlertScript.erase();
  #ifdef SYSYNC_CLIENT
  fAlertPrepScript.erase();
  #endif
  fDBFinishScript.erase();
  #endif
  // clear embedded
  fTypeSupport.clear();
  // clear inherited
  inherited::clear();
} // TLocalDSConfig::clear


#ifndef HARDCODED_CONFIG

// config element parsing
bool TLocalDSConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"dbtypeid")==0)
    expectUInt32(fLocalDBTypeID);
  else if (strucmp(aElementName,"typesupport")==0)
    expectChildParsing(fTypeSupport);
  else if (strucmp(aElementName,"conflictstrategy")==0)
    expectEnum(sizeof(fConflictStrategy),&fConflictStrategy,conflictStrategyNames,numConflictStrategies);
  else if (strucmp(aElementName,"slowsyncstrategy")==0)
    expectEnum(sizeof(fSlowSyncStrategy),&fSlowSyncStrategy,conflictStrategyNames,numConflictStrategies);
  else if (strucmp(aElementName,"firsttimestrategy")==0)
    expectEnum(sizeof(fFirstTimeStrategy),&fFirstTimeStrategy,conflictStrategyNames,numConflictStrategies);
  else if (strucmp(aElementName,"readonly")==0)
    expectBool(fReadOnly);
  else if (strucmp(aElementName,"canrestart")==0)
    expectBool(fCanRestart);
  else if (strucmp(aElementName,"syncmode")==0) {
    if (!fSyncModeBuffer.empty()) {
      fSyncModes.insert(fSyncModeBuffer);
      fSyncModeBuffer.clear();
    }
    expectString(fSyncModeBuffer);
  } else if (strucmp(aElementName,"reportupdates")==0)
    expectBool(fReportUpdates);
  else if (strucmp(aElementName,"deletewins")==0)
    expectBool(fDeleteWins);
  else if (strucmp(aElementName,"resendfailing")==0)
    expectBool(fResendFailing);
  #ifdef SYSYNC_SERVER
  else if (strucmp(aElementName,"tryupdatedeleted")==0)
    expectBool(fTryUpdateDeleted);
  else if (strucmp(aElementName,"alwayssendlocalid")==0)
    expectBool(fAlwaysSendLocalID);
  else if (strucmp(aElementName,"alias")==0) {
    // get a name
    string name;
    if (!getAttrExpanded(aAttributes, "name", name, true))
      return fail("Missing 'name' attribute in 'alias'");
    fAliasNames.push_back(name);
    expectEmpty();
  }
  #endif
  else if (strucmp(aElementName,"maxitemspermessage")==0)
    expectUInt32(fMaxItemsPerMessage);
  #ifdef OBJECT_FILTERING
  // filtering
  else if (strucmp(aElementName,"acceptfilter")==0)
    expectString(fRemoteAcceptFilter);
  else if (strucmp(aElementName,"silentdiscard")==0)
    expectBool(fSilentlyDiscardUnaccepted);
  else if (strucmp(aElementName,"localdbfilter")==0)
    expectString(fLocalDBFilterConf);
  else if (strucmp(aElementName,"makepassfilter")==0)
    expectString(fMakePassFilter);
  else if (strucmp(aElementName,"invisiblefilter")==0)
    expectString(fInvisibleFilter);
  else if (strucmp(aElementName,"makevisiblefilter")==0)
    expectString(fMakeVisibleFilter);
  else if (strucmp(aElementName,"ds12filters")==0)
    expectBool(fDS12FilterSupport);
  else if (strucmp(aElementName,"daterangesupport")==0)
    expectBool(fDateRangeSupported);
  #endif
  #ifdef SCRIPT_SUPPORT
  else if (strucmp(aElementName,"datastoreinitscript")==0)
    expectScript(fDBInitScript,aLine,&DBFuncTable);
  else if (strucmp(aElementName,"sentitemstatusscript")==0)
    expectScript(fSentItemStatusScript,aLine,&ErrorFuncTable);
  else if (strucmp(aElementName,"receiveditemstatusscript")==0)
    expectScript(fReceivedItemStatusScript,aLine,&ErrorFuncTable);
  else if (strucmp(aElementName,"alertscript")==0)
    expectScript(fAlertScript,aLine,&DBFuncTable);
  #ifdef SYSYNC_CLIENT
  else if (strucmp(aElementName,"alertprepscript")==0)
    expectScript(fAlertPrepScript,aLine,getClientDBFuncTable());
  #endif
  else if (strucmp(aElementName,"datastorefinishscript")==0)
    expectScript(fDBFinishScript,aLine,&DBFuncTable);
  #endif
  #ifndef MINIMAL_CODE
  else if (strucmp(aElementName,"displayname")==0)
    expectString(fDisplayName);
  #endif
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TLocalDSConfig::localStartElement

#endif

// resolve
void TLocalDSConfig::localResolve(bool aLastPass)
{
  if (!fSyncModeBuffer.empty()) {
    fSyncModes.insert(fSyncModeBuffer);
    fSyncModeBuffer.clear();
  }

  if (aLastPass) {
    #ifdef SCRIPT_SUPPORT
    TScriptContext *sccP = NULL;
    SYSYNC_TRY {
      // resolve all scripts in same context
      // - first script needed (when alert is created)
      #ifdef SYSYNC_CLIENT
      TScriptContext::resolveScript(getSyncAppBase(),fAlertPrepScript,sccP,NULL);
      #endif
      // - scripts needed when database is made ready
      TScriptContext::resolveScript(getSyncAppBase(),fDBInitScript,sccP,NULL);
      TScriptContext::resolveScript(getSyncAppBase(),fSentItemStatusScript,sccP,NULL);
      TScriptContext::resolveScript(getSyncAppBase(),fReceivedItemStatusScript,sccP,NULL);
      TScriptContext::resolveScript(getSyncAppBase(),fAlertScript,sccP,NULL);
      TScriptContext::resolveScript(getSyncAppBase(),fDBFinishScript,sccP,NULL);
      // - forget this context
      if (sccP) delete sccP;
    }
    SYSYNC_CATCH (...)
      if (sccP) delete sccP;
      SYSYNC_RETHROW;
    SYSYNC_ENDCATCH
    #endif
  }
  // resolve embedded
  fTypeSupport.Resolve(aLastPass);
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TLocalDSConfig::localResolve


// - add type support to datatstore from config
void TLocalDSConfig::addTypes(TLocalEngineDS *aDatastore, TSyncSession *aSessionP)
{
  TSyncItemType *typeP;
  TSyncItemType *writetypeP;

  // preferred types, create instances only if not already existing
  // - preferred receive
  typeP = aSessionP->findLocalType(fTypeSupport.fPreferredRx);
  if (!typeP) {
    typeP = fTypeSupport.fPreferredRx->newSyncItemType(aSessionP,NULL); // local types are never exclusively related to a datastore
    aSessionP->addLocalItemType(typeP); // add to session
  }
  // - preferred send
  writetypeP = aSessionP->findLocalType(fTypeSupport.fPreferredTx);
  if (!writetypeP) {
    writetypeP = fTypeSupport.fPreferredTx->newSyncItemType(aSessionP,NULL); // local types are never exclusively related to a datastore
    aSessionP->addLocalItemType(writetypeP);
  }
  // - set preferred types
  aDatastore->setPreferredTypes(typeP,writetypeP);
  // additional types
  TAdditionalTypesList::iterator pos;
  for (pos=fTypeSupport.fAdditionalTypes.begin(); pos!=fTypeSupport.fAdditionalTypes.end(); pos++) {
    // - search for type already created from same config item
    typeP = aSessionP->findLocalType((*pos).datatypeconfig);
    if (!typeP) {
      // - does not exist yet, create the type
      typeP = (*pos).datatypeconfig->newSyncItemType(aSessionP,NULL); // local types are never exclusively related to a datastore
      // - add type to session types
      aSessionP->addLocalItemType(typeP);
    }
    // - add type to datastore's supported types
    aDatastore->addTypeSupport(typeP,(*pos).forRead,(*pos).forWrite);
  }
  #ifndef NO_REMOTE_RULES
  // rulematch types
  for (pos=fTypeSupport.fRuleMatchTypes.begin(); pos!=fTypeSupport.fRuleMatchTypes.end(); pos++) {
    // - search for type already created from same config item
    typeP = aSessionP->findLocalType((*pos).datatypeconfig);
    if (!typeP) {
      // - does not exist yet, create the type
      typeP = (*pos).datatypeconfig->newSyncItemType(aSessionP,NULL); // local types are never exclusively related to a datastore
      // - add type to session types
      aSessionP->addLocalItemType(typeP);
    }
    // - add type to datastore's rulematch types
    aDatastore->addRuleMatchTypeSupport(typeP,(*pos).remoteRuleMatch.c_str());
  }
  #endif
  // now apply type limits
  // Note: this is usually derived, as limits are often defined within the datastore,
  //       not the type itself (however, for hardcoded template-based fieldlists,
  //       the limits are taken from the template, see  TLocalDSConfig::addTypeLimits()
  addTypeLimits(aDatastore, aSessionP);
} // TLocalDSConfig::addTypes


// Add (probably datastore-specific) limits such as MaxSize and NoTruncate to types
void TLocalDSConfig::addTypeLimits(TLocalEngineDS *aLocalDatastoreP, TSyncSession *aSessionP)
{
  // add field size limitations from map to all types
  TSyncItemTypePContainer::iterator pos;
  TSyncItemTypePContainer *typesP = &(aLocalDatastoreP->fRxItemTypes);
  for (uInt8 i=0; i<2; i++) {
    for (pos=typesP->begin(); pos!=typesP->end(); pos++) {
      // apply default limits to type (e.g. from hard-coded template in config)
      (*pos)->addDefaultTypeLimits();
    }
    typesP = &(aLocalDatastoreP->fTxItemTypes);
  }
} // TLocalDSConfig::addTypeLimits


// Check for alias names
uInt16 TLocalDSConfig::isDatastoreAlias(cAppCharP aDatastoreURI)
{
  // only servers have (and may need) aliases
  #ifdef SYSYNC_SERVER
  for (TStringList::iterator pos = fAliasNames.begin(); pos!=fAliasNames.end(); pos++) {
    if (*pos == aDatastoreURI)
      return (*pos).size(); // return size of match
  }
  #endif
  return 0;
} // TLocalDSConfig::isDatastoreAlias




/*
 * Implementation of TLocalEngineDS
 */


/// @Note InternalResetDataStore() must also be callable from destructor
///  (care not to call other objects which will refer to the already
///  half-destructed datastore!)
void TLocalEngineDS::InternalResetDataStore(void)
{
  // possibly complete, if not already done (should be, by engFinishDataStoreSync() !)
  if (fLocalDSState>dssta_idle)
    changeState(dssta_completed); // complete NOW, opportunity to show stats, etc.
  // switch down to idle
  changeState(dssta_idle);
  /// @todo obsolete: fState=dss_idle;
  fAbortStatusCode=LOCERR_OK; // not aborted yet
  fLocalAbortCause=true; // assume local cause
  fRemoteAddingStopped=false;
  fAlertCode=0; // not yet alerted

  /// Init Sync mode @ref dsSyncMode
  fSyncMode=smo_twoway; // default to twoway
  fForceSlowSync=false;
  fSlowSync=false;
  fRefreshOnly=false;
  fReadOnly=false;
  fReportUpdates=fDSConfigP->fReportUpdates; // update reporting according to what is configured
  fCanRestart=fDSConfigP->fCanRestart;
  fSyncModes=fDSConfigP->fSyncModes;
  fServerAlerted=false;
  fResuming=false;
  #ifdef SUPERDATASTORES
  fAsSubDatastoreOf=NULL; // is not a subdatastore
  #endif


  /// Init administrative data to defaults @ref dsAdminData
  // - last
  fLastRemoteAnchor.erase();
  fLastLocalAnchor.erase();
  // - current
  fNextRemoteAnchor.erase();
  fNextLocalAnchor.erase();
  // suspend state
  fResumeAlertCode=0; // none
  fPreventResuming=false;
  // suspend of chunked items
  fPartialItemState=pi_state_none;
  fLastSourceURI.erase();
  fLastTargetURI.erase();
  fLastItemStatus=0;
  fPITotalSize=0;
  fPIStoredSize=0;
  fPIUnconfirmedSize=0;
  if (fPIStoredDataAllocated) {
    smlLibFree(fPIStoredDataP);
    fPIStoredDataAllocated=false;
  }
  fPIStoredDataP=NULL;

  // other state info
  fFirstTimeSync=false; // not first sync by default

  #ifdef SYSYNC_CLIENT
  // - maps for add commands
  fPendingAddMaps.clear();
  fUnconfirmedMaps.clear();
  fLastSessionMaps.clear();
  #endif
  #ifdef SYSYNC_SERVER
  PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
    "fTempGUIDMap: removing %ld items", (long)fTempGUIDMap.size()
  ));
  fTempGUIDMap.clear();
  #endif

  /// Init type negotiation
  /// - for sending data
  fLocalSendToRemoteTypeP = NULL;
  fRemoteReceiveFromLocalTypeP = NULL;
  /// - for receiving data
  fLocalReceiveFromRemoteTypeP = NULL;
  fRemoteSendToLocalTypeP = NULL;

  /// Init Filtering @ref dsFiltering
  resetFiltering();

  /// Init item processing @ref dsItemProcessing
  fSessionConflictStrategy=cr_duplicate; // will be updated later when sync mode is known
  fItemSizeLimit=-1; // no limit yet
  fCurrentSyncOp = sop_none; // will be set at engProcessItem()
  fEchoItemOp = sop_none; // will be set at engProcessItem()
  fItemConflictStrategy=fSessionConflictStrategy; // will be set at engProcessItem()
  fForceConflict = false; // will be set at engProcessItem()
  fDeleteWins = false; // will be set at engProcessItem()
  fRejectStatus = -1; // will be set at engProcessItem()
  #ifdef SCRIPT_SUPPORT
  // - delete the script context if any
  if (fSendingTypeScriptContextP) {
    delete fSendingTypeScriptContextP;
    fSendingTypeScriptContextP=NULL;
  }
  if (fReceivingTypeScriptContextP) {
    delete fReceivingTypeScriptContextP;
    fReceivingTypeScriptContextP=NULL;
  }
  if (fDataStoreScriptContextP) {
    delete fDataStoreScriptContextP;
    fDataStoreScriptContextP=NULL;
  }
  #endif

  /// Init other vars @ref dsOther


  /// Init Counters and statistics @ref dsCountStats
  // - NOC from remote
  fRemoteNumberOfChanges=-1; // none known yet
  // - data transferred
  fIncomingDataBytes=0;
  fOutgoingDataBytes=0;
  // - locally performed ops
  fLocalItemsAdded=0;
  fLocalItemsUpdated=0;
  fLocalItemsDeleted=0;
  fLocalItemsError=0;
  // - remotely performed ops
  fRemoteItemsAdded=0;
  fRemoteItemsUpdated=0;
  fRemoteItemsDeleted=0;
  fRemoteItemsError=0;
  #ifdef SYSYNC_SERVER
  // - conflicts
  fConflictsServerWins=0;
  fConflictsClientWins=0;
  fConflictsDuplicated=0;
  // - slow sync matches
  fSlowSyncMatches=0;
  #endif

  // Override defaults from ancestor
  // - generally, limit GUID size to reasonable size (even if we can
  //   theoretically handle unlimited GUIDs in client, except for
  //   some DEBUGPRINTF statements that will crash for example with
  //   the brain-damaged GUIDs that Exchange server uses.
  fMaxGUIDSize = 64;
} // TLocalEngineDS::InternalResetDataStore


/// constructor
TLocalEngineDS::TLocalEngineDS(TLocalDSConfig *aDSConfigP, TSyncSession *aSessionP, const char *aName, uInt32 aCommonSyncCapMask) :
  TSyncDataStore(aSessionP, aName, aCommonSyncCapMask)
  ,fPIStoredDataP(NULL)
  ,fPIStoredDataAllocated(false)
  #ifdef SCRIPT_SUPPORT
  ,fSendingTypeScriptContextP(NULL) // no associated script context
  ,fReceivingTypeScriptContextP(NULL) // no associated script context
  ,fDataStoreScriptContextP(NULL) // no datastore level context
  #endif
  ,fRemoteDatastoreP(NULL) // no associated remote
{
  // set config ptr
  fDSConfigP = aDSConfigP;
  if (!fDSConfigP)
    SYSYNC_THROW(TSyncException(DEBUGTEXT("TLocalEngineDS::TLocalEngineDS called with NULL config","lds1")));
  /// Init Sync state @ref dsSyncState
  fLocalDSState=dssta_idle; // idle to begin with
  // now reset
  InternalResetDataStore();
} // TLocalEngineDS::TLocalEngineDS




TLocalEngineDS::~TLocalEngineDS()
{
  // reset everything
  InternalResetDataStore();
} // TLocalEngineDS::~TLocalEngineDS


#ifdef SYDEBUG

// return datastore state name
cAppCharP TLocalEngineDS::getDSStateName(void)
{
  return LocalDSStateNames[fLocalDSState];
} // TLocalEngineDS::getDSStateName


// return datastore state name
cAppCharP TLocalEngineDS::getDSStateName(TLocalEngineDSState aState)
{
  return LocalDSStateNames[aState];
} // TLocalEngineDS::getDSStateName

#endif

// reset datastore (after use)
void TLocalEngineDS::engResetDataStore(void)
{
  // now reset
  // - logic layer and above
  dsResetDataStore();
  // - myself
  InternalResetDataStore();
  // - anchestors
  inherited::engResetDataStore();
} // TLocalEngineDS::engResetDataStore



// check if this datastore is accessible with given URI
// NOTE: By default, local datastore type is addressed with
//       first path element of URI, rest of path might be used
//       by derivates to subselect data folders etc.
uInt16 TLocalEngineDS::isDatastore(const char *aDatastoreURI)
{
  // extract base name
  string basename;
  analyzeName(aDatastoreURI,&basename);
  // compare only base name
  // - compare with main name
  int res = inherited::isDatastore(basename.c_str());
  if (res==0) {
    // Not main name: compare with aliases
    res = fDSConfigP->isDatastoreAlias(basename.c_str());
  }
  return res;
} // TLocalEngineDS::isDatastore



/// get DB specific error message text for dbg log, or empty string if none
/// @return platform specific DB error text
string TLocalEngineDS::lastDBErrorText(void)
{
  string s;
  s.erase();
  uInt32 err = lastDBError();
  if (isDBError(err)) {
    StringObjPrintf(s," (DB specific error code = %ld)",(long)lastDBError());
  }
  return s;
} // TLocalEngineDS::lastDBErrorText


#ifdef SYSYNC_CLIENT

// - init Sync Parameters (client case)
//   Derivates might override this to pre-process and modify parameters
//   (such as adding client settings as CGI to remoteDBPath)
bool TLocalEngineDS::dsSetClientSyncParams(
  TSyncModes aSyncMode,
  bool aSlowSync,
  const char *aRemoteDBPath,
  const char *aDBUser,
  const char *aDBPassword,
  const char *aLocalPathExtension,
  const char *aRecordFilterQuery,
  bool aFilterInclusive
)
{
  // - set remote params
  fRemoteDBPath=aRemoteDBPath;
  AssignString(fDBUser,aDBUser);
  AssignString(fDBPassword,aDBPassword);
  // check for running under control of a superdatastore
  // - aRemoteDBPath might contain a special prefix: "<super>remote", with "super" specifying the
  //   name of a local superdatastore to run the sync with
  string opts;
  if (!fRemoteDBPath.empty() && fRemoteDBPath.at(0)=='<') {
    // we have an option prefix
    size_t pfxe = fRemoteDBPath.find('>', 1);
    if (pfxe!=string::npos) {
      // extract options
      opts.assign(fRemoteDBPath, 1, pfxe-1);
      // store remote path cleaned from options
      fRemoteDBPath.erase(0,pfxe+1);
    }
  }
  if (!opts.empty()) {
    #ifdef SUPERDATASTORES
    // For now, the only option withing angle brackets is the name of the superdatastore, so opts==superdatastorename
    // - look for superdatastore having the specified name
    TSuperDSConfig *superdscfgP = static_cast<TSuperDSConfig *>(getSession()->getSessionConfig()->getLocalDS(opts.c_str()));
    if (superdscfgP && superdscfgP->isAbstractDatastore()) {
      // see if we have an instance of this already
      fAsSubDatastoreOf = static_cast<TSuperDataStore *>(getSession()->findLocalDataStore(superdscfgP));
      if (fAsSubDatastoreOf) {
        // that superdatastore already exists, just override client sync params with those already set
        aSyncMode = fAsSubDatastoreOf->fSyncMode;
        aSlowSync = fAsSubDatastoreOf->fSlowSync;
        aRecordFilterQuery = fAsSubDatastoreOf->fRemoteRecordFilterQuery.c_str();
      }
      else {
        // instantiate new superdatastore
        fAsSubDatastoreOf = static_cast<TSuperDataStore *>(superdscfgP->newLocalDataStore(getSession()));
        if (fAsSubDatastoreOf) {
          fSessionP->fLocalDataStores.push_back(fAsSubDatastoreOf);
          // configure it with the same parameters as the subdatastore
          if (!fAsSubDatastoreOf->dsSetClientSyncParams(
            aSyncMode,
            aSlowSync,
            fRemoteDBPath.c_str(), // already cleaned from <xxx> prefix
            aDBUser,
            aDBPassword,
            aLocalPathExtension,
            aRecordFilterQuery,
            aFilterInclusive
          ))
            return false; // failed
        }
      }
      if (fAsSubDatastoreOf) {
        // find link config for this superdatastore
        TSubDSLinkConfig *lcfgP = NULL;
        TSubDSConfigList::iterator pos;
        for(pos=superdscfgP->fSubDatastores.begin();pos!=superdscfgP->fSubDatastores.end();pos++) {
          if ((*pos)->fLinkedDSConfigP==fDSConfigP) {
            // this is the link
            lcfgP = *pos;
            break;
          }
        }
        if (lcfgP) {
          // now link into superdatastore
          fAsSubDatastoreOf->addSubDatastoreLink(lcfgP,this);
        }
        else {
          PDEBUGPRINTFX(DBG_ERROR,("Warning: '%s' is not a subdatastore of '%s'", getName(), opts.c_str()));
          return false; // failed
        }
      }
    }
    else {
      PDEBUGPRINTFX(DBG_ERROR,("Warning: No superdatastore name '%s' exists -> can't run '%s' under superdatastore control", opts.c_str(), getName()));
      return false; // failed
    }
    #endif // SUPERDATASTORES
  }
  // sync mode
  fSyncMode=aSyncMode;
  fSlowSync=aSlowSync;
  fRefreshOnly=fSyncMode==smo_fromserver; // here to make sure, should be set by setSyncMode(FromAlertCode) later anyway
  // DS 1.2 filters
  AssignString(fRemoteRecordFilterQuery,aRecordFilterQuery);
  fRemoteFilterInclusive=aFilterInclusive;
  // params
  // - build local path
  fLocalDBPath=URI_RELPREFIX;
  fLocalDBPath+=getName();
  if (aLocalPathExtension && *aLocalPathExtension) {
    fLocalDBPath+='/';
    fLocalDBPath+=aLocalPathExtension;
  }
  // - we have the params for syncing now
  return changeState(dssta_clientparamset)==LOCERR_OK;
} // TLocalEngineDS::dsSetClientSyncParams

#endif // SYSYNC_CLIENT



// add support for more data types
// (for programatically creating local datastores from specialized TSyncSession derivates)
void TLocalEngineDS::addTypeSupport(TSyncItemType *aItemTypeP,bool aForRx, bool aForTx)
{
  if (aForRx) fRxItemTypes.push_back(aItemTypeP);
  if (aForTx) fTxItemTypes.push_back(aItemTypeP);
} // TLocalEngineDS::addTypeSupport


#ifndef NO_REMOTE_RULES
// add data type that overrides normal type selection if string matches active remote rule
void TLocalEngineDS::addRuleMatchTypeSupport(TSyncItemType *aItemTypeP,cAppCharP aRuleMatchString)
{
  TRuleMatchTypeEntry rme;
  rme.itemTypeP = aItemTypeP;
  rme.ruleMatchString = aRuleMatchString;
  fRuleMatchItemTypes.push_back(rme);
} // TLocalEngineDS::addRuleMatchTypeSupport
#endif


TTypeVariantDescriptor TLocalEngineDS::getVariantDescForType(TSyncItemType *aItemTypeP)
{
  // search in config for specific variant descriptor
  // - first check preferred rx
  if (fDSConfigP->fTypeSupport.fPreferredRx == aItemTypeP->getTypeConfig())
    return fDSConfigP->fTypeSupport.fPrefRxVariantDescP;
  // - then check preferred tx
  if (fDSConfigP->fTypeSupport.fPreferredTx == aItemTypeP->getTypeConfig())
    return fDSConfigP->fTypeSupport.fPrefTxVariantDescP;
  // - then check additional types
  TAdditionalTypesList::iterator pos;
  for (pos=fDSConfigP->fTypeSupport.fAdditionalTypes.begin(); pos!=fDSConfigP->fTypeSupport.fAdditionalTypes.end(); pos++) {
    if ((*pos).datatypeconfig == aItemTypeP->getTypeConfig())
      return (*pos).variantDescP;
  }
  // none found
  return NULL;
} // TLocalEngineDS::getVariantDescForType




// - called when a item in the sync set changes its localID (due to local DB internals)
void TLocalEngineDS::dsLocalIdHasChanged(const char *aOldID, const char *aNewID)
{
  #ifdef SYSYNC_SERVER
  if (IS_SERVER) {
    // make sure remapped localIDs get updated as well
    TStringToStringMap::iterator pos;
    for (pos=fTempGUIDMap.begin(); pos!=fTempGUIDMap.end(); pos++) {
      if (pos->second == aOldID) {
        // update ID
        PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
          "fTempGUIDMap: updating mapping of %s from %s to %s",
          pos->first.c_str(),
          aOldID,
          aNewID
        ));
        pos->second = aNewID;
        break;
      }
    }
  }
  #endif // SYSYNC_SERVER
} // TLocalEngineDS::dsLocalIdHasChanged


#ifdef SYSYNC_SERVER

// for received GUIDs (Map command), obtain real GUID (might be temp GUID due to maxguidsize restrictions)
void TLocalEngineDS::obtainRealLocalID(string &aLocalID)
{
  if (aLocalID.size()>0 && aLocalID[0]=='#') {
    // seems to be a temp GUID
    TStringToStringMap::iterator pos =
      fTempGUIDMap.find(aLocalID);
    if (pos!=fTempGUIDMap.end()) {
      // found temp GUID mapping, replace it
      PDEBUGPRINTFX(DBG_DATA,(
        "translated tempLocalID='%s' back to real localID='%s'",
        aLocalID.c_str(),
        (*pos).second.c_str()
      ));
      aLocalID = (*pos).second;
    }
    else {
      PDEBUGPRINTFX(DBG_ERROR,("No realLocalID found for tempLocalID='%s'",aLocalID.c_str()));
    }
  }
} // TLocalEngineDS::obtainRealLocalID


// for sending GUIDs (Add command), generate temp GUID which conforms to maxguidsize of remote datastore if needed
void TLocalEngineDS::adjustLocalIDforSize(string &aLocalID, sInt32 maxguidsize, sInt32 prefixsize)
{
  if (maxguidsize>0) {
    if (aLocalID.length()+prefixsize>(uInt32)maxguidsize) { //BCPPB needed unsigned cast
      // real GUID is too long, we need to create a temp
      #if SYDEBUG>1
      // first check if there is already a mapping for it,
      // because on-disk storage can only hold one; also
      // saves space
      // TODO: implement this more efficiently than this O(N) search
      for (TStringToStringMap::const_iterator it = fTempGUIDMap.begin();
           it != fTempGUIDMap.end();
           ++it) {
        if (it->second == aLocalID) {
          // found an existing mapping!
          PDEBUGPRINTFX(DBG_ERROR,(
            "fTempGUIDMap: translated realLocalID='%s' to tempLocalID='%s' (reused?!)",
            aLocalID.c_str(),
            it->first.c_str()
          ));
          aLocalID = it->first;
          return;
        }
      }
      string tempguid;
      long counter = fTempGUIDMap.size(); // as list only grows, we have unique tempuids for sure
      while (true) {
        counter++;
        StringObjPrintf(tempguid,"#%ld",counter);
        if (fTempGUIDMap.find(tempguid) != fTempGUIDMap.end()) {
          PDEBUGPRINTFX(DBG_ERROR,(
            "fTempGUIDMap: '%s' not new?!",
            tempguid.c_str()
          ));
        } else {
          break;
        }
      }
      #else
      // rely on tempguid list only growing (which still holds true)
      string tempguid;
      StringObjPrintf(tempguid,"#%ld",(long)fTempGUIDMap.size()+1); // as list only grows, we have unique tempuids for sure
      #endif
      fTempGUIDMap[tempguid]=aLocalID;
      PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
        "fTempGUIDMap: translated realLocalID='%s' to tempLocalID='%s'",
        aLocalID.c_str(),
        tempguid.c_str()
      ));
      aLocalID=tempguid;
    }
  }
} // TLocalEngineDS::adjustLocalIDforSize

#endif // SYSYNC_SERVER


// set Sync types needed for sending local data to remote DB
void TLocalEngineDS::setSendTypeInfo(
  TSyncItemType *aLocalSendToRemoteTypeP,
  TSyncItemType *aRemoteReceiveFromLocalTypeP
)
{
  fLocalSendToRemoteTypeP=aLocalSendToRemoteTypeP;
  fRemoteReceiveFromLocalTypeP=aRemoteReceiveFromLocalTypeP;
} // TLocalEngineDS::setSendTypeInfo


// set Sync types needed for receiving remote data in local DB
void TLocalEngineDS::setReceiveTypeInfo(
  TSyncItemType *aLocalReceiveFromRemoteTypeP,
  TSyncItemType *aRemoteSendToLocalTypeP
)
{
  fLocalReceiveFromRemoteTypeP=aLocalReceiveFromRemoteTypeP;
  fRemoteSendToLocalTypeP=aRemoteSendToLocalTypeP;
} // TLocalEngineDS::setReceiveTypeInfo


// - init usage of datatypes set with setSendTypeInfo/setReceiveTypeInfo
localstatus TLocalEngineDS::initDataTypeUse(void)
{
  localstatus sta = LOCERR_OK;

  // check compatibility
  if (
    !fLocalSendToRemoteTypeP || !fLocalReceiveFromRemoteTypeP ||
    !fLocalSendToRemoteTypeP->isCompatibleWith(fLocalReceiveFromRemoteTypeP)
  ) {
    // send and receive types not compatible
    sta = 415;
    PDEBUGPRINTFX(DBG_ERROR,("Incompatible send and receive types -> cannot sync (415)"));
    engAbortDataStoreSync(sta,true,false); // do not proceed with sync of this datastore, local problem, not resumable
    return sta;
  }
  #ifdef SCRIPT_SUPPORT
  // let types initialize themselves for being used by this datastore
  // - optimization: if both types are same, initialize only once
  if (fLocalSendToRemoteTypeP == fLocalReceiveFromRemoteTypeP)
    fLocalReceiveFromRemoteTypeP->initDataTypeUse(this,true,true); // send and receive
  else {
    fLocalSendToRemoteTypeP->initDataTypeUse(this,true,false); // for sending, not receiving
    fLocalReceiveFromRemoteTypeP->initDataTypeUse(this,false,true); // not sending, for receiving
  }
  #endif
  // ok
  return sta;
} // TLocalEngineDS::initDataTypeUse



// conflict resolution strategy. Conservative here
TConflictResolution TLocalEngineDS::getConflictStrategy(bool aForSlowSync, bool aForFirstTime)
{
  return aForSlowSync ?
    (aForFirstTime ? fDSConfigP->fFirstTimeStrategy : fDSConfigP->fSlowSyncStrategy) :
    fDSConfigP->fConflictStrategy;
} // TLocalEngineDS::getConflictStrategy



// add filter keywords and property names to filterCap
void TLocalEngineDS::addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps)
{
  #ifdef OBJECT_FILTERING
  // add my own properties
  if (fDSConfigP->fDateRangeSupported) {
    addPCDataStringToList("BEFORE", &aFilterKeywords);
    addPCDataStringToList("SINCE", &aFilterKeywords);
  }
  // get default send type
  TSyncItemType *itemTypeP = fSessionP->findLocalType(fDSConfigP->fTypeSupport.fPreferredTx);
  TTypeVariantDescriptor variantDesc = NULL;
  doesUseType(itemTypeP, &variantDesc);
  // have it add it's keywords and properties
  itemTypeP->addFilterCapPropsAndKeywords(aFilterKeywords,aFilterProps,variantDesc);
  #endif
} // TLocalEngineDS::addFilterCapPropsAndKeywords





// reset all filter settings
void TLocalEngineDS::resetFiltering(void)
{
  #ifdef OBJECT_FILTERING
  // - dynamic sync set filter
  fSyncSetFilter.erase();
  // - static filter
  fLocalDBFilter=fDSConfigP->fLocalDBFilterConf; // use configured localDB filter
  // - TAF filters
  #ifdef SYNCML_TAF_SUPPORT
  fTargetAddressFilter.erase(); // none from <sync> yet
  fIntTargetAddressFilter.erase(); // none from internal source (script)
  #endif
  #ifdef SYSYNC_TARGET_OPTIONS
  // - Other filtering options
  fDateRangeStart=0; // no date range
  fDateRangeEnd=0;
  fSizeLimit=-1; // no size limit
  fMaxItemCount=0; // no item count limit
  fNoAttachments=false; // attachments not suppressed
  fDBOptions.erase(); // no options
  #endif
  // - Filtering requirements
  fTypeFilteringNeeded=false;
  fFilteringNeededForAll=false;
  fFilteringNeeded=false;
  #endif // OBJECT_FILTERING
} // TLocalEngineDS::resetFiltering



#ifdef OBJECT_FILTERING

/// @brief check single filter term for DS 1.2 filterkeywords.
/// @return true if term still needs to be added to normal filter expression, false if term will be handled otherwise
bool TLocalEngineDS::checkFilterkeywordTerm(
  cAppCharP aIdent, bool aAssignToMakeTrue,
  cAppCharP aOp, bool aCaseInsensitive,
  cAppCharP aVal, bool aSpecialValue,
  TSyncItemType *aItemTypeP
)
{
  // show it to the datatype (if any)
  if (aItemTypeP) {
    if (!aItemTypeP->checkFilterkeywordTerm(aIdent, aAssignToMakeTrue, aOp, aCaseInsensitive, aVal, aSpecialValue))
      return false; // type fully handles it, no need to check it further or add it to the filter expression
  }
  // we generally implement BEFORE and SINCE on the datastore level
  // This might not make sense depending on the actual datatype, but does not harm either
  #ifdef SYSYNC_TARGET_OPTIONS
  timecontext_t tctx;

  if (strucmp(aIdent,"BEFORE")==0) {
    if (ISO8601StrToTimestamp(aVal,fDateRangeEnd,tctx)) {
      TzConvertTimestamp(fDateRangeEnd,tctx,TCTX_UTC,getSessionZones(),fSessionP->fUserTimeContext);
    }
    else {
      PDEBUGPRINTFX(DBG_ERROR,("invalid ISO datetime for BEFORE: '%s'",aVal));
      return true; // add it to filter, possibly this is not meant to be a filterkeyword
    }
  }
  else if (strucmp(aIdent,"SINCE")==0) {
    if (ISO8601StrToTimestamp(aVal,fDateRangeStart,tctx)) {
      TzConvertTimestamp(fDateRangeStart,tctx,TCTX_UTC,getSessionZones(),fSessionP->fUserTimeContext);
    }
    else {
      PDEBUGPRINTFX(DBG_ERROR,("invalid ISO datetime for SINCE: '%s'",aVal));
      return true; // add it to filter, possibly this is not meant to be a filterkeyword
    }
  }
  else if (strucmp(aIdent,"MAXSIZE")==0) {
    if (StrToFieldinteger(aVal,fSizeLimit)==0) {
      PDEBUGPRINTFX(DBG_ERROR,("invalid integer for MAXSIZE: '%s'",aVal));
      return true; // add it to filter, possibly this is not meant to be a filterkeyword
    }
  }
  else if (strucmp(aIdent,"MAXCOUNT")==0) {
    if (StrToULong(aVal,fMaxItemCount)==0) {
      PDEBUGPRINTFX(DBG_ERROR,("invalid integer for MAXSIZE: '%s'",aVal));
      return true; // add it to filter, possibly this is not meant to be a filterkeyword
    }
  }
  else if (strucmp(aIdent,"NOATT")==0) {
    if (!StrToBool(aVal,fNoAttachments)==0) {
      PDEBUGPRINTFX(DBG_ERROR,("invalid boolean for NOATT: '%s'",aVal));
      return true; // add it to filter, possibly this is not meant to be a filterkeyword
    }
  }
  else if (strucmp(aIdent,"DBOPTIONS")==0) {
    fDBOptions = aVal; // just get DB options
  }
  #endif
  else {
    // unknown identifier, add to filter expression
    return true;
  }
  // this term will be processed by special mechanism like fDateRangeStart/fDateRangeEnd
  // or fSizeLimit, so there is no need for normal filtering
  return false; // do not include into filter
} // TLocalEngineDS::checkFilterkeywordTerm


/// @brief parse "syncml:filtertype-cgi" filter, convert into internal filter syntax
///  and possibly sets some special filter options (fDateRangeStart, fDateRangeEnd)
///  based on "filterkeywords" available for the type passed (DS 1.2).
///  For parsing DS 1.1/1.0 TAF-style filters, aItemType can be NULL, no type-specific
///  filterkeywords can be parsed then.
/// @return pointer to next character after processing (usually points to terminator)
/// @param[in] aCGI the NUL-terminated filter string
/// @param[in] aItemTypeP if not NULL, this is the item type the filter applies to
const char *TLocalEngineDS::parseFilterCGI(cAppCharP aCGI, TSyncItemType *aItemTypeP, string &aFilter)
{
  const char *p=aCGI, *q;
  sInt16 paraNest=0; // nested paranthesis
  string ident;
  char op[3];
  char logop;
  string val;
  bool termtofilter;
  bool assigntomaketrue;
  bool specialvalue;
  bool caseinsensitive;

  PDEBUGPRINTFX(DBG_FILTER+DBG_EXOTIC,("Parsing syncml:filtertype-cgi filter: %s",aCGI));
  aFilter.erase();
  logop=0;
  while (p && *p) {
    if (aFilter.empty()) logop=0; // ignore logical operation that would be at beginning of an expression
    // skip spaces
    while (isspace(*p)) p++;
    // now we need an ident or paranthesis
    if (*p=='(') {
      if (logop) aFilter+=logop; // expression continues, we need the logop now
      logop=0; // now consumed
      paraNest++;
      aFilter+='(';
      p++;
    }
    else {
      // must be term: ident op val
      // - check special case pseudo-identifiers
      if (strucmp(p,"&LUID;",6)==0) {
        ident="LUID";
        p+=6;
      }
      else {
        // normal identifier
        // - search end
        q=p;
        while (isalnum(*q) || *q=='[' || *q==']' || *q=='.' || *q=='_') q++;
        // - assign
        if (q==p) {
          PDEBUGPRINTFX(DBG_ERROR,("Expected identifier but found '%s'",p));
          break;
        }
        ident.assign(p,q-p);
        p=q;
      }
      // skip spaces
      while (isspace(*p)) p++;
      // next must be comparison operator, possibly preceeded by modifiers
      op[0]=0; op[1]=0; op[2]=0;
      assigntomaketrue=false;
      specialvalue=false;
      caseinsensitive=false;
      // - check modifiers first
      if (*p==':') { assigntomaketrue=true; p++; }
      if (*p=='*') { specialvalue=true; p++; }
      if (*p=='^') { caseinsensitive=true; p++; }
      // - now OP either in internal form or as pseudo-entity
      if (*p=='>' || *p=='<') {
        // possible two-char ops (>=, <=, <>)
        op[0]=*p++;
        if (*p=='>' || *p=='=') {
          op[1]=*p++;
        }
      }
      else if (*p=='=' || *p=='%' || *p=='$') {
        // single char ops, just use them as is
        op[0]=*p++;
      }
      else if (*p=='&') {
        p++;
        if (tolower(*p)=='i') { caseinsensitive=true; p++; }
        if (strucmp(p,"eq;",3)==0) { op[0]='='; p+=3; }
        else if (strucmp(p,"gt;",3)==0) { op[0]='>'; p+=3; }
        else if (strucmp(p,"ge;",3)==0) { op[0]='>'; op[1]='='; p+=3; }
        else if (strucmp(p,"lt;",3)==0) { op[0]='<'; p+=3; }
        else if (strucmp(p,"le;",3)==0) { op[0]='<'; op[1]='='; p+=3; }
        else if (strucmp(p,"ne;",3)==0) { op[0]='<'; op[1]='>'; p+=3; }
        else if (strucmp(p,"con;",4)==0) { op[0]='%'; p+=4; }
        else if (strucmp(p,"ncon;",5)==0) { op[0]='$'; p+=5; }
        else {
          PDEBUGPRINTFX(DBG_ERROR,("Expected comparison operator pseudo-entity but found '%s'",p-1));
          break;
        }
      }
      else {
        PDEBUGPRINTFX(DBG_ERROR,("Expected comparison operator but found '%s'",p));
        break;
      }
      // next must be value
      // - check for special value cases
      if (strucmp(p,"&NULL;",6)==0) {
        // SyncML DS 1.2
        p+=6;
        val='E';
        specialvalue=true;
      }
      else if (strucmp(p,"&UNASSIGNED;",12)==0) {
        // Synthesis extension
        p+=12;
        val='U';
        specialvalue=true;
      }
      else {
        val.erase();
      }
      // - get value chars
      while (*p && *p!='&' && *p!='|' && *p!=')') {
        // value char, possibly hex escaped
        uInt16 c;
        if (*p=='%') {
          // convert from hex
          if (HexStrToUShort(p+1,c,2)==2) {
            p+=3;
          }
          else
            c=*p++;
        }
        else
          c=*p++;
        // add to value
        val += (char)c;
      } // value
      // now we have identifier, op and value
      // - check and possibly sort out filterkeyword terms
      termtofilter = checkFilterkeywordTerm(ident.c_str(),assigntomaketrue,op,caseinsensitive,val.c_str(),specialvalue,aItemTypeP);
      // - add to filter if not handled already by other mechanism
      if (termtofilter) {
        if (logop) aFilter+=logop; // if this is a continuation, add logical operator now
        aFilter += ident;
        if (assigntomaketrue) aFilter+=':';
        if (specialvalue) aFilter+='*';
        if (caseinsensitive) aFilter+='^';
        aFilter += op;
        aFilter += val;
      }
      else {
        PDEBUGPRINTFX(DBG_FILTER+DBG_EXOTIC,(
          "checkFilterkeywordTerm(%s,%hd,%s,%hd,%s,%hd) prevents adding to filter",
          ident.c_str(),(uInt16)assigntomaketrue,op,(uInt16)caseinsensitive,val.c_str(),(uInt16)specialvalue
        ));
        if (logop) {
          PDEBUGPRINTFX(DBG_FILTER,("Ignored logical operation '%c' due to always-ANDed filterkeyword",logop));
        }
      }
      // now check for continuation: optional closing paranthesis plus logical op
      // - closing paranthesis
      do {
        // skip spaces
        while (isspace(*p)) p++;
        if (*p!=')') break;
        if (paraNest==0) {
          // as we might parse filters as part of /fi() or /tf() options,
          // this is not an error but only means end of filter expression
          goto endFilter;
        }
        aFilter+=')';
        paraNest--;
        p++;
      } while (true);
      // - logical op
      if (*p==0)
        break; // done
      else if (*p=='&') {
        logop=*p++; // if no entity matches, & by itself is treated as AND
        if (strucmp(p,"amp;",4)==0) { logop='&'; p+=4; }
        else if (strucmp(p,"and;",4)==0) { logop='&'; p+=4; }
        else if (strucmp(p,"or;",3)==0) { logop='|'; p+=3; }
      }
      else if (*p=='|') {
        logop='|';
      }
      else {
        PDEBUGPRINTFX(DBG_ERROR,("Expected logical operator or end of filter but found '%s'",p));
        break;
      }
    } // not opening paranthesis
  } // while not end of filter
endFilter:
  PDEBUGPRINTFX(DBG_FILTER+DBG_EXOTIC,("Resulting internal filter: %s",aFilter.c_str()));
  // return pointer to terminating character
  return p;
} // TLocalEngineDS::parseFilterCGI


#endif


// analyze database name
void TLocalEngineDS::analyzeName(
  const char *aDatastoreURI,
  string *aBaseNameP,
  string *aTableNameP,
  string *aCGIP
)
{
  const char *p,*q=NULL, *r;
  r=strchr(aDatastoreURI,'?');
  p=strchr(aDatastoreURI,'/');
  if (r && p>r) p=NULL; // if slash is in CGI, ignore it
  else q=p+1; // slash exclusive
  if (p!=NULL) {
    // we have more than just the first element
    if (aBaseNameP) aBaseNameP->assign(aDatastoreURI,p-aDatastoreURI);
    // rest is table name and probably CGI
    if (aTableNameP) {
      if (r) aTableNameP->assign(q,r-q); // we have CGI
      else *aTableNameP=q; // entire rest is tablename
    }
  }
  else {
    // no second path element, but possibly CGI
    // - assign base name
    if (aBaseNameP) {
      if (r)
        (*aBaseNameP).assign(aDatastoreURI,r-aDatastoreURI); // only up to CGI
      else
        (*aBaseNameP)=aDatastoreURI; // complete name
    }
    // - there is no table name
    if (aTableNameP) aTableNameP->erase();
  }
  // return CGI (w/o question mark) if any
  if (aCGIP) {
    if (r) *aCGIP=r+1; else aCGIP->erase();
  }
} // TLocalEngineDS::analyzeName


#ifdef SYSYNC_TARGET_OPTIONS

// parses single option, returns pointer to terminating char of argument string
// or NULL on error
// Note: if aArguments is passed NULL, this is an option without arguments,
// and an arbitrary non-NULL will be returned if parsing is ok
const char *TLocalEngineDS::parseOption(
  const char *aOptName,
  const char *aArguments,
  bool aFromSyncCommand
)
{
  #ifdef OBJECT_FILTERING
  if (strucmp(aOptName,"fi")==0) {
    if (!aArguments) return NULL;
    // make sync set filter expression
    string f;
    aArguments=parseFilterCGI(aArguments,fLocalSendToRemoteTypeP,f); // if type being used for sending to remote is known here, use it
    if (!aFromSyncCommand) {
      addToFilter(f.c_str(),fSyncSetFilter,false); // AND chaining
      // call this once to give derivate a chance to see if it can filter the now set fSyncSetFilter
      engFilteredFetchesFromDB(true);
    }
    return aArguments; // end of filter pattern
  }
  #ifdef SYNCML_TAF_SUPPORT
  else if (strucmp(aOptName,"tf")==0) {
    if (!aArguments) return NULL;
    // make temporary filter (or TAF) expression
    aArguments=parseFilterCGI(aArguments,fLocalSendToRemoteTypeP,fTargetAddressFilter); // if type being used for sending to remote is known here, use it
    // Note: TAF filters are always evaluated internally as we need all SyncSet records
    //       regardless of possible TAF suppression (for slowsync matching etc.)
    return aArguments; // end of filter pattern
  }
  #endif
  else if (aArguments && strucmp(aOptName,"dr")==0) {
    // date range limit
    sInt16 dstart,dend;
    if (sscanf(aArguments,"%hd,%hd",&dstart,&dend)==2) {
      // - find end of arguments
      aArguments=strchr(aArguments,')');
      // - calculate start and end
      fDateRangeStart=getSystemNowAs(TCTX_UTC,getSessionZones());
      fDateRangeEnd=fDateRangeStart;
      // - now use offsets
      fDateRangeStart+=dstart*linearDateToTimeFactor;
      fDateRangeEnd+=dend*linearDateToTimeFactor;
      return aArguments;
    }
    else return NULL;
  }
  else if (aArguments && strucmp(aOptName,"li")==0) {
    // size limit
    sInt16 n=StrToFieldinteger(aArguments,fSizeLimit);
    if (n>0) {
      // - find end of arguments
      aArguments+=n;
      return aArguments;
    }
    else return NULL;
  }
  else
  if (!aArguments && strucmp(aOptName,"na")==0) {
    // no attachments
    fNoAttachments=true;
    return (const char *)1; // non-zero
  }
  else
  if (aArguments && strucmp(aOptName,"max")==0) {
    // maximum number of items (for email for example)
    sInt16 n=StrToULong(aArguments,fMaxItemCount);
    if (n>0) {
      // - find end of arguments
      aArguments+=n;
      return aArguments;
    }
    else return NULL;
  }
  else
  #endif
  #ifdef SYSYNC_SERVER
  if (IS_SERVER && !aArguments && strucmp(aOptName,"slow")==0) {
    // force a slow sync
    PDEBUGPRINTFX(DBG_HOT,("Slowsync forced by CGI-option in db path"));
    fForceSlowSync=true;
    return (const char *)1; // non-zero
  }
  else
  #endif // SYSYNC_SERVER
  if (aArguments && strucmp(aOptName,"o")==0) {
    // datastore options
    // - find end of arguments
    const char *p=strchr(aArguments,')');
    if (p) fDBOptions.assign(aArguments,p-aArguments);
    return p;
  }
  else
    return NULL; // not parsed
} // TLocalEngineDS::parseOption

#endif

// parse options
localstatus TLocalEngineDS::engParseOptions(
  const char *aTargetURIOptions,      // option string contained in target URI
  bool aFromSyncCommand               // must be set when parsing options from <sync> target URI
)
{
  localstatus sta=LOCERR_OK;
  if (aTargetURIOptions) {
    const char *p = aTargetURIOptions;
    #ifdef SYSYNC_TARGET_OPTIONS
    const char *q;
    #endif
    char c;
    string taf; // official TAF
    while ((c=*p)) {
      #ifdef SYSYNC_TARGET_OPTIONS
      if (c=='/') {
        // proprietary option lead-in
        // - get option name
        string optname;
        optname.erase();
        while(isalnum(c=*(++p)))
          optname+=c;
        // - get arguments
        if (c=='(') {
          q=p; // save
          p++; // skip "("
          p=parseOption(optname.c_str(),p,aFromSyncCommand);
          if (!p) {
            // unrecognized or badly formatted option, just add it to TAF
            taf+='/';
            taf+=optname;
            p=q; // restart after option name
            continue;
          }
          if (*p!=')') {
            sta=406;
            PDEBUGPRINTFX(DBG_ERROR,("Syntax error in target options"));
            break;
          }
        }
        else {
          // option without arguments
          if (!parseOption(optname.c_str(),NULL,aFromSyncCommand)) {
            sta=406;
            PDEBUGPRINTFX(DBG_ERROR,("Unknown target option"));
            break;
          } // error, not parsed
          p--; // will be incremented once again below
        }
      }
      else
      #endif
      {
        // char not part of an option
        taf+=c;
      }
      // next
      p++;
    }
    // check if we have TAF
    if (taf.size()>0) {
      #if defined(TAF_AS_SYNCSETFILTER) && defined(SYSYNC_TARGET_OPTIONS)
      // treat as "fi(<filterexpression>)" option like before 1.0.8.10
      if (!parseOption("fi",taf.c_str(),aFromSyncCommand)) { sta=406; } // error, not parsed
      #else
      #ifdef SYNCML_TAF_SUPPORT
      // treat as "tf(<filterexpression>)" = real TAF
      if (!parseOption("tf",taf.c_str(),aFromSyncCommand)) { sta=406; } // error, not parsed
      #else
      sta=406;
      PDEBUGPRINTFX(DBG_ERROR,("TAF not supported"));
      #endif
      #endif
    }
  }
  // return status
  return sta;
} //  TLocalEngineDS::engParseOptions


// process SyncML 1.2 style filter
localstatus TLocalEngineDS::engProcessDS12Filter(SmlFilterPtr_t aTargetFilter)
{
  localstatus sta=LOCERR_OK;

  if (aTargetFilter) {
    // check general availability
    #ifdef OBJECT_FILTERING
    if (!fDSConfigP->fDS12FilterSupport)
    #endif
    {
      PDEBUGPRINTFX(DBG_ERROR,("DS 1.2 style filtering is not available or disabled in config (<ds12filters>)"));
      sta=406;
      goto error;
    }
    // check filter
    TSyncItemType *itemTypeP=NULL; // no associated type so far
    bool inclusiveFilter=false; // default is EXCLUSIVE
    // - meta
    if (aTargetFilter->meta) {
      SmlMetInfMetInfPtr_t metaP = smlPCDataToMetInfP(aTargetFilter->meta);
      const char *typestr = smlMetaTypeToCharP(metaP);
      // get sync item type for it
      // - filter mostly applies to items SENT, so we search these first
      itemTypeP = getSendType(typestr,NULL);
      if (!itemTypeP)
        itemTypeP = getReceiveType(typestr,NULL);
      PDEBUGPRINTFX(DBG_FILTER,("DS12 <Filter> <Type> is '%s' -> %sfound",typestr,itemTypeP ? "" : "NOT "));
      if (!itemTypeP) {
        sta=415;
        goto error;
      }
    }
    // - filtertype
    if (aTargetFilter->filtertype) {
      const char *ftystr = smlPCDataToCharP(aTargetFilter->filtertype);
      if (strucmp(ftystr,SYNCML_FILTERTYPE_INCLUSIVE)==0) {
        inclusiveFilter=true;
      }
      else if (strucmp(ftystr,SYNCML_FILTERTYPE_EXCLUSIVE)==0) {
        inclusiveFilter=false;
      }
      else {
        PDEBUGPRINTFX(DBG_ERROR,("Invalid <FilterType> '%s'",ftystr));
        sta=422;
        goto error;
      }
    }
    // - field level filter
    if (aTargetFilter->field) {
      /// @todo %%% to be implemented
      PDEBUGPRINTFX(DBG_ERROR,("Field-level filtering not supported"));
      sta=406;
      goto error;
    }
    // - record level filter
    if (aTargetFilter->record) {
      #ifdef OBJECT_FILTERING
      SmlItemPtr_t recordItemP = aTargetFilter->record->item;
      if (recordItemP) {
        // - check grammar
        const char *grammarstr = smlMetaTypeToCharP(smlPCDataToMetInfP(recordItemP->meta));
        if (strucmp(grammarstr,SYNCML_FILTERTYPE_CGI)!=0) {
          PDEBUGPRINTFX(DBG_ERROR,("Invalid filter grammar '%s'",grammarstr));
          sta=422;
          goto error;
        }
        // now get the actual filter string
        const char *filterstring = smlPCDataToCharP(recordItemP->data);
        PDEBUGPRINTFX(DBG_HOT,(
          "Remote specified %sCLUSIVE filter query: '%s'",
          inclusiveFilter ? "IN" : "EX",
          filterstring
        ));
        if (*filterstring) {
          string f;
          // parse it
          filterstring = parseFilterCGI(filterstring,itemTypeP,f);
          if (*filterstring) {
            // not read to end
            PDEBUGPRINTFX(DBG_ERROR,("filter query syntax error at: '%s'",filterstring));
            sta=422;
            goto error;
          }
          /// @todo: %%% check if this is correct interpretation
          // - exclusive is what we used to call "sync set" filtering
          // - inclusive seems to be former TAF
          if (inclusiveFilter) {
            // INCLUSIVE
            #ifdef SYNCML_TAF_SUPPORT
            fTargetAddressFilter=f;
            #else
            PDEBUGPRINTFX(DBG_ERROR,("This SyncML engine version has no INCLUSIVE filter support"));
            #endif
          }
          else {
            // EXCLUSIVE
            addToFilter(f.c_str(),fSyncSetFilter,false); // AND chaining
            PDEBUGPRINTFX(DBG_FILTER,("complete sync set filter is now: '%s'",fSyncSetFilter.c_str()));
            // call this once to give derivate a chance to see if it can filter the now set fSyncSetFilter
            engFilteredFetchesFromDB(true);
          }
        }
      } // if item
      #else
      // no object filtering
      PDEBUGPRINTFX(DBG_ERROR,("This SyncML engine version has no filter support (only PRO has)"));
      sta=406;
      goto error;
      #endif
    } // record
  } // filter at all
error:
  return sta;
} // TLocalEngineDS::engProcessDS12Filter


// process Sync alert from remote party: check if alert code is supported,
// check if slow sync is needed due to anchor mismatch
// - server case: also generate appropriate Alert acknowledge command
TAlertCommand *TLocalEngineDS::engProcessSyncAlert(
  TSuperDataStore *aAsSubDatastoreOf, // if acting as subdatastore
  uInt16 aAlertCode,                  // the alert code
  const char *aLastRemoteAnchor,      // last anchor of remote
  const char *aNextRemoteAnchor,      // next anchor of remote
  const char *aTargetURI,             // target URI as sent by remote, no processing at all
  const char *aIdentifyingTargetURI,  // target URI that was used to identify datastore
  const char *aTargetURIOptions,      // option string contained in target URI
  SmlFilterPtr_t aTargetFilter,       // DS 1.2 filter, NULL if none
  const char *aSourceURI,             // source URI
  TStatusCommand &aStatusCommand      // status that might be modified
)
{
  TAlertCommand *alertcmdP=NULL;
  localstatus sta=LOCERR_OK;

  SYSYNC_TRY {
    if (IS_SERVER) {
      // save the identifying URI
      fIdentifyingDBName = aIdentifyingTargetURI;
    }
    // determine status of read-only option
    fReadOnly=
      fSessionP->getReadOnly() || // session level read-only flag (probably set by login)
      fDSConfigP->fReadOnly; // or datastore config
    #ifdef SUPERDATASTORES
    // if running as subdatastore of a superdatastore already, this call mus be from a superdatastore as well (aAsSubDatastoreOf!=NULL)
    // Note: On a client, fAsSubDatastoreOf is set earlier in dsSetClientSyncParams()
    //       On a server, fAsSubDatastoreOf will be set now to avoid alerting as sub- and normal datastore at the same time.
    if (fAsSubDatastoreOf && !aAsSubDatastoreOf) {
      // bad, cannot be alerted directly AND as subdatastore
      aStatusCommand.setStatusCode(400);
      ADDDEBUGITEM(aStatusCommand,"trying to alert already alerted subdatastore");
      PDEBUGPRINTFX(DBG_ERROR,("Already alerted as subdatastore of '%s'",fAsSubDatastoreOf->getName()));
      return NULL;
    }
    // set subdatastore mode
    fAsSubDatastoreOf = aAsSubDatastoreOf;
    #endif
    // reset type info
    fLocalSendToRemoteTypeP = NULL;
    fLocalReceiveFromRemoteTypeP = NULL;
    fRemoteReceiveFromLocalTypeP=NULL;
    fRemoteSendToLocalTypeP=NULL;
    // prepare database-level scripts
    // NOTE: in client case, alertprepscript is already rebuilt here!
    #ifdef SCRIPT_SUPPORT
    TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fDSConfigP->fDBInitScript,fDataStoreScriptContextP,fSessionP);
    TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fDSConfigP->fSentItemStatusScript,fDataStoreScriptContextP,fSessionP);
    TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fDSConfigP->fReceivedItemStatusScript,fDataStoreScriptContextP,fSessionP);
    TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fDSConfigP->fAlertScript,fDataStoreScriptContextP,fSessionP);
    TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fDSConfigP->fDBFinishScript,fDataStoreScriptContextP,fSessionP,true); // now instantiate vars
    #endif
    // NOTE for client case:
    //   ALL instantiated datastores have already sent an Alert to the server by now here

    // check DS 1.2 <filter>
    sta = engProcessDS12Filter(aTargetFilter);
    if (sta != LOCERR_OK) {
      aStatusCommand.setStatusCode(sta);
      ADDDEBUGITEM(aStatusCommand,"Invalid <Filter> in target options");
      return NULL; // error in options
    }

    // Filter CGI is now a combination of TAF and Synthesis-Style
    // extras (options).
    if (aTargetURIOptions && *aTargetURIOptions) {
      // there are target address options (such as filter CGI and TAF)
      sta = engParseOptions(aTargetURIOptions,false);
      if (sta != LOCERR_OK) {
        aStatusCommand.setStatusCode(sta);
        ADDDEBUGITEM(aStatusCommand,"Invalid CGI target URI options");
        return NULL; // error in options
      }
    }
    if (IS_SERVER) {
      // server case: initially we are not in refresh only mode. Alert code or alert script could change this
      fRefreshOnly=false;
    }

    // save it for suspend and reference in scripts
    fAlertCode=aAlertCode;
    #ifdef SCRIPT_SUPPORT
    // call the alert script, which might want to force a slow sync and/or a server sync set zap
    TScriptContext::execute(
      fDataStoreScriptContextP,
      fDSConfigP->fAlertScript,
      &DBFuncTable,
      this // caller context
    );
    aAlertCode=fAlertCode; // get possibly modified version back (SETALERTCODE)
    #endif
    // if we process a sync alert now, we haven't started sync or map generation
    #ifdef SYSYNC_SERVER
    if (IS_SERVER) {
      // server case: forget Temp GUID mapping
      // make sure we are not carrying forward any left-overs. Last sessions's tempGUID mappings that are
      // needed for "early map" resolution might be loaded by the call to engInitSyncAnchors below.
      // IMPORTANT NOTE: the tempGUIDs that might get loaded will become invalid as soon as <Sync>
      // starts - so fTempGUIDMap needs to be cleared again as soon as the first <Sync> command arrives from the client.
      fTempGUIDMap.clear();
    }
    #endif
    // save remote's next anchor for saving at end of session
    fNextRemoteAnchor = aNextRemoteAnchor;
    // get target info in case we are server
    #ifdef SYSYNC_SERVER
    if (IS_SERVER) {
      // now get anchor info out of database
      // - make sure other anchor variables are set
      sta = engInitSyncAnchors(
        aIdentifyingTargetURI, // use processed form, not as sent by remote
        aSourceURI
      );
      if (sta!=LOCERR_OK) {
        // error getting anchors
        aStatusCommand.setStatusCode(syncmlError(sta));
        PDEBUGPRINTFX(DBG_ERROR,("Could not get Sync Anchor info, status=%hd",sta));
        return NULL; // no alert to send back
      }
      // Server ok until here
      PDEBUGPRINTFX(DBG_PROTO,(
        "Saved Last Remote Client Anchor='%s', received <last> Remote Client Anchor='%s' (must match for normal sync)",
        fLastRemoteAnchor.c_str(),
        aLastRemoteAnchor
      ));
      PDEBUGPRINTFX(DBG_PROTO,(
        "Received <next> Remote Client Anchor='%s' (to be compared with <last> in NEXT session)",
        fNextRemoteAnchor.c_str()
      ));
      PDEBUGPRINTFX(DBG_PROTO,(
        "(Saved) Last Local Server Anchor='%s', (generated) Next Local Server Anchor='%s' (sent to client as <last>/<next> in <alert>)",
        fLastLocalAnchor.c_str(),
        fNextLocalAnchor.c_str()
      ));
    }
    #endif
    #ifdef SYSYNC_CLIENT
    if (IS_CLIENT) {
      // Client ok until here
      PDEBUGPRINTFX(DBG_PROTO,(
        "Saved Last Remote Server Anchor='%s', received <last> Remote Server Anchor='%s' (must match for normal sync)",
        fLastRemoteAnchor.c_str(),
        aLastRemoteAnchor
      ));
      PDEBUGPRINTFX(DBG_PROTO,(
        "Received <next> Remote Server Anchor='%s' (to be compared with <last> in NEXT session)",
        fNextRemoteAnchor.c_str()
      ));
    }
    #endif
    PDEBUGPRINTFX(DBG_PROTO,(
      "(Saved) fResumeAlertCode = %hd (valid for >DS 1.2 only)",
      fResumeAlertCode
    ));
    // Now check for resume
    // - default to what was actually alerted
    uInt16 effectiveAlertCode=aAlertCode;
    #ifdef SYSYNC_SERVER
    if (IS_SERVER) {
      // - check if resuming server session
      fResuming=false;
      if (aAlertCode==225) {
        if (fSessionP->getSyncMLVersion()<syncml_vers_1_2) {
          aStatusCommand.setStatusCode(406);
          ADDDEBUGITEM(aStatusCommand,"Resume not supported in SyncML prior to 1.2");
          PDEBUGPRINTFX(DBG_ERROR,("Resume not supported in SyncML prior to 1.2"));
          return NULL;
        }
        // Resume requested
        if (fResumeAlertCode==0 || !dsResumeSupportedInDB()) {
          // cannot resume, suggest a normal sync (in case anchors do not match, this will become a 508 below)
          aStatusCommand.setStatusCode(509); // cannot resume, override
          effectiveAlertCode=200; // suggest normal sync
          ADDDEBUGITEM(aStatusCommand,"Cannot resume, suggesting a normal sync");
          PDEBUGPRINTFX(DBG_ERROR,("Cannot resume, suggesting a normal sync"));
        }
        else {
          // we can resume, use the saved alert code
          effectiveAlertCode=fResumeAlertCode;
          PDEBUGPRINTFX(DBG_HOT,("Alerted to resume previous session, Switching to alert Code = %hd",fResumeAlertCode));
          fResuming=true;
        }
      }
    }
    #endif
    // now do the actual alert internally
    if (sta==LOCERR_OK) {
      // check if we can process the alert
      // NOTE: for client this might cause a change in Sync mode, if server
      //       alerts something different than client alerted before.
      //       Note that a client will keep fromserver mode even if server
      //       changes to two-way, as it might be that we have sent the server
      //       a two-way alert even if we want fromserver due to compatibility with
      //       servers that cannot do fromserver.
      if (IS_SERVER) {
        // - Server always obeys what client requests (that is, if alertscript does not modify it)
        sta = setSyncModeFromAlertCode(effectiveAlertCode,false); // as server
      } // server
      else {
        // - for client, check that server can't switch to a client writing mode
        //   (we had a case when mobical did that for a user and erased all his data)
        TSyncModes prevMode = fSyncMode; // remember previous mode
        sta = setSyncModeFromAlertCode(effectiveAlertCode,true); // as client
        if (prevMode==smo_fromclient && fSyncMode!=smo_fromclient) {
          // server tries to switch to a mode that could be writing data to the client
          // - forbidden
          sta=403;
          aStatusCommand.setStatusCode(syncmlError(sta));
          ADDDEBUGITEM(aStatusCommand,"Server may not write to client");
          PDEBUGPRINTFX(DBG_ERROR,("From-Client only: Server may not alert mode that writes client data (%hd) - blocked",effectiveAlertCode));
          // - also abort sync of this datastore without chance to resume, as this problem might
          //   originate from a resume attempt, which the server
          //   tried to convert to a normal or slow sync. With cancelling the resume here, we make sure
          //   next sync will start over and sending the desired (one-way) sync mode again.
          engAbortDataStoreSync(sta, false, false); // remote problem, not resumable
        }
      } // client
      if (!isAborted()) {
        if (sta!=LOCERR_OK) {
          // Sync type not supported
          // - go back to idle
          changeState(dssta_idle,true); // force to idle
          aStatusCommand.setStatusCode(syncmlError(sta));
          ADDDEBUGITEM(aStatusCommand,"Sync type not supported");
          PDEBUGPRINTFX(DBG_ERROR,("Sync type (Alert code %hd) not supported, err=%hd",effectiveAlertCode,sta));
        }
      }
    }
    if (sta==LOCERR_OK) {
      // Sync type supported
      // - set new state to alerted
      if (IS_CLIENT) {
        changeState(dssta_clientalerted,true); // force it
      }
      else {
        changeState(dssta_serveralerted,true); // force it
      }
      // - datastore state is now dss_alerted
      PDEBUGPRINTFX(DBG_HOT,(
        "Alerted (code=%hd) for %s%s %s%s%s ",
        aAlertCode,
        fResuming ? "Resumed " : "",
        SyncModeDescriptions[fSyncMode],
        fSlowSync ? (fSyncMode==smo_twoway ? "Slow Sync" : "Refresh") : "Normal Sync",
        fReadOnly ? " (Readonly)" : "",
        fRefreshOnly ? " (Refreshonly)" : ""
      ));
      CONSOLEPRINTF((
        "- Remote alerts (%hd): %s%s %s%s%s for '%s' (source='%s')",
        aAlertCode,
        fResuming ? "Resumed " : "",
        SyncModeDescriptions[fSyncMode],
        fSlowSync ? (fSyncMode==smo_twoway ? "Slow Sync" : "Refresh") : "Normal Sync",
        fReadOnly ? " (Readonly)" : "",
        fRefreshOnly ? " (Refreshonly)" : "",
        aTargetURI,
        aSourceURI
      ));
      // - now test if the sync state is same in client & server
      //   (if saved anchor is empty, slow sync is needed anyway)
      if (
        (
          (
            (!fLastRemoteAnchor.empty() &&
              ( (fLastRemoteAnchor==aLastRemoteAnchor)
                #ifdef SYSYNC_CLIENT
                || (fSessionP->fLenientMode && IS_CLIENT)
                #endif
              )
            ) || // either anchors must match (or lenient mode for client)...
            (fResuming && *aLastRemoteAnchor==0) // ...or in case of resume, remote not sending anchor is ok as well
          )
          && !fForceSlowSync // ...but no force for slowsync may be set internally
        )
        || fSlowSync // if slow sync is requested by the remote anyway, we don't need to be in sync anyway, so just go on
      ) {
        if (!(fLastRemoteAnchor==aLastRemoteAnchor) && fSessionP->fLenientMode) {
          PDEBUGPRINTFX(DBG_ERROR,("Warning - remote anchor mismatch but tolerated in lenient mode"));
        }
        // sync state ok or Slow sync requested anyway:
        #ifdef SYSYNC_SERVER
        if (IS_SERVER) {
          // we can generate Alert with same code as sent
          // %%% Note: this is not entirely clear, as SCTS sends
          //     corresponding SERVER ALERTED code back.
          //     Specs suggest that we send the code back unmodified
          uInt16 alertCode = getSyncStateAlertCode(fServerAlerted);
          alertcmdP = new TAlertCommand(fSessionP,this,alertCode);
          fAlertCode=alertCode; // save it for reference in scripts and for suspend/resume
        }
        #endif
      }
      else {
        // switch to slow sync
        fSlowSync=true;
        PDEBUGPRINTFX(DBG_HOT,("Switched to SlowSync because of Anchor mismatch or server-side user option"));
        CONSOLEPRINTF(("- switched to SlowSync because of Sync Anchor mismatch"));
        // sync state not ok, we need slow sync
        aStatusCommand.setStatusCode(508); // Refresh required
        // update effective alert code
        uInt16 alertCode = getSyncStateAlertCode(false);
        fAlertCode=alertCode; // save it for reference in scripts and for suspend
        // NOTE: if client detected slow-sync not before here, status 508 alone
        //       (without another Alert 201 sent to the server) is sufficient for
        //       server to switch to slow sync.
        if (IS_SERVER) {
          // generate Alert for Slow sync
          alertcmdP = new TAlertCommand(fSessionP,this,alertCode);
        }
      }
      // Now we are alerted for a sync
      // - reset item counters
      fItemsSent = 0;
      fItemsReceived = 0;
      #ifdef SYSYNC_SERVER
      if (IS_SERVER) {
        // Server case
        // - show info
        PDEBUGPRINTFX(DBG_HOT,(
          "ALERTED from client for %s%s%s Sync",
          fResuming ? "resumed " : "",
          fSlowSync ? "slow" : "normal",
          fFirstTimeSync ? " first time" : ""
        ));
        // server: add Item with Anchors and URIs
        SmlItemPtr_t itemP = newItem();
        // - anchors
        itemP->meta=newMetaAnchor(fNextLocalAnchor.c_str(),fLastLocalAnchor.c_str());
        // - MaxObjSize here again to make SCTS happy
        if (
          (fSessionP->getRootConfig()->fLocalMaxObjSize>0) &&
          (fSessionP->getSyncMLVersion()>=syncml_vers_1_1)
        ) {
          // SyncML 1.1 has object size and we need to put it here for SCTS
          smlPCDataToMetInfP(itemP->meta)->maxobjsize=newPCDataLong(
            fSessionP->getRootConfig()->fLocalMaxObjSize
          );
        }
        // - URIs (reversed from what was received in Alert)
        itemP->source=newLocation(aTargetURI); // use unprocessed form as sent by remote
        itemP->target=newLocation(aSourceURI);
        // - add to alert command
        alertcmdP->addItem(itemP);
        // - set new state, alert now answered
        changeState(dssta_serveransweredalert,true); // force it
      } // server case
      #endif // SYSYNC_SERVER
      #ifdef SYSYNC_CLIENT
      if (IS_CLIENT) {
        // Client case
        // - now sync mode is stable (late switch to slowsync has now occurred if any)
        changeState(dssta_syncmodestable,true);
        // - show info
        PDEBUGPRINTFX(DBG_HOT,(
          "ALERTED from server for %s%s%s Sync",
          fResuming ? "resumed " : "",
          fSlowSync ? "slow" : "normal",
          fFirstTimeSync ? " first time" : ""
        ));
      } // client Case
      #endif // SYSYNC_CLIENT
    }
    // clear partial item if we definitely know we are not resuming
    if (!fResuming) {
      // not resuming - prevent that partial item is used in TSyncOpCommand
      fPartialItemState=pi_state_none;
      // free this space early (would be freed at session end anyway, but we don't need it any more now)
      if (fPIStoredDataAllocated) {
        smlLibFree(fPIStoredDataP);
        fPIStoredDataAllocated=false;
      }
      fPIStoredDataP=NULL;
    }
    // save name how remote adresses local database
    // (for sending same URI back in own Sync)
    fRemoteViewOfLocalURI = aTargetURI; // save it
    if (IS_SERVER) {
      fRemoteDBPath = aSourceURI;
    }
    if (sta!=LOCERR_OK) {
      // no alert command
      if (alertcmdP) delete alertcmdP;
      alertcmdP=NULL;
      aStatusCommand.setStatusCode(syncmlError(sta));
      PDEBUGPRINTFX(DBG_HOT,("engProcessSyncAlert failed with status=%hd",sta));
    }
  }
  SYSYNC_CATCH (...)
    // clean up locally owned objects
    if (alertcmdP) delete alertcmdP;
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
  // return alert command, if any
  return alertcmdP;
} // TLocalEngineDS::engProcessSyncAlert


// process status received for sync alert
bool TLocalEngineDS::engHandleAlertStatus(TSyError aStatusCode)
{
  bool handled=false;
  if (IS_CLIENT) {
    // for client, make sure we have just sent the alert
    if (!testState(dssta_clientsentalert,true)) return false; // cannot switch if server not alerted
    // anyway, we have seen the status
    changeState(dssta_clientalertstatused,true); // force it
  }
  else {
    // for server, check if client did combined init&sync
    if (fLocalDSState>=dssta_syncmodestable) {
      // must be combined init&sync
      if (aStatusCode!=200) {
        // everything except ok is not allowed here
        PDEBUGPRINTFX(DBG_ERROR,("In combined init&sync, Alert status must be ok (but is %hd)",aStatusCode));
        dsAbortDatastoreSync(400,false); // remote problem
      }
      // aborted or not, status is handled
      return true;
    }
    // normal case with separate init: we need to have answered the alert here
    if (!testState(dssta_serveransweredalert,true)) return false; // cannot switch if server not alerted
  } // server case
  // now check status code
  if (aStatusCode==508) {
    // remote party needs slow sync
    PDEBUGPRINTFX(DBG_HOT,("engHandleAlertStatus: Remote party needs SlowSync, switching to slowsync (AFTER alert, cancelling possible Resume)"));
    // Note: in server and client cases, this mode change may happen AFTER alert command exchange
    // - switch to slow sync
    fSlowSync=true;
    // - if we are late-forced to slow sync, this means that this cannot be a resume
    fResuming=false;
    // - update effective alert code that will be saved when this session gets suspended
    fAlertCode=getSyncStateAlertCode(fServerAlerted);
    handled=true;
  }
  else if (aStatusCode==200) {
    handled=true;
  }
  if (IS_CLIENT) {
    // check for resume override by server
    if (!handled && fResuming) {
      // we have requested resume
      if (aStatusCode==509) {
        // resume not accepted by server, but overridden by another sync type
        fResuming=false;
        PDEBUGPRINTFX(DBG_ERROR,("engHandleAlertStatus: Server rejected Resume"));
        handled=true;
      }
    }
  }
  else {
    // if we have handled it here, sync mode is now stable
    if (handled) {
      // if we get that far, sync mode for server is now stable AND we can receive cached maps
      changeState(dssta_syncmodestable,true); // force it, sync mode is now stable, no further changes are possible
    }
  }
  // no other status codes are supported at the datastore level
  if (!handled && aStatusCode>=400) {
    engAbortDataStoreSync(aStatusCode, false); // remote problem
    handled=true;
  }
  return handled; // status handled
} // TLocalEngineDS::engHandleAlertStatus


// initialize reception of syncop commands for datastore
// Note: previously, this was implemented as initLocalDatastoreSync in syncsession
localstatus TLocalEngineDS::engInitForSyncOps(
  const char *aRemoteDatastoreURI // URI of remote datastore
)
{
  localstatus sta = LOCERR_OK;

  // no default types
  TSyncItemType *LocalSendToRemoteTypeP=NULL;       // used by local to send to remote
  TSyncItemType *RemoteReceiveFromLocalTypeP=NULL;  // used by remote to receive from local
  TSyncItemType *LocalReceiveFromRemoteTypeP=NULL;  // used by local to receive from remote
  TSyncItemType *RemoteSendToLocalTypeP=NULL;       // used by remote to send to local

  // Now determine remote datastore
  // Note: It might be that this was called already earlier in the session, so
  //       the link between local and remote datastore might already exist
  if (fRemoteDatastoreP==NULL) {
    // try to locate it by name and set it - in case of superdatastore, it will be set in all subdatastores
    engSetRemoteDatastore(fSessionP->findRemoteDataStore(aRemoteDatastoreURI));
  }
  else {
    // There is a remote datastore already associated
    #ifdef SYDEBUG
    // - make a sanity check to see if sepcified remote URI matches
    if(fRemoteDatastoreP!=fSessionP->findRemoteDataStore(aRemoteDatastoreURI)) {
      PDEBUGPRINTFX(DBG_ERROR,(
        "Warning: Received remote DS LocURI '%s' does not match already associated DS '%s'. We use the associated DS.",
        aRemoteDatastoreURI,
        fRemoteDatastoreP->getName()
      ));
    }
    #endif
  }
  // Now create a dummy remote data store for a blind sync attempt
  if (!fRemoteDatastoreP) {
    // no such remote datastore for this local datastore known, create one (or fail)
    #ifdef REMOTE_DS_MUST_BE_IN_DEVINF
    if (fSessionP->fRemoteDataStoresKnown) {
      // we have received devinf, but still can't find remote data store: error
      // Note: we had to disable this because of bugs in smartner server
      PDEBUGPRINTFX(DBG_ERROR,("Remote datastore name '%s' not found in received DevInf",aRemoteDatastoreURI));
      return 404;
    }
    else
    #else
    if (fSessionP->fRemoteDataStoresKnown) {
      // we have received devinf, but still can't find remote data store:
      // just show in log, but continue as if there was no devInf received at all
      PDEBUGPRINTFX(DBG_ERROR,("Warning: Remote datastore name '%s' not found in received DevInf.",aRemoteDatastoreURI));
    }
    #endif
    {
      // We couldn't retrieve DevInf (or !REMOTE_DS_MUST_BE_IN_DEVINF), so we have to try blind
      // - check remote specifics here if we had no devinf (there might be default remote
      //   rules to apply or checking license restrictions
      // - this is executed only once per session, after that, we'll be fRemoteDevInfLock-ed
      if (!fSessionP->fRemoteDevInfKnown && !fSessionP->fRemoteDevInfLock) {
        // detect client specific server behaviour if needed
        sta = fSessionP->checkRemoteSpecifics(NULL, NULL);
        fSessionP->remoteAnalyzed(); // analyzed now (accepted or not does not matter)
        if (sta!=LOCERR_OK)
          return sta; // not ok, device rejected
      }
      // default data types are those preferred by local datastore (or explicitly marked for blind sync attempts)
      if (getDSConfig()->fTypeSupport.fPreferredLegacy) {
        // we have a preferred type for blind sync attempts
        LocalSendToRemoteTypeP = getSession()->findLocalType(getDSConfig()->fTypeSupport.fPreferredLegacy);
        LocalReceiveFromRemoteTypeP = LocalSendToRemoteTypeP;
      }
      else {
        // no specific "blind" preference, use my own normally preferred types
        LocalSendToRemoteTypeP = getPreferredTxItemType(); // send in preferred tx type of local datastore
        LocalReceiveFromRemoteTypeP = getPreferredRxItemType(); // receive in preferred rx type of local datastore
      }
      // same type on both end (as only local type exists)
      RemoteReceiveFromLocalTypeP = LocalSendToRemoteTypeP; // same on both end
      RemoteSendToLocalTypeP = LocalReceiveFromRemoteTypeP; // same on both end (as only local type exists)
      // create "remote" datastore with matching properties to local one
      PDEBUGPRINTFX(DBG_ERROR,("Warning: No DevInf for remote datastore, running blind sync attempt"));
      TRemoteDataStore *remDsP;
      MP_NEW(remDsP,DBG_OBJINST,"TRemoteDataStore",TRemoteDataStore(
        fSessionP,
        aRemoteDatastoreURI, // remote name of datastore
        0 // standard Sync caps
      ));
      // - set it (in case of superdatastore in all subdatastores as well)
      engSetRemoteDatastore(remDsP);
      // add type support
      fRemoteDatastoreP->setPreferredTypes(
        RemoteReceiveFromLocalTypeP, // remote receives in preferred tx type of local datastore
        RemoteSendToLocalTypeP // remote sends in preferred rx type of local datastore
      );
      // add it to the remote datastore list
      fSessionP->fRemoteDataStores.push_back(fRemoteDatastoreP);
      // make sure late devInf arriving won't supersede our artificially created remote datastore any more
      fSessionP->fRemoteDevInfLock=true;
    }
  }
  else {
    // found remote DB, determine default data exchange types
    // - common types for sending data to remote
    LocalSendToRemoteTypeP=getTypesForTxTo(fRemoteDatastoreP,&RemoteReceiveFromLocalTypeP);
    // - common types for receiving data from remote
    LocalReceiveFromRemoteTypeP=getTypesForRxFrom(fRemoteDatastoreP,&RemoteSendToLocalTypeP);
  }
  #ifndef NO_REMOTE_RULES
  // check if rule match type will override what we found so far
  if (!fSessionP->fActiveRemoteRules.empty()) {
    // have a look at our rulematch types
    TRuleMatchTypesContainer::iterator pos;
    TSyncItemType *ruleMatchTypeP = NULL;
    for (pos=fRuleMatchItemTypes.begin();pos!=fRuleMatchItemTypes.end();++pos) {
      // there is a rule applied
      // - parse match string in format "rule[,rule]..." with * and ? wildcards allowed in "rule"
      cAppCharP p=(*pos).ruleMatchString;
      while (*p!=0) {
        // split at commas
        cAppCharP e=strchr(p,',');
        size_t n;
        if (e) {
          n=e-p;
          e++;
        }
        else {
          n=strlen(p);
          e=p+n;
        }
        // see if that matches with any of the active rules
        TRemoteRulesList::iterator apos;
        for(apos=fSessionP->fActiveRemoteRules.begin();apos!=fSessionP->fActiveRemoteRules.end();apos++) {
          if (strwildcmp((*apos)->getName(), p, 0, n)==0) {
            ruleMatchTypeP=(*pos).itemTypeP; // get the matching type
            break;
          }
        }
        if (ruleMatchTypeP) break; // found a rule match type
        // test next match target
        p=e;
      }
      // apply if found one already
      if (ruleMatchTypeP) {
        // use this instead of normal types
        // - local types
        LocalSendToRemoteTypeP=ruleMatchTypeP;       // used by local to send to remote
        LocalReceiveFromRemoteTypeP=ruleMatchTypeP;  // used by local to receive from remote
        // Find matching remote types
        // - first look for existing remote type with same config as local one
        TSyncItemType *remCorrTypeP = fSessionP->findRemoteType(ruleMatchTypeP->getTypeConfig(),fRemoteDatastoreP);
        // - if none found, create one and have it inherit the CTCap options of the generic version that is already there
        if (!remCorrTypeP) {
          // none found: need to create one
          remCorrTypeP = ruleMatchTypeP->newCopyForSameType(fSessionP,fRemoteDatastoreP);
          if (remCorrTypeP) {
            // - get generic remote type (the one that might have received CTCap already)
            TSyncItemType *remGenericTypeP = fRemoteDatastoreP->getSendType(ruleMatchTypeP);
            // - copy options
            if (remGenericTypeP) remCorrTypeP->copyCTCapInfoFrom(*remGenericTypeP);
          }
        }
        // now assign
        RemoteReceiveFromLocalTypeP=remCorrTypeP;
        RemoteSendToLocalTypeP=remCorrTypeP;
        // Show that we are using ruleMatch type
        PDEBUGPRINTFX(DBG_DATA+DBG_HOT,(
          "An active remote rule overrides default type usage - forcing type '%s' for send and receive",
          ruleMatchTypeP->getTypeConfig()->getName()
        ));
        // done
        break;
      }
    }
  }
  #endif
  // check if we are sync compatible (common type for both directions)
  if (LocalSendToRemoteTypeP && LocalReceiveFromRemoteTypeP && RemoteReceiveFromLocalTypeP && RemoteSendToLocalTypeP) {
    // avoid further changes in remote devInf (e.g. by late result of GET, sent *after* first <sync>)
    fSessionP->fRemoteDevInfLock=true;
    // there is a common data type for each of both directions
    // - show local types
    PDEBUGPRINTFX(DBG_DATA,(
      "Local Datastore '%s' - Types: tx to remote: '%s': %s (%s), rx from remote: '%s': %s (%s)",
      getName(),
      LocalSendToRemoteTypeP->getTypeConfig()->getName(),LocalSendToRemoteTypeP->getTypeName(), LocalSendToRemoteTypeP->getTypeVers(),
      LocalReceiveFromRemoteTypeP->getTypeConfig()->getName(),LocalReceiveFromRemoteTypeP->getTypeName(), LocalReceiveFromRemoteTypeP->getTypeVers()
    ));
    // - show remote types
    PDEBUGPRINTFX(DBG_DATA+DBG_DETAILS,(
      "Remote Datastore '%s' - Types: tx to local: '%s': %s (%s), rx from local: '%s': %s (%s)",
      fRemoteDatastoreP->getName(),
      RemoteSendToLocalTypeP->getTypeConfig()->getName(),RemoteSendToLocalTypeP->getTypeName(), RemoteSendToLocalTypeP->getTypeVers(),
      RemoteReceiveFromLocalTypeP->getTypeConfig()->getName(),RemoteReceiveFromLocalTypeP->getTypeName(), RemoteReceiveFromLocalTypeP->getTypeVers()
    ));
  }
  else {
    // datastores are not sync compatible
    sta=415;
    PDEBUGPRINTFX(DBG_ERROR,("No common datastore formats -> cannot sync (415)"));
    PDEBUGPRINTFX(DBG_EXOTIC,("- LocalSendToRemoteTypeP      = '%s'", LocalSendToRemoteTypeP ? LocalSendToRemoteTypeP->getTypeName() : "<missing>"));
    PDEBUGPRINTFX(DBG_EXOTIC,("- LocalReceiveFromRemoteTypeP = '%s'", LocalReceiveFromRemoteTypeP ? LocalReceiveFromRemoteTypeP->getTypeName() : "<missing>"));
    PDEBUGPRINTFX(DBG_EXOTIC,("- RemoteSendToLocalTypeP      = '%s'", RemoteSendToLocalTypeP ? RemoteSendToLocalTypeP->getTypeName() : "<missing>"));
    PDEBUGPRINTFX(DBG_EXOTIC,("- RemoteReceiveFromLocalTypeP = '%s'", RemoteReceiveFromLocalTypeP ? RemoteReceiveFromLocalTypeP->getTypeName() : "<missing>"));
    engAbortDataStoreSync(sta,true,false); // do not proceed with sync of this datastore, local problem, not resumable
    return sta;
  }
  // set type info in local datastore
  setSendTypeInfo(LocalSendToRemoteTypeP,RemoteReceiveFromLocalTypeP);
  setReceiveTypeInfo(LocalReceiveFromRemoteTypeP,RemoteSendToLocalTypeP);
  // - initialize usage of types (checks compatibility as well)
  return initDataTypeUse();
} // TLocalEngineDS::engInitForSyncOps


// called from <sync> command to generate sync sub-commands to be sent to remote
// Returns true if now finished for this datastore
// also changes state to dssta_syncgendone when all sync commands have been generated
bool TLocalEngineDS::engGenerateSyncCommands(
  TSmlCommandPContainer &aNextMessageCommands,
  TSmlCommand * &aInterruptedCommandP,
  const char *aLocalIDPrefix
)
{
  PDEBUGBLOCKFMT(("SyncGen","Now generating sync commands","datastore=%s",getName()));
  bool finished=false;
  #ifdef SYSYNC_CLIENT
  if (IS_CLIENT)
    finished = logicGenerateSyncCommandsAsClient(aNextMessageCommands, aInterruptedCommandP, aLocalIDPrefix);
  #endif
  #ifdef SYSYNC_SERVER
  if (IS_SERVER)
    finished = logicGenerateSyncCommandsAsServer(aNextMessageCommands, aInterruptedCommandP, aLocalIDPrefix);
  #endif
  // change state when finished
  if (finished) {
    changeState(dssta_syncgendone,true);
    if (IS_CLIENT) {
      // from client only skips to clientmapssent without any server communication
      // (except if we are in old synthesis-compatible mode which runs from-client-only
      // with empty sync-from-server and map phases.
      if (getSyncMode()==smo_fromclient && !fSessionP->fCompleteFromClientOnly) {
        // data access ends with all sync commands generated in from-client-only
        PDEBUGPRINTFX(DBG_PROTO,("From-Client-Only sync: skipping directly to end of map phase now"));
        changeState(dssta_dataaccessdone,true);
        changeState(dssta_clientmapssent,true);
      }
    }
  }
  PDEBUGPRINTFX(DBG_DATA,(
    "engGenerateSyncCommands ended, state='%s', sync generation %sdone",
    getDSStateName(),
    fLocalDSState>=dssta_syncgendone ? "" : "NOT "
  ));
  PDEBUGENDBLOCK("SyncGen");
  return finished;
} // TLocalEngineDS::engGenerateSyncCommands


// called to confirm a sync operation's completion (status from remote received)
// @note aSyncOp passed not necessarily reflects what was sent to remote, but what actually happened
void TLocalEngineDS::dsConfirmItemOp(TSyncOperation aSyncOp, cAppCharP aLocalID, cAppCharP aRemoteID, bool aSuccess, localstatus aErrorStatus)
{
  // commands failed with "cancelled" should be re-sent for resume
  if (!aSuccess && aErrorStatus==514 && dsResumeSupportedInDB() && fSessionP->isSuspending()) {
    // cancelled syncop as result of explicit suspend: mark for resume as it was never really processed at the other end
    PDEBUGPRINTFX(DBG_DATA+DBG_EXOTIC,("Cancelled SyncOp during suspend -> mark for resume"));
    engMarkItemForResume(aLocalID,aRemoteID,true);
  }
  PDEBUGPRINTFX(DBG_DATA+DBG_EXOTIC,(
    "dsConfirmItemOp completed, syncop=%s, localID='%s', remoteID='%s', %s, errorstatus=%hd",
    SyncOpNames[aSyncOp],
    aLocalID ? aLocalID : "<none>",
    aRemoteID ? aRemoteID : "<none>",
    aSuccess ? "SUCCESS" : "FAILURE",
    aErrorStatus
  ));
} // TLocalEngineDS::dsConfirmItemOp



// handle status of sync operation
// Note: in case of superdatastore, status is always directed to the originating subdatastore, as
//       the fDataStoreP of the SyncOpCommand is set to subdatastore when generating the SyncOps.
bool TLocalEngineDS::engHandleSyncOpStatus(TStatusCommand *aStatusCmdP,TSyncOpCommand *aSyncOpCmdP)
{
  TSyError statuscode = aStatusCmdP->getStatusCode();
  // we can make it simple here because we KNOW that we do not send multiple items per SyncOp, so we
  // just need to look at the first item's target and source
  const char *localID = aSyncOpCmdP->getSourceLocalID();
  const char *remoteID = aSyncOpCmdP->getTargetRemoteID();
  #ifdef SYSYNC_SERVER
  string realLocID;
  #endif
  if (localID) {
    #ifdef SUPERDATASTORES
    // remove possible prefix if this item was sent in the <sync> command context of a superdatastore
    if (fAsSubDatastoreOf) {
      // let superdatastore remove the prefix for me
      localID = fAsSubDatastoreOf->removeSubDSPrefix(localID,this);
    }
    #endif
    #ifdef SYSYNC_SERVER
    if (IS_SERVER) {
      // for server only: convert to internal representation
      realLocID=localID;
      obtainRealLocalID(realLocID);
      localID=realLocID.c_str();
    }
    #endif
  }
  // handle special cases for Add/Replace/Delete
  TSyncOperation sop = aSyncOpCmdP->getSyncOp();
  switch (sop) {
    case sop_wants_add:
    case sop_add:
      if (statuscode<300 || statuscode==419) {
        // All ok status 2xx as well as special "merged" 419 is ok for an add:
        // Whatever remote said, I know this is an add and so I counts this as such
        // (even if the remote somehow merged it with existing data,
        // it is obviously a new item in my sync set with this remote)
        fRemoteItemsAdded++;
        dsConfirmItemOp(sop_add,localID,remoteID,true); // ok added
      }
      else if (
        statuscode==418 &&
        (isResuming()
        || (isSlowSync() && IS_CLIENT)
        )
      ) {
        // "Already exists"/418 is acceptable...
        // ... in slow sync as client, as some servers use it instead of 200/419 for slow sync match
        // ... during resumed sync as server with clients like Symbian which
        //     can detect duplicate adds themselves. Should not generally
        //     occur, as we shouldn't re-send them as long as we haven't seen
        //     a map. But symbian cannot send early maps - it instead does
        //     it's own duplicate checking.
        // ... during resumed sync as client (as servers might issue 418 for
        //     items sent a second time after an implicit suspend)
        PDEBUGPRINTFX(DBG_ERROR,("Warning: received 418 status for add in resumed/slowsync session -> treat it as ok (200)"));
        dsConfirmItemOp(sop_replace,localID,remoteID,true); // kind of ok
        statuscode=200; // convert to ok (but no count incremented, as nothing changed)
      }
      else {
        dsConfirmItemOp(sop_add,localID,remoteID,false,statuscode); // failed add
      }
      // adding with 420 error: device full
      if (statuscode==420) {
        // special case: device indicates that it is full, so stop adding in this session
        PDEBUGPRINTFX(DBG_ERROR,("Warning: Status %hd: Remote device full -> preventing further adds in this session",statuscode));
        engStopAddingToRemote();
        fRemoteItemsError++; // this is considered a remote item error
      }
      break;
    // case sop_copy: break;
    case sop_wants_replace:
    case sop_replace:
      #ifdef SYSYNC_SERVER
      if (IS_SERVER && (statuscode==404 || statuscode==410)) {
        // obviously, remote item that we wanted to change does not exist any more.
        // Instead of aborting the session we'll just remove the map item for that
        // server item, such that it will be re-added in the next sync session
        PDEBUGPRINTFX(DBG_DATA,("Status %hd: Replace target not found on client -> silently ignore but remove map in server (item will be added in next session), ",statuscode));
        // remove map for remote item(s), targetRef contain remoteIDs
        SmlTargetRefListPtr_t targetrefP = aStatusCmdP->getStatusElement()->targetRefList;
        while (targetrefP) {
          // target ref available
          engProcessMap(smlPCDataToCharP(targetrefP->targetRef),NULL);
          // next
          targetrefP=targetrefP->next;
        }
        statuscode=410; // always use "gone" status (even if we might have received a 404)
        dsConfirmItemOp(sop_replace,localID,remoteID,false,statuscode);
        break;
      }
      else
      #endif
      if (statuscode==201) {
        fRemoteItemsAdded++;
        dsConfirmItemOp(sop_add,localID,remoteID,true); // ok as add
      }
      else if (statuscode<300 || statuscode==419) { // conflict resolved counts as ok as well
        fRemoteItemsUpdated++;
        dsConfirmItemOp(sop_replace,localID,remoteID,true); // ok as replace
      }
      #ifdef SYSYNC_CLIENT
      else if (IS_CLIENT && (isSlowSync() && statuscode==418)) {
        // "Already exists"/418 is acceptable as client in slow sync because some
        // servers use it instead of 200/419 for slow sync match
        PDEBUGPRINTFX(DBG_ERROR,("Warning: received 418 for for replace during slow sync - treat it as ok (200), but don't count as update"));
        dsConfirmItemOp(sop_replace,localID,remoteID,true); // 418 is acceptable in slow sync (not really conformant, but e.g. happening with Scheduleworld)
        statuscode=200; // always use "gone" status (even if we might have received a 404)
      }
      #endif // SYSYNC_CLIENT
      else {
        dsConfirmItemOp(sop_replace,localID,remoteID,false,statuscode); // failed replace
      }
      break;
    case sop_archive_delete:
    case sop_soft_delete:
    case sop_delete:
      if (statuscode<211) fRemoteItemsDeleted++;
      // allow 211 and 404 for delete - after all, the record is not there
      // any more on the remote
      if (statuscode==404 || statuscode==211) {
        PDEBUGPRINTFX(DBG_DATA,("Status: %hd: To-be-deleted item not found, but accepted this (changed status to 200)",statuscode));
        statuscode=200;
      }
      // if ok (explicit or implicit), we can confirm the delete
      dsConfirmItemOp(sop_delete,localID,remoteID,statuscode<300,statuscode); // counts as ok delete
      break;
    default: break;
  } // switch
  // check if we want to mark failed items for resend in the next session or abort
  bool resend = fDSConfigP->fResendFailing; // get default from config
  #ifdef SCRIPT_SUPPORT
  // let script check status code
  TErrorFuncContext errctx;
  errctx.statuscode = statuscode;
  errctx.resend = resend;
  errctx.newstatuscode = statuscode;
  errctx.syncop = sop;
  errctx.datastoreP = this;
  // - first check datastore level
  if (
    TScriptContext::executeTest(
      false, // assume script does NOT handle status entirely
      fDataStoreScriptContextP,
      fDSConfigP->fSentItemStatusScript,
      &ErrorFuncTable,
      &errctx // caller context
    )
  ) {
    // completely handled
    PDEBUGPRINTFX(DBG_ERROR,("Status: %hd: Handled by datastore script (original op was %s)",statuscode,SyncOpNames[sop]));
    return true;
  }
  errctx.statuscode = errctx.newstatuscode;
  // - then check session level
  if (
    TScriptContext::executeTest(
      false, // assume script does NOT handle status entirely
      fSessionP->fSessionScriptContextP,
      fSessionP->getSessionConfig()->fSentItemStatusScript,
      &ErrorFuncTable,
      &errctx // caller context
    )
  ) {
    // completely handled
    PDEBUGPRINTFX(DBG_ERROR,("Status: %hd: Handled by session script (original op was %s)",statuscode,SyncOpNames[sop]));
    return true;
  }
  // not completely handled, use possibly modified status code
  #ifdef SYDEBUG
  if (statuscode != errctx.newstatuscode) {
    PDEBUGPRINTFX(DBG_ERROR,("Status: Script changed original status=%hd to %hd (original op was %s)",statuscode,errctx.newstatuscode,SyncOpNames[errctx.syncop]));
  }
  #endif
  statuscode = errctx.newstatuscode;
  resend = errctx.resend;
  #endif
  // now perform default action according to status code
  switch (statuscode) {
    case 200:
      break;
    case 201:
      PDEBUGPRINTFX(DBG_PROTO,("Status: %hd: Item added (original op was %s)",statuscode,SyncOpNames[sop]));
      break;
    case 204:
      PDEBUGPRINTFX(DBG_PROTO,("Status: %hd: No content (original op was %s)",statuscode,SyncOpNames[sop]));
      break;
    case 207:
    case 208:
    case 209:
    case 419:
      PDEBUGPRINTFX(DBG_HOT,("Status: %hd: Conflict resolved (original op was %s)",statuscode,SyncOpNames[sop]));
      break;
    case 210:
      PDEBUGPRINTFX(DBG_HOT,("Status: %hd: Delete without archive (original op was %s)",statuscode,SyncOpNames[sop]));
      break;
    case 211:
      PDEBUGPRINTFX(DBG_ERROR,("Status: %hd: nothing deleted, item not found (original op was %s)",statuscode,SyncOpNames[sop]));
      break;
    case 410: // gone
    case 420: // device full
      // these have been handled above and are considered ok now
      break;
    case 514: // cancelled
      // ignore cancelling while suspending, as these are CAUSED by the suspend
      if (fSessionP->isSuspending() && dsResumeSupportedInDB()) {
        // don't do anything here - we'll be suspended later (but process commands until then)
        // dsConfirmItemOp() has already caused the item to be marked for resume
        break;
      }
      // for non-DS-1.2 sessions, we treat 514 like the other errors below (that is - retry might help)
    case 424: // size mismatch (e.g. due to faild partial item resume attempt -> retry will help)
    case 417: // retry later (remote says that retry will probably work)
    case 506: // processing error, retry later (remote says that retry will probably work)
    case 404: // not found (retry is not likely to help, but does not harm too much, either)
    case 408: // timeout (if that happens on a single item, retry probably helps)
    case 415: // bad type (retry is not likely to help, but does not harm too much, either)
    case 510: // datastore failure (too unspecific to know if retry might help, but why not?)
    case 500: // general failure (too unspecific to know if retry might help, but why not?)
      // these errors cause either a resend in a later session
      // or only abort the datastore, but not the session
      if (resend && dsResumeSupportedInDB()) {
        PDEBUGPRINTFX(DBG_ERROR,("Status: General error %hd (original op was %s) -> marking item for resend in next session",statuscode,SyncOpNames[sop]));
        engMarkItemForResend(localID,remoteID); // Note: includes incrementing fRemoteItemsError
      }
      else {
        PDEBUGPRINTFX(DBG_ERROR,("Status: General error %hd (original op was %s) -> aborting sync with this datastore",statuscode,SyncOpNames[sop]));
        engAbortDataStoreSync(statuscode,false); // remote problem
      }
      break;
    default:
      // let command handle it
      return false;
      //break;
  }
  // status handled
  return true; // handled status
} // TLocalEngineDS::engHandleSyncOpStatus


/// Internal events during sync for derived classes
/// @Note local DB authorisation must be established already before calling these
/// - cause loading of all session anchoring info and other admin data (logicMakeAdminReady())
///   fLastRemoteAnchor,fLastLocalAnchor,fNextLocalAnchor; isFirstTimeSync() will be valid after the call
/// - in case of superdatastore, consolidates the anchor information from the subdatastores
localstatus TLocalEngineDS::engInitSyncAnchors(
  cAppCharP aDatastoreURI,      ///< local datastore URI
  cAppCharP aRemoteDBID         ///< ID of remote datastore (to find session information in local DB)
)
{
  // nothing more to do than making admin data ready
  // - this will fill all dsSavedAdminData members here and in all derived classes
  localstatus sta=logicMakeAdminReady(aDatastoreURI, aRemoteDBID);
  if (sta==LOCERR_OK) {
    changeState(dssta_adminready); // admin data is now ready
  }
  // return on error
  return sta;
} // TLocalEngineDS::engInitSyncAnchors


#ifdef SYSYNC_CLIENT

// initialize Sync alert for datastore according to Parameters set with dsSetClientSyncParams()
localstatus TLocalEngineDS::engPrepareClientSyncAlert(void)
{
  #ifdef SUPERDATASTORES
  // no operation here if running under control of a superdatastore.
  // superdatastore's engPrepareClientSyncAlert() will call engPrepareClientRealDSSyncAlert of all subdatastores at the right time
  if (fAsSubDatastoreOf)
    return LOCERR_OK;
  #endif
  // this is a real datastore
  return engPrepareClientDSForAlert();
} // TLocalEngineDS::engPrepareClientSyncAlert



// initialize Sync alert for datastore according to Parameters set with dsSetClientSyncParams()
localstatus TLocalEngineDS::engPrepareClientDSForAlert(void)
{
  localstatus sta;

  // reset the filters that might be added to in alertprepscript
  // (as they might have been half set-up in a previous failed alert, they must be cleared and re-constructed here)
  resetFiltering();

  #ifdef SCRIPT_SUPPORT
  // AlertPrepareScript to add filters and CGI
  // - rebuild early (before all of the other DS scripts in makeAdminReady caused by engInitSyncAnchors below!)
  TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fDSConfigP->fAlertPrepScript,fDataStoreScriptContextP,fSessionP);
  // - add custom DS 1.2 filters and/or custom CGI to fRemoteDBPath
  TScriptContext::execute(
    fDataStoreScriptContextP,
    fDSConfigP->fAlertPrepScript,
    fDSConfigP->getClientDBFuncTable(), // function table with extra
    this // datastore pointer needed for context
  );
  #endif
  // - save the identifying name of the DB
  fIdentifyingDBName = fLocalDBPath;
  // - get information about last session out of database
  sta = engInitSyncAnchors(
    relativeURI(fLocalDBPath.c_str()),
    fRemoteDBPath.c_str()
  );
  if (sta!=LOCERR_OK) {
    // error getting anchors
    PDEBUGPRINTFX(DBG_ERROR,("Could not get Sync Anchor info"));
    return localError(sta);
  }
  // check if we are forced to slowsync (otherwise, fSlowSync is pre-set from dsSetClientSyncParams()
  fSlowSync = fSlowSync || fLastLocalAnchor.empty() || fFirstTimeSync;
  // check for resume
  if (fResumeAlertCode!=0 && fSessionP->getSyncMLVersion()>=syncml_vers_1_2) {
    // we have a suspended session, try to resume
    PDEBUGPRINTFX(DBG_PROTO,("Found suspended session with Alert Code = %hd",fResumeAlertCode));
    fResuming = true;
  }
  return LOCERR_OK; // ok
} // TLocalEngineDS::engPrepareClientDSForAlert


// generate Sync alert for datastore after initialisation with engPrepareClientSyncAlert()
// Note: this could be repeatedly called due to auth failures at beginning of session
// Note: this is a NOP for subDatastores (should not be called in this case, anyway)
localstatus TLocalEngineDS::engGenerateClientSyncAlert(
  TAlertCommand *&aAlertCommandP
)
{
  aAlertCommandP=NULL;
  #ifdef SUPERDATASTORES
  if (fAsSubDatastoreOf) return LOCERR_OK; // NOP, ok, only superdatastore creates an alert!
  #endif

  PDEBUGPRINTFX(DBG_PROTO,(
    "(Saved) Last Local Client Anchor='%s', (generated) Next Local Client Anchor='%s' (sent to server as <last>/<next> in <alert>)",
    fLastLocalAnchor.c_str(),
    fNextLocalAnchor.c_str()
  ));
  // create appropriate initial alert command
  TAgentConfig *configP = static_cast<TAgentConfig *>(static_cast<TSyncAgent *>(getSession())->getRootConfig()->fAgentConfigP);
  uInt16 alertCode = getSyncStateAlertCode(fServerAlerted, configP->fPreferSlowSync);
  // check for resume
  if (fResuming) {
    // check if what we resume is same as what we wanted to do
    if (alertCode != fResumeAlertCode) {
      // this is ok for client, just show in log
      PDEBUGPRINTFX(DBG_PROTO,(
        "Sync mode seems to have changed (alert code = %hd) since last Suspend (alert code = %hd)",
        alertCode,
        fResumeAlertCode
      ));
    }
    // resume
    alertCode=225; // resume
    PDEBUGPRINTFX(DBG_PROTO,(
      "Alerting resume of last sync session (original alert code = %hd)",
      fResumeAlertCode
    ));
  }
  aAlertCommandP = new TAlertCommand(fSessionP,this,alertCode);
  PDEBUGPRINTFX(DBG_HOT,(
    "%s: ALERTING server for %s%s%s Sync",
    getName(),
    fResuming ? "RESUMED " : "",
    fSlowSync ? "slow" : "normal",
    fFirstTimeSync ? " first time" : ""
  ));
  // add Item with Anchors and URIs
  SmlItemPtr_t itemP = newItem();
  // - anchors
  itemP->meta=newMetaAnchor(fNextLocalAnchor.c_str(),fLastLocalAnchor.c_str());
  // - MaxObjSize here again to make SCTS happy
  if (
    (fSessionP->getSyncMLVersion()>=syncml_vers_1_1) &&
    (fSessionP->getRootConfig()->fLocalMaxObjSize>0)
  ) {
    // SyncML 1.1 has object size and we need to put it here for SCTS
    smlPCDataToMetInfP(itemP->meta)->maxobjsize=newPCDataLong(
      fSessionP->getRootConfig()->fLocalMaxObjSize
    );
  }
  // - URIs
  itemP->source=newLocation(fLocalDBPath.c_str()); // local DB ID
  itemP->target=newLocation(fRemoteDBPath.c_str()); // use remote path as configured in client settings
  // - add DS 1.2 filters
  if (!fRemoteRecordFilterQuery.empty() || false /* %%% field level filter */) {
    if (fSessionP->getSyncMLVersion()<syncml_vers_1_2) {
      PDEBUGPRINTFX(DBG_ERROR,("Filter specified, but SyncML version is < 1.2"));
      engAbortDataStoreSync(406, true, false); // can't continue sync
      return 406; // feature not supported
    }
    SmlFilterPtr_t filterP = SML_NEW(SmlFilter_t);
    memset(filterP,0,sizeof(SmlFilter_t));
    if (filterP) {
      // Must have a meta with the content type
      // - get the preferred receive type
      TSyncItemType *itemTypeP = fSessionP->findLocalType(fDSConfigP->fTypeSupport.fPreferredRx);
      if (itemTypeP) {
        // add meta type
        filterP->meta = newMetaType(itemTypeP->getTypeName());
      }
      // add filtertype if needed (=not EXCLUSIVE)
      if (fRemoteFilterInclusive) {
        filterP->filtertype=newPCDataString(SYNCML_FILTERTYPE_INCLUSIVE);
      }
      // record level
      if (!fRemoteRecordFilterQuery.empty()) {
        // add <record>
        filterP->record = SML_NEW(SmlRecordOrFieldFilter_t);
        // - add item with data=filterquery
        filterP->record->item = newStringDataItem(fRemoteRecordFilterQuery.c_str());
        // - add meta type with grammar
        filterP->record->item->meta = newMetaType(SYNCML_FILTERTYPE_CGI);
        PDEBUGPRINTFX(DBG_HOT,(
          "Alerting with %sCLUSIVE Record Level Filter Query = '%s'",
          fRemoteFilterInclusive ? "IN" : "EX",
          fRemoteRecordFilterQuery.c_str()
        ));
      }
      // field level
      /// @todo %%% to be implemented
      if (false) {
        // !!! remember to add real check (now: false) in outer if-statement as well!!!!!
        // %%% tbd
      }
    }
    // add it to item
    itemP->target->filter = filterP;
  }
  // - add to alert command
  aAlertCommandP->addItem(itemP);
  // we have now produced the client alert command, change state
  return changeState(dssta_clientsentalert);
} // TLocalEngineDS::engGenerateClientSyncAlert


// Init engine for client sync
// - determine types to exchange
// - make sync set ready
localstatus TLocalEngineDS::engInitForClientSync(void)
{
  #ifdef SUPERDATASTORES
  // no init in case we are under control of a superdatastore -> the superdatastore will do that
  if (fAsSubDatastoreOf)
    return LOCERR_OK;
  #endif
  return engInitDSForClientSync();
} // TLocalEngineDS::engInitForClientSync



// Init engine for client sync
// - determine types to exchange
// - make sync set ready
localstatus TLocalEngineDS::engInitDSForClientSync(void)
{
  // make ready for syncops
  localstatus sta = engInitForSyncOps(getRemoteDBPath());
  if (sta==LOCERR_OK) {
    // - let local datastore (derived DB-specific class) prepare for sync
    sta = changeState(dssta_dataaccessstarted);
    if (sta==LOCERR_OK && isStarted(false)) {
      // already started now, change state
      sta = changeState(dssta_syncsetready);
    }
  }
  return sta;
} // TLocalEngineDS::engInitDSForClientSync


#endif // Client


// get Alert code for current Sync State
uInt16 TLocalEngineDS::getSyncStateAlertCode(bool aServerAlerted, bool aClientMinimal)
{
  uInt16 alertcode=0;

  switch (fSyncMode) {
    case smo_twoway :
      alertcode = aServerAlerted ? 206 : 200;
      break;
    case smo_fromclient :
      alertcode = aServerAlerted ? 207 : 202; // fully specified
      break;
    case smo_fromserver :
      if (aClientMinimal) {
        // refresh from server is just client not sending any data, so we can alert like two-way
        alertcode = aServerAlerted ? 206 : 200;
      }
      else {
        // correctly alert it
        alertcode = aServerAlerted ? 209 : 204;
      }
      break;
  case numSyncModes:
      // invalid
      break;
  }
  // slowsync/refresh variants are always plus one, except 206 --> 201 (same as client initiated slow sync)
  if (fSlowSync) alertcode = (alertcode!=206 ? alertcode+1 : 201);
  return alertcode;
} // TLocalEngineDS::getSyncStateAlertCode


/// initializes Sync state variables and returns false if alert is not supported
localstatus TLocalEngineDS::setSyncModeFromAlertCode(uInt16 aAlertCode, bool aAsClient)
{
  localstatus sta;
  TSyncModes syncMode;
  bool slowSync, serverAlerted;
  // - translate into mode and flags
  sta=getSyncModeFromAlertCode(aAlertCode,syncMode,slowSync,serverAlerted);
  if (sta==LOCERR_OK) {
    // - set them
    sta=setSyncMode(aAsClient,syncMode,slowSync,serverAlerted);
  }
  return sta;
} // TLocalEngineDS::setSyncModeFromAlertCode


/// initializes Sync mode variables
localstatus TLocalEngineDS::setSyncMode(bool aAsClient, TSyncModes aSyncMode, bool aIsSlowSync, bool aIsServerAlerted)
{
  // get sync caps of this datastore
  uInt32 synccapmask=getSyncCapMask();
  // check if mode supported
  if (aIsServerAlerted) {
    // check if we support server alerted modes, SyncCap/Bit=7
    if (~synccapmask & (1<<7)) return 406; // not supported
  }
  switch(aSyncMode) {
    case smo_twoway:
      // Two-way Sync, SyncCap/Bit=1
      // or Two-way slow Sync, SyncCap/Bit=2
      if (~synccapmask & (1<< (aIsSlowSync ? 2 : 1))) return 406; // not supported
      if (fSyncMode==smo_fromserver && aAsClient)
        aSyncMode=smo_fromserver; // for client, if already fromserver mode set, keep it
      break;
    case smo_fromclient:
      // One-way from client, SyncCap/Bit=3
      // or Refresh (=slow one-way) from client, SyncCap/Bit=4
      if (~synccapmask & (1<< (aIsSlowSync ? 4 : 3))) return 406; // not supported
      if (!aAsClient) fRefreshOnly=true; // as server, we are in refresh-only-mode if we get one-way from client
      break;
    case smo_fromserver:
      // One-way from server, SyncCap/Bit=5
      // or Refresh (=slow one-way) from server, SyncCap/Bit=6
      if (~synccapmask & (1<< (aIsSlowSync ? 6 : 5))) return 406; // not supported
      if (aAsClient) fRefreshOnly=true; // as client, we are in refresh-only-mode if we get one-way fromm server
      break;
    default:
      return 400; // bad request
  }
  // now set mode and flags (possibly adjusted above)
  fSyncMode=aSyncMode;
  fSlowSync=aIsSlowSync;
  fServerAlerted=aIsServerAlerted;
  // ok
  return LOCERR_OK;
} // TLocalEngineDS::setSyncMode


/// get Sync mode variables from given alert code
localstatus TLocalEngineDS::getSyncModeFromAlertCode(uInt16 aAlertCode, TSyncModes &aSyncMode, bool &aIsSlowSync, bool &aIsServerAlerted)
{
  // these might be pre-defined)
  /// @deprecated state change does not belong here
  aIsSlowSync=false;
  aIsServerAlerted=false;
  aSyncMode=smo_twoway; // to make sure it is valid
  // first test if server-alerted
  if (aAlertCode>=206 && aAlertCode<210) {
    // Server alerted modes
    aIsServerAlerted=true;
  }
  // test for compatibility with alert code
  switch(aAlertCode) {
    case 200:
    case 206:
      // Two-way Sync
      aSyncMode=smo_twoway;
      aIsSlowSync=false;
      break;
    case 201:
      // Two-way slow Sync
      aSyncMode=smo_twoway;
      aIsSlowSync=true;
      break;
    case 202:
    case 207:
      // One-way from client
      aSyncMode=smo_fromclient;
      aIsSlowSync=false;
      break;
    case 203:
    case 208:
      // refresh (=slow one-way) from client
      aSyncMode=smo_fromclient;
      aIsSlowSync=true;
      break;
    case 204:
    case 209:
      // One-way from server
      aSyncMode=smo_fromserver;
      aIsSlowSync=false;
      break;
    case 205:
    case 210:
      // refresh (=slow one-way) from server
      aSyncMode=smo_fromserver;
      aIsSlowSync=true;
      break;
    default:
      // bad alert
      return 400;
  }
  return LOCERR_OK;
} // TLocalEngineDS::getSyncModeFromAlertCode


// create new Sync capabilities info from capabilities mask
// Bit0=reserved, Bit1..Bitn = SyncType 1..n available
// Note: derived classes might add special sync codes and/or mask standard ones
SmlDevInfSyncCapPtr_t TLocalEngineDS::newDevInfSyncCap(uInt32 aSyncCapMask)
{
  SmlDevInfSyncCapPtr_t synccapP;
  SmlPcdataPtr_t synctypeP;

  // new synccap structure
  synccapP = SML_NEW(SmlDevInfSyncCap_t);
  // synccap list is empty
  synccapP->synctype=NULL;
  // now add standard synccaps
  for (sInt16 k=0; k<32; k++) {
    if (aSyncCapMask & (1<<k)) {
      // capability available
      synctypeP=newPCDataLong(k);
      addPCDataToList(synctypeP,&(synccapP->synctype));
    }
  }
  // Now add non-standard synccaps.
  // From the spec: "Other values can also be specified."
  // Values are PCDATA, so we can use plain strings.
  //
  // But the Funambol server expects integer numbers and
  // throws a parser error when sent a string. So better
  // stick to a semi-random number (hopefully no-one else
  // is using it).
  //
  // Worse, Nokia phones cancel direct sync sessions with an
  // OBEX error ("Forbidden") when non-standard sync modes
  // are included in the SyncCap. As a workaround for that
  // we use the following logic:
  // - libsynthesis in a SyncML client will always send
  //   all the extended sync modes; with the Funambol
  //   workaround in place that works
  // - libsynthesis in a SyncML server will only send the
  //   extended sync modes if the client has sent any
  //   extended sync modes itself; the 390002 mode is
  //   sent unconditionally for that purpose
  //
  // Corresponding code in TRemoteDataStore::setDatastoreDevInf().
  //
  if (!IS_SERVER ||
      fSessionP->receivedSyncModeExtensions()) {
    bool extended=false;
    if (canRestart()) {
      synctypeP=newPCDataString("390001");
      addPCDataToList(synctypeP,&(synccapP->synctype));
      extended=true;
    }
    // Finally add non-standard synccaps that are outside of the
    // engine's control.
    set<string> modes;
    getSyncModes(modes);
    for (set<string>::const_iterator it = modes.begin();
         it != modes.end();
         ++it) {
      synctypeP=newPCDataString(*it);
      addPCDataToList(synctypeP,&(synccapP->synctype));
      extended=true;
    }

    // Add fake mode to signal peer that we support extensions.
    // Otherwise a server won't send them, to avoid breaking
    // client's (like Nokia phones) which don't.
    //
    // Don't send the fake mode unnecessarily (= when some other
    // non-standard modes where already added), because Funambol seems
    // to have a hard-coded limit of 9 entries in the <SyncCap> and
    // complains with a 513 internal server error (when using WBXML)
    // or a 'Expected "CTCap" end tag, found "CTType" end tag' (when
    // using XML).
    if (!extended) {
      synctypeP=newPCDataString("390002");
      addPCDataToList(synctypeP,&(synccapP->synctype));
    }
  }

  // return it
  return synccapP;
} // TLocalEngineDS::newDevInfSyncCap


// obtain new datastore info, returns NULL if none available
SmlDevInfDatastorePtr_t TLocalEngineDS::newDevInfDatastore(bool aAsServer, bool aWithoutCTCapProps)
{
  SmlDevInfDatastorePtr_t datastoreP;

  // set only basic info, details must be added in derived class
  // - sourceref is the name of the datastore,
  //   or for server, if already alerted, the name used in the alert
  //   (this is to allow /dsname/foldername with clients that expect the
  //   devInf to contain exactly the same full path as name, like newer Nokias)
  string dotname;
  #ifdef SYSYNC_SERVER
  if (IS_SERVER && testState(dssta_serveralerted,false) && fSessionP->fDSPathInDevInf) {
    // server and already alerted
    // - don't include sub-datastores
    if (fAsSubDatastoreOf) {
      return NULL;
    }

    // - use datastore spec as sent from remote, minus CGI, as relative spec
    dotname = URI_RELPREFIX;
    dotname += fSessionP->SessionRelativeURI(fRemoteViewOfLocalURI.c_str());
    if (!fSessionP->fDSCgiInDevInf) {
      // remove CGI
      string::size_type n=dotname.find("?");
      if (n!=string::npos)
        dotname.resize(n); // remove CGI
    }
  }
  else
  #endif
  {
    // client or not yet alerted - just use datastore base name
    StringObjPrintf(dotname,URI_RELPREFIX "%s",fName.c_str());
  }

  // create new
  datastoreP=SML_NEW(SmlDevInfDatastore_t);
  datastoreP->sourceref=newPCDataString(dotname);
  #ifndef MINIMAL_CODE
  // - Optional display name
  datastoreP->displayname=newPCDataOptString(getDisplayName());
  #else
  datastoreP->displayname=NULL;
  #endif
  // - MaxGUIDsize (for client only)
  if (aAsServer)
    datastoreP->maxguidsize=NULL;
  else
    datastoreP->maxguidsize=newPCDataLong(fMaxGUIDSize);
  // - check for legacy mode type (that is to be used as "preferred" instead of normal preferred)
  TSyncItemType *legacyTypeP = NULL;
  if (getSession()->fLegacyMode && getDSConfig()->fTypeSupport.fPreferredLegacy) {
    // get the type marked as blind
    legacyTypeP = getSession()->findLocalType(getDSConfig()->fTypeSupport.fPreferredLegacy);
  }
  // - RxPref
  if (!fRxPrefItemTypeP) SYSYNC_THROW(TStructException("Datastore has no RxPref ItemType"));
  datastoreP->rxpref = (legacyTypeP ? legacyTypeP : fRxPrefItemTypeP)->newXMitDevInf();
  // - Rx (excluding the type we report as preferred)
  datastoreP->rx=TSyncItemType::newXMitListDevInf(fRxItemTypes,legacyTypeP ? legacyTypeP : fRxPrefItemTypeP);
  // - TxPref
  if (!fTxPrefItemTypeP) SYSYNC_THROW(TStructException("Datastore has no TxPref ItemType"));
  datastoreP->txpref = (legacyTypeP ? legacyTypeP : fTxPrefItemTypeP)->newXMitDevInf();
  // - Tx (excluding the type we report as preferred)
  datastoreP->tx=TSyncItemType::newXMitListDevInf(fTxItemTypes,legacyTypeP ? legacyTypeP : fTxPrefItemTypeP);
  // - DSMem
  /// @todo %%% tbd: add dsmem
  datastoreP->dsmem=NULL;
  // - SyncML DS 1.2 datastore-local CTCap
  if (fSessionP->getSyncMLVersion()>=syncml_vers_1_2) {
    // CTCap is local to datastore, get only CTCaps relevant for this datastore, but independently from alerted state
    datastoreP->ctcap = fSessionP->newLocalCTCapList(false, this, aWithoutCTCapProps);
  }
  else
    datastoreP->ctcap=NULL; // before SyncML 1.2, there was no datastore-local CTCap
  // - SyncML DS 1.2 flags (SmlDevInfHierarchical_f)
  /// @todo %%% tbd: add SmlDevInfHierarchical_f
  datastoreP->flags=0;
  // - SyncML DS 1.2 filters
  datastoreP->filterrx=NULL;
  datastoreP->filtercap=NULL;
  #ifdef OBJECT_FILTERING
  if (IS_SERVER && fDSConfigP->fDS12FilterSupport && fSessionP->getSyncMLVersion()>=syncml_vers_1_2) {
    // Show Filter info in 1.2 devInf if this is not a client
    // - FilterRx constant
    datastoreP->filterrx = SML_NEW(SmlDevInfXmitList_t);
    datastoreP->filterrx->next = NULL;
    datastoreP->filterrx->data = SML_NEW(SmlDevInfXmit_t);
    datastoreP->filterrx->data->cttype=newPCDataString(SYNCML_FILTERTYPE_CGI);
    datastoreP->filterrx->data->verct=newPCDataString(SYNCML_FILTERTYPE_CGI_VERS);
    // build filtercap
    SmlPcdataListPtr_t filterkeywords = NULL;
    SmlPcdataListPtr_t filterprops = NULL;
    // - fill the lists from types
    addFilterCapPropsAndKeywords(filterkeywords,filterprops);
    // - if we have something, actually build a filtercap
    if (filterkeywords || filterprops) {
      // we have filtercaps, add them
      // - FilterCap list
      datastoreP->filtercap = SML_NEW(SmlDevInfFilterCapList_t);
      datastoreP->filtercap->next = NULL;
      datastoreP->filtercap->data = SML_NEW(SmlDevInfFilterCap_t);
      datastoreP->filtercap->data->cttype=newPCDataString(SYNCML_FILTERTYPE_CGI);
      datastoreP->filtercap->data->verct=newPCDataString(SYNCML_FILTERTYPE_CGI_VERS);
      // - add list we've got
      datastoreP->filtercap->data->filterkeyword=filterkeywords;
      datastoreP->filtercap->data->propname=filterprops;
    }
  }
  #endif // OBJECT_FILTERING
  // - Sync capabilities of this datastore
  datastoreP->synccap=newDevInfSyncCap(getSyncCapMask());
  // return it
  return datastoreP;
} // TLocalEngineDS::newDevInfDatastore


// Set remote datastore for local
void TLocalEngineDS::engSetRemoteDatastore(
  TRemoteDataStore *aRemoteDatastoreP  // the remote datastore involved
)
{
  // save link to remote datastore
  if (fRemoteDatastoreP) {
    if (fRemoteDatastoreP!=aRemoteDatastoreP)
      SYSYNC_THROW(TSyncException("Sync continues with different datastore"));
  }
  fRemoteDatastoreP=aRemoteDatastoreP;
} // TLocalEngineDS::engSetRemoteDatastore


// SYNC command bracket start (check credentials if needed)
bool TLocalEngineDS::engProcessSyncCmd(
  SmlSyncPtr_t aSyncP,                // the Sync element
  TStatusCommand &aStatusCommand,     // status that might be modified
  bool &aQueueForLater // will be set if command must be queued for later (re-)execution
)
{
  // get number of changes info if available
  if (aSyncP->noc) {
    StrToLong(smlPCDataToCharP(aSyncP->noc),fRemoteNumberOfChanges);
  }
  // check if datastore is aborted
  if (CheckAborted(aStatusCommand)) return false;
  // check
  if (!fRemoteDatastoreP)
    SYSYNC_THROW(TSyncException("No remote datastore linked"));
  // let remote datastore process it first
  if (!fRemoteDatastoreP->remoteProcessSyncCmd(aSyncP,aStatusCommand,aQueueForLater)) {
    PDEBUGPRINTFX(DBG_ERROR,("TLocalEngineDS::engProcessSyncCmd: remote datastore failed processing <sync>"));
    changeState(dssta_idle,true); // force it
    return false;
  }
  // check for combined init&sync
  if (!testState(dssta_syncmodestable,false)) {
    // <sync> encountered before sync mode stable: could be combined init&sync session
    if (fLocalDSState>=dssta_serveransweredalert) {
      // ok for switching to combined init&sync
      PDEBUGPRINTFX(DBG_HOT,("TLocalEngineDS::engProcessSyncCmd: detected combined init&sync, freeze sync mode already now"));
      // - freeze sync mode as it is now
      changeState(dssta_syncmodestable,true); // force it, sync mode is now stable, no further changes are possible
    }
  }
  // now init if this is the first <sync> command
  bool startingNow = false; // assume start already initiated
  if (testState(dssta_syncmodestable,true)) {
    // all alert and alert status must be done by now, sync mode must be stable
    CONSOLEPRINTF(("- Starting Sync with Datastore '%s', %s sync",fRemoteViewOfLocalURI.c_str(),fSlowSync ? "slow" : "normal"));
    startingNow = true; // initiating start now
    #ifdef SYSYNC_SERVER
    if (IS_SERVER) {
      // at this point, all temporary GUIDs become invalid (no "early map" possible any more which might refer to last session's tempGUIDs)
      fTempGUIDMap.clear(); // forget all previous session's temp GUID mappings
      // let local datastore (derived DB-specific class) prepare for sync
      localstatus sta = changeState(dssta_dataaccessstarted);
      if (sta!=LOCERR_OK) {
        // abort session (old comment: %%% aborting datastore only does not work, will loop, why? %%%)
        aStatusCommand.setStatusCode(syncmlError(sta));
        PDEBUGPRINTFX(DBG_ERROR,("TLocalEngineDS::engProcessSyncCmd: could not change state to dsssta_dataaccessstarted -> abort"));
        engAbortDataStoreSync(sta,true); // local problem
        return false;
      }
    }
    #endif
  }
  // if data access started (finished or not), check start status
  // for every execution and re-execution of the sync command
  if (testState(dssta_dataaccessstarted)) {
    // queue <sync> command if datastore is not yet started already
    if (engIsStarted(!startingNow)) { // wait only if start was already initiated
      // - is now initialized
      if (IS_SERVER) {
        // - for server, make the sync set ready now (as engine is now started)
        changeState(dssta_syncsetready,true); // force it
      }
      else {
        // - for client, we need at least dssta_syncgendone (sync set has been ready long ago, we've already sent changes to server!)
        if (!testState(dssta_syncgendone)) {
          // bad sequence of commands (<sync> from server too early!)
          aStatusCommand.setStatusCode(400);
          PDEBUGPRINTFX(DBG_ERROR,("TLocalEngineDS::engProcessSyncCmd: client received SYNC before sending SYNC complete"));
          engAbortDataStoreSync(400,false,false); // remote problem, not resumable
          return false;
        }
      }
      PDEBUGPRINTFX(DBG_HOT,(
        "- Started %s Sync (first <sync> command)",
        fSlowSync ? "slow" : "normal"
      ));
      if (fRemoteNumberOfChanges>=0) PDEBUGPRINTFX(DBG_HOT,("- NumberOfChanges announced by remote = %ld",(long)fRemoteNumberOfChanges));
      DB_PROGRESS_EVENT(this,pev_syncstart,0,0,0);
    }
    else {
      // - not yet started
      PDEBUGPRINTFX(DBG_HOT,(
        "- Starting sync not yet complete - re-execute <sync> command again in next message"
      ));
      aQueueForLater=true;
      return true; // ok so far
    }
  }
  // must be syncready here (otherwise we return before we reach this)
  if (!testState(dssta_syncsetready)) {
    aStatusCommand.setStatusCode(403); // forbidden
    ADDDEBUGITEM(aStatusCommand,"SYNC received too early");
    PDEBUGPRINTFX(DBG_ERROR,("TLocalEngineDS::engProcessSyncCmd: SYNC received too early"));
    engAbortDataStoreSync(403,false,false); // remote problem, not resumable
    return false;
  }
  // state is now syncing
  /// @deprecated - dssta_syncsetready is enough
  //fState=dss_syncing;
  return true;
} // TLocalEngineDS::engProcessSyncCmd


// SYNC command bracket end (but another might follow in next message)
bool TLocalEngineDS::engProcessSyncCmdEnd(bool &aQueueForLater)
{
  bool ok=true;
  // queue it for later as long as datastore is not ready now
  if (!engIsStarted(false)) { // not waiting if not started
    // no state change, just postpone execution
    aQueueForLater=true;
  }
  // also inform remote
  if (fRemoteDatastoreP) ok=fRemoteDatastoreP->remoteProcessSyncCmdEnd();
  return ok;
} // TLocalEngineDS::engProcessSyncCmdEnd


#ifdef SYSYNC_SERVER

// server case: called whenever outgoing Message of Sync Package starts
void TLocalEngineDS::engServerStartOfSyncMessage(void)
{
  // this is where we might start our own <Sync> command (all
  // received commands are now processed)
  // - Note that this might be a subdatastore -> if so, do NOT
  //   start a sync (superdatastore will handle this)
  // - Note that this will be called even if current message is
  //   already full, so it could well be that this sync command
  //   is queued.
  if (!fSessionP->fCompleteFromClientOnly && testState(dssta_serverseenclientmods) && getSyncMode()==smo_fromclient) {
    // from-client only does not send back a <sync> command, simply end data access here
    PDEBUGPRINTFX(DBG_PROTO,("from-client-only:do not send <sync> command back to client, data access ends here"));
    changeState(dssta_syncgendone,true);
    changeState(dssta_dataaccessdone,true);
  }
  // ### SyncFest #5, found with Tactel Jazz Client:
  // - do not send anything when remote datastore is not known
  else if (fRemoteDatastoreP) {
    if (!testState(dssta_serversyncgenstarted) && testState(dssta_serverseenclientmods)) {
      changeState(dssta_serversyncgenstarted,true);
      if (!isSubDatastore()) {
        // - Note: if sync command was already started, the
        //   finished(), continueIssue() mechanism will make sure that
        //   more commands are generated
        // - Note2: if all sync commands can be sent at once,
        //   fState will be modified by issuing <sync>, so
        //   it must be ok for issuing syncops here already!
        TSyncCommand *synccmdP =
          new TSyncCommand(
            fSessionP,
            this,               // local datastore
            fRemoteDatastoreP   // remote datastore
          );
        // issue
        ISSUE_COMMAND_ROOT(fSessionP,synccmdP);
      }
    }
  }
  else {
    changeState(dssta_idle,true); // force it
  }
} // TLocalEngineDS::engServerStartOfSyncMessage


#endif // server only


// called whenever Message of Sync Package ends or after last queued Sync command is executed
// - aEndOfAllSyncCommands is true when at end of Sync-data-from-remote packet
//   AND all possibly queued sync/syncop commands have been processed.
void TLocalEngineDS::engEndOfSyncFromRemote(bool aEndOfAllSyncCommands)
{
  // is called for all local datastores, including superdatastore, even inactive ones, so check state first
  if (testState(dssta_syncsetready)) {
    if (aEndOfAllSyncCommands) {
      // we are at end of sync-data-from-remote for THIS datastore
      if (IS_CLIENT) {
        // - we are done with <Sync> from server, that is, data access is done now
        changeState(dssta_dataaccessdone,true); // force it
      }
      else {
        // - server has seen all client modifications now
        // Note: in case of the simulated-empty-sync-hack in action, we
        //       allow that we are already in server_sync_gen_started and
        //       won't try to force down to dssta_serverseenclientmods
        if (!fSessionP->fFakeFinalFlag || getDSState()<dssta_serverseenclientmods) {
          // under normal circumstances, wee need dssta_serverseenclientmods here
          changeState(dssta_serverseenclientmods,true); // force it
        }
        else {
          // hack exception
          PDEBUGPRINTFX(DBG_ERROR,("Warning: simulated </final> active - allowing state>server_seen_client_mods"));
        }
      }
    }
    #ifdef SYSYNC_SERVER
    if (IS_SERVER) {
      engServerStartOfSyncMessage();
    }
    #endif
    // now do final things
    if (testState(dssta_dataaccessdone,true)) {
      // @todo: I think, as long as we need to send maps, we're not done yet!!!
      // sync packets in both directions done, forget remote datastore
      fRemoteDatastoreP=NULL;
    }
  } // dssta_syncsetready
} // TLocalEngineDS::engEndOfSyncFromRemote


// - must return true if this datastore is finished with <sync>
//   (if all datastores return true,
//   session is allowed to finish sync packet with outgoing message
bool TLocalEngineDS::isSyncDone(void)
{
  // is called for all local datastores, even inactive ones, which must signal sync done, too
  // - only datastores currently receiving or sending <sync> commands are not done with sync
  // - aborted datastores are also done with sync, no matter what status they have
  return (
    fAbortStatusCode!=0 ||
    //(fState!=dss_syncsend && fState!=dss_syncing && fState!=dss_syncready && fState!=dss_syncfinish)
    !testState(dssta_clientsentalert) || // nothing really happened yet...
    testState(dssta_syncgendone) // ...or already completed generating <sync>
  );
} // TLocalEngineDS::isSyncDone


// test datastore state for minimal or exact state
bool TLocalEngineDS::testState(TLocalEngineDSState aMinState, bool aNeedExactMatch)
{
  bool result =
    (!aNeedExactMatch || (fLocalDSState==aMinState)) &&
    (fLocalDSState>=aMinState);
  DEBUGPRINTFX(DBG_EXOTIC,(
    "%s: testState=%s - expected state%c='%s', found state=='%s'",
    getName(),
    result ? "TRUE" : "FALSE",
    aNeedExactMatch ? '=' : '>',
    getDSStateName(aMinState),
    getDSStateName()
  ));
  return result;
} // TLocalEngineDS::testState


// change datastore state, calls logic layer before and after change
localstatus TLocalEngineDS::changeState(TLocalEngineDSState aNewState, bool aForceOnError)
{
  localstatus err1,err2;
  TLocalEngineDSState oldState = fLocalDSState;

  // nop if no change in state
  if (aNewState==oldState) return LOCERR_OK;
  // state cannot be decremented except down to adminready and below
  if ((aNewState<oldState) && (aNewState>dssta_adminready)) {
    PDEBUGPRINTFX(DBG_ERROR,(
      "%s: Internal error: attempt to reduce state from '%s' to '%s' - aborting",
      getName(),
      getDSStateName(oldState),
      getDSStateName(aNewState)
    ));
    err1 = 500;
    dsAbortDatastoreSync(err1,true);
    return err1;
  }
  // give logic opportunity to react before state changes
  PDEBUGBLOCKFMT((
    "DSStateChange",
    "Datastore changes state",
    "datastore=%s|oldstate=%s|newstate=%s",
    getName(),
    getDSStateName(oldState),
    getDSStateName(aNewState)
  ));
  err2 = LOCERR_OK;
  err1 = dsBeforeStateChange(oldState,aNewState);
  if (!aForceOnError && err1) goto endchange;
  // switch state
  fLocalDSState = aNewState;

  if (aNewState == dssta_syncmodestable) {
    // There are multiple places where the sync mode is frozen.  Ensure
    // that this change is reported in all of them by putting the code
    // here.
    PDEBUGPRINTFX(DBG_HOT,(
                         "executing %s%s%s Sync%s",
                         fResuming ? "resumed " : "",
                         fSlowSync ? "slow" : "normal",
                         fFirstTimeSync ? " first time" : "",
                         fSyncMode == smo_twoway ? ", two-way" :
                         fSyncMode == smo_fromclient ? " from client" :
                         fSyncMode == smo_fromserver ? " from server" :
                         " in unknown direction?!"
                           ));
#ifdef PROGRESS_EVENTS
    // progress event
    DB_PROGRESS_EVENT(this,
                      pev_alerted,
                      fSlowSync ? (fFirstTimeSync ? 2 : 1) : 0,
                      fResuming ? 1 : 0,
                      fSyncMode
                      );
#endif // PROGRESS_EVENTS
  }

  // now give logic opportunity to react again
  err2 = dsAfterStateChange(oldState,aNewState);
endchange:
  PDEBUGENDBLOCK("DSStateChange");
  // return most recent error
  return err2 ? err2 : err1;
} // TLocalEngineDS::changeState



// test datastore abort status
// datastore is aborted when
// - it was explicitly aborted (engAbortDataStoreSync() called, fAbortStatusCode set)
// - session is suspending and the datastore has not yet completed sync up to sending
//   maps (client) or admin already saved (server+client).
//   If client has sent maps, all that MIGHT be missing would be map status, and
//   if that hasn't arrived, the pendingMaps mechanism will make sure these get
//   sent in the next session.
bool TLocalEngineDS::isAborted(void)
{
  return fAbortStatusCode!=0 || (fSessionP->isSuspending() && !testState(dssta_clientmapssent));
} // TLocalEngineDS::isAborted


// abort sync with this datastore
void TLocalEngineDS::engAbortDataStoreSync(TSyError aStatusCode, bool aLocalProblem, bool aResumable)
{
  if (fLocalDSState!=dssta_idle && !fAbortStatusCode) {
    // prepare status
    fAbortStatusCode = aStatusCode ? aStatusCode : 514; // make sure we have a non-zero fAbortStatusCode
    fLocalAbortCause = aLocalProblem;
    if (!aResumable) preventResuming(); // prevent resuming
    PDEBUGBLOCKFMT((
      "DSAbort","Aborting datastore sync","abortStatusCode=%hd|localProblem=%s|resumable=%s",
      aStatusCode,
      aLocalProblem ? "yes" : "no",
      aResumable ? "yes" : "no"
    ));
    // tell that to the session
    fSessionP->DatastoreFailed(aStatusCode,aLocalProblem);
    // as soon as sync set is ready, we have potentially started the sync and resume makes sense
    // NOTE: before we have made the sync set ready, WE MUST NOT resume, because making the sync
    //   set ready includes zapping it on slow refreshes, and this is only done when not resuming
    //   (so saving a suspend state before dssta_syncsetready would cause that the zapping is
    //   possibly skipped)
    if (!testState(dssta_syncsetready)) preventResuming(); // prevent resuming before sync set is ready
    // save resume (or non-resumable!) status only if this is NOT A TIMEOUT, because if it is a
    // (server) timeout, suspend state was saved at end of last request, and writing again here would destroy
    // the state.
    if (aStatusCode!=408) {
      engSaveSuspendState(true); // save even if already aborted
    }
    // let derivates know
    dsAbortDatastoreSync(aStatusCode, aLocalProblem);
    // show abort
    PDEBUGPRINTFX(DBG_ERROR,(
      "*************** Warning: Datastore flagged aborted (after %ld sec. request processing, %ld sec. total) with %s Status %hd",
      (long)((getSession()->getSystemNowAs(TCTX_UTC)-fSessionP->getLastRequestStarted()) / secondToLinearTimeFactor),
      (long)((getSession()->getSystemNowAs(TCTX_UTC)-fSessionP->getSessionStarted()) / secondToLinearTimeFactor),
      aLocalProblem ? "LOCAL" : "REMOTE",
      aStatusCode
    ));
    DB_PROGRESS_EVENT(
      this,
      pev_syncend,
      getAbortStatusCode(),
      fSlowSync ? (fFirstTimeSync ? 2 : 1) : 0,
      fResuming ? 1 : 0
    );
    PDEBUGENDBLOCK("DSAbort");
  }
} // TLocalEngineDS::engAbortDataStoreSync


// check if aborted, set status to abort reason code if yes
bool TLocalEngineDS::CheckAborted(TStatusCommand &aStatusCommand)
{
  if (fAbortStatusCode!=0) {
    aStatusCommand.setStatusCode(
      fSessionP->getSyncMLVersion()>=syncml_vers_1_1 ? 514 : // cancelled
        (fAbortStatusCode<LOCAL_STATUS_CODE ? fAbortStatusCode : 512) // sync failed
    );
    PDEBUGPRINTFX(DBG_DATA,("This datastore is in aborted state, rejects all commands with %hd",aStatusCommand.getStatusCode()));
    return true; // aborted, status set
  }
  return false; // not aborted
} // TLocalEngineDS::CheckAborted



// Do common logfile substitutions
void TLocalEngineDS::DoLogSubstitutions(string &aLog,bool aPlaintext)
{
  #ifndef MINIMAL_CODE
  string s;

  if (aPlaintext) {
    StringObjTimestamp(s,fEndOfSyncTime);
    // %T Time of sync (in derived datastores, this is the point of reference for newer/older comparisons) as plain text
    StringSubst(aLog,"%T",s,2);
    // %seT Time of session end (with this datastore) as plain text
    StringSubst(aLog,"%seT",s,4);
    // %ssT Time of session start as plain text
    StringObjTimestamp(s,fSessionP->getSessionStarted());
    StringSubst(aLog,"%ssT",s,4);
  }
  // %sdT sync duration (in seconds) for this datastore (start of session until datastore finished)
  StringSubst(aLog,"%sdT",((sInt32)(fEndOfSyncTime-fSessionP->getSessionStarted())/secondToLinearTimeFactor),4);
  // %nD  Datastore name
  StringSubst(aLog,"%nD",getName(),3);
  // %rD  Datastore remote path
  StringSubst(aLog,"%rD",fRemoteDBPath,3);
  // %lD  Datastore local path (complete with all CGI)
  StringSubst(aLog,"%lD",fRemoteViewOfLocalURI,3);
  // %iR  Remote Device ID (URI)
  StringSubst(aLog,"%iR",fSessionP->getRemoteURI(),3);
  // %nR  Remote name: [Manufacturer ]Model")
  StringSubst(aLog,"%nR",fSessionP->getRemoteDescName(),3);
  // %vR  Remote Device Version Info ("Type (HWV, FWV, SWV) Oem")
  StringSubst(aLog,"%vR",fSessionP->getRemoteInfoString(),3);
  // %U User Name
  StringSubst(aLog,"%U",fSessionP->getSyncUserName(),2);
  // %iS local Session ID
  StringSubst(aLog,"%iS",fSessionP->getLocalSessionID(),3);
  // %sS  Status code (0 if successful)
  StringSubst(aLog,"%sS",fAbortStatusCode,3);
  // %ssS Session Status code (0 if successful)
  StringSubst(aLog,"%ssS",fSessionP->getAbortReasonStatus(),4);
  // %syV SyncML version (as text) of session
  StringSubst(aLog,"%syV",SyncMLVerDTDNames[fSessionP->getSyncMLVersion()],4);
  // %syV SyncML version numeric (0=unknown, 1=1.0, 2=1.1, 3=1.2) of session
  StringSubst(aLog,"%syVn",(long)fSessionP->getSyncMLVersion(),5);
  // %mS  Syncmode (0=twoway, 1=fromclient 2=fromserver)
  StringSubst(aLog,"%mS",(sInt32)fSyncMode,3);
  // %tS  Synctype (0=normal,1=slow,2=firsttime slow, +10 if resumed session)
  StringSubst(aLog,"%tS",(fSlowSync ? (fFirstTimeSync ? 2 : 1) : 0) + (isResuming() ? 10 : 0),3);
  // %laI locally added Items
  StringSubst(aLog,"%laI",fLocalItemsAdded,4);
  // %raI remotely added Items
  StringSubst(aLog,"%raI",fRemoteItemsAdded,4);
  // %ldI locally deleted Items
  StringSubst(aLog,"%ldI",fLocalItemsDeleted,4);
  // %rdI remotely deleted Items
  StringSubst(aLog,"%rdI",fRemoteItemsDeleted,4);
  // %luI locally updated Items
  StringSubst(aLog,"%luI",fLocalItemsUpdated,4);
  // %ruI remotely updated Items
  StringSubst(aLog,"%ruI",fRemoteItemsUpdated,4);
  // %reI locally not accepted Items (sent error to remote, remote MAY resend them or abort the session)
  StringSubst(aLog,"%leI",fLocalItemsError,4);
  // %leI remotely not accepted Items (got error from remote, local will resend them later)
  StringSubst(aLog,"%reI",fRemoteItemsError,4);
  #ifdef SYSYNC_SERVER
  if (IS_SERVER) {
    // %smI Slowsync matched Items
    StringSubst(aLog,"%smI",fSlowSyncMatches,4);
    // %scI Server won Conflicts
    StringSubst(aLog,"%scI",fConflictsServerWins,4);
    // %ccI Client won Conflicts
    StringSubst(aLog,"%ccI",fConflictsClientWins,4);
    // %dcI Conflicts with duplications
    StringSubst(aLog,"%dcI",fConflictsDuplicated,4);
    // %tiB total incoming bytes
    StringSubst(aLog,"%tiB",fSessionP->getIncomingBytes(),4);
    // %toB total outgoing bytes
    StringSubst(aLog,"%toB",fSessionP->getOutgoingBytes(),4);
  }
  #endif
  // %niB net incoming data bytes for this datastore
  StringSubst(aLog,"%diB",fIncomingDataBytes,4);
  // %noB net incoming data bytes for this datastore
  StringSubst(aLog,"%doB",fOutgoingDataBytes,4);
  #endif
} // TLocalEngineDS::DoLogSubstitutions


// log datastore sync result
// - Called at end of sync with this datastore
void TLocalEngineDS::dsLogSyncResult(void)
{
  #ifndef MINIMAL_CODE
  if (fSessionP->logEnabled()) {
    string logtext;
    logtext=fSessionP->getSessionConfig()->fLogFileFormat;
    if (!logtext.empty()) {
      // substitute
      DoLogSubstitutions(logtext,true); // plaintext
      // show
      fSessionP->WriteLogLine(logtext.c_str());
    }
  }
  #endif
} // TLocalEngineDS::dsLogSyncResult



// Terminate all activity with this datastore
// Note: may be called repeatedly, must only execute relevant shutdown code once
void TLocalEngineDS::engTerminateDatastore(localstatus aAbortStatusCode)
{
  // now abort (if not already aborted), then finish activities
  engFinishDataStoreSync(aAbortStatusCode);
  // and finally reset completely
  engResetDataStore();
} // TLocalEngineDS::TerminateDatastore


// called at very end of sync session, when everything is done
// Note: is also called before deleting a datastore (so aborted sessions
//   can do cleanup and/or statistics display as well)
void TLocalEngineDS::engFinishDataStoreSync(localstatus aErrorStatus)
{
  // set end of sync time
  fEndOfSyncTime = getSession()->getSystemNowAs(TCTX_UTC);
  // check if we have something to do at all
  if (fLocalDSState!=dssta_idle && fLocalDSState!=dssta_completed) {
    if (aErrorStatus==LOCERR_OK) {
      // Check if we need to abort now due to failed items only
      if (fRemoteItemsError>0) {
        // remote reported errors
        if (fSlowSync && fRemoteItemsAdded==0 && fRemoteItemsDeleted==0 && fRemoteItemsUpdated==0 && fSessionP->getSessionConfig()->fAbortOnAllItemsFailed) {
          PDEBUGPRINTFX(DBG_ERROR+DBG_DETAILS,("All remote item operations failed -> abort sync"));
          engAbortDataStoreSync(512,false,false); // remote problems (only failed items in a slow sync) caused sync to fail, not resumable
        }
        else
          fSessionP->DatastoreHadErrors(); // at least SOME items were successful, so it's not a completely unsuccessful sync
      }
    }
    // abort, if requested from caller or only-failed-items
    if (aErrorStatus!=LOCERR_OK)
      engAbortDataStoreSync(aErrorStatus,true); // if we have an error here, this is considered a local problem
    else {
      DB_PROGRESS_EVENT(
        this,
        pev_syncend,
        fAbortStatusCode,
        fSlowSync ? (fFirstTimeSync ? 2 : 1) : 0,
        fResuming ? 1 : 0
      );
    }
    #ifdef SUPERDATASTORES
    // if this is part of a superdatastore, include its statistics into mine, as
    // superdatastore can not save any statistics.
    // This ensures that the result sum over all subdatastores is correct,
    // however the assignment of error and byte counts is not (all non-related
    // counts go to first subdatastores with the following code)
    if (fAsSubDatastoreOf) {
      fOutgoingDataBytes += fAsSubDatastoreOf->fOutgoingDataBytes;
      fIncomingDataBytes += fAsSubDatastoreOf->fIncomingDataBytes;
      fRemoteItemsError += fAsSubDatastoreOf->fRemoteItemsError;
      fLocalItemsError += fAsSubDatastoreOf->fLocalItemsError;
      // consumed now, clear in superdatastore
      fAsSubDatastoreOf->fOutgoingDataBytes=0;
      fAsSubDatastoreOf->fIncomingDataBytes=0;
      fAsSubDatastoreOf->fRemoteItemsError=0;
      fAsSubDatastoreOf->fLocalItemsError=0;
    }
    #endif
    // make log entry
    dsLogSyncResult();
    // update my session state vars for successful sessions
    if (aErrorStatus==LOCERR_OK) {
      // update anchor
      fLastRemoteAnchor=fNextRemoteAnchor;
      fLastLocalAnchor=fNextLocalAnchor; // note: when using TStdLogicDS, this is not saved, but re-generated at next sync from timestamp
      // no resume
      fResumeAlertCode=0;
      // no resume item (just to make sure we don't get strange effects later)
      fLastItemStatus = 0;
      fLastSourceURI.erase();
      fLastTargetURI.erase();
      fPartialItemState = pi_state_none;
      fPIStoredSize = 0;
    }
    // now shift state to complete, let logic and implementation save the state
    changeState(dssta_completed,true);
    #ifdef SCRIPT_SUPPORT
    // - call DB finish script
    TScriptContext::execute(
      fDataStoreScriptContextP,
      fDSConfigP->fDBFinishScript,
      &DBFuncTable, // context's function table
      this // datastore pointer needed for context
    );
    #endif
  }
  // in any case: idle now again (note: could be shift from dssta_completed to dssta_idle)
  changeState(dssta_idle,true);
} // TLocalEngineDS::engFinishDataStoreSync


/// inform everyone of coming state change
localstatus TLocalEngineDS::dsBeforeStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState)
{
  localstatus sta = LOCERR_OK;
  return sta;
} // TLocalEngineDS::dsBeforeStateChange


/// inform everyone of happened state change
localstatus TLocalEngineDS::dsAfterStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState)
{
  localstatus sta = LOCERR_OK;
  if (aOldState>dssta_idle && aNewState==dssta_completed) {
    // we are going from a non-idle state to completed
    // - show statistics
    showStatistics();
  }
  return sta;
} // TLocalEngineDS::dsAfterStateChange


// show statistics or error of current sync
void TLocalEngineDS::showStatistics(void)
{
  // Console
  CONSOLEPRINTF((""));
  CONSOLEPRINTF(("- Sync Statistics for '%s' (%s), %s sync",
    getName(),
    fRemoteViewOfLocalURI.c_str(),
    fSlowSync ? "slow" : "normal"
  ));
  // now show results
  if (isAborted()) {
    // failed
    CONSOLEPRINTF(("  ************ Failed with status code=%hd",fAbortStatusCode));
  }
  else {
    // successful: show statistics on console
    CONSOLEPRINTF(("  =================================================="));
    if (IS_SERVER) {
      CONSOLEPRINTF(("                               on Server   on Client"));
    }
    else {
      CONSOLEPRINTF(("                               on Client   on Server"));
    }
    CONSOLEPRINTF(("  Added:                       %9ld   %9ld",(long)fLocalItemsAdded,(long)fRemoteItemsAdded));
    CONSOLEPRINTF(("  Deleted:                     %9ld   %9ld",(long)fLocalItemsDeleted,(long)fRemoteItemsDeleted));
    CONSOLEPRINTF(("  Updated:                     %9ld   %9ld",(long)fLocalItemsUpdated,(long)fRemoteItemsUpdated));
    CONSOLEPRINTF(("  Rejected with error:         %9ld   %9ld",(long)fLocalItemsError,(long)fRemoteItemsError));
    #ifdef SYSYNC_SERVER
    if (IS_SERVER) {
      CONSOLEPRINTF(("  SlowSync Matches:            %9ld",(long)fSlowSyncMatches));
      CONSOLEPRINTF(("  Server won Conflicts:        %9ld",(long)fConflictsServerWins));
      CONSOLEPRINTF(("  Client won Conflicts:        %9ld",(long)fConflictsClientWins));
      CONSOLEPRINTF(("  Conflicts with Duplication:  %9ld",(long)fConflictsDuplicated));
    }
    #endif
  }
  CONSOLEPRINTF((""));
  // Always provide statistics as events
  DB_PROGRESS_EVENT(this,pev_dsstats_l,fLocalItemsAdded,fLocalItemsUpdated,fLocalItemsDeleted);
  DB_PROGRESS_EVENT(this,pev_dsstats_r,fRemoteItemsAdded,fRemoteItemsUpdated,fRemoteItemsDeleted);
  DB_PROGRESS_EVENT(this,pev_dsstats_e,fLocalItemsError,fRemoteItemsError,0);
  #ifdef SYSYNC_SERVER
  if (IS_SERVER) {
    DB_PROGRESS_EVENT(this,pev_dsstats_s,fSlowSyncMatches,0,0);
    DB_PROGRESS_EVENT(this,pev_dsstats_c,fConflictsServerWins,fConflictsClientWins,fConflictsDuplicated);
  }
  #endif
  // NOTE: pev_dsstats_d should remain the last log data event sent (as it terminates collecting data in some GUIs)
  DB_PROGRESS_EVENT(this,pev_dsstats_d,fOutgoingDataBytes,fIncomingDataBytes,fRemoteItemsError);
  // Always show statistics in debug log
  #ifdef SYDEBUG
  PDEBUGPRINTFX(DBG_HOT,("Sync Statistics for '%s' (%s), %s sync",
    getName(),
    fRemoteViewOfLocalURI.c_str(),
    fSlowSync ? "slow" : "normal"
  ));
  if (PDEBUGTEST(DBG_HOT)) {
    string stats =
             "==================================================\n";
    if (IS_SERVER) {
      stats += "                             on Server   on Client\n";
    }
    else {
      stats += "                             on Client   on Server\n";
    }
    StringObjAppendPrintf(stats,"Added:                       %9ld   %9ld\n",(long)fLocalItemsAdded,(long)fRemoteItemsAdded);
    StringObjAppendPrintf(stats,"Deleted:                     %9ld   %9ld\n",(long)fLocalItemsDeleted,(long)fRemoteItemsDeleted);
    StringObjAppendPrintf(stats,"Updated:                     %9ld   %9ld\n",(long)fLocalItemsUpdated,(long)fRemoteItemsUpdated);
    StringObjAppendPrintf(stats,"Rejected with error:         %9ld   %9ld\n\n",(long)fLocalItemsError,(long)fRemoteItemsError);
    #ifdef SYSYNC_SERVER
    if (IS_SERVER) {
      StringObjAppendPrintf(stats,"SlowSync Matches:            %9ld\n",(long)fSlowSyncMatches);
      StringObjAppendPrintf(stats,"Server won Conflicts:        %9ld\n",(long)fConflictsServerWins);
      StringObjAppendPrintf(stats,"Client won Conflicts:        %9ld\n",(long)fConflictsClientWins);
      StringObjAppendPrintf(stats,"Conflicts with Duplication:  %9ld\n\n",(long)fConflictsDuplicated);
    }
    #endif
    StringObjAppendPrintf(stats,"Content Data Bytes sent:     %9ld\n",(long)fOutgoingDataBytes);
    StringObjAppendPrintf(stats,"Content Data Bytes received: %9ld\n\n",(long)fIncomingDataBytes);
    StringObjAppendPrintf(stats,"Duration of sync [seconds]:  %9ld\n",(long)((fEndOfSyncTime-fSessionP->getSessionStarted())/secondToLinearTimeFactor));
    PDEBUGPUTSXX(DBG_HOT,stats.c_str(),0,true);
  }
  if (isAborted()) {
    // failed
    PDEBUGPRINTFX(DBG_ERROR,("Warning: Failed with status code=%hd, statistics are incomplete!!",fAbortStatusCode));
  }
  #endif
} // TLocalEngineDS::showStatistics



// create a new syncop command for sending to remote
TSyncOpCommand *TLocalEngineDS::newSyncOpCommand(
  TSyncItem *aSyncItemP, // the sync item
  TSyncItemType *aSyncItemTypeP,  // the sync item type
  cAppCharP aLocalIDPrefix
)
{
  // get operation
  TSyncOperation syncop=aSyncItemP->getSyncOp();
  // obtain meta
  SmlPcdataPtr_t metaP = newMetaType(aSyncItemTypeP->getTypeName());
  // create command
  TSyncOpCommand *syncopcmdP = new TSyncOpCommand(fSessionP,this,syncop,metaP);
  // make sure item does not have stuff it is not allowed to have
  // %%% SCTS does not like SourceURI in Replace and Delete commands sent to Client
  // there are the only ones allowed to carry a GUID
  if (IS_SERVER) {
    #ifdef SYSYNC_SERVER
    // Server: commands only have remote IDs, except add which only has target ID
    if (syncop==sop_add || syncop==sop_wants_add)
      aSyncItemP->clearRemoteID(); // no remote ID
    else {
      if (!fDSConfigP->fAlwaysSendLocalID &&
          aSyncItemP->hasRemoteID()) {
        // only if localID may not be included in all syncops,
        // and not if the item has no remote ID yet
        //
        // The second case had to be added to solve an issue
        // during suspended syncs:
        // - server tries to add a new item and uses the Replace op for it
        // - pending Replace is added to map
        // - next sync resends the Replace, but with empty IDs and thus
        //   cannot be processed by client
        //
        // Log from such a failed sync:
        // Item localID='328' already has map entry: remoteid='', mapflags=0x1, changed=0, deleted=0, added=0, markforresume=0, savedmark=1
        // Resuming and found marked-for-resume -> send replace
        // ...
        // Command 'Replace': is 1-th counted cmd, cmdsize(+tags needed to end msg)=371, available=130664 (maxfree=299132, freeaftersend=298761, notUsableBufferBytes()=168468)
        // Item remoteID='', localID='', datasize=334
        // Replace: issued as (outgoing MsgID=2, CmdID=4), now queueing for status
        // ...
        // Status 404: Replace target not found on client -> silently ignore but remove map in server (item will be added in next session),
        aSyncItemP->clearLocalID(); // no local ID
      }
    }
    #endif
  }
  else {
    // Client: all commands only have local IDs
    aSyncItemP->clearRemoteID(); // no remote ID
  }
  // add the localID prefix if we do have a localID to send
  if (aSyncItemP->hasLocalID()) {
    if (IS_SERVER) {
      #ifdef SYSYNC_SERVER
      // make sure GUID (plus prefixes) is not exceeding allowed size
      adjustLocalIDforSize(aSyncItemP->fLocalID,getRemoteDatastore()->getMaxGUIDSize(),aLocalIDPrefix ? strlen(aLocalIDPrefix) : 0);
      #endif
    }
    // add local ID prefix, if any
    if (aLocalIDPrefix && *aLocalIDPrefix)
      aSyncItemP->fLocalID.insert(0,aLocalIDPrefix);
  }
  #ifdef SYSYNC_TARGET_OPTIONS
  // init item generation variables
  fItemSizeLimit=fSizeLimit;
  #else
  fItemSizeLimit=-1; // no limit
  #endif
  // now add item
  SmlItemPtr_t itemP = aSyncItemTypeP->newSmlItem(aSyncItemP,this);
  // check if data size is ok
  if (itemP && fSessionP->fMaxOutgoingObjSize) {
    if (itemP->data && itemP->data->content && itemP->data->length) {
      // there is data, check if size is ok
      if (itemP->data->length > fSessionP->fMaxOutgoingObjSize) {
        // too large, suppress it
        PDEBUGPRINTFX(DBG_ERROR,(
          "WARNING: outgoing item is larger (%ld) than MaxObjSize (%ld) of remote -> suppress now/mark for resend",
          (long)itemP->data->length,
          (long)fSessionP->fMaxOutgoingObjSize
        ));
        smlFreeItemPtr(itemP);
        itemP=NULL;
        // mark item for resend
        // For datastores without resume support, this will just have no effect at all
        engMarkItemForResend(aSyncItemP->getLocalID(),aSyncItemP->getRemoteID());
      }
    }
  }
  if (itemP) {
    // add it to the command
    syncopcmdP->addItem(itemP);
  }
  else {
    // no item - command is invalid, delete it
    delete syncopcmdP;
    syncopcmdP=NULL;
  }
  // return command
  return syncopcmdP;
} // TLocalEngineDS::newSyncOpCommand


// create SyncItem suitable for being sent from local to remote
TSyncItem *TLocalEngineDS::newItemForRemote(
  uInt16 aExpectedTypeID    // typeid of expected type
)
{
  // safety
  if (!canCreateItemForRemote())
    SYSYNC_THROW(TSyncException("newItemForRemote called without sufficient type information ready"));
  // create
  TSyncItem *itemP = fLocalSendToRemoteTypeP->newSyncItem(fRemoteReceiveFromLocalTypeP,this);
  if (!itemP)
    SYSYNC_THROW(TSyncException("newItemForRemote could not create item"));
  // check type
  if (!itemP->isBasedOn(aExpectedTypeID)) {
    PDEBUGPRINTFX(DBG_ERROR,(
      "newItemForRemote created item of typeID %hd, caller expects %hd",
      itemP->getTypeID(),
      aExpectedTypeID
    ));
    SYSYNC_THROW(TSyncException("newItemForRemote created wrong item type"));
  }
  return itemP;
} // TLocalEngineDS::newItemForRemote


// return pure relative (item) URI (removes absolute part or ./ prefix)
const char *TLocalEngineDS::DatastoreRelativeURI(const char *aURI)
{
  return relativeURI(relativeURI(aURI,fSessionP->getLocalURI()),getName());
} // TLocalEngineDS::DatastoreRelativeURI



// - init filtering and check if needed (sets fTypeFilteringNeeded, fFilteringNeeded and fFilteringNeededForAll)
void TLocalEngineDS::initPostFetchFiltering(void)
{
  #ifdef OBJECT_FILTERING
  if (!fLocalSendToRemoteTypeP) {
    fTypeFilteringNeeded=false;
    fFilteringNeeded=false;
    fFilteringNeededForAll=false;
  }
  else {
    // get basic settings from type
    fLocalSendToRemoteTypeP->initPostFetchFiltering(fTypeFilteringNeeded,fFilteringNeededForAll,this);
    fFilteringNeeded=fTypeFilteringNeeded;
    // NOTE: if type filtering is needed, it's the responsibility of initPostFetchFiltering() of
    //       the type to check (using the DBHANDLESOPTS() script func) if DB does already handle
    //       the range filters and such and possibly avoid type filtering then.
    // then check for standard filter requirements
    #ifdef SYDEBUG
    #ifdef SYNCML_TAF_SUPPORT
    if (!fTargetAddressFilter.empty()) PDEBUGPRINTFX(DBG_DATA,("using (dynamic, temporary) TAF expression from CGI : %s",fTargetAddressFilter.c_str()));
    if (!fIntTargetAddressFilter.empty()) PDEBUGPRINTFX(DBG_DATA,("using (dynamic, temporary) internally set TAF expression : %s",fIntTargetAddressFilter.c_str()));
    #endif // SYNCML_TAF_SUPPORT
    if (!fSyncSetFilter.empty()) PDEBUGPRINTFX(DBG_DATA,("using (dynamic) sync set filter expression : %s",fSyncSetFilter.c_str()));
    if (!fLocalDBFilter.empty()) PDEBUGPRINTFX(DBG_DATA,("using (static) local db filter expression : %s",fLocalDBFilter.c_str()));
    #endif // SYDEBUG
    // - if DB does the standard filters, we don't need to check them here again
    if (!engFilteredFetchesFromDB(true)) {
      // If DB does NOT do the standard filters, we have to do them here
      // - this is the case if we have an (old-style) sync set filter, but not filtered by DB
      //   we need to filter all because sync set filter can be dynamic
      if (!fSyncSetFilter.empty())
        fFilteringNeededForAll=true;
      // always return true if there is something to filter at all
      if (
        !fLocalDBFilter.empty() ||
        !fDSConfigP->fInvisibleFilter.empty() ||
        !fSyncSetFilter.empty() ||
        !fDSConfigP->fRemoteAcceptFilter.empty()
      )
        fFilteringNeeded=true;
    }
  }
  PDEBUGPRINTFX(DBG_FILTER+DBG_HOT,(
    "Datastore-level postfetch filtering %sneeded%s",
    fFilteringNeeded ? "" : "NOT ",
    fFilteringNeeded ? (fFilteringNeededForAll ? " and to be applied to all records" : " only for changed records") : ""
  ));
  #endif
} // TLocalEngineDS::initPostFetchFiltering


// filter fetched record
bool TLocalEngineDS::postFetchFiltering(TSyncItem *aSyncItemP)
{
  #ifndef OBJECT_FILTERING
  return true; // no filters, always pass
  #else
  if (!aSyncItemP) return false; // null item does not pass
  // first do standard filters
  // - if DB has filtered the
  bool passes=true;
  if (fFilteringNeeded) {
    // - first make sure outgoing object has all properties set
    //   such that it would pass the acceptance filter (for example KIND for calendar...)
    if (!aSyncItemP->makePassFilter(fDSConfigP->fRemoteAcceptFilter.c_str())) {
      // we could not make item pass acceptance filters
      PDEBUGPRINTFX(DBG_ERROR,("- item localid='%s' cannot be made passing <acceptfilter> -> ignored",aSyncItemP->getLocalID()));
      passes=false;
    }
    // now check for field-level filters
    if (passes && !engFilteredFetchesFromDB()) {
      // DB has not already filtered these, so we need to do it here
      // - "moving target" first
      passes=fSyncSetFilter.empty() || aSyncItemP->testFilter(fSyncSetFilter.c_str());
      if (passes) {
        // - static filters
        passes =
          aSyncItemP->testFilter(fLocalDBFilter.c_str()) && // local filter
          (
            fDSConfigP->fInvisibleFilter.empty() || // and either no invisibility defined...
            !aSyncItemP->testFilter(fDSConfigP->fInvisibleFilter.c_str()) // ...or NOT passed
          );
      }
    }
    if (passes && fTypeFilteringNeeded) {
      // finally, apply type's filter
      passes=aSyncItemP->postFetchFiltering(this);
    }
  }
  else {
    // no filtering needed, DB has already filtered out those that would not pass
    // BUT: make sure outgoing items WILL pass the acceptance filter. If this
    //      cannot be done, item will be filtered out.
    if (!aSyncItemP->makePassFilter(fDSConfigP->fRemoteAcceptFilter.c_str())) {
      // we could not make item pass acceptance filters
      PDEBUGPRINTFX(DBG_ERROR,("- item localid='%s' cannot be made passing <acceptfilter> -> ignored",aSyncItemP->getLocalID()));
      passes=false;
    }
  }
  #ifdef SYDEBUG
  if (!passes) {
    PDEBUGPRINTFX(DBG_DATA,("- item localid='%s' does not pass filters -> ignored",aSyncItemP->getLocalID()));
  }
  #endif
  // return result
  return passes;
  #endif
} // TLocalEngineDS::postFetchFiltering


#ifdef OBJECT_FILTERING

// - called to check if incoming item passes acception filters
bool TLocalEngineDS::isAcceptable(TSyncItem *aSyncItemP, TStatusCommand &aStatusCommand)
{
  // test acceptance
  if (aSyncItemP->testFilter(fDSConfigP->fRemoteAcceptFilter.c_str())) return true; // ok
  // not accepted, set 415 error
  if (!fDSConfigP->fSilentlyDiscardUnaccepted)
    aStatusCommand.setStatusCode(415);
  ADDDEBUGITEM(aStatusCommand,"Received item does not pass acceptance filter");
  PDEBUGPRINTFX(DBG_ERROR,(
    "Received item does not pass acceptance filter: %s",
    fDSConfigP->fRemoteAcceptFilter.c_str()
  ));
  return false;
} // TLocalEngineDS::isAcceptable


/// @brief called to make incoming item visible
/// @return true if now visible
bool TLocalEngineDS::makeVisible(TSyncItem *aSyncItemP)
{
  bool invisible=false;
  if (!fDSConfigP->fInvisibleFilter.empty()) {
    invisible=aSyncItemP->testFilter(fDSConfigP->fInvisibleFilter.c_str());
  }
  if (invisible) {
    return aSyncItemP->makePassFilter(fDSConfigP->fMakeVisibleFilter.c_str());
  }
  return true; // is already visible
} // TLocalEngineDS::makeVisible


/// @brief called to make incoming item INvisible
/// @return true if now INvisible
bool TLocalEngineDS::makeInvisible(TSyncItem *aSyncItemP)
{
  // return true if could make invisible or already was invisible
  if (fDSConfigP->fInvisibleFilter.empty())
    return false; // no invisible filter, cannot make invisible
  // make pass invisible filter - if successful, we're now invisible
  return aSyncItemP->makePassFilter(fDSConfigP->fInvisibleFilter.c_str()); // try to make invisible (and return result)
} // TLocalEngineDS::makeInvisible



// - called to make incoming item pass sync set filtering
bool TLocalEngineDS::makePassSyncSetFilter(TSyncItem *aSyncItemP)
{
  bool pass=true;

  // make sure we pass sync set filtering and stay visible
  if (!fSyncSetFilter.empty()) {
    // try to make pass sync set filter (modifies item only if it would not pass otherwise)
    pass=aSyncItemP->makePassFilter(fSyncSetFilter.c_str());
  }
  if (!pass || fSyncSetFilter.empty()) {
    // specified sync set filter cannot make item pass, or no sync set filter at all:
    // - apply makePassFilter default expression
    if (!fDSConfigP->fMakePassFilter.empty()) {
      pass=aSyncItemP->makePassFilter(fDSConfigP->fMakePassFilter.c_str());
      if (pass) {
        // check again to check if item would pass the syncset filter now
        pass=aSyncItemP->testFilter(fSyncSetFilter.c_str());
      }
    }
  }
  return pass;
} // TLocalEngineDS::makePassSyncSetFilter

#endif


// process remote item
bool TLocalEngineDS::engProcessRemoteItem(
  TSyncItem *syncitemP,
  TStatusCommand &aStatusCommand
)
{
  #ifdef SYSYNC_CLIENT
  if (IS_CLIENT)
    return engProcessRemoteItemAsClient(syncitemP,aStatusCommand); // status, must be set to correct status code (ok / error)
  #endif
  #ifdef SYSYNC_SERVER
  if (IS_SERVER)
    return engProcessRemoteItemAsServer(syncitemP,aStatusCommand); // status, must be set to correct status code (ok / error)
  #endif
  // neither
  return false;
} // TLocalEngineDS::engProcessRemoteItem


// process SyncML SyncOp command for this datastore
bool TLocalEngineDS::engProcessSyncOpItem(
  TSyncOperation aSyncOp,        // the operation
  SmlItemPtr_t aItemP,           // the item to be processed
  SmlMetInfMetInfPtr_t aMetaP,   // command-wide meta, if any
  TStatusCommand &aStatusCommand // pre-set 200 status, can be modified in case of errors
)
{
  bool regular = false;
  // determine SyncItemType that can handle this item data
  if (fRemoteDatastoreP==NULL) {
    PDEBUGPRINTFX(DBG_ERROR,("engProcessSyncOpItem: Remote Datastore not known"));
    aStatusCommand.setStatusCode(500);
  }
  // - start with default
  TSyncItemType *remoteTypeP=getRemoteSendType();
  TSyncItemType *localTypeP=getLocalReceiveType();
  // - see if command-wide meta plus item contents specify another type
  //   (item meta, if present, overrides command wide meta)
  // see if item itself or command meta specify a type name or format
  SmlMetInfMetInfPtr_t itemmetaP = smlPCDataToMetInfP(aItemP->meta);
  // - format
  TFmtTypes fmt=fmt_chr;
  if (itemmetaP && itemmetaP->format)
    smlPCDataToFormat(itemmetaP->format,fmt); // use type name from item's meta
  else if (aMetaP && aMetaP->format)
    smlPCDataToFormat(aMetaP->format,fmt); // use type name from command-wide meta
  // - type
  string versstr;
  const char *typestr = NULL;
  if (itemmetaP && itemmetaP->type)
    typestr = smlPCDataToCharP(itemmetaP->type); // use type name from item's meta
  else if (aMetaP && aMetaP->type)
    typestr = smlPCDataToCharP(aMetaP->type); // use type name from command-wide meta
  // check if there is a type specified
  if (typestr) {
    PDEBUGPRINTFX(DBG_DATA,("Explicit type '%s' specified in command or item meta",typestr));
    if (strcmp(remoteTypeP->getTypeName(),typestr)!=0) {
      // specified type is NOT default type: search appropriate remote type
      remoteTypeP=fRemoteDatastoreP->getSendType(typestr,NULL); // no version known so far
      if (!remoteTypeP) {
        // specified type is not a remote type listed in remote's devInf.
        // But as remote is actually using it, we can assume it does support it, so use local type of same name instead
        PDEBUGPRINTFX(DBG_ERROR,("According to remote devInf, '%s' is not supported, but obviously it is used here so we try to handle it",typestr));
        // look it up in local datastore's list
        remoteTypeP=getReceiveType(typestr,NULL);
      }
    }
    if (remoteTypeP) {
      #ifdef APP_CAN_EXPIRE
      // get modified date of item
      lineardate_t moddat=0; // IMPORTANT, must be initialized in case expiryFromData returns nothing!
      bool ok = remoteTypeP->expiryFromData(aItemP,moddat)<=MAX_EXPIRY_DIFF+5;
      // ok==true: we are within hard expiry
      // ok==false: we are out of hard expiry
      #ifdef SYSER_REGISTRATION
      if (getSession()->getSyncAppBase()->fRegOK) {
        // we have a license (permanent or timed) --> hard expiry is irrelevant
        // (so override ok according to validity of current license)
        ok=true; // assume ok
        // check if license is timed, and if so, check if mod date is within timed range
        // (if not, set ok to false)
        uInt8 rd = getSession()->getSyncAppBase()->fRegDuration;
        if (rd) {
          lineardate_t ending = date2lineardate(rd/12+2000,rd%12+1,1);
          ok = ending>=moddat; // ok if not modified after end of license period
        }
      }
      #endif
      // when we have no license (neither permanent nor timed), hard expiry decides as is
      // (so just use ok as is)
      if (!ok) {
        aStatusCommand.setStatusCode(403); // forbidden to hack this expiry stuff!
        fSessionP->AbortSession(403,true); // local problem
        return false;
      }
      #endif // APP_CAN_EXPIRE
      // we have a type, which should be able to determine version from data
      if (remoteTypeP->versionFromData(aItemP,versstr)) {
        // version found, Make sure version matches as well
        PDEBUGPRINTFX(DBG_DATA,("Version '%s' obtained from item data",versstr.c_str()));
        // check if current remotetype already has correct version (and type, but we know this already)
        if (!remoteTypeP->supportsType(remoteTypeP->getTypeName(),versstr.c_str(),true)) {
          // no, type/vers do not match, search again
          remoteTypeP=fRemoteDatastoreP->getSendType(typestr,versstr.c_str());
          if (!remoteTypeP) {
            // specified type is not a remote type listed in remote's devInf.
            // But as remote is actually using it, we can assume it does support it, so use local type of same name instead
            PDEBUGPRINTFX(DBG_ERROR,("According to remote devInf, '%s' version '%s' is not supported, but obviously it is used here so we try to handle it",typestr,versstr.c_str()));
            // look it up in local datastore's list
            remoteTypeP=getReceiveType(typestr,versstr.c_str());
          }
        }
      }
      else {
        PDEBUGPRINTFX(DBG_HOT,("Version could not be obtained from item data"));
      }
    }
    if (!remoteTypeP) {
      // no matching remote type: fail
      aStatusCommand.setStatusCode(415);
      ADDDEBUGITEM(aStatusCommand,"Incompatible content type specified in command or item meta");
      PDEBUGPRINTFX(DBG_ERROR,(
        "Incompatible content type '%s' version '%s' specified in command or item meta",
        typestr,
        versstr.empty() ? "[none]" : versstr.c_str()
      ));
      return false; // irregular
    }
    else {
      // we have the remote type, now determine matching local type
      // - first check if this is compatible with the existing localTypeP (which
      //   was possibly selected by remote rule match
      if (!localTypeP->supportsType(remoteTypeP->getTypeName(),remoteTypeP->getTypeVers(),false)) {
        // current default local type does not support specified remote type
        // - find a matching local type
        localTypeP=getReceiveType(remoteTypeP);
        #ifdef SYDEBUG
        if (localTypeP) {
          PDEBUGPRINTFX(DBG_DATA+DBG_HOT,(
            "Explicit type '%s' does not match default type -> switching to local type '%s' for processing item",
            typestr,
            localTypeP->getTypeConfig()->getName()
          ));
        }
        #endif
      }

    }
  }
  // now process
  if (localTypeP && remoteTypeP) {
    TSyncItem *syncitemP = NULL;
    // create the item (might have empty data in case of delete)
    syncitemP=remoteTypeP->newSyncItem(aItemP,aSyncOp,fmt,localTypeP,this,aStatusCommand);
    if (!syncitemP) {
      // failed to create item
      return false; // irregular
    }
    // Now start the real processing
    PDEBUGBLOCKFMT(("Process_Item","processing remote item",
      "SyncOp=%s|LocalID=%s|RemoteID=%s",
      SyncOpNames[syncitemP->getSyncOp()],
      syncitemP->getLocalID(),
      syncitemP->getRemoteID()
    ));
    #ifdef SCRIPT_SUPPORT
    TErrorFuncContext errctx;
    errctx.syncop = syncitemP->getSyncOp();
    #endif
    SYSYNC_TRY {
      // this call frees the item
      regular =
        engProcessRemoteItem(syncitemP,aStatusCommand);
      syncitemP = NULL;
      PDEBUGENDBLOCK("Process_Item");
    }
    SYSYNC_CATCH (...)
      // Hmm, was the item freed? Not sure, so assume that it was freed.
      PDEBUGENDBLOCK("Process_Item");
      SYSYNC_RETHROW;
    SYSYNC_ENDCATCH
    // Check for datastore level scripts that might change the status code and/or regular status
    #ifdef SCRIPT_SUPPORT
    errctx.statuscode = aStatusCommand.getStatusCode();
    errctx.newstatuscode = errctx.statuscode;
    errctx.datastoreP = this;
    // call script
    regular =
      TScriptContext::executeTest(
        regular, // pass through regular status
        fDataStoreScriptContextP,
        fDSConfigP->fReceivedItemStatusScript,
        &ErrorFuncTable,
        &errctx // caller context
      );
    // use possibly modified status code
    #ifdef SYDEBUG
    if (aStatusCommand.getStatusCode() != errctx.newstatuscode) {
      PDEBUGPRINTFX(DBG_ERROR,("Status: Datastore script changed original status=%hd to %hd (original op was %s)",aStatusCommand.getStatusCode(),errctx.newstatuscode,SyncOpNames[errctx.syncop]));
    }
    #endif
    aStatusCommand.setStatusCode(errctx.newstatuscode);
    #endif
    if (regular) {
      // item 100% successfully processed
      // - set new defaults to same type as current item
      setReceiveTypeInfo(localTypeP,remoteTypeP);
    }
  }
  else {
    // missing remote or local type: fail
    aStatusCommand.setStatusCode(415);
    ADDDEBUGITEM(aStatusCommand,"Unknown content type");
    PDEBUGPRINTFX(DBG_ERROR,(
      "Missing remote or local SyncItemType"
    ));
    regular=false; // irregular
  }
  return regular;
} // TLocalEngineDS::engProcessSyncOpItem


#ifdef SYSYNC_SERVER


// Server Case
// ===========

// helper to cause database version of an item (as identified by aSyncItemP's ID) to be sent to client
// (aka "force a conflict")
TSyncItem *TLocalEngineDS::SendDBVersionOfItemAsServer(TSyncItem *aSyncItemP)
{
  TStatusCommand dummy(fSessionP);
  // - create new item
  TSyncItem *conflictingItemP =
    newItemForRemote(aSyncItemP->getTypeID());
  if (!conflictingItemP) return NULL;
  // - set IDs
  conflictingItemP->setLocalID(aSyncItemP->getLocalID());
  conflictingItemP->setRemoteID(aSyncItemP->getRemoteID());
  // - this is always a replace conflict (item exists in DB)
  conflictingItemP->setSyncOp(sop_wants_replace);
  // - try to get from DB
  bool ok=logicRetrieveItemByID(*conflictingItemP,dummy);
  if (ok && dummy.getStatusCode()!=404) {
    // item found in DB, add it to the sync set so it can be sent to remote
    // if not cancelled by dontSendItemAsServer()
    SendItemAsServer(conflictingItemP);
    PDEBUGPRINTFX(DBG_DATA,("Forced conflict with corresponding item from server DB"));
  }
  else {
    // no item found, we cannot force a conflict
    delete conflictingItemP;
    conflictingItemP=NULL;
  }
  return conflictingItemP;
} // TLocalEngineDS::SendDBVersionOfItemAsServer



// process map
localstatus TLocalEngineDS::engProcessMap(cAppCharP aRemoteID, cAppCharP aLocalID)
{
  if (!testState(dssta_syncmodestable)) {
    // Map received when not appropriate
    PDEBUGPRINTFX(DBG_ERROR,("Map not allowed in this stage of sync"));
    return 403;
  }
  // pre-process localID
  string realLocalID;
  if (aLocalID && *aLocalID) {
    // Note: Map must be ready to have either empty local or remote ID to delete an entry
    // perform reverse lookup of received GUID to real GUID
    realLocalID = aLocalID;
    obtainRealLocalID(realLocalID);
    aLocalID=realLocalID.c_str();
  }
  else {
    aLocalID=NULL;
  }
  // pre-process remoteID
  if (!aRemoteID || *aRemoteID==0)
    aRemoteID=NULL;
  // let implementation process the map command
  return logicProcessMap(aRemoteID, aLocalID);
} // TLocalEngineDS::engProcessMap



// process sync operation from client with specified sync item
// (according to current sync mode of local datastore)
// - returns true (and unmodified or non-200-successful status) if
//   operation could be processed regularily
// - returns false (but probably still successful status) if
//   operation was processed with internal irregularities, such as
//   trying to delete non-existant item in datastore with
//   incomplete Rollbacks (which returns status 200 in this case!).
bool TLocalEngineDS::engProcessRemoteItemAsServer(
  TSyncItem *aSyncItemP,
  TStatusCommand &aStatusCommand // status, must be set to correct status code (ok / error)
)
{
  TSyncItem *conflictingItemP=NULL;
  TSyncItem *echoItemP=NULL;
  TSyncItem *delitemP=NULL;
  bool changedincoming=false;
  bool changedexisting=false;
  bool remainsvisible=true; // usually, we want the item to remain visible in the sync set
  TStatusCommand dummy(fSessionP);

  // get some info out of item (we might need it after item is already consumed)
  TSyncOperation syncop=aSyncItemP->getSyncOp();
  uInt16 itemtypeid=aSyncItemP->getTypeID();
  string remoteid=aSyncItemP->getRemoteID();
  // check if datastore is aborted
  if(CheckAborted(aStatusCommand))
    return false;
  // send event (but no abort checking)
  DB_PROGRESS_EVENT(this,pev_itemreceived,++fItemsReceived,fRemoteNumberOfChanges,0);
  fPreventAdd = false;
  fIgnoreUpdate = false;
  // show
  PDEBUGPRINTFX(DBG_DATA,("%s item operation received",SyncOpNames[syncop]));
  // check if receiving commands is allowed at all
  if (fSyncMode==smo_fromserver) {
    // Modifications from client not allowed during update from server only
    aStatusCommand.setStatusCode(403);
    ADDDEBUGITEM(aStatusCommand,"Client command not allowed in one-way/refresh from server");
    PDEBUGPRINTFX(DBG_ERROR,("Client command not allowed in one-way/refresh from server"));
    delete aSyncItemP;
    return false;
  }
  // let item check itself to catch special cases
  // - init variables which are used/modified by item checking
  #ifdef SYSYNC_TARGET_OPTIONS
  // init item generation variables
  fItemSizeLimit=fSizeLimit;
  #else
  fItemSizeLimit=-1; // no limit
  #endif
  fCurrentSyncOp = syncop;
  fEchoItemOp = sop_none;
  fItemConflictStrategy=fSessionConflictStrategy; // get default strategy for this item
  fForceConflict = false;
  fDeleteWins = fDSConfigP->fDeleteWins; // default to configured value
  fRejectStatus = -1; // no rejection
  // - now check
  //   check reads and possibly modifies:
  //   - fEchoItemOp : if not sop_none, the incoming item is echoed back to the remote with the specified syncop
  //   - fItemConflictStrategy : might be changed from the pre-set datastore default
  //   - fForceConflict : if set, a conflict is forced by adding the corresponding local item (if any) to the list of items to be sent
  //   - fDeleteWins : if set, in a replace/delete conflict delete will win (regardless of strategy)
  //   - fPreventAdd : if set, attempt to add item from remote (even implicitly trough replace) will cause no add but delete of remote item
  //   - fIgnoreUpdate : if set, attempt to update item from remote will be ignored, only adds (also implicit ones) are executed
  //   - fRejectStatus : if set>=0, incoming item is irgnored silently(==0) or with error(!=0)
  aSyncItemP->checkItem(this);
  // - create echo item if we need one
  if (fEchoItemOp!=sop_none) {
    // Note: sop_add makes no sense at all.
    // Note: If echo is enabled, conflicts are not checked, as echo makes only sense in
    //       cases where we know that a conflict cannot occur or is irrelevant
    // - artifically create a "conflicting" item, that is, one to be sent back to remote
    echoItemP=newItemForRemote(aSyncItemP->getTypeID());
    // - assign data from incoming item if echo is not a delete
    if (fEchoItemOp!=sop_delete && fEchoItemOp!=sop_archive_delete && fEchoItemOp!=sop_soft_delete)
      echoItemP->replaceDataFrom(*aSyncItemP);
    // - set remote ID (note again: sop_add makes no sense here)
    echoItemP->setRemoteID(aSyncItemP->getRemoteID());
    // - set sop
    echoItemP->setSyncOp(fEchoItemOp);
    // - now check for possible conflict
    if (!fSlowSync) {
      conflictingItemP = getConflictingItemByRemoteID(aSyncItemP);
      // remove item if there is one that would conflict with the echo
      if (conflictingItemP) dontSendItemAsServer(conflictingItemP);
      conflictingItemP = NULL;
    }
    // - add echo to the list of items to be sent (DB takes ownership)
    SendItemAsServer(echoItemP);
    PDEBUGPRINTFX(DBG_DATA,("Echoed item back to remote with sop=%s",SyncOpNames[fEchoItemOp]));
    // process item normally (except that we don't check for LUID conflicts)
  }
  // - check if incoming item should be processed at all
  if (fRejectStatus>=0) {
    // Note: a forced conflict can still occur even if item is rejected
    // (this has the effect of unconditionally letting the server item win)
    if (fForceConflict && syncop!=sop_add) {
      conflictingItemP = SendDBVersionOfItemAsServer(aSyncItemP);
      // Note: conflictingitem is always a replace
      if (conflictingItemP) {
        if (syncop==sop_delete) {
          // original was delete, forced conflict means re-adding to remote
          conflictingItemP->setSyncOp(sop_wants_add);
        }
        else {
          // merge here because we'll not process the item further
          conflictingItemP->mergeWith(*aSyncItemP,changedexisting,changedincoming,this);
        }
      }
    }
    // now discard the incoming item
    delete aSyncItemP;
    PDEBUGPRINTFX(fRejectStatus>=400 ? DBG_ERROR : DBG_DATA,("Item rejected with Status=%hd",fRejectStatus));
    if (fRejectStatus>0) {
      // rejected with status code (not necessarily error)
      aStatusCommand.setStatusCode(fRejectStatus);
      if (fRejectStatus>=300) {
        // non 200-codes are errors
        ADDDEBUGITEM(aStatusCommand,"Item rejected");
        return false;
      }
    }
    // silently rejected
    return true;
  }
  // now perform requested operation
  bool ok=false;
  localstatus sta;
  switch (syncop) {
    readonly_delete:
      // read-only handling of delete is like soft delete: remove map entry, but nothing else
      PDEBUGPRINTFX(DBG_DATA,("Read-Only Datastore: Prevented actual deletion, just removing map entry"));
    case sop_soft_delete:
      // Readonly: allowed, as only map is touched
      // soft delete from client is treated as an indication that the item was
      // removed from the client's datastore, but is still in the set
      // of sync data for that client.
      // This means that the map item must be removed.
      // - when the item is hard-deleted on the server, nothing will happen at next sync
      // - when the item is modified on the server, it will be re-added to the client at next sync
      // - when slow sync is performed, the item will be re-added, too.
      // %%%%% Note that this does NOT work as it is now, as adds also occur for non-modified
      //       items that have no map AND are visible under current targetFilter.
      //       probably we should use a map entry with no remoteID for soft-deleted items later....
      // Delete Map entry by remote ID
      aSyncItemP->clearLocalID(); // none
      sta=engProcessMap(aSyncItemP->getRemoteID(),NULL);
      ok=sta==LOCERR_OK;
      aStatusCommand.setStatusCode(ok ? 200 : sta);
      break;
    case sop_archive_delete:
      if (fReadOnly) goto readonly_delete; // register removal of item in map, but do nothing to data itself
      #ifdef OBJECT_FILTERING
      if (!fDSConfigP->fInvisibleFilter.empty()) {
        // turn into replace with all fields unavailable but made to pass invisible filter
        // - make sure that no data field is assigned
        aSyncItemP->cleardata();
        // - make item pass "invisible" filter
        if (aSyncItemP->makePassFilter(fDSConfigP->fInvisibleFilter.c_str())) {
          // item now passes invisible rule, that is, it is invisible -> replace in DB
          goto archive_delete;
        }
      }
      // fall trough, no archive delete supported
      #endif
      // No archive delete support if there is no filter to detect/generate invisibles
      // before SyncML 1.1 : we could return 210 here and still process the delete op.
      // SyncML 1.1 : we must return 501 (not implemented) here
      aStatusCommand.setStatusCode(501);
      PDEBUGPRINTFX(DBG_ERROR,("Datastore does not support Archive-Delete, error status = 501"));
      delete aSyncItemP;
      ok=false;
      break;
    case sop_delete:
      // delete item by LUID
      if (fSlowSync) {
        aStatusCommand.setStatusCode(403);
        ADDDEBUGITEM(aStatusCommand,"Delete during slow sync not allowed");
        PDEBUGPRINTFX(DBG_ERROR,("Delete during slow sync not allowed"));
        delete aSyncItemP;
        ok=false;
        break;
      }
      // check for conflict with replace from server
      // Note: conflict cases do not change local DB, so they are allowed before checking fReadOnly
      if (!echoItemP) conflictingItemP = getConflictingItemByRemoteID(aSyncItemP); // do not check conflicts if we have already created an echo
      // - check if we must force the conflict
      if (!conflictingItemP && fForceConflict) {
        conflictingItemP=SendDBVersionOfItemAsServer(aSyncItemP);
      }
      if (conflictingItemP) {
        // conflict only if other party has replace
        if (conflictingItemP->getSyncOp()==sop_replace || conflictingItemP->getSyncOp()==sop_wants_replace) {
          if (!fDeleteWins) {
            // act as if successfully deleted and cause re-adding of still existing server item
            // - discard deletion
            delete aSyncItemP;
            // - remove map entry for this item (it no longer exists on the client)
            sta = engProcessMap(NULL,conflictingItemP->getLocalID());
            aStatusCommand.setStatusCode(sta==LOCERR_OK ? 200 : sta);
            // - change replace to add (as to-be-replaced item is already deleted on remote)
            conflictingItemP->setSyncOp(sop_add);
            // - remove remote ID (will be assigned a new ID because the item is now re-added)
            conflictingItemP->setRemoteID("");
            // - no server operation needed
            PDEBUGPRINTFX(DBG_DATA,("Conflict of Client Delete with Server replace -> discarded delete, re-added server item to client"));
            ok=true;
            break;
          }
          else {
            // delete preceedes replace
            // - avoid sending item from server
            dontSendItemAsServer(conflictingItemP);
            // - let delete happen
          }
        }
        // if both have deleted the item, we should remove the map
        // and avoid sending a delete to the client
        else if (conflictingItemP->getSyncOp()==sop_delete) {
          // - discard deletion
          delete aSyncItemP;
          // - remove map entry for this item (it no longer exists)
          sta = engProcessMap(NULL,conflictingItemP->getLocalID());
          aStatusCommand.setStatusCode(sta==LOCERR_OK ? 200 : sta);
          // - make sure delete from server is not sent
          dontSendItemAsServer(conflictingItemP);
          PDEBUGPRINTFX(DBG_DATA,("Client and Server have deleted same item -> just removed map entry"));
          ok=true;
          break;
        }
      }
      // real delete is discarded silently when fReadOnly is set
      if (fReadOnly) goto readonly_delete; // register removal of item in map, but do nothing to data itself
      // really delete
      fLocalItemsDeleted++;
      remainsvisible=false; // deleted not visible any more
      ok=logicProcessRemoteItem(aSyncItemP,aStatusCommand,remainsvisible); // delete in local database NOW
      break;
    case sop_copy:
      if (fReadOnly) {
        delete aSyncItemP; // we don't need it
        aStatusCommand.setStatusCode(200);
        PDEBUGPRINTFX(DBG_DATA,("Read-Only: copy command silently discarded"));
        ok=true;
        break;
      }
      // %%% note: this would belong into specific datastore implementation, but is here
      //     now for simplicity as copy isn't used heavily het
      // retrieve data from local datastore
      if (!logicRetrieveItemByID(*aSyncItemP,aStatusCommand)) { ok=false; break; }
      // process like add
      /// @todo %%%%%%%%%%%%%%%% NOTE: MISSING SENDING BACK MAP COMMAND for new GUID created
      goto normal_add;
    case sop_add:
      // test for slow sync
      if (fSlowSync) goto sop_slow_add; // add in slow sync is like replace
    normal_add:
      // add as new item to server DB
      aStatusCommand.setStatusCode(201); // item added (if no error occurs)
      if (fReadOnly) {
        delete aSyncItemP; // we don't need it
        PDEBUGPRINTFX(DBG_DATA,("Read-Only: add command silently discarded"));
        ok=true;
        break;
      }
      // check if adds are prevented
      if (!fPreventAdd) {
        // add allowed
        fLocalItemsAdded++;
        #ifdef OBJECT_FILTERING
        // test if acceptable
        if (!isAcceptable(aSyncItemP,aStatusCommand)) { ok=false; break; } // cannot be accepted
        // Note: making item to pass sync set filter is implemented in derived DB implementation
        //   as criteria for passing might be in data that must first be read from the DB
        #endif
        remainsvisible=true; // should remain visible
        ok=logicProcessRemoteItem(aSyncItemP,aStatusCommand,remainsvisible); // add to local database NOW
        if (!remainsvisible && fSessionP->getSyncMLVersion()>=syncml_vers_1_2) {
          PDEBUGPRINTFX(DBG_DATA+DBG_HOT,("Added item is not visible under current filters -> remove it on client"));
          goto removefromremoteandsyncset;
        }
        break;
      }
      goto preventadd;
    archive_delete:
    sop_slow_add:
      aSyncItemP->setSyncOp(sop_replace); // set correct op
      // ...and process like replace
    case sop_reference_only:
    case sop_replace:
      #ifdef OBJECT_FILTERING
      // test if acceptable
      if (!isAcceptable(aSyncItemP,aStatusCommand)) { ok=false; break; } // cannot be accepted
      // Note: making item to pass sync set filter is implemented in derived DB implementation
      //   as criteria for passing might be in data that must first be read from the DB
      #endif
      // check for conflict with server side modifications
      if (!fSlowSync) {
        if (!echoItemP) conflictingItemP = getConflictingItemByRemoteID(aSyncItemP);
        // - check if we must force the conflict
        if (!conflictingItemP && fForceConflict) {
          conflictingItemP=SendDBVersionOfItemAsServer(aSyncItemP);
        }
        bool deleteconflict=false;
        if (conflictingItemP) {
          // Note: if there is a conflict, this replace cannot be an
          //       implicit add, so we don't need to check for fPreventAdd
          //       here.
          // Note: if we are in ignoreUpdate mode, the only conflict resolution
          //       possible is unconditional server win
          sInt16 cmpRes = SYSYNC_NOT_COMPARABLE;
          // assume we can resolve the conflict
          aStatusCommand.setStatusCode(419); // default to server win
          ADDDEBUGITEM(aStatusCommand,"Conflict resolved by server");
          PDEBUGPRINTFX(DBG_HOT,(
            "Conflict: Remote <%s> with remoteID=%s <--> Local <%s> with localID=%s, remoteID=%s",
            SyncOpNames[aSyncItemP->getSyncOp()],
            aSyncItemP->getRemoteID(),
            SyncOpNames[conflictingItemP->getSyncOp()],
            conflictingItemP->getLocalID(),
            conflictingItemP->getRemoteID()
          ));
          // we have a conflict, decide what to do
          TConflictResolution crstrategy;
          if (fReadOnly || fIgnoreUpdate) {
            // server always wins and overwrites modified client version
            PDEBUGPRINTFX(DBG_DATA,("Read-Only or IgnoreUpdate: server always wins"));
            crstrategy=cr_server_wins;
          }
          else {
            // two-way
            crstrategy = fItemConflictStrategy; // get conflict strategy pre-set for this item
            if (conflictingItemP->getSyncOp()==sop_delete) {
              // server wants to delete item, client wants to replace
              if (fDSConfigP->fTryUpdateDeleted) {
                // if items are not really deleted, but only made invisible,
                // we can assume we can update the "deleted" item
                // BUT ONLY if the conflict strategy is not "server always wins"
                if (crstrategy==cr_server_wins) {
                  PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,("Conflict of Client Replace with Server delete and strategy is server-wins -> delete from client"));
                  aStatusCommand.setStatusCode(419); // server wins, client command ignored
                  break; // done
                }
                else {
                  PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,("Conflict of Client Replace with Server delete -> try to update already deleted item (as it might still exist in syncset)"));
                  // apply replace (and in case of !fDeleteWins, possible implicit add)
                  fPreventAdd=fDeleteWins; // we want implicit add only if delete cannot win
                  remainsvisible=!fDeleteWins; // we want to see the item in the sync set if delete does not win!
                  ok=logicProcessRemoteItem(aSyncItemP,aStatusCommand,remainsvisible);
                }
                if (fDeleteWins) {
                  if (!ok) {
                    // could not update already deleted item
                    PDEBUGPRINTFX(DBG_PROTO,("Could not update already deleted server item (seems to be really deleted, not just invisible)"));
                    aStatusCommand.setStatusCode(419); // server wins, client command ignored
                  }
                  else {
                    // update of invisible item successful, but it will still be deleted from client
                    // Note: possibly, the update was apparently successful, but only because an UPDATE with no
                    //   target does not report an error. So effectively, no update might have happened.
                    PDEBUGPRINTFX(DBG_PROTO,("Updated already deleted server item, but delete still wins -> client item will be deleted"));
                    fLocalItemsUpdated++;
                    aStatusCommand.setStatusCode(200); // client command successful (but same item will still be deleted)
                  }
                  // nothing more to do, let delete happen on the client (conflictingItemP delete will be sent)
                  ok=true;
                }
                else {
                  // not fDeleteWins - item failed, updated or implicitly added
                  if (ok) {
                    // update (or implicit add) successful
                    if (aStatusCommand.getStatusCode()==201) {
                      PDEBUGPRINTFX(DBG_PROTO,("Client Update wins and has re-added already deleted server item -> prevent delete on client"));
                      fLocalItemsAdded++;
                    }
                    else {
                      PDEBUGPRINTFX(DBG_PROTO,("Client Update wins and has updated still existing server item -> prevent delete on client"));
                      fLocalItemsUpdated++;
                    }
                    // and client item wins - prevent sending delete to client
                    // - don't send delete to client
                    conflictingItemP->setSyncOp(sop_none); // just in case...
                    dontSendItemAsServer(conflictingItemP);
                  }
                }
                // done
                break;
              }
              else {
                // Normal delete conflict processing (assuming deleted items REALLY deleted)
                if (!fDeleteWins) {
                  // - client always wins (replace over delete)
                  crstrategy=cr_client_wins;
                  deleteconflict=true; // flag condition for processing below
                  // - change from replace to add, because item is already deleted in server and must be re-added
                  fLocalItemsAdded++;
                  aSyncItemP->setSyncOp(sop_add);
                  PDEBUGPRINTFX(DBG_PROTO,("Conflict of Client Replace with Server delete -> client wins, client item is re-added to server"));
                }
                else {
                  // delete wins, just discard incoming item
                  delete aSyncItemP;
                  PDEBUGPRINTFX(DBG_PROTO,("Conflict of Client Replace with Server delete -> DELETEWINS() set -> ignore client replace"));
                  ok=true;
                  break;
                }
              }
            }
            else {
              // replace from client conflicts with replace from server
              // - compare items for further conflict resolution
              //   NOTE: it is serveritem.compareWith(clientitem)
              cmpRes = conflictingItemP->compareWith(
                *aSyncItemP,eqm_conflict,this
                #ifdef SYDEBUG
                ,PDEBUGTEST(DBG_CONFLICT) // show conflict comparisons in normal sync if conflict details are enabled
                #endif
              );
              PDEBUGPRINTFX(DBG_DATA,(
                "Compared conflicting items with eqm_conflict: remoteItem %s localItem",
                cmpRes==0 ? "==" : (cmpRes>0 ? "<" : ">")
              ));
              // see if we can determine newer item
              if (crstrategy==cr_newer_wins) {
                if (cmpRes!=0 && conflictingItemP->sortable(*aSyncItemP)) {
                  // newer item wins
                  // (comparison was: serveritem.compareWith(clientitem), so
                  // cmpRes<0 means that client is newer
                  PDEBUGPRINTFX(DBG_PROTO,("Conflict resolved by identifying newer item"));
                  if (cmpRes > 0)
                    crstrategy=cr_server_wins; // server has newer item
                  else
                    crstrategy=cr_client_wins; // client has newer item
                }
                else {
                  // newer item cannot be determined, duplicate items
                  crstrategy=cr_duplicate;
                }
                PDEBUGPRINTFX(DBG_DATA,(
                  "Newer item %sdetermined: %s",
                  crstrategy==cr_duplicate ? "NOT " : "",
                  crstrategy==cr_client_wins ? "Client item is newer and wins" :
                    (crstrategy==cr_server_wins ? "Server item is newer ans wins" : "item is duplicated if different")
                ));
              }
            }
            // modify strategy based on compare
            if (cmpRes==0 && crstrategy==cr_duplicate) {
              // items are equal by definition of item comparison,
              // but obviously both changed, this means that changes should be
              // mergeable
              // So, by deciding arbitrarily that server has won, we will not loose any data
              crstrategy=cr_server_wins; // does not matter, because merge will be attempted
              PDEBUGPRINTFX(DBG_DATA,("Duplication avoided because items are equal by their own definition, just merge"));
            }
            // if adds prevented, we cannot duplicate, let server win
            if (fPreventAdd && crstrategy==cr_duplicate) crstrategy=cr_server_wins;
          } // not fReadOnly
          // now apply strategy
          if (crstrategy==cr_duplicate) {
            // add items vice versa
            PDEBUGPRINTFX(DBG_PROTO,("Conflict resolved by duplicating items in both databases"));
            aStatusCommand.setStatusCode(209);
            fConflictsDuplicated++;
            // - set server item such that it will be added as new item to client DB
            conflictingItemP->setSyncOp(sop_add);
            // - break up mapping between client and server item BEFORE adding to server
            //   because else adding of item with already existing remoteID can fail.
            //   In addition, item now being sent to client may not have a map before
            //   it receives a map command from the client!
            sta = engProcessMap(NULL,conflictingItemP->getLocalID());
            if(sta!=LOCERR_OK) {
              PDEBUGPRINTFX(DBG_ERROR,(
                "Problem (status=%hd) removing map entry for LocalID='%s'",
                 sta,
                 conflictingItemP->getLocalID()
              ));
            }
            // - add client item as new item to server DB
            fLocalItemsAdded++;
            aSyncItemP->setSyncOp(sop_add); // set correct op
            remainsvisible=true; // should remain visible
            ok=logicProcessRemoteItem(aSyncItemP,aStatusCommand,remainsvisible); // add to local database NOW
            break;
          }
          else if (crstrategy==cr_server_wins) {
            // Note: for fReadOnly, this is always the case!
            // server item wins and is sent to client
            PDEBUGPRINTFX(DBG_PROTO,("Conflict resolved: server item replaces client item"));
            aStatusCommand.setStatusCode(419); // server wins, client command ignored
            fConflictsServerWins++;
            // - make sure item is set to replace data in client
            conflictingItemP->setSyncOp(sop_replace);
            // - attempt to merge data from loosing item (accumulating fields)
            if (!fReadOnly) {
              conflictingItemP->mergeWith(*aSyncItemP,changedexisting,changedincoming,this);
            }
            if (fIgnoreUpdate) changedexisting=false; // never try to update existing item
            if (changedexisting) {
              // we have merged something, so server must be updated, too
              // Note: after merge, both items are equal. We check if conflictingitem
              //       has changed, but if yes, we write the incoming item. Conflicting item
              //       will get sent to client later
              PDEBUGPRINTFX(DBG_DATA,("*** Merged some data from loosing client item into winning server item"));
              // set correct status for conflict resultion by merge
              aStatusCommand.setStatusCode(207); // merged
              // process update in local database
              fLocalItemsUpdated++;
              aSyncItemP->setSyncOp(sop_replace); // update
              remainsvisible=true; // should remain visible
              ok=logicProcessRemoteItem(aSyncItemP,aStatusCommand,remainsvisible); // update in local database NOW
              break;
            }
            else {
              // - item sent by client has lost and can be deleted now
              //   %%% possibly add option here to archive item in some way
              //       BUT ONLY IF NOT fReadOnly
              delete aSyncItemP;
            }
          }
          else if (crstrategy==cr_client_wins) {
            // client item wins and is sent to server
            PDEBUGPRINTFX(DBG_PROTO,("Conflict resolved: client item replaces server item"));
            aStatusCommand.setStatusCode(208); // client wins
            fConflictsClientWins++;
            // - attempt to merge data from loosing item (accumulating fields)
            if (!deleteconflict) {
              aSyncItemP->mergeWith(*conflictingItemP,changedincoming,changedexisting,this);
            }
            if (changedincoming) {
              // we have merged something, so client must be updated even if it has won
              // Note: after merge, both items are equal. We check if aSyncItemP
              //       has changed, but if yes, we make sure the conflicting item gets
              //       sent to the client
              PDEBUGPRINTFX(DBG_DATA,("*** Merged some data from loosing server item into winning client item"));
              conflictingItemP->setSyncOp(sop_replace); // update
              // set correct status for conflict resultion by merge
              aStatusCommand.setStatusCode(207); // merged
              // will be sent because it is in the list
            }
            else {
              // - make sure conflicting item from server is NOT sent to client
              conflictingItemP->setSyncOp(sop_none); // just in case...
              dontSendItemAsServer(conflictingItemP);
              aStatusCommand.setStatusCode(200); // ok
            }
            // - replace item in server (or leave it as is, if conflict was with delete)
            if (!deleteconflict) {
              fLocalItemsUpdated++;
              aSyncItemP->setSyncOp(sop_replace);
            }
            remainsvisible=true; // should remain visible
            ok=logicProcessRemoteItem(aSyncItemP,aStatusCommand,remainsvisible); // replace in local database NOW
            break;
          }
        } // replace conflict
        else {
          // normal replace without any conflict
          if (fReadOnly) {
            delete aSyncItemP; // we don't need it
            aStatusCommand.setStatusCode(200);
            PDEBUGPRINTFX(DBG_DATA,("Read-Only: replace command silently discarded"));
            ok=true;
            break;
          }
          // no conflict, just let client replace server's item
          PDEBUGPRINTFX(DBG_DATA+DBG_CONFLICT,("No Conflict: client item replaces server item"));
          // - replace item in server (or add if item does not exist and not fPreventAdd)
          aSyncItemP->setSyncOp(sop_replace);
          remainsvisible=true; // should remain visible
          if (!logicProcessRemoteItem(aSyncItemP,aStatusCommand,remainsvisible)) {
            // check if this is a 404 or 410 and fPreventAdd
            if (fPreventAdd && (aStatusCommand.getStatusCode()==404 || aStatusCommand.getStatusCode()==410))
              goto preventadd2; // to-be-replaced item not found and implicit add prevented -> delete from remote
            // simply failed
            ok=false;
            break;
          }
          // still visible in sync set?
          if (!remainsvisible && fSessionP->getSyncMLVersion()>=syncml_vers_1_2) {
            // -> cause item to be deleted on remote
            PDEBUGPRINTFX(DBG_DATA+DBG_HOT,("Item replaced no longer visible in syncset -> returning delete to remote"));
            goto removefromremoteandsyncset;
          }
          // processed ok
          if (aStatusCommand.getStatusCode()==201)
            fLocalItemsAdded++;
          else
            fLocalItemsUpdated++;
          ok=true;
          break;
        }
      } // normal sync
      else {
        // slow sync (replaces AND adds are treated the same)
        // - first do a strict search for identical item. This is required to
        //   prevent that in case of multiple (loosely compared) matches we
        //   catch the wrong item and cause a mess at slowsync
        //   NOTE: we do compare only relevant fields (eqm_conflict)
        TSyncItem *matchingItemP = getMatchingItem(aSyncItemP,eqm_conflict);
        if (!matchingItemP) {
          // try again with less strict comparison (eqm_slowsync or eqm_always for firsttimesync)
          DEBUGPRINTFX(DBG_DATA+DBG_MATCH,("Strict search for matching item failed, try with configured EqMode now"));
          matchingItemP = getMatchingItem(aSyncItemP,fFirstTimeSync ? eqm_always : eqm_slowsync);
        }
        if (matchingItemP) {
          // both sides already have this item
          PDEBUGPRINTFX(DBG_DATA+DBG_HOT,(
            "Slow Sync Match detected - localID='%s' matches incoming item",
            matchingItemP->getLocalID()
          ));
          fSlowSyncMatches++;
          aStatusCommand.setStatusCode(syncop==sop_add ? 201 : 200); // default is: simply ok. But if original op was Add, MUST return 201 status (SCTS requires it)
          bool matchingok=false;
          // - do not update map yet, as we still don't know if client item will
          //   possibly be added instead of mapped
          // Note: ONLY in case this is a reference-only item, the map is already updated!
          bool mapupdated = syncop==sop_reference_only;
          // - determine which one is winning
          bool needserverupdate=false;
          bool needclientupdate=false;
          // if updates are ignored, we can short-cut here
          // Note: if this is a reference-only item, it was already updated (if needed) before last suspend
          //       so skip updating now!
          if (syncop!=sop_reference_only && !fIgnoreUpdate) {
            // Not a reference-only and also updates not suppressed
            // - for a read-only datastore, this defaults to server always winning
            TConflictResolution crstrategy =
              fReadOnly ?
              cr_server_wins : // server always wins for read-only
              fItemConflictStrategy; // pre-set strategy for this item
            // Determine what data to use
            if (crstrategy==cr_newer_wins) {
              // use newer
              // Note: comparison is clientitem.compareWith(serveritem)
              //       so if result>0, client is newer than server
              sInt16 cmpRes = aSyncItemP->compareWith(
                *matchingItemP,eqm_nocompare,this
                #ifdef SYDEBUG
                ,PDEBUGTEST(DBG_CONFLICT+DBG_DETAILS) // show age comparisons only if we want to see details
                #endif
              );
              if (cmpRes==1) crstrategy=cr_client_wins;
              else if (cmpRes==-1 || fPreventAdd) crstrategy=cr_server_wins; // server wins if adds prevented
              else crstrategy=cr_duplicate; // fall back to duplication if we can't determine newer item
              PDEBUGPRINTFX(DBG_DATA,(
                "Newer item %sdetermined: %s",
                crstrategy==cr_duplicate ? "NOT " : "",
                crstrategy==cr_client_wins ? "Client item is newer and wins" :
                  (crstrategy==cr_server_wins ? "Server item is newer and wins" : "item is duplicated if different")
              ));
            }
            // if adds prevented, we cannot duplicate, let server win
            if (fPreventAdd && crstrategy==cr_duplicate) crstrategy=cr_server_wins;
            // now execute chosen strategy
            if (crstrategy==cr_client_wins) {
              // - merge server's data into client item
              PDEBUGPRINTFX(DBG_DATA,("Trying to merge some data from loosing server item into winning client item"));
              aSyncItemP->mergeWith(*matchingItemP,changedincoming,changedexisting,this);
              // only count if server gets updated
              if (changedexisting) fConflictsClientWins++;
              // Note: changedexisting will cause needserverupdate to be set below
            }
            else if (crstrategy==cr_server_wins) {
              // - merge client data into server item
              PDEBUGPRINTFX(DBG_DATA,("Trying to merge some data from loosing client item into winning server item"));
              matchingItemP->mergeWith(*aSyncItemP,changedexisting,changedincoming,this);
              // only count if client gets updated
              if (changedincoming) fConflictsServerWins++;
              // Note: changedincoming will cause needclientupdate to be set below
            }
            else if (crstrategy==cr_duplicate) {
              // test if items are equal enough
              // (Note: for first-sync, compare mode for item matching can be looser
              // (eqm_always), so re-comparison here makes sense)
              if (
                matchingItemP->compareWith(
                  *aSyncItemP,eqm_slowsync,this
                  #ifdef SYDEBUG
                  ,PDEBUGTEST(DBG_CONFLICT+DBG_DETAILS) // show equality re-test only for details enabled
                  #endif
                )!=0
              ) {
                string guid;
                // items are not really equal in content, so duplicate them on both sides
                PDEBUGPRINTFX(DBG_PROTO,("Matching items are not fully equal, duplicate them on both sides"));
                fConflictsDuplicated++;
                // - duplicates contain merged data
                matchingItemP->mergeWith(*aSyncItemP,changedexisting,changedincoming,this);
                // - add client item (with server data merged) as new item to server
                fLocalItemsAdded++;
                aSyncItemP->setSyncOp(sop_add);
                aStatusCommand.setStatusCode(201); // item added (if no error occurs)
                remainsvisible=true; // should remain visible
                matchingok=logicProcessRemoteItem(aSyncItemP,aStatusCommand,remainsvisible,&guid); // add item in local database NOW
                aSyncItemP=NULL; // is already deleted!
                if (matchingok) { // do it only if server add successful, because otherwise we don't have a GUID
                  // - make sure same item is ADDED as new item to client
                  matchingItemP->setSyncOp(sop_add); // add it, prevent it from re-match (is sop_wants_add now)
                  matchingItemP->setLocalID(guid.c_str()); // this is now a pair with the newly added item (not the original one)
                  matchingItemP->setRemoteID(""); // we don't know the remote ID yet
                }
                // adding duplicate to server (add) is already done
                changedexisting=false;
                changedincoming=false;
                mapupdated=true; // no need to update map
                needserverupdate=false; // already done
                // client must received the (updated) server item as an add (set above)
                needclientupdate=true;
              }
            }
          } // if not ignoreUpdate
          // Update server map now if required
          // - NOTE THAT THIS IS VERY IMPORTANT TO DO BEFORE any possible
          //   replaces, because replacing the matchingItem can only be
          //   done via its remoteID, which is, at this moment, probably not
          //   valid. After Mapping, it is ensured that the mapped remoteID
          //   uniquely identifies the matchingItem.
          if (!mapupdated) {
            // - update map in server
            sta = engProcessMap(
              aSyncItemP->getRemoteID(), // remote ID (LUID) of item received from client
              matchingItemP->getLocalID() // local ID (GUID) of item already stored in server
            );
            matchingok = sta==LOCERR_OK;
            if (!matchingok) {
              // failed
              ok=false;
              aStatusCommand.setStatusCode(sta);
              break;
            }
          }
          // Now prepare updates of (already mapped) server and client items if needed
          if (changedexisting) {
            // matched item in server's sync set was changed and must be update in server DB
            // update server only if updates during non-first-time slowsync are enabled
            if (fFirstTimeSync || fSessionP->fUpdateServerDuringSlowsync) {
              needserverupdate=true; // note: ineffective if fReadOnly
              PDEBUGPRINTFX(DBG_DATA+DBG_HOT,("Server data was updated by record sent by client, REPLACE it in server DB"));
            }
            else {
              PDEBUGPRINTFX(DBG_DATA+DBG_HOT,("Server data was updated by record sent by client, but server updates during non-firsttime slowsyncs are disabled"));
            }
          }
          if (changedincoming) {
            // incoming item from remote was changed after receiving and must be reflected
            // back to client
            // NOTE: this can also happen with sop_reference_only items
            // - client will be updated because matchingItemP is still in list
            matchingItemP->setSyncOp(sop_replace); // cancel wants_add state, prevent re-match
            // - matchingItem was retrieved BEFORE map was done, and has no valid remote ID
            //   so remote ID must be added now.
            matchingItemP->setRemoteID(aSyncItemP->getRemoteID());
            // update client only if updates during non-first-time slowsync are enabled
            if (fFirstTimeSync || fSessionP->fUpdateClientDuringSlowsync) {
              PDEBUGPRINTFX(DBG_DATA+DBG_HOT,("Record sent by client was updated with server data, client will receive a REPLACE"));
              needclientupdate=true;
            }
            else {
              PDEBUGPRINTFX(DBG_DATA+DBG_HOT,("Record sent by client was updated, but client updates during non-firsttime slowsyncs are disabled"));
            }
          }
          // update
          // - check if client must be updated
          if (!needclientupdate) {
            // prevent updating client
            // - make sure matching item from server is NOT sent to client
            matchingItemP->setSyncOp(sop_none); // just in case...
            dontSendItemAsServer(matchingItemP);
          }
          else {
            // check if replaces may be sent to client during slowsync
            if (matchingItemP->getSyncOp()==sop_replace && fSessionP->fNoReplaceInSlowsync) {
              PDEBUGPRINTFX(DBG_DATA+DBG_HOT,("Update of client item suppressed because of <noreplaceinslowsync>"));
              matchingItemP->setSyncOp(sop_none); // just in case...
              dontSendItemAsServer(matchingItemP);
            }
            else {
              PDEBUGPRINTFX(DBG_DATA+DBG_HOT,("Update of client item enabled (will be sent later)"));
            }
          }
          // - check if server must be updated (NEVER for fReadOnly)
          if (!fReadOnly && needserverupdate && aSyncItemP) {
            // update server
            // - update server side (NOTE: processItemAsServer takes ownership, pointer gets invalid!)
            fLocalItemsUpdated++;
            aSyncItemP->setSyncOp(sop_replace);
            remainsvisible=true; // should remain visible
            matchingok=logicProcessRemoteItem(aSyncItemP,aStatusCommand,remainsvisible); // replace item in local database NOW
            PDEBUGPRINTFX(DBG_DATA+DBG_HOT,("Updated server item"));
          }
          else {
            // - delete incoming item unprocessed (if not already deleted)
            if (aSyncItemP) delete aSyncItemP;
          }
          // check if we need to actively delete item from the client as it falls out of the filter
          if (!remainsvisible && fSessionP->getSyncMLVersion()>=syncml_vers_1_2) {
            // -> cause item to be deleted on remote
            PDEBUGPRINTFX(DBG_DATA+DBG_HOT,("Slow sync matched item no longer visible in syncset -> returning delete to remote"));
            goto removefromremoteandsyncset;
          }
          ok=matchingok;
          break;
        }
        else {
          PDEBUGPRINTFX(DBG_DATA+DBG_HOT,("No matching item found - add it to local database"));
          // this item is not yet on the server, add it normally
          aStatusCommand.setStatusCode(201); // item added (if no error occurs)
          if (fReadOnly) {
            delete aSyncItemP; // we don't need it
            PDEBUGPRINTFX(DBG_DATA,("Read-Only: slow-sync add/replace command silently discarded"));
            ok=true;
            break;
          }
          if (fPreventAdd) goto preventadd;
          fLocalItemsAdded++;
          aSyncItemP->setSyncOp(sop_add); // set correct op
          remainsvisible=true; // should remain visible
          ok=logicProcessRemoteItem(aSyncItemP,aStatusCommand,remainsvisible); // add to local database NOW
          break;
        }
      } // slow sync
      break; // just in case....
    preventadd:
      // add not allowed, delete remote item instead
      // - consume original
      delete aSyncItemP;
    preventadd2:
      aStatusCommand.setStatusCode(201); // pretend adding item was successful
      PDEBUGPRINTFX(DBG_DATA,("Prevented explicit add, returning delete to remote"));
      ok=true;
      goto removefromremote; // as we have PREVENTED adding the item, it is not in the map
    removefromremoteandsyncset:
      // remove item from local map first
      engProcessMap(remoteid.c_str(),NULL);
    removefromremote:
      // have item removed from remote
      delitemP = newItemForRemote(itemtypeid);
      delitemP->setRemoteID(remoteid.c_str());
      delitemP->setSyncOp(sop_delete);
      SendItemAsServer(delitemP); // pass ownership
      break;
    default :
      SYSYNC_THROW(TSyncException("Unknown sync op in TLocalEngineDS::processRemoteItemAsServer"));
  } // switch
  if (ok) {
    DB_PROGRESS_EVENT(this,pev_itemprocessed,fLocalItemsAdded,fLocalItemsUpdated,fLocalItemsDeleted);
  }
  else {
    // if the DB has a error string to show, add it here
    aStatusCommand.addItemString(lastDBErrorText().c_str());
  }
  // done
  return ok;
} // TLocalEngineDS::engProcessRemoteItemAsServer


/// @brief called at end of request processing, should be used to save suspend state
/// @note superdatastore does it itself to have correct order of things happening
void TLocalEngineDS::engRequestEnded(void)
{
  #ifdef SUPERDATASTORES
  if (fAsSubDatastoreOf)
    return;
  #endif
  // If DS 1.2: Make sure everything is ready for a resume in case there's an abort (implicit Suspend)
  // before the next request. Note that the we cannot wait for session timeout, as the resume attempt
  // from the client probably arrives much earlier.
  // Note: It is ESSENTIAL not to save the state until sync set is ready, because saving state will
  //   cause DB access, and DB access is not permitted while sync set is possibly still loading
  //   (possibly in a separate thread!). So dssta_syncmodestable (as in <=3.0.0.2) is NOT enough here!
  if (testState(dssta_syncsetready)) {
    // make sure all unsent items are marked for resume
    engSaveSuspendState(false); // only if not already aborted
  }
  // let datastore prepare for end of request
  dsRequestEnded();
  // and let it prepare for end of this thread as well
  dsThreadMayChangeNow();
} // TLocalEngineDS::engRequestEnded

#endif // SYSYNC_SERVER



#ifdef SYSYNC_CLIENT

// Client Case
// ===========


// process sync operation from server with specified sync item
// (according to current sync mode of local datastore)
// - returns true (and unmodified or non-200-successful status) if
//   operation could be processed regularily
// - returns false (but probably still successful status) if
//   operation was processed with internal irregularities, such as
//   trying to delete non-existant item in datastore with
//   incomplete Rollbacks (which returns status 200 in this case!).
bool TLocalEngineDS::engProcessRemoteItemAsClient(
  TSyncItem *aSyncItemP,
  TStatusCommand &aStatusCommand // status, must be set to correct status code (ok / error)
)
{
  string remoteid,localid;
  bool ok;
  bool remainsvisible;

  // send event and check for user abort
  #ifdef PROGRESS_EVENTS
  if (!DB_PROGRESS_EVENT(this,pev_itemreceived,++fItemsReceived,fRemoteNumberOfChanges,0)) {
    fSessionP->AbortSession(500,true,LOCERR_USERABORT); // this also causes datastore to be aborted
  }
  if (!SESSION_PROGRESS_EVENT(fSessionP,pev_suspendcheck,NULL,0,0,0)) {
    fSessionP->SuspendSession(LOCERR_USERSUSPEND);
  }
  #endif
  // check if datastore is aborted
  if (CheckAborted(aStatusCommand)) return false;
  // init behaviour vars (these are normally used by server only,
  // but must be initialized correctly for client as well as descendants might test them
  fPreventAdd = false;
  fIgnoreUpdate = false;
  // get operation out of item
  TSyncOperation syncop=aSyncItemP->getSyncOp();
  // show
  DEBUGPRINTFX(DBG_DATA,("%s item operation received",SyncOpNames[syncop]));
  // check if receiving commands is allowed at all
  // - must be in correct sync state
  if (!testState(dssta_syncgendone)) {
    // Modifications from server not allowed before client has done sync gen
    // %%% we could possibly relax this one, depending on the DB
    aStatusCommand.setStatusCode(403);
    PDEBUGPRINTFX(DBG_ERROR,("Server command not allowed before client has sent entire <sync>"));
    delete aSyncItemP;
    return false;
  }
  // - must not be one-way
  if (fSyncMode==smo_fromclient) {
    // Modifications from server not allowed during update from client only
    aStatusCommand.setStatusCode(403);
    ADDDEBUGITEM(aStatusCommand,"Server command not allowed in one-way/refresh from client");
    PDEBUGPRINTFX(DBG_ERROR,("Server command not allowed in one-way/refresh from client"));
    delete aSyncItemP;
    return false;
  }
  else {
    // silently discard all modifications if readonly
    if (fReadOnly) {
      PDEBUGPRINTFX(DBG_ERROR,("Read-Only: silently discarding all modifications"));
      aStatusCommand.setStatusCode(200); // always ok (but never "added"), so server cannot expect Map
      delete aSyncItemP;
      return true;
    }
    // let item check itself to catch special cases
    // - init variables which are used/modified by item checking
    fItemSizeLimit=-1; // no limit
    fCurrentSyncOp = syncop;
    fEchoItemOp = sop_none;
    fItemConflictStrategy=fSessionConflictStrategy; // get default strategy for this item
    fForceConflict = false;
    fDeleteWins = fDSConfigP->fDeleteWins; // default to configured value
    fRejectStatus = -1; // no rejection
    // - now check
    //   check reads and possibly modifies:
    //   - fEchoItemOp : if not sop_none, the incoming item is echoed back to the remote with the specified syncop
    //   - fItemConflictStrategy : might be changed from the pre-set datastore default
    //   - fForceConflict : if set, a conflict is forced by adding the corresponding local item (if any) to the list of items to be sent
    //   - fDeleteWins : if set, in a replace/delete conflict delete will win (regardless of strategy)
    //   - fPreventAdd : if set, attempt to add item from remote (even implicitly trough replace) will cause no add but delete of remote item
    //   - fIgnoreUpdate : if set, attempt to update item from remote will be ignored, only adds (also implicit ones) are executed
    //   - fRejectStatus : if set>=0, incoming item is irgnored silently(==0) or with error(!=0)
    aSyncItemP->checkItem(this);
    // - check if incoming item should be processed at all
    if (fRejectStatus>=0) {
      // now discard the incoming item
      delete aSyncItemP;
      PDEBUGPRINTFX(fRejectStatus>=400 ? DBG_ERROR : DBG_DATA,("Item rejected with Status=%hd",fRejectStatus));
      if (fRejectStatus>0) {
        // rejected with status code (not necessarily error)
        aStatusCommand.setStatusCode(fRejectStatus);
        if (fRejectStatus>=300) {
          // non 200-codes are errors
          ADDDEBUGITEM(aStatusCommand,"Item rejected");
          return false;
        }
      }
      // silently rejected
      return true;
    }
    // now perform requested operation
    switch (syncop) {
      case sop_soft_delete:
      case sop_archive_delete:
      case sop_delete:
        // delete item
        fLocalItemsDeleted++;
        remainsvisible=false; // deleted not visible any more
        ok=logicProcessRemoteItem(aSyncItemP,aStatusCommand,remainsvisible); // delete in local database NOW
        break;
      case sop_copy:
        // %%% note: this would belong into specific datastore implementation, but is here
        //     now for simplicity as copy isn't used heavily het
        // retrieve data from local datastore
        if (!logicRetrieveItemByID(*aSyncItemP,aStatusCommand)) {
          ok=false;
          break;
        }
        // process like add
      case sop_add:
        // add as new item to client DB
        aStatusCommand.setStatusCode(201); // item added (if no error occurs)
        fLocalItemsAdded++;
        // add to local datastore
        // - get remoteid BEFORE processing item (as logicProcessRemoteItem consumes the item!!)
        remoteid=aSyncItemP->getRemoteID(); // get remote ID
        // check if this remoteid is already known from last session - if so, this means that
        // this add was re-sent and must NOT be executed
        if (isAddFromLastSession(remoteid.c_str())) {
          aStatusCommand.setStatusCode(418); // already exists
          PDEBUGPRINTFX(DBG_ERROR,("Warning: Item with same server-side ID (GUID) was already added in previous session - ignore add now"));
          delete aSyncItemP; // forget item
          ok=false;
          break;
        }
        #ifdef OBJECT_FILTERING
        if (!isAcceptable(aSyncItemP,aStatusCommand)) {
          delete aSyncItemP; // forget item
          ok=false; // cannot be accepted
          break;
        }
        #endif
        remainsvisible=true; // should remain visible
        ok=logicProcessRemoteItem(aSyncItemP,aStatusCommand,remainsvisible,&localid); // add to local database NOW, get back local GUID
        if (!ok) break;
        // if added (not replaced), we need to send map
        if (aStatusCommand.getStatusCode()==201) {
          // really added: remember map entry
          DEBUGPRINTFX(DBG_ADMIN+DBG_DETAILS,(
            "engProcessRemoteItemAsClient: add command: adding new entry to fPendingAddMaps - localid='%s', remoteID='%s'",
            localid.c_str(),
            remoteid.c_str()
          ));
          fPendingAddMaps[localid]=remoteid;
        }
        ok=true; // fine
        break;
      case sop_replace:
        // - replace item in client
        fLocalItemsUpdated++;
        aSyncItemP->setSyncOp(sop_replace);
        #ifdef OBJECT_FILTERING
        if (!isAcceptable(aSyncItemP,aStatusCommand)) {
          ok=false; // cannot be accepted
          break;
        }
        #endif
        // - get remoteid BEFORE processing item (as logicProcessRemoteItem consumes the item!!),
        //   in case replace is converted to add and we need to register a map entry.
        remoteid=aSyncItemP->getRemoteID(); // get remote ID
        remainsvisible=true; // should remain visible
        ok=logicProcessRemoteItem(aSyncItemP,aStatusCommand,remainsvisible,&localid); // replace in local database NOW
        // if added (not replaced), we need to send map
        if (aStatusCommand.getStatusCode()==201) {
          // Note: logicProcessRemoteItem should NOT do an add if we have no remoteid, but return 404.
          //       The following check is just additional security.
          // really added: remember map entry if server sent remoteID (normally, it won't for Replace)
          if (!remoteid.empty()) {
            // we can handle that, as remote sent us the remoteID we need to map it correctly
            DEBUGPRINTFX(DBG_ADMIN+DBG_DETAILS,(
              "engProcessRemoteItemAsClient: replace command for unknown localID but with remoteID -> adding new entry to fPendingAddMaps - localid='%s', remoteID='%s'",
              localid.c_str(),
              remoteid.c_str()
            ));
            fPendingAddMaps[localid]=remoteid;
          }
          else {
            // we cannot handle this (we shouldn't have, in logicProcessRemoteItem!!)
            aStatusCommand.setStatusCode(510);
            ok=false;
          }
        }
        break; // fine, ok = irregularity status
      default:
        SYSYNC_THROW(TSyncException("Unknown sync op in TLocalEngineDS::processRemoteItemAsClient"));
    } // switch
    // processed
    if (ok) {
      DB_PROGRESS_EVENT(this,pev_itemprocessed,fLocalItemsAdded,fLocalItemsUpdated,fLocalItemsDeleted);
    }
    else {
      // if the DB has a error string to show, add it here
      aStatusCommand.addItemString(lastDBErrorText().c_str());
    }
    return ok;
  }
} // TLocalEngineDS::processRemoteItemAsClient



// client case: called whenever outgoing Message of Sync Package starts
void TLocalEngineDS::engClientStartOfSyncMessage(void)
{
  // is called for all local datastores, even inactive ones, so check state first
  if (testState(dssta_dataaccessstarted) && !isAborted()) {
    // only alerted non-sub datastores will start a <sync> command here, ONCE
    // if there are pending maps, they will be sent FIRST
    if (fRemoteDatastoreP) {
      if (!testState(dssta_clientsyncgenstarted)) {
        // shift state to syncgen started
        //   NOTE: if all sync commands can be sent at once,
        //   state will change again by issuing <sync>, so
        //   it MUST be changed here (not after issuing!)
        changeState(dssta_clientsyncgenstarted,true);
        // - make sure remotedatastore has correct full name
        fRemoteDatastoreP->setFullName(getRemoteDBPath());
        // an interrupted command at this point is a map command - continueIssue() will take care
        if (!fSessionP->isInterrupedCmdPending() && numUnsentMaps()>0) {
          // Check for pending maps from previous session (even in DS 1.0..1.1 case this is possible)
          fUnconfirmedMaps.clear(); // no unconfirmed maps left...
          fLastSessionMaps.clear(); // ...and no lastSessionMaps yet: all are in pendingMaps
          // if this is a not-resumed slow sync, pending maps must not be sent, but discarded
          if (isSlowSync() && !isResuming()) {
            // forget the pending maps
            PDEBUGPRINTFX(DBG_HOT,("There are %ld cached map entries from last Session, but this is a non-resumed slow sync: discard them",(long)numUnsentMaps()));
            fPendingAddMaps.clear();
          }
          else {
            // - now sending pending (cached) map commands from previous session
            // Note: if map command was already started, the
            //   finished(), continueIssue() mechanism will make sure that
            //   more commands are generated. This mechanism will also make
            //   sure that outgoing package cannot get <final/> until
            //   map is completely sent.
            // Note2: subdatastores do not generate their own map commands,
            //   but are called by superdatastore for contributing to united map
            if (!isSubDatastore()) {
              PDEBUGBLOCKFMT(("LastSessionMaps","Now sending cached map entries from last Session","datastore=%s",getName()));
              TMapCommand *mapcmdP =
                new TMapCommand(
                  fSessionP,
                  this,               // local datastore
                  fRemoteDatastoreP   // remote datastore
                );
              // issue
              ISSUE_COMMAND_ROOT(fSessionP,mapcmdP);
              PDEBUGENDBLOCK("LastSessionMaps");
            }
          }
        }
        // Now, send sync command (unless we are a subdatastore, in this case the superdatastore will take care)
        if (!isSubDatastore()) {
          // Note: if sync command was already started, the
          //   finished(), continueIssue() mechanism will make sure that
          //   more commands are generated
          // Note2: subdatastores do not generate their own sync commands,
          //   but switch to dssta_client/serversyncgenstarted for contributing to united sync command
          TSyncCommand *synccmdP =
            new TSyncCommand(
              fSessionP,
              this,               // local datastore
              fRemoteDatastoreP   // remote datastore
            );
          // issue
          ISSUE_COMMAND_ROOT(fSessionP,synccmdP);
        }
      }
    }
    else {
      PDEBUGPRINTFX(DBG_ERROR,("engClientStartOfSyncMessage can't start sync phase - missing remotedatastore"));
      changeState(dssta_idle,true); // force to idle, nothing happened yet
    }
  } // if dsssta_dataaccessstarted and not aborted
} // TLocalEngineDS::engClientStartOfSyncMessage


// Client only: returns number of unsent map items
sInt32 TLocalEngineDS::numUnsentMaps(void)
{
  return fPendingAddMaps.size();
} // TLocalEngineDS::numUnsentMaps


// Client only: called whenever outgoing Message starts that may contain <Map> commands
// @param[in] aNotYetInMapPackage set if we are still in sync-from-server package
// @note usually, client starts sending maps while still receiving syncops from server
void TLocalEngineDS::engClientStartOfMapMessage(bool aNotYetInMapPackage)
{
  // is called for all local datastores, even inactive ones, so check state first
  if (testState(dssta_syncgendone) && !isAborted()) {
    // datastores that have finished generating their <sync>
    // now can start a <map> command here, once (if not isInterrupedCmdPending(), that is, not a map already started)
    if (fRemoteDatastoreP) {
      // start a new map command if we don't have any yet and we are not done (dssta_clientmapssent) yet
      if (!fSessionP->isInterrupedCmdPending() && !testState(dssta_clientmapssent)) {
        // check if we should start one (that is, if the list is not empty)
        if (numUnsentMaps()>0) {
          // - now sending map command
          //   NOTE: if all map commands can be sent at once,
          //   fState will be modified again by issuing <map>, so
          //   it MUST be set here (not after issuing!)
          // - data access done only if we are in Map package now
          if (!aNotYetInMapPackage)
            changeState(dssta_dataaccessdone); // usually already set in engEndOfSyncFromRemote(), but does not harm here
          // Note: if map command was already started, the
          //   finished(), continueIssue() mechanism will make sure that
          //   more commands are generated. This mechanism will also make
          //   sure that outgoing package cannot get <final/> until
          //   map is completely sent.
          // Note2: subdatastores do not generate their own map commands,
          //   but superdatastore calls their generateMapItem for contributing to united map
          if (!isSubDatastore()) {
            TMapCommand *mapcmdP =
              new TMapCommand(
                fSessionP,
                this,               // local datastore
                fRemoteDatastoreP   // remote datastore
              );
            // issue
            ISSUE_COMMAND_ROOT(fSessionP,mapcmdP);
          }
        }
        else {
          // we need no map items now, but if this is still sync-from-server-package,
          // we are not done yet
          if (aNotYetInMapPackage) {
            DEBUGPRINTFX(DBG_PROTO,("No map command need to be generated now, but still in <sync> from server package"));
          }
          else {
            // we are done now
            DEBUGPRINTFX(DBG_PROTO,("All map commands sending complete"));
            changeState(dssta_clientmapssent);
          }
        }
      }
    } // if fRemoteDataStoreP
  } // if >=dssta_syncgendone
} // TLocalEngineDS::engClientStartOfMapMessage



// called to mark maps confirmed, that is, we have received ok status for them
void TLocalEngineDS::engMarkMapConfirmed(cAppCharP aLocalID, cAppCharP aRemoteID)
{
  // As this is client-only, we don't need to check for tempGUIDs here.
  // Note: superdatastore has an implementation which dispatches by prefix
  TStringToStringMap::iterator pos=fUnconfirmedMaps.find(aLocalID);
  if (pos!=fUnconfirmedMaps.end()) {
    DEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
      "engMarkMapConfirmed: deleting confirmed entry localID='%s', remoteID='%s' from fUnconfirmedMaps",
      aLocalID,
      aRemoteID
    ));
    // move it to lastsessionmap
    fLastSessionMaps[(*pos).first]=(*pos).second;
    // remove it from unconfirmed map
    fUnconfirmedMaps.erase(pos);
  }
} // TLocalEngineDS::engMarkMapConfirmed



// Check if the remoteid was used by an add command not
// fully mapped&confirmed in the previous session
bool TLocalEngineDS::isAddFromLastSession(cAppCharP aRemoteID)
{
  TStringToStringMap::iterator pos;
  TStringToStringMap *mapListP;

  for (int i=0; i<3; i++) {
    // determine next list to search
    mapListP = i==0 ? &fPendingAddMaps : (i==1 ? &fUnconfirmedMaps : &fLastSessionMaps);
    // search it
    for (pos=mapListP->begin(); pos!=mapListP->end(); ++pos) {
      if (strcmp((*pos).second.c_str(),aRemoteID)==0)
        return true; // remoteID known -> is add from last session
    }
  }
  // not found in any of the lists
  return false;
} // TLocalEngineDS::isAddFromLastSession



// - called to generate Map items
//   Returns true if now finished for this datastore
//   also sets fState to dss_done when finished
bool TLocalEngineDS::engGenerateMapItems(TMapCommand *aMapCommandP, cAppCharP aLocalIDPrefix)
{
  #ifdef USE_SML_EVALUATION
  sInt32 leavefree = fSessionP->getNotUsableBufferBytes();
  #else
  sInt32 freeroom = fSessionP->getFreeCommandSize();
  #endif

  TStringToStringMap::iterator pos=fPendingAddMaps.begin();
  PDEBUGBLOCKFMT(("MapGenerate","Generating Map items...","datastore=%s",getName()));
  do {
    // check if already done
    if (pos==fPendingAddMaps.end()) break; // done
    // get ID
    string locID = (*pos).first;
    dsFinalizeLocalID(locID); // make sure we have the permanent version in case datastore implementation did deliver temp IDs
    // create prefixed version of ID
    string prefixedLocID;
    AssignString(prefixedLocID, aLocalIDPrefix); // init with prefix (if any)
    prefixedLocID += locID; // append local ID
    // add it to map command
    aMapCommandP->addMapItem(prefixedLocID.c_str(),(*pos).second.c_str());
    // check if we could send this command
    #ifdef USE_SML_EVALUATION
    if (
      (aMapCommandP->evalIssue(
        fSessionP->peekNextOutgoingCmdID(),
        fSessionP->getOutgoingMsgID()
      ) // what is left in buffer after sending Map so far
      >=leavefree) // all this must still be more than what we MUST leave free
    )
    #else
    if (freeroom>aMapCommandP->messageSize())
    #endif
    {
      // yes, it should work
      PDEBUGPRINTFX(DBG_PROTO,(
        "Mapitem generated: localID='%s', remoteID='%s'",
        prefixedLocID.c_str(),
        (*pos).second.c_str()
      ));
      // move sent ones to unconfirmed list (Note: use real locID, without prefix!)
      fUnconfirmedMaps[locID]=(*pos).second;
      // remove item from to-be-sent list
      TStringToStringMap::iterator temp_pos = pos++; // make copy and set iterator to next
      fPendingAddMaps.erase(temp_pos); // now entry can be deleted (N.M. Josuttis, pg204)
    }
    else {
      // no room for this item in this message, interrupt now
      // - delete already added item again
      aMapCommandP->deleteLastMapItem();
      // - interrupt here
      PDEBUGPRINTFX(DBG_PROTO,("Interrupted generating Map items because max message size reached"))
      PDEBUGENDBLOCK("MapGenerate");
      return false;
    }
  } while(true);
  // done
  // if we are dataaccessdone or more -> end of map phase for this datastore
  if (testState(dssta_dataaccessdone)) {
    changeState(dssta_clientmapssent,true);
    PDEBUGPRINTFX(DBG_PROTO,("Finished generating Map items, server has finished <sync>, we are done now"))
  }
  #ifdef SYDEBUG
  // else if we are not yet dssta_syncgendone -> this is the end of a early pending map send
  else if (!dbgTestState(dssta_syncgendone)) {
    PDEBUGPRINTFX(DBG_PROTO,("Finished sending cached Map items from last session"))
  }
  // otherwise, we are not really finished with the maps yet (but with the current map command)
  else {
    PDEBUGPRINTFX(DBG_PROTO,("Finished generating Map items for now, but server still sending <Sync>"))
  }
  #endif
  PDEBUGENDBLOCK("MapGenerate");
  return true;
} // TLocalEngineDS::engGenerateMapItems


#endif // SYSYNC_SERVER



// - called to mark an already generated (but probably not sent or not yet statused) item
//   as "to-be-resumed", by localID or remoteID (latter only in server case).
//   NOTE: This must be repeatable without side effects, as server must mark/save suspend state
//         after every request (and not just at end of session)
void TLocalEngineDS::engMarkItemForResume(cAppCharP aLocalID, cAppCharP aRemoteID, bool aUnSent)
{
  #ifdef SUPERDATASTORES
  // if we are acting as a subdatastore, aLocalID might be prefixed
  if (fAsSubDatastoreOf && aLocalID) {
    // remove prefix
    aLocalID = fAsSubDatastoreOf->removeSubDSPrefix(aLocalID, this);
  }
  #endif
  #ifdef SYSYNC_SERVER
  // Now mark for resume
  if (IS_SERVER && aLocalID && *aLocalID) {
    // localID can be a translated localid at this point (for adds), so check for it
    string localid=aLocalID;
    obtainRealLocalID(localid);
    logicMarkItemForResume(localid.c_str(), aRemoteID, aUnSent);
  }
  else
  #endif
  {
    // localID not used or client (which never has tempGUIDs)
    logicMarkItemForResume(aLocalID, aRemoteID, aUnSent);
  }
} // TLocalEngineDS::engMarkItemForResume


// - called to mark an already generated (but probably not sent or not yet statused) item
//   as "to-be-resent", by localID or remoteID (latter only in server case).
void TLocalEngineDS::engMarkItemForResend(cAppCharP aLocalID, cAppCharP aRemoteID)
{
  #ifdef SUPERDATASTORES
  // if we are acting as a subdatastore, aLocalID might be prefixed
  if (fAsSubDatastoreOf && aLocalID) {
    // remove prefix
    aLocalID = fAsSubDatastoreOf->removeSubDSPrefix(aLocalID, this);
  }
  #endif
  // a need to resend is always a problem with the remote (either explicit non-ok status
  // received or implicit like data size too big to be sent at all due to maxmsgsize or
  // maxobjsize restrictions.
  fRemoteItemsError++;
  // now mark for resend
  #ifdef SYSYNC_SERVER
  if (IS_SERVER && aLocalID && *aLocalID) {
    // localID can be a translated localid at this point (for adds), so check for it
    string localid=aLocalID;
    obtainRealLocalID(localid);
    logicMarkItemForResend(localid.c_str(), aRemoteID);
  }
  else
  #endif
  {
    // localID not used or client (which never has tempGUIDs)
    logicMarkItemForResend(aLocalID, aRemoteID);
  }
} // TLocalEngineDS::engMarkItemForResend





// @brief save everything needed to resume later, in case we get suspended
/// - Might be called multiple times during a session, must make sure every time
///   that the status is correct, that is, previous suspend state is erased
localstatus TLocalEngineDS::engSaveSuspendState(bool aAnyway)
{
  // only save here if not aborted already (aborting saves the state immediately)
  // or explicitly requested
  if (aAnyway || !isAborted()) {
    // only save if DS 1.2 and supported by DB
    if ((fSessionP->getSyncMLVersion()>=syncml_vers_1_2) && dsResumeSupportedInDB()) {
      PDEBUGBLOCKFMT(("SaveSuspendState","Saving state for suspend/resume","datastore=%s",getName()));
      // save alert state (if not explicitly prevented)
      fResumeAlertCode=fPreventResuming ? 0 : fAlertCode;
      if (fResumeAlertCode) {
        if (fPartialItemState!=pi_state_save_outgoing) {
          // ONLY if we have no request for saving an outgoing item state already,
          // we possibly need to save a pending incoming item
          // if there is an incompletely received item, let it update Partial Item (fPIxxx) state
          // (if it is an item of this datastore, that is).
          if (fSessionP->fIncompleteDataCommandP)
            fSessionP->fIncompleteDataCommandP->updatePartialItemState(this);
        }
        // this makes sure that only ungenerated (but to-be generated) items will be
        // marked for resume. Items that have been generated in this session (but might
        // have been marked for resume in a previous session must no longer be marked
        // after this call.
        // This also includes saving state for a partially sent item so we could resume it (fPIxxx)
        logicMarkOnlyUngeneratedForResume();
        // then, we need to additionally mark those items for resume which have been
        // generated, but not yet sent or sent but not received status so far.
        fSessionP->markPendingForResume(this);
      }
      // let datastore make all this persistent
      // NOTE: this must happen even if we have no suspend state here,
      //   as marked-for-resends need to be saved here as well.
      localstatus sta=logicSaveResumeMarks();
      if (sta!=LOCERR_OK) {
        PDEBUGPRINTFX(DBG_ERROR,("Error saving suspend state with logicSaveResumeMarks(), status=%hd",sta));
      }
      PDEBUGENDBLOCK("SaveSuspendState");
      return sta;
    }
    // resume not supported due to datastore or SyncML version<1.2 -> ok anyway
    PDEBUGPRINTFX(DBG_PROTO,("SaveSuspendState not possible (SyncML<1.2 or not supported by DB)"));
  }
  return LOCERR_OK;
} // TLocalEngineDS::engSaveSuspendState



} // namespace sysync

/* end of TLocalEngineDS implementation */

// eof
