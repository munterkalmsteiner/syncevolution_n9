/*
 *  File:         platform_pipe.c
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Platform specific utility routines
 *
 *  Copyright (c) 2003-2011 by Synthesis AG + plan44.ch
 *
 *  NOTE: this file is part of the mod_sysync source files.
 *
 */

#include <fcntl.h>
#include "platform_headers.h"
#include "platform_pipe.h"


// create new pipe. Fails if it already exists
int createPipe(const char *aPipePathName)
{
  if (mkfifo(aPipePathName,0620)!=0) {
    // error
    return appFalse;
  }
  // ok
  return appTrue;
} // createPipe


int deletePipe(const char *aPipePathName)
{
  if (unlink(aPipePathName)!=0) {
    // error
    return appFalse;
  }
  // ok
  return appTrue;
} // deletePipe


pipeDescriptor_t openPipeForRead(const char *aPipePathName)
{
  return open(aPipePathName, O_RDONLY);
} // openPipeForRead


pipeDescriptor_t openPipeForWrite(const char *aPipePathName)
{
  return open(aPipePathName, O_WRONLY);
} // openPipeForWrite


int closePipe(pipeDescriptor_t aPipeDesc)
{
  return (close(aPipeDesc)!=0);
} // closePipe


long readPipe(pipeDescriptor_t aPipeDesc,void *aBuffer, long aMaxBytes, int aReadAll)
{
  long gotBytes=0;
  long bytes;
  do {
    bytes=read(aPipeDesc,aBuffer,aMaxBytes);
    if (bytes<0) return bytes; // error
    if (bytes==0) return gotBytes; // no error, but end of pipe - abort reading and return number of bytes already read
    gotBytes+=bytes; // add them up
    aMaxBytes-=bytes; // reduce max
    aBuffer = (void *)((char *)aBuffer + bytes);
  } while(aReadAll && aMaxBytes>0);
  return gotBytes; // return total
} // readPipe


long writePipe(pipeDescriptor_t aPipeDesc,void *aBuffer, long aNumBytes)
{
  long sentBytes=0;
  long bytes;
  do {
    bytes=write(aPipeDesc,aBuffer,aNumBytes);
    if (bytes<0) return bytes; // error
    sentBytes+=bytes; // add them up
    aNumBytes-=bytes; // reduce max
    aBuffer = (void *)((char *)aBuffer + bytes);
  } while(aNumBytes>0);
  return sentBytes; // return total
} // writePipe


/* eof */
