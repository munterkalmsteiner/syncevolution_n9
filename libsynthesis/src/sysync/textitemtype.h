/*
 *  File:         TextItemType.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TTextItemType
 *    base class for plain text based items (notes, emails...)
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 *  2002-05-29 : luz : created from MimeDirItemType
 *
 */

#ifndef TextItemType_H
#define TextItemType_H

// includes
#include "syncitemtype.h"
#include "multifielditemtype.h"
#include "textprofile.h"


namespace sysync {

// Text based datatype
class TTextTypeConfig : public TMultiFieldTypeConfig
{
  typedef TMultiFieldTypeConfig inherited;
public:
  TTextTypeConfig(const char *aElementName, TConfigElement *aParentElementP);
  virtual ~TTextTypeConfig();
  // properties
  // public functions
  // - create Sync Item Type of appropriate type from config
  virtual TSyncItemType *newSyncItemType(TSyncSession *aSessionP, TSyncDataStore *aDatastoreP);
protected:
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void localResolve(bool aLastPass);
  #endif
  virtual void clear();
}; // TTextTypeConfig

const uInt16 ity_text=102; // must be unique

class TTextItemType: public TMultiFieldItemType
{
  typedef TMultiFieldItemType inherited;
public:
  // constructor
  TTextItemType(
    TSyncSession *aSessionP,
    TDataTypeConfig *aTypeCfgP, // type config
    const char *aCTType,
    const char *aVerCT,
    TSyncDataStore *aRelatedDatastoreP,
    TFieldListConfig *aFieldDefinitions // field definitions
  );
  // destructor
  virtual ~TTextItemType();
  // access to type
  virtual cAppCharP getTypeVers(sInt32 aMode=0);
  virtual uInt16 getTypeID(void) const { return ity_text; };
  virtual bool isBasedOn(uInt16 aItemTypeID) const { return aItemTypeID==ity_text ? true : TMultiFieldItemType::isBasedOn(aItemTypeID); };
  // differentiation between implemented and just descriptive TSyncTypeItems
  virtual bool isImplemented(void) { return true; }; // MIME-DIR is an implementation
  // helper to create same-typed instance via base class
  // MUST BE IMPLEMENTED IN ALL DERIVED CLASSES!
  virtual TSyncItemType *newCopyForSameType(
    TSyncSession *aSessionP,     // the session
    TSyncDataStore *aDatastoreP  // the datastore
  );
  /// @brief copy CTCap derived info from another SyncItemType
  virtual bool copyCTCapInfoFrom(TSyncItemType &aSourceItem);
  #ifdef OBJECT_FILTERING
  // filtering
  // - get field index of given filter expression identifier.
  virtual sInt16 getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex);
  #endif
protected:
  // - analyze CTCap for specific type
  virtual bool analyzeCTCap(SmlDevInfCTCapPtr_t aCTCapP);
  // - obtain property list for type, returns NULL if none available
  virtual SmlDevInfCTDataPropListPtr_t newCTDataPropList(TTypeVariantDescriptor aVariantDescriptor);
  // - Filtering: add keywords and property names to filterCap
  virtual void addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps, TTypeVariantDescriptor aVariantDesc);
  // Item data management
  // - create new sync item of proper type and optimization for specified target
  virtual TSyncItem *internalNewSyncItem(TSyncItemType *aTargetItemTypeP, TLocalEngineDS *aLocalDatastoreP);
  // - fill in SyncML data (but leaves IDs empty)
  virtual bool internalFillInData(
    TSyncItem *aSyncItemP,      // SyncItem to be filled with data
    SmlItemPtr_t aItemP,        // SyncML toolkit item Data to be converted into SyncItem (may be NULL if no data, in case of Delete or Map)
    TLocalEngineDS *aLocalDatastoreP, // local datastore
    TStatusCommand &aStatusCmd  // status command that might be modified in case of error
  );
  // - sets data and meta from SyncItem data, but leaves source & target untouched
  virtual bool internalSetItemData(
    TSyncItem *aSyncItemP,  // the syncitem to be represented as SyncML
    SmlItemPtr_t aItem,     // item with NULL meta and NULL data
    TLocalEngineDS *aLocalDatastoreP // local datastore
  );
private:
  // member fields
  TProfileHandler *fProfileHandlerP;
  TTextTypeConfig *fTypeCfgP; // the text type config element
}; // TTextItemType


} // namespace sysync

#endif  // TextItemType_H

// eof
