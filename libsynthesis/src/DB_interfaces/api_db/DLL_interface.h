/*
 *  File:    DLL_interface.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  General interface to access the routines
 *  of a DLL.
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 *
 */

#ifndef DLL_INTERFACE_H
#define DLL_INTERFACE_H

#include "target_options.h"
#include "platform_DLL.h"

#include <string>
using namespace std;

#include "generic_types.h"
#include "syerror.h"


#ifdef __cplusplus
  namespace sysync {
#endif


// base class for (internal) routine access
class TAccess
{
  public:
    TAccess() { fMod= NULL;
                fJNI= false;
                fLIB= false; }
                
    virtual ~TAccess() { }; // virtual destructor

    virtual bool Connect     ( cAppCharP  aModName, ErrReport,  void* ref= NULL )
    {
      fModName= aModName;
      ref= NULL; // dummy to avoid warning
      return true; 
    } // Connect

    virtual bool GetFunction ( cAppCharP aFuncName, cAppCharP,
                              appPointer    &aFunc, ErrMReport, void* ref= NULL )
    {
      if (aFuncName!=NULL) aFunc= (appPointer)aFuncName; // interpret it as address
      ref= NULL; // dummy to avoid warning
      return true;
    } // GetFunction

    virtual bool Disconnect () { return true; }

    bool   fJNI;
    bool   fLIB;

  protected:
    appPointer fMod;
    string     fModName;
}; // TAccess


#ifdef PLUGIN_DLL
//! derived class for DLL access
class TDLL : public TAccess
{
  public:
    virtual bool Connect    ( cAppCharP  aModName, ErrReport  aReport, void* ref= NULL );
    virtual bool GetFunction( cAppCharP aFuncName, cAppCharP  aParams,
                             appPointer    &aFunc, ErrMReport aReport, void* ref= NULL );
    virtual bool Disconnect ();
}; // TDLL
#endif // PLUGIN_DLL


// --------------------------------------------------------------------------
/*! General error reporting */
void Report_Error( cAppCharP aText, ... );

/*! Error output, if <aModName>  can't be found */
void ModuleConnectionError( void* /* ref */, cAppCharP aModName );

/*! Error output, if <aFuncName> can't be found */
void FuncConnectionError  ( void* /* ref */, cAppCharP aFuncName, cAppCharP aModName );


/*! Connects library/DLL/JNI <aModName>.
 *  <aMod> is the module reference pointer.
 */
TSyError ConnectModule( appPointer &aMod, cAppCharP aModName, bool is_jni= false );


/*! Connects a list of functions to module <aMod>.
 *  The functions will be filled into <aField> with <aFieldSize>. An error will be
 *  returned if the list of functions is too long or too short.
 *  The open parameter list will be interpreted - as function names for DLLs
 *               (or if module name is LIB/JNI) - as function pointers for libraries
 *  <aMod> is the module reference pointer.
 *  <aParamInfo> must be true, if each element contains a second param with parameter info.
 */
TSyError ConnectFunctions( appPointer aMod, appPointer aField, memSize aFieldSize,
                           bool aParamInfo, ... );


/*! Disconnect a connected unit
 *  If mode is a library, <aMod>=NULL can be passed
 */
TSyError DisconnectModule( appPointer &aMod );


#ifdef __cplusplus
  } // namespace
#endif

#endif /* DLL_INTERFACE_H */
/* eof */
