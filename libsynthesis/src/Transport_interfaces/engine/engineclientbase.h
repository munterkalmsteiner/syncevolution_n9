/*
 *  TEngineClientBase
 *    client library specific descendant of TSyncClientBase
 *    Global object, manages starting of client sessions.
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 *  2002-05-03 : luz : Created
 *
 */

#ifndef ENGINECLIENTBASE_H
#define ENGINECLIENTBASE_H

// required headers
#include "syncappbase.h"
#include "syncclientbase.h"
#include "syncagent.h"


namespace sysync {


// engine client root config
class TEngineClientRootConfig : public TRootConfig
{
  typedef TRootConfig inherited;
public:
  TEngineClientRootConfig(TSyncAppBase *aSyncAppBaseP) : inherited(aSyncAppBaseP) {};
  // factory methods
  virtual void installCommConfig(void);
  // Config parsing
  #ifndef HARDCODED_CONFIG
  virtual bool parseCommConfig(const char **aAttributes, sInt32 aLine);
  #endif
}; // TEngineClientRootConfig


// engine client transport config
class TEngineClientCommConfig: public TCommConfig
{
  typedef TCommConfig inherited;
public:
  TEngineClientCommConfig(TConfigElement *aParentElementP);
  virtual ~TEngineClientCommConfig();
protected:
  // check config elements
  #ifndef HARDCODED_CONFIG
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  virtual void clear();
  virtual void localResolve(bool aLastPass);
}; // TXPTCommConfig


// forward declarations
class TSyncSession;
class TEngineClientBase;



// AppBase class for all client engines (libararies with API to build custom clients)
class TEngineClientBase : public TSyncClientBase {
  typedef TSyncClientBase inherited;
public:
  // constructors/destructors
  TEngineClientBase();
  virtual ~TEngineClientBase();
private:
}; // TEngineClientBase

} // namespace sysync

#endif // ENGINECLIENTBASE_H


// eof
