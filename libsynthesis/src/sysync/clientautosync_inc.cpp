/*
 *  File:         clientautosync_inc.cpp
 *
 *  Include file for binfileagent.cpp and palmdbagent.cpp to provide
 *  autosync mechanisms.
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 *  2004-05-10 : luz : created from clientprovisioning_inc.cpp
 *
 *
 */

#ifdef AUTOSYNC_SUPPORT

#ifdef SYDEBUG

static const char * const IPPStateNames[num_ipp_states] = {
  "unknown",
  "idle",
  "activated",
  "subscribed"
};


static const char * const AutoSyncModeNames[num_autosync_modes] = {
  "push",
  "timed",
  "none",
  "serveralert"
};

static const char * const AutoSyncStateNames[num_autosync_states] = {
  "idle",
  "start", // start selected autosync mechanism
  "timedsync", // pure scheduled sync
  "timedsync_wait", // waiting for scheduled sync to occur
  "ipp_connectwait", // start waiting until connecting again
  "ipp_connectwaiting", // waiting for connecting again
  "ipp_resubscribe", // re-subscribe to DMU method 1
  "alert_implicit", // alert implicit datastores
  "ipp_connect", // connect to server
  "ipp_sendrequest", // send request to server
  "ipp_answerwait", // connected, waiting for answer from IPP server
  "ipp_disconnect", // disconnect from the server
  "ipp_endanswer",  // disconnect after a IPP answer
  "ipp_processanswer" // process collected IPP answer
};


#endif


// utility functions


// applies the value to the IPP parameter identified by aTag
// This can be called from client provisioning code or from a sync session's GET
// command RESULT (if the server supports IPP autoconfig)
void CLIENTAGENTCONFIG::ipp_setparam(const char *aTag, const char *aValue, TIPPSettings &aIPPSettings)
{
  #ifdef IPP_SUPPORT
  uInt16 res=0;

  if (strucmp(aTag,"srv")==0) {
    AssignCString(aIPPSettings.srv,aValue,maxipppathsiz);
    fIPPState=ippstate_unknown;
  }
  else if (strucmp(aTag,"path")==0) {
    AssignCString(aIPPSettings.path,aValue,maxipppathsiz);
  }
  else if (strucmp(aTag,"id")==0) {
    AssignCString(aIPPSettings.id,aValue,maxippidsiz);
    fIPPState=ippstate_unknown;
  }
  else if (strucmp(aTag,"port")==0) {
    StrToUShort(aValue,aIPPSettings.port);
  }
  else if (strucmp(aTag,"period")==0) {
    StrToUShort(aValue,aIPPSettings.period);
  }
  else if (strucmp(aTag,"method")==0) {
    StrToUShort(aValue,res);
    aIPPSettings.method=(uInt8)res;
    fIPPState=ippstate_unknown;
  }
  else if (strucmp(aTag,"cred")==0) {
    AssignCString(aIPPSettings.cred,aValue,maxipppathsiz);
    fIPPState=ippstate_unknown;
  }
  else if (strucmp(aTag,"maxinterval")==0) {
    StrToUShort(aValue,res);
    aIPPSettings.maxinterval=res;
  }
  else if (strucmp(aTag,"timedds")==0) {
    AssignCString(aIPPSettings.timedds,aValue,maxipppathsiz);
  }
  #ifdef GAL_CLIENT_SUPPORT
  else if (strucmp(aTag,"gal_url")==0) {
    AssignCString(gGALInfo.url,aValue,GAL_STRING_MAX);
    gGALInfo.galUpdated=true;
  }
  else if (strucmp(aTag,"gal_domain")==0) {
    AssignCString(gGALInfo.domain,aValue,GAL_STRING_MAX);
    gGALInfo.galUpdated=true;
  }
  #endif
  #endif
} // CLIENTAGENTCONFIG::ipp_setparam



// generic code that can work as palmdbagent or binfiledbagent method

// must be called when application starts to initialize autosync
// BEFORE autosync_setprofile()
void CLIENTAGENTCONFIG::autosync_init(lineartime_t aLastAlert)
{
  // make sure all autosync is disabled
  fAutosyncProfile.AutoSyncLevel[0].Mode=autosync_none;
  fAutosyncProfile.AutoSyncLevel[1].Mode=autosync_none;
  fAutosyncProfile.AutoSyncLevel[2].Mode=autosync_none;
  // init state
  fAutosyncState=autosync_idle;
  fAutosyncProfileLastidx=-1; // no profile selected yet
  fAutosyncLastAlert=aLastAlert; // as saved by the main app
  fNewMode=autosync_none; // no new mode
  // init per-profile state
  fCurrentLevelIndex=-1; // none yet
  fAutoSyncMode=autosync_none; // disabled until we have checked
  fNextSyncAt=0; // we do not know when next sync will be
  fWhyNotFlags=0; // no reason why not
  // cancel all current alerts
  autosync_ackAlert(false);
  #ifdef TCP_LEAK_DEBUG
  fTcpLeakCounter=0;
  #endif
} // CLIENTAGENTCONFIG::autosync_init


// to be called to forget or actually execute prepared autosync alerts in DS config
// (e.g. after asking user for confirmation after receiving a SAN)
void CLIENTAGENTCONFIG::autosync_ackAlert(bool aAck)
{
  if (!aAck) {
    // cancel alert
    fAutosyncAlerted=false;
    fUnsuccessfulAlerts=0;
    // make sure all prepared alerts in the DS config are cleared
    TLocalDSList::iterator pos;
    for (pos=fDatastores.begin(); pos!=fDatastores.end(); pos++) {
      CLIENTDSCONFIG *dscfgP = static_cast<CLIENTDSCONFIG *>(*pos);
      dscfgP->fAutosyncAlerted=false;
      dscfgP->fAutosyncForced=false;
      dscfgP->fAutosyncAlertCode=0;
      dscfgP->fAutosyncPathCGI.erase();
    }
  }
  else {
    // activate alerts prepared in DS config
    fAutosyncAlerted=true;
  }
} // autosync_ackAlert


// must be called when active profile changes (or at start of application)
void CLIENTAGENTCONFIG::autosync_setprofile(sInt32 aProfileIndex, bool aHasChanged)
{
  if (aProfileIndex != fAutosyncProfileLastidx || aHasChanged) {
    // a new profile gets active
    fAutosyncProfileLastidx=aProfileIndex;
    // - first stop autosync
    autosync_stop();
    // - cancel eventually pending alerts
    autosync_ackAlert(false);
    // - read profile that determines autosync behaviour
    if (getProfile(aProfileIndex,fAutosyncProfile)<0) {
      // make sure all autosync is disabled
      fAutosyncProfile.AutoSyncLevel[0].Mode=autosync_none;
      fAutosyncProfile.AutoSyncLevel[1].Mode=autosync_none;
      fAutosyncProfile.AutoSyncLevel[2].Mode=autosync_none;
    }
    // - re-init per-profile state
    fAutoSyncMode=autosync_none; // disabled until we have checked
    fAutosyncAlerted=false;
    fNextSyncAt=0; // we do not know when next sync will be
    fWhyNotFlags=0; // no reason why not
    #ifdef IPP_SUPPORT
    fIPPState=ippstate_unknown; // we do not know yet
    #endif
    // - make sure we do a full settings check
    fNextCheckAt=0; // do immediately check settings at next step
    fNextWakeupCheckAt=0;
  }
} // CLIENTAGENTCONFIG::autosync_setprofile


// must be called to obtain DMU PUT command data to send to SyncML server
void CLIENTAGENTCONFIG::autosync_get_putrequest(string &aReq)
{
  // use custom Put for requesting activation in s2g methods 0 and 1 (but not for method 2 and above)
  aReq.erase(); // no DMU request when DMU is not active or status unknown
  #ifdef IPP_SUPPORT
  if (fAutoSyncMode==autosync_ipp && fIPPState!=ippstate_unknown && fAutosyncProfile.ippSettings.method<IPP_METHOD_OMP) {
    // yes, send out status
    if (fIPPState==ippstate_idle) {
      aReq=IPP_REQ_ACTIVATE; // we have no DMUCI (any more), we need to be (re)activated
    }
    else {
      if (fIPPState==ippstate_activated && fAutosyncProfile.ippSettings.method==IPP_METHOD_DMUS2G) {
        aReq=IPP_REQ_SUBSCRIBE; // we have no valid creds or no DMU server, we need to (re)subscribe
      }
      else {
        aReq=IPP_REQ_CHECK; // from our side, everything's ok, but server might want to send new IPP settings anyway
      }
    }
    // always append DMUCI
    StringObjAppendPrintf(
      aReq,
      "=%s\r\n",
      fAutosyncProfile.ippSettings.id
    );
    // append extra params for method = IPP_METHOD_DMUS2G
    if (fAutosyncProfile.ippSettings.method==IPP_METHOD_DMUS2G) {
      StringObjAppendPrintf(
        aReq,
        "error=%hd\r\n",
        fAutosyncRetries
      );
    }
  }
  #endif
} // CLIENTAGENTCONFIG::autosync_get_putrequest



// should be called when environmental params have changed
// (battery, cradled/uncradled etc.)
void CLIENTAGENTCONFIG::autosync_condchanged(void)
{
  fNextCheckAt=0; // do immediately check settings at next step
} // CLIENTAGENTCONFIG::autosync_modecheck



// returns true if application may quit or otherwise stop calling
// autosync_check() in short intervals. Time when autosync_check() should
// be called next time latest will be returned in aRestartAt.
//
bool CLIENTAGENTCONFIG::autosync_canquit(lineartime_t &aRestartAt, bool aEnabled)
{
  aRestartAt=0; // assume no timed restart needed
  bool canquit=false;
  // check if we can quit
  if (!aEnabled || fAutoSyncMode==autosync_none || fAutoSyncMode==autosync_serveralerted)
    canquit=true; // we can quit if no autosync is running or if alert event can start the app (as in case of SAN)
  else if (fAutoSyncMode!=autosync_ipp)
    canquit=true; // if we have no IPP running, we can quit too (but need a restart later)
  // check if we need to restart at some time
  if (!aEnabled) {
    aRestartAt=0; // if not enabled, we do not need to restart at all
  }
  else if (fNextWakeupCheckAt<maxLinearTime || fNextSyncAt!=0) {
    // we have scheduled activity, we need to come back
    aRestartAt=
      (fNextSyncAt==0 || fNextWakeupCheckAt<fNextSyncAt) ?
      fNextWakeupCheckAt :
      fNextSyncAt;
  }
  #if SYDEBUG>3
  // Note: do no show this in normal debug clients any more as it now happens for EVERY autosync check
  if (PDEBUGTEST(DBG_TRANSP)) {
    string ts;
    StringObjTimestamp(ts,aRestartAt);
    PDEBUGPRINTFX(DBG_TRANSP,(
      "AUTOSYNC: app %s quit now, and must be running (again) by %s",
      canquit ? "MAY" : "MUST NOT",
      ts.c_str()
    ));
  }
  #endif
  return canquit;
} // CLIENTAGENTCONFIG::autosync_canquit



// returns true if we need to perform a sync session now
bool CLIENTAGENTCONFIG::autosync_check(bool aActive)
{
  // process one state machine step in active mode
  autosync_step(aActive);
  // check if we have a pending re-alert due to failed session execution,
  // but only if autosync mode is still active (could be temporarily off because of phone flight mode)
  if (!fAutosyncAlerted && aActive && fAutoSyncMode!=autosync_none) {
    // no real alert pending, check if we need to generate one for retry reasons
    if (fUnsuccessfulAlerts>0 && fUnsuccessfulAlerts<AUTOSYNC_MAX_ALERTRETRIES) {
      // we had unsuccessful alerts before
      if (getSystemNowAs(TCTX_SYSTEM)>fAutosyncLastAlert+(AUTOSYNC_MIN_ALERTRETRY << (fUnsuccessfulAlerts-1))*secondToLinearTimeFactor) {
        // re-alert
        PDEBUGPRINTFX(DBG_TRANSP,("AUTOSYNC: re-alerting after %hd unsuccessful attempts to run AutoSync Session",fUnsuccessfulAlerts));
        fAutosyncLastAlert=getSystemNowAs(TCTX_SYSTEM);
        fAutosyncAlerted=true;
        if (fUnsuccessfulAlerts>=AUTOSYNC_FORCEDOWN_RETRIES) {
          #ifndef ENGINE_LIBRARY
          // explicitly bring down connection (if we are in a XPT client)
          getXPTClientBase()->releaseConnection(true);
          #endif
          // bring down autosync
          autosync_stop();
        }
      }
    }
  }
  return fAutosyncAlerted && aActive;
} // CLIENTAGENTCONFIG::autosync_check


// called to announce that a autosync session was performed
void CLIENTAGENTCONFIG::autosync_confirm(bool aUnsuccessful)
{
  if (aUnsuccessful) {
    // re-alert again later
    fUnsuccessfulAlerts++;
    // but clear main alert flag
    fAutosyncAlerted=false;
  }
  else {
    // clear alert
    autosync_ackAlert(false);
  }
  PDEBUGPRINTFX(DBG_HOT,("AUTOSYNC: confirmed %ssuccessful execution of autosync session",aUnsuccessful ? "un" : ""));
} // CLIENTAGENTCONFIG::autosync_confirm



// returns true if we need to perform a sync session now
void CLIENTAGENTCONFIG::autosync_stop(void)
{
  // perform steps until autosync is idle
  while(fAutosyncState!=autosync_idle) {
    // process one state machine step in inactive mode
    autosync_step(false);
  }
} // CLIENTAGENTCONFIG::autosync_stop


// parse parameter from aText
static const char *getParam(const char *aText, string &aParam, bool aNeedsSemicolon)
{
  aParam.erase();
  // skip leading spaces
  while (*aText && (*aText==' ')) aText++;
  // must start with semicolon
  if (!aNeedsSemicolon || (*aText && (*aText==';'))) {
    if (aNeedsSemicolon) aText++; // skip it
    // skip more spaces if any
    while (*aText && (*aText==' ')) aText++;
    char quotechar=0;
    if (*aText==0x22) {
      quotechar=*aText++; // remember quote char
    }
    // read parameter
    while (*aText && (quotechar ? *aText!=quotechar : !isspace(*aText) && (*aText!=';'))) {
      // read char
      if (*aText=='\\') {
        aText++;
        if (*aText==0) break;
      }
      // add to result
      aParam += *aText++;
    }
    // skip quote char if this was one
    if (quotechar && *aText==quotechar) aText++;
  }
  return aText;
} // getParam


// get status information about autosync
void CLIENTAGENTCONFIG::autosync_status(
  TAutosyncModes &aAutosyncMode, // currently active mode
  lineartime_t &aNextSync, // 0 if no scheduled nextsync
  lineartime_t &aLastAlert, // 0 if no alert seen so far
  uInt8 &aWhyNotFlags // autosync status flags
)
{
  // return current mode
  aAutosyncMode =
    fAutosyncState==autosync_idle ?
    autosync_none : // show "disabled" state while idle
    fAutoSyncMode;
  // special check for IPP (but only if we are really in autosync at this time)
  #ifdef IPP_SUPPORT
  if (aAutosyncMode==autosync_ipp) {
    // check if we have the right activation level
    if (fIPPState!=ippstate_subscribed) {
      // reason for not being active is that we are not (yet) correctly activated and/or subscribed
      aWhyNotFlags=autosync_status_notsubscribed;
      aAutosyncMode=autosync_none; // show "disabled" while not activated or subscribed
    }
  }
  else
  #endif
  {
    // return reason for not being active right now
    // (but omit wrongday/wrongtime as these are user settings and should not show up as errors
    aWhyNotFlags=
      (fAutosyncState==autosync_idle || fAutosyncState==autosync_timedsync_wait)
      ? fWhyNotFlags & ~(autosync_status_wrongday+autosync_status_wrongtime)
      : 0;
  }
  // return time of next known sync
  aNextSync = fNextSyncAt;
  // timed sync disabled by zero interval?
  if (aAutosyncMode==autosync_timed) {
    if (aNextSync==0) {
      // reason for not being active is that for current mode (cradled/mobile), the interval is set to 0
      aWhyNotFlags=autosync_status_disabled;
      aAutosyncMode=autosync_none; // show "disabled" while timed autosync is not active
    }
    else if (aWhyNotFlags) {
      // other reason for timed sync to be not active
      aAutosyncMode=autosync_none; // show "disabled" while timed autosync is not active
    }
  }
  // also report last alert
  aLastAlert=fAutosyncLastAlert;
} // CLIENTAGENTCONFIG::autosync_status



/// @brief helper: check if environment factors (other than time), such as Mem or Bat, allow this level
/// @return true if active, false if not (and updated aWhyNotFlags)
static bool checkLevelEnvFactors(const TAutoSyncLevel &aLevel, uInt8 &aWhyNotFlags)
{
  bool levelActive;
  #ifndef ENGINE_LIBRARY
  uInt8 battlevel;
  bool acpowered;
  #ifdef ENGINEINTERFACE_SUPPORT
  #error "fMasterPointer in AppBase links to owning TEngineInterface!!"
  #endif
  if (getPowerStatus(battlevel,acpowered,getXPTClientBase()->fMasterPointer)) {
    // valid power info, check if this allows enabling level
    levelActive = acpowered || battlevel>=aLevel.ChargeLevel;
  }
  else {
    // no valid power info, enabled only if there's no min charge level defined
    levelActive = aLevel.ChargeLevel==0;
  }
  #else
  #ifdef RELEASE_VERSION
    #error "reorganize batt and memory level checks in a XPT independent way"
  #endif
  levelActive=true;
  #endif
  // set flag if battery is reason for inactive level
  if (!levelActive)
    aWhyNotFlags |= autosync_status_lowbat;
  // - check mem as well
  if (levelActive) {
    #ifndef ENGINE_LIBRARY
    // - battery power
    uInt8 memlevel;
    #ifdef ENGINEINTERFACE_SUPPORT
    #error "fMasterPointer in AppBase links to owning TEngineInterface!!"
    #endif
    if (getMemFreeStatus(memlevel,getXPTClientBase()->fMasterPointer)) {
      // valid memory info, check if this allows enabling level
      levelActive = memlevel>=aLevel.MemLevel;
    }
    else {
      // no valid memory info, enabled only if there's no min free mem level defined
      levelActive = aLevel.MemLevel==0;
    }
    #else
    levelActive=true;
    #endif
    // set flag if memory is reason for inactive level
    if (!levelActive)
      aWhyNotFlags |= autosync_status_lowmem;
  }
  return levelActive;
} // checkLevelEnvFactors



/// @brief helper: check if this level is active.
/// @return
/// - true if yes: returns true and sets aMode.
/// - false if no: returns false
/// - aNextCheckNeeded is always updated, if 0, we do not need to check because this level cannot get active any time
///   OR no timed activity is needed to activate it (as in case of SAN - as long as app starts when SMS arrives,
///   that's sufficient.
static bool checkAutoSyncLevel(const TAutoSyncLevel &aLevel, lineartime_t aNow, lineartime_t &aNextCheckNeeded, uInt8 &aWhyNotFlags)
{
  bool levelActive=false;
  // check if we are in one of the selected weekdays (localtime)
  uInt8 weekday=lineartime2weekday(aNow);
  if ((aLevel.WeekdayMask & (1<<weekday))!=0) {
    // aNow is in one of the selected weekdays, now check time window
    // - get minutes of the day
    uInt16 minuteofday = lineartime2timeonly(aNow) / secondToLinearTimeFactor / 60;
    // - check if we are within
    bool afterstart = minuteofday>=aLevel.StartDayTime;
    bool beforend = minuteofday<aLevel.EndDayTime;
    if (aLevel.StartDayTime<aLevel.EndDayTime) {
      // normal case: start before end: just check if now is between
      levelActive = afterstart && beforend;
      if (levelActive) {
        // - we are in window, calculate time when we'll be out again
        aNextCheckNeeded = aNow+ (aLevel.EndDayTime-minuteofday) * 60 * secondToLinearTimeFactor;
      }
      else {
        // - we are out of window, calculate time when we'll be in again
        aNextCheckNeeded = aNow+ (aLevel.StartDayTime+(afterstart ? 24*60 : 0)-minuteofday) * 60 * secondToLinearTimeFactor;
      }
    }
    else {
      // Special case: end same or before start, active time before end OR after start
      levelActive= beforend || afterstart;
      if (levelActive) {
        // - we are in window, calculate time when we'll be out again
        aNextCheckNeeded = aNow+ (aLevel.EndDayTime+(afterstart ? 24*60 : 0)-minuteofday) * 60 *secondToLinearTimeFactor;
      }
      else {
        // - we are out of window, calculate time when we'll be in again
        aNextCheckNeeded = aNow+ (aLevel.StartDayTime-minuteofday) * 60 * secondToLinearTimeFactor;
      }
    }
    // we know if we are in the time window and we know when we have to check next time
    if (levelActive) {
      // check for always enabled
      if (
        aLevel.WeekdayMask & 0x7F == 0x7F &&
        aLevel.StartDayTime==0 &&
        aLevel.EndDayTime==0
      ) {
        // unconditionally enabled level, we do not need to check for this again
        aNextCheckNeeded=maxLinearTime;
      }
      /*
      #ifdef IPP_SUPPORT
      // now check secondary factors already (only for IPP, timed and SAN can check
      // when actually SAN arrives or next sync time is reached)
      if (aLevel.Mode==autosync_ipp) {
        // - check them
        levelActive = checkLevelEnvFactors(aLevel, aWhyNotFlags);
      }
      #endif
      */
    }
    else {
      aWhyNotFlags|=autosync_status_wrongtime;
    }
  } // if weekday
  else if (aLevel.WeekdayMask==0) {
    aWhyNotFlags|=autosync_status_wrongday;
    // unconditionally disabled level, we do not need to check for this again
    aNextCheckNeeded=maxLinearTime;
  }
  else {
    aWhyNotFlags|=autosync_status_wrongday;
    // calculate when we'll reach next enabled weekday
    uInt8 w;
    for (w=1; w<7; w++) {
      if ((aLevel.WeekdayMask & (1<<((weekday+w) % 7)))!=0) break;
    }
    // we need to check w days from now
    aNextCheckNeeded =
      ((aNow / linearDateToTimeFactor)+w) * linearDateToTimeFactor;
  }
  // timed checks for entering or leaving this level are only needed if this is timed sync or IPP;
  // for SAN, we don't need to start the app to enter or leave the level - we'll check when an SMS arrives
  if (aLevel.Mode==autosync_serveralerted)
    aNextCheckNeeded=maxLinearTime;
  // return if level is active
  return levelActive;
} // checkAutoSyncLevel


#ifdef SERVERALERT_SUPPORT

/// @brief process SAN message and eventually trigger sync accordingly
/// @note should only be called if Autosync is not globally disabled.
/// @param[out] aAlertedProfile index of profile addressed by the alert (might be different from currently selected profile)
/// @param[out] aUIMode should be checked by caller to eventually display an alert before syncing
/// @return LOCERR_OK if SAN requests starting a sync (but does not actually trigger it yet, as we might want UI before)
///         DB_NotAllowed (405) if everything would be fine except that autosync is disabled. GUI may alert this and allow sync nevertheless
///         LOCERR_PROCESSMSG if something is wrong with the SAN
///         DB_Forbidden (403) if credentials not ok
///         DB_NotFound (404) if no matching profile or datastore found
localstatus CLIENTAGENTCONFIG::autosync_processSAN(bool aLocalPush, void *aPushMsg, uInt32 aPushMsgSz, cAppCharP aHeaders, sInt32 &aAlertedProfile, UI_Mode &aUIMode)
{
  TSyError err;
  int syncType;
  uInt32 contentType;
  string datastoreURI;
  bool wantsSync=false; // assume no sync wanted

  // normal SAN processing
  SanPackage sanObj;

  // pass SAN to obj
  err=sanObj.PassSan(aPushMsg,aPushMsgSz);
  if (err!=LOCERR_OK) {
    PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: PassSan: invalid SAN, err=%d",err));
    return LOCERR_PROCESSMSG; // bad SAN
  }

  // try to get the first sync to have the header parsed by sanObj.
  int nth=1; // start with first
  err=sanObj.GetNthSync(
    nth, // note: 0 here will only check header and number of syncs
    syncType,
    contentType,
    datastoreURI
  );
  if (err!=LOCERR_OK) {
    PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: GetNthSync: invalid SAN, err=%d",err));
    return LOCERR_PROCESSMSG; // bad SAN
  }
  // check if we have any autosync profile to check against
  if (fAutosyncProfileLastidx<0) {
    PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: no autosync profile active"));
    return DB_NotFound; // no target for SAN found
  }
  /// @todo: allow for switching to matching profile on the fly - which would allow
  ///   multi-server-push. For now, SAN must match current profile
  PROFILERECORD *alertedProfile = &fAutosyncProfile;
  // now server identifier (URL of server) should be ready, so we can check what
  // profile is affected
  #ifdef HARD_CODED_SERVER_URI
  const char *serverURI = DEFAULT_SERVER_URI;
  #else
  const char *serverURI = alertedProfile->serverURI;
  #endif
  const char *uripos = strstr(serverURI,sanObj.fServerID.c_str());
  if (!uripos) {
    if (aLocalPush && strstr(serverURI,"/*")) {
      // catchall for local push
      PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: ServerID in SAN (%s) does not match, but profile has catchall-URI for local push",sanObj.fServerID.c_str()));
    }
    else {
      PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: ServerID in SAN (%s) is not substring of current profile's Server URL (%s)",sanObj.fServerID.c_str(),serverURI));
      return DB_NotFound; // no matching profile found
    }
  }
  // server identifier is ok, check if there are any syncs at all
  if (sanObj.fNSync<1) {
    PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: SAN does not contain any Sync requests"));
    return LOCERR_PROCESSMSG; // bad SAN
  }
  // check Digest for SANs Version 1.2 and higher
  if (sanObj.fProtocolVersion>=12) {
    // get real size of SAN
    size_t sanSize=aPushMsgSz; // not longer than this
    err = sanObj.GetSanSize(aPushMsg,sanSize);
    if (err!=LOCERR_OK) {
      PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: invalid SAN size (raw buffer = %ld), err=%d",aPushMsgSz,err));
      return LOCERR_PROCESSMSG; // bad SAN
    }
    // now we have user/pw - check the digest
    string pw;
    getUnmangled(pw,alertedProfile->serverPassword,maxupwsiz);
    err=sanObj.CreateDigest(
      alertedProfile->serverUser,
      pw.c_str(),
      alertedProfile->lastNonce,
      aPushMsg,
      sanSize
    );
    if (err==LOCERR_OK) {
      // have it checked
      if (!sanObj.DigestOK(aPushMsg)) {
        /// @todo %%% as long as it is unclear where nonce should come from, allow a "Synthesis" constant noce
        err=sanObj.CreateDigest(
          alertedProfile->serverUser,
          pw.c_str(),
          "Synthesis",
          aPushMsg,
          sanSize
        );
        if (err==LOCERR_OK)
          err = sanObj.DigestOK(aPushMsg) ? LOCERR_OK : DB_Forbidden;
      }
      else
        err=LOCERR_OK;
    }
    // - @todo %%% missing for the moment, needs some work on san.cpp first
    if (err!=LOCERR_OK) {
      PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: invalid SAN digest, err=%hd",err));
      return err; // digest not ok
    }
  } // SAN with Digest
  // store some info from SAN
  aAlertedProfile = fAutosyncProfileLastidx; // %%% for now, we only check the current profile
  aUIMode=sanObj.fUI_Mode; // pass UI mode out to allow GUI to react
  // - adjust UI mode if not specified
  if (aUIMode==UI_not_specified) {
    // not specified: in background for local sync, visible for remote sync (which will not run unless AutoSync is generally on!)
    aUIMode = aLocalPush ? UI_background : UI_informative;
  }
  fAutosyncSANSessionID = sanObj.fSessionID;
  // process the sync requests, first one is already here
  do {
    // alert by name, force sync (= sync wether target is enabled or not)
    // if we have UI interaction or localPush, we also alert those datastores that have never synced so far
    PDEBUGPRINTFX(DBG_HOT,("AUTOSYNC: SAN requests alert %d for '%s', content type code = 0x%lX",syncType,datastoreURI.c_str(),contentType));
    if (alertDbByName(datastoreURI.c_str(), "",true,syncType,aUIMode==UI_user_interaction || aLocalPush)) {
      wantsSync=true;
    }
    // check if there are more
    nth++;
    err=sanObj.GetNthSync(
      nth,
      syncType,
      contentType,
      datastoreURI
    );
  } while(err==LOCERR_OK);
  if (err!=404) {
    PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: error processing %d-th sync request",nth,err));
    // but do not exit - it might still be that we have started alerted a sync already
  }
  if (wantsSync) {
    // update server version according to SAN SyncML version
    TSyncMLVersions alertVers;
    switch (sanObj.fProtocolVersion) {
      case 12 : alertVers = syncml_vers_1_2; break;
      case 11 : alertVers = syncml_vers_1_1; break;
      case 10 : alertVers = syncml_vers_1_0; break;
      default : alertVers = syncml_vers_unknown; break;
    }
    PDEBUGPRINTFX(DBG_TRANSP,("AUTOSYNC: SAN requests SyncML version %d.%d",sanObj.fProtocolVersion/10,sanObj.fProtocolVersion%10));
    if (alertedProfile->lastSyncMLVersion!=alertVers) {
      // adjust it
      alertedProfile->lastSyncMLVersion=alertVers;
      writeProfile(aAlertedProfile,*alertedProfile);
    }
    // check if the profile is enabled for autosync at all
    /// @todo implement checking through all matching profiles - for now, only current is active
    if (
      (fAutoSyncMode!=autosync_serveralerted && fAutoSyncMode!=autosync_ipp) || // no push mode selected for now
      fAutosyncState==autosync_idle // or active but disabled
    ) {
      // SAN is fine, however we are not in Autosync mode
      // Note: datastores are already alerted in DS config - GUI may choose to start a sync even if Autosync is off
      return DB_NotAllowed;
    }
    else {
      // we are in autosync mode, but check environment first
      fEnvCheckedAt = getSystemNowAs(TCTX_SYSTEM);
      if (fCurrentLevelIndex>=0 && !checkLevelEnvFactors(fAutosyncProfile.AutoSyncLevel[fCurrentLevelIndex], fWhyNotFlags)) {
        // environment factors disallow syncing now
        PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: SAN wants sync, but Bat/Mem of current level does not allow -> re-checking levels"));
        fNextCheckAt=0; // force re-checking for active levels
        return DB_NotAllowed;
      }
    }
  }
  // everything fine
  return wantsSync ? LOCERR_OK : DB_NotFound; // no matching target DS found
} // autosync_processSAN

#endif



// performs next step in the state machine
void CLIENTAGENTCONFIG::autosync_step(bool aActive)
{
  #ifdef IPP_SUPPORT
  TcpRc_t tcperr;
  localstatus err;
  uInt16 connwaitseconds;
  const char *q,*p;
  const int bufsiz=2000;
  unsigned char buffer[bufsiz];
  unsigned long received;
  #endif
  string s;
  TARGETRECORD target;
  sInt32 idx,seq;
  CLIENTDSCONFIG *dscfgP;
  uInt16 interval;
  string dbname;
  string filtercgi;
  bool done;
  bool newmode;
  lineartime_t now,nowminute;
  TAutoSyncStates newstate;
  bool isCradled;

  // get clientbase object
  #ifndef ENGINE_LIBRARY
  TXPTClientBase *clientBaseP = getXPTClientBase();
  #endif
  do {
    // assume no state change
    newstate=fAutosyncState;
    fCurrentLevelIndex = -1; // none chosen yet
    // get current time
    now=getSystemNowAs(TCTX_SYSTEM);
    nowminute = (now / 60 / secondToLinearTimeFactor) * 60 * secondToLinearTimeFactor; // precision by minute only
    newmode=false; // assume no mode change
    done=true; // assume done after processing current state
    // sanity check for last alert:
    if (fAutosyncLastAlert>now)
      fAutosyncLastAlert=0; // reset when "last" alert seems to be in the future - which is not possible
    // check for new mode
    if (fNextCheckAt==0 || fNextCheckAt<=now) {
      // we need to find relevant autosync params
      fNextCheckAt=maxLinearTime; // infinity
      fNextWakeupCheckAt=maxLinearTime; // infinity
      lineartime_t nextcheck;
      uInt8 lvl;
      fWhyNotFlags=0;
      bool found=false;
      for (lvl=0; lvl<NUM_AUTOSYNC_LEVELS; lvl++) {
        // check if this level is a candidate
        uInt8 newWhyNotFlags=0;
        bool newFound=false;
        bool isCandidate = checkAutoSyncLevel(fAutosyncProfile.AutoSyncLevel[lvl],nowminute,nextcheck,newWhyNotFlags);
        // check if level is active
        if (isCandidate) {
          // found candidate for active level
          // check if environment allows activating this level
          bool envOk = checkLevelEnvFactors(fAutosyncProfile.AutoSyncLevel[lvl], newWhyNotFlags);
          fEnvCheckedAt = now;
          // Note: for timed and SAN, environment is not relevant at this time,
          // because they don't need the app to be active while waiting for next sync time/event
          // Environment is important for these only when a sync is actually to be started
          if (fAutosyncProfile.AutoSyncLevel[lvl].Mode==autosync_ipp) {
            // only for IPP, we cannot make the level active if environment factors are not ok
            isCandidate = envOk;
          }
          // activate it if no other already activated
          if (!found && isCandidate) {
            newFound=true;
            fNewMode=fAutosyncProfile.AutoSyncLevel[lvl].Mode;
            // make sure we do not switch on DMU if it is not enabled
            if (fNewMode==autosync_ipp && !isFeatureEnabled(&fAutosyncProfile,APP_FTR_IPP)) {
              fNewMode=autosync_none; // force DMU/IPP off if not licensed
              fWhyNotFlags=autosync_status_notlicensed; // show why
            }
          }
        }
        // update overall whyNotFlags
        fWhyNotFlags |= newWhyNotFlags;
        // update when to check and/or wakeup next
        if (nextcheck<fNextCheckAt)
          fNextCheckAt=nextcheck;
        if (nextcheck<fNextWakeupCheckAt)
          fNextWakeupCheckAt=nextcheck;
        if (!isCandidate) {
          // this level cannot be made active. If this is an IPP level and it is not a candidate
          // NOT due to time settings, we must make sure the environment is checked again soon
          if (
            fAutosyncProfile.AutoSyncLevel[lvl].Mode==autosync_ipp &&
            ((newWhyNotFlags & (autosync_status_wrongday+autosync_status_wrongtime))==0)
          ) {
            // check if reason for not activating this level is memory
            // or battery - in that case we would need to check again soon
            lineartime_t t=maxLinearTime;
            // - is it memory?
            if (newWhyNotFlags & autosync_status_lowmem)
              t=now+AUTOSYNC_MEMCHECK_INTERVAL*secondToLinearTimeFactor;
            if (t<fNextCheckAt) fNextCheckAt=t;
            if (t<fNextWakeupCheckAt) fNextWakeupCheckAt=t;
            // - is it battery?
            if (newWhyNotFlags & autosync_status_lowbat)
              t=now+AUTOSYNC_BATTCHECK_INTERVAL*secondToLinearTimeFactor;
            if (t<fNextCheckAt) fNextCheckAt=t;
            if (t<fNextWakeupCheckAt) fNextWakeupCheckAt=t;
          }
        }
        // show summary of check
        if (PDEBUGTEST(DBG_TRANSP)) {
          string ts1,ts2;
          StringObjTimestamp(ts1,now);
          if (nextcheck==maxLinearTime)
            ts2="<never>";
          else
            StringObjTimestamp(ts2,nextcheck);
          PDEBUGPRINTFX(DBG_TRANSP,(
            "AUTOSYNC: Checked Level %d at %s - %s active, whynot=0x%1X, next check at %s",
            lvl,
            ts1.c_str(),
            newFound ? "is" : (isCandidate ? "is not (but could be)" : "cannot be"),
            (int)newWhyNotFlags,
            ts2.c_str()
          ));
        }
        // exit if we have found an active level
        if (newFound) {
          found=true;
          fCurrentLevelIndex = lvl;
          // check if activating this level is memory or battery dependent
          lineartime_t t=maxLinearTime;
          if (fAutosyncProfile.AutoSyncLevel[lvl].MemLevel>0)
            t=now+AUTOSYNC_MEMCHECK_INTERVAL*secondToLinearTimeFactor;
          if (t<fNextCheckAt) fNextCheckAt=t;
          if (fAutosyncProfile.AutoSyncLevel[lvl].ChargeLevel>0)
            t=now+AUTOSYNC_BATTCHECK_INTERVAL*secondToLinearTimeFactor;
          if (t<fNextCheckAt) fNextCheckAt=t;
          // if we are really active, there's no whynot to report
          if (fNewMode!=autosync_none && fNewMode!=autosync_timed)
            fWhyNotFlags=0;
          // show level
          if (PDEBUGTEST(DBG_TRANSP)) {
            string ts;
            StringObjTimestamp(ts,fNextCheckAt);
            PDEBUGPRINTFX(DBG_TRANSP,(
              "AUTOSYNC: Chosen Level %d, new mode=%s, whynot=0x%1X, next check (including regular batt/mem checks) at %s",
              lvl,
              AutoSyncModeNames[fNewMode],
              (int)fWhyNotFlags,
              ts.c_str()
            ));
          }
          //break; // %%% not any more - we now always go through all levels to make sure we have all params for switching
        }
      } // for all autosync levels
      // additionally check global conditions that could disable autosync
      if (found && fNewMode!=autosync_none) {
        // the device must be connectable
        #ifndef ENGINE_LIBRARY
        if (!clientBaseP->isConnectable(true)) {
          found=false;
          fWhyNotFlags=autosync_status_notconnectable;
          // check availability of connection soon again
          fNextCheckAt=now+AUTOSYNC_CONNCHECK_INTERVAL*secondToLinearTimeFactor;
        }
        else
        #endif
        if (fAutosyncState==autosync_ipp_answerwait) {
          // is connectable and we are waiting for an answer
          // now check for changes in cradled/mobile status
          #ifndef ENGINE_LIBRARY
          isCradled = clientBaseP->isDeviceCradled();
          #else
          isCradled = true;
          #endif
          if (fCurrentlyCradled!=isCradled) {
            fCurrentlyCradled=isCradled;
            #ifdef IPP_SUPPORT
            // seems this has changed, make sure we do a disconnect/reconnect
            // %%% is a bit ugly to influence the state machine here, but
            //     on the other hand it's here we check so we act here as well...
            PDEBUGPRINTFX(DBG_HOT,("Detected change to %s state -> reconnect IPP",fCurrentlyCradled ? "cradled" : "mobile"));
            newstate=autosync_ipp_disconnect;
            fAutosyncRetries=2; // retry almost immediately
            done=false;
            #endif
          }
        }
      }
      // switch autosync off if none of the levels is active
      if (!found) {
        fNewMode=autosync_none; // disable
      }
    }
    // if we have a mode switch pending
    if (fNewMode!=fAutoSyncMode) {
      // delay mode switch until we have reached autosync_idle in between
      if (fAutosyncState==autosync_idle) {
        // show mode change
        PDEBUGPRINTFX(DBG_HOT,(
          "AUTOSYNC: ** MODE CHANGED from %s to %s",
          AutoSyncModeNames[fAutoSyncMode],
          AutoSyncModeNames[fNewMode]
        ));
        // now we can switch modes
        fAutoSyncMode=fNewMode;
        // this is a real mode switch (not only temporary deactivation),
        // - last alert is cleared when we are now actively going idle. For switching
        //   between DMU and timed, we'll keep the last alert.
        if (fNewMode==autosync_none) {
          // - forget last alert, such that timed syncs will immediately start after re-start
          fAutosyncLastAlert=0; // never so far
        }
      }
      else {
        // stop current activity first before starting new
        aActive=false;
        // show pending mode change
        PDEBUGPRINTFX(DBG_TRANSP,(
          "AUTOSYNC: pending mode change from %s to %s - waiting for state to get idle",
          AutoSyncModeNames[fAutoSyncMode],
          AutoSyncModeNames[fNewMode]
        ));
      }
    }
    // now determine IPP state
    #ifdef IPP_SUPPORT
    if (isFeatureEnabled(&fAutosyncProfile,APP_FTR_IPP)) {
      if (fIPPState==ippstate_unknown) {
        // we have restarted, determine mode from what we have in the settings
        // (NOTE: this is independent of current autosync mode!)
        fIPPState=ippstate_idle; // Assume not activated at all
        fConversationStage=ippconvstage_login; // we need to log in first
        fDSCfgIterator = fDatastores.end();
        if (fAutosyncProfile.ippSettings.id[0]!=0) {
          // we have a DMUCI/deviceID
          // - check further params that must be present for either state
          if (fAutosyncProfile.ippSettings.method==IPP_METHOD_DMUS2G) {
            fIPPState=ippstate_activated; // with a DMUCI, we are at least activated
            // method IPP_METHOD_DMUS2G: if we have creds and server, we think we are subscribed
            if (
              fAutosyncProfile.ippSettings.srv[0]!=0 &&
              fAutosyncProfile.ippSettings.cred[0]!=0
            )
              fIPPState=ippstate_subscribed;
          }
          else {
            // method IPP_METHOD_DMU0 and OMP: when we have a server, we are subscribed
            // Note: for OMP, the actual SUBSCRIBE command will be issued after connecting later.
            //       The "subscribed" term here relates to DMU terminology.
            if (fAutosyncProfile.ippSettings.srv[0]!=0)
              fIPPState=ippstate_subscribed;
          }
        }
        if (fAutosyncProfile.ippSettings.method!=IPP_METHOD_DMU0) {
          PDEBUGPRINTFX(DBG_TRANSP,(
            "AUTOSYNC: re-evaluated IPP state from available settings = %s",
            IPPStateNames[fIPPState]
          ));
        }
      }
    }
    #endif
    // now let state machine run
    switch (fAutosyncState) {
      case autosync_idle:
        // if idle and we want autosync active, we must start it now
        if (aActive && fAutoSyncMode!=autosync_none) {
          newstate=autosync_start;
          done=false;
        }
        break;
      case autosync_start:
        if (!aActive) {
          newstate=autosync_idle;
          break;
        }
        // select which loop to start in the state machine
        if (fAutoSyncMode==autosync_timed) {
          // go to timed sync checks
          newstate=autosync_timedsync;
          done=false;
        }
        #ifdef IPP_SUPPORT
        else if (fAutoSyncMode==autosync_ipp) {
          // for push methods, we do NOT want a timed sync immediately,
          // but only after a certain time after the last alert.
          if (fAutosyncLastAlert==0)
            fAutosyncLastAlert=now; // assume we have just synced (to avoid a sync shortly after start)
          // try to connect
          newstate=autosync_ipp_connect;
          fAutosyncRetries=0;
          done=false;
        }
        #endif
        // otherwise, just stay in autosync_start (e.g. for SAN push)
        break;
      case autosync_timedsync:
        if (!aActive) {
          newstate=autosync_idle;
          break;
        }
        // check for timed sync
        #ifndef ENGINE_LIBRARY
        isCradled=clientBaseP->isDeviceCradled();
        #else
        isCradled=true;
        #endif
        interval =
          isCradled ?
          fAutosyncProfile.TimedSyncCradledPeriod :
          fAutosyncProfile.TimedSyncMobilePeriod;
        // calculate time of next sync, minute precision
        if (fAutosyncLastAlert==0) {
          if (interval!=0)
            fNextSyncAt=now; // if we don't know when we've last synced, sync NOW!
        }
        else {
          if (interval==0)
            break; // do not enter timed sync wait if interval is zero
          fNextSyncAt = fAutosyncLastAlert + (interval * 60 * secondToLinearTimeFactor);
        }
        if (PDEBUGTEST(DBG_TRANSP)) {
          string ts1,ts2;
          StringObjTimestamp(ts1,now);
          StringObjTimestamp(ts2,fNextSyncAt);
          PDEBUGPRINTFX(DBG_TRANSP,(
            "AUTOSYNC: Entered timed sync at %s, interval(%s)=%hd min -> next sync at %s",
            ts1.c_str(),
            isCradled ? "cradled" : "mobile",
            interval,
            ts2.c_str()
          ));
        }
        // update whyNot flags so we'll show the whynot cause as status
        if (fCurrentLevelIndex>=0) {
          checkLevelEnvFactors(fAutosyncProfile.AutoSyncLevel[fCurrentLevelIndex], fWhyNotFlags);
          fEnvCheckedAt = now;
        }
        // go waiting
        newstate=autosync_timedsync_wait;
        done=false;
        break;
      case autosync_timedsync_wait:
        if (!aActive) {
          newstate=autosync_idle;
          fNextSyncAt=0; // when we are not in timed sync, we don't know when next sync will be
          break;
        }
        // remove whynots after a while
        if (fWhyNotFlags && (now > fEnvCheckedAt+AUTOSYNC_ENVERRSDISPLAY_SECS*secondToLinearTimeFactor))
          fWhyNotFlags = 0; // forget them until we re-evaluate them
        // check for timed sync
        if (fNextSyncAt!=0 && now>=fNextSyncAt) {
          // we should start a timed sync - but check environment first (might have changed since we checked last when entering timed sync)
          if (fCurrentLevelIndex<0) {
            fNextCheckAt=0; // force immediate check for active levels
            break; // don't check before we know which level we are in
          }
          fEnvCheckedAt = now;
          if(!checkLevelEnvFactors(fAutosyncProfile.AutoSyncLevel[fCurrentLevelIndex], fWhyNotFlags)) {
            // environment factors disallow syncing now
            PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: timed sync scheduled for now, but Bat/Mem does not allow -> re-checking levels"));
            fNextCheckAt=0; // force immediate check for active levels
            fNextSyncAt=0; // cancel this sync
            fAutosyncLastAlert = now; // prevent immediate retries, treat as if a sync was alerted
            newstate=autosync_timedsync; // calc next timed sync
            break;
          }
          // we should start a autosync
          PDEBUGPRINTFX(DBG_TRANSP,("AUTOSYNC: starting timed sync session"));
          // we need to re-evaluate the time of next sync, and eventually
          // enter another mode (could be that we've got here to perform
          // a safeguard timed sync while in push mode)
          newstate=autosync_start;
          // if any datastores are enabled, this will now cause
          // a sync, so count this as alert
          fAutosyncLastAlert=now;
          // autosync-alert all enabled local datastores
          seq=0;
          do {
            idx=findTargetIndex(fAutosyncProfile.profileID,seq++);
            if (idx<0) break; // no more targets
            getTarget(idx,target);
            // get related datastore in config
            dscfgP = static_cast<CLIENTDSCONFIG *>(getLocalDS(target.dbname));
            // check if we would cause a zap on either side
            if (target.enabled) {
              if (!(
                target.forceSlowSync &&
                (target.syncmode==smo_fromclient || target.syncmode==smo_fromserver)
              )) {
                dscfgP->fAutosyncForced=false;
                dscfgP->fAutosyncAlerted=true;
                dscfgP->fAutosyncPathCGI.erase();
                // alerted at least one, autosync is in alerted state now
                fAutosyncAlerted=true;
                PDEBUGPRINTFX(DBG_HOT,("AUTOSYNC: *** ALERTED timed autosync for all enabled datastores"));
              }
              else {
                // prevent autosync because of possible zap
                PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: datastore '%s' would cause client or server to be totally overwritten -> do not do timed autosync",dscfgP->getName()));
              }
            }
          } while (true);
          #ifdef SYDEBUG
          if (!fAutosyncAlerted) {
            PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: No timed sync possible because no datastore available for sync"));
          }
          #endif
        }
        break;
      #ifdef IPP_SUPPORT
      case autosync_ipp_connectwait:
        if (!aActive) {
          newstate=autosync_idle;
          break;
        }
        // start wait phase
        fAutosyncLastStateChange=now;
        newstate=autosync_ipp_connectwaiting;
        // check if we need to resubscribe instead of connecting
        // (IPP_METHOD_DMUS2G only)
        if (
          fAutosyncProfile.ippSettings.method==IPP_METHOD_DMUS2G &&
          fAutosyncRetries>=IPP_RESUBSCRIBE_RETRIES
        ) {
          // check if we need to reactivate
          if (fAutosyncRetries>=IPP_REACTIVATE_RETRIES) {
            // force re-activation
            fIPPState=ippstate_idle;
            #ifdef SYDEBUG
            if (fAutosyncRetries==IPP_REACTIVATE_RETRIES)
              PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: too many unsuccessful retries (#%hd) -> cleared activation",fAutosyncRetries));
            #endif
            if (fAutosyncRetries>=IPP_BLOCK_RETRIES) {
              // prevent hectic syncs in case nothing works at all
              #ifdef SYDEBUG
              if (fAutosyncRetries==IPP_BLOCK_RETRIES)
                PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: too many unsuccessful retries (#%hd) -> IPP deactivated",fAutosyncRetries));
              #endif
              newstate=autosync_idle;
              break;
            }
          }
          if (fUnsuccessfulAlerts==0) {
            // resubscribe
            PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: too many unsuccessful retries (#%hd) -> requesting resubscription",fAutosyncRetries));
            newstate=autosync_ipp_resubscribe;
            break;
          }
          // in case of unsuccesful alerts pending, we'll just enter the waiting state
        }
        PDEBUGPRINTFX(DBG_TRANSP,("AUTOSYNC: waiting to connect before next retry (#%hd)",fAutosyncRetries));
        done=false;
        break;
      case autosync_ipp_connectwaiting:
        // check for deactivation
        if (!aActive) {
          newstate=autosync_idle;
          break;
        }
        // wait until we can reconnect
        connwaitseconds = IPP_MIN_RECONNECTWAIT<<(fAutosyncRetries-1);
        if (connwaitseconds>IPP_MAX_RECONNECTWAIT)
          connwaitseconds=IPP_MAX_RECONNECTWAIT;
        if (fAutosyncRetries==0 || now>fAutosyncLastStateChange+connwaitseconds*secondToLinearTimeFactor) {
          // waited long enough
          newstate=autosync_ipp_connect;
          PDEBUGPRINTFX(DBG_TRANSP,("AUTOSYNC: waited for %hd seconds in autosync_ipp_connectwaiting",connwaitseconds));
          done=false;
        }
        break;
      case autosync_ipp_resubscribe:
        // IPP_METHOD_DMUS2G only:
        // force a sync session for resubscription
        PDEBUGPRINTFX(DBG_HOT,("AUTOSYNC: Performing a sync session to (re-)subscribe now, datastores=%s",fAutosyncProfile.ippSettings.timedds));
        if (fIPPState>ippstate_activated) fIPPState=ippstate_activated; // forget subscription if we had one
        // - wipe server address to make sure we don't use old, expired one any more
        fAutosyncProfile.ippSettings.srv[0]=0;
        // - wipe credential
        fAutosyncProfile.ippSettings.cred[0]=0;
        // - do an implicit sync
        newstate=autosync_alert_implicit;
        done=false;
        break;
      case autosync_alert_implicit:
        // state is idle after an implicit alert
        newstate=autosync_idle;
        // only alert if no unsuccessful alerts are pending
        if (fUnsuccessfulAlerts==0) {
          // alert all datastores in fImplicitDatastores
          p = fAutosyncProfile.ippSettings.timedds;
          while (*p) {
            // find next comma or EOS
            for (q=p; *q!=',' && *q!=0; q++);
            dbname.assign(p,q-p);
            // alert for sync, no extra filter
            if (alertDbByName(dbname.c_str(),"",true)) {
              fAutosyncAlerted=true;
              fAutosyncLastAlert=now;
            }
            // next name
            if (*q) q++; // skip delimiter
            p=q; // start after next delimiter, if any
          }
        }
        // - now alert a timed sync session
        done=true;
        break;
      case autosync_ipp_connect:
        // check if we are connectable at this time
        if (!clientBaseP->isConnectable(true)) {
          // we cannot connect now, disable autosync for now
          newstate=autosync_idle;
          // effective immediately
          done=false;
          // force re-checking of autosync mode (which will keep autosync off until connectable)
          fNextCheckAt=0;
        }
        if (fAutosyncProfile.ippSettings.method==IPP_METHOD_DMU0) {
          // use the "maxinterval" sync interval in addition to push
          if (
            (fAutosyncLastAlert!=0) &&
            (fAutosyncProfile.ippSettings.maxinterval!=0) &&
            (now>fAutosyncLastAlert+fAutosyncProfile.ippSettings.maxinterval*60*secondToLinearTimeFactor) &&
            !(fAutosyncProfile.ippSettings.timedds[0]==0) &&
            (fUnsuccessfulAlerts==0)
          ) {
            // no push alert within maxinterval, perform an implicit timed sync with the datastores from timedds
            newstate=autosync_alert_implicit;
            PDEBUGPRINTFX(DBG_HOT,("AUTOSYNC: no alert within maxinterval=%hd mins, performing an implicit autosync session now",fAutosyncProfile.ippSettings.maxinterval));
            done=false;
            break;
          }
        }
        else if (fAutosyncProfile.ippSettings.method==IPP_METHOD_DMUS2G) {
          // enhanced protocol with router/server architecture
          // - check if we need to trigger a sync session to (re-)activate/subscribe and get creds
          //   - we start a sync session if we are not subscribed (and we haven't tried for at least IPP_MIN_RESUBSCRIBEWAIT)
          //   - we start a sync session if current credential has timed out
          if (
            (
              (fIPPState!=ippstate_subscribed && (fAutosyncRetries==0 || now>fAutosyncLastAlert+IPP_MIN_RESUBSCRIBEWAIT*secondToLinearTimeFactor)) ||
              (
                (fAutosyncLastAlert!=0) &&
                (fAutosyncProfile.ippSettings.maxinterval!=0) &&
                (now>fAutosyncLastAlert+fAutosyncProfile.ippSettings.maxinterval*60*secondToLinearTimeFactor)
              )
            ) &&
            !(fAutosyncProfile.ippSettings.timedds[0]==0) &&
            (fUnsuccessfulAlerts==0)
          ) {
            PDEBUGPRINTFX(DBG_HOT,("AUTOSYNC: no credential or no alert within current credential lifetime=%d mins, performing an implicit sync to (re-)subscribe now",fAutosyncProfile.ippSettings.maxinterval));
            // we need to resubscribe
            newstate=autosync_ipp_resubscribe;
            // this is a failed attempt to connect, make sure fAutosyncRetries is not zero any more
            fAutosyncRetries++;
            done=false;
            break;
          }
        }
        // if we have no server here, state must be re-evaluated
        if (fAutosyncProfile.ippSettings.srv[0]==0)
          fIPPState=ippstate_unknown;
        // check for unsubscribed again
        if (fIPPState!=ippstate_subscribed) {
          // not subscribed (no server address) -> keep waiting
          PDEBUGPRINTFX(DBG_TRANSP,("AUTOSYNC: IPP connection cannot be made because IPP state is not subscribed -> wait again"));
          if (fAutosyncRetries<IPP_RESUBSCRIBE_RETRIES)
            fAutosyncRetries=IPP_RESUBSCRIBE_RETRIES; // make sure we do a resubscribe at least
          else
            fAutosyncRetries++;
          newstate=autosync_ipp_connectwait;
          break;
        }
        // prepare for connecting the server (establish connection like GPRS, BT etc.)
        PDEBUGPRINTFX(DBG_TRANSP,("AUTOSYNC: autosync_ipp_connect: establishing BACKGROUND connection (GPRS etc.)"));
        StringObjPrintf(s,"http://%s:%d",fAutosyncProfile.ippSettings.srv,fAutosyncProfile.ippSettings.port);
        err=clientBaseP->establishConnectionFor(s.c_str(),true); // background priority connection
        if (err!=LOCERR_OK) {
          // failed, wait until trying again
          PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: autosync_ipp_connect: failed establishing connection, err=%hd",err));
          newstate=autosync_ipp_connectwait;
          // just not being able to establish a connection MUST NOT trigger a resubscription!
          if (fAutosyncRetries<IPP_RESUBSCRIBE_RETRIES-1)
            fAutosyncRetries++; // count retry if it would not cause a resubscription.
          break;
        }
        // connect to the server
        PDEBUGPRINTFX(DBG_TRANSP,("AUTOSYNC: autosync_ipp_connect: connecting to IPP server '%s'",s.c_str()));
        StringObjPrintf(s,"%s:%d",fAutosyncProfile.ippSettings.srv,fAutosyncProfile.ippSettings.port);
        tcperr = tcpOpenConnectionEx2(
          s.c_str(),
          &fAutosyncIPPSocket,
          fAutosyncProfile.ippSettings.port==443 ? "cS" : "c", // SSL if port is 443, non-SSL otherwise
          10, // do not hang longer than 10 seconds
          NULL
        );
        if (tcperr!=TCP_RC_OK) {
          // failed, wait until trying again
          PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: autosync_ipp_connect: failed connecting IPP server, tcperr=%d",tcperr));
          newstate=autosync_ipp_connectwait;
          fAutosyncRetries++; // count retry
          break;
        }
        #ifdef TCP_LEAK_DEBUG
        fTcpLeakCounter++;
        #endif
        // connection is open, send request
        newstate=autosync_ipp_sendrequest;
        done=false;
        break;
      case autosync_ipp_disconnect:
        // disconnect
        PDEBUGPRINTFX(DBG_TRANSP,("AUTOSYNC: autosync_ipp_disconnect: disconnecting..."));
        if (fAutosyncIPPSocket) {
          tcpCloseConnection(&fAutosyncIPPSocket);
          fAutosyncIPPSocket=0;
          #ifdef TCP_LEAK_DEBUG
          fTcpLeakCounter--;
          #endif
        }
        // release connection (force down if retried three times)
        clientBaseP->releaseConnection(fAutosyncRetries>=IPP_FORCEDOWN_RETRIES);
        PDEBUGPRINTFX(DBG_TRANSP,("AUTOSYNC: disconnected, entering wait for next connection..."));
        newstate=autosync_ipp_connectwait;
        break;
      case autosync_ipp_sendrequest:
        // check for deactivation
        if (!aActive) {
          newstate=autosync_ipp_disconnect;
          done=false;
          break;
        }
        // send IPP request
        // - clear answer string
        fAutosyncIPPAnswer.erase();
        // - nothing known about headers or conten length yet
        fHeaderLength = 0;
        fContentLength = 0;
        if (fAutosyncProfile.ippSettings.method==IPP_METHOD_DMU0) {
          // DMU method 0 (aka IPP, aka DMU demo)
          // - create GET request string
          StringObjPrintf(s,
            "GET %s%s\r\n",
            fAutosyncProfile.ippSettings.path,
            fAutosyncProfile.ippSettings.id
          );
        }
        else if (fAutosyncProfile.ippSettings.method==IPP_METHOD_DMUS2G) {
          // DMU method 1 (s2g router/server architecture)
          // - create LOGIN request string
          StringObjPrintf(s,
            "LOGIN %s%s\r\n"
            "cred=%s\r\n"
            "\r\n",
            fAutosyncProfile.ippSettings.path,
            fAutosyncProfile.ippSettings.id,
            fAutosyncProfile.ippSettings.cred
          );
        }
        else if (fAutosyncProfile.ippSettings.method==IPP_METHOD_OMP) {
          // Oracle Mobile Push
          string data;
          data.erase();
          // - prepare POST request
          StringObjPrintf(s,
            "POST %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: application/vnd.mobilepush\r\n",
            fAutosyncProfile.ippSettings.path,
            fAutosyncProfile.ippSettings.srv
          );
          // - now add specific command
          if (fConversationStage==ippconvstage_login) {
            // need login first
            string pw;
            getUnmangled(pw, fAutosyncProfile.serverPassword);
            // we need to login first
            StringObjPrintf(data,
              "LOGIN %s#P%s %s\r\n",
              fAutosyncProfile.serverUser,
              fAutosyncProfile.ippSettings.id,
              pw.c_str()
            );
          }
          else {
            // not login, add the cookie
            StringObjAppendPrintf(s,
              "Cookie: %s\r\n",
              fOMPCookie.c_str()
            );
            // now the command
            if (fConversationStage==ippconvstage_subscribe) {
              // logged in, now subscribe to all available datastores
              while (fDSCfgIterator != fDatastores.end()) {
                // check the datastore
                CLIENTDSCONFIG *dscfgP = static_cast<CLIENTDSCONFIG *>(*fDSCfgIterator);
                fDSCfgIterator++;
                cAppCharP subsName = NULL;
                if (dscfgP->fDSAvailFlag & dsavail_contacts)
                  subsName = "CONTACTS";
                else if (dscfgP->fDSAvailFlag & dsavail_events)
                  subsName = "CALENDAR";
                else if (dscfgP->fDSAvailFlag & dsavail_tasks)
                  subsName = "TODO";
                else if (dscfgP->fDSAvailFlag & dsavail_emails)
                  subsName = "EMAIL";
                else
                  continue; // try next
                // create subscription for this datastore
                StringObjPrintf(data,
                  "SUBSCRIBE %s\r\n",
                  subsName
                );
                break;
              }
            }
            else if (fConversationStage==ippconvstage_polling) {
              // subscribed, now poll
              // - add push time header
              StringObjAppendPrintf(s,
                "TruePush-time: %d\r\n",
                fAutosyncProfile.ippSettings.period
              );
              // - and the command
              data = "P\r\n";
            }
          }
          // add content with length header
          StringObjAppendPrintf(s,
            "Content-Length: %ld\r\n"
            "\r\n"
            "%s",
            data.size(),
            data.c_str()
          );
        }
        else {
          // unknown method, disconnect again
          PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: Warning: unknown method=%d, disconnecting",(int)fAutosyncProfile.ippSettings.method));
          newstate=autosync_ipp_disconnect;
          done=false;
          break;
        }
        PDEBUGPRINTFX(DBG_TRANSP,("AUTOSYNC: sending request: %s",s.c_str()));
        // - sent it
        tcperr = tcpSendDataEx(
          &fAutosyncIPPSocket, // socket
          (unsigned char *)s.c_str(), // request string
          s.size(),
          5 // maximally 5 seconds (usually, this should work w/o ANY timeout)
        );
        if (tcperr==TCP_RC_ETIMEDOUT)
          break; // just try again later
        if (tcperr!=TCP_RC_OK) {
          // failed: disconnect & reconnect
          PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: failed sending request, tcperr=%d",tcperr));
          newstate=autosync_ipp_disconnect;
          fAutosyncRetries++; // count retry
          done=false;
          break;
        }
        // request sent ok
        newstate=autosync_ipp_answerwait; // and wait for answer
        fAutosyncLastStateChange=now;
        break;
      case autosync_ipp_answerwait:
        // check for deactivation
        if (!aActive) {
          newstate=autosync_ipp_disconnect;
          done=false;
          break;
        }
        // when we've got so far, now enable timed-sync-while-ipp-active
        // if not already enabled (enabled = fAutosyncLastAlert is set)
        if (fAutosyncLastAlert==0) fAutosyncLastAlert=now;
        // wait for IPP answer
        received=bufsiz;
        tcperr = tcpReadDataEx(
          &fAutosyncIPPSocket, // Socket
          buffer,
          &received,
          0
        );
        if (tcperr==TCP_RC_ETIMEDOUT) {
          // check if connection is still ok
          if (!clientBaseP->isConnectionOK()) {
            PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: Connection not ok any more while waiting for answer -> disconnect"));
            newstate=autosync_ipp_disconnect;
            fAutosyncRetries=2; // retry almost immediately
            done=false;
          }
          // check if IPP period is over
          uInt32 timeout =
            fAutosyncProfile.ippSettings.method==IPP_METHOD_OMP && fConversationStage!=ippconvstage_polling ?
            IPP_OMP_COMMAND_TIMEOUT : // non-poll OMP command
            fAutosyncProfile.ippSettings.period; // poll command
          if (now>fAutosyncLastStateChange+timeout*secondToLinearTimeFactor) {
            // IPP period timeout
            PDEBUGPRINTFX(DBG_TRANSP,("AUTOSYNC: IPP period timeout (%d secs), go to disconnect",timeout));
            newstate=autosync_ipp_disconnect;
            fAutosyncRetries=0; // ok so far
            done=false;
          }
          break; // stay in answerwait or disconnect after period timeout
        }
        if (tcperr==TCP_RC_EOF) {
          // connection properly closed
          newstate=autosync_ipp_endanswer;
          break;
        }
        if (tcperr!=TCP_RC_OK) {
          // failed: disconnect & reconnect
          PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: failed receiving answer, tcperr=%d",tcperr));
          newstate=autosync_ipp_disconnect;
          fAutosyncRetries++; // count retry
          done=false;
          break;
        }
        // received answer bytes
        fAutosyncIPPAnswer.append((const char *)buffer,received);
        if (fAutosyncProfile.ippSettings.method==IPP_METHOD_OMP) {
          size_t n;
          // check for content length
          if (fContentLength==0) {
            n = fAutosyncIPPAnswer.find("Content-Length: ");
            if (n!=string::npos) {
              n += 16;
              n += StrToULong(fAutosyncIPPAnswer.c_str()+n, fContentLength, 9);
            }
          }
          // check for end of headers
          if (fHeaderLength==0) {
            n = fAutosyncIPPAnswer.find("\r\n\r\n");
            if (n!=string::npos) {
              fHeaderLength = n+4;
            }
          }
          // check for all data seen
          if (fHeaderLength && fContentLength && fAutosyncIPPAnswer.size()>=fHeaderLength+fContentLength) {
            // End of request, but not necessarily connection.
            // (processing answer will decide if connection must be closed)
            newstate=autosync_ipp_processanswer;
            break;
          }
        }
        // stay in answerwait until connection closes
        break;
      case autosync_ipp_endanswer:
        PDEBUGPRINTFX(DBG_TRANSP,("AUTOSYNC: end of data from server, closing connection"));
        // close connection
        tcpCloseConnection(&fAutosyncIPPSocket);
        fAutosyncIPPSocket=0;
        #ifdef TCP_LEAK_DEBUG
        fTcpLeakCounter--;
        #endif
        // release connection
        clientBaseP->releaseConnection();
        // state is idle after processing a message
        newstate=autosync_ipp_processanswer;
        // leave app some time to close and release the connection
        done=true;
        break;
      case autosync_ipp_processanswer:
        PDEBUGPRINTFX(DBG_HOT,("AUTOSYNC: processing answer from server: %" FMT_LENGTH("0.100") "s",FMT_LENGTH_LIMITED(100,fAutosyncIPPAnswer.c_str())));
        // state is idle after processing a message
        newstate=autosync_idle;
        // process collected answer bytes
        p=fAutosyncIPPAnswer.c_str();
        uInt16 retriedsofar=fAutosyncRetries;
        fAutosyncRetries=0;
        bool disconnect=true; // default to disconnect for every message
        if (fAutosyncProfile.ippSettings.method==IPP_METHOD_OMP) {
          // parse line by line
          bool inheaders=true;
          disconnect=false; // for OMP, only disconnect if explicitly requested by "Connection:" header
          while (*p) {
            // check for those headers we need to know
            if (strucmp(p,"\r\n",2)==0) {
              p+=2;
              inheaders=false;
              break;
            }
            if (strucmp(p,"Set-Cookie: ",12)==0) {
              // extract login cookie
              p+=12; q=p;
              while (*q && *q!=';' && *q>=' ') q++;
              fOMPCookie.assign(p,q-p);
            }
            else if (strucmp(p,"Connection: close",22)==0) {
              disconnect=true;
            }
            // go past next CRLF
            while (*p && *p!='\r') p++;
            p++; if (*p=='\n') p++;
          }
          // now decode status returned (still in headers means failure as well)
          bool success = !inheaders && (strucmp(p,"OK ",3)==0) || isdigit(*p);
          // a non-ok on any stage means we must retry or fall back to login after some retries
          if (!success) {
            fAutosyncRetries=retriedsofar+1; // this is an error, keep counting retries
            PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: OMP server error: %" FMT_LENGTH("0.100") "s",FMT_LENGTH_LIMITED(100,p)));
            newstate=autosync_ipp_disconnect;
            if (fAutosyncRetries>IPP_OMP_RELOGIN_RETRIES) {
              PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: OMP repeated server error -> force relogin"));
              fConversationStage = ippconvstage_login;
            }
            break;
          }
          // basically ok, now depends on communication stage
          if (fConversationStage==ippconvstage_login) {
            // last command sent was login
            // - next is subscribe
            fConversationStage=ippconvstage_subscribe;
            // - start with first datastore
            fDSCfgIterator = fDatastores.begin();
          }
          else if (fConversationStage==ippconvstage_subscribe) {
            // check if more DS must be subscribed
            if (fDSCfgIterator == fDatastores.end())
              fConversationStage=ippconvstage_polling; // all subscribed, proceed to polling now
          }
          else if (fConversationStage==ippconvstage_polling) {
            // check polling response
            while (isdigit(*p)) {
              // event line
              // Syntax: notify_seqno SP dest_app SP username SP deviceID SP event
              // 12 EMAIL john.doe@oracle.com 123123:132123:132123 SYNC
              // - skip seqno
              while (*p && isdigit(*p)) p++;
              if (*p==' ') {
                p++;
                // - extract app
                uInt16 availFlag;
                q=p; while (*q && *q!=' ') q++;
                if (strucmp(p,"CONTACTS",q-p)==0)
                  availFlag = dsavail_contacts;
                else if (strucmp(p,"CALENDAR",q-p)==0)
                  availFlag = dsavail_events;
                else if (strucmp(p,"TODO",q-p)==0)
                  availFlag = dsavail_tasks;
                else if (strucmp(p,"EMAIL",q-p)==0)
                  availFlag = dsavail_emails;
                if (availFlag) {
                  // check for SYNC event
                  p=q; if (*p==' ') p++;
                  // - skip user
                  while (*p && *p!=' ') p++;
                  if (*p==' ') p++;
                  // - skip device ID
                  while (*p && *p!=' ') p++;
                  if (*p==' ') p++;
                  // check for SYNC command
                  if (strucmp(p,"SYNC",4)==0) {
                    PDEBUGPRINTFX(DBG_HOT,("AUTOSYNC: received OMP sync request for availFlags=0x%hX",availFlag));
                    // alert for sync
                    if (alertDbByAppType(availFlag,true)) {
                      fAutosyncAlerted=true;
                      fAutosyncLastAlert=now;
                      disconnect=true; // break TCP connection
                    }
                  }
                }
                else
                  break;
              }
              // forward to end of line or string
              while (*p && (*p>=0x20)) p++;
              while (*p && ((*p=='\n') || (*p=='\r'))) p++;
            } // while
          }
        }
        else {
          // DMU0 or DMUS2G
          while (*p) {
            // check for basic command
            if (
              (fAutosyncProfile.ippSettings.method==IPP_METHOD_DMU0 && strucmp(p,"Sync:",5)==0) ||
              (fAutosyncProfile.ippSettings.method==IPP_METHOD_DMUS2G && strucmp(p,"sync=",5)==0)
            ) {
              p+=5;
              // get datastore name (remote datastore path, that is!)
              p=getParam(p,dbname,false);
              // get eventual additional params
              // - first is filter CGI
              p=getParam(p,filtercgi,true);
              // search DB to flag autosync for it
              PDEBUGPRINTFX(DBG_HOT,("AUTOSYNC: received sync request for db='%s', filter='%s'",dbname.c_str(),filtercgi.c_str()));
              // alert for sync
              if (alertDbByName(dbname.c_str(),filtercgi.c_str(),true)) {
                fAutosyncAlerted=true;
                fAutosyncLastAlert=now;
              }
            } // if "Sync:" or "sync=" line
            else if (
              (fAutosyncProfile.ippSettings.method==IPP_METHOD_DMUS2G && strucmp(p,"error=",6)==0)
            ) {
              p+=6;
              fAutosyncRetries=retriedsofar+1; // this is an error, keep counting retries
              // get error code
              uInt32 dmuerr=99999; // unknown error code
              StrToULong(p,dmuerr,6);
              if (dmuerr==10010 || dmuerr==10020) {
                // credential unknown, bad or expired
                // - make sure that we'll resubscribe
                if (fAutosyncRetries<IPP_RESUBSCRIBE_RETRIES)
                  fAutosyncRetries=IPP_RESUBSCRIBE_RETRIES;
              }
              // seems that we have a problem, cause re-subscription
              PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: DMU server error %ld, try again",dmuerr));
              newstate=autosync_ipp_disconnect;
            }
            // skip until eoln or eot
            while (*p && *p!='\n' && *p!='\r') p++;
            while (*p=='\n' || *p=='\r') p++;
            // this is now eot or start of next line
          } // while text
        } // if DMU0 or DMUS2G
        // close connection if requested (and not already closed anyway)
        if (disconnect) {
          // server requests disconnect, do it right now
          newstate = autosync_ipp_disconnect;
          done=false;
          break;
        }
        else if (fAutosyncIPPSocket) {
          // server does not request disconnect and TCP socket is still open - proceed with polling in same connection
          newstate = autosync_ipp_sendrequest;
        }
        done=true;
        break;
      #endif // IPP_SUPPORT
    } // switch
    // now perform state change if any
    if (newstate!=fAutosyncState) {
      // show transition
      #ifdef IPP_SUPPORT
      PDEBUGPRINTFX(DBG_TRANSP,(
        "AUTOSYNC: * STATE CHANGES from %s to %s, mode=%s, active=%s, ippstate=%s, retries=%hd, unsuccessful Alerts=%hd",
        AutoSyncStateNames[fAutosyncState],
        AutoSyncStateNames[newstate],
        AutoSyncModeNames[fAutoSyncMode],
        aActive ? "yes" : "no",
        IPPStateNames[fIPPState],
        fAutosyncRetries,
        fUnsuccessfulAlerts
      ));
      #else
      PDEBUGPRINTFX(DBG_TRANSP,(
        "AUTOSYNC: * STATE CHANGES from %s to %s, mode=%s, active=%s, unsuccessful Alerts=%hd",
        AutoSyncStateNames[fAutosyncState],
        AutoSyncStateNames[newstate],
        AutoSyncModeNames[fAutoSyncMode],
        aActive ? "yes" : "no",
        fUnsuccessfulAlerts
      ));
      #endif
      #ifdef TCP_LEAK_DEBUG
      PDEBUGPRINTFX(DBG_TRANSP,("            Number of open TCP connections = %ld",fTcpLeakCounter));
      #endif
      // set new state
      fAutosyncState=newstate;
    }
  } while (!done);
} // CLIENTAGENTCONFIG::autosync_step


/// @brief alert a datastore by remote database name
/// @note if syncType is given (e.g. from a SAN) complete overwrites are possible if explicitly requested
bool CLIENTAGENTCONFIG::alertDbByName(const char *aRemoteDBName, const char *aFilterCGI, bool aForced, uInt16 aSyncAlert, bool aAlwaysStart)
{
  sInt32 idx,seq;
  TARGETRECORD target;
  CLIENTDSCONFIG *dscfgP;
  bool alertedAny=false;

  seq=0;
  do {
    idx=findTargetIndex(fAutosyncProfile.profileID,seq++);
    if (idx<0) break; // no more targets
    getTarget(idx,target);
    // compare DB names
    if (strucmp(target.remoteDBpath,aRemoteDBName,strlen(aRemoteDBName))==0) {
      // found target record, get related datastore in config
      dscfgP = static_cast<CLIENTDSCONFIG *>(
        getLocalDS(target.dbname)
      );
      bool mayStart= aAlwaysStart || (target.lastSync!=0); // start allowed ONLY if we have synced before with this target
      // if we have an explicit sync mode, this overrides the preset mode
      if (mayStart) {
        if (aSyncAlert!=0) {
          // caller provides a sync mode
          dscfgP->fAutosyncAlertCode=aSyncAlert; // use passed sync alert code
        }
        else {
          // do not automatically start in "dangerous" modes
          dscfgP->fAutosyncAlertCode=0; // get mode from config
          mayStart = !(
            target.forceSlowSync &&
            (target.syncmode==smo_fromclient || target.syncmode==smo_fromserver)
          );
        }
      }
      // start if not "dangerous" or when sync mode is explicitly requested by trusted party (SAN)
      if (mayStart) {
        // alert
        dscfgP->fAutosyncForced=aForced; // forced if selected
        dscfgP->fAutosyncAlerted=true; // alerted anyway
        AssignString(dscfgP->fAutosyncPathCGI, aFilterCGI);
        // alerted at least one, IPP is in alerted state now
        alertedAny=true;
        PDEBUGPRINTFX(DBG_HOT,("AUTOSYNC: *** ALERTED autosync for datastore '%s' (requested alert=%hd)",dscfgP->getName(),dscfgP->fAutosyncAlertCode));
      }
      else {
        // prevent autosync because of possible zap
        PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: datastore '%s' never synceed so far or would IMPLICITLY cause client or server to be totally overwritten -> do not autosync",dscfgP->getName()));
      }
    }
  } while (true);
  return alertedAny;
} // CLIENTAGENTCONFIG::alertDbByName


/// @brief alert a datastore by generic application type (DSAvailFlag)
/// @note if syncType is given (e.g. from a SAN) complete overwrites are possible if explicitly requested
bool CLIENTAGENTCONFIG::alertDbByAppType(uInt16 aDSAvailFlag, bool aForced, bool aAlwaysStart)
{
  bool alertedAny=false;
  TARGETRECORD target;
  TLocalDSList::iterator pos;

  for(pos=fDatastores.begin();pos!=fDatastores.end();pos++) {
    CLIENTDSCONFIG *dscfgP = static_cast<CLIENTDSCONFIG *>(*pos);
    if ((dscfgP->fDSAvailFlag & aDSAvailFlag) == aDSAvailFlag) {
      // found datastore
      // - get target record to check settings
      sInt32 idx = findTargetIndexByDBInfo(fAutosyncProfile.profileID, dscfgP->fLocalDBTypeID, NULL);
      if (idx>=0) {
        getTarget(idx,target);
        bool mayStart= aAlwaysStart || (target.lastSync!=0); // start allowed ONLY if we have synced before with this target
        // if we have an explicit sync mode, this overrides the preset mode
        if (mayStart) {
          // do not automatically start in "dangerous" modes
          dscfgP->fAutosyncAlertCode=0; // get mode from config
          mayStart = !(
            target.forceSlowSync &&
            (target.syncmode==smo_fromclient || target.syncmode==smo_fromserver)
          );
        }
        // start if not "dangerous" or when sync mode is explicitly requested by trusted party (SAN)
        if (mayStart) {
          // alert
          dscfgP->fAutosyncForced=aForced; // forced if selected
          dscfgP->fAutosyncAlerted=true; // alerted anyway
          // alerted at least one, IPP is in alerted state now
          alertedAny=true;
          PDEBUGPRINTFX(DBG_HOT,("AUTOSYNC: *** ALERTED OMP autosync for datastore '%s' (requested alert=%hd)",dscfgP->getName(),dscfgP->fAutosyncAlertCode));
        }
        else {
          // prevent autosync because of possible zap
          PDEBUGPRINTFX(DBG_ERROR,("AUTOSYNC: datastore '%s' never synced so far or would IMPLICITLY cause client or server to be totally overwritten -> do not autosync",dscfgP->getName()));
        }
      }
    }
  }
  return alertedAny;
} // CLIENTAGENTCONFIG::alertDbByAppType



#endif


/* eof */
