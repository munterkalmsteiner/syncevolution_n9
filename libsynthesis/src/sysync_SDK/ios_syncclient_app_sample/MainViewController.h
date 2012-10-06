//
//  MainViewController.h
//  ios_syncclient_app_sample
//
//  Created by Lukas Zeller on 2011/09/16.
//  Copyright (c) 2011 plan44.ch. All rights reserved.
//

#import "FlipsideViewController.h"

#import "SyncMLClient.h"

@interface MainViewController : UIViewController <FlipsideViewControllerDelegate, SyncMLClientDelegate>
{
  // syncml client
  SyncMLClient *syncmlClient;
}

@property (strong, nonatomic) UIPopoverController *flipsidePopoverController;

@property (retain, nonatomic) IBOutlet UILabel *targetStatusLabel;
@property (retain, nonatomic) IBOutlet UILabel *globalStatusLabel;
@property (retain, nonatomic) IBOutlet UIButton *startStopSyncButton;
@property (retain, nonatomic) IBOutlet UIActivityIndicatorView *syncActivityIndicator;

@property (readonly, nonatomic) SyncMLClient *syncmlClient;

- (IBAction)showInfo:(id)sender;

- (IBAction)startStopSync:(id)sender;



@end
