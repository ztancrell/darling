// IOSurface as Chromium uses it for frames: create, draw, hand over as a Mach port, look up, read.
// Also: a surface port whose header lies about its size must be refused, not mapped.
#include <CoreFoundation/CoreFoundation.h>
#include <IOSurface/IOSurface.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <objc/runtime.h>
#include <stdio.h>
#include <string.h>

static int failed;
#define CHECK(cond, what) do { int ok_ = (cond); printf("%s %s\n", ok_ ? "ok  " : "FAIL", what); if (!ok_) failed = 1; } while (0)
static void set(CFMutableDictionaryRef d, CFStringRef k, long v) { CFNumberRef n = CFNumberCreate(NULL, kCFNumberLongType, &v); CFDictionarySetValue(d, k, n); CFRelease(n); }
static CFMutableDictionaryRef dict(void) { return CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks); }

// A two-page memory entry whose first page is `header`.
static mach_port_t fake(const void* header, size_t len) {
	mach_port_t h = MACH_PORT_NULL; memory_object_size_t size = 8192; mach_vm_address_t a = 0;
	if (mach_make_memory_entry_64(mach_task_self(), &size, 0, MAP_MEM_NAMED_CREATE | VM_PROT_READ | VM_PROT_WRITE, &h, MACH_PORT_NULL)) return MACH_PORT_NULL;
	if (mach_vm_map(mach_task_self(), &a, 4096, 0, VM_FLAGS_ANYWHERE, h, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE)) return MACH_PORT_NULL;
	memcpy((void*)a, header, len);
	mach_vm_deallocate(mach_task_self(), a, 4096);
	return h;
}

int main(void) {
	// 1. the short form
	CFMutableDictionaryRef d = dict();
	set(d, kIOSurfaceWidth, 600); set(d, kIOSurfaceHeight, 600); set(d, kIOSurfaceBytesPerElement, 4); set(d, kIOSurfacePixelFormat, 'BGRA');
	IOSurfaceRef s = IOSurfaceCreate(d);
	CHECK(s != NULL, "create 600x600 BGRA");
	if (!s) { printf("RESULT: FAIL\n"); return 1; }
	printf("  %zux%zu, %zu bytes/row, alloc %zu, id 0x%x, base %p\n", IOSurfaceGetWidth(s), IOSurfaceGetHeight(s), IOSurfaceGetBytesPerRow(s), IOSurfaceGetAllocSize(s), IOSurfaceGetID(s), IOSurfaceGetBaseAddress(s));
	CHECK(IOSurfaceGetWidth(s) == 600 && IOSurfaceGetHeight(s) == 600 && IOSurfaceGetBytesPerRow(s) >= 2400 && IOSurfaceGetPixelFormat(s) == 'BGRA', "geometry");
	CHECK(IOSurfaceGetAllocSize(s) >= 600 * IOSurfaceGetBytesPerRow(s) && IOSurfaceGetID(s) != 0 && IOSurfaceGetBaseAddress(s) != NULL, "memory and id");

	// 2. draw
	uint32_t seed0 = 0, seed1 = 0;
	CHECK(IOSurfaceLock(s, 0, &seed0) == 0, "lock");
	uint32_t* px = IOSurfaceGetBaseAddress(s);
	CHECK(px[0] == 0, "new surface is zeroed");
	px[0] = 0xff112233; ((uint8_t*)px)[IOSurfaceGetAllocSize(s) - 1] = 0x5a;
	CHECK(IOSurfaceUnlock(s, 0, &seed1) == 0 && seed1 != seed0, "unlock after writing changes the seed");

	// 3. hand over as a port, look up
	mach_port_t port = IOSurfaceCreateMachPort(s);
	CHECK(port != MACH_PORT_NULL, "Mach port");
	IOSurfaceRef t = IOSurfaceLookupFromMachPort(port);
	CHECK(t != NULL, "lookup from the port");
	if (t) {
		uint32_t* qx = IOSurfaceGetBaseAddress(t);
		CHECK(qx != px && IOSurfaceGetID(t) == IOSurfaceGetID(s) && IOSurfaceGetWidth(t) == 600 && IOSurfaceGetBytesPerRow(t) == IOSurfaceGetBytesPerRow(s), "second mapping, same surface");
		CHECK(qx[0] == 0xff112233 && ((uint8_t*)qx)[IOSurfaceGetAllocSize(t) - 1] == 0x5a, "pixels drawn through the first are read through the second");
		qx[1] = 0xdeadbeef;
		CHECK(px[1] == 0xdeadbeef, "and the other way");
		CHECK(!IOSurfaceIsInUse(s), "not in use");
		IOSurfaceIncrementUseCount(t);
		CHECK(IOSurfaceIsInUse(s), "use count taken through one is seen through the other");
		IOSurfaceDecrementUseCount(t);
		CHECK(!IOSurfaceIsInUse(s), "and released");
		CFRelease(t);
		CHECK(px[1] == 0xdeadbeef, "first mapping survives the second's release");
		t = IOSurfaceLookupFromMachPort(port);
		CHECK(t != NULL, "port still good after that release");
		if (t) CFRelease(t);
	}

	// 4. the long form Chromium uses (plane info, aligned sizes)
	long w = 1536, h = 864, bpr = IOSurfaceAlignProperty(kIOSurfacePlaneBytesPerRow, w * 4), psize = IOSurfaceAlignProperty(kIOSurfacePlaneSize, h * bpr);
	CHECK(bpr >= w * 4 && psize >= h * bpr, "IOSurfaceAlignProperty never shrinks");
	CFMutableDictionaryRef pl = dict(), d2 = dict();
	set(pl, kIOSurfacePlaneWidth, w); set(pl, kIOSurfacePlaneHeight, h); set(pl, kIOSurfacePlaneBytesPerElement, 4); set(pl, kIOSurfacePlaneBytesPerRow, bpr); set(pl, kIOSurfacePlaneOffset, 0); set(pl, kIOSurfacePlaneSize, psize);
	const void* arr[1] = { pl };
	CFArrayRef planes = CFArrayCreate(NULL, arr, 1, &kCFTypeArrayCallBacks);
	set(d2, kIOSurfaceWidth, w); set(d2, kIOSurfaceHeight, h); set(d2, kIOSurfacePixelFormat, 'BGRA'); set(d2, kIOSurfaceAllocSize, IOSurfaceAlignProperty(kIOSurfaceAllocSize, psize));
	CFDictionarySetValue(d2, kIOSurfacePlaneInfo, planes);
	IOSurfaceRef c = IOSurfaceCreate(d2);
	CHECK(c && IOSurfaceGetPlaneCount(c) == 1 && IOSurfaceGetWidthOfPlane(c, 0) == 1536 && IOSurfaceGetHeight(c) == 864 && IOSurfaceGetBytesPerRowOfPlane(c, 0) == (size_t)bpr && IOSurfaceGetBaseAddressOfPlane(c, 0) != NULL, "create 1536x864 with plane info");
	if (c) { memset(IOSurfaceGetBaseAddress(c), 0x7f, IOSurfaceGetAllocSize(c)); CHECK(1, "whole allocation is writable"); }

	// 5. refusals
	set(d, kIOSurfaceWidth, 0);
	CHECK(IOSurfaceCreate(d) == NULL, "zero width refused");
	CHECK(IOSurfaceLookupFromMachPort(MACH_PORT_NULL) == NULL, "null port refused");
	char junk[64]; memset(junk, 0xab, sizeof junk);
	CHECK(IOSurfaceLookupFromMachPort(fake(junk, sizeof junk)) == NULL, "memory that is not a surface refused");
	// copy a real header into a two-page entry: it now claims far more memory than the entry has
	mach_vm_address_t hdr = 0;
	if (mach_vm_map(mach_task_self(), &hdr, 4096, 0, VM_FLAGS_ANYWHERE, port, 0, FALSE, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE) == 0)
		CHECK(IOSurfaceLookupFromMachPort(fake((void*)hdr, 256)) == NULL, "header claiming more memory than the port has refused");
	else CHECK(0, "could not read a real header");
	mach_port_t alive = MACH_PORT_NULL;
	CHECK(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &alive) == 0, "server still answers afterwards");

	// 6. what Chromium probes before choosing how to show frames (information, not a check)
	Class ctx = objc_getClass("CAContext"), host = objc_getClass("CALayerHost");
	printf("info CAContext class %s, +contextWithCGSConnection:options: %s; CALayerHost class %s, -setContextId: %s\n",
		ctx ? "present" : "absent", ctx && class_respondsToSelector(object_getClass((id)ctx), sel_registerName("contextWithCGSConnection:options:")) ? "yes" : "no",
		host ? "present" : "absent", host && class_respondsToSelector(host, sel_registerName("setContextId:")) ? "yes" : "no");

	printf(failed ? "RESULT: FAIL\n" : "RESULT: PASS\n");
	return failed;
}
