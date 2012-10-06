//
//  FlipsideViewController.h
//  ios_syncclient_app_sample
//
//  Created by Lukas Zeller on 2011/09/16.
//  Copyright (c) 2011 plan44.ch. All rights reserved.
//

#import <UIKit/UIKit.h>

#import "SyncMLClient.h"

@class FlipsideViewController;

@protocol FlipsideViewControllerDelegate
- (void)flipsideViewControllerDidFinish:(FlipsideViewController *)controller;
@end

@interface FlipsideViewController : UIViewController
{
  NSArray *syncModes;
}

@property (assign, nonatomic) SyncMLClient *syncmlClient;


@property (assign, nonatomic) IBOutlet id <FlipsideViewControllerDelegate> delegate;

@property (retain, nonatomic) IBOutlet UITextField *serverURLfield;
@property (retain, nonatomic) IBOutlet UITextField *serverUserField;
@property (retain, nonatomic) IBOutlet UITextField *serverPasswordField;

@property (retain, nonatomic) IBOutlet UITextField *datastorePathField;
@property (retain, nonatomic) IBOutlet UIPickerView *datastoreSyncMode;

- (IBAction)done:(id)sender;

@end
