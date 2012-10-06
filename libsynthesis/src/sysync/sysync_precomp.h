/* Headers that might be available in precompiled form
 * (standard libraries, SyncML toolkit...)
 */

#ifndef SYSYNC_PRECOMP_H
#define SYSYNC_PRECOMP_H

/* global includes */
#include "target_options.h"

/* standard C includes */
#ifdef __PALM_OS__
  // don't use the *.h versions! they don't work any more with CW Palm v9
  #include <ctype>
  #include <cstdarg>
  #include <cstdio>
  #include <cstring>
  #include <ctime>
#else
  #include <ctype.h>
  #include <stdarg.h>
  #include <stdio.h>
  #include <string.h>
  #if !defined(WINCE) && !defined(MACOSX)
    #include <time.h> // BCPPB
  #endif
#endif

/* C++/STL includes */
/* - RTTI */
#if !defined(WINCE) && !defined(__EPOC_OS__)
  #include <typeinfo>
#endif
#if __MC68K__
  #warning "STL headers excluded from precompiled headers due to problems with CW Palm v9"
#else
  /* - STL */
  #include <string>
  #include <vector>
  #include <map>
  #include <list>
#endif

#if defined(LINUX) || defined(__EPOC_OS__)
  // gcc in standard distrs lacks the <limits> STL header
  #include <limits.h>
#else
  #include <limits>
#endif


#endif // defined SYSYNC_PRECOMP_H
