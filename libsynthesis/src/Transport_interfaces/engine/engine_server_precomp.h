/* Headers that might be available in precompiled form
 * (standard libraries, SyncML toolkit...)
 */

#ifndef ENGINE_SERVER_PRECOMP_H
#define ENGINE_SERVER_PRECOMP_H


/* precompiled portion for SySync Core */
#include "sysync_precomp.h"


#ifdef __EPOC_OS__
  // Symbian/EPOC: 
#elif __INTEL__
  // Windows32: nothing special
#elif defined(__MC68K__)
  // PalmOS: nothing special
#elif defined(LINUX)
  // Linux: nothing special
#elif defined(MACOSX)
  // MacOSX: nothing special
#elif defined(WINCE)
  // WinCE/PocketPC: nothing special
#elif defined(_MSC_VER)
  // Visual C++: nothing special
#else
  #error "Engine Client is Win32/PalmOS/MacOS/PocketPC/Linux only at this time"
#endif


#endif // ENGINE_SERVER_PRECOMP_H

// eof
