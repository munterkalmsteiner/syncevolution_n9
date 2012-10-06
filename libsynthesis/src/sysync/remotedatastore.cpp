/*
 *  File:         RemoteDataStore.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TRemoteDataStore
 *    Abstraction of remote data store for SyncML Server
 *    Buffers and forwards incoming remote data store commands for
 *    processing by sync engine.
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-06-12 : luz : created
 *
 */

// includes
#include "prefix_file.h"
#include "sysync.h"
#include "syncagent.h"
#include "remotedatastore.h"


using namespace sysync;


/*
 * Implementation of TRemoteDataStore
 */

void TRemoteDataStore::init(void)
{
  // nop so far
} // TSyncDataStore::init


void TRemoteDataStore::InternalResetDataStore(void)
{
  // for server, get default GUID size (in case remote devInf does not send one)
  #ifdef SYSYNC_SERVER
  if (IS_SERVER) {
    fMaxGUIDSize = static_cast<TAgentConfig *>(getSession()->getSessionConfig())->fMaxGUIDSizeSent;
  }
  #endif
} // TRemoteDataStore::InternalResetDataStore


TRemoteDataStore::TRemoteDataStore(TSyncSession *aSessionP) :
  TSyncDataStore(aSessionP)
{
  // nop so far
} // TSyncDataStore::TSyncDataStore


TRemoteDataStore::TRemoteDataStore(
  TSyncSession *aSessionP, const char *aName,
  uInt32 aCommonSyncCapMask
) :
  TSyncDataStore(aSessionP, aName, aCommonSyncCapMask)
{
  // assume full name same as real name, but could be updated at <alert> or <sync>
  fFullName=aName;
} // TSyncDataStore::TSyncDataStore



TRemoteDataStore::~TRemoteDataStore()
{
  InternalResetDataStore();
  // nop so far
} // TRemoteDataStore::~TRemoteDataStore


// - return pure relative (item) URI (removes absolute part or ./ prefix)
const char *TRemoteDataStore::DatastoreRelativeURI(const char *aURI)
{
  return relativeURI(aURI,fSessionP->getRemoteURI());
} // TRemoteDataStore::DatastoreRelativeURI


// check if this remote datastore is accessible with given URI
// NOTE: URI might include path elements or CGI params that are
// access options to the database.
// Remote datastores however only represent devinf, so
// it counts as name match if start of specified URI matches
// name and then continues with "/" or "?" (subpaths and CGI ignored)
uInt16 TRemoteDataStore::isDatastore(const char *aDatastoreURI)
{
  // - make pure relative paths
  const char *nam=DatastoreRelativeURI(fName.c_str());
  size_t n=strlen(nam);
  const char *uri=DatastoreRelativeURI(aDatastoreURI);
  // - compare up to end of local name
  if (strucmp(nam,uri,n,n)==0) {
    // beginnig matches.
    char c=uri[n]; // terminating char
    // match if full match, or uri continues with '/' or '?'
    // Note: return number of chars matched, to allow search for best match
    return (c==0 || c=='/' || c=='?') ? n : 0;
  }
  else
    return 0; // no match
} // TRemoteDataStore::isDatastore



// SYNC command bracket start (check credentials if needed)
bool TRemoteDataStore::remoteProcessSyncCmd(
  SmlSyncPtr_t aSyncP,            // the Sync element
  TStatusCommand &aStatusCommand,     // status that might be modified
  bool &aQueueForLater // will be set if command must be queued for later (re-)execution
)
{
  // adjust name of datastore (may include subpath or CGI)
  fFullName = smlSrcTargLocURIToCharP(aSyncP->source);
  // read meta of Sync Command for remote datastore
  // NOTE: this will overwrite Max values possibly read from DevInf
  //       as meta is more accurate (actual free bytes/ids, not total)
  SmlMetInfMetInfPtr_t metaP = smlPCDataToMetInfP(aSyncP->meta);
  if (metaP) {
    if (metaP->mem) {
      // - max free bytes
      if (metaP->mem->free)
        if (!StrToLongLong(smlPCDataToCharP(metaP->mem->free),fFreeMemory))
          return false;
      // - maximum ID
      if (metaP->mem->freeid)
        if (!StrToLongLong(smlPCDataToCharP(metaP->mem->freeid),fFreeID))
          return false;
    }
    PDEBUGPRINTFX(DBG_REMOTEINFO,("Sync Meta provides memory constraints: FreeMem=" PRINTF_LLD ", FreeID=" PRINTF_LLD,PRINTF_LLD_ARG(fFreeMemory),PRINTF_LLD_ARG(fFreeID)));
  }
  return true;
} // TRemoteDataStore::remoteProcessSyncCmd


// SYNC command bracket end (but another might follow in next message)
bool TRemoteDataStore::remoteProcessSyncCmdEnd(void)
{
  // %%% nop for now, %%% possibly obsolete
  return true;
} // TRemoteDataStore::remoteProcessSyncCmdEnd


// end of all sync commands from client
bool TRemoteDataStore::endOfClientSyncCmds(void)
{
  // %%% nop for now, %%% possibly obsolete
  return true;
} // TRemoteDataStore::endOfClientSyncCmds


// set description structure of datastore
bool TRemoteDataStore::setDatastoreDevInf(
  SmlDevInfDatastorePtr_t aDataStoreDevInfP,  // the datastore DevInf
  TSyncItemTypePContainer &aLocalItemTypes,   // list to look up local types (for reference)
  TSyncItemTypePContainer &aNewItemTypes      // list to add analyzed types if not already there
)
{
  // get important info out of the structure
  SYSYNC_TRY {
    // - name (sourceRef)
    fName=smlPCDataToCharP(aDataStoreDevInfP->sourceref);
    #ifndef MINIMAL_CODE
    // - displayname
    fDisplayName=smlPCDataToCharP(aDataStoreDevInfP->displayname);
    #endif
    // - MaxGUIDsize
    if (aDataStoreDevInfP->maxguidsize) {
      if (!StrToLong(smlPCDataToCharP(aDataStoreDevInfP->maxguidsize),fMaxGUIDSize))
        return false;
    }
    else {
      PDEBUGPRINTFX(DBG_REMOTEINFO,("Datastore DevInf does not specify MaxGUIDSize -> using default"));
    }
    PDEBUGPRINTFX(DBG_REMOTEINFO+DBG_HOT,(
      "Remote Datastore Name='%s', DisplayName='%s', MaxGUIDSize=%ld",
      getName(),
      getDisplayName(),
      (long)fMaxGUIDSize
    ));
    PDEBUGPRINTFX(DBG_REMOTEINFO,(
      "Preferred Rx='%s' version '%s', preferred Tx='%s' version '%s'",
      smlPCDataToCharP(aDataStoreDevInfP->rxpref->cttype),
      smlPCDataToCharP(aDataStoreDevInfP->rxpref->verct),
      smlPCDataToCharP(aDataStoreDevInfP->txpref->cttype),
      smlPCDataToCharP(aDataStoreDevInfP->txpref->verct)
    ));
    // - analyze DS 1.2 style datastore local CTCap
    if (getSession()->getSyncMLVersion()>=syncml_vers_1_2) {
      // analyze CTCaps (content type capabilities)
      SmlDevInfCtcapListPtr_t ctlP = aDataStoreDevInfP->ctcap;
      // loop through list
      PDEBUGBLOCKDESC("RemoteTypes", "Analyzing remote types listed in datastore level CTCap");
      if (getSession()->fIgnoreCTCap) {
        // ignore CTCap
        if (ctlP) {
          PDEBUGPRINTFX(DBG_REMOTEINFO+DBG_HOT,("Remote rule prevents looking at CTCap"));
        }
      }
      else {
        while (ctlP) {
          if (ctlP->data) {
            // create appropriate remote data itemtypes
            if (TSyncItemType::analyzeCTCapAndCreateItemTypes(
              getSession(),
              this, // this is the DS1.2 style where CTCap is related to this datastore
              ctlP->data, // CTCap
              aLocalItemTypes, // look up in local types for specialized classes
              aNewItemTypes // add new item types here
            )) {
              // we have CTCap info of at least one remote type
              getSession()->fRemoteDataTypesKnown=true;
            }
            else
              return false;
          }
          // - go to next item
          ctlP=ctlP->next;
        } // while
      }
      PDEBUGENDBLOCK("RemoteTypes");
    } // if >=DS1.2
    // - analyze supported rx types
    TSyncDataStore *relDsP = getSession()->getSyncMLVersion()>=syncml_vers_1_2 ? this : NULL;
    fRxPrefItemTypeP=TSyncItemType::registerRemoteType(fSessionP,aDataStoreDevInfP->rxpref,aLocalItemTypes,aNewItemTypes,relDsP);
    fRxItemTypes.push_back(fRxPrefItemTypeP);
    registerTypes(fRxItemTypes,aDataStoreDevInfP->rx,aLocalItemTypes,aNewItemTypes,relDsP);
    // - analyze supported tx types
    fTxPrefItemTypeP=TSyncItemType::registerRemoteType(fSessionP,aDataStoreDevInfP->txpref,aLocalItemTypes,aNewItemTypes,relDsP);
    fTxItemTypes.push_back(fTxPrefItemTypeP);
    registerTypes(fTxItemTypes,aDataStoreDevInfP->tx,aLocalItemTypes,aNewItemTypes,relDsP);
    // - Datastore Memory
    if (aDataStoreDevInfP->dsmem) {
      // datastore provides memory information
      // Note: <Sync> command meta could override these with actual free info
      // - max free bytes
      if (aDataStoreDevInfP->dsmem->maxmem) {
        if (!StrToLongLong(smlPCDataToCharP(aDataStoreDevInfP->dsmem->maxmem),fMaxMemory))
          return false;
        else
          fFreeMemory=fMaxMemory; // default for free = max (sync meta might correct this)
      }
      // - maximum free ID
      if (aDataStoreDevInfP->dsmem->maxid) {
        if (!StrToLongLong(smlPCDataToCharP(aDataStoreDevInfP->dsmem->maxid),fMaxID))
          return false;
        else
          fFreeID=fMaxID; // default for free = max (sync meta might correct this)
      }
      PDEBUGPRINTFX(DBG_REMOTEINFO,("DevInf provides DSMem: MaxMem=" PRINTF_LLD ", MaxID=" PRINTF_LLD,PRINTF_LLD_ARG(fFreeMemory),PRINTF_LLD_ARG(fMaxID)));
    }
    // - SyncCap - standard types currently ignored, only
    //   our own extensions are relevant.
    //   Corresponding code in TLocalEngineDS::newDevInfSyncCap()
    if (aDataStoreDevInfP->synccap) {
      SmlPcdataListPtr_t stlP = aDataStoreDevInfP->synccap->synctype;
      // loop through list
      PDEBUGBLOCKDESCCOLL("RemoteSyncTypes", "Analyzing remote sync types listed in datastore level SyncCap");
      while (stlP) {
        if (stlP->data) {
          const char *type = smlPCDataToCharP(stlP->data);
          PDEBUGPRINTFX(DBG_REMOTEINFO,("SyncType='%s'", type));
          fSyncModes.insert(type);
          if (!strcmp(type, "390001")) {
            fCanRestart = true;
          }
        }
        stlP = stlP->next;
      }
      PDEBUGENDBLOCK("RemoteSyncTypes");
    }
  }
  SYSYNC_CATCH (...)
    DEBUGPRINTFX(DBG_ERROR,("******** setDatastoreDevInf caused exception"));
    return false;
  SYSYNC_ENDCATCH
  return true;
} // TRemoteDataStore::setDatastoreDevInf


/* end of TRemoteDataStore implementation */

// eof
