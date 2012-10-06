/*
 *  File:         platform_exec.c
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Platform specific implementation for executing external commands
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 *  2004-07-21 : luz : created
 *
 */

#include "platform_exec.h"

#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>

// returns -1 if command could not be started, exit code of the command otherwise.
// if used with aBackground==true, the return code is always 0 in case
sInt32 shellExecCommand(cAppCharP aCommandName, cAppCharP aCommandParams, int aBackground)
{
  int status;
  uInt32 cmdlen;
  char *cmd;

  if (!aCommandName || *aCommandName==0) return 0; // no command -> exec "successful"
  // simply use system()
  // - prepare command line
  cmdlen=
    strlen(aCommandName) +
    (aCommandParams ? strlen(aCommandParams) : 0) +
    2; // for separator and terminator
  cmd= (char*)malloc(cmdlen);
  strcpy(cmd,aCommandName);
  if (aCommandParams) {
    strcat(cmd," ");
    strcat(cmd,aCommandParams);
  }
  // - execute
  status=system(cmd);
  free(cmd);
  return status;

} // shellExecCommand


/* eof */