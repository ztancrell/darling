#include <stdlib.h>
#include <string.h>
#include <stdio.h>  // for fprintf(stderr, "unimplemented")

#include <pthread.h>

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLInternal.h>
#include <OpenGL/gl.h>
#include <CoreFoundation/CFDictionary.h>
#include <pthread.h>

// Try to get the right (generic) type definitions.
// In particular, we really want EGLNativeDisplayType to be void *,
// not int as it is if __APPLE__ is defined.
#undef APPLE
#undef __APPLE__

#define __unix__
#define EGL_NO_X11

#include <EGL/egl.h>

#define APPLE
#define __APPLE__

// The host's EGL is loaded privately, not linked. macOS has no system EGL, and an application that
// brings its own (ANGLE, in every Chromium) looks its functions up by name through libraries that
// depend on this framework; linked here, the host's functions answered instead of the application's.
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define HOST_EGL_FUNCTIONS(X) \
    X(eglBindAPI) \
    X(eglChooseConfig) \
    X(eglCreateContext) \
    X(eglCreateWindowSurface) \
    X(eglDestroyContext) \
    X(eglDestroySurface) \
    X(eglGetConfigAttrib) \
    X(eglGetDisplay) \
    X(eglGetPlatformDisplay) \
    X(eglGetProcAddress) \
    X(eglInitialize) \
    X(eglMakeCurrent) \
    X(eglSwapBuffers) \
    X(eglSwapInterval) \
    X(eglTerminate)

static struct {
#define X(name) __typeof__(name) *name;
    HOST_EGL_FUNCTIONS(X)
#undef X
} host_egl_functions;

static void host_egl_load(void) {
    void *library = dlopen("/usr/lib/native/libEGL.dylib", RTLD_LAZY | RTLD_LOCAL);
#define X(name) host_egl_functions.name = library ? dlsym(library, #name) : NULL; \
    if (!host_egl_functions.name) { fprintf(stderr, "OpenGL: the host's EGL has no " #name ": %s\n", dlerror()); abort(); }
    HOST_EGL_FUNCTIONS(X)
#undef X
}

static __typeof__(host_egl_functions) *host_egl(void) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, host_egl_load);
    return &host_egl_functions;
}

#define eglBindAPI host_egl()->eglBindAPI
#define eglChooseConfig host_egl()->eglChooseConfig
#define eglCreateContext host_egl()->eglCreateContext
#define eglCreateWindowSurface host_egl()->eglCreateWindowSurface
#define eglDestroyContext host_egl()->eglDestroyContext
#define eglDestroySurface host_egl()->eglDestroySurface
#define eglGetConfigAttrib host_egl()->eglGetConfigAttrib
#define eglGetDisplay host_egl()->eglGetDisplay
#define eglGetPlatformDisplay host_egl()->eglGetPlatformDisplay
#define eglGetProcAddress host_egl()->eglGetProcAddress
#define eglInitialize host_egl()->eglInitialize
#define eglMakeCurrent host_egl()->eglMakeCurrent
#define eglSwapBuffers host_egl()->eglSwapBuffers
#define eglSwapInterval host_egl()->eglSwapInterval
#define eglTerminate host_egl()->eglTerminate

static EGLDisplay display;

static EGLConfig config;
static int num_config;
static int default_swap_interval = 1;

static EGLint const attribute_list[] = {
    EGL_RED_SIZE, 1,
    EGL_GREEN_SIZE, 1,
    EGL_BLUE_SIZE, 1,
    EGL_NONE
};

struct _CGLDisplay
{
    EGLDisplay display;
    EGLConfig config;
    int num_config;
};

static CFMutableDictionaryRef g_displays;
static pthread_mutex_t g_displaysMutex = PTHREAD_MUTEX_INITIALIZER;

struct _CGLContextObj {
    GLuint retain_count;
    pthread_mutex_t lock;
    EGLContext egl_context;
    EGLSurface egl_surface;
    // EGL has no function for getting the current swap interval,
    // so we need to save the last set value. The default is 1.
    int swap_interval;
};

struct _CGLPixelFormatObj {
    GLuint retain_count;
    CGLPixelFormatAttribute *attributes;
};

static inline int attribute_has_argument(CGLPixelFormatAttribute attr) {
    switch (attr) {
    case kCGLPFAAuxBuffers:
    case kCGLPFAColorSize:
    case kCGLPFAAlphaSize:
    case kCGLPFADepthSize:
    case kCGLPFAStencilSize:
    case kCGLPFAAccumSize:
    case kCGLPFARendererID:
    case kCGLPFADisplayMask:
    case kCGLPFAOpenGLProfile:
        return 1;
    default:
        return 0;
   }
}

__attribute__((constructor))
static void _CGLInitialize(void)
{
    g_displays = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
}

static int attributes_count(const CGLPixelFormatAttribute *attrs) {
    int result;
    for (result = 0; attrs[result] != 0; result++) {
        if (attribute_has_argument(attrs[result])) {
            result++;
        }
    }
    return result;
}

CGLError CGLRegisterNativeDisplay(void *native_display) {

    default_swap_interval = 1;
    display = eglGetDisplay(native_display);

    if (display == EGL_NO_DISPLAY) {
        return kCGLBadConnection;
    }

    eglInitialize(display, NULL, NULL);
    eglChooseConfig(display, attribute_list, &config, 1, &num_config);

    eglBindAPI(EGL_OPENGL_API);

    return kCGLNoError;
}

// Explicit-platform counterpart for backends whose native handles must not be
// interpreted using EGL's process default platform. Keep the X11 entry unchanged.
CGLError CGLRegisterNativeDisplayForPlatform(void *native_display, unsigned int platform) {
    if (!native_display)
        return kCGLBadConnection;
    EGLDisplay candidate = eglGetPlatformDisplay(platform, native_display, NULL);
    if (candidate == EGL_NO_DISPLAY)
        return kCGLBadConnection;
    EGLBoolean initialized = eglInitialize(candidate, NULL, NULL);
    if (!initialized)
        return kCGLBadConnection;
    CGLError error = kCGLBadConnection;
    const EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLint count = 0;
    if (!eglChooseConfig(candidate, attributes, NULL, 0, &count) || count <= 0) {
        error = kCGLBadPixelFormat;
        goto reject;
    }
    EGLConfig *configs = calloc((size_t)count, sizeof(*configs));
    if (!configs) {
        error = kCGLBadAlloc;
        goto reject;
    }
    EGLConfig candidate_config = NULL;
    if (eglChooseConfig(candidate, attributes, configs, count, &count)) {
        for (EGLint i = 0; i < count; ++i) {
            EGLint minimum_interval;
            if (eglGetConfigAttrib(candidate, configs[i], EGL_MIN_SWAP_INTERVAL, &minimum_interval) &&
                minimum_interval == 0) {
                candidate_config = configs[i];
                break;
            }
        }
    }
    free(configs);
    if (!candidate_config) {
        error = kCGLBadPixelFormat;
        goto reject;
    }
    if (!eglBindAPI(EGL_OPENGL_API)) {
        error = kCGLBadState;
        goto reject;
    }
    // Publish only a fully initialized display/config pair. A rejected platform
    // must not corrupt an already working backend.
    display = candidate;
    config = candidate_config;
    num_config = count;
    // Layer animations may keep drawing while their parent is hidden. Waiting
    // for a Wayland frame callback on an unmapped surface can block forever.
    default_swap_interval = 0;
    return kCGLNoError;

reject:
    // A failed probe must not leak a newly initialized display. Never terminate
    // the already-published display when a repeated registration probes it.
    if (initialized && candidate != display)
        eglTerminate(candidate);
    return error;
}

static struct _CGLDisplay* getCGLDisplay(CGSConnectionID cid)
{
    struct _CGLDisplay* rv;

    pthread_mutex_lock(&g_displaysMutex);
    rv = (struct _CGLDisplay*) CFDictionaryGetValue(g_displays, (const void*)(unsigned long) cid);
    pthread_mutex_unlock(&g_displaysMutex);

    if (!rv)
    {
        EGLDisplay disp = eglGetDisplay(_CGSNativeDisplay(cid));
        if (disp == EGL_NO_DISPLAY)
            return NULL;

        rv = (struct _CGLDisplay*) malloc(sizeof(*rv));
        rv->display = disp;

        eglInitialize(rv->display, NULL, NULL);
        eglChooseConfig(rv->display, attribute_list, &rv->config, 1, &rv->num_config);

        eglBindAPI(EGL_OPENGL_API);

        pthread_mutex_lock(&g_displaysMutex);
        CFDictionaryAddValue(g_displays, (const void*)(unsigned long) cid, rv);
        pthread_mutex_unlock(&g_displaysMutex);
    }

    return rv;
}

CGLError CGLSetSurface(CGLContextObj gl, CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid)
{
    struct _CGLDisplay* disp = getCGLDisplay(cid);
    if (!disp)
        return kCGLBadConnection;

    EGLNativeWindowType window;
    if (sid)
        window = (EGLNativeWindowType) _CGSNativeWindowForSurfaceID(cid, wid, sid);
    else
        window = (EGLNativeWindowType) _CGSNativeWindowForID(cid, wid);

    if (!window)
        return kCGLBadWindow;

    gl->egl_surface = eglCreateWindowSurface(disp->display, disp->config, window, NULL);
    if (gl->egl_surface == EGL_NO_SURFACE)
        return kCGLBadState;
    return kCGLNoError;
}

CGLWindowRef CGLGetWindow(void *native_window) {

    EGLNativeWindowType window = (EGLNativeWindowType) native_window;
    EGLSurface surface = eglCreateWindowSurface(display, config, window, NULL);

    if (surface == EGL_NO_SURFACE) {
        return NULL;
    }

    return (CGLWindowRef) surface;
}

CGL_EXPORT void CGLDestroyWindow(CGLWindowRef window) {
    eglDestroySurface(display, (EGLSurface) window);
}

CGL_EXPORT CGLError CGLContextMakeCurrentAndAttachToWindow(CGLContextObj context, CGLWindowRef window) {
    if (!context)
        return kCGLBadContext;
    if (!window)
        return kCGLBadDrawable;
    EGLSurface previous_surface = context->egl_surface;
    context->egl_surface = (EGLSurface) window;
    CGLError error = CGLSetCurrentContext(context);
    if (error != kCGLNoError)
        context->egl_surface = previous_surface;
    return error;
}

static pthread_key_t current_context_key;

static void make_key() {
    pthread_key_create(&current_context_key, NULL);
}

static pthread_key_t get_current_context_key() {
    static pthread_once_t key_once = PTHREAD_ONCE_INIT;
    pthread_once(&key_once, make_key);
    return current_context_key;
}

CGLContextObj CGLGetCurrentContext(void) {
    pthread_key_t key = get_current_context_key();
    return pthread_getspecific(key);
}

CGLError CGLSetCurrentContext(CGLContextObj context) {
    if (context != NULL) {
        EGLSurface surface = context->egl_surface;
        if (!eglMakeCurrent(display, surface, surface, context->egl_context))
            return kCGLBadContext;
        pthread_setspecific(get_current_context_key(), context);
        if (surface != EGL_NO_SURFACE && !eglSwapInterval(display, context->swap_interval))
            return kCGLBadValue;
    } else {
        if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
            return kCGLBadContext;
        pthread_setspecific(get_current_context_key(), NULL);
    }
    return kCGLNoError;
}

CGLError CGLEnable(CGLContextObj ctx, CGLContextEnable pname) {
    return kCGLNoError;
}

CGLError CGLDisable(CGLContextObj ctx, CGLContextEnable pname) {
    return kCGLNoError;
}

CGLError CGLIsEnabled(CGLContextObj ctx, CGLContextEnable pname, GLint *enable) {
    if (enable) *enable = 0;
    return kCGLNoError;
}

const char *CGLErrorString(CGLError error) {
    switch (error) {
    case kCGLNoError: return "no error";
    case kCGLBadAttribute: return "invalid pixel format attribute";
    case kCGLBadProperty: return "invalid renderer property";
    case kCGLBadPixelFormat: return "invalid pixel format";
    case kCGLBadRendererInfo: return "invalid renderer info";
    case kCGLBadContext: return "invalid context";
    case kCGLBadDrawable: return "invalid drawable";
    case kCGLBadDisplay: return "invalid display";
    case kCGLBadState: return "invalid context state";
    case kCGLBadValue: return "invalid numerical value";
    case kCGLBadMatch: return "invalid share context";
    case kCGLBadEnumeration: return "invalid enumerant";
    case kCGLBadOffScreen: return "invalid offscreen drawable";
    case kCGLBadFullScreen: return "invalid fullscreen drawable";
    case kCGLBadWindow: return "invalid window";
    case kCGLBadAddress: return "invalid pointer";
    case kCGLBadCodeModule: return "invalid code module";
    case kCGLBadAlloc: return "memory allocation failure";
    case kCGLBadConnection: return "invalid CoreGraphics connection";
    default: return "unknown error";
    }
}

// Textures that stand for an IOSurface. On macOS such a texture *is* the surface's memory. Here it has
// storage of its own on the host GPU, and what was drawn into it is copied to the surface's memory
// when the context is flushed, which is the point from which other processes may expect to see it.
// ponytail: every surface texture is read back on every flush, a full GPU-to-CPU copy each. Track
// which were drawn to, or share one GPU buffer between the processes, when this shows in profiles.
// ponytail: the copy is one way. A surface's pixels are not uploaded, so sampling a surface that was
// filled by the CPU (a video frame) reads nothing; and texture names are not told apart by share group.
#define SURFACE_TEXTURES_MAX 64
static struct surface_texture {
    GLuint texture;
    GLenum target, format, type;
    GLsizei width, height;
    CFTypeRef surface;
} surface_textures[SURFACE_TEXTURES_MAX];
static pthread_mutex_t surface_textures_lock = PTHREAD_MUTEX_INITIALIZER;

static void surface_texture_forget(struct surface_texture *entry) {
    CFRelease(entry->surface);
    memset(entry, 0, sizeof(*entry));
}

static void surface_textures_read_back(void) {
    static void *(*base_address)(CFTypeRef);
    static size_t (*bytes_per_row)(CFTypeRef);
    GLint read_framebuffer = 0, pack_buffer = 0, row_length = 0;
    GLuint framebuffer = 0;

    pthread_mutex_lock(&surface_textures_lock);
    for (int i = 0; i < SURFACE_TEXTURES_MAX; i++) {
        struct surface_texture *entry = &surface_textures[i];
        if (entry->surface == NULL)
            continue;
        if (!glIsTexture(entry->texture)) {
            surface_texture_forget(entry);
            continue;
        }
        if (framebuffer == 0) {
            // whoever passed a surface has IOSurface.framework loaded; this framework does not link it
            if (!base_address)
                base_address = dlsym(RTLD_DEFAULT, "IOSurfaceGetBaseAddress");
            if (!bytes_per_row)
                bytes_per_row = dlsym(RTLD_DEFAULT, "IOSurfaceGetBytesPerRow");
            if (!base_address || !bytes_per_row)
                break;
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer);
            glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack_buffer);
            glGetIntegerv(GL_PACK_ROW_LENGTH, &row_length);
            glGenFramebuffers(1, &framebuffer);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }
        void *pixels = base_address(entry->surface);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, entry->target, entry->texture, 0);
        if (pixels == NULL || glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            continue;
        // ponytail: assumes four bytes per pixel, the only kind the window side shows
        glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)(bytes_per_row(entry->surface) / 4));
        glReadPixels(0, 0, entry->width, entry->height, entry->format, entry->type, pixels);
    }
    if (framebuffer != 0) {
        glPixelStorei(GL_PACK_ROW_LENGTH, row_length);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pack_buffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read_framebuffer);
        glDeleteFramebuffers(1, &framebuffer);
    }
    pthread_mutex_unlock(&surface_textures_lock);
}

// glFlush and glFinish are this framework's own, in front of the host's that it re-exports, so that
// surface textures reach their surfaces. Libraries/libGL.dylib is a link to this framework because
// ANGLE loads that path and looks the GL functions up in it.
static void host_gl_call(const char *name) {
    static void *library;
    if (!library)
        library = dlopen("/usr/lib/native/libGL.dylib", RTLD_LAZY | RTLD_LOCAL);
    void (*function)(void) = library ? dlsym(library, name) : NULL;
    if (function)
        function();
}

void glFlush(void) {
    if (CGLGetCurrentContext() != NULL)
        surface_textures_read_back();
    host_gl_call("glFlush");
}

void glFinish(void) {
    if (CGLGetCurrentContext() != NULL)
        surface_textures_read_back();
    host_gl_call("glFinish");
}

CGLError CGLTexImageIOSurface2D(CGLContextObj ctx, GLenum target, GLenum internal_format,
                                GLsizei width, GLsizei height, GLenum format, GLenum type,
                                void *ioSurface, GLuint plane)
{
    GLint texture = 0;

    // Gives the texture bound in the current context storage of the surface's size, so that it can be
    // sampled and attached to a framebuffer.
    glTexImage2D(target, 0, internal_format, width, height, 0, format, type, NULL);
    glGetIntegerv(target == GL_TEXTURE_RECTANGLE ? GL_TEXTURE_BINDING_RECTANGLE : GL_TEXTURE_BINDING_2D, &texture);
    if (texture == 0 || ioSurface == NULL)
        return kCGLNoError;

    pthread_mutex_lock(&surface_textures_lock);
    struct surface_texture *entry = NULL;
    for (int i = 0; i < SURFACE_TEXTURES_MAX; i++) {
        struct surface_texture *candidate = &surface_textures[i];
        // a texture stands for one surface, and a surface is drawn through one texture
        if (candidate->surface != NULL && (candidate->texture == (GLuint)texture || candidate->surface == ioSurface))
            surface_texture_forget(candidate);
        if (candidate->surface == NULL && entry == NULL)
            entry = candidate;
    }
    if (entry != NULL)
        *entry = (struct surface_texture){ (GLuint)texture, target, format, type, width, height, CFRetain(ioSurface) };
    pthread_mutex_unlock(&surface_textures_lock);
    return kCGLNoError;
}

CGLError CGLSetFullScreen(CGLContextObj ctx) {
    printf("STUB: CGLSetFullScreen\n");

    return kCGLNoError;
}

CGLError CGLChoosePixelFormat(
    const CGLPixelFormatAttribute *attrs,
    CGLPixelFormatObj *result,
    GLint *number_of_screens
) {
    CGLPixelFormatObj format = malloc(sizeof(struct _CGLPixelFormatObj));
    int count = attributes_count(attrs);

    format->retain_count = 1;
    format->attributes = malloc(sizeof(CGLPixelFormatAttribute) * (count + 1));
    for (int i = 0; i < count; i++) {
        format->attributes[i] = attrs[i];
    }
    format->attributes[count] = 0;

    *result = format;
    *number_of_screens = 1;

    return kCGLNoError;
}

CGLError CGLClearDrawable(CGLContextObj ctx)
{
    printf("STUB: CGLClearDrawable\n");

    return kCGLNoError;
}

CGLError CGLUpdateContext(CGLContextObj ctx)
{
    return kCGLNoError;
}

CGLError CGLDescribePixelFormat(
    CGLPixelFormatObj format,
    GLint sreen_num,
    CGLPixelFormatAttribute attr,
    GLint *value
) {
    for (int i = 0; format->attributes[i] != 0; i++) {
        int has_arg = attribute_has_argument(format->attributes[i]);

        if (format->attributes[i] == attr) {
            if (has_arg) {
                *value = format->attributes[i + 1];
            } else {
                *value = 1;
            }
            return kCGLNoError;
        }

        if (has_arg) {
            i++;
        }
    }

    *value = 0;
    return kCGLNoError;
}

CGLPixelFormatObj CGLRetainPixelFormat(CGLPixelFormatObj format) {
    if (format == NULL) {
        return NULL;
    }

    format->retain_count++;
    return format;
}

void CGLReleasePixelFormat(CGLPixelFormatObj format) {
    if (format == NULL) {
        return;
    }

    format->retain_count--;

    if (format->retain_count == 0) {
        free(format->attributes);
        free(format);
    }
}

CGLError CGLDestroyPixelFormat(CGLPixelFormatObj pixelFormat) {
    CGLReleasePixelFormat(pixelFormat);
    return kCGLNoError;
}

GLuint CGLGetPixelFormatRetainCount(CGLPixelFormatObj pixelFormat) {
    return pixelFormat->retain_count;
}

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

// A process with no window (Chromium's GPU helper, command line tools) never registers a native
// display. It gets the host driver's off-screen display, so contexts are still created on the GPU.
// ponytail: contexts made here do not move to a window display registered later in the same process.
static pthread_once_t offscreen_display_once = PTHREAD_ONCE_INIT;

#define DARLING_EGL_PLATFORM_DEVICE_EXT 0x313F
#define DARLING_EGL_DRM_RENDER_NODE_FILE_EXT 0x3377

// The host GPU that drives the display, as a device display. The driver's own default for a process
// with no window can be a second GPU that shows nothing, and DRI_PRIME=1 only means "the other one",
// which flips the windowed processes onto that GPU instead. EGL_NO_DISPLAY when the host does not
// say which device is the display's; the caller then takes the driver's default.
static EGLDisplay display_gpu_open(void) {
    EGLBoolean (*query_devices)(EGLint, void **, EGLint *) = (void *)eglGetProcAddress("eglQueryDevicesEXT");
    const char *(*query_device_string)(void *, EGLint) = (void *)eglGetProcAddress("eglQueryDeviceStringEXT");
    void *devices[16];
    EGLint device_count = 0;

    if (!query_devices || !query_device_string || !query_devices(16, devices, &device_count))
        return EGL_NO_DISPLAY;
    for (EGLint i = 0; i < device_count; i++) {
        // "/dev/dri/renderD128"; software devices have no node
        const char *node = query_device_string(devices[i], DARLING_EGL_DRM_RENDER_NODE_FILE_EXT);
        const char *name = node ? strrchr(node, '/') : NULL;
        char path[128];
        if (name == NULL)
            continue;
        snprintf(path, sizeof(path), "/Volumes/SystemRoot/sys/class/drm%s/device/boot_vga", name);
        FILE *file = fopen(path, "r");
        if (file == NULL)
            continue;
        int boot_vga = fgetc(file);
        fclose(file);
        if (boot_vga == '1')
            return eglGetPlatformDisplay(DARLING_EGL_PLATFORM_DEVICE_EXT, devices[i], NULL);
    }
    return EGL_NO_DISPLAY;
}

static void offscreen_display_open(void) {
    static const EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig candidate_config = NULL;
    EGLint count = 0;

    if (display != EGL_NO_DISPLAY)
        return;
    // an explicit DRI_PRIME is the user's choice of GPU, and the driver applies it to its default
    EGLDisplay candidate = getenv("DRI_PRIME") ? EGL_NO_DISPLAY : display_gpu_open();
    if (candidate == EGL_NO_DISPLAY)
        candidate = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    if (candidate == EGL_NO_DISPLAY || !eglInitialize(candidate, NULL, NULL))
        return;
    if (!eglChooseConfig(candidate, attributes, &candidate_config, 1, &count) || count < 1) {
        eglTerminate(candidate);
        return;
    }
    display = candidate;
    config = candidate_config;
    num_config = count;
}

// The kCGLPFAOpenGLProfile value of a pixel format, or 0 when it names none (legacy profile).
static GLint pixel_format_profile(CGLPixelFormatObj format) {
    if (format == NULL || format->attributes == NULL)
        return 0;
    for (int i = 0; format->attributes[i] != 0; i++) {
        if (format->attributes[i] == kCGLPFAOpenGLProfile)
            return (GLint)format->attributes[i + 1];
        if (attribute_has_argument(format->attributes[i]))
            i++;
    }
    return 0;
}

CGLError CGLCreateContext(CGLPixelFormatObj pixelFormat, CGLContextObj share, CGLContextObj *resultp) {

    if (resultp == NULL)
        return kCGLBadAddress;
    *resultp = NULL;

    EGLContext egl_share = EGL_NO_CONTEXT;
    if (share != NULL) {
        egl_share = share->egl_context;
    }
    if (display == EGL_NO_DISPLAY)
        pthread_once(&offscreen_display_once, offscreen_display_open);
    // the bound API is per thread, and EGL's default is OpenGL ES
    eglBindAPI(EGL_OPENGL_API);

    GLint profile = pixel_format_profile(pixelFormat);
    const EGLint core_attributes[] = {
        EGL_CONTEXT_MAJOR_VERSION, profile == kCGLOGLPVersion_GL4_Core ? 4 : 3,
        EGL_CONTEXT_MINOR_VERSION, profile == kCGLOGLPVersion_GL4_Core ? 1 : 2,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };
    EGLContext egl_context = eglCreateContext(display, config, egl_share,
        profile >= kCGLOGLPVersion_3_2_Core ? core_attributes : NULL);

    if (egl_context == EGL_NO_CONTEXT) {
        return kCGLBadContext;
    }

    CGLContextObj context = malloc(sizeof(struct _CGLContextObj));

    if (context == NULL) {
        eglDestroyContext(display, egl_context);
        return kCGLBadAlloc;
    }

    context->retain_count = 1;
    pthread_mutex_init(&(context->lock), NULL);
    context->egl_context = egl_context;
    context->egl_surface = NULL;
    context->swap_interval = default_swap_interval;

    *resultp = context;

    return kCGLNoError;
}

CGLContextObj CGLRetainContext(CGLContextObj context) {
    if (context == NULL) {
        return NULL;
    }

    context->retain_count++;
    return context;
}

void CGLReleaseContext(CGLContextObj context) {
    if (context == NULL) {
        return;
    }

    if (CGLGetCurrentContext() == context) {
        // Do not free a context while EGL/TLS still identify it as current.
        // A failed unbind leaves ownership with the caller for a later retry.
        if (CGLSetCurrentContext(NULL) != kCGLNoError)
            return;
    }

    context->retain_count--;

    if (context->retain_count != 0) {
        return;
    }

    pthread_mutex_destroy(&(context->lock));

    eglDestroyContext(display, context->egl_context);

    free(context);
}

GLuint CGLGetContextRetainCount(CGLContextObj context) {
    if (context == NULL) {
        return 0;
    }

    return context->retain_count;
}

CGLError CGLDestroyContext(CGLContextObj context) {
    CGLReleaseContext(context);

    return kCGLNoError;
}

CGLError CGLLockContext(CGLContextObj context) {
    pthread_mutex_lock(&(context->lock));
    return kCGLNoError;
}

CGLError CGLUnlockContext(CGLContextObj context) {
    pthread_mutex_unlock(&(context->lock));
    return kCGLNoError;
}

CGLError CGLFlushDrawable(CGLContextObj context) {
    if (context == NULL)
        return kCGLBadContext;
    if (context->egl_surface == EGL_NO_SURFACE)
        return kCGLBadDrawable;
    return eglSwapBuffers(display, context->egl_surface) ? kCGLNoError : kCGLBadDrawable;
}

CGLError CGLSetParameter(CGLContextObj context, CGLContextParameter parameter, const GLint *value) {
    if (!value)
        return kCGLBadAddress;

    if (parameter == kCGLCPSwapInterval)
    {
        GLint v = *value;
        EGLBoolean success = eglSwapInterval(display, v);
        if (success)
            context->swap_interval = v;
        return success ? kCGLNoError : kCGLBadValue;
    }
    fprintf(stderr, "CGLSetParameter unimplemented for parameter %d\n", parameter);
    return kCGLNoError;
}

CGLError CGLGetParameter(CGLContextObj context, CGLContextParameter parameter, GLint *value) {
    if (!value)
        return kCGLBadAddress;

    if (parameter == kCGLCPSwapInterval)
    {
        *value = context->swap_interval;
        return kCGLNoError;
    }
    fprintf(stderr, "CGLGetParameter unimplemented for parameter %d\n", parameter);
    return kCGLNoError;
}

CGLError CGLDescribeRenderer(CGLRendererInfoObj rend, long rend_num, CGLRendererProperty prop, long *value) {
    return kCGLNoError;
}

CGLError CGLQueryRendererInfo(unsigned long display_mask, CGLRendererInfoObj *rend, long *nrend) {
    return kCGLNoError;
}

CGLError CGLDestroyRendererInfo(CGLRendererInfoObj rend) {
    return kCGLNoError;
}

void CGLGetVersion(GLint *majorvers, GLint *minorvers) {
    if (majorvers)
        *majorvers = 1;
    if (minorvers)
        *minorvers = 0;
}

/* ---- Darling: local discovery placeholders, NOT implementations ----
 * Each logs its own name when called and returns zero, so one run shows which of these an
 * application actually reaches. Replace each with a real implementation, or remove it. */
#include <stdio.h>
#ifndef DARLING_UNIMPLEMENTED
#define DARLING_UNIMPLEMENTED(sym) \
	__attribute__((visibility("default"))) long darling_unimplemented_##sym(void) __asm__("_" #sym); \
	long darling_unimplemented_##sym(void) { fprintf(stderr, "DARLING unimplemented function called: " #sym "\n"); return 0; }
#endif
DARLING_UNIMPLEMENTED(CGLGetVirtualScreen)
DARLING_UNIMPLEMENTED(CGLSetVirtualScreen)
