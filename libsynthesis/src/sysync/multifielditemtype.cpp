/*
 *  File:         MultiFieldItemType.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TMultiFieldItemType
 *    Type consisting of multiple data fields (TItemField objects)
 *    To be used as base class for field formats like vCard,
 *    vCalendar etc.
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-08-13 : luz : created
 *
 */

// includes
#include "prefix_file.h"
#include "sysync.h"
#include "multifielditemtype.h"


namespace sysync {

// multifield-based datatype config

TMultiFieldTypeConfig::TMultiFieldTypeConfig(const char* aName, TConfigElement *aParentElement) :
  TDataTypeConfig(aName,aParentElement)
{
  clear();
} // TMultiFieldTypeConfig::TMultiFieldTypeConfig


TMultiFieldTypeConfig::~TMultiFieldTypeConfig()
{
  clear();
} // TMultiFieldTypeConfig::~TMultiFieldTypeConfig


// init defaults
void TMultiFieldTypeConfig::clear(void)
{
  // clear properties
  // - remove link to field list
  fFieldListP=NULL;
  // - remove link to field list
  fProfileConfigP=NULL;
  #ifdef SCRIPT_SUPPORT
  fInitScript.erase();
  fIncomingScript.erase();
  fOutgoingScript.erase();
  fFilterInitScript.erase();
  fPostFetchFilterScript.erase();
  #ifdef SYSYNC_SERVER
  fCompareScript.erase();
  fMergeScript.erase();
  #endif
  fProcessItemScript.erase();
  #endif
  // clear properties
  fProfileMode=PROFILEMODE_DEFAULT; // default profile mode
  // clear inherited
  inherited::clear();
} // TMultiFieldTypeConfig::clear


#ifdef SCRIPT_SUPPORT

class TMFTypeFuncs {
public:

  // integer SYNCMODESUPPORTED(string mode)
  //
  // True if the remote datastore is known to support the specific mode
  // (usually because it sent SyncCap including the mode), false if unknown
  // or not supported.
  //
  // Only works inside scripts which work on remote items.
  static void func_SyncModeSupported(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    bool supported = false;
    TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext());
    if (mfitP->fFirstItemP &&
        mfitP->fFirstItemP->getRemoteItemType()) {
      TSyncDataStore *related = mfitP->fFirstItemP->getRemoteItemType()->getRelatedDatastore();
      if (related) {
        std::string mode;
        aFuncContextP->getLocalVar(0)->getAsString(mode);
        supported = related->syncModeSupported(mode);
      }
    }
    aTermP->setAsInteger(supported);
  }; // func_SyncModeSupported

  // void SETFILTERALL(integer all)
  // sets if all records in the syncset need to checked against filter or only those that are new or changed
  static void func_SetFilterAll(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext())->fNeedToFilterAll =
      aFuncContextP->getLocalVar(0)->getAsBoolean();
  }; // func_SetFilterAll


  #ifdef SYSYNC_TARGET_OPTIONS

  // integer SIZELIMIT()
  // gets the size limit set for this item
  static void func_Limit(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    fieldinteger_t i = static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext())->fDsP->fItemSizeLimit;
    if (i<0)
      aTermP->unAssign();
    else
      aTermP->setAsInteger(i);
  }; // func_Limit


  // SETSIZELIMIT(integer limit)
  // sets size limit used for generating this item for remote
  static void func_SetLimit(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext())->fDsP->fItemSizeLimit =
      aFuncContextP->getLocalVar(0)->isUnassigned() ? -1 : aFuncContextP->getLocalVar(0)->getAsInteger();
  }; // func_SetLimit

  #endif



  #ifdef SYSYNC_SERVER

  // void ECHOITEM(string syncop)
  // creates a duplicate of the processed item to be sent back to sender with the specified syncop
  static void func_EchoItem(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // convert to syncop
    string s;
    aFuncContextP->getLocalVar(0)->getAsString(s);
    sInt16 sop;
    StrToEnum(SyncOpNames, numSyncOperations, sop, s.c_str());
    static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext())->fDsP->fEchoItemOp =
      (TSyncOperation) sop;
  }; // func_EchoItem


  // void CONFLICTSTRATEGY(string strategy)
  // sets the conflict strategy for this item
  static void func_ConflictStrategy(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // convert to conflictstrategy
    string s;
    aFuncContextP->getLocalVar(0)->getAsString(s);
    sInt16 strategy;
    StrToEnum(conflictStrategyNames,numConflictStrategies, strategy, s.c_str());
    static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext())->fDsP->fItemConflictStrategy =
      (TConflictResolution) strategy;
  }; // func_ConflictStrategy


  // void FORCECONFLICT()
  // forces conflict between this item and item from the DB
  static void func_ForceConflict(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext())->fDsP->fForceConflict=true;
  }; // func_ForceConflict


  // void DELETEWINS()
  // in a replace/delete conflict, delete wins (normally, replace wins)
  static void func_DeleteWins(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext())->fDsP->fDeleteWins=true;
  }; // func_DeleteWins


  // void PREVENTADD()
  // if set, attempt to add item from remote will cause no add but delete of remote item
  static void func_PreventAdd(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext())->fDsP->fPreventAdd=true;
  }; // func_PreventAdd


  // void IGNOREUPDATE()
  // if set, attempt to update existing items from remote will be ignored
  static void func_IgnoreUpdate(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext())->fDsP->fIgnoreUpdate=true;
  }; // func_IgnoreUpdate


  // void MERGEFIELDS()
  static void func_MergeFields(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext());
    if (mfitP->fFirstItemP)
      mfitP->fFirstItemP->standardMergeWith(*(mfitP->fSecondItemP),mfitP->fChangedFirst,mfitP->fChangedSecond);
  }; // func_MergeFields


  // integer WINNINGCHANGED()
  // returns true if winning was changed
  static void func_WinningChanged(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext());
    aTermP->setAsInteger(mfitP->fChangedFirst ? 1 : 0);
  }; // func_WinningChanged

  // integer LOOSINGCHANGED()
  // returns true if loosing was changed
  static void func_LoosingChanged(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext());
    aTermP->setAsInteger(mfitP->fChangedSecond ? 1 : 0);
  }; // func_LoosingChanged


  static void func_SetWinningChanged(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext());
    mfitP->fChangedFirst =
      aFuncContextP->getLocalVar(0)->getAsBoolean();
  }; // func_SetWinningChanged

  static void func_SetLoosingChanged(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext());
    mfitP->fChangedSecond =
      aFuncContextP->getLocalVar(0)->getAsBoolean();
  }; // func_SetLoosingChanged


  // integer COMPAREFIELDS()
  // returns 0 if equal, 1 if first > second, -1 if first < second
  static void func_CompareFields(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext());
    if (!mfitP->fFirstItemP) aTermP->setAsInteger(SYSYNC_NOT_COMPARABLE);
    else {
      aTermP->setAsInteger(
        mfitP->fFirstItemP->standardCompareWith(*(mfitP->fSecondItemP),mfitP->fEqMode,OBJDEBUGTEST(mfitP,DBG_SCRIPTS+DBG_DATA+DBG_MATCH))
      );
    }
  }; // func_CompareFields


  // string COMPARISONMODE()
  // returns mode of comparison
  static void func_ComparisonMode(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext());
    if (!mfitP->fFirstItemP) aTermP->unAssign(); // no comparison
    else {
      aTermP->setAsString(comparisonModeNames[mfitP->fEqMode]);
    }
  }; // func_CompareFields



  #endif


  // string SYNCOP()
  // returns sync-operation as text
  static void func_SyncOp(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsString(
      SyncOpNames[static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext())->fCurrentSyncOp]
    );
  }; // func_SyncOp


  // void REJECTITEM(integer statuscode)
  // causes current item not to be processed, but rejected with status code (0=silently rejected)
  static void func_RejectItem(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext())->fDsP->fRejectStatus=
      aFuncContextP->getLocalVar(0)->getAsInteger();
  }; // func_RejectItem


  // string REMOTEID()
  // returns target item's remoteID as text
  static void func_RemoteID(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext());
    if (mfitP->fFirstItemP) {
      aTermP->setAsString(mfitP->fFirstItemP->getRemoteID());
    }
  }; // func_RemoteID


  // void SETREMOTEID(string remoteid)
  // sets the target item's remote ID as text
  static void func_SetRemoteID(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext());
    if (mfitP->fFirstItemP) {
      string s;
      aFuncContextP->getLocalVar(0)->getAsString(s);
      mfitP->fFirstItemP->setRemoteID(s.c_str());
    }
  }; // func_SetRemoteID


  // string LOCALID()
  // returns target item's localID as text
  static void func_LocalID(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext());
    if (mfitP->fFirstItemP) {
      aTermP->setAsString(mfitP->fFirstItemP->getLocalID());
    }
  }; // func_LocalID


  // void SETLOCALID(string remoteid)
  // sets the target item's localID as text
  static void func_SetLocalID(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(aFuncContextP->getCallerContext());
    if (mfitP->fFirstItemP) {
      string s;
      aFuncContextP->getLocalVar(0)->getAsString(s);
      mfitP->fFirstItemP->setLocalID(s.c_str());
    }
  }; // func_SetLocalID

}; // TMFTypeFuncs


// chain function to link datastore-level functions
static void* DataTypeChainFunc(void *&aNextCallerContext)
{
  // caller context for datastore-level functions is the datastore pointer
  if (aNextCallerContext)
    aNextCallerContext = static_cast<TMultiFieldItemType *>(aNextCallerContext)->fDsP;
  // next table is that of the localdatastore
  return (void *) &DBFuncTable;
} // DataTypeChainFunc

const uInt8 param_StrArg[] = { VAL(fty_string) };
const uInt8 param_IntArg[] = { VAL(fty_integer) };

// builtin functions for datastore-context table
const TBuiltInFuncDef DataTypeFuncDefs[] = {
  { "SYNCMODESUPPORTED", TMFTypeFuncs::func_SyncModeSupported, fty_integer, 1, param_StrArg },
  { "SETFILTERALL", TMFTypeFuncs::func_SetFilterAll, fty_none, 1, param_IntArg },
  #ifdef SYSYNC_TARGET_OPTIONS
  { "SIZELIMIT", TMFTypeFuncs::func_Limit, fty_integer, 0, NULL },
  { "SETSIZELIMIT", TMFTypeFuncs::func_SetLimit, fty_none, 1, param_IntArg },
  #endif
  #ifdef SYSYNC_SERVER
  { "ECHOITEM", TMFTypeFuncs::func_EchoItem, fty_none, 1, param_StrArg },
  { "CONFLICTSTRATEGY", TMFTypeFuncs::func_ConflictStrategy, fty_none, 1, param_StrArg },
  { "FORCECONFLICT", TMFTypeFuncs::func_ForceConflict, fty_none, 0, NULL },
  { "DELETEWINS", TMFTypeFuncs::func_DeleteWins, fty_none, 0, NULL },
  { "PREVENTADD", TMFTypeFuncs::func_PreventAdd, fty_none, 0, NULL },
  { "IGNOREUPDATE", TMFTypeFuncs::func_IgnoreUpdate, fty_none, 0, NULL },
  { "MERGEFIELDS", TMFTypeFuncs::func_MergeFields, fty_none, 0, NULL },
  { "WINNINGCHANGED", TMFTypeFuncs::func_WinningChanged, fty_integer, 0, NULL },
  { "LOOSINGCHANGED", TMFTypeFuncs::func_LoosingChanged, fty_integer, 0, NULL },
  { "SETWINNINGCHANGED", TMFTypeFuncs::func_SetWinningChanged, fty_none, 1, param_IntArg },
  { "SETLOOSINGCHANGED", TMFTypeFuncs::func_SetLoosingChanged, fty_none, 1, param_IntArg },
  { "COMPAREFIELDS", TMFTypeFuncs::func_CompareFields, fty_integer, 0, NULL },
  { "COMPARISONMODE", TMFTypeFuncs::func_ComparisonMode, fty_string, 0, NULL },
  #endif
  { "SYNCOP", TMFTypeFuncs::func_SyncOp, fty_string, 0, NULL },
  { "REJECTITEM", TMFTypeFuncs::func_RejectItem, fty_none, 1, param_IntArg },
  { "LOCALID", TMFTypeFuncs::func_LocalID, fty_string, 0, NULL },
  { "SETLOCALID", TMFTypeFuncs::func_SetLocalID, fty_none, 1, param_StrArg },
  { "REMOTEID", TMFTypeFuncs::func_RemoteID, fty_string, 0, NULL },
  { "SETREMOTEID", TMFTypeFuncs::func_SetRemoteID, fty_none, 1, param_StrArg },
};

const TFuncTable DataTypeFuncTable = {
  sizeof(DataTypeFuncDefs) / sizeof(TBuiltInFuncDef), // size of table
  DataTypeFuncDefs, // table pointer
  DataTypeChainFunc // chain to localdatastore funcs
};


#endif


#ifdef CONFIGURABLE_TYPE_SUPPORT

// config element parsing
bool TMultiFieldTypeConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  // Note: derived classes might override this "use" further
  //       Here, we check for using only a file list, or a profile which implies a field list
  if (strucmp(aElementName,"use")==0) {
    if (fFieldListP || fProfileConfigP)
      return fail("'use' cannot be specified more than once");
    // - get type registry
    TMultiFieldDatatypesConfig *mufcP;
    GET_CASTED_PTR(mufcP,TMultiFieldDatatypesConfig,getParentElement(),DEBUGTEXT("TMultiFieldTypeConfig with non-TMultiFieldDatatypesConfig parent","txit2"));
    // - check what to look for
    const char *pnam = getAttr(aAttributes,"fieldlist");
    if (pnam) {
      // we are addressing a fieldlist
      fFieldListP=mufcP->getFieldList(pnam);
      if (!fFieldListP)
        return fail("unknown field list '%s' specified in 'use'",pnam);
    }
    else {
      // this must be a profile (we may also call it 'mimeprofile' for historical, <=2.1 engine reasons)
      pnam = getAttr(aAttributes,"profile");
      if (!pnam)
        pnam = getAttr(aAttributes,"mimeprofile"); // %%% for compatibility with <=2.1 engine configs
      if (!pnam)
        return fail("'use' must have a 'fieldlist' or 'profile' attribute");
      fProfileConfigP=mufcP->getProfile(pnam);
      if (!fProfileConfigP)
        return fail("unknown profile '%s' specified in 'use'",pnam);
      // - copy field list pointer into TMultiFieldTypeConfig as well
      fFieldListP = fProfileConfigP->fFieldListP;
    }
    expectEmpty();
  }
  else if (strucmp(aElementName,"profilemode")==0)
    expectInt32(fProfileMode); // usually, this is set implicitly by derived types, such as vCard, vCalendar
  #ifdef SCRIPT_SUPPORT
  else if (strucmp(aElementName,"initscript")==0)
    expectScript(fInitScript,aLine,&DataTypeFuncTable);
  else if (strucmp(aElementName,"incomingscript")==0)
    expectScript(fIncomingScript,aLine,&DataTypeFuncTable);
  else if (strucmp(aElementName,"outgoingscript")==0)
    expectScript(fOutgoingScript,aLine,&DataTypeFuncTable);
  else if (strucmp(aElementName,"filterinitscript")==0)
    expectScript(fFilterInitScript,aLine,&DataTypeFuncTable);
  else if (strucmp(aElementName,"filterscript")==0)
    expectScript(fPostFetchFilterScript,aLine,&DataTypeFuncTable);
  #ifdef SYSYNC_SERVER
  else if (strucmp(aElementName,"comparescript")==0)
    expectScript(fCompareScript,aLine,&DataTypeFuncTable);
  else if (strucmp(aElementName,"mergescript")==0)
    expectScript(fMergeScript,aLine,&DataTypeFuncTable);
  #endif
  else if (strucmp(aElementName,"processitemscript")==0)
    expectScript(fProcessItemScript,aLine,&DataTypeFuncTable);
  #endif
  // - none known here
  else
    return TDataTypeConfig::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TMultiFieldTypeConfig::localStartElement


// resolve
void TMultiFieldTypeConfig::localResolve(bool aLastPass)
{
  // check
  if (aLastPass) {
    if (!fFieldListP)
      SYSYNC_THROW(TConfigParseException("missing 'use' in datatype"));
    #ifdef SCRIPT_SUPPORT
    TScriptContext *sccP = NULL;
    SYSYNC_TRY {
      // resolve all scripts in same context
      // - init script
      TScriptContext::resolveScript(getSyncAppBase(),fInitScript,sccP,fFieldListP);
      // - incoming/outgoing scripts
      TScriptContext::resolveScript(getSyncAppBase(),fIncomingScript,sccP,fFieldListP);
      TScriptContext::resolveScript(getSyncAppBase(),fOutgoingScript,sccP,fFieldListP);
      // - filtering scripts
      TScriptContext::resolveScript(getSyncAppBase(),fFilterInitScript,sccP,fFieldListP);
      TScriptContext::resolveScript(getSyncAppBase(),fPostFetchFilterScript,sccP,fFieldListP);
      // - compare and merge scripts
      #ifdef SYSYNC_SERVER
      TScriptContext::resolveScript(getSyncAppBase(),fCompareScript,sccP,fFieldListP);
      TScriptContext::resolveScript(getSyncAppBase(),fMergeScript,sccP,fFieldListP);
      #endif
      // - special processing of incoming items before they are sent to the DB
      TScriptContext::resolveScript(getSyncAppBase(),fProcessItemScript,sccP,fFieldListP);
      // - forget this context (rebuild will take place in the datastore's fXXXXTypeScriptContextP)
      if (sccP) delete sccP;
    }
    SYSYNC_CATCH (...)
      if (sccP) delete sccP;
      SYSYNC_RETHROW;
    SYSYNC_ENDCATCH
    #endif
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TMultiFieldTypeConfig::localResolve

#endif // CONFIGURABLE_TYPE_SUPPORT


#ifdef HARDCODED_TYPE_SUPPORT

void TMultiFieldTypeConfig::setConfig(TFieldListConfig *aFieldList, const char *aTypeName, const char* aTypeVers)
{
  // set field list
  fFieldListP=aFieldList;
  // set type name/version
  if (aTypeName) fTypeName=aTypeName;
  if (aTypeVers) fTypeVersion=aTypeVers;
} // TMultiFieldTypeConfig::setConfig

#endif // HARDCODED_TYPE_SUPPORT



/*
 * Implementation of TMultiFieldItemType
 */

/* public TMultiFieldItemType members */


TMultiFieldItemType::TMultiFieldItemType(
  TSyncSession *aSessionP,
  TDataTypeConfig *aTypeConfigP,
  const char *aCTType,
  const char *aVerCT,
  TSyncDataStore *aRelatedDatastoreP,
  TFieldListConfig *aFieldDefinitions // field definition list
) :
  TSyncItemType(aSessionP,aTypeConfigP,aCTType,aVerCT,aRelatedDatastoreP)
{
  // save field definition pointer
  fFieldDefinitionsP = aFieldDefinitions;
  if (!fFieldDefinitionsP)
    SYSYNC_THROW(TSyncException(DEBUGTEXT("MultiFieldItemType without fieldDefinitions","mfit1")));
  // create options array
  fFieldOptionsP = new TFieldOptions[fFieldDefinitionsP->numFields()];
  // - init default options
  for (sInt16 i=0; i<fFieldDefinitionsP->numFields(); i++) {
    // by default, all fields in the list are available
    fFieldOptionsP[i].available=true; // available
    fFieldOptionsP[i].maxsize=
      aSessionP->fLimitedRemoteFieldLengths ?
        FIELD_OPT_MAXSIZE_UNKNOWN : // limited, but unknown length
        FIELD_OPT_MAXSIZE_NONE; // no known limit
    fFieldOptionsP[i].maxoccur=0; // maximum occurrence count not defined
    fFieldOptionsP[i].notruncate=false; // allow truncation by default
  }
} // TMultiFieldItemType::TMultiFieldItemType


TMultiFieldItemType::~TMultiFieldItemType()
{
  // remove fields options array
  delete [] fFieldOptionsP;
} // TMultiFieldItemType::~TMultiFieldItemType


// compatibility (=assignment compatibility between items based on these types)
bool TMultiFieldItemType::isCompatibleWith(TSyncItemType *aReferenceType)
{
  // check compatibility
  // - reference must be based on TMultiFieldItemType
  if (!aReferenceType->isBasedOn(ity_multifield)) return false;
  // - both multifields must be based on same field list
  return
    fFieldDefinitionsP == static_cast<TMultiFieldItemType *>(aReferenceType)->fFieldDefinitionsP;
} // TMultiFieldItemType::isCompatibleWith



// Initialize use of datatype with a datastore
// Note: This might not affect any datatype-related members, as datatype can
//       be in use by different datastores at a time. Intitialisation is
//       performed on members of the datastore (such as fXXXXTypeScriptContextP)
void TMultiFieldItemType::initDataTypeUse(TLocalEngineDS *aDatastoreP, bool aForSending, bool aForReceiving)
{
  #ifdef SCRIPT_SUPPORT
  // Note: identifier situation is equal for all calls of initDataTypeUse, however,
  //       as multiple datastores might use the datatype, several instances of the
  //       context (with separate local var VALUEs) might exist.
  // Delete old context(s), if any
  if (aDatastoreP->fSendingTypeScriptContextP && aForSending) {
    // delete only if actually using this type for sending (otherwise, sending context must be left untouched)
    delete aDatastoreP->fSendingTypeScriptContextP;
    aDatastoreP->fSendingTypeScriptContextP=NULL;
  }
  if (aDatastoreP->fReceivingTypeScriptContextP && aForReceiving) {
    // delete only if actually using this type for receiving (otherwise, receiving context must be left untouched)
    delete aDatastoreP->fReceivingTypeScriptContextP;
    aDatastoreP->fReceivingTypeScriptContextP=NULL;
  }
  // Create contexts in datastore and instantiate variables for scripts
  TMultiFieldTypeConfig *cfgP = getMultifieldTypeConfig();
  // Get context
  // NOTE: if initialized for both sending and receiving, only the sending
  //       context will be used for both receiving and sending
  TScriptContext **ctxPP;
  if (aForSending)
    // sending or both
    ctxPP = &aDatastoreP->fSendingTypeScriptContextP;
  else
    // only receiving
    ctxPP = &aDatastoreP->fReceivingTypeScriptContextP;
  // NOTE: always rebuild all scripts, even though
  //       some might remain unused in the sending or receiving context
  //       This is needed because we don't know at ResolveScript() if a
  //       type will be used for input or output only, and rebuild must
  //       be in the same order as Resolve to guarantee same var/field indexes.
  // - init script (will be executed right after all scripts are resolved)
  TScriptContext::rebuildContext(cfgP->getSyncAppBase(),cfgP->fInitScript,*ctxPP,fSessionP);
  // - incoming and outgoing data brush-ups scripts
  TScriptContext::rebuildContext(cfgP->getSyncAppBase(),cfgP->fIncomingScript,*ctxPP,fSessionP);
  TScriptContext::rebuildContext(cfgP->getSyncAppBase(),cfgP->fOutgoingScript,*ctxPP,fSessionP);
  // - filtering scripts
  TScriptContext::rebuildContext(cfgP->getSyncAppBase(),cfgP->fFilterInitScript,*ctxPP,fSessionP);
  TScriptContext::rebuildContext(cfgP->getSyncAppBase(),cfgP->fPostFetchFilterScript,*ctxPP,fSessionP);
  // - compare and merge scripts
  #ifdef SYSYNC_SERVER
  TScriptContext::rebuildContext(cfgP->getSyncAppBase(),cfgP->fCompareScript,*ctxPP,fSessionP);
  TScriptContext::rebuildContext(cfgP->getSyncAppBase(),cfgP->fMergeScript,*ctxPP,fSessionP);
  #endif
  // - special processing of incoming items before they are sent to the DB
  TScriptContext::rebuildContext(cfgP->getSyncAppBase(),cfgP->fProcessItemScript,*ctxPP,fSessionP,true); // now build vars
  // Now execute init script
  // - init the needed member vars
  fDsP = aDatastoreP;
  fFirstItemP = NULL;
  fSecondItemP = NULL;
  fEqMode = eqm_none;
  fCurrentSyncOp = sop_none;
  TScriptContext::execute(
    *ctxPP, // the context
    cfgP->fInitScript, // the script
    &DataTypeFuncTable,  // context function table
    this // context data (myself)
  );
  #endif
} // TMultiFieldItemType::initDataTypeUse



#ifdef OBJECT_FILTERING

// - check if new-style filtering needed
void TMultiFieldItemType::initPostFetchFiltering(bool &aNeeded, bool &aNeededForAll, TLocalEngineDS *aDatastoreP)
{
  // init filter and determine if we need any filtering
  #ifndef SCRIPT_SUPPORT
  aNeeded=false;
  aNeededForAll=false;
  #else
  fNeedToFilterAll=true; // init member that will be acessed by the script
  fDsP=aDatastoreP; // set for access from script funcs
  fFirstItemP = NULL;
  fSecondItemP = NULL;
  fEqMode = eqm_none;
  aNeeded=TScriptContext::executeTest(
    false, // default to no need for any filters
    aDatastoreP->fSendingTypeScriptContextP,
    static_cast<TMultiFieldTypeConfig *>(fTypeConfigP)->fFilterInitScript,
    &DataTypeFuncTable,  // context function table
    this // context data (myself)
  );
  // - copy back script output
  aNeededForAll=fNeedToFilterAll;
  if (!aNeeded) aNeededForAll=false; // keep this consistent
  POBJDEBUGPRINTFX(fSessionP,DBG_FILTER,(
    "Type-specific postfetch filtering %sneeded%s",
    aNeeded ? "" : "NOT ",
    aNeeded ? (aNeededForAll ? " and to be applied to all records" : " only for changed records") : ""
  ));
  #endif
} // TMultiFieldItemType::initPostFetchFiltering


// called by TMultiField::postFetchFiltering()
// Checks if to-be-sent item fetched from DB passes all filters to be actually sent to remote
bool TMultiFieldItemType::postFetchFiltering(TMultiFieldItem *aItemP, TLocalEngineDS *aDatastoreP)
{
  #ifndef SCRIPT_SUPPORT
  return true; // no scripts, always pass
  #else
  // if no script, always pass
  string &script = static_cast<TMultiFieldTypeConfig *>(fTypeConfigP)->fPostFetchFilterScript;
  if (script.empty()) return true;
  // call script to test filter
  fDsP=aDatastoreP; // set for access from script funcs
  fFirstItemP = NULL;
  fSecondItemP = NULL;
  fEqMode = eqm_none;
  fCurrentSyncOp = aItemP->getSyncOp();
  return TScriptContext::executeTest(
    false, // default to not passing if filter script fails or returns no value
    aDatastoreP->fSendingTypeScriptContextP,
    script,
    &DataTypeFuncTable,  // context function table
    this, // context data (myself)
    aItemP, // target item
    true // can be written if needed
  );
  #endif
} // TMultiFieldItemType::postFetchFiltering

#endif


// check item before processing it
bool TMultiFieldItemType::checkItem(TMultiFieldItem &aItem, TLocalEngineDS *aDatastoreP)
{
  #ifndef SCRIPT_SUPPORT
  return true; // simply ok
  #else
  // execute a script if there is a context for it
  fDsP=aDatastoreP; // set for access from script funcs
  fFirstItemP = &aItem;
  fSecondItemP = NULL;
  fEqMode = eqm_none;
  fCurrentSyncOp = aItem.getSyncOp();
  return TScriptContext::execute(
    aDatastoreP->fReceivingTypeScriptContextP ?
      aDatastoreP->fReceivingTypeScriptContextP :
      aDatastoreP->fSendingTypeScriptContextP,
    static_cast<TMultiFieldTypeConfig *>(fTypeConfigP)->fProcessItemScript,
    &DataTypeFuncTable,  // context function table
    this, // context data (myself)
    &aItem, // the item
    true // checking might change data
  );
  fFirstItemP = NULL;
  #endif
} // TMultiFieldItemType::checkItem


#ifdef SYSYNC_SERVER

// compare two items
sInt16 TMultiFieldItemType::compareItems(
  TMultiFieldItem &aFirstItem,
  TMultiFieldItem &aSecondItem,
  TEqualityMode aEqMode,
  bool aDebugShow,
  TLocalEngineDS *aDatastoreP
)
{
  #ifndef SCRIPT_SUPPORT
  // just do standard compare
  return aFirstItem.standardCompareWith(aSecondItem,aEqMode,aDebugShow);
  #else
  // if no script use standard comparison
  sInt16 cmpres;
  string &script = static_cast<TMultiFieldTypeConfig *>(fTypeConfigP)->fCompareScript;
  if (script.empty())
    return aFirstItem.standardCompareWith(aSecondItem,aEqMode,aDebugShow);
  // execute script to perform comparison
  // - set up helpers
  fDsP = aDatastoreP;
  fEqMode = aEqMode;
  fFirstItemP = &aFirstItem;
  fSecondItemP = &aSecondItem;
  fCurrentSyncOp = fDsP->fCurrentSyncOp;
  TItemField *resP=NULL;
  bool ok=TScriptContext::executeWithResult(
    resP, // result will be put here if there is one
    aDatastoreP->fReceivingTypeScriptContextP ?
      aDatastoreP->fReceivingTypeScriptContextP :
      aDatastoreP->fSendingTypeScriptContextP,
    script,
    &DataTypeFuncTable,  // context function table
    this, // context data (myself)
    &aFirstItem, // winning item
    false, // compare is read-only
    &aSecondItem, // loosing item
    false, // compare is read-only
    !PDEBUGTEST(DBG_MATCH+DBG_EXOTIC) // suppress script debug output unless comparison detail display is on
  );
  // items are no longer available
  fFirstItemP = NULL;
  fSecondItemP = NULL;
  // get result
  cmpres=SYSYNC_NOT_COMPARABLE; // default to not comparable (if error or no result)
  if (ok) {
    if (resP) cmpres=resP->getAsInteger();
  }
  // dispose result field
  if (resP) delete resP;
  // return result
  return cmpres;
  #endif
} // TMultiFieldItemType::compareItems


// merge two items
void TMultiFieldItemType::mergeItems(
  TMultiFieldItem &aWinningItem,
  TMultiFieldItem &aLoosingItem,
  bool &aChangedWinning,
  bool &aChangedLoosing,
  TLocalEngineDS *aDatastoreP
)
{
  #ifndef SCRIPT_SUPPORT
  // just do standard merge
  aWinningItem.standardMergeWith(aLoosingItem,aChangedWinning,aChangedLoosing);
  return;
  #else
  // if no script use standard merging
  string &script = static_cast<TMultiFieldTypeConfig *>(fTypeConfigP)->fMergeScript;
  if (script.empty()) {
    aWinningItem.standardMergeWith(aLoosingItem,aChangedWinning,aChangedLoosing);
    return;
  }
  // execute script to perform merge
  // - set up helpers
  fDsP = aDatastoreP;
  fFirstItemP = &aWinningItem;
  fSecondItemP = &aLoosingItem;
  fChangedFirst = aChangedWinning;
  fChangedSecond = aChangedLoosing;
  fCurrentSyncOp = fDsP->fCurrentSyncOp;
  TScriptContext::execute(
    aDatastoreP->fReceivingTypeScriptContextP ?
      aDatastoreP->fReceivingTypeScriptContextP :
      aDatastoreP->fSendingTypeScriptContextP,
    script,
    &DataTypeFuncTable,  // context function table
    this, // context data (myself)
    &aWinningItem, // winning item
    true, // can be written if needed
    &aLoosingItem, // loosing item
    true, // can be written if needed
    !PDEBUGTEST(DBG_CONFLICT) // script output only if conflict details enabled
  );
  // items are no longer available
  fFirstItemP = NULL;
  fSecondItemP = NULL;
  // get change status back
  aChangedWinning = fChangedFirst;
  aChangedLoosing = fChangedSecond;
  #endif
} // TMultiFieldItemType::mergeItems

#endif // SYSYNC_SERVER


// helper to create same-typed instance via base class
TSyncItemType *TMultiFieldItemType::newCopyForSameType(
  TSyncSession *aSessionP,     // the session
  TSyncDataStore *aDatastoreP  // the datastore
)
{
  // create new itemtype of appropriate derived class type that can handle
  // this type
  MP_RETURN_NEW(TMultiFieldItemType,DBG_OBJINST,"TMultiFieldItemType",TMultiFieldItemType(aSessionP,fTypeConfigP,getTypeName(),getTypeVers(),aDatastoreP,fFieldDefinitionsP));
} // TMultiFieldItemType::newCopyForSameType


/// @brief copy CTCap derived info from another SyncItemType
/// @return false if item not compatible
/// @note required to create remote type variants from ruleMatch type alternatives
bool TMultiFieldItemType::copyCTCapInfoFrom(TSyncItemType &aSourceItem)
{
  // must be based on same type as myself and have the same fieldlist
  if (!aSourceItem.isBasedOn(getTypeID()))
    return false; // not compatible
  TMultiFieldItemType *itemTypeP = static_cast<TMultiFieldItemType *>(&aSourceItem);
  if (fFieldDefinitionsP!=itemTypeP->fFieldDefinitionsP)
    return false; // not compatible
  // both have the same fFieldDefinitionsP, so both option arrays have the same size
  // - we can copy the options 1:1
  for (sInt16 i=0; i<fFieldDefinitionsP->numFields(); i++) {
    // by default, all fields in the list are available
    fFieldOptionsP[i] = itemTypeP->fFieldOptionsP[i];
  }
  // other CTCap info is in the field options of MultiFieldItemType
  return inherited::copyCTCapInfoFrom(aSourceItem);
} // TMultiFieldItemType::copyCTCapInfoFrom



// apply default limits to type (e.g. from hard-coded template in config)
void TMultiFieldItemType::addDefaultTypeLimits(void)
{
  #ifdef HARDCODED_TYPE_SUPPORT
  // get default type settings from field list template
  if (fFieldDefinitionsP && fFieldDefinitionsP->fFieldListTemplateP) {
    // copy options from field list template
    for (sInt16 i=0; i<fFieldDefinitionsP->fFieldListTemplateP->numFields; i++) {
      // get options
      TFieldOptions *optP = getFieldOptions(i);
      // transfer limits if not already defined
      // - max field size
      if (optP->maxsize==FIELD_OPT_MAXSIZE_NONE)
        optP->maxsize=fFieldDefinitionsP->fFieldListTemplateP->fieldDefs[i].maxSize;
      // - notruncate option
      if (!optP->notruncate)
        optP->notruncate=fFieldDefinitionsP->fFieldListTemplateP->fieldDefs[i].noTruncate;
    }
  }
  #endif
} // TMultiFieldItemType::addDefaultTypeLimits



// access to fields by name (returns FID_NOT_SUPPORTED if field not found)
sInt16 TMultiFieldItemType::getFieldIndex(const char *aFieldName)
{
  for (sInt16 i=0; i<fFieldDefinitionsP->numFields(); i++) {
    if (strucmp(aFieldName,fFieldDefinitionsP->fFields[i].TCFG_CSTR(fieldname))==0)
      return i; // found
  }
  return FID_NOT_SUPPORTED; // not found
} // TMultiFieldItemType::getFieldIndex


bool TMultiFieldItemType::isFieldIndexValid(sInt16 aFieldIndex)
{
  return (aFieldIndex>=0 && aFieldIndex<fFieldDefinitionsP->numFields());
} // TMultiFieldItemType::isFieldIndexValid


// access to field options (returns NULL if field not found)
TFieldOptions *TMultiFieldItemType::getFieldOptions(sInt16 aFieldIndex)
{
  if (!isFieldIndexValid(aFieldIndex)) return NULL;
  return &(fFieldOptionsP[aFieldIndex]);
} // TMultiFieldItemType::getFieldOptions


// test if item could contain cut-off data (e.g. because of field size restrictions)
// compared to specified reference item
// NOTE: if no reference item is specified, any occurrence of a
//       size-limited field will signal possible cutoff
bool TMultiFieldItemType::mayContainCutOffData(TSyncItemType *aReferenceType)
{
  TMultiFieldItemType *refP=NULL;
  if (aReferenceType->isBasedOn(ity_multifield))
    refP=static_cast<TMultiFieldItemType *>(aReferenceType);
  for (sInt16 i=0; i<fFieldDefinitionsP->numFields(); i++) {
    sInt32 refsize,mysize;
    mysize = getFieldOptions(i)->maxsize;
    if (refP)
      refsize = refP->getFieldOptions(i)->maxsize;
    else
      refsize = FIELD_OPT_MAXSIZE_NONE; // assume no limit
    // now sort out cases: cutOff data can happen if
    if (
      // - this field is somehow limited && reference field is unlimited or limited, but unknown
      (mysize!=FIELD_OPT_MAXSIZE_NONE && (refsize<=0)) ||
      // - this field has smaller size than reference field
      (mysize<refsize && mysize>0 && refsize>0)
    ) {
      // field could contain cut-off data (when conataining data from reference type)
      return true;
    }
  }
  // no field limits detected, no cutoff
  return false;
} // TMultiFieldItemType::mayContainCutOffData



// fill in SyncML data (but leaves IDs empty)
//   Note: for MultiFieldItem, this is for post-processing data (FieldFillers)
bool TMultiFieldItemType::internalFillInData(
  TSyncItem *aSyncItemP,      // SyncItem to be filled with data
  SmlItemPtr_t aItemP,        // SyncML toolkit item Data to be converted into SyncItem (may be NULL if no data, in case of Delete or Map)
  TLocalEngineDS *aLocalDatastoreP, // local datastore
  TStatusCommand &aStatusCmd  // status command that might be modified in case of error
)
{
  #if defined(SCRIPT_SUPPORT)
  // check type
  TMultiFieldItem *itemP;
  GET_CASTED_PTR(itemP,TMultiFieldItem,aSyncItemP,DEBUGTEXT("TMultiFieldItemType::internalFillInData: incompatible item class","mfit2"));
  #endif
  #ifdef SCRIPT_SUPPORT
  // post-process data: apply incoming script
  // - init the needed member vars
  fDsP = aLocalDatastoreP;
  fFirstItemP = itemP;
  fSecondItemP = NULL;
  fEqMode = eqm_none;
  fCurrentSyncOp = itemP->getSyncOp();
  TScriptContext::execute(
    aLocalDatastoreP->fReceivingTypeScriptContextP ?
      aLocalDatastoreP->fReceivingTypeScriptContextP : // separate context for receiving, use it
      aLocalDatastoreP->fSendingTypeScriptContextP, // send and receive use same context (sending one)
    static_cast<TMultiFieldTypeConfig *>(fTypeConfigP)->fIncomingScript,
    &DataTypeFuncTable,
    this,
    itemP,
    true,
    NULL,
    false,
    !PDEBUGTEST(DBG_PARSE)
  );
  fFirstItemP = NULL;
  #endif
  // - do NOT call ancestor (it's a dummy for types w/o implementation returning false)
  // can't go wrong
  return true;
} // TMultiFieldItemType::internalFillInData


// sets data and meta from SyncItem data, but leaves source & target untouched
//   Note: for MultiFieldItem, this is for pre-processing data (FieldFillers)
bool TMultiFieldItemType::internalSetItemData(
  TSyncItem *aSyncItemP,  // the syncitem to be represented as SyncML
  SmlItemPtr_t aItem,     // item with NULL meta and NULL data
  TLocalEngineDS *aLocalDatastoreP // local datastore
)
{
  #if defined(SCRIPT_SUPPORT)
  // check type
  TMultiFieldItem *itemP;
  GET_CASTED_PTR(itemP,TMultiFieldItem,aSyncItemP,DEBUGTEXT("TMultiFieldItemType::internalSetItemData: incompatible item class","mdit8"));
  // do NOT call ancestor (it's a dummy for types w/o implementation returning false)
  #endif
  #ifdef SCRIPT_SUPPORT
  // pre-process data: apply outgoing script
  // - init the needed member vars
  fDsP = aLocalDatastoreP;
  fFirstItemP = itemP;
  fSecondItemP = NULL;
  fEqMode = eqm_none;
  fCurrentSyncOp = itemP->getSyncOp();
  return TScriptContext::execute(
    aLocalDatastoreP->fSendingTypeScriptContextP,
    static_cast<TMultiFieldTypeConfig *>(fTypeConfigP)->fOutgoingScript,
    &DataTypeFuncTable,
    this,
    itemP,
    true,
    NULL,
    false,
    !PDEBUGTEST(DBG_GEN)
  );
  fFirstItemP = NULL;
  #else
  // can't go wrong
  return true;
  #endif
} // TMultiFieldItemType::internalSetItemData


} // namespace sysync

/* end of TMultiFieldItemType implementation */

// eof
