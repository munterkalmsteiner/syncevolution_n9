/*
 *  File:    uiapi.h
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


#ifndef UI_API_H
#define UI_API_H

// access to the definitions of the interface
#include "sync_dbapidef.h"
#include "dbapi.h"


namespace sysync {


class TUI_Api
{
  public:
    TUI_Api(); // constructor
   ~TUI_Api(); //  destructor

    // Connect to the plug-in <moduleName> with <mContextName>
    TSyError Connect( const char* moduleName, const char* mContextName= "", bool aIsLib= false, bool allowDLL= true );
    bool     Connected() { return fConnected; } // read status of <fConnected>

    // Run the UI API
    TSyError RunContext();

    // Disconnect the UI API
    TSyError Disconnect();

    TDB_Api_Callback fCB;            // Callback wrapper

  private:
    TSyError CreateContext( const char* uiName, TDB_Api_Config &config );
    TSyError DeleteContext();

    bool               fConnected;   // if successfully connected to module
    TDB_Api_Config*    m;            // the module behind
    API_Methods*       dm;           // local reference to the API methods

    CContext           uContext;     // The UI context
    bool               uCreated;     // if successfully connected to ui context
    string             uName;        // local copy of <uiName>
}; // class TUI_Api



} // namespace
#endif // UI_API_H
/* eof */
