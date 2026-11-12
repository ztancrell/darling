// The app pattern: remember the current appearance, switch to another, switch back.
// The remembered object must still be alive after the switch.
// (the SDK's full AppKit headers do not compile here, so declare just what is used)
#import <objc/NSObject.h>
@class NSString;
extern NSString *const NSAppearanceNameAqua;
@interface NSAppearance : NSObject
+ (NSAppearance *) currentAppearance;
+ (void) setCurrentAppearance: (NSAppearance *) appearance;
+ (NSAppearance *) appearanceNamed: (NSString *) name;
@end
#include <stdio.h>
id objc_storeWeak(id *location, id obj);
id objc_loadWeak(id *location);

int main(void) {
	int failed = 0;
	@autoreleasepool {
		NSAppearance *saved = [NSAppearance currentAppearance];
		id weak = nil;
		objc_storeWeak(&weak, saved);
		NSAppearance *other = [NSAppearance appearanceNamed: NSAppearanceNameAqua];
		[NSAppearance setCurrentAppearance: other];
		BOOL alive = objc_loadWeak(&weak) != nil;
		printf("%s remembered appearance is still alive after switching away\n", alive ? "ok  " : "FAIL");
		if (!alive) failed = 1;
		else {
			[NSAppearance setCurrentAppearance: saved];
			BOOL back = [NSAppearance currentAppearance] == saved;
			printf("%s switching back restores it\n", back ? "ok  " : "FAIL");
			if (!back) failed = 1;
		}
		objc_storeWeak(&weak, nil);
	}
	printf(failed ? "RESULT: FAIL\n" : "RESULT: PASS\n");
	return failed;
}
