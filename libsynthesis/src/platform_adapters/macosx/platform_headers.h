/*
 *  File:         platform_headers.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Common include files for all platform-related standard headers
 *  (suitable for precompiled headers and/or prefix file)
 *
 *  Copyright (c) 2003-2011 by Synthesis AG + plan44.ch
 *
 *  2003-08-12 : luz : created
 *
 */

#ifndef __PLATFORM_HEADERS_H
#define __PLATFORM_HEADERS_H

// ANSI C
#ifndef __GNUC__
#include "MacHeadersMach-O.c"
#endif

// for the pipes
#include <sys/stat.h>
#include <unistd.h>

// C++
#ifdef __cplusplus

  // These are the C++ MSL headers
  #ifndef __GNUC__
  #include "MSLHeaders++.cp"
  #endif

#endif

#endif