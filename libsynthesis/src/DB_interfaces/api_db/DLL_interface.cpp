/*
 *  File:    DLL_interface.cpp
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


#include "sync_include.h"
#include "sync_dbapidef.h"
#include "sync_dbapiconnect.h"
#include "SDK_util.h"
#include "SDK_support.h"
#include "DLL_interface.h"

#ifdef ANDROID
  #include "android/log.h"
#endif

#ifdef SYSYNC_ENGINE
  #include "syncappbase.h"
#endif

#ifdef JNI_SUPPORT
  #include "JNI_interface.h"
#endif

#ifdef __cplusplus
  namespace sysync {
#endif


// ------------------------------------------------------------------
#ifdef PLUGIN_DLL
//! derived class for DLL access
bool TDLL::Connect( cAppCharP aModName, ErrReport aReport, void* /* ref */ )
{
                           fModName= Plugin_MainName( aModName );
  return ConnectDLL( fMod, fModName.c_str(), aReport );
} // TDLL::Connect


bool TDLL::GetFunction( cAppCharP aFuncName, cAppCharP /* aParams */,
                       appPointer    &aFunc, ErrMReport aReport, void* ref )
{
  appPointer m; /* don't overwrite <aFunc> if "" or in case of error */
  if                     (*aFuncName=='\0')              return true;
  if (!DLL_Function( fMod, aFuncName, m  )) {
             aReport( ref, aFuncName,fModName.c_str() ); return false; }

  aFunc= m; return true;
} // TDLL::GetFunction


bool TDLL::Disconnect() {
  return DisconnectDLL( fMod );
} // TDLL::Disconnect
#endif // PLUGIN_DLL


// ---- Handler for DLL connection reporting ------------------------
static void ReportPuts( cAppCharP aText )
{
  #ifdef ANDROiD
    __android_log_write( ANDROID_LOG_DEBUG, "ReportError", aText );
  #else
    ConsolePrintf( aText );
  #endif
} // ReportPuts


void Report_Error( cAppCharP aText, ... )
{
  const sInt16 maxmsglen=1024;
  char msg[maxmsglen];
  va_list args;

  msg[0]='\0';
  va_start(args, aText);
  // assemble the message string
  vsnprintf(msg, maxmsglen, aText, args);
  va_end(args);

  // write the string
  ReportPuts( msg );
}


void ModuleConnectionError( void* /* ref */, cAppCharP aModName )
{
  string               s= "Can't connect to library "  + Apo(  aModName ) + ".";
  Report_Error       ( s.c_str() );
} // ModuleConnectionError


void FuncConnectionError( void* /* ref */, cAppCharP aFuncName, cAppCharP aModName )
{
  string               s= "Can't connect to function " + Apo( aFuncName );
  if (*aModName!='\0') s+= " of module "               + Apo(  aModName );
                       s+= ".";
  Report_Error       ( s.c_str() );
} // FuncConnectionError



// ------------------------------------------------------------------
static TAccess* AssignedObject( cAppCharP name, bool is_jni )
// Create object for LIB or DLL or JNI
{
  TAccess* d     = NULL;
  bool     is_lib= false;

  do {
    #ifdef JNI_SUPPORT
      if (is_jni) { d= static_cast<TAccess*>( new TJNI_Methods ); break; } // JNI (Java)
    #endif

          is_lib= IsLib( name );
    if   (is_lib) { d=                        new TAccess;        break; } // LIB direct

    #ifdef PLUGIN_DLL
                    d= static_cast<TAccess*>( new TDLL );         break;   // DLL (C/C++)
    #endif
  } while (false);

  if (d==NULL) return NULL;

  d->fJNI= is_jni;
  d->fLIB= is_lib;

  return d;
} // AssignedObject



// ------------------------------------------------------------------
TSyError ConnectModule( appPointer &aMod, cAppCharP aModName, bool is_jni )
{
  TAccess* d= AssignedObject( aModName, is_jni );
  if     (!d)        return DB_Forbidden;
  if      (d->Connect       ( aModName,ModuleConnectionError ))
       { aMod=    d; return LOCERR_OK;   }
  else { aMod= NULL; return DB_NotFound; }
} // ConnectModule



TSyError ConnectFunctions( appPointer aMod, appPointer aField, memSize aFieldSize,
                           bool aParamInfo, ... )
/* Connect to DLL <aDLLname> (or LIB if <aDLLname> = "") */
{
  TSyError    err      = LOCERR_OK;
  TAccess*    d        = (TAccess*)aMod;
  char*       p        = NULL;
  char*       q;
  appPointer* m        = (appPointer*)aField;
  bool        notEnough= false;
  bool        tooMany  = false;

  if (aParamInfo) aFieldSize= 2*aFieldSize;

  // do vararg handling
  va_list   args;
  va_start( args, aParamInfo ); // the element before the open param list

  // now go thru the list of all functions and put them into the list
  uInt32 ii= 0;
  while (ii*sizeof(p)<=aFieldSize) {
      p= va_arg( args, char* ); if (p==NULL) break;
      if (p==(char*)XX)
          p= NULL; // XX is equivalent to NULL, but not termination of the list
      q= NULL;

    // if aParamInfo, treat them as a pair <p>,<q>
    if (aParamInfo) {
      q= va_arg( args, char* ); if (q==NULL) break;
      ii++;
    } // if
    ii++;

  if (!err) {
      bool ok= d->GetFunction( p,q, *m, FuncConnectionError );
      if (!ok) err= DB_NotFound;
  } // if

    m++;
  } // while
  if (err) return err;

  // check if table is too small or too long
  while (ii*sizeof(p)<aFieldSize) {
    notEnough= true;
    *m= NULL; // initialize the rest to zero
    m++;
                    ii++;
    if (aParamInfo) ii++;
  } // while
  tooMany= (p!=NULL);

  va_end( args );

  if (notEnough || tooMany) return LOCERR_WRONGUSAGE;
  return err;
} // ConnectFunctions



TSyError DisconnectModule( appPointer &aMod )
{
  TAccess* d  = (TAccess*)aMod;
  TSyError err= LOCERR_OK;

  if  (d==NULL)  return err;
  if (!d->Disconnect()) err= DB_NotFound;

  do {
    #ifdef JNI_SUPPORT
      if (d->fJNI) { TJNI_Methods* dj= static_cast<TJNI_Methods*>( d );
                     delete        dj; break; } // JNI (Java)
    #endif

    if   (d->fLIB) { delete        d;  break; } // LIB direct

    #ifdef PLUGIN_DLL
                   { TDLL*         dd= static_cast<TDLL*>( d );
                     delete        dd; break; } // DLL (C/C++)
    #endif
  } while (false);

  aMod= NULL; // no longer access
  return err;
} // DisconnectModule


#ifdef __cplusplus
  } // namespace
#endif

/* eof */
