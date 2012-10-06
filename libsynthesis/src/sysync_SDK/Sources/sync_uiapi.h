/*
 *  File:    sync_uiapi.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *  C/C++    Programming interface between the
 *           Synthesis SyncML engine and
 *           the user interface layer for plug-ins
 *
 *  Copyright (c) 2007-2011 by Synthesis AG + plan44.ch
 *
 */

/*
 * This is the calling interface between the Synthesis SyncML engine
 * and a customized module "sync_uiapi".
 * The same interface can be used either for Standard C or for C++.
 * And there is a equivalent interface for JNI (Java Native Interface).
 * Normally the customized module will be compiled as DLL and will
 * be called by the SyncML engine. A linkable library is available
 * (for C++) as well.
 *
 *  NOTE: The plugin's module context is currently defined at
 *        "sync_dbapi.h" as it is the same structure as for the
 *        data base plugin.
 *
 *  NOTE: As the SyncML engine calls the plug-in multithreaded,
 *        all global structure accesses must be thread save.
 */


#ifndef SYNC_UIAPI_H
#define SYNC_UIAPI_H

/*
// Global declarations
#if defined __cplusplus
  // combine the definitions of different namespaces
  #ifdef SYSYNC_ENGINE
    using sysync::TSyError;
  #endif
#endif
*/

/* C/C++ and DLL/library support
 * SYSYNC_ENGINE: true ( within the engine itself )         / false ( outside )
 * PLUGIN_INFO  : true ( within "plugin_info" program )     / false ( everywhere else )
 */
#if !defined SYSYNC_ENGINE && !defined SYSYNC_ENGINE_TEST && !defined DBAPI_LINKED && !defined PLUGIN_INFO && !defined DLL_EXPORT
  #define DLL_EXPORT
#endif

/* Visual Studio 2005 requires a specific entry point definition */
/* This definition is empty for all other platforms */
#undef    _ENTRY_
#ifdef DLL_EXPORT
  #define _ENTRY_ ENGINE_ENTRY
#else
  #define _ENTRY_
#endif


/* -- UI interface -------------------------------------------------------------- */
/*! Create a user interface context \<uContext>
 *
 *  @param  <uContext>   Returns a value, which allows to identify this UI context.
 *  @param  <uiName>     Name of this user interface context
 *  @param  <uCB>        UI_Callback structure for module logging
 *
 *  @return  error code, if context could not be created (e.g. not enough memory),
 *           0           if context successfully created.
 */
_ENTRY_ TSyError UI_CreateContext( CContext *uContext, cAppCharP uiName, UI_Call_In uCI );


/*! Start interaction with user interface at \<uContext>.
 */
_ENTRY_ TSyError UI_RunContext( CContext uContext );


/*! This routine will be called as the last call, before this module is disconnected.
 *  The SyncML engine will call 'UI_DisposeObj' (if required) before this call
 *
 *  NOTE: This routine will be called ONLY, if the server stops in a controlled way.
 *        Its good programming practice not to wait for this 'DeleteContext' call.
 *
 *  @param  <uContext>  The module context.
 *  @return  error code
 */
_ENTRY_ TSyError UI_DeleteContext( CContext uContext );


#endif
/* eof */
