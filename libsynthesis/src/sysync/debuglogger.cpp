/*
 *  File:         debuglogger.cpp
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


#include "prefix_file.h"

#ifdef SYDEBUG

#include "debuglogger.h"


#ifdef MULTI_THREAD_SUPPORT
#include "platform_thread.h"
#endif

namespace sysync {

#ifndef HARDCODED_CONFIG

// debug format modes
cAppCharP const DbgOutFormatNames[numDbgOutFormats] = {
  "text",       // plain text format (but can be indented)
  "xml",        // XML format
  "html"        // HTML format
};


// HTML dynamic folding modes
cAppCharP const DbgFoldingModeNames[numDbgFoldingModes] = {
  "none",       // do not include dynamic folding into HTML logs
  "collapsed",  // include folding - all collapsed by default
  "expanded",   // include folding - all expanded by default
  "auto"        // include folding - collapse/expand state predefined on a block-by-block basis
};


cAppCharP const DbgSourceModeNames[numDbgSourceModes] = {
  "none",       // do not include links into source code in HTML logs
  "hint",       // no links, but info about what file/line number the message comes from
  "doxygen",    // include link into doxygen prepared HTML version of source code
  "txmt",       // include txmt:// link (understood by TextMate and BBEdit) into source code
};


// debug flush modes
cAppCharP const DbgFlushModeNames[numDbgFlushModes] = {
  "buffered",   // no flush, keep open as long as possible, output buffered (fast, needed for network drives)
  "flush",      // flush every debug message
  "openclose"   // open and close debug channel separately for every message (as in 2.x engine)
};

// debug subthread isolation modes
cAppCharP const DbgSubthreadModeNames[numDbgSubthreadModes] = {
  "none",       // do not handle output from subthread specially
  "suppress",   // suppress output from subthreads
  "separate",   // create separate output stream (=file) for each subthread
  "mix",        // mix on a line by line basis
  "mixblocks"   // buffer thread's output and mix block-wise it into main stream when appropriate
};

#endif

// file extentsions for debug format modes
cAppCharP const DbgOutFormatExtensions[numDbgOutFormats] = {
  ".log",        // plain text format (but can be indented)
  ".xml",        // XML format
  ".html"        // HTML format
};


cAppCharP const DbgOutDefaultPrefixes[numDbgOutFormats] = {
  "*** Start of log",
  "<?xml version=\"1.0\"?>\n"
    "<sysync_log version=\"1.0\">",
  "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n"
  "<html><head><title>Synthesis SyncML Engine " SYSYNC_FULL_VERSION_STRING " Log</title>\n"
    "<meta http-equiv=\"content-type\" content=\"text/html;charset=UTF-8\">\n"
    "<style type=\"text/css\" media=\"screen\"><!--\n"
      ".block { color: #0000FF; font-weight: bold; }\n"
      ".attribute { color: #A5002C; }\n"
      ".attrval { color: #D80039; font-weight: bold; }\n"
      ".error { color: red; font-weight: bold; }\n"
      ".hotalone { color: #000000; font-weight: bold; }\n"
      ".hot { font-weight: bold; }\n"
      ".script { color: #996633; }\n" // brownish
      ".source { color: #3333FF; font-family:courier,monospace; font-size: 90%; font-weight: bold; }\n" // keyword blue
      ".comment { color: #669933; font-family:courier,monospace; font-size: 90%; font-weight: bold; }\n" // comment green
      ".skipped { color: #BBBBBB; font-family:courier,monospace; font-size: 90%; font-weight: bold; }\n" // skipped code
      ".value { color: #FF3300; }\n" // bright orange
      ".filter { color: #997F66; }\n" // brownish pale
      ".match { color: #A95E38; }\n" // brownish orange
      ".dbapi { color: #CC3366; }\n" // dark reddish/pink (pink/violet = database)
      ".plugin { color: #9151A3; }\n" // dark violet (pink/violet = database)
      ".incoming { color: #196D00; }\n" // really dark green (green = remote)
      ".outgoing { color: #002C84; }\n" // really dark blue (blue = local)
      ".conflict { color: #990000; }\n" // dark red
      ".remote { color: #709900; }\n" // greenish (green = remote)
      ".proto { color: #777100; }\n" // dark yellowish/brown
      ".rest { color: #AAAAAA; }\n" // greyed
      ".exotic { color: #FF9900; }\n" // mango
      "a.jump { color: #5D82BA; }\n"
      "pre { font-size: 90%; }\n"
      // for folding (always included, as it must be in header)
      ".exp {\n"
      " color: #FF0000;\n"
      " font-weight: bold;\n"
      " font-size: 90%;\n"
      " width: 1em;\n"
      " height: 1em;\n"
      " display: inline;\n"
      " border-width: 0.2em;\n"
      " border-style: solid;\n"
      " text-align: center;\n"
      " vertical-align: middle;\n"
      " padding: 0px 0.2em 0px 0.2em;\n"
      " margin: 0 4px 2px 0;\n"
      "}\n"
      ".coll {\n"
      " color: #754242;\n"
      " font-weight: bold;\n"
      " font-size: 90%;\n"
      " width: 1em;\n"
      " height: 1em;\n"
      " display: inline;\n"
      " border-width: 0.2em;\n"
      " border-style: solid;\n"
      " text-align: center;\n"
      " vertical-align: middle;\n"
      " padding: 0px 0.2em 0px 0.2em;\n"
      " margin: 0 4px 2px 0;\n"
      "}\n"
      ".doall { color: #754242; }\n"
    "--></style>\n"
    "</head><body><h2>Start of log - Synthesis SyncML Engine " SYSYNC_FULL_VERSION_STRING "</h2>\n<ul>\n"
};

cAppCharP const DbgOutDefaultSuffixes[numDbgOutFormats] = {
  "*** End of log",
  "</sysync_log>",
  "</ul><h2>End of log</h2></html>"
};


cAppCharP FoldingPrefix =
  "<script language=javascript1.2 type=text/javascript><!--\n"
  "function div_ref_style (id) {\n"
  " if      (document.layers)         return document.layers[id];\n"
  " else if (document.all)            return document.all[id].style;\n"
  " else if (document.getElementById) return document.getElementById(id).style;\n"
  " else                              return null;\n"
  "}\n"
  "function exp(id) {\n"
  " if(div_ref_style('B'+id).display!='block') {\n"
  "  div_ref_style('B'+id).display='block';\n"
  "  div_ref_style('E'+id).display='none';\n"
  "  div_ref_style('C'+id).display='inline';\n"
  " }\n"
  "}\n"
  "function coll(id) {\n"
  " if(div_ref_style('B'+id).display!='none') {\n"
  "  div_ref_style('B'+id).display='none';\n"
  "  div_ref_style('E'+id).display='inline';\n"
  "  div_ref_style('C'+id).display='none';\n"
  " }\n"
  "}\n"
  "function doall(id,collapse) {\n"
  " // get parent element\n"
  " if (id=='') {\n"
  "  mydiv=document;\n"
  " }\n"
  " else {\n"
  "  mydiv=document.getElementById('B'+id); // get div to collapse or expand\n"
  "  if (collapse) {\n"
  "   coll(id);\n"
  "  }\n"
  "  else {\n"
  "   exp(id);\n"
  "  }\n"
  " }\n"
  " // get all contained blocks\n"
  " divs=mydiv.getElementsByTagName('div')  // all divs\n"
  " for (i=0 ; i<divs.length ; i++) {\n"
  "  if (divs[i].className=='blk') {\n"
  "   // this is a foldable block div\n"
  "   bid = divs[i].id.substring(1);\n"
  "   if (collapse) {\n"
  "    coll(bid);\n"
  "   }\n"
  "   else {\n"
  "    exp(bid);\n"
  "   }\n"
  "  }\n"
  " }\n"
  "}\n"
  "--></script>\n"
  "<li><span class=\"doall\" onclick=\"doall('',true)\">[-- collapse all --]</span><span class=\"doall\" onclick=\"doall('',false)\">[++ expand all ++]</span></li>\n";


// externals

#ifdef CONSOLEINFO
// privately redefined here to avoid circular headers (would need syncappbase.h)
extern "C" void ConsolePuts(const char *text);
#endif

// TDbgOptions implementation
// --------------------------

TDbgOptions::TDbgOptions()
{
  // set defaults
  clear();
} // TDbgOptions::TDbgOptions


void TDbgOptions::clear(void)
{
  fOutputFormat = dbgfmt_html; // most universally readable and convenient
  fIndentString = "  "; // two spaces
  fCustomPrefix.erase(); // no custom prefix
  fCustomSuffix.erase(); // no custom suffix
  fSeparateMsgs = true; // separate text message lines (<msg></msg> in xml)
  fTimestampStructure = true; // timestamps in structure...
  fTimestampForAll = false; // ..but not for every line
  fThreadIDForAll = false; // not by default
  fFlushMode = dbgflush_none; // no special flush or openclose (fast, but might loose info on process abort)
  fFoldingMode = dbgfold_auto; // dynamic folding enabled, expanded/collapsed defaults automatically set on block-by-block basis
  #ifdef SYDEBUG_LOCATION
  fSourceLinkMode = dbgsource_none; // no links into source code
  fSourceRootPath = SYDEBUG_LOCATION; // use default path from build
  #endif
  fAppend = false; // default to overwrite existing logfiles
  fSubThreadMode = dbgsubthread_suppress; // simply suppress subthread info
  fSubThreadBufferMax = 1024*1024; // don't buffer more than one meg.
} // TDbgOptions::clear



// TDbgOut implementation
// ----------------------

TDbgOut::TDbgOut() :
  fDestructed(false)
{
  // init
  fIsOpen=false;
} // TDbgOut::TDbgOut


TDbgOut::~TDbgOut()
{
  destruct();
} // TDbgOut::~TDbgOut


void TDbgOut::destruct(void)
{
  if (!fDestructed) doDestruct();
  fDestructed=true;
} // TDbgOut::destruct


void TDbgOut::doDestruct(void)
{
  // make sure files are closed
  closeDbg();
} // TDbgOut::doDestruct


// TStdFileDbgOut implementation
// -----------------------------

#ifndef NO_C_FILES

TStdFileDbgOut::TStdFileDbgOut()
{
  // init
  fFileName.erase();
  fFile=NULL;
  mutex=newMutex();
} // TStdFileDbgOut::TStdFileDbgOut


TStdFileDbgOut::~TStdFileDbgOut()
{
  destruct();
  freeMutex(mutex);
} // TStdFileDbgOut::~TStdFileDbgOut


// open standard C file based debug output channel
bool TStdFileDbgOut::openDbg(cAppCharP aDbgOutputName, cAppCharP aSuggestedExtension, TDbgFlushModes aFlushMode, bool aOverWrite, bool aRawMode)
{
  if (fIsOpen) {
    // first close
    closeDbg();
  }
  // now apply new flush mode
  fFlushMode=aFlushMode;
  // save new file name
  fFileName=aDbgOutputName;
  // for C files, use the extension provided
  fFileName+=aSuggestedExtension;
  // open
  fFile=fopen(fFileName.c_str(),aRawMode ? (aOverWrite ? "wb" : "ab") : (aOverWrite ? "w" : "a"));
  // in case this fails, we'll have a NULL fFile. We can't do anything more here
  fIsOpen=fFile!=NULL;
  // For openclose mode, we have opened here only to check for logfile writability - close again
  if (fIsOpen && fFlushMode==dbgflush_openclose) {
    fclose(fFile);
    fFile=NULL;
  }
  // return false if we haven't been successful opening the channel
  return fIsOpen;
} // TStdFileDbgOut::openDbg


// return current size of debug file
uInt32 TStdFileDbgOut::dbgFileSize(void)
{
  if (!fIsOpen) return 0; // no file, no size
  uInt32 sz;
  if (fFlushMode==dbgflush_openclose) {
    // we need to open the file for append first
    fFile=fopen(fFileName.c_str(),"a");
    fseek(fFile,0,SEEK_END); // move to end (needed, otherwise ftell may return 0 despite "a" fopen mode)
    sz=ftell(fFile);
    fclose(fFile);
    fFile=NULL;
  }
  else {
    fseek(fFile,0,SEEK_END); // move to end (needed, otherwise ftell may return 0 despite "a" fopen mode)
    sz=ftell(fFile); // return size
  }
  return sz;
} // TStdFileDbgOut::dbgFileSize


// close standard C file based debug channel
void TStdFileDbgOut::closeDbg(void)
{
  if (fIsOpen) {
    if (fFile) {
      fclose(fFile);
      fFile=NULL;
    }
    fIsOpen=false;
  }
} // TStdFileDbgOut::closeDbg


// write single line to standard file based output channel
void TStdFileDbgOut::putLine(cAppCharP aLine, bool aForceFlush)
{
  // if not open, just NOP
  if (fIsOpen) {
    if (fFlushMode==dbgflush_openclose) {
      // we need to open the file for append first
      lockMutex(mutex);
      fFile=fopen(fFileName.c_str(),"a");
      if (!fFile)
        unlockMutex(mutex);
    }
    if (fFile) {
      // now output
      fputs(aLine,fFile);
      fputs("\n",fFile);

      // do required flushing
      if (fFlushMode==dbgflush_openclose) {
        // we need to close the file after every line of output
        fclose(fFile);
        fFile=NULL;
        unlockMutex(mutex);
      }
      else if (aForceFlush || fFlushMode==dbgflush_flush) {
        // simply flush
        fflush(fFile);
      }
    }
  }
} // TStdFileDbgOut::putLine


// write raw data to output file
void TStdFileDbgOut::putRawData(cAppPointer aData, memSize aSize)
{
  if (fIsOpen) {
    if (fFlushMode==dbgflush_openclose) {
      // we need to open the file for append first
      lockMutex(mutex);
      fFile=fopen(fFileName.c_str(),"a");
    }
    if (fFile) {
      if (fwrite(aData, 1, aSize, fFile) != 1) {
        // error ignored
      }
    }
    // do required flushing
    if (fFlushMode==dbgflush_openclose) {
      // we need to close the file after every line of output
      fclose(fFile);
      fFile=NULL;
      unlockMutex(mutex);
    }
    else if (fFlushMode==dbgflush_flush) {
      // simply flush
      fflush(fFile);
    }
  }
} // TStdFileDbgOut::putRawData


#endif


// TConsoleDbgOut implementation
// -----------------------------

TConsoleDbgOut::TConsoleDbgOut()
{
  // init
} // TStdFileDbgOut::TStdFileDbgOut


// open standard C file based debug output channel
bool TConsoleDbgOut::openDbg(cAppCharP aDbgOutputName, cAppCharP aSuggestedExtension, TDbgFlushModes aFlushMode, bool aOverWrite, bool aRawMode)
{
  if (fIsOpen) {
    // first close
    closeDbg();
  }
  // raw mode is not supported
  if (!aRawMode)
    fIsOpen=true;
  // return false if we haven't been successful opening the channel
  return fIsOpen;
} // TConsoleDbgOut::openDbg


// close standard C file based debug channel
void TConsoleDbgOut::closeDbg(void)
{
  fIsOpen=false;
} // TConsoleDbgOut::closeDbg


// write single line to standard file based output channel
void TConsoleDbgOut::putLine(cAppCharP aLine, bool aForceFlush)
{
  // if not open, just NOP
  if (fIsOpen) {
    CONSOLEPUTS(aLine);
  }
} // TConsoleDbgOut::putLine




// TDebugLoggerBase implementation
// -------------------------------

// constructor
TDebugLoggerBase::TDebugLoggerBase(GZones *aGZonesP) :
  fGZonesP(aGZonesP)
{
  fDebugMask=0;
  fDebugEnabled=true; // enabled by default
  fNextDebugMask=0;
  fDbgOutP=NULL;
  fDbgOptionsP=NULL;
  fIndent=0;
  fBlockHistory=NULL; // no Block open yet
  fOutStarted=false; // not yet started
  fOutputLoggerP=NULL; // no redirected output yet
} // TDebugLoggerBase::TDebugLoggerBase


// destructor
TDebugLoggerBase::~TDebugLoggerBase()
{
  // make sure debug is finalized
  DebugFinalizeOutput();
  // make sure possibly left-over history elements are erased
  while (fBlockHistory) {
    TBlockLevel *bl=fBlockHistory;
    fBlockHistory=bl->fNext;
    delete bl;
  }
  // get rid of output object
  if (fDbgOutP) delete fDbgOutP;
  fDbgOutP=NULL;
} // TDebugLoggerBase::TDebugLoggerBase


// @brief convenience version for getting time
lineartime_t TDebugLoggerBase::getSystemNowAs(timecontext_t aContext)
{
  return sysync::getSystemNowAs(aContext,fGZonesP);
} // TDebugLoggerBase::getSystemNowAs


// install outputter
void TDebugLoggerBase::installOutput(TDbgOut *aDbgOutP)
{
  // get rid of possibly installed previous outputter
  if (fDbgOutP) delete fDbgOutP;
  fDbgOutP=aDbgOutP;
} // TDebugLoggerBase::installOutput


/// @brief link this logger to another logger and redirect output to that logger
/// @param aDebugLoggerP[in] another logger, that must be alive as long as this logger is alive
void TDebugLoggerBase::outputVia(TDebugLoggerBase *aDebugLoggerP)
{
  // save logger and prefix
  fOutputLoggerP = aDebugLoggerP;
} // TDebugLoggerBase::outputVia

#if defined(CONSOLEINFO) && defined(CONSOLEINFO_LIBC)
extern "C" {
  int (*SySync_ConsolePrintf)(FILE *stream, const char *format, ...) = fprintf;
}
#endif

// output formatted text
void TDebugLoggerBase::DebugVPrintf(TDBG_LOCATION_PROTO uInt32 aDbgMask, cAppCharP aFormat, va_list aArgs)
{
  // we need a format and debug not completely off
  if ((getMask() & aDbgMask)==aDbgMask && aFormat) {
    const sInt16 maxmsglen=1024;
    char msg[maxmsglen];
    msg[0]='\0';
    // assemble the message string
    vsnprintf(msg, maxmsglen, aFormat, aArgs);
    // write the string
    DebugPuts(TDBG_LOCATION_ARG aDbgMask,msg);
  }
} // TDebugLoggerBase::DebugVPrintf


// helper needed for maintaining old DEBUGPRINTFX() macro syntax
TDebugLoggerBase &TDebugLoggerBase::setNextMask(uInt32 aDbgMask)
{
  fNextDebugMask=aDbgMask;
  return *this;
} // TDebugLoggerBase::setNextMask


// like DebugPrintf(), but using mask previously set by setNextMask()
void TDebugLoggerBase::DebugPrintfLastMask(TDBG_LOCATION_PROTO cAppCharP aFormat, ...)
{
  va_list args;
  // we need a format and debug not completely off
  if ((getMask() & fNextDebugMask)==fNextDebugMask && aFormat) {
    va_start(args, aFormat);
    DebugVPrintf(TDBG_LOCATION_ARG fNextDebugMask,aFormat,args);
    va_end(args);
  }
  fNextDebugMask=0;
} // TDebugLoggerBase::DebugPrintfLastMask


// output formatted text
void TDebugLoggerBase::DebugPrintf(TDBG_LOCATION_PROTO uInt32 aDbgMask, cAppCharP aFormat, ...)
{
  va_list args;
  // we need a format and debug not completely off
  if ((getMask() & aDbgMask)==aDbgMask && aFormat) {
    va_start(args, aFormat);
    DebugVPrintf(TDBG_LOCATION_ARG aDbgMask,aFormat,args);
    va_end(args);
  }
} // TDebugLoggerBase::DebugVPrintf


// open new Block without attribute list
void TDebugLoggerBase::DebugOpenBlock(TDBG_LOCATION_PROTO cAppCharP aBlockName, cAppCharP aBlockTitle, bool aCollapsed)
{
  // we need a format and debug not completely off
  if (getMask() && aBlockName) {
#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wformat-security"
#endif
    DebugOpenBlock(TDBG_LOCATION_ARG aBlockName,aBlockTitle,aCollapsed,NULL);
#ifdef __clang__
    #pragma clang diagnostic pop
#endif
  }
} // TDebugLoggerBase::DebugOpenBlock


// open new Block with attribute list, printf style
void TDebugLoggerBase::DebugOpenBlock(TDBG_LOCATION_PROTO cAppCharP aBlockName, cAppCharP aBlockTitle, bool aCollapsed, cAppCharP aBlockFmt, ...)
{
  va_list args;
  // we need a format and debug not completely off
  if (getMask() && aBlockName) {
    va_start(args, aBlockFmt);
    DebugVOpenBlock(TDBG_LOCATION_ARG aBlockName,aBlockTitle,aCollapsed,aBlockFmt,args);
    va_end(args);
  }
} // TDebugLoggerBase::DebugOpenBlock


// open new Block with attribute list, printf style, expanded by default
void TDebugLoggerBase::DebugOpenBlockExpanded(TDBG_LOCATION_PROTO cAppCharP aBlockName, cAppCharP aBlockTitle, cAppCharP aBlockFmt, ...)
{
  va_list args;
  // we need a format and debug not completely off
  if (getMask() && aBlockName) {
    va_start(args, aBlockFmt);
    DebugVOpenBlock(TDBG_LOCATION_ARG aBlockName,aBlockTitle,false,aBlockFmt,args);
    va_end(args);
  }
} // TDebugLoggerBase::DebugOpenBlockExpanded


// open new Block with attribute list, printf style, collapsed by default
void TDebugLoggerBase::DebugOpenBlockCollapsed(TDBG_LOCATION_PROTO cAppCharP aBlockName, cAppCharP aBlockTitle, cAppCharP aBlockFmt, ...)
{
  va_list args;
  // we need a format and debug not completely off
  if (getMask() && aBlockName) {
    va_start(args, aBlockFmt);
    DebugVOpenBlock(TDBG_LOCATION_ARG aBlockName,aBlockTitle,true,aBlockFmt,args);
    va_end(args);
  }
} // TDebugLoggerBase::DebugOpenBlockCollapsed


#ifdef SYDEBUG_LOCATION

#define MAKEDBGLINK(txt) dbg2Link(TDBG_LOCATION_ARG txt)

/// turn text into link to source code
string TDebugLoggerBase::dbg2Link(const TDbgLocation &aTDbgLoc, const string &aTxt)
{
  if (!aTDbgLoc.fFile || !fDbgOptionsP || fDbgOptionsP->fSourceLinkMode==dbgsource_none || fDbgOptionsP->fOutputFormat!=dbgfmt_html)
    return aTxt; // disabled, non-html or no information to create source link
  // create link or hint to source code
  string line;

  switch(fDbgOptionsP->fSourceLinkMode) {
    case dbgsource_hint: {
      // only add name/line number/function as title hint (in a otherwise inactive link)
      line = "<a href=\"#\" title=";
      StringObjPrintf(line,"<a href=\"#\" title=\"%s:%d",aTDbgLoc.fFile,aTDbgLoc.fLine);
      StringObjAppendPrintf(line," in %s",aTDbgLoc.fFunction);
      line += '"';
      goto closelink;
    }
    case dbgsource_doxygen: {
      // create link into doxygen
      line = "<a href=\"";
      // replace path with path to Doxygen HTML pages,
      // mangle base name like Doxygen does
      line += fDbgOptionsP->fSourceRootPath;
      line += "/";
      string file = aTDbgLoc.fFile;
      size_t off = file.rfind('/');
      if (off != file.npos)
        file = file.substr(off + 1);
      for (off = 0; off < file.size(); off++) {
        switch(file[off]) {
        case '_':
          line+="__";
          break;
        case '.':
          line+="_8";
          break;
        default:
          line+=file[off];
          break;
        }
      }
      StringObjAppendPrintf(line,"-source.html#l%05d",aTDbgLoc.fLine);
      line+="\"";
      if (aTDbgLoc.fFunction) {
        line+=" title=\"";
        line+=aTDbgLoc.fFunction;
        line+="\"";
      }
      goto closelink;
    }
    case dbgsource_txmt: {
      // create txmt:// URL scheme link, which opens TextMate or BBEdit at the correct line in MacOS X
      line = "<a href=\"txmt://open/?url=file://";
      // - create path
      string path = fDbgOptionsP->fSourceRootPath;
      path += aTDbgLoc.fFile;
      // - add path CGI encoded
      line += encodeForCGI(path.c_str());
      // - add line number
      if (aTDbgLoc.fLine>0)
        StringObjAppendPrintf(line,"&line=%d",aTDbgLoc.fLine);
      line+="\"";
      if (aTDbgLoc.fFunction) {
        line+=" title=\"";
        line+=aTDbgLoc.fFunction;
        line+='"';
      }
    }
    closelink: {
      line+=">";
      line+=aTxt;
      line+="</a>";
      break;
    }
    default:
      line = aTxt;
  } // switch
  // return
  return line;
} // TDebugLoggerBase::dbg2Link

#else

#define MAKEDBGLINK(txt) (txt)

#endif // SYDEBUG_LOCATION


// output text to debug channel
void TDebugLoggerBase::DebugPuts(TDBG_LOCATION_PROTO uInt32 aDbgMask, cAppCharP aText, stringSize aTextSize, bool aPreFormatted)
{
  // we need a text and debug not completely off
  if (!((getMask() & aDbgMask)==aDbgMask && aText && fDbgOptionsP)) {
    // cannot output
    //#ifdef __MWERKS__
    //#warning "ugly hack"
    DebugPutLine(TDBG_LOCATION_NONE "<li><span class=\"error\">Warning: Dbg output system already half shut down (limited formatting)!</span></li><li>");
    if (aText) DebugPutLine(TDBG_LOCATION_NONE aText);
    DebugPutLine(TDBG_LOCATION_NONE "</li>");
    //#endif
  }
  else {
    // make sure output is started
    if (!fOutStarted) {
      // try starting output
      DebugStartOutput();
      // disable debugging in this logger if starting output failed
      // (prevents endless re-trying to open debug logs e.g. when log directory does not exist)
      if (!fOutStarted) {
        fDebugEnabled = false;
        return; // stop all efforts here
      }
    }
    // dissect into lines
    cAppCharP end=aTextSize ? aText+aTextSize : NULL;
    bool firstLine=true;
    // check for preformatted message
    bool pre=strnncmp(aText,"&pre;",5)==0;
    if (pre) aText+=5;
    pre = pre || aPreFormatted;
    // now process text
    while ((!end || aText<end) && *aText) {
      // search for line end or end of string
      cAppCharP p=aText;
      while ((!end || p<end) && *p && *p!='\n' && *p!='\r') p++;
      // output this line, properly formatted
      string line;
      line.erase();
      cAppCharP q,s;
      string ts;
      switch (fDbgOptionsP->fOutputFormat) {
        // HTML
        case dbgfmt_html:
          // prefix first line with <li>, second and further with <br/>
          if (firstLine) {
            line="<li>";
            // add timestamp if needed for every line
            if (
              fDbgOptionsP->fTimestampForAll
              || fDbgOptionsP->fThreadIDForAll
              #ifdef SYDEBUG_LOCATION
              || fDbgOptionsP->fSourceLinkMode!=dbgsource_none
              #endif
            ) {
              string prefix;
              prefix = "<i>[";
              #ifdef MULTI_THREAD_SUPPORT
              if (fDbgOptionsP->fThreadIDForAll) {
                StringObjAppendPrintf(prefix,"%09lu",myThreadID());
                if (fDbgOptionsP->fTimestampForAll) prefix += ", ";
              }
              #endif
              if (fDbgOptionsP->fTimestampForAll) {
                StringObjTimestamp(ts,getSystemNowAs(TCTX_SYSTEM));
                prefix += ts;
              }
              #ifdef SYDEBUG_LOCATION
              else if (!fDbgOptionsP->fThreadIDForAll) {
                // neither threadID nor timestamp, but source requested -> put small text here
                prefix += "src";
              }
              #endif
              prefix+="]</i>&nbsp;";
              // if we have links into source code, add it here
              line += MAKEDBGLINK(prefix);
            }
            // colorize some messages
            string cl="";
            // colors, not mixable, most relevant first
            if (aDbgMask & DBG_ERROR) {
              cl="error";
            }
            else if (aDbgMask & DBG_EXOTIC) {
              cl="exotic";
            }
            else if (aDbgMask & DBG_SCRIPTS) {
              cl="script";
            }
            else if (aDbgMask & DBG_PLUGIN) {
              cl="plugin";
            }
            else if (aDbgMask & DBG_DBAPI) {
              cl="dbapi";
            }
            else if (aDbgMask & DBG_CONFLICT) {
              cl="conflict";
            }
            else if (aDbgMask & DBG_MATCH) {
              cl="match";
            }
            else if (aDbgMask & DBG_REMOTEINFO) {
              cl="remote";
            }
            else if (aDbgMask & DBG_PROTO) {
              cl="proto";
            }
            else if (aDbgMask & DBG_FILTER) {
              cl="filter";
            }
            else if (aDbgMask & DBG_PARSE) {
              cl="incoming";
            }
            else if (aDbgMask & DBG_GEN) {
              cl="outgoing";
            }
            else if (aDbgMask & DBG_REST) {
              cl="rest";
            }
            // apply basic color style
            if (!cl.empty()) {
              line+="<span class=\""; line+=cl; line+="\">";
            }
            // aditional style modifiers that can be combined with colors
            if (aDbgMask & DBG_HOT) {
              if (cl.empty())
                line+="<span class=\"hotalone\">";
              else
                line+="<span class=\"hot\">";
            }
            // start preformatted if selected
            if (pre)
              line+="<pre>";
          }
          else {
            if (!pre) line="<br/>";
          }
          goto xmlize;
        // XML, just output and replace special chars as needed
        case dbgfmt_xml:
          if (firstLine) {
            #ifdef MULTI_THREAD_SUPPORT
            if (fDbgOptionsP->fThreadIDForAll) {
              line+="<thread>";
              StringObjAppendPrintf(line,"%09lu",myThreadID());
              line+="</thread>";
            }
            #endif
            if (fDbgOptionsP->fTimestampForAll) {
              StringObjTimestamp(ts,getSystemNowAs(TCTX_SYSTEM));
              line+="<time>";
              line+=ts;
              line+="</time>";
            }
            DebugPutLine(TDBG_LOCATION_NONE line.c_str(),line.size());
            line.erase();
          }
          if (fDbgOptionsP->fSeparateMsgs) {
            line+="<msg>";
          }
        xmlize:
          q=aText;
          s=q;
          while (q<p) {
            if (*q=='&') {
              if (strucmp(q,"&html;",6)==0) {
                if (q>s) line.append(s,q-s); // flush stuff scanned so far
                // everything until next &html; does not need or want escaping, copy it as is
                // - search next &html;
                s=q=q+6;
                while(*q && strucmp(q,"&html;",6)!=0) q++;
                // - append everything between if we are in HTML mode
                if (fDbgOptionsP->fOutputFormat==dbgfmt_html && q>s)
                  line.append(s,q-s);
                s=q=q+6;
              }
              else if (strucmp(q,"&sp;",4)==0) {
                if (q>s) line.append(s,q-s); // flush stuff scanned so far
                // non-breaking space in HTML, normal space otherwise
                if (fDbgOptionsP->fOutputFormat==dbgfmt_html)
                  line += "&nbsp;";
                else
                  line += ' ';
                s=q=q+4; // skip &sp;
              }
              else {
                if (q>s) line.append(s,q-s);
                line+="&amp;";
                s=++q;
              }
            }
            else if (*q=='<') {
              if (q>s) line.append(s,q-s);
              line+="&lt;";
              s=++q;
            }
            else if (*q=='>') {
              if (q>s) line.append(s,q-s);
              line+="&gt;";
              s=++q;
            }
            else {
              q++;
            }
          }
          if (q>s) line.append(s,q-s);
          if (fDbgOptionsP->fSeparateMsgs && fDbgOptionsP->fOutputFormat==dbgfmt_xml) {
            line+="</msg>";
          }
          break;
        // plain text
        default:
        case dbgfmt_text:
          q=aText;
          s=q;
          while (q<p) {
            if (*q=='&') {
              if (strucmp(q,"&html;",6)==0) {
                if (q>s) line.append(s,q-s);
                // everything until next &html; must be filtered out
                // - search next &html;
                s=q=q+6;
                while(*q && strucmp(q,"&html;",6)!=0) q++;
                s=q=q+6;
              }
              else if (strucmp(q,"&sp;",4)==0) {
                if (q>s) line.append(s,q-s);
                s=q=q+4;
                line += ' '; // convert to plain space
              }
              else
                q++;
            }
            else {
              q++;
            }
          }
          if (q>s) line.append(s,q-s);
          break;
      } // switch text output
      firstLine=false;
      // skip the lineend, if any
      while (((!end || p<end) && *p=='\n') || *p=='\r') p++;
      if (fDbgOptionsP->fOutputFormat==dbgfmt_html && ((end && p>=end) || *p==0)) {
        // end preformatted
        if (pre)
          line+="</pre>";
        // colors
        if (aDbgMask & (
          DBG_ERROR |
          DBG_SCRIPTS |
          DBG_REST |
          DBG_EXOTIC |
          DBG_DBAPI |
          DBG_PLUGIN |
          DBG_PARSE |
          DBG_GEN |
          DBG_CONFLICT |
          DBG_MATCH |
          DBG_REMOTEINFO |
          DBG_PROTO |
          DBG_FILTER
        )) {
          line+="</span>"; // end special style
        }
        // HOT modifier
        if (aDbgMask & (
          DBG_HOT
        )) {
          line+="</span>"; // end special style
        }
        line+="</li>"; // we need to close the list entry
      }
      DebugPutLine(TDBG_LOCATION_NONE line.c_str(),line.size(),pre);
      // next line, if any
      aText=p;
    } // loop until all text done
  }
} // TDebugLoggerBase::DebugPuts


// open new Block with attribute list, varargs passed
void TDebugLoggerBase::DebugVOpenBlock(TDBG_LOCATION_PROTO cAppCharP aBlockName, cAppCharP aBlockTitle, bool aCollapsed, cAppCharP aBlockFmt, va_list aArgs)
{
  if (!fDbgOptionsP)
    return;
  if (fDbgOptionsP->fFoldingMode==dbgfold_collapsed)
    aCollapsed=true;
  else if (fDbgOptionsP->fFoldingMode==dbgfold_expanded)
    aCollapsed=false;
  if (getMask() && aBlockName && fDbgOptionsP) {
    // make sure output is started
    if (!fOutStarted) DebugStartOutput();
    // create Block line on current indent level
    string bl;
    string ts;
    // - preamble, possibly with timestamp
    bool withTime = fDbgOptionsP->fTimestampStructure;
    if (withTime)
      StringObjTimestamp(ts,getSystemNowAs(TCTX_SYSTEM));
    switch (fDbgOptionsP->fOutputFormat) {
      // XML
      case dbgfmt_xml:
        bl="<"; bl+=aBlockName;
        if (withTime) {
          bl+=" time=\"" + ts + "\"";
        }
        if (aBlockTitle) {
          bl+=" title=\"";
          bl+=aBlockTitle;
          bl+="\"";
        }
        break;
      // HTML
      case dbgfmt_html:
        bl="<li><span class=\"block\">";
        if (fDbgOptionsP->fFoldingMode!=dbgfold_none) {
          StringObjAppendPrintf(bl,
            "<div id=\"E%ld\" style=\"display:%s\" class=\"exp\" onclick=\"exp('%ld')\">+</div><div id=\"C%ld\" style=\"display:%s\" class=\"coll\" onclick=\"coll('%ld')\">&ndash;</div>",
                                long(getBlockNo()), aCollapsed ? "inline" : "none", long(getBlockNo()),
                                long(getBlockNo()), aCollapsed ? "none" : "inline", long(getBlockNo())
          );
        }
        StringObjAppendPrintf(bl,"<a name=\"H%ld\">", long(getBlockNo()));
        if (withTime) {
          bl += MAKEDBGLINK(string("[") + ts + "] ");
        }
        #ifdef SYDEBUG_LOCATION
        else if (fDbgOptionsP->fSourceLinkMode!=dbgsource_none) {
          bl += MAKEDBGLINK(string("[src] "));
        }
        #endif
        bl+="'";
        bl+=aBlockName;
        bl+="'";
        if (aBlockTitle) {
          bl+=" - ";
          bl+=aBlockTitle;
        }
        bl+="</a></span><span class=\"attribute\">";
        break;
      // plain text
      default:
      case dbgfmt_text:
        bl.erase();
        if (!fDbgOptionsP->fTimestampForAll && withTime) { // avoid timestamp here if all lines get timestamped anyway
          bl+="[" + ts + "] ";
        }
        bl+=aBlockName;
        if (aBlockTitle) {
          bl+=" - ";
          bl+=aBlockTitle;
        }
        break;
    } // switch preamble
    // - attributes
    if (aBlockFmt) {
      // first expand all printf parameters
      string attrs;
      vStringObjPrintf(attrs,aBlockFmt,true,aArgs);
      // isolate |-separated attribute format strings
      cAppCharP q,r,s,p=attrs.c_str();
      while (*p) {
        // search for beginning of value
        q=p;
        while(*q && *q!='=' && *q!='|') q++;
        // search for end of value
        r=q;
        s=q; // in case we don't have a =
        if (*q=='=') {
          s=q+1;
          r=s;
          while (*r && *r!='|') r++;
        }
        // now: p=start of attrname, q=end of attrname
        //      s=start of value, r=end of value
        // output an attribute now
        if (q>p && r>s) {
          switch (fDbgOptionsP->fOutputFormat) {
            // XML
            case dbgfmt_xml:
              bl+=" ";
              bl.append(p,q-p);
              bl+="=\"";
              bl.append(s,r-s);
              bl+="\"";
              break;
            case dbgfmt_html:
              bl+=", ";
              bl.append(p,q-p);
              bl+="=<span class=\"attrval\">";
              bl.append(s,r-s);
              bl+="</span>";
              break;
            case dbgfmt_text:
            default:
              bl+=", ";
              bl.append(p,q-p);
              bl+="=";
              bl.append(s,r-s);
              break;
          } // switch attribute
        } // non-empty attribute
        // more attributes to come?
        if (*r=='|') r++; // skip separator
        p=r;
      } // while
    } // attributes present
    // - finalize Block
    switch (fDbgOptionsP->fOutputFormat) {
      // XML
      case dbgfmt_xml:
        bl+=">";
        break;
      // HTML
      case dbgfmt_html:
        bl+="</span>"; // end span for attributes
        if (fDbgOptionsP->fFoldingMode!=dbgfold_none) {
          StringObjAppendPrintf(bl,
            "&nbsp;<span class=\"doall\" onclick=\"doall('%ld',true)\">[--]</span><span class=\"doall\" onclick=\"doall('%ld',false)\">[++]</span>",
                                long(getBlockNo()), long(getBlockNo())
          );
        }
        // link to end of block
        StringObjAppendPrintf(bl,"&nbsp;<a class=\"jump\" href=\"#F%ld\">[->end]</a>", long(getBlockNo()));
        // link to start of enclosing block (if any)
        if (fBlockHistory) {
          StringObjAppendPrintf(bl,"&nbsp;<a class=\"jump\" href=\"#H%ld\">[->enclosing]</a>", long(fBlockHistory->fBlockNo));
        }
        // start div for content folding
        if (fDbgOptionsP->fFoldingMode!=dbgfold_none) {
          StringObjAppendPrintf(bl,
            "<div class=\"blk\" id=\"B%ld\" style=\"display:%s\">",
            long(getBlockNo()),
            aCollapsed ? "none" : "inline"
          );
        }
        bl+="<ul>"; // now start list for block's contents
        break;
      // plain text
      default:
      case dbgfmt_text:
        break;
    } // switch preamble
    // now output Block line (on current indent level)
    DebugPutLine(TDBG_LOCATION_NONE bl.c_str(), bl.size());
    // increase indent level (applies to all Block contents)
    fIndent++;
    // save Block on stack
    TBlockLevel *newLevel = new TBlockLevel;
    newLevel->fBlockName=aBlockName;
    newLevel->fNext=fBlockHistory;
    newLevel->fBlockNo=getBlockNo(); // save block number to reference block in collapse box at end of block
    nextBlock(); // increment block number
    fBlockHistory=newLevel; // insert new level at start of list
  }
} // TDebugLoggerBase::DebugVOpenBlock


// close named Block. If no name given, topmost Block will be closed
void TDebugLoggerBase::DebugCloseBlock(TDBG_LOCATION_PROTO cAppCharP aBlockName)
{
  if (fOutStarted && getMask() && fDbgOptionsP && fBlockHistory) {
    if (aBlockName==NULL) {
      #if SYDEBUG>1
      internalCloseBlocks(TDBG_LOCATION_ARG fBlockHistory->fBlockName.c_str(),"Block Nest Warning: Missing Block name at close");
      #else
      internalCloseBlocks(TDBG_LOCATION_ARG fBlockHistory->fBlockName.c_str(),NULL);
      #endif
    }
    else {
      internalCloseBlocks(TDBG_LOCATION_ARG aBlockName,NULL);
    }
  }
} // TDebugLoggerBase::DebugCloseBlock



// internal helper used to close all or some Blocks
void TDebugLoggerBase::internalCloseBlocks(TDBG_LOCATION_PROTO cAppCharP aBlockName, cAppCharP aCloseComment)
{
  if (!fDbgOptionsP) return; // security
  bool withTime = fDbgOptionsP->fTimestampStructure;
  string comment;
  #if SYDEBUG>1
  if (!fBlockHistory && aBlockName) {
    // no blocks open any more and not close-all-remaining call (log close...)
    DebugPrintf(TDBG_LOCATION_ARG DBG_EXOTIC+DBG_ERROR,"Block Nest Warning: Trying to close block '%s', but no block is open",aBlockName);
  }
  #endif
  while (fBlockHistory) {
    // prepare comment
    comment.erase();
    if (aCloseComment) {
      comment += " - ";
      comment += aCloseComment;
    }
    // check if closing top-of-stack Block now
    bool found=
      (aBlockName && strucmp(aBlockName,fBlockHistory->fBlockName.c_str())==0);
    if (!found && fBlockHistory->fNext==NULL) {
      // last Block always counts as "found"...
      found = true;
      #if SYDEBUG>1
      // ...but issue warning as name is not what we would have expected
      StringObjAppendPrintf(comment, " - Block Nest Warning: closing '%s', but expected '%s'",aBlockName ? aBlockName : "<unknown>", fBlockHistory->fBlockName.c_str());
      #endif
    }
    // now close topmost Block
    string ts,bl;
    // - get time if needed and possibly put it within indented block
    if (withTime) {
      StringObjTimestamp(ts,getSystemNowAs(TCTX_SYSTEM));
      // for XML, the time must be shown before the close tag on a separate line
      if (fDbgOptionsP->fOutputFormat == dbgfmt_xml) {
        StringObjPrintf(bl,"<endblock time=\"%s\"/>",ts.c_str());
        DebugPutLine(TDBG_LOCATION_NONE bl.c_str(), bl.size()); // still within block, indented
      }
    }
    // - now unindent
    if (fIndent>0) fIndent--;
    // - then create closing Block
    #if SYDEBUG>1
    if (!found) StringObjAppendPrintf(comment," - Block Nest Warning: implicitly closed (by explicitly closing '%s')",aBlockName ? aBlockName : "<unknown parent>");
    #endif
    switch (fDbgOptionsP->fOutputFormat) {
      // XML
      case dbgfmt_xml:
        bl="</";
        bl+=fBlockHistory->fBlockName;
        bl+=">";
        if (!comment.empty()) {
          bl+=" <!-- ";
          bl+=comment;
          bl+=" -->";
        }
        break;
      // HTML
      case dbgfmt_html:
        bl="</ul><span class=\"block\">"; // end of content list
        if (fDbgOptionsP->fFoldingMode!=dbgfold_none) {
          StringObjAppendPrintf(bl,
            "<span class=\"coll\" onclick=\"coll('%ld')\">&ndash;</span>",
            long(fBlockHistory->fBlockNo)
          );
        }
        StringObjAppendPrintf(bl,"<a name=\"F%ld\">",long(fBlockHistory->fBlockNo));
        if (withTime) {
          bl += MAKEDBGLINK(string("[") + ts + "] ");
        }
        bl += "End of '";
        bl+=fBlockHistory->fBlockName;
        bl+="'";
        bl+=comment;
        bl+="</a></span>";
        // link to top of block
        StringObjAppendPrintf(bl,"&nbsp;<a class=\"jump\" href=\"#H%ld\">[->top]</a>",long(fBlockHistory->fBlockNo));
        // link to end of enclosing block (if any)
        if (fBlockHistory->fNext) {
          StringObjAppendPrintf(bl,"&nbsp;<a class=\"jump\" href=\"#F%ld\">[->enclosing]</a>",long(fBlockHistory->fNext->fBlockNo));
        }
        if (fDbgOptionsP->fFoldingMode!=dbgfold_none) {
          bl+="</div>"; // end of folding division
        }
        bl+="</li>"; // end of list entry containing entire block
        break;
      // plain text
      default:
      case dbgfmt_text:
        bl.erase();
        if (!fDbgOptionsP->fTimestampForAll && withTime) { // avoid timestamp here if all lines get timestamped anyway
          bl+="[" + ts + "] ";
        }
        bl+="End of '";
        bl+=fBlockHistory->fBlockName;
        bl+="'";
        bl+=comment;
        break;
    } // switch Block close
    // - output closing Block line
    DebugPutLine(TDBG_LOCATION_NONE bl.c_str(), bl.size());
    // - remove Block level
    TBlockLevel *closedLevel = fBlockHistory;
    fBlockHistory = closedLevel->fNext;
    delete closedLevel;
    // if we have found the Block, exit here
    if (found) break;
  }
} // TDebugLoggerBase::internalCloseBlocks


// start debugging output if needed and sets fOutStarted
bool TDebugLoggerBase::DebugStartOutput(void)
{
  if (!fOutStarted) {
    if (fOutputLoggerP) {
      // using another logger, call it to start output
      fOutStarted = fOutputLoggerP->DebugStartOutput();
      if (fOutStarted) {
        // start with indent level of parent logger
        fIndent = fOutputLoggerP->fIndent;
        // note: we'll use the parent logger's block number...
        fBlockNo = 0; // ...but init to something just in case
      }
    }
    else if (fDbgOptionsP && fDbgOutP && !fDbgPath.empty()) {
      // try to open the debug channel (force to openclose if we have multiple threads mixed in one file)
      if (fDbgOutP->openDbg(
        fDbgPath.c_str(),
        DbgOutFormatExtensions[fDbgOptionsP->fOutputFormat],
        fDbgOptionsP->fSubThreadMode==dbgsubthread_linemix ? dbgflush_openclose : fDbgOptionsP->fFlushMode,
        !fDbgOptionsP->fAppend
      )) {
        // make sure we don't recurse when we produce some output
        fOutStarted = true;
        fIndent = 0; // reset to make sure
        // create a block number that is unique in the file, even if we append multiple times.
        // We assume that a block consumes at least 256 bytes, so size_of_file/256 always gets
        // an unused block ID within that file
        // 256 is a safe assumption because the "fold" button <divs> alone are around 250 bytes
        fBlockNo = 1 + (fDbgOutP->dbgFileSize()/256);
        // now create required prefix
        DebugPutLine(TDBG_LOCATION_NONE fDbgOptionsP->fCustomPrefix.empty() ? DbgOutDefaultPrefixes[fDbgOptionsP->fOutputFormat] : fDbgOptionsP->fCustomPrefix.c_str());
        // add folding javascript if needed
        if (fDbgOptionsP->fOutputFormat==dbgfmt_html && fDbgOptionsP->fFoldingMode!=dbgfold_none) {
          DebugPutLine(TDBG_LOCATION_NONE FoldingPrefix);
        }
      } // debug channel opened successfully
    } // use own debug channel
  } // environment ready to start output
  return fOutStarted;
} // TDebugLoggerBase::DebugStartOutput


// @brief finalize debugging output (close Blocks, close output channel)
void TDebugLoggerBase::DebugFinalizeOutput(void)
{
  if (fOutputLoggerP) {
    // just close my own blocks
    internalCloseBlocks(TDBG_LOCATION_NONE NULL,"closed because sub-log ends here");
  }
  if (fOutStarted && fDbgOptionsP && fDbgOutP) {
    // close all left-open open Blocks
    internalCloseBlocks(TDBG_LOCATION_NONE NULL,"closed because log ends here");
    // now finalize output
    // - special stuff before
    if (fDbgOptionsP->fOutputFormat == dbgfmt_xml)
      fIndent=0; // unindent to zero (document is not a real Block)
    // - then suffix
    DebugPutLine(TDBG_LOCATION_NONE fDbgOptionsP->fCustomSuffix.empty() ? DbgOutDefaultSuffixes[fDbgOptionsP->fOutputFormat] : fDbgOptionsP->fCustomSuffix.c_str());
    // now close the debug channel
    fDbgOutP->closeDbg();
  }
  // whatever happened, we are not started any more
  fOutStarted=false;
} // TDebugLoggerBase::DebugFinalizeOutput


// Output single line to debug channel (includes indenting and other prefixing, but no further formatting)
void TDebugLoggerBase::DebugPutLine(TDBG_LOCATION_PROTO cAppCharP aText, stringSize aTextSize, bool aPre)
{
  if (!aText || (!fDbgOutP && !fOutputLoggerP)) return;
  if (*aText) {
    // not an empty line
    string msg;
    msg.erase();
    // prefix with timestamp if selected in text format
    if (fDbgOptionsP && fDbgOptionsP->fOutputFormat==dbgfmt_text && fDbgOptionsP->fTimestampForAll) {
      // prefix each line (before the indent!) with a timestamp
      string ts;
      StringObjTimestamp(ts,getSystemNowAs(TCTX_SYSTEM));
      msg='[';
      msg+=ts;
      msg+="] ";
    }
    // Indent if selected
    if (fDbgOptionsP && !fDbgOptionsP->fIndentString.empty() && !(fDbgOptionsP->fOutputFormat==dbgfmt_html && aPre)) {
      // with indent
      for (uInt16 n=0; n<fIndent; n++) {
        msg+=fDbgOptionsP->fIndentString;
      }
    }
    // add message itself
    if (aTextSize)
      msg.append(aText,aTextSize);
    else
      msg.append(aText);
    // now output
    if (fOutputLoggerP) {
      // use parent's output
      fOutputLoggerP->fDbgOutP->putLine(msg.c_str(),false); // %%% no forceflush for now
    }
    else {
      // use my own output channel
      fDbgOutP->putLine(msg.c_str(),false); // %%% no forceflush for now
    }
  }
} // TDebugLoggerBase::DebugPutLine


// TDebugLogger implementation
// ---------------------------

// constructor
TDebugLogger::TDebugLogger(GZones *aGZonesP) :
  inherited(aGZonesP)
{
  #ifdef MULTI_THREAD_SUPPORT
  fMainThreadID=0;
  fSubThreadLogs=NULL;
  fSilentLoggerP=NULL;
  #endif
} // TDebugLogger::TDebugLogger


// destructor
TDebugLogger::~TDebugLogger()
{
  #ifdef MULTI_THREAD_SUPPORT
  // remove subthread loggers
  TSubThreadLog* subThreadP = fSubThreadLogs;
  fSubThreadLogs = NULL;
  while (subThreadP) {
    // delete logger if any
    if (subThreadP->fSubThreadLogger) {
      SYSYNC_TRY {
        delete subThreadP->fSubThreadLogger;
      }
      SYSYNC_CATCH(...)
        // nop
      SYSYNC_ENDCATCH
    }
    TSubThreadLog* delP = subThreadP;
    subThreadP = subThreadP->fNext;
    delete delP;
  }
  //
  if (fSilentLoggerP) {
    delete fSilentLoggerP;
    fSilentLoggerP = NULL;
  }
  #endif
} // TDebugLogger::~TDebugLogger


#ifdef MULTI_THREAD_SUPPORT

/// @brief find (and possibly delete) subthread record
/// @param aAndRemove[in] if set, the subthread record will be removed in a thread safe way
///        IF AND ONLY IF aThreadID is the calling thread (i.e. only own thread may be removed from the list)!
///        Note that the caller must take care of deleting the subthread record
TSubThreadLog *TDebugLogger::findSubThread(uInt32 aThreadID, bool aAndRemove)
{
  TSubThreadLog* subThreadP = fSubThreadLogs;
  TSubThreadLog** subThreadLinkPP = &fSubThreadLogs;
  while (subThreadP) {
    if (subThreadP->fThreadID == aThreadID) {
      if (aAndRemove) {
        // bridge previous with next in one single assignment (i.e. thread safe)
        *subThreadLinkPP = subThreadP->fNext;
      }
      // return found record (note that it MUST BE DELETED by caller if no longer used)
      return subThreadP;
    }
    subThreadLinkPP = &subThreadP->fNext;
    subThreadP = *subThreadLinkPP;
  }
  return NULL; // none found
} // TDebugLogger::findSubThread


/// @brief find or create logger for subthread
TDebugLoggerBase *TDebugLogger::getThreadLogger(bool aCreateNew)
{
  if (!fDbgOptionsP || fDbgOptionsP->fSubThreadMode==dbgsubthread_none)
    return this; // no options, do not handle subthreads specially
  uInt32 threadID = myThreadID();
  if (fDbgOptionsP->fSubThreadMode==dbgsubthread_linemix || threadID==fMainThreadID) {
    // In line mix and for mainthread - I am the logger for this thread!
    return this;
  }
  TSubThreadLog* subThreadP = findSubThread(threadID);
  if (subThreadP) {
    // we know this subthread, return its logger
    return subThreadP->fSubThreadLogger; // can be NULL if subthread logging is disabled
  }
  // unknown subthread
  if (fMainThreadID==0) {
    // no current mainthread, let subthread write to main log
    // Note: this makes sure log info possibly trailing the DebugThreadOutputDone()
    //       also lands in the main log. This is not critical - the only thing that must be
    //       ensured is that starting new threads is made only with DebugDefineMainThread set.
    return this;
  }
  // new subthread, create entry in list
  if (aCreateNew) {
    string s;
    // create new entry
    subThreadP = new TSubThreadLog;
    subThreadP->fThreadID=threadID;
    subThreadP->fNext=fSubThreadLogs; // link current list behind this new entry
    // create logger for the thread (or none)
    switch (fDbgOptionsP->fSubThreadMode) {
      case dbgsubthread_separate:
        // separate file for subthread output
        // - create new base logger
        subThreadP->fSubThreadLogger = new TDebugLoggerBase(fGZonesP);
        // - install output (copy)
        subThreadP->fSubThreadLogger->installOutput(fDbgOutP->clone());
        // - same options
        subThreadP->fSubThreadLogger->setOptions(getOptions());
        // - inherit current mask/enable
        subThreadP->fSubThreadLogger->setMask(getMask());
        subThreadP->fSubThreadLogger->setEnabled(fDebugEnabled);
        // - debug path is same as myself plus Thread ID
        subThreadP->fSubThreadLogger->setDebugPath(fDbgPath.c_str());
        StringObjPrintf(s,"_%lu",threadID);
        subThreadP->fSubThreadLogger->appendToDebugPath(s.c_str());
        break;
      case dbgsubthread_suppress:
      default:
        // no output from subthreads
        subThreadP->fSubThreadLogger=NULL; // no logger
        break;
    }
    // now activate by linking it at top of list (this is thread safe)
    fSubThreadLogs = subThreadP;
    // return the logger
    return subThreadP->fSubThreadLogger;
  }
  return NULL; // no logger for this thread
} // TDebugLogger::getThreadLogger


// helper needed for maintaining old DEBUGPRINTFX() macro syntax
TDebugLoggerBase &TDebugLogger::setNextMask(uInt32 aDbgMask)
{
  TDebugLoggerBase *loggerP = getThreadLogger();
  if (loggerP) {
    // return pointer to loggerbase whose DebugPrintfLastMask() must be called
    return loggerP->inherited::setNextMask(aDbgMask);
  }
  else {
    // we have no logger but still need to return something
    if (!fSilentLoggerP) {
      fSilentLoggerP = new TDebugLoggerBase(fGZonesP);
      fSilentLoggerP->setEnabled(false);
    }
    fSilentLoggerP->setNextMask(DBG_ERROR); // must set non-zero to make sure it is NOT output!
    return *fSilentLoggerP;
  }
} // TDebugLoggerBase::setNextMask


// output text to debug channel, with checking for subthreads
void TDebugLogger::DebugPuts(TDBG_LOCATION_PROTO uInt32 aDbgMask, cAppCharP aText, stringSize aTextSize, bool aPreFormatted)
{
  TDebugLoggerBase *loggerP = getThreadLogger();
  if (loggerP) loggerP->inherited::DebugPuts(TDBG_LOCATION_ARG aDbgMask,aText,aTextSize,aPreFormatted);
} // TDebugLogger::DebugPuts


void TDebugLogger::DebugVPrintf(TDBG_LOCATION_PROTO uInt32 aDbgMask, cAppCharP aFormat, va_list aArgs)
{
  TDebugLoggerBase *loggerP = getThreadLogger();
  if (loggerP) loggerP->inherited::DebugVPrintf(TDBG_LOCATION_ARG aDbgMask,aFormat,aArgs);
} // TDebugLogger::DebugVPrintf


void TDebugLogger::DebugVOpenBlock(TDBG_LOCATION_PROTO cAppCharP aBlockName, cAppCharP aBlockTitle, bool aCollapsed, cAppCharP aBlockFmt, va_list aArgs)
{
  TDebugLoggerBase *loggerP = getThreadLogger();
  if (loggerP) loggerP->inherited::DebugVOpenBlock(TDBG_LOCATION_ARG aBlockName, aBlockTitle, aCollapsed, aBlockFmt, aArgs);
} // TDebugLogger::DebugVOpenBlock


void TDebugLogger:: DebugCloseBlock(TDBG_LOCATION_PROTO cAppCharP aBlockName)
{
  TDebugLoggerBase *loggerP = getThreadLogger();
  if (loggerP) loggerP->inherited::DebugCloseBlock(TDBG_LOCATION_ARG aBlockName);
} // TDebugLogger::DebugCloseBlock

#endif


// output all buffered subthread's output in a special subthread Block in the main output
void TDebugLogger::DebugShowSubThreadOutput(void)
{
  #ifdef MULTI_THREAD_SUPPORT
  // nop as long mixed-block mode is not implemented
  #endif
} // TDebugLogger::DebugShowSubThreadOutput


// the calling thread signals that it is done with doing output for now. If the main
// thread is doing this and we have bufferandmix mode, the next subthread will be allowed
// to write into the output channel until a new main thread gains control via
// DebugDefineMainThread();
void TDebugLogger::DebugThreadOutputDone(bool aRemoveIt)
{
  #ifdef MULTI_THREAD_SUPPORT
  uInt32 threadID = myThreadID();
  if (threadID==fMainThreadID) {
    // current main thread done
    fMainThreadID = 0;
  }
  // for session logs, subthreads are usually left in the list at this time (aRemoveIt==false)
  // (as they will get deleted with the session logger later anyway)
  if (aRemoveIt) {
    TSubThreadLog* tP = findSubThread(threadID,true);
    if (tP) {
      if (tP->fSubThreadLogger) {
        SYSYNC_TRY {
          delete tP->fSubThreadLogger;
        }
        SYSYNC_CATCH(...)
          // nop
        SYSYNC_ENDCATCH
      }
      delete tP;
    }
  }
  #endif
} // TDebugLogger::DebugThreadOutputDone


// Used to regain control as main thread (e.g. for the next request of a session which
// possibly occurs from another thread).
void TDebugLogger::DebugDefineMainThread(void)
{
  #ifdef MULTI_THREAD_SUPPORT
  uInt32 threadID = myThreadID();
  // if this is already the main thread, no op
  if (threadID == fMainThreadID)
    return; // nop, done
  // thread is not the current main thread
  // - search if it is a registered subthread
  TSubThreadLog *subThreadP = findSubThread(threadID);
  if (fMainThreadID==0) {
    // no main thread currently registered
    if (subThreadP!=NULL) {
      // this is not a new thread, but a known subthread, can't get main thread now
      return; // no further op
    }
    else {
      // this is not a known subthread, so it can become the main thread
      fMainThreadID = threadID;
      return; // done
    }
  }
  else {
    // cannot become main thread, will be treated as subthread if it generates output
    // - no op required
  }
  #endif
} // TDebugLogger::DebugDefineMainThread


} // namespace sysync

#endif // SYDEBUG

// eof

