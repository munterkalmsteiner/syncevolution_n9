/*
 *  File:    sync_dbapiconnect.cpp
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
 *  This module contains the method table and connection
 *  routines, which are used e.g. for the JNI plugin.
 *
 */


#include "sync_include.h"
#include "sync_dbapidef.h"
#include "sync_dbapiconnect.h"
#include "DLL_interface.h"

#ifdef __cplusplus
  namespace sysync {
#endif


// Support functions for creating Java signatures
string LCP( string jP, string className )
{
  if (jP.empty()) jP =       className;
  else            jP+= '/' + className;
  return    "L" + jP + ";";
} // LCP


       string JCS   ( void )       { return LCP( "java/lang", "String" ); }

static string Sgn   ( string s,
                      string ret  ){ return "(" + s + ")" + ret; }

       string SgnS  ( string s )   { return  Sgn( s, "S" ); }
       string SgnI  ( string s )   { return  Sgn( s, "I" ); }
       string SgnV  ( string s )   { return  Sgn( s, "V" ); }


class JSgn {
  public:
    JSgn() { f64bit= false; }
    bool     f64bit;
    string   fvr;
    string   fr;

    string SgnS_V( string s= "" ) { return Sgn( fvr + s,"S" ); }
    string SgnS_X( string s= "" ) { return Sgn(  fr + s,"S" ); }
    string SgnI_X( string s= "" ) { return Sgn(  fr + s,"I" ); }
    string SgnZ_X( string s= "" ) { return Sgn(  fr + s,"Z" ); }
    string SgnV_X( string s= "" ) { return Sgn(  fr + s,"V" ); }
}; // JSgn



/*! connect the list of these functions, including JNI signatures, if required */
TSyError DBApi_DLLAssign( appPointer aMod, appPointer aField, memSize aFieldSize,
                              string aKey,       bool a64bit,  string jP )
{
  bool keyCur, keyOld, keyOld2;
  string js1, js2, js3, js4, js5;

  string jt = JCS();
  string jvt= LCP( jP, c_VAR_String      );

  string jvl= LCP( jP, c_VAR_long        );
  string jvi= LCP( jP, c_VAR_int         );
  string jdc= LCP( jP, c_JNI_DB_Callback );
  string jmi= LCP( jP, c_JNI_MapID       );
  string jii= LCP( jP, c_JNI_ItemID      );

  JSgn j;
       j.f64bit= a64bit;
  if  (j.f64bit) { j.fvr= jvl; j.fr= "J"; }
  else           { j.fvr= jvi; j.fr= "I"; }

  string js_ = j.SgnS_X();                        // "(I)S"
  string jsT = j.SgnS_X( jt  );                   // "(ILjava/lang/String;)S"
  string jsvT= j.SgnS_X( jvt );                   // "(ILVAR_String;)S"
  string jsA = j.SgnS_X( jvt + jvt + jvt + "I" ); // "(ILVAR_String;LVAR_String;LVAR_String;I)S"


  // -----------------------------------------------------------------------------------------
  if (strcmp( aKey.c_str(),Plugin_Start )==0) {
                          //   "(LVAR_xxx;          ... (VAR_xxx = VAR_int/VAR_long)
    js1= j.SgnS_V( jt     // ... Ljava/lang/String; ...
                 + jt     // ... Ljava/lang/String; ...
                 + jt     // ... Ljava/lang/String; ...
                 + jdc ); // ... LDB_Callback;)S"
    js2= j.SgnI_X();

    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- start of plugin connection
             Mo_CC, js1.c_str(),
             Mo_Ve, js2.c_str(),
             Mo_Ca,jsvT.c_str(),
             NULL );
  } // if

  /*
  // compatibility to older version ( w/o <engineVersion> )
  keyOld= strcmp( aKey.c_str(),Plugin_Param_OLD )==0;
  keyCur= strcmp( aKey.c_str(),Plugin_Param     )==0;

  if   (keyCur || keyOld) {
    // additional param for newer version
    if (keyCur) js1= j.SgnS_X( jt + "I" ); // "(ILjava/lang/String;I)S"
    else        js1=           jsT;        // "(ILjava/lang/String;)S"
  */

  if (strcmp( aKey.c_str(),Plugin_Param )==0) {
    // additional param for newer version
    js1= j.SgnS_X( jt + "I" ); // "(ILjava/lang/String;I)S"

    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- start of plugin connection
             Mo_PP, js1.c_str(),
             NULL );
  } // if


  // ---- session ---------------------------------------------------
  if (strcmp( aKey.c_str(),Plugin_Session )==0) {
                          //   "(LVAR_xxx;          ... (VAR_xxx = VAR_int/VAR_long)
    js1= j.SgnS_V( jt     // ... Ljava/lang/String; ...
                 + jdc ); // ... LDB_Callback;)S"

    js2= j.SgnV_X();
    js3= j.SgnV_X( "Z" + jt );

    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- session ----
             Se_CC, js1.c_str(),
             "",    "", /*-----* adaptitem */

             "",    "", /*-----* session auth */
             "",    "", /*     */
             "",    "", /*-----*/

             "",    "", /*-----* device admin */
             "",    "", /*     */
             "",    "", /*     */
             "",    "", /*-----*/
             "",    "", /*-----* dbtime */

             Se_DO, "", /*~~~~~*/
             Se_TC, js2.c_str(),
             Se_DI, js3.c_str(),
             Se_DC, js_.c_str(),
             NULL );
  } // if

  if (strcmp( aKey.c_str(),Plugin_SE_Adapt )==0) {
    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- device admin (GetDBTime) ----
            "Session_AdaptItem", jsA.c_str(),
             NULL );
  } // if


  // The correct signature with VAR_String can be used now
  keyOld= strcmp( aKey.c_str(),Plugin_SE_Auth_OLD )==0;
  keyCur= strcmp( aKey.c_str(),Plugin_SE_Auth     )==0;

  if   (keyCur || keyOld) {
                js1= j.SgnI_X();
    if (keyCur) js2= j.SgnS_X( jt     //    "(ILjava/lang/String; ...
                             + jvt    // ...   LVAR_String;       ...
                             + jvt ); // ...   LVAR_String;)S"

    if (keyOld) js2= j.SgnS_X( jt     //    "(ILjava/lang/String; ...
                             + jt     // ...   Ljava/lang/String; ...
                             + jvt ); // ...   LVAR_String;)S"

    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- session auth ----
             Se_PM, js1.c_str(),
             Se_LI, js2.c_str(),
             Se_LO, js_.c_str(),
             NULL );
  } // if

  if (strcmp( aKey.c_str(),Plugin_DV_Admin )==0) {
    js1= j.SgnS_X( jt + jvt + jvt ); // "(ILjava/lang/String;LVAR_String;LVAR_String;)S"

    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- device admin ----
             Se_CD, js1.c_str(),
             Se_GN,jsvT.c_str(),
             Se_SN, jsT.c_str(),
             Se_SD, jsT.c_str(), // the same
             NULL );
  } // if

  if (strcmp( aKey.c_str(),Plugin_DV_DBTime )==0) {
    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- device admin (GetDBTime) ----
             Se_GT, jsvT.c_str(),
             NULL );
  } // if


  // ---- datastore -------------------------------------------------
  if (strcmp( aKey.c_str(),Plugin_DS_General )==0) {
    js1= j.SgnI_X(       jt );
    js2= j.SgnV_X(          );
    js3= j.SgnV_X(       jt );
    js4= j.SgnV_X( "Z" + jt );

    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- datastore general ----
             Da_CS, js1.c_str(),
             Da_FS, js1.c_str(),
             Da_TC, js2.c_str(),
             Da_WL, js3.c_str(),
             Da_DI, js4.c_str(),
             NULL );
  } // if


  if (strcmp( aKey.c_str(),Plugin_DS_Admin_Str )==0) {
    js1= j.SgnS_X( jt  + jt + jvt );  // "(ILjava/lang/String;Ljava/lang/String;LVAR_String;)S"

    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- datastore admin asStr ----
             Da_LA,     js1.c_str(),
             Da_SA,     jsT.c_str(),
             NULL );
  } // if

  if (strcmp( aKey.c_str(),Plugin_DS_Admin_Key )==0) {
    js1= j.SgnS_X( jt  + jt + j.fr ); // "(ILjava/lang/String;Ljava/lang/String;I)S"
    js2= j.SgnS_X           ( j.fr ); // "(II)S"

    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- datastore admin asKey ----
             Da_LAK,    js1.c_str(),
             Da_SAK,    js2.c_str(),
             NULL );
  } // if

  // "InsertMapItem" can be used now
  keyOld= strcmp( aKey.c_str(),Plugin_DS_Admin_OLD )==0;
  keyCur= strcmp( aKey.c_str(),Plugin_DS_Admin_Map )==0;

  if   (keyCur || keyOld) {
    cAppCharP   proc_insM= "";
    if (keyCur) proc_insM= Da_IM;

    js1= j.SgnZ_X( jmi + "Z" ); // "(ILMapID;Z)Z"
    js2= j.SgnS_X( jmi );       // "(ILMapID;)S"

    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- datastore admin ----
             Da_RM,     js1.c_str(),
             proc_insM, js2.c_str(),
             Da_UM,     js2.c_str(), // 2nd
             Da_DM,     js2.c_str(), // 3rd
             NULL );
  } // if
   

  // "DeleteBlob" can be used now
  keyOld = strcmp( aKey.c_str(),Plugin_DS_Data_OLD1 )==0;
  keyOld2= strcmp( aKey.c_str(),Plugin_DS_Data_OLD2 )==0;
  keyCur = strcmp( aKey.c_str(),Plugin_DS_Data      )==0;

  if   (keyCur || keyOld || keyOld2) {
    if (keyOld) js1=           jsT;       // "(ILjava/lang/String;)S"
    else        js1= j.SgnS_X( jt + jt ); // "(ILjava/lang/String;Ljava/lang/String;)S"

    cAppCharP   proc_fli= "";
    if (keyCur) proc_fli= Da_FLI;
    cAppCharP   proc_dss= "";
    if (keyCur) proc_dss= Da_DSS;

    js2= j.SgnS_X( "Z" + jvt ); // "(IZLVAR_String;)S"
    js3= j.SgnS_X( jii + jii ); // "(ILItemID;LItemID;)S"
    js4= j.SgnS_X( jii + jt  ); // "(ILItemID;Ljava/lang/String;)S"
    js5= j.SgnS_X( jii );       // "(ILItemID;)S"

    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- datastore data ----
             Da_SR,    js1.c_str(),  // read
             Da_ER,    js_.c_str(),
             Da_SW,    js_.c_str(),  // write
             Da_EW,    js2.c_str(),

                           /*-----*/
             "",       "", /*     */
             "",       "", /* str */
             "",       "", /*     */
             "",       "", /*     */
                           /*-----*/
             "",       "", /*     */
             "",       "", /* key */
             "",       "", /*     */
             "",       "", /*     */
                           /*-----*/

             proc_fli, js3.c_str(),  // independent
             Da_MvI,   js4.c_str(),
             Da_DeI,   js5.c_str(),
             proc_dss, js_.c_str(),
             NULL );
  } // if

  if (strcmp( aKey.c_str(),Plugin_DS_Data_Str )==0) {
    js1= j.SgnS_X(      jii + jvt + jvi + "Z" ); // "(ILItemID;LVAR_String;LVAR_int;Z)S"
    js2= j.SgnS_X(      jii + jvt );             // "(ILItemID;LVAR_String;)S"
    js3= j.SgnS_X( jt + jii );                   // "(ILjava/lang/String;LItemID;)S"
    js4= j.SgnS_X( jt + jii + jii );             // "(ILjava/lang/String;LItemID;LItemID;)S"

    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- aItemData routines ----
             Da_RN, js1.c_str(),
             Da_RI, js2.c_str(),
             Da_II, js3.c_str(),
             Da_UI, js4.c_str(),
             NULL );
  } // if

  if (strcmp( aKey.c_str(),Plugin_DS_Data_Key )==0) {
    js1= j.SgnS_X(        jii + j.fr + jvi + "Z" ); // "(ILItemID;ILVAR_int;Z)S"
    js2= j.SgnS_X(        jii + j.fr );             // "(ILItemID;I)S"
    js3= j.SgnS_X( j.fr + jii );                    // "(IILItemID;)S"
    js4= j.SgnS_X( j.fr + jii + jii );              // "(IILItemID;LItemID;)S"

    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- aItemKey  routines ----
             Da_RNK, js1.c_str(),
             Da_RIK, js2.c_str(),
             Da_IIK, js3.c_str(),
             Da_UIK, js4.c_str(),
             NULL );
  } // if

  keyOld = strcmp( aKey.c_str(),Plugin_DS_Blob_OLD1 )==0;
  keyOld2= strcmp( aKey.c_str(),Plugin_DS_Blob_OLD2 )==0;
  keyCur = strcmp( aKey.c_str(),Plugin_DS_Blob      )==0;

  if (keyCur || keyOld || keyOld2) {
    cAppCharP    proc_delB= "";
    if  (keyCur) proc_delB= Da_DB;

            string bvsz= j.fvr; string bsz= j.fr;
    if (!keyCur) { bvsz=   jvi;        bsz=  "I"; }

    js1 = j.SgnS_X( jii                                   // "(ILItemID;Ljava/lang/String;LVAR_byteArray; ...
                  + jt                                    // ... LVAR_int;LVAR_int;ZLVAR_boolean;)S"
                  + LCP( jP, c_VAR_byteArray )
                  + bvsz
                  + bvsz + "Z"
                  + LCP( jP, c_VAR_bool ) );

    js2 = j.SgnS_X( jii + jt + "[B" + bsz + bsz + "ZZ" ); // "(ILItemID;Ljava/lang/String;[BIIZZ)S"
    js3 = j.SgnS_X( jii + jt                           ); // "(ILItemID;Ljava/lang/String;)S"

    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- datastore data ----
             Da_RB,     js1.c_str(),
             Da_WB,     js2.c_str(),
             proc_delB, js3.c_str(),
             NULL );
  } // if

  if (strcmp( aKey.c_str(),Plugin_DS_Adapt )==0) {
    return ConnectFunctions( aMod, aField,aFieldSize, true,
            "AdaptItem", jsA.c_str(),
             NULL );
  } // if

  if (strcmp( aKey.c_str(),Plugin_Datastore )==0) {
                         //   "(LVAR_xxx;          ... (VAR_xxx = VAR_int/VAR_long)
    js1= j.SgnS_V( jt    // ... Ljava/lang/String; ...
                 + jdc   // ... LDB_Callback;      ...
                 + jt    // ... Ljava/lang/String; ...
                 + jt ); // ... Ljava/lang/String;)S"

    return ConnectFunctions( aMod, aField,aFieldSize, true,
             Da_CC, js1.c_str(),

             "",    "", /*-----* general */
             "",    "", /*     */
             "",    "", /*     */
             "",    "", /*     */
             "",    "", /*-----*/

                        /*-----* admin */
             "",    "", /* str */
             "",    "", /*     */
                        /*-----*/
             "",    "", /* key */
             "",    "", /*     */
                        /*-----*/
             "",    "", /*     */
             "",    "", /* map */
             "",    "", /*     */
             "",    "", /*     */
                        /*-----*/

                        /*-----* data read/write */
             "",    "", /* rd  */
             "",    "", /*     */
                        /*-----*/
             "",    "", /* wr  */
             "",    "", /*     */
                        /*-----*/
             "",    "", /*     */
             "",    "", /* str */
             "",    "", /*     */
             "",    "", /*     */
                        /*-----*/
             "",    "", /*     */
             "",    "", /* key */
             "",    "", /*     */
             "",    "", /*     */
                        /*-----*/
             "",    "", /*     */
             "",    "", /* ind */
             "",    "", /*     */
             "",    "", /*     */
                        /*-----*/

             "",    "", /*-----* blobs */
             "",    "", /*     */
             "",    "", /*-----*/

             "",    "", /*-----* adaptitem */

             Da_DO, "", /*~~~~~*/ // general
             Da_DC, js_.c_str(),  // close
             NULL );
  } // if


  // ---- ui context ------------------------------------------------
  if (strcmp( aKey.c_str(),Plugin_UI )==0) {
                          //   "(LVAR_xxx;          ... (VAR_xxx = VAR_int/VAR_long)
    js1= j.SgnS_V( jt     // ... Ljava/lang/String; ...
                 + jdc ); // ... LDB_Callback;)S"

    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- ui context ----
            "UI_CreateContext", js1.c_str(),
            "UI_RunContext",    js_.c_str(),
            "UI_DeleteContext", js_.c_str(),
             NULL );
  } // if


  // default settings
  if (strcmp( aKey.c_str(),"" )==0) {
    return ConnectFunctions( aMod, aField,aFieldSize, true,
          // ---- module --------------------------------------------------
            "",    "", /*-----* start */
            "",    "", /*     */
            "",    "", /*-----*/

            "",    "", /*-----* plugin params */

            Mo_DO, "", /*~~~~~*/
            Mo_DC, js_.c_str(),

          // ---- session -------------------------------------------------
            "",    "", /*+*/
            "",    "", /*-*---* adaptitem */
                                             /* */
            "",    "", /*-*---* session auth */
            "",    "", /*     */
            "",    "", /*-*---*/
                       /* */
            "",    "", /*-*---* device admin */
            "",    "", /*     */
            "",    "", /*     */
            "",    "", /*     */
            "",    "", /*-*---*(dbtime) */
                       /* */
            "",    "", /*+*/
            "",    "", /*+*/
            "",    "", /*+*/
            "",    "", /*+*/

          // ---- datastore -----------------------------------------------
            "",    "", /*-*     open */

            "",    "", /*-----* general */
            "",    "", /*     */
            "",    "", /*     */
            "",    "", /*     */
            "",    "", /*-----*/

                       /*-----* admin */
            "",    "", /* str */
            "",    "", /*     */
                       /*-----*/
            "",    "", /* key */
            "",    "", /*     */
                       /*-----*/
            "",    "", /*     */
            "",    "", /* map */
            "",    "", /*     */
            "",    "", /*     */
                       /*-----*/

                       /*-----* data read/write */
            "",    "", /* rd  */
            "",    "", /*     */
                       /*-----*/
            "",    "", /* wr  */
            "",    "", /*     */
                       /*-----*/
            "",    "", /*     */
            "",    "", /* str */
            "",    "", /*     */
            "",    "", /*     */
                       /*-----*/
            "",    "", /*     */
            "",    "", /* key */
            "",    "", /*     */
            "",    "", /*     */
                       /*-----*/
            "",    "", /*     */
            "",    "", /* ind */
            "",    "", /*     */
            "",    "", /*     */
                       /*-----*/

            "",    "", /*-----* blobs */
            "",    "", /*     */
            "",    "", /*-----*/

            "",    "", /*-----* adaptitem */

            "",    "", /*-*     general */
            "",    "", /*-*     close   */

         // ---- ui context ------------------------------------------------
            "",    "", /*-*     open    */
            "",    "", /*-*     run     */
            "",    "", /*-*     close   */
            NULL );
  } // if

  return DB_NotFound;
} // DBApi_DLLAssign


#if defined __cplusplus
  } // namespace */
#endif

/* eof */
