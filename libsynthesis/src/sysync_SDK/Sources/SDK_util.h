/*
 *  File:     SDK_util.h
 *
 *  Authors:  Beat Forster
 *
 *
 *  Useful SDK utility functions
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 */


#ifndef SDK_UTIL_H
#define SDK_UTIL_H


#include "sync_include.h"


/* C/C++ and DLL/library support */
#ifdef __cplusplus
  extern "C" {

  using sysync::uInt16;
  using sysync::uInt32;
  using sysync::appCharP;
  using sysync::cAppCharP;
  using sysync::appPointer;
  using sysync::CVersion;
  using sysync::TSyError;
#endif

#if defined SYSYNC_ENGINE || defined PLUGIN_INFO
  #define DLL_Info "DLL:false"
#else
  #define DLL_Info "DLL:true"
#endif


/*! Get the current version of the plug-in module.
 *  The returned value will be incremented in future releases of the SDK.
 *  Not to be changed by plug-in developer.
 *  A customer defined <buildNumber> can be set (0..255).
 */
CVersion Plugin_Version( short buildNumber );


/*! Check, if <version_feature> is supported in <currentVersion>
 *  NOTE: For the SyncML engine internally everything is supported
 *        This will be reflected with the fact that for 2.1.1.X
 *        the engine's version was always much much higher.
 *        Today the engine's version is equivalent to the SDK version.
 */
bool Feature_Supported  ( CVersion versionFeature, CVersion currentVersion );

/*! Check, if <version_feature> is equivalent to <currentVersion> */
bool Feature_SupportedEq( CVersion versionFeature, CVersion currentVersion );


/*! Get the "PLATFORM:xxx" string as const definition */
cAppCharP MyPlatform( void );


/* ---------- allocate/dispose local memory for a string ----- */
/*! Allocate a string locally, with length \<n> */
appCharP StrAllocN ( cAppCharP s, int n, bool fullSize );

/*! Allocate a string locally */
appCharP StrAlloc  ( cAppCharP s );

/* !Dispose a string which has been allocated with 'StrAlloc' */
void     StrDispose( void* s );




/* ---------------- field operations ------------------------- */
/*! Gets the <field> of <key> at <item>, if available */
bool Field    ( cAppCharP item, cAppCharP key,  appCharP *field );

/*! Returns true, if <item>'s <key> field is <field>. */
bool SameField( cAppCharP item, cAppCharP key, cAppCharP  field );



/* ---------- callback setup (for internal use only) --------- */
/*! Initialize the callback structure for debug output
 *  For internal use only !!
 */
void NBlk               ( void* aCB );
void InitCallback       ( void* aCB, uInt16 aVersion, appPointer aRoutine,
                                                      appPointer aExoticRoutine );

/*! Using 'CB_PurePrintf' as <aRoutine>; no overloading possible with Std C */
void InitCallback_Pure  ( void* aCB, uInt16 aVersion );
void InitCallback_Exotic( void* aCB, uInt16 aVersion ); /* plus exotic output */


/*! If simple "printf" is requested for the callback.
 * 'CB_PurePrintf' can be given as <aRoutine> for 'InitCallback.
 *  For internal use only !!
 */
void CB_PurePrintf( void* aRef, cAppCharP aTxt );


/* Get the size of the SDK_Interface_Struct to be copied */
TSyError SDK_Size ( void* aCB, uInt32 *sSize );



/* ---------- debug output ----------------------------------- */
#ifndef SYSYNC_ENGINE
  /* if it is running standalone as DLL */
  /* the Synthesis SyncML engine has its own implementation */
  /* => use this code in SDK only */
  void ConsoleVPrintf( cAppCharP format, va_list args );
  void ConsolePrintf ( cAppCharP text, ... );
#endif

/* Output modes */
#define OutputNorm         0
#define OutputExotic       1
#define OutputConsole      2
#define OutputBefore       3
#define OutputAfter        4
#define OutputExoticBefore 5
#define OutputExoticAfter  6


/* Get the callback version of <aCB> */
uInt16 CB_Version( void* aCB );

/* Check, if <aCB> structure is at least <minVersion> */
bool CB_OK       ( void* aCB, uInt16 minVersion );

/* Check, if <aCB> structure supports <gContext>  (cbVersion>=8) and <gContext> <> 0 */
bool CB_gContext ( void* aCB );

/* Check, if <aCB> structure supports CA_SubSytem (cbVersion>=10) */
bool CB_SubSystem( void* aCB );

/* Check, if <aCB> structure is at least <minVersion> and DBG_PLUGIN_DB is set */
bool DB_OK       ( void* aCB, uInt16 minVersion );

/* Check, if <aCB> structure is at least <minVersion> and one of <debugFlags> is set */
bool Callback_OK ( void* aCB, uInt16 minVersion, uInt16 debugFlags );

void CallbackPuts( void* aCB, cAppCharP text );
void DEBUG_      ( void* aCB, cAppCharP text, ... );

void DoDEBUG     ( void* aCB, uInt16    outputMode, bool withIdent,
                              cAppCharP ident,
                              cAppCharP routine, va_list args, cAppCharP text );


/*! The debug logging callback call
 *  The SyncML engine will write the text to the context assigned log file
 *
 *  Input: <aCB>    : Callback variable of the context. The SyncML engine will
 *                    pass such a reference, when creating the module, session
 *                    or datastore context. The plug-in must store it within its
 *                    context for later use.
 *      <debugFlags>: Debug logging flags, bits0/1 are reserved, all others can be
 *                    defined plugin specific
 *         <ident>  : Normally the name of the plug-in module, usually defined as 'MyDB'
 *         <routine>: The name of the current routine for reference
 *         <text>   : The logging text in "printf" format.
 *         ...      : The parameters, defined at \<text> with %s, %d, %X ...
 */
void DEBUG_Call       ( void* aCB, uInt16    debugFlags,
                                   cAppCharP ident, cAppCharP routine, cAppCharP text, ... );

/*! The same as DEBUG_Call, but only when specific DBG_PLUGIN_INT/DBG_PLUGIN_DB flag is set */
void DEBUG_INT        ( void* aCB, cAppCharP ident, cAppCharP routine, cAppCharP text, ... );
void DEBUG_DB         ( void* aCB, cAppCharP ident, cAppCharP routine, cAppCharP text, ... );

/*! The same as DEBUG_Call/DEBUG_INT/DEBUG/DEBUG_DB, but only, if exotic flag is set */
void DEBUG_Exotic_INT ( void* aCB, cAppCharP ident, cAppCharP routine, cAppCharP text, ... );
void DEBUG_Exotic_DB  ( void* aCB, cAppCharP ident, cAppCharP routine, cAppCharP text, ... );
void DEBUG_Exotic_DBW ( void* aCB, cAppCharP ident, cAppCharP routine, cAppCharP text, ... );

/*! Special block and thread markers
 *  If \<aTag> starts with "-", this block will be displayed collapsed by default
 */
void DEBUG_Block      ( void* aCB, cAppCharP aTag,  cAppCharP aDesc,   cAppCharP aAttrText );
void DEBUG_EndBlock   ( void* aCB, cAppCharP aTag );
void DEBUG_EndThread  ( void* aCB );


/* C/C++ and DLL/library support */
#if defined __cplusplus
  } // end extern "C"
#endif

#endif /* SDK_UTIL_H */
/* eof */
