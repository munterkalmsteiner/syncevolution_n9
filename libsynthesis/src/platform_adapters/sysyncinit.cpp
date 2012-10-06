/*
 *  File:         sysyncinit.cpp
 *
 *  Global variables instantiation
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 *  2004-08-09 : luz : created
 *
 *
 */


#include "prefix_file.h"

#define _IMPLEMENTS_DEBUG_GLOBALS
#include "sysync.h"
#undef _IMPLEMENTS_DEBUG_GLOBALS

// note: this is the generic implementation assuming an environment that supports
//       static global vars.

namespace sysync {

#ifdef DIRECT_APPBASE_GLOBALACCESS

/* %%% obsolete
// globals for debugging purposes
// Note: These are duplicates of the values in TSyncAppBase.fRootConfig.fDebugConfig
//       which are here for convenience
#ifdef SYDEBUG
uInt16 gDebug; // if <>0 (and #defined SYDEBUG), debug output is generated, value is used as mask
bool gGlobalDebugLogs; // if set, global debug output is generated
bool gMsgDump; // if set (and #defined MSGDUMP), messages sent and received are logged;
bool gXMLtranslate; // if set, communication will be translated to XML and logged
bool gSimMsgRead; // if set (and #defined SIMMSGREAD), simulated input with "i_" prefixed incoming messages are supported
bool gSeparateSessionLogs; // if set, session-specific logs are generated in separate files
string gDebugLogPath; // path to store debug files
#endif
*/

// static global for anchoring singluar syncappbase object
void *gSySyncGlobAnchor=NULL;


// get the global anchor for the sysync framework
void *sysync_glob_anchor(void)
{
  return gSySyncGlobAnchor;
} // sysync_glob_anchor

// get the global anchor for the sysync framework
void sysync_glob_setanchor(void *aAnchor)
{
  gSySyncGlobAnchor=aAnchor;
} // sysync_glob_setanchor


// global initialisation routine
void sysync_glob_init(void)
{
  gSySyncGlobAnchor=NULL;
} // sysync_glob_init

// global de-initialisation routine
void sysync_glob_deinit(void)
{
  // get rid of global stuff outside the SyncAppBase framework
  /* none here */
} // sysync_glob_deinit

#endif // DIRECT_APPBASE_GLOBALACCESS


} // namespace sysync

/* eof */
