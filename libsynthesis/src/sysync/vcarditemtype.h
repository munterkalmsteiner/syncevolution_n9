/*
 *  File:         VCardItemType.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TVCardItemType
 *    vCard item type, based on MIME-DIR Item Type, uses
 *    TMultiFieldItem as data item.
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-08-20 : luz : created
 *
 */

#ifndef VCardItemType_H
#define VCardItemType_H

// includes
#include "mimediritemtype.h"



using namespace sysync;

namespace sysync {


// vCard variants
typedef enum {
  vcard_vers_2_1,
  vcard_vers_3_0,
  vcard_vers_none, // must be last as version table has no entry for that
  numVCardVersions
} TVCardVersion;



// Vcard based datatype
class TVCardTypeConfig : public TMIMEDirTypeConfig
{
  typedef TMIMEDirTypeConfig inherited;
public:
  TVCardTypeConfig(const char *aElementName, TConfigElement *aParentElementP) :
    TMIMEDirTypeConfig(aElementName,aParentElementP) {};
  // properties
  TVCardVersion fVCardVersion;
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
}; // TVCardTypeConfig


const uInt16 ity_vcard=104; // must be unique

class TVCardItemType: public TMimeDirItemType
{
  typedef TMimeDirItemType inherited;
public:
  // constructor
  TVCardItemType(
    TSyncSession *aSessionP,
    TDataTypeConfig *aTypeConfigP,
    const char *aCTType,
    const char *aVerCT,
    TSyncDataStore *aRelatedDatastoreP,
    TFieldListConfig *aFieldDefinitions // field definitions
  ) : TMimeDirItemType(
    aSessionP,
    aTypeConfigP,
    aCTType,
    aVerCT,
    aRelatedDatastoreP,
    aFieldDefinitions
  ) { fVCardVersion = static_cast<TVCardTypeConfig *>(aTypeConfigP)->fVCardVersion; };
  // destructor
  virtual ~TVCardItemType() {};
  // access to type
  virtual uInt16 getTypeID(void) const { return ity_vcard; };
  virtual bool isBasedOn(uInt16 aItemTypeID) const { return aItemTypeID==ity_vcard ? true : TMimeDirItemType::isBasedOn(aItemTypeID); };
  // get type name / vers
  virtual cAppCharP getTypeName(sInt32 aMode=0);
  virtual cAppCharP getTypeVers(sInt32 aMode=0);
  // differentiation between implemented and just descriptive TSyncTypeItems
  virtual bool isImplemented(void) { return true; }; // vCard is an implemented data type
  // relaxed type comparison, taking into account common errors in real-world implementations
  virtual bool supportsType(const char *aName, const char *aVers, bool aVersMustMatch=false);
  // try to extract a version string from actual item data, false if none
  virtual bool versionFromData(SmlItemPtr_t aItemP, string &aString);
  #ifdef APP_CAN_EXPIRE
  // test if modified date of item is expired
  virtual sInt32 expiryFromData(SmlItemPtr_t aItemP, lineardate_t &aDat);
  #endif
  // helper to create same-typed instance via base class
  // MUST BE IMPLEMENTED IN ALL DERIVED CLASSES!
  virtual TSyncItemType *newCopyForSameType(
    TSyncSession *aSessionP,     // the session
    TSyncDataStore *aDatastoreP  // the datastore
  );
  #ifdef OBJECT_FILTERING
  // add extra keywords and property names to filterCap
  virtual void addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps, TTypeVariantDescriptor aVariantDesc);
  // find field index for filter identifier
  virtual sInt16 getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex);
  #endif
private:
  TVCardVersion getVCardVersionByMode(sInt32 aMode);
  // vCard version
  TVCardVersion fVCardVersion;
}; // TVCardItemType


} // namespace sysync

#endif  // VCardItemType_H

// eof
