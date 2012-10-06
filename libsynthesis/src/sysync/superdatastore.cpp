/*
 *  File:         SuperDataStore.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TSuperDataStore
 *    "Virtual" datastore consisting of an union of other
 *    datastores, for example a vCal datastore based on
 *    two separate vEvent and vTodo datastores.
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 *  2002-08-05 : luz : created
 *
 */

// includes
#include "prefix_file.h"
#include "sysync.h"
#include "syncappbase.h"
#include "superdatastore.h"

#ifdef SUPERDATASTORES


using namespace sysync;


// sub-datastore link config
// =========================


// config constructor
TSubDSLinkConfig::TSubDSLinkConfig(TLocalDSConfig *aLocalDSConfigP, TConfigElement *aParentElementP) :
  TConfigElement(aLocalDSConfigP->getName(),aParentElementP)
{
  clear();
  fLinkedDSConfigP=aLocalDSConfigP;
} // TSubDSLinkConfig::TSubDSLinkConfig


// config destructor
TSubDSLinkConfig::~TSubDSLinkConfig()
{
  clear();
} // TSubDSLinkConfig::~TSubDSLinkConfig


// init defaults
void TSubDSLinkConfig::clear(void)
{
  // init defaults
  fDispatchFilter.erase();
  fGUIDPrefix.erase();
  // clear inherited
  inherited::clear();
} // TSubDSLinkConfig::clear


#ifndef HARDCODED_CONFIG

// remote rule config element parsing
bool TSubDSLinkConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  // - identification of remote
  if (strucmp(aElementName,"dispatchfilter")==0)
    expectString(fDispatchFilter);
  else if (strucmp(aElementName,"guidprefix")==0)
    expectString(fGUIDPrefix);
  // - not known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TSubDSLinkConfig::localStartElement

#endif


// superdatastore config
// =====================

TSuperDSConfig::TSuperDSConfig(const char* aName, TConfigElement *aParentElement) :
  TLocalDSConfig(aName,aParentElement)
{
  clear();
} // TSuperDSConfig::TSuperDSConfig


TSuperDSConfig::~TSuperDSConfig()
{
  clear();
} // TSuperDSConfig::~TSuperDSConfig


// init defaults
void TSuperDSConfig::clear(void)
{
  // init defaults
  // - no datastore links
  TSubDSConfigList::iterator pos;
  for(pos=fSubDatastores.begin();pos!=fSubDatastores.end();pos++)
    delete *pos;
  fSubDatastores.clear();
  // clear inherited
  inherited::clear();
} // TSuperDSConfig::clear


#ifndef HARDCODED_CONFIG

// config element parsing
bool TSuperDSConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  // - links to sub-datastores
  if (strucmp(aElementName,"contains")==0) {
    // definition of a new datastore
    const char* nam = getAttr(aAttributes,"datastore");
    if (!nam)
      return fail("'contains' missing 'datastore' attribute");
    // search sub-datastore
    TLocalDSConfig *subdscfgP =
      static_cast<TSessionConfig *>(getParentElement())->getLocalDS(nam);
    if (!subdscfgP)
      return fail("unknown datastore '%s' specified",nam);
    // create new datastore link
    TSubDSLinkConfig *dslinkcfgP =
      new TSubDSLinkConfig(subdscfgP,this);
    // - save in list
    fSubDatastores.push_back(dslinkcfgP);
    // - let element handle parsing
    expectChildParsing(*dslinkcfgP);
  }
  // - none known here
  else
    return TLocalDSConfig::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TSuperDSConfig::localStartElement

#endif

// resolve
void TSuperDSConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    // check for required settings
    // %%% tbd
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TSuperDSConfig::localResolve


// - create appropriate datastore from config, calls addTypeSupport as well
TLocalEngineDS *TSuperDSConfig::newLocalDataStore(TSyncSession *aSessionP)
{
  // Synccap defaults to normal set supported by the engine by default
  TSuperDataStore *sdsP =
     new TSuperDataStore(this,aSessionP,getName(),aSessionP->getSyncCapMask());
  // add type support
  addTypes(sdsP,aSessionP);
  return sdsP;
} // TLocalDSConfig::newLocalDataStore





/*
 * Implementation of TSuperDataStore
 */

/* public TSuperDataStore members */


TSuperDataStore::TSuperDataStore(TSuperDSConfig *aDSConfigP, TSyncSession *aSessionP, const char *aName, uInt32 aCommonSyncCapMask) :
  TLocalEngineDS(aDSConfigP, aSessionP, aName, aCommonSyncCapMask)
{
  // set config ptr
  fDSConfigP = aDSConfigP;
  if (!fDSConfigP)
    SYSYNC_THROW(TSyncException(DEBUGTEXT("TSuperDataStore::TSuperDataStore called with NULL config","lds1")));
  // reset first
  InternalResetDataStore();
  // for server, all configured subdatastores are automatically linked in
  // (for client, only those that are active will be linked in at dsSetClientParams() time)
  if (IS_SERVER) {
    // create links to subdatastores
    TSubDSConfigList::iterator pos;
    for(pos=aDSConfigP->fSubDatastores.begin();pos!=aDSConfigP->fSubDatastores.end();pos++) {
      // add link
      addSubDatastoreLink(*pos,NULL); // search datastore
    }
  }
} // TSuperDataStore::TSuperDataStore


void TSuperDataStore::addSubDatastoreLink(TSubDSLinkConfig *aDSLinkConfigP, TLocalEngineDS *aDatastoreP)
{
  TSubDatastoreLink link;
  // start not yet pending
  link.fStartPending = false;
  // set link to subdatastore's config
  link.fDSLinkConfigP = aDSLinkConfigP;
  if (aDatastoreP)
    link.fDatastoreLinkP = aDatastoreP; // we already know the datastore (for client, it might not yet be in session list of datastores)
  else
    link.fDatastoreLinkP = fSessionP->findLocalDataStore(link.fDSLinkConfigP->fLinkedDSConfigP); // find actual datastore by "handle" (= config pointer)
  // make sure datastore is instantiated
  if (!link.fDatastoreLinkP) {
    // instantiate now (should not happen on server, as all datastores are instantiated on a server anyway)
    link.fDatastoreLinkP=fSessionP->addLocalDataStore(link.fDSLinkConfigP->fLinkedDSConfigP);
  }
  // save link
  fSubDSLinks.push_back(link);
  // Important: We need to get the iterator now again, in case the list was empty before
  // (because the iterator set in InternalResetDataStore() is invalid because it was created for an empty list).
  fCurrentGenDSPos=fSubDSLinks.begin();
}


void TSuperDataStore::InternalResetDataStore(void)
{
  // init
  fFirstTimeSync=false;
  TSubDSLinkList::iterator pos;
  // cancel all pending starts
  for(pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    pos->fStartPending=false;
  }
  fSuperStartPending = false;
  // make sure this is set in case startSync() is not called before generateSyncCommands()
  fCurrentGenDSPos=fSubDSLinks.begin();
} // TSuperDataStore::InternalResetDataStore


TSuperDataStore::~TSuperDataStore()
{
  InternalResetDataStore();
} // TSuperDataStore::~TSuperDataStore



// Session events, which need some distribution to subdatastores
// =============================================================

// Methods overriding TLocalEngineDS
// ----------------------------------

bool TSuperDataStore::canRestart()
{
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    if (!pos->fDatastoreLinkP->canRestart()) {
      return false;
    }
  }
  return true;
}

// obtain Sync Cap mask, must be lowest common mask of all subdatastores
uInt32 TSuperDataStore::getSyncCapMask(void)
{
  // AND of all subdatastores
  uInt32 capmask = ~0; // all bits set
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    capmask = capmask & pos->fDatastoreLinkP->getSyncCapMask();
  }
  return capmask;
} // TSuperDataStore::getSyncCapMask

bool TSuperDataStore::syncModeSupported(const std::string &mode)
{
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    if (!pos->fDatastoreLinkP->syncModeSupported(mode)) {
      return false;
    }
  }
  return true;
}

void TSuperDataStore::getSyncModes(set<string> &modes)
{
  // Initialize with content from first subdatastore.
  TSubDSLinkList::iterator pos = fSubDSLinks.begin();
  if (pos!=fSubDSLinks.end()) {
    pos->fDatastoreLinkP->getSyncModes(modes);
    ++pos;
  }
  // Remove any mode not found in any of the other subdatastores.
  while (pos!=fSubDSLinks.end() && !modes.empty()) {
    set<string> b;
    pos->fDatastoreLinkP->getSyncModes(b);
    set<string>::iterator it = modes.begin();
    while (it != modes.end()) { 
      if (b.find(*it) != b.end()) {
        ++it;
      } else {
        set<string>::iterator del = it;
        ++it;
        modes.erase(del);
      }
    }
    pos++;
  }
}


// process Sync alert from remote party: check if alert code is supported,
// check if slow sync is needed due to anchor mismatch
// - server case: also generate appropriate Alert acknowledge command
TAlertCommand *TSuperDataStore::engProcessSyncAlert(
  TSuperDataStore *aAsSubDatastoreOf, // if acting as subdatastore
  uInt16 aAlertCode,                  // the alert code
  const char *aLastRemoteAnchor,      // last anchor of client
  const char *aNextRemoteAnchor,      // next anchor of client
  const char *aTargetURI,             // target URI as sent by remote, no processing at all
  const char *aIdentifyingTargetURI,  // target URI that was used to identify datastore
  const char *aTargetURIOptions,      // option string contained in target URI
  SmlFilterPtr_t aTargetFilter,       // DS 1.2 filter, NULL if none
  const char *aSourceURI,             // source URI
  TStatusCommand &aStatusCommand      // status that might be modified
)
{
  TAlertCommand *alertcmdP=NULL;

  TAlertCommand *subalertcmdP=NULL;
  TStatusCommand substatus(fSessionP);

  SYSYNC_TRY {
    // alert all subdatastores
    TSubDSLinkList::iterator pos;
    for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
      subalertcmdP=pos->fDatastoreLinkP->engProcessSyncAlert(
        this,                   // as subdatastore of myself
        aAlertCode,             // the alert code
        aLastRemoteAnchor,      // last anchor of client
        aNextRemoteAnchor,      // next anchor of client
        aTargetURI,             // target URI as sent by remote, no processing at all
        aIdentifyingTargetURI,  // target URI (without possible CGI)
        aTargetURIOptions,      // filtering CGI (NULL or empty if none)
        aTargetFilter,          // DS 1.2 filter, NULL if none
        aSourceURI,             // source URI
        substatus               // status that might be modified
      );
      if (subalertcmdP) {
        // get rid of this, we don't need it (server case only, client case does not generate an alert command here anyway)
        delete subalertcmdP;
      }
      // check if processing alert had a problem
      // Notes:
      // - When we have a subalertcmdP here, it is the server case, which means a non-zero status code
      //   (such as 508) at this point is ok and should not stop processing alerts.
      //   Only in case we have no alert we need to check the status code and abort immediately if it's not ok.
      // - 508 can happen even in client for the rare case the server thinks anchors are ok, but client check
      //   says they are not, so we need to exclude 508 here.
      if (!subalertcmdP && substatus.getStatusCode()!=0 && substatus.getStatusCode()!=508) {
        // basic problem with one of the subdatastores
        // - propagate error code
        aStatusCommand.setStatusCode(substatus.getStatusCode());
        // - no alert to send
        return NULL;
      }
      // this one is pending for start
      pos->fStartPending=true;
    }
    // set flag to indicate this subdatastore has init pending
    // Now all subdatastores should be successfully alerted and have current anchor infos ready,
    // so we can call inherited (which will obtain combined anchors from our logicInitSyncAnchors)
    alertcmdP = inherited::engProcessSyncAlert(
      aAsSubDatastoreOf,      // as indicated by caller (normally, superdatastore is not subdatastore of another superdatastore, but...)
      aAlertCode,             // the alert code
      aLastRemoteAnchor,      // last anchor of client
      aNextRemoteAnchor,      // next anchor of client
      aTargetURI,             // target URI as sent by remote, no processing at all
      aIdentifyingTargetURI,  // target URI (without possible CGI)
      aTargetURIOptions,      // filtering CGI (NULL or empty if none)
      aTargetFilter,          // DS 1.2 filter, NULL if none
      aSourceURI,             // source URI
      aStatusCommand          // status that might be modified
    );
    // entire superdatastore is pending for start
    fSuperStartPending=true;
  }
  SYSYNC_CATCH (...)
    // clean up locally owned objects
    if (alertcmdP) delete alertcmdP;
    if (subalertcmdP) delete subalertcmdP;
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
  return alertcmdP;
} // TSuperDataStore::engProcessSyncAlert


// process status received for sync alert
bool TSuperDataStore::engHandleAlertStatus(TSyError aStatusCode)
{
  // show it to all subdatastores
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    pos->fDatastoreLinkP->engHandleAlertStatus(aStatusCode);
  }
  // all subdatastores have seen the alert status, so let superdatastore handle it as well
  return TLocalEngineDS::engHandleAlertStatus(aStatusCode);
} // TSuperDataStore::engHandleAlertStatus


// Set remote datastore for local
void TSuperDataStore::engSetRemoteDatastore(
  TRemoteDataStore *aRemoteDatastoreP  // the remote datastore involved
)
{
  // set all subdatastores to (same) remote datastore as superdatastore itself
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    pos->fDatastoreLinkP->engSetRemoteDatastore(aRemoteDatastoreP);
  }
  // set in superdatastore as well
  TLocalEngineDS::engSetRemoteDatastore(aRemoteDatastoreP);
} // TSuperDataStore::engSetRemoteDatastore


// set Sync types needed for sending local data to remote DB
void TSuperDataStore::setSendTypeInfo(
  TSyncItemType *aLocalSendToRemoteTypeP,
  TSyncItemType *aRemoteReceiveFromLocalTypeP
)
{
  // set all subdatastores to (same) types as superdatastore itself
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    pos->fDatastoreLinkP->setSendTypeInfo(aLocalSendToRemoteTypeP,aRemoteReceiveFromLocalTypeP);
  }
  // set in superdatastore as well
  TLocalEngineDS::setSendTypeInfo(aLocalSendToRemoteTypeP,aRemoteReceiveFromLocalTypeP);
} // TSuperDataStore::setSendTypeInfo


// set Sync types needed for receiving remote data in local DB
void TSuperDataStore::setReceiveTypeInfo(
  TSyncItemType *aLocalReceiveFromRemoteTypeP,
  TSyncItemType *aRemoteSendToLocalTypeP
)
{
  // set all subdatastores to (same) types as superdatastore itself
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    pos->fDatastoreLinkP->setReceiveTypeInfo(aLocalReceiveFromRemoteTypeP,aRemoteSendToLocalTypeP);
  }
  // set in superdatastore as well
  TLocalEngineDS::setReceiveTypeInfo(aLocalReceiveFromRemoteTypeP,aRemoteSendToLocalTypeP);
} // TSuperDataStore::setReceiveTypeInfo


// init usage of datatypes set with setSendTypeInfo/setReceiveTypeInfo
localstatus TSuperDataStore::initDataTypeUse(void)
{
  localstatus sta=LOCERR_OK;

  // set all subdatastores to (same) types as superdatastore itself
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    sta = pos->fDatastoreLinkP->initDataTypeUse();
    if (sta!=LOCERR_OK)
      return sta; // failed
  }
  // set in superdatastore as well
  return TLocalEngineDS::initDataTypeUse();
} // TSuperDataStore::initDataTypeUse


// SYNC command bracket start (check credentials if needed)
bool TSuperDataStore::engProcessSyncCmd(
  SmlSyncPtr_t aSyncP,                // the Sync element
  TStatusCommand &aStatusCommand,     // status that might be modified
  bool &aQueueForLater // will be set if command must be queued for later (re-)execution
)
{
  // start sync for all subdatastores
  bool ok=true;
  bool doqueue;
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    // Note: this will cause subdatastores to call their own startSync,
    //   so we do NOT need to iterate over subdatastores in our startSync!
    doqueue=false;
    // if in init phase (entire superdatastore pending to start)
    // only call subdatastores that are still pending for start, too
    ok=true;
    if (!fSuperStartPending || pos->fStartPending) {
      ok=pos->fDatastoreLinkP->engProcessSyncCmd(aSyncP,aStatusCommand,doqueue);
      if (!doqueue) {
        // this one is now initialized. Do not do it again until all others are initialized, too
        pos->fStartPending=false;
      }
      else {
        // we must queue the entire command for later (but some subdatastores might be excluded then)
        aQueueForLater=true; // queue if one of the subdatastores needs it
      }
    }
    if (!ok) return false;
  }
  // start sync myself
  ok=TLocalEngineDS::engProcessSyncCmd(aSyncP,aStatusCommand,doqueue);
  if (doqueue) aQueueForLater=true; // queue if one of the subdatastores needs it
  // if we reach this w/o queueing, start is no longer pending
  if (!aQueueForLater) fSuperStartPending=false;
  // done
  return ok;
} // TSuperDataStore::processSyncCmd


// SYNC command bracket end (but another might follow in next message)
bool TSuperDataStore::engProcessSyncCmdEnd(bool &aQueueForLater)
{
  // signal sync end to all subdatastores
  bool ok=true;
  bool doqueue;
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    doqueue=false;
    ok=pos->fDatastoreLinkP->engProcessSyncCmdEnd(doqueue);
    if (doqueue) aQueueForLater=true; // queue if one of the subdatastores needs it
    if (!ok) return false;
  }
  // signal it to myself
  ok=TLocalEngineDS::engProcessSyncCmdEnd(doqueue);
  if (doqueue) aQueueForLater=true; // queue if one of the subdatastores needs it
  return ok;
} // TSuperDataStore::engProcessSyncCmdEnd


#ifdef SYSYNC_SERVER

// process map
localstatus TSuperDataStore::engProcessMap(cAppCharP aRemoteID, cAppCharP aLocalID)
{
  TSubDatastoreLink *linkP = NULL;
  localstatus sta = LOCERR_OK;

  // item has local ID, we can find datastore by prefix
  linkP = findSubLinkByLocalID(aLocalID);
  if (!linkP) {
    sta = 404; // not found
    goto done;
  }
  // let subdatastore process (and only show subDS part of localID)
  sta=linkP->fDatastoreLinkP->engProcessMap(
    aRemoteID,
    aLocalID+linkP->fDSLinkConfigP->fGUIDPrefix.size()
  );
done:
  return sta;
} // TSuperDataStore::engProcessMap

#endif // SYSYNC_SERVER


// called to process incoming item operation
// Method takes ownership of syncitemP in all cases
// - returns true (and unmodified or non-200-successful status) if
//   operation could be processed regularily
// - returns false (but probably still successful status) if
//   operation was processed with internal irregularities, such as
//   trying to delete non-existant item in datastore with
//   incomplete Rollbacks (which returns status 200 in this case!).
bool TSuperDataStore::engProcessRemoteItem(
  TSyncItem *syncitemP,
  TStatusCommand &aStatusCommand
)
{
  bool regular=true;
  string datatext;
  #ifdef SYSYNC_SERVER
  TSyncItem *itemcopyP = NULL;
  #endif

  // show
  PDEBUGBLOCKFMT((
    "SuperProcessItem", "Processing incoming item in superdatastore",
    "datastore=%s|SyncOp=%s|RemoteID=%s|LocalID=%s",
    getName(),
    SyncOpNames[syncitemP->getSyncOp()],
    syncitemP->getRemoteID(),
    syncitemP->getLocalID()
  ));
  // let appropriate subdatastore handle the command
  TSubDatastoreLink *linkP = NULL;
  TSyncOperation sop=syncitemP->getSyncOp();
  string remid;
  TSubDSLinkList::iterator pos;
  #ifdef SYSYNC_SERVER
  if (IS_SERVER) {
    switch (sop) {
      // Server case
      case sop_wants_replace:
      case sop_replace:
      case sop_wants_add:
      case sop_add:
        // item has no local ID, we need to apply filters to item data
        PDEBUGPRINTFX(DBG_DATA,("Checkin subdatastore filters to find where it belongs"));
        linkP = findSubLinkByData(*syncitemP);
        if (!linkP) goto nods;
        PDEBUGPRINTFX(DBG_DATA,(
          "Found item belongs to subdatastore '%s'",
          linkP->fDatastoreLinkP->getName()
        ));
        // make sure item does not have a local ID (which would be wrong because of prefixes anyway)
        syncitemP->clearLocalID();
        // remembert because we might need it below for move-replace
        remid=syncitemP->getRemoteID();
        // let subdatastore process
        regular=linkP->fDatastoreLinkP->engProcessRemoteItem(syncitemP,aStatusCommand);
        // now check if replace was treated as add, if yes, this indicates
        // that this might be a move between subdatastores
        if (
          (sop==sop_replace || sop==sop_wants_replace) &&
          !fSlowSync && aStatusCommand.getStatusCode()==201
        ) {
          // this is probably a move from another datastore by changing an attribute
          // that dispatches datastores (such as a vEvent changed to a vToDo)
          // - so we delete all items with this remote ID in all other datastores
          PDEBUGPRINTFX(DBG_DATA,("Replace could be a move between subdatastores, trying to delete all items with same remoteID in other subdatastores"));
          TStatusCommand substatus(fSessionP);
          for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
            if (&(*pos) != linkP) {
              // all but original datastore
              substatus.setStatusCode(200);
              itemcopyP = new TSyncItem();
              // - only remote ID and syncop are relevant, leave everything else empty
              itemcopyP->setRemoteID(remid.c_str());
              itemcopyP->setSyncOp(sop_delete);
              // - now try to delete. This might fail if replace above wasn't a move
              //   itemcopyP is consumed
              PDEBUGPRINTFX(DBG_DATA+DBG_DETAILS,(
                "Trying to delete item with remoteID='%s' from subdatastore '%s'",
                itemcopyP->getRemoteID(),
                linkP->fDatastoreLinkP->getName()
              ));
              regular=pos->fDatastoreLinkP->engProcessRemoteItem(itemcopyP,substatus);
              #ifdef SYDEBUG
              if (regular) {
                // deleted ok
                PDEBUGPRINTFX(DBG_DATA,(
                  "Found item in '%s', deleted here (and moved to '%s')",
                  pos->fDatastoreLinkP->getName(),
                  linkP->fDatastoreLinkP->getName()
                ));
              }
              #endif
            }
          }
          PDEBUGPRINTFX(DBG_DATA,("End of (possible) move-replace between subdatastores"));
          regular=true; // fully ok, no matter if delete above has succeeded or not
        }
        goto done;
      case sop_archive_delete:
      case sop_soft_delete:
      case sop_delete:
      case sop_copy:
        // item has no local ID AND no data, only a remoteID:
        // we must try to read item or attempt to delete in all subdatastores by remoteID
        // until found in one of them

        // get an empty item of correct type to call logicRetrieveItemByID
        itemcopyP = getLocalReceiveType()->newSyncItem(getRemoteSendType(),this);
        // - only remote ID is relevant, leave everything else empty
        itemcopyP->setRemoteID(syncitemP->getRemoteID());
        // try to find item in any of the subdatastores
        for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
          linkP = &(*pos);
          // always start with 200
          aStatusCommand.setStatusCode(200);

          // item deleted by a failed engProcessRemoteItem()?
          if (!syncitemP) {
            // recreate it for next attempt
            syncitemP = getLocalReceiveType()->newSyncItem(getRemoteSendType(),this);
            syncitemP->setRemoteID(itemcopyP->getRemoteID());
            syncitemP->setSyncOp(sop);
          }

          if (sop!=sop_copy && linkP->fDatastoreLinkP->dsDeleteDetectsItemPresence()) {
            // attempt to delete, consuming original item on success
            regular=linkP->fDatastoreLinkP->engProcessRemoteItem(syncitemP,aStatusCommand);
            // must be ok AND not 404 (item not found)
            if (regular && aStatusCommand.getStatusCode()!=404) {
              PDEBUGPRINTFX(DBG_DATA,(
                "Item found in subdatastore '%s', deleted it there",
                linkP->fDatastoreLinkP->getName()
              ));

              // done
              goto done;
            } else {
              // syncitemP was deleted by engProcessRemoteItem() (for
              // example, in TStdLogicDS::logicProcessRemoteItem()),
              // so we must remember to recreate it from the copy for
              // another attempt if there is one
              syncitemP = NULL;
            }
          } else {
            // try to read first to determine whether the subdatastore contains the item;
            // necessary because the subdatastore is not able to report a 404 error in
            // the delete operation when the item does not exist
            PDEBUGPRINTFX(DBG_DATA+DBG_DETAILS,(
              "Trying to read item by remoteID='%s' from subdatastore '%s' to see if it is there",
              itemcopyP->getRemoteID(),
              linkP->fDatastoreLinkP->getName()
            ));
            regular=linkP->fDatastoreLinkP->logicRetrieveItemByID(*itemcopyP,aStatusCommand);
            // must be ok AND not 404 (item not found)
            if (regular && aStatusCommand.getStatusCode()!=404) {
              PDEBUGPRINTFX(DBG_DATA,(
                "Item found in subdatastore '%s', deleting it there",
                linkP->fDatastoreLinkP->getName()
              ));
              // now we can delete or copy, consuming original item
              regular=linkP->fDatastoreLinkP->engProcessRemoteItem(syncitemP,aStatusCommand);
              // done
              regular=true;
              goto done;
            }
          }
        }
        // none of the datastores could process this item --> error
        // - make sure delete reports 200 for incomplete-rollback-datastores
        if (aStatusCommand.getStatusCode()==404 && sop!=sop_copy) {
          // not finding an item for delete might be ok for remote...
          if (fSessionP->getSessionConfig()->fDeletingGoneOK) {
            // 404/410: item not found, could be because previous aborted session has
            // already committed deletion of that item -> behave as if delete was ok
            PDEBUGPRINTFX(DBG_DATA,("to-be-deleted item was not found, but do NOT report %hd",aStatusCommand.getStatusCode()));
            aStatusCommand.setStatusCode(200);
          }
          // ...but it is a internal irregularity, fall thru to return false
        }
        // is an internal irregularity
        regular=false;
        goto done;
    case sop_reference_only:
    case sop_move:
    case sop_none:
    case numSyncOperations:
      // nothing to do or shouldn't happen
      break;
    } // switch
  } // server
  #endif // SYSYNC_SERVER
  #ifdef SYSYNC_CLIENT
  if (IS_CLIENT) {
    switch (sop) {
      // Client case
      case sop_wants_replace:
      case sop_replace:
      case sop_archive_delete:
      case sop_soft_delete:
      case sop_delete:
      case sop_copy:
        // item has local ID, we can find datastore by prefix
        linkP = findSubLinkByLocalID(syncitemP->getLocalID());
        if (!linkP) goto nods;
        // remove prefix before letting subdatastore process it
        syncitemP->fLocalID.erase(0,linkP->fDSLinkConfigP->fGUIDPrefix.size());
        // now let subdatastore process
        regular=linkP->fDatastoreLinkP->engProcessRemoteItem(syncitemP,aStatusCommand);
        goto done;
      case sop_wants_add:
      case sop_add:
        // item has no local ID, we need to apply filters to item data
        linkP = findSubLinkByData(*syncitemP);
        if (!linkP) goto nods;
        // make sure item does not have a local ID (which would be wrong because of prefixes anyway)
        syncitemP->clearLocalID();
        // let subdatastore process
        regular=linkP->fDatastoreLinkP->engProcessRemoteItem(syncitemP,aStatusCommand);
        goto done;
    case sop_reference_only:
    case sop_move:
    case sop_none:
    case numSyncOperations:
      // nothing to do or shouldn't happen
      break;
    } // switch
  } // client
  #endif // SYSYNC_CLIENT
nods:
  // no datastore or unknown command, general DB error
  aStatusCommand.setStatusCode(510);
  PDEBUGPRINTFX(DBG_ERROR,("TSuperDataStore::processRemoteItem Fatal: Item cannot be processed by any subdatastore"));
  // consume item
  delete syncitemP;
  regular=false;
  goto done;
done:
  #ifdef SYSYNC_SERVER
  delete itemcopyP;
  #endif
  PDEBUGENDBLOCK("SuperProcessItem");
  return regular;
} // TSuperDataStore::engProcessRemoteItem



// - must return true if this datastore is finished with <sync>
//   (if all datastores return true,
//   session is allowed to finish sync packet with outgoing message
bool TSuperDataStore::isSyncDone(void)
{
  // check subdatastores
  bool done=true;
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    done=done && pos->fDatastoreLinkP->isSyncDone();
  }
  // check myself
  return done && TLocalEngineDS::isSyncDone();
} // TSuperDataStore::isSyncDone


// abort sync with this super datastore (that is, with all subdatastores as well)
void TSuperDataStore::engAbortDataStoreSync(TSyError aStatusCode, bool aLocalProblem, bool aResumable)
{
  // abort subdatastores
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    pos->fDatastoreLinkP->engAbortDataStoreSync(aStatusCode,aLocalProblem,aResumable);
  }
  // set code in my own ancestor
  TLocalEngineDS::engAbortDataStoreSync(aStatusCode,aLocalProblem,aResumable);
} // TSuperDataStore::engAbortDataStoreSync


// - must return true if this datastore is finished with <sync>
//   (if all datastores return true,
//   session is allowed to finish sync packet with outgoing message
bool TSuperDataStore::isAborted(void)
{
  // check subdatastores
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    if (pos->fDatastoreLinkP->isAborted()) return true; // one aborted, super aborted as well
  }
  // check myself
  return TLocalEngineDS::isAborted();
} // TSuperDataStore::isAborted


// called at very end of sync session, when everything is done
// Note: is also called before deleting a datastore (so aborted sessions
//   can do cleanup and/or statistics display as well)
void TSuperDataStore::engFinishDataStoreSync(localstatus aErrorStatus)
{
  // inform all subdatastores
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    pos->fDatastoreLinkP->engFinishDataStoreSync(aErrorStatus);
  }
  // call inherited
  inherited::engFinishDataStoreSync(aErrorStatus);
} // TSuperDataStore::engFinishDataStoreSync


// Internal events during sync to access local database
// ====================================================

// Methods overriding TLocalEngineDS
// ----------------------------------



// called at sync alert (before generating for client, after receiving for server)
// - obtains combined anchor from subdatastores
// - combines them into a common anchor (if possible)
// - updates fFirstTimeSync as well
localstatus TSuperDataStore::engInitSyncAnchors(
  cAppCharP aDatastoreURI,      // (Note: unused in superdatastore) local datastore URI
  cAppCharP aRemoteDBID         // (Note: unused in superdatastore) ID of remote datastore (to find session information in local DB)
)
{
  bool allanchorsequal=true;
  localstatus sta=LOCERR_OK;

  // superdatastore has no own anchors, so collect data from subdatastores
  fFirstTimeSync=false;
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    if (pos==fSubDSLinks.begin()) {
      // Server case note:
      //   Subdatastore's engInitSyncAnchors() MUST NOT be called here, because this routine will
      //   always be called after all subdatastore's engProcessSyncAlert() was called
      //   which in turn contains a call to engInitSyncAnchors().
      // Client case note:
      //   Subdatastore's engInitSyncAnchors() MUST NOT be called here, because this routine will
      //   be called from TSuperDataStore::engPrepareClientSyncAlert() after iterating through
      //   subdatastores and calling their engPrepareClientDSForAlert(), which in turn
      //   contains a call to engInitSyncAnchors().
      // This means we can safely assume we have the fLastRemoteAnchor/fNextLocalAnchor info
      // ready here.
      // - Assign references of first datastore
      //   Note: remote anchor must be same from all subdatastores
      fLastRemoteAnchor=pos->fDatastoreLinkP->fLastRemoteAnchor;
      // - these are used from the first datastore, and might differ (a few seconds,
      //   that is) for other datastores
      fLastLocalAnchor=pos->fDatastoreLinkP->fLastLocalAnchor;
      fNextLocalAnchor=pos->fDatastoreLinkP->fNextLocalAnchor;
    }
    else {
      // see if all are equal
      allanchorsequal = allanchorsequal &&
        pos->fDatastoreLinkP->fLastRemoteAnchor == fLastRemoteAnchor;
    }
    // also combine firstTimeSync (first time if it's first for any of the subdatastores)
    fFirstTimeSync = fFirstTimeSync || pos->fDatastoreLinkP->fFirstTimeSync;
  }
  // make sure common anchor is valid only if all of the subdatastores have equal anchors
  if (sta!=LOCERR_OK || fFirstTimeSync || !allanchorsequal) {
    fLastLocalAnchor.empty();
    fLastRemoteAnchor.empty();
  }
  // superdatastore gets adminready when all subdatastores have successfully done engInitSyncAnchors()
  if (sta==LOCERR_OK) {
    changeState(dssta_adminready); // admin data is now ready
  }
  // return status
  return sta;
} // TSuperDataStore::engInitSyncAnchors


// - called at start of first <Sync> command (prepare DB for reading/writing)
bool TSuperDataStore::startSync(TStatusCommand &aStatusCommand)
{
  DEBUGPRINTFX(DBG_HOT,("TSuperDataStore::startSync"));
  // make sure we start generating with first datastore
  fCurrentGenDSPos=fSubDSLinks.begin();
  // NOTE: Do NOT iterate subdatastores, because these were called already
  // by engProcessSyncCmd (server case) or engProcessSyncAlert (client case)
  return true; // ok
} // TSuperDataStore::startSync


/// check is datastore is completely started.
/// @param[in] aWait if set, call will not return until either started state is reached
///   or cannot be reached within the maximally allowed request processing time left.
bool TSuperDataStore::engIsStarted(bool aWait)
{
  // check subdatastores
  bool ready=true;
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    ready=ready && pos->fDatastoreLinkP->engIsStarted(aWait);
  }
  // check myself
  return ready && inherited::engIsStarted(aWait);
} // TSuperDataStore::engIsStarted


// remove prefix for given subDatastore
// @param[in] aIDWithPrefix points to ID with prefix
// @return NULL if either datastore not found or prefix not present in aIDWithPrefix
// @return pointer to first char in aIDWithPrefix which is not part of the prefix
cAppCharP TSuperDataStore::removeSubDSPrefix(cAppCharP aIDWithPrefix, TLocalEngineDS *aLocalDatastoreP)
{
  if (!aIDWithPrefix) return NULL;
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    if (pos->fDatastoreLinkP == aLocalDatastoreP) {
      // check the prefix
      if (strnncmp(
        aIDWithPrefix,
        pos->fDSLinkConfigP->fGUIDPrefix.c_str(),
        pos->fDSLinkConfigP->fGUIDPrefix.size()
      ) ==0)
        return aIDWithPrefix+pos->fDSLinkConfigP->fGUIDPrefix.size(); // return start of subDS ID
      else
        return aIDWithPrefix; // datastore found, but prefix is not there, return unmodified
    }
  }
  return NULL;
} // TSuperDataStore::removeSubDSPrefix


// private helper: find subdatastore which matches prefix of given localID
TSubDatastoreLink *TSuperDataStore::findSubLinkByLocalID(const char *aLocalID)
{
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    if (strnncmp(
      aLocalID,
      pos->fDSLinkConfigP->fGUIDPrefix.c_str(),
      pos->fDSLinkConfigP->fGUIDPrefix.size()
    ) ==0) {
      // found
      return &(*pos);
    }
  }
  return NULL; // not found
} // TSuperDataStore::findSubLinkByLocalID


// private helper: find subdatastore which can accept item data
TSubDatastoreLink *TSuperDataStore::findSubLinkByData(TSyncItem &aSyncItem)
{
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    PDEBUGPRINTFX(DBG_DATA+DBG_DETAILS,(
      "Testing item data against <dispatchfilter> of subdatastore '%s'",
      pos->fDatastoreLinkP->getName()
    ));
    if (aSyncItem.testFilter(pos->fDSLinkConfigP->fDispatchFilter.c_str())) {
      // found
      return &(*pos);
    }
  }
  return NULL; // not found
} // TSuperDataStore::findSubLinkByData


// only dummy, creates error if called
bool TSuperDataStore::logicRetrieveItemByID(
  TSyncItem &aSyncItem, // item to be filled with data from server. Local or Remote ID must already be set
  TStatusCommand &aStatusCommand // status, must be set on error or non-200-status
)
{
  aStatusCommand.setStatusCode(500);
  DEBUGPRINTFX(DBG_ERROR,("TSuperDataStore::logicRetrieveItemByID called, which should never happen!!!!!!"));
  return false; // not ok
} // TSuperDataStore::logicRetrieveItemByID


// only dummy, creates error if called
// - Method takes ownership of syncitemP in all cases
bool TSuperDataStore::logicProcessRemoteItem(
  TSyncItem *syncitemP,
  TStatusCommand &aStatusCommand,
  bool &aVisibleInSyncset, // on entry: tells if resulting item SHOULD be visible; on exit: set if processed item remains visible in the sync set.
  string *aGUID // GUID is stored here if not NULL
)
{
  delete syncitemP; // consume
  aStatusCommand.setStatusCode(500);
  DEBUGPRINTFX(DBG_ERROR,("TSuperDataStore::logicProcessRemoteItem called, which should never happen!!!!!!"));
  return false; // not ok
} // TSuperDataStore::logicProcessRemoteItem


// - returns true if DB implementation can filter during database fetch
//   (otherwise, fetched items must be filtered after being read from DB)
bool TSuperDataStore::engFilteredFetchesFromDB(bool aFilterChanged)
{
  // only if all subdatastores support it
  bool yes=true;
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    yes = yes && pos->fDatastoreLinkP->engFilteredFetchesFromDB(aFilterChanged);
  }
  return yes;
} // TSuperDataStore::engFilteredFetchesFromDB


// - called for SyncML 1.1 if remote wants number of changes.
//   Must return -1 if no NOC value can be returned
sInt32 TSuperDataStore::getNumberOfChanges(void)
{
  sInt32 noc,totalNoc = 0;
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    noc = pos->fDatastoreLinkP->getNumberOfChanges();
    if (noc<0) return -1; // if one of the subdatastores does not know NOC, we can't return a NOC
    // subdatastore knows its NOC, sum up
    totalNoc+=noc;
  }
  // return sum of all NOCs
  return totalNoc;
}; // TSuperDataStore::getNumberOfChanges


// show statistics or error of current sync
void TSuperDataStore::showStatistics(void)
{
  // show something in debug log
  PDEBUGPRINTFX(DBG_HOT,("Superdatastore Sync for '%s' (%s), %s sync status:",
    getName(),
    fRemoteViewOfLocalURI.c_str(),
    fSlowSync ? "slow" : "normal"
  ));
  // and on user console
  CONSOLEPRINTF((""));
  CONSOLEPRINTF(("- Superdatastore Sync for '%s' (%s), %s sync status:",
    getName(),
    fRemoteViewOfLocalURI.c_str(),
    fSlowSync ? "slow" : "normal"
  ));
  // now show results
  if (isAborted()) {
    // failed
    PDEBUGPRINTFX(DBG_ERROR,("Warning: Failed with status code=%hd",fAbortStatusCode));
    CONSOLEPRINTF(("  ************ Failed with status code=%hd",fAbortStatusCode));
  }
  else {
    // successful: show statistics on console
    PDEBUGPRINTFX(DBG_HOT,("Completed successfully - details see subdatastores"));
    CONSOLEPRINTF(("  Completed successfully  - details see subdatastores"));
  }
  CONSOLEPRINTF((""));
} // TSuperDataStore::showStatistics


// - returns true if DB implementation of all subdatastores support resume
bool TSuperDataStore::dsResumeSupportedInDB(void)
{
  // yes if all subdatastores support it
  bool yes=true;
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    yes = yes && pos->fDatastoreLinkP->dsResumeSupportedInDB();
  }
  return yes;
} // TSuperDataStore::dsResumeSupportedInDB


// helper to save resume state either at end of request or explicitly at reception of a "suspend"
localstatus TSuperDataStore::engSaveSuspendState(bool aAnyway)
{
  // only save here if not aborted already (aborting saves the state immediately)
  // or explicitly requested
  if (aAnyway || !isAborted()) {
    // only save if DS 1.2 and supported by DB
    if ((fSessionP->getSyncMLVersion()>=syncml_vers_1_2) && dsResumeSupportedInDB()) {
      PDEBUGBLOCKFMT(("SuperSaveSuspendState","Saving superdatastore suspend/resume state","superdatastore=%s",getName()));
      // save alert state
      fResumeAlertCode=fAlertCode;
      TSubDSLinkList::iterator pos;
      if (fResumeAlertCode) {
        // let all subdatastores update partial item and markOnlyUngeneratedForResume()
        for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
          // save partial state if any
          if (pos->fDatastoreLinkP->fPartialItemState!=pi_state_save_outgoing) {
            // ONLY if we have no request for saving an outgoing item state already,
            // we possibly need to save a pending incoming item
            // if there is an incompletely received item, let it update Partial Item (fPIxxx) state
            // (if it is an item of this datastore, that is).
            if (fSessionP->fIncompleteDataCommandP) {
              fSessionP->fIncompleteDataCommandP->updatePartialItemState(pos->fDatastoreLinkP);
            }
          }
          // mark ungenerated
          pos->fDatastoreLinkP->logicMarkOnlyUngeneratedForResume();
        }
        /// @note that already generated items are related to the originating
        /// localEngineDS, so markPendingForResume() on existing commands will
        /// directly reach the correct datastore
        /// @note markItemForResume() will get the localID as presented to
        /// remote, that is in case of superdatastores with prefixes that need to be removed
        fSessionP->markPendingForResume(this);
      }
      // let all subdatastores logicSaveResumeMarks() to make all this persistent
      localstatus globErr=LOCERR_OK;
      for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
        localstatus err=pos->fDatastoreLinkP->logicSaveResumeMarks();
        if (err!=LOCERR_OK) globErr=err;
      }
      PDEBUGENDBLOCK("SuperSaveSuspendState");
      return globErr;
    }
  }
  return LOCERR_OK;
} // TSuperDataStore::engSaveSuspendState


#ifdef SYSYNC_SERVER

/// @brief called at end of request processing, should be used to save suspend state
/// @note subdatastores don't do anything themselves, to make sure superds can make things happen in correct order
void TSuperDataStore::engRequestEnded(void)
{
  // variant for superdatastore - also handles its subdatastores
  // For DS 1.2: Make sure everything is ready for a resume in case there's an abort (implicit Suspend)
  // before the next request. Note that the we cannot wait for session timeout, as the resume attempt
  // from the client probably arrives much earlier.
  if (testState(dssta_syncmodestable)) {
    // make sure all unsent items are marked for resume
    localstatus sta=engSaveSuspendState(false); // only if not already aborted
    if (sta!=LOCERR_OK) {
      DEBUGPRINTFX(DBG_ERROR,("Could not save suspend state at end of Request: err=%hd",sta));
    }
  }
  // let datastore prepare for end of request (other than thread change)
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    pos->fDatastoreLinkP->dsRequestEnded();
  }
  // then let them know that thread may change
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    pos->fDatastoreLinkP->dsThreadMayChangeNow();
  }
} // TSuperDataStore::engRequestEnded

#endif


// - called to let server generate sync commands for client
//   Returns true if now finished for this datastore
//   also sets fState to dss_syncdone when finished
bool TSuperDataStore::engGenerateSyncCommands(
  TSmlCommandPContainer &aNextMessageCommands,
  TSmlCommand * &aInterruptedCommandP,
  const char *aLocalIDPrefix
)
{
  PDEBUGBLOCKFMT(("SuperSyncGen","Now generating sync commands from superdatastore","datastore=%s",getName()));
  bool finished=false;
  string prefix;

  while (!isAborted()) {
    // check for end
    if (fCurrentGenDSPos==fSubDSLinks.end()) {
      // done, update status
      changeState(dssta_syncgendone,true);
      break;
    }
    // create current prefix
    AssignString(prefix,aLocalIDPrefix);
    prefix.append(fCurrentGenDSPos->fDSLinkConfigP->fGUIDPrefix);
    // call subdatastore to generate commands
    finished=fCurrentGenDSPos->fDatastoreLinkP->engGenerateSyncCommands(
      aNextMessageCommands,
      aInterruptedCommandP,
      prefix.c_str()
    );
    // exit if not yet finished with generating commands for this datastore
    if (!finished) break;
    // done with this datastore, switch to next if any
    fCurrentGenDSPos++;
  } // while not aborted
  // finished when state is dss_syncdone
  PDEBUGPRINTFX(DBG_DATA,(
    "superdatastore's engGenerateSyncCommands ended, state='%s', sync generation %sdone",
    getDSStateName(),
    dbgTestState(dssta_syncgendone,true) ? "" : "NOT "
  ));
  PDEBUGENDBLOCK("SuperSyncGen");
  // also finished with this datastore when aborted
  return (isAborted() || testState(dssta_syncgendone,true));
} // TSuperDataStore::generateSyncCommands


#ifdef SYSYNC_CLIENT

// Client only: initialize Sync alert for datastore according to Parameters set with dsSetClientSyncParams()
localstatus TSuperDataStore::engPrepareClientSyncAlert(void)
{
  localstatus sta;

  // not resuming by default
  fResuming = false;
  fResumeAlertCode = 0;
  // prepare all subdatastores that were parametrized to participate in a sync by dsSetClientSyncParams()
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    TLocalEngineDS *dsP = pos->fDatastoreLinkP;
    if (dsP->testState(dssta_clientparamset)) {
      // configured for sync, prepare for alert
      sta = dsP->engPrepareClientDSForAlert();
      if (sta!=LOCERR_OK)
        return sta; // error
      // collect slow sync status
      fSlowSync = fSlowSync || dsP->fSlowSync;
      // collect resume alert code
      if (dsP->fResuming) {
        // subdatastore would like to resume
        if (fResumeAlertCode!=0 && fResumeAlertCode!=dsP->fResumeAlertCode) {
          // different idea about what to resume -> can't resume
          PDEBUGPRINTFX(DBG_ERROR,("subdatastores differ in resume alert code -> cancel resume of superdatastore"));
          fResuming = false;
        }
        else {
          // resume is possible (but might be cancelled if another subdatastore disagrees
          fResumeAlertCode = dsP->fResumeAlertCode;
          fResuming = true;
        }
      }
    }
  }
  // now init my own anchors and firstsync state, which are a combination of my subdatastore's
  sta = engInitSyncAnchors(NULL,NULL);
  if (sta!=LOCERR_OK)
    return sta; // error
  // determine final resume state
  if (fResuming) {
    PDEBUGPRINTFX(DBG_PROTO,("Found suspended session with Alert Code = %hd for all subdatastores",fResumeAlertCode));
  }
  else {
    // superdatastore can't resume, cancel all subdatastore's resumes that might be set
    for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
      pos->fDatastoreLinkP->fResuming = false;
    }
  }
  // all successful
  return LOCERR_OK;
} // TSuperDataStore::engPrepareClientSyncAlert



// Init engine for client sync
// - determine types to exchange
// - make sync set ready
localstatus TSuperDataStore::engInitForClientSync(void)
{
  localstatus sta = LOCERR_OK;
  // first let all subdatastores init for sync
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    sta = pos->fDatastoreLinkP->engInitDSForClientSync();
    if (sta!=LOCERR_OK)
      return sta;
  }
  // now change my own state
  return inherited::engInitDSForClientSync();
} // TSuperDataStore::engInitForClientSync






// Client only: returns number of unsent map items
sInt32 TSuperDataStore::numUnsentMaps(void)
{
  // add maps from all subdatastores
  uInt32 num=0;
  TSubDSLinkList::iterator pos;
  for (pos=fSubDSLinks.begin();pos!=fSubDSLinks.end();pos++) {
    num+=pos->fDatastoreLinkP->numUnsentMaps();
  }
  return num;
} // TSuperDataStore::numUnsentMaps


// called to mark maps confirmed, that is, we have received ok status for them
void TSuperDataStore::engMarkMapConfirmed(cAppCharP aLocalID, cAppCharP aRemoteID)
{
  // we must detect the subdatastore by prefix
  TSubDatastoreLink *linkP = findSubLinkByLocalID(aLocalID);
  if (linkP) {
    // pass to subdatastore with prefix removed
    linkP->fDatastoreLinkP->engMarkMapConfirmed(aLocalID+linkP->fDSLinkConfigP->fGUIDPrefix.size(),aRemoteID);
  }
} // TSuperDataStore::engMarkMapConfirmed


// - client only: called to generate Map items
//   Returns true if now finished for this datastore
//   also sets fState to dss_done when finished
bool TSuperDataStore::engGenerateMapItems(TMapCommand *aMapCommandP, cAppCharP aLocalIDPrefix)
{
  TSubDSLinkList::iterator pos=fSubDSLinks.begin();
  bool ok;
  string prefix;

  PDEBUGBLOCKDESC("SuperMapGenerate","TSuperDataStore: Generating Map items...");
  do {
    // check if already done
    if (pos==fSubDSLinks.end()) break; // done
    // create current prefix
    AssignString(prefix,aLocalIDPrefix);
    prefix.append(pos->fDSLinkConfigP->fGUIDPrefix);
    // generate Map items
    ok=pos->fDatastoreLinkP->engGenerateMapItems(aMapCommandP,prefix.c_str());
    // exit if not yet finished with generating map items for this datastore
    if (!ok) {
      PDEBUGENDBLOCK("MapGenerate");
      return false; // not all map items generated
    }
    // next datastore
    pos++;
  } while(true);
  // done
  // we are done if state is syncdone (no more sync commands will occur)
  if (testState(dssta_dataaccessdone)) {
    changeState(dssta_clientmapssent,true);
    PDEBUGPRINTFX(DBG_PROTO,("TSuperDataStore: Finished generating Map items, server has finished <sync>, we are done now"))
  }
  #ifdef SYDEBUG
  // else if we are not yet dssta_syncgendone -> this is the end of a early pending map send
  else if (!dbgTestState(dssta_syncgendone)) {
    PDEBUGPRINTFX(DBG_PROTO,("TSuperDataStore: Finished sending cached Map items from last session"))
  }
  // otherwise, we are not really finished with the maps yet (but with the current map command)
  else {
    PDEBUGPRINTFX(DBG_PROTO,("TSuperDataStore: Finished generating Map items for now, but server still sending <Sync>"))
  }
  #endif
  PDEBUGENDBLOCK("MapGenerate");
  return true;
} // TSuperDataStore::engGenerateMapItems


#endif // SYSYNC_CLIENT


/* end of TSuperDataStore implementation */

#endif // SUPERDATASTORES

// eof
