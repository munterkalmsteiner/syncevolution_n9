/**
 *  @File     binfile.cpp
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

#include "prefix_file.h"

#include "binfilebase.h"

namespace sysync {

// TBinFileBase
// ============

// constructor
TBinFileBase::TBinFileBase() :
  fDestructed(false)
{
  fBinFileHeader.idword=0;
  fBinFileHeader.version=0;
  fBinFileHeader.headersize=sizeof(TBinFileHeader);
  fBinFileHeader.recordsize=0;
  fBinFileHeader.numrecords=0;
  fBinFileHeader.allocatedrecords=0;
  fBinFileHeader.uniquerecordid=0;
  fHeaderDirty=false;
  fExtraHeaderDirty=false;
  fExtraHeaderP=NULL;
  fExtraHeaderSize=0;
  fFoundVersion=0;
} // TBinFileBase::TBinFileBase


// destructor
TBinFileBase::~TBinFileBase()
{
  destruct();
} // TBinFileBase::~TBinFileBase


void TBinFileBase::destruct(void)
{
  if (!fDestructed) doDestruct();
  fDestructed=true;
} // TBinFileBase::destruct


void TBinFileBase::doDestruct(void)
{
  // make sure files are closed
  close();
} // TBinFileBase::doDestruct





// - set path to binary file containing the database
void TBinFileBase::setFileInfo(const char *aFilename, uInt32 aVersion, uInt32 aIdWord, uInt32 aExpectedRecordSize)
{
  close();
  fFilename=aFilename;
  fIdWord=aIdWord;
  fVersion=aVersion;
  fExpectedRecordSize=aExpectedRecordSize;
} // TBinFileBase::setFileInfo


// - copy entire binfile 1:1 without looking at header
bferr TBinFileBase::createAsCopyFrom(TBinFileBase &aSourceBinFile, bool aOverwrite)
{
  // make sure it is closed first
  close();
  aSourceBinFile.close();
  // open original
  if (!aSourceBinFile.platformOpenFile(aSourceBinFile.fFilename.c_str(), fopm_update))
    return BFE_NOTFOUND;
  // get original header into my own fBinFileHeader
  aSourceBinFile.platformSeekFile(0);
  if (!aSourceBinFile.platformReadFile(&fBinFileHeader,sizeof(fBinFileHeader))) {
    aSourceBinFile.platformCloseFile();
    return BFE_BADSTRUCT;
  }
  // calculate file size
  uInt32 filesize = fBinFileHeader.headersize + fBinFileHeader.recordsize * fBinFileHeader.allocatedrecords;
  // create output file
  if (!aOverwrite) {
    // first test for existing file
    if (platformOpenFile(fFilename.c_str(), fopm_update)) {
      // already exists, don't copy
      platformCloseFile(); // close existing file
      aSourceBinFile.platformCloseFile(); // close input
      return BFE_EXISTS; // already exists, don't overwrite
    }
  }
  if (!platformOpenFile(fFilename.c_str(), fopm_create)) {
    aSourceBinFile.platformCloseFile(); // close input
    return BFE_IOERR;
  }
  // allocate buffer to read entire file
  void *copybuffer = malloc(filesize);
  if (!copybuffer) {
    aSourceBinFile.platformCloseFile(); // close input
    platformCloseFile(); // close output
    return BFE_MEMORY; // fatal
  }
  // read everything from old file
  aSourceBinFile.platformSeekFile(0);
  if (!aSourceBinFile.platformReadFile(copybuffer,filesize)) {
    aSourceBinFile.platformCloseFile(); // close input
    platformCloseFile(); // close output
    return BFE_IOERR;
  }
  aSourceBinFile.platformCloseFile(); // done with input file
  // write everything out to new file
  if (!platformWriteFile(copybuffer,filesize)) {
    platformCloseFile(); // close output
    return BFE_IOERR;
  }
  // return memory
  free(copybuffer);
  // close output
  platformCloseFile();
  // done ok
  return BFE_OK;
}



// - try to open existing DB file according to params set with setFileInfo
bferr TBinFileBase::open(uInt32 aExtraHeadersize, void *aExtraHeaderP, TUpdateFunc aUpdateFunc)
{
  // make sure it is closed first
  close();
  // save extra header info
  fExtraHeaderSize=aExtraHeadersize;
  fExtraHeaderP=aExtraHeaderP;
  // try to open file for (binary) update
  if (!platformOpenFile(fFilename.c_str(),fopm_update))
    return BFE_NOTFOUND;
  // read header
  fHeaderDirty=false;
  platformSeekFile(0);
  if (!platformReadFile(&fBinFileHeader,sizeof(fBinFileHeader))) {
    close();
    return BFE_BADSTRUCT;
  }
  // check type and Version
  if (fBinFileHeader.idword!=fIdWord) {
    close();
    return BFE_BADTYPE;
  }
  // remember the version we found when trying to open
  fFoundVersion = fBinFileHeader.version;
  // check need for upgrade
  if (fBinFileHeader.version!=fVersion) {
    // try to update file if update-func is provided
    if (aUpdateFunc) {
      // check if we can update (no data provided for update)
      uInt32 newrecordsize=aUpdateFunc(fFoundVersion,fVersion,NULL,NULL,0);
      if (newrecordsize) {
        // we can update from current to requested version
        // - allocate buffer for all records
        uInt32 numrecords = fBinFileHeader.numrecords;
        uInt32 oldrecordsize = fBinFileHeader.recordsize;
        void *oldrecords = malloc(numrecords * oldrecordsize);
        if (!oldrecords) return BFE_MEMORY;
        // - read all current records into memory (relative to old headersize)
        readRecord(0,oldrecords,numrecords);
        // Update header because extra header might have changed in size
        if (fExtraHeaderP && (fBinFileHeader.headersize!=sizeof(TBinFileHeader)+fExtraHeaderSize)) {
          // (extra) header has changed in size
          // - read old extra header (or part of it that will be retained in case it shrinks between versions)
          uInt32 oldEHdrSz = fBinFileHeader.headersize-sizeof(TBinFileHeader);
          platformSeekFile(sizeof(TBinFileHeader));
          platformReadFile(fExtraHeaderP,oldEHdrSz<=fExtraHeaderSize ? oldEHdrSz : fExtraHeaderSize);
          // - adjust the overall header size
          fBinFileHeader.headersize = sizeof(TBinFileHeader)+fExtraHeaderSize;
          // - let the update function handle init of the extra header
          aUpdateFunc(fFoundVersion,fVersion,NULL,fExtraHeaderP,0);
          // - make sure new extra header gets written
          fExtraHeaderDirty = true;
        }
        // - modify header fields
        fBinFileHeader.version=fVersion; // update version
        fBinFileHeader.recordsize=newrecordsize; // update record size
        fHeaderDirty=true; // header must be updated
        // - write new header (to make sure file is at least as long as header+extraheader)
        flushHeader();
        // - truncate the file (taking new extra header size into account already, in case it has changed)
        truncate();
        // - now convert buffered records
        void *newrecord = malloc(newrecordsize);
        for (uInt32 i=0; i<numrecords; i++) {
          // call updatefunc to convert record
          if (aUpdateFunc(fFoundVersion,fVersion,(void *)((uInt8 *)oldrecords+i*oldrecordsize),newrecord,oldrecordsize)) {
            // save new record
            uInt32 newi;
            newRecord(newi,newrecord);
          }
        }
        // - forget buffers
        free(newrecord);
        free(oldrecords);
        // - flush new header
        flushHeader();
      }
      else {
        // cannot update
        close();
        return BFE_BADVERSION;
      }
    }
    else {
      // cannot update
      close();
      return BFE_BADVERSION;
    }
  }
  // check record compatibility
  if (fExpectedRecordSize && fExpectedRecordSize!=fBinFileHeader.recordsize) {
    close();
    return BFE_BADSTRUCT;
  }
  // check extra header compatibility
  if (fBinFileHeader.headersize<sizeof(TBinFileHeader)+fExtraHeaderSize) {
    close();
    return BFE_BADSTRUCT;
  }
  // read extra header
  if (fExtraHeaderP && fExtraHeaderSize>0) {
    platformSeekFile(sizeof(TBinFileHeader));
    platformReadFile(fExtraHeaderP,fExtraHeaderSize);
    fExtraHeaderDirty=false;
  }
  return BFE_OK;
} // TBinFileBase::open


// - create new DB file according to params set with setFileInfo
bferr TBinFileBase::create(uInt32 aRecordsize, uInt32 aExtraHeadersize, void *aExtraHeaderP, bool aOverwrite)
{
  bferr e;

  close();
  // try to open
  e=open(aExtraHeadersize,NULL); // do not pass our new header data in case there is an old file already
  if (e==BFE_NOTFOUND || aOverwrite) {
    close();
    // create new file
    if (!platformOpenFile(fFilename.c_str(),fopm_create))
      return BFE_IOERR; // could not create for some reason
    // prepare header
    fBinFileHeader.idword=fIdWord;
    fBinFileHeader.version=fVersion;
    fBinFileHeader.headersize=sizeof(TBinFileHeader)+aExtraHeadersize;
    fBinFileHeader.recordsize=aRecordsize;
    fBinFileHeader.numrecords=0;
    fBinFileHeader.allocatedrecords=0;
    fBinFileHeader.uniquerecordid=0;
    fHeaderDirty = true;
    // - link in the new extra header buffer
    fExtraHeaderP = aExtraHeaderP;
    fExtraHeaderDirty = true; // make sure it gets written
    // write entire header
    e=flushHeader();
    // opened with new version
    fFoundVersion = fVersion;
  }
  else if (e==BFE_OK) {
    // already exists
    close();
    e=BFE_EXISTS;
  }
  return e;
} // TBinFileBase::create


// - close the file
bferr TBinFileBase::close(void)
{
  if (platformFileIsOpen()) {
    // remove empty space from end of file
    truncate(fBinFileHeader.numrecords);
    // write new header
    flushHeader();
    // close file
    platformCloseFile();
  }
  return BFE_OK;
} // TBinFileBase::close


// - close and delete file (full cleanup)
bferr TBinFileBase::closeAndDelete(void)
{
  close();
  // now delete
  return platformDeleteFile(fFilename.c_str()) ? BFE_OK : BFE_IOERR;
} // TBinFileBase::closeAndDelete



// - flush the header to the file
bferr TBinFileBase::flushHeader(void)
{
  // save header if dirty
  if (fHeaderDirty) {
    platformSeekFile(0);
    platformWriteFile(&fBinFileHeader,sizeof(fBinFileHeader));
    fHeaderDirty=false;
  }
  // save extra header if existing and requested
  if (fExtraHeaderP && fExtraHeaderDirty) {
    platformSeekFile(sizeof(fBinFileHeader));
    platformWriteFile(fExtraHeaderP,fExtraHeaderSize);
    fExtraHeaderDirty=false;
  }
  // make sure data is really flushed in case we get improperly
  // destructed
  platformFlushFile();
  return BFE_OK;
} // TBinFileBase::flushHeader


// - truncate to specified number of records
bferr TBinFileBase::truncate(uInt32 aNumRecords)
{
  if (!platformFileIsOpen()) return BFE_NOTOPEN;
  platformTruncateFile(fBinFileHeader.headersize+aNumRecords*fBinFileHeader.recordsize);
  fBinFileHeader.numrecords=aNumRecords;
  fBinFileHeader.allocatedrecords=aNumRecords;
  fHeaderDirty=true;
  return BFE_OK;
} // TBinFileBase::truncate


// - read by index
bferr TBinFileBase::readRecord(uInt32 aIndex, void *aRecordData, uInt32 aNumRecords)
{
  if (!platformFileIsOpen()) return BFE_NOTOPEN;
  if (aNumRecords==0) return BFE_OK;
  // find record position in file
  if (aIndex+aNumRecords>fBinFileHeader.numrecords)
    return BFE_BADINDEX; // not enough records to read
  if (!platformSeekFile(fBinFileHeader.headersize+aIndex*fBinFileHeader.recordsize))
    return BFE_BADINDEX;
  // read data now
  if (!platformReadFile(aRecordData,fBinFileHeader.recordsize*aNumRecords))
    return BFE_IOERR; // could not read as expected
  return BFE_OK;
} // TBinFileBase::readRecord


// - update by index
bferr TBinFileBase::updateRecord(uInt32 aIndex, const void *aRecordData, uInt32 aNumRecords)
{
  if (!platformFileIsOpen()) return BFE_NOTOPEN;
  if (aNumRecords==0) return BFE_OK; // nothing to do
  // find record position in file
  if (aIndex+aNumRecords>fBinFileHeader.numrecords)
    return BFE_BADINDEX; // trying to update more records than actually here
  if (!platformSeekFile(fBinFileHeader.headersize+aIndex*fBinFileHeader.recordsize))
    return BFE_BADINDEX;
  // write data now
  if (!platformWriteFile(aRecordData,fBinFileHeader.recordsize*aNumRecords))
    return BFE_IOERR; // could not read as expected
  return BFE_OK;
} // TBinFileBase::updateRecord


// - new record, returns new index
bferr TBinFileBase::newRecord(uInt32 &aIndex, const void *aRecordData)
{
  if (!platformFileIsOpen()) return BFE_NOTOPEN;
  // go to end of file
  if (!platformSeekFile(fBinFileHeader.headersize+fBinFileHeader.numrecords*fBinFileHeader.recordsize))
    return BFE_IOERR;
  // write a new record
  if (!platformWriteFile(aRecordData,fBinFileHeader.recordsize))
    return BFE_IOERR; // could not read as expected
  // update header
  aIndex=fBinFileHeader.numrecords++; // return index of new record
  fBinFileHeader.allocatedrecords++;
  fHeaderDirty=true;
  return BFE_OK;
} // TBinFileBase::newRecord


// - delete record
bferr TBinFileBase::deleteRecord(uInt32 aIndex)
{
  if (!platformFileIsOpen()) return BFE_NOTOPEN;
  if (aIndex>=fBinFileHeader.numrecords) return BFE_BADINDEX;
  if (aIndex<fBinFileHeader.numrecords-1) {
    // we need to move last record
    void *recP = malloc(fBinFileHeader.recordsize);
    if (!recP) return BFE_IOERR; // no memory
    // read last record
    readRecord(fBinFileHeader.numrecords-1,recP);
    // write it to new position
    updateRecord(aIndex,recP);
    free(recP);
  }
  fBinFileHeader.numrecords--;
  fHeaderDirty=true;
  return BFE_OK;
} // TBinFileBase::deleteRecord

} // namespace sysync

/* end of TBinFileBase implementation */

// eof
