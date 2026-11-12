// Does the wrapped host Vulkan driver work from Mac code? Load it the way a Vulkan loader would,
// create an instance through it, and list the physical devices it reports.
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef void *(*PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (*PFN_gipa)(void *instance, const char *name);
typedef int32_t (*PFN_negotiate)(uint32_t *version);
int main(int argc, char **argv) {
	const char *path = argc > 1 ? argv[1] : "/usr/lib/native/libvulkan_radeon.dylib"; void *lib = dlopen(path, RTLD_NOW);
	printf("load %s: %s\n", path, lib ? "ok" : dlerror()); if (!lib) return 1;
	PFN_negotiate negotiate = (PFN_negotiate)dlsym(lib, "vk_icdNegotiateLoaderICDInterfaceVersion");
	PFN_gipa gipa = (PFN_gipa)dlsym(lib, "vk_icdGetInstanceProcAddr");
	uint32_t version = 7; int32_t r = negotiate ? negotiate(&version) : -99;
	printf("negotiate loader interface: result %d, driver supports version %u\n", r, version);
	struct { uint32_t sType; const void *pNext; const char *appName; uint32_t appVersion; const char *engineName; uint32_t engineVersion; uint32_t apiVersion; } app = { 0, NULL, "vkicdtest", 1, "none", 1, (1u << 22) | (3u << 12) };
	struct { uint32_t sType; const void *pNext; uint32_t flags; const void *pApp; uint32_t layerCount; const char *const *layers; uint32_t extCount; const char *const *exts; } info = { 1, NULL, 0, &app, 0, NULL, 0, NULL };
	int32_t (*enumExt)(const char *, uint32_t *, void *) = (void *)gipa(NULL, "vkEnumerateInstanceExtensionProperties");
	uint32_t extCount = 0; enumExt(NULL, &extCount, NULL); static char exts[64][260]; if (extCount > 64) extCount = 64; enumExt(NULL, &extCount, exts);
	printf("instance extensions from the driver (%u):", extCount); for (uint32_t i = 0; i < extCount; i++) printf(" %s", exts[i]); printf("\n");
	int32_t (*createInstance)(const void *, const void *, void **) = (void *)gipa(NULL, "vkCreateInstance");
	void *instance = NULL; r = createInstance ? createInstance(&info, NULL, &instance) : -99;
	printf("vkCreateInstance through the driver: result %d, instance %p\n", r, instance); if (r != 0) return 1;
	PFN_gipa gpdpa = (PFN_gipa)dlsym(lib, "vk_icdGetPhysicalDeviceProcAddr");
	const char *q[] = { "vkGetPhysicalDeviceFragmentShadingRatesKHR", "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT", "vkGetPhysicalDeviceSurfaceCapabilities2KHR" };
	for (unsigned i = 0; i < 3; i++) printf("  driver: gipa(instance, %s) = %s, gpdpa = %s\n", q[i], gipa(instance, q[i]) ? "ok" : "NULL", gpdpa && gpdpa(instance, q[i]) ? "ok" : "NULL");
	int32_t (*enumerate)(void *, uint32_t *, void **) = (void *)gipa(instance, "vkEnumeratePhysicalDevices");
	void (*getProps)(void *, void *) = (void *)gipa(instance, "vkGetPhysicalDeviceProperties");
	uint32_t count = 0; enumerate(instance, &count, NULL); void *devs[8] = { 0 }; if (count > 8) count = 8; enumerate(instance, &count, devs);
	printf("physical devices: %u\n", count);
	for (uint32_t i = 0; i < count; i++) { static char props[4096]; getProps(devs[i], props); uint32_t api = *(uint32_t *)props; printf("  %u: \"%s\" (Vulkan %u.%u.%u)\n", i, props + 20, api >> 22, (api >> 12) & 0x3ff, api & 0xfff); }
	printf(count ? "RESULT: PASS\n" : "RESULT: FAIL\n");
	return count ? 0 : 1;
}
