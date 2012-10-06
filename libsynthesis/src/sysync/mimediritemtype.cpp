/*
 *  File:         mimediritemtype.cpp
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

// includes
#include "prefix_file.h"
#include "sysync.h"
#include "mimediritemtype.h"
#include "vtimezone.h"

using namespace sysync;


namespace sysync {

// mime-DIR mode names
const char * const mimeModeNames[numMimeModes] = {
  "old",
  "standard"
};


// enumeration modes
const char * const EnumModeNames[numEnumModes] = {
  "translate",        // translation from value to name and vice versa
  "prefix",           // translation of prefix while copying rest of string
  "defaultname",      // default name when translating from value to name
  "defaultvalue",     // default value when translating from name to value
  "ignore"            // ignore value or name
};


// profile modes
const char * const ProfileModeNames[numProfileModes] = {
  "custom",        // custom profile
  "vtimezones",    // VTIMEZONE profile(s), expands to a VTIMEZONE for every time zone referenced by convmode TZID fields
};


// Config
// ======


// MIMEDir-based datatype config

TMIMEDirTypeConfig::TMIMEDirTypeConfig(const char* aName, TConfigElement *aParentElement) :
  TMultiFieldTypeConfig(aName,aParentElement)
{
  clear();
} // TMIMEDirTypeConfig::TMIMEDirTypeConfig


TMIMEDirTypeConfig::~TMIMEDirTypeConfig()
{
  clear();
} // TMIMEDirTypeConfig::~TMIMEDirTypeConfig


// init defaults
void TMIMEDirTypeConfig::clear(void)
{
  // clear inherited
  inherited::clear();
} // TMIMEDirTypeConfig::clear


// init defaults
void TMIMEDirTypeConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    // make a casted copy of the profile pointer
    GET_CASTED_PTR(fMIMEProfileP,TMIMEProfileConfig,fProfileConfigP,DEBUGTEXT("TMIMEDirTypeConfig with non-TMIMEProfileConfig profile","mdit2"));
    #ifdef CONFIGURABLE_TYPE_SUPPORT
    if (!fMIMEProfileP)
      SYSYNC_THROW(TConfigParseException("no 'mimeprofile' found"));
    #endif
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TMIMEDirTypeConfig::localResolve



// create Sync Item Type of appropriate type from config
TSyncItemType *TMIMEDirTypeConfig::newSyncItemType(TSyncSession *aSessionP, TSyncDataStore *aDatastoreP)
{
  if (!fMIMEProfileP)
    SYSYNC_THROW(TSyncException(DEBUGTEXT("TMIMEDirTypeConfig: no or wrong-typed profile","mdit1")));
  return
    new TMimeDirItemType(
      aSessionP,
      this,
      fTypeName.c_str(),
      fTypeVersion.c_str(),
      aDatastoreP,
      fMIMEProfileP->fFieldListP
    );
} // TMIMEDirTypeConfig::newSyncItemType


// get a descriptor for selecting a variant of a datatype (if any), NULL=no variant with this name
TTypeVariantDescriptor TMIMEDirTypeConfig::getVariantDescriptor(const char *aVariantName)
{
  // variants in MIME-Dir are currently level 1 subprofiles
  // - we need it here already (we calculate it once again in Resolve)
  GET_CASTED_PTR(fMIMEProfileP,TMIMEProfileConfig,fProfileConfigP,DEBUGTEXT("TMIMEDirTypeConfig with non-TMIMEProfileConfig profile","mdit2"));
  const TProfileDefinition *subprofileP =
    fMIMEProfileP->fRootProfileP->subLevels;
  while (subprofileP) {
    if (strucmp(aVariantName,TCFG_CSTR(subprofileP->levelName))==0) {
      return (TTypeVariantDescriptor) subprofileP; // pointer to subProfile is our descriptor
    }
    // next
    subprofileP=subprofileP->next;
  }
  // not found
  return NULL;
} // TMIMEDirTypeConfig::getVariantDescriptor


#ifdef CONFIGURABLE_TYPE_SUPPORT

// config element parsing
bool TMIMEDirTypeConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  /* use profilemode instead
  if (strucmp(aElementName,"mimedirmode")==0)
    expectEnum(sizeof(fMimeDirMode),&fMimeDirMode,mimeModeNames,numMimeModes);
  else
  */
  // - none known here
    return TMultiFieldTypeConfig::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TMIMEDirTypeConfig::localStartElement

#endif


#pragma exceptions reset
#undef EXCEPTIONS_HERE
#define EXCEPTIONS_HERE TARGET_HAS_EXCEPTIONS


/*
 * Implementation of TMimeDirItemType
 */


TMimeDirItemType::TMimeDirItemType(
  TSyncSession *aSessionP,
  TDataTypeConfig *aTypeConfigP,
  const char *aCTType,
  const char *aVerCT,
  TSyncDataStore *aRelatedDatastoreP,
  TFieldListConfig *aFieldDefinitions // field definitions
) :
  TMultiFieldItemType(aSessionP,aTypeConfigP,aCTType,aVerCT,aRelatedDatastoreP,aFieldDefinitions)
{
  // create the profile handler
  fProfileHandlerP = static_cast<TMimeDirProfileHandler *>(static_cast<TMIMEDirTypeConfig *>(aTypeConfigP)->fProfileConfigP->newProfileHandler(this));
  // set profile mode
  fProfileHandlerP->setProfileMode(static_cast<TMIMEDirTypeConfig *>(aTypeConfigP)->fProfileMode);
  fReceivedFieldDefs=false;
} // TMimeDirItemType::TMimeDirItemType


TMimeDirItemType::~TMimeDirItemType()
{
  // release the profile handler
  delete fProfileHandlerP;
} // TMimeDirItemType::~TMimeDirItemType


#ifdef OBJECT_FILTERING


// get field index of given filter expression identifier.
sInt16 TMimeDirItemType::getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex)
{
  // check if explicit field level identifier
  if (strucmp(aIdentifier,"F.",2)==0) {
    // explicit field identifier, skip property lookup
    aIdentifier+=2;
  }
  else if (fProfileHandlerP) {
    // let profile search for fields by profile-defined alternative names
    sInt16 fid = fProfileHandlerP->getFilterIdentifierFieldIndex(aIdentifier, aIndex);
    if (fid!=FID_NOT_SUPPORTED)
      return fid;
  }
  // if no field ID found so far, look up in field list
  return TMultiFieldItemType::getFilterIdentifierFieldIndex(aIdentifier, aIndex);
} // TMimeDirItemType::getFilterIdentifierFieldIndex


#endif



// create new sync item of proper type and optimization for specified target
TSyncItem *TMimeDirItemType::internalNewSyncItem(TSyncItemType *aTargetItemTypeP, TLocalEngineDS *aLocalDatastoreP)
{
  // All MimeDirs are stored in MultiFieldItems
  TMultiFieldItemType *targetitemtypeP;
  GET_CASTED_PTR(targetitemtypeP,TMultiFieldItemType,aTargetItemTypeP,DEBUGTEXT("TMimeDirItemType::internalNewSyncItem with bad-typed target","mdit6"));
  MP_RETURN_NEW(TMultiFieldItem,DBG_OBJINST,"TMultiFieldItem",TMultiFieldItem(this,targetitemtypeP));
} // TMimeDirItemType::internalNewSyncItem


// fill in SyncML data (but leaves IDs empty)
bool TMimeDirItemType::internalFillInData(
  TSyncItem *aSyncItemP,      // SyncItem to be filled with data
  SmlItemPtr_t aItemP,        // SyncML toolkit item Data to be converted into SyncItem (may be NULL if no data, in case of Delete or Map)
  TLocalEngineDS *aLocalDatastoreP, // local datastore
  TStatusCommand &aStatusCmd  // status command that might be modified in case of error
)
{
  // check type
  TMultiFieldItem *itemP;
  GET_CASTED_PTR(itemP,TMultiFieldItem,aSyncItemP,DEBUGTEXT("TMimeDirItemType::internalFillInData: incompatible item class","mdit7"));
  // process data if any
  if (aItemP->data) {
    // set related datastore so handler can access datastore/session specific datastore state
    fProfileHandlerP->setRelatedDatastore(aLocalDatastoreP);
    // parse data
    stringSize sz;
    cAppCharP t = smlPCDataToCharP(aItemP->data,&sz);
    if (!fProfileHandlerP->parseText(t,sz,*itemP)) {
      // format error
      aStatusCmd.setStatusCode(415); // Unsupported media type or format
      ADDDEBUGITEM(aStatusCmd,"Error parsing MIME-DIR content");
      return false;
    }
  }
  else {
    // no data
    aStatusCmd.setStatusCode(412); // incomplete command
    ADDDEBUGITEM(aStatusCmd,"No data found in item");
    return false;
  }
  // let ancestor process data as well
  return TMultiFieldItemType::internalFillInData(aSyncItemP,aItemP,aLocalDatastoreP,aStatusCmd);
} // TMimeDirItemType::internalFillInData


// sets data and meta from SyncItem data, but leaves source & target untouched
bool TMimeDirItemType::internalSetItemData(
  TSyncItem *aSyncItemP,  // the syncitem to be represented as SyncML
  SmlItemPtr_t aItem,     // item with NULL meta and NULL data
  TLocalEngineDS *aLocalDatastoreP // local datastore
)
{
  // check type
  TMultiFieldItem *itemP;
  GET_CASTED_PTR(itemP,TMultiFieldItem,aSyncItemP,DEBUGTEXT("TMimeDirItemType::internalSetItemData: incompatible item class","mdit8"));
  // let ancestor prepare first
  if (!TMultiFieldItemType::internalSetItemData(aSyncItemP,aItem,aLocalDatastoreP)) return false;
  // set related datastore so handler can access datastore/session specific datastore state
  fProfileHandlerP->setRelatedDatastore(aLocalDatastoreP);
  // generate data item
  string dataitem;
  fProfileHandlerP->generateText(*itemP,dataitem);
  // put data item into opaque/cdata PCData
  aItem->data=newPCDataStringX((const uInt8 *)dataitem.c_str(),true);
  // can't go wrong
  return true;
} // TMimeDirItemType::internalSetItemData




bool TMimeDirItemType::parseForProperty(SmlItemPtr_t aItemP, const char *aPropName, string &aString)
{
  if (aItemP && aItemP->data)
    return parseForProperty(smlPCDataToCharP(aItemP->data),aPropName,aString);
  else
    return false;
} // TMimeDirItemType::parseForProperty


// scan Data item for specific property (used for quick type tests)
bool TMimeDirItemType::parseForProperty(const char *aText, const char *aPropName, string &aString)
{
  return static_cast<TMimeDirProfileHandler *>(fProfileHandlerP)->parseForProperty(aText, aPropName, aString);
} // TMimeDirItemType::parseForProperty



// generates SyncML-Devinf property list for type
SmlDevInfCTDataPropListPtr_t TMimeDirItemType::newCTDataPropList(TTypeVariantDescriptor aVariantDescriptor)
{
  // let profile handle that
  return fProfileHandlerP->newCTDataPropList(aVariantDescriptor, this);
}


#ifdef OBJECT_FILTERING

// Filtering: add keywords and property names to filterCap
void TMimeDirItemType::addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps, TTypeVariantDescriptor aVariantDescriptor)
{
  fProfileHandlerP->addFilterCapPropsAndKeywords(aFilterKeywords, aFilterProps, aVariantDescriptor, this);
  // add basics
  inherited::addFilterCapPropsAndKeywords(aFilterKeywords,aFilterProps,aVariantDescriptor);
} // TMimeDirItemType::addFilterCapPropsAndKeywords

#endif // OBJECT_FILTERING


// Analyze CTCap part of devInf
bool TMimeDirItemType::analyzeCTCap(SmlDevInfCTCapPtr_t aCTCapP)
{
  if (TMultiFieldItemType::analyzeCTCap(aCTCapP)) {
    // let profile check
    fProfileHandlerP->analyzeCTCap(aCTCapP, this);
  }
  #ifdef SYDEBUG
  if (PDEBUGTEST(DBG_REMOTEINFO+DBG_DETAILS)) {
    // Show which fields are enabled
    PDEBUGPRINTFX(DBG_REMOTEINFO,("Field options after CTCap analyzing:"));
    string finfo;
    for (sInt16 i=0; i<fFieldDefinitionsP->numFields(); i++) {
      StringObjAppendPrintf(finfo,
        "- %-20s : %-12s maxoccur=%ld, maxsize=%ld %s%s\n",
        fFieldDefinitionsP->fFields[i].TCFG_CSTR(fieldname),
        getFieldOptions(i)->available ? "AVAILABLE" : "n/a",
        (long)getFieldOptions(i)->maxoccur,
        (long)getFieldOptions(i)->maxsize,
        getFieldOptions(i)->maxsize == FIELD_OPT_MAXSIZE_UNKNOWN ?
          "(limited, but unknown size)" :
          (getFieldOptions(i)->maxsize == FIELD_OPT_MAXSIZE_NONE ? "(unlimited)" : ""),
        getFieldOptions(i)->notruncate ? ", noTruncate" : ""
      );
    }
    PDEBUGPUTSXX(DBG_REMOTEINFO+DBG_DETAILS,finfo.c_str(),0,true);
  }
  #endif
  return true;
}



/// @brief helper to create same-typed instance via base class
TSyncItemType *TMimeDirItemType::newCopyForSameType(
  TSyncSession *aSessionP,     // the session
  TSyncDataStore *aDatastoreP  // the datastore
)
{
  // create new itemtype of appropriate derived class type that can handle
  // this type
  MP_RETURN_NEW(TMimeDirItemType,DBG_OBJINST,"TMimeDirItemType",TMimeDirItemType(
    aSessionP,
    fTypeConfigP,
    getTypeName(),
    getTypeVers(),
    aDatastoreP,
    fFieldDefinitionsP
  ));
} // TMimeDirItemType::newCopyForSameType


/// @brief copy CTCap derived info from another SyncItemType
/// @return false if item not compatible
/// @note required to create remote type variants from ruleMatch type alternatives
bool TMimeDirItemType::copyCTCapInfoFrom(TSyncItemType &aSourceItem)
{
  // must be same type as myself or based on the type of myself
  if (!aSourceItem.isBasedOn(getTypeID()))
    return false; // not compatible
  TMimeDirItemType *itemTypeP = static_cast<TMimeDirItemType *>(&aSourceItem);
  // copy the fieldDefs flag
  fReceivedFieldDefs = itemTypeP->fReceivedFieldDefs;
  // other CTCap info is in the field options of MultiFieldItemType
  return inherited::copyCTCapInfoFrom(aSourceItem);
} // TMimeDirItemType::copyCTCapInfoFrom


/* end of TMimeDirItemType implementation */

} // namespace sysync


// eof
