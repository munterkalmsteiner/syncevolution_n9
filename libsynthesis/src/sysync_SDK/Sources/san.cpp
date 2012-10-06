/*
 *  File:         san.cpp
 *
 *  Author:       Beat Forster (bfo@synthesis.ch)
 *
 *  Server Alerted Notification
 *    for OMA DS 1.2
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 *
 */

/*
                      128 bit     64 bit + n char
                  +------------+--------------------+--------------------+
                  |            |                    |                    |
                  |  digest    |  notification-hdr  |  notification-body |
                  |            |                    |                    |
                  +------------+--------------------+--------------------+
                              /                      \                    \
                            /                          \ ---------          \
                          /                              \         \          \
     --------------------                                  ------    \          \
   /                                                              \    \          \
 /                                                                  \    \          \
+---------+---------+-----------+--------+---------+--------+--------+    +----------+
| version | ui-mode | initiator | future | session | server | server |    |  usage   |
|         |         |           |   use  |   id    | ident  | ident  |    | specific |
|         |         |           |        |         | length |        |    |          |
+---------+---------+-----------+--------+---------+--------+--------+    +----------+
  10 bit     2 bit      1 bit     27 bit   16 bit     8 bit   n char


H     = MD5 hashing function
B64   = Base64 encoding
digest= H(B64(H(server-identifier:password)):nonce:B64(H(notification)))



                notification body:
  +-------+--------+--------+--------+----------+
  |  num  | future | sync 1 | sync N | vendor   |
  | syncs |   use  |        |        | specific |
  |       |        |        |        |          |
  +-------+--------+--------+--------+----------+
    4 bit  4 bit  /          \          n char
                /              \
     ----------                  -------
   /                                     \
 /                                         \
+------+--------+---------+--------+--------+
| sync | future | content | server | server |
| type | use    | type    | URI    | URI    |
|      |        |         | length |        |
+------+--------+---------+--------+--------+
 4 bit   4 bit    24 bit    8 bit    n char

*/


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "prefix_file.h"
#include "sync_include.h"
#include "san.h"
#include "sysync_md5.h"
#include "sysync_b64.h"

#ifndef WITHOUT_SAN_1_1
#include "sysync_utils.h"
#endif


const uInt16 SyncML12 = 12;   // currently supported SyncML version
const uInt16 SyncML11 = 11;   // currently supported SyncML version
const uInt16 SyncML10 = 10;   // currently supported SyncML version

#pragma options align= packed // allow direct mapping of the structure


using namespace sysync;

namespace sysync {

// ---- structure definition ---------------------------------------------
#define BpB        8 // bits per byte
#define NBits      7 // bytes of the notification-hdr  bits
#define BBits      4 // bytes of the notification-body bits

#define DB_Full  420 // memory full error
#define DB_Error 510 // general DB error

struct TPackage {
  TDigestField digest;

  uInt8  bitField[ NBits ]; // version, ui-mode, initiator, future use, sesion id
  uInt8  serverID_len;
}; // TPackage


struct TBody {
  uInt8  bitField[ BBits ]; // sync type, future use, content type
  uInt8  serverURI_len;
}; // TBody



// ---- defined locally for the moment to avoid dependencies ----
//
// MD5 and B64 given string
static void MD5B64_Local(const char *aString, sInt32 aLen, string &aMD5B64)
{
  // determine input length
  if (aLen<=0) aLen=strlen(aString);
  // calc MD5
  md5::SYSYNC_MD5_CTX context;
  uInt8 digest[16];
  md5::Init (&context);
  md5::Update (&context, (const uInt8 *)aString,aLen);
  md5::Final (digest, &context);
  // b64 encode the MD5 digest
  uInt32 b64md5len;
  char *b64md5=b64::encode(digest,16,&b64md5len);
  // assign result
  aMD5B64.assign(b64md5,b64md5len);
  // done
  b64::free(b64md5); // return buffer allocated by b64::encode
} // MD5B64_Local



// ---- constructor/destructor -------------------------------------------
SanPackage::SanPackage() // constructor
{
  fBody= NULL;
  CreateEmptyNotificationBody();

  memset( &fDigest, 0, DigestSize );
  fProtocolVersion= 0;
  fUI_Mode        = UI_not_specified;
  fInitiator      = Initiator_Server;
  fSessionID      = 0;

  fSan            = NULL;
  fSanSize        = 0;
} // constructor


SanPackage::~SanPackage() // destructor
{
  ReleasePackage();
  ReleaseNotificationBody();
} // destructor



// ---- digest creation --------------------------------------------------
TDigestField SanPackage::H( string s )
{
  TDigestField df;

  // calc MD5
  md5::SYSYNC_MD5_CTX context;
  md5::Init        ( &context );
  md5::Update      ( &context, (const uInt8 *)s.c_str(), s.length() );
  md5::Final( df.b,  &context );
  return      df;
} // DigestField


string SanPackage::B64_H( string s1, string s2 )
{
  if          (!s2.empty()) s1+= ":" + s2;
  MD5B64_Local( s1.c_str(), s1.size(), s1 );
  return        s1;
} // B64_H


string SanPackage::B64_H_Notification( void* san, size_t sanSize )
{
  string                    s;
  const char*   v= (const char*)san + DigestSize;
  size_t           nfySize= sanSize - DigestSize;
  MD5B64_Local( v, nfySize, s );
  return                    s;
} // B64_H



/*! Prepare the SAN record */
void SanPackage::PreparePackage( string    aB64_H_srvID_pwd,
                                 string    aNonce,
                                 uInt16    aProtocolVersion,
                                 UI_Mode   aUI_Mode,
                                 Initiator aInitiator,
                                 uInt16    aSessionID,
                                 string    aSrvID )
{
  fB64_H_srvID_pwd= aB64_H_srvID_pwd;
  fNonce          = aNonce;
  fProtocolVersion= aProtocolVersion;
  fUI_Mode        = aUI_Mode;
  fInitiator      = aInitiator;
  fSessionID      = aSessionID;
  fServerID       = aSrvID;
} // PreparePackage


// if only hashes are available
TSyError SanPackage::CreateDigest( const char* b64_h_serverID_password,
                                   const char* aNonce,
                                   void* san, size_t sanSize )
{
  string s= b64_h_serverID_password;
  if   ( s.empty()) {
    for (int i= 0; i<DigestSize; i++) { // special case for empty digest
      fDigest.b[ i ]= 0x00;
    } // for
  }
  else {                    s+= ":";
                s+= aNonce; s+= ":";
                s+= B64_H_Notification( san,sanSize );
    fDigest= H( s );
  } // if

  return LOCERR_OK;
} // CreateDigest


TSyError SanPackage::CreateDigest( const char* aServerID,
                                   const char* aPassword,
                                   const char* aNonce,
                                   void* san, size_t sanSize )
{
  return CreateDigest( B64_H( aServerID,aPassword ).c_str(),
                       aNonce,
                       san,sanSize );
} // CreateDigest



bool SanPackage::DigestOK( void* san )
{
  TDigestField* sanD= (TDigestField*)san;

  for (int i= 0; i<DigestSize; i++) {
    if (fDigest.b[ i ]!=sanD->b[ i ]) return false;
  } // for

  return true;
} // DigestOK



// ---- bit operations ---------------------------------------------------
void SanPackage::AddBits( void* ptr, int pos, int n, uInt32 value )
{
  byte* b= (byte*)ptr;
  int    lim= pos+n;
  if    (lim>BpB*NBits) return; // check if within the field
  while (lim>BpB) { b++; lim-= BpB; }

  int  i;
  for (i=0; i<n; i++) {
    uInt8 db= 1<<(BpB-lim);

    if   ((value % 2)==1) *b|=  db; // add    bit
    else                  *b&= ~db; // remove bit
    value= value / 2;

        lim--;
    if (lim==0) { lim= BpB; b--; }
  } // for
} // AddBits


uInt32 SanPackage::GetBits( void* ptr, int pos, int n )
{
  uInt32 value= 0;

  byte* b= (byte*)ptr;
  int    lim= pos+n;
  if    (lim>BpB*NBits) return 0; // check if within the field
  while (lim>BpB) { b++; lim-= BpB; }

  int  i;
  for (i=0; i<n; i++) {
    uInt8 db= 1<<(BpB-lim);

    if ((*b & db)!=0) value|= (1<<n); // check bit and add it to <value>
    value= value / 2;

        lim--;
    if (lim==0) { lim= BpB; b--; }
  } // for

  return value;
} // GetBits



// ---- notification body generation -------------------------------------
void SanPackage::CreateEmptyNotificationBody()
{
  ReleaseNotificationBody();

                    fEmpty= 0x00; // no sync fields = ALL data stores concerned
  fBody=           &fEmpty;
  fBodySize= sizeof(fEmpty);
  fNSync= 0;
} // CreateEmptyNotificationBody



TSyError SanPackage::AddSync( int syncType, uInt32 contentType,
                              const char* serverURI )
{
  int    len= strlen(serverURI);
  int   nLen= BBits + 1 +  len;  // length of the new part
  int newLen= fBodySize + nLen;  // total length of the new block

  void*   fb=  malloc( newLen ); // allocate it
  memcpy( fb, fBody,fBodySize ); // copy existing structure to beginning

  byte*   b = (byte*)fb;
          b+= fBodySize;         // get a pointer to the new part

  ReleaseNotificationBody();     // release the old structure
  fNSync++;                      // adapt number of available parts
  fBody    = fb;                 // now the new bigger structure is assigned
  fBodySize= newLen;

  // fill in new counter value
  AddBits( fBody,         0, 4, fNSync       ); // number of sync datastores
  AddBits( fBody,         4, 4, 0            ); // future use

  // fill in contents of the nth structure
  TBody*   tb= (TBody*)b;
  AddBits( tb->bitField,  0, 4, syncType-200 ); // the sync type 206..210
  AddBits( tb->bitField,  4, 4, 0            ); // future use
  AddBits( tb->bitField,  8,24, contentType  ); // the content tye
           tb->serverURI_len=   len;

  byte*           pp= (byte*)(tb+1); // = right after TBody
  memcpy( (void*) pp, (void*)serverURI, len );
  return LOCERR_OK;
} // AddSync



void SanPackage::ReleaseNotificationBody()
{
  if (fBody!=NULL &&
      fBody!=&fEmpty) { free( fBody ); fBody= NULL; }
} // ReleaseNotificationBody


#ifndef WITHOUT_SAN_1_1
// general callback entry for all others
static Ret_t univ( ... )
{
//printf( "callback\n" );
  return 0;
} // univ


static Ret_t startM( InstanceID_t id, VoidPtr_t userData, SmlSyncHdrPtr_t pContent )
{
  cAppCharP Sy= "SyncML/";
  size_t n = strlen(Sy);

  SanPackage* a= (SanPackage*)userData;
  string mup  = "";
  string nonce= "";

  uInt16 major=0,minor=0;

  cAppCharP verP = smlPCDataToCharP(pContent->proto);
  if (strucmp(verP,Sy,n)==0) {
    n+=StrToUShort(verP+n,major);
    if (verP[n]=='.') {
      n++;
      StrToUShort(verP+n,minor);
    }
  }

  sInt32                                sessionID;
  smlPCDataToLong( pContent->sessionID, sessionID );

  string srvID= smlSrcTargLocURIToCharP(pContent->source);

  a->PreparePackage( mup, nonce, 10*major+minor, UI_not_specified, Initiator_Server, sessionID, srvID );
  a->CreateEmptyNotificationBody();
  return 0;
} // startM


static Ret_t alertM( InstanceID_t id, VoidPtr_t userData, SmlAlertPtr_t pContent )
{
  SanPackage* a= (SanPackage*)userData;

  sInt32                           syncType;
  smlPCDataToLong( pContent->data, syncType );
  uInt32 contentType= 0; // always 0

  SmlItemListPtr_t el= pContent->itemList;

  while (true) { // can be a chained list of elements
    string locURI= smlSrcTargLocURIToCharP(el->item->source);
    a->AddSync( syncType, contentType, locURI.c_str() ); // for each element add one

    if (el->next==NULL) break;
    el= el->next;
  } // while

//printf( "alert\n" );
  return 0;
} // alert


static Ret_t endM( InstanceID_t id, VoidPtr_t userData, Boolean_t final )
{
//printf( "end\n" );
  return 0;
} // endM


// Callback record, most of the routines are not used
static const SmlCallbacks_t mySmlCallbacks = {
  /* message callbacks */
  startM,                    // smlStartMessageCallback,
  endM,                      // smlEndMessageCallback,
  /* grouping commands */
  (smlStartSyncFunc)   univ, // smlStartSyncCallback,
  (smlEndSyncFunc)     univ, // smlEndSyncCallback,
  #ifdef ATOMIC_RECEIVE  /* these callbacks are NOT included in the Toolkit lite version */
    univ, // smlStartAtomicCallback,
    univ, // smlEndAtomicCallback,
  #endif
  #ifdef SEQUENCE_RECEIVE
    univ, // smlStartSequenceCallback,
    univ, // smlEndSequenceCallback,
  #endif
  /* Sync Commands */
  (smlAddCmdFunc)   univ, // smlAddCmdCallback,
  alertM,                 // smlAlertCmdCallback,
  (smlDeleteCmdFunc)univ, // smlDeleteCmdCallback,
  (smlGetCmdFunc)   univ, // smlGetCmdCallback,
  (smlPutCmdFunc)   univ, // smlPutCmdCallback,
  #ifdef MAP_RECEIVE
    (smlMapCmdFunc) univ, // smlMapCmdCallback,
  #endif
  #ifdef RESULT_RECEIVE
    (smlResultsCmdFunc)univ, // smlResultsCmdCallback,
  #endif
  (smlStatusCmdFunc) univ, // smlStatusCmdCallback,
  (smlReplaceCmdFunc)univ, // smlReplaceCmdCallback,
  /* other commands */
  #ifdef COPY_RECEIVE  /* these callbacks are NOT included in the Toolkit lite version */
    univ, // smlCopyCmdCallback,
  #endif
  #ifdef EXEC_RECEIVE
    univ, // smlExecCmdCallback,
  #endif
  #ifdef SEARCH_RECEIVE
    univ, // smlSearchCmdCallback,
  #endif
  smlMoveCmdFunc(univ), // smlMoveCmdCallback,
  /* Other Callbacks */
  smlHandleErrorFunc  (univ), // smlHandleErrorCallback,
  smlTransmitChunkFunc(univ)  // smlTransmitChunkCallback
}; /* sml_callbacks struct */



// Try to convert a 1.1 message
// - if successful, fill in values into 1.2 fields
// - if not successful, interpret it as 1.2 structure
TSyError SanPackage::Check_11( void* san, size_t sanSize )
{
  TSyError             err;
  SmlCallbacks_t       scb= mySmlCallbacks;
  SmlInstanceOptions_t sIOpts;
  InstanceID_t         id;
  Ret_t                cer;
  MemPtr_t             wPos;
  MemSize_t            freeSize;

  // struct assignment / 1k buffer
  sIOpts.encoding       = SML_WBXML; // it is always WBXML
  sIOpts.workspaceSize  = 1024*30;      // should be always sufficient
  sIOpts.maxOutgoingSize=    0;      // disabled for now

  err=   smlInitInstance( &scb, &sIOpts, this, &id );  if (err) return err;

  do {
    err= smlLockWriteBuffer  ( id, &wPos, &freeSize ); if (err) break;

    memcpy( wPos, san,sanSize ); // now we have a new internal copy
    err= smlUnlockWriteBuffer( id, sanSize );          if (err) break;
    err= smlProcessData      ( id, SML_ALL_COMMANDS ); if (err) break;
  } while (false);

  cer= smlTerminateInstance( id ); if (!err) err= cer;

  return err;
} // Check_11
#endif // WITHOUT_SAN_1_1


TSyError SanPackage::PassSan( void* san, size_t sanSize, int mode)
{
  TSyError err = LOCERR_OK;
  bool     use_as_12= true;

  ReleasePackage();
//printf( "here we will have the potential 1.1 -> 1.2 conversion\n" );

  #ifndef WITHOUT_SAN_1_1
  if (mode == 0 || mode == 1) {
      err= Check_11  ( san,sanSize );
      if (!err)  err= GetPackage( san,sanSize );
      //use_as_12= err==SML_ERR_XLT_INCOMP_WBXML_VERS;
      use_as_12= err!=0;
      //printf( "err=%d\n", err );
  }
#endif

  if (use_as_12 && mode !=1) {
    err= DB_Full;

        fSan=   malloc( sanSize );
    if (fSan) {
      fSanSize=         sanSize;
      memcpy( fSan, san,sanSize ); // now we have a new internal copy
      err= LOCERR_OK;
    } // if
  } // if

  return err;
} // PassSan


TSyError SanPackage::GetSanSize( void* san, size_t &sanSize )
{
  TPackage* tp= (TPackage*)san;
  TBody*    tb = NULL;

  byte* b= (byte*)(tp+1);
  byte* v;

  b+= tp->serverID_len;

  int nth= GetBits( b, 0,4 ); // first not valid = the end

  b++; // start of 1st element
  int    n= nth;
  while (n>0) {
    n--;
    tb= (TBody*)b;
    b = (byte*)(tb+1);

    if (b > (byte*)san+sanSize && sanSize>0) return DB_Forbidden;
    v=  b + tb->serverURI_len;
    if (b > (byte*)san+sanSize && sanSize>0) return DB_Forbidden;

    if (n==0) break;
    b=  v;
  } // while

  b+= tb->serverURI_len; // finally the serverURI length

  size_t   rslt= b - (byte*)san;
  if (sanSize>0 && sanSize<rslt) return DB_Forbidden;

  sanSize= rslt;
  return LOCERR_OK;
} // GetSanSize


// ---- notification body parsing ----------------------------------------
TSyError SanPackage::GetNthSync( int    nth,
                                 int    &syncType,
                                 uInt32 &contentType,
                                 string &serverURI )
{
  syncType   =  0; // set default values
  contentType=  0;
  serverURI  = "";

  TPackage* tp= (TPackage*)fSan;
  TBody*    tb;

  fDigest         =                     tp->digest;
  fProtocolVersion=            GetBits( tp->bitField,  0,10 );
  fUI_Mode        =   (UI_Mode)GetBits( tp->bitField, 10, 2 );
  fInitiator      = (Initiator)GetBits( tp->bitField, 12, 1 );
  fSessionID      =            GetBits( tp->bitField, 40,16 );

  /*If the version does not match, this should be an invalid SAN message*/
  if (fProtocolVersion!=SyncML12 &&
      fProtocolVersion!=SyncML11 &&
      fProtocolVersion!=SyncML10) return DB_Forbidden;

  byte* b= (byte*)(tp+1);
  byte* v;

  fServerID.assign( (const char*)b,(unsigned int)tp->serverID_len );
  b+= tp->serverID_len;

  fNSync= GetBits( b, 0,4 );

  if (nth==0)               return LOCERR_OK;
  if (nth<1 || nth>fNSync ) return DB_NotFound;

  b++; // start of 1st element
  int    n= nth;
  while (n>0) {
    n--;
    tb= (TBody*)b;
    b = (byte*)(tb+1);

    if (b > (byte*)fSan+fSanSize) return DB_Forbidden; // no access behind the message
    v=  b + tb->serverURI_len;
    if (v > (byte*)fSan+fSanSize) return DB_Forbidden; // no access behind the message

    if (n==0) break;
    b=  v;
  } // while

  syncType   = 200 + GetBits( tb->bitField,  0, 4 );
  contentType=       GetBits( tb->bitField,  8,24 );

  serverURI.assign( (const char*)b,(unsigned int)tb->serverURI_len );

  return LOCERR_OK;
} // GetNthSync


TSyError SanPackage::GetHeader()
{
  int    syncType; // these 3 variables are not really used
  uInt32 contentType;
  string serverURI;

  return GetNthSync( 0, syncType,contentType,serverURI );
} // GetHeader



// ---- package generation -----------------------------------------------
TSyError SanPackage::GetPackage( void* &san, size_t &sanSize,
                                 void*  vendorSpecific,
                                 size_t vendorSpecificSize )
{
  ReleasePackage(); // remove a previous one

  byte   len    = (byte)fServerID.length();     // calulate the full size
         sanSize= sizeof(TPackage) + len + fBodySize + vendorSpecificSize;
  //size_t nfySize= sanSize - DigestSize;
  fSan          = malloc( sanSize );
  san           =            fSan;
  TPackage*   tp= (TPackage*)fSan;

  // -------------------
  AddBits( tp->bitField,  0,10, fProtocolVersion );
  AddBits( tp->bitField, 10, 2, fUI_Mode         );
  AddBits( tp->bitField, 12, 1, fInitiator       );
  AddBits( tp->bitField, 13,27, 0                ); // future use, must be "0"
  AddBits( tp->bitField, 40,16, fSessionID       );
           tp->serverID_len=    len;

  // copy <fServerID> string at the end of TPackage struct
  byte*           pp= (byte*)(tp+1); // = right after TPackage
  memcpy( (void*) pp,      (void*)fServerID.c_str(), len );
  memcpy( (void*)(pp+len),        fBody,       fBodySize );

  if (vendorSpecific!=NULL &&
      vendorSpecificSize>0)
    memcpy( (void*)(pp+len+fBodySize), vendorSpecific,vendorSpecificSize );

  CreateDigest( fB64_H_srvID_pwd.c_str(), fNonce.c_str(), san,sanSize );
  tp->digest= fDigest;

  fSanSize= sanSize;
  return LOCERR_OK;
} // GetPackage


void SanPackage::ReleasePackage() {
  if (fSan!=NULL) { free( fSan ); fSan= NULL; }
} // ReleasePackage

#ifndef WITHOUT_SAN_1_1

const char * const SyncMLVerProtoNames[] =
{
    "undefined",
    "SyncML/1.0",
    "SyncML/1.1",
    "SyncML/1.2"
};

const char *const SyncMLVerDTDNames[] =
{
    "???",
    "1.0",
    "1.1",
    "1.2"
};

const SmlVersion_t SmlVersionCodes[] =
{
    SML_VERS_UNDEF,
    SML_VERS_1_0,
    SML_VERS_1_1,
    SML_VERS_1_1
};

TSyError SanPackage::GetPackageLegacy( void* &san,
                                       size_t &sanSize,
                                       const vector<pair <string, string> >& sources,
                                       int alertCode,
                                       bool wbxml)
{
  ReleasePackage(); // remove a previous one
  TSyError err;
  SmlCallbacks_t       scb= mySmlCallbacks;
  SmlInstanceOptions_t sIOpts;
  InstanceID_t         id;


  // struct assignment / 1k buffer
  sIOpts.encoding       = wbxml ? SML_WBXML : SML_XML;
  sIOpts.workspaceSize  = 1024;      // should be always sufficient
  sIOpts.maxOutgoingSize=    0;      // disabled for now

  err=   smlInitInstance( &scb, &sIOpts, this, &id );  if (err) return err;

  SmlSyncHdrPtr_t headerP = NULL;
  SmlAlertPtr_t alertP = NULL;

  do {
    SYSYNC_TRY{
      //create SyncHdr
      headerP = SML_NEW (SmlSyncHdr_t);
      headerP->elementType = SML_PE_HEADER;
      if (fProtocolVersion != 10 && fProtocolVersion != 11){
          //wrong version!
          err = DB_Error;
          break;
      }
      int version = fProtocolVersion - 10 + 1;
      headerP->version = newPCDataString (SyncMLVerDTDNames[version]);
      headerP->proto = newPCDataString (SyncMLVerProtoNames[version]);
      headerP->sessionID = newPCDataLong (fSessionID);
      headerP->msgID = newPCDataString ("1");
      headerP->target = newLocation ("/", "");
      headerP->source = newLocation (fServerID.c_str(), "");
      headerP->respURI = NULL;
      headerP->meta = NULL;
      headerP->flags = 0;
      //TODO generate the cred element for authentication
      headerP->cred = NULL;

      //create SyncMessage
      err = smlStartMessageExt (id, headerP, SmlVersionCodes[version]); if (err) break;
      //create Alert Commands
      //internal Alert element
      alertP = SML_NEW (SmlAlert_t);
      alertP->elementType = SML_PE_ALERT;
      alertP->cmdID = newPCDataLong(1);
      alertP->flags = 0;
      alertP->data = newPCDataLong (alertCode);
      alertP->cred = NULL;
      alertP->itemList = NULL;
      alertP->flags = 0;

      //for each source, add a item
      for (unsigned int num =0; num < sources.size(); num++) {
          SmlItemPtr_t alertItemP = newItem();
          alertItemP->source = newOptLocation (sources[num].second.c_str());
          alertItemP->meta = newMetaType (sources[num].first.c_str());
          addItemToList (alertItemP, &alertP->itemList);
      }

      err = smlAlertCmd (id, alertP); if (err) break;
      err = smlEndMessage (id, true); if (err) break;

      MemPtr_t buf = NULL;
      err = smlLockReadBuffer (id, (MemPtr_t *) &buf, (MemSize_t *)&sanSize); if (err) break;
      fSan          = malloc( sanSize ); if (!fSan) {err = DB_Full; break;}
      san = fSan;
      memcpy (san, buf, sanSize);
      err = smlUnlockReadBuffer (id, sanSize); if (err) break;
      } SYSYNC_CATCH (...)
      if  (headerP) {
          smlFreeProtoElement (headerP);
          headerP = NULL;
      }
      if (alertP) {
          smlFreeProtoElement (alertP);
          alertP = NULL;
      }
      err = DB_Full;
      SYSYNC_ENDCATCH
  } while (false);
  if  (headerP) {
      smlFreeProtoElement (headerP);
  }
  if (alertP) {
      smlFreeProtoElement (alertP);
  }
  if (err) return err;

  err = smlTerminateInstance( id );
  return err;
}
#endif

} // namespace sysync

// eof
