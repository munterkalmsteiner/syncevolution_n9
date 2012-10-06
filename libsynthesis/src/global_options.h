/* SySync entirely global options (affects all targets)
 * ====================================================
 *
 * (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef GLOBAL_OPTIONS_H
#define GLOBAL_OPTIONS_H


// the following should be defined BEFORE including this file
// - SYSYNC_CLIENT : defined if client functionality should be included
// - SYSYNC_SERVER : defined if server functionality should be included
//   If not defined, and SYSYNC_CLIENT is also not defined, build
//   defaults to SYSYNC_SERVER
// - platform defines, like MACOSX, MOBOSX, LINUX, WINCE, _WIN32
//   __PALM_OS__, __EPOC_OS__ etc.
// - RELEASE_VERSION : define in targets that are real product releases
//   not internal debug/test/experimental releases
// - RELEASE_SYDEBUG : define the debug level for RELEASE_VERSION
//   (0=no debug code at all, 1=basic debug, 2=extended, 3=developer only)
//   Non-release versions (RELEASE_VERSION undefined) usually have
//   a debug level of 2 or 3 (defined in target or product options).


// signal that we are compiling with the SYSYNC engine
#define SYSYNC_ENGINE 1


// Expiry date for products (those that have use EXPIRES_AFTER_DATE at all)
#if defined(EXPIRY_YEAR) && defined(SYSER_REGISTRATION)
  #error "it seems that this target still defines it's own private expiry date - please update target_options.h"
#endif
#if defined(EXPIRY_YEAR) && EXPIRY_YEAR<2014
	#warning "Target has a dangerously early expiry year - please check if it is correct"
#endif
// global expiry date (usually applies for unregistered demos or regular products in trial mode)
#ifndef EXPIRY_YEAR
  #define EXPIRY_DATE_STRING "2014-03-31"
  #define EXPIRY_YEAR 2014
  #define EXPIRY_MONTH 3
  #define EXPIRY_DAY 31
#endif

// Release date (date relevant for licenses that are valid only up to a certain release date)
#if !defined(RELEASE_YEAR) || !defined(RELEASE_MONTH)
  // define one globally in case target or product does not specify it's own date
  #define RELEASE_YEAR        	2010
  #define RELEASE_YEAR_TXT   		"2010"
  #define RELEASE_MONTH       	1
  #define RELEASE_MONTH_TXT  		"1"
#endif
// Real Release date (shown in some texts)
#if !defined(REAL_RELEASE_YEAR)
  #define REAL_RELEASE_YEAR     2012
  #define REAL_RELEASE_YEAR_TXT "2012"
#endif


// For old targets that do not have product_options.h yet
// - Set common Version information
#ifndef SYSYNC_VERSION_MAJOR
#define SYSYNC_VERSION_MAJOR        3
#define SYSYNC_VERSION_MAJOR_TXT   "3"
#endif

#ifndef SYSYNC_VERSION_MINOR
#define SYSYNC_VERSION_MINOR        4
#define SYSYNC_VERSION_MINOR_TXT   "4"
#endif

#ifndef SYSYNC_SUBVERSION
#define SYSYNC_SUBVERSION           0
#define SYSYNC_SUBVERSION_TXT      "0"
#endif

#ifndef SYSYNC_BUILDNUMBER
#define SYSYNC_BUILDNUMBER          41
#define SYSYNC_BUILDNUMBER_TXT     "41"
#endif


// Platform name
#ifndef SYSYNC_PLATFORM_NAME
  #if defined(__EPOC_OS__)
    #define SYSYNC_PLATFORM_NAME "SymbianOS"
  #elif defined(MOBOSX)
    #define SYSYNC_PLATFORM_NAME "iOS"
  #elif defined(MACOSX)
    #define SYSYNC_PLATFORM_NAME "MacOSX"
  #elif defined(ANDROID)
    #define SYSYNC_PLATFORM_NAME "Android"
  #elif defined(LINUX)
    #define SYSYNC_PLATFORM_NAME "Linux"
  #elif defined(__PALM_OS__)
    #define SYSYNC_PLATFORM_NAME "PalmOS"
  #elif defined(WINCE)
    #define SYSYNC_PLATFORM_NAME "WinCE"
  #elif defined(_WIN32)
    #define SYSYNC_PLATFORM_NAME "Win32"
  #endif
#endif


// product version string
#define SYSYNC_VERSION_STRING SYSYNC_VERSION_MAJOR_TXT "." SYSYNC_VERSION_MINOR_TXT "." SYSYNC_SUBVERSION_TXT
// full version string
#define SYSYNC_FULL_VERSION_STRING SYSYNC_VERSION_MAJOR_TXT "." SYSYNC_VERSION_MINOR_TXT "." SYSYNC_SUBVERSION_TXT "." SYSYNC_BUILDNUMBER_TXT
// full version string
#define SYSYNC_MAIN_VERSION_STRING SYSYNC_VERSION_MAJOR_TXT "." SYSYNC_VERSION_MINOR_TXT
// uInt32 format of version (8 bits per dotted group)
#define SYSYNC_VERSION_UINT32 (((uInt32)SYSYNC_VERSION_MAJOR<<24)+((uInt32)SYSYNC_VERSION_MINOR<<16)+((uInt32)SYSYNC_SUBVERSION<<8)+(uInt32)SYSYNC_BUILDNUMBER)


// client and server strings for XPT-based apps
#define SERVER_NAME "Synthesis SyncML Server/" SYSYNC_FULL_VERSION_STRING
#define CLIENT_NAME "Synthesis SyncML Client/" SYSYNC_FULL_VERSION_STRING " [en] (" SYSYNC_PLATFORM_NAME "; I)"

// FPI product identifier
#define SYSYNC_FPI "-//Synthesis AG//NONSGML SyncML Engine V" SYSYNC_FULL_VERSION_STRING "//EN"


// Global feature switches (might be overridden in individual targets)
// ###################################################################


// server or client macros
// - make sure we have either SYSYNC_CLIENT, SYSYNC_SERVER or both
// - if combined build, define SERVER_CLIENT_BUILD as well
#if defined(SYSYNC_CLIENT) && defined(SYSYNC_SERVER)
	#define SERVER_CLIENT_BUILD 1
#elif !defined(SYSYNC_CLIENT)
	#define SYSYNC_SERVER 1
#endif
// build differentiation macros
#ifdef SERVER_CLIENT_BUILD
	// dynamically switching between server and client
  #define IS_CLIENT (!getSyncAppBase()->isServer())
  #define IS_SERVER (getSyncAppBase()->isServer())
#else
	// static, built as either server or client
  #ifdef SYSYNC_CLIENT
  	// client
    #define IS_CLIENT 1
    #define IS_SERVER 0
  #else
  	// server
    #define IS_CLIENT 0
    #define IS_SERVER 1
  #endif
#endif


// build type
#ifdef ENGINE_LIBRARY
  // SySync engine with API
  // - may not have any globals
  #undef DIRECT_APPBASE_GLOBALACCESS
  // - has Engine Interface
  #define ENGINEINTERFACE_SUPPORT 1
  // - should support constant XML config
  #define CONSTANTXML_CONFIG 1
#else
  // Classic SySync build
  // - needs direct accessors to syncAppBase and maybe other global vars
  #define DIRECT_APPBASE_GLOBALACCESS 1
#endif

// binfile layer specifics
#if defined(BASED_ON_BINFILE_CLIENT) && !defined(SYSYNC_SERVER)
	// client-only build with binfiles included -> binfile must be always active
  // (and customimpl does not need a lot of stuff it would otherwise include)
	#define BINFILE_ALWAYS_ACTIVE 1
#endif


// default datatype support
#define MIMEDIR_SUPPORT 1
#define TEXTTYPE_SUPPORT 1
#define DATAOBJ_SUPPORT 1
#define RAWTYPE_SUPPORT 1

// intermediate stuff for v3.x development
/// @todo
/// the following defines are for v3.x development only and must be cleared out or moved to a permanent location for a final version!

// default to ODBC support (if and only if SQL_SUPPORT is selected in the target)
#define ODBCAPI_SUPPORT 1

// default to both text and AsKey variants of item passing in Api datastores
#define DBAPI_TEXTITEMS 1
#define DBAPI_ASKEYITEMS 1


// SyncML version support
#define MAX_SYNCML_VERSION syncml_vers_1_2

#ifdef SYSYNC_CLIENT
  // these are normally client options, servers don't have them by default

  // client provides GUI suport for CGI server options such as TAF and/or /li(x) /dr(x,y)...
  #define CGI_SERVER_OPTIONS 1

  #undef CHINESE_SUPPORT

#else
  // these are normally server options, clients don't have them by default

  // include large-footprint conversions between chinese and UTF-8
  #define CHINESE_SUPPORT 1

  // if defined, TAF option chars will be used like syncset filter /fi(...)
  // (that's how it always was before 1.0.8.9 server)
  // Was globally defined 1 until and including server 2.0.5.12
  // Starting with 2.0.5.13 server, real TAF is now (partially) available, so we
  // switch this off
  #undef TAF_AS_SYNCSETFILTER
  // real SyncML-like TAF
  #define SYNCML_TAF_SUPPORT 1

  // if defined, we support Synthesis style target options (/opt(args) style)
  #define SYSYNC_TARGET_OPTIONS 1

  // object is enabled by default for now (standard version turns it off)
  #define OBJECT_FILTERING 1

  // array field support is enabled by default for now (standard version turns it off)
  #define ARRAYFIELD_SUPPORT 1

  // array field support is enabled by default for now (standard version turns it off)
  #define STREAMFIELD_SUPPORT 1

  // full support for email in textitem
  #define EMAIL_FORMAT_SUPPORT 1
  #define EMAIL_ATTACHMENT_SUPPORT 1

  // script support is enabled by default for now (standard version turns it off)
  #define SCRIPT_SUPPORT 1
  #define REGEX_SUPPORT 1

  // superdatastore support is enabled by default
  #define SUPERDATASTORES 1

#endif



// ODBC not compatible with 1.0.5.x type config any more
#undef OLD_1_0_5_CONFIG_COMPATIBLE


// Settings that have proven globally correct by now
// #################################################

// SySync options
// ==============

// s2g does not need these extras any more, everything is solved with scripts
// #define SPACE2GO_EXTRAS 1

// use new space evaluation method
#define USE_SML_EVALUATION 1

// do not modify remote IDs in any way while processing them
#define DONT_STRIP_PATHPREFIX_FROM_REMOTEIDS 1

// Time options
// - if defined, querying current lineartime will return millisecond
//   accuray value.
#define NOW_WITH_MILLISECONDS 1
// activate the new timezone system
#define NEW_TIMEZONES 1

// - if defined, code for incoming and outgoing SyncML dumping into (WB)XML logfiles is included
#define MSGDUMP 1

// - if defined, code for incoming message simulation from "i_" prefixed incoming message dump file is included
#undef SIMMSGREAD

// - allow keep connection
#define HTTP_KEEP_CONNECTION 1


// SyncML Toolkit options
// ======================

// if defined, the entire complicated and thread-unsafe workspace manager
// is completely bypassed
#define NOWSM 1

/* correct tagging of XML payload with <![CDATA[ */
#define PCDATA_OPAQUE_AS_CDATA 1

// allow Alert w/o Itemlist
#define SYNCFEST_ALLOW_ALERT_WITHOUT_ITEMLIST 1


// PCRE options
// ============

#define PCRE_STATIC 1 // no special declaration, PCRE is compiled-in

#endif // GLOBAL_OPTIONS_H

/* eof */
