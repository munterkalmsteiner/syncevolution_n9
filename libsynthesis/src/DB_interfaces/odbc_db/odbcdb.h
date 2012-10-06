/*
 *  Common include file for all ODBC_H sources
 */

#ifndef ODBCDB_H
#define ODBCDB_H


// precompiled portion
#include "odbcdb_precomp.h"

// derived defines and sanity checks
#ifdef ODBC_SUPPORT
  #error "ODBC_SUPPORT no longer exists, is now called SQL_SUPPORT. ODBCAPI_SUPPORT exists."
#endif
// SQL based admin
#if defined (SQL_SUPPORT) && defined(ODBCAPI_SUPPORT) && !defined(BINFILE_ALWAYS_ACTIVE)
  #define HAS_SQL_ADMIN 1
#endif


// other common includes
#include "sysync.h"

// Items used by ODBC DB interface
#include "multifielditem.h"
#include "mimediritemtype.h"

#define TEXTMAP_FILE_EXTENSION ".txt"
#define USERAUTH_FILE_EXTENSION ".txt"

#endif

/* eof */
