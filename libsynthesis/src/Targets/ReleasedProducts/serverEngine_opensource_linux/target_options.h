/* Target options
 * ==============
 *
 */

// SYNCML SERVER ENGINE LIBRARY OPENSOURCE LINUX
// #############################################

// define platform
#define LINUX

// Release version status
#define RELEASE_VERSION
#define RELEASE_SYDEBUG 2 // extended DBG included
//#define OPTIONAL_SYDEBUG 1

// Eval limit options
// ==================

// OpenSource Library does not have any expiry mechanisms
#define NEVER_EXPIRES_IS_OK 1 // to explicitly override check in SyncAppBase

// now include platform independent product options (which include global_options.h)
#include "product_options.h"


// Identification strings
#define CUST_SYNC_MAN       SYSYNC_OEM  // manufactured by ourselves
#define CUST_SYNC_MODEL     "SySync Server Library OpenSource Linux"
#define CUST_SYNC_FIRMWARE  NULL  // no firmware
#define CUST_SYNC_HARDWARE  NULL  // no hardware

// String used to construct logfile names
#define TARGETID "sysynclib_srv_linux"


// Note: Hard expiration date settings moved to product_options.h

// - if defined, software will stop specified number of days after
//   first use
#undef EXPIRES_AFTER_DAYS // Engine does not have auto-demo

// Identification for update check and demo period
#define SYSER_VARIANT_CODE SYSER_VARIANT_PRO
#define SYSER_PRODUCT_CODE SYSER_PRODCODE_SERVER_LIB_LINUX
#define SYSER_EXTRA_ID SYSER_EXTRA_ID_NONE

// This is the opensource Linux library, and needs no registration
#undef SYSER_REGISTRATION
#undef VERSION_COMMENTS
#define VERSION_COMMENTS "Synthesis OpenSource"
#undef EXPIRES_AFTER_DATE

// - define allowed product codes
#define SYSER_PRODUCT_CODE_MAIN SYSER_PRODCODE_SERVER_DEMO // a permanent DEMO license
#define SYSER_PRODUCT_CODE_ALT1 SYSER_PRODCODE_SERVER_LIB_LINUX // ..or a library license for Linux
#define SYSER_PRODUCT_CODE_ALT2 SYSER_PRODCODE_SERVER_LIB_ALL // ..or for all platforms
#define SYSER_PRODUCT_CODE_ALT3 SYSER_PRODCODE_SERVER_LIB_DESK // ..or for desktop platforms
#define SYSER_PRODUCT_CODE_ALT4 SYSER_PRODCODE_SERVER_ODBC_PRO_LINUX // ..or a PRO server for Linux
// - define needed product flags
//   only licenses that are release date limited (or explicitly NOT release date limited,
//   or time limited) are allowed
#define SYSER_NEEDED_PRODUCT_FLAGS SYSER_PRODFLAG_MAXRELDATE
#define SYSER_FORBIDDEN_PRODUCT_FLAGS 0

// for the opensource version, DLL plugins are always allowed
// (even with licenses that do not have SYSER_PRODFLAG_SERVER_SDKAPI set)
// because we do not use a license at all!
#define DLL_PLUGINS_ALWAYS_ALLOWED 1


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
// - id defined, code allows calling of subsequent DLLs
#define PLUGIN_DLL            1

// - define what SDK modules are linked in
//#define DBAPI_DEMO          1
#define   DBAPI_TEXT          1
//#define DBAPI_SILENT        1
//#define DBAPI_EXAMPLE       1


// SySync options
// ==============

// - no progress events (server engine code does not handle them at this time)
#undef PROGRESS_EVENTS

// - we do not need to squeeze code
#undef MINIMAL_CODE

// - script with regex support
#define SCRIPT_SUPPORT 1
#define REGEX_SUPPORT 1

// - server does support target options
#define SYSYNC_TARGET_OPTIONS 1

// - filters
#define OBJECT_FILTERING 1

// - server does need superdatastores
#define SUPERDATASTORES 1


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
#define APPDATA_SUBDIR "synthesis.ch/SySyncLib_srv"

// - if defined, code for incoming and outgoing SyncML dumping into (WB)XML logfiles is included
#define MSGDUMP 1

/* eof */
