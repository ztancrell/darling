// Drive the Vulkan loader built into ANGLE's libGLESv2 directly: create an instance the way ANGLE
// does (API 1.1, VK_KHR_surface + VK_EXT_metal_surface), then ask it for instance functions.
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef void (*PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (*PFN_gipa)(void *instance, const char *name);
int main(int argc, char **argv) {
	void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
	printf("load %s: %s\n", argv[1], lib ? "ok" : dlerror()); if (!lib) return 1;
	PFN_gipa gipa = (PFN_gipa)dlsym(lib, "vkGetInstanceProcAddr");
	int32_t (*createInstance)(const void *, const void *, void **) = (void *)dlsym(lib, "vkCreateInstance");
	int32_t (*enumVersion)(uint32_t *) = (void *)gipa(NULL, "vkEnumerateInstanceVersion");
	uint32_t ver = 0; if (enumVersion) enumVersion(&ver); printf("loader instance version: %u.%u.%u\n", ver >> 22, (ver >> 12) & 0x3ff, ver & 0xfff);
	int32_t (*enumExt)(const char *, uint32_t *, void *) = (void *)gipa(NULL, "vkEnumerateInstanceExtensionProperties");
	uint32_t n = 0; enumExt(NULL, &n, NULL); static char exts[96][260]; if (n > 96) n = 96; enumExt(NULL, &n, exts);
	int haveMetal = 0; for (uint32_t i = 0; i < n; i++) if (!strcmp(exts[i], "VK_EXT_metal_surface")) haveMetal = 1;
	printf("loader instance extensions: %u, VK_EXT_metal_surface %s\n", n, haveMetal ? "present" : "ABSENT");
	// ANGLE's instance extension set with this driver (RendererVk::initialize, chromium/5359)
	const char *want[] = { "VK_KHR_surface", "VK_EXT_swapchain_colorspace", "VK_EXT_metal_surface", "VK_KHR_get_surface_capabilities2", "VK_KHR_surface_protected_capabilities", "VK_KHR_get_physical_device_properties2", "VK_KHR_external_memory_capabilities", "VK_KHR_external_semaphore_capabilities", "VK_KHR_external_fence_capabilities" };
	struct { uint32_t sType; const void *pNext; const char *a; uint32_t b; const char *c; uint32_t d; uint32_t apiVersion; } app = { 0, NULL, "vkloadertest", 1, "none", 1, (1u << 22) | (1u << 12) };
	struct { uint32_t sType; const void *pNext; uint32_t flags; const void *pApp; uint32_t lc; const char *const *l; uint32_t ec; const char *const *e; } info = { 1, NULL, 0, &app, 0, NULL, sizeof want / sizeof *want, want };
	void *instance = NULL; int32_t r = createInstance(&info, NULL, &instance);
	printf("vkCreateInstance via the loader: result %d, instance %p\n", r, instance); if (r) return 1;
	const char *names[] = { "vkEnumeratePhysicalDevices", "vkGetPhysicalDeviceProperties2", "vkGetPhysicalDeviceFragmentShadingRatesKHR", "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT", "vkGetPhysicalDeviceSurfaceCapabilities2KHR", "vkGetPhysicalDeviceToolPropertiesEXT" };
	for (unsigned i = 0; i < sizeof names / sizeof *names; i++) printf("  gipa(instance, %s) = %s\n", names[i], gipa(instance, names[i]) ? "ok" : "NULL");
	int32_t (*enumerate)(void *, uint32_t *, void **) = (void *)gipa(instance, "vkEnumeratePhysicalDevices");
	void (*getProps)(void *, void *) = (void *)gipa(instance, "vkGetPhysicalDeviceProperties");
	if (!enumerate || !getProps) { printf("RESULT: FAIL (loader gives no instance functions)\n"); return 1; }
	uint32_t count = 0; enumerate(instance, &count, NULL); void *devs[8] = { 0 }; if (count > 8) count = 8; enumerate(instance, &count, devs);
	printf("physical devices via the loader: %u\n", count);
	for (uint32_t i = 0; i < count; i++) { static char props[4096]; getProps(devs[i], props); printf("  %u: \"%s\"\n", i, props + 20); }
	int32_t (*enumDevExt)(void *, const char *, uint32_t *, void *) = (void *)gipa(instance, "vkEnumerateDeviceExtensionProperties");
	for (uint32_t i = 0; i < count && enumDevExt; i++) {
		uint32_t m = 0; enumDevExt(devs[i], NULL, &m, NULL); static char dexts[400][260]; if (m > 400) m = 400; enumDevExt(devs[i], NULL, &m, dexts);
		int vrs = 0; for (uint32_t j = 0; j < m; j++) if (!strcmp(dexts[j], "VK_KHR_fragment_shading_rate")) vrs = 1;
		printf("  device %u: %u device extensions via the loader, VK_KHR_fragment_shading_rate %s\n", i, m, vrs ? "PRESENT" : "absent");
	}
	printf(count ? "RESULT: PASS\n" : "RESULT: FAIL\n"); return !count;
}
