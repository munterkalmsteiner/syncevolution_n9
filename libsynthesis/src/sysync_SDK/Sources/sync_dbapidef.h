/*
 *  File:    sync_dbapidef.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *  C/C++ Programming interface between
 *        the Synthesis SyncML engine
 *        and the database layer
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 *
 * These are the definitions for the calling interface between the
 * Synthesis SyncML engine and a customized module "sync_dbapi".
 * The same interface can be used either for Standard C or for C++.
 * Normally the customized module will be compiled as DLL and will
 * be called by the SyncML engine. A linkable library is available
 * (for C++) as well.
 *
 */

/* NOTE: As this file should work for all kind of plain C compilers
 *       comments with double slashes "//" must be avoided !!
 */

#ifndef SYNC_DBAPIDEF_H
#define SYNC_DBAPIDEF_H

#include "sync_include.h"
#include "sync_declarations.h"

/* export prefix and suffix is dependent on platform and source language */
#ifdef __cplusplus
  #define ENTRY_C extern "C"
#else
  #define ENTRY_C
#endif

#if defined MACOSX
  #define ENGINE_ENTRY ENTRY_C
  #define ENGINE_ENTRY_CXX

  #ifdef __GNUC__
    #define ENTRY_ATTR __attribute__((visibility("default")))
  #else
    #define ENTRY_ATTR
  #endif
#elif defined _MSC_VER
  /* Visual Studio 2005 requires a specific entry point definition */
  /* This definition is empty for all other platforms */
  #define ENGINE_ENTRY ENTRY_C _declspec(dllexport)
  #define ENGINE_ENTRY_CXX ENGINE_ENTRY
  #define ENTRY_ATTR
#else
  #define ENGINE_ENTRY ENTRY_C
  #define ENGINE_ENTRY_CXX
  #define ENTRY_ATTR
#endif

/* compose name of external symbols with C binding:
 * by default use parameter as-is (backwards compatibility) for client,
 * and use "SySync_srv_" prefix for server.
 * Newer builds usually use "SySync_" as prefix for client entry points.
 * can be changed in any file that is included before
 * this file
 */
#ifndef SYSYNC_EXTERNAL
# define SYSYNC_EXTERNAL(_x) _x
# define SYSYNC_PREFIX ""
#endif
#ifndef SYSYNC_EXTERNAL_SRV
# define SYSYNC_EXTERNAL_SRV(_x) SySync_srv_ ## _x
# define SYSYNC_PREFIX_SRV "SySync_srv_"
#endif

#ifdef __cplusplus
  namespace sysync {
#endif


/* ---- Plugin versions ------------------------------------------------------------------------------- */
/* To make the SyncML engine and the Plugin SDK upwards/downwards compatible to each other,
 * some version info must be checked and compared: VP_XXX is the min version of the plugin to
 * support a certain feature, VE_XXX is the min version of the engine to support a feature.
 * The plugin will get the engine's version with "Plugin_Params".
 * The version number for engine AND plugin is defined as SDK_VERSION/SUBVERSION at "SDK_util.c"
 * Normally, the version number of the engine must be equal or higher than the plugin's version.
 */

/* ---- Plugin support ---- */
/* For the first versions, "N" is the platform identifier */
enum Version {
/*VP_Init                  0x01000000   * V1.0.N.0 : Initial version */
/*VP_1st                   0x01000002   * V1.0.N.2 : 1st delivered version */
/*VP_Session_Login         0x01000003   * V1.0.N.3 : VAR_String for Session_Login: this version only */
/*VP_EngineVersionParam    0x01000004   * V1.0.N.4 : <engineVersion> param for "Module_PluginParams" */
/*VP_CB_Version2         = 0x01000005   * V1.0.N.5 : Callback version >= 2 supported */

  /** V1.0.N.6 : With new function "DeleteBlob"                  */
  VP_DeleteBlob          = 0x01000006,
  /** V1.0.N.7 : With new "scripting" function "AdaptItemData"   */
  VP_AdaptItemData       = 0x01000007,
  /** V1.0.N.8 : Supports "InsertMapItem"                        */
  VP_InsertMapItem       = 0x01000008,

/* From now on, the platform info can be found at he capabilities, "X" is a user defined build nr */
  /** V1.0.5.X : "X": With customer defined plugin build number  */
  VP_NewBuildNumber      = 0x01000500,
  /** V1.0.6.X : Suspend/Resume extension for 'StartDataRead'    */
  VP_ResumeToken         = 0x01000600,
  /** V1.0.8.X : "Session_GetDBTime" supported                   */
  VP_GetDBTime           = 0x01000800,
  /** V1.0.9.X : Callback version >= 3 supported: exotic         */
  VP_CB_Version3         = 0x01000900,
  /** V1.0.10.X: Callback version >= 4 supported: allowDLL       */
  VP_CB_Version4         = 0x01000a00,
  /** V1.0.10.X: With enhanced "scripting" function "AdaptItem"  */
  VP_AdaptItem           = 0x01000a00,

/* The new SyncML engine V2.9.X/V3.X.X runs best with versions from here onwards */
  /** V1.1.0.X : Enhanced Plugin Info support: Server > 2.9.5.0  */
  VP_Plugin_Info         = 0x01010000,
  /** V1.2.1.X : With additional password mode + JNI adaption    */
  VP_MD5_Nonce_IN        = 0x01020100,
/*VP_061012                0x01020200   * V1.2.2 : Released 12-Oct-06 */
/*VP_061207                0x01030000   * V1.3.0 : Released 07-Dec-06, Visual Studio support */
  /** V1.3.1.X : With UI function support                        */
  VP_UI_Support          = 0x01030100,
/*VP_070131                0x01030200   * V1.3.2 : Released 31-Jan-07 */
/*VP_070201                0x01030300   * V1.3.3 : Released 01-Feb-07 */
  /** V1.3.4.X : Java Callback with <cContext>                   */
  VP_Call_cContext       = 0x01030400,
  /** V1.3.4.X : Callback version >= 7 supported: UI / thisCB    */
  VP_CB_Version7         = 0x01030400,
/*VP_070212                0x01030600   * V1.3.6.X : Released 12-Feb-07 */
  /** V1.3.8.X : Support for GlobContext structure               */
  VP_GlobContext         = 0x01030800,
  /** V1.3.9.X : Callback version >= 8 supported: extended UI    */
  VP_CB_Version8         = 0x01030900,
  /** V1.4.0.X : With UI application support / Beta release      */
  VP_UIApp               = 0x01040000,
  /** V1.4.7.X : Callback version >= 9 supported: dbapi tunnel   */
  VP_CB_Version9         = 0x01040700,
  /** V1.4.8.X : "FinalizeLocalID"/"DeleteSyncSet" support.      */
  VP_FLI_DSS             = 0x01040800,
  /** V1.5.0.X : Released version Aug 2008                       */
  VP_Release_1_5_0       = 0x01050000,
  /** V1.5.1.X : Multiple global contexts supported              */
  VP_GlobMulti           = 0x01050100,
  /** V1.5.2.X : Callback version >= 11 supported: dbapi tunnel  */
  VP_CB_Version11        = 0x01050200,
  /** V1.6.0.X : Tunnel support                                  */
  VP_Tunnel              = 0x01060000,
  /** V1.6.1.X : Correct SetValue support                        */
  VP_SetValue            = 0x01060100,
  /** V1.6.2.X : 64 bit Java BLOB signature                      */
  VP_BLOB_JSignature     = 0x01060200,
/*VP_091221                0x01060200   * V1.6.2 : Released 21-Dec-09 */
  /** V1.7.0.X : First release of the Android client             */
  VP_Android_1st_Release = 0x01070000,
/*VP_100429                0x01070000   * V1.7.0 : Released 29-Apr-10 */
/*                         0x01070200   * V1.7.2 : never released as SDK */
  VP_ADMIN_AS_KEY        = 0x01080000,
/*VP_110131              = 0x01090000   * V1.9.0 : Released 31-Jan-11 (Android) */

  /** V1.9.1.X : Current version, use 'Plugin_Version()'         */
  VP_CurrentVersion      = 0x01090100,

  /** -------- : Bad/undefined version                           */
  VP_BadVersion          = 0xffffffff,

/* ---- Engine support ---- */
  /** V1.0.7.0 : Engine supports "InsertMapItem"                 */
  VE_InsertMapItem       = 0x01000700
};



/* ---- Function names -------------------------------------------------------------------------------- */
/* ---- module ---- */
#define Mo_CC                "Module_CreateContext"
#define Mo_Ve                "Module_Version"
#define Mo_Ca                "Module_Capabilities"
#define Mo_PP                "Module_PluginParams"

#define Mo_DO                "Module_DisposeObj"
#define Mo_DC                "Module_DeleteContext"

/* ---- session ---- */
#define Se_CC                "Session_CreateContext"
#define Se_TC                "Session_ThreadMayChangeNow"
#define Se_DI                "Session_DispItems"

#define Se_CD                "Session_CheckDevice"
#define Se_GN                "Session_GetNonce"
#define Se_SN                "Session_SaveNonce"
#define Se_SD                "Session_SaveDeviceInfo"
#define Se_GT                "Session_GetDBTime"

#define Se_PM                "Session_PasswordMode"
#define Se_LI                "Session_Login"
#define Se_LO                "Session_Logout"

#define Se_DO                "Session_DisposeObj"
#define Se_DC                "Session_DeleteContext"

/* ---- datastore ---- */
#define Da_CC                "CreateContext"
#define Da_CS                "ContextSupport"
#define Da_FS                "FilterSupport"

#define Da_LA                "LoadAdminData"
#define Da_LAK               "LoadAdminDataAsKey"
#define Da_SA                "SaveAdminData"
#define Da_SAK               "SaveAdminDataAsKey"
#define Da_RM                "ReadNextMapItem"
#define Da_IM                "InsertMapItem"
#define Da_UM                "UpdateMapItem"
#define Da_DM                "DeleteMapItem"

#define Da_TC                "ThreadMayChangeNow"
#define Da_DI                "DispItems"
#define Da_WL                "WriteLogData"

#define Da_SR                "StartDataRead"
#define Da_RN                "ReadNextItem"
#define Da_RNK               "ReadNextItemAsKey"
#define Da_RI                "ReadItem"
#define Da_RIK               "ReadItemAsKey"
#define Da_RB                "ReadBlob"
#define Da_ER                "EndDataRead"

#define Da_SW                "StartDataWrite"
#define Da_II                "InsertItem"
#define Da_IIK               "InsertItemAsKey"
#define Da_UI                "UpdateItem"
#define Da_UIK               "UpdateItemAsKey"
#define Da_MvI               "MoveItem"
#define Da_DeI               "DeleteItem"
#define Da_DSS               "DeleteSyncSet"
#define Da_FLI               "FinalizeLocalID"
#define Da_WB                "WriteBlob"
#define Da_DB                "DeleteBlob"
#define Da_EW                "EndDataWrite"

#define Da_DO                "DisposeObj"
#define Da_DC                "DeleteContext"



/* ---- Predefined capability names ------------------------------------------------------------------- */
#define CA_MinVersion        "MINVERSION"   /* Minimum requested engine version by the plugin */
#define CA_SubVersion        "SUBVERSION"   /* version of the sub system */
#define CA_SubSystem         "SUBSYSTEM"    /* Name of the sub system, used as capability separator */
#define CA_Manufacturer      "MANUFACTURER" /* Name of the plugin manufacturer */
#define CA_Description       "DESCRIPTION"  /* A short description, what the plugin module is doing */
#define CA_Platform          "PLATFORM"     /* The software platform, e.g. "Windows", "Java", ... */
#define CA_DLL               "DLL"          /* Indicates, if plugin is a DLL */
#define CA_JNI               "JNI"          /* Indicates, if plugin is based on JNI */
#define CA_CSHARP            "C#"           /* Indicates, if plugin is based on C#  */
#define CA_GUID              "GUID"         /* GUID */
#define CA_Plugin            "PLUGIN"       /* Built-In plugin name */
#define CA_GlobContext       "GlobContext"  /* The global context, if available */
#define CA_ADMIN_Info        "ADMIN_Info"   /* Get ADMIN info as <name><SP>"ADMIN" with 'CreateContext' */
                                            /*   (supported for V1.3.7 and higher) */
#define CA_ItemAsKey         "ITEM_AS_KEY"  /* Supports the AsKey" mode */
#define CA_AdminAsKey        "ADMIN_AS_KEY" /* Supports the AsKey" mode for Load/SaveAdminData */
#define CA_DeleteSyncSet     "DeleteSyncSet"/* DeleteSyncSet ist fully implemented */
#define CA_Error             "ERROR"        /* Capability error */

/* Predefined identifiers */
#define ADMIN_Ident          " ADMIN"       /* Appended identifier for admin datastore recognition */
                                            /* Activated with capability CA_ADMIN_Info */



/* ---- Allow to switch off certain parts of the plug-in module --------------------------------------- */
#define Plugin_Start         "plugin_start"
#define Plugin_Param         "plugin_param"

#define Plugin_Session       "plugin_se"
#define Plugin_SE_Adapt      "plugin_sessionadapt"
#define Plugin_SE_Auth       "plugin_sessionauth"
#define Plugin_DV_Admin      "plugin_deviceadmin"
#define Plugin_DV_DBTime     "plugin_dbtime"

#define Plugin_Datastore     "plugin_ds"
#define Plugin_DS_General    "plugin_datageneral"
#define Plugin_DS_Admin      "plugin_datastoreadmin"
#define Plugin_DS_Admin_Str  "plugin_datastoreadmin_str"
#define Plugin_DS_Admin_Key  "plugin_datastoreadmin_key"
#define Plugin_DS_Admin_Map  "plugin_datastoreadmin_map"
#define Plugin_DS_Data       "plugin_datastore"
#define Plugin_DS_Data_Str   "plugin_datastore_str"
#define Plugin_DS_Data_Key   "plugin_datastore_key"
#define Plugin_DS_Blob       "plugin_datablob"
#define Plugin_DS_Adapt      "plugin_dataadapt"

#define Plugin_UI            "plugin_ui"


/* Compatibility to older versions */
#define Plugin_SE_Auth_OLD   "plugin_sessionauth_OLD"
#define Plugin_DS_Data_OLD1  "plugin_datastore_OLD1"
#define Plugin_DS_Data_OLD2  "plugin_datastore_OLD2"
#define Plugin_DS_Admin_OLD  "plugin_datastoreadmin_OLD"
#define Plugin_DS_Blob_OLD1  "plugin_datablob_OLD1"
#define Plugin_DS_Blob_OLD2  "plugin_datablob_OLD2"



/* ---------------------------------------------------------------------------------------------------- */
/*! Item and its special definitions */
#define    StdPattern ":"           /* std    identifier */
#define  ArrayPattern "["           /* array  identifier */
#define   BlobPattern ";BLOBID="    /* blob   identifier */
#define TZNamePattern ";TZNAME="    /* tzname identifier */
#define TZOffsPattern ";TZOFFS="    /* tzoffs identifier */


/*! Result value of Session_PasswordMode */
enum PasswordMode {
  Password_Mode_Undefined  = -1, /* undefined mode */
  Password_ClrText_IN      =  0,
  Password_ClrText_OUT     =  1,
  Password_MD5_OUT          = 2, /* MD5B64(user:pwd) */
  Password_MD5_Nonce_IN     = 3, /* MD5B64(MD5B64(user:pwd):nonce) */
};

/*! Result status of ReadNextItem */
enum ReadNextItemResult {
  ReadNextItem_EOF          = 0, /* no more items to read */
  ReadNextItem_Changed      = 1,
  ReadNextItem_Unchanged    = 2,
  ReadNextItem_Resumed      = 3, /* OMA DS 1.2 suspend/resume support */
};


/*! Structure of hierarchical item ID */
struct ItemIDType {
  appCharP item;
  appCharP parent;
};

/*! Structure of map ID */
struct MapIDType {
  appCharP  localID;
  appCharP remoteID;
  uInt16   flags;
  uInt8    ident;
};

/*! Structure of GlobContext */
struct GlobContext {
  void*               ref;           /* reference field */
  struct GlobContext* next;          /* reference to the next GlobContext structure */
  uInt32              cnt;           /* link count */
  char                refName[ 80 ]; /* the reference's name, length restricted */
};

#define GlobContext_JavaVM "JavaVM"


/*! Undefined function for place holder at 'ConnectFunctions', must have pointer size */
#define XX (char *)-1



/* ---- CALLBACK / CALL-IN -------------------------------------------------------- */
/*! The callback structure allows logging of debug information within the
 *  files of the Synthesis SyncML engine. 'CreateContext' for module, session
 *  and datastore will pass a reference to such a struct.
 *  The same structure contains all call-in functions for the UI Api.
 */

/**
 * <debugFlags>: Reserved plugin specific bit masks.
 * These flags are only used by the SDK_util logging helper functions.
 * They are *not* passed into the Synthesis engine and thus cannot
 * control how the logged text is stored in the log file.
 */
enum DebugFlags {
  /** No debugging */
  DBG_PLUGIN_NONE  = 0x0000,
  /** Engine internal calls for plugin interface */
  DBG_PLUGIN_INT   = 0x0001,
  /** DB access calls, standard for all plugin examples */
  DBG_PLUGIN_DB    = 0x0002,
  /** DB access calls, exotic calls as well */
  DBG_PLUGIN_EXOT  = 0x0004,
  /** direct printf calls for test */
  DBG_PLUGIN_DIRECT= 0x0008,
  /** GetValue/SetValue debugging */
  DBG_GET_SET_VALUE= 0x0010,
  /** Default mask: all bits set */
  DBG_PLUGIN_ALL   = 0xffff,
};


/*! Function prototypes for Debug calls */
typedef void     (*DB_DebugPuts_Func)     ( void* aCallbackRef, cAppCharP aText );
typedef void     (*DB_DebugExotic_Func)   ( void* aCallbackRef, cAppCharP aText );
typedef void     (*DB_DebugBlock_Func)    ( void* aCallbackRef, cAppCharP aTag,
                                                                cAppCharP aDesc,
                                                                cAppCharP aAttrText );
typedef void     (*DB_DebugEndBlock_Func) ( void* aCallbackRef, cAppCharP aTag );
typedef void     (*DB_DebugEndThread_Func)( void* aCallbackRef );


/* Function prototypes for UI call-in */
/* ---- Engine init ---- */
typedef TSyError (*SetStringMode_Func)    ( void* aCB,     uInt16 aCharSet,
                                                           uInt16 aLineEndMode,
                                                             bool aBigEndian );
typedef TSyError (*InitEngineXML_Func)    ( void* aCB,  cAppCharP aConfigXML );
typedef TSyError (*InitEngineFile_Func)   ( void* aCB,  cAppCharP aConfigFilePath );
typedef TSyError (*InitEngineCB_Func)     ( void* aCB, TXMLConfigReadFunc aReaderFunc, void* aContext );

/* ---- Running a Sync Session ---- */
typedef TSyError (*OpenSession_Func)      ( void* aCB, SessionH *aSessionH, uInt32  aSelector,
                                                                         cAppCharP  aSessionName );
typedef TSyError (*OpenSessionKey_Func)   ( void* aCB, SessionH  aSessionH,
                                                           KeyH *aKeyH,     uInt16  aMode );
typedef TSyError (*SessionStep_Func)      ( void* aCB, SessionH  aSessionH, uInt16 *aStepCmd,
                                                               TEngineProgressInfo *aInfoP );
typedef TSyError (*GetSyncMLBuffer_Func)  ( void* aCB, SessionH  aSessionH,   bool  aForSend,
                                                     appPointer *aBuffer,  memSize *aBufSize );
typedef TSyError (*RetSyncMLBuffer_Func)  ( void* aCB, SessionH  aSessionH,   bool  aForSend,
                                                                           memSize  aRetSize );
typedef TSyError (*ReadSyncMLBuffer_Func) ( void* aCB, SessionH  aSessionH,
                                                     appPointer  aBuffer,  memSize  aBufSize,
                                                                           memSize *aValSize );
typedef TSyError (*WriteSyncMLBuffer_Func)( void* aCB, SessionH  aSessionH,
                                                     appPointer  aBuffer,  memSize  aValSize );
typedef TSyError (*CloseSession_Func)     ( void* aCB, SessionH  aSessionH );

/* ---- Settings access ---- */
typedef TSyError (*OpenKeyByPath_Func)    ( void* aCB, KeyH *aKeyH,
                                                       KeyH  aParentKeyH, cAppCharP aPath, uInt16 aMode );
typedef TSyError (*OpenSubkey_Func)       ( void* aCB, KeyH *aKeyH,
                                                       KeyH  aParentKeyH, sInt32    aID,   uInt16 aMode );
typedef TSyError (*DeleteSubkey_Func)     ( void* aCB, KeyH  aParentKeyH, sInt32    aID );
typedef TSyError (*GetKeyID_Func)         ( void* aCB, KeyH  aKeyH,       sInt32   *aID );
typedef TSyError (*SetTextMode_Func)      ( void* aCB, KeyH  aKeyH,       uInt16 aCharSet,
                                                                          uInt16 aLineEndMode, bool aBigEndian );
typedef TSyError (*SetTimeMode_Func)      ( void* aCB, KeyH  aKeyH,       uInt16 aTimeMode );
typedef TSyError (*CloseKey_Func)         ( void* aCB, KeyH  aKeyH );

typedef TSyError (*GetValue_Func)         ( void* aCB, KeyH  aKeyH, cAppCharP  aValName,  uInt16  aValType,
                                                 appPointer  aBuffer, memSize  aBufSize, memSize *aValSize );
typedef TSyError (*GetValueByID_Func)     ( void* aCB, KeyH  aKeyH,    sInt32  aID,       sInt32  arrIndex,
                                                                       uInt16  aValType,
                                                 appPointer  aBuffer, memSize  aBufSize, memSize *aValSize );
typedef sInt32   (*GetValueID_Func)       ( void* aCB, KeyH  aKeyH, cAppCharP  aName );

typedef TSyError (*SetValue_Func)         ( void* aCB, KeyH  aKeyH, cAppCharP  aValName,  uInt16  aValType,
                                                cAppPointer  aBuffer, memSize  aValSize );
typedef TSyError (*SetValueByID_Func)     ( void* aCB, KeyH  aKeyH,    sInt32  aID,       sInt32  arrIndex,
                                                                       uInt16  aValType,
                                                cAppPointer  aBuffer, memSize  aValSize );

/* ---- Function prototypes for DBApi ------------------------------------------------------------------- */
typedef TSyError     (*SDR_Func)( CContext ac, cAppCharP lastToken, cAppCharP  resumeToken );
typedef TSyError (*RdNItemSFunc)( CContext ac,    ItemID aID,        appCharP *aItemData,
                                                                       sInt32 *aStatus, bool aFirst );
typedef TSyError (*RdNItemKFunc)( CContext ac,    ItemID aID,            KeyH  aItemKey,
                                                                       sInt32 *aStatus, bool aFirst );
typedef TSyError (*Rd_ItemSFunc)( CContext ac,   cItemID aID,        appCharP *aItemData );
typedef TSyError (*Rd_ItemKFunc)( CContext ac,   cItemID aID,            KeyH  aItemKey );
typedef TSyError     (*EDR_Func)( CContext ac );

typedef TSyError     (*SDW_Func)( CContext ac );
typedef TSyError (*InsItemSFunc)( CContext ac, cAppCharP aItemData,  ItemID aID );
typedef TSyError (*InsItemKFunc)( CContext ac,      KeyH aItemKey,   ItemID aID );
typedef TSyError (*UpdItemSFunc)( CContext ac, cAppCharP aItemData, cItemID aID,    ItemID    updID );
typedef TSyError (*UpdItemKFunc)( CContext ac,      KeyH aItemKey,  cItemID aID,    ItemID    updID );
typedef TSyError (*MovItem_Func)( CContext ac,                      cItemID aID, cAppCharP newParID );
typedef TSyError (*DelItem_Func)( CContext ac,                      cItemID aID );
typedef TSyError     (*FLI_Func)( CContext ac,                      cItemID aID,    ItemID    updID );
typedef TSyError   (*DelSS_Func)( CContext ac );
typedef TSyError     (*EDW_Func)( CContext ac, bool success, appCharP *newToken );

typedef void      (*DisposeProc)( CContext xContext, void* memory );



/* ------------------------------------------------------------------------------------------------------ */
/*!< UI Api methods
 * NOTE: As this struct is part of the SDK_Interface_Struct, it is frozen and not directly extendable
 */
typedef struct {
  SetStringMode_Func      SetStringMode;
  InitEngineXML_Func      InitEngineXML;
  InitEngineFile_Func     InitEngineFile;
  InitEngineCB_Func       InitEngineCB;

  OpenSession_Func        OpenSession;
  OpenSessionKey_Func     OpenSessionKey;
  SessionStep_Func        SessionStep;
  GetSyncMLBuffer_Func    GetSyncMLBuffer;
  RetSyncMLBuffer_Func    RetSyncMLBuffer;
  ReadSyncMLBuffer_Func   ReadSyncMLBuffer;
  WriteSyncMLBuffer_Func  WriteSyncMLBuffer;
  CloseSession_Func       CloseSession;

  OpenKeyByPath_Func      OpenKeyByPath;
  OpenSubkey_Func         OpenSubkey;
  DeleteSubkey_Func       DeleteSubkey;
  GetKeyID_Func           GetKeyID;
  SetTextMode_Func        SetTextMode;
  SetTimeMode_Func        SetTimeMode;
  CloseKey_Func           CloseKey;

  GetValue_Func           GetValue;
  GetValueByID_Func       GetValueByID;
  GetValueID_Func         GetValueID;
  SetValue_Func           SetValue;
  SetValueByID_Func       SetValueByID;
} SDK_UI_Struct;


/*! The SDK dbapi tunnel structure (for internal use only)
 * NOTE: As this struct is part of the SDK_Interface_Struct, it is frozen and not directly extendable
 */
typedef struct {
      SDR_Func            StartDataRead;
  RdNItemSFunc            ReadNextItem;
  Rd_ItemSFunc            ReadItem;
      EDR_Func            EndDataRead;

      SDW_Func            StartDataWrite;
  InsItemSFunc            InsertItem;
  UpdItemSFunc            UpdateItem;
  MovItem_Func            MoveItem;
  DelItem_Func            DeleteItem;
      EDW_Func            EndDataWrite;

   DisposeProc            DisposeObj;

  /* --- asKey functions */
  RdNItemKFunc            ReadNextItemAsKey;
  Rd_ItemKFunc            ReadItemAsKey;
  InsItemKFunc            InsertItemAsKey;
  UpdItemKFunc            UpdateItemAsKey;
} SDK_Tunnel_Struct;



enum {
  /** Current callback version */
  DB_Callback_Version = 11
};

/*! The SDK interface structure for callback and call-in */
typedef struct SDK_InterfaceType {
  uInt16                  callbackVersion;   /*!< The version of this callback struct */
  appPointer              callbackRef;       /*!< The opaque pointer that must be returned in
                                              *   the \<callbackRef> argument of all callbacks
                                              */

  /* ---- callbackVersion>=1 fields ---- */
  uInt16                  debugFlags;        /*!< Debug control flags: if<>0, debug is enabled */
  DB_DebugPuts_Func       DB_DebugPuts;      /*!< Output prodedure to add things to the debug log */

  CContext                cContext;          /*!< Callback C/C++ context, normally identical with
                                              *   \<mContext>; different e.g. for JNI
                                              */

  CContext                mContext;          /*!< Module  context */
  CContext                sContext;          /*!< Session context */

  /* ---- callbackVersion>=2 fields ---- */
  DB_DebugBlock_Func      DB_DebugBlock;     /*!< Output proc to open  a named (=aTag) structure block in the log */
  DB_DebugEndBlock_Func   DB_DebugEndBlock;  /*!< Output proc to close a prev. opened named structure block in the log */
  DB_DebugEndThread_Func  DB_DebugEndThread; /*!< Output proc to signal debug logging system that no more debug output
                                              *   will come from the calling thread
                                              */
  uInt32                  lCount;            /*!< Level counter, for internal use only */

  /* ---- callbackVersion>=3 fields ---- */
  DB_DebugExotic_Func     DB_DebugExotic;    /*!< Output prodedure to add exotic things to the debug log */

  /* ---- callbackVersion>=4 fields ---- */
  uInt16                  allow_DLL_legacy;  /*!< Legacy, because of alignment problems
                                              *   (as bool) on some platforms,
                                              *   Use <allow_DLL> for new applications.
                                              */

  /* ---- callbackVersion>=5 fields ---- */
  uInt16                  allow_DLL;         /*!< DLLs can be used */

  /* ---- callbackVersion>=6 fields ---- */
  uInt32                  reserved1;         /*!< (Legacy UI_* funcs, no longer used) */
  uInt32                  reserved2;
  uInt32                  reserved3;
  uInt32                  reserved4;

  /* ---- callbackVersion>=7 fields ---- */
  appPointer              thisCB;            /*!< a reference to this structure directly */
  uInt32                  logCount;          /*!< Incremental counter for logs */

  /* ---- callbackVersion>=8 fields ---- */
  appPointer              thisBase;          /*!< <this> base for abstract method calling */
  appPointer              jRef;              /*!< <this> base for java access */
  CContext                gContext;          /*!< common global context */
  SDK_UI_Struct           ui;                /*!< UI Api methods */

  /* ---- callbackVersion>=9 fields ---- */
  memSize                 SDK_Interface_size;                   /* size of this structure */
  SDK_Tunnel_Struct       dt;   /* dbapi tunnel callback functions, for internal use only */

  /* ---- callbackVersion>=10 fields ---- */
  /* no additional fields, just information that SUBSYSTEM (CA_SubSystem) is supported */

  /* ---- callbackVersion>=11 fields ---- */
  /* SDK_Tunnel_Struct extended (at the end) with "asKey" functions */

  /* ---- callbackVersion>=12 fields will go here ---- */
  /* ... */
} SDK_Interface_Struct, *DB_Callback, *UI_Call_In;


/* special case for <callbackVersion> = 8, higher versions contain a size field */
#define SDK_Interface_Struct_V8         8
#define SDK_Interface_Struct_V8_Size  184



/*! Wrapper for tunnel context callback */
struct TunnelWrapper {
  UI_Call_In tCB;          /* the Call-In reference */
  SessionH   tContext;     /* the tunnel session context */
  appCharP   tContextName; /* the tunnel session context name, needed for reopen in context */
  KeyH       tItemKey;     /* an item key element for AsKey operations */
  uInt16     tAfterWrite;  /* is true after first Insert/Update/Move/Delete */
}; /* Tunnelwrapper */



/* -------------------------------------------------------------------------------------- */
/*! Function prototypes for engine connection/disconnection */
typedef TSyError (*ConnectEngine_Func)    ( UI_Call_In *aCI,
                                            CVersion   *aEngVersion,
                                            CVersion    aPrgVersion,
                                            uInt16      aDebugFlags );

typedef TSyError (*ConnectEngineS_Func)   ( UI_Call_In  aCI,
                                            uInt16      aCallbackVersion,
                                            CVersion   *aEngVersion,
                                            CVersion    aPrgVersion,
                                            uInt16      aDebugFlags );

typedef TSyError (*DisconnectEngine_Func) ( UI_Call_In  aCI );



/* Entry point for connecting the SyncML engine as client from outside */
 ENGINE_ENTRY TSyError SYSYNC_EXTERNAL(ConnectEngine)
                                          ( UI_Call_In *aCI,
                                            CVersion   *aEngVersion,
                                            CVersion    aPrgVersion,
                                            uInt16      aDebugFlags ) ENTRY_ATTR;

 ENGINE_ENTRY TSyError SYSYNC_EXTERNAL(ConnectEngineS)
                                          ( UI_Call_In  aCI,
                                            uInt16      aCallbackVersion,
                                            CVersion   *aEngVersion,
                                            CVersion    aPrgVersion,
                                            uInt16      aDebugFlags ) ENTRY_ATTR;

/* Entry point for connecting the SyncML engine as server from outside */
 ENGINE_ENTRY TSyError SYSYNC_EXTERNAL_SRV(ConnectEngine)
                                          ( UI_Call_In *aCI,
                                            CVersion   *aEngVersion,
                                            CVersion    aPrgVersion,
                                            uInt16      aDebugFlags ) ENTRY_ATTR;

 ENGINE_ENTRY TSyError SYSYNC_EXTERNAL_SRV(ConnectEngineS)
                                          ( UI_Call_In  aCI,
                                            uInt16      aCallbackVersion,
                                            CVersion   *aEngVersion,
                                            CVersion    aPrgVersion,
                                            uInt16      aDebugFlags ) ENTRY_ATTR;


/* Entry point for disconnecting the client engine at the end */
 ENGINE_ENTRY TSyError SYSYNC_EXTERNAL(DisconnectEngine)
                                          ( UI_Call_In  aCI         ) ENTRY_ATTR;

/* Entry point for disconnecting the client engine at the end */
 ENGINE_ENTRY TSyError SYSYNC_EXTERNAL_SRV(DisconnectEngine)
                                          ( UI_Call_In  aCI         ) ENTRY_ATTR;



#ifdef __cplusplus
  } // namespace
#endif

#endif
/* eof */
