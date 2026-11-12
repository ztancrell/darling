// The Foundation/CFNetwork calls trustd makes for a CA-issuer fetch, under ARC like trustd, one pool per step.
// Run with NSZombieEnabled=YES: a double release then names its class. Usage: urltest [url]
// Not Foundation.h: it pulls Security.h, whose DERItem typedef clashes in this SDK.
#import <Foundation/NSString.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSURLComponents.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLSession.h>
#import <Foundation/NSUUID.h>
#import <Foundation/NSOperation.h>
#include <stdio.h>
#include <unistd.h>
#define STEP(name, body) do { printf("step %s\n", name); fflush(stdout); @autoreleasepool { body; } printf("  ok\n"); fflush(stdout); } while (0)
int main(int argc, char **argv) {
    NSURL *uri = [NSURL URLWithString:argc > 1 ? @(argv[1]) : @"http://example.com/ca.cer"];
    NSURLSessionConfiguration *config; NSURLSession *session; NSMutableURLRequest *request; NSURLSessionTask *task;
    STEP("host/scheme", (void)uri.host; (void)uri.scheme);
    STEP("components", NSURLComponents *c = [NSURLComponents componentsWithURL:uri resolvingAgainstBaseURL:YES]; (void)[c host]; printf("  URL %s\n", c.URL.absoluteString.UTF8String));
    STEP("request", request = [NSMutableURLRequest requestWithURL:uri]; [request addValue:[[NSUUID UUID] UUIDString] forHTTPHeaderField:@"X-Test"]);
    STEP("header read", printf("  %s\n", [[request valueForHTTPHeaderField:@"X-Test"] UTF8String]));
    STEP("config", config = [NSURLSessionConfiguration ephemeralSessionConfiguration]; config.timeoutIntervalForResource = 10; config.HTTPAdditionalHeaders = @{@"User-Agent": @"urltest"});
    STEP("session", session = [NSURLSession sessionWithConfiguration:config delegate:nil delegateQueue:[[NSOperationQueue alloc] init]]);
    STEP("task", task = [session dataTaskWithRequest:request]; (void)task.originalRequest; [task resume]);
    STEP("wait 5 s", sleep(5); printf("  state %ld\n", (long)task.state));
    return 0;
}
