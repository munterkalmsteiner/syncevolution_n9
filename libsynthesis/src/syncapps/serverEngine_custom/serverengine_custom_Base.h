/*
 *  TCustomServerEngineBase, TCustomServerEngineInterface
 *    SyncML client engine for custom server - base classes
 *
 *  Copyright (c) 2009-2011 by Synthesis AG + plan44.ch
 *
 *  2009-02-06 : luz : Created
 *
 */
 
#ifndef SERVERENGINE_CUSTOM_BASE_H
#define SERVERENGINE_CUSTOM_BASE_H

// directly based on session dispatcher for engine servers
#include "enginesessiondispatch.h"
// using generic base for customizable builds
#include "customimplagent.h"


namespace sysync {

// Appbase class
class TCustomServerEngineBase: public TEngineSessionDispatch
{
  typedef TEngineSessionDispatch inherited;
public:
  TCustomServerEngineBase();
  virtual ~TCustomServerEngineBase();
}; // TCustomServerEngineBase


// Engine interface class
class TCustomServerEngineInterface:
  public TServerEngineInterface
{
  typedef TServerEngineInterface inherited;
public:
  /// @brief returns a new application base.
  virtual TSyncAppBase *newSyncAppBase(void);
}; // TCustomServerEngineInterface

}

#endif  // SERVERENGINE_CUSTOM_BASE_H

// eof
