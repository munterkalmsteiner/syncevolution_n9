/*
 *  File:         VCalendarItemType.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TVCalendarItemType
 *    vCalendar item type, based on MIME-DIR Item Type, uses
 *    TMultiFieldItem as data item.
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-09-25 : luz : created
 *
 */

#ifndef VCalendarItemType_H
#define VCalendarItemType_H

// includes
#include "mimediritemtype.h"

using namespace sysync;

namespace sysync {

// vCalendar variants
typedef enum {
  vcalendar_vers_1_0,
  vcalendar_vers_2_0,
  vcalendar_vers_none,
  numVCalendarVersions
} TVCalendarVersion;


// Vcalendar based datatype
class TVCalendarTypeConfig : public TMIMEDirTypeConfig
{
  typedef TMIMEDirTypeConfig inherited;
public:
  TVCalendarTypeConfig(const char *aElementName, TConfigElement *aParentElementP) :
    TMIMEDirTypeConfig(aElementName,aParentElementP) {};
  // properties
  TVCalendarVersion fVCalendarVersion;
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
}; // TVCalendarTypeConfig


const uInt16 ity_vcalendar=103; // must be unique

class TVCalendarItemType: public TMimeDirItemType
{
  typedef TMimeDirItemType inherited;
public:
  // constructor
  TVCalendarItemType(
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
  ) { fVCalendarVersion = static_cast<TVCalendarTypeConfig *>(aTypeConfigP)->fVCalendarVersion; };
  // destructor
  virtual ~TVCalendarItemType() {};
  // access to type
  virtual uInt16 getTypeID(void) const { return ity_vcalendar; };
  virtual bool isBasedOn(uInt16 aItemTypeID) const { return aItemTypeID==ity_vcalendar ? true : TMimeDirItemType::isBasedOn(aItemTypeID); };
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
  // find field index for filter identifier
  virtual sInt16 getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex);
  #endif
private:
  TVCalendarVersion getVCalenderVersionByMode(sInt32 aMode);
  // vCard version
  TVCalendarVersion fVCalendarVersion;
}; // TVCalendarItemType


} // namespace sysync

#endif  // VCalendarItemType_H

// eof
