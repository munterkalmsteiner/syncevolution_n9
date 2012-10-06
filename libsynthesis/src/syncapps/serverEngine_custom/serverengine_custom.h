#ifndef SERVERENGINE_CUSTOM_H
#define SERVERENGINE_CUSTOM_H

/* Headers that might be available in precompiled form
 * (standard libraries, SyncML toolkit...)
 */
#include "serverengine_custom_precomp.h"

/* headers not suitable for / entirely included in precompilation */

// DB interface related includes
#ifdef SQL_SUPPORT
  #include "odbcdb.h"
#endif
#ifdef SDK_SUPPORT
  #include "plugindb.h"
#endif
#ifdef OUTLOOK_SUPPORT
  #include "outlookdb.h"
#endif


#endif // SERVERENGINE_CUSTOM_H
