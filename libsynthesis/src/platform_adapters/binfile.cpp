/**
 *  @File     binfile.cpp
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

#include "prefix_file.h"
#include "binfile.h"

#ifdef WINCE
#include "win32_utils.h"
#endif

namespace sysync {


// @brief constructor
TBinFile::TBinFile() :
  fCBinFile(NULL)
{
} // TBinFile


// @brief destructor
TBinFile::~TBinFile()
{
  destruct();
} // TBinFile::~TBinFile


// @brief test if platform file open
bool TBinFile::platformFileIsOpen(void)
{
  return fCBinFile!=NULL;
} // TBinFile::platformFileIsOpen


/// @brief Open a platform file
bool TBinFile::platformOpenFile(cAppCharP aFilePath, TFileOpenModes aMode)
{
  switch (aMode) {
    case fopm_update :
      fCBinFile = fopen(aFilePath,"r+b");
      break;
    case fopm_create :
      fCBinFile = fopen(aFilePath,"w+b");
      break;
  }
  return fCBinFile!=NULL;
} // TBinFile::platformOpenFile


/// @brief close file
bool TBinFile::platformCloseFile(void)
{
  fclose(fCBinFile);
  fCBinFile=NULL;
  return true;
} // TBinFile::platformCloseFile


/// @brief seek in file
bool TBinFile::platformSeekFile(uInt32 aPos, bool aFromEnd)
{
  return fseek(fCBinFile,aPos,aFromEnd ? SEEK_END : SEEK_SET) == 0;
} // TBinFile::platformSeekFile


/// @brief read from file
bool TBinFile::platformReadFile(void *aBuffer, uInt32 aMaxRead)
{
  return fread(aBuffer,aMaxRead,1,fCBinFile) == 1;
} // TBinFile::platformReadFile


/// @brief write to file
bool TBinFile::platformWriteFile(const void *aBuffer, uInt32 aBytes)
{
  return fwrite(aBuffer,aBytes,1,fCBinFile) == 1;
} // TBinFile::platformWriteFile


/// @brief flush all buffers
bool TBinFile::platformFlushFile(void)
{
  return fflush(fCBinFile)==0;
} // TBinFile::platformFlushFile


/// @brief truncate file to a specific length
bool TBinFile::platformTruncateFile(uInt32 aNewSize)
{
  #if defined(LINUX) || defined(MACOSX)
    fflush(fCBinFile); // unbuffer everything
    int fd = fileno(fCBinFile); // get file descriptor
    if (ftruncate(fd,aNewSize)) {
      ; // error ignored
    }
  #elif defined(WIN32)
    fflush(fCBinFile); // unbuffer everything
    HANDLE h = (HANDLE)fileno(fCBinFile); // get file descriptor
    SetFilePointer(h,aNewSize,NULL,FILE_BEGIN);
    SetEndOfFile(h);
  #elif defined(__EPOC_OS__)
    #ifdef RELEASE_VERSION
      #warning "%%% we need file truncation for symbian here"
    #endif
    return false;
  #else
    #error "file truncation not implemented for this platform"
  #endif
  // successfully truncated
  return true;
} // TBinFile::platformTruncateFile


/// @delete file entirely
bool TBinFile::platformDeleteFile(cAppCharP aFilePath)
{
  #if defined(LINUX) || defined(MACOSX)
    unlink(aFilePath);
  #elif defined(WINCE)
    TCHAR filename[MAX_PATH];
    UTF8toWCharBuf(aFilePath,filename,MAX_PATH);
    DeleteFileW(filename);
  #elif defined(WIN32)
    unlink(aFilePath);
  #else
    #error "file delete not implemented for this platform"
  #endif
  // successfully deleted
  return true;
} // TBinFile::platformDeleteFile



} // namespace sysync

// eof
