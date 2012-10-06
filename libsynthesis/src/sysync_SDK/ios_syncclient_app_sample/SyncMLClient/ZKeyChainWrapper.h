//
//  ZKeyChainWrapper.h
//
//  Created by Lukas Zeller on 2011/08/16.
//  Copyright 2011 plan44.ch. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <Security/SecItem.h>

#if TARGET_IPHONE_SIMULATOR && __IPHONE_OS_VERSION_MAX_ALLOWED < 30000
#error "requires iOS 3.0 or later for testing on simulator (but works on iPhoneOS 2.x devices)"
#endif

enum {
  kZKeyChainWrapperErrorBadArguments = -1001,
  kZKeyChainWrapperErrorNoPassword = -1002
};

extern NSString* const kZKeyChainWrapperErrorDomain;

@interface ZKeyChainWrapper : NSObject
{
  NSString *servicePrefix;
}

/// returns a shared instance of the keyChain wrapper using the bundle identifier
/// (plus a dot) as a prefix to service and account strings
+ (ZKeyChainWrapper *)sharedKeyChainWrapper; 

/// create a keychain wrapper with a custom service prefix (can be nil to have none)
- (id)initWithServicePrefix:(NSString *)aServicePrefix;

/// access the passwords
- (NSString *)passwordOrEmptyForService:(NSString *)service account:(NSString *)account;
- (NSString *)passwordForService:(NSString *)service account:(NSString *)account error:(NSError **)error;
- (BOOL)removePasswordForService:(NSString *)service account:(NSString *)account error:(NSError **)error;
- (BOOL)setPassword:(NSString *)password forService:(NSString *)service account:(NSString *)account error:(NSError **)error;


@end
