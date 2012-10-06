/*
 *  File:         MultiFieldItemType.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TMultiFieldItemType
 *    Type consisting of multiple data fields (TItemField objects)
 *    To be used as base class for field formats like vCard,
 *    vCalendar etc.
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-08-13 : luz : created
 *
 */

#ifndef MultiFieldItemType_H
#define MultiFieldItemType_H

// includes
#include "syncitemtype.h"
#include "multifielditem.h"
#include "scriptcontext.h"


using namespace sysync;


namespace sysync {

// special field IDs
#define FID_NOT_SUPPORTED -128 // no field-ID
#if defined(VARIDX_UNDEFINED) && FID_NOT_SUPPORTED!=VARIDX_UNDEFINED
  #error "FID_NOT_SUPPORTED must be the same as VARIDX_UNDEFINED"
#endif

// special field offsets
#define OFFS_NOSTORE -9999 // offset meaning "do not store"

// special repeat counts
#define REP_REWRITE 0      // unlimited repeating, but later occurrences overwrite previous ones
#define REP_ARRAY 32767    // virtually unlimited repeating, should be used with array fields only

// default profile mode (devivates might define their own)
#define PROFILEMODE_DEFAULT 0 // default mode of the profile, usually MIME-DIR



// type-instance specific options (there can be multiple
// TSyncItemType instances with the same TFieldListConfig,
// but different sets of field options, e.g. different
// versions of the same content type where one has fewer
// fields implemented, or server and client types with
// differing sets of available fields)
#define FIELD_OPT_MAXSIZE_UNKNOWN -1
#define FIELD_OPT_MAXSIZE_NONE 0
typedef struct {
  bool available; // set if field is available in this type
  sInt32 maxsize; // maximum field length, FIELD_OPT_MAXSIZE_NONE=none, FIELD_OPT_MAXSIZE_UNKNOWN=limited, but unknown
  bool notruncate; // set if type doesn't want to get truncated values for this field from remote
  sInt32 maxoccur; // maximum number of repetitions of properties based on this field
} TFieldOptions;


// multi-field based datatype
class TMultiFieldTypeConfig : public TDataTypeConfig
{
  typedef TDataTypeConfig inherited;
public:
  TMultiFieldTypeConfig(const char *aElementName, TConfigElement *aParentElementP);
  virtual ~TMultiFieldTypeConfig();
  // properties
  // - associated field list config (normally set from derived type)
  TFieldListConfig *fFieldListP;
  // - associated profile
  TProfileConfig *fProfileConfigP;
  // - mode for profile
  sInt32 fProfileMode;
  #ifdef SCRIPT_SUPPORT
  // - scripts
  string fInitScript; // executed once per usage by a datastore
  string fIncomingScript; // script that is executed after receiving item
  string fOutgoingScript; // script that is executed just before sending item
  string fFilterInitScript; // script that is executed once per session when filter params are all available. Must return true if something to filter at all
  string fPostFetchFilterScript; // script that is executed after item is fetched from DB. Must return true if item passes
  string fCompareScript; // script that is executed to compare items
  string fMergeScript; // script that is executed to merge items
  string fProcessItemScript; // script that is executed to decide what to do with an incoming item
  #endif
  #ifdef HARDCODED_TYPE_SUPPORT
  void setConfig(TFieldListConfig *aFieldList, const char *aTypeName, const char* aTypeVers);
  #endif
protected:
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void localResolve(bool aLastPass);
  #endif
  virtual void clear();
}; // TMultiFieldTypeConfig


class TMultiFieldItemType: public TSyncItemType
{
  typedef TSyncItemType inherited;
public:
  TMultiFieldItemType(
    TSyncSession *aSessionP,
    TDataTypeConfig *aTypeConfigP,
    const char *aCTType,
    const char *aVerCT,
    TSyncDataStore *aRelatedDatastoreP,
    TFieldListConfig *aFieldDefinitions // field definition list
  );
  virtual ~TMultiFieldItemType();
  // access to type
  virtual uInt16 getTypeID(void) const { return ity_multifield; };
  virtual bool isBasedOn(uInt16 aItemTypeID) const { return aItemTypeID==ity_multifield ? true : TSyncItemType::isBasedOn(aItemTypeID); };
  // differentiation between implemented and just descriptive TSyncTypeItems
  virtual bool isImplemented(void) { return true; }; // MultiFields are implemented
  // compatibility (=assignment compatibility between items based on these types)
  virtual bool isCompatibleWith(TSyncItemType *aReferenceType);
  // test if item could contain cut-off data (e.g. because of field size restrictions)
  // compared to specified reference item
  virtual bool mayContainCutOffData(TSyncItemType *aReferenceType);
  // helper to create same-typed instance via base class
  // MUST BE IMPLEMENTED IN ALL non-virtual DERIVED CLASSES!
  virtual TSyncItemType *newCopyForSameType(
    TSyncSession *aSessionP,     // the session
    TSyncDataStore *aDatastoreP  // the datastore
  );
  /// @brief copy CTCap derived info from another SyncItemType
  virtual bool copyCTCapInfoFrom(TSyncItemType &aSourceItemP);
  // apply default limits to type (e.g. from hard-coded template in config)
  virtual void addDefaultTypeLimits(void);
  // get config pointer
  TMultiFieldTypeConfig *getMultifieldTypeConfig(void) { return static_cast<TMultiFieldTypeConfig *>(fTypeConfigP); };
  // access field definitions
  TFieldListConfig *getFieldDefinitions(void) { return fFieldDefinitionsP; };
  // access to fields by name (returns -1 if field not found)
  sInt16 getFieldIndex(const char *aFieldName);
  bool isFieldIndexValid(sInt16 aFieldIndex);
  // access to field options (returns NULL if field not found)
  TFieldOptions *getFieldOptions(sInt16 aFieldIndex);
  TFieldOptions *getFieldOptions(const char *aFieldName)
    { return getFieldOptions(getFieldIndex(aFieldName)); };
  virtual bool hasReceivedFieldOptions(void) { return false; }; // returns true if field options are based on remote devinf (and not just defaults)
  // access to field definitions
  TFieldDefinition *getFieldDefinition(sInt16 aFieldIndex)
    { if (fFieldDefinitionsP) return &(fFieldDefinitionsP->fFields[aFieldIndex]); else return NULL; }
  // Prepare datatype for use with a datastore. This might be implemented
  // in derived classes to initialize the datastore's script context etc.
  virtual void initDataTypeUse(TLocalEngineDS *aDatastoreP, bool aForSending, bool aForReceiving);
  #ifdef OBJECT_FILTERING
  // new style generic filtering
  // - init new-style filtering, returns flag if needed at all
  virtual void initPostFetchFiltering(bool &aNeeded, bool &aNeededForAll, TLocalEngineDS *aDatastoreP);
  // - do the actual filtering
  bool postFetchFiltering(TMultiFieldItem *aItemP, TLocalEngineDS *aDatastoreP);
  // old style expression filtering
  // - get field index of given filter expression identifier. Note that
  //   derived classes might first check for MIME property names etc.
  virtual sInt16 getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex)
    { return getFieldIndex(aIdentifier); }; // base class just handles field names
  #endif
  // comparing and merging
  sInt16 compareItems(TMultiFieldItem &aFirstItem, TMultiFieldItem &aSecondItem, TEqualityMode aEqMode, bool aDebugShow, TLocalEngineDS *aDatastoreP);
  void mergeItems(
    TMultiFieldItem &aWinningItem,
    TMultiFieldItem &aLoosingItem,
    bool &aChangedWinning,
    bool &aChangedLoosing,
    TLocalEngineDS *aDatastoreP
  );
  // check item before processing it
  bool checkItem(TMultiFieldItem &aItem, TLocalEngineDS *aDatastoreP);
protected:
  // Item data management
  // - create new sync item of proper type and optimization for specified target
  /* MultiFields are only a base class and lack implementation of
     the following:
  virtual TSyncItem *internalNewSyncItem(TSyncItemType *aTargetItemTypeP, TLocalEngineDS *aLocalDatastoreP);
  */
  // - fill in SyncML data (but leaves IDs empty)
  //   Note: for MultiFieldItem, this is for post-processing data (FieldFillers)
  virtual bool internalFillInData(
    TSyncItem *aSyncItemP,      // SyncItem to be filled with data
    SmlItemPtr_t aItemP,        // SyncML toolkit item Data to be converted into SyncItem (may be NULL if no data, in case of Delete or Map)
    TLocalEngineDS *aLocalDatastoreP, // local datastore
    TStatusCommand &aStatusCmd  // status command that might be modified in case of error
  );
  // - sets data and meta from SyncItem data, but leaves source & target untouched
  //   Note: for MultiFieldItem, this is for pre-processing data (FieldFillers)
  virtual bool internalSetItemData(
    TSyncItem *aSyncItemP,  // the syncitem to be represented as SyncML
    SmlItemPtr_t aItem,     // item with NULL meta and NULL data
    TLocalEngineDS *aLocalDatastoreP // local datastore
  );
  // field definition list pointer
  TFieldListConfig *fFieldDefinitionsP;
public:
  // temporary variables needed by filter script context funcs
  bool fNeedToFilterAll; // need to filter all records, even not-changed ones (dynamic syncset)
  #ifdef SCRIPT_SUPPORT
  TLocalEngineDS *fDsP; // helper for script context funcs
  TMultiFieldItem *fFirstItemP; // helper for script context funcs
  TMultiFieldItem *fSecondItemP; // helper for script context funcs
  bool fChangedFirst; // helper for script context funcs
  bool fChangedSecond; // helper for script context funcs
  TEqualityMode fEqMode; // helper for script context funcs
  TSyncOperation fCurrentSyncOp; // helper for script context funcs
  #endif
private:
  // array of field options
  TFieldOptions *fFieldOptionsP;
}; // TMultiFieldItemType

} // namespace sysync

#endif  // MultiFieldItemType_H

// eof
