/**
 *  @File     binfile.h
 *
 *  @Author   Lukas Zeller (luz@plan44.ch)
 *
 *  @brief TBinFile
 *    Standard C file implementation of TBinFile
 *
 *    Copyright (c) 2003-2011 by Synthesis AG + plan44.ch
 *
 *  @Date 2006-03-28 : luz : created
 */
/*
 */

#ifndef BINFILE_H
#define BINFILE_H

#include "binfilebase.h"

#ifdef ANDROID
#include <stdio.h>
#include <unistd.h>
#endif

namespace sysync {

class TBinFile : public TBinFileBase
{
  typedef TBinFileBase inherited;
public:
  TBinFile();
  virtual ~TBinFile();
protected:
  // Platform file implementation abstraction
  // - test if platform file open
  virtual bool platformFileIsOpen(void);
  // - open file
  virtual bool platformOpenFile(cAppCharP aFilePath, TFileOpenModes aMode);
  // - close file
  virtual bool platformCloseFile(void);
  // - seek in file
  virtual bool platformSeekFile(uInt32 aPos, bool aFromEnd=false);
  // - read from file
  virtual bool platformReadFile(void *aBuffer, uInt32 aMaxRead);
  // - write to file
  virtual bool platformWriteFile(const void *aBuffer, uInt32 aBytes);
  // - flush all buffers
  virtual bool platformFlushFile(void);
  // - truncate file to a specific length
  virtual bool platformTruncateFile(uInt32 aNewSize);
  // - delete file entirely
  virtual bool platformDeleteFile(cAppCharP aFilePath);
private:
  FILE *fCBinFile;
}; // TBinFile

} // namespace sysync

#endif  // BINFILE_H
