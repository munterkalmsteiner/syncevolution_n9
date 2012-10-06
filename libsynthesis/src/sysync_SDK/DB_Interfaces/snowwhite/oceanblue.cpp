/*  
 *  File:    oceanblue.cpp
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *  datastore plugin interface and
 *  Base class for datastore plugins in C++
 *
 *  Copyright (c) 2008-2011 by Synthesis AG + plan44.ch
 *
 */

#include "myadapter.h"

namespace oceanblue {
  using sysync::SDK_Interface_Struct;
  using sysync::DB_Callback;
  using sysync::ItemID;
  using sysync::MapID;

  using sysync::LOCERR_OK;
  using sysync::LOCERR_NOTIMP;
  using sysync::LOCERR_TRUNCATED;
  using sysync::DB_Forbidden;
  using sysync::DB_NotFound;
  using sysync::DB_Full;
  using sysync::Password_Mode_Undefined;
  using sysync::ReadNextItem_EOF;
  using sysync::VP_GlobMulti;
  using sysync::VALTYPE_TEXT;
  using sysync::VALTYPE_INT16;
  using sysync::VALTYPE_INT32;

  using sysync::Manufacturer;
  using sysync::Description;
  using sysync::MinVersion;
  using sysync::YesField;
  using sysync::IsAdmin;


#include "sync_dbapi.h"  // include the interface file and utilities
#define MyDB "OceanBlue" // example debug name



/* ----- OceanBlue MODULE --------------------------------------------------------- */
TSyError OceanBlue_Module::CreateContext( cAppCharP subName )
{
  DEBUG_DB( fCB, MyDB,Mo_CC, "'%s' mod='%s' sub='%s'", fContextName.c_str(), fModName.c_str(), subName );
  return LOCERR_OK;
} // CreateContext

CVersion OceanBlue_Module::Version() { return Plugin_Version( 0 ); } 

TSyError OceanBlue_Module::Capabilities( appCharP &capa ) 
{ 
  string          s = MyPlatform();
  s+= '\n';       s+= DLL_Info;
  Manufacturer  ( s, "Synthesis AG"      );
  Description   ( s, "OceanBlue Adapter" );
  MinVersion    ( s,  VP_GlobMulti       ); // at least V1.5.1
  YesField      ( s,  CA_ADMIN_Info      ); // show "ADMIN" info
  capa= StrAlloc( s.c_str() ); 
  return LOCERR_OK;
} // Capabilities

TSyError OceanBlue_Module::PluginParams ( cAppCharP  /* mConfigParams */, // ignore
                                           CVersion  /* engineVersion */ ) { return LOCERR_OK; }
    
void     OceanBlue_Module::DisposeObj( void* memory ) { StrDispose( memory ); }                                

TSyError OceanBlue_Module::DeleteContext() {
  DEBUG_DB( fCB, MyDB,Mo_DC, "'%s'", fContextName.c_str() ); return LOCERR_OK; 
} // DeleteContext



/* ----- OceanBlue SESSION -------------------------------------------------------- */
TSyError OceanBlue_Session::CreateContext() {   
  DEBUG_DB( fCB, MyDB,Se_CC, "session='%s'", fSessionName.c_str() ); return LOCERR_OK;
} // CreateContext

// -- device
TSyError OceanBlue_Session::CheckDevice( cAppCharP /* aDeviceID */, 
                                          appCharP &sDevKey,
                                          appCharP &nonce ) 
{ 
  sDevKey= StrAlloc( "key"   );
  nonce  = StrAlloc( "nonce" ); return LOCERR_OK;
} // CheckDevice
                                             
TSyError OceanBlue_Session::GetNonce      (  appCharP& /* nonce       */ ) { return DB_NotFound; }
TSyError OceanBlue_Session::SaveNonce     ( cAppCharP  /* nonce       */ ) { return LOCERR_OK;   }
TSyError OceanBlue_Session::SaveDeviceInfo( cAppCharP  /* aDeviceInfo */ ) { return LOCERR_OK;   }
TSyError OceanBlue_Session::GetDBTime     (  appCharP& /* currDBTime  */ ) { return DB_NotFound; }

// -- login
sInt32   OceanBlue_Session::PasswordMode() { return Password_Mode_Undefined; }

TSyError OceanBlue_Session::Login         ( cAppCharP  /* sUsername */, 
                                             appCharP& /* sPassword */,
                                             appCharP& /* sUsrKey   */ ) { return DB_Forbidden; }

TSyError OceanBlue_Session::Logout() { return LOCERR_OK; }

// -- general
void     OceanBlue_Session::DisposeObj( void* memory )                { StrDispose( memory ); }
void     OceanBlue_Session::ThreadMayChangeNow()                                            { }
void     OceanBlue_Session::DispItems( bool /* allFields */, cAppCharP /* specificItem */ ) { }

TSyError OceanBlue_Session::Adapt_Item( appCharP& /* sItemData1  */, 
                                        appCharP& /* sItemData2  */,
                                        appCharP& /* sLocalVars  */,
                                          uInt32  /* sIdentifier */ ) { return LOCERR_OK; }

TSyError OceanBlue_Session::DeleteContext() { 
  DEBUG_DB( fCB, MyDB,Se_DC, "session='%s'", fSessionName.c_str() ); return LOCERR_OK; 
} // DeleteContext



/* ----- OceanBlue DATASTORE ------------------------------------------------------ */
TSyError OceanBlue::CreateContext ( cAppCharP sDevKey, cAppCharP sUsrKey )
{   
  DEBUG_DB( fCB, MyDB,Da_CC, "'%s' (%s) dev='%s' usr='%s'", 
                                fContextName.c_str(), fAsAdmin?"admin":"data", sDevKey,sUsrKey );
  
  #ifndef DISABLE_PLUGIN_DATASTOREADMIN
    fAdmin.Init( fCB, MyDB, "", fContextName, sDevKey,sUsrKey );
  #endif
  #ifndef DISABLE_PLUGIN_BLOBS
    fBlob.Init ( fCB, MyDB, "", fContextName, sDevKey,sUsrKey );
  #endif

  fNthItem= 0; // initialize it
  return LOCERR_OK;
} // CreateContext
                                    
                                    
uInt32 OceanBlue::ContextSupport( cAppCharP /* aContextRules */ ) { return 0; }
uInt32 OceanBlue::FilterSupport ( cAppCharP /* aFilterRules  */ ) { return 0; }

// -- admin
#ifndef DISABLE_PLUGIN_DATASTOREADMIN
  TSyError OceanBlue::LoadAdminData     ( cAppCharP    aLocDB, cAppCharP aRemDB, appCharP &adminData ) {
    return     fAdmin.LoadAdminData                  ( aLocDB,           aRemDB,          &adminData );
  }                // LoadAdminData
  
  TSyError OceanBlue::LoadAdminDataAsKey( cAppCharP /* aLocDB */,
                                          cAppCharP /* aRemDB */, KeyH /* adminKey */ ) {
    return LOCERR_NOTIMP;
  }                // LoadAdminDataAsKey

  TSyError OceanBlue::SaveAdminData     ( cAppCharP adminData ) {
    return     fAdmin.SaveAdminData               ( adminData );
  }                // SaveAdminData

  TSyError OceanBlue::SaveAdminDataAsKey( KeyH /* adminKey */ ) {
    return LOCERR_NOTIMP;
  }                // SaveAdminDataAsKey


  bool     OceanBlue::ReadNextMapItem (  MapID mID, bool aFirst ) {
    return     fAdmin.ReadNextMapItem        ( mID,      aFirst );
  }                // ReadNextMapItem

  TSyError OceanBlue::InsertMapItem   ( cMapID mID ) {
    return     fAdmin.InsertMapItem          ( mID );
  }                // InsertMapItem

  TSyError OceanBlue::UpdateMapItem   ( cMapID mID ) {
    return     fAdmin.UpdateMapItem          ( mID );
  }                // UpdateMapItem

  TSyError OceanBlue::DeleteMapItem   ( cMapID mID ) {
    return     fAdmin.DeleteMapItem          ( mID );
  }                // DeleteMapItem
#else
  TSyError OceanBlue::LoadAdminData     ( cAppCharP /* aLocDB */,
                                          cAppCharP /* aRemDB */, appCharP& /* adminData */ ) { return DB_Forbidden;  }
  TSyError OceanBlue::LoadAdminDataAsKey( cAppCharP /* aLocDB */,
                                          cAppCharP /* aRemDB */,     KeyH  /* adminKey  */ ) { return DB_Forbidden;  }

  TSyError OceanBlue::SaveAdminData                            ( cAppCharP  /* adminData */ ) { return DB_Forbidden;  }
  TSyError OceanBlue::SaveAdminDataAsKey                            ( KeyH  /* adminKey  */ ) { return DB_Forbidden;  }

  bool     OceanBlue::ReadNextMapItem (  MapID /* mID */,             bool  /* aFirst    */ ) { return false;         }
  TSyError OceanBlue::InsertMapItem   ( cMapID /* mID */ )                                    { return DB_Forbidden;  }
  TSyError OceanBlue::UpdateMapItem   ( cMapID /* mID */ )                                    { return DB_Forbidden;  }
  TSyError OceanBlue::DeleteMapItem   ( cMapID /* mID */ )                                    { return DB_Forbidden;  }
#endif


// -- read
TSyError OceanBlue::StartDataRead ( cAppCharP /*   lastToken */,
                                    cAppCharP /* resumeToken */ )                        { return LOCERR_OK;     }

TSyError OceanBlue::ReadNextItem     ( ItemID /* aID */, appCharP& /* aItemData */, 
                                       sInt32 &aStatus,      bool  /* aFirst    */ )    
                                             { aStatus= ReadNextItem_EOF;                  return LOCERR_OK;     }
TSyError OceanBlue::ReadNextItemAsKey( ItemID /* aID */,     KeyH  /* aItemKey  */,
                                       sInt32 &aStatus,      bool  /* aFirst    */ )
                                            { aStatus= ReadNextItem_EOF;                   return LOCERR_OK;     }

TSyError OceanBlue::ReadItem        ( cItemID /* aID */, appCharP& /* aItemData */ )     { return LOCERR_NOTIMP; }
TSyError OceanBlue::ReadItemAsKey   ( cItemID /* aID */,     KeyH  /* aItemKey  */ )     { return LOCERR_NOTIMP; }
TSyError OceanBlue::EndDataRead()                                                        { return LOCERR_OK;     }

// -- write
TSyError OceanBlue::StartDataWrite()                                                     { return LOCERR_OK;     }
TSyError OceanBlue::InsertItem    ( cAppCharP  /* aItemData */,    ItemID /* newID */ )  { return LOCERR_NOTIMP; }
TSyError OceanBlue::InsertItemAsKey    ( KeyH  /* aItemKey  */,    ItemID /* newID */ )  { return LOCERR_NOTIMP; }
TSyError OceanBlue::UpdateItem    ( cAppCharP  /* aItemData */,   cItemID /*   aID */, 
                                                                   ItemID /* updID */ )  { return LOCERR_NOTIMP; }
TSyError OceanBlue::UpdateItemAsKey    ( KeyH  /* aItemKey  */,   cItemID /*   aID */, 
                                                                   ItemID /* updID */ )  { return LOCERR_NOTIMP; }
    
TSyError OceanBlue::MoveItem        ( cItemID  /* aID */,    cAppCharP /* newParID */ )  { return LOCERR_NOTIMP; }
TSyError OceanBlue::DeleteItem      ( cItemID  /* aID */ )                               { return LOCERR_NOTIMP; }
TSyError OceanBlue::FinalizeLocalID ( cItemID  /* aID */,          ItemID /* updID */ )  { return LOCERR_NOTIMP; }
TSyError OceanBlue::DeleteSyncSet()                                                      { return LOCERR_NOTIMP; }
TSyError OceanBlue::EndDataWrite       ( bool /* success */, appCharP& /* newToken */ )  { return LOCERR_OK;     }


// -- blobs
TSyError OceanBlue::ReadBlob  ( cItemID  aID,   cAppCharP  aBlobID,
                             appPointer &aBlkPtr, memSize &aBlkSize, memSize &aTotSize,
                                   bool  aFirst,     bool &aLast )
{ 
  #ifndef DISABLE_PLUGIN_BLOBS
    return fBlob.ReadBlob  ( aID,aBlobID, &aBlkPtr,&aBlkSize,&aTotSize, aFirst,&aLast );
  #endif
  return LOCERR_NOTIMP;
} // ReadBlob

TSyError OceanBlue::WriteBlob ( cItemID aID,   cAppCharP aBlobID,
                             appPointer aBlkPtr, memSize aBlkSize, memSize aTotSize,
                                   bool aFirst,     bool aLast )
{ 
  #ifndef DISABLE_PLUGIN_BLOBS
    return fBlob.WriteBlob ( aID,aBlobID, aBlkPtr,aBlkSize,aTotSize, aFirst,aLast );
  #endif
  return LOCERR_NOTIMP;
} // WriteBlob

TSyError OceanBlue::DeleteBlob( cItemID  aID,  cAppCharP  aBlobID )
{ 
  #ifndef DISABLE_PLUGIN_BLOBS
    return fBlob.DeleteBlob( aID,aBlobID );
  #endif
  return LOCERR_NOTIMP;
} // DeleteBlob

    
// -- general
void     OceanBlue::WriteLogData  ( cAppCharP  /* logData */ )      { }
void     OceanBlue::DisposeObj        ( void*  /* memory  */ )      { }
void     OceanBlue::ThreadMayChangeNow()                            { }
void     OceanBlue::DispItems          ( bool  /* allFields */,
                                    cAppCharP  /* specificItem */ ) { }

TSyError OceanBlue::Adapt_Item    (  appCharP& /* aItemData1  */,
                                     appCharP& /* aItemData2  */,
                                     appCharP& /* aLocalVars  */, 
                                       uInt32  /* aIdentifier */ ) { return LOCERR_OK; }
                                       
TSyError OceanBlue::DeleteContext() { 
  DEBUG_DB( fCB, MyDB,Da_DC, "'%s'", fContextName.c_str() );   return LOCERR_OK; 
} // DeleteContext


// ---- call-in functions ----
sInt32   OceanBlue::GetValueID  ( KeyH aItemKey,    string  aValName, string suff )
{
  if      (!fCB->ui.GetValueID)   return -1;
                                                 aValName+= suff;
  return    fCB->ui.GetValueID  ( fCB, aItemKey, aValName.c_str() );
} // GetValueID

TSyError OceanBlue::GetValue    ( KeyH aItemKey,    string  aValName,  uInt16  aValType,
                            appPointer aBuffer,    memSize  aBufSize, memSize &aValSize, string suff )
{
  if      (!fCB->ui.GetValue)     return LOCERR_NOTIMP;
                                                 aValName+= suff;
  return    fCB->ui.GetValue    ( fCB, aItemKey, aValName.c_str(), aValType, aBuffer, aBufSize, &aValSize ); 
} // GetValue

TSyError OceanBlue::GetValueByID( KeyH aItemKey,    sInt32  aID,       sInt32  arrIndex, 
                                                                       uInt16  aValType,
                            appPointer aBuffer,    memSize  aBufSize, memSize &aValSize, string suff )
{
  if      (!fCB->ui.GetValueByID) return LOCERR_NOTIMP;
  AppendSuffixToID                   ( aItemKey, aID, suff.c_str() );
  return    fCB->ui.GetValueByID( fCB, aItemKey, aID, arrIndex,    aValType, aBuffer, aBufSize, &aValSize ); 
} // GetValueByID

TSyError OceanBlue::SetValue    ( KeyH aItemKey,    string  aValName,  uInt16  aValType,
                            appPointer aBuffer,                       memSize  aValSize, string suff )
{   
  if      (!fCB->ui.SetValue)     return LOCERR_NOTIMP;
                                                 aValName+= suff;
  return    fCB->ui.SetValue    ( fCB, aItemKey, aValName.c_str(), aValType, aBuffer, aValSize ); 
} // SetValue

TSyError OceanBlue::SetValueByID( KeyH aItemKey,    sInt32  aID,       sInt32  arrIndex, 
                                                                       uInt16  aValType,
                            appPointer aBuffer,                       memSize  aValSize, string suff )
{   
  if      (!fCB->ui.SetValueByID) return LOCERR_NOTIMP;
  AppendSuffixToID                   ( aItemKey, aID, suff.c_str() );
  return    fCB->ui.SetValueByID( fCB, aItemKey, aID, arrIndex,    aValType, aBuffer, aValSize ); 
} // SetValueByID


// -------------------------------------------------
void OceanBlue::AppendSuffixToID( KeyH aItemKeyH, sInt32 &aID, cAppCharP suff )
{
  string s= suff;
  if    (s=="") return;
                               s= VALNAME_FLAG + s; 
  aID+= GetValueID( aItemKeyH, s.c_str() );
} // AppendSuffixToID



// ---- convenience routines to access Get/SetValue for specific types ----
// string
TSyError OceanBlue::GetStr( KeyH aItemKey, string aValName, string &aText, string suff )
{
  TSyError err;
  const uInt16 ty= VALTYPE_TEXT;
  
  const int      txtFSz= 80; // a readonable string length to catch it at once
  char      txt[ txtFSz ];
  memSize        txtSz;

  // try directly to fit in a short string first
                             appCharP      f=  (char*)&txt;
  err=   GetValue( aItemKey, aValName, ty, f, txtFSz,  txtSz, suff );
  
  bool tooShort= err==LOCERR_TRUNCATED;
  if  (tooShort) {
    err= GetValue( aItemKey, aValName, ty, f, 0,       txtSz, suff ); // get size
                         f= (appCharP)malloc( txtSz+1 );              // plus NUL termination
    err= GetValue( aItemKey, aValName, ty, f, txtSz+1, txtSz, suff ); // no error anymore
  } // if
  
  if (err) aText= "";
  else     aText= f; // assign it                                 
  
  if (tooShort)
    free( f ); // and deallocate again, if used dynamically
 
  return err;
} // GetStr


TSyError OceanBlue::GetStrByID( KeyH aItemKey, sInt32 aID,      string &aText, 
                                               sInt32 arrIndex, string suff )
{
  TSyError err;
  const uInt16 ty= VALTYPE_TEXT;
  
  const int      txtFSz= 80; // a readonable string length to catch it at once
  char      txt[ txtFSz ];
  memSize        txtSz;

  // try directly to fit in a short string first
                                     appCharP      f=  (char*)&txt;
  err=   GetValueByID( aItemKey, aID,arrIndex, ty, f, txtFSz,  txtSz, suff );
  
  bool tooShort= err==LOCERR_TRUNCATED;
  if  (tooShort) {
    err= GetValueByID( aItemKey, aID,arrIndex, ty, f, 0,       txtSz, suff ); // get size
                                 f= (appCharP)malloc( txtSz+1 );              // plus NUL termination
    err= GetValueByID( aItemKey, aID,arrIndex, ty, f, txtSz+1, txtSz, suff ); // no error anymore
  } // if
  
  if (err) aText= "";
  else     aText= f; // assign it                                 
  
  if (tooShort)
    free( f ); // and deallocate again, if used dynamically
 
  return err;
} // GetStrByID


TSyError OceanBlue::SetStr    ( KeyH aItemKey, string aValName, string aText, string suff ) {
  //   -1 automatically calculate length from null-terminated string
  return SetValue    ( aItemKey, aValName,      VALTYPE_TEXT, (appPointer)aText.c_str(), (memSize)-1, suff );
} // SetStr


TSyError OceanBlue::SetStrByID( KeyH aItemKey, sInt32 aID,      string aText,
                                                     sInt32 arrIndex, string suff ) {
  //   -1 automatically calculate length from null-terminated string
  return SetValueByID( aItemKey, aID,arrIndex,  VALTYPE_TEXT, (appPointer)aText.c_str(), (memSize)-1, suff );
} // SetStrByID



/* sInt16 / uInt16 */
TSyError OceanBlue::GetInt16    ( KeyH aItemKey, string  aValName, sInt16 &aValue, 
                                                                   string  suff ) {
  memSize                                                                                vSize;
  return GetValue    ( aItemKey, aValName,      VALTYPE_INT16,&aValue,    sizeof(aValue),vSize, suff );
} // GetInt16
TSyError OceanBlue::GetInt16    ( KeyH aItemKey, string  aValName, uInt16 &aValue,
                                                                   string  suff ) {
  memSize                                                                                vSize;
  return GetValue    ( aItemKey, aValName,      VALTYPE_INT16,&aValue,    sizeof(aValue),vSize, suff );
} // GetInt16
    
TSyError OceanBlue::GetInt16ByID( KeyH aItemKey, sInt32  aID,      sInt16 &aValue,
                                                 sInt32  arrIndex, string  suff ) {                                
  memSize                                                                                vSize;
  return GetValueByID( aItemKey, aID,arrIndex,  VALTYPE_INT16,&aValue,    sizeof(aValue),vSize, suff );
} // GetInt16ByID
TSyError OceanBlue::GetInt16ByID( KeyH aItemKey, sInt32  aID,      uInt16 &aValue,
                                                 sInt32  arrIndex, string  suff ) {                                
  memSize                                                                                vSize;
  return GetValueByID( aItemKey, aID,arrIndex,  VALTYPE_INT16,&aValue,    sizeof(aValue),vSize, suff );
} // GetInt16ByID

    
TSyError OceanBlue::SetInt16    ( KeyH aItemKey, string  aValName, sInt16  aValue,
                                                                   string  suff ) {
  return SetValue    ( aItemKey, aValName,      VALTYPE_INT16,&aValue,    sizeof(aValue),       suff );
} // SetInt16

TSyError OceanBlue::SetInt16ByID( KeyH aItemKey, sInt32  aID,      sInt16  aValue,
                                                 sInt32  arrIndex, string  suff ) {
  return SetValueByID( aItemKey, aID,arrIndex,  VALTYPE_INT16,&aValue,    sizeof(aValue),       suff );
} // SetInt16ByID



/* sInt32 / uInt32 */
TSyError OceanBlue::GetInt32    ( KeyH aItemKey, string  aValName, sInt32 &aValue, 
                                                                   string  suff ) {
  memSize                                                                                vSize;
  return GetValue    ( aItemKey, aValName,      VALTYPE_INT32,&aValue,    sizeof(aValue),vSize, suff );
} // GetInt32
TSyError OceanBlue::GetInt32    ( KeyH aItemKey, string  aValName, uInt32 &aValue,
                                                                   string  suff ) {
  memSize                                                                                vSize;
  return GetValue    ( aItemKey, aValName,      VALTYPE_INT32,&aValue,    sizeof(aValue),vSize, suff );
} // GetInt32
    
TSyError OceanBlue::GetInt32ByID( KeyH aItemKey, sInt32  aID,      sInt32 &aValue,
                                                 sInt32  arrIndex, string  suff ) {                                
  memSize                                                                                vSize;
  return GetValueByID( aItemKey, aID,arrIndex,  VALTYPE_INT32,&aValue,    sizeof(aValue),vSize, suff );
} // GetInt32ByID
TSyError OceanBlue::GetInt32ByID( KeyH aItemKey, sInt32  aID,      uInt32 &aValue,
                                                 sInt32  arrIndex, string  suff ) {                                
  memSize                                                                                vSize;
  return GetValueByID( aItemKey, aID,arrIndex,  VALTYPE_INT32,&aValue,    sizeof(aValue),vSize, suff );
} // GetInt32ByID

    
TSyError OceanBlue::SetInt32    ( KeyH aItemKey, string  aValName, sInt32  aValue,
                                                                   string  suff ) {
  return SetValue    ( aItemKey, aValName,      VALTYPE_INT32,&aValue,    sizeof(aValue),       suff );
} // SetInt32

TSyError OceanBlue::SetInt32ByID( KeyH aItemKey, sInt32  aID,      sInt32  aValue,
                                                 sInt32  arrIndex, string  suff ) {
  return SetValueByID( aItemKey, aID,arrIndex,  VALTYPE_INT32,&aValue,    sizeof(aValue),       suff );
} // SetInt32ByID



/* -------------------------------------------------------------------------------- */
#if defined MACOSX && defined __MWERKS__
#pragma export on
#endif

/* ---- MODULE -------------------------------------------------------------------- */
static MyAdapter_Module* SW_Mo( CContext  mc ) { return (MyAdapter_Module*)mc; }
TSyError  Module_CreateContext( CContext *mc, cAppCharP   moduleName,
                                              cAppCharP      subName,
                                              cAppCharP mContextName, DB_Callback mCB )
{
  MyAdapter_Module*   swm= new MyAdapter_Module; if (swm==NULL) return DB_Full;
                      swm->fCB         = mCB;
                      swm->fModName    =   moduleName;
                      swm->fContextName= mContextName;
  TSyError err=       swm->CreateContext    ( subName );
  if      (err)delete swm;
  else *mc= (CContext)swm;
  return   err;
} // Module_CreateContext

CVersion       Module_Version     ( CContext mc ) {
  if          (!mc )  return Plugin_Version( 0 );
  return SW_Mo( mc )->Version();
} // Module_Version

TSyError       Module_Capabilities( CContext mc,  appCharP *capa ) {
  return SW_Mo( mc )->Capabilities                       ( *capa );
} // Module_Capabilities

TSyError       Module_PluginParams( CContext mc, cAppCharP mConfigParams, CVersion engineVersion ) {
  return SW_Mo( mc )->PluginParams                       ( mConfigParams,          engineVersion );
} // Module_PluginParams

void           Module_DisposeObj  ( CContext mc, void* memory ) {
         SW_Mo( mc )->DisposeObj                     ( memory );
} // Module_DisposeObj

TSyError Module_DeleteContext( CContext mc )
{
  MyAdapter_Module* swm= SW_Mo( mc );
  TSyError     err= swm->DeleteContext();
  delete            swm;
  return       err;
} // Module_DeleteContext




/* ---- SESSION ------------------------------------------------------------------- */
static MyAdapter_Session* SW_Se( CContext  sc ) { return (MyAdapter_Session*)sc; }
TSyError Session_CreateContext ( CContext *sc, cAppCharP sessionName, DB_Callback sCB )
{ 
  MyAdapter_Session*  sws= new MyAdapter_Session; if (sws==NULL) return DB_Full;
                      sws->fCB         = sCB;
                      sws->fSessionName= sessionName;
  TSyError err=       sws->CreateContext();
  if      (err)delete sws;
  else *sc= (CContext)sws;
  return   err;
} // Session_CreateContext


// -- device
TSyError      Session_CheckDevice   ( CContext sc, cAppCharP aDeviceID, appCharP *sDevKey, 
                                                                        appCharP *nonce ) {
  return SW_Se( sc )->CheckDevice                          ( aDeviceID, *sDevKey,*nonce );
} // Session_CheckDevice

TSyError      Session_GetNonce      ( CContext sc, appCharP *nonce ) {
  return SW_Se( sc )->GetNonce                            ( *nonce );
} // Session_GetNonce

TSyError      Session_SaveNonce     ( CContext sc, cAppCharP nonce ) {
  return SW_Se( sc )->SaveNonce                            ( nonce );
} // Session_SaveNonce

TSyError      Session_SaveDeviceInfo( CContext sc, cAppCharP aDeviceInfo ) {
  return SW_Se( sc )->SaveDeviceInfo                       ( aDeviceInfo );
} // Session_SaveDeviceInfo

TSyError      Session_GetDBTime     ( CContext sc, appCharP *currentDBTime ) { 
  return SW_Se( sc )->GetDBTime                           ( *currentDBTime );
} // Session_GetDBTime


// -- login
sInt32 Session_PasswordMode( CContext sc ) {
  return SW_Se( sc )->PasswordMode();
} // Session_PasswordMode

TSyError      Session_Login ( CContext sc, cAppCharP sUsername, appCharP *sPassword, appCharP *sUsrKey ) { 
  return SW_Se( sc )->Login                        ( sUsername,          *sPassword,          *sUsrKey );
} // Session_Login

TSyError Session_Logout( CContext sc ) {
  return SW_Se( sc )->Logout();
} // Session_Logout


// -- general
void          Session_DisposeObj( CContext sc, void* memory ) {
         SW_Se( sc )->DisposeObj                   ( memory );
} // Session_DisposeObj

void          Session_ThreadMayChangeNow( CContext sc ) {
         SW_Se( sc )->ThreadMayChangeNow();
} // Session_ThreadMayChangeNow

void          Session_DispItems ( CContext sc, bool allFields, cAppCharP specificItem ) {
         SW_Se( sc )->DispItems                   ( allFields,           specificItem );
} // Session_DispItems

TSyError      Session_AdaptItem ( CContext sc, appCharP *sItemData1, 
                                               appCharP *sItemData2,
                                               appCharP *sLocalVars, uInt32 sIdentifier ) {
  return SW_Se( sc )->Adapt_Item( *sItemData1, *sItemData2, *sLocalVars,    sIdentifier );
} // Session_AdaptItem

TSyError Session_DeleteContext( CContext sc ) 
{ 
  MyAdapter_Session* sws= SW_Se( sc );
  TSyError      err= sws->DeleteContext();
  delete             sws;
  return        err;
} // Session_DeleteContext




/* ---- DATASTORE ----------------------------------------------------------------- */
static   MyAdapter* SW( CContext  ac ) { return (MyAdapter*)ac; }
TSyError CreateContext( CContext *ac, cAppCharP aContextName, DB_Callback aCB,
                                      cAppCharP sDevKey,
                                      cAppCharP sUsrKey )
{
  MyAdapter*             sw= new MyAdapter; if (sw==NULL) return DB_Full;
                         sw->fCB         =  aCB;
                         sw->fContextName=  aContextName;
  sw->fAsAdmin= IsAdmin( sw->fContextName );
  TSyError err=          sw->CreateContext( sDevKey,sUsrKey );
  if      (err)   delete sw;
  else *ac=    (CContext)sw;
  return   err;
} // CreateContext

uInt32             ContextSupport( CContext ac, cAppCharP aContextRules ) {
  return SW( ac )->ContextSupport                       ( aContextRules );
} // ContextSupport

uInt32             FilterSupport ( CContext ac, cAppCharP aFilterRules ) {
  return SW( ac )->FilterSupport                        ( aFilterRules );
} // FilterSupport



// -- admin
TSyError           LoadAdminData     ( CContext ac, cAppCharP aLocDB, cAppCharP aRemDB, appCharP *adminData ) {
  return SW( ac )->LoadAdminData                            ( aLocDB,           aRemDB,          *adminData );
}               // LoadAdminData

TSyError           LoadAdminDataAsKey( CContext ac, cAppCharP aLocDB, cAppCharP aRemDB,     KeyH  adminKey ) {
  return SW( ac )->LoadAdminDataAsKey                       ( aLocDB,           aRemDB,           adminKey );
}               // LoadAdminDataAsKey

TSyError           SaveAdminData     ( CContext ac, cAppCharP adminData ) {
  return SW( ac )->SaveAdminData                            ( adminData );
}               // SaveAdminData

TSyError           SaveAdminDataAsKey( CContext ac,      KeyH adminKey ) {
  return SW( ac )->SaveAdminDataAsKey                       ( adminKey );
}               // SaveAdminDataAsKey

bool               ReadNextMapItem   ( CContext ac,  MapID mID, bool aFirst ) {
  return SW( ac )->ReadNextMapItem                       ( mID,      aFirst );
}               // ReadNextMapItem

TSyError           InsertMapItem     ( CContext ac, cMapID mID ) {
  return SW( ac )->InsertMapItem                         ( mID );
}               // InsertMapItem

TSyError           UpdateMapItem     ( CContext ac, cMapID mID ) {
  return SW( ac )->UpdateMapItem                         ( mID );
}               // UpdateMapItem

TSyError           DeleteMapItem     ( CContext ac, cMapID mID ) {
  return SW( ac )->DeleteMapItem                         ( mID );
}               // DeleteMapItem



// -- read
TSyError           StartDataRead    ( CContext ac, cAppCharP lastToken, cAppCharP resumeToken ) {
  return SW( ac )->StartDataRead                           ( lastToken,           resumeToken );
} // StartDataRead

TSyError           ReadNextItem     ( CContext ac,  ItemID aID, appCharP *aItemData, sInt32 *aStatus, bool aFirst ) {
  return SW( ac )->ReadNextItem                          ( aID,          *aItemData,        *aStatus,      aFirst );
} // ReadNextItem

TSyError           ReadNextItemAsKey( CContext ac,  ItemID aID,     KeyH  aItemKey,  sInt32 *aStatus, bool aFirst ) {
  return SW( ac )->ReadNextItemAsKey                     ( aID,           aItemKey,         *aStatus,      aFirst );
} // ReadNextItemAsKey

TSyError           ReadItem         ( CContext ac, cItemID aID, appCharP *aItemData ) {
  return SW( ac )->ReadItem                              ( aID,          *aItemData );
} // ReadItem

TSyError           ReadItemAsKey    ( CContext ac, cItemID aID,     KeyH  aItemKey  ) {
  return SW( ac )->ReadItemAsKey                         ( aID,           aItemKey  );
} // ReadItemAsKey

TSyError           EndDataRead      ( CContext ac ) {
  return SW( ac )->EndDataRead();
} // EndDataRead



// -- write
TSyError           StartDataWrite ( CContext ac ) {
  return SW( ac )->StartDataWrite();
} // StartDataWrite

TSyError           InsertItem     ( CContext ac, cAppCharP aItemData,  ItemID newID ) {
  return SW( ac )->InsertItem                            ( aItemData,         newID );
} // InsertItem

TSyError           InsertItemAsKey( CContext ac,      KeyH aItemKey,   ItemID newID ) {
  return SW( ac )->InsertItemAsKey                       ( aItemKey,          newID );
} // InsertItemAsKey

TSyError           UpdateItem     ( CContext ac, cAppCharP aItemData, cItemID aID, ItemID updID ) {
  return SW( ac )->UpdateItem                            ( aItemData,         aID,        updID );
} // UpdateItem

TSyError           UpdateItemAsKey( CContext ac,      KeyH aItemKey,  cItemID aID, ItemID updID ) {
  return SW( ac )->UpdateItemAsKey                       ( aItemKey,          aID,        updID );
} // UpdateItemAsKey

TSyError           MoveItem       ( CContext ac, cItemID aID, cAppCharP newParID ) {
  return SW( ac )->MoveItem                            ( aID,           newParID );
} // MoveItem

TSyError           DeleteItem     ( CContext ac, cItemID aID ) {
  return SW( ac )->DeleteItem                          ( aID );
} // DeleteItem

TSyError           FinalizeLocalID( CContext ac, cItemID aID, ItemID updID ) {
  return SW( ac )->FinalizeLocalID                     ( aID,        updID );
} // FinalizeLocalID

TSyError           DeleteSyncSet  ( CContext ac ) {
  return SW( ac )->DeleteSyncSet();
} // DeleteSyncSet

TSyError           EndDataWrite   ( CContext ac, bool success, appCharP *newToken ) {
  return SW( ac )->EndDataWrite                     ( success,          *newToken );
} // EndDataWrite



// -- blobs
TSyError           ReadBlob  ( CContext ac, cItemID  aID,   cAppCharP  aBlobID,
                                         appPointer *aBlkPtr, memSize *aBlkSize, 
                                                              memSize *aTotSize,
                                               bool  aFirst,     bool *aLast ) {
  return SW( ac )->ReadBlob  ( aID,aBlobID, *aBlkPtr,*aBlkSize,*aTotSize, aFirst,*aLast );
} // ReadBlob

TSyError           WriteBlob ( CContext ac, cItemID  aID,   cAppCharP  aBlobID,
                                         appPointer  aBlkPtr, memSize  aBlkSize, 
                                                              memSize  aTotSize,
                                               bool  aFirst,     bool  aLast ) {
  return SW( ac )->WriteBlob ( aID,aBlobID,  aBlkPtr, aBlkSize, aTotSize, aFirst, aLast );
} // WriteBlob

TSyError           DeleteBlob( CContext ac, cItemID  aID, cAppCharP  aBlobID ) {
  return SW( ac )->DeleteBlob( aID,aBlobID );
} // DeleteBlob



// -- general
void               WriteLogData( CContext ac, cAppCharP logData ) {
         SW( ac )->WriteLogData                       ( logData );
} // WriteLogData

void               DisposeObj  ( CContext ac, void* memory ) {
         SW( ac )->DisposeObj                     ( memory );
} // DisposeObj

void               ThreadMayChangeNow( CContext ac ) {
         SW( ac )->ThreadMayChangeNow();
} // ThreadMayChangeNow

void               DispItems ( CContext ac, bool allFields, cAppCharP specificItem ) {
         SW( ac )->DispItems                   ( allFields,           specificItem );
} // DispItems

TSyError           AdaptItem ( CContext ac, appCharP *aItemData1,
                                            appCharP *aItemData2,
                                            appCharP *aLocalVars, uInt32 aIdentifier ) { 
  return SW( ac )->Adapt_Item( *aItemData1, *aItemData2, *aLocalVars,    aIdentifier );
} // AdaptItem

TSyError DeleteContext( CContext ac )
{
  MyAdapter*    sw= SW( ac );
  TSyError err= sw->DeleteContext();
  delete        sw;
  return   err;
} // DeleteContext


} // namespace
/* eof */
