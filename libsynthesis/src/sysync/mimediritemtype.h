/*
 *  File:         mimediritemtype.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TMimeDirItemType
 *    base class for MIME DIR based content types (vCard, vCalendar...)
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-06-12 : luz : created
 *
 */

#ifndef MimeDirItemType_H
#define MimeDirItemType_H

// includes
#include "syncitemtype.h"
#include "multifielditemtype.h"

#include "mimedirprofile.h"

#include <set>

namespace sysync {


// MIME-dir based datatype
class TMIMEDirTypeConfig : public TMultiFieldTypeConfig
{
  typedef TMultiFieldTypeConfig inherited;
public:
  TMIMEDirTypeConfig(const char *aElementName, TConfigElement *aParentElementP);
  virtual ~TMIMEDirTypeConfig();
  // properties
  // - associated MIME profile (casted version of ancestor's fProfileConfigP)
  TMIMEProfileConfig *fMIMEProfileP;
  // public functions
  // - create Sync Item Type of appropriate type from config
  virtual TSyncItemType *newSyncItemType(TSyncSession *aSessionP, TSyncDataStore *aDatastoreP);
  // get a descriptor for selecting a variant of a datatype (if any), NULL=no variant with this name
  virtual TTypeVariantDescriptor getVariantDescriptor(const char *aVariantName);
protected:
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  virtual void localResolve(bool aLastPass);
  virtual void clear();
}; // TMIMEDirTypeConfig


const uInt16 ity_mimedir=101; // must be unique

class TMimeDirItemType: public TMultiFieldItemType
{
  typedef TMultiFieldItemType inherited;
  friend class TMimeDirProfileHandler;
public:
  // constructor
  TMimeDirItemType(
    TSyncSession *aSessionP,
    TDataTypeConfig *aTypeConfigP,
    const char *aCTType,
    const char *aVerCT,
    TSyncDataStore *aRelatedDatastoreP,
    TFieldListConfig *aFieldDefinitions // field definitions
  );
  // destructor
  virtual ~TMimeDirItemType();
  // access to type
  virtual uInt16 getTypeID(void) const { return ity_mimedir; };
  virtual bool isBasedOn(uInt16 aItemTypeID) const { return aItemTypeID==ity_mimedir ? true : TMultiFieldItemType::isBasedOn(aItemTypeID); };
  // differentiation between implemented and just descriptive TSyncTypeItems
  virtual bool isImplemented(void) { return true; }; // MIME-DIR is an implementati on
  // returns true if field options are based on remote devinf (and not just defaults)
  virtual bool hasReceivedFieldOptions(void) { return fReceivedFieldDefs; };
  // helper to create same-typed instance via base class
  // MUST BE IMPLEMENTED IN ALL DERIVED CLASSES!
  virtual TSyncItemType *newCopyForSameType(
    TSyncSession *aSessionP,     // the session
    TSyncDataStore *aDatastoreP  // the datastore
  );
  #ifdef OBJECT_FILTERING
  // filtering
  // - get field index of given filter expression identifier.
  virtual sInt16 getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex);
  #endif
protected:
  // member fields
  TMimeDirProfileHandler *fProfileHandlerP;
  // CTCap parsing/generation
  // - analyze CTCap for specific type
  virtual bool analyzeCTCap(SmlDevInfCTCapPtr_t aCTCapP);
  /// @brief copy CTCap derived info from another SyncItemType
  virtual bool copyCTCapInfoFrom(TSyncItemType &aSourceItemP);
  // - obtain property list for type, returns NULL if none available
  virtual SmlDevInfCTDataPropListPtr_t newCTDataPropList(TTypeVariantDescriptor aVariantDescriptor);
  #ifdef OBJECT_FILTERING
  // - Filtering: add keywords and property names to filterCap
  virtual void addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps, TTypeVariantDescriptor aVariantDesc);
  #endif
  // scan for specific property value string (for version check)
  bool parseForProperty(const char *aText, const char *aPropName, string &aString);
  bool parseForProperty(SmlItemPtr_t aItemP, const char *aPropName, string &aString);
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
  bool fReceivedFieldDefs;
}; // TMimeDirItemType



} // namespace sysync

#endif  // MimeDirItemType_H

// eof

