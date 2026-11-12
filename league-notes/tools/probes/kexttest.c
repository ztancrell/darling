// kext_request(): the host call behind "list loaded kernel extensions" (kextstat, KextManager).
// Darling has no kernel extensions. Pass: the call returns an error and the system stays up.
#include <mach/mach.h>
#include <mach/host_priv.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
	static char req[] = "<dict><key>Kext Request Predicate</key><string>Get Loaded Kext Info</string></dict>";
	vm_offset_t resp = 0, log = 0;
	mach_msg_type_number_t resplen = 0, loglen = 0;
	kern_return_t op = 0;
	setvbuf(stdout, NULL, _IONBF, 0);
	kern_return_t kr = kext_request(mach_host_self(), 0, (vm_offset_t)req, sizeof(req), &resp, &resplen, &log, &loglen, &op);
	printf("kext_request kr 0x%x (%s) op 0x%x response %u bytes\n", kr, mach_error_string(kr), op, resplen);
	// still able to talk to the system afterwards?
	mach_port_t p = MACH_PORT_NULL;
	kern_return_t kr2 = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &p);
	printf("afterwards: mach_port_allocate kr 0x%x, getppid %d\n", kr2, getppid());
	int ok = kr != KERN_SUCCESS && kr2 == KERN_SUCCESS;
	puts(ok ? "PASS" : "FAIL");
	return !ok;
}
