/*
 *  Include file to allow SyncML RTK to use progress callback
 *
 *  Copyright (c) 2003-2011 by Synthesis AG + plan44.ch
 */

#ifndef GLOBAL_PROGRESS_H
#define GLOBAL_PROGRESS_H

#include "generic_types.h"

#ifdef __cplusplus
namespace sysync {
#endif


// - progress event types
typedef enum {
  // global
  pev_error, // some fatal aborting error
  pev_message, // extra messages
  pev_errcode, // extra error code
  pev_nop, // no extra message, just called to allow aborting
  pev_wait, // called to signal main program that caller would want to wait for extra1 milliseconds
  pev_debug, // called to allow debug interactions, extra1=code
  // transport-related
  pev_sendstart,
  pev_sendend,
  pev_recvstart,
  pev_recvend,
  pev_ssl_expired, // expired
  pev_ssl_notrust, // not completely trusted
  pev_conncheck, // sent periodically when waiting for network, allows application to check connection
  pev_suspendcheck, // sent when client could initiate a explicit suspend
  // General
  pev_display100, // alert 100 received from remote, extra1=char * to message text in UTF8
  // Session-related
  pev_sessionstart,
  pev_sessionend, // session ended, probably with error in extra
  // Datastore-related
  pev_preparing, // preparing (e.g. preflight in some clients), extra1=progress, extra2=total
  pev_deleting, // deleting (zapping datastore), extra1=progress, extra2=total
  pev_alerted, // datastore alerted (extra1=0 for normal, 1 for slow, 2 for first time slow, extra2=1 for resumed session, extra3=syncmode)
  pev_syncstart, // sync started
  pev_itemreceived, // item received, extra1=current item count, extra2=number of expected changes (if >= 0)
  pev_itemsent, // item sent, extra1=current item count, extra2=number of expected items to be sent (if >=0)
  pev_itemprocessed, // item locally processed, extra1=# added, extra2=# updated, extra3=# deleted
  pev_syncend, // sync finished, probably with error in extra1 (0=ok), syncmode in extra2 (0=normal, 1=slow, 2=first time), extra3=1 for resumed session)
  pev_dsstats_l, // datastore statistics for local (extra1=# added, extra2=# updated, extra3=# deleted)
  pev_dsstats_r, // datastore statistics for remote (extra1=# added, extra2=# updated, extra3=# deleted)
  pev_dsstats_e, // datastore statistics for local/remote rejects (extra1=# locally rejected, extra2=# remotely rejected)
  pev_dsstats_s, // datastore statistics for server slowsync (extra1=# slowsync matches)
  pev_dsstats_c, // datastore statistics for server conflicts (extra1=# server won, extra2=# client won, extra3=# duplicated)
  pev_dsstats_d, // datastore statistics for data volume (extra1=outgoing bytes, extra2=incoming bytes)
  // number of enums
  numProgressEventTypes
} TProgressEventType;


#ifndef ENGINE_LIBRARY

// globally accessible progress event posting
#ifdef __cplusplus
extern "C" int GlobalNotifyProgressEvent
#else
extern int GlobalNotifyProgressEvent
#endif
(
  TProgressEventType aEventType,
  sInt32 aExtra1,
  sInt32 aExtra2,
  sInt32 aExtra3
);

#endif // not ENGINE_LIBRARY

#ifdef __cplusplus
} // namespace sysync
#endif

#endif // GLOBAL_PROGRESS_H

/* eof */
