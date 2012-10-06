/*
 *  File:         mimedirprofile.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TMimeDirItemType
 *    base class for MIME DIR based content types (vCard, vCalendar...)
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2009-01-09 : luz : created from mimediritemtype.cpp
 *
 */

// includes
#include "prefix_file.h"
#include "sysync.h"
#include "vtimezone.h"
#include "rrules.h"

#include "mimedirprofile.h"
#include "mimediritemtype.h"

#include "syncagent.h"

#include <ctype.h>

using namespace sysync;


namespace sysync {


// mime-DIR mode names
const char * const mimeModeNames[numMimeModes] = {
  "old",
  "standard"
};


// VTIMEZONE generation modes
const char * const VTimeZoneGenModes[numVTimeZoneGenModes] = {
  "current",
  "start",
  "end",
  "range",
  "openend"
};


// VTIMEZONE generation modes
const char * const VTzIdGenModes[numTzIdGenModes] = {
  "default",
  "olson"
};


// enumeration modes
const char * const EnumModeNames[numEnumModes] = {
  "translate",        // translation from value to name and vice versa
  "prefix",           // translation of prefix while copying rest of string
  "defaultname",      // default name when translating from value to name
  "defaultvalue",     // default value when translating from name to value
  "ignore"            // ignore value or name
};


// profile modes
const char * const ProfileModeNames[numProfileModes] = {
  "custom",        // custom profile
  "vtimezones",    // VTIMEZONE profile(s), expands to a VTIMEZONE for every time zone referenced by convmode TZID fields
};


// Config
// ======

#pragma exceptions off

// Type registry

TMIMEProfileConfig::TMIMEProfileConfig(const char* aName, TConfigElement *aParentElement) :
  TProfileConfig(aName,aParentElement)
{
  fRootProfileP=NULL; // no profile yet
  clear();
} // TMIMEProfileConfig::TMIMEProfileConfig


TMIMEProfileConfig::~TMIMEProfileConfig()
{
  clear();
} // TMIMEProfileConfig::~TMIMEProfileConfig


// init defaults
void TMIMEProfileConfig::clear(void)
{
  // init defaults
  if (fRootProfileP) {
    delete fRootProfileP;
    fRootProfileP=NULL; // no profile any more
  }
  // init options
  fUnfloatFloating = false;
  fVTimeZoneGenMode = vtzgen_current; // show VTIMEZONE valid for current year (i.e. not dependent on record's time stamps)
  fTzIdGenMode = tzidgen_default; // default TZIDs
  // reset group building mechanism
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  fLastProperty = NULL;
  fPropertyGroupID = 1; // start at 1 (0=no group)
  #endif
  // clear inherited
  inherited::clear();
} // TMIMEProfileConfig::clear

#pragma exceptions reset


// handler factory
TProfileHandler *TMIMEProfileConfig::newProfileHandler(TMultiFieldItemType *aItemTypeP)
{
  // check if fieldlists match as they should
  if (aItemTypeP->getFieldDefinitions()!=fFieldListP) {
    // profile is for another field list, cannot be used for this item type
    return NULL;
  }
  // our handler is the text profile handler
  return (TProfileHandler *)(new TMimeDirProfileHandler(this,aItemTypeP));
}


#ifdef CONFIGURABLE_TYPE_SUPPORT


// get conversion mode, virtual, can be overridden by derivates
bool TMIMEProfileConfig::getConvMode(cAppCharP aText, sInt16 &aConvMode)
{
  // separate options
  size_t n=0; // no size
  cAppCharP op = strchr(aText, '+');
  if (op) n = op-aText;
  // check basic modes
  if (op && n==0) {
    // only options, no basic mode
    aConvMode = CONVMODE_NONE;
  }
  else {
    // first item is basic mode
    if (strucmp(aText,"none",n)==0)
      aConvMode = CONVMODE_NONE;
    else if (strucmp(aText,"version",n)==0)
      aConvMode = CONVMODE_VERSION;
    else if (strucmp(aText,"prodid",n)==0)
      aConvMode = CONVMODE_PRODID;
    else if (strucmp(aText,"timestamp",n)==0)
      aConvMode = CONVMODE_TIMESTAMP;
    else if (strucmp(aText,"date",n)==0)
      aConvMode = CONVMODE_DATE;
    else if (strucmp(aText,"autodate",n)==0)
      aConvMode = CONVMODE_AUTODATE;
    else if (strucmp(aText,"autoenddate",n)==0)
      aConvMode = CONVMODE_AUTOENDDATE;
    else if (strucmp(aText,"tz",n)==0)
      aConvMode = CONVMODE_TZ;
    else if (strucmp(aText,"daylight",n)==0)
      aConvMode = CONVMODE_DAYLIGHT;
    else if (strucmp(aText,"tzid",n)==0)
      aConvMode = CONVMODE_TZID;
    else if (strucmp(aText,"emptyonly",n)==0)
      aConvMode = CONVMODE_EMPTYONLY;
    else if (strucmp(aText,"bitmap",n)==0)
      aConvMode = CONVMODE_BITMAP;
    else if (strucmp(aText,"multimix",n)==0)
      aConvMode = CONVMODE_MULTIMIX;
    else if (strucmp(aText,"blob_b64",n)==0)
      aConvMode = CONVMODE_BLOB_B64;
    else if (strucmp(aText,"blob_auto",n)==0)
      aConvMode = CONVMODE_BLOB_AUTO;
    else if (strucmp(aText,"mailto",n)==0)
      aConvMode = CONVMODE_MAILTO;
    else if (strucmp(aText,"valuetype",n)==0)
      aConvMode = CONVMODE_VALUETYPE;
    else if (strucmp(aText,"fullvaluetype",n)==0)
      aConvMode = CONVMODE_FULLVALUETYPE;
    else if (strucmp(aText,"rrule",n)==0)
      aConvMode = CONVMODE_RRULE;
    else {
      fail("'conversion' value '%s' is invalid",aText);
      return false;
    }
  }
  // now check for options flags and or them into conversion value
  while (op) {
    aText = op+1; // skip +
    n = 0;
    op = strchr(aText, '+');
    if (op) n = op-aText;
    if (strucmp(aText,"extfmt",n)==0)
      aConvMode |= CONVMODE_FLAG_EXTFMT;
    else if (strucmp(aText,"millisec",n)==0)
      aConvMode |= CONVMODE_FLAG_MILLISEC;
    else {
      fail("'conversion' option '%s' is invalid",aText);
      return false;
    }
  }
  return true;
} // TMIMEProfileConfig::getConvMode




// private helper
bool TMIMEProfileConfig::getConvAttrs(const char **aAttributes, sInt16 &aFid, sInt16 &aConvMode, char &aCombSep)
{
  // - get options
  const char *fnam = getAttr(aAttributes,"field");
  if (fnam && *fnam!=0) {
    // has field spec
    // - find field ID
    aFid = fFieldListP->fieldIndex(fnam);
    if (aFid==VARIDX_UNDEFINED) {
      fail("'field' '%s' does not exist in field list '%s'",fnam,fFieldListP->getName());
      return false;
    }
  }
  // - get conversion mode
  const char *conv = getAttr(aAttributes,"conversion");
  if (conv) {
    if (!getConvMode(conv,aConvMode)) return false;
  }
  // - get combination char
  const char *comb = getAttr(aAttributes,"combine");
  if (comb) {
    if (strucmp(comb,"no")==0)
      aCombSep=0;
    else if (strucmp(comb,"lines")==0)
      aCombSep='\n';
    else if (strlen(comb)==1)
      aCombSep=*comb;
    else {
      fail("'combine' value '%s' is invalid",comb);
      return false;
    }
  }
  return true; // ok
} // TMIMEProfileConfig::getConvAttrs


bool TMIMEProfileConfig::getMask(const char **aAttributes, const char *aName, TParameterDefinition *aParamP, TNameExtIDMap &aMask)
{
  const char *m=getAttr(aAttributes,aName);
  if (m) {
    while (*m) {
      // skip comma separators and spaces
      if (*m==',' || *m<=0x20) { m++; continue; }
      // decode substring
      TParameterDefinition *paramP = aParamP; // default param
      size_t n=0;
      while(m[n]>0x20 && m[n]!=',') {
        if (m[n]=='.') {
          // qualified enum, search param
          paramP=fOpenProperty->findParameter(m,n);
          if (!paramP) {
            fail("Unknown param '%s' referenced in '%s'",m,aName);
            return false;
          }
          m+=n+1; // set start to enum name
          n=0; // start anew
          continue; // prevent increment
        }
        n++;
      }
      if (!paramP) {
        fail("Enum value must be qualified with parameter name: '%s'",m);
        return false;
      }
      TNameExtIDMap msk = paramP->getExtIDbit(m,n);
      if (msk==0) {
        fail("'%s' is not an enum of parameter '%s'",m,paramP->paramname.c_str());
        return false;
      }
      aMask = aMask | msk;
      m+=n; // advance pointer
    }
  }
  return true; // ok;
} // TMIMEProfileConfig::getMask


bool TMIMEProfileConfig::processPosition(TParameterDefinition *aParamP, const char **aAttributes)
{
  // <position has="TYPE.HOME" hasnot="FAX,CELL" shows="VOICE"
  //  field="TEL_HOME" repeat="4" increment="1" minshow="0"/>
  // - get maps
  TNameExtIDMap hasmap=0;
  TNameExtIDMap hasnotmap=0;
  TNameExtIDMap showsmap=0;
  if (!getMask(aAttributes,"has",aParamP,hasmap)) return false; // failed
  if (!getMask(aAttributes,"hasnot",aParamP,hasnotmap)) return false; // failed
  if (!getMask(aAttributes,"shows",aParamP,showsmap)) return false; // failed
  // - get field
  sInt16 fid=FID_NOT_SUPPORTED;
  const char *fnam = getAttr(aAttributes,"field");
  if (fnam) {
    fid = fFieldListP->fieldIndex(fnam);
    if (fid==VARIDX_UNDEFINED)
      return !fail("'field' '%s' does not exist in field list '%s'",fnam,fFieldListP->getName());
  }
  // - calculate offset from first specified value field in property
  sInt16 fidoffs=FID_NOT_SUPPORTED;
  if (fid>=0) {
    for (sInt16 k=0; k<fOpenProperty->numValues; k++) {
      fidoffs=fOpenProperty->convdefs[k].fieldid;
      if (fidoffs>=0) break; // found field offset
    }
    if (fidoffs<0)
      return !fail("property '%s' does not have any field assigned, cannot use 'position'",fOpenProperty->propname.c_str());
    // calc now
    fidoffs=fid-fidoffs;
  }
  // - get repeat and increment
  sInt16 repeat=1; // no repeat, no rewrite, but check other <position>s when property occurs again.
  sInt16 incr=1; // inc by 1
  sInt16 minshow=-1; // auto mode, same as repeat
  bool overwriteempty=true; // do not store empty repetitions
  bool readonly=false; // not just a parsing alternative
  sInt16 sharecountoffs=0; // no repeat counter sharing
  // - check special maxrepeat values
  const char *repval = getAttr(aAttributes,"repeat");
  if (repval) {
    if (strucmp(repval,"rewrite")==0) repeat=REP_REWRITE;
    #ifdef ARRAYFIELD_SUPPORT
    else if (strucmp(repval,"array")==0) repeat=REP_ARRAY;
    #endif
    else if (!StrToShort(repval,repeat))
      return !fail("expected number, 'rewrite' or 'array' in 'repeat'");
  }
  // - increment and minshow
  if (
    !getAttrShort(aAttributes,"increment",incr,true) ||
    !getAttrShort(aAttributes,"minshow",minshow,true) ||
    !getAttrShort(aAttributes,"sharepreviouscount",sharecountoffs,true)
  )
    return !fail("number expected in 'increment', 'minshow' and 'sharepreviouscount'");
  // - overwrite empty
  if (!getAttrBool(aAttributes,"overwriteempty",overwriteempty,true))
    return !fail("expected boolean value in 'overwriteempty'");
  // - read only position
  if (!getAttrBool(aAttributes,"readonly",readonly,true))
    return !fail("expected boolean value in 'readonly'");
  // - create name extension (position)
  fOpenProperty->addNameExt(
    fRootProfileP,hasmap,hasnotmap,showsmap,fidoffs,
    repeat,incr,minshow,overwriteempty,readonly,sharecountoffs
  );
  expectEmpty(); // no contents (and not a separte nest level)
  return true; // ok
} // TMIMEProfileConfig::processPosition


// called at end of nested parsing level
void TMIMEProfileConfig::nestedElementEnd(void)
{
  // - change mode
  if (fOpenConvDef) fOpenConvDef=NULL; // done with value spec
  else if (fOpenParameter) fOpenParameter=NULL; // done with paramater
  else if (fOpenProperty) fOpenProperty=NULL; // done with property
  else if (fOpenProfile) {
    fOpenProfile=fOpenProfile->parentProfile; // back to parent profile (or NULL if root)
    // groups do not span profiles
    fLastProperty=NULL;
  }
} // TMIMEProfileConfig::nestedElementEnd


// config element parsing
bool TMIMEProfileConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  sInt16 nummand;
  const char *nam;
  const char *val;
  const char *fnam;
  sInt16 fid;
  sInt16 convmode;
  char combsep;

  // MIME profile. This is multi-level and therefore needs
  // complicated parser.
  if (fNest==0) {
    // reset to root level
    fOpenProfile=NULL;
    fOpenProperty=NULL;
    fOpenParameter=NULL;
    fOpenConvDef=NULL;
  }
  // - parse generics
  // - get MIME-DIR type dependency
  TMimeDirMode modeDep = numMimeModes; // no mode dependency by default
  sInt16 m;
  cAppCharP modeDepName = getAttr(aAttributes,"onlyformode");
  if (modeDepName) {
    if (!StrToEnum(mimeModeNames,numMimeModes,m,modeDepName))
      return fail("unknown 'onlyformode' attribute value '%s'",modeDepName);
    else
      modeDep=(TMimeDirMode)m;
  }
  // - now parse specifics
  if (fOpenConvDef) {
    if (strucmp(aElementName,"enum")==0) {
      // <enum name="nam" value="val" mode="translate" positional="yes">
      // - get name and value
      nam = getAttr(aAttributes,"name");
      val = getAttr(aAttributes,"value");
      // - get mode
      TEnumMode mode=enm_translate; // default to translate
      const char *mod=getAttr(aAttributes,"mode");
      if (mod) {
        if (!StrToEnum(EnumModeNames,numEnumModes,m,mod))
          return fail("unknown 'mode' '%s'",mod);
        else
          mode=(TEnumMode)m;
      }
      // - get options
      bool positional=fOpenParameter && fOpenParameter->extendsname; // default to parameter
      if (!getAttrBool(aAttributes,"positional",positional,true))
        return fail("bad boolean value for 'positional'");
      if (fOpenParameter && positional && !fOpenParameter->extendsname)
        return fail("'parameter' must have set 'positional' to use positional 'enum's");
      // check logic
      if (mode!=enm_default_value && mode!=enm_ignore && (!nam || *nam==0))
        return fail("non-default/non-positional 'enum' must have 'name' attribute");
      // 1:1 translation shortcut: if no value specified, use name as value
      if (mode==enm_translate && !val)
        val=nam; // use name as value
      // default name and ignore can have no value (prefix can have empty value)
      if (!positional && mode!=enm_default_name && mode!=enm_ignore && (!val || (*val==0 && mode!=enm_prefix)))
        return fail("non-default 'enum' must have (possibly non-empty) 'value' attribute");
      // - create enum
      if (positional)
        fOpenConvDef->addEnumNameExt(fOpenProperty, nam,val,mode);
      else
        fOpenConvDef->addEnum(nam,val,mode);
      expectEmpty(); // no contents (and not a separate nest level)
    }
    // none known here
    else
      return false; // parent is TConfigElement, no need to call inherited
  }
  else if (fOpenParameter) {
    if (strucmp(aElementName,"value")==0) {
      // <value field="N_FIRST" conversion="none" combine="no"/>
      // - set default options
      fid=FID_NOT_SUPPORTED;
      convmode = CONVMODE_NONE;
      combsep = 0;
      // - get other options of convdef
      if (!getConvAttrs(aAttributes,fid,convmode,combsep)) return true; // failed
      // - set convdef
      fOpenConvDef = fOpenParameter->setConvDef(fid,convmode,combsep);
      startNestedParsing();
    }
    else if (strucmp(aElementName,"position")==0) {
      // Position within parameter, enums reference this parameter without
      // explicitly qualified enum names. To reference enums of other params,
      // qualified names are allowed.
      // <position has="TYPE.HOME" hasnot="FAX,CELL" shows="VOICE" field="TEL_HOME" repeat="4" increment="1"/>
      if (!processPosition(fOpenParameter,aAttributes))
        return true; // failed
    }
    // none known here
    else
      return inherited::localStartElement(aElementName, aAttributes, aLine); // call inherited
  }
  else if (fOpenProperty) {
    if (strucmp(aElementName,"value")==0) {
      // <value index="1" field="N_FIRST" conversion="none" combine="no"/>
      // - get index
      sInt16 idx=0;
      if (!getAttrShort(aAttributes,"index",idx,fOpenProperty->numValues==1)) // optional only for 1-value properties (or lists)
        return fail("'index' missing or with invalid value");
      if (idx>=fOpenProperty->numValues)
        return fail("'index' out of range (0..%hd)",fOpenProperty->numValues);
      // - set default options
      fid=FID_NOT_SUPPORTED;
      convmode = CONVMODE_NONE;
      combsep = 0;
      // - get other options of convdef
      if (!getConvAttrs(aAttributes,fid,convmode,combsep))
        return true; // failed
      // - set convdef
      fOpenConvDef = fOpenProperty->setConvDef(idx,fid,convmode,combsep);
      startNestedParsing();
    }
    else if (strucmp(aElementName,"parameter")==0) {
      // <parameter name="TYPE" default="yes" positional="yes">
      // - get name
      nam = getAttr(aAttributes,"name");
      if (!nam || *nam==0)
        return fail("'parameter' must have 'name' attribute");
      // - get options
      bool positional=false;
      bool defparam=false;
      bool shownonempty=false; // don't show properties that have only param values, but no main value
      bool showinctcap=false; // don't show parameter in CTCap by default
      if (
        !getAttrBool(aAttributes,"positional",positional,true) ||
        !getAttrBool(aAttributes,"default",defparam,true) ||
        !getAttrBool(aAttributes,"shownonempty",shownonempty,true) ||
        !getAttrBool(aAttributes,"show",showinctcap,true) ||
        !getAttrBool(aAttributes,"showindevinf",showinctcap,true) // synonymous with "show" for parameters (note that "show" on properties is no longer effective on purpose!)
      )
        return fail("bad boolean value");
      // - add parameter
      fOpenParameter = fOpenProperty->addParam(nam,defparam,positional,shownonempty,showinctcap,modeDep);
      #ifndef NO_REMOTE_RULES
      const char *depRuleName = getAttr(aAttributes,"rule");
      TCFG_ASSIGN(fOpenParameter->dependencyRuleName,depRuleName); // save name for later resolving
      #endif
      startNestedParsing();
    }
    else if (strucmp(aElementName,"position")==0) {
      // Position outside parameter, enums must reference parameters using
      // explicitly qualified enum names like "TYPE.HOME"
      // <position has="TYPE.HOME" hasnot="TYPE.FAX,TYPE.CELL" shows="TYPE.VOICE" field="TEL_HOME" repeat="4" increment="1"/>
      if (!processPosition(NULL,aAttributes))
        return true; // failed
    }
    // none known here
    else
      return inherited::localStartElement(aElementName, aAttributes, aLine); // call inherited
  }
  else if (fOpenProfile) {
    if (strucmp(aElementName,"subprofile")==0) {
      // <subprofile name="VTODO" nummandatory="1" field="KIND" value="TODO" showlevel="yes" showprops="yes">
      // <subprofile name="VTODO" nummandatory="1" useproperties="VEVENT" field="KIND" value="TODO" showlevel="yes" showprops="yes">
      // - starting a new subprofile starts a new property group anyway
      fLastProperty=NULL;
      // - get name
      nam = getAttr(aAttributes,"name");
      if (!nam || *nam==0)
        return fail("'subprofile' must have 'name' attribute");
      // - get profile mode
      TProfileModes mode = profm_custom;
      cAppCharP pfmode = getAttr(aAttributes,"mode");
      if (pfmode) {
        if (!StrToEnum(ProfileModeNames,numProfileModes,m,pfmode))
          return fail("unknown profile 'mode' '%s'",pfmode);
        else
          mode=(TProfileModes)m;
      }
      // - check mode dependent params
      TProfileDefinition *profileP = NULL; // no foreign properties by default
      if (mode==profm_custom) {
        // Custom profile
        // - get number of mandatory properties
        if (!getAttrShort(aAttributes,"nummandatory",nummand,false))
          return fail ("missing or bad 'nummandatory' specification");
        // - check if using properties of other profile
        const char *use = getAttr(aAttributes,"useproperties");
        if (use) {
          profileP = fRootProfileP->findProfile(use);
          if (!profileP)
            return fail("unknown profile '%s' specified in 'useproperties'",use);
          expectEmpty(true); // subprofile is a nest level, so we need to flag that (otherwise, nestedElementEnd() would not get called)
        }
        else {
          // parsing nested elements in this TConfigElement
          startNestedParsing();
        }
      }
      else {
        // non-custom profiles are expected to be empty
        expectEmpty(true); // subprofile is a nest level, so we need to flag that (otherwise, nestedElementEnd() would not get called)
      }
      // - get DevInf visibility options
      bool showifselectedonly = false; // default: show anyway
      if (!getAttrBool(aAttributes,"showifselectedonly",showifselectedonly,true))
        return fail("bad boolean value for showifselectedonly");
      // - create subprofile now
      fOpenProfile = fOpenProfile->addSubProfile(nam,nummand,showifselectedonly,mode,modeDep);
      // - add properties of other level if any
      if (profileP) fOpenProfile->usePropertiesOf(profileP);
      // - add level control field stuff, if any
      fnam = getAttr(aAttributes,"field");
      if (fnam) {
        // - "value" is optional, without a value subprofile is activated if field is non-empty
        val = getAttr(aAttributes,"value");
        // - find field
        fid = fFieldListP->fieldIndex(fnam);
        if (fid==VARIDX_UNDEFINED)
          return fail("'field' '%s' does not exist in field list '%s'",fnam,fFieldListP->getName());
        // - set level control convdef
        TConversionDef *cdP = fOpenProfile->setConvDef(fid); // set field ID of level control field
        if (val)
          cdP->addEnum("",val,enm_translate); // set value to be set into level control field when level is entered
      }
    }
    else if (strucmp(aElementName,"property")==0) {
      // <property name="VERSION" rule="other" values="1" mandatory="no" show="yes" suppressempty="no" delayedparsing="0">
      // - get name
      nam = getAttr(aAttributes,"name");
      if (!nam || *nam==0)
        return fail("'property' must have 'name' attribute");
      // - check grouping
      if (!fLastProperty || strucmp(TCFG_CSTR(fLastProperty->propname),nam)!=0) {
        // first property in group
        fPropertyGroupID++; // new group ID
      }
      #ifndef NO_REMOTE_RULES
      // - get rule dependency
      bool isRuleDep=false;
      const char *depRuleName = getAttr(aAttributes,"rule");
      if (depRuleName) {
        isRuleDep=true;
        if (strucmp(depRuleName,"other")==0) {
          // "other" rule (property is active if no other property from the group gets active)
          depRuleName=NULL;
        }
      }
      #endif
      // - get number of values
      sInt16 numval=1; // default to 1
      const char *nvs = getAttr(aAttributes,"values");
      if (nvs) {
        if (strucmp(nvs,"list")==0) numval=NUMVAL_LIST;
        else if (strucmp(nvs,"expandedlist")==0) numval=NUMVAL_REP_LIST;
        else if (!StrToShort(nvs,numval))
          return fail("invalid value in 'values' attribute");
      }
      // - get options
      bool mandatory = false;
      bool showprop = true; // show property in devInf by default
      bool suppressempty = false;
      bool allowFoldAtSep = false;
      bool canfilter = false; // 3.2.0.9 onwards: do not show filter caps by default (devInf gets too large)
      if (
        !getAttrBool(aAttributes,"mandatory",mandatory,true) ||
        !getAttrBool(aAttributes,"showindevinf",showprop,true) || // formerly just called "show" (but renamed to make it ineffective in old configs as new engine prevents duplicates automatically)
        !getAttrBool(aAttributes,"suppressempty",suppressempty,true) ||
        !getAttrBool(aAttributes,"filter",canfilter,true) ||
        !getAttrBool(aAttributes,"foldbetween",allowFoldAtSep,true)
      ) return fail("bad boolean value");
      const char *valsep= getAttr(aAttributes,"valueseparator");
      if (!valsep) valsep=";"; // default to semicolon if not defined
      const char *altvalsep= getAttr(aAttributes,"altvalueseparator");
      if (!altvalsep) altvalsep=""; // default to none if not defined
      // - group field ID
      sInt16 groupFieldID = FID_NOT_SUPPORTED; // no group field ID by default
      cAppCharP gfin = getAttr(aAttributes, "groupfield");
      if (gfin) {
        groupFieldID = fFieldListP->fieldIndex(gfin);
        if (groupFieldID==VARIDX_UNDEFINED) {
          fail("'groupfield' '%s' does not exist in field list '%s'",gfin,fFieldListP->getName());
          return false;
        }
      }
      // - delayed processing
      sInt16 delayedprocessing=0; // default to 0
      if (!getAttrShort(aAttributes,"delayedparsing",delayedprocessing,true))
        return fail ("bad 'delayedparsing' specification");
      // - create property now and open new level of parsing
      fOpenProperty=fOpenProfile->addProperty(nam, numval, mandatory, showprop, suppressempty, delayedprocessing, *valsep, fPropertyGroupID, canfilter, modeDep, *altvalsep, groupFieldID, allowFoldAtSep);
      fLastProperty=fOpenProperty; // for group checking
      #ifndef NO_REMOTE_RULES
      // - add rule dependency (pointer will be resolved later)
      fOpenProperty->dependsOnRemoterule=isRuleDep;
      fOpenProperty->ruleDependency=NULL; // not known yet
      TCFG_ASSIGN(fOpenProperty->dependencyRuleName,depRuleName); // save name for later resolving
      #endif
      startNestedParsing();
    }
    // none known here
    else
      return inherited::localStartElement(aElementName, aAttributes, aLine); // call inherited
  }
  else {
    if (strucmp(aElementName,"profile")==0) {
      // <profile name="VCARD" nummandatory="2">
      if (fRootProfileP)
        return fail("'profile' cannot be defined more than once");
      // new profile starts new property group
      fLastProperty=NULL;
      // get name
      nam = getAttr(aAttributes,"name");
      if (!nam || *nam==0)
        return fail("'profile' must have 'name' attribute");
      // - get number of mandatory properties
      if (!getAttrShort(aAttributes,"nummandatory",nummand,false))
        return fail ("missing or bad 'nummandatory' specification");
      // create root profile
      fRootProfileP = new TProfileDefinition(NULL,nam,nummand,false,profm_custom,numMimeModes); // root needs no selection to be shown, is always a custom profile, and not mode dependent
      // parsing nested elements in this TConfigElement
      fOpenProfile=fRootProfileP; // current open profile
      startNestedParsing();
    }
    else if (strucmp(aElementName,"unfloattimestamps")==0)
      expectBool(fUnfloatFloating);
    else if (strucmp(aElementName,"vtimezonegenmode")==0)
      expectEnum(sizeof(fVTimeZoneGenMode),&fVTimeZoneGenMode,VTimeZoneGenModes,numVTimeZoneGenModes);
    else if (strucmp(aElementName,"tzidgenmode")==0)
      expectEnum(sizeof(fTzIdGenMode),&fTzIdGenMode,VTzIdGenModes,numTzIdGenModes);
    // none known here
    else
      return inherited::localStartElement(aElementName, aAttributes, aLine);
  }
  // ok
  return true;
} // TMIMEProfileConfig::localStartElement



#ifndef NO_REMOTE_RULES
// resolve remote rule dependencies in profile (recursive)
static void resolveRemoteRuleDeps(TProfileDefinition *aProfileP, TAgentConfig *aSessionConfigP)
{
  TProfileDefinition *profileP = aProfileP;
  while (profileP) {
    // resolve properties
    TPropertyDefinition *propP = profileP->propertyDefs;
    while (propP) {
      // check for rule-dependent props
      if (propP->dependsOnRemoterule) {
        propP->ruleDependency=NULL; // assume the "other" rule entry
        if (!TCFG_ISEMPTY(propP->dependencyRuleName)) {
          // find remote rule
          TRemoteRulesList::iterator pos;
          for(pos=aSessionConfigP->fRemoteRulesList.begin();pos!=aSessionConfigP->fRemoteRulesList.end();pos++) {
            if (strucmp(TCFG_CSTR(propP->dependencyRuleName),(*pos)->getName())==0) {
              // found rule by name
              propP->ruleDependency=(*pos);
              break;
            }
          }
          if (propP->ruleDependency==NULL) {
            string s;
            StringObjPrintf(s,"property '%s' depends on unknown rule '%s'",TCFG_CSTR(propP->propname),TCFG_CSTR(propP->dependencyRuleName));
            SYSYNC_THROW(TConfigParseException(s.c_str()));
          }
        } // rule specified
      }

      // also fix rule-dependent parameters
      TParameterDefinition *paramP = propP->parameterDefs;
      while (paramP) {
        if (!TCFG_ISEMPTY(paramP->dependencyRuleName)) {
          TRemoteRulesList::iterator pos;
          for(pos=aSessionConfigP->fRemoteRulesList.begin();pos!=aSessionConfigP->fRemoteRulesList.end();pos++) {
            if (strucmp(TCFG_CSTR(paramP->dependencyRuleName),(*pos)->getName())==0) {
              paramP->ruleDependency=(*pos);
              break;
            }
          }
          if (paramP->ruleDependency==NULL) {
            string s;
            StringObjPrintf(s,"parameter '%s' in property '%s' depends on unknown rule '%s'",
                            TCFG_CSTR(paramP->paramname),
                            TCFG_CSTR(propP->propname),
                            TCFG_CSTR(propP->dependencyRuleName));
            SYSYNC_THROW(TConfigParseException(s.c_str()));
          }
        }
        paramP = paramP->next;
      }

      // next
      propP=propP->next;
    }
    // resolve subprofiles
    resolveRemoteRuleDeps(profileP->subLevels,aSessionConfigP);
    // next
    profileP=profileP->next;
  }
} // resolveRemoteRuleDeps
#endif

// resolve
void TMIMEProfileConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    // check for required settings
    if (!fRootProfileP)
      SYSYNC_THROW(TConfigParseException("empty 'mimeprofile' not allowed"));
    #ifndef NO_REMOTE_RULES
    // recursively resolve remote rule dependencies in all properties
    resolveRemoteRuleDeps(
      fRootProfileP,
      static_cast<TAgentConfig *>(static_cast<TRootConfig *>(getRootElement())->fAgentConfigP)
    );
    #endif
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TMIMEProfileConfig::localResolve

#endif // CONFIGURABLE_TYPE_SUPPORT






// implementation of MIME-DIR info classes

#pragma exceptions off
#define EXCEPTIONS_HERE 0


TEnumerationDef::TEnumerationDef(const char *aEnumName, const char *aEnumVal, TEnumMode aMode, sInt16 aNameExtID)
{
  next=NULL;
  TCFG_ASSIGN(enumtext,aEnumName);
  TCFG_ASSIGN(enumval,aEnumVal);
  enummode=aMode;
  nameextid=aNameExtID;
} // TEnumerationDef::TEnumerationDef


TEnumerationDef::~TEnumerationDef()
{
  // make sure entire chain gets deleted
  if (next) delete next;
} // TEnumerationDef::~TEnumerationDef



TConversionDef::TConversionDef()
{
  fieldid=FID_NOT_SUPPORTED;
  enumdefs=NULL;
  convmode=0;
  combineSep=0;
} // TConversionDef::TConversionDef


TConversionDef::~TConversionDef()
{
  // make sure enum list gets deleted
  if (enumdefs) delete enumdefs;
} // TEnumerationDef::~TEnumerationDef


TConversionDef *TConversionDef::setConvDef(
  sInt16 aFieldId,
  sInt16 aConvMode,
  char aCombSep
)
{
  fieldid=aFieldId;
  convmode=aConvMode;
  combineSep=aCombSep;
  return this;
} // TConversionDef::setConvDef


const TEnumerationDef *TConversionDef::findEnumByName(const char *aName, sInt16 n)
const
{
  TEnumerationDef *enumP = enumdefs;
  TEnumerationDef *defaultenumP = NULL;
  while(enumP) {
    // check plain match
    if (
      (enumP->enummode==enm_translate || enumP->enummode==enm_ignore) &&
      strucmp(aName,TCFG_CSTR(enumP->enumtext),n)==0
    ) break; // found full match
    // check prefix match
    else if (
      enumP->enummode==enm_prefix &&
      (TCFG_SIZE(enumP->enumtext)==0 || strucmp(aName,TCFG_CSTR(enumP->enumtext),TCFG_SIZE(enumP->enumtext))==0)
    ) break; // found prefix match (or prefix entry with no text, which means match as well)
    // otherwise: remember if this is a default
    else if (enumP->enummode==enm_default_value) {
      // default value entry
      defaultenumP=enumP; // anyway: remember default value entry
      // allow searching default value by name (for "has","hasnot" parsing via getExtIDbit())
      if (!(TCFG_ISEMPTY(enumP->enumtext)) && strucmp(aName,TCFG_CSTR(enumP->enumtext),n)==0)
        break; // found named default value
    }
    // check next
    enumP=enumP->next;
  }
  return enumP ? enumP : defaultenumP;
} // TConversionDef::findEnumByName


const TEnumerationDef *TConversionDef::findEnumByVal(const char *aVal, sInt16 n)
const
{
  TEnumerationDef *enumP = enumdefs;
  TEnumerationDef *defaultenumP = NULL;
  while(enumP) {
    // check full match
    if (
      (enumP->enummode==enm_translate || enumP->enummode==enm_ignore) &&
      strucmp(aVal,TCFG_CSTR(enumP->enumval),n)==0
    ) break; // found
    // check prefix match
    else if (
      enumP->enummode==enm_prefix &&
      (TCFG_SIZE(enumP->enumval)==0 || strucmp(aVal,TCFG_CSTR(enumP->enumval),TCFG_SIZE(enumP->enumval))==0)
    ) break; // found prefix match (or prefix entry with no value, which means match as well)
    // remember if this is a default
    else if (enumP->enummode == enm_default_name) defaultenumP=enumP; // remember default
    // check next
    enumP=enumP->next;
  }
  return enumP ? enumP : defaultenumP;
} // TConversionDef::findEnumByVal


void TConversionDef::addEnum(const char *aEnumName, const char *aEnumVal, TEnumMode aMode)
{
  TEnumerationDef **enumPP = &enumdefs;
  while(*enumPP!=NULL) enumPP=&((*enumPP)->next); // find last in chain
  *enumPP = new TEnumerationDef(aEnumName,aEnumVal,aMode); // w/o name extension
} // TConversionDef::addEnum



// add enum for name extension, auto-creates property-unique name extension ID
void TConversionDef::addEnumNameExt(TPropertyDefinition *aProp, const char *aEnumName, const char *aEnumVal, TEnumMode aMode)
{
  TEnumerationDef **enumPP = &enumdefs;
  while(*enumPP!=NULL) enumPP=&((*enumPP)->next); // find last in chain
  if (aProp->nextNameExt>31)
    #if EXCEPTIONS_HERE
    SYSYNC_THROW(TSyncException(DEBUGTEXT("more than 32 name extensions","mdit3")));
    #else
    return; // silently ignore
    #endif
  *enumPP = new TEnumerationDef(aEnumName,aEnumVal, aMode, aProp->nextNameExt++);
} // TConversionDef::addEnumNameExt


TParameterDefinition::TParameterDefinition(
  const char *aName, bool aDefault, bool aExtendsName, bool aShowNonEmpty, bool aShowInCTCap, TMimeDirMode aModeDep
) {
  next=NULL;
  TCFG_ASSIGN(paramname,aName);
  defaultparam=aDefault;
  extendsname=aExtendsName;
  shownonempty=aShowNonEmpty;
  showInCTCap=aShowInCTCap;
  modeDependency=aModeDep;
  #ifndef NO_REMOTE_RULES
  ruleDependency=NULL;
  TCFG_CLEAR(dependencyRuleName);
  #endif
} // TParameterDefinition::TParameterDefinition


TParameterDefinition::~TParameterDefinition()
{
  if (next) delete next;
} // TParameterDefinition::~TParameterDefinition


TNameExtIDMap TParameterDefinition::getExtIDbit(const char *aEnumName, sInt16 n)
{
  const TEnumerationDef *enumP=convdef.findEnumByName(aEnumName,n);
  if (enumP) {
    return ((TNameExtIDMap)1<<enumP->nameextid);
  }
  return 0;
} // TParameterDefinition::getExtIDbit


TPropNameExtension::TPropNameExtension(
  TNameExtIDMap aMusthave_ids, TNameExtIDMap aForbidden_ids, TNameExtIDMap aAddtlSend_ids,
  sInt16 aFieldidoffs, sInt16 aMaxRepeat, sInt16 aRepeatInc, sInt16 aMinShow,
  bool aOverwriteEmpty, bool aReadOnly, sInt16 aRepeatID
) {
  next=NULL;
  musthave_ids=aMusthave_ids;
  forbidden_ids=aForbidden_ids;
  addtlSend_ids=aAddtlSend_ids;
  fieldidoffs=aFieldidoffs;
  maxRepeat=aMaxRepeat;
  repeatInc=aRepeatInc;
  minShow=aMinShow;
  overwriteEmpty=aOverwriteEmpty;
  readOnly=aReadOnly;
  repeatID=aRepeatID;
} // TPropNameExtension::TPropNameExtension


TPropNameExtension::~TPropNameExtension()
{
  if (next) delete next;
} // TPropNameExtension::~TPropNameExtension


TPropertyDefinition::TPropertyDefinition(const char* aName, sInt16 aNumVals, bool aMandatory, bool aShowInCTCap, bool aSuppressEmpty, uInt16 aDelayedProcessing, char aValuesep, char aAltValuesep, uInt16 aPropertyGroupID, bool aCanFilter, TMimeDirMode aModeDep, sInt16 aGroupFieldID, bool aAllowFoldAtSep)
{
  next = NULL;
  TCFG_ASSIGN(propname,aName);
  nameExts = NULL; // none yet
  nextNameExt = 0; // no enums with name extensions defined yet for this property
  valuelist = false; // no value list by default
  expandlist = false; // not expanding value list into repeating property by default
  valuesep = aValuesep; // separator for structured-value and value-list properties
  altvaluesep = aAltValuesep; // alternate separator for structured-value and value-list properties (for parsing only)
  allowFoldAtSep = aAllowFoldAtSep; // allow folding at value separators even if it inserts a space at the end of the previous value
  groupFieldID = aGroupFieldID; // fid for field that contains the group tag (prefix to the property name, like "a" in "a.TEL:079122327")
  propGroup = aPropertyGroupID; // property group ID
  // check if this is an unprocessed wildcard property
  unprocessed = strchr(aName, '*')!=NULL;
  // check value list
  if (aNumVals==NUMVAL_LIST || aNumVals==NUMVAL_REP_LIST) {
    // value list
    valuelist = true;
    expandlist = aNumVals==NUMVAL_REP_LIST;
    numValues = 1; // we accept a single convdef only
  }
  else {
    // individual values
    numValues = aNumVals;
  }
  // create convdefs array
  convdefs = new TConversionDef[numValues];
  parameterDefs = NULL; // none yet
  mandatory = aMandatory;
  showInCTCap = aShowInCTCap;
  canFilter = aCanFilter;
  suppressEmpty = aSuppressEmpty;
  delayedProcessing = aDelayedProcessing;
  modeDependency = aModeDep;
  #ifndef NO_REMOTE_RULES
  // not dependent on rule yet (as rules do not exists at TPropertyDefinition creation,
  // dependency will be added later, if any)
  dependsOnRemoterule = false;
  ruleDependency = NULL;
  #endif
} // TPropertyDefinition::TPropertyDefinition


TPropertyDefinition::~TPropertyDefinition()
{
  // delete name extensions
  if (nameExts) delete nameExts;
  // delete convdefs array
  if (convdefs) delete [] convdefs;
  // delete parameter definitions
  if (parameterDefs) delete parameterDefs;
  // delete rest of chain
  if (next) delete next;
} // TPropertyDefinition::~TPropertyDefinition


TConversionDef *TPropertyDefinition::setConvDef(sInt16 aValNum, sInt16 aFieldId,sInt16 aConvMode,char aCombSep)
{
  if (aValNum<0 || aValNum>=numValues)
    #if EXCEPTIONS_HERE
    SYSYNC_THROW(TSyncException(DEBUGTEXT("setConvDef for Property with bad value number","mdit4")));
    #else
    return NULL; // silently ignore
    #endif
  return convdefs[aValNum].setConvDef(aFieldId,aConvMode,aCombSep);
}; // TPropertyDefinition::TConversionDef


void TPropertyDefinition::addNameExt(TProfileDefinition *aRootProfile, // for profile-global RepID generation
  TNameExtIDMap aMusthave_ids, TNameExtIDMap aForbidden_ids, TNameExtIDMap aAddtlSend_ids,
  sInt16 aFieldidoffs, sInt16 aMaxRepeat, sInt16 aRepeatInc, sInt16 aMinShow,
  bool aOverwriteEmpty, bool aReadOnly, sInt16 aShareCountOffs
)
{
  TPropNameExtension **namextPP = &nameExts;
  while(*namextPP!=NULL) namextPP=&((*namextPP)->next); // find last in chain
  if (aMinShow<0) {
    if (aMaxRepeat==REP_ARRAY)
      aMinShow=0; // by default, show nothing if array is empty
    else
      aMinShow=aMaxRepeat; // auto mode, show all repetitions
  }
  *namextPP = new TPropNameExtension(
    aMusthave_ids,aForbidden_ids,aAddtlSend_ids,aFieldidoffs,
    aMaxRepeat,aRepeatInc,aMinShow,aOverwriteEmpty,aReadOnly,
    // readOnly alternative parsing <position> might want to share the
    // repeat count with previous <position> occurrences
    aShareCountOffs ? aRootProfile->nextRepID-aShareCountOffs : aRootProfile->nextRepID++
  );
} // TPropertyDefinition::addNameExt


TParameterDefinition *TPropertyDefinition::addParam(
  const char *aName, bool aDefault, bool aExtendsName, bool aShowNonEmpty, bool aShowInCTCap, TMimeDirMode aModeDep
)
{
  TParameterDefinition **paramPP = &parameterDefs;
  while(*paramPP!=NULL) paramPP=&((*paramPP)->next); // find last in chain
  *paramPP = new TParameterDefinition(aName,aDefault,aExtendsName,aShowNonEmpty,aShowInCTCap, aModeDep);
  return *paramPP;
} // TPropertyDefinition::addParam


// find parameter by name
TParameterDefinition *TPropertyDefinition::findParameter(const char *aNam, sInt16 aLen)
{
  TParameterDefinition *paramP = parameterDefs;
  while (paramP) {
    if (strucmp(aNam,TCFG_CSTR(paramP->paramname),aLen)==0)
      return paramP; // found
    paramP=paramP->next; // next
  }
  // not found
  return NULL;
} // TPropertyDefinition::findParameter


TProfileDefinition::TProfileDefinition(
  TProfileDefinition *aParentProfileP, // parent profile
  const char *aProfileName, // name
  sInt16 aNumMandatory,
  bool aShowInCTCapIfSelectedOnly,
  TProfileModes aProfileMode,
  TMimeDirMode aModeDep
)
{
  parentProfile=aParentProfileP; // NULL if root
  next=NULL;
  // set fields
  TCFG_ASSIGN(levelName,aProfileName);
  shownIfSelectedOnly = aShowInCTCapIfSelectedOnly;
  profileMode = aProfileMode;
  modeDependency = aModeDep;
  // init
  numMandatoryProperties=aNumMandatory;
  propertyDefs=NULL;
  subLevels=NULL;
  ownsProps=true;
  nextRepID=0;
} // TProfileDefinition::TProfileDefinition


TProfileDefinition::~TProfileDefinition()
{
  if (propertyDefs && ownsProps) delete propertyDefs;
  if (subLevels) delete subLevels;
  if (next) delete next;
} // TProfileDefinition::~TProfileDefinition


TProfileDefinition *TProfileDefinition::addSubProfile(
  const char *aProfileName, // name
  sInt16 aNumMandatory,
  bool aShowInCTCapIfSelectedOnly,
  TProfileModes aProfileMode,
  TMimeDirMode aModeDep
)
{
  TProfileDefinition **profilePP=&subLevels;
  while (*profilePP!=NULL) profilePP=&((*profilePP)->next);
  *profilePP=new TProfileDefinition(this,aProfileName,aNumMandatory,aShowInCTCapIfSelectedOnly,aProfileMode,aModeDep);
  return *profilePP;
} // TProfileDefinition::addSubProfile


TPropertyDefinition *TProfileDefinition::addProperty(
  const char *aName, // name
  sInt16 aNumValues, // number of values
  bool aMandatory, // mandatory
  bool aShowInCTCap, // show in CTCap
  bool aSuppressEmpty, // suppress empty ones on send
  uInt16 aDelayedProcessing, // delayed processing when parsed, 0=immediate processing, 1..n=delayed
  char aValuesep, // value separator
  uInt16 aPropertyGroupID, // property group ID (alternatives for same-named properties should have same ID>0)
  bool aCanFilter, // can be filtered -> show in filter cap
  TMimeDirMode aModeDep, // property valid only for specific MIME mode
  char aAltValuesep, // alternate separator (for parsing)
  sInt16 aGroupFieldID, // group field ID
  bool aAllowFoldAtSep // allow folding at separators
)
{
  TPropertyDefinition **propPP=&propertyDefs;
  while (*propPP!=NULL) propPP=&((*propPP)->next);
  *propPP=new TPropertyDefinition(aName,aNumValues,aMandatory,aShowInCTCap,aSuppressEmpty,aDelayedProcessing,aValuesep,aAltValuesep,aPropertyGroupID,aCanFilter,aModeDep,aGroupFieldID,aAllowFoldAtSep);
  // return new property
  return *propPP;
} // TProfileDefinition::addProperty


void TProfileDefinition::usePropertiesOf(TProfileDefinition *aProfile)
{
  ownsProps=false;
  propertyDefs=aProfile->propertyDefs;
} // TProfileDefinition::usePropertiesOf


// find (sub)profile by name, recursively
TProfileDefinition *TProfileDefinition::findProfile(const char *aNam)
{
  // check myself
  if (levelName==aNam) return this;
  // check sublevels
  TProfileDefinition *lvlP = subLevels;
  TProfileDefinition *foundlvlP;
  while(lvlP) {
    foundlvlP=lvlP->findProfile(aNam);
    if (foundlvlP) return foundlvlP;
    lvlP=lvlP->next;
  }
  // does not match myself nor one of my sublevels
  return NULL;
} // TProfileDefinition::findProfile

#pragma exceptions reset
#undef EXCEPTIONS_HERE
#define EXCEPTIONS_HERE TARGET_HAS_EXCEPTIONS


#ifdef OBJECT_FILTERING

// get property definition of given filter expression identifier.
TPropertyDefinition *TProfileDefinition::getPropertyDef(const char *aPropName)
{
  TPropertyDefinition *propP = NULL;

  if (!aPropName) return propP; // no name, no fid
  // Depth first: search in subprofiles, if any
  TProfileDefinition *profileP = subLevels;
  while (profileP) {
    // search depth first
    if ((propP=profileP->getPropertyDef(aPropName))!=NULL)
      return propP; // found
    // test next profile
    profileP=profileP->next;
  }
  // now search my own properties
  propP = propertyDefs;
  while (propP) {
    // compare names
    if (strucmp(aPropName,TCFG_CSTR(propP->propname))==0) {
      return propP;
    }
    // test next property
    propP=propP->next;
  }
  // not found
  return NULL;
} // TProfileDefinition::getPropertyDef


// get field index of given filter expression identifier.
sInt16 TProfileDefinition::getPropertyMainFid(const char *aPropName, uInt16 aIndex)
{
  sInt16 fid = VARIDX_UNDEFINED;

  // search property definition with matching name
  TPropertyDefinition *propP = getPropertyDef(aPropName);
  // search for first value with a field assigned
  if (propP) {
    // found property with matching name
    if (propP->convdefs) {
      if (aIndex==0) {
        // no index specified -> search first with a valid FID
        for (uInt16 i=0; i<propP->numValues; i++) {
          if ((fid=propP->convdefs[i].fieldid)!=VARIDX_UNDEFINED)
            return fid; // found a field index
        }
      }
      else {
        // index specified for multivalued properties -> return specified value's ID
        if (aIndex<=propP->numValues) {
          return propP->convdefs[aIndex-1].fieldid;
        }
      }
    }
  }
  // not found
  return VARIDX_UNDEFINED;
} // TProfileDefinition::getPropertyMainFid


#endif // OBJECT_FILTERING



/*
 * Implementation of TMimeDirProfileHandler
 */


TMimeDirProfileHandler::TMimeDirProfileHandler(
  TMIMEProfileConfig *aMIMEProfileCfgP,
  TMultiFieldItemType *aItemTypeP
) : TProfileHandler(aMIMEProfileCfgP, aItemTypeP)
{
  // save profile config pointer
  fProfileCfgP = aMIMEProfileCfgP;
  fProfileDefinitionP = fProfileCfgP->fRootProfileP;
  // settable options defaults
  fMimeDirMode=mimo_standard;
  fReceiverCanHandleUTC = true;
  fVCal10EnddatesSameDay = false; // avoid 23:59:59 style end date by default
  fReceiverTimeContext = TCTX_UNKNOWN; // none in particular
  fDontSendEmptyProperties = false; // send all defined properties
  fDefaultOutCharset = chs_utf8; // standard
  fDefaultInCharset = chs_utf8; // standard
  fDoQuote8BitContent = false; // no quoting needed per se
  fDoNotFoldContent = false; // standard requires folding
  fTreatRemoteTimeAsLocal = false; // only for broken implementations
  fTreatRemoteTimeAsUTC = false; // only for broken implementations
  fActiveRemoteRules.clear(); // no dependency on certain remote rules
} // TMimeDirProfileHandler::TMimeDirProfileHandler


TMimeDirProfileHandler::~TMimeDirProfileHandler()
{
  // nop for now
} // TMimeDirProfileHandler::~TTextProfileHandler



#ifdef OBJECT_FILTERING

// get field index of given filter expression identifier.
sInt16 TMimeDirProfileHandler::getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex)
{
  // search properties for field index
  return fProfileDefinitionP->getPropertyMainFid(aIdentifier, aIndex);
} // TMimeDirProfileHandler::getFilterIdentifierFieldIndex

#endif // OBJECT_FILTERING



// parses enum value for CONVMODE_MULTIMIX
// [offs.](Bx|Lzzzzzzz)
// aN returns the bit number or the offset of the zzzzz literal within aMixVal, depending on aIsBitMap
static bool mixvalparse(cAppCharP aMixVal, uInt16 &aOffs, bool &aIsBitMap, uInt16 &aN)
{
  aOffs = 0;
  cAppCharP p = aMixVal;
  // check offset (2 digit max)
  if (isdigit(*p)) {
    p+=StrToUShort(p,aOffs,2);
    if (*p++ != '.') return false; // wrong syntax
  }
  // check command
  if (*p == 'B') {
    // bit number
    aIsBitMap = true;
    if (StrToUShort(p+1,aN,2)<1) return false; // wrong syntax
  }
  else if (*p=='L') {
    // literal, return position within string
    aIsBitMap = false;
    aN = p+1-aMixVal; // literal starts at this position
  }
  else
    return false; // unknown command
  return true;
} // mixvalparse



// returns the size of the field block (how many fids in sequence) related
// to a given convdef (for multi-field conversion modes such as CONVMODE_RRULE
sInt16 TMimeDirProfileHandler::fieldBlockSize(const TConversionDef &aConvDef)
{
  if ((aConvDef.convmode & CONVMODE_MASK)==CONVMODE_RRULE)
    return 6; // RRULE fieldblock: DTSTART,FREQ,INTERVAL,FIRSTMASK,LASTMASK,UNTIL = 6 fields
  else
    return 1; // single field
} // TMimeDirProfileHandler::fieldBlockSize



// special field translation (to be extended in derived classes)
// Note: the string returned by this function will be scanned as a
//   value list if combinesep is set, and every single value will be
//   enum-translated if enums defined.
bool TMimeDirProfileHandler::fieldToMIMEString(
  TMultiFieldItem &aItem,           // the item where data comes from
  sInt16 aFid,                       // the field ID (can be NULL for special conversion modes)
  sInt16 aArrIndex,                  // the repeat offset to handle array fields
  const TConversionDef *aConvDefP,  // the conversion definition record
  string &aString                   // output string
)
{
  const int maxmix = 10;
  uInt16 mixOffs[maxmix];
  bool mixIsFlags[maxmix];
  TEnumerationDef *enumP;
  uInt16 offs; bool isFlags;
  int nummix, i;
  fieldinteger_t flags;
  uInt16 bitNo;
  TTimestampField *tsFldP;
  TIntegerField *ifP;
  TStringField *sfP;
  timecontext_t tctx;
  lineartime_t ts;
  string s;
  // RRULE field block values
  char freq; // frequency
  char freqmod; // frequency modifier
  sInt16 interval; // interval
  fieldinteger_t firstmask; // day mask counted from the first day of the period
  fieldinteger_t lastmask; // day mask counted from the last day of the period
  lineartime_t until; // last day
  timecontext_t untilcontext;


  // get pointer to leaf field
  TItemField *fldP = aItem.getArrayField(aFid,aArrIndex,true); // existing array elements only

  bool dateonly = false; // assume timestamp mode
  bool autodate = true; // show date-only values automatically as date-only, even if stored in a timestamp field
  bool extFmt = (aConvDefP->convmode & CONVMODE_FLAG_EXTFMT)!=0;
  bool milliSec = (aConvDefP->convmode & CONVMODE_FLAG_MILLISEC)!=0;
  sInt16 convmode = aConvDefP->convmode & CONVMODE_MASK;
  switch (convmode) {
    // no special mode
    case CONVMODE_NONE:
    case CONVMODE_EMPTYONLY:
      // just get field as string
      if (!fldP) return false; // no field, no value
      if (!fldP->isBasedOn(fty_timestamp)) goto normal;
      // Based on timestamp
      // - handle date-only specially
      if (fldP->getType()==fty_date)
        goto dateonly; // date-only
      else
        goto timestamp; // others are treated as timestamps
    // date & time modes
    case CONVMODE_DATE: // always show as date
    dateonly:
      dateonly = true; // render as date in all cases
      goto timestamp;
    case CONVMODE_AUTOENDDATE:
    case CONVMODE_AUTODATE: // show date-only as date in iCal 2.0 (mimo_standard), but always as timestamp for vCal 1.0 (mimo_old)
      if (fMimeDirMode==mimo_standard) goto timestamp; // use autodate if MIME-DIR format is not vCal 1.0 style
      // for vCal 1.0 style, always renders as timestamp (as date-only values are not allowed there)
    case CONVMODE_TIMESTAMP: // always show as timestamp
      // get explictly as timestamp (even if field or field contents is date)
      autodate = false; // do not show as date, even if it is a date-only
    timestamp:
      if (!fldP) return false; // no field, no value
      if (!fldP->isBasedOn(fty_timestamp)) goto normal;
      // show as timestamp
      tsFldP = static_cast<TTimestampField *>(fldP);
      tctx = tsFldP->getTimeContext();
      // check for auto-date
      if (autodate) {
        if (TCTX_IS_DATEONLY(tctx))
          dateonly=true;
      }
      // check for special cases
      if (TCTX_IS_DURATION(tctx)) {
        // duration is shown as such
        tsFldP->getAsISO8601(aString, TCTX_UNKNOWN | TCTX_DURATION, false, false, extFmt, milliSec);
      }
      else if (dateonly) {
        // date-only are either floating or shown as date-only part of original timestamp
        tsFldP->getAsISO8601(aString, TCTX_UNKNOWN | TCTX_DATEONLY, false, false, extFmt, milliSec);
      }
      else if (fReceiverCanHandleUTC && !tsFldP->isFloating()) {
        // remote can handle UTC and the timestamp is not floating
        if (!TCTX_IS_UNKNOWN(fPropTZIDtctx)) {
          // if we have rendered a TZID for this property, this means that apparently the remote
          // supports TZID (otherwise the field would not be marked available in the devInf).
          // - show it as floating, explicitly with both date AND time (both flags set)
          tsFldP->getAsISO8601(aString, TCTX_UNKNOWN | TCTX_TIMEONLY | TCTX_DATEONLY, false, false, extFmt, milliSec);
        }
        else {
          // - show it as UTC
          tsFldP->getAsISO8601(aString, TCTX_UTC, true, false, extFmt, milliSec);
        }
      }
      else {
        // remote cannot handle UTC or time is floating (possibly dateonly or duration)
        if (tsFldP->isFloating()) {
          // floating, show as-is
          ts = tsFldP->getTimestampAs(TCTX_UNKNOWN);
          if (ts==noLinearTime)
            aString.erase();
          else {
            if (TCTX_IS_DATEONLY(tctx)) {
              // value is a date-only, but we must render it a datetime
              ts=lineartime2dateonlyTime(ts); // make time part 0:00:00
            }
            // first check for auto-end-date (which must be floating)
            // Note: we don't get here with a date only mimo_standard because it will be catched above, so test is not really needed
            if (convmode==CONVMODE_AUTOENDDATE && fVCal10EnddatesSameDay && TCTX_IS_DATEONLY(tctx) && fMimeDirMode==mimo_old)
              ts-=1; // subtract one unit to make end show last time unit of previous day
            // now show as floating ISO8601
            TimestampToISO8601Str(aString, ts, TCTX_UNKNOWN, extFmt, milliSec);
          }
        }
        else {
          // not floating (=not a enddateonly), but we can't send UTC - render as localtime
          // in item time zone (which defaults to session time zone)
          tsFldP->getAsISO8601(aString, fItemTimeContext, false, false, extFmt, milliSec);
        }
      }
      return true; // found
    normal:
      // simply as string
      fldP->getAsString(aString);
      return true; // found
    case CONVMODE_TZ:
    case CONVMODE_TZID:
    case CONVMODE_DAYLIGHT:
      // use now as default point in time for possible offset calculations
      ts = getSession()->getSystemNowAs(TCTX_SYSTEM);
      // if no field is specified, the item context is used (which defaults to
      // the session's user context)
      // Note that testing fldP is not enough, because an empty array will also cause fldP==NULL
      if (!fldP) {
        if (aFid!=FID_NOT_SUPPORTED)
          return false; // field not available (but conversion definition DOES refer to a field --> no time zone)
        // conversion definition does not refer to a field: use item context
        tctx = fItemTimeContext;
      }
      else if (fldP->isBasedOn(fty_timestamp)) {
        // time zone of a timestamp
        tsFldP = static_cast<TTimestampField *>(fldP);
        // - if floating time, we have no time zone
        if (tsFldP->isFloating() || tsFldP->isDuration()) return false; // floating or duration -> no time zone
        // - get context
        tctx = tsFldP->getTimeContext(); // get the context
        // - get the value
        ts = tsFldP->getTimestampAs(TCTX_UNKNOWN);
        // prevent generating TZID (and associated VTIMEZONES later) for empty timestamp
        if (ts==noLinearTime) return false; // no timestamp -> no time zone
      }
      else if (fldP->getCalcType()==fty_integer) {
        // integer field is simply a time zone offset in minutes
        tctx = TCTX_MINOFFSET(fldP->getAsInteger());
      }
      else if (!fldP->isEmpty()) {
        // string field can be timezone name or numeric minute offset
        fldP->getAsString(s);
        if (!TimeZoneNameToContext(s.c_str(),tctx,getSessionZones())) {
          // if not recognized as time zone name, use integer value
          tctx = TCTX_MINOFFSET(fldP->getAsInteger());
        }
      }
      else
        return false; // no TZ to show
      // if remote cannot handle UTC (i.e. only understands localtime), then make sure
      // the time zone shown is the general item zone (user zone).
      if (!fReceiverCanHandleUTC) {
        TzConvertTimestamp(ts,tctx,fItemTimeContext,getSessionZones());
        tctx = fItemTimeContext; // use item zone
      }
      // now render context as selected
      if (convmode==CONVMODE_TZID) {
        // time zone ID for iCal 2.0 TZID parameter
        // - make sure meta context is resolved (we don't want "SYSTEM" as TZID!)
        if (!TzResolveMetaContext(tctx, getSessionZones())) return false; // cannot resolve, no time zone ID
        // - if time zone is not UTC (which is represented as "Z" and needs no TZID), show name
        if (!TCTX_IS_UTC(tctx) && !TCTX_IS_UNKNOWN(tctx) && !TCTX_IS_DATEONLY(tctx)) {
          // - show name of zone as TZID
          if (!TimeZoneContextToName(tctx, aString, getSessionZones(), fProfileCfgP->fTzIdGenMode==tzidgen_olson ? "o" : NULL)) return false; // cannot get name/ID
          // - flag property-level TZID generated now
          fPropTZIDtctx=tctx;
          // - add to set of TZID-referenced time zones (for vTimezone generation)
          fUsedTCtxSet.insert(fUsedTCtxSet.end(),tctx);
          // - update range of time covered for generating VTIMEZONE later
          if (ts) {
            if (fEarliestTZDate==noLinearTime || fEarliestTZDate>ts) fEarliestTZDate = ts; // new minimum
            if (fLatestTZDate==noLinearTime || fLatestTZDate<ts) fLatestTZDate = ts; // new maximum
          }
        }
      }
      else {
        // CONVMODE_TZ or CONVMODE_DAYLIGHT
        // - there's only one TZ/DAYLIGHT per item, so set it as item context
        if (!fReceiverCanHandleUTC) {
          // devices that can't handle UTC should not be bothered with TZ info
          // (e.g. for N-Gage/3650 presence of a TZ shifts the data by the TZ value!?)
          return false; // prevent generation of TZ or DAYLIGHT props
        }
        else {
          // only if remote can handle UTC we may change the item time context
          // (otherwise, the timestamp must be rendered in itemzone/userzone)
          fItemTimeContext = tctx;
          fHasExplicitTZ = true; // flag setting explicit time zone for item
        }
        // - get resolved TZ offset and DAYLIGHT string for vCal 1.0
        ContextToTzDaylight(tctx,ts,s,tctx,getSessionZones());
        if (convmode==CONVMODE_TZ) {
          // time zone in +/-hh[:mm] format for vCal 1.0 TZ property
          // - render offset in extended format
          aString.erase();
          // - return true only if we actually have a TZ
          return ContextToISO8601StrAppend(aString, tctx, true);
        }
        else if (convmode==CONVMODE_DAYLIGHT) {
          // TZ and DAYLIGHT property for vCal 1.0
          aString = s;
          // - return true only if we actually have a DAYLIGHT
          return !s.empty();
        }
      }
      // done
      return true;
    case CONVMODE_MAILTO:
      // make sure we have a mailto: prefix (but not if string is empty)
      if (!fldP) return false; // no field, no value
      fldP->getAsString(s);
      aString.erase();
      if (strucmp(s.c_str(),"mailto:",7)!=0 && s.size()>0)
        aString="mailto:";
      aString+=s;
      return true;
    case CONVMODE_VALUETYPE:
    case CONVMODE_FULLVALUETYPE:
      // specify value type of field if needed
      if (!fldP) return false; // no field -> no VALUE param
      if (fldP->isBasedOn(fty_timestamp)) {
        // show VALUE=DATE if we have date-only or time-only
        tctx = static_cast<TTimestampField *>(fldP)->getTimeContext();
        if (TCTX_IS_DURATION(tctx)) aString="DURATION";
        else if (TCTX_IS_DATEONLY(tctx)) aString="DATE";
        else if (TCTX_IS_TIMEONLY(tctx)) aString="TIME";
        else {
          // only show type if full value type requested
          if (convmode==CONVMODE_FULLVALUETYPE)
            aString="DATE-TIME";
          else
            return false; // we don't need a VALUE param for normal datetimes
        }
      }
      else
        return false; // no field type that needs VALUE param
      // valuetype generated
      return true;
    case CONVMODE_VERSION:
      // version string
      aString=aItem.getItemType()->getTypeVers(fProfileMode);
      return true;
    case CONVMODE_PRODID:
      // PRODID ISO9070 non-registered FPI
      // -//ABC Corporation//NONSGML My Product//EN
      aString = SYSYNC_FPI;
      return true;
    case CONVMODE_BITMAP:
      // bitmap is a special case of multimix, set up params
      nummix = 1;
      mixOffs[0]=0;
      mixIsFlags[0]=true;
      goto genmix;
    case CONVMODE_MULTIMIX:
      // list of special values that can be either literals or bit masks, and can optionally affect more than one field
      // Syntax:
      //  Bx              : Bit number x (like in CONVMODE_BITMAP, x = 0..63)
      //  Lxxxx           : Literal xxxxx (xxxxx will just be copied from the source field)
      //  y.Bx or y.Lxxxx : use y as field offset to use (no y means 0 offset)
      // - collect parameters to generate mix from enums
      nummix = 0;
      enumP = aConvDefP->enumdefs;
      while(enumP) {
        if (mixvalparse(TCFG_CSTR(enumP->enumval),offs,isFlags,bitNo)) {
          // check if this field is in list already
          for (i=0; i<nummix; i++) {
            if (mixOffs[i] == offs) goto next; // referring to same field again, skip
          }
          // is a new field, add it to list
          mixOffs[nummix] = offs;
          mixIsFlags[nummix] = isFlags;
          nummix++;
          if (nummix>=maxmix) break; // no more mixes allowed, stop scanning
        }
      next:
        // check next enum
        enumP=enumP->next;
      }
    genmix:
      // now generate strings from collected data
      aString.erase();
      for (i=0; i<nummix; i++) {
        // get target field
        fldP = aItem.getArrayField(aFid+mixOffs[i],aArrIndex,true); // existing array elements only
        if (fldP) {
          if (mixIsFlags[i]) {
            // use target as bitmask to create bit numbers
            flags=fldP->getAsInteger();
            bitNo=0;
            while (flags) {
              if (flags & 1) {
                // create bit representation
                if (!aString.empty() && aConvDefP->combineSep)
                  aString+=aConvDefP->combineSep; // separator first if not first item
                if (convmode==CONVMODE_MULTIMIX) {
                  // multimix mode, use full syntax
                  if (mixOffs[i]>0)
                    StringObjAppendPrintf(aString,"%d.",mixOffs[i]);
                  aString += 'B';
                }
                // add bit number
                StringObjAppendPrintf(aString,"%hd",bitNo);
              }
              flags >>= 1; // consume this one
              bitNo++;
            }
          }
          else {
            // literal
            if (!fldP->isEmpty()) {
              if (!aString.empty() && aConvDefP->combineSep)
                aString+=aConvDefP->combineSep; // append separator if there are more flags
              if (mixOffs[i]>0)
                StringObjAppendPrintf(aString,"%d.",mixOffs[i]);
              aString += 'L'; // literal
              fldP->appendToString(aString);
            }
          }
        } // field available
      } // for each mix
      return true;
    case CONVMODE_RRULE: {
      // get values from field block
      if (aFid<0) return false; // no field, no string
      // - freq/freqmod
      if (!(sfP = ITEMFIELD_DYNAMIC_CAST_PTR(TStringField,fty_string,aItem.getArrayField(aFid,aArrIndex,true)))) return false;
      aFid++; // do NOT INCREMENT in macro, as it would get incremented twice
      sfP->getAsString(s);
      freq='0'; // none
      freqmod=' '; // no modifier
      if (s.size()>0) freq=s[0];
      if (s.size()>1) freqmod=s[1];
      // - interval
      if (!(ifP = ITEMFIELD_DYNAMIC_CAST_PTR(TIntegerField,fty_integer,aItem.getArrayField(aFid,aArrIndex,true)))) return false;
      aFid++; // do NOT INCREMENT in macro, as it would get incremented twice
      interval=(sInt16)ifP->getAsInteger();
      // - firstmask
      if (!(ifP = ITEMFIELD_DYNAMIC_CAST_PTR(TIntegerField,fty_integer,aItem.getArrayField(aFid,aArrIndex,true)))) return false;
      aFid++; // do NOT INCREMENT in macro, as it would get incremented twice
      firstmask=ifP->getAsInteger();
      // - lastmask
      if (!(ifP = ITEMFIELD_DYNAMIC_CAST_PTR(TIntegerField,fty_integer,aItem.getArrayField(aFid,aArrIndex,true)))) return false;
      aFid++; // do NOT INCREMENT in macro, as it would get incremented twice
      lastmask=ifP->getAsInteger();
      // - until
      if (!(tsFldP = ITEMFIELD_DYNAMIC_CAST_PTR(TTimestampField,fty_timestamp,aItem.getArrayField(aFid,aArrIndex,true)))) return false;
      aFid++; // do NOT INCREMENT in macro, as it would get incremented twice
      // Until
      // - UTC preferred as output format if basically possible and not actively disabled
      untilcontext=
        fReceiverCanHandleUTC && getSession()->canHandleUTC() ?
        TCTX_UTC :
        fItemTimeContext;
      // - get in preferred zone (or floating)
      until=tsFldP->getTimestampAs(untilcontext,&untilcontext);
      lineartime_t tzend = until;
      // A RRULE with no end extends at least into current time (for tz range update, see below)
      if (until==noLinearTime) {
        // no end, but we still need a range to generate time zones for
        tzend = getSession()->getSystemNowAs(TCTX_UTC);
      }
      else {
        // Treat RR_END similar to CONVMODE_AUTODATE, i.e. prevent rendering a date-only value in mimo_old (which is not correct according to the standard)
        if (TCTX_IS_DATEONLY(untilcontext) && fMimeDirMode==mimo_old) {
          // there are no date-only recurrence ends in vCalendar 1.0
          until = lineartime2dateonlyTime(until)+secondToLinearTimeFactor*SecsPerHour*24-1 ; // make time part 23:59:59.999 of this day
          untilcontext &= ~TCTX_DATEONLY; // clear dateonly rendering flag
        }
      }
      // Now do the conversion
      bool ok;
      if (fMimeDirMode==mimo_old) {
        // vCalendar 1.0 type RRULE
        ok = internalToRRULE1(
          aString,
          freq,
          freqmod,
          interval,
          firstmask,
          lastmask,
          until,
          untilcontext,
          GETDBGLOGGER
        );
      }
      else {
        // iCalendar 2.0 type RRULE
        ok = internalToRRULE2(
          aString,
          freq,
          freqmod,
          interval,
          firstmask,
          lastmask,
          until,
          untilcontext,
          GETDBGLOGGER
        );
      }
      // if we actually generated a RRULE, the range of used time zones must be updated according
      // to the recurrence end (date or open end, see tzend calculation above)
      if (!aString.empty()) {
        if (fEarliestTZDate==noLinearTime || tzend<fEarliestTZDate) fEarliestTZDate = tzend;
        if (fLatestTZDate==noLinearTime || tzend>fLatestTZDate) fLatestTZDate = tzend;
      }
      return ok;
      break; // just in case
    }
    default:
      // unknown mode, no value
      return false;
  }
  return false;
} // TMimeDirProfileHandler::fieldToMIMEString



/// @brief test if char is part of a line end
/// @return true if aChar is a line end char
/// @param [in] aChar charcter to check
static bool isLineEndChar(appChar aChar)
{
  return (aChar=='\x0D') || (aChar=='\x0A');
} // isLineEndChar


/// @brief test if char is end of a line or end of the text (NUL)
/// @return true if aChar is a line end char or NUL
/// @param [in] aChar charcter to check
static bool isEndOfLineOrText(appChar aChar)
{
  return (aChar==0) || isLineEndChar(aChar);
} // isEndOfLineOrText


/// @brief test if a line end of any kind is at aText
/// @note CR,LF,CRLF and CR...CRLF sequences are all considered one line end
/// @return true if line end found
/// @param [in/out] aText advance past line end sequence
static bool testAndSkipLineEnd(cAppCharP &aText)
{
  cAppCharP p = aText;
  bool crFound = false;
  // skip sequence of CRs
  while (*p=='\x0D') {
    p++;
    crFound = true;
  }
  // past all CRs in a row
  if (*p=='\x0A') {
    // independent of the number of CRs preceeding, this is a line end including the LF
    aText = p+1; // past LF
    return true;
  }
  else if (crFound) {
    // we previously found at least one CR at the beginning, but no LF is following
    // -> assume CR only line ends, consider first CR as a line end by itself
    aText++; // skip first CR
    return true;
  }
  // not a line end
  return false;
} // testAndSkipLineEnd



// return incremented pointer pointing to original char or next non-folded char
static cAppCharP skipfolded(cAppCharP aText, TMimeDirMode aMimeMode, bool qpSoftBreakCancel=false)
{
  cAppCharP p = aText;
  if (testAndSkipLineEnd(p)) {
    // check for folding sequence
    if (*p==' ' || *p=='\x09') {
      // line end followed by space: folding sequence
      if (aMimeMode==mimo_standard) {
        // ignore entire sequence (CR,LF,SPACE/TAB)
        return p+1;
      }
      else {
        // old folding type, LWSP must be preserved
        return p;
      }
    }
  }
  else if (qpSoftBreakCancel && *p=='=') {
    // could be soft break sequence, check for line end
    p++;
    if (testAndSkipLineEnd(p)) {
      return p;
    }
  }
  // not folding sequence, return ptr to char as is
  return aText;
} // skipfolded


// get next character, while skipping MIME-DIR folding sequences
// if qpSoftBreakCancel, QUOTED-PRINTABLE encoding style soft-line-break sequences
// will be eliminated
static const char *nextunfolded(const char *p, TMimeDirMode aMimeMode, bool qpSoftBreakCancel=false)
{
  if (*p==0) return p; // at end of string, do not advance
  p++; // point to next
  return skipfolded(p,aMimeMode,qpSoftBreakCancel);
} // nextunfolded


// helper for MIME DIR parsing:
// - apply encoding and charset conversion to values part of property if needed
static void decodeValue(
  TEncodingTypes aEncoding,    // the encoding to be used
  TCharSets aCharset,          // charset to be applied to 8-bit chars
  TMimeDirMode aMimeMode,      // the MIME mode
  char aStructSep,             // input is structured value, stop when aStructSep is encountered
  char aAltSep,                // alternate separator, also stop when encountering this one (but only if aStructSep is !=0)
  const char *&aText,          // where to start decoding, updated past last char added to aVal
  string &aVal                 // decoded data is stored here (possibly some binary data)
)
{
  const int maxseqlen=6;
  int seqlen;
  char c,chrs[maxseqlen];
  const char *p,*q;

  aVal.erase();
  bool escaped = false;
  bool lastWasQPCR = false;
  if (aEncoding==enc_quoted_printable) {
    // decode quoted-printable content
    p = skipfolded(aText,aMimeMode,true); // get unfolded start point (in case value starts with folding sequence)
    do {
      // decode standard content
      c=*p;
      if (isEndOfLineOrText(c) || (!escaped && aStructSep!=0 && (c==aStructSep || c==aAltSep))) break; // EOLN and struct separators terminate value
      // test if escape char (but do not filter it out, as actual de-escaping is done in parseValue() later
      escaped=(!escaped) && (c=='\\'); // escape next only if we are not escaped already
      // char found
      if (c=='=') {
        uInt16 code;
        const char *s;
        char hex[2];
        s=nextunfolded(p,aMimeMode,true);
        if (*s==0) break; // end of string
        hex[0]=*s; // first digit
        s=nextunfolded(s,aMimeMode,true);
        if (*s==0) break; // end of string
        hex[1]=*s; // second digit
        if (HexStrToUShort(hex,code,2)==2) {
          p=s; // continue with next char after second digit
          c=code; // decoded char
          if (c=='\x0D') {
            c='\n'; // convert to newline
            lastWasQPCR = true; // remember
          }
          else if (c=='\x0A') {
            if (lastWasQPCR) {
              // if last was CR, ignore LF (CR already has generated a newline)
              p = nextunfolded(p,aMimeMode,true); // but skip it for now.
              lastWasQPCR = false;
              continue; // ignore LF
            }
            // LF not preceeded by CR is a newline
            c='\n'; // convert to newline
          }
          else {
            // neither CR nor LF
            lastWasQPCR = false;
          }
        }
      }
      else {
        lastWasQPCR = false;
      }
      seqlen=1; // assume logical char consists of single byte
      chrs[0]=c;
      do {
        seqlen=appendCharsAsUTF8(chrs,aVal,aCharset,seqlen); // add char (possibly with UTF8 expansion) to aVal
        if (seqlen<=1) break; // done
        // need more bytes to encode entire char
        for (int i=1;i<seqlen;i++) {
          p=nextunfolded(p,aMimeMode,true);
          chrs[i]=*p;
        }
      } while(true);
      p=nextunfolded(p,aMimeMode,true);
    } while(true);
  } // quoted printable
  else if (aEncoding==enc_base64 || aEncoding==enc_b) {
    // Decode b64
    // - find start of property value
    p = skipfolded(aText,aMimeMode,false); // get unfolded start point (in case value starts with folding sequence
    // - find end of property value
    q=p;
    while (*q) {
      if (aStructSep!=0 && (*q==aStructSep || *q==aAltSep))
        break; // structure separator terminates B64 as well (colon, semicolon and comma never appear in B64)
      if (isLineEndChar(*q)) {
        // end of line. Check if this is folding or end of property
        cAppCharP r=skipfolded(q,aMimeMode,false);
        if (r==q) {
          // no folding skipped -> this appears to be the end of the property
          // Now for ill-encoded vCard 2.1 which chop B64 into lines, but do not prefix continuation
          // lines with some whitespace, make sure the next line contains a colon
          // - skip that line end
          while (isLineEndChar(*r)) r++;
          // - examine next line
          bool eob64 = false;
          for (cAppCharP r2=r; *r2 && !isLineEndChar(*r2); r2++) {
            if (*r2==':' || *r2==';') {
              eob64 = true;
              break;
            }
          }
          if (eob64) break; // q is end of B64 string -> go decode it
          // there's more to the b64 string at r, continue looking for end
        }
        // skip to continuation of B64 string
        q=r;
      }
      else
        q++;
    }
    // - decode base 64
    uInt32 binsz=0;
    uInt8 *binP = b64::decode(p, q-p, &binsz);
    aVal.append((const char *)binP,binsz);
    b64::free(binP);
    // - continue at next char after b64 value
    p=q;
  }
  else {
    // no (known) encoding
    p = skipfolded(aText,aMimeMode,false); // get unfolded start point (in case value starts with folding sequence)
    do {
      c=*p;
      if (isEndOfLineOrText(c) || (!escaped && aStructSep!=0 && (c==aStructSep || c==aAltSep))) break; // EOLN and structure-sep (usually ;) terminate value
      // test if escape char (but do not filter it out, as actual de-escaping is done in parseValue() later
      escaped=(!escaped) && (c=='\\'); // escape next only if we are not escaped already
      // process char
      seqlen=1; // assume logical char consists of single byte
      chrs[0]=c;
      do {
        seqlen=appendCharsAsUTF8(chrs,aVal,aCharset,seqlen); // add char (possibly with UTF8 expansion) to aVal
        if (seqlen<=1) break; // done
        // need more bytes to encode entire char
        for (int i=1;i<seqlen;i++) {
          p=nextunfolded(p,aMimeMode,false);
          chrs[i]=*p;
        }
      } while(true);
      p=nextunfolded(p,aMimeMode,false);
    } while(true);
  } // no encoding
  // return pointer to terminating char
  aText=p;
} // decodeValue


// helper for MIME DIR generation:
// - apply encoding to values part of property if needed
static void encodeValues(
  TEncodingTypes aEncoding,    // the encoding to be used
  TCharSets aCharSet,          // charset to be applied to 8-bit chars
  const string &aValuedata,    // the data to be encoded (possibly some binary data)
  string &aPropertytext,       // the property string where encoded data is appended
  bool aDoNotFoldContent       // special override for folding
)
{
  const uInt8 *valPtr = (const uInt8 *)aValuedata.c_str();
  size_t valSz = aValuedata.size();
  string s;
  if (aCharSet!=chs_utf8) {
    // we need to convert to target charset first
    appendUTF8ToString((const char *)valPtr,s,aCharSet,lem_none,qm_none);
    valPtr = (const uInt8 *)s.c_str();
    valSz = s.size();
  }
  // - apply encoding if needed
  appendEncoded(
    valPtr, // input
    valSz,
    aPropertytext, // append output here
    aEncoding, // desired encoding
    aDoNotFoldContent ?
    0                     // disable insertion of soft line breaks
    : MIME_MAXLINESIZE-1, // limit to standard MIME-linesize, leave one free for possible extra folding space
    aPropertytext.size() % MIME_MAXLINESIZE, // current line size
    true // insert CRs only for softbreaks (for post-processing by folding)
  );
} // encodeValues


// helper for MIME DIR generation:
// - fold, copy and terminate (CRLF) property into aString output
// - \n in input is explicit "fold here" indicator
// - \b in input is an optional "fold here" indicator, which will appear as space in the
//      output when needed, but will otherwise be discarded
// - \r in input indicates that a line end must be inserted
//      if aDoSoftBreak==true, only a line break is inserted (QUOTED-PRINTABLE soft line break)
//      otherwise, a full folding sequence (CRLF + space) is inserted. In case of MIME-DIR,
//      QP softbreaks are nothing special, and still need an extra space (as this is reversed on parsing).
static void finalizeProperty(
  const char *proptext,
  string &aString,
  TMimeDirMode aMimeMode, // MIME mode (older or newer vXXX format compatibility)
  bool aDoNotFold, // set to prevent folding
  bool aDoSoftBreak // set to insert QP-softbreaks when \r is encountered, otherwise do a full hard break (which essentially inserts a space for mimo_old)
)
{
  // make sure that allocation does not increase char by char
  aString.reserve(aString.size()+strlen(proptext)+100);
  char c;
  ssize_t n = 0, llen = 0;
  ssize_t foldLoc = -1; // possible break location - linear white space or explicit break indicator
  bool explf;
  cAppCharP firstunwritten=proptext; // none written yet
  while (proptext && (c=*proptext)!=0) {
    // remember position of last lwsp (space or TAB)
    if (aMimeMode==mimo_old && (c==' ' || c==0x09))
      foldLoc=n; // white spaces are relevant in mimo_old only (but '\b' are checked also for MIME-DIR, see below)
    // next (UTF8) char
    // Note: we prevent folding within UTF8 sequences as result string would become inconvertible e.g. into UTF16
    uInt32 uc;
    cAppCharP nP = UTF8toUCS4(proptext, uc);
    if (uc!=0) {
      // UTF-8 compliant byte (or byte sequence), skip as an entiety
      n += nP-proptext;
      proptext = nP;
    }
    else {
      // Not UTF-8 compliant, simply one byte
      n++;
      proptext++;
    }
    // check for optional break indicator (MIME-DIR or allowFoldAtSep)
    if (c=='\b') {
      aString.append(firstunwritten,n-1); // copy what we have up to that '\b'
      firstunwritten += n; // now pointing to next char after '\b'
      foldLoc = 0; // usually now pointing to a NON-LWSP, except if by accident a LWSP follows, which is ok as well
      n = 0; // continue checking from here
      continue; // check next
    }
    // update line length
    llen++;
    // explicit linefeed flag
    explf=(c=='\n' || c=='\r');
    if (aDoNotFold) {
      // prohibit folding for ugly devices like V3i
      if (explf) {
        // append what we have until here
        n--; // explicit \n or \r is ignored
        aString.append(firstunwritten,n);
        // forget the explicit linefeed - and continue
        n = 0;
        llen = 0;
        firstunwritten = proptext;
      }
    }
    else if ((llen>=MIME_MAXLINESIZE && *proptext) || explf) { // avoid unnecessary folding (there must be something more coming)
      // folding needed (line gets longer than MIME_MAXLINESIZE or '\n' found in input string)
      if (aMimeMode==mimo_old && !explf) {
        // vCard 2.1 type folding, must occur before an LWSP
        if (foldLoc<0) {
          // emergency force fold and accept data being shredded
          // - copy all we have by now
          aString.append(firstunwritten,n);
          firstunwritten += n; // now pointing to next
          n = 0; // none left - new line is empty now
          // - insert line break
          aString.append("\x0D\x0A "); // line break AND an extra shredding space
        }
        else {
          // - copy all up to (but not including) last LWSP (or '\b' break indicator)
          aString.append(firstunwritten,foldLoc);
          firstunwritten += foldLoc; // now pointing to LWSP (or non-LWSP in case of '\b')
          n -= foldLoc; // number of chars left (including LWSP)
          // - insert line break
          aString.append("\x0D\x0A"); // line break
          if (*firstunwritten!=' ' && *firstunwritten!=0x09)
            aString += ' '; // breaking at location indicated by '\b', LWSP must be added
          // - copy rest scanned so far (except in '\b' case, this begins with an LWSP)
          aString.append(firstunwritten,n);
        }
      }
      else {
        // MIME-DIR type folding, can occur anywhere and *adds* a LWSP (which is removed at unfolding later)
        // or mimo-old type folding containing explicit CR(LF)s -> break here
        if (explf) {
          // explicit \n or \r is not copied, but only causes line break to occur
          n--;
        }
        if (foldLoc<0 || explf) {
          // no or explf indicator, just fold here
          aString.append(firstunwritten,n);
          n = 0; // nothing left to carry over - new line is empty now
        }
        else {
          // we have a preferred folding location detected before: copy all up to (but not including) break indicator
          aString.append(firstunwritten,foldLoc);
          firstunwritten += foldLoc; // now pointing to next char after break)
          n -= foldLoc; // number of chars left (including LWSP)
        }
        // now fold
        aString.append("\x0D\x0A"); // line break
        if (
          (c!='\r' && aMimeMode==mimo_standard) || // folding indicator and MIME-DIR -> folding always must insert extra space
          (c=='\r' && !aDoSoftBreak) // soft-break indicator, but not in softbreak mode (i.e. B64 input) -> always insert extra space
        )
          aString += ' '; // not only soft line break, but MIMD-DIR type folding
        // - copy carry over from previous line
        if (n>0) aString.append(firstunwritten,n);
      }
      // we are on a new line now
      llen = n; // new line has this size of what was carried over from the previous line
      foldLoc = -1;
      n = 0;
      firstunwritten = proptext;
    }
  }
  // append rest
  aString.append(firstunwritten,n);
  // terminate property
  aString.append("\x0D\x0A"); // CRLF
} // finalizeProperty


// results for generateValue:
#define GENVALUE_NOTSUPPORTED 0 // field not supported
#define GENVALUE_EXHAUSTED    1 // array field exhausted
#define GENVALUE_EMPTYELEMENT 2 // array field empty
#define GENVALUE_EMPTY        3 // non-array field empty
#define GENVALUE_ELEMENT      4 // non-empty array element
#define GENVALUE_NONEMPTY     5 // non-empty non-array value

// helper for generateMimeDir()
// - generate parameter or property value(list),
//   returns: GENVALUE_xxx
sInt16 TMimeDirProfileHandler::generateValue(
  TMultiFieldItem &aItem,     // the item where data comes from
  const TConversionDef *aConvDefP,
  sInt16 aBaseOffset,         // basic fid offset to use
  sInt16 aRepOffset,          // repeat offset, adds to aBaseOffset for non-array fields, is array index for array fields
  string &aString,            // where value is ADDED
  char aSeparator,            // separator to be used between values if field contains multiple values in a list separated by confdef->combineSep
  TMimeDirMode aMimeMode,     // MIME mode (older or newer vXXX format compatibility)
  bool aParamValue,           // set if generating parameter value (different escaping rules, i.e. colon must be escaped, or entire value double-quoted)
  bool aStructured,           // set if value consists of multiple values (needs semicolon content escaping)
  bool aCommaEscape,          // set if "," content escaping is needed (for values in valuelists like TYPE=TEL,WORK etc.)
  TEncodingTypes &aEncoding,  // modified if special value encoding is required
  bool &aNonASCII,            // set if any non standard 7bit ASCII-char is contained
  char aFirstChar,            // will be appended before value if there is any value (and in MIME-DIR '\b' optional break indicator is appended as well)
  sInt32 &aNumNonSpcs,        // how many non-spaces are already in the value
  bool aFoldAtSeparators,     // if true, even in mimo_old folding may appear at value separators (adding an extra space - which is ok for EXDATE and similar)
  bool aEscapeOnlyLF          // if true, only linefeeds are escaped as \n, but nothing else (not even \ itself)
)
{
  string vallist;             // as received from fieldToMIMEString()
  string val;                 // single value
  string outval;              // entire value (list) escaped
  char c;

  // determine field ID
  bool isarray = false; // no array by default
  sInt16 fid=aConvDefP->fieldid;
  if (fid>=0) {
    // field has storage
    // - fid is always offset by baseoffset
    fid += aBaseOffset;
    // - adjust now
    isarray = aItem.adjustFidAndIndex(fid,aRepOffset);
    // generate only if available in both source and target (or non-SyncML context)
    if (isFieldAvailable(aItem,fid)) {
      // find out if value exists
      if (aItem.isAssigned(fid)) {
        // - field has a value assigned (altough this might be empty string)
        // determine max size to truncate value if needed
        outval.erase();
        sInt32 valsiz=0; // net size of value
        //%%%% getTargetItemType???
        sInt32 maxSiz=aItem.getTargetItemType()->getFieldOptions(fid)->maxsize;
        if (maxSiz==FIELD_OPT_MAXSIZE_UNKNOWN || maxSiz==FIELD_OPT_MAXSIZE_NONE)
          maxSiz = 0; // no size restriction
        bool noTruncate=aItem.getTargetItemType()->getFieldOptions(fid)->notruncate;
        // check for BLOB values
        sInt16 convmode = aConvDefP->convmode & CONVMODE_MASK;
        if (convmode==CONVMODE_BLOB_B64 || convmode==CONVMODE_BLOB_AUTO) {
          // no value lists, escaping, enums. Simply set value and encoding
          TItemField *fldP = aItem.getArrayField(fid,aRepOffset,true); // existing array elements only
          if (!fldP) return GENVALUE_EXHAUSTED; // no leaf field - must be exhausted array (fldP==NULL is not possible here for non-arrays)
          if (fldP->isUnassigned()) return GENVALUE_EMPTYELEMENT; // must be empty element empty element, but field supported (fldP==NULL is not possible here for non-arrays)
          // check max size and truncate if needed
          if (maxSiz && sInt32(fldP->getStringSize())>maxSiz) {
            if (noTruncate || getSession()->getSyncMLVersion()<syncml_vers_1_2) {
              // truncate not allowed (default for pre-SyncML 1.2 for BLOB fields)
              PDEBUGPRINTFX(DBG_ERROR+DBG_GEN,("BLOB value exceeds max size (%ld) and cannot be truncated -> omit", (long)maxSiz));
              return GENVALUE_NOTSUPPORTED; // treat it as if field was not supported locally
            }
          }
          // append to existing string
          fldP->appendToString(outval,maxSiz);
          if (convmode==CONVMODE_BLOB_AUTO) {
            // auto mode: use  B64 encoding only if non-printable or
            // non-ASCII characters are in the value
            size_t len = outval.size();
            for (size_t i = 0; i < len; i++) {
              char c = outval[i];
              if (!isascii(c) || !isprint(c)) {
                aEncoding=enc_base64;
                break;
              }
            }
          }
          else {
            // blob mode: always use B64
            aEncoding=enc_base64;
          }
          // only ASCII in value: either because it contains only
          // those to start with or because they will be encoded
          aNonASCII=false;
        }
        else {
          // apply custom field(s)-to-string translation if needed
          if (!fieldToMIMEString(aItem,fid,aRepOffset,aConvDefP,vallist)) {
            // check if no value because array was exhausted
            if (aItem.getArrayField(fid,aRepOffset,true))
              return isarray ? GENVALUE_EMPTYELEMENT : GENVALUE_EMPTY; // no value (but field supported)
            else
              return GENVALUE_EXHAUSTED; // no leaf field - must be exhausted array
          }
          // separate value list into multiple values if needed
          const char *lp = vallist.c_str(); // list item pointer
          const char *sp; // start of item pointer (helper)
          sInt32 n;
          while (*lp!=0) {
            // find (single) input value string's end
            for (sp=lp,n=0; (c=*lp)!=0; lp++, n++) {
              if (c==aConvDefP->combineSep) break;
            }
            // - n=size of input value, p=ptr to end of value (0 or sep)
            val.assign(sp,n);
            // perform enum translation if needed
            if (aConvDefP->enumdefs) {
              const TEnumerationDef *enumP = aConvDefP->findEnumByVal(val.c_str());
              if (enumP) {
                PDEBUGPRINTFX(DBG_GEN+DBG_EXOTIC,("Val='%s' translated to enumName='%s' mode=%s", val.c_str(), TCFG_CSTR(enumP->enumtext), EnumModeNames[enumP->enummode]));
                if (enumP->enummode==enm_ignore)
                  val.erase(); // ignore -> make value empty as empty values are never stored
                else if (enumP->enummode==enm_prefix) {
                  // replace value prefix by text prefix
                  n=TCFG_SIZE(enumP->enumval);
                  val.replace(0,n,TCFG_CSTR(enumP->enumtext)); // replace val prefix by text prefix
                }
                else {
                  // simply use translated value
                  val=enumP->enumtext;
                }
              }
              else {
                PDEBUGPRINTFX(DBG_GEN+DBG_EXOTIC,("No translation found for Val='%s'", val.c_str()));
              }
            }
            // - val is now translated enum (or original value if value does not match any enum text)
            valsiz+=val.size();
            // perform escaping and determine need for encoding
            bool spaceonly = true;
            bool firstchar = true;
            for (const char *p=val.c_str();(c=*p)!=0 && (c!=aConvDefP->combineSep);p++) {
              // process char
              // - check for whitespace
              if (!isspace(c)) {
                spaceonly = false; // does not consist of whitespace only
                aNumNonSpcs++; // count consecutive non-spaces
                if (aMimeMode==mimo_old && aEncoding==enc_none && aNumNonSpcs>MIME_MAXLINESIZE) {
                  // If text contains words with critical (probably unfoldable) size in mimo-old, select quoted printable encoding
                  aEncoding=enc_quoted_printable;
                }
              }
              else {
                aNumNonSpcs = 0; // new word starts
              }
              // only text must be fully escaped, turn escaping off for RRULE (RECUR type)
              // escape reserved chars
              switch (c) {
                case '"':
                  if (firstchar && aParamValue && aMimeMode==mimo_standard) goto do_escape; // if param value starts with a double quote, we need to escape it because param value can be in double-quote-enclosed form
                  goto add_char; // otherwise, just add
                case ',':
                  // in MIME-DIR, always escape commas, in pre-MIME-DIR only if usage in value list requires it
                  if (!aCommaEscape && aMimeMode==mimo_old) goto add_char;
                  goto do_escape;
                case ':':
                  // always escape colon in parameters
                  if (!aParamValue) goto add_char;
                  goto do_escape;
                case '\\':
                  // Backslash must always be escaped
                  // - for MIMO-old: at least Nokia 9210 does it this way
                  // - for MIME-DIR: specified in the standard
                  goto do_escape;
                case ';':
                  // in MIME-DIR, always escape semicolons, in pre-MIME-DIR only in parameters and structured values
                  if (!aParamValue && !aStructured && aMimeMode==mimo_old) goto add_char;
                do_escape:
                  if (!aEscapeOnlyLF) {
                    // escape chars with backslash
                    outval+='\\';
                  }
                  goto out_char;
                case '\r':
                  // ignore returns
                  break;
                case '\n':
                  // quote linefeeds
                  if (aMimeMode==mimo_old) {
                    if (aEncoding==enc_none) {
                      // For line ends in mimo_old: select quoted printable encoding
                      aEncoding=enc_quoted_printable;
                    }
                    // just pass it, will be encoded later
                    goto add_char;
                  }
                  else {
                    // MIME-DIR: use quoted C-style notation
                    outval.append("\\n");
                  }
                  break;
                default:
                add_char:
                  // prevent adding space-only for params
                  if (spaceonly && aParamValue) break; // just check next
                out_char:
                  // check for non ASCII and set flag if found
                  if ((uInt8)c > 0x7F) aNonASCII=true;
                  // just copy to output
                  outval+=c;
                  firstchar = false; // first char is out
                  break;
              }
            } // for all chars in val item
            // go to next item in the val list (if any)
            if (*lp!=0) {
              // more items in the list
              // - add separator if previous one is not empty param value
              if (!(spaceonly && aParamValue)) {
                if (aMimeMode==mimo_standard || aFoldAtSeparators) {
                  outval+='\b'; // preferred break location (or location where extra space is allowed for mimo_old)
                  aNumNonSpcs=0; // we can fold here, so word is broken
                }
                outval+=aSeparator;
                aNumNonSpcs++; // count it (assuming separator is never a space!)
                valsiz++; // count it as part of the value
              }
              lp++; // skip input list separator
            }
            // check for truncation needs (do not truncate parameters, ever)
            if (maxSiz && valsiz>maxSiz && !aParamValue) {
              // size exceeded
              if (noTruncate) {
                // truncate not allowed
                PDEBUGPRINTFX(DBG_ERROR+DBG_GEN,(
                  "Value '%" FMT_LENGTH(".40") "s' exceeds %ld chars net length but is noTruncate -> omit",
                  FMT_LENGTH_LIMITED(40,outval.c_str()),
                  (long)maxSiz
                ));
                // treat it as if field was not supported locally
                return GENVALUE_NOTSUPPORTED;
              }
              else {
                // truncate allowed, shorten output accordingly
                outval.erase(outval.size()-(valsiz-maxSiz));
                PDEBUGPRINTFX(DBG_GEN,(
                  "Truncated value '%" FMT_LENGTH(".40") "s' to %ld chars net length (maxSize)",
                  FMT_LENGTH_LIMITED(40,outval.c_str()),
                  (long)maxSiz
                ));
                // do not add more chars
                break;
              }
            }
          } // while value chars available
        } // not BLOB conversion
        // value generated in outval (altough it might be an empty string)
      } // if field assigned
      else {
        // not assigned. However a not assigned array means an array with no elements, which
        // is the same as an exhausted array
        return isarray ? GENVALUE_EXHAUSTED : GENVALUE_NOTSUPPORTED; // array is exhaused, non-array unassigned means not available
      }
    } // source and target both support the field (or field belongs to mandatory property)
    else
      return GENVALUE_NOTSUPPORTED; // field not supported by either source or target (and not mandatory) -> do not generate value
  } // if fieldid exists
  else {
    // could be special conversion using no data or data from
    // internal object variables (such as VERSION value)
    if (fieldToMIMEString(aItem,FID_NOT_SUPPORTED,0,aConvDefP,vallist)) {
      // got some output, use it as value
      outval=vallist;
    }
    else
      // no value, no output
      return GENVALUE_NOTSUPPORTED; // field not supported
  }
  // now we have a value in outval, check if encoding needs to be applied
  // - check if we should select QUOTED-PRINTABLE because of nonASCII
  if (aNonASCII && fDoQuote8BitContent && aEncoding==enc_none)
    aEncoding=enc_quoted_printable;
  // just append
  if (!outval.empty() && aFirstChar!=0) {
    if (aMimeMode==mimo_standard || aFoldAtSeparators) {
      aString+='\b'; // preferred break location (or location where extra space is allowed for mimo_old)
      aNumNonSpcs = 0; // we can break here, new word starts
    }
    aString+=aFirstChar; // we have a value, add sep char first
    aNumNonSpcs++; // count it (assuming separator is never a space!)
  }
  aString.append(outval);
  // done
  return outval.empty()
    ? (isarray ? GENVALUE_EMPTYELEMENT : GENVALUE_EMPTY) // empty
    : (isarray ? GENVALUE_ELEMENT : GENVALUE_NONEMPTY); // non empty
} // TMimeDirProfileHandler::generateValue



// generate parameters for one property instance
// - returns true if parameters with shownonempty=true were generated
bool TMimeDirProfileHandler::generateParams(
  TMultiFieldItem &aItem, // the item where data comes from
  string &aString, // the string to add parameters to
  const TPropertyDefinition *aPropP, // the property to generate (all instances)
  TMimeDirMode aMimeMode, // MIME mode (older or newer vXXX format compatibility)
  sInt16 aBaseOffset,
  sInt16 aRepOffset,
  TPropNameExtension *aPropNameExt, // propname extension for generating musthave param values
  sInt32 &aNumNonSpcs // number of consecutive non-spaces, accumulated so far
)
{
  const TParameterDefinition *paramP;
  bool paramstarted;
  char sep=0; // separator for value lists
  bool nonasc=false;
  TEncodingTypes encoding;
  string paramstr;
  bool showalways=false;

  // Generate parameters
  // Note: altough positional values are always the same, non-positional values
  //       can vary from repetition to repetition and can be mixed with the
  //       positional values. So we must generate the mixture again for
  //       every repetition.
  // - check all parameters for musthave values
  paramP = aPropP->parameterDefs;
  while (paramP) {
    // parameter not started yet
    paramstarted=false;
    // process param only if matching mode and active rules
    if (mimeModeMatch(paramP->modeDependency)
      #ifndef NO_REMOTE_RULES
      && (!paramP->ruleDependency || isActiveRule(paramP->ruleDependency))
      #endif
    ) {
      // first append extendsname param values
      if (paramP->extendsname && aPropNameExt) {
        const TEnumerationDef *enumP = paramP->convdef.enumdefs;
        while (enumP) {
          if (enumP->nameextid>=0) {
            // value is relevant for name extension, check if required for this param
            if ((((TNameExtIDMap)1<<enumP->nameextid) & (aPropNameExt->musthave_ids | aPropNameExt->addtlSend_ids))!=0) {
              // found param value which is required or flagged to be sent additionally as name extension
              if (!paramstarted) {
                paramstarted=true;
                aString+=';'; // param always starts with ;
                if (paramP->defaultparam && (aMimeMode==mimo_old)) {
                  // default param, values are written like a list of params
                  sep=';'; // separator, in case other values follow
                }
                else {
                  // normal parameter, first add param separator and name
                  // - lead-in
                  aString.append(paramP->paramname);
                  aString+='=';
                  // - separator, in case other values follow
                  sep=','; // value list separator is comma by default
                }
              }
              else {
                // add separator for one more value
                aString+=sep;
              }
              // add value
              aString.append(enumP->enumtext);
            } // if enum value is a "must have" value for name extension
          } // if enum value is relevant to name extension
          // next enum value
          enumP=enumP->next;
        } // while enum values
      } // if extendsname
      // append value(s) if there is an associated field
      paramstr.erase(); // none to start with
      if (paramP->convdef.fieldid!=FID_NOT_SUPPORTED) {
        if (!paramstarted) {
          // Note: paramstarted must not be set here, as empty value might prevent param from being written
          // parameter starts with ";"
          paramstr+=';';
          if (paramP->defaultparam && (aMimeMode==mimo_old)) {
            // default param, values are written like a list of params
            sep=';';
          }
          else {
            // normal parameter, first add name
            paramstr.append(paramP->paramname);
            paramstr+='=';
            sep=','; // value list separator is comma by default
          }
        }
        else {
          // already started values, just add more
          // - next value starts with a separator
          paramstr+=sep;
        }
        // add parameter value(list)
        encoding=enc_none; // parameters are not encoded
        // NOTE: only non-empty parameters are generated
        // NOTE: parameters themselves cannot have a value list that is stored in an array,
        //       but parameters of repeating properties can be stored in array elements (using the
        //       same index as for the property itself)
        // Note: Escape commas if separator is a comma
        sInt32 numNoSpcs = aNumNonSpcs;
        if (generateValue(aItem,&(paramP->convdef),aBaseOffset,aRepOffset,paramstr,sep,aMimeMode,true,false,sep==',',encoding,nonasc,0,numNoSpcs,false,false)>=GENVALUE_ELEMENT) {
          // value generated, add parameter name/value (or separator/value for already started params)
          aString.append(paramstr);
          aNumNonSpcs = numNoSpcs; // actually added, count now
          paramstarted=true; // started only if we really have appended something at all
        }
      } // if field defined for this param
      // update show status
      if (paramP->shownonempty && paramstarted)
        showalways=true; // param has a value and must make property show
    }
    // next param
    paramP=paramP->next;
  } // while params
  return showalways;
} // TMimeDirProfileHandler::generateParams


// generateProperty return codes:
#define GENPROP_EXHAUSTED 0 // nothing generated because data source exhausted (or field not supported)
#define GENPROP_EMPTY     1 // nothing generated because empty value (but field supported)
#define GENPROP_NONEMPTY  2 // something generated



// helper for generateMimeDir(), expansion of property according to nameExts
void TMimeDirProfileHandler::expandProperty(
  TMultiFieldItem &aItem, // the item where data comes from
  string &aString, // the string to add properties to
  const char *aPrefix, // the prefix (property name)
  const TPropertyDefinition *aPropP, // the property to generate (all instances)
  TMimeDirMode aMimeMode // MIME mode (older or newer vXXX format compatibility)
)
{
  // scan nameExts to generate name-extended variants and repetitions
  TPropNameExtension *propnameextP = aPropP->nameExts;
  if (!propnameextP) {
    // no name extensions -> this is a non-repeating property
    // just generate once, even if empty (except if it has suppressempty set)
    generateProperty(
      aItem,          // the item where data comes from
      aString,        // the string to add properties to
      aPrefix,        // the prefix (property name)
      aPropP,         // the property to generate
      0,              // field ID offset to be used
      0,              // additional repeat offset / array index
      aMimeMode,      // MIME mode (older or newer vXXX format compatibility)
      false           // if set, a property with only empty values will never be generated
    );
  }
  else {
    // scan name extensions
    sInt16 generated=0;
    sInt16 maxOccur=0; // default to no limit
    while (propnameextP) {
      sInt16 baseoffs=propnameextP->fieldidoffs;
      sInt16 repoffs=0; // no repeat offset yet
      if (baseoffs!=OFFS_NOSTORE && !propnameextP->readOnly) {
        // we can address fields for this property and it's not readonly (parsing variant)
        // generate value part
        sInt16 n=propnameextP->maxRepeat;
        // check for value list
        if (aPropP->valuelist && !aPropP->expandlist) {
          // property contains a value list -> all repetitions are shown within ONE property instance
          // NOTE: generateProperty will exhaust possible repeats
          generateProperty(
            aItem,          // the item where data comes from
            aString,        // the string to add properties to
            aPrefix,        // the prefix (property name)
            aPropP,         // the property to generate
            baseoffs,       // field ID offset to be used
            repoffs,        // additional repeat offset / array index
            aMimeMode,      // MIME mode (older or newer vXXX format compatibility)
            propnameextP->minShow<1, // suppress if fewer to show than 1 (that is, like suppressempty in this case)
            propnameextP    // propname extension for generating musthave param values and maxrep/repinc for valuelists
          );
        }
        else {
          // now generate separate properties for all repetitions
          // Note: strategy is to keep order as much as possible (completely if
          //       minShow is >= maxRepeat
          sInt16 emptyRepOffs=-1;
          // get occurrence limit as provided by remote
          for (sInt16 i=0; i<aPropP->numValues; i++) {
            sInt16 fid=aPropP->convdefs[0].fieldid;
            if (fid>=0) {
              if (fRelatedDatastoreP) {
                // only if datastore is related we are in SyncML context, otherwise we should not check maxOccur
                maxOccur = aItem.getItemType()->getFieldOptions(fid)->maxoccur;
              }
              else
                maxOccur = 0; // no limit
              // Note: all value fields of the property will have the same maxOccur, so we can stop here
              break;
            }
          }
          do {
            // generate property for this repetition
            // - no repeating within generateProperty takes place!
            sInt16 genres = generateProperty(
              aItem,          // the item where data comes from
              aString,        // the string to add properties to
              aPrefix,        // the prefix (property name)
              aPropP,         // the property to generate
              baseoffs,       // field ID offset to be used
              repoffs,        // additional repeat offset / array index
              aMimeMode,      // MIME mode (older or newer vXXX format compatibility)
              propnameextP->minShow-generated<n, // suppress if fewer to show than remaining repeats
              propnameextP    // propname extension for generating musthave param values
            );
            if (genres==GENPROP_NONEMPTY) {
              // generated a property
              generated++;
            }
            else {
              // nothing generated
              if (emptyRepOffs<0) emptyRepOffs=repoffs; // remember empty
              // check for array repeat, in which case exhausted array or non-supported field will stop generating
              if (propnameextP->maxRepeat==REP_ARRAY && genres==GENPROP_EXHAUSTED) break; // exit loop if any only if array exhausted
            }
            // one more generated of the maximum possible (note: REP_ARRAY=32k, so this will not limit an array)
            n--;
            repoffs+=propnameextP->repeatInc;
            // end generation if remote's maxOccur limit is reached
            if (maxOccur && generated>=maxOccur) {
              PDEBUGPRINTFX(DBG_GEN,(
                "maxOccur (%hd) for Property '%s' reached - no more instances will be generated",
                maxOccur,
                TCFG_CSTR(aPropP->propname)
              ));
              break;
            }
          } while(n>0);
          // add empty ones if needed
          while (generated<propnameextP->minShow && emptyRepOffs>=0 && !(maxOccur && generated>=maxOccur)) {
            // generate empty ones (no suppression)
            generateProperty(aItem,aString,aPrefix,aPropP,baseoffs,emptyRepOffs,aMimeMode,false);
            generated++; // count as generated anyway (even in case generation of empty is globally turned off)
          }
        } // repeat properties when we have repeating enabled
      } // if name extension is stored
      propnameextP=propnameextP->next;
      // stop if maxOccur reached
      if (maxOccur && generated>=maxOccur) break;
    } // while nameexts
  } // if nameexts at all
} // TMimeDirProfileHandler::expandProperty


// helper for expandProperty: generates property
//   returns: GENPROP_xxx
sInt16 TMimeDirProfileHandler::generateProperty(
  TMultiFieldItem &aItem, // the item where data comes from
  string &aString, // the string to add properties to
  const char *aPrefix, // the prefix (property name)
  const TPropertyDefinition *aPropP, // the property to generate (all instances)
  sInt16 aBaseOffset, // field ID offset to be used
  sInt16 aRepeatOffset, // additional repeat offset / array index
  TMimeDirMode aMimeMode, // MIME mode (older or newer vXXX format compatibility)
  bool aSuppressEmpty, // if set, a property with only empty values will not be generated
  TPropNameExtension *aPropNameExt // propname extension for generating musthave param values and maxrep/repinc for valuelists
)
{
  string proptext; // unfolded property text
  proptext.reserve(300); // not too small
  string elemtext; // single element (value or param) text
  TEncodingTypes encoding;
  bool nonasc=false;

  // - reset TZID presence flag
  fPropTZIDtctx = TCTX_UNKNOWN;
  // - start with empty text
  proptext.erase();
  // - init flags
  bool anyvaluessupported = false; // at least one of the main values must be supported by the remote in order to generate property at all
  bool arrayexhausted = false; // flag will be set if a main value was not generated because array exhausted
  bool anyvalues = false; // flag will be set when at least one value has been generated
  sInt32 numNonSpcs=0;
  encoding=enc_none; // default is no encoding
  sInt16 v=0; // value counter
  nonasc=false; // assume plain ASCII
  sInt16 genres;
  TEncodingTypes enc;
  bool na;
  const TConversionDef *convP;
  // - now generate
  if (aPropP->unprocessed) {
    convP = &(aPropP->convdefs[0]);
    // unfolded and not-linefeed-escaped raw property stored in string - generate it as one value
    enc=encoding;
    na=false;
    genres=generateValue(
      aItem,
      convP,
      aBaseOffset, // base offset, relative to
      aRepeatOffset, // repeat offset or array index
      elemtext, // value will be stored here (might be binary in case of BLOBs, but then encoding will be set)
      0, // should for some exotic reason values consist of a list, separate it by "," (";" is reserved for structured values)
      aMimeMode,
      false, // no parameter
      false, // not structured value, don't escape ";"
      false, // don't escape commas, either
      enc, // will receive needed encoding (usually B64 for binary values)
      na,
      0, // no first char
      numNonSpcs, // number of consecutive non-spaces, accumulated
      aPropP->allowFoldAtSep,
      true // linefeed only escaping
    );
    // check if something was generated
    if (genres>=GENVALUE_ELEMENT) {
      // generated something, might have caused encoding/noasc change
      encoding=enc;
      nonasc=nonasc || na;
      anyvaluessupported=true;
      anyvalues=true;
      // separate name+params and values
      bool dq = false;
      bool fc = true;
      size_t pti = string::npos;
      appChar c;
      for (cAppCharP p=elemtext.c_str(); (c=*p)!=0; p++) {
        if (dq) {
          if (c=='"') dq = false; // end of double quoted part
        }
        else {
          // not in doublequote
          if (fc && c=='"') dq = true; // if first char is doublequote, start quoted string
          else if (c==':') { pti = p-elemtext.c_str(); break; } // found
          else if (c=='\\' && *(p+1)) p++; // make sure next char is not evaluated
        }
        fc = false; // no longer first char
      }
      if (pti!=string::npos) {
        proptext.assign(elemtext, 0, pti); // name+params without separator
        elemtext.erase(0, pti+1); // remove name and separator
      }
    }
    else {
      arrayexhausted = true; // no more values, assume array exhausted
    }
  }
  else {
    // Normally generated property
    // - first set group if there is one
    if (aPropP->groupFieldID!=FID_NOT_SUPPORTED) {
      // get group name
      TItemField *g_fldP = aItem.getArrayFieldAdjusted(aPropP->groupFieldID+aBaseOffset, aRepeatOffset, true);
      if (g_fldP && !g_fldP->isEmpty()) {
        g_fldP->appendToString(proptext);
        proptext += '.'; // group separator
      }
    }
    // - append name and (possibly) parameters that are constant over all repetitions
    proptext += aPrefix;
    // - up to here assume no spaces
    numNonSpcs = proptext.size()+14; // some extra room for possible ";CHARSET=UTF8"
    // - append parameter values
    //   anyvalues gets set if a parameter with shownonempty attribute was generated
    anyvalues=generateParams(
      aItem,  // the item where data comes from
      proptext, // where params will be appended
      aPropP,  // the property definition
      aMimeMode,
      aBaseOffset,
      aRepeatOffset,
      aPropNameExt,
      numNonSpcs
    );
    // - append value(s)
    sInt16 maxrep=1,repinc=1;
    if (aPropNameExt) {
      maxrep=aPropNameExt->maxRepeat;
      repinc=aPropNameExt->repeatInc;
    }
    // generate property contents
    if (aPropP->valuelist && !aPropP->expandlist) {
      // property with value list
      // NOTE: convdef[0] is used for all values, aRepeatOffset changes
      convP = &(aPropP->convdefs[0]);
      // - now iterate over available repeats or array contents
      while(aRepeatOffset<maxrep*repinc || maxrep==REP_ARRAY) {
        // generate one value
        enc=encoding;
        na=false;
        genres=generateValue(
          aItem,
          convP,
          aBaseOffset, // offset relative to base field
          aRepeatOffset, // additional offset or array index
          elemtext,
          aPropP->valuesep, // use valuelist separator between multiple values possibly generated from a list in a single field (e.g. CATEGORIES)
          aMimeMode,
          false, // not a param
          true, // always escape ; in valuelist properties
          aPropP->valuesep==',' || aPropP->altvaluesep==',', // escape commas if one of the separators is a comma
          enc,
          na,
          v>0 ? aPropP->valuesep : 0, // separate with specified multi-value-delimiter if not first value
          numNonSpcs, // number of consecutive non-spaces, accumulated
          aPropP->allowFoldAtSep,
          (convP->convmode & CONVMODE_MASK)==CONVMODE_RRULE // RRULES are not to be escaped
        );
        // check if something was generated
        if (genres>=GENVALUE_ELEMENT) {
          // generated something, might have caused encoding/noasc change
          encoding=enc;
          nonasc=nonasc || na;
        }
        // update if we have at least one value of this property supported (even if empty) by the remote party
        if (genres>GENVALUE_NOTSUPPORTED) anyvaluessupported=true;
        if (genres==GENVALUE_EXHAUSTED) arrayexhausted=true; // for at least one component of the property, the array is exhausted
        // update if we have any value now (even if only empty)
        // - generate empty property according to
        //   - aSuppressEmpty
        //   - session-global fDontSendEmptyProperties
        //   - supressEmpty property flag in property definition
        //   - if no repeat (i.e. no aPropNameExt), exhausted array is treated like empty value (i.e. rendered unless suppressempty set)
        anyvalues = anyvalues || (genres>=
          (aSuppressEmpty || fDontSendEmptyProperties || aPropP->suppressEmpty
            ? GENVALUE_ELEMENT // if empty values should be suppressed, we need a non-empty element (array or simple field)
            : GENVALUE_EXHAUSTED // for valuelists, if empty values are not explicitly suppressed (i.e. minshow==0, see caller) exhausted array must always produce an empty value
          )
        );
        // count effective value appended
        v++;
        // update repeat offset
        aRepeatOffset+=repinc;
        // check for array mode - stop if array is exhausted or field not supported
        if (maxrep==REP_ARRAY && genres<=GENVALUE_EXHAUSTED) break;
      }
    }
    else {
      // property with individual values (like N)
      // NOTE: field changes with different convdefs, offsets remain stable
      arrayexhausted = true; // assume all arrays exhausted unless we find at least one non-exhausted array
      bool somearrays = false; // no arrays yet
      do {
        convP = &(aPropP->convdefs[v]);
        // generate one value
        enc=encoding;
        na=false;
        genres=generateValue(
          aItem,
          convP,
          aBaseOffset, // base offset, relative to
          aRepeatOffset, // repeat offset or array index
          elemtext, // value will be stored here (might be binary in case of BLOBs, but then encoding will be set)
          ',', // should for some exotic reason values consist of a list, separate it by "," (";" is reserved for structured values)
          aMimeMode,
          false,
          aPropP->numValues>1, // structured value, escape ";"
          aPropP->altvaluesep==',', // escape commas if alternate separator is a comma
          enc, // will receive needed encoding (usually B64 for binary values)
          na,
          0, // no first char
          numNonSpcs, // number of consecutive non-spaces, accumulated
          aPropP->allowFoldAtSep,
          (convP->convmode & CONVMODE_MASK)==CONVMODE_RRULE // RRULES are not to be escaped
        );
        //* %%% */ PDEBUGPRINTFX(DBG_EXOTIC,("generateValue #%hd for property '%s' returns genres==%hd",v,TCFG_CSTR(aPropP->propname),genres));
        // check if something was generated
        if (genres>=GENVALUE_ELEMENT) {
          // generated something, might have caused encoding/noasc change
          encoding=enc;
          nonasc=nonasc || na;
        }
        // update if we have at least one value of this property supported (even if empty) by the remote party
        if (genres>GENVALUE_NOTSUPPORTED) anyvaluessupported=true;
        if (genres==GENVALUE_ELEMENT || genres==GENVALUE_EMPTYELEMENT) {
          arrayexhausted = false; // there is at least one non-exhausted array we're reading from (even if only empty value)
          somearrays = true; // generating from array
        }
        else if (genres==GENVALUE_EXHAUSTED)
          somearrays = true; // generating from array
        // update if we have any value now (even if only empty)
        // - generate empty property according to
        //   - aSuppressEmpty
        //   - session-global fDontSendEmptyProperties
        //   - supressEmpty property flag in property definition
        //   - if no repeat (i.e. no aPropNameExt), exhausted array is treated like empty value (i.e. rendered unless suppressempty set)
        anyvalues = anyvalues ||
          (genres>=(aSuppressEmpty || fDontSendEmptyProperties || aPropP->suppressEmpty ? GENVALUE_ELEMENT : (aPropNameExt ? GENVALUE_EMPTYELEMENT : GENVALUE_EXHAUSTED)));
        // insert delimiter if not last value
        v++;
        if (v>=aPropP->numValues) break; // done with all values
        // add delimiter for next value
        if (aMimeMode==mimo_standard || aPropP->allowFoldAtSep) {
          elemtext+='\b'; // preferred break location (or location where extra space is allowed for mimo_old)
          numNonSpcs = 0; // can break here, new word starts
        }
        elemtext+=aPropP->valuesep;
        numNonSpcs++; // count it (assuming separator is never a space!)
        // add break indicator
      } while(true);
      // if none of the data sources is an array, we can't be exhausted.
      if (!somearrays) arrayexhausted = false;
    }
  } // normal generated property from components
  // - finalize property if it contains supported fields at all (or is mandatory)
  if ((anyvaluessupported && anyvalues) || aPropP->mandatory) {
    // - generate charset parameter if needed
    //   NOTE: MIME-DIR based formats do NOT have the CHARSET attribute any more!
    if (nonasc && aMimeMode==mimo_old && fDefaultOutCharset!=chs_ansi) {
      // non-ASCII chars contained, generate property telling what charset is used
      proptext.append(";CHARSET=");
      proptext.append(MIMECharSetNames[fDefaultOutCharset]);
    }
    // - generate encoding parameter if needed
    if (encoding!=enc_none) {
      // in MIME-DIR, only "B" is allowed for binary, for vCard 2.1 it is "BASE64"
      if (encoding==enc_base64 || encoding==enc_b) {
        encoding = aMimeMode==mimo_standard ? enc_b : enc_base64;
      }
      // add the parameter
      proptext.append(";ENCODING=");
      proptext.append(MIMEEncodingNames[encoding]);
    }
    // - separate value from property text
    proptext+=':';
    // - append (probably encoded) values now, always in UTF-8
    encodeValues(encoding,fDefaultOutCharset,elemtext,proptext,fDoNotFoldContent);
    // - fold, copy and terminate (CRLF) property into aString output
    finalizeProperty(proptext.c_str(),aString,aMimeMode,fDoNotFoldContent,encoding==enc_quoted_printable);
    // - special case: base64 (but not B) encoded value must have an extra CRLF even if folding is
    //   disabled, so we need to insert it here (because non-folding mode eliminates it from being
    //   generated automatically in encodeValues/finalizeProperty)
    if (fDoNotFoldContent && encoding==enc_base64)
      aString.append("\x0D\x0A"); // extra CRLF terminating a base64 encoded property (note, base64 only occurs in mimo_old)
    // - property generated
    return GENPROP_NONEMPTY;
  }
  else {
    // Note: it is essential to return GENPROP_EXHAUSTED if no values are supported for this property at
    //       all (otherwise caller might loop endless trying to generate a non-empty property
    return
      anyvaluessupported
      ? (arrayexhausted ? GENPROP_EXHAUSTED : GENPROP_EMPTY) // no property generated
      : GENPROP_EXHAUSTED; // no values supported means "exhausted" as well
  }
} // TMimeDirProfileHandler::generateProperty



// generate MIME-DIR from item into string object
void TMimeDirProfileHandler::generateMimeDir(TMultiFieldItem &aItem, string &aString)
{
  // clear string
  aString.reserve(3000); // not too small
  aString.erase();
  // reset item time zone before generating
  fHasExplicitTZ = false; // none set explicitly
  fItemTimeContext = fReceiverTimeContext; // default to receiver context
  fUsedTCtxSet.clear(); // no TZIDs used yet
  fEarliestTZDate = noLinearTime; // reset range of generated timestamps related to a TZID or TZ/DAYLIGHT
  fLatestTZDate = noLinearTime;
  fVTimeZonePendingProfileP = NULL; // no VTIMEZONE pending for generation
  fVTimeZoneInsertPos = 0; // no insert position yet
  // recursively generate levels
  generateLevels(aItem,aString,fProfileDefinitionP);
  // now generate VTIMEZONE, if needed
  if (fVTimeZonePendingProfileP) {
    string s, val, vtz;
    vtz.erase();
    // generate needed vTimeZones (according to fUsedTCtxSet)
    for (TTCtxSet::iterator pos=fUsedTCtxSet.begin(); pos!=fUsedTCtxSet.end(); pos++) {
      // - calculate first and last year covered by timestamps in this record
      sInt16 startYear=0,endYear=0;
      if (fEarliestTZDate && fProfileCfgP->fVTimeZoneGenMode!=vtzgen_current) {
        // dependent on actually created dates
        lineartime2date(fEarliestTZDate, &startYear, NULL, NULL);
        lineartime2date(fLatestTZDate, &endYear, NULL, NULL);
        // there is at least one date in the record
        switch (fProfileCfgP->fVTimeZoneGenMode) {
          case vtzgen_start:
            endYear = startYear; // only show for start of range
            break;
          case vtzgen_end:
            startYear = endYear; // only show for end of range
            break;
          case vtzgen_range:
            // pass both start and end year
            break;
          case vtzgen_openend:
            // pass start year but request that all rules from start up to the current date are inlcuded
            endYear = 0;
            break;
          case vtzgen_current:
          case numVTimeZoneGenModes:
            // case statement to keep gcc happy, will not be reached because of if() above
            break;
        }
      }
      // - lead-in
      s="BEGIN:";
      s.append(fVTimeZonePendingProfileP->levelName);
      finalizeProperty(s.c_str(),vtz,fMimeDirMode,false,false);
      // - generate raw string
      //%%% endYear is not yet implemented in internalToVTIMEZONE(), fTzIdGenMode has only the olson option for now
      internalToVTIMEZONE(*pos, val, getSessionZones(), NULL, startYear, endYear, fProfileCfgP->fTzIdGenMode==tzidgen_olson ? "o" : NULL);
      size_t i,n = 0;
      while (val.size()>n) {
        i = val.find('\n',n); // next line end
        if (i==string::npos) i=val.size();
        if (i-n>1) {
          // more than one char = not only a trailing line end
          s.assign(val,n,i-n);
          // finalize and add property
          finalizeProperty(s.c_str(),vtz,fMimeDirMode,false,false);
          // advance cursor beyond terminating LF
          n=i+1;
        }
      }
      // - lead out
      s="END:";
      s.append(fVTimeZonePendingProfileP->levelName);
      finalizeProperty(s.c_str(),vtz,fMimeDirMode,false,false);
    } // for
    // now insert the VTIMEZONE into the output string (so possibly making it appear BEFORE the
    // properties that use TZIDs)
    aString.insert(fVTimeZoneInsertPos, vtz);
    // done
    fVTimeZonePendingProfileP = NULL;
  } // if pending VTIMEZONE
} // TMimeDirProfileHandler::generateMimeDir


// generate nested levels of MIME-DIR content
void TMimeDirProfileHandler::generateLevels(
  TMultiFieldItem &aItem,
  string &aString,
  const TProfileDefinition *aProfileP
)
{
  // check if level must be generated
  bool dolevel=false;
  string s,val;
  sInt16 fid=aProfileP->levelConvdef.fieldid;
  if (fid<0) dolevel=true; // if no controlling field there, generate anyway
  else {
    // check field contents to determine if generation is needed
    if (aItem.isAssigned(fid)) {
      const TEnumerationDef *enumP = aProfileP->levelConvdef.enumdefs;
      aItem.getField(fid)->getAsString(val);
      if (enumP) {
        // if enumdefs, content must match first enumdef's enumval (NOT enumtext!!)
        dolevel = strucmp(val.c_str(),TCFG_CSTR(enumP->enumval))==0;
      }
      else {
        // just being not empty enables level
        dolevel = !aItem.getField(fid)->isEmpty();
      }
    }
  }
  // check for MIME mode dependency
  dolevel = dolevel && mimeModeMatch(aProfileP->modeDependency);
  // generate level if enabled
  if (dolevel) {
    // generate level start
    if (aProfileP->profileMode==profm_vtimezones) {
      // don't generate now, just remember the string position where we should add the
      // VTIMEZONEs when we're done generating the record.
      fVTimeZonePendingProfileP = aProfileP;
      fVTimeZoneInsertPos = aString.size();
    }
    else {
      // standard custom level
      s="BEGIN:";
      s.append(aProfileP->levelName);
      finalizeProperty(s.c_str(),aString,fMimeDirMode,false,false);
      // loop through all properties of that level
      const TPropertyDefinition *propP = aProfileP->propertyDefs;
      #ifndef NO_REMOTE_RULES
      uInt16 propGroup=0; // group identifier (all props with same name have same group ID)
      const TPropertyDefinition *otherRulePropP = NULL; // default property which is used if none of the rule-dependent in the group was used
      bool ruleSpecificExpanded = false;
      #endif
      const TPropertyDefinition *expandPropP;
      while (propP) {
        // check for mode dependency
        if (!mimeModeMatch(propP->modeDependency)) {
          // no mode match -> just skip this one
          propP=propP->next;
          continue;
        }
        #ifndef NO_REMOTE_RULES
        // check for beginning of new group (no or different property group number)
        if (propP->propGroup==0 || propP->propGroup!=propGroup) {
          // end of last group - start of new group
          propGroup = propP->propGroup; // remember new group number
          // expand "other"-rule dependent variant from last group
          if (!ruleSpecificExpanded && otherRulePropP) {
            expandProperty(
              aItem,
              aString,
              TCFG_CSTR(otherRulePropP->propname), // the prefix consists of the property name
              otherRulePropP, // the property definition
              fMimeDirMode // MIME-DIR mode
            );
          }
          // for next group, no rule-specific version has been expanded yet
          ruleSpecificExpanded = false;
          // for next group, we don't have a "other"-rule variant
          otherRulePropP=NULL;
        }
        // check if entry is rule-specific
        expandPropP=NULL; // do not expand by default
        if (propP->dependsOnRemoterule) {
          // check if depends on current rule
          if (propP->ruleDependency==NULL) {
            // this is the "other"-rule dependent variant
            // - just remember
            otherRulePropP=propP;
          }
          else if (isActiveRule(propP->ruleDependency)) {
            // specific for the applied rule
            expandPropP=propP; // default to expand current prop
            // now we have expanded a rule-specific property (blocks expanding of "other"-rule dependent prop)
            ruleSpecificExpanded=true;
          }
        }
        else {
          // does not depend on rule, expand anyway
          expandPropP=propP;
        }
        // check if this is last prop of list
        propP=propP->next;
        if (!propP && otherRulePropP && !ruleSpecificExpanded) {
          // End of prop list, no rule-specific expand yet, and there is a otherRuleProp
          // expand "other"-rule's property instead
          expandPropP=otherRulePropP;
        }
        #else
        // simply expand it
        expandPropP=propP;
        propP=propP->next;
        #endif
        // now expand if selected
        if (expandPropP)
        {
          // recursively generate all properties that expand from this entry
          // (includes extendsfieldid-parameters and repetitions
          expandProperty(
            aItem,
            aString,
            TCFG_CSTR(expandPropP->propname), // the prefix consists of the property name
            expandPropP, // the property definition
            fMimeDirMode // MIME-DIR mode
          );
        }
      } // properties loop
      // generate sublevels, if any
      const TProfileDefinition *subprofileP = aProfileP->subLevels;
      while (subprofileP) {
        // generate sublevels (possibly, none is generated)
        generateLevels(aItem,aString,subprofileP);
        // next
        subprofileP=subprofileP->next;
      }
      // generate level end
      s="END:";
      s.append(aProfileP->levelName);
      finalizeProperty(s.c_str(),aString,fMimeDirMode,false,false);
    } // normal level
  } // if level must be generated
} // TMimeDirProfileHandler::generateLevels


// Convert string from MIME-format into field value(s).
// - the string passed to this function is already a translated value
//   list if combinesep is set, and every single value is already
//   enum-translated if enums are defined.
// - returns false if field(s) could not be assigned because aText has
//   a bad syntax.
// - returns true if field(s) assigned something useful or no field is
//   available to assign anything to.
bool TMimeDirProfileHandler::MIMEStringToField(
  const char *aText,                // the value text to assign or add to the field
  const TConversionDef *aConvDefP,  // the conversion definition record
  TMultiFieldItem &aItem,           // the item where data goes to
  sInt16 aFid,                       // the field ID (can be NULL for special conversion modes)
  sInt16 aArrIndex                   // the repeat offset to handle array fields
)
{
  sInt16 moffs;
  uInt16 offs,n;
  bool isBitMap;
  fieldinteger_t flags = 0;
  TTimestampField *tsFldP;
  timecontext_t tctx;
  TParsedTzidSet::iterator tz;
  string s;
  // RRULE
  lineartime_t dtstart;
  timecontext_t startcontext = 0, untilcontext = 0;
  char freq;
  char freqmod;
  sInt16 interval;
  fieldinteger_t firstmask;
  fieldinteger_t lastmask;
  lineartime_t until;
  bool dostore;

  // get pointer to leaf field
  TItemField *fldP = aItem.getArrayField(aFid,aArrIndex);
  sInt16 convmode = aConvDefP->convmode & CONVMODE_MASK;
  switch (convmode) {
    case CONVMODE_MAILTO:
      // remove the mailto: prefix if there is one
      if (strucmp(aText,"mailto:",7)==0)
        aText+=7; // remove leading "mailto:"
      goto normal;
    case CONVMODE_EMPTYONLY:
      // same as CONVMODE_NONE, but assigns only first occurrence (that is,
      // when field is still empty)
      if (!fldP) return true; // no field, assignment "ok" (=nop)
      if (!fldP->isEmpty()) return true; // field not empty, discard new assignment
    case CONVMODE_TIMESTAMP: // nothing special for parsing
    case CONVMODE_AUTODATE: // nothing special for parsing
    case CONVMODE_AUTOENDDATE: // check for "last minute of the day"
    case CONVMODE_DATE: // dates will be made floating
    case CONVMODE_NONE:
    normal:
      if (!fldP) return true; // no field, assignment "ok" (=nop)
      // just set as string or add if combine mode
      if (aConvDefP->combineSep) {
        // combine mode
        if (!fldP->isEmpty()) {
          // not empty, append with separator
          char cs[2];
          cs[0]=aConvDefP->combineSep;
          cs[1]=0;
          fldP->appendString(cs);
        }
        fldP->appendString(aText);
      }
      else {
        // for non-strings, skip leading spaces before trying to parse
        if (!fldP->isBasedOn(fty_string)) {
          while (*aText && *aText==' ') aText++; // skip leading spaces
        }
        // simple assign mode
        if (fldP->isBasedOn(fty_timestamp)) {
          // read as ISO8601 timestamp
          tsFldP = static_cast<TTimestampField *>(fldP);
          // if field already has a non-unknown context (e.g. set via TZID, or TZ/DAYLIGHT),
          // use that as context for floating ISO date (i.e. no "Z" or "+/-hh:mm" suffix)
          if (!tsFldP->isFloating()) {
            // field already has a TZ specified (e.g. by a TZID param), use that instead of item level context
            tctx = tsFldP->getTimeContext();
          }
          else {
            // no pre-known zone for this specific field, check if property has a specific zone
            if (!TCTX_IS_UNKNOWN(fPropTZIDtctx)) {
              // property has a specified time zone context from a TZID, use it
              tctx = fPropTZIDtctx; // default to property's TZID (if one was parsed, otherwise this will be left floating)
            }
            else if (fHasExplicitTZ) {
              // item has an explicitly specified time zone context (e.g. set via TZ: property),
              // treat all timestamps w/o own time zone ("Z" suffix) in that context
              tctx = fItemTimeContext;
            }
            else {
              // item has no explicitly specified time zone context,
              // parse and leave floating float for now
              tctx = TCTX_UNKNOWN; // default to floating
            }
          }
          // Now tctx is the default zone to bet set for ALL values that are in floating notation
          // - check for special handling of misbehaving remotes
          if (fTreatRemoteTimeAsLocal || fTreatRemoteTimeAsUTC) {
            // ignore time zone specs which might be present possibly
            tsFldP->setAsISO8601(aText, tctx, true);
            // now force time zone to item/user context or UTC depending on flag settings
            tctx = fTreatRemoteTimeAsLocal ? fItemTimeContext : TCTX_UTC;
            // set it
            tsFldP->setTimeContext(tctx);
          }
          else {
            // read with time zone, if present, and default to tctx set above
            tsFldP->setAsISO8601(aText, tctx, false);
            // check if still floating now
            if (tsFldP->isFloating()) {
              // unfloat only if remote cannot handle UTC and therefore ALWAYS uses localtime.
              // otherwise, assume that floating status is intentional and must be retained.
              // Note: TZID and TZ, if present, are already applied by now
              // Note: DURATION and DATE floating will always be retained, as they are always intentional
              if ((!fReceiverCanHandleUTC || fProfileCfgP->fUnfloatFloating) && !TCTX_IS_DATEONLY(tsFldP->getTimeContext()) && !tsFldP->isDuration()) {
                // not intentionally floating, but just not capable otherwise
                // - put it into context of item (which is in this case session's user context)
                tsFldP->setTimeContext(fItemTimeContext);
              }
            }
            else {
              // non-floating
              if (fHasExplicitTZ) {
                // item has explicit zone - move timestamp to it (e.g. if timestamps are sent
                // in ISO8601 Z notation, but a TZ/DAYLIGHT or TZID is present)
                tsFldP->moveToContext(tctx,false);
              }
            }
          }
          // special conversions
          if (convmode==CONVMODE_DATE) {
            tsFldP->makeFloating(); // date-only is forced floating
          }
          else if (convmode==CONVMODE_AUTOENDDATE && fMimeDirMode==mimo_old) {
            // check if this could be a 23:59 type end-of-day
            lineartime_t ts = tsFldP->getTimestampAs(fItemTimeContext,&tctx); // get in item context or floating
            lineartime_t ts0 = lineartime2dateonlyTime(ts);
            if (ts0!=ts && AlldayCount(ts0,ts)>0) { // only if not already a 0:00
              // this is a 23:59 type end-of-day, convert it to midnight of next day (AND adjust time context, in case it is now different from original)
              tsFldP->setTimestampAndContext(lineartime2dateonlyTime(ts)+linearDateToTimeFactor,tctx);
            }
          }
        }
        else {
          // read as text
          fldP->setAsString(aText);
        }
      }
      return true; // found

    // Time zones
    case CONVMODE_TZ:
      // parse time zone
      if (ISO8601StrToContext(aText, tctx)!=0) {
        // Note: this is always global for the entire item, so set the item context
        //  (which is then used when parsing dates (which should be delayed to make sure TZ is seen first)
        fItemTimeContext = tctx;
        if (!TCTX_IS_TZ(tctx)) {
          // only offset. Try to symbolize it by passing a DAYLIGHT:FALSE and the offset
          if (TzDaylightToContext("FALSE", fItemTimeContext, tctx, getSessionZones(), fReceiverTimeContext))
            fItemTimeContext = tctx; // there is a symbolized context, keep that
        }
        fHasExplicitTZ = true; // zone explicitly set, not only copied from session's user zone
        goto timecontext;
      }
      return true; // not set, is ok
    case CONVMODE_DAYLIGHT:
      // parse DAYLIGHT zone description property, prefer user zone (among multiple zones matching the Tz/daylight info)
      // - resolve to offset (assuming that item context came from a TZ property, so it will
      //   be one of the non-DST zones, so reftime does not matter)
      tctx = fItemTimeContext;
      TzResolveContext(tctx, getSystemNowAs(TCTX_UTC, getSessionZones()), true, getSessionZones());
      // - now find matching zone for given offset and DAYLIGHT property string
      if (TzDaylightToContext(aText,tctx,tctx,getSessionZones(),fReceiverTimeContext)) {
        // this is always global for the entire item, so set the item context
        // (which is then used when parsing dates (which should be delayed to make sure TZ is seen first)
        fItemTimeContext = tctx;
        fHasExplicitTZ = true; // zone explicitly set, not only copied from session's user zone
        goto timecontext;
      }
      return true; // not set, is ok
    case CONVMODE_TZID:
      // try to get context for named zone
      // - look up in TZIDs we've parsed so far from VTIMEZONE
      tz = fParsedTzidSet.find(aText);
      if (tz!=fParsedTzidSet.end()) {
        tctx = tz->second; // get tctx resolved from VTIMEZONE
        // use tctx for all values from this property
        fPropTZIDtctx = tctx;
        goto timecontext;
      }
      else if (TimeZoneNameToContext(aText, tctx, getSessionZones())) {
        // found valid TZID property, save it so we can use it for all values of this property that don't specify their own TZ
        PDEBUGPRINTFX(DBG_ERROR,("Warning: TZID %s could be resolved against internal name, but appropriate VTIMEZONE is missing",aText));
        fPropTZIDtctx=tctx;
        goto timecontext;
      }
      else {
        PDEBUGPRINTFX(DBG_ERROR,("Invalid TZID value '%s' found (no related VTIMEZONES found and not referring to an internal time zone name)",aText));
      }
      return true; // not set, is ok
    timecontext:
      // if no field, we still have the zone as fItemTimeContext
      if (!fldP) return true; // no field, is ok
      else if (fldP->isBasedOn(fty_timestamp)) {
        // based on timestamp, assign context to that timestamp
        tsFldP = static_cast<TTimestampField *>(fldP);
        tsFldP->setTimeContext(tctx);
      }
      else if (fldP->getCalcType()==fty_integer || !TCTX_IS_TZ(tctx)) {
        // integer field or non-symbolic time zone:
        // assign minute offset as number (calculated for now)
        TzResolveToOffset(tctx, moffs, getSession()->getSystemNowAs(TCTX_UTC), true, getSessionZones());
        fldP->setAsInteger(moffs);
      }
      else {
        // assign symbolic time zone name
        TimeZoneContextToName(tctx, s, getSessionZones());
        fldP->setAsString(s);
      }
      return true;

    case CONVMODE_MULTIMIX:
    case CONVMODE_BITMAP:
      while (*aText && *aText==' ') aText++; // skip leading spaces
      if (convmode==CONVMODE_MULTIMIX) {
        // parse value to determine field
        if (!mixvalparse(aText, offs, isBitMap, n)) return true; // syntax not ok, nop
        fldP = aItem.getArrayField(aFid+offs,aArrIndex);
      }
      else {
        // just bit number
        isBitMap=true;
        if (StrToUShort(aText,n,2)<1) return true; // no integer convertible value, nop
      }
      if (!fldP) return true; // no field, assignment "ok" (=nop)
      if (isBitMap) {
        // store or add to bitmap
        // - get current bitmap value if we have a spearator (means that we can have multiple values)
        if (aConvDefP->combineSep)
          flags=fldP->getAsInteger();
        flags = flags | ((fieldinteger_t)1<<n);
        // - save updated flags
        fldP->setAsInteger(flags);
      }
      else {
        // store as literal
        fldP->setAsString(aText+n);
      }
      return true; // ok
    case CONVMODE_VERSION:
      // version string
      // - return true if correct version string
      return strucmp(aText,aItem.getItemType()->getTypeVers(fProfileMode))==0;
    case CONVMODE_PRODID:
    case CONVMODE_VALUETYPE:
    case CONVMODE_FULLVALUETYPE:
      return true; // simply ignore, always ok
    case CONVMODE_RRULE:
      // helpers
      TTimestampField *tfP;
      TIntegerField *ifP;
      TStringField *sfP;
      if (aFid<0) return true; // no field block, assignment "ok" (=nop)
      // read DTSTART (last=6th field in block) as reference for converting count to end time point
      dtstart=0; // start date/time, as reference
      if (!(tfP = ITEMFIELD_DYNAMIC_CAST_PTR(TTimestampField,fty_timestamp,aItem.getArrayField(aFid+5,aArrIndex)))) return false;
      // TZ and TZID should be applied to dates by now, so dtstart should be in right zone
      dtstart = tfP->getTimestampAs(TCTX_UNKNOWN,&startcontext);
      if (TCTX_IS_UTC(startcontext)) {
        // UTC is probably not the correct zone to resolve weekdays -> convert to item zone
        dtstart = tfP->getTimestampAs(fItemTimeContext,&startcontext);
      }
      // init field block values
      freq='0'; // frequency
      freqmod=' '; // frequency modifier
      interval=0; // unspecified interval
      firstmask=0; // day mask counted from the first day of the period
      lastmask=0; // day mask counted from the last day of the period
      until=0; // last day
      // do the conversion here
      dostore=false;
      if (fMimeDirMode==mimo_old) {
        // vCalendar 1.0 type RRULE
        dostore=RRULE1toInternal(
          aText, // RRULE string to be parsed
          dtstart, // reference date for parsing RRULE
          startcontext,
          freq,
          freqmod,
          interval,
          firstmask,
          lastmask,
          until,
          untilcontext,
          GETDBGLOGGER
        );
      }
      else {
        // iCalendar 2.0 type RRULE
        dostore=RRULE2toInternal(
          aText, // RRULE string to be parsed
          dtstart, // reference date for parsing RRULE
          startcontext,
          freq,
          freqmod,
          interval,
          firstmask,
          lastmask,
          until,
          untilcontext,
          GETDBGLOGGER
        );
      }
      if (dostore) {
        // store values into field block
        // - freq/freqmod
        if (!(sfP = ITEMFIELD_DYNAMIC_CAST_PTR(TStringField,fty_string,aItem.getArrayField(aFid,aArrIndex)))) return false;
        aFid++; // do NOT INCREMENT in macro, as it would get incremented twice
        sfP->assignEmpty();
        if (freq!='0') {
          sfP->appendChar(freq);
          sfP->appendChar(freqmod);
        }
        // - interval
        if (!(ifP = ITEMFIELD_DYNAMIC_CAST_PTR(TIntegerField,fty_integer,aItem.getArrayField(aFid,aArrIndex)))) return false;
        aFid++; // do NOT INCREMENT in macro, as it would get incremented twice
        ifP->setAsInteger(interval);
        // - firstmask
        if (!(ifP = ITEMFIELD_DYNAMIC_CAST_PTR(TIntegerField,fty_integer,aItem.getArrayField(aFid,aArrIndex)))) return false;
        aFid++; // do NOT INCREMENT in macro, as it would get incremented twice
        ifP->setAsInteger(firstmask);
        // - lastmask
        if (!(ifP = ITEMFIELD_DYNAMIC_CAST_PTR(TIntegerField,fty_integer,aItem.getArrayField(aFid,aArrIndex)))) return false;
        aFid++; // do NOT INCREMENT in macro, as it would get incremented twice
        ifP->setAsInteger(lastmask);
        // - until
        if (!(tfP = ITEMFIELD_DYNAMIC_CAST_PTR(TTimestampField,fty_timestamp,aItem.getArrayField(aFid,aArrIndex)))) return false;
        aFid++; // do NOT INCREMENT in macro, as it would get incremented twice
        tfP->setTimestampAndContext(until,untilcontext);
        // - dtstart is not stored, but only read above for reference
        // done
        return true;
      }
      else {
        return false;
      }
      break; // just in case
    default:
      // unknown mode, cannot convert
      return false;
  }
  return false;
} // TMimeDirProfileHandler::MIMEStringToField


// helper for parseMimeDir()
// - parse parameter or property value(list), returns false if no value(list)
bool TMimeDirProfileHandler::parseValue(
  const string &aText,        // string to parse as value (could be binary content)
  const TConversionDef *aConvDefP,
  sInt16 aBaseOffset,         // base offset
  sInt16 aRepOffset,          // repeat offset, adds to aBaseOffset for non-array fields, is array index for array fileds
  TMultiFieldItem &aItem,     // the item where data goes to
  bool &aNotEmpty,            // is set true (but never set false) if property contained any (non-positional) values
  char aSeparator,            // separator between values
  TMimeDirMode aMimeMode,     // MIME mode (older or newer vXXX format compatibility)
  bool aParamValue,           // set if parsing parameter value (different escaping rules)
  bool aStructured,           // set if value consists of multiple values (has semicolon content escaping)
  bool aOnlyDeEscLF           // set if de-escaping only for \n -> LF, but all visible char escapes should be left intact
)
{
  string val,val2;
  char c;
  const char *p;

  // determine field ID
  sInt16 fid=aConvDefP->fieldid;
  if (fid>=0) {
    // value has field where it can be stored
    // - fid is ALWAYS offset by baseoffset
    fid += aBaseOffset;
    // - adjust fid and repoffset (add them and reset aRepOffset if no array field)
    aItem.adjustFidAndIndex(fid,aRepOffset);
    // find out if value exists (available in source and target)
    if (isFieldAvailable(aItem,fid)) {
      // parse only if field available in both source and target
      if (
        (aConvDefP->convmode & CONVMODE_MASK)==CONVMODE_BLOB_B64 ||
        (aConvDefP->convmode & CONVMODE_MASK)==CONVMODE_BLOB_AUTO
      ) {
        // move 1:1 into field
        // - get pointer to leaf field
        TItemField *fldP = aItem.getArrayField(fid,aRepOffset);
        // - directly set field with entire (possiby binary) string content
        if (fldP) fldP->setAsString(aText);
        // parsed successfully
        return true;
      }
      // normal text value, apply de-escaping, charset transformation, value list and enum conversion
      p = aText.c_str(); // start here
      while (*p) {
        // value list loop
        // - get next value
        val.erase();
        while ((c=*p)!=0) {
          // check for field list separator (if field allows list at all)
          if (c==aSeparator && aConvDefP->combineSep) {
            p++; // skip separator
            break;
          }
          // check for escaped chars
          if (c=='\\') {
            p++;
            c=*p;
            if (!c) break; // half escape sequence, ignore
            else if (c=='n' || c=='N') c='\n';
            else if (aOnlyDeEscLF) val+='\\'; // if deescaping only for \n, transfer this non-LF escape into output
            // other escaped chars are shown as themselves
          }
          // add char
          val+=c;
          // next
          p++;
        }
        // find first non-space and number of chars excluding leading and trailing spaces
        const char* valnospc = val.c_str();
        size_t numnospc=val.size();
        while (*valnospc && *valnospc==' ') { valnospc++; numnospc--; }
        while (*(valnospc+numnospc-1)==' ') { numnospc--; }
        // - counts as non-empty if there is a non-empty (and not space-only) value string (even if
        //   it might be converted to empty-value in enum conversion)
        if (*valnospc) aNotEmpty=true;
        // - apply enum translation if any
        const TEnumerationDef *enumP = aConvDefP->findEnumByName(valnospc,numnospc);
        if (enumP) {
          // we have an explicit value (can be default if there is a enm_defaultvalue enum)
          if (enumP->enummode==enm_ignore)
            continue; // do not assign anything, get next value
          else {
            if (enumP->enummode==enm_prefix) {
              // append original value minus prefix to translation
              size_t n=TCFG_SIZE(enumP->enumtext);
              val2.assign(valnospc+n,numnospc-n); // copying from original val
              val=enumP->enumval; // assign the prefix
              val+=val2; // and append the original value sans prefix
            }
            else {
              val=enumP->enumval; // just use translated value
            }
          }
        }
        // assign (or add) value to field
        if (!MIMEStringToField(
          val.c_str(),            // the value text to assign or add to the field
          aConvDefP,              // the conversion definition
          aItem,
          fid,                    // field ID, can be -1
          aRepOffset              // 0 or array index
        )) {
          // field conversion error
          PDEBUGPRINTFX(DBG_ERROR,(
            "TMimeDirProfileHandler::parseValue: MIMEStringToField assignment (fid=%hd, arrindex=%hd) failed",
            fid,
            aRepOffset
          ));
          return false;
        }
      } // while(more chars in value text)
    } // if source and target fields available
    else {
      // show this in log, as most probably it's a remote devInf bug
      PDEBUGPRINTFX(DBG_PARSE,("No value stored for field index %hd because remote indicates not supported in devInf",fid));
    }
  } // if fieldid exists
  else {
    // could be special conversion using no data or data from
    // internal object variables (such as VERSION value)
    if (!MIMEStringToField(
      aText.c_str(),          // the value text to process
      aConvDefP,              // the conversion definition
      aItem,
      FID_NOT_SUPPORTED,
      0
    )) {
      // field conversion error
      PDEBUGPRINTFX(DBG_ERROR,(
        "TMimeDirProfileHandler::parseValue: MIMEStringToField in check mode (no field) failed with val=%s",
        aText.c_str()
      ));
      return false;
    }
  }
  // parsed successfully
  return true;
} // TMimeDirProfileHandler::parseValue



// parse given property
bool TMimeDirProfileHandler::parseProperty(
  cAppCharP &aText, // where to start interpreting property, will be updated past end of what was scanned
  TMultiFieldItem &aItem, // item to store data into
  const TPropertyDefinition *aPropP, // the property definition
  sInt16 *aRepArray,  // array[repeatID], holding current repetition COUNT for a certain nameExts entry
  sInt16 aRepArraySize, // size of array (for security)
  TMimeDirMode aMimeMode, // MIME mode (older or newer vXXX format compatibility)
  cAppCharP aGroupName, // property group ("a" in "a.TEL:131723612")
  size_t aGroupNameLen,
  cAppCharP aFullPropName, // entire property name (excluding group) - might be needed in case of wildcard property match
  size_t aFullNameLen
)
{
  TNameExtIDMap nameextmap;
  const TParameterDefinition *paramP;
  const char *p,*ep,*vp;
  char c;
  string pname;
  string val;
  string unprocessedVal;
  bool defaultparam;
  bool fieldoffsetfound;
  bool notempty = false;
  bool valuelist;
  sInt16 pidx; // parameter index
  TEncodingTypes encoding;
  TCharSets charset;
  // field storage info vars, defaults are used if property has no TPropNameExtension
  sInt16 baseoffset = 0;
  sInt16 repoffset = 0;
  sInt16 maxrep = 1; // no repeat by default
  sInt16 repinc = 1; // inc by 1
  sInt16 repid = -1; // invalid by default
  bool overwriteempty = false; // do not overwrite empty values by default
  bool repoffsByGroup = false;

  // init
  encoding = enc_none; // no encoding by default
  charset = aMimeMode==mimo_standard ? chs_utf8 : fDefaultInCharset; // always UTF8 for real MIME-DIR (same as enclosing SyncML doc), for mimo_old depends on <inputcharset> remote rule option (normally UTF-8)
  nameextmap = 0; // no name extensions detected so far
  fieldoffsetfound = (aPropP->nameExts==NULL); // no first pass needed at all w/o nameExts, just use offs=0
  valuelist = aPropP->valuelist; // cache flag
  // prepare storage as unprocessed value
  if (aPropP->unprocessed) {
    if (aGroupName && *aGroupName) {
      unprocessedVal.assign(aGroupName, aGroupNameLen);
      unprocessedVal += '.';
    }
    unprocessedVal.append(aFullPropName, aFullNameLen);
  }
  // scan parameter list (even if unprocessed, to catch ENCODING and CHARSET)
  do {
    p=aText;
    while (*p==';') {
      // param follows
      defaultparam=false;
      pname.erase();
      p=nextunfolded(p,aMimeMode);
      // parameter expected here
      // - find end of parameter name
      vp=NULL; // no param name found
      for (ep=p; *ep; ep=nextunfolded(ep,aMimeMode)) {
        if (*ep=='=') {
          // param value follows at vp
          vp=nextunfolded(ep,aMimeMode);
          break;
        }
        else if (*ep==':' || *ep==';') {
          // end of parameter name w/o equal sign
          if (aMimeMode!=mimo_old) {
            // only mimo_old allows default params, but as e.g. Nokia Intellisync (Synchrologic) does this completely wrong, we now tolerate it
            POBJDEBUGPRINTFX(getSession(),DBG_ERROR,(
              "Parameter without value: %s - is wrong in MIME-DIR, but we tolerate it and parse as default param name",
              pname.c_str()
            ));
          }
          // treat this as a value of the default parameter (correct syntax in old vCard 2.1/vCal 1.0, wrong in MIME-DIR)
          defaultparam=true; // default param
          // value is equal to param name and starts at p
          vp=p;
          break;
        }
        // add char to param name (unfolded!)
        pname+=*ep;
      }
      if (!vp) {
        POBJDEBUGPRINTFX(getSession(),DBG_ERROR,("parseProperty: bad parameter %s (missing value)",pname.c_str()));
        return false;
      }
      // parameter name & value isolated, pname=name (if not defaultparam), vp points to value
      // - obtain unfolded value
      val.erase();
      bool dquoted = false;
      bool wasdquoted = false;
      // - note: we allow quoted params even with mimo_old, as the chance is much higher that a param value
      //   beginning with doublequote is actually a quoted string than a value containing a doublequote at the beginning
      if (*vp=='"') {
        dquoted = true;
        wasdquoted = true;
        vp=nextunfolded(vp,aMimeMode);
      }
      do {
        c=*vp;
        if (isEndOfLineOrText(c)) break;
        if (dquoted) {
          // within double quoted value, only closing dquote can end it
          if (c=='"') {
            // swallow closing double quote and proceed (next should be end of value anyway)
            vp = nextunfolded(vp,aMimeMode);
            dquoted = false;
            continue;
          }
        }
        else {
          // not within double quoted value
          if (c==':' || c==';') break; // end of value
          // check escaped characters
          if (c=='\\') {
            // escape char, do not check next char for end-of-value (but DO NOT expand \-escaped chars here!!)
            vp=nextunfolded(vp,aMimeMode);
            c=*vp; // get next
            if (c) {
              val+='\\'; // keep the escaped sequence for later when value is actually processed!
            }
            else {
              // half-finished escape at end of value, ignore
              break;
            }
          }
        }
        val+=c;
        // cancel QP softbreaks if encoding is already switched to QP at this point
        vp=nextunfolded(vp,aMimeMode,encoding==enc_quoted_printable);
      } while(true);
      // - processing of next param starts here
      p=vp;
      // check for global parameters
      bool storeUnprocessed = true; // in case this is a unprocessed property, flag will be cleared to prevent storing params that still ARE processed
      if ((aMimeMode==mimo_old && defaultparam) || strucmp(pname.c_str(),"ENCODING")==0) {
        // get encoding
        // Note: always process ENCODING, as QP is mimo-old specific and must be removed for normalized storage
        for (sInt16 k=0; k<numMIMEencodings; k++) {
          if (strucmp(val.c_str(),MIMEEncodingNames[k])==0) {
            encoding=static_cast <TEncodingTypes> (k);
          }
        }
        if (aPropP->unprocessed) {
          if (encoding==enc_quoted_printable)
            storeUnprocessed = false; // QP will be decoded (for unprocessed properties), so param must not be stored
          else
            encoding = enc_none; // other encodings will not be processed for unprocessed properties
        }
      }
      else if (strucmp(pname.c_str(),"CHARSET")==0) {
        // charset specified (mimo_old value-only not supported)
        // Note: always process CHARSET, because non-UTF8 cannot be safely passed to DBs, so we need
        //       to convert in case it's not UTF-8 even for "unprocessed" properties
        sInt16 k;
        for (k=1; k<numCharSets; k++) {
          if (strucmp(val.c_str(),MIMECharSetNames[k])==0) {
            // charset found
            charset=TCharSets(k);
            break;
          }
        }
        if (k>=numCharSets) {
          // unknown charset
          POBJDEBUGPRINTFX(getSession(),DBG_ERROR,("========== WARNING: Unknown Charset '%s'",val.c_str()));
          // %%% replace 8bit chars with underscore
          charset=chs_unknown;
        }
        storeUnprocessed = false; // CHARSET is never included in unprocessed property, as we always store UTF-8
      }
      if (aPropP->unprocessed && storeUnprocessed && fieldoffsetfound) {
        // append in reconstructed form for storing "unprocessed" (= lightly normalized)
        unprocessedVal += ';';
        unprocessedVal += pname;
        if (!defaultparam) {
          unprocessedVal += '=';
          if (wasdquoted) unprocessedVal += '"';
          unprocessedVal += val;
          if (wasdquoted) unprocessedVal += '"';
        }
      }
      // find param in list now
      paramP = aPropP->parameterDefs;
      pidx=0; // parameter index
      while (paramP) {
        // check for match
        if (
          mimeModeMatch(paramP->modeDependency) &&
          #ifndef NO_REMOTE_RULES
          (!paramP->ruleDependency || isActiveRule(paramP->ruleDependency)) &&
          #endif
          ((defaultparam && paramP->defaultparam) || strucmp(pname.c_str(),TCFG_CSTR(paramP->paramname))==0)
        ) {
          // param name found
          // - process value (list)
          if (!fieldoffsetfound) {
            // first pass, check for extendsname parameters
            if (paramP->extendsname) {
              // - for each value in the value list, check if it has a nameextid
              if (!paramP->convdef.enumdefs) {
                DEBUGPRINTFX(DBG_PARSE,(
                  "parseProperty: extendsname param w/o enum : %s;%s",
                  TCFG_CSTR(aPropP->propname),
                  TCFG_CSTR(paramP->paramname)
                ));
                return false;
              }
              // - loop through value list
              ep=val.c_str();
              while (*ep) {
                sInt32 n;
                const char *pp;
                // find end of next value in list
                for (n=0,pp=ep; *pp; pp++) {
                  if (*pp==',') {
                    pp++; // skip the comma
                    break;
                  }
                  n++;
                }
                // search in enums list
                const TEnumerationDef *enumP = paramP->convdef.findEnumByName(ep,n);
                if (enumP && enumP->nameextid>=0) {
                  // set name extension map bit
                  nameextmap |= ((TNameExtIDMap)1<<enumP->nameextid);
                }
                // next value in list
                ep=pp;
              }
            } // if extendsname
          } // first pass
          else {
            // second pass: read param value(s)
            if (!parseValue(
              val,          // input string, possibly binary (e.g. in case of B64 encoded PHOTO)
              &(paramP->convdef),
              baseoffset,   // base offset (as determined by position)
              repoffset,    // repetition offset or array index
              aItem,        // the item where data goes to
              notempty,     // set true if value(s) parsed are not all empty
              defaultparam ? ';' : ',', // value list separator
              aMimeMode,    // MIME mode (older or newer vXXX format compatibility)
              true,         // parsing a parameter
              false,        // no structured value
              false         // normal, full de-escaping
            )) {
              DEBUGPRINTFX(DBG_PARSE,(
                "TMimeDirProfileHandler::parseProperty: %s: value not parsed: %s",
                pname.c_str(),
                val.c_str()
              ));
              return false;
            }
          } // second pass
        } // if (param known)
        // test next param
        paramP=paramP->next;
        pidx++;
      } // while more params
      // p points to ';' of next param or ':' of value
    } // while more parameters (*p==';')
    // check if both passes done or if property storage is explicitly blocked already (baseoffset=-1)
    if (fieldoffsetfound) break;
    // start second pass
    fieldoffsetfound=true;
    // - assume empty to start with
    notempty=false;
    // - prepare for second pass: check if set of param values match
    //   an entry in the nameexts list
    TPropNameExtension *propnameextP = aPropP->nameExts;
    if (propnameextP) {
      repoffsByGroup = false;
      bool dostore = false;
      while (propnameextP) {
        // check if entry matches parsed extendsname param values
        if (
          ((propnameextP->musthave_ids & nameextmap) == propnameextP->musthave_ids) && // needed there
          ((propnameextP->forbidden_ids & nameextmap) == 0) // none of the forbidden ones there
        ) {
          // found match, get offset
          baseoffset=propnameextP->fieldidoffs;
          if (baseoffset==OFFS_NOSTORE) break; // abort with dostore=false
          // check if repeat needed/allowed
          maxrep=propnameextP->maxRepeat;
          if (maxrep==REP_REWRITE) {
            dostore=true; // we can store
            break; // unlimited repeat allowed but stored in same fields (overwrite), no need for index search by group
          }
          // find index where to store this repetition
          repid=propnameextP->repeatID;
          if (repid>=aRepArraySize)
            SYSYNC_THROW(TSyncException(DEBUGTEXT("TMimeDirProfileHandler::parseProperty: repID too high","mdit11")));
          // check if a group ID determines the repoffset (not possible for valuelists)
          if (aPropP->groupFieldID!=FID_NOT_SUPPORTED && !valuelist) {
            // search in group field
            string s;
            bool someGroups = false;
            for (sInt16 n=0; n<maxrep || maxrep==REP_ARRAY; n++) {
              sInt16 g_repoffset = n*propnameextP->repeatInc; // original repeatoffset (not adjusted yet)
              TItemField *g_fldP =  aItem.getArrayFieldAdjusted(aPropP->groupFieldID+baseoffset,g_repoffset,true); // get leaf field, if it exists
              if (!g_fldP) break; // group field for that repetition does not (yet) exist, array exhausted
              // compare group name
              if (g_fldP->isAssigned()) {
                someGroups = someGroups || !g_fldP->isEmpty(); // when we find a non-empty group field, we have at least one group detected
                if (someGroups) {
                  // don't use repetitions already used by SOME of the fields in the group
                  // for auto-assigning new groups (or ungrouped occurrences)
                  if (aRepArray[repid]<n+1)
                    aRepArray[repid] = n+1;
                }
                // check if group matches (only if there is a group at all)
                g_fldP->getAsString(s);
                if (aGroupName && strucmp(aGroupName,s.c_str(),aGroupNameLen)==0) {
                  repoffsByGroup = true;
                  dostore = true;
                  repoffset = g_repoffset;
                  PDEBUGPRINTFX(DBG_PARSE+DBG_EXOTIC,("parseProperty: found group '%s' at repoffset=%d (repcount=%d)",s.c_str(),repoffset,n));
                  break;
                }
              }
            } // for all possible repetitions
            // minrep now contains minimal repetition count for !repoffsByGroup case
          } // if grouped property
          if (!repoffsByGroup) {
            if (aRepArray[repid]<maxrep || maxrep==REP_ARRAY) {
              // not exhausted, we can use this entry
              // - calculate repeat offset to be used
              repinc=propnameextP->repeatInc;
              // note: repArray will be updated below (if property not empty or !overwriteempty)
              dostore=true; // we can store
              do {
                repoffset = aRepArray[repid]*repinc;
                // - set flag if repeat offset should be incremented after storing an empty property or not
                overwriteempty = propnameextP->overwriteEmpty;
                // - check if target property main value is empty (must be, or we will skip that repetition)
                dostore = false; // if no field exists, we do not store
                for (sInt16 e=0; e<aPropP->numValues; e++) {
                  if (aPropP->convdefs[e].fieldid==FID_NOT_SUPPORTED)
                    continue; // no field, no need to check it
                  sInt16 e_fid = aPropP->convdefs[e].fieldid+baseoffset;
                  sInt16 e_rep = repoffset;
                  aItem.adjustFidAndIndex(e_fid,e_rep);
                  // - get base field
                  TItemField *e_basefldP = aItem.getField(e_fid);
                  TItemField *e_fldP = NULL;
                  if (e_basefldP)
                    e_fldP=e_basefldP->getArrayField(e_rep,true); // get leaf field, if it exists
                  if (!e_basefldP || (e_fldP && e_fldP->isAssigned())) {
                    // base field of one of the main fields does not exist or leaf field is already assigned
                    // -> skip that repetition
                    dostore = false;
                    break;
                  }
                  else
                    dostore = true; // at least one field exists, we might store
                }
                // check if we can test more repetitions
                if (!dostore) {
                  if (aRepArray[repid]+1<maxrep || maxrep==REP_ARRAY) {
                    // we can increment and try next repetition
                    aRepArray[repid]++;
                  }
                  else
                    break; // no more possible repetitions with this position rule (check next rule)
                }
              } while (!dostore);
              if (dostore) break; // we can store now
            } // if repeat not yet exhausted
          } // if repoffset no already found
        } // if position rule matches
        // next
        propnameextP=propnameextP->next;
      } // while search for matching nameExts entry
      // abort if we can't store
      if (!dostore) {
        aText=p; // this is what we've read so far
        return false;
      }
    } // if name extension list not empty
    // Now baseoffset/repoffset are valid to be used for storage
  } while(true); // until parameter pass 1 & pass 2 done
  // parameters are all processed by now, decision made to store data (if !dostore, routine exits above)
  // - store the group tag value if we have one
  if (aPropP->groupFieldID!=FID_NOT_SUPPORTED) {
    TItemField *g_fldP =  aItem.getArrayFieldAdjusted(aPropP->groupFieldID+baseoffset,repoffset,false);
    if (g_fldP)
      g_fldP->setAsString(aGroupName,aGroupNameLen); // store the group name (aGroupName might be NULL, that's ok)
  }
  if (aPropP->unprocessed) {
    if (*p==':') {
      // there is a value
      p++;
      // - get entire property value part, not checking for any separators, but converting to appchar (UTF-8) and unfolding
      decodeValue(encoding==enc_quoted_printable ? encoding : enc_none, charset, aMimeMode, 0, 0, p, val);
      // - add it to "unprocessed" value representation
      unprocessedVal += ':';
      unprocessedVal += val;
      // - process this as a whole (de-escaping ONLY CRLFs) and assign to field
      if (!parseValue(
        unprocessedVal,
        &(aPropP->convdefs[0]), // the conversion definition
        baseoffset, // identifies base field
        repoffset,  // repeat offset to base field / array index
        aItem,      // the item where data goes to
        notempty,   // set true if value(s) parsed are not all empty
        0, // no value list separator
        aMimeMode,  // MIME mode (older or newer vXXX format compatibility)
        false,      // no parameter
        false,      // not structured
        true        // only de-escape linefeeds, but nothing else
      )) {
        return false;
      }
    } // if property has a value part
  } // if unprocessed property
  else {
    // - read and decode value(s)
    char sep=':'; // first value starts with colon
    // repeat until we have all values
    for (sInt16 i=0; i<aPropP->numValues || valuelist; i++) {
      if (*p!=sep && (aPropP->altvaluesep==0 || *p!=aPropP->altvaluesep)) {
        #ifdef SYDEBUG
        // Note: for valuelists, this is the normal loop exit case as we are not limited by numValues
        if (!valuelist) {
          // New behaviour: omitting values is ok (needed e.g. for T39m)
          DEBUGPRINTFX(DBG_PARSE,("TMimeDirProfileHandler::parseProperty: %s does not specify all values",TCFG_CSTR(aPropP->propname)));
        }
        #endif
        break; // all available values read
      }
      // skip separator
      p++;
      // get value(list) unfolded
      decodeValue(encoding,charset,aMimeMode,aPropP->numValues > 1 || valuelist ? aPropP->valuesep : 0,aPropP->altvaluesep,p,val);
      // check if we can store, otherwise just read over value
      // - get the conversion def for the value
      TConversionDef *convDef = &(aPropP->convdefs[valuelist ? 0 : i]); // always use convdef[0] for value lists
      // - store value if not a value list (but simple value or part of structured value), or store if
      //   valuelist and repeat not yet exhausted, or if valuelist without repetition but combination separator
      //   which allows to put multiple values into a single field
      if (!valuelist || repoffset<maxrep*repinc || maxrep==REP_ARRAY || (valuelist && convDef->combineSep)) {
        // convert and store value (or comma separated value-list, not to mix with valuelist-property!!)
        if (!parseValue(
          val,
          convDef,
          baseoffset, // identifies base field
          repoffset,  // repeat offset to base field / array index
          aItem,      // the item where data goes to
          notempty,   // set true if value(s) parsed are not all empty
          ',',
          aMimeMode,  // MIME mode (older or newer vXXX format compatibility)
          false,      // no parameter
          aPropP->numValues > 1, // structured if multiple values
          false       // normal, full de-escaping
        )) {
          PDEBUGPRINTFX(DBG_PARSE+DBG_EXOTIC,(
            "TMimeDirProfileHandler::parseProperty: %s: value not parsed: %s",
            TCFG_CSTR(aPropP->propname),
            val.c_str()
          ));
          return false;
        }
        // update repeat offset and repeat count if this is a value list
        if (valuelist && convDef->combineSep==0 && (notempty || !overwriteempty)) {
          // - update count for every non-empty value (for empty values only if overwriteempty is not set)
          if (repid>=0)
            aRepArray[repid]++; // next repetition
          repoffset+=repinc; // also update repeat offset
        }
      }
      else {
        // value cannot be stored
        PDEBUGPRINTFX(DBG_PARSE+DBG_EXOTIC,(
          "TMimeDirProfileHandler::parseProperty: %s: value not stored because repeat exhausted: %s",
          TCFG_CSTR(aPropP->propname),
          val.c_str()
        ));
      }
      // more values must be separated by the value sep char (default=';' but can be ',' e.g. for iCalendar 2.0 CATEGORIES)
      sep = aPropP->valuesep;
    } // for all values
  } // process values
  if (notempty && !valuelist) {
    // at least one of the components is not empty. Make sure all components are "touched" such that
    // in case of arrays, these are assigned even if empty
    for (sInt16 j=0; j<aPropP->numValues; j++) {
      sInt16 fid=aPropP->convdefs[j].fieldid;
      if (fid>=0) {
        // requesting the pointer creates the field if it does not already exist
        aItem.getArrayFieldAdjusted(fid+baseoffset,repoffset,false);
      }
    }
  }
  if (!valuelist && repid>=0 && (notempty || !overwriteempty) && !repoffsByGroup) {
    // we have used this repetition and actually stored values, so count it now
    // (unless we have stored an empty value only and overwriteempty is true, in
    // this case we don't increment, so next value found for this repetition will
    // overwrite empty value
    // Also, if repeat offset was found by group name, don't increment (aRepArray
    // is already updated in this case)
    aRepArray[repid]++;
  }
  // update read pointer past end of what we've scanned (but not necessarily up
  // to next property beginning)
  aText=p;
  // done, ok
  return true;
} // TMimeDirProfileHandler::parseProperty


// parse MIME-DIR from specified string into item
bool TMimeDirProfileHandler::parseMimeDir(const char *aText, TMultiFieldItem &aItem)
{
  // start with empty item
  aItem.cleardata();
  // reset item time zone before parsing
  fHasExplicitTZ = false; // none set explicitly
  fItemTimeContext = fReceiverTimeContext; // default to user context
  fDelayedProps.clear(); // start w/o delayed props
  fParsedTzidSet.clear(); // start w/o time zones
  // start parsing on root level
  if (parseLevels(aText,aItem,fProfileDefinitionP,true)) {
    // make sure all supported (=available) fields are at least empty (but not missing!)
    aItem.assignAvailables();
    return true;
  }
  else
    return false;
} // TMimeDirProfileHandler::parseMimeDir


// parameter string for QP encoding. Needed when skipping otherwise unknown properties
#define QP_ENCODING_PARAM "ENCODING=QUOTED-PRINTABLE"

// parse MIME-DIR level from specified string into item
bool TMimeDirProfileHandler::parseLevels(
  const char *&aText,
  TMultiFieldItem &aItem,
  const TProfileDefinition *aProfileP,
  bool aRootLevel
)
{
  appChar c;
  cAppCharP p, propname, groupname;
  sInt32 n, gn;
  sInt16 foundmandatory=0;
  const sInt16 maxreps = 50;
  sInt16 repArray[maxreps];
  bool atStart = aRootLevel;

  // reset repetition counts
  for (sInt16 k=0; k<maxreps; k++) repArray[k]=0;
  // level is known
  sInt16 disabledLevels=0;
  // set level marker field, if any is defined
  sInt16 fid=aProfileP->levelConvdef.fieldid;
  if (fid>=0) {
    // field defined for level entry
    // - make sure field exists and is assigned empty value at least
    aItem.getFieldRef(fid).assignEmpty();
    const TEnumerationDef *enumP = aProfileP->levelConvdef.enumdefs;
    if (enumP) {
      // if enumdefs, content is set to first enumdef's enumval (NOT enumtext!!)
      aItem.getField(fid)->setAsString(TCFG_CSTR(enumP->enumval));
    }
  }
  // skip possible leading extra LF and CR and whitespace here
  // NOTE: Magically server sends XML CDATA with 0x0D 0x0D 0x0A for example
  while (isspace(*aText)) aText++;
  // parse input text property by property
  do {
    // start of property parsing
    // - reset TZID flag
    fPropTZIDtctx = TCTX_UNKNOWN;
    // - prepare scanning
    p=aText;
    propname = p; // assume name starts at beginning of text
    n = 0;
    groupname = NULL; // assume no group
    gn = 0;
    // determine property name end (and maybe group name)
    do {
      c=*p;
      if (!c) {
        // end of text reached w/o property name
        POBJDEBUGPRINTFX(getSession(),DBG_ERROR,("parseMimeDir: no property name found, text=%s",aText));
        return false;
      }
      if (c==':' || c==';') break;
      // handle grouping
      if (c=='.') {
        // this is a group name (or element of it)
        // - remember the group name
        if (!groupname) groupname = propname;
        gn = p-groupname; // size of groupname
        // - prop name starts after group name (and dot)
        propname = ++p; // skip group
        n = 0;
        continue;
      }
      // next char
      p++; n++;
    } while(true);
    // propname points to start, p points to end of property name, n=name size
    // - search through all properties
    bool propparsed=false;
    // - check for BEGIN and END
    if (strucmp(propname,"BEGIN",n)==0) {
      // BEGIN encountered
      p = propname+n;
      // - skip possible parameters for broken implementations like Intellisync/Synchrologic
      if (*p==';') while (*p && *p!=':') p++;
      // - isolate value
      size_t l=0; const char *lnam=p+1;
      while (*(lnam+l)>=0x20) l++; // calculate length of value
      p=lnam+l; // advance scanning pointer to terminator
      n=0; // prevent false advancing at end of prop loop
      if (atStart) {
        // value must be level name, else this is a bad profile
        if (strucmp(lnam,TCFG_CSTR(aProfileP->levelName),l)!=0) {
          POBJDEBUGPRINTFX(getSession(),DBG_ERROR,("parseMimeDir: root level BEGIN has bad value: %s",aText));
          return false;
        }
        atStart=false; // no special lead-in check any more
        propparsed=true;
      }
      else {
        // value determines new level to enter
        if (disabledLevels==0) {
          // search for sublevel
          const TProfileDefinition *subprofileP = aProfileP->subLevels;
          while (subprofileP) {
            // check
            if (
              mimeModeMatch(subprofileP->modeDependency) &&
              strucmp(lnam,TCFG_CSTR(subprofileP->levelName),l)==0
            ) {
              // sublevel found, process
              while ((uInt8)(*p)<0x20) p++; // advance scanning pointer to beginning of next property
              // check special case first
              if (subprofileP->profileMode==profm_vtimezones) {
                // vTimeZone is handled specially
                string s2;
                string s = "END:";
                s.append(subprofileP->levelName);
                n = s.size(); // size of lead-out
                cAppCharP e = strstr(p,s.c_str());
                if (e==NULL) return false; // unterminated vTimeZone sublevel
                s.assign(p,e-p); // everything between lead-in and lead-out
                p = e+n; // advance pointer beyond VTIMEZONES
                appendStringAsUTF8(s.c_str(), s2, chs_utf8, lem_cstr, false);
                timecontext_t tctx;
                // identify or add this in the session zones
                string tzid;
                if (VTIMEZONEtoInternal(s2.c_str(), tctx, getSessionZones(), getDbgLogger(), &tzid)) {
                  // time zone identified
                  #ifdef SYDEBUG
                  string tzname;
                  TimeZoneContextToName(tctx, tzname, getSessionZones());
                  PDEBUGPRINTFX(DBG_PARSE+DBG_EXOTIC,("parseMimeDir: VTIMEZONE with ID='%s' parsed to internal time zone '%s'",tzid.c_str(),tzname.c_str()));
                  #endif
                  // remember it by original name for TZID parsing
                  fParsedTzidSet[tzid] = tctx;
                }
                else {
                  POBJDEBUGPRINTFX(getSession(),DBG_ERROR,("parseMimeDir: could not parse VTIMEZONE: %s",s.c_str()));
                }
              }
              else {
                // ordinary non-root level
                if (!parseLevels(p,aItem,subprofileP,false)) return false;
              }
              // - now continue on this level
              propparsed=true;
              break;
            }
            // next
            subprofileP=subprofileP->next;
          }
          if (!propparsed) {
            // no matching sublevel found, disable this level
            disabledLevels=1;
          }
        }
        else {
          // already disabled, just nest
          disabledLevels++;
        }
      } // BEGIN not on rootlevel
    } // BEGIN found
    else if (strucmp(propname,"END",n)==0) {
      // END encountered
      p = propname+n;
      // - skip possible parameters for broken implementations like Intellisync/Synchrologic
      if (*p==';') while (*p && *p!=':') p++;
      // - isolate value
      size_t l=0; const char *lnam=p+1;
      while (*(lnam+l)>=0x20) l++; // calculate length of value
      p=lnam+l; // advance scanning pointer to terminator
      n=0; // prevent false advancing at end of prop loop
      // check
      if (disabledLevels>0) {
        // end of a disabled level, just un-nest
        disabledLevels--;
      }
      else {
        // should be end of active level, check name
        if (strucmp(lnam,TCFG_CSTR(aProfileP->levelName),l)!=0) {
          POBJDEBUGPRINTFX(getSession(),DBG_ERROR,("parseMimeDir: unexpected END value: %s",aText));
          return false;
        }
        // correct end of level
        aText=p; // points to terminator, which is correct for end-of-level
        // break scanner loop
        break;
      }
    } // END found
    else if (disabledLevels==0) {
      if (atStart) {
        POBJDEBUGPRINTFX(getSession(),DBG_ERROR,("parseMimeDir: root level does not start with BEGIN: %s",aText));
        return false;
      }
      // not disabled level
      const TPropertyDefinition *propP = aProfileP->propertyDefs;
      #ifndef NO_REMOTE_RULES
      const TPropertyDefinition *otherRulePropP = NULL; // default property which is used if none of the rule-dependent in the group was used
      bool ruleSpecificParsed = false;
      uInt16 propGroup=0; // group identifier (all props with same name have same group ID)
      #endif
      const TPropertyDefinition *parsePropP;
      while(propP) {
        // compare
        if (
          mimeModeMatch(propP->modeDependency) && // none or matching mode dependency
          strwildcmp(propname,TCFG_CSTR(propP->propname),n)==0 // wildcards allowed (for unprocessed properties for example)
        ) {
          // found property def with matching name (and MIME mode)
          // check all in group (=all subsequent with same name)
          #ifndef NO_REMOTE_RULES
          propGroup=propP->propGroup;
          ruleSpecificParsed=false;
          otherRulePropP=NULL;
          while (propP && propP->propGroup==propGroup && propP->propGroup!=0)
          #else
          do
          #endif
          {
            // still in same group (= same name)
            #ifndef NO_REMOTE_RULES
            // check if this property should be used for parsing
            parsePropP=NULL; // do not parse by default
            if (propP->dependsOnRemoterule) {
              // check if depends on current rule
              if (propP->ruleDependency==NULL) {
                // this is the "other"-rule dependent variant
                // - just remember for now
                otherRulePropP=propP;
              }
              else if (isActiveRule(propP->ruleDependency)) {
                // specific for the applied rule
                parsePropP=propP; // default to expand current prop
                // now we have expanded a rule-specific property (blocks parsing of "other"-rule dependent prop)
                ruleSpecificParsed=true;
              }
            }
            else {
              // does not depend on rule, parse anyway
              parsePropP=propP;
            }
            // check if this is last prop of list
            propP=propP->next;
            if (!(propP && propP->propGroup==propGroup) && otherRulePropP && !ruleSpecificParsed) {
              // End of alternatives for parsing this property, no rule-specific parsed yet, and there is a otherRuleProp
              // parse "other"-rule's property instead
              parsePropP=otherRulePropP;
            }
            #else
            // simply parse it
            parsePropP=propP;
            propP=propP->next;
            #endif
            // now parse (or save for delayed parsing later)
            if (parsePropP) {
              if (parsePropP->delayedProcessing) {
                // buffer parameters needed to parse later
                PDEBUGPRINTFX(DBG_PARSE+DBG_EXOTIC,("parseMimeDir: property %s parsing delayed, rank=%hd",TCFG_CSTR(parsePropP->propname),parsePropP->delayedProcessing));
                TDelayedPropParseParams dppp;
                dppp.delaylevel = parsePropP->delayedProcessing;
                dppp.start = p;
                dppp.groupname = groupname;
                dppp.groupnameLen = gn;
                dppp.propDefP = parsePropP;
                TDelayedParsingPropsList::iterator pos;
                for (pos=fDelayedProps.begin(); pos!=fDelayedProps.end(); pos++) {
                  // insert at end or before first occurrence of higer delay
                  if ((*pos).delaylevel>dppp.delaylevel) {
                    fDelayedProps.insert(pos,dppp);
                    break;
                  }
                }
                if (pos==fDelayedProps.end())
                  fDelayedProps.push_back(dppp);
                // update mandatory count (even if we haven't parsed it yet)
                if (parsePropP->mandatory) foundmandatory++;
                // skip for now
                p=propname+n;
                propparsed=true; // but is "parsed" for loop
                break; // parse next
              }
              if (parseProperty(
                p, // where to start interpreting property, will be updated past end of poperty
                aItem, // item to store data into
                parsePropP, // the (matching) property definition
                repArray,
                maxreps,
                fMimeDirMode, // MIME-DIR mode
                groupname,
                gn,
                propname,
                n
              )) {
                // property parsed successfully
                propparsed=true;
                // count mandarory properties found
                if (parsePropP->mandatory) foundmandatory++;
                break; // parse next
              }
              // if not successfully parsed, continue with next property which
              // can have the same name, but possibly different parameter definitions
            } // if parseProp
          } // while same property group (poperties with same name)
          #ifdef NO_REMOTE_RULES
          while(false); // if no remote rules, we do not loop
          #endif
          if (propparsed) break; // do not continue outer loop if inner loop has parsed a prop successfully
        } // if name matches (=start of group found)
        else {
          // not start of group
          // - next property
          propP=propP->next;
        }
      } // while all properties
    } // else: neither BEGIN nor END
    if (!propparsed) {
      // unknown property
      PDEBUGPRINTFX(DBG_PARSE,("parseMimeDir: property not parsed (unknown or not storable): %" FMT_LENGTH(".30") "s",FMT_LENGTH_LIMITED(30,aText)));
      // skip parsed part (the name)
      p=propname+n;
    }
    // p is now end of parsed part
    // - skip rest up to EOLN (=any ctrl char)
    //   Note: we need to check if this is quoted-printable, otherwise we might NOT cancel soft breaks
    bool isqp = false;
    while ((c=*p)!=0) {
      if (isEndOfLineOrText(c)) break; // end of line or string
      if (c==';' && *(p+1)) {
        if (strucmp(p+1, QP_ENCODING_PARAM, strlen(QP_ENCODING_PARAM))==0) {
          c = *(p+1+strlen(QP_ENCODING_PARAM));
          isqp = c==':' || c==';'; // the property is QP encoded, we need to cancel QP softbreaks while looking for end of property
        }
      }
      p=nextunfolded(p,fMimeDirMode,isqp); // cancel soft breaks if we are in QP encoded property
    }
    // - skip entire EOLN (=all control chars in sequence %%%)
    while (*p && (uInt8)(*p)<'\x20') p=nextunfolded(p,fMimeDirMode);
    // set next property start point
    aText=p;
  } while (*aText); // exit if end of string
  // now parse delayed ones (list is in delay order already)
  if (aRootLevel) {
    // process delayed properties only after entire record is parsed (i.e. when we are at root level here)
    TDelayedParsingPropsList::iterator pos;
    for (pos=fDelayedProps.begin(); pos!=fDelayedProps.end(); pos++) {
      p = (*pos).start; // where to start parsing
      PDEBUGPRINTFX(DBG_PARSE+DBG_EXOTIC,(
        "parseMimeDir: now parsing delayed property rank=%hd: %" FMT_LENGTH(".30") "s",
        (*pos).delaylevel,
        FMT_LENGTH_LIMITED(30,(*pos).start)
      ));
      if (parseProperty(
        p, // where to start interpreting property, will be updated past end of property
        aItem, // item to store data into
        (*pos).propDefP, // the (matching) property definition
        repArray,
        maxreps,
        fMimeDirMode, // MIME-DIR mode
        (*pos).groupname,
        (*pos).groupnameLen,
        "X-delayed", 9 // dummy, unprocessed properties must not be parsed in delayed mode!
      )) {
        // count mandarory properties found
        //%%% moved this to when we queue the delayed props, as mandatory count is per-profile
        //if ((*pos).propDefP->mandatory) foundmandatory++;
      }
      else {
        // delayed parsing failed
        PDEBUGPRINTFX(DBG_PARSE,("parseMimeDir: failed delayed parsing of property %" FMT_LENGTH(".30") "s",FMT_LENGTH_LIMITED(30,(*pos).start)));
      }
    }
    // we don't need them any more - clear delayed props
    fDelayedProps.clear();
  }
  // verify integrity
  if (foundmandatory<aProfileP->numMandatoryProperties) {
    // not all mandatory properties found
    POBJDEBUGPRINTFX(getSession(),DBG_ERROR,(
      "parseMimeDir: missing %d of %hd mandatory properies",
      aProfileP->numMandatoryProperties-foundmandatory,
      aProfileP->numMandatoryProperties
    ));
    // unsuccessful parsing
    return false;
  }
  // successful parsing done
  return true;
  // %%%%% NOTE: exactly those fields in aItem should be assigned
  //             which are available in source and target.
  //             possibly this should be done in prepareForSendTo (o.‰) of
  //             MultiFieldItem...
} // TMimeDirProfileHandler::parseLevels


void TMimeDirProfileHandler::getOptionsFromDatastore(void)
{
  // get options datastore if one is related;
  // ignore the session from getSession() here because we need
  // to distinguish between script context and normal sync context
  // (the former has no datastore, the latter has)
  TSyncSession *sessionP = fRelatedDatastoreP ? fRelatedDatastoreP->getSession() : NULL;
  if (sessionP) {
    fReceiverCanHandleUTC = sessionP->fRemoteCanHandleUTC;
    fVCal10EnddatesSameDay = sessionP->fVCal10EnddatesSameDay;
    fReceiverTimeContext = sessionP->fUserTimeContext; // default to user context
    fDontSendEmptyProperties = sessionP->fDontSendEmptyProperties;
    fDefaultOutCharset = sessionP->fDefaultOutCharset;
    fDefaultInCharset = sessionP->fDefaultInCharset;
    fDoQuote8BitContent = sessionP->fDoQuote8BitContent;
    fDoNotFoldContent = sessionP->fDoNotFoldContent;
    fTreatRemoteTimeAsLocal = sessionP->fTreatRemoteTimeAsLocal;
    fTreatRemoteTimeAsUTC = sessionP->fTreatRemoteTimeAsUTC;
    #ifndef NO_REMOTE_RULES
    fActiveRemoteRules = sessionP->fActiveRemoteRules; // copy the list
    #endif
  }
}


// generate Data item (includes header and footer)
void TMimeDirProfileHandler::generateText(TMultiFieldItem &aItem, string &aString)
{
  // get options datastore if one is related
  getOptionsFromDatastore();
  #ifdef SYDEBUG
  PDEBUGPRINTFX(DBG_GEN+DBG_HOT,("Generating...."));
  aItem.debugShowItem(DBG_DATA+DBG_GEN);
  #endif
  // baseclass just generates MIME-DIR
  fBeginEndNesting=0; // no BEGIN out yet
  generateMimeDir(aItem,aString);
  #ifdef SYDEBUG
  if (PDEBUGTEST(DBG_GEN+DBG_USERDATA)) {
    // note, do not use debugprintf because string is too long
    PDEBUGPRINTFX(DBG_GEN,("Generated: "));
    PDEBUGPUTSXX(DBG_GEN+DBG_USERDATA,aString.c_str(),0,true);
  }
  #endif
} // TMimeDirProfileHandler::generateText


// parse Data item (includes header and footer)
bool TMimeDirProfileHandler::parseText(const char *aText, stringSize aTextSize, TMultiFieldItem &aItem)
{
  //#warning "aTextSize must be checked!"
  // get options datastore if one is related
  getOptionsFromDatastore();
  // baseclass just parses MIME-DIR
  fBeginEndNesting = 0; // no BEGIN found yet
  #ifdef SYDEBUG
  if (PDEBUGTEST(DBG_PARSE)) {
    // very detailed, show item being parsed
    PDEBUGPRINTFX(DBG_PARSE+DBG_HOT,("Parsing: "));
    PDEBUGPUTSXX(DBG_PARSE+DBG_USERDATA,aText,0,true);
  }
  #endif
  if (parseMimeDir(aText,aItem)) {
    if (fBeginEndNesting) {
      PDEBUGPRINTFX(DBG_ERROR,("TMimeDirProfileHandler parsing ended with NestCount<>0: %hd",fBeginEndNesting));
      return false; // unmatched BEGIN/END
    }
    #ifdef SYDEBUG
    PDEBUGPRINTFX(DBG_PARSE,("Successfully parsed: "));
    aItem.debugShowItem(DBG_DATA+DBG_PARSE);
    #endif
    return true;
  }
  else {
    PDEBUGPRINTFX(DBG_ERROR,("Failed parsing item"));
    return false;
  }
} // TMimeDirProfileHandler::parseText


bool TMimeDirProfileHandler::parseForProperty(SmlItemPtr_t aItemP, const char *aPropName, string &aString)
{
  if (aItemP && aItemP->data)
    return parseForProperty(smlPCDataToCharP(aItemP->data),aPropName,aString);
  else
    return false;
} // TMimeDirProfileHandler::parseForProperty


// scan Data item for specific property (used for quick type tests)
bool TMimeDirProfileHandler::parseForProperty(const char *aText, const char *aPropName, string &aString)
{
  uInt16 n=strlen(aPropName);
  while (*aText) {
    const char *p=aText;
    // find property end
    do {
      p=nextunfolded(p,fMimeDirMode,true);
    } while ((*p)>=0x20);
    // p now points to property end
    if (strucmp(aText,aPropName,n)==0 && aText[n]==':') {
      aText+=n+1; // start of value
      aString.assign(aText,p-aText); // save value
      return true;
    }
    // find next property beginning
    do {
      p=nextunfolded(p,fMimeDirMode,true);
    } while (*p && ((*p)<0x20));
    // set to beginning of next
    aText=p;
  }
  // not found
  return false;
} // TMimeDirProfileHandler::parseForProperty



// helper for newCTDataPropList
void TMimeDirProfileHandler::enumerateLevels(const TProfileDefinition *aProfileP, SmlPcdataListPtr_t *&aPcdataListPP, const TProfileDefinition *aSelectedProfileP, TMimeDirItemType *aItemTypeP)
{
  // only if mode matches
  if (!mimeModeMatch(aProfileP->modeDependency)) return;
  // add name of this profile if...
  // ...generally enabled for CTCap (shownIfSelectedOnly=false), independent of what other profiles might be selected (e.g. VALARM)
  // ...this is the explicitly selected profile (like VTODO while creating DS 1.2 devinf for the tasks datastor)
  // ...no profile is specifically selected, which means we want to see ALL profiles (like a DS 1.1 vCalendar type outside <datastore>)
  // This means, the only case a name is NOT added are those with those having showlevel="no" when ANOTHER profile is explicitly selected.
  if (!aProfileP->shownIfSelectedOnly || aProfileP==aSelectedProfileP || aSelectedProfileP==NULL) {
    aPcdataListPP = addPCDataStringToList(TCFG_CSTR(aProfileP->levelName),aPcdataListPP);
    // check for special subprofiles
    if (aProfileP->profileMode==profm_vtimezones) {
      // has STANDARD and DAYLIGHT subprofiles
      aPcdataListPP = addPCDataStringToList("STANDARD",aPcdataListPP);
      aPcdataListPP = addPCDataStringToList("DAYLIGHT",aPcdataListPP);
    }
    // add names of subprofiles, if any
    const TProfileDefinition *subprofileP = aProfileP->subLevels;
    while (subprofileP) {
      // If this profile is the selected profile, ALL subprofiles must be shown in all cases (so we pass NULL)
      enumerateLevels(subprofileP,aPcdataListPP,aProfileP==aSelectedProfileP ? NULL : aSelectedProfileP, aItemTypeP);
      // next
      subprofileP=subprofileP->next;
    }
  }
} // TMimeDirProfileHandler::enumerateLevels



// add a CTDataProp item to a CTDataPropList
static void addCTDataPropToListIfNotExists(
  SmlDevInfCTDataPropPtr_t aCTDataPropP, // existing CTDataProp item data structure, ownership is passed to list
  SmlDevInfCTDataPropListPtr_t *aCTDataPropListPP // adress of list root pointer (which points to existing item list or NULL)
)
{
  // add it to the list (but only if we don't already have it)
  while (*aCTDataPropListPP) {
    // check name
    if (strcmp(smlPCDataToCharP(aCTDataPropP->prop->name),smlPCDataToCharP((*aCTDataPropListPP)->data->prop->name))==0) {
      //%%% we can add merging parameters here as well
      // same property already exists, forget this one
      smlFreeDevInfCTDataProp(aCTDataPropP);
      aCTDataPropP = NULL;
      break;
    }
    aCTDataPropListPP = &((*aCTDataPropListPP)->next);
  }
  // if not detected duplicate, add it now
  if (aCTDataPropP) {
    addCTDataPropToList(aCTDataPropP,aCTDataPropListPP);
  }
} // addCTDataPropToListIfNotExists


// add a CTData describing a property (as returned by newDevInfCTData())
// as a new property without parameters to a CTDataPropList
static void addNewPropToListIfNotExists(
  SmlDevInfCTDataPtr_t aPropCTData, // CTData describing property
  SmlDevInfCTDataPropListPtr_t *aCTDataPropListPP // adress of list root pointer (which points to existing item list or NULL)
)
{
  SmlDevInfCTDataPropPtr_t propdataP = SML_NEW(SmlDevInfCTDataProp_t);
  propdataP->param = NULL; // no params
  propdataP->prop = aPropCTData;
  addCTDataPropToListIfNotExists(propdataP, aCTDataPropListPP);
} // addNewPropToListIfNotExists



// helper for newCTDataPropList
void TMimeDirProfileHandler::enumerateProperties(const TProfileDefinition *aProfileP, SmlDevInfCTDataPropListPtr_t *&aPropListPP, const TProfileDefinition *aSelectedProfileP, TMimeDirItemType *aItemTypeP)
{
  // remember start of properties
  // add all properties of this level (if enabled)
  // Note: if this is the explicitly selected (sub)profile, it will be shown under any circumstances
  if  ((!aProfileP->shownIfSelectedOnly || aProfileP==aSelectedProfileP || aSelectedProfileP==NULL) && mimeModeMatch(aProfileP->modeDependency)) {
    if (aProfileP->profileMode==profm_vtimezones) {
      // Add properties of VTIMEZONE here
      addNewPropToListIfNotExists(newDevInfCTData("TZID"),aPropListPP);
      addNewPropToListIfNotExists(newDevInfCTData("DTSTART"),aPropListPP);
      addNewPropToListIfNotExists(newDevInfCTData("RRULE"),aPropListPP);
      addNewPropToListIfNotExists(newDevInfCTData("TZOFFSETFROM"),aPropListPP);
      addNewPropToListIfNotExists(newDevInfCTData("TZOFFSETTO"),aPropListPP);
      addNewPropToListIfNotExists(newDevInfCTData("TZNAME"),aPropListPP);
    }
    else {
      // normal profile defined in config, add properties as defined in profile, avoid duplicates
      const TPropertyDefinition *propP = aProfileP->propertyDefs;
      while (propP) {
        if (propP->showInCTCap && mimeModeMatch(propP->modeDependency)) {
          // - new list entry in CTCap (if property to be shown)
          SmlDevInfCTDataPropPtr_t propdataP = SML_NEW(SmlDevInfCTDataProp_t);
          propdataP->param = NULL; // default to no params
          // - add params, if needed
          SmlDevInfCTDataListPtr_t *nextParamPP = &(propdataP->param);
          const TParameterDefinition *paramP = propP->parameterDefs;
          while(paramP) {
            // check if parameter is enabled for being shown in CTCap
            if (paramP->showInCTCap && mimeModeMatch(paramP->modeDependency)) {
              // For some older 1.1 devices (in particular Nokia 7610), enum values of default params
              // in pre-MIME-DIR must be shown as param NAMES (not enums).
              // But newer 1.2 Nokias like E90 need proper TYPE param with valEnums (when run in 1.2 mode. E90 is fine with 7610 style for 1.1)
              // So: normally (fEnumDefaultPropParams==undefined==-1), we show 7610 style for 1.1 and E90 style for 1.2.
              // <enumdefaultpropparams> and ENUMDEFAULTPROPPARAMS() can be used to control this behaviour when needed
              if (
                paramP->defaultparam &&
                fMimeDirMode==mimo_old &&
                (
                  (getSession()->fEnumDefaultPropParams==-1 && getSession()->getSyncMLVersion()<syncml_vers_1_2) || // auto mode and SyncML 1.1 or older
                  (getSession()->fEnumDefaultPropParams==1) // ..or explicitly enabled
                )
              ) {
                // add the name extending enum values as param names
                TEnumerationDef *enumP = paramP->convdef.enumdefs;
                while(enumP) {
                  if (!TCFG_ISEMPTY(enumP->enumtext) && enumP->enummode==enm_translate) {
                    // create new param list entry
                    nextParamPP = addCTDataToList(newDevInfCTData(TCFG_CSTR(enumP->enumtext)),nextParamPP);
                  }
                  enumP=enumP->next;
                }
              }
              else {
                // - proper parameter with valEnum list
                SmlDevInfCTDataPtr_t paramdataP = newDevInfCTData(TCFG_CSTR(paramP->paramname));
                // - add valenums if any
                SmlPcdataListPtr_t *nextValenumPP = &(paramdataP->valenum);
                TEnumerationDef *enumP = paramP->convdef.enumdefs;
                while(enumP) {
                  if (!TCFG_ISEMPTY(enumP->enumtext) && enumP->enummode==enm_translate) {
                    // create new valenum list entry
                    nextValenumPP = addPCDataStringToList(TCFG_CSTR(enumP->enumtext),nextValenumPP);
                  }
                  enumP=enumP->next;
                }
                // - add it to the params list
                nextParamPP = addCTDataToList(paramdataP,nextParamPP);
              }
            } // if param to be shown
            paramP=paramP->next;
          }
          // - get possible size limit and notruncate flag
          uInt32 sz=0; // no size limit by default
          bool noTruncate=false; // by default, truncation is ok
          TFieldDefinition *fieldDefP = NULL;
          for (sInt16 i=0; i<propP->numValues; i++) {
            sInt16 fid=propP->convdefs[0].fieldid;
            if (fid>=0) {
              // Field type (we need it later when we have a maxsize, which is only allowed together with a datatype in 1.1 DTD)
              if (!fieldDefP)
                fieldDefP = fItemTypeP->getFieldDefinition(fid);
              // Size
              uInt32 fsz = fItemTypeP->getFieldOptions(fid)->maxsize; // only if related datastore (i.e. SyncML context)
              // - smallest non-fieldblock (excludes RRULE-type special conversions), not-unknown and not-unlimited maxsize is used
              if (fieldBlockSize(propP->convdefs[0])==1 && (sz==0 || sz>fsz) && fsz!=FIELD_OPT_MAXSIZE_NONE && sInt32(fsz)!=FIELD_OPT_MAXSIZE_UNKNOWN)
                sz=fsz;
              // If any field requests no truncation, report noTruncate
              if (getSession()->getSyncMLVersion()>=syncml_vers_1_2 && fItemTypeP->getFieldOptions(fid)->notruncate)
                noTruncate=true;
            }
          }
          // - calculate our own maxoccur (value in our field options is not used for now %%%)
          uInt32 maxOccur=0;
          if (getSession()->getSyncMLVersion()>=syncml_vers_1_2) {
            if (propP->nameExts) {
              // name extensions determine repeat count
              TPropNameExtension *extP = propP->nameExts;
              while (extP) {
                if (!extP->readOnly) {
                  if (extP->maxRepeat==REP_ARRAY) {
                    // no limit
                    maxOccur=0; // unlimited
                    break; // prevent other name extensions to intervene
                  }
                  else {
                    // limited number of occurrences, add to count
                    maxOccur+=extP->maxRepeat;
                  }
                }
                // next
                extP=extP->next;
              }
            }
            else {
              // not repeating: property may not occur more than once
              maxOccur=1;
            }
          }
          // - some SyncML 1.0 clients crash when they see type/size
          if (!(getSession()->fShowTypeSzInCTCap10) && getSession()->getSyncMLVersion()<=syncml_vers_1_0) {
            sz = 0; // prevent size/type in SyncML 1.0 (as old clients like S55 crash if it is included)
          }
          // - find out if we need to show the type (before SyncML 1.2, Size MUST be preceeded by DataType)
          //   On the other hand, DataType MUST NOT be used in 1.2 for VersIt types!!!
          cAppCharP dataType=NULL;
          if (sz!=0 && fieldDefP && getSession()->getSyncMLVersion()<syncml_vers_1_2) {
            // we have a size, so we NEED a datatype
            TPropDataTypes dt = devInfPropTypes[fieldDefP->type];
            if (dt==proptype_text) dt=proptype_chr; // SyncML 1.1 does not have "text" type
            if (dt!=proptype_unknown)
              dataType = propDataTypeNames[dt];
          }
          // - add property data descriptor
          propdataP->prop = newDevInfCTData(TCFG_CSTR(propP->propname),sz,noTruncate,maxOccur,dataType);
          if (propP->convdefs && (propP->convdefs->convmode & CONVMODE_MASK)==CONVMODE_VERSION) {
            // special case: add version valenum
            addPCDataStringToList(aItemTypeP->getTypeVers(),&(propdataP->prop->valenum));
          }
          // add it if not already same-named property in the list, otherwise discard it
          addCTDataPropToListIfNotExists(propdataP,aPropListPP);
        } // if to be shown in CTCap
        propP=propP->next;
      } // while properties
    } // normal profile defined in config
    // add properties of other levels
    const TProfileDefinition *subprofileP = aProfileP->subLevels;
    while (subprofileP) {
      // only if the current profile is the selected profile, properties of ALL contained subprofiles will be shown
      // (otherwise, selection might be within the current profile, so we need to pass on the selection)
      enumerateProperties(subprofileP,aPropListPP,aProfileP==aSelectedProfileP ? NULL : aSelectedProfileP, aItemTypeP);
      // next
      subprofileP=subprofileP->next;
    }
  }
} // TMimeDirProfileHandler::enumerateProperties


// helper: enumerate filter properties
void TMimeDirProfileHandler::enumeratePropFilters(const TProfileDefinition *aProfileP, SmlPcdataListPtr_t &aFilterProps, const TProfileDefinition *aSelectedProfileP, TMimeDirItemType *aItemTypeP)
{
  // add all properties of this level (if enabled)
  // Note: if this is the explicitly selected (sub)profile, it will be shown under any circumstances
  if  (!aProfileP->shownIfSelectedOnly || aProfileP==aSelectedProfileP) {
    const TPropertyDefinition *propP = aProfileP->propertyDefs;
    while (propP) {
      if (
        propP->canFilter &&
        (propP->showInCTCap || aProfileP==aSelectedProfileP) &&
        propP->convdefs && (propP->convdefs[0].convmode & CONVMODE_MASK)!=CONVMODE_VERSION && (propP->convdefs[0].convmode & CONVMODE_MASK)!=CONVMODE_PRODID
      ) {
        // Note: properties of explicitly selected (sub)profiles will be shown anyway,
        //       as only purpose of suppressing properties in devInf is to avoid
        //       duplicate listing in case of multiple subprofiles in ONE CTCap.
        // - add property name to filter property list
        addPCDataStringToList(TCFG_CSTR(propP->propname), &aFilterProps);
      } // if to be shown in filterCap
      propP=propP->next;
    }
  }
  // add properties of other levels
  const TProfileDefinition *subprofileP = aProfileP->subLevels;
  while (subprofileP) {
    if (aSelectedProfileP==NULL || subprofileP==aSelectedProfileP) {
      // only if the current profile is the selected profile, filter properties of ALL contained subprofiles will be shown
      enumeratePropFilters(subprofileP,aFilterProps,aProfileP==aSelectedProfileP ? NULL : aSelectedProfileP, aItemTypeP);
    }
    // next
    subprofileP=subprofileP->next;
  }
} // TMimeDirProfileHandler::enumeratePropFilters


#ifdef OBJECT_FILTERING

// Filtering: add keywords and property names to filterCap
void TMimeDirProfileHandler::addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps, TTypeVariantDescriptor aVariantDescriptor, TSyncItemType *aItemTypeP)
{
  // get pointer to selected variant (if none, all variants will be shown)
  const TProfileDefinition *selectedSubprofileP = (const TProfileDefinition *)aVariantDescriptor;
  // get pointer to mimedir item type
  TMimeDirItemType *mimeDirItemTypeP;
  GET_CASTED_PTR(mimeDirItemTypeP,TMimeDirItemType,aItemTypeP,"MIME-DIR profile used with non-MIME-DIR type");
  // add name of all properties that have canFilter attribute set
  enumeratePropFilters(fProfileDefinitionP,aFilterProps,selectedSubprofileP, mimeDirItemTypeP);
} // TMimeDirProfileHandler::addFilterCapPropsAndKeywords

#endif // OBJECT_FILTERING



// generates SyncML-Devinf property list for type
SmlDevInfCTDataPropListPtr_t TMimeDirProfileHandler::newCTDataPropList(TTypeVariantDescriptor aVariantDescriptor, TSyncItemType *aItemTypeP)
{
  TMimeDirItemType *itemTypeP = static_cast<TMimeDirItemType *>(aItemTypeP);
  // get pointer to selected variant (if none, all variants will be shown)
  const TProfileDefinition *selectedSubprofileP = (const TProfileDefinition *)aVariantDescriptor;
  // generate new list
  SmlDevInfCTDataPropListPtr_t proplistP = SML_NEW(SmlDevInfCTDataPropList_t);
  SmlDevInfCTDataPropListPtr_t nextpropP = proplistP;
  // generate BEGIN property
  // - add property contents
  nextpropP->data = SML_NEW(SmlDevInfCTDataProp_t);
  nextpropP->data->param=NULL; // no params
  // - property data descriptor
  SmlDevInfCTDataPtr_t pdataP = newDevInfCTData("BEGIN");
  nextpropP->data->prop = pdataP;
  // - add valenums for all profiles and subprofiles
  SmlPcdataListPtr_t *liststartPP = &pdataP->valenum;
  enumerateLevels(fProfileDefinitionP,liststartPP,selectedSubprofileP,itemTypeP);
  // generate END property
  nextpropP->next = SML_NEW(SmlDevInfCTDataPropList_t);
  nextpropP=nextpropP->next;
  // - add property contents
  nextpropP->data = SML_NEW(SmlDevInfCTDataProp_t);
  nextpropP->data->param=NULL; // no params
  // - property data descriptor
  pdataP = newDevInfCTData("END");
  nextpropP->data->prop = pdataP;
  // - add valenums for all profiles and subprofiles
  liststartPP = &pdataP->valenum;
  enumerateLevels(fProfileDefinitionP,liststartPP,selectedSubprofileP,itemTypeP);
  // generate all other properties of all levels
  nextpropP->next=NULL; // in case no properties are found
  SmlDevInfCTDataPropListPtr_t *propstartPP = &nextpropP->next;
  enumerateProperties(fProfileDefinitionP,propstartPP,selectedSubprofileP,itemTypeP);
  // done
  return proplistP;
} // TMimeDirProfileHandler::newCTDataPropList


// Analyze CTCap part of devInf
bool TMimeDirProfileHandler::analyzeCTCap(SmlDevInfCTCapPtr_t aCTCapP, TSyncItemType *aItemTypeP)
{
  TMimeDirItemType *itemTypeP = static_cast<TMimeDirItemType *>(aItemTypeP);
  // assume all sublevels enabled (as long as we don't get a
  // BEGIN CTCap listing all the available levels.
  //aItemTypeP->setLevelOptions(NULL,true);
  // check details
  SmlDevInfCTDataPropListPtr_t proplistP = aCTCapP->prop;
  if (proplistP) {
    if (!itemTypeP->fReceivedFieldDefs) {
      // there is a propList, and we haven't scanned one already for this type
      // (could be the case for DS 1.2 vCalendar where we get events & tasks separately)
      // so disable all non-mandatory fields first (available ones will be re-enabled)
      for (sInt16 i=0; i<itemTypeP->fFieldDefinitionsP->numFields(); i++) {
        itemTypeP->getFieldOptions(i)->available=false;
      }
      // force mandatory properties to be always "available"
      setfieldoptions(NULL,fProfileDefinitionP,itemTypeP);
    }
    // now we have received fields
    itemTypeP->fReceivedFieldDefs=true;
  }
  while (proplistP) {
    // get property descriptor
    SmlDevInfCTDataPtr_t propP = proplistP->data->prop;
    // see if we have this property in any of the levels
    setfieldoptions(propP,fProfileDefinitionP,itemTypeP);
    // next property in CTCap
    proplistP=proplistP->next;
  } // properties in CTCap
  return true;
} // TMimeDirProfileHandler::analyzeCTCap



// %%%%% dummy for now
bool TMimeDirProfileHandler::setLevelOptions(const char *aLevelName, bool aEnable, TMimeDirItemType *aItemTypeP)
{
  // do it recursively.
  // %%% we need to have a flag somewhere for these
  //     don't we have one already???
  //     YES, its fSubLevelRestrictions (supposedly a bitmask for max 32 levels)
  // %%% checking the levels and ignoring incoming/outgoing items with
  //     wrong levels must be added later
  return true;
} // TMimeDirProfileHandler::setLevelOptions




// enable fields related to aPropP property in profiles recursively
// or (if aPropP is NULL), enable fields of all mandatory properties
void TMimeDirProfileHandler::setfieldoptions(
  const SmlDevInfCTDataPtr_t aPropP, // property to enable fields for, NULL if all mandatory properties should be enabled
  const TProfileDefinition *aProfileP,
  TMimeDirItemType *aItemTypeP
)
{
  // set defaults
  sInt32 propsize = FIELD_OPT_MAXSIZE_NONE;
  sInt32 maxOccur = 0; // none by default
  bool noTruncate=false;
  const char* propname = NULL;
  TFieldOptions *fo;
  // get params from CTCap property definition (if any)
  if (aPropP) {
    // get name of CTCap property
    propname = smlPCDataToCharP(aPropP->name);
    // get possible maxSize
    if (aPropP->maxsize) {
      if (getSession()->fIgnoreDevInfMaxSize) {
        // remote rule flags maxsize as invalid (like in E90), flag it as unknown (but possibly limited)
        propsize = FIELD_OPT_MAXSIZE_UNKNOWN;
      }
      else {
        // treat as valid
        StrToLong(smlPCDataToCharP(aPropP->maxsize),propsize);
      }
    }
    // get possible maxOccur
    if (aPropP->maxoccur) {
      StrToLong(smlPCDataToCharP(aPropP->maxoccur),maxOccur);
    }
    // get possible noTruncate
    if (aPropP->flags & SmlDevInfNoTruncate_f)
      noTruncate=true;
    // check for BEGIN to check for enabled sublevels
    if (strucmp(propname,"BEGIN")==0) {
      // check ValEnums that denote supported levels
      SmlPcdataListPtr_t valenumP = aPropP->valenum;
      if (valenumP) {
        // we HAVE supported BEGINs listed, so disable all levels first
        // and have them individually enabled below according to ValEnums
        setLevelOptions(NULL,false,aItemTypeP);
      }
      while (valenumP) {
        // get sublevel name
        const char *slname = smlPCDataToCharP(valenumP->data);
        setLevelOptions(slname,true,aItemTypeP); // enable this one
        // check next
        valenumP=valenumP->next;
      }
    }
  } // if enabling for specified property
  // enable all fields related to this property and set options
  const TPropertyDefinition *propdefP = aProfileP->propertyDefs;
  sInt16 j,q,i,o,r,bs;
  while (propdefP) {
    // compare
    if (
      (propname==NULL && propdefP->mandatory) ||
      (propname && (strucmp(propname,TCFG_CSTR(propdefP->propname))==0))
    ) {
      // match (or enabling mandatory) -> enable all fields that are related to this property
      // - values
      for (i=0; i<propdefP->numValues; i++) {
        // base field ID
        j=propdefP->convdefs[i].fieldid;
        bs=fieldBlockSize(propdefP->convdefs[i]);
        if (j>=0) {
          // field supported
          TPropNameExtension *pneP = propdefP->nameExts;
          if (pneP) {
            while(pneP) {
              o = pneP->fieldidoffs;
              if (o>=0) {
                r=0;
                // for all repetitions (but only for first if mode is REP_ARRAY
                // or field is an array)
                do {
                  // make entire field block addressed by this convdef available
                  // and set maxoccur/notruncate
                  for (q=0; q<bs; q++) {
                    // flag available
                    fo = aItemTypeP->getFieldOptions(j+o+q);
                    if (fo) fo->available=true;
                  }
                  // set size if specified (only for first field in block)
                  fo = aItemTypeP->getFieldOptions(j+o);
                  if (fo) {
                    if (propsize!=FIELD_OPT_MAXSIZE_NONE) fo->maxsize=propsize;
                    // set maxoccur if specified
                    if (maxOccur!=0) fo->maxoccur=maxOccur;
                    // set noTruncate
                    if (noTruncate) fo->notruncate=true;
                  }
                  // next
                  o+=pneP->repeatInc;
                } while (
                  ++r < pneP->maxRepeat &&
                  pneP->maxRepeat!=REP_ARRAY
                  #ifdef ARRAYFIELD_SUPPORT
                  && !aItemTypeP->getFieldDefinition(j)->array
                  #endif
                );
              }
              pneP=pneP->next;
            }
          }
          else {
            // single variant, non-repeating property
            // make entire field block addressed by this convdef available
            for (q=0; q<bs; q++) { fo = aItemTypeP->getFieldOptions(j+q); if (fo) fo->available=true; }
            // set size if specified
            fo = aItemTypeP->getFieldOptions(j);
            if (fo) {
              if (propsize!=FIELD_OPT_MAXSIZE_NONE) fo->maxsize=propsize;
              // set maxoccur if specified
              if (maxOccur!=0) fo->maxoccur=maxOccur;
              // set noTruncate
              if (noTruncate) fo->notruncate=true;
            }
          }
        }
      } // enable values
      // - parameter values
      const TParameterDefinition *paramdefP = propdefP->parameterDefs;
      while(paramdefP) {
        // base field ID
        j=paramdefP->convdef.fieldid;
        bs=fieldBlockSize(paramdefP->convdef);
        if (j>=0) {
          // field supported
          TPropNameExtension *pneP = propdefP->nameExts;
          if (pneP) {
            while(pneP) {
              o = pneP->fieldidoffs;
              if (o>=0) {
                r=0;
                // for all repetitions
                do {
                  // make entire field block addressed by this convdef available
                  for (q=0; q<bs; q++) { fo = aItemTypeP->getFieldOptions(j+o+q); if (fo) fo->available=true; }
                  // set size if specified
                  fo = aItemTypeP->getFieldOptions(j+o);
                  if (propsize!=FIELD_OPT_MAXSIZE_NONE && fo) fo->maxsize=propsize;
                  // Note: MaxOccur and NoTruncate are not relevant for parameter values
                  // next
                  o+=pneP->repeatInc;
                } while (
                  ++r < pneP->maxRepeat &&
                  pneP->maxRepeat!=REP_ARRAY
                  #ifdef ARRAYFIELD_SUPPORT
                  && !aItemTypeP->getFieldDefinition(j)->array
                  #endif
                );
              }
              pneP=pneP->next;
            }
          }
          else {
            // single variant, non-repeating property
            // make entire field block addressed by this convdef available
            for (q=0; q<bs; q++) { fo=aItemTypeP->getFieldOptions(j+q); if (fo) fo->available=true; }
            // set size if specified
            fo = aItemTypeP->getFieldOptions(j);
            if (propsize!=FIELD_OPT_MAXSIZE_NONE && fo) fo->maxsize=propsize;
          }
        }
        paramdefP=paramdefP->next;
      } // while
    } // if known property
    propdefP=propdefP->next;
  }
  // now enable fields in all subprofiles
  const TProfileDefinition *subprofileP = aProfileP->subLevels;
  while (subprofileP) {
    setfieldoptions(aPropP,subprofileP,aItemTypeP);
    // next
    subprofileP=subprofileP->next;
  }
} // TMimeDirProfileHandler::setfieldoptions



// set mode (for those profiles that have more than one, like MIME-DIR's old/standard)
void TMimeDirProfileHandler::setProfileMode(sInt32 aMode)
{
  fProfileMode = aMode;
  // determine derived mime mode
  switch (aMode) {
    case PROFILEMODE_OLD  : fMimeDirMode=mimo_old; break; // 1 = old = vCard 2.1 / vCalendar 1.0
    default : fMimeDirMode=mimo_standard; break; // anything else = standard = vCard 3.0 / iCalendar 2.0 style
  }
} // TMimeDirProfileHandler::setProfileMode


#ifndef NO_REMOTE_RULES

void TMimeDirProfileHandler::setRemoteRule(const string &aRemoteRuleName)
{
  TSessionConfig *scP = getSession()->getSessionConfig();
  TRemoteRulesList::iterator pos;
  for(pos=scP->fRemoteRulesList.begin();pos!=scP->fRemoteRulesList.end();pos++) {
    if((*pos)->fElementName == aRemoteRuleName) {
      // only this rule and all rules included by it rule must be active
      fActiveRemoteRules.clear();
      activateRemoteRule(*pos);
      break;
    }
  }
} // TMimeDirProfileHandler::setRemoteRule

void TMimeDirProfileHandler::activateRemoteRule(TRemoteRuleConfig *aRuleP)
{
  // activate this rule (similar code as in TSyncSession::checkRemoteSpecifics()
  fActiveRemoteRules.push_back(aRuleP);
  // - apply options that have a value
  //if (aRuleP->fLegacyMode>=0) fLegacyMode = aRuleP->fLegacyMode;
  //if (aRuleP->fLenientMode>=0) fLenientMode = aRuleP->fLenientMode;
  //if (aRuleP->fLimitedFieldLengths>=0) fLimitedRemoteFieldLengths = aRuleP->fLimitedFieldLengths;
  if (aRuleP->fDontSendEmptyProperties>=0) fDontSendEmptyProperties = aRuleP->fDontSendEmptyProperties;
  if (aRuleP->fDoQuote8BitContent>=0) fDoQuote8BitContent = aRuleP->fDoQuote8BitContent;
  if (aRuleP->fDoNotFoldContent>=0) fDoNotFoldContent = aRuleP->fDoNotFoldContent;
  //if (aRuleP->fNoReplaceInSlowsync>=0) fNoReplaceInSlowsync = aRuleP->fNoReplaceInSlowsync;
  if (aRuleP->fTreatRemoteTimeAsLocal>=0) fTreatRemoteTimeAsLocal = aRuleP->fTreatRemoteTimeAsLocal;
  if (aRuleP->fTreatRemoteTimeAsUTC>=0) fTreatRemoteTimeAsUTC = aRuleP->fTreatRemoteTimeAsUTC;
  if (aRuleP->fVCal10EnddatesSameDay>=0) fVCal10EnddatesSameDay = aRuleP->fVCal10EnddatesSameDay;
  //if (aRuleP->fIgnoreDevInfMaxSize>=0) fIgnoreDevInfMaxSize = aRuleP->fIgnoreDevInfMaxSize;
  //if (aRuleP->fIgnoreCTCap>=0) fIgnoreCTCap = aRuleP->fIgnoreCTCap;
  //if (aRuleP->fDSPathInDevInf>=0) fDSPathInDevInf = aRuleP->fDSPathInDevInf;
  //if (aRuleP->fDSCgiInDevInf>=0) fDSCgiInDevInf = aRuleP->fDSCgiInDevInf;
  //if (aRuleP->fUpdateClientDuringSlowsync>=0) fUpdateClientDuringSlowsync = aRuleP->fUpdateClientDuringSlowsync;
  //if (aRuleP->fUpdateServerDuringSlowsync>=0) fUpdateServerDuringSlowsync = aRuleP->fUpdateServerDuringSlowsync;
  //if (aRuleP->fAllowMessageRetries>=0) fAllowMessageRetries = aRuleP->fAllowMessageRetries;
  //if (aRuleP->fStrictExecOrdering>=0) fStrictExecOrdering = aRuleP->fStrictExecOrdering;
  //if (aRuleP->fTreatCopyAsAdd>=0) fTreatCopyAsAdd = aRuleP->fTreatCopyAsAdd;
  //if (aRuleP->fCompleteFromClientOnly>=0) fCompleteFromClientOnly = aRuleP->fCompleteFromClientOnly;
  //if (aRuleP->fRequestMaxTime>=0) fRequestMaxTime = aRuleP->fRequestMaxTime;
  if (aRuleP->fDefaultOutCharset!=chs_unknown) fDefaultOutCharset = aRuleP->fDefaultOutCharset;
  if (aRuleP->fDefaultInCharset!=chs_unknown) fDefaultInCharset = aRuleP->fDefaultInCharset;
  // - possibly override decisions that are otherwise made by session
  //   Note: this is not a single option because we had this before rule options were tristates.
  //if (aRuleP->fForceUTC>0) fRemoteCanHandleUTC=true;
  //if (aRuleP->fForceLocaltime>0) fRemoteCanHandleUTC=false;

  // now recursively activate included rules
  TRemoteRulesList::iterator pos;
  for(pos=aRuleP->fSubRulesList.begin();pos!=aRuleP->fSubRulesList.end();pos++) {
    activateRemoteRule(*pos);
  }
}

// check if given rule (by name, or if aRuleName=NULL by rule pointer) is active
bool TMimeDirProfileHandler::isActiveRule(TRemoteRuleConfig *aRuleP)
{
  TRemoteRulesList::iterator pos;
  for(pos=fActiveRemoteRules.begin();pos!=fActiveRemoteRules.end();pos++) {
    if ((*pos)==aRuleP)
      return true;
  }
  // no match
  return false;
} // TMimeDirProfileHandler::isActiveRule

#endif // NO_REMOTE_RULES


// - check mode
bool TMimeDirProfileHandler::mimeModeMatch(TMimeDirMode aMimeMode)
{
  return
    aMimeMode==numMimeModes || // not dependent on MIME mode
    aMimeMode==fMimeDirMode;
} // TMimeDirProfileHandler::mimeModeMatch



/* end of TMimeDirProfileHandler implementation */


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
uInt16 AlldayCount(lineartime_t aStart, lineartime_t aEnd)
{
  lineartime_t startTime = lineartime2timeonly(aStart);
  if (startTime!=0) return 0; // start not at midnight -> no allday
  lineartime_t endTime = lineartime2timeonly(aEnd);
  if (endTime==0) {
    if (aStart==aEnd) aEnd += linearDateToTimeFactor; // one day
  }
  else if (endTime>= (23*MinsPerHour+59)*SecsPerMin*secondToLinearTimeFactor) {
    // add one minute to make sure we reach into next day
    aEnd += SecsPerMin*secondToLinearTimeFactor;
  }
  else
    return 0; // allday criteria not met
  // now calculate number of days
  return (aEnd-aStart) / linearDateToTimeFactor;
} // AlldayCount


/// @brief checks two timestamps if they represent an all-day event
/// @param[in] aStartFldP start time field
/// @param[in] aEndFldP end time field
/// @param[in] aTimecontext context to use to check allday criteria for all non-floating timestamps
///            or UTC timestamps only (if aContextForUTC is set).
/// @param[in] aContextForUTC if set, context is only applied for UTC timestamps, other non-floatings are checked as-is
/// @return 0 if not allday, x=1..n if allday (spanning x days)
uInt16 AlldayCount(TItemField *aStartFldP, TItemField *aEndFldP, timecontext_t aTimecontext, bool aContextForUTC)
{
  if (!aStartFldP->isBasedOn(fty_timestamp)) return 0;
  if (!aEndFldP->isBasedOn(fty_timestamp)) return 0;
  TTimestampField *startFldP = static_cast<TTimestampField *>(aStartFldP);
  TTimestampField *endFldP = static_cast<TTimestampField *>(aEndFldP);
  // check in specified time zone if originally UTC (or aContextForUTC not set), otherwise check as-is
  timecontext_t tctx;
  lineartime_t start = startFldP->getTimestampAs(!aContextForUTC || TCTX_IS_UTC(startFldP->getTimeContext()) ? aTimecontext : TCTX_UNKNOWN, &tctx);
  lineartime_t end = endFldP->getTimestampAs(!aContextForUTC || TCTX_IS_UTC(endFldP->getTimeContext()) ? aTimecontext : TCTX_UNKNOWN, &tctx);
  return AlldayCount(start,end);
} // AlldayCount


/// @brief makes two timestamps represent an all-day event
/// @param[in/out] aStart start time within the first day, will be set to midnight (00:00:00)
/// @param[in/out] aEnd end time within the last day or at midnight of the next day,
///                will be set to midnight of the next day
/// @param[in]     aDays if>0, this is used to calculate the aEnd timestamp (aEnd input is
///                ignored then)
void MakeAllday(lineartime_t &aStart, lineartime_t &aEnd, sInt16 aDays)
{
  lineartime_t duration = 0;

  // first calculate duration (assuming that even if there's a timezone problem, both
  // timestamps will be affected so duration is still correct)
  if (aDays<=0) {
    // use implicit duration
    duration = aEnd-aStart;
  } else {
    // use explicit duration
    duration = aDays * linearDateToTimeFactor;
  }
  // truncate start to midnight
  aStart = lineartime2dateonlyTime(aStart);
  // calculate timestamp that for sure is in next day
  aEnd = aStart + duration + linearDateToTimeFactor-1; // one unit less than a full day, ensures that 00:00:00 input will remain same day
  // make day-only of next day
  aEnd = lineartime2dateonlyTime(aEnd);
} // MakeAllday


/// @brief makes two timestamp fields represent an all-day event
/// @param[in/out] aStartFldP start time within the first day, will be set to dateonly
/// @param[in/out] aEndFldP end time within the last day or at midnight of the next day, will be set to dateonly of the next day
/// @param[in] aTimecontext context to calculate day boundaries in (if timestamp is not already floating), can be floating to treat in context of start date
/// @param[in] aDays if>0, this is used to calculate the aEnd timestamp (aEnd input is
///            ignored then)
/// @note fields will be made floating and dateonly
void MakeAllday(TItemField *aStartFldP, TItemField *aEndFldP, timecontext_t aTimecontext, sInt16 aDays)
{
  if (!aStartFldP->isBasedOn(fty_timestamp)) return;
  if (!aEndFldP->isBasedOn(fty_timestamp)) return;
  TTimestampField *startFldP = static_cast<TTimestampField *>(aStartFldP);
  TTimestampField *endFldP = static_cast<TTimestampField *>(aEndFldP);
  // adjust in specified time zone (or floating)
  timecontext_t tctx;
  lineartime_t start = startFldP->getTimestampAs(aTimecontext,&tctx);
  // context must match, unless either requested-as-is or timestamp is already floating
  if (tctx!=aTimecontext && !TCTX_IS_UNKNOWN(aTimecontext) && !TCTX_IS_UNKNOWN(tctx)) return; // cannot do anything
  // get end in same context as start is
  lineartime_t end = endFldP->getTimestampAs(tctx);
  // make allday
  MakeAllday(start,end,aDays);
  // store back and floating + dateonly
  tctx = TCTX_UNKNOWN | TCTX_DATEONLY;
  // for output format capable of date-only
  startFldP->setTimestampAndContext(start,tctx);
  endFldP->setTimestampAndContext(end,tctx);
} // MakeAllday



} // namespace sysync


// eof
