#include <dataconversion.h>
#include <syncsession.h>
#include <multifielditemtype.h>

using namespace sysync;

namespace sysync {

class TDummyLocalDSConfig : public TLocalDSConfig {
public:
  TDummyLocalDSConfig(TConfigElement *element) :
    TLocalDSConfig("dummy", element)
  {}
  virtual TLocalEngineDS* newLocalDataStore(sysync::TSyncSession*) { return NULL; }
};
class TDummyTLocalEngineDS : public TLocalEngineDS {
public:
  TDummyTLocalEngineDS(TLocalDSConfig *config, TSyncSession *session) :
    TLocalEngineDS(config, session, "dummy")
  {}
  virtual bool logicProcessRemoteItem(TSyncItem*, TStatusCommand&, bool&, std::string*) { return false; }
  virtual bool logicRetrieveItemByID(TSyncItem&, TStatusCommand&) { return false; }
#ifdef SYSYNC_CLIENT
  virtual bool logicGenerateSyncCommandsAsClient(TSmlCommandPContainer&, TSmlCommand*&, const char*) { return false; }
#endif
#ifdef SYSYNC_SERVER
  virtual bool logicGenerateSyncCommandsAsServer(TSmlCommandPContainer &, TSmlCommand * &,
    const char *) { return false; }
#endif
  virtual void logicMarkOnlyUngeneratedForResume() {}
  virtual void logicMarkItemForResume(const char*, const char*, bool) {}
  virtual void logicMarkItemForResend(const char*, const char*) {}
#ifdef SYSYNC_SERVER
  virtual TSyncItem *getConflictingItemByRemoteID(TSyncItem *syncitemP) { return NULL; }
  virtual TSyncItem *getConflictingItemByLocalID(TSyncItem *syncitemP) { return NULL; }
  virtual TSyncItem *getMatchingItem(TSyncItem *syncitemP, TEqualityMode aEqMode) { return NULL; }
  virtual void dontSendItemAsServer(TSyncItem *syncitemP) {}
  virtual void SendItemAsServer(TSyncItem *aSyncitemP) {}
  virtual localstatus logicProcessMap(cAppCharP aRemoteID, cAppCharP aLocalID) { return 0; }
#endif
};

bool DataConversion(SessionH aSession,
                    const char *aFromTypeName,
                    const char *aToTypeName,                    
                    std::string &aItemData)
{
  bool res = false;
  TProfileHandler *fromProfile = NULL;
  TProfileHandler *toProfile = NULL;
  TSyncItemType *fromItemType = NULL;
  TSyncItemType *toItemType = NULL;
  TSyncItem *fromItem = NULL;
  TSyncItem *toItem = NULL;
  SmlItemPtr_t smlitem = NULL;
  TSyncSession *session = reinterpret_cast<TSyncSession *>(aSession);
  TConfigElement dummyElement("dummy", NULL);
  class TDummyLocalDSConfig dummyConfig(&dummyElement);
  class TDummyTLocalEngineDS dummy(&dummyConfig, session);

  // Temporarily disable per-session workarounds for our peer's
  // time handling. Eventually we want to make this configurable
  // per function call, not per session, but for that more
  // code restructuring is necessary.
  bool oldRemoteCanHandleUTC = session->fRemoteCanHandleUTC;
  session->fRemoteCanHandleUTC = true;
  timecontext_t oldUserTimeContext = session->fUserTimeContext;
  session->fUserTimeContext = TCTX_UNKNOWN;

  SYSYNC_TRY {
    TRootConfig *config = session->getRootConfig();
    TDatatypesConfig *types = config->fDatatypesConfigP;
    TDataTypeConfig *fromTypeConfig = types->getDataType(aFromTypeName);
    TDataTypeConfig *toTypeConfig = types->getDataType(aToTypeName);
    TStatusCommand statusCmd(session);

    if (!fromTypeConfig) {
      LOGDEBUGPRINTFX(session->getDbgLogger(), DBG_PARSE, ("%s: unknown type", aFromTypeName));
      goto done;
    }
    if (!toTypeConfig) {
      LOGDEBUGPRINTFX(session->getDbgLogger(), DBG_PARSE, ("%s: unknown type", aToTypeName));
      goto done;
    }

    fromItemType = fromTypeConfig->newSyncItemType(session, NULL);
    toItemType = toTypeConfig->newSyncItemType(session, NULL);

    if (!fromItemType || !toItemType) {
      LOGDEBUGPRINTFX(session->getDbgLogger(), DBG_PARSE, ("could not create item types"));
      goto done;
    }

    // parse data and create item from it
    smlitem = newItem();
    if (!smlitem) {
      LOGDEBUGPRINTFX(session->getDbgLogger(), DBG_PARSE, ("SML item allocation failed"));
      goto done;
    }
    smlitem->data = newPCDataStringX(reinterpret_cast<const uInt8 *>(aItemData.c_str()), true, aItemData.size());
    if (!smlitem->data) {
      LOGDEBUGPRINTFX(session->getDbgLogger(), DBG_PARSE, ("SML data allocation failed"));
      goto done;
    }
    fromItem = fromItemType->newSyncItem(smlitem,
                                         sop_replace,
                                         fmt_chr,
                                         fromItemType,
                                         &dummy,
                                         statusCmd);
    smlFreeItemPtr(smlitem);
    smlitem = NULL;
    if (!fromItem) {
      LOGDEBUGPRINTFX(session->getDbgLogger(), DBG_PARSE, ("could not parse data as %s", aFromTypeName));
      goto done;
    }

    // create item of target format, fill it with the parsed data, then encode
    toItem = toItemType->newSyncItem(toItemType, &dummy);
    if (!toItem) {
      LOGDEBUGPRINTFX(session->getDbgLogger(), DBG_PARSE, ("could not allocate item"));
      goto done;
    }
    if (!toItem->replaceDataFrom(*fromItem)) {
      LOGDEBUGPRINTFX(session->getDbgLogger(), DBG_PARSE, ("%s and %s are not compatible", aFromTypeName, aToTypeName));
      goto done;
    }
    smlitem = toItemType->newSmlItem(toItem, &dummy);
    if (!smlitem) {
      LOGDEBUGPRINTFX(session->getDbgLogger(), DBG_PARSE, ("could not encode data as %s", aToTypeName));
      goto done;
    }
    stringSize size;
    const char *data = smlPCDataToCharP(smlitem->data, &size);
    if (!data && size) {
      LOGDEBUGPRINTFX(session->getDbgLogger(), DBG_PARSE, ("string allocation failed"));
      goto done;
    }

    aItemData.assign(data, size);
    res = true;
  } SYSYNC_CATCH (...)
    // TODO: error logging
    res = false;
  SYSYNC_ENDCATCH

  done:
  if (smlitem)
    smlFreeItemPtr(smlitem);
  delete fromItem;
  delete toItem;
  delete fromProfile;
  delete toProfile;
  delete fromItemType;
  delete toItemType;
  session->fRemoteCanHandleUTC = oldRemoteCanHandleUTC;
  session->fUserTimeContext = oldUserTimeContext;

  return res;
}

} // namespace sysync
