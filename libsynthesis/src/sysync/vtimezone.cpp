/*
 *  File:         vtimezone.cpp
 *
 *  Author:       Beat Forster (bfo@synthesis.ch)
 *
 *  Parser/Generator routines for vTimezone
 *
 *  Copyright (c) 2006-2011 by Synthesis AG + plan44.ch
 *
 *  2006-03-06 : bfo : created from exctracts from rrules.cpp
 *
 */


// includes
#include "prefix_file.h"

// do not import the whole thing to make life easier for standalone apps
#ifdef FULLY_STANDALONE
  #include "sysync_globs.h"
  #include "sysync_debug.h"
#else
  #include "sysync.h"
#endif

#include "vtimezone.h"
#include "rrules.h"
#include "lineartime.h"
#include "timezones.h"
#include "iso8601.h"
#include "stringutils.h"


typedef unsigned long ulong;


using namespace sysync;

namespace sysync {


struct RType {          // RRule block definition
         char           freq;
         char           freqmod;
         sInt16         interval;
         fieldinteger_t firstmask;
         fieldinteger_t lastmask;
         lineartime_t   until;
       }; // RType

#ifdef SYSYNC_TOOL
/*! Get boolean as const char string */
static cAppCharP Bo( bool b )
{
  if (b) return "true";
  else   return "false";
} // Bo
#endif



/*! Logging RType info */
static void RTypeInfo( RType &r, bool ok, TDebugLogger* aLogP )
{
  string                       runt;
  TimestampToISO8601Str( runt, r.until, TCTX_UTC); // get end time as string

  #ifdef SYSYNC_TOOL
    ulong* f1= (ulong*)&r.firstmask;   // prepare for longlong disp
    ulong* f2= f1++;
    ulong* l1= (ulong*)&r.lastmask;
    ulong* l2= l1++;

    LOGDEBUGPRINTFX( aLogP, DBG_GEN,( "RType ok=%s freq=%c freqmod=%c iv=%d",
                                         Bo( ok ), r.freq, r.freqmod, r.interval ) );
    LOGDEBUGPRINTFX( aLogP, DBG_GEN,( "RType 1st=%08X %08X last=%08X %08X '%s'",
                                            *f1,*f2, *l1,*l2, runt.c_str()) );
  #endif
} // RTypeInfo



/*! VTIMEZONE keywords */
const char* VTZ_ID    = "TZID";
const char* VTZ_BEGIN = "BEGIN";
const char* VTZ_END   = "END";
const char* VTZ_STD   = "STANDARD";
const char* VTZ_DST   = "DAYLIGHT";
const char* VTZ_RR    = "RRULE";
const char* VTZ_START = "DTSTART";
const char* VTZ_OFROM = "TZOFFSETFROM";
const char* VTZ_OTO   = "TZOFFSETTO";
const char* VTZ_NAME  = "TZNAME";

/*!
  * escapes newline ('\n'), comma, semicolon and backslash itself with backslash,
  * according to RFC 2445 "4.3.1 Text" value definition
  */
static string escapeText(const string &str)
{
  string res;
  res.reserve(str.size() * 110 / 100);

  for (size_t i = 0; i < str.size(); i++) {
    switch (str[i]) {
    case '\n':
      res += "\\n";
      break;
    case '\\':
    case ',':
    case ';':
      res += '\\';
      // no break!
    default:
      res += str[i];
      break;
    }
  }

  return res;
}

/*!
 * reverses escapeText(); in addition, accepts arbitrary characters
 * after backslash and replaces them with that character
 */
static string unescapeText(const string &str)
{
  string res;
  res.reserve(str.size());

  for (size_t i = 0; i < str.size(); i++) {
    switch (str[i]) {
    case '\\':
      i++;
      if (i < str.size()) {
        switch (str[i]) {
        case 'n':
          res += '\n';
          break;
        default:
          res += str[i];
          break;
        }
      }
      break;
    default:
      res += str[i];
      break;
    }
  }

  return res;
}

/*! RRULE2toInternal, using RType
 *  Converts  vCalendar 2.0 RRULE string into internal recurrence representation
 */
static bool RRULE2toInternalR( const char*   aText,   // RRULE string to be parsed
                               lineartime_t* dtstartP, // reference date for parsing RRULE (might be modified!)
                               RType         &r,
                               TDebugLogger* aLogP )
{
  timecontext_t untilcontext = TCTX_UNKNOWN;
  bool ok= RRULE2toInternal( aText, *dtstartP, TCTX_UNKNOWN,
                             r.freq,r.freqmod,r.interval,r.firstmask,r.lastmask,r.until,untilcontext,
                               aLogP, dtstartP );
  if (aLogP) RTypeInfo( r, ok, aLogP );
  return ok;
} // RRULE2toInternalR


/*! internalToRRULE2, using RType
 *  Converts internal recurrence into vCalendar 2.0 RRULE string
 */
static bool internalRToRRULE2( string        &aString, // receives RRULE string
                               RType         r,
                               bool          asUTC,
                               TDebugLogger* aLogP )
{
  bool ok= internalToRRULE2( aString,
                             r.freq,r.freqmod,r.interval,r.firstmask,r.lastmask,r.until,
                             asUTC, aLogP );
  #ifdef SYSYNC_TOOL
    if (aLogP)
      LOGDEBUGPRINTFX( aLogP, DBG_GEN,( "RType ok=%s aString='%s'", Bo( ok ),
                                                     aString.c_str() ) );
  #endif

  return ok;
} // internalRtoRRULE2



// ----------------------------------------------------------------------------------------
/*! Convert mask values into <aDayOfWeek>,<aNth> */
static void R_to_WD( RType &r, short &aDayOfWeek, short &aNth )
{
  int i,j;

  aDayOfWeek= -1;
  aNth      = -1;

  for   (i= 0;  i<WeeksOfMonth;  i++) {
    for (j= 0;  j<DaysOfWeek;    j++) {
      sInt16 index= i*DaysOfWeek + j;
      if (r.firstmask & ((uInt64)1<<index)) { aDayOfWeek= j; aNth= i+1; return; }
    } // for
  } // for

  for   (i= 0;  i<WeeksOfMonth;  i++) {
    for (j= 0;  j<DaysOfWeek;    j++) {
      sInt16 index= i*DaysOfWeek + j;
      if (r.lastmask  & ((uInt64)1<<index)) { aDayOfWeek= j; aNth= WeeksOfMonth-i; return; }
    } // for
  } // for
} // R_to_WD


/*! Convert <r>,<tm> into tChange <c> */
static void Rtm_to_tChange( RType &r, lineartime_t tim, tChange &c )
{
  sInt16 mo, da, ho, mi;
  lineartime2date(tim, NULL, &mo, &da);
  lineartime2time(tim, &ho,  &mi, NULL, NULL);

              c.wMonth = mo;
              c.wHour  = ho;
              c.wMinute= mi;
  R_to_WD( r, c.wDayOfWeek,     c.wNth );
  if         (c.wDayOfWeek==-1) c.wNth= da;
} // Rtm_to_tChange



// ----------------------------------------------------------------------------------------
/*! Convert <aDayOfWeek>,<aNth> into mask values */
static void WD_to_R( int aDayOfWeek, int aNth, RType &r )
{
  r.firstmask = 0; // default values
  r.lastmask  = 0;
  sInt16 index= aDayOfWeek % DaysOfWeek;

  bool last= aNth==WeeksOfMonth; // 5 is the last week
  if  (last) r.lastmask = (uInt64)1<<  index;
  else       r.firstmask= (uInt64)1<<( index + (aNth-1)*DaysOfWeek );
} // WD_to_R


static void AdaptDay( lineartime_t &tim, tChange c )
{
  sInt16 yr, mo, dowk;

  lineartime2date( tim, &yr ,&mo, NULL );
  tim = date2lineartime( yr,  mo, 1    ); // make calculation with the 1st of month
  dowk= lineartime2weekday ( tim );       // get weekday

  sInt16     da= c.wDayOfWeek - dowk + 1;
  if (da<1)  da+= DaysOfWeek; // this is the 1st occurance of the given weekday
  da+= (c.wNth-1)*DaysOfWeek;

  AdjustDay( da,mo,yr ); // and ajust <d> within the month
  tim= date2lineartime( yr,mo,da );
} // AdaptDay


/*! Convert <r>,<tm> into tChange <c> */
static void tChange_to_Rtm( tChange c, int year, RType &r, lineartime_t &tim )
{
  bool noDOW = c.wDayOfWeek==-1;
  bool noCalc= c.wMonth==0;
  if  (noCalc) c.wMonth= 1;

  sInt16     da= 1;
  if (noDOW) da= c.wNth;

               tim = date2lineartime( year,    c.wMonth,  da  );
  lineartime_t timT= time2lineartime( c.wHour, c.wMinute, 0,0 );

  if (!(noCalc || noDOW)) {
    AdaptDay( tim, c ); // get wDayOfWeek/wNth, fitting for <year>
    WD_to_R ( c.wDayOfWeek, c.wNth, r );
  } // if

  tim+= timT; // don't forget time
} // tChange_to_Rtm



// ----------------------------------------------------------------------------------------
/*! Get the <bias> value from TZOFFSFROM/TZOFFSTO strings */
static bool Get_Bias( string of, string ot, short &bias )
{
  bool negative= ot.find( "-",0 )==0;
  bool positive= ot.find( "+",0 )==0; // needed because positive TZOFFSxxx should have it as per specs
  if  (negative || positive) ot= ot.substr( 1,ot.length()-1);

  string h= ot.substr( 0,2 );
  string m= ot.substr( 2,2 );

  bias= atoi( h.c_str() )*MinsPerHour + atoi( m.c_str() );
  if (negative) bias= -bias;
  return true;
} // Get_Bias


/*! Fill in the TZ info.
 * @return false if some information was found, but couldn't be extracted;
 *         true if not found (in which case c, cBias, cName are unchanged)
 *         or found and extracted
 */
static bool GetTZInfo( cAppCharP     aText,
                       cAppCharP     aIdent,
                       tChange      &c,
                       short        &cBias,
                       string       &cName,
                       sInt32        aNth, // take nth occurance, -1: take last
                       TDebugLogger* aLogP )
{
  RType         r;
  timecontext_t tctx;
  lineartime_t  dtstart, dtH= 0;
  string        a, st;
  bool          success = true;

  if (aNth==-1) { // search for the last (in time)
    sInt32 i= 1;

    while (true) {
                  a= VStr( aText, aIdent, i );  if (a=="") break;
      st= VValue( a, VTZ_START );    //            - start time
      if (ISO8601StrToTimestamp( st.c_str(), dtstart, tctx )==0) break;
      if (dtH<dtstart) { dtH= dtstart; aNth= i; } // the latest one with index

      i++;
    } // while
  } // if

  a= VStr( aText, aIdent, aNth );
  if ( a.empty() ) {
    // Happens for VTIMEZONEs without summer saving time when this
    // function is called to extract changes for DAYLIGHT. Don't
    // treat this as failure and continue with clean change rules, as
    // before.
    return success;
  }

  string rr= VValue( a, VTZ_RR    ); // sub items: - RRULE
         st= VValue( a, VTZ_START ); //            - start time
  string of= VValue( a, VTZ_OFROM ); //            - tz offset from
  string ot= VValue( a, VTZ_OTO   ); //            - tz offset to
      cName= unescapeText( VValue( a, VTZ_NAME  ) ); // - tz name

  if (ISO8601StrToTimestamp( st.c_str(), dtstart, tctx )==0)  return false;
  if (!Get_Bias( of,ot, cBias ))                              return false;

  if ( rr.empty() ) {
    // Happens when parsing STANDARD part of VTIMEZONE
    // without DAYLIGHT saving.
  } else if (RRULE2toInternalR    ( rr.c_str(), &dtstart, r, aLogP )) {
    // Note: dtstart might have been adjusted by this call in case of DTSTART not meeting a occurrence for given RRULE
    string             vvv;
    internalRToRRULE2( vvv, r, false, aLogP );
    Rtm_to_tChange        ( r, dtstart, c );
  } else {
    success = false;
  } // if

  return success;
} // GetTZInfo


/*! Create a property string */
static string Property( const string &propertyName, const string &value ) {
  return propertyName + ":" + value + "\n";
} // Property

/*! Find a Property inside another string. If the property string ends
  in \n, then that character matches arbitrary line endings (\r, \n,
  end of string).  If the property does not end in \n, a normal string
  search is done. */
static string::size_type FindProperty( const string &aText, const string &aProperty, string::size_type aOffset = 0 ) {
  if (!aProperty.empty() && aProperty[aProperty.size()-1] == '\n') {
    // ignore trailing \n, check for either \r or \n in aText instead
    string::size_type offset = aOffset;
    string::size_type textlen = aProperty.size() - 1;
    while (true) {
      string::size_type hit = aText.find(aProperty.c_str(), offset, textlen);
      if (hit == string::npos)
        return string::npos;

      // prefix match, now must check for line end
      if (hit + textlen >= aText.size()) {
        // end of string is okay
        return hit;
      }

      char eol = aText[hit + textlen];
      switch (eol) {
      case '\r':
      case '\n':
        // found it
        return hit;
      }
      // keep searching
      offset = hit + 1;
    }
  } else {
    // normal string search
    return aText.find(aProperty, aOffset);
  }
} // FindProperty

/*! Find last instance of Property inside another string. If the
  property string ends in \n, then that character matches arbitrary
  line endings (\r, \n, end of string).  If the property does not end
  in \n, a normal string search is done. */
static string::size_type RfindProperty( const string &aText, const string &aProperty, string::size_type aOffset = string::npos ) {
  if (!aProperty.empty() && aProperty[aProperty.size()-1] == '\n') {
    // ignore trailing \n, check for either \r or \n in aText instead
    string::size_type offset = aOffset;
    string::size_type textlen = aProperty.size() - 1;
    while (true) {
      string::size_type hit = aText.rfind(aProperty.c_str(), offset, textlen);
      if (hit == string::npos)
        return string::npos;

      // prefix match, now must check for line end
      if (hit + textlen >= aText.size()) {
        // end of string is okay
        return hit;
      }

      char eol = aText[hit + textlen];
      switch (eol) {
      case '\r':
      case '\n':
        // found it
        return hit;
      }
      // keep searching, if possible
      if (hit == 0)
        return string::npos;
      offset = hit - 1;
    }
  } else {
    // normal reversed string search
    return aText.rfind(aProperty, aOffset);
  }
} // RfindProperty

/*! Check, if "BEGIN:value" is available only once */
static int PMulti( string &aText, string value )
{
  string             p= Property( VTZ_BEGIN, value );
  string::size_type n;
  n= FindProperty( aText, p ); if (n==string::npos) return 0;
  n= FindProperty( aText, p, n+1 ); if (n==string::npos) return 1;
  /* else */                                        return 2;
} // PMulti


/* vTimezones with more than one STANDARD or DAYLIGHT sequence can't be resolved */
static void MultipleSeq( string aText, int &s, int &d )
{
  s= PMulti( aText,VTZ_STD );
  d= PMulti( aText,VTZ_DST );
} // MultipleSeq


bool VTIMEZONEtoTZEntry( const char*    aText, // VTIMEZONE string to be parsed
                         tz_entry      &t,
                         TDebugLogger*  aLogP)
{
  short dBias; // the full bias for DST
  bool success = true;

                                 t.name   = "";
                                 t.ident  = "";
                                 t.dynYear= "CUR";
                                 t.biasDST= 0;
                                 t.bias   = 0;
                                 t.stdName =
                                   t.dstName = "";
  if (!GetTZInfo( aText,VTZ_STD, t.std, t.bias, t.stdName, -1, aLogP )) {
    success = false;
  }
  // default value if not found (which is treated as success by GetTZInfo)
  dBias= t.bias;
  if (!GetTZInfo( aText,VTZ_DST, t.dst,  dBias, t.dstName, -1, aLogP )) {
    // unknown failure, better restore default
    dBias= t.bias;
    success = false;
  }

  if  (t.bias ==  dBias) ClrDST( t ); // no DST ?
  else t.biasDST= dBias - t.bias; // t.biasDST WILL be calculated here

  // get TZID as found in VTIMEZONE
  t.name = unescapeText( VValue( aText, VTZ_ID ) );

  return success;
} // VTIMEZONEtoTZEntry


/*! Convert VTIMEZONE string ito internal context value */
bool VTIMEZONEtoInternal( const char*    aText, // VTIMEZONE string to be parsed
                          timecontext_t &aContext,
                          GZones*        g,
                          TDebugLogger*  aLogP,
                          string*        aTzidP )  ///< if not NULL, receives TZID as found in VTIMEZONE
{
  aContext= tctx_tz_unknown;

  tz_entry      t;
  string        lName;
  timecontext_t lContext;

  if (!VTIMEZONEtoTZEntry( aText, t, aLogP )) {
    PLOGDEBUGPRINTFX(aLogP, DBG_PARSE+DBG_ERROR, ("parsing VTIMEZONE failed:\n%s", aText));
  }
  // Telling the caller about the original TZID is necessary because this
  // code might match that TZID against an existing definition with a different
  // TZID. Previously it was also necessary when importing the VTIMEZONE, because
  // the original TZID was overwritten. Now imported definitions retain the
  // original TZID in t.name.
  if (aTzidP) *aTzidP = t.name; // return the original TZID as found, needed to match with TZID occurences in rest of vCalendar

  bool ok = true;
  bool okM= true;
  int                 s,d;
  MultipleSeq( aText, s,d );

  if  (s==0 && d==0) return false;
  okM= s<=1 && d<=1; // test if more than one section

  // find best match for VTIMEZONE: checks name and rules
  // allows multiple timezone, if last is ok !
  if (!g) return false; // avoid crashes with g==NULL
  ok=  g->matchTZ(t, aLogP, aContext);

  if (!ok && !okM) { // store it "as is" if both is not ok
    ClrDST( t );
    t.name = aText;
    t.ident= "$";
    t.bias = 0;
    return FoundTZ( t, lName, aContext, g, true );
  } // if

  #ifdef SYSYNC_TOOL
  if (ok) {
    string existing_name;
    TimeZoneContextToName(aContext, existing_name, g);
    LOGDEBUGPRINTFX( aLogP, DBG_PARSE,( "found matching time zone with name='%s' tx=%08X %d",
                                        existing_name.c_str(),   aContext,
                                        TCTX_OFFSCONTEXT( aContext ) ) );
  }
  #endif

  // if not found, then try again with name and add
  // the entry; doing a full comparison again is
  // redundant here, but there is no other way to
  // add the entry
  string new_name;

  if (!ok) ok= FoundTZ( t, new_name, aContext, g, true );

  if  (ok && t.std.wMonth!=0 &&
             t.dst.wMonth!=0) {
    tz_entry std;
    std.name = t.stdName;
    std.ident= "s"; // standard
    std.bias = t.bias;
    FoundTZ( std, lName,lContext, g, true );

    tz_entry dst;
    dst.name = t.dstName;
    dst.ident= "d"; // daylight saving
    dst.bias = t.bias + t.biasDST;
    FoundTZ( dst, lName,lContext, g, true );
  } // if

  return ok;
} // VTIMEZONEtoInternal


// -----------------------------------------------------------------------------------------
/*! Create a string with <txt> in-between BEGIN:<value> .. END:<value> */
static string Encapsuled( string value, string txt )
{
  return Property( VTZ_BEGIN, value ) + txt +
         Property( VTZ_END,   value );
} // Encapsuled


/*! Get the hour/minute string of <bias> */
string HourMinStr( int bias )
{
  const char*  form;
  if (bias>=0) form= "%+03d%02d";
  else       { form= "-%02d%02d"; bias= -bias; } // for negative values

  char     s[ 10 ];
  sprintf( s, form, bias / MinsPerHour,
                    bias % MinsPerHour );
  return   s;
} // HourMinStr


/*! Get TZOFFSETFROM/TO strings */
static string FromTo( int biasFrom, int biasTo )
{
  return Property( VTZ_OFROM, HourMinStr( biasFrom ) ) +
         Property( VTZ_OTO,   HourMinStr( biasTo   ) );
} // FromTo


/*! Generate a TZ info:
 *   - <t>      : the whole tz entry
 *   - <value>  : STANDARD/DAYLIGHT
 *   - <aIdent> : "s"/"d"
 *   - <c>      : std / dst info
 *   - <y>      : staring year
 *   - <aFrom>  : bias before changing
 *   - <aTo>    :   "  after      "
 *   - <aLogP>  : the debug logger
 */
static string GenerateTZInfo( tz_entry t,
                              const char* value, const char* aIdent,
                              tChange c, int y,
                              int aFrom, int aTo,
                              GZones *g,
                              TDebugLogger* aLogP )
{
  string        rTxt, dtStart;
  RType         r;
  lineartime_t  tim;
  string        aName;
  timecontext_t aContext;
  bool withDST= strcmp( aIdent," " )!=0;

  r.freq     = 'M'; // these are the fixed parameters for TZ
  r.freqmod  = 'W';
  r.interval = 12;
  r.until    = noLinearTime; // last day
  r.firstmask=  0; // default values
  r.lastmask =  0;

  tChange_to_Rtm( c,y, r, tim );
  TimestampToISO8601Str( dtStart, tim, TCTX_UNKNOWN); // with time, but no offset

  if (withDST) internalRToRRULE2( rTxt, r, false, aLogP );

                t.name = "";
                t.ident= aIdent;
                t.bias = aTo; // this is the bias to search for STD/DST name
  if (!FoundTZ( t, aName, aContext, g )) aName= "";

  string       s = Property( VTZ_START, dtStart   );
  if (withDST) s+= Property( VTZ_RR,    rTxt      );
               s+= FromTo             ( aFrom,aTo );
  if (withDST &&
     !aName.empty()) s+= Property( VTZ_NAME, escapeText( aName ) );

  return Encapsuled( value, s );
} // GenerateTZInfo


/*! Convert internal context value into VTIMEZONE */
bool internalToVTIMEZONE( timecontext_t  aContext,
                          string        &aText, // receives VTIMEZONE string
                          GZones*        g,
                          TDebugLogger*  aLogP,
                          sInt32         testYear,
                          sInt32         untilYear,
                          cAppCharP      aPrefIdent )
{
  // %%% note: untilYear needs to be implemented, is without functionality so far

  sInt16              yy= testYear;
  if (testYear==0)    yy= MyYear( g );
  TzResolveMetaContext( aContext, g ); // we need actual zone, not meta-context

  int       t_plus= 0;
  tz_entry  t, tp;
  cAppCharP id;
  bool      withDST= false;

  bool tzEnum= TCTX_IS_TZ( aContext );
  if  (tzEnum) {
    GetTZ( aContext, t, g, yy );
    withDST=         t.std.wMonth!=0 &&
                     t.dst.wMonth!=0;
    if              (t.ident == "$") { aText= t.name; return true; } // just give it back
  }
  else {
    t.bias= TCTX_MINOFFSET( aContext );
  //t.name= "OFFS" + HourMinStr( t.bias ); // will be done once at TimeZoneContextToName
  }
                 t_plus = t.bias;    id= " ";
  if (withDST) { t_plus+= t.biasDST; id= "s"; } // there is only an offset with DST

                   // time zone start year
  int y_std= 1967; // this info gets lost in the TZ_Entry system
  int y_dst= 1987; // hard coded, because there is currently no field to store it

  // modify it, if dyn year is later
  if      (!t.dynYear.empty()) {
    timecontext_t aDJ= aContext+1;
    GetTZ       ( aDJ, tp, g, -1 );

    // but only if not the first entry of dynYear
    if    (t.name == tp.name &&
           t.dynYear != tp.dynYear) {
      y_std= atoi( t.dynYear.c_str() );
      y_dst= y_std;
    } // if
  } // if

  // make sure we get the right TZID string according to aPrefIdent
  string tzn = t.name;
  if (aPrefIdent || !tzEnum) {
    TimeZoneContextToName( aContext, tzn, g, aPrefIdent );
  }

  // at least one STANDARD or DAYLIGHT info is mandatory
  aText=    Property      (    VTZ_ID,      escapeText( tzn ) ) +
            GenerateTZInfo( t, VTZ_STD, id, t.std, y_std, t_plus,t.bias, g, aLogP );

  if (withDST)
    aText+= GenerateTZInfo( t, VTZ_DST,"d", t.dst, y_dst, t.bias,t_plus, g, aLogP );

  return false;
} // internalToVTIMEZONE



static bool NextStr( string &s, string &nx )
{
  string::size_type i= s.find( ";", 0  );
  if (i==string::npos ) return false; // mismatch
  nx= s.substr ( 0,   i  );
  s = s.substr ( i+1, s.length()-i-1 );
  return true;
} // NextStr


/*! Convert TZ/DAYLIGHT string into internal context value */
bool TzDaylightToContext( const char*    aText,     ///< DAYLIGHT property value to be parsed
                          timecontext_t  aStdOffs,  ///< Standard (non-DST) offset obtained from TZ
                          timecontext_t &aContext,  ///< receives context
                          GZones*        g,
                          timecontext_t  aPreferredCtx, // preferred context, if rule matches more than one context
                          TDebugLogger*  aLog )
{
  TzResolveMetaContext( aPreferredCtx, g ); // we need actual zone, not meta-context
  string   s= aText;
  string   hrs, dst, std, l, r, rslt;
  string::size_type i;
  tz_entry t, tCopy;
  timecontext_t cc, ccFirst, ccSlash;

  aContext= aStdOffs; // as default, convert it into a enum TZ
  string    sSv;
  bool      dbg= false; // currently no debugging

  do {
    if (s=="FALSE"      ) { s= ""; break; } // no DST

    /*
    i=  s.find( ";", 0  );                  // TRUE
    if (i==string::npos ) return false;     // mismatch
    l=  s.substr( 0, i  );
    */
    if (!NextStr( s, l   )) return false;   // mismatch

    if   (l=="FALSE")     { s= ""; break; } // no DST
    if (!(l=="TRUE" ))      return false;   // either "TRUE" or "FALSE"

    if (!NextStr( s, hrs )) return false;   // mismatch
    if (!NextStr( s, dst )) return false;   // mismatch
    if (!NextStr( s, std )) return false;   // mismatch

    int minsDST= MinsPerHour; // %%% not yet perfect for Namibia !!

    /*
    i=  s.find( ";", 0   ); // +XX
    i=  s.find( ";", i+1 ); // DST time
    i=  s.find( ";", i+1 ); // STD time

    if (i==string::npos ) return false; // parsing error

    s=  s.substr( i+1, s.length()-i-1 );
    */

    ISO8601StrToContext( hrs.c_str(), cc );
    int mins=         TCTX_MINOFFSET( cc )-minsDST;

    i=           s.find  ( ";", 0   );
    l=           s.substr( 0,              i   );
    r=           s.substr( i+1, s.length()-i-1 );
    if (l==r)    s= l; // twice the same => take it once
    StringSubst( s, ";", "/" );

    TimeZoneNameToContext( s.c_str(), aContext, g );
    sSv= s; // make a copy for Olson name test later

    // if it perfectly fits to a named zone, take it
    if (GetTZ( std,dst, mins,minsDST, t, g )) { ccFirst= TCTX_UNKNOWN; // start with these defaults
                                                ccSlash= TCTX_UNKNOWN;
                                                cc     = TCTX_SYSTEM;
      if   (dbg) printf( "lv1 s='%s' %d pref=%d\n", s.c_str(), aContext, aPreferredCtx );
      while   (FoundTZ( t, rslt, cc, g, false,  cc )) {
        if (dbg) printf( "lv1 rslt='%s'\n", rslt.c_str() );
        if (s==rslt) { aContext= cc; break; }
        if (s.empty()) {
          if (ccFirst==TCTX_UNKNOWN) ccFirst= cc;
          if (ccSlash==TCTX_UNKNOWN) {
                i= rslt.find( "/",0 ); // a slash TZ would be the best choice
            if (i!=0 && i!=string::npos) ccSlash= cc;
          } // if
        } // if
      } // while

      if      (ccSlash!=TCTX_UNKNOWN) aContext= ccSlash;
      else if (ccFirst!=TCTX_UNKNOWN) aContext= ccFirst;
    } // if
  } while (false);

  // check if it fits to <aPreferredCtx>
  bool          pUnk       = TCTX_IS_UNKNOWN ( aPreferredCtx );
  timecontext_t t_Greenwich= TCTX_ENUMCONTEXT( tctx_tz_Greenwich );

  ccFirst= TCTX_UNKNOWN; // start with these defaults
  cc     = TCTX_SYSTEM;
  tCopy= t; // make the copy before

  if (GetTZ( aContext, t,        g )) {
    if   (dbg) printf( "lv2 s='%s' %d\n", s.c_str(), aContext );
    if       (FoundTZ( t, s, cc, g, false, cc )) { // search by correct name first
      if (dbg) printf( "lv2 s='%s' / loc='%s'\n", s.c_str(), t.location.c_str() );
      if (!(cc!=aPreferredCtx && cc==t_Greenwich)) { // take UTC for this case
        if (pUnk || cc==aPreferredCtx) { aContext= cc; return true; }
        if (ccFirst==TCTX_UNKNOWN)        ccFirst= cc; // keep it, just in case
      } // if
    } // if
  }
  else {
    t.bias   = TCTX_MINOFFSET( aContext );
    t.biasDST= 0; // no DST offset
    t.dynYear= "";
    ClrDST( t );
    tCopy=  t;
  } // if

                 //tCopy= t;
                   tCopy.name = ""; // make more generic comparison
                   tCopy.ident= "";        cc= TCTX_SYSTEM;
  while  (FoundTZ( tCopy, s, cc, g, false, cc )) {
    if (dbg) printf( "lv3 s='%s' loc='%s' cc=%d\n", s.c_str(), tCopy.location.c_str(), cc );
    if (!(cc!=aPreferredCtx && cc==t_Greenwich)) { // take UTC for this case
      if (pUnk || cc==aPreferredCtx) {  ccFirst= cc; break; }
      if (ccFirst==TCTX_UNKNOWN)        ccFirst= cc; // keep it, just in case
    } // if
  } // while

  aContext= ccFirst;
  bool ok= !TCTX_IS_UNKNOWN( aContext );
  if (dbg) printf( "aContext=%d ok=%d\n", aContext, ok );

  // if not ok, try to use Olson names
  if (!ok) {               s= sSv;
    TimeZoneNameToContext( s.c_str(), aContext, g, true );
    if (dbg) printf( "lv4 s='%s' aContext=%d\n", s.c_str(), aContext );
    ok= !TCTX_IS_UNKNOWN( aContext );
  }

  if (dbg) printf( "aContext=%d ok=%d\n", aContext, ok );
  return ok;
} // TzDaylightToContext



/*! Create DAYLIGHT string from context for a given sample time(year) */
bool ContextToTzDaylight( timecontext_t  aContext,
                          lineartime_t   aSampleTime, ///< specifies the time after which we search DST
                          string        &aText,       ///< receives DAYLIGHT string
                          timecontext_t &aStdOffs,    ///< receives standard (non-DST) offset for TZ
                          GZones*        g,
                          TDebugLogger*  aLog )
{
//#ifdef RELEASE_VERSION
  //#error "%%%missing actual implementation - this is just a q&d dummy for testing"
//#endif
  TzResolveMetaContext( aContext, g ); // we need actual zone, not meta-context

  sInt16                         year, month, day;
  lineartime2date( aSampleTime, &year,&month,&day ); // we need the active year
  aStdOffs= 0; // default

  tz_entry     t, tCopy;
  bool         found= false;
  bool         dDone= false;
  bool         sDone= false;
  lineartime_t dt = 0, st = 0;
  string       s;

  do {
    bool ok= GetTZ( aContext, t, g );
    if (!ok) return false;

    aStdOffs= TCTX_OFFSCONTEXT( t.bias ); // need this here in case of no DST

         ok= DSTCond( t );
    if (!ok) { aText= "FALSE"; return true; }

    const int         FLen= 15;
    char           f[ FLen ];
    sprintf      ( f, "%d", year );
    string     yy= f;
    t.dynYear= yy.c_str();

    // go further only in case of DST available
        s= t.name;
    if (s.find( "/",0 )==string::npos) { // search for a time zone with slash in it
      tCopy= t;
      tCopy.name = ""; // make more generic comparison
      tCopy.ident= "";

      timecontext_t                           cc= TCTX_UNKNOWN;
      while (FoundTZ( tCopy, s, cc, g, false, cc )) {
        // search for a time zone with slash in it (which is
        // interpreted as separator between dstName and stdName) *or*
        // one which has dstName and stdName set explicitly;
        // prefer explicit names over splitting name
        tz_entry zone;
        if (GetTZ( cc, zone, g, -1 ) &&
            !zone.stdName.empty() &&
            !zone.dstName.empty()) {
          s = zone.stdName + ";" + zone.dstName;
          found= true; break;
        } else if (s.find( "/",0 )!=string::npos) {
          // Assumption here is that s contains exactly one slash,
          // otherwise s is not valid for stdName;dstName in vCalendar
          // 1.0.
          StringSubst( s, "/", ";" );
          found= true; break;
        } // if
      } // while

      if (!found) {
        string stdName;
        if (t.stdName.empty()) {
          stdName = t.name;
        } else {
          stdName = t.stdName;
        }
        s = stdName; // create a <x>;<x> string
        s+= ";";
        s+= stdName;
      } // if
    } else if (!t.stdName.empty() && !t.dstName.empty()) {
      // use explicit zone names instead of splitting s
      s = t.stdName + ";" + t.dstName;
    } else {
      StringSubst( s, "/", ";" );
    }

    if (!dDone) dt= DST_Switch( t, t.bias, year, true  ); // get the switch time/date for DST
    if (!sDone) st= DST_Switch( t, t.bias, year, false ); // get the switch time/date for STD

    // search into future, <dt> must be earlier than <st>
    if (dt>aSampleTime         ) dDone= true;
    if (st>aSampleTime && st>dt) sDone= true;

    year++; // search in next year
  } while (!dDone || !sDone);

//lineartime_t lt= seconds2lineartime( t.bias * SecsPerMin );
  string                     offs;
  timecontext_t                    tc= TCTX_OFFSCONTEXT( t.bias+t.biasDST );
  ContextToISO8601StrAppend( offs, tc, false );

  // now concatenate the result string
                         aText= "TRUE;";
  StringObjAppendPrintf( aText, "%s;", offs.c_str() );
//StringObjAppendPrintf( aText, "%+03d;", t.bias / MinsPerHour );

  string                 dstStr;
  TimestampToISO8601Str( dstStr, dt, TCTX_UTC );
  string                 stdStr;
  TimestampToISO8601Str( stdStr, st, TCTX_UTC );

  // s must be of the format stdName;dstName here, for example CET;CEST or PST;PDT
  aText+= dstStr + ";" + stdStr + ";" + s;

  return true;
} // ContextToTzDaylight




// -----------------------------------------------------------------------------------------
// Get sequence between <bv> and <ev>. If these strings end in \n, then that character
// matches arbitrary line endings (in other words, \n, \r, end of string).
static string PeeledStr( const string &aStr, const string &bv, const string &ev, sInt32 aNth )
{
  string::size_type bp= 0;

  if (aNth==-1) {
    bp= RfindProperty( aStr, bv ); if (bp==string::npos) return "";
  }
  else {
    sInt32 i= 1;

    while (true) {
      bp= FindProperty( aStr, bv, bp ); if (bp==string::npos) return "";
      if (i>=aNth) break;
      i++; bp++;
    } // while
  } // if

//string::size_type bp= aStr.find( bv, 0 ); if (bp==string::npos) return "";
  string::size_type ep= FindProperty( aStr, ev, bp ); if (ep==string::npos) return "";

  string::size_type bpl= bp + bv.length();
  return aStr.substr( bpl, ep - bpl );
} // PeeledStr


// Get the string between "BEGIN:<value>[line end]" and "END:<value>[line end]"
string  VStr( const string &aStr, const string &value, sInt32 aNth ) {
  return PeeledStr( aStr, Property( VTZ_BEGIN, value ),
                          Property( VTZ_END,   value ), aNth );
} // VStr


// Get the value string between "<field>:" and "\r?\n"
string VValue( const string &aStr, const string &key ) {
  string res = PeeledStr( aStr, key + ":", "\n", 1 );

  // strip optional trailing \r
  if (!res.empty() && res[res.size() - 1] == '\r')
    return res.substr(0, res.size() - 1);
  else
    return res;
} // VValue


} // namespace sysync

/* eof */
