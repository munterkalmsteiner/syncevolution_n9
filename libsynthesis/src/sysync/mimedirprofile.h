/*
 *  File:         mimedirprofile.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TMimeDirItemType
 *    base class for MIME DIR based content types (vCard, vCalendar...)
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2009-01-09 : luz : created from mimediritemtype.h
 *
 */

#ifndef MimeDirProfile_H
#define MimeDirProfile_H

// includes
#include "syncitemtype.h"
#include "multifielditemtype.h"
#include "engine_defs.h"

#include <set>

namespace sysync {

// conversion mode is a basic value plus some optional flags
#define CONVMODE_MASK 0xFF // 8 bits for basic convmode

// flags
#define CONVMODE_FLAG_EXTFMT 0x0100 // use ISO8601 extended format for rendering date and time
#define CONVMODE_FLAG_MILLISEC 0x0200 // render milliseconds

// special field conversion modes
#define CONVMODE_NONE 0     // no conversion (just string copy), but includes value list parsing and enum conversion
#define CONVMODE_VERSION 1  // version
#define CONVMODE_PRODID 2  // PRODID
#define CONVMODE_TIMESTAMP 3 // forced full timestamp, even if this is a date field
#define CONVMODE_DATE 4 // forced date-only, even if this is a timestamp field
#define CONVMODE_AUTODATE 5 // in vCal 1.0 style formats, force to timestamp representation, in MIME-DIR, show depending on actual value
#define CONVMODE_AUTOENDDATE 6 // in vCal 1.0 style formats, force to timestamp representation - if actual value is a date, subtract 1 unit (to show end of previous day rather than midnight of next)
#define CONVMODE_TZ 7 // time zone offset in ISO8601 hh:mm representation for vCalendar 1.0 representation
#define CONVMODE_DAYLIGHT 8 // DAYLIGHT property to describe time zone with DST in vCalendar 1.0
#define CONVMODE_TZID 9 // time zone ID for iCalendar 2.0
#define CONVMODE_EMPTYONLY 10 // same as CONVMODE_NONE, except that assignment occurs only if field is still empty
#define CONVMODE_BITMAP 11 // values, converted to integer, are interpreted as bit numbers
#define CONVMODE_BLOB_B64 12 // 1:1 storage of (decoded) value into field. When encoding, b64 is used
#define CONVMODE_MAILTO 13 // 1:1 storage of (decoded) value into field. When encoding, b64 is used
#define CONVMODE_VALUETYPE 14 // automatic VALUE parameter e.g. for timestamp fields that contain a date-only value (VALUE=DATE) or duration (VALUE=DURATION)
#define CONVMODE_MULTIMIX 15 // special mode for mapping enums to bits (like CONVMODE_BITMAP), but mixed from multiple fields and with option to store as-is (special enum "value" syntax needed)
#define CONVMODE_FULLVALUETYPE 16 // explicit VALUE parameter, does not assume a default
#define CONVMODE_BLOB_AUTO 17 // like CONVMODE_BLOB_B64, but if data consists of printable ASCII-chars only, no B64 encoding is used

// derived type modes start here
#define CONVMODE_MIME_DERIVATES 20

// define those that we want to implement (also work as getConfMode conditionals)
#define CONVMODE_RRULE CONVMODE_MIME_DERIVATES+0 // RRULE, needs RRULE field block


// special numvals
#define NUMVAL_LIST -1     // property contains a value list (like EXDATE) rather than individual values (like N)
#define NUMVAL_REP_LIST -2 // same as NUMVAL_LIST, but property is output as repetition of the entire property rather than as list in single property



// profile internal MIME-DIR mode (externally, profile mode is set by PROFILEMODE_xxx and setProfileMode()
typedef enum {
  mimo_old,     // vCard 2.1 type encoding, CRLF, default params
  mimo_standard, // MIME DIR conformant, vCard 3.0 mode
  numMimeModes
} TMimeDirMode;


// VTIMEZONE generation mode (what timezone definition rules to include)
typedef enum {
  vtzgen_current,
  vtzgen_start,
  vtzgen_end,
  vtzgen_range,
  vtzgen_openend,
  numVTimeZoneGenModes
} TVTimeZoneGenMode;


typedef enum {
  tzidgen_default,
  tzidgen_olson,
  numTzIdGenModes
} TTzIdGenMode;



// name extension map
typedef uInt32 TNameExtIDMap;

// forward
class TProfileDefinition;
class TPropertyDefinition;
class TMimeDirItemType;
class TRemoteRuleConfig;

// enumeration modes
typedef enum {
  enm_translate,      // translation from value to name and vice versa
  enm_prefix,         // enumtext/enumval are prefixes of
  enm_default_name,   // default name when translating from value to name
  enm_default_value,  // default value when translating from name to value
  enm_ignore,         // ignore value or name
  numEnumModes
} TEnumMode;


// enumeration element definition
class TEnumerationDef {
public:
  // constructor/destructor
  TEnumerationDef(const char *aEnumName, const char *aEnumVal, TEnumMode aMode, sInt16 aNameExtID=-1);
  ~TEnumerationDef();
  // next item
  TEnumerationDef *next;
  // enum text
  TCFG_STRING enumtext;
  // enum translation (value to be stored in DB field), NULL if no translation
  TCFG_STRING enumval;
  // enum mode
  TEnumMode enummode;
  // ID (0..31) identifying ID of this value for property name extension purposes
  // -1 means value is irrelevant to name extension
  sInt16 nameextid;
}; // TEnumerationDef


// conversion & storage definition
class TConversionDef {
public:
  // constructor/destructor
  TConversionDef();
  ~TConversionDef();
  // tools
  void addEnum(const char *aEnumName, const char *aEnumVal, TEnumMode aMode=enm_translate);
  void addEnumNameExt(TPropertyDefinition *aProp, const char *aEnumName, const char *aEnumVal=NULL, TEnumMode aMode=enm_translate);
  TConversionDef *setConvDef(sInt16 aFieldId=FID_NOT_SUPPORTED,sInt16 aConvMode=0,char aCombSep=0);
  const TEnumerationDef *findEnumByName(const char *aName, sInt16 n=0) const;
  const TEnumerationDef *findEnumByVal(const char *aVal, sInt16 n=0) const;
  // base field id for parameter (will be offset for name-extended and repeated properties)
  sInt16 fieldid;    // VARIDX_UNDEFINED (negative) means value is not supported
  // enumeration list, NULL if none
  TEnumerationDef *enumdefs;
  // conversion
  sInt16 convmode;   // 0=direct, 1..n=special procedure needed
  char combineSep;  // 0=no combination, char=char to be used to combine multiple values in field
}; // TConversionDef


// parameter definition
class TParameterDefinition {
public:
  // constructor/destructor
  TParameterDefinition(const char *aName, bool aDefault, bool aExtendsName, bool aShowNonEmpty, bool aShowInCTCap, TMimeDirMode aModeDep);
  ~TParameterDefinition();
  // tools
  TConversionDef *setConvDef(sInt16 aFieldId=FID_NOT_SUPPORTED,sInt16 aConvMode=0,char aCombSep=0)
    { return convdef.setConvDef(aFieldId,aConvMode,aCombSep); };
  TNameExtIDMap getExtIDbit(const char *aEnumName, sInt16 n=0);
  // next
  TParameterDefinition *next;
  // parameter name
  TCFG_STRING paramname; // NULL for terminator
  // used as default param, for example for type tags which have no explicit TYPE= in older vXX formats
  bool defaultparam;
  // parameter exists only in specific MIME-DIR mode (set to numMimeModes for non-dependent parameter)
  TMimeDirMode modeDependency;
  // parameter can extend property name by enumerated values with nameextid's
  bool extendsname;
  // if parameter has non-empty value, property will be treated as non-empty
  bool shownonempty;
  // flag if parameter should be (not necessarily IS, depending on SyncML version) shown in CTCap
  bool showInCTCap;
  // conversion information
  TConversionDef convdef;
  #ifndef NO_REMOTE_RULES
  // rule processing is simpler than with properties:
  // a parameter is expanded or parsed if no rule was set or the given
  // rule is active
  TRemoteRuleConfig *ruleDependency;
  // name of remote rule dependency (will be resolved to set ruleDependency)
  TCFG_STRING dependencyRuleName;
  #endif
}; // TParameterDefinition


// property name extension by values of parameters
class TPropNameExtension {
public:
  // constructor/destructor
  TPropNameExtension(
    TNameExtIDMap aMusthave_ids, TNameExtIDMap aForbidden_ids, TNameExtIDMap aAddtlSend_ids,
    sInt16 aFieldidoffs, sInt16 aMaxRepeat, sInt16 aRepeatInc, sInt16 aMinShow,
    bool aOverwriteEmpty, bool aReadOnly, sInt16 aRepeatID
  );
  ~TPropNameExtension();
  // link to next, NULL if end
  TPropNameExtension *next;
  // Bitmap, has bit set for each nameextid which must be present for property
  // in order to store it with given fid offset.
  // - This nameextids will also be used to generate properties with
  //   corresponding parameter values.
  TNameExtIDMap musthave_ids;
  // Bitmap, has bit set for each nameextid which MAY NOT be present for
  // property in order to store it with given fid offset.
  TNameExtIDMap forbidden_ids;
  // Bitmap, has bit set for each nameextid which should additionally be present
  // when sending the property (such as VOICE for telephone), but is not a musthave.
  TNameExtIDMap addtlSend_ids;
  // field ID offset to be used for storage of property value(s) and
  // non-name extending parameter value(s) on musthave_ids/forbidden_ids matches
  // OFFS_NOSTORE : prevents storing/generating of this property value
  sInt16 fieldidoffs;
  // allowable repeat count (adding repeatInc to all related field ids when property is repeated)
  // - if set to REP_REWRITE (=0), value can occur multiple times, but later occurrences
  //   override earlier ones (no offset incrementing)
  // - if set to REP_ARRAY, repeat is unlimited (should be used with array fields only)
  sInt16 maxRepeat;
  // if maxRepeat>1, the offset is incremented by the given value
  // (to allow multi-value properties to be repeated blockwise, not field-by-field)
  sInt16 repeatInc;
  // minimal number of times the property should be shown, even if repetitions do not contain
  // any values. If set to 0, property will not show at all if no values are there
  sInt16 minShow;
  // flag to allow overwriting empty instances with next non-empty instance of same property
  bool overwriteEmpty;
  // flag for name extension variants only used for parsing, not for generating
  bool readOnly;
  // unique (over all TPropNameExtension) ID used for keeping track of repetitions at parsing
  sInt16 repeatID;
}; // TPropNameExtension


// property definition
class TPropertyDefinition {
public:
  // constructor/destructor
  TPropertyDefinition(const char* aName, sInt16 aNumVals, bool aMandatory, bool aShowInCTCap, bool aSuppressEmpty, uInt16 aDelayedProcessing, char aValuesep, char aAltValuesep, uInt16 aPropertyGroupID, bool aCanFilter, TMimeDirMode aModeDep, sInt16 aGroupFieldID, bool aAllowFoldAtSep);
  ~TPropertyDefinition();
  // tools
  TParameterDefinition *addParam(const char *aName, bool aDefault, bool aExtendsName, bool aShowNonEmpty=false, bool aShowInCTCap=false, TMimeDirMode aModeDep=numMimeModes);
  void addNameExt(TProfileDefinition *aRootProfile, // for profile-global RepID generation
    TNameExtIDMap aMusthave_ids, TNameExtIDMap aForbidden_ids, TNameExtIDMap aAddtlSend_ids,
    sInt16 aFieldidoffs, sInt16 aMaxRepeat=1, sInt16 aRepeatInc=1, sInt16 aMinShow=-1,
    bool aOverwriteEmpty=true, // show all, but overwrite empty repetitions
    bool aReadOnly=false, // not a parsing alternative
    sInt16 aShareCountOffs=0 // not sharing the repeat ID with a previous name extension
  );
  TConversionDef *setConvDef(sInt16 aValNum, sInt16 aFieldId=FID_NOT_SUPPORTED,sInt16 aConvMode=0,char aCombSep=0);
  TParameterDefinition *findParameter(const char *aNam, sInt16 aLen=0);
  // next in list
  TPropertyDefinition *next;
  // property name
  TCFG_STRING propname;
  // property name extension list.
  // - If NULL, property is non-repeatable and has no name extensions at all
  TPropNameExtension *nameExts;
  // ID of group field
  sInt16 groupFieldID;
  // number of values
  sInt16 numValues;
  // conversion specification(s) for each value
  TConversionDef *convdefs;
  // if set, property is not processed but stored entirely (unfolded, but otherwise unprocessed) in the first <value> defined
  // This gets automatically set when a property name contains an asterisk wildcard character
  bool unprocessed;
  // if set, property has a list of values that are stored in an array field or
  // by offseting fid. Note that a PropNameExtension is needed to allow storing more
  // than a single value. If valuelist=true, convdefs should only contain a single entry,
  // other entries are not used
  bool valuelist;
  // if set, valuelist properties should be rendered by repeating the property instead of creating a list of values in one property
  bool expandlist;
  // char to separate value list items (defaults to semicolon)
  char valuesep;
  char altvaluesep; // second value separator to respect when parsing (generating always uses valuesep)
  bool allowFoldAtSep; // allow folding at value separators (for mimo_old, even if it inserts an extra space)
  // parameter listm
  TParameterDefinition *parameterDefs;
  // mandatory
  bool mandatory;
  // flag if property should be shown in CTCap
  bool showInCTCap;
  // flag if property can be used in filters (and will be shown in FilterCap, when showInCTCap is true as well
  bool canFilter;
  // flag if property should be suppressed if it has only empty values
  bool suppressEmpty;
  // delayed processing order (for things like RRULE that must be processed at the end)
  uInt16 delayedProcessing;
  // property exists only in specific MIME-DIR mode (set to numMimeModes for non-dependent properties)
  TMimeDirMode modeDependency;
  // internal for creation of name extension IDs
  sInt16 nextNameExt;
  // property group ID
  uInt16 propGroup; // starting at 1, groups subsequent props that have the same name
  #ifndef NO_REMOTE_RULES
  // set if property enabled only if ruleDependency matches session's applied rule (even if NULL = no rule must be applied)
  bool dependsOnRemoterule;
  // the rule that must be selected to enable this property. If NULL, this property
  // is used only if NO rule is selected.
  TRemoteRuleConfig *ruleDependency;
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  // name of remote rule dependency (will be resolved to set ruleDependency)
  TCFG_STRING dependencyRuleName;
  #endif
  #endif
}; // TPropertyDefinition


// enumeration modes
typedef enum {
  profm_custom,       // custom defined profile/subprofile
  profm_vtimezones,   // VTIMEZONE profile(s), expands to a VTIMEZONE for every time zone referenced by convmode TZID fields
  numProfileModes
} TProfileModes;


// Profile level definition
class TProfileDefinition {
public:
  // constructor/destructor
  TProfileDefinition(
    TProfileDefinition *aParentProfileP, // parent profile
    const char *aProfileName, // name
    sInt16 aNumMandatory,
    bool aShowInCTCapIfSelectedOnly,
    TProfileModes aProfileMode,
    TMimeDirMode aModeDep
  );
  ~TProfileDefinition();
  // tools
  TConversionDef *setConvDef(sInt16 aFieldId=FID_NOT_SUPPORTED,sInt16 aConvMode=0,char aCombSep=0)
    { return levelConvdef.setConvDef(aFieldId,aConvMode,aCombSep); };
  TProfileDefinition *addSubProfile(
    const char *aProfileName, // name
    sInt16 aNumMandatory,
    bool aShowInCTCapIfSelectedOnly,
    TProfileModes aProfileMode = profm_custom,
    TMimeDirMode aModeDep = numMimeModes
  );
  TPropertyDefinition *addProperty(
    const char *aName, // name
    sInt16 aNumValues, // number of values, NUMVAL_LIST/NUMVAL_REP_LIST if it is a value list
    bool aMandatory, // mandatory
    bool aShowInCTCap, // show in CTCap
    bool aSuppressEmpty, // suppress empty ones on send
    uInt16 aDelayedProcessing=0, // delayed processing when parsed, 0=immediate processing, 1..n=delayed
    char aValuesep=';', // value separator
    uInt16 aPropertyGroupID=0, // property group ID (alternatives for same-named properties should have same ID>0)
    bool aCanFilter=false, // can be filtered -> show in filter cap
    TMimeDirMode aModeDep=numMimeModes, // property valid only for specific MIME mode
    char aAltValuesep=0, // no alternate separator
    sInt16 aGroupFieldID=FID_NOT_SUPPORTED, // no group field
    bool aAllowFoldAtSep=false // do not fold at separators when it would insert extra spaces
  );
  void usePropertiesOf(TProfileDefinition *aProfile);
  TPropertyDefinition *getPropertyDef(const char *aPropName);
  sInt16 getPropertyMainFid(const char *aPropName, uInt16 aIndex);
  TProfileDefinition *findProfile(const char *aNam);
  // next in chain
  TProfileDefinition *next;
  // parent profile
  TProfileDefinition *parentProfile; // NULL if root
  // Profile Level name
  TCFG_STRING levelName;
  // Level existence control field (affected/tested when level entered)
  // NOTE: levelConvdef.enumdefs is used specially: it points to a SINGLE entry,
  //       not an array (no terminator!!). This entry contains IN THE enumval
  //       field (NOT enumtext!!!) the value to be stored in the field
  //       indicated by fieldid when this level is entered with BEGIN
  TConversionDef levelConvdef;
  // this subprofile (and related properties and sub-sub-profiles)
  // is shown in devInf only if it is explictly selected or if all are selected
  bool shownIfSelectedOnly;
  // number of mandatory properties in this level
  sInt16 numMandatoryProperties;
  // property list, NULL if none
  TPropertyDefinition *propertyDefs;
  // sublevel list, NULL if none
  TProfileDefinition *subLevels;
  // next repeat ID for this (root) profile
  sInt16 nextRepID;
  // profile mode (custom profile or predefined profile like vTIMEZONE)
  TProfileModes profileMode;
  // dependency on MIME-dir mode
  TMimeDirMode modeDependency;
private:
  bool ownsProps;
}; // TProfileDefinition


// MIME profile definition config
class TMIMEProfileConfig : public TProfileConfig
{
  typedef TProfileConfig inherited;
public:
  TMIMEProfileConfig(const char* aName, TConfigElement *aParentElement);
  virtual ~TMIMEProfileConfig();
  // handler factory
  virtual TProfileHandler *newProfileHandler(TMultiFieldItemType *aItemTypeP);
  // properties
  // - root profile
  TProfileDefinition *fRootProfileP;
  // - options
  bool fUnfloatFloating; // set if floating timestamps should always be unfloated into item time zone
  TVTimeZoneGenMode fVTimeZoneGenMode; // how outgoing VTIMEZONE records should be generated
  TTzIdGenMode fTzIdGenMode; // what type of TZIDs should be generated
protected:
  // check config elements
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
public:
  virtual void localResolve(bool aLastPass);
protected:
  virtual void nestedElementEnd(void);
  // parse conversion mode
  bool getConvMode(cAppCharP aText, sInt16 &aConvMode);
  #endif
  virtual void clear();
private:
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  // parsing help
  bool getConvAttrs(const char **aAttributes, sInt16 &aFid, sInt16 &aConvMode, char &aCombSep);
  bool getMask(const char **aAttributes, const char *aName, TParameterDefinition *aParamP, TNameExtIDMap &aMask);
  bool processPosition(TParameterDefinition *aParamP, const char **aAttributes);
  // parsing vars
  TProfileDefinition *fOpenProfile; // profile being parsed
  TPropertyDefinition *fOpenProperty; // property being parsed
  TParameterDefinition *fOpenParameter; // parameter being parsed
  TConversionDef *fOpenConvDef; // conversion definition being parsed
  TPropertyDefinition *fLastProperty; // last property added in profile (to build groups)
  uInt16 fPropertyGroupID; // property grouping
  #endif
}; // TMIMEProfileConfig



// delayed property parsing info
typedef struct {
  uInt16 delaylevel;
  const char *start;
  const char *groupname;
  size_t groupnameLen;
  const TPropertyDefinition *propDefP;
} TDelayedPropParseParams;
// delayed property parsing list
typedef std::list<TDelayedPropParseParams> TDelayedParsingPropsList;

// used time context set
typedef std::set<timecontext_t> TTCtxSet;
// parsed TZID map
typedef std::map<string,timecontext_t> TParsedTzidSet;



class TMimeDirProfileHandler : public TProfileHandler
{
  typedef TProfileHandler inherited;
public:
  // constructor
  TMimeDirProfileHandler(
    TMIMEProfileConfig *aMIMEProfileCfgP,
    TMultiFieldItemType *aItemTypeP
  );
  // destructor
  virtual ~TMimeDirProfileHandler();
  #ifdef OBJECT_FILTERING
  // filtering
  // - get field index of given filter expression identifier.
  virtual sInt16 getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex);
  // - add keywords and property names to filterCap
  virtual void addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps, TTypeVariantDescriptor aVariantDescriptor, TSyncItemType *aItemTypeP);
  #endif
  // - obtain property list for type, returns NULL if none available
  virtual SmlDevInfCTDataPropListPtr_t newCTDataPropList(TTypeVariantDescriptor aVariantDescriptor, TSyncItemType *aItemTypeP);
  // - Analyze CTCap part of devInf
  virtual bool analyzeCTCap(SmlDevInfCTCapPtr_t aCTCapP, TSyncItemType *aItemTypeP);
  // set profile options
  // - mode (for those profiles that have more than one, like MIME-DIR's old/standard)
  virtual void setProfileMode(sInt32 aMode);
  #ifndef NO_REMOTE_RULES
  // set specific remote rule and activate the behavior defined by it;
  // to be used only in script context, inside a session the session
  // properties are used instead
  virtual void setRemoteRule(const string &aRemoteRuleName);
  #endif
  // generate Text Data (includes header and footer)
  virtual void generateText(TMultiFieldItem &aItem, string &aString);
  // parse Data item (includes header and footer)
  virtual bool parseText(const char *aText, stringSize aTextSize, TMultiFieldItem &aItem);
  // scan for specific property value string (for version check)
  bool parseForProperty(const char *aText, const char *aPropName, string &aString);
  bool parseForProperty(SmlItemPtr_t aItemP, const char *aPropName, string &aString);
  // get profile definition
  TProfileDefinition *getProfileDefinition(void) { return fProfileDefinitionP; };
private:
  // Settable options
  // - profile mode
  sInt32 fProfileMode;
  // - derived from profile mode: MIME dir mode
  TMimeDirMode fMimeDirMode;
  // - ability of receiver to handle UTC
  bool fReceiverCanHandleUTC;
  // - how end dates should be formatted
  bool fVCal10EnddatesSameDay;
  // - empty property policy
  bool fDontSendEmptyProperties;
  // - default output charset
  TCharSets fDefaultOutCharset;
  // - default input interpretation charset
  TCharSets fDefaultInCharset;
  // - user time context
  timecontext_t fReceiverTimeContext;
  // - set if any 8-bit content must be encoded QUOTED-PRINTABLE
  bool fDoQuote8BitContent;
  // - set if no line folding should be done
  bool fDoNotFoldContent;
  // - time handling
  bool fTreatRemoteTimeAsLocal;
  bool fTreatRemoteTimeAsUTC;
  #ifndef NO_REMOTE_RULES
  // - dependency on certain remote rule(s)
  TRemoteRulesList fActiveRemoteRules; // list of active remote rules that might influence behaviour
  bool isActiveRule(TRemoteRuleConfig *aRuleP); // check if given rule is among the active ones
  #endif
  // vars
  TMIMEProfileConfig *fProfileCfgP; // the MIME-DIR profile config element
  // property definitions
  TProfileDefinition *fProfileDefinitionP;
  // BEGIN/END nesting
  sInt16 fBeginEndNesting;
  // time zone management
  bool fHasExplicitTZ; // parsed or generated explicit time zone for entire item (CONVMODE_TZ)
  timecontext_t fItemTimeContext; // time zone context for entire item, is copy of session's user context as long as fHasExplicitTZ not set
  timecontext_t fPropTZIDtctx; // property level time zone context - is set whenever a CONVMODE_TZID is successfully parsed or generated, reset at property start
  TTCtxSet fUsedTCtxSet; // all time contexts used in this item (for generating)
  lineartime_t fEarliestTZDate; // earliest date/time generated using a time context from fUsedTCtxSet
  lineartime_t fLatestTZDate; // latest date/time generated using a time context from fUsedTCtxSet
  TParsedTzidSet fParsedTzidSet; // all time contexts parsed in from VTIMEZONE
  // delayed generation of VTIMEZONE subprofile (after all TZID are collected in fUserTctxSet)
  const TProfileDefinition *fVTimeZonePendingProfileP; // the profile definition, NULL if none
  size_t fVTimeZoneInsertPos; // where to insert VTIMEZONE
  // delayed processing
  TDelayedParsingPropsList fDelayedProps; // list of properties to parse out-of-order
  // helper
  void getOptionsFromDatastore(void);
protected:
  // generate MIME-DIR from item into string object
  void generateMimeDir(TMultiFieldItem &aItem, string &aString);
  // parse MIME-DIR from specified string into item
  bool parseMimeDir(const char *aText, TMultiFieldItem &aItem);
  // special field translations (to be overridden in derived classes)
  // - returns the size of the field block (how many fids in sequence) related
  //   to a given convdef (for multi-field conversion modes such as CONVMODE_RRULE
  virtual sInt16 fieldBlockSize(const TConversionDef &aConvDef);
  // - field value to string for further MIME-DIR generation processing
  virtual bool fieldToMIMEString(
    TMultiFieldItem &aItem,           // the item where data goes to
    sInt16 aFid,                       // the field ID (can be NULL for special conversion modes)
    sInt16 aArrIndex,                  // the repeat offset to handle array fields
    const TConversionDef *aConvDefP,  // the conversion definition record
    string &aString                   // output string
  );
  // - single MIME-DIR value string to field (to be overridden in derived classes)
  virtual bool MIMEStringToField(
    const char *aText,                // the value text to assign or add to the field
    const TConversionDef *aConvDefP,  // the conversion definition record
    TMultiFieldItem &aItem,           // the item where data goes to
    sInt16 aFid,                       // the field ID (can be NULL for special conversion modes)
    sInt16 aArrIndex                   // the repeat offset to handle array fields
  );
private:
  // helpers for CTCap/FilterCap
  // - set field options (enabled, maxsize, maxoccur, notruncate) of fields related to aPropP property in profiles recursively
  //   or (if aPropP is NULL), enable fields of all mandatory properties
  void setfieldoptions(
    const SmlDevInfCTDataPtr_t aPropP, // property to enable fields for, NULL if all mandatory properties should be enabled
    const TProfileDefinition *aProfileP,
    TMimeDirItemType *aItemTypeP
  );
  // - set level
  bool setLevelOptions(const char *aLevelName, bool aEnable, TMimeDirItemType *aItemTypeP);
  // - add level description to CTCap list
  void enumerateLevels(const TProfileDefinition *aProfileP, SmlPcdataListPtr_t *&aPcdataListPP, const TProfileDefinition *aSelectedProfileP, TMimeDirItemType *aItemTypeP);
  // - add property description to CTCap list
  void enumerateProperties(const TProfileDefinition *aProfileP, SmlDevInfCTDataPropListPtr_t *&aPropListPP, const TProfileDefinition *aSelectedProfileP, TMimeDirItemType *aItemTypeP);
  // - enumerate filter properties
  void enumeratePropFilters(const TProfileDefinition *aProfileP, SmlPcdataListPtr_t &aFilterProps, const TProfileDefinition *aSelectedProfileP, TMimeDirItemType *aItemTypeP);
  // - check mode
  bool mimeModeMatch(TMimeDirMode aMimeMode);
  // helpers for generateMimeDir()
  // - generate parameter or property value(list),
  //   returns: 2 if value found, 1 if no value (but field supported), 0 if field not supported
  sInt16 generateValue(
    TMultiFieldItem &aItem,   // the item where data comes from
    const TConversionDef *aConvDefP,
    sInt16 aBaseOffset,          // basic fid offset to use
    sInt16 aArrayOffset,         // additional offset to use, or array index in case of array field
    string &aString,            // where value is ADDED
    char aSeparator,
    TMimeDirMode aMimeMode,     // MIME mode (older or newer vXXX format compatibility)
    bool aParamValue,           // set if generating parameter value (different escaping rules, i.e. ";" and ":" must be escaped)
    bool aStructured,           // set if value consists of multiple values (needs ";" escaping)
    bool aCommaEscape,          // set if "," content escaping is needed (for values in valuelists like TYPE=TEL,WORK etc.)
    TEncodingTypes &aEncoding,  // modified if special value encoding is required
    bool &aNonASCII,            // set if any non standard 7bit ASCII-char is contained
    char aFirstChar,            // will be appended before value if there is any value
    sInt32 &aNumNonSpcs,        // how many non-spaces are already in the value
    bool aFoldAtSeparator,      // if true, even in mimo_old folding may appear at value separators (adding an extra space - which is ok for EXDATE and similar)
    bool aEscapeOnlyLF          // if true, only linefeeds are escaped as \n, but nothing else (not even \ itself)
  );
  // - recursive expansion of properties
  void expandProperty(
    TMultiFieldItem &aItem, // the item where data comes from
    string &aString, // the string to add properties to
    const char *aPrefix, // the prefix (property name)
    const TPropertyDefinition *aPropP, // the property to generate (all instances)
    TMimeDirMode aMimeMode // MIME mode (older or newer vXXX format compatibility)
  );
  // - generate single property (except for valuelist-type properties)
  sInt16 generateProperty(
    TMultiFieldItem &aItem,     // the item where data comes from
    string &aString,            // the string to add properties to
    const char *aPrefix,        // the prefix (property name)
    const TPropertyDefinition *aPropP, // the property to generate (single instance)
    sInt16 aBaseOffset,          // field ID offset to be used
    sInt16 aRepeatOffset,        // additional repeat offset / array index
    TMimeDirMode aMimeMode,     // MIME mode (older or newer vXXX format compatibility)
    bool aSuppressEmpty,        // if set, a property with only empty values will not be generated
    TPropNameExtension *aPropNameExt=NULL // propname extension for generating musthave param values and maxrep/repinc for valuelists
  );
  // - generate parameters for one property instance
  //   returns true if parameters with shownonempty=true were generated
  bool generateParams(
    TMultiFieldItem &aItem, // the item where data comes from
    string &aString, // the string to add parameters to
    const TPropertyDefinition *aPropP, // the property to generate (all instances)
    TMimeDirMode aMimeMode, // MIME mode (older or newer vXXX format compatibility)
    sInt16 aBaseOffset,
    sInt16 aRepOffset,
    TPropNameExtension *aPropNameExt, // propname extension for generating musthave param values
    sInt32 &aNumNonSpcs // how many non-spaces are already in the value
  );
  // - recursively generate levels
  void generateLevels(
    TMultiFieldItem &aItem,
    string &aString,
    const TProfileDefinition *aProfileP
  );
  // - parse parameter or property value(list), returns false if no value(list)
  bool parseValue(
    const string &aText,        // string to parse as value (could be binary content)
    const TConversionDef *aConvDefP,
    sInt16 aBaseOffset,         // base offset
    sInt16 aRepOffset,          // repeat offset, adds to aBaseOffset for non-array fields, is array index for array fileds
    TMultiFieldItem &aItem,     // the item where data goes to
    bool &aNotEmpty,            // is set true (but never set false) if property contained any (non-positional) values
    char aSeparator,            // separator between values that consist of a list of enums etc. (more common for params than for values)
    TMimeDirMode aMimeMode,     // MIME mode (older or newer vXXX format compatibility)
    bool aParamValue,           // set if parsing parameter value (different escaping rules)
    bool aStructured,           // set if value consists of multiple values (has semicolon content escaping)
    bool aOnlyDeEscLF           // set if de-escaping only for \n -> LF, but all visible char escapes should be left intact
  );
  // - parse given property
  bool parseProperty(
    cAppCharP &aText, // where to start interpreting property, will be updated past end of poperty
    TMultiFieldItem &aItem, // item to store data into
    const TPropertyDefinition *aPropP, // the property definition
    sInt16 *aRepArray,  // array[repeatID], holding current repetition COUNT for a certain nameExts entry
    sInt16 aRepArraySize, // size of array (for security)
    TMimeDirMode aMimeMode, // MIME mode (older or newer vXXX format compatibility)
    cAppCharP aGroupName, // property group ("a" in "a.TEL:131723612")
    size_t aGroupNameLen,
    cAppCharP aFullPropName, // entire property name (excluding group) - might be needed in case of wildcard property match
    size_t aFullNameLen
  );
  // parse MIME-DIR level from specified string into item
  bool parseLevels(
    const char *&aText,
    TMultiFieldItem &aItem,
    const TProfileDefinition *aProfileP,
    bool aRootLevel
  );
#ifndef NO_REMOTE_RULES
  // helper for setRemoteRule(): add one specific remote rule and activate the behavior defined by it
  void activateRemoteRule(TRemoteRuleConfig *aRuleP);
#endif
}; // TMimeDirProfileHandler



// Utility functions
// -----------------


/// @brief checks two timestamps if they represent an all-day event
/// @param[in] aStart start time
/// @param[in] aEnd end time
/// @return 0 if not allday, x=1..n if allday (spanning x days) by one of the
///         following criteria:
///         - both start and end at midnight of the same day (= 1 day)
///         - both start and end at midnight of different days (= 1..n days)
///         - start at midnight and end between 23:59:00 and 23:59:59 of
///           same or different days (= 1..n days)
uInt16 AlldayCount(lineartime_t aStart, lineartime_t aEnd);

/// @brief checks two timestamps if they represent an all-day event
/// @param[in] aStartFldP start time field
/// @param[in] aEndFldP end time field
/// @param[in] aTimecontext context to use to check allday criteria for all non-floating timestamps
///            or UTC timestamps only (if aContextForUTC is set).
/// @param[in] aContextForUTC if set, context is only applied for UTC timestamps, other non-floatings are checked as-is
/// @return 0 if not allday, x=1..n if allday (spanning x days)
uInt16 AlldayCount(TItemField *aStartFldP, TItemField *aEndFldP, timecontext_t aTimecontext, bool aContextForUTC);


/// @brief makes two timestamps represent an all-day event
/// @param[in/out] aStart start time within the first day, will be set to midnight (00:00:00)
/// @param[in/out] aEnd end time within the last day or at midnight of the next day,
///                will be set to midnight of the next day
/// @param[in]     aDays if>0, this is used to calculate the aEnd timestamp (aEnd input is
///                ignored then)
void MakeAllday(lineartime_t &aStart, lineartime_t &aEnd, sInt16 aDays=0);

/// @brief makes two timestamp fields represent an all-day event
/// @param[in/out] aStartFldP start time within the first day, will be set to dateonly
/// @param[in/out] aEndFldP end time within the last day or at midnight of the next day, will be set to dateonly of the next day
/// @param[in] aTimecontext context to calculate day boundaries in
/// @param[in] aDays if>0, this is used to calculate the aEnd timestamp (aEnd input is
///            ignored then)
/// @note fields will be made floating and dateonly
void MakeAllday(TItemField *aStartFldP, TItemField *aEndFldP, timecontext_t aTimecontext, sInt16 aDays=0);



} // namespace sysync

#endif  // MimeDirProfile_H

// eof

