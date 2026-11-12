// getaddrinfo from many threads at once, the way libcurl's threaded resolver calls it (one short-lived
// thread per lookup, several running together). Counts failures per round.
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
static const char* names[] = { "auth.riotgames.com", "telemetry.sgp.pvp.net", "data.riotgames.com", "clientconfig.rpg.riotgames.com",
	"lol.secure.dyn.riotcdn.net", "riot-client.secure.dyn.riotcdn.net", "www.riotgames.com", "example.com" };
#define N (int)(sizeof names / sizeof names[0])
static int rcs[N];
static void* one(void* arg) {
	long i = (long)arg; struct addrinfo hints, *res = 0; memset(&hints, 0, sizeof hints);
	hints.ai_socktype = SOCK_STREAM; hints.ai_family = PF_UNSPEC;
	rcs[i] = getaddrinfo(names[i], "443", &hints, &res); if (res) freeaddrinfo(res); return 0;
}
int main(int argc, char** argv) {
	int rounds = argc > 1 ? atoi(argv[1]) : 10, failed = 0, total = 0, par = argc > 2 ? atoi(argv[2]) : N;
	setvbuf(stdout, 0, _IONBF, 0);
	for (int r = 0; r < rounds; r++) {
		pthread_t t[N];
		for (long i = 0; i < par; i++) pthread_create(&t[i], 0, one, (void*)i);
		for (int i = 0; i < par; i++) pthread_join(t[i], 0);
		for (int i = 0; i < par; i++) { total++; if (rcs[i]) { failed++; printf("round %d: %s -> %d (%s)\n", r, names[i], rcs[i], gai_strerror(rcs[i])); } }
	}
	printf("%d of %d lookups failed\nRESULT: %s\n", failed, total, failed ? "FAIL" : "PASS"); return failed != 0;
}
