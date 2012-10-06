/*
 *  File:    dbitem.cpp
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  Example DBApi database adapter.
 *  TDBItem class for item handling
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 */

#include "sync_include.h"  // import some global things
#include "sync_dbapidef.h"

#include "SDK_util.h"      // include SDK utilities
#include "SDK_support.h"
#include "dbitem.h"

#ifndef SYSYNC_ENGINE
  #include "stringutil.h"  // local implementation of CStr <=> Str conversions
  #include "timeutil.h"    // local implementation for time routines
#endif


namespace sysync {

#define MyDB   "DBItem"    // 'DBItem' debug name


#define FFirst    10000    // start number for item name (will be incremented)
#define ARR_Sep  '\x1D'    // Array element separator within textdb file
#define ARR_SepS "\x1D"    // ... the same as string



// ---- utility functions ----------------------------------------------------------
// Calculate the length of <fVal>'s array expansion
static int ArrLen( cAppCharP fKey, cAppCharP fVal )
{
  cAppCharP z= "[0]";
  cAppCharP x= "[x]:";
  int     len= strlen( fVal );

  // some additional space is needed for array elements, calculate it first
  int  j= 0;
  int  i;
  for (i= 0; i<len; i++) {
    if (fVal[ i ]==ARR_Sep) j++;
  } // for

  // yes, there are such elements => increase <newLen>
  // don't forget element "[0]"
               len = 0;
  if   (j>0) { len+= strlen( z ) + j*(strlen( fKey )+strlen( x )); // "yy[x]:"
    if (j>10)  len+= j-10;                                         //  "[xx]"
  } // if

  return len;
} // ArrLen


/*! Create an item field */
void MakeField( TDBItemField* &act, cAppCharP field,
                                    cAppCharP fieldEnd, int &len, bool convert )
{
  MakeObj     ( act );
  if (fieldEnd) act->field.assign( field, (unsigned int)(fieldEnd-field) );
  else          act->field.assign( field );

  if (convert) {                  string s= "";   // don't touch array separator
    StrToCStrAppend( act->field.c_str(), s, true, ARR_Sep );
    act->field= s;
  } // if

//printf( "len=%3d field='%s'\n", len, act->field.c_str() );
  len+= act->field.length()+1;
} // MakeField


// Get a token string
static string Token_Str( string aToken )
{
  if (aToken.empty()) return "";
  else                return "[" + aToken + "]";
} // Token_Str;


// create a UTC token for time comparisons
string CurrentTime( int secsOffs, bool dateOnly )
{
  string       isoStr;
  lineartime_t f= secondToLinearTimeFactor * secsOffs;

  #ifdef SYSYNC_ENGINE
    timecontext_t tctx= TCTX_UTC;
    if (dateOnly) tctx= TCTX_DATEONLY;

  //lineartime_t                   timestamp= getSystemNowAs( TCTX_UTC, (GZones*)aGZones );
    lineartime_t                   timestamp= getSystemNowAs( TCTX_UTC, NULL );
    TimestampToISO8601Str( isoStr, timestamp + f,             tctx, false, false );
  #else
    lineartime_t                   timestamp= utcNowAsLineartime();
    timeStampToISO8601           ( timestamp + f, isoStr, dateOnly );
  #endif

  return isoStr;
} // CurrentTime



// Compare time of ISO8601 <aToken> with <aLastToken>/<aResumeToken>
int CompareTokens( string aToken, string aLastToken, string aResumeToken )
{
  bool chg=    aLastToken.empty()  || // calculate newer  condition
               aLastToken < aToken;
  bool res= !aResumeToken.empty()  && // calculate resume condition
             aResumeToken < aToken &&
             aResumeToken > aLastToken;

  int      aStatus= ReadNextItem_Unchanged;
  if (chg) aStatus= ReadNextItem_Changed;
  if (res) aStatus= ReadNextItem_Resumed;
  return   aStatus;
} // CompareTokens


// -------------------------------------------------------------------------
/* Combines <sKey> and <sField> into string <s>
 * support for blobs, tz names and array fields
 */
string KeyAndField( cAppCharP sKey, cAppCharP sField )
{
  cAppCharP fs= strstr( sField, ";" ); // semicolon pattern: BLOB, TZNAME, ...
  cAppCharP q =         sField;
  cAppCharP qE;

  string    s = sKey;
  if (fs) { s+= fs; return s; } // with semicolon

  int i= 0;
  while (true) {
         qE= strstr( q, ARR_SepS );
    if (!qE && i==0) break;

    s+= '[' + IntStr( i ) + ']'; if (!qE) break;

    string    tmp;
              tmp.assign( q, (unsigned int)( qE-q ) );
    s+= ':' + tmp + '\n';
    i++;
    q= qE+1;

    s+= sKey;
  } // loop

  s+= ":";
  s+= q;
  return s;
} // KeyAndField


string KeyAndField( TDBItemField* actKey,
                    TDBItemField* actField ) {
              return KeyAndField( actKey->field.c_str(),
                                  actField->field.c_str() );
} // KeyAndField


void AddFields( string &aDat, string aAdd, cAppCharP param1,
                                           cAppCharP param2,
                                           cAppCharP param3,
                                           cAppCharP param4,
                                           cAppCharP param5 )
{
  string p;

  if (aAdd.empty()) return;

  int i= 1;
  while (true) {
    switch ( i ) {
      case 1 : p= param1; break;
      case 2 : p= param2; break;
      case 3 : p= param3; break;
      case 4 : p= param4; break;
      case 5 : p= param5; break;
      default: p= "";
    } // switch

    string chk= "%" + IntStr( i++ );
    string::size_type pos= aAdd.find( chk,0 );
    if               (pos==string::npos) break;

    aAdd.replace( pos,chk.length(), p );
  } // loop

  if (!aDat.empty()) {
    cAppCharP c= aDat.c_str() + aDat.length()-1; // last char
    if      (*c!='\n')          aDat+= '\n';
  } // if

  aDat+= aAdd;
} // AddFields


/* ------------------------------------------------------------------------ */
//! assign ID and callback
void TDBItem::init( cAppCharP aItemID, cAppCharP aParentID, void* aCB, TDBItem* aNext )
{
  itemID  = aItemID;
  parentID= aParentID;
  fCB     = aCB;
  next    = aNext;
} // init


//! assign ID and callback (overloaded)
void TDBItem::init( cAppCharP aItemID, void* aCB, TDBItem* aNext ) {
  init( aItemID,"", aCB, aNext );
} // init


// the combined item/parent string
cAppCharP TDBItem::c_str()
{
  fID=   itemID;
  if (!parentID.empty()) { fID+= ","; fID+= parentID; }
  return    fID.c_str();
} // c_str



/* Get a specific item <actL>, which represents <itemID>,<parentID>.
 * Returns error, if not found.
 * If <first> is true (default), start at the beginning of the list,
 * else continue
 */
TSyError TDBItem::GetItem( cItemID aID, TDBItem* &actL, bool first )
{
  char* p= aID->parent; if (!p) p=  const_cast<char *>("");

  if (first)              actL= this;
  while        (ListNext( actL )) {
    if (strcmp( aID->item,actL->itemID.c_str()   )==0 &&
        strcmp( p,        actL->parentID.c_str() )==0) return LOCERR_OK;
  } // while

  actL= NULL;
  return DB_NotFound;
} // GetItem


/* Overloaded for <aItemID> only w/o parent ID
 * Get a specific item <actL>, which represents <itemID>
 * Returns error, if not found.
 */
TSyError TDBItem::GetItem( cAppCharP aItemID, TDBItem* &actL, bool first )
{
  ItemID_Struct a; a.item  = (char*)aItemID;
                   a.parent= NULL;
  return GetItem( &a, actL, first );
} // GetItem


/* Overloaded for <aItemID> only w/o parent ID
 * Get a specific item <actL>, which represents <itemID>
 * Returns error, if not found.
 */
TSyError TDBItem::GetItem_2( cAppCharP aItemID, cAppCharP aField2,
                             TDBItem* mpL, TDBItem* &act )
{
  TSyError err= LOCERR_OK;
  ItemID_Struct a; a.item  = (char*)aItemID;
                   a.parent= NULL;

  bool first= true;
  string s;

  while (true) {
    err= GetItem( &a, act, first ); if (err) break; // not found
    mpL->Field  ( "2",act, s );
    if    (strcmp( aField2,s.c_str() )==0)   break; // found
    first= false;
  } // loop

  return err;
} // GetItem_2



TSyError TDBItem::DeleteItem( TDBItem* actL )
{
  TDBItem*  prvL= actL;
  ListBack( prvL, this ); // search for the previous element

  prvL->next= actL->next;
  actL->next= NULL; // avoid destroying the whole chain
  delete      actL;

  fChanged= true;
  return LOCERR_OK;
} /* DeleteItem */



TSyError TDBItem::DeleteItem( cItemID aID )
{
  TSyError err= DB_NotFound;

  TDBItem*           actI;
  err= GetItem( aID, actI ); if (err) return err;

  // item must be not available as parent element
  TDBItem*         actL= this;
  while (ListNext( actL )) {
    if (strcmp( aID->item,actL->parentID.c_str() )==0) return DB_Forbidden;
  } // while

  return DeleteItem( actI );
} /* DeleteItem */



TSyError TDBItem::DeleteItem( cAppCharP aItemID )
{
  ItemID_Struct a;    a.item  = (char*)aItemID;
                      a.parent=  const_cast<char *>("");
  return DeleteItem( &a );
} // DeleteItem



// parent element must be "" or exist
TSyError TDBItem::ParentExist( cAppCharP aItemID, cAppCharP aParentID )
{
  if (*aParentID==0)                  return LOCERR_OK;
  if (strcmp( aItemID,aParentID )==0) return DB_Forbidden; // no recursion allowed

  TDBItem*                actL= this;
  while        (ListNext( actL )) {
    if (strcmp( aParentID,actL->itemID.c_str() )==0) return LOCERR_OK;
  } // while

  return DB_NotFound;
} // ParentExists



void TDBItem::CreateItem( string newItemID, string parentID, TDBItem* &actL )
{
  TDBItemField* actK;
  TDBItemField* actF;

  TDBItem*    newL= new TDBItem; // create item root
              newL->fCB= fCB;    // inherit callback
  actF=      &newL->item;
              newL->itemID  = newItemID;
              newL->parentID= parentID;
  actL->next= newL;
  ListNext( actL );

  int n= 0;        actK= &item;
  while (ListNext( actK )) { // number of elements according to the key
    MakeField    ( actF, "", NULL, actL->len );
    n++;
  } // while

  actL->len--; // termination is included already
  fChanged= true;
} // CreateItem



/* The strategy is simple: Take ID="10000" as start, go thru the whole list.
 * if found, increment. Insert new element at the end.
 */
TSyError TDBItem::CreateEmptyItem( ItemID aID, string &newItemID, TDBItem* &actL,
                                               string      newID )
{
  TSyError err;
  int            itemID= atoi( newID.c_str() );
  if (itemID==0) itemID= atoi( F_First       );

  TDBItem*      last;
//TDBItem*      newL;
//TDBItemField* actK;
//TDBItemField* actF;

  // parent element must be "" or exist
  err= ParentExist( "", aID->parent ); if (err) return err;

      newItemID= aID->item;
  if (newItemID.empty()) {
                         actL= this;
    while (true) { last= actL;
      if     (!ListNext( actL )) break;

      int itemNum= atoi( actL->itemID.c_str() );
      if (itemNum>=itemID)     itemID= itemNum+1; // assign largest unique number
    } // while

    newItemID= IntStr( itemID );
    actL     = last;
  }
  else {
    actL= LastElem( this ); // get the last element
  } // if


  CreateItem( newItemID, aID->parent, actL );

  /*
  // create item root
              newL= new TDBItem;
              newL->fCB= fCB; // inherit callback
  actF=      &newL->item;
              newL->itemID  = newItemID;
              newL->parentID= aID->parent;
  actL->next= newL;
  ListNext( actL );

  int n= 0;        actK= &item;
  while (ListNext( actK )) { // number of elements according to the key
    MakeField    ( actF, "", NULL, actL->len );
    n++;
  } // while

  actL->len--; // termination is included already
  */

  return LOCERR_OK;
} // CreateEmptyItem


TSyError TDBItem::CreateEmptyItem( cAppCharP aParentID, string &newItemID, TDBItem* &actL )
{
  ItemID_Struct a; a.item  =  const_cast<char *>("");
                   a.parent= (char*)aParentID;
  return CreateEmptyItem      ( &a, newItemID, actL );
} // CreateEmptyItem


TSyError TDBItem::CreateEmptyItem( string &newItemID, TDBItem* &actL ) {
  return CreateEmptyItem( "", newItemID, actL );
} // CreateEmptyItem


/*!
 * Check, if <aKey> can be interpreted as number
 */
static bool NumKey( cAppCharP aKey, int &iKey )
{
         iKey= atoi( aKey );
  return iKey!=0 || strcmp( aKey,"0")==0;
} // NumKey


/* Update the specific field <fKey> with the new value <fVal>.
 * Create the whole bunch of missing keys, if not yet available.
 */
TSyError TDBItem::UpdateField( void* aCB, cAppCharP fKey,
                                          cAppCharP fVal, TDBItem* hdI, bool asName )
{
  TSyError err= DB_NotFound;
  char*    ts;
  string   s;
  int      iKey= 0, iLast= -1; // start value if data base is empty

  TDBItemField*  actK= &item; // the key
  TDBItemField* lastK;
  TDBItemField*  actI= &hdI->item;
  TDBItemField* lastI;

  if (aCB==NULL) aCB= fCB; // get the default callback, if not overridden by <aCB>

  // check, if the specific element is already existing
  while (true) {
    lastK=         actK;
    if (!ListNext( actK )) break;

    lastI=         actI; // a not existing field will be created (empty)
    if (!ListNext( actI )) {
                   actI= lastI;
      MakeField  ( actI, "", NULL, hdI->len );
    } // if

    if (strcmp( fKey,actK->field.c_str())==0) {
      int oldLen=    actI->field.length();
      int newLen= strlen( fVal ) + ArrLen( fKey,fVal );

      hdI->len  += newLen-oldLen; // adapt the whole length
      actI->field= fVal;          // and assign value of the new field
      err= LOCERR_OK;             // the new value is assigned, everything is ok
      fChanged= true;             // must be marked for a change
      break;
    } // if
  } // while

  // -----------------------------------------------------
  if (err && !asName) {
    if (!lastK->field.empty()) iLast= atoi( lastK->field.c_str() );
    if (!NumKey( fKey,iKey )) err= DB_NotFound; // not a valid key
  } // if

  if (err!=DB_NotFound) {
    s= KeyAndField( fKey,fVal );

    // create name info
    string vv= hdI->c_str();
    if   (!vv.empty()) vv= " " + Parans( vv ) + " ";

    // create debug info <eInfo>
    string   eInfo;
    if (err) eInfo= " err=" + IntStr( err );

    DEBUG_Exotic_DB( aCB, MyDB,"UpdateField", "%s'%s'%s",
                          vv.c_str(), s.c_str(), eInfo.c_str() );
    return err;
  } // if

  if (asName) iKey= iLast+1; // create just one element

  // now add the missing key fields
  int  i;
  for (i= iLast+1; i<=iKey; i++) {
    string tmp= IntStr( i );
    if       (asName) ts= (char*)fKey;
    else              ts= (char*)tmp.c_str();
    MakeField( lastK, ts, NULL, len ); // create a new key element
  } // for

  // and the same for each item
  TDBItem*         actV= this;
  while (ListNext( actV )) {
    actI=         &actV->item;
    while (true) {
      lastI= actI;
      if (!ListNext( actI )) {
        for (i= iLast+1; i<=iKey; i++) MakeField( lastI, "", NULL, actV->len );
        break;
      } // if
    } // loop
  } // while

  // the task is not yet done => do it again (recursively)
  return UpdateField( aCB, fKey, fVal, hdI, asName );
} // UpdateField



TSyError TDBItem::Field( cAppCharP fKey, TDBItem* hdI, string &s )
{
  TDBItemField* actK=      &item;
  TDBItemField* actF= &hdI->item;

  while (ListNext( actK,actF )) {
    if    (strcmp( actK->field.c_str(),fKey )==0) {
      s=           actF->field.c_str();
      return LOCERR_OK;
    } // if
  } // while

  return DB_NotFound;
} // Field


static void ReplaceArrElem( string &s, int n, string arrElem )
{
  int  pos, v= 0;
  bool fnd;

  int    i= n;
  while (true) {                  pos= v;
              v= s.find( ARR_Sep, pos );
         fnd= v!=(int)string::npos;
    if (!fnd) v= s.length();

    if (i<=0) break;

    if (!fnd) {
      if (arrElem.empty()) return; // adding not needed
      s+= ARR_Sep;
    } // if

    v++; i--;
  } // while

  s= s.substr( 0, pos ) + arrElem + s.substr( v, s.length()-v );
} // ReplaceArrElem



static void CutEmptyEnd( string &s )
{
  while (!s.empty()) {
    uInt32                last= s.length()-1;
    if (s.rfind( ARR_Sep, last )!=last) break; // last char ?
    s=  s.substr     ( 0, last );              // if yes, cut it
  } // while
} // CutEmptyEnd


static int ArrIndex( cAppCharP q, cAppCharP qN )
{
  int n= 0;

  if (qN) {
    string     s;
               s.assign( q, (unsigned int)( qN-q ) );
    CutBracks( s );
    n= atoi  ( s.c_str() );
  } // if

  return n;
} // ArrIndex


void TDBItem::Array_TDB( cAppCharP &q, cAppCharP aKey, TDBItem* hdI, string &aVal )
{
  cAppCharP qR;
  cAppCharP qN;
  cAppCharP qA;
  bool firstElem= true; // true during first element handling
  string s;             // temporary local string
  int    n;

//aVal= "";             // init string
//string            org;
  Field( aKey, hdI, aVal ); // as it is now

  while (*q!='\0') {
    qN= strstr( q,":" );
    qA= strstr( q,"[" );

    if (!qN)          break; // no more regular elements
    if (!qA || qN<qA) break; // this is not an array element

    if (!firstElem) {
          s.assign( q, (unsigned int)( qA-q ) );
      if (!(s==aKey)) break;

    //if (strcmp( aKey, s.c_str() )!=0) break; // not the same key
    } // if

    n= ArrIndex( qA,qN ); // get the index of this field
    if  (qN)     q= qN+1;

    // get the string in-between ':' and '\r' or '\n'
         qN= strstr( q,"\n" );
    if (!qN) qN= (cAppCharP)q + strlen( q );
         qR= qN-1;
    if (*qR!='\r') qR= qN;

    s.assign( q, (unsigned int)( qR-q ) );
    /*
    if (!firstElem) aVal+= ARR_Sep; // separator between elements, even if elements = ""
    aVal+= s;
    */

    ReplaceArrElem( aVal, n, s );
    CutEmptyEnd   ( aVal );

    q= qN; if (*q!='\0')  q++;
    firstElem= false;
  } // while
} // Array_TDB



TSyError TDBItem::UpdateFields( void* aCB, cAppCharP aItemData, TDBItem* hdI, bool asName,
                                           cAppCharP aNewToken )
{
  TSyError  err= LOCERR_OK;
  cAppCharP q  = aItemData;
  cAppCharP qR;
  string    fKey, fVal;
  int       iKey;

  if (!hdI)  { hdI= this; ListNext( hdI ); } // special case, if only one item
  if (!asName) hdI->fToken= aNewToken;

  while (*q!='\0') { // break the string <key>':'<value>'\n' into key and value
    cAppCharP qN= strstr( q,  StdPattern ); // normal     notation
    cAppCharP qA= strstr( q,ArrayPattern ); // try array  notation
  //cAppCharP qB= strstr( q, BlobPattern ); // try blob   notation
  //cAppCharP qT= strstr( q,   TZPattern ); // try tzname notation
    cAppCharP qS= strstr( q,         ";" ); // semicolon  notation

    bool isArr = qA && (!qN || qA<qN); // available and before ":"
    if  (isArr)  qN= qA;     if (!qN) { err= DB_Fatal; break; }

  //bool isBlob= qB && (!qN || qB<qN); // available and before ":"
  //if  (isBlob) qN= qB;     if (!qN) { err= DB_Fatal; break; }

  //bool isTZ  = qT && (!qN || qT<qN); // available and before ":"
  //if  (isTZ)   qN= qT;     if (!qN) { err= DB_Fatal; break; }

    bool isSemi= qS && (!qN || qS<qN); // available and before ":"
    if  (isSemi) qN= qS;     if (!qN) { err= DB_Fatal; break; }

    fKey.assign( q, (unsigned int)( qN-q ) );
  //q= qN; if (!isBlob && !isArr && !isTZ) q++;
    q= qN; if (!isArr  && !isSemi) q++;

    if  (isArr) {
      Array_TDB( q, fKey.c_str(), hdI, fVal );
    }
    else {       qN= strstr   ( q,"\n" ); // allow "\r\n" (what we expect), and also "\n"
      if (!qN)   qN= (cAppCharP)q + strlen( q );
           qR=   qN-1;
      if (*qR!='\r') qR= qN;
      fVal.assign( q, (unsigned int)( qR-q ) );
      q= qN; if (*q!='\0')  q++;
    } // if

    asName|= !NumKey( fKey.c_str(), iKey ); // must interpret them as names !!
    err= UpdateField( aCB, fKey.c_str(),fVal.c_str(), hdI, asName );
    if (err) break;
  } // while

  fChanged= true;
  return err;
} // UpdateFields



bool TDBItem::SameField( cAppCharP fKey,
                         cAppCharP fVal, TDBItem* hdI )
{
  string s;
  TSyError err= Field( fKey, hdI, s );
  return  !err && strcmp( s.c_str(), fVal )==0;
} // SameField



void TDBItem::Disp_ItemData( void* aCB, cAppCharP title, cAppCharP aTxt, cAppCharP aItemData )
{
  DEBUG_Block   ( aCB, title, "", aTxt );
  DEBUG_        ( aCB, "%s", aItemData );
  DEBUG_EndBlock( aCB, title );
} // Disp_ItemData


void TDBItem::Disp_Items( void* aCB, cAppCharP txt, bool allFields,
                                     cAppCharP specificItem )
{
  cAppCharP DIT= "-Disp_Items"; // collapsed display with '-' at the beginning

  string s= txt;
  if   (!s.empty()) s= "call=" + s;

  if (aCB==NULL) aCB= fCB; // get the default callback, if not overridden by <aCB>
  DEBUG_Block  ( aCB, DIT, itemID.c_str(), s.c_str() ); // hierarchical log

  TDBItem*         actI= this;
  while (ListNext( actI )) {         // for each item do ...
    if       (*specificItem!=0 &&
       strcmp( specificItem,actI->itemID.c_str() )!=0 ) continue;

    s= Token_Str( actI->fToken );
    DEBUG_( aCB, "%s= (%s) %s", this->c_str(), actI->c_str(), s.c_str() );

    TDBItemField* actK=       &item; // the key identifier
    TDBItemField* actF= &actI->item; // the item current field

    cAppCharP f;
    while (ListNext( actK )) {       // for each element do ...
      if  (ListNext( actF )) f= actF->field.c_str();
      else                   f= "";  // "<???>";

      if (allFields || !actF->field.empty()) { // display only under conditions
        s= KeyAndField( actK->field.c_str(), f );
        DEBUG_( aCB, "%s", s.c_str() );
      } // if
    } // while
  } // while

  DEBUG_EndBlock( aCB, DIT );
} // Disp_Items



static void AddKey( void* aCB, bool withKey, cAppCharP qA,
                                             cAppCharP qR,
                    int n, TDBItemField* &actF, int &len )
{
  if (withKey)
    MakeField( actF, qA,qR,            len, true );
  else {                  // just numbering, starting with "0"
    string           tmp= IntStr( n-2 );
    MakeField( actF, tmp.c_str(),NULL, len, true );
    DEBUG_Exotic_DB( aCB, MyDB,"AddKey", "'%s'", tmp.c_str() );
  } // if
} // AddKey


TSyError TDBItem::LoadDB( bool withKey, cAppCharP aPrefix, void* aCB )
{
  cAppCharP LDB= "-LoadDB"; // collapsed display with '-' at the beginning
  TSyError err= LOCERR_OK;

  char   ch, prv= '\0';
  char*  q;
  char*  qA;
  char*  qR;
  char*  qC;

  // create root structure
  TDBItem*      actL= this;
  TDBItem*      actI= NULL;
  TDBItem*      newL= NULL;
  TDBItemField* actF;
  TDBItemField* actT;
  bool          first= true;
  int           n= 0, nMax= 0;

  if (fLoaded) return LOCERR_OK;
  if (withKey) init( aPrefix, aCB );

  if (aCB==NULL) aCB= fCB; // get the default callback, if not overridden by <aCB>

  FILE* f= fopen( fFileName.c_str(),"rb" );
  if  (!f) err= DB_NotFound; /* empty DB */

  string s= "err=" + IntStr( err );
  string rslt;
  DEBUG_Block( aCB, LDB, fFileName.c_str(), s.c_str() ); // hierarchical log

  if (!err) {
    while (true) {      // loop ...
      bool is0D= false; // last line ended with 0D ?

      // get lines, make it compatible for Windows, Mac and Linux
      q= &ch;
      s= "";
      while (true) {
        if (fread( q, 1,1, f ) != 1) {
          ; // error ignored
        }
        
        if (feof( f )) break;

        if (*q=='\x0D') { is0D= true; break; }
        if (*q=='\x0A') {
          if     (!is0D) break;
          *q= prv; is0D= false;
        } // if

        s += ch;
        prv= ch;
      } // while
      if (s.empty() && feof( f )) break; // .. until end of file

      q= (char*)s.c_str();
      // remove possible UTF-8 lead-in
      if ((q[0] & 0xFF) == 0xEF &&
          (q[1] & 0xFF) == 0xBB &&
          (q[2] & 0xFF) == 0xBF) {
        q+=3; // skip UTF-8 lead-in
        DEBUG_Exotic_DB( aCB, MyDB,"", "UTF-8 lead-in skipped" );
      } // if

      if (*q=='\0') continue; // empty line ?
      ReplaceLoad( q, rslt ); // textdb specific => normal
      q=       (char*)rslt.c_str();

      DEBUG_Exotic_DB( aCB, MyDB,"", "line='%s'", q );

      // create index tree
      if (first) {
        actT= &actL->item;

        n= 0;    qA= q;
        while ( *qA!='\0') {
          qR=  strstr( qA,"\t" );
          if (!qR) qR= qA + strlen( qA );              // no tab to skip

          if (n>1)                    // the index itself is not a field
            AddKey( aCB, withKey, qA,qR, n, actT, actL->len );

          n++;
          if (*qR=='\0') break;
          qA=  qR+1;
        } // while

        nMax= n;    // keep it for later appending of additional elements
        actI= actL; // save it for later use
      } // if

      // create item elements
      if (!first || !withKey) {
        n= 0;    qA= q;
        while ( *qA!='\0') {
          qR=  strstr( qA,"\t" );
          if (!qR) qR= qA + strlen( qA ); // no tab to skip

          if (n==0) { newL= new TDBItem;  // create new item root
                      newL->fCB= fCB;     // inherit callback
            actF=    &newL->item;
            qC= strstr( qA,"," );
            if (!qC ||  qC>qR) qC= qR;

            /* separate itemID/parentID, if comma separator available */
                        newL->itemID.assign  ( qA, (unsigned int)(qC-qA) );
            if (qC!=qR) qC++;
                        newL->parentID.assign( qC, (unsigned int)(qR-qC) );
            actL->next= newL;
            ListNext  ( actL );
          }
          else if (n==1)  newL->fToken.assign( qA, (unsigned int)(qR-qA) );
          else {
            MakeField( actF, qA,qR, actL->len, true );
          //printf( "A: %3d\n", actL->len );
            actL->len+= ArrLen( "xx", actF->field.c_str() );
          //printf( "B: %3d\n", actL->len );
          } // if

          if (n>=nMax && actI!=NULL) {
            if (n>1) {
            //printf( "indexLenV=%d\n", actI->len );
              AddKey( aCB, withKey, "???",NULL, n, actT, actI->len );
            //printf( "indexLenN=%d\n", actI->len );
            } // if

            nMax++; // adapt the new limit
          } // if

          if (*qR=='\0') break;
          qA=  qR+1; n++;
        } // while
      } // if

      actL->len--;  // termination is included
    //printf( "C: %3d\n", actL->len );
    //printf( "D: '%s'\n", q );
      first= false; // first run is done
    } // while

    fclose( f );
    fLoaded = true;
    fChanged= false;
  } // if

  DEBUG_EndBlock( aCB, LDB );
  return err;
} // LoadDB


TSyError TDBItem::SaveDB( bool withKey, void* aCB )
{
  cAppCharP SDB= "-SaveDB"; // collapsed display with '-' at the beginning
  TSyError err= LOCERR_OK;

  if (!fChanged)         return LOCERR_OK; // nothing changed, no saving
  if (fFileName.empty()) return DB_NotFound;

  if (aCB==NULL) aCB= fCB; // get the default callback, if not overridden by <aCB>

  FILE* f= fopen( fFileName.c_str(),"wb" );
  if  (!f) err= DB_NotFound;

  string s= "err=" + IntStr( err );
  string rslt;
  DEBUG_Block( aCB, SDB, fFileName.c_str(), s.c_str() ); // hierarchical log

  if (!err) {
    fputs( "\xEF\xBB\xBF", f ); // UTF-8 lead-in
    DEBUG_Exotic_DB( aCB, MyDB,"","UTF-8 lead-in written" );

    string line;
    TDBItem*                    actI= this;
    while (withKey || ListNext( actI )) {
      line = actI->c_str();        // starting with the <itemID>[","<parentID>]
      line+= "\t";
      line+= actI->fToken.c_str(); // append the timestamp

      int n= 0;
      TDBItemField*      actF= &actI->item;                        // the item current field
      while   (ListNext( actF )) {            s= "";               // tab separated
        CStrToStrAppend( actF->field.c_str(), s, false, ARR_Sep ); // don't touch array separator
        ReplaceSave                         ( s.c_str(), rslt );   // normal => textdb specific

        line+= "\t" + rslt;
        n++;
      } // while

      int    ii= line.length(); // remove empty items at the end
      while (ii>0) {
        if (line[ --ii ]!='\t') break;
      }
      line= line.substr( 0, ii+1 );

      // now log and write the whole line
      DEBUG_DB( aCB, MyDB,"", "line='%s'", line.c_str() );
      fprintf( f, "%s\r\n", line.c_str() );
      withKey= false;
    } // while

    fclose( f );
    fChanged= false;
  } // if

  DEBUG_EndBlock( aCB, SDB );
  return err;
} // SaveDB


} /* namespace */
/* eof */
