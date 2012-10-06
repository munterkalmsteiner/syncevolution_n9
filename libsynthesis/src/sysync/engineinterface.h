/**
 *  @File     engineinterface.h
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TEngineInterface - common interface to SySync engine for SDK
 *
 *    Copyright (c) 2007-2011 by Synthesis AG + plan44.ch
 *
 */

/*
 */

#ifndef ENGINEINTERFACE_H
#define ENGINEINTERFACE_H

#ifdef ENGINEINTERFACE_SUPPORT

// published engine definitions
#include "engine_defs.h"

// internal sysync engine definitions
#include "sysync.h"
#include "syserial.h"

// decide if using with EngineModuleBase class
#ifdef SIMPLE_LINKING
  #define EMBVIRTUAL
#else
  #include "enginemodulebase.h"
  #define EMBVIRTUAL virtual
#endif

// DBAPI tunnel support
#ifdef DBAPI_TUNNEL_SUPPORT
  #define TUNNEL_IMPL
  #define TUNNEL_IMPL_VOID
#else
  #define TUNNEL_IMPL { return LOCERR_NOTIMP; }
  #define TUNNEL_IMPL_VOID {}
#endif



using namespace std;

namespace sysync {

// forward
class TEngineInterface;
class TSyncAppBase;



#ifdef SIMPLE_LINKING
  // for using the engineInterface without EngineModuleBase, engine base class
  // for the newXXXXEngine() functions is TEngineInterface
  #define ENGINE_IF_CLASS TEngineInterface
  // factory function declarations are here, as we have no EngineModuleBase
  #ifdef SYSYNC_CLIENT
  ENGINE_IF_CLASS *newClientEngine(void);
  #endif
  #ifdef SYSYNC_SERVER
  ENGINE_IF_CLASS *newServerEngine(void);
  #endif
#else
  // with EngineModuleBase, use it as base class. newXXXXXEngine are declared in
  // enginemodulebase.h
  #define ENGINE_IF_CLASS TEngineModuleBase
#endif


// settings key implementation base class
class TSettingsKeyImpl
{
public:
  TSettingsKeyImpl(TEngineInterface *aEngineInterfaceP);
  virtual ~TSettingsKeyImpl();

  // open subkey(chain) by relative(!) path
  // - walks down through needed subkeys, always starting at myself
  TSyError OpenKeyByPath(
    TSettingsKeyImpl *&aSettingsKeyP,
    cAppCharP aPath, uInt16 aMode,
    bool aImplicit
  );

  // open Settings subkey key by ID or iterating over all subkeys
  virtual TSyError OpenSubkey(
    TSettingsKeyImpl *&aSettingsKeyP,
    sInt32 aID, uInt16 aMode
  ) {
    // by default, opening by key is not allowed (only if derived class implements it)
    return DB_NotAllowed;
  };

  // delete Settings subkey key by ID
  virtual TSyError DeleteSubkey(sInt32 aID)
  {
    // by default, keys cannot be deleted
    return DB_NotAllowed;
  };

  // return ID of current key
  virtual TSyError GetKeyID(sInt32 &aID) {
    // by default, key has no ID access to subkeys
    return DB_NotAllowed;
  };

  // get value's ID (e.g. internal index)
  virtual sInt32 GetValueID(cAppCharP aName) {
    return KEYVAL_ID_UNKNOWN; // unknown
  };

  // Set text format parameters
  TSyError SetTextMode(uInt16 aCharSet, uInt16 aLineEndMode, bool aBigEndian);

  // Set text format parameters
  TSyError SetTimeMode(uInt16 aTimeMode) {
    fTimeMode = aTimeMode;
    return LOCERR_OK;
  };

  // Reads a named value in specified format into passed memory buffer
  TSyError GetValueByID(
    sInt32 aID, sInt32 aArrayIndex, uInt16 aValType,
    appPointer aBuffer, memSize aBufSize, memSize &aValSize
  );

  // Writes a named value in specified format passed in memory buffer
  TSyError SetValueByID(
    sInt32 aID, sInt32 aArrayIndex, uInt16 aValType,
    cAppPointer aBuffer, memSize aValSize
  );

  // get engine interface
  TEngineInterface *getEngineInterface(void) { return fEngineInterfaceP; };

protected:
  // open subkey by name (not by path!)
  // - this is the actual implementation
  virtual TSyError OpenSubKeyByName(
    TSettingsKeyImpl *&aSettingsKeyP,
    cAppCharP aName, stringSize aNameSize,
    uInt16 aMode
  ) {
    // by default, no key can be found (derived class must implement it)
    return DB_NotFound;
  };

  // get value's native type
  // Note: this must be safe if aID is out of range and must return VALTYPE_UNKNOWN if so.
  virtual uInt16 GetValueType(sInt32 aID) {
    return VALTYPE_UNKNOWN; // unknown
  };

  // Internal Get Value in internal format.
  // Notes:
  // - all values, even strings can be returned as bare data (no NUL terminator needed, but allowed for VALTYPE_TEXT)
  // - in all cases, no more than aBufSize bytes may be copied (and in case of strings, if aBufSize equals the string
  //   size, the routine must return the entire string data, even if this means omitting the terminator)
  // - in all cases, aValSize must return the actual size of the entire value, regardless of how many bytes could
  //   actually be copied. Note that this is UNLIKE aValSize for the exposed API GetValue(), which always returns
  //   the number of bytes returned, even if these are truncated.
  virtual TSyError GetValueInternal(
    sInt32 aID, sInt32 aArrayIndex,
    appPointer aBuffer, memSize aBufSize, memSize &aValSize
  ) {
    // By default, there are no values
    return DB_NotAllowed;
  };

  // set value
  virtual TSyError SetValueInternal(
    sInt32 aID, sInt32 aArrayIndex,
    cAppPointer aBuffer, memSize aValSize
  ) {
    // By default, there are no values
    return DB_NotAllowed;
  };


  // helper for detecting generic field attribute access
  bool checkFieldAttrs(cAppCharP aName, size_t &aBaseNameSize, sInt32 &aFldID);
  // helper for returning generic field attribute type
  bool checkAttrValueType(sInt32 aID, uInt16 &aValType);
  // helper for returning generic field attribute values
  bool checkAttrValue(
    sInt32 aID, sInt32 aArrayIndex,
    appPointer aBuffer, memSize aBufSize, memSize &aValSize
  );

  // the engine interface
  TEngineInterface *fEngineInterfaceP;
  // Value format modes
  TCharSets fCharSet;
  bool fBigEndian;
  TLineEndModes fLineEndMode;
  uInt16 fTimeMode;

private:
  // link to implicitly opened parent (when opening via OpenKeyByPath)
  TSettingsKeyImpl *fImplicitParentKeyP;

}; // TSettingsKeyImpl


// Table for TReadOnlyInfoKey
typedef struct {
  cAppCharP valName; // null for terminator
  uInt16 valType;
  appPointer valPtr;
  memSize valSiz; // size of var or 0 for autosize for null terminated strings
} TReadOnlyInfo;


// key delivering constant infos
class TReadOnlyInfoKey :
  public TSettingsKeyImpl
{
  typedef TSettingsKeyImpl inherited;
public:
  TReadOnlyInfoKey(TEngineInterface *aEngineInterfaceP) :
    inherited(aEngineInterfaceP) {};

  // get value's ID (e.g. internal index)
  virtual sInt32 GetValueID(cAppCharP aName);

protected:

  // get value's native type
  virtual uInt16 GetValueType(sInt32 aID);

  // get value
  virtual TSyError GetValueInternal(
    sInt32 aID, sInt32 aArrayIndex,
    appPointer aBuffer, memSize aBufSize, memSize &aValSize
  );

  virtual const TReadOnlyInfo *getInfoTable(void) = 0;
  virtual sInt32 numInfos(void) = 0;

private:
  // iterator
  uInt16 fIterator;
}; // TReadOnlyInfoKey



// key for accessing global config vars
class TConfigVarKey :
  public TSettingsKeyImpl
{
  typedef TSettingsKeyImpl inherited;
public:
  TConfigVarKey(TEngineInterface *aEngineInterfaceP) :
    inherited(aEngineInterfaceP) {};

  // get value's ID (config vars do NOT have an ID!)
  virtual sInt32 GetValueID(cAppCharP aName);

protected:

  // get value's native type (all config vars are text)
  virtual uInt16 GetValueType(sInt32 aID);

  // get value
  virtual TSyError GetValueInternal(
    sInt32 aID, sInt32 aArrayIndex,
    appPointer aBuffer, memSize aBufSize, memSize &aValSize
  );

  // set value
  virtual TSyError SetValueInternal(
    sInt32 aID, sInt32 aArrayIndex,
    cAppPointer aBuffer, memSize aValSize
  );

private:
  // cached name of last variable name used (to get around IDs)
  string fVarName;

}; // TConfigVarKey



// forward
class TStructFieldsKey;
typedef struct TStructFieldInfo TStructFieldInfo;


// function prototype to get values
typedef TSyError (*TGetValueProc)(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
);

// function prototype to set values
typedef TSyError (*TSetValueProc)(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
);


// Table for TStructFieldsKey
typedef struct TStructFieldInfo {
  cAppCharP valName; // null for terminator
  uInt16 valType;
  bool writable; // set if field can be written
  size_t fieldOffs; // offset of field from beginning of structure
  size_t valSiz; // size of field in bytes, if 0, only get/setValueProc access is possible (and if these are not defined, access is not allowed)
  TGetValueProc getValueProc; // if not NULL, this routine is called instead of reading data directly
  TSetValueProc setValueProc; // if not NULL, this routine is called instead of writing data directly
} TStructFieldInfo;


// key for access to a struct containing settings fields
class TStructFieldsKey :
  public TSettingsKeyImpl
{
  typedef TSettingsKeyImpl inherited;
public:
  TStructFieldsKey(TEngineInterface *aEngineInterfaceP) :
    inherited(aEngineInterfaceP),
    fDirty(false),
    fIterator(0)
    {};

  // get value's ID (e.g. internal index)
  virtual sInt32 GetValueID(cAppCharP aName);

  // Static helpers
  // - static helper for procedural string readers
  static TSyError returnString(cAppCharP aReturnString, appPointer aBuffer, memSize aBufSize, memSize &aValSize);
  static TSyError returnInt(sInt32 aInt, memSize aIntSize, appPointer aBuffer, memSize aBufSize, memSize &aValSize);
  static TSyError returnLineartime(lineartime_t aTime, appPointer aBuffer, memSize aBufSize, memSize &aValSize);

protected:

  // get value's native type
  virtual uInt16 GetValueType(sInt32 aID);

  // get value
  virtual TSyError GetValueInternal(
    sInt32 aID, sInt32 aArrayIndex,
    appPointer aBuffer, memSize aBufSize, memSize &aValSize
  );

  // set value
  virtual TSyError SetValueInternal(
    sInt32 aID, sInt32 aArrayIndex,
    cAppPointer aBuffer, memSize aValSize
  );

  // get table describing the fields in the struct
  virtual const TStructFieldInfo *getFieldsTable(void) { return NULL; };
  virtual sInt32 numFields(void) { return 0; };

  // get actual struct base address
  virtual uInt8P getStructAddr(void) { return NULL; };

  // flag that will be set on first write access
  bool fDirty;

private:
  // iterator
  uInt16 fIterator;

}; // TStructFieldsKey



// Engine info

static const uInt16 variantCode = SYSER_VARIANT_CODE;

static const TReadOnlyInfo EngineInfoTable[] = {
  // name, type, ptr, siz
  { "version", VALTYPE_TEXT, (appPointer)SYSYNC_FULL_VERSION_STRING, 0 },
  { "platform", VALTYPE_TEXT, (appPointer)SYSYNC_PLATFORM_NAME, 0 },
  { "name", VALTYPE_TEXT, (appPointer)CUST_SYNC_MODEL, 0 },
  { "manufacturer", VALTYPE_TEXT, (appPointer)CUST_SYNC_MAN, 0 },
  { "comment", VALTYPE_TEXT, (appPointer)VERSION_COMMENTS, 0 },
  // other build infos
  { "variantcode", VALTYPE_INT16, (appPointer)&variantCode, 2 },
  // Note: productcode and extraid are already available as configvars
};

class TEngineInfoKey :
  public TReadOnlyInfoKey
{
  typedef TReadOnlyInfoKey inherited;
public:
  TEngineInfoKey(TEngineInterface *aEngineInterfaceP) :
    inherited(aEngineInterfaceP) {};

protected:
  virtual const TReadOnlyInfo *getInfoTable(void) { return EngineInfoTable; };
  virtual sInt32 numInfos(void) { return sizeof(EngineInfoTable)/sizeof(TReadOnlyInfo); };

}; // TEngineInfoKey


// Licensing

#ifdef SYSER_REGISTRATION

class TLicensingKey :
  public TStructFieldsKey
{
  typedef TStructFieldsKey inherited;
public:
  TLicensingKey(TEngineInterface *aEngineInterfaceP) :
    inherited(aEngineInterfaceP) {};

protected:
  // get table describing the fields in the struct
  virtual const TStructFieldInfo *getFieldsTable(void);
  virtual sInt32 numFields(void);
  // get actual struct base address
  virtual uInt8P getStructAddr(void);

}; // TLicensingKey

#endif


// Generic root key, providing settings keys common to all
// SySync applications
class TSettingsRootKey :
  public TSettingsKeyImpl
{
  typedef TSettingsKeyImpl inherited;

public:
  TSettingsRootKey(TEngineInterface *aEngineInterfaceP) :
    inherited(aEngineInterfaceP) {};

protected:
  // open subkey by name (not by path!)
  // - this is the actual implementation
  virtual TSyError OpenSubKeyByName(
    TSettingsKeyImpl *&aSettingsKeyP,
    cAppCharP aName, stringSize aNameSize,
    uInt16 aMode
  ) {
    if (strucmp(aName,"engineinfo",aNameSize)==0)
      aSettingsKeyP = new TEngineInfoKey(fEngineInterfaceP);
    else if (strucmp(aName,"configvars",aNameSize)==0)
      aSettingsKeyP = new TConfigVarKey(fEngineInterfaceP);
    #ifdef SYSER_REGISTRATION
    else if (strucmp(aName,"licensing",aNameSize)==0)
      aSettingsKeyP = new TLicensingKey(fEngineInterfaceP);
    #endif
    else
      return inherited::OpenSubKeyByName(aSettingsKeyP,aName,aNameSize,aMode);
    // opened a key
    return LOCERR_OK;
  };
}; // TSettingsRootKey



// Common Engine Interface
class TEngineInterface
#ifndef SIMPLE_LINKING
  : public TEngineModuleBase
#endif
{
  #ifndef SIMPLE_LINKING
  typedef TEngineModuleBase inherited;
  #endif
  friend class TSettingsKeyImpl;

  public:
    TEngineInterface(); // constructor
    virtual ~TEngineInterface(); //  destructor

    /// @brief Object init, usually called at connect time
    /// @note needed to allow virtual call to newSyncAppBase() factory function
    EMBVIRTUAL TSyError Init(void);
    EMBVIRTUAL TSyError Term(void);

    // Engine init
    // -----------

    /// @brief Set the global mode for string paramaters (when never called, default params are UTF-8 with C-style line ends)
    /// @param aCharSet[in] charset
    /// @param aLineEndMode[in] line end mode (default is C-lineends of the platform (almost always LF))
    /// @param aBigEndian[in] determines endianness of UTF16 text (defaults to little endian = intel order)
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError SetStringMode(uInt16 aCharSet, uInt16 aLineEndMode=LEM_CSTR, bool aBigEndian=false);

    /// @brief init object, optionally passing XML config text in memory
    /// @param aConfigXML[in] NULL or empty string if no external config needed, config text otherwise
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError InitEngineXML(cAppCharP aConfigXML);

    /// @brief init object, optionally passing a open FILE for reading config
    /// @param aConfigFilePath[in] path to config file
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError InitEngineFile(cAppCharP aConfigFilePath);

    /// @brief init object, optionally passing a callback for reading config
    /// @param aReaderFunc[in] callback function which can deliver next chunk of XML config data
    /// @param aContext[in] free context pointer passed back with callback
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError InitEngineCB(TXMLConfigReadFunc aReaderFunc, void *aContext);


    // Running a Sync Session
    // ----------------------

    /// @brief Open a session
    /// @param aNewSessionH[out] receives session handle for all session execution calls
    /// @param aSelector[in] selector, depending on session type. For multi-profile clients: profile ID to use
    /// @param aSessionName[in] a text name/id to identify a session, useage depending on session type.
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError OpenSession(SessionH &aNewSessionH, uInt32 aSelector=0, cAppCharP aSessionName=NULL);
protected:
    // internal implementation, sessionname already in application format
    virtual TSyError OpenSessionInternal(SessionH &aNewSessionH, uInt32 aSelector, cAppCharP aSessionName);
public:

    /// @brief open session specific runtime parameter/settings key
    /// @note key handle obtained with this call must be closed BEFORE SESSION IS CLOSED!
    /// @param aNewKeyH[out] receives the opened key's handle on success
    /// @param aSessionH[in] session handle obtained with OpenSession.
    ///                      When used as callback from DBApi, this parameter is irrelevant and
    ///                      must be set to NULL as a callback from DBApi has an implicit session context
    ///                      automatically.
    /// @param aMode[in] the open mode
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError OpenSessionKey(SessionH aSessionH, KeyH &aNewKeyH, uInt16 aMode);

    /// @brief Close a session
    /// @note  It depends on session type if this also destroys the session or if it may persist and can be re-opened.
    /// @param aSessionH[in] session handle obtained with OpenSession
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError CloseSession(SessionH aSessionH);

    /// @brief Executes sync session or other sync related activity step by step
    /// @param aSessionH[in] session handle obtained with OpenSession
    /// @param aStepCmd[in/out] step command (STEPCMD_xxx):
    ///        - tells caller to send or receive data or end the session etc.
    ///        - instructs engine to suspend or abort the session etc.
    /// @param aInfoP[in] pointer to a TEngineProgressInfo structure, NULL if no progress info needed
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    virtual TSyError SessionStep(SessionH aSessionH, uInt16 &aStepCmd,  TEngineProgressInfo *aInfoP = NULL);

    /// @brief Get access to SyncML message buffer
    /// @param aSessionH[in] session handle obtained with OpenSession
    /// @param aBuffer[out] receives pointer to buffer (empty for receive, full for send)
    /// @param aBufSize[out] receives size of empty or full buffer
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError GetSyncMLBuffer(SessionH aSessionH, bool aForSend, appPointer &aBuffer, memSize &aBufSize);

    /// @brief Return SyncML message buffer to engine
    /// @param aSessionH[in] session handle obtained with OpenSession
    /// @param aProcessed[in] number of bytes put into or read from the buffer
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError RetSyncMLBuffer(SessionH aSessionH, bool aForSend, memSize aProcessed);

    /// @brief Read data from SyncML message buffer
    /// @param aSessionH[in] session handle obtained with OpenSession
    /// @param aBuffer[in] pointer to buffer
    /// @param aBufSize[in] size of buffer, maximum to be read
    /// @param aMsgSize[out] size of data available in the buffer for read INCLUDING just returned data.
    /// @note  If the aBufSize is too small to return all available data LOCERR_TRUNCATED will be returned, and the
    ///        caller can repeat calls to ReadSyncMLBuffer to get the next chunk.
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError ReadSyncMLBuffer (SessionH aSessionH, appPointer aBuffer, memSize aBufSize, memSize &aMsgSize);

    /// @brief Write data to SyncML message buffer
    /// @param aSessionH[in] session handle obtained with OpenSession
    /// @param aBuffer[in] pointer to buffer
    /// @param aMsgSize[in] size of message to write to the buffer
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError WriteSyncMLBuffer(SessionH aSessionH, appPointer aBuffer, memSize aMsgSize);


    // Settings access
    // ---------------

    /// @brief open Settings key by path specification
    /// @param aNewKeyH[out] receives the opened key's handle on success
    /// @param aParentKeyH[in] NULL if path is absolute from root, handle to an open key for relative access
    /// @param aPath[in] the path specification as null terminated string
    /// @param aMode[in] the open mode
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError OpenKeyByPath (
      KeyH &aNewKeyH,
      KeyH aParentKeyH,
      cAppCharP aPath, uInt16 aMode
    );


    /// @brief open Settings subkey key by ID or iterating over all subkeys
    /// @param aNewKeyH[out] receives the opened key's handle on success
    /// @param aParentKeyH[in] handle to the parent key (NULL = root key)
    /// @param aID[in] the ID of the subkey to open,
    ///                or KEYVAL_ID_FIRST/KEYVAL_ID_NEXT to iterate over existing subkeys, returns DB_NoContent when no more found
    ///                or KEYVAL_ID_NEW(_xxx) to create a new subkey
    /// @param aMode[in] the open mode
    /// @return LOCERR_OK on success, DB_NoContent when no more subkeys are found with
    ///         KEYVAL_ID_FIRST/KEYVAL_ID_NEXT
    ///         or any other SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError OpenSubkey(
      KeyH &aNewKeyH,
      KeyH aParentKeyH,
      sInt32 aID, uInt16 aMode
    );


    /// @brief delete Settings subkey key by ID
    /// @param aParentKeyH[in] handle to the parent key
    /// @param aID[in] the ID of the subkey to delete
    /// @return LOCERR_OK on success
    ///         or any other SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError DeleteSubkey(KeyH aParentKeyH, sInt32 aID);

    /// @brief Get key ID of currently open key. Note that the Key ID is only locally unique within
    ///        the parent key.
    /// @param aKeyH[in] an open key handle
    /// @param aID[out] receives the ID of the open key, which can be used to re-access the
    ///        key within its parent using OpenSubkey()
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError GetKeyID(KeyH aKeyH, sInt32 &aID);


    /// @brief Set text format parameters (when never called, default params are those set with global SetStringMode())
    /// @param aKeyH[in] an open key handle
    /// @param aCharSet[in] charset
    /// @param aLineEndMode[in] line end mode (defaults to C-lineends of the platform (almost always LF))
    /// @param aBigEndian[in] determines endianness of UTF16 text (defaults to little endian = intel order)
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError SetTextMode(KeyH aKeyH, uInt16 aCharSet, uInt16 aLineEndMode=LEM_CSTR, bool aBigEndian=false);

    /// @brief Set time format parameters
    /// @param aKeyH[in] an open key handle
    /// @param aTimeMode[in] time mode, see TMODE_xxx (default is platform's lineratime_t when SetTimeMode() is not used)
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError SetTimeMode(KeyH aKeyH, uInt16 aTimeMode);


    /// @brief Closes a key opened by OpenKeyByPath() or OpenSubKey()
    /// @param aKeyH[in] an open key handle. Will be invalid when call returns with LOCERR_OK. Do not re-use!
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError CloseKey(KeyH aKeyH);

    /// @brief Reads a named value in specified format into passed memory buffer
    /// @param aKeyH[in] an open key handle
    /// @param aValueName[in] name of the value to read. Some keys offer special ".XXX"
    ///        suffixes (VALSUFF_XXX) to value names, which return alternate values (like a timestamp field's
    ///        time zone name with ".TZNAME").
    /// @param aValType[in] desired return type, see VALTYPE_xxxx
    /// @param aBuffer[in/out] buffer where to store the data
    /// @param aBufSize[in] size of buffer in bytes (ALWAYS in bytes, even if value is Unicode string)
    ///        Note: to get only size of a value (useful especially for strings), pass 0 as buffer size.
    /// @param aValSize[out] actual size of value.
    ///        For VALTYPE_TEXT, size is string length (IN BYTES) excluding NULL terminator
    ///        Note that this will be set also when return value is LOCERR_BUFTOOSMALL,
    ///        to indicate the required buffer size (buffer contents is undefined in case of LOCERR_BUFTOOSMALL).
    ///        For values that can be truncated (strings), LOCERR_TRUNCATED will be returned
    ///        when returned value is not entire value - aValSize is truncated size then. Use
    ///        a call with aBufSize==0 to determine actual value size in bytes. Add one character size
    ///        to the buffer size in case of VALTYPE_TEXT for the NUL terminator.
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
    EMBVIRTUAL TSyError GetValue(
      KeyH aKeyH, cAppCharP aValueName,
      uInt16 aValType,
      appPointer aBuffer, memSize aBufSize, memSize &aValSize
    );

    /// @brief Writes a named value in specified format passed in memory buffer
    /// @param aKeyH[in] an open key handle
    /// @param aValueName[in] name of the value to write. Some keys offer special ".XXX"
    ///        suffixes (VALSUFF_XXX) to value names, which are used to set alternate values (like a
    ///        timestamp field's time zone name with ".TZNAME").
    /// @param aValType[in] type of value passed in, see VALTYPE_xxxx
    /// @param aBuffer[in] buffer containing the data
    /// @param aValSize[in] size of value. For VALTYPE_TEXT, size can be passed as -1 if string is null terminated
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure. If value
    ///         buffer passed in is too small for aValType (such as only 2 bytes for a VALTYPE_INT32,
    ///         LOCERR_BUFTOOSMALL will be returned and nothing stored. If buffer passed in is too
    ///         long (e.g. for strings) to be entirely stored, only the beginning is stored and
    ///         LOCERR_TRUNCATED is returned.
    EMBVIRTUAL TSyError SetValue(
      KeyH aKeyH, cAppCharP aValueName,
      uInt16 aValType,
      cAppPointer aBuffer, memSize aValSize
    );

    /// @brief get value's ID for use with Get/SetValueByID()
    /// @param aKeyH[in] an open key handle
    /// @param aName[in] name of the value to write. Some keys offer special ".FLAG.XXX"
    ///        values, which return flag bits which can be added to the regular ID to obtain
    ///        alternate values (like the value name with ".FLAG.VALNAME", useful when iterating
    ///        over values).
    /// @return KEYVAL_ID_UNKNOWN when no ID available for name, ID of value otherwise
    EMBVIRTUAL sInt32 GetValueID(KeyH aKeyH, cAppCharP aName);


    /// @brief Reads a named value in specified format into passed memory buffer
    /// @param aKeyH[in] an open key handle
    /// @param aID[in] ID of the value to read (evenually plus flag mask obtained by GetValueID(".FLAG.XXX") call)
    /// @param aArrayIndex[in] 0-based array element index for array values.
    /// @param aValType[in] desired return type, see VALTYPE_xxxx
    /// @param aBuffer[in/out] buffer where to store the data
    /// @param aBufSize[in] size of buffer in bytes (ALWAYS in bytes, even if value is Unicode string)
    ///        Note: to get only size of a value (useful especially for strings), pass 0 as buffer size.
    /// @param aValSize[out] actual size of value.
    ///        For VALTYPE_TEXT, size is string length (IN BYTES) excluding NULL terminator
    ///        Note that this will be set also when return value is LOCERR_BUFTOOSMALL,
    ///        to indicate the required buffer size (buffer contents is undefined in case of LOCERR_BUFTOOSMALL).
    ///        For values that can be truncated (strings), LOCERR_TRUNCATED will be returned
    ///        when returned value is not entire value - aValSize is truncated size then. Use
    ///        a call with aBufSize==0 to determine actual value size in bytes. Add one character size
    ///        to the buffer size in case of VALTYPE_TEXT for the NUL terminator.
    /// @return LOCERR_OK on success, LOCERR_OUTOFRANGE when array index is out of range
    ///        SyncML or LOCERR_xxx error code on other failure.
    EMBVIRTUAL TSyError GetValueByID(
      KeyH aKeyH, sInt32 aID, sInt32 aArrayIndex,
      uInt16 aValType,
      appPointer aBuffer, memSize aBufSize, memSize &aValSize
    );

    /// @brief Writes a named value in specified format passed in memory buffer
    /// @param aKeyH[in] an open key handle
    /// @param aID[in] ID of the value to write (evenually plus flag mask obtained by GetValueID(".FLAG.XXX") call)
    /// @param aArrayIndex[in] 0-based array element index for array values.
    /// @param aValType[in] type of value passed in, see VALTYPE_xxxx
    /// @param aBuffer[in] buffer containing the data
    /// @param aValSize[in] size of value. For VALTYPE_TEXT, size can be passed as -1 if string is null terminated
    /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure. If value
    ///         buffer passed in is too small for aValType (such as only 2 bytes for a VALTYPE_INT32,
    ///         LOCERR_BUFTOOSMALL will be returned and nothing stored. If buffer passed in is too
    ///         long (e.g. for strings) to be entirely stored, only the beginning is stored and
    ///         LOCERR_TRUNCATED is returned.
    EMBVIRTUAL TSyError SetValueByID(
      KeyH aKeyH, sInt32 aID, sInt32 aArrayIndex,
      uInt16 aValType,
      cAppPointer aBuffer, memSize aValSize
    );

    /// @name DB-Api-like access to datastore adaptor (eg. when using tunnel plugin)
    /// @note These all return LOCERR_NOTIMP when DBAPI_TUNNEL_SUPPORT is not defined
    ///
    /// @{
    EMBVIRTUAL TSyError StartDataRead    ( SessionH aSessionH, cAppCharP lastToken, cAppCharP resumeToken ) TUNNEL_IMPL;
    EMBVIRTUAL TSyError ReadNextItem     ( SessionH aSessionH,  ItemID aID, appCharP *aItemData, sInt32 *aStatus, bool aFirst ) TUNNEL_IMPL;
    EMBVIRTUAL TSyError ReadItem         ( SessionH aSessionH, cItemID aID, appCharP *aItemData ) TUNNEL_IMPL;
    EMBVIRTUAL TSyError EndDataRead      ( SessionH aSessionH ) TUNNEL_IMPL;
    EMBVIRTUAL TSyError StartDataWrite   ( SessionH aSessionH ) TUNNEL_IMPL;
    EMBVIRTUAL TSyError InsertItem       ( SessionH aSessionH, cAppCharP aItemData,  ItemID aID ) TUNNEL_IMPL;
    EMBVIRTUAL TSyError UpdateItem       ( SessionH aSessionH, cAppCharP aItemData, cItemID aID, ItemID updID ) TUNNEL_IMPL;
    EMBVIRTUAL TSyError MoveItem         ( SessionH aSessionH, cItemID aID, cAppCharP newParID ) TUNNEL_IMPL;
    EMBVIRTUAL TSyError DeleteItem       ( SessionH aSessionH, cItemID aID ) TUNNEL_IMPL;
    EMBVIRTUAL TSyError EndDataWrite     ( SessionH aSessionH, bool success, appCharP *newToken ) TUNNEL_IMPL;
    EMBVIRTUAL void     DisposeObj       ( SessionH aSessionH, void* memory ) TUNNEL_IMPL_VOID;

    // ---- asKey ----
    EMBVIRTUAL TSyError ReadNextItemAsKey( SessionH aSessionH,  ItemID aID, KeyH aItemKey, sInt32 *aStatus, bool aFirst ) TUNNEL_IMPL;
    EMBVIRTUAL TSyError ReadItemAsKey    ( SessionH aSessionH, cItemID aID, KeyH aItemKey ) TUNNEL_IMPL;
    EMBVIRTUAL TSyError InsertItemAsKey  ( SessionH aSessionH, KeyH aItemKey,  ItemID aID ) TUNNEL_IMPL;
    EMBVIRTUAL TSyError UpdateItemAsKey  ( SessionH aSessionH, KeyH aItemKey, cItemID aID, ItemID updID ) TUNNEL_IMPL;

    EMBVIRTUAL TSyError debugPuts(cAppCharP aFile, int aLine, cAppCharP aFunction,
                                  int aDbgLevel, cAppCharP aLinePrefix,
                                  cAppCharP aText);

    /// @}

    /// @brief returns the current application base object
    TSyncAppBase *getSyncAppBase(void) { return fAppBaseP; };

    #ifdef DIRECT_APPBASE_GLOBALACCESS
    // only as a hack until we are completely free of global anchors
    void setSyncAppBase(TSyncAppBase *aAppBaseP) { fAppBaseP=aAppBaseP; };
    #endif

  protected:
    /// @brief Must be derived in engineBase derivates for generating root for the appropriate settings tree
    /// @return root settings object, or NULL if failure
    virtual TSettingsKeyImpl *newSettingsRootKey(void) {
      return new TSettingsRootKey(this); // return base class which can return some engine infos
    };

    /// @brief returns a new application base.
    /// @note in engineInterface based targets, this is the replacement for the formerly
    ///       global newSyncAppBase() factory function.
    virtual TSyncAppBase *newSyncAppBase(void) = 0;

    /// @brief returns the SML instance for a given session handle
    virtual InstanceID_t getSmlInstanceOfSession(SessionH aSessionH) { return 0; /* no instance in base class */ };

  private:

    // conversion helper
    // - converts byte stream (which can be an 8-bit char set or unicode in intel or motorola order)
    //   to internal UTF-8, according to text mode set with SetStringMode()
    cAppCharP makeAppString(cAppCharP aTextP, string &aString);

    // the application base
    TSyncAppBase *fAppBaseP;

    // String format modes
    TCharSets fCharSet;
    bool fBigEndian;
    TLineEndModes fLineEndMode;

}; // TEngineInterface


} // namespace sysync

#endif // ENGINEINTERFACE_SUPPORT

#endif // ENGINEINTERFACE_H
// eof
