/*
 *  File:         scriptcontext.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TScriptContext
 *    Environment to tokenize, prepare and run scripts
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 *  2002-09-11 : luz : created
 *
 *
 */

#ifdef SCRIPT_SUPPORT

#ifndef SCRIPT_CONTEXT_H
#define SCRIPT_CONTEXT_H

// includes
#include "sysync.h"


#include "itemfield.h"
#include "multifielditem.h"


using namespace sysync;

namespace sysync {


/*
 * builtin function definitions
 */

#define PARAM_TYPEMASK 0x1F
#define PARAM_OPT      0x20
#define PARAM_REF      0x40
#define PARAM_ARR      0x80

#define VAL(x)    ( (uInt8)x)
#define OPTVAL(x) (((uInt8)x)+PARAM_OPT)
#define REF(x)    (((uInt8)x)+PARAM_REF)
#define REFARR(x) (((uInt8)x)+PARAM_REF+PARAM_ARR)


// Token definitions

// - Multi-Byte tokens:
//   tldddddddddddddd
//   - t=token ID alphanum.
//   - l=length of additional token data, 0x00..0xFF as one char
//   - dddddd=additional token data

// highest Multibyte token
#define TK_MAX_MULTIBYTE 0x1F

// - literals, dddd=literal string
#define TK_NUMERIC_LITERAL 0x10
#define TK_STRING_LITERAL 0x11
// - variable identifiers, dddd = idddddd
//   where i=room for field index, dddd=identifier
#define TK_IDENTIFIER 0x12
// - script line token, dd = line number 16bit, followed by source text (NUL terminated) if script debug on
#define TK_SOURCELINE 0x13
// - object specifier, d=single object index byte
#define TK_OBJECT 0x14
// - builtin function identifier, d=function index
#define TK_FUNCTION 0x15
// - user defined function identifier, idddddd i=room for function index, ddd=function name
#define TK_USERFUNCTION 0x16
// - built-in context-dependent function
#define TK_CONTEXTFUNCTION 0x17

// - type definition, d=TItemFieldTypes enum
#define TK_TYPEDEF 0x18


// object indices
#define OBJ_AUTO 0 // not specified, is local or target field
#define OBJ_LOCAL 1 // local variable
#define OBJ_TARGET 2 // target field
#define OBJ_REFERENCE 3 // reference field



// - language elements, 0x80..0x9F
#define TK_IF 0x80
#define TK_ELSE 0x81
#define TK_LOOP 0x82
#define TK_BREAK 0x83
#define TK_CONTINUE 0x84
#define TK_RETURN 0x85
#define TK_WHILE 0x86

// - constants
#define TK_EMPTY 0x90
#define TK_UNASSIGNED 0x91
#define TK_TRUE 0x92
#define TK_FALSE 0x93

// - grouping, in the 0x20..0x3F area
#define TK_DOMAINSEP '.'
#define TK_OPEN_PARANTHESIS '('
#define TK_CLOSE_PARANTHESIS ')'
#define TK_LIST_SEPARATOR ','
#define TK_BEGIN_BLOCK 0x30
#define TK_END_BLOCK 0x31
#define TK_END_STATEMENT ';'
#define TK_OPEN_ARRAY 0x32
#define TK_CLOSE_ARRAY 0x33

// - operators, in the 0x40..0x7F range
//   in order of precedence (upper 6 bits = precedence)
#define TK_OP_PRECEDENCE_MASK 0xFC
#define TK_BINOP_MIN 0x44
#define TK_BINOP_MAX 0x68 // excluding assignment %%%

//   Pure unaries
//   - negation
#define TK_BITWISENOT 0x40
#define TK_LOGICALNOT 0x41

//   Binaries (some of them also used as unaries, like MINUS)
//   - multiply, divide
#define TK_MULTIPLY 0x44
#define TK_DIVIDE 0x45
#define TK_MODULUS 0x46
//   - add, subtract
#define TK_PLUS 0x48
#define TK_MINUS 0x49
//   - shift
#define TK_SHIFTLEFT 0x4C
#define TK_SHIFTRIGHT 0x4D
//   - comparison
#define TK_LESSTHAN 0x50
#define TK_GREATERTHAN 0x51
#define TK_LESSEQUAL 0x52
#define TK_GREATEREQUAL 0x53
//   - equality
#define TK_EQUAL 0x54
#define TK_NOTEQUAL 0x55
//   - bitwise AND
#define TK_BITWISEAND 0x58
//   - bitwise XOR
#define TK_BITWISEXOR 0x5C
//   - bitwise OR
#define TK_BITWISEOR 0x60
//   - logical AND
#define TK_LOGICALAND 0x64
//   - logical OR
#define TK_LOGICALOR 0x68

//   Special modifying operator
//   - assignment
#define TK_ASSIGN 0x6C


// local script variable definition
class TScriptVarDef
{
public:
  TScriptVarDef(cAppCharP aName,uInt16 aIdx, TItemFieldTypes aType, bool aIsArray, bool aIsRef, bool aIsOpt);
  ~TScriptVarDef();
  uInt16 fIdx; // index (position in container).
  string fVarName;  // Variable name
  TItemFieldTypes fVarType; // type of variable
  bool fIsArray;
  bool fIsRef; // set if this is a local reference to an existing variable
  bool fIsOpt; // set if this is a optional parameter
}; // TScriptVarDef


// script variables
typedef std::vector<TScriptVarDef *> TVarDefs;


// user defined script function
class TUserScriptFunction
{
public:
  string fFuncName;
  string fFuncDef;
}; // TUserScriptFunction

typedef std::vector<TUserScriptFunction *> TUserScriptList;

// global script config (such as user-defined functions)
class TScriptConfig : public TConfigElement
{
  typedef TConfigElement inherited;
public:
  TScriptConfig(TConfigElement *aParentElementP);
  virtual ~TScriptConfig();
  // properties
  uInt32 fMaxLoopProcessingTime; // seconds
  // special scripts
  // user-defined functions
  TUserScriptList fFunctionScripts;
  // macros (pure texts used while parsing)
  TStringToStringMap fScriptMacros;
  // accessing user defined functions
  string *getFunctionScript(sInt16 aFuncIndex);
  sInt16 getFunctionIndex(cAppCharP aName, size_t aLen);
  virtual void clear();
  void clearmacros() { fScriptMacros.clear(); }; // called when config is read, as then templates are no longer needed
protected:
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void localResolve(bool aLastPass);
}; // TScriptConfig


// script tokenizing error
class TTokenizeException : public TConfigParseException
{
  typedef TConfigParseException inherited;
public:
  TTokenizeException(cAppCharP aScriptName, cAppCharP aMsg1,cAppCharP aScript, uInt16 aIndex, uInt16 aLine);
}; // TTokenizeException


// script resolving or execution error
class TScriptErrorException : public TConfigParseException
{
  typedef TConfigParseException inherited;
public:
  TScriptErrorException(cAppCharP aMsg1, uInt16 aLine, cAppCharP aIdent=NULL);
}; // TScriptErrorException


// folder mapping table
typedef std::vector<string> TMacroArgsArray;


// script execution state stack
typedef enum {
  ssta_statement,
  ssta_block,
  ssta_if,
  ssta_else,
  ssta_chainif,
  ssta_loop,
  ssta_while,
} TScriptState;

typedef struct {
  TScriptState state; // state
  cUInt8P begin; // begin of that state in source
  uInt16 line; // line number
} TScriptStackEntry;

// flow control stack depth
const sInt16 maxstackentries=40;

class TMultiFieldItem;

// script context
class TScriptContext
{
  friend class TScriptVarKey;
public:
  TScriptContext(TSyncAppBase *aAppBaseP, TSyncSession *aSessionP);
  virtual ~TScriptContext();
  // Tokenizing is available without context
  static void Tokenize(TSyncAppBase *aAppBaseP, cAppCharP aScriptName, sInt32 aLine, cAppCharP aScriptText, string &aTScript, const TFuncTable *aContextFuncs, bool aFuncHeader=false, bool aNoDeclarations=false, TMacroArgsArray *aMacroArgsP=NULL);
  static void TokenizeAndResolveFunction(TSyncAppBase *aAppBaseP, sInt32 aLine, cAppCharP aScriptText, TUserScriptFunction &aFuncDef);
  // resolve identifiers in a script, if there is a context passed at all
  static void resolveScript(TSyncAppBase *aAppBaseP, string &aTScript,TScriptContext *&aCtxP, TFieldListConfig *aFieldListConfigP);
  // rebuild a script context for a script, if the script is not empty
  // - Script must already be resolved with ResolveIdentifiers
  // - If context already exists, adds new locals to existing ones
  // - If aBuildVars is set, buildVars() will be called after rebuilding variable definitions
  static void rebuildContext(TSyncAppBase *aAppBaseP, string &aTScript,TScriptContext *&aCtxP, TSyncSession *aSessionP=NULL, bool aBuildVars=false);
  // - link a script into a contect with already instantiated variables. This is e.g. for activating remoterule scripts
  static void linkIntoContext(string &aTScript,TScriptContext *aCtxP, TSyncSession *aSessionP);
  // Build the local variables according to definitions (clears existing vars first)
  static void buildVars(TScriptContext *&aCtxP);
  // execute a script if there is a context for it
  bool static execute(
    TScriptContext *aCtxP,
    const string &aTScript,
    const TFuncTable *aFuncTableP, // context's function table, NULL if none
    void *aCallerContext, // free pointer possibly having a meaning for context functions
    TMultiFieldItem *aTargetItemP=NULL, // target (or "loosing") item
    bool aTargetWritable=true, // if set, target item may be modified
    TMultiFieldItem *aReferenceItemP=NULL, // reference for source (or "old" or "winning") item
    bool aRefWritable=false, // if set, reference item may also be written
    bool aNoDebug=false // if set, debug output is suppressed even if DBG_SCRIPTS is generally on
  );
  // execute a script returning a boolean if there is a context for it
  bool static executeTest(
    bool aDefaultAnswer, // result if no script is there or script returns no value
    TScriptContext *aCtxP,
    const string &aTScript,
    const TFuncTable *aFuncTableP, // context's function table, NULL if none
    void *aCallerContext, // free pointer possibly having a meaning for context functions
    TMultiFieldItem *aTargetItemP=NULL, // target (or "loosing") item
    bool aTargetWritable=true, // if set, target item may be modified
    TMultiFieldItem *aReferenceItemP=NULL, // reference for source (or "old" or "winning") item
    bool aRefWritable=false // if set, reference item may also be written
  );
  // execute a script returning a result if there is a context for it
  bool static executeWithResult(
    TItemField *&aResultField, // can be default result or NULL, will contain result or NULL if no result
    TScriptContext *aCtxP,
    const string &aTScript,
    const TFuncTable *aFuncTableP, // context's function table, NULL if none
    void *aCallerContext, // free pointer possibly having a meaning for context functions
    TMultiFieldItem *aTargetItemP, // target (or "loosing") item
    bool aTargetWritable, // if set, target item may be modified
    TMultiFieldItem *aReferenceItemP, // reference for source (or "old" or "winning") item
    bool aRefWritable, // if set, reference item may also be written
    bool aNoDebug=false // if set, debug output is suppressed even if DBG_SCRIPTS is generally on
  );
  // get info
  uInt16 getScriptLine(void) { return line; };
  // get context pointers
  TSyncSession *getSession(void) { return fSessionP; };
  TSyncAppBase *getSyncAppBase(void) { return fAppBaseP; };
  GZones *getSessionZones(void);
  #ifdef SYDEBUG
  TDebugLogger *getDbgLogger(void);
  uInt32 getDbgMask(void);
  #endif
  // Reset context (clear all variables and definitions)
  void clear(void);
  // clear all fields (local variables), but not their definitions
  void clearFields(void);
  // Resolve local variable declarations, references and field references
  void ResolveIdentifiers(
    string &aTScript,
    TFieldListConfig *aFieldListConfigP,
    bool aRebuild=false,
    string *aFuncNameP=NULL,
    bool aNoNewLocals=false // if set, declaration of new locals will be suppressed
  );
  // Prepare local variables (create local fields), according to definitions (re-)built with ResolveIdentifiers()
  bool PrepareLocals(void);
  // Execute script
  bool ExecuteScript(
    const string &aTScript,
    TItemField **aResultPP, // if not NULL, a result field will be returned here (must be deleted by caller)
    bool aAsFunction, // if set, this is a function call
    const TFuncTable *aFuncTableP, // context's function table, NULL if none
    void *aCallerContext, // free pointer possibly having a meaning for context functions
    TMultiFieldItem *aTargetItemP, // target (or "loosing") item
    bool aTargetWritable, // if set, target item may be modified
    TMultiFieldItem *aReferenceItemP, // reference for source (or "old" or "winning") item
    bool aRefWritable, // if set, reference item may also be written
    bool aNoDebug=false // if set, debug output is suppressed even if DBG_SCRIPTS is generally on
  );
  // context variable access
  // - get variable definition, NULL if none defined yet
  TScriptVarDef *getVarDef(cAppCharP aVarName,size_t len=0);
  TScriptVarDef *getVarDef(sInt16 aLocalVarIdx);
  // - get identifier index (>=0: field index, <0: local var)
  //   returns VARIDX_UNDEFINED for unknown identifier
  sInt16 getIdentifierIndex(sInt16 aObjIndex, TFieldListConfig *aFieldListConfigP, cAppCharP aIdentifier,size_t aLen=0);
  // - get field by fid, can also be field of aItem, also resolves arrays
  TItemField *getFieldOrVar(TMultiFieldItem *aItemP, sInt16 aFid, sInt16 aArrIdx);
  // - get field by fid, can also be field of aItem
  TItemField *getFieldOrVar(TMultiFieldItem *aItemP, sInt16 aFid);
  // - get local var by local index
  TItemField *getLocalVar(sInt16 aVarIdx);
  // - set local var by local index (used for passing references)
  void setLocalVar(sInt16 aVarIdx, TItemField *aFieldP);
  sInt16 getNumLocals(void) { return fNumVars; };
  // - parameters
  sInt16 getNumParams(void) { return fNumParams; };
  // - caller's context data pointer (opaque, for use by context functions)
  void *getCallerContext(void) { return fCallerContext; };
private:
  // syncappbase link, must always exist
  TSyncAppBase *fAppBaseP;
  // session link, can be NULL
  TSyncSession *fSessionP;
  // local variable definitions (used in resolve phase)
  TVarDefs fVarDefs;
  // actually instantiated local variable fields (size of fFieldsP array, used at execution)
  // Note: might differ from fVarDefs when new vars have been defined, but not instantiated yet)
  uInt16 fNumVars;
  // function properties (if context is a function context)
  uInt16 fNumParams; // this determines the number of parameter locals
  TItemFieldTypes fFuncType; // return type of function
  // The local variables and field references
  TItemField **fFieldsP;
  // script parsing
  // - variables
  sInt16 skipping; // skipping
  uInt16 line,nextline; // line numbers
  cUInt8P bp; // start of script
  cUInt8P ep; // end of script
  cUInt8P p; // cursor, start at beginning
  cUInt8P np; // next token
  #ifdef SYDEBUG
  const char *scriptname; // name of script
  const char *linesource; // start of embedded source for current line
  bool executing; // set if executing (not resolving)
  bool debugon; // set if debug enabled
  bool inComment; // for colorizer
  #endif
  // - helpers
  void initParse(const string &aTScript, bool aExecuting=false); // init parsing variables
  uInt8 gettoken(void); // get next token
  void reusetoken(void); // re-use last token fetched with gettoken()
  // script execution
  // - flow control state stack
  sInt16 fStackEntries;
  TScriptStackEntry fScriptstack[maxstackentries];
  // - context information
public:
  //   Items
  TMultiFieldItem *fTargetItemP;
  bool fTargetWritable;
  TMultiFieldItem *fReferenceItemP;
  bool fRefWritable;
  //   Link to calling script context (for function contexts)
  TScriptContext *fParentContextP;
private:
  //   Function table
  const TFuncTable *fFuncTableP; // caller context's function table
  void *fCallerContext; // free pointer possibly having a meaning for context functions
  // - helpers
  void pushState(TScriptState aNewState, cUInt8P aBegin=NULL, uInt16 aLine=0);
  void popState(TScriptState aCurrentStateExpected);
  bool getVarField(TItemField *&aItemFieldP);
  void evalParams(TScriptContext *aFuncContextP);
  TItemField *evalTerm(TItemFieldTypes aResultType);
  void evalExpression(
    TItemField *&aResultFieldP, // result (created new if passed NULL, modified and casted if passed a field)
    bool aShowResult=true, // if set, this is the main expression (and we want to see the result in DBG)
    TItemField *aLeftTermP=NULL, // if not NULL, chain-evaluate rest of expression according to aBinaryOp and aPreviousOp. WILL BE CONSUMED
    uInt8 *aBinaryOpP=NULL, // operator to be applied between term passed in aLeftTermP and next term, will receive next operator that has same or lower precedence than aPreviousOp
    uInt8 aPreviousOp=0 // if an operator of same or lower precedence than this is found, expression evaluation ends
  );
  void defineBuiltInVars(const TBuiltInFuncDef *aFuncDefP);
  void executeBuiltIn(TItemField *&aTermP, const TBuiltInFuncDef *aFuncDefP);
  #ifdef SYDEBUG
  void showVarDefs(cAppCharP aTxt);
  #endif
}; // TScriptContext


#ifdef ENGINEINTERFACE_SUPPORT


// key for access to script context variables
class TScriptVarKey :
  public TItemFieldKey
{
  typedef TItemFieldKey inherited;
public:
  TScriptVarKey(TEngineInterface *aEngineInterfaceP, TScriptContext *aScriptContext) :
    inherited(aEngineInterfaceP),
    fScriptContext(aScriptContext), // may be NULL, no vars will be accessible then but no crash occurs
    fIterator(0)
  {};

protected:

  // methods to actually access a TItemField
  virtual sInt16 getFidFor(cAppCharP aName, stringSize aNameSz);
  virtual TItemField *getBaseFieldFromFid(sInt16 aFid);
  virtual bool getFieldNameFromFid(sInt16 aFid, string &aFieldName);

  // the script context
  TScriptContext *fScriptContext;
  // value iterator
  sInt16 fIterator;

}; // TScriptVarKey


#endif // ENGINEINTERFACE_SUPPORT


} // namespace sysync

#endif // SCRIPT_CONTEXT_H

#endif // SCRIPT_SUPPORT

/* eof */
