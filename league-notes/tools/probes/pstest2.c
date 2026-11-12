// The other half of Chromium's PowerMonitorDeviceSource::PlatformInit: the battery run loop source.
// Run as the first process after a container boot. Any argument: do a plain notify registration first.
#include <stdio.h>
#include <notify.h>
#include <mach/mach.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>
static void cb(void* c) {}
int main(int argc, char** argv) {
	if (argc > 1) { int tok = -1; mach_port_t p = 0; uint32_t s = notify_register_mach_port("probe.cold", &p, 0, &tok); printf("notify_register_mach_port status=%u port=0x%x token=%d\n", s, p, tok); }
	for (int i = 0; i < 3; i++) { CFRunLoopSourceRef src = IOPSNotificationCreateRunLoopSource(cb, NULL); printf("IOPSNotificationCreateRunLoopSource #%d = %p\n", i, src); }
	return 0;
}
