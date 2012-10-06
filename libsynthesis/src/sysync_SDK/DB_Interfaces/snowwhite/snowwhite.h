/*  
 *  File:    snowwhite.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *  Datastore plugin classes which is derived 
 *  from the "oceanblue" classes.
 *
 *  Copyright (c) 2008-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef SNOWWHITE_H
#define SNOWWHITE_H

#include "myadapter.h"
#include "sync_include.h"

#include "oceanblue.h"
namespace oceanblue {



class MyAdapter_Module : public OceanBlue_Module
{
  typedef OceanBlue_Module inherited;
  
  public:
    /**** any of the OceanBlue_Module methods can be overriden ****/
    CVersion Version();
    TSyError Capabilities( appCharP &capa ); 
}; // MyAdapter_Module



class MyAdapter_Session : public OceanBlue_Session
{
  typedef OceanBlue_Session inherited;
  
  public:
    /**** any of the OceanBlue_Session methods can be overriden ****/
    sInt32   PasswordMode();
    TSyError Login( cAppCharP sUsername, appCharP &sPassword, appCharP &sUsrKey );
}; // MyAdapter_Session



class MyAdapter : public OceanBlue
{
  typedef OceanBlue inherited;
  
  private:
    uInt32 fCreItem; // an internal number for new item creation
    
    // Fill example items
    void     SetContactFields( KeyH aItemKey, cAppCharP aFirst, 
                                              cAppCharP aLast,
                                              cAppCharP aCity );
    void     SetEventFields  ( KeyH aItemKey, cAppCharP aSum, 
                                              cAppCharP aLoc,
                                              cAppCharP aMod,
                                              cAppCharP aStart,
                                              cAppCharP aEnd, bool allday= true );
    // Get field information
    TSyError GetFieldKey     ( KeyH aItemKey, sInt32 aID, string &aKey,
                                                          uInt16 &aType,
                                                          sInt32 &nFields,
                                                          string &aTZ );
    // Display fields of the work item                                                       
    void   DisplayFields     ( KeyH aItemKey );

  public:
    /**** any of the OceanBlue methods can be overriden ****/
    TSyError CreateContext   ( cAppCharP  sDevKey, cAppCharP sUsrKey );
    
    // for this example, the <asKey> methods will be used
    TSyError ReadNextItemAsKey  ( ItemID  aID,         KeyH aItemKey,
                                  sInt32 &aStatus,     bool aFirst   );
    TSyError ReadItemAsKey     ( cItemID  aID,         KeyH aItemKey );
    
    TSyError InsertItemAsKey      ( KeyH  aItemKey,  ItemID newID    );
    TSyError UpdateItemAsKey      ( KeyH  aItemKey, cItemID   aID, ItemID updID );
    
    TSyError DeleteItem        ( cItemID  aID );
}; // MyAdapter


} // namespace
#endif // SNOWWHITE_H
/* eof */
