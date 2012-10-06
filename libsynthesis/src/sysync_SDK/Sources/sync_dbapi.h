/*
 *  File:    sync_dbapi.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *  C/C++    Programming interface between the
 *           Synthesis SyncML engine and
 *           the database layer for plug-ins
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 */

/*
 * This is the calling interface between the Synthesis SyncML engine
 * and a customized module "sync_dbapi".
 * The same interface can be used either for Standard C or for C++.
 * And there is a equivalent interface for JNI (Java Native Interface).
 * Normally the customized module will be compiled as DLL and will
 * be called by the SyncML engine. A linkable library is available
 * (for C++) as well.
 *
 * For more detailed information about the DBApi interface
 * please consult the SDK_manual.pdf which contains a tutorial,
 * detailed descriptions and some example code.
 *
 * The flow for accessing the datastores is always the same:
 * The SyncML engine will call these routines step by step
 *
 *  DATASTORE ACCESS FLOW
 *  =====================
 *
 *   1)  CreateContext
 *   2) [ContextSupport]   (optional)
 *   3) [FilterSupport]    (optional)
 *   4) [Read Admin Data]  (optional)
 *
 *   5)  StartDataRead
 *   6)  do ReadNextItem while (aStatus!=ReadNextItem_EOF);
 *   7)  any number of random calls to:
 *         -  ReadItem
 *         -  ReadBlob
 *         - [AdaptItem]   (optional)
 *   8)  EndDataRead
 *
 *   9)  StartDataWrite
 *  10)  any number of random calls to:
 *         -  InsertItem
 *         -  UpdateItem
 *         -  DeleteItem
 *         -  DeleteSyncSet
 *         -  ReadItem
 *         -  WriteBlob
 *         -  ReadBlob
 *         -  DeleteBlob
 *         - [AdaptItem]      (optional)
 *         -  FinalizeLocalID (at the end)
 *  11)  EndDataWrite
 *
 *  12) [Write Admin Data] (optional)
 *  13)  DeleteContext
 *
 *
 *  NOTE: 'DisposeObj' calls can occur anywhere between any statement
 *        of the flow above (but not before 'CreateContext' and not after
 *        'DeleteContext'). The SyncML engine is responsible for
 *        disposing all <aItemData> and <aItemID> objects after use.
 *
 *  NOTE: Returned errors (value > 0) will influence the flow
 *        Error at 1)      will not call any further step.
 *          "   "  2)..3)  these functions do not return errors
 *          "   "  5)..10) will cause an 'EndDataWrite' call with
 *                         <success> = false, then 'DeleteContext'.
 *          "   "  4)
 *                11)..12) 'DeleteContext' will be called afterwards.
 *
 *  The module must be able to handle several contexts in parallel.
 *  All routines will have an <aContext> parameter (assigned by
 *  'CreateContext'), which allows to identify the correct context.
 *  Routines can be called from different threads, but they are
 *  always sequential for each context. If the thread changes,
 *  'ThreadMayChangeNow' will notify the module about this issue.
 *  This routine can left empty and ignored, if not used.
 *
 *  NOTE: As the SyncML engine calls the plug-in multithreaded,
 *        all global structure accesses must be thread save.
 */


#ifndef SYNC_DBAPI_H
#define SYNC_DBAPI_H

/* Global declarations */
#include "sync_dbapidef.h"


#if defined __cplusplus
  /* combine the definitions of different namespaces */
  using sysync::sInt32;
  using sysync::uInt32;
  using sysync::CContext;
  using sysync::CVersion;
  using sysync::TSyError;
  using sysync::ItemID;
  using sysync::MapID;
  using sysync::cItemID;
  using sysync::cMapID;
  using sysync::appCharP;
  using sysync::cAppCharP;
  using sysync::appPointer;
  using sysync::memSize;
  using sysync::KeyH;
  using sysync::SDK_Interface_Struct;
  using sysync::DB_Callback;
  using sysync::UI_Call_In;
#endif


/* C/C++ and DLL/library support
 * SYSYNC_ENGINE     : true ( within the engine itself )         / false ( outside )
 * SYSYNC_ENGINE_TEST: true ( within a test engine module )      / false ( outside )
 * DBAPI_LINKED      : true ( at standalone APP, e.g. "helloX" ) / false ( within engine/within DLL )
 * PLUGIN_INFO       : true ( within "plugin_info" program )     / false ( everywhere else )
 */
#if !defined SYSYNC_ENGINE && !defined SYSYNC_ENGINE_TEST && !defined DBAPI_LINKED && !defined PLUGIN_INFO && !defined DLL_EXPORT
  #define DLL_EXPORT 1
#endif

#undef    _ENTRY_ /* could be defined already here */
#ifdef DLL_EXPORT
  #define _ENTRY_ ENGINE_ENTRY
#else
  #define _ENTRY_
#endif


/* -- MODULE -------------------------------------------------------------------- */
/*! Create a module context \<mContext>
 *  This routine will be called as the 2nd call, when the module will be connected.
 *  (The 1st call is a 'Module_Version( 0 )' call outside any context).
 *
 *  It will be called not only once, but once for each session and datastore context,
 *  as defined at the XML config file. This routine can return error 20028 (LOCERR_ALREADY),
 *  if already created. This will be treated not as an error. For this case, it must
 *  return the same \<mContext> as for the former call(s).
 *
 *  NOTE: The module context can exist once and can be shared for all plug-in accesses.
 *        This can either be done with an allocated global variable at the plug-in or
 *        even better using the "GlobContext" structure provided by the SyncML engine.
 *        Please note, that write access to such a common module context structure must
 *        be thread-safe, when accessed from the session or datastore context.
 *        All the 'Module_CreateContext' calls for this module will be called
 *        sequentially by one thread. The plug-in programmer is responsible not to
 *        re-initialize the context for subsequent calls.
 *
 * If the module name at the XML config file is defined as "aaa!bbb!ccc" it will be passed
 * as "aaa" to \<moduleName> and "bbb!ccc" to \<subName>. This mechanism can be used to
 * cascade plug-in modules, where the next module gets "bbb" as \<moduleName> and "ccc" as
 * \<subName>. The JNI plug-in for Java is using this structure to address the JNI plug-in
 * and its assigned Java class. Error 20034 (LOCERR_UNKSUBSYSTEM) should be returned in case
 * the subsystem does not exist, no error if no subsystem has been chosen at all.
 *
 *
 *  @param  <mContext>     Returns a value, which allows to identify this module context.
 *                         Allowed values: Anything except 0, which is reserved for no context.
 *  @param  <moduleName>   Name of this plug-in
 *  @param  <subName>      Name of sub module (if available)
 *  @param  <mContextName> Name of the (datastore) context, e.g. "contacts";
 *                         this string is empty for calll concerning the session.
 *  @param  <mCB>          DB_Callback structure for module logging
 *
 *  @return  error code     if context could not be created       (e.g. not enough memory)
 *           LOCERR_ALREADY if global module context already exists (not treated as error)
 *           0              if context successfully created.
 */
_ENTRY_ TSyError Module_CreateContext( CContext *mContext, cAppCharP   moduleName,
                                                           cAppCharP      subName,
                                                           cAppCharP mContextName,
                                                         DB_Callback mCB );



/*! Get the module's version.
 *
 *  NOTE: The SyncML will take decisions depending on this version number, so
 *  the plug-in developer should not change the values at the delivered sample code.
 *  Plugin_Version( short buildNumber ) of 'SDK_util' should be used.
 *  The \<buildNumber> can be defined by the user.
 *
 *  NOTE: This function can be called by the engine outside any context with
 *  \<mContext> = 0. For this case, any callback is not permitted (as no DB_Callback
 *  is available).
 *
 *  @param  <mContext>      The module context ( 0, if none ).
 *  @return current version as SDK_VERSION_MAJOR | SDK_VERSION_MINOR)
 *                           | SDK_SUBVERSION    | buildNumber
 */
_ENTRY_ CVersion Module_Version( CContext mContext );



/*! Get the module's capabilities
 *  Currently the SyncML engine currently understands and supports:
 *   - "plugin_sessionauth"
 *   - "plugin_deviceadmin"
 *   - "plugin_datastoreadmin"
 *   - "plugin_datastore"
 *
 *  If one of these identifiers will be defined as "no" ( e.g. "plugin_sessionauth:no" ),
 *  the according routines will not be connected and used.
 *
 *  NOTE: The \<mCapabilities> can be allocated with "StrAlloc" (SDK_util.h) for C/C++
 *
 *  @param  <mContext>      The module context.
 *  @param  <mCapabilities> Returns the module's capabilities as multiline
 *                          aa:bb\<CRLF>cc:dd[\<CRLF>]
 *  @return  error code
 */
_ENTRY_ TSyError Module_Capabilities( CContext mContext, appCharP *mCapabilities );



/*! The module's config params will be sent to the plug-in.
 *  It can be used for access path definitions or other things.
 *  The \<plugin_params> can be defined individually for each session and datastore.
 *  The SyncML engine checks the syntax, but not the content.
 *  This routine should return an error 20010 (LOCERR_CFGPARSE), if one of
 *  these parameters is not supported.
 *
 *  EXAMPLE: Definition at XML config file:\n
 *             \<plugin_params>\n
 *               \<datapath>/var/log/sysync\</datapath>\n
 *               \<ultimate_answer>42\</ultimate_answer>\n
 *             \</plugin_params>
 *
 *           will be passed as:\n
 *             "datapath:/var/log/sysync\n
 *              ultimate_answer:42"
 *
 *  NOTE: Module_PluginParams will be called ALWAYS for each module context,
 *        even if no plug-in parameter is defined. This allows to react consistently
 *        on parameters, which are not always available.
 *
 *  @param  <mContext>      The module context.
 *  @param  <mConfigParams> The plugin params as multiline aa:bb\<CRLF>cc:dd[\<CRLF>]
 *  @param  <engineVersion> The SyncML engine's version
 *  @return  error code
 */
_ENTRY_ TSyError Module_PluginParams( CContext mContext, cAppCharP mConfigParams,
                                                          CVersion engineVersion );



/*! Disposes memory, which has been allocated within the module context.
 *  (At the moment this is only the capabilities string).
 *  'Module_DisposeObj' can occur at any time within \<mContext>.
 *
 *  NOTE: - If \<mCapabilities> has been allocated with "StrAlloc", use "Str_Dispose"
 *          (SDK_util.h) to release the memory again.
 *        - If it is defined as const within the plugin module (the module
 *          itself knows about !), this routine can be implemented empty.
 *
 *  @param  <mContext>  The module context.
 *  @param  <memory>    Dispose allocated memory.
 *  @return  -
 */
_ENTRY_ void Module_DisposeObj( CContext mContext, void* memory );



/*! This routine will be called as the last call, before this module is disconnected.
 *  The SyncML engine will call 'Module_DisposeObj' (if required) before this call
 *
 *  NOTE: This routine will be called ONLY, if the server stops in a controlled way.
 *        Its good programming practice not to wait for this 'DeleteContext' call.
 *
 *  @param  <mContext>  The module context.
 *  @return  error code
 */
_ENTRY_ TSyError Module_DeleteContext( CContext mContext );




/* -- SESSION ------------------------------------------------------------------- */
/*! By default the session context will be handled by the ODBC interface.
 *  The session context of this plug-in module will be used only,
 *  if \<server type="plugin"> and \<plugin_module> is defined
 *  ( \<plugin_module>name_of_the_plugin\</plugin_module> ).
 *  \<plugin_params> can be defined individually.
 *
 *  @param  <sContext>    Returns a value, which allows to identify this session context.
 *  @param  <sessionName> Name of this session
 *  @param  <sCB>         DB_Callback structure for session logging
 *
 *  @result  error code, if context could not be created (e.g. not enough memory)
 *           0           if context successfully created,
 *
 *  Flags (at the XML config file):
 *  - \<plugin_deviceadmin>yes\</plugin_deviceadmin>: "Session_CheckDevice", "Session_GetNonce"
 *                                                    "Session_SaveNonce" and
 *                                                    "Session_SaveDeviceInfo" will be used.
 *
 *  - \<plugin_sessionauth>yes\</plugin_sessionauth>: "Session_PasswordMode",
 *                                                    "Session_Login" and "Session_Logout" will be used.
 */
_ENTRY_ TSyError Session_CreateContext( CContext *sContext, cAppCharP sessionName,
                                                          DB_Callback sCB );



/*! This function adapts itemData
 *
 *  @param  <sContext>    The session context
 *  @param  <sItemData1>  The 1st item's data
 *  @param  <sItemData2>  The 2nd item's data
 *  @param  <sLocalVars>  The local vars
 *  @param  <sIdentifier> To identify, where it is called
 *
 *  @return  error code
 *
 *  NOTE:   The memory for adapted strings must be allocated locally.
 *          The SyncML engine will call 'DisposeObj' later, to release again its memory.
 *          One or more strings can be returned unchanged as well.
 */
_ENTRY_ TSyError Session_AdaptItem( CContext sContext, appCharP *sItemData1,
                                                       appCharP *sItemData2,
                                                       appCharP *sLocalVars,
                                                         uInt32  sIdentifier );


/*! Check the database entry of \<aDeviceID> and return its \<nonce> string.
 *  If \<aDeviceID> is not yet available at the plug-in, return "" for \<nonce>
 *
 *  @param  <sContext>   The session context
 *  @param  <aDeviceID>  The assigned device ID string
 *  @param  <sDevKey>    The device key string (will be used for datastore accesses later)
 *  @param  <nonce>      The nonce string of the last session
 *                       If \<aDeviceID> is not yet available, return "" for \<nonce>
 *                       and error code 0.
 *
 *  @result  error code 403 (Forbidden), if plugin_deviceadmin is not supported;
 *                        0, if successful
 *
 *  USED ONLY WITH \<plugin_deviceadmin>
 */
_ENTRY_ TSyError Session_CheckDevice( CContext sContext, cAppCharP  aDeviceID,
                                                          appCharP *sDevKey,
                                                          appCharP *nonce );


/*! Get a new nonce from the database. If this routine returns an error,
 *  the SyncML engine will create its own nonce.
 *
 *  @param  <sContext>   The session context
 *  @param  <nonce>      A valid new nonce value (for the assigned device ID).
 *
 *  @return  error code 404 (NotFound), if no \<nonce> has been generated;
 *                       0, if a valid \<nonce> has been generated
 *
 *  USED ONLY WITH \<plugin_deviceadmin>
 */
_ENTRY_ TSyError Session_GetNonce( CContext sContext, appCharP *nonce );



/*! Save the new nonce (which will be expected to be returned in the
 *  next session for this device ID.
 *
 *  @param  <sContext>   The session context
 *  @param  <nonce>      New \<nonce> for the next session (of the assigned device ID)
 *
 *  @result  error code 403 (Forbidden), if plugin_deviceadmin is not supported;
 *                        0,             if successful
 *
 *  USED ONLY WITH \<plugin_deviceadmin>
 */
_ENTRY_ TSyError Session_SaveNonce( CContext sContext, cAppCharP nonce );



/*! Save the device info for \<sContext>
 *
 *  @param  <sContext>     The session context
 *  @param  <aDeviceInfo>  More information about the assigned device (for DB and logging)
 *
 *  @result  error code 403 (Forbidden), if plugin_deviceadmin is not supported;
 *                        0,             if successful
 *
 *  USED ONLY WITH \<plugin_deviceadmin>
 */
_ENTRY_ TSyError Session_SaveDeviceInfo( CContext sContext, cAppCharP aDeviceInfo );



/*! Get the current DB time of \<sContext>
 *
 *  @param  <sContext>       The session context
 *  @param  <currentDBTime>  The current time of the plugin's DB (as ISO8601 format).
 *
 *  @result  error code 403 (Forbidden), if plugin_deviceadmin is not supported;
 *                      404 (NotFound),  if not available -> the engine creates its own time
 *                        0,             if successful
 */
_ENTRY_ TSyError Session_GetDBTime( CContext sContext, appCharP *currentDBTime );



/*--------------------------------------------------------------------------------*/
/*! Get the password mode.
 *  There are currently 4 different password modes supported.
 *
 *  @param <sContext>          The session context
 *
 *  @result
 *    - Password_ClrText_IN  : 'SessionLogin' will get    clear text password
 *    - Password_ClrText_OUT :        "       must return clear text password
 *    - Password_MD5_OUT     :        "       must return MD5 coded  password
 *    - Password_MD5_Nonce_IN:        "       will get    MD5B64(MD5B64(user:pwd):nonce)
 *
 *  USED ONLY WITH \<plugin_sessionauth>
 */
_ENTRY_ sInt32 Session_PasswordMode( CContext sContext );


/*! Get \<sUsrKey> of \<sUsername>,\<sPassword> in the session context.
 *
 *  @param  <sContext>   The session context
 *  @param  <sUsername>  The user name ...
 *  @param  <sPassword>  ... and the password. \<sPassword> is an input parameter
                         for 'Password_ClrTxt_IN' mode and an output parameter for
 *                       'Password_ClrText_OUT' and 'Password_MD5_OUT' modes.
 *  @param  <sUsrKey>    Returns the internal reference key, which will be passed to
 *                       to the datastore contexts later.
 *
 *  @result  error code 403 (Forbidden), if plugin_sessionauth is not supported;
 *                        0, if successful
 *
 *  USED ONLY WITH \<plugin_sessionauth>
 */
_ENTRY_ TSyError Session_Login( CContext sContext, cAppCharP  sUsername,
                                                    appCharP *sPassword,
                                                    appCharP *sUsrKey );


/*! Logout for this session context
 *
 *  @param  <sContext>  The session context
 *
 *  @result  error code 403 (Forbidden), if plugin_sessionauth is not supported;
 *                        0, if successful
 *
 *  USED ONLY WITH \<plugin_sessionauth>
 */
_ENTRY_ TSyError Session_Logout( CContext sContext );



/*! Disposes memory, which has been allocated within the session context.
 *  'Session_DisposeObj' can occur at any time within \<sContext>.
 *
 *  @param  <sContext>  The session context.
 *  @param  <memory>    Dispose allocated memory.
 *
 *  @return  -
 */
_ENTRY_ void Session_DisposeObj( CContext sContext, void* memory );



/*! Due to the architecture of the SyncML engine, the system may run in a multithread
 *  environment. The consequence is that each routine of this plugin module can be
 *  called by a different thread. Normally this is not a problem, nevertheless
 *  this routine notifies about thread changes in \<sContext>.
 *  It can be ignored ( =implemented empty), if not really needed.
 *
 *  @param  <sContext>      The session context
 *  @return  -
 */
_ENTRY_ void Session_ThreadMayChangeNow( CContext sContext );



/*! Writes the context of all items to dbg output path
 *  This routine is implemented for debug purposes only and will NOT BE CALLED by the
 *  SyncML engine. Can be implemented empty
 *
 *  @param  <sContext>      The session context
 *  @param  <allFields>     true : all fields, also empty ones, will be displayed;
 *                          false: only fields <> "" will be shown
 *  @param  <specificItem>  ""   : all items will be shown;
 *                          else   shows the \<specificItem>
 *
 *  @return  -
 */
_ENTRY_ void Session_DispItems( CContext sContext, bool allFields, cAppCharP specificItem );



/*! Delete a session context.
 *  No access to \<sContext> will be done after this call
 *
 *  @param  <sContext>  The session context
 *  @return  error code, if context could not be deleted.
 */
_ENTRY_ TSyError Session_DeleteContext( CContext sContext );




/* -- OPEN ---------------------------------------------------------------------- */
/*! This routine is called to create a new context for a datastore access.
 *  It must allocate all resources for this context and initialize the \<aContext>
 *  parameter with a value that allows re-identifying the context.
 *  \<aContext> can either be a pointer to the local context structure or any key
 *  value which allows to re-identify the context later.
 *  Subsequent calls related to this context will pass the \<aContext> value as returned
 *  from CreateContext. The context must be valid until 'DeleteContext' is called.
 *  \<plugin_params> can be defined individually.
 *
 *  NOTE:   The SyncML engine treats \<aContext> simply as a key. The only condition
 *          is uniqueness for all datastore contexts. Even \<aContext> = 0 can be used.
 *
 *  @param  <aContext>     Returns a value, which allows to identify this datastore context.
 *  @param  <aContextName> Allows to identify the context, if more than one must be
 *                         handled. \<contextName> is defined at the XML configuration.
 *  @param  <aCB>          DB_Callback structure for datatstore logging.
 *  @param  <sDevKey>      The result of 'Session_CheckDevice' comes in here.
 *  @param  <sUsrKey>      The result of 'Session_Login'       comes in here.
 *
 *  @return  error code, if context could not be created (e.g. not enough memory),
 *           0           if context successfully created.
 */
_ENTRY_ TSyError CreateContext( CContext *aContext, cAppCharP aContextName, DB_Callback aCB,
                                                    cAppCharP sDevKey,
                                                    cAppCharP sUsrKey );


/*! This function asks for specific context configurations
 *
 *  @param  <aContext>      The datastore context
 *  @param  <aSupportRules> The SyncML sends a list of support rules.
 *                          This function has to reply, up to which rule,
 *                          contexts are supported (and switched on now).
 *                          Data is formatted as multiline aa:bb\<CRLF>cc:dd[\<CRLF>]
 *
 *  @return  Up to \<n> fields are supported (and switched on) for this context.
 *           If 0 will be returned, no field of \<aSupportRules> is supported.
 *
 */
_ENTRY_ uInt32 ContextSupport( CContext aContext, cAppCharP aSupportRules );



/*! This function asks for filter support.
 *
 *  @param  <aContext>      The datastore context
 *  @param  <aFilterRules>  The SyncML sends a list of filter rules.
 *                          This function has to reply, up to which rule,
 *                          filters are supported (and switched on now).
 *                          Data is formatted as multiline aa:bb\<CRLF>cc:dd[\<CRLF>]
 *
 *  @return  Up to \<n> filters are supported (and switched on) for this context
 *           If 0 will be returned, no field of \<aFilterRules> are supported.
 */
_ENTRY_ uInt32 FilterSupport( CContext aContext, cAppCharP aFilterRules );



/* -- GENERAL ------------------------------------------------------------------- */
/*! Due to the architecture of the SyncML engine, the system may run in a multithread
 *  environment. The consequence is that each routine of this API module can be
 *  called by a different thread. Normally this is not a problem, nevertheless
 *  this routine notifies about thread changes in \<aContext>.
 *  It can be ignored ( =implemented empty), if not really needed.
 *
 *  @param  <aContext>  The datastore context.
 *
 *  @result  -
 */
_ENTRY_ void ThreadMayChangeNow( CContext aContext );



/*! This functions writes \<logData> for this context
 *  Can be implemented empty, if not needed.
 *
 *  @param  <aContext>  The datastore context.
 *  @param  <logData>   Logging information, formatted as multiline aa:bb\<CRLF>cc:dd[\<CRLF>]
 *
 *  @result  -
 */
_ENTRY_ void WriteLogData( CContext aContext, cAppCharP logData );



/*! Writes the context of all items to dbg output path
 *  This routine is implemented for debug purposes only and will NOT BE CALLED by the
 *  SyncML engine. Can be implemented empty, if not needed.
 *
 *  @param  <aContext>      The datastore context.
 *  @param  <allFields>
 *                          - true : all fields, also empty ones, will be displayed;
 *                          - false: only fields <> "" will be shown
 *  @param  <specificItem>
 *                          - ""   : all items will be shown;
 *                          - else : shows the \<specificItem>
 *
 *  @return  -
 */
_ENTRY_ void DispItems( CContext aContext, bool allFields, cAppCharP specificItem );



/* -- ADMINISTRATION ------------------------------------------------------------ */
/* This section contains the 'admin read' and 'admin write' routines. */

/*! This function gets the stored information about the record with the four paramters:
 *  \<sDevKey>, \<sUsrKey>, \<aLocDB>, \<aRemDB>.
 *
 *  - \<plugin_deviceadmin>yes\</plugin_deviceadmin>: Admin/Map routines will be used.
 *
 *  @param  <aContext>   The datastore context
 *  @param  <aLocDB>     Name of the local  DB
 *  @param  <aRemDB>     Name of the remote DB
 *  @param  <adminData>  The data, saved with the last 'SaveAdminData' call
 *
 *  @return  error code 404 (NotFound), if record is not (yet) available,
 *                        0 (no error)  if admin data found
 *
 *  NOTE: \<sDevKey> and \<sUsrKey> have been passed with 'CreateContext' already.
 *        The plug-in module must have stored them within the datastore context.
 *
 *  USED ONLY WITH \<plugin_datastoredadmin>
 */
_ENTRY_ TSyError LoadAdminData     ( CContext aContext, cAppCharP aLocDB,
                                                        cAppCharP aRemDB, appCharP *adminData );

/*! This is the equivalent to 'LoadAdminData', but using a key instead of a data string */
_ENTRY_ TSyError LoadAdminDataAsKey( CContext aContext, cAppCharP aLocDB,
                                                        cAppCharP aRemDB,     KeyH  aItemKey );


/*! This functions stores the new \<adminData> for this context
 *
 *  @param  <aContext>   The datastore context
 *  @param  <adminData>  The new set of admin data to be stored, will be loaded again
 *                       with the next 'LoadAdminData' call.
 *
 *  @result  error code, if data could not be saved (e.g. not enough memory);
 *                       0 if successfully created.
 *
 *  USED ONLY WITH \<plugin_datastoredadmin>
 */
_ENTRY_ TSyError SaveAdminData     ( CContext aContext, cAppCharP adminData );

/*! This is the equivalent to 'SaveAdminData', but using a key instead of a data string */
_ENTRY_ TSyError SaveAdminDataAsKey( CContext aContext,      KeyH aItemKey );



/*! Map table handling: Get the next map item of this context.
 *  If \<aFirst> is true, the routine must start to return the first element
 *
 *  @param  <aContext>   The datastore context
 *  @param  <mID>        MapID ( with \<localID>,\<remoteID>, <flags> and \<ident> ).
 *  @param  <aFirst>     Starting with the first MapID. When creating a context,
 *                       the first call will get the first MapID, even if \<aFirst>
 *                       is false.
 *
 *  @return
 *          - true:  as long as there is a MapID available, which must be assigned to <mID>
 *          - false: if there is no more MapID. Nothing must be assigned to <mID>
 *
 *  USED ONLY WITH \<plugin_datastoredadmin>
 */
_ENTRY_ bool ReadNextMapItem( CContext aContext, MapID mID, bool aFirst );


/*! Map table handling: Insert a map item of this context
 *
 *  @param  <aContext>   The datastore context
 *  @param  <mID>        MapID ( with \<localID>,\<remoteID>, \<flags> and \<ident> ).
 *
 *  @return  error code, if this MapID can't be inserted, or if already existing
 *
 *  USED ONLY WITH \<plugin_datastoredadmin>
 */
_ENTRY_ TSyError InsertMapItem( CContext aContext, cMapID mID );


/*! Map table handling: Update a map item of this context
 *
 *  @param  <aContext>   The datastore context
 *  @param  <mID>        MapID ( with \<localID>,\<remoteID>, \<flags> and \<ident> ).
 *                       If there is already a MapID element with \<localID> and \<ident>,
 *                       it will be updated, else created.
 *
 *  @return  error code, if this MapID can't be updated (e.g. not yet existing).
 *
 *  USED ONLY WITH \<plugin_datastoredadmin>
 */
_ENTRY_ TSyError UpdateMapItem( CContext aContext, cMapID mID );


/*! Map table handling: Delete a map item of this context
 *
 *  @param  <aContext>   The datastore context
 *  @param  <mID>        MapID ( with \<localID>,\<remoteID>, \<flags> and \<ident> ).
 *
 *  @return  error code, if this MapID can't deleted,
 *                    or if this MapID does not exist.
 *
 *  USED ONLY WITH \<plugin_datastoredadmin>
 */
_ENTRY_ TSyError DeleteMapItem( CContext aContext, cMapID mID );




/* -- READ ---------------------------------------------------------------------- */
/*! This routine initializes reading from the database
 *  StartDataRead must prepare the database to return the objects of this context.
 *
 *  @param  <aContext>     The datastore context.
 *  @param  <lastToken>    The value which has been returned by this module
 *                         at the last "EndDataWrite" call will be given.
 *                         It will be "", when called the first time.
 *                         Normally this token is an ISO8601 formatted string
 *                         which represents the module's current time (at the
 *                         beginning of a session). It will be used to decide at
 *                         'ReadNextItem' whether a record has been changed.
 *  @param   <resumeToken> Token for Suspend/Resume mode.
 *
 *  @return  error code
 */
_ENTRY_ TSyError StartDataRead( CContext aContext, cAppCharP   lastToken,
                                                   cAppCharP resumeToken );



/*! This routine reads the next ItemID from the database.
 *  \<allfields>    of 'ContextSupport' ( "ReadNextItem:allfields" ) and
 *  \<aFilterRules> of 'FilterSupport' must be considered.
 *  If \<aFirst> is true, the routine must return the first element (again).
 *
 *  @param  <aContext>  The datastore context.
 *  @param  <aID>       The assigned ItemID in the database;
 *                      will be ignored by the SyncML engine, if \<aStatus> = 0
 *  @param  <aItemData> The data, formatted as multiline aa:bb\<CRLF>cc:dd[\<CRLF>];
 *                      will be ignored by the SyncML engine, if \<aStatus> = 0
 *  @param  <aStatus>
 *                      - ReadItem_EOF       ( =0 ) for none ( =eof ),
 *                      - ReadItem_Changed   ( =1 ) for a changed item,
 *                      - ReadItem_Unchanged ( =2 ) for unchanged item.
 *                      - ReadItem_Resumed   ( =3 ) for a changed item (since resumed)
 *  @param  <aFirst>
 *                      - true:  the routine must return the first element
 *                      - false: the routine must return the next  element
 *
 *  @return  error code, if not ok. No datasets found is a success as well !
 *
 *
 *  NOTE:   The memory for \<aID> and \<aItemData> must be allocated locally.
 *          The SyncML engine will call 'DisposeObj' later for these objects to
 *          release the memory again. It needn't to be allocated, if \<aStatus>
 *          is ReadItem_EOF.
 *
 *  NOTE:   By default, the SyncML engine asks for \<aID> only.
 *          \<aItemData> can be returned, if anyway available or
 *          \<aItemData> must be returned, if the engine asks for it
 *          (when calling "ReadNextItem:allfields" at 'ContextSupport' with \<allfields>).
 */
_ENTRY_ TSyError ReadNextItem     ( CContext aContext, ItemID aID, appCharP *aItemData,
                                     sInt32 *aStatus,    bool aFirst );

/*! This is the equivalent to 'ReadNextItem', but using a key instead of a data string */
_ENTRY_ TSyError ReadNextItemAsKey( CContext aContext, ItemID aID,     KeyH aItemKey,
                                     sInt32 *aStatus,    bool aFirst );



/*! This routine reads the contents of a specific ItemID \<aID> from the database.
 *
 *  @param  <aContext>  The datastore context.
 *  @param  <aID>       The assigned ItemID in the database
 *  @param  <aItemData> Returns the data, formatted as multiline aa:bb\<CRLF>cc:dd[\<CRLF>]
 *
 *  @return  error code, if not ok ( e.g. invalid \<aItemID> )
 *
 *
 *  NOTE:   The memory for \<aItemData> must be allocated locally.
 *          The SyncML engine will call 'DisposeObj' later for \<aItemData>,
 *          to release again its memory.
 */
_ENTRY_ TSyError ReadItem     ( CContext aContext, cItemID aID, appCharP *aItemData );

/*! This is the equivalent to 'ReadItem', but using a key instead of a data string */
_ENTRY_ TSyError ReadItemAsKey( CContext aContext, cItemID aID,     KeyH aItemKey );



/*! This routine reads the specific binary logic block \<aID>,\<aBlobID>
 *  from the database.
 *
 *  @param  <aContext>  The datastore context.
 *  @param  <aID>       ItemID ( with \<item>,\<parent> ).
 *  @param  <aBlobID>   The assigned ID of the blob.
 *
 *  @param  <aBlkPtr>   Position and size (in bytes) of the blob block.
 *  @param  <aBlkSize>
 *                      - Input:  Maximum size (in bytes) of the blob block to be read.
 *                                If \<blkSize> is 0, the result size is not limited.
 *                      - Output: Size (in bytes) of the blob block.
 *                                \<blkSize> must not be larger than its input value.
 *  @param  <aTotSize>  Total size of the blob (in bytes), can be also 0,
 *                      if not available, e.g. for a stream.
 *
 *  @param  <aFirst>    (Input)
 *                      - true : Engine asks for the first block of this blob.
 *                      - false: Engine asks for the next  block of this blob.
 *
 *  @param  <aLast>     (Output)
 *                      - true : This is the last part (or the whole) blob.
 *                      - false: More blocks will follow.
 *
 *  @return  error code, if not ok ( e.g. invalid \<aItemID>,\<aBlobID> )
 *
 *
 *  NOTE 1) The memory at \<blkPtr>,\<blkSize> must be allocated locally.
 *          The SyncML engine will call 'DisposeObj' later for \<blkPtr>,
 *          to release the memory.
 *
 *  NOTE 2) Empty blobs are allowed, \<blkSize> and \<totSize> must be set to 0,
 *          \<blkPtr> can be undefined, \<aLast> must be true.
 *          No 'DisposeObj' call is required for this case.
 *
 *  NOTE 3) The SyncML engine can change to read another blob before
 *          having read the whole blob. It will never resume reading of this
 *          incomplete blob, but start reading again with \<aFirst> = true.
 */
_ENTRY_ TSyError ReadBlob( CContext aContext, cItemID  aID,   cAppCharP  aBlobID,
                                           appPointer *aBlkPtr, memSize *aBlkSize,
                                                                memSize *aTotSize,
                                                 bool  aFirst,     bool *aLast );



/*! This routine terminates the read from database phase
 *  It can be used e.g. for termination of a transaction.
 *  In standard case it can be implemented empty, returning simply a value LOCERR_OK = 0.
 *
 *  @param  <aContext>   The datastore context.
 *  @return  error code
 */
_ENTRY_ TSyError EndDataRead( CContext aContext );




/* -- WRITE --------------------------------------------------------------------- */
/*! This routine initializes writing to the database
 *
 *  @param  <aContext>   The datastore context.
 *  @return  error code, if not ok (e.g. invalid select options)
 */
_ENTRY_ TSyError StartDataWrite( CContext aContext );



/*! This routine inserts a new dataset to the database. The assigned
 *  new ItemID \<aId> will be returned.
 *
 *  @param  <aContext>   The datastore context.
 *  @param  <aItemData>  The data, formatted as multiline aa:bb\<CRLF>cc:dd[\<CRLF>]
 *  @param  <aID>        Database key of the new dataset.
 *
 *  @return  error code
 *             - LOCERR_OK       (   =0 ), if successful
 *             - DB_DataMerged   ( =207 ), if successful, but item actually stored
 *                                                        was updated with data from
 *                                                        external source or another
 *                                                        sync set item. Engine will
 *                                                        request updated version
 *                                                        using ReadItem.
 *                                                        (server add case only)
 *             - DB_DataReplaced ( =208 ), if successful, but item added replaces
 *                                                        another item that already
 *                                                        existed in the sync set.
 *                                                        (server add case only)
 *             - DB_Conflict     ( =409 ), if database requests engine to merge
 *                                                        existing data with the
 *                                                        to-be stored data first.
 *             - DB_Forbidden    ( =403 ), if \<aItemData> can't be resolved
 *             - DB_Full         ( =420 ), if not enough space in the DB
 *             - ... or any other SyncML error code, see Reference Manual
 *
 *  NOTE:   The memory for \<aItemID> must be allocated locally.
 *          The SyncML engine will call 'DisposeObj' later for \<aItemID>,
 *          to release the memory
 *
 */
_ENTRY_ TSyError InsertItem     ( CContext aContext, cAppCharP aItemData, ItemID aID );

/*! This is the equivalent to 'InsertItem', but using a key instead of a data string */
_ENTRY_ TSyError InsertItemAsKey( CContext aContext,      KeyH aItemKey,  ItemID aID );



/*! This routine updates an existing dataset of the database
 *
 *  @param  <aContext>   The datastore context.
 *  @param  <aItemData>  The data, formatted as multiline aa:bb\<CRLF>cc:dd[\<CRLF>]
 *  @param  <aID>        Database key of dataset to be updated
 *  @param  <updID>
 *                       - Input:  NULL is assigned as default value to
 *                                 \<updID.item> and \<updID.parent>.
 *                       - Output: The updated database key for \<aID>.
 *                                 Can be NULL,  if the same as \<aID>
 *
 *  @return  error code
 *             - LOCERR_OK    (   =0 ), if successful
 *             - DB_Forbidden ( =403 ), if \<aItemData> can't be resolved
 *             - DB_NotFound  ( =404 ), if unknown  \<aID>
 *             - DB_Full      ( =420 ), if not enough space in the DB
 *             - ... or any other SyncML error code, see Reference Manual
 *
 *
 *  NOTE:   \<updID> must either contain NULL references ( if the same as \<aID> ),
 *          or the memory for \<updID.item>,\<updID.parent> must be allocated locally.
 *          The SyncML engine will call 'DisposeObj' later for \<updID.item> and
 *          \<updID.parent> to release the memory.
 *          \<updID.parent> can be NULL, if the hierarchical model is not supported.
 */
_ENTRY_ TSyError UpdateItem     ( CContext aContext, cAppCharP aItemData, cItemID   aID,
                                                                           ItemID updID );

/*! This is the equivalent to 'UpdateItem', but using a key instead of a data string */
_ENTRY_ TSyError UpdateItemAsKey( CContext aContext,      KeyH aItemKey,  cItemID   aID,
                                                                           ItemID updID );



/*! This routine moves \<aID.item> from \<aID.parent> to \<newParID>
 *
 *  @param  <aContext>  The datastore context.
 *  @param  <aID>       ItemID ( with \<item>,\<parent> ) to be moved.
 *  @param  <newParID>  New parent ID for \<aID>
 *
 *  @return  error code
 *             - LOCERR_OK    (   =0 ), if successful
 *             - DB_NotFound  ( =404 ), if unknown  \<newParID>
 *             - DB_Full      ( =420 ), if not enough space in the DB
 *             - ... or any other SyncML error code, see Reference Manual
 */
_ENTRY_ TSyError MoveItem( CContext aContext, cItemID aID, cAppCharP newParID );



/*! This routine deletes a dataset from the database
 *
 *  @param  <aContext>  The datastore context.
 *  @param  <aID>       ItemID ( with \<item>,\<parent> ) to be deleted.
 *
 *  @return  error code
 *             - LOCERR_OK    (   =0 ), if successful
 *             - DB_NotFound  ( =404 ), if unknown \<aItemID>
 *             - ... or any other SyncML error code, see Reference Manual
 */
_ENTRY_ TSyError DeleteItem( CContext aContext, cItemID aID );




/*! This routine updates a temporary <aID> to an <updID> at the end
 *  For cached systems which assign IDs at the end of a run.
 *
 *  @param  <aContext>   The datastore context.
 *  @param  <aID>        Database key of dataset to be updated
 *  @param  <updID>
 *                       - Input:  NULL is assigned as default value to
 *                                 \<updID.item> and \<updID.parent>.
 *                       - Output: The updated database key for \<aID>.
 *                                 Can be NULL,  if the same as \<aID>
 *
 *  @return  error code
 *             - LOCERR_OK     (     =0 ), if successful
 *             - DB_Forbidden  (   =403 ), if \<aItemData> can't be resolved
 *             - DB_NotFound   (   =404 ), if unknown \<aID>
 *             - LOCERR_NOTIMP ( =20030 ), if no finalizing is needed at all
 *             - ... or any other SyncML error code, see Reference Manual
 *
 *  NOTE:   \<updID> must either contain NULL references ( if the same as \<aID> ),
 *          or the memory for \<updID.item> must be allocated locally.
 *          The SyncML engine will call 'DisposeObj' later for \<updID.item>
 *          to release the memory. \<updID.parent>should be always NULL.
 */
_ENTRY_ TSyError FinalizeLocalID( CContext aContext, cItemID aID, ItemID updID );




/*! This routine deletes all datasets from the database
 *
 *  @param  <aContext>  The datastore context.
 *
 *  @return  error code
 *             - LOCERR_OK     (     =0 ), if successful
 *             - LOCERR_NOTIMP ( =20030 ). For this case, the engine removes all items directly
 *             - ... or any other SyncML error code, see Reference Manual
 */
_ENTRY_ TSyError DeleteSyncSet( CContext aContext );




/*! This routine writes the specific binary logic block \<blobID> to the database.
 *
 *  @param  <aContext>  The datastore context.
 *  @param  <aID>       ItemID ( with \<item>,\<parent> ).
 *  @param  <aBlobID>   The assigned ID of the blob.
 *
 *  @param  <aBlkPtr>
 *  @param  <aBlkSize>  Position and size (in bytes) of the blob block.
 *  @param  <aTotSize>  Total size of the blob (in bytes),
 *                      Can be also 0, if not available, e.g. for a stream.
 *
 *  @param  <aFirst>
 *                      - true : this is the first block of the blob.
 *                      - false: this is the next  block.
 *  @param  <aLast>
 *                      - true : this is the last  block.
 *                      - false: more blocks will follow.
 *
 *  @return  error code, if not ok ( e.g. invalid \<aID>,\<aBlobID> )
 *
 *  NOTE:   Empty blobs are possible, \<blkSize> and \<totSize> will be set to 0,
 *          \<blkPtr> will be NULL, \<aFirst> and \<aLast> will be true.
 */
_ENTRY_ TSyError WriteBlob( CContext aContext, cItemID aID,   cAppCharP aBlobID,
                                            appPointer aBlkPtr, memSize aBlkSize,
                                                                memSize aTotSize,
                                                  bool aFirst,     bool aLast );


/*! This routine deletes the specific binary logic block \<blobID> at the database.
 *
 *  @param  <aContext>  The datastore context.
 *  @param  <aID>       ItemID ( with \<item>,\<parent> ).
 *  @param  <aBlobID>   The assigned ID of the blob.
 *
 *  @return  error code, if not ok ( e.g. invalid \<aID>,\<aBlobID> )
 */
_ENTRY_ TSyError DeleteBlob( CContext aContext, cItemID aID, cAppCharP aBlobID );


/*! Advises the database to finsish the running transaction
 *
 *  @param  <aContext>   The datastore context.
 *  @param  <success>
 *                       - true:  All former actions were successful,
 *                                so the database can commit
 *                       - false: The transaction was not successful,
 *                                so the database may rollback or ignore the transaction.
 *
 *  @param  <newToken>   An internally generated string value, which
 *                       will be used to identify changed database records.
 *                       It is normally an ISO8601 formatted string, which
 *                       represents the module's current time (at the
 *                       time the 'StartDataRead' of this context has been
 *                       called). All changed records of the currrent context
 *                       must get this token as timestamp as as well.
 *                       The SyncML engine will return this value with the
 *                       'StartDataRead' call within the next session.
 *                       It must return NULL in case of no \<success>.
 *
 *  @return  error code, if operation can't be performed. No \<success> is not an error.
 *
 *
 *  NOTE: By default, the SyncML engine expects an ISO8601 string for \<newToken>.
 *        But the SyncML engine can be configured to treat this value completely
 *        opaque, if implemented in a different way.
 *
 *        The \<newToken> must be allocated locally and will be
 *        disposed with a 'DisposeObj' call later by the SyncML engine.
 */
_ENTRY_ TSyError EndDataWrite( CContext aContext, bool success, appCharP *newToken );



/* ---- ADAPT ITEM -------------------------------------------------------------- */
/*! This function adapts aItemData
 *
 *  @param  <aContext>      The datastore context
 *  @param  <aItemData1>    The 1st item's data
 *  @param  <aItemData2>    The 2nd item's data
 *  @param  <aLocalVars>    The local vars
 *  @param  <aIdentifier>   To identify, where it is called
 *
 *  @return  error code
 *
 *  NOTE:   The memory for adapted strings must be allocated locally.
 *          The SyncML engine will call 'DisposeObj' later, to release again its memory.
 *          One or more strings can be returned unchanged as well.
 */
_ENTRY_ TSyError AdaptItem( CContext aContext, appCharP *aItemData1,
                                               appCharP *aItemData2,
                                               appCharP *aLocalVars,
                                                 uInt32  aIdentifier );


/* -- DISPOSE / CLOSE ----------------------------------------------------------- */
/*! Disposes memory, which has been allocated within the datastore context.
 *  'DisposeObj' can occur at any time within \<aContext>.
 *
 *  @param  <aContext>  The datastore context.
 *  @param  <memory>    Dispose allocated memory.
 *
 *  @return  -
 *
 */
_ENTRY_ void DisposeObj( CContext aContext, void* memory );



/*! This routine is called to delete a context, that was previously created with
 *  'CreateContext'. The DB Module must free all resources related to this context.
 *  No calls with \<aContext> will be done after calling this routine, so the
 *  assigned structure, allocated at 'CreateContext' can be released here.
 *
 *  @param  <aContext>   The datastore context.
 *
 *  @result  error code, if context could not be deleted ( e.g. not existing \<aContext> ).
 */
_ENTRY_ TSyError DeleteContext( CContext aContext );


#endif
/* eof */
