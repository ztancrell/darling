// Two DNS queries at once from threads, through libresolv's shared-state call (res_query) and through
// its per-caller-state call (res_nquery). Shows whether sharing the state is what breaks concurrent lookups.
#include <resolv.h>
#include <arpa/nameser.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
static int mode; static const char* names[] = { "auth.riotgames.com", "data.riotgames.com", "example.com", "telemetry.sgp.pvp.net" };
static int got[4];
static void* one(void* arg) {
	long i = (long)arg; unsigned char buf[4096];
	if (mode == 0) got[i] = res_query(names[i], ns_c_in, ns_t_a, buf, sizeof buf);
	else { struct __res_state st; memset(&st, 0, sizeof st); if (res_ninit(&st)) { got[i] = -2; return 0; }
		got[i] = res_nquery(&st, names[i], ns_c_in, ns_t_a, buf, sizeof buf); res_ndestroy(&st); }
	return 0;
}
int main(void) {
	setvbuf(stdout, 0, _IONBF, 0);
	for (mode = 0; mode < 2; mode++) {
		int failed = 0, total = 0;
		for (int r = 0; r < 10; r++) {
			pthread_t t[4];
			for (long i = 0; i < 4; i++) pthread_create(&t[i], 0, one, (void*)i);
			for (int i = 0; i < 4; i++) { pthread_join(t[i], 0); total++; if (got[i] < 0) failed++; }
		}
		printf("%s: %d of %d failed\n", mode ? "res_nquery, own state per call" : "res_query, shared state", failed, total);
	}
	return 0;
}
