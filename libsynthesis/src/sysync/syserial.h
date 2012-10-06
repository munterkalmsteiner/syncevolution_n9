/*
 *  File:         syserial.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Serial number generator and checker
 *
 *  Copyright (c) 2003-2011 by Synthesis AG + plan44.ch
 *
 *  2003-02-11 : luz : created
 *
 */

#ifndef SYSERIAL_H
#define SYSERIAL_H

#include "generic_types.h"
#include <string>

using namespace std;

namespace sysync {


// variant codes
// =============

#define SYSER_VARIANT_UNKNOWN 0
#define SYSER_VARIANT_STD 1
#define SYSER_VARIANT_PRO 2
#define SYSER_VARIANT_CUSTOM 3
#define SYSER_VARIANT_DEMO 10


// branding codes
#define SYSER_EXTRA_ID_NONE 0
#define SYSER_EXTRA_ID_PROTO 1
#define SYSER_EXTRA_ID_DBG 2

#define SYSER_EXTRA_ID_GOOSYNC 10 // Toffa.com Goosync special version
#define SYSER_EXTRA_ID_FONELINK 11 // novamedia FoneLink special version
#define SYSER_EXTRA_ID_SOOCIAL 12 // soocial.com special version


// product codes
// =============

// PDA Clients
// - universal codes for all PDA clients = pocketPC codes
#define SYSER_PRODCODE_CLIENT_PDA_STD 2 // this is the former PocketPC STD code (most widespread)
#define SYSER_PRODCODE_CLIENT_PDA_PRO 5 // this is the former PocketPC PRO code (most widespread)

// Old platform specific PDA codes (still valid, but no longer required)
// - Palm & PPC STD
#define SYSER_PRODCODE_CLIENT_PALM_STD 1 // for PalmOS
#define SYSER_PRODCODE_CLIENT_PPC_STD  2 // for PocketPC
#define SYSER_PRODCODE_CLIENT_PALMPPC_STD  3 // combined for PPC and PalmOS
// - Palm & PPC PRO
#define SYSER_PRODCODE_CLIENT_PALM_PRO 4 // for PalmOS
#define SYSER_PRODCODE_CLIENT_PPC_PRO  5 // for PocketPC
#define SYSER_PRODCODE_CLIENT_PALMPPC_PRO  6 // combined for PPC and PalmOS
// - Symbian client
#define SYSER_PRODCODE_CLIENT_SYMBIAN_STD 18
#define SYSER_PRODCODE_CLIENT_SYMBIAN_PRO 19
// - Smartphone
#define SYSER_PRODCODE_CLIENT_MSSMP_STD 13 // for Microsoft SmartPhone (2003)
#define SYSER_PRODCODE_CLIENT_MSSMP_PRO 14 // for Microsoft SmartPhone (2003)


// - ODBC client STD
#define SYSER_PRODCODE_CLIENT_ODBC_STD_WIN32 7 // Win32 ODBC PRO
#define SYSER_PRODCODE_CLIENT_ODBC_STD_MACOSX 8 // for Mac OS X
#define SYSER_PRODCODE_CLIENT_ODBC_STD_LINUX 9 // for Linux
// - ODBC client PRO
#define SYSER_PRODCODE_CLIENT_ODBC_PRO_WIN32 10 // Win32 ODBC PRO
#define SYSER_PRODCODE_CLIENT_ODBC_PRO_MACOSX 11 // for Mac OS X
#define SYSER_PRODCODE_CLIENT_ODBC_PRO_LINUX 12 // for Linux


// - Demo client
#define SYSER_PRODCODE_CLIENT_DEMO 15 // Demo Client (Text only)

// - Outlook client
#define SYSER_PRODCODE_CLIENT_OUTLOOK_STD   16 // Outlook Client STD
#define SYSER_PRODCODE_CLIENT_OUTLOOK_PRO   17 // Outlook Client PRO (with email)

// - Client Libraries
#define SYSER_PRODCODE_CLIENT_LIB_WIN32     18 // Win32 ODBC PRO
#define SYSER_PRODCODE_CLIENT_LIB_MACOSX    19 // for Mac OS X
#define SYSER_PRODCODE_CLIENT_LIB_LINUX     20 // for Linux
#define SYSER_PRODCODE_CLIENT_LIB_SYMBIAN   21 // for Symbian
#define SYSER_PRODCODE_CLIENT_LIB_WM        22 // for Windows Mobile
#define SYSER_PRODCODE_CLIENT_LIB_PALM      23 // for PALMOS
#define SYSER_PRODCODE_CLIENT_LIB_IPHONEOS  28 // iPhone OS
#define SYSER_PRODCODE_CLIENT_LIB_ANDROID   40 // Android


#define SYSER_PRODCODE_CLIENT_LIB_ALL       24 // ALL Platforms
#define SYSER_PRODCODE_CLIENT_LIB_MOBILE    25 // ALL Mobile Platforms
#define SYSER_PRODCODE_CLIENT_LIB_DESK      26 // ALL Desktop Platforms

#define SYSER_PRODCODE_CLIENT_LIB_DEMO      27 // All DEMO Libraries


// - Client product flags (no flags -> only XPT version allowed)
#define SYSER_PRODFLAG_CLIENT_DMU 0x01 // DMU enabled
#define SYSER_PRODFLAG_CLIENT_APP 0x02 // App enabled (not only library)


// Servers

// - Server (=usually unified server+client) Libraries
#define SYSER_PRODCODE_SERVER_LIB_WIN32     39 // Win32 ODBC PRO
#define SYSER_PRODCODE_SERVER_LIB_MACOSX    29 // for Mac OS X
#define SYSER_PRODCODE_SERVER_LIB_LINUX     30 // for Linux
#define SYSER_PRODCODE_SERVER_LIB_SYMBIAN   31 // for Symbian
#define SYSER_PRODCODE_SERVER_LIB_WM        32 // for Windows Mobile
#define SYSER_PRODCODE_SERVER_LIB_PALM      33 // for PALMOS
#define SYSER_PRODCODE_SERVER_LIB_IPHONEOS  38 // iPhone OS
#define SYSER_PRODCODE_SERVER_LIB_ANDROID   41 // Android


#define SYSER_PRODCODE_SERVER_LIB_ALL       34 // ALL Platforms
#define SYSER_PRODCODE_SERVER_LIB_MOBILE    35 // ALL Mobile Platforms
#define SYSER_PRODCODE_SERVER_LIB_DESK      36 // ALL Desktop Platforms

#define SYSER_PRODCODE_SERVER_LIB_DEMO      37 // All DEMO Libraries

// - Demo
#define SYSER_PRODCODE_SERVER_DEMO 50 // Demo Server (Text only)
// - ODBC
#define SYSER_PRODCODE_SERVER_STD 51 // STD Server (with ODBC)
#define SYSER_PRODCODE_SERVER_PRO 52 // PRO Server (with ODBC)
// - XML2GO
#define SYSER_PRODCODE_SERVER_XML2GO 53 // xml2go Server (with ODBC and XML2GO)

// - Server product flags (no ISAPI or APACHE flags -> only XPT version allowed)
#define SYSER_PRODFLAG_SERVER_ISAPI 0x01 // ISAPI version
#define SYSER_PRODFLAG_SERVER_APACHE 0x02 // Apache version
#define SYSER_PRODFLAG_SERVER_SDKAPI 0x04 // external DB API plugins allowed


// special flag: if set, time code in license does not specify when temporary
// license expires, but for up to what release date (hard-coded into the binary)
// this code is valid. This allows to issue time unlimited licenses that will allow
// be used with new releases only up to a defined time period after issuing.
// If set, the duration bits (encoded absolute month) are no longer the
// expiry date, but the max release date supported.
// If this bit is set in SYSER_NEEDED_PRODUCT_FLAGS, this means that the
// license must either have the bit set, too, or the license must be a
// time limited license. Only licenses limited neither in time nor in release
// date will be rejected.
#define SYSER_PRODFLAG_MAXRELDATE 0x80


// license types
#define SYSER_LTYP_STANDARD 0   // standard license, nothing special
#define SYSER_LTYP_SYN_REG 1    // requires activation at synthesis
#define SYSER_LTYP_S2G_REG 2    // requires activation at space2go


// registration checking URLs
#define SYSER_SYN_REG_HOST "www.synthesis.ch"
#define SYSER_SYN_REG_DOC  "/reg/"
#define SYSER_S2G_REG_HOST "sync.space2go.com"
#define SYSER_S2G_REG_DOC  "/reg/"

// update checking URL
#define SYSER_SYN_UDC_HOST "www.synthesis.ch"
#define SYSER_SYN_UDC_DOC  "/udc/"



// Internals
// =========

// size of serial number
#define SYSER_SERIALNUM_SIZE 20 // 4*4 chars, plus 3 dashes, plus one terminator = 16+3+1 = 20
#define SYSER_SERIALNUM_MANGLED_SIZE 17 // dashes are optimized away, so 4*4+1 = 17
// max size of "name" string (only that much will be stored and tested)
#define SYSER_NAMESTRING_MAX 80 // should be enough for name and email


#ifdef LINUX
#define SYSER_CRC32_SEED ((uInt32)4119203362LL) // phone Tiefenau :-)
#else
#define SYSER_CRC32_SEED 4119203362 // phone Tiefenau :-)
#endif

// make sure we don't ever include the generator into a product
#ifndef SYSYNC_VERSION_MAJOR

// generate serial
void generateSySerial(
  char *outbuf, // must be able to receive SYSER_SERIALNUM_SIZE chars (including terminator)
  uInt8 productflags, uInt16 productcode, uInt8 licensetype, uInt16 quantity, uInt8 duration,
  const char *name,
  bool aIprevent
);

#endif

#if defined SYSER_REGISTRATION || !defined(SYSYNC_VERSION_MAJOR)

bool getSySerialInfo(
  const char *input,
  uInt8 &productflags, uInt16 &productcode, uInt8 &licensetype, uInt16 &quantity, uInt8 &duration,
  uInt32 &crc, uInt32 &infocrc,
  bool aMangled=false
);

#endif // SYSER_REGISTRATION


uInt32 addToCrc(uInt32 aCRC, uInt8 aByte);
uInt32 addNameToCRC(uInt32 aCRC, const char *aName, bool aMangled=false, uInt16 aMaxChars=32000);



} // namespace sysync

#endif // SYSERIAL_H

/* eof */
