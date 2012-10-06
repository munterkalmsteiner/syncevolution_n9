/*
 *  File:         RemoteDataStore.h
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

#ifndef RemoteDataStore_H
#define RemoteDataStore_H

// includes
#include "sysync.h"
#include "syncdatastore.h"


namespace sysync {

class TRemoteDataStore: public TSyncDataStore
{
  typedef TSyncDataStore inherited;
  friend class TSyncAgent;
private:
  void init(void); // internal init
  void InternalResetDataStore(void); // reset for re-use without re-creation
public:
  TRemoteDataStore(TSyncSession *aSessionP);
  TRemoteDataStore(TSyncSession *aSessionP, const char *aName, uInt32 aCommonSyncCapMask=0);
  virtual void engResetDataStore(void) { InternalResetDataStore(); inherited::engResetDataStore(); };
  virtual ~TRemoteDataStore();
  // Naming
  // check if this remote datastore is accessible with given URI
  virtual uInt16 isDatastore(const char *aDatastoreURI);
  const char *getFullName(void) { return fFullName.c_str(); }
  void setFullName(const char *aFullName) { fFullName=aFullName; };
  // - SYNC command bracket start (check credentials if needed)
  bool remoteProcessSyncCmd(
    SmlSyncPtr_t aSyncP,            // the Sync element
    TStatusCommand &aStatusCommand, // status that might be modified
    bool &aQueueForLater // will be set if command must be queued for later (re-)execution
  );
  // %%% probably obsolete %%%% process client commands in server case
  // - SYNC command bracket end (but another might follow in next message)
  bool remoteProcessSyncCmdEnd(void);
  // %%% probably obsolete %%%% process client commands in server case
  // - end of all sync commands from client
  bool endOfClientSyncCmds(void);
  // description structure of datastore (NULL if not available)
  // - set description structure, ownership is passed to TSyncDataStore
  virtual bool setDatastoreDevInf(
    SmlDevInfDatastorePtr_t aDataStoreDevInfP,  // the datastore DevInf
    TSyncItemTypePContainer &aLocalItemTypes,   // list to look up local types (for reference)
    TSyncItemTypePContainer &aNewItemTypes      // list to add analyzed types if not already there
  );
  // helpers
  // - return display name
  #ifndef MINIMAL_CODE
  virtual const char *getDisplayName(void) { return fDisplayName.c_str(); };
  #endif
  // - return pure relative (item) URI (removes absolute part or ./ prefix)
  const char *DatastoreRelativeURI(const char *aURI);
protected:
  #ifndef MINIMAL_CODE
  // Display name of Datastore
  string fDisplayName;
  #endif
private:
  string fFullName;
}; // TRemoteDataStore

} // namespace sysync

#endif  // RemoteDataStore_H

// eof
