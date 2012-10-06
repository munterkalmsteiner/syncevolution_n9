//
//  ZWebRequest.m
//
//  Created by Lukas Zeller on 2011/02/03.
//  Copyright (c) 2011 by plan44.ch
//

#import <Foundation/Foundation.h>
#import <CFNetwork/CFNetwork.h>

// Some pseudo HTTP-errors
#define ZWEBREQUEST_STATUS_NONE 0  // no HTTP status received, but otherwise request ok
#define ZWEBREQUEST_STATUS_ABORTED 29999  // aborted request
#define ZWEBREQUEST_STATUS_NETWORK_ERROR 29998 // network level error occurred


@class ZWebRequest;

@protocol ZWebRequestDelegate <NSObject>
@optional
- (void)completedWebRequest:(ZWebRequest *)aWebRequest withStatus:(UInt16)aHTTPStatus;
- (void)failedWebRequest:(ZWebRequest *)aWebRequest withReadStream:(CFReadStreamRef)aReadStreamRef;
@end


@interface ZWebRequest : NSObject {
	// properties
  NSString *httpUser;
  NSString *httpPassword;
  NSString *contentType;
  NSData *requestData;
  id delegate;
  BOOL ignoreSSLErrors;
  BOOL useSysProxy;
	// internal
  CFHTTPMessageRef webRequest;  
  CFHTTPAuthenticationRef httpAuthentication;
  CFDictionaryRef httpCredentials;
  CFReadStreamRef responseStream;
  BOOL isSSL;
	// answer
	NSMutableData *answerData;
	BOOL answerBufferExternal;
	UInt8 *answerBufferP;
	CFIndex answerBufferSize;
	CFIndex answerSize;
	CFIndex answerMaxSize;
}
@property(nonatomic,retain) NSString *httpUser;
@property(nonatomic,retain) NSString *httpPassword;
@property(nonatomic,assign) id delegate;
@property(nonatomic,retain) NSData *requestData;
@property(nonatomic,retain) NSString *contentType;
@property(nonatomic,assign) BOOL ignoreSSLErrors;
@property(nonatomic,assign) BOOL useSysProxy;
// answer
@property(nonatomic,readonly) CFIndex answerMaxSize;
@property(nonatomic,readonly) CFIndex answerSize;
@property(nonatomic,readonly) NSMutableData *answerData;
@property(nonatomic,readonly) UInt8 *answerBufferP;
@property(nonatomic,readonly) CFIndex answerBufferSize;

- (id)init;

- (void)setRequestString:(NSString *)aString;
- (void)setRequestCString:(const char *)aCString;
- (void)setRequestBuffer:(UInt8 *)aBuf withSize:(CFIndex)aSize;

- (void)setAnswerBuffer:(UInt8 *)aBuf withSize:(CFIndex)aSize;
- (void)setAnswerMaxSize:(CFIndex)aSize;

- (BOOL)sendRequestToURL:(NSString *)aURL withHTTPMethod:(NSString *)aMethod;
- (BOOL)sendDataToWebDAVURL:(NSString *)aURL;
- (BOOL)sendRequest;

- (void)abortRequest;

@end // ZWebRequest
