/* MD5.H - header file for MD5C.C
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
 */

/* 2001-05-29: adapted to MW-C and ANSI-C by luz */


/* originally GLOBAL.H - RSAREF types and constants
 * now moved here to keep everything in namespace
 */

// define this to add test functions String() and Print()
// #define MD5_TEST_FUNCS 1

#ifndef SYSYNC_MD5_H
#define SYSYNC_MD5_H


#include "generic_types.h"

using namespace sysync;

namespace md5 {

/* POINTER defines a generic pointer type */
typedef uInt8 *SYSYNC_POINTER;

/* UINT2 defines a two byte word */
typedef uInt16 SYSYNC_UINT2;

/* UINT4 defines a four byte word */
/* Note: "typedef uInt32 SYSYNC_UINT4;" is unreliable as uInt32 was 64bit in some weird builds */
#if !defined(__WORDSIZE) || (__WORDSIZE < 32)
typedef unsigned long SYSYNC_UINT4;
#else
typedef unsigned int SYSYNC_UINT4;
#endif 

/* MD5 context. */
typedef struct {
  SYSYNC_UINT4 state[4];                                   /* state (ABCD) */
  SYSYNC_UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  uInt8 buffer[64];                         /* input buffer */
  uInt8 PADDING[64];  /* padding space */
} SYSYNC_MD5_CTX;

/* MD5 functions */
void Init (SYSYNC_MD5_CTX *);
void Update (SYSYNC_MD5_CTX *, const uInt8 *, uInt32);
void Final (uInt8 [16], SYSYNC_MD5_CTX *);

#ifdef MD5_TEST_FUNCS
/* for test */
void String (const char *aString, char *s);
void Print (uInt8 *digest, char * &s);
void dotest(void);
#endif

} // end namespace md5

#endif // SYSYNC_MD5_H

/* eof */
