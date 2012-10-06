/* Headers that might be available in precompiled form
 * (standard libraries, SyncML toolkit...)
 */

#ifndef SYSYNC_PRECOMP_XPT_H
#define SYSYNC_PRECOMP_XPT_H


// include what is needed anyway for sysync...
#include "sysync_precomp.h"

// ...plus XPT SyncML Toolkit includes
// - SyncML Toolkit external API
extern "C" {
  #include "xpt.h"
}

#endif // defined SYSYNC_PRECOMP_XPT_H
