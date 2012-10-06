/*
 *  File:         platform_pipe.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Platform specific pipe implementation
 *
 *  Copyright (c) 2003-2011 by Synthesis AG + plan44.ch
 *
 *  NOTE: this file is part of the mod_sysync source files.
 */

#include <errno.h>
#include "generic_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int pipeDescriptor_t;
#define PIPEDESCRIPTORVALID(pd) (pd>=0)
#define PIPEERRORCODE ((int)errno)
#define PIPEINVALIDDESC -1
#define PIPEPATHDELIM '/'

int createPipe(const char *aPipePathName);
int deletePipe(const char *aPipePathName);
pipeDescriptor_t openPipeForRead(const char *aPipePathName);
pipeDescriptor_t openPipeForWrite(const char *aPipePathName);
int closePipe(pipeDescriptor_t aPipeDesc);
long readPipe(pipeDescriptor_t aPipeDesc,void *aBuffer, long aMaxBytes, int aReadAll);
long writePipe(pipeDescriptor_t aPipeDesc,void *aBuffer, long aNumBytes);

#ifdef __cplusplus
}
#endif

/* eof */
