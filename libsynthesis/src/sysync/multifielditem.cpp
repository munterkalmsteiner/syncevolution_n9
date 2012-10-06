/*
 *  File:         MultiFieldItem.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TMultiFieldItem
 *    Item consisting of multiple data fields (TItemField objects)
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-08-08 : luz : created
 *
 */

// includes
#include "prefix_file.h"
#include "sysync.h"
#include "multifielditem.h"
#include "multifielditemtype.h"


using namespace sysync;

namespace sysync {

// Config
// ======


TFieldListConfig::TFieldListConfig(const char* aName, TConfigElement *aParentElement) :
  TConfigElement(aName,aParentElement)
{
  clear();
} // TFieldListConfig::TFieldListConfig


TFieldListConfig::~TFieldListConfig()
{
  clear();
} // TFieldListConfig::~TFieldListConfig


// init defaults
void TFieldListConfig::clear(void)
{
  // init defaults
  fAgeSortable=false;
  fFields.clear();
  #ifdef HARDCODED_TYPE_SUPPORT
  fFieldListTemplateP=NULL;
  #endif
  // clear inherited
  inherited::clear();
} // TFieldListConfig::clear


#ifdef CONFIGURABLE_TYPE_SUPPORT

// config element parsing
bool TFieldListConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // - fieldlist entry
  //   <field name="REV" type="timestamp" compare="never" age="yes" merge="no"/>
  if (strucmp(aElementName,"field")==0) {
    // may not contain anything
    expectEmpty();
    // check attributes
    const char *nam = getAttr(aAttributes,"name");
    const char *type = getAttr(aAttributes,"type");
    const char *rel = getAttr(aAttributes,"compare");
    if (!(nam && *nam && type && rel))
      return fail("'field' must have 'name', 'type' and 'compare' attributes");
    // parse enums
    sInt16 ty;
    if (!StrToEnum(ItemFieldTypeNames,numFieldTypes,ty,type))
      return fail("Unknown 'type' attribute: '%s'",type);
    sInt16 eqrel;
    if (!StrToEnum(compareRelevanceNames,numEQmodes,eqrel,rel))
      return fail("Unknown 'compare' attribute: '%s'",rel);
    // set defaults
    bool agerelevant=false; // not age relevant by default
    sInt16 mergemode=mem_none; // no merge   by default
    // get optional attributes
    if (!getAttrBool(aAttributes,"age",agerelevant,true))
      return fail("Bad boolean value");
    #ifdef ARRAYFIELD_SUPPORT
    bool array=false; // not an array
    if (!getAttrBool(aAttributes,"array",array,true))
      return fail("Bad boolean value");
    #endif
    const char *p = getAttr(aAttributes,"merge");
    if (p) {
      // sort out special cases
      if (strucmp(p,"no")==0)
        mergemode=mem_none;
      else if (strucmp(p,"fillempty")==0)
        mergemode=mem_fillempty;
      else if (strucmp(p,"addunassigned")==0)
        mergemode=mem_addunassigned;
      else if (strucmp(p,"append")==0)
        mergemode=mem_concat;
      else if (strucmp(p,"lines")==0)
        mergemode='\n';
      else if (strlen(p)==1)
        mergemode=*p; // single char is merge char
      else
        return fail("Invalid value '%s' for 'merge' attribute",p);
    }
    // now add new field specification
    TFieldDefinition fielddef;
    // prepare template element
    fielddef.type=(TItemFieldTypes)ty;
    #ifdef ARRAYFIELD_SUPPORT
    fielddef.array = array;
    #endif
    TCFG_ASSIGN(fielddef.fieldname,nam);
    fielddef.eqRelevant=(TEqualityMode)eqrel;
    fielddef.ageRelevant=agerelevant;
    fAgeSortable=fAgeSortable || fielddef.ageRelevant; // if at least one field is age-relevant, we can sort items
    fielddef.mergeMode=mergemode;
    // copy into array
    fFields.push_back(fielddef);
  }
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TFieldListConfig::localStartElement


// resolve
void TFieldListConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    // check for required settings
    if (fFields.size()==0)
      SYSYNC_THROW(TSyncException("fieldlist must contain at least one field"));
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TFieldListConfig::localResolve

#endif


// get index of a field
sInt16 TFieldListConfig::fieldIndex(const char *aName, size_t aLen)
{
  TFieldDefinitionList::iterator pos;
  sInt16 n;
  for (n=0,pos=fFields.begin(); pos!=fFields.end(); ++n,pos++) {
    if (strucmp(aName,pos->TCFG_CSTR(fieldname),aLen)==0) {
      return n; // return field ID
    }
  }
  return VARIDX_UNDEFINED; // not found
} // TFieldListConfig::fieldIndex


// profile handler

TProfileHandler::TProfileHandler(TProfileConfig *aProfileCfgP, TMultiFieldItemType *aItemTypeP)
{
  // save profile config pointer
  fItemTypeP = aItemTypeP;
  // no related datastore yet
  fRelatedDatastoreP = NULL;
} // TProfileHandler::TProfileHandler


TProfileHandler::~TProfileHandler()
{
  // nop for now
} // TProfileHandler::~TProfileHandler


// - get session pointer
TSyncSession *TProfileHandler::getSession(void)
{
  return fItemTypeP ? fItemTypeP->getSession() : NULL;
} // TProfileHandler::getSession

// - get session zones pointer
GZones *TProfileHandler::getSessionZones(void)
{
  return fItemTypeP ? fItemTypeP->getSessionZones() : NULL;
} // TProfileHandler::getSessionZones


#ifdef SYDEBUG

TDebugLogger *TProfileHandler::getDbgLogger(void)
{
  // commands log to session's logger
  return fItemTypeP ? fItemTypeP->getDbgLogger() : NULL;
} // TProfileHandler::getDbgLogger

uInt32 TProfileHandler::getDbgMask(void)
{
  if (!fItemTypeP) return 0; // no item type, no debug
  return fItemTypeP->getDbgMask();
} // TProfileHandler::getDbgMask

#endif


// - check availability (depends on item "supported" flags only in SyncML datastore context)
bool TProfileHandler::isFieldAvailable(TMultiFieldItem &aItem, sInt16 aFieldIndex)
{
  if (fRelatedDatastoreP) {
    // in datastore/SyncML context, only fields supported on both sides are considered "available"
    return aItem.isAvailable(aFieldIndex);
  }
  else {
    // in non-datastore context, all fields are considered available, as long as
    // the field index is in range
    TMultiFieldItemType *mfitP = aItem.getItemType();
    return mfitP && mfitP->isFieldIndexValid(aFieldIndex);
  }
} // TProfileHandler::isFieldAvailable






// Profile config root element

TProfileConfig::TProfileConfig(const char* aName, TConfigElement *aParentElement) :
  TConfigElement(aName,aParentElement)
{
  clear();
} // TProfileConfig::TProfileConfig


TProfileConfig::~TProfileConfig()
{
  clear();
} // TProfileConfig::~TProfileConfig


// init defaults
void TProfileConfig::clear(void)
{
  // init defaults
  fFieldListP=NULL; // no field list linked
  // clear inherited
  inherited::clear();
} // TProfileConfig::clear



#ifdef HARDCODED_TYPE_SUPPORT

// read hardcoded fieldlist config
void TFieldListConfig::readFieldListTemplate(const TFieldDefinitionsTemplate *aTemplateP)
{
  if (!aTemplateP) return;
  // save the link to the template as well (we'll need it for maxsize etc. later)
  fFieldListTemplateP=aTemplateP;
  fFields.clear();
  // copy values
  fAgeSortable=false;
  TFieldDefinition fielddef;
  for (sInt16 i=0; i<aTemplateP->numFields; i++) {
    const TFieldDefinitionTemplate *afieldP= &(aTemplateP->fieldDefs[i]);
    // prepare template element
    fielddef.type=afieldP->type;
    #ifdef ARRAYFIELD_SUPPORT
    fielddef.array = afieldP->array;
    #endif
    TCFG_ASSIGN(fielddef.fieldname,afieldP->fieldname);
    fielddef.eqRelevant=afieldP->eqRelevant;
    fielddef.ageRelevant=afieldP->ageRelevant;
    fAgeSortable=fAgeSortable || fielddef.ageRelevant; // if at least one field is age-relevant, we can sort
    fielddef.mergeMode=afieldP->mergeMode;
    // copy into array
    fFields.push_back(fielddef);
  }
} // TFieldListConfig::readFieldListTemplate

#endif



TMultiFieldDatatypesConfig::TMultiFieldDatatypesConfig(TConfigElement *aParentElement) :
  TDatatypesConfig("datatypes",aParentElement)
{
  clear();
} // TMultiFieldDatatypesConfig::TMultiFieldDatatypesConfig


TMultiFieldDatatypesConfig::~TMultiFieldDatatypesConfig()
{
  // make sure we don't re-build types (createHardcodedTypes() in clear())
  // so we call internalClear() here!
  internalClear();
} // TMultiFieldDatatypesConfig::~TMultiFieldDatatypesConfig


// init defaults
void TMultiFieldDatatypesConfig::clear(void)
{
  // remove internals
  internalClear();
  // Now datatypes registry is really empty
  #ifdef HARDCODED_TYPE_SUPPORT
  // - add hard-coded default type information (if any)
  static_cast<TRootConfig *>(getRootElement())->createHardcodedTypes(this);
  #endif
} // TMultiFieldDatatypesConfig::clear


// init defaults
void TMultiFieldDatatypesConfig::internalClear(void)
{
  // remove fieldlists
  TFieldListsList::iterator pos1;
  for(pos1=fFieldLists.begin();pos1!=fFieldLists.end();pos1++)
    delete *pos1;
  fFieldLists.clear();
  // remove profiles
  TProfilesList::iterator pos2;
  for(pos2=fProfiles.begin();pos2!=fProfiles.end();pos2++)
    delete *pos2;
  fProfiles.clear();
  // clear inherited
  inherited::clear();
} // TMultiFieldDatatypesConfig::internalClear


// get a field list by name
TFieldListConfig *TMultiFieldDatatypesConfig::getFieldList(const char *aName)
{
  TFieldListsList::iterator pos;
  for(pos=fFieldLists.begin();pos!=fFieldLists.end();pos++) {
    if (strucmp((*pos)->getName(),aName)==0) {
      // found
      return *pos;
    }
  }
  return NULL; // not found
} // TMultiFieldDatatypesConfig::getFieldList


// get a profile by name
TProfileConfig *TMultiFieldDatatypesConfig::getProfile(const char *aName)
{
  TProfilesList::iterator pos;
  for(pos=fProfiles.begin();pos!=fProfiles.end();pos++) {
    if (strucmp((*pos)->getName(),aName)==0) {
      // found
      return *pos;
    }
  }
  return NULL; // not found
} // TMultiFieldDatatypesConfig::getProfile



#ifdef CONFIGURABLE_TYPE_SUPPORT

// config element parsing
bool TMultiFieldDatatypesConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  // - field lists or profiles can appear at this level
  bool newFieldList=false;
  TProfileConfig *newProfileP=NULL;
  TFieldListConfig *flP = NULL;
  // in case it is a field list or a profile - check for name
  const char* nam = getAttr(aAttributes,"name");
  // now check if fieldlist or profile
  if (strucmp(aElementName,"fieldlist")==0) {
    newFieldList=true;
  }
  // the xml tag itself is used as the profile's typename (historical reasons/compatibility with existing config)
  else if ((newProfileP=getSyncAppBase()->getRootConfig()->newProfileConfig(nam,aElementName,this))!=NULL) {
    const char* flnam = getAttr(aAttributes,"fieldlist");
    if (!flnam)
      return fail("%s is missing 'fieldlist' attribute",aElementName);
    else {
      flP = getFieldList(flnam);
      if (!flP)
        return fail("fieldlist '%s' unknown in %s",flnam,aElementName);
    }
  }
  // - tag not known here
  else
    return TDatatypesConfig::localStartElement(aElementName,aAttributes,aLine);
  // known tag, check if we need further processing
  if (newFieldList || newProfileP) {
    if (!nam)
      return fail("%s is missing 'name' attribute",aElementName);
    // create new named field list or use already created profile
    if (newFieldList) {
      // new field list
      TFieldListConfig *fieldlistcfgP = new TFieldListConfig(nam,this);
      fFieldLists.push_back(fieldlistcfgP); // save in list
      expectChildParsing(*fieldlistcfgP); // let element handle parsing
    }
    else {
      // new profile
      newProfileP->fFieldListP=flP; // set field list for profile
      fProfiles.push_back(newProfileP); // save in list
      expectChildParsing(*newProfileP); // let element handle parsing
    }
  }
  // ok
  return true;
} // TMultiFieldDatatypesConfig::localStartElement


// resolve
void TMultiFieldDatatypesConfig::localResolve(bool aLastPass)
{
  // resolve profiles
  TProfilesList::iterator pos1;
  for(pos1=fProfiles.begin();pos1!=fProfiles.end();pos1++) {
    (*pos1)->localResolve(aLastPass);
  }
  // resolve field lists
  TFieldListsList::iterator pos2;
  for(pos2=fFieldLists.begin();pos2!=fFieldLists.end();pos2++) {
    (*pos2)->localResolve(aLastPass);
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TMultiFieldDatatypesConfig::localResolve


#endif


/*
 * Implementation of TMultiFieldItem
 */

/* public TMultiFieldItem members */


TMultiFieldItem::TMultiFieldItem(
  TMultiFieldItemType *aItemTypeP,        // owner's (=source) type
  TMultiFieldItemType *aTargetItemTypeP   // target type (for optimization)
) :
  TSyncItem(aItemTypeP)
{
  // save types
  fItemTypeP = aItemTypeP;  // owner (source) type
  fTargetItemTypeP = aTargetItemTypeP; // target (destination) type
  // copy field definitions pointer for fast access
  fFieldDefinitionsP = fItemTypeP->getFieldDefinitions();
  if (!fFieldDefinitionsP)
    SYSYNC_THROW(TSyncException(DEBUGTEXT("MultiFieldItem without FieldDefinitions","mfi3")));
  // test if target has same field defs
  if (fTargetItemTypeP->getFieldDefinitions()!=fFieldDefinitionsP)
    SYSYNC_THROW(TSyncException(DEBUGTEXT("MultiFieldItem with non-matching target field definitions","mfi1")));
  // create fields array
  fFieldsP = new TItemFieldP[fFieldDefinitionsP->numFields()];
  // - init it with null pointers
  for (sInt16 i=0; i<fFieldDefinitionsP->numFields(); i++) fFieldsP[i]=NULL;
} // TMultiFieldItem::TMultiFieldItem


TMultiFieldItem::~TMultiFieldItem()
{
  // remove fields
  cleardata();
  // remove fields list
  delete[] fFieldsP;
} // TMultiFieldItem::~TMultiFieldItem



// remove all data from item
void TMultiFieldItem::cleardata(void)
{
  if (fFieldDefinitionsP) {
    for (sInt16 i=0; i<fFieldDefinitionsP->numFields(); i++) {
      if (fFieldsP[i]) {
        delete fFieldsP[i]; // delete field object
        fFieldsP[i]=NULL;
      }
    }
  }
} // TMultiFieldItem::cleardata


#if defined(CHECKSUM_CHANGELOG) && !defined(RECORDHASH_FROM_DBAPI)

// changelog support: calculate CRC over contents
uInt16 TMultiFieldItem::getDataCRC(uInt16 crc, bool aEQRelevantOnly)
{
  // iterate over all fields
  if (fFieldDefinitionsP) {
    for (sInt16 i=0; i<fFieldDefinitionsP->numFields(); i++) {
      if (!aEQRelevantOnly || fFieldDefinitionsP->fFields[i].eqRelevant!=eqm_none) {
        if (fFieldsP[i]) {
          crc=fFieldsP[i]->getDataCRC(crc);
        }
      }
    }
  }
  return crc;
} // TMultiFieldItem::getDataCRC

#endif



// adjust fid and repeat offset to access array element if
// base fid is an array field or to offset fid accordingly
// if based fid is NOT an array field
// - returns adjusted aFid and aIndex ready to be used with getArrayField()
// - returns true if aFid IS an array field
bool TMultiFieldItem::adjustFidAndIndex(sInt16 &aFid, sInt16 &aIndex)
{
  bool arrfield;

  #ifdef ARRAYFIELD_SUPPORT
  // fid is offset repoffset only if not an array field
  arrfield=true;
  // check for array field first
  TItemField *fldP = getField(aFid);
  if (fldP) {
    if (!(fldP->isArray())) {
      // no array field
      arrfield=false;
      aFid += aIndex; // use array offset as additional field ID offset
      aIndex=0; // no array index
    }
  }
  else {
    // Note: if field does not exist, do not apply offset, but don't report array field either!
    arrfield = false;
  }
  #else
  // without array support, fid is always offset by rep offset
  aFid += aIndex;
  aIndex=0; // no array index
  arrfield=false;
  #endif
  // return true if this is really an array field
  return arrfield;
} // adjustFidAndIndex



// return specified leaf field of array field or regular field
// depending if aFid addresses an array or not.
// (This is a shortcut method to access fields specified by a base fid and a repeat)
TItemField *TMultiFieldItem::getArrayFieldAdjusted(sInt16 aFid, sInt16 aIndex, bool aExistingOnly)
{
  adjustFidAndIndex(aFid,aIndex);
  return getArrayField(aFid, aIndex, aExistingOnly);
} // TMultiFieldItem::getArrayFieldAdjusted


// return specified leaf field of array field
TItemField *TMultiFieldItem::getArrayField(sInt16 aFid, sInt16 aIndex, bool aExistingOnly)
{
  #ifdef ARRAYFIELD_SUPPORT
  TItemField *fiP = getField(aFid);
  if (!fiP) return NULL;
  return fiP->getArrayField(aIndex,aExistingOnly);
  #else
  // without array support, we can only access index==0
  if (aIndex>0) return NULL; // other indices don't exist
  return getField(aFid);
  #endif
} // TMultiFieldItem::getArrayField


// get field by name (returns NULL if not known, creates if known but not existing yet)
TItemField *TMultiFieldItem::getArrayField(const char *aFieldName, sInt16 aIndex, bool aExistingOnly)
{
  return getArrayField(fItemTypeP->getFieldIndex(aFieldName),aIndex,aExistingOnly);
} // TMultiFieldItem::getArrayField


// get field by index (returns NULL if not known, creates if known but not existing yet)
TItemField *TMultiFieldItem::getField(sInt16 aFieldIndex)
{
  if (!fItemTypeP->isFieldIndexValid(aFieldIndex)) return NULL; // invalid index
  TItemField *fiP = fFieldsP[aFieldIndex];
  if (!fiP) {
    // we must create the field first
    fiP=newItemField(
      fFieldDefinitionsP->fFields[aFieldIndex].type,
      getSessionZones()
      #ifdef ARRAYFIELD_SUPPORT
      ,fFieldDefinitionsP->fFields[aFieldIndex].array
      #endif
    );
    // save in array
    fFieldsP[aFieldIndex] = fiP;
  }
  return fiP;
} // TMultiFieldItem::getField


// find index of field (returns FID_NOT_SUPPORTED if field is not a field of this item)
sInt16 TMultiFieldItem::getIndexOfField(const TItemField *aFieldP)
{
  for (sInt16 i=0; i<fFieldDefinitionsP->numFields(); i++) {
    if (fFieldsP[i]==aFieldP) {
      // found field, return it's index
      return i;
    }
  }
  return FID_NOT_SUPPORTED; // not found
} // TMultiFieldItem::getIndexOfField



// get field by name (returns NULL if not known, creates if known but not existing yet)
TItemField *TMultiFieldItem::getField(const char *aFieldName)
{
  return getField(fItemTypeP->getFieldIndex(aFieldName));
} // TMultiFieldItem::getField


// get field reference (create if not yet created)
// throws if bad index
TItemField &TMultiFieldItem::getFieldRef(sInt16 aFieldIndex)
{
  TItemField *fiP = getField(aFieldIndex);
  if (!fiP)
    SYSYNC_THROW(TSyncException(DEBUGTEXT("getFieldRef with bad index called","mfi2"))); // invalid index
  return *fiP;
} // TMultiFieldItem::getFieldRef


// check if field is assigned (exists and has a value)
bool const TMultiFieldItem::isAssigned(const char *aFieldName)
{
  return isAssigned(fItemTypeP->getFieldIndex(aFieldName));
} // TMultiFieldItem::isAssigned


TMultiFieldItemType *TMultiFieldItem::getLocalItemType(void)
{
  return fItemTypeP && fItemTypeP->isRemoteType() ? fTargetItemTypeP : fItemTypeP;
} // TMultiFieldItem::getLocalItemType


TMultiFieldItemType *TMultiFieldItem::getRemoteItemType(void)
{
  return fItemTypeP && fItemTypeP->isRemoteType() ? fItemTypeP : fTargetItemTypeP;
} // TMultiFieldItem::getRemoteItemType



// check if field is assigned (exists and has a value)
bool const TMultiFieldItem::isAssigned(sInt16 aFieldIndex)
{
  // check if field object exists at all
  if (!fItemTypeP->isFieldIndexValid(aFieldIndex)) return false; // invalid index
  TItemField *fiP = fFieldsP[aFieldIndex];
  if (!fiP) return false; // field object does not exist
  // return if field object is assigned
  return fiP->isAssigned();
} // TMultiFieldItem::isAssigned


// field availability (combined source & target)
bool TMultiFieldItem::isAvailable(const char *aFieldName)
{
  return isAvailable(fItemTypeP->getFieldIndex(aFieldName));
} // TMultiFieldItem::isAvailable


// field availability (combined source & target)
bool TMultiFieldItem::isAvailable(sInt16 aFieldIndex)
{
  if (fItemTypeP && fTargetItemTypeP) {
    if (!fItemTypeP->isFieldIndexValid(aFieldIndex)) return false; // invalid index
    return
      fItemTypeP->getFieldOptions(aFieldIndex)->available &&
      fTargetItemTypeP->getFieldOptions(aFieldIndex)->available;
  }
  else
    return false; // source or target missing, not available
} // TMultiFieldItem::isAvailable


bool TMultiFieldItem::knowsRemoteFieldOptions(void)
{
  // knows them if either myself or the other side has received devInf
  // (depends: received item has it in its own type, to be sent one in the target type)
  return
    (fItemTypeP &&  fItemTypeP->hasReceivedFieldOptions()) ||
    (fTargetItemTypeP && fTargetItemTypeP->hasReceivedFieldOptions());
} // TMultiFieldItem::knowsRemoteFieldOptions



// make sure that all fields that are available in source and target are
// assigned at least an empty value
void TMultiFieldItem::assignAvailables(void)
{
  if (fFieldDefinitionsP) {
    for (sInt16 k=0; k<fFieldDefinitionsP->numFields(); k++) {
      if (isAvailable(k)) {
        TItemField *fldP=getField(k); // force creation
        if (fldP) {
          // make sure it is assigned a "empty" value
          if (fldP->isUnassigned())
            fldP->assignEmpty();
        }
      }
    }
  }
} // TMultiFieldItem::assignAvailables




// cast pointer to same type, returns NULL if incompatible
TMultiFieldItem *TMultiFieldItem::castToSameTypeP(TSyncItem *aItemP)
{
  if (aItemP->isBasedOn(ity_multifield)) {
    TMultiFieldItem *multifielditemP=static_cast<TMultiFieldItem *> (aItemP);
    // class compatible, now test type compatibility
    // - field definition list must be the same instance(!) in both items
    if (fFieldDefinitionsP==multifielditemP->fFieldDefinitionsP)
      return multifielditemP;
    else
      return NULL;
  }
  // not even class compatible
  return NULL;
} // TMultiFieldItem::castToSameTypeP


// test if comparable (at least for equality)
bool TMultiFieldItem::comparable(TSyncItem &aItem)
{
  // test if comparable: other type must be same type of multifield
  return castToSameTypeP(&aItem)!=NULL;
} // TMultiFieldItem::comparable


// test if sortable (by age, newer are > than older)
bool TMultiFieldItem::sortable(TSyncItem &aItem)
{
  if (!fFieldDefinitionsP->fAgeSortable) return false; // not sortable at all
  if (comparable(aItem)) {
    // item is comparable (has same FieldDefinitions)
    // Now check if all ageRelevant fields are assigned on both sides
    // Note: we can static-cast here because comparable() has verified aItem's type
    TMultiFieldItem *multifielditemP=static_cast<TMultiFieldItem *> (&aItem);
    // search for ageRelevant fields
    for (sInt16 i=0; i<fFieldDefinitionsP->numFields(); i++) {
      if (fFieldDefinitionsP->fFields[i].ageRelevant) {
        // check if available for both types
        if (!(
          isAssigned(i) &&  // my own
          multifielditemP->isAssigned(i) // aItem's
        ))
          return false; // missing needed field on one side
      }
    }
    return true; // all ageRelevant fields are assigned in both sides
  }
  else return false; // not comparable is not sortable either
} // TMultiFieldItem::sortable


#ifdef OBJECT_FILTERING


// check post-fetch filter
bool TMultiFieldItem::postFetchFiltering(TLocalEngineDS *aDatastoreP)
{
  return fItemTypeP->postFetchFiltering(this,aDatastoreP);
} // TMultiFieldItem::postFetchFiltering


// test if item passes filter
bool TMultiFieldItem::testFilter(const char *aFilterString)
{
  // process filter without modifying
  #ifdef SYDEBUG
  PDEBUGPRINTFX(DBG_DATA+DBG_FILTER+DBG_HOT,(
    "Testing filter '%s' against item:",
    aFilterString
  ));
  if (*aFilterString && PDEBUGTEST(DBG_DATA+DBG_FILTER+DBG_USERDATA)) {
    debugShowItem(DBG_DATA+DBG_FILTER);
  }
  #endif
  bool result=processFilter(false,aFilterString);
  PDEBUGPRINTFX(DBG_DATA+DBG_FILTER+DBG_HOT,(
    "Filter test result is %s",
    result ? "TRUE" : "FALSE"
  ));
  // false on syntax error
  if (*aFilterString) {
    PDEBUGPRINTFX(DBG_ERROR,("unexpected chars in filter expression: %s",aFilterString));
    return false;
  }
  return result;
} // TMultiFieldItem::testFilter


// make item pass filter
bool TMultiFieldItem::makePassFilter(const char *aFilterString)
{
  // process filter with making modifications such that item passes filter condition
  bool result = processFilter(true,aFilterString);
  // false on syntax error
  if (*aFilterString) {
    PDEBUGPRINTFX(DBG_ERROR+DBG_FILTER,("unexpected chars in filter expression: %s",aFilterString));
    return false;
  }
  return result;
} // TMultiFieldItem::makePassFilter


// process filter expression
bool TMultiFieldItem::processFilter(bool aMakePass, const char *&aPos, const char *aStop, sInt16 aLastOpPrec)
{
  char c=0;
  const char *st;
  string str;
  bool result;
  sInt16 fid;
  sInt16 cmpres=0;
  bool neg;
  bool assignToMakeTrue;
  bool specialValue;
  bool caseinsensitive;
  TStringField idfield;
  TItemField *fldP;

  // determine max length
  if (aStop==NULL) aStop=aPos+strlen(aPos);
  // empty expression is true
  result=true;
  // process simple term (<ident><op><value>)
  // Note: do not allow negation to make sure
  //       that TRUE of a comparison always
  //       adds to TRUE of the entire expression
  neg=false; // not negated
  // - get first non-space
  while (aPos<=aStop) {
    c=*aPos;
    if (c!=' ') break;
    aPos++;
  }
  // Term starts here, first char is c, aPos points to it
  // - check subexpression paranthesis
  if (c=='(') {
    // boolean term is grouped subexpression, don't stop at logical operation
    aPos++;
    result=processFilter(aMakePass,aPos,aStop,0); // dont stop at any logical operator
    // check if matching paranthesis
    if (*(aPos++)!=')') {
      PDEBUGPRINTFX(DBG_ERROR+DBG_FILTER,("Filter expression error (missing \")\") at: %s",--aPos));
      //%%% no, don't skip rest, as otherwise caller will not know that filter processing failed!%%% aPos=aStop; // skip rest
      return false; // always fail
    }
    if (neg) result=!result;
  }
  else if (c==0) {
    // empty term, counts as true
    return result;
  }
  else {
    // must be simple boolean term
    // - remember start of ident
    st=aPos;
    // - search end of ident
    while (isFilterIdent(c)) c=*(++aPos);
    // - c/aPos=char after ident, get ident
    str.assign(st,aPos-st);
    // - check for subscript index
    uInt16 subsIndex=0; // no index (index is 1-based in DS 1.2 filter specs)
    if (c=='[') {
      // expect numeric index
      aPos++; // next
      aPos+=StrToUShort(aPos,subsIndex);
      if (*aPos!=']') {
        PDEBUGPRINTFX(DBG_ERROR+DBG_FILTER,("Filter expression error (missing \"]\") at: %s",--aPos));
        return false; // syntax error, does not pass
      }
      c=*(++aPos); // process next after subscript
    }
    // - get field ID for that ident (can be -1 if none found)
    //   - check special idents first
    if (str=="LUID") {
      // this is SyncML-TAF Standard
      // it is also produced by DS 1.2 &LUID; pseudo-identifier
      if (IS_CLIENT)
        idfield.setAsString(getLocalID());
      else
        idfield.setAsString(getRemoteID());
      fldP=&idfield;
    }
    else if (str=="LOCALID") {
      // this is a Synthesis extension
      idfield.setAsString(getLocalID());
      fldP=&idfield;
    }
    #ifdef SYSYNC_SERVER
    else if (IS_SERVER && str=="GUID") {
      // this is a Synthesis extension, added for symmetry to LUID
      idfield.setAsString(getLocalID());
      fldP=&idfield;
    }
    else if (IS_SERVER && str=="REMOTEID") {
      // this is a Synthesis extension
      idfield.setAsString(getRemoteID());
      fldP=&idfield;
    }
    #endif // SYSYNC_SERVER
    else {
      // must be a field
      if (fItemTypeP)
        fid=fItemTypeP->getFilterIdentifierFieldIndex(str.c_str(),subsIndex);
      else
        fid=VARIDX_UNDEFINED; // none
      // now get field pointer (or NULL if field not found)
      fldP = getField(fid);
    }
    // - skip spaces
    while (isspace(c)) c=*(++aPos);
    // - check for makepass-assignment modifier ":"
    assignToMakeTrue=c==':';
    if (assignToMakeTrue) c=*(++aPos);
    // - check for special-value modifier "*"
    specialValue=c=='*';
    if (specialValue) c=*(++aPos);
    // - check for case-insensitive comparison mode
    caseinsensitive=c=='^';
    if (caseinsensitive) c=*(++aPos);
    // - now find comparison mode
    //   cmpres = expected strcmp-style result:
    //            0 if equal, 1 if ident > value, -1 if ident < value,
    aPos++; // consume first char of comparison anyway
    if (c=='%') { cmpres=2; } // special flag for CONTAINS
    else if (c=='$') { cmpres=2; neg=!neg; }
    else if (c=='=') { cmpres=0; } // equal
    else if (c=='>') {
      if (*aPos=='=') { aPos++; neg=!neg; cmpres=-1; } // >= is not <
      else { cmpres=1; } // >
    }
    else if (c=='<') {
      if (*aPos=='>') { aPos++; neg=!neg; cmpres=0; } // <> is not =
      else if (*aPos=='=') { aPos++; neg=!neg; cmpres=1; } // <= is not >
      else { cmpres=-1; } // <
    }
    // - now read value
    st=aPos; // should start here
    // - find end (end of string, closing paranthesis or logical op)
    while (aPos<aStop && *aPos!='&' && *aPos!='|' && *aPos!=')') aPos++;
    // - assign st string
    str.assign(st,aPos-st);
    // - check field
    if (!fldP) {
      // field does not exist -> result of term, negated or not, is always FALSE
      result=false;
    }
    else {
      if (cmpres==2) {
        // "contains"
        // - create a reference field
        TItemField *valfldP = newItemField(fldP->getElementType(),getSessionZones());
        // - assign value as string
        valfldP->setAsString(str.c_str());
        result = fldP->contains(*valfldP,caseinsensitive);
        // assign to make pass if enabled
        if (!result && aMakePass && assignToMakeTrue) {
          if (fldP->isArray())
            fldP->append(*valfldP); // just append another element to make it contained
          else
            *fldP = *valfldP; // just overwrite value with to-be-contained value
          result=true; // now passes
        }
        delete valfldP; // no longer needed
      }
      else {
        if (specialValue) {
          if (cmpres!=0)
            result=false; // can only compare for equal
          else {
            if (str=="E") {
              // empty
              result = fldP->isEmpty();
            }
            else if (str=="N") {
              // NULL, unassigned
              result = fldP->isAssigned();
            }
            if (neg) result=!result;
            // make empty or unassigned to pass filter (make non-empty is not possible)
            if (!result && aMakePass && assignToMakeTrue && !neg) {
              if (str=="E") {
                fldP->assignEmpty();
              }
              else if (str=="N")  {
                fldP->unAssign();
              }
              result=true; // now passes
            }
          }
        }
        else {
          // create a reference field
          TItemField *valfldP = newItemField(fldP->getElementType(),getSessionZones());
          // assign value as string
          valfldP->setAsString(str.c_str());
          // compare fields, then compare result with what was expected
          result = (fldP->compareWith(*valfldP,caseinsensitive) == cmpres);
          // negate result if needed
          if (neg) result=!result;
          // if field not assigned, comparison is always false
          if (fldP->isUnassigned()) result=false;
          // assign to make pass if enabled
          if (!result && aMakePass && assignToMakeTrue) {
            (*fldP) = (*valfldP);
            result=true; // now passes
          }
          // now clear again
          delete valfldP;
        }
      }
    }
  }
  // term is now evaluated, show what follows
  // - check for boolean op chain, aPos points now to possible logical operator
  do {
    // - skip spaces
    c=*aPos;
    while (c==' ') c=*(++aPos);
    // - check char at aPos
    if (c=='&') {
      // AND
      if (2<=aLastOpPrec) return result; // evaluation continues in caller (always as long as we don't have higher prec than AND)
      // skip op
      aPos++;
      // next term must be true as well
      // - return when encountering AND, OR and end of expression
      // - next term must also be modified to make pass
      bool termres = processFilter(aMakePass, aPos, aStop, 2);
      result = result && termres;
    }
    else if (c=='|') {
      // OR
      if (1<=aLastOpPrec) return result; // evaluation continues in caller
      // skip op
      aPos++;
      // next term must be true only if this one is not true
      // - return only when encountering AND or
      // - if first term is already true, next must never be modifed to pass
      bool termres = processFilter(result ? false : aMakePass, aPos, aStop, 1);
      result = result || termres;
    }
    else {
      // End of Expression
      // would be: if (0<=aLastOpPrec)
      return result;
    }
  } while(true);
}

#endif

#ifdef SYSYNC_SERVER

// compare function, returns 0 if equal, 1 if this > aItem, -1 if this < aItem
sInt16 TMultiFieldItem::compareWith(
  TSyncItem &aItem,
  TEqualityMode aEqMode,
  TLocalEngineDS *aDatastoreP
  #ifdef SYDEBUG
  ,bool aDebugShow
  #endif
)
{
  #ifndef SYDEBUG
  const aDebugShow = false;
  #endif
  sInt16 cmpres;
  TMultiFieldItem *multifielditemP = castToSameTypeP(&aItem);
  if (!multifielditemP) {
    cmpres = SYSYNC_NOT_COMPARABLE;
    goto exit;
  }
  // do the compare
  if (fItemTypeP)
    cmpres=fItemTypeP->compareItems(*this,*multifielditemP,aEqMode,aDebugShow,aDatastoreP);
  else
    cmpres=standardCompareWith(*multifielditemP,aEqMode,aDebugShow);
exit:
  #ifdef SYDEBUG
  if (aDebugShow) {
    OBJDEBUGPRINTFX(getItemType()->getSession(),DBG_DATA,(
      "Compared [LOC=%s,REM=%s] with  [LOC=%s,REM=%s] (eqMode=%hd), cmpres=%hd",
      getLocalID(),
      getRemoteID(),
      aItem.getLocalID(),
      aItem.getRemoteID(),
      (sInt16) aEqMode,
      cmpres
    ));
  }
  #endif
  return cmpres;
} // TMultiFieldItem::compareWith


// compare function, returns 0 if equal, 1 if this > aItem, -1 if this < aItem
sInt16 TMultiFieldItem::standardCompareWith(
  TMultiFieldItem &aItem,
  TEqualityMode aEqMode,
  bool aDebugShow
)
{
  sInt16 commonfound=0;
  sInt16 result=0; // default to equal
  // we should test for comparable() before!
  if (!comparable(aItem)) {
    result=SYSYNC_NOT_COMPARABLE;
    goto exit;
  }
  // now compare field-by-field
  // - equal means equality of all eqRelevant fields (both non-existing is
  //   equality, too)
  //   (but possibly differences in ageRelevant fields)
  // - larger/smaller means not equal in eqRelevant fields but
  //   older/newer by ageRelevant fields
  // - SYSYNC_NOT_COMPARABLE means not equal and not ageSortable either
  if (aEqMode!=eqm_nocompare) {
    for (sInt16 i=0; i<fFieldDefinitionsP->numFields(); i++) {
      // both fields must be available in their respective ItemType
      if (!getItemType()->getFieldOptions(i)->available ||
        !aItem.getItemType()->getFieldOptions(i)->available)
        continue; // not available in both items, do not compare
      // then test for equality (if relevant in given context)
      if (fFieldDefinitionsP->fFields[i].eqRelevant>=aEqMode) {
        // at least one is available and relevant in both items
        commonfound++;
        // this is an EQ-relevant field
        // - get fields
        TItemField &f1=getFieldRef(i);
        TItemField &f2=aItem.getFieldRef(i);
        // - For slowsync and firstsync matching, non-ASSIGNED fields will not
        //   be compared (to allow matching a less-equipped clinet record with its
        //   better equipped server record and vice versa. Example is the S55
        //   which discards private addresses, old method rendered lots of
        //   duplicates on slow sync
        if (aEqMode>=eqm_slowsync) {
          if (f1.isUnassigned() || f2.isUnassigned())
            continue; // omit comparing fields where one side is unassigned
        }
        // - get assigned status of both fields
        //   BCPPB revealed bad error: isAssigned was not called (forgot ())!!!
        // %%% Note: I think that isAssigned() is the wrong function here, we will use
        //     !isEmpty(), which returns true for unassigned fields as well as for empty ones
        //bool a1 = f1.isAssigned(); // assignment status of field in this item
        //bool a2 = f2.isAssigned(); // assignment status of same field in other item
        bool a1 = !f1.isEmpty(); // non-empty status of field in this item
        bool a2 = !f2.isEmpty(); // non-empty status of same field in other item
        // - if both are unassigned -> equal
        if (!a1 && !a2) continue; // we are staying equal, test next field
        // - if one of them is unassigned -> not equal
        // - if both are assigned, fields must be equal
        if (!a1 || !a2) {
          // one not assigned
          result=SYSYNC_NOT_COMPARABLE;
          #ifdef SYDEBUG
          if (aDebugShow) {
            // not assigned
            if (!a1) {
              PDEBUGPRINTFX(DBG_DATA+DBG_MATCH,("- not equal because fid=%hd not assigned/empty in this item",i));
            } else if (!a2) {
              PDEBUGPRINTFX(DBG_DATA+DBG_MATCH,("- not equal because fid=%hd not assigned/empty in other item",i));
            }
          }
          #endif
          break;
        }
        else if (f1 != f2) {
          // content not equal
          #ifdef SYDEBUG
          string ds;
          if (aDebugShow) {
            // assigned but not equal
            PDEBUGPRINTFX(DBG_DATA+DBG_MATCH,("- not equal because fid=%hd not same in both items:",i));
            getField(i)->getAsString(ds);
            PDEBUGPRINTFX(DBG_DATA+DBG_MATCH+DBG_USERDATA,(
              "- this item  : '%-.1000s'",ds.c_str()
            ));
            aItem.getField(i)->getAsString(ds);
            PDEBUGPRINTFX(DBG_DATA+DBG_MATCH+DBG_USERDATA,(
              "- other item : '%-.1000s'",ds.c_str()
            ));
            PDEBUGPRINTFX(DBG_DATA+DBG_MATCH,(
              "- thisItem.CompareWith(otherItem) = %hd",
              getFieldRef(i).compareWith(aItem.getFieldRef(i))
            ));
          }
          #endif
          // now check for cut-off situation
          sInt32 s1=getItemType()->getFieldOptions(i)->maxsize;
          sInt32 s2=aItem.getItemType()->getFieldOptions(i)->maxsize;
          // Note: (2002-12-01) do not actually use size, as it will probably not be accurate enough,
          //       but always pass FIELD_OPT_MAXSIZE_UNKNOWN.
          if (s1!=FIELD_OPT_MAXSIZE_NONE) s1=FIELD_OPT_MAXSIZE_UNKNOWN;
          if (s2!=FIELD_OPT_MAXSIZE_NONE) s2=FIELD_OPT_MAXSIZE_UNKNOWN;
          // Now check short versions
          if (f1.isShortVers(f2,s2) || f2.isShortVers(f1,s1)) {
            // cutoff detected, counts as equal
            #ifdef SYDEBUG
            if (aDebugShow) {
              PDEBUGPRINTFX(DBG_DATA+DBG_MATCH,(
                "- Cutoff detected, field considered equal, maxsize(thisitem)=%ld, maxsize(otheritem)=%ld",
                (long)s1,(long)s2
              ));
            }
            #endif
          }
          else {
            // no cutoff, not equal
            result=SYSYNC_NOT_COMPARABLE;
            break;
          }
        } // else if not equal
      } // if eq-relevant
    } // for all fields
  } // if EQ-compare at all
  if (!commonfound) result=SYSYNC_NOT_COMPARABLE;
  // if not equal, try to compare age (if age-sortable item at all)
  if (result!=0 && fFieldDefinitionsP->fAgeSortable) {
    for (sInt16 i=0; i<fFieldDefinitionsP->numFields(); i++) {
      // then test for age (if relevant)
      if (fFieldDefinitionsP->fFields[i].ageRelevant) {
        // this is an  age relevant field
        // - get assigned status of both fields
        bool a1 = isAssigned(i); // assignment status of field in this item
        bool a2 = aItem.isAssigned(i); // assignment status of same field in other item
        // - if both are unassigned -> cannot decide, continue
        if (!a1 && !a2) continue; // test next field
        // - if one of them is unassigned -> not age-comparable
        if (!a1 || !a2) {
          result=SYSYNC_NOT_COMPARABLE;
          goto exit;
        }
        // - if both are assigned, return field comparison value
        result=getFieldRef(i).compareWith(aItem.getFieldRef(i));
        if (result!=0) {
          // not equal, newer item determined
          goto exit;
        }
        // continue to resolve age with next fields
      }
    }
    // no age relevant fields or all age relevant fields equal (possibly all unassigned)
    result=SYSYNC_NOT_COMPARABLE;
  }
  // done
exit:
  return result;
} // TMultiFieldItem::standardCompareWith

#endif // server only



// update dependencies of fields (such as BLOB proxies) on localID
void TMultiFieldItem::updateLocalIDDependencies(void)
{
  const char *localid = getLocalID();
  // go through all fields
  for (sInt16 k=0; k<fFieldDefinitionsP->numFields(); k++) {
    TItemField *fldP = getField(k);
    for (sInt16 i=0; i<fldP->arraySize(); i++) {
      TItemField *leaffldP = fldP->getArrayField(i);
      if (leaffldP) leaffldP->setParentLocalID(localid);
    }
  }
} // TMultiFieldItem::updateLocalIDDependencies




/// @brief replace data contents from specified item
/// @param aAvailableOnly: only replace contents actually available in aItem, leave rest untouched
///   NOTE: this was changed slightly between 1.x.8.5 and 1.x.8.6:
///   If the source type has not received devinf saying which fields are available,
///   only fields that are actually ASSIGNED are written. If aAssignedOnly is
///   additionally set, only assigned fields will be written anyway.
/// @param aDetectCutOffs: use field's maxsize specs to detect contents cut off by limited field
///        lengths and do not replace data if target is equal with source up to field length
/// @param aAssignedOnly: just copy assigned fields (no check for availability)
/// @param aTransferUnassigned: transfer unassigned status from source item (i.e. unassign those
///        in target that are unassigned in source, no check for availability)
bool TMultiFieldItem::replaceDataFrom(TSyncItem &aItem, bool aAvailableOnly, bool aDetectCutoffs, bool aAssignedOnly, bool aTransferUnassigned)
{
  TMultiFieldItem *multifielditemP = castToSameTypeP(&aItem);
  if (!multifielditemP) return false;
  // ok, same type, copy data
  for (sInt16 i=0; i<fFieldDefinitionsP->numFields(); i++) {
    if (
      !aAvailableOnly || (
        !aAssignedOnly && // availability is relevant only if not aAssignedOnly
        multifielditemP->fItemTypeP->hasReceivedFieldOptions() &&
        multifielditemP->fItemTypeP->getFieldOptions(i)->available
      )
      || multifielditemP->isAssigned(i)
    ) {
      // copy field
      sInt32 siz=multifielditemP->fItemTypeP->getFieldOptions(i)->maxsize;
      if (aDetectCutoffs && siz!=FIELD_OPT_MAXSIZE_NONE) {
        // check if source fields's content is fully contained at beginning of
        // target string, if yes, don't do anything
        // Note: (2002-12-01) do not actually use size, as it will probably not be accurate enough,
        //       but always pass FIELD_OPT_MAXSIZE_UNKNOWN.
        if (getFieldRef(i).isShortVers(multifielditemP->getFieldRef(i),FIELD_OPT_MAXSIZE_UNKNOWN)) {
          // yes, we think that this is a cut-off,
          // so leave target untouched as it has more complete version of this field
          continue;
        }
      }
      // copy source field into this target field
      getFieldRef(i)=multifielditemP->getFieldRef(i);
    }
    else if (aTransferUnassigned && !multifielditemP->isAssigned(i)) {
      // explicitly transfer unassigned status
      // Note: this is useful in read-modify-write done exclusively for cutoff prevention,
      //       as it prevents re-writing fields that were not actually transmitted from the remote
      //       (i.e. no get-from-DB-and-write-same-value-back). Might be essential in case of special
      //       fields where the datastore MUST know if these were sent with the data, like FN in pocketpc)
      getFieldRef(i).unAssign();
    }
  }
  return true;
} // TMultiFieldItem::replaceDataFrom


// check item before processing it
bool TMultiFieldItem::checkItem(TLocalEngineDS *aDatastoreP)
{
  return fItemTypeP->checkItem(*this,aDatastoreP);
} // TMultiFieldItem::checkItem


#ifdef SYSYNC_SERVER

// merge this item with specified item.
// Notes:
// - specified item is treated as loosing item, this item is winning item
// - also updates other item to make sure it is equal to the winning after the merge
// sets (but does not reset) change status of this and other item.
// Note that changes of non-relevant fields are not reported here.
void TMultiFieldItem::mergeWith(TSyncItem &aItem, bool &aChangedThis, bool &aChangedOther, TLocalEngineDS *aDatastoreP)
{
  TMultiFieldItem *multifielditemP = castToSameTypeP(&aItem);
  if (!multifielditemP) return;
  // do the merge
  if (fItemTypeP)
    fItemTypeP->mergeItems(*this,*multifielditemP,aChangedThis,aChangedOther,aDatastoreP);
  else
    standardMergeWith(*multifielditemP,aChangedThis,aChangedOther);
  // show result
  OBJDEBUGPRINTFX(getItemType()->getSession(),DBG_DATA+DBG_CONFLICT,(
    "mergeWith() final status: thisitem: %schanged, otheritem: %schanged (relevant; eqm_none field changes are not indicated)",
    aChangedThis ? "" : "not ",
    aChangedOther ? "" : "not "
  ));
} // TMultiFieldItem::mergeWith


// merge this item with specified item.
// Notes:
// - specified item is treated as loosing item, this item is winning item
// - also updates other item to make sure it is equal to the winning after the merge
// returns update status of this and other item. Note that changes of non-relevant fields are
// not reported here.
void TMultiFieldItem::standardMergeWith(TMultiFieldItem &aItem, bool &aChangedThis, bool &aChangedOther)
{
  // same type of multifield, try to merge
  for (sInt16 i=0; i<fFieldDefinitionsP->numFields(); i++) {
    // get merge mode
    sInt16 sep=fFieldDefinitionsP->fFields[i].mergeMode;
    // possible merging is only relevant (=to be reported) for fields that are not eqm_none
    bool mergerelevant = fFieldDefinitionsP->fFields[i].eqRelevant!=eqm_none;
    // check if available in both items at all
    if (
      fItemTypeP->getFieldOptions(i)->available && // winning
      aItem.fItemTypeP->getFieldOptions(i)->available // loosing
    ) {
      // fields available in both items
      // - get both fields
      TItemField &winningField = getFieldRef(i);
      TItemField &loosingField = aItem.getFieldRef(i);
      // - get assigned status of both fields
      bool winning = winningField.isAssigned();
      bool loosing = loosingField.isAssigned();
      // - now decide what to do
      if (sep!=mem_none) {
        // merge enabled
        PDEBUGPRINTFX(DBG_DATA+DBG_CONFLICT,(
          "Field '%s' available and enabled for merging, mode/sep=0x%04hX, %srelevant",
          fFieldDefinitionsP->fFields[i].TCFG_CSTR(fieldname),
          sep,
          mergerelevant ? "" : "NOT "
        ));
        PDEBUGPRINTFX(DBG_DATA+DBG_CONFLICT,(
          "- %sassigned in winning / %sassigned in loosing",
          winning ? "" : "not ",
          loosing ? "" : "not "
        ));
        // - if both are unassigned -> nop
        if (!winning && !loosing) continue; // test next field
        // - if this item has field unassigned and other has it assigned: copy contents
        else if (!winning && loosing && (sep==mem_fillempty || sep==mem_addunassigned)) {
          // assign loosing item's content to non-assigned winning item
          getFieldRef(i)=aItem.getFieldRef(i);
          #ifdef SYDEBUG
          string ds;
          getFieldRef(i).getAsString(ds);
          PDEBUGPRINTFX(DBG_DATA+DBG_CONFLICT+DBG_USERDATA,(
            "- assigned value '%" FMT_LENGTH(".40") "s' to winning (which had nothing assigned here)",
            FMT_LENGTH_LIMITED(40,ds.c_str())
          ));
          #endif
          // count only if relevant and not assigned empty value
          // (so empty and unassigned are treated equally)
          if (mergerelevant && !aItem.getFieldRef(i).isEmpty())
            aChangedThis=true; // merged something into this item
        }
        else if (winning && loosing) {
          // merge loosing field into winning field
          if (sep==mem_fillempty) {
            // only fill up empty winning fields
            if (winningField.isEmpty() && !loosingField.isEmpty()) {
              // only copy value from loosing if winning is empty
              winningField=loosingField;
              #ifdef SYDEBUG
              string ds;
              winningField.getAsString(ds);
              PDEBUGPRINTFX(DBG_DATA+DBG_CONFLICT+DBG_USERDATA,(
                "- copied value '%" FMT_LENGTH(".40") "s' from loosing to empty winning",
                FMT_LENGTH_LIMITED(40,ds.c_str())
              ));
              #endif
              if (mergerelevant) aChangedThis=true;
            }
          }
          else {
            // try real merge (sep might be 0 (mem_concat) or a separator char)
            #ifdef SYDEBUG
            string ds1,ds2;
            winningField.getAsString(ds1);
            loosingField.getAsString(ds2);
            PDEBUGPRINTFX(DBG_DATA+DBG_CONFLICT+DBG_USERDATA,(
              "- try merging winning value '%" FMT_LENGTH(".40") "s' with loosing value '%" FMT_LENGTH(".40") "s'",
              FMT_LENGTH_LIMITED(40,ds1.c_str()),
              FMT_LENGTH_LIMITED(40,ds2.c_str())
            ));
            #endif
            if (winningField.merge(loosingField,sep))
              aChangedThis=true;
            #ifdef SYDEBUG
            winningField.getAsString(ds1);
            PDEBUGPRINTFX(DBG_DATA+DBG_CONFLICT+DBG_USERDATA,(
              "  merged %sthing, winning value is '%" FMT_LENGTH(".40") "s'",
              aChangedThis ? "some" : "no",
              FMT_LENGTH_LIMITED(40,ds1.c_str())
            ));
            #endif
          }
        }
      } // merge enabled
      // with or without merge, loosing fields must be equal to winning ones
      // - for non merge-relevant (that is, never-compared) fields,
      //   just assign winning value to loosing and do not compare (this
      //   is important to avoid pulling large blobs and strings here -
      //   assignment just passes the proxy)
      if (!mergerelevant) {
        // everything is handled by the field assignment mechanisms
        loosingField = winningField;
      }
      else if (winningField!=loosingField) {
        // merge relevant fields will get more sophisticated treatment, such
        // as checking if a change has occurred and cutoff detection
        #ifdef SYDEBUG
        string wfv,lfv;
        winningField.getAsString(wfv);
        loosingField.getAsString(lfv);
        PDEBUGPRINTFX(DBG_DATA+DBG_CONFLICT+DBG_USERDATA,(
          "Winning and loosing Field '%s' not equal: '%" FMT_LENGTH(".30") "s' <> '%" FMT_LENGTH(".30") "s'",
          fFieldDefinitionsP->fFields[i].TCFG_CSTR(fieldname),
          FMT_LENGTH_LIMITED(30,wfv.c_str()),FMT_LENGTH_LIMITED(30,lfv.c_str())
        ));
        #endif
        // update loosing item, too
        if (loosingField.isShortVers(winningField,fItemTypeP->getFieldOptions(i)->maxsize)) {
          // winning field is short version of loosing field -> loosing field is "better", use it
          winningField=loosingField;
          if (mergerelevant) aChangedThis=true;
        }
        else {
          // standard case, loosing field is replaced by winning field
          loosingField=winningField;
          if (mergerelevant) aChangedOther=true;
        }
        // this is some kind of item-level merge as well
        #ifdef SYDEBUG
        string ds;
        winningField.getAsString(ds);
        PDEBUGPRINTFX(DBG_DATA+DBG_CONFLICT+DBG_USERDATA,(
          "- updated fields such that both have same value '%" FMT_LENGTH(".40") "s'",
          FMT_LENGTH_LIMITED(40,ds.c_str())
        ));
        #endif
      }
    } // field available in both items
  } // field loop
} // TMultiFieldItem::standardMergeWith

#endif // server only

#ifdef SYDEBUG
// show item contents for debug
void TMultiFieldItem::debugShowItem(uInt32 aDbgMask)
{
  TFieldListConfig *fielddefsP = getFieldDefinitions();
  string val,fshow,oloc,orem;
  TFieldOptions *foptP;
  TItemField *fldP;

  if (PDEBUGTEST(aDbgMask|DBG_DETAILS)) {
    // very detailed
    bool hasdata =
      getSyncOp()!=sop_archive_delete &&
      getSyncOp()!=sop_soft_delete &&
      getSyncOp()!=sop_delete &&
      getSyncOp()!=sop_copy &&
      getSyncOp()!=sop_move;
    // - item header info
    PDEBUGPRINTFX(aDbgMask|DBG_DETAILS|DBG_HOT,(
      "Item LocalID='%s', RemoteID='%s', operation=%s%s",
      getLocalID(),
      getRemoteID(),
      SyncOpNames[getSyncOp()],
      hasdata && PDEBUGTEST(aDbgMask|DBG_USERDATA) ? ", size: [maxlocal,maxremote,actual]" : ""
    ));
    if (!hasdata)
      return; // do not show data for delete
    if (!PDEBUGTEST(aDbgMask|DBG_USERDATA)) {
      PDEBUGPRINTFX(aDbgMask|DBG_DETAILS,("*** field data not shown because userdata log is disabled ***"));
      return; // do not show any field data
    }
    // - fields
    fshow.erase();
    for (sInt16 k=0; k<fielddefsP->numFields(); k++) {
      SYSYNC_TRY {
        const TFieldDefinition *fdP = &(fielddefsP->fFields[k]);
        // get options
        oloc="?";
        if (fItemTypeP) {
          foptP=fItemTypeP->getFieldOptions(k);
          if (!foptP->available)
            oloc="n/a";
          else
            StringObjPrintf(oloc,"%ld",(long)foptP->maxsize);
        }
        orem="?";
        if (fTargetItemTypeP) {
          foptP=fTargetItemTypeP->getFieldOptions(k);
          if (!foptP->available)
            orem="n/a";
          else
            StringObjPrintf(orem,"%ld",(long)foptP->maxsize);
        }
        // get value (but prevent pulling proxies)
        fldP=NULL;
        size_t n=0; // empty or unknown size
        if (isAssigned(k)) {
          fldP = getField(k);
          val.erase();
          n = fldP->StringObjFieldAppend(val,99); // get string representation and size
        }
        else
          val="<unassigned>";
        // now show main info
        StringObjAppendPrintf(fshow,
          "- %2d : %10s %-15s [%4s,%4s,%6ld] : %s\n",
          k, // fid
          ItemFieldTypeNames[fdP->type], // field type
          TCFG_CSTR(fdP->fieldname), // field name
          oloc.c_str(),
          orem.c_str(),
          long(n),
          val.c_str()
        );
        // show array contents if any
        if (fldP && fldP->isArray()) {
          int arridx;
          for (arridx=0; arridx<fldP->arraySize(); arridx++) {
            // show array elements
            TItemField *elemP = fldP->getArrayField(arridx,true);
            val.erase();
            elemP->StringObjFieldAppend(val,99);
            StringObjAppendPrintf(fshow,
              "                                     -- element %4d : %s\n",
              arridx,
              val.c_str()
            );
          }
        } // isArray
      }
      SYSYNC_CATCH(exception &e)
        PDEBUGPRINTFX(DBG_ERROR,("Exception when trying to show field fid=%hd: %s[FLUSH]",k,e.what()));
      SYSYNC_ENDCATCH
      SYSYNC_CATCH(...)
        PDEBUGPRINTFX(DBG_ERROR,("Unknown Exception when trying to show field fid=%hd[FLUSH]",k));
      SYSYNC_ENDCATCH
    } // for all fields
    PDEBUGPUTSXX(aDbgMask|DBG_DETAILS|DBG_USERDATA,fshow.c_str(),0,true);
  }
  else if (PDEBUGTEST(aDbgMask)) {
    // single line only
    StringObjPrintf(fshow,
      "Item locID='%s', RemID='%s', op=%s",
      getLocalID(),
      getRemoteID(),
      SyncOpNames[getSyncOp()]
    );
    if (!PDEBUGTEST(aDbgMask|DBG_USERDATA)) {
      fshow+=", *** userdata log disabled ***";
    }
    else if (
      getSyncOp()==sop_archive_delete ||
      getSyncOp()==sop_soft_delete ||
      getSyncOp()==sop_delete
    ) {
      // do not show data for delete
    }
    else {
      fshow+=", Fields: ";
      for (sInt16 k=0; k<fielddefsP->numFields(); k++) {
        if (isAssigned(k)) {
          if (!getField(k)->isEmpty() && !getField(k)->hasProxy()) {
            getField(k)->StringObjFieldAppend(fshow,20);
            fshow+=", ";
          }
        }
      }
    }
    PDEBUGPUTSX(aDbgMask,fshow.c_str());
  }
} // TMultiFieldItem::debugShowItem

#endif

/* end of TMultiFieldItem implementation */


#ifdef DBAPI_TUNNEL_SUPPORT

// TMultiFieldItemKey
// ==================


// set new content item
void TMultiFieldItemKey::setItem(TMultiFieldItem *aItemP, bool aPassOwner)
{
  forgetItem();
  fItemP = aItemP;
  fOwnsItem = aPassOwner;
  fWritten = fItemP; // if we have set an item, this counts as written
} // TMultiFieldItemKey::setItem


// get FID for specified name
sInt16 TMultiFieldItemKey::getFidFor(cAppCharP aName, stringSize aNameSz)
{
  if (!fItemP) return VARIDX_UNDEFINED; // no item, no field is accessible

  TFieldListConfig *flcP = fItemP->getFieldDefinitions();

  // check for iterator commands first
  if (strucmp(aName,VALNAME_FIRST)==0) {
    fIteratorFid = 0;
    if (fIteratorFid<flcP->numFields())
      return fIteratorFid;
  }
  else if (strucmp(aName,VALNAME_NEXT)==0) {
    if (fIteratorFid<flcP->numFields())
      fIteratorFid++;
    if (fIteratorFid<flcP->numFields())
      return fIteratorFid;
  }
  else {
    return flcP->fieldIndex(aName,aNameSz);
  }
  // none found
  return VARIDX_UNDEFINED;
} // TMultiFieldItemKey::getFidFor



TItemField *TMultiFieldItemKey::getBaseFieldFromFid(sInt16 aFid)
{
  if (!fItemP) return NULL; // no item, no field is accessible
  return fItemP->getField(aFid);
} // TMultiFieldItemKey::getBaseFieldFromFid


bool TMultiFieldItemKey::getFieldNameFromFid(sInt16 aFid, string &aFieldName)
{
  if (!fItemP) return false; // no item, no field is accessible
  // name is field name
  TFieldListConfig *flcP = fItemP->getFieldDefinitions();
  if (aFid>=0 && aFid<flcP->numFields()) {
    aFieldName = flcP->fFields[aFid].fieldname;
    return true;
  }
  // none found
  return false;
} // TMultiFieldItemKey::getFieldNameFromFid


#endif // DBAPI_TUNNEL_SUPPORT


} // namespace sysync

// eof
