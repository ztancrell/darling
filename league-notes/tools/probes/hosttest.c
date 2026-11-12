// The two resolvers RiotClientServices imports besides getaddrinfo:
// CFHost (asynchronous, on a run loop) and libresolv's res_nquery.
#include <CoreFoundation/CoreFoundation.h>
#include <CFNetwork/CFNetwork.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <resolv.h>
#include <stdio.h>
static int called;
static void done(CFHostRef host, CFHostInfoType type, const CFStreamError* err, void* info) {
	Boolean resolved = 0; CFArrayRef a = CFHostGetAddressing(host, &resolved);
	called = 1;
	printf("CFHost callback: error domain=%ld code=%d, resolved=%d, addresses=%ld\n", err ? (long)err->domain : 0, err ? (int)err->error : 0, resolved, a ? (long)CFArrayGetCount(a) : -1);
	if (a && CFArrayGetCount(a)) {
		const struct sockaddr* sa = (const struct sockaddr*)CFDataGetBytePtr(CFArrayGetValueAtIndex(a, 0)); char buf[64] = "?";
		inet_ntop(sa->sa_family, sa->sa_family == AF_INET ? (void*)&((struct sockaddr_in*)sa)->sin_addr : (void*)&((struct sockaddr_in6*)sa)->sin6_addr, buf, sizeof buf);
		printf("  first: %s\n", buf);
	}
	CFRunLoopStop(CFRunLoopGetCurrent());
}
int main(int argc, char** argv) {
	const char* name = argc > 1 ? argv[1] : "clientconfig.rpg.riotgames.com";
	CFStringRef s = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
	CFHostRef h = CFHostCreateWithName(NULL, s);
	CFHostClientContext ctx = { 0 }; CFStreamError err = { 0 };
	printf("CFHostSetClient: %d\n", CFHostSetClient(h, done, &ctx));
	CFHostScheduleWithRunLoop(h, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	Boolean started = CFHostStartInfoResolution(h, kCFHostAddresses, &err);
	printf("CFHostStartInfoResolution: started=%d error domain=%ld code=%d\n", started, (long)err.domain, (int)err.error);
	if (started && !called) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 10, 0);
	printf("CFHost: callback %s\n", called ? "arrived" : "NEVER arrived (10 s)");

	struct __res_state st; unsigned char ans[1500];
	int rc = res_ninit(&st);
	printf("res_ninit: %d, %d nameserver(s)\n", rc, st.nscount);
	int n = res_nquery(&st, name, ns_c_in, ns_t_a, ans, sizeof ans);
	printf("res_nquery A: %d bytes (h_errno %d)\n", n, st.res_h_errno);
	return 0;
}
