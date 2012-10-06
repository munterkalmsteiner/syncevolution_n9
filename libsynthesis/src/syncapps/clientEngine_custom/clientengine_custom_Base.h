/*
 *  TCustomClientEngineBase, TCustomClientEngineInterface
 *    SyncML client engine for custom clients - base classes
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 *  2007-09-04 : luz : Created
 *
 *
 */

#ifndef CLIENTENGINE_CUSTOM_BASE_H
#define CLIENTENGINE_CUSTOM_BASE_H

// directly based on syncclient base
#include "engineclientbase.h"
// using binfile engine interface
#include "binfileimplclient.h"


namespace sysync {

// Appbase class
class TCustomClientEngineBase: public TEngineClientBase
{
  typedef TEngineClientBase inherited;
public:
  TCustomClientEngineBase();
  virtual ~TCustomClientEngineBase();
}; // TCustomClientEngineBase


// Engine interface class
class TCustomClientEngineInterface:
  public TBinfileEngineInterface
{
  typedef TBinfileEngineInterface inherited;
public:
  /// @brief returns a new application base.
  virtual TSyncAppBase *newSyncAppBase(void);
}; // TCustomClientEngineInterface

}

#endif  // CLIENTENGINE_CUSTOM_BASE_H

// eof
