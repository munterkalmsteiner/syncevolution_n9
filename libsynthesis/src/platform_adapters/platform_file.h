/*
 *  File:    platform_file.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  General interface to get/set file info
 *  like date/attributes/...
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 *
 */

#ifndef PLATFORM_FILE_H
#define PLATFORM_FILE_H

#include "sync_dbapidef.h"
#include <string>
using namespace std;


namespace sysync {


/*! File attributes */
struct TAttr {
  bool h,s,a,d,w,r,x;
}; // TAttr


/*! File dates */
struct TDates {
  /* ISO8601 format, usually as localtime */
  string created, modified, accessed;
}; // TDates

/* Get/set attributes */
TSyError Get_FileAttr( string pathName, TAttr  &aAttr, bool &isFolder );
TSyError Set_FileAttr( string pathName, TAttr   aAttr );

/* Get/set file dates */
TSyError Get_FileDate( string pathName, TDates &aDate );
TSyError Set_FileDate( string pathName, TDates  aDate );


} // namespace
#endif /* PLATFORM_FILE_H */
/* eof */
