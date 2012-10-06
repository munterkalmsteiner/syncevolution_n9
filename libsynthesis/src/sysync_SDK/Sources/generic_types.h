/* generic types to avoid int size dependencies */
/* ============================================ */

/* NOTE: this file is part of the mod_sysync source files. */

/* NOTE: As this file should work for all kind of plain C compilers
 *       comments with double slashes "//" must be avoided !!
 */

#ifndef GENERIC_TYPES_H
#define GENERIC_TYPES_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(HAVE_STDINT_H)
# include <stdint.h>
#endif

#ifndef __WORDSIZE
/* 64 platforms should have this defined, so assume that */
/* platforms without the define are 32 bit. */
# define __WORDSIZE 32
#endif


#ifdef __cplusplus
  namespace sysync {
#endif

#if defined(HAVE_STDINT_H)
typedef uint64_t uInt64;
typedef int64_t sInt64;
typedef uint32_t uInt32;
typedef int32_t sInt32;
typedef uint16_t uInt16;
typedef int16_t sInt16;
typedef uint8_t uInt8;
typedef int8_t sInt8;

typedef uintptr_t uIntPtr;
typedef intptr_t  sIntPtr;
typedef uintptr_t bufferIndex; /* index into app buffers (small platforms may have 16bit here) */
typedef uintptr_t stringIndex; /* index into string (small platforms may have 16bit here) */
typedef uintptr_t stringSize;  /* size of a string object */
typedef uintptr_t memSize;     /* size of a memory buffer */

#else

/* defined size types */
/* - integers */
#ifdef _MSC_VER
typedef unsigned __int64 uInt64;
typedef signed __int64 sInt64;
#else
typedef unsigned long long uInt64;
typedef signed long long sInt64;
#endif

/* machine size integer, at least 32 bits */
typedef unsigned long uIntArch;
typedef signed long sIntArch;


/* TODO, FIXME %%%% for now, we need uInt32 to be 64bit actually */
/*#define UINT32_HAS_64BITS_ON_ARCH64 1 */


#if defined(UINT32_HAS_64BITS_ON_ARCH64) || defined(__PALM_OS__) || (__WORDSIZE < 32)
/* must use long to get a 32bit int (but gets 64bit in 64-bit architectures) */
typedef  unsigned long uInt32;
typedef  signed   long sInt32;
#ifdef UINT32_HAS_64BITS_ON_ARCH64
  #warning "%%% UINT32_HAS_64BITS_ON_ARCH64 - which means that uInt32 is really 64bit: UGLY & DANGEROUS, please be careful"
#endif
#else
/* according to the ILP32/LP64 std for all other platforms, int is 32bit (and long is 64bit) */
typedef unsigned int uInt32;
typedef signed   int sInt32;
#endif

typedef unsigned short uInt16;
typedef signed short sInt16;
typedef unsigned char uInt8;
typedef signed char sInt8;

#if __WORDSIZE == 64
typedef unsigned long long uIntPtr;
typedef signed long long sIntPtr;
#else
typedef unsigned long uIntPtr;
typedef signed long sIntPtr;
#endif

/* - application integers */
typedef uIntArch bufferIndex; /* index into app buffers (small platforms may have 16bit here) */
typedef uIntArch stringIndex; /* index into string (small platforms may have 16bit here) */
typedef uIntArch stringSize;  /* size of a string object */
typedef uIntArch memSize;     /* size of a memory buffer */

#endif /* HAVE_STDINT_H */


/* undefined size types */
/* - application chars & pointers */
typedef char appChar;
typedef char *appCharP;
typedef const char *cAppCharP;
/* - application bool */
#ifdef __cplusplus
  typedef bool appBool;
  /* Just define application-specific true and false constants */
  #define appFalse false
  #define appTrue true
#else
  /* we have no appBool in C, as C++/C mixture could cause bad */
  /* surprises if types were differently defined in C and C++  */
  /* Just define application-specific true and false constants */
  #define appFalse 0
  #define appTrue 1
#endif

/* - application pointers */
typedef void * appPointer;
typedef const void * cAppPointer;

/* - bytes, byte pointers */
/*   Note: gcc does not like using "typedef uInt8 *UInt8P" for some strange reason */
typedef uInt8 *uInt8P;
typedef const uInt8 *cUInt8P;

/* - Context/Version variables */
typedef struct ContextType *CContext;
typedef unsigned long CVersion;

/* - ssize_t is not predefined for Windows CW or EVC */
#if defined _WIN32 && (!defined _MSC_VER || defined WINCE)
typedef sInt32 ssize_t;
#endif


#ifdef __cplusplus
  } // namespace
#endif

#endif /* GENERIC_TYPES_H */

/* eof */


