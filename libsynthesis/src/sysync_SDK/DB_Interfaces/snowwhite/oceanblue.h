/*  
 *  File:    oceanblue.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *  Base class for datastore plugins in C++
 *
 *  Copyright (c) 2008-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef OCEANBLUE_H
#define OCEANBLUE_H

#include "sync_include.h"   // include general SDK definitions
#include "sync_dbapidef.h"  // include the interface file and utilities
#include "SDK_util.h"       // include SDK utilities
#include "SDK_support.h"    // include more SDK support

// There are reference implementations for admin data handling and for BLOBs
// But they needn't to be used. The "target_options.h" defines whether to use them
#ifndef DISABLE_PLUGIN_DATASTOREADMIN
  #include "admindata.h"
#endif
#ifndef DISABLE_PLUGIN_BLOBS
  #include "blobs.h"
#endif

// Use them in two different namespaces
namespace oceanblue {
  using sysync::sInt16;
  using sysync::uInt16;
  using sysync::sInt32;
  using sysync::uInt32;
  using sysync::appPointer;
  using sysync::memSize;
  
  using sysync::DB_Callback;
  using sysync::MapID;
  using sysync::cMapID;
  using sysync::ItemID;
  using sysync::cItemID;
  using sysync::KeyH;

#ifndef DISABLE_PLUGIN_DATASTOREADMIN
  using sysync::TAdminData;
#endif
#ifndef DISABLE_PLUGIN_BLOBS
  using sysync::TBlob;
#endif


// The three classes "OceanBlue_Module", "OceanBlue_Session" and "OceanBlue" should
// be derived as shown in the "snowwhite" adapter example.
// It is intended not to change this module, but only the derived adapter.
// The name of the derived adapter is defined "myadapter.h"


// Module access
class OceanBlue_Module
{
  public:
    virtual ~OceanBlue_Module() { }; // virtual destructor

    DB_Callback fCB;          // local callback reference
    string      fModName;     // the module  name
    string      fContextName; // the context name
    
    virtual TSyError CreateContext ( cAppCharP  subName );
    virtual CVersion Version();
    virtual TSyError Capabilities   ( appCharP &mCapabilities );
    virtual TSyError PluginParams  ( cAppCharP  mConfigParams, CVersion engineVersion );
    virtual void     DisposeObj        ( void*  memory );                                
    virtual TSyError DeleteContext();
}; // OceanBlue_Module


// Session access
class OceanBlue_Session
{
  public:
    virtual ~OceanBlue_Session() { }; // virtual destructor
    
    DB_Callback fCB;          // local callback reference
    string      fSessionName; // the session name
  
    virtual TSyError CreateContext();
    
    // -- device
    virtual TSyError CheckDevice    ( cAppCharP  aDeviceID, 
                                       appCharP &sDevKey,
                                       appCharP &nonce );
    virtual TSyError GetNonce       (  appCharP &nonce );
    virtual TSyError SaveNonce      ( cAppCharP  nonce );
    virtual TSyError SaveDeviceInfo ( cAppCharP  aDeviceInfo );
    virtual TSyError GetDBTime      (  appCharP &currentDBTime );

    // -- login
    virtual sInt32   PasswordMode();
    virtual TSyError Login          ( cAppCharP  sUsername, 
                                       appCharP &sPassword, appCharP &sUsrKey );
    virtual TSyError Logout();

    // -- general
    virtual void     DisposeObj         ( void*  memory );
    virtual void     ThreadMayChangeNow();
    virtual void     DispItems           ( bool  allFields, 
                                      cAppCharP  specificItem );
    
    virtual TSyError Adapt_Item      ( appCharP &sItemData1, 
                                       appCharP &sItemData2,
                                       appCharP &sLocalVars, uInt32 sIdentifier );
    virtual TSyError DeleteContext();
}; // OceanBlue_Session


// Datastore access
class OceanBlue
{
  public:
    virtual ~OceanBlue() { };  // virtual destructor

    DB_Callback  fCB;          // local callback reference
    string       fContextName; // the context name
    bool         fAsAdmin;     // true, if it is admin context
    uInt32       fNthItem;

    #ifndef DISABLE_PLUGIN_DATASTOREADMIN
      TAdminData fAdmin;       // an internal ADMIN implementation can be used  
    #endif
    #ifndef DISABLE_PLUGIN_BLOBS
      TBlob      fBlob;        // an internal BLOB implementation can be used  
    #endif

    
    virtual TSyError CreateContext     ( cAppCharP  sDevKey, cAppCharP sUsrKey );
    virtual uInt32   ContextSupport    ( cAppCharP  aContextRules );
    virtual uInt32   FilterSupport     ( cAppCharP  aFilterRules  );

    // -- admin
    virtual TSyError LoadAdminData     ( cAppCharP  aLocDB,
                                         cAppCharP  aRemDB, appCharP &adminData );
    virtual TSyError LoadAdminDataAsKey( cAppCharP  aLocDB,
                                         cAppCharP  aRemDB,     KeyH  adminKey  );

    virtual TSyError SaveAdminData                       ( cAppCharP  adminData );
    virtual TSyError SaveAdminDataAsKey                       ( KeyH  adminKey  );

    virtual bool     ReadNextMapItem      (  MapID  mID, bool aFirst );
    virtual TSyError InsertMapItem        ( cMapID  mID );
    virtual TSyError UpdateMapItem        ( cMapID  mID );
    virtual TSyError DeleteMapItem        ( cMapID  mID );

    // -- read
    virtual TSyError StartDataRead  ( cAppCharP lastToken, cAppCharP resumeToken );

    virtual TSyError ReadNextItem      ( ItemID  aID,      appCharP &aItemData, 
                                         sInt32 &aStatus,      bool  aFirst );
    virtual TSyError ReadNextItemAsKey ( ItemID  aID,          KeyH  aItemKey,
                                         sInt32 &aStatus,      bool  aFirst );

    virtual TSyError ReadItem         ( cItemID  aID,      appCharP &aItemData );
    virtual TSyError ReadItemAsKey    ( cItemID  aID,          KeyH  aItemKey  );
    virtual TSyError EndDataRead();

    // -- write
    virtual TSyError StartDataWrite();
    virtual TSyError InsertItem     ( cAppCharP  aItemData,               ItemID newID );
    virtual TSyError InsertItemAsKey(      KeyH  aItemKey,                ItemID newID );
    virtual TSyError UpdateItem     ( cAppCharP  aItemData, cItemID  aID, ItemID updID );
    virtual TSyError UpdateItemAsKey(      KeyH  aItemKey,  cItemID  aID, ItemID updID );
    
    virtual TSyError MoveItem         ( cItemID  aID,     cAppCharP  newParID );
    virtual TSyError DeleteItem       ( cItemID  aID );
    virtual TSyError FinalizeLocalID  ( cItemID  aID,        ItemID  updID );
    virtual TSyError DeleteSyncSet();
    virtual TSyError EndDataWrite        ( bool  success,  appCharP &newToken );
    
    // -- blobs
    virtual TSyError ReadBlob         ( cItemID  aID,     cAppCharP  aBlobID,
                                     appPointer &aBlkPtr,   memSize &aBlkSize, 
                                                            memSize &aTotSize,
                                           bool  aFirst,       bool &aLast );
    virtual TSyError WriteBlob        ( cItemID  aID,     cAppCharP  aBlobID,
                                     appPointer  aBlkPtr,   memSize  aBlkSize, 
                                                            memSize  aTotSize,
                                           bool  aFirst,       bool  aLast );
                                            
    virtual TSyError DeleteBlob       ( cItemID  aID,     cAppCharP  aBlobID  );
    
    // -- general
    virtual void     WriteLogData   ( cAppCharP  logData );
    virtual void     DisposeObj         ( void*  memory );
    virtual void     ThreadMayChangeNow();
    virtual void     DispItems           ( bool  allFields,
                                      cAppCharP  specificItem );

    virtual TSyError Adapt_Item      ( appCharP &aItemData1,
                                       appCharP &aItemData2,
                                       appCharP &aLocalVars, uInt32 aIdentifier );
    virtual TSyError DeleteContext();
    
    
    
    // ---- call-in functions ----
    virtual sInt32   GetValueID  ( KeyH aItemKey, string  aValName,    string  suff="" );
    
    virtual TSyError GetValue    ( KeyH aItemKey, string  aValName,    uInt16  aValType,
                             appPointer aBuffer, memSize  aBufSize,   memSize &aValSize,
                                                                       string  suff="" );
    virtual TSyError GetValueByID( KeyH aItemKey, sInt32  aID,         sInt32  arrIndex, 
                                                                       uInt16  aValType,
                             appPointer aBuffer, memSize  aBufSize,   memSize &aValSize,
                                                                       string  suff="" );
                                                                                 
    virtual TSyError SetValue    ( KeyH aItemKey, string  aValName,    uInt16  aValType,
                             appPointer aBuffer, memSize  aValSize,    string  suff="" );   
    virtual TSyError SetValueByID( KeyH aItemKey, sInt32  aID,         sInt32  arrIndex, 
                                                                       uInt16  aValType,
                             appPointer aBuffer, memSize  aValSize,    string  suff="" );
                                   
    // ---- convenience routines to access Get/SetValue for specific types ----
    /* string */
    virtual TSyError GetStr      ( KeyH aItemKey, string  aValName,    string &aText,
                                                                       string  suff="" );
    virtual TSyError GetStrByID  ( KeyH aItemKey, sInt32  aID,         string &aText,
                                                  sInt32  arrIndex= 0, string  suff="" );
    virtual TSyError SetStr      ( KeyH aItemKey, string  aValName,    string  aText, 
                                                                       string  suff="" );
    virtual TSyError SetStrByID  ( KeyH aItemKey, sInt32  aID,         string  aText,
                                                  sInt32  arrIndex= 0, string  suff="" );
    
    
    /* sInt16 / uInt16 */
    virtual TSyError GetInt16    ( KeyH aItemKey, string  aValName,    sInt16 &aValue, 
                                                                       string  suff="" );
    virtual TSyError GetInt16    ( KeyH aItemKey, string  aValName,    uInt16 &aValue,
                                                                       string  suff="" );
    
    virtual TSyError GetInt16ByID( KeyH aItemKey, sInt32  aID,         sInt16 &aValue,
                                                  sInt32  arrIndex= 0, string  suff="" );
    virtual TSyError GetInt16ByID( KeyH aItemKey, sInt32  aID,         uInt16 &aValue,
                                                  sInt32  arrIndex= 0, string  suff="" );
    
    virtual TSyError SetInt16    ( KeyH aItemKey, string  aValName,    sInt16  aValue,
                                                                       string  suff="" );
    virtual TSyError SetInt16ByID( KeyH aItemKey, sInt32  aID,         sInt16  aValue,
                                                  sInt32  arrIndex= 0, string  suff="" );
                                                                                                                
    /* sInt32 / uInt32 */
    virtual TSyError GetInt32    ( KeyH aItemKey, string  aValName,    sInt32 &aValue, 
                                                                       string  suff="" );
    virtual TSyError GetInt32    ( KeyH aItemKey, string  aValName,    uInt32 &aValue,
                                                                       string  suff="" );
    
    virtual TSyError GetInt32ByID( KeyH aItemKey, sInt32  aID,         sInt32 &aValue,
                                                  sInt32  arrIndex= 0, string  suff="" );
    virtual TSyError GetInt32ByID( KeyH aItemKey, sInt32  aID,         uInt32 &aValue,
                                                  sInt32  arrIndex= 0, string  suff="" );
    
    virtual TSyError SetInt32    ( KeyH aItemKey, string  aValName,    sInt32  aValue, 
                                                                       string  suff="" );

    virtual TSyError SetInt32ByID( KeyH aItemKey, sInt32  aID,         sInt32  aValue,
                                                  sInt32  arrIndex= 0, string  suff="" );
    
    /* suffix handling */                                              
    virtual void AppendSuffixToID( KeyH aItemKey, sInt32 &aID,      cAppCharP  suff    );
}; // OceanBlue


} // namespace
#endif // OCEANBLUE_H
/* eof */
