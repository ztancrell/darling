/*
 * system_profiler implementation for Darling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPriv.h>

static void get_gpu_and_res(char *gpu, size_t gpusz, int *w, int *h)
{
    snprintf(gpu, gpusz, "Generic GPU");
    *w = 1920;
    *h = 1080;

    // Check DISPLAY env and try xdpyinfo or screen resolution
    const char *disp = getenv("DISPLAY");
    if (disp != NULL) {
        // Try reading resolution via popen of xwininfo / xdpyinfo if available
        FILE *fp = popen("xdpyinfo 2>/dev/null | awk '/dimensions:/ {print $2}'", "r");
        if (fp) {
            char line[64];
            if (fgets(line, sizeof(line), fp)) {
                int dw = 0, dh = 0;
                if (sscanf(line, "%dx%d", &dw, &dh) == 2 && dw > 0 && dh > 0) {
                    *w = dw;
                    *h = dh;
                }
            }
            pclose(fp);
        }
    }

    // Try reading GPU model from CPU brand or GL / sysfs
    char cpu_brand[256];
    size_t len = sizeof(cpu_brand);
    if (sysctlbyname("machdep.cpu.brand_string", cpu_brand, &len, NULL, 0) == 0) {
        if (strstr(cpu_brand, "Radeon") != NULL || strstr(cpu_brand, "Vega") != NULL) {
            snprintf(gpu, gpusz, "AMD Radeon Vega Graphics");
        } else if (strstr(cpu_brand, "Intel") != NULL) {
            snprintf(gpu, gpusz, "Intel Iris / UHD Graphics");
        }
    }

    // Try lspci / glxinfo if present
    FILE *glp = popen("glxinfo 2>/dev/null | awk -F': ' '/OpenGL renderer string/ {print $2}'", "r");
    if (glp) {
        char gline[128];
        if (fgets(gline, sizeof(gline), glp)) {
            // strip newline
            char *nl = strchr(gline, '\n');
            if (nl) *nl = '\0';
            if (strlen(gline) > 0) {
                snprintf(gpu, gpusz, "%s", gline);
            }
        }
        pclose(glp);
    }
}

static void print_displays(void)
{
    char gpu[128];
    int w = 1920, h = 1080;
    get_gpu_and_res(gpu, sizeof(gpu), &w, &h);

    printf("Graphics/Displays:\n\n");
    printf("    %s:\n\n", gpu);
    printf("      Chipset Model: %s\n", gpu);
    printf("      Type: GPU\n");
    printf("      Bus: PCIe\n");
    printf("      Displays:\n");
    printf("        Display:\n");
    printf("          Resolution: %d x %d\n", w, h);
    printf("          UI Looks like: %d x %d\n\n", w, h);
}

// One entry of the array `system_profiler -xml` prints: {_dataType, _items}.
static void add_xml_type(CFMutableArrayRef out, const char *type, CFArrayRef items)
{
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef t = CFStringCreateWithCString(NULL, type, kCFStringEncodingUTF8);
    CFDictionarySetValue(d, CFSTR("_dataType"), t);
    CFDictionarySetValue(d, CFSTR("_items"), items);
    CFArrayAppendValue(out, d);
    CFRelease(t);
    CFRelease(d);
}

static void add_xml_displays(CFMutableArrayRef out)
{
    char gpu[128];
    int w = 1920, h = 1080;
    get_gpu_and_res(gpu, sizeof(gpu), &w, &h);

    CFStringRef model = CFStringCreateWithCString(NULL, gpu, kCFStringEncodingUTF8);
    if (!model) model = CFSTR("Generic GPU");
    CFStringRef res = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d x %d"), w, h);

    const void *dk[] = { CFSTR("_name"), CFSTR("_spdisplays_pixels"), CFSTR("_spdisplays_resolution") };
    const void *dv[] = { CFSTR("Display"), res, res };
    CFDictionaryRef disp = CFDictionaryCreate(NULL, dk, dv, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFArrayRef disps = CFArrayCreate(NULL, (const void **)&disp, 1, &kCFTypeArrayCallBacks);

    // No spdisplays_vram: the amount is not known here, and macOS omits the key too when it has none.
    const void *gk[] = { CFSTR("_name"), CFSTR("sppci_model"), CFSTR("sppci_bus"), CFSTR("sppci_device_type"), CFSTR("spdisplays_ndrvs") };
    const void *gv[] = { model, model, CFSTR("spdisplays_pcie_device"), CFSTR("spdisplays_gpu"), disps };
    CFDictionaryRef card = CFDictionaryCreate(NULL, gk, gv, 5, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFArrayRef items = CFArrayCreate(NULL, (const void **)&card, 1, &kCFTypeArrayCallBacks);

    add_xml_type(out, "SPDisplaysDataType", items);
    CFRelease(items); CFRelease(card); CFRelease(disps); CFRelease(disp); CFRelease(res); CFRelease(model);
}

static void print_hardware(void)
{
    char cpu[256] = "Unknown";
    size_t len = sizeof(cpu);
    sysctlbyname("machdep.cpu.brand_string", cpu, &len, NULL, 0);

    int ncpu = 1;
    len = sizeof(ncpu);
    sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0);

    uint64_t memsize = 0;
    len = sizeof(memsize);
    sysctlbyname("hw.memsize", &memsize, &len, NULL, 0);
    int mem_gb = (int)(memsize / (1024 * 1024 * 1024));

    printf("Hardware:\n\n");
    printf("    Hardware Overview:\n\n");
    printf("      Model Name: Mac\n");
    printf("      Model Identifier: Darling1,1\n");
    printf("      Processor Name: %s\n", cpu);
    printf("      Total Number of Cores: %d\n", ncpu);
    printf("      Memory: %d GB\n\n", mem_gb);
}

static void print_software(void)
{
    struct utsname un;
    uname(&un);

    CFDictionaryRef dict = _CFCopySystemVersionDictionary();
    const char *prod_name = "macOS";
    const char *prod_vers = "26.0";
    const char *build_vers = "25A5279f";
    char pbuf[64], vbuf[64], bbuf[64];

    if (dict) {
        CFStringRef p = CFDictionaryGetValue(dict, _kCFSystemVersionProductNameKey);
        CFStringRef v = CFDictionaryGetValue(dict, _kCFSystemVersionProductVersionKey);
        CFStringRef b = CFDictionaryGetValue(dict, _kCFSystemVersionBuildVersionKey);
        if (p && CFStringGetCString(p, pbuf, sizeof(pbuf), kCFStringEncodingUTF8)) prod_name = pbuf;
        if (v && CFStringGetCString(v, vbuf, sizeof(vbuf), kCFStringEncodingUTF8)) prod_vers = vbuf;
        if (b && CFStringGetCString(b, bbuf, sizeof(bbuf), kCFStringEncodingUTF8)) build_vers = bbuf;
    }

    printf("Software:\n\n");
    printf("    System Software Overview:\n\n");
    printf("      System Version: %s %s (%s)\n", prod_name, prod_vers, build_vers);
    printf("      Kernel Version: %s %s\n\n", un.sysname, un.release);
}

int main(int argc, char *argv[])
{
    int xml = 0, types = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-xml") == 0) xml = 1;
        else if (argv[i][0] != '-') types++;
    }

    if (xml) {
        CFMutableArrayRef out = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        CFArrayRef none = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
        for (int i = 1; i < argc; i++) {
            if (argv[i][0] == '-') continue;
            if (strcmp(argv[i], "SPDisplaysDataType") == 0) add_xml_displays(out);
            // Other types: a well-formed entry with no items, so a caller's plist parse still succeeds.
            else add_xml_type(out, argv[i], none);
        }
        if (types == 0) add_xml_displays(out);
        CFDataRef data = CFPropertyListCreateData(NULL, out, kCFPropertyListXMLFormat_v1_0, 0, NULL);
        if (!data) return 1;
        fwrite(CFDataGetBytePtr(data), 1, CFDataGetLength(data), stdout);
        return 0;
    }

    if (types == 0) {
        print_hardware();
        print_displays();
        print_software();
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "SPDisplaysDataType") == 0) {
            print_displays();
        } else if (strcmp(argv[i], "SPHardwareDataType") == 0) {
            print_hardware();
        } else if (strcmp(argv[i], "SPSoftwareDataType") == 0) {
            print_software();
        }
    }
    return 0;
}
