/*
 *  File:    SDK_support.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *  Some SDK support utility functions for C++
 *
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef SDK_SUPPORT_H
#define SDK_SUPPORT_H

#include "sync_include.h"   // import some global things
#include <string>

namespace sysync {


/*! Search for separators */
bool SepFound( string name, string::size_type &pos, string sep= "!", bool backwards= false );


// ---- Bracket (LIB) support
/*! Returns true, if <name> belongs to an internal LIB.
 *  Returns false for DLL or JNI (Java Native Interface) access
 */
bool       IsLib( string  name );

/*! Returns true, if <name> is within brackets: "[aaa]" */
bool    InBracks( string  name );

/*! Cut brackets, if at beginning and end
 *  Returns true, if <name> is within brackets: "[aaa]"
 */
bool   CutBracks( string &name );

/*! Cut the brackets at beginning/end of <name> */
string  NoBracks( string  name );

/*! Add brackets at beginning/end of <name>
 *  if not yet there and if not empty string
 */
string AddBracks( string  name );


/*! Get plug-in's main path of <name>, e.g. "aaa!bbb" ==> "aaa" */
string Plugin_MainName( string  name );

/*! Get plug-in's sub  path of <name>, e.g. "aaa!bbb" ==> "bbb" */
string Plugin_SubName ( string  name );

/*! Get the two path's <sMain> and <sSub>
 *  Returns true, if sSub is not empty ""
 */
bool WithSubSystem  ( string  name, string &sMain, string &sSub );

/*! Returns true if <aContext> names contains " ADMIN" at the end
 *  The " ADMIN" part will be ut, if available
 */
bool IsAdmin( string &aContextName );



// ------ command line utility functions -------------------------------------
class CLine
{
  public:
    CLine() { fArgc= 0; }

    void InitOptions( int argc, char* argv[] );          // assign command line arguments
    void StrOpt     ( char opt, int  n, string &value ); // Get <n>th option string of <opt>
    void GetName              ( int  n, string &value );
    bool NextOpt    ( char opt, int &n, string &value ); // Get next  option, if available

    void IntOpt     ( char opt, int  n, uInt32 &value );
    void IntOpt     ( char opt, int  n, sInt32 &value );
    void IntOpt     ( char opt, int  n, uInt16 &value );
    void IntOpt     ( char opt, int  n, sInt16 &value );

    bool Opt_Found  ( char opt );                        // check for a specific option

  private:
    int    fArgc;
    char** fArgv;
}; // Cline



// ---- Capabilities support --------------------------------------------------------
/*! Add a fieldname with "no", "yes" or "both" attribute */
void   NoField( string &s, string fieldName );
void  YesField( string &s, string fieldName );
void BothField( string &s, string fieldName );

/*! Add the minimum supported version for the plugin to <s>
 *  NOTE: If \<internalMinVersion> is not specified or lower than the system's
 *        minimum version, the system's minimum version will be taken.
 *        If \<internalMinVersion> is higher, it will be directly returned.
 */
void MinVersion  ( string &s, CVersion internalMinVersion= 0 );

/*! Add sub system's version, if smaller than internal */
void SubVersion  ( string &s, CVersion subSysVersion );

/*! Sub system's name; additionally this is a capability separator
 *  So all following capabilities belong to the sub system
 */
void SubSystem   ( string &s, string subSysName );

/*! Add the manufacturer's <name> to \<s> */
void Manufacturer( string &s, string name );

/*! Add a short description <desc>, what the module is doing to <s> */
void Description ( string &s, string desc );

/*! Add a <guid> string to <s> */
void GuidStr     ( string &s, string guidStr );

/*! Add a <plugin> string to <s> */
void BuiltIn     ( string &s, string  plugin );

/*! Add global context <gContext> information to <s> */
void GContext    ( string &s, GlobContext* gContext );

/*! Add capability error */
void CapaError   ( string &s, TSyError err );



// ---- utility functions ----------------------------------------------------------
/* Allocate local memory for an item */
void MapAlloc  (  MapID rslt,  cMapID mID );
void ItemAlloc ( ItemID rslt, cItemID aID );
void BlockAlloc( appPointer *newPtr, appPointer bPtr, uInt32 bSize );

/*! Convert \<str> into \<rslt> string for Save/Load operations */
void ReplaceSave( cAppCharP str, string &rslt );
void ReplaceLoad( cAppCharP str, string &rslt );

/*! Get int value <i> as string */
string  IntStr( sInt32 i );

/*! Get ptr address value <a> as string */
string  RefStr( void*  a, bool asHex= true );

/*! Get byte/hex/longhex value <h> as string */
string ByteStr( uInt8  h );
string  HexStr( uInt16 h );
string LHexStr( uInt32 h );

/*! Get <ch> as int value */
int HexNr( char ch );

/*! Get string as hex value */
void* LHexRef( string v );
long  LHex   ( string v );

/*! Get boolean as const char string */
cAppCharP Bo ( bool b );

/*! Get boolean as short const char string "v"=true / "o"=false */
cAppCharP BoS( bool b );

/*! Get <v> as version string Va.b.c.d */
string   VersionStr( CVersion v );

/*! Get <v> as version long 0x0a0b0c0d */
CVersion VersionNr (   string v );


/* true, if <s> starts/ends with <cmp> */
bool SameBegin( string s, string cmp );
bool SameEnd  ( string s, string cmp );


/*! Get a string within single quotes */
string Apo    ( string s );

/*! Get a string within parans */
string Parans ( string s );

/*! Get a string within brackets */
string Bracks ( string s );

/*! Token separation */
void CutCh    ( string &s );
void CutLSP   ( string &s );
bool NextToken( string &s, string &nx, string sep=" " );


/*! Display a line with <maxLen> and removed '\n' */
string LineConv( string str, uInt32 maxLen= 0, bool visibleN= false );


/*! Get (\<mID.localID>,\<mID.remoteID> \<mID.flags> \<mID.ident>) string */
string MapID_Info ( cMapID  mID );

/*! Get (\<aID.item>,\<aID.parent>) string */
string ItemID_Info( cItemID aID, string aName= "" );

/*! Concatenate <path1>,<path2>,[<path3>] with OS specific separator in-between */
string ConcatPaths( string path1, string path2,               bool isJava= false );
string ConcatPaths( string path1, string path2, string path3, bool isJava= false );

/*! Notation: <name1>"!"<name2> */
string ConcatNames( string name, string subName, string sep= "!" );



// -----------------------------------------------------------------------------------
/*! Get <aKey>/<aNth> field from <aDat>. <value> will be returned */
bool GetField   ( string  aDat, string aKey, string &value );

/*! Remove <aKey> field from <aDat>. <removedValue> will be returned */
bool RemoveField( string &aDat, string aKey, string &removedValue );

/*! Check if <aKey> is -                "yes"/"true/"both" ( <isYes> = true )
 *                     - not available or NOT "no"/"false" ( else )
 */
bool FlagOK  ( string aDat, string aKey, bool isYes= false );

/*! Check if <aKey> is "both" */
bool FlagBoth( string aDat, string aKey );

/*! filter out <aFilter> item fields from <aItemData>. Result is <aDat> */
void FilterFields( string &aDat, string aFilter );



/* ---------- global context handling ------------------------ */
bool GlobContextFound( string dbName, GlobContext* &g );


/* ---------- UI call-in: tunnel itemKey --------------------- */
TSyError  OpenTunnel_ItemKey( TunnelWrapper* tw );
TSyError CloseTunnel_ItemKey( TunnelWrapper* tw );



} // namespace
#endif /* SDK_SUPPORT */
/* eof */
