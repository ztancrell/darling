// hw.model through every documented route. On macOS each returns 0 and a short name such as
// "MacBookPro16,1"; a size query (NULL buffer) sets the length and returns 0.
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/sysctl.h>

static int bad;
static void show(const char *what, int r, size_t len, const char *buf)
{
	int ok = r == 0 && len > 1 && len < 64 && (!buf || strlen(buf) + 1 == len);
	printf("%-34s ret %d errno %d len %zu value '%s' %s\n", what, r, r ? errno : 0, len, buf ? buf : "-", ok ? "ok" : "WRONG");
	bad |= !ok;
}

int main(void)
{
	char buf[256];
	size_t len;
	int mib[2] = { CTL_HW, HW_MODEL }, mib2[8];
	size_t miblen = 8;

	len = 0xdeadbeefdeadbeef; show("sysctlbyname size query", sysctlbyname("hw.model", NULL, &len, NULL, 0), len, NULL);
	len = sizeof(buf); memset(buf, 0, sizeof(buf)); show("sysctlbyname read", sysctlbyname("hw.model", buf, &len, NULL, 0), len, buf);
	len = 0xdeadbeefdeadbeef; show("sysctl {CTL_HW,HW_MODEL} size query", sysctl(mib, 2, NULL, &len, NULL, 0), len, NULL);
	len = sizeof(buf); memset(buf, 0, sizeof(buf)); show("sysctl {CTL_HW,HW_MODEL} read", sysctl(mib, 2, buf, &len, NULL, 0), len, buf);
	int r = sysctlnametomib("hw.model", mib2, &miblen);
	printf("sysctlnametomib ret %d miblen %zu mib {%d,%d}\n", r, miblen, mib2[0], mib2[1]);
	if (r == 0) {
		len = 0xdeadbeefdeadbeef; show("sysctl via nametomib size query", sysctl(mib2, (u_int)miblen, NULL, &len, NULL, 0), len, NULL);
		len = sizeof(buf); memset(buf, 0, sizeof(buf)); show("sysctl via nametomib read", sysctl(mib2, (u_int)miblen, buf, &len, NULL, 0), len, buf);
	} else bad = 1;
	len = 4; memset(buf, 0, sizeof(buf)); r = sysctlbyname("hw.model", buf, &len, NULL, 0);
	printf("short buffer (4 bytes): ret %d errno %d len %zu (macOS: -1, ENOMEM=12)\n", r, r ? errno : 0, len);
	puts(bad ? "FAIL" : "PASS");
	return bad;
}
