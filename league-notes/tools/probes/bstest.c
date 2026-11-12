// Can a process started through `darling shell` reach launchd's Mach services?
#include <stdio.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
int main(void) {
    const char *names[] = {"com.apple.system.notification_center", "com.apple.iokit.registry",
        "com.apple.coreservices.launchservicesd", "com.apple.SecurityServer", "com.apple.system.opendirectoryd.libinfo",
        "com.apple.SystemConfiguration.configd", "com.apple.cfprefsd.daemon", "com.apple.dnssd.service", "com.apple.distributed_notifications@Uv3"};
    printf("bootstrap_port = 0x%x\n", bootstrap_port);
    for (unsigned i = 0; i < sizeof(names) / sizeof(*names); i++) {
        mach_port_t p = MACH_PORT_NULL;
        kern_return_t kr = bootstrap_look_up(bootstrap_port, names[i], &p);
        printf("%-48s kr=%d (%s) port=0x%x\n", names[i], kr, kr ? bootstrap_strerror(kr) : "ok", p);
    }
    return 0;
}
