/*
 *  File:         MultiFieldItem.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TMultiFieldItem
 *    Item consisting of multiple data fields (TItemField objects)
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-08-08 : luz : created
 *
 */
#ifndef MultiFieldItem_H
#define MultiFieldItem_H

// includes
#include "syncitem.h"
#include "itemfield.h"
#include "configelement.h"
#include "syncappbase.h"



using namespace sysync;


namespace sysync {


// undefined field/var index
// Notes:
// - positive indices address fields in the field list, negative indices address script variables
// - this must be in 8-bit-negative range for current implementation of TScripTContext
// - FID_NOT_SUPPORTED must be the same value as these are used interchangeably
//   (historically FID_NOT_SUPPORTED was -1). Definition of FID_NOT_SUPPORTED checks this now.
#define VARIDX_UNDEFINED -128


// helper macros for getting a field by FID with casting
// - get dynamically casted field, returns NULL if field has wrong type or does not exist
#define GETFIELD_DYNAMIC_CAST(ty,tyid,fid,itemP) ( itemP->getField(fid) ? ITEMFIELD_DYNAMIC_CAST_PTR(ty,tyid,itemP->getField(fid)) : NULL )
// - get statically casted field, field must exist and have that type
#define GETFIELD_STATIC_CAST(ty,fid,itemP) ( static_cast<ty *>(itemP->getField(fid)) )

// single field definition
class TFieldDefinition {
public:
  // type
  TItemFieldTypes type;
  #ifdef ARRAYFIELD_SUPPORT
  bool array; // set if this is an array field
  #endif
  // name
  TCFG_STRING fieldname;
  // relevant for equality (used in slow sync and in conflicts)
  TEqualityMode eqRelevant;
  // relevant for age sorting (used in case of conflict)
  bool ageRelevant;
  // merge options (used whenever one record overwrites other)
  // - if mergeMode = mem_none, merge is disabled
  // - if mergeMode = mem_fillempty, empty fields will be filled, but no concatenation is used
  // - if mergeMode>=0, fields that are non-equal will be accumulated
  // - if >0 then mergeMode will be used as separation char
  char mergeMode;
}; // TFieldDefinition


#ifdef HARDCODED_TYPE_SUPPORT

// single field definition
typedef struct {
  // type
  TItemFieldTypes type;
  //#ifdef ARRAYFIELD_SUPPORT
  //%%% make all templates equal
  bool array; // set if this is an array field
  //#endif
  // name
  const char *fieldname;
  // relevant for equality (used in slow sync and in conflicts)
  TEqualityMode eqRelevant;
  // relevant for age sorting (used in case of conflict)
  bool ageRelevant;
  // merge options (used whenever one record overwrites other)
  // - if mergeMode = mem_none, merge is disabled
  // - if mergeMode = mem_fillempty, empty fields will be filled, but no concatenation is used
  // - if mergeMode>=0, fields that are non-equal will be accumulated
  // - if >0 then mergeMode will be used as separation char
  char mergeMode;
  // Type limit defaults
  uInt32 maxSize;
  bool noTruncate;
} TFieldDefinitionTemplate;


// field definitions template
typedef struct {
  // number of fields
  sInt16 numFields;
  // sortable by age?
  bool ageSortable; // %%%% not needed any more, will be set automatically if at least one field is agerelevant
  // field definitions [0..numfields-1]
  // note: sort order is 0..n, i.e. fields with lower indexes are more relevant
  const TFieldDefinitionTemplate *fieldDefs;
} TFieldDefinitionsTemplate;

#endif


// field array
typedef std::vector<TFieldDefinition> TFieldDefinitionList;


class TMultiFieldDatatypesConfig; // forward
class TMultiFieldItem;
class TMultiFieldItemType;

// field definition config
// This object MUST exist only ONCE per
// MultiField-based type (as assignment compatibility is
// given by POINTER IDENTITY of the MultiField's TFieldListConfig).
class TFieldListConfig : public TConfigElement
{
  typedef TConfigElement inherited;
  friend class TMultiFieldDatatypesConfig;
public:
  TFieldListConfig(const char* aName, TConfigElement *aParentElement);
  virtual ~TFieldListConfig();
  #ifdef HARDCODED_TYPE_SUPPORT
  const TFieldDefinitionsTemplate *fFieldListTemplateP;
  void readFieldListTemplate(const TFieldDefinitionsTemplate *aTemplateP);
  #endif
  // properties
  // - sortable by age?
  bool fAgeSortable;
  // - field definition array
  TFieldDefinitionList fFields;
  sInt16 numFields(void) { return fFields.size(); };
  sInt16 fieldIndex(const char *aName, size_t aLen=0);
protected:
  // check config elements
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void localResolve(bool aLastPass);
  #endif
  virtual void clear();
}; // TFieldListConfig


// profile handler abstract base class
class TProfileHandler
{
public:
  // constructor/destructor
  TProfileHandler(TProfileConfig *aProfileCfgP, TMultiFieldItemType *aItemTypeP);
  virtual ~TProfileHandler();
  #ifdef OBJECT_FILTERING
  // filtering
  // - get field index of given filter expression identifier.
  virtual sInt16 getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex) = 0;
  // - add keywords and property names to filterCap
  virtual void addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps, TTypeVariantDescriptor aVariantDesc, TSyncItemType *aItemTypeP) = 0;
  #endif
  // DevInf
  // - obtain property list for type, returns NULL if none available
  virtual SmlDevInfCTDataPropListPtr_t newCTDataPropList(TTypeVariantDescriptor aVariantDescriptor, TSyncItemType *aItemTypeP) { return NULL; };
  // - Analyze CTCap part of devInf
  virtual bool analyzeCTCap(SmlDevInfCTCapPtr_t aCTCapP, TSyncItemType *aItemTypeP) { return true; };
  // set profile options
  // - mode (for those profiles that have more than one, like MIME-DIR's old/standard)
  virtual void setProfileMode(sInt32 aMode) { /* nop here */ };
#ifndef NO_REMOTE_RULES
  // - choose remote rule by name, true if found
  virtual void setRemoteRule(const string &aRemoteRuleName) { /* nop here */ }
#endif
  // set related datastore (NULL for independent use e.g. from script functions)
  void setRelatedDatastore(TLocalEngineDS *aRelatedDatastoreP) { fRelatedDatastoreP = aRelatedDatastoreP; };
  // generate Text Data
  virtual void generateText(TMultiFieldItem &aItem, string &aString) = 0;
  // parse Data item
  virtual bool parseText(const char *aText, stringSize aTextSize, TMultiFieldItem &aItem) = 0;
  // Debug
  #ifdef SYDEBUG
  TDebugLogger *getDbgLogger(void);
  uInt32 getDbgMask(void);
  #endif
protected:
  TLocalEngineDS *fRelatedDatastoreP; // related datastore, can be NULL
  TMultiFieldItemType *fItemTypeP; // the item type using this handler
  // helpers
  // - get session pointer
  TSyncSession *getSession(void);
  // - get session zones pointer
  GZones *getSessionZones(void);
  // - check availability (depends on item "supported" flags only in SyncML datastore context)
  bool isFieldAvailable(TMultiFieldItem &aItem, sInt16 aFieldIndex);
}; // TProfileHandler


// profile config base class (e.g. for MIMEDIR and text profiles)
// A profile is a config element related to a single fieldlist, but probably to more
// than one datatype
class TProfileConfig : public TConfigElement
{
  typedef TConfigElement inherited;
  friend class TMultiFieldDatatypesConfig;
  friend class TTextTypeConfig;
public:
  TProfileConfig(const char* aName, TConfigElement *aParentElement);
  virtual ~TProfileConfig();
  // handler factory
  virtual TProfileHandler *newProfileHandler(TMultiFieldItemType *aItemTypeP) = 0;
  // properties
  // - field list config
  TFieldListConfig *fFieldListP;
protected:
  // check config elements
  virtual void clear();
}; // TProfileConfig



// fieldlists list
typedef std::list<TFieldListConfig *> TFieldListsList;
// profiles list
typedef std::list<TProfileConfig *> TProfilesList;

// multi-field based type config registry
class TMultiFieldDatatypesConfig : public TDatatypesConfig
{
  typedef TDatatypesConfig inherited;
public:
  TMultiFieldDatatypesConfig(TConfigElement *aParentElement);
  virtual ~TMultiFieldDatatypesConfig();
  // properties
  // - field lists
  TFieldListsList fFieldLists;
  TFieldListConfig *getFieldList(const char *aName);
  // - profiles (fieldlist-related definitions, but probably used in multiple datatypes)
  TProfilesList fProfiles;
  TProfileConfig *getProfile(const char *aName);
protected:
  // check config elements
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void localResolve(bool aLastPass);
  #endif
  virtual void clear();
private:
  void internalClear(void);
}; // TMultiFieldDatatypesConfig


const uInt16 ity_multifield = 100; // must be unique

class TMultiFieldItem: public TSyncItem
{
  typedef TSyncItem inherited;
public:
  TMultiFieldItem(
    TMultiFieldItemType *aItemTypeP,        // owner's (=source) type
    TMultiFieldItemType *aTargetItemTypeP   // target type (for optimization)
  );
  virtual ~TMultiFieldItem();
  // access to type
  virtual uInt16 getTypeID(void) const { return ity_multifield; };
  virtual bool isBasedOn(uInt16 aItemTypeID) const { return aItemTypeID==ity_multifield ? true : TSyncItem::isBasedOn(aItemTypeID); };
  // assignment (IDs and contents)
  virtual TSyncItem& operator=(TSyncItem &aSyncItem) { return TSyncItem::operator=(aSyncItem); };
  // changelog support
  #if defined(CHECKSUM_CHANGELOG) && !defined(RECORDHASH_FROM_DBAPI)
  virtual uInt16 getDataCRC(uInt16 crc=0, bool aEQRelevantOnly=false);
  #endif
  // update dependencies of fields (such as BLOB proxies) on localID
  virtual void updateLocalIDDependencies(void);
  // - access fields by field name or index
  TItemField *getField(sInt16 aFieldIndex);
  TItemField *getField(const char *aFieldName);
  TItemField &getFieldRef(sInt16 aFieldIndex); // get field reference (create if not yet created)
  // for array support, but is always there to simplify implementations
  TItemField *getArrayField(const char *aFieldName, sInt16 aIndex, bool aExistingOnly=false);
  TItemField *getArrayField(sInt16 aFid, sInt16 aIndex, bool aExistingOnly=false);
  TItemField *getArrayFieldAdjusted(sInt16 aFid, sInt16 aIndex, bool aExistingOnly=false);
  // find index of field (returns FID_NOT_SUPPORTED if field is not a field of this item)
  sInt16 getIndexOfField(const TItemField *aFieldP);
  // adjust fid and repeat offset to access array element if
  // base fid is an array field or to offset fid accordingly
  // if based fid is NOT an array field
  // - returns adjusted aFid and aIndex ready to be used with getArrayField()
  // - returns true if aFid IS an array field
  bool adjustFidAndIndex(sInt16 &aFid, sInt16 &aIndex);
  // - check if field is assigned (exists and has a value)
  bool const isAssigned(sInt16 aFieldIndex);
  bool const isAssigned(const char *aFieldName);
  // access field definitions
  TFieldListConfig *getFieldDefinitions(void) { return fFieldDefinitionsP; };
  // get associated MultiFieldItemType
  TMultiFieldItemType *getItemType(void) { return fItemTypeP; };
  TMultiFieldItemType *getTargetItemType(void) { return fTargetItemTypeP; };
  TMultiFieldItemType *getLocalItemType(void);
  TMultiFieldItemType *getRemoteItemType(void);
  // field availability (combined source & target)
  bool isAvailable(sInt16 aFieldIndex);
  bool isAvailable(const char *aFieldName);
  bool knowsRemoteFieldOptions(void);
  // - instantiate all fields that are available in source and target
  void assignAvailables(void);
  // compare abilities
  virtual bool comparable(TSyncItem &aItem);
  virtual bool sortable(TSyncItem &aItem);
  // clear item data
  virtual void cleardata(void);
  // replace data contents from specified item
  // - aAvailable only: only replace contents actually available in aItem, leave rest untouched
  // - aDetectCutOffs: handle case where aItem could have somehow cut-off data and prevent replacing
  //   complete data with cut-off version (e.g. mobiles like T39m with limited name string capacity)
  virtual bool replaceDataFrom(TSyncItem &aItem, bool aAvailableOnly=false, bool aDetectCutoffs=false, bool aAssignedOnly=false, bool aTransferUnassigned=false);
  // check item before processing it
  virtual bool checkItem(TLocalEngineDS *aDatastoreP);
  #ifdef SYSYNC_SERVER
  // merge this item with specified item.
  // Notes:
  // - specified item is treated as loosing item, this item is winning item
  // - also updates other item to make sure it is equal to the winning after the merge
  // sets (but does not reset) change status of this and other item.
  // Note that changes of non-relevant fields are not reported here.
  virtual void mergeWith(TSyncItem &aItem, bool &aChangedThis, bool &aChangedOther, TLocalEngineDS *aDatastoreP);
  // standard merge (subset of mergeWith, used if no merge script is defined)
  void standardMergeWith(TMultiFieldItem &aItem, bool &aChangedThis, bool &aChangedOther);
  // compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem
  // SYSYNC_NOT_COMPARABLE if not equal and no ordering known
  virtual sInt16 compareWith(
    TSyncItem &aItem,
    TEqualityMode aEqMode,
    TLocalEngineDS *aDatastoreP
    #ifdef SYDEBUG
    ,bool aDebugShow=false
    #endif
  );
  // standard compare (subset of compareWith, used if no compare script is defined)
  sInt16 standardCompareWith(
    TMultiFieldItem &aItem,
    TEqualityMode aEqMode,
    bool aDebugShow
  );
  #endif
  #ifdef SYDEBUG
  // show item contents for debug
  virtual void debugShowItem(uInt32 aDbgMask=DBG_DATA);
  #endif
  #ifdef OBJECT_FILTERING
  // New style generic filtering
  // - check if item passes filter and probably apply some modifications to it
  virtual bool postFetchFiltering(TLocalEngineDS *aDatastoreP);
  // Old style object filtering
  // - test if item passes filter
  virtual bool testFilter(const char *aFilterString);
  // - make item pass filter
  virtual bool makePassFilter(const char *aFilterString);
  // - actually process filter expression
  bool processFilter(bool aMakePass, const char *&aPos, const char *aStop=NULL, sInt16 aLastOpPrec=0);
  #endif
protected:
  // associated multifield type items (owner and target)
  TMultiFieldItemType *fItemTypeP;
  TMultiFieldItemType *fTargetItemTypeP;
  // associated field definitions (copied from ItemType for performance)
  TFieldListConfig *fFieldDefinitionsP;
  // contents: array of actual fields
  TItemField **fFieldsP;
private:
  // cast pointer to same type, returns NULL if incompatible
  TMultiFieldItem *castToSameTypeP(TSyncItem *aItemP); // all are compatible TSyncItem
}; // TMultiFieldItem


#ifdef DBAPI_TUNNEL_SUPPORT

// key for access to a item using the settings key API
class TMultiFieldItemKey :
  public TItemFieldKey
{
  typedef TItemFieldKey inherited;
public:
  TMultiFieldItemKey(TEngineInterface *aEngineInterfaceP, TMultiFieldItem *aItemP, bool aOwnsItem=false) :
    inherited(aEngineInterfaceP),
    fItemP(aItemP),
    fOwnsItem(aOwnsItem)
  {};
  virtual ~TMultiFieldItemKey() { forgetItem(); };
  TMultiFieldItem *getItem(void) { return fItemP; };
  void setItem(TMultiFieldItem *aItemP, bool aPassOwner=false);

protected:

  // methods to actually access a TItemField
  virtual sInt16 getFidFor(cAppCharP aName, stringSize aNameSz);
  virtual bool getFieldNameFromFid(sInt16 aFid, string &aFieldName);
  virtual TItemField *getBaseFieldFromFid(sInt16 aFid);

  // the item being accessed
  TMultiFieldItem *fItemP;
  bool fOwnsItem;
  // iterator
  sInt16 fIteratorFid;

private:
  void forgetItem() { if (fOwnsItem && fItemP) { delete fItemP; } fItemP=NULL; };

}; // TMultiFieldItemKey

#endif // DBAPI_TUNNEL_SUPPORT




} // namespace sysync

#endif  // MultiFieldItem_H

// eof
