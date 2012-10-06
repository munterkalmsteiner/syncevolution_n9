/*
 *  File:    platform_file.cpp
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  General interface to get/set file info
 *  like date/attributes/...
 *  >>>> UNIX Version <<<<
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 *
 */

#include "platform_file.h"
#include <time.h>


namespace sysync {


TSyError Get_FileAttr( string /* pathName */, TAttr &attr, bool &isFolder )
{
  /*
  WIN32_FIND_DATA                   data;
  TSyError err= FileData( pathName, data );
  if     (!err) {
    DWORD   att= data.dwFileAttributes;
    attr.h= att & FILE_ATTRIBUTE_HIDDEN;
    attr.s= att & FILE_ATTRIBUTE_SYSTEM;
    attr.a= att & FILE_ATTRIBUTE_ARCHIVE;
    attr.d= false; // delete
    attr.w= true;
    attr.r= true;
    attr.x= false;
  } // if

  return err;
  */

  attr.h= false;
  attr.s= false;
  attr.a= false;
  attr.d= false; // delete
  attr.w= true;
  attr.r= true;
  attr.x= false;

  isFolder= false;

  return LOCERR_OK;
} // Get_FileAttr


TSyError Set_FileAttr( string /* pathName */, TAttr /* aAttr */ )
{
  return LOCERR_OK;
} // Set_FileAttr


static void FTime( time_t u, string &utcStr )
{
  char f[ 20 ];
  struct tm* t= localtime( &u );
  sprintf( f, "%04d%02d%02dT%02d%02d%02d", t->tm_year+1900, t->tm_mon+1, t->tm_mday,
                                           t->tm_hour,      t->tm_min,   t->tm_sec );
  utcStr=  f;
} // FTime


extern "C" {
  #include <ctime>
  #include <sys/stat.h>
  #include <utime.h>
}


static TSyError FSTime( string tStr, time_t &u )
// UTC is currently not supported
{
  string      tt= tStr.substr(  8,1 );
  if       (!(tt=="T")) return DB_Forbidden;
              tt= tStr.substr( 15,1 );
  //bool usUTC= tt=="Z";

  time( &u ); // get some defaults
  struct tm* t= localtime( &u );

  t->tm_year  = atoi( tStr.substr(  0,4 ).c_str() )-1900;
  t->tm_mon   = atoi( tStr.substr(  4,2 ).c_str() )-1;
  t->tm_mday  = atoi( tStr.substr(  6,2 ).c_str() );
                                // "T"
  t->tm_hour  = atoi( tStr.substr(  9,2 ).c_str() );
  t->tm_min   = atoi( tStr.substr( 11,2 ).c_str() );
  t->tm_sec   = atoi( tStr.substr( 13,2 ).c_str() );

  t->tm_wday  = 0; // avoid troubles
  t->tm_yday  = 0;
//t->tm_isdst = 1;

  u= mktime( t );
  return LOCERR_OK;
} // FSTime


TSyError Get_FileDate( string pathName, TDates &aDate )
{
  struct stat info;
  TSyError err= stat( pathName.c_str(), &info );
  if     (!err) {
    FTime( info.st_ctime, aDate.created  );
    FTime( info.st_mtime, aDate.modified );
    FTime( info.st_atime, aDate.accessed );
  } // if

  return err;
} // Get_FileDate


TSyError Set_FileDate( string pathName, TDates aDate )
{
  TSyError err;
  struct utimbuf buf;
  err= FSTime( aDate.created,  buf.modtime ); if (err) return err;
  err= FSTime( aDate.modified, buf.modtime ); if (err) return err;
  err= FSTime( aDate.accessed, buf.actime  ); if (err) return err;

//string uu;
//FTime( buf.modtime, uu ); printf( "uum='%s'\n", uu.c_str() );
//FTime( buf.actime,  uu ); printf( "uua='%s'\n", uu.c_str() );

  return utime( pathName.c_str(), &buf ); return err;
} // Set_FileDate


} /* namespace */
/* eof */
