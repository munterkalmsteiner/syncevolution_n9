/**
 *  @File     binfile.h
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TBinFileBase
 *    Simple record based binary file storage class
 *
 *    Copyright (c) 2003-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2006-03-28 : luz : extracted into separate file from TBinfileImplDS
 */
/*
 */

#ifndef BINFILEBASE_H
#define BINFILEBASE_H

#include "generic_types.h"

// we need STL strings
#include <string>
using namespace std;


namespace sysync {

#ifndef ANDROID
#pragma pack(push,4) // 32bit
#endif

// general defines for bindatastore

// File header
typedef struct {
  uInt32 idword; // identifies the database file type
  uInt32 version; // identifies the version of the database file
  uInt32 headersize; // size of header in bytes (specific files might have additional header info)
  uInt32 recordsize; // size in bytes of a single record
  uInt32 numrecords; // number of actual records
  uInt32 allocatedrecords; // number of allocated records in the file (including empty space)
  uInt32 uniquerecordid; // ever increasing counter to generate unique IDs for records
} TBinFileHeader;

#ifndef ANDROID
#pragma pack(pop)
#endif

typedef uInt16 bferr;
#define BFE_OK          0 // ok
#define BFE_BADVERSION  1 // version mismatch
#define BFE_BADTYPE     2 // type mismatch
#define BFE_BADSTRUCT   3 // bad file structure (e.g. extra header size too big)
#define BFE_NOTFOUND    4 // file not found
#define BFE_EXISTS      5 // file already exists
#define BFE_BADINDEX    6 // no such record
#define BFE_NOTOPEN     7 // file not open
#define BFE_IOERR       8 // I/O error
#define BFE_MEMORY      9 // memory error

typedef enum {
  fopm_update,  // open for read and write
  fopm_create   // create for read and write (truncate possibly existing)
} TFileOpenModes;

// DB version update function:
// - when called with aOldRecordData==aNewRecordData==NULL, just checks if update is possible and returns new record size if yes
// - when called with aOldRecordData==NULL, but aNewRecordData!=0, aNewRecordData points to the extra header and should be
//   updated for new version (only called when extra header size is actually different)
// - when called with aOldRecordData!=0 and aNewRecordData!=0, it must update the record data to the new version.
//   This is repeated for all records in the binfile.
typedef uInt32 (*TUpdateFunc)(uInt32 aOldVersion, uInt32 aNewVersion, void *aOldRecordData, void *aNewRecordData, uInt32 aOldSize);

class TBinFileBase
{
  // construction/destruction
private:
  bool fDestructed; // flag which will be set once destruct() has been called - by the outermost derivate's destructor
public:
  TBinFileBase();
  virtual void doDestruct(void); // will be called by destruct, derived must call inherited if they implement it
  void destruct(void); // to be called by ALL destructors of derivates.
  virtual ~TBinFileBase();
  // DB file access
  // - set path to binary file containing the database (aExpectedRecordSize can be zero if record size is not predetermined by a sizeof())
  void setFileInfo(const char *aFilename, uInt32 aVersion, uInt32 aIdWord, uInt32 aExpectedRecordSize);
  // - get version file had when opening
  uInt32 getFoundVersion(void) { return fFoundVersion; };
  // - check if open
  bool isOpen(void) { return platformFileIsOpen(); };
  // - create a copy
  bferr createAsCopyFrom(TBinFileBase &aSourceBinFile, bool aOverwrite=false);
  // - try to open existing DB file according to params set with setFileInfo
  bferr open(uInt32 aExtraHeadersize=0, void *aExtraHeaderP=NULL, TUpdateFunc aUpdateFunc=NULL);
  // - create existing DB file according to params set with setFileInfo
  bferr create(uInt32 aRecordsize, uInt32 aExtraHeadersize=0, void *aExtraHeaderP=NULL, bool aOverwrite=false);
  // - truncate to specified number of records
  bferr truncate(uInt32 aNumRecords=0);
  // - make the extra header dirty
  void setExtraHeaderDirty(void) { fExtraHeaderDirty=true; };
  // - flush the header to the file
  bferr flushHeader(void);
  // - close the file
  bferr close(void);
  // - close and delete file (full cleanup)
  bferr closeAndDelete(void);
  // Info
  // - number of records
  uInt32 getNumRecords(void) { return fBinFileHeader.numrecords; };
  // - net size of record
  uInt32 getRecordSize(void) { return fBinFileHeader.recordsize; };
  // - get next unique ID (starts at 1, is never 0!)
  uInt32 getNextUniqueID(void) { return ++fBinFileHeader.uniquerecordid; fHeaderDirty=true; };
  // record access
  // - read by index
  bferr readRecord(uInt32 aIndex, void *aRecordData, uInt32 aNumRecords=1);
  // - update by index
  bferr updateRecord(uInt32 aIndex, const void *aRecorddata, uInt32 aNumRecords=1);
  // - new record
  bferr newRecord(const void *aRecorddata) { uInt32 i; return newRecord(i,aRecorddata); };
  bferr newRecord(uInt32 &aIndex, const void *aRecorddata);
  // - delete record
  bferr deleteRecord(uInt32 aIndex);
protected:
  // Platform file implementation abstraction
  // - test if platform file open
  virtual bool platformFileIsOpen(void) = 0;
  // - open file
  virtual bool platformOpenFile(cAppCharP aFilePath, TFileOpenModes aMode) = 0;
  // - close file
  virtual bool platformCloseFile(void) = 0;
  // - seek in file
  virtual bool platformSeekFile(uInt32 aPos, bool aFromEnd=false) = 0;
  // - read from file
  virtual bool platformReadFile(void *aBuffer, uInt32 aMaxRead) = 0;
  // - write to file
  virtual bool platformWriteFile(const void *aBuffer, uInt32 aBytes) = 0;
  // - flush all buffers
  virtual bool platformFlushFile(void) = 0;
  // - truncate file to a specific length
  virtual bool platformTruncateFile(uInt32 aNewSize) = 0;
  // - delete file entirely
  virtual bool platformDeleteFile(cAppCharP aFilePath) = 0;
private:
  // file identification
  string fFilename;
  uInt32 fIdWord;
  uInt32 fVersion;
  uInt32 fFoundVersion; // version of file found when opening (to check after opening for happened upgrade)
  uInt32 fExpectedRecordSize;
  // cached header
  bool fHeaderDirty; // set if header must be written back to DB file
  bool fExtraHeaderDirty; // set if extra header must be written back to DB file
  TBinFileHeader fBinFileHeader; // standard header
  void *fExtraHeaderP; // pointer to extra header that will be read at open() and written at close()
  uInt32 fExtraHeaderSize; // size of extra header bytes at fExtraHeaderP
}; // TBinFileBase


} // namespace sysync

#endif  // BINFILEBASE_H
