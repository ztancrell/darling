// Chromium looks EGL functions up in libGLESv2 first, then libEGL (dlsym on each handle). A handle
// lookup also searches that library's dependencies. Which library answers for each?
// Usage: eglwho <directory containing ANGLE's libEGL.dylib and libGLESv2.dylib>
#include <dlfcn.h>
#include <stdio.h>
static void who(const char *label, void *h, const char *sym) {
	Dl_info info; void *p = dlsym(h, sym);
	printf("  %-10s %-16s -> %s\n", label, sym, !p ? "not found" : dladdr(p, &info) && info.dli_fname ? info.dli_fname : "?");
}
int main(int argc, char **argv) {
	char path[2048];
	snprintf(path, sizeof path, "%s/libGLESv2.dylib", argv[1]); void *gles = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
	snprintf(path, sizeof path, "%s/libEGL.dylib", argv[1]); void *egl = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
	if (!gles || !egl) { printf("load failed: %s\n", dlerror()); return 2; }
	who("libGLESv2", gles, "eglInitialize"); who("libGLESv2", gles, "eglGetDisplay"); who("libGLESv2", gles, "EGL_Initialize");
	who("libEGL", egl, "eglInitialize"); who("libEGL", egl, "eglGetDisplay");
	who("default", RTLD_DEFAULT, "eglInitialize");
	return 0;
}
