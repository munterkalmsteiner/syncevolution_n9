/*
 *  File:         UI_util.h
 *
 *  Author:       Beat Forster
 *
 *  Programming interface between a user application
 *  and the Synthesis SyncML client engine.
 *
 *  Copyright (c) 2007-2011 by Synthesis AG + plan44.ch
 *
 *
 */

#include <string>
#include "sync_dbapidef.h"


namespace sysync {


class UIContext {
  public:
    UI_Call_In uCI;
    appPointer uDLL;
    string     uName;
}; // UIContext



/* Function definitions */
TSyError UI_Connect   ( UI_Call_In &aCI,
                        appPointer &aDLL,
                        bool &aIsServer, cAppCharP aEngineName,
                                         CVersion  aPrgVersion,
                                         uInt16    aDebugFlags );
TSyError UI_Disconnect( UI_Call_In  aCI,
                        appPointer  aDLL,
                        bool  aIsServer );


} // namespace sysync
/* eof */
