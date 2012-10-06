/* Target options
 * ==============
 *
 *
 */

// Most hyperglobal definitions, might even influence global_options.h:
// - THIS IS THE DIAGNOSTIC TOOL
#define SYSYNC_TOOL

// - this is the LINUX version of the sysytool
#define LINUX

// now include global switches
#include "global_options.h"

// - is a standalone APP
#define STANDALONE_APP 1

// Identification strings
#define CUST_SYNC_MAN       SYSYNC_OEM  // manufactured by ourselves
#define CUST_SYNC_MODEL     "SySync SyncML Diagnostic Tool"
#define CUST_SYNC_FIRMWARE  NULL  // no firmware
#define CUST_SYNC_HARDWARE  NULL  // no hardware

// String used to construct logfile names
#define TARGETID "sysytool"

// general comments
#define VERSION_COMMENTS "Internal Synthesis AG Version"

// Eval limit options
// ==================

// - if defined, server will have a restriction on concurrent sessions
//   from different devices.
#define VERSION_COMMENTS "Limited to 1 simultaneous sync sessions"

// - if defined, software will stop working as demo after defined date
#define EXPIRES_AFTER_DATE 1

// - if defined, software will stop specified number of days after
//   first use
#undef EXPIRES_AFTER_DAYS

// - variant
#define SYSER_VARIANT_CODE SYSER_VARIANT_PRO

// - if defined, software can be registered
#define SYSER_REGISTRATION 1
// - define allowed product codes (any server)
#define SYSER_PRODUCT_CODE_MAIN SYSER_PRODCODE_SERVER_PRO
#define SYSER_PRODUCT_CODE_ALT1 SYSER_PRODCODE_SERVER_STD
#define SYSER_PRODUCT_CODE_ALT2 SYSER_PRODCODE_SERVER_DEMO
#define SYSER_PRODUCT_CODE_ALT3 SYSER_PRODCODE_SERVER_XML2GO
#undef SYSER_PRODUCT_CODE_ALT4
// - define needed product flags
#define SYSER_NEEDED_PRODUCT_FLAGS 0 // no ISAPI/APACHE flag needed for XPT
#define SYSER_FORBIDDEN_PRODUCT_FLAGS 0


// Database support options
// ========================

// - if defined, SQL support is included
#define SQL_SUPPORT           1
#define ODBCAPI_SUPPORT       1
#define SQLITE_SUPPORT        1
// - if defined, ODBC DB mapping of arrays to aux tables is supported
#define ARRAYDBTABLES_SUPPORT 1

// - if defined, SDK support is included
//#define SDK_SUPPORT         1
//#define PLUGIN_DLL          1

// - define what SDK modules are linked in
//#define DBAPI_DEMO          1
#define DBAPI_TEXT            1
#define FILEOBJ_SUPPORT       1
#define ADAPTITEM_SUPPORT     1
#define JNI_SUPPORT           1


// SySync options
// ==============

// - if defined, debug code is included (not necessarily enabled, see gDebug)
//   if 1, only "public" debugging is enabled, if >1, all debugging is enabled
#ifdef RELEASE_VERSION
#define SYDEBUG 1
#else
#define SYDEBUG 2
#endif

#define CONSOLEINFO 1

// %%% include all profiling
//#define TIME_PROFILING 1
//#define MEMORY_PROFILING 1


// - if defined, support for configurable types will be included
#define CONFIGURABLE_TYPE_SUPPORT 1

// - if defined, object filtering will be included
#define OBJECT_FILTERING 1

// - if defined, procedure interpreter features will be included
#define SCRIPT_SUPPORT 1

// - if defined, superdatastores will be included
#define SUPERDATASTORES 1

// - if defined, array field support will be included
#define ARRAYFIELD_SUPPORT 1

// do not modify remote IDs in any way while processing them
#define DONT_STRIP_PATHPREFIX_FROM_REMOTEIDS 1


// ODBC options
// ============

// No SEH around ODBC calls
#define NO_AV_GUARDING 1

// if both text and ODBC maps are supported, mode can be switched
// in config.
#define USE_TEXTMAPS 1
#define USE_ODBCMAPS 1


// SyncML Toolkit options
// ======================

// if defined, the entire complicated and thread-unsafe workspace manager
// is completely bypassed
#define NOWSM 1

/* correct tagging of payload with <![CDATA[, does not parse correctly
   with RTK, therefore it should be normally disabled */
// %%% disabled for DCM, magically works ok with it.
#define PCDATA_OPAQUE_AS_CDATA 1

// %%%%% allow Alert w/o Itemlist, was needed for magically
//#define SYNCFEST_ALLOW_ALERT_WITHOUT_ITEMLIST 1

/* thread safety (added by luz@synthesis.ch, 2001-10-29) %%% */
//#define __MAKE_THREADSAFE 1
/* debug thread locking by using global ThreadDebugPrintf() to log locks/unlocks */
/* 1=only log waiting for >1sec for lock, 2=log all lock enter/leave ops */
//#define __DEBUG_LOCKS 1

// we want the toolkit linked static
#define __LINK_TOOLKIT_STATIC__ 1
// we want the XPT linked static
#define LINK_TRANSPORT_STATICALLY 1
// - select transports
#define INCLUDE_HTTP_STATICALLY
//#define INCLUDE_OBEX_STATICALLY
//#define INCLUDE_WSP_STATICALLY

// Verbose XPT debug only if high debug level
#if SYDEBUG>2

// switch on tracing for XPT
#define TRACE 1
#define TRACE_TO_STDOUT 1 // use global localOutput() function

// Debug options for OBEX (smlobex)
// - define one or several of these
//#define DEBUGALL // also hex-dumps all!! IrDA traffic
#define DEBUGFLOW
#define DEBUGINFO
#define DEBUGERROR

#endif
