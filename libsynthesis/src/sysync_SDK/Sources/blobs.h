/*
 *  File:    blobs.h
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
 *
 */

#ifndef BLOBS_H
#define BLOBS_H

#include "sync_dbapidef.h" // get some definitions
#ifdef PLATFORM_FILE
  #include "platform_file.h"
#endif

#include <string>

namespace sysync {



/* NOTE: A BLOB object can be used for different BLOBs,
 *       but only for one BLOB simultaneously.
 *       If a new BLOB will be opened, the old one will
 *       be closed automatically, if still opened.
 *       read/write with <aFirst> = true  will open a new one
 *         "    "     "      "    = false will ignore <aBlobName>
 */
class TBlob {
  public:
    TBlob();
   ~TBlob();

    void     Init( void* aCB, cAppCharP aDBName, string aBlobPath,
                                                 string aContextName,
                                                 string sDevKey,
                                                 string sUsrKey );

    string   BlobName  ( cItemID  aID, cAppCharP aBlobID );

    size_t   BlobSize  (  string  aBlobName ); // allowed also for open BLOBs

    TSyError ReadBlob  (  string  aBlobName,
                      appPointer *blkPtr, memSize *blkSize, memSize *totSize,
                            bool  aFirst,    bool *aLast );
    TSyError ReadBlob  ( cItemID  aID,  cAppCharP  aBlobID,
                      appPointer *blkPtr, memSize *blkSize, memSize *totSize,
                            bool  aFirst,    bool *aLast );

    TSyError WriteBlob (  string  aBlobName,
                      appPointer  blkPtr, memSize  blkSize, memSize  totSize,
                            bool  aFirst,    bool  aLast );
    TSyError WriteBlob ( cItemID  aID,  cAppCharP  aBlobID,
                      appPointer  blkPtr, memSize  blkSize, memSize  totSize,
                            bool  aFirst,    bool  aLast );

    TSyError DeleteBlob(  string  aBlobName );
    TSyError DeleteBlob( cItemID  aID, cAppCharP  aBlobID );

    #ifdef PLATFORM_FILE
      TSyError GetAttr ( string  aBlobName, TAttr  &aAttr, bool &isFolder ); // get BLOB's file attributes
      TSyError SetAttr                    ( TAttr   aAttr );                 // set  "      "      "

      TSyError GetDates( string  aBlobName, TDates &aDate );                 // get BLOB's file dates
      TSyError SetDates( string  aBlobName, TDates  aDate );                 // set  "      "      "
    #endif

    string getDBName() const { return fDBName; }
    string getBlobPath() const { return fBlobPath; }
    string getContextName() const { return fContextName; }
    string getDevKey() const { return fDevKey; }
    string getUsrKey() const { return fUsrKey; }

  private:
    void*  fCB;       // callback structure, for debug logs
    string fDBName;   // database name,      for debug logs

    string fBlobPath; // params for creating BLOB's name
    string fContextName;
    string fDevKey;
    string fUsrKey;

    FILE*   fFile;     // assigned file
    bool    fOpened;   // Is it currently opened ?
    memSize fCurPos;   // current position
    memSize fSize;     // BLOB's size

    string fName;     // BLOB's file name

    #ifdef PLATFORM_FILE
      TAttr  fAttr;   // BLOB's file attributes
      bool   fAttrActive;

      TDates fDate;   // BLOB's file dates
      bool   fDateActive;
    #endif

    TSyError  OpenBlob( const char* mode );
    TSyError CloseBlob();
}; // TBlob


} /* namespace */
#endif /* BLOBS_H */
/* eof */
