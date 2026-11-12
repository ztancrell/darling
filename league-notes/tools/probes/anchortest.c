// What the system says about trust anchors, the way Chromium's own verifier asks:
// SecTrustSettingsCopyCertificates per domain, SecTrustCopyAnchorCertificates, and the trust settings of one root.
// Usage: anchortest <root.der>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecBase.h>
#include <Security/SecCertificate.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustSettings.h>
#include <stdio.h>
int main(int argc, char** argv) {
	setvbuf(stdout, NULL, _IONBF, 0);
	const char* names[] = { "user", "admin", "system" };
	for (int d = 0; d < 3; d++) {
		CFArrayRef certs = NULL; OSStatus st = SecTrustSettingsCopyCertificates((SecTrustSettingsDomain)d, &certs);
		printf("SecTrustSettingsCopyCertificates(%s): status %d, %ld certificates\n", names[d], (int)st, certs ? (long)CFArrayGetCount(certs) : -1);
	}
	CFArrayRef anchors = NULL; OSStatus st = SecTrustCopyAnchorCertificates(&anchors);
	printf("SecTrustCopyAnchorCertificates: status %d, %ld anchors\n", (int)st, anchors ? (long)CFArrayGetCount(anchors) : -1);
	if (argc > 1) {
		FILE* f = fopen(argv[1], "rb"); unsigned char buf[8192]; size_t n = f ? fread(buf, 1, sizeof buf, f) : 0;
		SecCertificateRef root = SecCertificateCreateWithData(NULL, CFDataCreate(NULL, buf, n));
		printf("root: %s\n", root ? "parsed" : "NOT parsed");
		for (int d = 0; root && d < 3; d++) {
			CFArrayRef settings = NULL; st = SecTrustSettingsCopyTrustSettings(root, (SecTrustSettingsDomain)d, &settings);
			printf("SecTrustSettingsCopyTrustSettings(root, %s): status %d, %ld entries\n", names[d], (int)st, settings ? (long)CFArrayGetCount(settings) : -1);
		}
		if (root && anchors) {
			int found = 0; CFDataRef want = SecCertificateCopyData(root);
			for (CFIndex i = 0; i < CFArrayGetCount(anchors); i++) { CFDataRef have = SecCertificateCopyData((SecCertificateRef)CFArrayGetValueAtIndex(anchors, i)); if (have && CFEqual(have, want)) found = 1; }
			printf("root among the anchors: %s\n", found ? "yes" : "no");
		}
	}
	return 0;
}
