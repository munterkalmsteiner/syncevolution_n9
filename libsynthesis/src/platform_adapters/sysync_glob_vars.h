/*
 *  File:         sysync_glob_vars.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Global variables
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2004-08-09 : luz : created
 *
 */

#ifndef SYSYNC_GLOB_VARS_H
#define SYSYNC_GLOB_VARS_H

#include "generic_types.h"

namespace sysync {

#ifdef DIRECT_APPBASE_GLOBALACCESS

// init and deinit global sysync stuff
void sysync_glob_init(void);
void sysync_glob_deinit(void);
void *sysync_glob_anchor(void);
void sysync_glob_setanchor(void *aAnchor);

#else // DIRECT_APPBASE_GLOBALACCESS

// define dummies
#define sysync_glob_init()
#define sysync_glob_deinit()

#endif // not DIRECT_APPBASE_GLOBALACCESS

} // namespace sysync

#endif // SYSYNC_GLOB_VARS_H

// eof

