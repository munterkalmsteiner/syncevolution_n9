/*
 *  File:    uiapi.cpp
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *  TUI_Api class
 *    Bridge to user programmable interface
 *
 *  Copyright (c) 2007-2011 by Synthesis AG + plan44.ch
 *
 */

/*
 * The "TUI_Api" class acts as a standard interface between
 * the SySync Server and a (user programmable) module "sync_uiapi".
 *
 * It is possible to have more than one (identical) interface module,
 * either packed into a DLL or directly linked to the server
 * (or combined).
 *
 */


#include "uiapi.h"
#include "sync_uiapi.h"


namespace sysync {


/* ---- "DB_Api_Config" implementation ------------------------------------- */
TUI_Api:: TUI_Api() { fConnected= false; uCreated= false; } // constructor
TUI_Api::~TUI_Api() { /* empty */                         } //  destructor



/* Create a context */
TSyError TUI_Api::CreateContext( cAppCharP uiName, TDB_Api_Config &config )
{
  typedef TSyError (*CreateU_Func)( CContext *uContext,
                                   cAppCharP  uiName,
                                 DB_Callback  uCB );

  if (uCreated) return DB_Forbidden;
  uName= uiName; // make a local copy

                                dm= &config.m; // assign reference to the methods
  CreateU_Func p= (CreateU_Func)dm->ui.UI_CreateContext;

  DB_Callback uCB= &fCB.Callback;
  CContext* uc= &uCB->mContext;    // store it at a temporary var
           *uc= 0;                 // set it to check later, if changed
                                              uCB->cContext= config.mContext; // inherit info
                     uContext= 0;             uCB->mContext= config.fCB.Callback.mContext;
  TSyError  err= p( &uContext, uName.c_str(), uCB );
  if      (!err) {
    uCreated= true;
    if (*uc==0) *uc= uContext; // assign for datastores, but only if not assigned in plug-in module
  } // if

  return err;
} // CreateContext



// Run the UI API
TSyError TUI_Api::RunContext()
{
  if (!uCreated) return DB_Forbidden;

  Context_Func   p= (Context_Func)dm->ui.UI_RunContext;
  TSyError err= p( uContext );
  return   err;
} // RunContext



TSyError TUI_Api::DeleteContext()
{
  if (!uCreated) return DB_Forbidden;

  Context_Func  p= (Context_Func)dm->ui.UI_DeleteContext;
  TSyError err= p( uContext );
  if     (!err) uCreated= false;
  return   err;
} // DeleteContext



// Connect the UI API
TSyError TUI_Api::Connect( cAppCharP moduleName,
                           cAppCharP mContextName, bool aIsLib, bool allowDLL )
{
  m= new TDB_Api_Config;
  m->fCB= fCB;

  CContext                              gContext= 0;
  TSyError err= m->Connect( moduleName, gContext, mContextName, aIsLib,allowDLL );

  if     (!err) {
           err= CreateContext( moduleName, *m );
  } // if

  if (err) delete m;
  else     fConnected= true;


  printf( "TUI connected err=%d\n", err );
  return err;
} // Connect


// Disconnect the UI API
TSyError TUI_Api::Disconnect()
{
  if (!fConnected) return DB_Forbidden;

  TSyError err= DeleteContext();
  if     (!err) {
           err=          m->Disconnect();
    if   (!err) { delete m; fConnected= false; }
  } // if

  printf( "TUI disconnected err=%d\n", err );
  return err;
} // Disconnect



} // namespace
/* eof */
