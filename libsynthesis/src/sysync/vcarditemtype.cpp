/*
 *  File:         VCardItemType.cpp
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

// includes
#include "prefix_file.h"

#include "sysync.h"
#include "vcarditemtype.h"


using namespace sysync;


/* version info table */
const struct {
  sInt32 profilemode;
  const char* typetext;
  const char* versiontext;
} VCardVersionInfo[numVCardVersions-1] = {
  // vCard 2.1
  { PROFILEMODE_OLD, "text/x-vcard","2.1" },
  // vCard 3.0
  { PROFILEMODE_MIMEDIR, "text/vcard","3.0" }
};


// available vCard versions
const char * const vCardVersionNames[numVCardVersions] = {
  "2.1",
  "3.0",
  "none"
};


// VCard config

// init defaults
void TVCardTypeConfig::clear(void)
{
  // clear properties
  fVCardVersion=vcard_vers_none;
  // clear inherited
  inherited::clear();
} // TVCardTypeConfig::clear


// create Sync Item Type of appropriate type from config
TSyncItemType *TVCardTypeConfig::newSyncItemType(TSyncSession *aSessionP, TSyncDataStore *aDatastoreP)
{
  return
    new TVCardItemType(
      aSessionP,
      this,
      fTypeName.c_str(),
      fTypeVersion.c_str(),
      aDatastoreP,
      fMIMEProfileP->fFieldListP
    );
} // TVCardTypeConfig::newSyncItemType


// resolve (note: needed even if not configurable!)
void TVCardTypeConfig::localResolve(bool aLastPass)
{
  // pre-set profile mode if a predefined vCard mode is selected
  if (fVCardVersion!=vcard_vers_none) {
    fProfileMode=VCardVersionInfo[fVCardVersion].profilemode;
    // also set these, but getTypeName()/getTypeVers() do not use them (but required for syntax check)
    fTypeName = VCardVersionInfo[fVCardVersion].typetext;
    fTypeVersion = VCardVersionInfo[fVCardVersion].versiontext;
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TVCardTypeConfig::localResolve


#ifdef CONFIGURABLE_TYPE_SUPPORT

// config element parsing
bool TVCardTypeConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"version")==0)
    expectEnum(sizeof(fVCardVersion),&fVCardVersion,vCardVersionNames,numVCardVersions);
  else
    return TMIMEDirTypeConfig::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TVCardTypeConfig::localStartElement

#endif




/*
 * Implementation of TVCardItemType
 */


// private helper
TVCardVersion TVCardItemType::getVCardVersionByMode(sInt32 aMode)
{
  if (aMode!=PROFILEMODE_DEFAULT) {
    for (int v=vcard_vers_2_1; v<numVCardVersions-1; v++) {
      if (VCardVersionInfo[v].profilemode==aMode)
        return (TVCardVersion)v;
    }
  }
  // not found or default mode
  return fVCardVersion; // return the configured version
} // TVCardItemType::getVCalenderVersionByMode


// - get type name / vers
cAppCharP TVCardItemType::getTypeName(sInt32 aMode)
{
  TVCardVersion v = getVCardVersionByMode(aMode);
  if (v!=vcard_vers_none)
    return VCardVersionInfo[v].typetext;
  else
    return inherited::getTypeName(aMode);
} // TVCardItemType::getTypeName


cAppCharP TVCardItemType::getTypeVers(sInt32 aMode)
{
  TVCardVersion v = getVCardVersionByMode(aMode);
  if (v!=vcard_vers_none)
    return VCardVersionInfo[v].versiontext;
  else
    return inherited::getTypeVers(aMode);
} // TVCardItemType::getTypeVers



// relaxed type comparison, taking into account common errors in real-world implementations
bool TVCardItemType::supportsType(const char *aName, const char *aVers, bool aVersMustMatch)
{
  bool match = inherited::supportsType(aName,aVers,aVersMustMatch);
  if (!match) {
    // no exact match, but also check for unambiguos misstyped variants
    if (aVers && strstr(aName,"vcard")!=NULL) {
      // if "vcard" is in type name, version alone is sufficient to identify the vcard version,
      // even if the type string is wrong, like mixing "x-vcard" and "vcard"
      match = strucmp(getTypeVers(),aVers)==0;
      if (match) {
        PDEBUGPRINTFX(DBG_ERROR,(
          "Warning: fuzzy type match: probably misspelt %s version %s detected as correct type %s version %s",
          aName,aVers,
          getTypeName(),getTypeVers()
        ));
      }
    }
  }
  // return result now
  return match;
} // TVCardItemType::supportsType


// try to extract a version string from actual item data, NULL if none
bool TVCardItemType::versionFromData(SmlItemPtr_t aItemP, string &aString)
{
  // try to extract version
  return parseForProperty(aItemP,"VERSION",aString);
} // TVCardItemType::versionFromData


#ifdef APP_CAN_EXPIRE

// try to extract a version string from actual item data, NULL if none
sInt32 TVCardItemType::expiryFromData(SmlItemPtr_t aItemP, lineardate_t &aDat)
{
  string rev;
  aDat=0; // default to no date
  // try to extract version
  if (!parseForProperty(aItemP,"REV",rev)) return 0; // no REV, is ok
  lineartime_t t;
  timecontext_t tctx;
  if (ISO8601StrToTimestamp(rev.c_str(),t,tctx)==0) return 0; // bad format, is ok;
  aDat=t/linearDateToTimeFactor; // mod date
  #ifdef EXPIRES_AFTER_DATE
  sInt16 y,mo,d;
  lineardate2date(aDat,&y,&mo,&d);
  // check if this is after expiry date
  long v=SCRAMBLED_EXPIRY_VALUE;
  v =
    (y-1720)*12*42+
    (mo-1)*42+
    (d+7);
  v=v-SCRAMBLED_EXPIRY_VALUE;
  return (v>0 ? v : 0);
  #else
  return 0; // not expired
  #endif
} // TVCardItemType::expiryFromData

#endif // APP_CAN_EXPIRE


#ifdef OBJECT_FILTERING

// get field index of given filter expression identifier.
sInt16 TVCardItemType::getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex)
{
  // check if explicit field level identifier
  if (strucmp(aIdentifier,"F.",2)==0) {
    // explicit field identifier, skip property lookup
    return TMultiFieldItemType::getFilterIdentifierFieldIndex(aIdentifier+2,aIndex);
  }
  else {
    // translate SyncML-defined abstracts
    if (strucmp(aIdentifier,"FAMILY")==0) {
      TPropertyDefinition *propP = fProfileHandlerP->getProfileDefinition()->getPropertyDef("N");
      if (propP) {
        // value with convdef index 0 is family (last) name
        if (propP->numValues>0)
          return propP->convdefs[0].fieldid;
      }
      return VARIDX_UNDEFINED;
    }
    else if (strucmp(aIdentifier,"GIVEN")==0) {
      TPropertyDefinition *propP = fProfileHandlerP->getProfileDefinition()->getPropertyDef("N");
      if (propP) {
        // value with convdef index 1 is given (first) name
        if (propP->numValues>1)
          return propP->convdefs[1].fieldid;
      }
      return VARIDX_UNDEFINED;
    }
    else if (strucmp(aIdentifier,"GROUP")==0) {
      return inherited::getFilterIdentifierFieldIndex("CATEGORIES",0);
    }
  }
  // simply search for matching property names
  return inherited::getFilterIdentifierFieldIndex(aIdentifier,aIndex);
} // TVCardItemType::getFilterIdentifierFieldIndex


void TVCardItemType::addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps, TTypeVariantDescriptor aVariantDesc)
{
  // add my own properties
  addPCDataStringToList("GROUP", &aFilterKeywords);
  // add basics
  inherited::addFilterCapPropsAndKeywords(aFilterKeywords,aFilterProps,aVariantDesc);
} // TVCardItemType::addFilterCapPropsAndKeywords



#endif


// helper to create same-typed instance via base class
TSyncItemType *TVCardItemType::newCopyForSameType(
  TSyncSession *aSessionP,     // the session
  TSyncDataStore *aDatastoreP  // the datastore
)
{
  // create new itemtype of appropriate derived class type that can handle
  // this type
  MP_RETURN_NEW(TVCardItemType,DBG_OBJINST,"TVCardItemType",TVCardItemType(
    aSessionP,
    fTypeConfigP,
    getTypeName(),
    getTypeVers(),
    aDatastoreP,
    fFieldDefinitionsP
  ));
} // TVCardItemType::newCopyForSameType




/* end of TVCardItemType implementation */

// eof
