/*
 *  File:         san.h
 *
 *  Author:       Beat Forster (bfo@synthesis.ch)
 *
 *  Server Alerted Notification
 *    for OMA DS 1.2
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 */

#ifndef San_H
#define San_H


// ---------- standalone definitions ------------------------------
#include "generic_types.h" // some basic defs, which aren't available
#include "syerror.h"       // error code definitions

#include <cstdio>         // used for printf calls
#include <cstring>        // used for strcpy/strlen calls

#ifdef __cplusplus
  #include <string>      // STL includes
  #include <list>
  #include <vector>
  using namespace std;
#endif

typedef unsigned char  byte;

// ----------------------------------------------------


namespace sysync {


// The digest field structure
#define DigestSize 16

struct TDigestField {
  uInt8 b[ DigestSize ];
}; // TDigestField


enum UI_Mode {
  UI_not_specified    = 0, // "00"
  UI_background       = 1, // "01"
  UI_informative      = 2, // "10"
  UI_user_interaction = 3  // "11"
};

enum Initiator {
  Initiator_User      = 0, //  "0"
  Initiator_Server    = 1  //  "1"
};


/*!
 *  How to create a SAN package (on server side):
 *     1) Prepare the SAN package using 'PreparePackage'
 *
 *        If      all datastores need to be notified, skip 2 and 3
 *        If specific datastores need to be notified, call 2 and 3
 *
 *     2) Call 'CreateEmptyNotificationBody' (not needed 1st time) and
 *     3) call 'AddSync' for each datastore to be notified
 *
 *     4) Create the SAN package with 'GetPackage'.
 *        A vendor specific record can be added, if required.
 *
 * How to create a Legacy SAN package (server side SAN 1.0/1.1):
 *    1) Prepare the SAN package using 'PreparePackage'
 *    2) Call 'CreateEmptyNotificationBody' (not needed 1st time) and
 *    3) Create the SAN package with 'GetPackageLegacy'.
 *
 *--------------------------------------------------------------------
 *  How to check a SAN package (on client side):
 *     1) pass the san message with 'PassSan'
 *     1) create <fDigest> first (using 'CreateDigest')
 *     2) call 'DigestOK', to verify it
 *
 *  How to get the <n>th sync message:
 *        call 'GetNthSync( n, .. )'
 *      [ call 'GetHeader (       )' or
 *              GetNthSync( 0, .. )' to get header params only ]
 *
 */
class SanPackage {
  public:
    SanPackage(); // constructor
   ~SanPackage(); //  destructor

    /*! Base64 encoded MD5, two strings can be concatenated with ":" */
    string B64_H( string s1, string s2= "" );

    /*! Base64 encoded MD5 of the notification part of <san>,<sanSize>. */
    string B64_H_Notification( void* san, size_t sanSize );


    /*! Prepare the SAN package */
    void PreparePackage( string    aB64_H_srvID_pwd,
                         string    aNonce,
                         uInt16    aProtocolVersion,
                         UI_Mode   aUI_Mode,
                         Initiator aInitiator,
                         uInt16    aSessionID,
                         string    aSrvID );



    /*! These variables will be assigned with the 'PreparePackage' call
     *  <fProtocolVersion>: 10*version => max= V102.3 / V1.0 = 10 )
     *                      for OMA DS 1.2 use '12'
     */
    string    fB64_H_srvID_pwd;
    string    fNonce;
    uInt16    fProtocolVersion; // 10 bit
    UI_Mode   fUI_Mode;         //  2 bit
    Initiator fInitiator;       //  1 bit
    uInt16    fSessionID;       // 16 bit
    string    fServerID;


    /*! Create an empty notification body */
    void CreateEmptyNotificationBody();


    /*! Add a sync sequence to the notification body
     *
     *  (in)
     *  @param  <syncType>     206..210 (internally less 200: 206 -> 6)
     *  @param  <contentType>  MIME media content type (24 bit)
     *  @param  <serverURI>    server's URI
     */
    TSyError AddSync( int syncType, uInt32 contentType, const char* serverURI );


    /*! Get the SAN package for v1.2
     *
     *  (out)
     *  @param  <san>                get the pointer to the SAN message.
     *  @param  <sanSize>            get the SAN message size (in bytes).
     *
     *  (in)
     *  @param  <vendorSpecific>     reference to vendor specific part
     *  @param  <vendorSpecificSize> size (in bytes) of vendor specific part
     *
     *  @return error code           if operation can't be performed
     *
     *  NOTE: The notification body will be added automatically
     *
     */
    TSyError GetPackage( void* &san, size_t &sanSize,
                         void*  vendorSpecific= NULL,
                         size_t vendorSpecificSize= 0 );

#ifndef WITHOUT_SAN_1_1
    /*! Get the SAN package for v1.1/v1.0
     *
     *  (out)
     *  @param  <san>                get the pointer to the SAN message.
     *  @param  <sanSize>            get the SAN message size (in bytes).
     *
     *  (in)
     *  @param  <sources>            vector of alerted sources
     *  @param  <alertCode>          the synchronization mode
     *  @param  <wbxml>              use wbxml or plain xml
     *
     *  @return error code           if operation can't be performed
     *
     *  NOTE: The notification body will be added automatically
     *
     */
    TSyError GetPackageLegacy( void* &san,
                               size_t &sanSize,
                               const vector<pair <string, string> > &sources,
                               int alertCode,
                               bool wbxml = true);
#endif

    /*! Create the digest for the SAN package:
     *  digest= H(B64(H(server-identifier:password)):nonce:B64(H(notification)))
     *  where notification will be calculated from <san>/<sanSize>.
     */
    TSyError CreateDigest( const char* aSrvID,
                           const char* aPwd,
                           const char* aNonce,
                           void* san, size_t sanSize );

    /*! overloaded version, if only the B64 hashes are available */
    TSyError CreateDigest( const char* b64_h_srvID_pwd,
                           const char* aNonce,
                           void* san, size_t sanSize );


    /*! Check, if the digest of <san> is correct */
    bool DigestOK( void* san );


    /*! Pass SAN message <san>,<sanSize> to object,
     *  a local copy will be kept then internally
     *  (in)
     *  @param  <san>          the pointer to the SAN message
     *  @param  <sanSize>      the max. SAN message size (in bytes)
     *  @param  <mode>         0|1|2, 0 tries both San 1.1 and 1.2, 1 tries only
     *                         1.1 and 2 tries only 1.2
     */
    TSyError PassSan( void* san, size_t sanSize , int mode = 0);


    /*! Get the effective size of an already created <san> message
     *  (without vendor specific part)
     *
     *  (in)
     *  @param  <san>          the pointer to the SAN message
     *  @param  <sanSize>      the max. SAN message size (in bytes)
     *                         0, if unknown.
     *  (out)
     *  @param  <sanSize>      the effective SAN message size (in bytes)
     *
     *  @return error code     403, if input <sanSize> is too small
     */
    TSyError GetSanSize( void* san, size_t &sanSize );


    /*! Get the nth sync info
     *
     *  (in)
     *  @param  <san>          the pointer to the SAN message
     *  @param  <sanSize>      the SAN message size (in bytes)
     *  @param  <nth>          asks for the <nth> sync info
     *                         nth=0 is allowed also, but will only assign
     *                         the header variables
     *  (out)
     *  @param  <syncType>     206..210 (internally less 200: 206 -> 6)
     *  @param  <contentType>  MIME media content type (24 bit)
     *  @param  <serverURI>    server's URI
     *
     *  @return error code     403, if <sanSize> is too small
     *                         404, if <nth> is out of range
     */
  //TSyError GetNthSync( void*  san, size_t sanSize, int nth,
    TSyError GetNthSync( int    nth,
                         int    &syncType,
                         uInt32 &contentType,
                         string &serverURI );

    /*! Alternative call for GetNthSync( 0, ... ) */
  //TSyError GetHeader ( void*  san, size_t sanSize );
    TSyError GetHeader ();


    TDigestField fDigest; // The digest, created with "CreateDigest"
    int          fNSync;  // number of actual sync fields

  private:
    /*! the internally built notification-body structure */
    byte   fEmpty;    // direct reference to empty structure
    void*  fBody;     // the body structure ...
    size_t fBodySize; // .. and its size

    /*! local copies of <san>,<sanSize> */
    void*  fSan;
    size_t fSanSize;

    /*! MD5 conversion */
    TDigestField H( string s );

    /* Try to interpret SyncML 1.1 SAN */
    TSyError Check_11( void* san, size_t sanSize );

    /*! Add <value> into field <b> at <pos>,<n> */
    void   AddBits( void* ptr, int pos, int n, uInt32 value );
    /*! Get  value  from field <b> at <pos>,<n> */
    uInt32 GetBits( void* ptr, int pos, int n );

    /*! Release notification body */
    void ReleaseNotificationBody();

    /*! Release the SAN package */
    void ReleasePackage();
}; // SanPackage


} // namespace sysync
#endif  // San_H
// eof
