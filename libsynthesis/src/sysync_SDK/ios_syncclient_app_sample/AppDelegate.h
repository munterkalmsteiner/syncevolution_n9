//
//  AppDelegate.h
//  ios_syncclient_app_sample
//
//  Created by Lukas Zeller on 2011/09/16.
//  Copyright (c) 2011 plan44.ch. All rights reserved.
//

#import <UIKit/UIKit.h>



// This defines the datatype ("SyncML target") to be used in the app.
// This simple sample app just synchronizes a single "target", but libsynthesis
// supports synchronizing more than one datatype/target in one sync session
// (needs more elaborate UI to allow configuring multiple target DBs and
// providing status display for different data types)
// 
// Note: - For the commercial plugins accessing the iOS native PIM databases, the sample configuration
//         uses the following IDs:
//         ID 1001 = iOS Address book (using ABAdressbook API)
//         ID 1003 = iOS Calendar (using EkEventKit API)
//       - The opensource sample app which stores data in TAB separated text files uses:      
//         ID 2001 = text file based contacts
//         ID 2002 = text file based calendar

#define SYNCML_TARGET_DBID 2001


@class MainViewController;

@interface AppDelegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow *window;

@property (strong, nonatomic) MainViewController *mainViewController;

@end
