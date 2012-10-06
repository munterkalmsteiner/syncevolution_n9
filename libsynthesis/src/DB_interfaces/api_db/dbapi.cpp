/*
 *  File:    dbapi.cpp
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  Programming interface between Synthesis SyncML engine
 *  and a database structure: C++ class interface
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 */

#include "dbapi.h"
#include "SDK_util.h"
#include "SDK_support.h"
#include "DLL_interface.h"

#ifndef SYSYNC_ENGINE
#include "stringutil.h"
#endif

#ifdef JNI_SUPPORT
#include "JNI_interface.h"
#endif


// the simple demo adapter
#ifdef DBAPI_DEMO
  namespace SDK_demodb {
    #include "dbapi_include.h"
  } // namespace
#endif

// more built-in adapters of the same kind can be used
#ifdef DBAPI_EXAMPLE
  namespace example1 {
    #include "dbapi_include.h"
  } // namespace
  namespace example2 {
    #include "dbapi_include.h"
  } // namespace
#endif

// a silent adapter, which always returns OK
#ifdef DBAPI_SILENT
  namespace silent {
    #include "dbapi_include.h"
  } // namespace
#endif

// the textdb adapter
#ifdef DBAPI_TEXT
  namespace SDK_textdb {
    #include "dbapi_include.h"
  } // namespace
#endif

// the snowwhite/oceanblue adapter
#ifdef DBAPI_SNOWWHITE
  namespace oceanblue {
    #include "dbapi_include.h"
  } // namespace
#endif

// the JNI bridge to Java
#if defined JNI_SUPPORT && defined PLUGIN_DLL
  namespace SDK_jni {
    #include "dbapi_include.h"
  } // namespace
#endif

// the C# bridge
#if defined CSHARP_SUPPORT && defined PLUGIN_DLL
  namespace SDK_csharp {
    #include "dbapi_include.h"
  } // namespace
#endif

// the bridge to OMA-DS 1.2 FILEOBJ
#ifdef DBAPI_FILEOBJ
  namespace SDK_fileobj {
    #include "dbapi_include.h"
  } // namespace
#endif

// the dbapi logger bridge
#ifdef DBAPI_LOGGER
  namespace logger {
    #include "dbapi_include.h"
  } // namespace
#endif


// some internal modules do not have the full set of routines
#define DISABLE_PLUGIN_SESSIONAUTH    1
#define DISABLE_PLUGIN_DEVICEADMIN    1
#define DISABLE_PLUGIN_DATASTOREADMIN 1
#define DISABLE_PLUGIN_ADAPTITEM      1

// the bridge via tunnel to any datastore inside the engine
#if defined DBAPI_TUNNEL && defined PLUGIN_DLL
  namespace SDK_tunnel {
    #include "dbapi_include.h"
  } // namespace
#endif


// iPhone plugins are AsKey only
#define DISABLE_PLUGIN_DATASTOREDATA_STR
// static plugins required for iPhoneOS (no DLLs allowed)
#ifdef IPHONE_PLUGINS_STATIC
  #ifdef HARDCODED_CONTACTS
    namespace iPhone_addressbook {
      #include "dbapi_include.h"
    } // namespace
  #endif

  #ifdef HARDCODED_CALENDAR
    namespace iPhone_calendar {
      #include "dbapi_include.h"
    } // namespace
  #endif

  #ifdef HARDCODED_TODO
    namespace iPhone_todos {
      #include "dbapi_include.h"
    } // namespace
  #endif

  #ifdef HARDCODED_NOTES
    namespace iPhone_notes {
      #include "dbapi_include.h"
    } // namespace
  #endif

  #ifdef HARDCODED_EMAILS
    namespace iPhone_emails {
      #include "dbapi_include.h"
    } // namespace
  #endif

  #ifdef HARDCODED_CUSTOM
    namespace iPhone_dbplugin1 {
      #include "dbapi_include.h"
    } // namespace

    namespace iPhone_dbplugin2 {
      #include "dbapi_include.h"
    } // namespace

    namespace iPhone_dbplugin3 {
      #include "dbapi_include.h"
    } // namespace

    namespace iPhone_dbplugin4 {
      #include "dbapi_include.h"
    } // namespace
  #endif
#endif
#undef DISABLE_PLUGIN_DATASTOREDATA_STR


#undef  DISABLE_PLUGIN_ADAPTITEM
#define DISABLE_PLUGIN_DATASTOREDATA 1

// the "adaptitem" module does not contain most of the routines
#ifdef ADAPTITEM_SUPPORT
  namespace SDK_adapt {
    #include "dbapi_include.h"
  } // namespace
#endif

// the UI demo adapter is something special
#define DISABLE_PLUGIN_ADAPTITEM 1
#define  ENABLE_PLUGIN_UI        1

#ifdef UIAPI_DEMO
  namespace SDK_ui {
    #include "dbapi_include.h"
  } // namespace
#endif

#undef DISABLE_PLUGIN_ADAPTITEM
#undef DISABLE_PLUGIN_SESSIONAUTH
#undef DISABLE_PLUGIN_DEVICEADMIN
#undef DISABLE_PLUGIN_DATASTOREADMIN
#undef DISABLE_PLUGIN_DATASTOREDATA
// all should be switched on now for "no_dbapi"



// ------------------------------------------------------------
#define MyDB "DBApi" // identifier of the sandwich layer


namespace no_dbapi {
  #include "dbapi_include.h"

  // combine the definitions of different namespaces
  using sysync::DB_Callback;
  using sysync::UI_Call_In;
  using sysync::ItemID;
  using sysync::MapID;
  using sysync::cItemID;
  using sysync::cMapID;

  using sysync::LOCERR_NOTIMP;
  using sysync::Password_Mode_Undefined;

  // default routines, if no context is available
  // all of them are returning false or 0

  /* --- MODULE ------------------------------------------------------------------------------------ */
  TSyError Module_CreateContext      ( CContext*    mc,    cAppCharP /*   moduleName */,
                                                           cAppCharP /*      subName */,
                                                           cAppCharP /* mContextName */,
                                                         DB_Callback /* mCB */ )   { *mc= 0; return LOCERR_OK;    }

  CVersion Module_Version            ( CContext  /* mc */ )               /* invalid */    { return VP_BadVersion;}
  TSyError Module_Capabilities       ( CContext  /* mc */,  appCharP *aCapaP )
                                                                   { *aCapaP= NULL;          return DB_NotFound;  }
  TSyError Module_PluginParams       ( CContext  /* mc */, cAppCharP /* mConfigParams */,
                                                            CVersion /* engineVersion */ ) { return DB_Error;     }
  void     Module_DisposeObj         ( CContext  /* mc */,     void* /* memory */ )        { }
  TSyError Module_DeleteContext      ( CContext  /* mc */ )                                { return LOCERR_OK;    }


  /* --- SESSION ----------------------------------------------------------------------------------- */
  TSyError Session_CreateContext     ( CContext* /* sc */, cAppCharP  /* sessionName */,
                                                         DB_Callback  /* sCB */          ) { return DB_Fatal;     }

  TSyError Session_AdaptItem         ( CContext  /* sc */,  appCharP* /* sItemData1  */,
                                                            appCharP* /* sItemData2  */,
                                                            appCharP* /* sLocalvars  */,
                                                              uInt32  /* sIdentifier */ )  { return DB_Forbidden; }

  TSyError Session_CheckDevice       ( CContext  /* sc */, cAppCharP  /* aDeviceID */,
                                                            appCharP* /* sDevKey */,
                                                            appCharP* /* nonce */ )        { return DB_Forbidden; }
  sInt32   Session_PasswordMode      ( CContext  /* sc */ )                     { return Password_Mode_Undefined; }
  TSyError Session_Login             ( CContext  /* sc */, cAppCharP  /* sUsername */,
                                                            appCharP* /* sPassword */,
                                                            appCharP* /* sUsrKey */ )      { return DB_Forbidden; }
  TSyError Session_Logout            ( CContext  /* sc */ )                                { return DB_Forbidden; }

  TSyError Session_GetNonce          ( CContext  /* sc */,  appCharP* /* nonce */ )        { return DB_Forbidden; }
  TSyError Session_SaveNonce         ( CContext  /* sc */, cAppCharP  /* nonce */ )        { return DB_Forbidden; }
  TSyError Session_SaveDeviceInfo    ( CContext  /* sc */, cAppCharP  /* aDeviceInfo */  ) { return DB_Forbidden; }
  TSyError Session_GetDBTime         ( CContext  /* sc */,  appCharP* /* currentDBTime */) { return DB_Forbidden; }

  void     Session_DisposeObj        ( CContext  /* sc */,     void*  /* memory */ )       { }
  void     Session_ThreadMayChangeNow( CContext  /* sc */ )                                { }
  void     Session_DispItems         ( CContext  /* sc */,     bool, cAppCharP )           { }
  TSyError Session_DeleteContext     ( CContext  /* sc */ )                                { return DB_Fatal; }


  /* --- DATASTORE --------------------------------------------------------------------------------- */
  /* ----- OPEN ------------------------ */
  TSyError CreateContext     ( CContext* /* co */, cAppCharP /* aContextName */, DB_Callback /* aCB */,
                                                   cAppCharP /* sDevKey */,
                                                   cAppCharP /* sUsrKey */ )       { return DB_Fatal; }
  uInt32   ContextSupport    ( CContext  /* co */, cAppCharP /* aContextRules */ ) { return 0; }
  uInt32   FilterSupport     ( CContext  /* co */, cAppCharP /* aFilterRules  */ ) { return 0; }


  /* ----- ADMINISTRATION -------------- */
  TSyError LoadAdminData     ( CContext  /* co */, cAppCharP  /* aLocDB    */,
                                                   cAppCharP  /* aRemDB    */,
                                                    appCharP* /* adminData */ )              { return DB_Forbidden; }
  TSyError LoadAdminDataAsKey( CContext  /* co */, cAppCharP  /* aLocDB    */,
                                                   cAppCharP  /* aRemDB    */,
                                                        KeyH  /* adminKey  */ )              { return DB_Forbidden; }

  TSyError SaveAdminData     ( CContext  /* co */, cAppCharP  /* adminData */ )              { return DB_Forbidden; }
  TSyError SaveAdminDataAsKey( CContext  /* co */,      KeyH  /* adminKey  */ )              { return DB_Forbidden; }

  bool     ReadNextMapItem   ( CContext  /* co */,     MapID  /* mID */, bool /* aFirst */ ) { return false;        }
  TSyError   InsertMapItem   ( CContext  /* co */,    cMapID  /* mID */ )                    { return DB_Forbidden; }
  TSyError   UpdateMapItem   ( CContext  /* co */,    cMapID  /* mID */ )                    { return DB_Forbidden; }
  TSyError   DeleteMapItem   ( CContext  /* co */,    cMapID  /* mID */ )                    { return DB_Forbidden; }


  /* ----- GENERAL --------------------- */
  void DisposeObj           ( CContext  /* co */, void* /* memory */ )            { }
  void ThreadMayChangeNow   ( CContext  /* co */ )                                { }
  void WriteLogData         ( CContext  /* co */,       cAppCharP /* logData */ ) { }
  void DispItems            ( CContext  /* co */, bool, cAppCharP )               { }

  /* ----- "script-like" ADAPT --------- */
  TSyError AdaptItem        ( CContext  /* co */,  appCharP* /* aItemData1  */,
                                                   appCharP* /* aItemData2  */,
                                                   appCharP* /* aLocalvars  */,
                                                     uInt32  /* aIdentifier */ ) { return DB_Forbidden; }

  /* ----- READ ------------------------ */
  TSyError StartDataRead    ( CContext  /* co */, cAppCharP  /*   lastToken */,
                                                  cAppCharP  /* resumeToken */ )                       { return DB_Fatal; }

  TSyError ReadNextItem     ( CContext  /* co */,    ItemID  /* aID */,    appCharP* /* aItemData */,
                                                     sInt32* /* aStatus */,    bool  /* aFirst */ )    { return DB_Fatal; }
  TSyError ReadNextItemAsKey( CContext  /* co */,    ItemID  /* aID */,        KeyH  /* aItemKey */,
                                                     sInt32* /* aStatus */,    bool  /* aFirst */ )    { return DB_Fatal; }

  TSyError ReadItem         ( CContext  /* co */,   cItemID  /* aID */,    appCharP* /* aItemData */ ) { return DB_Fatal; }
  TSyError ReadItemAsKey    ( CContext  /* co */,   cItemID  /* aID */,        KeyH  /* aItemKey  */ ) { return DB_Fatal; }

  TSyError ReadBlob         ( CContext  /* co */,   cItemID  /* aID */,   cAppCharP  /* aBlobID  */,
                                                 appPointer* /* aBlkPtr */, memSize* /* aBlkSize */,
                                                                            memSize* /* aTotSize */,
                                                       bool  /* aFirst */,     bool* /* aLast    */ )  { return DB_Fatal; }
  TSyError EndDataRead      ( CContext  /* co */ )                                                     { return DB_Fatal; }


  /* ----- WRITE ----------------------- */
  TSyError StartDataWrite   ( CContext  /* co */ )                                                 { return DB_Fatal; }

  TSyError InsertItem       ( CContext  /* co */, cAppCharP /* aItemData */,  ItemID /* newID */ ) { return DB_Fatal; }
  TSyError InsertItemAsKey  ( CContext  /* co */,      KeyH /* aItemKey  */,  ItemID /* newID */ ) { return DB_Fatal; }
  TSyError UpdateItem       ( CContext  /* co */, cAppCharP /* aItemData */, cItemID /*   aID */,
                                                                              ItemID /* updID */ ) { return DB_Fatal; }
  TSyError UpdateItemAsKey  ( CContext  /* co */,      KeyH /* aItemKey  */, cItemID /*   aID */,
                                                                              ItemID /* updID */ ) { return DB_Fatal; }

  TSyError MoveItem         ( CContext  /* co */,   cItemID /* aID */,  cAppCharP /* newParID */ ) { return DB_Fatal; }
  TSyError DeleteItem       ( CContext  /* co */,   cItemID /* aID */ )                            { return DB_Fatal; }
  TSyError FinalizeLocalID  ( CContext  /* co */,   cItemID /* aID */,        ItemID /* updID */ ) { return DB_Fatal; }
  TSyError DeleteSyncSet    ( CContext  /* co */ )                                                 { return DB_Fatal; }
  TSyError WriteBlob        ( CContext  /* co */,   cItemID /* aID */,  cAppCharP /*  aBlobID */,
                                                 appPointer /* aBlkPtr */,memSize /* aBlkSize */,
                                                                          memSize /* aTotSize */,
                                                       bool /* aFirst */,    bool /* aLast    */ ) { return DB_Fatal; }

  // There are older implementations, where "DeleteBlob" is not yet available.
  TSyError DeleteBlob       ( CContext  /* co */,   cItemID /* aID */,  cAppCharP /* aBlobID  */ ) { return LOCERR_NOTIMP; }
  TSyError EndDataWrite     ( CContext  /* co */,      bool /* success */, char** /* newToken */ ) { return DB_Fatal; }

  /* ----- CLOSE ----------------------- */
  TSyError DeleteContext    ( CContext  /* co */ ) { return DB_Fatal; }


  /* ---- UI API ----------------------- */
  TSyError UI_CreateContext ( CContext* /* co */, cAppCharP /* uiName */, UI_Call_In /* uCI */ ) { return DB_Fatal; }
  TSyError UI_RunContext    ( CContext  /* co */ )                                               { return DB_Fatal; }
  TSyError UI_DeleteContext ( CContext  /* co */ )                                               { return DB_Fatal; }
} // namespace no_dbapi



/* ---- "TDB_Api_Str" implementation --------------------------------------- */
namespace sysync {

TDB_Api_Str::TDB_Api_Str()            { clear(); }                         // constructor
TDB_Api_Str::TDB_Api_Str( string &s ) { clear(); fStr= (char*)s.c_str(); } // alternative constructor
TDB_Api_Str::~TDB_Api_Str()           { DisposeStr(); }                    // destructor


void TDB_Api_Str::AssignStr( CContext aContext, DisposeProc aDisposeProc, bool itself )
{
  fContext= aContext;
  fItself = itself;

  if (fStr!=NULL) fDisposeProc= aDisposeProc;
  else          { fDisposeProc= NULL; fStr= const_cast<char *>(""); /* this is okay because the string is not going to be disposed */ }
} // AssignStr



void TDB_Api_Str::DisposeStr()
{
  // nothing to do, already disposed ?
  if (fStr        !=NULL &&
      fDisposeProc!=NULL) {
    void*        ref= const_cast<char *>(fStr);
    if (fItself) ref= this; fDisposeProc( fContext,ref );
  } // if

  clear(); // don't do it again !!
} // DisposeStr



/* Dispose a string which has been allocated with 'StrAlloc'
 * Used as handler for:
 *    typedef void( *DisposeProc)( long aContext, void* s );
 */
static void StrDispose_Handler( CContext /* aContext */, void* s ) {
  StrDispose( s );
} // StrDispose_Handler



/* ---------- allocate local memory for a string ------------- */
void TDB_Api_Str::LocalAlloc( CContext aContext, cAppCharP str )
{
  fStr= StrAlloc( str );
  AssignStr( aContext, (DisposeProc)StrDispose_Handler );
} // LocalAlloc



/* ---- "DB_Api_Blk" implementation --------------------------------------- */
TDB_Api_Blk::TDB_Api_Blk()  { clear();      } // constructor
TDB_Api_Blk::~TDB_Api_Blk() { DisposeBlk(); } //  destructor


void TDB_Api_Blk::AssignBlk( CContext aContext, DisposeProc aDisposeProc, bool itself )
{
  fContext= aContext;
  fItself = itself;

  if (fPtr!=NULL) fDisposeProc= (DisposeProc)aDisposeProc;
  else          { fDisposeProc= NULL; fSize= 0; }
} // AssignBlk



void TDB_Api_Blk::DisposeBlk()
{
  // nothing to do, already disposed ?
  if (fPtr        !=NULL &&
      fDisposeProc!=NULL) {
    void*        ref= fPtr;
    if (fItself) ref= this; fDisposeProc( fContext,ref );
  } // if

  clear(); // don't do it again !!
} // DisposeBlk


// Returns true if <ps> is name or #<n>
static bool BuiltIn( string &ps, int &n, cAppCharP name )
{
  if (strucmp( ps.c_str(), name      )==0)             return true;

  string                   s= "#" + IntStr( n++ );
  if (strucmp( ps.c_str(), s.c_str() )==0) { ps= name; return true; }

  return false;
} // BuiltIn



/* --- connect to library ------------------------------------------------- */
/*! These are the built-in linked libraries, which can be accessed directly
 *  NOTE: Some of them are normally switched off with compile options
 *
 *  SDK plugin modules can be also linked into the engine directly.
 *  Normally the compiler must be able to link all SDK routines.
 *  The SDK concept allows to have some routines unimplemented,
 *  they will be provided by the "no_dbapi" built-in module.
 */
static TSyError DBApi_LibAssign( appPointer aMod, string &ps, appPointer aField, int aFSize,
                                                  cAppCharP aKey= "" )
{
  TSyError err= DB_NotFound;
  int n= 0; // incremental counter starting with 0

                       // the blind adapter is always available and acts as default as well
  if        (strucmp( ps.c_str(),   ""   )==0 ||
             BuiltIn( ps,n, "no_dbapi"   )) err=    no_dbapi::AssignMethods( aMod, aField,aFSize, aKey );

  #ifdef DBAPI_DEMO    // demo (C) adapter, which will be delivered with SDK
    else if (BuiltIn( ps,n, "SDK_demodb" )) err=  SDK_demodb::AssignMethods( aMod, aField,aFSize, aKey );
  #endif

  #ifdef DBAPI_EXAMPLE // other (C++) linked versions of the demo adapter (for test)
    else if (BuiltIn( ps,n, "example1"   )) err=    example1::AssignMethods( aMod, aField,aFSize, aKey );
    else if (BuiltIn( ps,n, "example2"   )) err=    example2::AssignMethods( aMod, aField,aFSize, aKey );
  #endif

  #ifdef DBAPI_SILENT  // a silent adapter, which always says, everthing is ok
    else if (BuiltIn( ps,n, "silent"     )) err=      silent::AssignMethods( aMod, aField,aFSize, aKey );
  #endif

  #ifdef DBAPI_TEXT    // "text_db" implementation, which emulates the "textdb"
    else if (BuiltIn( ps,n, "SDK_textdb" )) err=  SDK_textdb::AssignMethods( aMod, aField,aFSize, aKey );
  #endif

  #ifdef DBAPI_SNOWWHITE // "oceanblue/snowwhite" implementation
    else if (BuiltIn( ps,n, "snowwhite"  )) err=   oceanblue::AssignMethods( aMod, aField,aFSize, aKey );
  #endif

  #ifdef PLUGIN_DLL    // plugin bridges
    #ifdef    JNI_SUPPORT  // the Java (JNI) adapter
      else if (BuiltIn( ps,n, "JNI"      )) err=     SDK_jni::AssignMethods( aMod, aField,aFSize, aKey );
    #endif

    #ifdef CSHARP_SUPPORT  // the C# adapter
      else if (BuiltIn( ps,n, "CSHARP"   )) err=  SDK_csharp::AssignMethods( aMod, aField,aFSize, aKey );
    #endif

    #ifdef   DBAPI_TUNNEL  // direct access to internal adapters
      else if (BuiltIn( ps,n, "tunnel"   )) err=  SDK_tunnel::AssignMethods( aMod, aField,aFSize, aKey );
    #endif

    #ifdef   DBAPI_LOGGER  // dbapi logger bridge
      else if (BuiltIn( ps,n, "logger"   )) err=      logger::AssignMethods( aMod, aField,aFSize, aKey );
    #endif
  #endif

  #ifdef DBAPI_FILEOBJ     // dbapi fileobj
    else if (BuiltIn( ps,n, "FILEOBJ"    )) err= SDK_fileobj::AssignMethods( aMod, aField,aFSize, aKey );
  #endif

  #ifdef ADAPTITEM_SUPPORT // "script-like" adapt item
    else if (BuiltIn( ps,n, "ADAPTITEM"  )) err=   SDK_adapt::AssignMethods( aMod, aField,aFSize, aKey );
  #endif

  #ifdef UIAPI_DEMO   // demo (C++) UI interface adapter, which will be delivered with SDK
    else if (BuiltIn( ps,n, "SDK_ui"     )) err=      SDK_ui::AssignMethods( aMod, aField,aFSize, aKey );
  #endif

  #ifdef IPHONE_PLUGINS_STATIC   // iPhone OS does not allow DLL plugins at this time, so we must link them statically
    #ifdef HARDCODED_CONTACTS
    else if (BuiltIn( ps,n, "iPhone_addressbook")) err= iPhone_addressbook::AssignMethods( aMod, aField,aFSize, aKey );
    #endif
    #ifdef HARDCODED_CALENDAR
    else if (BuiltIn( ps,n, "iPhone_calendar"   )) err=    iPhone_calendar::AssignMethods( aMod, aField,aFSize, aKey );
    #endif
    #ifdef HARDCODED_TODO
    else if (BuiltIn( ps,n, "iPhone_todos"      )) err=       iPhone_todos::AssignMethods( aMod, aField,aFSize, aKey );
    #endif
    #ifdef HARDCODED_NOTES
    else if (BuiltIn( ps,n, "iPhone_notes"      )) err=       iPhone_notes::AssignMethods( aMod, aField,aFSize, aKey );
    #endif
    #ifdef HARDCODED_EMAILS
    else if (BuiltIn( ps,n, "iPhone_emails"     )) err=      iPhone_emails::AssignMethods( aMod, aField,aFSize, aKey );
    #endif

    #ifdef HARDCODED_CUSTOM
    else if (BuiltIn( ps,n, "iPhone_dbplugin1" )) err=  iPhone_dbplugin1::AssignMethods( aMod, aField,aFSize, aKey );
    else if (BuiltIn( ps,n, "iPhone_dbplugin2" )) err=  iPhone_dbplugin2::AssignMethods( aMod, aField,aFSize, aKey );
    else if (BuiltIn( ps,n, "iPhone_dbplugin3" )) err=  iPhone_dbplugin3::AssignMethods( aMod, aField,aFSize, aKey );
    else if (BuiltIn( ps,n, "iPhone_dbplugin4" )) err=  iPhone_dbplugin4::AssignMethods( aMod, aField,aFSize, aKey );
    #endif
  #endif

  else  if  (strucmp( ps.c_str(), "#" )==0) err= LOCERR_UNKSUBSYSTEM;
  else  if (!BuiltIn( ps,n, "" )) ModuleConnectionError( NULL, AddBracks( ps ).c_str() );

  return err;
} // DBApi_LibAssign



/* ---- wrapper for DB_Callback -------------------------------------------- */
TDB_Api_Callback::TDB_Api_Callback() { InitCallback( &Callback, DB_Callback_Version, NULL,NULL ); }



/* ---- "DB_Api_Config" implementation ------------------------------------- */
TDB_Api_Config::TDB_Api_Config()
{
  fMod= NULL;
  clear();

  fTSTversion= VP_BadVersion;
} // constructor


TDB_Api_Config::~TDB_Api_Config()
{
  Disconnect();
} //  destructor



static void connect_no_dbapi( appPointer &aMod, API_Methods &m )
{
  string no_dbapi= "";

  DisconnectModule( aMod    ); // avoid memory leak
  ConnectModule   ( aMod,"" ); // default: no db_api -> all methods return false
//---- module --------------------------------
  DBApi_LibAssign ( aMod,no_dbapi, &m.start,        sizeof(m.start),        Plugin_Start );
  DBApi_LibAssign ( aMod,no_dbapi, &m.param,        sizeof(m.param),        Plugin_Param );
  DBApi_LibAssign ( aMod,no_dbapi, &m,              sizeof(m) );

//---- session -------------------------------
  DBApi_LibAssign ( aMod,no_dbapi, &m.se,           sizeof(m.se),           Plugin_Session      );
  DBApi_LibAssign ( aMod,no_dbapi, &m.se.seAdapt,   sizeof(m.se.seAdapt),   Plugin_SE_Adapt     );
  DBApi_LibAssign ( aMod,no_dbapi, &m.se.seAuth,    sizeof(m.se.seAuth),    Plugin_SE_Auth      );
  DBApi_LibAssign ( aMod,no_dbapi, &m.se.dvAdmin,   sizeof(m.se.dvAdmin),   Plugin_DV_Admin     );
  DBApi_LibAssign ( aMod,no_dbapi, &m.se.dvTime,    sizeof(m.se.dvTime),    Plugin_DV_DBTime    );

//---- datastore -----------------------------
  DBApi_LibAssign ( aMod,no_dbapi, &m.ds,           sizeof(m.ds),           Plugin_Datastore    );
  DBApi_LibAssign ( aMod,no_dbapi, &m.ds.dsg,       sizeof(m.ds.dsg),       Plugin_DS_General   );
  DBApi_LibAssign ( aMod,no_dbapi, &m.ds.dsAdapt,   sizeof(m.ds.dsAdapt),   Plugin_DS_Adapt     );
  DBApi_LibAssign ( aMod,no_dbapi, &m.ds.dsAdm.str, sizeof(m.ds.dsAdm.str), Plugin_DS_Admin_Str );
  DBApi_LibAssign ( aMod,no_dbapi, &m.ds.dsAdm.key, sizeof(m.ds.dsAdm.key), Plugin_DS_Admin_Key );
  DBApi_LibAssign ( aMod,no_dbapi, &m.ds.dsAdm.map, sizeof(m.ds.dsAdm.map), Plugin_DS_Admin_Map );
  DBApi_LibAssign ( aMod,no_dbapi, &m.ds.dsData,    sizeof(m.ds.dsData),    Plugin_DS_Data      );
  DBApi_LibAssign ( aMod,no_dbapi, &m.ds.dsData.str,sizeof(m.ds.dsData.str),Plugin_DS_Data_Str  );
  DBApi_LibAssign ( aMod,no_dbapi, &m.ds.dsData.key,sizeof(m.ds.dsData.key),Plugin_DS_Data_Key  );
  DBApi_LibAssign ( aMod,no_dbapi, &m.ds.dsBlob,    sizeof(m.ds.dsBlob),    Plugin_DS_Blob      );
//---- ui context ----------------------------
  DBApi_LibAssign ( aMod,no_dbapi, &m.ui,           sizeof(m.ui),           Plugin_UI );
  DisconnectModule( aMod    ); // no longer used, avoid memory leak
} // connect_no_dbapi



void TDB_Api_Config::clear()
{
  connect_no_dbapi( fMod, m );

  fConnected = false;
  fADMIN_Info= false;
  fSDKversion= VP_BadVersion;
  fMODversion= VP_BadVersion;
  mContext   = 0;

  DB_Callback mCB= &fCB.Callback;
              mCB->cContext= 0; // set it to uninitialized default
              mCB->mContext= 0;
} // clear



// ---------------------------------------------------------------------
//! Module string disposer
void TDB_Api_Config::DisposeStr( TDB_Api_Str &s )
{
  fSList.remove( &s ); // remove it from the list first to avoid double free

  s.AssignStr( mContext, (DisposeProc)m.Module_DisposeObj );
  s.DisposeStr();
} // DisposeStr


//! Local wrapper, because it must be DisposeProc type and can't be C++ method
static void Local_Module_DisposeStr( CContext mObj, void* memory )
{
  TDB_Api_Config* c= (TDB_Api_Config*)mObj;
  TDB_Api_Str*    u= (TDB_Api_Str*)memory;
  c->DisposeStr( *u );
} // Local_Session_DisposeStr


void TDB_Api_Config::AssignStr( TDB_Api_Str &s )
{
  if (s.fStr==NULL) return;

  s.AssignStr( (CContext)this, Local_Module_DisposeStr, true );
  fSList.push_back( &s ); // add the element to the list
} // AssignStr



// ---------------------------------------------------------------------
// general datastore access ?
bool DSConnect( cAppCharP aItem )
{
  return FlagOK( aItem, Plugin_Datastore ) &&
       ( FlagOK( aItem, Plugin_DS_Admin  ) ||
         FlagOK( aItem, Plugin_DS_Data   ) ||
         FlagOK( aItem, Plugin_DS_Adapt  ) );
} // DSConnect



TSyError TDB_Api_Config::DBApi_Assign( cAppCharP aItem, appPointer aField,
                                                           memSize aFieldSize, cAppCharP aKey )
{
  TSyError err= LOCERR_OK;

  if (FlagOK( aItem,aKey )) {
    if (is_lib) err= DBApi_LibAssign( fMod, fPlugin, aField,aFieldSize, aKey );
    else {      err= DB_Forbidden; // only allowed, if PLUGIN_DLL is active
      #ifdef PLUGIN_DLL
                err= DBApi_DLLAssign( fMod,          aField,aFieldSize, aKey, false );
      #endif
    } // if

    DEBUG_Exotic_INT( &fCB.Callback,MyDB, "DBApi_Assign", "aKey='%s' (size=%d) err=%d",
                                                           aKey, aFieldSize,   err );
  } // if

  return err;
} // DBApi_Assign



// Returns the engine's SDK version
long TDB_Api_Config::EngineSDKVersion()
{                  // If not changed ...
  if (fTSTversion==VP_BadVersion) return Plugin_Version( 0 ); // Called here it's the engine's SDK version
  else                            return fTSTversion;         // modified for test
} // EngineSDKVersion



// Returns LOCERR_TOOOLD, if the <capa> version is too old
TSyError TDB_Api_Config::MinVersionCheck( string capa, CVersion &vMin )
{
  TSyError err= LOCERR_OK;
  vMin= 0;

  string                                   value;
  while (RemoveField( capa, CA_MinVersion, value )) {
    CVersion minVersion=        VersionNr( value );         // the minimum required version of the plugin
    CVersion engVersion= EngineSDKVersion  () | 0x000000ff; // the engine's version with max build number
    if      (engVersion<minVersion) err = LOCERR_TOOOLD;
    if      (vMin      <minVersion) vMin= minVersion;

    DB_Callback mCB= &fCB.Callback;
    DEBUG_INT ( mCB, MyDB, "MinVersionCheck", "engine=%08X >= %08X / err=%d",
                                               engVersion, minVersion, err );
    if (err) break;
  } // while

  return err;
} // MinVersionCheck


// Connect to internally linked library or external DLL, dependent on <aIsLib> and <allowDLL>
TSyError TDB_Api_Config::Connect( cAppCharP aModName, CContext &globContext,
                                  cAppCharP mContextName,
                                  bool aIsLib, bool allowDLL )
{
  TSyError err;
  string   gcs;

  if (fConnected) Disconnect();
  if (fConnected) return DB_Error; // still connected ? => error

  cAppCharP  x= "";
  DB_Callback     mCB= &fCB.Callback;
  CContext* mc=  &mCB->mContext; // store it at a temporary var
           *mc= 0;

  clear();
  is_lib  = IsLib( aModName );
  fModName=        aModName; // make a local copy

  if (aIsLib) { // && !is_lib) : special cases like "[aaa]!bbb" => "[[aaa]!bbb]"
    fModName= AddBracks( aModName );
    is_lib  = true;
  } // if

  GlobContext* g;
  GlobContext* gp;

  if (CB_OK( mCB,8 )) {         // <gContext> available ?
    mCB->gContext= globContext;

    for (int i= 1; i<=3; i++) { // create 3 empty elements
      gp= new GlobContext;      // create and init a new record
      gp->ref = NULL;
      gp->next= NULL;
      gp->cnt = 0;
      strcpy( gp->refName,"" );

      if (!mCB->gContext)  mCB->gContext= (CContext)gp; // assign it
      else {
               g= (GlobContext*)mCB->gContext; // or add it to the end of the chain
        while (g->next!=NULL) g= (GlobContext*)g->next;
               g->next= gp;
      } // if
    } // for
  } // if

  do { // exit part
    if (!is_lib) {
      x= "(DLL)";
      if (!allowDLL) { err= DB_Forbidden; break; }
    } // if

    WithSubSystem  ( fModName, fModMain, fModSub );
    bool in_bracks= CutBracks( fModMain );      // "[aaa xx]" => "aaa xx"
                     fOptions= fModMain;
    NextToken      ( fOptions, fModMain, " " ); // separate <mOptions>
    fModName=                  fModMain;
                    CutBracks( fModMain );
    fPlugin =                  fModMain;

    if (in_bracks) fModName = AddBracks( fModName );

    /* this mode is no longer supported ( moduleName:globContext ) */
    /* keep it for compatiblity */
    if (globContext) {
             g= (GlobContext*)globContext;
      while (g!=NULL) {
        if (strcmp( g->refName,"" )!=0) { // at least one field is set
          fModMain+= ":" + RefStr( globContext ); break;
        } // if

        g= g->next;
      } // while
    } // if

    if (!fOptions.empty()) fModMain+= " " + fOptions;

    // create some downwards compatibility info
    if   (CB_OK( mCB,4 )) {
      if     (allowDLL) mCB->allow_DLL_legacy= 0xffff; // Assign DLL allowance info
      else              mCB->allow_DLL_legacy= 0x0000;

      if (CB_OK( mCB,5 )) {
        mCB->allow_DLL= mCB->allow_DLL_legacy;
      } // if
    } // if

    // Connect "Module_CreateContext" and "Module_Version" first, to take decisions
    err= ConnectModule( fMod, fModName.c_str() );                      if (err) break;
    err=  DBApi_Assign( "", &m.start, sizeof(m.start), Plugin_Start ); if (err) break;

    Version_Func pv= (Version_Func)m.start.Module_Version;
    fSDKversion= pv( 0 );      // get the plugin's version before making the first tests
    DEBUG_INT( mCB,MyDB, "Connect", "fSDKversion=%s", VersionStr( fSDKversion ).c_str() );

    string sn= fModSub;
    if    (sn=="[#]" && !(fPlugin=="logger")) sn= ""; // remove the recursion breaker

    CreateM_Func p=  (CreateM_Func)m.start.Module_CreateContext;
        err=     p( &mContext, fModMain.c_str(), sn.c_str(), mContextName, mCB );
    if (err==LOCERR_ALREADY) err= LOCERR_OK; // this is not an error, just avoid multiple assignment
    DEBUG_INT( mCB,MyDB, "Connect", "mContext=%08X err=%d", mContext,err );

    if (err) break;
    fMODversion= pv( mContext ); // get the plugin's version before making the first tests
    mCB->cContext  = mContext;   // assign for the callback mechanism
    if (*mc==0) *mc= mContext;   // assign for session & datastores, if not already in use (e.g. for JNI)
  //err= VersionCheck(); if (err) break;

    TDB_Api_Str               aCapa;
    err=        Capabilities( aCapa ); if (err) break;
    cAppCharP             ca= aCapa.c_str();
    CVersion                  vMin;
    err= MinVersionCheck( ca, vMin );  if (err) break;
    fADMIN_Info=  FlagOK( ca, CA_ADMIN_Info, true );

    GetField( ca, CA_Description, fDesc );
    GetField( ca, CA_GlobContext, gcs   ); // is there a global context inside ?

  // !Supported( VP_EngineVersionParam ): only JNI signature changes
  // !Supported( VP_MD5_Nonce_IN       ): only JNI signature changes

    cAppCharP                             vda= Plugin_DS_Admin_Map;
    if (!Supported( VP_InsertMapItem   )) vda= Plugin_DS_Admin_OLD;
    cAppCharP                             vdd= Plugin_DS_Data;
    if (!Supported( VP_FLI_DSS         )) vdd= Plugin_DS_Data_OLD2;
    if (!Supported( VP_ResumeToken     )) vdd= Plugin_DS_Data_OLD1;
    cAppCharP                             vdb= Plugin_DS_Blob;
    if (!Supported( VP_BLOB_JSignature )) vdb= Plugin_DS_Blob_OLD2; // new BLOB signature
    if (!Supported( VP_DeleteBlob      )) vdb= Plugin_DS_Blob_OLD1;

  //---- module ---------------------------------
    if (!err) err=     DBApi_Assign( "", &m.param,        sizeof(m.param),        Plugin_Param );
    if (!err) err=     DBApi_Assign( "", &m,              sizeof(m) );

  //---- session --------------------------------
    if     (!err   &&      FlagOK  ( ca, Plugin_Session  )) {
                  err= DBApi_Assign( ca, &m.se,           sizeof(m.se),           Plugin_Session  );

      if   (!err   &&    ( FlagOK  ( ca, Plugin_SE_Auth  ) ||
                           FlagOK  ( ca, Plugin_DV_Admin ) )) {
                  err= DBApi_Assign( ca, &m.se.seAuth,    sizeof(m.se.seAuth),    Plugin_SE_Auth  );
        if (!err) err= DBApi_Assign( ca, &m.se.dvAdmin,   sizeof(m.se.dvAdmin),   Plugin_DV_Admin );

        if (!err   &&  Supported( VP_GetDBTime )) {
                  err= DBApi_Assign( ca, &m.se.dvTime,    sizeof(m.se.dvTime),    Plugin_DV_DBTime );
        } // if
      } // if

      if   (!err   &&  Supported( VP_AdaptItem )) {
                  err= DBApi_Assign( ca, &m.se.seAdapt,   sizeof(m.se.seAdapt),   Plugin_SE_Adapt );
      } // if
    } // if

  //---- datastore ------------------------------
    if     (!err   &&      FlagOK  ( ca, Plugin_Datastore )) {
      if   (!err   &&     DSConnect( ca )) {
                  err= DBApi_Assign( ca, &m.ds,           sizeof(m.ds),           Plugin_Datastore );
      } // if

      if   (!err   &&    ( FlagOK  ( ca, Plugin_DS_Admin ) ||
                           FlagOK  ( ca, Plugin_DS_Data  ) )) {
                  err= DBApi_Assign( ca, &m.ds.dsg,       sizeof(m.ds.dsg),       Plugin_DS_General );
        if (!err) err= DBApi_Assign( ca, &m.ds.dsBlob,    sizeof(m.ds.dsBlob),    vdb );
      } // if

      if   (!err   &&      FlagOK  ( ca, Plugin_DS_Admin )) {
        bool asK=          FlagOK  ( ca, CA_AdminAsKey, true );
        if  (asK) err= DBApi_Assign( ca, &m.ds.dsAdm.key, sizeof(m.ds.dsAdm.key),Plugin_DS_Admin_Key );

        bool asS= !asK ||  FlagBoth( ca, CA_AdminAsKey );
        if  (asS) err= DBApi_Assign( ca, &m.ds.dsAdm.str, sizeof(m.ds.dsAdm.str),Plugin_DS_Admin_Str );

        if   (!err) err= DBApi_Assign( ca, &m.ds.dsAdm.map, sizeof(m.ds.dsAdm.map), vda );
      } // if

      if   (!err) err= DBApi_Assign( ca, &m.ds.dsData,    sizeof(m.ds.dsData),    vdd );

      if   (!err   &&      FlagOK  ( ca, Plugin_DS_Data )) {
        bool asK=          FlagOK  ( ca, CA_ItemAsKey, true );
        if  (asK) err= DBApi_Assign( ca, &m.ds.dsData.key,sizeof(m.ds.dsData.key),Plugin_DS_Data_Key );

        bool asS= !asK ||  FlagBoth( ca, CA_ItemAsKey );
        if  (asS) err= DBApi_Assign( ca, &m.ds.dsData.str,sizeof(m.ds.dsData.str),Plugin_DS_Data_Str );
      } // if

      if   (!err   &&  Supported( VP_AdaptItem )) {
                  err= DBApi_Assign( ca, &m.ds.dsAdapt,   sizeof(m.ds.dsAdapt),   Plugin_DS_Adapt );
      } // if
    } // if

  //---- ui context -----------------------------
    if     (!err   &&        FlagOK( ca,Plugin_UI, true )) {
                  err= DBApi_Assign( ca, &m.ui,           sizeof(m.ui),           Plugin_UI );
    } // if
  } while (false); // end exit part

  if (err) clear();

  if (CB_OK( mCB,8 )) { globContext= mCB->gContext; // <gContext> available ?
    DeleteGlobContext ( globContext, mCB, true );   // remove elements with empty text and no ref
    mCB->gContext=      globContext;                // can be fully removed again (gContext==0)
  }
  else {
    // ---- the old fashioned way to do it ----
    if (!gcs.empty()) {
      uIntPtr u;
      HexStrToUIntPtr( gcs.c_str(), u );

      if (!globContext) globContext= (CContext)u;
      if  (globContext!=(CContext)u) { // link it if more than one element
        GlobContext* gc=       (GlobContext*)globContext;
                     gc->next= (GlobContext*)u;
      } // if
    } // if
    // -----------------------------------------
  } // if

  string s= "ok";
  if (err) {
    char     f[ 15 ];
    sprintf( f, "err=%d", err );
    s=       f;
  } // if

  DEBUG_INT( mCB,MyDB,"Connect", "%s '%s' %-9s", s.c_str(), fModName.c_str(), x );
  fConnected= !err;
  return       err;
} // Connect


/* Get the plug-in's version number */
long TDB_Api_Config::Version() { return fMODversion; }


/* Get the plug-in's capabilities */
TSyError TDB_Api_Config::Capabilities( TDB_Api_Str &aCapa )
{
  typedef TSyError (*CapabFunc)( CContext  mContext,
                                 appCharP *mCapabilities );

                              aCapa.DisposeStr();
  CapabFunc     p= (CapabFunc)m.start.Module_Capabilities;
  TSyError err= p( mContext, const_cast<char **>(&aCapa.fStr) );
  if     (!err)    AssignStr( aCapa );
  return   err;
} // Capabilities


/* Check, if <version_feature> is supported
 * NOTE: For the SyncML engine internally everything is supported
 *       This will be reflected with the fact that the engine's
 *       version is alway much much higher
 */
bool TDB_Api_Config::Supported( CVersion versionFeature ) {
  return              Feature_Supported( versionFeature, fSDKversion );
} // Supported



TSyError TDB_Api_Config::PluginParams( cAppCharP mConfigParams )
{
  typedef TSyError     (*PlugProc)( CContext mContext,
                                   cAppCharP mConfigParams,  CVersion engineVersion );
//typedef TSyError (*OLD_PlugProc)( CContext mContext,
//                                 cAppCharP mConfigParams ); // w/o <engineVersion>

  TSyError err;
  if (!fConnected) return DB_Error;

  /*
  // new param supported for Plugin Version >= 1.0.X.4
  if (Supported( VP_EngineVersionParam )) {
    PlugProc     p=     (PlugProc)m.param.Module_PluginParams;
    err=         p( mContext, mConfigParams, EngineSDKVersion() );
  }
  else {
    OLD_PlugProc p= (OLD_PlugProc)m.param.Module_PluginParams; // w/o the SDK version parameter
    err=         p( mContext, mConfigParams );
  } // if
  */

  PlugProc    p= (PlugProc)m.param.Module_PluginParams;
         err= p( mContext, mConfigParams, EngineSDKVersion() );
  if    (err==LOCERR_ALREADY) err= LOCERR_OK;
  return err;
} // PluginParams



// Set the SDK version
// *** override the internal version number / for test only ***
void TDB_Api_Config::SetVersion( long versionNr )
{
  fTSTversion= versionNr;
  DEBUG_INT( &fCB.Callback, MyDB,"SetVersion", "%08X", fTSTversion );
} // SetVersion



TSyError TDB_Api_Config::Disconnect()
{
  if (!fConnected) return DB_Error; // avoid double free

  // remove all still allocated elements before removing the content
  while (!fSList.empty()) DisposeStr( *fSList.front() );

  Context_Func  p= (Context_Func)m.Module_DeleteContext;
  TSyError err= p( mContext );

  fModName= "";
  DisconnectModule( fMod );
  fConnected= false;
  return err;
} // Disconnect



/* ------------------------------------------------------------------------- */
void DispGlobContext( CContext &globContext, DB_Callback mCB )
{
  cAppCharP DGC= "DispGlobContext";

  GlobContext*       g= (GlobContext*)globContext;
  DEBUG_Exotic_DB  ( mCB, MyDB,DGC, "g=%s mCB->gContext=%s",
                                     RefStr( g ).c_str(), RefStr( mCB->gContext ).c_str() );
  while             (g!=NULL) {
    GlobContext* nx= g->next;
    DEBUG_Exotic_DB( mCB, MyDB,DGC, "g=%s g->ref=%s g->cnt=%-3d '%s'",
                                     RefStr( g ).c_str(), RefStr( g->ref ).c_str(),
                                     g->cnt, g->refName );
    g= nx;
  } // while

  DEBUG_Exotic_DB  ( mCB, MyDB,DGC, "(eof)" );
} // DispGlobContext


// mCB->gContext will not be touched here
void DeleteGlobContext( CContext &globContext, DB_Callback mCB, bool emptyTextOnly )
{
  DispGlobContext( globContext, mCB );
  DEBUG_Exotic_DB( mCB, "","DeleteGlobContext", "" );

  GlobContext*    ls= NULL;
  GlobContext*    gs= (GlobContext*)globContext;
  GlobContext* g= gs;
  while       (g!=NULL) {
    GlobContext*  nx= g->next;

    bool remCond=  g->ref==NULL && (!emptyTextOnly || strcmp( g->refName,"" )==0);
    if  (remCond) {
      if  (gs==g)  gs      = nx; // go forward if first element removed
      else         ls->next= nx;

      delete g;
      g= ls;
    } // if

    ls= g;
    g = nx;
  } // while

                   globContext= (CContext)gs;
  DispGlobContext( globContext, mCB );
} // DeleteGlobContext



/* ---- "DB_Api_Session" implementation ------------------------------------ */
TDB_Api_Session::TDB_Api_Session()
{
  sCreated= false;
  sContext= 0;

  appPointer        modu= NULL;
  connect_no_dbapi( modu, sNo_dbapi ); // the empty connector is default as well
  dm=                    &sNo_dbapi;   // and assign it

  sPwMode= Password_Mode_Undefined;
} // constructor



/* Create a context for a new session */
TSyError TDB_Api_Session::CreateContext( cAppCharP sessionName, TDB_Api_Config &config )
{
  if (sCreated) return DB_Forbidden;
  sSessionName= sessionName; // make a local copy

                                dm= &config.m; // assign reference to the methods
  CreateS_Func p= (CreateS_Func)dm->se.Session_CreateContext;

  DB_Callback sCB= &fCB.Callback;
  CContext* sc=  &sCB->sContext; // store it at a temporary var
           *sc= 0;               // set it to check later, if changed

  string vers= VersionStr( config.fSDKversion );
  if  (config.fMODversion!=config.fSDKversion) vers+= " / " + VersionStr( config.fMODversion );

  DEBUG_INT( sCB,MyDB,Se_CC, "desc='%s', vers=%s", config.fDesc.c_str(),
                                                           vers.c_str() );

                                                     sCB->cContext= config.mContext; // inherit info
                     sContext= 0;                    sCB->mContext= config.fCB.Callback.mContext;
  TSyError err= p(  &sContext, sSessionName.c_str(), sCB );
  if     (!err) {
    sCreated= true;
    if (*sc==0) *sc= sContext; // assign for datastores, but only if not assigned in plug-in module
  } // if

  return LOCERR_OK; // workaround to avoid problems w/o a session
  return err;
} // CreateContext



// ---------------------------------------------------------------------
void TDB_Api_Session::DisposeStr( TDB_Api_Str &s )
// Session str disposer
{
  fSList.remove( &s ); // remove it from the list first to avoid double free

  s.AssignStr( sContext, (DisposeProc)dm->se.Session_DisposeObj );
  s.DisposeStr();
} // DisposeStr


static void Local_Session_DisposeStr( CContext sObj, void* memory )
/* Local wrapper, because it must be DisposeProc type and can't be C++ method */
{
  TDB_Api_Session* s= (TDB_Api_Session*)sObj;
  TDB_Api_Str*     u= (TDB_Api_Str*)memory;
  s->DisposeStr ( *u );
} // Local_Session_DisposeStr


void TDB_Api_Session::AssignStr( TDB_Api_Str &s )
{
  if (s.fStr==NULL) return;

  s.AssignStr( (CContext)this, Local_Session_DisposeStr, true );
  fSList.push_back( &s ); // add the element to the list
} // AssignStr



// --------------------------------------------------------------------------------
TSyError TDB_Api_Session::CheckDevice( cAppCharP deviceID, TDB_Api_Str &sDevKey,
                                                           TDB_Api_Str &nonce )
{
  typedef TSyError (*ChkDevFunc)( CContext  sContext,
                                 cAppCharP  deviceID,
                                  appCharP *sDevKey,
                                  appCharP *nonce );

                             sDevKey.DisposeStr();
                               nonce.DisposeStr();
  ChkDevFunc    p= (ChkDevFunc)dm->se.dvAdmin.Session_CheckDevice;
  TSyError err= p( sContext, deviceID, &sDevKey.fStr, &nonce.fStr );
  if     (!err) { AssignStr( sDevKey );
                  AssignStr( nonce   ); }
  return   err;
} // CheckDevice



TSyError TDB_Api_Session::GetNonce( TDB_Api_Str &nonce )
{
  typedef TSyError (*GetNcFunc)( CContext  sContext,
                                 appCharP *nonce );

                              nonce.DisposeStr();
  GetNcFunc     p= (GetNcFunc)dm->se.dvAdmin.Session_GetNonce;
  TSyError err= p( sContext, &nonce.fStr );
  if     (!err)    AssignStr( nonce );
  return   err;
} // GetNonce



TSyError TDB_Api_Session::SaveNonce( cAppCharP nonce )
{
  SvInfo_Func   p= (SvInfo_Func)dm->se.dvAdmin.Session_SaveNonce;
  TSyError err= p( sContext, nonce );
  return   err;
} // SaveNonce



TSyError TDB_Api_Session::SaveDeviceInfo( cAppCharP aDeviceInfo )
{
  SvInfo_Func   p= (SvInfo_Func)dm->se.dvAdmin.Session_SaveDeviceInfo;
  TSyError err= p( sContext, aDeviceInfo );
  return   err;
} // SaveDeviceInfo


TSyError TDB_Api_Session::GetDBTime( TDB_Api_Str &currentDBTime )
{
  typedef TSyError (*DBTimeFunc)( CContext  sContext,
                                  appCharP *currentDBTime );

                               currentDBTime.DisposeStr();
  DBTimeFunc    p= (DBTimeFunc)dm->se.dvTime.Session_GetDBTime;
  TSyError err= p( sContext,  &currentDBTime.fStr );
  if     (!err)     AssignStr( currentDBTime );
  return   err;
} // GetDBTime


#ifdef SYSYNC_ENGINE
// No 'lineartime_t' available for standalone
TSyError TDB_Api_Session::GetDBTime( lineartime_t &currentDBTime, GZones* g )
{
  timecontext_t                                                       tctx;
  TDB_Api_Str               s;
  TSyError  err= GetDBTime( s );                       currentDBTime= 0;
  if      (!err && !ISO8601StrToTimestamp ( s.c_str(), currentDBTime, tctx )) return DB_Error;

  TzConvertTimestamp( currentDBTime, tctx, TCTX_UTC, g, TCTX_SYSTEM );
  return    err;
} // GetDBTime
#endif



/* ---- Login Handling ---------------------------------------- */
sInt32 TDB_Api_Session::PasswordMode()
{
  if            (sContext==0) return Password_Mode_Undefined;
  PwMode_Func p= (PwMode_Func)dm->se.seAuth.Session_PasswordMode;
  sPwMode=    p( sContext );

  /* calculate it always
  if (sPwMode==Password_Mode_Undefined) {
    ...
  } // if
  */

  return sPwMode;
} // PasswordMode



/*! XXX_IN modes */
TSyError TDB_Api_Session::Login( cAppCharP sUsername,
                                 cAppCharP sPassword, TDB_Api_Str &sUsrKey )
{
  switch (PasswordMode()) {
    case  Password_ClrText_IN   :
    case  Password_MD5_Nonce_IN : break;
    default                     : return DB_Forbidden; // !sCreated is covered here
  } // switch

                             sUsrKey.DisposeStr();
  Login_Func    p= (Login_Func)dm->se.seAuth.Session_Login;
  TSyError err= p( sContext, sUsername, (char**)&sPassword, &sUsrKey.fStr );
  if     (!err)   AssignStr( sUsrKey );
  return   err;
} // Login


/*! XXX_OUT modes */
TSyError TDB_Api_Session::Login( cAppCharP    sUsername,
                                 TDB_Api_Str &sPassword, TDB_Api_Str &sUsrKey )
{
  switch (PasswordMode()) {
    case  Password_ClrText_OUT :
    case  Password_MD5_OUT     : break;
    default                    : return DB_Forbidden; // !sCreated is covered here
  } // switch

                             sPassword.DisposeStr();
                               sUsrKey.DisposeStr();
  Login_Func    p= (Login_Func)dm->se.seAuth.Session_Login;
  TSyError err= p( sContext, sUsername, &sPassword.fStr, &sUsrKey.fStr );
  if     (!err) { AssignStr( sPassword );
                  AssignStr(   sUsrKey ); }
  return   err;
} // Login (overloaded)



TSyError TDB_Api_Session::Logout()
{
  if             (sContext==0) return LOCERR_OK;
  Context_Func p= (Context_Func)dm->se.seAuth.Session_Logout;
  return       p( sContext );
} // Logout



/* ---- General session routines ------------------------------- */
void TDB_Api_Session::AssignChanged( string &a, TDB_Api_Str &u ) {
  if (a.c_str()!=u.fStr) { a= u.fStr; AssignStr( u ); } // assign for destruction afterwards
} // AssignChanged


TSyError TDB_Api_Session::AdaptItem( string &sItemData1,
                                     string &sItemData2,
                                     string &sLocalVars, uInt32 sIdentifier )
{
  TDB_Api_Str updItemData1( sItemData1 );
  TDB_Api_Str updItemData2( sItemData2 );
  TDB_Api_Str updLocalVars( sLocalVars );

  Adapt_Func    p= (Adapt_Func)dm->se.seAdapt.Session_AdaptItem;
  TSyError err= p( sContext, &updItemData1.fStr,
                             &updItemData2.fStr,
                             &updLocalVars.fStr, sIdentifier );

  AssignChanged( sItemData1,  updItemData1 );
  AssignChanged( sItemData2,  updItemData2 );
  AssignChanged( sLocalVars,  updLocalVars );

  return err;
} // AdaptItem


void TDB_Api_Session::ThreadMayChangeNow()
{
  VoidProc p= (VoidProc)dm->se.Session_ThreadMayChangeNow;
           p( sContext );
} // ThreadMayChangeNow



void TDB_Api_Session::DispItems( bool allFields, cAppCharP specificItem )
{
  DispProc p= (DispProc)dm->se.Session_DispItems;
           p( sContext, allFields,specificItem );
} // DispItems



/* ---- Delete the session context ---------------------------- */
TSyError TDB_Api_Session::DeleteContext()
{
  if (!sCreated) return DB_Forbidden;

  // remove all still allocated elements before removing the content
  while (!fSList.empty()) DisposeStr( *fSList.front() );

  Context_Func  p= (Context_Func)dm->se.Session_DeleteContext;
  TSyError err= p( sContext );
  if     (!err) sCreated= false;
  return   err;
} // DeleteContext



/* ---- "DB_Api" implementation ------------------------------------------- */
TDB_Api::TDB_Api()
{
  fCreated= false;

  appPointer        modu= NULL;
  connect_no_dbapi( modu, fNo_dbapi ); // the empty connector is default as well
  dm=                    &fNo_dbapi;   // and assign it
} // constructor


TDB_Api::~TDB_Api()
{
  DeleteContext();
} // destructor



// --- open section -------------------------------------
TSyError TDB_Api::CreateContext( cAppCharP aContextName, bool asAdmin, TDB_Api_Config*  config,
                                 cAppCharP sDevKey,
                                 cAppCharP sUsrKey,                    TDB_Api_Session* session )
{
  if (fCreated || !config ||
                  !config->fConnected) return DB_Forbidden;

  string cNam= aContextName;
  if    (cNam.empty()) return DB_NotFound; // empty <aContextName> is not allowed

  fConfig= config; // make a local copy

  /* inherit the context information of module and session */
                 fCB.Callback.cContext=  config->mContext;
                 fCB.Callback.mContext=  config->fCB.Callback.mContext;
  if (session) { fCB.Callback.sContext= session->fCB.Callback.sContext; }
                 fDevKey              = sDevKey;
                 fUsrKey              = sUsrKey;

  if (config->fADMIN_Info && asAdmin) cNam+= ADMIN_Ident;

                    fContext= 0;     dm= &config->m;
  CreateD_Func  p=     (CreateD_Func)dm->ds.CreateContext;
  TSyError err= p( &fContext, cNam.c_str(), &fCB.Callback, fDevKey.c_str(),
                                                           fUsrKey.c_str() );
  if     (!err) fCreated= true;
  return   err;
} // CreateContext



TSyError TDB_Api::RunContext( cAppCharP    aContextName, bool asAdmin,
                              SequenceProc sequence,
                              string      &token,        TDB_Api_Config*  config,
                              cAppCharP    sDevKey,
                              cAppCharP    sUsrKey,      TDB_Api_Session* session )
{
  TSyError    err= CreateContext( aContextName, asAdmin, config,
                                  sDevKey,sUsrKey,       session ); if (err) return err;
  TSyError    dErr;
  TDB_Api_Str newToken;

  err= StartDataRead( token.c_str() );
  // no sequence is allowed as well
  if (!err && sequence!=NULL) {
    err =     sequence( *this );
    dErr= EndDataWrite( !err, newToken ); if (!err) err= dErr;
    token=                    newToken.c_str();
    newToken.DisposeStr();
  } // if

  dErr= DeleteContext(); if (!err) err= dErr;
  return err;
} // RunContext



// ---------------------------------------------------------------------
// returns the nth config data field, which is supported (and activated)
// result= 0: no filter supported.
uInt32 TDB_Api::ContextSupport( cAppCharP aContextRules )
{
  Text_Func p= (Text_Func)dm->ds.dsg.ContextSupport;
  return    p( fContext,aContextRules );
} // ContextSupport


// returns the nth filter rule, which is supported (and activated)
// result= 0: no filter supported.
uInt32 TDB_Api::FilterSupport ( cAppCharP aFilterRules  )
{
  Text_Func p= (Text_Func)dm->ds.dsg.FilterSupport;
  return    p( fContext,aFilterRules  );
} // FilterSupport



// ---------------------------------------------------------------------
// Str disposer
void TDB_Api::DisposeStr( TDB_Api_Str &s )
{
  fSList.remove( &s ); // remove it from the list first to avoid double free

  s.AssignStr( fContext, (DisposeProc)dm->ds.DisposeObj );
  s.DisposeStr();
} // DisposeStr



// Local wrapper, because it must be DisposeProc type and can't be C++ method
static void Local_DisposeStr( CContext aObj, void* memory )
{
  TDB_Api*     a= (TDB_Api*)aObj;
  TDB_Api_Str* u= (TDB_Api_Str*)memory; a->DisposeStr( *u );
} // Local_DisposeStr



void TDB_Api::AssignStr( TDB_Api_Str &s )
{
  if (s.fStr==NULL) return;

  s.AssignStr( (CContext)this, Local_DisposeStr, true );
  fSList.push_back( &s ); // add the element to the list
} // AssignStr



// ---------------------------------------------------------------------
// Blk disposer
void TDB_Api::DisposeBlk( TDB_Api_Blk &b )
{
  fBList.remove( &b ); // remove it from the list first to avoid double free

  b.AssignBlk( fContext, (DisposeProc)dm->ds.DisposeObj );
  b.DisposeBlk();
} // DisposeBlk



// Local wrapper, because it must be DisposeProc type and can't be C++ method
static void Local_DisposeBlk( CContext aObj, void* memory )
{
  TDB_Api*     a= (TDB_Api*)aObj;
  TDB_Api_Blk* u= (TDB_Api_Blk*)memory; a->DisposeBlk( *u );
} // Local_DisposeBlk



void TDB_Api::AssignBlk( TDB_Api_Blk &b )
{
  if (b.fPtr==NULL) return;

  b.AssignBlk( (CContext)this, Local_DisposeBlk, true );
  fBList.push_back( &b ); // add the element to the list
} // AssignBlk



// ---------------------------------------------------------------------
void TDB_Api::GetItemID( TDB_Api_ItemID &aID, TDB_Api_Str &aItemID )
{
  aItemID=        aID.item;
  fSList.remove( &aID.item );
                  aID.item.fStr= NULL; // avoid double delete
  AssignStr ( aItemID );

  aID.parent.DisposeStr(); // not used
} // GetItemID



// --- admin section -------------------------------
// This function gets the stored information about the record with the four paramters:
// <sDevKey>, <sUsrKey> (taken from the session context)
// <aLocDB>,  <aRemDB>.
TSyError TDB_Api::LoadAdminData( cAppCharP aLocDB,
                                 cAppCharP aRemDB, TDB_Api_Str &adminData )
{
                                             adminData.DisposeStr();
  LoadAdm_SFunc p= (LoadAdm_SFunc)dm->ds.dsAdm.str.LoadAdminData;
  TSyError err= p( fContext, aLocDB,aRemDB, &adminData.fStr );
  if     (!err)                   AssignStr( adminData );
  return   err;
} // LoadAdminData

TSyError TDB_Api::LoadAdminDataAsKey( cAppCharP aLocDB,
                                      cAppCharP aRemDB, KeyH aAdminKey )
{
  LoadAdm_KFunc p= (LoadAdm_KFunc)dm->ds.dsAdm.key.LoadAdminDataAsKey;
  TSyError err= p( fContext, aLocDB,aRemDB, aAdminKey );
  return   err;
} // LoadAdminDataAsKey


//! This functions stores the new <adminData> for this context
TSyError TDB_Api::SaveAdminData( cAppCharP adminData )
{
  SaveAdm_SFunc p= (SaveAdm_SFunc)dm->ds.dsAdm.str.SaveAdminData;
  return        p( fContext, adminData );
} // SaveAdminData

TSyError TDB_Api::SaveAdminData_AsKey( KeyH adminKey )
{
  SaveAdm_KFunc p= (SaveAdm_KFunc)dm->ds.dsAdm.key.SaveAdminDataAsKey;
  return        p( fContext, adminKey );
} // SaveAdminDataAsKey



// --- Map table handling ----------------------------
//! Get a map item of this context
bool TDB_Api::ReadNextMapItem( TDB_Api_MapID &mID, bool aFirst )
{
  MapID_Struct u;
  mID.localID.DisposeStr();
  mID.remoteID.DisposeStr();

  RdNMap_Func p= (RdNMap_Func)dm->ds.dsAdm.map.ReadNextMapItem;
  bool   ok=  p( fContext, &u, aFirst );
  if    (ok) {
    mID.localID.fStr = u.localID;  AssignStr( mID.localID  );
    mID.remoteID.fStr= u.remoteID; AssignStr( mID.remoteID );
    mID.flags        = u.flags;
    mID.ident        = u.ident;
  } // if

  return ok;
} // ReadNextMapItem



// Insert a map item of this context
TSyError TDB_Api::InsertMapItem( MapID mID )
{
  InsMap_Func p= (InsMap_Func)dm->ds.dsAdm.map.InsertMapItem;
  return      p( fContext, mID );
} // InsertMapItem



// Update a map item of this context
TSyError TDB_Api::UpdateMapItem( MapID mID )
{
  UpdMap_Func p= (UpdMap_Func)dm->ds.dsAdm.map.UpdateMapItem;
  return      p( fContext, mID );
} // UpdateMapItem



// Delete a map item of this context
TSyError TDB_Api::DeleteMapItem( MapID mID )
{
  DelMap_Func p= (DelMap_Func)dm->ds.dsAdm.map.DeleteMapItem;
  return      p( fContext, mID );
} // DeleteMapItem



// --- general -----------------------------------------
void TDB_Api::ThreadMayChangeNow()
{
  VoidProc p= (VoidProc)dm->ds.dsg.ThreadMayChangeNow;
           p( fContext );
} // ThreadMayChangeNow



//! This functions gets the <logData> for this datastore
void TDB_Api::WriteLogData( cAppCharP logData )
{
  SvInfo_Func p= (SvInfo_Func)dm->ds.dsg.WriteLogData;
              p( fContext, logData );
} // WriteLogData



void TDB_Api::DispItems( bool allFields, cAppCharP specificItem )
{
  DispProc p= (DispProc)dm->ds.dsg.DispItems;
           p( fContext, allFields, specificItem );
} // DispItems


void TDB_Api::AssignChanged( string &a, TDB_Api_Str &u ) {
  if (a.c_str()!=u.fStr) { a= u.fStr; AssignStr( u ); } // assign for destruction afterwards
} // AssignChanged


TSyError TDB_Api::AdaptItem( string &sItemData1,
                             string &sItemData2,
                             string &sLocalVars, uInt32 sIdentifier )
{
  TDB_Api_Str updItemData1( sItemData1 );
  TDB_Api_Str updItemData2( sItemData2 );
  TDB_Api_Str updLocalVars( sLocalVars );

  Adapt_Func    p= (Adapt_Func)dm->ds.dsAdapt.AdaptItem;
  TSyError err= p( fContext, &updItemData1.fStr,
                             &updItemData2.fStr,
                             &updLocalVars.fStr, sIdentifier );

  AssignChanged( sItemData1,  updItemData1 );
  AssignChanged( sItemData2,  updItemData2 );
  AssignChanged( sLocalVars,  updLocalVars );

  return err;
} // AdaptItem



// --- read section ------------------------------------
TSyError TDB_Api::StartDataRead( cAppCharP lastToken, cAppCharP resumeToken )
{
  typedef TSyError (*OLD_SDR_Func)( CContext aContext, cAppCharP lastToken );

  if (!fCreated) return DB_Fatal; // <fConfig> is not defined, if not created

  // new param supported for Plugin Version >= 1.0.6.X
  if (fConfig->Supported( VP_ResumeToken )) {
    SDR_Func     p=     (SDR_Func)dm->ds.dsData.StartDataRead;
    return       p( fContext, lastToken,resumeToken );
  }
  else {
    OLD_SDR_Func p= (OLD_SDR_Func)dm->ds.dsData.StartDataRead;
    return       p( fContext, lastToken );
  } // if
} // StartDataRead


// overloaded for using DB:Api_ItemID class
TSyError TDB_Api::ReadNextItem( TDB_Api_ItemID &aID,
                                TDB_Api_Str &aItemData, int &aStatus, bool aFirst )
{
  ItemID_Struct u;
  sInt32 lStatus;

  aID.item.DisposeStr();
  aID.parent.DisposeStr();
  aItemData.DisposeStr();

  u.parent= NULL; // works correctly, even if not implemented on user side
  RdNItemSFunc  p= (RdNItemSFunc)dm->ds.dsData.str.ReadNextItem;
  TSyError err= p( fContext, &u, &aItemData.fStr, &lStatus, aFirst ); aStatus= (int)lStatus;
  if      (err || aStatus==ReadNextItem_EOF) return err;

  aID.item.fStr  = u.item;   AssignStr( aID.item   );
  aID.parent.fStr= u.parent; AssignStr( aID.parent );
                             AssignStr( aItemData  );

  // additional info for test
//DEBUG_Exotic_DB( fCB, aID.item.c_str(), "ReadNextItem", aItemData.c_str() );

  return LOCERR_OK;
} // ReadNextItem


// overloaded for mode w/o aParentID
TSyError TDB_Api::ReadNextItem( TDB_Api_Str &aItemID,
                                TDB_Api_Str &aItemData,
                                        int &aStatus, bool aFirst )
{
  TDB_Api_ItemID aID;

  TSyError err= ReadNextItem( aID, aItemData, aStatus, aFirst );
  if      (err || aStatus==ReadNextItem_EOF) return err;
  GetItemID                 ( aID, aItemID );
  return LOCERR_OK;
} // ReadNextItem


TSyError TDB_Api::ReadNextItemAsKey( TDB_Api_ItemID &aID,
                                               KeyH aItemKey,
                                                int &aStatus, bool aFirst )
{
  ItemID_Struct u;
  sInt32 lStatus;

  aID.item.DisposeStr();
  aID.parent.DisposeStr();

  u.parent= NULL; // works correctly, even if not implemented on user side
  RdNItemKFunc  p= (RdNItemKFunc)dm->ds.dsData.key.ReadNextItemAsKey;
  TSyError err= p( fContext, &u, aItemKey, &lStatus, aFirst ); aStatus= (int) lStatus;
  if      (err || aStatus==ReadNextItem_EOF) return err;

  aID.item.fStr  = u.item;   AssignStr( aID.item   );
  aID.parent.fStr= u.parent; AssignStr( aID.parent );

  return LOCERR_OK;
} // ReadNextItemAsKey



// overloaded for using ItemID_Struct
TSyError TDB_Api::ReadItem( ItemID_Struct aID, TDB_Api_Str &aItemData )
{
                                     aItemData.DisposeStr();
  Rd_ItemSFunc  p= (Rd_ItemSFunc)dm->ds.dsData.str.ReadItem;
  TSyError err= p( fContext, &aID,  &aItemData.fStr );
  if     (!err) AssignStr          ( aItemData );
  return   err;
} // ReadItem


// overloaded for <aItemID>,<aParentID> call
TSyError TDB_Api::ReadItem( cAppCharP   aItemID,
                            cAppCharP aParentID, TDB_Api_Str &aItemData )
{
  ItemID_Struct a; a.item  = (char*)  aItemID;
                   a.parent= (char*)aParentID;
  return ReadItem( a, aItemData );
} // ReadItem


// overloaded for mode w/o aParentID
TSyError TDB_Api::ReadItem( cAppCharP aItemID, TDB_Api_Str &aItemData ) {
  return ReadItem( aItemID,"", aItemData );
} // ReadItem


TSyError TDB_Api::ReadItemAsKey( ItemID_Struct aID, KeyH aItemKey )
{
  Rd_ItemKFunc  p= (Rd_ItemKFunc)dm->ds.dsData.key.ReadItemAsKey;
  TSyError err= p( fContext, &aID, aItemKey );
  return   err;
} // ReadItemAsKey



// overloaded for using ItemID_Struct
TSyError TDB_Api::ReadBlob( ItemID_Struct aID,
                            cAppCharP aBlobID, memSize  blkSize,
                            TDB_Api_Blk &aBlk, memSize &totSize, bool aFirst, bool &aLast )
{
  typedef TSyError (*Rd_Blob_Func)( CContext  aContext,
                                     cItemID  aID,
                                   cAppCharP  aBlobID,
                                  appPointer *blkPtr,
                                     memSize *blkSize,
                                     memSize *totSize,
                                        bool  aFirst,
                                        bool *aLast );
  aBlk.DisposeBlk();
  aBlk.fSize= blkSize;

  Rd_Blob_Func  p= (Rd_Blob_Func)dm->ds.dsBlob.ReadBlob;
  TSyError err= p( fContext, &aID,aBlobID,
                             &aBlk.fPtr,&aBlk.fSize, &totSize, aFirst,&aLast );
  if     (!err) aBlk.AssignBlk( fContext, (DisposeProc)dm->ds.DisposeObj );
  return   err;
} // ReadBlob


// overloaded for <aItemID>,<aParentID> call
TSyError TDB_Api::ReadBlob( cAppCharP   aItemID,
                            cAppCharP aParentID,
                            cAppCharP   aBlobID, memSize  blkSize,
                            TDB_Api_Blk   &aBlk, memSize &totSize, bool aFirst, bool &aLast )
{
  ItemID_Struct a; a.item  =  (char*)aItemID;
                   a.parent=  (char*)aParentID;
  return ReadBlob( a, aBlobID, blkSize,aBlk,totSize, aFirst,aLast );
} // ReadBlob


// overloaded for mode w/o aParentID
TSyError TDB_Api::ReadBlob( cAppCharP aItemID,
                            cAppCharP aBlobID, memSize  blkSize,
                            TDB_Api_Blk &aBlk, memSize &totSize, bool aFirst, bool &aLast ) {
  return ReadBlob( aItemID,"", aBlobID, blkSize,aBlk,totSize, aFirst,aLast );
} // ReadBlob



TSyError TDB_Api::EndDataRead()
{
  EDR_Func p= (EDR_Func)dm->ds.dsData.EndDataRead;
  return   p( fContext );
} // EndDataRead



// --- write section ----------------------------------------------------------------------
TSyError TDB_Api::StartDataWrite()
{
  SDW_Func p= (SDW_Func)dm->ds.dsData.StartDataWrite;
  return   p( fContext );
} // StartDataWrite



// Assign <aID> to <newID>, ignore new parent
void TDB_Api::Assign_ItemID( TDB_Api_ItemID &newID, ItemID_Struct &a, cAppCharP parentID )
{
               newID.item.fStr= a.item;
  AssignStr  ( newID.item );

  if (a.parent!=parentID) {                 // remove it, if explicitely allocated
               newID.parent.fStr= a.parent; // parent can't be changed !!
    AssignStr( newID.parent );
               newID.parent.DisposeStr();
  } // if

  newID.parent.LocalAlloc( fContext, parentID );
} // Assign_ItemID


// -------------------------------------------------------------------------
// overloaded for <aParentID> call
TSyError TDB_Api::InsertItem( cAppCharP aItemData, cAppCharP    parentID,
                                                   TDB_Api_ItemID &newID )
{
  newID.item.DisposeStr();
  newID.parent.DisposeStr();

  ItemID_Struct a;
  a.item  = NULL;
  a.parent= (char*)parentID; // works correctly, even if not implemented on user side

  InsItemSFunc  p= (InsItemSFunc)dm->ds.dsData.str.InsertItem;
  TSyError err= p( fContext, aItemData, &a );
  if     (!err || err==DB_DataMerged || err==DB_DataReplaced || err==DB_Conflict) {
    Assign_ItemID( newID, a, parentID );
  } // if

  return err;
} // InsertItem


// overloaded for mode w/o aParentID
TSyError TDB_Api::InsertItem( cAppCharP aItemData, TDB_Api_Str &newItemID )
{
  TDB_Api_ItemID                          nID;
  TSyError err= InsertItem( aItemData, "",nID );
  if     (!err || err==DB_DataMerged || err==DB_DataReplaced || err==DB_Conflict) {
    GetItemID( nID, newItemID );
  } // if
  
  return   err;
} // InsertItem


TSyError TDB_Api::InsertItemAsKey( KeyH aItemKey, cAppCharP parentID,
                                             TDB_Api_ItemID   &newID )
{
  newID.item.DisposeStr();
  newID.parent.DisposeStr();

  ItemID_Struct a;
  a.item  = NULL;
  a.parent= (char*)parentID; // works correctly, even if not implemented on user side

  InsItemKFunc  p= (InsItemKFunc)dm->ds.dsData.key.InsertItemAsKey;
  TSyError err= p( fContext, aItemKey, &a );
  if     (!err || err==DB_DataMerged || err==DB_DataReplaced || err==DB_Conflict) {
    Assign_ItemID( newID, a, parentID );
  } // if

  return err;
} // InsertItemAsKey



// -------------------------------------------------------------------------
// overloaded for using ItemID_Struct
TSyError TDB_Api::UpdateItem( cAppCharP aItemData, ItemID_Struct     aID,
                                                   TDB_Api_ItemID &updID )
{
  updID.item.DisposeStr();
  updID.parent.DisposeStr();

  ItemID_Struct u;
  u.item  = NULL;
  u.parent= (char*)aID.parent; // works correctly, even if not implemented on user side

  UpdItemSFunc  p= (UpdItemSFunc)dm->ds.dsData.str.UpdateItem;
  TSyError err= p( fContext, aItemData, &aID,&u );
  if     (!err) {
    Assign_ItemID( updID, u, aID.parent );
  } // if

  return err;
} // UpdateItem


// overloaded for <aItemID>,<aParentID> call
TSyError TDB_Api::UpdateItem( cAppCharP aItemData,
                              cAppCharP aItemID,
                              cAppCharP aParentID, TDB_Api_ItemID &updID )
{
  ItemID_Struct a;              a.item  = (char*)aItemID;
                                a.parent= (char*)aParentID;
  return UpdateItem( aItemData, a,updID );
} // UpdateItem


// overloaded for mode w/o aParentID
TSyError TDB_Api::UpdateItem( cAppCharP aItemData, cAppCharP      aItemID,
                                                   TDB_Api_Str &updItemID )
{
  TDB_Api_ItemID                                   uID;
  TSyError err= UpdateItem( aItemData, aItemID,"", uID );
  if     (!err)                         GetItemID( uID, updItemID );
  return   err;
} // UpdateItem


TSyError TDB_Api::UpdateItem( cAppCharP  aItemData, TDB_Api_ItemID   &aID,
                                                    TDB_Api_ItemID &updID )
{
  return UpdateItem( aItemData, (char*)aID.item.c_str(),
                                (char*)aID.parent.c_str(), updID );
} // UpdateItem



TSyError TDB_Api::UpdateItemAsKey( KeyH aItemKey, ItemID_Struct     aID,
                                                  TDB_Api_ItemID &updID )
{
  updID.item.DisposeStr();
  updID.parent.DisposeStr();

  ItemID_Struct u;
  u.item  = NULL;
  u.parent= (char*)aID.parent; // works correctly, even if not implemented on user side

  UpdItemKFunc  p= (UpdItemKFunc)dm->ds.dsData.key.UpdateItemAsKey;
  TSyError err= p( fContext, aItemKey, &aID,&u );
  if     (!err) {
    Assign_ItemID( updID, u, aID.parent );
  } // if

  return err;
} // UpdateItemAsKey


TSyError TDB_Api::UpdateItemAsKey( KeyH aItemKey, TDB_Api_ItemID   &aID,
                                                  TDB_Api_ItemID &updID )
{
  ItemID_Struct a;                  a.item  = (char*)aID.item.c_str();
                                    a.parent= (char*)aID.parent.c_str();
  return UpdateItemAsKey( aItemKey, a, updID );
} // UpdateItemAsKey



// -------------------------------------------------------------------------
// overloaded for using ItemID_Struct
TSyError TDB_Api::MoveItem( ItemID_Struct aID, cAppCharP newParID )
{
  MovItem_Func p= (MovItem_Func)dm->ds.dsData.ind.MoveItem;
  return       p( fContext, &aID,newParID );
} // MoveItem


// overloaded for <aItemID>,<aParentID> call
TSyError TDB_Api::MoveItem(  cAppCharP   aItemID,
                             cAppCharP aParentID, cAppCharP newParID )
{
  ItemID_Struct a; a.item  = (appCharP)  aItemID;
                   a.parent= (appCharP)aParentID;
  return MoveItem( a, newParID );
} // MoveItem



// -------------------------------------------------------------------------
// overloaded for using ItemID_Struct
TSyError TDB_Api::DeleteItem( ItemID_Struct aID )
{
  DelItem_Func p= (DelItem_Func)dm->ds.dsData.ind.DeleteItem;
  return       p( fContext, &aID );
} // DeleteItem


// overloaded for <aItemID>,<aParentID> call
TSyError TDB_Api::DeleteItem( cAppCharP aItemID, cAppCharP aParentID )
{
  ItemID_Struct a;   a.item  = (char*)aItemID;
                     a.parent= (char*)aParentID;
  return DeleteItem( a );
} // DeleteItem


// overloaded for using TDB_Api_ItemID
// <aID> object will not be removed here
TSyError TDB_Api::DeleteItem( TDB_Api_ItemID &aID )
{
  return DeleteItem( (char*)aID.item.c_str(),
                     (char*)aID.parent.c_str() );
} // DeleteItem



// -------------------------------------------------------------------------
TSyError TDB_Api::FinalizeLocalID( ItemID_Struct      aID,
                                   TDB_Api_ItemID  &updID )
{
  updID.item.DisposeStr();
  updID.parent.DisposeStr();

  if (!fConfig->Supported( VP_FLI_DSS )) return LOCERR_NOTIMP;

  ItemID_Struct u;
  u.item  = NULL;
  u.parent= (char*)aID.parent; // works correctly, even if not implemented on user side

  FLI_Func      p= (FLI_Func)dm->ds.dsData.ind.FinalizeLocalID;
  TSyError err= p( fContext, &aID,&u );
  if     (!err) {
    Assign_ItemID( updID, u, aID.parent );
  } // if

  return err;
} // FinalizeLocalID


TSyError TDB_Api::FinalizeLocalID( TDB_Api_ItemID     &aID,
                                   TDB_Api_ItemID   &updID ) {
  return FinalizeLocalID( (char*)aID.item.c_str(),
                          (char*)aID.parent.c_str(), updID );
} // FinalizeLocalID


TSyError TDB_Api::FinalizeLocalID( cAppCharP      aItemID,
                                   TDB_Api_Str &updItemID )
{
  TDB_Api_ItemID                             uID;
  TSyError err= FinalizeLocalID( aItemID,"", uID );
  if     (!err)                   GetItemID( uID, updItemID );
  return   err;
} // FinalizeLocalID


TSyError TDB_Api::FinalizeLocalID( cAppCharP     aItemID,
                                   cAppCharP   aParentID,
                                   TDB_Api_ItemID &updID )
{
  ItemID_Struct a;        a.item  = (char*)aItemID;
                          a.parent= (char*)aParentID;
  return FinalizeLocalID( a, updID );
} // FinalizeLocalID



TSyError TDB_Api::DeleteSyncSet()
{
  if (!fConfig->Supported( VP_FLI_DSS )) return LOCERR_NOTIMP;

  DelSS_Func p= (DelSS_Func)dm->ds.dsData.ind.DeleteSyncSet;
  return     p( fContext );
} // DeleteSyncSet



// -------------------------------------------------------------------------
// overloaded for using ItemID_Struct
TSyError TDB_Api::WriteBlob( ItemID_Struct  aID, cAppCharP aBlobID,
                             appPointer  blkPtr,   memSize blkSize,
                                                   memSize totSize, bool aFirst, bool aLast )
{
  typedef TSyError (*Wr_BlobFunc)( CContext aContext,
                                    cItemID aID,
                                  cAppCharP aBlobID,
                                 appPointer blkPtr,
                                    memSize blkSize,
                                    memSize totSize,
                                       bool aFirst,
                                       bool aLast );

  Wr_BlobFunc p= (Wr_BlobFunc)dm->ds.dsBlob.WriteBlob;
  return      p( fContext, &aID,aBlobID, blkPtr,blkSize,totSize, aFirst,aLast );
} // WriteBlob


// overloaded for mode with <aItemID> and <aParentID>
TSyError TDB_Api::WriteBlob( cAppCharP   aItemID,
                             cAppCharP aParentID, cAppCharP aBlobID,
                             appPointer   blkPtr,   memSize blkSize,
                                                    memSize totSize, bool aFirst, bool aLast )
{
  ItemID_Struct a;   a.item  = (char*)aItemID;
                     a.parent= (char*)aParentID;
  return  WriteBlob( a,aBlobID, blkPtr,blkSize,totSize, aFirst,aLast );
} // WriteBlob


// overloaded for mode w/o <aParentID>
TSyError TDB_Api::WriteBlob( cAppCharP   aItemID, cAppCharP aBlobID,
                             appPointer   blkPtr,   memSize blkSize,
                                                    memSize totSize, bool aFirst, bool aLast )
{
  return WriteBlob( aItemID,"", aBlobID, blkPtr,blkSize,totSize, aFirst,aLast );
} // WriteBlob


// overloaded for using TDB_Api_ItemID
TSyError TDB_Api::WriteBlob( TDB_Api_ItemID &aID, cAppCharP aBlobID,
                             appPointer   blkPtr,   memSize blkSize,
                                                    memSize totSize, bool aFirst, bool aLast )
{
  return WriteBlob( (char*)aID.item.c_str(),
                    (char*)aID.parent.c_str(), aBlobID, blkPtr,blkSize,totSize, aFirst,aLast );
} // WriteBlob



// -------------------------------------------------------------------------
// overloaded for using ItemID_Struct
TSyError TDB_Api::DeleteBlob( ItemID_Struct aID, cAppCharP aBlobID )
{
  typedef TSyError (*DelBlobFunc)( CContext aContext,
                                    cItemID aID,
                                  cAppCharP aBlobID );

  DelBlobFunc p= (DelBlobFunc)dm->ds.dsBlob.DeleteBlob;
  return      p( fContext, &aID,aBlobID );
} // DeleteBlob


// overloaded for mode with <aItemID> and <aParentID>
TSyError TDB_Api::DeleteBlob( cAppCharP aItemID,
                              cAppCharP aParentID,
                              cAppCharP aBlobID )
{
  ItemID_Struct a;   a.item  = (char*)aItemID;
                     a.parent= (char*)aParentID;
  return DeleteBlob( a,aBlobID );
} // DeleteBlob


// overloaded for mode w/o <aParentID>
TSyError TDB_Api::DeleteBlob( cAppCharP aItemID,
                              cAppCharP aBlobID )
{
  return DeleteBlob( aItemID,"", aBlobID );
} // DeleteBlob


// overloaded for using TDB_Api_ItemID
TSyError TDB_Api::DeleteBlob( TDB_Api_ItemID &aID, cAppCharP aBlobID )
{
  return DeleteBlob( (char*)aID.item.c_str(),
                     (char*)aID.parent.c_str(), aBlobID );
} // DeleteBlob



// -------------------------------------------------------------------------
TSyError TDB_Api::EndDataWrite( bool success, TDB_Api_Str &newToken )
{
                                       newToken.DisposeStr();
  EDW_Func      p= (EDW_Func)dm->ds.dsData.EndDataWrite;
  TSyError err= p( fContext, success, &newToken.fStr );
  if     (!err) {
    AssignStr( newToken );
    if (!success) err= DB_Fatal;
  } // if

  return err;
} // EndDataWrite



// --- close section --------------------------------
TSyError TDB_Api::DeleteContext()
{
  if (!fCreated) return DB_Forbidden;

  // remove all still allocated elements before removing the content
  while (!fSList.empty()) DisposeStr( *fSList.front() );
  while (!fBList.empty()) DisposeBlk( *fBList.front() );

  Context_Func  p= (Context_Func)dm->ds.DeleteContext;
  TSyError err= p( fContext );
  if     (!err) fCreated= false;
  return   err;
} // DeleteContext


} // namespace
/* eof */
