/*
 *  File:         DataObjType.h
 *
 *  Author:       Beat Forster (bfo@synthesis.ch)
 *
 *  TDataObjType
 *    base class for data object based items (EMAILOBJ, FILEOBJ, FOLDEROBJ)
 *    implemented for OMA DS V1.2
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 *  2005-07-20 : bfo : created from TextItemType
 *
 */

#ifndef DataObjType_H
#define DataObjType_H

// includes
#include "syncitemtype.h"
#include "multifielditemtype.h"
#include "textprofile.h"

// for test
//#define HARDCODED_TYPE_SUPPORT


namespace sysync {

// property definition
class TTagMapDefinition : public TConfigElement {
  typedef TConfigElement inherited;
public:
  // constructor/destructor
  TTagMapDefinition(TConfigElement *aParentElementP, sInt16 aFid);
  virtual ~TTagMapDefinition();
  // properties

  // - tagged field
  TCFG_STRING fXmlTag;
  // - attribute field of tag
  TCFG_STRING fXmlAttr;

  // - specifies the field as boolean
  bool fBoolType;
  // - specifies an embedded profile (for RFC2822)
  bool fEmbedded;

  // - the expected parent tag (if not empty)
  TCFG_STRING fParent;

  // - field id
  sInt16 fFid;

  #ifdef OBJECT_FILTERING
  // - no filterkeyword
  TCFG_STRING fFilterKeyword;
  #endif

  #ifdef CONFIGURABLE_TYPE_SUPPORT
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif

  virtual void clear();
}; // TTagMapDefinition


// tag mapping definitions
typedef std::list<TTagMapDefinition *> TTagMapList;

// Text based datatype
class TDataObjConfig : public TMultiFieldTypeConfig
{
  typedef TMultiFieldTypeConfig inherited;
public:
  TDataObjConfig(const char *aElementName, TConfigElement *aParentElementP);
  virtual ~TDataObjConfig();
  // properties
  // - Note, field list is parsed here, but is a property of TMultiFieldTypeConfig
  // - text-line based field definitions
  TTagMapList fTagMaps;

  // public functions
  // - create Sync Item Type of appropriate type from config
  virtual TSyncItemType *newSyncItemType(TSyncSession *aSessionP, TSyncDataStore *aDatastoreP);

  #ifdef HARDCODED_TYPE_SUPPORT
  TTagMapDefinition *addTagMap( sInt16 aFid, const char* aXmlTag,
                                bool aBoolType, bool aEmbedded,
                                const char* aParent );
  #endif

protected:
  #ifdef CONFIGURABLE_TYPE_SUPPORT
    // check config elements
    virtual bool localStartElement( const char *aElementName,
                                    const char **aAttributes, sInt32 aLine );
    virtual void localResolve( bool aLastPass );
  #endif

  virtual void clear();
}; // TDataObjConfig


const uInt16 ity_dataobj= 200; // must be unique

class TDataObjType: public TMultiFieldItemType
{
  typedef TMultiFieldItemType inherited;
public:
  // constructor
  TDataObjType(
    TSyncSession *aSessionP,
    TDataTypeConfig *aTypeCfgP, // type config
    const char *aCTType,
    const char *aVerCT,
    TSyncDataStore *aRelatedDatastoreP,
    TFieldListConfig *aFieldDefinitions // field definitions
  );
  // destructor
  virtual ~TDataObjType();
  // access to type
  virtual uInt16 getTypeID(void) const { return ity_dataobj; };
  virtual bool isBasedOn(uInt16 aItemTypeID) const { return aItemTypeID==ity_dataobj ? true : TMultiFieldItemType::isBasedOn(aItemTypeID); };
  // differentiation between implemented and just descriptive TSyncTypeItems
  virtual bool isImplemented(void) { return true; }; // MIME-DIR is an implementation
  // helper to create same-typed instance via base class
  // MUST BE IMPLEMENTED IN ALL DERIVED CLASSES!
  virtual TSyncItemType *newCopyForSameType(
    TSyncSession *aSessionP,     // the session
    TSyncDataStore *aDatastoreP  // the datastore
  );
  /// @brief copy CTCap derived info from another SyncItemType
  virtual bool copyCTCapInfoFrom(TSyncItemType &aSourceItem);
  #ifdef OBJECT_FILTERING
  // filtering
  // - get field index of given filter expression identifier.
  virtual sInt16 getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex);
  #endif
protected:
  // - analyze CTCap for specific type
  virtual bool analyzeCTCap(SmlDevInfCTCapPtr_t aCTCapP);
  // - obtain property list for type, returns NULL if none available
  virtual SmlDevInfCTDataPropListPtr_t newCTDataPropList(TTypeVariantDescriptor aVariantDescriptor);
  // - Filtering: add keywords and property names to filterCap
  virtual void addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps, TTypeVariantDescriptor aVariantDesc);
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

  // get attribute <aAttr> of <aXmlTag>. If not available, return ""
  virtual string getAttr( TMultiFieldItem &aItem, const char* aXmlTag, const char* aXmlAttr );

  // generate Data item (includes header and footer)
  virtual void generateData(TLocalEngineDS *aDatastoreP, TMultiFieldItem &aItem, string &aString);
  // parse Data item (includes header and footer)
  virtual bool parseData(const char *aText, stringSize aSize, TMultiFieldItem &aItem, TLocalEngineDS *aDatastoreP);
private:
  // member fields
  TProfileHandler *fProfileHandlerP;
  TDataObjConfig *fTypeCfgP; // the text type config element
}; // TDataObjType


} // namespace sysync

#endif  // DataObjType_H

// eof
