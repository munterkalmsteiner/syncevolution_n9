/*
 *  File:    blobs.cpp
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  DBApi database adapter
 *  BLOB (Binary Large Object) access
  *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 *
 *  E X A M P L E    C O D E
 *    (text_db interface)
 *
 */


#include "blobs.h"
#include "SDK_util.h"    // include SDK utilities
#include "SDK_support.h"


namespace sysync { // the BLOB class is part of sysync namespace



#define P_Blob "BLB_" // BLOB data


// BLOB constructor
TBlob::TBlob()
{
  fOpened= false;

  #ifdef PLATFORM_FILE
    fAttrActive= false; // the BLOB can have file dates and attributes
    fDateActive= false;
  #endif
} // constructor


// BLOB destructor
TBlob::~TBlob() {
  CloseBlob();
} // destructor



void TBlob::Init( void* aCB, cAppCharP aDBName, string aBlobPath,
                                                string aContextName,
                                                string sDevKey,
                                                string sUsrKey )
{
  fCB         = aCB;     // the callback for debug purposes only
  fDBName     = aDBName; // DBName, used for debug purposes only

  fBlobPath   = aBlobPath;
  fContextName= aContextName;
  fDevKey     = sDevKey;
  fUsrKey     = sUsrKey;
} // Init



// -------------------------------------------------------------------------
// Get the BLOB's name
string TBlob::BlobName( cItemID aID, cAppCharP aBlobID )
{
  // if it is not an item, but e.g. an admin element => take the device key
  string v=  aID->item;        if (v.empty()) v= fDevKey;
  string s=           ConcatNames( fUsrKey,   fContextName, "_" );
         s=  P_Blob + ConcatNames( v,         s,            "_" ) + "_";
         s+= aBlobID;
         s=           ConcatPaths( fBlobPath, s ); // take a specific file path
  return s;
} // BlobName



// Get the BLOB's size. For this implementation the BLOB size is
// identical to the file size.
// Operation allowed also for open BLOBs
size_t TBlob::BlobSize( string aBlobName )
{
  TSyError err;
  fpos_t   tmp;

      fName= aBlobName;
  if (fName.empty()) return 0; // A BLOB w/o a name has size 0

  bool opn= fOpened; // if not yet opened, open it temporary
  if (!opn) { err= OpenBlob( "rb" ); if (err) return 0; }
  else fgetpos( fFile, &tmp ); // save the current pos

  fseek             ( fFile, 0,SEEK_END );
  size_t size= ftell( fFile );

  if (!opn) { err= CloseBlob(); if (err) return 0; }
  else fsetpos( fFile, &tmp ); // restore the current pos

  return size;
} // BlobSize



// BLOB will be stored as separate files in this implementation
// This is the routine to open them for read or write
TSyError TBlob::OpenBlob( cAppCharP mode )
{
  CloseBlob();

       fFile= fopen( fName.c_str(), mode );
  if (!fFile) return DB_NotFound;

  fOpened= true;
  fseek         ( fFile, 0, SEEK_END );
  fCurPos= 0;
  fSize  = ftell( fFile ); // get total size
  rewind        ( fFile );
  return LOCERR_OK;
} // OpenBlob


// BLOB will be stored as separate files in this implementation
// This is the routine to close the current one.
TSyError TBlob::CloseBlob()
{
  if (fOpened) { fclose( fFile ); fOpened= false; }
  fCurPos= 0;
  fSize  = 0;
  return LOCERR_OK;
} // CloseBlob



// Attribute and date handling routines
#ifdef PLATFORM_FILE
  TSyError TBlob::GetAttr( string aBlobName, TAttr &aAttr, bool &isFolder )
  {
    TSyError err= Get_FileAttr( aBlobName, fAttr, isFolder );
  //if     (!err && isFolder) err= DB_Forbidden;

    DEBUG_Exotic_DB( fCB, fDBName.c_str(),"GetAttr", "'%s': hsadwrx=%d%d%d%d%d%d%d folder=%d err=%d",
                        aBlobName.c_str(), fAttr.h,fAttr.s,fAttr.a,fAttr.d,
                                           fAttr.w,fAttr.r,fAttr.x, isFolder, err );
    aAttr= fAttr;
    return err;
  } // GetAttr


  TSyError TBlob::SetAttr( TAttr aAttr )
  {
    fAttr      = aAttr;
    fAttrActive= true;
    DEBUG_Exotic_DB( fCB, fDBName.c_str(),"SetAttr", "hsadwrx=%d%d%d%d%d%d%d",
                                           fAttr.h,fAttr.s,fAttr.a,fAttr.d,
                                           fAttr.w,fAttr.r,fAttr.x );
    return LOCERR_OK; // no error here
  } // SetAttr


  // -----------------------------------------------------------------------------------
  TSyError TBlob::GetDates( string aBlobName, TDates &aDate )
  {
    TSyError err= Get_FileDate( aBlobName, fDate );
    DEBUG_Exotic_DB( fCB, fDBName.c_str(),"GetDates", "'%s': cre=%s mod=%s acc=%s err=%d",
                        aBlobName.c_str(), fDate.created.c_str(),
                                           fDate.modified.c_str(),
                                           fDate.accessed.c_str(), err );
    aDate= fDate;
    return err;
  } // GetDates


  TSyError TBlob::SetDates( string aBlobName, TDates aDate )
  {
    fDate       = aDate;
    fDateActive = true;
    TSyError err= Set_FileDate( aBlobName, fDate );
    DEBUG_Exotic_DB( fCB, fDBName.c_str(),"SetDates", "cre=%s mod=%s acc=%s err=%d",
                                           fDate.created.c_str(),
                                           fDate.modified.c_str(),
                                           fDate.accessed.c_str(), err );
    return LOCERR_OK; // no error here
  } // SetDates
#endif // PLATFORM_FILE



// -----------------------------------------------------------------------------------
// BLOB will be stored as separate files in this implementation
// This is the routine to read the current one or a part of it
TSyError TBlob::ReadBlob( string aBlobName, appPointer *blkPtr,
                                               memSize *blkSize, memSize *totSize,
                                                  bool  aFirst,     bool *aLast )
{
  // This example shows two values of influence:
  // - the maximum value of returned <size> is BlobBlk
  // - for blob sizes < BlobChk, the <totSize> will be returned, else 0.
  #define BlobBlk  2048
  #define BlobMax  2048 // checking limit for returning <totSize> = 0
//#define BlobMax 10000 // checking limit for returning <totSize> = 0

  TSyError   err= LOCERR_OK;
  *blkPtr = NULL; // initialize
  *totSize=    0;
  *aLast  = true;
  if (aFirst) fName= aBlobName;

  DEBUG_DB( fCB, fDBName.c_str(), Da_RB, "blobName='%s' size=%d 1st=%s",
                                  fName.c_str(), *blkSize, Bo( aFirst ) );

  do { // exit part
    if (fName.empty()) { err= DB_NotFound;  break; }

    if (aFirst) { // special treatement for <first>: open the file
            err= OpenBlob( "rb" );
      if   (err) {
        if (err==DB_NotFound) { // a not existing BLOB is not an error
            err= LOCERR_OK; *blkSize= 0;
        } // if

        break;
      } // if
    } // if
    if (!fOpened) { err= DB_Forbidden; break; } // now it must be open !

    // adapt the default size
    if (*blkSize==0 ||
        *blkSize>BlobBlk) *blkSize= BlobBlk;

                         *blkPtr= malloc( *blkSize );
    uInt32  rslt= fread( *blkPtr, 1,      *blkSize, fFile );
    *aLast= rslt!=*blkSize;

    // check if already finished
        fCurPos+= rslt;
    if (fCurPos>=fSize) *aLast  = true; // yes, it's done
    if (fSize<BlobMax)  *totSize= fSize;

    // special treatement for <last>: close the file
    if (*aLast) {
      *blkSize= rslt;
      err= CloseBlob();
    } // if
  } while (false); // end exit part

  DEBUG_DB( fCB, fDBName.c_str(), Da_RB, "blk(%08X,%d) tot=%d last=%s err=%d",
                                  *blkPtr, *blkSize, *totSize, Bo( *aLast ), err );
  return err;
} // ReadBlob

TSyError TBlob::ReadBlob( cItemID  aID,  cAppCharP  aBlobID,
                       appPointer *blkPtr, memSize *blkSize, memSize *totSize,
                             bool  aFirst,    bool *aLast ) {
  return       ReadBlob( BlobName( aID, aBlobID ),
                                   blkPtr,blkSize,totSize, aFirst,aLast );
} // ReadBlob



// BLOB will be stored as separate files in this implementation
// This is the routine to write the current one or a part of it
TSyError TBlob::WriteBlob( string aBlobName, appPointer blkPtr,
                                                memSize blkSize, memSize totSize,
                                                   bool aFirst,     bool aLast )
{
  TSyError err= LOCERR_OK;
  if   (aFirst) {            // empty blobs needn't to be written
    if (aLast && blkSize==0) return DeleteBlob( aBlobName );
    fName= aBlobName;
  } // if

  do {
    if (fName.empty()) { err= DB_NotFound; break; }

    if (aFirst) { // special treatement for <first>: open the file
      err= OpenBlob( "wb" );  if (err) break;
    } // if
    if (!fOpened) { err= DB_Forbidden; break; } // now it must be open !

    uInt32    rslt= fwrite( blkPtr, 1,blkSize, fFile );
    if       (rslt!=blkSize) err= DB_Error;
    fCurPos+= rslt;

    // special treatement for <last>: close the file
    if (aLast || err) {
      CloseBlob();

      #ifdef PLATFORM_FILE
        if (fAttrActive) Set_FileAttr( fName, fAttr );
        if (fDateActive) Set_FileDate( fName, fDate );

        fAttrActive= false; // it's written now
        fDateActive= false;
      #endif
    } // if
  } while (false);

  DEBUG_DB( fCB, fDBName.c_str(),Da_WB, "blobName='%s' blk(%08X,%d) tot=%d",
                                         fName.c_str(), blkPtr, blkSize, totSize );
  DEBUG_DB( fCB, fDBName.c_str(),Da_WB, "1st=%s last=%s err=%d",
                                         Bo( aFirst ), Bo( aLast ), err );
  return err;
} // WriteBlob

TSyError TBlob::WriteBlob(   cItemID aID,  cAppCharP aBlobID,
                          appPointer blkPtr, memSize blkSize, memSize totSize,
                                bool aFirst,    bool aLast ) {
  return        WriteBlob( BlobName( aID, aBlobID ),
                                     blkPtr,blkSize,totSize, aFirst,aLast );
} // WriteBlob



// BLOB will be stored as separate files in this implementation
// This is the routine to delete the current one.
TSyError TBlob::DeleteBlob( string aBlobName )
{
  CloseBlob(); // close potential old one

  TSyError err= LOCERR_OK;
  fName= aBlobName;

  do { // exit part
    if (fName.empty()) { err= DB_NotFound;  break; }
    if (fOpened)       { err= DB_Forbidden; break; }

    remove( fName.c_str() ); // not existing BLOB is not an error
  } while (false); // end exit part

  DEBUG_DB( fCB, fDBName.c_str(),Da_DB, "blobName='%s' err=%d", fName.c_str(), err );
  return err;
} // DeleteBlob

TSyError TBlob::DeleteBlob(  cItemID  aID, cAppCharP aBlobID ) {
  return        DeleteBlob( BlobName( aID,           aBlobID ) );
} // DeleteBlob


} /* namespace */
/* eof */
