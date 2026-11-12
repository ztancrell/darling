// Certificate trust evaluation the way Chromium 108 asks for it: SecTrustCreateWithCertificates
// with an SSL policy, SecTrustEvaluate, SecTrustGetResult. Usage: trusttest <cert.der>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecBase.h>
#include <Security/SecCertificate.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
static double now(void) { struct timeval t; gettimeofday(&t, 0); return t.tv_sec + t.tv_usec / 1e6; }
static SecCertificateRef load(const char* path) {
	FILE* f = fopen(path, "rb"); if (!f) { perror(path); exit(2); }
	unsigned char buf[16384]; size_t n = fread(buf, 1, sizeof buf, f); fclose(f);
	CFDataRef der = CFDataCreate(NULL, buf, n);
	SecCertificateRef cert = SecCertificateCreateWithData(NULL, der);
	printf("%s: %s\n", path, cert ? "parsed" : "NOT parsed"); if (!cert) exit(1);
	return cert;
}
// Usage: trusttest <leaf.der>                       (host 127.0.0.1, as before)
//        trusttest <host> <leaf.der> [more.der...]   (a server's chain as it sent it)
int main(int argc, char** argv) {
	setvbuf(stdout, NULL, _IONBF, 0);
	const char* host = argc > 2 ? argv[1] : "127.0.0.1"; int first = argc > 2 ? 2 : 1;
	CFMutableArrayRef arr = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	for (int i = first; i < argc; i++) CFArrayAppendValue(arr, load(argv[i]));
	CFStringRef h = CFStringCreateWithCString(NULL, host, kCFStringEncodingUTF8);
	SecPolicyRef policy = SecPolicyCreateSSL(true, h);
	SecTrustRef trust = NULL; double t = now();
	OSStatus st = SecTrustCreateWithCertificates(arr, policy, &trust);
	printf("SecTrustCreateWithCertificates: %d (%.2fs)\n", (int)st, now() - t); if (st || !trust) return 1;
	SecTrustResultType result = 99; t = now();
	st = SecTrustEvaluate(trust, &result);
	// 1 proceed, 4 unspecified (trusted by the system roots), 5 recoverable failure, 6 fatal failure
	printf("SecTrustEvaluate: status %d, result %u (%.2fs)\n", (int)st, (unsigned)result, now() - t);
	CFArrayRef props = SecTrustCopyProperties(trust);
	if (props) CFShow(props);
	printf("chain length built: %ld\n", (long)SecTrustGetCertificateCount(trust));
	printf((result == 1 || result == 4) ? "RESULT: TRUSTED\n" : "RESULT: NOT TRUSTED\n");
	return !(result == 1 || result == 4);
}
