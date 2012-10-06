/**
 *  @File     odbcapiagent.h
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TODBCApiAgent
 *    ODBC based agent (client or server session) implementation
 *
 *    Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-10-06 : luz : created from odbcdbagent
 */

#ifdef SQL_SUPPORT

#ifndef ODBCAPIAGENT_H
#define ODBCAPIAGENT_H

// includes

#ifdef SQLITE_SUPPORT
#include "sqlite3.h"
#endif

#include "odbcdb.h"

#include "customimplagent.h"
#include "customimplds.h"
#include "multifielditem.h"
#include "scriptcontext.h"


using namespace sysync;

namespace sysync {

// Support for SySync Diagnostic Tool
#ifdef SYSYNC_TOOL
// - execute SQL command
int execSQL(int argc, const char *argv[]);
#endif // SYSYNC_TOOL


#ifdef SCRIPT_SUPPORT
// publish as derivates might need it
extern const TFuncTable ODBCAgentFuncTable;
// publish as ODBCDBDatastore uses this (chains back to Datastore level funcs)
extern const TFuncTable ODBCDSFuncTable1;
#endif

// supported transaction isolation levels
typedef enum {
  // Note: these MUST be in the same order as
  //   Win32/ODBC defined SQL_TXN_READ_UNCOMMITTED..SQL_TXN_SERIALIZABLE
  //   bits appear in the transaction bitmasks.
  txni_uncommitted, // read uncommitted (dirty reads possible)
  txni_committed, // read committed only
  txni_repeatable, // repeatable
  txni_serializable, // no dirty reads, repeatable, no phantoms
  // special values
  txni_none, // no transactions
  txni_default // default of data source
} TTxnIsolModes;
const sInt16 numTxnIsolModes = txni_default-txni_uncommitted+1;


// standard size for parameter buffers if not specified
const size_t std_paramsize = 512;

// parameter value type
typedef enum {
  param_field,
  param_localid_str,
  param_localid_int,
  param_remoteid_str,
  param_remoteid_int,
  param_buffer
} TParamMode;

// parameter mapping entry
typedef struct {
  // parameter specification
  bool inparam;
  bool outparam;
  TParamMode parammode;
  // value source/target
  TDBFieldType dbFieldType;
  // field info for param_field
  TItemField *fieldP;
  size_t maxSize;
  TMultiFieldItem *itemP; // can be null if no item available
  // size info for param_buffer
  uInt32 *outSiz;
  // Parameter value buffer
  bool mybuffer;
  #ifdef ODBCAPI_SUPPORT
  SQLPOINTER ParameterValuePtr;
  SQLLEN BufferLength;
  SQLLEN StrLen_or_Ind;
  #elif defined(SQLITE_SUPPORT)
  appPointer ParameterValuePtr;
  memSize BufferLength;
  memSize StrLen_or_Ind;
  #endif
} TParameterMap;

// parameter mapping list
typedef std::list<TParameterMap> TParameterMapList;

#ifdef WIN32
  // windows, has SEH, leave it on (unless set by target options etc.)
#else
  // non-windows, no SEH anyway
  #define NO_AV_GUARDING
#endif

#ifdef ODBCAPI_SUPPORT

#if defined(NO_AV_GUARDING) // || __option(microsoft_exceptions)

// SEH is on (or we can't catch AV at all), we don't need extra crash-guarding
#define SafeSQLAllocHandle SQLAllocHandle
#define SafeSQLFreeHandle SQLFreeHandle
#define SafeSQLSetEnvAttr SQLSetEnvAttr
#define SafeSQLSetConnectAttr SQLSetConnectAttr
#define SafeSQLConnect SQLConnect
#define SafeSQLDriverConnect SQLDriverConnect
#define SafeSQLGetInfo SQLGetInfo
#define SafeSQLEndTran SQLEndTran
#define SafeSQLExecDirect SQLExecDirect
#define SafeSQLFetch SQLFetch
#define SafeSQLNumResultCols SQLNumResultCols
#define SafeSQLGetData SQLGetData
#define SafeSQLCloseCursor SQLCloseCursor

#ifdef ODBC_UNICODE
#define SafeSQLExecDirectW SQLExecDirectW
#endif

#else

// special exception for SEH
class TODBCSEHexception : public TSyncException
{
  typedef TSyncException inherited;
public:
  TODBCSEHexception(uInt32 aCode);
}; // TODBCSEHexception


// prototypes

SQLRETURN SafeSQLAllocHandle(
  SQLSMALLINT     HandleType,
  SQLHANDLE       InputHandle,
  SQLHANDLE *     OutputHandlePtr);

SQLRETURN SafeSQLFreeHandle(
  SQLSMALLINT HandleType,
  SQLHANDLE   Handle);

SQLRETURN SafeSQLSetEnvAttr(
  SQLHENV     EnvironmentHandle,
  SQLINTEGER  Attribute,
  SQLPOINTER  ValuePtr,
  SQLINTEGER  StringLength);

SQLRETURN SafeSQLSetConnectAttr(
  SQLHDBC     ConnectionHandle,
  SQLINTEGER  Attribute,
  SQLPOINTER  ValuePtr,
  SQLINTEGER  StringLength);

SQLRETURN SafeSQLConnect(
  SQLHDBC     ConnectionHandle,
  SQLCHAR *   ServerName,
  SQLSMALLINT NameLength1,
  SQLCHAR *   UserName,
  SQLSMALLINT NameLength2,
  SQLCHAR *   Authentication,
  SQLSMALLINT NameLength3);

SQLRETURN SafeSQLDriverConnect(
  SQLHDBC       ConnectionHandle,
  SQLHWND       WindowHandle,
  SQLCHAR *     InConnectionString,
  SQLSMALLINT   StringLength1,
  SQLCHAR *     OutConnectionString,
  SQLSMALLINT   BufferLength,
  SQLSMALLINT * StringLength2Ptr,
  SQLUSMALLINT  DriverCompletion);

SQLRETURN SafeSQLGetInfo(
  SQLHDBC       ConnectionHandle,
  SQLUSMALLINT  InfoType,
  SQLPOINTER    InfoValuePtr,
  SQLSMALLINT   BufferLength,
  SQLSMALLINT * StringLengthPtr);

SQLRETURN SafeSQLEndTran(
  SQLSMALLINT HandleType,
  SQLHANDLE   Handle,
  SQLSMALLINT CompletionType);

SQLRETURN SafeSQLExecDirect(
  SQLHSTMT    StatementHandle,
  SQLCHAR *   StatementText,
  SQLINTEGER  TextLength);

#ifdef ODBC_UNICODE
SQLRETURN SafeSQLExecDirectW(
  SQLHSTMT    StatementHandle,
  SQLWCHAR *  StatementText,
  SQLINTEGER  TextLength);
#endif

SQLRETURN SafeSQLFetch(
  SQLHSTMT  StatementHandle);

SQLRETURN SafeSQLNumResultCols(
  SQLHSTMT      StatementHandle,
  SQLSMALLINT * ColumnCountPtr);

SQLRETURN SafeSQLGetData(
  SQLHSTMT      StatementHandle,
  SQLUSMALLINT  ColumnNumber,
  SQLSMALLINT   TargetType,
  SQLPOINTER    TargetValuePtr,
  SQLINTEGER    BufferLength,
  SQLLEN *      StrLen_or_IndPtr);

SQLRETURN SafeSQLCloseCursor(
  SQLHSTMT  StatementHandle);

#endif

#endif // ODBCAPI_SUPPORT

// ODBC Utils

// - return quoted version of string if aDoQuote is set
const char *quoteString(string &aIn, string &aOut, TQuotingModes aQuoteMode);
const char *quoteString(const char *aIn, string &aOut, TQuotingModes aQuoteMode);
void quoteStringAppend(string &aIn, string &aOut, TQuotingModes aQuoteMode);
void quoteStringAppend(const char *aIn, string &aOut, TQuotingModes aQuoteMode);


// forward
class TODBCDSConfig;
#ifdef SCRIPT_SUPPORT
class TScriptContext;
#endif


class TOdbcAgentConfig : public TCustomAgentConfig
{
  typedef TCustomAgentConfig inherited;
public:
  TOdbcAgentConfig(TConfigElement *aParentElement);
  virtual ~TOdbcAgentConfig();
  // properties
  #ifdef ODBCAPI_SUPPORT
  // - ODBC datasource
  string fDataSource;
  #ifdef SCRIPT_SUPPORT
  // - scripts
  string fAfterConnectScript; // called after opening an ODBC connection
  #endif
  // - Server username / password
  string fUsername;
  string fPassword;
  // - connection string
  string fDBConnStr;
  // - preventing use of SQLSetConnectAttr (for drivers that crash when using it)
  bool fNoConnectAttrs;
  // - timeout
  sInt32 fODBCTimeout;
  // - transaction isolation mode
  sInt16 fODBCTxnMode;
  // - cursor library usage
  bool fUseCursorLib;
  // - statements to maintain device table
  string fGetDeviceSQL; // if set, fGetDeviceSQL must return device key and last saved nonce
  string fNewDeviceSQL; // create new device entry
  string fSaveNonceSQL; // save nonce into existing device entry
  string fSaveInfoSQL; // save extra device info
  string fSaveDevInfSQL; // save device information (such as name) into existing device entry
  string fLoadDevInfSQL; // load cached device information from DB
  // - statement to obtain user key (and password if fClearTextPw) from Database
  //   replacements: %U=username, %M=user:pw: MD5
  string fUserKeySQL;
  bool fClearTextPw; // if set, fUserKeySQL must return clear text PW in second column.
  bool fMD5UserPass; // if set, fUserKeySQL must return MD5B64("user:password") or HEX(MD5("user:password"))in second column.
  bool fMD5UPAsHex; // if set, format expected from DB is a hex string (not B64)
  // - statement to obtain database server's date and time (if none, local time is used)
  string fGetCurrentDateTimeSQL;
  // - statement to save log info
  string fWriteLogSQL;
  #endif // ODBCAPI_SUPPORT
  // - Literal string quoting mode
  TQuotingModes fQuotingMode;
  #ifdef SCRIPT_SUPPORT
  // provided to allow derivates to add API specific script functions to scripts called from customagent
  virtual const TFuncTable *getAgentFuncTableP(void) { return &ODBCAgentFuncTable; };
  #endif
protected:
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  virtual void clear();
  virtual void localResolve(bool aLastPass);
  #ifdef SCRIPT_SUPPORT
  virtual void ResolveAPIScripts(void);
  #endif
  #ifdef SYSYNC_CLIENT
  // create appropriate session (=agent) for this client
  virtual TSyncAgent *CreateClientSession(const char *aSessionID);
  #endif
public:
  #ifdef SYSYNC_SERVER
  // create appropriate session (=agent) for this server
  virtual TSyncAgent *CreateServerSession(TSyncSessionHandle *aSessionHandle, const char *aSessionID);
  #endif
}; // TOdbcAgentConfig


class TODBCApiDS;

class TODBCApiAgent: public TCustomImplAgent
{
  friend class TODBCCommonFuncs;
  friend class TODBCAgentFuncs;
  typedef TCustomImplAgent inherited;
public:
  TODBCApiAgent(TSyncAppBase *aAppBaseP, TSyncSessionHandle *aSessionHandleP, cAppCharP aSessionID);
  virtual ~TODBCApiAgent();
  virtual void TerminateSession(void); // Terminate session, like destructor, but without actually destructing object itself
  virtual void ResetSession(void); // Resets session (but unlike TerminateSession, session might be re-used)
  void InternalResetSession(void); // static implementation for calling through virtual destructor and virtual ResetSession();
  #ifdef HAS_SQL_ADMIN
  // Note: %%% for now, login is supported by ODBC only
  // user authentication
  #ifdef SYSYNC_SERVER
  // - return auth type to be requested from remote
  virtual TAuthTypes requestedAuthType(void); // avoids MD5 when it cannot be checked
  // - get next nonce string top be sent to remote party for subsequent MD5 auth
  virtual void getNextNonce(const char *aDeviceID, string &aNextNonce);
  // - get nonce string, which is expected to be used by remote party for MD5 auth.
  virtual void getAuthNonce(const char *aDeviceID, string &aAuthNonce);
  #endif // SYSYNC_SERVER
  // - clean up after all login activity is over (including finishscript)
  virtual void LoginCleanUp(void);
  #endif // HAS_SQL_ADMIN
  #ifdef ODBCAPI_SUPPORT
  // current date & time
  virtual lineartime_t getDatabaseNowAs(timecontext_t aTimecontext);
  #endif
  // SQL generic
  // - do substitutions (%x-type) in a SQL string
  void DoSQLSubstitutions(string &aSQL);
  // Parameter handling
  // - reset all mapped parameters
  void resetSQLParameterMaps(void);
  // - add parameter definition to the session level parameter list
  void addSQLParameterMap(
    bool aInParam,
    bool aOutParam,
    TParamMode aParamMode,
    TItemField *aFieldP,
    TDBFieldType aDbFieldType
  );
  #ifdef ODBCAPI_SUPPORT
  // ODBC
  SQLHENV getODBCEnvironmentHandle(void);
  // - get handle to open connection (open it if not already open)
  SQLHDBC getODBCConnectionHandle(void);
  // - pull (=take ownership of) connection handle out of session into another object (datastore)
  SQLHDBC pullODBCConnectionHandle(void);
  // - check for connection-level error
  void checkConnectionError(SQLRETURN aResult);
  #endif // ODBCAPI_SUPPORT
  // SQL based agent config
  TOdbcAgentConfig *fConfigP;
  // %%% Script statement is always defined, as we need it as dummy even in SQLite only case for now
  #ifdef SCRIPT_SUPPORT
  HSTMT fScriptStatement;
  #endif
  // SQL generic utils
  // - make ODBC string from field
  bool appendFieldValueLiteral(
    TItemField &aField,TDBFieldType aDBFieldType, uInt32 aMaxSize, string &aSQL,
    TCharSets aDataCharSet, TLineEndModes aDataLineEndMode, TQuotingModes aQuotingMode,
    timecontext_t aTimeContext,
    sInt32 &aRecordSize
  );
  // - make ODBC string literal from UTF8 string
  void stringToODBCLiteralAppend(
    cAppCharP aText,
    string &aLiteral,
    TCharSets aCharSet,
    TLineEndModes aLineEndMode,
    TQuotingModes aQuotingMode,
    size_t aMaxBytes=0
  );
  // - make ODBC date/time literals from lineartime_t
  void lineartimeToODBCLiteralAppend(
    lineartime_t aTimestamp, string &aString,
    bool aWithDate, bool aWithTime,
    timecontext_t aTsContext=TCTX_UNKNOWN, timecontext_t aDBContext=TCTX_UNKNOWN
  );
  // - make integer-based literals from lineartime_t
  void lineartimeToIntLiteralAppend(
    lineartime_t aTimestamp, string &aString,
    TDBFieldType aDbfty,
    timecontext_t aTsContext=TCTX_UNKNOWN, timecontext_t aDBContext=TCTX_UNKNOWN
  );
  /* obsolete
  // - make ODBC date/time literal from timestamp
  void timeStampToODBCLiteralAppend(lineartime_t aTimeStamp, string &aString,
    bool aAsUTC, bool aWithDate, bool aWithTime);
  // - make ODBC date/time literal from struct tm
  void tmToODBCLiteralAppend(const struct tm &tim, string &aString,
    bool aWithDate, bool aWithTime);
  */
  #ifdef ODBCAPI_SUPPORT
  // ODBC utils
  // - commit and close possibly open script statement
  void commitAndCloseScriptStatement(void);
  #ifdef SCRIPT_SUPPORT
  // - get statement for executing scripted SQL
  HSTMT getScriptStatement(void);
  #endif
  // - close connection
  void closeODBCConnection(SQLHDBC &aConnHandle);
  // - check if aResult signals error and throw exception if so
  void checkODBCError(SQLRETURN aResult,SQLSMALLINT aHandleType,SQLHANDLE aHandle);
  // - check if aResult signals error and throw exception if so
  void checkStatementError(SQLRETURN aResult,SQLHSTMT aHandle);
  // - check if aResult is NO_DATA, return False if so, throw exception on error
  bool checkStatementHasData(SQLRETURN aResult,SQLHSTMT aHandle);
  // - get ODBC error message for given result code, returns false if no error
  bool getODBCError(SQLRETURN aResult, string &aMessage, string &aSQLState, SQLSMALLINT aHandleType, SQLHANDLE aHandle);
  // - get new statement handle
  SQLHSTMT newStatementHandle(SQLHDBC aConnection);
  // - get column value as database field
  bool getColumnValueAsField(
    SQLHSTMT aStatement, sInt16 aColIndex, TDBFieldType aDbfty, TItemField *aFieldP,
    TCharSets aDataCharSet, timecontext_t aTimecontext, bool aMoveToUserContext
  );
  // - get integer value, returns false if no Data or Null
  bool getColumnValueAsULong(
    SQLHSTMT aStatement,
    sInt16 aColNumber,
    uInt32 &aLongValue
  );
  bool getColumnValueAsLong(
    SQLHSTMT aStatement,
    sInt16 aColNumber,
    sInt32 &aLongValue
  );
  // - get double value, returns false if no Data or Null
  bool getColumnValueAsDouble(
    SQLHSTMT aStatement,
    sInt16 aColNumber,
    double &aDoubleValue
  );
  // - get string value, returns false if no Data or Null
  bool getColumnValueAsString(
    SQLHSTMT aStatement,
    sInt16 aColNumber,
    string &aStringValue,
    TCharSets aCharSet,
    bool aAsBlob=false
  );
  // - get time/date values, returns false if no Data or Null
  bool getColumnAsODBCTimestamp(
    SQLHSTMT aStatement,
    sInt16 aColNumber,
    SQL_TIMESTAMP_STRUCT &aODBCTimestamp
  );
  bool getColumnValueAsTimestamp(
    SQLHSTMT aStatement,
    sInt16 aColNumber,
    lineartime_t &aTimestamp
  );
  bool getColumnValueAsTime(
    SQLHSTMT aStatement,
    sInt16 aColNumber,
    lineartime_t &aTime
  );
  bool getColumnValueAsDate(
    SQLHSTMT aStatement,
    sInt16 aColNumber,
    lineartime_t &aDate
  );
  // - bind parameters (and values for IN-Params) to the statement
  void bindSQLParameters(
    TSyncSession *aSessionP,
    SQLHSTMT aStatement,
    TParameterMapList &aParamMapList, // the list of mapped parameters
    TCharSets aDataCharSet,
    TLineEndModes aDataLineEndMode
  );
  // - save out parameter values and clean up
  void saveAndCleanupSQLParameters(
    TSyncSession *aSessionP,
    SQLHSTMT aStatement,
    TParameterMapList &aParamMapList, // the list of mapped parameters
    TCharSets aDataCharSet,
    TLineEndModes aDataLineEndMode
  );
  // - bind parameters (and values for IN-Params) to the statement for session level SQL
  void bindSQLParameters(
    SQLHSTMT aStatement
  );
  // - save out parameter values and clean up for session level SQL
  void saveAndCleanupSQLParameters(
    SQLHSTMT aStatement
  );
  #endif // ODBCAPI_SUPPORT
  // SQLite utils
  #ifdef SQLITE_SUPPORT
  // - get SQLite error message for given result code; returns false if no error
  bool getSQLiteError(int aRc,string &aMessage, sqlite3 *aDb);
  // - check if aResult signals error and throw exception if so
  void checkSQLiteError(int aRc, sqlite3 *aDb);
  // - check if aRc signals error and throw exception if so; returns true if data available, false if not
  bool checkSQLiteHasData(int aRc, sqlite3 *aDb);
  // - get column value into field
  bool getSQLiteColValueAsField(
    sqlite3_stmt *aStatement, sInt16 aColIndex, TDBFieldType aDbfty, TItemField *aFieldP,
    TCharSets aDataCharSet, timecontext_t aTimecontext, bool aMoveToUserContext
  );
  // - prepare SQLite statement
  void prepareSQLiteStatement(
    cAppCharP aSQL,
    sqlite3 *aDB,
    sqlite3_stmt *&aStatement
  );
  // - bind SQLite parameters (we have ONLY in-params)!
  void bindSQLiteParameters(
    TSyncSession *aSessionP,
    sqlite3_stmt *aStatement,
    TParameterMapList &aParamMapList, // the list of mapped parameters
    TCharSets aDataCharSet,
    TLineEndModes aDataLineEndMode
  );
  #endif // SQLITE_SUPPORT
  // parameter handling routines
  // - reset all mapped parameters
  void resetSQLParameterMaps(TParameterMapList &aParamMapList);
  // - parsing of %p(fieldname,mode[,dbfieldtype]) sequence in SQL statements
  bool ParseParamSubst(
    string &aSQL, // string to parse
    string::size_type &i, // position where % sequence starts in aSQL
    string::size_type &n, // input=number of chars of % sequence without parameters, if result==true: output=number of chars to substitute at i in aSQL
    TParameterMapList &aParameterMaps, // parameter maps list to add params to
    TMultiFieldItem *aItemP // the involved item for field params
    #ifdef SCRIPT_SUPPORT
    ,TScriptContext *aScriptContextP // the script context for variable params
    #endif
  );
protected:
  #ifdef HAS_SQL_ADMIN
  // - check device ID related stuff
  virtual void CheckDevice(const char *aDeviceID);
  // - check user/pw (part of SessionLogin process)
  virtual bool CheckLogin(const char *aOriginalUserName, const char *aModifiedUserName, const char *aAuthString, TAuthSecretTypes aAuthStringType, const char *aDeviceID);
  // - remote device is analyzed, possibly save status
  virtual void remoteAnalyzed(void);
  #endif // HAS_SQL_ADMIN
  #ifdef SYSYNC_SERVER
  // - request end, used to clean up
  virtual void RequestEnded(bool &aHasData);
  #endif
private:
  #ifdef ODBCAPI_SUPPORT
  // ODBC vars
  // - session-specific connection string
  string fSessionDBConnStr;
  string fSessionDBPassword;
  // - handles
  SQLHDBC fODBCConnectionHandle;
  SQLHENV fODBCEnvironmentHandle;
  #ifdef SCRIPT_SUPPORT
  // private context for afterconnect script as it might be called nested in other scripts (e.g. triggered by SQLEXECUTE)
  TScriptContext *fAfterConnectContext;
  #endif // SCRIPT_SUPPORT
  #endif // ODBCAPI_SUPPORT
  // parameter list for execution of commands
  TParameterMapList fParameterMaps;
}; // TODBCApiAgent


} // namespace sysync

#endif // ODBCAPIAGENT_H

#endif // SQL_SUPPORT

// eof
