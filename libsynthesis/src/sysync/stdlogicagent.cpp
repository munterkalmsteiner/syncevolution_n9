/**
 *  @File     stdlogicagent.cpp
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

// includes
#include "prefix_file.h"
#include "stdlogicagent.h"


/*
 * Implementation of TStdLogicAgent
 */

/* public TStdLogicAgent members */

TStdLogicAgent::TStdLogicAgent(TSyncAppBase *aAppBaseP, TSyncSessionHandle *aSessionHandleP, cAppCharP aSessionID) :
  inherited(aAppBaseP, aSessionHandleP, aSessionID)
{
  if (IS_SERVER) {
    #ifdef SYSYNC_SERVER
    InternalResetSession();
    #endif
  }
} // TStdLogicAgent::TStdLogicAgent


// destructor
TStdLogicAgent::~TStdLogicAgent()
{
  // make sure everything is terminated BEFORE destruction of hierarchy begins
  TerminateSession();
} // TStdLogicAgent::~TStdLogicAgent


// Terminate session
void TStdLogicAgent::TerminateSession()
{
  if (!fTerminated) {
    InternalResetSession();
  }
  inherited::TerminateSession();
} // TStdLogicAgent::TerminateSession



// Reset session
void TStdLogicAgent::InternalResetSession(void)
{
} // TStdLogicAgent::InternalResetSession


// Virtual version
void TStdLogicAgent::ResetSession(void)
{
  // do my own stuff
  InternalResetSession();
  // let ancestor do its stuff
  inherited::ResetSession();
} // TStdLogicAgent::ResetSession


/* end of TStdLogicAgent implementation */

// eof
