//
//  ZKeyChainWrapper.m
//
//  Created by Lukas Zeller on 2011/08/16.
//  Copyright 2011 plan44.ch. All rights reserved.
//

#import "ZKeyChainWrapper.h"

@implementation ZKeyChainWrapper

static ZKeyChainWrapper *sharedKeyChainWrapper = nil;

NSString* const kZKeyChainWrapperErrorDomain = @"ch.plan44.ZKeyChainWrapper";


- (id)initWithServicePrefix:(NSString *)aServicePrefix
{
  if ((self = [super init])) {
    servicePrefix = [aServicePrefix retain];
  }
  return self;
}

- (void)dealloc
{
  [servicePrefix release];
  [super dealloc];
}


+ (ZKeyChainWrapper *)sharedKeyChainWrapper
{
  if (!sharedKeyChainWrapper) {
    sharedKeyChainWrapper = [[self alloc] initWithServicePrefix:
      [[[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleIdentifier"] stringByAppendingString:@"."]
    ];
  }
  return sharedKeyChainWrapper;
}


// Most useful info about keychain on iOS so far:
// http://useyourloaf.com/blog/2010/4/28/keychain-duplicate-item-when-adding-password.html
// Main point is that Apple sample is wrong - what constitues the unique key is account & service, not AttrGeneric


- (NSMutableDictionary *)keychainQueryForService:(NSString *)service account:(NSString *)account
{
  NSString *uniqueService = [servicePrefix stringByAppendingString:service];
  NSMutableDictionary *query = [NSMutableDictionary dictionaryWithObjectsAndKeys:
    (id)kSecClassGenericPassword, (id)kSecClass, // class is "generic password"    
    [uniqueService dataUsingEncoding:NSUTF8StringEncoding], (id)kSecAttrGeneric, // use service as generic attr as well, must be NSData!
    account, (id)kSecAttrAccount,
    uniqueService, (id)kSecAttrService,
    nil
  ];
  return query;
}



- (NSString *)passwordOrEmptyForService:(NSString *)service account:(NSString *)account
{
  NSString *result = [self passwordForService:service account:account error:NULL];
  if (!result) result = @"";
  return result;
}


- (NSString *)passwordForService:(NSString *)service account:(NSString *)account error:(NSError **)error
{
  OSStatus status = kZKeyChainWrapperErrorBadArguments;
  NSString *result = nil;
  if ([service length]>0 && [account length]>0) {
    CFDataRef passwordData = NULL;
    // get the basic query data
    NSMutableDictionary *keychainQuery = [self keychainQueryForService:service account:account];
    // add specifics for finding a entry
    [keychainQuery setObject:(id)kCFBooleanTrue forKey:(id)kSecReturnData]; // return password data (rather than all attributes with kSecReturnAttributes=YES)
    [keychainQuery setObject:(id)kSecMatchLimitOne forKey:(id)kSecMatchLimit]; // only one in case of multiples
    // issue query
    status = SecItemCopyMatching(
      (CFDictionaryRef)keychainQuery,
      (CFTypeRef *)&passwordData
    );
    if (status==noErr && [(NSData *)passwordData length]>0) {
      // we got a password
      result = [[[NSString alloc]
        initWithData:(NSData *)passwordData
        encoding:NSUTF8StringEncoding
      ] autorelease];
    }
    if (passwordData!=NULL) {
      CFRelease(passwordData);
    }
  }
  if (status!=noErr && error!=NULL) {
    *error = [NSError errorWithDomain:kZKeyChainWrapperErrorDomain code:status userInfo:nil];
  }
  return result;
}



- (BOOL)removePasswordForService:(NSString *)service account:(NSString *)account error:(NSError **)error
{
  OSStatus status = kZKeyChainWrapperErrorBadArguments;
  if ([service length]>0 && [account length]>0) {
    NSMutableDictionary *keychainQuery = [self keychainQueryForService:service account:account];
    status = SecItemDelete((CFDictionaryRef)keychainQuery);
  }
  if (status!=noErr && error!=NULL) {
    *error = [NSError errorWithDomain:kZKeyChainWrapperErrorDomain code:status userInfo:nil];
  }
  return status==noErr;
}



- (BOOL)setPassword:(NSString *)password forService:(NSString *)service account:(NSString *)account error:(NSError **)error
{
  OSStatus status = kZKeyChainWrapperErrorBadArguments;
  if ([service length]>0 && [account length]>0) {
    // remove old version first
    [self removePasswordForService:service account:account error:nil];
    // now add new version
    if ([password length]>0) {
      NSMutableDictionary *keychainQuery = [self keychainQueryForService:service account:account];
      NSData *passwordData = [password dataUsingEncoding:NSUTF8StringEncoding];
      [keychainQuery setObject:passwordData forKey:(id)kSecValueData];
      status = SecItemAdd((CFDictionaryRef)keychainQuery, NULL);
    }
  }
  if (status!=noErr && error!=NULL) {
    *error = [NSError errorWithDomain:kZKeyChainWrapperErrorDomain code:status userInfo:nil];
  }
  return status==noErr;
}


@end