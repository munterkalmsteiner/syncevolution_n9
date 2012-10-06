/* Target options
 * ==============
 *
 *
 */

// SYNCML CLIENT ENGINE LIBRARY DEMO Linux
// #######################################

// - define to make release version
// define platform
#define LINUX

// Release version status
#undef RELEASE_VERSION
#define RELEASE_SYDEBUG 2 // extended DBG included
//#define OPTIONAL_SYDEBUG 1

// now include platform independent product options (which include global_options.h)
#include "product_options.h"


// Identification strings
#define CUST_SYNC_MAN       SYSYNC_OEM  // manufactured by ourselves
#define CUST_SYNC_MODEL     "SySync Client Library DEMO Linux"
#define CUST_SYNC_FIRMWARE  NULL  // no firmware
#define CUST_SYNC_HARDWARE  NULL  // no hardware

// String used to construct logfile names
#define TARGETID "sysynclib_linux"

// Configuration
// =============

// Default profile
#undef HARD_CODED_SERVER_URI
#undef HARD_CODED_DBNAMES
#define DEFAULT_SERVER_URI ""
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

// - if defined, server will stop working after defined date
#undef VERSION_COMMENTS
#define VERSION_COMMENTS "Demo Version Expiring after " EXPIRY_DATE_STRING
#define EXPIRES_AFTER_DATE 1

// - if defined, software will stop specified number of days after
//   first use
#undef EXPIRES_AFTER_DAYS
// - if defined, software will run without valid license until hard
//   expiry date (EXPIRES_AFTER_DATE) and then continue only with a valid license
#define NO_LICENSE_UNTIL_HARDEXPIRY 1

// - Identification for update check and demo period
#define SYSER_VARIANT_CODE SYSER_VARIANT_DEMO
#define SYSER_PRODUCT_CODE SYSER_PRODCODE_CLIENT_LIB_WIN32
#define SYSER_EXTRA_ID SYSER_EXTRA_ID_NONE


// - if defined, software can be registered
#define SYSER_REGISTRATION 1
// - define allowed product codes
#define SYSER_PRODUCT_CODE_MAIN SYSER_PRODCODE_CLIENT_DEMO // a permanent DEMO license
#define SYSER_PRODUCT_CODE_ALT1 SYSER_PRODCODE_CLIENT_LIB_LINUX // ..or a library license for MacOSX
#define SYSER_PRODUCT_CODE_ALT2 SYSER_PRODCODE_CLIENT_LIB_ALL // ..or for all platforms
#define SYSER_PRODUCT_CODE_ALT3 SYSER_PRODCODE_CLIENT_LIB_DESK // ..or for desktop platforms
#define SYSER_PRODUCT_CODE_ALT4 SYSER_PRODCODE_CLIENT_ODBC_PRO_LINUX // ..or a PRO console client for MacOSX
// - define needed product flags
//   only licenses that are release date limited (or explicitly NOT release date limited,
//   or time limited) are allowed
#define SYSER_NEEDED_PRODUCT_FLAGS SYSER_PRODFLAG_MAXRELDATE
#define SYSER_FORBIDDEN_PRODUCT_FLAGS 0


// Database support options
// ========================

// - if defined, SQL support is included
#define SQL_SUPPORT           1
#undef ODBCAPI_SUPPORT
#define SQLITE_SUPPORT        1
// - if defined, ODBC DB mapping of arrays to aux tables is supported
#define ARRAYDBTABLES_SUPPORT 1

// - if defined, SDK support is included
#define SDK_SUPPORT           1
#undef  PLUGIN_DLL

// - define what SDK modules are linked in
//#define DBAPI_DEMO          1
#define DBAPI_TEXT            1
#define FILEOBJ_SUPPORT       1
#define ADAPTITEM_SUPPORT     1
#define JNI_SUPPORT           1 // JNI needed for GUI, but without PLUGIN_DLL, JNI is not available for DB plugins (correct for DEMO!)


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


// SySync options
// ==============

// - enhanced profile record
#define ENHANCED_PROFILES_2004 1
#define CLIENTFEATURES_2008 1

// - NO support for automatic syncing (timed, IPP, server alerted...)
#undef AUTOSYNC_SUPPORT
// - support for intelligent push & poll (IPP)
//#define IPP_SUPPORT 1
//#define IPP_SUPPORT_ALWAYS 1 // regardless of license flags
// - support for timed sync
#undef TIMEDSYNC_SUPPORT
// - support for WAP push alerted sync
#undef SERVERALERT_SUPPORT

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
#define APPDATA_SUBDIR "synthesis.ch/SySyncLib"

// - if defined, code for incoming and outgoing SyncML dumping into (WB)XML logfiles is included
#define MSGDUMP 1



/* eof */
