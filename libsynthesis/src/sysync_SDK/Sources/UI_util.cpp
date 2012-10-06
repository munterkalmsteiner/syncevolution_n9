/*
 *  File:         UI_util.cpp
 *
 *  Author:       Beat Forster
 *
 *  Programming interface between a user application
 *  and the Synthesis SyncML client engine.
 *
 *  Copyright (c) 2007-2011 by Synthesis AG + plan44.ch
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "prefix_file.h"
#include  "UI_util.h"
#include "SDK_util.h"
#include "SDK_support.h"

// ---- include DLL functionality ----
#if   defined _WIN32
  #include <windows.h>
  #define  DLL_Suffix ".dll"
#elif defined MACOSX
  #include <dlfcn.h>
  #define  DLL_Suffix ".dylib"
  #define  RFlags (RTLD_NOW + RTLD_GLOBAL)
#elif defined LINUX
  #include <dlfcn.h>
  #define  DLL_Suffix ".so"
  #define  RFlags  RTLD_LAZY
#else
  #define  DLL_Suffix ""
#endif
// -----------------------------------



namespace sysync {


static TSyError NotFnd( appPointer aRef, cAppCharP /* aName */ )
{
  TSyError err= LOCERR_OK;

  if (aRef==NULL) {
    err= DB_NotFound;
  //printf( "Not found: '%s' err=%d\n", aName, err );
  } // if

  return err;
} // NotFnd


// Connect to DLL
static TSyError ConnectDLL( cAppCharP aDLLname, appPointer &aDLL )
{
  #if   defined _WIN32
    aDLL= LoadLibrary( aDLLname );
  #elif defined MACOSX || defined LINUX
    aDLL=      dlopen( aDLLname, RFlags );
  //printf( "'%s' %s\n", aDLLname, dlerror() );
  #else
    aDLL= NULL;
  #endif

  return NotFnd( aDLL, aDLLname );
} // ConnectDLL



// Get <aFunc> of <aFuncName> at <aDLL>
static TSyError DLL_Func( appPointer aDLL, cAppCharP aFuncName, appPointer &aFunc )
{
  #if   defined _WIN32
    aFunc= (appPointer)GetProcAddress( (HINSTANCE)aDLL, aFuncName );
  #elif defined MACOSX || defined LINUX
    aFunc= dlsym( aDLL, aFuncName );
  #else
    aFunc= NULL;
  #endif

  return NotFnd( aFunc, aFuncName );
} // DLL_Func



static bool IsLib( cAppCharP name )
{
  int    len= strlen(name);
  return len==0 ||  (name[     0 ]=='[' &&
                     name[ len-1 ]==']'); // empty or embraced with "[" "]"
} // IsLib


// Connect SyncML engine
TSyError UI_Connect( UI_Call_In &aCI, appPointer &aDLL, bool &aIsServer,
                                                        cAppCharP aEngineName,
                                                        CVersion  aPrgVersion,
                                                        uInt16    aDebugFlags )
{
  // Always search for BOTH names, independently of environment
  cAppCharP SyFName= "SySync_ConnectEngine";
  cAppCharP   FName=        "ConnectEngine";
  string        name= aEngineName;
  TSyError       err= 0;
  bool           dbg= ( aDebugFlags & DBG_PLUGIN_DIRECT )!=0;

  CVersion   engVersion;
  appPointer fFunc;

  ConnectEngine_Func fConnectEngine= NULL;

  aIsServer = false;
  do {
    aCI = NULL; // no such structure available at the beginning
    aDLL= NULL;
    if (dbg) printf( "name='%s' err=%d\n", name.c_str(), err );

    if (name.empty()) {
       // not yet fully implemented: Take default settings
                         aCI= new SDK_Interface_Struct;
      InitCallback_Pure( aCI,      DB_Callback_Version );
                         aCI->debugFlags= aDebugFlags;
      break;
    } // if

    if (IsLib( name.c_str() )) {
      if (name == "[]") {
        #ifdef DBAPI_LINKED
          fConnectEngine= SYSYNC_EXTERNAL(ConnectEngine);
        #endif
      } else if (name == "[server:]") {
        aIsServer=true;
        #ifdef DBAPI_SRV_LINKED
          fConnectEngine= SySync_srv_ConnectEngine;
        #endif
      }

      break;
    } // if

    cAppCharP prefix = "server:";
    size_t prefixlen = strlen(prefix);
    if (name.size() > prefixlen &&
        !name.compare(0, prefixlen, prefix)) {
      // ignore prefix and if we find the lib, look for different entry points
      aIsServer=true;
      name = name.substr(prefixlen);
      SyFName= "SySync_srv_ConnectEngine";
      FName=        "srv_ConnectEngine";
    }

    err= ConnectDLL( name.c_str(), aDLL ); // try with name directly
    if (dbg) printf( "modu='%s' err=%d\n", name.c_str(), err );

    if (err) {
                         name+= DLL_Suffix;
        err= ConnectDLL( name.c_str(), aDLL ); // try with suffix next
    } // if

    if (dbg) printf( "modu='%s' err=%d\n", name.c_str(), err );
    if (err) break;

    cAppCharP              fN= SyFName;
    err=   DLL_Func( aDLL, fN,   fFunc );
    fConnectEngine=   (ConnectEngine_Func)fFunc;
    if   (dbg) printf( "func err=%d '%s' %s\n", err, fN, RefStr( (void*)fConnectEngine ).c_str() );

    if (!fConnectEngine) { fN=   FName;
      err= DLL_Func( aDLL, fN,   fFunc );
      fConnectEngine= (ConnectEngine_Func)fFunc;
      if (dbg) printf( "func err=%d '%s' %s\n", err, fN, RefStr( (void*)fConnectEngine ).c_str() );
   } // if

  } while (false);

  if    (fConnectEngine)
    err= fConnectEngine( &aCI, &engVersion, aPrgVersion, aDebugFlags );
  if (dbg) printf( "call err=%d\n", err );

//DEBUG_DB     ( aCI, MyMod, "ConnectEngine", "aCB=%08X eng=%08X prg=%08X aDebugFlags=%04X err=%d",
//                                             aCB, engVersion, aPrgVersion, aDebugFlags,  err );
  if    (fConnectEngine && err) return err;
  return NotFnd( aCI, "ConnectEngine" );
} // UI_Connect


TSyError UI_Disconnect( UI_Call_In aCI, appPointer aDLL, bool aIsServer )
{
  // Always search for BOTH names, independently of environment
  cAppCharP SyFName= "SySync_DisconnectEngine";
  cAppCharP   FName=        "DisconnectEngine";
  TSyError       err= 0;
  appPointer   fFunc;

  DisconnectEngine_Func fDisconnectEngine= NULL;

  do {
    if (aDLL==NULL) {
      if (aIsServer) {
        #ifdef DBAPI_LINKED
          fDisconnectEngine= SYSYNC_EXTERNAL(DisconnectEngine);
        #endif
      }
      else {
        #ifdef DBAPI_SRV_LINKED
          fDisconnectEngine= SySync_srv_DisconnectEngine;
        #endif
      }

      break;
    } // if

    if (aIsServer) {
      SyFName = "SySync_srv_DisconnectEngine";
      FName = "srv_DisconnectEngine";
    }

    cAppCharP                             fN= SyFName;
    err=                  DLL_Func( aDLL, fN,   fFunc );
    fDisconnectEngine=   (DisconnectEngine_Func)fFunc;

    if (!fDisconnectEngine)             { fN=   FName;
      err=                DLL_Func( aDLL, fN,   fFunc );
      fDisconnectEngine= (DisconnectEngine_Func)fFunc;
    } // if

  //printf( "func err=%d %08X\n", err, fConnectEngine );
  } while (false);

  if    (fDisconnectEngine)
    err= fDisconnectEngine( aCI );
//printf( "call err=%d\n", err );

//DEBUG_DB     ( aCI, MyMod, "ConnectEngine", "aCB=%08X eng=%08X prg=%08X aDebugFlags=%04X err=%d",
//                                             aCB, engVersion, aPrgVersion, aDebugFlags,  err );
  if (fDisconnectEngine && err) return err;
  return NotFnd( aCI, "DisconnectEngine" );
} // UI_Disconnect



} // namespace sysync
/* eof */
