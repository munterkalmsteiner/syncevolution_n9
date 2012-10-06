/* common include file for all PCRE sources
 * Added by luz 2007-09-03 to allow more flexibility for
 * multi-platform static linking with platforms w/o config.h
 * mechanisms
 */

/* Note: target_options may define PCRE_HAS_CONFIG_H if environment
 * has generic config.h that works for PCRE and does not conflict
 * with other components.
 */  

#include "target_options.h" // to get platform defines

#ifdef PCRE_HAS_CONFIG_H
  #include <config.h>
#else
  
  #if defined(WIN32)
    /* hand-modified win32 config.h! */
    #include "pcre_config_win32.h"
  #elif defined(MACOSX)
    /* hand-modified MacOS config.h! */
    #include "pcre_config_macosx.h"
  #elif defined(ANDROID)
    /* hand-modified MacOS config.h! */
    #include "pcre_config_android.h"
  #else
    #error "PCRE not yet prepared for this platform" 
  #endif


#endif /* not PCRE_HAS_CONFIG_H */
