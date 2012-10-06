/*
 *  File:    target_options.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch),
 *           Patrick Ohly (patrick.ohly@intel.com)
 *
 *  Programming interface between Synthesis SyncML engine
 *  and a database structure or client.
 *
 *  Options for the client side which links against
 *  libsynthesissdk.
 *
 *  Copyright (c) 2009-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef TARGET_OPTIONS_H
#define TARGET_OPTIONS_H


/* - find out target platform */
#if defined (__MACH__) && defined(__APPLE__)
  #define MACOSX
#else
  #if defined __MWERKS__ || defined _MSC_VER
    #ifndef _WIN32
      #define _WIN32
    #endif
  #else
    #define LINUX
  #endif
#endif


/* - we are not at the SyncML engine's side here */
#undef  SYSYNC_ENGINE

/* - but we link directly to the module */
#define DBAPI_LINKED 1

/*
 * The libsynthesis shared library uses SySync_ as prefix for C
 * functions. TEngineModuleBridge checks for the name with and without
 * the prefix, so clients are compatible with the current shared
 * libraries and (potentially older) commercial releases.
 */
#define SYSYNC_EXTERNAL(_x) SySync_ ## _x
#define SYSYNC_PREFIX "SySync_"

/* activate debug output */
#define SYDEBUG 2

#define NOWSM 1

#endif /* TARGET_OPTIONS_H */
/* eof */
