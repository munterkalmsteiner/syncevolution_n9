/*
 *  File:         syerror.h
 *
 *  Author:       luz@synthesis.ch / bfo@synthesis.ch
 *
 *  Synthesis SyncML engine error code definitions
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *
 */

#ifndef SYERROR_H
#define SYERROR_H

#include "generic_types.h"

#ifdef __cplusplus
  namespace sysync {
#endif

/**
 * global type for error codes
 *
 * Using an integer simplifies some arithmetic,
 * allows assigning HTTP error codes to it,
 * and most importantly, ensures that the ABI
 * does not accidentally change.
 */
typedef uInt16 TSyError;

/**
 * Local error codes
 */
enum TSyErrorEnum {
  /** ok */
  LOCERR_OK = 0,

  /** no content / end of file / end of iteration / empty/NULL value */
  DB_NoContent = 204,

  /** while adding item, additional data (from external source or other syncset item) has been merged */
  DB_DataMerged = 207,

  /** adding item has replaced an existing item */
  DB_DataReplaced = 208,

  /** not authorized */
  DB_Unauthorized = 401,
  /** forbidden / access denied */
  DB_Forbidden = 403,
  /** object not found / unassigned field */
  DB_NotFound = 404,
  /** command not allowed (possibly: only at this time) */
  DB_NotAllowed = 405,

  /** proxy authentication required */
  DB_ProxyAuth = 407,

  /** conflict, item was not stored because it requires merge (on part of the engine) first */
  DB_Conflict = 409,

  /** item already exists */
  DB_AlreadyExists = 418,

  /** command failed / fatal DB error */
  DB_Fatal = 500,
  /** general DB error */
  DB_Error = 510,
  /** database / memory full error */
  DB_Full = 420,

  /** bad or unknown protocol */
  LOCERR_BADPROTO = 20001,
  /** fatal problem with SML init/setuserdata etc. */
  LOCERR_SMLFATAL = 20002,
  /** cannot open communication (TCP level) */
  LOCERR_COMMOPEN = 20003,
  /** cannot send data */
  LOCERR_SENDDATA = 20004,
  /** cannot receive data */
  LOCERR_RECVDATA = 20005,
  /** Bad content in response (i.e. non-SyncML, exceeding buffer or HTTP status not 200) */
  LOCERR_BADCONTENT = 20006,
  /** SML (or SAN) error processing incoming message */
  LOCERR_PROCESSMSG = 20007,
  /** cannot close communication */
  LOCERR_COMMCLOSE = 20008,
  /** transport layer authorisation (e.g. HTTP auth) failed */
  LOCERR_AUTHFAIL = 20009,
  /** error parsing config file */
  LOCERR_CFGPARSE = 20010,
  /** error reading config file */
  LOCERR_CFGREAD = 20011,
  /** no config/profile found at all, or not enough for requested op (client session start) */
  LOCERR_NOCFG = 20012,
  /** config file could not be found */
  LOCERR_NOCFGFILE = 20013,
  /** expired */
  LOCERR_EXPIRED = 20014,
  /** bad usage (e.g. wrong order of library calls) */
  LOCERR_WRONGUSAGE = 20015,
  /** bad handle (e.g. datastore) */
  LOCERR_BADHANDLE = 20016,
  /** aborted by user */
  LOCERR_USERABORT = 20017,
  /** bad registration (not valid generally or for current product) */
  LOCERR_BADREG = 20018,
  /** limited trial version */
  LOCERR_LIMITED = 20019,
  /** connection timeout */
  LOCERR_TIMEOUT = 20020,
  /** connection SSL certificate expired */
  LOCERR_CERT_EXPIRED = 20021,
  /** connection SSL certificate invalid */
  LOCERR_CERT_INVALID = 20022,
  /** incomplete sync session (some datastores or items have failed) */
  LOCERR_INCOMPLETE = 20023,
  /** internal code signalling that client should retry sending message (instance buffer still contains the message) */
  LOCERR_RETRYMSG = 20024,
  /** out of memory */
  LOCERR_OUTOFMEM = 20025,
  /** we have no means to open a connection (such as phone flight mode etc.) */
  LOCERR_NOCONN = 20026,
  /** connection (not TCP, but underlying PPP/GPRS/BT or whatever) cannot be established */
  LOCERR_CONN = 20027,
  /** element is already installed */
  LOCERR_ALREADY = 20028,
  /** this build is too new for this license (need upgrading license) */
  LOCERR_TOONEW = 20029,
  /** function not implemented */
  LOCERR_NOTIMP = 20030,
  /** this license code is valid, but not for this product */
  LOCERR_WRONGPROD = 20031,
  /** explicitly suspended by user */
  LOCERR_USERSUSPEND = 20032,
  /** this build is too old for this SDK/plugin */
  LOCERR_TOOOLD = 20033,
  /** unknown subsystem */
  LOCERR_UNKSUBSYSTEM = 20034,
  /** internal code signalling that next message will be a session restart (client should disconnect transport) */
  LOCERR_SESSIONRST = 20035,
  /** local datastore is not ready (used to pop up alert) */
  LOCERR_LOCDBNOTRDY = 20036,
  /** session should be restarted from scratch */
  LOCERR_RESTART = 20037,
  /** internal pipe communication problem */
  LOCERR_PIPECOMM = 20038,
  /** buffer too small for requested value */
  LOCERR_BUFTOOSMALL = 20039,
  /** value truncated to fit into field or buffer */
  LOCERR_TRUNCATED = 20040,
  /** bad parameter */
  LOCERR_BADPARAM = 20041,
  /** out of range */
  LOCERR_OUTOFRANGE = 20042,
  /** external transport failure (no details known in engine) */
  LOCERR_TRANSPFAIL = 20043,
  /** class     not registered */
  LOCERR_CLASSNOTREG = 20044,
  /** interface not registered */
  LOCERR_IIDNOTREG = 20045,
  /** bad URL */
  LOCERR_BADURL = 20046,
  /** server not found */
  LOCERR_SRVNOTFOUND = 20047,
  /**
   * ABORTDATASTORE() parameter to flag the current datastore as bad
   * without aborting the whole session. Exact reason for abort depends
   * on caller of that macro.
   */
  LOCERR_DATASTORE_ABORT = 20048,

  /** cURL error code */
  LOCERR_CURL = 21000,

  /** base code for linux signals (SIGXXX). SIGXXX enum value will be added to LOCERR_SIGNAL */
  LOCERR_SIGNAL = 20500,

  /** TSyncException without specific error code set */
  LOCERR_EXCEPTION = 20998,
  /** undefined error message */
  LOCERR_UNDEFINED = 20999,

  /**
   * A range of error codes not used by the engine itself, start and
   * end included. Can be used to pass application specific error
   * codes from the DB layer up to the UI.
   */
  APP_STATUS_CODE_START = 22000,
  APP_STATUS_CODE_END = 22999,

  /**
   * Local codes signalling SyncML status are shown as LOCAL_STATUS_CODE+<SyncML-Statuscode>.
   * This applies to codes >= LOCAL_STATUS_CODE and <= LOCAL_STATUS_CODE_END = 10599.
   *
   * For example, 500 = DB_Fatal = fatal error signaled by peer.
   * 10500 = LOCAL_STATUS_CODE + DB_Fatal = fatal error encountered locally.
   */
  LOCAL_STATUS_CODE = 10000,
  LOCAL_STATUS_CODE_END = 10599
};

#ifdef __cplusplus
  } // namespace
#endif


#endif /* SYERROR_H */

/* eof */

