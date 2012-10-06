//
//  SyncMLClient.m
//  TodoZ
//
//  Created by Lukas Zeller on 2011/02/03.
//  Copyright 2011 plan44.ch. All rights reserved.
//

#import "SyncMLClient.h"

#import "ZKeyChainWrapper.h"

@interface SyncMLClient ()
// private methods
- (void)fatalError:(NSString *)aErrorMsg;
- (void)updateProgressStatus:(TEngineProgressInfo *)aProgressInfoP;
@end



@implementation SyncMLClient

@synthesize lastStatus;
@synthesize syncMLURL;
@synthesize lastTransportStatus;
@synthesize sessionEndStatus;
@synthesize localDataChanged;
@synthesize loggingSession;


- (id)initWithClientConfigFile:(NSString *)aClientConfigFile
{
	if ((self = [super init])) {
    syncEngine = nil;
    clientConfigFile = [aClientConfigFile retain];
    NSAssert(clientConfigFile, @"SyncMLClient: initWithClientConfigFile: No config file path specified");
    syncSession = nil;
    syncMLURL = nil;
    localDataChanged = NO;
    loggingSession = NO;
    abortRequested = NO;
    suspendRequest = NO;
    suspendRequested = NO;
    runsInBackgroundThread = NO;
    lastStatus = LOCERR_WRONGUSAGE;
    lastTransportStatus = LOCERR_WRONGUSAGE;
  }
  return self;
}


- (void)dealloc
{
  [syncSession release]; syncSession=nil;
  [syncEngine release]; syncEngine=nil;
  [clientConfigFile release]; clientConfigFile=nil;
  [syncMLURL release]; syncMLURL=nil;
	[super dealloc]; // includes stop
}


#pragma private methods

- (void)fatalError:(NSString *)aErrorMsg
{
  DBGNSLOG(@"fatalError: %@",aErrorMsg);
  if (self.delegate && [self.delegate respondsToSelector:@selector(fatalSyncMLError:syncMLClient:)]) {
    [self.delegate fatalSyncMLError:aErrorMsg syncMLClient:self];
  }
}



#pragma mark Setting up the SyncML engine


// standard configuration of sync engine (before config XML loading)
// - calls delegate to add custom config
// - can also be derived to add custom config
- (TSyError)configSyncEngine:(SyncEngine *)aSyncEngine
{
	TSyError sta;
  SettingsKey *configVarsKey = [syncEngine newOpenKeyByPath:"/configvars" withMode:0 err:&sta];
  if (sta==LOCERR_OK) {
    // sandbox home path
    [configVarsKey setStringValueByName:"sandbox" toValue:NSHomeDirectory()];
    // pass debug status to config
    #if DEBUG
    [configVarsKey setStringValueByName:"debugbuild" toValue:@"1"];
    #endif
    // done with config vars
  }
  [configVarsKey release];
  // call delegate
  if (self.delegate && [delegate respondsToSelector:@selector(willInitSyncMLEngine:)]) {
    [self.delegate willInitSyncMLEngine:syncEngine];
  }
  return sta;
}


- (SyncEngine *)syncEngine
{
	if (!syncEngine) {
    // Init SyncML engine
    lastStatus = LOCERR_OK;
    // - create the engine
    syncEngine =
      [[SyncEngine alloc] initWithLibraryPath:@"dummy_because_statically_linked"
        andDebugFlags:0
        andEntryPointPrefix:@"SySync_" // client entry point
      ];
    DBGNSLOG(@"after SyncEngine init, syncEngine=0x%lX",(unsigned long)(void *)syncEngine);
    if (syncEngine == nil) {
      [self fatalError:@"Cannot initialize Sync Engine library as client"];
      return nil;
    }
    // pre-set some config vars
		[self configSyncEngine:syncEngine];
    // init engine with config
    lastStatus = [syncEngine initEngineFile:clientConfigFile];
    DBGNSLOG(@"initEngineFile:%@ sta=%hd\n",clientConfigFile,lastStatus);
    if (lastStatus != LOCERR_OK) {
      if (lastStatus == LOCERR_BADREG) {
        // unregistered - still allow start-up
        DBGNSLOG(@"xml config returned LOCERR_BADREG - ignore for now");
      }
      else if (lastStatus == LOCERR_EXPIRED) {
        // expired - display appropriate message
        [self fatalError:@"application expired"];
      }
      else {
        [self fatalError:[NSString stringWithFormat:@"Cannot read XML config - err=%hd\n",lastStatus]];
      }
    }
    // show some config vars again
    SettingsKey *configVarsKey = [syncEngine newOpenKeyByPath:"/configvars" withMode:0 err:&lastStatus];
    if (lastStatus==LOCERR_OK) {
      DBGNSLOG(@"logpath = %@\n", [configVarsKey stringValueByName:"logpath"]);
    }
    else {
      DBGNSLOG(@"error opening /configvars, sta=%hd\n",lastStatus);
    }
    [configVarsKey release];
  }
  return syncEngine;
}


#pragma mark configuration management


/// create a new default profile (and return the newly created key)
- (SettingsKey *)newDefaultProfileKey:(SettingsKey *)aProfilesKey
{
  SettingsKey *profileKey = nil;
  if (delegate && [delegate respondsToSelector:@selector(newDefaultSyncMLProfileIn:)]) {
    profileKey = [delegate newDefaultSyncMLProfileIn:aProfilesKey];
  }
  if (!profileKey) {
    profileKey = [aProfilesKey newOpenSubKeyByID:KEYVAL_ID_NEW_DEFAULT withMode:0 err:&lastStatus];
  }
  return profileKey;
}


/// create a new profile with default values
/// Note: default value are either engine-level or obtained via newDefaultSyncMLProfileIn delegate method
- (SettingsKey *)newDefaultProfileKey
{
  SettingsKey *profilesKey = [self newProfilesKey];
  SettingsKey *newProfile = [self newDefaultProfileKey:profilesKey];
  if (newProfile) [newProfile ownParent:profilesKey];
  [profilesKey release];
  return newProfile;
}


- (SettingsKey *)newProfilesKey
{
  return
    [self.syncEngine
      newOpenKeyByPath:"/profiles" withMode:0 err:&lastStatus
    ];
}



/// open a key for a given profile.
/// @param aProfileID profile to open
///   PROFILEID_FIRST=first,
///   PROFILEID_FIRST_OR_DEFAULT=first, and create default profile if none exists so far
- (SettingsKey *)newProfileKeyForID:(sInt32)aProfileID
{
  // open profiles
  SettingsKey *profileKey = nil;
  SettingsKey *profilesKey = [self newProfilesKey];
  if (lastStatus==LOCERR_OK) {
    if (aProfileID>=0) {
      // specific profile
      profileKey = [profilesKey newOpenSubKeyByID:aProfileID withMode:0 err:&lastStatus];
    }
    else if (aProfileID==PROFILEID_FIRST || aProfileID==PROFILEID_FIRST_OR_DEFAULT) {
      // load first profile
      profileKey = [profilesKey newOpenSubKeyByID:KEYVAL_ID_FIRST withMode:0 err:&lastStatus];
      // if no profile at all, create default profile
      if (lastStatus==DB_NoContent && (aProfileID==PROFILEID_FIRST_OR_DEFAULT)) {
        profileKey = [self newDefaultProfileKey:profilesKey];
      }
    }
    // attach container to the returned key, will be closed with it
    if (profileKey) [profileKey ownParent:profilesKey];
    [profilesKey release];
  }
  return profileKey;
}



#pragma mark running a sync session

- (BOOL)isRunning
{
  // when we have a sync session, it is running
  return syncSession!=nil;
}


- (BOOL)isSuspending
{
  // running sync session and suspend request pending or processing
  return self.isRunning && (suspendRequest || suspendRequested);
}





/// start sync with specified profileID
/// @param aProfileID profile to sync
/// @return YES if sync could be initiated.
- (BOOL)startWithProfileID:(sInt32)aProfileID inBackgroundThread:(BOOL)aInBackgroundThread
{
  if (!self.isRunning) {
    // start new sync
    // - check for loggin in the profile
    SettingsKey *profileKey = [self newProfileKeyForID:aProfileID];
    if (profileKey) {
      // save profileSelector
      profileSelector = aProfileID;
      // determine if logging this session
      loggingSession = ([profileKey intValueByName:"profileFlags"] & PROFILEFLAG_LOGNEXTSYNC)!=0;
      [profileKey release];
      // if so, make sure logging dir exists, as this makes the engine actually creating logs.
      // Note: for DEBUG builds, always create the logdir
      #ifndef ALWAYS_KEEP_SYNC_LOGS
      if (loggingSession)
      #endif
      {
        // - get log path as configured
        SettingsKey *configVarsKey = [self.syncEngine newOpenKeyByPath:"/configvars" withMode:0 err:&lastStatus];
        NSString *logpath = [configVarsKey stringValueByName:"logpath"];
        [configVarsKey release];
        NSError *nserror;
        // make sure log directory exists
        [[NSFileManager defaultManager] createDirectoryAtPath:logpath withIntermediateDirectories:NO attributes:nil error:&nserror];
        DBGNSLOG(@"Session will create logs in %@",logpath);
      }
    }
    // - init flags
    suspendRequest = NO;
    suspendRequested = NO;
    localDataChanged = NO; // nothing changed yet
    // create a new session
    syncSession = [self.syncEngine newOpenSessionWithSelector:aProfileID andSessionName:"syncMLiOS" err:&lastStatus];
    if (lastStatus!=LOCERR_OK) {
      if (lastStatus==LOCERR_EXPIRED) {
        [self fatalError:@"Demo library or license has expired"];
      }
      else {
        [self fatalError:[NSString stringWithFormat:@"Cannot open a sync session handle, error=%d",lastStatus]];
      }
      return NO; // failed
    }
    // - initialize stepping state
    stepCmd = STEPCMD_CLIENTSTART;
    // - issue pseudo-event
    TEngineProgressInfo pi;
    pi.eventtype = PEV_PROFILEID;
    pi.extra1 = aProfileID;
    pi.targetID = 0;
    [self updateProgressStatus:&pi];
    // - start sync
    runsInBackgroundThread = aInBackgroundThread;
    if (runsInBackgroundThread) {
      // on a separate thread
      [self performSelectorInBackground:@selector(syncThreadMain) withObject:nil];
    }
    else {
      // on main thread
      [self performSelector:@selector(nextSyncStep) withObject:nil afterDelay:0];
    }
    return YES; // initiated
  }
  return NO; // already running, not initiated again
}


- (void)suspend
{
  if (!suspendRequest) {
    suspendRequest = YES; // request pending
    suspendRequested = NO; // not yet proapagated to engine
    // issue a pseudo early "suspending" event such that we have immediate feedback
    TEngineProgressInfo pi;
    pi.eventtype = PEV_SUSPENDING;
    pi.targetID = 0;
    [self updateProgressStatus:&pi];
  }
}


- (void)stop
{
  // signal abort to engine
  abortRequested = YES;
  // make sure pending network request ends as well
  [self abortRequest];
}


#pragma mark - internal methods for transport handling

// called in all cases, on failure as well as on success
// on transport level failure, failedWebRequestWithReadStream will be called before
- (void)completedWebRequestWithStatus:(CFIndex)aHTTPStatus
{
  // ended network activity
  [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;
  // check status
  if (aHTTPStatus==200) {
    // pass filled buffer back to engine
    [syncSession retSyncMLBufferForSend:NO processed:self.answerSize];
    // all data received, signal "got data" for next step
    stepCmd = STEPCMD_GOTDATA; // signal engine that we have received response data
  }
  else {
    // SyncML request failed
    stepCmd = STEPCMD_TRANSPFAIL; // signal engine that transport failed
    // see if we need to analyze the HTTP answer code
    if (lastTransportStatus==LOCERR_OK) {
      // no details about transport failure yet, try to get some useful info from HTTP response
      switch (aHTTPStatus) {
        case 404: lastTransportStatus = LOCERR_SRVNOTFOUND; break;
        case 401:
        case 403: lastTransportStatus = LOCERR_AUTHFAIL; break;
        case ZWEBREQUEST_STATUS_NONE: lastTransportStatus = LOCERR_BADCONTENT; break; // apparently not HTTP
        case ZWEBREQUEST_STATUS_ABORTED: lastTransportStatus = LOCERR_USERABORT; break;
        case ZWEBREQUEST_STATUS_NETWORK_ERROR: lastTransportStatus = LOCERR_TRANSPFAIL; break;
        default: lastTransportStatus = LOCERR_TRANSPFAIL; break;
      }
    }
  }
  // done with reqest
  [syncMLURL release]; syncMLURL=nil;
  // make sure engine gets called next
  [self performSelector:@selector(nextSyncStep) withObject:nil afterDelay:0];
}


// called BEFORE completedWebRequestWithStatus in case there are transport level problems
- (void)failedWebRequestWithReadStream:(CFReadStreamRef)aReadStream
{
  CFErrorRef streamErr;
  streamErr = CFReadStreamCopyError(aReadStream);
  CFIndex errCode = CFErrorGetCode(streamErr);
  // now catch some of the distinct error codes to cause better display in the UI
  switch (errCode) {
      // CFNetwork
    case kCFErrorHTTPBadURL :
      lastTransportStatus = LOCERR_BADURL;
      break;
    case kCFHostErrorHostNotFound :
      lastTransportStatus = LOCERR_SRVNOTFOUND;
      break;
      // SSL
    case kCFURLErrorServerCertificateHasBadDate:
    case kCFURLErrorServerCertificateNotYetValid:
    case -9828: // errSSLPeerCertExpired, but SecureTransport.h is missing in SDK. Probably device never returns this, but Simulator does
      lastTransportStatus = LOCERR_CERT_EXPIRED;
      break;
    case kCFURLErrorServerCertificateUntrusted:
    case kCFURLErrorServerCertificateHasUnknownRoot:
    case kCFURLErrorSecureConnectionFailed:
      lastTransportStatus = LOCERR_CERT_INVALID; // invalid cert (or general SSL error)
      break;
    default:
      // catch-all other SSL errors that largely mean invalid certificate
      // SecureTransport.h is missing in iPhone OS device SDK headers
      if (errCode<=-9807 && errCode>=-9849) // errSSLXCertChainInvalid..errCode>=errSSLLast (SecureTransport.h is missing in SDK. Probably device never returns these, but Simulator does)
        lastTransportStatus = LOCERR_CERT_INVALID; // invalid cert (or general SSL error)
      else
        lastTransportStatus = LOCERR_TRANSPFAIL; // general network error
      break;
  }
}




// Start SyncML data exchange by setting up SyncML request to server
// Note: as request exchange is one operation, actual exchange will start not before startDataWait
- (BOOL)startDataExchange
{
  // no transport failure so far
  lastTransportStatus = LOCERR_OK;
  // get params from session
  SettingsKey *sessionKey = [syncSession newOpenSessionKeyWithMode:0 err:&lastStatus];
  if (lastStatus!=LOCERR_OK)
    return false; // error
  // get URL
  [syncMLURL release];
  syncMLURL = [[sessionKey stringValueByName:"connectURI"] retain];
  // get content type
  self.contentType = [sessionKey stringValueByName:"contenttype"];
  // get extra HTTP parameters from profile
  SettingsKey *profileKey = [sessionKey newOpenKeyByPath:"profile" withMode:0 err:&lastStatus];
  if (lastStatus==LOCERR_OK) {
    // we can access the profile, get extra params
    if ([profileKey intValueByName:"transpFlags"] & TRANSPFLAG_USEHTTPAUTH) {
      // Use HTTP auth
      self.httpUser = [profileKey stringValueByName:"transportUser"];
      // get PW from keychain
      self.httpPassword = [[ZKeyChainWrapper sharedKeyChainWrapper]
        passwordForService:[NSString stringWithFormat:@"SyncMLProfile.%d",[profileKey keyID]]
        account:@"transportPassword"
        error:NULL
      ];
    }
    else {
      // do not use HTTP auth
      self.httpUser = nil;
      self.httpPassword = nil;
    }
    // - use system proxy switch
    self.useSysProxy = [profileKey intValueByName:"useConnectionProxy"];
    // - SSL
    self.ignoreSSLErrors = [profileKey intValueByName:"transpFlags"] & TRANSPFLAG_SSLIGNORECERTFAIL;
    // done
    [profileKey release];
  }
  // close session key
  [sessionKey release];
  // send data to server and expect answer
  appPointer syncmlRequestData;
  lastStatus = [syncSession getSyncMLBufferForSend:YES buffer:&syncmlRequestData bufSize:&syncmlRequestSize];
  if (lastStatus==LOCERR_OK) {
    // prepare request
    [self setRequestBuffer:syncmlRequestData withSize:syncmlRequestSize];
  }
  // Don't start sending yet, first call sync engine again to indicate we have acknowledged the data
  return lastStatus==LOCERR_OK;
}


- (BOOL)startDataWait
{
  // send as POST request
  if ([self sendRequestToURL:self.syncMLURL withHTTPMethod:@"POST"]) {
    // request initiated
    // - starting network activity
    [UIApplication sharedApplication].networkActivityIndicatorVisible = YES;
    // started ok
    lastStatus=LOCERR_OK;
  }
  else {
    // failed starting request
    lastStatus=LOCERR_TRANSPFAIL; // failed
  }
  // anyway, release SyncML request buffer
  [syncSession retSyncMLBufferForSend:YES processed:syncmlRequestSize];
  // prepare answer buffer
  if (lastStatus==LOCERR_OK) {
    // get the buffer to put SyncML response into
    appPointer syncmlAnswerData; // buffer pointer (received from SyncML engine)
    memSize syncmlAnswerMax; // size of buffer
    lastStatus = [syncSession getSyncMLBufferForSend:NO buffer:&syncmlAnswerData bufSize:&syncmlAnswerMax];
    if (lastStatus==LOCERR_OK) {
      [self setAnswerBuffer:syncmlAnswerData withSize:syncmlAnswerMax];
    }
  }
  // in case of successful start, completedWebRequestWithStatus will continue execution of sync session
  return lastStatus==LOCERR_OK;
}



# pragma mark internal methods for running sync

- (void)sendAndReleaseProgressDict:(NSDictionary *)aDict
{
  [[NSNotificationCenter defaultCenter]
    postNotificationName:@"SyncMLClientProgress" // the name
    object:self // the sender
    userInfo:aDict
  ];
  // done
  [aDict release];
}



// report progress
- (void)updateProgressStatus:(TEngineProgressInfo *)aProgressInfoP
{
  // Show status to console when compiled with DEBUG=1 defined
  DBGNSLOG(
    @"*** Got SyncML Client progress Info: eventtype=%hd, targetid=%ld, extra1=%ld, extra2=%ld, extra3=%ld\n",
    aProgressInfoP->eventtype,
    (long int)aProgressInfoP->targetID,
    aProgressInfoP->extra1,
    aProgressInfoP->extra2,
    aProgressInfoP->extra3
  );
  // check for event that means local data has changed
  if (aProgressInfoP->eventtype==PEV_ITEMRECEIVED || aProgressInfoP->eventtype==PEV_DELETING)
    localDataChanged = YES;
  // check for event that delivers session status
  if (aProgressInfoP->eventtype==PEV_SESSIONEND)
    sessionEndStatus = aProgressInfoP->extra1;
  // make dict
  NSDictionary *dict = [[NSDictionary alloc] initWithObjectsAndKeys:
    [NSNumber numberWithInt:aProgressInfoP->eventtype], @"eventtype",
    [NSNumber numberWithInt:aProgressInfoP->targetID], @"targetID",
    [NSNumber numberWithInt:aProgressInfoP->extra1], @"extra1",
    [NSNumber numberWithInt:aProgressInfoP->extra2], @"extra2",
    [NSNumber numberWithInt:aProgressInfoP->extra3], @"extra3",
    nil
  ];
  // deliver to main thread
  if (runsInBackgroundThread) {
    // execute progress notification on the main thread
    [self performSelectorOnMainThread:@selector(sendAndReleaseProgressDict:) withObject:dict waitUntilDone:YES];
  }
  else {
    // we are on the main thread, directly send
    [self sendAndReleaseProgressDict:dict];
  }
}


- (void)notifyDelegateOfEndOfSync
{
  if (delegate && [delegate respondsToSelector:@selector(completedSyncMLClientSync:)]) {
    [delegate completedSyncMLClientSync:self];
  }
}


// execute next step of a sync session
- (BOOL)nextSyncStep
{
  TEngineProgressInfo progressInfo;
  TSyError sta;
  BOOL scheduleNextStepNow = YES;
  BOOL initSession = stepCmd==STEPCMD_CLIENTSTART || stepCmd==STEPCMD_CLIENTAUTOSTART;

  // take next step
  // - first check for suspend or abort, if so, modify step command for next step
  if (abortRequested) {
    DBGNSLOG(@"!!! abort request detected - setting stepCmd to STEPCMD_ABORT");
    stepCmd = STEPCMD_ABORT;
    abortRequested = NO;
  }
  else if (suspendRequest) {
    DBGNSLOG(@"!!! suspend request detected - setting stepCmd to STEPCMD_SUSPEND");
    stepCmd = STEPCMD_SUSPEND;
    suspendRequest = NO;
    suspendRequested = YES; // flag for two step button to issue an abort on second use
  }
  DBGNSLOG(@"runSyncStep calling sessionStepWithCmd:%hd",stepCmd);
  sta = [syncSession sessionStepWithCmd:&stepCmd andProgressInfo:&progressInfo];
  DBGNSLOG(@"- sessionStepWithCmd returned stepCmd:%hd sta=%d", stepCmd, sta);
  if (sta!=LOCERR_OK) {
    // fatal error, immediately terminate with error
    DBGNSLOG(@"-> unexpected error %hd while processing session step - discontinue session",sta);
  }
  else {
    // step ran ok, evaluate step command
    if (initSession) {
      // Note: the following code can be used to provide the session password on-the-fly
      //       rather than relying on passwords stored in the profile (which is stored obfuscated,
      //       but not hard encrypted). For example, passwords can be stored in the keychain
      //       or requested from the user via a dialog, depending on security needs.
      //       For simplicity of this sample however the password is stored in the profile.
      // last step was initializing the session, now provide session password from keychain
      SettingsKey *sessionKey = [syncSession newOpenSessionKeyWithMode:0 err:&sta];
      if (sta==LOCERR_OK) {
        SettingsKey *profileKey = [sessionKey newOpenKeyByPath:"profile" withMode:0 err:&sta];
        if (sta==LOCERR_OK) {
          [sessionKey setStringValueByName:"sessionPassword" toValue:
            [[ZKeyChainWrapper sharedKeyChainWrapper]
              passwordForService:[NSString stringWithFormat:@"SyncMLProfile.%d",[profileKey keyID]]
              account:@"SyncMLPassword"
              error:NULL
            ]
          ];
          [profileKey release];
        }
        [sessionKey release];
      }
    }
    switch (stepCmd) {
      case STEPCMD_OK:
        // no progress info, call step again
        stepCmd = STEPCMD_STEP;
        break;
      case STEPCMD_PROGRESS:
        // new progress info to show
        [self updateProgressStatus:&progressInfo];
        stepCmd = STEPCMD_STEP;
        break;
      case STEPCMD_ERROR:
        // error, terminate (should not happen, as status is already checked above)
        break;
      case STEPCMD_RESTART:
        // make sure connection is closed and will be re-opened for next request
        // %%% Nop at this time as we don't keep the connection anyway in this simplified sample
        stepCmd = STEPCMD_STEP;
        break;
      case STEPCMD_SENDDATA:
        // send data to remote
        if (![self startDataExchange])
          stepCmd = STEPCMD_TRANSPFAIL;
        else
          stepCmd = STEPCMD_SENTDATA; // signal engine that we have sent the request data
        break;
      case STEPCMD_NEEDDATA:
        // initiate receiving data
        if ([self startDataWait]) {
          // start waiting initiated - let runloop notify us when complete
          scheduleNextStepNow = NO; // end of receiving data will restart the timer
        }
        else
          stepCmd = STEPCMD_TRANSPFAIL; // failure
        break;
    } // switch stepcmd
  }
  // check for end of session
  if (stepCmd==STEPCMD_DONE || stepCmd==STEPCMD_ERROR) {
    // done with session
    DBGNSLOG(@"SyncML Session terminated with stepCmd:%hd, sessionEndStatus=%hd",stepCmd,sessionEndStatus);
    // release session
    [syncSession release]; syncSession = nil;
    // reset logging flag if we did log
    if (loggingSession) {
      SettingsKey *profileKey = [self newProfileKeyForID:profileSelector];
      if (profileKey) {
        // reset log session flag in profile
        uInt32 profileFlags = [profileKey intValueByName:"profileFlags"];
        [profileKey setIntValueByName:"profileFlags" toValue:profileFlags & ~PROFILEFLAG_LOGNEXTSYNC];
        [profileKey release];
        DBGNSLOG(@"Session was logging, reset log-next-sync flag in profile now");
      }
    }
    // done
    if (runsInBackgroundThread) {
      // just clear the background running flag, this will exit the background thread
      runsInBackgroundThread = NO;
      // stop the current runloop
      CFRunLoopStop(CFRunLoopGetCurrent());
    }
    else {
      // directly inform delegate if requested
      [self notifyDelegateOfEndOfSync];
    }
    return NO; // no next step
  }
  else if (scheduleNextStepNow) {
    // continues, let runloop issue next step shortly
    [self performSelector:@selector(nextSyncStep) withObject:nil afterDelay:0];
  }
  return YES; // next step scheduled directly or will be issued by network callback later
} // nextSyncStep


#pragma mark - support for running sync on a background thread

- (void)syncThreadMain
{
  if (runsInBackgroundThread) {
    // set up autorelease pool
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init]; // Top-level pool
    // init a run loop
    NSRunLoop* myRunLoop = [NSRunLoop currentRunLoop]; // runloop is created on demand if it does not yet exist
    // add a timer source to fire the next sync step
    [self performSelector:@selector(nextSyncStep) withObject:nil afterDelay:0];
    // now run the loop
    do {
      // Start the run loop for 10 seconds max, but return after each source is handled.
      SInt32 result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 10, YES);
      // If a source explicitly stopped the run loop, or if there are no
      // sources or timers, go ahead and exit.
      if ((result == kCFRunLoopRunStopped) || (result == kCFRunLoopRunFinished)) {
        runsInBackgroundThread = NO; // done
      }
    }
    while (runsInBackgroundThread);
    // done with the thread
    [pool release];
  }
  // have delegate informed on main thread
  [self performSelectorOnMainThread:@selector(notifyDelegateOfEndOfSync) withObject:nil waitUntilDone:YES];
}



@end // SyncMLClient
