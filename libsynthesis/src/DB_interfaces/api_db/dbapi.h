/*
 *  File:    dbapi.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *  TDB_Api class
 *    Bridge to user programmable interface
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 *  The "TDB_Api" class acts as a standard interface between
 *  the SySync Server and a (user programmable) module "sync_dbapi".
 *
 *  It is possible to have more than one (identical) interface module,
 *  either packed into a DLL or directly linked to the server
 *  (or combined).
 *
 *  - In case of DLL, the object must be created with
 *    <interfaceName> = DLL file name, e.g "sync_dbapi.dll"
 *    The constructor automatically creates the connection to the DLL,
 *    the destructor releases the DLL.
 *
 *  - In case of the directly linked module, it runs in a
 *    specific namespace, which can be assigned by calling
 *    <interfaceName> = '['<namespace>']', e.g. "[example1]".
 *    (Of course an internal implementation for each namespace must
 *    exist!)
 *
 *  NOTE: The public method names are identical to the ones of the
 *        user interface, but not all parameters are visible on the
 *        object side, e.g. the context variable is held private
 *        within the object.
 *        The methods will be assigned internally (at API_Methods),
 *        when the object is created.
 */


#ifndef DB_API_H
#define DB_API_H

// access to the definitions of the interface
#include <list>
#include <string>
#include "sync_dbapiconnect.h"
#include "sync_dbapidef.h"


namespace sysync {


/* -- Utility procs -- */
bool DSConnect( cAppCharP aItem );



/* -- handling for "sync_dbapi" returned strings -- */
class TDB_Api_Str
{
  friend class TDB_Api_Config;
  friend class TDB_Api_Session;
  friend class TDB_Api;

  public:
    TDB_Api_Str();            // constructor
    TDB_Api_Str( string &s ); // alternative constructor
   ~TDB_Api_Str();            // destructor

    // the string reference
    cAppCharP c_str() const { return (fStr==NULL) ? "":fStr; }
    bool empty(void)        { return *c_str()==0; }
    int  length() const     { return sizeof( c_str() ); };

    // has the same effect as the destructor, but can be called earlier
    void DisposeStr();

  private:
    void AssignStr ( CContext aContext, DisposeProc aDisposeProc, bool itself= false );
    void LocalAlloc( CContext aContext, cAppCharP ); // if memory must be allocated locally

    CContext       fContext;
    char*          fStr;
    bool           fItself; // if true: Dispose <this> instead of <fStr>
    DisposeProc    fDisposeProc;
    void clear() { fDisposeProc= NULL; fStr= NULL; }
}; // TDB_Api_Str



/* -- handling for "sync_dbapi" returned itemID/parentID strings */
class TDB_Api_ItemID
{
  public:
    TDB_Api_Str item;
    TDB_Api_Str parent;
}; // TDB_Api_ItemID



class TDB_Api_MapID
{
  public:
    TDB_Api_Str  localID;
    TDB_Api_Str remoteID;
    uInt16         flags;
    uInt8          ident;
}; // TDB_Api_MapID



/* -- handling for "sync_dbapi" returned blocks */
class TDB_Api_Blk
{
  friend class TDB_Api_Config;
  friend class TDB_Api;

  public:
    TDB_Api_Blk();           // constructor
   ~TDB_Api_Blk();           //  destructor

    appPointer fPtr;         // the block reference
    memSize    fSize;        // the block size
    void       DisposeBlk(); // has the same effect as the destructor, but can be called earlier

  protected:
    void AssignBlk( CContext aContext, DisposeProc aDisposeProc, bool itself= false );

  private:
    CContext       fContext;
    bool           fItself; // if true: Dispose <this> instead of <fPtr>
    DisposeProc    fDisposeProc;
    void clear() { fDisposeProc= NULL; fPtr= NULL; fSize= 0; }
}; // TDB_Api_Blk



// -- internally used type definitions ---------
typedef TSyError  (*CreateM_Func)( CContext *mc, cAppCharP    moduleName,
                                                 cAppCharP       subName,
                                                 cAppCharP  mContextName, DB_Callback mCB );
typedef CVersion  (*Version_Func)( CContext  mc );
typedef TSyError  (*Context_Func)( CContext  mc );

// ---------------------------------------------
typedef TSyError  (*CreateS_Func)( CContext *sc, cAppCharP  sessionName,  DB_Callback sCB );
typedef TSyError   (*SvInfo_Func)( CContext  sc, cAppCharP  info );
typedef int        (*PwMode_Func)( CContext  sc );
typedef TSyError    (*Login_Func)( CContext  sc, cAppCharP  sUsername,
                                                  appCharP *sPassword,
                                                  appCharP *sUsrKey );
// ---------------------------------------------
typedef TSyError  (*CreateD_Func)( CContext *ac, cAppCharP  aContextName, DB_Callback aCB,
                                                 cAppCharP  sDevKey,
                                                 cAppCharP  sUsrKey );
typedef int          (*Text_Func)( CContext  ac, cAppCharP  aText );
typedef void          (*DispProc)( CContext  ac, bool       allFields, cAppCharP specificItem );

typedef TSyError (*LoadAdm_SFunc)( CContext  ac, cAppCharP  aLocDB,
                                                 cAppCharP  aRemDB,
                                                  appCharP *adminData );
typedef TSyError (*LoadAdm_KFunc)( CContext  ac, cAppCharP  aLocDB,
                                                 cAppCharP  aRemDB,
                                                      KeyH  adminKey );
typedef TSyError (*SaveAdm_SFunc)( CContext  sc, cAppCharP  info );
typedef TSyError (*SaveAdm_KFunc)( CContext  ac,      KeyH  adminKey );

typedef bool       (*RdNMap_Func)( CContext  ac,     MapID  mID, bool aFirst );
typedef TSyError   (*InsMap_Func)( CContext  ac,    cMapID  mID );
typedef TSyError   (*UpdMap_Func)( CContext  ac,    cMapID  mID );
typedef TSyError   (*DelMap_Func)( CContext  ac,    cMapID  mID );

typedef void          (*VoidProc)( CContext  ac );

typedef TSyError    (*Adapt_Func)( CContext  ac,  appCharP *aItemData1,
                                                  appCharP *aItemData2,
                                                  appCharP *aLocalVars, sInt32 aIdentifier );
// ---------------------------------------------



// wrapper for DB_Callback
class TDB_Api_Callback
{
  public:
    TDB_Api_Callback();
    SDK_Interface_Struct Callback;
}; // TDB_Api_Callback


// the module context class
class TDB_Api_Config
{
  friend class TDB_Api_Session;
  friend class TDB_Api;
  friend class TUI_Api;

  public:
    TDB_Api_Config(); // constructor
   ~TDB_Api_Config(); //  destructor

    // Connect to the plug-in <moduleName> with <mContextName>
    // <globContext> must be 0 before called the first time
    TSyError Connect( cAppCharP moduleName,   CContext &globContext,
                      cAppCharP mContextName= "", bool aIsLib= false, bool allowDLL= true );

    bool     Connected() { return fConnected; } // read status of <fConnected>

    // The plug-in module's version, capabilities and plugin params from config file
    long          Version(); // the plugin's          SDK version number
    long EngineSDKVersion(); // the internal engine's SDK version number
    TSyError Capabilities( TDB_Api_Str &mCapabilities );
    TSyError PluginParams( cAppCharP    mConfigParams );

    // Change the internal engine's SDK version;
    // *** override the version number / for test only ***
    void SetVersion( long versionNr );

    /* Check the minimum required version */
    TSyError MinVersionCheck( string capa, CVersion &vMin ); // Check, if MINVERSION is high enough

    // Disconnect the module(at the end
    TSyError Disconnect();

    bool             is_lib; // flag: as internal library
    TDB_Api_Callback fCB;    // Callback wrapper

    cAppCharP ModName()     { return fModName.c_str(); } // the <moduleName>
    cAppCharP ModOptions()  { return fOptions.c_str(); } // the module's parameters
    cAppCharP ModPlugin()   { return fPlugin.c_str();  } // the main module's name
    cAppCharP ModMainName() { return fModMain.c_str(); } // the main module's name (with optional extension ":..."
    cAppCharP ModSubName()  { return fModSub.c_str();  } // the sub  module's name (with params)

    void DisposeStr( TDB_Api_Str &s );
  private:
    void  AssignStr( TDB_Api_Str &s ); // for internal use
    void  clear();

    bool Supported( CVersion version_feature ); // Check, if <version_feature> is supported

    // Internal Api assignment
    TSyError DBApi_Assign( cAppCharP item, appPointer aField, memSize aFieldSize, cAppCharP aKey= "" );

    CContext           mContext;     // the module's context
    API_Methods        m;            // set of Lib/DLL routines

    string             fModName;     // local copy of <aModName>
    string             fOptions;     // local copy of module's paramters
    string             fPlugin;      // main part  of <aModName> w/o brackets  w/o extension
    string             fModMain;     // main part  of <aModName> w/o brackets, with optional extension
    string             fModSub;      // sub  part  of <aModName> w/o brackets
    string             fDesc;        // module's description

    bool               fConnected;   // if successful= API_Methods valid
    bool               fADMIN_Info;  // ADMIN info will be given with "CreateContext"

    CVersion           fSDKversion;  // The SDK's version (directly connected module)
    CVersion           fMODversion;  // The SDK's version (lowest of the chain)
    CVersion           fTSTversion;  // Modified  version (*** for test only ***)
    appPointer         fMod;         // internal module reference
    list<TDB_Api_Str*> fSList;       // list of currently allocated str objects of this session
}; // class TDB_Api_Config


// Display global context contents
void DispGlobContext  ( CContext &globContext, DB_Callback mCB );

// Delete the global context at the end
void DeleteGlobContext( CContext &globContext, DB_Callback mCB, bool emptyTextOnly= false );


// the session context class
class TDB_Api_Session
{
  public:
    TDB_Api_Session(); // constructor

    TSyError CreateContext ( cAppCharP sessionName, TDB_Api_Config &config );

    TSyError CheckDevice   ( cAppCharP deviceID, TDB_Api_Str &sDevKey, TDB_Api_Str &nonce );
    TSyError GetNonce                                                ( TDB_Api_Str &nonce );
    TSyError SaveNonce                                               ( cAppCharP    nonce );
    TSyError SaveDeviceInfo( cAppCharP       aDeviceInfo );
    TSyError GetDBTime     ( TDB_Api_Str  &currentDBTime );

    #ifdef SYSYNC_ENGINE
    TSyError GetDBTime     ( lineartime_t &currentDBTime, GZones* g );
    #endif

    sInt32   PasswordMode();
    TSyError Login( cAppCharP    sUsername,
                    cAppCharP    sPassword, TDB_Api_Str &sUsrKey ); // for password mode 0+3
    TSyError Login( cAppCharP    sUsername,
                    TDB_Api_Str &sPassword, TDB_Api_Str &sUsrKey ); // for password mode 1+2
    TSyError Logout();

    TSyError AdaptItem( string &sItemData1,
                        string &sItemData2,
                        string &sLocalVars, uInt32 sIdentifier );

    void     ThreadMayChangeNow(); // notification for the context
    void     DispItems( bool allFields= true, cAppCharP specificItem= "" ); // by default: all
    TSyError DeleteContext();

    TDB_Api_Callback fCB;              // Callback wrapper

    bool     sCreated;
    CContext sContext;                 // the session context

    void DisposeStr( TDB_Api_Str &s );
  private:
    void AssignStr ( TDB_Api_Str &s ); // for internal use
    void AssignChanged  ( string &a, TDB_Api_Str &u );

    API_Methods* dm;           // local reference to the API methods
    API_Methods  sNo_dbapi;    // default connection for call methods
    string       sSessionName; // local copy of <sessionName>
    sInt32       sPwMode;      // local copy of password mode

    list<TDB_Api_Str*> fSList; // list of currently allocated str objects of this session
}; // class TDB_Api_Session



// for detailed description of the methods see
// C interface definition at "sync_dbapi.h"

// An object for each context must be created.
// It is not possible to create more than one context per object
// It is allowed to bind more than one context to the same module.
class TDB_Api
{
  public:
    TDB_Api(); // constructor
   ~TDB_Api(); //  destructor

    // --- open section --------------------------------
    //! Open a context, it is possible to do this w/o any session
    TSyError CreateContext( cAppCharP aContextName, bool asAdmin, TDB_Api_Config*  config,
                            cAppCharP sDevKey= "",
                            cAppCharP sUsrKey= "",                TDB_Api_Session* session= NULL );

    /*! Gets true after calling 'CreateContext'.
     *  Will return again false after 'DeleteContext'
     */
    bool Created() { return fCreated; };


    //! run a sequence: CreateContext -> StartDataRead -> sequence -> EndDataWrite -> DeleteContext
    //! <token> as  input: the token of the last session, "" if the first time.
    //!    "    as output: the new token, which must be given to the next session.
    typedef TSyError(*SequenceProc)( TDB_Api &dbApi );
    TSyError    RunContext( cAppCharP    aContextName, bool asAdmin,
                            SequenceProc sequence,
                            string      &token,        TDB_Api_Config*  config,
                            cAppCharP    sDevKey= "",
                            cAppCharP    sUsrKey= "",  TDB_Api_Session* session= NULL );


    //! returns the nth config data field, which is supported (and activated)
    //! result= 0: no filter supported.
    uInt32 ContextSupport( cAppCharP aContextRules );


    //! returns the nth filter rule, which is supported (and activated)
    //! result= 0: no filter supported.
    uInt32  FilterSupport( cAppCharP aFilterRules  );



    // --- admin section -------------------------------
    //! This function gets the stored information about the record with the four paramters:
    //! <sDevKey>, <sUsrKey> (taken from the session context)
    //! <aLocDB>,  <aRemDB>.
    //! And the same for AsKey (will be activated with ADMIN_AS_KEY)
    TSyError LoadAdminData     ( cAppCharP aLocDB,
                                 cAppCharP aRemDB, TDB_Api_Str &adminData );
    TSyError LoadAdminDataAsKey( cAppCharP aLocDB,
                                 cAppCharP aRemDB,       KeyH   adminKey  );

    //! This functions stores the new <adminData> for this context
    //! And the same for AsKey (will be activated with ADMIN_AS_KEY)
    TSyError SaveAdminData      ( cAppCharP adminData );
    TSyError SaveAdminData_AsKey( KeyH      adminKey  );


    // --- Map table handling
    //! Get a map item of this context
    bool   ReadNextMapItem( TDB_Api_MapID &mID, bool aFirst= false );

    //! Insert a map item of this context
    TSyError InsertMapItem( MapID mID );

    //! Update a map item of this context
    TSyError UpdateMapItem( MapID mID );

    //! Delete a map item of this context
    TSyError DeleteMapItem( MapID mID );



    // --- utility section -----------------------------
    //! Get the current context value
    CContext MyContext() { return fContext; }

    //! Notification for the context
    void ThreadMayChangeNow();

    //! Write log information for the datastore access
    void WriteLogData( cAppCharP logData );

    //! Display the current items (not used by the engine)
    void DispItems( bool allFields= true, cAppCharP specificItem= "" ); // by default: all


    //! Adapt <aItemData1>,<aItemData2>,<aLocalVars> of script context, with <aIdentifier>
    TSyError AdaptItem( string &aItemData1,
                        string &aItemData2,
                        string &aLocalVars, uInt32 aIdentifier );

    // --- read section --------------------------------
    TSyError StartDataRead( cAppCharP lastToken, cAppCharP resumeToken= "" );

    // <aID>/<aItemID> and <aItemData> will automatically be disposed at the beginning
    // 2 overloaded versions
    TSyError ReadNextItem     ( TDB_Api_ItemID  &aID, TDB_Api_Str &aItemData, int &aStatus, bool aFirst= false );
    TSyError ReadNextItem     ( TDB_Api_Str &aItemID, TDB_Api_Str &aItemData, int &aStatus, bool aFirst= false );
    TSyError ReadNextItemAsKey( TDB_Api_ItemID  &aID,       KeyH   aItemKey,  int &aStatus, bool aFirst= false );


    // <aItemData> will automatically be disposed at the beginning
    // 3 overloaded versions
    TSyError ReadItem     ( ItemID_Struct   aID, TDB_Api_Str &aItemData );
    TSyError ReadItem     ( cAppCharP   aItemID, TDB_Api_Str &aItemData );
    TSyError ReadItem     ( cAppCharP   aItemID,
                            cAppCharP aParentID, TDB_Api_Str &aItemData );
    TSyError ReadItemAsKey( ItemID_Struct   aID,       KeyH   aItemKey  );

    // <aBlk> will automatically be disposed at the beginning
    // 3 overloaded versions
    TSyError ReadBlob( ItemID_Struct   aID, cAppCharP  aBlobID,
                                              memSize  blkSize,
                       TDB_Api_Blk   &aBlk,   memSize &totSize, bool aFirst, bool &aLast );

    TSyError ReadBlob( cAppCharP   aItemID, cAppCharP  aBlobID,
                                              memSize  blkSize,
                       TDB_Api_Blk   &aBlk,   memSize &totSize, bool aFirst, bool &aLast );

    TSyError ReadBlob( cAppCharP   aItemID,
                       cAppCharP aParentID, cAppCharP  aBlobID,
                                              memSize  blkSize,
                       TDB_Api_Blk   &aBlk,   memSize &totSize, bool aFirst, bool &aLast );

    TSyError EndDataRead();



    // --- write section --------------------------------
    TSyError StartDataWrite();

    // <newItemID> will automatically be disposed at the beginning.
    // 2 overloaded versions
    TSyError InsertItem     ( cAppCharP  aItemData, cAppCharP  parentID, TDB_Api_ItemID  &newID );
    TSyError InsertItem     ( cAppCharP  aItemData,                      TDB_Api_Str &newItemID );

    TSyError InsertItemAsKey(       KeyH aItemKey,  cAppCharP  parentID, TDB_Api_ItemID  &newID );


    TSyError FinalizeLocalID( ItemID_Struct      aID,
                              TDB_Api_ItemID  &updID );
    TSyError FinalizeLocalID( TDB_Api_ItemID    &aID,
                              TDB_Api_ItemID  &updID );// <aID> obj will not be removed here

    TSyError FinalizeLocalID( cAppCharP      aItemID,
                              TDB_Api_Str &updItemID );
    TSyError FinalizeLocalID( cAppCharP      aItemID,
                              cAppCharP    aParentID,
                              TDB_Api_ItemID  &updID );


    // <updID> will automatically be disposed at the beginning.
    // 4 overloaded versions
    TSyError UpdateItem( cAppCharP aItemData, ItemID_Struct      aID,
                                              TDB_Api_ItemID  &updID );
    TSyError UpdateItem( cAppCharP aItemData, TDB_Api_ItemID    &aID,
                                              TDB_Api_ItemID  &updID ); // <aID> obj will not be removed here

    TSyError UpdateItem( cAppCharP aItemData, cAppCharP      aItemID,
                                              TDB_Api_Str &updItemID );
    TSyError UpdateItem( cAppCharP aItemData, cAppCharP      aItemID,
                                              cAppCharP    aParentID,
                                              TDB_Api_ItemID  &updID );

    TSyError UpdateItemAsKey( KeyH  aItemKey, ItemID_Struct      aID,
                                              TDB_Api_ItemID  &updID );
    TSyError UpdateItemAsKey( KeyH  aItemKey, TDB_Api_ItemID    &aID,
                                              TDB_Api_ItemID  &updID ); // <aID> obj will not be removed here

    // 2 overloaded versions
    TSyError MoveItem  ( ItemID_Struct   aID, cAppCharP newParID );
    TSyError MoveItem  ( cAppCharP   aItemID,
                         cAppCharP aParentID, cAppCharP newParID );

    // 3 overloaded versions
    TSyError DeleteItem( ItemID_Struct   aID );
    TSyError DeleteItem( cAppCharP   aItemID, cAppCharP parentID="" );
    TSyError DeleteItem( TDB_Api_ItemID &aID ); // <aID> obj will not be removed here

    TSyError DeleteSyncSet();


    // <totSize>, <aFirst> and <aLast> can be omitted, their default values are 0,true,true
    // 4 overloaded versions
    TSyError WriteBlob ( ItemID_Struct   aID, cAppCharP aBlobID,
                         appPointer   blkPtr,   memSize blkSize,
                                                memSize totSize= 0, bool aFirst= true, bool aLast= true );
    TSyError WriteBlob ( cAppCharP   aItemID, cAppCharP aBlobID,
                         appPointer   blkPtr,   memSize blkSize,
                                                memSize totSize= 0, bool aFirst= true, bool aLast= true );
    TSyError WriteBlob ( cAppCharP   aItemID,
                         cAppCharP aParentID, cAppCharP aBlobID,
                         appPointer   blkPtr,   memSize blkSize,
                                                memSize totSize= 0, bool aFirst= true, bool aLast= true );
    TSyError WriteBlob ( TDB_Api_ItemID &aID, cAppCharP aBlobID,   // <aID> obj will not be removed here
                         appPointer   blkPtr,   memSize blkSize,
                                                memSize totSize= 0, bool aFirst= true, bool aLast= true );

    // 4 overloaded versions
    TSyError DeleteBlob( ItemID_Struct   aID, cAppCharP aBlobID );
    TSyError DeleteBlob( cAppCharP   aItemID, cAppCharP aBlobID );
    TSyError DeleteBlob( cAppCharP   aItemID,
                         cAppCharP aParentID, cAppCharP aBlobID );
    TSyError DeleteBlob( TDB_Api_ItemID &aID, cAppCharP aBlobID ); // <aID> obj will not be removed here

    TSyError EndDataWrite( bool success, TDB_Api_Str &newToken );


    // --- close section --------------------------------
    TSyError DeleteContext();


    TDB_Api_Callback   fCB;      // Callback wrapper
    CContext           fContext; // local copy of the context

    void DisposeStr   ( TDB_Api_Str &s );
    void DisposeBlk   ( TDB_Api_Blk &b );

  private:
    void AssignStr    ( TDB_Api_Str &s ); // for internal use only
    void AssignBlk    ( TDB_Api_Blk &b );
    void AssignChanged( string      &a,        TDB_Api_Str   &u );
    void GetItemID    ( TDB_Api_ItemID &aID,   TDB_Api_Str   &aItem );
    void Assign_ItemID( TDB_Api_ItemID &newID, ItemID_Struct &aID, cAppCharP parentID );

    API_Methods*       dm;        // connection field reference
    API_Methods        fNo_dbapi; // default connection for call methods

    list<TDB_Api_Str*> fSList;    // list of currently allocated str objects of this datastore
    list<TDB_Api_Blk*> fBList;    //   "  "      "        "      blk    "    "    "      "

    TDB_Api_Config*    fConfig;   // assigned module
    bool               fCreated;  // true, as long <fContext> is valid (between 'CreateContext'
                                                                         // and 'DeleteContext')
    string             fDevKey;   // local copies
    string             fUsrKey;
}; // TDB_Api



} // namespace
#endif // DB_API_H
/* eof */
