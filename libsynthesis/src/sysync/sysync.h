/* sysync generic header file */

#ifndef SYSYNC_H
#define SYSYNC_H

/* Headers that might be available in precompiled form
 * (standard libraries)
 */
#ifndef SYSYNC_PRECOMP_H
#include "sysync_precomp.h"
#endif

#if __MC68K__
  // Note: STL includes may not be in precompiled headers for CW Palm v9
  //       (STL map crashes), so we have them here for MC68k
  /* - STL includes */
  #include <string>
  #include <vector>
  #include <map>
  #include <list>
#endif

#ifdef ANDROID
#include "android/log.h"
#endif

/* SySync headers (not precompiled during SySync development) */

// global constants and settings
#include "sysync_globs.h"

#ifdef DIRECT_APPBASE_GLOBALACCESS
  // only in old style environment with global anchors
  #include "sysync_glob_vars.h"
#endif

/* SyncML Toolkit includes */
// - SyncML Toolkit external API
extern "C" {
  #include "sml.h"
  #include "smlerr.h"
  #include "smldtd.h"
  #include "smldevinfdtd.h"
  #include "smlmetinfdtd.h"
  #include "mgrutil.h" // utilities to work with SmlXXX structs
  #include "libmem.h" // utilities to allocate/deallocate SML memory

}
// engine defs (public defines also used in SDK)
#include "engine_defs.h"

// utilities
#include "sysync_utils.h"
#ifdef SYSYNC_ENGINE
#include "stringutils.h"
#endif
#include "lineartime.h"
#include "iso8601.h"
#include "debuglogger.h"
// platform adapters
#include "configfiles.h"

/* integrated extensions into RTK sources 2002-06-20 %%%
// Synthesis SyncML toolkit extensions
extern "C" {
  #include "smlextensions.h"
}
*/

// utility classes without cross-dependencies
#include "syncexception.h"
#include "profiling.h"

// base classes
//#include "syncappbase.h"
//#include "itemfield.h"
//#include "syncitemtype.h"
//#include "syncitem.h"
//#include "vcarditemtype.h"
//#include "vcalendaritemtype.h"
//#include "mimediritemtype.h"
//#include "syncdatastore.h"
//#include "localengineds.h"
//#include "synccommand.h"
//#include "syncsession.h"
//#include "syncclient.h"
//#include "syncserver.h"
//#include "syncsessiondispatch.h"

// use sysync namespace
using namespace sysync;

/* globals */


#endif // SYSYNC_H

