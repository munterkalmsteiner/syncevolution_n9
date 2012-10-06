/* Common Product options
 * ======================
 *
 */

// Custom Unified Server/Client Library Products
// #############################################

// Separate Version
// ================

// Note: for now, library has NO SEPARATE VERSION, but shows engine version

/*
#define SYSYNC_VERSION_MAJOR       1
#define SYSYNC_VERSION_MAJOR_TXT   "1"

#define SYSYNC_VERSION_MINOR       1
#define SYSYNC_VERSION_MINOR_TXT   "1"

#define SYSYNC_SUBVERSION       0
#define SYSYNC_SUBVERSION_TXT   "0"

#define SYSYNC_BUILDNUMBER      0
#define SYSYNC_BUILDNUMBER_TXT  "0"
*/


// new major or minor gives a new demo period
#define SYSER_VERSCHECK_MASK 0xFFFF0000


// Most hyperglobal definition, might even influence global_options.h:
// - THIS IS BOTH SERVER and CLIENT
#define SYSYNC_SERVER 1
#define SYSYNC_CLIENT 1

// - is a engine library
#define ENGINE_LIBRARY 1

// - ...which is used in standalone apps
#define STANDALONE_APP 1

// global options
#include "global_options.h"

// Product (but not target) specific options
// #########################################

// Hard expiration date, software will stop working after defined date
// if not registered
#undef VERSION_COMMENTS
#define VERSION_COMMENTS "Demo expires after " EXPIRY_DATE_STRING " if not registered"
#define EXPIRES_AFTER_DATE 1

// SySync options
// ==============

// - use precise time when possible
#define NOW_WITH_MILLISECONDS 1


// - if defined, debug code is included (not necessarily enabled, see gDebug)
//   if 1, only "public" debugging is enabled, if >1, all debugging is enabled
#ifdef RELEASE_VERSION
  #if RELEASE_SYDEBUG
    #define SYDEBUG RELEASE_SYDEBUG
  #else
    #undef SYDEBUG // absolutely no debug code for release!
    #undef CONSOLEINFO
  #endif
#else
  #define SYDEBUG 2 // 3=including XPT trace, 4=including memory leak detection
  #undef CONSOLEINFO
#endif

// local Database options
#define DESKTOP_CLIENT 1 // selects different binfile format (actually, will merge with mobile format at version 5)
// separate files for outlook - different from old 1.0 client to make sure we don't import old problems
#define SETTINGS_BFI_PREFIX "sysynclib_"

// - if defined, local DB does not require any login in client mode
#define NO_LOCAL_DBLOGIN 1

// - if defined, config file might contain <syncrequest>s which will
//   allow a client to run directly from config w/o additionally
//   provided parameters.
#undef PRECONFIGURED_SYNCREQUESTS

// - if defined, XML config parsing is completely disabled,
//   all config must be set programmatically
#undef HARDCODED_CONFIG

// - if defined, remoterule mechanism is not included
#undef NO_REMOTE_RULES

// - if defined, support for hardcoded (predefined)
//   type definitions will be included
#undef HARDCODED_TYPE_SUPPORT
// - if defined, support for configurable types will be included
#define CONFIGURABLE_TYPE_SUPPORT 1


// do not modify remote IDs in any way while processing them
#define DONT_STRIP_PATHPREFIX_FROM_REMOTEIDS 1

// if defined, session continues after 401/407
//#define AUTH_RETRY_USES_SAME_CLIENT_SESSION



// SyncML Toolkit options
// ======================

// if defined, the entire complicated and thread-unsafe workspace manager
// is completely bypassed
#define NOWSM 1

/* if defined, avoids using <![CDATA[ for opaque data except when really needed, i.e. contents would break XML */
#define PCDATA_OPAQUE_AS_CDATA 1


// we want the toolkit linked static
#define __LINK_TOOLKIT_STATIC__ 1


/* eof */
