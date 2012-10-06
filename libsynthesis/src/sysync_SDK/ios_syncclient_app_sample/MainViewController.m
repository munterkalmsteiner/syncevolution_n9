//
//  MainViewController.m
//  ios_syncclient_app_sample
//
//  Created by Lukas Zeller on 2011/09/16.
//  Copyright (c) 2011 plan44.ch. All rights reserved.
//

#import "MainViewController.h"

#include "AppDelegate.h"

@interface MainViewController ()
// private methods declaration

- (void)updateProgressInfo:(NSNotification *)aNotification;
- (void)updateStaticStatus;

+ (NSString *)stringWithErrorText:(TSyError)aSta;
+ (NSString *)stringWithEventText:(NSDictionary *)aProgressInfoDict andTranspStatus:(TSyError)aTranspStatus;

@end


@implementation MainViewController

@synthesize flipsidePopoverController = _flipsidePopoverController;
@synthesize targetStatusLabel = _targetStatusLabel;
@synthesize globalStatusLabel = _globalStatusLabel;
@synthesize startStopSyncButton = _startStopSyncButton;
@synthesize syncActivityIndicator = _syncActivityIndicator;



- (void)didReceiveMemoryWarning
{
  [super didReceiveMemoryWarning];
  // Release any cached data, images, etc that aren't in use.
}


#pragma mark - View lifecycle

- (void)viewDidLoad
{
  [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
  // - install observer for sync progress
  [[NSNotificationCenter defaultCenter]
    addObserver:self
    selector:@selector(updateProgressInfo:)
    name:@"SyncMLClientProgress"
    object:nil
  ];    
}


- (void)viewDidUnload
{
  [self setTargetStatusLabel:nil];
  [self setGlobalStatusLabel:nil];
  [self setStartStopSyncButton:nil];
  [self setSyncActivityIndicator:nil];
  [syncmlClient release]; syncmlClient = nil;
  [super viewDidUnload];
  // Release any retained subviews of the main view.
  // e.g. self.myOutlet = nil;
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
}


- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
  // update the sync static status
  [self updateStaticStatus];
}


- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];
}


- (void)viewDidDisappear:(BOOL)animated
{
	[super viewDidDisappear:animated];
}


- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
  // Return YES for supported orientations
  if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
    return (interfaceOrientation != UIInterfaceOrientationPortraitUpsideDown);
  } else {
    return YES;
  }
}

#pragma mark - Flipside View Controller

- (void)flipsideViewControllerDidFinish:(FlipsideViewController *)controller
{
  if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
    [self dismissModalViewControllerAnimated:YES];
  } else {
    [self.flipsidePopoverController dismissPopoverAnimated:YES];
  }
}


- (void)dealloc
{
  [_flipsidePopoverController release];
  [_syncActivityIndicator release];
  [super dealloc];
}


- (IBAction)showInfo:(id)sender
{
  if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
    FlipsideViewController *controller = [[[FlipsideViewController alloc] initWithNibName:@"FlipsideViewController" bundle:nil] autorelease];
    controller.delegate = self;
    // pass it the SyncMLClient object
    controller.syncmlClient = self.syncmlClient;

    controller.modalTransitionStyle = UIModalTransitionStyleFlipHorizontal;
    [self presentModalViewController:controller animated:YES];
  } else {
    if (!self.flipsidePopoverController) {
      FlipsideViewController *controller = [[[FlipsideViewController alloc] initWithNibName:@"FlipsideViewController" bundle:nil] autorelease];
      controller.delegate = self;
      // pass it the SyncMLClient object
      controller.syncmlClient = self.syncmlClient;
      
      self.flipsidePopoverController = [[[UIPopoverController alloc] initWithContentViewController:controller] autorelease];
    }
    if ([self.flipsidePopoverController isPopoverVisible]) {
      [self.flipsidePopoverController dismissPopoverAnimated:YES];
    } else {
      [self.flipsidePopoverController presentPopoverFromBarButtonItem:sender permittedArrowDirections:UIPopoverArrowDirectionAny animated:YES];
    }
  }
}

#pragma mark - SyncML client


// On-demand creation of SyncML client object
- (SyncMLClient *)syncmlClient
{
  if (syncmlClient==nil) {
    // create syncML client
    syncmlClient = [[SyncMLClient alloc] initWithClientConfigFile:
      [[NSBundle mainBundle] pathForResource:@"ios_syncclient_app_sample" ofType:@"xml"]
    ];
    // make main view controller the delegate
    syncmlClient.delegate = self;
  }
  return syncmlClient;
}



- (IBAction)startStopSync:(id)sender
{
  // if not running - start sync
  // if running - suspend sync
  // if suspending - abort sync
  if (!self.syncmlClient.isRunning) {
    // not running - start sync
    // - turn on activity indicator
    [self.syncActivityIndicator startAnimating];
    // - convert start button into suspend button
    [self.startStopSyncButton setTitle:@"Suspend sync" forState:UIControlStateNormal];
    // - initiate sync
    [self.syncmlClient startWithProfileID:PROFILEID_FIRST inBackgroundThread:NO];
  }
  else {
    // already running
    if (!self.syncmlClient.isSuspending) {
      // not yet suspending, try suspending
      [self.syncmlClient suspend];
      // - convert suspend button into abort button
      [self.startStopSyncButton setTitle:@"Abort sync" forState:UIControlStateNormal];
    }
    else {
      // already suspending, abort now
      [self.syncmlClient stop];
    }
  }
  
}


#pragma mark - SyncMLClient delegate methods


- (void)actionSheet:(UIActionSheet *)actionSheet clickedButtonAtIndex:(NSInteger)buttonIndex
{
  // fatal error, end app process
  exit(1);
}



// SyncMLClient delegate method: fatal error occurred
- (void)fatalSyncMLError:(NSString *)aErrorMsg syncMLClient:(SyncMLClient *)aSyncMLClient
{

  UIActionSheet *alert = [[UIActionSheet alloc]
    initWithTitle:[NSString stringWithFormat:
      @"Fatal error in SyncMLClient: %@",
      aErrorMsg
    ]
    delegate:(id<UIActionSheetDelegate>)self
    cancelButtonTitle:@"Quit"
    destructiveButtonTitle:nil
    otherButtonTitles:nil
  ];
  [alert showInView:self.view];
  [alert release];
}


// SyncMLClient delegate method: opportunity to define additional config vars before XML loading
- (void)willInitSyncMLEngine:(SyncEngine *)aSyncEngine
{
  // add setting of SyncML engine configuration variables here.
  // These will be set before the XML config is loaded
  TSyError sta;
  SettingsKey *configVarsKey = [aSyncEngine newOpenKeyByPath:"/configvars" withMode:0 err:&sta];
  if (sta==LOCERR_OK) {
    #if DEBUG
    // In debug builds, direct XML configuration errors to console (stdout)
    [configVarsKey setStringValueByName:"conferrpath" toValue:@"console"];
    #endif
    /* %%% enable these if you want a custom model or hardcoded URL
    // custom devInf model string
    [configVarsKey setStringValueByName:"custmodel" toValue:@"" CUSTOM_DEVINF_MODEL];    
    // custom predefined (fixed) URL
    [configVarsKey setStringValueByName:"serverurl" toValue:@"" CUSTOM_SERVER_URL];
    */
  }
  // done with config vars
  [configVarsKey release];
  // if debug build, create the log subdirectory in the sandbox' tmp/, such that
  // the syncml engine will write detailed HTML logs. The engine checks the presence of
  // the log directory and disables logging if it is not present.
  // Note: this has to be in sync with the definition of "logpath" in the XML config 
  #if DEBUG
  [[NSFileManager defaultManager] createDirectoryAtPath:[NSHomeDirectory() stringByAppendingPathComponent:@"tmp/sysynclogs"] withIntermediateDirectories:NO attributes:nil error:NULL];
  #endif
}

 
// SyncMLClient delegate method: opportunity to create a default profile, return nil if none created
- (SettingsKey *)newDefaultSyncMLProfileIn:(SettingsKey *)aProfilesKey
{
  SettingsKey *profileKey = nil;
  /*
  // create new empty profile
  profileKey = [aProfilesKey newOpenSubKeyByID:KEYVAL_ID_NEW withMode:0 err:&lastStatus];
  if (profileKey) {
    // created, now set some values
    [profileKey setStringValueByName:"serverURI" toValue:@"http://www.sampleserver.com/syncml"
    [profileKey setStringValueByName:"serverUser" toValue:@"user"
    [profileKey setStringValueByName:"serverPassword" toValue:@"supersecretpassword"
    // .... more setup
  }
  */
  return profileKey;
}



// SyncMLClient delegate method: sync has completed
- (void)completedSyncMLClientSync:(SyncMLClient *)aSyncMLClient
{
  // Sync done
  // - turn off activity indicator
  [self.syncActivityIndicator stopAnimating];
  // - button ready for starting next sync
  [self.startStopSyncButton setTitle:@"Start Sync" forState:UIControlStateNormal];  
}


#pragma mark - SyncML progress and status display



// update static (non-syncing) status display
- (void)updateStaticStatus
{
  TSyError sta;
  
  // use global status label for "Last Sync" title
  self.globalStatusLabel.text = @"Last Synchronisation:";
  // show the time of last sync of the datastore with ID SYNCML_TARGET_DBID
  // we need to open the current profile, and find the target with ID SYNCML_TARGET_DBID
  // (this is the <dbtypeid> of the  datastore as configured in the xml config file)
  
  SettingsKey *profileKey = [self.syncmlClient newProfileKeyForID:PROFILEID_FIRST];
  if (profileKey) {
    // successfully opened current profile key
    // - open the targets container (a "target" represents settings for one datastore)
    SettingsKey *targetsKey = [profileKey newOpenKeyByPath:"/targets" withMode:0 err:&sta];
    if (sta==LOCERR_OK) {
      // successfully opened targets container
      // - now open the target we are interested in, which is that for the address book
      //   by its ID 1001
      SettingsKey *targetKey = [targetsKey newOpenSubKeyByID:SYNCML_TARGET_DBID withMode:0 err:&sta];
      if (sta==LOCERR_OK) {
        // get the date of last sync
        NSDate *lastSync = [targetKey dateValueByName:"lastSync"];
        // show formatted
        NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
        [formatter setDateStyle:NSDateFormatterMediumStyle];
        [formatter setTimeStyle:NSDateFormatterShortStyle];
        self.targetStatusLabel.text =
        (lastSync==nil ? @"Never" : [formatter stringFromDate:lastSync]);
        [formatter release];
        // close the target key
        [targetKey release];
      }
      // close the targets container key
      [targetsKey release];
    }
    // close the current profile
    [profileKey release];
  }
}



- (void)updateProgressInfo:(NSNotification *)aNotification
{
  // get textual message for error, nil if nothing to display for this event
  NSString *msg = [[self class] stringWithEventText:aNotification.userInfo andTranspStatus:syncmlClient.lastTransportStatus];
  // if we got a message, show it
  if (msg) {
    // targetID determines which datastore this event is related to
    // - 0 means global event (no particular datastore)
    // - other values correspond with the <dbtypeid> of the datastore as configured in the
    //   xml config file. For this sample implementation, we assume only one datastore
    //   and show all datastore related info in targetStatusLabel
    if ([[aNotification.userInfo objectForKey:@"targetID"] intValue]!=0) {
      // event specifically related to a particular datastore, show it in the targetStatusLabel
      self.targetStatusLabel.text = msg;
    }
    else {
      // otherwise, assume the event is global
      self.globalStatusLabel.text = msg;
    }
  }  
}



// error code as string
// See SyncML DS specifications for Error codes 100..599 and syerror.h file for Synthesis SyncML
// engine related error codes or refer to the "Error Codes" chapter in the SDK manual
+ (NSString *)stringWithErrorText:(TSyError)aSta
{
  switch (aSta) {
    case LOCERR_OK            : return @"";  // ok
    case 406                  : return @"Feature not supported";
    case 401                  :
    case 401+LOCAL_STATUS_CODE:
    case 407                  :
    case 407+LOCAL_STATUS_CODE:
    case DB_Forbidden         : return @"Access denied";
    case DB_NotFound          : return @"Server database not found";
    case DB_NotFound+
           LOCAL_STATUS_CODE  : return @"Local database not found";
    case DB_NotAllowed        : return @"Not allowed";
    case DB_Error             : return @"Server Database error";
    case DB_Error+
           LOCAL_STATUS_CODE  : return @"Local Database error";
    case DB_Full              :
    case DB_Full+
         LOCAL_STATUS_CODE    : return @"Datastore capacity exceeded";
    case LOCERR_BADCONTENT    :
    case LOCERR_PROCESSMSG    : return @"Invalid data from server (wrong URL?)";
    case LOCERR_NOCFG         : return @"Configuration missing or no datastore enabled";
    case LOCERR_EXPIRED       : return @"License or demo period expired";
    case LOCERR_USERABORT     : return @"Aborted by user";
    case LOCERR_BADREG        : return @"License not valid here";
    case LOCERR_LIMITED       : return @"Limited trial version";
    case LOCERR_INCOMPLETE    : return @"Sync incomplete";
    case LOCERR_OUTOFMEM      : return @"Out of memory";
    case LOCERR_TOONEW        : return @"License too old for this version of the software";
    case LOCERR_WRONGPROD     : return @"License installed is not valid for this product";
    case LOCERR_USERSUSPEND   : return @"Suspended by User";
    case LOCERR_LOCDBNOTRDY   : return @"Local datastore not ready";
    case LOCERR_TRANSPFAIL    : return @"Network error - check internet connection";
    case LOCERR_CERT_INVALID  : return @"Invalid SSL Server certificate";
    case LOCERR_CERT_EXPIRED  : return @"Expired SSL Server certificate";
    case LOCERR_BADURL        : return @"Bad URL - probably http/https missing";
    case LOCERR_SRVNOTFOUND   : return @"Server not found - check URL";
    default                   : return [NSString stringWithFormat:@"Error %hd",aSta];
  }
}


+ (NSString *)stringWithEventText:(NSDictionary *)aProgressInfoDict andTranspStatus:(TSyError)aTranspStatus
{
  NSString *msg=nil;
  TSyError sta=LOCERR_OK;
  // get values from dict
  int eventtype = [[aProgressInfoDict valueForKey:@"eventtype"] intValue];
  int extra1 = [[aProgressInfoDict valueForKey:@"extra1"] intValue];
  int extra2 = [[aProgressInfoDict valueForKey:@"extra2"] intValue];
  int extra3 = [[aProgressInfoDict valueForKey:@"extra3"] intValue];

  switch (eventtype) {
    // transport-related
    case PEV_SENDSTART      : msg = @"Sending..."; break;
    case PEV_SENDEND        : msg = @"Waiting..."; break;
    case PEV_RECVSTART      : msg = @"Receiving..."; break;
    case PEV_RECVEND        : msg = @"Processing..."; break;
    case PEV_SESSIONSTART   : msg = @"Starting..."; break;
    case PEV_SUSPENDING     : msg = @"Suspending..."; break;
    case PEV_SESSIONEND     :
    case PEV_SYNCEND        :
      // session ended, probably with error in extra
      sta = extra1;
      if (sta==LOCERR_OK)
        msg = @"Sync successfully completed";
      else {
        if (sta==LOCERR_TRANSPFAIL)
          sta = aTranspStatus; // use probably more detailed transport failure status
        msg = [NSString stringWithFormat:@"Sync failed: %@",[MainViewController stringWithErrorText:sta]];
      }
      break;
    // datastore-related
    case PEV_PREPARING      :
      msg = [NSString stringWithFormat:@"Checking %d/%d",extra1,extra2]; // preparing (e.g. preflight in some clients), extra1=progress, extra2=total
      break;
    case PEV_DELETING       :
      msg = [NSString stringWithFormat:@"Deleting %d/%d",extra1,extra2]; // deleting (zapping datastore), extra1=progress, extra2=total
      break;
    case PEV_ALERTED        :
      // datastore alerted (extra1=0 for normal, 1 for slow, 2 for first time slow, extra2=1 for resumed session)
      if (extra2==1)
        msg = @"Resumed ";
      else
        msg = @"";
      switch (extra1) {
        case 0 : msg = [msg stringByAppendingString:@"Normal Sync"]; break;
        case 1 : msg = [msg stringByAppendingString:@"Slow Sync"]; break;
        case 2 : msg = [msg stringByAppendingString:@"First Time Slow Sync"]; break;
      }
      break;
    case PEV_ITEMRECEIVED   :
      msg = [NSString stringWithFormat:@"Received %d/%d",extra1,extra2]; // item received, extra1=current item count, extra2=number of expected changes (if >= 0)
      break;
    case PEV_ITEMSENT       :
      msg = [NSString stringWithFormat:@"Sent %d/%d",extra1,extra2]; // item sent, extra1=current item count, extra2=number of expected items to be sent (if >=0)
      break;
    case PEV_ITEMPROCESSED  :
      msg = [NSString stringWithFormat:@"Added %d, Updated %d, Deleted %d",extra1,extra2,extra3]; // item processed
      break;
  }
  return msg;
}


@end
