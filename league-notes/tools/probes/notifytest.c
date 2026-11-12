// An observer that is deallocated without removing itself must not be messaged
// (macOS 10.11 and later). Build: build.sh notifytest -x objective-c -framework Foundation
// (the SDK's full Foundation headers do not compile here, so declare just what is used)
#import <objc/NSObject.h>
@class NSString, NSNotification;
@interface NSNotificationCenter : NSObject
+ (NSNotificationCenter *) defaultCenter;
- (void) addObserver: (id) o selector: (SEL) s name: (NSString *) n object: (id) obj;
- (void) removeObserver: (id) o;
- (void) postNotificationName: (NSString *) n object: (id) obj;
@end
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <objc/runtime.h>

static int calls;

@interface Obs : NSObject
@end
@implementation Obs
- (void) note: (NSNotification *) n {
    calls++;
}
@end

int main(void) {
    NSNotificationCenter *c = [NSNotificationCenter defaultCenter];
    Obs *dead = [Obs new], *live = [Obs new], *removed = [Obs new];
    size_t size = class_getInstanceSize([Obs class]);
    [c addObserver: dead selector: @selector(note:) name: @"T" object: nil];
    [c addObserver: live selector: @selector(note:) name: @"T" object: nil];
    [c addObserver: removed selector: @selector(note:) name: @"T" object: nil];
    [c postNotificationName: @"T" object: nil];
    fprintf(stderr, "all alive: %d calls (want 3)\n", calls);
    if (calls != 3) return 1;

    [c removeObserver: removed];
    [dead release];
    // Reuse the freed block so a stale pointer has no class, as in the app's crash.
    void *junk[64];
    for (int i = 0; i < 64; i++) junk[i] = calloc(1, size);
    calls = 0;
    [c postNotificationName: @"T" object: nil];
    for (int i = 0; i < 64; i++) free(junk[i]);
    fprintf(stderr, "one dead, one removed: %d calls (want 1)\n", calls);
    fprintf(stderr, calls == 1 ? "PASS\n" : "FAIL\n");
    return calls != 1;
}
