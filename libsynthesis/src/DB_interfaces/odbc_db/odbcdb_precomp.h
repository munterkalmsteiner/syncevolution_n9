/*
 *  precompiled/precompilable headers
 */

#ifndef ODBCDB_PRECOMP_H
#define ODBCDB_PRECOMP_H


/* Specific headers for ODBC DB interface
 */


#ifdef ODBCAPI_SUPPORT
  // Real ODBC support, include headers
  #ifdef _WIN32

    #include <windows.h>
    #include <odbcinst.h>
    #include <sqlext.h>

  #else
    // ODBC HEADERS (MacOSX and Linux: in /usr/include)
    #include <sql.h>
    #include <sqlext.h>

    #ifdef OBDC_UNICODE
    // unicode ODBC
    #include <sqlucode.h>
    #endif

    // it seems that SQLLEN is not defined on all platforms, in particular, Mac OS X
    #ifdef MACOSX
    #ifndef SQLLEN
    #define SQLLEN SQLINTEGER
    #endif // SQLLEN
    #endif // MACOSX

  #endif // not _WIN32

#else
  // Some fake definitions to make SQLite-only versions compile w/o having ODBC headers

  #define HSTMT long
  #define SQLHSTMT HSTMT
  #define SQL_NULL_HANDLE 0
  #define SQLRETURN long
  #define SQL_NULL_DATA (-1)
  #define SQLLEN long
  #define SQL_SUCCESS 0

#endif // ODBCAPI_SUPPORT


#endif // ODBCDB_PRECOMP_H

/* eof */
