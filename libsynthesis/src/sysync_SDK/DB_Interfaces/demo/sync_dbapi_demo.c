/*  
 *  File:    sync_dbapi_demo.c
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  Programming interface between Synthesis SyncML engine
 *  and a database structure.
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 *
 *    E X A M P L E    C O D E
 *    ========================
 * (To be used as starting point for SDK development.
 *  It can be adapted and extended by programmer)
 *
 */

#include "sync_include.h"   /* include general SDK definitions */
#include "sync_dbapidef.h"  /* include the interface file and utilities */
#include "SDK_util.h"       /* include SDK utilities */

/* C/C++ support */
#ifdef __cplusplus  /* this allows to have more than one of these modules in C++ */
  namespace SDK_demodb {
    using sysync::SDK_Interface_Struct;
    using sysync::DB_Callback;
    using sysync::ItemID;
    using sysync::MapID;

    using sysync::LOCERR_OK;
    using sysync::LOCERR_NOTIMP;
    using sysync::DB_Full;
    using sysync::DB_NotFound;
    using sysync::DB_Forbidden;

    using sysync::ReadNextItem_Changed;
    using sysync::ReadNextItem_Unchanged;
    using sysync::ReadNextItem_EOF;

    using sysync::Password_ClrText_IN;
#endif

#include "sync_dbapi.h" /* include the interface file and utilities */

#define BuildNumber  0  /* User defined build number, can be 0..255 */
#define MyDB   "DemoDB" /* example debug name */
#define MY_ID       42  /* example datastore context */

#define STRLEN      80  /* Max length of local string copies */

#if defined MACOSX && defined __MWERKS__
#pragma export on
#endif


/* -- MODULE -------------------------------------------------------------------- */
/* this is an example, how a context could be structured */
/**** CAN BE ADAPTED BY USER ****/
#ifdef __cplusplus
  class ModuleContext {
    public:
      ModuleContext() { fCB= NULL; }

      DB_Callback fCB;                   /* callback structure */
      char        fModuleName[ STRLEN ]; /* the module's name  */
  };
#else
  typedef struct {
      DB_Callback fCB;                   /* callback structure */
      char        fModuleName[ STRLEN ]; /* the module's name  */
  } ModuleContext;
#endif


/* <mContext> will be casted to the ModuleContext* structure */
static ModuleContext* MoC( CContext mContext ) { return (ModuleContext*)mContext; }



TSyError Module_CreateContext( CContext *mContext, cAppCharP   moduleName,
                                                   cAppCharP      subName,
                                                   cAppCharP mContextName,
                                                 DB_Callback mCB )
{
  cAppCharP sep= "";

  ModuleContext* mc;

  #ifdef __cplusplus
    mc= new ModuleContext;
  #else
    mc=    (ModuleContext*)malloc( sizeof(ModuleContext) );
  #endif

  if (mc==NULL) return DB_Full;

  if (subName!=NULL &&
     *subName!='\0') sep= "!"; /* Notation: <moduleName>"!"<subName> */

  strncpy ( mc->fModuleName, moduleName, STRLEN );
            mc->fCB        = mCB;
  DEBUG_DB( mc->fCB, MyDB,Mo_CC, "'%s%s%s' (%s)", mc->fModuleName,sep,subName,
                                                     mContextName );

  *mContext= (CContext)mc; /* return the created context structure */
  return LOCERR_OK;
} /* Module_CreateContext */



/* Get the plug-in's version number */
CVersion Module_Version( CContext mContext )
{
  CVersion v= Plugin_Version( BuildNumber ); /* The current plugin's SDK version is expected here */

  if             ( mContext )
    DEBUG_DB( MoC( mContext )->fCB, MyDB,Mo_Ve, "%08X", v );

  return v;
} /* Module_Version */



/* Get the plug-in's capabilities */
TSyError Module_Capabilities( CContext mContext, appCharP *mCapabilities )
{
  char     s[ 256 ]; /* copy it into a local string */
  char* p= s;
  
  /* return value of sprintf contains the total string length: increment will concatenate */
  p+= sprintf( p,    "%s\n",     MyPlatform() );
  p+= sprintf( p,    "%s\n",     DLL_Info     );
  p+= sprintf( p, "%s:%s\n",  CA_MinVersion,   "V1.0.6.0"            ); /* must not be changed */
  p+= sprintf( p, "%s:%s\n",  CA_Manufacturer, "Synthesis AG"        ); /**** SHOULD BE ADAPTED BY USER ****/
  p+= sprintf( p, "%s:%s",    CA_Description,  "Demo Example Module" ); /**** SHOULD BE ADAPTED BY USER ****/
  
  *mCapabilities= StrAlloc( s );

  DEBUG_DB( MoC( mContext )->fCB, MyDB,Mo_Ca, "'%s'", *mCapabilities );
  return LOCERR_OK;
} /* Module_Capabilities */



TSyError Module_PluginParams( CContext mContext, cAppCharP mConfigParams, CVersion engineVersion )
{
  ModuleContext* mc= MoC( mContext );
  DEBUG_DB     ( mc->fCB, MyDB,Mo_PP, " Engine=%08X", engineVersion );
  DEBUG_DB     ( mc->fCB, MyDB,Mo_PP, "'%s'",         mConfigParams );

/*return LOCERR_CFGPARSE;*/ /* if there are unsupported params */
  return LOCERR_OK;
} /* Module_PluginParams */



/* Dispose the memory of the module context */
void Module_DisposeObj( CContext mContext, void* memory )
{
  DEBUG_Exotic_DB( MoC( mContext )->fCB, MyDB,Mo_DO, "free at %08X '%s'", memory,memory );
  StrDispose( memory );
} /* Module_DisposeObj */



TSyError Module_DeleteContext( CContext mContext )
{
  ModuleContext* mc= MoC( mContext );
  DEBUG_DB     ( mc->fCB, MyDB,Mo_DC, "'%s'", mc->fModuleName );

  #ifdef __cplusplus
    delete mc;
  #else
    free ( mc );
  #endif

  return LOCERR_OK;
} /* Module_DeleteContext */




/* ---------------------- session handling --------------------- */
/* this is an example, how a context could be structured */
/**** CAN BE ADAPTED BY USER ****/
#ifdef __cplusplus
  class SessionContext {
    public:
      SessionContext() { fCB= NULL; }

      int         fID; /* a reference number. */
      DB_Callback fCB; /* callback structure  */
      int      fPMode; /* The login password mode */
  };
#else
  typedef struct {
      int         fID; /* a reference number. */
      DB_Callback fCB; /* callback structure  */
      int      fPMode; /* The login password mode */
  } SessionContext;
#endif



/* <sContext> will be casted to the SessionContext* structure */
static SessionContext* SeC( CContext sContext ) { return (SessionContext*)sContext; }



/* Create a context for a new session */
TSyError Session_CreateContext( CContext *sContext, cAppCharP sessionName, DB_Callback sCB )
{
  SessionContext* sc;

/*return DB_Error;*/ /* added for test */

  #ifdef __cplusplus
    sc= new SessionContext;
  #else
    sc=    (SessionContext*)malloc( sizeof(SessionContext) );
  #endif

  if (sc==NULL) return DB_Full;

  /**** CAN BE ADAPTED BY USER ****/
            sc->fID= 333; /* as an example */
            sc->fCB= sCB;
            sc->fPMode= Password_ClrText_IN;        /* take this mode ... */
         /* sc->fPMode= Password_ClrText_OUT; */    /* ... or this        */
  DEBUG_DB( sc->fCB, MyDB,Se_CC, "%d '%s'", sc->fID,sessionName );

  *sContext= (CContext)sc; /* return the created context structure */
  return LOCERR_OK;
} /* Session_CreateContext */



/* ----- "script-like" ADAPT --------- */
TSyError Session_AdaptItem( CContext sContext, appCharP *sItemData1,
                                               appCharP *sItemData2,
                                               appCharP *sLocalVars,
                                                 uInt32  sIdentifier )
{
  /**** CAN BE ADAPTED BY USER ****/
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,"Session_AdaptItem", "'%s' '%s' '%s' id=%d",
                                *sItemData1,*sItemData2,*sLocalVars, sIdentifier );
  return LOCERR_OK;
} /* Session_AdaptItem */



/* Check the database entry of <deviceID> and return its nonce string */
TSyError Session_CheckDevice( CContext sContext, cAppCharP aDeviceID, appCharP *sDevKey,
                                                                      appCharP *nonce )
{
  /**** CAN BE ADAPTED BY USER ****/
  SessionContext* sc= SeC( sContext );

  *sDevKey= StrAlloc( aDeviceID  );
  *nonce  = StrAlloc( "xyz_last" );
  DEBUG_DB( sc->fCB, MyDB,Se_CD, "%d dev='%s' nonce='%s'", sc->fID, *sDevKey,*nonce );
  return LOCERR_OK;
} /* Session_CheckDevice */



/* Get a new nonce from the database. If this returns an error, the SyncML engine
 * will create its own nonce.
 */
TSyError Session_GetNonce( CContext sContext, appCharP *nonce )
{
  /**** CAN BE ADAPTED BY USER ****/
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,Se_GN, "%d (not supported)", sc->fID );
  *nonce= NULL;
  return DB_NotFound;
} /* Session_GetNonce */



/* Save the new nonce (which will be expected to be returned
 * in the next session for this device
 */
TSyError Session_SaveNonce( CContext sContext, cAppCharP nonce )
{
  /**** CAN BE ADAPTED BY USER ****/
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,Se_SN, "%d nonce='%s'", sc->fID, nonce );
  return LOCERR_OK;
} /* Session_SaveNonce */



/* Save the device info of <sContext> */
TSyError Session_SaveDeviceInfo( CContext sContext, cAppCharP aDeviceInfo )
{
  /**** CAN BE ADAPTED BY USER ****/
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,Se_SD, "%d info='%s'", sc->fID, aDeviceInfo );
  return LOCERR_OK;
} /* Session_SaveDeviceInfo */



/* Get the plugin's DB time */
TSyError Session_GetDBTime( CContext sContext, appCharP *currentDBTime )
{
  /**** CAN BE ADAPTED BY USER ****/
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,Se_GT, "%d", sc->fID );
  *currentDBTime= NULL;
  return DB_NotFound;
} /* Session_GetDBTime */



/* Return: Password_ClrText_IN    'SessionLogin' will get    clear text password
 *         Password_ClrText_OUT         "        must return clear text password
 *         Password_MD5_OUT             "        must return MD5 coded  password
 *         Password_MD5_Nonce_IN        "        will get    MD5B64(MD5B64(user:pwd):nonce)
 */
sInt32 Session_PasswordMode( CContext sContext )
{
  /**** CAN BE ADAPTED BY USER ****/
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,Se_PM, "%d mode=%d", sc->fID, sc->fPMode );
  return          sc->fPMode;
} /* Session_PasswordMode */



/* Make login */
TSyError Session_Login( CContext sContext, cAppCharP sUsername, appCharP *sPassword,
                                                                appCharP *sUsrKey )
{
  /**** CAN BE ADAPTED BY USER ****/
  SessionContext* sc= SeC( sContext );
  TSyError       err= DB_Forbidden; /* default */

  /* different modes, choose one for the plugin */
  if (sc->fPMode==Password_ClrText_IN) {
    if (strcmp(  sUsername,"super" )==0 &&
        strcmp( *sPassword,"user"  )==0) { *sUsrKey  = StrAlloc( "1234" ); err= LOCERR_OK; }
  }
  else {       /* Password will be returned */
    if (strcmp(  sUsername,"super" )==0) { *sPassword= StrAlloc( "user" );
                                           *sUsrKey  = StrAlloc( "1234" ); err= LOCERR_OK; }
  } /* if */

  if (err) { DEBUG_DB( sc->fCB, MyDB,Se_LI, "%d usr='%s' err=%d",
                                             sc->fID,sUsername, err ); }
  else       DEBUG_DB( sc->fCB, MyDB,Se_LI, "%d usr='%s' pwd='%s' => key='%s'",
                                             sc->fID,sUsername,*sPassword, *sUsrKey );
  return err;
} /* Session_Login */



/* Make logout */
TSyError Session_Logout( CContext sContext )
{
  /**** CAN BE ADAPTED BY USER ****/
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,Se_LO, "%d",sc->fID );
  return LOCERR_OK;
} /* Session_Logout */



void Session_DisposeObj( CContext sContext, void* memory )
{
  /**** CAN BE ADAPTED BY USER ****/
  SessionContext*  sc= SeC( sContext );
  DEBUG_Exotic_DB( sc->fCB, MyDB,Se_DO, "%d free at %08X '%s'",
                   sc->fID, memory,memory );
  StrDispose              ( memory );
} /* Session_DisposeObj */



/* Can be implemented empty, if no action is required */
void Session_ThreadMayChangeNow( CContext sContext )
{
  /**** CAN BE ADAPTED BY USER ****/
  SessionContext*  sc= SeC( sContext );
  DEBUG_Exotic_DB( sc->fCB, MyDB,Se_TC, "%d",sc->fID );
} /* Session_ThreadMayChangeNow */



/* This routine is implemented for debug purposes only and will NOT BE CALLED by the
 * SyncML engine. Can be implemented empty, if not needed
 */
void Session_DispItems( CContext sContext, bool allFields, cAppCharP specificItem )
{
  /**** CAN BE ADAPTED BY USER ****/
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,Se_DI, "%d %d '%s'",
                  sc->fID, allFields,specificItem );
} /* Session_DispItems */



/* Delete a session context */
TSyError Session_DeleteContext( CContext sContext )
{
  /**** CAN BE ADAPTED BY USER ****/
  SessionContext* sc= SeC( sContext );
  DEBUG_DB      ( sc->fCB, MyDB,Se_DC, "%d",sc->fID );

  #ifdef __cplusplus
    delete sc;
  #else
    free ( sc );
  #endif

  return LOCERR_OK;
} /* Session_DeleteContext */




/* ----------------------------------------------------------------- */
/* This is an example, how a context could be structured */
/**** CAN BE ADAPTED BY USER ****/
#ifdef __cplusplus
  class TDBContext {
    public:
      TDBContext() { fCB= NULL; }

      DB_Callback fCB; /* debug logging callback */

      int contextID;   /* context identifier */
      int nthItem;     /* for 'ReadNextItem' */

      char fDevKey[ STRLEN ];
      char fUsrKey[ STRLEN ];
  };
#else
  typedef struct {
      DB_Callback fCB; /* debug logging callback */

      int contextID;   /* context identifier */
      int nthItem;     /* for 'ReadNextItem' */

      char fDevKey[ STRLEN ];
      char fUsrKey[ STRLEN ];
  } TDBContext;
#endif

typedef TDBContext* ContextP;


/* <aContext> will be casted to the ContextP structure */
static ContextP DBC( CContext aContext ) { return (ContextP)aContext; }



/* -- OPEN ----------------------------------------------------------------------- */
TSyError CreateContext( CContext *aContext, cAppCharP aContextName, DB_Callback aCB,
                                            cAppCharP sDevKey,
                                            cAppCharP sUsrKey )
{
  ContextP ac;

  #ifdef __cplusplus
    ac= new TDBContext;
  #else
    ac= (ContextP)malloc( sizeof(TDBContext) );
  #endif

  if (ac==NULL) return DB_Full;

  /**** CAN BE ADAPTED BY USER ****/
            ac->fCB      = aCB;             /* debug logging callback */
            ac->contextID= MY_ID;           /* example context */
            ac->nthItem  = 0;               /* reset counter */
  strncpy ( ac->fDevKey, sDevKey, STRLEN ); /* local copies */
  strncpy ( ac->fUsrKey, sUsrKey, STRLEN );

  DEBUG_DB( ac->fCB, MyDB,Da_CC, "%d '%s' dev='%s' usr='%s'",
                          ac->contextID, aContextName, ac->fDevKey, ac->fUsrKey );

  *aContext= (CContext)ac; /* return the created context structure */
  return LOCERR_OK;
} /* CreateContext */



uInt32 ContextSupport( CContext aContext, cAppCharP aContextRules )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_CS, "%d '%s'", ac->contextID,aContextRules );
  return 0;
} /* ContextSupport */



uInt32 FilterSupport( CContext aContext, cAppCharP aFilterRules )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_FS, "%d '%s'", ac->contextID,aFilterRules );
  return 0;
} /* FilterSupport */



/* -- ADMINISTRATION ------------------------------------------------------------ */
TSyError LoadAdminData( CContext aContext, cAppCharP aLocDB,
                                           cAppCharP aRemDB, appCharP *adminData )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_LA, "%d '%s' '%s' '%s' '%s'",
                                  ac->contextID, ac->fDevKey, ac->fUsrKey, aLocDB, aRemDB );
  *adminData= NULL;
  return DB_Forbidden; /* not yet implemented */
} /* LoadAdminData */



TSyError LoadAdminDataAsKey( CContext aContext, cAppCharP aLocDB,
                                                cAppCharP aRemDB, KeyH adminKey )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_LA, "%d '%s' '%s' '%s' '%s' adminKey=%08X",
                                  ac->contextID, ac->fDevKey, ac->fUsrKey, aLocDB, aRemDB, adminKey );
  return DB_Forbidden; /* not yet implemented */
} /* LoadAdminDataAsKey */



TSyError SaveAdminData( CContext aContext, cAppCharP adminData )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_SA, "%d '%s'", ac->contextID, adminData );
  return DB_Forbidden; /* not yet implemented */
} /* SaveAdminData */



TSyError SaveAdminDataAsKey( CContext aContext, KeyH adminKey )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_SA, "%d adminKey=%08X", ac->contextID, adminKey );
  return DB_Forbidden; /* not yet implemented */
} /* SaveAdminDataAsKey */



bool ReadNextMapItem( CContext aContext, MapID mID, bool aFirst )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB, Da_RM, "%d %08X first=%d (EOF)", ac->contextID, mID, aFirst );
  return false; /* not yet implemented */
} /* ReadNextMapItem */



TSyError InsertMapItem( CContext aContext, cMapID mID )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_IM, "%d %08X: '%s' '%s' %04X %d",
                                  ac->contextID, mID, mID->localID, mID->remoteID, mID->flags, mID->ident );
  return DB_Forbidden; /* not yet implemented */
} /* InsertMapItem */



TSyError UpdateMapItem( CContext aContext, cMapID mID )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_UM, "%d %08X: '%s' '%s' %04X %d",
                                  ac->contextID, mID, mID->localID, mID->remoteID, mID->flags, mID->ident );
  return DB_Forbidden; /* not yet implemented */
} /* UpdateMapItem */



TSyError DeleteMapItem( CContext aContext, cMapID mID )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_DM, "%d %08X: '%s' '%s' %04X %d",
                                  ac->contextID, mID, mID->localID, mID->remoteID, mID->flags, mID->ident );
  return DB_Forbidden; /* not yet implemented */
} /* DeleteMapItem */




/* -- GENERAL -------------------------------------------------------------------- */
void DisposeObj( CContext aContext, void* memory )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP         ac= DBC( aContext );
  DEBUG_Exotic_DB( ac->fCB, MyDB,Da_DO, "%d free at %08X", ac->contextID,memory );
  free( memory );
} /* DisposeObj */



void ThreadMayChangeNow( CContext aContext )
{
  /**** CAN BE ADAPTED BY USER ****/
  /* can be implemented empty, if no action is required */
  ContextP         ac= DBC( aContext );
  DEBUG_Exotic_DB( ac->fCB, MyDB,Da_TC, "%d", ac->contextID );
} /* ThreadMayChangeNow */



void WriteLogData( CContext aContext, cAppCharP logData )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB, Da_WL, "%d (BEGIN)\n%s", ac->contextID, logData );
  DEBUG_DB( ac->fCB, MyDB, Da_WL, "%d (END)",       ac->contextID );
} /* WriteLogData */



/* This routine is implemented for debug purposes only and will NOT BE CALLED by the
 * SyncML engine. Can be implemented empty, if not needed
 */
void DispItems( CContext aContext, bool allFields, cAppCharP specificItem )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_DI, "%d %d '%s'", ac->contextID,allFields,specificItem );
} /* DispItems */



/* ----- "script-like" ADAPT --------- */
TSyError AdaptItem( CContext aContext, appCharP *aItemData1,
                                       appCharP *aItemData2,
                                       appCharP *aLocalVars,
                                         uInt32  aIdentifier )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,"AdaptItem", "'%s' '%s' '%s' id=%d",
                          *aItemData1,*aItemData2,*aLocalVars, aIdentifier );
  return LOCERR_OK;
} /* AdaptItem */



/* -- READ ---------------------------------------------------------------------- */
TSyError StartDataRead( CContext aContext, cAppCharP   lastToken,
                                           cAppCharP resumeToken )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_SR, "%d last='%s' resume='%s'",
                      ac->contextID, lastToken,resumeToken );
  ac->nthItem= 0; /* reset counter */
  return LOCERR_OK;
} /* StartDataRead */



TSyError ReadNextItem( CContext aContext, ItemID aID, appCharP *aItemData, sInt32 *aStatus, bool aFirst )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP    ac= DBC( aContext );
  if (aFirst) ac->nthItem= 0;

  /* Show all visible items to the SyncML engine here */
  /* "ReadNextItem" will be called subsequently until ReadNextItem_EOF is returned */
  if (ac->nthItem>0) {
    *aStatus= ReadNextItem_EOF;
    DEBUG_DB( ac->fCB, MyDB,Da_RN, "%d aStatus=%d", ac->contextID, *aStatus );
    return LOCERR_OK;
  } /* if */

  /* This example just shows one hard coded element */
  aID->item  = StrAlloc( "demo_ID"     );
  aID->parent= StrAlloc( "demo_parent" );
  *aItemData = StrAlloc( "demo_data"   );
  *aStatus   = ReadNextItem_Changed; /* comparison not implemented here */

  ac->nthItem++;
  DEBUG_DB( ac->fCB, MyDB,Da_RN, "%d aStatus=%d aItemData='%s' aID=(%s,%s)",
                                  ac->contextID, *aStatus, *aItemData, aID->item,aID->parent );
  return LOCERR_OK;
} /* ReadNextItem */


TSyError ReadNextItemAsKey( CContext aContext, ItemID aID, KeyH aItemKey,
                                                           sInt32*    aStatus, bool aFirst )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP    ac= DBC( aContext );
  if (aFirst) ac->nthItem= 0;

  /* Show all visible items to the SyncML engine here */
  /* "ReadNextItemAsKey" will be called subsequently until ReadNextItem_EOF is returned */
  if (ac->nthItem>0) {
    *aStatus= ReadNextItem_EOF;
    DEBUG_DB( ac->fCB, MyDB,Da_RNK, "%d aStatus=%d", ac->contextID, *aStatus );
    return LOCERR_OK;
  } /* if */

  /* This example just shows one hard coded element */
  aID->item  = StrAlloc( "demo_ID"     );
  aID->parent= StrAlloc( "demo_parent" );
  *aStatus   = ReadNextItem_Changed; /* comparison not implemented here */

  ac->nthItem++;
  DEBUG_DB( ac->fCB, MyDB,Da_RNK, "%d aStatus=%d aItemKey=%08X aID=(%s,%s)",
                                   ac->contextID, *aStatus, aItemKey, aID->item,aID->parent );
  return LOCERR_OK;
} /* ReadNextItemAsKey */



TSyError ReadItem( CContext aContext, cItemID aID, appCharP *aItemData )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );

  *aItemData= StrAlloc( "demo_data" );
  DEBUG_DB( ac->fCB, MyDB,Da_RI,  "%d aItemData='%s' aID=(%s,%s)",
                                   ac->contextID,*aItemData, aID->item,aID->parent );
  return LOCERR_OK;
} /* ReadItem */



TSyError ReadItemAsKey( CContext aContext, cItemID aID, KeyH aItemKey )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_RIK, "%d aItemKey=%08X aID=(%s,%s)",
                                   ac->contextID, aItemKey,  aID->item,aID->parent );
  return LOCERR_OK;
} /* ReadItemAsKey */



TSyError ReadBlob( CContext aContext, cItemID  aID,   cAppCharP  aBlobID,
                                   appPointer *aBlkPtr, memSize *aBlkSize,
                                                        memSize *aTotSize,
                                         bool  aFirst,     bool *aLast )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  
  const memSize sz= sizeof(int);
  int* ip = (int*)malloc( sz ); /* example BLOB structure for test (=4 bytes) */
      *ip = 231;

  *aBlkPtr = (appPointer)ip; if (*aBlkSize==0 || *aBlkSize>=sz) *aBlkSize= sz;
  *aTotSize= *aBlkSize;
  *aLast   = true;

  DEBUG_DB( ac->fCB, MyDB,Da_RB, "%d aID=(%s,%s) aBlobID=(%s)",
                                  ac->contextID, aID->item,aID->parent, aBlobID );
  DEBUG_DB( ac->fCB, MyDB,"",    "aBlkPtr=%08X aBlkSize=%d aTotSize=%d aFirst=%s aLast=%s",
                                 *aBlkPtr, *aBlkSize, *aTotSize,
                                  aFirst?"true":"false", *aLast?"true":"false" );
  return LOCERR_OK;
} /* ReadBlob */



TSyError EndDataRead( CContext aContext )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_ER, "%d", ac->contextID );
  return LOCERR_OK;
} /* EndDataRead */




/* -- WRITE --------------------------------------------------------------------- */
TSyError StartDataWrite( CContext aContext )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_SW, "%d", ac->contextID );
  return LOCERR_OK;
} /* StartDataWrite */



TSyError InsertItem( CContext aContext, cAppCharP aItemData, ItemID newID )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  newID->item = StrAlloc( "hello" );
  DEBUG_DB( ac->fCB, MyDB,Da_II, "%d '%s'\nnewID=(%s,%s)",
                                  ac->contextID, aItemData, newID->item,newID->parent );
  return LOCERR_OK;
} /* InsertItem */


TSyError InsertItemAsKey( CContext aContext, KeyH aItemKey, ItemID newID )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_IIK, "%d %08X\n", ac->contextID, aItemKey );
  newID= NULL;
  return LOCERR_NOTIMP;
} /* InsertItemAsKey */



TSyError UpdateItem( CContext aContext, cAppCharP aItemData, cItemID   aID,
                                                              ItemID updID )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  if (strcmp( aID->item,"example" )!=0) updID->item = StrAlloc( "example" );

  DEBUG_DB( ac->fCB, MyDB,Da_UI, "%d '%s'\naID=(%s,%s)",
                                  ac->contextID, aItemData, aID->item,aID->parent );

  if (updID->item==NULL) { DEBUG_DB( ac->fCB, MyDB,"", "NULL" ); }
  else                   { DEBUG_DB( ac->fCB, MyDB,"", "updID=(%s,%s)",
                                                        updID->item,updID->parent ); }

  return LOCERR_OK;
} /* UpdateItem */


TSyError UpdateItemAsKey( CContext aContext, KeyH aItemKey, cItemID   aID,
                                                             ItemID updID )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_UIK, "%d %08X\naID=(%s,%s)",
                                   ac->contextID, aItemKey, aID->item,aID->parent );
  updID= NULL;
  return LOCERR_NOTIMP;
} /* UpdateItemAsKey */



TSyError MoveItem( CContext aContext, cItemID aID, cAppCharP newParID )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_MvI, "%d aID=(%s,%s) => (%s,%s)",
                                   ac->contextID, aID->item,aID->parent,
                                                  aID->item,newParID );
  return LOCERR_OK;
} /* MoveItem */



TSyError DeleteItem( CContext aContext, cItemID aID )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_DeI, "%d aID=(%s,%s)", ac->contextID,aID->item,aID->parent );
  return LOCERR_OK;
} /* DeleteItem */



TSyError FinalizeLocalID( CContext aContext, cItemID aID, ItemID updID )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_FLI, "%d aID=(%s,%s)", ac->contextID,aID->item,aID->parent );
  updID= NULL;
  return LOCERR_NOTIMP;
} /* FinalizeLocalID */



TSyError DeleteSyncSet( CContext aContext )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_DSS, "%d", ac->contextID );
  return LOCERR_NOTIMP;
} /* DeleteSyncSet */



TSyError WriteBlob( CContext aContext, cItemID aID,   cAppCharP aBlobID,
                                    appPointer aBlkPtr, memSize aBlkSize,
                                                        memSize aTotSize,
                                          bool aFirst,     bool aLast )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_WB, "%d aID=(%s,%s) aBlobID=(%s)",
                                  ac->contextID, aID->item,aID->parent, aBlobID );
  DEBUG_DB( ac->fCB, MyDB,"",    "aBlkPtr=%08X aBlkSize=%d aTotSize=%d aFirst=%s aLast=%s",
                                  aBlkPtr, aBlkSize, aTotSize,
                                  aFirst?"true":"false", aLast ?"true":"false" );
  return LOCERR_OK;
} /* WriteBlob */



TSyError DeleteBlob( CContext aContext, cItemID aID, cAppCharP aBlobID )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_DB, "%d aID=(%s,%s) aBlobID=(%s)",
                                  ac->contextID, aID->item,aID->parent, aBlobID );
  return LOCERR_OK;
} /* DeleteBlob */



TSyError EndDataWrite( CContext aContext, bool success, appCharP *newToken )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP ac= DBC( aContext );

  #define TokenExample "20041110T230000Z"
  *newToken= StrAlloc( TokenExample );

  DEBUG_DB( ac->fCB, MyDB,Da_EW, "%d %s '%s'",
                          ac->contextID, success ? "COMMIT":"ROLLBACK", *newToken );
  return LOCERR_OK;
} /* EndDataWrite */



/* ----------------------------------- */
TSyError DeleteContext( CContext aContext )
{
  /**** CAN BE ADAPTED BY USER ****/
  ContextP  ac= DBC( aContext );
  DEBUG_DB( ac->fCB, MyDB,Da_DC, "%d", ac->contextID );

  #ifdef __cplusplus
    delete ac;
  #else
    free ( ac ); /* release the structure itself */
  #endif

  return LOCERR_OK;
} /* DeleteContext */



/* C/C++ support */
#ifdef __cplusplus
} /* namespace */
#endif
/* eof */
