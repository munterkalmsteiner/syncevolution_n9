/*
 *  File:     SDK_util.c
 *
 *  Authors:  Beat Forster
 *
 *
 *  SDK utility functions for
 *   - version handling
 *   - string alloc/dispose
 *   - debug/callback
 *
 *  written in Std C, can be used in C++ as well
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 */


#include "sync_dbapidef.h"
#include "SDK_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef __MACH__
#include <malloc.h>
#endif

#ifdef ANDROID
#include "android/log.h"
#endif

#define MyDB "SDK"                    /* local debug name */
#define Old_SDKversionMask 0xffff00ff /* Old mask for version comparison: Omit OS identifier */
#define maxmsglen 1024                /* Maximum string length for callback string */



/* This StdC module is independent from namespace "sysync" */

/* Get the plug-in's version/subversion number
 * The plugin will contain a user defined <buildNumber> 0..255
 * This version number is defined by Synthesis and should not be changed:
 * The engine contains also a version number, and makes some comparisons.
 */
CVersion Plugin_Version( short buildNumber )
{
  #define P 256
  long    v;

  #define SDK_VERSION_MAJOR 1 /* Release: V1.9.1, change this if you need troubles */
  #define SDK_VERSION_MINOR 9
  #define SDK_SUBVERSION    1

  /* allowed range for the local build number */
  if (buildNumber<  0) buildNumber=   0;
  if (buildNumber>255) buildNumber= 255;

  v= ((SDK_VERSION_MAJOR *P +
       SDK_VERSION_MINOR)*P +
       SDK_SUBVERSION   )*P +
       buildNumber;

  return v;
} /* Plugin_Version */



/* Check, if <version_feature> is supported in <currentVersion>
 * NOTE: For the SyncML engine internally everything is supported
 *       This will be reflected with the fact that for 2.1.1.X
 *       the engine's version was always much much higher.
 *       Today the engine's version is equivalent to the SDK version.
 */
bool Feature_Supported  ( CVersion versionFeature, CVersion currentVersion )
{
  CVersion v= currentVersion;
  if      (v<VP_NewBuildNumber) v= v & Old_SDKversionMask; /* avoid OS identifier comparison */
  return   v>=versionFeature;
} /* FeatureSupported */


/* Check, if <version_feature> is equivalent to <currentVersion>
 */
bool Feature_SupportedEq( CVersion versionFeature, CVersion currentVersion )
{
  CVersion v= currentVersion;
  if      (v<VP_NewBuildNumber) v= v & Old_SDKversionMask; /* avoid OS identifier comparison */
  return   v==versionFeature;
} /* FeatureSupportedEq */



/* Get platform identifier */
cAppCharP MyPlatform( void )
{
  cAppCharP p;

  #if defined  MACOSX
    #if defined IPHONEOS
      p= "PLATFORM:iPhone"; /* there is ONLY XCode, no need to mention that here */
    #elif defined __MWERKS__
      p= "PLATFORM:Mac (CodeWarrior)";
    #elif defined __GNUC__
      p= "PLATFORM:Mac (XCode)";
    #else
      p= "PLATFORM:Mac";
    #endif

  #elif defined _WIN32
    #ifdef __MWERKS__
      p= "PLATFORM:Windows (CodeWarrior)";
    #elif defined _MSC_VER
      p= "PLATFORM:Windows (VisualStudio)";
    #else
      p= "PLATFORM:Windows";
    #endif

  #elif defined  LINUX
    #ifdef ANDROID
      p= "PLATFORM:Android";
    #else
      p= "PLATFORM:Linux";
    #endif

/* elif **** for JAVA defined at the Java code ******** */
/*  p= "PLATFORM:Java" */

/* elif **** for .net defined at the C#   code ******** */
/*  p= "PLATFORM:C#"   */

  #else
    p= "PLATFORM:unknown";
  #endif

  return p;
} /* MyPlatform */



/* -------------------- string allocation/deallocation -------- */
/* Allocate local memory for a string */
appCharP StrAllocN( cAppCharP s, int n, bool fullSize )
{
  char* cp;
  int   len;

  if (!fullSize) {
          len= strlen( s );
    if (n>len) n= len; /* never more than the length of <s> */
  } /* if */

           cp= (char*)malloc( n+1 );
  strncpy( cp, s, n ); /* not yet NUL terminated !! */
           cp[ n ]= '\0';
  return   cp;
} /* StrAllocN */


/* Allocate local memory for a string */
appCharP StrAlloc ( cAppCharP  s ) {
  return StrAllocN( s, strlen( s ), true );
} /* StrAlloc */


/* !Dispose a string which has been allocated with 'StrAlloc' */
void StrDispose( void* s ) {
  if (s!=NULL)   free( s );
} /* StrDispose */



/* ------------------ field operations ------------------------- */
bool Field( cAppCharP item, cAppCharP key, char** field )
{
  char* b;
  char* e;
  char* t= (char*)item;

  *field= NULL;
  while (true) {
    b= strstr( t,key ); if (!b) break; /* <key> available ? */
            e= strstr( b,"\r" );
    if (!e) e= strstr( b,"\n" ); /* get the end of this field */

    if (b==item || *(b-1)=='\r'
                || *(b-1)=='\n') {
      b+= strlen( key )+1;

      if (*(b-1)==':') {                         /* correctly separated ? */
        if (!e) *field= StrAlloc ( b );             /* either the rest    */
        else    *field= StrAllocN( b, e-b, false ); /* or till next field */
        return true;
      } /* if */

      if (!e) break;
    } /* if */

    t= e+1; /* go to the next field */
  } /* loop */

  return false;
} /* Field */



bool SameField( cAppCharP item, cAppCharP key, cAppCharP field )
{
  char* vv;
  bool  ok= Field( item, key, &vv );   /* get the <key> field */
  if   (ok) {
    ok= strcmp( field,vv )==0; /* and compare it with <field> */
    StrDispose      ( vv );     /* <vv> is no longer used now */
  } /* if */

  return ok;
} /* SameField */



/* ------------------ callback system -------------------------------------- */
/* Initialize to safe defaults, useable for Std C as well
 * NOTE: The current = latest available <callbackVersion> will be taken
 */
static bool BadCB( void* aCallbackRef )
{
  DB_Callback cb= aCallbackRef;

  if (cb==NULL) {
    printf( "bad cb==NULL\n" );    return true;
  } /* if */

  if (cb->callbackRef!=cb) {
    printf( "bad callbackRef %08lX <> %08lX\n",
               (unsigned long)cb->callbackRef,
               (unsigned long)cb ); return true;
  } /* if */

  return false; /* false= it's ok */
} /* BadCB */


void NBlk( void* aCallbackRef )
{
  int  i;
  DB_Callback cb= aCallbackRef;
  if (!CB_OK( cb,2 )) return;
/*if     ( BadCB( aCallbackRef )) return;*/

  for (i=0; i<cb->lCount; i++) printf( "  " );
} /* NBlk */



static void BeginBlk( void* aCallbackRef, cAppCharP aTag,
                                          cAppCharP aDesc,
                                          cAppCharP aAttrText )
{
  DB_Callback cb= aCallbackRef;
  if     ( BadCB( aCallbackRef )) return;

  NBlk( cb ); printf( "<%s> %s %s\n", aTag, aDesc, aAttrText );
        cb->lCount++;
} /* BeginBlk */



static void EndBlk( void* aCallbackRef, cAppCharP aTag )
{
  DB_Callback cb= aCallbackRef;
  if (!CB_OK( cb,2 )) return;
/*if     ( BadCB( aCallbackRef )) return;*/

        cb->lCount--;
  NBlk( cb ); printf( "</%s>\n", aTag );
} /* EndBlk */



static void EndThread( void* aCallbackRef )
{
  DB_Callback cb= aCallbackRef;
  if (!CB_OK( cb,2 )) return;
/*if     ( BadCB( aCallbackRef )) return;*/

  NBlk( cb ); printf( "=EndThread=\n" );
} /* EndThread */



/* -------------------------------------------------------------------- */
void InitCallback( void* aCB, uInt16 aVersion, void* aRoutine, void* aExoticRoutine )
{
  DB_Callback cb= aCB;
  if        (!cb) return;

  cb->callbackVersion= aVersion;
  cb->callbackRef    = cb;                 /* the callback pointer is the cb itself here */

  if (CB_OK( cb,1 )) {
    cb->debugFlags   = 0;                  /* debug disable so far */
    cb->DB_DebugPuts = aRoutine;
    if                (aRoutine) {
      if        (aExoticRoutine) cb->debugFlags= DBG_PLUGIN_ALL;
      else                       cb->debugFlags= DBG_PLUGIN_INT + DBG_PLUGIN_DB;
    } /* if */

    cb->cContext= 0;                       /* contexts */
    cb->mContext= 0;
    cb->sContext= 0;
  } /* if */

  if (CB_OK( cb,2 )) {
    cb->DB_DebugBlock    = BeginBlk;       /* level 2 */
    cb->DB_DebugEndBlock = EndBlk;
    cb->DB_DebugEndThread= EndThread;
    cb->lCount           = 0;
  } /* if */

  if (CB_OK( cb,3 ))
    cb->DB_DebugExotic  = aExoticRoutine;  /* level 3 */

  if (CB_OK( cb,4 ))
    cb->allow_DLL_legacy= 0; /* false */   /* level 4 */

  if (CB_OK( cb,5 ))
    cb->allow_DLL       = 0; /* false */   /* level 5 */

  if (CB_OK( cb,6 )) {
    cb->reserved1= 0;                      /* level 6 */
    cb->reserved2= 0;
    cb->reserved3= 0;
    cb->reserved4= 0;
  } /* if */

  if (CB_OK( cb,7 )) {
    cb->thisCB  = cb;                      /* level 7 */
    cb->logCount=  0;
  } /* if */

  if (CB_OK( cb,8 )) {
    cb->thisBase            = NULL;        /* level 8 */
    cb->jRef                = NULL;
    cb->gContext            = 0;

    cb->ui.SetStringMode    = NULL;
    cb->ui.InitEngineXML    = NULL;
    cb->ui.InitEngineFile   = NULL;
    cb->ui.InitEngineCB     = NULL;

    cb->ui.OpenSession      = NULL;
    cb->ui.OpenSessionKey   = NULL;
    cb->ui.SessionStep      = NULL;
    cb->ui.GetSyncMLBuffer  = NULL;
    cb->ui.RetSyncMLBuffer  = NULL;
    cb->ui.ReadSyncMLBuffer = NULL;
    cb->ui.WriteSyncMLBuffer= NULL;
    cb->ui.CloseSession     = NULL;

    cb->ui.OpenKeyByPath    = NULL;
    cb->ui.OpenSubkey       = NULL;
    cb->ui.DeleteSubkey     = NULL;
    cb->ui.GetKeyID         = NULL;
    cb->ui.SetTextMode      = NULL;
    cb->ui.SetTimeMode      = NULL;
    cb->ui.CloseKey         = NULL;

    cb->ui.GetValue         = NULL;
    cb->ui.GetValueByID     = NULL;
    cb->ui.GetValueID       = NULL;
    cb->ui.SetValue         = NULL;
    cb->ui.SetValueByID     = NULL;
  } /* if */

  if (CB_OK( cb,9 )) {
    cb->SDK_Interface_size= sizeof(SDK_Interface_Struct); /* level 9 */

    cb->dt.StartDataRead    = NULL; /* tunnel callback functions, for internal use only */
    cb->dt.ReadNextItem     = NULL;
    cb->dt.ReadItem         = NULL;
    cb->dt.EndDataRead      = NULL;

    cb->dt.StartDataWrite   = NULL;
    cb->dt.InsertItem       = NULL;
    cb->dt.UpdateItem       = NULL;
    cb->dt.MoveItem         = NULL;
    cb->dt.DeleteItem       = NULL;
    cb->dt.EndDataWrite     = NULL;
  } /* if */

  if (CB_OK( cb,11 )) {
    cb->dt.DisposeObj       = NULL;

    cb->dt.ReadNextItemAsKey= NULL;
    cb->dt.ReadItemAsKey    = NULL;
    cb->dt.InsertItemAsKey  = NULL;
    cb->dt.UpdateItemAsKey  = NULL;
  } /* if */
} /* InitCallback */



/* Initialize to safe defaults and "CB_PurePrintf", usable for Std C as well */
/* Normal debug output */
void InitCallback_Pure  ( void* aCB, uInt16 aVersion ) {
     InitCallback             ( aCB,        aVersion, CB_PurePrintf, NULL );
} /* InitCallback_Pure */


/* Initialize to safe defaults and "CB_PurePrintf", usable for Std C as well */
/* Normal and exotic debug output */
void InitCallback_Exotic( void* aCB, uInt16 aVersion ) {
     InitCallback             ( aCB,        aVersion, CB_PurePrintf, CB_PurePrintf );
} /* InitCallback_Exotic */



/* The <aRoutine> for 'InitCallback', when a simple "printf" is
 * requested for the callback.
 *
 * Routine must be defined as
 *   typedef void (*DB_DebugPuts_Func)( void *aCallbackRef, cAppCharP aText );
 */
void CB_PurePrintf( void* aCB, cAppCharP aText )
{
  DB_Callback cb= aCB;
  if (!CB_OK( cb,2 )) return;
/*if ( BadCB( aCB ))  return;*/

  NBlk( cb ); printf( "%s\n", aText );
} /* CB_PurePrintf */



/* Get the size of the SDK_Interface_Struct to be copied */
TSyError SDK_Size( void* aCB, uInt32 *sSize )
{
  TSyError    err= LOCERR_OK;
  DB_Callback cb = aCB;

  do {
    /* try to get the SDK_Interface_Struct <sSize> of the calling engine */
    if (cb->callbackVersion< SDK_Interface_Struct_V8)  { err= LOCERR_TOOOLD; break; }
    if (cb->callbackVersion==SDK_Interface_Struct_V8) *sSize=     SDK_Interface_Struct_V8_Size;
    else                                              *sSize= cb->SDK_Interface_size;

    /* but it must neither be smaller than version 8 size nor larger than the own size */
    if (*sSize<       SDK_Interface_Struct_V8_Size)    { err= LOCERR_TOOOLD; break; }
    if (*sSize>sizeof(SDK_Interface_Struct))          *sSize= sizeof(SDK_Interface_Struct);
  } while (false);

/*printf( "sizeof=%d sSize=%d err=%d\n", sizeof(SDK_Interface_Struct), *sSize, err );*/
  return err;
} /* SDK_Size */



/* ---------- debug output ----------------------------------- */
/* prints directly to the screen */
static void ConsolePuts( cAppCharP msg )
{
  #ifdef ANDROID
    __android_log_write( ANDROID_LOG_DEBUG, "ConsolePuts", msg );
  #else
    printf( "%s\n", msg );
  #endif
} /* ConsolePuts */


#if !defined(SYSYNC_ENGINE) && !defined(UIAPI_LINKED)
  /* the Synthesis SyncML engine has its own implementation */
  /* => use this code in SDK only */
  void ConsoleVPrintf( cAppCharP format, va_list args )
  {
    #ifdef SYDEBUG
      char         msg[ maxmsglen ];
                   msg[ 0 ]= '\0';               /* start with an empty <msg> */
      vsnprintf  ( msg, maxmsglen,format,args ); /* assemble the message string */
      ConsolePuts( msg );                        /* write the string */
    #endif
  } /* ConsoleVPrintf */

  void ConsolePrintf( cAppCharP text, ... )
  {
    va_list              args;
    va_start           ( args,text );
    ConsoleVPrintf( text,args );
    va_end             ( args );
  } /* ConsolePrintf */
#endif


/* Get the callback version of <aCB> */
uInt16 CB_Version( void* aCB )
{
  DB_Callback    cb= aCB;
  if (cb) return cb->callbackVersion;
  else    return 0;
} /* CB_Version */


/* Check, if <aCB> structure is at least <minVersion> */
bool CB_OK( void* aCB, uInt16 minVersion ) { return CB_Version( aCB )>=minVersion; }

/* Check, if <aCB> structure supports UI callback (cbVersion>=6) */
/* static bool CB_UI( void* aCB ) { return CB_OK( aCB,6 ); } */

/* Check, if <aCB> structure supports <gContext>  (cbVersion>=8) and <gContext> <> 0 */
bool CB_gContext ( void* aCB )
{
  DB_Callback cb= aCB;
  return CB_OK  ( aCB,8 ) && cb->gContext!=0;
} /* CB_gContext */


/* Check, if <aCB> structure supports CA_SubSytem (cbVersion>=10) */
bool CB_SubSystem( void* aCB ) { return CB_OK( aCB,10 ); }


/* Check, if <aCB> structure is at least <minVersion> and DBG_PLUGIN_DB is set */
bool DB_OK( void* aCB, uInt16 minVersion )
{
  DB_Callback cb= aCB;                 /* at least one flag must be set */
  return   CB_OK( aCB,minVersion ) && (DBG_PLUGIN_DB & cb->debugFlags)!=DBG_PLUGIN_NONE;
} /* DB_OK */


/* Check, if <aCB> structure is at least <minVersion> and one of <debugFlags> is set */
bool Callback_OK( void* aCB, uInt16 minVersion, uInt16 debugFlags )
{
  DB_Callback cb= aCB; /* at least one flag must be set */
  return   CB_OK( aCB,minVersion ) && (debugFlags & cb->debugFlags)!=DBG_PLUGIN_NONE;
} /* Callback_OK */



/* Normal callback output of <text> */
void CallbackPuts( void* aCB, cAppCharP text )
{
  DB_Callback   cb= aCB;
  if (!Callback_OK( aCB, 1,DBG_PLUGIN_ALL )) return;

  if (cb->DB_DebugPuts)
      cb->DB_DebugPuts( cb->callbackRef, text );
} /* CallbackPuts */



/* Exotic callback output of <text> */
static void CallbackExotic( void* aCB, cAppCharP text )
{
  DB_Callback   cb= aCB;
  if (!Callback_OK( aCB, 3,DBG_PLUGIN_ALL )) { CallbackPuts( aCB,text ); return; }

  if (cb->DB_DebugExotic)
      cb->DB_DebugExotic( cb->callbackRef, text );
} /* CallbackExotic */



static void CallbackVPrintf( DB_Callback aCB, cAppCharP format, va_list args, uInt16 outputMode )
{
  #ifdef SYDEBUG
    #ifdef __GNUC__
      int isMax;
    #endif

    char               message[ maxmsglen ];
    char* ptr= (char*)&message;

    #ifdef __GNUC__
    // need a copy for vasprintf() call
    va_list copy;
    va_copy(copy, args);
    #endif

               message[ 0 ]= '\0';                /* start with an empty <msg> */
    vsnprintf( message, maxmsglen, format,args ); /* assemble the message string */

    #ifdef __GNUC__
          isMax= strlen(message)==maxmsglen-1;
      if (isMax && vasprintf( &ptr, format, copy ) != -1) {};
      va_end(copy);
    #endif

    switch (outputMode) {
      case OutputNorm        : CallbackPuts  ( aCB, ptr ); break;
   /* case OutputExoticBefore: */
   /* case OutputExoticAfter : */
      case OutputExotic      : CallbackExotic( aCB, ptr ); break;
      case OutputConsole     :
      case OutputExoticBefore:
      case OutputExoticAfter :
      case OutputBefore      :
      case OutputAfter       : NBlk   ( (void*)aCB );
                               ConsolePuts        ( ptr ); break;
    } /* switch */

    #ifdef __GNUC__
      if (isMax) free( ptr );
    #endif
  #endif
} /* CallbackVPrintf */



void DEBUG_( void* aCB, cAppCharP text, ... )
{
  #ifdef SYDEBUG
    va_list args;

    if (Callback_OK( aCB, 1,DBG_PLUGIN_ALL )) {
      va_start                 ( args,text );
      CallbackVPrintf( aCB, text,args, false );
      va_end                   ( args );
    } /* if (gDebug) */
  #endif
} /* DEBUG_ */



#ifdef SYDEBUG
void DoDEBUG( void* aCB, uInt16 outputMode, bool withIntro,
                    cAppCharP ident,
                    cAppCharP routine, va_list args,
                    cAppCharP text )
{
  cAppCharP dbIntro= "";
  cAppCharP id     = "";
  cAppCharP isX    = "";
  cAppCharP p      = "";
  char*     s;
  int       size;

  if (*routine!='\0') {
    if (withIntro) {                      dbIntro= "##### ";
      if (outputMode==OutputBefore ||
          outputMode==OutputExoticBefore) dbIntro= ">>>>> ";
      if (outputMode==OutputAfter  ||
          outputMode==OutputExoticAfter)  dbIntro= "<<<<< ";
    } /* if */

    id= ident;

    #if   defined SDK_LIB
      isX=       ": ";
    #elif defined SDK_DLL
      isX= " (DLL): ";
    #else
      isX= " (LNK): ";
    #endif

    if (*text!='\0') p= ": ";
  } /* if */

  size= strlen(dbIntro) + strlen(id)      +
        strlen(isX)     + strlen(routine) +
        strlen(p)       + strlen(text)    + 1;

           s= (char*)malloc( size );
  sprintf( s, "%s%s%s%s%s%s", dbIntro,id,isX,routine,p,text );

  CallbackVPrintf( aCB, s,args, outputMode );

  free   ( s );
} /* DoDEBUG */
#endif



/* ------------------------------------------------------------------------------------- */
void DEBUG_Call( void* aCB, uInt16 debugFlags,
                         cAppCharP ident,
                         cAppCharP routine,
                         cAppCharP text, ... )
{
  #ifdef SYDEBUG
    va_list args;

    if (Callback_OK( aCB, 1,debugFlags )) {
      va_start                               ( args,text );
      DoDEBUG( aCB, false,true, ident,routine, args,text );
      va_end                                 ( args );
    } /* if */
  #endif
} /* DEBUG_Call */



void DEBUG_INT( void* aCB, cAppCharP ident,
                           cAppCharP routine,
                           cAppCharP text, ... )
{
  #ifdef SYDEBUG
    va_list args;
    if (Callback_OK( aCB, 1,DBG_PLUGIN_INT )) {
      va_start                               ( args,text );
      DoDEBUG( aCB, false,true, ident,routine, args,text );
      va_end                                 ( args );
    } /* if */
  #endif
} /* DEBUG_INT */



void DEBUG_DB( void* aCB, cAppCharP ident,
                          cAppCharP routine,
                          cAppCharP text, ... )
{
  #ifdef SYDEBUG
    va_list args;
    if (Callback_OK( aCB, 1,DBG_PLUGIN_DB )) {
      va_start                               ( args,text );
      DoDEBUG( aCB, false,true, ident,routine, args,text );
      va_end                                 ( args );
    } /* if */
  #endif
} /* DEBUG_DB */



/* ------------------------------------------------------------------------------------- */
void DEBUG_Exotic_INT( void* aCB, cAppCharP ident,
                                  cAppCharP routine,
                                  cAppCharP text, ... )
{
  #ifdef SYDEBUG
    va_list args;
    if (Callback_OK( aCB, 1,DBG_PLUGIN_INT  ) &&
        Callback_OK( aCB, 1,DBG_PLUGIN_EXOT )) {
      va_start                              ( args,text );
      DoDEBUG( aCB, true,true, ident,routine, args,text );
      va_end                                ( args );
    } /* if */
  #endif
} /* DEBUG_Exotic_INT */



void DEBUG_Exotic_DB( void* aCB, cAppCharP ident,
                                 cAppCharP routine,
                                 cAppCharP text, ... )
{
  #ifdef SYDEBUG
    va_list args;
    if (Callback_OK( aCB, 1,DBG_PLUGIN_EXOT )) {
      va_start                              ( args,text );
      DoDEBUG( aCB, true,true, ident,routine, args,text );
      va_end                                ( args );
    } /* if */
  #endif
} /* DEBUG_Exotic_DB */



void DEBUG_Exotic_DBW( void* aCB, cAppCharP ident,
                                  cAppCharP routine,
                                  cAppCharP text, ... )
{
  #ifdef SYDEBUG
    va_list args;
    if (Callback_OK( aCB, 1,DBG_PLUGIN_EXOT )) {
      va_start                              ( args,text );
      DoDEBUG( aCB, true,false,ident,routine, args,text );
      va_end                                ( args );
    } /* if */
  #endif
} /* DEBUG_Exotic_DBW */



/* ------------------------------------------------------------------------------------- */
/* Start of sub block */
void DEBUG_Block( void* aCB, cAppCharP aTag,
                             cAppCharP aDesc,
                             cAppCharP aAttrText )
{
  /* callbackVersion >= 2 support for blocks */
  #ifdef SYDEBUG
    DB_Callback cb=  aCB;
    if (Callback_OK( aCB, 2,DBG_PLUGIN_ALL ) &&
        cb->DB_DebugBlock)
        cb->DB_DebugBlock( cb->callbackRef, aTag,aDesc,aAttrText );
    else /* old */
      DEBUG_DB( aCB, MyDB, aTag, "%s (%s) BEGIN", aAttrText,aDesc );
  #endif
} /* DEBUG_Block */



/* End of sub block */
void DEBUG_EndBlock( void* aCB, cAppCharP aTag )
{
  #ifdef SYDEBUG
    DB_Callback cb=  aCB;
    if (Callback_OK( aCB, 2,DBG_PLUGIN_ALL ) &&
        cb->DB_DebugEndBlock)
        cb->DB_DebugEndBlock( cb->callbackRef, aTag );
    else /* old */
      DEBUG_DB( aCB, MyDB, aTag, "END" );
  #endif
} /* DEBUG_EndBlock */



void DEBUG_EndThread( void* aCB )
{
  #ifdef SYDEBUG
    DB_Callback cb=  aCB;
    if (Callback_OK( aCB, 2,DBG_PLUGIN_ALL ) &&
        cb->DB_DebugEndThread)
        cb->DB_DebugEndThread( cb->callbackRef );
    else /* old */
      DEBUG_DB( aCB, MyDB, "THREAD", "END" );
  #endif
} /* DEBUG_EndThread */


/* eof */
