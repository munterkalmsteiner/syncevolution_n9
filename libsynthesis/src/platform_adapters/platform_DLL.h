/*
 *  File:    platform_DLL.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  General interface to access the routines
 *  of a DLL.
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 *
 */

#ifndef PLATFORM_DLL_H
#define PLATFORM_DLL_H

#include "target_options.h"
#include <stddef.h> // NULL
#include <string>

using namespace std;

// ------------------------------------------------------------------
// Error handler procedure type
typedef void  (*ErrReport)( void* ref, const char* aName );
typedef void (*ErrMReport)( void* ref, const char* aName, const char* aModName );


bool ConnectDLL( void* &aMod, const char* aModName, ErrReport aReport, void* ref= NULL );
/* Connect to <aModName>, result is <aMod> reference */
/* Returns 0, if successful */


bool DLL_Function( void* aMod, const char* aFuncName, void* &aFunc );
/* Get <aFunc> of <aFuncName> at <aMod> */
/* Returns 0, if available */


bool DisconnectDLL( void* aMod );
/* Disconnect <aMod>. Returns 0, if operation successful */


#endif /* PLATFORM_DLL_H */
/* eof */
