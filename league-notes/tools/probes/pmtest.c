// What Chromium's PowerMonitorDeviceSource does at start: IORegisterForSystemPower, then the
// notification port's run loop source. Also the two iokitd service names, looked up directly.
#include <stdio.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
static void cb(void* r, io_service_t s, natural_t t, void* a) {}
int main(void) {
	const char* names[] = {"org.darlinghq.iokitd", "com.apple.PowerManagement.control", "com.apple.system.notification_center"};
	for (int i = 0; i < 3; i++) { mach_port_t p = 0; kern_return_t kr = bootstrap_look_up(bootstrap_port, names[i], &p); printf("%s kr=%d port=0x%x\n", names[i], kr, p); }
	IONotificationPortRef np = NULL; io_object_t n = 0;
	io_connect_t c = IORegisterForSystemPower(NULL, &np, cb, &n);
	printf("IORegisterForSystemPower connect=0x%x port=%p notifier=0x%x source=%p\n", c, np, n, np ? IONotificationPortGetRunLoopSource(np) : NULL);
	return 0;
}
