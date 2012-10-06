/*
 *  File:         VCalendarItemType.cpp
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

// includes
#include "prefix_file.h"

#include "sysync.h"
#include "vcalendaritemtype.h"

#include "rrules.h"

using namespace sysync;

namespace sysync {

/* version info table */
const struct {
  sInt32 profilemode;
  const char* typetext;
  const char* versiontext;
} VCalendarVersionInfo[numVCalendarVersions-1] = {
  // vCalendar 1.0
  { PROFILEMODE_OLD, "text/x-vcalendar","1.0" },
  // iCalendar 2.0
  { PROFILEMODE_MIMEDIR, "text/calendar","2.0" }
};


// available vCard versions
const char * const vCalendarVersionNames[numVCalendarVersions] = {
  "1.0",
  "2.0",
  "none"
};


// VCalendar config

// init defaults
void TVCalendarTypeConfig::clear(void)
{
  // clear properties
  fVCalendarVersion=vcalendar_vers_none;
  // clear inherited
  inherited::clear();
} // TVCalendarTypeConfig::clear


// create Sync Item Type of appropriate type from config
TSyncItemType *TVCalendarTypeConfig::newSyncItemType(TSyncSession *aSessionP, TSyncDataStore *aDatastoreP)
{
  return
    new TVCalendarItemType(
      aSessionP,
      this,
      fTypeName.c_str(),
      fTypeVersion.c_str(),
      aDatastoreP,
      fMIMEProfileP->fFieldListP
    );
} // TVCalendarTypeConfig::newSyncItemType


// resolve (note: needed even if not configurable!)
void TVCalendarTypeConfig::localResolve(bool aLastPass)
{
  // pre-set profile mode if a predefined vCard mode is selected
  if (fVCalendarVersion!=vcalendar_vers_none) {
    fProfileMode = VCalendarVersionInfo[fVCalendarVersion].profilemode;
    // also set these, but getTypeName()/getTypeVers() do not use them (but required for syntax check)
    fTypeName = VCalendarVersionInfo[fVCalendarVersion].typetext;
    fTypeVersion = VCalendarVersionInfo[fVCalendarVersion].versiontext;
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TVCalendarTypeConfig::localResolve


#ifdef CONFIGURABLE_TYPE_SUPPORT


// config element parsing
bool TVCalendarTypeConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"version")==0)
    expectEnum(sizeof(fVCalendarVersion),&fVCalendarVersion,vCalendarVersionNames,numVCalendarVersions);
  else
    return TMIMEDirTypeConfig::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TVCalendarTypeConfig::localStartElement


#endif


/*
 * Implementation of TVCalendarItemType
 */

/* public TVCalendarItemType members */


// private helper
TVCalendarVersion TVCalendarItemType::getVCalenderVersionByMode(sInt32 aMode)
{
  if (aMode!=PROFILEMODE_DEFAULT) {
    for (int v=vcalendar_vers_1_0; v<numVCalendarVersions-1; v++) {
      if (VCalendarVersionInfo[v].profilemode==aMode)
        return (TVCalendarVersion)v;
    }
  }
  // not found or default mode
  return fVCalendarVersion; // return the configured version
} // TVCalendarItemType::getVCalenderVersionByMode


// - get type name / vers
cAppCharP TVCalendarItemType::getTypeName(sInt32 aMode)
{
  TVCalendarVersion v = getVCalenderVersionByMode(aMode);
  if (v!=vcalendar_vers_none)
    return VCalendarVersionInfo[v].typetext;
  else
    return inherited::getTypeName(aMode);
} // TVCalendarItemType::getTypeName


cAppCharP TVCalendarItemType::getTypeVers(sInt32 aMode)
{
  TVCalendarVersion v = getVCalenderVersionByMode(aMode);
  if (v!=vcalendar_vers_none)
    return VCalendarVersionInfo[v].versiontext;
  else
    return inherited::getTypeVers(aMode);
} // TVCalendarItemType::getTypeVers


// relaxed type comparison, taking into account common errors in real-world implementations
bool TVCalendarItemType::supportsType(const char *aName, const char *aVers, bool aVersMustMatch)
{
  bool match = inherited::supportsType(aName,aVers,aVersMustMatch);
  if (!match) {
    // no exact match, but also check for unambiguos misstyped variants
    if (aVers && strstr(aName,"calendar")!=NULL) {
      // if "calendar" is in type name, version alone is sufficient to identify the vCalendar version,
      // even if the type string is wrong, like mixing "x-vcalendar" and "calendar"
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
} // TVCalendarItemType::supportsType


// try to extract a version string from actual item data, NULL if none
bool TVCalendarItemType::versionFromData(SmlItemPtr_t aItemP, string &aString)
{
  // try to extract version
  return parseForProperty(aItemP,"VERSION",aString);
}


#ifdef APP_CAN_EXPIRE

// try to extract a version string from actual item data, NULL if none
sInt32 TVCalendarItemType::expiryFromData(SmlItemPtr_t aItemP, lineardate_t &aDat)
{
  string rev;
  aDat=0; // default to no date
  // try to extract version
  if (!parseForProperty(aItemP,"LAST-MODIFIED",rev)) return 0; // no date, is ok
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
} // TVCalendarItemType::expiryFromData

#endif // APP_CAN_EXPIRE


#ifdef OBJECT_FILTERING

// get field index of given filter expression identifier.
sInt16 TVCalendarItemType::getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex)
{
  // check if explicit field level identifier
  if (strucmp(aIdentifier,"F.",2)==0) {
    // explicit field identifier, skip property lookup
    return TMultiFieldItemType::getFilterIdentifierFieldIndex(aIdentifier+2,aIndex);
  }
  else {
    // translate SyncML-defined abstracts
    if (strucmp(aIdentifier,"START")==0)
      return inherited::getFilterIdentifierFieldIndex("DTSTART",0);
    else if (strucmp(aIdentifier,"END")==0)
      return inherited::getFilterIdentifierFieldIndex("DTEND",0);
  }
  // simply search for matching property names
  return inherited::getFilterIdentifierFieldIndex(aIdentifier,aIndex);
} // TVCalendarItemType::getFilterIdentifierFieldIndex

#endif


// helper to create same-typed instance via base class
TSyncItemType *TVCalendarItemType::newCopyForSameType(
  TSyncSession *aSessionP,     // the session
  TSyncDataStore *aDatastoreP  // the datastore
)
{
  // create new itemtype of appropriate derived class type that can handle
  // this type
  MP_RETURN_NEW(TVCalendarItemType,DBG_OBJINST,"TVCalendarItemType",TVCalendarItemType(
    aSessionP,
    fTypeConfigP,
    getTypeName(),
    getTypeVers(),
    aDatastoreP,
    fFieldDefinitionsP
  ));
} // TVCalendarItemType::newCopyForSameType


} // namespace sysync

/* end of TVCalendarItemType implementation */

// eof
