// Which step of IOPSNotificationCreateRunLoopSource fails under Darling?
#include <stdio.h>
#include <notify.h>
#include <mach/mach.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>
static void cb(void *c) {}
static void mpcb(CFMachPortRef p, void *m, CFIndex s, void *i) {}
int main(void) {
    mach_port_t mp = MACH_PORT_NULL; int token = 0;
    int st = notify_register_mach_port("com.apple.system.powersources.timeremaining", &mp, 0, &token);
    printf("notify_register_mach_port: status=%d port=0x%x token=%d\n", st, mp, token);
    int t2 = 0; int st2 = notify_register_check("com.apple.system.powersources.timeremaining", &t2);
    printf("notify_register_check: status=%d\n", st2);
    if (st == 0) {
        Boolean reused = 0; CFMachPortContext ctx = {0};
        CFMachPortRef r = CFMachPortCreateWithPort(0, mp, mpcb, &ctx, &reused);
        printf("CFMachPortCreateWithPort: %p reused=%d\n", (void *)r, reused);
        if (r) printf("CFMachPortCreateRunLoopSource: %p\n", (void *)CFMachPortCreateRunLoopSource(0, r, 0));
    }
    printf("IOPSNotificationCreateRunLoopSource: %p\n", (void *)IOPSNotificationCreateRunLoopSource(cb, NULL));
    return 0;
}
