/*
 *  File:    enginemodulebridge.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  Synthesis SyncML client bridge connector
 *
 *  Copyright (c) 2007-2011 by Synthesis AG + plan44.ch
 *
 *
 */


#ifndef ENGINEMODULEBRIDGE_H
#define ENGINEMODULEBRIDGE_H

// inherited from ...
#include "enginemodulebase.h"


namespace sysync {


class TEngineModuleBridge : public TEngineModuleBase
{
  typedef TEngineModuleBase inherited;

  public:
             TEngineModuleBridge();
    virtual ~TEngineModuleBridge();

    appPointer fDLL;
    bool       fIsServer;

    virtual TSyError Init();
    virtual TSyError Term();

    // ---- Engine init -------------------------------------------------------------------------
    virtual TSyError SetStringMode    ( uInt16 aCharSet,
                                        uInt16 aLineEndMode= LEM_CSTR, bool aBigEndian= false );

    virtual TSyError InitEngineXML    ( cAppCharP          aConfigXML );
    virtual TSyError InitEngineFile   ( cAppCharP          aConfigFilePath );
    virtual TSyError InitEngineCB     ( TXMLConfigReadFunc aReaderFunc, void* aContext );

    // ---- Running a Sync Session --------------------------------------------------------------
    virtual TSyError OpenSession      ( SessionH   &aSessionH,      uInt32  aSelector    = 0,
                                        cAppCharP   aSessionName = NULL );
    virtual TSyError OpenSessionKey   ( SessionH    aSessionH, KeyH &aKeyH, uInt16  aMode );
    virtual TSyError SessionStep      ( SessionH    aSessionH,              uInt16 &aStepCmd, TEngineProgressInfo *aInfoP= NULL );
    virtual TSyError GetSyncMLBuffer  ( SessionH    aSessionH, bool    aForSend,
                                        appPointer &aBuffer,                     memSize &aBufSize );
    virtual TSyError RetSyncMLBuffer  ( SessionH    aSessionH, bool    aForSend, memSize  aRetSize );
    virtual TSyError ReadSyncMLBuffer ( SessionH    aSessionH,
                                        appPointer  aBuffer,   memSize aBufSize, memSize &aValSize );
    virtual TSyError WriteSyncMLBuffer( SessionH    aSessionH,
                                        appPointer  aBuffer,   memSize aValSize );
    virtual TSyError CloseSession     ( SessionH    aSessionH );

    // ---- Settings access ---------------------------------------------------------------------
    virtual TSyError OpenKeyByPath    ( KeyH       &aKeyH,
                                        KeyH        aParentKeyH, cAppCharP  aPath, uInt16 aMode );
    virtual TSyError OpenSubkey       ( KeyH       &aKeyH,
                                        KeyH        aParentKeyH,    sInt32  aID,   uInt16 aMode );
    virtual TSyError DeleteSubkey     ( KeyH        aParentKeyH,    sInt32  aID );
    virtual TSyError GetKeyID         ( KeyH        aKeyH,          sInt32 &aID );
    virtual TSyError SetTextMode      ( KeyH        aKeyH,          uInt16  aCharSet,  uInt16  aLineEndMode= LEM_CSTR,
                                                                                         bool  aBigEndian  = false );
    virtual TSyError SetTimeMode      ( KeyH        aKeyH,          uInt16  aTimeMode );
    virtual TSyError CloseKey         ( KeyH        aKeyH );

    virtual TSyError GetValue         ( KeyH        aKeyH,       cAppCharP  aValName,  uInt16  aValType,
                                        appPointer  aBuffer,       memSize  aBufSize, memSize &aValSize );
    virtual TSyError GetValueByID     ( KeyH        aKeyH,          sInt32  aID,       sInt32  arrIndex,
                                        uInt16      aValType,
                                        appPointer  aBuffer,       memSize  aBufSize, memSize &aValSize );
    virtual sInt32   GetValueID       ( KeyH        aKeyH,       cAppCharP  aName );

    virtual TSyError SetValue         ( KeyH        aKeyH,       cAppCharP  aValName,  uInt16  aValType,
                                        cAppPointer aBuffer,                          memSize  aValSize );
    virtual TSyError SetValueByID     ( KeyH        aKeyH,          sInt32  aID,       sInt32  arrIndex,
                                        uInt16      aValType,
                                        cAppPointer aBuffer,                          memSize  aValSize );

    // ---- Tunnel methods ----------------------------------------------------------------------
    virtual TSyError StartDataRead    ( SessionH    ac,  cAppCharP   lastToken,
                                                         cAppCharP resumeToken );
    virtual TSyError ReadNextItem     ( SessionH    ac,     ItemID aID,      appCharP *aItemData,
                                                                               sInt32 *aStatus, bool aFirst );
    virtual TSyError ReadItem         ( SessionH    ac,    cItemID aID,      appCharP *aItemData );
    virtual TSyError EndDataRead      ( SessionH    ac );
    virtual TSyError StartDataWrite   ( SessionH    ac );
    virtual TSyError InsertItem       ( SessionH    ac,  cAppCharP aItemData,  ItemID  aID );
    virtual TSyError UpdateItem       ( SessionH    ac,  cAppCharP aItemData, cItemID  aID, ItemID updID );
    virtual TSyError MoveItem         ( SessionH    ac,    cItemID aID,     cAppCharP           newParID );
    virtual TSyError DeleteItem       ( SessionH    ac,    cItemID aID );
    virtual TSyError EndDataWrite     ( SessionH    ac,       bool success,  appCharP *newToken );
    virtual void     DisposeObj       ( SessionH    ac,      void* memory );

    // ---- asKey ----
    virtual TSyError ReadNextItemAsKey( SessionH    ac,     ItemID aID,    KeyH  aItemKey,
                                                                         sInt32 *aStatus, bool aFirst );
    virtual TSyError ReadItemAsKey    ( SessionH    ac,    cItemID aID,    KeyH  aItemKey );
    virtual TSyError InsertItemAsKey  ( SessionH    ac, KeyH aItemKey,   ItemID  aID );
    virtual TSyError UpdateItemAsKey  ( SessionH    ac, KeyH aItemKey,  cItemID  aID, ItemID updID );
}; // TEngineModuleBridge

}  // namespace
#endif // ENGINEMODULEBRIDGE
/* eof */
