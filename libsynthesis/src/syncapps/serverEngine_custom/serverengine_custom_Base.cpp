/*
 *  TCustomServerEngineBase, TCustomServerEngineInterface
 *    SyncML client engine for custom server - base classes
 *
 *  Copyright (c) 2009-2011 by Synthesis AG + plan44.ch
 *
 *  2009-02-06 : luz : Created
 *
 */
 

// includes
#include "serverengine_custom.h"
#include "serverengine_custom_Base.h"

// common includes
#include "enginesessiondispatch.h"


namespace sysync {


// factory function implementation - declared in TEngineInterface
ENGINE_IF_CLASS *newServerEngine(void)
{
  return new TCustomServerEngineInterface;
} // newServerEngine



/*
 * Implementation of TCustomServerEngineInterface
 */


/// @brief returns a new application base.
TSyncAppBase *TCustomServerEngineInterface::newSyncAppBase(void)
{
  return new TCustomServerEngineBase;
} // TCustomServerEngineInterface::newSyncAppBase



/*
 * Implementation of TCustomServerEngineBase
 */


TCustomServerEngineBase::TCustomServerEngineBase()
{  
  // create config root
  fConfigP = new TEngineServerRootConfig(this);
} // TCustomServerEngineBase::TCustomServerEngineBase


TCustomServerEngineBase::~TCustomServerEngineBase()
{
  fDeleting=true; // flag deletion to block calling critical (virtual) methods
} // TCustomServerEngineBase::~TCustomServerEngineBase


} // namespace sysync

// eof
