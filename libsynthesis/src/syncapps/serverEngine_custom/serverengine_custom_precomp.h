/* Headers that might be available in precompiled form
 * (standard libraries, SyncML toolkit...)
 *
 */ 

#ifndef SERVERENGINE_CUSTOM_PRECOMP_H
#define SERVERENGINE_CUSTOM_PRECOMP_H

// Needed for Engine 
#include "sysync_precomp.h"

// DB interface related includes
#ifdef SQL_SUPPORT
  #include "odbcdb_precomp.h"
#endif
#ifdef SDK_SUPPORT
  #include "plugindb_precomp.h"
#endif
#ifdef OUTLOOK_SUPPORT
  #include "outlookdb_precomp.h"
#endif


#endif // SERVERENGINE_CUSTOM_PRECOMP_H

// eof
