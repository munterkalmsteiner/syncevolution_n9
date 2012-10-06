/*
 *  File:         enginestubs.c
 *
 *  Author:       Patrick Ohly
 *
 *  Dummy implementations of the engine entry points
 *  for client and server, to be linked statically against
 *  SDK instead of the real engine. The engine then has
 *  to be opened as module.
 *
 *  Copyright (c) 2009 by Synthesis AG (www.synthesis.ch)
 *
 */

#include "sync_dbapi.h"

ENGINE_ENTRY TSyError
SYSYNC_EXTERNAL(ConnectEngine)
    (UI_Call_In *aCI,
     CVersion   *aEngVersion,
     CVersion    aPrgVersion,
     uInt16      aDebugFlags
     ) ENTRY_ATTR
{
  return 404;
}

ENGINE_ENTRY TSyError
SYSYNC_EXTERNAL(DisconnectEngine)
    (UI_Call_In aCI) ENTRY_ATTR
{
  return 404;
}

ENGINE_ENTRY TSyError
SYSYNC_EXTERNAL_SRV(ConnectEngine)
   (UI_Call_In *aCI,
    CVersion   *aEngVersion,
    CVersion    aPrgVersion,
    uInt16      aDebugFlags
    ) ENTRY_ATTR
{
  return 404;
}

ENGINE_ENTRY TSyError
SYSYNC_EXTERNAL_SRV(DisconnectEngine)
    (UI_Call_In aCI) ENTRY_ATTR
{
  return 404;
}
