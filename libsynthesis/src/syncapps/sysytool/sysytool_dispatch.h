/**
 *  @File     sysytool_dispatch.h
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TSySyncToolDispatch
 *    Pseudo "Dispatcher" for SySyTool (debugging aid tool operating on a server session)
 *
 *    Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-12-07 : luz : created from old version
 */

#ifndef SYSYTOOL_DISPATCH_H
#define SYSYTOOL_DISPATCH_H

// required headers
// - based on XPT transport session dispatcher
#include "xptsessiondispatch.h"


namespace sysync {


class TSySyncToolDispatch: public TXPTSessionDispatch
{
  typedef TXPTSessionDispatch inherited;
public:
  TSySyncToolDispatch();
  virtual ~TSySyncToolDispatch();
protected:
  // return output channel handle for debug
  virtual TDbgOut *newDbgOutputter(bool aGlobal) { return new TConsoleDbgOut; };
}; // TSySyncToolDispatch


} // namespace sysync

#endif	// SYSYTOOL_DISPATCH_H

// eof
