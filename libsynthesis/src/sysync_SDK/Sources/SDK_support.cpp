/*
 *  File:    SDK_support.cpp
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *  Some SDK support utility functions for C++
 *
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 */

#include "sync_include.h"  // import some global things
#include "sync_dbapidef.h"
#include "SDK_util.h"
#include "SDK_support.h"


#ifdef __cplusplus
  namespace sysync {
#endif

#define FLen 30 // max length of (internal) item name



// --- plugin name handling ----------------------------------------
//! recognize separation
bool SepFound( string name, string::size_type &pos, string sep, bool backwards )
{
  if (backwards) pos= name.rfind( sep, name.length()-1 );
  else           pos= name.find ( sep, 0 );
  return         pos!=string::npos;
} // SepFound



// returns true if in brackets: "[aaa]"
bool InBracks( string name )
{
  string::size_type o, c;
  return SepFound( name, o, "["        ) && o==0
      && SepFound( name, c, "]",  true ) && c==name.length()-1;
} // InBracks



// returns true if twice in brackets: "[[aaa]]"
static bool InDBracks( string name )
{
  string::size_type o, c;
  return SepFound( name, o, "[["       ) && o==0
      && SepFound( name, c, "]]", true ) && c==name.length()-2;
} // InDBracks



bool CutBracks( string &name )
{
  bool   ok= InBracks( name );
  if    (ok)     name= name.substr( 1, name.length()-2 ); // cut the brackets
  return ok;
} // CutBracks



/*! Returns true, if <name> belongs to an internal LIB.
 *  Returns false for DLL access
 *  The following notations are recognized as LIB:
 *    ""
 *    "[aaa]"
 *    "[aaa!bbb]"
 *    "[aaa x]"
 *    "[aaa x!bbb]"
 *
 *    "[aaa]!bbb"
 *    "[aaa x]!bbb"
 *
 *    "[aaa] x"
 *    "[aaa] x!bbb"
 */
bool IsLib( string name )
{
  if (name.empty() || InBracks( name )) return true;  // empty or embraced with '[' ']'

  string::size_type o, c;
  return SepFound( name, o, "["      ) &&         o==0
      && SepFound( name, c, "]",true ) && ( name[ c+1 ]=='!' ||
                                            name[ c+1 ]==' ' );
} // IsLib



// Cut the brackets at begin/end of <name>, if available
string NoBracks( string name )
{
  string p;

  string::size_type pos;
  if (SepFound( name, pos )) { // check the case "[aaa]!bbb" first
                   p=          Plugin_MainName( name );
    if     (IsLib( p )) {
      p= NoBracks( p ) + "!" + Plugin_SubName ( name );
      return       p;
    } // if
  } // if

  if (SepFound( name, pos, " " )) { // check the case "[aaa] xx"
    if (name[     0 ]=='[' &&
        name[ pos-1 ]==']' &&
        NextToken( name, p )) {
      p= NoBracks( p ) + " " + name;
      return       p;
    } // if
  } // if

  // check now for "[aaa]" or "[aaa!bbb]"
  CutBracks( name );
  return     name;
} // NoBracks



/*! Add brackets at begin/end of <name>, if not yet there */
string AddBracks( string name )
{
  if   (!name.empty() && !InBracks( name )) name= Bracks( name );
  return name;
} // AddBracks



static string Plugin_PartName( string name, bool asSub )
{
  string::size_type pos, o, c;
  if (!SepFound( name, pos )) { // at least one '!' must be there
    if (asSub) return "";
    else       return name;
  } // if

  int nBridge= 0;
  if (SepFound( name, o, "[" ) && o==0) {
    for (c= pos+1; c<name.length(); c++) {
      if (name[ c ]=='[') nBridge--;
      if (name[ c ]==']') nBridge++;
    } // for

    if (nBridge<0) nBridge= 0; // adjustment problems
  } // if

  if (asSub) {
    name= name.substr( pos+1, name.length()-pos-1 );
    while (nBridge-- > 0) name= '[' + name;
  }
  else {
    name= name.substr( 0, pos );
    while (nBridge-- > 0) name= name + ']';
  } // if

  while (InDBracks( name )) CutBracks( name );
  return            name;
} // Plugin_PartName



//! the part before and after '!'
string Plugin_MainName( string name ) { return Plugin_PartName( name, false ); }
string Plugin_SubName ( string name ) { return Plugin_PartName( name, true  ); }

bool WithSubSystem( string name, string &sMain, string &sSub )
{
  sMain= Plugin_MainName( name );
  sSub = Plugin_SubName ( name );
  return !sSub.empty();
} // WithSubSystem


bool IsAdmin( string &aContextName )
{
  uInt32 fLen= aContextName.length();
  uInt32 fPos= fLen - strlen( ADMIN_Ident );

  bool   asAdmin= aContextName.rfind( ADMIN_Ident,fLen )==fPos;
  if    (asAdmin) aContextName=   aContextName.substr( 0, fPos );
  return asAdmin;
} // IsAdmin



// ------ command line utility functions -------------------------------------
// assign command line arguments
void CLine::InitOptions( int argc, char* argv[] )
{
  fArgc= argc; // get copies
  fArgv= argv;
} // InitOptions


// Get <n>th option string of <opt>
void CLine::StrOpt( char opt, int n, string &value )
{
  int p= 0;
  for (int i=1; i<fArgc; i++) {
    if    (i+n >= fArgc) return;
    char*    s=   fArgv[ i+p ];

    if     (*s=='-') {                   // search for options
      do {   s++;
        if (*s==opt) {                   // yes, that's the one we're looking for
          bool ok= true;
          int act= i+p+1;
          int lim= i+n;

          for (int j=act; j<=lim; j++) {
                   s= fArgv[ j ];
            if   (*s=='-') {             // no other options allowed
                   s++;
              if (*s<'0' || *s>'9') {    // negative numbers ARE allowed
                ok= false; break;
              } // if
            } // if

            p++;
          } // for

          if (ok) {
            value= s; return;            // it's ok -> assign
          } // if
        } // if
      } while (*s!='\0' && *s!=' ');
    } // if
  } // for
} // StrOpt


void CLine::GetName( int n, string &value )
{
  int j= 0;
  
  for (int i=1; i<fArgc; i++) {
    string s=     fArgv[ i ];
    if    (s.find( '-',0 )!=0) { // search for params
          j++;
      if (j==n) { value= s; return; }
    } // if
  } // for
} // GetName



bool CLine::NextOpt( char opt, int &n, string &value )
{
                     value= "";
  StrOpt( opt,  n+1, value );

  if (value.empty()) return false;
  else        { n++; return true; }
} // NextOpt


void CLine::IntOpt( char opt, int n, uInt32 &value )
{
  string         s= IntStr( value );
  StrOpt( opt,n, s );
  value=   atoi( s.c_str() );
} // IntOpt


void CLine::IntOpt( char opt, int n, sInt32 &value )
{
  string         s= IntStr( value );
  StrOpt( opt,n, s );
  value=   atoi( s.c_str() );
} // IntOpt


void CLine::IntOpt( char opt, int n, uInt16 &value )
{
  string         s= IntStr( value );
  StrOpt( opt,n, s );
  value=   atoi( s.c_str() );
} // IntOpt


void CLine::IntOpt( char opt, int n, sInt16 &value )
{
  string         s= IntStr( value );
  StrOpt( opt,n, s );
  value=   atoi( s.c_str() );
} // IntOpt


// check for a specific option
bool CLine::Opt_Found( char opt )
{
  for (int n=1; n<fArgc; n++) {
    char*    s=   fArgv[ n ];
    if     (*s=='-') {
      do {   s++;
        if (*s==opt) return true;
      } while (*s!='\0' && *s!=' ');
    } // if
  } // for

  return false;
} // Opt_Found




// ---- Capabilities support --------------------------------------------------------
static void AddCapa( string &s, string fieldName, string fieldValue )
{
  if (fieldValue.empty()) return; // only with a valid <fieldValue>

  if (!s.empty()) s+= '\n';       // add a separator (in-between)

  s+=       fieldName;            // and the new identifier
  s+= ':' + fieldValue;
} // AddCapa


// Add a fieldname with "no" attribute
void   NoField( string &s, string fieldName ) { AddCapa( s, fieldName,  "no" ); }

// Add a fieldname with "yes" attribute
void  YesField( string &s, string fieldName ) { AddCapa( s, fieldName, "yes" ); }

// Add a fieldname with "both" attribute
void BothField( string &s, string fieldName ) { AddCapa( s, fieldName,"both" ); }


// Add the minimum supported version for the plugin to <s>:
// ===> <resumeToken> MUST be sent at StartDataRead ==> V1.0.6.X
void MinVersion( string &s, CVersion internalMinVersion )
{
  CVersion minV= VP_ResumeToken;            // At least V1.0.6.X for all ...
  if      (minV<=internalMinVersion)        // ... or even higher
           minV= internalMinVersion;

  string                      value;
  GetField( s, CA_MinVersion, value );

  if             (!value.empty()) {         // already a min version defined ?
    if (VersionNr( value )>=minV) return;   // it's perfectly fine already -> done

    RemoveField( s, CA_MinVersion, value ); // <value> must be replaced, remove it
  } // if

  AddCapa( s, CA_MinVersion, VersionStr( minV ) );
} // MinVersion


// Add sub system's version, if smaller than internal
void SubVersion( string &s, CVersion subSysVersion )
{
  if                   (Plugin_Version( 0 )<subSysVersion)
    AddCapa( s, CA_SubVersion, VersionStr ( subSysVersion ) );
} // SubVersion


// Separator for the sub system
void SubSystem  ( string &s,   string subName ) {
  AddCapa( s, CA_SubSystem, NoBracks( subName ) );
} // SubSystem


// Compose capability string
void Manufacturer( string &s, string    name ) { AddCapa( s, CA_Manufacturer, name ); }
void Description ( string &s, string    desc ) { AddCapa( s, CA_Description,  desc ); }
void GuidStr     ( string &s, string guidStr ) { AddCapa( s, CA_GUID,      guidStr ); }
void BuiltIn     ( string &s, string  plugin ) { AddCapa( s, CA_Plugin,     plugin ); }


// Add global context <gContext> information to <s>
void GContext( string &s,              GlobContext* gContext ) {
  if (gContext) AddCapa( s, CA_GlobContext, RefStr( gContext ) );
} // CContext


// Add capability error, if <err>
void CapaError( string &s,          TSyError err ) {
  if (err)      AddCapa( s, CA_Error, IntStr(err) );
} // CapaError



// ---- utility functions ----------------------------------------------------------
// Allocate local memory for an item
void MapAlloc( MapID rslt, cMapID mID )
{
  rslt->localID = StrAlloc( mID->localID  );
  rslt->remoteID= StrAlloc( mID->remoteID );
  rslt->flags   =           mID->flags;
  rslt->ident   =           mID->ident;
} // MapAlloc

// Allocate local memory for an item
void ItemAlloc( ItemID rslt, cItemID aID )
{
  rslt->item  = StrAlloc( aID->item   );
  rslt->parent= StrAlloc( aID->parent );
} // ItemAlloc

void BlockAlloc( appPointer *newPtr, appPointer bPtr, uInt32 bSize )
{
          *newPtr= malloc( bSize );
  memcpy( *newPtr, bPtr,   bSize );
} // BlockAlloc



// -----------------------------------------------------------
// Get int value <i> as string
string IntStr( sInt32 i )
{
  char      f[ FLen ];
  sprintf ( f, "%d", (int)i );
  string s= f;
  return s;
} // IntStr


string RefStr( void* a, bool asHex )
{
  char f[ FLen ];

  #if __WORDSIZE == 64
    if (asHex) sprintf( f, "%016llX", (unsigned long long)a );
    else       sprintf( f,    "%lld",     (long long int )a );
  #else
    if (asHex) sprintf( f,    "%08X",       (unsigned int)a );
    else       sprintf( f,      "%d",                (int)a );
  #endif

  string s= f;
  return s;
} // RefStr


static string HexConv( uInt32 h, cAppCharP format )
{
  char      f[ FLen ];
  sprintf ( f, format, h );
  string s= f;
  return s;
} // HexConv

/*! Get byte/hex/longhex value <h> as string */
string ByteStr( uInt8  h ) { return HexConv( h, "%02X" ); }
string  HexStr( uInt16 h ) { return HexConv( h, "%04X" ); }
string LHexStr( uInt32 h ) { return HexConv( h, "%08X" ); }


/*! Get ch as int value */
int HexNr( char ch )
{
  int                       i= 0;
  if ( ch>='0' && ch<='9' ) i= ch-'0';
  if ( ch>='a' && ch<='f' ) i= ch-'a'+10;
  if ( ch>='A' && ch<='F' ) i= ch-'A'+10;
  return                    i;
} // HexNr


/*! Get string as hex value */
void* LHexRef( string v )
{
  #if __WORDSIZE == 64
    long long  a=  0;
    const int mx= 16;
  #else
    long       a=  0;
    const int mx=  8;
  #endif

  int len= v.length();
  if (len>mx) len= mx;

  for (int i= 0; i<len; i++) {
    a= 16*a + HexNr( v[ i ] );
  } // for

  return (void*)a;
} // LHexRef


/*! Get string as hex value */
long LHex( string v )
{
  long       a=  0;
  const int mx=  8;

  int len= v.length();
  if (len>mx) len= mx;

  for (int i= 0; i<len; i++) {
    a= 16*a + HexNr( v[ i ] );
  } // for

  return a;
} // LHex


/*! Get boolean as const char string */
cAppCharP Bo( bool b )
{
  if (b) return "true";
  else   return "false";
} // Bo

/*! Get boolean as short const char string "v"=true / "o"=false */
cAppCharP BoS( bool b )
{
  if (b) return "v";
  else   return "o";
} // BoS


// Get <v> as version string Va.b.c.d
string VersionStr( CVersion v )
{
  if (v==VP_BadVersion) return "<unknown>";
  if (v==0)             return "--";

  string   s;
  for (int i= 0; i<= 3; i++) {
    if (!s.empty()) s= '.' + s;
    s=   IntStr( v % 256 ) + s;
    v=           v / 256;
  } // for

  return 'V' + s;
} // VersionStr



// Get <v> as version long 0x0a0b0c0d
CVersion VersionNr( string s )
{
  long   v= 0x00000000;
  string a;

  int j= s.find( 'V',0 );
  if (j==0) s= s.substr( 1, s.length()-1 );

  if (s.length()==8 && // direct conversion
      s.find( '.',0 )==string::npos) {
    return LHex( s );
  } // if

  for (int i= 0; i<= 3; i++) {
           a= s;
        j= a.find( '.',1 );
    if (j>=1) {
      a= a.substr( 0,j );
      s= s.substr( j+1, s.length()-j-1 );
    }
    else
      s= "";
    // if

    j= atoi( a.c_str() );
    v= 256*v + j;
  } // for

  return v;
} // VersionNr



bool SameBegin( string s, string cmp )
{
  ssize_t w=      s.length()-cmp.length();
  return w>=0 && s.find   ( cmp, 0 )==0;
} // SameBegin


bool SameEnd  ( string s, string cmp )
{
  ssize_t w=      s.length()-cmp.length();
  return w>=0 && s.find   ( cmp, w )==(size_t)w;
} // SameEnd



// Replace 0A -> 0B, TAB -> ' ' when saving
void ReplaceSave( cAppCharP str, string &rslt )
{
  unsigned char* s= (unsigned char*)str; // support UTF8 as well
  char ch= '\0';
  rslt= "";

  while (*s!='\0') {
    bool ok= true;

    switch (*s) {
      case 0x0A : ch= 0x0B; break;
      case 0x09 : ch=  ' '; break; // TAB -> SP
      case 0x1D : ch= *s  ; break; // do not touch ARR Sep

      default   : if (*s>=0x20 &&
                      *s!=0x7F) ch= *s;
                  else ok= false;  // ignore others
    } // switch

            s++;
    if (ok) rslt+= ch;
  } // while
} // ReplaceSave



// Replace 0B -> 0A, when loading
void ReplaceLoad( cAppCharP str, string &rslt )
{
  unsigned char* s= (unsigned char*)str; // support UTF8 as well
  char ch= '\0';
  rslt= "";

  while (*s!='\0') {
    bool ok= true;

    switch (*s) {
      case 0x0B : ch= 0x0A; break;
      case 0x09 :
      case 0x1D : ch= *s  ; break; // do not touch ARR Sep
      default   : if (*s>=0x20 &&
                      *s!=0x7F) ch= *s;
                  else ok= false;  // ignore others
    } // switch

            s++;
    if (ok) rslt+= ch;
  } // while
} // ReplaceLoad



// ----------------------------------------------------------------------
string Apo   ( string  s ) { return "'" + s + "'"; } // Get string within single quotes
string Parans( string  s ) { return "(" + s + ")"; } // Get string within parans
string Bracks( string  s ) { return "[" + s + "]"; } // Get string within brackets

void   CutCh ( string &s ) { s=     s.substr( 1, s.length()-1 );    } // Cut 1st char
void   CutLSP( string &s ) { while (s.find( " ",0 )==0) CutCh( s ); } // Cut all leading SP


// Token separation
bool NextToken( string &s, string &nx, string sep )
{
  nx=      "";
  CutLSP  ( s );
  if      ( s.empty()) return false;

  string::size_type len=  s.length();
  string::size_type pos=  s.find( sep, 0 ); // search for the last separator
  if    (pos==string::npos) {
    nx= s; // no separation possible, result is all
    s = "";
  }
  else {
    int sepLen= sep.length();
    nx=     s.substr(          0,    pos ); // separate, if found
    s =     s.substr( pos+sepLen,len-pos-sepLen+1 );
    CutLSP( s );
  } // if

  return true;
} // NextToken



string LineConv( string str, uInt32 maxLen, bool visibleN )
{
  cAppCharP BN = "\\n";
  string s, nx, value;
  bool   first= true;
  bool   b= false;
  bool   e= false;

  if (visibleN) {
    string::size_type len= str.length();
    if               (len>0) {
      b= str.find ( "\n", 0     )==0;
      e= str.rfind( "\n", len-1 )==len-1;
    } // if
  } // if

  while (NextToken( str,nx, "\n" )) {
    if    (first && str.empty()) return nx;

    string        rema; // cut \r
    NextToken( nx,rema, "\r" );
    nx=           rema;

    string v=       nx;
    if  (NextToken( nx,value, ":" ) && !nx.empty()) {
      if (!s.empty()) {
        if (visibleN) s+= BN;
        else          s+= " ";
      } // if

      s+= v;
    } // if

    first= false;
  } // while

  if (b) s=     BN + s;
  if (e) s= s + BN;

  if (maxLen>0 &&
      maxLen<s.length()) { s= s.substr( 0,maxLen-2 ) + ".."; }

  return s;
} // LineConv



// Get <aID.item>,<aID.parent> string
string ItemID_Info( cItemID aID, string aName )
{
  string id;

  if   (aID) {
    if (aID->item) id= aID->item;
    if (aID->parent) {
      string p= aID->parent;
      if   (!p.empty()) id+= "," + p;
    } // if
  } // if
                      id=       Parans( id );
  if (!aName.empty()) id= aName + "=" + id;
  return              id;
} // ItemID_Info


// Get <mID.localID> <mID.remoteID> <mID.flags> <mID.ident> string
string MapID_Info( cMapID mID )
{
  string id;

  if   (mID) {
    if (mID->localID)  id+= Apo( mID->localID  ) + " ";
    if (mID->remoteID) id+= Apo( mID->remoteID ) + " ";

    id+= HexStr( mID->flags ) + " ";
    id+= IntStr( mID->ident );
  } // if

         id= "map=" + Parans( id );
  return id;
} // MapID_Info



// Concat <path1> and <path2>
string ConcatPaths( string path1, string path2, bool isJava )
{
  if (path2.empty()) return path1;
  if (path1.empty()) return path2;

  if (isJava) path1+= '/';
  else {
    #ifdef _WIN32
      path1+= '\\';
    #else
      path1+= '/';
    #endif
  } // if

  return path1 + path2;
} // ConcatPaths


// Concat <path1>, <path2> and <path3>
string   ConcatPaths( string path1, string path2,    string path3, bool isJava ) {
  return ConcatPaths( ConcatPaths ( path1, path2, isJava ), path3,      isJava );
} // ConcatPaths



/*! Notation: <name1>"!"<name2> */
string ConcatNames( string name1, string name2, string sep )
{
  if (!name1.empty() &&
      !name2.empty()) name1+= sep;
                      name1+= name2;
  return              name1;
} // ConcatNames



static bool Chk( string s, string &aDat, cAppCharP &q, cAppCharP &qV, int offs )
{
  string::size_type pos= aDat.find( s, 0 );

  do {
    // either at the beginning ...
    if (pos==0) break;

    // ... or right after a '\n'
        pos= aDat.find( "\n" + s, 0 );
    if (pos==string::npos) return false;
        pos++; // without \n at the beginning
  } while (false);

  q = aDat.c_str() + pos;
  qV= q + s.length() + offs;
  return true;
} // Chk



static bool GetField_R( string &aDat, string aKey, string &value, bool removeIt )
{
  cAppCharP q;
  cAppCharP qV;
  cAppCharP qN;
  cAppCharP qR;

  value= ""; // in case of not found

  if (!Chk( aKey +   StdPattern, aDat, q,qV, 0 ) && // is it one of these types ?
      !Chk( aKey + ArrayPattern, aDat, q,qV,-1 ) &&
      !Chk( aKey +          ";", aDat, q,qV,-1 )) return false;

             qN=    strstr( q,"\n" );           // search for next separator
  bool last= qN==NULL;
  if  (last) qN= (cAppCharP)q + strlen( q );    // it's the last field ?
       qR= qN-1;
  if (*qR!='\r') qR= qN;                        // ignore \r
  value.assign ( qV, qR-qV );                   // get the snippet

  if (removeIt) {
    cAppCharP qE= qN; if (*qE) qE++;

    if (last) { // if it is the last one, cut line break before as well
      while (q>aDat.c_str()) {
             q--;
        if (*q!='\n' &&
            *q!='\r') { q++; break; }
      } // while
    } // if

    string::size_type pos= q-aDat.c_str();      // offset to <q> at <aDat>
    string::size_type n  = qE-q;                // size of the field

    aDat= aDat.erase( pos, n );
  } // if

  return true;
} // GetField_R



bool     GetField( string  aDat, string aKey, string &value ) {
  return GetField_R      ( aDat,        aKey,         value, false );
} // GetField


bool  RemoveField( string &aDat, string aKey, string &removedValue ) {
  return GetField_R      ( aDat,        aKey,         removedValue, true )
                                                  && !removedValue.empty();
} // RemoveField



bool FlagOK( string aDat, string aKey, bool isYes )
{
  string                value;
  GetField( aDat, aKey, value );
  bool  ok, emp=  aKey.empty();

  if (isYes) { ok= !emp   &&  ( value=="yes"
                          ||    value=="true"
                          ||    value=="both"  ); }
  else       { ok=  emp   || !( value=="no"
                          ||    value=="false" ); }

//printf( "value=%-10s isYes=%-5d ok=%-5d aEmp=%-5d aKey='%s'\n",
//         value.c_str(), isYes, ok, aDat.empty(), aKey.c_str() );
  return ok;
} // FlagOK



bool FlagBoth( string aDat, string aKey )
{
  string                value;
  GetField( aDat, aKey, value );
  return                value=="both";
} // FlagBoth



void FilterFields( string &aDat, string aFilter )
{
  string s, removedValue;

  cAppCharP qN;
  cAppCharP qR;
  cAppCharP q= aFilter.c_str();

  while     (*q) {
             qN=    strstr( q,"\n" );
    if (!qN) qN= (cAppCharP)q + strlen( q );
         qR= qN-1;
    if (*qR!='\r') qR= qN;

    s.assign(   q, (unsigned int)( qR-q ) );
    q= qN; if (*q) q++;

    RemoveField( aDat, s, removedValue );
  } // while
} // FilterFields



/* ---------- global context handling ------------------------ */
bool GlobContextFound( string dbName, GlobContext* &g )
{
  while (g!=NULL) {
    if (dbName==g->refName) break; // work is done

    if (strcmp( g->refName,"" )==0) {
        strcpy( g->refName, dbName.c_str() );
        return false;
    } // if

    if  (g->next==NULL) {
        return false;
    } // if

    g= g->next;
  } // while

  return g!=NULL && g->ref!=NULL;
} // GlobContextFound



/* ---------- Tunnel itemKey --------------------------------- */
TSyError  OpenTunnel_ItemKey( TunnelWrapper* tw )
{
  TSyError err, cer;
  SDK_UI_Struct* ui= &tw->tCB->ui;

  KeyH                                                    tKey, sKey;
         err= ui->OpenSessionKey( tw->tCB, tw->tContext,       &sKey,           0 );
  if   (!err) {
         err= ui->OpenKeyByPath ( tw->tCB,               &tKey, sKey, "tunnel", 0 );
    if (!err)  {
         err= ui->OpenKeyByPath ( tw->tCB, &tw->tItemKey, tKey,       "item",   0 );
         cer= ui->CloseKey      ( tw->tCB, tKey ); if (!err) err= cer;
    } // if

         cer= ui->CloseKey      ( tw->tCB, sKey ); if (!err) err= cer;
  } // if

  return err;
} // OpenTunnel_ItemKey


TSyError CloseTunnel_ItemKey( TunnelWrapper* tw )
{
  SDK_UI_Struct* ui= &tw->tCB->ui;
  return         ui->CloseKey( tw->tCB, tw->tItemKey );
} // CloseTunnel_ItemKey



#if defined __cplusplus
  } // namespace */
#endif


/* eof */
