/**
 *  @File     odbcapiagent.cpp
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

#include "prefix_file.h"

#ifdef SQL_SUPPORT

// includes
#include "odbcapids.h"
#include "odbcapiagent.h"

#ifdef SYSYNC_TOOL
#include "syncsessiondispatch.h"
#endif

namespace sysync {


// Support for SySync Diagnostic Tool
#ifdef SYSYNC_TOOL

// execute SQL command
int execSQL(int argc, const char *argv[])
{
  if (argc<0) {
    // help requested
    CONSOLEPRINTF(("  execsql <sql statement> [<maxrows>]"));
    CONSOLEPRINTF(("    Execute SQL statement via ODBC on the server database"));
    return EXIT_SUCCESS;
  }

  TODBCApiAgent *odbcagentP = NULL;
  const char *sql = NULL;

  // show only one row by default
  sInt32 maxrows=1;

  // check for argument
  if (argc<1) {
    CONSOLEPRINTF(("argument containing SQL statement required"));
    return EXIT_FAILURE;
  }
  sql = argv[0];
  if (argc>=2) {
    // second arg is maxrows
    StrToLong(argv[1],maxrows);
  }

  // get ODBC session to work with
  odbcagentP = dynamic_cast<TODBCApiAgent *>(
    static_cast<TSyncSessionDispatch *>(getSyncAppBase())->getSySyToolSession()
  );
  if (!odbcagentP) {
    CONSOLEPRINTF(("Config does not contain an ODBC server section"));
    return EXIT_FAILURE;
  }

  // execute query
  // - show DB where we will exec it
  CONSOLEPRINTF(("Connecting to ODBC with connection string = '%s'",odbcagentP->fConfigP->fDBConnStr.c_str()));

  SQLRETURN res;
  SQLHSTMT statement=odbcagentP->newStatementHandle(odbcagentP->getODBCConnectionHandle());
  try {
    // set parameter
    const int paramsiz=100;
    SQLCHAR paramval[paramsiz];
    SQLINTEGER lenorind=SQL_NULL_DATA;
    res=SQLBindParameter(
      statement,
      1, // parameter index
      SQL_PARAM_OUTPUT, // inout
      SQL_C_CHAR, // we want it as string
      SQL_INTEGER, // parameter type
      paramsiz, // column size
      0, // decimal digits
      &paramval, // parameter value
      paramsiz, // value buffer size
      (SQLLEN*)&lenorind // length or indicator
    );
    odbcagentP->checkStatementError(res,statement);

    // issue
    res = SafeSQLExecDirect(
      statement,
      (SQLCHAR *)sql,
      SQL_NTS
    );
    odbcagentP->checkStatementError(res,statement);
    // show param
    if (lenorind!=SQL_NULL_DATA || lenorind!=SQL_NO_TOTAL) {
      CONSOLEPRINTF(("Returned parameter = '%s'",paramval));
    }

    // - get number of result columns
    SQLSMALLINT numcolumns;
    SQLSMALLINT stringlength;
    // - get number of columns
    res = SafeSQLNumResultCols(statement,&numcolumns);
    odbcagentP->checkStatementError(res,statement);
    // - fetch result row(s)
    int rownum=0;
    while (true) {
      // try to fetch row
      res=SafeSQLFetch(statement);
      if (!odbcagentP->checkStatementHasData(res,statement))
        break; // done
      // we have a row to show
      if (rownum>=maxrows) {
        CONSOLEPRINTF(("\nMore rows in result than allowed to display"));
        break;
      }
      // Show Row no
      rownum++;
      CONSOLEPRINTF(("\nResult Row #%d: ",rownum));
      // fetch data
      for (int i=1; i<=numcolumns; i++) {
        // - get name of the column
        const int maxnamelen=100;
        SQLCHAR colname[maxnamelen];
        SQLSMALLINT colnamelen;
        SQLINTEGER dummy;
        res=SQLColAttribute (
          statement,        // statement handle
          i,                // column number
          SQL_DESC_BASE_COLUMN_NAME,   // return column base name
          colname,          // col name return buffer
          maxnamelen,       // col name return buffer size
          &colnamelen,      // no string length expected
          (SQLLEN*)&dummy   // dummy
        );
        if (res!=SQL_SUCCESS) {
          strcpy((char *)colname,"<error getting name>");
        }
        // - get data of the column in this row
        const int maxdatalen=100;
        SQLCHAR databuf[maxdatalen];
        SQLINTEGER actualLength;
        res = SQLGetData(
          statement,        // statement handle
          i,                // column number
          SQL_C_CHAR,       // target type: string
          &databuf,         // Target Value buffer pointer
          maxdatalen,       // max size of value
          (SQLLEN*)&actualLength
        );
        if (res!=SQL_SUCCESS) break; // no more columns
        if (actualLength==SQL_NULL_DATA || actualLength==SQL_NO_TOTAL) {
          strcpy((char *)databuf,"<NULL>");
        }
        CONSOLEPRINTF((" %3d. %20s : %s",i,colname,databuf));
      }
    }
    CONSOLEPRINTF((""));
    SafeSQLCloseCursor(statement);
    // dispose statement handle
    SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
  }
  catch (exception &e) {
    // dispose statement handle
    SafeSQLCloseCursor(statement);
    SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
    // show error
    CONSOLEPRINTF(("ODBC Error : %s",e.what()));
    return EXIT_FAILURE;
  }
  catch (...) {
    // dispose statement handle
    SafeSQLCloseCursor(statement);
    SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
    // show error
    CONSOLEPRINTF(("ODBC caused unknown exception"));
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
} // execSQL

#endif // SYSYNC_TOOL




// names for transaction isolation modes
const char *TxnIsolModeNames[numTxnIsolModes] = {
  // Note: these MUST be in the same order as
  //   Win32/ODBC defined SQL_TXN_READ_UNCOMMITTED..SQL_TXN_SERIALIZABLE
  //   bits appear in the transaction bitmasks.
  "read-uncommitted",
  "read-committed",
  "repeatable",
  "serializable",
  // special values
  "none",
  "default"
};


// Config

TOdbcAgentConfig::TOdbcAgentConfig(TConfigElement *aParentElement) :
  TCustomAgentConfig(aParentElement)
{
  // nop so far
} // TOdbcAgentConfig::TOdbcAgentConfig


TOdbcAgentConfig::~TOdbcAgentConfig()
{
  clear();
} // TOdbcAgentConfig::~TOdbcAgentConfig


// init defaults
void TOdbcAgentConfig::clear(void)
{
  // init defaults
  #ifdef ODBCAPI_SUPPORT
  // - ODBC data source
  fDataSource.erase();
  fUsername.erase();
  fPassword.erase();
  #ifdef SCRIPT_SUPPORT
  fAfterConnectScript.erase();
  #endif
  // - usually, SQLSetConnectAttr() is not problematic
  fNoConnectAttrs=false;
  // - use medium timeout by default
  fODBCTimeout=30;
  // - use DB default
  fODBCTxnMode=txni_default;
  // - cursor library usage
  fUseCursorLib=false; // use driver (this is also the ODBC default)
  // - device table
  fGetDeviceSQL.erase();
  fNewDeviceSQL.erase();
  fSaveNonceSQL.erase();
  fSaveInfoSQL.erase();
  fSaveDevInfSQL.erase();
  fLoadDevInfSQL.erase();
  // - auth
  fUserKeySQL.erase();
  fClearTextPw=true; // otherwise, Nonce auth is not possible
  fMD5UserPass=false; // exclusive with ClearTextPw
  fMD5UPAsHex=false;
  // - datetime
  fGetCurrentDateTimeSQL.erase();
  // - statement to save log info
  fWriteLogSQL.erase();
  #endif // ODBCAPI_SUPPORT
  fQuotingMode=qm_duplsingle; // default to what was hard-coded before it became configurable in 2.1.1.5
  // clear inherited
  inherited::clear();
} // TOdbcAgentConfig::clear


#ifdef SCRIPT_SUPPORT


// ODBC agent specific script functions
// ====================================


class TODBCCommonFuncs {
public:

  // string DBLITERAL(variant value, string dbfieldtype)
  static void func_DBLiteral(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TODBCApiAgent *agentP = static_cast<TODBCApiAgent *>(aFuncContextP->getCallerContext());
    string tname,literal;
    TItemField *fieldP = aFuncContextP->getLocalVar(0); // the field to be converted
    aFuncContextP->getLocalVar(1)->getAsString(tname); // DB type string
    // search DB type, default to string
    sInt16 ty;
    TDBFieldType dbfty=dbft_string;
    if (StrToEnum(DBFieldTypeNames,numDBfieldTypes,ty,tname.c_str()))
      dbfty=(TDBFieldType)ty;
    // now create literal
    literal.erase();
    TOdbcAgentConfig *cfgP = static_cast<TODBCApiAgent *>(aFuncContextP->getCallerContext())->fConfigP;
    timecontext_t tctx;
    TCharSets chs;
    TLineEndModes lem;
    TQuotingModes qm;
    if (agentP->fScriptContextDatastore) {
      TOdbcDSConfig *dsCfgP = static_cast<TOdbcDSConfig *>(
        static_cast<TODBCApiDS *>(agentP->fScriptContextDatastore)->fConfigP
      );
      tctx=dsCfgP->fDataTimeZone;
      chs=dsCfgP->fDataCharSet;
      lem=dsCfgP->fDataLineEndMode;
      qm=dsCfgP->fQuotingMode;
    }
    else {
      tctx=cfgP->fCurrentDateTimeZone;
      chs=cfgP->fDataCharSet;
      lem=cfgP->fDataLineEndMode;
      qm=cfgP->fQuotingMode;
    }
    sInt32 s=0;
    agentP->appendFieldValueLiteral(
      *fieldP,dbfty,0,literal,
      chs, lem, qm, tctx,
      s
    );
    // return it
    aTermP->setAsString(literal);
  }; // func_DBLiteral


  #ifdef ODBCAPI_SUPPORT

  // SETDBCONNECTSTRING(string dbconnectstring)
  // sets DB connect string, useful in <logininitscript> to switch database
  // depending on user
  static void func_SetDBConnectString(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TODBCApiAgent *agentP = static_cast<TODBCApiAgent *>(aFuncContextP->getCallerContext());
    // - close the current script statement (if any)
    agentP->commitAndCloseScriptStatement();
    // - close the current connection (if any)
    agentP->closeODBCConnection(agentP->fODBCConnectionHandle);
    // - assign new DB connection string which will be used for next connection
    aFuncContextP->getLocalVar(0)->getAsString(agentP->fSessionDBConnStr); // override default DB connect string from config
  } // func_SetDBConnectString


  // SETDBPASSWORD(string password)
  // sets password, useful when using SETDBCONNECTSTRING() to switch databases
  // depending on user
  static void func_SetDBPassword(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TODBCApiAgent *agentP = static_cast<TODBCApiAgent *>(aFuncContextP->getCallerContext());
    // - assign new DB connection password which will be used for next connection
    aFuncContextP->getLocalVar(0)->getAsString(agentP->fSessionDBPassword); // override default DB password from config
  } // func_SetDBPassword

  #endif // ODBCAPI_SUPPORT



  // integer SQLEXECUTE(string statement)
  // executes statement, returns 0 or ODBC error code
  static void func_SQLExecute(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string sql;
    #ifdef ODBCAPI_SUPPORT
    SQLRETURN res;
    #endif
    TODBCApiAgent *agentP = static_cast<TODBCApiAgent *>(aFuncContextP->getCallerContext());
    TODBCApiDS *datastoreP = static_cast<TODBCApiDS *>(agentP->fScriptContextDatastore);
    aFuncContextP->getLocalVar(0)->getAsString(sql); // get SQL to be executed
    bool ok = true; // assume ok
    SQLHSTMT stmt = SQL_NULL_HANDLE;
    try {
      if (datastoreP) {
        // do datastore-level substitutions and parameter mapping
        datastoreP->resetSQLParameterMaps();
        datastoreP->DoDataSubstitutions(
          sql,
          datastoreP->fConfigP->fFieldMappings.fFieldMapList,
          0, // standard set
          false, // not for write
          false, // not for update
          aFuncContextP->fTargetItemP // item in this context, if any
        );
        #ifdef SQLITE_SUPPORT
        if (!datastoreP->fUseSQLite)
        #endif
        {
          #ifdef ODBCAPI_SUPPORT
          stmt = agentP->getScriptStatement();
          #endif // ODBCAPI_SUPPORT
        }
      }
      else {
        #ifdef ODBCAPI_SUPPORT
        // do agent-level substitutions and parameter mapping
        stmt=agentP->getScriptStatement();
        agentP->resetSQLParameterMaps();
        agentP->DoSQLSubstitutions(sql);
        #else
        // %%% no session level SQLEXECUTE for now
        aTermP->setAsBoolean(false);
        POBJDEBUGPUTSX(aFuncContextP->getSession(),DBG_ERROR,"SQLEXECUTE() can only be used in datastore context for non-ODBC!");
        return;
        #endif
      }
      // execute SQL statement
      #ifdef SYDEBUG
      if (POBJDEBUGTEST(aFuncContextP->getSession(),DBG_SCRIPTS+DBG_DBAPI)) {
        POBJDEBUGPUTSX(aFuncContextP->getSession(),DBG_DBAPI,"SQL issued by SQLEXECUTE() script function:");
        POBJDEBUGPUTSX(aFuncContextP->getSession(),DBG_DBAPI,sql.c_str());
      }
      else {
        POBJDEBUGPUTSX(aFuncContextP->getSession(),DBG_DBAPI,"SQLEXECUTE() executes a statement (hidden because script debugging is off)");
      }
      #endif
      if (datastoreP) {
        datastoreP->prepareSQLStatement(stmt,sql.c_str(),true,NULL);
        datastoreP->bindSQLParameters(stmt,true);
        // execute in datastore
        datastoreP->execSQLStatement(stmt, sql, true, NULL, true);
        // do datastore-level parameter mapping
        datastoreP->saveAndCleanupSQLParameters(stmt,true);
      }
      #ifdef ODBCAPI_SUPPORT
      else {
        agentP->bindSQLParameters(stmt);
        // execute
        res = SafeSQLExecDirect(
          stmt,
          (SQLCHAR *)sql.c_str(),
          SQL_NTS
        );
        agentP->checkStatementHasData(res,stmt); // treat NO_DATA error as ok
        // do agent-level parameter mapping
        agentP->saveAndCleanupSQLParameters(stmt);
      }
      #endif // ODBCAPI_SUPPORT
    }
    catch (exception &e) {
      POBJDEBUGPRINTFX(aFuncContextP->getSession(),DBG_ERROR,(
        "SQLEXECUTE() caused Error: %s",
        e.what()
      ));
      ok=false;
    }
    // return ok status
    aTermP->setAsBoolean(ok);
  }; // func_SQLExecute


  // integer SQLFETCHROW()
  // fetches next row, returns true if data found
  static void func_SQLFetchRow(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    #ifdef ODBCAPI_SUPPORT
    SQLRETURN res;
    #endif
    TODBCApiAgent *agentP = static_cast<TODBCApiAgent *>(aFuncContextP->getCallerContext());
    TODBCApiDS *datastoreP = static_cast<TODBCApiDS *>(agentP->fScriptContextDatastore);
    bool ok = true; // assume ok
    try {
      if (datastoreP) {
        ok=datastoreP->fetchNextRow(agentP->fScriptStatement,true); // always data access
      }
      #ifdef ODBCAPI_SUPPORT
      else {
        // - fetch result row
        res=SafeSQLFetch(agentP->getScriptStatement());
        ok=agentP->checkStatementHasData(res,agentP->getScriptStatement());
      }
      #endif
    }
    catch (exception &e) {
      POBJDEBUGPRINTFX(aFuncContextP->getSession(),DBG_ERROR,(
        "SQLFETCHROW() caused Error: %s",
        e.what()
      ));
      ok=false;
    }
    // return ok status
    aTermP->setAsBoolean(ok);
  }; // func_SQLFetchRow


  // integer SQLGETCOLUMN(integer index, &field, string dbtype)
  // gets value of next column into specified variable
  // returns true if successful
  static void func_SQLGetColumn(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TODBCApiAgent *agentP = static_cast<TODBCApiAgent *>(aFuncContextP->getCallerContext());
    TODBCApiDS *datastoreP = static_cast<TODBCApiDS *>(agentP->fScriptContextDatastore);

    bool ok = true; // assume ok
    // get column index
    sInt16 colindex = aFuncContextP->getLocalVar(0)->getAsInteger();
    // get field reference
    TItemField *fldP = aFuncContextP->getLocalVar(1);
    // get DB type for field
    string tname;
    aFuncContextP->getLocalVar(2)->getAsString(tname); // DB type string
    sInt16 ty;
    TDBFieldType dbfty=dbft_string; // default to string
    if (StrToEnum(DBFieldTypeNames,numDBfieldTypes,ty,tname.c_str()))
      dbfty=(TDBFieldType)ty;
    // get DB params
    TOdbcAgentConfig *cfgP = agentP->fConfigP;
    timecontext_t tctx;
    bool uzo;
    #ifdef SQLITE_SUPPORT
    bool sqlite=false;
    #endif
    TCharSets chs;
    if (datastoreP) {
      TOdbcDSConfig *dsCfgP = static_cast<TOdbcDSConfig *>(datastoreP->fConfigP);
      tctx=dsCfgP->fDataTimeZone;
      chs=dsCfgP->fDataCharSet;
      uzo=dsCfgP->fUserZoneOutput;
      #ifdef SQLITE_SUPPORT
      sqlite=datastoreP->fUseSQLite;
      #endif
    }
    else {
      tctx=cfgP->fCurrentDateTimeZone;
      chs=cfgP->fDataCharSet;
      uzo=false;
      #ifdef SQLITE_SUPPORT
      sqlite=false; // no SQLite support at agent level
      #endif
    }
    // get value now
    try {
      #ifdef SQLITE_SUPPORT
      if (sqlite && datastoreP) {
        // - get column value as field
        if (!agentP->getSQLiteColValueAsField(
          datastoreP->fSQLiteStmtP,
          colindex-1,  // SQLITE colindex starts at 0, not 1 like in ODBC
          dbfty, fldP,
          chs, tctx, uzo
        ))
          fldP->unAssign(); // NULL -> unassigned
      }
      else
      #endif
      {
        #ifdef ODBCAPI_SUPPORT
        // - get column value as field
        if (!agentP->getColumnValueAsField(
          agentP->getScriptStatement(), colindex, dbfty, fldP,
          chs, tctx, uzo
        ))
          fldP->unAssign(); // NULL -> unassigned
        #endif
      }
    }
    catch (exception &e) {
      POBJDEBUGPRINTFX(aFuncContextP->getSession(),DBG_ERROR,(
        "SQLGETCOLUMN() caused Error: %s",
        e.what()
      ));
      ok=false;
    }
    // return ok status
    aTermP->setAsBoolean(ok);
  }; // func_SQLGetColumn


  // SQLCOMMIT()
  // commits agent-level transactions
  static void func_SQLCommit(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TODBCApiAgent *agentP = static_cast<TODBCApiAgent *>(aFuncContextP->getCallerContext());
    TODBCApiDS *datastoreP = static_cast<TODBCApiDS *>(agentP->fScriptContextDatastore);
    if (datastoreP) {
      // we might need finalizing (for SQLite...)
      datastoreP->finalizeSQLStatement(agentP->fScriptStatement, true);
    }
    #ifdef ODBCAPI_SUPPORT
    try {
      agentP->commitAndCloseScriptStatement();
    }
    catch (exception &e) {
      POBJDEBUGPRINTFX(aFuncContextP->getSession(),DBG_ERROR,(
        "SQLCOMMIT() caused Error: %s",
        e.what()
      ));
    }
    #endif // ODBCAPI_SUPPORT
  }; // func_SQLCommit


  // SQLROLLBACK()
  // rollback agent-level transactions
  static void func_SQLRollback(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    TODBCApiAgent *agentP = static_cast<TODBCApiAgent *>(aFuncContextP->getCallerContext());
    TODBCApiDS *datastoreP = static_cast<TODBCApiDS *>(agentP->fScriptContextDatastore);
    if (datastoreP) {
      // we might need finalizing (for SQLite...)
      datastoreP->finalizeSQLStatement(agentP->fScriptStatement, true);
    }
    #ifdef ODBCAPI_SUPPORT
    try {
      if (agentP->fScriptStatement!=SQL_NULL_HANDLE) {
        // only roll back if we have used a statement at all in scripts
        SafeSQLFreeHandle(SQL_HANDLE_STMT,agentP->fScriptStatement);
        agentP->fScriptStatement=SQL_NULL_HANDLE;
        SafeSQLEndTran(SQL_HANDLE_DBC,agentP->getODBCConnectionHandle(),SQL_ROLLBACK);
      }
    }
    catch (exception &e) {
      POBJDEBUGPRINTFX(aFuncContextP->getSession(),DBG_ERROR,(
        "SQLROLLBACK() caused Error: %s",
        e.what()
      ));
    }
    #endif // ODBCAPI_SUPPORT
  }; // func_SQLRollback

}; // TODBCCommonFuncs

const uInt8 param_DBLiteral[] = { VAL(fty_none), VAL(fty_string) };
const uInt8 param_OneString[] = { VAL(fty_string) };
const uInt8 param_SQLGetColumn[] = { VAL(fty_integer), REF(fty_none), VAL(fty_string) };

// builtin function defs for ODBC database and login contexts
const TBuiltInFuncDef ODBCAgentAndDSFuncDefs[] = {
  // generic DB access
  { "DBLITERAL", TODBCCommonFuncs::func_DBLiteral, fty_string, 2, param_DBLiteral }, // note that there's a second version of this in ODBCDatastore
  #ifdef ODBCAPI_SUPPORT
  { "SETDBCONNECTSTRING", TODBCCommonFuncs::func_SetDBConnectString, fty_none, 1, param_OneString },
  { "SETDBPASSWORD", TODBCCommonFuncs::func_SetDBPassword, fty_none, 1, param_OneString },
  #endif // ODBCAPI_SUPPORT
  { "SQLEXECUTE", TODBCCommonFuncs::func_SQLExecute, fty_integer, 1, param_OneString },
  { "SQLFETCHROW", TODBCCommonFuncs::func_SQLFetchRow, fty_integer, 0, NULL },
  { "SQLGETCOLUMN", TODBCCommonFuncs::func_SQLGetColumn, fty_integer, 3, param_SQLGetColumn },
  { "SQLCOMMIT", TODBCCommonFuncs::func_SQLCommit, fty_none, 0, NULL },
  { "SQLROLLBACK", TODBCCommonFuncs::func_SQLRollback, fty_none, 0, NULL }
};


#ifdef BINFILE_ALWAYS_ACTIVE

// Binfile based version just has the SQL access functions

// function table for connectionscript
const TFuncTable ODBCAgentFuncTable = {
  sizeof(ODBCAgentAndDSFuncDefs) / sizeof(TBuiltInFuncDef), // size of table
  ODBCAgentAndDSFuncDefs, // table pointer
  NULL // no chain func
};

#else

// Full range of functions for non-binfile-based version

// chain from ODBC agent funcs to Custom agent Funcs
extern const TFuncTable CustomAgentFuncTable2;
static void *ODBCAgentChainFunc1(void *&aCtx)
{
  // caller context remains unchanged
  // -> no change needed
  // next table is Custom Agent's general function table
  return (void *)&CustomAgentFuncTable2;
} // ODBCAgentChainFunc1

// function table which is chained from login-context function table
const TFuncTable ODBCAgentFuncTable2 = {
  sizeof(ODBCAgentAndDSFuncDefs) / sizeof(TBuiltInFuncDef), // size of table
  ODBCAgentAndDSFuncDefs, // table pointer
  ODBCAgentChainFunc1 // chain to non-ODBC specific agent Funcs
};

// chain from login context agent funcs to general agent funcs
extern const TFuncTable ODBCDSFuncTable2;
static void *ODBCAgentChainFunc(void *&aCtx)
{
  // caller context remains unchanged
  // -> no change needed
  // next table is Agent's general function table
  return (void *)&ODBCAgentFuncTable2;
} // ODBCAgentChainFunc

// function table for login context scripts
// Note: ODBC agent has no login-context specific functions, but is just using those from customImplAgent
const TFuncTable ODBCAgentFuncTable = {
  sizeof(CustomAgentFuncDefs) / sizeof(TBuiltInFuncDef), // size of table
  CustomAgentFuncDefs, // table pointer
  ODBCAgentChainFunc // chain to general agent funcs.
};


#endif // not BASED_ON_BINFILE_CLIENT


// chain from agent funcs to ODBC local datastore funcs (when chained via ODBCDSFuncTable1
extern const TFuncTable ODBCDSFuncTable2;
static void *ODBCDSChainFunc1(void *&aCtx)
{
  // caller context for datastore-level functions is the datastore pointer
  if (aCtx)
    aCtx = static_cast<TODBCApiAgent *>(aCtx)->fScriptContextDatastore;
  // next table is ODBC datastore's
  return (void *)&ODBCDSFuncTable2;
} // ODBCDSChainFunc1

// function table for linking in Custom agent level functions between ODBC agent and ODBC DS level functions
const TFuncTable ODBCAgentFuncTable3 = {
  sizeof(CustomAgentAndDSFuncDefs) / sizeof(TBuiltInFuncDef), // size of agent's table
  CustomAgentAndDSFuncDefs, // table pointer to agent's general purpose (non login-context specific) funcs
  ODBCDSChainFunc1 // NOW finally chain back to ODBC datastore level DB functions
};


// chain from ODBC agent funcs to generic agent funcs and THEN back to ODBC DS funcs
extern const TFuncTable ODBCDSFuncTable2;
static void *ODBCDSChainFunc2(void *&aCtx)
{
  // caller context remains unchanged
  // -> no change needed
  // next table is Agent's general function table
  return (void *)&ODBCAgentFuncTable3;
} // ODBCDSChainFunc2

// function table which is used by ODBC datastore scripts to access agent-level funcs and then chain
// back to datastore level funcs
const TFuncTable ODBCDSFuncTable1 = {
  sizeof(ODBCAgentAndDSFuncDefs) / sizeof(TBuiltInFuncDef), // size of agent's table
  ODBCAgentAndDSFuncDefs, // table pointer to agent's general purpose (non login-context specific) funcs
  ODBCDSChainFunc2 // first chain to Custom Agent level functions, but then back to ODBC datastore level DB functions
};


#endif

// config element parsing
bool TOdbcAgentConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  #ifdef ODBCAPI_SUPPORT
  // - ODBC connection
  if (strucmp(aElementName,"datasource")==0)
    expectString(fDataSource);
  else if (strucmp(aElementName,"dbuser")==0)
    expectString(fUsername);
  else if (strucmp(aElementName,"dbpass")==0)
    expectString(fPassword);
  else if (strucmp(aElementName,"dbconnectionstring")==0)
    expectString(fDBConnStr);
  // - odbc timeout
  else if (strucmp(aElementName,"dbtimeout")==0)
    expectInt32(fODBCTimeout);
  // - transaction mode
  else if (strucmp(aElementName,"transactionmode")==0)
    expectEnum(sizeof(fODBCTxnMode),&fODBCTxnMode,TxnIsolModeNames,numTxnIsolModes);
  // - preventing use of SQLSetConnectAttr()
  else if (strucmp(aElementName,"preventconnectattrs")==0)
    expectBool(fNoConnectAttrs);
  // - cursor library usage
  else if (strucmp(aElementName,"usecursorlib")==0)
    expectBool(fUseCursorLib);
  #ifdef SCRIPT_SUPPORT
  else if (strucmp(aElementName,"afterconnectscript")==0)
    expectScript(fAfterConnectScript,aLine,getAgentFuncTableP());
  #endif
  #ifdef HAS_SQL_ADMIN
  // - device table
  else if (strucmp(aElementName,"getdevicesql")==0)
    expectString(fGetDeviceSQL);
  else if (strucmp(aElementName,"newdevicesql")==0)
    expectString(fNewDeviceSQL);
  else if (strucmp(aElementName,"savenoncesql")==0)
    expectString(fSaveNonceSQL);
  else if (strucmp(aElementName,"saveinfosql")==0)
    expectString(fSaveInfoSQL);
  else if (strucmp(aElementName,"savedevinfsql")==0)
    expectString(fSaveDevInfSQL);
  else if (strucmp(aElementName,"loaddevinfsql")==0)
    expectString(fLoadDevInfSQL);
  // - user auth SQL
  else if (strucmp(aElementName,"userkeysql")==0)
    expectString(fUserKeySQL);
  else if (strucmp(aElementName,"cleartextpw")==0)
    expectBool(fClearTextPw);
  else if (strucmp(aElementName,"md5userpass")==0)
    expectBool(fMD5UserPass);
  else if (strucmp(aElementName,"md5hex")==0)
    expectBool(fMD5UPAsHex);
  // - database time SQL
  else if (strucmp(aElementName,"timestampsql")==0)
    expectString(fGetCurrentDateTimeSQL);
  // - statement to save log info
  else if (strucmp(aElementName,"writelogsql")==0)
    expectString(fWriteLogSQL);
  else
  #endif // HAS_SQL_ADMIN
  #endif // ODBCAPI_SUPPORT
  // - quoting mode
  if (strucmp(aElementName,"quotingmode")==0)
    expectEnum(sizeof(fQuotingMode),&fQuotingMode,quotingModeNames,numQuotingModes);
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TOdbcAgentConfig::localStartElement


// resolve
void TOdbcAgentConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    #ifdef ODBCAPI_SUPPORT
    // check for required settings
    // - create fDBConnStr from dsn,user,pw if none specified explicitly
    if (fDBConnStr.empty()) {
      // Note: we do not add the password here as fDBConnStr is shown in logs.
      StringObjPrintf(fDBConnStr,"DSN=%s;UID=%s;",fDataSource.c_str(),fUsername.c_str());
    }
    #ifdef HAS_SQL_ADMIN
    if (fClearTextPw && fMD5UserPass)
      throw TConfigParseException("only one of 'cleartextpw' and 'md5userpass' can be set");
    #ifdef SYSYNC_SERVER
    if (IS_SERVER) {
      if (fAutoNonce && !fClearTextPw && !fMD5UserPass)
        throw TConfigParseException("if 'autononce' is set, 'cleartextpw' or 'md5userpass' MUST be set as well");
    }
    #endif // SYSYNC_SERVER
    #ifdef SYSYNC_CLIENT
    if (IS_CLIENT) {
      #ifndef NO_LOCAL_DBLOGIN
      if (fNoLocalDBLogin && !fUserKeySQL.empty())
        throw TConfigParseException("'nolocaldblogin' is not allowed when 'userkeysql' is defined");
      #endif
    }
    #endif // SYSYNC_CLIENT
    #endif // HAS_SQL_ADMIN
    #endif // ODBCAPI_SUPPORT
  }
  // resolve inherited
  // - Note: this resolves the ancestor's scripts first
  inherited::localResolve(aLastPass);
} // TOdbcAgentConfig::localResolve



#ifdef SCRIPT_SUPPORT
// resolve scripts
void TOdbcAgentConfig::ResolveAPIScripts(void)
{
  #ifdef ODBCAPI_SUPPORT
  // afterconnect script has it's own context as it might be called nested in other scripts (e.g. triggered by SQLEXECUTE)
  TScriptContext *ctxP = NULL;
  TScriptContext::resolveScript(getSyncAppBase(),fAfterConnectScript,ctxP,NULL);
  if (ctxP) delete ctxP;
  #endif
} // TOdbcAgentConfig::localResolve
#endif


/* public TODBCApiAgent members */


// private init routine for both client and server constructor
TODBCApiAgent::TODBCApiAgent(TSyncAppBase *aAppBaseP, TSyncSessionHandle *aSessionHandleP, cAppCharP aSessionID) :
  TCustomImplAgent(aAppBaseP, aSessionHandleP, aSessionID)
{
  // init basics
  #ifdef ODBCAPI_SUPPORT
  fODBCConnectionHandle = SQL_NULL_HANDLE;
  fODBCEnvironmentHandle = SQL_NULL_HANDLE;
  #ifdef SCRIPT_SUPPORT
  fScriptStatement = SQL_NULL_HANDLE;
  fAfterConnectContext = NULL;
  #endif
  #endif // ODBCAPI_SUPPORT
  // get config for agent and save direct link to agent config for easy reference
  fConfigP = static_cast<TOdbcAgentConfig *>(getRootConfig()->fAgentConfigP);
  // Note: Datastores are already created from config
  #ifdef ODBCAPI_SUPPORT
  // - assign default DB connection string and password
  fSessionDBConnStr = fConfigP->fDBConnStr;
  fSessionDBPassword = fConfigP->fPassword;
  #ifdef SCRIPT_SUPPORT
  // - rebuild afterconnect script
  TScriptContext::rebuildContext(getSyncAppBase(),fConfigP->fAfterConnectScript,fAfterConnectContext,this,true);
  #endif
  #endif
} // TODBCApiAgent::TODBCApiAgent


// destructor
TODBCApiAgent::~TODBCApiAgent()
{
  // make sure everything is terminated BEFORE destruction of hierarchy begins
  TerminateSession();
} // TODBCApiAgent::~TODBCApiAgent


// Terminate session
void TODBCApiAgent::TerminateSession()
{
  if (!fTerminated) {
    string msg,state;

    // Note that the following will happen BEFORE destruction of
    // individual datastores, so make sure datastore have their ODBC
    // stuff finished before disposing environment
    InternalResetSession();
    #ifdef ODBCAPI_SUPPORT
    #ifdef SCRIPT_SUPPORT
    // get rid of afterconnect context
    if (fAfterConnectContext) delete fAfterConnectContext;
    #endif
    SQLRETURN res;
    if (fODBCEnvironmentHandle!=SQL_NULL_HANDLE) {
      // release the enviroment
      res=SafeSQLFreeHandle(SQL_HANDLE_ENV,fODBCEnvironmentHandle);
      if (getODBCError(res,msg,state,SQL_HANDLE_ENV,fODBCEnvironmentHandle)) {
        DEBUGPRINTFX(DBG_ERROR,("~TODBCApiAgent: SQLFreeHandle(ENV) failed: %s",msg.c_str()));
      }
      fODBCEnvironmentHandle=SQL_NULL_HANDLE;
    }
    #endif
    // Make sure datastores know that the agent will go down soon
    announceDestruction();
  }
  inherited::TerminateSession();
} // TODBCApiAgent::TerminateSession



// Reset session
void TODBCApiAgent::InternalResetSession(void)
{
  // reset all datastores now to make sure ODBC is reset before we close the
  // global connection and the environment handle (if called by destructor)!
  // (Note: TerminateDatastores() will be called again by ancestors)
  TerminateDatastores();
  #ifdef ODBCAPI_SUPPORT
  #ifdef SCRIPT_SUPPORT
  // commit connection (possible scripted statements)
  commitAndCloseScriptStatement();
  #endif
  // clear parameter maps
  resetSQLParameterMaps();
  // close session level connection if not already closed
  closeODBCConnection(fODBCConnectionHandle);
  #endif // ODBCAPI_SUPPORT
} // TODBCApiAgent::InternalResetSession


// Virtual version
void TODBCApiAgent::ResetSession(void)
{
  // do my own stuff
  InternalResetSession();
  // let ancestor do its stuff
  inherited::ResetSession();
} // TODBCApiAgent::ResetSession


#ifdef ODBCAPI_SUPPORT
#ifdef SCRIPT_SUPPORT

// commit and close possibly open script statement
void TODBCApiAgent::commitAndCloseScriptStatement(void)
{
  if (fODBCConnectionHandle!=SQL_NULL_HANDLE) {
    // free script statement, if any still open
    if (fScriptStatement!=SQL_NULL_HANDLE) {
      PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("Script statement exists -> closing it now"));
      SafeSQLFreeHandle(SQL_HANDLE_STMT,fScriptStatement);
      fScriptStatement=SQL_NULL_HANDLE;
    }
    // now commit transaction
    SafeSQLEndTran(SQL_HANDLE_DBC,fODBCConnectionHandle,SQL_COMMIT);
  }
} // TODBCApiAgent::commitAndCloseScriptStatement


// get statement for executing scripted SQL
HSTMT TODBCApiAgent::getScriptStatement(void)
{
  if (fScriptStatement==SQL_NULL_HANDLE) {
    fScriptStatement=newStatementHandle(getODBCConnectionHandle());
  }
  return fScriptStatement;
} // TODBCApiAgent::getScriptStatement

#endif
#endif // ODBCAPI_SUPPORT


#ifdef ODBCAPI_SUPPORT

#if !defined(NO_AV_GUARDING) // && !__option(microsoft_exceptions)

#ifndef _WIN32
  #error "AV Guarding is only for Win32"
#endif

// SEH-aware versions of ODBC calls (to avoid that crashing drivers blame our server)
// ==================================================================================

// Special exception returning the SEH code
TODBCSEHexception::TODBCSEHexception(uInt32 aCode)
{
  StringObjPrintf(fMessage,"ODBC Driver caused SEH/AV Code=%08lX",aCode);
} // TODBCSEHexception::TODBCSEHexception



// define what object is to be thrown
#define SEH_THROWN_OBJECT TODBCSEHexception(GetExceptionCode())

SQLRETURN SafeSQLAllocHandle(
  SQLSMALLINT     HandleType,
  SQLHANDLE       InputHandle,
  SQLHANDLE *     OutputHandlePtr)
{
  __try {
    /*
    #ifndef RELEASE_VERSION
    // %%% causes AV
    InputHandle=0;
    *((char *)(InputHandle)) = 'X';
    #else
    #error "throw that out!! %%%%"
    #endif
    */
    return SQLAllocHandle(HandleType,InputHandle,OutputHandlePtr);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    throw SEH_THROWN_OBJECT;
  }
} // SafeSQLAllocHandle


SQLRETURN SafeSQLFreeHandle(
  SQLSMALLINT HandleType,
  SQLHANDLE   Handle)
{
  __try {
    return SQLFreeHandle(HandleType,Handle);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    throw SEH_THROWN_OBJECT;
  }
} // SafeSQLFreeHandle


SQLRETURN SafeSQLSetEnvAttr(
  SQLHENV     EnvironmentHandle,
  SQLINTEGER  Attribute,
  SQLPOINTER  ValuePtr,
  SQLINTEGER  StringLength)
{
  __try {
    return SQLSetEnvAttr(EnvironmentHandle,Attribute,ValuePtr,StringLength);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    throw SEH_THROWN_OBJECT;
  }
} // SafeSQLSetEnvAttr


SQLRETURN SafeSQLSetConnectAttr(
  SQLHDBC     ConnectionHandle,
  SQLINTEGER  Attribute,
  SQLPOINTER  ValuePtr,
  SQLINTEGER  StringLength)
{
  __try {
    return SQLSetConnectAttr(ConnectionHandle,Attribute,ValuePtr,StringLength);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    throw SEH_THROWN_OBJECT;
  }
} // SafeSQLSetConnectAttr


SQLRETURN SafeSQLConnect(
  SQLHDBC     ConnectionHandle,
  SQLCHAR *   ServerName,
  SQLSMALLINT NameLength1,
  SQLCHAR *   UserName,
  SQLSMALLINT NameLength2,
  SQLCHAR *   Authentication,
  SQLSMALLINT NameLength3)
{
  __try {
    return SQLConnect(ConnectionHandle,ServerName,NameLength1,UserName,NameLength2,Authentication,NameLength3);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    throw SEH_THROWN_OBJECT;
  }
} // SafeSQLConnect


SQLRETURN SafeSQLDriverConnect(
  SQLHDBC       ConnectionHandle,
  SQLHWND       WindowHandle,
  SQLCHAR *     InConnectionString,
  SQLSMALLINT   StringLength1,
  SQLCHAR *     OutConnectionString,
  SQLSMALLINT   BufferLength,
  SQLSMALLINT * StringLength2Ptr,
  SQLUSMALLINT  DriverCompletion)
{
  __try {
    return SQLDriverConnect(ConnectionHandle,WindowHandle,InConnectionString,StringLength1,OutConnectionString,BufferLength,StringLength2Ptr,DriverCompletion);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    throw SEH_THROWN_OBJECT;
  }
} // SafeSQLDriverConnect


SQLRETURN SafeSQLGetInfo(
  SQLHDBC       ConnectionHandle,
  SQLUSMALLINT  InfoType,
  SQLPOINTER    InfoValuePtr,
  SQLSMALLINT   BufferLength,
  SQLSMALLINT * StringLengthPtr)
{
  __try {
    return SQLGetInfo(ConnectionHandle,InfoType,InfoValuePtr,BufferLength,StringLengthPtr);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    throw SEH_THROWN_OBJECT;
  }
} // SafeSQLGetInfo


SQLRETURN SafeSQLEndTran(
  SQLSMALLINT HandleType,
  SQLHANDLE   Handle,
  SQLSMALLINT CompletionType)
{
  __try {
    return SQLEndTran(HandleType,Handle,CompletionType);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    throw SEH_THROWN_OBJECT;
  }
} // SafeSQLEndTran


SQLRETURN SafeSQLExecDirect(
  SQLHSTMT    StatementHandle,
  SQLCHAR *   StatementText,
  SQLINTEGER  TextLength)
{
  __try {
    return SQLExecDirect(StatementHandle,StatementText,TextLength);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    throw SEH_THROWN_OBJECT;
  }
} // SafeSQLExecDirect


#ifdef ODBC_UNICODE

SQLRETURN SafeSQLExecDirectW(
  SQLHSTMT    StatementHandle,
  SQLWCHAR *  StatementText,
  SQLINTEGER  TextLength)
{
  __try {
    return SQLExecDirectW(StatementHandle,StatementText,TextLength);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    throw SEH_THROWN_OBJECT;
  }
} // SafeSQLExecDirectW

#endif


SQLRETURN SafeSQLFetch(
  SQLHSTMT  StatementHandle)
{
  __try {
    return SQLFetch(StatementHandle);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    throw SEH_THROWN_OBJECT;
  }
} // SafeSQLFetch


SQLRETURN SafeSQLNumResultCols(
  SQLHSTMT      StatementHandle,
  SQLSMALLINT * ColumnCountPtr)
{
  __try {
    return SQLNumResultCols(StatementHandle,ColumnCountPtr);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    throw SEH_THROWN_OBJECT;
  }
} // SafeSQLNumResultCols


SQLRETURN SafeSQLGetData(
  SQLHSTMT      StatementHandle,
  SQLUSMALLINT  ColumnNumber,
  SQLSMALLINT   TargetType,
  SQLPOINTER    TargetValuePtr,
  SQLINTEGER    BufferLength,
  SQLLEN *      StrLen_or_IndPtr)
{
  __try {
    return SQLGetData(StatementHandle,ColumnNumber,TargetType,TargetValuePtr,BufferLength,StrLen_or_IndPtr);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    throw SEH_THROWN_OBJECT;
  }
} //


SQLRETURN SafeSQLCloseCursor(
  SQLHSTMT  StatementHandle)
{
  __try {
    return SQLCloseCursor(StatementHandle);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    throw SEH_THROWN_OBJECT;
  }
} // SafeSQLCloseCursor


#endif // AV Guarding

#endif // ODBCAPI_SUPPORT


// append field value as literal to SQL text
// - returns true if field(s) were not empty
// - even non-existing or empty field will append at least NULL or '' to SQL
bool TODBCApiAgent::appendFieldValueLiteral(
  TItemField &aField,TDBFieldType aDBFieldType, uInt32 aMaxSize, string &aSQL,
  TCharSets aDataCharSet, TLineEndModes aDataLineEndMode, TQuotingModes aQuotingMode,
  timecontext_t aTimeContext,
  sInt32 &aRecordSize
)
{
  bool dat=false;
  bool tim=false;
  bool intts=false;
  string val;
  TTimestampField *tsFldP;
  sInt32 factor;
  sInt32 sz;
  sInt16 moffs;
  lineartime_t ts;
  timecontext_t tctx;

  bool isempty=aField.isEmpty();
  if (
    isempty &&
    aDBFieldType!=dbft_string
  ) {
    // non-string field does not have a value: NULL
    aSQL+="NULL";
  }
  else {
    switch (aDBFieldType) {
      // numeric time offsets
      case dbft_uctoffsfortime_hours:
        factor = 0; // special case, float
        goto timezone;
      case dbft_uctoffsfortime_mins:
        factor = 1;
        goto timezone;
      case dbft_uctoffsfortime_secs:
        factor = SecsPerMin;;
        goto timezone;
      case dbft_zonename:
        factor = -1; // name, not offset
      timezone:
        // get field
        if (!aField.isBasedOn(fty_timestamp))
          goto nullfield; // no timestamp -> no zone
        tsFldP = static_cast<TTimestampField *>(&aField);
        // get name or offset
        tctx = tsFldP->getTimeContext();
        if (factor>=0) {
          // offset requested
          if (tsFldP->isFloating()) goto nullfield; // floating -> no offset
          if (!TzResolveToOffset(tctx,moffs,tsFldP->getTimestampAs(TCTX_UNKNOWN),false,tsFldP->getGZones()))
            goto nullfield; // cannot calc offset -> no zone
          if (factor==0)
            StringObjAppendPrintf(aSQL,"%g",(sInt32)moffs/60.0); // make hours with fraction
          else
            StringObjAppendPrintf(aSQL,"%ld",(long)moffs * factor); // mins or seconds
        }
        else {
          // name requested
          if (!TCTX_IS_DURATION(tctx) && !TCTX_IS_DATEONLY(tctx) && TCTX_IS_UNKNOWN(tctx))
            goto nullfield; // really floating (not duration or dateonly) -> no zone (and not "FLOATING" string we'd get from TimeZoneContextToName)
          TimeZoneContextToName(tctx, val, tsFldP->getGZones());
          goto asstring;
        }
        break;
      // date and time values
      case dbft_lineardate:
      case dbft_unixdate_s:
      case dbft_unixdate_ms:
      case dbft_unixdate_us:
        intts=true; // integer timestamp
      case dbft_date: // date-only field
        dat=true;
        goto settimestamp;
      case dbft_timefordate:
      case dbft_time:
        tim=true;
        goto settimestamp;
      case dbft_lineartime:
      case dbft_unixtime_s:
      case dbft_nsdate_s:
      case dbft_unixtime_ms:
      case dbft_unixtime_us:
        intts=true; // integer timestamp
      case dbft_dateonly: // date-only, but stored as timestamp
      case dbft_timestamp:
        dat=true;
        tim=true;
      settimestamp:
        // get timestamp in DB time zone
        if (aField.isBasedOn(fty_timestamp)) {
          // get it from field in specified zone (or floating)
          ts=static_cast<TTimestampField *>(&aField)->getTimestampAs(aTimeContext,&tctx);
        }
        else {
          // try to convert ISO8601 string representation
          aField.getAsString(val);
          ISO8601StrToTimestamp(val.c_str(), ts, tctx);
          TzConvertTimestamp(ts,tctx,aTimeContext,getSessionZones(),aTimeContext);
        }
        // remove time part on date-only
        if (dat & !tim)
          ts = lineartime2dateonlyTime(ts);
        if (intts) {
          // Timestamp represented as integer in the DB
          // - add as integer timestamp
//          sInt64 ii = lineartimeToDbInt(ts,aDBFieldType);
//          PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,(
//            "ts=%lld -> dbInteger=%lld/0x%llX (UnixToLineartimeOffset=%lld, secondToLinearTimeFactor=%lld)",
//            ts,ii,ii,UnixToLineartimeOffset,secondToLinearTimeFactor
//          ));
//          StringObjAppendPrintf(aSQL,PRINTF_LLD,PRINTF_LLD_ARG(ii));
          StringObjAppendPrintf(aSQL,PRINTF_LLD,PRINTF_LLD_ARG(lineartimeToDbInt(ts,aDBFieldType)));
        }
        else {
          // add as ODBC date/time literal
          lineartimeToODBCLiteralAppend(ts, aSQL, dat, tim);
        }
        break;
      case dbft_numeric:
        // numeric fields are copied to SQL w/o quotes
        aField.getAsString(val);
        aSQL.append(val); // just append
        break;
      case dbft_blob:
        // BLOBs cannot be written literally (should never occur, as we automatically parametrize them)
        throw TSyncException("FATAL: BLOB fields must be written with parameters");
        break;
      case dbft_string:
      default:
        // Database field is string (or unknown), add it as string literal
        aField.getAsString(val);
        // only net string sizes are counted
        sz=val.size(); // net size of string
        if (sz>sInt32(aMaxSize)) sz=aMaxSize; // limit to what can be actually stored
        aRecordSize+=sz; // add to count
      asstring:
        stringToODBCLiteralAppend(
          val.c_str(),
          aSQL,
          aDataCharSet,
          aDataLineEndMode,
          aQuotingMode,
          aMaxSize
        );
        break;
      nullfield:
        aSQL+="NULL";
        break;
    } // switch
  } // field has a value
  return !isempty;
} // TODBCApiAgent::appendFieldValueLiteral


// - make ODBC string literal from UTF8 string
void TODBCApiAgent::stringToODBCLiteralAppend(
  cAppCharP aText,
  string &aLiteral,
  TCharSets aCharSet,
  TLineEndModes aLineEndMode,
  TQuotingModes aQuotingMode,
  size_t aMaxBytes
)
{
  aLiteral+='\'';
  appendUTF8ToString(
    aText,aLiteral,
    aCharSet, // charset
    aLineEndMode, // line end mode
    aQuotingMode, // quoting mode
    aMaxBytes // max size (0 if unlimited)
  );
  aLiteral+='\'';
} // TODBCApiAgent::stringToODBCLiteralAppend


// - make ODBC date/time literals from lineartime_t
void TODBCApiAgent::lineartimeToODBCLiteralAppend(
  lineartime_t aTimestamp, string &aString,
  bool aWithDate, bool aWithTime,
  timecontext_t aTsContext, timecontext_t aDBContext
)
{
  // make correct zone if needed
  if (!TCTX_IS_UNKNOWN(aTsContext)) {
    TzConvertTimestamp(aTimestamp,aTsContext,aDBContext,getSessionZones(),TCTX_UNKNOWN);
  }
  // calculate components
  sInt16 y,mo,d,h,mi,s,ms;
  lineartime2date(aTimestamp,&y,&mo,&d);
  lineartime2time(aTimestamp,&h,&mi,&s,&ms);
  // create prefix
  aString+='{';
  if (aWithDate && aWithTime)
    aString+="ts";
  else if (aWithTime)
    aString+="t";
  else
    aString+="d";
  aString+=" '";
  // add date if selected
  if (aWithDate) {
    StringObjAppendPrintf(
      aString,"%04d-%02d-%02d",
      y, mo, d
    );
  }
  // add time if selected
  if (aWithTime) {
    if (aWithDate) aString+=' '; // separate
    StringObjAppendPrintf(aString,
      "%02d:%02d:%02d",
      h, mi, s
    );
    // microseconds, if any
    if (ms!=0) {
      StringObjAppendPrintf(aString,".%03d",ms);
    }
  }
  // suffix
  aString+="'}";
} // TODBCApiAgent::lineartimeToODBCLiteralAppend


// - make integer-based literals from lineartime_t
void TODBCApiAgent::lineartimeToIntLiteralAppend(
  lineartime_t aTimestamp, string &aString,
  TDBFieldType aDbfty,
  timecontext_t aTsContext, timecontext_t aDBContext
)
{
  // make correct zone if needed
  if (!TCTX_IS_UNKNOWN(aTsContext)) {
    TzConvertTimestamp(aTimestamp,aTsContext,aDBContext,getSessionZones(),TCTX_UNKNOWN);
  }
  // - add as integer timestamp
  StringObjAppendPrintf(aString,PRINTF_LLD,PRINTF_LLD_ARG(lineartimeToDbInt(aTimestamp,aDbfty)));
} // TODBCApiAgent::lineartimeToIntLiteralAppend



/*%%% obsolete
// - make ODBC date/time literals from UTC timestamp
void TODBCApiAgent::timeStampToODBCLiteralAppend(lineartime_t aTimeStamp, string &aString, bool aAsUTC, bool aWithDate, bool aWithTime)
{
  if (aTimeStamp!=0) {
    if (!aAsUTC)
      aTimeStamp=makeLocalTimestamp(aTimeStamp); // convert to local time
    struct tm tim;
    lineartime2tm(aTimeStamp,&tim);
    // format as ODBC date/time literal
    tmToODBCLiteralAppend(tim,aString,aWithDate,aWithTime);
  }
  else {
    aString.append("NULL");
  }
} // TODBCApiAgent::timeStampToODBCLiteralAppend

// - make ODBC date/time literals from struct tm
void TODBCApiAgent::tmToODBCLiteralAppend(const struct tm &tim, string &aString, bool aWithDate, bool aWithTime)
{
  // create prefix
  aString+='{';
  if (aWithDate && aWithTime)
    aString+="ts";
  else if (aWithTime)
    aString+="t";
  else
    aString+="d";
  aString+=" '";
  // add date if selected
  if (aWithDate) {
    StringObjAppendPrintf(
      aString,"%04d-%02d-%02d",
      tim.tm_year+1900,
      tim.tm_mon+1,
      tim.tm_mday
    );
  }
  // add time if selected
  if (aWithTime) {
    if (aWithDate) aString+=' '; // separate
    StringObjAppendPrintf(aString,
      "%02d:%02d:%02d",
      tim.tm_hour,
      tim.tm_min,
      tim.tm_sec
    );
  }
  // suffix
  aString+="'}";
} // TODBCApiAgent::tmToODBCLiteralAppend
*/



// - return quoted version of string if aDoQuote is set
// bfo: Problems with XCode (expicit qualification), already within namespace ?
//const char *sysync::quoteString(string &aIn, string &aOut, TQuotingModes aQuoteMode)
const char *quoteString(string &aIn, string &aOut, TQuotingModes aQuoteMode)
{
  return quoteString(aIn.c_str(),aOut,aQuoteMode);
} // TODBCApiAgent::quoteString


// - return quoted version of string if aDoQuote is set
// bfo: Problems with XCode (expicit qualification), already within namespace ?
//const char *sysync::quoteString(const char *aIn, string &aOut, TQuotingModes aQuoteMode)
const char *quoteString(const char *aIn, string &aOut, TQuotingModes aQuoteMode)
{
  aOut.erase();
  quoteStringAppend(aIn,aOut,aQuoteMode);
  return aOut.c_str();
} // TODBCApiAgent::quoteString


// - append quoted version of string if aDoQuote is set
// bfo: Problems with XCode (expicit qualification), already within namespace ?
//void sysync::quoteStringAppend(const char *aIn, string &aOut, TQuotingModes aQuoteMode)
void quoteStringAppend(const char *aIn, string &aOut, TQuotingModes aQuoteMode)
{
  if (!aQuoteMode!=qm_none) aOut.append(aIn);
  else {
    sInt16 n=strlen(aIn);
    aOut.reserve(n+2);
    aOut+='\'';
    appendUTF8ToString(
      aIn,
      aOut,
      chs_ascii, // charset
      lem_cstr, // line end mode
      aQuoteMode, // quoting mode
      0 // max size (0 if unlimited)
    );
    aOut+='\'';
  }
} // TODBCApiAgent::quoteStringAppend


// - append quoted version of string if aDoQuote is set
// bfo: Problems with XCode (expicit qualification), already within namespace ?
//void sysync::quoteStringAppend(string &aIn, string &aOut, TQuotingModes aQuoteMode)
void quoteStringAppend(string &aIn, string &aOut, TQuotingModes aQuoteMode)
{
  quoteStringAppend(aIn.c_str(),aOut,aQuoteMode);
} // TODBCApiAgent::quoteStringAppend


// reset all mapped parameters
void TODBCApiAgent::resetSQLParameterMaps(TParameterMapList &aParamMapList)
{
  TParameterMapList::iterator pos;
  for (pos=aParamMapList.begin();pos!=aParamMapList.end();++pos) {
    // clean up entry
    if (pos->mybuffer && pos->ParameterValuePtr) {
      // delete buffer if we have allocated one
      sysync_free(pos->ParameterValuePtr);
      pos->ParameterValuePtr=NULL;
      pos->mybuffer=false;
    }
  }
  // now clear list
  aParamMapList.clear();
} // TODBCApiAgent::resetSQLParameterMaps


// parsing of %p(mode,var_or_field[,dbfieldtype[,maxcolsize]]) sequence
bool TODBCApiAgent::ParseParamSubst(
  string &aSQL, // string to parse
  string::size_type &i, // input=position where % sequence starts in aSQL, output = if result==false: where to continue parsing, else: where to substitute
  string::size_type &n, // input=number of chars of % sequence possibly with "(" but nothing more, if result==true: output=number of chars to substitute at i in aSQL
  TParameterMapList &aParameterMaps, // parameter maps list to add params to
  TMultiFieldItem *aItemP // the involved item for field params
  #ifdef SCRIPT_SUPPORT
  ,TScriptContext *aScriptContextP // the script context for variable params
  #endif
)
{
  string::size_type j,k,h;

  //  %p(mode,fieldname,dbfieldtype) = field as SQL parameter, where mode can be "i","o" or "io"
  j=i+n;
  // find closing paranthesis
  k = aSQL.find(")",j);
  if (k==string::npos) { i=j; n=0; return false; } // no closing paranthesis, do not substitute
  // get mode
  bool paramin=false,paramout=false;
  paramin=false;
  paramout=false;
  if (tolower(aSQL[j]=='i')) {
    paramin=true;
    if (tolower(aSQL[j+1])=='o') {
      paramout=true;
      ++j;
    }
    ++j;
  }
  else if (tolower(aSQL[j]=='o')) {
    paramout=true;
    ++j;
  }
  // now get item field or variable name
  if (aSQL[j]!=',') { i=k+1; n=0; return false; } // continue after closing paranthesis
  ++j;
  // extract name (without possible array index)
  h = aSQL.find(",",j);
  if (h==string::npos) { h=k; } // no second comma, only field name, use default dbfieldtype
  string fldname;
  fldname.assign(aSQL,j,h-j);
  j=h+1; // after , or )
  // get fieldtype (default if none specified)
  TDBFieldType dbfty=dbft_string; // default to string
  uInt32 colmaxsize=0;
  if (h<k) {
    // more params specified (database field type and possibly column size)
    h = aSQL.find(",",j);
    if (h==string::npos) { h=k; } // no third comma, only field type, use default column size)
    // get database field type
    string tyname;
    tyname.assign(aSQL,j,h-j);
    sInt16 ty;
    if (StrToEnum(DBFieldTypeNames,numDBfieldTypes,ty,tyname.c_str()))
      dbfty=(TDBFieldType)ty;
    j=h+1; // after , or )
    // get column max size (default if none specified)
    if (h<k) {
      // column size specified
      StrToULong(aSQL.c_str()+j,colmaxsize,k-j);
    }
  }
  // find field or var
  TItemField *fldP=NULL;
  #ifdef SCRIPT_SUPPORT
  if (aScriptContextP) {
    sInt16 idx=aScriptContextP->getIdentifierIndex(OBJ_AUTO, aItemP ? aItemP->getFieldDefinitions() : NULL ,fldname.c_str());
    fldP=aScriptContextP->getFieldOrVar(aItemP,idx,0);
  }
  else
  #endif
    if (aItemP) fldP = aItemP->getArrayField(fldname.c_str(),0,true);
  if (!fldP) { i=k+1; n=0; return false; } // no suitable mapping
  // now map as parameter
  TParameterMap map;
  // assign basics
  map.inparam=paramin;
  map.outparam=paramout;
  map.parammode=param_field;
  map.mybuffer=false;
  map.ParameterValuePtr=NULL;
  map.BufferLength=0;
  map.StrLen_or_Ind=SQL_NULL_DATA; // note that this is not zero (but -1)
  map.itemP=aItemP;
  map.fieldP=fldP;
  map.maxSize=colmaxsize;
  map.dbFieldType=dbfty;
  // save in list
  aParameterMaps.push_back(map);
  // set substitution parameters
  n=k+1-i;
  // ok, substitute
  return true;
} // TODBCApiAgent::ParseParamSubst


// do generic substitutions
void TODBCApiAgent::DoSQLSubstitutions(string &aSQL)
{
  #ifndef BINFILE_ALWAYS_ACTIVE
  if (!binfilesActive()) {
    // substitute: %u = userkey
    StringSubst(aSQL,"%u",fUserKey,2,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
    // substitute: %d = devicekey
    StringSubst(aSQL,"%d",fDeviceKey,2,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
    #ifdef SCRIPT_SUPPORT
    // substitute: %C = domain name (such as company selector)
    StringSubst(aSQL,"%C",fDomainName,2,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
    #endif
  }
  #endif // not BINFILE_ALWAYS_ACTIVE
  #ifdef SCRIPT_SUPPORT
  // substitute %sv(sessionvarname) = session variable by name
  string::size_type i=0;
  while((i=aSQL.find("%sv(",i))!=string::npos) {
    string s;
    // skip lead-in
    string::size_type j=i+4;
    // find closing paranthesis
    string::size_type k = aSQL.find(")",j);
    if (k==string::npos) { i=j; continue; } // no closing paranthesis
    sInt32 m = k; // assume end of name is here
    // extract name
    s.assign(aSQL,j,m-j);
    // find session var with this name
    if (!fSessionScriptContextP) { i=j; continue; }
    TItemField *fldP = fSessionScriptContextP->getFieldOrVar(
      NULL,
      fSessionScriptContextP->getIdentifierIndex(OBJ_LOCAL,NULL,s.c_str(),s.size())
    );
    if (!fldP) { i=j; continue; } // field not found, no action
    // get field contents
    fldP->getAsString(s);
    // subsititute (starting with "%v(", ending with ")" )
    aSQL.replace(i,k+1-i,s); i+=s.size();
  }
  // substitute %p(mode,var_or_field[,dbfieldtype]) = map field to parameter
  // - as the only source of params in session-level SQL is the %p() sequence, we
  //   can clear the list here (to make sure no param from another call remains active)
  // - if this is called from Datastore context, %p() sequences are already substituted, and
  //   therefore no parameter list mixing will occur
  resetSQLParameterMaps();
  i=0;
  while((i=aSQL.find("%p(",i))!=string::npos) {
    string::size_type n=3; // size of base sequence %p(
    if (!ParseParamSubst(
      aSQL,i,n,
      fParameterMaps,
      NULL
      #ifdef SCRIPT_SUPPORT
      ,fAgentContext
      #endif
    )) break;
    // subsititute param spec with single question mark
    aSQL.replace(i,n,"?"); i+=1;
  }
  #endif
} // TODBCApiAgent::DoSQLSubstitutions


// reset all mapped parameters
void TODBCApiAgent::resetSQLParameterMaps(void)
{
  resetSQLParameterMaps(fParameterMaps);
} // TODBCApiAgent::resetSQLParameterMaps


// add parameter definition to the session level parameter list
void TODBCApiAgent::addSQLParameterMap(
  bool aInParam,
  bool aOutParam,
  TParamMode aParamMode,
  TItemField *aFieldP,
  TDBFieldType aDbFieldType
)
{
  TParameterMap map;

  // assign basics
  map.inparam=aInParam;
  map.outparam=aOutParam;
  map.parammode=aParamMode;
  map.mybuffer=false;
  map.ParameterValuePtr=NULL;
  map.BufferLength=0;
  map.StrLen_or_Ind=SQL_NULL_DATA; // note that this is not zero (but -1)
  map.itemP=NULL; // no item
  map.fieldP=aFieldP;
  map.maxSize=std_paramsize;
  map.dbFieldType=aDbFieldType;
  map.outSiz=0;
  // save in list
  fParameterMaps.push_back(map);
} // TODBCApiAgent::addSQLParameterMap


// ODBC Utils
// ==========

#ifdef ODBCAPI_SUPPORT

// get existing or create new ODBC environment handle
SQLHENV TODBCApiAgent::getODBCEnvironmentHandle(void)
{
  if (fODBCEnvironmentHandle==SQL_NULL_HANDLE) {
    // create one environment handle for the session
    if (SafeSQLAllocHandle(
      SQL_HANDLE_ENV, SQL_NULL_HANDLE, &fODBCEnvironmentHandle
    ) != SQL_SUCCESS) {
      // problem
      throw TSyncException("Cannot allocated ODBC environment handle");
    }
    // Set ODBC 3.0 (needed, else function sequence error will occur)
    if (SafeSQLSetEnvAttr(
      fODBCEnvironmentHandle, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0
    ) != SQL_SUCCESS) {
      // problem
      throw TSyncException("Cannot set environment to ODBC 3.0");
    }
  }
  // return the handle
  return fODBCEnvironmentHandle;
} // TODBCApiAgent::getODBCEnvironmentHandle


// check for connection-level error
void TODBCApiAgent::checkConnectionError(SQLRETURN aResult)
{
  checkODBCError(aResult,SQL_HANDLE_DBC,fODBCConnectionHandle);
} // TODBCApiAgent::checkConnectionError



// get handle to open connection
SQLHDBC TODBCApiAgent::getODBCConnectionHandle(void)
{
  SQLRETURN res;

  if (fODBCConnectionHandle==SQL_NULL_HANDLE) {
    // no connection exists, allocate new Connection Handle
    PDEBUGPRINTFX(DBG_DBAPI,("Trying to open new ODBC connection with ConnStr = '%s'",fSessionDBConnStr.c_str()));
    // - get environment handle (session global)
    SQLHENV envhandle=getODBCEnvironmentHandle();
    if (envhandle==SQL_NULL_HANDLE)
      throw TSyncException("No environment handle available");
    // - allocate connection handle
    res=SafeSQLAllocHandle(
      SQL_HANDLE_DBC,
      envhandle,
      &fODBCConnectionHandle
    );
    checkODBCError(res,SQL_HANDLE_ENV,envhandle);
    try {
      // Some ODBC drivers (apparently MyODBC 3.51 on Mac OS X 10.5.2) crash when using SQLSetConnectAttr
      if (!fConfigP->fNoConnectAttrs) {
        // Set Connection attributes
        // - Make sure no dialog boxes are ever shown
        res=SafeSQLSetConnectAttr(fODBCConnectionHandle,SQL_ATTR_QUIET_MODE,NULL,0);
        checkConnectionError(res);
        // - Cursor library usage (default is NO)
        res=SafeSQLSetConnectAttr(fODBCConnectionHandle,SQL_ATTR_ODBC_CURSORS,(void*)(fConfigP->fUseCursorLib ? SQL_CUR_USE_ODBC : SQL_CUR_USE_DRIVER),0);
        checkConnectionError(res);
        // - Commit mode manually
        res=SafeSQLSetConnectAttr(fODBCConnectionHandle,SQL_ATTR_AUTOCOMMIT,(void*)SQL_AUTOCOMMIT_OFF,0);
        checkConnectionError(res);
        // - Set a timeout
        res=SafeSQLSetConnectAttr(fODBCConnectionHandle,SQL_ATTR_CONNECTION_TIMEOUT,(void*)fConfigP->fODBCTimeout,0);
        checkConnectionError(res);
      }
      // Now connect, before setting transaction stuff
      // - append password to configured connection string
      string connstr;
      connstr=fSessionDBConnStr.c_str();
      if (!fSessionDBPassword.empty()) {
        connstr+="PWD=";
        connstr+=fSessionDBPassword;
        connstr+=';';
      }
      const SQLSMALLINT outStrMax=1024;
      SQLCHAR outStr[outStrMax];
      SQLSMALLINT outStrSiz;
      res=SafeSQLDriverConnect(
        fODBCConnectionHandle, // connection handle
        NULL, // no windows handle
        (SQLCHAR *)connstr.c_str(), // input string
        connstr.size(),
        outStr, // output string
        outStrMax,
        &outStrSiz,
        SQL_DRIVER_NOPROMPT
      );
      checkConnectionError(res);
      // Note: the following may show the password, so it MUST NOT be a PDEBUGxxx, but a DEBUGxxx !
      DEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("SQLDriverConnect returns connection string = '%s'",outStr));
      // Now configure transactions
      #ifdef SYDEBUG
      if (PDEBUGMASK) {
        string msg,state;
        // - check what DB can offer for isolation levels
        SQLUINTEGER txnmask;
        res=SafeSQLGetInfo(fODBCConnectionHandle,SQL_TXN_ISOLATION_OPTION,&txnmask,0,NULL);
        if (getODBCError(res,msg,state,SQL_HANDLE_DBC,fODBCConnectionHandle)) {
          PDEBUGPRINTFX(DBG_ERROR,("SQLGetInfo for SQL_TXN_ISOLATION_OPTION failed: %s",msg.c_str()));
        } else {
          PDEBUGPRINTFX(DBG_DBAPI,("ODBC source's Transaction support mask = 0x%04lX",txnmask));
        }
        // - check standard isolation level
        SQLUINTEGER txn;
        res=SafeSQLGetInfo(fODBCConnectionHandle,SQL_DEFAULT_TXN_ISOLATION,&txn,0,NULL);
        if (getODBCError(res,msg,state,SQL_HANDLE_DBC,fODBCConnectionHandle)) {
          DEBUGPRINTFX(DBG_ERROR,("SQLGetInfo for SQL_DEFAULT_TXN_ISOLATION failed: %s",msg.c_str()));
        }
        else {
          DEBUGPRINTF(("ODBC source's standard isolation mask  = 0x%04lX",txn));
        }
      }
      #endif
      // - set isolation level
      if (fConfigP->fODBCTxnMode!=txni_default) {
        SQLUINTEGER txnmode = 0; // none by default
        if (fConfigP->fODBCTxnMode<txni_none) {
          txnmode = 1 << (fConfigP->fODBCTxnMode);
        }
        PDEBUGPRINTFX(DBG_DBAPI,("Setting SQL_ATTR_TXN_ISOLATION to      = 0x%04lX",txnmode));
        res=SafeSQLSetConnectAttr(fODBCConnectionHandle,SQL_ATTR_TXN_ISOLATION,(void*)txnmode,0);
        checkConnectionError(res);
      }
      // done!
      PDEBUGPRINTFX(DBG_DBAPI,("Created and opened new ODBC connection (timeout=%ld sec)",fConfigP->fODBCTimeout));
      #ifdef SCRIPT_SUPPORT
      // save datastore context (as this script can be executed implicitly from another scripts SQLEXECUTE())
      TCustomImplDS *currScriptDS = fScriptContextDatastore;
      // now execute afterconnect script, if any
      fScriptContextDatastore=NULL; // connecting is not datastore related
      TScriptContext::execute(
        fAfterConnectContext,
        fConfigP->fAfterConnectScript,
        fConfigP->getAgentFuncTableP(),  // context function table
        (void *)this // context data (myself)
      );
      // restore datastore context as it was before
      fScriptContextDatastore = currScriptDS;
      #endif
    }
    catch(...) {
      // connection is not usable, dispose handle again
      SafeSQLFreeHandle(SQL_HANDLE_DBC,fODBCConnectionHandle);
      fODBCConnectionHandle=SQL_NULL_HANDLE;
      throw;
    }
  }
  PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("Session: using connection handle 0x%lX",(uIntArch)fODBCConnectionHandle));
  return fODBCConnectionHandle;
} // TODBCApiAgent::getODBCConnectionHandle


// pull connection handle out of session into another object (datastore)
SQLHDBC TODBCApiAgent::pullODBCConnectionHandle(void)
{
  #ifdef SCRIPT_SUPPORT
  // make sure possible script statement gets disposed, as connection will be owned by datastore from now on
  // Note: the script statement must be closed here already, because creating a new connection with getODBCConnectionHandle()
  //   might trigger afterconnectscript, which in turn may use SQLEXECUTE() will ask for the current script statement.
  //   If the script statement was not closed before that, SQLEXECUTE() would execute on the old statement and hence on
  //   the old connection rather than on the new one.
  commitAndCloseScriptStatement();
  #endif
  SQLHDBC connhandle = getODBCConnectionHandle();
  fODBCConnectionHandle = SQL_NULL_HANDLE; // owner is caller, must do closing and disposing
  return connhandle;
} // TODBCApiAgent::pullODBCConnectionHandle


// close connection
void TODBCApiAgent::closeODBCConnection(SQLHDBC &aConnHandle)
{
  string msg,state;
  SQLRETURN res;

  if (aConnHandle!=SQL_NULL_HANDLE) {
    // Roll back to make sure we don't leave a unfinished transaction
    res=SafeSQLEndTran(SQL_HANDLE_DBC,aConnHandle,SQL_ROLLBACK);
    if (getODBCError(res,msg,state,SQL_HANDLE_DBC,aConnHandle)) {
      PDEBUGPRINTFX(DBG_ERROR,("closeODBCConnection: SQLEndTran failed: %s",msg.c_str()));
    }
    // Actually disconnect
    res=SQLDisconnect(aConnHandle);
    if (getODBCError(res,msg,state,SQL_HANDLE_DBC,aConnHandle)) {
      PDEBUGPRINTFX(DBG_ERROR,("closeODBCConnection: SQLDisconnect failed: %s",msg.c_str()));
    }
    // free the connection handle
    res=SafeSQLFreeHandle(SQL_HANDLE_DBC,aConnHandle);
    if (getODBCError(res,msg,state,SQL_HANDLE_DBC,aConnHandle)) {
      PDEBUGPRINTFX(DBG_ERROR,("closeODBCConnection: SQLFreeHandle(DBC) failed: %s",msg.c_str()));
    }
    aConnHandle=SQL_NULL_HANDLE;
  }
} // TODBCApiAgent::closeODBCConnection


// check if aResult signals error and throw exception if so
void TODBCApiAgent::checkODBCError(SQLRETURN aResult,SQLSMALLINT aHandleType,SQLHANDLE aHandle)
{
  string msg,state;

  if (getODBCError(aResult,msg,state,aHandleType,aHandle)) {
    // error
    throw TSyncException(msg.c_str());
  }
} // TODBCApiAgent::checkODBCError


// - check if aResult signals error and throw exception if so
void TODBCApiAgent::checkStatementError(SQLRETURN aResult,SQLHSTMT aHandle)
{
  checkODBCError(aResult,SQL_HANDLE_STMT,aHandle);
} // TODBCApiAgent::checkStatementError


// - check if aResult signals error and throw exception if so
//   does not report NO_DATA error, but returns false if NO_DATA condition exists
bool TODBCApiAgent::checkStatementHasData(SQLRETURN aResult,SQLHSTMT aHandle)
{
  if (aResult==SQL_NO_DATA)
    return false; // signal NO DATA
  else
    checkStatementError(aResult,aHandle);
  return true; // signal ok
} // TODBCApiAgent::checkStatementHasData


// get ODBC error message for given result code
// - returns false if no error
bool TODBCApiAgent::getODBCError(SQLRETURN aResult,string &aMessage,string &aSQLState, SQLSMALLINT aHandleType,SQLHANDLE aHandle)
{
  aSQLState.erase();
  if(aResult==SQL_SUCCESS)
    return false; // no error
  else {
    StringObjPrintf(aMessage,"ODBC SQL return code = %ld\n",(sInt32)aResult);
    // get Diag Info
    SQLCHAR sqlstate[6];   // buffer for state message
    sqlstate[5]=0; // terminate

    SQLINTEGER nativeerror;
    SQLSMALLINT msgsize,recno;
    SQLRETURN res;
    recno=1;
    // message buffer
    sInt16 maxmsgsize = 200;
    SQLCHAR *messageP = (SQLCHAR *) malloc(maxmsgsize);
    do {
      if (messageP==NULL) {
        StringObjAppendPrintf(aMessage,"- SQLGetDiagRec[%hd] failed because needed buffer of %ld bytes cannot be allocated",(sInt16)recno,(sInt32)maxmsgsize);
        break; // don't continue
      }
      msgsize=0; // just to make sure
      res=SQLGetDiagRec(
        aHandleType,  // handle Type
        aHandle,          // statement handle
        recno,            // record number
        sqlstate,         // state buffer
        &nativeerror,     // native error code
        messageP,         // message buffer, gets message
        maxmsgsize,       // message buffer size
        &msgsize          // gets size of message buffer
      );
      if (res==SQL_NO_DATA) break; // seen all diagnostic info
      if (res==SQL_SUCCESS_WITH_INFO) {
        if (msgsize>maxmsgsize) {
          // buffer is too small, allocate a bigger one and try again
          free(messageP);
          maxmsgsize=msgsize+1; // make buffer large enough for message
          messageP = (SQLCHAR *) malloc(maxmsgsize);
          // try again
          continue;
        }
        // SQL_SUCCESS_WITH_INFO, but buffer is large enough - strange...
        StringObjAppendPrintf(aMessage,"- SQLGetDiagRec[%hd] said SQL_SUCCESS_WITH_INFO, but buffer is large enough\n",(sInt16)recno);
      }
      // info found, append it to error text message
      else if (res==SQL_SUCCESS) {
        StringObjAppendPrintf(aMessage,"- SQLState = '%s', NativeError=%ld, Message = %s\n",sqlstate,(sInt32)nativeerror,messageP);
        // save latest SQLState
        aSQLState.assign((const char *)sqlstate,5);
      }
      else {
        StringObjAppendPrintf(aMessage,"- SQLGetDiagRec[%hd] failed, Result=%ld\n",(sInt16)recno,(sInt32)res);
        break; // abort on error, too
      }
      recno++;
    } while(true);
    // get rid of message buffer
    free(messageP);
    // if it's success with Info, this is not considered an error, but do show info in log
    if (aResult==SQL_SUCCESS_WITH_INFO) {
      // show info as exotics in debug logs, but treat as success
      // (note: MS-SQL is verbose here...)
      PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("SQL_SUCCESS_WITH_INFO: %s",aMessage.c_str()));
      return false; // this is no error
    }
    else {
      return true; // treat as error error
    }
  }
} // TODBCApiAgent::getODBCError



// get new statement handle
SQLHSTMT TODBCApiAgent::newStatementHandle(SQLHDBC aConnection)
{
  SQLRETURN res;
  SQLHSTMT statement;

  // Allocate Statement Handle
  res=SafeSQLAllocHandle(SQL_HANDLE_STMT,aConnection,&statement);
  checkODBCError(res,SQL_HANDLE_DBC,aConnection);
  // Statement Attributes
  // - row array size (ODBC 3.0, not really needed as default is 1 anyway)
  res=SQLSetStmtAttr(
    statement,
    SQL_ATTR_ROW_ARRAY_SIZE,
    (void*)1,
    SQL_IS_UINTEGER
  );
  checkStatementError(res,statement);
  return statement;
} // TODBCApiAgent::newStatementHandle





/* About SQLGetData from WIN32_SDK:

If the driver does not support extensions to SQLGetData, the function can only
return data for unbound columns with a number greater than that of the last bound
column. Furthermore, within a row of data, the value of the ColumnNumber argument
in each call to SQLGetData must be greater than or equal to the value of
ColumnNumber in the previous call; that is, data must be retrieved in increasing
column number order. Finally, if no extensions are supported, SQLGetData cannot
be called if the rowset size is greater than 1.

*/

// get value, returns false if no Data or Null
bool TODBCApiAgent::getColumnValueAsULong(
  SQLHSTMT aStatement,
  sInt16 aColNumber,
  uInt32 &aLongValue
)
{
  SQLRETURN res;
  SQLINTEGER ind;
  SQLSMALLINT numcols;

  // check if there aColNumber is in range
  res = SafeSQLNumResultCols(aStatement,&numcols);
  checkStatementError(res,aStatement);
  if (aColNumber<1 || aColNumber>numcols)
    throw TSyncException(DEBUGTEXT("getColumnValueAsULong with bad col index","odds1"));
  aLongValue=0;
  res = SafeSQLGetData(
    aStatement,       // statement handle
    aColNumber,       // column number
    SQL_C_ULONG,      // target type: unsigned long
    &aLongValue,      // where to store the long
    4,                // max size of value (not used here)
    (SQLLEN*)&ind     // indicator
  );
  checkStatementError(res,aStatement);
  // return true if real data returned
  return (ind!=SQL_NULL_DATA && ind!=SQL_NO_TOTAL);
} // TODBCApiAgent::getColumnValueAsULong


// get value, returns false if no Data or Null
bool TODBCApiAgent::getColumnValueAsLong(
  SQLHSTMT aStatement,
  sInt16 aColNumber,
  sInt32 &aLongValue
)
{
  SQLRETURN res;
  SQLINTEGER ind;
  SQLSMALLINT numcols;

  // check if there aColNumber is in range
  res = SafeSQLNumResultCols(aStatement,&numcols);
  checkStatementError(res,aStatement);
  if (aColNumber<1 || aColNumber>numcols)
    throw TSyncException(DEBUGTEXT("getColumnValueAsLong with bad col index","odds2"));
  aLongValue=0;
  res = SafeSQLGetData(
    aStatement,       // statement handle
    aColNumber,       // column number
    SQL_C_LONG,       // target type: signed long
    &aLongValue,      // where to store the long
    4,                // max size of value (not used here)
    (SQLLEN*)&ind     // indicator
  );
  checkStatementError(res,aStatement);
  // return true if real data returned
  return (ind!=SQL_NULL_DATA && ind!=SQL_NO_TOTAL);
} // TODBCApiAgent::getColumnValueAsLong


// get value, returns false if no Data or Null
bool TODBCApiAgent::getColumnValueAsDouble(
  SQLHSTMT aStatement,
  sInt16 aColNumber,
  double &aDoubleValue
)
{
  SQLRETURN res;
  SQLINTEGER ind;
  SQLSMALLINT numcols;

  // check if there aColNumber is in range
  res = SafeSQLNumResultCols(aStatement,&numcols);
  checkStatementError(res,aStatement);
  if (aColNumber<1 || aColNumber>numcols)
    throw TSyncException(DEBUGTEXT("getColumnValueAsDouble with bad col index","odds3"));
  aDoubleValue=0;
  res = SafeSQLGetData(
    aStatement,       // statement handle
    aColNumber,       // column number
    SQL_C_DOUBLE,     // target type: signed long
    &aDoubleValue,    // where to store the double
    8,                // max size of value (not used here)
    (SQLLEN*)&ind     // indicator
  );
  checkStatementError(res,aStatement);
  // return true if real data returned
  return (ind!=SQL_NULL_DATA && ind!=SQL_NO_TOTAL);
} // TODBCApiAgent::getColumnValueAsDouble


// get value, returns false if no Data or Null
bool TODBCApiAgent::getColumnValueAsString(
  SQLHSTMT aStatement,
  sInt16 aColNumber,
  string &aStringValue,
  TCharSets aCharSet, // real charset, including UTF16!
  bool aAsBlob
)
{
  SQLRETURN res;
  sInt32 maxstringlen=512; // enough to start with for most fields
  sInt32 nextbuflen;
  uInt8 *strbufP=NULL;
  SQLINTEGER siz;
  SQLSMALLINT numcols;
  bool gotData=false;
  #ifdef ODBC_UNICODE
  string wStr;
  #endif

  // check if there aColNumber is in range
  res = SafeSQLNumResultCols(aStatement,&numcols);
  checkStatementError(res,aStatement);
  if (aColNumber<1 || aColNumber>numcols)
    throw TSyncException(DEBUGTEXT("getColumnValueAsString with bad col index","odds4"));
  // get data
  // - start with empty string
  aStringValue.erase();
  strbufP = new uInt8[maxstringlen+1];
  try {
    bool gotAllData=false;
    do {
      // make sure we have the buffer
      if (!strbufP)
        throw TSyncException(DEBUGTEXT("getColumnValueAsString can't allocate enough buffer memory","odds4a"));
      strbufP[maxstringlen]=0; // make sure we have ALWAYS a terminator (for appendStringAsUTF8)
      nextbuflen=maxstringlen; // default to same size we already have
      // now get data
      res = SafeSQLGetData(
        aStatement,       // statement handle
        aColNumber,       // column number
        aAsBlob ? SQL_C_BINARY :
          #ifdef ODBC_UNICODE
          (aCharSet==chs_utf16 ? SQL_C_WCHAR : SQL_C_CHAR), // target type: Binary, 8-bit or 16-bit string
          #else
          SQL_C_CHAR, // no 16-bit chars
          #endif
        strbufP,          // where to store the data
        maxstringlen,     // max size of string
        (SQLLEN*)&siz     // returns real remaining size of data (=what we got in this call + what still remains to be fetched)
      );
      if (res==SQL_NO_DATA) {
        // no (more) data
        gotAllData=true;
        break;
      }
      if (res!=SQL_SUCCESS && res!=SQL_SUCCESS_WITH_INFO) {
        checkStatementError(res,aStatement);
        break;
      }
      // determine size
      if (siz==SQL_NULL_DATA) {
        // NULL data
        gotAllData=true;
        break;
      }
      else if (siz==SQL_NO_TOTAL) {
        // we do not know how much is remaining, so it's certainly at least a full buffer
        PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("SQLGetData returned SQL_NO_TOTAL and %ld bytes of data",maxstringlen));
        siz=maxstringlen;
        nextbuflen=maxstringlen*2; // suggest next buffer twice as big as current one
      }
      else {
        // what we get is either the rest or a full buffer
        if (siz>maxstringlen) {
          PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("SQLGetData returned %ld bytes of %ld total remaining",maxstringlen,siz));
          // that's how much we need for the remaining data. Plus one to avoid extra loop
          // at end for drivers that do not return state '01004'
          nextbuflen=siz-maxstringlen+1;
          // that's how much we got this time: one buffer full
          siz=maxstringlen;
        }
      }
      gotData=true; // not NULL
      // now copy data
      if (res==SQL_SUCCESS_WITH_INFO) {
        // probably data truncated
        string msg,sqlstate;
        getODBCError(res,msg,sqlstate,SQL_HANDLE_STMT,aStatement);
        if (sqlstate=="01004") {
          // data truncated.
          PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("SQLGetData returns state '01004' (truncated) -> more data to be fetched"));
          // - do not yet exit loop
          gotAllData=false; // not all...
          gotData=true; // ...but some
        }
        else {
          // otherwise treat as success
          PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("SQLGetData returns state '%s' -> ignore",sqlstate.c_str()));
          res=SQL_SUCCESS;
        }
      }
      if (res==SQL_SUCCESS) {
        // it seems that not all drivers return SQL_SUCCESS_WITH_INFO when there is more data to read
        // so only stop reading here already if we haven't got one buffer full. Otherwise,
        // loop one more time to try getting more. We'll get SQL_NULL_DATA or SQL_NO_DATA then and exit the loop.
        if (siz<maxstringlen) {
          // received all we can receive
          gotAllData=true;
        }
        else {
          // received exactly one buffer full - maybe there is more (MyODBC on Linux does not issue SQL_SUCCESS_WITH_INFO "01004")
          PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("SQLGetData returns SQL_SUCCESS but buffer full of data (%ld bytes) -> try to get more",(uInt32)siz));
        }
      }
      // copy what we already have
      if (aAsBlob)
        aStringValue.append((const char *)strbufP,siz); // assign all data 1:1 to string
      #ifdef ODBC_UNICODE
      else if (aCharSet==chs_utf16)
        wStr.append((const char *)strbufP,siz); // assign all data 1:1 to wide string buffer, will be converted later
      #endif
      else
        appendStringAsUTF8((const char *)strbufP, aStringValue, aCharSet, lem_cstr); // Convert to app-charset (UTF8) and C-type lineends
      // get a bigger buffer in case there's more to fetch and we haven't got 64k already
      if (!gotAllData) {
        if (maxstringlen<65536 && nextbuflen>maxstringlen) {
          // we could need a larger buffer
          maxstringlen = nextbuflen>65536 ? 65536 : nextbuflen;
          delete strbufP;
          strbufP = new uInt8[maxstringlen+1];
          PDEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,("Allocating bigger buffer for next call to SQLGetData: %ld bytes",(uInt32)maxstringlen));
        }
      }
    } while(!gotAllData);
    // done, we don't need the buffer any more
    if (strbufP) delete strbufP;
  }
  catch (...) {
    // clean up buffer
    if (strbufP) delete strbufP;
    throw;
  }
  // convert from Unicode to UTF-8 if we got unicode here
  #ifdef ODBC_UNICODE
  if (aCharSet==chs_utf16) {
    appendUTF16AsUTF8((const uInt16 *)wStr.c_str(), wStr.size()/2, ODBC_BIGENDIAN, aStringValue, true, false);
  }
  #endif
  // done
  return gotData;
} // TODBCApiAgent::getColumnValueAsString


// returns true if successfully and filled aODBCTimestamp
bool TODBCApiAgent::getColumnAsODBCTimestamp(
  SQLHSTMT aStatement,
  sInt16 aColNumber,
  SQL_TIMESTAMP_STRUCT &aODBCTimestamp
)
{
  SQLRETURN res;
  SQLINTEGER ind;
  SQLSMALLINT numcols;

  // check if there aColNumber is in range
  res = SafeSQLNumResultCols(aStatement,&numcols);
  checkStatementError(res,aStatement);
  if (aColNumber<1 || aColNumber>numcols)
    throw TSyncException(DEBUGTEXT("getColumnAsODBCTimestamp with bad col index","odds5"));
  // get data
  res = SafeSQLGetData(
    aStatement,       // statement handle
    aColNumber,       // column number
    SQL_C_TYPE_TIMESTAMP,       // target type: timestamp
    &aODBCTimestamp,          // where to store the timestamp
    0,                // n/a
    (SQLLEN*)&ind     // returns indication if NULL
  );
  checkStatementError(res,aStatement);
  // return true if data filled in
  return (ind!=SQL_NULL_DATA && ind!=SQL_NO_TOTAL);
} // TODBCApiAgent::getColumnAsODBCTimestamp


// get value (UTC timestamp), returns false if no Data or Null
bool TODBCApiAgent::getColumnValueAsTimestamp(
  SQLHSTMT aStatement,
  sInt16 aColNumber,
  lineartime_t &aTimestamp
)
{
  SQL_TIMESTAMP_STRUCT odbctimestamp;

  if (getColumnAsODBCTimestamp(aStatement,aColNumber,odbctimestamp)) {
    // there is a timestamp
    DEBUGPRINTFX(DBG_DBAPI+DBG_EXOTIC,(
      "ODBCTimestamp: %04hd-%02hd-%02hd %02hd:%02hd:%02hd.%03ld",
      odbctimestamp.year,odbctimestamp.month,odbctimestamp.day,
      odbctimestamp.hour,odbctimestamp.minute,odbctimestamp.second,
      odbctimestamp.fraction / 1000000
    ));
    aTimestamp =
      date2lineartime(odbctimestamp.year,odbctimestamp.month,odbctimestamp.day) +
      time2lineartime(odbctimestamp.hour,odbctimestamp.minute,odbctimestamp.second, odbctimestamp.fraction / 1000000);
    return true;
  }
  // no data
  aTimestamp=0;
  return false;
} // TODBCApiAgent::getColumnValueAsTimestamp


// get value, returns false if no Data or Null
bool TODBCApiAgent::getColumnValueAsTime(
  SQLHSTMT aStatement,
  sInt16 aColNumber,
  lineartime_t &aTime
)
{
  SQL_TIMESTAMP_STRUCT odbctimestamp;

  if (getColumnAsODBCTimestamp(aStatement,aColNumber,odbctimestamp)) {
    // there is a timestamp
    aTime = time2lineartime(
      odbctimestamp.hour,
      odbctimestamp.minute,
      odbctimestamp.second,
      odbctimestamp.fraction / 1000000
    );
    return true;
  }
  // no data
  aTime=0;
  return false;
} // TODBCApiAgent::getColumnValueAsTime


// get value, returns false if no Data or Null
bool TODBCApiAgent::getColumnValueAsDate(
  SQLHSTMT aStatement,
  sInt16 aColNumber,
  lineartime_t &aDate
)
{
  SQL_TIMESTAMP_STRUCT odbctimestamp;

  if (getColumnAsODBCTimestamp(aStatement,aColNumber,odbctimestamp)) {
    // there is a timestamp
    aDate = date2lineartime(
      odbctimestamp.year,
      odbctimestamp.month,
      odbctimestamp.day
    );
    return true;
  }
  // no data
  aDate=0;
  return false;
} // TODBCApiAgent::getColumnValueAsDate



// get column value as database field
bool TODBCApiAgent::getColumnValueAsField(
  SQLHSTMT aStatement, sInt16 aColIndex, TDBFieldType aDbfty, TItemField *aFieldP,
  TCharSets aDataCharSet, timecontext_t aTimecontext, bool aMoveToUserContext
)
{
  string val;
  lineartime_t ts, basedate=0; // no base date yet
  sInt32 dbts;
  timecontext_t tctx = TCTX_UNKNOWN;
  sInt16 moffs=0;
  sInt32 i=0;
  double hrs=0;
  bool notnull=false; // default to empty
  // get pointer if assigning into timestamp field
  TTimestampField *tsfP = NULL;
  if (aFieldP->isBasedOn(fty_timestamp)) {
    tsfP = static_cast<TTimestampField *>(aFieldP);
  }
  // field available in multifielditem
  switch (aDbfty) {
    case dbft_uctoffsfortime_hours:
      notnull=getColumnValueAsDouble(aStatement,aColIndex,hrs);
      if (!notnull) goto assignzone; // assign TCTX_UNKNOWN
      moffs=(sInt16)(hrs*MinsPerHour); // convert to minutes
      goto assignoffs;
    case dbft_uctoffsfortime_mins:
      notnull=getColumnValueAsLong(aStatement,aColIndex,i);
      if (!notnull) goto assignzone; // assign TCTX_UNKNOWN
      moffs=i; // these are minutes
      goto assignoffs;
    case dbft_uctoffsfortime_secs:
      notnull=getColumnValueAsLong(aStatement,aColIndex,i);
      if (!notnull) goto assignzone; // assign TCTX_UNKNOWN
      moffs = i / SecsPerMin;
      goto assignoffs;
    case dbft_zonename:
      // get zone name as string
      notnull=getColumnValueAsString(aStatement,aColIndex,val,aDataCharSet, lem_cstr);
      if (!notnull) goto assignzone; // assign TCTX_UNKNOWN
      // convert to context
      TimeZoneNameToContext(val.c_str(), tctx, getSessionZones());
      goto assignzone;
    assignoffs:
      tctx = TCTX_MINOFFSET(moffs);
    assignzone:
      // zone works only for timestamps
      // - move to new zone or assign zone if timestamp is still empty or floating
      if (tsfP) {
        // first move to original context (to compensate for possible move to
        // fUserTimeContext done when reading timestamp with aMoveToUserContext)
        // Note: this is important for cases where the new zone is floating or dateonly
        // Note: if timestamp field had the "f" flag, it is still floating here, and will not be
        //       moved to non-floating aTimecontext(=DB context) here.
        tsfP->moveToContext(aTimecontext,false);
        // now move to specified zone, or assign zone if timestamp is still floating here
        tsfP->moveToContext(tctx,true);
      }
      break;

    case dbft_lineardate:
    case dbft_unixdate_s:
    case dbft_unixdate_ms:
    case dbft_unixdate_us:
      notnull=getColumnValueAsLong(aStatement,aColIndex,dbts);
      if (notnull)
        ts=dbIntToLineartime(dbts,aDbfty);
      goto dateonly;
    case dbft_dateonly:
    case dbft_date:
      notnull=getColumnValueAsDate(aStatement,aColIndex,ts);
    dateonly:
      tctx = TCTX_UNKNOWN | TCTX_DATEONLY; // dates are always floating
      goto assignval;
    case dbft_timefordate:
      // get base date used to build up datetime
      if (tsfP) {
        basedate = tsfP->getTimestampAs(TCTX_UNKNOWN); // unconverted, as-is
      }
      // otherwise handle like time
    case dbft_time:
      notnull=getColumnValueAsTime(aStatement,aColIndex,ts);
      // combine with date
      ts+=basedate;
      goto assigntimeval;
    case dbft_timestamp:
      notnull=getColumnValueAsTimestamp(aStatement,aColIndex,ts);
      goto assigntimeval;
    case dbft_lineartime:
    case dbft_unixtime_s:
    case dbft_nsdate_s:
    case dbft_unixtime_ms:
    case dbft_unixtime_us:
      notnull=getColumnValueAsLong(aStatement,aColIndex,dbts);
      if (notnull)
        ts=dbIntToLineartime(dbts,aDbfty);
    assigntimeval:
      // time values will be put into aTimecontext (usually <datatimezone>, but can
      // be TCTX_UNKNOWN for field explicitly marked floating in DB with <map mode="f"...>
      tctx = aTimecontext;
    assignval:
      // something convertible to lineartime_t
      if (notnull) {
        // first, if output in user zone is selected, move timestamp to user zone
        // Note: before moving to a field-specific zone, this will be reverted such that
        //       field-specific TZ=DATE conversion will be done in the original (DB) TZ context
        if (aMoveToUserContext) {
          if (TzConvertTimestamp(ts,tctx,fUserTimeContext,getSessionZones()))
            tctx = fUserTimeContext; // moved to user zone
        }
        // now store in item field
        if (tsfP) {
          // assign timestamp and context passed (TCTX_UNKNOWN for floating)
          tsfP->setTimestampAndContext(ts,tctx);
        }
        else {
          // destination is NOT timestamp, assign ISO date/time string,
          // either UTC or qualified with local zone offset
          bool dateonly = aDbfty==dbft_date || aDbfty==dbft_dateonly;
          tctx = dateonly ? TCTX_UNKNOWN | TCTX_DATEONLY : aTimecontext;
          // - aWithTime, NOT aAsUTC, aWithOffset, offset, NOT aShowOffset
          TimestampToISO8601Str(val, ts, tctx, false, false);
          aFieldP->setAsString(val.c_str());
        }
      }
      else
        aFieldP->assignEmpty(); // NULL value: not assigned
      break;
    case dbft_blob:
      // Database field is BLOB, assign it to item as binary string
      if ((notnull=getColumnValueAsString(aStatement,aColIndex,val,chs_unknown,true)))
        aFieldP->setAsString(val.c_str(),val.size());
      else
        aFieldP->assignEmpty(); // NULL value: not assigned
      break;
    case dbft_string:
    case dbft_numeric:
    default:
      // Database field is string (or unknown), assign to item as string
      if ((notnull=getColumnValueAsString(aStatement,aColIndex,val,aDataCharSet)))
        aFieldP->setAsString(val.c_str());
      else
        aFieldP->assignEmpty(); // NULL value: not assigned
      break;
  } // switch
  return notnull;
} // TODBCApiAgent::getColumnValueAsField


// bind parameters (and values for IN-Params) to the statement
void TODBCApiAgent::bindSQLParameters(
  TSyncSession *aSessionP,
  SQLHSTMT aStatement,
  TParameterMapList &aParamMapList, // the list of mapped parameters
  TCharSets aDataCharSet,
  TLineEndModes aDataLineEndMode
)
{
  SQLSMALLINT valueType,paramType;
  SQLUSMALLINT paramNo=1;
  SQLUINTEGER colSiz;
  string s1,s2;
  TParameterMapList::iterator pos;
  bool copyvalue;
  for (pos=aParamMapList.begin();pos!=aParamMapList.end();++pos) {
    // bind parameter to statement
    copyvalue=false;
    colSiz=std_paramsize;
    switch (pos->parammode) {
      case param_localid_str:
      case param_localid_int:
      case param_remoteid_str:
      case param_remoteid_int:
        pos->StrLen_or_Ind=0; // no size indication known yet
        // a item id, always as string
        valueType=SQL_C_CHAR;
        // but column could also be integer
        paramType=pos->parammode==param_localid_int || pos->parammode==param_remoteid_int ?  SQL_INTEGER : SQL_VARCHAR;
        // get input value (if any)
        if (pos->inparam) {
          if (pos->itemP) {
            // item available
            pos->ParameterValuePtr=(void *)(
              pos->parammode==param_localid_str || pos->parammode==param_localid_int ?
              pos->itemP->getLocalID() :
              pos->itemP->getRemoteID()
            );
          }
          else {
            // no item
            pos->ParameterValuePtr=(void *)""; // empty value
          }
          pos->StrLen_or_Ind = strlen((const char *)pos->ParameterValuePtr); // actual length
        }
        // maximum value size
        pos->BufferLength=std_paramsize+1; // %%% fixed value for ids, should be enough for all cases
        colSiz=std_paramsize;
        break;
      case param_field:
        pos->StrLen_or_Ind=0; // no size indication known for field yet (for buffer we use predefined values)
      case param_buffer:
        // determine value type and pointer
        switch (pos->dbFieldType) {
          case dbft_numeric:
            valueType=SQL_C_CHAR;
            paramType=SQL_INTEGER;
            break;
          case dbft_blob:
            valueType=SQL_C_BINARY;
            paramType=SQL_LONGVARBINARY;
            break;
          case dbft_string:
          default:
            #ifdef ODBC_UNICODE
            if (aDataCharSet==chs_utf16) {
              // 16-bit string
              valueType=SQL_C_WCHAR;
              paramType=SQL_WVARCHAR;
            }
            else
            #endif
            {
              // 8-bit string
              valueType=SQL_C_CHAR;
              paramType=SQL_VARCHAR;
            }
            break;
        }
        // that's all for param with already prepared buffer
        if (pos->parammode==param_buffer)
          break;
        // get maximum value size
        colSiz=pos->maxSize;
        if (pos->maxSize==0) {
          // no max size specified
          colSiz=std_paramsize; // default to standard size
        }
        // %%%no longer, we calc it below now%%% pos->BufferLength=colSiz+1;
        if (pos->inparam) {
          if (pos->fieldP) {
            // get actual value to pass as param into string s2
            switch (pos->dbFieldType) {
              default:
              case dbft_string:
              case dbft_numeric:
                // get as app string
                pos->fieldP->getAsString(s1);
                // convert to database string
                s2.erase();
                #ifdef ODBC_UNICODE
                if (aDataCharSet==chs_utf16) {
                  // 16-bit string
                  appendUTF8ToUTF16ByteString(
                    s1.c_str(),
                    s2,
                    ODBC_BIGENDIAN,
                    aDataLineEndMode,
                    pos->maxSize // max size (0 = unlimited)
                  );
                }
                else
                #endif // ODBC_UNICODE
                {
                  // 8-bit string
                  appendUTF8ToString(
                    s1.c_str(),
                    s2,
                    aDataCharSet,
                    aDataLineEndMode,
                    qm_none, // no quoting needed
                    pos->maxSize // max size (0 = unlimited)
                  );
                }
                break;
              case dbft_blob:
                #ifndef _MSC_VER // does not understand warnings
                #warning "maybe we should store blobs using SQLPutData and SQL_DATA_AT_EXEC mode"
                #endif

                // get as unconverted binary chunk
                if (pos->fieldP->isBasedOn(fty_blob))
                  static_cast<TBlobField *>(pos->fieldP)->getBlobAsString(s2);
                else
                  pos->fieldP->getAsString(s2);
                break;
            } // switch
            // s2 now contains the value
            pos->ParameterValuePtr=(void *)s2.c_str(); // value
            // default to full length of input string
            pos->StrLen_or_Ind=s2.size(); // size
            pos->BufferLength=s2.size(); // default to as long as string is
            // limit to max size
            if (pos->maxSize!=0 && pos->maxSize<s2.size()) {
              pos->BufferLength=pos->maxSize;
              pos->StrLen_or_Ind=pos->maxSize;
            }
            // expand buffer to colsiz if input is larger than default column size
            if (colSiz<pos->BufferLength)
              colSiz=pos->BufferLength;
            // buffer for output must be at least colsiz (which is maxsize, or std_paramsize if no maxsize specified)
            if (pos->outparam) {
              // input/output param, make buffer minimally as large as indicated by colsiz
              if (pos->BufferLength<colSiz)
                pos->BufferLength=colSiz;
            }
            // plus one for terminator
            pos->BufferLength+=1;
            copyvalue=true; // we need to copy it
          } // if field
        } // if inparam
        break;
    } // switch parammode
    // if this is an out param, create a buffer of BufferLength
    if (pos->parammode!=param_buffer) {
      if (pos->outparam || copyvalue) {
        // create buffer
        void *bP = sysync_malloc(pos->BufferLength);
        pos->mybuffer=true; // this is now a buffer allocated by myself
        if (pos->inparam) {
          // init buffer with input value
          memcpy(bP,pos->ParameterValuePtr,pos->StrLen_or_Ind+1);
        }
        // pass buffer, not original value
        pos->ParameterValuePtr = bP;
        // make sure buffer contains a NUL terminator
        *((uInt8 *)bP+pos->StrLen_or_Ind)=0;
      }
      if (pos->outparam && !pos->inparam)
        pos->StrLen_or_Ind=SQL_NULL_DATA; // out only has no indicator for input
    }
    // now actually bind to parameter
    POBJDEBUGPRINTFX(aSessionP,DBG_DBAPI+DBG_EXOTIC,(
      "SQLBind: sizeof(SQLLEN)=%d, sizeof(SQLINTEGER)=%d, paramNo=%hd, in=%d, out=%d, parammode=%hd, valuetype=%hd, paramtype=%hd, lenorind=%d, valptr=%X, bufsiz=%d, maxcol=%d, colsiz=%d",
      sizeof(SQLLEN), sizeof(SQLINTEGER), // to debug, as there can be problematic 32bit/64bit mismatches between these
      (uInt16)paramNo,
      (int)pos->inparam,
      (int)pos->outparam,
      (uInt16)(pos->outparam ? (pos->inparam ? SQL_PARAM_INPUT_OUTPUT : SQL_PARAM_OUTPUT ) : SQL_PARAM_INPUT), // type of param
      (uInt16)valueType,
      (uInt16)paramType,
      (int)pos->StrLen_or_Ind,
      pos->ParameterValuePtr,
      (int)pos->BufferLength,
      (int)pos->maxSize,
      (int)colSiz
    ));
    /*
    SQLRETURN SQL_API SQLBindParameter(
        SQLHSTMT           hstmt,
        SQLUSMALLINT       ipar,
        SQLSMALLINT        fParamType,
        SQLSMALLINT        fCType,
        SQLSMALLINT        fSqlType,
        SQLULEN            cbColDef,
        SQLSMALLINT        ibScale,
        SQLPOINTER         rgbValue,
        SQLLEN             cbValueMax,
        SQLLEN             *pcbValue);
    */
    SQLRETURN res=SQLBindParameter(
      aStatement,
      paramNo, // parameter number
      pos->outparam ? (pos->inparam ? SQL_PARAM_INPUT_OUTPUT : SQL_PARAM_OUTPUT ) : SQL_PARAM_INPUT, // type of param
      valueType, // value type: how we want it represented
      paramType, // parameter type (integer or char)
      (colSiz==0 ? 1 : colSiz), // column size, relevant for char parameter only (must never be 0, even for zero length input-only params)
      0, // decimal digits
      pos->ParameterValuePtr, // parameter value
      pos->BufferLength, // value buffer size (for output params)
      &(pos->StrLen_or_Ind) // length or indicator
    );
    checkStatementError(res,aStatement);
    // next param
    paramNo++;
  } // for all parametermaps
} // TODBCApiAgent::bindSQLParameters


// save out parameter values and clean up
void TODBCApiAgent::saveAndCleanupSQLParameters(
  TSyncSession *aSessionP,
  SQLHSTMT aStatement,
  TParameterMapList &aParamMapList, // the list of mapped parameters
  TCharSets aDataCharSet,
  TLineEndModes aDataLineEndMode
)
{
  SQLUSMALLINT paramNo=1;
  string s,s2;
  TParameterMapList::iterator pos;
  for (pos=aParamMapList.begin();pos!=aParamMapList.end();++pos) {
    // save output parameter value
    if (pos->outparam) {
      switch (pos->parammode) {
        case param_localid_str:
        case param_localid_int:
        case param_remoteid_str:
        case param_remoteid_int:
          // id output params are always strings
          POBJDEBUGPRINTFX(aSessionP,DBG_DBAPI+DBG_EXOTIC,(
            "Postprocessing Item ID param: in=%d, out=%d, lenorind=%ld, valptr=%lX, bufsiz=%ld",
            (int)pos->inparam,
            (int)pos->outparam,
            pos->StrLen_or_Ind,
            pos->ParameterValuePtr,
            pos->BufferLength
          ));
          /* Note: len or ind does not contain a useful value
          DEBUGPRINTFX(DBG_DBAPI,("%%%% saving remote or localid out param: len=%ld, '%s'",pos->StrLen_or_Ind, s.c_str()));
          if (pos->StrLen_or_Ind==SQL_NULL_DATA || pos->StrLen_or_Ind==SQL_NO_TOTAL)
            s.erase();
          else
            s.assign((const char *)pos->ParameterValuePtr,pos->StrLen_or_Ind);
          */
          // SQL_C_CHAR is null terminated, so just assign
          s=(const char *)pos->ParameterValuePtr;
          // assign to item
          if (pos->itemP) {
            if (pos->parammode==param_localid_str || pos->parammode==param_localid_int)
              pos->itemP->setLocalID(s.c_str());
            else
              pos->itemP->setRemoteID(s.c_str());
          }
          break;
        case param_buffer:
          // buffer will be processed externally
          if (pos->outSiz) {
            if (pos->StrLen_or_Ind==SQL_NULL_DATA || pos->StrLen_or_Ind==SQL_NO_TOTAL)
              *(pos->outSiz) = 0; // no output
            else
              *(pos->outSiz) = pos->StrLen_or_Ind; // indicate how much is in buffer
          }
        case param_field:
          // get value field
          if (pos->fieldP) {
            POBJDEBUGPRINTFX(aSessionP,DBG_DBAPI+DBG_EXOTIC,(
              "Postprocessing field param: in=%d, out=%d, lenorind=%ld, valptr=%lX, bufsiz=%ld",
              (int)pos->inparam,
              (int)pos->outparam,
              pos->StrLen_or_Ind,
              pos->ParameterValuePtr,
              pos->BufferLength
            ));
            // check for NULL
            if (pos->StrLen_or_Ind==SQL_NULL_DATA || pos->StrLen_or_Ind==SQL_NO_TOTAL)
              pos->fieldP->assignEmpty(); // NULL is empty
            else {
              // save value according to type
              switch (pos->dbFieldType) {
                default:
                case dbft_string:
                case dbft_numeric:
                  // get as db string
                  /* Note: len or ind does not contain a useful value
                  DEBUGPRINTFX(DBG_DBAPI,("%%%% saving remote or localid out param: len=%ld, '%s'",pos->StrLen_or_Ind, s.c_str()));
                  if (pos->StrLen_or_Ind==SQL_NULL_DATA || pos->StrLen_or_Ind==SQL_NO_TOTAL)
                    s.erase();
                  else
                    s.assign((const char *)pos->ParameterValuePtr,pos->StrLen_or_Ind);
                  */
                  #ifdef ODBC_UNICODE
                  if (aDataCharSet==chs_utf16) {
                    // SQL_C_WCHAR is null terminated, so just assign
                    appendUTF16AsUTF8(
                      (const uInt16 *)pos->ParameterValuePtr,
                      pos->StrLen_or_Ind, // num of 16-bit chars
                      ODBC_BIGENDIAN,
                      s2,
                      true, // convert line ends
                      false // no filemaker CRs
                    );
                  }
                  else
                  #endif
                  {
                    // SQL_C_CHAR is null terminated, so just assign
                    s=(const char *)pos->ParameterValuePtr;
                    // convert to app string
                    s2.erase();
                    appendStringAsUTF8(
                      (const char *)s.c_str(),
                      s2,
                      aDataCharSet,
                      aDataLineEndMode
                    );
                  }
                  pos->fieldP->setAsString(s2);
                  break;
                case dbft_blob:
                  // save as it comes from DB
                  pos->fieldP->setAsString((const char *)pos->ParameterValuePtr,pos->StrLen_or_Ind);
                  break;
              } // switch
            } // if not NULL
          } // if field
          break;
      } // switch parammode
    } // if outparam
    // if there is a buffer, free it
    if (pos->mybuffer && pos->ParameterValuePtr) {
      // delete buffer if we have allocated one
      sysync_free(pos->ParameterValuePtr);
      pos->ParameterValuePtr=NULL;
      pos->mybuffer=false;
    }
    // next param
    paramNo++;
  } // for all parametermaps
} // TODBCApiAgent::saveAndCleanupSQLParameters

// Parameter handling for session level statements

// bind parameters (and values for IN-Params) to the statement
void TODBCApiAgent::bindSQLParameters(
  SQLHSTMT aStatement
)
{
  bindSQLParameters(
    this,
    aStatement,
    fParameterMaps,
    fConfigP->fDataCharSet,
    fConfigP->fDataLineEndMode
  );
} // TODBCApiAgent::bindSQLParameters


// save out parameter values and clean up
void TODBCApiAgent::saveAndCleanupSQLParameters(
  SQLHSTMT aStatement
)
{
  saveAndCleanupSQLParameters(
    this,
    aStatement,
    fParameterMaps,
    fConfigP->fDataCharSet,
    fConfigP->fDataLineEndMode
  );
} // TODBCApiAgent::bindSQLParameters


#endif // ODBCAPI_SUPPORT


// SQLite utils
// ============

#ifdef SQLITE_SUPPORT

// get SQLite error message for given result code
// - returns false if no error
bool TODBCApiAgent::getSQLiteError(int aRc,string &aMessage, sqlite3 *aDb)
{
  if (aRc == SQLITE_OK || aRc == SQLITE_ROW) return false; // no error
  StringObjPrintf(aMessage,"SQLite Error %d : %s",aRc,aDb==NULL ? "<cannot look up text>" : sqlite3_errmsg(aDb));
  return true;
} // getSQLiteError


// check if aResult signals error and throw exception if so
void TODBCApiAgent::checkSQLiteError(int aRc, sqlite3 *aDb)
{
  string msg;

  if (getSQLiteError(aRc,msg,aDb)) {
    // error
    throw TSyncException(msg.c_str());
  }
} // TODBCApiAgent::checkSQLiteError


// - check if aRc signals error and throw exception if so
//   returns true if data available, false if not
bool TODBCApiAgent::checkSQLiteHasData(int aRc, sqlite3 *aDb)
{
  if (aRc==SQLITE_DONE)
    return false; // signal NO DATA
  else if (aRc==SQLITE_ROW)
    return true; // signal data
  else
    checkSQLiteError(aRc,aDb);
  return true; // signal ok
} // TODBCApiAgent::checkStatementHasData


// get column value from SQLite result
bool TODBCApiAgent::getSQLiteColValueAsField(
  sqlite3_stmt *aStatement, sInt16 aColIndex, TDBFieldType aDbfty, TItemField *aFieldP,
  TCharSets aDataCharSet, timecontext_t aTimecontext, bool aMoveToUserContext
)
{
  string val;
  lineartime_t ts;
  timecontext_t tctx = TCTX_UNKNOWN;
  sInt16 moffs=0;
  size_t siz;
  double hrs=0;
  // get pointer if assigning into timestamp field
  TTimestampField *tsfP = NULL;
  if (aFieldP->isBasedOn(fty_timestamp)) {
    tsfP = static_cast<TTimestampField *>(aFieldP);
  }
  // determine if NULL
  bool notnull=sqlite3_column_type(aStatement,aColIndex)!=SQLITE_NULL; // if column is not null
  // field available in multifielditem
  switch (aDbfty) {
    case dbft_uctoffsfortime_hours:
      if (!notnull) goto assignzone; // assign TCTX_UNKNOWN
      hrs = sqlite3_column_double(aStatement,aColIndex);
      moffs=(sInt16)(hrs*MinsPerHour); // convert to minutes
      goto assignoffs;
    case dbft_uctoffsfortime_mins:
      if (!notnull) goto assignzone; // assign TCTX_UNKNOWN
      moffs = sqlite3_column_int(aStatement,aColIndex);
      goto assignoffs;
    case dbft_uctoffsfortime_secs:
      if (!notnull) goto assignzone; // assign TCTX_UNKNOWN
      moffs = sqlite3_column_int(aStatement,aColIndex);
      moffs /= SecsPerMin;
      goto assignoffs;
    case dbft_zonename:
      if (!notnull) goto assignzone; // assign TCTX_UNKNOWN
      // get zone name as string
      appendStringAsUTF8((const char *)sqlite3_column_text(aStatement,aColIndex), val, aDataCharSet, lem_cstr);
      // convert to context
      TimeZoneNameToContext(val.c_str(), tctx, getSessionZones());
      goto assignzone;
    assignoffs:
      tctx = TCTX_MINOFFSET(moffs);
    assignzone:
      // zone works only for timestamps
      // - move to new zone or assign zone if timestamp is still empty or floating
      if (tsfP) {
        // first move to original context (to compensate for possible move to
        // fUserTimeContext done when reading timestamp with aMoveToUserContext)
        // Note: this is important for cases where the new zone is floating or dateonly
        // Note: if timestamp field had the "f" flag, it is still floating here, and will not be
        //       moved to non-floating aTimecontext(=DB context) here.
        tsfP->moveToContext(aTimecontext,false);
        // now move to specified zone, or assign zone if timestamp is still floating here
        tsfP->moveToContext(tctx,true);
      }
      break;

    case dbft_lineardate:
    case dbft_unixdate_s:
    case dbft_unixdate_ms:
    case dbft_unixdate_us:
      if (notnull)
        ts = dbIntToLineartime(sqlite3_column_int64(aStatement,aColIndex),aDbfty);
      tctx = TCTX_UNKNOWN | TCTX_DATEONLY; // dates are always floating
      goto assignval;
    case dbft_lineartime:
    case dbft_unixtime_s:
    case dbft_nsdate_s:
    case dbft_unixtime_ms:
    case dbft_unixtime_us:
      if (notnull)
        ts = dbIntToLineartime(sqlite3_column_int64(aStatement,aColIndex),aDbfty);
      // time values will be put into aTimecontext (usually <datatimezone>, but can
      // be TCTX_UNKNOWN for field explicitly marked floating in DB with <map mode="f"...>
      tctx = aTimecontext;
    assignval:
      // something convertible to lineartime_t
      if (notnull) {
        // first, if output in user zone is selected, move timestamp to user zone
        // Note: before moving to a field-specific zone, this will be reverted such that
        //       field-specific TZ=DATE conversion will be done in the original (DB) TZ context
        if (aMoveToUserContext) {
          if (TzConvertTimestamp(ts,tctx,fUserTimeContext,getSessionZones()))
            tctx = fUserTimeContext; // moved to user zone
        }
        // now store in item field
        if (tsfP) {
          // assign timestamp and context (TCTX_UNKNOWN for floating)
          tsfP->setTimestampAndContext(ts,tctx);
        }
        else {
          // destination is NOT timestamp, assign ISO date/time string,
          // either UTC or qualified with local zone offset
          bool dateonly = aDbfty==dbft_date || aDbfty==dbft_dateonly;
          tctx = dateonly ? TCTX_UNKNOWN | TCTX_DATEONLY : aTimecontext;
          // - aWithTime, NOT aAsUTC, aWithOffset, offset, NOT aShowOffset
          TimestampToISO8601Str(val, ts, tctx, false, false);
          aFieldP->setAsString(val.c_str());
        }
      }
      else
        aFieldP->assignEmpty(); // NULL value: not assigned
      break;
    case dbft_blob:
      // Database field is BLOB, assign it to item as binary string
      siz=0;
      if (notnull) {
        siz = sqlite3_column_bytes(aStatement,aColIndex);
      }
      if (siz>0)
        aFieldP->setAsString((cAppCharP)sqlite3_column_blob(aStatement,aColIndex),siz);
      else
        aFieldP->assignEmpty(); // NULL or empty value: not assigned
      break;
    case dbft_string:
    case dbft_numeric:
    default:
      // Database field is string (or unknown), assign to item as string
      if (notnull) {
        appendStringAsUTF8((const char *)sqlite3_column_text(aStatement,aColIndex), val, aDataCharSet, lem_cstr); // Convert to app-charset (UTF8) and C-type lineends
        aFieldP->setAsString(val.c_str());
      }
      else
        aFieldP->assignEmpty(); // NULL value: not assigned
      break;
  } // switch
  return notnull;
} // TODBCApiAgent::getSQLiteColValueAsField


// - prepare SQLite statement
void TODBCApiAgent::prepareSQLiteStatement(
  cAppCharP aSQL,
  sqlite3 *aDB,
  sqlite3_stmt *&aStatement
)
{
  const char *sqltail;

  // discard possibly existing one
  if (aStatement) {
    sqlite3_finalize(aStatement);
    aStatement=NULL;
  }
  // make new one
  #if (SQLITE_VERSION_NUMBER>=3003009)
  // use new recommended v2 call with SQLite >=3.3.9
  int rc = sqlite3_prepare_v2(
    aDB,  /* Database handle */
    aSQL, /* SQL statement, UTF-8 encoded */
    -1,   /* null terminated string */
    &aStatement, /* OUT: Statement handle */
    &sqltail     /* OUT: Pointer to unused portion of zSql */
  );
  #else
  // use now deprecated call for older SQLite3 version compatibility
  int rc = sqlite3_prepare(
    aDB,  /* Database handle */
    aSQL, /* SQL statement, UTF-8 encoded */
    -1,   /* null terminated string */
    &aStatement, /* OUT: Statement handle */
    &sqltail     /* OUT: Pointer to unused portion of zSql */
  );
  #endif
  checkSQLiteError(rc,aDB);
} // TODBCApiAgent::prepareSQLiteStatement


// bind parameter values to the statement (only IN-params for strings and BLOBs are supported at all)
void TODBCApiAgent::bindSQLiteParameters(
  TSyncSession *aSessionP,
  sqlite3_stmt *aStatement,
  TParameterMapList &aParamMapList, // the list of mapped parameters
  TCharSets aDataCharSet,
  TLineEndModes aDataLineEndMode
)
{
  string s1,s2;
  TParameterMapList::iterator pos;
  int paramno=1;
  int rc;
  for (pos=aParamMapList.begin();pos!=aParamMapList.end();++pos) {
    // bind parameter to statement
    rc=SQLITE_OK;
    if (pos->inparam) {
      // SQLite only supports input params
      switch (pos->parammode) {
        case param_field:
          if (pos->fieldP) {
            // get actual value to pass as param into string s2
            switch (pos->dbFieldType) {
              default:
              case dbft_string:
              case dbft_numeric:
                // get as app string
                pos->fieldP->getAsString(s1);
                // convert to database string
                s2.erase();
                appendUTF8ToString(
                  s1.c_str(),
                  s2,
                  aDataCharSet,
                  aDataLineEndMode,
                  qm_none, // no quoting needed
                  pos->maxSize // max size (0 = unlimited)
                );
                // s2 is on the stack and will be deallocated on routine exit
                rc=sqlite3_bind_text(aStatement,paramno++,s2.c_str(),s2.size(),SQLITE_TRANSIENT);
                break;
              case dbft_blob:
                // get as unconverted binary chunk
                if (pos->fieldP->isBasedOn(fty_blob)) {
                  void *ptr;
                  size_t sz;
                  static_cast<TBlobField *>(pos->fieldP)->getBlobDataPtrSz(ptr,sz);
                  // BLOB buffer remains static as until statement is finalized
                  rc=sqlite3_bind_blob(aStatement,paramno++,ptr,sz,SQLITE_STATIC);
                }
                else {
                  pos->fieldP->getAsString(s2);
                  // s2 is on the stack and will be deallocated on routine exit
                  rc=sqlite3_bind_blob(aStatement,paramno++,s2.c_str(),s2.size(),SQLITE_TRANSIENT);
                }
                break;
            } // switch
          } // if field
          break;
        case param_buffer:
          // we already have buffer pointers
          switch (pos->dbFieldType) {
            default:
            case dbft_string:
            case dbft_numeric:
              // external buffer remains stable until statement finalizes
              rc=sqlite3_bind_text(aStatement,paramno++,(const char *)pos->ParameterValuePtr,pos->StrLen_or_Ind,SQLITE_STATIC);
              break;
            case dbft_blob:
              // external buffer remains stable until statement finalizes
              rc=sqlite3_bind_blob(aStatement,paramno++,pos->ParameterValuePtr,pos->StrLen_or_Ind,SQLITE_STATIC);
              break;
          } // switch
          break;
        case param_localid_int:
        case param_localid_str:
        case param_remoteid_int:
        case param_remoteid_str:
          // invalid
          break;
      } // switch parammode
      if (rc==SQLITE_OK) {
        POBJDEBUGPRINTFX(aSessionP,DBG_DBAPI+DBG_EXOTIC,(
          "Bound Param #%d to SQLite statement",
          paramno
        ));
      }
      else {

      }
    } // if inparam
    else {
      POBJDEBUGPRINTFX(aSessionP,DBG_ERROR,("SQLite only supports IN params"));
    }
  } // for all parametermaps
} // TODBCApiAgent::bindSQLiteParameters


#endif // SQLITE_SUPPORT



// Session level DB access
// =======================


#ifdef ODBCAPI_SUPPORT

// get database time
lineartime_t TODBCApiAgent::getDatabaseNowAs(timecontext_t aTimecontext)
{
  lineartime_t now;

  if (fConfigP->fGetCurrentDateTimeSQL.empty()) {
    return inherited::getDatabaseNowAs(aTimecontext); // just use base class' implementation
  }
  else {
    // query database for current time
    SQLRETURN res;
    SQLHSTMT statement=newStatementHandle(getODBCConnectionHandle());
    try {
      // issue
      PDEBUGPUTSX(DBG_DBAPI,"SQL for getting current date/time:");
      PDEBUGPUTSX(DBG_DBAPI,fConfigP->fGetCurrentDateTimeSQL.c_str());
      TP_DEFIDX(li);
      TP_SWITCH(li,fTPInfo,TP_database);
      res = SafeSQLExecDirect(
        statement,
        (SQLCHAR *)fConfigP->fGetCurrentDateTimeSQL.c_str(),
        SQL_NTS
      );
      TP_START(fTPInfo,li);
      checkStatementError(res,statement);
      // - fetch result row
      res=SafeSQLFetch(statement);
      if (!checkStatementHasData(res,statement))
        throw TSyncException("no data getting date/time from database");
      // get datetime (in database time context)
      if (!getColumnValueAsTimestamp(statement,1,now))
        throw TSyncException("bad data getting date/time from database");
      SafeSQLCloseCursor(statement);
      // dispose statement handle
      SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
    }
    catch (...) {
      // dispose statement handle
      SafeSQLCloseCursor(statement);
      SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
      throw;
    }
    // convert to requested zone
    TzConvertTimestamp(now,fConfigP->fCurrentDateTimeZone,aTimecontext,getSessionZones(),TCTX_UNKNOWN);
    return now;
  }
} // TODBCApiAgent::getDatabaseNowAs


#ifdef SYSYNC_SERVER

// info about requested auth type
TAuthTypes TODBCApiAgent::requestedAuthType(void)
{
  TAuthTypes auth = TSyncAgent::requestedAuthType();

  // make sure that we don't request MD5 if we cannot check it:
  // either we need plain text passwords in the DB or SyncML V1.1 MD5 schema
  if (auth==auth_md5 && fSyncMLVersion<syncml_vers_1_1 && !fConfigP->fClearTextPw) {
    PDEBUGPRINTFX(DBG_ADMIN,("Switching down to Basic Auth because server cannot check pre-V1.1 MD5"));
    auth=auth_basic; // force basic auth, this can be checked
  }
  return auth;
} // TODBCApiAgent::requestedAuthType


// %%% note: make the following available in clients as well if they support more than
// pseudo-auth by local config (that is, real nonce is needed)

// - get nonce string, which is expected to be used by remote party for MD5 auth.
void TODBCApiAgent::getAuthNonce(const char *aDeviceID, string &aAuthNonce)
{
  if (!aDeviceID || fConfigP->fSaveNonceSQL.empty()) {
    // no device ID or no persistent nonce, use method of ancestor
    TStdLogicAgent::getAuthNonce(aDeviceID,aAuthNonce);
  }
  else {
    // we have a persistent nonce
    DEBUGPRINTFX(DBG_ADMIN,("getAuthNonce: current auth nonce='%s'",fLastNonce.c_str()));
    aAuthNonce=fLastNonce.c_str();
  }
} // TODBCApiAgent::getAuthNonce


// get next nonce (to be sent to remote party)
void TODBCApiAgent::getNextNonce(const char *aDeviceID, string &aNextNonce)
{
  SQLRETURN res;
  SQLHSTMT statement;
  string sql;

  if (fConfigP->fSaveNonceSQL.empty()) {
    // no persistent nonce, use method of ancestor
    TStdLogicAgent::getNextNonce(aDeviceID,aNextNonce);
  }
  else {
    // we have persistent nonce, create a new one and save it
    // - create one (pure 7bit ASCII)
    generateNonce(aNextNonce,aDeviceID,getSystemNowAs(TCTX_UTC)+rand());
    // - save it
    try {
      statement=newStatementHandle(getODBCConnectionHandle());
      try {
        // get SQL
        sql = fConfigP->fSaveNonceSQL;
        // - substitute: %N = new nonce
        StringSubst(sql,"%N",aNextNonce,2,chs_ascii,lem_none,fConfigP->fQuotingMode);
        // - standard substitutions: %u=userkey, %d=devicekey
        resetSQLParameterMaps();
        DoSQLSubstitutions(sql);
        bindSQLParameters(statement);
        // - issue
        PDEBUGPUTSX(DBG_ADMIN+DBG_DBAPI,"SQL for saving nonce");
        PDEBUGPUTSX(DBG_ADMIN+DBG_DBAPI,sql.c_str());
        TP_DEFIDX(li);
        TP_SWITCH(li,fTPInfo,TP_database);
        res = SafeSQLExecDirect(
          statement,
          (SQLCHAR *)sql.c_str(),
          SQL_NTS
        );
        TP_START(fTPInfo,li);
        checkStatementError(res,statement);
        saveAndCleanupSQLParameters(statement);
        // commit saved nonce
        SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
        SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_COMMIT);
      } // try
      catch (exception &e) {
        // release the statement handle
        SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
        SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_ROLLBACK);
        throw;
      }
    }
    catch (exception &e) {
      PDEBUGPRINTFX(DBG_ERROR,("Failed saving nonce: %s",e.what()));
      aNextNonce=fLastNonce; // return last nonce again
    }
  }
} // TODBCApiAgent::getNextNonce

#endif // SYSYNC_SERVER


#ifdef HAS_SQL_ADMIN

// cleanup what is needed after login
void TODBCApiAgent::LoginCleanUp(void)
{
  SQLRETURN res;

  try {
    // close the transaction as well
    if (fODBCConnectionHandle!=SQL_NULL_HANDLE) {
      res=SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_ROLLBACK);
      checkODBCError(res,SQL_HANDLE_DBC,getODBCConnectionHandle());
    }
  }
  catch (exception &e) {
    // log error
    PDEBUGPRINTFX(DBG_ERROR,("TODBCApiAgent::LoginCleanUp: Exception: %s",e.what()));
  }
} // TODBCApiAgent::LoginCleanUp


// check device related stuff
void TODBCApiAgent::CheckDevice(const char *aDeviceID)
{
  // first let ancestor
  if (!fConfigP->fGetDeviceSQL.empty()) {
    SQLRETURN res;
    SQLHSTMT statement;
    string sql;
    statement=newStatementHandle(getODBCConnectionHandle());
    try {
      bool creatednew=false;
      do {
        // get SQL
        sql = fConfigP->fGetDeviceSQL;
        // substitute: %D = deviceID
        StringSubst(sql,"%D",aDeviceID,2,-1,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
        // issue
        PDEBUGPUTSX(DBG_ADMIN+DBG_DBAPI,"SQL for getting device key (and last nonce sent to device)");
        PDEBUGPUTSX(DBG_ADMIN+DBG_DBAPI,sql.c_str());
        TP_DEFIDX(li);
        TP_SWITCH(li,fTPInfo,TP_database);
        res = SafeSQLExecDirect(
          statement,
          (SQLCHAR *)sql.c_str(),
          SQL_NTS
        );
        TP_START(fTPInfo,li);
        checkStatementHasData(res,statement);
        // - fetch result row
        res=SafeSQLFetch(statement);
        if (!checkStatementHasData(res,statement)) {
          if (creatednew) {
            throw TSyncException("Fatal: inserted device record cannot be found again");
          }
          PDEBUGPRINTFX(DBG_ADMIN,("Unknown device '%.30s', creating new record",aDeviceID));
          if (IS_SERVER) {
            #ifdef SYSYNC_SERVER
            // device does not exist yet
            fLastNonce.erase();
            #endif
          }
          // create new device
          SafeSQLCloseCursor(statement);
          // - get SQL
          sql = fConfigP->fNewDeviceSQL;
          string dk; // temp device key
          // - substitute: %D = deviceID
          StringSubst(sql,"%D",aDeviceID,2,-1,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
          // - issue
          PDEBUGPUTSX(DBG_ADMIN+DBG_DBAPI,"SQL for inserting new device (and last nonce sent to device)");
          PDEBUGPUTSX(DBG_ADMIN+DBG_DBAPI,sql.c_str());
          TP_DEFIDX(li);
          TP_SWITCH(li,fTPInfo,TP_database);
          res = SafeSQLExecDirect(
            statement,
            (SQLCHAR *)sql.c_str(),
            SQL_NTS
          );
          TP_START(fTPInfo,li);
          checkStatementError(res,statement);
          // commit new device
          SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_COMMIT);
          // try again to get device key
          creatednew=true;
          continue;
        }
        else {
          // device exists, read key and nonce
          #ifdef SCRIPT_SUPPORT
          fUnknowndevice=false; // we have seen it once before at least
          #endif
          // - device key
          getColumnValueAsString(statement,1,fDeviceKey,chs_ascii);
          // - nonce
          if (IS_CLIENT) {
            string dummy;
            getColumnValueAsString(statement,2,dummy,chs_ascii);
          }
          else {
            #ifdef SYSYNC_SERVER
            getColumnValueAsString(statement,2,fLastNonce,chs_ascii);
            #endif
          }
          // - done for now
          SafeSQLCloseCursor(statement);
          PDEBUGPRINTFX(DBG_ADMIN,("Device '%.30s' found, fDeviceKey='%.30s'",aDeviceID,fDeviceKey.c_str()));
          if (IS_SERVER) {
            #ifdef SYSYNC_SERVER
            DEBUGPRINTFX(DBG_ADMIN,("Last nonce saved for device='%.30s'",fLastNonce.c_str()));
            #endif
          }
          break;
        }
      } while (true); // do until device found or created new
    } // try
    catch (...) {
      // release the statement handle
      SafeSQLCloseCursor(statement);
      SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
      SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_ROLLBACK);
      throw;
    }
    // release the statement handle
    SafeSQLCloseCursor(statement);
    SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
  } // if device table implemented
} // TODBCApiAgent::CheckDevice


// check credential string
bool TODBCApiAgent::CheckLogin(const char *aOriginalUserName, const char *aModifiedUserName, const char *aAuthString, TAuthSecretTypes aAuthStringType, const char *aDeviceID)
{
  bool authok = false;
  string nonce;
  SQLRETURN res;
  SQLHSTMT statement;

  // - if no userkey query, we cannot proceed and must fail auth now
  if (fConfigP->fUserKeySQL.empty()) return false;
  // - get nonce (if we have a device table, we should have read it by now)
  if (aAuthStringType==sectyp_md5_V10 || aAuthStringType==sectyp_md5_V11)
    getAuthNonce(aDeviceID,nonce);
  // - try to obtain an appropriate user key
  statement=newStatementHandle(getODBCConnectionHandle());
  try {
    // get SQL
    string sql = fConfigP->fUserKeySQL;
    string uk; // temp user key
    // substitute: %U = original username as sent by remote,
    //             %dU = username modified for DB search,
    //             %M = authstring, %N = Nonce string
    StringSubst(sql,"%U",aOriginalUserName,2,-1,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
    StringSubst(sql,"%dU",aModifiedUserName,3,-1,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
    // %%% probably obsolete
    StringSubst(sql,"%M",aAuthString,2,-1,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
    StringSubst(sql,"%N",nonce,2,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
    // now do the standard stuff
    resetSQLParameterMaps();
    DoSQLSubstitutions(sql);
    bindSQLParameters(statement);
    // issue
    PDEBUGPUTSX(DBG_ADMIN+DBG_DBAPI,"SQL for getting user key (and password/md5userpass if not checked in query):");
    PDEBUGPUTSX(DBG_ADMIN+DBG_DBAPI,sql.c_str());
    TP_DEFIDX(li);
    TP_SWITCH(li,fTPInfo,TP_database);
    res = SafeSQLExecDirect(
      statement,
      (SQLCHAR *)sql.c_str(),
      SQL_NTS
    );
    TP_START(fTPInfo,li);
    checkStatementHasData(res,statement);
    saveAndCleanupSQLParameters(statement);
    // - fetch result row
    res=SafeSQLFetch(statement);
    if (fConfigP->fClearTextPw || fConfigP->fMD5UserPass) {
      authok=false;
      // go though all returned rows until match (same username might be used with 2 different pws)
      while (checkStatementHasData(res,statement)) {
        // read in ascending index order!!
        string secret;
        SQLSMALLINT col=1;
        // - user key
        getColumnValueAsString(statement,col,uk,chs_ascii);
        col++;
        #ifdef SCRIPT_SUPPORT
        TItemField *hfP;
        //   also store it in first local context var
        if (fAgentContext) {
          hfP=fAgentContext->getLocalVar(0);
          if (hfP) hfP->setAsString(uk.c_str());
        }
        #endif
        //   also store it in fUserKey already for USERKEY() function
        fUserKey=uk;
        // - password or md5userpass
        getColumnValueAsString(statement,col,secret,chs_ascii);
        // - convert from hex to b64 if needed
        if (fConfigP->fMD5UPAsHex && secret.size()>=32) {
          uInt8 md5[16];
          cAppCharP p=secret.c_str();
          for (int k=0; k<16; k++) {
            uInt16 h;
            if (HexStrToUShort(p+(k*2),h,2)<2) break;
            md5[k]=(uInt8)h;
          }
          p = b64::encode(&md5[0],16);
          secret = p; // save converted string
          b64::free((void *)p);
        }
        col++;
        #ifdef SCRIPT_SUPPORT
        //   also store it in second local context var
        if (fAgentContext) {
          hfP=fAgentContext->getLocalVar(1);
          if (hfP) hfP->setAsString(secret.c_str());
          // if result set has more columns, save them as well
          SQLSMALLINT numcols;
          // - get number of columns
          res = SafeSQLNumResultCols(statement,&numcols);
          checkStatementError(res,statement);
          // - save remaining columns into defined variables
          while (col<=numcols) {
            // check if enough variables
            if (col>fAgentContext->getNumLocals()) break;
            // get variable
            hfP=fAgentContext->getLocalVar(col-1);
            if (!hfP) break;
            // timestamp is special
            if (hfP->getType()==fty_timestamp) {
              // get as timestamp
              lineartime_t ts;
              if (!getColumnValueAsTimestamp(statement,col,ts)) break;
              static_cast<TTimestampField *>(hfP)->setTimestampAndContext(ts,fConfigP->fCurrentDateTimeZone);
            }
            else {
              // get and assign as string
              string v;
              if (!getColumnValueAsString(statement,col,v,chs_ascii)) break;
              hfP->setAsString(v.c_str());
            }
            col++;
          }
        }
        #endif
        // make standard auth check
        if (fConfigP->fClearTextPw) {
          // we have the clear text password, check against what was transmitted from remote
          authok=checkAuthPlain(aOriginalUserName,secret.c_str(),nonce.c_str(),aAuthString,aAuthStringType);
        }
        else {
          // whe have the MD5 of user:password, check against what was transmitted from remote
          // Note: this can't work with non-V1.1 type creds
          authok=checkAuthMD5(aOriginalUserName,secret.c_str(),nonce.c_str(),aAuthString,aAuthStringType);
        }
        #ifdef SCRIPT_SUPPORT
        // if there is a script, call it to perform decision
        // - refresh auth status
        fStandardAuthOK=authok; // for AUTHOK() func
        // - call script (if any)
        fScriptContextDatastore=NULL;
        authok=TScriptContext::executeTest(
          authok, // default for no script, no result or script error is current auth
          fAgentContext,
          fConfigP->fLoginCheckScript,
          fConfigP->getAgentFuncTableP(),  // context function table
          (void *)this // context data (myself)
        );
        #endif
        if (authok) break; // found, authorized
        // fetch next
        res=SafeSQLFetch(statement);
      }
    }
    else {
      // AuthString was checked in SQL query
      // - no record found -> no auth
      if (res==SQL_NO_DATA) authok=false;
    }
    if (authok) {
      // now assign user key
      fUserKey=uk;
      PDEBUGPRINTFX(DBG_ADMIN,("Auth successful, fUserKey='%s'",fUserKey.c_str()));
    }
    else {
      // we do not have a valid user key
      fUserKey.erase();
    }
  }
  catch (...) {
    // release the statement handle
    SafeSQLCloseCursor(statement);
    SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
    SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_ROLLBACK);
    throw;
  }
  // release the statement handle
  SafeSQLCloseCursor(statement);
  SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
  // return auth status
  return authok;
} // TODBCApiAgent::CheckLogin


// do something with the analyzed data
void TODBCApiAgent::remoteAnalyzed(void)
{
  SQLRETURN res;
  SQLHSTMT statement;
  string sql;

  // call ancestor first
  inherited::remoteAnalyzed();
  if (!fConfigP->fSaveInfoSQL.empty()) {
    // save info string
    statement=newStatementHandle(getODBCConnectionHandle());
    try {
      // get SQL
      sql = fConfigP->fSaveInfoSQL;
      // %nR  Remote name: [Manufacturer ]Model")
      StringSubst(sql,"%nR",getRemoteDescName(),3,-1,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
      // %vR  Remote Device Version Info ("Type (HWV, FWV, SWV) Oem")
      StringSubst(sql,"%vR",getRemoteInfoString(),3,-1,fConfigP->fDataCharSet,fConfigP->fDataLineEndMode,fConfigP->fQuotingMode);
      // - standard substitutions: %u=userkey, %d=devicekey
      resetSQLParameterMaps();
      DoSQLSubstitutions(sql);
      bindSQLParameters(statement);
      // - issue
      PDEBUGPUTSX(DBG_ADMIN+DBG_DBAPI,"SQL for saving device info");
      PDEBUGPUTSX(DBG_ADMIN+DBG_DBAPI,sql.c_str());
      TP_DEFIDX(li);
      TP_SWITCH(li,fTPInfo,TP_database);
      res = SafeSQLExecDirect(
        statement,
        (SQLCHAR *)sql.c_str(),
        SQL_NTS
      );
      TP_START(fTPInfo,li);
      checkStatementError(res,statement);
      saveAndCleanupSQLParameters(statement);
      // commit new device
      SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
      SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_COMMIT);
    } // try
    catch (exception &e) {
      // release the statement handle
      SafeSQLFreeHandle(SQL_HANDLE_STMT,statement);
      SafeSQLEndTran(SQL_HANDLE_DBC,getODBCConnectionHandle(),SQL_ROLLBACK);
      PDEBUGPRINTFX(DBG_ERROR,("Failed saving device info: %s",e.what()));
    }
  }
} // TODBCApiAgent::remoteAnalyzed

#endif // HAS_SQL_ADMIN

#endif // ODBCAPI_SUPPORT



#ifdef SYSYNC_SERVER

void TODBCApiAgent::RequestEnded(bool &aHasData)
{
  // first let ancestors finish their stuff
  inherited::RequestEnded(aHasData);
  #ifdef ODBCAPI_SUPPORT
  // now close the ODBC connection if there is one left at the session level
  // and no script statement is still unfinished
  #ifdef SCRIPT_SUPPORT
  if (!fScriptStatement)
  #endif
  {
    closeODBCConnection(fODBCConnectionHandle);
  }
  #endif
} // TODBCApiAgent::RequestEnded

#endif // SYSYNC_SERVER


// factory methods of Agent config
// ===============================

#ifdef SYSYNC_CLIENT

TSyncAgent *TOdbcAgentConfig::CreateClientSession(const char *aSessionID)
{
  // return appropriate client session
  MP_RETURN_NEW(TODBCApiAgent,DBG_HOT,"TODBCApiAgent",TODBCApiAgent(getSyncAppBase(),NULL,aSessionID));
} // TOdbcAgentConfig::CreateClientSession

#endif

#ifdef SYSYNC_SERVER

TSyncAgent *TOdbcAgentConfig::CreateServerSession(TSyncSessionHandle *aSessionHandle, const char *aSessionID)
{
  // return XML2GO or ODBC-server session
  MP_RETURN_NEW(TODBCApiAgent,DBG_HOT,"TODBCApiAgent",TODBCApiAgent(getSyncAppBase(),aSessionHandle,aSessionID));
} // TOdbcAgentConfig::CreateServerSession

#endif


} // namespace sysync

/* end of TODBCApiAgent implementation */

#endif // SQL_SUPPORT

// eof
