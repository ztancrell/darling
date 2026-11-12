// bootstrap_look_up for the names given, with timing: does a service exist, and does asking for
// one that does not exist fail or hang? Usage: lookuptest <service name>...
#include <stdio.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <sys/time.h>
static double now(void) { struct timeval t; gettimeofday(&t, 0); return t.tv_sec + t.tv_usec / 1e6; }
int main(int argc, char** argv) {
	for (int i = 1; i < argc; i++) {
		mach_port_t p = MACH_PORT_NULL; double t = now();
		kern_return_t kr = bootstrap_look_up(bootstrap_port, argv[i], &p);
		printf("%-32s kr=%d (%s) port=0x%x %.2fs\n", argv[i], kr, kr ? bootstrap_strerror(kr) : "ok", p, now() - t); fflush(stdout);
	}
	return 0;
}
