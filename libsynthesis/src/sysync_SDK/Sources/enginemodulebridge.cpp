/*
 *  File:    enginemodulebridge.cpp
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  Synthesis SyncML client test connector
 *
 *  Copyright (c) 2007-2011 by Synthesis AG + plan44.ch
 *
 *
 */


#include "enginemodulebridge.h"  // include the interface file and utilities
#include "SDK_util.h"
#include "UI_util.h"


namespace sysync {


// local name
#define MyName "enginemodulebridge"



// --------------------------------------------------------------
TEngineModuleBridge::TEngineModuleBridge() {
  fDLL= NULL;
  fIsServer= false;
} // constructor


TEngineModuleBridge::~TEngineModuleBridge() {
  Term();
} // destructor



// --------------------------------------------------------------
TSyError TEngineModuleBridge::Init()
{
  TSyError err= UI_Connect( fCI, fDLL, fIsServer, fEngineName.c_str(), fPrgVersion, fDebugFlags );
  return   err;
} // Init


TSyError TEngineModuleBridge::Term()
{
  TSyError   err= LOCERR_OK;
  if (fCI) { err= UI_Disconnect( fCI, fDLL, fIsServer ); fCI= NULL; }
  return     err;
} // Term


// --------------------------------------------------------------
TSyError TEngineModuleBridge::SetStringMode( uInt16 aCharSet,
                                             uInt16 aLineEndMode, bool aBigEndian )
{
  SetStringMode_Func  p= fCI->ui.SetStringMode;  if (!p) return LOCERR_NOTIMP;
  return              p( fCI, aCharSet, aLineEndMode, aBigEndian );
} // SetStringMode


TSyError TEngineModuleBridge::InitEngineXML( cAppCharP aConfigXML )
{
  InitEngineXML_Func  p= fCI->ui.InitEngineXML;  if (!p) return LOCERR_NOTIMP;
  return              p( fCI, aConfigXML );
} // InitEngineXML


TSyError TEngineModuleBridge::InitEngineFile( cAppCharP aConfigFilePath )
{
  InitEngineFile_Func p= fCI->ui.InitEngineFile; if (!p) return LOCERR_NOTIMP;
  return              p( fCI, aConfigFilePath );
} // InitEngineFile


TSyError TEngineModuleBridge::InitEngineCB( TXMLConfigReadFunc aReaderFunc, void* aContext )
{
  InitEngineCB_Func   p= fCI->ui.InitEngineCB;   if (!p) return LOCERR_NOTIMP;
  return              p( fCI, aReaderFunc, aContext );
} // InitEngineCB



// --------------------------------------------------------------
TSyError TEngineModuleBridge::OpenSession( SessionH &aSessionH, uInt32 aSelector,
                                           cAppCharP aSessionName )
{
  OpenSession_Func       p= fCI->ui.OpenSession;       if (!p) return LOCERR_NOTIMP;
  return                 p( fCI, &aSessionH, aSelector, aSessionName );
} // OpenSession


TSyError TEngineModuleBridge::OpenSessionKey( SessionH aSessionH,
                                              KeyH &aKeyH, uInt16 aMode )
{
  OpenSessionKey_Func    p= fCI->ui.OpenSessionKey;    if (!p) return LOCERR_NOTIMP;
  return                 p( fCI, aSessionH, &aKeyH, aMode );
} // OpenSessionKey


TSyError TEngineModuleBridge::SessionStep( SessionH aSessionH, uInt16 &aStepCmd,
                                           TEngineProgressInfo *aInfoP )
{
  SessionStep_Func       p= fCI->ui.SessionStep;       if (!p) return LOCERR_NOTIMP;
  return                 p( fCI, aSessionH, &aStepCmd, aInfoP );
} // SessionStep


TSyError TEngineModuleBridge::GetSyncMLBuffer( SessionH aSessionH,  bool  aForSend,
                                               appPointer &aBuffer, memSize &aBufSize )
{
  GetSyncMLBuffer_Func   p= fCI->ui.GetSyncMLBuffer;   if (!p) return LOCERR_NOTIMP;
  return                 p( fCI, aSessionH, aForSend, &aBuffer, &aBufSize );
} // GetSyncMLBuffer


TSyError TEngineModuleBridge::RetSyncMLBuffer( SessionH aSessionH, bool aForSend,
                                               memSize aRetSize )
{
  RetSyncMLBuffer_Func   p= fCI->ui.RetSyncMLBuffer;   if (!p) return LOCERR_NOTIMP;
  return                 p( fCI, aSessionH, aForSend, aRetSize );
} // RetSyncMLBuffer


TSyError TEngineModuleBridge::ReadSyncMLBuffer( SessionH aSessionH,
                                                appPointer aBuffer, memSize  aBufSize,
                                                                    memSize &aValSize )
{
  ReadSyncMLBuffer_Func  p= fCI->ui.ReadSyncMLBuffer;  if (!p) return LOCERR_NOTIMP;
  return                 p( fCI, aSessionH, aBuffer, aBufSize, &aValSize );
} // ReadSyncMLBuffer


TSyError TEngineModuleBridge::WriteSyncMLBuffer( SessionH aSessionH,
                                                 appPointer aBuffer, memSize aValSize )
{
  WriteSyncMLBuffer_Func p= fCI->ui.WriteSyncMLBuffer; if (!p) return LOCERR_NOTIMP;
  return                 p( fCI, aSessionH, aBuffer, aValSize );
} // WriteSyncMLBuffer


TSyError TEngineModuleBridge::CloseSession( SessionH aSessionH )
{
  CloseSession_Func      p= fCI->ui.CloseSession;      if (!p) return LOCERR_NOTIMP;
  return                 p( fCI, aSessionH );
} // CloseSession



// --------------------------------------------------------------
TSyError TEngineModuleBridge::OpenKeyByPath( KeyH &aKeyH,
                                             KeyH aParentKeyH, cAppCharP aPath,
                                             uInt16 aMode )
{
  OpenKeyByPath_Func p= fCI->ui.OpenKeyByPath; if (!p) return LOCERR_NOTIMP;
  return             p( fCI, &aKeyH, aParentKeyH, aPath, aMode );
} // OpenKeyByPath


TSyError TEngineModuleBridge::OpenSubkey( KeyH &aKeyH,
                                          KeyH aParentKeyH, sInt32 aID,
                                          uInt16 aMode )
{
  OpenSubkey_Func    p= fCI->ui.OpenSubkey;    if (!p) return LOCERR_NOTIMP;
  return             p( fCI, &aKeyH, aParentKeyH, aID, aMode );
} // OpenSubKey


TSyError TEngineModuleBridge::DeleteSubkey( KeyH aParentKeyH, sInt32 aID )
{
  DeleteSubkey_Func  p= fCI->ui.DeleteSubkey;  if (!p) return LOCERR_NOTIMP;
  return             p( fCI, aParentKeyH, aID );
} // DeleteSubkey


TSyError TEngineModuleBridge::GetKeyID( KeyH aKeyH, sInt32 &aID )
{
  GetKeyID_Func      p= fCI->ui.GetKeyID;      if (!p) return LOCERR_NOTIMP;
  return             p( fCI, aKeyH, &aID );
} // GetKeyID


TSyError TEngineModuleBridge::SetTextMode( KeyH aKeyH, uInt16 aCharSet, uInt16 aLineEndMode,
                                           bool aBigEndian )
{
  SetTextMode_Func   p= fCI->ui.SetTextMode;   if (!p) return LOCERR_NOTIMP;
  return             p( fCI, aKeyH, aCharSet, aLineEndMode, aBigEndian );
} // SetTextMode


TSyError TEngineModuleBridge::SetTimeMode( KeyH aKeyH, uInt16 aTimeMode )
{
  SetTimeMode_Func   p= fCI->ui.SetTimeMode;   if (!p) return LOCERR_NOTIMP;
  return             p( fCI, aKeyH, aTimeMode );
} // SetTimeMode


TSyError TEngineModuleBridge::CloseKey( KeyH aKeyH )
{
  CloseKey_Func      p= fCI->ui.CloseKey;      if (!p) return LOCERR_NOTIMP;
  return             p( fCI, aKeyH );
} // CloseKey



// --------------------------------------------------------------
TSyError TEngineModuleBridge::GetValue( KeyH aKeyH, cAppCharP aValName,  uInt16  aValType,
                                  appPointer aBuffer, memSize aBufSize, memSize &aValSize )
{
  GetValue_Func     p= fCI->ui.GetValue;     if (!p) return LOCERR_NOTIMP;
  return            p( fCI, aKeyH, aValName, aValType, aBuffer,aBufSize,&aValSize );
} // GetValue


TSyError TEngineModuleBridge::GetValueByID( KeyH aKeyH,    sInt32 aID,       sInt32  arrIndex,
                                                                             uInt16  aValType,
                                      appPointer aBuffer, memSize aBufSize, memSize &aValSize )
{
  GetValueByID_Func p= fCI->ui.GetValueByID; if (!p) return LOCERR_NOTIMP;
  return            p( fCI, aKeyH, aID, arrIndex, aValType, aBuffer,aBufSize,&aValSize );
} // GetValueByID


sInt32   TEngineModuleBridge::GetValueID( KeyH aKeyH, cAppCharP aName )
{
  GetValueID_Func   p= fCI->ui.GetValueID;   if (!p) return 0;
  return            p( fCI, aKeyH, aName );
} // GetValueID


TSyError TEngineModuleBridge::SetValue( KeyH aKeyH, cAppCharP aValName, uInt16 aValType,
                                 cAppPointer aBuffer, memSize aValSize )
{
  SetValue_Func     p= fCI->ui.SetValue;     if (!p) return LOCERR_NOTIMP;
  return            p( fCI, aKeyH, aValName, aValType, aBuffer,aValSize );
} // SetValue


TSyError TEngineModuleBridge::SetValueByID( KeyH aKeyH,    sInt32 aID, sInt32 arrIndex,
                                                           uInt16 aValType,
                                     cAppPointer aBuffer, memSize aValSize )
{
  SetValueByID_Func p= fCI->ui.SetValueByID; if (!p) return LOCERR_NOTIMP;
  return            p( fCI, aKeyH, aID, arrIndex, aValType, aBuffer,aValSize );
} // SetValueByID



// ---- tunnel functions -----------------------------------------------------------------
TSyError TEngineModuleBridge::StartDataRead( SessionH aContext, cAppCharP   lastToken,
                                                                cAppCharP resumeToken )
{
  SDR_Func     p= fCI->dt.StartDataRead; if (!p) return LOCERR_NOTIMP;
  return       p( (CContext)aContext, lastToken, resumeToken );
} // StartDataRead


TSyError TEngineModuleBridge::ReadNextItem( SessionH aContext, ItemID  aID, appCharP *aItemData,
                                                               sInt32 *aStatus, bool  aFirst )
{
  RdNItemSFunc p= fCI->dt.ReadNextItem; if (!p) return LOCERR_NOTIMP;
  return       p( (CContext)aContext, aID, aItemData, aStatus, aFirst );
} // ReadNextItem


TSyError TEngineModuleBridge::ReadItem( SessionH aContext, cItemID aID, appCharP *aItemData )
{
  Rd_ItemSFunc p= fCI->dt.ReadItem; if (!p) return LOCERR_NOTIMP;
  return       p( (CContext)aContext, aID, aItemData );
} // ReadItem


TSyError TEngineModuleBridge::EndDataRead( SessionH aContext )
{
  EDR_Func     p= fCI->dt.EndDataRead; if (!p) return LOCERR_NOTIMP;
  return       p( (CContext)aContext );
} // EndDataRead


TSyError TEngineModuleBridge::StartDataWrite( SessionH aContext )
{
  SDW_Func     p= fCI->dt.StartDataWrite; if (!p) return LOCERR_NOTIMP;
  return       p( (CContext)aContext );
} // StartDataWrite


TSyError TEngineModuleBridge::InsertItem( SessionH aContext, cAppCharP aItemData, ItemID aID )
{
  InsItemSFunc p= fCI->dt.InsertItem; if (!p) return LOCERR_NOTIMP;
  return       p( (CContext)aContext, aItemData, aID );
} // InsertItem


TSyError TEngineModuleBridge::UpdateItem( SessionH aContext, cAppCharP aItemData, cItemID   aID,
                                                                                   ItemID updID )
{
  UpdItemSFunc p= fCI->dt.UpdateItem; if (!p) return LOCERR_NOTIMP;
  return       p( (CContext)aContext, aItemData, aID, updID  );
} // UpdateItem


TSyError TEngineModuleBridge::MoveItem( SessionH aContext, cItemID aID, cAppCharP newParID )
{
  MovItem_Func p= fCI->dt.MoveItem; if (!p) return LOCERR_NOTIMP;
  return       p( (CContext)aContext, aID, newParID );
} // MoveItem


TSyError TEngineModuleBridge::DeleteItem( SessionH aContext, cItemID aID )
{
  DelItem_Func p= fCI->dt.DeleteItem; if (!p) return LOCERR_NOTIMP;
  return       p( (CContext)aContext, aID );
} // DeleteItem


TSyError TEngineModuleBridge::EndDataWrite( SessionH aContext, bool success, appCharP *newToken )
{
  EDW_Func     p= fCI->dt.EndDataWrite; if (!p) return LOCERR_NOTIMP;
  return       p( (CContext)aContext, success, newToken );
} // EndDataWrite


void     TEngineModuleBridge::DisposeObj( SessionH aContext, void* memory )
{
  DisposeProc  p= fCI->dt.DisposeObj; if (!p) return;
               p( (CContext)aContext, memory );
} // DisposeObj


// --- asKey ---
TSyError TEngineModuleBridge::ReadNextItemAsKey( SessionH aContext, ItemID  aID,     KeyH aItemKey,
                                                                    sInt32 *aStatus, bool aFirst )
{
  RdNItemKFunc p= fCI->dt.ReadNextItemAsKey; if (!p) return LOCERR_NOTIMP;
  return       p( (CContext)aContext, aID, aItemKey, aStatus, aFirst );
} // ReadNextItemAsKey


TSyError TEngineModuleBridge::ReadItemAsKey( SessionH aContext, cItemID aID, KeyH aItemKey )
{
  Rd_ItemKFunc p= fCI->dt.ReadItemAsKey; if (!p) return LOCERR_NOTIMP;
  return       p( (CContext)aContext, aID, aItemKey );
} // ReadItemAsKey


TSyError TEngineModuleBridge::InsertItemAsKey( SessionH aContext, KeyH aItemKey, ItemID aID )
{
  InsItemKFunc p= fCI->dt.InsertItemAsKey; if (!p) return LOCERR_NOTIMP;
  return       p( (CContext)aContext, aItemKey, aID );
} // InsertItemAsKey


TSyError TEngineModuleBridge::UpdateItemAsKey( SessionH aContext, KeyH aItemKey, cItemID   aID,
                                                                                  ItemID updID )
{
  UpdItemKFunc p= fCI->dt.UpdateItemAsKey; if (!p) return LOCERR_NOTIMP;
  return       p( (CContext)aContext, aItemKey, aID, updID  );
} // UpdateItemAsKey





} // namespace sysync
/* eof */
