/*
 *  File:         TextItemType.cpp
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

// includes
#include "prefix_file.h"

#include "sysync.h"
#include "textitemtype.h"


using namespace sysync;


// Config
// ======


// text-based datatype config

TTextTypeConfig::TTextTypeConfig(const char* aName, TConfigElement *aParentElement) :
  TMultiFieldTypeConfig(aName,aParentElement)
{
  clear();
} // TTextTypeConfig::TTextTypeConfig


TTextTypeConfig::~TTextTypeConfig()
{
  // make sure everything is deleted (was missing long time and caused mem leaks!)
  clear();
} // TTextTypeConfig::~TTextTypeConfig


// init defaults
void TTextTypeConfig::clear(void)
{
  // clear inherited
  inherited::clear();
} // TTextTypeConfig::clear


// create Sync Item Type of appropriate type from config
TSyncItemType *TTextTypeConfig::newSyncItemType(TSyncSession *aSessionP, TSyncDataStore *aDatastoreP)
{
  if (!fFieldListP)
    SYSYNC_THROW(TSyncException(DEBUGTEXT("TTextTypeConfig::newSyncItemType: no fFieldListP","txit1")));
  return
    new TTextItemType(
      aSessionP,
      this,
      fTypeName.c_str(),
      fTypeVersion.c_str(),
      aDatastoreP,
      fFieldListP
    );
} // TTextTypeConfig::newSyncItemType


#ifdef CONFIGURABLE_TYPE_SUPPORT


// config element parsing
bool TTextTypeConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // For >2.1 engines, the text item does not have linemaps etc. in the datatype, but uses
  // profiles that can be shared among datatypes. To maintain compatibility with old config
  // files, we implicitly create and link a profile if we see profile definions at this
  // level
  // - first, check textItem specifics
  // %%% none
  // - then, check ancestor
  if (TMultiFieldTypeConfig::localStartElement(aElementName,aAttributes,aLine))
    return true; // ancestor could parse the tag
  else {
    // ancestor did not know the tag, could be implicit profile
    if (!fProfileConfigP) {
      // there is no profile config yet, create an implicit one
      // - create an implicit profile with the same name as the datatype itself
      if (!fFieldListP)
        return fail("%s must have a 'use' tag before referencing fields");
      TMultiFieldDatatypesConfig *mfdtcfgP = static_cast<TMultiFieldDatatypesConfig *>(getParentElement());
      fProfileConfigP=new TTextProfileConfig(getName(),mfdtcfgP);
      if (fProfileConfigP) {
        fProfileConfigP->fFieldListP = fFieldListP; // assign field list
        mfdtcfgP->fProfiles.push_back(fProfileConfigP); // add to profiles
      }
      // - warn
      ReportError(false,
        "Warning: old-style config - '%s' directly in datatype '%s', should be moved to textprofile",
        aElementName,
        getName()
      );
    }
    // now try to parse
    if (fProfileConfigP) {
      // let the profile parse as if it was inside a "textprofile"
      return delegateParsingTo(fProfileConfigP,aElementName,aAttributes,aLine);
    }
    else
      return false; // cannot parse
  }
} // TTextTypeConfig::localStartElement


// resolve
void TTextTypeConfig::localResolve(bool aLastPass)
{
  // check correct type of profile
  if (DYN_CAST<TTextProfileConfig *>(fProfileConfigP)==NULL)
    SYSYNC_THROW(TConfigParseException("missing 'use' of a 'textprofile' in datatype"));
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TTextTypeConfig::localResolve

#endif


/*
 * Implementation of TTextItemType
 */


TTextItemType::TTextItemType(
  TSyncSession *aSessionP,
  TDataTypeConfig *aTypeCfgP, // type config
  const char *aCTType,
  const char *aVerCT,
  TSyncDataStore *aRelatedDatastoreP,
  TFieldListConfig *aFieldDefinitions // field definitions
) :
  TMultiFieldItemType(aSessionP,aTypeCfgP,aCTType,aVerCT,aRelatedDatastoreP,aFieldDefinitions),
  fProfileHandlerP(NULL)
{
  // save typed config pointer again
  fTypeCfgP = static_cast<TTextTypeConfig *>(aTypeCfgP);
  // create the profile handler
  fProfileHandlerP = static_cast<TTextTypeConfig *>(aTypeCfgP)->fProfileConfigP->newProfileHandler(this);
  // set profile mode
  fProfileHandlerP->setProfileMode(fTypeCfgP->fProfileMode);
} // TTextItemType::TTextItemType


TTextItemType::~TTextItemType()
{
  if (fProfileHandlerP)
    delete fProfileHandlerP;
} // TTextItemType::~TTextItemType

cAppCharP TTextItemType::getTypeVers(sInt32 aMode)
{
  // This function is called by TMimeDirProfile when generating the
  // VERSION property. Allow converting a plain text item to
  // iCalendar 2.0 (aMode = 2) or vCalendar 1.0 (aMode = 1) by
  // overriding the base version that was configured for the
  // underlying text item type.
  switch (aMode) {
  default:
    return inherited::getTypeVers(aMode);
  case 1:
    return "1.0";
  case 2:
    return "2.0";
  }
} // TTextItemType::getTypeVers


#ifdef OBJECT_FILTERING

// get field index of given filter expression identifier.
sInt16 TTextItemType::getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex)
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
} // TTextItemType::getFilterIdentifierFieldIndex

#endif


// helper to create same-typed instance via base class
TSyncItemType *TTextItemType::newCopyForSameType(
  TSyncSession *aSessionP,     // the session
  TSyncDataStore *aDatastoreP  // the datastore
)
{
  // create new itemtype of appropriate derived class type that can handle
  // this type
  MP_RETURN_NEW(TTextItemType,DBG_OBJINST,"TTextItemType",TTextItemType(
    aSessionP,
    fTypeConfigP,
    getTypeName(),
    getTypeVers(),
    aDatastoreP,
    fFieldDefinitionsP
  ));
} // TTextItemType::newCopyForSameType


// create new sync item of proper type and optimization for specified target
TSyncItem *TTextItemType::internalNewSyncItem(TSyncItemType *aTargetItemTypeP, TLocalEngineDS *aLocalDatastoreP)
{
  // All TextItems are stored in MultiFieldItems
  if (!aTargetItemTypeP->isBasedOn(ity_multifield))
    SYSYNC_THROW(TSyncException(DEBUGTEXT("TTextItemType::internalNewSyncItem with bad-typed target","txit3")));
  TMultiFieldItemType *targetitemtypeP =
    static_cast<TMultiFieldItemType *> (aTargetItemTypeP);
  return new TMultiFieldItem(this,targetitemtypeP);
} // TTextItemType::internalNewSyncItem


// fill in SyncML data (but leaves IDs empty)
bool TTextItemType::internalFillInData(
  TSyncItem *aSyncItemP,      // SyncItem to be filled with data
  SmlItemPtr_t aItemP,        // SyncML toolkit item Data to be converted into SyncItem (may be NULL if no data, in case of Delete or Map)
  TLocalEngineDS *aLocalDatastoreP, // local datastore
  TStatusCommand &aStatusCmd  // status command that might be modified in case of error
)
{
  // check type
  if (!aSyncItemP->isBasedOn(ity_multifield) || !fProfileHandlerP)
    SYSYNC_THROW(TSyncException(DEBUGTEXT("TTextItemType::internalFillInData: incompatible item class","txit4")));
  TMultiFieldItem *itemP = static_cast<TMultiFieldItem *> (aSyncItemP);
  // process data if any
  if (aItemP->data) {
    // set related datastore so handler can access session specific datastore state
    fProfileHandlerP->setRelatedDatastore(aLocalDatastoreP);
    // for text items, the profile handler does all the parsing
    stringSize sz;
    cAppCharP t = smlPCDataToCharP(aItemP->data,&sz);
    if (!fProfileHandlerP->parseText(t,sz,*itemP)) {
      // format error
      aStatusCmd.setStatusCode(415); // Unsupported media type or format
      ADDDEBUGITEM(aStatusCmd,"Error parsing Text content");
      return false;
    }
  }
  else {
    // no data
    aStatusCmd.setStatusCode(412); // incomplete command
    ADDDEBUGITEM(aStatusCmd,"No data found in item");
    return false;
  }
  // ok, let ancestor process data as well
  return TMultiFieldItemType::internalFillInData(aSyncItemP,aItemP,aLocalDatastoreP,aStatusCmd);
} // TTextItemType::internalFillInData


// sets data and meta from SyncItem data, but leaves source & target untouched
bool TTextItemType::internalSetItemData(
  TSyncItem *aSyncItemP,  // the syncitem to be represented as SyncML
  SmlItemPtr_t aItem,     // item with NULL meta and NULL data
  TLocalEngineDS *aLocalDatastoreP // local datastore
)
{
  // check type
  if (!aSyncItemP->isBasedOn(ity_multifield) || !fProfileHandlerP)
    SYSYNC_THROW(TSyncException(DEBUGTEXT("TTextItemType::internalSetItemData: incompatible item class","txit4")));
  TMultiFieldItem *itemP = static_cast<TMultiFieldItem *> (aSyncItemP);
  // let ancestor prepare first
  if (!TMultiFieldItemType::internalSetItemData(aSyncItemP,aItem,aLocalDatastoreP)) return false;
  // set related datastore so handler can access session specific datastore state
  fProfileHandlerP->setRelatedDatastore(aLocalDatastoreP);
  // generate data item
  string dataitem;
  fProfileHandlerP->generateText(*itemP,dataitem);
  // put data item into opaque/cdata PCData (note that dataitem could be BINARY string, so we need to pass size!)
  aItem->data=newPCDataStringX((const uInt8 *)dataitem.c_str(),true,dataitem.size());
  // can't go wrong
  return true;
} // TTextItemType::internalSetItemData


// generates SyncML-Devinf property list for type
SmlDevInfCTDataPropListPtr_t TTextItemType::newCTDataPropList(TTypeVariantDescriptor aVariantDescriptor)
{
  // no properties here
  return NULL;
} // TTextItemType::newCTDataPropList


// Filtering: add keywords and property names to filterCap
void TTextItemType::addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps, TTypeVariantDescriptor aVariantDescriptor)
{
  #ifdef OBJECT_FILTERING
  // let profile add the keywords
  if (fProfileHandlerP) {
    fProfileHandlerP->addFilterCapPropsAndKeywords(aFilterKeywords, aFilterProps, aVariantDescriptor, this);
  }
  // let base class add own keywords/props
  inherited::addFilterCapPropsAndKeywords(aFilterKeywords, aFilterProps, aVariantDescriptor);
  #endif
} // TTextItemType::addFilterCapPropsAndKeywords



// intended for creating SyncItemTypes for remote databases from
// transmitted DevInf.
// SyncItemType MUST NOT take ownership of devinf structure passed
// (because multiple types might be created from a single CTCap entry)
bool TTextItemType::analyzeCTCap(SmlDevInfCTCapPtr_t aCTCapP)
{
  // just let parent handle
  return inherited::analyzeCTCap(aCTCapP);
} // TTextItemType::analyzeCTCap


/// @brief copy CTCap derived info from another SyncItemType
/// @return false if item not compatible
/// @note required to create remote type variants from ruleMatch type alternatives
bool TTextItemType::copyCTCapInfoFrom(TSyncItemType &aSourceItem)
{
  // just let parent handle
  return inherited::copyCTCapInfoFrom(aSourceItem);
} // TTextItemType::copyCTCapInfoFrom




/* end of TTextItemType implementation */

// eof
