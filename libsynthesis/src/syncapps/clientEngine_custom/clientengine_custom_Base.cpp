/*
 *  TCustomClientEngineBase, TCustomClientEngineInterface
 *    SyncML client engine for custom clients - base classes
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 *  2007-09-04 : luz : Created
 *
 */


// includes
#include "clientengine_custom.h"
#include "clientengine_custom_Base.h"

// common includes
#include "engineclientbase.h"


namespace sysync {


// factory function implementation - declared in TEngineInterface
ENGINE_IF_CLASS *newClientEngine(void)
{
  return new TCustomClientEngineInterface;
} // newClientEngine



/*
 * Implementation of TCustomClientEngineInterface
 */


/// @brief returns a new application base.
TSyncAppBase *TCustomClientEngineInterface::newSyncAppBase(void)
{
  return new TCustomClientEngineBase;
} // TCustomClientEngineInterface::newSyncAppBase



/*
 * Implementation of TCustomClientEngineBase
 */


TCustomClientEngineBase::TCustomClientEngineBase()
{
  // create config root
  fConfigP=new TEngineClientRootConfig(this);
} // TCustomClientEngineBase::TCustomClientEngineBase


TCustomClientEngineBase::~TCustomClientEngineBase()
{
  fDeleting=true; // flag deletion to block calling critical (virtual) methods
} // TCustomClientEngineBase::~TCustomClientEngineBase


} // namespace sysync

// eof
