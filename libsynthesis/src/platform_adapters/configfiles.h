/*
 *  File:         configfiles.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Platform adaptors for accessing config files
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 *  2002-11-18 : luz : created
 *
 *
 */

// conditional field length specs for Xprintf
#ifdef __PALM_OS__
  // PalmOS do not support all formatting
  #define FMT_LENGTH(n)
  #define FMT_LENGTH_LIMITED(n,p) ((string(p,strlen(p)>n ? n : strlen(p))).c_str())
  #define PRINTF_LLD "%ld"
  #define PRINTF_LLD_ARG(x) ((long)x)
  #ifdef NO64BITINT
    #define LONGLONGTOSTR(s,ll) %%%%%%% // cause compiling error
  #else
    #define LONGLONGTOSTR(s,ll) LongLongToStr(s,ll)
    // need a function to do that
    void LongLongToStr(string &s, sInt64 ll);
  #endif
#else
  #define PRINTF_LLD "%lld"
  #define PRINTF_LLD_ARG(x) static_cast<long long>(x)
  #define FMT_LENGTH(n) n
  #define FMT_LENGTH_LIMITED(n,p) p
  #define LONGLONGTOSTR(s,ll) StringObjPrintf(s,"%lld",ll)
#endif

/* %%% replaced by getPlatformString()
// get default paths
// - for writing logs (when we have no configured log dir)
bool getDefaultWritePath(string &aString);
// - for reading local config (first try)
bool getDefaultLocalConfigPath(string &aString);
// - for reading global config (second try)
bool getDefaultGlobalConfigPath(string &aString);
*/


// IDs to request platform-specific strings (mostly paths)
// Note: paths do NOT have a trailing separator!
typedef enum {
  pfs_platformvers, // version string of the current platform (as from getFirmwareVersion())
  pfs_globcfg_path, // global system-wide config path (such as C:\Windows or /etc)
  pfs_loccfg_path, // local config path (such as exedir or user's dir)
  pfs_defout_path, // default path to writable directory to write default logs
  pfs_temp_path, // path where we can write temp files
  pfs_exedir_path, // path to directory where executable resides
  pfs_userdir_path, // path to the user's home directory for user-visible documents and files
  pfs_appdata_path, // path to the user's preference directory for this application
  pfs_prefs_path, // path to directory where all application prefs reside (not just mine)
  pfs_device_uri, // URI of the device (as from former getDeviceInfo)
  pfs_device_name, // Name of the device (as from former getHardwareVersion())
  pfs_user_name, // Name of the current user
  numPlatformStrings
} TPlatformStringID;

/// @brief get platform specific string by ID
/// @return false if string is not available for this platform
/// @param aStringID string selector ID
/// @param aString will receive the requested string
bool getPlatformString(TPlatformStringID aStringID, string &aString);


/// @brief update string such that it can be used as target OS directory path
///   (filename can be appended without any separator)
/// @param aPath path to be updated
/// @param aMakeDirs if set, directories along path are created if not existing
/// @return false if directory does not exist or could not be created (when aMakeDirs is set)
bool makeOSDirPath(string &aPath, bool aMakeDirs=false);


// returns timestamp (UTC) of last file modification or 0 if none known
lineartime_t getFileModificationDate(const char *aFileName);

// get local device URI (returns false if ID cannot be guaranteed unique)
bool getLocalDeviceID(string &aURI);


// write to platform's "console", whatever that is
void PlatformConsolePuts(const char *aText);



/* eof */
