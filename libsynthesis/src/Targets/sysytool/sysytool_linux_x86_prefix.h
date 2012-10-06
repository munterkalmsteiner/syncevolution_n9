/* prefix file
 * ===========
 *
 */

#ifdef __cplusplus
  // include all headers that are suitable for precompiled use
  // - target options can incfluence everything
  #include "target_options.h"
  // - platform specifics
  #include "platform_headers.h"
  // - precompilable headers
  #include "sysytool_precomp.h"
#else
  // include all headers that are suitable for precompiled
  // C version use
  // - target options can incfluence everything
  #include "target_options.h"
  // - platform specifics
  #include "platform_headers.h"
  // - SML toolkit including xpt part
  #include "smltk_precomp_xpt.h"
#endif

/* eof */
