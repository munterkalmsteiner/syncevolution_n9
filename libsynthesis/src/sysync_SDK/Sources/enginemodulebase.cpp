/**
 *  @File     enginemodulebase.cpp
 *
 *  @Author   Beat Forster (bfo@synthesis.ch)
 *
 *  @brief TEngineModuleBase
 *    engine bus bar class
 *
 *    Copyright (c) 2007-2011 by Synthesis AG + plan44.ch
 *
 */


#include "prefix_file.h"
#include "enginemodulebase.h"
#include "SDK_util.h"

#if defined SYSYNC_ENGINE || defined SYSYNC_ENGINE_TEST
  #include "engineentry.h"
#endif

#if !defined(SYSYNC_ENGINE) || defined(ENGINEINTERFACE_SUPPORT)

namespace sysync {



// TEngineModuleBase
// =================

// local name
#define MyName "enginemodulebase"

// constructor
TEngineModuleBase::TEngineModuleBase()
{
  fCI        = NULL;
  fEngineName= ""; // none
  fPrgVersion= 0;
  fDebugFlags= 0;
  fCIisStatic= false;
} // TEngineModuleBase


// destructor
TEngineModuleBase::~TEngineModuleBase()
{
  #if defined SYSYNC_ENGINE || defined SYSYNC_ENGINE_TEST
    if (fCI && !fCIisStatic) delete fCI;
  #endif
} // ~TEngineModuleBase



// --- Initial connection ------------------------
TSyError TEngineModuleBase::Connect( string   aEngineName,
                                     CVersion aPrgVersion,
                                     uInt16   aDebugFlags)
{
  TSyError     err= LOCERR_OK;

  fEngineName = aEngineName;
  fPrgVersion = aPrgVersion;
  fDebugFlags = aDebugFlags;

  #if defined SYSYNC_ENGINE || defined SYSYNC_ENGINE_TEST
    uInt16 cbVersion= DB_Callback_Version; // use current by default
    if (fCI==NULL) {
      fCI = &fCIBuffer;
      fCIisStatic = true;
    }
    else {
      cbVersion= fCI->callbackVersion; // the cbVersion from outside (ConnectEngineS)
    } // if

    InitCallback_Exotic( fCI, cbVersion ); // be aware that it could be an older version
                         fCI->thisBase= this; // get <this> later for callback calls
    CB_Connect         ( fCI );
  #endif

  if (fPrgVersion==0) fPrgVersion= Plugin_Version( 0 ); // take the internal one
  if (fPrgVersion<VP_UIApp)   err= LOCERR_NOTIMP;

  if (fCI)  fCI->debugFlags= fDebugFlags;
  if (!err) err= Init();
  DEBUG_DB( fCI, MyName, "Connect","version=%08X flags=%04X err=%d", fPrgVersion, fDebugFlags, err );
  return    err;
} // Connect


TSyError TEngineModuleBase::Disconnect()
{
  if      (!fCI) return LOCERR_OK;
  DEBUG_DB( fCI, MyName, "Disconnect","" );

  TSyError  err= Term();
  return    err;
} // Disconnect


// ----------------------------------------------------------------------------------------------
TSyError TEngineModuleBase::GetStrValue( KeyH aKeyH, cAppCharP aValName, string &aText )
{
  TSyError err;

  const int         txtFieldSize= 80; // a readonable string length to catch it at once
  char    txtField[ txtFieldSize ];
  memSize txtSize;
  char*   f= (char*)&txtField;

  // try directly to fit in a short string first
  err=   GetValue( aKeyH, aValName, VALTYPE_TEXT, f, txtFieldSize, txtSize );

  bool tooShort= err==LOCERR_TRUNCATED;
  if  (tooShort) {
    err= GetValue( aKeyH, aValName, VALTYPE_TEXT, f, 0,            txtSize ); // get size
    f=                                (char*)malloc( txtSize+1 );             // plus NUL termination
    err= GetValue( aKeyH, aValName, VALTYPE_TEXT, f, txtSize+1,    txtSize ); // no error anymore
  } // if

  aText.assign(f, txtSize); // assign it

  if (tooShort)
    free( f ); // and deallocate again, if used dynamically

  return err;
} // GetStrValue;


TSyError TEngineModuleBase::SetStrValue( KeyH aKeyH, cAppCharP aValName, string aText )
{
  //   -1 automatically calculate length from null-terminated string
  return SetValue( aKeyH, aValName, VALTYPE_TEXT, aText.c_str(), (memSize)-1 );
} // SetStrValue


// ----------------------------------------------------------------------------------------------
TSyError TEngineModuleBase::GetInt8Value( KeyH aKeyH, cAppCharP aValName, sInt8  &aValue )
{
  memSize                                                                   vSize;
  return GetValue( aKeyH, aValName, VALTYPE_INT8,  &aValue, sizeof(aValue), vSize );
} // GetInt8Value

TSyError TEngineModuleBase::GetInt8Value( KeyH aKeyH, cAppCharP aValName, uInt8  &aValue )
{
  memSize                                                                   vSize;
  return GetValue( aKeyH, aValName, VALTYPE_INT8,  &aValue, sizeof(aValue), vSize );
} // GetInt8Value

TSyError TEngineModuleBase::SetInt8Value( KeyH aKeyH, cAppCharP aValName, uInt8   aValue )
{
  return SetValue( aKeyH, aValName, VALTYPE_INT8,  &aValue, sizeof(aValue) );
} // SetInt8Value


// ----------------------------------------------------------------------------------------------
TSyError TEngineModuleBase::GetInt16Value( KeyH aKeyH, cAppCharP aValName, sInt16 &aValue )
{
  memSize                                                                   vSize;
  return GetValue( aKeyH, aValName, VALTYPE_INT16, &aValue, sizeof(aValue), vSize );
} // GetInt16Value

TSyError TEngineModuleBase::GetInt16Value( KeyH aKeyH, cAppCharP aValName, uInt16 &aValue )
{
  memSize                                                                   vSize;
  return GetValue( aKeyH, aValName, VALTYPE_INT16, &aValue, sizeof(aValue), vSize );
} // GetInt16Value

TSyError TEngineModuleBase::SetInt16Value( KeyH aKeyH, cAppCharP aValName, uInt16  aValue )
{
  return SetValue( aKeyH, aValName, VALTYPE_INT16, &aValue, sizeof(aValue) );
} // SetInt16Value


// ----------------------------------------------------------------------------------------------
TSyError TEngineModuleBase::GetInt32Value( KeyH aKeyH, cAppCharP aValName, sInt32 &aValue )
{
  memSize                                                                   vSize;
  return GetValue( aKeyH, aValName, VALTYPE_INT32, &aValue, sizeof(aValue), vSize );
} // GetInt32Value

TSyError TEngineModuleBase::GetInt32Value( KeyH aKeyH, cAppCharP aValName, uInt32 &aValue )
{
  memSize                                                                   vSize;
  return GetValue( aKeyH, aValName, VALTYPE_INT32, &aValue, sizeof(aValue), vSize );
} // GetInt32Value

TSyError TEngineModuleBase::SetInt32Value( KeyH aKeyH, cAppCharP aValName, uInt32  aValue )
{
  return SetValue( aKeyH, aValName, VALTYPE_INT32, &aValue, sizeof(aValue) );
} // SetInt32Value


// ----------------------------------------------------------------------------------------------
void     TEngineModuleBase::AppendSuffixToID( KeyH aKeyH, sInt32 &aID, cAppCharP aSuffix )
{
  string s= aSuffix;
  if    (s=="") return;
                           s= VALNAME_FLAG + s; 
  aID+= GetValueID( aKeyH, s.c_str() );
} // AppendSuffixToID



} // namespace sysync
/* end of TEngineModuleBase implementation */

#endif // not used in engine or engine with ENGINEINTERFACE_SUPPORT

// eof
