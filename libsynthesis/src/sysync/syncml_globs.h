/*
 *  File:         syncml_globs.h
 *
 *  Authors:      Lukas Zeller (luz@plan44.ch)
 *                Beat Forster (bfo@synthesis.ch)
 *
 *  Global SyncML definitions/macros/constants
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef SYNCML_GLOBS_H
#define SYNCML_GLOBS_H

#include "generic_types.h"

#ifdef __cplusplus

#if defined _MSC_VER && !defined WINCE
  #include <windows.h>
#endif

// Consistent support for Linux, MacOSX CW & XCode & Visual Studio
#if defined __GNUC__ || defined _MSC_VER
  #include <ctype.h>
#else
  #include <ctype>
#endif

// we need the namespace std
using namespace std;
namespace sysync {
#endif

// basic SyncML constants
#define SYNCML_HDRCMDNAME "SyncHdr"

// SyncML Encodings (to be appended to SYNCML_MIME_TYPE and SYNCML_DEVINF_META_TYPE
#define SYNCML_ENCODING_XML "xml"
#define SYNCML_ENCODING_WBXML "wbxml"
// - spearator between MIME-Type and encoding string
#define SYNCML_ENCODING_SEPARATOR "+"

// SyncML content type constants
#define SYNCML_MIME_TYPE "application/vnd.syncml" // plus encoding

// SyncML charset
#define SYNCML_DEFAULT_CHARSET "utf-8"

// prefix for relative URIs
#define URI_RELPREFIX "./"

// SyncML DEVINF constants
#define SYNCML_DEVINF_LOCNAME "Device Information"
#define SYNCML_DEVINF_META_TYPE "application/vnd.syncml-devinf" // plus encoding !
#define SYNCML_META_VERSION "syncml:metinf"

// SyncML Filter grammars
#define SYNCML_FILTERTYPE_CGI "syncml:filtertype-cgi"
#define SYNCML_FILTERTYPE_CGI_VERS "1.0"
#define SYNCML_FILTERTYPE_INCLUSIVE "INCLUSIVE"
#define SYNCML_FILTERTYPE_EXCLUSIVE "EXCLUSIVE"


// SyncML encodings
// Note: SmlEncoding_t is defined in the RTK smldef.h
#define numSyncMLEncodings (SML_XML-SML_UNDEF+1)

#ifdef __cplusplus
} // namespace sysync
#endif

#endif // SYNCML_GLOBS_H

// eof

