/*
 *  File:         dbapi_include.h
 *
 *  Author:       Beat Forster
 *
 *  DB_Api class
 *    Bridge to user programmable interface
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 */



/* include definitions for each namespace */
#undef    SYNC_DBAPI_H
#include "sync_dbapi.h"

#undef    SYNC_UIAPI_H
#include "sync_uiapi.h"

#if defined __cplusplus
  using sysync::ConnectFunctions;
  using sysync::LOCERR_OK;
  using sysync::DB_NotFound;
  using sysync::DB_Forbidden;
  using sysync::DB_Error;
  using sysync::DB_Fatal;
  using sysync::VP_BadVersion;
#endif


static TSyError AssignMethods( appPointer aMod, appPointer aField, memSize aFieldSize, cAppCharP aKey= "" )
{
  // ---- module -----------------------------------------------------------------
  if (strcmp( aKey,Plugin_Start )==0) {
    return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- start of plugin connection
                      Module_CreateContext,
                      Module_Version,
                      Module_Capabilities,
                      NULL );
  } // if

  if (strcmp( aKey,Plugin_Param )==0) {
    return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- plugin params
                      Module_PluginParams,
                      NULL );
  } // if


  // ---- session ----------------------------------------------------------------
  #if !defined DISABLE_PLUGIN_SESSIONAUTH || !defined DISABLE_PLUGIN_DEVICEADMIN
    if (strcmp( aKey,Plugin_Session )==0) {
      return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- session ----
                      Session_CreateContext,
                      XX, /*-----* adaptitem */

                      XX, /*-----* session auth */
                      XX, /*     */
                      XX, /*-----*/

                      XX, /*-----* device admin */
                      XX, /*     */
                      XX, /*     */
                      XX, /*     */
                      XX, /*-----*(dbtime) */

                      Session_DisposeObj,
                      Session_ThreadMayChangeNow,
                      Session_DispItems,
                      Session_DeleteContext,
                      NULL );
    } // if

    if (strcmp( aKey,Plugin_SE_Adapt )==0) {
      return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- device admin (GetDBTime) ----
                      Session_AdaptItem,
                      NULL );
    } // if
  #endif

  #ifndef DISABLE_PLUGIN_SESSIONAUTH
    if (strcmp( aKey,Plugin_SE_Auth     )==0 || // new AND old
        strcmp( aKey,Plugin_SE_Auth_OLD )==0) {
      return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- session auth ----
                      Session_PasswordMode,
                      Session_Login,
                      Session_Logout,
                      NULL );
    } // if
  #endif

  #ifndef DISABLE_PLUGIN_DEVICEADMIN
    if (strcmp( aKey,Plugin_DV_Admin )==0) {
      return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- device admin ----
                      Session_CheckDevice,
                      Session_GetNonce,
                      Session_SaveNonce,
                      Session_SaveDeviceInfo,
                      NULL );
    } // if

    if (strcmp( aKey,Plugin_DV_DBTime )==0) {
      return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- device admin (GetDBTime) ----
                      Session_GetDBTime,
                      NULL );
    } // if
  #endif


  // ---- datastore --------------------------------------------------------------
  #if !defined DISABLE_PLUGIN_DATASTOREADMIN || !defined DISABLE_PLUGIN_DATASTOREDATA
    if (strcmp( aKey,Plugin_DS_General )==0) {
      return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- general datastore routines ----
                      ContextSupport,
                      FilterSupport,
                      ThreadMayChangeNow,
                      WriteLogData,
                      DispItems,
                      NULL );
    } // if
  #endif

  /*
  #ifndef DISABLE_PLUGIN_DATASTOREADMIN
    if (strcmp( aKey,Plugin_DS_Admin     )==0 || // new AND old
        strcmp( aKey,Plugin_DS_Admin_OLD )==0) {
      return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- datastore admin ----
                      LoadAdminData,
                      SaveAdminData,
                      ReadNextMapItem,
                      InsertMapItem,
                      UpdateMapItem,
                      DeleteMapItem,
                      NULL );
    } // if
  #endif
  */

  #ifndef   DISABLE_PLUGIN_DATASTOREADMIN
    #ifndef DISABLE_PLUGIN_DATASTOREADMIN_STR
      if (strcmp( aKey,Plugin_DS_Admin_Str )==0) {
        return ConnectFunctions( aMod, aField,aFieldSize, false,
                // ---- aItemData functions ----
                        LoadAdminData,
                        SaveAdminData,
                        NULL );
      } // if
    #endif

    #ifndef DISABLE_PLUGIN_DATASTOREADMIN_KEY
      if (strcmp( aKey,Plugin_DS_Admin_Key )==0) {
        return ConnectFunctions( aMod, aField,aFieldSize, false,
                // ---- aItemKey functions ----
                        LoadAdminDataAsKey,
                        SaveAdminDataAsKey,
                        NULL );
      } // if
    #endif

    if   (strcmp( aKey,Plugin_DS_Admin_Map )==0 || // new AND old
          strcmp( aKey,Plugin_DS_Admin_OLD )==0) {
      return   ConnectFunctions( aMod, aField,aFieldSize, false,
                // ---- datastore admin ----
                        ReadNextMapItem,
                        InsertMapItem,
                        UpdateMapItem,
                        DeleteMapItem,
                        NULL );
      } // if
  #endif

  #ifndef DISABLE_PLUGIN_ADAPTITEM
    if (strcmp( aKey,Plugin_DS_Adapt )==0) {
      return ConnectFunctions( aMod, aField,aFieldSize, false,
                      AdaptItem,
                      NULL );
    } // if
  #endif

  #ifndef DISABLE_PLUGIN_DATASTOREDATA
    if (strcmp( aKey,Plugin_DS_Data      )==0 || // new AND old
        strcmp( aKey,Plugin_DS_Data_OLD1 )==0 ||
        strcmp( aKey,Plugin_DS_Data_OLD2 )==0) {
      return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- datastore data ----
                      StartDataRead,     // read
                      EndDataRead,

                      StartDataWrite,    // write
                      EndDataWrite,

                          /*-----*/
                      XX, /*     */
                      XX, /* str */
                      XX, /*     */
                      XX, /*     */
                          /*-----*/
                      XX, /*     */
                      XX, /* key */
                      XX, /*     */
                      XX, /*     */
                          /*-----*/

                      FinalizeLocalID, // ind
                      MoveItem,
                      DeleteItem,
                      DeleteSyncSet,
                      NULL );
    } // if

    #ifndef DISABLE_PLUGIN_DATASTOREDATA_STR
    if (strcmp( aKey,Plugin_DS_Data_Str )==0) {
      return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- aItemData routines ----
                      ReadNextItem,
                      ReadItem,
                      InsertItem,
                      UpdateItem,
                      NULL );
    } // if
    #endif

    #ifndef DISABLE_PLUGIN_DATASTOREDATA_KEY
    if (strcmp( aKey,Plugin_DS_Data_Key )==0) {
      return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- aItemKey  routines ----
                      ReadNextItemAsKey,
                      ReadItemAsKey,
                      InsertItemAsKey,
                      UpdateItemAsKey,
                      NULL );
    } // if
    #endif
  #endif

  #if !defined DISABLE_PLUGIN_DATASTOREADMIN  || !defined DISABLE_PLUGIN_DATASTOREDATA
    if (strcmp( aKey,Plugin_DS_Blob      )==0 || // new AND old
        strcmp( aKey,Plugin_DS_Blob_OLD1 )==0 ||
        strcmp( aKey,Plugin_DS_Blob_OLD2 )==0) {
      return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- BLOBs ----
                      ReadBlob,
                      WriteBlob,
                      DeleteBlob,
                      NULL );
    } // if
  #endif

  #if !defined DISABLE_PLUGIN_DATASTOREADMIN || !defined DISABLE_PLUGIN_DATASTOREDATA || !defined DISABLE_PLUGIN_ADAPTITEM
    if (strcmp( aKey,Plugin_Datastore )==0) {
      return ConnectFunctions( aMod, aField,aFieldSize, false,
                      CreateContext,  // open

                      XX, /*+*---- general */
                      XX, /*+*/
                      XX, /*+*/
                      XX, /*+*/
                      XX, /*+*/
                          /* */
                          /*-----* admin */
                      XX, /* str */
                      XX, /*     */
                          /*-----*/
                      XX, /* key */
                      XX, /*     */
                          /*-----*/
                      XX, /*     */
                      XX, /* map */
                      XX, /*     */
                      XX, /*     */
                          /*-----*/
                          /* */
                          /*-----* data read/write */
                      XX, /* rd  */
                      XX, /*     */
                          /*-----*/
                      XX, /* wr  */
                      XX, /*     */
                          /*-----*/
                      XX, /*     */
                      XX, /* str */
                      XX, /*     */
                      XX, /*     */
                          /*-----*/
                      XX, /*     */
                      XX, /* key */
                      XX, /*     */
                      XX, /*     */
                          /*-----*/
                      XX, /*     */
                      XX, /* ind */
                      XX, /*     */
                      XX, /*     */
                          /*-----*/
                          /* */
                      XX, /*-----* blobs */
                      XX, /*     */
                      XX, /*-----*/

                      XX, /*-----* adaptitem */

                      DisposeObj,
                      DeleteContext, // close
                      NULL );
    } // if
  #endif


  #ifdef ENABLE_PLUGIN_UI
  // ---- ui context ------------------------------------------------
    if (strcmp( aKey,Plugin_UI )==0) {
      return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- ui ----
                      UI_CreateContext,
                      UI_RunContext,
                      UI_DeleteContext,
                      NULL );
    } // if
  #endif


  // default settings
  if (*aKey=='\0') {
    return ConnectFunctions( aMod, aField,aFieldSize, false,
              // ---- module ----
                      XX, /*-----* start */
                      XX, /*     */
                      XX, /*-----*/

                      XX, /*-----* plugin params */

                      Module_DisposeObj,
                      Module_DeleteContext,

              // ---- session ----
                      XX, /*+*/
                          /* */
                      XX, /*-*---* login */
                      XX, /*     */
                      XX, /*-*---*/
                          /* */
                      XX, /*-*---* dev admin */
                      XX, /*     */
                      XX, /*     */
                      XX, /*     */
                      XX, /*-*---*(db time) */
                          /* */
                      XX, /*-*---* adaptitem */
                          /* */
                      XX, /*+*/
                      XX, /*+*/
                      XX, /*+*/
                      XX, /*+*/

              // ---- datastore ----
                      XX, /*-*     open */

                      XX, /*+*---- general */
                      XX, /*+*/
                      XX, /*+*/
                      XX, /*+*/
                      XX, /*+*/
                          /* */
                          /*-----* admin */
                      XX, /* str */
                      XX, /*     */
                          /*-----*/
                      XX, /* key */
                      XX, /*     */
                          /*-----*/
                      XX, /*     */
                      XX, /* map */
                      XX, /*     */
                      XX, /*     */
                          /*-----*/
                          /* */
                          /*-----* data read/write */
                      XX, /* rd  */
                      XX, /*     */
                          /*-----*/
                      XX, /* wr  */
                      XX, /*     */
                          /*-----*/
                      XX, /*     */
                      XX, /* str */
                      XX, /*     */
                      XX, /*     */
                          /*-----*/
                      XX, /*     */
                      XX, /* key */
                      XX, /*     */
                      XX, /*     */
                          /*-----*/
                      XX, /*     */
                      XX, /* ind */
                      XX, /*     */
                      XX, /*     */
                          /*-----*/
                          /* */
                      XX, /*-----* blobs */
                      XX, /*     */
                      XX, /*-----*/

                      XX, /*-----* "script-like" adapt */

                      XX, /*-*     DisposeObj */
                      XX, /*-*     close */

              // ---- ui context ----
                      XX, /*-*     open  */
                      XX, /*-*     run   */
                      XX, /*-*     close */
                      NULL );
  } // if

  return DB_NotFound;
} // AssignMethods

/* eof */
