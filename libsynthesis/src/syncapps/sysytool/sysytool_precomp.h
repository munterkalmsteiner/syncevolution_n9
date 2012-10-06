/* Headers that might be available in precompiled form
 * (standard libraries, SyncML toolkit...)
 */

#ifndef SYSYTOOL_PRECOMP_H
#define SYSYTOOL_PRECOMP_H


// transport related includes
#include "xpt_server_precomp.h"

// DB interface related includes
#ifdef XML2GO_SUPPORT
  #include "xml2godb_precomp.h"
#endif
#ifdef SQL_SUPPORT
  #include "odbcdb_precomp.h"
#endif
#ifdef SDK_SUPPORT
  #include "plugindb_precomp.h"
#endif


#endif // SYSYTOOL_PRECOMP_H

// eof
