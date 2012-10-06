/*  
 *  File:    snowwhite.cpp
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  Programming interface between Synthesis SyncML engine 
 *  and a database structure.
 *
 *  Copyright (c) 2008-2011 by Synthesis AG + plan44.ch
 *
 *  
 *    E X A M P L E    C O D E
 *    ========================
 * (To be used as starting point for SDK development.
 *  It can be adapted and extended by programmer)
 *
 */

#include "myadapter.h"

namespace oceanblue {
  using sysync::memSize;
  using sysync::stringSize;
  using sysync::appChar;
  
  using sysync::LOCERR_OK;
  using sysync::DB_Forbidden;
  using sysync::DB_NotFound;
  using sysync::Password_MD5_OUT;
  using sysync::ReadNextItem_Unchanged;
  using sysync::ReadNextItem_EOF;
  using sysync::VP_GlobMulti;
  using sysync::VALTYPE_TIME64;

  using sysync::MinVersion;
  using sysync::Manufacturer;
  using sysync::Description;
  using sysync::YesField;
  using sysync::IntStr;
  using sysync::Parans;
  

#define BuildNumber       1 // user defined build number, can be 0..255
#define MyDB MyAdapter_Name // plugin module's name

#define Example_UsrKey "5678"

#define ItemRdOffs    12000 // examples: first valid item for reading
#define ItemOffs      15000 //           first valid item



// ---- module -----------------------------------------------------------------------------
// If your implementation has a higher build number, override it as done here
CVersion MyAdapter_Module::Version()
{ 
  CVersion  v= Plugin_Version( BuildNumber ); // an individual build number
  DEBUG_DB( fCB, MyDB,Mo_Ve, "%08X", v );     // NOTE: do not change the version number itself
  return    v;                                //       as it used by the engine to
} // Version                                  //       guarantee up/downwards compatibility


// If your implementation has special capabilities, override them here
// Your company's name and the plugin descriptiom should be added here
TSyError MyAdapter_Module::Capabilities( appCharP &capa ) 
{ 
  string          s = MyPlatform();    // some standard identifiers
  s+= '\n';       s+= DLL_Info;
  
  Manufacturer  ( s, "Plugin GmbH"  ); // **** your company's name ****
  
  string              desc =   MyAdapter_Name;
                      desc+= " Plugin";
  Description   ( s,  desc          ); // **** your plugin's description ****
  
  MinVersion    ( s,  VP_GlobMulti  ); // at least V1.5.1
  YesField      ( s,  CA_ADMIN_Info ); // show "ADMIN" info
  YesField      ( s,  CA_ItemAsKey  ); // use the <asKey> routines
  capa= StrAlloc( s.c_str() ); 
  return LOCERR_OK;
} // Capabilities



// ---- session ----------------------------------------------------------------------------
// Example password mode
sInt32 MyAdapter_Session::PasswordMode()
{
  int    mode= Password_MD5_OUT;
  return mode;
} /* Session_PasswordMode */



// Example login allowed for super:user MD5 login, resulting in <sUsrKey> = "5678"
TSyError MyAdapter_Session::Login( cAppCharP  sUsername, 
                                    appCharP &sPassword, appCharP &sUsrKey )
{
  cAppCharP MD5_OUT_SU= "11JrMX94iTR1ob5KZFtEwQ=="; // the MD5B64 example for "super:user"
  TSyError  err= DB_Forbidden; // default

  if (strcmp( sUsername,"super" )==0) { // an example account ("super"), hard coded here
    err      = LOCERR_OK;
    sPassword= StrAlloc( MD5_OUT_SU );
    sUsrKey  = StrAlloc( Example_UsrKey ); // <sUsrKey> allows to access the datastore later
  } // if

  return err;
} // Login



// ---- utility routines -------------------------------------------------------------------
// set some contacts field as example
void MyAdapter::SetContactFields( KeyH aItemKey, cAppCharP aFirst, 
                                                 cAppCharP aLast,
                                                 cAppCharP aCity )
{
  SetInt32( aItemKey, "SYNCLVL",    10     ); // set some fields
  SetStr  ( aItemKey, "N_FIRST",    aFirst );
  SetStr  ( aItemKey, "N_LAST",     aLast  ); 
  SetStr  ( aItemKey, "ADR_W_CITY", aCity  ); 
} // SetContactFields


// set some events field as example
void MyAdapter::SetEventFields  ( KeyH aItemKey, cAppCharP aSum, 
                                                 cAppCharP aLoc,
                                                 cAppCharP aMod,
                                                 cAppCharP aStart, 
                                                 cAppCharP aEnd, bool allDay )
{
  SetInt32( aItemKey, "SYNCLVL",    10     ); // set some fields
  SetStr  ( aItemKey, "KIND",      "EVENT" );
  SetStr  ( aItemKey, "SUMMARY",    aSum   );
  SetStr  ( aItemKey, "LOCATION",   aLoc   ); 
  SetStr  ( aItemKey, "DMODIFIED",  aMod   );
  SetStr  ( aItemKey, "DTSTART",    aStart );
  SetStr  ( aItemKey, "DTEND",      aEnd   );
  SetInt32( aItemKey, "ALLDAY",     allDay );

  if (allDay) {
    sInt32               id= GetValueID( aItemKey, "DTSTART" );
    SetStrByID( aItemKey,id, "DATE", 0,VALSUFF_TZNAME ); // timezone name
  } // if

  if (allDay) {
    sInt32               id= GetValueID( aItemKey, "DTEND" );
    SetStrByID( aItemKey,id, "DATE", 0,VALSUFF_TZNAME ); // timezone name
  } // if
} // SetEventFields



// Get values: name, type, array size, timezone name of <aID>
TSyError MyAdapter::GetFieldKey( KeyH aItemKey, sInt32 aID, string &aKey,
                                                            uInt16 &aType,
                                                            sInt32 &nFields,
                                                            string &aTZ )
{
  TSyError err;
  
  aTZ= "";
  err= GetStrByID  ( aItemKey,aID, aKey,    0,VALSUFF_NAME   ); if (err) return err;  // field name
  err= GetInt16ByID( aItemKey,aID, aType,   0,VALSUFF_TYPE   ); if (err) return err;  // field type
  err= GetInt32ByID( aItemKey,aID, nFields, 0,VALSUFF_ARRSZ  ); if (err) nFields= -1; // array size
  
  if (aType==VALTYPE_TIME64) {
    err= GetStrByID( aItemKey,aID, aTZ,     0,VALSUFF_TZNAME ); // timezone name
  } // if
  
  return LOCERR_OK;
} // GetFieldKey


// Display available fields of <aItemKey>
void MyAdapter::DisplayFields( KeyH aItemKey )
{
  cAppCharP DF= "Display_Fields";
  TSyError  err;
  sInt32    id;
  string    vKey, vTZ, vName;
  uInt16    vType;
  sInt32    nFields;

  cAppCharP step= VALNAME_FIRST;
  while (true) {
    id = GetValueID ( aItemKey, step );
    err= GetFieldKey( aItemKey, id, vKey,vType,nFields,vTZ ); if (err) break;
    
    bool isArr= nFields>=0; // check array condition
    if  (isArr)   DEBUG_DB( fCB, MyDB,DF, "  (%2d) %s: **ARRAY** %d", 
                                          vType, vKey.c_str(), nFields );
    else        nFields= 1; // not an array -> 1 element
    
    if (vTZ!="") vTZ= " " + Parans( "TZNAME=" + vTZ );

    // for each array element do ...
    for (int i= 0; i<nFields; i++) {
      err= GetStrByID( aItemKey, id, vName, i ); if (err) break;
      
      DEBUG_DB( fCB, MyDB,DF, "  (%2d) %s:%s%s", vType, vKey.c_str(),
                                                       vName.c_str(),
                                                         vTZ.c_str() );
    } // for
    
    step= VALNAME_NEXT; // next element
  } // loop
} // DisplayFields



// ---- datastore --------------------------------------------------------------------------
TSyError MyAdapter::CreateContext( cAppCharP sDevKey, cAppCharP sUsrKey )
{
  string s=        sUsrKey;
  if    (s!=Example_UsrKey) return DB_Forbidden; // Check the example <sUsrKey>
  
  // just return 15000,15001,... as valid item ids
  // please choose a persistent solution for a real implementation
  fCreItem= ItemOffs;
  
  return inherited::CreateContext( sDevKey,sUsrKey ); // and call the base method
} // CreateContext



// Example routine: Return 4 valid items 12000 .. 12003
TSyError MyAdapter::ReadNextItemAsKey( ItemID  aID,     KeyH /* aItemKey */,
                                       sInt32 &aStatus, bool    aFirst )
{
  // Example: Constant 4 valid contact items or 2 event items for this example
  // See "ReadItemAsKey" for details of this example
  uInt32                        n= 0;
  if (fContextName=="contacts") n= 4;
  if (fContextName=="events"  ) n= 2;
  
  if (aFirst) fNthItem= 0; // re-initialize it
  if         (fNthItem>=n) { aStatus= ReadNextItem_EOF; return LOCERR_OK; }
  
  // Create items 12000 ..
  aID->item= StrAlloc( IntStr( ItemRdOffs+fNthItem ).c_str() );
                                          fNthItem++; // next
  aStatus= ReadNextItem_Unchanged;
  return LOCERR_OK;
} // ReadNextItemAsKey



// Example routine: Return 4 valid items 12000..12003 for contacts
//                         2 valid items 12000..12001 for events
TSyError MyAdapter::ReadItemAsKey( cItemID aID, KeyH aItemKey  )
{ 
  TSyError err= LOCERR_OK;
  string   s  = aID->item;
  
  do {
    if (fContextName=="contacts") {
      if (s=="12000") { SetContactFields( aItemKey, "Johann Sebastian","Bach",   "Leipzig"  ); break; }
      if (s=="12001") { SetContactFields( aItemKey, "Wolfgang Amadeus","Mozart", "Salzburg" ); break; }
      if (s=="12002") { SetContactFields( aItemKey, "Richard",         "Wagner", "Bayreuth" ); break; }
      if (s=="12003") { SetContactFields( aItemKey, "Giacomo",         "Puccini","Lucca"    ); break; }
    } // if
    
    if (fContextName=="events") { // can have the same IDs as contacts, because they are independent
      if (s=="12000") { SetEventFields  ( aItemKey, "Sechsel\xC3\xA4uten", 
                                                    "Bellevue",              "20081110T140000Z",
                                                                 "20090420", "20090421" );    break; }
      if (s=="12001") { SetEventFields  ( aItemKey, "Knabenschiessen",     
                                                    "Albisg\xC3\xBC\x65tli", "20081110T140000",
                                                          "20090914T041500", "20090915T17" ); break; }
    } // if
   
    // not found
    err= DB_NotFound;
  } while (false);
  
  return err;
} // ReadItemAsKey



// Example routine: Debug display of the item contents. 
// Return item names 15000 ...
TSyError MyAdapter::InsertItemAsKey( KeyH aItemKey, ItemID newID ) 
{ 
  // **** implementation for inserting an item must be added here ****
  TSyError err;
  sInt32   id;
  
  // ---- as an example, first and last name will be read ----
  string vFirst, vLast;
  err= GetStr    ( aItemKey, "N_FIRST", vFirst ); // get it directly  
  
  id = GetValueID( aItemKey, "N_LAST" );          // or get it via id
  err= GetStrByID( aItemKey,  id,       vLast  );
  
  DEBUG_DB( fCB, MyDB,Da_IIK, "%08X %s %s", aItemKey, vFirst.c_str(),vLast.c_str() );
  
  // ---- this is a simple loop to show the contents of each field of an item ----
  DisplayFields( aItemKey );
  
  // just return 15000, 15001, ... as valid item ids
  newID->item= StrAlloc( IntStr( fCreItem++ ).c_str() );
  return LOCERR_OK;
} // InsertItemAsKey



TSyError MyAdapter::UpdateItemAsKey( KeyH aItemKey, cItemID /* aID */, ItemID /* updID */ ) 
{
  // **** implementation for updating an item must be added here ****
  DEBUG_DB( fCB, MyDB,Da_UIK, "%08X", aItemKey );
  DisplayFields( aItemKey );

  return LOCERR_OK;
} // UpdateItemAsKey



TSyError MyAdapter::DeleteItem( cItemID /* aID */ )
{
  // **** implementation for deleting an item must be added here ****
  return DB_NotFound;
} // DeleteItem



} // namespace
/* eof */
