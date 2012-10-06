/*
 *  File:         ConfigElement.h
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

#ifndef ConfigElement_H
#define ConfigElement_H

#include "prefix_file.h"

#include "sysync.h"
#include "syncexception.h"
#include "sysync_globs.h"

namespace sysync {


// forward
class TConfigElement;
class TDebugLogger;


// parse modes
typedef enum {
  pamo_element,  // normal, expecting sub-elements
  pamo_nested,   // like pamo_element, but scanning nested elements in same TConfigElement
  pamo_delegated, // I have delegated parsing of a single sub-element of mine to another element (without XML nesting)
  pamo_end,   // expecting end of element (empty element)
  pamo_endnested, // expecting end of nested element (i.e. will call nestedElementEnd())
  pamo_all,   // read over all content
  pamo_string,
  pamo_cstring,
  pamo_rawstring,
  pamo_macrostring, // string which can contain macros to substitute config vars in $xxx or $() format
  #ifdef SCRIPT_SUPPORT
  pamo_script,
  pamo_functiondef,
  #endif
  pamo_field, // field ID
  pamo_path,  // string is updated such that a filename can be appended directly, otherwise like pamo_macrostring
  pamo_boolean,
  pamo_tristate,
  pamo_timestamp,
  pamo_timezone, // time zone name
  pamo_vtimezone, // time zone definition in vTimezone format
  pamo_idCode,
  pamo_char,
  pamo_int64,
  pamo_int32,
  pamo_int16,
  pamo_enum1by,
  pamo_enum2by,
  pamo_enum4by,
  numParseModes
} TCfgParseMode;



class TConfigParseException : public TSyncException
{
  typedef TSyncException inherited;
public:
  TConfigParseException(const char *aMsg1) : TSyncException(aMsg1) {};
}; // TConfigParseException


class TRootConfigElement; // forward
class TFieldListConfig;
#ifdef SCRIPT_SUPPORT
class TUserScriptFunction;
class TScriptContext;
#endif

class TSyncAppBase; // forward

class TConfigElement
{
public:
  // constructor
  TConfigElement(const char *aElementName, TConfigElement *aParentElementP);
  virtual ~TConfigElement();
  virtual void clear(void); // remove all dynamic elements, reset to default values
  // - called when all parsing is done, intended for cross-element links
  void Resolve(bool aLastPass);
  // - called when app should save its persistent state
  virtual void saveAppState(void) { /* nop by default */};
  #ifndef HARDCODED_CONFIG
  // Sax parsing
  // - report parsing error (may only be called from within parsing methods)
  void ReportError(bool aFatal, const char *aMessage, ...);
  bool fail(const char *aMessage, ...);
  // - start of element, this config element decides who processes this element
  virtual bool startElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  // - character data of current element
  void charData(const char *aCharData, sInt32 aNumChars);
  // - end of element, returns true when this config element is done parsing
  bool endElement(const char *aElementName, bool aIsDelegated=false);
  #endif
  // access to nodes
  // - root node
  TRootConfigElement *getRootElement(void) { return fRootElementP; };
  // - parent node, NULL if none
  TConfigElement *getParentElement(void) { return fParentElementP; };
  // - get sync app base (application root)
  TSyncAppBase *getSyncAppBase(void);
  // - convenience version for getting time
  lineartime_t getSystemNowAs(timecontext_t aContext);
  // - name
  const char *getName(void) { return fElementName.c_str(); };
  // Public properties
  // - name of element
  string fElementName;
  #ifndef HARDCODED_CONFIG
  // helpers for attributes
  static const char *getAttr(const char **aAttributes, const char *aAttrName);
  bool getAttrExpanded(const char **aAttributes, const char *aAttrName, string &aValue, bool aDefaultExpand);
  static bool getAttrBool(const char **aAttributes, const char *aAttrName, bool &aBool, bool aOpt=false);
  static bool getAttrShort(const char **aAttributes, const char *aAttrName, sInt16 &aShort, bool aOpt=false);
  static bool getAttrLong(const char **aAttributes, const char *aAttrName, sInt32 &aLong, bool aOpt=false);
  #ifdef SCRIPT_SUPPORT
  static sInt16 getFieldIndex(cAppCharP aFieldName, TFieldListConfig *aFieldListP, TScriptContext *aScriptContextP=NULL);
  #else
  static sInt16 getFieldIndex(cAppCharP aFieldName, TFieldListConfig *aFieldListP);
  #endif
  // public helpers
  bool delegateParsingTo(TConfigElement *aConfigElemP, const char *aElementName, const char **aAttributes, sInt32 aLine);
  void expectChildParsing(TConfigElement &aConfigElem)
    { aConfigElem.ResetParsing(); fChildParser=&aConfigElem; }; // let child element parse contents
  void expectAll(void);
  void expectEmpty(bool nested=false) { fParseMode = nested ? pamo_endnested : pamo_end; };
  #endif
  // locked config sections support
  #ifdef SYSER_REGISTRATION
  bool fLockedElement; // set while parsing locked element
  bool fHadLockedSubs; // set if this element contained locked non-child subelements (will prevent string overwrites)
  #endif
protected:
  #ifdef SYDEBUG
  TDebugLogger *getDbgLogger(void);
  uInt32 getDbgMask(void);
  #endif
  // - called after parsing is finished, should resolve links and throws on error
  virtual void localResolve(bool /* aLastPass */) { }; // nop
  #ifndef HARDCODED_CONFIG
  // parsing
  virtual void ResetParsing(void);
  // - checks if element known and decides what to do
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine) { return false; } // unknown element
  // - called after nested level ends
  virtual void nestedElementEnd(void) {};
  // - helpers
  void expectNested(void) { fParseMode=pamo_nested; };
  void startNestedParsing()
    { fNest++; expectNested(); } // let same element parse nested levels
  void expectString(string &aString) { fParseMode=pamo_string; fResultPtr.fStringP=&aString; };
  void expectMacroString(string &aString) { fParseMode=pamo_macrostring; fResultPtr.fStringP=&aString; };
  void expectCString(string &aString) { fParseMode=pamo_cstring; fResultPtr.fStringP=&aString; };
  void expectRawString(string &aString) { fParseMode=pamo_rawstring; fResultPtr.fStringP=&aString; };
  #ifdef SCRIPT_SUPPORT
  void expectScript(string &aTScript,sInt32 aLine, const TFuncTable *aContextFuncs, bool aNoDeclarations=false) { fParseMode=pamo_script; fResultPtr.fStringP=&aTScript; fExpectLine=aLine; fExpectContextFuncs=aContextFuncs; fExpectNoDeclarations=aNoDeclarations; };
  void expectFunction(TUserScriptFunction &aFuncDef,sInt32 aLine=0) { fParseMode=pamo_functiondef; fResultPtr.fFuncDefP=&aFuncDef; fExpectLine=aLine; };
  #endif
  void expectPath(string &aString) { fParseMode=pamo_path; fResultPtr.fStringP=&aString; };
  void expectBool(bool &aBool) { fParseMode=pamo_boolean; fResultPtr.fBoolP=&aBool; };
  void expectTristate(sInt8 &aTristate) { fParseMode=pamo_tristate; fResultPtr.fByteP=&aTristate; };
  void expectTimestamp(lineartime_t &aTimestamp) { fParseMode=pamo_timestamp; fResultPtr.fTimestampP=&aTimestamp; };
  void expectTimezone(timecontext_t &aTimeContext) { fParseMode=pamo_timezone; fResultPtr.fTimeContextP=&aTimeContext; };
  void expectVTimezone(GZones *aZonesToAddTo) { fParseMode=pamo_vtimezone; fResultPtr.fGZonesP=aZonesToAddTo; };
  void expectIdCode(uInt32 &aLong) { fParseMode=pamo_idCode; fResultPtr.fLongP=(sInt32 *)(&aLong); };
  void expectChar(char &aChar) { fParseMode=pamo_char; fResultPtr.fCharP=(char *)(&aChar); };
  void expectInt64(sInt64 &aLongLong) { fParseMode=pamo_int64; fResultPtr.fLongLongP=&aLongLong; };
  void expectInt32(sInt32 &aLong) { fParseMode=pamo_int32; fResultPtr.fLongP=&aLong; };
  void expectUInt32(uInt32 &aLong) { fParseMode=pamo_int32; fResultPtr.fLongP=(sInt32 *)(&aLong); };
  void expectInt16(sInt16 &aShort) { fParseMode=pamo_int16; fResultPtr.fShortP=&aShort; };
  void expectUInt16(uInt16 &aShort) { fParseMode=pamo_int16; fResultPtr.fShortP=(sInt16 *)(&aShort); };
  void expectEnum(sInt16 aDestSize,void *aPtr, const char * const aEnumNames[], sInt16 aNumEnums);
  #ifdef SCRIPT_SUPPORT
  void expectFieldID(sInt16 &aShort, TFieldListConfig *aFieldListP, TScriptContext *aScriptContextP=NULL)
    { fParseMode=pamo_field; fResultPtr.fShortP=&aShort; fFieldListP=aFieldListP; fScriptContextP=aScriptContextP; };
  #else
  void expectFieldID(sInt16 &aShort, TFieldListConfig *aFieldListP)
    { fParseMode=pamo_field; fResultPtr.fShortP=&aShort; fFieldListP=aFieldListP; };
  #endif
  #endif
  // config tree branches (childs)
  TRootConfigElement *fRootElementP; // root element
  sInt16 fNest; // nesting count for unprocessed or nested (in same TConfigElement) XML tags
  sInt16 fExpectAllNestStart; // nesting count where expectAll() was called while in pamo_nested mode
  bool fResolveImmediately;
private:
  TConfigElement *fParentElementP; // parent of this element
  #ifndef HARDCODED_CONFIG
  // parsing
  TConfigElement *fChildParser; // child parser, NULL if none (this is parsing)
  string fTempString; // string to accumulate chars
  TCfgParseMode fParseMode;
  sInt32 fExpectLine; // line number of where expected data started
  TFieldListConfig *fFieldListP; // field list for expectField
  #ifdef SCRIPT_SUPPORT
  const TFuncTable *fExpectContextFuncs; // context-specific function table
  TScriptContext *fScriptContextP; // script context for expectField
  bool fExpectNoDeclarations; // flag for tokenizer
  #endif
  // - parse params
  const char * const * fParseEnumArray;
  sInt16 fParseEnumNum;
  bool fCompleted; // set when entire element has been scanned ok (not in progress any more)
  sInt8 fCfgVarExp; // config var expansion mode: -1 = no, 0=auto (depending on expectXXX), 1=yes, 2=yes with recursion
  // - where to store result (must be in accordance with fParseMode
  union {
    string *fStringP;
    sInt64 *fLongLongP;
    sInt32 *fLongP;
    char *fCharP;
    sInt16 *fShortP;
    bool *fBoolP;
    sInt8 *fByteP;
    lineartime_t *fTimestampP;
    GZones *fGZonesP;
    timecontext_t *fTimeContextP;
    #ifdef SCRIPT_SUPPORT
    TUserScriptFunction *fFuncDefP;
    #endif
  } fResultPtr;
  #endif // free config
}; // TConfigElement


class TRootConfigElement : public TConfigElement
{
  typedef TConfigElement inherited;
  friend class TConfigElement;
public:
  TRootConfigElement(TSyncAppBase *aSyncAppBaseP) : TConfigElement("root",NULL) { fSyncAppBaseP=aSyncAppBaseP; fRootElementP=this; };
  bool ResolveAll(void);
  #ifndef HARDCODED_CONFIG
  void resetError(void);
  const char *getErrorMsg(void);
  localstatus getFatalError(void) { return fFatalError; };
  void setError(bool aFatal, const char *aMsg);
  void setFatalError(localstatus aLocalFatalError) { fFatalError=aLocalFatalError; };
  virtual void ResetParsing(void);
  // - start of element, this config element decides who processes this element
  virtual bool startElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
protected:
  // Sync app base
  TSyncAppBase *fSyncAppBaseP;
  #ifndef HARDCODED_CONFIG
  // global parsing error handling
  bool fError; // error occurred
  localstatus fFatalError; // set<>0 if parsing should be aborted
  string fErrorMessage; // error message
public:
  // section locking support
  #ifndef SYSER_REGISTRATION
  // - dummy
  uInt16 getConfigLockCRC(void) { return 0; };
  #else
  // - get current config lock CRC
  uInt16 getConfigLockCRC(void) { return fLockCRC; };
  // - add config text to locking CRC
  void addToLockCRC(const char *aCharData, size_t aNumChars=0);
private:
  uInt16 fLockCRC; // accumulated CRC of all locked elements
  #endif
private:
  bool fDocStarted;
  #endif
};


} // namespace sysync

#endif  // ConfigElement_H

// eof
