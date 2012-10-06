/*
 *  File:         SyncItemType.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TSyncItemType
 *    Type description and converter (template) for TSyncItem.
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-05-16 : luz : created
 *
 */

#ifndef SyncItemType_H
#define SyncItemType_H

#include "sysync_globs.h"

namespace sysync {


// for hardcoded config, we do not use any C++ strings
#ifndef CONFIGURABLE_TYPE_SUPPORT
  // string pointers only
  #define TCFG_STRING const char *
  #define TCFG_CSTR(s) s
  #define TCFG_ASSIGN(s,c) { if (c) s=c; else s=""; }
  #define TCFG_CLEAR(s) s=""
  #define TCFG_ISEMPTY(s) (!s || *s==0)
  #define TCFG_SIZE(s) (s ? strlen(s) : 0)
#else
  // C++ strings
  #define TCFG_STRING string
  #define TCFG_CSTR(s) s.c_str()
  #define TCFG_ASSIGN(s,c) { if (c) s=c; else s.erase(); }
  #define TCFG_CLEAR(s) s.erase()
  #define TCFG_ISEMPTY(s) s.empty()
  #define TCFG_SIZE(s) s.size()
#endif


// forward
class TSyncSession;
class TSyncItemType;
class TSyncItem;
class TStatusCommand;
class TDataTypeConfig;
class TLocalEngineDS;
class TRemoteDataStore;
class TSyncDataStore;

// container types
typedef std::list<TSyncItemType*> TSyncItemTypePContainer; // contains item types

// type variant descriptor (for now, a simple pointer, if needed we can make it a class later)
typedef void * TTypeVariantDescriptor;

const uInt16 ity_syncitem = 0; // must be unique

class TSyncItemType {
private:
  void init(
    TSyncSession *aSessionP,
    TDataTypeConfig *aTypeConfigP,
    const char *aCTType,
    const char *aVerCT,
    TSyncDataStore *aRelatedDatastoreP
  );
public:
  // constructor
  TSyncItemType(
    TSyncSession *aSessionP,
    TDataTypeConfig *aTypeConfigP,
    const char *aCTType,
    const char *aVerCT,
    TSyncDataStore *aRelatedDatastoreP
  );
  // destructor
  virtual ~TSyncItemType();
  // access to type
  virtual uInt16 getTypeID(void) const { return ity_syncitem; };
  virtual bool isBasedOn(uInt16 aItemTypeID) const { return aItemTypeID==ity_syncitem; };
  // differentiation between implemented and just descriptive TSyncTypeItems
  virtual bool isImplemented(void) { return false; }; // base class is descriptive only
  // differentiation between type describing local and remote items
  bool isRemoteType(void) { return fIsRemoteType; };
  void defineAsRemoteType(void) { fIsRemoteType = true; };
  // compatibility (=assignment compatibility between items based on these types)
  virtual bool isCompatibleWith(TSyncItemType *aReferenceType) { return this==aReferenceType; } // compatible if same type
  // get session pointer
  TSyncSession *getSession(void) { return fSessionP; };
  // get session zones pointer
  GZones *getSessionZones(void);
  // ret related datastore (can be NULL for session-global types like before DS 1.2)
  TSyncDataStore *getRelatedDatastore(void) { return fRelatedDatastoreP; };
  // helper to create same-typed instance via base class
  // MUST BE IMPLEMENTED IN ALL DERIVED CLASSES!
  virtual TSyncItemType *newCopyForSameType(
    TSyncSession *aSessionP,     // the session
    TSyncDataStore *aDatastoreP  // the datastore
  );
  // apply default limits to type (e.g. from hard-coded template in config)
  virtual void addDefaultTypeLimits(void) { /* nop */ };
  // Prepare datatype for use with a datastore. This might be implemented
  // in derived classes to initialize the datastore's script context etc.
  virtual void initDataTypeUse(TLocalEngineDS * /* aDatastoreP */, bool /* aForSending */, bool /* aForReceiving */) { /* nop */ };
  // Filtering
  // - add keywords and property names to filterCap
  virtual void addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps, TTypeVariantDescriptor aVariantDesc) { /* nop */ };
  // - check for special filter keywords the type might want to handle directly
  virtual bool checkFilterkeywordTerm(
    cAppCharP aIdent, bool aAssignToMakeTrue,
    cAppCharP aOp, bool aCaseInsensitive,
    cAppCharP aVal, bool aSpecialValue
  ) { return true; /* we do not handle this specially, handle at DS level or include into filter expression */ };
  // - init new-style filtering, returns flag if needed at all
  virtual void initPostFetchFiltering(bool &aNeeded, bool &aNeededForAll, TLocalEngineDS * /* aDatastoreP */) { aNeeded=false; aNeededForAll=false; };
  // item read and write
  // - test if item could contain cut-off data (e.g. because of field size restrictions)
  //   compared to specified reference item
  virtual bool mayContainCutOffData(TSyncItemType * /* aReferenceType */) { return false; }; // generally, no
  // - try to extract a version string from actual item data, NULL if none
  virtual bool versionFromData(SmlItemPtr_t /* aItemP */, string & /* aString */) { return false; };
  #ifdef APP_CAN_EXPIRE
  // test if modified date of item is hard-expired
  virtual sInt32 expiryFromData(SmlItemPtr_t /* aItemP */, lineardate_t & /* aDat */) { return 0; }; // default to not expired
  #endif
  /// create new empty sync item
  TSyncItem *newSyncItem(
    TSyncItemType *aTargetItemTypeP,  ///< the targeted type (for optimizing field lists etc.)
    TLocalEngineDS *aLocalDataStoreP  ///< local datastore
  );
  /// create new sync item from SyncML data
  TSyncItem *newSyncItem(
    SmlItemPtr_t aItemP,              ///< SyncML toolkit item Data to be converted into SyncItem
    TSyncOperation aSyncOp,           ///< the operation to be performed with this item
    TFmtTypes aFormat,                ///< the format (normally fmt_chr)
    TSyncItemType *aTargetItemTypeP,  ///< the targeted type (for optimizing field lists etc.)
    TLocalEngineDS *aLocalDataStoreP, ///< local datastore
    TStatusCommand &aStatusCmd        ///< status command that might be modified in case of error
  );
  /// create new SyncML toolkit item from SyncItem
  SmlItemPtr_t newSmlItem(
    TSyncItem *aSyncItemP,   ///< the syncitem to be represented as SyncML
    TLocalEngineDS *aLocalDatastoreP ///< local datastore
  );
  /// @brief get CTCap entry
  /// @param aOnlyForDS[in]
  /// - if NULL, CTCap is generated suitable for all datastores
  /// - if not NULL, CTCap is generated specifically for the datastore passed
  const SmlDevInfCTCapPtr_t getCTCapDevInf(TLocalEngineDS *aOnlyForDS,  TTypeVariantDescriptor aVariantDescriptor, bool aWithoutCTCapProps);
  /// @brief analyze CTCap for specific type
  virtual bool analyzeCTCap(SmlDevInfCTCapPtr_t aCTCapP);
  /// @brief copy CTCap derived info from another SyncItemType
  virtual bool copyCTCapInfoFrom(TSyncItemType &aSourceItem);
  /// @brief returns true if type is able to ACCEPT field level updates
  virtual bool canAcceptFieldLevelUpdates(void) { return false; }; /* no by default */
  // - static function to search type in a TSyncItemTypePContainer
  static TSyncItemType *findTypeInList(
    TSyncItemTypePContainer &aList,
    const char *aName, const char *aVers,
    bool aVersMustMatch,
    bool aMustBeImplemented,
    TSyncDataStore *aRelatedDatastoreP
  );
  static TSyncItemType *findTypeInList(
    TSyncItemTypePContainer &aList,
    SmlDevInfXmitPtr_t aXmitType,                // name and version of type
    bool aVersMustMatch,
    bool aMustBeImplemented,
    TSyncDataStore *aRelatedDatastoreP
  );
  // - static function to analyze CTCap and add entries to passed list
  static bool analyzeCTCapAndCreateItemTypes(
    TSyncSession *aSessionP,
    TRemoteDataStore *aRemoteDataStoreP,        // if not NULL, this is the datastore to which this type is local (DS 1.2 case)
    SmlDevInfCTCapPtr_t aCTCapP,
    TSyncItemTypePContainer &aLocalItemTypes,   // list to look up local types (for reference)
    TSyncItemTypePContainer &aNewItemTypes      // list to add analyzed types if not already there
  );
  // - static function to add new or copied ItemType to passed list
  static TSyncItemType *registerRemoteType(
    TSyncSession *aSessionP,
    const char *aName, const char *aVers,       // name and version of type
    TSyncItemTypePContainer &aLocalItemTypes,   // list to look up local types (for reference)
    TSyncItemTypePContainer &aNewItemTypes,     // list to add analyzed types if not already there
    TSyncDataStore *aRelatedDatastoreP
  );
  static TSyncItemType *registerRemoteType(
    TSyncSession *aSessionP,
    SmlDevInfXmitPtr_t aXmitTypeP,              // name and version of type
    TSyncItemTypePContainer &aLocalItemTypes,   // list to look up local types (for reference)
    TSyncItemTypePContainer &aNewItemTypes,     // list to add analyzed types if not already there
    TSyncDataStore *aRelatedDatastoreP
  );
  // static helper for creating rx/tx type lists
  static SmlDevInfXmitListPtr_t newXMitListDevInf(
    TSyncItemTypePContainer &aTypeList,
    TSyncItemType *aDontIncludeP
  );
  // type support check
  virtual bool supportsType(const char *aName, const char *aVers, bool aVersMustMatch=false);
  bool supportsType(SmlDevInfXmitPtr_t aXmitType, bool aVersMustMatch=false);
  // - get type name / vers
  virtual cAppCharP getTypeName(sInt32 aMode=0) { return fTypeName.c_str(); };
  virtual cAppCharP getTypeVers(sInt32 aMode=0) { return fTypeVers.c_str(); };
  void setTypeVers(const char *aVers) { fTypeVers=aVers; };
  bool hasTypeVers(void) { return !fTypeVers.empty(); };
  // - read type as Rx/Tx entry
  SmlDevInfXmitPtr_t newXMitDevInf(void);
  // - get config pointer of type
  TDataTypeConfig *getTypeConfig(void) { return fTypeConfigP; };
  // - get debug
  #ifdef SYDEBUG
  TDebugLogger *getDbgLogger(void);
  uInt32 getDbgMask(void);
  #endif
protected:
  // methods
  // obtain property list for type, returns NULL if none available
  virtual SmlDevInfCTDataPropListPtr_t newCTDataPropList(TTypeVariantDescriptor aVariantDescriptor) { return NULL; } // no properties available
  // Item data management
  // - create new sync item of proper type.
  //   NOTE: aTargetItemTypeP is passed to allow creation of optimized items for
  //   reception by a specific target type (e.g. common field list optimization etc.)
  virtual TSyncItem *internalNewSyncItem(
    TSyncItemType * /* aTargetItemTypeP */,
    TLocalEngineDS * /* aLocalDatastoreP */
  ) { return NULL; } // no op in base class (returns no item)
  // - fill in SyncML data (but leaves IDs empty)
  virtual bool internalFillInData(
    TSyncItem * /* aSyncItemP */,      // SyncItem to be filled with data
    SmlItemPtr_t /* aItemP */,        // SyncML toolkit item Data to be converted into SyncItem (may be NULL if no data, in case of Delete or Map)
    TLocalEngineDS * /* aLocalDatastoreP */, // local datastore
    TStatusCommand & /* aStatusCmd */  // status command that might be modified in case of error
  ) { return false; } // no op in base class (cannot fill)
  // - sets data and meta from SyncItem data, but leaves source & target untouched
  virtual bool internalSetItemData(
    TSyncItem * /* aSyncItemP */,  // the syncitem to be represented as SyncML
    SmlItemPtr_t /* aItem */,     // item with NULL meta and NULL data
    TLocalEngineDS * /* aLocalDatastoreP */ // local datastore
  ) { return false; } // no op in base class (leaves item untouched)
  // session pointer
  TSyncSession *fSessionP;
  // the config for this type
  TDataTypeConfig *fTypeConfigP;
private:
  // the related datastore (for DS 1.2)
  TSyncDataStore *fRelatedDatastoreP;
  // the item's type name and version
  string fTypeName;
  string fTypeVers;
  #if defined(ZIPPED_BINDATA_SUPPORT) && defined(SYDEBUG)
  sInt32 fRawDataBytes;
  sInt32 fZippedDataBytes;
  #endif
  // flag if this is a remote type
  bool fIsRemoteType;
}; // TSyncItemType


} // namespace sysync

#endif  // SyncItemType_H

// eof
