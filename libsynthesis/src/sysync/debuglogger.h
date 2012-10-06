/*
 *  File:         debuglogger.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Global debug mechanisms
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 *  2005-08-04 : luz : created
 *
 */

#ifndef DEBUGLOGGER_H
#define DEBUGLOGGER_H

#ifdef SYDEBUG

#include "generic_types.h"
#include "platform_mutex.h"
#include "sysync.h"

namespace sysync {


/// @brief Debug output formats
typedef enum {
  dbgfmt_text,        ///< plain text format (but can be indented)
  dbgfmt_xml,         ///< XML format
  dbgfmt_html,        ///< HTML format
  numDbgOutFormats
} TDbgOutFormats;

/// @brief HTML dynamic folding
typedef enum {
  dbgfold_none,       ///< do not include dynamic folding into HTML logs
  dbgfold_collapsed,  ///< include folding - all collapsed by default
  dbgfold_expanded,   ///< include folding - all expanded by default
  dbgfold_auto,       ///< include folding - collapse/expand state predefined on a block-by-block basis
  numDbgFoldingModes
} TDbgFoldingModes;

/// @brief Debug flush modes
typedef enum {
  dbgflush_none,      ///< no flush, keep open as long as possible
  dbgflush_flush,     ///< flush every debug message
  dbgflush_openclose, ///< open and close debug channel separately for every message (as in 2.x engine)
  numDbgFlushModes
} TDbgFlushModes;

/// @brief Debug subthread logging modes
typedef enum {
  dbgsubthread_none,          ///< do not handle output from subthread specially
  dbgsubthread_suppress,      ///< suppress output from subthreads
  dbgsubthread_separate,      ///< create separate output stream (=file) for each subthread
  dbgsubthread_linemix,       ///< mix output on a line by line basis (forcing output to slow openclose mode)
  dbgsubthread_bufferandmix,  ///< buffer thread's output and mix it into main stream when appropriate
  numDbgSubthreadModes
} TDbgSubthreadModes;


/// @brief HTML linking into source code
typedef enum {
  dbgsource_none,     ///< do not include links into source code in HTML logs
  dbgsource_hint,     ///< no links, but info about what file/line number the message comes from
  dbgsource_doxygen,  ///< include link into doxygen prepared HTML version of source code
  dbgsource_txmt,     ///< include txmt:// link (understood by TextMate and BBEdit) into source code
  numDbgSourceModes
} TDbgSourceModes;


#ifndef HARDCODED_CONFIG
extern cAppCharP const DbgOutFormatNames[numDbgOutFormats];
extern cAppCharP const DbgFoldingModeNames[numDbgFoldingModes];
extern cAppCharP const DbgFlushModeNames[numDbgFlushModes];
extern cAppCharP const DbgSubthreadModeNames[numDbgSubthreadModes];
extern cAppCharP const DbgSourceModeNames[numDbgSourceModes];
#endif
extern cAppCharP const DbgOutFormatExtensions[numDbgOutFormats];

/// @brief Debug options container
class TDbgOptions {
public:
  // constructor
  TDbgOptions();
  // methods
  void clear(void);
  // properties
  TDbgOutFormats fOutputFormat; ///< format
  string fIndentString; ///< indent string
  string fCustomPrefix; ///< custom prefix (different xml header or html with different styles for example)
  string fCustomSuffix; ///< custom suffix (should match prefix)
  string fBasename; ///< the initial part of the log file name, can override the hard-coded TARGETID (empty if unset)
  bool fSeparateMsgs; ///< separate message lines (needed especially in XML to avoid unformatted PCDATA block)
  bool fTimestampStructure; ///< include timestamp for structure elements (blocks)
  bool fTimestampForAll; ///< include timestamp information for every message
  bool fThreadIDForAll; ///< include thread ID information for every message
  TDbgFlushModes fFlushMode; ///< how and when to flush
  TDbgFoldingModes fFoldingMode; ///< if and how to fold HTML output
  TDbgSourceModes fSourceLinkMode; ///< if and how to link with source code
  string fSourceRootPath; ///< defines root path for source links
  bool fAppend; ///< if set, existing debug files will not be overwritten, but appended to
  TDbgSubthreadModes fSubThreadMode; ///< how to handle debug messages from subthreads
  uInt32 fSubThreadBufferMax; ///< how much to buffer for subthread maximally
}; // TDbgOptions


/// @brief Debug output channel
class TDbgOut {
  // construction/destruction
private:
  bool fDestructed; // flag which will be set once destruct() has been called - by the outermost derivate's destructor
public:
  TDbgOut();
  virtual ~TDbgOut();
  virtual void doDestruct(void); // will be called by destruct, derived must call inherited if they implement it
  void destruct(void); // to be called by ALL destructors of derivates.
  // methods
  /// @brief duplicate output channel
  virtual TDbgOut *clone(void) { return new TDbgOut; };
  /// @brief open debug output channel
  /// Notes:
  /// - Autocloses current channel if already open
  /// - May not actually open the channel, but should test if channel is writable
  /// @return                         false if debug channel cannot be opened and written to
  /// @param aDbgOutputName[in]       name (usually file name) of debug output channel
  /// @param aSuggestedExtension[in]  file extension suggested (may not be used depending on channel type)
  /// @param aFlushMode[in]           flush mode to be used on the channel
  /// @param aOverWrite[in]           if true, debug output channel (=file) will be overwritten (otherwise: appended to)
  /// @param aRawMode[in]             if true, debug output channel (=file) is opened in binary raw mode (for message dumps etc.)
  virtual bool openDbg(cAppCharP aDbgOutputName, cAppCharP aSuggestedExtension, TDbgFlushModes aFlushMode, bool aOverWrite, bool aRawMode=false) { return true; };
  /// @brief get current size of output file
  /// @return number of bytes, 0 if file is empty or for non-files (like console)
  virtual uInt32 dbgFileSize(void) { return 0; };
  /// @brief close and flush all log output
  virtual void closeDbg(void) { /* nop */ };
  /// @brief write single line to debug output channel
  /// @param aLine[in]                text for line to be written out (must not contain line ends)
  /// @param aForceFlush[in]          if true, debug output will be flushed to permanent storage regardless of current flush mode
  virtual void putLine(cAppCharP aLine, bool aForceFlush) { /* nop */};
  /// @brief write raw data to debug output channel (usually makes sense only when channel is opened in raw mode)
  /// @param aData[in]                pointer to data to be written
  /// @param aSize[in]                size in bytes of data block at aData to be written
  virtual void putRawData(cAppPointer aData, memSize aSize) { /* nop */};
protected:
  bool fIsOpen;
}; // TDbgOut



#ifndef NO_C_FILES

/// @brief Standard file debug output channel
class TStdFileDbgOut : public TDbgOut {
  typedef TDbgOut inherited;
public:
  // constructor/destructor
  TStdFileDbgOut();
  virtual ~TStdFileDbgOut();
  // methods
  virtual TDbgOut *clone(void) { return new TStdFileDbgOut; };
  virtual bool openDbg(cAppCharP aDbgOutputName, cAppCharP aSuggestedExtension, TDbgFlushModes aFlushMode, bool aOverWrite, bool aRawMode=false);
  virtual uInt32 dbgFileSize(void);
  virtual void closeDbg(void);
  virtual void putLine(cAppCharP aLine, bool aForceFlush);
  virtual void putRawData(cAppPointer aData, memSize aSize);
private:
  TDbgFlushModes fFlushMode;
  string fFileName;
  FILE * fFile;
  MutexPtr_t mutex;
}; // TStdFileDbgOut

#endif


/// @brief Output to console
class TConsoleDbgOut : public TDbgOut {
  typedef TDbgOut inherited;
public:
  // constructor/destructor
  TConsoleDbgOut();
  // methods
  virtual TDbgOut *clone(void) { return new TConsoleDbgOut; };
  virtual bool openDbg(cAppCharP aDbgOutputName, cAppCharP aSuggestedExtension, TDbgFlushModes aFlushMode, bool aOverWrite, bool aRawMode=false);
  virtual void closeDbg(void);
  virtual void putLine(cAppCharP aLine, bool aForceFlush);
  virtual void putRawData(cAppPointer aData, memSize aSize) { /* not supported on console, just NOP */ };
}; // TConsoleDbgOut


// Debug logger class
// ------------------

/// @brief hierachical block history
typedef struct BlockLevel {
  string fBlockName;
  uInt32 fBlockNo;
  struct BlockLevel *fNext;
} TBlockLevel;

class TDebugLogger;
class GZones;

/// @brief Debug logger base class (without subthread handling)
class TDebugLoggerBase {
public:
  // constructor/destructor
  TDebugLoggerBase(GZones *aGZonesP);
  virtual ~TDebugLoggerBase();
  // methods
  /// @brief install output channel handler object (and pass it's ownership!)
  /// @param aDbgOutP[in] output channel to be used for this logger (will be owned and finally destroyed by the logger)
  void installOutput(TDbgOut *aDbgOutP);
  /// @brief link this logger to another logger and redirect output to that logger
  /// @param aDebugLoggerP[in] another logger, that must be alive as long as this logger is alive
  void outputVia(TDebugLoggerBase *aDebugLoggerP);
  /// @brief check if an output channel is already established other than with default values
  bool outputEstablished(void) { return fOutStarted; };
  /// @brief set debug options
  void setOptions(const TDbgOptions *aDbgOptionsP) { fDbgOptionsP=aDbgOptionsP; };
  /// @brief get debug options pointer
  const TDbgOptions *getOptions(void) { return fDbgOptionsP; };
  // @brief convenience version for getting time
  lineartime_t getSystemNowAs(timecontext_t aContext);
  /// @brief get current debug mask for this logger.
  /// Note that setEnabled(false) will cause this to return 0 even if the mask itself is non-zero
  uInt32 getMask(void) { return fDebugEnabled ? fDebugMask : 0; };
  uInt32 getRealMask(void) { return fDebugMask; };
  /// @brief set new debug mask for this logger
  void setMask(uInt32 aDbgMask)
    { fDebugMask=aDbgMask; };
  /// @brief enable or disable this logger (but leave dbgMask intact)
  void setEnabled(bool aEnabled)
    { fDebugEnabled=aEnabled; };
  /// @brief set debug output path + filename (no extension, please)
  void setDebugPath(cAppCharP aPath) { fDbgPath = aPath; };
  /// @brief append to debug output path + filename (no extension, please)
  void appendToDebugPath(cAppCharP aPathElement) { fDbgPath += aPathElement; };
  /// @brief get debug output file path (w/o extension)
  cAppCharP getDebugPath(void) { return fOutputLoggerP ? fOutputLoggerP->getDebugPath() : fDbgPath.c_str(); };
  /// @brief get debug output file name (w/o path or extension)
  cAppCharP getDebugFilename(void) { if (fOutputLoggerP) return fOutputLoggerP->getDebugFilename(); size_t n=fDbgPath.find_last_of("\\/:"); return fDbgPath.c_str()+(n!=string::npos ? n+1 : 0); };
  /// @brief get debug output file extension
  cAppCharP getDebugExt(void) { return fOutputLoggerP ? fOutputLoggerP->getDebugExt() : fDbgOptionsP ? DbgOutFormatExtensions[fDbgOptionsP->fOutputFormat] : ""; };
  // - normal output
  /// @brief Write text to debug output channel.
  /// Notes:
  /// - Line will be terminated by linefeed automatically (no need to include a linefeed for single line message)
  /// - \n chars can be used to separate multi-line output. Formatter will take care that
  ///   all lines are equally indented/formatted/prefixed
  /// @param aDbgMask debug mask, bits set here must be set in the debuglogger's own mask in order to display the debug text
  /// @param aText[in] text to be written out
  /// @param aTextSize[in] if>0, this is the maximum number of chars to output from aText
  virtual void DebugPuts(TDBG_LOCATION_PROTO uInt32 aDbgMask, cAppCharP aText, stringSize aTextSize=0, bool aPreFormatted=false);
  /// @brief Write formatted text to debug output channel.
  /// @param aDbgMask debug mask, bits set here must be set in the debuglogger's own mask in order to display the debug text
  /// @param aFormat[in] format text in vprintf style to be written out
  /// @param aArgs[in] varargs in vprintf style
  virtual void DebugVPrintf(TDBG_LOCATION_PROTO uInt32 aDbgMask, cAppCharP aFormat, va_list aArgs);
  /// @brief Write formatted text to debug output channel.
  /// @param aDbgMask debug mask, bits set here must be set in the debuglogger's own mask in order to display the debug text
  /// @param aFormat[in] format text in printf style to be written out
  void DebugPrintf(TDBG_LOCATION_PROTO uInt32 aDbgMask, cAppCharP aFormat, ...)
    #ifdef __GNUC__
    __attribute__((format(printf, TDBG_LOCATION_ARG_NUM + 3, TDBG_LOCATION_ARG_NUM + 4)))
    #endif
    ;
  /// @brief set debug mask to be used for next DebugPrintfLastMask() call
  /// @param aDbgMask debug mask, bits set here must be set in the debuglogger's own mask in order to display the debug text
  virtual TDebugLoggerBase &setNextMask(uInt32 aDbgMask);
  /// @brief like DebugPrintf(), but using mask previously set by setNextMask()
  /// @param aFormat[in] format text in printf style to be written out
  void DebugPrintfLastMask(TDBG_LOCATION_PROTO cAppCharP aFormat, ...)
    #ifdef __GNUC__
    __attribute__((format(printf, TDBG_LOCATION_ARG_NUM + 2, TDBG_LOCATION_ARG_NUM + 3)))
    #endif
    ;
  // - Blocks
  /// @brief Open structure Block. Depending on the output format, this will generate indent, XML tags, HTML headers etc.
  /// @param aBlockName[in]  Name of Block. Will be used e.g. for tag name in XML. Intention is to group similar entities with the same BlockName
  /// @param aBlockTitle[in] Title (descriptive text) of Block.
  /// @param aCollapsed[in]  If set, and folding mode is auto, block will be initially collapsed when log is opened in browser.
  /// @param aBlockFmt[in]   Format string for additional Block info. Should contain one or multiple tag=value pairs, separated by the pipe char |.
  ///                          This will be used to generate XML attributes or other identifiers.
  /// @param aArgs[in]         varargs in vprintf style for aBlockFmt
  virtual void DebugVOpenBlock(TDBG_LOCATION_PROTO cAppCharP aBlockName, cAppCharP aBlockTitle, bool aCollapsed, cAppCharP aBlockFmt, va_list aArgs);
  /// @brief Open structure Block, printf style variant
  void DebugOpenBlock(TDBG_LOCATION_PROTO cAppCharP aBlockName, cAppCharP aBlockTitle, bool aCollapsed, cAppCharP aBlockFmt, ...)
    #ifdef __GNUC__
    __attribute__((format(printf, TDBG_LOCATION_ARG_NUM + 5, TDBG_LOCATION_ARG_NUM + 6)))
    #endif
      ;
  void DebugOpenBlockExpanded(TDBG_LOCATION_PROTO cAppCharP aBlockName, cAppCharP aBlockTitle, cAppCharP aBlockFmt, ...)
    #ifdef __GNUC__
    __attribute__((format(printf, TDBG_LOCATION_ARG_NUM + 4, TDBG_LOCATION_ARG_NUM + 5)))
    #endif
    ;
  void DebugOpenBlockCollapsed(TDBG_LOCATION_PROTO cAppCharP aBlockName, cAppCharP aBlockTitle, cAppCharP aBlockFmt, ...)
    #ifdef __GNUC__
    __attribute__((format(printf, TDBG_LOCATION_ARG_NUM + 4, TDBG_LOCATION_ARG_NUM + 5)))
    #endif
    ;
  /// @brief Open structure Block, without any attributes
  virtual void DebugOpenBlock(TDBG_LOCATION_PROTO cAppCharP aBlockName, cAppCharP aBlockTitle=NULL, bool aCollapsed=false);
  /// @brief Close structure Block. Name is used to close possibly unclosed contained Blocks automatically.
  virtual void DebugCloseBlock(TDBG_LOCATION_PROTO cAppCharP aBlockName);
protected:
  // helper methods
  /// @brief start debugging output if needed and sets fOutStarted
  bool DebugStartOutput(void);
  /// @brief Output single line to debug channel (includes indenting, but no other formatting)
  void DebugPutLine(TDBG_LOCATION_PROTO cAppCharP aText, stringSize aTextSize=0, bool aPre=false);
  /// @brief finalize debugging output
  void DebugFinalizeOutput(void);
  /// @brief get block number
  uInt32 getBlockNo(void) { return fOutputLoggerP ? fOutputLoggerP->getBlockNo() : fBlockNo; };
  /// @brief increment block number
  void nextBlock(void) { if (fOutputLoggerP) fOutputLoggerP->nextBlock(); else fBlockNo++; };
  /// @brief internal helper for closing debug Blocks
  /// @param aBlockName[in]   Name of Block to close. All Blocks including the first with given name will be closed. If NULL, all Blocks will be closed.
  /// @param aCloseComment[in]  Comment about closing Block. If NULL, no comment will be shown (unless implicit closes occur, which auto-creates a comment)
  void internalCloseBlocks(TDBG_LOCATION_PROTO cAppCharP aBlockName, cAppCharP aCloseComment);
  #ifdef SYDEBUG_LOCATION
  /// @brief turn text into link to source code
  string dbg2Link(const TDbgLocation &aTDbgLoc, const string &aTxt);
  #endif // SYDEBUG_LOCATION
  // Variables
  TDbgOut *fDbgOutP; // the debug output
  string fDbgPath; // the output path+filename (w/o extension)
  const TDbgOptions *fDbgOptionsP; // the debug options
  uInt32 fDebugMask; // the debug mask
  bool fDebugEnabled; // on-off-switch for debugging output
  uInt32 fNextDebugMask; // debug mask to be used for next DebugPrintfLastMask()
  uInt16 fIndent; // the current indent
  TBlockLevel *fBlockHistory; // the linked list of Block history entries
  bool fOutStarted; // set if output has started
  uInt32 fBlockNo; // block count for folding
  GZones *fGZonesP; // zones list for time conversions
  TDebugLoggerBase *fOutputLoggerP; // another logger to be used for output
}; // TDebugLoggerBase


#ifdef MULTI_THREAD_SUPPORT
/// @brief Subthread log handling
typedef struct SubThreadLog {
  uInt32 fThreadID;
  struct SubThreadLog *fNext;
  TDebugLoggerBase *fSubThreadLogger;
} TSubThreadLog;
#endif



/// @brief Debug logger class
class TDebugLogger : public TDebugLoggerBase {
  typedef TDebugLoggerBase inherited;
public:
  // constructor/destructor
  TDebugLogger(GZones *aGZonesP);
  virtual ~TDebugLogger();
  // methods
  #ifdef MULTI_THREAD_SUPPORT
  virtual void DebugPuts(TDBG_LOCATION_PROTO uInt32 aDbgMask, cAppCharP aText, stringSize aTextSize=0, bool aPreFormatted=false);
  virtual void DebugVPrintf(TDBG_LOCATION_PROTO uInt32 aDbgMask, cAppCharP aFormat, va_list aArgs);
  virtual void DebugVOpenBlock(TDBG_LOCATION_PROTO cAppCharP aBlockName, cAppCharP aBlockTitle, bool aCollapsed, cAppCharP aBlockFmt, va_list aArgs);
  virtual void DebugCloseBlock(TDBG_LOCATION_PROTO cAppCharP aBlockName);
  virtual TDebugLoggerBase &setNextMask(uInt32 aDbgMask);
  #endif
  // - thread debug output serializing
  /// @brief output all buffered subthread's output in a special subthread Block in the main output
  void DebugShowSubThreadOutput(void);
  /// @brief signals the calling thread that it is done with doing output for now.
  /// @param aRemoveIt[in] if set, do remove thread from the subthread logger list
  /// Notes:
  /// - If the main thread is doing this and we have bufferandmix mode, the next subthread will be allowed
  ///   to write into the output channel until a new main thread gains control via DebugDefineMainThread();
  void DebugThreadOutputDone(bool aRemoveIt=false);
  /// @brief Define the current calling thread as the main debug thread
  /// Note: This is used for example when starting to process the next request of a session which possibly
  //        occurs from another thread).
  void DebugDefineMainThread(void);
private:
  #ifdef MULTI_THREAD_SUPPORT
  // helpers
  /// @brief find (and possibly delete) subthread record
  /// @param aAndRemove[in] if set, the subthread record will be removed in a thread safe way
  ///        IF AND ONLY IF aThreadID is the calling thread (i.e. only own thread may be removed from the list)!
  ///        Note that the caller must take care of deleting the subthread record
  TSubThreadLog *findSubThread(uInt32 aThreadID, bool aAndRemove=false);
  /// @brief find or create logger for subthread
  TDebugLoggerBase *getThreadLogger(bool aCreateNew=true);
  // Variables
  uInt32 fMainThreadID;
  TSubThreadLog *fSubThreadLogs; // the linked list of active subthreads
  TDebugLoggerBase *fSilentLoggerP; // a silent (inactive) logger required for suppressed subthreads
  #endif
}; // TDebugLogger




} // namespace sysync

#endif // SYDEBUG

#endif // DEBUGLOGGER_H


// eof

