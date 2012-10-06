/*
 *  File:         SyncDataStore.h
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

#ifndef SyncDataStore_H
#define SyncDataStore_H

#include "syncitemtype.h"
#include <set>

namespace sysync {


// forward
class TSyncItemType;
class TSyncSession;
class TSyncAppBase;


class TSyncDataStore
{
private:
  void init(TSyncSession *aSessionP); // internal init
  void InternalResetDataStore(void); // reset for re-use without re-creation
public:
  TSyncDataStore(TSyncSession *aSessionP);
  TSyncDataStore(TSyncSession *aSessionP, const char *aName, uInt32 aCommonSyncCapMask=0);
  virtual ~TSyncDataStore();
  virtual void engResetDataStore(void) { InternalResetDataStore(); };
  virtual void announceAgentDestruction(void) { /* nop */ }; ///< called while agent is still fully ok, so we must clean up such that later call of destructor does NOT access agent any more
  // set preferred type support
  void setPreferredTypes(TSyncItemType *aRxItemTypeP, TSyncItemType *aTxItemTypeP=NULL);
  // access datastore
  // - get name (as used in path)
  const char *getName(void) { return fName.c_str(); }
  // - return display name (descriptive)
  virtual const char *getDisplayName(void)
  #ifndef MINIMAL_CODE
    =0; // implemented in derivates
  #else
    { return getName(); }; // just return normal name
  #endif
  // - check if this datastore is accessible with given URI
  //   NOTE: URI might include path elements or CGI params that are
  //   access options to the database; derived classes might
  //   therefore base identity check on more than simple name match
  virtual uInt16 isDatastore(const char *aDatastoreURI);
  // - returns if specified type is used by this datastore
  bool doesUseType(TSyncItemType *aSyncItemType, TTypeVariantDescriptor *aVariantDescP=NULL);
  // - true if data store is able to return 404 when asked to delete non-existent item
  virtual bool dsDeleteDetectsItemPresence() const { return false; }
  // - get common types to send to or receive from another datastore
  TSyncItemType *getTypesForTxTo(TSyncDataStore *aDatastoreP, TSyncItemType **aCorrespondingTypePP=NULL);
  TSyncItemType *getTypesForRxFrom(TSyncDataStore *aDatastoreP, TSyncItemType **aCorrespondingTypePP=NULL);
  // - get datastore's type for receive / send of specified type
  TSyncItemType *getReceiveType(TSyncItemType *aSyncItemTypeP);
  TSyncItemType *getReceiveType(const char *aType, const char *aVers);
  TSyncItemType *getSendType(TSyncItemType *aSyncItemTypeP);
  TSyncItemType *getSendType(const char *aType, const char *aVers);
  virtual TTypeVariantDescriptor getVariantDescForType(TSyncItemType *aItemTypeP) { return NULL; };
  // - get preferred Item types
  TSyncItemType *getPreferredRxItemType(void) { return fRxPrefItemTypeP; };
  TSyncItemType *getPreferredTxItemType(void) { return fTxPrefItemTypeP; };
  // - get max GUID size
  sInt32 getMaxGUIDSize(void) { return fMaxGUIDSize; };
  // - memory limits
  sInt64 getFreeMemory(void) { return fFreeMemory; };
  sInt64 getMaxMemory(void) { return fMaxMemory; };
  sInt64 getFreeID(void) { return fFreeID; };
  sInt64 getMaxID(void) { return fMaxID; };
  bool canRestart() { return fCanRestart; }
  virtual bool syncModeSupported(const std::string &mode) { return fSyncModes.find(mode) != fSyncModes.end(); }
  virtual void getSyncModes(set<string> &modes) { modes = fSyncModes; }
  // - session
  TSyncSession *getSession(void) { return fSessionP; };
  // description structure of datastore (NULL if not available)
  // - get description structure, but ownership remains at TSyncDataStore
  SmlDevInfDatastorePtr_t getDatastoreDevinf(bool aAsServer, bool aWithoutCTCapProps);
  // - set description structure
  virtual bool setDatastoreDevInf(
    SmlDevInfDatastorePtr_t /* aDataStoreDevInfP */,  // the datastore DevInf
    TSyncItemTypePContainer & /* aLocalItemTypes */,   // list to look up local types (for reference)
    TSyncItemTypePContainer & /* aNewItemTypes */      // list to add analyzed types if not already there
  ) { return true; /* nop for base class */ };
  // helpers
  // - returns session zones
  GZones *getSessionZones(void);
  // - return pure relative (item) URI (removes absolute part or ./ prefix)
  virtual const char *DatastoreRelativeURI(const char *aURI) = 0;
  // - get debug mask/logger
  #ifdef SYDEBUG
  TDebugLogger *getDbgLogger(void);
  uInt32 getDbgMask(void);
  #endif
  // - get session owner (dispatcher/clientbase)
  TSyncAppBase *getSyncAppBase(void);  
protected:
  // obtain new datastore info, returns NULL if none available
  virtual SmlDevInfDatastorePtr_t newDevInfDatastore(bool /* aAsServer */, bool /* aWithoutCTCapProps */) { return NULL; } // no description in base class
  SmlDevInfXmitListPtr_t newXMitListDevInf(TSyncItemTypePContainer &aTypeList);
  // obtain Sync Cap mask, defaults to value passed at creation by session
  virtual uInt32 getSyncCapMask(void) { return fCommonSyncCapMask; };
  // SyncML-exposed name (SourceRef in DevInf) of the datastore
  string fName;
  // Maximal size of GUIDs sent to that (remote) datastore, so if real local GUID
  // is longer, temporary GUIDs that meet MaxGUIDSize must be created by the server.
  sInt32 fMaxGUIDSize;
  // Information about datastore memory (could be enormous, so provide longlongs here)
  sInt64 fMaxMemory;   // maximum number bytes for datastore
  sInt64 fFreeMemory;  // number of free bytes
  sInt64 fMaxID;       // maximum number of ID
  sInt64 fFreeID;      // free IDs
  bool fCanRestart;    // if set, then the datastore is able to participate in multiple sync sessions; in other words after a successful read/write cycle it is possible to restart at the reading phase
  set<string> fSyncModes; // all supported sync modes that we know about (empty unless SyncCap was parsed)
public:
  // Type of items in this datastore (read-only, can be used by multiple Datastores simultaneously)
  // - receiving types (also used as default item type)
  TSyncItemType *fRxPrefItemTypeP;
  TSyncItemTypePContainer fRxItemTypes;
  // - sending types
  TSyncItemType *fTxPrefItemTypeP;
  TSyncItemTypePContainer fTxItemTypes;
protected:
  // session
  TSyncSession *fSessionP;
  // common (session) Sync capabilities
  uInt32 fCommonSyncCapMask;
  // type registering
  void registerTypes(
    TSyncItemTypePContainer &aSupportedXmitTypes,
    SmlDevInfXmitListPtr_t aXmitTypeList,
    TSyncItemTypePContainer &aLocalItemTypes,   // list to look up local types (for reference)
    TSyncItemTypePContainer &aNewItemTypes,     // list to add analyzed types if not already there
    TSyncDataStore *aRelatedDatastoreP
  );
}; // TSyncDataStore


} // namespace sysync

#endif  // SyncDataStore_H

// eof
