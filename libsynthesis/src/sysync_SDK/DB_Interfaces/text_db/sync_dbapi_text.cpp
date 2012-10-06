/*
 *  File:    sync_dbapi_text.cpp
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  Example DBApi database adapter plugin
 *  Written in C++
 *
 *  Similar behaviour as the former "text_db".
 *    (with extensions for ARRAY and BLOB access)
 *
 *  For newer engine versions, this sample code
 *  is 1:1 used as datastore connector for the
 *  so called demo server/client.
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 *
 *  E X A M P L E    C O D E
 *    (text_db interface)
 *
 */

#include "sync_include.h"       // include general SDK definitions
#include "sync_dbapidef.h"      // include the interface file and utilities
#include "SDK_util.h"           // include SDK utilities
#include "SDK_support.h"        // and some C++ support functionality

#ifndef SYSYNC_ENGINE           // outside the SyncML engine we need additional stuff:
  #include "stringutil.h"       // local implementation for CStr <=> Str conversions
  #include <ctype.h>            // isalnum
#endif

#include "admindata.h"          // TAdminData class
#include "blobs.h"              // TBlob      class
#include "dbitem.h"             // TDBItem    class

namespace SDK_textdb {          // the plugin runs within a namespace
  using sysync::DB_Callback;    // common definitions for both namespaces
  using sysync::ItemID_Struct;
  using sysync::ItemID;
  using sysync::cItemID;
  using sysync::MapID;
  using sysync::KeyH;
  using sysync::TDBItem;
  using sysync::TDBItemField;
  using sysync::TAdminData;
  using sysync::TBlob;
  
  using sysync::LOCERR_OK;
  using sysync::LOCERR_TOOOLD;
  using sysync::LOCERR_NOTIMP;
  using sysync::DB_Forbidden;
  using sysync::DB_NotFound;
  using sysync::DB_Full;
  using sysync::DB_Error;
  using sysync::Password_ClrText_IN;
  using sysync::Password_ClrText_OUT;
  using sysync::Password_MD5_Nonce_IN;
  using sysync::Password_MD5_OUT;
  using sysync::ReadNextItem_Unchanged;
  using sysync::ReadNextItem_Changed;
  using sysync::ReadNextItem_Resumed;
  using sysync::ReadNextItem_EOF;
  using sysync::VP_GlobMulti;
  
  using sysync::Manufacturer;
  using sysync::Description;
  using sysync::GContext;
  using sysync::MinVersion;
  using sysync::NoField;
  using sysync::YesField;
  using sysync::IsAdmin;

  using sysync::GlobContext;
  using sysync::GlobContextFound;
  using sysync::Apo;
  using sysync::IntStr;
  using sysync::VersionStr;
  using sysync::ConcatNames;
  using sysync::ConcatPaths;
  using sysync::CurrentTime;
  using sysync::CompareTokens;
  using sysync::CStrToStrAppend;

#include "sync_dbapi.h"         // include the interface file within the namespace

#define BuildNumber 0           // User defined build number, initial value is 0
#define MyDB "TextDB"           // textdb example debug name

#if defined MACOSX && defined __MWERKS__ // export the items for Mac OS X as well
#pragma export on                        // (for Windows, a ".def" file is required)
#endif


/* ------------------------------------------------------------------------------ */
/* File name prefixes for the textdb implementation */
#define P_Device  "DEV_"  // device info
#define P_Data    "TDB_"  // textdb data
#define P_NewItem "NID_"  // new item id (for a persistent approach)



/* -- MODULE -------------------------------------------------------------------- */
/* All the plugin's common variables are defined here
 * Normally there is just one such context per module.
 * These variables could be defined as global vars as well.
 *
 * The example here shows, how to handle a common AND a
 * module context in parallel. A module context will be
 * opened once for the Session environment and twice for
 * each Datastore (for the data and the admin part).
 * The idea is to have the session's plugin params as
 * default values for the datastore plugin params. These
 * default values (for gDataPath/gBlobPath and gMapPath)
 * will be stored at the common context.
 */
class CommonContext {
  public:
    CommonContext() { cInstalled    = false;
                      cEngineVersion= 0; // engine's version not yet available here
                      cGlob         = NULL; }
                      
    bool              cInstalled;     // The module is already installed
    long              cEngineVersion; // the engine's version
    GlobContext*      cGlob;          // structure for reaching the common context

                                      // --- local copies of ...
                                      // NOTE: These are default values if datatore specific
                                      //       paths are not defined
    string            cDataPath;      // directory where to store data
    string            cBlobPath;      //     "       "    "   "   BLOBs
    string            cMapPath;       //     "       "    "   "   maps and admin data

    /* other elements can be defined here */
    /* ... */
}; // CommonContext


class ModuleContext {
  public:
    ModuleContext() { fCB= NULL;
                      fCC= NULL; }
                      
                                    // --- local copies of ...
    DB_Callback       fCB;          // the callback structure
    CommonContext*    fCC;          // reference to the common context

    string            fModuleName;  // the module's name
    string            fSubName;     // the sub path name (not used for textdb implementation)
    string            fContextName; // the context  name

    string            fDataPath;    // directory where to store data
    string            fBlobPath;    //     "       "    "   "   BLOBs
    string            fMapPath;     //     "       "    "   "   maps and admin data
    
    /* other elements can be defined here */
    /* ... */
}; // ModuleContext

/* <mContext> will be casted to the ModuleContext* structure */
static ModuleContext* MoC( CContext mContext ) { return (ModuleContext*)mContext; }



TSyError Module_CreateContext( CContext *mContext, cAppCharP   moduleName,
                                                   cAppCharP      subName,
                                                   cAppCharP mContextName, DB_Callback mCB )
{
  DEBUG_DB( mCB, MyDB,Mo_CC, "'%s' (%s)", ConcatNames( moduleName,subName ).c_str(),
                                                             mContextName );
  if (!CB_gContext( mCB )) return LOCERR_TOOOLD; // the new <gContext> system is required !
  
  ModuleContext*       mc= new ModuleContext; // get the (unique) module context
  *mContext= (CContext)mc;                    // return the context variable

  mc->fModuleName =   moduleName;         // local copy of module's     name
  mc->fSubName    =      subName;         //   "    "   "  module's sub name
  mc->fContextName= mContextName;         //   "    "   "  the contexts name (e.g. "contacts")
  mc->fCB         = mCB;                  //   "    "   "  the callback reference
  
  string db= MyDB;
  #ifdef _MSC_VER
         db+= "_visual";
  #endif
  #if defined __MACH__ && !defined __MWERKS__
         db+= "_universal";
  #endif
  
  GlobContext* g= (GlobContext*)mCB->gContext;
  if (GlobContextFound( db, g ))
            mc->fCC=    (CommonContext*)g->ref; // connect the structure    
  else {    mc->fCC= new CommonContext;
            mc->fCC->cGlob= g;
    g->ref= mc->fCC; // connect the structure    
  } // if
  g->cnt++;

  DEBUG_DB( mCB, MyDB,Mo_CC, "mContext=%08X  gContext=%08X g->ref=%08X",
                             *mContext, mCB->gContext,     g->ref );
  return LOCERR_OK;
} // Module_CreateContext



/* -- VERSION/CAPABILITIES ---- */
/* Get the plug-in's version number */
CVersion Module_Version( CContext mContext )
{
  /* The current plugin's SDK version is expected here */
  CVersion v= Plugin_Version( BuildNumber ); 

  if             ( mContext )
    DEBUG_DB( MoC( mContext )->fCB, MyDB,Mo_Ve, "%s", VersionStr( v ).c_str() );
  return v;
} /* Module_Version */



/* Get the plug-in's capabilities */
TSyError Module_Capabilities( CContext mContext, appCharP *mCapabilities )
{
  if                    (!mContext) return DB_Forbidden;
  ModuleContext* mc= MoC( mContext );

  string        s = MyPlatform();
  s+= '\n';     s+= DLL_Info;
  Manufacturer( s, "Synthesis AG" );                 // **** can be adapted ***
  Description ( s, "Text database module. Writes data directly to TDB_*.txt file" );
  YesField    ( s,  CA_ADMIN_Info );
  MinVersion  ( s,  VP_GlobMulti  ); // at least V1.5.1

  if              (mc->fCC) {
      GContext( s, mc->fCC->cGlob );
  } // if
  
  // Parts of the plug-in can be disabled (will not be connected and therefore not be used)
  // These settings are expected ath the "target_options.h" of the plugin
  #ifdef DISABLE_PLUGIN_SESSIONAUTH
    NoField( s, Plugin_SE_Auth );
  #endif
  #ifdef DISABLE_PLUGIN_DEVICEADMIN
    NoField( s, Plugin_DV_Admin );
  #endif
  #ifdef DISABLE_PLUGIN_DATASTOREADMIN
    NoField( s, Plugin_DS_Admin );
  #endif
  #ifdef DISABLE_PLUGIN_DATASTOREDATA
    NoField( s, Plugin_DS_Data );
  #endif

  *mCapabilities= StrAlloc( s.c_str() );
  DEBUG_DB( mc->fCB, MyDB,Mo_Ca, "'%s'", *mCapabilities );
  return LOCERR_OK;
} // Module_Capabilities



TSyError Module_PluginParams( CContext mContext, cAppCharP mConfigParams,
                                                  CVersion engineVersion )
{
  /* Decide, which data path to take */
  cAppCharP DataFilePath= "datafilepath"; // TDB_
  cAppCharP BlobFilePath= "blobfilepath"; // BLB_
  cAppCharP  MapFilePath=  "mapfilepath"; // DEV_ / ADM_ / MAP_

  // ---- legacy, please use <datafilepath> and <mapfilepath> for newer implementations
  #if defined LINUX || defined MACOSX
    cAppCharP DataPath= "unixpath";
  #else
    cAppCharP DataPath= "winpath";
  #endif
  // ---- END legacy ----

  ModuleContext* mc= MoC( mContext );
  DEBUG_DB     ( mc->fCB, MyDB,Mo_PP, "EngineVersion: %s", 
                                  VersionStr( engineVersion ).c_str() );
  DEBUG_DB     ( mc->fCB, MyDB,Mo_PP, "'%s'", mConfigParams );

  // ------------------------------------------------------
  // Get the data path for the textDB files
  char* vv;
  mc->fDataPath= mc->fCC->cDataPath; // as default

  // ---- legacy data path
  if (Field( mConfigParams,   DataPath, &vv )) {
                         mc->fDataPath= "";
    CStrToStrAppend( vv, mc->fDataPath );
    StrDispose     ( vv );
  } // if
  // ---- END legacy ----

  // *** DATA PATH ***
  // overwrite it, if using new definition
  if (Field( mConfigParams,   DataFilePath, &vv )) {
                         mc->fDataPath= "";
    CStrToStrAppend( vv, mc->fDataPath );
    StrDispose     ( vv );
  } // if

  // *** BLOB PATH ***
  // the same for the BLOB files
  // take this as the default
                             mc->fBlobPath= mc->fCC->cBlobPath; //    take the global one
  if (mc->fBlobPath.empty()) mc->fBlobPath=      mc->fDataPath; // or take the local data path
  if (mc->fBlobPath.empty()) mc->fBlobPath= mc->fCC->cDataPath; // or even more globally

  if (Field( mConfigParams,       BlobFilePath, &vv )) {
                             mc->fBlobPath= "";
    CStrToStrAppend( vv,     mc->fBlobPath );
    StrDispose     ( vv );
  } // if

  // *** MAP PATH ***
  // and the same for the map/admin files
  // take this as the default
                             mc->fMapPath= mc->fCC->cMapPath;  //    take the global one
  if (mc->fMapPath.empty())  mc->fMapPath=      mc->fDataPath; // or take the local data path
  if (mc->fMapPath.empty())  mc->fMapPath= mc->fCC->cDataPath; // or even more globally

  if (Field( mConfigParams,       MapFilePath, &vv )) {
                             mc->fMapPath= "";
    CStrToStrAppend( vv,     mc->fMapPath );
    StrDispose     ( vv );
  } // if

  // get global defaults when called for session context
  // Indication: <mc->fContextName> is empty.
  if (!mc->fCC->cInstalled &&   mc->fContextName.empty()) {
       mc->fCC->cDataPath     = mc->fDataPath;
       mc->fCC->cBlobPath     = mc->fBlobPath;
       mc->fCC->cMapPath      = mc->fMapPath;
  } // if
       
  if  (mc->fCC->cEngineVersion==0) // set it once     
       mc->fCC->cEngineVersion= engineVersion;

  mc->fCC->cInstalled= true; // now it's really installed
  return LOCERR_OK;
} // Module_PluginParams



/* Dispose the memory of the module context */
void Module_DisposeObj( CContext mContext, void* memory )
{
  ModuleContext*   mc= MoC( mContext );
  DEBUG_Exotic_DB( mc->fCB, MyDB,Mo_DO, "%d free at %08X '%s'", mc, memory,memory );
  StrDispose     ( memory );
} // Module_DisposeObj



TSyError Module_DeleteContext( CContext mContext )
{
  ModuleContext*  mc= MoC( mContext );
//printf( "DEL: '%s' %08X\n", mc->fModuleName.c_str(), mc->fCB->gContext );
  DEBUG_DB      ( mc->fCB, MyDB,Mo_DC, "'%s'", mc->fModuleName.c_str() );
  
  GlobContext* g= mc->fCC->cGlob;
               g->cnt--;
  if          (g->cnt==0) {
    CommonContext* cc= (CommonContext*)g->ref;
    delete         cc;                 g->ref= NULL;
  } // if
  
  delete mc;
  return LOCERR_OK;
} // Module_DeleteContext



// ---------------------- session handling ---------------------
#if !defined DISABLE_PLUGIN_DEVICEADMIN || !defined DISABLE_PLUGIN_DEVICEADMIN

/*! Each session requires a session context, which will be created with
 *  'Session_CreateContext' and deleted with 'Session_DeleteContext'.
 */
class SessionContext {
  public:
    DB_Callback fCB;

    TDBItem  fDevList;  // The current device list
    TDBItem* fDev;      // reference
    string   fDeviceID; // stored <aDeviceID> value of 'CheckDevice'

    /* other elements can be defined here */
    /* ... */
}; // SessionContext

/* <sContext> will be casted to the SessionContext* structure */
static SessionContext* SeC( CContext sContext ) { return (SessionContext*)sContext; }



/* Create a context for a new session */
TSyError Session_CreateContext( CContext *sContext, cAppCharP sessionName, DB_Callback sCB )
{
  SessionContext*      sc= new SessionContext;
  if                  (sc==NULL) return DB_Full;

                       sc->fDev= NULL;
                       sc->fCB = sCB;
  DEBUG_DB           ( sc->fCB, MyDB,Se_CC, "%d '%s'", sc,sessionName );
  *sContext= (CContext)sc; // return the context variable
  return LOCERR_OK;
} // Session_CreateContext


/* ----- "script-like" ADAPT --------- */
TSyError Session_AdaptItem( CContext sContext, appCharP *sItemData1,
                                               appCharP *sItemData2,
                                               appCharP *sLocalVars, uInt32 sIdentifier )
{
  /**** CAN BE ADAPTED BY USER ****/
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,"AdaptItem", "'%s' '%s' '%s' id=%d",
                                *sItemData1,*sItemData2,*sLocalVars, sIdentifier );
  return LOCERR_OK;
} /* Session_AdaptItem */


/*! Create a alphanumerical string out of <aName> (ignore all other chars)
 *  This is an internal utility proc.
 */
static string AlphaNum( cAppCharP aName )
{
  cAppCharP p= aName;
  string    s;

  while (*p!='\0') {
    if (isalnum( *p )) s+= tolower( *p );
    p++;
  } // while

  return s;
} // AlphaNum


#ifndef DISABLE_PLUGIN_DEVICEADMIN
/* Check the database entry of <deviceID> and return its nonce string */
TSyError Session_CheckDevice( CContext sContext, cAppCharP aDeviceID, appCharP *sDevKey,
                                                                      appCharP *nonce )
{
  SessionContext* sc= SeC         ( sContext );
  ModuleContext*  mc= MoC( sc->fCB->cContext );

  TSyError  err= LOCERR_OK;
  sc->fDeviceID=    AlphaNum( aDeviceID );
  string    s= P_Device + sc->fDeviceID + ".txt";

       sc->fDevList.fFileName= ConcatPaths( mc->fMapPath, s );
  err= sc->fDevList.LoadDB( true, "DEV", sc->fCB ); // load the DEV file info

  if                    (!sc->fDev) {
    /* get the device */  sc->fDev= &sc->fDevList;
    bool found= ListNext( sc->fDev );
    if (!found) { /* not found -> create such an element */
      err= sc->fDevList.CreateEmptyItem( s, sc->fDev ); if (err) return err;
    } // if
  } // if

  *sDevKey= StrAlloc( sc->fDeviceID.c_str() ); // 1:1 assigned at the moment
  *nonce  = StrAlloc( sc->fDev->fToken.c_str() );
  DEBUG_DB          ( sc->fCB, MyDB,Se_CD,
                     "devKey='%s' nonce='%s' err=%d", *sDevKey, *nonce, err );
  return LOCERR_OK;
} /* Session_CheckDevice */



/* Get a new nonce from the database.
 * If this function returns an error (as done for this implementiation here),
 * the SyncML engine has to create its own nonce.
 */
TSyError Session_GetNonce( CContext sContext, appCharP * /* nonce */ )
{
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,Se_GN, "%d (not supported)", sc );
  return DB_NotFound;
} /* Session_GetNonce */



/* Save the new nonce (which will be expected to be returned
 * in the next session for this device
 */
TSyError Session_SaveNonce( CContext sContext, cAppCharP nonce )
{
  TSyError       err= DB_NotFound;
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB, Se_SN, "%d '%s'", sc,nonce );

  if (sc->fDev) { sc->fDev->fToken= nonce;
                  sc->fDevList.fChanged= true;
    err=          sc->fDevList.SaveDB( true, sc->fCB ); // save it
  } // if

  return err;
} /* Session_SaveNonce */



/* Save the device info for <sContext> */
TSyError Session_SaveDeviceInfo( CContext sContext, cAppCharP aDeviceInfo )
{
  SessionContext* sc= SeC         ( sContext );
  ModuleContext*  mc= MoC( sc->fCB->cContext );
  TSyError       err= DB_NotFound;

  if (!sc->fDev) {
    string s= P_Device + sc->fDeviceID + ".txt";
    sc->fDevList.fFileName= ConcatPaths( mc->fMapPath, s );

    /* get the device */  sc->fDev= &sc->fDevList;
    bool found= ListNext( sc->fDev );
    if (!found) { /* not found -> create such an element */
      err= sc->fDevList.CreateEmptyItem( s, sc->fDev ); if (err) return err;
    } // if
  } // if

  if              (sc->fDev) {
              err= sc->fDevList.UpdateFields( sc->fCB, aDeviceInfo, sc->fDev, true );
    if (!err) err= sc->fDevList.SaveDB( true, sc->fCB );
  } // if

  DEBUG_DB( sc->fCB, MyDB,Se_SD, "%d err=%d", sc, err );
  sc->fDevList.Disp_Items( sc->fCB, Se_SD );

  return err;
} /* Session_SaveDeviceInfo */


TSyError Session_GetDBTime( CContext /* sContext */, appCharP *currentDBTime )
{
  string                    iso8601_str= CurrentTime();
  *currentDBTime= StrAlloc( iso8601_str.c_str() );
  return LOCERR_OK;
} /* Session_GetDBTime */
#endif // DISABLE_PLUGIN_DEVICEADMIN



#ifndef DISABLE_PLUGIN_SESSIONAUTH
/* There are currently 4 different password modes supported:
 * Return: Password_ClrText_IN    'SessionLogin' will get    clear text password
 *         Password_ClrText_OUT         "        must return clear text password
 *         Password_MD5_OUT             "        must return MD5 coded  password
 *         Password_MD5_Nonce_IN        "        will get    MD5B64(MD5B64(user:pwd):nonce)
 */
sInt32 Session_PasswordMode( CContext sContext )
{
//int mode= Password_ClrText_IN;
//int mode= Password_ClrText_OUT;
//int mode= Password_MD5_Nonce_IN;
  int mode= Password_MD5_OUT;

  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,Se_PM, "%d mode=%d", sc, mode );
  return mode;
} /* Session_PasswordMode */



/* Make login */
/* This example here shows how the different modes are working  */
/* In practice, it's sufficient to support one of theses modes  */
/* The chosen mode must be returned with "Session_PasswordMode" */
TSyError Session_Login( CContext sContext, cAppCharP sUsername, appCharP *sPassword,
                                                                appCharP *sUsrKey )
{
  cAppCharP MD5_OUT_SU = "11JrMX94iTR1ob5KZFtEwQ=="; // the MD5B64 example for "super:user"
  cAppCharP MD5_OUT_TT = "xPlhtDgO4k2YL+127za+nw=="; // the MD5B64 example for  "test:test"

  SessionContext* sc= SeC( sContext );
  TSyError       err= DB_Forbidden; // default
  sInt32        mode= Session_PasswordMode( sContext );

  if (strcmp( sUsername,"super" )==0) { // an example account ("super"), hard coded here
    switch (mode) {
      case Password_ClrText_IN  : if (strcmp( *sPassword,"user" )==0)  err= LOCERR_OK; break;
      case Password_ClrText_OUT : *sPassword=  StrAlloc( "user" );     err= LOCERR_OK; break;
      case Password_MD5_OUT     : *sPassword=  StrAlloc( MD5_OUT_SU ); err= LOCERR_OK; break;
      case Password_MD5_Nonce_IN: break; // currently not supported for SDK_textdb
    } // switch

    if (!err) *sUsrKey= StrAlloc( "5678" ); // <sUsrKey> allows to access the datastore later
  } // if

  if (strcmp( sUsername,"test"  )==0) { // an example account ("test"),  hard coded here
    switch (mode) {
      case Password_ClrText_IN  : if (strcmp( *sPassword,"test" )==0)  err= LOCERR_OK; break;
      case Password_ClrText_OUT : *sPassword=  StrAlloc( "test" );     err= LOCERR_OK; break;
      case Password_MD5_OUT     : *sPassword=  StrAlloc( MD5_OUT_TT ); err= LOCERR_OK; break;
      case Password_MD5_Nonce_IN: break; // currently not supported for SDK_textdb
    } // switch

    if (!err) *sUsrKey= StrAlloc( "test" );
  } // if

  appCharP                               pw= *sPassword;
  if  (err && (mode==Password_ClrText_OUT ||
               mode==Password_MD5_OUT )) pw= (appCharP)"";

  appCharP                               uk= *sUsrKey;
  if  (err)                              uk= (appCharP)"";

  DEBUG_DB( sc->fCB, MyDB,Se_LI,"%d usr='%s' pwd='%s' => key='%s' err=%d",
                                 sc, sUsername, pw,uk, err );
  return err;
} // Session_Login



/* Make logout */
TSyError Session_Logout( CContext sContext )
{
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,Se_LO, "%d", sc );
  return LOCERR_OK; /* do nothing */
} // Session_Logout
#endif // DISABLE_PLUGIN_SESSIONAUTH



void Session_DisposeObj( CContext sContext, void* memory )
{
  SessionContext*  sc= SeC( sContext );
  DEBUG_Exotic_DB( sc->fCB, MyDB,Se_DO, "%d free at %08X '%s'", sc, memory,memory );
  StrDispose     ( memory );
} /* Session_DisposeObj */



void Session_ThreadMayChangeNow( CContext sContext )
{
  SessionContext*  sc= SeC( sContext );
  DEBUG_Exotic_DB( sc->fCB, MyDB,Se_TC, "%d", sc );
} /* Session_ThreadMayChangeNow */



/* For Debugging only !
 * Will not be called by the SyncML engine
 * Can be implemented empty, if not needed
 */
void Session_DispItems( CContext sContext, bool allFields, cAppCharP specificItem )
{
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,Se_DI, "%d", sc );
                  sc->fDevList.Disp_Items( sc->fCB, "", allFields, specificItem );
} /* Session_DispItems */



/* Delete the session context; there will be no subsequent calls
 * to <sContext>. All 'Session_DisposeObj' calls will be done before
 */
TSyError Session_DeleteContext( CContext sContext )
{
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,Se_DC, "%d", sc );
  delete          sc;
  return LOCERR_OK;
} // Session_DeleteContext

#endif
// ---- end session ------------------------------------------



// ---------- context class ----------------------------------
/*! Each datastore access requires a context, which will be created with
 *  'CreateContext' and deleted with 'DeleteContext'.
 */
typedef struct {
  bool   allfields; // false (default): 'ReadNextItem' will return ID only /
                    // true:             returns ID + data

  /* other elements can be defined here */
  /* ... */
} SupportType;


class TDBContext {
  public:
    TDBContext( cAppCharP aContextName, DB_Callback aCB, cAppCharP sDevKey, cAppCharP sUsrKey );
   ~TDBContext();

    DB_Callback fCB;          // callback structure for logging

    string      fContextName; // local copy of the context name
    string      fUserName;    // unique key of     usr-local
    string      fCombiName;   // unique key of dev-usr-local
    TDBItem     fLogList;     // log   info

    TDBItem*    fCurrent;     // for 'ReadNextItem'
    int         fNth;         // and according counter

    TDBItem     fNewItem;
    string      fNewID;
    TDBItem     fItemList;

    #ifndef DISABLE_PLUGIN_DATASTOREADMIN
    TAdminData  fAdmin;       // admin data handling
    #endif
    
    #ifndef DISABLE_PLUGIN_BLOBS
    TBlob       fBlob;        // current blob info
    #endif

    TDBItem     fSupportList;
    SupportType fSupport;     // fast readable form

    TDBItem     fFilterList;

    string      fLastToken;   // the <newToken> of the last session
    string      fResumeToken; // suspend/resume support
    string      fNewToken;    // the <newToken> of this session

    void     ResetCounter();
    TSyError RemoveAll    ( TDBItem* hdK );
    TSyError UpdateSupport( TDBItem* hdK, int &n );
    void     KeyAndField  ( cAppCharP       sKey, cAppCharP       sField, string &s );
    void     KeyAndField  ( TDBItemField* actKey, TDBItemField* actField, string &s );

    /* other elements can be defined here */
    /* ... */
}; // DBContext
typedef TDBContext* ContextP;


/* <aContext> will be casted to the ContextP structure */
static ContextP DBC( CContext aContext ) { return (ContextP)aContext; }


/* the constructor */
TDBContext::TDBContext( cAppCharP aContextName, DB_Callback aCB, cAppCharP sDevKey, 
                                                                 cAppCharP sUsrKey )
{
                         fContextName= aContextName; // make a local copy
  bool asAdmin= IsAdmin( fContextName );  
  
            fCB = aCB;
  DEBUG_DB( fCB, MyDB,Da_CC, "'%s' dev='%s' usr='%s' type=%s",
                             fContextName.c_str(), sDevKey,sUsrKey, 
                             asAdmin ? "ADMIN" : "DATA" );

  fUserName = ConcatNames( sUsrKey, fContextName, "_" );
  fCombiName= ConcatNames( sDevKey, fUserName,    "_" );

  ModuleContext*          mc= MoC( aCB->cContext );
  #ifndef DISABLE_PLUGIN_DATASTOREADMIN
  if (asAdmin)
    fAdmin.Init( aCB, MyDB, mc->fMapPath,  fContextName, sDevKey,sUsrKey );
  #endif
  
  #ifndef DISABLE_PLUGIN_BLOBS
    fBlob.Init ( aCB, MyDB, mc->fBlobPath, fContextName, sDevKey,sUsrKey );
  #endif

  ResetCounter();
  fNewItem.init    ( "newItem",           aCB );
  fItemList.init   ( "itemID","parentID", aCB );
  fSupportList.init( "support",           aCB, new TDBItem );
  fSupport.allfields= false;
  fFilterList.init ( "filter",            aCB, new TDBItem );
  fLogList.init    ( "log",               aCB, new TDBItem );
} // constructor


TDBContext::~TDBContext() {
  DEBUG_DB( fCB, MyDB,Da_DC, "" );
} // destructor



/*! Reset the counter for 'ReadNextItem' */
void TDBContext::ResetCounter() {
  fCurrent= &fItemList;
  fNth= 0;
} // ResetCounter


TSyError TDBContext::UpdateSupport( TDBItem* hdK, int &n )
// Update the specific field <fKey> with the new value <fVal>.
// Create the whole bunch of missing keys, if not yet available.
{
  TDBItem*      hdI =  hdK; ListNext( hdI );
  TDBItemField* actK= &hdK->item; // the key
  TDBItemField* actI= &hdI->item;
  
//printf( "   v allfields=%s\n", 
//              fSupport.allfields?"true":"false" );
  
      n= 0; // init nr of valid contexts
  int i= 0; // init nr of total contexts
  while (ListNext( actK,actI ) && i==n) { // still all ?
    if                   (strcmp( actK->field.c_str(), Da_RN      )==0) {
      fSupport.allfields= strcmp( actI->field.c_str(),"allfields" )==0;
      n++;
    } // if
    
    i++;
    
  //printf( "     i=%d n=%d '%s' '%s'\n", 
  //              i, n, actK->field.c_str(), actI->field.c_str() );
  } // while

//printf( "   n allfields=%s\n", 
//              fSupport.allfields?"true":"false" );
  return LOCERR_OK;
} // UpdateSupport


TSyError TDBContext::RemoveAll( TDBItem* hdK )
// Remove all elements of the list
{
  TDBItem*      hdI =  hdK; ListNext( hdI );
  TDBItemField* prvK= &hdK->item; // the key
  TDBItemField* prvI= &hdI->item;
    
  while (true) {
    TDBItemField* actK= prvK; // the key
    TDBItemField* actI= prvI;
      
    if (!ListNext( actK,actI )) break;
    
    prvK->next= actK->next;
    actK->next= NULL; // avoid destroying the whole chain
    delete      actK;

    prvI->next= actI->next;
    actI->next= NULL; // avoid destroying the whole chain
    delete      actI;
  } // while

  return LOCERR_OK;
} // RemoveAll




/* -- OPEN ----------------------------------------------------------------------- */
TSyError CreateContext( CContext *aContext, cAppCharP aContextName, DB_Callback aCB,
                                            cAppCharP sDevKey, 
                                            cAppCharP sUsrKey )
{
  ContextP             ac= new TDBContext( aContextName, aCB, sDevKey,sUsrKey );
  if                  (ac==NULL) return DB_Full; // this is really fatal
  *aContext= (CContext)ac;
  return LOCERR_OK;
} /* CreateContext */



/* -- GENERAL -------------------------------------------------------------------- */
#if !defined DISABLE_PLUGIN_DATASTOREADMIN || !defined DISABLE_PLUGIN_DATASTOREDATA
uInt32 ContextSupport( CContext aContext, cAppCharP aSupportRules )
{
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_CS,            "'%s'", aSupportRules );
            ac->RemoveAll( &ac->fSupportList );
            ac->fSupportList.UpdateFields( ac->fCB, aSupportRules );

  int    n;
  ac->UpdateSupport( &ac->fSupportList, n );
  DEBUG_DB( ac->fCB, MyDB,Da_CS, "allfields=%d", ac->fSupport.allfields );
  return n; // number of supported contexts
} // ContextSupport


uInt32 FilterSupport( CContext aContext, cAppCharP aFilterRules )
{
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_FS,           "'%s'", aFilterRules );
            ac->fFilterList.UpdateFields( ac->fCB, aFilterRules );
  return 0; // none supported until now
} // FilterSupport


void ThreadMayChangeNow( CContext aContext )
{
  ContextP         ac= DBC( aContext );
  DEBUG_Exotic_DB( ac->fCB, MyDB,Da_TC, "(%08X)", ac );

                   ac->fNewItem.SaveDB ( false ); // possibly partly save
                   ac->fItemList.SaveDB( false );
} /* ThreadMayChangeNow */


void WriteLogData( CContext aContext, cAppCharP logData )
{
  ContextP ac= DBC( aContext );
  ac->fLogList.UpdateFields( ac->fCB, logData );
  ac->fLogList.Disp_Items  ( ac->fCB, Da_WL   );
} /* WriteLogData */


/* ---- display database contents, for debugging only ---- */
/* Writes the context of items to dbg output path
 * This routine is implemented for debug purposes only and will NOT BE CALLED by the
 * SyncML engine.
 */
void DispItems( CContext aContext, bool allFields, cAppCharP specificItem )
{
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_DI, "BEGIN" );

  TDBItem*      actB= &ac->fItemList;
  TDBItemField* actK;

  ac->fSupportList.Disp_Items( NULL, "fSupport", allFields,specificItem );
  ac->fFilterList.Disp_Items ( NULL, "fFilter",  allFields,specificItem );
  DEBUG_( ac->fCB, "---" );

  if (!actB->next) { // key titles only
      DEBUG_( ac->fCB, "%s",    actB->c_str() );

                     actK= &actB->item; // the key identifier
    while (ListNext( actK ))
      DEBUG_( ac->fCB, "%s: -", actK->field.c_str() );
  }
  else ac->fItemList.Disp_Items( NULL, "fItem",  allFields,specificItem );

  DEBUG_DB( ac->fCB, MyDB,Da_DI, "END" );
} // DispItems
#endif
// ----- end general ----------------------------------------



#ifndef DISABLE_PLUGIN_DATASTOREADMIN
/* -- ADMINISTRATION ------------------------------------------------------------ */
/* (will be passed to the "admindata" module) */
TSyError LoadAdminData     ( CContext    aContext,    cAppCharP    aLocDB,
                                                      cAppCharP    aRemDB, appCharP  *adminData ) {
  return DBC( aContext )->fAdmin.LoadAdminData                   ( aLocDB, aRemDB,    adminData );
}     // LoadAdminData

TSyError LoadAdminDataAsKey( CContext /* aContext */, cAppCharP /* aLocDB */,
                                                      cAppCharP /* aRemDB */, KeyH /* adminKey */ ) {
  return LOCERR_NOTIMP;
}     // LoadAdminDataAsKey


TSyError SaveAdminData    ( CContext     aContext, cAppCharP  adminData ) {
  return DBC( aContext )->fAdmin.SaveAdminData              ( adminData );
}     // SaveAdminData

TSyError SaveAdminDataAsKey( CContext /* aContext */, KeyH /* adminKey */ ) {
  return LOCERR_NOTIMP;
}     // SaveAdminDataAsKey


bool   ReadNextMapItem( CContext aContext,  MapID mID, bool aFirst ) {
  return DBC( aContext )->fAdmin.ReadNextMapItem( mID,      aFirst );
}     // ReadNextMapItem

TSyError InsertMapItem( CContext aContext, cMapID mID ) {
  return DBC( aContext )->fAdmin.InsertMapItem  ( mID );
}     // InsertMapItem

TSyError UpdateMapItem( CContext aContext, cMapID mID ) {
  return DBC( aContext )->fAdmin.UpdateMapItem  ( mID );
}     // UpdateMapItem

TSyError DeleteMapItem( CContext aContext, cMapID mID ) {
  return DBC( aContext )->fAdmin.DeleteMapItem  ( mID );
}     // DeleteMapItem
#endif // DISABLE_PLUGIN_DATASTOREADMIN




#ifndef DISABLE_PLUGIN_DATASTOREDATA
/* -- READ ---------------------------------------------------------------------- */
TSyError StartDataRead( CContext aContext, cAppCharP lastToken, cAppCharP resumeToken )
{
  ContextP         ac= DBC         ( aContext );
  ModuleContext*   mc= MoC( ac->fCB->cContext );

  ac->fLastToken  =   lastToken;       // store it in case of no success
  ac->fResumeToken= resumeToken;       // used for OMA DS 1.2 suspend/resume support
  ac->fNewToken   = CurrentTime( -1 ); // make sure that all changes will be seen

  DEBUG_DB( ac->fCB, MyDB,Da_SR, "last='%s' resume='%s' new='%s'",
                                  lastToken,resumeToken, ac->fNewToken.c_str() );

  ac->ResetCounter(); // start with the first element

  string  s= P_Data     + ac->fUserName + ".txt";
  if (mc) s= ConcatPaths( mc->fDataPath,s );

  ac->fItemList.fFileName= s;    // load the database into memory
  ac->fItemList.LoadDB( false ); // implementation could be changed to "by name" here

  // -------------------
          s= P_NewItem  + ac->fUserName + ".txt";
  if (mc) s= ConcatPaths( mc->fDataPath,s );

  ac->fNewItem.fFileName= s;     // load the database into memory
  ac->fNewItem.LoadDB ( false ); // implementation could be changed to "by name" here
  
  TDBItem*      act=  &ac->fNewItem;
                       ac->fNewID= F_First;
  if (ListNext( act )) ac->fNewID= act->itemID;

  return LOCERR_OK;
} /* StartDataRead */



TSyError ReadNextItem( CContext aContext, ItemID aID, appCharP *aItemData,
                                                      sInt32   *aStatus, bool aFirst )
{
  ContextP    ac= DBC( aContext );
  if (aFirst) ac->ResetCounter();
  *aItemData= NULL;           // safety setting, if no value returning
  cAppCharP   mode= "Unused"; // default
  string      s;

  ac->fNth++;
  if            (ListNext( ac->fCurrent )) {
    aID->item  = StrAlloc( ac->fCurrent->itemID.c_str()   );
    aID->parent= StrAlloc( ac->fCurrent->parentID.c_str() );

    if (ac->fSupport.allfields) ReadItem( aContext, aID, aItemData );

    // assume, there are two yyyymmddThhmmssZ strings, which can be
    // compared directly w/o conversion into lineartime
             *aStatus= CompareTokens( ac->fCurrent->fToken, ac->fLastToken, ac->fResumeToken );
    switch  (*aStatus) {
      case ReadNextItem_Unchanged: mode= "Unchanged"; s= Apo(ac->fLastToken  ) + " >= "; break;
      case ReadNextItem_Changed  : mode=   "Changed"; s= Apo(ac->fLastToken  ) + " < " ; break;
      case ReadNextItem_Resumed  : mode=   "Resumed"; s= Apo(ac->fResumeToken) + " < " ; break;
    } // switch
    s+= Apo( ac->fCurrent->fToken );

    DEBUG_DB( ac->fCB, MyDB,Da_RN, "(no=%d) %s (%s): %s",
                             ac->fNth, mode, ac->fCurrent->c_str(), s.c_str() );
    return LOCERR_OK;
  } // if

  DEBUG_DB( ac->fCB, MyDB,Da_RN, "(no=%d) EOF", ac->fNth );
  *aStatus= ReadNextItem_EOF;
  return LOCERR_OK;
} /* ReadNextItem */


// The <asKey> variant is not supported for textdb
TSyError ReadNextItemAsKey( CContext /* aContext */, ItemID /* aID */,     KeyH /* aItemKey */,
                                                    sInt32* /* aStatus */, bool /* aFirst */ ) {
  return LOCERR_NOTIMP;
} /* ReadNextItemAsKey */



static bool ItemFound( cItemID aID, TDBItem* &actL )
{
  appCharP p= aID->parent; 
  if  (!p) p= (appCharP)"";

  while         (ListNext( actL )) {
    if (strcmp( aID->item, actL->itemID.c_str()   )==0 && // search for item <aID>
        strcmp( p,         actL->parentID.c_str() )==0) return true;
  } // while      
  
  return false;
} // ItemFound


TSyError ReadItem( CContext aContext, cItemID aID, appCharP *aItemData )
{
  ContextP      ac = DBC( aContext );
  TSyError      err= DB_NotFound;
  TDBItem*      actL;
  TDBItemField* actK;
  TDBItemField* actF;
  int           a= 0;
  string        s, dat;

  *aItemData= (appCharP)"";
                      actL= &ac->fItemList;
  if (ItemFound( aID, actL )) {
                      actK     = &ac->fItemList.item; // now concatenate <aItemData>
    int n= 0;              actF=         &actL->item;
    while  (ListNext( actK,actF )) {
      s= KeyAndField( actK,actF );

      if (n>0) dat+= '\n';
      n++;     dat+= s;
    } // while

    *aItemData= StrAlloc( dat.c_str() );
    err= LOCERR_OK;
  } // if
  
  s= ItemID_Info( aID );

  if (!err) a= strlen( *aItemData );
  DEBUG_DB( ac->fCB, MyDB,Da_RI, "%s '%s' len=%d err=%d",
                                  s.c_str(), *aItemData, a, err );
  return err;
} /* ReadItem */


// The <asKey> variant is not supported for textdb
TSyError ReadItemAsKey( CContext /* aContext */, cItemID /* aID */, KeyH /* aItemKey */ ) {
  return LOCERR_NOTIMP;
} /* ReadItemAsKey */



TSyError EndDataRead( CContext aContext )
{
  ContextP  ac= DBC( aContext ); // no additional actions needed at this point
  DEBUG_DB( ac->fCB, MyDB,Da_ER, "(%08X)", ac ); // just logging
  return LOCERR_OK;
} /* EndDataRead */




/* -- WRITE --------------------------------------------------------------------- */
TSyError StartDataWrite( CContext aContext )
{
  ContextP  ac= DBC( aContext ); // no additional actions needed at this point
  DEBUG_DB( ac->fCB, MyDB,Da_SW, "(%08X)", ac ); // just logging
  return LOCERR_OK;
} /* StartDataWrite */



TSyError InsertItem( CContext aContext, cAppCharP aItemData, ItemID newID )
{
  ContextP ac= DBC( aContext );
  string   newItemID;
  TDBItem* act;

  ItemID_Struct a; a.item  = (appCharP)"";
                   a.parent= newID->parent; if (!a.parent) a.parent= (appCharP)"";

  TSyError err= ac->fItemList.CreateEmptyItem( &a, newItemID, act, ac->fNewID );

  if (err) a.item= (appCharP)"???"; // undefined
  else     a.item= (appCharP)newItemID.c_str();
  string   s= ItemID_Info( &a );

  DEBUG_DB( ac->fCB, MyDB,Da_II, "%s '%s' err=%d", s.c_str(), aItemData, err );

  if (!err) { // the newly created item will now be updated => filled with content
    ItemID_Struct u; u.item  = NULL; // no change of item
                     u.parent= NULL;
    err= UpdateItem( aContext, aItemData, &a,&u );
  } // if

  if (err) { 
    DeleteItem( aContext, &a ); // remove it again in case of an error
    newID->item= NULL; 
  }
  else { 
    newID->item= StrAlloc    ( newItemID.c_str() );
    ac->fNewID = IntStr( atoi( newID->item )+1 );
  } // if

  return err;
} /* InsertItem */


// The <asKey> variant is not supported for textdb
TSyError InsertItemAsKey( CContext /* aContext */, KeyH /* aItemKey */, ItemID /* newID */ ) {
  return LOCERR_NOTIMP;
} /* InsertItemAsKey */



TSyError UpdateItem( CContext aContext, cAppCharP aItemData, cItemID      aID, 
                                                              ItemID /* updID */ )
// This implementation will not create a new <updID>
{
  ContextP ac = DBC( aContext );
  TSyError err= DB_NotFound;
                                    TDBItem* actI;
       err=      ac->fItemList.GetItem( aID, actI ); // the item must exist already
  if (!err) err= ac->fItemList.UpdateFields( ac->fCB, aItemData, actI, false,
                                             ac->fNewToken.c_str() );

                                            string s= ItemID_Info( aID );
  DEBUG_DB( ac->fCB, MyDB,Da_UI, "%s '%s' err=%d", s.c_str(), aItemData, err );
  return err;
} /* UpdateItem */


// The <asKey> variant is not supported for textdb
TSyError UpdateItemAsKey( CContext /* aContext */, KeyH /* aItemKey */, cItemID   /* aID */, 
                                                                         ItemID /* updID */ ) {
  return LOCERR_NOTIMP;
} /* UpdateItemAsKey */



TSyError MoveItem( CContext aContext, cItemID aID, cAppCharP newParID )
{
  ContextP ac = DBC( aContext );
  TSyError err= DB_NotFound;
  TDBItem* actL;

  ItemID_Struct nID;   nID.item  =      aID->item;
                       nID.parent= (appCharP)newParID;
  err= ac->fItemList.ParentExist( aID->item, newParID );

  if (!err) err= ac->fItemList.GetItem( aID, actL );
  if (!err) actL->parentID= newParID;

  string s= ItemID_Info(  aID );
  string n= ItemID_Info( &nID );
  DEBUG_DB( ac->fCB, MyDB,Da_MvI, "%s => %s err=%d", s.c_str(),n.c_str(), err );
  return err;
} /* MoveItem */



TSyError DeleteItem( CContext aContext, cItemID aID )
{
  ContextP ac= DBC( aContext );

  TDBItem* act;
  TSyError err= ac->fItemList.GetItem( aID, act ); if (err) return err; // must exist already
  if (ac->fCurrent==act) ListBack( ac->fCurrent, &ac->fItemList );

  err=      ac->fItemList.DeleteItem( aID );
  DEBUG_DB( ac->fCB, MyDB,Da_DeI, "%s err=%d", ItemID_Info( aID ).c_str(), err );
  return err;
} /* DeleteItem */



TSyError FinalizeLocalID( CContext /* aContext */, cItemID aID, ItemID updID )
{
  updID->item= StrAlloc( aID->item );
  return LOCERR_OK;
  
//return LOCERR_NOTIMP;
} /* FinalizeLocalID */



TSyError DeleteSyncSet( CContext aContext )
{
  TSyError      err;
  ContextP      ac= DBC( aContext );
  ItemID_Struct iID;
  appCharP      itemData;
  sInt32        status;
  
  while (true) {
        err= ReadNextItem( aContext, &iID, &itemData, &status, true ); 
    if (err || status==ReadNextItem_EOF) break;

        err= DeleteItem( aContext, &iID );
    DEBUG_DB( ac->fCB, MyDB,Da_DSS, "%s: err=%d\n", ItemID_Info( &iID ).c_str(), err );
    if (err) break;

    DisposeObj( aContext, iID.item   );
    DisposeObj( aContext, iID.parent );
    DisposeObj( aContext, itemData   );
  } // while

  return err;
} /* DeleteSyncSet */



/* -- BLOBs --------------------------------------------------------------------- */
#ifndef DISABLE_PLUGIN_BLOBS
TSyError ReadBlob ( CContext aContext, cItemID  aID,   cAppCharP  aBlobID,
                                    appPointer *aBlkPtr, memSize *aBlkSize,
                                                         memSize *aTotSize,
                                          bool  aFirst,     bool *aLast ) {
  return   DBC( aContext )->fBlob.ReadBlob    ( aID,              aBlobID,
                                                aBlkPtr,aBlkSize, aTotSize, aFirst,aLast );
} // ReadBlob

TSyError WriteBlob( CContext aContext, cItemID  aID,   cAppCharP  aBlobID,
                                     appPointer aBlkPtr, memSize  aBlkSize,
                                                         memSize  aTotSize,
                                           bool aFirst,     bool  aLast ) {
  return   DBC( aContext )->fBlob.WriteBlob   ( aID,              aBlobID,
                                                aBlkPtr,aBlkSize, aTotSize, aFirst,aLast );
} // WriteBlob;

TSyError DeleteBlob( CContext aContext, cItemID aID,   cAppCharP  aBlobID ) {
  return   DBC( aContext )->fBlob.DeleteBlob  ( aID,              aBlobID );
} // DeleteBlob
#endif
/* ------------------------------------------------------------------------------ */



/* If all operations for this datastore access are successful, the current
 * itemlist can be written to a text file and the <newToken> will be returned
 */
TSyError EndDataWrite( CContext aContext, bool success, appCharP *newToken )
{
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_EW, "=> %s", success ? "COMMIT":"ROLLBACK" );

  TDBItem*                                act= &ac->fNewItem;
  ac->fNewItem.CreateItem( ac->fNewID,"", act );

  TSyError     err= DB_Error;
                    ac->fNewItem.SaveDB ( false );
  if (success) err= ac->fItemList.SaveDB( false ); // be aware that this implementation
         // in case of no success might have saved already a part at "ThreadMyChangeNow"
  if (err) ac->fNewToken= ac->fLastToken;
  *newToken=    StrAlloc( ac->fNewToken.c_str() );

  DEBUG_DB( ac->fCB, MyDB,Da_EW, "err=%d newToken='%s'", err, *newToken );
  return err;
} /* EndDataWrite */
#endif // DISABLE_PLUGIN_DATASTOREDATA



/* ----- "script-like" ADAPT --------- */
TSyError AdaptItem( CContext aContext, appCharP *aItemData1, 
                                       appCharP *aItemData2,
                                       appCharP *aLocalVars, uInt32 aIdentifier )
{
  /**** CAN BE ADAPTED BY USER ****/
  // NOTE: will not yet be called by the SyncML engine
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,"AdaptItem", "'%s' '%s' '%s' id=%d",
                          *aItemData1,*aItemData2,*aLocalVars, aIdentifier );
  *aItemData1 = StrAlloc( *aItemData1 );
  *aItemData2 = StrAlloc( "4:just_a_test\n" );
  return LOCERR_OK;
} /* AdaptItem */



/* ----------------------------------- */
void DisposeObj( CContext aContext, void* memory )
{
  ContextP         ac= DBC( aContext );
  DEBUG_Exotic_DB( ac->fCB, MyDB,Da_DO, "%d free at %08X '%s'", ac, memory,memory );
  StrDispose     ( memory );
} /* DisposeObj */


TSyError DeleteContext( CContext aContext )
{
  ContextP ac= DBC( aContext );
  delete   ac; // release the structure itself
  return LOCERR_OK;
} /* DeleteContext */


} /* namespace */
/* eof */
