/*
 *  File:         SyncItemType.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TSyncItemType
 *    Type description and converter (template) for TSyncItem.
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-05-16 : luz : created
 *
 */

// includes
#include "prefix_file.h"

#include "sysync.h"
#include "syncitemtype.h"
#include "syncitem.h"
#include "synccommand.h"
#include "syncsession.h"

#ifdef ZIPPED_BINDATA_SUPPORT
  #include "zlib.h"
#endif

using namespace sysync;


/*
 * Implementation of TSyncItemType
 */

/* TSyncItemType members */

void TSyncItemType::init(
  TSyncSession *aSessionP,
  TDataTypeConfig *aTypeConfigP,
  const char *aCTType,
  const char *aVerCT,
  TSyncDataStore *aRelatedDatastoreP
)
{
  // link to config
  fTypeConfigP=aTypeConfigP;
  // link to session
  fSessionP=aSessionP;
  // set type name and vers (if any)
  fTypeName=aCTType;
  if (aVerCT) fTypeVers=aVerCT;
  // set relation to a specific datastore (if any)
  fRelatedDatastoreP=aRelatedDatastoreP;
  // assume local
  fIsRemoteType = false;

  #if defined(ZIPPED_BINDATA_SUPPORT) && defined(SYDEBUG)
  // data compression accounting
  fRawDataBytes=0;
  fZippedDataBytes=0;
  #endif

} // TSyncItemType::init


TSyncItemType::TSyncItemType(
  TSyncSession *aSessionP,
  TDataTypeConfig *aTypeConfigP,
  const char *aCTType,
  const char *aVerCT,
  TSyncDataStore *aRelatedDatastoreP
)
{
  // only basic init
  init(aSessionP,aTypeConfigP,aCTType,aVerCT,aRelatedDatastoreP);
} // TSyncItemType::TSyncItemType


TSyncItemType::~TSyncItemType()
{
  #if defined(ZIPPED_BINDATA_SUPPORT) && defined(SYDEBUG)
  // show compression statistics, if any
  if (fSessionP && fRawDataBytes && fZippedDataBytes) {
    POBJDEBUGPRINTFX(fSessionP,DBG_HOT,(
      "##### Type '%s', zippedbindata send statistics: raw=%ld, compressed=%ld, compressed down to %ld%%",
      getTypeName(),
      (long)fRawDataBytes,
      (long)fZippedDataBytes,
      (long)(fZippedDataBytes*100/fRawDataBytes)
    ));
  }
  #endif
} // TSyncItemType::~TSyncItemType



// get session zones pointer
GZones *TSyncItemType::getSessionZones(void)
{
  return getSession() ? getSession()->getSessionZones() : NULL;
} // TSyncItemType::getSessionZones


#ifdef SYDEBUG

TDebugLogger *TSyncItemType::getDbgLogger(void)
{
  // commands log to session's logger
  return fSessionP ? getSession()->getDbgLogger() : NULL;
} // TSyncItemType::getDbgLogger

uInt32 TSyncItemType::getDbgMask(void)
{
  if (!fSessionP) return 0; // no session, no debug
  return fSessionP->getDbgMask();
} // TSyncItemType::getDbgMask

#endif




// helper to create same-typed instance via base class
TSyncItemType *TSyncItemType::newCopyForSameType(
  TSyncSession *aSessionP,     // the session
  TSyncDataStore *aDatastoreP  // the datastore
)
{
  // create new itemtype of appropriate derived class type that can handle
  // this type
  MP_RETURN_NEW(TSyncItemType,DBG_OBJINST,"TSyncItemType",TSyncItemType(aSessionP,fTypeConfigP,getTypeName(),getTypeVers(),aDatastoreP));
} // TSyncItemType::newCopyForSameType



// check if type is supported
// - if version specified is NULL, first name-matching flavour is returned
// - if type has no version, it matches any versions given
// - if aVersMustMatch is set, version must match (both w/o version is a match, too)
bool TSyncItemType::supportsType(const char *aName, const char *aVers, bool aVersMustMatch)
{
  if (!aName) return false; // no support for unnamed
  if (!aVers) aVers=""; // empty version
  return (
    strucmp(fTypeName.c_str(),aName)==0 &&
    ( ((*aVers==0 || fTypeVers.empty()) && !aVersMustMatch) || strucmp(fTypeVers.c_str(),aVers)==0 )
  );
} // TSyncItemType::supportsType



bool TSyncItemType::supportsType(SmlDevInfXmitPtr_t aXmitType, bool aVersMustMatch)
{
  if (!aXmitType) return false; // null type not found
  return (
    supportsType(
      smlPCDataToCharP(aXmitType->cttype),
      smlPCDataToCharP(aXmitType->verct),
      aVersMustMatch
    )
  );
} // TSyncItemType::supportsType



/// @brief get CTCap entry
/// @param aOnlyForDS[in]
/// - if NULL, CTCap is generated suitable for all datastores
/// - if not NULL, CTCap is generated specifically for the datastore passed
const SmlDevInfCTCapPtr_t TSyncItemType::getCTCapDevInf(TLocalEngineDS *aOnlyForDS, TTypeVariantDescriptor aVariantDescriptor, bool aWithoutCTCapProps)
{
  SmlDevInfCTCapPtr_t ctcap=NULL;
  // get item type and property description (part of <CTCap>), if any
  // - first see if we have (and are allowed to show) property descriptions at all
  SmlDevInfCTDataPropListPtr_t proplistP = NULL;
  if (fSessionP->fShowCTCapProps && !aWithoutCTCapProps)
    proplistP = newCTDataPropList(aVariantDescriptor);
  // - and if we should report field level replace capability to remote
  bool acceptsFieldLevel = getSession()->getSyncMLVersion()>=syncml_vers_1_2 && canAcceptFieldLevelUpdates();
  if (proplistP || acceptsFieldLevel) {
    // there are properties available for this item, create DevInfCTCap
    ctcap = SML_NEW(SmlDevInfCTCap_t);
    // - add type name
    ctcap->cttype=newPCDataString(getTypeName());
    // - for DS 1.2, add the VerCT
    if (getSession()->getSyncMLVersion()>=syncml_vers_1_2)
      ctcap->verct=newPCDataString(getTypeVers());
    else
      ctcap->verct=NULL;
    // - init the flags
    ctcap->flags= acceptsFieldLevel ? SmlDevInfFieldLevel_f : 0;
    // - add property list
    ctcap->prop=proplistP;
  }
  return ctcap;
} // TSyncItemType::getCTCapDevInf


// intended for creating SyncItemTypes for remote databases from
// transmitted DevInf.
bool TSyncItemType::analyzeCTCap(SmlDevInfCTCapPtr_t aCTCapP)
{
  // Note: derived classes will possibly get some type-related info out of the CTCaps
  return true;
} // TSyncItemType::analyzeCTCap


/// @brief copy CTCap derived info from another SyncItemType
/// @return false if item not compatible
/// @note required to create remote type variants from ruleMatch type alternatives
bool TSyncItemType::copyCTCapInfoFrom(TSyncItemType &aSourceItem)
{
  // no generic CTCap info to copy
  return true;
} // TSyncItemType::copyCTCapInfoFrom



// - static function to search type in a TSyncItemTypePContainer
TSyncItemType *TSyncItemType::findTypeInList(
  TSyncItemTypePContainer &aList,
  const char *aName, const char *aVers,
  bool aVersMustMatch,
  bool aMustBeImplemented,
  TSyncDataStore *aRelatedDatastoreP // if not NULL, type must be specific to this datastore (or unspecific)
)
{
  TSyncItemTypePContainer::iterator pos;
  // first priority: return type which is specific to aRelatedDatastoreP
  for (pos=aList.begin(); pos!=aList.end(); ++pos) {
    if ((*pos)->supportsType(aName,aVers,aVersMustMatch)) {
      // found, return if implementation is ok and related to the correct datastore (or no relation requested)
      if (
        ( !aMustBeImplemented || (*pos)->isImplemented() ) &&
        ( (aRelatedDatastoreP==NULL) || ((*pos)->getRelatedDatastore()==aRelatedDatastoreP) )
      )
        return (*pos); // return it
    }
  }
  if (aRelatedDatastoreP) {
    // second priority: return type which is expressedly not specific to a datastore
    for (pos=aList.begin(); pos!=aList.end(); ++pos) {
      if ((*pos)->supportsType(aName,aVers,aVersMustMatch)) {
        // found, return if implementation is ok and related to the correct datastore
        if (
          (!aMustBeImplemented || (*pos)->isImplemented()) &&
          ((*pos)->getRelatedDatastore()==NULL)
        )
          return (*pos); // return it
      }
    }
  }
  return NULL; // not found
} // static TSyncItemType::findTypeInList


// - static function to search type in a TSyncItemTypePContainer
TSyncItemType *TSyncItemType::findTypeInList(
  TSyncItemTypePContainer &aList,
  SmlDevInfXmitPtr_t aXmitType,  // name and version of type
  bool aVersMustMatch,
  bool aMustBeImplemented,
  TSyncDataStore *aRelatedDatastoreP // if not NULL, type must be specific to THIS datastore (or unspecific)
)
{
  return (
    findTypeInList(
      aList,
      smlPCDataToCharP(aXmitType->cttype),
      smlPCDataToCharP(aXmitType->verct),
      aVersMustMatch,
      aMustBeImplemented,
      aRelatedDatastoreP
    )
  );
} // static TSyncItemType::findTypeInList


// - static function to add new or copied ItemType to passed list
TSyncItemType *TSyncItemType::registerRemoteType(
  TSyncSession *aSessionP,
  SmlDevInfXmitPtr_t aXmitTypeP,              // name and version of type
  TSyncItemTypePContainer &aLocalItemTypes,   // list to look up local types (for reference)
  TSyncItemTypePContainer &aNewItemTypes,      // list to add analyzed types if not already there
  TSyncDataStore *aRelatedDatastoreP
)
{
  return (
    registerRemoteType(
      aSessionP,
      smlPCDataToCharP(aXmitTypeP->cttype),
      smlPCDataToCharP(aXmitTypeP->verct),
      aLocalItemTypes,
      aNewItemTypes,
      aRelatedDatastoreP
    )
  );
} // static TSyncItemType::registerRemoteType


// - static function to add new or copied ItemType to passed list
//   If type is already in aNewItemTypes, nothing will be added
TSyncItemType *TSyncItemType::registerRemoteType(
  TSyncSession *aSessionP,
  const char *aName, const char *aVers,       // name and version of type
  TSyncItemTypePContainer &aLocalItemTypes,   // list to look up local types (for reference) - aRelatedDatastoreP does NOT relate to the types here, as these are local ones
  TSyncItemTypePContainer &aNewItemTypes,     // list to add analyzed types if not already there
  TSyncDataStore *aRelatedDatastoreP          // if NULL, type is not related to a specific (remote!) datastore
)
{
  if (!aName) SYSYNC_THROW(TSyncException("cannot register type w/o name"));
  #ifdef SYDEBUG
  if (aSessionP) {
    // Show warning if no version
    if (!aVers || *aVers==0) {
      POBJDEBUGPRINTFX(aSessionP,DBG_REMOTEINFO,("WARNING: Registering type with no version specification!"));
    }
  }
  #endif
  // - check if type is already in the list (version must match, but implementation not required
  TSyncItemType *newitemtypeP = findTypeInList(aNewItemTypes,aName,aVers,true,false,aRelatedDatastoreP);
  if (newitemtypeP) {
    POBJDEBUGPRINTFX(aSessionP,DBG_REMOTEINFO+DBG_EXOTIC,(
      "Type already registered as '%s' Version='%s'",
      newitemtypeP->getTypeName(),
      newitemtypeP->hasTypeVers() ? newitemtypeP->getTypeVers() : "[none]"
    ));
  }
  else {
    // - check if one of the local item types supports this type
    //   Note: SySync engine only has one global list of supported types. It might contain multiple entries for
    //         the same type, when related to different datastores. The following search will find the first with
    //         matching name - this is no problem as long as all types with same name have the same CLASS type
    //         (as the only reason to look up in local types is to create the correct class' instance). When
    //         actually USED later by datastores, each datastore finds it's own INSTANCES via it's tx/rx(pref).
    //         No need for version match, but for implementation
    TSyncItemType *localitemtypeP = findTypeInList(aLocalItemTypes,aName,aVers,false,true,NULL); // local types are not specific to a datastore
    if (localitemtypeP) {
      // %%% test for conforming version if type being registered has no version spec
      // local itemtype exists that can support this type/vers
      // - create new item type of same class (type-specialized derivate of TSyncItemType)
      // - but relate it to the remote datastore (for DS1.2, or none for DS1.1 and earlier)
      newitemtypeP = localitemtypeP->newCopyForSameType(aSessionP,aRelatedDatastoreP);
    }
    else {
      // no local support of this type
      // - create base class item just describing the type found
      //   (but without handling abilities)
      newitemtypeP = new TSyncItemType(aSessionP,NULL,aName,aVers,aRelatedDatastoreP);
    }
    // this is a remote type!
    newitemtypeP->defineAsRemoteType();
    // add item to list
    aNewItemTypes.push_back(newitemtypeP);
    POBJDEBUGPRINTFX(aSessionP,DBG_REMOTEINFO+DBG_HOT,(
      "Registered Type '%s' Version='%s', %s%s, related to remote datastore '%s'",
      newitemtypeP->getTypeName(),
      newitemtypeP->hasTypeVers() ? newitemtypeP->getTypeVers() : "[none]",
      newitemtypeP->isImplemented() ? "implemented by local type " : "NOT implemented",
      newitemtypeP->isImplemented() ? newitemtypeP->getTypeConfig()->getName() : "",
      aRelatedDatastoreP ? aRelatedDatastoreP->getName() : "<none>"
    ));
    // Only show this if we REALLY have a local type-to-DS relation (NEVER so far - that is in 3.1.2.1)
    if (newitemtypeP->isImplemented() && localitemtypeP && localitemtypeP->getRelatedDatastore()) {
      POBJDEBUGPRINTFX(aSessionP,DBG_REMOTEINFO,(
        "- Implementation is exclusively related to local datastore '%s'",
        localitemtypeP->getRelatedDatastore()->getName()
      ));
    }
  } // if not already in list
  return newitemtypeP;
} // static TSyncItemType::registerRemoteType


/// @brief static function to analyze CTCap and add entries to passed list
/// @todo %%% to be moved to TMimeDirItemType (as basic TSyncItemType does not really know the VERSION property)
bool TSyncItemType::analyzeCTCapAndCreateItemTypes(
  TSyncSession *aSessionP,
  TRemoteDataStore *aRemoteDataStoreP,        ///< if not NULL, this is the datastore to which this type is local (DS 1.2 case)
  SmlDevInfCTCapPtr_t aCTCapP,
  TSyncItemTypePContainer &aLocalItemTypes,   ///< list to look up local types (for reference)
  TSyncItemTypePContainer &aNewItemTypes      ///< list to add analyzed types if not already there
)
{
  bool versfound = false;

  // create one TSyncItemType for each type contained in this CTCap
  if (aCTCapP) {
    // type name
    const char *name = smlPCDataToCharP(aCTCapP->cttype);
    // - from DS 1.2 on, we should have the verct without digging into properties
    const char *vers = smlPCDataToCharP(aCTCapP->verct);
    if (vers && *vers) {
      // we have a non-empty version from verct (new DS 1.2 devinf)
      #ifdef SYDEBUG
      if (aSessionP) { PLOGDEBUGBLOCKFMTCOLL(aSessionP->getDbgLogger(),("RemoteCTCap", "Registering remote Type/Version from >=DS 1.2 style CTCap", "type=%s|version=%s", name, vers)); }
      #endif
      // valenum shows version supported
      TSyncItemType *newitemtypeP =
        registerRemoteType(
          aSessionP,
          name,vers,         // name and version of type
          aLocalItemTypes,   // list to look up local types (for reference)
          aNewItemTypes,     // list to add it to
          aRemoteDataStoreP
        );
      // now let new item process the CTCap
      if (newitemtypeP) newitemtypeP->analyzeCTCap(aCTCapP);
      #ifdef SYDEBUG
      if (aSessionP) { PLOGDEBUGENDBLOCK(aSessionP->getDbgLogger(),"RemoteCTCap"); }
      #endif
      versfound=true;
    }
    else {
      // - pre-DS 1.2 hack: try to find VERSION property
      SmlDevInfCTDataPropListPtr_t prlP = aCTCapP->prop;
      while (prlP) {
        // get property name
        const char *n = smlPCDataToCharP(prlP->data->prop->name);
        if (n && strcmp(n,"VERSION")==0) {
          // version property found. Now create flavour for each ValEnum
          SmlPcdataListPtr_t velP = prlP->data->prop->valenum;
          while (velP) {
            vers = smlPCDataToCharP(velP->data);
            if (vers) {
              versfound=true; // found at least one
              #ifdef SYDEBUG
              if (aSessionP) { PLOGDEBUGBLOCKFMTCOLL(aSessionP->getDbgLogger(),("RemoteCTCap", "Registering remote Type/Version from old style CTCap w/o verct", "type=%s|version=%s", name, vers)); }
              #endif
              // valenum shows version supported
              TSyncItemType *newitemtypeP =
                registerRemoteType(
                  aSessionP,
                  name,vers,         // name and version of type
                  aLocalItemTypes,   // list to look up local types (for reference)
                  aNewItemTypes,     // list to add it to
                  aRemoteDataStoreP
                );
              // now let new item process the CTCap
              if (newitemtypeP) newitemtypeP->analyzeCTCap(aCTCapP);
              #ifdef SYDEBUG
              if (aSessionP) { PLOGDEBUGENDBLOCK(aSessionP->getDbgLogger(),"RemoteCTCap"); }
              #endif
            } // if version is not null
            // next
            velP=velP->next;
          } // while valenums
          break; // VERSION found, done with properties for now
        } // while properties
        prlP=prlP->next;
      } // while
    }
    if (!versfound) {
      // version is NULL, for a start, create type w/o version
      #ifdef SYDEBUG
      if (aSessionP) { PLOGDEBUGBLOCKFMT(aSessionP->getDbgLogger(),("RemoteCTCap", "Registering remote Type w/o version from CTCap", "type=%s|version=[none]", name)); }
      #endif
      TSyncItemType *newitemtypeP =
        registerRemoteType(
          aSessionP,
          name,NULL,         // name, but no version
          aLocalItemTypes,   // list to look up local types (for reference)
          aNewItemTypes,     // list to add it to
          aRemoteDataStoreP
        );
      // now let new item process the CTCap
      if (newitemtypeP) newitemtypeP->analyzeCTCap(aCTCapP);
      #ifdef SYDEBUG
      if (aSessionP) { PLOGDEBUGENDBLOCK(aSessionP->getDbgLogger(),"RemoteCTCap"); }
      #endif
      // this is ok, too
      return true;
    } // without version
  }
  return versfound;
} // static TSyncItemType::analyzeCTCapAndCreateItemTypes



// get type as transmit format
SmlDevInfXmitPtr_t TSyncItemType::newXMitDevInf(void)
{
  SmlDevInfXmitPtr_t xmitP;

  xmitP = SML_NEW(SmlDevInfXmit_t);
  xmitP->cttype=newPCDataString(getTypeName());
  xmitP->verct=newPCDataString(getTypeVers());

  return xmitP;
} // TSyncItemType::newXMitDevInf


// static helper: get type list as transmit format
SmlDevInfXmitListPtr_t TSyncItemType::newXMitListDevInf(
  TSyncItemTypePContainer &aTypeList,
  TSyncItemType *aDontIncludeP
)
{
  SmlDevInfXmitListPtr_t resultP = NULL;
  SmlDevInfXmitListPtr_t *listPP = &resultP;

  TSyncItemTypePContainer::iterator pos;
  for (pos=aTypeList.begin(); pos!=aTypeList.end(); ++pos) {
    if (aDontIncludeP!=(*pos)) { // only if item in list does not match "except" item
      // new list item
      (*listPP) = SML_NEW(SmlDevInfXmitList_t);
      (*listPP)->next=NULL;
      // add type
      (*listPP)->data=(*pos)->newXMitDevInf();
      // next
      listPP=&((*listPP)->next);
    }
  }
  return resultP;
} // TSyncItemType::newXMitListDevInf


// create new empty sync item
TSyncItem *TSyncItemType::newSyncItem(
  TSyncItemType *aTargetItemTypeP,   // the targeted type (for optimizing field lists etc.)
  TLocalEngineDS *aLocalDataStoreP   // the datastore
)
{
  if (!aLocalDataStoreP) {
    PDEBUGPRINTFX(DBG_ERROR,("Trying to call newSyncItem w/o datastore"));
    return NULL;
  }
  // test if implemented at all
  if (!isImplemented()) {
    PDEBUGPRINTFX(DBG_ERROR,(
      "Tried to create SyncItem for unimplemented SyncItemType '%s' (%s)",
      fTypeName.c_str(),
      fTypeVers.c_str()
    ));
    return NULL;
  }
  // check for compatibility with targeted type
  if (!isCompatibleWith(aTargetItemTypeP)) {
    PDEBUGPRINTFX(DBG_ERROR,(
      "Local type (%s) is not assignment compatible with remote target type (%s) - probably multiple types with same name/version but different fieldlists in config",
      getTypeConfig()->getName(),aTargetItemTypeP-> getTypeConfig()->getName()
    ));
    return NULL;
  }
  // get appropriate item
  TSyncItem *syncitemP = internalNewSyncItem(aTargetItemTypeP,aLocalDataStoreP);
  // return new item (if any)
  return syncitemP;
} // TSyncItemType::newSyncItem


// create new sync item from SyncML data
TSyncItem *TSyncItemType::newSyncItem(
  SmlItemPtr_t aItemP,              // SyncML toolkit item Data to be converted into SyncItem
  TSyncOperation aSyncOp,           // the operation to be performed with this item
  TFmtTypes aFormat,                // the format (normally fmt_chr)
  TSyncItemType *aTargetItemTypeP,  // the targeted type (for optimizing field lists etc.)
  TLocalEngineDS *aLocalDataStoreP, // local datastore
  TStatusCommand &aStatusCmd        // status command that might be modified in case of error
)
{
  TSyncItem *syncitemP = NULL;
  // test if implemented at all
  if (!isImplemented()) {
    DEBUGPRINTFX(DBG_ERROR,(
      "Tried to create SyncItem for unimplemented SyncItemType '%s' (%s)",
      fTypeName.c_str(),
      fTypeVers.c_str()
    ));
    aStatusCmd.setStatusCode(415);
    ADDDEBUGITEM(aStatusCmd,"Known, but unimplemented Item Type");
    return NULL;
  }
  PDEBUGBLOCKFMT(("Item_Parse","parsing SyncML item",
    "SyncOp=%s|format=%s|LocalID=%s|RemoteID=%s",
    SyncOpNames[aSyncOp],
    encodingFmtNames[aFormat],
    smlSrcTargLocURIToCharP(aItemP->target),
    smlSrcTargLocURIToCharP(aItemP->source)
  ));
  SYSYNC_TRY {
    // get appropriate item type
    syncitemP = internalNewSyncItem(aTargetItemTypeP,aLocalDataStoreP);
    // get local and remote IDs
    if (syncitemP) {
      // set operation type
      syncitemP->setSyncOp(aSyncOp);
      // an appropriate syncitem was created, set target and source now
      // - we have received this item from remote, so target=myself, source=remote party
      syncitemP->setLocalID(relativeURI(smlSrcTargLocURIToCharP(aItemP->target)));
      #ifdef DONT_STRIP_PATHPREFIX_FROM_REMOTEIDS
      syncitemP->setRemoteID(smlSrcTargLocURIToCharP(aItemP->source));
      #else
      syncitemP->setRemoteID(relativeURI(smlSrcTargLocURIToCharP(aItemP->source)));
      #endif
      PDEBUGPRINTFX(DBG_DATA+DBG_DETAILS,(
        "Created new item of datatype '%s', localID='%s' remoteID='%s'",
        getTypeConfig()->getName(),
        syncitemP->getLocalID(),
        syncitemP->getRemoteID()
      ));
      if (aSyncOp!=sop_delete && aSyncOp!=sop_archive_delete && aSyncOp!=sop_soft_delete && aSyncOp!=sop_copy && aSyncOp!=sop_move) {
        // Item has data, parse it
        // - uncompress data first if zippedbindata selected in type
        #ifdef ZIPPED_BINDATA_SUPPORT
        if (fTypeConfigP->fZippedBindata && fSessionP->getEncoding()==SML_WBXML && fSessionP->getSyncMLVersion()>=syncml_vers_1_1) {
          // this type uses zipped bindata and we have WBXML (we cannot use zipped bindata in XML)
          // - get input data
          MemPtr_t zipBinPayload = NULL;
          MemSize_t zipBinSize = 0;
          MemPtr_t expandedPayload = NULL;
          sInt32 expandedSize = 0;
          if (aItemP->data) {
            if ((zipBinSize=aItemP->data->length)>0) {
              zipBinPayload = (MemPtr_t) aItemP->data->content;
            }
          }
          // uncompress it, if we have data at all
          if (zipBinPayload) {
            // - <item><meta><maxobjsize> contains expanded size of payload for pre-allocating the buffer
            SmlMetInfMetInfPtr_t metaP = smlPCDataToMetInfP(aItemP->meta);
            if (metaP && metaP->maxobjsize) {
              smlPCDataToLong(metaP->maxobjsize, expandedSize);
            }
            if (expandedSize>0) {
              // we have data to expand AND we know how big the output will be (if we did not get MaxObjSize, this
              // means that the data is not compressed
              expandedPayload = (MemPtr_t) smlLibMalloc(expandedSize+1); // we need one more for the terminator
              // uncompress the <data> payload with gzip
              z_stream zipstream;
              // - no special alloc
              zipstream.zalloc=NULL;
              zipstream.zfree=NULL;
              zipstream.opaque=NULL;
              // - no input yet
              zipstream.next_in=NULL;
              zipstream.avail_in=0;
              // - init deflate
              inflateInit2(&zipstream,15+32); // 15=default window size, +16=sets gzip detect flag (+32: sets gzip+zlib detect flag)
              // - actually inflate item's <data>...
              zipstream.next_in=zipBinPayload;
              zipstream.avail_in=zipBinSize;
              // - ...into new buffer
              zipstream.next_out=expandedPayload;
              zipstream.avail_out=expandedSize;
              int err = inflate(&zipstream,Z_SYNC_FLUSH);
              // - replace compressed by expanded data if everything's fine
              if (err==Z_OK && zipstream.avail_in==0) {
                // make sure data is null terminated
                expandedPayload[expandedSize]=0;
                // set expanded data in place of compressed
                aItemP->data->length=expandedSize;
                aItemP->data->content=expandedPayload;
                // forget compressed data
                smlLibFree(zipBinPayload);
              }
              else {
                // if failed, get rid of unneeded buffer
                smlLibFree(expandedPayload);
              }
              // clean up zip decompressor
              inflateEnd(&zipstream);
            } // if expected data size is known (from maxobjsize), i.e. was sent compressed
          } // if input data available at all
        } // if zippedBinData enabled
        #endif
        // convert payload (as a whole) from known format encodings
        if (aFormat==fmt_b64) {
          MemSize_t origSize = aItemP->data->length;
          cAppCharP origData = (cAppCharP)aItemP->data->content;
          if (origSize) {
            // something to decode, do it and replace original content
            aItemP->data->content = b64::decode(origData, origSize, (uInt32 *)&(aItemP->data->length));
            // we don't need the original data any more
            b64::free((void *)origData);
          }
        }
        // convert payload (as a whole) from UTF16 (Unicode) to UTF-8
        if (fTypeConfigP->fUseUTF16) {
          // get original size
          MemSize_t origSize = aItemP->data->length;
          cAppCharP origData = (cAppCharP)aItemP->data->content;
          if (origSize) {
            // we usually don't need more memory than the original
            string utf8Payload;
            // now convert
            appendUTF16AsUTF8(
              (const uInt16 *)origData,
              origSize/2,
              fTypeConfigP->fMSBFirst,
              utf8Payload,
              false, false
            );
            // replace contents
            if (MemSize_t(utf8Payload.size())<origSize) origSize=utf8Payload.size();
            // copy into existing contents, as UTF-8 is usually smaller
            memcpy((void *)aItemP->data->content,utf8Payload.c_str(),origSize+1); // include terminator byte in copy
            aItemP->data->length=origSize;
          }
        }
        // fill in data, if any (virtual method implemented in descendant)
        if (!internalFillInData(syncitemP,aItemP,aLocalDataStoreP,aStatusCmd)) {
          // delete item, as it could not be filled properly
          delete syncitemP;
          PDEBUGPRINTFX(DBG_ERROR,("Could not fill item -> immediately deleted, none returned"));
          syncitemP=NULL; // none any more
        }
      }
    }
    PDEBUGENDBLOCK("Item_Parse");
  }
  SYSYNC_CATCH (...)
    PDEBUGENDBLOCK("Item_Parse");
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
  // return new item (if any)
  return syncitemP;
} // TSyncItemType::newSyncItem


// - create new SyncML toolkit item from SyncItem
SmlItemPtr_t TSyncItemType::newSmlItem(
  TSyncItem *aSyncItemP,   // the syncitem to be represented as SyncML
  TLocalEngineDS *aLocalDatastoreP // local datastore
)
{
  SmlItemPtr_t smlitemP = NULL;
  PDEBUGBLOCKFMT(("Item_Generate","generating SyncML item",
    "SyncOp=%s|LocalID=%s|RemoteID=%s",
    SyncOpNames[aSyncItemP->getSyncOp()],
    aSyncItemP->getLocalID(),
    aSyncItemP->getRemoteID()
  ));
  SYSYNC_TRY {
    // allocate an empty smlItem
    smlitemP = newItem();
    // data only if not delete, copy or map
    TSyncOperation syncop = aSyncItemP->getSyncOp();
    if (syncop!=sop_delete && syncop!=sop_archive_delete && syncop!=sop_soft_delete && syncop!=sop_copy) {
      // let virtual method implemented in descendant fill in data and, possibly, meta.
      if (!internalSetItemData(aSyncItemP,smlitemP,aLocalDatastoreP)) {
        SYSYNC_THROW(TSyncException("newSmlItem: internalSetItemData() failed"));
      }
      // convert payload (as a whole) to UTF16 (Unicode)
      if (fTypeConfigP->fUseUTF16) {
        string utf16bytestream;
        appendUTF8ToUTF16ByteString(
          (cAppCharP)smlitemP->data->content,
          utf16bytestream,
          fTypeConfigP->fMSBFirst,
          lem_none,
          0
        );
        // dispose old data
        smlLibFree((appPointer)smlitemP->data->content);
        // create new data block
        smlitemP->data->content= (void*)( (const char *)smlLibMalloc(utf16bytestream.size()+2) );
        smlitemP->data->length = utf16bytestream.size();
        // copy contents
        memcpy((appPointer)smlitemP->data->content,utf16bytestream.c_str(),utf16bytestream.size());
      }
      // compress data if zippedbindata selected in type
      #ifdef ZIPPED_BINDATA_SUPPORT
      if (fTypeConfigP->fZippedBindata && fSessionP->getEncoding()==SML_WBXML && fSessionP->getSyncMLVersion()>=syncml_vers_1_1) {
        // this type uses zipped bindata and we have WBXML (we cannot use zipped bindata in XML)
        // compress the <data> payload with gzip if there IS any data
        MemSize_t expandedSize = 0;
        MemPtr_t expandedPayload = NULL;
        MemPtr_t zipBinPayload = NULL;
        // - get expanded data
        if (smlitemP->data) {
          if ((expandedSize=smlitemP->data->length)>0) {
            expandedPayload = (MemPtr_t) smlitemP->data->content;
          }
        }
        if (expandedPayload) {
          // there is data to send - zip it and send it as binary
          // - assume output will not be bigger than input
          zipBinPayload = (MemPtr_t) smlLibMalloc(expandedSize);
          if (zipBinPayload) {
            // compress the <data> payload with gzip
            z_stream zipstream;
            // - no special alloc
            zipstream.zalloc=NULL;
            zipstream.zfree=NULL;
            zipstream.opaque=NULL;
            // - no input yet
            zipstream.next_in=NULL;
            zipstream.avail_in=0;
            // - init deflate
            int comprLevel = fTypeConfigP->fZipCompressionLevel;
            if (comprLevel>9 || comprLevel<0)
              comprLevel=Z_DEFAULT_COMPRESSION;
            deflateInit(&zipstream,comprLevel);
            // - actually deflate item's <data>...
            zipstream.next_in=expandedPayload;
            zipstream.avail_in=expandedSize; // not more than uncompressed version would take
            // - ...into new buffer
            zipstream.next_out=zipBinPayload;
            zipstream.avail_out=expandedSize;
            int err = deflate(&zipstream,Z_SYNC_FLUSH);
            // - replace compressed by expanded data if everything's fine
            if (err==Z_OK && zipstream.avail_in==0) {
              // compression ok, set compressed data in place of original
              smlitemP->data->length=zipstream.total_out;
              smlitemP->data->content=zipBinPayload;
              // forget original data
              smlLibFree(expandedPayload);
              // only if we have succeeded, we will set the maxobjsize and format Otherwise, we'll send uncompressed data
              SmlMetInfMetInfPtr_t metaP = smlPCDataToMetInfP(smlitemP->meta);
              if (!metaP) {
                // we have no meta yet at all, create it first
                smlitemP->meta = newMeta();
                metaP = (SmlMetInfMetInfPtr_t)(smlitemP->meta->content);
              }
              // - store expanded size in meta maxobjsize
              if (metaP->maxobjsize)
                smlFreePcdata(metaP->maxobjsize); // delete if there is already something here
              metaP->maxobjsize=newPCDataLong(expandedSize);
              // - set format to "bin" as a flag that contents are compressed
              if (metaP->format)
                smlFreePcdata(metaP->format); // delete if there is already something here
              metaP->format=newPCDataString("bin");
            }
            else {
              // if failed, get rid of unneeded buffer
              smlLibFree(zipBinPayload);
            }
            // update statistics
            #ifdef SYDEBUG
            POBJDEBUGPRINTFX(fSessionP,DBG_DATA,(
              "zippedbindata item statistics: raw=%ld, compressed=%ld, compressed down to %ld%%",
              expandedSize,
              smlitemP->data->length,
              smlitemP->data->length*100/expandedSize
            ));
            fRawDataBytes+=expandedSize;
            fZippedDataBytes+=smlitemP->data->length;
            #endif
            // clean up zip compressor
            deflateEnd(&zipstream);
          } // can allocate
        } // if any payload at all
      }
      #endif
    }
    // set source and target (AFTER setting data, as item data might influence item ID
    // in some special cases (as for Nokia 9500-style email)
    // - we will send this item to remote, so target=remote party, source=myself
    smlitemP->target=newOptLocation(aSyncItemP->getRemoteID());
    smlitemP->source=newOptLocation(aSyncItemP->getLocalID());
    PDEBUGENDBLOCK("Item_Generate");
  }
  SYSYNC_CATCH (...)
    PDEBUGENDBLOCK("Item_Generate");
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
  // return smlItem
  return smlitemP;
} // TSyncItemType::newSmlItem



/* end of TSyncItemType implementation */

// eof
