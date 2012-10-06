/*
 *  File:    sync_dbapiconnect.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *  C/C++ Programming interface between
 *        the Synthesis SyncML engine
 *        and the database layer
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 *
 * This module contains the method table and connection
 * routines, which are used e.g. for the JNI plugin.
 *
 */

#ifndef SYNC_DBAPICONNECT_H
#define SYNC_DBAPICONNECT_H

#include "sync_include.h"
#include <string>

#ifdef __cplusplus
  namespace sysync {
#endif


/* -- configuration for the DB_Api class ------------------------------------------- */
/* ---- MODULE ---------------- */
typedef struct {                            /* start */
  void* Module_CreateContext;
  void* Module_Version;
  void* Module_Capabilities;
} Start_Methods;


typedef struct {                    /* plugin params */
  void* Module_PluginParams;
} Param_Methods;


/* ---- SESSION --------------- */
typedef struct {
  void* Session_PasswordMode;        /* session auth */
  void* Session_Login;
  void* Session_Logout;
} SE_Auth__Methods;


typedef struct {
  void* Session_CheckDevice;         /* device admin */
  void* Session_GetNonce;
  void* Session_SaveNonce;
  void* Session_SaveDeviceInfo;
} DV_Admin_Methods;


typedef struct {
  void* Session_GetDBTime;                 /* dbtime */
} DV_Time__Methods;


typedef struct {
  void* Session_AdaptItem;              /* adaptitem */
} SE_Adapt_Methods;


typedef struct {         /* --- session handling --- */
  void* Session_CreateContext;               /* open */

  SE_Adapt_Methods seAdapt;             /* adaptitem */
  SE_Auth__Methods seAuth;                   /* auth */
  DV_Admin_Methods dvAdmin;                 /* admin */
  DV_Time__Methods dvTime;                 /* dbtime */

  void* Session_DisposeObj;               /* general */
  void* Session_ThreadMayChangeNow;
  void* Session_DispItems;

  void* Session_DeleteContext;              /* close */
} SE_Methods;


/* ---- DATASTORE ------------- */
typedef struct {
  void* ContextSupport;                   /* general */
  void*  FilterSupport;
  void* ThreadMayChangeNow;
  void* WriteLogData;
  void* DispItems;
} DS_General_Methods;


typedef struct {
  void* LoadAdminData;        /* datastore admin str */
  void* SaveAdminData;
} DS_Adm_Str_Methods;


typedef struct {
  void* LoadAdminDataAsKey;   /* datastore admin key */
  void* SaveAdminDataAsKey;
} DS_Adm_Key_Methods;


typedef struct {
  void* ReadNextMapItem;      /* datastore admin map */
  void* InsertMapItem;
  void* UpdateMapItem;
  void* DeleteMapItem;
} DS_Adm_Map_Methods;


typedef struct {
  DS_Adm_Str_Methods str;         /* datastore admin */
  DS_Adm_Key_Methods key;
  DS_Adm_Map_Methods map;
} DS_Adm_Methods;


typedef struct {                /* string read/write */
  void* ReadNextItem;
  void* ReadItem;
  void* InsertItem;
  void* UpdateItem;
} DS_Str_Methods;


typedef struct {                   /* key read/write */
  void* ReadNextItemAsKey;
  void* ReadItemAsKey;
  void* InsertItemAsKey;
  void* UpdateItemAsKey;
} DS_Key_Methods;


typedef struct {                      /* independent */
  void* FinalizeLocalID;
  void* MoveItem;
  void* DeleteItem;
  void* DeleteSyncSet;
} DS_Ind_Methods;


typedef struct {
  void* StartDataRead;                       /* read */
  void* EndDataRead;

  void* StartDataWrite;                     /* write */
  void* EndDataWrite;

  DS_Str_Methods str;
  DS_Key_Methods key;
  DS_Ind_Methods ind;
} DS_Data_Methods;


typedef struct {                            /* BLOBs */
  void* ReadBlob;
  void* WriteBlob;
  void* DeleteBlob;
} DS_Blob_Methods;


typedef struct {                        /* adaptitem */
  void* AdaptItem;
} DS_Adapt_Methods;


typedef struct {       /* --- datastore handling --- */
  void* CreateContext;                       /* open */

  DS_General_Methods   dsg;               /* general */
  DS_Adm_Methods       dsAdm;          /* data admin */
  DS_Data_Methods      dsData;         /* data rd/wr */
  DS_Blob_Methods      dsBlob;              /* BLOBs */
  DS_Adapt_Methods     dsAdapt;         /* adaptitem */

  void* DisposeObj;
  void* DeleteContext;                      /* close */
} DS_Methods;



typedef struct {      /* --- ui context handling --- */
  void* UI_CreateContext;                   /*  open */
  void* UI_RunContext;                      /*   run */
  void* UI_DeleteContext;                   /* close */
} UI_Methods;



/* ======= configuration for the DB_Api class ====== */
typedef struct {
  /* -- module ------------------------------------- */
  Start_Methods start;                    /*   start */
  Param_Methods param;                    /*  params */

  void* Module_DisposeObj;                /* general */
  void* Module_DeleteContext;

  /* -- session ------------------------------------ */
  SE_Methods se;

  /* -- datastore ---------------------------------- */
  DS_Methods ds;

  /* -- ui context --------------------------------- */
  UI_Methods ui;
} API_Methods;



// the Java interface classes
#define c_VAR_short       "VAR_short"
#define c_VAR_int         "VAR_int"
#define c_VAR_long        "VAR_long"
#define c_VAR_bool        "VAR_boolean"
#define c_VAR_String      "VAR_String"
#define c_VAR_byteArray   "VAR_byteArray"

#define c_JNI_ItemID      "ItemID"
#define c_JNI_MapID        "MapID"
#define c_JNI_TPI         "TEngineProgressInfo"
#define c_JNI_DB_Callback "DB_Callback"
#define c_JNI_JCallback     "JCallback"
#define c_JNI_JCallback64   "JCallback64"


#ifdef __cplusplus
  // Support functions for creating Java signatures
  string LCP ( string jP, string className ); //  "L<jP>/<className>;
  string JCS ( void );                        //  "Ljava/lang/String;"
  string SgnS( string s= "" );                //  "(<s>)S"
  string SgnI( string s= "" );                //  "(<s>)I"
  string SgnV( string s= "" );                //  "(<s>)V"

  /*! Assign the DB_API interface functions and JNI signatures */
  /*  (Call ConnectFunctions) */
  TSyError DBApi_DLLAssign( appPointer aMod, appPointer aField, memSize aFieldSize,
                                string aKey,       bool a64bit,  string jP= "" );
#endif


#if defined __cplusplus
  } // namespace */
#endif

#endif /* SYNC_DBAPICONNECT_H */
/* eof */
