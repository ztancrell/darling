// Unrecognized selector: raises by default; with DARLING_TOLERATE_UNRECOGNIZED_SELECTORS it is
// logged once and answers zero.
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSException.h>
#include <stdio.h>
@interface NSObject (Missing)
- (id)darlingNoSuchMethod;
+ (long)darlingNoSuchClassMethod:(int)x;
@end
int main(void) {
    {
        NSObject *o = [[NSObject alloc] init];
        @try {
            id a = [o darlingNoSuchMethod];
            id b = [o darlingNoSuchMethod];
            long c = [NSObject darlingNoSuchClassMethod:7];
            printf("no exception: instance=%p again=%p class=%ld\n", (void *)a, (void *)b, c);
        } @catch (NSException *e) {
            printf("exception raised: %s\n", [[e reason] UTF8String]);
        }
    }
    return 0;
}
