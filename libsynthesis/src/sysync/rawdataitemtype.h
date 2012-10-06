/*
 *  File:         rawdataitemtype.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TRawDataItemType
 *    Item type for 1:1 raw items (SyncML payload is exchanged 1:1 with database backend)
 *
 *  Copyright (c) 2010 by Synthesis AG (www.synthesis.ch)
 *
 *  2010-05-14 : luz : created
 *
 */

#ifndef RawDataItemType_H
#define RawDataItemType_H

// includes
#include "syncitemtype.h"
#include "multifielditemtype.h"


namespace sysync {


// MIME-dir based datatype
class TRawDataTypeConfig : public TMultiFieldTypeConfig
{
  typedef TMultiFieldTypeConfig inherited;
public:
  TRawDataTypeConfig(const char *aElementName, TConfigElement *aParentElementP);
  virtual ~TRawDataTypeConfig();
  // properties
  // public functions
  // - create Sync Item Type of appropriate type from config
  virtual TSyncItemType *newSyncItemType(TSyncSession *aSessionP, TSyncDataStore *aDatastoreP);
protected:
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  virtual void localResolve(bool aLastPass);
  virtual void clear();
public:
  // FIDs of the predefined fields we need to access
  sInt16 fFidItemData; // BLOB or string field that will contain item <data>
  // %%% add more FID vars here
}; // TRawDataTypeConfig


const uInt16 ity_rawdata=90; // must be unique

class TRawDataItemType: public TMultiFieldItemType
{
  typedef TMultiFieldItemType inherited;
  friend class TMimeDirProfileHandler;
public:
  // constructor
  TRawDataItemType(
    TSyncSession *aSessionP,
    TDataTypeConfig *aTypeConfigP,
    const char *aCTType,
    const char *aVerCT,
    TSyncDataStore *aRelatedDatastoreP,
    TFieldListConfig *aFieldDefinitions // field definitions
  );
  // destructor
  virtual ~TRawDataItemType();
  // access to type
  virtual uInt16 getTypeID(void) const { return ity_rawdata; };
  virtual bool isBasedOn(uInt16 aItemTypeID) const { return aItemTypeID==ity_rawdata ? true : inherited::isBasedOn(aItemTypeID); };
  // differentiation between implemented and just descriptive TSyncTypeItems
  virtual bool isImplemented(void) { return true; }; // RawItem is an implementati on
  // helper to create same-typed instance via base class
  // MUST BE IMPLEMENTED IN ALL DERIVED CLASSES!
  virtual TSyncItemType *newCopyForSameType(
    TSyncSession *aSessionP,     // the session
    TSyncDataStore *aDatastoreP  // the datastore
  );
protected:
  // CTCap parsing/generation
  // - analyze CTCap for specific type
  virtual bool analyzeCTCap(SmlDevInfCTCapPtr_t aCTCapP);
  /// @brief copy CTCap derived info from another SyncItemType
  virtual bool copyCTCapInfoFrom(TSyncItemType &aSourceItemP);
  // - obtain property list for type, returns NULL if none available
  virtual SmlDevInfCTDataPropListPtr_t newCTDataPropList(TTypeVariantDescriptor aVariantDescriptor);
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
  // convenience casted pointer to my config
  TRawDataTypeConfig *fCfgP;
}; // TRawDataItemType



} // namespace sysync

#endif  // RawDataItemType_H

// eof

