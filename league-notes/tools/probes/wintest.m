// An app's window subclass that replaces the frame/content conversion methods (as Electron's does)
// must not have them called while NSWindow's own initialiser is still running.
// (the SDK's full AppKit headers do not compile here, so declare just what is used)
#import <objc/NSObject.h>
#include <stdio.h>
typedef struct { double x, y; } NSPoint;
typedef struct { double width, height; } NSSize;
typedef struct { NSPoint origin; NSSize size; } NSRect;
@interface NSApplication : NSObject
+ (NSApplication *) sharedApplication;
@end
@interface NSWindow : NSObject
- (instancetype) initWithContentRect: (NSRect) r styleMask: (unsigned long) m backing: (unsigned long) b defer: (BOOL) d;
- (NSRect) contentRectForFrameRect: (NSRect) r;
- (NSRect) frameRectForContentRect: (NSRect) r;
- (NSRect) frame;
@end

static int calls;
@interface TestWindow : NSWindow
@end
@implementation TestWindow
- (NSRect) contentRectForFrameRect: (NSRect) r { calls++; return [super contentRectForFrameRect: r]; }
- (NSRect) frameRectForContentRect: (NSRect) r { calls++; return [super frameRectForContentRect: r]; }
@end

int main(void) {
	[NSApplication sharedApplication];
	NSRect content = {{100, 100}, {400, 300}};
	TestWindow *w = [[TestWindow alloc] initWithContentRect: content styleMask: 15 backing: 2 defer: YES];
	int during = calls;
	NSRect f = [w frame];
	NSRect c = [w contentRectForFrameRect: f];
	printf("frame %.0fx%.0f, content of that frame %.0fx%.0f\n", f.size.width, f.size.height, c.size.width, c.size.height);
	int ok = during == 0 && c.size.width == 400 && c.size.height == 300 && calls == 1;
	printf("%s replacement methods called %d time(s) during setup; requested content size %s\n", during == 0 ? "ok  " : "FAIL", during, (c.size.width == 400 && c.size.height == 300) ? "kept" : "NOT kept");
	printf(ok ? "RESULT: PASS\n" : "RESULT: FAIL\n");
	return !ok;
}
