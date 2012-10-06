/*
 *  File:         rawdataitemtype.cpp
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


// includes
#include "prefix_file.h"
#include "sysync.h"
#include "rawdataitemtype.h"

using namespace sysync;


namespace sysync {


// Config
// ======


// MIMEDir-based datatype config

TRawDataTypeConfig::TRawDataTypeConfig(const char* aName, TConfigElement *aParentElement) :
  TMultiFieldTypeConfig(aName,aParentElement)
{
  clear();
} // TRawDataTypeConfig::TRawDataTypeConfig


TRawDataTypeConfig::~TRawDataTypeConfig()
{
  clear();
} // TRawDataTypeConfig::~TRawDataTypeConfig


// init defaults
void TRawDataTypeConfig::clear(void)
{
  // clear FIDs
  fFidItemData = FID_NOT_SUPPORTED;
  // clear inherited
  inherited::clear();
} // TRawDataTypeConfig::clear


// resolve dependencies
void TRawDataTypeConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    // get FIDs of the fields we directly use
    // - the "ITEMDATA" field containing the raw item data
    fFidItemData = getFieldIndex("ITEMDATA",fFieldListP);
    if (fFidItemData==FID_NOT_SUPPORTED) goto missingfield;
    // %%% add more FID searches (and checks, if the field is mandatory) here
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
  return;
missingfield:
  SYSYNC_THROW(TConfigParseException("fieldlist for RawDataItem must contain certain predefined fields (like ITEMDATA)!"));
} // TRawDataTypeConfig::localResolve



// create Sync Item Type of appropriate type from config
TSyncItemType *TRawDataTypeConfig::newSyncItemType(TSyncSession *aSessionP, TSyncDataStore *aDatastoreP)
{
  return
    new TRawDataItemType(
      aSessionP,
      this,
      fTypeName.c_str(),
      fTypeVersion.c_str(),
      aDatastoreP,
      fFieldListP
    );
} // TRawDataTypeConfig::newSyncItemType



#ifdef CONFIGURABLE_TYPE_SUPPORT

// config element parsing
bool TRawDataTypeConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  // - none known here
    return TMultiFieldTypeConfig::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TRawDataTypeConfig::localStartElement

#endif


/*
 * Implementation of TRawDataItemType
 */


TRawDataItemType::TRawDataItemType(
  TSyncSession *aSessionP,
  TDataTypeConfig *aTypeConfigP,
  const char *aCTType,
  const char *aVerCT,
  TSyncDataStore *aRelatedDatastoreP,
  TFieldListConfig *aFieldDefinitions // field definitions
) :
  TMultiFieldItemType(aSessionP,aTypeConfigP,aCTType,aVerCT,aRelatedDatastoreP,aFieldDefinitions)
{
  fCfgP = static_cast<TRawDataTypeConfig *>(aTypeConfigP);
} // TRawDataItemType::TRawDataItemType


TRawDataItemType::~TRawDataItemType()
{
} // TRawDataItemType::~TRawDataItemType


// create new sync item of proper type and optimization for specified target
TSyncItem *TRawDataItemType::internalNewSyncItem(TSyncItemType *aTargetItemTypeP, TLocalEngineDS *aLocalDatastoreP)
{
  // All RawDataItems are stored in MultiFieldItems (in specific, predefined fields)
  TMultiFieldItemType *targetitemtypeP;
  GET_CASTED_PTR(targetitemtypeP,TMultiFieldItemType,aTargetItemTypeP,DEBUGTEXT("TRawDataItemType::internalNewSyncItem with bad-typed target","mdit6"));
  MP_RETURN_NEW(TMultiFieldItem,DBG_OBJINST,"TMultiFieldItem",TMultiFieldItem(this,targetitemtypeP));
} // TRawDataItemType::internalNewSyncItem


// fill in SyncML data (but leaves IDs empty)
bool TRawDataItemType::internalFillInData(
  TSyncItem *aSyncItemP,      // SyncItem to be filled with data
  SmlItemPtr_t aItemP,        // SyncML toolkit item Data to be converted into SyncItem (may be NULL if no data, in case of Delete or Map)
  TLocalEngineDS *aLocalDatastoreP, // local datastore
  TStatusCommand &aStatusCmd  // status command that might be modified in case of error
)
{
  // check type
  TMultiFieldItem *itemP;
  GET_CASTED_PTR(itemP,TMultiFieldItem,aSyncItemP,DEBUGTEXT("TRawDataItemType::internalFillInData: incompatible item class","mdit7"));
  // store data, if any, in predefined ITEMDATA field
  if (aItemP->data) {
    // read data into predefined raw data field
    TItemField *fldP = itemP->getField(fCfgP->fFidItemData);
    if (fldP) {
      // get raw data
      stringSize sz;
      cAppCharP data = smlPCDataToCharP(aItemP->data,&sz);
      // put it into ITEMDATA field
      fldP->setAsString(data, sz);
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
} // TRawDataItemType::internalFillInData


// sets data and meta from SyncItem data, but leaves source & target untouched
bool TRawDataItemType::internalSetItemData(
  TSyncItem *aSyncItemP,  // the syncitem to be represented as SyncML
  SmlItemPtr_t aItem,     // item with NULL meta and NULL data
  TLocalEngineDS *aLocalDatastoreP // local datastore
)
{
  // check type
  TMultiFieldItem *itemP;
  GET_CASTED_PTR(itemP,TMultiFieldItem,aSyncItemP,DEBUGTEXT("TRawDataItemType::internalSetItemData: incompatible item class","mdit8"));
  // let ancestor prepare first
  if (!TMultiFieldItemType::internalSetItemData(aSyncItemP,aItem,aLocalDatastoreP)) return false;
  // generate data item from predefined ITEMDATA field
  TItemField *fldP = itemP->getField(fCfgP->fFidItemData);
  if (fldP) {
    string dataitem;
    if (fldP->isBasedOn(fty_blob)) {
      // is a BLOB, don't use getAsString as BLOB only returns pseudo-data indicating length of BLOB
      ((TBlobField *)fldP)->getBlobAsString(dataitem);
    }
    else {
      // for all other types, just get it as string
      fldP->getAsString(dataitem);
    }
    // put data item into opaque/cdata PCData
    aItem->data=newPCDataStringX((const uInt8 *)dataitem.c_str(),true,dataitem.size());
  }
  // can't go wrong
  return true;
} // TRawDataItemType::internalSetItemData



// generates SyncML-Devinf property list for type
SmlDevInfCTDataPropListPtr_t TRawDataItemType::newCTDataPropList(TTypeVariantDescriptor aVariantDescriptor)
{
  // return supported properties
  //#warning "TODO create this list from some configuration data"
  return NULL; // %%% for now: none
}


// Analyze CTCap part of devInf
bool TRawDataItemType::analyzeCTCap(SmlDevInfCTCapPtr_t aCTCapP)
{
  // no analysis so far
  // TODO: maybe add mechanism to capture CTCap here and pass it to the DB backend in a predefined field for analysis
  return inherited::analyzeCTCap(aCTCapP);
}



/// @brief helper to create same-typed instance via base class
TSyncItemType *TRawDataItemType::newCopyForSameType(
  TSyncSession *aSessionP,     // the session
  TSyncDataStore *aDatastoreP  // the datastore
)
{
  // create new itemtype of appropriate derived class type that can handle
  // this type
  MP_RETURN_NEW(TRawDataItemType,DBG_OBJINST,"TRawDataItemType",TRawDataItemType(
    aSessionP,
    fTypeConfigP,
    getTypeName(),
    getTypeVers(),
    aDatastoreP,
    fFieldDefinitionsP
  ));
} // TRawDataItemType::newCopyForSameType


/// @brief copy CTCap derived info from another SyncItemType
/// @return false if item not compatible
/// @note required to create remote type variants from ruleMatch type alternatives
bool TRawDataItemType::copyCTCapInfoFrom(TSyncItemType &aSourceItem)
{
  // must be same type as myself or based on the type of myself
  if (!aSourceItem.isBasedOn(getTypeID()))
    return false; // not compatible
  //TRawDataItemType *itemTypeP = static_cast<TRawDataItemType *>(&aSourceItem);
  // all CTCap info we might have is in the field options of MultiFieldItemType
  return inherited::copyCTCapInfoFrom(aSourceItem);
} // TRawDataItemType::copyCTCapInfoFrom


/* end of TRawDataItemType implementation */

} // namespace sysync


// eof
