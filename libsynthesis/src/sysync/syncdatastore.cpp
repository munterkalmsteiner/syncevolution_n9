/*
 *  File:         SyncDataStore.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TSyncDataStore
 *    <describe here>
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-xx-xx : luz : created
 *
 */

// includes
#include "prefix_file.h"

#include "sysync.h"

#include "syncdatastore.h"
#include "syncsession.h"



using namespace sysync;


/*
 * Implementation of TSyncDataStore
 */


void TSyncDataStore::init(TSyncSession *aSessionP)
{
  // save link to session
  fSessionP=aSessionP;
  // no sync capabilities by default
  fCommonSyncCapMask=0;
  // name not known
  fName="[unknown]";
  // no types
  fRxPrefItemTypeP = NULL;
  fTxPrefItemTypeP = NULL;
  // no max GUID size yet
  fMaxGUIDSize = 0;
  // unlimited (that is, maximum possible with longlong) size and ID
  #ifdef __BORLANDC__
  fMaxMemory = numeric_limits<longlong>::max();
  fFreeMemory = fMaxMemory;
  fMaxID = numeric_limits<longlong>::max();
  fFreeID = fMaxID;
  #elif defined(LINUX) || defined(WINCE) || defined(_MSC_VER) || defined(__EPOC_OS__)
  fMaxMemory = LONG_MAX;
  fFreeMemory = fMaxMemory;
  fMaxID = LONG_MAX;
  fFreeID = fMaxID;
  #else
  fMaxMemory = numeric_limits<__typeof__(fMaxMemory)>::max();
  fFreeMemory = fMaxMemory;
  fMaxID = numeric_limits<__typeof__(fMaxID)>::max();
  fFreeID = fMaxID;
  #endif
  fCanRestart = false;
  // rest ist like reset
  InternalResetDataStore();
} // TSyncDataStore::init


TSyncDataStore::TSyncDataStore(TSyncSession *aSessionP)
{
  init(aSessionP);
} // TSyncDataStore::TSyncDataStore


TSyncDataStore::TSyncDataStore(TSyncSession *aSessionP, const char *aName, uInt32 aCommonSyncCapMask)
{
  // basic init
  init(aSessionP);
  // make sure we have SyncML minimal Caps, save it
  fCommonSyncCapMask=aCommonSyncCapMask | SCAP_MASK_MINIMAL;
  // save name of datastore
  fName = aName;
} // TSyncDataStore::TSyncDataStore


void TSyncDataStore::setPreferredTypes(TSyncItemType *aRxItemTypeP, TSyncItemType *aTxItemTypeP)
{
  // save standard Rx and Tx preferences
  fRxPrefItemTypeP = aRxItemTypeP;
  fTxPrefItemTypeP = aTxItemTypeP ? aTxItemTypeP : aRxItemTypeP;
  // add them to the tx/rx lists
  fRxItemTypes.push_back(fRxPrefItemTypeP);
  fTxItemTypes.push_back(fTxPrefItemTypeP);
} // TSyncDataStore::setPreferredTypes


void TSyncDataStore::InternalResetDataStore(void)
{
  // empty for now
} // TSyncDataStore::InternalResetDataStore


TSyncDataStore::~TSyncDataStore()
{
  InternalResetDataStore();
} // TSyncDataStore::~TSyncDataStore



// - returns session zones
GZones *TSyncDataStore::getSessionZones(void)
{
  return fSessionP->getSessionZones();
} // TSyncDataStore::getSessionZones


#ifdef SYDEBUG

TDebugLogger *TSyncDataStore::getDbgLogger(void)
{
  // commands log to session's logger
  return fSessionP ? fSessionP->getDbgLogger() : NULL;
} // TSyncDataStore::getDbgLogger

uInt32 TSyncDataStore::getDbgMask(void)
{
  if (!fSessionP) return 0; // no session, no debug
  return fSessionP->getDbgMask();
} // TSyncDataStore::getDbgMask

#endif


TSyncAppBase *TSyncDataStore::getSyncAppBase(void)
{
  return fSessionP ? fSessionP->getSyncAppBase() : NULL;
} // TSyncDataStore::getSyncAppBase



// check if this datastore is accessible with given URI
// NOTE: URI might include path elements or CGI params that are
// access options to the database; derived classes might
// therefore base identity check on more than simple name match
uInt16 TSyncDataStore::isDatastore(const char *aDatastoreURI)
{
  // base class only implements case insensitive name comparison
  return strucmp(fName.c_str(),aDatastoreURI)==0 ? fName.size() : 0;
} // TSyncDataStore::isDatastore


/// @brief checks if specified type is used by this datastore
/// @return true if type is used by this datastore
/// @param aSyncItemType[in] type to be checked for being used
/// @param aVariantDescP[out] if not NULL, will receive the variant descriptor associated with that use
bool TSyncDataStore::doesUseType(TSyncItemType *aSyncItemType, TTypeVariantDescriptor *aVariantDescP)
{
  // true if either used for send or for receive
  TSyncItemType *rx, *tx;
  rx=getReceiveType(aSyncItemType);
  tx=getSendType(aSyncItemType);
  // determine usage variant if needed
  if (aVariantDescP) {
    *aVariantDescP = getVariantDescForType(aSyncItemType);
  }
  // used if used as rx or tx
  return (rx!=NULL || tx!=NULL);
} // TSyncDataStore::doesUseType


// - returns type that this datastore can use to send data to specified datastore
//   Note: if possible, use preferred Rx type of specified datastore, if this
//         is not possible, use preferred Tx type of this datastore, else
//         use any matching pair.
TSyncItemType *TSyncDataStore::getTypesForTxTo(
  TSyncDataStore *aDatastoreP, // usually remote datastore
  TSyncItemType **aCorrespondingTypePP
)
{
  TSyncItemTypePContainer::iterator pos;
  TSyncItemType *corrP;
  TSyncItemType *txtypeP=NULL, *corrtypeP=NULL; // none found so far
  TSyncItemType *preftxP=NULL, *preftxcorrP=NULL; // none found so far

  // if no datastore there: none
  if (!aDatastoreP) return NULL;
  // get common type for transmitting TO specified datastore
  // - loop through all send types of myself
  for (pos=fTxItemTypes.begin(); pos!=fTxItemTypes.end(); ++pos) {
    // - check if remote has this type
    if ((corrP=aDatastoreP->getReceiveType(*pos))!=NULL) {
      // found matching type
      // - save pointers (we found at least one matching type)
      txtypeP=*pos; // TSyncItemType of this datastore
      corrtypeP=corrP; // TSyncItemType of other datastore
      // - check if preferred by remote
      if (aDatastoreP->getPreferredRxItemType()==corrtypeP) {
        // yes, this is best case, save it and stop searching
        preftxP=txtypeP;
        preftxcorrP=corrtypeP;
        break; // done
      }
      // - if found, but not preferred by remote, check if its our own preference
      if (getPreferredTxItemType()==txtypeP) {
        // remember in case we don't find the remote's preferred type
        preftxP=txtypeP;
        preftxcorrP=corrtypeP;
        // but continue because we might find remote's preferred
      }
    }
  }
  // Now txtypeP/corrtypeP are NULL if no match found
  // Now preftxP/=preftxcorrP are NULL if no preferred match found
  // check if we found a preferred
  if (preftxP) {
    // use it instead of last found (or NULL if none found)
    txtypeP=preftxP;
    corrtypeP=preftxcorrP;
  }
  // return send type
  if (aCorrespondingTypePP) *aCorrespondingTypePP = corrtypeP; // corresponding TSyncItemType of other datastore
  return txtypeP;
} // TSyncDataStore::getTypesForTxTo


// - returns type that this datastore can use to receive data from specified datastore
TSyncItemType *TSyncDataStore::getTypesForRxFrom(
  TSyncDataStore *aDatastoreP, // usually remote datastore
  TSyncItemType **aCorrespondingTypePP
)
{
  TSyncItemTypePContainer::iterator pos;
  TSyncItemType *rxtypeP=NULL, *corrP;

  // get common type for receiving FROM specified datastore
  for (pos=fRxItemTypes.begin(); pos!=fRxItemTypes.end(); ++pos) {
    // loop through all receive types of myself
    if ((corrP=aDatastoreP->getSendType(*pos))!=NULL) {
      // found
      rxtypeP=*pos;
      if (aCorrespondingTypePP) *aCorrespondingTypePP = corrP; // corresponding TSyncItemType of other datastore
      break;
    }
  }
  // return receive type
  return rxtypeP;
} // TSyncDataStore::getTypesForRxFrom



// get datastore's SyncItemType for receive of specified type, NULL if none
TSyncItemType *TSyncDataStore::getReceiveType(const char *aType, const char *aVers)
{
  return (
    TSyncItemType::findTypeInList(
      fRxItemTypes,
      aType,
      aVers,
      false, // version doesn't need to match
      true, // but we want an implemented type
      NULL // no datastore link comparison
    )
  );
} // TSyncDataStore::getReceiveType


// get datastore's SyncItemType for receive of specified type, NULL if none
TSyncItemType *TSyncDataStore::getReceiveType(TSyncItemType *aSyncItemTypeP)
{
  return (
    getReceiveType (
      aSyncItemTypeP->getTypeName(),
      aSyncItemTypeP->getTypeVers()
    )
  );
} // TSyncDataStore::getReceiveType


// get datastore's SyncItemType for send of specified type, NULL if none
TSyncItemType *TSyncDataStore::getSendType(const char *aType, const char *aVers)
{
  return (
    TSyncItemType::findTypeInList(
      fTxItemTypes,
      aType,
      aVers,
      false, // version doesn't need to match
      true, // but we want an implemented type
      NULL // no datastore link comparison
    )
  );
} // TSyncDataStore::getSendType


// get datastore's SyncItemType for send of specified type, NULL if none
TSyncItemType *TSyncDataStore::getSendType(TSyncItemType *aSyncItemTypeP)
{
  return (
    getSendType(
      aSyncItemTypeP->getTypeName(),
      aSyncItemTypeP->getTypeVers()
    )
  );
} // TSyncDataStore::getSendType



// description structure of datastore (NULL if not available)
SmlDevInfDatastorePtr_t TSyncDataStore::getDatastoreDevinf(bool aAsServer, bool aWithoutCTCapProps)
{
  // get Datastore description (when I am a Server)
  return newDevInfDatastore(aAsServer, aWithoutCTCapProps);
} // TSyncDataStore::getDatastoreDevinf


// private helper for setDatastoreDevInf
void TSyncDataStore::registerTypes(
  TSyncItemTypePContainer &aSupportedXmitTypes,
  SmlDevInfXmitListPtr_t aXmitTypeListP,
  TSyncItemTypePContainer &aLocalItemTypes,   // list to look up local types (for reference)
  TSyncItemTypePContainer &aNewItemTypes,     // list to add analyzed types if not already there
  TSyncDataStore *aRelatedDatastoreP
)
{
  while (aXmitTypeListP) {
    if (aXmitTypeListP->data) {
      // register
      TSyncItemType *itemtypeP =
        TSyncItemType::registerRemoteType(fSessionP,aXmitTypeListP->data,aLocalItemTypes,aNewItemTypes,aRelatedDatastoreP);
      // add to list
      aSupportedXmitTypes.push_back(itemtypeP);
    }
    // next
    aXmitTypeListP=aXmitTypeListP->next;
  }
} // TSyncDataStore::registerTypes


/* end of TSyncDataStore implementation */

// eof
