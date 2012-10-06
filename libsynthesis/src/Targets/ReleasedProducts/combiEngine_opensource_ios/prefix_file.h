#ifndef __PREFIX_FILE_H
#define __PREFIX_FILE_H

#ifdef __cplusplus
  // include all headers that are suitable for precompiled use
  // - target options can incfluence everything
  #include "target_options.h"
  // - precompilable headers
  #include "serverengine_custom_precomp.h"
#else
  // include all headers that are suitable for precompiled
  // C version use
  // - target options can incfluence everything
  #include "target_options.h"
  // - SML toolkit WITHOUT xpt part
  #include "smltk_precomp.h"
#endif

#endif