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
 */

#include "prefix_file.h"

#ifdef SCRIPT_SUPPORT

// includes
#include "scriptcontext.h"

#include "platform_exec.h" // for SHELLEXECUTE
#include "rrules.h" // for RECURRENCE_COUNT/DATE
#include "vtimezone.h" // for SETTIMEZONE
#include "mimediritemtype.h" // for AlldayCount/MakeAllday
#ifdef REGEX_SUPPORT
  #include "pcre.h" // for RegEx functions
#endif

#include <stdio.h>
#include <errno.h>

// script debug messages
#ifdef SYDEBUG
  #define SCRIPTDBGMSGX(lvl,x) { if (debugon && fSessionP) { POBJDEBUGPRINTFX(fSessionP,lvl,x); } }
  #define SCRIPTDBGMSG(x) SCRIPTDBGMSGX(DBG_SCRIPTS,x)
  #define SCRIPTDBGSTART(nam) { if (debugon && fSessionP) { if (fSessionP->getDbgMask() & DBG_SCRIPTS) { fSessionP->PDEBUGBLOCKFMTCOLL(("ScriptExecute","Start executing Script","name=%s",nam)); } else { POBJDEBUGPRINTFX(fSessionP,DBG_DATA,("Executing Script '%s'",nam)); } } }
  #define SCRIPTDBGEND() { if (debugon && fSessionP && (fSessionP->getDbgMask() & DBG_SCRIPTS)) { fSessionP->PDEBUGENDBLOCK("ScriptExecute"); } }
  #define SCRIPTDBGTEST (debugon && fSessionP && (fSessionP->getDbgMask() & DBG_SCRIPTS))
  #define EXPRDBGTEST (debugon && fSessionP && ((fSessionP->getDbgMask() & (DBG_SCRIPTS|DBG_SCRIPTEXPR)) == (DBG_SCRIPTS|DBG_SCRIPTEXPR)))
  #define DBGSTRINGDEF(s) string s
  #define DBGVALUESHOW(s,v) dbgValueShow(s,v)
  #if SYDEBUG>1
    #define SHOWVARDEFS(t) showVarDefs(t)
  #else
    #define SHOWVARDEFS(t)
  #endif
#else
  #define SCRIPTDBGMSGX(lvl,x)
  #define SCRIPTDBGMSG(x)
  #define SCRIPTDBGSTART(nam)
  #define SCRIPTDBGEND()
  #define SCRIPTDBGTEST false
  #define EXPRDBGTEST false
  #define DBGSTRINGDEF(s)
  #define DBGVALUESHOW(s,v)
  #define SHOWVARDEFS(t)
#endif


namespace sysync {

#ifdef SYDEBUG

// show value of a field
static void dbgValueShow(string &aString, TItemField *aFieldP)
{
  TItemFieldTypes ty;
  if (aFieldP) {
    // value
    aString = "&html;<span class=\"value\">&html;";
    aFieldP->StringObjFieldAppend(aString,200);
    aString += "&html;</span>&html;";
    // add type info
    ty=aFieldP->getType();
    aString += " (";
    aString += ItemFieldTypeNames[ty];
    aString += ")";
  }
  else {
    aString="<NO FIELD>";
  }
} // dbgValueShow

#endif


TTokenizeException::TTokenizeException(cAppCharP aScriptName, cAppCharP aMsg1,cAppCharP aScript, uInt16 aIndex, uInt16 aLine)
 : TConfigParseException("")
{
  cAppCharP p2=aScript+aIndex;
  cAppCharP p1=p2-(aIndex>5 ? 5 : aIndex);

  StringObjPrintf(fMessage,
    "%s: %s at line %hd: '...%-.5s_%-.10s...'",
    aScriptName ? aScriptName : "<unnamed script>",
    aMsg1,
    aLine,
    p1,
    p2
  );
} // TTokenizeException::TTokenizeException


TScriptErrorException::TScriptErrorException(cAppCharP aMsg1, uInt16 aLine, cAppCharP aIdent)
 : TConfigParseException("")
{
  if (aIdent) {
    StringObjPrintf(fMessage,aMsg1,aIdent);
  }
  else {
    StringObjPrintf(fMessage,"%s",aMsg1);
  }
  StringObjAppendPrintf(
    fMessage,
    " in script at line %hd",
    aLine
  );
} // TScriptErrorException::TScriptErrorException



/*
 * Implementation of TScriptConfig
 */


// config constructor
TScriptConfig::TScriptConfig(TConfigElement *aParentElementP) :
  TConfigElement("scripting",aParentElementP)
{
  clear();
} // TScriptConfig::TScriptConfig


// config destructor
TScriptConfig::~TScriptConfig()
{
  clear();
} // TScriptConfig::~TScriptConfig


// init defaults
void TScriptConfig::clear(void)
{
  // delete options
  fMaxLoopProcessingTime =
    #if SYDEBUG>1
    60; // 1 min for debugging
    #else
    5; // 5 seconds for real execution
    #endif
  // delete functions
  TUserScriptList::iterator pos;
  for (pos=fFunctionScripts.begin();pos!=fFunctionScripts.end();++pos) {
    delete (*pos);
  }
  fFunctionScripts.clear();
  fScriptMacros.clear();
  // clear inherited
  inherited::clear();
} // TScriptConfig::clear


// server config element parsing
bool TScriptConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"looptimeout")==0)
    expectUInt32(fMaxLoopProcessingTime);
  else if (strucmp(aElementName,"function")==0) {
    // create new function definition
    TUserScriptFunction *funcdefP = new TUserScriptFunction;
    fFunctionScripts.push_back(funcdefP);
    expectFunction(*funcdefP,aLine);
  }
  else if (strucmp(aElementName,"macro")==0) {
    const char *macroname = getAttr(aAttributes,"name");
    if (!macroname)
      fail("<macro> must have a 'name' attribute");
    // create new macro
    expectRawString(fScriptMacros[macroname]);
  }
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TScriptConfig::localStartElement


// resolve
void TScriptConfig::localResolve(bool aLastPass)
{
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TScriptConfig::localResolve


// get script text
string *TScriptConfig::getFunctionScript(sInt16 aFuncIndex)
{
  if (aFuncIndex<0 || aFuncIndex>(sInt16)fFunctionScripts.size())
    return NULL;
  return &(fFunctionScripts[aFuncIndex]->fFuncDef);
} // TScriptConfig::getFunctionScript


// get index of specific function
sInt16 TScriptConfig::getFunctionIndex(cAppCharP aName, size_t aLen)
{
  TUserScriptList::iterator pos;
  sInt16 i=0;
  for (pos=fFunctionScripts.begin();pos!=fFunctionScripts.end();++pos) {
    if (strucmp((*pos)->fFuncName.c_str(),aName)==0)
      return i;
    i++;
  }
  // unknown
  return VARIDX_UNDEFINED;
} // TScriptConfig::getFunctionIndex


// Script variable definition

// create new scrip variable definition
TScriptVarDef::TScriptVarDef(cAppCharP aName,uInt16 aIdx, TItemFieldTypes aType, bool aIsArray, bool aIsRef, bool aIsOpt)
{
  fVarName=aName;
  fIdx=aIdx;
  fVarType=aType;
  fIsArray=aIsArray;
  fIsRef=aIsRef;
  fIsOpt=aIsOpt;
} // TScriptVarDef::TScriptVarDef


TScriptVarDef::~TScriptVarDef()
{
} // TScriptVarDef::~TScriptVarDef



/*
 * builtin function definitions
 */


class TBuiltinStdFuncs {
public:

  // timestamp NOW()
  // returns current date/time stamp in UTC with timezone set
  static void func_Now(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aTermP);

    tsP->setTimestampAndContext(
      getSystemNowAs(TCTX_UTC, tsP->getGZones()),
      TCTX_UTC
    );
  }; // func_Now


  // timestamp SYSTEMNOW()
  // returns current date/time stamp as system time with timezone set
  static void func_SystemNow(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aTermP);

    tsP->setTimestampAndContext(
      getSystemNowAs(TCTX_SYSTEM, tsP->getGZones()),
      TCTX_SYSTEM
    );
  }; // func_SystemNow


  // timestamp DBNOW()
  // returns database's idea of "now" in UTC with local timezone set
  static void func_DbNow(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aTermP);

    tsP->setTimestampAndContext(
      aFuncContextP->getSession()->getDatabaseNowAs(TCTX_UTC),
      TCTX_UTC
    );
  }; // func_DbNow


  // integer ZONEOFFSET(timestamp atime)
  // returns zone offset for given timestamp.
  // New in 3.1: Floating timestamps return unassigned
  static void func_ZoneOffset(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));
    sInt16 moffs=0;

    if (tsP->isFloating()) {
      // floating timestamps do not have an offset
      aTermP->unAssign();
    }
    else {
      moffs = tsP->getMinuteOffset();
      aTermP->setAsInteger((sInt32)moffs*SecsPerMin);
    }
  }; // func_ZoneOffset


  // helper to get a time context from integer seconds offset or string zone name
  static timecontext_t contextFromSpec(TItemField *aVariantP, TScriptContext *aFuncContextP)
  {
    timecontext_t tctx = TCTX_UNKNOWN;

    if (aVariantP->isBasedOn(fty_timestamp)) {
      // just use context of another timestamp
      tctx = static_cast<TTimestampField *>(aVariantP)->getTimeContext();
    }
    else if (aVariantP->getCalcType()==fty_integer) {
      // integer specifies a seconds offset
      tctx = TCTX_MINOFFSET(aVariantP->getAsInteger() / SecsPerMin);
    }
    else if (aVariantP->isEmpty()) {
      // empty non-timestamp and non-integer mean unknown/floating timezone
      tctx = TCTX_UNKNOWN;
    }
    else {
      // treat as string specifying time zone by name or vTimezone
      string str;
      aVariantP->getAsString(str);
      if (strucmp(str.c_str(),"USERTIMEZONE")==0) {
        // special case - session's user time zone
        tctx = aFuncContextP->getSession()->fUserTimeContext;
      }
      else if (strucmp(str.c_str(),"SYSTEM")==0) {
        tctx = TCTX_SYSTEM;
      }
      else if (strucmp(str.c_str(),"BEGIN:VTIMEZONE",15)==0) {
        // is a vTimezone, get it
        if (!VTIMEZONEtoInternal(str.c_str(), tctx, aFuncContextP->getSession()->getSessionZones(),aFuncContextP->getSession()->getDbgLogger()))
          tctx = TCTX_UNKNOWN;
      }
      else {
        // search for timezone by name
        if (!TimeZoneNameToContext(str.c_str(), tctx, aFuncContextP->getSession()->getSessionZones())) {
          // last attempt is parsing it as a ISO8601 offset spec
          ISO8601StrToContext(str.c_str(), tctx);
        }
      }
    }
    return tctx;
  } // contextFromSpec


  // helper to represent a time context as string
  static void zoneStrFromContext(timecontext_t aContext, TItemField *aZoneStrFieldP, TScriptContext *aFuncContextP)
  {
    string str;

    if (TCTX_IS_UNKNOWN(aContext)) {
      // no time zone
      aZoneStrFieldP->unAssign();
    }
    else if (TCTX_IS_TZ(aContext)) {
      // symbolic time zone, show name
      TimeZoneContextToName(aContext,str,aFuncContextP->getSession()->getSessionZones());
      aZoneStrFieldP->setAsString(str);
    }
    else {
      // is non-symbolic minute offset, show it in ISO8601 extended form
      str.erase();
      ContextToISO8601StrAppend(str, aContext, true);
    }
  } // zoneStrFromContext


  // string TIMEZONE(timestamp atime)
  // returns time zone name
  static void func_Timezone(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));

    zoneStrFromContext(tsP->getTimeContext(), aTermP, aFuncContextP);
  }; // func_Timezone


  // string VTIMEZONE(timestamp atime)
  // returns time zone in VTIMEZONE format
  static void func_VTimezone(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));

    string z;
    internalToVTIMEZONE(tsP->getTimeContext(),z,aFuncContextP->getSession()->getSessionZones());
    aTermP->setAsString(z);
  }; // func_VTimezone


  // SETTIMEZONE(timestamp &atime,variant zonespec)
  // sets time zone for given timestamp field
  static void func_SetTimezone(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));

    // get context from variant spec
    timecontext_t tctx = contextFromSpec(aFuncContextP->getLocalVar(1), aFuncContextP);
    // set it
    tsP->setTimeContext(tctx);
  }; // func_SetTimezone


  // SETFLOATING(timestamp &atime)
  // sets given timestamp to floating (no timezone)
  // this is an efficient shortform for SETTIMEZONE(atime,"FLOATING") or SETTIMEZONE(atime,"")
  static void func_SetFloating(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));
    // set it
    tsP->setTimeContext(TCTX_UNKNOWN);
  }; // func_SetFloating


  // timestamp CONVERTTOZONE(timestamp atime, variant zonespec [,boolean doUnfloat])
  // returns timestamp converted to specified zone.
  // - If doUnfloat, floating timestamps will be fixed in the new zone w/o conversion of the timestamp itself.
  // - timestamps that already have a zone will be converted
  static void func_ConvertToZone(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));

    // get context from variant spec
    timecontext_t actual,tctx = contextFromSpec(aFuncContextP->getLocalVar(1), aFuncContextP);
    // convert and get actually resulting context back (can also be floating)
    lineartime_t ts = tsP->getTimestampAs(tctx,&actual);
    // unfloat floats if selected
    if (aFuncContextP->getLocalVar(2)->getAsBoolean() && TCTX_IS_UNKNOWN(actual)) actual=tctx; // unfloat
    // assign it to result
    static_cast<TTimestampField *>(aTermP)->setTimestampAndContext(ts,actual);
  }; // func_ConvertToZone


  // timestamp CONVERTTOUSERZONE(timestamp atime [,boolean doUnfloat])
  // returns timestamp converted to user time zone.(or floating timestamp as-is)
  // - this is an efficient shortform for CONVERTTOZONE(atime,"USERTIMEZONE")
  // - If doUnfloat, floating timestamps will be fixed in the new zone w/o conversion of the timestamp itself.
  // - timestamps that already have a zone will be converted
  static void func_ConvertToUserZone(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));

    timecontext_t actual,tctx = aFuncContextP->getSession()->fUserTimeContext;
    // convert and get actually resulting context back (can also be floating)
    lineartime_t ts = tsP->getTimestampAs(tctx,&actual);
    // unfloat floats if selected
    if (aFuncContextP->getLocalVar(1)->getAsBoolean() && TCTX_IS_UNKNOWN(actual)) actual=tctx; // unfloat
    // assign it to result
    static_cast<TTimestampField *>(aTermP)->setTimestampAndContext(ts,actual);
  }; // func_ConvertToUserZone


  // string USERTIMEZONE()
  // returns session user time zone name
  static void func_UserTimezone(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    zoneStrFromContext(aFuncContextP->getSession()->fUserTimeContext,aTermP, aFuncContextP);
  }; // func_UserTimezone


  // SETUSERTIMEZONE(variant zonespec)
  // sets session user time zone
  static void func_SetUserTimezone(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get context from variant spec
    timecontext_t tctx = contextFromSpec(aFuncContextP->getLocalVar(1), aFuncContextP);

    aFuncContextP->getSession()->fUserTimeContext = tctx;
  }; // func_SetUserTimezone


  // integer ISDATEONLY(timestamp atime)
  // returns true if given timestamp is a date-only
  static void func_IsDateOnly(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));

    aTermP->setAsInteger(TCTX_IS_DATEONLY(tsP->getTimeContext()) ? 1 : 0);
  }; // func_IsDateOnly


  // timestamp DATEONLY(timestamp atime)
  // returns a floating(!) date-only of the given timestamp
  static void func_DateOnly(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));

    // get timestamp as dateonly
    timecontext_t tctx;
    lineartime_t ts;
    ts = tsP->getTimestampAs(TCTX_UNKNOWN | TCTX_DATEONLY, &tctx);
    // assign it to result (but do NOT pass in tctx, as it might contain a zone
    // but we need it as floating to make dates comparable
    static_cast<TTimestampField *>(aTermP)->setTimestampAndContext(ts,TCTX_DATEONLY|TCTX_UNKNOWN);
  }; // func_DateOnly


  // timestamp TIMEONLY(timestamp atime)
  // returns a floating(!) time-only of the given timestamp
  static void func_TimeOnly(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));

    // get timestamp as dateonly
    timecontext_t tctx;
    lineartime_t ts;
    ts = tsP->getTimestampAs(TCTX_UNKNOWN | TCTX_TIMEONLY, &tctx);
    // assign it to result (but do NOT pass in tctx, as it might contain a zone
    // but we need it as floating to make dates comparable
    static_cast<TTimestampField *>(aTermP)->setTimestampAndContext(ts,TCTX_TIMEONLY|TCTX_UNKNOWN);
  }; // func_TimeOnly



  // integer ISDURATION(timestamp atime)
  // returns true if given timestamp is a duration value
  static void func_IsDuration(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));
    aTermP->setAsInteger(tsP->isDuration() ? 1 : 0);
  }; // func_IsDuration


  // timestamp DURATION(timestamp atime)
  // returns the timestamp as a duration (floating, duration flag set)
  static void func_Duration(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));

    // get timestamp as-is
    timecontext_t tctx;
    lineartime_t ts;
    ts = tsP->getTimestampAs(TCTX_UNKNOWN, &tctx);
    // result is floating timestamp with duration flag set (and dateonly/timeonly flags retained)
    static_cast<TTimestampField *>(aTermP)->setTimestampAndContext(ts,TCTX_UNKNOWN|(tctx&TCTX_RFLAGMASK)|TCTX_DURATION);
  }; // func_Duration


  // timestamp POINTINTIME(timestamp atime)
  // returns the timestamp as a point in time (i.e. not duration and not dateonly/timeonly)
  static void func_PointInTime(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));

    // get timestamp as-is
    timecontext_t tctx;
    lineartime_t ts;
    ts = tsP->getTimestampAs(TCTX_UNKNOWN, &tctx);
    // assign it to result with duration and dateonly flag cleared
    static_cast<TTimestampField *>(aTermP)->setTimestampAndContext(ts,tctx & (~(TCTX_DURATION+TCTX_DATEONLY+TCTX_TIMEONLY)));
  }; // func_PointInTime



  // integer ISFLOATING(timestamp atime)
  // returns true if given timestamp is floating (i.e. not bound to a time zone)
  static void func_IsFloating(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TTimestampField *tsP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));
    aTermP->setAsInteger(tsP->isFloating() ? 1 : 0);
  }; // func_IsFloating




  // integer ALLDAYCOUNT(timestamp start, timestamp end [, boolean checkinusercontext [, onlyutcinusercontext]])
  // returns number of days for an all-day event
  // Note: Timestamps must be in the context in which they are to be checked for midnight, 23:59:xx etc.
  //       except if checkinusercontext ist set (then non-floating timestamps are moved into user context before
  //       checking. onlyutcinusercontext limits moving to user context to UTC timestamps only.
  static void func_AlldayCount(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    sInt16 c = AlldayCount(
      aFuncContextP->getLocalVar(0),
      aFuncContextP->getLocalVar(1),
      aFuncContextP->getLocalVar(2)->getAsBoolean() ? aFuncContextP->getSession()->fUserTimeContext : TCTX_UNKNOWN,
      aFuncContextP->getLocalVar(3)->getAsBoolean()
    );
    aTermP->setAsInteger(c);
  }; // func_AlldayCount


  // MAKEALLDAY(timestamp &start, timestamp &end [,integer days])
  // adjusts timestamps for allday representation, makes them floating
  // Note: Timestamps must already represent local day times
  static void func_MakeAllday(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    sInt16 days=aFuncContextP->getLocalVar(2)->getAsInteger(); // returns 0 if unassigned
    MakeAllday(
      aFuncContextP->getLocalVar(0),
      aFuncContextP->getLocalVar(1),
      TCTX_UNKNOWN,
      days
    );
  }; // func_MakeAllday


  // integer WEEKDAY(timestamp timestamp)
  // returns weekday (0=sunday, 1=monday ... 6=saturday) from a timestamp
  static void func_Weekday(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    lineartime_t lt = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0))->getTimestampAs(TCTX_UNKNOWN);
    aTermP->setAsInteger(lineartime2weekday(lt));
  }; // func_Weekday


  // integer SECONDS(integer timeunits)
  // returns number of seconds from a time unit spec
  static void func_Seconds(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    fieldinteger_t v=aFuncContextP->getLocalVar(0)->getAsInteger();
    aTermP->setAsInteger(v/secondToLinearTimeFactor);
  }; // func_Seconds


  // integer MILLISECONDS(integer timeunits)
  // returns number of milliseconds from a time unit spec
  static void func_Milliseconds(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    fieldinteger_t v=aFuncContextP->getLocalVar(0)->getAsInteger();
    if (secondToLinearTimeFactor==1) {
      v=v*1000; // internal unit is seconds
    }
    else if (secondToLinearTimeFactor!=1000) {
      v=v/secondToLinearTimeFactor; // seconds
      v=v*1000; // artifical milliseconds
    }
    aTermP->setAsInteger(v);
  }; // func_Milliseconds


  // void SLEEPMS(integer milliseconds)
  // sleeps process (or thread) for specified time
  static void func_SleepMS(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    fieldinteger_t sl=aFuncContextP->getLocalVar(0)->getAsInteger();
    // make nanoseconds, then timeunits
    sl *= 1000000LL;
    sl /= nanosecondsPerLinearTime;
    sleepLineartime(sl);
  }; // func_SleepMS


  // integer TIMEUNITS(integer seconds)
  // returns number of time units from a seconds spec
  static void func_Timeunits(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    fieldinteger_t v=aFuncContextP->getLocalVar(0)->getAsInteger();
    aTermP->setAsInteger(v*secondToLinearTimeFactor);
  }; // func_Timeunits


  // integer DAYUNITS(integer days)
  // returns number of time units from a seconds spec
  static void func_Dayunits(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    fieldinteger_t v=aFuncContextP->getLocalVar(0)->getAsInteger();
    aTermP->setAsInteger(v*linearDateToTimeFactor);
  }; // func_Dayunits


  // integer MONTHDAYS(timestamp date)
  // returns number of days of the month date is in
  static void func_MonthDays(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    lineartime_t ts = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0))->getTimestampAs(TCTX_UNKNOWN);
    aTermP->setAsInteger(getMonthDays(lineartime2dateonly(ts)));
  }; // func_MonthDays



  // DEBUGMESSAGE(string msg)
  // writes debug message to debug log file if debugging is not completely disabled
  static void func_Debugmessage(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    #ifdef SYDEBUG
    if (aFuncContextP->getDbgMask()) {
      // get message
      string s;
      aFuncContextP->getLocalVar(0)->getAsString(s);
      // write it
      if (aFuncContextP->getSession()) {
        POBJDEBUGPRINTFX(aFuncContextP->getSession(),DBG_HOT,("Script DEBUGMESSAGE() at Line %hd: %s",aFuncContextP->getScriptLine(),s.c_str()));
      } else {
        POBJDEBUGPRINTFX(aFuncContextP->getSyncAppBase(),DBG_HOT,("Script DEBUGMESSAGE() at Line %hd: %s",aFuncContextP->getScriptLine(),s.c_str()));
      }
    }
    #endif
  }; // func_Debugmessage


  // DEBUGSHOWVARS()
  // shows values of all local script variables
  static void func_DebugShowVars(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    #ifdef SYDEBUG
    if (aFuncContextP->getDbgMask() && aFuncContextP->fParentContextP) {
      POBJDEBUGPRINTFX(aFuncContextP->getSession(),DBG_HOT,("Script DEBUGSHOWVARS() at Line %hd:",aFuncContextP->getScriptLine()));
      // show all local vars (of PARENT!)
      TScriptContext *showContextP = aFuncContextP->fParentContextP;
      string fshow;
      uInt16 i;
      for (i=0; i<showContextP->getNumLocals(); i++) {
        StringObjAppendPrintf(fshow,"- %-20s : ",showContextP->getVarDef(i)->fVarName.c_str());
        showContextP->getLocalVar(i)->StringObjFieldAppend(fshow,80);
        fshow += '\n';
      }
      // output preformatted
      POBJDEBUGPUTSXX(aFuncContextP->getSession(),DBG_HOT,fshow.c_str(),0,true);
    }
    #endif
  }; // func_DebugShowVars


  // DEBUGSHOWITEM([bool aShowRefItem])
  // shows all fields and values of current item
  static void func_DebugShowItem(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    #ifdef SYDEBUG
    if (aFuncContextP->getDbgMask() && aFuncContextP->fParentContextP) {
      POBJDEBUGPRINTFX(aFuncContextP->getSession(),DBG_HOT,("Script DEBUGSHOWITEM() at Line %hd:",aFuncContextP->getScriptLine()));
      // select item
      TMultiFieldItem *showItemP =
        aFuncContextP->getLocalVar(0)->getAsBoolean() ? // optional param to select ref item instead of target
        aFuncContextP->fParentContextP->fReferenceItemP :
        aFuncContextP->fParentContextP->fTargetItemP;
      if (showItemP) {
        showItemP->debugShowItem(DBG_HOT);
      }
      else {
        POBJDEBUGPRINTFX(aFuncContextP->getSession(),DBG_HOT,("- no item to show"));
      }
    }
    #endif
  }; // func_DebugShowItem




  // SETXMLTRANSLATE(bool yesorno)
  // enables or disables XML translated SyncML message dumping on a per session basis
  static void func_SetXMLTranslate(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    #ifdef SYDEBUG
    if (aFuncContextP->getSession())
      aFuncContextP->getSession()->fXMLtranslate=aFuncContextP->getLocalVar(0)->getAsBoolean();
    #endif
  }; // func_SetXMLTranslate


  // SETMSGDUMP(bool yesorno)
  // enables or disables raw SyncML message dumping on a per session basis
  static void func_SetMsgDump(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    #ifdef SYDEBUG
    if (aFuncContextP->getSession())
      aFuncContextP->getSession()->fMsgDump=aFuncContextP->getLocalVar(0)->getAsBoolean();
    #endif
  }; // func_SetMsgDump


  // SETDEBUGOPTIONS(string optionname, boolean set)
  // sets or clears debug option flags (for the currently running session)
  static void func_SetDebugOptions(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    #ifdef SYDEBUG
    TDebugLogger *loggerP=aFuncContextP->getDbgLogger();
    if (loggerP) {
      // get option string
      string s;
      aFuncContextP->getLocalVar(0)->getAsString(s);
      // convert to bitmask
      sInt16 k;
      if (StrToEnum(debugOptionNames,numDebugOptions,k,s.c_str())) {
        // found mask, modify
        uInt32 currentmask=loggerP->getRealMask();
        if (aFuncContextP->getLocalVar(1)->getAsBoolean())
          currentmask |= debugOptionMasks[k];
        else
          currentmask &= ~debugOptionMasks[k];
        // .. and apply
        loggerP->setMask(currentmask);
      }
    }
    #endif
  }; // func_SetDebugOptions


  // SETDEBUGMASK(integer mask)
  // sets the debug mask
  static void func_SetDebugMask(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    #ifdef SYDEBUG
    TDebugLogger *loggerP=aFuncContextP->getDbgLogger();
    if (loggerP) {
      // get mask value
      loggerP->setMask(aFuncContextP->getLocalVar(0)->getAsInteger());
    }
    #endif
  }; // func_SetDebugMask


  // integer GETDEBUGMASK()
  // gets the current debug mask
  static void func_GetDebugMask(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    uInt32 m=0; // without debug, mask is always 0
    #ifdef SYDEBUG
    TDebugLogger *loggerP=aFuncContextP->getDbgLogger();
    if (loggerP) {
      // get mask value
      loggerP->getRealMask();
    }
    #endif
    aTermP->setAsInteger(m);
  }; // func_GetDebugMask


  // void REQUESTMAXTIME(integer maxtime_seconds)
  static void func_RequestMaxTime(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TSyncSession *sessionP = aFuncContextP->getSession();
    if (sessionP) {
      sessionP->fRequestMaxTime=aFuncContextP->getLocalVar(0)->getAsInteger();
    }
  } // func_RequestMaxTime


  // void REQUESTMINTIME(integer maxtime_seconds)
  // artificial delay for testing
  static void func_RequestMinTime(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TSyncSession *sessionP = aFuncContextP->getSession();
    if (sessionP) {
      sessionP->fRequestMinTime=aFuncContextP->getLocalVar(0)->getAsInteger();
    }
  } // func_RequestMinTime


  // string SYNCMLVERS()
  // gets the SyncML version of the current session as string like "1.2" or "1.0" etc.
  static void func_SyncMLVers(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsString(SyncMLVersionNames[aFuncContextP->getSession()->getSyncMLVersion()]);
  }; // func_SyncMLVers


  // integer SHELLEXECUTE(string command, string arguments [,boolean inbackground])
  // executes the command line in a shell, returns the exit code of the command
  static void func_Shellexecute(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    string cmd,params;
    bool inbackground;
    sInt32 exitcode;
    aFuncContextP->getLocalVar(0)->getAsString(cmd);
    aFuncContextP->getLocalVar(1)->getAsString(params);
    // optional param
    inbackground=aFuncContextP->getLocalVar(2)->getAsBoolean(); // returns false if not assigned -> not in background
    // execute now
    exitcode=shellExecCommand(cmd.c_str(),params.c_str(),inbackground);
    // return result code
    aTermP->setAsInteger(exitcode);
  }; // func_Shellexecute

  // string READ(string file)
  // reads the file and returns its content or UNASSIGNED in case of failure;
  // errors are logged
  static void func_Read(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    string file;
    aFuncContextP->getLocalVar(0)->getAsString(file);

    // execute now
    string content;
    FILE *in;
    in = fopen(file.c_str(), "rb");
    if (in) {
      long size = fseek(in, 0, SEEK_END);
      if (size >= 0) {
        // managed to obtain size, use it to pre-allocate result
        content.reserve(size);
        fseek(in, 0, SEEK_SET);
      } else {
        // ignore seek error, might not be a plain file
        clearerr(in);
      }

      if (!ferror(in)) {
        char buf[8192];
        size_t read;
        while ((read = fread(buf, 1, sizeof(buf), in)) > 0) {
          content.append(buf, read);
        }
      }
    }

    if (in && !ferror(in)) {
      // return content as string
      aTermP->setAsString(content);
    } else {
        PLOGDEBUGPRINTFX(aFuncContextP->getDbgLogger(),DBG_ERROR,(
          "IO error in READ(\"%s\"): %s ",
          file.c_str(),
          strerror(errno)
        ));
    }

    if (in) {
      fclose(in);
    }
  } // func_Read

  // string URIToPath(string uri)
  // extracts the file path in a file:// uri; handles uri decoding
  // Returns UNASSIGNED if not a file:// uri
  static void func_URIToPath(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    string uri;
    aFuncContextP->getLocalVar(0)->getAsString(uri);

    string protocol, doc;
    splitURL(uri.c_str(), &protocol, NULL, &doc, NULL, NULL, NULL, NULL);
    if (protocol == "file") {
      string path;
      path.reserve(doc.size() + 1);
      path += "/"; // leading slash is never included by splitURL()
      path += doc;
      urlDecode(&path);
      aTermP->setAsString(path);
    }
  } // func_URIToPath

  // string REMOTERULENAME()
  // returns name of the LAST matched remote rule (or subrule), empty if none
  // Note: this is legacy from 3.4.0.4 onwards, as we now have a list of multiple active rules
  static void func_Remoterulename(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    #ifndef NO_REMOTE_RULES
    if (aFuncContextP->getSession()) {
      // there is a session
      if (!aFuncContextP->getSession()->fActiveRemoteRules.empty()) {
        // there is at least one rule applied, get the last one in the list
        aTermP->setAsString(aFuncContextP->getSession()->fActiveRemoteRules.back()->getName());
        return;
      }
    }
    #endif
    // no remote rule applied
    aTermP->assignEmpty();
  }; // func_Remoterulename


  // boolean ISACTIVERULE(string rulename)
  // checks if given rule is currently activated
  static void func_isActiveRule(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    #ifndef NO_REMOTE_RULES
    string r;
    aFuncContextP->getLocalVar(0)->getAsString(r);
    if (aFuncContextP->getSession()) {
      // there is a session, return status
      aTermP->setAsBoolean(aFuncContextP->getSession()->isActiveRule(r.c_str()));
      return;
    }
    #endif
    // remote rules not supported or no session active
    aTermP->assignEmpty();
  }; // func_Remoterulename




  // TREATASLOCALTIME(integer flag)
  static void func_SetTreatAsLocaltime(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TSyncSession *sessionP = aFuncContextP->getSession();
    if (sessionP) sessionP->fTreatRemoteTimeAsLocal = aFuncContextP->getLocalVar(0)->getAsBoolean();
  }; // func_SetTreatAsLocaltime


  // TREATASUTC(integer flag)
  static void func_SetTreatAsUTC(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TSyncSession *sessionP = aFuncContextP->getSession();
    if (sessionP) sessionP->fTreatRemoteTimeAsUTC = aFuncContextP->getLocalVar(0)->getAsBoolean();
  }; // func_SetTreatAsUTC


  // UPDATECLIENTINSLOWSYNC(integer flag)
  static void func_SetUpdateClientInSlowSync(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TSyncSession *sessionP = aFuncContextP->getSession();
    if (sessionP) sessionP->fUpdateClientDuringSlowsync = aFuncContextP->getLocalVar(0)->getAsBoolean();
  }; // func_SetUpdateClientInSlowSync


  // UPDATESERVERINSLOWSYNC(integer flag)
  static void func_SetUpdateServerInSlowSync(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TSyncSession *sessionP = aFuncContextP->getSession();
    if (sessionP) sessionP->fUpdateServerDuringSlowsync = aFuncContextP->getLocalVar(0)->getAsBoolean();
  }; // func_SetUpdateServerInSlowSync


  // SHOWCTCAPPROPERTIES(bool yesorno)
  static void func_ShowCTCapProps(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TSyncSession *sessionP = aFuncContextP->getSession();
    if (sessionP) sessionP->fShowCTCapProps = aFuncContextP->getLocalVar(0)->getAsBoolean();
  } // func_ShowCTCapProps


  // SHOWTYPESIZEINCTCAP10(bool yesorno)
  static void func_ShowTypeSizeInCTCap10(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TSyncSession *sessionP = aFuncContextP->getSession();
    if (sessionP) sessionP->fShowTypeSzInCTCap10 = aFuncContextP->getLocalVar(0)->getAsBoolean();
  } // func_ShowTypeSizeInCTCap10


  // ENUMDEFAULTPROPPARAMS(bool yesorno)
  static void func_EnumDefaultPropParams(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TSyncSession *sessionP = aFuncContextP->getSession();
    // Note that this is a tristate!
    if (sessionP) sessionP->fEnumDefaultPropParams =
      aFuncContextP->getLocalVar(0)->getAsBoolean() ? 1 : 0;
  } // func_EnumDefaultPropParams


  // string LOCALURI()
  // returns local URI as used by client for starting session
  static void func_LocalURI(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string r;
    if (aFuncContextP->getSession()) {
      // there is a session
      aTermP->setAsString(aFuncContextP->getSession()->getInitialLocalURI());
      return;
    }
    // no session??
    aTermP->assignEmpty();
  }; // func_LocalURI


  // string SUBSTR(string, from [, count])
  static void func_Substr(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    string s;
    aFuncContextP->getLocalVar(0)->getAsString(s);
    string::size_type i=aFuncContextP->getLocalVar(1)->getAsInteger();
    // optional param
    string::size_type l=s.size(); // default to entire string
    if (aFuncContextP->getLocalVar(2)->isAssigned())
      l=aFuncContextP->getLocalVar(2)->getAsInteger(); // use specified count
    string r;
    // adjust params
    if (i>=s.size()) l=0;
    else if (i+l>s.size()) l=s.size()-i;
    // evaluate
    if (l>0) r.assign(s,i,l);
    // save result
    aTermP->setAsString(r);
  }; // func_Substr


  // string EXPLODE(string glue, variant &parts[])
  static void func_Explode(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    string glue,s;
    aFuncContextP->getLocalVar(0)->getAsString(glue);
    TItemField *fldP = aFuncContextP->getLocalVar(1);
    // concatenate array elements with glue
    aTermP->assignEmpty();
    for (uInt16 i=0; i<fldP->arraySize(); i++) {
      // get array element as string
      fldP->getArrayField(i)->getAsString(s);
      // add to output
      aTermP->appendString(s);
      if (i+1<fldP->arraySize()) {
        // we have more elements, add glue
        aTermP->appendString(glue);
      }
    }
  }; // func_Explode


  #ifdef _MSC_VER
    static long long V_llabs( long long j )
    {
    return (j < 0 ? -j : j);
    }
  #endif

  // integer ABS(integer val)
  static void func_Abs(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    #ifdef _MSC_VER
    aTermP->setAsInteger(V_llabs(aFuncContextP->getLocalVar(0)->getAsInteger()));
    #else
    aTermP->setAsInteger(::llabs(aFuncContextP->getLocalVar(0)->getAsInteger()));
    #endif
  }; // func_Abs


  // integer SIGN(integer val)
  //  i == 0 :  0
  //  i >  0 :  1
  //  i <  0 : -1
  static void func_Sign(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    fieldinteger_t i = aFuncContextP->getLocalVar(0)->getAsInteger();

    aTermP->setAsInteger(i==0 ? 0 : (i>0 ? 1 : -1));
  }; // func_Sign


  // integer RANDOM(integer range [, integer seed])
  // generates random number between 0 and range-1. Seed is optional to init random generator
  static void func_Random(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // seed if second param there
    if (aFuncContextP->getLocalVar(1)->isAssigned()) {
      // seed random gen first
      srand((unsigned int)aFuncContextP->getLocalVar(1)->getAsInteger());
    }
    // now get random value
    fieldinteger_t r = rand();
    fieldinteger_t max = RAND_MAX;
    // scale to specified range
    aTermP->setAsInteger(r * aFuncContextP->getLocalVar(0)->getAsInteger() / (max+1));
  }; // func_Random



  // string NUMFORMAT(integer num, integer digits [,string filler=" " [,boolean opts=""]])
  static void func_NumFormat(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // mandatory params
    fieldinteger_t i = aFuncContextP->getLocalVar(0)->getAsInteger();
    sInt16 numdigits = aFuncContextP->getLocalVar(1)->getAsInteger(); // negative = left justified
    // optional params
    string filler = " "; // default to filling with spaces
    if (aFuncContextP->getLocalVar(2)->isAssigned())
      aFuncContextP->getLocalVar(2)->getAsString(filler); // empty: no filling, only truncation
    string opts;
    aFuncContextP->getLocalVar(3)->getAsString(opts); // +: show plus sign. space: show space as plus sign

    // generate raw string
    string s;
    // - determine hex mode
    bool hex = opts.find("x")!=string::npos;
    // - create sign
    char sign = 0;
    if (!hex) {
      if (i<0)
        sign = '-';
      else {
        if (opts.find("+")!=string::npos) sign='+';
        else if (opts.find(" ")!=string::npos) sign=' ';
      }
    }
    // create raw numeric string
    if (hex) {
      StringObjPrintf(s,"%llX",(long long)i);
    }
    else {
      #ifdef _MSC_VER
        StringObjPrintf(s,"%lld",V_llabs(i));
      #else
        StringObjPrintf(s,"%lld",::llabs(i));
      #endif
    }
    // adjust
    char c = *(filler.c_str()); // NUL or filler char
    if (c!='0' && sign) {
      s.insert((size_t)0,(size_t)1,sign); // no zero-padding: insert sign before padding
      sign=0; // done now
    }
    sInt32 n,sz = s.size() + (sign ? 1 : 0); // leave room for sign after zero padding
    if (numdigits>0) {
      // right aligned field
      n = numdigits-sz; // empty space in field
      if (n<0)
        s.erase(0,-n); // delete at beginning
      else if (n>0 && c)
        s.insert((size_t)0,(size_t)n,c); // insert at beginning
    }
    else {
      // left aligned field
      n = -numdigits-sz; // empty space in field
      if (n<0)
        s.erase(sz-n,-n); // delete at end
      else if (n>0 && c)
        s.insert((size_t)sz,(size_t)n,c); // insert at end
    }
    // insert plus now if filled with zeroes
    if (sign)
      s.insert((size_t)0,(size_t)1,sign); // insert sign after zero padding
    // return string
    aTermP->setAsString(s);
  } // func_NumFormat


  // integer LENGTH(string)
  static void func_Length(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TItemField *fldP = aFuncContextP->getLocalVar(0);
    fieldinteger_t siz;
    if (fldP->isBasedOn(fty_string)) {
      // don't get value to avoid pulling large strings just for size
      siz=static_cast<TStringField *>(fldP)->getStringSize();
    }
    else {
      // brute force
      string s;
      fldP->getAsString(s);
      siz=s.size();
    }
    // save result
    aTermP->setAsInteger(siz);
  }; // func_Length


  // integer SIZE(&var)
  static void func_Size(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TItemField *fldP = aFuncContextP->getLocalVar(0);
    fieldinteger_t siz;
    if (fldP->isArray()) {
      siz=fldP->arraySize();
    }
    else if (fldP->isBasedOn(fty_string)) {
      // don't get value to avoid pulling large strings just for size
      siz=static_cast<TStringField *>(fldP)->getStringSize();
    }
    else {
      // brute force
      string s;
      fldP->getAsString(s);
      siz=s.size();
    }
    // save result
    aTermP->setAsInteger(siz);
  }; // func_Size



  // integer FIND(string, pattern [, startat])
  static void func_Find(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    string s,pat;
    string::size_type p;
    aFuncContextP->getLocalVar(0)->getAsString(s);
    aFuncContextP->getLocalVar(1)->getAsString(pat);
    // optional param
    uInt32 i=aFuncContextP->getLocalVar(2)->getAsInteger(); // returns 0 if unassigned
    // find in string
    if (i<s.size())
      p=s.find(pat,i);
    else
      p=string::npos;
    // return UNASSIGNED for "not found" and position otherwise
    if (p==string::npos) aTermP->unAssign();
    else aTermP->setAsInteger(p);
  }; // func_Find


  // integer RFIND(string, pattern [, startat])
  static void func_RFind(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    string s,pat;
    string::size_type p;
    aFuncContextP->getLocalVar(0)->getAsString(s);
    aFuncContextP->getLocalVar(1)->getAsString(pat);
    // optional param
    uInt32 i=aFuncContextP->getLocalVar(2)->getAsInteger(); // returns 0 if unassigned
    if (i>s.size()) i=s.size();
    // find in string
    p=s.rfind(pat,i);
    // return UNASSIGNED for "not found" and position otherwise
    if (p==string::npos) aTermP->unAssign();
    else aTermP->setAsInteger(p);
  }; // func_RFind


  #ifdef REGEX_SUPPORT

  // run PCRE regexp
  // Returns:          > 0 => success; value is the number of elements filled in
  //                   = 0 => success, but offsets is not big enough
  //                    -1 => failed to match
  //                    -2 => PCRE_ERROR_NULL => did not compile, error reported to aDbgLogger
  //                  < -2 => some kind of unexpected problem
  static int run_pcre(cAppCharP aRegEx, cAppCharP aSubject, stringSize aSubjLen, stringSize aSubjStart, int *aOutVec, int aOVSize, TDebugLogger *aDbgLogger)
  {
    string regexpat;
    // set default options
    int options=0;
    // scan input pattern. If it starts with /, we assume /xxx/opt form
    cAppCharP p = aRegEx;
    char c=*p;
    if (c=='/') {
      // delimiter found
      p++;
      // - now search end
      while (*p) {
        if (*p=='\\') {
          // escaped char
          p++;
          if (*p) p++;
        }
        else {
          if (*p==c) {
            // found end of regex
            size_t n=p-aRegEx-1; // size of plain regExp
            // - scan options
            cAppCharP o = p++;
            while (*o) {
              switch (*o) {
                case 'i' : options |= PCRE_CASELESS; break;
                case 'm' : options |= PCRE_MULTILINE; break;
                case 's' : options |= PCRE_DOTALL; break;
                case 'x' : options |= PCRE_EXTENDED; break;
                case 'U' : options |= PCRE_UNGREEDY; break;
              }
              o++;
            }
            // - extract regex itself
            regexpat.assign(aRegEx+1,n);
            aRegEx = regexpat.c_str();
            break; // done
          }
          p++;
        }
      } // while chars in regex
    } // if regex with delimiter
    // - compile regex
    pcre *regex;
    cAppCharP errMsg=NULL;
    int errOffs=0;
    regex = pcre_compile(aRegEx, options | PCRE_UTF8, &errMsg, &errOffs, NULL);
    if (regex==NULL) {
      // error, display it in log if script logging is on
      PLOGDEBUGPRINTFX(aDbgLogger,DBG_SCRIPTS+DBG_ERROR,(
        "RegEx error at pattern pos %d: %s ",
        errOffs,
        errMsg ? errMsg : "<unknown>"
      ));
      return PCRE_ERROR_NULL; // -2, regexp did not compile
    }
    else {
      // regExp is ok and can be executed against subject
      int r = pcre_exec(regex, NULL, aSubject, aSubjLen, aSubjStart, 0, aOutVec, aOVSize);
      pcre_free(regex);
      return r;
    }
  } // run_pcre


  // integer REGEX_FIND(string subject, string pattern [, integer startat])
  static void func_Regex_Find(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    string s,pat;
    aFuncContextP->getLocalVar(0)->getAsString(s);
    aFuncContextP->getLocalVar(1)->getAsString(pat);
    // optional param
    sInt16 i=aFuncContextP->getLocalVar(2)->getAsInteger(); // returns 0 if unassigned
    // use PCRE to find
    const int ovsize=3; // we need no matches
    int ov[ovsize];
    int rc = run_pcre(pat.c_str(),s.c_str(),s.size(),i,ov,ovsize,aFuncContextP->getDbgLogger());
    if (rc>=0) {
      // return start position
      aTermP->setAsInteger(ov[0]);
    }
    else {
      // return UNASSIGNED for "not found" and error
      aTermP->unAssign();
    }
  }; // func_Regex_Find


  // integer REGEX_MATCH(string subject, string regexp, integer startat, array &matches)
  static void func_Regex_Match(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    string s,pat;
    aFuncContextP->getLocalVar(0)->getAsString(s);
    aFuncContextP->getLocalVar(1)->getAsString(pat);
    sInt32 i=aFuncContextP->getLocalVar(2)->getAsInteger();
    TItemField *matchesP = aFuncContextP->getLocalVar(3);
    string m;
    // use PCRE to find
    const int ovsize=54; // max matches (they say this must be a multiple of 3, no idea why; I'd say 2...)
    int ov[ovsize];
    int rc = run_pcre(pat.c_str(),s.c_str(),s.size(),i,ov,ovsize,aFuncContextP->getDbgLogger());
    if (rc>0) {
      // return start position
      aTermP->setAsInteger(ov[0]);
      // return matches
      int mIdx;
      TItemField *fldP;
      for (mIdx=0; mIdx<rc; mIdx++) {
        // get field to assign match to
        if (matchesP->isArray())
          fldP = matchesP->getArrayField(mIdx);
        else {
          // non-array specified
          fldP = matchesP;
          // - if there are no subpatterns, assign the first match (entire pattern)
          // - if there are subpatterns, assign the first subpattern match
          if (rc>1) {
            // there is at least one subpattern
            mIdx++; // skip the entire pattern match such that 1st subpattern gets assigned
          }
        }
        // assign match (first is entire pattern)
        fldP->setAsString(s.c_str()+ov[mIdx*2],ov[mIdx*2+1]-ov[mIdx*2]); // assign substring
        // end if matches is not an array
        if (!matchesP->isArray())
          break;
      }
    }
    else {
      // return UNASSIGNED for "not found" and all errors
      aTermP->unAssign();
    }
  }; // func_Regex_Match


  // integer REGEX_SPLIT(string subject, string separatorregexp, array elements [, boolean emptyElements])
  // returns number of elements created in elements array
  static void func_Regex_Split(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    string s,pat;
    aFuncContextP->getLocalVar(0)->getAsString(s);
    aFuncContextP->getLocalVar(1)->getAsString(pat);
    TItemField *elementsP = aFuncContextP->getLocalVar(2);
    // optional params
    bool emptyElements = aFuncContextP->getLocalVar(3)->getAsBoolean(); // skip empty elements by default
    // find all recurrences of separator
    uInt32 i = 0; // start at beginning
    sInt32 rIdx = 0; // no results so far
    while (i<s.size()) {
      // use PCRE to find
      const int ovsize=3; // we need no matches
      int ov[ovsize];
      int rc = run_pcre(pat.c_str(),s.c_str(),s.size(),i,ov,ovsize,aFuncContextP->getDbgLogger());
      if (rc<=0) {
        // no further match found
        // - simulate match at end of string
        ov[0]=s.size();
        ov[1]=ov[0];
      }
      // copy element
      if (uInt32(ov[0])>i || emptyElements) {
        TItemField *fldP = elementsP->getArrayField(rIdx);
        if (ov[0]-i>0)
          fldP->setAsString(s.c_str()+i,ov[0]-i);
        else
          fldP->assignEmpty(); // empty element
        // next element
        rIdx++;
      }
      // skip separator
      i = ov[1];
    } // while not at end of string
    // return number of elements found
    aTermP->setAsInteger(rIdx);
  }; // func_Regex_Split


  // string REGEX_REPLACE(string,regexp,replacement [,startat [,repeat]])
  static void func_Regex_Replace(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    string s,pat,reppat,res;
    aFuncContextP->getLocalVar(0)->getAsString(s);
    aFuncContextP->getLocalVar(1)->getAsString(pat);
    aFuncContextP->getLocalVar(2)->getAsString(reppat);
    // optional params
    sInt16 i=aFuncContextP->getLocalVar(3)->getAsInteger(); // returns 0 if unassigned -> start at beginning
    sInt32 c=aFuncContextP->getLocalVar(4)->getAsInteger(); // returns 0 if unassigned -> replace all
    // use PCRE to find
    const int ovsize=54; // max matches (they say this must be a multiple of 3, no idea why; I'd say 2...)
    int ov[ovsize];
    res.assign(s.c_str(),i); // part of string not searched at all
    do {
      int rc = run_pcre(pat.c_str(),s.c_str(),s.size(),i,ov,ovsize,aFuncContextP->getDbgLogger());
      if (rc<0)
        break; // error or no more matches found
      // found an occurrence
      // - subsititute matches in replacement string
      cAppCharP p=reppat.c_str();
      cAppCharP q=p;
      string rep;
      rep.erase();
      while (*p) {
        if (*p=='\\') {
          p++;
          if (*p==0) break;
          // check for escaped backslash
          if (*p=='\\') { p++; continue; }
          // get replacement number
          if (isdigit(*p)) {
            // is replacement escape sequence, get index
            uInt16 mIdx = *(p++)-'0';
            // append chars before \x
            rep.append(q,p-q-2);
            // append match (if there is one)
            if (mIdx<rc)
              rep.append(s.c_str(),ov[mIdx*2],ov[mIdx*2+1]-ov[mIdx*2]);
            // update rest pointer
            q=p;
            continue;
          }
        }
        p++; // next
      }
      rep.append(q); // copy rest of pattern
      // - insert replacement string into result
      res.append(s.c_str(),i,ov[0]-i); // from previous match or beginning of string to current match
      res.append(rep); // replacement
      i=ov[1]; // search continues here
    } while (c<=0 || (--c)>0); // if c==0, replace all, otherwise as many times as c says
    res.append(s.c_str()+i); // rest
    // return result
    aTermP->setAsString(res);
  }; // func_Regex_Replace

  #endif // REGEX_SUPPORT


  // integer COMPARE(value, value)
  // - returns 0 if equal, 1 if first > second, -1 if first < second,
  //   SYSYNC_NOT_COMPARABLE if not equal and no ordering known or if field
  //   types do not match.
  static void func_Compare(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // compare first field with second one
    aTermP->setAsInteger(
      aFuncContextP->getLocalVar(0)->compareWith(*(aFuncContextP->getLocalVar(1)))
    );
  }; // func_Compare


  // integer CONTAINS(&ref, value [,bool caseinsensitive])
  // - returns 1 if value contained in ref, 0 if not
  static void func_Contains(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // check if second is contained in first
    bool caseinsensitive = aFuncContextP->getLocalVar(2)->getAsBoolean(); // returns false if not specified
    aTermP->setAsBoolean(
      aFuncContextP->getLocalVar(0)->contains(*(aFuncContextP->getLocalVar(1)),caseinsensitive)
    );
  }; // func_Contains


  // APPEND(&ref, value)
  // - appends value to ref (ref can be array)
  static void func_Append(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // append second to first
    aFuncContextP->getLocalVar(0)->append(*(aFuncContextP->getLocalVar(1)));
  }; // func_Append



  // string UPPERCASE(string)
  static void func_UpperCase(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string s;
    TItemField *fldP = aFuncContextP->getLocalVar(0);
    if (fldP->isAssigned()) {
      fldP->getAsString(s);
      StringUpper(s);
      // save result
      aTermP->setAsString(s);
    }
    else {
      aTermP->unAssign();
    }
  }; // func_UpperCase


  // string LOWERCASE(string)
  static void func_LowerCase(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string s;
    TItemField *fldP = aFuncContextP->getLocalVar(0);
    if (fldP->isAssigned()) {
      fldP->getAsString(s);
      StringLower(s);
      // save result
      aTermP->setAsString(s);
    }
    else {
      aTermP->unAssign();
    }
  }; // func_LowerCase


  // string NORMALIZED(variant value)
  // get as normalized string (trimmed CR/LF/space at both ends, no special chars for telephone numbers, http:// added for URLs w/o protocol spec)
  static void func_Normalized(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get field reference
    TItemField *fldP = aFuncContextP->getLocalVar(0);
    if (fldP->isAssigned()) {
      // get normalized version
      string s;
      fldP->getAsNormalizedString(s);
      // save it
      aTermP->setAsString(s);
    }
    else {
      aTermP->unAssign();
    }
  }; // func_Normalized


  // bool ISAVAILABLE(variant &fieldvar)
  // check if field is available (supported by both ends)
  // - returns EMPTY if availability is not known
  // - fieldvar must be a field contained in the primary item of the caller, else function returns UNASSIGNED
  static void func_IsAvailable(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    if (aFuncContextP->fParentContextP) {
      // get item to find field in
      TMultiFieldItem *checkItemP = aFuncContextP->fParentContextP->fTargetItemP;
      // check if this item's type has actually received availability info
      if (!checkItemP->knowsRemoteFieldOptions()) {
        aTermP->assignEmpty(); // nothing known about field availability
        return;
      }
      else {
        // we have availability info
        // - get index of field by field pointer (passed by reference)
        sInt16 fid = checkItemP->getIndexOfField(aFuncContextP->getLocalVar(0));
        if (fid!=FID_NOT_SUPPORTED) {
          // field exists, return availability
          aTermP->setAsBoolean(checkItemP->isAvailable(fid));
          return;
        }
      }
    }
    // no parent context or field not found
    aTermP->unAssign();
  }; // func_IsAvailable


  // SETFIELDOPTIONS(variant &fieldvar, bool available [[[, int maxsize=0 ], int maxoccur=0 ], int notruncate=FALSE])
  // set field options (override what might have been set by reading devInf)
  // - fieldvar must be a field contained in the primary item of the caller, else function is NOP
  static void func_SetFieldOptions(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    if (aFuncContextP->fParentContextP) {
      // get item to find field in
      TMultiFieldItem *checkItemP = aFuncContextP->fParentContextP->fTargetItemP;
      // modify options on the "remote" type
      TMultiFieldItemType *mfitP = checkItemP->getRemoteItemType();
      // - get index of field by field pointer (passed by reference)
      sInt16 fid = checkItemP->getIndexOfField(aFuncContextP->getLocalVar(0));
      if (mfitP && fid!=FID_NOT_SUPPORTED) {
        // field exists, we can set availability
        // - get params
        bool available = aFuncContextP->getLocalVar(1)->getAsBoolean();
        sInt32 maxsize = FIELD_OPT_MAXSIZE_NONE;
        if (aFuncContextP->getLocalVar(2)->isAssigned())
          maxsize = aFuncContextP->getLocalVar(2)->getAsInteger();
        sInt32 maxoccur = aFuncContextP->getLocalVar(3)->getAsInteger(); // returns 0 if not specified
        bool notruncate = aFuncContextP->getLocalVar(4)->getAsBoolean();
        // - now set options
        TFieldOptions *fo = mfitP->getFieldOptions(fid);
        if (fo) {
          fo->available=available;
          fo->maxsize=maxsize;
          fo->maxoccur=maxoccur;
          fo->notruncate=notruncate;
        }
      }
    }
  }; // func_SetFieldOptions


  // string ITEMDATATYPE()
  // returns the type's internal name (like "vcard21")
  static void func_ItemDataType(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    if (aFuncContextP->fParentContextP) {
      // get item of which we want to know the type
      TMultiFieldItem *checkItemP = aFuncContextP->fParentContextP->fTargetItemP;
      if (checkItemP) {
        TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(checkItemP->getItemType());
        if (mfitP) {
          aTermP->setAsString(mfitP->getTypeConfig()->getName());
          return;
        }
      }
    }
    // no type associated or no item in current context
    aTermP->unAssign();
  }; // func_ItemDataType


  // string ITEMTYPENAME()
  // returns the type's name (like "text/x-vcard")
  static void func_ItemTypeName(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    if (aFuncContextP->fParentContextP) {
      // get item of which we want to know the type
      TMultiFieldItem *checkItemP = aFuncContextP->fParentContextP->fTargetItemP;
      if (checkItemP) {
        TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(checkItemP->getItemType());
        if (mfitP) {
          aTermP->setAsString(mfitP->getTypeName());
          return;
        }
      }
    }
    // no type associated or no item in current context
    aTermP->unAssign();
  }; // func_ItemTypeName


  // string ITEMTYPEVERS()
  // returns the type's version string (like "2.1")
  static void func_ItemTypeVers(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    if (aFuncContextP->fParentContextP) {
      // get item of which we want to know the type
      TMultiFieldItem *checkItemP = aFuncContextP->fParentContextP->fTargetItemP;
      if (checkItemP) {
        TMultiFieldItemType *mfitP = static_cast<TMultiFieldItemType *>(checkItemP->getItemType());
        if (mfitP) {
          aTermP->setAsString(mfitP->getTypeVers());
          return;
        }
      }
    }
    // no type associated or no item in current context
    aTermP->unAssign();
  }; // func_ItemTypeVers



  // void SWAP(untyped1,untyped2)
  static void func_Swap(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    TItemField *p1 = aFuncContextP->getLocalVar(0);
    TItemField *p2 = aFuncContextP->getLocalVar(1);
    TItemField *tempP = newItemField(p1->getType(), aFuncContextP->getSessionZones());

    // swap
    (*tempP)=(*p1);
    (*p1)=(*p2);
    (*p2)=(*tempP);
  }; // func_Swap


  // string TYPENAME(untyped1)
  static void func_TypeName(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    TItemFieldTypes ty = aFuncContextP->getLocalVar(0)->getType();
    // return type name
    aTermP->setAsString(ItemFieldTypeNames[ty]);
  }; // func_TypeName


  // variant SESSIONVAR(string varname)
  static void func_SessionVar(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TItemField *sessionVarP;
    TScriptVarDef *sessionVarDefP;
    string varname;
    TScriptContext *sessionContextP=NULL;

    // get name
    aFuncContextP->getLocalVar(0)->getAsString(varname);
    // get variable from session
    if (aFuncContextP->getSession())
      sessionContextP=aFuncContextP->getSession()->getSessionScriptContext();
    if (sessionContextP) {
      // get definition
      sessionVarDefP = sessionContextP->getVarDef(
        varname.c_str(),varname.size()
      );
      if (sessionVarDefP) {
        // get variable
        sessionVarP = sessionContextP->getLocalVar(sessionVarDefP->fIdx);
        if (sessionVarP) {
          // create result field of appropriate type
          aTermP = newItemField(sessionVarP->getType(), aFuncContextP->getSessionZones());
          // copy value
          (*aTermP) = (*sessionVarP);
        }
      }
    }
    if (!aTermP) {
      // if no such variable found, return unassigned (but not no-value, which would abort script)
      aTermP=newItemField(fty_none, aFuncContextP->getSessionZones());
      aTermP->unAssign(); // make it (already is...) unassigned
    }
  }; // func_SessionVar


  // SETSESSIONVAR(string varname, value)
  static void func_SetSessionVar(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TItemField *sessionVarP;
    TScriptVarDef *sessionVarDefP;
    string varname;
    TScriptContext *sessionContextP=NULL;

    // get name
    aFuncContextP->getLocalVar(0)->getAsString(varname);
    // get variable from session
    if (aFuncContextP->getSession()) sessionContextP=aFuncContextP->getSession()->getSessionScriptContext();
    if (sessionContextP) {
      // get definition
      sessionVarDefP = sessionContextP->getVarDef(
        varname.c_str(),varname.size()
      );
      if (sessionVarDefP) {
        // get variable
        sessionVarP = sessionContextP->getLocalVar(sessionVarDefP->fIdx);
        if (sessionVarP) {
          // store new value
          (*sessionVarP) = (*(aFuncContextP->getLocalVar(1)));
        }
      }
    }
  }; // func_SetSessionVar


  // void ABORTSESSION(integer statuscode)
  static void func_AbortSession(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TSyncSession *sessionP = aFuncContextP->getSession();
    if (sessionP) {
      sessionP->AbortSession(aFuncContextP->getLocalVar(0)->getAsInteger(),true); // locally caused
    }
  }; // func_AbortSession


  // SETDEBUGLOG(integer enabled)
  // set debug log output for this sync session
  static void func_SetDebugLog(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    #ifdef SYDEBUG
    TSyncSession *sessionP = aFuncContextP->getSession();
    if (sessionP) {
      sessionP->getDbgLogger()->setEnabled(
        aFuncContextP->getLocalVar(0)->getAsBoolean()
      );
      /// @todo: remove this
      // %%% for now, we also need to set this separate flag
      sessionP->fSessionDebugLogs=
        aFuncContextP->getLocalVar(0)->getAsBoolean();
    }
    #endif
  }; // func_SetDebugLog


  // SETLOG(integer enabled)
  // set debug log output for this sync session
  static void func_SetLog(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    #ifndef MINIMAL_CODE
    TSyncSession *sessionP = aFuncContextP->getSession();
    if (sessionP) {
      sessionP->fLogEnabled=
        aFuncContextP->getLocalVar(0)->getAsBoolean();
    }
    #endif
  }; // func_SetLog


  // SETREADONLY(integer readonly)
  // set readonly option of this sync session
  static void func_SetReadOnly(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TSyncSession *sessionP = aFuncContextP->getSession();
    if (sessionP) {
      sessionP->setReadOnly(aFuncContextP->getLocalVar(0)->getAsBoolean());
    }
  }; // func_SetReadOnly


  // string CONFIGVAR(string varname)
  static void func_ConfigVar(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string varname,value;

    // get name
    aFuncContextP->getLocalVar(0)->getAsString(varname);
    // get value from syncappbase
    if (!aFuncContextP->getSyncAppBase()->getConfigVar(varname.c_str(),value))
      aTermP->setAsString(value);
    else
      aTermP->unAssign(); // not found
  }; // func_ConfigVar


  // timestamp RECURRENCE_DATE(
  //             timestamp start,
  //             string rr_freq, integer interval,
  //             integer fmask, integer lmask,
  //             boolean occurrencecount,
  //             integer count
  //           )
  static void func_Recurrence_Date(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    string rr_freq;
    TTimestampField *startFldP = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0));
    timecontext_t tctx;
    // - get start timestamp as is along with current context
    lineartime_t start = startFldP->getTimestampAs(TCTX_UNKNOWN,&tctx);
    aFuncContextP->getLocalVar(1)->getAsString(rr_freq);
    char freq = rr_freq.size()>0 ? rr_freq[0] : ' ';
    char freqmod = rr_freq.size()>1 ? rr_freq[1] : ' ';
    sInt16 interval = aFuncContextP->getLocalVar(2)->getAsInteger();
    fieldinteger_t fmask = aFuncContextP->getLocalVar(3)->getAsInteger();
    fieldinteger_t lmask = aFuncContextP->getLocalVar(4)->getAsInteger();
    bool occurrencecount = aFuncContextP->getLocalVar(5)->getAsBoolean();
    uInt16 count = aFuncContextP->getLocalVar(6)->getAsInteger();
    // now calculate
    lineartime_t occurrence;
    if(endDateFromCount(
      occurrence,
      start,
      freq,freqmod,
      interval,
      fmask,lmask,
      count,
      occurrencecount,
      aFuncContextP->getDbgLogger()
    )) {
      // successful, set timestamp in same context as start timestamp had
      static_cast<TTimestampField *>(aTermP)->setTimestampAndContext(occurrence,tctx);
    }
    else {
      // unsuccessful
      aTermP->unAssign();
    }
  } // func_Recurrence_Date


  // integer RECURRENCE_COUNT(
  //           timestamp start,
  //           string rr_freq, integer interval,
  //           integer fmask, integer lmask,
  //           boolean occurrencecount,
  //           timestamp occurrence
  //         )
  static void func_Recurrence_Count(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    string rr_freq;
    timecontext_t tctx;
    // - start time context is used for rule evaluation
    lineartime_t start = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(0))->getTimestampAs(TCTX_UNKNOWN,&tctx);
    aFuncContextP->getLocalVar(1)->getAsString(rr_freq);
    char freq = rr_freq.size()>0 ? rr_freq[0] : ' ';
    char freqmod = rr_freq.size()>1 ? rr_freq[1] : ' ';
    sInt16 interval = aFuncContextP->getLocalVar(2)->getAsInteger();
    fieldinteger_t fmask = aFuncContextP->getLocalVar(3)->getAsInteger();
    fieldinteger_t lmask = aFuncContextP->getLocalVar(4)->getAsInteger();
    bool occurrencecount = aFuncContextP->getLocalVar(5)->getAsBoolean();
    // - get end date / recurrence to get count for
    //   Note: this is obtained in the same context as start, even if it might be in another context
    lineartime_t occurrence = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(6))->getTimestampAs(tctx);
    // now calculate
    sInt16 count;
    if(countFromEndDate(
      count, occurrencecount,
      start,
      freq,freqmod,
      interval,
      fmask,lmask,
      occurrence,
      aFuncContextP->getDbgLogger()
    )) {
      // successful
      aTermP->setAsInteger(count);
    }
    else {
      // unsuccessful
      aTermP->unAssign();
    }
  } // func_Recurrence_Count



  // string MAKE_RRULE(
  //          boolean rrule2,
  //          string rr_freq, integer interval,
  //          integer fmask, integer lmask,
  //          timestamp until
  //        )
  static void func_Make_RRULE(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    // get params
    string rr_freq;
    timecontext_t untilcontext;
    // - start time context is used for rule evaluation
    bool rruleV2 = aFuncContextP->getLocalVar(0)->getAsBoolean();
    aFuncContextP->getLocalVar(1)->getAsString(rr_freq);
    char freq = rr_freq.size()>0 ? rr_freq[0] : ' ';
    char freqmod = rr_freq.size()>1 ? rr_freq[1] : ' ';
    sInt16 interval = aFuncContextP->getLocalVar(2)->getAsInteger();
    fieldinteger_t fmask = aFuncContextP->getLocalVar(3)->getAsInteger();
    fieldinteger_t lmask = aFuncContextP->getLocalVar(4)->getAsInteger();
    lineartime_t until = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(5))->getTimestampAs(TCTX_UNKNOWN,&untilcontext);
    // convert to RRULE string
    string rrule;
    bool ok;
    if (rruleV2)
      ok = internalToRRULE2(rrule,freq,freqmod,interval,fmask,lmask,until,untilcontext,aFuncContextP->getDbgLogger());
    else
      ok = internalToRRULE1(rrule,freq,freqmod,interval,fmask,lmask,until,untilcontext,aFuncContextP->getDbgLogger());
    if (ok) {
      // successful
      aTermP->setAsString(rrule);
    }
    else {
      // unsuccessful
      aTermP->unAssign();
    }
  } // func_Make_RRULE


  // boolean PARSE_RRULE(
  //           boolean rruleV2,
  //           string rrule,
  //           timestamp start,
  //           string &rr_freq, integer &interval,
  //           integer &fmask, integer &lmask,
  //           timestamp &until
  //         )
  static void func_Parse_RRULE(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    timecontext_t startcontext;
    char freq[3] = {' ', ' ', 0};
    sInt16 interval;
    fieldinteger_t fmask,lmask;
    lineartime_t until;
    timecontext_t untilcontext;

    // get params
    // - start time context is used for rule evaluation
    bool rruleV2 = aFuncContextP->getLocalVar(0)->getAsBoolean();
    string rrule;
    aFuncContextP->getLocalVar(1)->getAsString(rrule);
    lineartime_t start = static_cast<TTimestampField *>(aFuncContextP->getLocalVar(2))->getTimestampAs(TCTX_UNKNOWN,&startcontext);
    bool ok;
    if (rruleV2)
      ok = RRULE2toInternal(rrule.c_str(),start,startcontext,freq[0],freq[1],interval,fmask,lmask,until,untilcontext,aFuncContextP->getDbgLogger());
    else
      ok = RRULE1toInternal(rrule.c_str(),start,startcontext,freq[0],freq[1],interval,fmask,lmask,until,untilcontext,aFuncContextP->getDbgLogger());
    if (ok) {
      // successful
      // - save return values
      aFuncContextP->getLocalVar(3)->setAsString(freq);
      aFuncContextP->getLocalVar(4)->setAsInteger(interval);
      aFuncContextP->getLocalVar(5)->setAsInteger(fmask);
      aFuncContextP->getLocalVar(6)->setAsInteger(lmask);
      static_cast<TTimestampField *>(aFuncContextP->getLocalVar(7))->setTimestampAndContext(until, untilcontext);
    }
    aTermP->setAsBoolean(ok);
  } // func_Parse_RRULE



  // integer PARSEEMAILSPEC(string emailspec, string &name, string &email)
  static void func_ParseEmailSpec(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string spec,name,addr;
    aFuncContextP->getLocalVar(0)->getAsString(spec);
    cAppCharP e = parseRFC2822AddrSpec(spec.c_str(),name,addr);
    aTermP->setAsInteger(e-spec.c_str()); // return number of chars parsed
    aFuncContextP->getLocalVar(1)->setAsString(name);
    aFuncContextP->getLocalVar(2)->setAsString(addr);
  } // func_ParseEmailSpec


  // string MAKEEMAILSPEC(string name, string email)
  static void func_MakeEmailSpec(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string spec,name,addr;
    aFuncContextP->getLocalVar(0)->getAsString(name);
    aFuncContextP->getLocalVar(1)->getAsString(addr);
    makeRFC2822AddrSpec(name.c_str(),addr.c_str(),spec);
    aTermP->setAsString(spec); // return RFC2822 email address specification
  } // func_MakeEmailSpec


  // helper to create profile handler by name
  // - returns NULL if no such profile name or profile's field list does not match the item's fieldlist
  static TProfileHandler *newProfileHandlerByName(cAppCharP aProfileName, TMultiFieldItem *aItemP)
  {
    // get type registry to find profile config in
    TMultiFieldDatatypesConfig *mufcP;
    GET_CASTED_PTR(mufcP,TMultiFieldDatatypesConfig,aItemP->getItemType()->getTypeConfig()->getParentElement(),"PARSETEXTWITHPROFILE/MAKETEXTWITHPROFILE used with non-multifield item");
    // get profile config from type registry
    TProfileConfig *profileConfig = mufcP->getProfile(aProfileName);
    if (profileConfig) {
      // create a profile handler for the item type
      return profileConfig->newProfileHandler(aItemP->getItemType());
    }
    return NULL; // no such profile
  }


  // integer PARSETEXTWITHPROFILE(string textformat, string profileName [, int mode = 0 = default [, string remoteRuleName = "" = other]])
  static void func_ParseTextWithProfile(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    bool ok = false;
    if (aFuncContextP->fParentContextP) {
      // get the item to work with
      TMultiFieldItem *itemP = aFuncContextP->fParentContextP->fTargetItemP;
      // get a handler by name
      string s;
      aFuncContextP->getLocalVar(1)->getAsString(s);
      TProfileHandler *profileHandlerP = newProfileHandlerByName(s.c_str(), itemP);
      if (profileHandlerP) {
        // now we can convert
        // - set the mode code (none = 0 = default)
        profileHandlerP->setProfileMode(aFuncContextP->getLocalVar(2)->getAsInteger());
        profileHandlerP->setRelatedDatastore(NULL); // no datastore in particular is related
        #ifndef NO_REMOTE_RULES
        // - try to find remote rule
        TItemField *field = aFuncContextP->getLocalVar(3);
        if (field) {
          field->getAsString(s);
          if (!s.empty())
            profileHandlerP->setRemoteRule(s);
        }
        #endif
        // - convert
        aFuncContextP->getLocalVar(0)->getAsString(s);
        ok = profileHandlerP->parseText(s.c_str(), s.size(), *itemP);
        // - forget
        delete profileHandlerP;
      }
    }
    aTermP->setAsBoolean(ok);
  } // func_ParseTextWithProfile


  // string MAKETEXTWITHPROFILE(string profileName [, int mode [, string remoteRuleName = "" = other] ])
  static void func_MakeTextWithProfile(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    if (aFuncContextP->fParentContextP) {
      // get the item to work with
      TMultiFieldItem *itemP = aFuncContextP->fParentContextP->fTargetItemP;
      // get a handler by name
      string s;
      aFuncContextP->getLocalVar(0)->getAsString(s);
      TProfileHandler *profileHandlerP = newProfileHandlerByName(s.c_str(), itemP);
      if (profileHandlerP) {
        // now we can convert
        // - set the mode code (none = 0 = default)
        profileHandlerP->setProfileMode(aFuncContextP->getLocalVar(1)->getAsInteger());
        profileHandlerP->setRelatedDatastore(NULL); // no datastore in particular is related
        #ifndef NO_REMOTE_RULES
        // - try to find remote rule
        TItemField *field = aFuncContextP->getLocalVar(2);
        if (field) {
          field->getAsString(s);
          if (!s.empty())
            profileHandlerP->setRemoteRule(s);
        }
        #endif
        // - convert, after clearing the string (some generateText() implementations
        // append instead of overwriting)
        s = "";
        profileHandlerP->generateText(*itemP,s);
        aTermP->setAsString(s); // return text generated according to profile

        // - forget
        delete profileHandlerP;
      }
    }
  } // func_MakeTextWithProfile



}; // TBuiltinStdFuncs


const uInt8 param_oneTimestamp[] = { VAL(fty_timestamp) };
const uInt8 param_oneInteger[] = { VAL(fty_integer) };
const uInt8 param_oneString[] = { VAL(fty_string) };
const uInt8 param_oneVariant[] = { VAL(fty_none) };
const uInt8 param_oneOptInteger[] = { OPTVAL(fty_integer) };

const uInt8 param_Random[] = { VAL(fty_integer), OPTVAL(fty_integer) };
const uInt8 param_SetTimezone[] = { REF(fty_timestamp), VAL(fty_none) };
const uInt8 param_SetFloating[] = { REF(fty_timestamp) };
const uInt8 param_ConvertToZone[] = { VAL(fty_timestamp), VAL(fty_none), OPTVAL(fty_integer) };
const uInt8 param_ConvertToUserZone[] = { VAL(fty_timestamp), OPTVAL(fty_integer) };
const uInt8 param_Shellexecute[] = { VAL(fty_string), VAL(fty_string), OPTVAL(fty_integer) };
const uInt8 param_substr[] = { VAL(fty_string), VAL(fty_integer), OPTVAL(fty_integer) };
const uInt8 param_size[] = { REF(fty_none) };
const uInt8 param_Normalized[] = { REF(fty_none) };
const uInt8 param_find[] = { VAL(fty_string), VAL(fty_string), OPTVAL(fty_integer) };
const uInt8 param_compare[] = { VAL(fty_none), VAL(fty_none) };
const uInt8 param_contains[] = { REF(fty_none), VAL(fty_none), OPTVAL(fty_integer) };
const uInt8 param_append[] = { REF(fty_none), VAL(fty_none) };
const uInt8 param_swap[] = { REF(fty_none), REF(fty_none) };
const uInt8 param_isAvailable[] = { REF(fty_none) };
const uInt8 param_setFieldOptions[] = { REF(fty_none), VAL(fty_integer), OPTVAL(fty_integer), OPTVAL(fty_integer), OPTVAL(fty_integer) };
const uInt8 param_typename[] = { VAL(fty_none) };
const uInt8 param_SetSessionVar[] = { VAL(fty_string), VAL(fty_none) };
const uInt8 param_SetDebugOptions[] = { VAL(fty_string), VAL(fty_integer) };
const uInt8 param_Recurrence_Date[] = { VAL(fty_timestamp), VAL(fty_string), VAL(fty_integer), VAL(fty_integer), VAL(fty_integer), VAL(fty_integer), VAL(fty_integer) };
const uInt8 param_Recurrence_Count[] = { VAL(fty_timestamp), VAL(fty_string), VAL(fty_integer), VAL(fty_integer), VAL(fty_integer), VAL(fty_integer), VAL(fty_timestamp) };
const uInt8 param_Make_RRULE[] = { VAL(fty_integer), VAL(fty_string), VAL(fty_integer), VAL(fty_integer), VAL(fty_integer), VAL(fty_timestamp) };
const uInt8 param_Parse_RRULE[] = { VAL(fty_integer), VAL(fty_string), VAL(fty_timestamp), REF(fty_string), REF(fty_integer), REF(fty_integer), REF(fty_integer), REF(fty_timestamp) };
const uInt8 param_AlldayCount[] = { VAL(fty_timestamp), VAL(fty_timestamp), OPTVAL(fty_integer), OPTVAL(fty_integer) };
const uInt8 param_MakeAllday[] = { REF(fty_timestamp), REF(fty_timestamp), OPTVAL(fty_integer) };
const uInt8 param_NumFormat[] = { VAL(fty_integer), VAL(fty_integer), OPTVAL(fty_string), OPTVAL(fty_string) };
const uInt8 param_Explode[] = { VAL(fty_string), REFARR(fty_none) };
const uInt8 param_parseEmailSpec[] = { VAL(fty_string), REF(fty_string), REF(fty_string) };
const uInt8 param_makeEmailSpec[] = { VAL(fty_string), VAL(fty_string) };
const uInt8 param_parseTextWithProfile[] = { VAL(fty_string), VAL(fty_string), OPTVAL(fty_integer), OPTVAL(fty_string) };
const uInt8 param_makeTextWithProfile[] = { VAL(fty_string), OPTVAL(fty_integer), OPTVAL(fty_string) };


#ifdef REGEX_SUPPORT
const uInt8 param_regexfind[] = { VAL(fty_string), VAL(fty_string), OPTVAL(fty_integer) };
const uInt8 param_regexmatch[] = { VAL(fty_string), VAL(fty_string), VAL(fty_integer), REF(fty_none) };
const uInt8 param_regexreplace[] = { VAL(fty_string), VAL(fty_string), VAL(fty_string), OPTVAL(fty_integer), OPTVAL(fty_integer) };
const uInt8 param_regexsplit[] = { VAL(fty_string), VAL(fty_string), REFARR(fty_none), OPTVAL(fty_integer) };
#endif

// builtin function table
const TBuiltInFuncDef BuiltInFuncDefs[] = {
  { "ABS", TBuiltinStdFuncs::func_Abs, fty_integer, 1, param_oneInteger },
  { "SIGN", TBuiltinStdFuncs::func_Sign, fty_integer, 1, param_oneInteger },
  { "RANDOM", TBuiltinStdFuncs::func_Random, fty_integer, 2, param_Random },
  { "NUMFORMAT", TBuiltinStdFuncs::func_NumFormat, fty_string, 4, param_NumFormat },
  { "NORMALIZED", TBuiltinStdFuncs::func_Normalized, fty_string, 1, param_Normalized },
  { "ISAVAILABLE", TBuiltinStdFuncs::func_IsAvailable, fty_integer, 1, param_isAvailable },
  { "SETFIELDOPTIONS", TBuiltinStdFuncs::func_SetFieldOptions, fty_none, 5, param_setFieldOptions },
  { "ITEMDATATYPE", TBuiltinStdFuncs::func_ItemDataType, fty_string, 0, NULL },
  { "ITEMTYPENAME", TBuiltinStdFuncs::func_ItemTypeName, fty_string, 0, NULL },
  { "ITEMTYPEVERS", TBuiltinStdFuncs::func_ItemTypeVers, fty_string, 0, NULL },
  { "EXPLODE", TBuiltinStdFuncs::func_Explode, fty_string, 2, param_Explode },
  { "SUBSTR", TBuiltinStdFuncs::func_Substr, fty_string, 3, param_substr },
  { "LENGTH", TBuiltinStdFuncs::func_Length, fty_integer, 1, param_oneString },
  { "SIZE", TBuiltinStdFuncs::func_Size, fty_integer, 1, param_size },
  { "FIND", TBuiltinStdFuncs::func_Find, fty_integer, 3, param_find },
  { "RFIND", TBuiltinStdFuncs::func_RFind, fty_integer, 3, param_find },
  #ifdef REGEX_SUPPORT
  { "REGEX_FIND", TBuiltinStdFuncs::func_Regex_Find, fty_integer, 3, param_regexfind },
  { "REGEX_MATCH", TBuiltinStdFuncs::func_Regex_Match, fty_integer, 4, param_regexmatch },
  { "REGEX_SPLIT", TBuiltinStdFuncs::func_Regex_Split, fty_integer, 4, param_regexsplit },
  { "REGEX_REPLACE", TBuiltinStdFuncs::func_Regex_Replace, fty_string, 5, param_regexreplace },
  #endif
  { "COMPARE", TBuiltinStdFuncs::func_Compare, fty_integer, 2, param_compare },
  { "CONTAINS", TBuiltinStdFuncs::func_Contains, fty_integer, 3, param_contains },
  { "APPEND", TBuiltinStdFuncs::func_Append, fty_none, 2, param_append },
  { "UPPERCASE", TBuiltinStdFuncs::func_UpperCase, fty_string, 1, param_oneString },
  { "LOWERCASE", TBuiltinStdFuncs::func_LowerCase, fty_string, 1, param_oneString },
  { "SWAP", TBuiltinStdFuncs::func_Swap, fty_none, 2, param_swap },
  { "TYPENAME", TBuiltinStdFuncs::func_TypeName, fty_string, 1, param_oneVariant },
  { "REMOTERULENAME", TBuiltinStdFuncs::func_Remoterulename, fty_string, 0, NULL },
  { "ISACTIVERULE", TBuiltinStdFuncs::func_isActiveRule, fty_integer, 1, param_oneString },
  { "LOCALURI", TBuiltinStdFuncs::func_LocalURI, fty_string, 0, NULL },
  { "NOW", TBuiltinStdFuncs::func_Now, fty_timestamp, 0, NULL },
  { "SYSTEMNOW", TBuiltinStdFuncs::func_SystemNow, fty_timestamp, 0, NULL },
  { "DBNOW", TBuiltinStdFuncs::func_DbNow, fty_timestamp, 0, NULL },
  { "ZONEOFFSET", TBuiltinStdFuncs::func_ZoneOffset, fty_integer, 1, param_oneTimestamp },
  { "TIMEZONE", TBuiltinStdFuncs::func_Timezone, fty_string, 1, param_oneTimestamp },
  { "VTIMEZONE", TBuiltinStdFuncs::func_VTimezone, fty_string, 1, param_oneTimestamp },
  { "SETTIMEZONE", TBuiltinStdFuncs::func_SetTimezone, fty_none, 2, param_SetTimezone },
  { "SETFLOATING", TBuiltinStdFuncs::func_SetFloating, fty_none, 1, param_SetFloating },
  { "CONVERTTOZONE", TBuiltinStdFuncs::func_ConvertToZone, fty_timestamp, 3, param_ConvertToZone },
  { "CONVERTTOUSERZONE", TBuiltinStdFuncs::func_ConvertToUserZone, fty_timestamp, 2, param_ConvertToUserZone },
  { "USERTIMEZONE", TBuiltinStdFuncs::func_UserTimezone, fty_string, 0, NULL },
  { "SETUSERTIMEZONE", TBuiltinStdFuncs::func_SetUserTimezone, fty_none, 1, param_oneVariant },
  { "ISDATEONLY", TBuiltinStdFuncs::func_IsDateOnly, fty_integer, 1, param_oneTimestamp },
  { "DATEONLY", TBuiltinStdFuncs::func_DateOnly, fty_timestamp, 1, param_oneTimestamp },
  { "TIMEONLY", TBuiltinStdFuncs::func_TimeOnly, fty_timestamp, 1, param_oneTimestamp },
  { "ISDURATION", TBuiltinStdFuncs::func_IsDuration, fty_integer, 1, param_oneTimestamp },
  { "DURATION", TBuiltinStdFuncs::func_Duration, fty_timestamp, 1, param_oneTimestamp },
  { "POINTINTIME", TBuiltinStdFuncs::func_PointInTime, fty_timestamp, 1, param_oneTimestamp },
  { "ISFLOATING", TBuiltinStdFuncs::func_IsFloating, fty_integer, 1, param_oneTimestamp },
  { "WEEKDAY", TBuiltinStdFuncs::func_Weekday, fty_integer, 1, param_oneTimestamp },
  { "SECONDS", TBuiltinStdFuncs::func_Seconds, fty_integer, 1, param_oneInteger },
  { "MILLISECONDS", TBuiltinStdFuncs::func_Milliseconds, fty_integer, 1, param_oneInteger },
  { "SLEEPMS", TBuiltinStdFuncs::func_SleepMS, fty_none, 1, param_oneInteger },
  { "TIMEUNITS", TBuiltinStdFuncs::func_Timeunits, fty_integer, 1, param_oneInteger },
  { "DAYUNITS", TBuiltinStdFuncs::func_Dayunits, fty_integer, 1, param_oneInteger },
  { "MONTHDAYS", TBuiltinStdFuncs::func_MonthDays, fty_integer, 1, param_oneTimestamp },
  { "DEBUGMESSAGE", TBuiltinStdFuncs::func_Debugmessage, fty_none, 1, param_oneString },
  { "DEBUGSHOWVARS", TBuiltinStdFuncs::func_DebugShowVars, fty_none, 0, NULL },
  { "DEBUGSHOWITEM", TBuiltinStdFuncs::func_DebugShowItem, fty_none, 1, param_oneOptInteger },
  { "SETDEBUGOPTIONS", TBuiltinStdFuncs::func_SetDebugOptions, fty_none, 2, param_SetDebugOptions },
  { "SETDEBUGMASK", TBuiltinStdFuncs::func_SetDebugMask, fty_none, 1, param_oneInteger },
  { "SETXMLTRANSLATE", TBuiltinStdFuncs::func_SetXMLTranslate, fty_none, 1, param_oneInteger },
  { "SETMSGDUMP", TBuiltinStdFuncs::func_SetMsgDump, fty_none, 1, param_oneInteger },
  { "GETDEBUGMASK", TBuiltinStdFuncs::func_GetDebugMask, fty_integer, 0, NULL },
  { "REQUESTMAXTIME", TBuiltinStdFuncs::func_RequestMaxTime, fty_none, 1, param_oneInteger },
  { "REQUESTMINTIME", TBuiltinStdFuncs::func_RequestMinTime, fty_none, 1, param_oneInteger },
  { "SHELLEXECUTE", TBuiltinStdFuncs::func_Shellexecute, fty_integer, 3, param_Shellexecute },
  { "READ",  TBuiltinStdFuncs::func_Read, fty_string, 1, param_oneString },
  { "URITOPATH",  TBuiltinStdFuncs::func_URIToPath, fty_string, 1, param_oneString },
  { "SESSIONVAR", TBuiltinStdFuncs::func_SessionVar, fty_none, 1, param_oneString },
  { "SETSESSIONVAR", TBuiltinStdFuncs::func_SetSessionVar, fty_none, 2, param_SetSessionVar },
  { "ABORTSESSION", TBuiltinStdFuncs::func_AbortSession, fty_none, 1, param_oneInteger },
  { "SETDEBUGLOG", TBuiltinStdFuncs::func_SetDebugLog, fty_none, 1, param_oneInteger },
  { "SETLOG", TBuiltinStdFuncs::func_SetLog, fty_none, 1, param_oneInteger },
  { "SETREADONLY", TBuiltinStdFuncs::func_SetReadOnly, fty_none, 1, param_oneInteger },
  { "CONFIGVAR", TBuiltinStdFuncs::func_ConfigVar, fty_string, 1, param_oneString },
  { "TREATASLOCALTIME", TBuiltinStdFuncs::func_SetTreatAsLocaltime, fty_none, 1, param_oneInteger },
  { "TREATASUTC", TBuiltinStdFuncs::func_SetTreatAsUTC, fty_none, 1, param_oneInteger },
  { "UPDATECLIENTINSLOWSYNC", TBuiltinStdFuncs::func_SetUpdateClientInSlowSync, fty_none, 1, param_oneInteger },
  { "UPDATESERVERINSLOWSYNC", TBuiltinStdFuncs::func_SetUpdateServerInSlowSync, fty_none, 1, param_oneInteger },
  { "SHOWCTCAPPROPERTIES", TBuiltinStdFuncs::func_ShowCTCapProps, fty_none, 1, param_oneInteger },
  { "SHOWTYPESIZEINCTCAP10", TBuiltinStdFuncs::func_ShowTypeSizeInCTCap10, fty_none, 1, param_oneInteger },
  { "ENUMDEFAULTPROPPARAMS", TBuiltinStdFuncs::func_EnumDefaultPropParams, fty_none, 1, param_oneInteger },
  { "RECURRENCE_DATE", TBuiltinStdFuncs::func_Recurrence_Date, fty_timestamp, 7, param_Recurrence_Date },
  { "RECURRENCE_COUNT", TBuiltinStdFuncs::func_Recurrence_Count, fty_integer, 7, param_Recurrence_Count },
  { "MAKE_RRULE", TBuiltinStdFuncs::func_Make_RRULE, fty_string, 6, param_Make_RRULE },
  { "PARSE_RRULE", TBuiltinStdFuncs::func_Parse_RRULE, fty_integer, 8, param_Parse_RRULE },
  { "PARSEEMAILSPEC", TBuiltinStdFuncs::func_ParseEmailSpec, fty_integer, 3, param_parseEmailSpec },
  { "MAKEEMAILSPEC", TBuiltinStdFuncs::func_MakeEmailSpec, fty_string, 2, param_makeEmailSpec },
  { "PARSETEXTWITHPROFILE", TBuiltinStdFuncs::func_ParseTextWithProfile, fty_integer, 4, param_parseTextWithProfile },
  { "MAKETEXTWITHPROFILE", TBuiltinStdFuncs::func_MakeTextWithProfile, fty_string, 3, param_makeTextWithProfile },
  { "SYNCMLVERS", TBuiltinStdFuncs::func_SyncMLVers, fty_string, 0, NULL },
  { "ALLDAYCOUNT", TBuiltinStdFuncs::func_AlldayCount, fty_integer, 4, param_AlldayCount },
  { "MAKEALLDAY", TBuiltinStdFuncs::func_MakeAllday, fty_integer, 3, param_MakeAllday },
};

const TFuncTable BuiltInFuncTable = {
  sizeof(BuiltInFuncDefs) / sizeof(TBuiltInFuncDef), // size of table
  BuiltInFuncDefs, // table pointer
  NULL // no chain func
};


/*
 * Implementation of TScriptContext
 */

/* public TScriptContext members */


TScriptContext::TScriptContext(TSyncAppBase *aAppBaseP, TSyncSession *aSessionP) :
  fAppBaseP(aAppBaseP), // save syncappbase link, must always exist
  fSessionP(aSessionP), // save session, can be NULL
  fNumVars(0), // number of instantiated vars
  fNumParams(0),
  fFieldsP(NULL), // no field contents yet
  scriptname(NULL), // no script name known yet
  linesource(NULL),
  executing(false),
  debugon(false),
  fTargetItemP(NULL),
  fReferenceItemP(NULL),
  fParentContextP(NULL)
{
  fVarDefs.clear();
} // TScriptContext::TScriptContext


TScriptContext::~TScriptContext()
{
  clear();
} // TScriptContext::~TScriptContext


// Reset context (clear all variables and definitions)
void TScriptContext::clear(void)
{
  // clear actual fields
  clearFields();
  // clear definitions
  TVarDefs::iterator pos;
  for (pos=fVarDefs.begin(); pos!=fVarDefs.end(); pos++) {
    if (*pos) delete (*pos);
  }
  fVarDefs.clear();
} // TScriptContext::clear


GZones *TScriptContext::getSessionZones(void)
{
  return
    fSessionP ? fSessionP->getSessionZones() : NULL;
} // TScriptContext::getSessionZones


#ifdef SYDEBUG
// get debug logger
TDebugLogger *TScriptContext::getDbgLogger(void)
{
  // use session logger if linked to a session
  if (fSessionP)
    return fSessionP->getDbgLogger();
  // otherwise, use global logger
  return fAppBaseP ? fAppBaseP->getDbgLogger() : NULL;
} // TScriptContext::getDbgLogger


uInt32 TScriptContext::getDbgMask(void)
{
  // use session logger if linked to a session
  if (fSessionP)
    return fSessionP->getDbgMask();
  // otherwise, use global logger
  return fAppBaseP ? fAppBaseP->getDbgMask() : 0;
} // TScriptContext::getDbgMask
#endif



// Reset context (clear all variables and definitions)
void TScriptContext::clearFields(void)
{
  if (fFieldsP) {
    // clear local vars (fields), but not references
    TVarDefs::iterator pos;
    for (pos=fVarDefs.begin(); pos!=fVarDefs.end(); pos++) {
      sInt16 i=(*pos)->fIdx;
      if (i>=fNumVars) break; // all instantiated vars done, stop even if more might be defined
      if (fFieldsP[i]) {
        if (!(*pos)->fIsRef)
          delete fFieldsP[i]; // delete field object (but only if not reference)
        fFieldsP[i]=NULL;
      }
    }
    // clear array of field pointers
    delete [] fFieldsP;
    fFieldsP=NULL;
    fNumVars=0;
  }
} // TScriptContext::clearFields



// check for identifier
static bool isidentchar(appChar c) {
  return isalnum(c) || c=='_';
} // isidentchar


// adds source line (including source text, if selected) to token stream
static void addSourceLine(uInt16 aLine, const char *aText, string &aScript, bool aIncludeSource, uInt16 &aLastIncludedLine)
{
  aScript+=TK_SOURCELINE; // token
  #ifdef SYDEBUG
  sInt16 linelen=0;
  // line #0 is script/function name and must be included anyway
  if (aIncludeSource && (aLine>aLastIncludedLine || aLine==0)) {
    const char *tt=aText;
    while (*tt && *tt!=0x0D && *tt!=0x0A) ++tt;
    linelen=tt-aText+1; // room for the terminator
    if (linelen>250) linelen=250; // limit size (2 chars needed for number, 3 reserved)
    // Note: even empty line will have at least one char (the terminator)
    aScript+=(appChar)(2+linelen); // length of additional data
  }
  else {
    // Note: not-included line will have no extra data (not even the terminator), so
    //       it can be distinguished from empty line
    aIncludeSource=false;
    aScript+=(appChar)(2); // length of additional data
  }
  #else
  aScript+=(appChar)(2); // length of additional data
  #endif
  // add source line number
  aScript+=(appChar)(aLine>>8); // source line
  aScript+=(appChar)(aLine & 0xFF);
  // add source itself
  #ifdef SYDEBUG
  if (aIncludeSource) {
    aScript.append(aText,linelen-1);
    aScript+=(appChar)(0); // add terminator
    aLastIncludedLine = aLine;
  }
  #endif
} // addSourceLine


// Tokenize input string
void TScriptContext::Tokenize(TSyncAppBase *aAppBaseP, cAppCharP aScriptName, sInt32 aLine, cAppCharP aScriptText, string &aTScript, const TFuncTable *aContextFuncs, bool aFuncHeader, bool aNoDeclarations, TMacroArgsArray *aMacroArgsP)
{
  string itm;
  string macro;
  appChar c,c2;
  appChar token,lasttoken=0;
  cAppCharP text = aScriptText;
  cAppCharP p;
  uInt16 line=aLine; // script starts here
  sInt16 enu;

  // clear output
  aTScript.erase();

  // debug info if script debugging is enabled in configuration
  bool includesource =
    #ifdef SYDEBUG
    aAppBaseP->getRootConfig()->fDebugConfig.fDebug & DBG_SCRIPTS;
    #else
    false;
    #endif
  uInt16 lastincludedline = 0;

  if (*text) {
    #ifdef SYDEBUG
    // insert script name as line #0 in all but completely empty scripts (or functions)
    if (aScriptName) addSourceLine(0,aScriptName,aTScript,true,lastincludedline);
    #endif
    // insert source line identification token for start of script for all but completely empty scripts
    addSourceLine(line,text,aTScript,includesource,lastincludedline);
  }
  // marco argument expansion
  // Note: $n (with n=1,2,3...9) is expanded before any other processing. To insert e.g. $2 literally, use $$2.
  //       $n macros that can't be expanded will be left in the text AS IS
  string itext;
  if (aMacroArgsP) {
    itext = text; // we need a string to substitute macro args in
    size_t i = 0;
    while (i<itext.size()) {
      c=itext[i++];
      if (c=='$') {
        // could be macro argument
        c=itext[i];
        if (c=='$') {
          // $$ expands to $
          itext.erase(i, 1); // erase second occurrence of $
          continue;
        }
        else if (isdigit(c)) {
          ssize_t argidx = c-'1';
          if (argidx>=0 && (size_t)argidx<aMacroArgsP->size()) {
          	// found macro argument, replace in input string
            itext.replace(i-1, 2, (*aMacroArgsP)[argidx]);
            // no nested macro argument eval, just advance pointer behind replacement text
            i += (*aMacroArgsP)[argidx].size()-1;
            // check next char
            continue;
          }
        }
      }
    }
    // now use expanded version of text for tokenizing
    text = itext.c_str();
  }
  // actual tokenisation
  cAppCharP textstart = text;
  SYSYNC_TRY {
    // process text
    while (*text) {
      // get next token
      token=0; // none yet
      // - skip spaces (but not line ends)
      while (*text==' ' || *text=='\t') text++;
      // - dispatch different types of tokens
      c=*text++;
      if (c==0) break; // done with script
      // - check input now
      if (isdigit(c)) {
        // numeric literal
        p=text-1; // beginning of literal
        while (isalnum(*text)) text++;
        // - p=start, text=past end of numeric literal
        itm.assign(p,text-p);
        // code literal into token string
        aTScript+=TK_NUMERIC_LITERAL; // token
        aTScript+=(appChar)(itm.size()); // length of additional data
        aTScript.append(itm);
      }
      else if (c=='"') {
        // string literal, parse
        itm.erase();
        while ((c=*text)) {
          text++;
          if (c=='"') { break; } // done
          if (c=='\\') {
            // escape char
            c2=*text++;
            if (!c2) break; // escape without anything following -> done
            else if (c2=='n') c='\n'; // internal line end
            else if (c2=='t') c='\t'; // internal tab
            else if (c2=='x') {
              // hex char spec
              uInt16 sh;
              text+=HexStrToUShort(text,sh,2);
              c=(appChar)sh;
            }
            else c=c2; // simply use char following the escape char
          }
          // now add
          if (c) itm+=c;
        }
        // code literal into token string
        aTScript+=TK_STRING_LITERAL; // token
        aTScript+=(appChar)(itm.size()); // length of additional data
        aTScript.append(itm);
      }
      else if (isalpha(c)) {
        // identifier
        // - get identifier
        p=text-1;
        while (isidentchar(*text)) text++;
        // - now p=start of identified, text=end
        uInt16 il=text-p;
        // - skip whitespace following identifier
        while (*text==' ' || *text=='\t') text++;
        // - check language keywords
        if (strucmp(p,"IF",il)==0) token=TK_IF;
        else if (strucmp(p,"ELSE",il)==0) token=TK_ELSE;
        else if (strucmp(p,"LOOP",il)==0) token=TK_LOOP;
        else if (strucmp(p,"WHILE",il)==0) token=TK_WHILE;
        else if (strucmp(p,"BREAK",il)==0) token=TK_BREAK;
        else if (strucmp(p,"CONTINUE",il)==0) token=TK_CONTINUE;
        else if (strucmp(p,"RETURN",il)==0) token=TK_RETURN;
        // - check special constants
        else if (strucmp(p,"EMPTY",il)==0) token=TK_EMPTY;
        else if (strucmp(p,"UNASSIGNED",il)==0) token=TK_UNASSIGNED;
        else if (strucmp(p,"TRUE",il)==0) token=TK_TRUE;
        else if (strucmp(p,"FALSE",il)==0) token=TK_FALSE;
        // - check types
        else if (StrToEnum(ItemFieldTypeNames,numFieldTypes,enu,p,il)) {
          // check if declaration and if allowed
          if (aNoDeclarations && lasttoken!=TK_OPEN_PARANTHESIS)
            SYSYNC_THROW(TTokenizeException(aScriptName, "no local variable declarations allowed in this script",textstart,text-textstart,line));
          // code type into token
          aTScript+=TK_TYPEDEF; // token
          aTScript+=1; // length of additional data
          aTScript+=enu; // type
        }
        // - check function calls if in body
        else if (*text=='(' && !aFuncHeader) {
          // identifier followed by ( must be function call (if not in header of function itself)
          // - check for built-in function
          sInt16 k=0;
          while (k<BuiltInFuncTable.numFuncs) {
            if (strucmp(p,BuiltInFuncDefs[k].fFuncName,il)==0) {
              // built-in
              aTScript+=TK_FUNCTION; // token
              aTScript+=1; // length of additional data
              aTScript+=k; // built-in function index
              k=-1; // found flag
              break;
            }
            k++;
          }
          if (k>=0) {
            // no built-in base function, could be context-related function
            k=0; // function index (may span several chain links)
            // - start with passed functable
            TFuncTable *functableP = (TFuncTable *)aContextFuncs;
            while(functableP) {
              // get the function table properties
              sInt16 fidx,numfuncs = functableP->numFuncs;
              const TBuiltInFuncDef *funcs = functableP->funcDefs;
              // process this func table
              for (fidx=0; fidx<numfuncs; fidx++) {
                if (strucmp(p,funcs[fidx].fFuncName,il)==0) {
                  // built-in
                  aTScript+=TK_CONTEXTFUNCTION; // token
                  aTScript+=1; // length of additional data
                  aTScript+=k+fidx; // built-in function index
                  k=-1; // found flag
                  break;
                }
              }
              // exit loop if found
              if (k<0) break;
              // not found, try to chain
              if (functableP->chainFunc) {
                // obtain next function table (caller context pointer is irrelevant here)
                k+=numfuncs; // index for next chained table starts at end of indexes for current table
                void *ctx=NULL;
                functableP=(TFuncTable *)functableP->chainFunc(ctx);
              }
              else {
                functableP=NULL; // end chaining loop
              }
            } // while
            if (k>=0) {
              // no built-in nor context-built-in found, assume user-defined
              aTScript+=TK_USERFUNCTION; // token
              aTScript+=(appChar)(il+1); // length of additional data
              aTScript+=VARIDX_UNDEFINED; // no function index defined yet
              aTScript.append(p,il); // identifier name
            }
          }
        }
        // - check object qualifiers
        else if (*text=='.') {
          // must be qualifier
          uInt8 objidx=OBJ_AUTO;
          if (strucmp(p,"LOCAL",il)==0) objidx=OBJ_LOCAL;
          else if (strucmp(p,"OLD",il)==0) objidx=OBJ_REFERENCE;
          else if (strucmp(p,"LOOSING",il)==0) objidx=OBJ_REFERENCE;
          else if (strucmp(p,"REFERENCE",il)==0) objidx=OBJ_REFERENCE;
          else if (strucmp(p,"NEW",il)==0) objidx=OBJ_TARGET;
          else if (strucmp(p,"WINNING",il)==0) objidx=OBJ_TARGET;
          else if (strucmp(p,"TARGET",il)==0) objidx=OBJ_TARGET;
          else
            SYSYNC_THROW(TTokenizeException(aScriptName,"unknown object name",textstart,text-textstart,line));
          text++; // skip object qualifier
          aTScript+=TK_OBJECT; // token
          aTScript+=1; // length of additional data
          aTScript+=objidx; // object index
        }
        else {
          // generic identifier, must be some kind of variable reference
          aTScript+=TK_IDENTIFIER; // token
          aTScript+=(appChar)(il+1); // length of additional data
          aTScript+=VARIDX_UNDEFINED; // no variable index defined yet
          aTScript.append(p,il); // identifier name
        }
      } // if identifier
      else {
        // get next char for double-char tokens
        c2=*text;
        // check special single chars
        switch (c) {
          // - macro
          case '$': {
            // get macro name
            p=text;
            while (isidentchar(*text)) text++;
            if (text==p)
              SYSYNC_THROW(TTokenizeException(aScriptName,"missing macro name after $",textstart,text-textstart,line));
            itm.assign(p,text-p);
            // see if we have such a macro
            TScriptConfig *cfgP = aAppBaseP->getRootConfig()->fScriptConfigP;
            TStringToStringMap::iterator pos = cfgP->fScriptMacros.find(itm);
            if (pos==cfgP->fScriptMacros.end())
              SYSYNC_THROW(TTokenizeException(aScriptName,"unknown macro",textstart,p-1-textstart,line));
            TMacroArgsArray macroArgs;
            // check for macro arguments
            if (*text=='(') {
              // Macro has Arguments
              text++;
              string arg;
              // Note: closing brackets and commas must be escaped when used as part of a macro argument
              while (*text) {
                c=*text++;
                if (c==',' || c==')') {
                  // end of argument
                  macroArgs.push_back(arg); // save it in array
                  arg.erase();
                  if (c==')') break; // end of macro
                  continue; // skip comma, next arg
                }
                else if (c=='\\') {
                  if (*text==0) break; // end of string
                  // escaped - use next char w/o testing for , or )
                  c=*text++;
                }
                // add to argument string
                arg += c;
              }
            }
            // continue tokenizing with macro text
            TScriptContext::Tokenize(
              aAppBaseP,
              itm.c_str(), // pass macro name as "script" name
              1, // line number relative to beginning of macro
              (*pos).second.c_str(), // use macro text as script text
              macro, // produce tokenized macro here
              aContextFuncs, // same context
              false, // not in function header
              aNoDeclarations, // same condition
              &macroArgs // macro arguments
            );
            // append tokenized macro to current script
            aTScript+=macro;
            // continue with normal text
            break;
          }
          // - grouping
          case '(': token=TK_OPEN_PARANTHESIS; break; // open subexpression/argument paranthesis
          case ')': token=TK_CLOSE_PARANTHESIS; break; // close subexpression/argument paranthesis
          case ',': token=TK_LIST_SEPARATOR; break; // comma for separating arguments
          case '{': token=TK_BEGIN_BLOCK; aFuncHeader=false; break; // begin block (and start of function body)
          case '}': token=TK_END_BLOCK; break; // end block
          case ';': token=TK_END_STATEMENT; break; // end statement
          case '[': token=TK_OPEN_ARRAY; break; // open array paranthesis
          case ']': token=TK_CLOSE_ARRAY; break; // close array paranthesis
          // line ends
          case 0x0D:
            if (c2==0x0A) text++; // skip LF of CRLF sequence as well to make sure it is not counted twice
            // otherwise treat like LF
          case 0x0A:
            // new line begins : insert source line identification token (and source of next line, if any)
            line++;
            addSourceLine(line,text,aTScript,includesource,lastincludedline);
            break;
          // possible multi-char tokens
          case '/':
            if (c2=='/') {
              text++;
              // end-of-line comment, skip it
              do { c=*text; if (c==0 || c==0x0D || c==0x0A) break; text++; } while(true);
            }
            else if (c2=='*') {
              // C-style comment, skip until next '*/'
              text++;
              do {
                c=*text;
                if (c==0) break;
                text++; // next
                if (c=='*' && *text=='/') {
                  // end of comment
                  text++; // skip /
                  break; // end of comment
                }
                else if (c==0x0D || c==0x0A) {
                  if (*text==0x0A) text++; // skip LF of CRLF sequence as well to make sure it is not counted twice
                  // new line begins : insert source line identification token (and source of next line, if any)
                  line++;
                  addSourceLine(line,text,aTScript,includesource,lastincludedline);
                }
              } while(true);
            }
            else
              token=TK_DIVIDE; // simple division
            break;
          case '*': token=TK_MULTIPLY; break; // multiply
          case '%': token=TK_MODULUS; break; // modulus
          case '+': token=TK_PLUS; break; // add
          case '-': token=TK_MINUS; break; // subtract/unary minus
          case '^': token=TK_BITWISEXOR; break; // bitwise XOR
          case '~': token=TK_BITWISENOT; break; // bitwise not (one's complement)
          case '!':
            if (c2=='=') { token=TK_NOTEQUAL; text++; } // !=
            else token=TK_LOGICALNOT; // !
            break;
          case '=':
            if (c2=='=') { token=TK_EQUAL; text++; } // ==
            else token=TK_ASSIGN; // =
            break;
          case '>':
            if (c2=='=') { token=TK_GREATEREQUAL; text++; } // >=
            else if (c2=='>') { token=TK_SHIFTRIGHT; text++; } // >>
            else token=TK_GREATERTHAN; // >
            break;
          case '<':
            if (c2=='=') { token=TK_LESSEQUAL; text++; } // <=
            else if (c2=='<') { token=TK_SHIFTLEFT; text++; } // <<
            else if (c2=='>') { token=TK_NOTEQUAL; text++; } // <>
            else token=TK_LESSTHAN; // <
            break;
          case '&':
            if (c2=='&') { token=TK_LOGICALAND; text++; } // &&
            else token=TK_BITWISEAND; // &
            break;
          case '|':
            if (c2=='|') { token=TK_LOGICALOR; text++; } // ||
            else token=TK_BITWISEOR; // |
            break;
          default:
            SYSYNC_THROW(TTokenizeException(aScriptName,"Syntax Error",textstart,text-textstart,line));
        }
      }
      // add token if simple token found
      if (token) aTScript+=token;
      lasttoken=token; // save for differentiating casts from declarations etc.
    } // while more script text
  }
  SYSYNC_CATCH (...)
    // make sure that script with errors is not stored
    aTScript.erase();
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
} // TScriptContext::Tokenize


// tokenize and resolve user-defined function
void TScriptContext::TokenizeAndResolveFunction(TSyncAppBase *aAppBaseP, sInt32 aLine, cAppCharP aScriptText, TUserScriptFunction &aFuncDef)
{
  TScriptContext *resolvecontextP=NULL;

  Tokenize(aAppBaseP, NULL, aLine,aScriptText,aFuncDef.fFuncDef,NULL,true); // parse as function
  SYSYNC_TRY {
    // resolve identifiers
    resolvecontextP=new TScriptContext(aAppBaseP,NULL);
    resolvecontextP->ResolveIdentifiers(
      aFuncDef.fFuncDef,
      NULL, // no fields
      false, // not rebuild
      &aFuncDef.fFuncName // store name here
    );
    delete resolvecontextP;
  }
  SYSYNC_CATCH (exception &e)
    delete resolvecontextP;
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
} // TScriptContext::TokenizeAndResolveFunction



// resolve identifiers in a script, if there is a context passed at all
void TScriptContext::resolveScript(TSyncAppBase *aAppBaseP, string &aTScript,TScriptContext *&aCtxP, TFieldListConfig *aFieldListConfigP)
{
  if (aTScript.empty()) return; // no resolving needed
  if (!aCtxP) {
    // we need a context, create one
    aCtxP = new TScriptContext(aAppBaseP, NULL);
  }
  aCtxP->ResolveIdentifiers(
    aTScript,
    aFieldListConfigP,
    false
  );
} // TScriptContext::ResolveScript


// link a script into a context with already instantiated variables.
// This is for "late bound" scripts (such as rulescript) that cannot be bound at config
// but are determined only later, when their context is already instantiated.
void TScriptContext::linkIntoContext(string &aTScript,TScriptContext *aCtxP, TSyncSession *aSessionP)
{
  if (aTScript.empty() || !aCtxP) return; // no resolving needed (no script or no context)
  // resolve identfiers (no new declarations possible)
  aCtxP->ResolveIdentifiers(
    aTScript,
    NULL, // no field list
    false, // do not rebuild
    NULL, // no function name
    false // no declarations allowed
  );
} // TScriptContext::linkIntoContext


// rebuild a script context for a script, if the script is not empty
// - Script must already be resolved with ResolveIdentifiers
// - If context already exists, adds new locals to existing ones
// - If aBuildVars is set, buildVars() will be called after rebuilding variable definitions
void TScriptContext::rebuildContext(TSyncAppBase *aAppBaseP, string &aTScript,TScriptContext *&aCtxP, TSyncSession *aSessionP, bool aBuildVars)
{
  // Optimization: Nop if script is empty and NOT build var requested for already existing context (=vars from other scripts!)
  if (aTScript.empty() && !(aBuildVars && aCtxP)) return;
  // Create context if there isn't one yet
  if (!aCtxP) {
    // we need a context, create one
    aCtxP = new TScriptContext(aAppBaseP,aSessionP);
  }
  SYSYNC_TRY {
    aCtxP->ResolveIdentifiers(
      aTScript,
      NULL,
      true
    );
    if (aBuildVars) {
      // call this one, too
      buildVars(aCtxP);
    }
  }
  SYSYNC_CATCH (TScriptErrorException &e)
    // show error in log, but otherwise ignore it
    POBJDEBUGPRINTFX(aSessionP,DBG_ERROR,("Failed rebuilding script context: %s",e.what()));
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
  SYSYNC_ENDCATCH
} // TScriptContext::rebuildContext


// Builds the local variables according to definitions (clears existing vars first)
void TScriptContext::buildVars(TScriptContext *&aCtxP)
{
  if (aCtxP) {
    // instantiate the variables
    aCtxP->PrepareLocals();
  }
} // TScriptContext::buildVars


// init parsing vars
void TScriptContext::initParse(const string &aTScript, bool aExecuting)
{
  executing=aExecuting;
  bp=(cUInt8P)aTScript.c_str();  // start of script
  ep=bp+aTScript.size(); // end of script
  p=bp; // cursor, start at beginning
  np=NULL; // no next token yet
  line=0; // no line yet
  linesource=NULL; // no source yet
  scriptname=NULL; // no name yet
  inComment=false; // not in comment yet
  if (p == ep)
    // empty script
    return;

  // try to get start line
  if (ep>bp && *p!=TK_SOURCELINE)
    SYSYNC_THROW(TScriptErrorException(DEBUGTEXT("Script does not start with TK_SOURCELINE","scri2"),line));
  do {
    // get script name or first source code line
    line = (((uInt8)*(p+2))<<8) + (uInt8)(*(p+3));
    nextline=line; // assume same
    #ifdef SYDEBUG
    sInt16 n=(uInt8)*(p+1)-2;
    if (n>0) {
      linesource=(const char *)(p+4); // source for this line starts here
      if (line==0) {
        // this is the script name
        scriptname=linesource; // remember
        linesource=NULL;
      }
    }
    p+=2+2+n;
    #else
    p+=2+2; // script starts here
    #endif
    if (line==0) {
      // we have the name
      continue; // get first line now
    }
    break;
  } while(true);
} // TScriptContext::initParse


#ifdef SYDEBUG

// colorize source line (comments only so far)
static void colorizeSourceLine(cAppCharP aSource, string &aColorSource, bool &aComment, bool skipping)
{
  appChar c;
  bool start=true;
  bool incomment=aComment;
  bool lineendcomment=false;

  aColorSource.erase();
  while ((c=*aSource++)) {
    // check for comment start
    if (!aComment && !skipping) {
      // not in comment
      if (c=='/') {
        if (*aSource=='*')
          { aComment=true; }
        else if (*aSource=='/')
          { aComment=true; lineendcomment=true; }
      }
    }
    // switch to new color if changes at this point
    if (start || incomment!=aComment) {
      aColorSource += "&html;";
      if (!start)
        aColorSource += "</span>"; // not start
      aColorSource += "<span class=\"";
      aColorSource += skipping ? "skipped" : (aComment ? "comment" : "source");
      aColorSource += "\">&html;";
      incomment = aComment;
    }
    // output current char
    if (c==' ')
      aColorSource += "&sp;"; // will convert to &nbsp; in HTML
    else
      aColorSource += c;
    // check for end of comment after this char
    if (aComment && !lineendcomment && !skipping) {
      // in C-style comment
      if (c=='/' && !start && *(aSource-2)=='*') {
        // this was end of C-comment
        aComment=false;
      }
    }
    // not start any more
    start=false;
  }
  // end of color
  aColorSource += "&html;</span>&html;";
  // end of line terminates //-style comment
  if (lineendcomment) aComment=false;
} // colorizeSourceLine

#endif


// get token at p if not end of script, updates np to point to next token
uInt8 TScriptContext::gettoken(void)
{
  uInt8 tk;
  DBGSTRINGDEF(s);

  if (np) p=np; // advance to next token
  line=nextline; // advance to next line
  if (p>=ep) return 0; // end of script
  do {
    // get token and search next
    tk=*p; // get token
    if (tk>TK_MAX_MULTIBYTE) np=p+1; // single byte token, next is next char
    else np=p+2+*(p+1); // use length to find next token
    // show current line with source if we have it
    #ifdef SYDEBUG
    // delay display of line when processing a end block statement to make sure
    // end-skipping decision is made BEFORE showing the line
    if (SCRIPTDBGTEST &&
        linesource &&
        executing &&
        tk!=TK_END_STATEMENT &&
        tk!=TK_END_BLOCK) {
      bool sk = skipping>0;
      colorizeSourceLine(linesource,s,inComment,sk);
      SCRIPTDBGMSG(("Line %4hd: %s",line,s.c_str()));
      linesource=NULL; // prevent showing same line twice
    }
    #endif
    // check if it is line token, which would be processed invisibly
    if (tk==TK_SOURCELINE) {
      // get source code line number
      line = (((uInt8)*(p+2))<<8) + (uInt8)(*(p+3));
      // get source code itself, if present
      #ifdef SYDEBUG
      sInt16 n=(uInt8)*(p+1)-2;
      if (n>0) linesource=(const char *)(p+4); // source for this line starts here
      #endif
      // go to next token
      p=np;
      continue; // fetch next
    }
    nextline=line; // by default, assume next token is on same line
    do {
      // check if next is line token, if yes, skip it as well
      if (*np==TK_SOURCELINE) {
        // get next token's source code line
        #ifdef SYDEBUG
        uInt16 showline = nextline; // current line is next that must be shown
        #endif
        nextline = (((uInt8)*(np+2))<<8) + (uInt8)(*(np+3));
        #ifdef SYDEBUG
        // - show last line if we'll skip another line
        if (linesource && executing) {
          colorizeSourceLine(linesource,s,inComment,skipping>0);
          SCRIPTDBGMSG(("Line %4hd: %s",showline,s.c_str()));
          linesource=NULL; // prevent showing same line twice
        }
        // - get next line's number and text
        sInt16 n=(uInt8)*(np+1)-2;
        if (n>0) linesource=(const char *)(np+4); // source for this line starts here
        np=np+2+2+n;
        #else
        np=np+2+2; // skip token
        #endif
        continue; // test again
      }
      break;
    } while(true);
    break;
  } while(true);
  return tk;
} // TScriptContext::gettoken


// re-use last token fetched with gettoken()
void TScriptContext::reusetoken(void)
{
  // set pointer such that next gettoken will fetch the same token again
  np=p;
  nextline=line;
} // TScriptContext::reusetoken



// Resolve local variable declarations, references and field references
// Note: does not clear the context, so multiple scripts can
//       share the same context
// Note: modifies the aTScript passed (inserts identifier IDs)
void TScriptContext::ResolveIdentifiers(string &aTScript,TFieldListConfig *aFieldListConfigP, bool aRebuild, string *aFuncNameP, bool aNoNewLocals)
{
  uInt8 tk; // current token

  TItemFieldTypes ty=fty_none;
  bool deftype=false;
  bool refdecl=false;
  bool funcparams=false;
  string ident;
  sInt16 objidx;

  // init parsing
  initParse(aTScript);
  objidx=OBJ_AUTO;
  while ((tk=gettoken())) {
    // check declaration syntax
    if (deftype && tk!=TK_IDENTIFIER)
      SYSYNC_THROW(TScriptErrorException("Bad declaration",line));
    if (!funcparams && tk==TK_OPEN_PARANTHESIS) {
      // could be typecast
      if (*np==TK_TYPEDEF) {
        // is a typecase
        gettoken(); // swallow type identifier
        // next must be closing paranthesis
        if (gettoken()!=TK_CLOSE_PARANTHESIS)
          SYSYNC_THROW(TScriptErrorException("Invalid typecast",line));
      }
    }
    // process token
    else if (tk==TK_TYPEDEF) {
      // type definition (for variable declaration or function definition)
      ty = (TItemFieldTypes)(*(p+2));
      deftype=true; // we are in type definition mode now
      // check if we are declaring a variable reference
      if (*np==TK_BITWISEAND) {
        gettoken();
        refdecl=true; // defining a reference
        if (!funcparams)
          SYSYNC_THROW(TScriptErrorException("Field reference declaration allowed for function parameters",line));
      }
    }
    else if (tk==TK_OBJECT) {
      objidx=*(p+2);
      continue; // avoid resetting to OBJ_AUTO at end of loop
    }
    else if (tk==TK_CLOSE_PARANTHESIS) {
      if (funcparams) {
        // end of function parameters
        fNumParams=fVarDefs.size(); // locals declared up to here are parameters
        funcparams=false; // done with parameters
      }
    }
    else if (tk==TK_IDENTIFIER) {
      // get identifier
      ident.assign((cAppCharP)(p+3),(size_t)(*(p+1)-1));
      // check for function definition
      if (!funcparams && *np==TK_OPEN_PARANTHESIS) {
        // this is a function declaration
        if (!aRebuild) {
          // - check if allowed
          if (!aFuncNameP)
            SYSYNC_THROW(TScriptErrorException("cannot declare function here",line));
          // - save name of function
          aFuncNameP->assign(ident);
        }
        // - determine return type
        if (deftype)
          fFuncType=ty; // save function return type
        else
          fFuncType=fty_none; // void function
        // - switch to function param parsing
        gettoken(); // get opening paranthesis
        funcparams=true;
        deftype=false;
      }
      else if (deftype) {
        // defining new local variable or parameter
        if (objidx!=OBJ_AUTO && objidx!=OBJ_LOCAL)
          SYSYNC_THROW(TScriptErrorException("cannot declare non-local variable '%s'",line,ident.c_str()));
        bool arr=false;
        // define new identifier(s)
        cUInt8P idp=p; // remember identifier token pointer
        // - check if this is an array definition
        if (*np==TK_OPEN_ARRAY) {
          arr=true;
          gettoken();
          if (*np!=TK_CLOSE_ARRAY)
            SYSYNC_THROW(TScriptErrorException("Invalid array declaration for '%s'",line,ident.c_str()));
          #ifndef ARRAYFIELD_SUPPORT
          SYSYNC_THROW(TScriptErrorException("Arrays not available in this version",line));
          #endif
          gettoken(); // swallow closing bracket
        }
        // - check if variable already defined
        TScriptVarDef *vardefP=NULL;
        #if SYDEBUG>1
        // always get it by name and compare index
        vardefP = getVarDef(ident.c_str(),ident.size());
        #else
        if (!aRebuild || (sInt8)(*(idp+2))==VARIDX_UNDEFINED)
          // first time build or index not yet set for another reason: get by name
          vardefP = getVarDef(ident.c_str(),ident.size());
        else
          // on rebuild, get definition by already known index
          vardefP = getVarDef(-((sInt8)(*(idp+2))+1));
        #endif
        #ifndef RELEASE_VERSION
        DEBUGPRINTFX(DBG_SCRIPTS+DBG_EXOTIC,(
          "ident=%s, vardefP=0x%lX, vardefP->fIdx=%d, stored idx=%d (=vardef idx %d)",
          ident.c_str(),
          (long)vardefP,
          vardefP ? (int)vardefP->fIdx : VARIDX_UNDEFINED,
          (int)((sInt8)(*(idp+2))),
          (int)(-((sInt8)(*(idp+2))+1))
        ));
        #endif
        // check for match
        if (vardefP) {
          // check if type matching
          if (ty!=vardefP->fVarType || arr!=vardefP->fIsArray || refdecl!=vardefP->fIsRef)
            SYSYNC_THROW(TScriptErrorException("Redefined '%s' to different type",line,ident.c_str()));
        }
        else {
          // not existing yet
          if (aNoNewLocals) {
            // Note: aNoNewLocals is useful when resolving a script into a context
            //   with already existing variables (such as RuleScript). We cannot
            //   add new variables once the VarDefs have been instantiated!
            SYSYNC_THROW(TScriptErrorException("Cannot declare variables in this script",line));
          }
          else {
            // create new variable definition
            vardefP = new TScriptVarDef(ident.c_str(),fVarDefs.size(),ty,arr,refdecl,false);
            fVarDefs.push_back(vardefP);
            #ifndef RELEASE_VERSION
            DEBUGPRINTFX(DBG_SCRIPTS+DBG_EXOTIC,(
              "created new vardef, ident=%s, type=%s, vardefP=0x%lX, vardefs.size()=%ld",
              ident.c_str(),
              ItemFieldTypeNames[ty],
              (long)vardefP,
              (long)fVarDefs.size()
            ));
            #endif
          }
        }
        if (!aRebuild) {
          // creating defs for the first time, set index
          aTScript[idp-bp+2]= -(sInt8)(vardefP->fIdx)-1;
        }
        #if SYDEBUG>1
        else {
          // check existing index against new one
          if ((sInt8)(*(idp+2))!=-(sInt8)(vardefP->fIdx)-1)
            SYSYNC_THROW(TScriptErrorException(DEBUGTEXT("Rebuilding defs gives different index","scri1"),line));
        }
        #endif
        // - check what follows
        deftype=false; // default to nothing
        refdecl=false;
        if (funcparams) {
          if (*np==TK_LIST_SEPARATOR) gettoken(); // ok, next param
        }
        else {
          if (*np==TK_LIST_SEPARATOR) {
            deftype=true; // continue with more definitions of same type
            gettoken(); // consume separator
          }
          else if (*np!=TK_END_STATEMENT)
            SYSYNC_THROW(TScriptErrorException("Missing ';' after declaration of '%s'",line,ident.c_str()));
        }
      }
      else if (!aRebuild) {
        // refer to identifier
        sInt16 i=getIdentifierIndex(objidx,aFieldListConfigP,ident.c_str(),ident.size());
        if (i==VARIDX_UNDEFINED)
          SYSYNC_THROW(TScriptErrorException("Undefined identifier '%s'",line,ident.c_str()));
        // save field/var index in script
        aTScript[p-bp+2]= (sInt8)i;
      }
    }
    else {
      // Other non-declaration tokens
      // - if this is function param def, no other tokens are allowed
      if (funcparams) {
        SYSYNC_THROW(TScriptErrorException("Invalid function parameter declaration",line));
      }
      else {
        // non-declaration stuff
        if (tk==TK_USERFUNCTION && !aRebuild) {
          // resolve function
          ident.assign((cAppCharP)(p+3),(size_t)(*(p+1)-1));
          sInt16 i=getSyncAppBase()->getRootConfig()->fScriptConfigP->getFunctionIndex(ident.c_str(),ident.size());
          if (i==VARIDX_UNDEFINED)
            SYSYNC_THROW(TScriptErrorException("Undefined function '%s'",line,ident.c_str()));
          // save function index in script
          aTScript[p-bp+2]= (sInt8)i;
        }
      }
    }
    objidx=OBJ_AUTO; // default to auto-select
    // process next
  }
  SHOWVARDEFS(aRebuild ? "Rebuilding" : "Resolving");
} // TScriptContext::ResolveIdentifiers


// create local variable definitions for calling a built-in function
// This is called for built-in functions before calling PrepareLocals, and
// builds up local var definitions like ResolveIdentifiers does for
// script code.
void TScriptContext::defineBuiltInVars(const TBuiltInFuncDef *aFuncDefP)
{
  // get information and save it in the function's context
  fNumParams=aFuncDefP->fNumParams;
  fFuncType=aFuncDefP->fReturntype;
  // get params
  sInt16 i=0;
  while (i<fNumParams) {
    // create parameter definition
    uInt8 paramdef=aFuncDefP->fParamTypes[i];
    TItemFieldTypes ty= (TItemFieldTypes)(paramdef & PARAM_TYPEMASK);
    bool isref = paramdef & PARAM_REF;
    bool isarr = paramdef & PARAM_ARR;
    bool isopt = paramdef & PARAM_OPT;
    TScriptVarDef *vardefP = new TScriptVarDef("", fVarDefs.size(), ty, isarr, isref, isopt);
    fVarDefs.push_back(vardefP);
    i++;
  }
} // TScriptContext::defineBuiltInVars


// execute built-in function
void TScriptContext::executeBuiltIn(TItemField *&aTermP, const TBuiltInFuncDef *aFuncDefP)
{
  TItemField *resultP=NULL;

  // pre-create result field if type is known
  // Note: multi-typed functions might create result themselves depending on result type
  if (fFuncType!=fty_none)
    resultP = newItemField(fFuncType, getSessionZones());
  // call actual function routine
  (aFuncDefP->fFuncProc)(resultP,this);
  // return result
  if (aTermP) {
    // copy value to exiting result field
    if (resultP) {
      (*aTermP)=(*resultP);
      delete resultP;
    } else
      aTermP=NULL; // no result
  }
  else {
    // just pass back result field (or NULL)
    aTermP=resultP;
  }
} // TScriptContext::executeBuiltIn


#if SYDEBUG>1
void TScriptContext::showVarDefs(cAppCharP aTxt)
{
  if (DEBUGTEST(DBG_SCRIPTS+DBG_EXOTIC)) {
    // Show var defs
    DEBUGPRINTFX(DBG_SCRIPTS+DBG_EXOTIC+DBG_HOT,("%s - %s, ctx=0x%lX, VarDefs:",aTxt,scriptname ? scriptname : "<name unknown>",(long)this));
    TVarDefs::iterator pos;
    for (pos=fVarDefs.begin(); pos!=fVarDefs.end(); pos++) {
      DEBUGPRINTFX(DBG_SCRIPTS+DBG_EXOTIC,(
        "%d: %s %s%s%s",
        (*pos)->fIdx,
        ItemFieldTypeNames[(*pos)->fVarType],
        (*pos)->fIsRef ? "&" : "",
        (*pos)->fVarName.c_str(),
        (*pos)->fIsArray ? "[]" : ""
      ));
    }
  }
} // TScriptContext::showVarDefs
#endif



// Prepare local variables (create local fields), according to definitions (re-)built with ResolveIdentifiers()
// Note: This is called after ResolveIdentifiers() for each script of that context
//       have been called. Normally ResolveIdentifiers() was called already at config parse
//       with an anonymous context that is lost when PrepareLocals needs to be called.
//       Therefore, ResolveIdentifiers(aRebuild=true) can be called to rebuild the definitions from
//       the script(s) involved. Care must be taken to call ResolveIdentifiers() for each script
//       in the same order as at config parse to make sure already resolved indexes will match the variables (again).
// Note: Field references will have a NULL pointer (which must be filled when the function is
//       called.
bool TScriptContext::PrepareLocals(void)
{
  // make sure old array is cleared
  SHOWVARDEFS("PrepareLocals");
  clearFields();
  // create array of appropriate size for locals
  fNumVars=fVarDefs.size();
  SYSYNC_TRY {
    if (fNumVars) {
      fFieldsP=new TItemFieldP[fNumVars];
      if (fFieldsP==NULL) return false;
      // - init it with null pointers to make sure we can survive when field instantiation fails
      for (sInt16 i=0; i<fNumVars; i++) fFieldsP[i]=NULL;
      // - now instantiate local fields (but leave references = NULL)
      TVarDefs::iterator pos;
      for (pos=fVarDefs.begin(); pos!=fVarDefs.end(); pos++) {
        if (!(*pos)->fIsRef && ((*pos)->fVarType!=fty_none)) {
          // this is not a reference and not an untyped parameter:
          // instantiate a locally owned field
          fFieldsP[(*pos)->fIdx]=sysync::newItemField((*pos)->fVarType,getSessionZones(),(*pos)->fIsArray);
        }
      }
    }
  }
  SYSYNC_CATCH (...)
    // failed, remove fields again
    clearFields();
    return false;
  SYSYNC_ENDCATCH
  return true;
} // TScriptContext::PrepareLocals


// execute a script returning a boolean
bool TScriptContext::executeTest(
  bool aDefaultAnswer, // result if no script is there or script returns no value
  TScriptContext *aCtxP,
  const string &aTScript,
  const TFuncTable *aFuncTableP, // context's function table, NULL if none
  void *aCallerContext, // free pointer possibly having a meaning for context functions and chain function
  TMultiFieldItem *aTargetItemP, // target (or "loosing") item
  bool aTargetWritable, // if set, target item may be modified
  TMultiFieldItem *aReferenceItemP, // reference for source (or "old" or "winning") item
  bool aRefWritable // if set, reference item may also be written
)
{
  // if no script (no context), return default answer
  if (!aCtxP) return aDefaultAnswer;
  // now call and evaluate boolean result
  TItemField *resP;
  bool res;
  res=aCtxP->ExecuteScript(
    aTScript,
    &resP, // we want a result
    false, // not a function
    aFuncTableP,  // context function table
    aCallerContext, // context data
    aTargetItemP, // target (or "loosing") item
    aTargetWritable, // if set, target item may be modified
    aReferenceItemP, // reference for source (or "old" or "winning") item
    aRefWritable // if set, reference item may also be written
  );
  if (!res) {
    // script execution failed, do as if there was no script
    res=aDefaultAnswer;
  }
  else {
    // evaluate result
    if (resP) {
      res=resP->getAsBoolean();
      // get rid of result
      delete resP;
    }
    else {
      // no result, return default
      res=aDefaultAnswer;
    }
  }
  // return result
  return res;
} // TScriptContext::executeTest


// execute a script returning a result if there is a context for it
bool TScriptContext::executeWithResult(
  TItemField *&aResultField, // can be default result or NULL, will contain result or NULL if no result
  TScriptContext *aCtxP,
  const string &aTScript,
  const TFuncTable *aFuncTableP, // context's function table, NULL if none
  void *aCallerContext, // free pointer possibly having a meaning for context functions
  TMultiFieldItem *aTargetItemP, // target (or "loosing") item
  bool aTargetWritable, // if set, target item may be modified
  TMultiFieldItem *aReferenceItemP, // reference for source (or "old" or "winning") item
  bool aRefWritable, // if set, reference item may also be written
  bool aNoDebug // if set, debug output is suppressed even if DBG_SCRIPTS is generally on
)
{
  // if no script (no context), return default answer
  if (!aCtxP) return true; // ok, leave aResultFieldP unmodified
  // now call and evaluate result
  return aCtxP->ExecuteScript(
    aTScript,
    &aResultField, // we want a result, if we pass a default it will be modified if script has a result
    false, // not a function
    aFuncTableP,  // context function table
    aCallerContext, // context data
    aTargetItemP, // target (or "loosing") item
    aTargetWritable, // if set, target item may be modified
    aReferenceItemP, // reference for source (or "old" or "winning") item
    aRefWritable, // if set, reference item may also be written
    aNoDebug // if set, debug output is suppressed even if DBG_SCRIPTS is generally on
  );
} // TScriptContext::executeTest




// execute a script if there is a context for it
bool TScriptContext::execute(
  TScriptContext *aCtxP,
  const string &aTScript,
  const TFuncTable *aFuncTableP, // context's function table, NULL if none
  void *aCallerContext, // free pointer possibly having a meaning for context functions
  TMultiFieldItem *aTargetItemP, // target (or "loosing") item
  bool aTargetWritable, // if set, target item may be modified
  TMultiFieldItem *aReferenceItemP, // reference for source (or "old" or "winning") item
  bool aRefWritable, // if set, reference item may also be written
  bool aNoDebug // if set, debug output is suppressed even if DBG_SCRIPTS is generally on
)
{
  if (!aCtxP) return true; // no script, success
  // execute script in given context
  SYSYNC_TRY {
    bool r=aCtxP->ExecuteScript(
      aTScript,
      NULL, // we don't need a result
      false, // not a function
      aFuncTableP, // context specific function table
      aCallerContext, // caller's context private data pointer
      aTargetItemP, // target (or "loosing") item
      aTargetWritable, // if set, target item may be modified
      aReferenceItemP, // reference for source (or "old" or "winning") item
      aRefWritable, // if set, reference item may also be written
      aNoDebug // if set, debug output is suppressed even if DBG_SCRIPTS is generally on
    );
    return r;
  }
  SYSYNC_CATCH (...)
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
} // TScriptContext::execute


void TScriptContext::pushState(TScriptState aNewState, cUInt8P aBegin, uInt16 aLine)
{
  if (fStackEntries>=maxstackentries)
    SYSYNC_THROW(TScriptErrorException("too many IF/ELSE/WHILE/LOOP nested",line));
  // use defaults
  if (aBegin==NULL) {
    aBegin = np; // next token after current token
    aLine = nextline;
  }
  // push current state
  fScriptstack[fStackEntries].state=aNewState; // new state
  fScriptstack[fStackEntries].begin=aBegin; // start of block
  fScriptstack[fStackEntries].line=aLine; // line where block starts
  fStackEntries++;
} // TScriptContext::pushState


// pop state from flow control state stack
void TScriptContext::popState(TScriptState aCurrentStateExpected)
{
  if (fScriptstack[fStackEntries-1].state!=aCurrentStateExpected)
    SYSYNC_THROW(TScriptErrorException("bad block/IF/ELSE/WHILE/LOOP nesting",line));
  // remove entry
  fStackEntries--; // remove stack entry
  if (fStackEntries<1)
    SYSYNC_THROW(TScriptErrorException(DEBUGTEXT("unbalanced control flow stack pop","scri3"),line));
} // TScriptContext::popState


// get variable specification, returns true if writable
bool TScriptContext::getVarField(TItemField *&aItemFieldP)
{
  sInt16 objnum, varidx, arridx;
  TMultiFieldItem *itemP=NULL;
  bool writeable=false;
  bool fidoffs=false;
  uInt8 tk;

  aItemFieldP=NULL; // none by default
  tk=gettoken();
  objnum=OBJ_AUTO; // default to automatic object selection
  // check for object specifier
  if (tk==TK_OBJECT) {
    // object qualifier
    objnum=*(p+2);
    tk=gettoken();
    // must be followed by identifier
    if (tk!=TK_IDENTIFIER)
      SYSYNC_THROW(TScriptErrorException("bad variable/field reference",line));
  }
  // check for object itself
  if (tk==TK_IDENTIFIER) {
    #ifdef SYDEBUG
    cAppCharP idnam=(cAppCharP)(p+3);
    int idlen=*(p+1)-1;
    #endif
    // get index
    varidx=(sInt8)(*(p+2));
    if (varidx==VARIDX_UNDEFINED)
      SYSYNC_THROW(TScriptErrorException("Undefined identifier",line));
    // check possible array index
    arridx=-1; // default to non-array
    if (*np==TK_OPEN_ARRAY) {
      tk=gettoken(); // consume open bracket
      // check special field-offset access mode
      if (*np==TK_PLUS) {
        fidoffs=true;
        gettoken(); // consume the plus
      }
      TItemField *fldP = new TIntegerField;
      SYSYNC_TRY {
        evalExpression(fldP,EXPRDBGTEST,NULL); // do not show array index expression evaluation
        arridx=fldP->getAsInteger();
        delete fldP;
      }
      SYSYNC_CATCH (...)
        delete fldP;
        SYSYNC_RETHROW;
      SYSYNC_ENDCATCH
      // check closing array bracket
      tk=gettoken();
      if (tk!=TK_CLOSE_ARRAY)
        SYSYNC_THROW(TScriptErrorException("missing ']' for array reference",line));
    }
    // now access field
    #ifdef SYDEBUG
    sInt16 oai=arridx;
    #endif
    // - determine object if not explicitly specified
    if (objnum==OBJ_AUTO) {
      if (varidx<0) objnum=OBJ_LOCAL;
      else objnum=OBJ_TARGET;
    }
    // - now access (arridx==-1 if we don't want to access leaf element)
    if (objnum==OBJ_LOCAL) {
      if (fidoffs) { varidx-=arridx; arridx=-1; } // use array index as offset within local fields (negative indices!)
      aItemFieldP=getFieldOrVar(NULL,varidx,arridx);
      writeable=true; // locals are always writeable
    }
    else {
      // prepare index/arrayindex to access
      if (fidoffs) { varidx+=arridx; arridx=-1; } // use array index as offset within field list
      // get item to access
      if (objnum==OBJ_TARGET) { itemP=fTargetItemP; writeable=fTargetWritable; }
      else if (objnum==OBJ_REFERENCE) { itemP=fReferenceItemP; writeable=fRefWritable; }
      if (itemP)
        aItemFieldP=getFieldOrVar(itemP,varidx,arridx);
      else
        SYSYNC_THROW(TScriptErrorException("field not accessible in this context",line));
    }
    // error if no field
    DBGSTRINGDEF(s);
    DBGSTRINGDEF(sa);
    DBGVALUESHOW(s,aItemFieldP);
    if (EXPRDBGTEST) {
      if (oai>=0) {
        if (arridx!=-1)
          StringObjPrintf(sa,"[%hd]",oai);
        else
          StringObjPrintf(sa,"[+%hd]",oai);
      }
      else {
        // no array access
        sa.erase();
      }
      SCRIPTDBGMSG((
        "- %s: %.*s%s = %s",
        objnum==OBJ_LOCAL ? "Local Variable" :
          (objnum==OBJ_REFERENCE ? "OLD/LOOSING Field " : "Field"),
        idlen,idnam,
        sa.c_str(),
        s.c_str()
      ));
    }
    if (!aItemFieldP)
      SYSYNC_THROW(TScriptErrorException("undefined identifier, bad array index or offset",line));
    // return field pointer we've found, if any
    return writeable;
  }
  else
    SYSYNC_THROW(TScriptErrorException("expected identifier",line));
} // TScriptContext::getVarField



// evaluate function parameters
void TScriptContext::evalParams(TScriptContext *aFuncContextP)
{
  uInt8 tk;
  sInt16 paramidx;
  TScriptVarDef *vardefP;
  TItemField *fldP;

  tk=gettoken();
  if (tk!=TK_OPEN_PARANTHESIS)
    SYSYNC_THROW(TScriptErrorException("missing '(' of function parameter list",line));
  paramidx=0;
  // special check for function with only optional params
  if (aFuncContextP->getNumParams()>0 && aFuncContextP->getVarDef((short)0)->fIsOpt) {
    if (*np==TK_CLOSE_PARANTHESIS) goto noparams; // first is optional already -> ok to see closing paranthesis here
  }
  // at least one param
  while(paramidx<aFuncContextP->getNumParams()) {
    vardefP=aFuncContextP->getVarDef(paramidx);
    // check what param is expected next
    if (vardefP->fIsRef) {
      // by reference, we must have a writable lvalue here
      fldP=NULL;
      if (*np==TK_IDENTIFIER || *np==TK_OBJECT) {
        // get writable field/var of caller
        if (!getVarField(fldP))
          SYSYNC_THROW(TScriptErrorException("expected writable by-reference parameter",line));
        // type must match (or destination must be untyped)
        if (vardefP->fVarType!=fldP->getType() && vardefP->fVarType!=fty_none)
          SYSYNC_THROW(TScriptErrorException("expected by-reference parameter of type '%s'",line,ItemFieldTypeNames[vardefP->fVarType]));
        // type must match (or destination must be untyped)
        if (vardefP->fIsArray && !(fldP->isArray()))
          SYSYNC_THROW(TScriptErrorException("expected array as by-reference parameter",line));
        // assign field to function context's local var list
        aFuncContextP->setLocalVar(paramidx,fldP);
      }
      else
        SYSYNC_THROW(TScriptErrorException("expected by-reference parameter",line));
    }
    else {
      // by value
      // - get field where to assign value
      if (vardefP->fVarType!=fty_none) {
        // already prepared typed parameter
        fldP=aFuncContextP->getLocalVar(paramidx);
        // - evaluate expression into local var
        evalExpression(fldP,EXPRDBGTEST); // do not show expression, as we will show param
      }
      else {
        // untyped param, let evaluation create correct type
        fldP=NULL;
        evalExpression(fldP,EXPRDBGTEST); // do not show expression, as we will show param
        // untyped params are not yet instantiated, assign expression result now
        aFuncContextP->setLocalVar(paramidx,fldP);
      }
    }
    DBGSTRINGDEF(s);
    DBGVALUESHOW(s,fldP);
    SCRIPTDBGMSG((
      "- Parameter #%d (by %s) = %s",
      paramidx+1,
      vardefP->fIsRef ? "reference" : "value",
      s.c_str()
    ));
    // got param
    paramidx++;
    // check parameter delimiters
    if (paramidx<aFuncContextP->getNumParams()) {
      // function potentially has more params
      tk=gettoken();
      if (tk!=TK_LIST_SEPARATOR) {
        // - check if next is an optional parameter
        if (aFuncContextP->getVarDef(paramidx)->fIsOpt)
          goto endofparams; // yes, optional -> check for end of parameter list
        else
          SYSYNC_THROW(TScriptErrorException("expected ',' (more parameters)",line)); // non-optional parameter missing
      }
    }
  }
  // check end of parameter list
noparams:
  tk=gettoken();
endofparams:
  if (tk!=TK_CLOSE_PARANTHESIS)
    SYSYNC_THROW(TScriptErrorException("missing ')' of function parameter list",line));
} // TScriptContext::evalParams



// evaluate term, return caller-owned field containing term data
TItemField *TScriptContext::evalTerm(TItemFieldTypes aResultType)
{
  TItemFieldTypes termtype=aResultType; // default to result type
  TItemField *termP=NULL;
  uInt8 tk;
  TScriptContext *funccontextP;
  string *funcscript;
  const char *funcname;
  uInt16 funcnamelen;

  // Evaluate term. A term is
  // - a subexpression in paranthesis
  // - a variable reference
  // - a builtin function call
  // - a user defined function call
  // - a literal, including the special EMPTY and UNASSIGNED ones
  // A term can be preceeded by a typecast
  do {
    tk=gettoken();
    if (tk==TK_OPEN_PARANTHESIS) {
      // typecast or subexpression
      if (*np==TK_TYPEDEF) {
        // this is a typecast
        gettoken(); // actually get type
        termtype=(TItemFieldTypes)(*(p+2)); // set new term target type
        if (gettoken()!=TK_CLOSE_PARANTHESIS)
          SYSYNC_THROW(TScriptErrorException("missing ')' after typecast",line));
        if (aResultType!=fty_none && aResultType!=termtype)
          SYSYNC_THROW(TScriptErrorException("invalid typecast",line));
        continue; // now get term
      }
      else {
        // process subexpression into term
        if (termtype!=fty_none) {
          // prepare distinct result type field
          termP=newItemField(termtype, getSessionZones());
        }
        // puts result in a caller-owned termP (creates new one ONLY if caller passes NULL)
        evalExpression(termP,EXPRDBGTEST); // do not show subexpression results
        if (gettoken()!=TK_CLOSE_PARANTHESIS)
          SYSYNC_THROW(TScriptErrorException("missing ')' after expression",line));
      }
    }
    else if (tk==TK_FUNCTION || tk==TK_CONTEXTFUNCTION) {
      // get function definition table reference
      const TFuncTable *functableP =
        (tk==TK_FUNCTION) ?
        &BuiltInFuncTable : // globally available functions
        fFuncTableP; // context specific functions
      // get the function index
      sInt16 funcid=*(p+2);
      // now find the function definition record in the chain of function tables
      void *callerContext = fCallerContext; // start with caller context of current script
      while(functableP && funcid>=functableP->numFuncs) {
        // function is in a chained table
        // - adjust id to get offset based on next table
        funcid-=functableP->numFuncs;
        // - get next table and next caller context
        if (functableP->chainFunc)
          functableP = (TFuncTable *)functableP->chainFunc(callerContext);
        else
          functableP = NULL;
      }
      if (!functableP)
        SYSYNC_THROW(TScriptErrorException("undefined context function",line));
      // found function table and caller context, now get funcdef
      const TBuiltInFuncDef *funcdefP = &(functableP->funcDefs[funcid]);
      // execute built-in function call
      funcname=funcdefP->fFuncName;
      funcnamelen=100; // enough
      SCRIPTDBGMSG(("- %s() built-in function call:",funcname));
      // - get context for built-in function
      funccontextP=new TScriptContext(fAppBaseP,fSessionP);
      SYSYNC_TRY {
        // define built-in parameters of function context
        funccontextP->defineBuiltInVars(funcdefP);
        // prepare local variables of function context
        funccontextP->PrepareLocals();
        // prepare parameters
        evalParams(funccontextP);
        // copy current line for reference (as builtins have no own line number
        funccontextP->line=line;
        // copy caller's context pointer (possibly modified by function table chaining)
        funccontextP->fCallerContext=callerContext;
        funccontextP->fParentContextP=this; // link to calling script context
        // copy target and reference item vars
        funccontextP->fTargetItemP = fTargetItemP;
        funccontextP->fTargetWritable = fTargetWritable;
        funccontextP->fReferenceItemP = fReferenceItemP;
        funccontextP->fRefWritable = fRefWritable;
        // execute function
        funccontextP->executeBuiltIn(termP,funcdefP);
        // show by-ref parameters after call
        if (SCRIPTDBGTEST) {
          // show by-ref variables
          for (uInt16 i=0; i<funcdefP->fNumParams; i++) {
            if (funcdefP->fParamTypes[i] & PARAM_REF) {
              // is a parameter passed by reference, possibly changed by function call
              DBGSTRINGDEF(s);
              DBGVALUESHOW(s,funccontextP->getLocalVar(i));
              SCRIPTDBGMSG((
                "- return value of by-ref parameter #%d = %s",
                i+1, s.c_str()
              ));
            }
          }
        }
        // done
        delete funccontextP;
      }
      SYSYNC_CATCH (...)
        if (funccontextP) delete funccontextP;
        SYSYNC_RETHROW;
      SYSYNC_ENDCATCH
      goto funcresult;
    }
    else if (tk==TK_USERFUNCTION) {
      // user defined function call
      funcname=(const char *)p+3;
      funcnamelen=*(p+1)-1;
      SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC+DBG_SCRIPTEXPR,("- User-defined function %.*s call:",funcnamelen,funcname));
      // - get function text
      funcscript=getSyncAppBase()->getRootConfig()->fScriptConfigP->getFunctionScript(*(p+2));
      if (!funcscript)
        SYSYNC_THROW(TSyncException(DEBUGTEXT("invalid user function index","scri7")));
      // %%% possibly add caching of function contexts here.
      //     Now we rebuild a context for every function call. Not extremely efficient...
      funccontextP=NULL;
      rebuildContext(fAppBaseP,*funcscript,funccontextP,fSessionP,true);
      if (!funccontextP)
        SYSYNC_THROW(TSyncException(DEBUGTEXT("no context for user-defined function call","scri5")));
      SYSYNC_TRY {
        // prepare parameters
        evalParams(funccontextP);
        // pass parent context
        funccontextP->fParentContextP=this; // link to calling script context
        // process sub-script into term
        if (termtype!=fty_none)
          termP=newItemField(termtype, getSessionZones()); // we want a specific type, pre-define the field
        // execute function
        if (!funccontextP->ExecuteScript(
          *funcscript,
          &termP, // receives result, if any
          true, // is a function
          NULL, NULL, // no context functions/datapointer
          NULL, false, // no target fields
          NULL, false // no reference fields
        )) {
          SCRIPTDBGMSG(("- User-defined function failed to execute"));
          SYSYNC_THROW(TSyncException("User-defined function failed to execute properly"));
        }
        // done
        if (funccontextP) delete funccontextP;
      }
      SYSYNC_CATCH (...)
        if (funccontextP) delete funccontextP;
        SYSYNC_RETHROW;
      SYSYNC_ENDCATCH
    funcresult:
      DBGSTRINGDEF(s);
      DBGVALUESHOW(s,termP);
      SCRIPTDBGMSG((
        "- %.*s() function result = %s",
        funcnamelen,funcname,
        s.c_str()
      ));
    }
    else if (tk==TK_EMPTY) {
      termP=newItemField(fty_none, getSessionZones());
      termP->assignEmpty(); // make it EMPTY (that is, assigned)
      //SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC+DBG_SCRIPTEXPR,("- Literal: EMPTY"));
    }
    else if (tk==TK_UNASSIGNED) {
      termP=newItemField(fty_none, getSessionZones());
      termP->unAssign(); // make it (already is...) unassigned
      //SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC+DBG_SCRIPTEXPR,("- Literal: UNASSIGNED"));
    }
    else if (tk==TK_TRUE || tk==TK_FALSE) {
      termP=newItemField(fty_integer, getSessionZones());
      termP->setAsInteger(tk==TK_TRUE ? 1 : 0); // set 0 or 1
      //SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC+DBG_SCRIPTEXPR,("- Literal BOOLEAN: %d",tk==TK_TRUE ? 1 : 0));
    }
    else if (tk==TK_NUMERIC_LITERAL) {
      // %%% possibly add fty_float later
      // set type to integer if not another type requested
      //SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC+DBG_SCRIPTEXPR,("- Literal number: %0.*s",*(p+1),p+2));
      if (termtype==fty_none) termtype=fty_integer;
      goto literalterm;
    }
    else if (tk==TK_STRING_LITERAL) {
      if (termtype==fty_none) termtype=fty_string;
      //SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC+DBG_SCRIPTEXPR,("- Literal string: \"%0.*s\"",*(p+1),p+2));
    literalterm:
      termP = newItemField(termtype, getSessionZones());
      termP->setAsString((cAppCharP)(p+2),*(p+1));
    }
    else {
      // must be identifier
      reusetoken();
      TItemField *varP=NULL;
      getVarField(varP);
      // copy and convert
      if (termtype==fty_none) termtype=varP->getType();
      termP=newItemField(termtype, getSessionZones());
      (*termP)=(*varP); // simply assign (automatically converts if needed)
    }
    break;
  } while(true); // repeat only for typecast
  return termP; // return caller-owned field
} // TScriptContext::evalTerm


// evaluate expression, creates new or fills passed caller-owned field with result
void TScriptContext::evalExpression(
  TItemField *&aResultFieldP, // result (created new if passed NULL, modified and casted if passed a field)
  bool aShowResult, // if set, this is the main expression (and we want to see the result in DBG)
  TItemField *aLeftTermP, // if not NULL, chain-evaluate rest of expression according to aBinaryOp and aPreviousOp. WILL BE CONSUMED
  uInt8 *aBinaryOpP,  // operator to be applied between term passed in aLeftTermP and next term, will receive next operator that has same or lower precedence than aPreviousOp
  uInt8 aPreviousOp // if an operator of same or lower precedence than this is found, expression evaluation ends
)
{
  TItemFieldTypes termtype; // type of term
  TItemField *termP; // next term
  TItemField *resultP; // intermediate result
  string s; // temp string
  bool retainType=false;

  uInt8 unaryop,binaryop,nextbinaryop;

  fieldinteger_t a,b;

  // defaults
  binaryop=0; // none by default
  if (aBinaryOpP) binaryop=*aBinaryOpP; // get one if one was passed
  termtype=fty_none; // first term can be anything

  // init first term/operation if there is one
  if (binaryop && !aLeftTermP) binaryop=0; // security

  // process expression
  #ifdef SYDEBUG
  // - determine if subexpression
  if (!binaryop) {
    // starting new evaluation
    SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC+DBG_SCRIPTEXPR,("- Starting expression evaluation"));
  }
  #endif

  SYSYNC_TRY {
    // Note: if binaryop!=0, we have a valid aLeftTermP
    do {
      resultP=NULL;
      termP=NULL;
      // Get next term
      unaryop=0; // default to none
      // - check for unaries
      if (*np==TK_BITWISENOT || *np==TK_LOGICALNOT || *np==TK_MINUS) {
        unaryop=gettoken();
        // evaluate to integer, as unary ops all only make sense with integers
        termtype=fty_integer;
      }
      // - get next term
      termP=evalTerm(termtype);
      // Apply unaries now
      if (unaryop) {
        // all unaries are integer ops
        if (termP->getCalcType()!=fty_integer)
          SYSYNC_THROW(TScriptErrorException("unary operator applied to non-integer",line));
        fieldinteger_t ival = termP->getAsInteger();
        // apply op
        switch (unaryop) {
          case TK_MINUS:
            ival=-ival;
            break;
          case TK_BITWISENOT:
            ival= ~ival;
            break;
          case TK_LOGICALNOT:
            ival= !termP->getAsBoolean();
            break;
        }
        // store back into term field
        termP->setAsInteger(ival);
      }
      // now we have a new term in termP (or NULL if term had no result)
      // - check for a next operator
      nextbinaryop=0;
      if (*np>=TK_BINOP_MIN && *np <=TK_BINOP_MAX) {
        // this is a binary operator
        nextbinaryop=gettoken();
      }
      // - check for non-existing term
      if (!termP && (binaryop || nextbinaryop))
        SYSYNC_THROW(TScriptErrorException("non-value cannot be used in expression",line));
      // - check if there is a previous operation pending
      if (binaryop) {
        fieldinteger_t intres;
        // There is an operation to perform between aLeftTermP and current term (or expression with
        // higher precedence)
        // - get precedences (make them minus to have lowerprec<higherprec)
        sInt8 currentprec=-(binaryop & TK_OP_PRECEDENCE_MASK);
        sInt8 nextprec=-(nextbinaryop & TK_OP_PRECEDENCE_MASK);
        // - we must evaluate part of the expression BEFORE applying binaryop if next operator
        //   has higher precedence than current one
        if (nextbinaryop && nextprec>currentprec) {
          // next operator has higher precedence, evaluate everything up to next operator with same or
          // lower precedence as current one BEFORE applying binaryop
          evalExpression(
            resultP, // we'll receive the result here
            EXPRDBGTEST, // do not normally show subexpression results
            termP, // left term of new expression, WILL BE CONSUMED
            &nextbinaryop, // operator, will be updated to receive next operator to apply
            binaryop // evaluation stops when expression reaches operator with same or lower precedence
          );
          // nextbinaryop now has next operation to perform AFTER doing binaryop
          termP=resultP; // original termP has been consumed, use result as new termP
          resultP=NULL;
        }
        // Now we can apply binaryop between lasttermP and termP
        // Note: this must CONSUME termP AND aLeftTermP and CREATE resultP
        // - check for operators that work with multiple types
        if (binaryop==TK_PLUS) {
          if (aLeftTermP->getCalcType()!=fty_integer) {
            // treat plus as general append (defaults to string append for non-arrays)
            aLeftTermP->append(*termP);
            resultP=aLeftTermP; // we can simply pass the pointer
            aLeftTermP=NULL; // consume
            goto opdone; // operation done, get rid of termP
          }
        }
        else if (binaryop>=TK_LESSTHAN && binaryop<=TK_NOTEQUAL) {
          // comparison operators
          sInt16 cmpres=-3; // will never happen
          bool neg=false;
          switch (binaryop) {
            //   - comparison
            case TK_LESSTHAN      : cmpres=-1; break;
            case TK_GREATERTHAN   : cmpres=1; break;
            case TK_LESSEQUAL     : cmpres=1; neg=true; break;
            case TK_GREATEREQUAL  : cmpres=-1; neg=true; break;
            //   - equality
            case TK_EQUAL         : cmpres=0; break;
            case TK_NOTEQUAL      : cmpres=0; neg=true; break;
          }
          // do comparison
          if (termP->getType()==fty_none) {
            // EMPTY or UNASSIGNED comparison
            if (termP->isUnassigned()) {
              intres = aLeftTermP->isUnassigned() ? 0 : 1; // if left is assigned, it is greater
            }
            else {
              intres = aLeftTermP->isEmpty() ? 0 : 1; // if left is not empty, it is greater
            }
          }
          else {
            // normal comparison
            intres=aLeftTermP->compareWith(*termP);
          }
          if (intres==SYSYNC_NOT_COMPARABLE) intres=0; // false
          else {
            intres=cmpres==intres;
            if (neg) intres=!intres;
          }
          retainType= aLeftTermP->getType()==fty_integer;; // comparisons must always have plain integer result
          goto intresult;
        }
        // - integer operators
        a=aLeftTermP->getAsInteger();
        b=termP->getAsInteger();
        // for integer math operators, retain type of left term if it can calculate as integer
        // (such that expressions like: "timestamp + integer" will have a result type of timestamp)
        retainType = aLeftTermP->getCalcType()==fty_integer;
        // now perform integer operation
        switch (binaryop) {
          // - multiply, divide
          case TK_MULTIPLY      : intres=a*b; break;
          case TK_DIVIDE        : intres=a/b; break;
          case TK_MODULUS       : intres=a%b; break;
          //   - add, subtract
          case TK_PLUS          : intres=a+b; break;
          case TK_MINUS         : intres=a-b; break;
          //   - shift
          case TK_SHIFTLEFT     : intres=a<<b; break;
          case TK_SHIFTRIGHT    : intres=a>>b; break;
          //   - bitwise AND
          case TK_BITWISEAND    : intres=a&b; break;
          //   - bitwise XOR
          case TK_BITWISEXOR    : intres=a^b; break;
          //   - bitwise OR
          case TK_BITWISEOR     : intres=a|b; break;
          //   - logical AND
          case TK_LOGICALAND    : intres=a&&b; break;
          //   - logical OR
          case TK_LOGICALOR     : intres=a||b; break;
          default:
            SYSYNC_THROW(TScriptErrorException("operator not implemented",line));
        }
      intresult:
        // save integer result (optimized, generate new field only if aLeftTermP is not already integer-calc-type)
        if (retainType)
          resultP=aLeftTermP; // retain original left-side type (including extra information like time zone context and rendering type)
        else {
          // create new integer field
          resultP = newItemField(fty_integer, getSessionZones());
          delete aLeftTermP;
        }
        aLeftTermP=NULL;
        resultP->setAsInteger(intres);
      opdone:
        // check for special conditions when operating on timestamps
        if (resultP->isBasedOn(fty_timestamp) && termP->isBasedOn(fty_timestamp)) {
          // both based on timestamp.
          if (binaryop==TK_MINUS && !static_cast<TTimestampField *>(resultP)->isDuration() && !static_cast<TTimestampField *>(termP)->isDuration()) {
            // subtracted two points in time -> result is duration
            static_cast<TTimestampField *>(resultP)->makeDuration();
          }
        }
        // get rid of termP
        delete termP;
        termP=NULL;
      } // if binary op was pending
      else {
        // no binary op pending, simply pass term as result
        resultP=termP;
        termP=NULL;
      }
      // Now termP and aLeftTermP are consumed, resultP is alive
      // - determine next operation
      if (nextbinaryop) {
        // check precedence, if we can pass back to previous
        sInt8 lastprec=-(aPreviousOp & TK_OP_PRECEDENCE_MASK);
        sInt8 thisprec=-(nextbinaryop & TK_OP_PRECEDENCE_MASK);
        if (aPreviousOp && thisprec<=lastprec) break; // force exit, resultP is assigned
      }
      // result is going to be left term for next operation
      aLeftTermP=resultP;
      binaryop=nextbinaryop;
    } while(nextbinaryop); // as long as expression continues
    // show result
    #ifdef SYDEBUG
    // show final result of main expression only
    if (!nextbinaryop && aShowResult) {
      DBGVALUESHOW(s,resultP);
      SCRIPTDBGMSG((
        "- Expression result: %s",
        s.c_str()
      ));
    }
    #endif
    // convert result to desired type
    if (!aResultFieldP) {
      // no return field passed, just pass pointer (and ownership) of our resultP
      aResultFieldP=resultP;
    }
    else {
      // return field already exists, assign value (will convert type if needed)
      if (resultP) {
        (*aResultFieldP)=(*resultP);
        delete resultP; // not passed to caller, so we must get rid of it
      }
      else {
        // no result, unAssign
        aResultFieldP->unAssign();
      }
    }
    // return nextbinaryop if requested
    if (aBinaryOpP) *aBinaryOpP=nextbinaryop; // this is how we ended
  }
  SYSYNC_CATCH (...)
    // delete terms
    if (resultP) delete resultP;
    if (aLeftTermP) delete aLeftTermP;
    if (termP) delete termP;
    SYSYNC_RETHROW;
  SYSYNC_ENDCATCH
} // TScriptContext:evalExpression



// execute script
// returns false if script execution was not successful
bool TScriptContext::ExecuteScript(
  const string &aTScript,
  TItemField **aResultPP, // if not NULL, a result field will be returned here (must be deleted by caller)
  bool aAsFunction, // if set, this is a function call
  const TFuncTable *aFuncTableP, // context's function table, NULL if none
  void *aCallerContext, // free pointer possibly having a meaning for context functions
  TMultiFieldItem *aTargetItemP, // target (or "loosing") item
  bool aTargetWritable, // if set, target item may be modified
  TMultiFieldItem *aReferenceItemP, // reference for source (or "old" or "winning") item
  bool aRefWritable, // if set, reference item may also be written
  bool aNoDebug // if set, debug output is suppressed even if DBG_SCRIPTS is generally on
)
{
  if (aResultPP) *aResultPP=NULL; // no result yet
  TItemField *resultP = NULL; // none yet

  uInt8 tk; // current token
  TScriptState sta; // status

  skipping=0; // skipping level

  bool funchdr=aAsFunction; // flag if processing function header first
  if (funchdr) skipping=1; // skip stuff

  // endless loop protection
  lineartime_t loopmaxtime=0; // not in a loop

  // save context parameters
  fTargetItemP=aTargetItemP; // target (or "loosing") item
  fTargetWritable=aTargetWritable; // if set, target item may be modified
  fReferenceItemP=aReferenceItemP; // reference for source (or "old" or "winning") item
  fRefWritable=aRefWritable; // if set, reference item may also be written
  fFuncTableP=aFuncTableP; // context's function table, NULL if none
  fCallerContext=aCallerContext;

  #ifdef SYDEBUG
  debugon = !aNoDebug;
  #endif

  // init parsing (for execute)
  initParse(aTScript,true);
  // test if there's something to execute at all
  if (ep>bp) {
    #ifdef SYDEBUG
    if (aAsFunction) {
      SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_HOT,("* Starting execution of user-defined function"));
    } else {
      SCRIPTDBGSTART(scriptname ? scriptname : "<unnamed>");
    }
    #endif
    SYSYNC_TRY {
      // init state stack
      fStackEntries=0; // stack is empty
      pushState(ssta_statement); // start with statement

      // execute
      while ((tk=gettoken())) {
        resultP=NULL;
        // start of statement
        // - check for block
        if (tk==TK_BEGIN_BLOCK) {
          // push current state to stack and start new block
          pushState(ssta_block);
          if (funchdr) {
            // start of first block = start of function body, stop skipping
            funchdr=false;
            skipping=0;
          }
          continue; // process next
        }
        else if (tk==TK_END_BLOCK) {
          // pop block start, throw error if none there
          popState(ssta_block);
          // treat like end-of-statement, check for IF/ELSE/LOOP etc.
          goto endstatement;
        }
        else if (tk==TK_END_STATEMENT) {
          goto endstatement;
        }
        // - handle skipping of conditionally excluded blocks
        if (skipping!=0) {
          // only IF and ELSE are of any relevance
          if (tk==TK_IF) {
            pushState(ssta_if); // nested IF
            skipping++; // skip anyway
          }
          else if (tk==TK_ELSE) {
            // %%% this case probably never happens, as it is pre-fetched at end-of-statement
            // only push ELSE if this is not a chained ELSE IF
            if (*np!=TK_IF) pushState(ssta_else); // nested ELSE
          }
          // all others are irrelevant during skip
          continue;
        }
        else {
          // really executing
          // - check empty statement
          if (tk==TK_END_STATEMENT) goto endstatement;
          // - check IF statement
          else if (tk==TK_IF || tk==TK_WHILE) {
            // IF or WHILE conditional
            // - remember for WHILE case as condition must be re-evaluated for every loop
            cUInt8P condBeg = p;
            uInt16 condLine = line;
            // - process boolean expression
            tk=gettoken();
            if (tk!=TK_OPEN_PARANTHESIS)
              SYSYNC_THROW(TScriptErrorException("missing '(' after IF or WHILE",line));
            SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC+DBG_SCRIPTEXPR,("- IF or WHILE, evaluating condition..."));
            evalExpression(resultP,EXPRDBGTEST);
            tk=gettoken();
            if (tk!=TK_CLOSE_PARANTHESIS)
              SYSYNC_THROW(TScriptErrorException("missing ')' in IF or WHILE",line));
            // - determine which branch to process
            if (!(resultP && resultP->getAsBoolean()))
              skipping=1; // enter skip level 1
            SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC,("- %s condition is %s", *condBeg==TK_WHILE ? "WHILE" : "IF", skipping ? "false" : "true"));
            delete resultP;
            resultP=NULL;
            if (*condBeg==TK_WHILE) {
              if (skipping) {
                SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_HOT,("- WHILE condition is false -> skipping WHILE body"));
              }
              // for every subsequent loop, WHILE must be reprocessed
              pushState(ssta_while,condBeg,condLine);
              goto startloop; // limit loop execution time
            }
            else {
              // process statement (block) after IF
              pushState(ssta_if);
            }
            continue;
          }
          // Note: ELSE is checked at end of statement only
          // Check loop beginning
          else if (tk==TK_LOOP) {
            // initiate loop
            pushState(ssta_loop);
            SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC,("- LOOP entered"));
          startloop:
            // - remember entry time of outermost loop
            if (loopmaxtime==0) {
              uInt32 loopSecs = getSyncAppBase()->getRootConfig()->fScriptConfigP->fMaxLoopProcessingTime;
              if (loopSecs>0) {
                loopmaxtime=
                  getSystemNowAs(TCTX_UTC, getSessionZones())+
                  secondToLinearTimeFactor * loopSecs;
              }
              else {
                loopmaxtime=maxLinearTime; // virtually unlimited time
              }
            }
            continue;
          }
          else if (tk==TK_CONTINUE || tk==TK_BREAK) {
            // see if there is a LOOP or WHILE on the stack
            bool inloop=false;
            bool inwhile=false;
            sInt16 sp=fStackEntries;
            while (sp>0) {
              sta = fScriptstack[sp-1].state;
              if (sta==ssta_loop) { inloop=true; break; } // we are in a LOOP
              if (sta==ssta_while) { inwhile=true; break; } // we are in a WHILE
              sp--;
            }
            // no loop found, error
            if (!inloop && !inwhile)
              SYSYNC_THROW(TScriptErrorException("BREAK or CONTINUE without enclosing LOOP or WHILE",line));
            if (tk==TK_BREAK) {
              // make sure we are skipping everything (including any number of nested if/else
              // and blocks up to reaching next end-of-loop). Note that if a LOOP or WHILE is in the
              // skipped part, it will not cause troubles because it is not recognized as
              // such while skipping.
              skipping=maxstackentries;
            }
            else {
              // continue, remove stack entries down to LOOP or WHILE and jump to beginning of loop
              // - same as if we had reached the bottom of the loop, but pop
              //   open blocks, if's and else's first
              // - sta is ssta_loop or ssta_while to decide if we must pop the status or not
              goto loopcontinue;
            }
          }
          else if (tk==TK_TYPEDEF) {
            // declaration, skip it
            do {
              if (*np!=TK_IDENTIFIER)
                SYSYNC_THROW(TScriptErrorException("Invalid declaration",line));
              tk=gettoken(); // swallow identifier
              if (*np==TK_OPEN_ARRAY) {
                // must be array declaration
                tk=gettoken(); // swallow [
                if (*np==TK_CLOSE_ARRAY)
                  gettoken(); // swallow ]
              }
              if (*np!=TK_LIST_SEPARATOR) break;
              gettoken(); // swallow separator
            } while(true);
            // end of statement should follow here, will be checked below.
          }
          else if (tk==TK_IDENTIFIER || tk==TK_OBJECT) {
            SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC+DBG_SCRIPTEXPR,("- Starting assignment/unstored expression"));
            // could be assignment if identifier is followed by TK_ASSIGN
            // otherwise, it could be a simple expression evaluation
            cUInt8P ts=p; // remember start of statement
            uInt16 tl=line; // remember line as well
            TItemField *fldP;
            reusetoken();
            bool writeable=getVarField(fldP);
            // now check if this is an assignment
            if (*np==TK_ASSIGN) {
              gettoken(); // swallow TK_ASSIGN
              // must be writeable
              if (!writeable)
                SYSYNC_THROW(TScriptErrorException("Not allowed to assign to this field/variable",line));
              // evaluate expression into given field
              evalExpression(fldP,false); // we show the result ourselves
              DBGSTRINGDEF(s);
              DBGVALUESHOW(s,fldP);
              SCRIPTDBGMSG(("- Assigned expression result = %s",s.c_str()));
            }
            else {
              // must be plain expression
              np=ts; // back to start of statement;
              nextline=tl;
              evalExpression(resultP,true); // we always want to see the result
              SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC+DBG_SCRIPTEXPR,("- Evaluated unstored expression"));
              // - if nothing follows, script ends here with result of expression
              if (*np==0) break;
              // - otherwise, forget result
              delete resultP;
              resultP=NULL;
            }
          }
          else if (tk==TK_RETURN) {
            // simply return if no expression follows
            resultP=NULL;
            if (*np==TK_END_STATEMENT) {
              SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_HOT,("- RETURN: ending %s without result",aAsFunction ? "function" : "script"));
              break;
            }
            // evaluate expression first
            SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC+DBG_SCRIPTEXPR,("- RETURN: evaluating result expression..."));
            evalExpression(resultP,false); // we always show the result ourselves
            DBGSTRINGDEF(s);
            DBGVALUESHOW(s,resultP);
            SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_HOT,(
              "- RETURN: ending %s with result = %s",
              aAsFunction ? "function" : "script",
              s.c_str()
            ));
            // now end script
            break;
          }
          else {
            // everything else must be plain expression (e.g. function calls etc.)
            SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC+DBG_SCRIPTEXPR,("- Starting evaluating unstored expression"));
            reusetoken();
            evalExpression(resultP,false); // we always show the result ourselves
            SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC+DBG_SCRIPTEXPR,("- Evaluated unstored expression"));
            /* %%% wrong, prevents script from returning NOTHING!
                   to return a value, RETURN *must* be used!
            // - if nothing follows, script ends here with result of expression
            if (*np==0) {
              DBGSTRINGDEF(s);
              DBGVALUESHOW(s,resultP);
              SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_HOT,(
                "- Script ends with result = %s",
                s.c_str()
              ));
              break;
            }
            */
            // - otherwise, forget result
            delete resultP;
            resultP=NULL;
          }
        } // not skipping
        // Statement executed, next must be end-of-statement
        tk=gettoken();
        if (tk!=TK_END_STATEMENT)
          SYSYNC_THROW(TScriptErrorException("missing ';' after statement",line));
        // Statement executed and properly terminated
      endstatement:
        // check state
        sta=fScriptstack[fStackEntries-1].state;
        // check end of IF block
        if (sta==ssta_if) {
          SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC,("- End of %s IF",skipping ? "skipped" : "executed"));
          popState(sta); // end this state
          if (skipping) {
            // IF branch was not executed, check for else
            if (*np==TK_ELSE) {
              // else follows
              // - swallow TK_ELSE
              gettoken();
              // - check for ELSE IF chain
              //   (in this case, we must NOT push a state, but let chained IF push TK_IF again
              //   and also increment skipping again, after it is decremented by one below)
              if (*np!=TK_IF) {
                pushState(ssta_else); // end-of-chain ELSE: enter else state
                // keep skipping (compensate for decrement below) ONLY if end-of-chain ELSE
                // is not to be executed due to surrounding skip.
                if (skipping>1) skipping++;
              }
            }
            // no ELSE, just continue with next statement
            skipping--; // reduce skip level
            continue;
          }
          else {
            // IF branch was executed, check for else
            if (*np==TK_ELSE) {
              // else follows, skip it (including all chained IFs)
              skipping=1;
              gettoken(); // swallow ELSE
              // - check for ELSE IF chain
              if (*np==TK_IF) {
                // chained if while skipping
                gettoken(); // consume it (avoid regular parsing to see it and increment skipping)
                pushState(ssta_chainif); // chained IF, make sure nothing is executed up to and including end-of-chain else
              }
              else
                pushState(ssta_else); // process as skipped ELSE part
            }
            // no else, simply continue execution
            continue;
          }
        }
        else if (sta==ssta_chainif) {
          // only entered while skipping rest of chain
          SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC,("- End of skipped ELSE IF"));
          if (*np==TK_ELSE) {
            gettoken(); // consume it
            if (*np==TK_IF) {
              // chained if while skipping
              gettoken(); // consume it (avoid regular parsing to see it and increment skipping)
              // stay in ssta_chainif
            }
            else {
              popState(ssta_chainif); // end of ELSE IF chain
              pushState(ssta_else); // rest is a normal skipped ELSE part
            }
          }
          else {
            // end of skipped chained ifs
            popState(ssta_chainif);
            skipping--;
          }
        }
        else if (sta==ssta_else) {
          SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC,("- End of %s ELSE",skipping ? "skipped" : "executed"));
          popState(ssta_else); // end of else state
          if (skipping) skipping--;
        }
        // check end of LOOP block
        else if (sta==ssta_loop || sta==ssta_while) {
          // Note: we'll never see that for a completely skipped loop/while, as
          //       no ssta_loop/ssta_while is pushed while skipping.
          if (!skipping) goto loopcontinue; // not end of while or breaking out of loop, repeat
          // - end of loop reached
          popState(sta);
          skipping=0; // end of loop found after BREAK, continue normal execution
          SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_EXOTIC,("- End of WHILE or LOOP"));
        }
        // nothing special, just execute next statement
        continue;
      loopcontinue:
        // continue at beginning of loop which is at top of state stack
        if (getSystemNowAs(TCTX_UTC, getSessionZones())>loopmaxtime)
          SYSYNC_THROW(TScriptErrorException("loop execution aborted because <looptimeout> reached (endless loop?)",line));
        // go to beginning of loop: restore position of beginning of loop (including condition in case of WHILE)
        np=fScriptstack[fStackEntries-1].begin; // next token after current token
        nextline=fScriptstack[fStackEntries-1].line; // line of next token after current token
        linesource=NULL; // prevent showing source (which is that of NEXT line, not that of jump target's
        // for while, we must pop the stack entry as it will be recreated by re-excution of the WHILE statement
        if (sta==ssta_while)
          popState(sta);
        SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_HOT,("- Starting next iteration of LOOP/WHILE -> jumping to line %hd",nextline));
        continue;
      } // while more tokens
    } // try
    SYSYNC_CATCH (exception &e)
      // make sure field is deleted
      if (resultP) delete resultP;
      // show error message
      SCRIPTDBGMSGX(DBG_ERROR,("Warning: TERMINATING SCRIPT WITH ERROR: %s",e.what()));
      if (!aAsFunction) SCRIPTDBGEND();
      return false;
    SYSYNC_ENDCATCH
    #ifdef SYDEBUG
    if (aAsFunction) {
      SCRIPTDBGMSGX(DBG_SCRIPTS+DBG_HOT,("* Successfully finished execution of user-defined function"));
    } else {
      SCRIPTDBGEND();
    }
    #endif
  } // if something to execute at all
  // return most recent evaluated expression (normally RETURN expression)
  if (aResultPP) {
    // caller wants to see result, pass it back
    if (*aResultPP && resultP) {
      // script result field already exists, assign value
      (**aResultPP)=(*resultP);
      delete resultP;
    }
    else {
      // script result field does not yet exist, just pass result (even if it is NULL)
      *aResultPP=resultP;
    }
  }
  else if (resultP) delete resultP; // nobody needs the result, delete it
  // execution successful
  return true;
} // TScriptContext::ExecuteScript


// get variable definition by name, NULL if none defined yet
TScriptVarDef *TScriptContext::getVarDef(cAppCharP aVarName,size_t len)
{
  TVarDefs::iterator pos;
  for (pos=fVarDefs.begin(); pos!=fVarDefs.end(); pos++) {
    if (strucmp((*pos)->fVarName.c_str(),aVarName,0,len)==0) return (*pos);
  }
  return NULL;
} // TScriptContext::getVarDef


// get variable definition by index, NULL if none defined yet
TScriptVarDef *TScriptContext::getVarDef(sInt16 aLocalVarIdx)
{
  if (aLocalVarIdx<0 || uInt32(aLocalVarIdx)>=fVarDefs.size()) return NULL;
  return fVarDefs[aLocalVarIdx];
} // TScriptContext::getVarDef


// - get identifier index (>=0: field index, <0: local var)
//   returns VARIDX_UNDEFINED for unknown identifier
sInt16 TScriptContext::getIdentifierIndex(sInt16 aObjIndex, TFieldListConfig *aFieldListConfigP, cAppCharP aIdentifier,size_t aLen)
{
  // first look for local variable
  if (aObjIndex==OBJ_AUTO || aObjIndex==OBJ_LOCAL) {
    TScriptVarDef *vardefP = getVarDef(aIdentifier,aLen);
    if (vardefP) {
      return - (sInt16)vardefP->fIdx-1;
    }
  }
  if (aObjIndex==OBJ_AUTO || aObjIndex==OBJ_TARGET || aObjIndex==OBJ_REFERENCE) {
    // look for field with that name
    if (!aFieldListConfigP) return VARIDX_UNDEFINED; // no field list available here
    return aFieldListConfigP->fieldIndex(aIdentifier,aLen);
  }
  else return VARIDX_UNDEFINED; // unknown object, unknown index
} // TScriptContext::getIdentifierIndex


// get field by fid, can also be field of aItem, also resolves arrays
TItemField *TScriptContext::getFieldOrVar(TMultiFieldItem *aItemP, sInt16 aFid, sInt16 aArrIdx)
{
  TItemField *fldP = getFieldOrVar(aItemP,aFid);
  if (!fldP) return NULL;
  #ifdef ARRAYFIELD_SUPPORT
  if (aArrIdx>=0)
    return fldP->getArrayField(aArrIdx);
  else
    return fldP; // return field without array resolution
  #else
  if (aArrIdx>0) return NULL;
  return fldP;
  #endif
} // TScriptContext::getFieldOrVar


// get field by fid, can be field of aItem (positive aFid) or local variable (negative fid)
TItemField *TScriptContext::getFieldOrVar(TMultiFieldItem *aItemP, sInt16 aFid)
{
  if (aFid<0) return getLocalVar(-aFid-1);
  if (!aItemP) return NULL;
  return aItemP->getField(aFid);
} // TScriptContext::getFieldOrVar


// get local var by local index
TItemField *TScriptContext::getLocalVar(sInt16 aVarIdx)
{
  if (aVarIdx<0 || aVarIdx>=fNumVars) return NULL;
  return fFieldsP[aVarIdx];
} // TScriptContext::getLocalVar


// set local var by local index (used for passing references)
void TScriptContext::setLocalVar(sInt16 aVarIdx, TItemField *aFieldP)
{
  if (aVarIdx<0 || aVarIdx>=fNumVars) return;
  fFieldsP[aVarIdx]=aFieldP; // set new
} // TScriptContext::setLocalVar


/* end of TScriptContext implementation */


#ifdef ENGINEINTERFACE_SUPPORT

// TScriptVarKey
// =============


// get FID for specified name
sInt16 TScriptVarKey::getFidFor(cAppCharP aName, stringSize aNameSz)
{
  if (fScriptContext==NULL) return VARIDX_UNDEFINED;
  // check for iterator commands first
  if (strucmp(aName,VALNAME_FIRST)==0) {
    fIterator=0;
    if (fIterator>=sInt32(fScriptContext->fVarDefs.size())) return VARIDX_UNDEFINED;
    return -fIterator-1;
  }
  else if (strucmp(aName,VALNAME_NEXT)==0) {
    fIterator++;
    if (fIterator>=sInt32(fScriptContext->fVarDefs.size())) return VARIDX_UNDEFINED;
    return -fIterator-1;
  }
  else
    return fScriptContext->getIdentifierIndex(OBJ_LOCAL, NULL, aName, aNameSz);
} // TScriptVarKey::getFidFor



// get base field from FID
TItemField *TScriptVarKey::getBaseFieldFromFid(sInt16 aFid)
{
  if (fScriptContext==NULL) return NULL;
  return fScriptContext->getFieldOrVar(NULL, aFid);
} // TScriptVarKey::getBaseFieldFromFid


// get field name from FID
bool TScriptVarKey::getFieldNameFromFid(sInt16 aFid, string &aFieldName)
{
  if (fScriptContext==NULL) return false;
  TScriptVarDef *vardefP = fScriptContext->getVarDef(-aFid-1);
  if (vardefP) {
    aFieldName = vardefP->fVarName;
    return true;
  }
  // none found
  return false;
} // TScriptVarKey::getFieldNameFromFid



#endif // ENGINEINTERFACE_SUPPORT


} // namespace sysync


#endif // SCRIPT_SUPPORT

// eof
