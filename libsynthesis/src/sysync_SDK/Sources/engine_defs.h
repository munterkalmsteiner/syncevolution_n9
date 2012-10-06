/**
 *  @File     engine_defs.h
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *            Beat Forster (bfo@synthesis.ch)
 *
 *  @brief Definitions needed for the common engine interface
 *
 *  Copyright (c) 2007-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef ENGINE_DEFS_H
#define ENGINE_DEFS_H

#include "generic_types.h"

#ifdef __cplusplus
namespace sysync {
#endif


/* ---- supported charsets ---- */
enum TCharsetEnum {
  /** invalid */
  CHS_UNKNOWN = 0,
  /** 7 bit ASCII-only, with nearest char conversion for umlauts etc. */
  CHS_ASCII = 1,
  CHS_ANSI = 2,
  CHS_ISO_8859_1 = 3,
  CHS_UTF8 = 4,
  CHS_UTF16 = 5,
  CHS_GB2312 = 6,
  CHS_CP936 = 7,
};

/* ---- line end modes ---- */
enum TLineendEnum {
  /** none specified   */
  LEM_NONE = 0,
  /** LF = 0x0A, Unix, Linux, MacOS X */
  LEM_UNIX = 1,
  /** CR = 0x0D, Mac OS classic       */
  LEM_MAC = 2,
  /** CRLF = 0x0D 0x0A, Windows, DOS  */
  LEM_DOS = 3,
  /** as in C strings, '\n' which is 0x0A normally
      (but might be 0x0D on some platforms) */
  LEM_CSTR = 4,
  /** 0x0B (filemaker tab-separated text format,
      CR is shown as 0x0B within fields */
  LEM_FILEMAKER = 5
};

/* ---- literal quoting modes ---- */
enum TQuotingEnum {
  /** none specified */
  QM_NONE = 0,
  /** single quote must be duplicated */
  QM_DUPLSINGLE = 1,
  /** double quote must be duplicated */
  QM_DUPLDOUBLE = 2,
  /** C-string-style escapes of CR,LF,TAB,BS,\," and '
      (but no full c-string escape with \xXX etc.) */
  QM_BACKSLASH = 3
};

/* ---- time formats ---- */
enum TTimeModeEnum {
  /** SySync lineartime (64 bit milliseconds) */
  TMODE_LINEARTIME = 0,
  /** SySync lineardate (32 bit days)         */
  TMODE_LINEARDATE = 1,
  /** Unix epoch time   (32 bit seconds)      */
  TMODE_UNIXTIME = 2,
  /** Unix epoch time   (64 bit milliseconds) */
  TMODE_UNIXTIME_MS = 3,
  /** Flag that can be added for time in UTC  */
  TMODE_FLAG_UTC = 0x8000,
  /** Flag that can be added for time as-is   */
  TMODE_FLAG_FLOATING = 0x4000,
  /** Mask to separate time mode              */
  TMODE_MODEMASK = 0x003F
};


/* ---- path separator for settings key paths ---- */
#define SETTINGSKEY_PATH_SEPARATOR '/'

/* ---- key/value special IDs ---- */
enum TKeyValEnum {
  /** special ID to signal unknown subkey or value */
  KEYVAL_ID_UNKNOWN = -1,
  /** special ID to open first subkey */
  KEYVAL_ID_FIRST = -2,
  /** special ID to open next  subkey */
  KEYVAL_ID_NEXT = -3,
  /** special ID to create new subkey (empty values) */
  KEYVAL_ID_NEW = -4,
  /** special ID to create new subkey and initialize with
      (hard-coded or engine-config-predefined) default values */
  KEYVAL_ID_NEW_DEFAULT = -5,
  /** special ID to create new subkey using previously opened
      subkey as template for initializing values */
  KEYVAL_ID_NEW_DUP = -6,
  /** special ID for values that cannot be accessed by ID AT ALL */
  KEYVAL_NO_ID = -7,
  /** special ID to specify entire contents of a container */
  KEYVAL_ID_ALL = -8
};

/* ---- special value names ---- */
#define VALNAME_FIRST      ".FIRST" /* for GetValueID(): get first value, for keys that support value iteration */
#define VALNAME_NEXT        ".NEXT" /* for GetValueID(): get next value, for keys that support value iteration */
#define VALNAME_FLAG        ".FLAG" /* for GetValueID(): get flag mask that can be added to value ID to get special attributes of a value instead of main value */
/* ---- special value name suffixes (not available for all settings keys, but primarily for item keys) ---- */
#define VALSUFF_NAME     ".VALNAME" /* return field/value name as VALTYPE_TEXT */
#define VALSUFF_TYPE     ".VALTYPE" /* return value's native type (VALTYPE_xxx) name as VALTYPE_INT16 */
#define VALSUFF_ARRSZ  ".ARRAYSIZE" /* return size of array as VALTYPE_INT16. Returns DB_NotFound for non-array values */
#define VALSUFF_TZNAME    ".TZNAME" /* if value is a time stamp, returns name of the time zone as VALTYPE_TEXT (empty=floating) */
#define VALSUFF_TZOFFS    ".TZOFFS" /* if value is a time stamp, returns minute offset east of GMT as VALTYPE_INT16, DB_NotFound for floating timestamps */
#define VALSUFF_NORM        ".NORM" /* return normalized value of field */



/* ---- value types ---- */
enum TValTypeEnum {
  /** Unknown */
  VALTYPE_UNKNOWN = 0,
  /**  8-bit integer */
  VALTYPE_INT8 = 1,
  /** 16-bit integer */
  VALTYPE_INT16 = 2,
  /** 32-bit integer */
  VALTYPE_INT32 = 3,
  /** 64-bit integer */
  VALTYPE_INT64 = 4,
  /** enum (for the SDK user, this is equivalent to VALTYPE_INT8) */
  VALTYPE_ENUM = 5,
  /** no value (for writing only, only works for values that have no value status) */
  VALTYPE_NULL = 6,
  /** raw buffer, binary (for texts, will return application charset,
     null terminated) */
  VALTYPE_BUF = 10,
  /** text in charset / line-end mode as specified with SetTextMode()  */
  VALTYPE_TEXT = 20,
  /** internal use only: text, internally saved in obfuscated format   */
  VALTYPE_TEXT_OBFUS = 21,
  /** time specification 64 bit in format specified with SetTimeMode() */
  VALTYPE_TIME64 = 30,
  /** time specification 32 bit in format specified with SetTimeMode() */
  VALTYPE_TIME32 = 31,
};

/* ---- application feature numbers ---- */
enum TAppFeatureEnum {
  /** client autosync */
  APP_FTR_AUTOSYNC = 1,
  /** client IPP/DMU */
  APP_FTR_IPP = 2,
  /** client settings for event date range */
  APP_FTR_EVENTRANGE = 3,
  /** client settings for email date and maxsize range */
  APP_FTR_EMAILRANGE = 4
};

/* ---- danger flags ---- */
enum TDangerEnum {
  /** sync session will zap client data */
  DANGERFLAG_WILLZAPCLIENT = 1,
  /** sync session will zap server data */
  DANGERFLAG_WILLZAPSERVER = 2
};


/* ---- read-only indicators for certain settings fields --- */
enum TReadOnlyEnum {
  /** server URI is read-only */
  RDONLY_URI = 1,
  /** URI path suffix is read-only */
  RDONLY_URIPATH = 2,
  /** protocol (transport http/https flag as well as SyncML version) are read-only */
  RDONLY_PROTOCOL = 4,
  /** DB path names */
  RDONLY_DBPATH = 8,
  /** profile may not be deleted or renamed (but non-readonly options can be set) */
  RDONLY_PROFILE = 16,
  /** proxy settings (except proxy auth settings) are read-only */
  RDONLY_PROXY = 32
};

/**
 * Profile modes (MIME DIR mode).
 *
 * These are the values also used as parameter of the
 * MAKETEXTWITHPROFILE/PARSETEXTWITHPROFILE macros.
 */
enum ProfileModeEnum {
  PROFILEMODE_DEFAULT = 0,        /**< default mode of profile */
  PROFILEMODE_OLD = 1,            /**< vCard 2.1 and vCalendar 1.0 style */
  PROFILEMODE_MIMEDIR = 2         /**< MIME-DIR = vCard 3.0, iCalendar 2.0 style */
};


/* ---- bit definitons for "profileFlags" profile field --- */
enum TProfileFlagsEnum {
  /** we should log the next session (not handled in engine!) */
  PROFILEFLAG_LOGNEXTSYNC = 0x00000001,
  /** run session in legacy mode */
  PROFILEFLAG_LEGACYMODE  = 0x00000002,
  /** run session in lenient mode */
  PROFILEFLAG_LENIENTMODE = 0x00000004,
  /** we should keep msg logs of the next session (not handled in engine!) */
  PROFILEFLAG_MSGNEXTSYNC = 0x00000008,
  /** we should use this profile in global syncs (not handled in engine!) */
  PROFILEFLAG_USEINGLOBSYNC = 0x00000008
};


/* ---- bit definitons for "transpFlags" profile field --- */
enum TTranspFlagsEnum {
  /** SSL certificate verification/expiration errors should be ignored (not handled in engine!) */
  TRANSPFLAG_SSLIGNORECERTFAIL = 0x00000001,
  /** use http auth for server connection (not handled in engine!) */
  TRANSPFLAG_USEHTTPAUTH = 0x00000002
};




/* ---- step commands ---- */
/* - input to engine */
enum TStepCmdEnum {
  /** start a new client session */
  STEPCMD_CLIENTSTART = 1,
  /** start a autosync session */
  STEPCMD_CLIENTAUTOSTART = 2,

  /** just run next step */
  STEPCMD_STEP = 10,
  /** run next step (after receiving STEPCMD_NEEDDATA and putting received
      SyncML data into buffer using GetSyncMLBuffer/ReturnSyncMLBuffer) */
  STEPCMD_GOTDATA = 11,
  /** run next step (after receiving STEPCMD_SENDDATA and sending SyncML
      data from buffer using GetSyncMLBuffer/ReturnSyncMLBuffer) */
  STEPCMD_SENTDATA = 12,

  /** suspend the session. Note that this command can be issued out-of order
      instead of the next pending command (STEPCMD_GOTDATA, STEPCMD_SENTDATA, STEPCMD_STEP)
      and will always return STEPCMD_OK. The next in-order STEPCMD_xx must be issued
      in the next step to complete the suspend */
  STEPCMD_SUSPEND = 20,
  /** abort the session (user abort) */
  STEPCMD_ABORT = 21,
  /** transport failure causes aborting the session */
  STEPCMD_TRANSPFAIL = 22,
  /** timeout causes aborting the session */
  STEPCMD_TIMEOUT = 23,

  /** process SAN in SyncML buffer */
  STEPCMD_SAN_CHECK = 40,
  /** check for periodic autosync */
  STEPCMD_AUTOSYNC_CHECK = 41,
/* - output from engine */
  /** engine returns to caller w/o progress info, and should be called
      again ASAP with STEPCMD_STEP or, if the previous call was
      STEPCMD_SUSPEND, with the step command that was pending before
      doing the suspend (for example STEPCMD_GOTDATA, STEPCMD_SENTDATA, STEPCMD_STEP) */
  STEPCMD_OK = 100,
  /** engine returns to caller to show progress, and should be called
      again ASAP with STEPCMD_STEP */
  STEPCMD_PROGRESS = 101,
  /** error (see return value of SessionStep call) */
  STEPCMD_ERROR = 102,
  /** engine has new data to send to remote, use GetSyncMLBuffer() to get
      access to the engine buffer containing the message */
  STEPCMD_SENDDATA = 110,
  /** engine needs new data, use GetSyncMLBuffer() to get access to the
      empty buffer where to put data */
  STEPCMD_NEEDDATA = 111,
  /** engine needs to reset status to resend data */
  STEPCMD_RESENDDATA = 112,
  /** session done, SAN processed, Autosync checked etc.
      No further action required */
  STEPCMD_DONE = 120,
  /** communication session ends here (close connection!) but SyncML session continues  */
  STEPCMD_RESTART = 121,
  /** STEPCMD_AUTOSYNC_CHECK or STEPCMD_SAN_CHECK has detected need
      for performing a sync session */
  STEPCMD_NEEDSYNC = 130
};

/* ---- session type selectors */
enum TSessionSelectorEnum {
  /* - binfile client engine selectors */
  /** normal client session base selector value, add profile ID to select profile */
  SESSIONSEL_CLIENT_NORMAL = 0x00000000,
  /** mask to extract profile ID from selector */
  SESSIONSEL_PROFILEID_MASK = 0x0FFFFFFF,
  /** Autosync (timed, IPP, SAN) checking session */
  SESSIONSEL_CLIENT_AS_CHECK = 0x10000000,
  /* - other special session selectors */
  /** test session to access a datastore using DBAPI-like calls */
  SESSIONSEL_DBAPI_TUNNEL = 0x80000001
};


/* ---- XML config data reader func type ---- */
#if defined _WIN32 && !defined _MSC_VER
  #define _CALLING_ __stdcall
#else
  #define _CALLING_
#endif

typedef int _CALLING_ (*TXMLConfigReadFunc)(
  appCharP     aBuffer,
  bufferIndex  aMaxSize,
  bufferIndex *aReadCharsP,
  void        *aContext
);


/* ---- Engine progress information ---- */
/* - progress event types */
enum TProgressEventEnum {
  /* global */

  /** some fatal aborting error */
  PEV_ERROR = 0,
  /** extra messages */
  PEV_MESSAGE = 1,
  /** extra error code */
  PEV_ERRCODE = 2,
  /** no extra message, just called to allow aborting */
  PEV_NOP = 3,
  /** called to signal main program, that caller would want to
      wait for extra1 milliseconds */
  PEV_WAIT = 4,
  /** called to allow debug interactions, extra1=code */
  PEV_DEBUG = 5,

  /* transport-related */

  PEV_SENDSTART = 6,
  PEV_SENDEND = 7,
  PEV_RECVSTART = 8,
  PEV_RECVEND = 9,
  /** expired */
  PEV_SSL_EXPIRED = 10,
  /** not completely trusted */
  PEV_SSL_NOTRUST = 11,
  /** sent periodically when waiting for network,
      allows application to check connection */
  PEV_CONNCHECK = 12,
  /** sent when client could initiate a explicit suspend */
  PEV_SUSPENDCHECK = 13,

  /* general */

  /** alert 100 received from remote, SessionKey's "displayalert" value contains message */
  PEV_DISPLAY100 = 14,

  /* session-related */

  PEV_SESSIONSTART = 15,
  /** session ended, probably with error in extra */
  PEV_SESSIONEND = 16,
  /* datastore-related */
  /** preparing (e.g. preflight in some clients), extra1=progress, extra2=total */
  PEV_PREPARING = 17,
  /** deleting (zapping datastore), extra1=progress, extra2=total */
  PEV_DELETING = 18,
  /** datastore alerted (extra1=0 for normal, 1 for slow, 2 for first time slow,
      extra2=1 for resumed session, extra3=syncmode: 0=twoway, 1=fromserver, 2=fromclient)
      
      This used to be reported only in client mode, directly after receiving and checking
      the ALERT command from the server. Now this event is also reported in server mode
      at that point in time where the final sync mode is known (in other words, after
      the client acknowledged the alerted sync mode).

      Note that a server cannot reliably distinguish between "refresh-from-server" and
      "slow" sync. A client might delete all its data and then do a "slow" sync - the
      Synthesis engine itself does that.
  */
  PEV_ALERTED = 19,
  /** sync started */
  PEV_SYNCSTART = 20,
  /** item received, extra1=current item count,
      extra2=number of expected changes (if >= 0) */
  PEV_ITEMRECEIVED = 21,
  /** item sent,     extra1=current item count,
      extra2=number of expected items to be sent (if >=0) */
  PEV_ITEMSENT = 22,
  /** item locally processed,               extra1=# added,
      extra2=# updated,
      extra3=# deleted */
  PEV_ITEMPROCESSED = 23,
  /** sync finished, probably with error in extra1 (0=ok),
      syncmode in extra2 (0=normal, 1=slow, 2=first time),
      extra3=1 for resumed session) */
  PEV_SYNCEND = 24,
  /** datastore statistics for local       (extra1=# added,
      extra2=# updated,
      extra3=# deleted) */
  PEV_DSSTATS_L = 25,
  /** datastore statistics for remote      (extra1=# added,
      extra2=# updated,
      extra3=# deleted) */
  PEV_DSSTATS_R = 26,
  /** datastore statistics for local/remote rejects (extra1=# locally rejected,
      extra2=# remotely rejected) */
  PEV_DSSTATS_E = 27,
  /** datastore statistics for server slowsync  (extra1=# slowsync matches) */
  PEV_DSSTATS_S = 28,
  /** datastore statistics for server conflicts (extra1=# server won,
      extra2=# client won,
      extra3=# duplicated) */
  PEV_DSSTATS_C = 29,
  /** datastore statistics for data   volume    (extra1=outgoing bytes,
      extra2=incoming bytes) */
  PEV_DSSTATS_D = 30,
  /** engine is in process of suspending */
  PEV_SUSPENDING = 31,

  /**
   * range of event codes that can be used by applications for their
   * own purposes
   */
  PEV_CUSTOM_START = 128,
  PEV_CUSTOM_END = 255
};


/* - progress event structure */
typedef struct TEngineProgressType {
  uInt16 eventtype; /* PEV_XXX definition */
  sInt32 targetID;  /* target ID if event is specific to a target, KEYVAL_ID_UNKNOWN if session global */
  sIntPtr extra1;   /* extra info, such as error code or count for progress or # of added items */
  sIntPtr extra2;   /* extra info, such as total for progress or # of updated items */
  sIntPtr extra3;   /* extra info, such as # of deleted items */
} TEngineProgressInfo;



#ifdef __cplusplus
} // namespace
#endif

#endif /* ENGINE_DEFS_H */

/* eof */
