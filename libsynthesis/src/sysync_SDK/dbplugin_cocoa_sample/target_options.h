// Sysync related target options for sample database adaptor plugin

#ifndef TARGET_OPTIONS_H
#define TARGET_OPTIONS_H

// define platform
// - we are on MACOSX (but on the mobile variant of it)
#define MACOSX 1
#define MOBOSX 1

// this is not the SyncML engine itself
#undef SYSYNC_ENGINE

// Sysync related debug
#define SYDEBUG 2

// general app debug
#if DEBUG
  // general app debug
  #define CONSOLEDBG 1
#else
  // no output to console, please
  #undef CONSOLEDBG
#endif

// this can be #undef'ed when the DB does not need finalizing local IDs
#define HAS_FINALIZE_LOCALID 1

#endif // TARGET_OPTIONS_H

/* eof */
