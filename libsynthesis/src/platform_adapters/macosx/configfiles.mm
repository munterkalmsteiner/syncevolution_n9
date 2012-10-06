/*
 *  File:         configfiles.mm
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  iPhoneOS Platform adaptor
 *
 *  Copyright (c) 2008-2012 by plan44.ch + Synthesis AG
 *
 *  2008-06-12 : luz : created
 *
 */


#import <Foundation/NSString.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSBundle.h>


#include "sysync.h"


#ifdef MOBOSX
  #import <UIKit/UIDevice.h>
#else
  #include <sys/types.h>
  #include <pwd.h>

  extern "C" {
    #include <CoreFoundation/CoreFoundation.h>
    #include <IOKit/IOKitLib.h>
    #include <sys/sysctl.h>
  }
#endif



#ifdef MOBOSX

// iPhone

// determine OS version
static bool getOSVersion(string &aOSVersion)
{
  UIDevice *theDevice = [UIDevice currentDevice];
  // Obtain OS Version
  // - Name
  aOSVersion = [theDevice.systemName UTF8String];
  aOSVersion += "/";
  // - Version
  aOSVersion += [theDevice.systemVersion UTF8String];
  return true;
} // getOSVersion


// determine hardware name
static bool getHardwareName(string &aHardwareName)
{
  UIDevice *theDevice = [UIDevice currentDevice];
  // device name
  aHardwareName = [theDevice.model UTF8String];
  return true;
} // getHardwareName


static void getMacDir(string &aString, bool aIsTemp)
{
  // get home or temp directory
  NSString *dir;
  if (aIsTemp)
    dir = NSTemporaryDirectory();
  else
    dir = NSHomeDirectory();
  aString = [dir UTF8String];
} // getMacDir

#else

// Mac OS X

// determine OS version
static bool getOSVersion(string &aOSVersion)
{
  // Obtain OS Version
  int mib[2];
  size_t len;
  char *p;
  mib[0] = CTL_KERN;
  // - %%% we can't easily get the 10.x.x string, so what we show here is the Darwin version
  mib[1] = KERN_OSTYPE;
  sysctl(mib, 2, NULL, &len, NULL, 0);
  p = (char *)malloc(len);
  sysctl(mib, 2, p, &len, NULL, 0);
  aOSVersion = p;
  free(p);
  aOSVersion += "/";
  mib[1] = KERN_OSRELEASE;
  sysctl(mib, 2, NULL, &len, NULL, 0);
  p = (char *)malloc(len);
  sysctl(mib, 2, p, &len, NULL, 0);
  aOSVersion += p;
  free(p);
  return true;
} // getOSVersion


// determine hardware name
static bool getHardwareName(string &aHardwareName)
{
  // device name
  // - use sysctl to get machine/model name
  int mib[2];
  size_t len;
  char *p;
  mib[0] = CTL_HW;
  // - the machine (is the name on MOBOSX "iPod1,1", but the CPU Architecture for MacOSX "i386")
  mib[1] = HW_MACHINE;
  sysctl(mib, 2, NULL, &len, NULL, 0);
  p = (char *)malloc(len);
  sysctl(mib, 2, p, &len, NULL, 0);
  aHardwareName = p;
  free(p);
  aHardwareName += "/";
  // - the model (is the model number on MOBOSX "N45AP", "M68xx", but the model of MacOSX "MacBookPro1,2"
  mib[1] = HW_MODEL;
  sysctl(mib, 2, NULL, &len, NULL, 0);
  p = (char *)malloc(len);
  sysctl(mib, 2, p, &len, NULL, 0);
  aHardwareName += p;
  free(p);
  return true;
} // getHardwareName


static void getMacDir(string &aString, bool aIsTemp)
{
  #ifdef xxxxxyyyyGUGUS
  // %%% probably use getEnv(HOME) and TEMP resp.
  CFStringRef cfstr;
  if (aIsTemp)
    cfstr = (CFStringRef)NSTemporaryDirectory();
  else
    cfstr = (CFStringRef)NSHomeDirectory();
  // convert to UTF8 application string
  CFStringGetCString(cfstr,buffer,bufsiz,kCFStringEncodingUTF8);
  CFRelease(cfstr);
  aString = buffer;
  #else
  #ifdef MOBOSX
  #warning "must get real HOME and TMP env vars or leave these undefined"
  // %%% hoping for app starting in its HOME dir anyway
  if (aIsTemp)
    aString = "./tmp";
  else
    aString = ".";
  #else
  // Mac OS X is unix style
  if (aIsTemp) {
    aString = "/tmp";
  }
  else {
    struct passwd *userInfoP=NULL;
    userInfoP = getpwuid(getuid());
    aString = userInfoP->pw_dir; // home dir
  }
  #endif // mac os x
  #endif // q&d methods
} // getMacDir

#endif



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
      // temp path, common for Mac OS X and iPhoneOS
      getMacDir(aString,true);
      break;
    case pfs_userdir_path:
      getMacDir(aString,false);
      break;
    // iPhone OS, with sandboxed apps
    case pfs_prefs_path:
      // "application support" is also in sandbox Documents
    case pfs_appdata_path:
      // appdata for iPhone is just Documents in the app Sandbox
      getMacDir(aString,false);
      aString += "/Documents";
      break;
    case pfs_exedir_path:
    exedir:
      // path where module file resides (bundle path)
      aString = [[[NSBundle mainBundle] bundlePath] UTF8String];
      break;
    default:
      // unknown ID
      return false;
  }
  // ok
  return true;
} // getPlatformString



extern "C" {
  #include <ctime>
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
  uInt32 n=aPath.size();
  if (n>2 && aPath[n-1]!='/')
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


#ifdef MOBOSX

// iPhone

// get local device URI/ID
bool getLocalDeviceID(string &aURI)
{
  UIDevice *theDevice = [UIDevice currentDevice];
  // Obtain unique ID
  aURI = [theDevice.uniqueIdentifier UTF8String];
  return true;
} // getLocalDeviceID

#else

// Mac OS X

// get local device URI/ID
bool getLocalDeviceID(string &aURI)
{
  // Get serial number from IOKit
  CFStringRef serialNumber = NULL;
  io_service_t platformExpert =
    IOServiceGetMatchingService(
      kIOMasterPortDefault,
      IOServiceMatching("IOPlatformExpertDevice")
    );
  if (platformExpert) {
    CFTypeRef serialNumberAsCFString =
      IORegistryEntryCreateCFProperty(
        platformExpert,
        CFSTR(kIOPlatformSerialNumberKey),
        kCFAllocatorDefault, 0
      );
    if (serialNumberAsCFString) {
      serialNumber = (CFStringRef)serialNumberAsCFString;
    }
    IOObjectRelease(platformExpert);
  }
  if (serialNumber) {
    // found serial number
    // - get as C string
    const size_t bufSiz=100;
    char serialBuf[bufSiz];
    if (CFStringGetCString(
      serialNumber,
      serialBuf,
      bufSiz,
      kCFStringEncodingUTF8
    )) {
      // now put into STL string
      aURI = "OSX:";
      aURI += serialBuf;
    }
    // release the CFString
    CFRelease(serialNumber);
    // unique number found
    return true;
  }
  // no serial
  aURI = "OSX_without_serial";
  return false;
} // getLocalDeviceID

#endif



// write to platform's "console", whatever that is
void PlatformConsolePuts(const char *aText)
{
  // generic standard output
  puts(aText); // appends newline itself
} // PlatformConsolePuts


/* eof */
