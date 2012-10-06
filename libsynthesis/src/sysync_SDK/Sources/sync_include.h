/*
 *  File:     sync_include.h
 *
 *  Authors:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  SDK include definitions
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 */


#ifndef SYNC_INCLUDE_H
#define SYNC_INCLUDE_H

#include "target_options.h"
#include "generic_types.h"   /* some basic defs, which aren't available */
#include "engine_defs.h"
#include "syerror.h"         /* "syerror.h" uses "generic_types.h" */

#if !defined(__cplusplus) && !defined(__OBJC__)
  #if !defined(SYSYNC_ENGINE) || !defined(__MACH__) || defined(__GNUC__)
    typedef unsigned char bool;
    #define false 0
    #define true  1
  #endif
#endif

#if   !defined SYSYNC_ENGINE || !defined LINUX || defined __MACH__
  typedef unsigned long ulong;
#endif

#ifdef SYSYNC_ENGINE
  /* ==> if it is running within the SyncML engine */
  #ifdef __cplusplus
    #include "sysync.h"
    #include "platform_file.h"
  #else
    #if defined _MSC_VER
      #define NULL 0
    #endif

    #include "sysync_debug.h"
  #endif
#else
  /* ==> if running standalone, e.g. at a plug-in module */
  #include <stdio.h>           /* used for printf calls */
  #include <stdlib.h>          /* used for the malloc/free calls */
  #include <string.h>          /* used for strcpy/strlen calls */

  #ifdef __cplusplus
    #ifndef __MACH__           /* MACH: IOFBF/RAND_MAX duplicate problem */
      #include <string>        /* STL includes */
      #include <list>
    #endif

    #ifdef __GNUC__
      #include <typeinfo>
    #else
      #include <typeinfo.h>  /* type_info class */
    #endif

    using namespace std;
  #endif

  #if defined __MACH__ && !defined __GNUC__ /* used for va_list support */
    #include <mw_stdarg.h>
  #else
    #include <stdarg.h>
  #endif
#endif

#ifdef DBAPI_FILEOBJ
  #ifndef FILEOBJ_SUPPORT
  #define FILEOBJ_SUPPORT 1
  #endif
  
  #define PLATFORM_FILE   1
#endif

/* JAVA native interface JNI */
#ifdef JNI_SUPPORT
  #include <jni.h>

//#ifdef MACOSX
//  #include <JavaVM/jni.h>
//#else
//  #include <jni.h>
//#endif
#endif


#endif /* SYNC_INCLUDE_H */
/* eof */
