/*
 *  File:    dbitem.h
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

#ifndef DBITEM_H
#define DBITEM_H

#include "sync_include.h"   // import some global things


#ifndef SYSYNC_ENGINE
  #include "stringutil.h" // local implementation of CStr <=> Str conversions
#endif


#define F_First "10000"   // start number for item name (will be incremented)



namespace sysync {
// ---------- list templates ----------------------------------
//! go to the next element of <act>. Returns true, if available
template <class T> bool ListNext( T* &act )
{
  if (act) act= static_cast< T* >( act->next );
  return   act!=NULL;
} // ListNext


//! go to the next element of <act>. Returns true, if available
//  the same with 2 parameters
template <class T1, class T2> bool ListNext( T1* &act1, T2* &act2 ) {
  return ListNext( act1 ) && ListNext( act2 );
} // ListNext


template <class T> bool ListBack( T* &act, T* li )
// go to the previous element of <act>. Returns true, if available
{
  T* last;
  if (act==li) return false;

  do {         last= li;
  } while (ListNext( li ) && li!=act);

  act= last; return true;
} // ListBack


template <class T> T* LastElem( T* li )
// go to the last element of <act>.
{ T*        act= NULL;
  ListBack( act,li );
  return    act;
} // LastElem


template <class T> void DeleteNext( T* &next, void* aCB= NULL,
                                    const char* s= "", bool dbg= false )
// delete the rest of the list <act>
{
  #ifndef ANDROID
  // no rtti support for Android
  if (dbg) DEBUG_Exotic_DB( aCB, "","Delete", "%s (%s)", typeid( T ).name(), s );
  #endif

  if      (next) {
    delete next;
           next= NULL; // don't let it undefined
  } // if
} // DeleteNext


template <class T> void MakeObj( T* &act )
// create an element at position <act> of the given list
{
  T* sv    = static_cast< T* >( act->next ); // save it for linking later again
  act->next= static_cast< T* >( new T );     // create an element of the given type
  ListNext ( act );
  act->next= sv;
} // MakeObj



// ---------- list class --------------------------------------
class TList
{
  public:
             TList() { next= NULL; }         // constructor
    virtual ~TList() { DeleteNext( next ); } //  destructor  (if not yet otherwise deleted)
    TList*    next;                 // reference to the next element
}; // TList


// ---------- item field class --------------------------------
class TDBItemField : public TList
{
   typedef TList inherited;
  public:
             TDBItemField() { next= NULL; }                              // constructor
    virtual ~TDBItemField() { TDBItemField** act= (TDBItemField**)&next; // destructor
                              DeleteNext  ( *act ); }

    string field; // the string field
}; // TDBItemField


// ---------- item class -------------------------------------
class TDBItem : public TDBItemField
{
    typedef TDBItemField inherited;
  public:
             TDBItem( void* aCB= NULL ) { len= 1; fCB= aCB; fLoaded = false;
                                                            fChanged= false; } // constructor
    virtual ~TDBItem() { TDBItem**    act= (TDBItem**)&next;                   //  destructor
                         DeleteNext( *act, fCB, c_str(), true ); }

    TDBItemField item;      // the header element
    int          len;       // length of all fields of this item
    bool         fLoaded;   // indicates, if already loaded
    bool         fChanged;  // indicates, if already changed

    string         itemID;
    string       parentID;
    const char*  c_str();   // <itemID>[","<parentID>]

    void         init( cAppCharP aItemID, cAppCharP aParentID, void* aCB= NULL, TDBItem* aNext= NULL ); // initalize
    void         init( cAppCharP aItemID,                      void* aCB= NULL, TDBItem* aNext= NULL );

    string       fFileName; // the (full) file name for LoadDB/SaveDB

  private:
    string       fID;       // internal memory space for c_str()
    void         Array_TDB      ( cAppCharP &q, cAppCharP aKey, TDBItem* hdI, string &aVal );

  public:
    string       fToken;    // the timestamp token (as string)
    void*        fCB;       // callback structure

    // two different versions, overloaded
    TSyError     GetItem        ( cItemID         aID, TDBItem* &actL, bool first= true ); // get a specific item
    TSyError     GetItem        ( cAppCharP   aItemID, TDBItem* &actL, bool first= true );

    TSyError     GetItem_2      ( cAppCharP   aItemID,
                                  cAppCharP   aField2, TDBItem* mpL,
                                                       TDBItem* &actL ); // get item with specific field 0

    // three different versions, overloaded
    TSyError     DeleteItem     ( cItemID         aID );
    TSyError     DeleteItem     ( cAppCharP   aItemID );
    TSyError     DeleteItem     ( TDBItem*       actL );

    TSyError     ParentExist    ( cAppCharP   aItemID, cAppCharP aParentID );

    void         CreateItem     ( string    newItemID, string   parentID, TDBItem* &actL );
    // three different versions, overloaded
    TSyError     CreateEmptyItem( ItemID          aID, string &newItemID, TDBItem* &actL, string itemID= F_First );
    TSyError     CreateEmptyItem( cAppCharP aParentID, string &newItemID, TDBItem* &actL );
    TSyError     CreateEmptyItem                     ( string &newItemID, TDBItem* &actL );

    TSyError     UpdateField    ( void* aCB, cAppCharP fKey,
                                             cAppCharP fVal,      TDBItem* hdI= NULL, bool asName= true );
    TSyError     UpdateFields   ( void* aCB, cAppCharP aItemData, TDBItem* hdI= NULL, bool asName= true,
                                             cAppCharP aNewToken= "" );

    TSyError     Field          ( cAppCharP   fKey,                      TDBItem* hdI, string &s );
    bool         SameField      ( cAppCharP   fKey,     cAppCharP fVal,  TDBItem* hdI );
    void         Disp_ItemData  ( void* aCB,            cAppCharP title, cAppCharP attrTxt,
                                                        cAppCharP aItemData );
    void         Disp_Items     ( void* aCB= NULL,      cAppCharP txt= "",
                                  bool allFields= true, cAppCharP specificItem= "" );

    // load and save the database from/to file
    TSyError     LoadDB         ( bool withKey, cAppCharP aPrefix= "", void* aCB= NULL );
    TSyError     SaveDB         ( bool withKey,                        void* aCB= NULL );
}; // TDBItem


// ---- utility functions ----------------------------------------------------------
/*! Create an item field */
void MakeField( TDBItemField* &act, cAppCharP field,
                                    cAppCharP fieldEnd, int &len, bool convert= false );


/*! Create a UTC token for time comparisons */
string CurrentTime( int secsOffs= 0, bool dateOnly= false );

/*! Compare time of ISO8601 <aToken> with <aLastToken>/<aResumeToken> */
int CompareTokens( string aToken, string aLastToken, string aResumeToken );


/*! combine key and field in a string */
string KeyAndField( cAppCharP     sKey,
                    cAppCharP     sField );
string KeyAndField( TDBItemField* actKey,
                    TDBItemField* actField );

/*! add some <aAdd> item fields to the beginning of <aItemData>. Result is <aDat>.
 *  If <aAdd> contains %1, it will be replaced by <param1>.
 *   "    "      "     %2, "    "   "     "     " <param2>
 */
void AddFields( string &aDat, string aAdd, cAppCharP param1= "",
                                           cAppCharP param2= "",
                                           cAppCharP param3= "",
                                           cAppCharP param4= "",
                                           cAppCharP param5= "" );


} /* namespace */
#endif /* DBITEM_H */
/* eof */
