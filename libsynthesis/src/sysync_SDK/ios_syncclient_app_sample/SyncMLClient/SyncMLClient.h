//
//  SyncMLClient.h
//
//  Created by Lukas Zeller on 2011/02/03.
//  Copyright 2011 plan44.ch. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "SyncEngine.h"
#import "ZWebRequest.h"

@class SyncMLClient;

// Special Profile IDs
#define PROFILEID_FIRST -1 // first profile
#define PROFILEID_FIRST_OR_DEFAULT -2 // first profile, or newly created default profile if no profile already exists
#define PROFILEID_ALL_ENABLED -3 // all profiles enabled for global sync
#define PROFILEID_NONE -999 // no profile

// custom progress events
#define PEV_PROFILEID PEV_CUSTOM_START // extra1=profileID



@protocol SyncMLClientDelegate <ZWebRequestDelegate>
@optional
// initialisation
- (void)fatalSyncMLError:(NSString *)aErrorMsg syncMLClient:(SyncMLClient *)aSyncMLClient; // report fatal syncml engine error
- (void)willInitSyncMLEngine:(SyncEngine *)aSyncEngine; // opportunity to define additional config vars before XML loading
- (SettingsKey *)newDefaultSyncMLProfileIn:(SettingsKey *)aProfilesKey; // opportunity to create a default profile, return nil if none created
// running syncs
- (void)completedSyncMLClientSync:(SyncMLClient *)aSyncMLClient;
@end



@interface SyncMLClient : ZWebRequest
{
  NSString *clientConfigFile;
  SyncEngine *syncEngine;
  TSyError lastStatus; // last status encountered from engine
  TSyError sessionEndStatus; // end of session status
  // transport
  TSyError lastTransportStatus; // last transport status
  // running sync session
  SyncSession *syncSession;
  sInt32 profileSelector;
  uInt16 stepCmd;
  BOOL loggingSession;
  BOOL suspendRequest;
  BOOL suspendRequested;
  BOOL abortRequested;
  BOOL runsInBackgroundThread;
  BOOL localDataChanged;
@private
  // these need to be kept between startDataExchange and startDataWait
  memSize syncmlRequestSize; 
  NSString *syncMLURL; // the SyncML URL
}
@property(readonly) NSString *syncMLURL;
@property(readonly) SyncEngine *syncEngine;
@property(readonly) TSyError lastStatus;
@property(readonly) TSyError lastTransportStatus;
@property(readonly) TSyError sessionEndStatus;
@property(readonly) BOOL isRunning;
@property(readonly) BOOL isSuspending;
@property(readonly) BOOL localDataChanged;
@property(readonly) BOOL loggingSession;

// configure
- (id)initWithClientConfigFile:(NSString *)aClientConfigFile;
- (SettingsKey *)newProfilesKey;
- (SettingsKey *)newProfileKeyForID:(sInt32)aProfileID;
- (SettingsKey *)newDefaultProfileKey;

// run sync
- (BOOL)startWithProfileID:(sInt32)aProfileID inBackgroundThread:(BOOL)aInBackgroundThread;
- (void)suspend;
- (void)stop;

@end // SyncMLClient
