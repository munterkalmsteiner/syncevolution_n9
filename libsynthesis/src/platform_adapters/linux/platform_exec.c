/*
 *  File:         platform_exec.c
 *
 *  Author:			  Lukas Zeller (luz@plan44.ch)
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
#include <stdlib.h>
#include <string.h>


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
  cmd=malloc(cmdlen);
  strcpy(cmd,aCommandName);
  if (aCommandParams) {
    strcat(cmd," ");
    strcat(cmd,aCommandParams);
  }
  // - execute
  status=system(cmd);
  free(cmd);
  return status;

  /* does not work ok
  // fork
  pid = fork();
  alarm(0); // make sure we have no alarm running
  if (pid == -1) return -1; // fork failed
  if (pid == 0) {
    // forked process
    // prepare command line
    cmdlen=
      strlen(aCommandName) +
      (aCommandParams ? strlen(aCommandParams) : 0) +
      2; // for separator and terminator
    cmd=malloc(cmdlen);
    strcpy(cmd,aCommandName);
    if (aCommandParams) {
      strcat(cmd," ");
      strcat(cmd,aCommandParams);
    }
    // prepare for execve
    argv[0] = "sh";
    argv[1] = "-c";
    argv[2] = cmd;
    argv[3] = 0;
    envp[1] = 0; // no environment
    execve("/bin/sh", argv, envp); // replace process, will exit with command's exit code
    exit(-1); // problem executing
  }
  else {
    // process that called fork()
    do {
      if (waitpid(pid, &status, 0) == -1) {
        // keep waiting if it's EINTR
        if (errno != EINTR)
          return -1;
      }
      else {
        // return exit status of executed process
        return status;
      }
    } while(1);
  }
  */
} // shellExecCommand


/* eof */
