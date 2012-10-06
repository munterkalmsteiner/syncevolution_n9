/* 
 *  File:    target_options.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *  Programming interface between Synthesis SyncML engine 
 *  and a database structure.
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef TARGET_OPTIONS_H
#define TARGET_OPTIONS_H


/* - find out target platform */
#ifdef __MACH__
  #define MACOSX
#else
  #if defined __MWERKS__ || defined _MSC_VER
    #ifndef   _WIN32
      #define _WIN32
    #endif
    
    #ifndef    WIN32
      #define  WIN32
    #endif
  #else
    #define LINUX
  #endif
#endif


/* code is running within DLL */
#define SDK_DLL 1

/* - we are not at the SyncML engine's side here */
/* - and we need an extern "C" interface for the DLL */
#undef SYSYNC_ENGINE


/* activate debug output */
#define SYDEBUG 2


/* use internal BLOB and ADMIN implementation */
//#define DISABLE_PLUGIN_DATASTOREADMIN 1
//#define DISABLE_PLUGIN_BLOBS          1


#endif /* TARGET_OPTIONS_H */
/* eof */
