/* Target options
 * ==============
 */

// SYNCML CLIENT ENGINE LIBRARY OPENSOURCE iOS
// ###########################################

// define platform
// - we are on MACOSX (but on the mobile variant aka iOS of it)
#define MACOSX 1
#define MOBOSX 1

// - XCode does support exceptions
#define TARGET_HAS_EXCEPTIONS 1

// Release version status
#if !DEBUG
#define RELEASE_VERSION
#endif
#define RELEASE_SYDEBUG 2 // extended DBG included
//#define OPTIONAL_SYDEBUG 1

// Eval limit options
// ==================

// Opensource version does not have any licensing checks
#define NEVER_EXPIRES_IS_OK 1 // to explicitly override check in SyncAppBase

// now include platform independent product options (which include global_options.h)
#include "product_options.h"

// turn on console for XCode debug builds
#if DEBUG
	#define CONSOLEINFO 1 // sysync enables console output with this
	#define CONSOLEDBG 1 // our Cocoa wrapper code and plugins enables some debug message with this
  #define SYDEBUG_LOCATION "" // source code links enabled
#endif

// enable tunnel API for direct access to backend datastores
#define DBAPI_TUNNEL_SUPPORT 1


// Identification strings
#define CUST_SYNC_MAN       SYSYNC_OEM  // manufactured by ourselves
#define CUST_SYNC_MODEL     "SySync Client Library Opensource iOS"
#define CUST_SYNC_FIRMWARE  NULL  // no firmware
#define CUST_SYNC_HARDWARE  NULL  // no hardware

#define SYNCML_CLIENT_DEVTYP "pda"; // iPhone and iPod Touch can be seen as PDA (better than "smartphone", the iPod is not a phone)


// String used to construct logfile names
#define TARGETID "sysynclib_client_iphoneos"

// Configuration
// =============

// Default profile
#undef HARD_CODED_SERVER_URI
#undef HARD_CODED_DBNAMES
#define DEFAULT_SERVER_URI "http://"
#define DEFAULT_LOCALDB_PROFILE ""
#define DEFAULT_SERVER_USER ""
#define DEFAULT_SERVER_PASSWD ""
#define DEFAULT_ENCODING SML_WBXML
#define DEFAULT_TRANSPORT_USER NULL
#define DEFAULT_TRANSPORT_PASSWD NULL
#define DEFAULT_SOCKS_HOST NULL
#define DEFAULT_PROXY_HOST NULL
#define DEFAULT_DATASTORES_ENABLED false
#define DEFAULT_EVENTS_DAYSBEFORE 30
#define DEFAULT_EVENTS_DAYSAFTER 90
#define DEFAULT_EVENTS_LIMITED false
#define DEFAULT_EMAILS_LIMITED true
#define DEFAULT_EMAILS_HDRONLY true
#define DEFAULT_EMAILS_MAXKB 2
#define DEFAULT_EMAILS_ONLYLAST true
#define DEFAULT_EMAILS_DAYSBEFORE 10


// Eval limit options
// ==================

// Note: Hard expiration date settings moved to product_options.h

// - if defined, software will stop specified number of days after
//   first use
#undef EXPIRES_AFTER_DAYS // Engine does not have auto-demo

// Identification for update check and demo period
#define SYSER_VARIANT_CODE SYSER_VARIANT_PRO
#define SYSER_PRODUCT_CODE SYSER_PRODCODE_CLIENT_LIB_IPHONEOS
#define SYSER_EXTRA_ID SYSER_EXTRA_ID_NONE

// This is a static library hard-linked with the plan44.ch app sold via AppStore and thus needs no licensing
// and may not expire
#undef SYSER_REGISTRATION
#undef VERSION_COMMENTS
#define VERSION_COMMENTS "plan44.ch libsynthesis opensource"
#undef EXPIRES_AFTER_DATE

// - define allowed product codes
#define SYSER_PRODUCT_CODE_MAIN SYSER_PRODUCT_CODE // license specifically for the product we are
#define SYSER_PRODUCT_CODE_ALT1 SYSER_PRODCODE_CLIENT_PDA_PRO // a PRO license for a PDA client works as well
#define SYSER_PRODUCT_CODE_ALT2 SYSER_PRODCODE_CLIENT_LIB_ALL
#define SYSER_PRODUCT_CODE_ALT3 SYSER_PRODCODE_CLIENT_LIB_MOBILE
#undef SYSER_PRODUCT_CODE_ALT4
// - define needed product flags
//   only licenses that are release date limited (or explicitly NOT release date limited,
//   or time limited) are allowed
#define SYSER_NEEDED_PRODUCT_FLAGS SYSER_PRODFLAG_MAXRELDATE
#define SYSER_FORBIDDEN_PRODUCT_FLAGS 0

// for the internal version, DLL plugins are always allowed
// (even with licenses that do not have SYSER_PRODFLAG_SERVER_SDKAPI set)
// because we do not use a license at all!
#define DLL_PLUGINS_ALWAYS_ALLOWED 1


// SySync options
// ==============

// - we can rely on iOS devices to have a unique device ID
//   (disables userhash-for-Oracle server workarounds we need e.g. for WinMobile with all those crappy
//   OEM implementations like MotoQ and SGH i780 that return same ID for all devices).
#define GUARANTEED_UNIQUE_DEVICID 1

// - enhanced profile record
#define ENHANCED_PROFILES_2004 1
#define CLIENTFEATURES_2008 1

// - support for Proxy
#define PROXY_SUPPORT 1

// - support for multiple profiles
#define MULTI_PROFILE_SUPPORT 1

// - show progress events
#define PROGRESS_EVENTS 1

// - we do not need to squeeze code
#undef MINIMAL_CODE

// - script with regex support
#define SCRIPT_SUPPORT
#define REGEX_SUPPORT

// - client does not need target options
#undef SYSYNC_TARGET_OPTIONS

// - filters
#define OBJECT_FILTERING

// - client does not need superdatastores
#undef SUPERDATASTORES


// general options needed for email
#define EMAIL_FORMAT_SUPPORT 1
#define EMAIL_ATTACHMENT_SUPPORT 1
#define ARRAYFIELD_SUPPORT 1

// - if defined, stream field support will be included
#define STREAMFIELD_SUPPORT 1

// - if defined, semi-proprietary zipped-binary <data> for items (any type) can be used
//   (enabled on a by type basis in the config)
#define ZIPPED_BINDATA_SUPPORT 1


// - where to save application data by default (if not otherwise configured)
//   APPDATA_SUBDIR is a subdirectory of the user's "application data" dir.
#define APPDATA_SUBDIR "plan44.ch/sysynclib_client"

#if RELEASE_SYDEBUG>0
  // - if defined, code for incoming and outgoing SyncML dumping into (WB)XML logfiles is included
  #define MSGDUMP 1
#else
	#undef MSGDUMP
#endif

// Datastore Options
// -----------------

// - link in customimplds on top of binfile
#define BASED_ON_BINFILE_CLIENT 1
// - SQL and API datastores have changedetection, so build binfile for that
#define CHANGEDETECTION_AVAILABLE 1
// - we need a separate changelog mechanism
#define CHECKSUM_CHANGELOG 1
// - string localIDs with sufficiently large size
#define STRING_LOCALID_MAXLEN 256

// - supports SQL, but no ODBC, SQLite and DBApi
#define SQL_SUPPORT 1
#undef ODBCAPI_SUPPORT
#define SQLITE_SUPPORT 1
// - master detail (array) tables supported
#define ARRAYDBTABLES_SUPPORT 1

// - Plugin SDK support is included
#define SDK_SUPPORT         1
#undef PLUGIN_DLL // no external dylib plugins, iOS SDK does not allow them
#define IPHONE_PLUGINS_STATIC 1 // iOS SDK needs statically linked plugins

// - always link 4 custom plugins. A separate project provides a set of
//   4 dummy plugins that can be linked if no real custom plugins are needed
//   (e.g. in a SQLite3 only sync)
#define HARDCODED_CUSTOM 1
// - always link the TAB separate text file plugin
#define DBAPI_TEXT 1

/* eof */
