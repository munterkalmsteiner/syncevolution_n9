/*  
 *  File:    dbplugin_cocoa_wrapper.mm
 *
 *  Author:  Lukas Zeller (luz@plan44.ch)
 *
 *  Wrapper for creating statically linked base class for cocoa database plugins
 *
 *  Copyright (c) 2010 by Synthesis AG
 *
 */

// this makes dbplugin_cocoa.m compile as baseclass without C/C++ entry points for common use
// for all statically linked plugins.
// C++ entry points must be created for each plugin in separate namespaces - this is done by
// compiling the plugin wrappers or static-only .mm plugin source files
#define PLUGIN_BASECLASS 1

// the generic cocoa interface
#include "dbplugin_cocoa.m"


/* eof */