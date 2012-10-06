/*
 *  File:         clientprovisioning_inc.cpp
 *
 *  Include file for binfileagent.cpp and palmdbagent.cpp to provide
 *  string-command based client provisioning.
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Copyright (c) 2003-2011 by Synthesis AG + plan44.ch
 *
 *  2003-09-30 : luz : created
 *
 *
 */

#ifndef ENHANCED_PROFILES_2004
  #error "no support any more for non-ENHANCED_PROFILES_2004"
#endif


// Configure profile
// - params must always start with pf="profilename" or defpf="profilename"
// - aActiveProfile is updated only if profile was actually added, changed or deleted
static bool configProfile(CLIENTAGENTCONFIG *aCfgP, const char *aParm, bool aMayAdd, bool aMayUpdate, sInt32 &aActiveProfile)
{
  #ifdef MULTI_PROFILE_SUPPORT
  const sInt32 maxprofiles = 30; // prevent overflowing popup too much
  #else
  const sInt32 maxprofiles = 1; // only one profile
  #endif
  string tag,pfnam,value;
  // get profile name
  aParm=nextTag(aParm,tag,pfnam);
  if (!aParm) return false;
  bool def=false;
  if (tag=="defpf") def=true;
  else if (!(tag=="pf")) return false;
  // find profile by name
  sInt32 np = aCfgP->numProfiles();
  sInt32 pi = np;
  PROFILERECORD profile;
  bool found=false;
  while (pi>0) {
    pi--;
    aCfgP->getProfile(pi,profile);
    if (strucmp(profile.profileName,pfnam.c_str())==0) {
      found=true;
      break;
    }
  }
  // now profile is current profile record if found==true
  // - check delete first
  if (!aMayAdd && !aMayUpdate) {
    // delete requested
    if (!found) return false; // does not exist
    if (np<2) return false; // only one profile left, do not delete
    // perform delete
    aCfgP->deleteProfile(pi);
    aActiveProfile=0; // re-activate profile 0
    return true;
  }
  // - For update (modify or add): If profile not found AND...
  //   ...if a default profile is specified -> overwrite first profile
  //   ...if single-profile client -> overwrite the profile
  if (!found && aMayUpdate && aMayAdd && (def || maxprofiles==1) && np>0) {
    pi=0; // first is default
    aCfgP->getProfile(pi,profile);
    AssignCString(profile.profileName,pfnam.c_str(),maxnamesiz);
    found=true;
  }
  // - if not found and add not allowed or already too many profiles -> exit
  if (!found && (!aMayAdd || np>=maxprofiles)) return false; // nothing added
  // - if found, but update not allowed -> exit
  if (found && !aMayUpdate) return false; // nothing updated
  // Now, create new profile if we don't modify
  if (!found) {
    // - create new profile without default settings
    pi = aCfgP->newProfile(pfnam.c_str(),false);
    // - get the record
    aCfgP->getProfile(pi,profile);
  }
  // now set the params
  sInt32 ti=-1; // no target selected yet
  TARGETRECORD target;
  uInt32 profileID=aCfgP->getIDOfProfile(pi);
  // loop through params
  while (aParm!=NULL) {
    // get next tag
    aParm=nextTag(aParm,tag,value);
    // process tag
    if (tag=="db") {
      // save current target first, if any
      if (ti>=0) aCfgP->writeTarget(ti,target);
      // select new target by DB name
      sInt32 si=0;
      do {
        ti=aCfgP->findTargetIndex(profileID,si++);
        if (ti<0) break; // all checked, none found
        // check db name
        ti=aCfgP->getTarget(ti,target);
      } while(!(value==target.dbname));
    }
    else if (tag=="uri") {
      // URI (or URI suffix)
      #ifdef HARD_CODED_SERVER_URI
      // - URI is fixed, so specified URI must be URI suffix (from old pre-2004 profiles)
      AssignCString(profile.URIpath,value.c_str(),maxpathsiz);
      #else
      // - URI can be set, except if config specifies a fixed URI
      if (aCfgP->fServerURI.empty())
        AssignCString(profile.serverURI,value.c_str(),maxurisiz);
      #endif
      // setting URI resets the device to unknown SyncML version
      profile.lastSyncMLVersion=syncml_vers_unknown;
    }
    #ifdef PROTOCOL_SELECTOR
    else if (tag=="proto") {
      // Protocol selector
      profile.protocol = (TTransportProtocols)uint16Val(value.c_str());
      if (profile.protocol>=num_transp_protos)
        profile.protocol=transp_proto_uri;
    }
    #endif
    else if (tag=="syncmlvers") {
      // initial SyncML version
      profile.lastSyncMLVersion = (TSyncMLVersions)uint16Val(value.c_str());
      if (profile.lastSyncMLVersion>=numSyncMLVersions)
        profile.lastSyncMLVersion=syncml_vers_unknown;
    }
    else if (tag=="uripath") {
      // URI path
      AssignCString(profile.URIpath,value.c_str(),maxpathsiz);
    }
    else if (tag=="roflags") {
      // Read only flags
      profile.readOnlyFlags=uint16Val(value.c_str());
    }
    else if (tag=="ftrflags") {
      // Feature flags
      profile.featureFlags=uint16Val(value.c_str());
    }
    else if (tag=="dsflags") {
      // Datastore available flags
      profile.dsAvailFlags=uint16Val(value.c_str());
    }
    else if (tag=="user") {
      // User
      AssignCString(profile.serverUser,value.c_str(),maxupwsiz);
    }
    else if (tag=="pwd") {
      // password
      assignMangledToCString(profile.serverPassword, value.c_str(), maxupwsiz,true);
    }
    else if (tag=="httpuser") {
      // User
      AssignCString(profile.transportUser,value.c_str(),maxupwsiz);
    }
    else if (tag=="httppwd") {
      // password
      assignMangledToCString(profile.transportPassword, value.c_str(), maxupwsiz,true);
    }
    else if (tag=="transpflags") {
      // transport related flags
      profile.transpFlags=uint32Val(value.c_str());
    }
    else if (tag=="profileflags") {
      // general profile flags
      profile.transpFlags=uint32Val(value.c_str());
    }
    #ifdef PROXY_SUPPORT
    else if (tag=="proxy") {
      // User
      AssignCString(profile.proxyHost,value.c_str(),maxurisiz);
    }
    else if (tag=="socks") {
      // User
      AssignCString(profile.socksHost,value.c_str(),maxurisiz);
    }
    else if (tag=="connproxy") {
      // use connection's proxy settings
      profile.useConnectionProxy=boolVal(value.c_str());
    }
    else if (tag=="useproxy") {
      // use connection's proxy settings
      profile.useProxy=boolVal(value.c_str());
    }
    else if (tag=="proxyuser") {
      // User
      AssignCString(profile.proxyUser,value.c_str(),maxupwsiz);
    }
    else if (tag=="proxypwd") {
      // password
      assignMangledToCString(profile.proxyPassword, value.c_str(), maxupwsiz,true);
    }
    #endif
    #ifdef AUTOSYNC_SUPPORT
    // Autosync activity schedule
    if (strucmp(tag.c_str(),"asl",3)==0) {
      // autosync level
      do {
        // - get level number
        if (tag.size()<5) break; // too short
        uInt8 lvl=tag[3]-0x30-1; // get level number 1..n and convert to 0-based index
        if (lvl>=NUM_AUTOSYNC_LEVELS) break; // invalid level number
        if (tag[4]!='_') break;
        // - level number found, check for subtag
        const char *subtag = tag.c_str()+5;
        if (strucmp(subtag,"mode")==0) {
          // autosync mode
          profile.AutoSyncLevel[lvl].Mode=(TAutosyncModes)uint16Val(value.c_str());
        }
        else if (strucmp(subtag,"start")==0) {
          // start day time for autosync
          profile.AutoSyncLevel[lvl].StartDayTime=uint16Val(value.c_str());
        }
        else if (strucmp(subtag,"end")==0) {
          // end day time for autosync
          profile.AutoSyncLevel[lvl].EndDayTime=uint16Val(value.c_str());
        }
        else if (strucmp(subtag,"wdays")==0) {
          // weekdays for autosync (bit 0=Su, 1=Mo .. 6=Sa)
          profile.AutoSyncLevel[lvl].WeekdayMask=uint16Val(value.c_str());
        }
        else if (strucmp(subtag,"charge")==0) {
          // charge level (% of full battery)
          profile.AutoSyncLevel[lvl].ChargeLevel=uint16Val(value.c_str());
        }
        else if (strucmp(subtag,"mem")==0) {
          // min free mem level (% of total memory)
          profile.AutoSyncLevel[lvl].MemLevel=uint16Val(value.c_str());
        }
        else if (strucmp(subtag,"flags")==0) {
          // extra flags
          profile.AutoSyncLevel[lvl].Flags=uint16Val(value.c_str());
        }
      } while(false);
    } // aslvlN_xxxx
    #ifdef IPP_SUPPORT
    else if (strucmp(tag.c_str(),"ipp_",4)==0) {
      // IPP parameters, parse rest after ipp_ prefix
      aCfgP->ipp_setparam(tag.c_str()+4,value.c_str(),profile.ippSettings);
    }
    #endif
    // Timed sync schedule
    else if (tag=="ts_mobile") {
      // interval (minutes) for mobile timed sync
      profile.TimedSyncMobilePeriod=uint16Val(value.c_str());
    }
    else if (tag=="ts_cradled") {
      // interval (minutes) for cradled timed sync
      profile.TimedSyncCradledPeriod=uint16Val(value.c_str());
    }
    #endif
    else if (ti>=0) {
      // target-level settings
      if (tag=="enabled") {
        // enabled
        target.enabled = value=="1";
      }
      else if (tag=="slow") {
        // force slowsync
        target.forceSlowSync = value=="1";
      }
      else if (tag=="mode" && value.size()>=1) {
        // mode: 0=twoway, 1=fromserver, 2=fromclient
        uInt8 m=value[0]-0x30;
        if (m<=2) target.syncmode = (TSyncModes)m;
      }
      else if (tag=="path") {
        // remote DB path
        AssignCString(target.remoteDBpath,value.c_str(),remoteDBpathMaxLen);
      }
      #if TARGETS_DB_VERSION>5
      else if (tag=="remoteFilters") {
        // remote DB path
        AssignCString(target.remoteFilters,value.c_str(),filterExprMaxLen);
      }
      else if (tag=="localFilters") {
        // remote DB path
        AssignCString(target.remoteFilters,value.c_str(),filterExprMaxLen);
      }
      #endif
      else if (tag=="ext") {
        // extras
        StrToULong(value.c_str(), target.extras);
      }
      else if (tag=="lim1") {
        // limit1
        StrToLong(value.c_str(), target.limit1);
      }
      else if (tag=="lim2") {
        // limit2
        StrToLong(value.c_str(), target.limit2);
      }
    } // if target selected
  } // while more tags
  // save open target if any
  if (ti>=0) aCfgP->writeTarget(ti,target);
  // save profile
  aCfgP->writeProfile(pi,profile);
  aActiveProfile=pi; // return changed or added profile number
  return true;
} // configProfile


// Configure registration
// cmd="reg";text="name ::m=manufacturer";key="ABCD-EFGH-KIJL-1234";
static bool configRegistration(const char *aRegParams, TSyncAppBase *aAppBaseP)
{
  #ifdef SYSER_REGISTRATION
  string tag,text,key;
  // get text
  aRegParams = nextTag(aRegParams,tag,text);
  if (!(tag=="text")) return false;
  // get key
  if (!aRegParams) return false;
  aRegParams = nextTag(aRegParams,tag,key);
  if (!(tag=="key")) return false;
  // apply registration
  return aAppBaseP->checkAndSaveRegInfo(text.c_str(), key.c_str())==LOCERR_OK;
  #else
  return false; // no registration possible
  #endif
} // configRegistration


// generic code that can work as palmdbagent or binfiledbagent method
// - Provisioning string format: tag="value"[;tag="value"]....
// - commands:
//   - add : only adds a profile, does nothing if already existing or
//     with single-profile-client.
//   - update : adds new or updates existing profile, if defpf is used
//     or with single-profile client, the first profile will be
//     updated (overwritten)
//   - modify : only modifies profile with specified name, never overwrites
//     another profile.
//   - delete : deletes profile with specified name (except if it is only
//     profile left)
//   - reg : apply license information
bool CLIENTAGENTCONFIG::executeProvisioningString(const char *aProvisioningCmd, sInt32 &aActiveProfile)
{
  string tag,value;
  const char *p=aProvisioningCmd;
  // get command tag
  p=nextTag(p,tag,value);
  if (!p) return false;
  // first tag must be "cmd"
  if (!(tag=="cmd")) return false;
  // dispatch commands
  if (value=="reg") {
    // registration
    return configRegistration(p,getSyncAppBase());
  }
  else if (value=="add") {
    // add new profile if not already there
    return configProfile(this,p,true,false,aActiveProfile); // may add but may not update
  }
  else if (value=="update") {
    // update or add profile
    return configProfile(this,p,true,true,aActiveProfile); // may add and update
  }
  else if (value=="modify") {
    // only modify existing
    return configProfile(this,p,false,true,aActiveProfile); // may not add but may update
  }
  else if (value=="delete") {
    // delete existing
    return configProfile(this,p,false,false,aActiveProfile); // neither add nor update = delete
  }
  else return false; // unknown command
} // CLIENTAGENTCONFIG::executeProvisioningString





/* eof */
