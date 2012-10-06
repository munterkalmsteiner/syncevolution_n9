/*
 *  File:         profiling.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Platform-dependent profiling implementation
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 *  2002-10-01 : luz : created
 *
 */


#ifndef _PROFILING_H
#define _PROFILING_H

#ifdef __cplusplus

//#include "sysync.h"
#include "generic_types.h"
#include "lineartime.h"
#include "sysync_globs.h"

#endif

#ifdef TIME_PROFILING

#ifdef __cplusplus

typedef enum {
  TP_general,
  TP_database,
  TP_configread,
  TP_scripts,
  TP_dbgout,
  TP_none, // does not count
} TTP_Types;
const sInt16 numTPTypes = TP_none-TP_general; // do not count TP_none

extern const char * const TP_TypeNames[numTPTypes];


// a profiling timer
typedef struct {
  long long kerneltime;
  long long usertime;
} TTP_Timer;

// profiling info packet
typedef struct {
  TTP_Timer fTP_last;
  TTP_Timer fTP_total;
  TTP_Timer fTP_times[numTPTypes];
  bool fTP_started;
  TTP_Types fTP_lastindex;
  lineartime_t fTP_startrealtime;
} TTP_info;


// opaque pointer
typedef void *TTP_infoP;

// initialize time profiling
void TP_Init(TTP_infoP aTpinfoP);

// start profiling a certain part of the code
TTP_Types TP_Start(TTP_infoP aTpinfoP, TTP_Types aTimerIdx);

// stop profiling
void TP_Stop(TTP_infoP aTpinfoP);

// get profiled information
uInt32 TP_GetSystemMS(TTP_infoP aTpinfoP,TTP_Types aTimerIdx=TP_none);
uInt32 TP_GetUserMS(TTP_infoP aTpinfoP,TTP_Types aTimerIdx=TP_none);
uInt32 TP_GetRealtimeMS(TTP_infoP aTpinfoP);

#endif // C++


// active macros
#define TP_DEFINFO(x) TTP_info x;
#define TP_DEFIDX(i) TTP_Types i
#define TP_INIT(i) TP_Init(&i)
#define TP_START(i,x) { if (PDEBUGMASK & DBG_PROFILE) TP_Start(&i,x); }
#define TP_SWITCH(l,i,x) { if (PDEBUGMASK & DBG_PROFILE) l=TP_Start(&i,x); }
#define TP_STOP(i) { if (PDEBUGMASK & DBG_PROFILE) TP_Stop(&i); }
#define TP_GETSYSTEMMS(i,x) TP_GetSystemMS(&i,x)
#define TP_GETUSERMS(i,x) TP_GetUserMS(&i,x)
#define TP_GETTOTALSYSTEMMS(i) TP_GetSystemMS(&i)
#define TP_GETTOTALUSERMS(i) TP_GetUserMS(&i)
#define TP_GETREALTIME(i) TP_GetRealtimeMS(&i)


#else // TIME_PROFILING

// dummy macros
#define TP_DEFINFO(x)
#define TP_DEFIDX(x)
#define TP_INIT(i)
#define TP_START(i,x)
#define TP_SWITCH(l,i,x)
#define TP_STOP(i)
#define TP_GETSYSTEMMS(i,x) 0
#define TP_GETUSERMS(i,x) 0
#define TP_GETTOTALSYSTEMMS(i) 0
#define TP_GETTOTALUSERMS(i) 0
#define TP_GETREALTIME(i) 0


#endif // TIME_PROFILING


#ifdef MEMORY_PROFILING

#warning "Probably obsolete and dangerous"

#ifdef __cplusplus
#define SYSYNC_CDECL "C"
#else
#define SYSYNC_CDECL
#endif


extern SYSYNC_CDECL size_t gAllocatedMem;
extern SYSYNC_CDECL size_t gMaxAllocatedMem;
extern SYSYNC_CDECL int gMemProfilingInUse;

extern SYSYNC_CDECL void *sysync_malloc(size_t size);
extern SYSYNC_CDECL void *sysync_realloc(void *mem, size_t newsize);
extern SYSYNC_CDECL void sysync_free(void *mem);

#ifdef __cplusplus

void* operator new (std::size_t size) throw(std::bad_alloc);
void* operator new (std::size_t size, const std::nothrow_t&) throw();

void* operator new[](std::size_t size) throw(std::bad_alloc);
void* operator new[](std::size_t size, const std::nothrow_t&) throw();

void  operator delete(void* ptr) throw();
void  operator delete(void* ptr, const std::nothrow_t&) throw();

void  operator delete[](void* ptr) throw();
void  operator delete[](void* ptr, const std::nothrow_t&) throw();


#ifdef MP_SHOW_NEW_AND_DELETE

#define MP_NEW(p,lvl,msg,x) {\
  size_t MP_before=gAllocatedMem;\
  if (!gMemProfilingInUse) {\
    gMemProfilingInUse=1;\
    PDEBUGPRINTFX((lvl|DBG_PROFILE),(\
      "+++ %-30s : before calling new:         %10ld total, %10ld max",\
      msg,\
      gAllocatedMem,\
      gMaxAllocatedMem\
    ));\
    gMemProfilingInUse=0;\
  }\
  p = new x;\
  if (!gMemProfilingInUse) {\
    gMemProfilingInUse=1;\
    PDEBUGPRINTFX((lvl|DBG_PROFILE),(\
      "+++ %-30s : %10ld bytes allocated, %10ld total, %10ld max",\
      msg,\
      gAllocatedMem-MP_before,\
      gAllocatedMem,\
      gMaxAllocatedMem\
    ));\
    gMemProfilingInUse=0;\
  }\
}

#define MP_RETURN_NEW(t,lvl,msg,x) {\
  t *obj;\
  MP_NEW(obj,lvl,msg,x);\
  return obj;\
}

#define MP_DELETE(lvl,msg,x) {\
  size_t MP_before=gAllocatedMem;\
  if (!gMemProfilingInUse) {\
    gMemProfilingInUse=1;\
    PDEBUGPRINTFX((lvl|DBG_PROFILE),(\
      "--- %-30s : before calling delete:      %10ld total, %10ld max",\
      msg,\
      gAllocatedMem,\
      gMaxAllocatedMem\
    ));\
    gMemProfilingInUse=0;\
  }\
  delete x;\
  if (!gMemProfilingInUse) {\
    gMemProfilingInUse=1;\
    PDEBUGPRINTFX((lvl|DBG_PROFILE),(\
      "--- %-30s : %10ld bytes freed,     %10ld total, %10ld max",\
      msg,\
      MP_before-gAllocatedMem,\
      gAllocatedMem,\
      gMaxAllocatedMem\
    ));\
    gMemProfilingInUse=0;\
  }\
}

#else // MP_SHOW_NEW_AND_DELETE

// let new and delete only count, but not show
#define MP_NEW(p,lvl,msg,x) p = new x
#define MP_RETURN_NEW(t,lvl,msg,x) return new x
#define MP_DELETE(lvl,msg,x) delete x

#endif // MP_SHOW_NEW_AND_DELETE


#define MP_SHOWCURRENT(lvl,msg) \
  PDEBUGPRINTFX((lvl|DBG_PROFILE),(\
    "=== %-30s : Current memory usage:       %10ld total, %10ld max",\
    msg,\
    gAllocatedMem,\
    gMaxAllocatedMem\
  ))

#endif

#else // MEMORY_PROFILING

#define sysync_malloc(m) malloc(m)
#define sysync_realloc(m,n) realloc(m,n)
#define sysync_free(m) free(m)

#ifdef __cplusplus

#define MP_NEW(p,lvl,msg,x) p = new x
#define MP_RETURN_NEW(t,lvl,msg,x) return new x
#define MP_DELETE(lvl,msg,x) delete x
#define MP_SHOWCURRENT(lvl,msg)

#endif

#endif // MEMORY_PROFILING


#endif // _PROFILING_H

/* eof */
