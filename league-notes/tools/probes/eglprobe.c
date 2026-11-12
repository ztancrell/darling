// Which library answers when an app loads its own bundled libEGL/libGLESv2 by full path and looks up EGL functions?
// Usage: eglprobe <directory containing libEGL.dylib and libGLESv2.dylib>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <objc/runtime.h>

// Experiment only: Darling's Metal device has no -supportsFamily:. If PROBE_FAMILIES is set (a comma
// list of MTLGPUFamily numbers: Apple 1001.., Mac1 2001, Mac2 2002, Common1-3 3001-3003, Metal3 5001),
// add one to the device class here, inside this probe, that logs each question and says yes to the listed ones.
static signed char probe_supports_family(id self, SEL _cmd, long family) {
	char needle[32], list[256];
	snprintf(needle, sizeof needle, ",%ld,", family);
	snprintf(list, sizeof list, ",%s,", getenv("PROBE_FAMILIES"));
	int yes = strstr(list, needle) != NULL;
	printf("  [asked: supportsFamily %ld -> %s]\n", family, yes ? "yes" : "no"); fflush(stdout);
	return yes;
}

static void *load(const char *dir, const char *name) {
	char path[2048];
	snprintf(path, sizeof path, "%s/%s", dir, name);
	void *h = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
	printf("dlopen %s: %s\n", name, h ? "ok" : dlerror());
	return h;
}

static void where(void *h, const char *sym) {
	Dl_info info;
	void *p = h ? dlsym(h, sym) : NULL;
	if (!p) { printf("  %-28s not found\n", sym); return; }
	printf("  %-28s -> %s\n", sym, dladdr(p, &info) && info.dli_fname ? info.dli_fname : "?");
}

// Milestone 2: draw one frame off-screen and read it back. Clears to a known colour, then draws a
// green triangle over the middle with a shader, and checks one pixel of each.
static void draw(void *egl, void *gles, void *display) {
#define EGLFN(ret, name, ...) ret (*name)(__VA_ARGS__) = (ret (*)(__VA_ARGS__))dlsym(egl, #name)
#define GLFN(ret, name, ...) ret (*name)(__VA_ARGS__) = (ret (*)(__VA_ARGS__))dlsym(gles, #name)
	EGLFN(unsigned, eglChooseConfig, void *, const int *, void **, int, int *);
	EGLFN(void *, eglCreatePbufferSurface, void *, void *, const int *);
	EGLFN(void *, eglCreateContext, void *, void *, void *, const int *);
	EGLFN(unsigned, eglMakeCurrent, void *, void *, void *, void *);
	EGLFN(int, eglGetError, void);
	GLFN(void, glClearColor, float, float, float, float);
	GLFN(void, glClear, unsigned);
	GLFN(void, glFinish, void);
	GLFN(void, glReadPixels, int, int, int, int, unsigned, unsigned, void *);
	GLFN(unsigned, glGetError, void);
	GLFN(const unsigned char *, glGetString, unsigned);
	GLFN(unsigned, glCreateShader, unsigned);
	GLFN(void, glShaderSource, unsigned, int, const char *const *, const int *);
	GLFN(void, glCompileShader, unsigned);
	GLFN(void, glGetShaderiv, unsigned, unsigned, int *);
	GLFN(unsigned, glCreateProgram, void);
	GLFN(void, glAttachShader, unsigned, unsigned);
	GLFN(void, glBindAttribLocation, unsigned, unsigned, const char *);
	GLFN(void, glLinkProgram, unsigned);
	GLFN(void, glGetProgramiv, unsigned, unsigned, int *);
	GLFN(void, glGetProgramInfoLog, unsigned, int, int *, char *);
	GLFN(void, glGetShaderInfoLog, unsigned, int, int *, char *);
	GLFN(void, glUseProgram, unsigned);
	GLFN(void, glVertexAttribPointer, unsigned, int, unsigned, unsigned char, int, const void *);
	GLFN(void, glEnableVertexAttribArray, unsigned);
	GLFN(void, glDrawArrays, unsigned, int, int);
	GLFN(void, glViewport, int, int, int, int);

	const int config_attribs[] = { 0x3033 /*SURFACE_TYPE*/, 0x0001 /*PBUFFER*/, 0x3040 /*RENDERABLE_TYPE*/, 0x0004 /*ES2*/, 0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8, 0x3038 };
	const int surface_attribs[] = { 0x3057 /*WIDTH*/, 64, 0x3056 /*HEIGHT*/, 64, 0x3038 };
	const int context_attribs[] = { 0x3098 /*CONTEXT_CLIENT_VERSION*/, 2, 0x3038 };
	void *config = NULL; int count = 0;
	unsigned char px[4] = { 0 };

	if (!eglChooseConfig(display, config_attribs, &config, 1, &count) || count < 1) { printf("  draw: no config (error 0x%x)\n", eglGetError()); return; }
	void *surface = eglCreatePbufferSurface(display, config, surface_attribs);
	printf("  off-screen surface: %s (error 0x%x)\n", surface ? "ok" : "FAILED", eglGetError()); fflush(stdout);
	void *context = eglCreateContext(display, config, NULL, context_attribs);
	printf("  context: %s (error 0x%x)\n", context ? "ok" : "FAILED", eglGetError()); fflush(stdout);
	if (!surface || !context) return;
	unsigned current = eglMakeCurrent(display, surface, surface, context);
	printf("  make current: %s (error 0x%x)\n", current ? "ok" : "FAILED", eglGetError()); fflush(stdout);
	if (!current) return;
	printf("  renderer: %s\n  GL version: %s\n", glGetString(0x1F01), glGetString(0x1F02)); fflush(stdout);

	glViewport(0, 0, 64, 64);
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
	glClear(0x4000 /*COLOR_BUFFER_BIT*/);
	glFinish();
	glReadPixels(1, 1, 1, 1, 0x1908 /*RGBA*/, 0x1401 /*UNSIGNED_BYTE*/, px);
	printf("  %s clear to red, read back: %u %u %u %u (GL error 0x%x)\n", (px[0] == 255 && px[1] == 0 && px[2] == 0 && px[3] == 255) ? "ok  " : "FAIL", px[0], px[1], px[2], px[3], glGetError()); fflush(stdout);

	const char *vs = "attribute vec2 p; void main() { gl_Position = vec4(p, 0.0, 1.0); }";
	const char *fs = "precision mediump float; void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }";
	int status = 0;
	unsigned v = glCreateShader(0x8B31), f = glCreateShader(0x8B30), prog = glCreateProgram();
	glShaderSource(v, 1, &vs, NULL); glCompileShader(v); glGetShaderiv(v, 0x8B81, &status);
	printf("  vertex shader compile: %s\n", status ? "ok" : "FAILED"); fflush(stdout);
	glShaderSource(f, 1, &fs, NULL); glCompileShader(f); glGetShaderiv(f, 0x8B81, &status);
	printf("  fragment shader compile: %s\n", status ? "ok" : "FAILED"); fflush(stdout);
	glAttachShader(prog, v); glAttachShader(prog, f); glBindAttribLocation(prog, 0, "p"); glLinkProgram(prog); glGetProgramiv(prog, 0x8B82, &status);
	printf("  program link: %s\n", status ? "ok" : "FAILED"); fflush(stdout);
	if (!status) {
		static char log[8192]; int len = 0;
		glGetProgramInfoLog(prog, sizeof log, &len, log); printf("  program log (%d): %s\n", len, log);
		glGetShaderInfoLog(v, sizeof log, &len, log); if (len) printf("  vertex log: %s\n", log);
		glGetShaderInfoLog(f, sizeof log, &len, log); if (len) printf("  fragment log: %s\n", log);
		fflush(stdout);
		return;
	}
	static const float triangle[] = { -0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f };
	glUseProgram(prog);
	glVertexAttribPointer(0, 2, 0x1406 /*FLOAT*/, 0, 0, triangle);
	glEnableVertexAttribArray(0);
	glDrawArrays(0x0004 /*TRIANGLES*/, 0, 3);
	glFinish();
	unsigned char mid[4] = { 0 }, corner[4] = { 0 };
	glReadPixels(32, 28, 1, 1, 0x1908, 0x1401, mid);
	glReadPixels(1, 1, 1, 1, 0x1908, 0x1401, corner);
	printf("  %s triangle: middle pixel %u %u %u %u (want 0 255 0 255), corner %u %u %u %u (want 255 0 0 255), GL error 0x%x\n", (mid[1] == 255 && mid[0] == 0 && corner[0] == 255 && corner[1] == 0) ? "ok  " : "FAIL", mid[0], mid[1], mid[2], mid[3], corner[0], corner[1], corner[2], corner[3], glGetError());
	fflush(stdout);
}

static void debug_message(unsigned error, const char *command, int type, void *thread, void *object, const char *message) {
	printf("  [ANGLE %s in %s: %s]\n", type == 0x33B9 ? "critical" : type == 0x33BA ? "error" : type == 0x33BB ? "warning" : "info", command ? command : "?", message ? message : "");
	fflush(stdout);
}

int sandbox_init_with_parameters(const char *profile, uint64_t flags, const char *const parameters[], char **errorbuf);

// PROBE_SANDBOX=<profile file>: apply Chromium's GPU-helper profile with the parameters its helper passes
// (same list as sbgputest) before touching EGL, to see what the sandbox does to initialisation.
static void sandbox_first(void) {
	const char *path = getenv("PROBE_SANDBOX");
	if (!path) return;
	static char text[1 << 20]; FILE *f = fopen(path, "r");
	if (!f) { perror(path); exit(2); }
	text[fread(text, 1, sizeof text - 1, f)] = '\0'; fclose(f);
	const char *const params[] = { "BROWSER_PID", "100", "BUNDLE_ID", "com.riotgames.RiotClient", "BUNDLE_PATH", "/Users/Shared/Riot Games/Riot Client.app", "COMPONENT_PATH", "/tmp/c", "CURRENT_PID", "200", "DARWIN_USER_CACHE_DIR", "/private/var/folders/xx/C", "DARWIN_USER_DIR", "/private/var/folders/xx", "DARWIN_USER_TEMP_DIR", "/private/var/folders/xx/T", "DISABLE_METAL_SHADER_CACHE", "FALSE", "DISABLE_SANDBOX_DENIAL_LOGGING", "TRUE", "ENABLE_LOGGING", "FALSE", "EXECUTABLE_PATH", "/Users/Shared/Riot Games/Riot Client.app/Contents/MacOS/Riot Client", "USER_HOMEDIR_AS_LITERAL", "/Users/zpt", "LOG_FILE_PATH", "/tmp/log", "OS_VERSION", "1016", "FILTER_SYSCALLS", "FALSE", "FILTER_SYSCALLS_DEBUG", "FALSE", NULL };
	char *error = NULL;
	int rc = sandbox_init_with_parameters(text, 0, params, &error);
	printf("sandbox: %s%s%s\n", rc == 0 ? "applied" : "FAILED", error ? ": " : "", error ? error : ""); fflush(stdout);
	if (rc != 0) exit(2);
}

int main(int argc, char **argv) {
	if (argc < 2) return 2;
	sandbox_first();
	void *gles = load(argv[1], "libGLESv2.dylib");
	void *egl = load(argv[1], "libEGL.dylib");
	where(egl, "eglGetPlatformDisplay"); where(egl, "eglGetPlatformDisplayEXT"); where(egl, "eglInitialize"); where(egl, "eglQueryString");
	where(gles, "glGetString"); where(gles, "EGL_GetPlatformDisplay");
	if (egl) {
		const char *(*query)(void *, int) = (const char *(*)(void *, int))dlsym(egl, "eglQueryString");
		const char *ext = query ? query(NULL, 0x3055 /* EGL_EXTENSIONS, client extensions */) : NULL;
		printf("client extensions: %s\n", ext ? ext : "(none)");
	}

	// try each ANGLE backend the way Chromium does: get a display, then initialise it
	void *(*get_display)(unsigned, void *, const long *) = (void *(*)(unsigned, void *, const long *))(egl ? dlsym(egl, "eglGetPlatformDisplay") : NULL);
	unsigned (*initialize)(void *, int *, int *) = (unsigned (*)(void *, int *, int *))(egl ? dlsym(egl, "eglInitialize") : NULL);
	int (*get_error)(void) = (int (*)(void))(egl ? dlsym(egl, "eglGetError") : NULL);
	const char *(*query)(void *, int) = (const char *(*)(void *, int))(egl ? dlsym(egl, "eglQueryString") : NULL);
	static const struct { const char *name; long type; long device; } backends[] = {
		{ "default", 0x3206, 0 }, { "metal", 0x3489, 0 }, { "opengl", 0x320D, 0 }, { "vulkan", 0x3450, 0 }, { "swiftshader", 0x3450, 0x3487 },
	};
	// ANGLE reports why initialisation fails through EGL_KHR_debug; listen before touching any display.
	void *(*getProc)(const char *) = (void *(*)(const char *))(egl ? dlsym(egl, "eglGetProcAddress") : NULL);
	int (*debugControl)(void (*)(unsigned, const char *, int, void *, void *, const char *), const long *) = getProc ? (int (*)(void (*)(unsigned, const char *, int, void *, void *, const char *), const long *))getProc("eglDebugMessageControlKHR") : NULL;
	if (debugControl) {
		static const long dbg[] = { 0x33B9 /*EGL_DEBUG_MSG_CRITICAL_KHR*/, 1, 0x33BA /*ERROR*/, 1, 0x33BB /*WARN*/, 1, 0x33BC /*INFO*/, 1, 0x3038 };
		debugControl(debug_message, dbg);
	}
	if (getenv("PROBE_FAMILIES")) {
		Class device_class = objc_getClass("MTLDeviceInternal");
		printf("adding -supportsFamily: to %s for this run: %s\n", device_class ? "MTLDeviceInternal" : "(class not found)", device_class && class_addMethod(device_class, sel_registerName("supportsFamily:"), (IMP)probe_supports_family, "c@:q") ? "ok" : "failed");
	}
	const char *only = argc > 2 ? argv[2] : NULL;
	for (unsigned i = 0; get_display && initialize && get_error && i < sizeof backends / sizeof *backends; i++) {
		if (only && strcmp(only, backends[i].name)) continue;
		long attribs[] = { 0x3203, backends[i].type, backends[i].device ? 0x3209 : 0x3038, backends[i].device, 0x3038 };
		printf("backend %s:\n", backends[i].name); fflush(stdout);
		void *display = get_display(0x3202, NULL, attribs);
		printf("  get display: %s (error 0x%x)\n", display ? "ok" : "FAILED", get_error()); fflush(stdout);
		if (!display) continue;
		int major = 0, minor = 0;
		unsigned ok = initialize(display, &major, &minor);
		printf("  initialize: %s (error 0x%x) EGL %d.%d\n", ok ? "ok" : "FAILED", get_error(), major, minor); fflush(stdout);
		if (ok && query) printf("  vendor: %s | version: %s\n", query(display, 0x3053), query(display, 0x3054));
		if (ok && getenv("PROBE_DRAW")) draw(egl, gles, display);
	}
	return 0;
}
