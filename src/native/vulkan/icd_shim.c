// A Vulkan driver shim: the host driver (wrapped as libvulkan_radeon.dylib) with one difference.
//
// ANGLE's Vulkan backend, built for macOS, refuses to create an instance unless the
// VK_EXT_metal_surface extension is available: it was written against MoltenVK, where that is how
// a window is reached. The host driver knows X11, Wayland and headless surfaces, not Metal ones.
//
// ponytail: this advertises VK_EXT_metal_surface and strips it before the real driver sees it. It
// is a claim no window in Darling can honour; it only lets off-screen rendering (which is all
// Chromium's GPU process does through Vulkan) get started. If anyone does present a Metal surface
// to the driver, the queries below answer "surface lost" instead of handing the driver a surface
// type it has never seen. The real fix is a surface mapping onto Darling's own windows.

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Just enough of the Vulkan ABI; the real headers are not in Darling's SDK.
typedef int32_t VkResult;
typedef void (*PFN_vkVoidFunction)(void);
typedef uint64_t VkSurfaceKHR;
#define VK_SUCCESS 0
#define VK_INCOMPLETE 5
#define VK_ERROR_SURFACE_LOST_KHR (-1000000000)

typedef struct { char extensionName[256]; uint32_t specVersion; } VkExtensionProperties;
typedef struct {
	uint32_t sType; const void* pNext; uint32_t flags; const void* pApplicationInfo;
	uint32_t enabledLayerCount; const char* const* ppEnabledLayerNames;
	uint32_t enabledExtensionCount; const char* const* ppEnabledExtensionNames;
} VkInstanceCreateInfo;
// The loader hands drivers its own surface objects; this is their common header.
typedef struct { uint32_t platform; } VkIcdSurfaceBase;
enum { VK_ICD_WSI_PLATFORM_MACOS = 6, VK_ICD_WSI_PLATFORM_IOS = 7, VK_ICD_WSI_PLATFORM_METAL = 10 };

typedef PFN_vkVoidFunction (*PFN_GetInstanceProcAddr)(void* instance, const char* name);
typedef VkResult (*PFN_NegotiateVersion)(uint32_t* version);
typedef VkResult (*PFN_EnumerateInstanceExtensionProperties)(const char* layer, uint32_t* count, VkExtensionProperties* props);
typedef VkResult (*PFN_CreateInstance)(const VkInstanceCreateInfo* info, const void* allocator, void** instance);

static const char* const kMetalSurface = "VK_EXT_metal_surface";
static const char* const kDriverPath = "/usr/lib/native/libvulkan_radeon.dylib";

static PFN_GetInstanceProcAddr real_gipa;
static PFN_GetInstanceProcAddr real_gpdpa;
static PFN_NegotiateVersion real_negotiate;

static int load_driver(void) {
	static void* handle;
	if (handle) {
		return 1;
	}
	handle = dlopen(kDriverPath, RTLD_NOW | RTLD_LOCAL);
	if (!handle) {
		fprintf(stderr, "DARLING vulkan shim: cannot load %s: %s\n", kDriverPath, dlerror());
		return 0;
	}
	real_gipa = (PFN_GetInstanceProcAddr)dlsym(handle, "vk_icdGetInstanceProcAddr");
	real_gpdpa = (PFN_GetInstanceProcAddr)dlsym(handle, "vk_icdGetPhysicalDeviceProcAddr");
	real_negotiate = (PFN_NegotiateVersion)dlsym(handle, "vk_icdNegotiateLoaderICDInterfaceVersion");
	return real_gipa != NULL;
}

static VkResult shim_EnumerateInstanceExtensionProperties(const char* layer, uint32_t* count, VkExtensionProperties* props) {
	PFN_EnumerateInstanceExtensionProperties real = (PFN_EnumerateInstanceExtensionProperties)real_gipa(NULL, "vkEnumerateInstanceExtensionProperties");
	if (layer) {
		return real(layer, count, props);
	}

	uint32_t available = 0;
	VkResult result = real(NULL, &available, NULL);
	if (result != VK_SUCCESS) {
		return result;
	}
	if (!props) {
		*count = available + 1;
		return VK_SUCCESS;
	}

	uint32_t capacity = *count;
	uint32_t written = capacity < available ? capacity : available;
	result = real(NULL, &written, props);
	if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
		return result;
	}
	if (written < capacity) {
		strlcpy(props[written].extensionName, kMetalSurface, sizeof(props[written].extensionName));
		props[written].specVersion = 1;
		written++;
	}
	*count = written;
	return written == available + 1 ? VK_SUCCESS : VK_INCOMPLETE;
}

static VkResult shim_CreateInstance(const VkInstanceCreateInfo* info, const void* allocator, void** instance) {
	PFN_CreateInstance real = (PFN_CreateInstance)real_gipa(NULL, "vkCreateInstance");
	VkInstanceCreateInfo copy = *info;
	const char** names = calloc(info->enabledExtensionCount ? info->enabledExtensionCount : 1, sizeof(*names));
	if (!names) {
		return -1; // VK_ERROR_OUT_OF_HOST_MEMORY
	}
	uint32_t kept = 0;
	for (uint32_t i = 0; i < info->enabledExtensionCount; i++) {
		if (strcmp(info->ppEnabledExtensionNames[i], kMetalSurface) != 0) {
			names[kept++] = info->ppEnabledExtensionNames[i];
		}
	}
	copy.enabledExtensionCount = kept;
	copy.ppEnabledExtensionNames = names;
	VkResult result = real(&copy, allocator, instance);
	free(names);
	return result;
}

static int is_apple_surface(VkSurfaceKHR surface) {
	const VkIcdSurfaceBase* base = (const VkIcdSurfaceBase*)(uintptr_t)surface;
	return base && (base->platform == VK_ICD_WSI_PLATFORM_MACOS || base->platform == VK_ICD_WSI_PLATFORM_IOS || base->platform == VK_ICD_WSI_PLATFORM_METAL);
}

// The surface queries whose second argument is the surface: refuse Apple surfaces, forward the rest.
#define SURFACE_QUERY(name, ...) \
	static VkResult shim_##name(void* physicalDevice, VkSurfaceKHR surface, __VA_ARGS__);
#define SURFACE_QUERY_BODY(name, args, ...) \
	static VkResult shim_##name(void* physicalDevice, VkSurfaceKHR surface, __VA_ARGS__) { \
		if (is_apple_surface(surface)) { \
			return VK_ERROR_SURFACE_LOST_KHR; \
		} \
		VkResult (*real)(void*, VkSurfaceKHR, __VA_ARGS__) = (VkResult (*)(void*, VkSurfaceKHR, __VA_ARGS__))real_gipa(NULL, #name); \
		return real args; \
	}

SURFACE_QUERY_BODY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, (physicalDevice, surface, a), void* a)
SURFACE_QUERY_BODY(vkGetPhysicalDeviceSurfaceFormatsKHR, (physicalDevice, surface, a, b), uint32_t* a, void* b)
SURFACE_QUERY_BODY(vkGetPhysicalDeviceSurfacePresentModesKHR, (physicalDevice, surface, a, b), uint32_t* a, void* b)

static VkResult shim_vkGetPhysicalDeviceSurfaceSupportKHR(void* physicalDevice, uint32_t queueFamily, VkSurfaceKHR surface, uint32_t* supported) {
	if (is_apple_surface(surface)) {
		*supported = 0;
		return VK_SUCCESS;
	}
	VkResult (*real)(void*, uint32_t, VkSurfaceKHR, uint32_t*) = (VkResult (*)(void*, uint32_t, VkSurfaceKHR, uint32_t*))real_gipa(NULL, "vkGetPhysicalDeviceSurfaceSupportKHR");
	return real(physicalDevice, queueFamily, surface, supported);
}

__attribute__((visibility("default")))
VkResult vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t* version) {
	if (!load_driver()) {
		return -1;
	}
	return real_negotiate ? real_negotiate(version) : VK_SUCCESS;
}

__attribute__((visibility("default")))
PFN_vkVoidFunction vk_icdGetInstanceProcAddr(void* instance, const char* name) {
	if (!load_driver()) {
		return NULL;
	}
	static const struct { const char* name; PFN_vkVoidFunction function; } overrides[] = {
		{ "vkEnumerateInstanceExtensionProperties", (PFN_vkVoidFunction)shim_EnumerateInstanceExtensionProperties },
		{ "vkCreateInstance", (PFN_vkVoidFunction)shim_CreateInstance },
		{ "vkGetPhysicalDeviceSurfaceSupportKHR", (PFN_vkVoidFunction)shim_vkGetPhysicalDeviceSurfaceSupportKHR },
		{ "vkGetPhysicalDeviceSurfaceCapabilitiesKHR", (PFN_vkVoidFunction)shim_vkGetPhysicalDeviceSurfaceCapabilitiesKHR },
		{ "vkGetPhysicalDeviceSurfaceFormatsKHR", (PFN_vkVoidFunction)shim_vkGetPhysicalDeviceSurfaceFormatsKHR },
		{ "vkGetPhysicalDeviceSurfacePresentModesKHR", (PFN_vkVoidFunction)shim_vkGetPhysicalDeviceSurfacePresentModesKHR },
	};
	for (size_t i = 0; i < sizeof(overrides) / sizeof(*overrides); i++) {
		if (strcmp(name, overrides[i].name) == 0) {
			// only override what the driver actually provides, so the override never outlives the real one
			return real_gipa(instance, name) ? overrides[i].function : NULL;
		}
	}
	return real_gipa(instance, name);
}

__attribute__((visibility("default")))
PFN_vkVoidFunction vk_icdGetPhysicalDeviceProcAddr(void* instance, const char* name) {
	if (!load_driver() || !real_gpdpa) {
		return NULL;
	}
	return real_gpdpa(instance, name);
}
