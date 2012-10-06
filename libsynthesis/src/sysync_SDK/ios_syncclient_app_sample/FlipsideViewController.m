//
//  FlipsideViewController.m
//  ios_syncclient_app_sample
//
//  Created by Lukas Zeller on 2011/09/16.
//  Copyright (c) 2011 plan44.ch. All rights reserved.
//

#import "FlipsideViewController.h"

#import "ZKeyChainWrapper.h"

#import "AppDelegate.h"

@implementation FlipsideViewController

@synthesize delegate = _delegate;

@synthesize syncmlClient = _syncmlClient;

@synthesize serverURLfield;
@synthesize serverUserField;
@synthesize serverPasswordField;
@synthesize datastorePathField;
@synthesize datastoreSyncMode;

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
  self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
  if (self) {
    self.contentSizeForViewInPopover = CGSizeMake(320.0, 480.0);
  }
  return self;
}

							
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
  // - initialize picker contents array with the 6 sync modes available
  syncModes = [[NSArray alloc] initWithObjects:
    @"Normal", @"Slow", @"Update device", @"Reload Device", @"Update Server", @"Reload Server", nil
  ];
}


- (void)viewDidUnload
{
  [syncModes release]; syncModes = nil;
  [self setServerURLfield:nil];
  [self setServerUserField:nil];
  [self setServerPasswordField:nil];
  [self setDatastorePathField:nil];
  [self setDatastoreSyncMode:nil];
  [super viewDidUnload];
}



- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
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


- (void)dealloc
{
  [serverURLfield release];
  [serverUserField release];
  [serverPasswordField release];
  [datastorePathField release];
  [datastoreSyncMode release];
  [super dealloc];
}



#pragma mark - datasource and delegate methods for sync mode picker view


- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView
{
  return 1; // only one component (the sync mode string)
}


- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)component
{
  return [syncModes count];
}


- (NSString *)pickerView:(UIPickerView *)pickerView titleForRow:(NSInteger)row forComponent:(NSInteger)component
{
  return [syncModes objectAtIndex:row];
}


#pragma mark - text field delegates

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
  // make sure keyboard gets dismissed
  [textField performSelector:@selector(resignFirstResponder) withObject:nil afterDelay:0];
  // otherwise process the return key normally
  return YES;
}


//- (BOOL)textFieldShouldEndEditing:(UITextField *)textField
//{
//  return YES;
//}


#pragma mark - Actions


- (IBAction)done:(id)sender
{
  [self.delegate flipsideViewControllerDidFinish:self];
}


#pragma mark - loading and saving SyncML client settings when flip side appears or disappears, resp.

- (void)viewWillAppear:(BOOL)animated
{
  // load settings from SyncML engine into controls
  TSyError sta;

  // get the first profile's settings. PROFILEID_FIRST_OR_DEFAULT causes a default profile
  // to be created if none already exists
  SettingsKey *profileKey = [self.syncmlClient newProfileKeyForID:PROFILEID_FIRST_OR_DEFAULT];
  if (profileKey) {
    // successfully opened profile key. Now we can get the profile-level settings
    // - Server URL
    self.serverURLfield.text = [profileKey stringValueByName:"serverURI"];
    // - user name
    self.serverUserField.text = [profileKey stringValueByName:"serverUser"];
    // - password. Note that the password can be stored in the profile itself, but this is not recommended
    //   for security reasons (it is stored only obfuscated, not encrypted).
    //   Therefore, we store the password in the keychain, and the SyncMLClient object fetches it
    //   from the keychain when needed while running a session.
    self.serverPasswordField.text  =[[ZKeyChainWrapper sharedKeyChainWrapper]
      passwordForService:[NSString stringWithFormat:@"SyncMLProfile.%d",[profileKey keyID]]
      account:@"SyncMLPassword"
      error:NULL
    ];
    // Now get the per-datastore settings. In this sample, we only have one single datastore.
    // The datastores are identified by their <dbtypeid>, which is defined in the the
    // ios_syncclient_app_sample.xml config file.
    // For this app, the actual dbtypeid numeric value is defined as SYNCML_TARGET_DBID in AppDelegate.h    
    // - first open the targets container (a "target" represents settings for one datastore)
    SettingsKey *targetsKey = [profileKey newOpenKeyByPath:"/targets" withMode:0 err:&sta];
    if (targetsKey) {
      // successfully opened targets container
      // - now open the target we are interested in, by ID (<dbtypeid> in the configuration XML file)
      //   Note: See definition of SYNCML_TARGET_DBID in AppDelegate.h for explanation of standard
      //         IDs to use with sample configurations as provided in libsynthesis and plan44.ch
      //         SDKs.      
      SettingsKey *targetKey = [targetsKey newOpenSubKeyByID:SYNCML_TARGET_DBID withMode:0 err:&sta];
      if (targetKey) {
        // successfully opened targets container key. Now we can get the datastore level settings
        // - the server path for the contacts (defaults to "contacts")
        self.datastorePathField.text = [targetKey stringValueByName:"remotepath"];
        // - the sync mode. This consists of the basic mode (twoway/fromserver/fromclient) plus
        //   a flag with differentiates between slow or normal variant of the basic mode.
        //
        //   Basic mode     Slow      end user selection
        //   ------------   ------    -------------------
        //   two way        No        Normal
        //   two way        Yes       Slow
        //   from server    No        Update device
        //   from server    Yes       Reload device
        //   from client    No        Update server
        //   from client    Yes       Reload server
        //
        // - basic mode
        short syncmode = [targetKey intValueByName:"syncmode"]; // sync mode: 0=twoway, 1=from server only, 2=from client only
        // - slow sync flag
        BOOL slowsync = [targetKey intValueByName:"forceslow"]; // YES if user wants to force slow-sync (=reload for one-way mode)
        // - set current selection in picker
        [self.datastoreSyncMode selectRow:syncmode*2 + (slowsync ? 1 : 0) inComponent:0 animated:NO];
        // close the target key
        [targetKey release];
      }
      // close the targets container key
      [targetsKey release];
    }
    // close the current profile
    [profileKey release];
  }
  [super viewWillAppear:animated];
}


- (void)viewWillDisappear:(BOOL)animated
{
  // save settings from controls into SyncML engine
  TSyError sta;

  // get the first profile's settings. PROFILEID_FIRST_OR_DEFAULT causes a default profile
  // to be created if none already exists
  SettingsKey *profileKey = [self.syncmlClient newProfileKeyForID:PROFILEID_FIRST_OR_DEFAULT];
  if (profileKey) {
    // successfully opened current profile key. Now we can store the profile-level settings
    // - Server URL
    [profileKey setStringValueByName:"serverURI" toValue:self.serverURLfield.text];
    // - user name
    [profileKey setStringValueByName:"serverUser" toValue:self.serverUserField.text];
    // - password. Note that the password can be stored in the profile itself, but this is not recommended
    //   for security reasons (it is stored only obfuscated, not encrypted).
    //   Therefore, we store the password in the keychain, and the SyncMLClient object fetches it
    //   from the keychain when needed while running a session.
    [[ZKeyChainWrapper sharedKeyChainWrapper]
      setPassword:self.serverPasswordField.text
      forService:[NSString stringWithFormat:@"SyncMLProfile.%d",[profileKey keyID]]
      account:@"SyncMLPassword"
      error:NULL
    ];
    // Now save the per-datastore settings. In this sample, we only have one single datastore.
    // The datastores are identified by their <dbtypeid>, which is defined in the the
    // ios_syncclient_app_sample.xml config file.
    // For this app, the actual dbtypeid numeric value is defined as SYNCML_TARGET_DBID in AppDelegate.h
    // - first open the targets container (a "target" represents settings for one datastore)
    SettingsKey *targetsKey = [profileKey newOpenKeyByPath:"/targets" withMode:0 err:&sta];
    if (sta==LOCERR_OK) {
      // successfully opened targets container
      // - now open the target we are interested in, by ID (<dbtypeid> in the configuration XML file)
      //   Note: See definition of SYNCML_TARGET_DBID in AppDelegate.h for explanation of standard
      //         IDs to use with sample configurations as provided in libsynthesis and plan44.ch
      //         SDKs.      
      SettingsKey *targetKey = [targetsKey newOpenSubKeyByID:SYNCML_TARGET_DBID withMode:0 err:&sta];
      if (sta==LOCERR_OK) {
        // successfully opened targets container key. Now we can save the datastore level settings
        // - always enable sync with our one and only datastore. For SyncML clients supporting multiple datastores,
        //   the enable flag should be presented as a switch to the user to allow synchronizing just
        //   some of the available datastores.
        [targetKey setIntValueByName:"enabled" toValue:1];
        // - the server path for the contacts (defaults to "contacts")
        [targetKey setStringValueByName:"remotepath" toValue:self.datastorePathField.text];
        // - the sync mode. This consists of the basic mode (twoway/fromserver/fromclient) plus
        //   a flag with differentiates between slow or normal variant of the basic mode.
        //
        //   Basic mode       Slow      end user selection
        //   - -----------    ------    - ----------------
        //   0 two way        No        0 Normal
        //   0 two way        Yes       1 Slow
        //   1 from server    No        2 Update device
        //   1 from server    Yes       3 Reload device
        //   2 from client    No        4 Update server
        //   2 from client    Yes       5 Reload server
        //
        // - get end user selection
        short userSyncMode = [self.datastoreSyncMode selectedRowInComponent:0];
        // - basic mode
        [targetKey setIntValueByName:"syncmode" toValue:userSyncMode/2];
        // - slow sync flag
        [targetKey setIntValueByName:"forceslow" toValue:(userSyncMode & 1)!=0]; // odd user selection number means slow-sync (=reload for one-way mode)
        // close the target key
        [targetKey release];
      }
      // close the targets container key
      [targetsKey release];
    }
    // close the current profile
    [profileKey release];
  }
  [super viewWillDisappear:animated];
}



@end
