/*
 *  File:    platform_DLL.cpp
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  General interface to access the routines
 *  of a DLL (LINUX platform)
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 *
 */
#define DLOPEN_NO_WARN 1
#define GNU_SOURCE 1
#include <dlfcn.h> // the Linux DLL functionality

#include "platform_DLL.h"
#include <stdint.h>
#include <string>
#include <map>
#include <stdio.h>

using namespace std;

/**
 * Instances of this class are returned
 * as DLL handles to the caller of ConnectDLL.
 *
 * Loadable modules are opened dlopen() and
 * functions are found via dlsym(). If the
 * platform supports RTLD_DEFAULT and finds
 * a <DLName>_Module_Version, then dlopen()
 * isn't used and all functions are searched
 * via RTLD_DEFAULT.
 */
class DLWrapper {
  /**
   * name of shared object, including suffix,
   * or function prefix (when functions were
   * linked in)
   *
   * As a special case, "//static/<symbol>=<address>/..." is supported,
   * with <symbol> being the name of a symbol at an address
   * specified as decimal value in <address>.
   */
  string aDLLName;
  /** dlopen() result, NULL if not opened */
  void *aDLL;
  /** result of parsing aDLLName when it contains the "static:" format. */
  map<string, void *> fSymbols;

public:
  DLWrapper(const char *name) :
    aDLLName(name),
    aDLL(NULL)
  {}

  /** check for linked function or open shared object,
      return true for success */
  bool connect()
  {
    string prefix = "//static/";
    if (aDLLName.substr(0, prefix.size()) == prefix) {
      const char *token = aDLLName.c_str() + prefix.size();
      string sym;
      uintptr_t adr = 0;
      bool done = false;
      while (!done) {
        // scan name
        switch (*token) {
        case 0:
          // premature end
          done = true;
          break;
        default:
          sym += *token;
          token++;
          break;
        case '/':
          // symbol without address?!
          sym = "";
          token++;
          break;
        case '=':
          token++;
          // scan decimal address
          while (!done && sym.size()) {
            switch (*token) {
            case 0:
              done = true;
              // fall through
            case '/':
              if (sym.size() && adr)
                fSymbols[sym] = (void *)adr;
              sym = "";
              adr = 0;
              token++;
              break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
              adr = adr * 10 + *token - '0';
              token++;
              break;
            default:
              // ???
              token++;
              break;
            }
          }
          break;
        }
      }
      return true;
    }

#ifdef RTLD_NEXT
    string fullname = aDLLName + "_Module_Version";
    if (dlsym(RTLD_DEFAULT, fullname.c_str()))
      return true;
#endif

    const char* DSuff= ".so";
    string      lName;
    string      aName= aDLLName;

    do {
      aDLL = dlopen(aName.c_str(), RTLD_LAZY); if (!dlerror()) break;      

      aName+= DSuff;
      aDLL = dlopen(aName.c_str(), RTLD_LAZY); if (!dlerror()) break;

      lName= "./";   lName+= aName;    // try Linux current path as well
      aDLL = dlopen( lName.c_str(), RTLD_LAZY ); if (!dlerror()) break;
      aDLL = dlopen( aDLLName.c_str(),      RTLD_LAZY ); if (!dlerror()) break;

      lName= "./";   lName+= aDLLName; // try Linux current path as well
      aDLL = dlopen( lName.c_str(), RTLD_LAZY );
    } while (false);

    return !dlerror();
  }

  bool destroy()
  {
    bool ok  = true;
    if (this) {
      if (aDLL) {
        int err = dlclose(aDLL);
        ok = !err;
      }
      delete this;
    }
    return ok;
  }

  bool function(const char *aFuncName, void *&aFunc)
  {
    map<string, void *>::const_iterator it = fSymbols.find(aFuncName);
    if ((it) != fSymbols.end()) {
      aFunc = it->second;
      return true;
    }

#ifdef RTLD_DEFAULT
    if (!aDLL) {
      string fullname = aDLLName + '_' + aFuncName;
      aFunc = dlsym(RTLD_DEFAULT, fullname.c_str());
    } else
#endif
      aFunc = dlsym(aDLL, aFuncName);
    return aFunc!=NULL && dlerror()==NULL;
  }
};

bool ConnectDLL( void* &aDLL, const char* aDLLname, ErrReport aReport, void* ref )
/* Connect to <aDLLname>, result is <aDLL> reference */
/* Returns true, if successful */
{
  DLWrapper *wrapper = new DLWrapper(aDLLname);
  bool ok = false;
  if  (wrapper) {
    ok = wrapper->connect();
    if (ok)
      aDLL = (void *)wrapper;
    else
      delete wrapper;
  }

  if   (!ok && aReport) aReport( ref, aDLLname );
  return ok;
} // ConnectDLL



bool DisconnectDLL( void* aDLL )
/* Disconnect <hDLL> */
{
  return ((DLWrapper  *)aDLL)->destroy();
} // DisonnectDLL



bool DLL_Function( void* aDLL, const char* aFuncName, void* &aFunc )
/* Get <aFunc> of <aFuncName> */
/* Returns true, if available */
{
  return ((DLWrapper *)aDLL)->function(aFuncName, aFunc);
} // DLL_Function


/* eof */
