/*
 *  File:         timezones.cpp
 *
 *  Author:       Beat Forster
 *
 *  Timezones conversion from/to linear time scale.
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 *  2004-04-18 : bfo : initial version
 *
 */


/* ToDo / open issues
 *
 * 1) done 04/04/18  <aISOstr> does not contain TZ info, how to bring it in ? -> new param
 * 2)      04/04/18  <aOffsetSecs> not yet limited to one day
 * 3) done 04/04/18  Is 20040101Z a valid 8601 ? (yes it is)
 * 4) done 04/04/20  Fill in all valid time zones
 * 5) done 04/04/20  Adapt straight forward conversions Enum <=> Name
 * 6) done 04/06/14  System time zone calculation for Linux
 * 7) done 04/06/14  InsertTZ / RemoveTZ implementation
 * 8)      06/04/12  Offset calculation for more complicated vTZ ("$") records
 * 9) done 07/02/26  Multi year list support
 */

// must be first in file, everything above is ignored by MVC compilers
#include "prefix_file.h"

#define TIMEZONES_INTERNAL 1

// do not import the whole thing to make life easier for standalone apps
#ifdef FULLY_STANDALONE
  #include "sysync_globs.h"
  #include "sysync_debug.h"
#else
  #include "sysync.h"
#endif

#include "lineartime.h"
#include "timezones.h"
#include "iso8601.h"
#include "stringutils.h"
#include "vtimezone.h"

namespace sysync {

static bool tzcmp  ( const tz_entry &t, const tz_entry &tzi, bool olsonSupport );
static bool YearFit( const tz_entry &t, const tz_entry &tzi, GZones* g );

// ---- global structure -----------------------------------------------------------

// %%% this is now a global variable, which is BAD. Unlike a const struct array, which can be put into the
//     code section, this "const" is a run-time generated structure (won't work e.g. for Symbian)
//     We should avoid this and create that object in the app level GZones, and let further GZone objects
//     allow using it (without copying!) - similar mechanism as the old CopyCustomTZFrom() had
//     (see 8742d15b65fecf02db54ce9b7447cee09f913ad0 commit which removed it)

const class tzdata : public std::vector<tz_entry>
{
 public:
  tzdata() {
    #ifndef NO_BUILTIN_TZ
    // add the global entries
    reserve(tctx_numtimezones);
    for (int i=0; i<tctx_numtimezones; i++) {
      const tbl_tz_entry &t = tbl_tz[i];
      if (i > 0 && back().name == t.name) {
        // the previous entry wasn't really the last of its group,
        // fix that
        back().groupEnd = false;
      }
      // add new entry, assuming that it terminates its group
      push_back(tz_entry(
        t.name, t.bias, t.biasDST, t.ident, t.dynYear,
        tChange(t.dst.wMonth, t.dst.wDayOfWeek, t.dst.wNth, t.dst.wHour, t.dst.wMinute),
        tChange(t.std.wMonth, t.std.wDayOfWeek, t.std.wNth, t.std.wHour, t.std.wMinute),
        true // groupEnd
      ));

      // allow olson names for Android
    //#ifdef ANDROID --
      if (t.olsonName!=NULL) back().location= t.olsonName;
    //#endif --
    }
    //push_back(tz_entry("unknown",               0,  0, "x",    "", tChange( 0, 0,0, 0,0),  tChange( 0, 0,0, 0,0)));  //   0
    #endif
  }
} tz;


// ---------------------------------------------------------------------------------
// GZones

bool GZones::initialize()
{
  bool ok = true;
  // load system wide definitions.
  bool nobuiltin = loadSystemZoneDefinitions(this);
  // %%%% later, we'll load system zones not into each GZones, but only once into
  //      a global list. Then, the return value of loadSystemZoneDefinitions() will
  //      determine if zones from the built-in list should be added or not
  // %%%% for now, we can't do that yet, built-in zones are always active
  if (!nobuiltin) {
    //%%% add entries from tz_table
  }
  return ok;
}

void GZones::loggingStarted()
{
  if (!fSystemZoneDefinitionsFinalized) {
    finalizeSystemZoneDefinitions(this);
    fSystemZoneDefinitionsFinalized = true;
  }
}


bool GZones::matchTZ(const tz_entry &aTZ, TDebugLogger *aLogP, timecontext_t &aContext)
{
  // keeps track of best match while iterating
  class comparison : public visitor {
    /** best solution so far has matching rules */
    bool fRuleMatch;
    /** best solution has matching location */
    bool fLocationMatch;
    /** best solution so far */
    timecontext_t fContext;

    /** the time zone we try to match */
    tz_entry fTZ;
    /** the TZID we try to match */
    string fTZID;

    /** the last entry without dynYear, i.e., the main entry of a group */
    timecontext_t fLeadContext;

    TDebugLogger *fLogP;

    /** time zones */
    GZones *fG;


  public:
    comparison(const tz_entry &aTZ, TDebugLogger *aLogP, GZones *g) :
      fRuleMatch(false),
      fLocationMatch(false),
      fContext(TCTX_UNKNOWN),
      fTZID(aTZ.name),
      fLeadContext(TCTX_UNKNOWN),
      fLogP(aLogP),
      fG(g)
    {
      // prepare information for tzcmp() and YearFit()
      fTZ.bias = aTZ.bias;
      fTZ.biasDST = aTZ.biasDST;
      fTZ.std = aTZ.std;
      fTZ.dst = aTZ.dst;
      // setting the year here instead of 'CUR' avoids repeated calls
      // to MyYear() inside YearFit()
      int year = MyYear(g);
      StringObjPrintf(fTZ.dynYear, "%d", year);
    }

    bool visit(const tz_entry &aTZ, timecontext_t aContext)
    {
      if (aTZ.ident=="x") return false; // comparison with this type is not possible

      bool olsonSupport= false;
      #ifdef ANDROID
           olsonSupport= true;
      #endif
      bool rule_match = tzcmp( fTZ, aTZ, olsonSupport ) && YearFit(fTZ, aTZ, fG);

      // start of a group is an entry that has a dynYear empty or set to "CUR" (for dynamically created entries)
      if (aTZ.dynYear.empty() || aTZ.dynYear == "CUR")
        fLeadContext = aContext;

      if (rule_match &&
         !fTZ.name.empty() && // empty match is NOT better !!
          fTZ.name == aTZ.name) {
        // name AND rule match => best possible match, return early
        PLOGDEBUGPRINTFX(fLogP, DBG_PARSE+DBG_EXOTIC,
                         ("matchTZ: final rule and name match for %s", fTZ.name.c_str()));
        fContext = fLeadContext;
        return true;
      }

      if (!aTZ.location.empty() &&
          fTZID.find(aTZ.location) != fTZID.npos) {
        // location name is part of the TZID we try to match
        if (!fLocationMatch ||
            (!fRuleMatch && rule_match)) {
          // previous match did not match location or
          // not the rules and we do, so this match is better
          PLOGDEBUGPRINTFX(fLogP, DBG_PARSE+DBG_EXOTIC,
                           ("matchTZ %s: location %s found, rules %s",
                            aTZ.name.c_str(), aTZ.location.c_str(),
                            rule_match ? "match" : "don't match"));
          fLocationMatch = true;
          fRuleMatch = rule_match;
          fContext = fLeadContext;
          return false;
        }
      }

      // a rule match with no better match yet?
      if (!fLocationMatch &&
          rule_match &&
          !fRuleMatch) {
          PLOGDEBUGPRINTFX(fLogP, DBG_PARSE+DBG_EXOTIC,
                           ("matchTZ %s: rules match (context #%d/%d, leadcontext=#%d)", aTZ.name.c_str(), TCTX_TZENUM(aContext), tctx_numtimezones, TCTX_TZENUM(fLeadContext)));
        fContext = fLeadContext;
        fRuleMatch = true;
      }

      return false;
    } // visit

    bool result(timecontext_t &aContext)
    {
      if (fContext != TCTX_UNKNOWN) {
        aContext = fContext;
        return true;
      } else {
        return false;
      }
    } // result
  }

  c(aTZ, aLogP, this);

  foreachTZ(c);

  return c.result(aContext);
} // matchTZ

bool GZones::foreachTZ(visitor &v)
{
  int  i; // visit hard coded elements first
  for (i= 1; i<(int)tctx_numtimezones; i++) {
    if (v.visit(tz[i], TCTX_ENUMCONTEXT(i))) {
      return true;
    }
  }

  bool result = false;
  #ifdef MUTEX_SUPPORT
    lockMutex(muP);
  #endif

  for (TZList::iterator pos= tzP.begin();
       pos!=tzP.end();
       pos++, i++) {
    if (v.visit(*pos, TCTX_ENUMCONTEXT(i)))
      break;
  } // for

  #ifdef MUTEX_SUPPORT
    unlockMutex(muP);
  #endif

  return result;
} // foreachTZ

// ---------------------------------------------------------------------------------

// Get signed minute offset
sInt16 TCTX_MINOFFSET( timecontext_t tctx ) {
  return (sInt16)(tctx & TCTX_OFFSETMASK );
} // TCTX_MINOFFSET


// Get time zone enum
TTimeZones TCTX_TZENUM( timecontext_t tctx ) {
  return (TTimeZones)(tctx & TCTX_OFFSETMASK );
} // TCTX_TZENUM


// Check if <tctx> is a symbolic TZ info
bool TCTX_IS_TZ( timecontext_t tctx ) {
  return (tctx & TCTX_SYMBOLIC_TZ)!=0;
} // TCTX_IS_TZ


// Check if <tctx> is a built-in symbolic TZ info
bool TCTX_IS_BUILTIN( timecontext_t tctx ) {
  return
    (tctx & TCTX_SYMBOLIC_TZ) && // is a symbolic time zone
    (TCTX_TZENUM(tctx)<tctx_numtimezones); // and is in the internal list
} // TCTX_IS_BUILTIN


// Check if <tctx> has TCTX_DATEONLY mode set (but not TIMEONLY - both set means same as both not set)
bool TCTX_IS_DATEONLY( timecontext_t tctx ) {
  return (tctx  & TCTX_DATEONLY) && !(tctx  & TCTX_TIMEONLY);
} // TCTX_IS_DATEONLY


// Check if <tctx> has TCTX_TIMEONLY mode set (but not DATEONLY - both set means same as both not set)
bool TCTX_IS_TIMEONLY( timecontext_t tctx ) {
  return (tctx  & TCTX_TIMEONLY) && !(tctx  & TCTX_DATEONLY);
} // TCTX_IS_TIMEONLY


// Check if <tctx> has TCTX_DURATION mode set
bool TCTX_IS_DURATION( timecontext_t tctx ) {
  return ( tctx  & TCTX_DURATION )!=0;
} // TCTX_IS_DURATION


// Check if <tctx> is a unknown time zone
bool TCTX_IS_UNKNOWN( timecontext_t tctx ) {
  return TCTX_IS_TZ( tctx ) && ( TCTX_TZENUM( tctx )==tctx_tz_unknown );
} // TCTX_IS_UNKNOWN


// Check if <tctx> is the system time zone
bool TCTX_IS_SYSTEM ( timecontext_t tctx ) {
  return TCTX_IS_TZ( tctx ) && ( TCTX_TZENUM( tctx )==tctx_tz_system );
} // TCTX_IS_SYSTEM


// Check if <tctx> is the UTC time zone
bool TCTX_IS_UTC ( timecontext_t tctx ) {
  return TCTX_IS_TZ( tctx ) && ( TCTX_TZENUM( tctx )==tctx_tz_UTC );
} // TCTX_IS_SYSTEM


//! result is render flags from <aRFlagContext> with zone info from <aZoneContext>
timecontext_t TCTX_JOIN_RFLAGS_TZ ( timecontext_t aRFlagContext, timecontext_t aZoneContext )
{
  return
    (aRFlagContext &  TCTX_RFLAGMASK) |
    (aZoneContext  & ~TCTX_RFLAGMASK);
} // TCTX_JOIN_RFLAGS_TZ



// ---- utility functions --------------------------------------------------------------
// Specific bias string. Unit: hours
string BiasStr( int bias )
{
  char hrs[ 80 ];
  float f= (float)bias/MinsPerHour;

  if          (bias % MinsPerHour == 0) sprintf( hrs,"%3.0f    ", f ); // not with .0
  else if (abs(bias % MinsPerHour)==30) sprintf( hrs,"%5.1f  ",   f );
  else                                  sprintf( hrs,"%6.2f ",    f );

  return hrs;
} // BiasStr


// Clear the DST info of <t>
void ClrDST( tz_entry &t )
{
  memset( &t.dst, 0, sizeof(t.dst) );
  memset( &t.std, 0, sizeof(t.std) );
} // ClrDST


// -------------------------------------------------------------------------------------
// Special cases: <year> =  0, the one w/o dynYear
//                       = -1, direct index
bool GetTZ( timecontext_t aContext, tz_entry &t, GZones* g, int year )
{
  if (!TCTX_IS_TZ( aContext )) return false;

  int           aTZ= aContext & TCTX_OFFSETMASK;
  if (aTZ>=0 && aTZ<tctx_numtimezones) {
    while (true) {
      t= tz[ aTZ ]; if (t.dynYear.empty() || year==-1) break; // replace it by a non-dynYear
      if    (aTZ==0)                                   break;
             aTZ--;
    } // while

    if (year<=0) return true;

    int    nx=  aTZ+1;
    while (nx<tctx_numtimezones) { // search for the dynamic year
      if (t.name != tz[ nx ].name) break; // no or no more
      t=            tz[ nx ];
      if (year<=atoi( t.dynYear.c_str() )) break; // this is the year line we are looking for
      nx++;
    } // while

    return true; // this is it !!
  } // if

  t= tz[ tctx_tz_unknown ];  // default, if not yet ok
  if (g==NULL) return false; // If there is no <g>, it is definitely false

  // -----------------------
  bool ok= false;
//if (g==NULL) g= gz();      // either <g> or global list

  #ifdef MUTEX_SUPPORT
    lockMutex( g->muP );
  #endif

  int  i = tctx_numtimezones;
  TZList::iterator pos;
  for (pos= g->tzP.begin();          // go thru the additional list
       pos!=g->tzP.end(); pos++) {
    if   (aTZ==i &&                  // no removed elements !!
          !(pos->ident=="-")) {
      t =  *pos;
      ok= true;
      if (year<=0) break; // pass unlock now

             pos++;
      while (pos!=g->tzP.end()) { // search for the dynamic year
        if (t.name != pos->name) break; // no or no more
        t=*pos;
        if (year<=atoi( t.dynYear.c_str() )) break; // this is the year line we are looking for
        pos++;
      } // while

      break; // pass unlock now
    } // if

    i++;
  } // for

  #ifdef MUTEX_SUPPORT
    unlockMutex( g->muP );
  #endif
  // -----------------------

  return ok;
} /* GetTZ */



void Get_tChange( lineartime_t tim, tChange &v, sInt16 &y, bool asDate )
{
  sInt16 day, d, sec, ms;

  lineartime2date( tim, &y,       &v.wMonth,  &day );
  lineartime2time( tim, &v.wHour, &v.wMinute, &sec, &ms );

  if (asDate) {
    v.wDayOfWeek=  -1; // use it as date directly
    v.wNth      = day;
  }
  else {
    v.wDayOfWeek= lineartime2weekday( tim );
    v.wNth      = ( day-1 ) / DaysPerWk + 1;

    if (v.wNth==4) { // the last one within month ?
                 d= day + DaysPerWk;
      AdjustDay( d, v.wMonth, y );
      if       ( d==day ) v.wNth= 5;
    } // if
  } // if
} // Get_tChange



static bool Fill_tChange( string iso8601, int bias, int biasDST, tChange &tc, sInt16 &y, bool isDST )
{
  lineartime_t  l;
  timecontext_t c;
//sInt16        y, day, d, sec, ms;

  string::size_type rslt= ISO8601StrToTimestamp( iso8601.c_str(), l, c );
  if    (rslt!=iso8601.length()) return false;

  if (iso8601[ rslt-1 ]=='Z') { // to it for UTC only
    int                     bMins = bias;
    if (!isDST)             bMins+= biasDST;
    l+= seconds2lineartime( bMins*SecsPerMin );
  } // if

  Get_tChange( l, tc, y );

  /*
  lineartime2date( l, &y,        &tc.wMonth,  &day );
  lineartime2time( l, &tc.wHour, &tc.wMinute, &sec, &ms );
  tc.wDayOfWeek= lineartime2weekday( l );
  tc.wNth      = ( day-1 ) / DaysPerWk + 1;

  if (tc.wNth==4) { // the last one within month ?
               d= day + DaysPerWk;
    AdjustDay( d, tc.wMonth, y );
    if       ( d==day ) tc.wNth= 5;
  } // if
  */

  return true;
} // Fill_tChange



// Get int value <i> as string
static string IntStr( sInt32 i )
{
  const int    FLen= 15;    /* max length of (internal) item name */
  char      f[ FLen ];
  // cheating: this printf format assumes that sInt32 == int
  sprintf ( f, "%d", int(i) );
  string s= f;
  return s;
} // IntStr



bool GetTZ( string std, string dst, int bias, int biasDST, tz_entry &t, GZones* g )
{
  t.name   = "";
  t.bias   = bias;
  t.biasDST= biasDST;
  t.ident  = "";
  t.dynYear= "";

  sInt16 y;
  bool   ok= Fill_tChange( std, bias,biasDST, t.std, y, false ) &&
             Fill_tChange( dst, bias,biasDST, t.dst, y, true  );
  if    (ok) t.dynYear= IntStr( y );

//printf( "tS   m=%d dw=%d n=%d H=%d M=%d\n",   t.std.wMonth,   t.std.wDayOfWeek, t.std.wNth,
//                                                              t.std.wHour,      t.std.wMinute );
//printf( "tD   m=%d dw=%d n=%d H=%d M=%d\n",   t.dst.wMonth,   t.dst.wDayOfWeek, t.dst.wNth,
//                                                              t.dst.wHour,      t.dst.wMinute );
//printf( "t    bs=%d bd=%d\n",                 t.bias,         t.biasDST );
//
//printf( "ok=%d y=%d dyn='%s'\n", ok, y, t.dynYear.c_str() );

  return ok;
} // GetTZ


static bool Same_tChange( const tChange &tCh1, const tChange &tCh2 )
{
  return tCh1.wMonth    ==tCh2.wMonth     &&
         tCh1.wDayOfWeek==tCh2.wDayOfWeek &&
         tCh1.wNth      ==tCh2.wNth       &&
         tCh1.wHour     ==tCh2.wHour      &&
         tCh1.wMinute   ==tCh2.wMinute;
} // Same_tChange


/*! Compare time zone information */
static bool tzcmp( const tz_entry &t, const tz_entry &tzi, bool olsonSupport )
{
  bool sameName= !tzi.name.empty() &&
         strucmp( tzi.name.c_str(), t.name.c_str() )==0;

  bool sameLoc = false; // by default, no olson support here
//#ifdef ANDROID --
  if (olsonSupport) {
    // allow olson names as well here
       sameLoc=  !tzi.location.empty() &&
         strucmp( tzi.location.c_str(), t.name.c_str() )==0;

//__android_log_print( ANDROID_LOG_DEBUG, "TZ CMP", "'%s' == '%s' / '%s'\n",
//                     t.name.c_str(), tzi.name.c_str(), tzi.location.c_str() );
  }
//#endif --

//printf( "name='%s' loc='%s' ident='%s' sameName=%d sameLoc=%d\n",
//         t.name.c_str(), t.ident.c_str(), tzi.location.c_str(), sameName, sameLoc );

  if        (!t.name.empty() &&       !sameName && !sameLoc)  return false;

  bool idN=   t.ident.empty();
  if (!idN && t.ident == "?" &&       (sameName ||  sameLoc)) return true;

  if (!idN && t.ident == "$" &&
              t.ident == tzi.ident && (sameName ||  sameLoc)) return true;

  /*
  PNCDEBUGPRINTFX( DBG_SESSION,( "tS   m=%d dw=%d n=%d h=%d M=%d\n",   t.std.wMonth,   t.std.wDayOfWeek,   t.std.wNth,
                                                                       t.std.wHour,    t.std.wMinute ) );
  PNCDEBUGPRINTFX( DBG_SESSION,( "tD   m=%d dw=%d n=%d h=%d M=%d\n",   t.dst.wMonth,   t.dst.wDayOfWeek,   t.dst.wNth,
                                                                       t.dst.wHour,    t.dst.wMinute ) );
  PNCDEBUGPRINTFX( DBG_SESSION,( "tziS m=%d dw=%d n=%d h=%d M=%d\n", tzi.std.wMonth, tzi.std.wDayOfWeek, tzi.std.wNth,
                                                                     tzi.std.wHour,  tzi.std.wMinute ) );
  PNCDEBUGPRINTFX( DBG_SESSION,( "tziD m=%d dw=%d n=%d h=%d M=%d\n", tzi.dst.wMonth, tzi.dst.wDayOfWeek, tzi.dst.wNth,
                                                                     tzi.dst.wHour,  tzi.dst.wMinute ) );

  PNCDEBUGPRINTFX( DBG_SESSION,( "t    bs=%d bd=%d\n",   t.bias,   t.biasDST ) );
  PNCDEBUGPRINTFX( DBG_SESSION,( "tzi  bs=%d bd=%d\n", tzi.bias, tzi.biasDST ) );
  */

  if (t.bias!=tzi.bias) return false; // bias must be identical

  if (t.ident == "o") {
    // stop comparing, return result of last check
    return t.biasDST == tzi.biasDST;
  }

  bool   tIsDst= DSTCond( t   );
  bool tziIsDst= DSTCond( tzi );
  if (tIsDst!=tziIsDst) return false; // DST cond must be on or off for both

  if (tIsDst) {
  //if (memcmp(&t.dst,    &tzi.dst, sizeof(t.dst))!=0 ||
  //    memcmp(&t.std,    &tzi.std, sizeof(t.std))!=0 ||
    if (!Same_tChange( t.dst,     tzi.dst ) ||
        !Same_tChange( t.std,     tzi.std ) ||
                       t.biasDST!=tzi.biasDST) return false;
  } // if

  return !(tzi.ident=="-") && // not removed
                     ( idN ||
           t.ident.empty() ||
           t.ident == tzi.ident );

  /*
  return memcmp(&t.dst,    &tzi.dst, sizeof(t.dst))==0 &&
         memcmp(&t.std,    &tzi.std, sizeof(t.std))==0 &&
                 t.bias   ==tzi.bias                   &&
                 t.biasDST==tzi.biasDST                && //%%% luz: this must be compared as well
         strcmp( "-",       tzi.ident )!=0             && // removed
               (   idN                     ||
         strcmp( t.ident,   ""        )==0 ||
         strcmp( t.ident,   tzi.ident )==0 );
  */
} // tzcmp



sInt16 MyYear( GZones* g )
{
  sInt16 y, m, d;

  lineartime_t     t= getSystemNowAs( TCTX_UTC, g, true );
  lineartime2date( t, &y,&m,&d );
  return y;
} // MyYear


static bool YearFit( const tz_entry &t, const tz_entry &tzi, GZones* g )
{
  int                     yearS;
  if (t.dynYear == "CUR") yearS= MyYear( g );
  else                    yearS= atoi( t.dynYear.c_str() );
  if                     (yearS==0) return true;

  int yearI= atoi( tzi.dynYear.c_str() );
  if (yearI==0) return true;

  if  (tzi.groupEnd) return yearS>=yearI;
  else              return yearS<=yearI;
} // YearFit



/*  Returns true, if the given TZ is existing already
 *    <t>            tz_entry to search for:
 *                   If <t.name> == "" search for any entry with these values.
 *                      <t.name> != "" name must fit
 *                   If <t.ident>== "" search for any entry with these values
 *                      <t.ident>!= "" name must fit
 *    <aName>        is <t.name> of the found record
 *    <aContext>     is the assigned context
 *    <createIt>     create an entry, if not yet existing / default: false
 *    <searchOffset> says, where to start searching       / default: at the beginning
 *
 *    supported <t.ident> values:
 *      ""   any       (to search)
 *      "?"  name only (to search)
 *      "o"  compare offsets, but not changes (to search); name is compared if set
 *
 *      "x"  unknown/system
 *      "m"  military zones
 *      "s"  standard zones
 *      "d"  daylight zones
 *      "-"  removed  zones
 *      "$"  not converted zones (pure text)
 *      " "  all others
 */
bool FoundTZ( const tz_entry &tc,
              string        &aName,
              timecontext_t &aContext, GZones* g, bool createIt,
              timecontext_t searchOffset,
              bool          olsonSupport )
{
  aName    = "";
  aContext = TCTX_UNKNOWN;
  int offs = TCTX_OFFSCONTEXT( searchOffset );
  bool  ok = false;
  tz_entry t = tc;

  if (!t.ident.empty() && // specific items will not contain more info
     !(t.ident==" ")) {
    ClrDST  ( t );
  } // if

  int  i; // search hard coded elements first
  for (i= offs+1; i<(int)tctx_numtimezones; i++) {
    const tz_entry &tzi = tz[ i ];
    if (tzcmp  ( t, tzi, olsonSupport ) &&
        YearFit( t, tzi, g )) {
      aName=        tzi.name;
    //printf( "name='%s' i=%d\n", aName.c_str(), i );
      ok   = true; break;
    } // if
  } // for

//printf( "ok=%d name='%s' i=%d olson=%d\n", ok, aName.c_str(), i, olsonSupport );

  // don't go thru the mutex, if not really needed
  if (!ok && g!=NULL) {
    // -------------------------------------------
  //if (g==NULL) g= gz();

    #ifdef MUTEX_SUPPORT
      lockMutex( g->muP );
    #endif

    TZList::iterator pos;
    int j= offs-(int)tctx_numtimezones; // remaining gap to be skipped

    // Search for all not removed elements first
    for (pos= g->tzP.begin();
         pos!=g->tzP.end(); pos++) {
      if (j<0 &&
          !(pos->ident=="-") && // element must not be removed
          tzcmp( t, *pos, olsonSupport )) {
        aName= pos->name;
        ok   = true; break;
      } // if

      i++;
      j--;
    } // for

    // now check, if an already removed element can be reactivated
    if (createIt && !ok) {
      i=      (int)tctx_numtimezones;
      j= offs-(int)tctx_numtimezones; // remaining gap to be skipped

      for (pos= g->tzP.begin();
           pos!=g->tzP.end(); pos++) {
        if (j<0 &&
            pos->ident == "-" && // removed element ?
            tzcmp( t, *pos, olsonSupport )) {
          pos->ident= t.ident; // reactivate the identifier
          aName = pos->name;  // should be the same
          ok    = true; break;
        } // if

        i++;
        j--;
      } // for
    } // if

    // no such element => must be created
    if (createIt && !ok) { // create it, if not yet ok
      g->tzP.push_back( t );
      ok= true;
    } // if

    #ifdef MUTEX_SUPPORT
      unlockMutex( g->muP );
    #endif
    // -------------------------------------------
  } // if

  if (ok) aContext= TCTX_ENUMCONTEXT( i );
  return  aContext!=TCTX_UNKNOWN;
} /* FoundTZ */



/* Remove a time zone definition, if already existing
 * NOTE: Currently only dynamic entries can be removed
 */
bool RemoveTZ( const tz_entry &t, GZones* g )
{
//printf( "RemoveTZ '%s' %d\n", t.name, t.bias );
  bool ok= false;

  // ---------------------------
  if (g==NULL) return ok;
//if (g==NULL) g= gz();

  #ifdef MUTEX_SUPPORT
    lockMutex( g->muP );
  #endif

  bool olsonSupport= false;
  #ifdef ANDROID
       olsonSupport= true;
  #endif

  TZList::iterator pos;
  for (pos= g->tzP.begin();
       pos!=g->tzP.end(); pos++) {
    if (!(pos->ident=="-") && // element must not be removed
        tzcmp( t, *pos, olsonSupport )) {
      pos->ident = "-";
    //gz()->tzP.erase( pos ); // do not remove it, keep it persistent
      ok= true; break;
    } // if
  } // for

  #ifdef MUTEX_SUPPORT
    unlockMutex( g->muP );
  #endif
  // ---------------------------

  return ok;
} /* RemoveTZ */



bool TimeZoneNameToContext( cAppCharP aName, timecontext_t &aContext, GZones* g, bool olsonSupport )
{
  // check some special cases
  if (strucmp(aName,"DATE")==0) {
    aContext = TCTX_UNKNOWN|TCTX_DATEONLY;
    return true;
  }
  else if (strucmp(aName,"DURATION")==0) {
    aContext = TCTX_UNKNOWN|TCTX_DURATION;
    return true;
  }
  else if (*aName==0 || strucmp(aName,"FLOATING")==0) {
    aContext = TCTX_UNKNOWN;
    return true;
  }

  const char* GMT= "GMT";
  const char* UTC= "UTC";

  int      v, n;
  char*    q;
  tz_entry t;

  t.name   = aName; // prepare searching
  t.ident  = "?";
  t.dynYear= "";    // luz: must be initialized!

  string          tName;
  if (FoundTZ( t, tName, aContext, g, olsonSupport )) return true;

  /*
  int  i;     aContext= TCTX_UNKNOWN;
  for (i=0; i<(int)tctx_numtimezones; i++) {
    if (strucmp( tz[i].name,aName )==0) { aContext= TCTX_ENUMCONTEXT(i); break; }
  } // for

  if (aContext!=TCTX_UNKNOWN) return true;
  */

  // calculate the UTC offset
  n= 1;        q= (char *)strstr( aName,UTC );
  if (q==NULL) q= (char *)strstr( aName,GMT );
  if (q!=NULL  &&   strlen( q )>strlen( UTC )) {
    q+= strlen( UTC );
    n= MinsPerHour;
  }
  else {
    q= (char*)aName;
  } // if

      v= atoi( q );
  if (v!=0 || strcmp( q, "0" )==0
           || strcmp( q,"+0" )==0
           || strcmp( q,"-0" )==0) {
    aContext= TCTX_OFFSCONTEXT(v*n); return true;
  } // if

  return false;
} /* TimeZoneNameToContext */



bool TimeZoneContextToName( timecontext_t aContext, string &aName, GZones* g,
                                cAppCharP aPrefIdent )
{
  // check some special cases
  if (!TCTX_IS_TZ( aContext )) {
    short lBias=   TCTX_MINOFFSET( aContext );
          aName= "OFFS" + HourMinStr( lBias );
    return true;
  } // if

  if (TCTX_IS_UNKNOWN(aContext)) {
    aName = TCTX_IS_DURATION(aContext) ? "DURATION" : (TCTX_IS_DATEONLY(aContext) ? "DATE" : "FLOATING");
    return true;
  }

  tz_entry t;
  aName= "UNKNOWN";

  // setting it via param does not work currently for some reasons, switch it on permanently for Android
//#ifdef ANDROID --
    aPrefIdent= "o";
  //__android_log_print( ANDROID_LOG_DEBUG, "ContextToName", "pref='%s' / aContext=%d\n", aPrefIdent, aContext );
//#endif --

  // if aPrefIndent contains "o", this means we'd like to see olson name, if possible
  // %%% for now, we can return olson for the built-ins only
  if (TCTX_IS_BUILTIN(aContext) && aPrefIdent && strchr(aPrefIdent, 'o')!=NULL) {
    #ifndef NO_BUILTIN_TZ
    // look up in hardcoded table
    cAppCharP oname = tbl_tz[TCTX_TZENUM(aContext)].olsonName;
    if (oname) {
      // we have an olson name for this entry, return it
      aName = oname;
      return true;
    }
    #endif
  }

  // %%% <aPrefIdent> has not yet any influence
  if (GetTZ( aContext, t, g )) {
    if (t.ident == "$") return false; // unchanged elements are not yet supported
    aName= t.name;
  } // if

  return true;
} /* TimeZoneContextToName */



/*! Is it a DST time zone ? (both months must be defined for DST mode) */
bool DSTCond( const tz_entry &t ) {
  return (t.dst.wMonth!=0 &&
          t.std.wMonth!=0);
} // DSTCond



/* adjust to day number within month <m>, %% valid till year <y> 2099 */
void AdjustDay( sInt16 &d, sInt16 m, sInt16 y )
{
  while (d>31)                                    d-= DaysPerWk;
  if    (d>30 && (m==4 || m==6 || m==9 || m==11)) d-= DaysPerWk;
  if    (d>29 &&  m==2)                           d-= DaysPerWk;
  if    (d>28 &&  m==2 && (y % 4)!=0)             d-= DaysPerWk;
} // AdjustDay


/* Get the day where STD <=> DST switch will be done */
static sInt16 DaySwitch( lineartime_t aTime, const tChange* c )
{
  sInt16 y, m, d, ds;
  lineartime2date( aTime, &y, &m, &d );

  if   (c->wDayOfWeek==-1) {
    ds= c->wNth;
  }
  else {
    sInt16 wkDay= lineartime2weekday( aTime );
    ds = ( wkDay +  5*DaysPerWk -   ( d-1 )     ) % DaysPerWk;     /* wkday of 1st  */
    ds = ( DaysPerWk - ds       + c->wDayOfWeek ) % DaysPerWk + 1; /* 1st occurance */
    ds+= ( c->wNth-1 )*DaysPerWk;

    AdjustDay( ds, c->wMonth, y );
  } // if

  return ds;
} // DaySwitch


/*! Get lineartime_t of <t> for a given <year>, either from std <toDST> or vice versa */
lineartime_t DST_Switch( const tz_entry &t, int bias, sInt16 aYear, bool toDST )
{
  sInt16 ds;

  const tChange*   c;
  if (toDST) c= &t.dst;
  else       c= &t.std;

  lineartime_t tim= date2lineartime( aYear, c->wMonth, 1 );

  ds= DaySwitch( tim, c );

  sInt32      bMins = t.bias;
  if (!toDST) bMins+= t.biasDST;

  return    date2lineartime( aYear,    c->wMonth,  ds  )
       +    time2lineartime( c->wHour, c->wMinute, 0,0 )
       - seconds2lineartime( bMins*SecsPerMin );
} /* DST_Switch */



/* Check whether <aValue> of time zone <t> is DST based */
static bool IsDST( lineartime_t aTime, const tz_entry &t )
{
  bool   ok;
  sInt16 y, m, d, h, min, ds;

  if (!DSTCond( t )) return false;

  lineartime2date( aTime, &y, &m, &d );
  lineartime2time( aTime, &h, &min, NULL, NULL );

  const tChange*       c= NULL;
  if (m==t.std.wMonth) c= &t.std;
  if (m==t.dst.wMonth) c= &t.dst;

  /* calculation is a little bit more tricky within the two switching months */
  if (c!=NULL) {
    /*
    if   (c->wDayOfWeek==-1) {
      ds= c->wNth;
    }
    else {
      sInt16 wkDay= lineartime2weekday( aTime );
      ds = ( wkDay +   5*DaysPerWk - (d-1)          ) % DaysPerWk;     // wkday of 1st
      ds = ( DaysPerWk  - ds       + c->wDayOfWeek  ) % DaysPerWk + 1; // 1st occurance
      ds+= ( c->wNth-1 )*DaysPerWk;

      AdjustDay( ds, m, y );
    } // if
    */

    /* <ds> is the day when dst<=>std takes place */
        ds= DaySwitch( aTime, c );
    if (ds        < d   ) return c==&t.dst;
    if (ds        > d   ) return c==&t.std;

    /* day  correct => compare hours */
    if (c->wHour  < h   ) return c==&t.dst;
    if (c->wHour  > h   ) return c==&t.std;

    /* hour correct => compare minutes */
    if (c->wMinute< min ) return c==&t.dst;
    if (c->wMinute> min ) return c==&t.std;

    /* decide for the margin, if identical */
    return c==&t.dst;
  } /* if */

  /* northern and southern hemnisphere supported */
  if (t.dst.wMonth<t.std.wMonth)
         ok= m>t.dst.wMonth && m<t.std.wMonth;
  else   ok= m<t.std.wMonth || m>t.dst.wMonth;
  return ok;
} /* IsDST */



static sInt32 DST_Offs( lineartime_t aValue, const tz_entry &t, bool backwards )
{
  if (backwards) aValue+= (lineartime_t)(t.bias*SecsPerMin)*secondToLinearTimeFactor;
  if (IsDST( aValue,t )) return t.bias + t.biasDST;
  else                   return t.bias;
} /* DST_Offs */



/* get offset in minutes */
static bool TimeZoneToOffs( lineartime_t aValue, timecontext_t aContext,
                                 bool backwards, sInt32 &offs, GZones* g )
{
  tz_entry t;

  if (TCTX_IS_TZ( aContext )) {
    offs= 0; // default

    sInt16                     year;
    lineartime2date( aValue,  &year, NULL, NULL );
    if (GetTZ( aContext, t, g, year )) {
      if (t.ident == "$") return false; // unchanged elements are not yet supported
      offs= DST_Offs( aValue, t, backwards );
    } // if
  }
  else {
    offs= TCTX_MINOFFSET(aContext);
  } // if

  return true;
} /* TimeZoneToOffs */




timecontext_t SelectTZ( TDaylightSavingZone zone, int bias, int biasDST, lineartime_t tNow, bool isDbg )
{
  bool dst, ok;
  bool withDST= zone!=EDstNone;
  timecontext_t t= tctx_tz_unknown;
  bool special= false; // possibly needed true for NGage

  int  i; // go thru the whole list of time zones
  for (i=(int)tctx_tz_system+1; i<(int)tctx_numtimezones; i++) {
    bool tCond= DSTCond( tz[ i ] ); // are there any DST rules ?
    if  (tCond==withDST) {
      if (withDST) dst= IsDST( tNow, tz[ i ] ); // check, if now in DST
      else         dst= false;

      int                 b= bias;
      if (dst && special) b= bias - biasDST;
      if (tz[ i ].bias==b) { // the bias must fit exactly
        switch (zone) {
          case EDstEuropean :
          case EDstNorthern : ok= tz[ i ].dst.wMonth<tz[ i ].std.wMonth; break;
          case EDstSouthern : ok= tz[ i ].dst.wMonth>tz[ i ].std.wMonth; break;
          default           : ok= true;
        } // switch

        if (isDbg) {
          PNCDEBUGPRINTFX( DBG_SESSION,( " %d %d dst=%d cond=%d '%s'",
                                         i, bias, dst, tCond, tz[ i ].name.c_str() ));
        }
        if   (ok) { // check, if european time zone
          ok= (zone==EDstEuropean) ==
            (tz[ i ].name.find("Europe") != string::npos ||
             tz[ i ].name == "CET/CEST" ||
             tz[ i ].name == "Romance" ||
             tz[ i ].name == "GMT");
          if (ok) { t= i; break; }
        } // if
      } // if
    } // if
  } // for

  if (isDbg) {
    PNCDEBUGPRINTFX( DBG_SESSION,( "SelectTZ: zone=%s bias=%d",
                                   tz[ t ].name.c_str(), bias ) );
  }
  return TCTX_SYMBOLIC_TZ+t;
} // SelectTZ



/* get system's time zone */
static bool MyContext( timecontext_t &aContext, GZones* g )
{
  bool isDbg= false;
  // check for cached system time zone, return it if available
  if      (g) {
    isDbg= g->isDbg;
    // luz: added safety here to avoid that incorrectly initialized GZone
    //      (with tctx_tz_unknown instead of TCTX_UNKNOWN, as it was in timezones.h)
    //      does not cause that system timezone is assumed known (as UTC) and returned WRONG!)
    //      Note: rearranged to make non-locked writing to g->sysTZ is safe (for hacks needed pre-3.1.2.x!)
    timecontext_t   curSysTZ= g->sysTZ;
    if    (!(TCTX_IS_UNKNOWN( curSysTZ ) ||
             TCTX_IS_SYSTEM ( curSysTZ )
            ))    { aContext= curSysTZ; return true; }
  } // if

  // there is no system time zone cached, we need to determine it from the operating system
  // - call platform specific routine
  bool ok = getSystemTimeZoneContext( aContext, g );
  if (isDbg) {
    PNCDEBUGPRINTFX( DBG_SESSION, ( "MyContext: %08X ok=%d", aContext, ok ));
  }
  // update cached system context
  if (g && ok) g->sysTZ= aContext; // assign the system context
  return   ok;
} /* MyContext */


/* %%% luz 2009-04-02 seems of no relevance except for Symbian/Epoc -> Epoc code moved to Symbian/platform_timezones.cpp
   added a replacement here as it is still used from sysytest, which should return the same result (but not
   the debug output */
timecontext_t SystemTZ( GZones *g, bool isDbg )
{
  timecontext_t tctx;
  if (MyContext(tctx, g))
    return tctx;
  else
    return TCTX_UNKNOWN;
}



bool ContextForEntry( timecontext_t &aContext, tz_entry &t, bool chkNameFirst, GZones* g )
{
  string s;
  string sName = t.name;
  bool ok = true;
  do {
    if (chkNameFirst &&       !sName.empty() &&
        TimeZoneNameToContext( sName.c_str(),aContext, g )) break;

    // if there are no rules defined => switch it off
    if (t.dst.wMonth==0 ||
        t.std.wMonth==0 ||
        Same_tChange( t.dst, t.std )) {
      ClrDST( t );
    } // if

                 t.name   = (char*)sName.c_str();
                 t.ident  = "";
                 t.dynYear= ""; // MUST be set as this is assigned unchecked to a string
                                // (which crashes when assigned NULL or low number)
    if (FoundTZ( t, s, aContext, g )) break; // with this name

                 t.name   = "";
                 t.dynYear= "CUR";
    if (FoundTZ( t, s, aContext, g )) break; // with different name

    int i= 0;
    if (sName.empty()) sName= "unassigned";

    string v;
    while (true) {
               v = sName.c_str();
      if (i>0) v+= "_" + IntStr( i );
      if (!TimeZoneNameToContext( v.c_str(),aContext, g )) break; // A timezone with this name must not exist
      i++;
    } // while

    t.name= (char*)v.c_str();

    string      tz_Name;
    FoundTZ( t, tz_Name, aContext, g, true ); // create it
    if (TimeZoneNameToContext( v.c_str(),aContext, g )) break;
    ok= false;

    /*
    t.name= (char*)sName.c_str();

    string      tzName;
    FoundTZ( t, tzName, aContext, g, true ); // create it
    if (TimeZoneNameToContext( sName.c_str(),aContext, g )) break;
    ok= false;
    */
  } while (false);
  return ok;
} /* ContextFromNameAndEntry */



/* returns true, if the context rules are identical */
static bool IdenticalRules( timecontext_t aSourceContext,
                            timecontext_t aTargetContext, GZones* g )
{
  tz_entry ts, tt;

  /* only performed in TZ mode */
  if (!GetTZ( aSourceContext, ts, g, 0 ) ||
      !GetTZ( aTargetContext, tt, g, 0 )) return false;

  TTimeZones asTZ= TCTX_TZENUM( aSourceContext );
  TTimeZones atTZ= TCTX_TZENUM( aTargetContext );
  if  (asTZ==atTZ) return true; // identical

  if (ts.ident == "$" ||
      tt.ident == "$") return false;

  if              (ts.bias==tt.bias  &&
  // memcmp( &ts.dst, &tt.dst, sizeof(tChange))==0 &&
  // memcmp( &ts.std, &tt.std, sizeof(tChange))==0) return true;
     Same_tChange( ts.dst,  tt.dst ) &&
     Same_tChange( ts.std,  tt.std )) return true;

  /*
  if (!TCTX_IS_TZ( aSourceContext ) ||
      !TCTX_IS_TZ( aTargetContext )) return false;

  // get the time zones for calculation
  TTimeZones asTZ= TCTX_TZENUM(aSourceContext);
  TTimeZones atTZ= TCTX_TZENUM(aTargetContext);

  if (asTZ==atTZ) return true;

  if (         tz[ asTZ ].bias==tz[ atTZ ].bias &&
      memcmp( &tz[ asTZ ].dst, &tz[ atTZ ].dst, sizeof(tChange))==0 &&
      memcmp( &tz[ asTZ ].std, &tz[ atTZ ].std, sizeof(tChange))==0) {
    return true;
  } // if
  */

  return false;
} /* IdenticalRules */



/*! get system's time zone context (i.e. resolve the TCTX_SYSTEM meta-context)
 *  @param[in,out] aContext : context will be made non-meta, that is, if input is TCTX_SYSTEM,
 *                            actual time zone will be determined.
 */
bool TzResolveMetaContext( timecontext_t &aContext, GZones* g )
{
  if (!TCTX_IS_SYSTEM(aContext))
    return true; // no meta zone, just return unmodified
  // is meta-context TCTX_SYSTEM, determine actual symbolic context
  return MyContext(aContext,g);
} // TzResolveMetaContext


/* make time context non-symbolic (= calculate minute offset east of UTC for aRefTime) */
bool TzResolveContext( timecontext_t &aContext, lineartime_t aRefTime, bool aRefTimeUTC, GZones* g )
{
  sInt32 offs;
  // resolve possible meta context (TCTX_SYSTEM at this time)
  if (!TzResolveMetaContext(aContext,g)) return false;
  // check if already an offset (non-symbolic)
  if (!TCTX_IS_TZ(aContext))
    return true; // yes, no conversion needed
  // is symbolic, needs conversion to offset
  bool ok = TimeZoneToOffs(
    aRefTime, // reference time for which we want to know the offset
    aContext, // context
    aRefTimeUTC, // "backwards" means refTime is UTC and we want know offset to go back to local
    offs, // here we get the offset
    g
  );
  if (ok) {
    aContext = TCTX_JOIN_RFLAGS_TZ(
      aContext, // join original rendering flags...
      TCTX_OFFSCONTEXT(offs) // ...with new offset based context
    );
  }
  return ok;
} // TzResolveContext


/*! calculate minute offset east of UTC for aRefTime
 *  @param[in] aContext       : time context to resolve
 *  @param[out] aMinuteOffset : receives minute offset east of UTC
 *  @param[in] aRefTime       : reference time point for resolving the offset
 *  @param[in] aRefTimeUTC    : if set, reference time must be UTC,
 *                              otherwise, reference time must be in context of aContext
 */
bool TzResolveToOffset( timecontext_t aContext, sInt16 &aMinuteOffset, lineartime_t aRefTime, bool aRefTimeUTC, GZones* g )
{
  bool ok = TzResolveContext(aContext, aRefTime, aRefTimeUTC, g);
  if (ok) {
    aMinuteOffset = TCTX_MINOFFSET(aContext);
  }
  return ok;
} // TzResolveToOffset



/*! Offset between two contexts (in seconds)
 *  Complex time zones (type "$") can't be currently resolved, they return false
 *  @param[in] aSourceValue   : reference time for which the offset should be calculated
 *  @param[in] aSourceContext : source time zone context
 *  @param[in] aTargetContext : source time zone context
 *  @param[out] sDiff         : receives offset east of UTC in seconds
 */
bool TzOffsetSeconds( lineartime_t aSourceValue, timecontext_t aSourceContext,
                                                 timecontext_t aTargetContext,
                                                 sInt32        &sDiff, GZones* g,
                                                 timecontext_t aDefaultContext )
{
  bool         sSys,  tSys,
               sUnk,  tUnk;
  sInt32       sOffs= 0, tOffs= 0; // initialize them for sure
  lineartime_t aTargetValue;

  bool      ok= true;
  bool   isDbg= false;
  if (g) isDbg= g->isDbg;

  // set default context for unknown zones
  if (TCTX_IS_UNKNOWN(aSourceContext)) aSourceContext=aDefaultContext;
  if (TCTX_IS_UNKNOWN(aTargetContext)) aTargetContext=aDefaultContext;

  do {
    sDiff= 0;
    if (aSourceContext==aTargetContext) break; /* no conversion, if identical context */

    /* both unknown or system is still ok */
    if   (TCTX_IS_UNKNOWN( aSourceContext ) &&
          TCTX_IS_UNKNOWN( aTargetContext )) break;
    sSys= TCTX_IS_SYSTEM ( aSourceContext );
    tSys= TCTX_IS_SYSTEM ( aTargetContext ); if (sSys && tSys) break;

    /* calculate specifically for the system's time zone */
    if (sSys)   MyContext( aSourceContext, g );
    if (tSys)   MyContext( aTargetContext, g );

    /* if both are unknown now, then it's good as well */
    sUnk= TCTX_IS_UNKNOWN( aSourceContext );
    tUnk= TCTX_IS_UNKNOWN( aTargetContext ); if (sUnk && tUnk)              break;
    /* this case can't be resolved: */       if (sUnk || tUnk) { ok= false; break; }

    if (IdenticalRules( aSourceContext,aTargetContext, g )) break;

    /* now do the "hard" things */
    if (!TimeZoneToOffs( aSourceValue, aSourceContext, false, sOffs, g )) return false;
                         aTargetValue= aSourceValue
                                     - (lineartime_t)(sOffs*SecsPerMin)*secondToLinearTimeFactor;
    if (!TimeZoneToOffs( aTargetValue, aTargetContext,  true, tOffs, g )) return false;

    sDiff= ( tOffs - sOffs )*SecsPerMin;
  } while (false);

  if (isDbg) {
    PNCDEBUGPRINTFX( DBG_SESSION,( "sSys=%d tSys=%d / sUnk=%d tUnk=%d / sOffs=%d tOffs=%d",
                                    sSys,tSys, sUnk,tUnk, sOffs,tOffs ));
    PNCDEBUGPRINTFX( DBG_SESSION,( "TzOffsetSeconds=%d", sDiff ));
  } // if

  return ok;
} /* TzOffsetSeconds */


/*! Converts timestamp value from one zone to another
 *  Complex time zones (type "$") can't be currently resolved, they return false
 *  @param[in/out] aValue     : will be converted from source context to target context. If==noLinearTime==0, no conversion is done
 *  @param[in] aSourceContext : source time zone context
 *  @param[in] aTargetContext : source time zone context
 *  @param[in] aDefaultContext: default context to use if source or target is TCTX_UNKNOWN
 */
bool TzConvertTimestamp( lineartime_t &aValue,   timecontext_t aSourceContext,
                                                 timecontext_t aTargetContext,
                                                                       GZones* g,
                                                 timecontext_t aDefaultContext )
{
  sInt32 sdiff;
  if (aValue==noLinearTime) return true; // no time, don't convert
  bool ok = TzOffsetSeconds( aValue, aSourceContext, aTargetContext, sdiff, g, aDefaultContext );
  if (ok)
    aValue += (lineartime_t)(sdiff)*secondToLinearTimeFactor;
  return ok;
} /* TzConvertTimestamp */


} // namespace sysync


/* eof */

