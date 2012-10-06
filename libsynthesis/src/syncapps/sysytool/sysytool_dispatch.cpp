/**
 *  @File     sysytool_dispatch.cpp
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


// includes
#include "sysytool.h"
#include "sysytool_dispatch.h"

// common includes
#include "syncappbase.h"

#ifdef XML2GO_SUPPORT
  #include "curl.h"
#endif


namespace sysync {

// dispatcher creation function
TXPTSessionDispatch *newXPTSessionDispatch(void)
{
  // create new app-sepcific dispatcher
  MP_RETURN_NEW(TSySyncToolDispatch,DBG_OBJINST,"TSySyncToolDispatch",TSySyncToolDispatch());
} // newXPTSessionDispatch



/*
 * Implementation of TSySyncToolDispatch
 */

/* public TSySyncToolDispatch members */


TSySyncToolDispatch::TSySyncToolDispatch()
{
  // create config root
  fConfigP=new TXPTServerRootConfig(this);
  #ifdef XML2GO_SUPPORT
  // xml2go needs cURL
  curl_global_init(CURL_GLOBAL_ALL);
  #endif
} // TSySyncToolDispatch::TSySyncToolDispatch


TSySyncToolDispatch::~TSySyncToolDispatch()
{
  fDeleting=true; // flag deletion to block calling critical (virtual) methods
  #ifdef XML2GO_SUPPORT
  // xml2go needs cURL
  curl_global_cleanup();
  #endif
} // TSySyncToolDispatch::TSySyncToolDispatch


} // namespace sysync

/* end of TSySyncToolDispatch implementation */

// eof
