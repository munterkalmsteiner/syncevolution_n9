/*
 *  File:         configfiles.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Linux Platform adaptor for accessing config files
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 *  2002-11-18 : luz : created
 *
 *
 */

#include "sysync.h"

#ifdef ANDROID
#include <unistd.h>
#endif

#include <sys/types.h>
#include <pwd.h>


// determine OS version
static bool getOSVersion(string &aOSVersion)
{
  // Obtain OS Version
  //%%%% for now
  aOSVersion="unknown";
  return false;
} // getOSVersion


// determine hardware name
static bool getHardwareName(string &aHardwareName)
{
  // Obtain Device name
  #ifdef ANDROID
  aHardwareName="Android Device";
  #else
  aHardwareName="Linux PC";
  #endif
  return true;
} // getHardwareName


/// @brief get platform specific string by ID
/// @return false if string is not available for this platform
/// @param aStringID string selector ID
/// @param aString will receive the requested string
bool getPlatformString(TPlatformStringID aStringID, string &aString)
{
  const sInt16 bufsiz=1024;
  char buffer[bufsiz];
  buffer[0]=0; // terminate for safety
  string str;
  string aSub;
  struct passwd *userInfoP=NULL;

  switch (aStringID) {
    case pfs_platformvers:
      // Platform OS version string
      getOSVersion(aString);
      break;
    case pfs_device_uri:
      getLocalDeviceID(aString);
      break;
    case pfs_device_name:
      getHardwareName(aString);
      break;
    case pfs_loccfg_path:
      #ifdef STANDALONE_APP
      // default path for local config for XPT standalones is the current dir
      aString = ".";
      break;
      #endif
      // for server modules, it is same as the global config path
    #ifndef STANDALONE_APP
    case pfs_globcfg_path:
      // global config directory path
      aString = "/etc";
      break;
    #endif
    case pfs_defout_path:
      // default output path (formerly: default write path)
      #ifdef STANDALONE_APP
      // default path for local config for XPT standalones is the current dir
      aString = ".";
      #else
      // for server modules, default log path is /var/log
      aString = "/var/log";
      #endif
      break;
    case pfs_temp_path:
      // temp path
      aString = "/temp";
      break;
    case pfs_userdir_path:
      // user's home dir for user-visible documents and files
      userInfoP = getpwuid(getuid());
      aString = userInfoP->pw_dir; // home dir
      break;
    #ifdef APPDATA_SUBDIR
    case pfs_appdata_path:
      // My specific subdirectory for storing my app data/prefs
      userInfoP = getpwuid(getuid());
      aString = userInfoP->pw_dir; // user home dir
      aSub = APPDATA_SUBDIR;
      #ifdef ANDROID
      aString += "/data/com.sysync"; // application specific subdir for android
      if (!aSub.empty()) aString+= "/"; // slash only if subdir is really there
      #else
      aString += "/.sysync/"; // application specific subdir
      // don't adapt it here to avoid potential problems on other platforms
      #endif
      aString += aSub;
      break;
    #endif
    /*
    case pfs_prefs_path:
      // general directory where all applications store data & prefs (for current user)
      aString = %%%
      break;
    */
    /*
    case pfs_exedir_path:
    exedir:
      // path where module file resides
      aString = %%%
      break;
    */
    default:
      // unknown ID
      return false;
  }
  // ok
  return true;
} // getPlatformString


extern "C" {
  #ifndef ANDROID
  #include <ctime>
  #endif

  #include <sys/stat.h>
}

/// @brief update string such that it can be used as target OS directory path
///   (filename can be appended without any separator)
/// @param aPath path to be updated
/// @param aMakeDirs if set, directories along path are created if not existing
/// @return false if directory does not exist or could not be created (when aMakeDirs is set)
bool makeOSDirPath(string &aPath, bool aMakeDirs)
{
  // make sure path ends with backslash
  string::size_type n=aPath.size();
  if (n>=1 && aPath[n-1]!='/')
    aPath+='/';
  // now make sure path exists if requested
  if (aMakeDirs) {
    // no slash at the end
    string tmppath;
    tmppath.assign(aPath,0,aPath.size()-1);
    // optimization check if entire path exists
    struct stat statinfo;
    int rc=stat(aPath.c_str(),&statinfo);
    if(rc!=0 || !S_ISDIR(statinfo.st_mode)) {
      #ifdef ANDROID
      rc = 0; // BFO_INCOMPLETE
      #else
      rc = errno;
      #endif
      // path does not exist yet - start from beginning to create it
      n=0;
      bool knownmissing=false;
      // skip first slash for absolute paths
      if (aPath[0]=='/') n+=1; // skip
      do {
        // find first directory in path
        n=aPath.find("/",n);
        // if no more separators to find, all dirs exist now
        if (n==string::npos) break;
        tmppath.assign(aPath,0,n);
        n+=1; // skip separator
        if (!knownmissing) {
          // test if this dir exists
          rc=stat(tmppath.c_str(),&statinfo);
          if(rc!=0 || !S_ISDIR(statinfo.st_mode)) {
            #ifdef ANDROID
            rc = 0; // BFO_INCOMPLETE
            #else
            rc = errno;
            #endif
            knownmissing=true;
          }
        }
        if (knownmissing) {
          // create the subdir (might fail if part of path already exists)
          if (mkdir(tmppath.c_str(),S_IRWXU)!=0)
            return false; // failure to create directory
        }
      } while(true);
    } // path does not exist yet entirely
  } // make path existing
  return true;
} // makeOSDirPath


// returns timestamp (UTC) of last file modification or 0 if none known
lineartime_t getFileModificationDate(const char *aFileName)
{
  struct stat st;
  lineartime_t res;

  if (stat(aFileName,&st)!=0) res=0;
  else {
    // stat ok, get modification date
    res= ((lineartime_t)st.st_mtime * secondToLinearTimeFactor) + UnixToLineartimeOffset;
  }
  // return timestamp
  return res;
} // getFileModificationDate


#include <netdb.h>
#include <unistd.h>

// get local device URI/name info
bool getLocalDeviceID(string &aURI)
{
  char     szHostname[100];
  struct hostent *pHostEnt=NULL;
  string hostName;

  // get name of own machine
  if (gethostname(szHostname, sizeof(szHostname))!=0) {
    hostName="_unknown_";
  }
  else {
    // get host entry
    pHostEnt = gethostbyname(szHostname);
    // return fully qualified name of machine as ID
    if (pHostEnt)
      hostName=pHostEnt->h_name; // DNS name of machine
    else
      hostName=szHostname; // just name of machine
  }
  // generate URI from name
  #ifdef ANDROID
  aURI="android:";
  #else
  aURI="linux:"; // %%% SCTS does not like http:// here, so we take os:xxxx
  #endif
  // add name of this machine (fully qualified if possible)
  aURI+=hostName;
  // this is more or less unique
  return true;
} // getLocalDeviceID


// write to platform's "console", whatever that is
void PlatformConsolePuts(const char *aText)
{
  // generic output
  #ifdef ANDROID
  //__android_log_write( ANDROID_LOG_DEBUG, "ConsolePuts", aText );
  #else
    puts(aText); // appends newline itself
  #endif
} // PlatformConsolePuts



/* eof */
