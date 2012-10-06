/*
 *  File:         SyncException.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TSyncException...
 *    SySync Exception classes
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-05-28 : luz : created
 *
 */

// includes
#include "prefix_file.h"
#include "sync_include.h"
#include "sysync.h"
#include "syncexception.h"



using namespace sysync;



TSyncException::TSyncException(const char *aMsg1, localstatus aLocalStatus) NOTHROW
{
  fMessage = aMsg1;
  fLocalStatus = aLocalStatus;
} // TSyncException::TSyncException


TSyncException::TSyncException(localstatus aLocalStatus) NOTHROW
{
  fLocalStatus = aLocalStatus;
  fMessage = "Exception due to local error status";
} // TSyncException::TSyncException


TSyncException::~TSyncException() NOTHROW
{
} // TSyncException::~TSyncException


const char *TSyncException::what() const  NOTHROW
{
  return fMessage.c_str();
} // TSyncException::getMessage


void TSyncException::setMsg(const char *p)
{
  fMessage=p;
} // TSyncException::setMsg




TSmlException::TSmlException(const char *aMsg, Ret_t aSmlError) NOTHROW
{
  const int msgsiz=256;
  char msg[msgsiz];

  fSmlError=aSmlError;
  msg[0]=0;
  snprintf(msg,msgsiz,"SyncML Toolkit error=0x%04hX, %s",(uInt16)fSmlError,aMsg);
  setMsg(msg);
} // TSmlException::TSmlException


// eof
