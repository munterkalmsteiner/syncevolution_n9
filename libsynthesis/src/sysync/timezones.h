/*
 *  File:         timezones.h
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

#ifndef TIMEZONES_H
#define TIMEZONES_H

#include <string>
#include <list>
#include <vector>

#include "lineartime.h"
#include "debuglogger.h"

#ifdef MUTEX_SUPPORT
  #include "platform_mutex.h"
#endif


using namespace std; // for string and list

namespace sysync {

// include the built-in time zone table
#ifndef NO_BUILTIN_TZ
  #include "tz_table.h"
#endif

/// time zone table entry definition
typedef struct tChangeStruct {
          short wMonth;
          short wDayOfWeek;
          short wNth;      // nth occurance
          short wHour;
          short wMinute;

          tChangeStruct() :
            wMonth(0),
            wDayOfWeek(0),
            wNth(0),
            wHour(0),
            wMinute(0)
          {}

          tChangeStruct(short aMonth,
                        short aDayOfWeek,
                        short aNth,
                        short aHour,
                        short aMinute) :
            wMonth(aMonth),
            wDayOfWeek(aDayOfWeek),
            wNth(aNth),
            wHour(aHour),
            wMinute(aMinute)
            {}
        } tChange;


class tz_entry {
        public:
          std::string name;          /**< name, same as TZID in
                                        VTIMEZONE, e.g. CET/CEST or
                                        /softwarestudio.org/Tzfile/Europe/Berlin;
                                        see also dst/stdName */
          std::string stdName;       /**< optional standard time name, e.g. CEST; must be
                                        set if "name" is not a concatenation of
                                        standard and daylight name (CET/CEST);
                                        the vCalendar 1.0 code relies on that */
          std::string dstName;       /**< used instead of splitting
                                        "name" if (and only if)
                                        stdName is set; may be empty
                                        in zones without daylight
                                        saving */
          std::string location;      /**< location string as used in Olson TZID, e.g. Europe/Berlin */
          short       bias;          /**< minutes difference to UTC (west negative, east positive */
          short       biasDST;       /**< minutes difference to bias (not UTC!) */
          std::string ident;         /**< see FoundTZ() */
          std::string dynYear;       /**< if this time zone rule is assigned to a specific year range */
          tChange     dst;           /**< describes when daylight saving time will become active */
          tChange     std;           /**< describes when standard time will become active */
          bool        groupEnd;      /**< true if the entry which follows this one belongs to a different zone or last entry */

          tz_entry() :
            bias(0),
            biasDST(0),
            groupEnd(true)
          {}

          tz_entry( const std::string &aName,
                    short              aBias,
                    short              aBiasDST,
                    const std::string &aIdent,
                    const std::string &aDynYear,
                    const tChange     &aDst,
                    const tChange     &aStd,
                    bool               aGroupEnd ) :
            name(aName),
            bias(aBias),
            biasDST(aBiasDST),
            ident(aIdent),
            dynYear(aDynYear),
            dst(aDst),
            std(aStd),
            groupEnd(aGroupEnd)
            {}
};





//const int DST_Bias= 60; // General DST offset (in minutes)

// Symbian timezone categories
// visible for all other systems as well
#ifndef __EPOC_OS__
  enum TDaylightSavingZone {
    EDstHome    =0x40000000,
    EDstNone    =0,
    EDstEuropean=1,
    EDstNorthern=2,
    EDstSouthern=4
  };
#endif


// Offset mask
const uInt32 TCTX_OFFSETMASK = 0x0000FFFF;

// time context flags
// - symbolic zone flag
const uInt32 TCTX_SYMBOLIC_TZ= 0x00010000;
// - rendering flags
const uInt32 TCTX_DATEONLY   = 0x00020000;
const uInt32 TCTX_TIMEONLY   = 0x00040000;
const uInt32 TCTX_DURATION   = 0x00080000;

const uInt32 TCTX_RFLAGMASK = TCTX_DATEONLY+TCTX_TIMEONLY+TCTX_DURATION;

//! Get signed minute offset
sInt16  TCTX_MINOFFSET( timecontext_t tctx );

//! Get time zone enum
TTimeZones TCTX_TZENUM( timecontext_t tctx );


// macro to get time zone context
#define TCTX_ENUMCONTEXT( tzenum ) ((timecontext_t)      ((tzenum) | TCTX_SYMBOLIC_TZ))
#define TCTX_UNKNOWN                           TCTX_ENUMCONTEXT( tctx_tz_unknown )
#define TCTX_SYSTEM                            TCTX_ENUMCONTEXT( tctx_tz_system  )
#define TCTX_UTC                               TCTX_ENUMCONTEXT( tctx_tz_UTC  )
#define TCTX_OFFSCONTEXT( offs )   ((timecontext_t)((sInt16)(offs) & TCTX_OFFSETMASK ))


/*
// macro to get time zone and other flags
#define TCTX_IS_TZ( tctx )         ((bool)(tctx  & TCTX_SYMBOLIC_TZ))
#define TCTX_IS_DATEONLY( tctx )   ((bool)(tctx  & TCTX_DATEONLY   ))
*/

//! Check if <tctx> is a symbolic TZ info
bool TCTX_IS_TZ      ( timecontext_t tctx );

// Check if <tctx> is a built-in symbolic TZ info
bool TCTX_IS_BUILTIN ( timecontext_t tctx );

//! Check if <tctx> has TCTX_DATEONLY mode set
bool TCTX_IS_DATEONLY( timecontext_t tctx );

//! Check if <tctx> has TCTX_TIMEONLY mode set
bool TCTX_IS_TIMEONLY( timecontext_t tctx );

//! Check if <tctx> has TCTX_DURATION mode set
bool TCTX_IS_DURATION( timecontext_t tctx );

//! Check if <tctx> is a unknown time zone
bool TCTX_IS_UNKNOWN ( timecontext_t tctx );

//! Check if <tctx> is the system time zone
bool TCTX_IS_SYSTEM  ( timecontext_t tctx );

//! Check if <tctx> is the UTC time zone
bool TCTX_IS_UTC  ( timecontext_t tctx );

//! result is render flags from <aRFlagContext> with zone info from <aZoneContext>
timecontext_t TCTX_JOIN_RFLAGS_TZ ( timecontext_t aRFlagContext, timecontext_t aZoneContext );



// ---- utility functions -------------------------------------------------------------
/*! Specific bias string. Unit: hours */
string BiasStr( int bias );

/*! Clear the DST info of <t> */
void ClrDST( tz_entry &t );


typedef std::list<tz_entry> TZList;

class GZones {
  public:
    GZones() {
      #ifdef MUTEX_SUPPORT
        muP= newMutex();
      #endif

      predefinedSysTZ= TCTX_UNKNOWN; // no predefined system time zone
      sysTZ= predefinedSysTZ; // default to predefined zone, if none, this will be obtained from OS APIs
      isDbg= false; // !!! IMPORTANT: do NOT enable this except for test targets, as it leads to recursions (debugPrintf calls time routines!)
      fSystemZoneDefinitionsFinalized = false;

      #ifdef SYDEBUG
        getDbgMask  = 0;
        getDbgLogger= NULL;
      #endif
    } // constructor

    /*! @brief populate GZones with system information
     *
     * Sets predefinedSysTZ, sysTZ and adds time zones
     * to tzP, if that information can be found on the
     * system.
     *
     * Returns false in case of a fatal error.
     */
    bool initialize();

    /*! @brief log and/or add more GZones
     *
     * Called after config was read and normal debug logging
     * is possible.
     */
    void loggingStarted();

    /*! @brief find a matching time zone
     *
     * This returns the best match, with "better" defined as (best
     * match first):
     * - exact name AND exact rule set match
     * - existing entry has a location (currently only the case for
     *   time zones imported from libical) AND that location is part of the
     *   name being searched for AND the rule matches
     * - as before, but with the rule match
     * - exact rule set match
     *
     * If there are multiple entries which are equally good, the first one
     * is returned.
     *
     * When doing rule matching, the dynYear value of an existing
     * entry has to match the current year as implemented by the
     * FitYear() function in timezone.cpp. In most (all?) cases this
     * has the effect that historic rules are skipped. Likewise,
     * VTIMEZONE information sent out is based on the current rules
     * for the zone, regardless of when the event itself takes place.
     *
     * This is approach is intentional: it helps avoid mismatches
     * when historic rules of one zone match the current rules of
     * another. Furthermore, it helps peers which do rule-based
     * matching and only know the current rules of each zone.
     *
     * @param  aTZ         entry with rules and name set; ident is ignored
     * @retval aContext    the matching time zone context ID; it always
     *                     refers to the tz_entry without a dynYear
     * @return true if match found
     */
    bool matchTZ(const tz_entry &aTZ, TDebugLogger *aLogP, timecontext_t &aContext);

    class visitor {
    public:
      /** @return true to stop iterating */
      virtual bool visit(const tz_entry &aTZ, timecontext_t aContext) = 0;
      virtual ~visitor() {} // destructor
    };

    /*! @brief invoke visitor once for each time zone
     * @return true if the visitor returned true
     */
    bool foreachTZ(visitor &v);

    void ResetCache(void) {
      sysTZ= predefinedSysTZ; // reset cached system time zone to make sure it is re-evaluated
    }

    #ifdef MUTEX_SUPPORT
    #endif

    TZList                    tzP; // the list of additional time zones
    timecontext_t predefinedSysTZ; // can be set to a specific zone to override zone returned by OS API
    timecontext_t           sysTZ; // the system's time zone, will be calculated,
                                   // if set to tctx_tz_unknown
    bool                    isDbg; // write debug information
    bool fSystemZoneDefinitionsFinalized; // finalizeSystemZoneDefinitions() already called

    #ifdef SYDEBUG
      uInt32        getDbgMask; // allow debugging in a specific context
      TDebugLogger* getDbgLogger;
    #endif
}; // GZones



// visible for debugging only
timecontext_t SystemTZ( GZones *g, bool isDbg= false );
timecontext_t SelectTZ( TDaylightSavingZone zone, int bias, int biasDST, lineartime_t tNow,
                                   bool isDbg= false );

// visible for platform_timezones.cpp/.mm
bool ContextForEntry( timecontext_t &aContext, tz_entry &t, bool chkNameFirst, GZones* g );
void Get_tChange( lineartime_t tim, tChange &v, sInt16 &y, bool asDate= false );



/*! Get <tz_entry> from <aContext>
 *                   or <std>/<dst>
 */
bool GetTZ( timecontext_t aContext,                        tz_entry &t, GZones* g, int year= 0 );
bool GetTZ( string std, string dst, int bias, int biasDST, tz_entry &t, GZones* g );

/*! Get the current year
 */
sInt16 MyYear( GZones* g );


/*! Returns true, if the given TZ is existing already
 *    <t>            tz_entry to search for:
 *                   If <t.name> == ""  search for any entry with these values.
 *                      <t.name> != ""  name must fit
 *                   If <t.ident>== ""  search for any entry with these values
 *                      <t.ident>!= ""  ident must fit
 *                      <t.ident>== "?" search for the name <t.name> only.
 *
 *    <aName>        is <t.name> of the found record
 *    <aContext>     is the assigned context
 *    <g>            global list of additional time zones
 *    <createIt>     create an entry, if not yet existing / default: false
 *    <searchOffset> says, where to start searching       / default: at the beginning
 */
bool FoundTZ( const tz_entry &t,
              string        &aName,
              timecontext_t &aContext,
              GZones*       g,
              bool          createIt    = false,
              timecontext_t searchOffset= tctx_tz_unknown,
              bool          olsonSupport= false );


/*! Remove an existing entry
 *  Currently, elements of the hard coded list can't be removed
 */
bool RemoveTZ( const tz_entry &t, GZones* g );


/*! Is it a DST time zone ? (both months must be defined for DST mode) */
bool DSTCond( const tz_entry &t );

/*! Adjust to day number within month <m>, %% valid till year <y> 2099 */
void AdjustDay( sInt16 &d, sInt16 m, sInt16 y );

/*! Get lineartime_t of <t> for a given <year>, either from std <toDST> or vice versa */
lineartime_t DST_Switch( const tz_entry &t, int bias, sInt16 aYear, bool toDST );

/*! Convert time zone name into context
 *  @param[in]  aName    : context name to resolve
 *  @param[out] aContext : context for this aName
 *  @param[in]  g        : global list of additional time zones
 *
 */
bool TimeZoneNameToContext( cAppCharP aName, timecontext_t &aContext, GZones* g, bool olsonSupport= false );

/*! Convert context into time zone name, a preferred name can be given
 *  @param[in]  aContext   : time context to resolve
 *  @param[out] aName      : context name for this aContext
 *  @param[in]  g          : global list of additional time zones
 *  @param[in]  aPrefIdent : preferred name, if more than one is fitting
 *
 */
bool TimeZoneContextToName( timecontext_t aContext, string &aName, GZones* g, cAppCharP aPrefIdent= "" );



/*! get system's time zone context (i.e. resolve the TCTX_SYSTEM meta-context)
 *  @param[in,out] aContext : context will be made non-meta, that is, if input is TCTX_SYSTEM,
 *                            actual time zone will be determined.
 *  @param[in] g            : global list of additional time zones
 */
bool TzResolveMetaContext( timecontext_t &aContext, GZones* g );

/*! make time context non-symbolic (= calculate minute offset east of UTC for aRefTime)
 *  but retain other time context flags in aContext
 *  @param[in,out] aContext : context will be made non-symbolic, that is resolved to minute offset east of UTC
 *  @param[in] aRefTime     : reference time point for resolving the offset
 *  @param[in] aRefTimeUTC  : if set, reference time must be UTC,
 *                            otherwise, reference time must be in context of aContext
 *  @param[in] g            : global list of additional time zones
 */
bool TzResolveContext( timecontext_t &aContext, lineartime_t aRefTime, bool aRefTimeUTC, GZones* g );

/*! calculate minute offset east of UTC for aRefTime
 *  @param[in] aContext       : time context to resolve
 *  @param[out] aMinuteOffset : receives minute offset east of UTC
 *  @param[in] aRefTime       : reference time point for resolving the offset
 *  @param[in] aRefTimeUTC    : if set, reference time must be UTC,
 *                              otherwise, reference time must be in context of aContext
 *  @param[in] g              : global list of additional time zones
 */
bool TzResolveToOffset( timecontext_t aContext, sInt16      &aMinuteOffset,
                                                lineartime_t aRefTime,
                                                bool         aRefTimeUTC,
                                                GZones*      g );


/*! Offset between two contexts (in seconds)
 *  Complex time zones (type "$") can't be currently resolved, they return false
 *  @param[in] aSourceValue   : reference time for which the offset should be calculated
 *  @param[in] aSourceContext : source time zone context
 *  @param[in] aTargetContext : source time zone context
 *  @param[out] sDiff         : receives offset east of UTC in seconds
 *  @param[in] g              : global list of additional time zones
 *  @param[in] aDefaultContext: default context to use if source or target is TCTX_UNKNOWN
 */
bool TzOffsetSeconds( lineartime_t aSourceValue, timecontext_t aSourceContext,
                                                 timecontext_t aTargetContext,
                                                 sInt32        &sDiff,
                                                 GZones*       g,
                                                 timecontext_t aDefaultContext= TCTX_UNKNOWN);



/*! Converts timestamp value from one zone to another
 *  Complex time zones (type "$") can't be currently resolved, they return false
 *  @param[in/out] aValue     : will be converted from source context to target context
 *  @param[in] aSourceContext : source time zone context
 *  @param[in] aTargetContext : source time zone context
 *  @param[in] g              : global list of additional time zones
 *  @param[in] aDefaultContext: default context to use if source or target is TCTX_UNKNOWN
 */
bool TzConvertTimestamp( lineartime_t &aValue,   timecontext_t aSourceContext,
                                                 timecontext_t aTargetContext,
                                                 GZones*       g,
                                                 timecontext_t aDefaultContext = TCTX_UNKNOWN);


/*! Prototypes for platform-specific implementation of time-zone-related routines
 *  which are implemented in platform_time.cpp
 */

/*! @brief get system real time
 *  @return system's real time in lineartime_t scale, in specified time zone context
 *  @param[in] aTimeContext : desired output time zone
 *  @param[in] g            : global list of additional time zones
 *  @param[in] aNoOffset    : no offset calculation, if true (to avoid recursive calls)
 */
lineartime_t getSystemNowAs( timecontext_t aTimeContext, GZones* g, bool aNoOffset= false );


/*! Prototypes for platform-specific implementation of time-zone-related routines
 *  which are implemented in platform_timezones.cpp/.mm
 */

/*! @brief platform specific loading of time zone definitions
 *  @return true if this list is considered complete (i.e. no built-in zones should be used additionally)
 *  @param[in/out] aGZones : the GZones object where system zones should be loaded into
 *  @note this is called at construction of the SyncAppBase before any logging facilities are
 *        available. This routine should load enough time zone information such that config
 *        can be read and conversion between UTC and system local time is possible.
 *        Use finalizeSystemZoneDefinitions() to add time zones with full logging available.
 */
bool loadSystemZoneDefinitions( GZones* aGZones );

/*! @brief second opportunity to load platform specific time zone definitions with logging available (and config already parsed)
 *  Called only once per GZones instance.
 *  @param[in/out] aGZones : the GZones object where additional system zones should be loaded into
 */
void finalizeSystemZoneDefinitions( GZones* aGZones );

/*! @brief get current system time zone
 *  @return true if successful
 *  @param[out] aContext : the time zone context representing the current system time zone.
 *  @param[in] aGZones : the GZones object.
 */
bool getSystemTimeZoneContext( timecontext_t &aContext, GZones* aGZones );



} // namespace sysync

#endif
/* eof */
