/*
 *  SySyTool
 *    Synthesis SyncML Diagnostic Tool
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 */


#include <iostream>
#include <signal.h>

#include "sysytool.h"

// include files where commands are implemented
#include "odbcapiagent.h"
#include "syncagent.h"
#include "sysync_utils.h"
#include "rrules.h"

static void cleanup(void)
{
  NCDEBUGPRINTFX(DBG_ERROR,("Program aborted, atexit handler will now kill syncappbase"));
  // kill base object if any (will cause all current sessions to die)
  freeSyncAppBase();
}

static void sighandler(int sig)
{
  cleanup();
}


// global options
const char *gConfigFile = NULL; // -f, not optional
const char *gUserName = NULL;  // -u
const char *gDeviceID = NULL; // -d
bool gVerbose = false; // -v
bool gSilent = false; // -s


static void printTitle(void)
{
	CONSOLEPRINTF((
	  "Synthesis SyncML Diagnostic Tool %d.%d.%d.%d - (c) 2004-" REAL_RELEASE_YEAR_TXT " by Synthesis AG\n",
    SYSYNC_VERSION_MAJOR,
    SYSYNC_VERSION_MINOR,
    SYSYNC_SUBVERSION,
    SYSYNC_BUILDNUMBER
	));
} // printTitle


static void printUsage(const char *aCmdName)
{
  printTitle();
  CONSOLEPRINTF(("Usage:"));
  CONSOLEPRINTF(("  %s [<options>] <command> [<commandarg> ...]",aCmdName));
  CONSOLEPRINTF(("Options:"));
  CONSOLEPRINTF(("  -f configfile    : (required) server's xml configuration file"));
  CONSOLEPRINTF(("Commands:"));
  CONSOLEPRINTF(("  check"));
  CONSOLEPRINTF(("    check config for syntax errors"));
  // let them show their own help text
  #ifdef SQL_SUPPORT
  execSQL(-1,NULL);
  #endif
  convertData(-1,NULL);
  testLogin(-1,NULL);
  charConv(-1,NULL);
  timeConv(-1,NULL);
  rruleConv(-1,NULL);
  parse2822AddrSpec(-1,NULL);
  wbxmlConv(-1,NULL);
}


int main(int argc, char *argv[])
{
  Ret_t err;

  #ifdef MEMORY_PROFILING
  size_t mematstart = gAllocatedMem;
  #endif

  // init hyper global stuff
  sysync_glob_init();

  // set exit handler
  atexit(cleanup);
  signal(SIGABRT,sighandler);

  const int maxcmdargs=5;
  int cmdargc=-1; // number of non-option command arguments (-1 = command not found)
  const char *command = NULL;
  const char *cmdargv[maxcmdargs];

  int idx=1;
  const char *p;
  while (idx<argc) {
    // get arg
    p=argv[idx];
    // check for option
    if (*p=='-') {
      ++p;
      if (*p=='-') {
        // extended option syntax
        // %%% tbd
        CONSOLEPRINTF(("Invalid Option '-%s'\n",p));
        printUsage(argv[0]);
        exit(EXIT_FAILURE);
      }
      else {
        // get next arg if any
        const char* optarg = NULL;
        if (idx+1<argc)
          optarg = argv[idx+1];
        // evaluate option
        switch (*p) {
          case 'h' : // help
            printUsage(argv[0]);
            exit(EXIT_SUCCESS);
          case 'f' :
            gConfigFile = optarg;
            break;
          case 'u' :
            gUserName = optarg;
            break;
          case 'd' :
            gDeviceID = optarg;
            break;
          case 'v' :
            gVerbose = true;
            gSilent = false;
            optarg=NULL;
            break;
          case 's' :
            gSilent = true;
            optarg=NULL;
            break;
          default:
            CONSOLEPRINTF(("Unknown Option '-%c'\n",*p));
            printUsage(argv[0]);
            exit(EXIT_FAILURE);
        }
        // skip option argument if consumed by option
        if (optarg) {
          idx++;
        }
      } // single char options
    } // option
    else {
      // non-option
      if (cmdargc<0) {
        // this is the command
        command = argv[idx];
        cmdargc=0;
      }
      else if (cmdargc<maxcmdargs) {
        cmdargv[cmdargc]=argv[idx];
        cmdargc++;
      }
    }
    // next arg
    idx++;
  } // while args

  // check if we have a command
  if (!command) {
    CONSOLEPRINTF(("No command specified\n"));
    printUsage(argv[0]);
    exit(EXIT_FAILURE);
  }

  // now we need a config first
  if (!gConfigFile) {
    CONSOLEPRINTF(("Please specify a config file using the -f option!\n"));
    printUsage(argv[0]);
    exit(EXIT_FAILURE);
  }
  // read config
  // - create dispatcher object
  TXPTSessionDispatch *sessionDispatchP = getXPTSessionDispatch();
  // let it read the config
  err = sessionDispatchP->readXMLConfigStandard(gConfigFile,false,true); // local and global try
  // test if config is ok
  if (err!=LOCERR_OK) {
    CONSOLEPRINTF(("Fatal error in configuration (Error=%hd)\n",err));
    exit(EXIT_FAILURE);
  }

  // override config's debug options to make it suitable for console output
  TDebugLogger *loggerP = sessionDispatchP->getDbgLogger();
  TDbgOptions *optsP = (TDbgOptions *)(loggerP->getOptions());
  optsP->fIndentString.erase();
  optsP->fOutputFormat=dbgfmt_text;
  optsP->fTimestampForAll=false;
  optsP->fTimestampStructure=true;
  if (gVerbose) {
    sessionDispatchP->getDbgLogger()->setMask(DBG_TOOL);
    sessionDispatchP->getDbgLogger()->setEnabled(true);
  }
  else {
    sessionDispatchP->getDbgLogger()->setMask(0);
    sessionDispatchP->getDbgLogger()->setEnabled(false);
  }
  // now execute command
  if (strucmp(command,"check")==0)
    exit(EXIT_SUCCESS); // ok if we get that far
  #ifdef SQL_SUPPORT
  else if (strucmp(command,"execsql")==0)
    exit(sysync::execSQL(cmdargc,cmdargv));
  #endif
  else if (strucmp(command,"convert")==0)
    exit(sysync::convertData(cmdargc,cmdargv));
  else if (strucmp(command,"login")==0)
    exit(sysync::testLogin(cmdargc,cmdargv));
  else if (strucmp(command,"charconv")==0)
    exit(sysync::charConv(cmdargc,cmdargv));
  else if (strucmp(command,"addrparse")==0)
    exit(sysync::parse2822AddrSpec(cmdargc,cmdargv));
  else if (strucmp(command,"time")==0)
    exit(sysync::timeConv(cmdargc,cmdargv));
  else if (strucmp(command,"rrule")==0)
    exit(sysync::rruleConv(cmdargc,cmdargv));
  else if (strucmp(command,"wbxml2xml")==0)
    exit(sysync::wbxmlConv(cmdargc,cmdargv));
  else {
    CONSOLEPRINTF(("Unknown Command '%s'\n",command));
    printUsage(argv[0]);
    exit(EXIT_FAILURE);
  }
}

// eof
