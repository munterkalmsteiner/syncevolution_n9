/*
 *  File:    admindata.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  DBApi database adapter
 *  Admin data handling
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 *  E X A M P L E    C O D E
 *    (text_db interface)
 *
 */

#ifndef ADMINDATA_H
#define ADMINDATA_H

#include "sync_include.h"     // import global things
#include "sync_dbapidef.h"    // get some definitions
#include "SDK_util.h"         // include SDK utilities
#include "dbitem.h"
#include <string>


namespace sysync {


class TAdminData {
  public:
    void      Init( void* aCB, cAppCharP aDBName, string aAdminPath,
                                                  string aContextName,
                                                  string sDevKey,
                                                  string sUsrKey );

    TSyError  LoadAdminData     ( string  aLocDB,
                                  string  aRemDB,
                                appCharP *adminData );
    TSyError  SaveAdminData  ( cAppCharP  adminData );

    bool      ReadNextMapItem(  MapID mID, bool aFirst );
    TSyError    InsertMapItem( cMapID mID );
    TSyError    UpdateMapItem( cMapID mID );
    TSyError    DeleteMapItem( cMapID mID );

  private:
    void      ResetMapCounter();
    TSyError  GetMapItem     ( cMapID mID, TDBItem* &act );
    TSyError  UpdMapItem     ( cMapID mID, TDBItem*  act );

    string    MapID_Flag_Str ( cMapID mID, bool asHex= false );
    string    MapID_Str      ( cMapID mID );

    void*     fCB;         // callback structure, for debug logs
    string    fDBName;     // database name,      for debug logs
    cAppCharP myDB;        // fDBName.c_str()

    string    fMapPath;
    string    fContextName;
    string    fDevKey;
    string    fUsrKey;

    TDBItem   fAdmList;    // admin structure
    TDBItem*  fAdm;        // admin item
    TDBItem   fMapList;    // map   structure
    TDBItem*  fMap;        // map   item
}; // TAdminData


} /* namespace */
#endif /* ADMINDATA_H */
/* eof */
