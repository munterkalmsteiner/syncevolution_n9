/*
 *  File:         platform_exec.h
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


#include <generic_types.h>

#ifdef __cplusplus
extern "C" {
#endif

// returns -1 if command could not be started, exit code of the command otherwise.
// if used with aBackground==true, the return code is always 0 in case
sInt32 shellExecCommand(cAppCharP aCommandName, cAppCharP aCommandParams, int aBackground);

#ifdef __cplusplus
}
#endif

/* eof */