/**
 *  @File     enginemodulebase.h
 *
 *  @Author   Beat Forster (bfo@synthesis.ch)
 *
 *  @brief TEngineModuleBase
 *    engine bus bar class
 *
 *    Copyright (c) 2007-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef ENGINEMODULEBASE_H
#define ENGINEMODULEBASE_H

#include "generic_types.h"
#include "sync_dbapidef.h"

// we need STL strings
#include <string>
using namespace std;



namespace sysync {

// Engine module base
class TEngineModuleBase
{
    SDK_Interface_Struct fCIBuffer; // used for fCI if the caller of Connect() doesn't set something

  public:
             TEngineModuleBase(); // constructor
    virtual ~TEngineModuleBase(); //  destructor

    UI_Call_In fCI;           // call in structure
    string     fEngineName;   // name of the SyncML engine to be connected
    CVersion   fPrgVersion;   // program's SDK version
    uInt16     fDebugFlags;   // debug flags to be used
    bool       fCIisStatic;   // this is kept purely for source code backwards compatibility:
                              // because fCI is *always* statically allocated

    TSyError Connect( string   aEngineName, // connect the SyncML engine
                      CVersion aPrgVersion= 0,
                      uInt16   aDebugFlags= DBG_PLUGIN_NONE );

    TSyError Disconnect();    // disconnect the SyncML engine

    virtual TSyError Init() = 0;
    virtual TSyError Term() = 0;


    // Engine init
    // -----------

    /// @brief Set the global mode for string paramaters (when never called, default params are UTF-8 with C-style line ends)
    /// @param aCharSet[in] charset
    /// @param aLineEndMode[in] line end mode (default is C-lineends of the platform (almost always LF))
    /// @param aBigEndian[in] determines endianness of UTF16 text (defaults to little endian = intel order)
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError SetStringMode ( uInt16 aCharSet,
                                     uInt16 aLineEndMode= LEM_CSTR,
                                     bool   aBigEndian  = false ) = 0;


    /// @brief init object, optionally passing XML config text in memory
    /// @param aConfigXML[in] NULL or empty string if no external config needed, config text otherwise
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError InitEngineXML  ( cAppCharP aConfigXML ) = 0;

    /// @brief init object, optionally passing a open FILE for reading config
    /// @param aConfigFilePath[in] path to config file
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError InitEngineFile ( cAppCharP aConfigFilePath ) = 0;

    /// @brief init object, optionally passing a callback for reading config
    /// @param aReaderFunc[in] callback function which can deliver next chunk of XML config data
    /// @param aContext[in] free context pointer passed back with callback
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError InitEngineCB   ( TXMLConfigReadFunc aReaderFunc, void *aContext ) = 0;


    // Running a Sync Session
    // ----------------------

    /// @brief Open a session
    /// @param aNewSessionH[out] receives session handle for all session execution calls
    /// @param aSelector[in] selector, depending on session type. For multi-profile clients: profile ID to use
    /// @param aSessionName[in] a text name/id to identify a session, useage depending on session type.
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError OpenSession( SessionH &aNewSessionH, uInt32 aSelector=0, cAppCharP aSessionName=NULL ) = 0;

    /// @brief open session specific runtime parameter/settings key
    /// @note key handle obtained with this call must be closed BEFORE SESSION IS CLOSED!
    /// @param aNewKeyH[out] receives the opened key's handle on success
    /// @param aSessionH[in] session handle obtained with OpenSession.
    ///                      When used as callback from DBApi, this parameter is irrelevant and
    ///                      must be set to NULL as a callback from DBApi has an implicit session context
    ///                      automatically.
    /// @param aMode[in] the open mode
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError OpenSessionKey ( SessionH aSessionH, KeyH &aNewKeyH, uInt16 aMode ) = 0;

    /// @brief Executes sync session or other sync related activity step by step
    /// @param aSessionH[in] session handle obtained with OpenSession
    /// @param aStepCmd[in/out] step command (STEPCMD_xxx):
    ///        - tells caller to send or receive data or end the session etc.
    ///        - instructs engine to suspend or abort the session etc.
    /// @param aInfoP[in] pointer to a TEngineProgressInfo structure, NULL if no progress info needed
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError SessionStep( SessionH aSessionH, uInt16 &aStepCmd,  TEngineProgressInfo *aInfoP = NULL ) = 0;

    /// @brief Get access to SyncML message buffer
    /// @param aSessionH[in] session handle obtained with OpenSession
    /// @param aForSend[in] direction send/receive
    /// @param aBuffer[out] receives pointer to buffer (empty for receive, full for send)
    /// @param aBufSize[out] receives size of empty or full buffer
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError GetSyncMLBuffer( SessionH aSessionH, bool aForSend, appPointer &aBuffer, memSize &aBufSize ) = 0;

    /// @brief Return SyncML message buffer to engine
    /// @param aSessionH[in] session handle obtained with OpenSession
    /// @param aForSend[in] direction send/receive
    /// @param aProcessed[in] number of bytes put into or read from the buffer
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError RetSyncMLBuffer( SessionH aSessionH, bool aForSend, memSize aProcessed ) = 0;

    /// @brief Read data from SyncML message buffer
    /// @param aSessionH[in] session handle obtained with OpenSession
    /// @param aBuffer[in] pointer to buffer
    /// @param aBufSize[in] size of buffer, maximum to be read
    /// @param aMsgSize[out] size of data available in the buffer for read INCLUDING just returned data.
    /// @note  If the aBufSize is too small to return all available data LOCERR_TRUNCATED will be returned, and the
    ///        caller can repeat calls to ReadSyncMLBuffer to get the next chunk.
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError ReadSyncMLBuffer ( SessionH aSessionH, appPointer aBuffer, memSize aBufSize, memSize &aMsgSize ) = 0;

    /// @brief Write data to SyncML message buffer
    /// @param aSessionH[in] session handle obtained with OpenSession
    /// @param aBuffer[in] pointer to buffer
    /// @param aMsgSize[in] size of message to write to the buffer
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError WriteSyncMLBuffer( SessionH aSessionH, appPointer aBuffer, memSize aMsgSize ) = 0;

    /// @brief Close a session
    /// @note  It depends on session type if this also destroys the session or if it may persist and can be re-opened.
    /// @param aSessionH[in] session handle obtained with OpenSession
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError CloseSession( SessionH aSessionH ) = 0;



    // Settings access
    // ---------------

    /// @brief open Settings key by path specification
    /// @param aNewKeyH[out] receives the opened key's handle on success
    /// @param aParentKeyH[in] NULL if path is absolute from root, handle to an open key for relative access
    /// @param aPath[in] the path specification as null terminated string
    /// @param aMode[in] the open mode
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError OpenKeyByPath ( KeyH &aNewKeyH,
                                     KeyH aParentKeyH,
                                     cAppCharP   aPath, uInt16 aMode ) = 0;


    /// @brief open Settings subkey key by ID or iterating over all subkeys
    /// @param aNewKeyH[out] receives the opened key's handle on success
    /// @param aParentKeyH[in] handle to the parent key
    /// @param aID[in] the ID of the subkey to open,
    ///                or KEYVAL_ID_FIRST/KEYVAL_ID_NEXT to iterate over existing subkeys
    ///                or KEYVAL_ID_NEW to create a new subkey
    /// @param aMode[in] the open mode
    /// @return LOCERR_OK on success, DB_NoContent when no more subkeys are found with
    ///         KEYVAL_ID_FIRST/KEYVAL_ID_NEXT
    ///         or any other SyncML or LOCERR_xxx error code on failure
    virtual TSyError OpenSubkey    ( KeyH &aNewKeyH,
                                     KeyH aParentKeyH,
                                     sInt32 aID, uInt16 aMode ) = 0;


    /// @brief delete Settings subkey key by ID
    /// @param aParentKeyH[in] handle to the parent key
    /// @param aID[in] the ID of the subkey to delete
    /// @return LOCERR_OK on success
    ///         or any other SyncML or LOCERR_xxx error code on failure
    virtual TSyError DeleteSubkey  ( KeyH aParentKeyH, sInt32 aID ) = 0;

    /// @brief Get key ID of currently open key. Note that the Key ID is only locally unique within
    ///        the parent key.
    /// @param aKeyH[in] an open key handle
    /// @param aID[out] receives the ID of the open key, which can be used to re-access the
    ///        key within its parent using OpenSubkey()
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError GetKeyID      ( KeyH aKeyH, sInt32 &aID ) = 0;


    /// @brief Set text format parameters (when never called, default params are those set with global SetStringMode())
    /// @param aKeyH[in] an open key handle
    /// @param aCharSet[in] charset
    /// @param aLineEndMode[in] line end mode (defaults to C-lineends of the platform (almost always LF))
    /// @param aBigEndian[in] determines endianness of UTF16 text (defaults to little endian = intel order)
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError SetTextMode   ( KeyH aKeyH, uInt16 aCharSet,
                                     uInt16 aLineEndMode= LEM_CSTR,
                                     bool   aBigEndian  = false ) = 0;

    /// @brief Set time format parameters
    /// @param aKeyH[in] an open key handle
    /// @param aTimeMode[in] time mode, see TMODE_xxx (default is platform's lineratime_t when SetTimeMode() is not used)
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError SetTimeMode   ( KeyH aKeyH, uInt16 aTimeMode ) = 0;


    /// @brief Closes a key opened by OpenKeyByPath() or OpenSubKey()
    /// @param aKeyH[in] an open key handle. Will be invalid when call returns with LOCERR_OK. Do not re-use!
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError CloseKey      ( KeyH aKeyH ) = 0;


    /// @brief Reads a named value in specified format into passed memory buffer
    /// @param aKeyH[in] an open key handle
    /// @param aValueName[in] name of the value to read. Some keys offer special ".XXX"
    ///        suffixes to value names, which return alternate values (like a timestamp field's
    ///        time zone name with ".TZNAME").
    /// @param aValType[in] desired return type, see VALTYPE_xxxx
    /// @param aBuffer[in/out] buffer where to store the data
    /// @param aBufSize[in] size of buffer in bytes (ALWAYS in bytes, even if value is Unicode string)
    ///        Note: to get only size of a value (useful especially for strings), pass 0 as buffer size.
    /// @param aValSize[out] actual size of value.
    ///        For VALTYPE_TEXT, size is string length (IN BYTES) excluding NULL terminator
    ///        Note that this will be set also when return value is LOCERR_BUFTOOSMALL,
    ///        to indicate the required buffer size
    ///        For values that can be truncated (strings), LOCERR_TRUNCATED will be returned
    ///        when returned value is not entire value - aValSize is truncated size then.
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError GetValue      ( KeyH aKeyH, cAppCharP aValName,  uInt16  aValType,
                                     appPointer aBuffer, memSize aBufSize, memSize &aValSize ) = 0;

    /// @brief get value's ID for use with Get/SetValueByID()
    /// @param aKeyH[in] an open key handle
    /// @param aName[in] name of the value to write. Some keys offer special ".FLAG.XXX"
    ///        values, which return flag bits which can be added to the regular ID to obtain
    ///        alternate values (like the value name with ".FLAG.VALNAME", useful when iterating
    ///        over values).
    /// @return KEYVAL_ID_UNKNOWN when no ID available for name, ID of value otherwise
    virtual sInt32 GetValueID      ( KeyH aKeyH, cAppCharP aName ) = 0;


    /// @brief Reads a named value in specified format into passed memory buffer
    /// @param aKeyH[in] an open key handle
    /// @param aID[in] ID of the value to read
    /// @param arrIndex[in] 0-based array element index for array values.
    /// @param aValType[in] desired return type, see VALTYPE_xxxx
    /// @param aBuffer[in/out] buffer where to store the data
    /// @param aBufSize[in] size of buffer in bytes (ALWAYS in bytes, even if value is Unicode string)
    ///        Note: to get only size of a value (useful especially for strings), pass 0 as buffer size.
    /// @param aValSize[out] actual size of value.
    ///        For VALTYPE_TEXT, size is string length (IN BYTES) excluding NULL terminator
    ///        Note that this will be set also when return value is LOCERR_BUFTOOSMALL,
    ///        to indicate the required buffer size
    ///        For values that can be truncated (strings), LOCERR_TRUNCATED will be returned
    ///        when returned value is not entire value - aValSize is truncated size then.
    /// @return LOCERR_OK on success, LOCERR_OUTOFRANGE when array index is out of range
    ///        SyncML or LOCERR_xxx error code on other failure
    virtual TSyError GetValueByID  ( KeyH aKeyH,    sInt32 aID,       sInt32  arrIndex,
                                                                            uInt16  aValType,
                                     appPointer aBuffer, memSize aBufSize, memSize &aValSize ) = 0;


    /// @brief Writes a named value in specified format passed in memory buffer
    /// @param aKeyH[in] an open key handle
    /// @param aValueName[in] name of the value to write. Some keys offer special ".XXX"
    ///        suffixes to value names, which are used to set alternate values (like a
    ///        timestamp field's time zone name with ".TZNAME").
    /// @param aValType[in] type of value passed in, see VALTYPE_xxxx
    /// @param aBuffer[in] buffer containing the data
    /// @param aValSize[in] size of value. For VALTYPE_TEXT, size can be passed as -1 if string is null terminated
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure. If value
    ///         buffer passed in is too small for aValType (such as only 2 bytes for a VALTYPE_INT32,
    ///         LOCERR_BUFTOOSMALL will be returned and nothing stored. If buffer passed in is too
    ///         long (e.g. for strings) to be entirely stored, only the beginning is stored and
    ///         LOCERR_TRUNCATED is returned.
    virtual TSyError SetValue      ( KeyH aKeyH, cAppCharP aValName, uInt16 aValType,
                                     cAppPointer aBuffer, memSize aValSize ) = 0;


    /// @brief Writes a named value in specified format passed in memory buffer
    /// @param aKeyH[in] an open key handle
    /// @param aID[in] ID of the value to read
    /// @param arrIndex[in] 0-based array element index for array values.
    /// @param aValType[in] type of value passed in, see VALTYPE_xxxx
    /// @param aBuffer[in] buffer containing the data
    /// @param aValSize[in] size of value. For VALTYPE_TEXT, size can be passed as -1 if string is null terminated
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure. If value
    ///         buffer passed in is too small for aValType (such as only 2 bytes for a VALTYPE_INT32,
    ///         LOCERR_BUFTOOSMALL will be returned and nothing stored. If buffer passed in is too
    ///         long (e.g. for strings) to be entirely stored, only the beginning is stored and
    ///         LOCERR_TRUNCATED is returned.
    virtual TSyError SetValueByID  ( KeyH aKeyH,    sInt32 aID, sInt32 arrIndex,
                                     uInt16 aValType,
                                     cAppPointer aBuffer, memSize aValSize ) = 0;


    // Convenience routines to close and NULL a handle in one step
    // -----------------------------------------------------------

    /// @brief Closes a key and nulls the handle
    /// @param aKeyH[in/out] an open key handle. Will be set to NULL on exit (to make sure it is not re-used)
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    TSyError CloseKeyAndNULL       ( KeyH &aKeyH )
    {
      TSyError sta = CloseKey(aKeyH);
      if (sta==LOCERR_OK) aKeyH=NULL;
      return   sta;
    };

    /// @brief Closes a session and nulls the handle
    /// @param aSessionH[in] session handle obtained with OpenSession
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    TSyError CloseSessionAndNULL   ( SessionH &aSessionH )
    {
      TSyError sta = CloseSession(aSessionH);
      if (sta==LOCERR_OK) aSessionH=NULL;
      return   sta;
    };


    // Convenience routines to access Get/SetValue for specific types
    // --------------------------------------------------------------
    virtual TSyError GetStrValue      ( KeyH aKeyH, cAppCharP aValName, string  &aText );
    virtual TSyError SetStrValue      ( KeyH aKeyH, cAppCharP aValName, string   aText );

    virtual TSyError GetInt8Value     ( KeyH aKeyH, cAppCharP aValName, sInt8  &aValue ); // signed
    virtual TSyError GetInt8Value     ( KeyH aKeyH, cAppCharP aValName, uInt8  &aValue ); // unsigned
    virtual TSyError SetInt8Value     ( KeyH aKeyH, cAppCharP aValName, uInt8   aValue );

    virtual TSyError GetInt16Value    ( KeyH aKeyH, cAppCharP aValName, sInt16 &aValue ); // signed
    virtual TSyError GetInt16Value    ( KeyH aKeyH, cAppCharP aValName, uInt16 &aValue ); // unsigned
    virtual TSyError SetInt16Value    ( KeyH aKeyH, cAppCharP aValName, uInt16  aValue );

    virtual TSyError GetInt32Value    ( KeyH aKeyH, cAppCharP aValName, sInt32 &aValue ); // signed
    virtual TSyError GetInt32Value    ( KeyH aKeyH, cAppCharP aValName, uInt32 &aValue ); // unsigned
    virtual TSyError SetInt32Value    ( KeyH aKeyH, cAppCharP aValName, uInt32  aValue );

    virtual void     AppendSuffixToID ( KeyH aKeyH, sInt32   &aID,   cAppCharP aSuffix );



    // Tunnel Interface Methods ---------------------------------------------------------------------
    virtual TSyError StartDataRead    ( SessionH aSessionH,  cAppCharP    lastToken,
                                                             cAppCharP  resumeToken )                = 0;
    virtual TSyError ReadNextItem     ( SessionH aSessionH,     ItemID  aID,  appCharP *aItemData,
                                                                sInt32 *aStatus,  bool  aFirst )     = 0;
    virtual TSyError ReadItem         ( SessionH aSessionH,    cItemID  aID,  appCharP *aItemData )  = 0;
    virtual TSyError EndDataRead      ( SessionH aSessionH ) = 0;
    virtual TSyError StartDataWrite   ( SessionH aSessionH ) = 0;
    virtual TSyError InsertItem       ( SessionH aSessionH,  cAppCharP  aItemData,     ItemID   aID )= 0;
    virtual TSyError UpdateItem       ( SessionH aSessionH,  cAppCharP  aItemData,    cItemID   aID,
                                                                                       ItemID updID )= 0;

    virtual TSyError MoveItem         ( SessionH aSessionH,    cItemID  aID,    cAppCharP  newParID )= 0;
    virtual TSyError DeleteItem       ( SessionH aSessionH,    cItemID  aID )                        = 0;
    virtual TSyError EndDataWrite     ( SessionH aSessionH,       bool  success, appCharP *newToken )= 0;
    virtual void     DisposeObj       ( SessionH aSessionH,      void*  memory )                     = 0;

    // -- asKey --
    virtual TSyError ReadNextItemAsKey( SessionH aSessionH,     ItemID  aID,          KeyH aItemKey,
                                                                sInt32 *aStatus,      bool aFirst   )= 0;
    virtual TSyError ReadItemAsKey    ( SessionH aSessionH,    cItemID  aID,          KeyH aItemKey )= 0;

    virtual TSyError InsertItemAsKey  ( SessionH aSessionH,       KeyH  aItemKey,      ItemID   aID )= 0;
    virtual TSyError UpdateItemAsKey  ( SessionH aSessionH,       KeyH  aItemKey,     cItemID   aID,
                                                                                       ItemID updID )= 0;
    virtual TSyError debugPuts(cAppCharP aFile, int aLine, cAppCharP aFunction,
                               int aDbgLevel, cAppCharP aLinePrefix,
                               cAppCharP aText) { return LOCERR_NOTIMP; }
}; // TEngineModuleBase

// debug level masks (original definition in sysync_debug.h)
#define DBG_HOT        0x00000001    // hot information
#define DBG_ERROR      0x00000002    // Error conditions

/**
 * @param aFile        source file name from which log entry comes
 * @param aLine        source file line
 * @param aFunction    function name
 * @param aDbgLevel    same bit mask as in the internal TDebugLogger;
 *                     currently DBG_HOT and DBG_ERROR are defined publicly
 * @param aLinePrefix  a short string to be displayed in front of each line;
 *                     the advantage of passing this separately instead of
 *                     making it a part of aText is that the logger might
 *                     be able to insert the prefix more efficiently and/or
 *                     (depending on the log format) with extra formatting
 * @param aText        the text to be printed, may consist of multiple lines;
 *                     the log always starts a new line after the text, regardless
 *                     of how many newlines might be at the end of the text
 */
void SySyncDebugPuts(void* aCB,
                     cAppCharP aFile, int aLine, cAppCharP aFunction,
                     int aDbgLevel, cAppCharP aLinePrefix,
                     cAppCharP aText);


// factory function declarations - must be implemented in the source file of the leaf derivates of TEngineInterface
#ifdef SYSYNC_CLIENT
TEngineModuleBase *newClientEngine(void);
#endif
#ifdef SYSYNC_SERVER
TEngineModuleBase *newServerEngine(void);
#endif

} // namespace sysync
#endif // ENGINEMODULEBASE_H


// eof
