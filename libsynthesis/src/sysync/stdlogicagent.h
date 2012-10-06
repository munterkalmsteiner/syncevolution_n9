/**
 *  @File     stdlogicagent.h
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TStdLogicAgent
 *    Agent (=server or client session) for standard database logic implementations, see @ref TStdLogicDS
 *
 *    Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2005-09-23 : luz : created from custdbagent
 */
/*
 */

#ifndef STDLOGICAGENT_H
#define STDLOGICAGENT_H

// includes
#include "sysync.h"
#include "syncagent.h"
#include "localengineds.h"


using namespace sysync;

namespace sysync {


class TStdLogicAgent:
  public TSyncAgent
{
  typedef TSyncAgent inherited;
public:
  TStdLogicAgent(TSyncAppBase *aAppBaseP, TSyncSessionHandle *aSessionHandleP, cAppCharP aSessionID);
  virtual ~TStdLogicAgent();
  virtual void TerminateSession(void); // Terminate session, like destructor, but without actually destructing object itself
  virtual void ResetSession(void); // Resets session (but unlike TerminateSession, session might be re-used)
  void InternalResetSession(void); // static implementation for calling through virtual destructor and virtual ResetSession();
  // user authentication
  #ifndef SYSYNC_CLIENT
  // - server-only build should implement it, so we make it abstract here again (altough there is
  //   an implementation for simpleauth in session.
  virtual bool SessionLogin(const char *aUserName, const char *aAuthString, TAuthSecretTypes aAuthStringType, const char *aDeviceID) = 0;
  #endif // not SYSYNC_CLIENT
}; // TStdLogicAgent


} // namespace sysync


#endif  // STDLOGICAGENT_H

// eof
