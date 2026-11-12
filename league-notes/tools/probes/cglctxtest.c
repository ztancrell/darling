// A process with no window asks for OpenGL contexts, as Chromium's GPU helper does:
// 3.2 core, 4.1 core and legacy, each made current, then asks the driver who it is.
// Pass: all three contexts exist and a renderer name comes back.
#include <OpenGL/OpenGL.h>
#include <stdio.h>
#include <string.h>
extern const unsigned char *glGetString(unsigned int name);
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02

static int try(const char *label, int profile)
{
	CGLPixelFormatAttribute attrs[8] = { kCGLPFAAccelerated, kCGLPFAAllowOfflineRenderers, 0 };
	if (profile) { attrs[2] = 99 /* kCGLPFAOpenGLProfile */; attrs[3] = profile; attrs[4] = 0; }
	CGLPixelFormatObj format = NULL; GLint count = 0; CGLContextObj context = NULL;
	CGLError e1 = CGLChoosePixelFormat(attrs, &format, &count);
	CGLError e2 = format ? CGLCreateContext(format, NULL, &context) : -1;
	CGLError e3 = context ? CGLSetCurrentContext(context) : -1;
	const char *renderer = e3 == 0 ? (const char *)glGetString(GL_RENDERER) : NULL;
	const char *version = e3 == 0 ? (const char *)glGetString(GL_VERSION) : NULL;
	printf("%-10s choose %d create %d current %d | %s | %s\n", label, e1, e2, e3, renderer ? renderer : "-", version ? version : "-");
	if (context) { CGLSetCurrentContext(NULL); CGLDestroyContext(context); }
	return e1 == 0 && e2 == 0 && e3 == 0 && renderer && *renderer;
}

int main(void)
{
	int ok = try("3.2 core", 0x3200) & try("4.1 core", 0x4100) & try("legacy", 0);
	puts(ok ? "RESULT: PASS" : "RESULT: FAIL");
	return !ok;
}
