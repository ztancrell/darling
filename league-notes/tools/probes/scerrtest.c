// SCErrorString must always return text: callers pass it straight to string functions.
#include <stdio.h>
#include <string.h>
const char *SCErrorString(int status);
int main(void) {
	int codes[] = { 0, 1001, 1002, 2 /* ENOENT */, 987654 }, failed = 0;
	for (unsigned i = 0; i < sizeof codes / sizeof *codes; i++) {
		const char *s = SCErrorString(codes[i]);
		printf("%s %d -> %s\n", (s && strlen(s)) ? "ok  " : "FAIL", codes[i], s ? s : "(null)");
		if (!s || !strlen(s)) failed = 1;
	}
	if (strcmp(SCErrorString(0), "Success!")) { printf("FAIL 0 should be \"Success!\"\n"); failed = 1; }
	printf(failed ? "RESULT: FAIL\n" : "RESULT: PASS\n");
	return failed;
}
