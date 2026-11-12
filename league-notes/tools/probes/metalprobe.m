// What does Darling's Metal say? Is there a device, what is it called, and what macOS version do apps see?
// (the SDK's full headers do not compile here, so declare just what is used)
#import <objc/NSObject.h>
#include <stdio.h>
typedef struct { long majorVersion, minorVersion, patchVersion; } NSOperatingSystemVersion;
@interface NSString : NSObject
- (const char *) UTF8String;
@end
@interface NSArray : NSObject
- (unsigned long) count;
@end
@interface NSProcessInfo : NSObject
+ (NSProcessInfo *) processInfo;
- (NSOperatingSystemVersion) operatingSystemVersion;
- (NSString *) operatingSystemVersionString;
@end
@protocol MTLDevice
- (NSString *) name;
- (BOOL) isLowPower;
- (BOOL) isHeadless;
- (BOOL) isRemovable;
- (unsigned long long) registryID;
@end
id<MTLDevice> MTLCreateSystemDefaultDevice(void);
NSArray *MTLCopyAllDevices(void);

int main(void) {
	@autoreleasepool {
		NSOperatingSystemVersion v = [[NSProcessInfo processInfo] operatingSystemVersion];
		printf("macOS version apps see: %ld.%ld.%ld (\"%s\")\n", v.majorVersion, v.minorVersion, v.patchVersion, [[[NSProcessInfo processInfo] operatingSystemVersionString] UTF8String]);
		fflush(stdout);
		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		printf("default Metal device: %s\n", device ? [[device name] UTF8String] : "NONE"); fflush(stdout);
		NSArray *all = MTLCopyAllDevices();
		printf("all Metal devices: %lu\n", all ? [all count] : 0UL); fflush(stdout);
		if (device) {
			printf("registry id: 0x%llx\n", [(id)device respondsToSelector: @selector(registryID)] ? [device registryID] : 0ULL);
		}
	}
	return 0;
}
