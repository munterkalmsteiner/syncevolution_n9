/**
 *  @File     pluginapids.cpp
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TPluginApiDS
 *    Plugin based datastore API implementation
 *
 *    Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-10-06 : luz : created from apidbdatastore
 */

// includes
#include "sysync.h"
#include "pluginapids.h"
#include "pluginapiagent.h"

#ifdef SYSER_REGISTRATION
#include "syserial.h"
#endif

#ifdef ENGINEINTERFACE_SUPPORT
#include "engineentry.h"
#endif

#include "SDK_support.h"


namespace sysync {

// Config
// ======

// Helpers
// =======


// Field Map item
// ==============

TApiFieldMapItem::TApiFieldMapItem(const char *aElementName, TConfigElement *aParentElement) :
  inherited(aElementName,aParentElement)
{
  /* nop for now */
} // TApiFieldMapItem::TApiFieldMapItem


void TApiFieldMapItem::checkAttrs(const char **aAttributes)
{
  /* nop for now */
  inherited::checkAttrs(aAttributes);
} // TApiFieldMapItem::checkAttrs


#ifdef ARRAYDBTABLES_SUPPORT

// Array Map item
// ===============

TApiFieldMapArrayItem::TApiFieldMapArrayItem(TCustomDSConfig *aCustomDSConfigP, TConfigElement *aParentElement) :
  inherited(aCustomDSConfigP,aParentElement)
{
  /* nop for now */
} // TApiFieldMapArrayItem::TApiFieldMapArrayItem


void TApiFieldMapArrayItem::checkAttrs(const char **aAttributes)
{
  /* nop for now */
  inherited::checkAttrs(aAttributes);
} // TApiFieldMapArrayItem::checkAttrs

#endif



// TPluginDSConfig
// ============

TPluginDSConfig::TPluginDSConfig(const char* aName, TConfigElement *aParentElement) :
  inherited(aName,aParentElement),
  fPluginParams_Admin(this),
  fPluginParams_Data(this)
{
  // nop so far
  clear();
} // TPluginDSConfig::TPluginDSConfig


TPluginDSConfig::~TPluginDSConfig()
{
  clear();
  // disconnect from the API module
  fDBApiConfig_Data.Disconnect();
  fDBApiConfig_Admin.Disconnect();
} // TPluginDSConfig::~TPluginDSConfig


// init defaults
void TPluginDSConfig::clear(void)
{
  // init defaults
  fDBAPIModule_Admin.erase();
  fDBAPIModule_Data.erase();
  fDataModuleAlsoHandlesAdmin=false;
  fEarlyStartDataRead = false;
  // - default to use all debug flags set (if debug for plugin is enabled at all)
  fPluginDbgMask_Admin=0xFFFF;
  fPluginDbgMask_Data=0xFFFF;
  // - clear plugin params
  fPluginParams_Admin.clear();
  fPluginParams_Data.clear();
  // - clear capabilities
  fItemAsKey = false;
  fHasDeleteSyncSet = false;
  // clear inherited
  inherited::clear();
} // TPluginDSConfig::clear


// config element parsing
bool TPluginDSConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"plugin_module")==0)
    expectMacroString(fDBAPIModule_Data);
  else if (strucmp(aElementName,"plugin_datastoreadmin")==0)
    expectBool(fDataModuleAlsoHandlesAdmin);
  else if (strucmp(aElementName,"plugin_earlystartdataread")==0)
    expectBool(fEarlyStartDataRead);
  else if (strucmp(aElementName,"plugin_params")==0)
    expectChildParsing(fPluginParams_Data);
  else if (strucmp(aElementName,"plugin_debugflags")==0)
    expectUInt16(fPluginDbgMask_Data);
  else if (strucmp(aElementName,"plugin_module_admin")==0)
    expectMacroString(fDBAPIModule_Admin);
  else if (strucmp(aElementName,"plugin_params_admin")==0)
    expectChildParsing(fPluginParams_Admin);
  else if (strucmp(aElementName,"plugin_debugflags_admin")==0)
    expectUInt16(fPluginDbgMask_Admin);
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TPluginDSConfig::localStartElement


// resolve
void TPluginDSConfig::localResolve(bool aLastPass)
{
  // resolve plugin specific config leaf
  fPluginParams_Admin.Resolve(aLastPass);
  fPluginParams_Data.Resolve(aLastPass);
  // try to resolve configured API-module name into set of function pointers
  if (aLastPass) {
    // Determine if we may use non-built-in plugins
    bool allowDLL= true; // by default, it is allowed, but if PLUGIN_DLL is not set, it will be disabled anyway.
    #if defined(SYSER_REGISTRATION) && !defined(DLL_PLUGINS_ALWAYS_ALLOWED)
    // license flags present and DLL plugins not generally allowed:
    // -> license decides if DLL is allowed
    allowDLL= (getSyncAppBase()->fRegProductFlags & SYSER_PRODFLAG_SERVER_SDKAPI)!=0;
    // warn about DLL not allowed ONLY if this build actually supports DLL plugins
    #if defined(PLUGIN_DLL)
    if (!allowDLL) {
      SYSYNC_THROW(TConfigParseException("License does not allow using <datastore type=\"plugin\">"));
    }
    #endif // DLL support available in the code at all
    #endif // DLL plugins not generally allowed (or DLL support not compiled in)
    // Connect module for handling data access
    if (!fDBAPIModule_Data.empty()) {
      // we have a module specified for data access
      DB_Callback cb= &fDBApiConfig_Data.fCB.Callback;
      cb->callbackRef       = getSyncAppBase();
      #ifdef ENGINEINTERFACE_SUPPORT
      cb->thisBase          = getSyncAppBase()->fEngineInterfaceP;
      #endif
      #ifdef SYDEBUG
      // direct Module level debug to global log
      cb->debugFlags= (getSyncAppBase()->getRootConfig()->fDebugConfig.fGlobalDebugLogs) &&
                      PDEBUGTEST(DBG_DATA+DBG_DBAPI+DBG_PLUGIN) ? fPluginDbgMask_Data : 0;
      cb->DB_DebugPuts      = AppBaseLogDebugPuts;
      cb->DB_DebugBlock     = AppBaseLogDebugBlock;
      cb->DB_DebugEndBlock  = AppBaseLogDebugEndBlock;
      cb->DB_DebugEndThread = AppBaseLogDebugEndThread;
      cb->DB_DebugExotic    = AppBaseLogDebugExotic;
      #endif
      if (fDBApiConfig_Data.Connect(fDBAPIModule_Data.c_str(), getSyncAppBase()->fApiInterModuleContext, getName(), false, allowDLL)!=LOCERR_OK)
        SYSYNC_THROW(TConfigParseException("Cannot connect to datastore implementation module specified in <plugin_module>"));
      // now pass plugin-specific config
      if (fDBApiConfig_Data.PluginParams(fPluginParams_Data.fConfigString.c_str())!=LOCERR_OK)
        SYSYNC_THROW(TConfigParseException("Module does not understand params passed in <plugin_params>"));
      // Check module capabilities
      TDB_Api_Str capa;
      fDBApiConfig_Data.Capabilities(capa);
      string capaStr = capa.c_str();
      // - check existence of DeleteSyncSet()
      fHasDeleteSyncSet = FlagOK(capaStr,CA_DeleteSyncSet,true);
      // - Check for new method for data access (as keys instead of as text items)
      fItemAsKey = FlagOK(capaStr,CA_ItemAsKey,true);
      // Check if engine is compatible
      #ifndef DBAPI_TEXTITEMS
      if (!fItemAsKey) SYSYNC_THROW(TConfigParseException("This engine does not support data items in text format"));
      #endif
      #if !defined(DBAPI_ASKEYITEMS) || !defined(ENGINEINTERFACE_SUPPORT)
      if (fItemAsKey) SYSYNC_THROW(TConfigParseException("This engine does not support data items passed as key handles"));
      #endif
    }
    // connect module for handling admin access
    // - use same module and params as data if no separate module specified and plugin_datastoreadmin is set
    if (fDBAPIModule_Admin.empty() && fDataModuleAlsoHandlesAdmin) {
      fDBAPIModule_Admin=fDBAPIModule_Data;
      fPluginParams_Admin=fPluginParams_Data;
      fPluginDbgMask_Admin=fPluginDbgMask_Data;
    }
    // - now connect the admin module
    if (!fDBAPIModule_Admin.empty()) {
      // we have a module specified for data access
      DB_Callback cb= &fDBApiConfig_Admin.fCB.Callback;
      cb->callbackRef      = getSyncAppBase();
      #ifdef ENGINEINTERFACE_SUPPORT
      cb->thisBase         = getSyncAppBase()->fEngineInterfaceP;
      #endif
      #ifdef SYDEBUG
      // direct Module level debug to global log
      cb->debugFlags= (getSyncAppBase()->getRootConfig()->fDebugConfig.fGlobalDebugLogs) &&
      PDEBUGTEST(DBG_ADMIN+DBG_DBAPI+DBG_PLUGIN) ? fPluginDbgMask_Admin : 0;
      cb->DB_DebugPuts     = AppBaseLogDebugPuts;
      cb->DB_DebugBlock    = AppBaseLogDebugBlock;
      cb->DB_DebugEndBlock = AppBaseLogDebugEndBlock;
      cb->DB_DebugEndThread= AppBaseLogDebugEndThread;
      cb->DB_DebugExotic   = AppBaseLogDebugExotic;
      #endif
      if (fDBApiConfig_Admin.Connect(fDBAPIModule_Admin.c_str(),getSyncAppBase()->fApiInterModuleContext,getName())!=LOCERR_OK)
        SYSYNC_THROW(TConfigParseException("Cannot connect to datastore implementation module specified in <plugin_module_admin>"));
      // now pass plugin-specific config
      if (fDBApiConfig_Admin.PluginParams(fPluginParams_Admin.fConfigString.c_str())!=LOCERR_OK)
        SYSYNC_THROW(TConfigParseException("Module does not understand params passed in <plugin_params_admin>"));
    }
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TPluginDSConfig::localResolve


// - create appropriate datastore from config, calls addTypeSupport as well
TLocalEngineDS *TPluginDSConfig::newLocalDataStore(TSyncSession *aSessionP)
{
  // Synccap defaults to normal set supported by the engine by default
  TLocalEngineDS *ldsP;
  if (IS_CLIENT) {
    ldsP = new TPluginApiDS(this,aSessionP,getName(),aSessionP->getSyncCapMask() & ~(isOneWayFromRemoteSupported() ? 0 : SCAP_MASK_ONEWAY_SERVER));
  }
  else {
    ldsP = new TPluginApiDS(this,aSessionP,getName(),aSessionP->getSyncCapMask() & ~(isOneWayFromRemoteSupported() ? 0 : SCAP_MASK_ONEWAY_CLIENT));
  }
  // do common stuff
  addTypes(ldsP,aSessionP);
  // return
  return ldsP;
} // TPluginDSConfig::newLocalDataStore


/*
 * Implementation of TPluginApiDS
 */

// constructor
TPluginApiDS::TPluginApiDS(
  TPluginDSConfig *aConfigP,
  sysync::TSyncSession *aSessionP,
  const char *aName,
  uInt32 aCommonSyncCapMask
) :
  #ifdef SDK_ONLY_SUPPORT
  TCustomImplDS(aConfigP,aSessionP, aName, aCommonSyncCapMask)
  #else
  TODBCApiDS(aConfigP,aSessionP, aName, aCommonSyncCapMask)
  #endif
{
  // save a type casted pointer to the agent
  fPluginAgentP=static_cast<TPluginApiAgent *>(aSessionP);
  // save pointer to config record
  fPluginDSConfigP=aConfigP;
  // make a local copy of the typed agent pointer (note that the agent itself does
  // NOT YET have its constructor completely run so we can't just copy the agents pointer)
  fPluginAgentConfigP = DYN_CAST<TPluginAgentConfig *>(
    aSessionP->getRootConfig()->fAgentConfigP
  );
  if (!fPluginAgentConfigP) SYSYNC_THROW(TSyncException(DEBUGTEXT("TPluginApiDS finds no AgentConfig","api1")));
  // Note: do not create context here because Agent is not yet initialized.
  // clear rest
  InternalResetDataStore();
} // TPluginApiDS::TPluginApiDS


TPluginApiDS::~TPluginApiDS()
{
  InternalResetDataStore();
} // TPluginApiDS::~TPluginApiDS


/// @brief called while agent is still fully ok, so we must clean up such that later call of destructor does NOT access agent any more
void TPluginApiDS::announceAgentDestruction(void)
{
  // reset myself
  InternalResetDataStore();
  // make sure we don't access the agent any more
  engTerminateDatastore();
  fPluginAgentP = NULL;
  // destroy API context
  fDBApi_Data.DeleteContext();
  fDBApi_Admin.DeleteContext();
  // call inherited
  inherited::announceAgentDestruction();
} // TPluginApiDS::announceAgentDestruction


/// @brief called to reset datastore
/// @note must be safe to be called multiple times and even after announceAgentDestruction()
void TPluginApiDS::InternalResetDataStore(void)
{
  // filtering capabilities need to be evaluated first
  fAPICanFilter = false;
  fAPIFiltersTested = false;
} // TPluginApiDS::InternalResetDataStore




#ifdef DBAPI_TEXTITEMS

// store API key/value pair field in mapped field, if one is defined
bool TPluginApiDS::storeField(
  cAppCharP aName,
  cAppCharP aParams,
  cAppCharP aValue,
  TMultiFieldItem &aItem,
  uInt16 aSetNo,
  sInt16 aArrayIndex
)
{
  TFieldMapList *fmlP = &(fPluginDSConfigP->fFieldMappings.fFieldMapList);
  TFieldMapList::iterator pos;
  TApiFieldMapItem *fmiP;
  string s;
  bool stored=false;
  // search field map list for matching map entry
  for (pos=fmlP->begin(); pos!=fmlP->end(); pos++) {
    fmiP = static_cast<TApiFieldMapItem *>(*pos);
    // check name
    if (
      fmiP->readable &&
      fmiP->setNo==aSetNo &&
      strucmp(fmiP->getName(),aName)==0
    ) {
      // DB-readable field with matching name
      TDBFieldType dbfty = fmiP->dbfieldtype;
      TItemField *fieldP;
      sInt16 fid = fmiP->fid;
      // determine leaf field
      fieldP = getMappedFieldOrVar(aItem,fid,aArrayIndex);
      // continue only if we have a field
      if (!fieldP) continue;
      // check if the field is proxyable and input defines a BLOB id
      #ifdef STREAMFIELD_SUPPORT
      if (fieldP->isBasedOn(fty_string)) {
        // - check if params contain a BLOBID
        string blobid;
        if (paramScan(aParams,"BLOBID",blobid)) {
          // this field is a blob, create a proxy for it
          TApiBlobProxy *apiProxyP = new TApiBlobProxy(this,!fieldP->isBasedOn(fty_blob),blobid.c_str(),aItem.getLocalID());
          // attach it to the string or blob field
          static_cast<TStringField *>(fieldP)->setBlobProxy(apiProxyP);
          // check if we must read it right now
          if (paramScan(aParams,"READNOW",blobid))
            static_cast<TStringField *>(fieldP)->pullFromProxy();
          // check next mapping
          continue;
        }
      }
      #endif
      // store according to database field type
      switch (dbfty) {
        case dbft_string:
          // for explicit strings, perform character set and line feed conversion
          s.erase();
          // - convert from database charset to UTF-8 and to C-string linefeeds
          appendStringAsUTF8(aValue, s, fPluginDSConfigP->fDataCharSet, lem_cstr);
          fieldP->setAsString(s.c_str());
          break;
        case dbft_blob:
          // blob is treated as 1:1 string if there's no proxy for it
        default:
          // for all other DB types, string w/o charset conversion is enough (these are by definition all single-line, ASCII-only)
          if (fieldP->isBasedOn(fty_timestamp)) {
            // interpret timestamps in dataTimeZone context (or as floating if this field is mapped in "f" mode)
            TTimestampField *tsfP = static_cast<TTimestampField *>(fieldP);
            tsfP->setAsISO8601(aValue, fmiP->floating_ts ? TCTX_UNKNOWN : fPluginDSConfigP->fDataTimeZone, false);
            // modify time zone if params contain a TZNAME
            if (paramScan(aParams,"TZNAME",s)) {
              // convert to time zone context
              timecontext_t tctx;
              TimeZoneNameToContext(s.c_str(), tctx, tsfP->getGZones());
              tsfP->moveToContext(tctx, true); // move to new context, bind floating (and float fixed, if TZNAME=FLOATING)
            }
          }
          else {
            // just set as string
            fieldP->setAsString(aValue);
          }
          break;
      } // switch
      // field successfully stored, do NOT exit loop
      // because there could be a second map for the same attribute!
      stored=true;
    } // if map found for attribute
  } // for all field mappings
  return stored;
} // TPluginApiDS::storeField



// - parse text data into item
//   Note: generic implementation, using virtual storeField() method
//         to differentiate between use with mapped fields in DBApi and
//         direct (unmapped) TMultiFieldItem access in Tunnel API.
bool TPluginApiDS::parseDBItemData(
  TMultiFieldItem &aItem,
  cAppCharP aItemData,
  uInt16 aSetNo
)
{
  bool stored = parseItemData(aItem,aItemData,aSetNo);
  if (stored) {
    // post-process
    stored = postReadProcessItem(aItem,aSetNo);
  }
  return stored;
} // TPluginApiDS::parseItemData



// generate text representations of item's fields (BLOBs and parametrized fields not included)
// - returns true if at least one field appended
bool TPluginApiDS::generateDBItemData(
  bool aAssignedOnly,
  TMultiFieldItem &aItem,
  uInt16 aSetNo,
  string &aDataFields
)
{
  TFieldMapList *fmlP = &(fPluginDSConfigP->fFieldMappings.fFieldMapList);
  TFieldMapList::iterator pos;
  TApiFieldMapItem *fmiP;
  string val;
  bool createdone=false;

  // pre-process (run scripts)
  if (!preWriteProcessItem(aItem)) return false;
  // create text representation for all mapped and writable fields
  for (pos=fmlP->begin(); pos!=fmlP->end(); pos++) {
    fmiP = static_cast<TApiFieldMapItem *>(*pos);
    if (
      fmiP->writable &&
      fmiP->setNo==aSetNo
    ) {
      // get field
      TItemField *basefieldP;
      sInt16 fid = fmiP->fid;
      // determine base field (might be array)
      basefieldP = getMappedBaseFieldOrVar(aItem,fid);
      if (generateItemFieldData(
        aAssignedOnly,
        fPluginDSConfigP->fDataCharSet,
        fPluginDSConfigP->fDataLineEndMode,
        fPluginDSConfigP->fDataTimeZone,
        basefieldP,
        fmiP->getName(),
        aDataFields
      ))
        createdone=true; // we now have at least one field
    } // if writable field
  } // for all field mappings
  PDEBUGPRINTFX(DBG_USERDATA+DBG_DBAPI+DBG_EXOTIC+DBG_HOT,("generateDBItemData generated string for DBApi:"));
  PDEBUGPUTSXX(DBG_USERDATA+DBG_DBAPI+DBG_EXOTIC,aDataFields.c_str(),0,true);
  return createdone;
} // TPluginApiDS::generateDBItemData



#endif // DBAPI_TEXTITEMS


// - post process item after reading from DB (run script)
bool TPluginApiDS::postReadProcessItem(TMultiFieldItem &aItem, uInt16 aSetNo)
{
  #if defined(DBAPI_ASKEYITEMS) && defined(STREAMFIELD_SUPPORT)
  // for ItemKey mode, we need to create a BLOB proxy for each as_param mapped field
  if (fPluginDSConfigP->fItemAsKey) {
    // find all asParam fields that can have proxies and create BLOB proxies for these
    TFieldMapList *fmlP = &(fPluginDSConfigP->fFieldMappings.fFieldMapList);
    TFieldMapList::iterator pos;
    TApiFieldMapItem *fmiP;

    // post-process all mapped and writable fields
    for (pos=fmlP->begin(); pos!=fmlP->end(); pos++) {
      fmiP = static_cast<TApiFieldMapItem *>(*pos);
      if (
        fmiP->readable &&
        fmiP->setNo==aSetNo
      ) {
        // get field
        TItemField *basefieldP, *leaffieldP;
        #ifdef ARRAYFIELD_SUPPORT
        uInt16 arrayIndex=0;
        #endif
        sInt16 fid = fmiP->fid;
        // determine base field (might be array)
        basefieldP = getMappedBaseFieldOrVar(aItem,fid);
        // ignore map if we have no field for it
        if (!basefieldP) continue;
        // We have a base field for this, check what to do
        if (fPluginDSConfigP->fUserZoneOutput && !fmiP->floating_ts && basefieldP->elementsBasedOn(fty_timestamp)) {
          // userzoneoutput requested for non-floating timestamp field, move it!
          #ifdef ARRAYFIELD_SUPPORT
          arrayIndex=0;
          #endif
          do {
            #ifdef ARRAYFIELD_SUPPORT
            if (basefieldP->isArray()) {
              leaffieldP = basefieldP->getArrayField(arrayIndex,true); // get existing leaf fields only
              arrayIndex++;
            }
            else
              leaffieldP = basefieldP; // leaf is base field
            // if no leaf field, we'll need to exit here (we're done with the array)
            if (leaffieldP==NULL) break;
            #else
            leaffieldP = basefieldP; // no arrays: leaf is always base field
            #endif
            static_cast<TTimestampField *>(leaffieldP)->moveToContext(fSessionP->fUserTimeContext, false);
          } while(basefieldP->isArray()); // only arrays do loop all array elements
        }
        if (fmiP->as_param && basefieldP->elementsBasedOn(fty_string)) {
          // string based field (string or BLOB) mapped as parameter
          // unlike with textItems that get the BLOBID from the DB,
          // in asKey mode, BLOBID is just map name plus a possible array index.
          // Plugin must be able to identify the BLOB using this plus the item ID.
          // Plugin must also make sure an array element exists (value does not matter, can be empty)
          // for each element that should be proxied here.
          // - create the proxies (one for each array element)
          #ifdef ARRAYFIELD_SUPPORT
          arrayIndex=0;
          #endif
          do {
            // first check if there is an element at all
            string blobid = fmiP->getName(); // map name
            #ifdef ARRAYFIELD_SUPPORT
            if (basefieldP->isArray()) {
              leaffieldP = basefieldP->getArrayField(arrayIndex,true); // get existing leaf fields only
              StringObjAppendPrintf(blobid,"[%d]",arrayIndex); // add array index to blobid
              arrayIndex++;
            }
            else
              leaffieldP = basefieldP; // leaf is base field
            // if no leaf field, we'll need to exit here (we're done with the array)
            if (leaffieldP==NULL) break;
            #else
            leaffieldP = basefieldP; // no arrays: leaf is always base field
            #endif
            // this array element exists, create the proxy
            TApiBlobProxy *apiProxyP = new TApiBlobProxy(this,!leaffieldP->isBasedOn(fty_blob),blobid.c_str(),aItem.getLocalID());
            // Note: we do not support "READNOW" proxies here, as they are useless in ItemKey context: if the
            //       BLOB cannot be read later, no need for as_aparam map exists and plugin should just put the value
            //      directly via the SetKeyValue() API.
            // attach proxy to the string or blob field
            static_cast<TStringField *>(leaffieldP)->setBlobProxy(apiProxyP);
          } while(basefieldP->isArray()); // only arrays do loop all array elements
        }
      } // readable field mapping with explicit as_param mark
    } // for all field mappings
  } // if AsKey access
  #endif // DBAPI_ASKEYITEMS
  // execute afterread script now
  #ifdef SCRIPT_SUPPORT
  // process afterread script
  fPluginAgentP->fScriptContextDatastore=this;
  if (!TScriptContext::execute(fScriptContextP,fPluginDSConfigP->fFieldMappings.fAfterReadScript,fPluginDSConfigP->getDSFuncTableP(),fPluginAgentP,&aItem,true))
    SYSYNC_THROW(TSyncException("<afterreadscript> fatal error"));
  #endif
  // always ok for now %%%
  return true;
} // TPluginApiDS::postReadProcessItem



// - pre-process item before writing to DB (run script)
bool TPluginApiDS::preWriteProcessItem(TMultiFieldItem &aItem)
{
  #ifdef SCRIPT_SUPPORT
  // process beforewrite script
  fWriting=true;
  fDeleting=false;
  fParentKey=aItem.getLocalID();
  fPluginAgentP->fScriptContextDatastore=this;
  if (!TScriptContext::execute(fScriptContextP,fPluginDSConfigP->fFieldMappings.fBeforeWriteScript,fPluginDSConfigP->getDSFuncTableP(),fPluginAgentP,&aItem,true))
    SYSYNC_THROW(TSyncException("<beforewritescript> fatal error"));
  #endif
  // always ok for now %%%
  return true;
} // TPluginApiDS::preWriteProcessItem





// - send BLOBs of this item one by one. Returns true if we have a blob at all
bool TPluginApiDS::writeBlobs(
  bool aAssignedOnly,
  TMultiFieldItem &aItem,
  uInt16 aSetNo
)
{
  TFieldMapList *fmlP = &(fPluginDSConfigP->fFieldMappings.fFieldMapList);
  TFieldMapList::iterator pos;
  TApiFieldMapItem *fmiP;
  string blobfieldname;
  bool blobwritten=false;
  TSyError dberr;

  // store all parametrized fields or BLOBs one by one
  for (pos=fmlP->begin(); pos!=fmlP->end(); pos++) {
    fmiP = static_cast<TApiFieldMapItem *>(*pos);
    if (
      fmiP->writable &&
      fmiP->setNo==aSetNo
    ) {
      TItemField *basefieldP,*leaffieldP;
      sInt16 fid = fmiP->fid;
      // determine base field (might be array)
      basefieldP = getMappedBaseFieldOrVar(aItem,fid);
      // ignore map if we have no field for it
      if (!basefieldP) continue;
      // ignore map if field is not assigned and assignedonly flag is set
      if (aAssignedOnly && basefieldP->isUnassigned()) continue;
      // omit all non-BLOB normal fields
      // Note: in ItemKey mode, blobs must have the as_aparam flag to be written as blobs.
      //       in textItem mode, all fty_blob based fields will ALWAYS be written as blobs.
      if (!(fmiP->as_param || (basefieldP->elementsBasedOn(fty_blob) && !fPluginDSConfigP->fItemAsKey))) continue;
      // yes, we want to write this field as a BLOB
      #ifdef ARRAYFIELD_SUPPORT
      uInt16 arrayIndex;
      for (arrayIndex=0; true; arrayIndex++)
      #endif
      {
        // first create BLOB ID
        blobfieldname=fmiP->getName();
        #ifdef ARRAYFIELD_SUPPORT
        // append array index if this is an array field
        if (basefieldP->isArray()) {
          StringObjAppendPrintf(blobfieldname,"[%d]",arrayIndex);
          // calculate leaf field
          leaffieldP = basefieldP->getArrayField(arrayIndex,true); // get existing leaf fields only
        }
        else {
          leaffieldP = basefieldP; // leaf is base field
        }
        // if no leaf field, we'll need to exit here (we're done with the array)
        if (leaffieldP==NULL) break;
        #else
        leaffieldP = basefieldP; // leaf is base field
        #endif
        // Now write this BLOB or string field
        size_t maxBlobWriteBlockSz;
        size_t blobsize;
        void *bufferP = NULL;
        string strDB;
        bool isString = !leaffieldP->isBasedOn(fty_blob);
        if (!isString) {
          #ifdef STREAMFIELD_SUPPORT
          leaffieldP->resetStream(); // this might be the wrong place to do this ...
          blobsize = leaffieldP->getStreamSize();
          maxBlobWriteBlockSz = 16000;
          // allocate buffer
          if (blobsize)
            bufferP = new unsigned char[blobsize>maxBlobWriteBlockSz ? maxBlobWriteBlockSz : blobsize];
          #else
          // we cannot stream, but just read string contents
          blobsize = leaffieldP->getStringSize();
          maxBlobWriteBlockSz = blobsize; // all at once
          bufferP = (void *) leaffieldP->getCStr(); // get pointer to string
          #endif
        }
        else {
          // is string -> first convert to DB charset
          blobsize = leaffieldP->getStringSize();
          maxBlobWriteBlockSz = blobsize;
          string s;
          leaffieldP->getAsString(s);
          // convert to DB charset and linefeeds
          appendUTF8ToString(
            s.c_str(),
            strDB,
            fPluginDSConfigP->fDataCharSet,
            fPluginDSConfigP->fDataLineEndMode
          );
          bufferP = (void *)strDB.c_str();
        }
        SYSYNC_TRY {
          // now read from stream field and send to API
          bool first=true;
          bool last=false;
          size_t remaining=blobsize;
          size_t bytes,actualbytes;
          while (!last) {
            // it's the last block when we can write remaining bytes now
            last = remaining<=maxBlobWriteBlockSz;
            // calculate block size of this iteration
            bytes = last ? remaining : maxBlobWriteBlockSz;
            #ifdef STREAMFIELD_SUPPORT
            if (!isString) {
              // read these from the stream field
              actualbytes = leaffieldP->readStream(bufferP,bytes); // we need to read
            }
            else
            #endif
            {
              // we have it already in the buffer
              actualbytes = bytes;
            }
            if (actualbytes<bytes) last=false;
            // write to the DB API
            dberr= fDBApi_Data.WriteBlob(
              aItem.getLocalID(),
              blobfieldname.c_str(),
              bufferP,
              actualbytes,
              blobsize,
              first,
              last
            );
            if (dberr!=LOCERR_OK) SYSYNC_THROW(TSyncException("DBapi::WriteBlob fatal error"));
            // not first call any more
            first=false;
            // calculate new remaining
            remaining-=actualbytes;
          } // while not last
          #ifdef STREAMFIELD_SUPPORT
          if (!isString && bufferP) delete (char *)bufferP;
          #endif
        }
        SYSYNC_CATCH (...)
          #ifdef STREAMFIELD_SUPPORT
          if (!isString && bufferP) delete (char *)bufferP;
          #endif
          SYSYNC_RETHROW;
        SYSYNC_ENDCATCH
        // we now have at least one field
        blobwritten=true;
        #ifdef ARRAYFIELD_SUPPORT
        // non-array do not loop
        if (!basefieldP->isArray()) break;
        #endif
      } // for all array elements
    } // if writable BLOB/parametrized field
  } // for all field mappings
  return blobwritten;
} // TPluginApiDS::writeBlobs


bool TPluginApiDS::deleteBlobs(
  bool aAssignedOnly,
  TMultiFieldItem &aItem,
  uInt16 aSetNo
)
{
  TFieldMapList *fmlP = &(fPluginDSConfigP->fFieldMappings.fFieldMapList);
  TFieldMapList::iterator pos;
  TApiFieldMapItem *fmiP;
  string blobfieldname;
  bool blobdeleted=false;
  TSyError dberr;

  // store all parametrized fields or BLOBs one by one
  for (pos=fmlP->begin(); pos!=fmlP->end(); pos++) {
    fmiP = static_cast<TApiFieldMapItem *>(*pos);
    if (fmiP->writable &&
        fmiP->setNo==aSetNo) {
      // get field
      TItemField *basefieldP,*leaffieldP;
      sInt16 fid = fmiP->fid;
      // determine base field (might be array)
      basefieldP = getMappedBaseFieldOrVar(aItem,fid);
      // ignore map if we have no field for it
      if (!basefieldP) continue;

      // %%% what is this, obsolete??
      // ignore map if field is not assigned and assignedonly flag is set
      //if (aAssignedOnly && basefieldP->isUnassigned()) continue;

      // omit all non-BLOB normal fields
      // Note: in ItemKey mode, blobs must have the as_aparam flag to be written as blobs.
      //       in textItem mode, all fty_blob based fields will ALWAYS be written as blobs.
      if (!(fmiP->as_param || (basefieldP->elementsBasedOn(fty_blob) && !fPluginDSConfigP->fItemAsKey))) continue;
      // yes, we want to write this field as a BLOB

      #ifdef ARRAYFIELD_SUPPORT
      uInt16 arrayIndex;
      for (arrayIndex=0; true; arrayIndex++)
      #endif
      {
        // first create BLOB ID
        blobfieldname=fmiP->getName();
        #ifdef ARRAYFIELD_SUPPORT
        // append array index if this is an array field
        if (basefieldP->isArray()) {
          StringObjAppendPrintf(blobfieldname,"[%d]",arrayIndex);
          // calculate leaf field
          leaffieldP= basefieldP->getArrayField(arrayIndex,true); // get existing leaf fields only
        }
        else {
          leaffieldP= basefieldP; // leaf is base field
        }
        // if no leaf field, we'll need to exit here (we're done with the array)
        if (leaffieldP==NULL) break;
        #else
          leaffieldP= basefieldP; // leaf is base field
        #endif

        SYSYNC_TRY {
          dberr= fDBApi_Data.DeleteBlob( aItem.getLocalID(),blobfieldname.c_str() );
          if (dberr==LOCERR_NOTIMP) dberr= LOCERR_OK; // Not implemented is not an error
          if (dberr!=LOCERR_OK) SYSYNC_THROW(TSyncException("DBapi::DeleteBlob fatal error"));
        }
        SYSYNC_CATCH (...)
          SYSYNC_RETHROW;
        SYSYNC_ENDCATCH

        // we now have at least one field
        blobdeleted= true;

        #ifdef ARRAYFIELD_SUPPORT
        // non-array do not loop
        if (!basefieldP->isArray()) break;
        #endif
      } // for all array elements
    } // if writable BLOB/parametrized field
  } // for all field mappings
  return blobdeleted;
} // TPluginApiDS::deleteBlobs



/// returns true if DB implementation supports resume (saving of resume marks, alert code, pending maps, tempGUIDs)
bool TPluginApiDS::dsResumeSupportedInDB(void)
{
  if (fPluginDSConfigP->fDBApiConfig_Admin.Connected()) {
    // we can do resume if plugin supports it
    return fPluginDSConfigP->fDBApiConfig_Admin.Version()>=sInt32(VE_InsertMapItem);
  }
  return inherited::dsResumeSupportedInDB();
} // TPluginApiDS::dsResumeSupportedInDB


#ifdef OBJECT_FILTERING

// - returns true if DB implementation can also apply special filters like CGI-options
//   /dr(x,y) etc. during fetching
bool TPluginApiDS::dsOptionFilterFetchesFromDB(void)
{
  #ifndef SDK_ONLY_SUPPORT
  // if we are not connected, let immediate ancestor check it (e.g. SQL/ODBC)
  if (!fDBApi_Data.Created()) return inherited::dsOptionFilterFetchesFromDB();
  #endif
  #ifdef SYSYNC_TARGET_OPTIONS
  string rangeFilter,s;
  // check range filtering
  // - date start end
  rangeFilter = "daterangestart:";
  TimestampToISO8601Str(s, fDateRangeStart, TCTX_UTC, false, false);
  rangeFilter += s.c_str();
  // - date range end
  rangeFilter += "\r\ndaterangeend:";
  TimestampToISO8601Str(s, fDateRangeEnd, TCTX_UTC, false, false);
  rangeFilter += s.c_str();
  // %%% tbd:
  // - attachments inhibit
  // - size limit
  #if (!defined _MSC_VER || defined WINCE) && !defined(__GNUC__)
  #warning "attachments and limit filters not yet supported"
  #endif
  // - let plugin know and check (we can filter at DBlevel if plugin understands both start/end)
  rangeFilter += "\r\n";
  bool canfilter =
    (fDBApi_Data.FilterSupport(rangeFilter.c_str())>=2) ||
    (fDateRangeStart==0 && fDateRangeEnd==0); // no range can always be "filtered"
  // if we can filter, that's sufficient
  if (canfilter) return true;
  #else
  // there is no range ever: yes, we can filter
  return true;
  #endif
  // otherwise, let implementation test (not immediate anchestor, which is a different API like ODBC)
  return TCustomImplDS::dsOptionFilterFetchesFromDB();
} // TPluginApiDS::dsOptionFilterFetchesFromDB


// - returns true if DB implementation can filter the standard filters
//   (LocalDBFilter, TargetFilter and InvisibleFilter) during database fetch
//   - otherwise, fetched items will be filtered after being read from DB.
bool TPluginApiDS::dsFilteredFetchesFromDB(bool aFilterChanged)
{
  #ifndef SDK_ONLY_SUPPORT
  // if we are not connected, let immediate ancestor check it (e.g. SQL/ODBC)
  // Note that this can happen when dsFilteredFetchesFromDB() is called via Alertscript to resolve filter dependencies
  //   before the DS is actually connected. In this case, the return value is not checked so that's ok.
  //   Before actually laoding or zapping the sync set this is called once again with connected data plugin.
  if (!fDBApi_Data.Created()) return inherited::dsFilteredFetchesFromDB(aFilterChanged);
  #endif
  if (aFilterChanged || !fAPIFiltersTested) {
    fAPIFiltersTested = true;
    // Anyway, let DBApi know (even if all filters are empty)
    string filters;
    // - local DB filter (=static filter, from config)
    filters = "staticfilter:";
    filters += fLocalDBFilter.c_str();
    // - dynamic sync set filter
    filters += "\r\ndynamicfilter:";
    filters += fSyncSetFilter.c_str();
    // - invisible filter (those that MATCH this filter should NOT be included)
    filters += "\r\ninvisiblefilter:";
    filters += fPluginDSConfigP->fInvisibleFilter.c_str();
    // - let plugin know and check (we can filter at DBlevel if plugin understands both start/end)
    filters += "\r\n";
    fAPICanFilter =
      (fDBApi_Data.FilterSupport(filters.c_str())>=3) ||
      (fLocalDBFilter.empty() && fSyncSetFilter.empty() && fPluginDSConfigP->fInvisibleFilter.empty()); // no filter set = we can "filter"
  }
  // if we can filter, that's sufficient
  if (fAPICanFilter) return true;
  // otherwise, let implementation test (not immediate anchestor, which might be a different API like ODBC)
  return TCustomImplDS::dsFilteredFetchesFromDB(aFilterChanged);
} // TPluginApiDS::dsFilteredFetchesFromDB

#endif



// can return 508 to force a slow sync. Other errors abort the sync
localstatus TPluginApiDS::apiEarlyDataAccessStart(void)
{
  TSyError dberr = LOCERR_OK;
  if (fPluginDSConfigP->fEarlyStartDataRead) {
    // prepare
    dberr = apiPrepareReadSyncSet();
    if (dberr==LOCERR_OK) {
      // start the reading phase anyway (to make sure call order is always StartRead/EndRead/StartWrite/EndWrite)
      dberr = fDBApi_Data.StartDataRead(fPreviousToRemoteSyncIdentifier.c_str(),fPreviousSuspendIdentifier.c_str());
      if (dberr!=LOCERR_OK) {
        PDEBUGPRINTFX(DBG_ERROR,("apiEarlyDataAccessStart - DBapi::StartDataRead error: %hd",dberr));
      }
    }
  }
  return dberr;
}


// prepare for reading the sync set
localstatus TPluginApiDS::apiPrepareReadSyncSet(void)
{
  TSyError dberr = LOCERR_OK;
  #ifdef BASED_ON_BINFILE_CLIENT
  if (binfileDSActive()) {
    // we need to create the context for the data plugin here, as loadAdminData is not called in BASED_ON_BINFILE_CLIENT case.
    dberr = connectDataPlugin();
    if (dberr==LOCERR_OK) {
      if (!fDBApi_Data.Created()) {
        // - use datastore name as context name and link with session context
        dberr = fDBApi_Data.CreateContext(
          getName(), false,
          &(fPluginDSConfigP->fDBApiConfig_Data),
          "anydevice", // no real device key
          "singleuser", // no real user key
          NULL // no associated session level // fPluginAgentP->getDBApiSession()
        );
        if (dberr==LOCERR_OK) {
          // make sure plugin now sees filters before starting to read sync set
          // Note: due to late instantiation of the data plugin, previous calls to engFilteredFetchesFromDB() were not
          //   evaluated by the plugin, so we need to do that here explicitly once again
          engFilteredFetchesFromDB(false);
        }
      }
    }
    else if (dberr==LOCERR_NOTIMP)
      dberr=LOCERR_OK; // we just don't have a data plugin, that's ok
  } // binfile active
  #endif // BASED_ON_BINFILE_CLIENT
  return dberr;
}




// read sync set IDs and mod dates (and rest of data if technically unavoidable or
// requested by aNeedAll)
localstatus TPluginApiDS::apiReadSyncSet(bool aNeedAll)
{
  TSyError dberr=LOCERR_OK;
  #ifdef SYDEBUG
  string ts1,ts2;
  #endif

  if (!fPluginDSConfigP->fEarlyStartDataRead) {
    // normal sequence, start data read is not called before starting to read the sync set
    dberr = apiPrepareReadSyncSet();
    if (dberr!=LOCERR_OK)
      goto endread;
  }
  #ifndef SDK_ONLY_SUPPORT
  // only handle here if we are in charge - otherwise let ancestor handle it
  if (!fDBApi_Data.Created())
    return inherited::apiReadSyncSet(aNeedAll);
  #endif

  // just let plugin know if we want data (if it actually does is the plugin's choice)
  if (aNeedAll) {
    // we'll need all data in the datastore in the end, let datastore know
    // Note: this is a suggestion to the plugin only - plugin does not need to follow it
    //       and can return only ID/changed or all data for both states of this flag,
    //       even changing on a item-by-item basis (can make sense for optimization).
    //       The Plugin will return "1" here only in case it really follows the suggestion
    //       an WILL return all data at ReadNextItem(). Otherwise, it may or may
    //       not return data on a item by item basis (which is handled by the code
    //       below). Therefore, at this time, the engine does not make use of the
    //       ContextSupport() return value here.
    fDBApi_Data.ContextSupport("ReadNextItem:allfields\n\r");
  }
  #ifdef SCRIPT_SUPPORT
  // process init script
  fParentKey.erase();
  fWriting=false;
  fPluginAgentP->fScriptContextDatastore=this;
  if (!TScriptContext::executeTest(true,fScriptContextP,fPluginDSConfigP->fFieldMappings.fInitScript,fPluginDSConfigP->getDSFuncTableP(),fPluginAgentP)) {
    PDEBUGPRINTFX(DBG_ERROR,("<initscript> failed"));
    goto endread;
  }
  #endif
  // start reading
  // - read list of all local IDs that are in the current sync set
  DeleteSyncSet();
  #ifdef SYDEBUG
  StringObjTimestamp(ts1,getPreviousToRemoteSyncCmpRef());
  StringObjTimestamp(ts2,getPreviousSuspendCmpRef());
  PDEBUGPRINTFX(DBG_DATA,(
    "Now reading local sync set: report changes since reference1 at %s, and since reference2 at %s",
    ts1.c_str(),
    ts2.c_str()
  ));
  #endif
  if (!fPluginDSConfigP->fEarlyStartDataRead) {
    // start the reading phase anyway (to make sure call order is always StartRead/EndRead/StartWrite/EndWrite)
    dberr = fDBApi_Data.StartDataRead(fPreviousToRemoteSyncIdentifier.c_str(),fPreviousSuspendIdentifier.c_str());
    if (dberr!=LOCERR_OK) {
      PDEBUGPRINTFX(DBG_ERROR,("DBapi::StartDataRead fatal error: %hd",dberr));
      goto endread;
    }
  }
  // we don't need to load the syncset if we are only refreshing from remote
  // but we also must load it if we can't zap without it on slow refresh, or when we can't retrieve items on non-slow refresh
  // (we won't retrieve anything in case of slow refresh, because after zapping there's nothing left by definition)
  if (!fRefreshOnly || (fSlowSync && apiNeedSyncSetToZap()) || (!fSlowSync && implNeedSyncSetToRetrieve())) {
    SYSYNC_TRY {
      // true for initial ReadNextItem*() call, false later on
      bool firstReadNextItem=true;

      // read the items
      #if defined(DBAPI_ASKEYITEMS) && defined(ENGINEINTERFACE_SUPPORT)
      TMultiFieldItem *mfitemP = NULL;
      #endif
      #ifdef DBAPI_TEXTITEMS
      TDB_Api_Str itemData;
      #endif
      do {
        // read next item
        int itemstatus;
        TSyncSetItem *syncSetItemP=NULL;
        TDB_Api_ItemID itemAndParentID;
        // two API variants
        #if defined(DBAPI_ASKEYITEMS) && defined(ENGINEINTERFACE_SUPPORT)
        if (fPluginDSConfigP->fItemAsKey) {
          // ALWAYS prepare a multifield item, in case plugin wants to return data (it normally does not
          // unless queried with ContextSupport("ReadNextItem:allfields"), but it's a per-item decision
          // of the plugin itself (and probably overall optimization considerations for speed or memory)
          // if it wants to follow the recommendation set with "ReadNextItem:allfields".
          // Note: check for canCreateItemForRemote() should be always true now as loading syncset has been
          //       moved within the progress of client sync session to a point where types ARE known.
          //       Pre-3.2 engines however called this routine early so types could be unknown here.
          if (mfitemP==NULL && canCreateItemForRemote()) {
            mfitemP =
              (TMultiFieldItem *) newItemForRemote(
                ity_multifield
              );
          }
          // as key (Note: will be functional key but w/o any fields in case we pass NULL item pointer)
          TDBItemKey *itemKeyP = newDBItemKey(mfitemP);
          dberr=fDBApi_Data.ReadNextItemAsKey(itemAndParentID, (KeyH)itemKeyP, itemstatus, firstReadNextItem);
          // check if plugin wrote something to our key. If so, we assume this is the item and save
          // it, EVEN IF we did not request getting item data.
          if (!itemKeyP->isWritten()) {
            // nothing in this item, forget it
            delete itemKeyP; // key first
            if (mfitemP) delete mfitemP; // then item if we had one at all
            mfitemP=NULL;
          }
          else {
            // got item, delete the key
            delete itemKeyP;
            // post-process (run scripts, create BLOB proxies if needed)
            postReadProcessItem(*mfitemP,0);
          }
        }
        else
        #endif
        #ifdef DBAPI_TEXTITEMS
        {
          // as text item
          dberr=fDBApi_Data.ReadNextItem(itemAndParentID, itemData, itemstatus, firstReadNextItem);
        }
        #else
        return LOCERR_WRONGUSAGE; // completely wrong usage - should never happen as compatibility is tested at module connect
        #endif
        firstReadNextItem=false;
        if (dberr!=LOCERR_OK) {
          PDEBUGPRINTFX(DBG_ERROR,("DBapi::ReadNextItem fatal error = %hd",dberr));
          #if defined(DBAPI_ASKEYITEMS) && defined(ENGINEINTERFACE_SUPPORT)
          if (mfitemP) delete mfitemP;
          #endif
          goto endread;
        }
        // check if we have seen all items
        if (itemstatus==ReadNextItem_EOF)
          break;
        // we have received an item
        // - save returned data as item in the syncsetlist
        syncSetItemP = new TSyncSetItem;
        // - copy item object ID
        syncSetItemP->localid = itemAndParentID.item.c_str();
        // - copy parent ID
        // %%% tbd, now empty
        syncSetItemP->containerid = "";
        // - set modified status
        syncSetItemP->isModifiedAfterSuspend = itemstatus==ReadNextItem_Resumed;
        syncSetItemP->isModified = syncSetItemP->isModifiedAfterSuspend || itemstatus==ReadNextItem_Changed;
        #ifdef SYDEBUG
        PDEBUGPRINTFX(DBG_DATA+DBG_EXOTIC,(
          "read local item info in sync set: localid='%s'%s%s",
          syncSetItemP->localid.c_str(),
          syncSetItemP->isModified ? ", MODIFIED since reference1" : "",
          syncSetItemP->isModifiedAfterSuspend ? " AND since reference2" : ""
        ));
        #endif
        // no data yet, no item yet
        syncSetItemP->itemP = NULL;
        // two API variants
        #if defined(DBAPI_ASKEYITEMS) && defined(ENGINEINTERFACE_SUPPORT)
        if (fPluginDSConfigP->fItemAsKey) {
          // as key
          if (mfitemP) {
            // we have read some data
            syncSetItemP->itemP = mfitemP;
            mfitemP = NULL; // now owned by syncSetItem
            syncSetItemP->itemP->setLocalID(itemAndParentID.item.c_str());
          }
        }
        else
        #endif
        #ifdef DBAPI_TEXTITEMS
        {
          // as text item
          // - if we have received actual item data already, create and store an item here
          if (!itemData.empty()) {
            // store data in new item now
            // - create new empty TMultiFieldItem
            syncSetItemP->itemP =
              (TMultiFieldItem *) newItemForRemote(
                ity_multifield
              );
            // - set localid as we might need it for reading specials or arrays
            syncSetItemP->itemP->setLocalID(itemAndParentID.item.c_str());
            // - read data into item
            parseItemData(*(syncSetItemP->itemP),itemData.c_str(),0);
          }
        }
        #else
        return LOCERR_WRONGUSAGE; // completely wrong usage - should never happen as compatibility is tested at module connect
        #endif
        // now save syncset item
        fSyncSetList.push_back(syncSetItemP);
      } while (true);
    } // try
    SYSYNC_CATCH (...)
      dberr=LOCERR_EXCEPTION;
    SYSYNC_ENDCATCH
  } // if we need the syncset at all
endread:
  // then end read here
  if (dberr==LOCERR_OK) {
    dberr=fDBApi_Data.EndDataRead();
    if (dberr!=LOCERR_OK) {
      PDEBUGPRINTFX(DBG_ERROR,("DBapi::EndDataRead failed, err=%hd",dberr));
    }
  }
  return dberr;
} // TPluginApiDS::apiReadSyncSet



// Check if we need the syncset to zap
bool TPluginApiDS::apiNeedSyncSetToZap(void)
{
  #ifndef SDK_ONLY_SUPPORT
  // only handle here if we are in charge - otherwise let ancestor handle it
  if (!fDBApi_Data.Created()) return inherited::apiNeedSyncSetToZap();
  #endif
  // only if we have deleteSyncSet on API level AND api can also apply all filters, we don't need the syncset to zap the datastore
  return !(fPluginDSConfigP->fHasDeleteSyncSet && engFilteredFetchesFromDB(false));
} // TPluginApiDS::apiNeedSyncSetToZap


// Zap all data in syncset (note that everything outside the sync set will remain intact)
localstatus TPluginApiDS::apiZapSyncSet(void)
{
  #ifndef SDK_ONLY_SUPPORT
  // only handle here if we are in charge - otherwise let ancestor handle it
  if (!fDBApi_Data.Created()) return inherited::apiZapSyncSet();
  #endif
  TSyError dberr = LOCERR_OK;
  // API must be able to process current filters in order to execute a zap - otherwise we would delete
  // more than the sync set defined by local filters.
  bool apiCanZap = engFilteredFetchesFromDB(false);
  if (apiCanZap) {
    // try to use plugin's specialized implementation
    dberr = fDBApi_Data.DeleteSyncSet();
    apiCanZap = dberr!=LOCERR_NOTIMP; // API claims to be able to zap (but still might have failed with a DBerr in this case!)
  }
  // do it one by one if DeleteAllItems() is not implemented or plugin cannot apply current filters
  if (!apiCanZap) {
    dberr = zapSyncSetOneByOne();
  }
  // return status
  return dberr;
} // TPluginApiDS::apiZapSyncSet


// fetch actual record from DB by localID. SyncSetItem might be passed to give additional information
// such as containerid
localstatus TPluginApiDS::apiFetchItem(TMultiFieldItem &aItem, bool aReadPhase, TSyncSetItem *aSyncSetItemP)
{
  #ifndef SDK_ONLY_SUPPORT
  // only handle here if we are in charge - otherwise let ancestor handle it
  if (!fDBApi_Data.Created()) return inherited::apiFetchItem(aItem, aReadPhase, aSyncSetItemP);
  #endif

  TSyError dberr=LOCERR_OK;
  ItemID_Struct itemAndParentID;

  // set up item ID and parent ID
  itemAndParentID.item=(appCharP)aItem.getLocalID();
  itemAndParentID.parent=const_cast<char *>("");

  // two API variants
  #if defined(DBAPI_ASKEYITEMS) && defined(ENGINEINTERFACE_SUPPORT)
  if (fPluginDSConfigP->fItemAsKey) {
    // get key
    TDBItemKey *itemKeyP = newDBItemKey(&aItem);
    // let plugin use it to fill item
    dberr=fDBApi_Data.ReadItemAsKey(itemAndParentID,(KeyH)itemKeyP);
    if (itemKeyP->isWritten()) {
      // post-process (run scripts, create BLOB proxies if needed)
      postReadProcessItem(aItem,0);
    }
    // done with the key
    delete itemKeyP;
  }
  else
  #endif
  #ifdef DBAPI_TEXTITEMS
  {
    TDB_Api_Str itemData;
    // read the item in text form from the DB
    dberr=fDBApi_Data.ReadItem(itemAndParentID,itemData);
    if (dberr==LOCERR_OK) {
      // put it into aItem
      parseItemData(aItem,itemData.c_str(),0);
    }
  }
  #else
  return LOCERR_WRONGUSAGE; // completely wrong usage - should never happen as compatibility is tested at module connect
  #endif
  // return status
  return dberr;
} // TPluginApiDS::apiFetchItem



// start of write
localstatus TPluginApiDS::apiStartDataWrite(void)
{
  #ifndef SDK_ONLY_SUPPORT
  // only handle here if we are in charge - otherwise let ancestor handle it
  if (!fDBApi_Data.Created()) return inherited::apiStartDataWrite();
  #endif

  TSyError dberr=fDBApi_Data.StartDataWrite();
  if (dberr!=LOCERR_OK) {
    PDEBUGPRINTFX(DBG_ERROR,("DBapi::StartDataWrite returns dberr=%hd",dberr));
  }
  return dberr;
} // TPluginApiDS::apiStartDataWrite



// add new item to datastore, returns created localID
localstatus TPluginApiDS::apiAddItem(TMultiFieldItem &aItem, string &aLocalID)
{
  #ifndef SDK_ONLY_SUPPORT
  // only handle here if we are in charge - otherwise let ancestor handle it
  if (!fDBApi_Data.Created()) return inherited::apiAddItem(aItem, aLocalID);
  #endif

  TSyError dberr=LOCERR_OK;
  TDB_Api_ItemID itemAndParentID;

  // prepare data from item
  #ifdef SCRIPT_SUPPORT
  fInserting=true; // flag for script, we are inserting new record
  #endif
  // two API variants
  #if defined(DBAPI_ASKEYITEMS) && defined(ENGINEINTERFACE_SUPPORT)
  if (fPluginDSConfigP->fItemAsKey) {
    // preprocess
    if (!preWriteProcessItem(aItem)) return 510; // DB error
    // get key
    TDBItemKey *itemKeyP = newDBItemKey(&aItem);
    // let plugin use it to obtain data to write
    dberr=fDBApi_Data.InsertItemAsKey((KeyH)itemKeyP,"",itemAndParentID);
    // done with the key
    delete itemKeyP;
  }
  else
  #endif
  #ifdef DBAPI_TEXTITEMS
  {
    string itemData;
    generateDBItemData(
      false, // all fields, not only assigned ones
      aItem,
      0, // we do not use different sets for now
      itemData // here we'll get the data
    );
    // now insert main record
    dberr=fDBApi_Data.InsertItem(itemData.c_str(),"",itemAndParentID);
  }
  #else
  return LOCERR_WRONGUSAGE; // completely wrong usage - should never happen as compatibility is tested at module connect
  #endif
  // now check result
  if (dberr==LOCERR_OK ||
      dberr==DB_Conflict ||
      dberr==DB_DataReplaced ||
      dberr==DB_DataMerged) {
    // save new ID
    aLocalID = itemAndParentID.item.c_str();
    aItem.setLocalID(aLocalID.c_str()); // make sure item itself has correct ID as well
    if (dberr!=DB_Conflict) {
      // now write all the BLOBs
      writeBlobs(false,aItem,0);
      #ifdef SCRIPT_SUPPORT
      // process overall afterwrite script
      fWriting=true;
      fInserting=true;
      fDeleting=false;
      fPluginAgentP->fScriptContextDatastore=this;
      if (!TScriptContext::execute(fScriptContextP,fPluginDSConfigP->fFieldMappings.fAfterWriteScript,fPluginDSConfigP->getDSFuncTableP(),fPluginAgentP,&aItem,true)) {
        PDEBUGPRINTFX(DBG_ERROR,("<afterwritescript> failed"));
        dberr = LOCERR_WRONGUSAGE;
      }
      #endif
    }
  }
  // return status
  return dberr;
} // TPluginApiDS::apiAddItem


#ifdef SYSYNC_CLIENT

/// finalize local ID (for datastores that can't efficiently produce these at insert)
bool TPluginApiDS::dsFinalizeLocalID(string &aLocalID)
{
  #ifndef SDK_ONLY_SUPPORT
  // only handle here if we are in charge - otherwise let ancestor handle it
  if (!fDBApi_Data.Created()) return inherited::dsFinalizeLocalID(aLocalID);
  #endif

  TDB_Api_Str finalizedID;
  localstatus sta = fDBApi_Data.FinalizeLocalID(aLocalID.c_str(),finalizedID);
  if (sta==LOCERR_OK && !finalizedID.empty()) {
    // pass modified ID back
    aLocalID = finalizedID.c_str();
    // ID was updated
    return true;
  }
  // no change - ID is ok as-is
  return false;
} // TPluginApiDS::dsFinalizeLocalID

#endif // SYSYNC_CLIENT



// update existing item in datastore, returns 404 if item not found
localstatus TPluginApiDS::apiUpdateItem(TMultiFieldItem &aItem)
{
  #ifndef SDK_ONLY_SUPPORT
  // only handle here if we are in charge - otherwise let ancestor handle it
  if (!fDBApi_Data.Created()) return inherited::apiUpdateItem(aItem);
  #endif

  TSyError dberr=LOCERR_OK;
  TDB_Api_ItemID updItemAndParentID;
  ItemID_Struct itemAndParentID;

  // set up item ID and parent ID
  itemAndParentID.item=(appCharP)aItem.getLocalID();
  itemAndParentID.parent=const_cast<char *>("");

  // prepare data from item
  #ifdef SCRIPT_SUPPORT
  fInserting=false; // flag for script, we are updating, not inserting now
  #endif
  // two API variants
  #if defined(DBAPI_ASKEYITEMS) && defined(ENGINEINTERFACE_SUPPORT)
  if (fPluginDSConfigP->fItemAsKey) {
    // preprocess
    if (!preWriteProcessItem(aItem)) return 510; // DB error
    // get key
    TDBItemKey *itemKeyP = newDBItemKey(&aItem);
    // let plugin use it to obtain data to write
    dberr=fDBApi_Data.UpdateItemAsKey((KeyH)itemKeyP,itemAndParentID,updItemAndParentID);
    // done with the key
    delete itemKeyP;
  }
  else
  #endif
  #ifdef DBAPI_TEXTITEMS
  {
    string itemData;
    generateDBItemData(
      true, // only assigned fields
      aItem,
      0, // we do not use different sets for now
      itemData // here we'll get the data
    );
    // now update main record
    dberr=fDBApi_Data.UpdateItem(itemData.c_str(),itemAndParentID,updItemAndParentID);
  }
  #else
  return LOCERR_WRONGUSAGE; // completely wrong usage - should never happen as compatibility is tested at module connect
  #endif
  if (dberr==LOCERR_OK) {
    // check if ID has changed
    if (!updItemAndParentID.item.empty() && strcmp(updItemAndParentID.item.c_str(),aItem.getLocalID())!=0) {
      if (IS_SERVER) {
        // update item ID and Map
        dsLocalIdHasChanged(aItem.getLocalID(),updItemAndParentID.item.c_str());
      }
      // - update in this item we have here as well
      aItem.setLocalID(updItemAndParentID.item.c_str());
      aItem.updateLocalIDDependencies();
    }
    // now write all the BLOBs
    writeBlobs(true,aItem,0);
    #ifdef SCRIPT_SUPPORT
    // process overall afterwrite script
    fWriting=true;
    fInserting=false;
    fDeleting=false;
    fPluginAgentP->fScriptContextDatastore=this;
    if (!TScriptContext::execute(fScriptContextP,fPluginDSConfigP->fFieldMappings.fAfterWriteScript,fPluginDSConfigP->getDSFuncTableP(),fPluginAgentP,&aItem,true)) {
      PDEBUGPRINTFX(DBG_ERROR,("<afterwritescript> failed"));
      dberr = LOCERR_WRONGUSAGE;
    }
    #endif
  }
  // return status
  return dberr;
} // TPluginApiDS::apiUpdateItem


// delete existing item in datastore, returns 211 if not existing any more
localstatus TPluginApiDS::apiDeleteItem(TMultiFieldItem &aItem)
{
  #ifndef SDK_ONLY_SUPPORT
  // only handle here if we are in charge - otherwise let ancestor handle it
  if (!fDBApi_Data.Created()) return inherited::apiDeleteItem(aItem);
  #endif

  TSyError dberr=LOCERR_OK;

  // delete item
  dberr=fDBApi_Data.DeleteItem( aItem.getLocalID() );
  if (dberr==LOCERR_OK) {
    deleteBlobs(true,aItem,0); // Item related blobs must be removed as well
    #ifdef SCRIPT_SUPPORT
    // process overall afterwrite script
    fWriting=true;
    fInserting=false;
    fDeleting=true;
    fPluginAgentP->fScriptContextDatastore=this;
    if (!TScriptContext::execute(fScriptContextP,fPluginDSConfigP->fFieldMappings.fAfterWriteScript,fPluginDSConfigP->getDSFuncTableP(),fPluginAgentP,&aItem,true)) {
      PDEBUGPRINTFX(DBG_ERROR,("<afterwritescript> failed"));
      dberr = LOCERR_WRONGUSAGE;
    }
    #endif
  } // if

  // return status
  return dberr;
} // TPluginApiDS::apiDeleteItem




// - end DB data write sequence (but not yet admin data), returns DB-specific identifier for this sync (if any)
localstatus TPluginApiDS::apiEndDataWrite(string &aThisSyncIdentifier)
{
  #ifndef SDK_ONLY_SUPPORT
  // only handle here if we are in charge - otherwise let ancestor handle it
  if (!fDBApi_Data.Created()) return inherited::apiEndDataWrite(aThisSyncIdentifier);
  #endif

  // nothing special to do in ODBC case, as we do not have a separate sync identifier
  TDB_Api_Str newSyncIdentifier;
  TSyError sta = fDBApi_Data.EndDataWrite(true, newSyncIdentifier);
  aThisSyncIdentifier=newSyncIdentifier.c_str();
  return sta;
} // TPluginApiDS::apiEndDataWrite



// must be called before starting a thread. If returns false, starting a thread now
// is not allowed and must be postponed.
// Includes ThreadMayChange() call
bool TPluginApiDS::startingThread(void)
{
  // %%% tbd: if modules are completely independent, we might not need this in all cases
  if (!dbAccessLocked()) {
    static_cast<TPluginApiAgent *>(fSessionP)->fApiLocked=true;
    // Now post possible thread change to the API
    // - on the database level
    ThreadMayChangeNow();
    // - on the session level as well (to make sure, and because we will post a change back
    //   at the end of the thread, which will be called FROM the new thread, which constitutes
    //   executing something in the session context.
    static_cast<TPluginApiAgent *>(fSessionP)->getDBApiSession()->ThreadMayChangeNow();
    return true;
  }
  else
    return false;
} // TPluginApiDS::startingThread


// - must be called when a thread's activity has ended
//   BUT THE CALL MUST BE FROM THE ENDING THREAD, not the main thread!
void TPluginApiDS::endingThread(void) {
  // thread may change for the API now again
  // - on the database level
  ThreadMayChangeNow();
  // - on the session level as well
  static_cast<TPluginApiAgent *>(fSessionP)->getDBApiSession()->ThreadMayChangeNow();
  // Now other threads are allowed to access the API again
  static_cast<TPluginApiAgent *>(fSessionP)->fApiLocked=false;
} // TPluginApiDS::endingThread


// should be called before doing DB accesses that might be locked (e.g. because another thread is using the DB resources)
bool TPluginApiDS::dbAccessLocked(void)
{
  return static_cast<TPluginApiAgent *>(fSessionP)->fApiLocked;
} // TPluginApiDS::dbAccessLocked


// - alert possible thread change to plugins
//   Does not check if API is locked or not, see dsThreadMayChangeNow()
void TPluginApiDS::ThreadMayChangeNow(void)
{
  // let API know, thread might change for next request (but not necessarily does!)
  if (fDBApi_Data.Created()) fDBApi_Data.ThreadMayChangeNow();
  if (fDBApi_Admin.Created()) fDBApi_Admin.ThreadMayChangeNow();
} // TPluginApiDS::ThreadMayChangeNow


// - engine Thread might change
void TPluginApiDS::dsThreadMayChangeNow(void)
{
  // Do not post thread change infos when DB access is locked.
  // If it is locked, the thread that locked it will call a
  // ThreadMayChangeNow() to the API when the thread terminates (in endingThread()).
  if (!dbAccessLocked()) {
    ThreadMayChangeNow();
  }
  // let ancestor do it's own stuff
  inherited::dsThreadMayChangeNow();
} // TPluginApiDS::dsThreadMayChangeNow



// - connect data handling part of plugin, Returns LOCERR_NOTIMPL when no data plugin is selected
//   Note: this is either called as part of apiLoadAdminData (even if plugin is NOT responsible for data!)
//         or directly before startDataRead (in BASED_ON_BINFILE_CLIENT binfileDSActive() case)
TSyError TPluginApiDS::connectDataPlugin(void)
{
  TSyError err = LOCERR_NOTIMP;
  // filtering capabilities need to be reevaluated anyway
  fAPICanFilter = false;
  fAPIFiltersTested = false;
  // only connect if we have plugin data support
  if (fPluginDSConfigP->fDBApiConfig_Data.Connected()) {
    err = LOCERR_OK;
    DB_Callback cb= &fDBApi_Data.fCB.Callback;
    cb->callbackRef       = fSessionP; // the session
    #ifdef ENGINEINTERFACE_SUPPORT
    cb->thisBase          = fPluginDSConfigP->getSyncAppBase()->fEngineInterfaceP;
    #endif
    #ifdef SYDEBUG
    // Datastore Data access debug goes to session log
    cb->debugFlags        = PDEBUGTEST(DBG_DATA+DBG_DBAPI+DBG_PLUGIN) ? 0xFFFF : 0;
    cb->DB_DebugPuts      = SessionLogDebugPuts;
    cb->DB_DebugBlock     = SessionLogDebugBlock;
    cb->DB_DebugEndBlock  = SessionLogDebugEndBlock;
    cb->DB_DebugEndThread = SessionLogDebugEndThread;
    cb->DB_DebugExotic    = SessionLogDebugExotic;
    #endif // SYDEBUG
    #ifdef ENGINEINTERFACE_SUPPORT
    // Data module can use Get/SetValue for "AsKey" routines and for session script var access
    // Note: these are essentially context free and work without a global call-in structure
    //       (which is not necessarily there, for example in no-library case)
    CB_Connect_KeyAccess(cb); // connect generic key access routines
    // Version of OpenSessionKey that implicitly opens a key for the current session (DB plugins
    // do not have a session handle, as their use is always implicitly in a session context).
    cb->ui.OpenSessionKey = SessionOpenSessionKey;
    #endif // ENGINEINTERFACE_SUPPORT
  }
  return err;
} // connectDataPlugin


#ifndef BINFILE_ALWAYS_ACTIVE

/// @brief save admin data
///   Must save the following items:
///   - fRemoteSyncAnchor = anchor string used by remote party for this session
///   - fThisLocalAnchor  = anchor (beginning of session) timestamp for this sync
///   - fThisSync         = timestamp for this sync (same as fThisLocalAnchor unless fSyncTimeStampAtEnd config is set)
///   - fLastToRemoteLocalAnchor = timestamp for anchor (beginning of session) of last session that sent data to remote
///                         (same as fThisLocalAnchor unless we did a refrehs-from-remote session)
///   - fLastToRemoteSync = timestamp for last session that sent data to remote
///                         (same as fThisSync unless we did a refresh-from-remote session)
///   - fLastToRemoteSyncIdentifier = string identifying last session that sent data to remote (needs only be saved
///                         if derived datastore cannot work with timestamps and has its own identifier).
///   - fMapTable         = list<TMapEntry> containing map entries. For each entry the implementation must:
///                         - if changed==false: the entry hasn't been changed, so no DB operation is required
///                         - if changed==true and remoteid is not empty:
///                           - if added==true, add the entry as a new record to the DB
///                           - if added==false, update the entry in the DB with matching localid
///                         - if changed==true and remoteid is empty and added==false: delete the entry(s) in the DB with matching localid
///   For resumable datastores (fConfigP->fResumeSupport==true):
///   - fMapTable         = In addition to the above, the markforresume flag must be saved in the mapflags
//                          when it is not equal to the savedmark flag - independently of added/deleted/changed.
///   - fResumeAlertCode  = alert code of current suspend state, 0 if none
///   - fPreviousSuspendCmpRef = reference time of last suspend (used to detect items modified during a suspend / resume)
///   - fPreviousSuspendIdentifier = identifier of last suspend (used to detect items modified during a suspend / resume)
///                         (needs only be saved if derived datastore cannot work with timestamps and has
///                         its own identifier)
///
///   For datastores that can resume in middle of a chunked item (fConfigP->fResumeItemSupport==true):
///   - fPartialItemState = state of partial item (TPartialItemState enum):
///                         - if pi_state_none: save params, delete BLOB data (empty data)
///                         - if pi_state_save_incoming: save params+BLOB, save as in DB such that we will get pi_state_loaded_incoming when loaded again
///                         - if pi_state_save_outgoing: save params+BLOB, save as in DB such that we will get pi_state_loaded_outgoing when loaded again
///                         - if pi_state_loaded_incoming: no need to save, as params+BLOB have not changed since last save (but currently saved params+BLOB in DB must be retained)
///                         - if pi_state_loaded_outgoing: no need to save, as params+BLOB have not changed since last save (but currently saved params+BLOB in DB must be retained)
///
///   - fLastItemStatus   = status code (TSyError) of last item
///   - fLastSourceURI    = item ID (string, if limited in length should be long enough for large IDs, >=64 chars recommended)
///   - fLastTargetURI    = item ID (string, if limited in length should be long enough for large IDs, >=64 chars recommended)
///   - fPITotalSize      = uInt32, total item size
///   - fPIUnconfirmedSize= uInt32, unconfirmed part of item size
///   - fPIStoredSize     = uInt32, size of BLOB to store, 0=none
///   - fPIStoredDataP    = void *, BLOB data, NULL if none
///
/// @param aDataCommitted[in] indicates if data has been committed to the database already or not
/// @param aSessionFinished[in] indicates if this is a final, end-of-session admin save (otherwise, it's only a resume state save)
localstatus TPluginApiDS::apiSaveAdminData(bool aDataCommitted, bool aSessionFinished)
{
  // security - don't use API when locked
  if (dbAccessLocked()) return 503; // service unavailable

  const char* PIStored = "PIStored"; // blob name field

  #ifndef SDK_ONLY_SUPPORT
  // only handle here if we are in charge - otherwise let ancestor handle it
  if (!fDBApi_Admin.Created()) return inherited::apiSaveAdminData(aDataCommitted, aSessionFinished);
  #endif

  localstatus sta=LOCERR_OK;
  TMapContainer::iterator pos;

  // save the entire map list differentially
  pos=fMapTable.begin();
  PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("apiSaveAdminData: internal map table has %ld entries (normal and others)",(long)fMapTable.size()));
  while (pos!=fMapTable.end()) {
    DEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
      "apiSaveAdminData: entryType=%s, localid='%s', remoteID='%s', mapflags=0x%lX, changed=%d, deleted=%d, added=%d, markforresume=%d, savedmark=%d",
      MapEntryTypeNames[(*pos).entrytype],
      (*pos).localid.c_str(),
      (*pos).remoteid.c_str(),
      (long)(*pos).mapflags,
      (int)(*pos).changed,
      (int)(*pos).deleted,
      (int)(*pos).added,
      (int)(*pos).markforresume,
      (int)(*pos).savedmark
    ));
    // check if item has changed since map table was read, or if its markforresume has changed
    // or if this is a successful end of a session, when we can safely assume that any pending maps
    // are from adds to the client that have never reached the client (otherwise, we'd have got
    // a map for it, even if the add was in a previous session or session attempt)
    if (
      (*pos).changed || (*pos).added || (*pos).deleted || // update of DB needed
      ((*pos).markforresume!=(*pos).savedmark) // mark for resume changed
    ) {
      // make sure it does not get written again if not really modified again
      (*pos).changed=false;
      // update new mapflags w/o changing mapflag_useforresume in the actual flags (as we still need it while session goes on)
      uInt32 newmapflags = (*pos).mapflags & ~mapflag_useforresume;
      if ((*pos).markforresume)
        newmapflags |= mapflag_useforresume;
      // remember last saved state
      (*pos).savedmark=(*pos).markforresume;
      // do something!
      MapID_Struct mapid;
      mapid.ident=(int)(*pos).entrytype;
      mapid.localID=(char *)((*pos).localid.c_str());
      mapid.remoteID=(char *)((*pos).remoteid.c_str());
      mapid.flags=newmapflags;
      if ((*pos).deleted) {
        if (!(*pos).added) {
          // delete this entry (only needed if it was not also added since last save - otherwise, map entry was never saved to the DB yet)
          sta=fDBApi_Admin.DeleteMapItem(&mapid);
          if (sta!=LOCERR_OK) break;
        }
        // now remove it from the list, such that we don't try to delete it again
        TMapContainer::iterator delpos=pos++; // that's the next to have a look at
        fMapTable.erase(delpos); // remove it now
        continue; // pos is already updated
      } // deleted
      else if ((*pos).added) {
        // add a new entry
        sta=fDBApi_Admin.InsertMapItem(&mapid);
        if (sta!=LOCERR_OK) break;
        // is now added, don't add again later
        (*pos).added=false;
      }
      else {
        // explicitly changed or needs update because of resume mark or pendingmap flag
        // change existing entry
        sta=fDBApi_Admin.UpdateMapItem(&mapid);
        if (sta!=LOCERR_OK) break;
      }
    } // if something changed
    // anyway - reset mark for resume, it must be reconstructed before next save
    (*pos).markforresume=false;
    // next
    pos++;
  } // while
  if (sta!=LOCERR_OK) return sta;
  // collect admin data in a string
  string adminData,s;
  adminData.erase();
  // add remote sync anchor
  adminData+="remotesyncanchor:";
  StrToCStrAppend(fLastRemoteAnchor.c_str(),adminData,true); // allow 8-bit chars to be represented as-is (no \xXX escape needed)
  /* not needed any more
  // add local anchor
  adminData+="\r\nlastlocalanchor:";
  timeStampToISO8601(fThisLocalAnchor,s,true,true);
  adminData+=s.c_str();
  */
  // add last sync time
  adminData+="\r\nlastsync:";
  TimestampToISO8601Str(s, fPreviousSyncTime, TCTX_UTC, false, false);
  adminData+=s.c_str();
  /* not needed any more
  // add local anchor of last sync with sending data to remote
  adminData+="\r\nlasttoremotelocalanchor:";
  timeStampToISO8601(fLastToRemoteLocalAnchor,s,true,true);
  adminData+=s.c_str();
  */
  // add last to remote sync time
  adminData+="\r\nlasttoremotesync:";
  TimestampToISO8601Str(s, fPreviousToRemoteSyncCmpRef, TCTX_UTC, false, false);
  adminData+=s.c_str();
  // add identifier needed by datastore to identify records changes since last to-remote-sync
  if (fPluginDSConfigP->fStoreSyncIdentifiers) {
    adminData+="\r\nlasttoremotesyncid:";
    StrToCStrAppend(fPreviousToRemoteSyncIdentifier.c_str(),adminData,true); // allow 8-bit chars to be represented as-is (no \xXX escape needed)
  }
  // add resume alert code
  adminData+="\r\nresumealertcode:"; StringObjAppendPrintf(adminData,"%hd",fResumeAlertCode);
  // add last suspend time
  adminData+="\r\nlastsuspend:";
  TimestampToISO8601Str(s, fPreviousSuspendCmpRef, TCTX_UTC, false, false);
  adminData+=s.c_str();
  // add identifier needed by datastore to identify records changes since last suspend
  if (fPluginDSConfigP->fStoreSyncIdentifiers) {
    adminData+="\r\nlastsuspendid:";
    StrToCStrAppend(fPreviousSuspendIdentifier.c_str(),adminData,true); // allow 8-bit chars to be represented as-is (no \xXX escape needed)
  }

  /// For datastores that can resume in middle of a chunked item (fConfigP->fResumeItemSupport==true):
  void*   blPtr = fPIStoredDataP; // position
  memSize blSize= fPIStoredSize;  // actualbytes

  if (dsResumeChunkedSupportedInDB()) {
    ///   - fPartialItemState = state of partial item (TPartialItemState enum):
    ///                         - if pi_state_none: save params, delete BLOB data (empty data)
    ///                         - if pi_state_save_incoming: save params+BLOB, save as in DB such that we will get pi_state_loaded_incoming when loaded again
    ///                         - if pi_state_save_outgoing: save params+BLOB, save as in DB such that we will get pi_state_loaded_outgoing when loaded again
    ///                         - if pi_state_loaded_incoming  or
    ///                              pi_state_loaded_outgoing: no need to save, as  params+BLOB have not changed since last save
    ///                                                        (but currently saved params+BLOB in DB must be retained)
    if (
      fPartialItemState!=pi_state_loaded_incoming &&
      fPartialItemState!=pi_state_loaded_outgoing
    ) {
      // and create the new status for these cases
      TPartialItemState pp= fPartialItemState;
      if (pp==pi_state_save_incoming) pp= pi_state_loaded_incoming; // adapt them before
      if (pp==pi_state_save_outgoing) pp= pi_state_loaded_outgoing;
      adminData+="\r\npartialitemstate:"; StringObjAppendPrintf( adminData,"%d",pp );
      // - fLastItemStatus   = status code (TSyError) of last item
      adminData+="\r\nlastitemstatus:"; StringObjAppendPrintf( adminData,"%hd",fLastItemStatus );
      // - fLastSourceURI    = item ID (string, if limited in length should be long enough for large IDs, >=64 chars recommended)
      adminData+="\r\nlastsourceURI:";  StrToCStrAppend( fLastSourceURI.c_str(), adminData,true );
      // - fLastTargetURI    = item ID (string, if limited in length should be long enough for large IDs, >=64 chars recommended)
      adminData+="\r\nlasttargetURI:"; StrToCStrAppend( fLastTargetURI.c_str(), adminData,true );
      // - fPITotalSize      = uInt32, total item size
      adminData+="\r\ntotalsize:"; StringObjAppendPrintf( adminData,"%ld", (long)fPITotalSize );
      // - fPIUnconfirmedSize= uInt32, unconfirmed part of item size
      adminData+="\r\nunconfirmedsize:"; StringObjAppendPrintf( adminData,"%ld", (long)fPIUnconfirmedSize );
      // - fPIStoredSize     = uInt32, size of BLOB to store, store it as well to make ReadBlob easier (mallloc)
      adminData+="\r\nstoredsize:"; StringObjAppendPrintf( adminData,"%ld", (long)blSize );
      // - fPIStoredSize     = uInt32, size of BLOB to store, 0=none
      // - fPIStoredDataP    = void *, BLOB data, NULL if none
      adminData+="\r\nstored;BLOBID="; adminData+= PIStored;
    } // if
  } // if
  // CRLF at end
  adminData+="\r\n";
  // save admin data
  sta= fDBApi_Admin.SaveAdminData(adminData.c_str()); if (sta) return sta;
  // now write all the BLOBs, currently there is only the PIStored object of ResumeChunkedSupport
  if (dsResumeChunkedSupportedInDB()) {
    TPartialItemState pis= fPartialItemState;
    if (
      blSize==0 &&
      (pis==pi_state_save_incoming ||  pis==pi_state_save_outgoing)
    )
      pis= pi_state_none; // delete blob, if size==0
    // handle BLOB
    switch (pis) {
      // make sure BLOB is deleted when it is empty
      case pi_state_none:
        sta =
          fDBApi_Admin.DeleteBlob(
            "",         // aItem.getLocalID()
            PIStored    // blobfieldname.c_str()
          );
        if (sta==DB_NotFound)
          sta= LOCERR_OK;    // no error, if not existing
        break;
      // save BLOB contents
      case pi_state_save_incoming: // Write the whole BLOB at once
      case pi_state_save_outgoing:
        sta =
          fDBApi_Admin.WriteBlob (
            "", // aItem.getLocalID()
            PIStored,   // blobfieldname.c_str()
            blPtr,      // bufferP
            blSize,     // actualbytes
            blSize,     // blobsize
            true,       // first
            true        // last
          );
          break;
      case pi_state_loaded_incoming:
      case pi_state_loaded_outgoing:
        // do nothing, as the blob is saved already
        break;
    } // switch
  } // if
  return sta;
} // TPluginApiDS::apiSaveAdminData


/// @brief Load admin data from Plugin-implemented database
///   Must search for existing target record matching the triple (aDeviceID,aDatabaseID,aRemoteDBID)
///   - if there is a matching record: load it
///   - if there is no matching record, set fFirstTimeSync=true. The implementation may already create a
///     new record with the key (aDeviceID,aDatabaseID,aRemoteDBID) and initialize it with the data from
///     the items as shown below. At least, fTargetKey must be set to a value that will allow apiSaveAdminData to
///     update the record. In case implementation chooses not create the record only in apiSaveAdminData, it must
///     buffer the triple (aDeviceID,aDatabaseID,aRemoteDBID) such that it is available at apiSaveAdminData.
///   If a record exists implementation must load the following items:
///   - fTargetKey        = some key value that can be used to re-identify the target record later at SaveAdminData.
///                         If the database implementation has other means to re-identify the target, this can be
///                         left unassigned.
///   - fLastRemoteAnchor = anchor string used by remote party for last session (and saved to DB then)
///   - fPreviousSyncTime = anchor (beginning of session) timestamp of last session.
///   - fPreviousToRemoteSyncCmpRef = Reference time to determine items modified since last time sending data to remote
///                         (or last changelog update in case of BASED_ON_BINFILE_CLIENT && binfileDSActive())
///   - fPreviousToRemoteSyncIdentifier = string identifying last session that sent data to remote
///                         (or last changelog update in case of BASED_ON_BINFILE_CLIENT && binfileDSActive()). Needs
///                         only be saved if derived datastore cannot work with timestamps and has its own identifier.
///   - fMapTable         = list<TMapEntry> containing map entries. The implementation must load all map entries
///                         related to the current sync target identified by the triple of (aDeviceID,aDatabaseID,aRemoteDBID)
///                         or by fTargetKey. The entries added to fMapTable must have "changed", "added" and "deleted" flags
///                         set to false.
///   For resumable datastores (fConfigP->fResumeSupport==true):
///   - fMapTable         = In addition to the above, the markforresume flag must be saved in the mapflags
//                          when it is not equal to the savedmark flag - independently of added/deleted/changed.
///   - fResumeAlertCode  = alert code of current suspend state, 0 if none
///   - fPreviousSuspendCmpRef = reference time of last suspend (used to detect items modified during a suspend / resume)
///   - fPreviousSuspendIdentifier = identifier of last suspend (used to detect items modified during a suspend / resume)
///                         (needs only be saved if derived datastore cannot work with timestamps and has
///                         its own identifier)
///   - fPendingAddMaps   = map<string,string>. The implementation must load all  all pending maps (client only) into
///                         fPendingAddMaps (and fUnconfirmedMaps must be left empty).
///   - fTempGUIDMap      = map<string,string>. The implementation must save all entries as temporary LUID to GUID mappings
///                         (server only)
///
///   For datastores that can resume in middle of a chunked item (fConfigP->fResumeItemSupport==true):
///   - fPartialItemState = state of partial item (TPartialItemState enum):
///                         - after load, value must always be pi_state_none, pi_state_loaded_incoming or pi_state_loaded_outgoing.
///                         - pi_state_save_xxx MUST NOT be set after loading (see apiSaveAdminData comments)
///   - fLastItemStatus   = status code (TSyError) of last item
///   - fLastSourceURI    = item ID (string, if limited in length should be long enough for large IDs, >=64 chars recommended)
///   - fLastTargetURI    = item ID (string, if limited in length should be long enough for large IDs, >=64 chars recommended)
///   - fPITotalSize      = uInt32, total item size
///   - fPIUnconfirmedSize= uInt32, unconfirmed part of item size
///   - fPIStoredSize     = uInt32, size of BLOB, 0=none
///   - fPIStoredDataP    = void *, BLOB data.
///                         - If this is not NULL on entry AND fPIStoredDataAllocated is set,
///                           the current block must be freed using smlLibFree() (and NOT JUST free()!!).
///                         - If no BLOB is loaded, this must be set to NULL
///                         - If a BLOB is loaded, an appropriate memory block should be allocated for it
///                           using smlLibMalloc() (and NOT JUST malloc()!!)
///   - fPIStoredDataAllocated: MUST BE SET to true when a memory block was allocated into fPIStoredDataP.
/// @param aDeviceID[in]       remote device URI (device ID)
/// @param aDatabaseID[in]     local database ID
/// @param aRemoteDBID[in]     database ID of remote device
localstatus TPluginApiDS::apiLoadAdminData(
  const char *aDeviceID,
  const char *aDatabaseID,
  const char *aRemoteDBID
)
{
  // security - don't use API when locked
  if (dbAccessLocked()) return 503; // service unavailable

  const char* PIStored = "PIStored"; // blob name field

  TSyError err = LOCERR_OK;

  // In any case - this is the time to create the contexts for the datastore
  // - admin if selected
  if (fPluginDSConfigP->fDBApiConfig_Admin.Connected()) {
    DB_Callback cb= &fDBApi_Admin.fCB.Callback;
    cb->callbackRef       = fSessionP; // the session
    #ifdef ENGINEINTERFACE_SUPPORT
    cb->thisBase          = fSessionP->getSyncAppBase()->fEngineInterfaceP;
    #endif
    #ifdef SYDEBUG
    // Datastore Admin debug goes to session log
    cb->debugFlags        = PDEBUGTEST(DBG_ADMIN+DBG_DBAPI+DBG_PLUGIN) ? 0xFFFF : 0;
    cb->DB_DebugPuts      = SessionLogDebugPuts;
    cb->DB_DebugBlock     = SessionLogDebugBlock;
    cb->DB_DebugEndBlock  = SessionLogDebugEndBlock;
    cb->DB_DebugEndThread = SessionLogDebugEndThread;
    cb->DB_DebugExotic    = SessionLogDebugExotic;
    #endif // SYDEBUG
    #ifdef ENGINEINTERFACE_SUPPORT
    // Admin module can use Get/SetValue for session script var access
    // Note: these are essentially context free and work without a global call-in structure
    //       (which is not necessarily there, for example in no-library case)
    CB_Connect_KeyAccess(cb); // connect generic key access routines
    // Version of OpenSessionKey that implicitly opens a key for the current session (DB plugins
    // do not have a session handle, as their use is always implicitly in a session context).
    cb->ui.OpenSessionKey = SessionOpenSessionKey;
    #endif // ENGINEINTERFACE_SUPPORT
    if (!fDBApi_Admin.Created()) {
      // - use datastore name as context name and link with session context
      err= fDBApi_Admin.CreateContext(
        getName(), true,
        &(fPluginDSConfigP->fDBApiConfig_Admin),
        fPluginAgentP->fDeviceKey.c_str(),
        fPluginAgentP->fUserKey.c_str(),
        fPluginAgentP->getDBApiSession()
      );
    }
    if (err!=LOCERR_OK)
      SYSYNC_THROW(TSyncException("Error creating context for plugin module handling admin",err));
  }
  // - data if selected
  err = connectDataPlugin();
  if (err==LOCERR_OK) {
    if (!fDBApi_Data.Created()) {
      // - use datastore name as context name and link with session context
      err= fDBApi_Data.CreateContext(
        getName(), false,
        &(fPluginDSConfigP->fDBApiConfig_Data),
        fPluginAgentP->fDeviceKey.c_str(),
        fPluginAgentP->fUserKey.c_str(),
        fPluginAgentP->getDBApiSession()
      );
    }
  }
  else if (err==LOCERR_NOTIMP)
    err=LOCERR_OK; // we just don't have a data plugin, that's ok, inherited (SQL) will handle data
  if (err!=LOCERR_OK)
    SYSYNC_THROW(TSyncException("Error creating context for plugin module handling data",err));
  // Perform actual loading of admin data
  #ifndef SDK_ONLY_SUPPORT
  // only handle here if we are in charge - otherwise let ancestor handle it
  if (!fDBApi_Admin.Created())
    return inherited::apiLoadAdminData(aDeviceID, aDatabaseID, aRemoteDBID);
  #endif
  // find and read (or create) the admin data
  TDB_Api_Str adminData;
  err=fDBApi_Admin.LoadAdminData(aDatabaseID,aRemoteDBID,adminData);
  if (err==404) {
    // this means that this admin data set did not exists before
    fFirstTimeSync=true;
  }
  else if (err!=LOCERR_OK)
    return err; // failed
  else
    fFirstTimeSync=false; // we already have admin data, so it can't be first sync
  // parse data
  const char *p = adminData.c_str();
  // second check: if empty adminData returned, this is treated as first sync as well
  if (*p==0) fFirstTimeSync=true;
  const char *q;
  string fieldname,value;
  lineartime_t *ltP;
  string *strP;
  uInt16 *usP;
  uInt32 *ulP;
  // read all fields
  while(*p) {
    // find name
    for (q=p; *q && (*q!=':' && *q!=';');) q++;
    fieldname.assign(p,q-p);
    p=q;
    // p should now point to ':' or ';'
    if (*p==':' || *p==';') {
      p++; // consume colon or semicolon
      // get value
      value.erase();
      p += CStrToStrAppend(p, value, true); // stop at quote or ctrl char
      // analyze and store now
      // - no storage location found yet
      ltP=NULL;
      strP=NULL;
      usP=NULL;
      ulP=NULL;

      // - find where we need to store this
      if (strucmp(fieldname.c_str(),"remotesyncanchor")==0) {
        strP=&fLastRemoteAnchor;
      }
      else if (strucmp(fieldname.c_str(),"lastsync")==0) {
        ltP=&fPreviousSyncTime;
      }
      else if (strucmp(fieldname.c_str(),"lasttoremotesync")==0) {
        ltP=&fPreviousToRemoteSyncCmpRef;
      }
      else if (strucmp(fieldname.c_str(),"lasttoremotesyncid")==0) {
        strP=&fPreviousToRemoteSyncIdentifier;
      }
      else if (strucmp(fieldname.c_str(),"resumealertcode")==0) {
        usP=&fResumeAlertCode;
      }
      else if (strucmp(fieldname.c_str(),"lastsuspend")==0) {
        ltP=&fPreviousSuspendCmpRef;
      }
      else if (strucmp(fieldname.c_str(),"lastsuspendid")==0) {
        strP=&fPreviousSuspendIdentifier;
      }

      /// For datastores that can resume in middle of a chunked item (fConfigP->fResumeItemSupport==true):
      else {
        if (dsResumeChunkedSupportedInDB()) {
          if (strucmp(fieldname.c_str(),"partialitemstate")==0) {
            usP = (TSyError*)&fPartialItemState;  // enum
          }
          else if (strucmp(fieldname.c_str(),"lastitemstatus")==0) {
            usP= &fLastItemStatus; // status code (TSyError) of last item
          }
          else if (strucmp(fieldname.c_str(),"lastsourceURI"   )==0) {
            strP= &fLastSourceURI; // item ID (string, if limited in len should be long enough for large IDs, >=64 chars recommended)
          }
          else if (strucmp(fieldname.c_str(),"lasttargetURI"   )==0) {
            strP= &fLastTargetURI; // item ID (string, if limited in len should be long enough for large IDs, >=64 chars recommended)
          }
          else if (strucmp(fieldname.c_str(),"totalsize"      )==0) {
            ulP= &fPITotalSize; // uInt32, total item size
          }
          else if (strucmp(fieldname.c_str(),"unconfirmedsize")==0) {
            ulP= &fPIUnconfirmedSize; // uInt32, unconfirmed part of item size
          }
          else if (strucmp(fieldname.c_str(),"storedsize")==0) {
            ulP= &fPIStoredSize; // uInt32, size of BLOB, 0=none
          }
          ///   - fPIStoredDataP    = void *, BLOB data.
          ///                         - If this is not NULL on entry AND fPIStoredDataAllocated is set,
          ///                           the current block must be freed using smlLibFree() (and NOT JUST free()!!).
          ///                         - If no BLOB is loaded, this must be set to NULL
          ///                         - If a BLOB is loaded, an appropriate memory block should be allocated for it
          ///                           using smlLibMalloc() (and NOT JUST malloc()!!)
          ///   - fPIStoredDataAllocated: MUST BE SET to true when a memory block was allocated into fPIStoredDataP.
          else if (strucmp(fieldname.c_str(),"stored")==0) {
            if (
              fPIStoredDataP!=NULL &&
              fPIStoredDataAllocated
            )
              smlLibFree(fPIStoredDataP);
            fPIStoredDataP= NULL;
            fPIStoredDataAllocated= false;

            TDB_Api_Blk b;
            memSize totSize;
            bool last;

            if (fPIStoredSize>0) {
              fPIStoredDataP= smlLibMalloc( fPIStoredSize ); // now prepare for the full blob
              fPIStoredDataAllocated= true;
              unsigned char* dp = (unsigned char*)fPIStoredDataP;
              unsigned char* lim = dp + fPIStoredSize;
              bool first= true;
              do {
                err= fDBApi_Admin.ReadBlob(
                  "",       // fParentObjectID.c_str(), // the item ID
                  PIStored, // fBlobID.c_str(), // the ID of the blob
                  0,        // neededBytes, // how much we need
                  b,        // blobData, // blob data
                  totSize,  // totalsize, // will receive total size or 0 if unknown
                  first,    // first,
                  last      // last
                );
                if (err)
                  break;

                memSize rema= b.fSize;
                if  (dp+rema > lim)
                  rema= lim-dp;    // avoid overflow
                memcpy( dp, b.fPtr, rema ); dp+= rema;
                fDBApi_Admin.DisposeBlk( b );         // we have now a copy => remove it
                first= false;
              } while (!last);
            } // if
          }
        } // if (dsResume ...)
      } // if

      // - store
      if (strP) {
        // - is a string
        (*strP) = value;
      }
      else if (usP) {
        // - is a uInt16
        StrToUShort(value.c_str(),*usP);
      }
      else if (ulP) {
        // - is a uInt32
        StrToULong(value.c_str(),*ulP);
      }
      else if (ltP) {
        // - is a ISO8601
        lineartime_t tim;
        timecontext_t tctx;
        if (ISO8601StrToTimestamp(value.c_str(), tim, tctx)!=0) {
          // converted ok, now make sure we get UTC
          TzConvertTimestamp(tim,tctx,TCTX_UTC,getSessionZones(),fPluginDSConfigP->fDataTimeZone);
          *ltP=tim;
        }
        else {
          // no valid date/time = empty
          *ltP=0;
        }
      }
    }
    // skip everything up to next end of line (in case value was terminated by a quote or other ctrl char)
    while (*p && *p!='\r' && *p!='\n') p++;
    // skip all line end chars up to beginning of next line or end of record
    while (*p && (*p=='\r' || *p=='\n')) p++;
    // p now points to next line's beginning
  };
  // then read maps
  bool firstEntry=true;
  fMapTable.clear();
  TDB_Api_MapID mapid;
  TMapEntry mapEntry;
  while (fDBApi_Admin.ReadNextMapItem(mapid, firstEntry)) {
    // get entry
    mapEntry.localid=mapid.localID.c_str();
    mapEntry.remoteid=mapid.remoteID.c_str();
    mapEntry.mapflags=mapid.flags;
    // check for old API which did not support entry types
    if (fPluginDSConfigP->fDBApiConfig_Admin.Version()<sInt32(VE_InsertMapItem)) {
      mapEntry.entrytype = mapentry_normal; // DB has no entry types, treat all as normal entries
    }
    else {
      if (mapid.ident>=numMapEntryTypes)
        mapEntry.entrytype = mapentry_invalid;
      else
        mapEntry.entrytype = (TMapEntryType)mapid.ident;
    }
    PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
      "read map entry (type=%s): localid='%s', remoteid='%s', mapflags=0x%lX",
      MapEntryTypeNames[mapEntry.entrytype],
      mapEntry.localid.c_str(),
      mapEntry.remoteid.c_str(),
      (long)mapEntry.mapflags
    ));
    // save entry in list
    mapEntry.changed=false; // not yet changed
    mapEntry.added=false; // already there
    // remember saved state of suspend mark
    mapEntry.markforresume=false; // not yet marked for this session (mark of last session is in mapflag_useforresume!)
    mapEntry.savedmark=mapEntry.mapflags & mapflag_useforresume;
    // IMPORTANT: non-normal entries must be saved as deleted in the main map - they will be re-activated at the
    // next save if needed
    mapEntry.deleted = mapEntry.entrytype!=mapentry_normal; // only normal ones may be saved as existing in the main map
    // save to main map list anyway to allow differential updates to map table (instead of writing everything all the time)
    fMapTable.push_back(mapEntry);
    // now save special maps to extra lists according to type
    // Note: in the main map, these are marked deleted. Before the next saveAdminData, these will
    //       be re-added (=re-activated) from the extra lists if they still exist.
    switch (mapEntry.entrytype) {
      #ifdef SYSYNC_SERVER
      case mapentry_tempidmap:
        if (IS_SERVER) {
          PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
            "fTempGUIDMap: restore mapping from %s to %s",
            mapEntry.remoteid.c_str(),
            mapEntry.localid.c_str()
          ));

          fTempGUIDMap[mapEntry.remoteid]=mapEntry.localid; // tempGUIDs are accessed by remoteID=tempID
        }
        break;
      #endif
      #ifdef SYSYNC_CLIENT
      case mapentry_pendingmap:
        if (IS_CLIENT)
          fPendingAddMaps[mapEntry.localid]=mapEntry.remoteid;
        break;
      #endif
    case mapentry_invalid:
    case mapentry_normal:
    case numMapEntryTypes:
      // nothing to do or should not occur
      break;
    }
    // next is not first entry any more
    firstEntry=false;
  }
  return LOCERR_OK;
} // TPluginApiDS::apiLoadAdminData


#endif // not BINFILE_ALWAYS_ACTIVE


/// @brief log datastore sync result, called at end of sync with this datastore
/// Available log information is:
/// - fCurrentSyncTime                  : timestamp of start this sync session (anchor time)
/// - fCurrentSyncCmpRef                : timestamp used by next session to detect changes since this one
///                                       (end of session time of last session that send data to remote)
/// - fTargetKey                        : string, identifies target (user/device/datastore/remotedatastore)-tuple
/// - fSessionP->fUserKey               : string, identifies user
/// - fSessionP->fDeviceKey             : string, identifies device
/// - fSessionP->fDomainName            : string, identifies domain (aka clientID, aka enterpriseID)
/// - getName()                         : string, local datastore's configured name (such as "contacts", "events" etc.)
/// - getRemoteDBPath()                 : string, remote datastore's path (things like EriCalDB or "Z:\System\Contacts.cdb")
/// - getRemoteViewOfLocalURI()         : string, shows how remote specifies local datastore URI (including cgi, or
///                                       in case of symbian "calendar", this can be different from getName(), as "calendar"
///                                       is internally split-processed by "events" and "tasks".
/// - fSessionP->getRemoteURI()         : string, remote Device URI (usually IMEI or other globally unique ID)
/// - fSessionP->getRemoteDescName()    : string, remote Device's descriptive name (constructed from DevInf <man> and <mod>, or
///                                       as set by <remoterule>'s <descriptivename>.
/// - fSessionP->getRemoteInfoString()  : string, Remote Device Version Info ("Type (HWV, FWV, SWV) Oem")
/// - fSessionP->getSyncUserName()      : string, User name as sent by the device
/// - fSessionP->getLocalSessionID()    : string, the local session ID (the one that is used to construct log file names)
/// - fAbortStatusCode                  : TSyError, if == 0, sync with this datastore was ok, if <>0, there was an error.
/// - fSessionP->getAbortReasonStatus() : TSyError, shows status of entire session at the point when datastore finishes syncing.
/// - fSyncMode                         : TSyncModes, (smo_twoway,smo_fromserver,smo_fromclient)
/// - isSlowSync()                      : boolean, true if slow sync (or refresh from sync, which is a one-way-slow-sync)
/// - isFirstTimeSync()                 : boolean, true if first time sync of this device with this datastore
/// - isResuming()                      : boolean, true if this is a resumed session
/// - SyncMLVerDTDNames[fSessionP->getSyncMLVersion()] : SyncML version string ("1.0", "1.1". "1.2" ...)
/// - fLocalItemsAdded                  : number of locally added Items
/// - fRemoteItemsAdded                 : number of remotely added Items
/// - fLocalItemsDeleted                : number of locally deleted Items
/// - fRemoteItemsDeleted               : number of remotely deleted Items
/// - fLocalItemsError                  : number of locally rejected Items (caused error to be sent to remote)
/// - fRemoteItemsError                 : number of remotely rejected Items (returned error, will be resent)
/// - fLocalItemsUpdated                : number of locally updated Items
/// - fRemoteItemsUpdated               : number of remotely updated Items
/// - fSlowSyncMatches                  : number of items matched in Slow Sync
/// - fConflictsServerWins              : number of server won conflicts
/// - fConflictsClientWins              : number of client won conflicts
/// - fConflictsDuplicated              : number of conflicts solved by duplicating item
/// - fSessionP->getIncomingBytes()     : total number of incoming bytes in this session so far
/// - fSessionP->getOutgoingBytes()     : total number of outgoing bytes in this session so far
/// - fIncomingDataBytes                : net incoming data bytes for this datastore (= payload data, without SyncML protocol overhead)
/// - fOutgoingDataBytes                : net outgoing data bytes for this datastore (= payload data, without SyncML protocol overhead)
void TPluginApiDS::dsLogSyncResult(void)
{
  // security - don't use API when locked
  if (dbAccessLocked()) return;

  // format for DB Api
  string logData,s;
  logData.erase();
  logData+="lastsync:"; TimestampToISO8601Str(s,fCurrentSyncTime,TCTX_UTC); logData+=s.c_str();
  logData+="\r\ntargetkey:"; StrToCStrAppend(fTargetKey.c_str(),logData,true);
  #ifndef BINFILE_ALWAYS_ACTIVE
  logData+="\r\nuserkey:"; StrToCStrAppend(fPluginAgentP->fUserKey.c_str(),logData,true);
  logData+="\r\ndevicekey:"; StrToCStrAppend(fPluginAgentP->fDeviceKey.c_str(),logData,true);
  #ifdef SCRIPT_SUPPORT
  logData+="\r\ndomain:"; StrToCStrAppend(fPluginAgentP->fDomainName.c_str(),logData,true);
  #endif
  #endif // BINFILE_ALWAYS_ACTIVE
  logData+="\r\ndsname:"; StrToCStrAppend(getName(),logData,true);
  #ifndef MINIMAL_CODE
  logData+="\r\ndsremotepath:"; StrToCStrAppend(getRemoteDBPath(),logData,true);
  #endif
  logData+="\r\ndslocalpath:"; StrToCStrAppend(getRemoteViewOfLocalURI(),logData,true);
  logData+="\r\nfolderkey:"; StrToCStrAppend(fFolderKey.c_str(),logData,true);
  logData+="\r\nremoteuri:"; StrToCStrAppend(fSessionP->getRemoteURI(),logData,true);
  logData+="\r\nremotedesc:"; StrToCStrAppend(fSessionP->getRemoteDescName(),logData,true);
  logData+="\r\nremoteinfo:"; StrToCStrAppend(fSessionP->getRemoteInfoString(),logData,true);
  logData+="\r\nremoteuser:"; StrToCStrAppend(fSessionP->getSyncUserName(),logData,true);
  logData+="\r\nsessionid:"; StrToCStrAppend(fSessionP->getLocalSessionID(),logData,true);
  logData+="\r\nsyncstatus:"; StringObjAppendPrintf(logData,"%hd",fAbortStatusCode);
  logData+="\r\nsessionstatus:"; StringObjAppendPrintf(logData,"%hd",fSessionP->getAbortReasonStatus());
  logData+="\r\nsyncmlvers:"; StrToCStrAppend(SyncMLVerDTDNames[fSessionP->getSyncMLVersion()],logData,true);
  logData+="\r\nsyncmode:"; StringObjAppendPrintf(logData,"%hd",(uInt16)fSyncMode);
  logData+="\r\nsynctype:"; StringObjAppendPrintf(logData,"%d",(fSlowSync ? (fFirstTimeSync ? 2 : 1) : 0) + (isResuming() ? 10 : 0));
  logData+="\r\nlocaladded:"; StringObjAppendPrintf(logData,"%ld",(long)fLocalItemsAdded);
  logData+="\r\ndeviceadded:"; StringObjAppendPrintf(logData,"%ld",(long)fRemoteItemsAdded);
  logData+="\r\nlocaldeleted:"; StringObjAppendPrintf(logData,"%ld",(long)fLocalItemsDeleted);
  logData+="\r\ndevicedeleted:"; StringObjAppendPrintf(logData,"%ld",(long)fRemoteItemsDeleted);
  logData+="\r\nlocalrejected:"; StringObjAppendPrintf(logData,"%ld",(long)fLocalItemsError);
  logData+="\r\ndevicerejected:"; StringObjAppendPrintf(logData,"%ld",(long)fRemoteItemsError);
  logData+="\r\nlocalupdated:"; StringObjAppendPrintf(logData,"%ld",(long)fLocalItemsUpdated);
  logData+="\r\ndeviceupdated:"; StringObjAppendPrintf(logData,"%ld",(long)fRemoteItemsUpdated);
  #ifdef SYSYNC_SERVER
  if (IS_SERVER) {
    logData+="\r\nslowsyncmatches:"; StringObjAppendPrintf(logData,"%ld",(long)fSlowSyncMatches);
    logData+="\r\nserverwins:"; StringObjAppendPrintf(logData,"%ld",(long)fConflictsServerWins);
    logData+="\r\nclientwins:"; StringObjAppendPrintf(logData,"%ld",(long)fConflictsClientWins);
    logData+="\r\nduplicated:"; StringObjAppendPrintf(logData,"%ld",(long)fConflictsDuplicated);
  } // server
  #endif // SYSYNC_SERVER
  logData+="\r\nsessionbytesin:"; StringObjAppendPrintf(logData,"%ld",(long)fSessionP->getIncomingBytes());
  logData+="\r\nsessionbytesout:"; StringObjAppendPrintf(logData,"%ld",(long)fSessionP->getOutgoingBytes());
  logData+="\r\ndatabytesin:"; StringObjAppendPrintf(logData,"%ld",(long)fIncomingDataBytes);
  logData+="\r\ndatabytesout:"; StringObjAppendPrintf(logData,"%ld",(long)fOutgoingDataBytes);
  logData+="\r\n";
  if (fDBApi_Admin.Created()) {
    fDBApi_Admin.WriteLogData(logData.c_str());
    if (fPluginDSConfigP->fDBAPIModule_Data != fPluginDSConfigP->fDBAPIModule_Admin) {
      // admin and data are different modules, show log to data module as well
      if (fDBApi_Data.Created())
        fDBApi_Data.WriteLogData(logData.c_str());
    }
  }
  else if (fDBApi_Data.Created()) {
    fDBApi_Data.WriteLogData(logData.c_str());
  }
  // anyway: let ancestor save log info as well (if it is configured so)
  inherited::dsLogSyncResult();
} // TPluginApiDS::dsLogSyncResult



#ifdef STREAMFIELD_SUPPORT

// TApiBlobProxy
// =============

TApiBlobProxy::TApiBlobProxy(
  TPluginApiDS *aApiDsP,
  bool aIsStringBLOB,
  const char *aBlobID,
  const char *aParentID
)
{
  // save values
  fApiDsP = aApiDsP;
  fIsStringBLOB = aIsStringBLOB;
  fBlobID = aBlobID;
  fParentObjectID = aParentID;
  fBlobSize = 0;
  fBlobSizeKnown = false;
  fFetchedSize = 0;
  fBufferSize = 0;
  fBlobBuffer = NULL; // nothing retrieved yet
} // TApiBlobProxy::TApiBlobProxy


TApiBlobProxy::~TApiBlobProxy()
{
  if (fBlobBuffer) delete (char *)fBlobBuffer; // gcc 3.2.2 needs cast to suppress warning
  fBlobBuffer=NULL;
} // TApiBlobProxy::~TApiBlobProxy


// fetch BLOB from DPAPI
void TApiBlobProxy::fetchBlob(size_t aNeededSize, bool aNeedsTotalSize, bool aNeedsAllData)
{
  TSyError dberr=LOCERR_OK;

  if (fBufferSize==0 || aNeededSize>fFetchedSize) {
    // if do not have anything yet or not enough yet, we need to read
    uInt8P bufP = NULL;
    bool last = false;
    bool first = fFetchedSize==0; // first if we haven't fetched anything so far
    TDB_Api_Blk blobData;
    memSize neededBytes = aNeededSize-fFetchedSize; // how much we need to read more
    memSize totalsize = 0; // not known

    if (fIsStringBLOB)
      aNeedsAllData = true; // strings must be fetched entirely, as they need to be converted before we can measure size or get data
    if (!fBlobBuffer && aNeededSize==0 && (aNeedsTotalSize || aNeedsAllData))
      neededBytes=200; // just read a bit to possibly obtain the total size
    do {
      // read a block
      dberr = fApiDsP->fDBApi_Data.ReadBlob(
        fParentObjectID.c_str(), // the item ID
        fBlobID.c_str(), // the ID of the blob
        neededBytes, // how much we need
        blobData, // blob data
        totalsize, // will receive total size or 0 if unknown
        first,
        last
      );
      if (dberr!=LOCERR_OK)
        SYSYNC_THROW(TSyncException("ReadBlob fatal error",dberr));
      // sanity check
      if (blobData.fSize>neededBytes)
        SYSYNC_THROW(TSyncException("ReadBlob returned more data than requested"));
      // check if we know the total size reliably now
      if (totalsize) {
        // non-zero return means we know the total size now
        fBlobSize = totalsize;
        fBlobSizeKnown = true;
      }
      else {
        // could be unknown size OR zero blob
        if (neededBytes>0 && blobData.fSize==0) {
          // we tried to read, but got nothing, and total size is zero -> this means explicit zero size
          fBlobSize = 0;
          fBlobSizeKnown = true;
        }
      }
      // calculate how large the buffer needs to be
      size_t newBufSiz = (aNeededSize ? aNeededSize : fFetchedSize+blobData.fSize) + 1; // +1 for string terminator possibly needed
      if (fBufferSize<newBufSiz) {
        // we need a larger buffer
        bufP = new uInt8[newBufSiz];
        fBufferSize=newBufSiz; // save new buffer size
        // if there's something in the old buffer, we need to copy it and delete it
        if (fBlobBuffer) {
          if (fFetchedSize)
            memcpy(bufP,fBlobBuffer,fFetchedSize); // copy fetched portion from old buffer
          delete (uInt8P)fBlobBuffer; // dispose old buffer
        } // if
        fBlobBuffer = bufP; // save new one, in EVERY CASE
      }
      // get pointer where to copy data to
      bufP = fBlobBuffer+fFetchedSize; // append to what is already in the buffer
      // actually copy data from DBApi block to buffer
      memcpy(bufP,blobData.fPtr,blobData.fSize);
      // calculate how much we want to read next time
      // if neededBytes now gets zero, this will request as much as possible for the next call to ReadBlob
      fFetchedSize += blobData.fSize;
      neededBytes -= blobData.fSize; // see what's remaining until what we originally requested
      blobData.DisposeBlk();         // <blobData.fSize> will be set back to 0 here !!
      // check end of data from API
      if (last) {
        if (!fBlobSizeKnown) {
          fBlobSize = fFetchedSize;
          fBlobSizeKnown = true;
        }
        // end of BLOB: done fetching ANYWAY
        break;
      }
      // the BLOB is bigger than what we have fetched so far
      // - check if we are done even if not at end of blob
      if (!aNeedsAllData && fFetchedSize>=aNeededSize && (!aNeedsTotalSize || fBlobSizeKnown))
        break; // we have what was requested
      // - we need to load more data
      if (aNeedsAllData) {
        if (fBlobSizeKnown)
          neededBytes = fBlobSize-fFetchedSize; // try to get rest in one chunk
        else
          neededBytes = 4096; // we don't know how much is coming, continue reading in 4k chunks
      }
      // we need to continue until we get the total size or last
      first=false;
    } while(true);

    // for strings, we need to convert the data and re-adjust the size
    if (fIsStringBLOB) {
      // we KNOW that we have the entire BLOB text here (because we set aNeedsAllData above when this is a string BLOB)
      // - set a terminator
      *((char *)fBlobBuffer+fFetchedSize) = 0; // set terminator
      // - convert to UTF8 and internal linefeeds
      string strUtf8;
      appendStringAsUTF8((const char *)fBlobBuffer, strUtf8, fApiDsP->fPluginDSConfigP->fDataCharSet, lem_cstr);
      // set actual size
      fBlobSize=strUtf8.size();
      // copy from string to buffer
      if (fBlobSize+1<=fBufferSize) {
        bufP = fBlobBuffer; // use old buffer
      }
      else {
        fBufferSize=fBlobSize+1;
        bufP = new unsigned char [fBufferSize];
        delete (unsigned char *)fBlobBuffer;
        fBlobBuffer = bufP;
      }
      memcpy(bufP,strUtf8.c_str(),strUtf8.size());
      fFetchedSize=strUtf8.size();
      *((char *)fBlobBuffer+fFetchedSize)=0; // set terminator
    } // if
  }
} // TApiBlobProxy::fetchBlob




// returns size of entire blob
size_t TApiBlobProxy::getBlobSize(TStringField *aFieldP)
{
  fetchBlob(0,true,false); // only needs the size, but no data
  return fBlobSize;
} // TApiBlobProxy::getBlobSize


// read from Blob from specified stream position and update stream pos
size_t TApiBlobProxy::readBlobStream(TStringField *aFieldP, size_t &aPos, void *aBuffer, size_t aMaxBytes)
{
  if (fFetchedSize<aPos+aMaxBytes || !fBlobBuffer) {
    // we need to read (more of) the body
    if (!fBlobSizeKnown || fFetchedSize<fBlobSize) {
      // we know that we need to fetch more, or we are not sure that we have fetched everything already -> fetch more
      fetchBlob(aPos+aMaxBytes,false,false); // fetch at least up to the given size (unless blob is actually smaller)
    }
  }
  // now copy from our buffer
  if (aPos>fFetchedSize) return 0; // position obviously out of range
  if (aPos+aMaxBytes>fFetchedSize) aMaxBytes=fFetchedSize-aPos; // reduce to what we have
  if (aMaxBytes==0) return 0; // safety
  // copy data from fBlobBuffer (which contains beginning or all of the BLOB) to caller's buffer
  memcpy(aBuffer,(char *)fBlobBuffer+aPos,aMaxBytes);
  aPos += aMaxBytes;
  return aMaxBytes; // return number of bytes actually read
} // TApiBlobProxy::readBlobStream


#endif // STREAMFIELD_SUPPORT



} // namespace sysync

/* end of TPluginApiDS implementation */

// eof
