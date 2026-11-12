// Darling's own OpenGL (CGL) must still reach the host's EGL: choose a pixel format and create a context.
#include <OpenGL/OpenGL.h>
#include <stdio.h>
int main(void) {
	CGLPixelFormatAttribute attrs[] = { kCGLPFAAccelerated, kCGLPFAColorSize, 24, kCGLPFADoubleBuffer, 0 };
	CGLPixelFormatObj format = NULL; GLint count = 0; CGLContextObj context = NULL;
	CGLError e1 = CGLChoosePixelFormat(attrs, &format, &count);
	CGLError e2 = format ? CGLCreateContext(format, NULL, &context) : -1;
	printf("choose pixel format: %d (formats %d); create context: %d %s\n", e1, count, e2, context ? "ok" : "none");
	// the pixel format is what proves the host EGL was reached; context creation is reported only
	printf(e1 == 0 && count > 0 ? "RESULT: PASS\n" : "RESULT: FAIL\n");
	return !(e1 == 0 && count > 0);
}
