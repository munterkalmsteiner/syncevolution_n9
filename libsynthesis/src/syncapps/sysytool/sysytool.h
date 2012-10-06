/* syncserver xpt generic header file */

#ifndef SYSYTOOL_H
#define SYSYTOOL_H

/* Headers that might be available in precompiled form
 * (standard libraries, SyncML toolkit...)
 */
#include "sysytool_precomp.h"

/* headers not suitable for / entirely included in precompilation */

// transport related includes
#include "xpt_server.h"

// DB interface related includes
#ifdef XML2GO_SUPPORT
  #include "xml2godb.h"
#endif
#ifdef SQL_SUPPORT
  #include "odbcdb.h"
#endif
#ifdef SDK_SUPPORT
  #include "plugindb.h"
#endif

#endif // SYSYTOOL_H
