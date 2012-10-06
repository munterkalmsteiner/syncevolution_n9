/*
 *  File:         ConfigElement.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TConfigElement
 *    Element of hierarchical configuration
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-11-14 : luz : created
 */

// includes
#include "prefix_file.h"
#include "sysync.h"
#include "configelement.h"
#include "syncappbase.h"
#include "scriptcontext.h"
#include "multifielditem.h"
#include "sysync_crc16.h"
#include "vtimezone.h"


using namespace sysync;

#ifndef RELEASE_VERSION
// if defined, parsing debug info goes to console
//#define CONSDEBUG
#endif

#ifdef CONSDEBUG
#define CONSDBGPRINTF(m) CONSOLEPRINTF(m)

const char * const ParseModeNames[numParseModes] = {
  "element",  // normal, expecting sub-elements
  "nested",   // like pamo_element, but scanning nested elements in same TConfigElement
  "delegated", // I have delegated parsing of a single sub-element of mine to another element (without XML nesting)
  "end",   // expecting end of element
  "endnested", // expecting end of nested element (i.e. will call nestedElementEnd())
  "all",   // read over all content
  "string",   // string, but with all WSP converted to space and removed at beginning an end
  "rawstring", // string without any changes
  "cstring", // string with \\, \t,\n,\r and \xXX escape conversion
  "macrostring", // string which can contain macros to substitute config vars in $xxx or $() format
  #ifdef SCRIPT_SUPPORT
  "script", // tokenized script
  "functiondef", // script function definition
  #endif
  "field",
  "path",  // string is updated such that a filename can be appended directly
  "boolean",
  "tristate",
  "timestamp",
  "timezone",
  "vtimezone",
  "idcode",
  "char",
  "int64",
  "int32",
  "int16",
  "enum1by",
  "enum2by",
  "enum4by"
};
#else
#define CONSDBGPRINTF(m)
#endif



/*
 * Implementation of TConfigElement
 */

/* public TConfigElement members */


TConfigElement::TConfigElement(const char *aElementName, TConfigElement *aParentElementP)
{
  // set element name
  fElementName=aElementName;
  fResolveImmediately=false; // by default, elements do not resolve immediately, but when entire config is read
  // set parent and root element pointers
  fParentElementP=aParentElementP;
  if (fParentElementP) {
    // has parent, get root from parent
    fRootElementP=fParentElementP->getRootElement();
  }
  else {
    // base element cannot be root
    fRootElementP=NULL;
  }
  #ifndef HARDCODED_CONFIG
  // init parsing
  ResetParsing();
  #endif
  fCfgVarExp=0;
} // TConfigElement::TConfigElement


TConfigElement::~TConfigElement()
{
  clear();
} // TConfigElement::~TConfigElement


TSyncAppBase *TConfigElement::getSyncAppBase(void)
{
  return fRootElementP ? fRootElementP->fSyncAppBaseP : NULL;
} // TConfigElement::getSyncAppBase


// - convenience version for getting time
lineartime_t TConfigElement::getSystemNowAs(timecontext_t aContext)
{
  return sysync::getSystemNowAs(aContext,getSyncAppBase()->getAppZones());
} // TConfigElement::getSystemNowAs


#ifndef HARDCODED_CONFIG

// static helper, returns attribute value or NULL if none
const char *TConfigElement::getAttr(const char **aAttributes, const char *aAttrName)
{
  if (!aAttributes) return NULL;
  const char *attname;
  while ((attname=*aAttributes)!=0) {
    if (strucmp(attname,aAttrName)==0) {
      return *(++aAttributes); // found, return value
    }
    aAttributes+=2; // skip value, go to next name
  }
  return NULL; // not found
} // TConfigElement::getAttr


// get attribute value, check for macro expansion
// @param aDefaultExpand : if set, non-recursive expansion is done anyway, otherwise, a "expand" attribute is required
bool TConfigElement::getAttrExpanded(const char **aAttributes, const char *aAttrName, string &aValue, bool aDefaultExpand)
{
  cAppCharP val = getAttr(aAttributes, aAttrName);
  if (!val) return false; // no value
  aValue = val;
  getSyncAppBase()->expandConfigVars(aValue, aDefaultExpand ? 1 : fCfgVarExp, this, getName());
  return true;
} // TConfigElement::getAttrExpanded


// static helper, returns true if attribute has valid (or none, if aOpt) bool value
bool TConfigElement::getAttrBool(const char **aAttributes, const char *aAttrName, bool &aBool, bool aOpt)
{
  const char *v = getAttr(aAttributes,aAttrName);
  if (!v) return aOpt; // not existing, is ok if optional
  return StrToBool(v,aBool);
} // TConfigElement::getAttrBool


// static helper, returns true if attribute has valid (or none, if aOpt) short value
bool TConfigElement::getAttrShort(const char **aAttributes, const char *aAttrName, sInt16 &aShort, bool aOpt)
{
  const char *v = getAttr(aAttributes,aAttrName);
  if (!v) return aOpt; // not existing, is ok if optional
  return StrToShort(v,aShort);
} // TConfigElement::getAttrShort


// static helper, returns true if attribute has valid (or none, if aOpt) short value
bool TConfigElement::getAttrLong(const char **aAttributes, const char *aAttrName, sInt32 &aLong, bool aOpt)
{
  const char *v = getAttr(aAttributes,aAttrName);
  if (!v) return aOpt; // not existing, is ok if optional
  return StrToLong(v,aLong);
} // TConfigElement::getAttrLong


#ifdef SCRIPT_SUPPORT
sInt16 TConfigElement::getFieldIndex(cAppCharP aFieldName, TFieldListConfig *aFieldListP, TScriptContext *aScriptContextP)
{
  // fields or local script variables (if any) can be mapped
  if (aScriptContextP)
    return aScriptContextP->getIdentifierIndex(OBJ_AUTO, aFieldListP,aFieldName);
  else
    return aFieldListP ? aFieldListP->fieldIndex(aFieldName) : VARIDX_UNDEFINED;
}
#else
sInt16 TConfigElement::getFieldIndex(cAppCharP aFieldName, TFieldListConfig *aFieldListP)
{
  // only direct mapping of MultiFieldItem fields
  return aFieldListP ? aFieldListP->fieldIndex(aFieldName) : VARIDX_UNDEFINED;
}
#endif

#endif // HARDCODED_CONFIG



void TConfigElement::clear(void)
{
  // nop
} // TConfigElement::clear


// resolve (finish after all data is parsed)
void TConfigElement::Resolve(bool aLastPass)
{
  #ifndef HARDCODED_CONFIG
  // Only resolve if element was not already resolved when it finished parsing
  // or if it was not parsed at all (that is, it did not appear in the config
  // at all and only contains default values)
  if (!fResolveImmediately || !fCompleted) {
    // try to finally resolve private stuff now that all children are resolved
    localResolve(aLastPass);
  }
  #else
  // hardcoded config is never resolved early
  localResolve(aLastPass);
  #endif
}; // TConfigElement::Resolve


#ifdef SYDEBUG

TDebugLogger *TConfigElement::getDbgLogger(void)
{
  // commands log to session's logger
  TSyncAppBase *appBase = getSyncAppBase();
  return appBase ? appBase->getDbgLogger() : NULL;
} // TConfigElement::getDbgLogger

uInt32 TConfigElement::getDbgMask(void)
{
  TSyncAppBase *appBase = getSyncAppBase();
  if (!appBase) return 0; // no session, no debug
  return appBase->getDbgMask();
} // TConfigElement::getDbgMask

#endif


#ifndef HARDCODED_CONFIG

void TConfigElement::ResetParsing(void)
{
  fChildParser=NULL;
  fParseMode=pamo_element; // expecting elements
  fNest=0; // normal elements start with Nest=0
  fExpectAllNestStart=-1; // no expectAll called yet
  fCompleted=false; // not yet completed parsing
  fTempString.erase(); // nothing accumulated yet
  #ifdef SYSER_REGISTRATION
  fLockedElement=false;
  fHadLockedSubs=false;
  #endif
} // TConfigElement::ResetParsing(void)


// report error
void TConfigElement::ReportError(bool aFatal, const char *aMessage, ...)
{
  const sInt32 maxmsglen=1024;
  char msg[maxmsglen];
  va_list args;

  msg[0]='\0';
  va_start(args, aMessage);
  char *p = msg;
  int n=0;
  // show fatal flag
  if (aFatal) {
    strcat(p,"Fatal: ");
    n=strlen(p);
    p+=n;
  }
  // assemble the message string
  vsnprintf(p, maxmsglen-n, aMessage, args);
  va_end(args);
  // set the message
  TRootConfigElement *rootP = getRootElement();
  if (!rootP)
    SYSYNC_THROW(TConfigParseException("Element without root"));
  rootP->setError(aFatal,msg);
} // TConfigElement::ReportError


// fail in parsing (short form of ReportError)
bool TConfigElement::fail(const char *aMessage, ...)
{
  const sInt32 maxmsglen=1024;
  char msg[maxmsglen];
  va_list args;

  msg[0]='\0';
  va_start(args, aMessage);
  // assemble the message string
  vsnprintf(msg, maxmsglen, aMessage, args);
  va_end(args);
  // report the error
  ReportError(true,msg);
  // skip the rest
  expectAll();
  // return value
  return true;
} // TConfigElement::fail


// start of element, this config element decides who processes this element
bool TConfigElement::startElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  if (fChildParser) {
    // parsing in a sub-level, delegate
    return fChildParser->startElement(aElementName,aAttributes,aLine);
  }
  else {
    CONSDBGPRINTF((
      "'%s' starts in configElement='%s' with parsemode='%s' nest=%hd at source line=%ld",
      aElementName,
      getName(),
      ParseModeNames[fParseMode],
      fNest,
      aLine
    ));
    if (fParseMode==pamo_all) {
      // read over contents, no matter what is inside
      fNest++;
      return true;
    }
    #ifdef SYSER_REGISTRATION
    // check for locked elements or subtrees
    // - copy parent's lock status
    if (getParentElement()) fLockedElement=getParentElement()->fLockedElement;
    // - if not already locked, see if locked section starts with this element
    if (!fLockedElement) {
      getAttrBool(aAttributes,"locked",fLockedElement,true);
    }
    // Perform CRC sum if locked element
    if (fLockedElement) {
      CONSDBGPRINTF((
        "'%s' is locked and is included in LockCRC = 0x%04hX",
        aElementName,
        getRootElement()->getConfigLockCRC()
      ));
      // CRC over opening element name
      getRootElement()->addToLockCRC(aElementName);
      // CRC over all attributes
      if (aAttributes) {
        const char **attrs=aAttributes;
        const char *attelem;
        while ((attelem=*attrs++)!=0) {
          // alternating names and values
          getRootElement()->addToLockCRC(attelem);
        }
      }
    }
    #endif
    // check conditional parsing
    bool condmet=true; // assume parsing
    cAppCharP cond;
    string vv,val;
    // check platform filter
    if (condmet) {
      cAppCharP pf=getAttr(aAttributes,"platform");
      if (pf && strucmp(pf,SYSYNC_PLATFORM_NAME)!=0) {
        // tag is not for this platform, ignore
        condmet=false;
      }
    }
    // check for config var conditionals
    if (condmet) {
      cond=getAttr(aAttributes,"if");
      if (cond) {
        // check for value comparison
        string nam;
        // - determine comparison
        cAppCharP p2,p = cond;
        int cmpres = 2; // 2: invalid, 1: var > cond, -1: var < cond, 0: var=cond
        bool neg=false;
        appChar c;
        while ((c=*p)) {
          p2=p++;
          if (c=='=') {
            if (*p=='=') p++; // = and == are equivalent
            cmpres=0; // must be equal
            break;
          }
          else if (c=='!' && *p=='=') {
            p++;
            cmpres=0;
            neg=true; // must not be equal
            break;
          }
          else if (c=='>') {
            if (*p=='=') {
              p++;
              cmpres=-1; // var >= cond is equal to NOT val < cond
              neg=true;
            }
            else {
              cmpres=1; // var > cond
            }
            break;
          }
          else if (c=='<') {
            if (*p=='=') {
              p++;
              cmpres=1; // var <= cond is equal to NOT val > cond
              neg=true;
            }
            else {
              cmpres=-1; // var < cond
            }
            break;
          }
        }
        // now value is at *p or we have not found a comparison op
        if (cmpres<2) {
          nam.assign(cond,p2-cond);
          val=p;
        }
        else {
          nam=cond;
          val.erase();
        }
        // get var - if it does not exist, comparison always renders false
        if (nam=="version") {
          // special comparison mode for version
          // - parse reference into version components
          uInt16 maj=0,min=0,rev=0,bld=0;
          p = val.c_str();
          do {
            p+=StrToUShort(p,maj); if (*p++!='.') break;
            p+=StrToUShort(p,min); if (*p++!='.') break;
            p+=StrToUShort(p,rev); if (*p++!='.') break;
            p+=StrToUShort(p,bld);
          } while(false);
          // - compare with hexversion
          StringObjPrintf(val,"%02X%02X%02X%02X",maj,min,rev,bld);
          nam="hexversion";
        }
        // get config var to perform condition check
        condmet=getSyncAppBase()->getConfigVar(nam.c_str(),vv);
        if (condmet) {
          // var exists, perform comparison
          if (cmpres>=2) {
            // no comparison, but just boolean check. Non-bool but empty=false, non-bool-non-empty=true
            condmet = !vv.empty();
            if (condmet) StrToBool(vv.c_str(), condmet);
          }
          else {
            int res = strcmp(vv.c_str(),val.c_str());
            res = res>0 ? 1 : (res<0 ? -1 : 0);
            condmet = neg != (cmpres==res);
          }
        }
      }
    }
    // check for ifdef
    if (condmet) {
      cond=getAttr(aAttributes,"ifdef");
      if (cond)
        condmet = getSyncAppBase()->getConfigVar(cond,vv);
    }
    // check for ifndef
    if (condmet) {
      cond=getAttr(aAttributes,"ifndef");
      if (cond)
        condmet = !getSyncAppBase()->getConfigVar(cond,vv);
    }
    // skip this tag if conditions not met
    if (!condmet) {
      // skip everything inside
      CONSDBGPRINTF(("Condition for parsing not met - skipping tag"));
      expectAll();
      return true; // ok, go on
    }
    // Now we know that we must actually parse this tag
    // check configvar expansion mode
    fCfgVarExp=0; // default to automatic (i.e. certain content such as paths or macrostrings will be expanded without "expand" attr)
    cAppCharP cm=getAttr(aAttributes,"expand");
    if (cm) {
      // explicit expand directive for this tag (includes expandable attributes of this tag,
      // but only attributes queried with getAttrExpanded() can expand at all)
      bool b;
      if (strucmp(cm,"single")==0) fCfgVarExp=1;
      else if (StrToBool(cm,b)) fCfgVarExp = b ? 2 : -1;
    }
    // check for use-everywhere special tags
    if (strucmp(aElementName,"configmsg")==0) {
      bool iserr=true;
      cAppCharP msg=getAttr(aAttributes,"error");
      if (!msg) {
        msg=getAttr(aAttributes,"warning");
        iserr=false;
      }
      if (!msg) msg="Error: found <configmsg>";
      ReportError(iserr,msg);
      expectAll();
      return false; // generate error message
    }
    // check if we are re-entering the same object again (trying to overwrite)
    if (fCompleted) {
      ReportError(true,"Duplicate definition of <%s>",aElementName);
      expectAll();
      return false; // not allowed, generate error message
    }
    if (fParseMode==pamo_element || fParseMode==pamo_nested) {
      // expecting element
      fTempString.erase();
      if (localStartElement(aElementName,aAttributes,aLine)) {
        // element known and parsing initialized ok
        #ifdef SYSER_REGISTRATION
        if (fLockedElement && fChildParser==NULL && fParseMode!=pamo_element && fParseMode!=pamo_nested)
          fHadLockedSubs=true; // flag that this element has processed non-child subelements in locked mode
        #endif
        return true;
      }
      else {
        // unknown element: read over all its contents
        ReportError(false,"invalid tag <%s>",aElementName);
        expectAll();
        return false; // element not known, generate error message
      }
    }
    /* %%% moved up to beginning
    else if (fParseMode==pamo_all) {
      // read over contents
      fNest++;
    }
    */
    else {
      ReportError(false,"no XML tag expected here");
    }
    return true;
  }
} // TConfigElement::startElement


// character data of current element
void TConfigElement::charData(const char *aCharData, sInt32 aNumChars)
{
  if (fChildParser) {
    // parsing in a sub-level, delegate
    return fChildParser->charData(aCharData,aNumChars);
  }
  else {
    if (fParseMode==pamo_all /* %%% not needed, I think || fNest>0 */) {
      // just ignore
    }
    else if (fParseMode==pamo_element || fParseMode==pamo_nested || fParseMode==pamo_end || fParseMode==pamo_endnested) {
      // only whitespace allowed here
      while (aNumChars--) {
        if (!isspace(*aCharData++)) {
          ReportError(false,"no character data expected");
          break;
        }
      }
    }
    else {
      // accumulate char data in string
      fTempString.append(aCharData,aNumChars);
    }
  }
} // TConfigElement::charData



// read over all contents of current TConfigElement
void TConfigElement::expectAll(void)
{
  // we are already in an element but have no non-decrementing
  // parse mode set like expectEmpty() or expectString() etc.
  // so nest count must be incremented to balance decrement occurring
  // at next element end.
  fExpectAllNestStart = fParseMode==pamo_nested ? fNest : -1; // remember where we left nested mode and entered ignoring mode
  fNest++;
  fParseMode=pamo_all;
} // TConfigElement::expectAll


// expect Enum element
void TConfigElement::expectEnum(sInt16 aDestSize,void *aPtr, const char * const aEnumNames[], sInt16 aNumEnums)
{
  // save params
  fParseEnumArray = aEnumNames;
  fParseEnumNum = aNumEnums;
  // determine mode and destination
  switch (aDestSize) {
    case 1 :
      fParseMode=pamo_enum1by;
      fResultPtr.fCharP=(char *)aPtr;
      break;
    case 2 :
      fParseMode=pamo_enum2by;
      fResultPtr.fShortP=(sInt16 *)aPtr;
      break;
    case 4 :
      fParseMode=pamo_enum4by;
      fResultPtr.fLongP=(sInt32 *)aPtr;
      break;
    default:
      SYSYNC_THROW(TConfigParseException("expectEnum: invalid enum size"));
  }
} // TConfigElement::expectEnum


// delegate parsing of a single element to another config element
// after processing aElementName and all subtags, processing will return to this object
// (rather than waiting for an end tag in aConfigElemP)
bool TConfigElement::delegateParsingTo(TConfigElement *aConfigElemP, const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  if (aConfigElemP) {
    expectChildParsing(*aConfigElemP);
    fParseMode=pamo_delegated;
    // let child parse the current tag right away
    return aConfigElemP->localStartElement(aElementName,aAttributes,aLine);
  }
  else
    return false; // if we have no delegate, we can't understand this tag
} // TConfigElement::delegateParsingTo


// end of element, returns true when this config element is done parsing
bool TConfigElement::endElement(const char *aElementName, bool aIsDelegated)
{
  if (fChildParser) {
    // parsing in a real or simulated (delegateParsingTo) sub-level
    if (fChildParser->endElement(aElementName,fParseMode==pamo_delegated)) {
      // child has finished parsing
      fChildParser=NULL; // handle next element myself
      if (aIsDelegated) {
        // - we were delegated to process a single element from another element.
        //   So, we nust not continue parsing here, but pass back to parent
        //   for next element
        return true;
      }
      // otherwise, wait here for next element to start (or encosing element to end)
      fParseMode=pamo_element; // expect another element or end of myself
    }
    else {
      // child is still parsing, so am I
      return false;
    }
  }
  else {
    CONSDBGPRINTF((
      "'%s' ends in configElement='%s' with parsemode='%s' nest=%hd, aIsDelegated=%d",
      aElementName,
      getName(),
      ParseModeNames[fParseMode],
      fNest,
      aIsDelegated
    ));
    #ifdef SYSER_REGISTRATION
    // Perform CRC sum if locked element
    if (fLockedElement) {
      // CRC over CharData
      getRootElement()->addToLockCRC(fTempString.c_str());
    }
    #endif
    const char *p; // BCPPB: declaring vars in case does not work.
    size_t n,lnwsp;
    bool hlp;
    timecontext_t tctx;
    // expand macros in string first
    if (fCfgVarExp==0) {
      // auto mode
      fCfgVarExp = fParseMode==pamo_path || fParseMode==pamo_macrostring ? 2 : -1;
    }
    // do config variable expansion now (or not, according to fCfgVarExp)
    getSyncAppBase()->expandConfigVars(fTempString,fCfgVarExp,this,aElementName);
    // now parse
    switch (fParseMode) {
      case pamo_end:
      case pamo_endnested:
        if (fNest>0) {
          // Note: normal empty elements are NOT considered nested elements by default, only if
          //       they request endnested mode explicitly
          if (fParseMode==pamo_endnested)
            nestedElementEnd(); // inform possible parser of nested element that a nested element ends here
          // back to nested
          fParseMode=pamo_nested;
        }
        else
          fParseMode=pamo_element; // back to normal element parsing
        return false; // do not exit
      case pamo_all:
      case pamo_nested:
        if (fNest>0) {
          // not yet finished with startlevel, continue with pamo_all/pamo_nested
          fNest--;
          if (fExpectAllNestStart>0) {
            // we are in expectAll mode within pamo_nested
            if(fNest==fExpectAllNestStart) {
              // reached level where we started ignoring contents before
              fExpectAllNestStart = -1; // processed now
              fParseMode = pamo_nested; // back to nested (but active) parsing, like in Mime-Dir profile
            }
            else {
              // NOP here - do NOT call nestedElementEnd()
            }
          }
          else {
            // end of active nested element
            nestedElementEnd(); // inform possible parser of nested element
            if (fNest==0) {
              // if back on nest level 0, switch to pamo_element
              fParseMode = pamo_element;
            }
          }
          return false; // stay in this element
        }
        // nested or all at level 0 cause handling like pamo_element
      case pamo_element:
        // end of element while looking for elements:
        // this is end of this config element itself
        // - flag completion of this element
        fCompleted=true; // prevents re-entry
        // Resolve if this is element says it is self-contained (no references to other elements
        // that might follow later in the config file)
        if (fResolveImmediately) {
          // resolve element NOW, last pass
          localResolve(true);
        }
        // - return parsing authority to caller
        return true;
      case pamo_cstring:
        // interpret C-type escapes (only \t,\r,\n and \xXX, no octal)
        #ifdef SYSER_REGISTRATION
        if (fHadLockedSubs && !fResultPtr.fStringP->empty())
          ReportError(true,"Duplicate definition of <%s>",aElementName);
        #endif
        fResultPtr.fStringP->erase();
        CStrToStrAppend(fTempString.c_str(), *(fResultPtr.fStringP));
        break;
      case pamo_string:
      case pamo_macrostring:
      case pamo_path:
        #ifdef SYSER_REGISTRATION
        if (fHadLockedSubs && !fResultPtr.fStringP->empty())
          ReportError(true,"Duplicate definition of <%s>",aElementName);
        #endif
        // remove spaces at beginning and end
        fResultPtr.fStringP->erase();
        p = fTempString.c_str();
        // - skip leading spaces
        while (*p && isspace(*p)) ++p;
        // - copy chars and convert all wsp to spaces
        n=0; lnwsp=0;
        while (*p) {
          ++n; // count char
          if (isspace(*p))
            fResultPtr.fStringP->append(" ");
          else {
            fResultPtr.fStringP->append(p,1);
            lnwsp=n; // remember last non-whitespace
          }
          ++p;
        }
        // - remove trailing spaces
        fResultPtr.fStringP->resize(lnwsp);
        // - if path requested, shape it up if needed
        if (fParseMode==pamo_path) {
          makeOSDirPath(*(fResultPtr.fStringP));
        }
        break;
      case pamo_rawstring:
        #ifdef SYSER_REGISTRATION
        if (fHadLockedSubs && !fResultPtr.fStringP->empty())
          ReportError(true,"Duplicate definition of <%s>",aElementName);
        #endif
        (*(fResultPtr.fStringP))=fTempString;
        break;
      case pamo_field:
        #ifdef SCRIPT_SUPPORT
        *(fResultPtr.fShortP) = getFieldIndex(fTempString.c_str(),fFieldListP,fScriptContextP);
        #else
        *(fResultPtr.fShortP) = getFieldIndex(fTempString.c_str(),fFieldListP);
        #endif
        break;
      #ifdef SCRIPT_SUPPORT
      case pamo_script:
      case pamo_functiondef:
        SYSYNC_TRY {
          if (fParseMode==pamo_functiondef)
            TScriptContext::TokenizeAndResolveFunction(getSyncAppBase(),fExpectLine,fTempString.c_str(),(*(fResultPtr.fFuncDefP)));
          else {
            #ifdef SYSER_REGISTRATION
            if (fHadLockedSubs && !fResultPtr.fStringP->empty())
              ReportError(true,"Duplicate definition of <%s>",aElementName);
            #endif
            TScriptContext::Tokenize(getSyncAppBase(),aElementName,fExpectLine,fTempString.c_str(),(*(fResultPtr.fStringP)),fExpectContextFuncs,false,fExpectNoDeclarations);
          }
        }
        SYSYNC_CATCH (exception &e)
          ReportError(true,"Script Error: %s",e.what());
        SYSYNC_ENDCATCH
        break;
      #endif
      case pamo_timestamp:
        // expect timestamp, store as UTC
        if (ISO8601StrToTimestamp(fTempString.c_str(), *(fResultPtr.fTimestampP), tctx)==0)
          ReportError(false,"bad ISO8601 timestamp value");
        if (TCTX_IS_UNKNOWN(tctx)) tctx=TCTX_SYSTEM;
        TzConvertTimestamp(*(fResultPtr.fTimestampP),tctx,TCTX_UTC,getSyncAppBase()->getAppZones());
        break;
      case pamo_timezone:
        // time zone by name
        if (!TimeZoneNameToContext(fTempString.c_str(), *(fResultPtr.fTimeContextP), getSyncAppBase()->getAppZones()))
          ReportError(false,"invalid/unknown timezone name");
        break;
      case pamo_vtimezone:
        // definition of custom time zone in vTimezone format
        if (!VTIMEZONEtoInternal(fTempString.c_str(), tctx, fResultPtr.fGZonesP, getDbgLogger()))
          ReportError(false,"invalid vTimezone defintion");
        break;
      case pamo_boolean:
        if (!StrToBool(fTempString.c_str(),*(fResultPtr.fBoolP)))
          ReportError(false,"bad boolean value");
        break;
      case pamo_tristate:
        if (fTempString.empty() || fTempString=="unspecified" || fTempString=="default")
          *(fResultPtr.fByteP)=-1; // unspecified
        else {
          if (!StrToBool(fTempString.c_str(),hlp))
            ReportError(false,"bad boolean value");
          else
            *(fResultPtr.fByteP)= hlp ? 1 : 0;
        }
        break;
      case pamo_int64:
        if (!StrToLongLong(fTempString.c_str(),*(fResultPtr.fLongLongP)))
          ReportError(false,"bad integer (64bit) value");
        break;
      case pamo_int32:
        if (!StrToLong(fTempString.c_str(),*(fResultPtr.fLongP)))
          ReportError(false,"bad integer (32bit) value");
        break;
      case pamo_char:
        if (fTempString.size()>1)
          ReportError(false,"single char or nothing (=NUL char) expected");
        if (fTempString.size()==0)
        *(fResultPtr.fCharP) = 0;
        else
          *(fResultPtr.fCharP) = fTempString[0];
        break;
      case pamo_idCode:
        // Palm/MacOS-type 4-char code
        if (fTempString.size()!=4)
          ReportError(false,"id code must be exactly 4 characters");
        *(fResultPtr.fLongP) =
          ((uInt32)fTempString[0] << 24) +
          ((uInt32)fTempString[1] << 16) +
          ((uInt16)fTempString[2] << 8) +
          ((uInt8)fTempString[3]);
        break;
      case pamo_int16:
        if (!StrToShort(fTempString.c_str(),*(fResultPtr.fShortP)))
          ReportError(false,"bad integer (16bit) value");
        break;
      case pamo_enum1by:
      case pamo_enum2by:
      case pamo_enum4by:
        sInt16 tempenum;
        if (!StrToEnum(fParseEnumArray,fParseEnumNum,tempenum,fTempString.c_str()))
          ReportError(false,"bad enumeration value '%s'",fTempString.c_str());
        else {
          // now assign enum
          if (fParseMode==pamo_enum1by) *fResultPtr.fCharP = tempenum;
          else if (fParseMode==pamo_enum2by) *fResultPtr.fShortP = tempenum;
          else if (fParseMode==pamo_enum4by) *fResultPtr.fLongP = tempenum;
        }
        break;
      default:
        SYSYNC_THROW(TConfigParseException(DEBUGTEXT("Unknown parse mode","ce1")));
    }
    // normal case: end of simple element parsed at same level as parent
    if (aIsDelegated) {
      // - we were delegated to process a single element from another element.
      //   So, we nust not continue parsing here, but pass back to parent
      //   for next element
      return true;
    }
    else {
      // - expect next element
      fParseMode=pamo_element;
    }
  }
  // end of embedded element, but not of myself
  #ifdef SYSER_REGISTRATION
  #ifdef CONSDEBUG
  if (fLockedElement) {
    CONSDBGPRINTF((
      "'%s' was locked and was included in LockCRC = 0x%04hX",
      aElementName,
      getRootElement()->getConfigLockCRC()
    ));
  }
  #endif
  // - back to parent's lock status (or false if no parent)
  fLockedElement=getParentElement() ? getParentElement()->fLockedElement : false;
  #endif
  // do not return to parent config element
  return false;
} // TConfigElement::endElement



// reset error
void TRootConfigElement::resetError(void)
{
  fError=false;
  fErrorMessage.erase();
} // TRootConfigElement::ResetError


// set error
void TRootConfigElement::setError(bool aFatal, const char *aMsg)
{
  if (!fErrorMessage.empty())
    fErrorMessage += '\n'; // multiple messages on multiple lines
  fErrorMessage.append(aMsg);
  if (aFatal && fFatalError==LOCERR_OK)
    fFatalError=LOCERR_CFGPARSE; // this is a config parse error
  fError=true;
} // TRootConfigElement::setError


// check for error message
const char *TRootConfigElement::getErrorMsg(void)
{
  if (!fError) return NULL;
  return fErrorMessage.c_str();
} // TRootConfigElement::GetErrorMsg


// reset parsing (=reset error and fatal errors)
void TRootConfigElement::ResetParsing(void)
{
  resetError();
  fDocStarted=false;
  fFatalError=LOCERR_OK;
  #ifdef SYSER_REGISTRATION
  fLockCRC=0; // no CRC lock sum yet
  #endif
  TConfigElement::ResetParsing();
} // TRootConfigElement::ResetParsing


#endif

// resolve config tree and catch errors
bool TRootConfigElement::ResolveAll(void)
{
  SYSYNC_TRY {
    Resolve(true);
    return true;
  }
  SYSYNC_CATCH (TConfigParseException &e)
    #ifndef HARDCODED_CONFIG
    ReportError(true,e.what());
    #endif
    return false;
  SYSYNC_ENDCATCH
} // TRootConfigElement::ResolveAll


#ifndef HARDCODED_CONFIG

// start of element, this config element decides who processes this element
bool TRootConfigElement::startElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  if (!fDocStarted) {
    // document not yet started
    if (strucmp(aElementName,XMLCONFIG_DOCNAME)==0) {
      // check version
      const char* vers = getAttr(aAttributes,"version");
      if (!vers) {
        ReportError(true,"Missing version attribute for document");
        expectAll(); // ignore everything
      }
      else if (strucmp(vers,XMLCONFIG_DOCVERSION)!=0) {
        ReportError(true,
          "Bad config document version (expected %s, found %s)",
          XMLCONFIG_DOCVERSION,
          vers
        );
        expectAll(); // ignore everything inside that element
      }
      fDocStarted=true; // started normally
      return true;
    }
    else {
      // invalid element
      return false;
    }
  }
  else {
    return TConfigElement::startElement(aElementName,aAttributes,aLine);
  }
} // TRootConfigElement::startElement


#ifdef SYSER_REGISTRATION
// add config text to locking CRC
void TRootConfigElement::addToLockCRC(const char *aCharData, size_t aNumChars)
{
  size_t n = aNumChars ? aNumChars : strlen(aCharData);
  char c;
  bool lastwasctrl=false;
  while (n--) {
    c=*aCharData++;
    // compact multiple whitespace to single char
    if ((uInt8)c<=' ') {
      if (lastwasctrl) continue;
      lastwasctrl=true;
      // treat them all as spaces
      c=' ';
    }
    else {
      lastwasctrl=false; // this is a non-space
    }
    // add to CRC
    fLockCRC=sysync::sysync_crc16(fLockCRC,(uInt8)c);
  }
} // TRootConfigElement::addToLockCRC
#endif

#endif // no hardcode config


/* end of TConfigElement implementation */

// eof
