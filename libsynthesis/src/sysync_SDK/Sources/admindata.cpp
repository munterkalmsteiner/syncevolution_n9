/*
 *  File:    admindata.cpp
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


#include "admindata.h"
#include "SDK_util.h"    // include SDK utilities
#include "SDK_support.h"


namespace sysync { // the TAdminData class is part of sysync namespace

#define P_Admin  "ADM_"  // admin table
#define P_Map    "MAP_"  // map   table


// ---------------------------------------------------------------------------
void TAdminData::Init( void* aCB, cAppCharP aDBName, string aMapPath,
                                                     string aContextName,
                                                     string sDevKey,
                                                     string sUsrKey )
{
  fCB         = aCB;             // the callback for debug purposes
  fDBName     = aDBName;         // DBName, used for debug purposes
  myDB        = fDBName.c_str(); // access the same way as in the textdb module

  fMapPath    = aMapPath;
  fContextName= aContextName;
  fDevKey     = sDevKey;
  fUsrKey     = sUsrKey;
} // Init


/*! Reset the map counter for 'ReadNextMapItem' */
void TAdminData::ResetMapCounter() {
  fMap= &fMapList;
} // ResetMapCounter


// Get <mID.flags> string
string TAdminData::MapID_Flag_Str( cMapID mID, bool asHex )
{
  if (asHex) return HexStr( mID->flags );
  else       return IntStr( mID->flags ); // default is integer
} // MapID_Flag_Str


// Get <mID.localID>,<mID.remoteID> <mID.flags> string
string TAdminData::MapID_Str( cMapID mID )
{
  string id= mID->localID;
  if       (*mID->remoteID!=0) { id+= ","; id+= mID->remoteID; } // if

         id+= " flags=" + MapID_Flag_Str(         mID, true  );
         id+= " ident=" + IntStr        ( (sInt32)mID->ident );
  return id;
} // MapID_Str



// ---------------------------------------------------------------------------
TSyError TAdminData::LoadAdminData( string aLocDB, string aRemDB, appCharP *adminData )
{
  TSyError err;
  TDBItem* actI;

  string combi  = ConcatNames( fUsrKey,     fContextName, "_" );
         combi  = ConcatNames( fDevKey,            combi, "_" );
  string admName= ConcatPaths( fMapPath, P_Admin + combi + ".txt" );
  string mapName= ConcatPaths( fMapPath, P_Map   + combi + ".txt" );

       fAdmList.fFileName= admName;
  err= fAdmList.LoadDB( true, "ADM", fCB );

  // Get all the elements of the list as a \n separated string <s>
  string s= "";         actI= &fAdmList;
  bool found= ListNext( actI );
  if  (found) {
    TDBItemField*     actK= &fAdmList.item; // the key identifier
    TDBItemField*     actF=    &actI->item; // the item current field
    while  (ListNext( actK,actF )) {
      cAppCharP    k= actK->field.c_str();
      cAppCharP    f= actF->field.c_str();

      s+= KeyAndField( k,f ) + '\n';
    } // while
  }
  else { // no such element: create a new one with empty admin data
    string                    newItemID;
    fAdmList.CreateEmptyItem( newItemID, actI );
    DEBUG_DB( fCB, myDB, Da_LA, "newItemID='%s'", newItemID.c_str() );
  } // if

  *adminData= StrAlloc( s.c_str() );
  fAdm= actI;
  fAdmList.Disp_Items( fCB );
  fMapList.fFileName= mapName;

  if (!mapName.empty()) {
    err= fMapList.LoadDB( false, "", fCB );
    DEBUG_DB( fCB, myDB, Da_LA, "mapFile='%s' err=%d", fMapList.fFileName.c_str(), err );
  } // if

  ResetMapCounter(); // <aLocDB> and <aRemDB> are not yes use in this implementation
  DEBUG_DB( fCB, myDB, Da_LA, "'%s' '%s'", aLocDB.c_str(),aRemDB.c_str() );

  if (found) return LOCERR_OK;
  else       return DB_NotFound;
} /* LoadAdminData */


TSyError TAdminData::SaveAdminData( cAppCharP adminData )
{
                fAdmList.UpdateFields( fCB, adminData, fAdm, true );
                fAdmList.Disp_Items  ( fCB,Da_SA );
  TSyError err= fAdmList.SaveDB( true, fCB );
  if     (!err)
           err= fMapList.SaveDB( false,fCB );
  return   err;
} /* SaveAdminData */



// ---------------------------------------------------------------------------
bool TAdminData::ReadNextMapItem( MapID mID, bool aFirst )
{
  if (aFirst) ResetMapCounter();
  string s= "(EOF)";
  bool  ok= ListNext( fMap );
  if  ( ok ) {
    TDBItem* mpL= &fMapList;
    TDBItem* act=  fMap;
                              mID->localID = StrAlloc( act->itemID.c_str() );
    mpL->Field( "0",act, s ); mID->remoteID= StrAlloc(           s.c_str() );
    mpL->Field( "1",act, s ); mID->flags   = atoi    (           s.c_str() );
    mpL->Field( "2",act, s ); mID->ident   = atoi    (           s.c_str() );
    s=             MapID_Str( mID );
  } // if

  DEBUG_DB( fCB, myDB,Da_RM, "%s", s.c_str() );
  return ok;
} // ReadNextMapItem



TSyError TAdminData::GetMapItem( cMapID mID, TDBItem* &act )
{
  TDBItem* mpL= &fMapList;
  return   mpL->GetItem_2( mID->localID, IntStr( (sInt32)mID->ident ).c_str(), mpL, act );
} // GetMapItem


TSyError TAdminData::UpdMapItem( cMapID mID, TDBItem* act )
{
  TSyError  err= fMapList.UpdateField( fCB, "0",                 mID->remoteID,        act, false );
  if (!err) err= fMapList.UpdateField( fCB, "1", MapID_Flag_Str( mID ).c_str(),        act, false );
  if (!err) err= fMapList.UpdateField( fCB, "2", IntStr( (sInt32)mID->ident ).c_str(), act, false );
  return    err;
} // UpdMapItem



TSyError TAdminData::InsertMapItem( cMapID mID )
{
  TDBItem*                          act;
  TSyError    err= GetMapItem( mID, act );
  if   (!err) err= DB_Error; // Element already exists
  else {
    ItemID_Struct a; a.item  = mID->localID;
                     a.parent= const_cast<char *>(""); // map items are not hierarchical

    string                                       newItemID;
              err= fMapList.CreateEmptyItem( &a, newItemID, act );
    if (!err) err= UpdMapItem                        ( mID, act );
  } // if

  DEBUG_DB( fCB, myDB,Da_IM, "%s err=%d", MapID_Str( mID ).c_str(), err );
  return      err;
} // InsertMapItem


TSyError TAdminData::UpdateMapItem( cMapID mID )
{
  TDBItem*                        act;
  TSyError  err= GetMapItem( mID, act ); // Element does not yet exist ?
  if (!err) err= UpdMapItem( mID, act );

  DEBUG_DB( fCB, myDB,Da_UM, "%s err=%d", MapID_Str( mID ).c_str(), err );
  return    err;
} // UpdateMapItem


TSyError TAdminData::DeleteMapItem( cMapID mID )
{
  TDBItem*                            act;
  TSyError  err=     GetMapItem( mID, act );
  if (!err) err= fMapList.DeleteItem( act );

  DEBUG_DB( fCB, myDB,Da_DM, "%s err=%d", MapID_Str( mID ).c_str(), err );
  return    err;
} // DeleteMapItem


} /* namespace */
/* eof */
