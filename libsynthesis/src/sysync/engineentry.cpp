/**
 *  @File     enginementry.cpp
 *
 *  @Author   Beat Forster (bfo@synthesis.ch)
 *
 *  @brief TEngineModuleBase
 *    engine bus bar class
 *
 *    Copyright (c) 2007-2011 by Synthesis AG + plan44.ch
 *
 */

/*
 */


#include "prefix_file.h"
#include "SDK_util.h"
#include "engineentry.h"
#include "enginemodulebase.h"


namespace sysync {


// Get object reference
static DB_Callback        DBC ( void*    aCB      ) { return            (DB_Callback) aCB;             }
static TEngineModuleBase* URef( void*    aCB      ) { return (TEngineModuleBase*)DBC( aCB )->thisBase; }
static TunnelWrapper*     TW  ( CContext aContext ) { return (TunnelWrapper*)aContext;                 }
static TEngineModuleBase* TRef( CContext aContext ) { return       URef( TW( aContext )->tCB );        }
static SessionH           TCon( CContext aContext ) { return             TW( aContext )->tContext;     }


// --- Callback entries --------------------------
void DebugDB             ( void* aCB, cAppCharP aParams ) {
  DBC( aCB )->DB_DebugPuts     ( aCB,           aParams );
} // DebugDB

void DebugExotic         ( void* aCB, cAppCharP aParams ) {
  DBC( aCB )->DB_DebugExotic   ( aCB,           aParams );
} // DebugExotic

void DebugBlock          ( void* aCB, cAppCharP aTag, cAppCharP aDesc, cAppCharP aAttrText ) {
  DBC( aCB )->DB_DebugBlock    ( aCB,           aTag,           aDesc,           aAttrText );
} // DebugBlock

void DebugEndBlock       ( void* aCB, cAppCharP aTag ) {
  DBC( aCB )->DB_DebugEndBlock ( aCB,           aTag );
} // DebugEndBlock

void DebugEndThread      ( void* aCB ) {
  DBC( aCB )->DB_DebugEndThread( aCB );
} // DebugEndThread



// ----------------------------------------------------------------------------------------
TSyError              SetStringMode ( void* aCB, uInt16 aCharSet,
                                                 uInt16 aLineEndMode, bool aBigEndian ) {
  return URef( aCB )->SetStringMode         ( aCharSet, aLineEndMode,      aBigEndian );
} //                  SetStringMode

TSyError              InitEngineXML ( void* aCB, cAppCharP aConfigXML ) {
  return URef( aCB )->InitEngineXML                      ( aConfigXML );
} //                  InitEngineXML

TSyError              InitEngineFile( void* aCB, cAppCharP aConfigFilePath ) {
  return URef( aCB )->InitEngineFile                     ( aConfigFilePath );
} //                  InitEngineFile

TSyError              InitEngineCB  ( void* aCB, TXMLConfigReadFunc aReaderFunc, void* aContext ) {
  return URef( aCB )->InitEngineCB                                ( aReaderFunc,       aContext );
} //                  InitEngineCB


// ----------------------------------------------------------------------------------------
TSyError              OpenSession( void* aCB, SessionH *aSessionH, uInt32 aSelector,
                                              cAppCharP   aSessionName ) {
  return URef( aCB )->OpenSession( *aSessionH, aSelector, aSessionName );
} //                  OpenSession

TSyError              OpenSessionKey( void* aCB, SessionH  aSessionH, KeyH *aKeyH, uInt16 aMode ) {
  return URef( aCB )->OpenSessionKey                     ( aSessionH,      *aKeyH,        aMode );
} //                  OpenSessionKey

TSyError              SessionStep( void* aCB, SessionH aSessionH, uInt16 *aStepCmd,
                                                       TEngineProgressInfo *aInfoP ) {
  return URef( aCB )->SessionStep                   ( aSessionH, *aStepCmd, aInfoP );
} //                  SessionStep

TSyError              GetSyncMLBuffer( void* aCB, SessionH    aSessionH,  bool  aForSend,
                                                  appPointer *aBuffer, memSize *aBufSize ) {
  return URef( aCB )->GetSyncMLBuffer ( aSessionH, aForSend, *aBuffer,         *aBufSize );
} //                  GetSyncMLBuffer

TSyError              RetSyncMLBuffer( void* aCB, SessionH aSessionH,  bool  aForSend,
                                                                       memSize  aRetSize ) {
  return URef( aCB )->RetSyncMLBuffer                    ( aSessionH, aForSend, aRetSize );
} //                  RetSyncMLBuffer

TSyError              ReadSyncMLBuffer ( void* aCB, SessionH   aSessionH,
                                                    appPointer aBuffer, memSize  aBufSize,
                                                                        memSize *aValSize ) {
  return URef( aCB )->ReadSyncMLBuffer   ( aSessionH, aBuffer, aBufSize,        *aValSize );
} //                  ReadSyncMLBuffer

TSyError              WriteSyncMLBuffer( void* aCB, SessionH   aSessionH,
                                                    appPointer aBuffer, memSize aValSize ) {
  return URef( aCB )->WriteSyncMLBuffer           ( aSessionH, aBuffer,         aValSize );
} //                  WriteSyncMLBuffer

TSyError              CloseSession( void* aCB, SessionH aSessionH ) {
  return URef( aCB )->CloseSession                    ( aSessionH );
} //                  CloseSession



// ----------------------------------------------------------------------------------------
TSyError              OpenKeyByPath( void* aCB, KeyH *aKeyH,
                                                KeyH  aParentKeyH, cAppCharP aPath, uInt16 aMode ) {
  return URef( aCB )->OpenKeyByPath         ( *aKeyH, aParentKeyH,           aPath,        aMode );
} //                  OpenKeyByPath

TSyError              OpenSubkey   ( void* aCB, KeyH *aKeyH,
                                                KeyH  aParentKeyH, sInt32 aID, uInt16 aMode ) {
  return URef( aCB )->OpenSubkey            ( *aKeyH, aParentKeyH,        aID,        aMode );
} //                  OpenSubkey

TSyError              DeleteSubkey( void* aCB, KeyH aParentKeyH, sInt32 aID ) {
  return URef( aCB )->DeleteSubkey                ( aParentKeyH,        aID );
} //                  DeleteSubkey

TSyError              GetKeyID( void* aCB, KeyH aKeyH, sInt32 *aID ) {
  return URef( aCB )->GetKeyID                ( aKeyH,        *aID );
} //                  GetKeyID

TSyError              SetTextMode( void* aCB, KeyH aKeyH, uInt16 aCharSet, uInt16 aLineEndMode,
                                                                             bool aBigEndian ) {
  return URef( aCB )->SetTextMode                ( aKeyH, aCharSet, aLineEndMode, aBigEndian );
} //                  SetTextMode

TSyError              SetTimeMode( void* aCB, KeyH aKeyH, uInt16 aTimeMode ) {
  return URef( aCB )->SetTimeMode                ( aKeyH,        aTimeMode );
} //                  SetTimeMode

TSyError              CloseKey( void* aCB, KeyH aKeyH ) {
  return URef( aCB )->CloseKey                ( aKeyH );
} //                  CloseKey



// ----------------------------------------------------------------------------------------
TSyError              GetValue    ( void* aCB, KeyH aKeyH,   cAppCharP  aValName,
                                             uInt16 aValType,
                                         appPointer aBuffer,   memSize  aBufSize,
                                                               memSize *aValSize ) {
  return URef( aCB )->GetValue( aKeyH, aValName, aValType, aBuffer, aBufSize,*aValSize );
} //                  GetValue

TSyError              GetValueByID( void* aCB, KeyH aKeyH,      sInt32  aID,
                                             sInt32 arrIndex,   uInt16  aValType,
                                         appPointer aBuffer,   memSize  aBufSize,
                                                               memSize *aValSize ) {
  return URef( aCB )->GetValueByID( aKeyH, aID, arrIndex, aValType,  aBuffer, aBufSize, *aValSize );
} //                  GetValueByID

sInt32                GetValueID  ( void* aCB, KeyH aKeyH,   cAppCharP  aName ) {
  return URef( aCB )->GetValueID                  ( aKeyH,              aName );
} //                  GetValueID

TSyError              SetValue    ( void* aCB, KeyH aKeyH,   cAppCharP  aValName,
                                             uInt16 aValType,
                                        cAppPointer aBuffer,   memSize  aValSize ) {
  return URef( aCB )->SetValue         ( aKeyH, aValName, aValType,  aBuffer, aValSize );
} //                  SetValue

TSyError              SetValueByID( void* aCB, KeyH aKeyH,      sInt32  aID,
                                             sInt32 arrIndex,   uInt16  aValType,
                                        cAppPointer aBuffer,   memSize  aValSize ) {
  return URef( aCB )->SetValueByID( aKeyH, aID, arrIndex, aValType,  aBuffer, aValSize );
} //                  SetValueByID



// ----------------------------------------------------------------------------------------
TSyError             StartDataRead    ( CContext ac, cAppCharP lastToken, cAppCharP resumeToken ) {
  return TRef( ac )->StartDataRead    (    TCon( ac ),         lastToken,           resumeToken );
} //                 StartDataRead

TSyError             ReadNextItem     ( CContext ac,     ItemID  aID, appCharP *aItemData,
                                                         sInt32 *aStatus, bool  aFirst ) {
  return TRef( ac )->ReadNextItem     (    TCon( ac ), aID, aItemData, aStatus, aFirst );
} //                 ReadNextItem

TSyError             ReadItem         ( CContext ac,    cItemID  aID, appCharP *aItemData ) {
  return TRef( ac )->ReadItem         (    TCon( ac ),           aID,           aItemData );
} //                 ReadItem

TSyError             EndDataRead      ( CContext ac ) {
  return TRef( ac )->EndDataRead      (    TCon( ac ) );
} //                 EndDataRead

TSyError             StartDataWrite   ( CContext ac ) {
  return TRef( ac )->StartDataWrite   (    TCon( ac ) );
} //                 StartDataWrite

TSyError             InsertItem       ( CContext ac, cAppCharP aItemData,  ItemID   aID ) {
  return TRef( ac )->InsertItem       (    TCon( ac ),         aItemData,           aID );
} //                 InsertItem

TSyError             UpdateItem       ( CContext ac, cAppCharP aItemData, cItemID   aID,
                                                                           ItemID updID ) {
  return TRef( ac )->UpdateItem       (    TCon( ac ),         aItemData,    aID, updID );
} //                 UpdateItem

TSyError             MoveItem         ( CContext ac, cItemID aID,    cAppCharP newParID ) {
  return TRef( ac )->MoveItem         (    TCon( ac ),       aID,              newParID );
} //                 MoveItem

TSyError             DeleteItem       ( CContext ac, cItemID aID ) {
  return TRef( ac )->DeleteItem       (    TCon( ac ),       aID );
} //                 DeleteItem

TSyError             EndDataWrite     ( CContext ac, bool success, appCharP *newToken ) {
  return TRef( ac )->EndDataWrite     (    TCon( ac ),    success,           newToken );
} //                 EndDataWrite

void                 DisposeObj       ( CContext ac, void* memory ) {
         TRef( ac )->DisposeObj       (    TCon( ac ),     memory );
} //                 DisposeObj

// ---- asKey functions ----
TSyError             ReadNextItemAsKey( CContext ac,  ItemID aID, KeyH aItemKey,  sInt32 *aStatus,
                                                                                    bool  aFirst ) {
  return TRef( ac )->ReadNextItemAsKey(    TCon( ac ),       aID,      aItemKey, aStatus, aFirst );
} //                 ReadNextItemAsKey

TSyError             ReadItemAsKey    ( CContext ac, cItemID aID, KeyH aItemKey ) {
  return TRef( ac )->ReadItemAsKey    (    TCon( ac ),       aID,      aItemKey );
} //                 ReadItemAsKey

TSyError             InsertItemAsKey  ( CContext ac, KeyH aItemKey,  ItemID   aID ) {
  return TRef( ac )->InsertItemAsKey  (    TCon( ac ),    aItemKey,           aID );
} //                 InsertItemAsKey

TSyError             UpdateItemAsKey  ( CContext ac, KeyH aItemKey, cItemID   aID,
                                                                     ItemID updID ) {
  return TRef( ac )->UpdateItemAsKey  (    TCon( ac ),    aItemKey,    aID, updID );
} //                 UpdateItemAsKey



// ----------------------------------------------------------------------------------------
// connect generic key access routines
void CB_Connect_KeyAccess( void* aCB )
{
  DB_Callback cb= (DB_Callback)aCB;
  if (!CB_OK( cb,8 )) return; // minimum callback version required

  // calls used by both DBApi and UIApi
  cb->ui.OpenKeyByPath    = OpenKeyByPath;
  cb->ui.OpenSubkey       = OpenSubkey;
  cb->ui.DeleteSubkey     = DeleteSubkey;
  cb->ui.GetKeyID         = GetKeyID;
  cb->ui.SetTextMode      = SetTextMode;
  cb->ui.SetTimeMode      = SetTimeMode;
  cb->ui.CloseKey         = CloseKey;

  cb->ui.GetValue         = GetValue;
  cb->ui.GetValueByID     = GetValueByID;
  cb->ui.GetValueID       = GetValueID;
  cb->ui.SetValue         = SetValue;
  cb->ui.SetValueByID     = SetValueByID;
} // CB_Connect_KeyAccess


static void CB_Connect_Tunnel( void* aCB )
{
  DB_Callback cb= (DB_Callback)aCB;
  if (!CB_OK( cb, 9 )) return; // minimum callback version required

  cb->dt.StartDataRead    = StartDataRead;
  cb->dt.ReadNextItem     = ReadNextItem;
  cb->dt.ReadItem         = ReadItem;
  cb->dt.EndDataRead      = EndDataRead;

  cb->dt.StartDataWrite   = StartDataWrite;
  cb->dt.InsertItem       = InsertItem;
  cb->dt.UpdateItem       = UpdateItem;
  cb->dt.MoveItem         = MoveItem;
  cb->dt.DeleteItem       = DeleteItem;
  cb->dt.EndDataWrite     = EndDataWrite;


  if (!CB_OK( cb,11 )) return; // minimum callback version required

  cb->dt.DisposeObj       = DisposeObj;

  cb->dt.ReadNextItemAsKey= ReadNextItemAsKey;
  cb->dt.ReadItemAsKey    = ReadItemAsKey;
  cb->dt.InsertItemAsKey  = InsertItemAsKey;
  cb->dt.UpdateItemAsKey  = UpdateItemAsKey;
} // CB_Connect_Tunnel


void CB_Connect( void* aCB )
{
  DB_Callback cb= (DB_Callback)aCB;
  if (!CB_OK( cb,8 )) return; // minimum callback version required

  // calls needed for accessing engine from the outside (UIApi)
  cb->ui.SetStringMode    = SetStringMode;
  cb->ui.InitEngineXML    = InitEngineXML;
  cb->ui.InitEngineFile   = InitEngineFile;
  cb->ui.InitEngineCB     = InitEngineCB;

  cb->ui.OpenSession      = OpenSession;
  cb->ui.OpenSessionKey   = OpenSessionKey;
  cb->ui.SessionStep      = SessionStep;
  cb->ui.GetSyncMLBuffer  = GetSyncMLBuffer;
  cb->ui.RetSyncMLBuffer  = RetSyncMLBuffer;
  cb->ui.ReadSyncMLBuffer = ReadSyncMLBuffer;
  cb->ui.WriteSyncMLBuffer= WriteSyncMLBuffer;
  cb->ui.CloseSession     = CloseSession;

  // generic key access calls used by both DBApi and UIApi
  CB_Connect_KeyAccess( cb );
  CB_Connect_Tunnel   ( cb );
} // CB_Connect


void SySyncDebugPuts(void* aCB,
                     cAppCharP aFile, int aLine, cAppCharP aFunction,
                     int aDbgLevel, cAppCharP aLinePrefix,
                     cAppCharP aText)
{
  URef( aCB )->debugPuts(aFile, aLine, aFunction,
                         aDbgLevel, aLinePrefix, aText);
}

} // namespace sysync

// eof
