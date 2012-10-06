//
//  ZWebRequest.m
//
//  Created by Lukas Zeller on 2011/02/03.
//  Copyright (c) 2011 by plan44.ch
//

#import "ZWebRequest.h"

@interface ZWebRequest ()
// private methods
- (void)cleanRequest;
- (void)releaseAnswerBuffer;
@end


// private functions
void responseClientCallBack(
   CFReadStreamRef aReadStream,
   CFStreamEventType aEventType,
   void *aClientCallBackInfo
);




@implementation ZWebRequest

@synthesize httpUser;
@synthesize httpPassword;
@synthesize delegate;
@synthesize requestData;
@synthesize contentType;
@synthesize ignoreSSLErrors;
@synthesize useSysProxy;
// answer
@synthesize answerData;
@synthesize answerSize, answerMaxSize;
@synthesize answerBufferP, answerBufferSize;


- (id)init
{
	if ((self = [super init])) {
  	httpUser = nil;
    httpPassword = nil;
    requestData = nil;
    contentType = nil;
    ignoreSSLErrors = NO;
    useSysProxy = YES;
    httpCredentials =  NULL;
    httpAuthentication = NULL;
    webRequest = NULL;
    responseStream = NULL;
    isSSL = NO;
		// answer
		answerData = nil;
		answerMaxSize = 0; // no answer expected
		answerBufferP = NULL;
		answerBufferSize = 0;
		answerSize = 0;
		answerBufferExternal = NO;
  }
  return self;
}


- (void)dealloc
{
	[self releaseAnswerBuffer];
	[self cleanRequest];
  [httpUser release], httpUser = nil;
  [httpPassword release], httpPassword = nil;
	[requestData release], requestData = nil;
	[contentType release], contentType = nil;
  delegate = nil;
  [super dealloc];
}


- (void)cleanRequest
{
  if (responseStream) { CFRelease(responseStream); responseStream = NULL; }
  if (httpAuthentication) { CFRelease(httpAuthentication); httpAuthentication = NULL; }
  if (httpCredentials) { CFRelease(httpCredentials); httpCredentials = NULL; }
  if (webRequest) { CFRelease(webRequest); webRequest = NULL; }
}




- (void)setRequestCString:(const char *)aCString;
{
	if (!aCString) aCString = "";
  self.requestData = [[NSData alloc] initWithBytes:(const void *)aCString length:strlen(aCString)];
}


- (void)setRequestString:(NSString *)aString
{
	if (!aString) aString = @"";
	[self setRequestCString:[aString UTF8String]];
}


- (void)setRequestBuffer:(UInt8 *)aBuf withSize:(CFIndex)aSize
{
  // contents of buffer is owned by caller, but NSData is not
  self.requestData = [[NSData alloc] initWithBytesNoCopy:aBuf length:aSize];
}



- (void)releaseAnswer
{
	// forget the current answer.
	// - if we have a buffer, it will be overwritten with the next answer
	// - if we had a NSData, it will be released
	answerSize = 0;
	if (answerData) {
		[answerData release];
		answerData = nil;
	}
}


- (void)releaseAnswerBuffer
{
	[self releaseAnswer];
	if (!answerBufferExternal) {
		// we own the buffer, release memory
		if (answerBufferP)
			free(answerBufferP);
	}
	answerBufferP = NULL;
	answerBufferSize = 0;
	answerBufferExternal = NO;
}


// set the maximum answer size. If not using an external buffer, this must always be set
- (void)setAnswerMaxSize:(CFIndex)aSize
{
	[self releaseAnswer];
	if (answerBufferP && aSize>answerBufferSize) {
		aSize = answerBufferSize; // limit to buffer size
	}
	answerMaxSize = aSize;
}


// pass an external buffer for the answer, also sets max answer size to buffer size
- (void)setAnswerBuffer:(UInt8 *)aBuf withSize:(CFIndex)aSize
{
	[self releaseAnswerBuffer];
	answerBufferP = aBuf;
	answerBufferSize = aSize;
	answerBufferExternal = YES;
	[self setAnswerMaxSize:aSize]; // by default, answer size is limited by the buffer size
}




- (BOOL)sendRequestToURL:(NSString *)aURL withHTTPMethod:(NSString *)aMethod
{
	[self cleanRequest];
  // - make URL (and trim URL in case it has whitespace or LF in front or at end)
  CFURLRef myURL = CFURLCreateWithString(kCFAllocatorDefault,
    (CFStringRef)[aURL stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]],
    NULL
  );
  // - check for SSL
	NSString *scheme = (NSString *)CFURLCopyScheme(myURL);
  isSSL = [scheme isEqualToString:@"https"];
  [scheme release];
  // - create the request
  webRequest =
    CFHTTPMessageCreateRequest(
      kCFAllocatorDefault,
      (CFStringRef)aMethod,
      myURL,
      kCFHTTPVersion1_1
    );
  CFRelease(myURL);
  if (requestData) CFHTTPMessageSetBody(webRequest, (CFDataRef)requestData); // attach the data
  // - set the content type header
  CFHTTPMessageSetHeaderFieldValue(webRequest, CFSTR("Content-Type"), (CFStringRef)contentType);
  // send request now
  return [self sendRequest];
}


- (BOOL)sendDataToWebDAVURL:(NSString *)aURL
{
	return [self sendRequestToURL:aURL withHTTPMethod:@"PUT"];
}


- (void)completedWebRequestWithStatus:(CFIndex)aHTTPStatus
{
	// Base class action: inform delegate
  // Note: subclasses will probably override this
	if (delegate && [delegate respondsToSelector:@selector(completedWebRequest:withStatus:)]) {
  	[delegate completedWebRequest:self withStatus:aHTTPStatus];
  }
}


- (void)failedWebRequestWithReadStream:(CFReadStreamRef)aReadStream
{
	// Base class action: inform delegate
  // Note: subclasses will probably override this
	if (delegate && [delegate respondsToSelector:@selector(failedWebRequest:withReadStream:)]) {
  	[delegate failedWebRequest:self withReadStream:aReadStream];
  }
}





- (void)responseClientCallBackWithStream:(CFReadStreamRef)aReadStream andEvent:(CFStreamEventType)aEventType
{
  const CFIndex chunkSize = 1000;
	const CFIndex initialDataCapactiy = chunkSize*3;
  char buf[chunkSize];
  CFIndex bytesRead;
  // check the status
  CFHTTPMessageRef myResponse = (CFHTTPMessageRef)CFReadStreamCopyProperty(aReadStream, kCFStreamPropertyHTTPResponseHeader);
  // if we did't get that far (e.g. server not reachable), we'll get nil for myResponse
  CFIndex myHTTPStatus = ZWEBREQUEST_STATUS_NONE;
  if (myResponse) {
    myHTTPStatus = CFHTTPMessageGetResponseStatusCode(myResponse);
    if (myHTTPStatus==401) {
      // missing or wrong authentication
      if (httpAuthentication) {
        // wrong auth - fail with myHTTPStatus
        goto requestDone;
      }
      // create appropriate auth from response (challenge)
      httpAuthentication = CFHTTPAuthenticationCreateFromResponse(NULL, myResponse);
      // no need to read more from this stream
      CFReadStreamClose(aReadStream);
      CFRelease(myResponse); myResponse = nil;
      // retry with new auth
      [self sendRequest];
      return;
    }
  }
  // no auth failure - normally evaluate events
	switch (aEventType) {
    case kCFStreamEventHasBytesAvailable:
			// three modes: read to buffer passed from the outside, read into NSData, or discard answer completely
			if (answerBufferP && answerSize<answerMaxSize) {
				// We have a buffer and still some room there
				// - read as much as possible directly into buffer
				bytesRead = CFReadStreamRead(
					aReadStream,
					answerBufferP+answerSize,
					answerBufferSize-answerSize
				);
        answerSize+=bytesRead;
			}
			else {
				// no buffer or buffer already full
				// - read into temp buffer
				bytesRead = CFReadStreamRead(
					aReadStream,
					(UInt8 *)buf,
					(CFIndex)chunkSize
				);
				// if we have no buffer, but a maxsize, we want to fill a NSData
				// Note: answerMaxSize is not exactly met, we might get up to one chunkSize more into the NSMutableData
				if (bytesRead>0 && !answerBufferP && answerMaxSize>0 && answerSize<answerMaxSize) {
					if (!answerData) {
						// create a answer data
						answerData = [[NSMutableData alloc] initWithCapacity:initialDataCapactiy];
					}
					// append to data
					[answerData appendBytes:buf length:bytesRead];
          answerSize+=bytesRead;
				}
			}
      if (bytesRead<0) {
        goto requestDone; // no data - send done (with myHTTPStatus)
      }
      break;
    case kCFStreamEventErrorOccurred:
      // no real HTTP status, even in case we got one (unlikely)
      myHTTPStatus = ZWEBREQUEST_STATUS_NETWORK_ERROR; // network error
      // let derived class or delegate handle this
      [self failedWebRequestWithReadStream:aReadStream];
    case kCFStreamEventEndEncountered:
    	[self cleanRequest];
    	goto requestDone;
  } // switch
  // continue
	// - forget response
	if (myResponse) CFRelease(myResponse);
	return;
requestDone:
	// no more reading
  CFReadStreamClose(aReadStream);
  // let derived class or delegate handle this (always called, failed or not)
  [self completedWebRequestWithStatus:myHTTPStatus];
	// forget response
	if (myResponse) CFRelease(myResponse);
  // also forget the response stream we kept so far to be able to abort the request
  if (responseStream) { CFRelease(responseStream); responseStream = NULL; }
  // now I can be released (from my self-retain in sendRequest, which kept me around
  // until now in case caller is not interested in the result at all (e.g. push only)
  [self autorelease];
} // responseClientCallBackWithStream


// the callback from the read stream scheduled with the run loop
void responseClientCallBack(
   CFReadStreamRef aReadStream,
   CFStreamEventType aEventType,
   void *aClientCallBackInfo
) {
  [(ZWebRequest *)aClientCallBackInfo responseClientCallBackWithStream:aReadStream andEvent:aEventType];
} // responseClientCallBack




- (BOOL)sendRequest
{
	CFStreamError streamErr;

	// forget previous answer, if any
	[self releaseAnswer];
	// start new request
	if (httpAuthentication) {
  	// we should set user/pw
		if (CFHTTPAuthenticationRequiresUserNameAndPassword(httpAuthentication)) {
    	CFHTTPMessageApplyCredentials(
      	webRequest,
        httpAuthentication,
        httpUser ? (CFStringRef)httpUser : CFSTR(""),
      	httpPassword ? (CFStringRef)httpPassword : CFSTR(""),
        &streamErr
      );
		}
  }
  // - create a read stream for receiving the answer
  if (responseStream) CFRelease(responseStream);
  responseStream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, webRequest);
  // - setup proxy usage
  if (useSysProxy) {
    // Make sure we use proxy when one is defined
    #if TARGET_IPHONE_SIMULATOR==0
    // iPhoneOS has new function CFNetworkCopySystemProxySettings() for getting proxy settings
    CFDictionaryRef proxyDict = CFNetworkCopySystemProxySettings();
    #else
    // simulator needs Mac OS X version
    //%%%CFDictionaryRef proxyDict = SCDynamicStoreCopyProxies(NULL);
    CFDictionaryRef proxyDict = nil;
    #endif
    if (proxyDict) {
      CFReadStreamSetProperty(responseStream, kCFStreamPropertyHTTPProxy, proxyDict);
      CFRelease(proxyDict);
    }
  }
  // - check SSL ignore options if needed
  if (isSSL) {
    NSDictionary *sslSettings = nil;
    // prepare SSL options
    if (ignoreSSLErrors) {
      // for relaxed "ignore SSL errors" mode
      sslSettings = [NSDictionary dictionaryWithObjectsAndKeys:(id)
        /* %%%%Does not help with the cases we had, but forcing SSLv3 seems to help. So we try that
        // request SSLv3..TLS1.0 security - using kCFStreamSocketSecurityLevelNegotiatedSSL will try TLS1.2,
        // which some servers cannot gracefully fall back from
        // See Apple TN2287 - http://developer.apple.com/library/ios/#technotes/tn2287/_index.html#//apple_ref/doc/uid/DTS40011309)
        //CFSTR("kCFStreamSocketSecurityLevelTLSv1_0SSLv3"), (NSString *)kCFStreamSSLLevel,
        //kCFStreamSocketSecurityLevelNegotiatedSSL, (NSString *)kCFStreamSSLLevel,  // request *some* security (default is NO security, which is not correct for https)
        */
        // forcing SSL v3 seems to be a good compromise
        kCFStreamSocketSecurityLevelSSLv3, (NSString *)kCFStreamSSLLevel,  // request SSLv3 security as a hopefully interoperable compromise (default is NO security, which is not correct for https)
        kCFBooleanTrue, kCFStreamSSLAllowsExpiredCertificates,
        kCFBooleanTrue, kCFStreamSSLAllowsExpiredRoots,
        kCFBooleanFalse, kCFStreamSSLValidatesCertificateChain,
        kCFNull, kCFStreamSSLPeerName,
        kCFBooleanTrue, kCFStreamSSLAllowsAnyRoot,
        nil
      ];
    }
    else {
      // for normal operation
      sslSettings = [NSDictionary dictionaryWithObjectsAndKeys:(id)
        /* %%%%Does not help with the cases we had, so we leave normal operation with kCFStreamSocketSecurityLevelNegotiatedSSL
        // request SSLv3..TLS1.0 security - using kCFStreamSocketSecurityLevelNegotiatedSSL will try TLS1.2,
        // which some servers cannot gracefully fall back from
        // See Apple TN2287 - http://developer.apple.com/library/ios/#technotes/tn2287/_index.html#//apple_ref/doc/uid/DTS40011309)
        // CFSTR("kCFStreamSocketSecurityLevelTLSv1_0SSLv3"), (NSString *)kCFStreamSSLLevel,
        */
        kCFStreamSocketSecurityLevelNegotiatedSSL, (NSString *)kCFStreamSSLLevel,  // request *some* security (default is NO security, which is not correct for https)
        nil
      ];
    }
    if (sslSettings) {
      // set the options
      CFReadStreamSetProperty(responseStream, kCFStreamPropertySSLSettings, (CFDictionaryRef)sslSettings);
    }
  }
  // prepare for answer
  CFStreamClientContext clientContext;
  clientContext.version = 0; // Version number of the structure. Must be 0.
  clientContext.info = self; // callback to WebRequest
  clientContext.retain = NULL;
  clientContext.release = NULL;
  clientContext.copyDescription = NULL;
  // set the client
  if (CFReadStreamSetClient(
    responseStream,
    kCFStreamEventHasBytesAvailable | kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered | kCFStreamEventOpenCompleted,
    &responseClientCallBack,
    &clientContext
  )) {
  	// retain myself during sending process, in case owner does not want to keep me around anyway
    [self retain];
    // schedule with run loop
    CFReadStreamScheduleWithRunLoop(
      responseStream,
      CFRunLoopGetCurrent(),
      kCFRunLoopCommonModes // in common modes  %%% kCFRunLoopDefaultMode // in default mode
    );
  }
  // send the request (Note: Must be done AFTER scheduling the client with the runloop!)
  CFReadStreamOpen(responseStream);
  // Note: keep responseStream retained so we can abort it. We'll release it in the callback
  // ok
  return YES;
} // sendRequest



- (void)abortRequest
{
  // stop waiting for data
  if (responseStream) {
    // terminate reading stream
    CFReadStreamClose(responseStream);
    // unschedule from run loop
    CFReadStreamUnscheduleFromRunLoop(
      responseStream,
      CFRunLoopGetCurrent(),
      kCFRunLoopCommonModes // in common modes  %%% kCFRunLoopDefaultMode // in default mode
    );
    // request completed with abort
    [self completedWebRequestWithStatus:ZWEBREQUEST_STATUS_ABORTED];
    // clean up everything
    [self cleanRequest];
  }
}


@end // ZWebRequest
