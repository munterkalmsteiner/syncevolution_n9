/* C Headers that might be available in precompiled form
 * for SML toolkit compilation.
 * (standard libraries, SyncML toolkit itself...)
 */

#ifndef SMLTK_PRECOMP_H
#define SMLTK_PRECOMP_H


/* prerequisites for SML toolkit */
// - compiler/platform specific defines
#include "define.h"
// - special, platform dependent lock library
//   (may include extra platform support headers BEFORE
//   other SML files will include standard platform support)
#include "liblock.h"


/* standard C includes */
#include <ctype.h>
#include <string.h>
#ifdef __PALM_OS__
  #include <unix_stdarg.h>
  #include <stdlib.h>
  #include <time.h>
#else
  #include <stdio.h>
  #include <stdarg.h>
#endif
#if !defined(__MC68K__) && !defined(LINUX) && !defined(WINCE) && !defined(MACOSX) && !defined(__EPOC_OS__)
  #include <extras.h>
#endif


/* SyncML Toolkit includes */
// - SyncML Toolkit external API
#include "sml.h"
#include "smlerr.h"

#endif /* defined SMLTK_PRECOMP_H */
